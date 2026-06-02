# DuckStation libretro core — Phase 1 skeleton boot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `duckstation_libretro.dylib` (universal arm64+x86_64) plus minimal RetroNest wiring so a PS1 game boots end-to-end through RetroNest — load, render (Metal-to-NSView), sound, digital-pad input, clean exit.

**Architecture:** New in-tree libretro frontend target inside the current-upstream DuckStation fork (`duckstation-libretro/src/duckstation-libretro/`), written fresh against DuckStation's current `Host::`/`Core::`/`System::` + `GPUDevice` surface (SwanStation's shim is reference-only; its `HostInterface`/`HostDisplay` base classes no longer exist). Single-threaded inline run loop (`VideoThread` non-threaded, `System::Execute()` interrupted at `FrameDone`). Renderer drives DuckStation's existing `MetalDevice`, which builds its `CAMetalLayer` on RetroNest's provided NSView (delivered via `Host::AcquireRenderWindow` + the `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` private env callback). PCSX2's in-tree frontend is the structural template.

**Tech Stack:** C++20, CMake, Apple Metal, libretro ABI, Qt (RetroNest side). Build via the DuckStation fork's CMake; test by launching through RetroNest.

**Companion docs:**
- Spec: `docs/superpowers/specs/2026-06-01-duckstation-libretro-skeleton-design.md`
- Delta report: `/Users/mark/Documents/Projects/duckstation-libretro/docs/swanstation-delta-2026-06-01.md`

**Key paths:**
- Fork: `/Users/mark/Documents/Projects/duckstation-libretro/`
- New frontend dir: `duckstation-libretro/src/duckstation-libretro/`
- RetroNest: `/Users/mark/Documents/Projects/RetroNest-Project/`
- PCSX2 template: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/` (`LibretroFrontend.cpp`, `HostStubs.cpp`, `LibretroAudioStream.cpp`, `CMakeLists.txt`, `CoreResources.cpp`)
- SwanStation reference: `/Users/mark/Documents/Projects/swanstation/src/libretro/`

**License constraint (non-negotiable):** Never `git push` the DuckStation fork or the `.dylib` to any remote. Never bundle into the public RetroNest variant. Commits are local-only.

---

## Verification model

This is C++ emulator integration; most verification is **build + link + manual smoke through RetroNest**, not unit tests. Where logic is pure (RetroPad→bind mapping, settings-key mapping, audio frame-count math) there are real unit tests. "Smoke" steps below give the exact observable (a logged line, a frame on screen, audio) — treat a missing observable as failure and debug before proceeding.

Build commands assume the fork configures like upstream DuckStation. Confirm the exact CMake invocation in Task A1; subsequent tasks reuse it via the shell var `$DS` (fork root) and the build dirs `build-arm64` / `build-x86_64`.

---

## File structure (locked decomposition)

DuckStation fork — new files under `duckstation-libretro/src/duckstation-libretro/`:

| File | Responsibility |
|---|---|
| `libretro.cpp` | All `retro_*` entry points; init/boot/run/unload; env-callback plumbing; the single-frame driver; RetroPad→controller mapping. |
| `libretro_host.cpp` | The full `Host::` linker contract (lifecycle no-ops + the ~12 real functions, incl. `AcquireRenderWindow`, resource-file access, `FrameDoneOnVideoThread`). |
| `libretro_settings.cpp` / `.h` | Map libretro core options → base `INISettingsInterface`; set `EmuFolders`; `MapRetroPad` lives here for unit-testability. |
| `libretro_audio.cpp` / `.h` | Capturing `AudioStream` subclass + drain to `retro_audio_sample_batch`. |
| `libretro_core_options.h` | Phase-1-minimal core-option table. |
| `CMakeLists.txt` | `duckstation_libretro` MODULE target. |

DuckStation fork — small modifications to existing files:

| File | Change |
|---|---|
| `src/core/spu.cpp` (`CreateOutputStream`) | Route the SPU output stream to the capturing libretro audio stream when libretro mode is active. |
| `src/CMakeLists.txt` (top of `src/`) | `option(ENABLE_LIBRETRO ...)` + `add_subdirectory(duckstation-libretro)`. |

RetroNest — new + modified:

| File | Change |
|---|---|
| `cpp/src/adapters/libretro/duckstation_libretro_adapter.h` (+ `.cpp` if needed) | Skeleton `LibretroAdapter` subclass. |
| `cpp/src/adapters/adapter_registry.cpp` | Swap the `"duckstation"` registration from process adapter to libretro adapter. |
| `manifests/duckstation.json` | Convert to libretro form, omit `github_repo`. |
| `cpp/src/core/manifest_loader.cpp` | Relax `validateManifest` to allow empty `github_repo` when `backend=="libretro"`. |
| `cpp/src/services/emulator_service.cpp` | Guard the update-check loop against empty `github_repo`. |

---

## Phase A — RetroNest wiring + loadable stub

Goal: RetroNest recognizes a libretro DuckStation core and `dlopen`s a stub dylib with all required symbols. Establishes the integration boundary before any emulation logic.

### Task A1: Confirm the fork builds, capture the build recipe

**Files:** none (investigation + a scratch note).

- [ ] **Step 1: Configure + build the existing fork once (arm64)**

```bash
export DS=/Users/mark/Documents/Projects/duckstation-libretro
cd "$DS"
cmake -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-arm64 --target core util common 2>&1 | tail -20
```

Expected: `core`, `util`, `common` static libs build successfully. If CMake options differ (e.g. a preset in `CMakePresets.json`), record the working invocation; all later build steps reuse it.

- [ ] **Step 2: Record the recipe**

Write the exact working configure+build commands into the top of `duckstation-libretro/src/duckstation-libretro/BUILD_NOTES.md` (create the dir). This is the canonical build recipe for the rest of the plan.

- [ ] **Step 3: Commit (fork repo — init if needed)**

```bash
cd "$DS"
[ -d .git ] || git init -q   # fork may not be a git repo yet; local-only, never push
git add src/duckstation-libretro/BUILD_NOTES.md
git commit -m "chore(libretro): record build recipe for skeleton port"
```

### Task A2: Relax manifest validation for libretro local-only cores

**Files:**
- Modify: `RetroNest-Project/cpp/src/core/manifest_loader.cpp:111-116` (the `validateManifest` required-field checks)

- [ ] **Step 1: Read the current validation**

Read `manifest_loader.cpp:105-120`. The required set is `id`, `name`, `systems`, `github_repo`, `executable`. We need `github_repo` to be optional when `backend=="libretro"`.

- [ ] **Step 2: Make `github_repo` conditional**

Change the `github_repo`-required check so it only fails when `backend != "libretro"`. Concretely, replace the unconditional `github_repo.isEmpty()` rejection with:

```cpp
// Libretro cores may be local-only (no GitHub release); their dylib is
// hand-placed / built locally. Only process-backend emulators require a repo.
if (m.backend != "libretro" && m.github_repo.isEmpty()) {
    qWarning() << "[Manifest] Rejecting" << m.id << "— missing github_repo";
    return false;
}
```

(Keep `id`, `name`, `systems`, `executable` unconditionally required.)

- [ ] **Step 3: Build RetroNest to confirm it compiles**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build build --target RetroNest 2>&1 | tail -20    # use the project's actual build dir/target
```

Expected: compiles. (If the build dir/target differ, use the project's standard build command from its CLAUDE.md.)

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/manifest_loader.cpp
git commit -m "feat(manifest): allow empty github_repo for libretro backend"
```

### Task A3: Guard the update-check loop against empty github_repo

**Files:**
- Modify: `RetroNest-Project/cpp/src/services/emulator_service.cpp:263-272` (collection loop)

- [ ] **Step 1: Add the guard**

In the collection loop (after the `adapter`/`isInstalled` check, before `items.append`), add:

```cpp
if (emu.github_repo.isEmpty()) continue;   // local-only core: no remote to check
```

- [ ] **Step 2: Build + commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build build --target RetroNest 2>&1 | tail -5
git add cpp/src/services/emulator_service.cpp
git commit -m "feat(updates): skip update check for local-only (no github_repo) cores"
```

### Task A4: Skeleton RetroNest adapter + registry swap

**Files:**
- Create: `RetroNest-Project/cpp/src/adapters/libretro/duckstation_libretro_adapter.h`
- Modify: `RetroNest-Project/cpp/src/adapters/adapter_registry.cpp:2,16`

- [ ] **Step 1: Write the skeleton adapter header**

Model on `pcsx2_libretro_adapter.h` and the leaner `mgba_libretro_adapter.h`. Only `coreId()` is pure-virtual; everything else has defaults.

```cpp
#pragma once
#include "libretro_adapter.h"

// Minimal skeleton-phase DuckStationLibretroAdapter.
// coreId() is the only pure-virtual on LibretroAdapter; the registry only
// instantiates concrete subclasses, so we need a named class to register.
// Settings schema, controller types, RA console id, resume lookup, etc. are
// deferred to follow-on specs (see 2026-06-01-duckstation-libretro-skeleton-design.md).
class DuckStationLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "duckstation"; }
    HardwareRenderBackend hardwareRenderBackend() const override {
        return HardwareRenderBackend::MetalNSView;
    }
    // PS1 → rcheevos console id 12, but RA is out of scope for the skeleton;
    // returning 0 keeps rcheevos disabled until the RA sub-spec wires it.
    int raConsoleId(const QString& systemId) const override { return 0; }
};
```

- [ ] **Step 2: Swap the registry**

In `adapter_registry.cpp`: change the include at line 2 from `#include "duckstation_adapter.h"` to `#include "libretro/duckstation_libretro_adapter.h"`, and line 16 from `registerAdapter("duckstation", std::make_unique<DuckStationAdapter>());` to:

```cpp
registerAdapter("duckstation", std::make_unique<DuckStationLibretroAdapter>());
```

Note: the standalone `duckstation_adapter.{h,cpp}` stays in the tree (still compiled if referenced elsewhere) as the UI-surface reference; only the registration changes.

- [ ] **Step 3: Build RetroNest**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build build --target RetroNest 2>&1 | tail -20
```

Expected: compiles and links. If `DuckStationAdapter` becomes unreferenced and the linker drops it, that's fine; if it errors on an unused-include, remove the stale include.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/libretro/duckstation_libretro_adapter.h cpp/src/adapters/adapter_registry.cpp
git commit -m "feat(adapter): register skeleton DuckStation libretro adapter"
```

### Task A5: Libretro manifest

**Files:**
- Modify: `RetroNest-Project/manifests/duckstation.json`

- [ ] **Step 1: Convert to libretro form**

```json
{
  "id": "duckstation",
  "name": "DuckStation",
  "description": "PlayStation 1 emulator (libretro core).",
  "systems": ["psx"],
  "executable": "duckstation_libretro.dylib",
  "install_folder": "libretro",
  "rom_extensions": ["bin", "cue", "iso", "img", "pbp", "chd", "m3u"],
  "launch_args": [],
  "backend": "libretro",
  "core_dylib": "duckstation_libretro.dylib"
}
```

(No `github_repo`. `executable` must be non-empty — set to the dylib name.)

- [ ] **Step 2: Smoke — manifest loads**

Launch RetroNest; confirm DuckStation appears as an emulator and is not rejected at load. Check logs for any `[Manifest] Rejecting duckstation` line (there should be none).

- [ ] **Step 3: Commit**

```bash
git add manifests/duckstation.json
git commit -m "feat(manifest): convert duckstation to local-only libretro core"
```

### Task A6: Stub dylib that loads in RetroNest

**Files:**
- Create: `duckstation-libretro/src/duckstation-libretro/libretro.cpp` (stub)
- Create: `duckstation-libretro/src/duckstation-libretro/CMakeLists.txt`
- Modify: `duckstation-libretro/src/CMakeLists.txt`

- [ ] **Step 1: Write a stub `libretro.cpp` exporting all 22 required symbols**

These 22 are mandatory (`core_loader.cpp:28-49`): `retro_api_version, retro_init, retro_deinit, retro_set_environment, retro_set_video_refresh, retro_set_audio_sample, retro_set_audio_sample_batch, retro_set_input_poll, retro_set_input_state, retro_get_system_info, retro_get_system_av_info, retro_set_controller_port_device, retro_reset, retro_run, retro_load_game, retro_unload_game, retro_get_region, retro_serialize_size, retro_serialize, retro_unserialize, retro_get_memory_data, retro_get_memory_size`.

```cpp
#include "libretro.h"          // vendored libretro header (copy from pcsx2-libretro or dep/)
#include <cstring>

namespace { retro_environment_t g_environ = nullptr; }

extern "C" {
RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }
RETRO_API void retro_init(void) {}
RETRO_API void retro_deinit(void) {}
RETRO_API void retro_set_environment(retro_environment_t cb) { g_environ = cb; }
RETRO_API void retro_set_video_refresh(retro_video_refresh_t) {}
RETRO_API void retro_set_audio_sample(retro_audio_sample_t) {}
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t) {}
RETRO_API void retro_set_input_poll(retro_input_poll_t) {}
RETRO_API void retro_set_input_state(retro_input_state_t) {}
RETRO_API void retro_get_system_info(retro_system_info* info) {
  std::memset(info, 0, sizeof(*info));
  info->library_name = "DuckStation";
  info->library_version = "0.1-skeleton";
  info->valid_extensions = "bin|cue|iso|img|pbp|chd|m3u";
  info->need_fullpath = true;
  info->block_extract = true;
}
RETRO_API void retro_get_system_av_info(retro_system_av_info* info) {
  std::memset(info, 0, sizeof(*info));
  info->geometry.base_width = 320; info->geometry.base_height = 240;
  info->geometry.max_width = 1024; info->geometry.max_height = 512;
  info->geometry.aspect_ratio = 4.0f / 3.0f;
  info->timing.fps = 59.94; info->timing.sample_rate = 44100.0;
}
RETRO_API void retro_set_controller_port_device(unsigned, unsigned) {}
RETRO_API void retro_reset(void) {}
RETRO_API void retro_run(void) {}
RETRO_API bool retro_load_game(const retro_game_info*) { return false; }   // not yet
RETRO_API void retro_unload_game(void) {}
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void*, size_t) { return false; }
RETRO_API bool retro_unserialize(const void*, size_t) { return false; }
RETRO_API void* retro_get_memory_data(unsigned) { return nullptr; }
RETRO_API size_t retro_get_memory_size(unsigned) { return 0; }
RETRO_API bool retro_load_game_special(unsigned, const retro_game_info*, size_t) { return false; }
RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned, bool, const char*) {}
}
```

Copy a `libretro.h` into the new dir (from `/Users/mark/Documents/Projects/swanstation/dep/libretro-common/include/libretro.h` or pcsx2's `pcsx2-libretro/libretro.h`).

- [ ] **Step 2: Write the CMake target** (model on `pcsx2-libretro/CMakeLists.txt`)

```cmake
add_library(duckstation_libretro MODULE
  libretro.cpp
)
target_include_directories(duckstation_libretro PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}            # libretro.h
  ${CMAKE_SOURCE_DIR}/src                # core/util/common headers
)
set_target_properties(duckstation_libretro PROPERTIES
  PREFIX ""
  OUTPUT_NAME "duckstation_libretro"
  SUFFIX ".dylib"                        # force .dylib (MODULE default is .so)
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
# Later tasks add: target_link_libraries(duckstation_libretro PRIVATE core util common ...)
```

- [ ] **Step 3: Wire into the build**

In `duckstation-libretro/src/CMakeLists.txt` add near the other subdirs:

```cmake
option(ENABLE_LIBRETRO "Build the libretro core (duckstation_libretro.dylib)" OFF)
if(ENABLE_LIBRETRO)
  add_subdirectory(duckstation-libretro)
endif()
```

- [ ] **Step 4: Build the stub**

```bash
cd "$DS"
cmake -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DENABLE_LIBRETRO=ON
cmake --build build-arm64 --target duckstation_libretro 2>&1 | tail -20
ls -la build-arm64/src/duckstation-libretro/duckstation_libretro.dylib
nm -gU build-arm64/src/duckstation-libretro/duckstation_libretro.dylib | grep -c '_retro_'
```

Expected: dylib produced; the `nm` count shows ≥22 exported `retro_*` symbols.

- [ ] **Step 5: Smoke — RetroNest loads the stub**

Place the dylib where RetroNest's `install_folder: "libretro"` expects it (the `libretro` cores dir under the emulators path — confirm via `Paths::emulatorsDir("libretro")`). Launch a PS1 game from RetroNest. Expected: RetroNest `dlopen`s the core (no "missing symbol" / "failed to resolve" errors in logs), then fails gracefully because `retro_load_game` returns false. **The success criterion here is the clean load + the load-game-returns-false path, not a running game.**

- [ ] **Step 6: Commit**

```bash
cd "$DS"
git add src/duckstation-libretro/ src/CMakeLists.txt
git commit -m "feat(libretro): loadable stub core with all required retro_* exports"
```

---

## Phase B — Universal build + resources

Goal: produce a universal arm64+x86_64 dylib and bundle the runtime resources DuckStation needs.

### Task B1: Build x86_64 + lipo to universal

**Files:**
- Create: `duckstation-libretro/src/duckstation-libretro/build-universal.sh`

- [ ] **Step 1: Confirm x86_64 builds**

```bash
cd "$DS"
cmake -B build-x86_64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DENABLE_LIBRETRO=ON
cmake --build build-x86_64 --target duckstation_libretro 2>&1 | tail -20
```

Expected: x86_64 dylib builds. (DuckStation has x64 + arm64 recompilers, so both arches are real — unlike PCSX2.)

- [ ] **Step 2: Write the lipo script**

```bash
#!/usr/bin/env bash
set -euo pipefail
DS="$(cd "$(dirname "$0")/../../.." && pwd)"
A="$DS/build-arm64/src/duckstation-libretro/duckstation_libretro.dylib"
X="$DS/build-x86_64/src/duckstation-libretro/duckstation_libretro.dylib"
OUT="$DS/build-universal/duckstation_libretro.dylib"
mkdir -p "$(dirname "$OUT")"
lipo -create "$A" "$X" -output "$OUT"
lipo -info "$OUT"
```

- [ ] **Step 3: Run it, verify universal**

```bash
chmod +x "$DS/src/duckstation-libretro/build-universal.sh"
"$DS/src/duckstation-libretro/build-universal.sh"
```

Expected: `lipo -info` reports `arm64 x86_64`.

- [ ] **Step 4: Commit**

```bash
git add src/duckstation-libretro/build-universal.sh
git commit -m "build(libretro): universal arm64+x86_64 via lipo"
```

### Task B2: Determine + bundle runtime resources

**Files:**
- (Possibly) Create: a resources-copy step or note in `BUILD_NOTES.md`.

- [ ] **Step 1: Identify what resources the core reads**

Search the fork for resource reads to learn what must ship beside the dylib:

```bash
cd "$DS"
grep -rn "ReadResourceFile\|ResourceFileExists\|EmuFolders::Resources\|gamedb\|\.yaml\|gamecontrollerdb" src/core/*.cpp | grep -i "resource\|gamedb\|database" | head -20
ls data/resources 2>/dev/null | head
```

Expected: a `data/resources/` dir in the fork (game database, shaders, etc.). Record the list in `BUILD_NOTES.md`.

- [ ] **Step 2: Decide Metal shader handling**

Check whether DuckStation compiles Metal shaders at runtime (via its shadergen + `MTLDevice newLibraryWithSource`) or needs a prebuilt `.metallib`:

```bash
grep -rn "newLibraryWithSource\|newLibraryWithURL\|metallib\|MTLLibrary" src/util/metal_device.mm | head
```

Expected: if `newLibraryWithSource` dominates, shaders are runtime-generated and **no external metallib is needed** (unlike PCSX2). Record the finding. If a metallib IS required, add its compilation to `build-universal.sh` mirroring PCSX2's workflow recipe.

- [ ] **Step 3: Establish the resources sibling dir**

DuckStation locates resources via `EmuFolders::Resources`. The libretro core must point `EmuFolders::Resources` at a dir shipped beside the dylib (resolved at runtime via `dladdr`, like PCSX2's `CoreResources::ResolveResourcesDir`). For now, document the required layout in `BUILD_NOTES.md`: `duckstation_libretro_resources/` sibling to the dylib, containing the contents of `data/resources/`. Actual wiring happens in Task D2 (`EmuFolders` setup) and Task C-host (resource-file reads).

- [ ] **Step 4: Commit**

```bash
git add src/duckstation-libretro/BUILD_NOTES.md
git commit -m "docs(libretro): record runtime resource requirements"
```

---

## Phase C — Host contract

Goal: implement the `Host::` linker contract so the core links and core-thread/resource/window plumbing works. Reference: agent-extracted contract (below) + PCSX2's `HostStubs.cpp`.

### Task C1: Implement the no-op / trivial Host functions

**Files:**
- Create: `duckstation-libretro/src/duckstation-libretro/libretro_host.cpp`
- Modify: `duckstation-libretro/src/duckstation-libretro/CMakeLists.txt` (add the source + link `core util common`)

- [ ] **Step 1: Implement the no-op-safe set**

These can be no-ops/trivial returns for a skeleton. Signatures verbatim from `core/system_private.h`, `core/host.h`, `core/core_private.h`:

```cpp
#include "core/host.h"
#include "core/system.h"
// ... (other core headers as needed)

void Host::OnSettingsReloaded() {}
void Host::OnSystemStarting() {}
void Host::OnSystemStarted() {}
void Host::OnSystemStopping() {}
void Host::OnSystemDestroyed() {}
void Host::OnSystemPaused() {}
void Host::OnSystemResumed() {}
void Host::OnSystemAbnormalShutdown(const std::string_view reason) { /* log */ }
void Host::OnSystemGameChanged(const std::string& disc_path, const std::string& game_serial,
                               const std::string& game_name, GameHash game_hash) {}
void Host::OnSystemUndoStateAvailabilityChanged(bool, u64) {}
void Host::OnMediaCaptureStarted() {}
void Host::OnMediaCaptureStopped() {}
void Host::OpenURL(std::string_view) {}
std::string Host::GetClipboardText() { return {}; }
bool Host::CopyTextToClipboard(std::string_view) { return false; }
std::span<const std::pair<const char*, const char*>> Host::GetAvailableLanguageList() { return {}; }
const char* Host::GetLanguageName(std::string_view) { return ""; }
bool Host::ChangeLanguage(const char*) { return false; }
void Host::RunOnUIThread(std::function<void()> f, bool) { if (f) f(); }
void Host::QueueAsyncTask(std::function<void()> f) { if (f) f(); }
void Host::WaitForAllAsyncTasks() {}
void Host::CommitBaseSettingChanges() {}
bool Host::SetScreensaverInhibit(bool, Error*) { return true; }
void Host::OnSettingsResetToDefault(bool, bool, bool) {}
void Host::ReportErrorAsync(std::string_view title, std::string_view message) { /* log */ }
void Host::ReportStatusMessage(std::string_view) {}
void Host::ConfirmMessageAsync(std::string_view, std::string_view, std::string_view,
                               ConfirmMessageAsyncCallback callback, std::string_view, std::string_view) {
  if (callback) callback(true);
}
```

Confirm exact signatures against the headers as you go (the extraction lists file:line for each); add any additional optional-subsystem `Host::` functions only if the linker reports them missing.

- [ ] **Step 2: Add to CMake + link core libs**

In the new `CMakeLists.txt`, add `libretro_host.cpp` to the sources and:

```cmake
target_link_libraries(duckstation_libretro PRIVATE core util common)
```

- [ ] **Step 3: Build, read the linker errors as a worklist**

```bash
cd "$DS"
cmake --build build-arm64 --target duckstation_libretro 2>&1 | tee /tmp/ds_link.log | tail -40
grep -i "undefined symbol" /tmp/ds_link.log | grep -i "Host::" | sort -u
```

Expected: remaining undefined `Host::` symbols are exactly the "real" ones implemented in Task C2. Use this list to confirm completeness.

- [ ] **Step 4: Commit**

```bash
git add src/duckstation-libretro/libretro_host.cpp src/duckstation-libretro/CMakeLists.txt
git commit -m "feat(libretro): Host contract — no-op/trivial functions"
```

### Task C2: Implement the real Host functions

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro_host.cpp`

- [ ] **Step 1: Core-thread identity + execution**

For the inline single-threaded model, the core thread IS the calling (libretro) thread. Store a `Threading::ThreadHandle` for the current thread at `retro_init` (exposed via an extern so `libretro.cpp` sets it) and return it:

```cpp
namespace { Threading::ThreadHandle s_core_thread; }   // set in retro_init
namespace LibretroHost { void SetCoreThread(Threading::ThreadHandle h) { s_core_thread = std::move(h); } }

const Threading::ThreadHandle& Host::GetCoreThreadHandle() { return s_core_thread; }
bool Host::IsOnCoreThread() { return s_core_thread.IsCallingThread(); }
void Host::RunOnCoreThread(std::function<void()> f, bool /*block*/) { if (f) f(); }  // inline: run now
```

Verify `Threading::ThreadHandle` API (`GetForCallingThread()` / `IsCallingThread()`) in `common/threading.h`.

- [ ] **Step 2: Resource-file access (game DB etc.)**

Implement against `EmuFolders::Resources` (set in Task D2). Read files from disk:

```cpp
bool Host::ResourceFileExists(std::string_view filename, bool allow_override) {
  return FileSystem::FileExists(Path::Combine(EmuFolders::Resources, filename).c_str());
}
std::optional<DynamicHeapArray<u8>> Host::ReadResourceFile(std::string_view filename, bool, Error* error) {
  return FileSystem::ReadBinaryFile(Path::Combine(EmuFolders::Resources, filename).c_str(), error);
}
std::optional<std::string> Host::ReadResourceFileToString(std::string_view filename, bool, Error* error) {
  return FileSystem::ReadFileToString(Path::Combine(EmuFolders::Resources, filename).c_str(), error);
}
std::optional<std::time_t> Host::GetResourceFileTimestamp(std::string_view filename, bool) {
  FILESYSTEM_STAT_DATA sd;
  if (!FileSystem::StatFile(Path::Combine(EmuFolders::Resources, filename).c_str(), &sd)) return std::nullopt;
  return sd.ModificationTime;
}
```

Confirm exact `FileSystem::` signatures in `common/file_system.h`.

- [ ] **Step 3: Fatal error + default settings**

```cpp
[[noreturn]] void Host::ReportFatalError(std::string_view title, std::string_view message) {
  // log title+message, then abort — libretro has no recovery path for a fatal core error
  std::abort();
}
void Host::SetDefaultSettings(SettingsInterface& si) { /* skeleton: we set settings explicitly; leave empty */ }
```

- [ ] **Step 4: Window-info type + resize**

```cpp
WindowInfoType Host::GetRenderWindowInfoType() { return WindowInfoType::MacOS; }
void Host::RequestResizeHostDisplay(s32 width, s32 height) { /* Task F: forward to retro_set_geometry */ }
void Host::RequestSystemShutdown(bool, bool, bool) { /* Task E: set a flag retro_run checks */ }
```

- [ ] **Step 5: Build to confirm only `AcquireRenderWindow` / `FrameDoneOnVideoThread` / `PumpMessagesOnCoreThread` remain**

```bash
cmake --build build-arm64 --target duckstation_libretro 2>&1 | grep -i "undefined symbol" | grep -i "Host::" | sort -u
```

Expected: the only remaining undefined `Host::` symbols are `AcquireRenderWindow` (Task F1), `ReleaseRenderWindow`, `FrameDoneOnVideoThread` (Task F2), and `PumpMessagesOnCoreThread` (Task E2). These are implemented in their respective tasks.

- [ ] **Step 6: Commit**

```bash
git add src/duckstation-libretro/libretro_host.cpp
git commit -m "feat(libretro): Host contract — real functions (thread, resources, errors)"
```

---

## Phase D — Settings, EmuFolders, BIOS

Goal: configure the core via a base `INISettingsInterface` and point it at the BIOS/resources/memcard dirs.

### Task D1: Core-option table + RetroPad mapping (pure logic, unit-tested)

**Files:**
- Create: `duckstation-libretro/src/duckstation-libretro/libretro_core_options.h`
- Create: `duckstation-libretro/src/duckstation-libretro/libretro_settings.h` / `.cpp`
- Create: `duckstation-libretro/src/duckstation-libretro/libretro_settings_test.cpp` (a tiny standalone test main)

- [ ] **Step 1: Write the failing test for RetroPad→DigitalController bind mapping**

`MapRetroPad` returns the `DigitalController::Button` index for a libretro `RETRO_DEVICE_ID_JOYPAD_*` id (mapping verbatim from `digital_controller.h:13-32`).

```cpp
// libretro_settings_test.cpp
#include "libretro_settings.h"
#include "libretro.h"
#include <cassert>
#include <cstdio>
int main() {
  // RetroPad B (bottom face) → Cross(14); A (right face) → Circle(13)
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_B) == 14);
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_A) == 13);
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_Y) == 15);  // Square
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_X) == 12);  // Triangle
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_START) == 3);
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_SELECT) == 0);
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_UP) == 4);
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_L) == 10);  // L1
  assert(MapRetroPadToDigital(RETRO_DEVICE_ID_JOYPAD_R2) == 9);
  printf("OK\n"); return 0;
}
```

- [ ] **Step 2: Run it, verify it fails to compile/link (function missing)**

```bash
cd "$DS/src/duckstation-libretro"
clang++ -std=c++20 -I. libretro_settings_test.cpp libretro_settings.cpp -o /tmp/lst 2>&1 | tail -5
```

Expected: FAIL — `MapRetroPadToDigital` undefined (or libretro_settings.cpp not yet created).

- [ ] **Step 3: Implement `MapRetroPadToDigital`**

In `libretro_settings.cpp` (declared in `.h`):

```cpp
#include "libretro_settings.h"
#include "libretro.h"
int MapRetroPadToDigital(unsigned retro_id) {
  switch (retro_id) {
    case RETRO_DEVICE_ID_JOYPAD_SELECT: return 0;   // Select
    case RETRO_DEVICE_ID_JOYPAD_L3:     return 1;   // L3
    case RETRO_DEVICE_ID_JOYPAD_R3:     return 2;   // R3
    case RETRO_DEVICE_ID_JOYPAD_START:  return 3;   // Start
    case RETRO_DEVICE_ID_JOYPAD_UP:     return 4;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return 5;
    case RETRO_DEVICE_ID_JOYPAD_DOWN:   return 6;
    case RETRO_DEVICE_ID_JOYPAD_LEFT:   return 7;
    case RETRO_DEVICE_ID_JOYPAD_L2:     return 8;
    case RETRO_DEVICE_ID_JOYPAD_R2:     return 9;
    case RETRO_DEVICE_ID_JOYPAD_L:      return 10;  // L1
    case RETRO_DEVICE_ID_JOYPAD_R:      return 11;  // R1
    case RETRO_DEVICE_ID_JOYPAD_X:      return 12;  // Triangle
    case RETRO_DEVICE_ID_JOYPAD_A:      return 13;  // Circle
    case RETRO_DEVICE_ID_JOYPAD_B:      return 14;  // Cross
    case RETRO_DEVICE_ID_JOYPAD_Y:      return 15;  // Square
    default: return -1;
  }
}
```

- [ ] **Step 4: Run the test, verify it passes**

```bash
clang++ -std=c++20 -I. libretro_settings_test.cpp libretro_settings.cpp -o /tmp/lst && /tmp/lst
```

Expected: prints `OK`.

- [ ] **Step 5: Write the minimal core-options table**

In `libretro_core_options.h`, define a small `retro_core_option_v2_definition[]` with at most: `duckstation_region` (Auto/NTSC-U/NTSC-J/PAL), `duckstation_renderer` (Auto/Software), `duckstation_resolution_scale` (1–4). Keep it tiny — full schema is deferred.

- [ ] **Step 6: Commit**

```bash
cd "$DS"
git add src/duckstation-libretro/libretro_core_options.h src/duckstation-libretro/libretro_settings.{h,cpp} src/duckstation-libretro/libretro_settings_test.cpp
git commit -m "feat(libretro): core-option table + RetroPad→DigitalController mapping (tested)"
```

### Task D2: Build the base settings layer + EmuFolders at load

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro_settings.cpp` (add `ApplySettings`)

- [ ] **Step 1: Implement `ApplySettings(retro_environment_t)`**

Recipe (verbatim API from extraction): empty-path base layer = pure defaults, no disk I/O.

```cpp
#include "core/core.h"
#include "core/system.h"      // for System::LoadSettings via Host? actually internal
#include "core/settings.h"    // EmuFolders, g_settings, MemoryCardType
#include "common/error.h"

void ApplySettings(retro_environment_t environ_cb, std::string_view system_dir) {
  Error error;
  Core::InitializeBaseSettingsLayer(std::string(), &error);   // "" → defaults, no file

  // Resolve resources + BIOS dir relative to the dylib / system dir.
  EmuFolders::Resources = /* <dir-of-dylib>/duckstation_libretro_resources via dladdr */;
  EmuFolders::Bios = std::string(system_dir);                 // libretro system dir holds the BIOS
  EmuFolders::MemoryCards = /* libretro save dir or a temp dir */;
  EmuFolders::EnsureFoldersExist();

  {
    auto lock = Core::GetSettingsLock();
    SettingsInterface* si = Core::GetBaseSettingsLayer();
    // Read libretro options and map → settings keys. Renderer/region/etc.
    // e.g. si->SetStringValue("GPU", "Renderer", "Software");   // skeleton-safe
    si->SetStringValue("Console", "Region", "Auto");
    si->SetStringValue("MemoryCards", "Card1Type", "NonPersistent");  // delta §6: Libretro type gone
    si->SetBoolValue("GPU", "UseThread", false);                       // inline run loop
    EmuFolders::LoadConfig(*si);
  }
  // Settings get applied during boot (System::Internal load). If a pre-boot
  // apply is needed, the internal System::LoadSettings(false) runs at boot.
}
```

Resolve the dladdr-based resources path the way PCSX2's `CoreResources::ResolveResourcesDir` does (copy that helper). Confirm the exact setting section/key names by grepping `settings.cpp` for `GetStringValue("GPU", "Renderer"` etc. — the keys must match what `Settings::Load` reads.

- [ ] **Step 2: Verify setting keys against the loader**

```bash
cd "$DS"
grep -n 'GetStringValue("GPU"\|GetStringValue("Console"\|"Region"\|"Renderer"\|Card1Type\|"UseThread"' src/core/settings.cpp | head
```

Expected: confirms the exact `(section,key)` strings. Fix the `Set*Value` calls to match.

- [ ] **Step 3: Commit**

```bash
git add src/duckstation-libretro/libretro_settings.cpp
git commit -m "feat(libretro): base settings layer + EmuFolders wiring"
```

---

## Phase E — Boot + run loop

Goal: `retro_load_game` boots the system; `retro_run` advances exactly one frame.

### Task E1: Implement boot in `retro_load_game`

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro.cpp` (replace the stub bodies; remove the stub Host include collisions)

- [ ] **Step 1: Wire init + boot**

```cpp
// retro_init: stash the core thread handle, init logging
RETRO_API void retro_init(void) {
  LibretroHost::SetCoreThread(Threading::ThreadHandle::GetForCallingThread());
}

RETRO_API bool retro_load_game(const retro_game_info* game) {
  if (!game || !game->path) return false;

  const char* system_dir = nullptr;
  g_environ(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir);
  ApplySettings(g_environ, system_dir ? system_dir : "");

  SystemBootParameters params(std::string(game->path));   // ctor takes the disc path
  // params.force_software_renderer = true;  // (Task F0 uses this for first bring-up)

  Error error;
  if (!System::BootSystem(std::move(params), &error)) {
    // log error.GetDescription()
    return false;
  }
  return true;
}
```

`SystemBootParameters{path}` sets `params.path` (the disc/ROM). `BootSystem` loads the BIOS via `BIOS::GetBIOSImage` from `EmuFolders::Bios` and creates the GPU device (which calls `Host::AcquireRenderWindow`, Task F1).

- [ ] **Step 2: Build + link**

```bash
cd "$DS"
cmake --build build-arm64 --target duckstation_libretro 2>&1 | tail -30
```

Expected: links (given Phases C/F provide the Host functions). Resolve any remaining undefined symbols.

- [ ] **Step 3: Smoke — boot reaches BootSystem (with a BIOS present)**

Place a real PS1 BIOS in the libretro `system` dir. Launch a game through RetroNest. With a temporary log line after `BootSystem`, expected: `BootSystem` returns true (or logs a specific BIOS/disc error you can act on). Video won't show until Phase F; this validates settings+BIOS+boot only.

- [ ] **Step 4: Commit**

```bash
git add src/duckstation-libretro/libretro.cpp
git commit -m "feat(libretro): retro_load_game boots the system"
```

### Task E2: One-frame run loop

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro.cpp` (`retro_run`)
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro_host.cpp` (`PumpMessagesOnCoreThread`, shutdown flag)

- [ ] **Step 1: Implement single-frame stepping**

In the inline model, force non-threaded video (done via `UseThread=false` in settings) and interrupt `Execute()` at the frame boundary. `Host::PumpMessagesOnCoreThread` is called by the core once per guest vsync (inside `FrameDone`) — use it to request execution exit after one frame:

```cpp
// libretro_host.cpp
namespace { std::atomic<bool> s_frame_boundary{false}; }
void Host::PumpMessagesOnCoreThread() {
  // Called at end-of-frame on the core thread. Signal the driver to stop after this frame.
  s_frame_boundary.store(true, std::memory_order_release);
  System::InterruptExecution();   // makes the current System::Execute() return
}
namespace LibretroHost { bool TakeFrameBoundary() { return s_frame_boundary.exchange(false); } }
```

```cpp
// libretro.cpp
RETRO_API void retro_run(void) {
  if (!System::IsValid()) return;
  g_input_poll();
  UpdateControllers();                 // Task H
  System::Execute();                   // runs until InterruptExecution() fires at FrameDone
  // Present already happened inline (MetalDevice → CAMetalLayer).
  if (g_video_refresh) g_video_refresh(RETRO_HW_FRAME_BUFFER_VALID, g_fb_w, g_fb_h, 0); // heartbeat (no-op for MetalNSView)
  DrainAudio();                        // Task G
}
```

Confirm that `System::Execute()` returns after a single `InterruptExecution()` rather than looping; if it re-enters, gate re-entry on `TakeFrameBoundary()`. (This is flagged risk #2 — validate here.)

- [ ] **Step 2: Wire the shutdown flag**

```cpp
// libretro_host.cpp
namespace { std::atomic<bool> s_shutdown_requested{false}; }
void Host::RequestSystemShutdown(bool, bool, bool) { s_shutdown_requested.store(true); }
```

In `retro_run`, if `s_shutdown_requested`, call `g_environ(RETRO_ENVIRONMENT_SHUTDOWN, nullptr)` so RetroNest unloads cleanly.

- [ ] **Step 3: Build**

```bash
cmake --build build-arm64 --target duckstation_libretro 2>&1 | tail -20
```

Expected: links. (Render still validated in Phase F.)

- [ ] **Step 4: Commit**

```bash
git add src/duckstation-libretro/libretro.cpp src/duckstation-libretro/libretro_host.cpp
git commit -m "feat(libretro): single-frame inline run loop"
```

---

## Phase F — Renderer / present (load-bearing risk)

Goal: DuckStation's `MetalDevice` builds its layer on RetroNest's NSView and presents the emulated frame on screen. **Validate the software renderer path first to decouple boot/run-loop from Metal.**

### Task F1: Implement `Host::AcquireRenderWindow` (NSView → WindowInfo)

**Files:**
- Create: `duckstation-libretro/src/duckstation-libretro/libretro_window.mm` (Objective-C++ for NSView metrics)
- Modify: `libretro_host.cpp` (or put the Host function in the .mm), `CMakeLists.txt`

- [ ] **Step 1: Implement the render-window acquisition**

Mirror PCSX2's `HostStubs.cpp:267-312`. Confirm the exact `Host::AcquireRenderWindow` signature in `core/system_private.h` / `core/host.h` (it returns `std::optional<WindowInfo>`):

```objc++
// libretro_window.mm
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "core/host.h"
#include "util/window_info.h"

static constexpr unsigned RETRONEST_GET_NSVIEW = (1u | 0x20000); // RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW

std::optional<WindowInfo> Host::AcquireRenderWindow(RenderAPI api, bool fullscreen, bool exclusive, Error* error) {
  void* ns_view = nullptr;
  if (!g_environ(RETRONEST_GET_NSVIEW, &ns_view) || !ns_view) { /* log */ return std::nullopt; }
  NSView* view = (__bridge NSView*)ns_view;
  WindowInfo wi{};
  wi.type = WindowInfoType::MacOS;
  wi.window_handle = ns_view;
  const CGSize sz = view.bounds.size;
  const CGFloat scale = view.window ? view.window.backingScaleFactor : 1.0;
  wi.surface_width  = (u16)(sz.width * scale);
  wi.surface_height = (u16)(sz.height * scale);
  wi.surface_scale  = (float)scale;
  wi.surface_refresh_rate = 0.0f;
  return wi;
}
void Host::ReleaseRenderWindow() {}
```

`g_environ` must be visible here (declare `extern retro_environment_t g_environ;` in a shared internal header). Confirm the precise `AcquireRenderWindow` parameter list against the header — it may differ slightly (e.g. include a `std::string_view adapter`).

- [ ] **Step 2: Add the .mm to CMake**

```cmake
target_sources(duckstation_libretro PRIVATE libretro_window.mm)
set_source_files_properties(libretro_window.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
# link frameworks if not already: "-framework AppKit" "-framework QuartzCore" "-framework Metal"
```

- [ ] **Step 3: Implement `Host::FrameDoneOnVideoThread`**

In inline mode this runs on the core thread and is where the frame is submitted/presented. For the skeleton, forward to the standard presenter (the same call DuckStation's own video thread makes — find it in `core/video_thread.cpp`'s frame-done handling and mirror it):

```cpp
void Host::FrameDoneOnVideoThread(GPUBackend* gpu_backend, u32 frame_number) {
  // Mirror VideoThread's present: VideoPresenter::PresentFrame(gpu_backend, frame_number)
  // (confirm the exact call DuckStation uses internally)
}
```

Read `core/video_thread.cpp` around its `FrameDoneOnVideoThread` / present site and replicate the minimal present call. Capture the framebuffer dims into `g_fb_w/g_fb_h` here for the AV-info/heartbeat.

- [ ] **Step 4: Build + commit**

```bash
cmake --build build-arm64 --target duckstation_libretro 2>&1 | tail -20
git add src/duckstation-libretro/libretro_window.mm src/duckstation-libretro/libretro_host.cpp src/duckstation-libretro/CMakeLists.txt
git commit -m "feat(libretro): AcquireRenderWindow (NSView) + frame present"
```

### Task F2: Smoke — frame on screen

- [ ] **Step 1: Build universal + place beside resources**

```bash
"$DS/src/duckstation-libretro/build-universal.sh"
# copy duckstation_libretro.dylib + duckstation_libretro_resources/ into RetroNest's libretro cores dir
```

- [ ] **Step 2: Launch through RetroNest**

Launch a PS1 game. Expected (the Phase 1 video done-criterion): the game renders in the RetroNest emulation view via the Metal layer. If black/blank:
- Confirm `AcquireRenderWindow` returned a valid NSView (log it).
- Confirm `MetalDevice::CreateSwapChain` ran (`setLayer:` on the view) — add a temporary log.
- Try `params.force_software_renderer = true` to isolate Metal-renderer vs present-path issues.
- Fallback (flagged risk #1): if `MetalDevice`'s direct-to-NSView present conflicts with RetroNest compositing, switch to `VideoPresenter::RenderDisplay(offscreen GPUTexture)` + hand the `MTLTexture` (`MetalTexture::GetMTLTexture()`) to RetroNest. This is a larger change — document and escalate before taking it.

- [ ] **Step 3: Commit any fixes**

```bash
git add -A src/duckstation-libretro/
git commit -m "fix(libretro): renderer present validated through RetroNest"
```

---

## Phase G — Audio

Goal: PS1 audio plays through RetroNest via `retro_audio_sample_batch`. This requires a localized core-touch (DuckStation audio has no frontend drain hook).

### Task G1: Capturing audio stream + SPU routing

**Files:**
- Create: `duckstation-libretro/src/duckstation-libretro/libretro_audio.cpp` / `.h`
- Modify: `duckstation-libretro/src/core/spu.cpp` (`CreateOutputStream`)

- [ ] **Step 1: Write the frames-per-host-frame test (pure logic)**

```cpp
// in libretro_settings_test.cpp (extend it)
assert(FramesPerHostFrame(44100, 59.94) == 736);   // round(44100/59.94)
assert(FramesPerHostFrame(44100, 50.0)  == 882);
```

Run it (same clang++ invocation as Task D1), confirm FAIL then implement `FramesPerHostFrame(sample_rate, fps) = lround(sample_rate / fps)`, confirm PASS.

- [ ] **Step 2: Implement a capturing `AudioStream`**

Subclass DuckStation's `AudioStream` (`util/audio_stream.h`) — or, simpler, a class the SPU writes into — that buffers `s16` stereo frames instead of playing them. Model on PCSX2's `LibretroAudioStream` (ring buffer + `DrainToLibretroCallback`). Confirm the exact `AudioStream` interface to implement (the SPU pushes via `BeginWrite`/`EndWrite` on `CoreAudioStream`; the cleanest tap is a custom backend the `CoreAudioStream` pulls from, or replacing `CoreAudioStream`'s backend).

```cpp
// libretro_audio.h
class LibretroAudioStream {
public:
  void Push(const s16* frames, u32 num_frames);          // called by SPU path
  void DrainTo(retro_audio_sample_batch_t cb, u32 max_frames);
  static LibretroAudioStream* Active();
};
```

- [ ] **Step 3: Route the SPU output to it**

In `spu.cpp`'s `CreateOutputStream`, when libretro mode is active (a global flag set by the frontend), construct/return the capturing stream instead of the Cubeb/SDL backend. Keep the change minimal and `#ifndef`-free — guard on a runtime flag so the normal DuckStation app is unaffected. Set `g_settings.audio_backend` accordingly (or bypass it for the libretro flag).

- [ ] **Step 4: Drain in `retro_run`**

```cpp
void DrainAudio() {
  if (auto* s = LibretroAudioStream::Active(); s && g_audio_batch)
    s->DrainTo(g_audio_batch, FramesPerHostFrame(44100, g_fps));
}
```

- [ ] **Step 5: Build + smoke (audio)**

Build universal, launch through RetroNest. Expected: game audio plays without heavy crackle/drift. Tune `FramesPerHostFrame` source values (use `SPU::SAMPLE_RATE`=44100 and the detected `g_fps`).

- [ ] **Step 6: Commit**

```bash
git add src/duckstation-libretro/libretro_audio.{h,cpp} src/core/spu.cpp
git commit -m "feat(libretro): capturing audio stream + SPU routing → retro_audio_sample_batch"
```

---

## Phase H — Input

Goal: a digital pad on port 0 controls the game.

### Task H1: Attach controller + push state

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro.cpp`

- [ ] **Step 1: Attach a DigitalController at boot**

After `BootSystem` succeeds (or on `retro_set_controller_port_device`):

```cpp
Pad::SetController(0, Controller::Create(ControllerType::DigitalController, 0));
```

- [ ] **Step 2: Implement `UpdateControllers`**

```cpp
void UpdateControllers() {
  Controller* c = System::GetController(0);
  if (!c) return;
  for (unsigned id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id) {
    const int bind = MapRetroPadToDigital(id);
    if (bind < 0) continue;
    const int16_t pressed = g_input_state(0, RETRO_DEVICE_JOYPAD, 0, id);
    c->SetBindState(static_cast<u32>(bind), pressed ? 1.0f : 0.0f);
  }
}
```

- [ ] **Step 3: Build + smoke (input)**

Launch through RetroNest; confirm the d-pad and face buttons control the game (navigate a menu / move in-game).

- [ ] **Step 4: Commit**

```bash
git add src/duckstation-libretro/libretro.cpp
git commit -m "feat(libretro): digital pad on port 0 via SetBindState"
```

---

## Phase I — End-to-end + cleanup

### Task I1: Full done-criteria pass

- [ ] **Step 1: Clean run through RetroNest**

Launch a PS1 game and verify all five done-criteria in one session:
1. Boots through BIOS to the game.
2. Video renders via the Metal layer.
3. Audio plays.
4. Digital pad controls the game.
5. Exit/unload is clean — quit from RetroNest, no crash on view teardown (RetroNest's `LibretroMetalItem` dtor defers QWindow teardown; confirm no abort in logs).

- [ ] **Step 2: `retro_unload_game` / `retro_deinit` cleanup**

Confirm `retro_unload_game` calls `System::ShutdownSystem(false)` and tears down the GPU device; `retro_deinit` resets globals. Verify no leaked Metal layer / dangling NSView (no crash on relaunch within the same RetroNest session).

- [ ] **Step 3: Commit**

```bash
git add -A src/duckstation-libretro/
git commit -m "feat(libretro): end-to-end skeleton boot through RetroNest"
```

### Task I2: Update docs, retire the SwanStation reference clone

- [ ] **Step 1: Note completion in the spec + delta companion**

Add a "Phase 1 complete" note (date, commit) to the spec doc and the delta report.

- [ ] **Step 2: Remove the SwanStation reference clone** (per spec: delete after implementation)

```bash
rm -rf /Users/mark/Documents/Projects/swanstation
```

- [ ] **Step 3: Commit the doc updates (RetroNest repo)**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add docs/superpowers/specs/2026-06-01-duckstation-libretro-skeleton-design.md
git commit -m "docs: mark DuckStation libretro Phase 1 skeleton complete"
```

---

## Flagged risks (re-verify during execution)

1. **Renderer present (Task F2).** Direct `MetalDevice`→NSView present must coexist with RetroNest's compositing. Fallback: offscreen `VideoPresenter::RenderDisplay` + `MTLTexture` handoff. Validate software-renderer boot first to isolate.
2. **One-frame stepping (Task E2).** `System::Execute()` + `InterruptExecution()` at `FrameDone` must yield exactly one frame per `retro_run` without throttle fighting the host. Fallback: threaded `VideoThread` + present-CV handshake (PCSX2's model).
3. **Audio core-touch (Task G1).** No clean frontend hook; the SPU-routing change must not regress the normal DuckStation app (guard on a runtime libretro flag).
4. **Metal resources (Task B2).** If `MetalDevice` needs a prebuilt `.metallib` (rather than runtime `newLibraryWithSource`), add metallib compilation to `build-universal.sh`.

## Out of scope (deferred specs)
Settings-schema migration; RetroAchievements (`raConsoleId`/memory maps); analog/multitap/rumble controllers; save states (needs a small `System::LoadStateDataFromBuffer` core addition — delta §5); hotkeys; resume-on-launch; the public/SwanStation RetroNest variant.

---

## OUTCOME — Phase 1 COMPLETE (2026-06-03)

All phases executed and verified working through RetroNest (boot, video, sound, input, clean exit/re-load), packaged universal via `package.sh`. The detailed outcome, the **deviations from this plan** (fork core-touches: `System::RunFrame`, the `Core::ProcessStartup`/`CoreThreadInitialize` startup bootstrap, the `CoreAudioStream` capture mode, the Cross/Circle swap), the corrected Host contract, settings injection, RetroNest-side changes, the runtime deps + deploy script, and what's deferred are recorded in the spec's **"Implementation Outcome"** section:
`docs/superpowers/specs/2026-06-01-duckstation-libretro-skeleton-design.md`.

Notable plan-vs-reality corrections worth carrying forward:
- The plan assumed minimal/no core changes; **four intentional fork core-touches** were required (see spec outcome).
- Startup bootstrap was under-specified — `Core::ProcessStartup` + `Core::CoreThreadInitialize` are mandatory before boot (else segfaults), matching `duckstation-regtest`.
- Input required **`controllerTypes()` + `controllerBindingDefsForType()`** on the RetroNest adapter — the "minimal" skeleton adapter was insufficient; with no binding defs, RetroNest's InputRouter routes nothing.
- Metal needs a precompiled `metal_shaders.metallib` (not runtime shader compilation), and ImGui needs `libshaderc`/`libspirv-cross` at runtime — all handled by `package.sh`.
