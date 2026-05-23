# SP1: Dolphin Libretro Core Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a minimal `dolphin_libretro.dylib` that exports the full `retro_*` C ABI as stubs, links against Dolphin's `core` + `uicommon` static libraries, and can be `dlopen`'d cleanly. No emulation, no rendering — pure plumbing.

**Architecture:** New `Source/Core/DolphinLibretro/` target sibling to `DolphinQt` and `DolphinNoGUI`, gated on `-DENABLE_LIBRETRO=ON`. Produces a `MODULE` library (loadable plugin) named `dolphin_libretro.dylib` (no `lib` prefix). All `retro_*` entrypoints return sane no-op values; `retro_load_game` returns `false`. All 19 `Host_*` functions Dolphin's core/uicommon require are stubbed (modeled on `DolphinNoGUI/MainNoGUI.cpp`). A standalone `dlopen` smoke test verifies the dylib loads, `retro_api_version` matches `RETRO_API_VERSION`, and `retro_load_game(nullptr)` returns `false` without crashing.

**Tech Stack:** CMake 3.20+, Dolphin's static libraries (`core`, `uicommon` and their transitive deps — `common`, `videocommon`, `audiocommon`, `inputcommon`, `discio`, `videobackends`), libretro C ABI, C++20, Objective-C++ on macOS.

**Parent spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-23-dolphin-libretro-conversion-design.md`

**Predecessor:** SP0 complete — see `RetroNest-Project/docs/superpowers/notes/2026-05-23-moltenvk-metal-interop-spike.md` for Vulkan path decision (does not affect SP1; SP1 is Metal-only stubs).

**Working directory:** `/Users/mark/Documents/Projects/dolphin-libretro/` (the Dolphin source tree). This sub-project does NOT touch `RetroNest-Project/`.

**Git workflow:** Commit directly to whatever branch this tree is on (the user's preferred workflow per SP0). The tree is NOT a git repo at the start of SP1 — initialize one in Task 1.

---

## File structure

| File | Responsibility |
|---|---|
| `Source/Core/DolphinLibretro/CMakeLists.txt` | Module library target, links `core` + `uicommon`, hides non-`retro_*` symbols, gates the build on `ENABLE_LIBRETRO` (handled at parent CMakeLists). |
| `Source/Core/DolphinLibretro/libretro.h` | Copied verbatim from PPSSPP-libretro. Standard libretro C ABI header. |
| `Source/Core/DolphinLibretro/LibretroFrontend.cpp` | All `retro_*` C ABI entrypoints as skeleton stubs. Stores callback function pointers; `retro_load_game` returns false. |
| `Source/Core/DolphinLibretro/HostStubs.cpp` | All 19 `Host_*` functions Dolphin's static libs require. No-ops or sensible defaults. Modeled on `DolphinNoGUI/MainNoGUI.cpp`. |
| `Source/Core/DolphinLibretro/EmuThread.h` | Class declaration for `LibretroEmuThread` — owns pause flag + per-frame signal. SP1 stub: no `BootManager` integration. |
| `Source/Core/DolphinLibretro/EmuThread.cpp` | Class definition. Empty `Start()`, `Stop()`, `RunFrame()` methods. |
| `Source/Core/DolphinLibretro/tools/test_load.cpp` | Standalone `dlopen` test. Not linked against Dolphin. Builds as a separate executable that exercises the dylib's ABI. |
| `Source/Core/CMakeLists.txt` | Modified — add `if(ENABLE_LIBRETRO) add_subdirectory(DolphinLibretro) endif()` block. |
| `CMakeLists.txt` (top-level) | Modified — add `option(ENABLE_LIBRETRO "Enable libretro core" OFF)`. |
| `.gitignore` | Created at tree root if not present. Excludes build dirs (`build*/`), CMake cache, DS_Store. |

---

### Task 1: Initialize git + add ENABLE_LIBRETRO option

**Files:**
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/.gitignore`
- Modify: `/Users/mark/Documents/Projects/dolphin-libretro/CMakeLists.txt` (one line added near line 86, next to existing `ENABLE_NOGUI` / `ENABLE_QT` options)
- Modify: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/CMakeLists.txt` (one block added next to existing `ENABLE_NOGUI` block at line 10)

The Dolphin tree at `/Users/mark/Documents/Projects/dolphin-libretro/` is NOT yet a git repo. Initialize it first so subsequent tasks can commit incrementally.

- [ ] **Step 1: Initialize git in the Dolphin tree**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git init -b main
git status --short | head -20
```

Expected: a fresh git repo on branch `main` with a huge number of untracked files (the whole Dolphin source tree). Don't commit anything yet.

- [ ] **Step 2: Create .gitignore at the tree root**

Create `/Users/mark/Documents/Projects/dolphin-libretro/.gitignore`:

```gitignore
# Build outputs
build*/
cmake-build*/
out/
*.o
*.a
*.so
*.dylib
*.dll

# CMake-generated
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
CTestTestfile.cmake
Testing/
compile_commands.json

# IDE
.idea/
.vscode/
*.swp
*.swo
.DS_Store

# Local config
CMakeUserPresets.json
```

- [ ] **Step 3: Initial commit (Dolphin tree as-is)**

Stage everything (including the .gitignore) and make a baseline commit so subsequent SP1 commits show only our changes:

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add .gitignore
git add Source/ CMake/ CMakeLists.txt CMakeSettings.json Readme.md Contributing.md COPYING \
        LICENSES/ Data/ Externals/ Languages/ Tools/ docs/ \
        BuildMacOSUniversalBinary.py AndroidSetup.md CODE_OF_CONDUCT.md \
        Flatpak/ Installer/
git status --short | head -5
```

Note: this is the controller's commit, not the subagent's. The subagent reports DONE_AND_READY_TO_COMMIT after staging; the controller verifies the staged set is reasonable and commits via:
```bash
git commit -m "Initial commit: Dolphin source tree (pre-libretro)"
```

- [ ] **Step 4: Add ENABLE_LIBRETRO option to top-level CMakeLists.txt**

In `/Users/mark/Documents/Projects/dolphin-libretro/CMakeLists.txt`, find line 85-86 which read:
```cmake
option(ENABLE_NOGUI "Enable NoGUI frontend" ON)
option(ENABLE_QT "Enable Qt (Default)" ON)
```

Add a third option immediately after line 86:
```cmake
option(ENABLE_LIBRETRO "Build the libretro core (dolphin_libretro.dylib)" OFF)
```

The default is OFF so anyone building stock Dolphin (DolphinQt) doesn't get a libretro target by accident.

- [ ] **Step 5: Add add_subdirectory in Source/Core/CMakeLists.txt**

In `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/CMakeLists.txt`, find line 10-12 which read:
```cmake
if(ENABLE_NOGUI)
  add_subdirectory(DolphinNoGUI)
endif()
```

Add a parallel block immediately after line 12:
```cmake
if(ENABLE_LIBRETRO)
  add_subdirectory(DolphinLibretro)
endif()
```

- [ ] **Step 6: Verify cmake config still succeeds (target dir does not yet exist — gate stays OFF)**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake -B build-libretro -S . -G Ninja \
    -DENABLE_LIBRETRO=OFF -DENABLE_QT=OFF -DENABLE_NOGUI=OFF \
    2>&1 | tail -20
```

Expected: cmake configures successfully (no errors). The `ENABLE_LIBRETRO=OFF` keeps our new subdirectory disabled, so cmake won't complain about the missing `DolphinLibretro/CMakeLists.txt`. We disable QT and NOGUI too to avoid pulling in Qt/X11 dependencies during the skeleton phase.

If cmake fails with errors related to required system libraries (Qt, FFmpeg, etc.) — these are pre-existing in the Dolphin tree, not caused by our changes. If unsure, run the same cmake invocation WITHOUT our changes to confirm — that's the baseline. Our changes are 2 lines added, 0 removed.

- [ ] **Step 7: Stage only (controller commits)**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add CMakeLists.txt Source/Core/CMakeLists.txt
git status --short
```

Report. Controller will commit with message:
```
SP1: add ENABLE_LIBRETRO option and DolphinLibretro subdirectory gate

Wires the new Source/Core/DolphinLibretro/ target into Dolphin's CMake
graph behind an OFF-by-default gate. No new directory created yet — that
comes in Task 2 alongside the actual sources.
```

---

### Task 2: Source files (HostStubs, EmuThread, LibretroFrontend, libretro.h)

**Files:**
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/libretro.h`
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/HostStubs.cpp`
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/EmuThread.h`
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/EmuThread.cpp`
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/LibretroFrontend.cpp`

All five files in one task. The CMakeLists in Task 3 turns them into a build target.

- [ ] **Step 1: Create the DolphinLibretro directory**

Run:
```bash
mkdir -p /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools
```

- [ ] **Step 2: Copy libretro.h from PPSSPP-libretro**

The PPSSPP-libretro tree at `/Users/mark/Documents/Projects/ppsspp-libretro/libretro/libretro.h` is a known-working version. Use that exact file.

Run:
```bash
cp /Users/mark/Documents/Projects/ppsspp-libretro/libretro/libretro.h \
   /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/libretro.h
head -5 /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/libretro.h
wc -l /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/libretro.h
```

Expected: file copied, > 1500 lines (libretro.h is large). Header guards `LIBRETRO_H_` or similar at top.

- [ ] **Step 3: Write HostStubs.cpp**

Create `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/HostStubs.cpp`:

```cpp
// SP1: Host_* function stubs for the libretro core.
//
// Dolphin's core/uicommon static libraries reference 19 Host_* functions
// that the frontend (DolphinQt or DolphinNoGUI) is expected to provide.
// The libretro core has no UI to delegate to, so each is a no-op or a
// sensible default. Modeled on Source/Core/DolphinNoGUI/MainNoGUI.cpp's
// implementations of the same symbols.
//
// When SP2 wires up actual emulation, some of these will gain behavior
// (e.g. Host_Message handling WMUserStop to signal the EmuThread).

#include <string>
#include <vector>
#include <memory>

#include "Core/Host.h"

namespace HW::GBA { class Core; }
class GBAHostInterface;

std::vector<std::string> Host_GetPreferredLocales()
{
    return {};
}

void Host_PPCSymbolsChanged()
{
}

void Host_PPCBreakpointsChanged()
{
}

bool Host_UIBlocksControllerState()
{
    return false;
}

void Host_Message(HostMessageID id)
{
    // SP2 wires WMUserStop to signal EmuThread shutdown.
    (void)id;
}

void Host_UpdateTitle(const std::string& title)
{
    (void)title;
}

void Host_UpdateDisasmDialog()
{
}

void Host_JitCacheInvalidation()
{
}

void Host_JitProfileDataWiped()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
    (void)width;
    (void)height;
}

bool Host_RendererHasFocus()
{
    // Libretro frontend is always considered focused from Dolphin's POV;
    // pause-on-focus-loss is handled by RetroNest separately.
    return true;
}

bool Host_RendererHasFullFocus()
{
    return true;
}

bool Host_RendererIsFullscreen()
{
    // The host (RetroNest) owns the window; Dolphin shouldn't toggle.
    return false;
}

bool Host_TASInputHasFocus()
{
    return false;
}

void Host_YieldToUI()
{
}

void Host_TitleChanged()
{
}

void Host_UpdateDiscordClientID(const std::string& client_id)
{
    (void)client_id;
}

bool Host_UpdateDiscordPresenceRaw(const std::string& details, const std::string& state,
                                   const std::string& large_image_key,
                                   const std::string& large_image_text,
                                   const std::string& small_image_key,
                                   const std::string& small_image_text,
                                   const int64_t start_timestamp, const int64_t end_timestamp,
                                   const int party_size, const int party_max)
{
    (void)details; (void)state;
    (void)large_image_key; (void)large_image_text;
    (void)small_image_key; (void)small_image_text;
    (void)start_timestamp; (void)end_timestamp;
    (void)party_size; (void)party_max;
    return false;
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
    (void)core;
    return nullptr;
}
```

- [ ] **Step 4: Write EmuThread.h**

Create `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/EmuThread.h`:

```cpp
// SP1: Skeleton EmuThread coordinator.
//
// Owns the pause flag and per-frame signaling primitive that retro_run
// will use to gate Dolphin's CPU/Fifo threads. SP1 keeps it minimal — no
// BootManager integration yet — so the build can prove it links. SP2
// adds the real Start()/Stop() wiring around BootManager::BootCore.

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace DolphinLibretro {

class EmuThread
{
public:
    EmuThread();
    ~EmuThread();

    EmuThread(const EmuThread&) = delete;
    EmuThread& operator=(const EmuThread&) = delete;

    // SP1: stub. SP2 spins up BootManager::BootCore + Dolphin's CPU/Fifo threads.
    void Start();

    // SP1: stub. SP2 calls Core::Stop + BootManager::Stop.
    void Stop();

    // SP1: stub. SP2 signals the CPU thread to advance one frame and waits
    // for the video output before returning.
    void RunFrame();

    void SetPaused(bool paused);
    bool IsPaused() const { return m_paused.load(); }

private:
    std::atomic<bool> m_paused{false};
    std::mutex m_frame_mutex;
    std::condition_variable m_frame_cv;
    bool m_frame_ready{false};
};

}  // namespace DolphinLibretro
```

- [ ] **Step 5: Write EmuThread.cpp**

Create `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/EmuThread.cpp`:

```cpp
// SP1: Skeleton EmuThread implementation. See EmuThread.h for SP2 plans.

#include "DolphinLibretro/EmuThread.h"

namespace DolphinLibretro {

EmuThread::EmuThread() = default;
EmuThread::~EmuThread() = default;

void EmuThread::Start()
{
    // SP2: BootManager::BootCore + spin up Dolphin's CPU/Fifo threads.
}

void EmuThread::Stop()
{
    // SP2: Core::Stop + BootManager::Stop + join threads.
}

void EmuThread::RunFrame()
{
    // SP2: signal the CPU thread to advance one frame, wait for video, return.
}

void EmuThread::SetPaused(bool paused)
{
    m_paused.store(paused);
}

}  // namespace DolphinLibretro
```

- [ ] **Step 6: Write LibretroFrontend.cpp**

Create `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/LibretroFrontend.cpp`:

```cpp
// SP1: retro_* C ABI entrypoints — skeleton only.
//
// All entrypoints return sane no-op values. Callback function pointers are
// stored but not yet used. retro_load_game returns false ("no game loaded").
// SP2 wires up actual emulation behind these symbols.
//
// libretro requires C linkage; visibility is exported per-function by the
// RETRO_API macro in libretro.h. The CMakeLists hides everything else.

#include "DolphinLibretro/libretro.h"
#include "DolphinLibretro/EmuThread.h"

#include <memory>

namespace {

// Stored callbacks supplied by the libretro frontend.
retro_environment_t        s_environ_cb         = nullptr;
retro_video_refresh_t      s_video_refresh_cb   = nullptr;
retro_audio_sample_t       s_audio_sample_cb    = nullptr;
retro_audio_sample_batch_t s_audio_batch_cb     = nullptr;
retro_input_poll_t         s_input_poll_cb      = nullptr;
retro_input_state_t        s_input_state_cb     = nullptr;

std::unique_ptr<DolphinLibretro::EmuThread> s_emu_thread;

}  // namespace

extern "C" {

RETRO_API unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info* info)
{
    info->library_name     = "Dolphin";
    info->library_version  = "SP1-skeleton";
    info->valid_extensions = "iso|gcm|gcz|ciso|wbfs|rvz|wad|wia|nkit|m3u|dol|elf|tgc";
    info->need_fullpath    = true;
    info->block_extract    = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info)
{
    // Conservative defaults; SP2 reads actual values from Dolphin's video output.
    info->geometry.base_width   = 640;
    info->geometry.base_height  = 480;
    info->geometry.max_width    = 5120;   // 8x EFB scale upper bound
    info->geometry.max_height   = 4096;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    info->timing.fps            = 60.0;
    info->timing.sample_rate    = 32000.0;  // DSP HLE default; SP7 settles
}

RETRO_API void retro_set_environment(retro_environment_t cb)        { s_environ_cb       = cb; }
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)    { s_video_refresh_cb = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)      { s_audio_sample_cb  = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
                                                                    { s_audio_batch_cb   = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb)          { s_input_poll_cb    = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb)        { s_input_state_cb   = cb; }

RETRO_API void retro_init(void)
{
    s_emu_thread = std::make_unique<DolphinLibretro::EmuThread>();
}

RETRO_API void retro_deinit(void)
{
    s_emu_thread.reset();
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
    (void)port;
    (void)device;
}

RETRO_API void retro_reset(void)
{
    // SP2: BootManager::Stop + BootCore the same ROM.
}

RETRO_API void retro_run(void)
{
    // SP2: poll input, advance one frame, drain audio + video.
    if (s_input_poll_cb)
        s_input_poll_cb();
}

RETRO_API size_t retro_serialize_size(void)
{
    return 0;  // SP2: State::GetMaxBufferSize for current STATE_VERSION
}

RETRO_API bool retro_serialize(void* data, size_t size)
{
    (void)data; (void)size;
    return false;
}

RETRO_API bool retro_unserialize(const void* data, size_t size)
{
    (void)data; (void)size;
    return false;
}

RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
    (void)index; (void)enabled; (void)code;
}

RETRO_API bool retro_load_game(const struct retro_game_info* game)
{
    // SP1: skeleton always returns false ("no game loaded").
    // SP2: BootManager::BootCore(BootParameters::GenerateFromFile(game->path)).
    (void)game;
    return false;
}

RETRO_API bool retro_load_game_special(unsigned game_type,
                                       const struct retro_game_info* info,
                                       size_t num_info)
{
    (void)game_type; (void)info; (void)num_info;
    return false;
}

RETRO_API void retro_unload_game(void)
{
    // SP2: Core::Stop + BootManager::Stop + tear down render context.
}

RETRO_API unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

RETRO_API void* retro_get_memory_data(unsigned id)
{
    (void)id;
    return nullptr;
}

RETRO_API size_t retro_get_memory_size(unsigned id)
{
    (void)id;
    return 0;
}

}  // extern "C"
```

- [ ] **Step 7: Sanity check the files exist and roughly the right size**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro
ls -1 && wc -l libretro.h HostStubs.cpp EmuThread.h EmuThread.cpp LibretroFrontend.cpp
```

Expected: 5 files (plus the `tools/` empty subdir). Roughly:
- `libretro.h`: 1500+ lines
- `HostStubs.cpp`: 100-130 lines
- `EmuThread.h`: 30-50 lines
- `EmuThread.cpp`: 25-35 lines
- `LibretroFrontend.cpp`: 130-160 lines

- [ ] **Step 8: Stage only (controller commits)**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/libretro.h \
        Source/Core/DolphinLibretro/HostStubs.cpp \
        Source/Core/DolphinLibretro/EmuThread.h \
        Source/Core/DolphinLibretro/EmuThread.cpp \
        Source/Core/DolphinLibretro/LibretroFrontend.cpp
git status --short
```

Report. Controller commits with message:
```
SP1: skeleton sources for the libretro core

LibretroFrontend.cpp: all retro_* entrypoints, stubbed. retro_load_game
  returns false; callback function pointers are stored.
HostStubs.cpp: 19 Host_* functions Dolphin's core/uicommon require.
  Modeled on DolphinNoGUI/MainNoGUI.cpp.
EmuThread.{h,cpp}: skeleton class — pause flag + per-frame signal,
  no BootManager integration yet (SP2).
libretro.h: copied verbatim from ppsspp-libretro for ABI compatibility.
```

---

### Task 3: DolphinLibretro/CMakeLists.txt

**Files:**
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/CMakeLists.txt`

The target that turns Task 2's sources into a loadable dylib.

- [ ] **Step 1: Write the CMakeLists.txt**

Create `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/CMakeLists.txt`:

```cmake
# Source/Core/DolphinLibretro/CMakeLists.txt
#
# Builds the libretro core (dolphin_libretro.dylib) as a MODULE library —
# loadable plugin, not a regular shared lib. Only the retro_* C ABI is
# exported; everything else is hidden via CXX_VISIBILITY_PRESET hidden.
#
# Built only when -DENABLE_LIBRETRO=ON. Gated at the parent
# Source/Core/CMakeLists.txt level.

add_library(dolphin_libretro MODULE)

target_sources(dolphin_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    EmuThread.cpp
)

target_include_directories(dolphin_libretro PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/Source/Core"
)

# Link Dolphin's static libs. core + uicommon transitively pull in
# audiocommon, videocommon, inputcommon, discio, videobackends, common.
target_link_libraries(dolphin_libretro PRIVATE
    core
    uicommon
)

# Libretro naming convention: dolphin_libretro.dylib (no "lib" prefix).
set_target_properties(dolphin_libretro PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dolphin_libretro"
)

# Hide all symbols by default — libretro frontends only need the retro_*
# C ABI. The RETRO_API macro in libretro.h adds explicit
# __attribute__((visibility("default"))) per function on non-Windows.
set_target_properties(dolphin_libretro PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
```

- [ ] **Step 2: Stage only (controller commits)**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CMakeLists.txt
git status --short
```

Report. Controller commits with message:
```
SP1: DolphinLibretro CMakeLists — MODULE library target

Builds dolphin_libretro.dylib (no lib prefix) linking against Dolphin's
core + uicommon static libs. Hidden visibility default — only retro_*
symbols export via RETRO_API.
```

---

### Task 4: Build the dylib and verify exports

**Files:** None modified. This is build verification.

- [ ] **Step 1: Configure cmake with ENABLE_LIBRETRO=ON**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake -B build-libretro -S . -G Ninja \
    -DENABLE_LIBRETRO=ON \
    -DENABLE_QT=OFF \
    -DENABLE_NOGUI=OFF \
    -DENABLE_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    2>&1 | tail -30
```

Expected: clean configure. Notice the line `-- Configuring done` and `-- Generating done`. If cmake fails on missing system dependencies (FFmpeg, libusb, etc.), they're Dolphin's pre-existing requirements — install via Homebrew as instructed by Dolphin's own README, then retry. Report any cmake errors that mention `DolphinLibretro/` specifically — those are ours.

- [ ] **Step 2: Build the dolphin_libretro target**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -40
```

Expected: build succeeds, last line is `[NNN/NNN] Linking CXX shared module .../dolphin_libretro.dylib` (or similar).

Common failure modes and fixes:
- **Undefined Host_* function**: Some additional Host_* function exists in Dolphin's core that we didn't stub. Check the link error for the exact symbol name (e.g. `_Host_NewSymbol`) and add a stub in HostStubs.cpp. Report this as DONE_WITH_CONCERNS so the plan can be updated.
- **`#include "Core/Host.h" not found`**: include path is wrong — the CMakeLists has `"${CMAKE_SOURCE_DIR}/Source/Core"` which makes `Core/Host.h` resolvable. If broken, double-check the include directory.
- **`audiocommon` / `videocommon` not linked**: transitive deps not pulled in. Add them explicitly to `target_link_libraries`.

- [ ] **Step 3: Locate the built dylib**

Run:
```bash
find /Users/mark/Documents/Projects/dolphin-libretro/build-libretro -name "dolphin_libretro.dylib" -type f
```

Expected: exactly one path printed. Record it as `$DYLIB_PATH` for Step 4.

- [ ] **Step 4: Verify the dylib exports the retro_* C ABI**

Run (substitute the actual path from Step 3):
```bash
DYLIB_PATH=$(find /Users/mark/Documents/Projects/dolphin-libretro/build-libretro -name "dolphin_libretro.dylib" -type f)
nm -gU "$DYLIB_PATH" | grep '_retro_' | sort
echo "---"
# Count: 25 retro_* entrypoints in the core C ABI we implement.
nm -gU "$DYLIB_PATH" | grep -c '_retro_'
```

Expected: 25 exported `_retro_*` symbols (underscore-prefixed because that's the Mach-O C naming convention). The full list:
```
_retro_api_version
_retro_cheat_reset
_retro_cheat_set
_retro_deinit
_retro_get_memory_data
_retro_get_memory_size
_retro_get_region
_retro_get_system_av_info
_retro_get_system_info
_retro_init
_retro_load_game
_retro_load_game_special
_retro_reset
_retro_run
_retro_serialize
_retro_serialize_size
_retro_set_audio_sample
_retro_set_audio_sample_batch
_retro_set_controller_port_device
_retro_set_environment
_retro_set_input_poll
_retro_set_input_state
_retro_set_video_refresh
_retro_unload_game
_retro_unserialize
```
That's 25; `retro_api_version` is one of them. (Some libretro headers also include `retro_audio_callback` declarations but we don't define those.)

Count should be exactly 25. If it's < 25, an entrypoint was missed in LibretroFrontend.cpp. If it's > 25, something else is leaking (visibility issue).

- [ ] **Step 5: Verify NON-retro symbols are NOT exported**

Run:
```bash
DYLIB_PATH=$(find /Users/mark/Documents/Projects/dolphin-libretro/build-libretro -name "dolphin_libretro.dylib" -type f)
# Total exported symbol count. Should be roughly the retro_* count
# (25) plus a handful of standard C++ runtime symbols. Definitely
# should NOT be tens of thousands.
nm -gU "$DYLIB_PATH" | wc -l
```

Expected: under 100. If it's > 1000, the visibility hiding didn't take effect — `CXX_VISIBILITY_PRESET hidden` isn't being applied. Check CMakeLists for typos.

- [ ] **Step 6: Stage nothing**

No file changes in this task. Move to Task 5.

---

### Task 5: Standalone load test

**Files:**
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools/test_load.cpp`
- Create: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools/CMakeLists.txt`
- Modify: `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/CMakeLists.txt` (add one line to include the tools/ subdirectory)

A separate executable that `dlopen`s the dylib and exercises its ABI. Doesn't link any Dolphin libs — just the system loader and `libretro.h`.

- [ ] **Step 1: Write tools/test_load.cpp**

Create `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools/test_load.cpp`:

```cpp
// SP1 smoke test: dlopen the built dolphin_libretro.dylib, verify the
// retro_* ABI is callable, retro_load_game(nullptr) returns false cleanly.
//
// Doesn't link any Dolphin code — purely a libretro-frontend-perspective
// load test. Stand-alone so it can run in CI without needing RetroArch
// installed.
//
// Usage:
//   ./dolphin_libretro_load_test <path-to-dolphin_libretro.dylib>

#include <dlfcn.h>
#include <cstdio>
#include <cstring>

#include "../libretro.h"

namespace {

void* g_handle = nullptr;

template <typename T>
T resolve(const char* name)
{
    T sym = reinterpret_cast<T>(dlsym(g_handle, name));
    if (!sym)
    {
        fprintf(stderr, "FAIL: dlsym(%s): %s\n", name, dlerror());
    }
    return sym;
}

// libretro environment callback the core may invoke during retro_init.
// Returns false for any request — we're not implementing any environment
// extensions in this smoke test.
bool environ_cb(unsigned cmd, void* data)
{
    (void)cmd; (void)data;
    return false;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <path-to-dolphin_libretro.dylib>\n", argv[0]);
        return 1;
    }

    g_handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!g_handle)
    {
        fprintf(stderr, "FAIL: dlopen: %s\n", dlerror());
        return 2;
    }

    // 1. retro_api_version matches our header.
    auto p_retro_api_version = resolve<unsigned(*)(void)>("retro_api_version");
    if (!p_retro_api_version) { dlclose(g_handle); return 3; }
    const unsigned ver = p_retro_api_version();
    if (ver != RETRO_API_VERSION)
    {
        fprintf(stderr, "FAIL: retro_api_version mismatch: got %u, expected %u\n",
                ver, RETRO_API_VERSION);
        dlclose(g_handle);
        return 4;
    }
    printf("OK: retro_api_version = %u\n", ver);

    // 2. retro_get_system_info returns sensible identification.
    auto p_get_sys_info =
        resolve<void(*)(struct retro_system_info*)>("retro_get_system_info");
    if (!p_get_sys_info) { dlclose(g_handle); return 5; }
    struct retro_system_info info = {};
    p_get_sys_info(&info);
    if (!info.library_name || strcmp(info.library_name, "Dolphin") != 0)
    {
        fprintf(stderr, "FAIL: library_name = %s (expected Dolphin)\n",
                info.library_name ? info.library_name : "(null)");
        dlclose(g_handle);
        return 6;
    }
    printf("OK: library = %s v%s\n",
           info.library_name, info.library_version ? info.library_version : "");
    printf("OK: extensions = %s\n", info.valid_extensions ? info.valid_extensions : "");

    // 3. retro_set_environment + retro_init lifecycle works cleanly.
    auto p_set_env = resolve<void(*)(retro_environment_t)>("retro_set_environment");
    auto p_init    = resolve<void(*)(void)>("retro_init");
    auto p_deinit  = resolve<void(*)(void)>("retro_deinit");
    if (!p_set_env || !p_init || !p_deinit) { dlclose(g_handle); return 7; }
    p_set_env(environ_cb);
    p_init();
    printf("OK: retro_init returned cleanly\n");

    // 4. retro_load_game(nullptr) returns false (skeleton behavior).
    auto p_load = resolve<bool(*)(const struct retro_game_info*)>("retro_load_game");
    if (!p_load) { p_deinit(); dlclose(g_handle); return 8; }
    const bool loaded = p_load(nullptr);
    if (loaded)
    {
        fprintf(stderr, "FAIL: retro_load_game(nullptr) returned true (skeleton should return false)\n");
        p_deinit();
        dlclose(g_handle);
        return 9;
    }
    printf("OK: retro_load_game(nullptr) = false (skeleton)\n");

    // 5. Clean shutdown.
    p_deinit();
    printf("OK: retro_deinit returned cleanly\n");

    if (dlclose(g_handle) != 0)
    {
        fprintf(stderr, "FAIL: dlclose: %s\n", dlerror());
        return 10;
    }
    printf("OK: dlclose returned cleanly\n");

    printf("\nSP1 load test: ALL PASS\n");
    return 0;
}
```

- [ ] **Step 2: Write tools/CMakeLists.txt**

Create `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools/CMakeLists.txt`:

```cmake
# tools/CMakeLists.txt
#
# Standalone smoke test that dlopens dolphin_libretro.dylib and exercises
# the retro_* ABI. Doesn't link any Dolphin libs — independent of the
# core's build success.

add_executable(dolphin_libretro_load_test EXCLUDE_FROM_ALL test_load.cpp)
target_include_directories(dolphin_libretro_load_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
# Ensure the dylib exists before the test is built.
add_dependencies(dolphin_libretro_load_test dolphin_libretro)
```

- [ ] **Step 3: Wire tools/ into the parent CMakeLists.txt**

In `/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/CMakeLists.txt`, add ONE line at the end:

```cmake
add_subdirectory(tools)
```

So the full file ends:
```cmake
set_target_properties(dolphin_libretro PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_subdirectory(tools)
```

- [ ] **Step 4: Build the load test**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake -B build-libretro 2>&1 | tail -5  # re-configure for the new tools/ subdir
cmake --build build-libretro --target dolphin_libretro_load_test 2>&1 | tail -10
```

Expected: clean build of `dolphin_libretro_load_test` executable.

- [ ] **Step 5: Run the load test**

Run:
```bash
DYLIB=$(find /Users/mark/Documents/Projects/dolphin-libretro/build-libretro -name "dolphin_libretro.dylib" -type f)
TEST=$(find /Users/mark/Documents/Projects/dolphin-libretro/build-libretro -name "dolphin_libretro_load_test" -type f)
"$TEST" "$DYLIB"
echo "exit: $?"
```

Expected output (all OK lines, then ALL PASS):
```
OK: retro_api_version = 1
OK: library = Dolphin vSP1-skeleton
OK: extensions = iso|gcm|gcz|ciso|wbfs|rvz|wad|wia|nkit|m3u|dol|elf|tgc
OK: retro_init returned cleanly
OK: retro_load_game(nullptr) = false (skeleton)
OK: retro_deinit returned cleanly
OK: dlclose returned cleanly

SP1 load test: ALL PASS
exit: 0
```

Any FAIL line or non-zero exit means the skeleton has a real issue — report as DONE_WITH_CONCERNS with the full output. Common issues:
- Crash on `retro_init`: Dolphin's `UICommon::Init()` (called indirectly by some Host_* invocation) may need additional setup. SP1 shouldn't trigger this since we don't call UICommon::Init yet, but if it happens, simplify retro_init further.
- `library_name = (null)`: LibretroFrontend.cpp's `retro_get_system_info` mutated the wrong field. Re-check.

- [ ] **Step 6: Stage only (controller commits)**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/tools/test_load.cpp \
        Source/Core/DolphinLibretro/tools/CMakeLists.txt \
        Source/Core/DolphinLibretro/CMakeLists.txt
git status --short
```

Report. Controller commits with message:
```
SP1: standalone load test for dolphin_libretro.dylib

dlopen smoke test that verifies retro_api_version, retro_get_system_info,
retro_init/deinit, and retro_load_game(nullptr)→false. No Dolphin code
linked — pure libretro-frontend-perspective ABI test.
```

---

### Task 6: Close out

**Files:** None modified.

- [ ] **Step 1: Verify all deliverables exist and the smoke test passes**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
echo "=== Source files ==="
ls -1 Source/Core/DolphinLibretro/{CMakeLists.txt,libretro.h,HostStubs.cpp,EmuThread.h,EmuThread.cpp,LibretroFrontend.cpp}
ls -1 Source/Core/DolphinLibretro/tools/{CMakeLists.txt,test_load.cpp}
echo ""
echo "=== Built dylib ==="
find build-libretro -name "dolphin_libretro.dylib" -type f
echo ""
echo "=== Load test ==="
DYLIB=$(find build-libretro -name "dolphin_libretro.dylib" -type f)
TEST=$(find build-libretro -name "dolphin_libretro_load_test" -type f)
"$TEST" "$DYLIB" 2>&1 | tail -10
echo ""
echo "=== Commits this SP ==="
git log --oneline -8
```

Expected:
- All 8 files present
- dylib built
- load test prints ALL PASS, exit 0
- Recent commits include: initial, ENABLE_LIBRETRO option, skeleton sources, CMakeLists, load test

- [ ] **Step 2: Report**

Report:
- All SP1 deliverables present
- dylib builds clean
- Load test passes
- SP2 (Metal NSView render path) is the next sub-project

- [ ] **Step 3: Nothing to commit**

SP1 done.

---

## Notes for the implementer

- This sub-project produces a **dylib that does nothing useful** by design. Don't try to wire actual emulation — SP2 handles that. SP1's job is to prove the build, link, and ABI export all work.
- **All git work happens in `/Users/mark/Documents/Projects/dolphin-libretro/`**, not `RetroNest-Project/`. The Dolphin tree gets its own git init in Task 1.
- **Commits are made by the controller**, not the subagent. The auto-mode classifier blocks subagent commits. Subagents stage with `git add` and report; the controller commits.
- **If Dolphin's required system dependencies aren't installed** (FFmpeg, libusb, etc.), cmake configure will fail with errors NOT mentioning `DolphinLibretro/`. Those are pre-existing Dolphin requirements — install via Homebrew per Dolphin's README and retry. Report which dependency was missing as a footnote.
- **If a Host_* function is missing from HostStubs.cpp**, the link error will name it directly (e.g. `Undefined symbol: _Host_FooBar`). Add it to HostStubs.cpp as a no-op or sensible default, and report this as a plan gap for the spec to capture.
- **Don't enable ENABLE_QT or ENABLE_NOGUI during this build** — they pull in Qt6 and X11 dependencies we don't need for SP1. The libretro target stands alone.
