# Packet 7 Stage 1 — retronest-libretro Contract Package Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create one canonical `retronest-libretro` contract package (pinned `libretro.h`, private env-command registry, game-identity struct, optional-export prototypes, shared NSView helper, shared core-options emit helper, style/deploy docs) in RetroNest and vendor it byte-identically into the duckstation, pcsx2, and dolphin forks, deleting every hand-copied constant.

**Architecture:** The canonical package lives at `RetroNest-Project/vendor/retronest-libretro/` (absorbing `vendor/libretro-api/`). `sync.sh` copies it into each fork's isolation directory; `check-drift.sh` + `MANIFEST.sha256` catch local edits via a CMake `ALL` target in each fork. Forks keep a 2-line forwarding `libretro.h` at their old header path so no `#include` lines change. Constants are byte-identical by construction — the only deliberate behavior change is pcsx2 gaining duckstation's main-thread-safe NSView metrics query.

**Tech Stack:** C/C++17, Objective-C++, CMake, QtTest, bash, shasum.

**Spec:** `docs/superpowers/specs/2026-07-04-packet7-shared-contract-design.md` (RetroNest-Project).

## Global Constraints

- The x86_64/Rosetta app at `cpp/build-x86_64/RetroNest.app` is the **daily driver**. Never leave it broken between tasks.
- **Nothing is pushed to any remote.** Local commits only; the user pushes after confirming each smoke gate in-app.
- Each task ends at a **USER SMOKE GATE**: STOP, hand the user the checklist, and wait for their confirmation before starting the next task.
- Repos: `~/Documents/Projects/{RetroNest-Project, duckstation-libretro, pcsx2-libretro, dolphin-libretro}`. ppsspp-libretro and mgba-libretro are **not touched** in this stage.
- ABI values are frozen: commands `0x20001–0x20005`, struct `retronest_game_identity { const char* ra_hash; const char* serial; }`, exports `retronest_set_paused(bool)` / `retronest_set_fast_forward(bool)` / `retronest_shutdown_wedged(void)->bool`.
- RetroNest builds/tests always via `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 6` and `arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 --output-on-failure` (run from the RetroNest-Project repo root).
- The fork-side Python fidelity checkers (`check_schema_fidelity.py`) are **left untouched** — they die in Stage 2 with the hand schemas.

---

### Task 1: Canonical package + ABI pin test (RetroNest-Project)

**Files:**
- Create: `vendor/retronest-libretro/retronest_libretro.h`
- Create: `vendor/retronest-libretro/retronest_nsview.h`
- Create: `vendor/retronest-libretro/retronest_nsview.mm`
- Create: `vendor/retronest-libretro/emit_core_options_v2.h`
- Create: `vendor/retronest-libretro/README.md`
- Create: `vendor/retronest-libretro/docs/option-style-guide.md`
- Create: `vendor/retronest-libretro/docs/deploy-contract.md`
- Create: `vendor/retronest-libretro/sync.sh` (mode 755)
- Create: `vendor/retronest-libretro/check-drift.sh` (mode 755)
- Move: `vendor/libretro-api/libretro.h` → `vendor/retronest-libretro/libretro.h` (banner prepended); delete `vendor/libretro-api/` (including its `README.md`)
- Modify: `cpp/CMakeLists.txt` lines 49, 149, 199, 319, 422, 748 (`vendor/libretro-api` → `vendor/retronest-libretro`) + new test target
- Test: `cpp/tests/test_retronest_contract.cpp`

**Interfaces:**
- Produces (used by every later task): header `retronest_libretro.h` defining macros `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW/_GET_BOOT_STATE_PATH/_GET_MEMCARDS_DIR/_GET_TEXTURES_DIR/_SET_GAME_IDENTITY`, `struct retronest_game_identity`, typedefs `retronest_set_paused_t = void(*)(bool)`, `retronest_set_fast_forward_t = void(*)(bool)`, `retronest_shutdown_wedged_t = bool(*)(void)`, and (under `#ifdef RETRONEST_LIBRETRO_CORE`) `extern "C"` prototypes of the three exports.
- Produces: `retronest::NSViewMetrics retronest::QueryNSViewMetrics(void* ns_view)` in `retronest_nsview.h` (struct fields `uint32_t surface_width/surface_height` — 0 when unknown, `float surface_scale` — default 1.0f, `float refresh_rate` — 0.0f when unknown).
- Produces: `bool retronest::EmitCoreOptionsV2(retro_environment_t cb, const retro_core_option_v2_definition* defs)` in `emit_core_options_v2.h`.
- Produces: `sync.sh` (copies package to the three forks, regenerating `MANIFEST.sha256` first) and `check-drift.sh` (verifies a vendored copy against its manifest).

- [ ] **Step 1: Write the failing test**

Create `cpp/tests/test_retronest_contract.cpp`:

```cpp
// ABI pin for the retronest-libretro contract package. These numeric values
// and layouts are burned into shipped core dylibs — if this test fails, you
// renumbered the contract (a cross-repo breaking change), not the test.
#include <QtTest>
#include <cstddef>
#include "retronest_libretro.h"

class TestRetronestContract : public QObject {
    Q_OBJECT
private slots:
    void commandValues() {
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW),   0x20001u);
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH), 0x20002u);
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR),   0x20003u);
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR),   0x20004u);
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY),  0x20005u);
    }
    void identityStructLayout() {
        QCOMPARE(sizeof(retronest_game_identity), 2 * sizeof(const char*));
        QCOMPARE(offsetof(retronest_game_identity, ra_hash), size_t(0));
        QCOMPARE(offsetof(retronest_game_identity, serial), sizeof(const char*));
    }
    void exportTypedefsCompile() {
        retronest_set_paused_t p = nullptr;
        retronest_set_fast_forward_t f = nullptr;
        retronest_shutdown_wedged_t w = nullptr;
        QVERIFY(p == nullptr && f == nullptr && w == nullptr);
    }
};
QTEST_APPLESS_MAIN(TestRetronestContract)
#include "test_retronest_contract.moc"
```

Add the target to `cpp/CMakeLists.txt` next to the other schema-guard tests (after the `test_pcsx2_libretro_schema` block, around line 643):

```cmake
# Packet 7 Stage 1: ABI pin for the retronest-libretro contract package.
add_executable(test_retronest_contract tests/test_retronest_contract.cpp)
target_link_libraries(test_retronest_contract PRIVATE retronest_core Qt6::Test)
add_test(NAME RetronestContract COMMAND test_retronest_contract)
```

- [ ] **Step 2: Run test to verify it fails**

Run (from `~/Documents/Projects/RetroNest-Project`):
```bash
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_retronest_contract -j 6
```
Expected: FAIL — `'retronest_libretro.h' file not found`.

- [ ] **Step 3: Create the package directory and move the pinned header**

```bash
cd ~/Documents/Projects/RetroNest-Project
mkdir -p vendor/retronest-libretro/docs
git mv vendor/libretro-api/libretro.h vendor/retronest-libretro/libretro.h
git rm vendor/libretro-api/README.md
```

Prepend this banner to `vendor/retronest-libretro/libretro.h` (above the existing copyright comment, as its own block):

```c
/* ============================================================================
 * RetroNest pinned libretro.h — part of the retronest-libretro contract
 * package. CANONICAL COPY: RetroNest-Project/vendor/retronest-libretro/.
 * Source: libretro/RetroArch libretro-common/include/libretro.h (see
 * README.md in this directory for the refresh procedure).
 * Synced byte-identical into every core fork by sync.sh — DO NOT EDIT the
 * vendored copies; edit the canonical file and re-run sync.sh.
 * ==========================================================================*/
```

- [ ] **Step 4: Write the contract header**

Create `vendor/retronest-libretro/retronest_libretro.h`:

```c
/* retronest-libretro — the RetroNest ⇄ libretro-core private contract.
 *
 * CANONICAL COPY: RetroNest-Project/vendor/retronest-libretro/retronest_libretro.h
 * Vendored byte-identical into each core fork by sync.sh. DO NOT EDIT the
 * vendored copies — edit the canonical file and re-run sync.sh. Drift is
 * caught by check-drift.sh (wired as a CMake ALL target in each fork).
 *
 * libretro reserves RETRO_ENVIRONMENT_PRIVATE (0x20000+) for frontend↔core
 * private contracts. This header is the single registry of RetroNest's
 * private environment commands and dlsym'd optional core exports. The
 * numeric values are frozen ABI — pinned by RetroNest's
 * test_retronest_contract and by hardcoded literals in each fork's
 * standalone tests (those literals are deliberate: they are the other
 * side of the contract).
 */
#ifndef RETRONEST_LIBRETRO_H
#define RETRONEST_LIBRETRO_H

#include "libretro.h" /* the pinned copy shipped alongside this header */
#include <stdbool.h>

/* ---- Private environment command registry --------------------------------
 * NEXT FREE SLOT: (6 | RETRO_ENVIRONMENT_PRIVATE) = 0x20006.
 * Register new commands HERE (never locally in a core or the host), with
 * direction, data type, and current users documented.
 */

/* 0x20001 — host→core. data: void** written with the NSView* hosting the
 * core's CAMetalLayer. Returns false when no native view is registered
 * (software-rendered cores). Users: duckstation, pcsx2, dolphin (query);
 * RetroNest environment_callbacks.cpp (serve). */
#define RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW (1u | RETRO_ENVIRONMENT_PRIVATE)

/* 0x20002 — host→core. data: const char** written with a UTF-8 resume-state
 * path, valid only for the synchronous duration of the env call (the core
 * must copy it). Queried during retro_load_game so PCSX2 can cold-resume via
 * VMBootParameters::save_state. The host marks the path consumed on read and
 * then skips its legacy post-load retro_unserialize fallback. Returns false
 * when no resume path is set. Users: pcsx2. */
#define RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH (2u | RETRO_ENVIRONMENT_PRIVATE)

/* 0x20003 / 0x20004 — host→core path overrides (user's Paths settings).
 * data: const char** written with a UTF-8 dir path (same lifetime rule as
 * 0x20002). Returns false when no override exists — the core falls back to
 * its save_dir-derived default, so non-RetroNest hosts work unchanged.
 * Users: pcsx2. */
#define RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR (3u | RETRO_ENVIRONMENT_PRIVATE)
#define RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR (4u | RETRO_ENVIRONMENT_PRIVATE)

/* 0x20005 — core→host, called during retro_load_game. data: a
 * retronest_game_identity*. The host copies both strings before returning.
 * Lets cores hand over a RetroAchievements hash + serial computed from
 * formats rcheevos can't hash by path (compressed RVZ/CHD via DiscIO).
 * Returns false if data is null. Users: duckstation, dolphin (call);
 * RetroNest (serve). */
#define RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY (5u | RETRO_ENVIRONMENT_PRIVATE)

/* Payload for SET_GAME_IDENTITY. Fields may be "" when unavailable, never
 * null. Layout is frozen ABI (2 pointers). */
struct retronest_game_identity {
    const char* ra_hash; /* rcheevos hash string */
    const char* serial;  /* game id, e.g. "SCUS-94900" / "GZ2P01" */
};

/* ---- Optional core exports ------------------------------------------------
 * The host resolves these via dlsym after dlopen; every one is optional —
 * cores that don't export them get the standard libretro behavior
 * (duckstation exports none of them and is fully conforming).
 *
 *   retronest_set_paused(true/false)      — host-initiated pause/resume for
 *       cores that pace internally (pcsx2, dolphin).
 *   retronest_set_fast_forward(on/off)    — engage the core's internal
 *       turbo/limiter (cores that ignore faster retro_run calls).
 *   retronest_shutdown_wedged() -> true   — the core detached a wedged VM
 *       thread during shutdown; the host MUST skip retro_deinit + dlclose
 *       (keep the dylib mapped) and refuse a new session in-process (pcsx2).
 */
typedef void (*retronest_set_paused_t)(bool paused);
typedef void (*retronest_set_fast_forward_t)(bool fast_forward);
typedef bool (*retronest_shutdown_wedged_t)(void);

#if defined(RETRONEST_LIBRETRO_CORE)
/* A core's defining TU should `#define RETRONEST_LIBRETRO_CORE` before
 * including this header so a signature mismatch is a compile error.
 * Definitions still carry RETRO_API for default visibility. */
#ifdef __cplusplus
extern "C" {
#endif
void retronest_set_paused(bool paused);
void retronest_set_fast_forward(bool fast_forward);
bool retronest_shutdown_wedged(void);
#ifdef __cplusplus
}
#endif
#endif /* RETRONEST_LIBRETRO_CORE */

#endif /* RETRONEST_LIBRETRO_H */
```

- [ ] **Step 5: Write the shared NSView helper**

Create `vendor/retronest-libretro/retronest_nsview.h`:

```cpp
// retronest-libretro — shared NSView-metrics query for Metal-backed cores.
// CANONICAL COPY: RetroNest-Project/vendor/retronest-libretro/. Do not edit
// vendored copies; see retronest_libretro.h for the sync/drift rules.
//
// Merged from duckstation's libretro_window.mm (which carries the
// main-thread dispatch fix) and pcsx2's MacNSViewMetrics.mm. AppKit requires
// UI-object access on the main thread; cores call this from their video
// threads, so QueryNSViewMetrics bounces the reads to the main queue via
// dispatch_sync when needed.
#pragma once

#include <cstdint>

namespace retronest {

struct NSViewMetrics {
    // Physical pixels (view bounds × backing scale). 0 when the view is
    // null or not yet realized — callers pick their own fallback.
    uint32_t surface_width = 0;
    uint32_t surface_height = 0;
    // Backing scale factor (1.0 non-Retina, 2.0 standard Retina).
    float surface_scale = 1.0f;
    // NSScreen.maximumFramesPerSecond in Hz; 0.0f when unknown — callers
    // pick their own fallback (pcsx2 uses 60.0f, duckstation lets the
    // Metal device decide).
    float refresh_rate = 0.0f;
};

// Main-thread-safe. Returns a zeroed struct (scale 1.0) if ns_view is null.
NSViewMetrics QueryNSViewMetrics(void* ns_view);

} // namespace retronest
```

Create `vendor/retronest-libretro/retronest_nsview.mm`:

```objc
// See retronest_nsview.h. Compiles under both ARC and MRC (__bridge casts
// are accepted in non-ARC translation units as plain casts).
#include "retronest_nsview.h"

#import <AppKit/AppKit.h>

namespace retronest {

namespace {

NSViewMetrics QueryOnMain(NSView* view)
{
    NSViewMetrics out{};
    if (view == nil)
        return out;

    const NSRect bounds = [view bounds];
    NSWindow* host_window = [view window];

    // Pick the screen the view is displayed on; fall back to the main
    // screen when the view isn't yet hosted in a window (early Acquire).
    NSScreen* screen = (host_window != nil) ? [host_window screen] : nil;
    if (screen == nil)
        screen = [NSScreen mainScreen];

    // Prefer the hosting window's backing scale (matches the layer's real
    // rendering target); only consult the screen if the window is unbacked.
    CGFloat scale = 1.0;
    if (host_window != nil)
        scale = [host_window backingScaleFactor];
    else if (screen != nil)
        scale = [screen backingScaleFactor];

    // NSScreen.maximumFramesPerSecond is macOS 12+; guard for older systems.
    float refresh = 0.0f;
    if (screen != nil && [screen respondsToSelector:@selector(maximumFramesPerSecond)])
    {
        const NSInteger fps = [screen maximumFramesPerSecond];
        if (fps > 0)
            refresh = static_cast<float>(fps);
    }

    out.surface_width  = static_cast<uint32_t>(bounds.size.width * scale);
    out.surface_height = static_cast<uint32_t>(bounds.size.height * scale);
    out.surface_scale  = static_cast<float>(scale);
    out.refresh_rate   = refresh;
    return out;
}

} // namespace

NSViewMetrics QueryNSViewMetrics(void* ns_view)
{
    NSView* view = (__bridge NSView*)ns_view;
    if ([NSThread isMainThread])
        return QueryOnMain(view);

    // Cores call this from video threads; AppKit reads must happen on the
    // main thread. dispatch_sync is safe here because the main run loop is
    // owned by the host app and never blocks on the video thread during
    // AcquireRenderWindow.
    __block NSViewMetrics out{};
    dispatch_sync(dispatch_get_main_queue(), ^{
        out = QueryOnMain(view);
    });
    return out;
}

} // namespace retronest
```

- [ ] **Step 6: Write the emit helper, README, and the two docs**

Create `vendor/retronest-libretro/emit_core_options_v2.h`:

```cpp
// retronest-libretro — shared SET_CORE_OPTIONS_V2 emission.
// CANONICAL COPY: RetroNest-Project/vendor/retronest-libretro/. Do not edit
// vendored copies; see retronest_libretro.h for the sync/drift rules.
#pragma once

#include "libretro.h"

namespace retronest {

// Sends the terminated v2 definition array with categories=nullptr —
// RetroNest's host adapter owns grouping (SettingDef.category), so cores
// never emit categories. Returns the env_cb result: false means the host
// lacks category support, but options ARE still registered and
// GET_VARIABLE works (libretro.h SET_CORE_OPTIONS_V2 contract) — callers
// may log an informational warning with their own logger.
inline bool EmitCoreOptionsV2(retro_environment_t cb,
                              const retro_core_option_v2_definition* defs)
{
    if (!cb || !defs)
        return false;
    retro_core_options_v2 opts{};
    opts.categories = nullptr;
    opts.definitions = const_cast<retro_core_option_v2_definition*>(defs);
    return cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts);
}

} // namespace retronest
```

Create `vendor/retronest-libretro/README.md`:

```markdown
# retronest-libretro — the RetroNest ⇄ core contract package

This directory is the CANONICAL copy of everything shared between the
RetroNest frontend and its libretro core forks:

| File | What |
|---|---|
| `libretro.h` | Pinned upstream libretro ABI header (single vintage suite-wide) |
| `retronest_libretro.h` | Private env-command registry, game-identity struct, optional-export prototypes |
| `retronest_nsview.h/.mm` | Main-thread-safe NSView metrics query (Metal cores) |
| `emit_core_options_v2.h` | Shared SET_CORE_OPTIONS_V2 emission |
| `docs/option-style-guide.md` | Grammar rules for NEW core options |
| `docs/deploy-contract.md` | Core packaging/deploy contract (zip layout, signing, paths) |

## Editing workflow

1. Edit files HERE (never in a fork's vendored copy).
2. Run `./sync.sh` — regenerates `MANIFEST.sha256` and copies the package
   into every adopting fork.
3. Rebuild + smoke-test each fork you re-synced.

Each fork verifies its copy against `MANIFEST.sha256` on every build
(`retronest_contract_check` CMake target running `check-drift.sh`), so a
locally edited vendored copy fails that fork's build.

## Adopters

- RetroNest-Project (canonical, via `cpp/CMakeLists.txt` include dirs)
- duckstation-libretro → `src/duckstation-libretro/retronest-libretro/`
- pcsx2-libretro → `pcsx2-libretro/retronest-libretro/`
- dolphin-libretro → `Source/Core/DolphinLibretro/retronest-libretro/`
- ppsspp-libretro: NOT yet (its `ppsspp-libretro/` scaffold has no code)
- mgba: NEVER while it ships as a stock upstream core

Each fork also keeps a 2-line forwarding `libretro.h` at its historical
header path so upstream-era `#include "libretro.h"` lines keep resolving.

## Refreshing the pinned libretro.h

    curl -fsSL -o libretro.h \
      https://raw.githubusercontent.com/libretro/RetroArch/master/libretro-common/include/libretro.h

then re-add the RetroNest banner block at the top, run `./sync.sh`, and
rebuild ALL adopters (an ABI-header bump is a suite-wide event).
```

Create `vendor/retronest-libretro/docs/option-style-guide.md`:

```markdown
# Core-option style guide

Binding for NEW options in all RetroNest core forks. Existing options are
grandfathered — migrating their values is a schema-breaking release and is
explicitly out of scope until one is planned.

- **Keys:** `<coreid>_<snake_case>` (`pcsx2_fast_forward_speed`). Never
  rename a shipped key (persisted in every user's options.json).
- **Booleans:** values `"enabled"` / `"disabled"`, default explicit.
  (duckstation's existing `"true"/"false"` options are grandfathered.)
- **Floats:** shortest round-trip text, no trailing zero padding: `"1.5"`,
  `"2"` — never `"1.500000"`.
- **Ints:** plain decimal, no unit suffix in the value (put units in the
  display label: `"Rewind granularity (frames)"`).
- **Value labels:** every non-obvious value gets a display label in the v2
  definition; raw enum ints (e.g. EXI device numbers) must never surface
  as user-visible values.
- **Naming across cores:** one concept, one suffix — check the other forks'
  option tables before inventing a new name for an existing concept
  (existing divergence like `cdrom_preload`/`cdvd_precache` is
  grandfathered; don't add a third name).
- **Categories:** cores emit `categories = nullptr`
  (see `emit_core_options_v2.h`) — grouping is host-side curation.
- **Wording:** `desc` = short sentence-case label; `info` = one-to-three
  tooltip sentences, no trailing period on `desc`, period(s) on `info`.
```

Create `vendor/retronest-libretro/docs/deploy-contract.md`:

```markdown
# Core deploy/packaging contract

The single description of how a RetroNest core reaches
`{root}/emulators/libretro/cores/`. CI consolidation into a shared script
is a planned follow-up (Packet 7 decision 5) — until then the three CI
workflows (pcsx2/ppsspp/dolphin `libretro_release.yml`) and duckstation's
`package.sh` each implement this contract independently. When you fix one,
check the others.

## Install layout (per core)

    {root}/emulators/libretro/cores/
      <core>_libretro.dylib             — the core (naming: no lib prefix)
      <core>_libretro.dylib.version     — per-core version sidecar (packet 6)
      <core>_libretro_resources/        — optional data payload (dolphin Sys/, ppsspp assets)
      <core>_libretro_libs/             — bundled non-system dylibs

## Release zip (`<core>_libretro.dylib.zip`)

Contains the dylib + `_resources/` + `_libs/` at the zip root. Release body
embeds the upstream merge-base short SHA.

## Dependency bundling (CI)

1. `dylibbundler -od -b -x <dylib> -d <core>_libretro_libs/ -p @loader_path/<core>_libretro_libs/`
2. **Flatten sibling refs**: for every lib inside `_libs/`, rewrite
   inter-lib references to `@loader_path/$(basename)` (`install_name_tool
   -change`) — dylibbundler's single prefix doubles paths for siblings
   (2026-07-04 bug, fixed at source in pcsx2 + dolphin CI).
3. Ad-hoc sign every lib and the core: `codesign --force --sign -`.
4. Assert zero `/usr/local` / `/opt/homebrew` references remain (`otool -L`).

## Arch policy

Declared per core in `manifests/<core>.json` `core_arch`
(`universal|x86_64|arm64`); `scripts/verify-universal.sh` checks every
manifest core. Current truth: duckstation/ppsspp/mgba universal; pcsx2 and
dolphin releases x86_64-only (Rosetta CI).

## Install-side (RetroNest emulator_installer.cpp)

Unzips into `cores/`, dequarantines + ad-hoc signs the core AND its
`_libs/`/`_resources/`, repairs doubled `@loader_path` refs (self-heal for
pre-fix releases), writes the per-core `.version` sidecar.
```

- [ ] **Step 7: Write sync.sh and check-drift.sh**

Create `vendor/retronest-libretro/sync.sh` and `chmod 755`:

```bash
#!/usr/bin/env bash
# Sync the canonical retronest-libretro package into the adopting core forks.
# Run after ANY edit to a package file. Regenerates MANIFEST.sha256 first, so
# the canonical tree and every vendored copy always agree.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECTS="$(cd "$HERE/../../.." && pwd)"   # …/Documents/Projects

FILES=(
  libretro.h
  retronest_libretro.h
  retronest_nsview.h
  retronest_nsview.mm
  emit_core_options_v2.h
  README.md
  check-drift.sh
  docs/option-style-guide.md
  docs/deploy-contract.md
)

cd "$HERE"
shasum -a 256 "${FILES[@]}" > MANIFEST.sha256
echo "regenerated MANIFEST.sha256 (${#FILES[@]} files)"

DESTS=(
  "$PROJECTS/duckstation-libretro/src/duckstation-libretro/retronest-libretro"
  "$PROJECTS/pcsx2-libretro/pcsx2-libretro/retronest-libretro"
  "$PROJECTS/dolphin-libretro/Source/Core/DolphinLibretro/retronest-libretro"
)
for dest in "${DESTS[@]}"; do
  if [ ! -d "$(dirname "$dest")" ]; then
    echo "SKIP (fork dir missing): $dest"
    continue
  fi
  mkdir -p "$dest/docs"
  for f in "${FILES[@]}"; do
    cp "$HERE/$f" "$dest/$f"
  done
  cp "$HERE/MANIFEST.sha256" "$dest/MANIFEST.sha256"
  chmod 755 "$dest/check-drift.sh"
  echo "synced -> $dest"
done
```

Create `vendor/retronest-libretro/check-drift.sh` and `chmod 755`:

```bash
#!/usr/bin/env bash
# Verify a vendored retronest-libretro copy matches its MANIFEST.sha256
# (catches local edits — the canonical copy lives in RetroNest-Project).
# Usage: check-drift.sh [package-dir]   (defaults to this script's dir)
set -euo pipefail
cd "${1:-"$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"}"
if shasum -a 256 --check --quiet MANIFEST.sha256; then
  echo "retronest-libretro: no drift"
else
  echo "retronest-libretro: DRIFT DETECTED — do not edit vendored copies." >&2
  echo "Edit RetroNest-Project/vendor/retronest-libretro/ and re-run sync.sh." >&2
  exit 1
fi
```

- [ ] **Step 8: Point RetroNest's build at the new directory**

In `cpp/CMakeLists.txt`, replace all six occurrences of
`${CMAKE_SOURCE_DIR}/../vendor/libretro-api` with
`${CMAKE_SOURCE_DIR}/../vendor/retronest-libretro` (lines 49, 149, 199, 319, 422, 748).

- [ ] **Step 9: Build and run the pin test**

```bash
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_retronest_contract -j 6
arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 -R RetronestContract --output-on-failure
```
Expected: PASS (3 test functions).

- [ ] **Step 10: Generate the manifest (without syncing yet) and commit**

```bash
cd vendor/retronest-libretro
FILES=(libretro.h retronest_libretro.h retronest_nsview.h retronest_nsview.mm emit_core_options_v2.h README.md check-drift.sh docs/option-style-guide.md docs/deploy-contract.md)
shasum -a 256 "${FILES[@]}" > MANIFEST.sha256
./check-drift.sh   # expected: "retronest-libretro: no drift"
cd ../..
git add vendor/retronest-libretro cpp/tests/test_retronest_contract.cpp cpp/CMakeLists.txt
git commit -m "packet7-1: canonical retronest-libretro contract package + ABI pin test"
```

(Do NOT run sync.sh yet — forks adopt in Tasks 3–5.)

---

### Task 2: RetroNest adoption + full suite + SMOKE GATE 1

**Files:**
- Modify: `cpp/src/core/libretro/environment_callbacks.h:1-56` (macro block + struct → include)
- Modify: `cpp/src/core/libretro/core_loader.h:34-57` (nested typedefs → package typedefs)
- Modify: `cpp/tests/fixtures/fake_libretro_core.c:1` (prototype-check the fake core's export)

**Interfaces:**
- Consumes: `retronest_libretro.h` from Task 1 (already on every target's include path via `retronest_core`'s PUBLIC include dirs).
- Produces: RetroNest compiles with zero local declarations of the private ABI; all downstream code (`environment_callbacks.cpp`, `core_runtime.cpp`, `core_loader.cpp`) is unchanged because macro/struct/typedef names are identical.

- [ ] **Step 1: Re-point environment_callbacks.h at the package**

In `cpp/src/core/libretro/environment_callbacks.h`, replace lines 1–56 (everything from `#pragma once` through the closing brace of `struct retronest_game_identity`) with:

```cpp
#pragma once
#include "libretro.h"
// Private RETRONEST_ENVIRONMENT_* commands + retronest_game_identity come
// from the vendored contract package (single source of truth for the
// host AND all core forks — see vendor/retronest-libretro/README.md).
#include "retronest_libretro.h"
#include <cstdint>
```

Everything from `#include <QByteArray>` (old line 58) down stays byte-identical.

- [ ] **Step 2: Re-point core_loader.h at the package typedefs**

In `cpp/src/core/libretro/core_loader.h`: add `#include "retronest_libretro.h"` directly under `#include "libretro.h"` (line 3). Then replace the three nested `using` declarations inside `CoreSymbols` (keeping every comment and the three member declarations exactly as they are) — lines 41, 49, and 56 change from e.g.

```cpp
    using retronest_set_paused_t = void (*)(bool);
```
to
```cpp
    using retronest_set_paused_t = ::retronest_set_paused_t;   // from retronest_libretro.h
```
and likewise `retronest_set_fast_forward_t` and `retronest_shutdown_wedged_t`. (Nested aliases keep `CoreSymbols::retronest_*_t` spellings in core_loader.cpp compiling unchanged.)

- [ ] **Step 3: Prototype-check the fake test core**

In `cpp/tests/fixtures/fake_libretro_core.c`, insert directly above the existing `#include "libretro.h"` (line 1):

```c
/* Compile-check this fake core's retronest_* export signatures against the
 * canonical contract prototypes. */
#define RETRONEST_LIBRETRO_CORE
#include "retronest_libretro.h"
```

- [ ] **Step 4: Full build + full test suite**

```bash
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 6
arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 --output-on-failure
```
Expected: build succeeds (macdeployqt POST_BUILD hook may re-run — that's normal and takes a few minutes); ALL tests pass (45 including RetronestContract).

- [ ] **Step 5: Sanity-launch the app yourself**

```bash
cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1 &
sleep 20 && kill %1
grep -i "qml\|error\|fatal" /tmp/rn.log | head -20
```
Expected: app reaches the main UI; no new errors vs. a normal launch.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/libretro/environment_callbacks.h cpp/src/core/libretro/core_loader.h cpp/tests/fixtures/fake_libretro_core.c
git commit -m "packet7-1: host adopts retronest-libretro contract header (no ABI change)"
```

- [ ] **Step 7: USER SMOKE GATE 1 — STOP and hand over**

Give the user this checklist and WAIT for confirmation (constants are byte-identical, so this is a regression sweep of every private-ABI path through the host dispatch):

> 1. Launch a **PS2 game** — video appears (NSView 0x20001), it cold-resumes if a resume state exists (0x20002), memcards land in your overridden folder if you have one (0x20003).
> 2. Open the in-game menu: **pause** works, **resume** works (retronest_set_paused). Toggle **fast-forward** (retronest_set_fast_forward). Quit cleanly.
> 3. Launch a **DuckStation game** with a RetroAchievements set — the achievements load (SET_GAME_IDENTITY 0x20005).

---

### Task 3: dolphin-libretro adoption + SMOKE GATE 2

**Files (repo: `~/Documents/Projects/dolphin-libretro`):**
- Create (via sync.sh): `Source/Core/DolphinLibretro/retronest-libretro/` (11 files)
- Modify: `Source/Core/DolphinLibretro/libretro.h` → 3-line forwarding header
- Modify: `Source/Core/DolphinLibretro/LibretroEnvironment.h:17-30`
- Modify: `Source/Core/DolphinLibretro/LibretroFrontend.cpp` (top: `RETRONEST_LIBRETRO_CORE` prototype check)
- Modify: `Source/Core/DolphinLibretro/CoreOptions.cpp:43-57` (adopt shared emit helper)
- Modify: `Source/Core/DolphinLibretro/CMakeLists.txt` (drift target)

**Interfaces:**
- Consumes: the package (Task 1) via `sync.sh`; canonical macro names `RETRONEST_ENVIRONMENT_*`, `retronest_game_identity`, `retronest::EmitCoreOptionsV2`.
- Produces: dolphin's `Environment::RETRONEST_GET_MACOS_NSVIEW` / `RETRONEST_SET_GAME_IDENTITY` / `RetroNestGameIdentity` become aliases of the canonical names — all existing dolphin call sites compile unchanged.

- [ ] **Step 1: Sync the package in**

```bash
~/Documents/Projects/RetroNest-Project/vendor/retronest-libretro/sync.sh
cd ~/Documents/Projects/dolphin-libretro
git status --short Source/Core/DolphinLibretro/retronest-libretro/
```
Expected: 11 new untracked files under `retronest-libretro/` (sync also writes into the other two forks — leave those uncommitted; Tasks 4–5 pick them up).

- [ ] **Step 2: Turn dolphin's libretro.h into a forwarder**

Replace the ENTIRE contents of `Source/Core/DolphinLibretro/libretro.h` with:

```c
/* Forwarder — the pinned ABI header lives in the vendored contract package.
 * See retronest-libretro/README.md. Do not put declarations here. */
#pragma once
#include "retronest-libretro/libretro.h"
```

(The quoted include resolves relative to this file's directory, so the 4 existing `#include "DolphinLibretro/libretro.h"` sites are untouched. Note: the pinned header is one upstream generation NEWER than dolphin's old copy — additive changes only; the build in Step 6 is the verification.)

- [ ] **Step 3: Re-point LibretroEnvironment.h at the canonical registry**

In `Source/Core/DolphinLibretro/LibretroEnvironment.h`, add under line 10's `#include "DolphinLibretro/libretro.h"`:

```cpp
#include "DolphinLibretro/retronest-libretro/retronest_libretro.h"
```

Then replace the two constexpr definitions and the struct (lines 14–30) with aliases (keep the surrounding comments trimmed to one pointer each):

```cpp
// Canonical values + docs live in retronest-libretro/retronest_libretro.h.
// These aliases keep dolphin's historical spellings compiling.
constexpr unsigned RETRONEST_GET_MACOS_NSVIEW = RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW;
constexpr unsigned RETRONEST_SET_GAME_IDENTITY = RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY;
using RetroNestGameIdentity = retronest_game_identity;
```

- [ ] **Step 4: Prototype-check dolphin's exports**

`Source/Core/DolphinLibretro/LibretroFrontend.cpp` defines `retronest_set_paused` (line ~343) and `retronest_set_fast_forward` (~354). At the top of the file, above its first include, add:

```cpp
// Compile-check the retronest_* export signatures against the contract.
#define RETRONEST_LIBRETRO_CORE
#include "DolphinLibretro/retronest-libretro/retronest_libretro.h"
```

(dolphin does not export `retronest_shutdown_wedged` — the header only declares prototypes, so an undefined function is fine.)

- [ ] **Step 5: Adopt the shared emit helper**

In `Source/Core/DolphinLibretro/CoreOptions.cpp`, add `#include "DolphinLibretro/retronest-libretro/emit_core_options_v2.h"` with the other includes, then replace the body of `EmitCoreOptionsV2` (lines 43–57) with:

```cpp
bool EmitCoreOptionsV2(retro_environment_t cb)
{
    const bool ok = retronest::EmitCoreOptionsV2(cb, BuildDefinitions().data());
    if (!ok) {
        CORE_OPTIONS_LOG(RETRO_LOG_WARN,
            "[CoreOptions] Host does not support core-option categories "
            "(options still registered; GET_VARIABLE will work)");
    }
    return ok;
}
```

- [ ] **Step 6: Wire the drift check and build both slices**

In `Source/Core/DolphinLibretro/CMakeLists.txt`, after the `add_library(dolphin_libretro MODULE)` block, add:

```cmake
# Packet 7: the vendored contract package must match its manifest — edits
# belong in RetroNest-Project/vendor/retronest-libretro/ + sync.sh.
add_custom_target(retronest_contract_check ALL
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/retronest-libretro/check-drift.sh
    COMMENT "retronest-libretro drift check"
    VERBATIM
)
```

Build both slices (dirs are already configured):
```bash
cmake --build build-libretro --target dolphin_libretro retronest_contract_check -j 8
arch -x86_64 cmake --build build-libretro-x86_64 --target dolphin_libretro retronest_contract_check -j 8
```
Expected: both succeed; each prints `retronest-libretro: no drift`.

- [ ] **Step 7: Verify byte-identical ABI (belt and braces)**

```bash
grep -rn "(1u | RETRO_ENVIRONMENT_PRIVATE)\|(5u | RETRO_ENVIRONMENT_PRIVATE)\|| 0x20000" \
  Source/Core/DolphinLibretro --include='*.cpp' --include='*.h' --include='*.mm' \
  | grep -v retronest-libretro/ | grep -v tools/test_harness.mm
```
Expected: zero hits — no hardcoded private-command values remain in dolphin outside the vendored package and `tools/test_harness.mm:40` (the harness literal is a deliberate contract pin — leave it).

- [ ] **Step 8: Deploy and commit**

```bash
./Source/Core/DolphinLibretro/tools/deploy.sh
git add Source/Core/DolphinLibretro
git commit -m "packet7-1: adopt retronest-libretro contract package (constants from canonical header, shared emit helper, drift check)"
```

- [ ] **Step 9: USER SMOKE GATE 2 — STOP and hand over**

> 1. Launch a **GameCube (RVZ) game** — boots, video fine, achievements identify (SET_GAME_IDENTITY via DiscIO).
> 2. Pause/resume + fast-forward from the in-game menu (dolphin's retronest_set_paused / _set_fast_forward).
> 3. Save a state, quit, resume it. Audio normal (regression-checks the packet 3 audio path against the new libretro.h vintage).

---

### Task 4: pcsx2-libretro adoption + SMOKE GATE 3

**Files (repo: `~/Documents/Projects/pcsx2-libretro`):**
- Add (already synced by Task 3 Step 1): `pcsx2-libretro/retronest-libretro/` (11 files)
- Modify: `pcsx2-libretro/libretro.h` → forwarding header
- Modify: `pcsx2-libretro/HostStubs.cpp` (~line 38 include, ~271 constant, ~292 Query call)
- Modify: `pcsx2-libretro/LibretroFrontend.cpp` (~175 constant; top: prototype check)
- Modify: `pcsx2-libretro/Settings.cpp` (~43-46 constants)
- Modify: `pcsx2-libretro/CoreOptions.cpp` (~55 emit helper)
- Delete: `pcsx2-libretro/MacNSViewMetrics.h`, `pcsx2-libretro/MacNSViewMetrics.mm`, `pcsx2-libretro/tools/__pycache__/`
- Modify: `pcsx2-libretro/CMakeLists.txt` (source swap, PCH skip, drift target)

**Interfaces:**
- Consumes: package headers; `retronest::QueryNSViewMetrics` (returns zeroed metrics on failure — pcsx2's call site already maps 0→640/448/1.0/60.0 fallbacks, so semantics are preserved).
- Produces: pcsx2 with zero local ABI declarations and the thread-safe NSView query (the deliberate behavior fix of this stage: `Host::AcquireRenderWindow` runs on the MTGS thread; the old `Mac::Query` touched AppKit off-main).

- [ ] **Step 1: Forwarding libretro.h**

Replace the ENTIRE contents of `pcsx2-libretro/libretro.h` (the copy WITH the old provenance banner — that banner's job is now done by the package) with:

```c
/* Forwarder — the pinned ABI header lives in the vendored contract package.
 * See retronest-libretro/README.md. Do not put declarations here. */
#pragma once
#include "retronest-libretro/libretro.h"
```

- [ ] **Step 2: Replace the four constant declarations**

All four sites keep their usage lines unchanged (the canonical macro names match pcsx2's local names exactly); only the local declarations are deleted:

1. `pcsx2-libretro/HostStubs.cpp`: add `#include "retronest-libretro/retronest_libretro.h"` under line 38's `#include "MacNSViewMetrics.h"` (which Step 3 replaces); delete the line `static constexpr unsigned RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW = (1 | 0x20000);` (~line 271) and its two "Hardcoded to avoid a cross-repo header dependency" comment lines — the include now provides the macro.
2. `pcsx2-libretro/LibretroFrontend.cpp`: add the same include with the file's local includes (next to `#include "LibretroFrontend.h"`); delete the `constexpr unsigned RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH = (2u | RETRO_ENVIRONMENT_PRIVATE);` declaration (~line 175-176) and trim its "Number must match RetroNest's environment_callbacks.h" comment block to `// Canonical value + docs: retronest-libretro/retronest_libretro.h.`
3. `pcsx2-libretro/Settings.cpp`: same include; delete the two `constexpr unsigned RETRONEST_ENVIRONMENT_GET_{MEMCARDS,TEXTURES}_DIR` declarations (~lines 43-46), keeping the behavioral comment above them.
4. At the very top of `LibretroFrontend.cpp` (it defines all three exports at ~326/795/830), make the include a prototype check:

```cpp
// Compile-check the retronest_* export signatures against the contract.
#define RETRONEST_LIBRETRO_CORE
```
placed immediately above the `#include "retronest-libretro/retronest_libretro.h"` line.

(`tools/test_settings_overrides.cpp:18-20` keeps its hardcoded literals — a standalone contract pin, same policy as dolphin's harness.)

- [ ] **Step 3: Swap MacNSViewMetrics for the shared helper**

In `pcsx2-libretro/HostStubs.cpp`:
- Line 38: `#include "MacNSViewMetrics.h"` → `#include "retronest-libretro/retronest_nsview.h"`
- ~Line 292: `const auto metrics = Pcsx2Libretro::Mac::Query(ns_view);` → `const auto metrics = retronest::QueryNSViewMetrics(ns_view);`
  (the `sw/sh/ss/rr` fallback lines below it stay byte-identical — they already handle zeroed metrics.)
- Above the Query call, extend the existing comment block with one line:
  `// Shared helper (retronest-libretro) — also fixes off-main-thread AppKit access: AcquireRenderWindow runs on the MTGS thread and the old local Query read NSView state without bouncing to main.`

```bash
git rm pcsx2-libretro/MacNSViewMetrics.h pcsx2-libretro/MacNSViewMetrics.mm
git rm -r pcsx2-libretro/tools/__pycache__
```

In `pcsx2-libretro/CMakeLists.txt`:
- In `target_sources` (line 26): `MacNSViewMetrics.mm` → `retronest-libretro/retronest_nsview.mm`
- Line 32: `set_source_files_properties(MacNSViewMetrics.mm …)` → `set_source_files_properties(retronest-libretro/retronest_nsview.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)`

- [ ] **Step 4: Adopt the emit helper + drift target**

`pcsx2-libretro/CoreOptions.cpp`: add `#include "retronest-libretro/emit_core_options_v2.h"`, replace the `EmitCoreOptionsV2` body (~line 55) with:

```cpp
bool EmitCoreOptionsV2(retro_environment_t cb)
{
    const bool ok = retronest::EmitCoreOptionsV2(cb, BuildDefinitions().data());
    if (!ok) {
        // Per libretro.h, false only means no category support — options are
        // still registered and GET_VARIABLE works.
        FrontendLog(RETRO_LOG_WARN,
            "[CoreOptions] Host does not support core-option categories "
            "(options are still registered and GET_VARIABLE will work)");
    }
    return ok;
}
```

`pcsx2-libretro/CMakeLists.txt` — after the `add_library(pcsx2_libretro MODULE)` block:

```cmake
# Packet 7: the vendored contract package must match its manifest — edits
# belong in RetroNest-Project/vendor/retronest-libretro/ + sync.sh.
add_custom_target(retronest_contract_check ALL
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/retronest-libretro/check-drift.sh
    COMMENT "retronest-libretro drift check"
    VERBATIM
)
```

- [ ] **Step 5: Build (x86_64 — the shipping arch) and deploy**

```bash
cd ~/Documents/Projects/pcsx2-libretro
arch -x86_64 cmake --build build-x86_64 --target pcsx2_libretro retronest_contract_check -j 8
find build-x86_64 -name 'pcsx2_libretro.dylib' -newer pcsx2-libretro/CMakeLists.txt
```
Expected: build succeeds, drift check prints `no drift`, `find` prints one fresh dylib path. Deploy it (the user's installed core is a local build linking their Homebrew, so a local rebuild is the correct deploy):

```bash
DYLIB=$(find build-x86_64 -name 'pcsx2_libretro.dylib' | head -1)
codesign --force --sign - "$DYLIB"
cp "$DYLIB" ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

- [ ] **Step 6: Commit**

```bash
git add pcsx2-libretro
git commit -m "packet7-1: adopt retronest-libretro contract package; shared thread-safe NSView query replaces MacNSViewMetrics"
```

- [ ] **Step 7: USER SMOKE GATE 3 — STOP and hand over**

This gate carries the stage's one deliberate behavior change (NSView query now bounces to the main thread), so watch startup extra carefully:

> 1. Launch a **PS2 game** — video appears promptly, **aspect ratio / letterboxing looks exactly as before** in both windowed and fullscreen (the metrics query changed internally).
> 2. Cold-resume from a resume state (boot-state path 0x20002); memcard override honored if set (0x20003).
> 3. Pause/resume, fast-forward, save state, **quit from the in-game menu** (the God of War quit case — retronest_shutdown_wedged path must stay quiet).

---

### Task 5: duckstation-libretro adoption + SMOKE GATE 4

**Files (repo: `~/Documents/Projects/duckstation-libretro`):**
- Add (already synced): `src/duckstation-libretro/retronest-libretro/` (11 files)
- Modify: `src/duckstation-libretro/libretro.h` → forwarding header
- Modify: `src/duckstation-libretro/libretro.cpp:349-361` (constant + struct → include), `:480` (rename)
- Modify: `src/duckstation-libretro/libretro_window.mm:51-133` (local metrics code → shared helper)
- Modify: `src/duckstation-libretro/CMakeLists.txt` (add .mm, PCH skip, drift target)

**Interfaces:**
- Consumes: package headers; `retronest::QueryNSViewMetrics`.
- Produces: duckstation with zero local ABI declarations. Its `libretro.h` jumps one upstream generation (2023 → the pin) — the full core build is the compile-compat verification.

- [ ] **Step 1: Forwarding libretro.h**

Replace the ENTIRE contents of `src/duckstation-libretro/libretro.h` with:

```c
/* Forwarder — the pinned ABI header lives in the vendored contract package.
 * See retronest-libretro/README.md. Do not put declarations here. */
#pragma once
#include "retronest-libretro/libretro.h"
```

- [ ] **Step 2: Replace the identity constant + struct in libretro.cpp**

In `src/duckstation-libretro/libretro.cpp`:
- Add `#include "retronest-libretro/retronest_libretro.h"` next to the existing `#include "libretro.h"` at the top.
- Delete the anonymous-namespace block at lines 355–361 (`constexpr unsigned RETRONEST_SET_GAME_IDENTITY …` + local `struct retronest_game_identity`), trimming the comment above it (lines 349–354) to:

```cpp
// RetroNest private env call: hand the host a precomputed RetroAchievements
// hash + serial (rcheevos can't hash a compressed .chd by path). Canonical
// command + struct: retronest-libretro/retronest_libretro.h.
```
- At line ~480 rename the usage: `RETRONEST_SET_GAME_IDENTITY` → `RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY`. (`retronest_game_identity` keeps its name — the canonical struct is identical. Search the file for any other `RETRONEST_SET_GAME_IDENTITY` occurrence and rename it too: `grep -n RETRONEST_SET_GAME_IDENTITY src/duckstation-libretro/libretro.cpp` must come back empty afterwards.)

- [ ] **Step 3: Re-point libretro_window.mm at the shared helper**

In `src/duckstation-libretro/libretro_window.mm`:
- Add includes next to `#include "libretro_internal.h"` (line 31):

```cpp
#include "retronest-libretro/retronest_libretro.h"
#include "retronest-libretro/retronest_nsview.h"
```
- Delete the whole anonymous namespace (lines 51–114: local `struct NSViewMetrics`, `QueryNSViewMetricsOnMain`, `QueryNSViewMetrics`) and the `#if __has_feature(objc_arc) #error` block (lines 39–41 — the shared helper is ARC-agnostic; this TU still builds MRC, which it tolerates).
- In `Host::AcquireRenderWindow`: delete the `static constexpr unsigned RETRONEST_GET_NSVIEW = (1u | 0x20000u);` line and its "Hardcoded to avoid a cross-repo header dependency" comment (lines 120–123); change the env call to use `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` and the error log text from `RETRONEST_GET_NSVIEW failed` to `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW failed`.
- Change the metrics call (line 133) to `const retronest::NSViewMetrics metrics = retronest::QueryNSViewMetrics(ns_view);` — the `wi.surface_*` lines below already map 0→1 fallbacks and cast, but the struct fields are now `uint32_t`, so update the two width/height assignments to cast explicitly:

```cpp
  wi.surface_width = static_cast<u16>((metrics.surface_width > 0) ? metrics.surface_width : 1);
  wi.surface_height = static_cast<u16>((metrics.surface_height > 0) ? metrics.surface_height : 1);
```
(scale/refresh lines stay as-is; note the old local code clamped to ≥1 inside the query while the shared helper returns raw 0 — the call-site fallback maps that to 1, so behavior is identical.)

- [ ] **Step 4: CMake — compile the shared .mm + drift target**

In `src/duckstation-libretro/CMakeLists.txt`:
- Add `retronest-libretro/retronest_nsview.mm` to the `add_library(duckstation_libretro MODULE …)` source list (after `libretro_window.mm`).
- Extend the PCH exclusion (line 12):

```cmake
set_source_files_properties(libretro_window.mm retronest-libretro/retronest_nsview.mm
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
```
- Append at the end of the file:

```cmake
# Packet 7: the vendored contract package must match its manifest — edits
# belong in RetroNest-Project/vendor/retronest-libretro/ + sync.sh.
add_custom_target(retronest_contract_check ALL
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/retronest-libretro/check-drift.sh
    COMMENT "retronest-libretro drift check"
    VERBATIM
)
```

- [ ] **Step 5: Build universal + deploy via package.sh**

```bash
cd ~/Documents/Projects/duckstation-libretro
./src/duckstation-libretro/package.sh
```
Expected: universal build succeeds on BOTH arches (x86_64 failure is fatal by design since `e41e430`), metallib compiled, core + resources + libs deployed to `~/Documents/RetroNest/emulators/libretro/cores/`, ad-hoc signed. This build is also the compile-compat proof for the newer pinned libretro.h.

- [ ] **Step 6: Commit**

```bash
git add src/duckstation-libretro
git commit -m "packet7-1: adopt retronest-libretro contract package (canonical constants, shared NSView helper, pinned libretro.h)"
```

- [ ] **Step 7: USER SMOKE GATE 4 — STOP and hand over**

> 1. Launch a **PS1 game** — video appears, aspect/letterboxing unchanged (shared metrics helper now in use here too).
> 2. Achievements identify and unlock feed works (SET_GAME_IDENTITY under its canonical name).
> 3. Save state, quit, cold-resume. Player 2 hot-plug still detected if quick to test (regression vs. the newer libretro.h).

---

### Task 6: Stage-1 wrap-up (verification + records)

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects/memory/retronest-suite-review-2026-07.md` (progress note)

**Interfaces:**
- Consumes: all four smoke-gate confirmations.

- [ ] **Step 1: Full-suite verification sweep**

```bash
cd ~/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 --output-on-failure
./scripts/verify-universal.sh
~/Documents/Projects/RetroNest-Project/vendor/retronest-libretro/check-drift.sh ~/Documents/Projects/duckstation-libretro/src/duckstation-libretro/retronest-libretro
~/Documents/Projects/RetroNest-Project/vendor/retronest-libretro/check-drift.sh ~/Documents/Projects/pcsx2-libretro/pcsx2-libretro/retronest-libretro
~/Documents/Projects/RetroNest-Project/vendor/retronest-libretro/check-drift.sh ~/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/retronest-libretro
```
Expected: all tests pass; verify-universal passes; three × `no drift`.

- [ ] **Step 2: Confirm nothing hardcodes the ABI outside pins**

```bash
grep -rn "0x20001\|0x20002\|0x20003\|0x20004\|0x20005\|| 0x20000" \
  ~/Documents/Projects/RetroNest-Project/cpp/src \
  ~/Documents/Projects/duckstation-libretro/src/duckstation-libretro \
  ~/Documents/Projects/pcsx2-libretro/pcsx2-libretro \
  ~/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro \
  --include='*.cpp' --include='*.h' --include='*.mm' \
  | grep -v retronest-libretro/ | grep -v test_
```
Expected: zero hits (test files and the vendored package are the only remaining literal carriers).

- [ ] **Step 3: Update memory + report status**

Append to the memory file's packet log: Stage 1 of packet 7 done — canonical package `vendor/retronest-libretro/` (commit hashes per repo), all four repos adopted + user-smoked, **local commits in 4 repos NOT pushed pending user's final say-so**, pcsx2 NSView thread fix shipped, checkers/hand-schemas untouched (die in Stage 2). Then summarize to the user: per-repo commit list, what to push when ready, and that Stage 2 (schema single-source) starts with the CoreProber spike.
