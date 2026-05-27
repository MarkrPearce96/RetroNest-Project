# Dolphin libretro publishing + libretro install-from-fork wiring (x86_64)

Follow-on to SP8 / RVZ. Goal: make RetroNest's **"Install Dolphin"** download the user's own
x86_64 core from `github.com/markrpearce96/dolphin-libretro`. Investigation found this needs
two things, not one: a CI release on the fork **and** a host-side fix so the installer
actually routes patched libretro forks to their GitHub Releases (today it routes them to the
official libretro buildbot). The host fix is generalized to all patched forks (Dolphin, PCSX2,
PPSSPP) per the user's decision; mGBA (a standard core) stays on buildbot.

> **Status: spec + plan written for a fresh session to execute (context handoff).** Execute via
> `superpowers:subagent-driven-development`. The repo-creating / `gh release` / `git push fork` /
> tag-push steps must be run by the user — Claude Code's auto-mode classifier blocks them.

## Problem (verified)

`EmulatorInstaller::fetchReleaseInfo` (`cpp/src/services/emulator_installer.cpp:221`) consults
`adapter->resolveDirectDownload()` **first** and skips GitHub Releases if it returns a URL.
`LibretroAdapter::resolveDirectDownload` (`cpp/src/adapters/libretro/libretro_adapter.cpp:99-130`)
returns a **`buildbot.libretro.com/nightly/apple/osx/<arch>/latest/<core_buildbot_path>`** URL
whenever `core_buildbot_path` is set — even if the HEAD check fails. Every manifest sets
`core_buildbot_path`, and no libretro adapter overrides `resolveDirectDownload`/`matchAsset`. So:

- **Install resolves to the official libretro core on buildbot, not the user's fork.** For the
  heavily-patched forks (Dolphin: Metal NSView handover, `retronest_set_paused`/`_fast_forward`,
  `SET_GAME_IDENTITY`; PCSX2/PPSSPP: similar), the official cores wouldn't work in RetroNest. (These
  forks are currently run from local deploys, not app-installed.)
- The GitHub-Releases path's asset selection (`matchAsset` → `assetMatchRules()`) has no rule that
  matches a `*.dylib.zip` asset, so even forcing that path wouldn't pick the core.

## Current fork state (verified 2026-05-27)

| id | manifest `github_repo` | releases | notes |
|----|------------------------|----------|-------|
| dolphin | `markrpearce96/dolphin-libretro` ✓ | **none** | needs CI + first release |
| ppsspp  | `markrpearce96/ppsspp-libretro` ✓  | **yes** (`v2026.05.22`, asset `ppsspp_libretro.dylib.zip`) | routing only |
| pcsx2   | `markpearce/pcsx2-retronest` ✗ **(404)** | — | manifest repo wrong; real fork `markrpearce96/pcsx2-libretro` **has** `v2026.05.21.3` |
| mgba    | `libretro/mgba` (official) | — | **unchanged** — stays on buildbot (official core works for a standard SW core) |

All patched forks already publish (or will) a `<core>_libretro.dylib.zip` asset.

## Scope

**In:** (A) host install-from-fork wiring for Dolphin + PPSSPP + PCSX2; (B) Dolphin CI release
workflow on the fork; (C) cut the first Dolphin release.

**Out:** universal/arm64/Windows builds (x86_64-only for now); Sys-data distribution (dylib-only
release — Sys comes from the RetroNest app bundle, already present on the dev machine; fresh-machine
Sys packaging is a separate RetroNest-packaging task); mGBA changes; PPSSPP/PCSX2 CI (their forks
already publish — only their *routing* is fixed here).

## Part A — Host install-from-fork wiring

**A1. Drop `core_buildbot_path`** from `manifests/dolphin.json`, `manifests/ppsspp.json`,
`manifests/pcsx2.json`. With it empty, `LibretroAdapter::resolveDirectDownload` returns an empty
`DirectDownloadInfo` (`libretro_adapter.cpp:102`), so the installer falls through to the
`github_repo` `/releases/latest` path. **Keep** `core_buildbot_path` in `manifests/mgba.json`.

**A2. Fix `manifests/pcsx2.json` `github_repo`**: `markpearce/pcsx2-retronest` →
`markrpearce96/pcsx2-libretro` (the real fork with releases).

**A3. Add `LibretroAdapter::assetMatchRules()` override** so `matchAsset`
(`emulator_adapter.h:510`, which walks `assetMatchRules()`) selects the `*.dylib.zip` asset:

```cpp
// libretro_adapter.h (public, with the other EmulatorAdapter overrides)
QVector<AssetMatchRule> assetMatchRules() const override;
// libretro_adapter.cpp
QVector<AssetMatchRule> LibretroAdapter::assetMatchRules() const {
    // Libretro core releases ship one "<core>_libretro.dylib.zip" asset; the
    // installer's postDownload path keys off the ".dylib.zip" suffix to unzip
    // into cores/ and derive the dylib name. No platform substring needed
    // (the forks publish one macOS x86_64 asset).
    return { AssetMatchRule{/*extension*/ ".dylib.zip", /*substrings*/ {}} };
}
```

(Confirm the exact `AssetMatchRule` field order/types in `emulator_adapter.h` ~line 490 when
implementing.) mGBA never reaches `matchAsset` (it still uses `resolveDirectDownload`/buildbot),
so this override is inert for mGBA.

**Result of A:** `postDownload` (`emulator_installer.cpp:301-336`) already unzips a `.dylib.zip`
into `emulators/libretro/cores/<core>_libretro.dylib`, strips quarantine, and writes a
`.version` sidecar. So after A, **PPSSPP and PCSX2 install from their forks immediately**
(releases already exist); Dolphin installs once it has a release (Parts B/C).

## Part B — Dolphin CI release workflow (on the fork)

Net-new files at the `dolphin-libretro` repo root (`libretro` branch), pushed to `fork`:
`.github/workflows/libretro_release.yml`, `README.md`, `UPSTREAM-UPDATE.md`. No upstream Dolphin
files touched; the workflow triggers only on `v*` tags (won't interfere with upstream CI).

Mirror the proven PCSX2/PPSSPP workflows (`/Users/mark/Documents/Projects/pcsx2-libretro/.github/workflows/libretro_release.yml`):
- `on: push: tags: ['v*']`; `permissions: contents: write`.
- **Job `build`** on **`macos-14`** (Apple Silicon — the Intel `macos-13` pool is starved): install
  Rosetta + **x86_64 Homebrew at `/usr/local`** + `arch -x86_64` wrappers; `brew install` Dolphin's
  x86_64 deps (verified from the local x86_64 dylib's links: `fmt sdl3 lz4 lzo xz zstd` + build
  tools `cmake ninja pkg-config`; tune over 1–2 CI runs); configure with the `/opt/homebrew`-scrub
  (`-DCMAKE_IGNORE_PATH=/opt/homebrew`, `PKG_CONFIG_PATH=/usr/local/...`, `HOMEBREW_PREFIX=/usr/local`)
  and Dolphin's flags (`-DENABLE_LIBRETRO=ON -DENABLE_QT=OFF -DENABLE_NOGUI=OFF -DENABLE_TESTS=OFF
  -DENCODE_FRAMEDUMPS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -G Ninja`);
  `cmake --build build --target dolphin_libretro`. **No metallib step** (Dolphin's Metal backend
  compiles shaders at runtime — unlike PCSX2's GS) and **no resources dir** (Sys is host-relative,
  out of scope). Zip `build/Source/Core/DolphinLibretro/dolphin_libretro.dylib` →
  **`dolphin_libretro.dylib.zip`** (dylib at the zip root).
- **Job `release`** (`needs: build`, `ubuntu-latest`): `softprops/action-gh-release@v2` for the tag,
  attach `dolphin_libretro.dylib.zip`, `generate_release_notes: true` + a prelude (platform +
  upstream Dolphin SHA via `git merge-base HEAD upstream/master`).

No code signing/notarization (the installer de-quarantines the downloaded dylib). Date tags
`v2026.MM.DD` (numeric suffix for same-day re-cuts).

## Part C — First Dolphin release

Push a `v2026.MM.DD` tag to `fork` → CI builds + publishes the release. (Optional unblock: a freshly
built local x86_64 `dolphin_libretro.dylib` can be hand-zipped + uploaded as the first release while
CI is finalized.) All tag/`gh`/push steps are **user-run** (auto-mode block).

## Testing / acceptance

- **A (routing), no new release needed for PPSSPP/PCSX2:** after the manifest + `assetMatchRules`
  changes + host rebuild, in RetroNest remove the local PPSSPP core and "Install PPSSPP" → it
  downloads `ppsspp_libretro.dylib.zip` from `markrpearce96/ppsspp-libretro` releases into
  `cores/` → launch a PSP game. Repeat for PCSX2 (`markrpearce96/pcsx2-libretro`).
- **B (CI):** the tag push yields a release with a `dolphin_libretro.dylib.zip` asset; the zip
  contains a Mach-O **x86_64** dylib (`file`/`lipo -info`).
- **C (end-to-end):** "Install Dolphin" downloads + unzips the fork's dylib into `cores/` → launch
  a GC + Wii game (Sys already in the app bundle on this machine).
- **mGBA unaffected:** still installs from buildbot (official core).

## Risks

- **Routing PPSSPP/PCSX2 before verifying their assets** — both confirmed to have
  `<core>_libretro.dylib.zip` assets (PPSSPP `v2026.05.22`; PCSX2 `v2026.05.21.3`), so routing is
  safe. The PCSX2 manifest-repo fix (A2) is required or PCSX2 would 404.
- **CI dep/flag iteration** for the Dolphin build on a clean runner (1–2 passes expected).
- **`assetMatchRules` `.dylib.zip` rule** must not regress mGBA — it doesn't (mGBA uses buildbot).
- **Sys on fresh machines** — install delivers only the dylib; out of scope (RetroNest-packaging).

## Sequencing (informs the plan)

1. **A** (host): manifests (drop `core_buildbot_path` x3 + fix pcsx2 repo) + `assetMatchRules`
   override → build host → verify PPSSPP/PCSX2 install from forks.
2. **B** (fork): `README.md` + `UPSTREAM-UPDATE.md` + `libretro_release.yml`.
3. **C**: first Dolphin release (user-run tag push) → end-to-end install verify.

Parts A and B are independent; A delivers PPSSPP/PCSX2 install immediately, B+C deliver Dolphin.
