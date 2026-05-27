# Dolphin libretro — GitHub publishing (x86_64)

Follow-on to the SP8 / RVZ work. The Dolphin libretro core is now a public fork at
`github.com/markrpearce96/dolphin-libretro`, but it has **no releases**, so RetroNest's
"Install Dolphin" / update-checker (which downloads a release asset) finds nothing. This
spec sets up the GitHub Actions release pipeline so the app can install the x86_64 core
from the user's own GitHub — mirroring the existing PCSX2 publishing setup
(`docs/superpowers/specs/2026-05-21-pcsx2-libretro-github-publishing-design.md`).

## Problem

`emulator_installer.cpp` installs a libretro core by hitting
`https://api.github.com/repos/<github_repo>/releases/latest`, finding the asset whose name
ends in `.dylib.zip`, and unzipping it into `emulators/libretro/cores/` (deriving the dylib
name by stripping `.zip`). `github_client.h::fetchLatestRelease` drives the update check off
the release `tag` / `published_at`. `manifests/dolphin.json` already points at
`github_repo: "markrpearce96/dolphin-libretro"` with `core_buildbot_path:
"dolphin_libretro.dylib.zip"`. The only missing piece is a **release that carries that
asset**.

## Goal

A GitHub Actions workflow on the fork that, on a version tag push, builds the **x86_64**
`dolphin_libretro.dylib`, packages it as `dolphin_libretro.dylib.zip`, and publishes it as a
GitHub Release asset — so "Install Dolphin" in RetroNest works end-to-end.

## Scope

**In:** the release workflow (`.github/workflows/libretro_release.yml`), `README.md`,
`UPSTREAM-UPDATE.md`, the tag/version convention, and cutting the first release.

**Out:**
- **Universal / cross-platform builds.** x86_64-only for now (RetroNest runs under Rosetta);
  arm64 + Windows is a future "convert all emulators" effort.
- **Sys-data distribution.** The release is the dylib only (see "Sys data" below).
- **RetroNest app-bundle packaging** (shipping Dolphin's Sys to fresh machines) — a separate
  RetroNest-side task.

## Key facts (verified)

- Install path (`cpp/src/services/emulator_installer.cpp:299-317`): libretro cores "arrive as
  a single `.dylib.zip` — unzip into `cores/`"; the dylib name is the archive name minus
  `.zip`. So the release asset must be **`dolphin_libretro.dylib.zip`** containing
  `dolphin_libretro.dylib`. No resources/VERSION handling on the libretro path.
- Download source is **GitHub Releases** `releases/latest` (`emulator_installer.cpp:236,436`),
  not the libretro buildbot. Update staleness uses the release `tag`/`published_at`
  (`github_client.h:113-142`).
- Downloaded dylib quarantine is stripped by the installer ("quarantine removal",
  `emulator_installer.cpp:288`) — so the dylib needs no code signing / notarization.
- Local x86_64 build (from `memory/dolphin-libretro-build-setup`): `arch -x86_64` cmake with
  `-DENABLE_LIBRETRO=ON -DENABLE_QT=OFF -DENABLE_NOGUI=OFF -DENABLE_TESTS=OFF
  -DENCODE_FRAMEDUMPS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local`; output at
  `build/Source/Core/DolphinLibretro/dolphin_libretro.dylib`.
- Fork remotes already set: `origin = dolphin-emu/dolphin` (upstream), `fork =
  github.com/MarkrPearce96/dolphin-libretro` (writable); `libretro` tracks `fork/libretro`.

## Decisions

**D1. Workflow lives on the fork's `libretro` branch; net-new files only.**
`.github/workflows/libretro_release.yml`, `README.md`, `UPSTREAM-UPDATE.md` at the repo root.
No upstream Dolphin files touched. (Note: Dolphin already has upstream `.github/workflows/` —
the new file sits alongside them and only triggers on `v*` tags, so it won't interfere with
upstream CI, which triggers on pushes/PRs.)

**D2. Trigger: version tag push.** `on: push: tags: ['v*']`. Zero CI cost between releases;
a release is cut by pushing a `v…` tag. Matches PCSX2.

**D3. Build on `macos-13` (Intel runner) — native x86_64.** No cross-compile. (If GitHub
retires `macos-13`, x86_64 would need cross-compilation on an arm64 runner — future risk.)

**D4. Release asset = `dolphin_libretro.dylib.zip` (dylib only).** No `resources/` dir (unlike
PCSX2 — Dolphin's data is the host-relative `Sys` folder, handled separately) and no
`VERSION` file inside the zip (the update-checker uses the release tag/`published_at`). This
exactly matches what `emulator_installer.cpp` unzips into `cores/`.

**D5. No code signing / notarization.** The installer strips the quarantine xattr on the
downloaded dylib. Ship it unsigned, as the other core forks do.

**D6. Date tags.** `v2026.MM.DD`, with a numeric suffix for same-day re-cuts
(`v2026.05.27.1`). Auto-generated release notes + a short prelude (platform + upstream Dolphin
SHA), via `softprops/action-gh-release@v2`.

**D7. Sys data is out of scope (dylib-only).** Dolphin resolves `Sys` relative to the host
binary (`RetroNest.app/Contents/Resources/Sys`), and a signed `.app` can't be written at
install time — so Sys can't ride in the core zip. On the developer machine Sys is already
deployed there (via `tools/deploy.sh`), so install works now. Bundling Sys into RetroNest's
distributable build for fresh machines is a separate RetroNest-packaging task (or, later, a
core patch making the Sys dir configurable like the `GET_SAVE_DIRECTORY` user-dir fix).

## Workflow shape — `.github/workflows/libretro_release.yml`

```yaml
on:
  push:
    tags: ['v*']
```

**Job `build`** (`runs-on: macos-13`):
- `actions/checkout@v4` with `submodules: recursive` (Dolphin's `Externals`).
- `brew install` the x86_64 build deps. Exact list derived in the plan from the local
  toolchain + the dylibs the core links (seen at link time: `fmt`, `sdl3` (or `sdl2`), `lz4`,
  `zstd`, `xz`/`lzma`, `lzo`, `libpng`; plus `ninja`, `pkg-config`). Tune over 1–2 CI runs.
- Configure: `cmake -B build -S . -G Ninja -DENABLE_LIBRETRO=ON -DENABLE_QT=OFF
  -DENABLE_NOGUI=OFF -DENABLE_TESTS=OFF -DENCODE_FRAMEDUMPS=OFF -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_PREFIX_PATH=/usr/local -DCMAKE_OSX_ARCHITECTURES=x86_64`.
- Build: `cmake --build build --target dolphin_libretro`.
- Package: `ditto`/`zip` `build/Source/Core/DolphinLibretro/dolphin_libretro.dylib` →
  `dolphin_libretro.dylib.zip` (the dylib at the zip root, so unzip yields the bare dylib).
- Upload the zip as a workflow artifact.

**Job `release`** (`needs: build`):
- Download the artifact.
- `softprops/action-gh-release@v2` with the pushed tag: create the release, attach
  `dolphin_libretro.dylib.zip`, `generate_release_notes: true`, and a `body` prelude
  (`platform=macos-x86_64`, upstream Dolphin SHA, build date).

## Install-flow contract (must hold)

After a release exists, RetroNest must: resolve `releases/latest` for
`markrpearce96/dolphin-libretro` → find the `dolphin_libretro.dylib.zip` asset → download +
unzip into `emulators/libretro/cores/dolphin_libretro.dylib` → record the release
`tag`/`published_at` for the update-checker. (All existing app behavior; this spec only
produces the release it consumes.)

## First release

Two ways to produce the first release; either unblocks "Install Dolphin":
- **Via CI (normal path):** push a `v2026.05.27` tag to `fork` → the workflow builds + publishes.
- **Manual stopgap (optional, to unblock immediately):** zip the already-built local x86_64
  `dolphin_libretro.dylib` as `dolphin_libretro.dylib.zip` and attach it to a hand-created
  release at the same tag. Useful if CI needs a couple of iterations to get the dep list right.

(Either way, the repo-creating / `gh release` / tag-push steps are run by the user — Claude
Code's auto-mode classifier blocks repo/release-touching `gh`/`git push` commands.)

## Testing / acceptance

- **CI green:** the tag push produces a release with a `dolphin_libretro.dylib.zip` asset; the
  zip contains a Mach-O x86_64 `dolphin_libretro.dylib` (`lipo -info` / `file`).
- **End-to-end install:** in RetroNest, uninstall/remove the local core, then "Install Dolphin"
  → it downloads + unzips the release dylib into `cores/` → launch a GC or Wii game and confirm
  it boots (Sys already present in the app bundle on this machine).
- **Update check:** bump the tag, confirm RetroNest reports an update available.

## Risks

- **CI dep/flag iteration:** porting the local build to a clean `macos-13` runner usually needs
  1–2 passes to get the Homebrew dep list + prefix paths right. Expected, not a blocker.
- **`macos-13` retirement:** x86_64 native runner is fine today; would need cross-compile later.
- **Sys on fresh machines:** install delivers only the dylib; a machine whose RetroNest bundle
  lacks `Sys` would black-screen. Out of scope here (RetroNest-packaging follow-up).

## Sequencing (informs the plan)

1. `README.md` + `UPSTREAM-UPDATE.md`.
2. `libretro_release.yml` (build + release jobs).
3. First release (CI tag push, or manual stopgap) — user-run.
4. End-to-end install verification in RetroNest.
