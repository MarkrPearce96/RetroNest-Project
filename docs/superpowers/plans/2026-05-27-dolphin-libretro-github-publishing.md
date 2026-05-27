# Dolphin libretro publishing + install-from-fork wiring — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.
>
> **Context handoff:** written in a prior (high-context) session for a fresh session to execute. Read the spec first: `docs/superpowers/specs/2026-05-27-dolphin-libretro-github-publishing-design.md`. **All repo-creating / `gh release` / `git push fork` / `git tag` push steps must be run by the USER** — Claude Code's auto-mode classifier blocks them; the assistant prepares everything else and hands the exact commands over.

**Goal:** Make RetroNest "Install Dolphin/PPSSPP/PCSX2" download the user's own x86_64 cores from their GitHub forks (today they wrongly resolve to the official libretro buildbot), and set up the Dolphin fork's CI to publish the release the app then installs.

**Architecture:** Part A (host): drop `core_buildbot_path` from the three patched-fork manifests (keep mGBA) + fix the PCSX2 `github_repo` + add a `LibretroAdapter::assetMatchRules()` rule selecting the `*.dylib.zip` asset, so the installer uses each fork's GitHub Releases. Part B (fork): a tag-triggered GitHub Actions workflow builds the x86_64 `dolphin_libretro.dylib` and publishes `dolphin_libretro.dylib.zip`. Part C: cut the first Dolphin release.

**Tech Stack:** Qt6/C++ host (`RetroNest-Project`, branch `main`); GitHub Actions YAML + the Dolphin fork (`dolphin-libretro`, branch `libretro`, remote `fork = github.com/MarkrPearce96/dolphin-libretro`); macOS x86_64 build via Rosetta on `macos-14` runners.

---

## Context the executor needs

- **Two repos.** Host: `/Users/mark/Documents/Projects/RetroNest-Project` (branch `main`, push to `origin`). Fork files: `/Users/mark/Documents/Projects/dolphin-libretro` (branch `libretro`, push to `fork`).
- **Host build + redeploy** (after any host C++ change): `cmake --build cpp/build-x86_64 --target RetroNest --parallel`, then `/usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app` + `codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app`.
- **Launch for testing:** `RETRONEST_DOLPHIN_LOG=1 arch -x86_64 cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/retronest.log 2>&1 &`.
- **Editor clangd shows false "file not found"** for these files — trust the actual build.
- **Verified facts (do not re-derive):**
  - Installer prefers `resolveDirectDownload()` over GitHub Releases (`emulator_installer.cpp:221`); `LibretroAdapter::resolveDirectDownload` returns a buildbot URL iff `core_buildbot_path` is non-empty (`libretro_adapter.cpp:99-130`).
  - `postDownload` unzips a `*.dylib.zip` asset into `emulators/libretro/cores/`, derives the dylib name by stripping `.zip`, strips quarantine, writes a `.version` sidecar (`emulator_installer.cpp:299-336`).
  - `matchAsset` walks `assetMatchRules()` then a generic heuristic (`emulator_adapter.h:510`). `AssetMatchRule` is defined in `emulator_adapter.h` (~line 490) — confirm its exact fields when implementing A3.
  - Fork release state (2026-05-27): ppsspp `markrpearce96/ppsspp-libretro` has `v2026.05.22` (asset `ppsspp_libretro.dylib.zip`); pcsx2 real fork `markrpearce96/pcsx2-libretro` has `v2026.05.21.3`; dolphin `markrpearce96/dolphin-libretro` has none yet.
  - Proven workflow template: `/Users/mark/Documents/Projects/pcsx2-libretro/.github/workflows/libretro_release.yml`.

---

## Task 1: Manifests — route patched forks to GitHub Releases

**Files:**
- Modify: `manifests/dolphin.json`, `manifests/ppsspp.json`, `manifests/pcsx2.json`
- (Leave `manifests/mgba.json` untouched)

- [ ] **Step 1: Drop `core_buildbot_path` from the three patched-fork manifests**

In each of `dolphin.json`, `ppsspp.json`, `pcsx2.json`, delete the `"core_buildbot_path": "..."` line (and fix the trailing comma on the now-last line so the JSON stays valid). Example for `dolphin.json` — the object becomes:

```json
{
  "id": "dolphin",
  "name": "Dolphin",
  "description": "GameCube and Wii emulator (libretro core)",
  "systems": ["gc", "wii"],
  "github_repo": "markrpearce96/dolphin-libretro",
  "executable": "dolphin_libretro.dylib",
  "install_folder": "libretro",
  "rom_extensions": ["iso", "gcm", "gcz", "ciso", "wbfs", "rvz", "wad", "wia", "nkit", "m3u", "dol", "elf", "tgc"],
  "launch_args": [],
  "backend": "libretro",
  "core_dylib": "dolphin_libretro.dylib"
}
```

Do the same (remove `core_buildbot_path`, fix the preceding comma) for `ppsspp.json` and `pcsx2.json`. Keep every other field. **Do not touch `mgba.json`.**

- [ ] **Step 2: Fix the PCSX2 `github_repo`**

In `manifests/pcsx2.json`, change `"github_repo": "markpearce/pcsx2-retronest"` to `"github_repo": "markrpearce96/pcsx2-libretro"` (the real fork; the old value 404s).

- [ ] **Step 3: Validate JSON + run the manifest test**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
for f in dolphin ppsspp pcsx2 mgba; do python3 -c "import json;json.load(open('manifests/$f.json'))" && echo "$f OK"; done
cmake --build cpp/build-x86_64 --target test_manifest_libretro_fields --parallel && ctest --test-dir cpp/build-x86_64 -R Manifest --output-on-failure
```
Expected: all `OK`; manifest test passes (the loader makes `core_buildbot_path` optional — confirm by reading `manifest_loader.cpp:70`, which uses `obj.value(...)`, so a missing key is fine).

- [ ] **Step 4: Commit**

```bash
git add manifests/dolphin.json manifests/ppsspp.json manifests/pcsx2.json
git commit -m "publishing: route patched libretro forks to GitHub Releases (drop core_buildbot_path); fix pcsx2 repo"
```

---

## Task 2: `LibretroAdapter::assetMatchRules()` — select the `.dylib.zip` asset

**Files:**
- Modify: `cpp/src/adapters/libretro/libretro_adapter.h`, `cpp/src/adapters/libretro/libretro_adapter.cpp`

- [ ] **Step 1: Confirm the `AssetMatchRule` struct shape**

Read `cpp/src/adapters/emulator_adapter.h` around line 480-510 to confirm `AssetMatchRule`'s exact members (it is used as `name.endsWith(rule.extension)` and iterating `rule.substrings` with `lower.contains(...)`, so it has at least `QString extension;` and a string list `substrings;`). Match that exact field order/types in Step 3.

- [ ] **Step 2: Declare the override in `libretro_adapter.h`**

In the `public:` EmulatorAdapter-override block (next to `resolveDirectDownload`), add:

```cpp
    /** Select the "<core>_libretro.dylib.zip" GitHub release asset. The
     *  installer's postDownload path keys off the .dylib.zip suffix to unzip
     *  into cores/ and derive the dylib name. */
    QVector<AssetMatchRule> assetMatchRules() const override;
```

- [ ] **Step 3: Implement it in `libretro_adapter.cpp`**

Add (matching the `AssetMatchRule` field order confirmed in Step 1 — shown here as `{extension, substrings}`):

```cpp
QVector<EmulatorAdapter::AssetMatchRule> LibretroAdapter::assetMatchRules() const {
    // One macOS x86_64 asset per fork release, named "<core>_libretro.dylib.zip".
    // No platform substring needed. mGBA never reaches matchAsset (it keeps
    // core_buildbot_path -> buildbot), so this rule is inert for mGBA.
    return { AssetMatchRule{".dylib.zip", {}} };
}
```

(Use the correct fully-qualified `AssetMatchRule` type / namespace as declared in `emulator_adapter.h`; if it's a nested type, the return type is `QVector<EmulatorAdapter::AssetMatchRule>`.)

- [ ] **Step 4: Build the host**

Run: `cd /Users/mark/Documents/Projects/RetroNest-Project && cmake --build cpp/build-x86_64 --target RetroNest --parallel`
Expected: compiles + links, exit 0.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/adapters/libretro/libretro_adapter.h cpp/src/adapters/libretro/libretro_adapter.cpp
git commit -m "publishing: LibretroAdapter::assetMatchRules picks the .dylib.zip release asset"
```

---

## Task 3: Redeploy host + verify PPSSPP/PCSX2 install from forks (HUMAN)

No code. This proves Part A end-to-end (PPSSPP/PCSX2 forks already have releases).

- [ ] **Step 1: Redeploy the host**
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
/usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
```

- [ ] **Step 2: Verify PPSSPP installs from the fork**
Remove the locally-deployed PPSSPP core (`~/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib`), launch RetroNest, "Install PPSSPP". Confirm in `/tmp/retronest.log` it downloads from `markrpearce96/ppsspp-libretro` releases (NOT buildbot.libretro.com) — `[Installer] Selected asset: ppsspp_libretro.dylib.zip` and `[Installer] Libretro core installed to .../cores/ppsspp_libretro.dylib` — then launch a PSP game.

- [ ] **Step 3: Verify PCSX2 installs from the fork**
Same for PCSX2 (`markrpearce96/pcsx2-libretro`, asset `pcsx2_libretro.dylib.zip`). Confirm no `buildbot.libretro.com` in the install log.

> If either still hits buildbot, re-check Task 1 (the manifest still has `core_buildbot_path`) and Task 2 (`assetMatchRules` not picked up — rebuild/redeploy).

---

## Task 4: Fork docs — README.md + UPSTREAM-UPDATE.md

**Files (in `/Users/mark/Documents/Projects/dolphin-libretro`, branch `libretro`):**
- Create: `README.md`, `UPSTREAM-UPDATE.md`

- [ ] **Step 1: Write `README.md`**

```markdown
# dolphin-libretro (RetroNest fork)

A libretro core build of [Dolphin](https://github.com/dolphin-emu/dolphin) (GameCube + Wii),
forked for [RetroNest](https://github.com/MarkrPearce96/RetroNest-Project). Adds the RetroNest
libretro integration: Metal NSView handover, RetroAchievements game-identity + memory map,
savestates, pause / fast-forward, and persistent user dir.

## Install
RetroNest installs this core automatically ("Install Dolphin") from this repo's GitHub Releases
(macOS x86_64 `dolphin_libretro.dylib.zip`). License: GPL-2.0+ (inherited from Dolphin).

## Build (local, macOS x86_64)
See `RetroNest-Project/memory` build notes. In short:
`arch -x86_64 cmake -B build-libretro-x86_64 -G Ninja -DENABLE_LIBRETRO=ON -DENABLE_QT=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local` then build the `dolphin_libretro` target.

## Releases / upstream rebase
See `UPSTREAM-UPDATE.md`.
```

- [ ] **Step 2: Write `UPSTREAM-UPDATE.md`**

```markdown
# Upstream rebase + release process

Remotes: `origin` = dolphin-emu/dolphin (upstream, read-only), `fork` =
github.com/MarkrPearce96/dolphin-libretro (this repo). Work + releases live on `libretro`.

## Rebase onto upstream
1. `git fetch origin master`
2. `git rebase origin/master`   # resolve conflicts in Source/Core/DolphinLibretro/* + patched files
3. Rebuild x86_64 (see README) and smoke-test in RetroNest.
4. `git push fork libretro` (force-with-lease if rebased).

## Cut a release (triggers CI → GitHub Release with the x86_64 dylib)
1. `git tag v2026.MM.DD` (numeric suffix vN for same-day re-cuts)
2. `git push fork v2026.MM.DD`
3. GitHub Actions (`.github/workflows/libretro_release.yml`) builds + publishes
   `dolphin_libretro.dylib.zip`. RetroNest's "Install Dolphin" picks it up.
```

- [ ] **Step 3: Commit (push is user-run)**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add README.md UPSTREAM-UPDATE.md
git commit -m "docs: fork README + upstream-rebase/release process"
```
Then the USER runs: `git push fork libretro`.

---

## Task 5: Dolphin CI release workflow

**Files (in `dolphin-libretro`, branch `libretro`):**
- Create: `.github/workflows/libretro_release.yml`

- [ ] **Step 1: Write the workflow**

```yaml
name: libretro_release

# Builds the dolphin-libretro core (macOS x86_64) on a v* tag push and publishes
# a GitHub Release with dolphin_libretro.dylib.zip — the asset RetroNest installs.
# Mirrors the pcsx2-libretro / ppsspp-libretro release workflows.

on:
  push:
    tags: ['v*']

permissions:
  contents: write

jobs:
  build:
    name: Build macOS x86_64
    runs-on: macos-14   # Apple Silicon; build x86_64 via Rosetta (Intel pool is starved)
    timeout-minutes: 120
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install Rosetta
        run: softwareupdate --install-rosetta --agree-to-license || true

      - name: Install x86_64 Homebrew at /usr/local
        run: |
          if [ ! -x /usr/local/bin/brew ]; then
            NONINTERACTIVE=1 arch -x86_64 /bin/bash -c \
              "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
          fi
          arch -x86_64 /usr/local/bin/brew --version

      - name: Install dependencies (x86_64)
        run: |
          arch -x86_64 /usr/local/bin/brew update
          # Verified from the local x86_64 dylib's links + build tools. Tune over
          # 1-2 CI runs if configure/link reports a missing dep.
          arch -x86_64 /usr/local/bin/brew install \
            cmake ninja pkg-config fmt sdl3 lz4 lzo xz zstd libpng

      - name: Configure (x86_64, scrubbing /opt/homebrew)
        run: |
          arch -x86_64 env \
            HOMEBREW_PREFIX=/usr/local \
            HOMEBREW_CELLAR=/usr/local/Cellar \
            PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig \
            PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
            /usr/local/bin/cmake -S . -B build -G Ninja \
              -DCMAKE_PREFIX_PATH=/usr/local \
              -DCMAKE_IGNORE_PATH=/opt/homebrew \
              -DCMAKE_OSX_ARCHITECTURES=x86_64 \
              -DENABLE_LIBRETRO=ON \
              -DENABLE_QT=OFF \
              -DENABLE_NOGUI=OFF \
              -DENABLE_TESTS=OFF \
              -DENCODE_FRAMEDUMPS=OFF \
              -DCMAKE_BUILD_TYPE=Release

      - name: Build dolphin_libretro
        run: |
          arch -x86_64 env PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
            /usr/local/bin/cmake --build build --target dolphin_libretro -j

      - name: Package dolphin_libretro.dylib.zip
        run: |
          set -euo pipefail
          DYLIB=build/Source/Core/DolphinLibretro/dolphin_libretro.dylib
          test -f "$DYLIB"
          file "$DYLIB"            # expect: Mach-O 64-bit dynamically linked shared library x86_64
          mkdir -p stage && cp "$DYLIB" stage/
          (cd stage && zip -j ../dolphin_libretro.dylib.zip dolphin_libretro.dylib)
          ls -la dolphin_libretro.dylib.zip

      - uses: actions/upload-artifact@v4
        with:
          name: dolphin_libretro-dylib-zip
          path: dolphin_libretro.dylib.zip
          if-no-files-found: error

  release:
    name: Publish release
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - uses: actions/download-artifact@v4
        with:
          name: dolphin_libretro-dylib-zip
          path: .
      - name: Compute upstream SHA
        id: meta
        run: |
          git remote add upstream https://github.com/dolphin-emu/dolphin.git
          git fetch --no-tags upstream master
          echo "upstream_short=$(git merge-base HEAD upstream/master | cut -c1-7)" >> "$GITHUB_OUTPUT"
      - name: Compose body
        run: |
          {
            echo "**Platform:** macOS x86_64 (Intel + Apple Silicon via Rosetta)"
            echo "**Upstream Dolphin:** ${{ steps.meta.outputs.upstream_short }} (master)"
          } > body.md
      - uses: softprops/action-gh-release@v2
        with:
          tag_name: ${{ github.ref_name }}
          files: dolphin_libretro.dylib.zip
          body_path: body.md
          generate_release_notes: true
```

- [ ] **Step 2: Commit (push is user-run)**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add .github/workflows/libretro_release.yml
git commit -m "ci: x86_64 dolphin_libretro release workflow on tag push"
```
Then the USER runs: `git push fork libretro`.

---

## Task 6: First Dolphin release + end-to-end install (HUMAN)

- [ ] **Step 1: Cut the first release (USER-run)**
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git tag v2026.MM.DD          # today's date
git push fork v2026.MM.DD
```
Watch the Actions run on `github.com/markrpearce96/dolphin-libretro`. (If CI needs a dep/flag fix, amend `libretro_release.yml`, push, re-tag with a `.N` suffix.) **Optional unblock:** instead of waiting on CI, hand-zip the already-built local `build-libretro-x86_64/.../dolphin_libretro.dylib` as `dolphin_libretro.dylib.zip` and attach it to a hand-created release at the same tag.

- [ ] **Step 2: Verify the release asset**
On the release page, confirm a `dolphin_libretro.dylib.zip` asset exists; download + `unzip -l` it shows `dolphin_libretro.dylib`; `file` says Mach-O x86_64.

- [ ] **Step 3: End-to-end install in RetroNest**
Remove the local `~/Documents/RetroNest/emulators/libretro/cores/dolphin_libretro.dylib`, launch RetroNest, "Install Dolphin" → confirm `/tmp/retronest.log` shows it downloading from `markrpearce96/dolphin-libretro` releases (not buildbot) + installing to `cores/`, then launch a GC and a Wii game (Sys is already in the app bundle on this machine).

---

## Self-review notes

- **Spec coverage:** Part A1/A2 ↔ Task 1; A3 ↔ Task 2; A verification ↔ Task 3; Part B ↔ Tasks 4-5; Part C ↔ Task 6. mGBA intentionally untouched (Task 1 leaves `mgba.json`).
- **Out of scope (per spec):** universal/Windows builds, Sys-data distribution, mGBA changes, PPSSPP/PCSX2 CI (their forks already publish) — none appear as tasks.
- **Type consistency:** `assetMatchRules()` override (Task 2) matches the base `EmulatorAdapter::matchAsset`/`AssetMatchRule` contract (confirm fields in Task 2 Step 1). The release asset name `dolphin_libretro.dylib.zip` (Task 5) matches the `.dylib.zip` rule (Task 2) and `manifest.core_dylib` (`dolphin_libretro.dylib`) so `postDownload` derives the right name.
- **Human/auto-mode:** Tasks 3 and 6, and the `git push fork` / tag-push / `gh` steps in Tasks 4-6, are user-run (auto-mode blocks repo/release-touching commands).
