# RPCS3 → libretro core, Milestone 1 ("It's a core") — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> Read the spec first: `docs/superpowers/specs/2026-05-29-rpcs3-libretro-milestone1-design.md`. **Fork creation (`gh repo fork`), `git push`, and any tag/release steps are USER-RUN** (auto-mode blocks them) — the assistant prepares everything else and hands over exact commands.

**Goal:** Fork RPCS3 and get an in-tree `rpcs3_libretro` target that builds to `rpcs3_libretro.dylib` (Mach-O x86_64) and loads in a libretro frontend (RetroArch x86_64): `retro_init` runs and `retro_run()` loops emitting a black frame without crashing.

**Architecture:** A new in-tree directory `rpcs3/rpcs3_libretro/` (vendored `libretro.h` + `libretro.cpp` + `CMakeLists.txt`) built as a `MODULE` library behind an OFF-by-default `BUILD_LIBRETRO` CMake option, so the normal `rpcs3` app build is untouched. "C-first": Phase 1 links none of RPCS3's emulator code (pure libretro API stub) to prove build→load; Phase 1b (stretch) links the `Emu` core to attempt a headless boot, using symbols discovered in the Phase 0 recon task.

**Tech Stack:** RPCS3 (C++20, CMake, Ninja, LLVM, Vulkan/MoltenVK); macOS **x86_64 under Rosetta** with x64 Homebrew at `/usr/local` (`arch -x86_64`), mirroring the Dolphin libretro core's toolchain; RetroArch (Intel/x86_64) as the reference loader.

---

## Context the executor needs

- **Two-arch rule (from the Dolphin work):** x86_64 deps come from `/usr/local` (x64 Homebrew); arm64 from `/opt/homebrew`. Always build with `arch -x86_64 /usr/local/bin/cmake … -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_IGNORE_PATH=/opt/homebrew`, and steer `PKG_CONFIG_PATH=/usr/local/lib/pkgconfig` + `PATH=/usr/local/bin:…`. Mixing arches → `symbol(s) not found for architecture` link errors.
- **MODULE suffix:** a CMake `MODULE` library defaults to `.so` on macOS; libretro cores must be `.dylib`. Override `SUFFIX ".dylib"` (RPCS3 does the same trick for its own targets; Dolphin's `DolphinLibretro/CMakeLists.txt` is the precedent).
- **RPCS3 layout (confirm exact paths in Phase 0):** repo root has a top-level `CMakeLists.txt`; emulator core lives under `rpcs3/Emu/`; the Qt frontend under `rpcs3/rpcs3qt/`; submodules under `3rdparty/`. The global emulator instance and its boot method + GUI callback struct are what Phase 0 recon pins down.
- **RPCS3 build wiki (authoritative, dep list shifts):** https://wiki.rpcs3.net/index.php?title=Help:Building_RPCS3
- **Editor clangd will show false "file not found"** for RPCS3 headers (huge include graph) — trust the actual build, not the editor (same as the Dolphin repo).
- **The heavy build is slow under Rosetta** (LLVM + many submodules). Expect long wall-clock and possibly 1–2 dependency iterations.

---

## Task 1: Fork + clone RPCS3 (HUMAN for the fork; clone is local)

**Files:** none (repo setup).

- [ ] **Step 1 (USER-RUN): Fork the repo on GitHub**

```bash
gh repo fork RPCS3/rpcs3 --clone=false
```
Expected: a fork at `github.com/<your-user>/rpcs3`.

- [ ] **Step 2: Clone the fork with submodules into the Projects dir**

```bash
cd /Users/mark/Documents/Projects
git clone --recursive https://github.com/MarkrPearce96/rpcs3.git rpcs3-libretro
cd rpcs3-libretro
git remote rename origin fork
git remote add origin https://github.com/RPCS3/rpcs3.git   # upstream, read-only
git checkout -b libretro
```
Expected: `rpcs3-libretro/` exists, `git remote -v` shows `fork` = your fork and `origin` = RPCS3 upstream, branch is `libretro`.

- [ ] **Step 3: Verify submodules are present**

Run: `git submodule status | head` and `ls 3rdparty`
Expected: submodule SHAs listed (no `-` prefixes meaning uninitialised); `3rdparty/` populated.

- [ ] **Step 4: Commit the branch marker (empty tree change not needed — skip if nothing to commit)**

No commit yet; the branch exists. Proceed to Task 2.

---

## Task 2: Phase 0 — build stock RPCS3 (x86_64/Rosetta) to validate the toolchain

**Files:** none (build only). This is the gate: if stock RPCS3 won't build here, stop and report before writing any libretro code.

- [ ] **Step 1: Install x86_64 build dependencies (starting set — tune per the wiki/build errors)**

```bash
arch -x86_64 /usr/local/bin/brew update
arch -x86_64 /usr/local/bin/brew install \
  cmake ninja llvm@19 git glew vulkan-headers molten-vk \
  ffmpeg sdl3 pkg-config nasm
```
Note: this mirrors the RPCS3 macOS wiki's dependency set; the authoritative, current list is on the wiki — if `cmake` configure reports a missing package, add it here and re-run (same dep-tuning loop as the Dolphin CI build).

- [ ] **Step 2: Configure (x86_64, scrubbing /opt/homebrew)**

```bash
cd /Users/mark/Documents/Projects/rpcs3-libretro
arch -x86_64 env \
  HOMEBREW_PREFIX=/usr/local \
  PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig \
  PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
  LLVM_DIR=/usr/local/opt/llvm@19/lib/cmake/llvm \
  /usr/local/bin/cmake -S . -B build -G Ninja \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_IGNORE_PATH=/opt/homebrew \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_NATIVE_INSTRUCTIONS=OFF \
    -DUSE_SYSTEM_FFMPEG=ON
```
Expected: configure completes, exit 0. If it fails on a missing dep/flag, fix per the wiki and re-run (record what you changed).

- [ ] **Step 2b: If configure flags differ from the wiki, reconcile**

Read the RPCS3 build wiki's macOS section and adjust the flags above to match the current guidance (the project's flags evolve). Re-run Step 2 until configure succeeds.

- [ ] **Step 3: Build stock RPCS3 (long; run in background or expect a long wait)**

```bash
arch -x86_64 env PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
  /usr/local/bin/cmake --build build -j
```
Expected: build completes, exit 0, producing `build/bin/rpcs3.app` (or the path the wiki documents).

- [ ] **Step 4: Verify the built app is x86_64 (toolchain proof)**

```bash
file build/bin/rpcs3.app/Contents/MacOS/rpcs3
```
Expected: `Mach-O 64-bit … x86_64`.

> **If Step 3/4 cannot be made to pass after reasonable dep tuning, STOP** and report — the x86_64/Rosetta build of RPCS3 itself is the blocker, and the design's platform assumption must be revisited (e.g. arm64) before any libretro work.

---

## Task 3: Phase 0 — recon the core↔GUI boundary (feeds Tasks 4 & 6)

**Files:**
- Create: `rpcs3/rpcs3_libretro/RECON.md` (a committed notes file; later replaced/trimmed)

This task produces the exact symbol names Phase 1b needs. Do NOT guess them elsewhere — record them here.

- [ ] **Step 1: Find the emulator boot entry point and global instance**

```bash
cd /Users/mark/Documents/Projects/rpcs3-libretro
grep -rn "BootGame\|class Emulator\|extern Emulator" rpcs3/Emu/System.h rpcs3/Emu/System.cpp | head -40
grep -rn "Emulator Emu\|^Emulator " rpcs3/Emu/*.cpp | head
```
Expected: identify (a) the global emulator object (commonly `Emu`), (b) the boot method signature (e.g. `Emu.BootGame(path, …)` — record the EXACT signature/return type), (c) the header to include.

- [ ] **Step 2: Find the GUI/host callback indirection**

```bash
grep -rn "EmuCallbacks\|SetCallbacks\|get_gs_frame\|GSFrameBase" rpcs3/Emu/system_config*.h rpcs3/Emu/*.h rpcs3/rpcs3qt/*.cpp | head -40
```
Expected: identify the callbacks struct the emulator requires (the one `rpcs3qt` fills in) and which members are mandatory for boot (especially the GS-frame factory). Record the struct name, its header, and the members.

- [ ] **Step 3: Record findings in RECON.md and commit**

Write `rpcs3/rpcs3_libretro/RECON.md` documenting, with exact names + file:line: the boot method signature; the global emulator instance; the callbacks struct + mandatory members; and which translation units / link targets the `Emu` core lives in (needed for Task 6's `target_link_libraries`). Then:
```bash
git add rpcs3/rpcs3_libretro/RECON.md
git commit -m "rpcs3_libretro: Phase 0 recon — core boot + EmuCallbacks boundary"
```

---

## Task 4: Phase 1 — in-tree stub core that builds to rpcs3_libretro.dylib

**Files:**
- Create: `rpcs3/rpcs3_libretro/libretro.h` (vendored)
- Create: `rpcs3/rpcs3_libretro/libretro.cpp`
- Create: `rpcs3/rpcs3_libretro/CMakeLists.txt`
- Modify: top-level `CMakeLists.txt` (add the `BUILD_LIBRETRO` option + subdir)

- [ ] **Step 1: Vendor the libretro API header (pinned)**

```bash
cd /Users/mark/Documents/Projects/rpcs3-libretro
curl -fsSL -o rpcs3/rpcs3_libretro/libretro.h \
  https://raw.githubusercontent.com/libretro/libretro-common/v1/include/libretro.h
grep -c "RETRO_API_VERSION" rpcs3/rpcs3_libretro/libretro.h
```
Expected: file downloaded; grep returns ≥1. (If the `v1` ref 404s, use the `master` ref of `libretro/libretro-common/include/libretro.h` and pin the commit SHA in a comment at the top of the file.)

- [ ] **Step 2: Write the stub `libretro.cpp` (complete minimal core)**

Create `rpcs3/rpcs3_libretro/libretro.cpp`:
```cpp
// rpcs3_libretro — Milestone 1 stub core.
// Implements only the libretro API surface needed to LOAD + run an empty
// (black) frame loop. No RPCS3 emulator code is linked yet (that is M1b/M2).
#include "libretro.h"
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <vector>

namespace {
retro_environment_t        env_cb           = nullptr;
retro_video_refresh_t      video_cb         = nullptr;
retro_audio_sample_t       audio_cb         = nullptr;
retro_audio_sample_batch_t audio_batch_cb   = nullptr;
retro_input_poll_t         input_poll_cb    = nullptr;
retro_input_state_t        input_state_cb   = nullptr;
retro_log_printf_t         log_cb           = nullptr;

constexpr unsigned FB_W = 1280;
constexpr unsigned FB_H = 720;
std::vector<uint32_t> framebuffer;  // XRGB8888, opaque black

void fallback_log(enum retro_log_level, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); std::vfprintf(stderr, fmt, ap); va_end(ap);
}
}  // namespace

RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }

RETRO_API void retro_set_environment(retro_environment_t cb) {
    env_cb = cb;
    bool no_game = true;  // allow loading the core with no content
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)        { video_cb = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)          { audio_cb = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb){ audio_batch_cb = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb)              { input_poll_cb = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb)            { input_state_cb = cb; }

RETRO_API void retro_get_system_info(struct retro_system_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name     = "RPCS3 (libretro M1 stub)";
    info->library_version  = "0.0.1";
    info->valid_extensions = "elf|self|bin|iso|pkg";  // PS3 content; refined later
    info->need_fullpath    = true;   // RPCS3 needs a real path, not an in-memory buffer
    info->block_extract    = true;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->geometry.base_width   = FB_W;
    info->geometry.base_height  = FB_H;
    info->geometry.max_width    = FB_W;
    info->geometry.max_height   = FB_H;
    info->geometry.aspect_ratio = 16.0f / 9.0f;
    info->timing.fps            = 60.0;
    info->timing.sample_rate    = 48000.0;
}

RETRO_API void retro_init(void) {
    struct retro_log_callback logging;
    if (env_cb && env_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;
    else
        log_cb = fallback_log;
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (env_cb) env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    framebuffer.assign(static_cast<size_t>(FB_W) * FB_H, 0xff000000u);
    log_cb(RETRO_LOG_INFO, "[rpcs3-libretro] retro_init (M1 stub)\n");
}

RETRO_API void retro_deinit(void) {
    framebuffer.clear();
    framebuffer.shrink_to_fit();
}

RETRO_API bool retro_load_game(const struct retro_game_info* game) {
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "[rpcs3-libretro] retro_load_game: %s\n",
               (game && game->path) ? game->path : "(no path)");
    return true;  // M1: accept; no emulation yet
}

RETRO_API void retro_run(void) {
    if (input_poll_cb) input_poll_cb();
    if (video_cb)
        video_cb(framebuffer.data(), FB_W, FB_H, FB_W * sizeof(uint32_t));
}

RETRO_API void retro_unload_game(void) {}
RETRO_API void retro_reset(void) {}
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void*, size_t) { return false; }
RETRO_API bool retro_unserialize(const void*, size_t) { return false; }
RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned, bool, const char*) {}
RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t) { return false; }
RETRO_API void* retro_get_memory_data(unsigned) { return nullptr; }
RETRO_API size_t retro_get_memory_size(unsigned) { return 0; }
RETRO_API void retro_set_controller_port_device(unsigned, unsigned) {}
```

- [ ] **Step 3: Write the target `CMakeLists.txt`**

Create `rpcs3/rpcs3_libretro/CMakeLists.txt`:
```cmake
# rpcs3_libretro — Milestone 1 stub core. Built only with -DBUILD_LIBRETRO=ON.
# Produces rpcs3_libretro.dylib (MODULE). Links no RPCS3 emulator code yet.
add_library(rpcs3_libretro MODULE libretro.cpp)

target_include_directories(rpcs3_libretro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(rpcs3_libretro PROPERTIES
    PREFIX ""               # no "lib" prefix
    OUTPUT_NAME "rpcs3_libretro"
    SUFFIX ".dylib"         # MODULE would default to .so on macOS
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
```

- [ ] **Step 4: Wire it into the top-level `CMakeLists.txt`**

In the repo-root `CMakeLists.txt`, near the existing `add_subdirectory(rpcs3)` line (confirm exact location from Phase 0), add:
```cmake
option(BUILD_LIBRETRO "Build the experimental rpcs3_libretro core (Milestone 1)" OFF)
if(BUILD_LIBRETRO)
    add_subdirectory(rpcs3/rpcs3_libretro)
endif()
```

- [ ] **Step 5: Configure with the libretro target enabled + build it**

```bash
cd /Users/mark/Documents/Projects/rpcs3-libretro
arch -x86_64 env \
  PKG_CONFIG_PATH=/usr/local/lib/pkgconfig PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
  LLVM_DIR=/usr/local/opt/llvm@19/lib/cmake/llvm \
  /usr/local/bin/cmake -S . -B build -G Ninja \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_IGNORE_PATH=/opt/homebrew \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRETRO=ON
arch -x86_64 env PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
  /usr/local/bin/cmake --build build --target rpcs3_libretro -j
```
Expected: compiles + links a tiny module, exit 0. (The stub has no RPCS3 deps, so this is fast even though the tree is huge.)

- [ ] **Step 6: Verify the artifact is an x86_64 dylib**

```bash
DY=$(find build -name 'rpcs3_libretro.dylib' | head -1); echo "$DY"
file "$DY"
```
Expected: a path is found; `file` → `Mach-O 64-bit … x86_64` (a "bundle" or "dynamically linked shared library" — both fine for a libretro core).

- [ ] **Step 7: Commit**

```bash
git add rpcs3/rpcs3_libretro/libretro.h rpcs3/rpcs3_libretro/libretro.cpp \
        rpcs3/rpcs3_libretro/CMakeLists.txt CMakeLists.txt
git commit -m "rpcs3_libretro: Phase 1 stub core builds to rpcs3_libretro.dylib (x86_64)"
```
(Push is USER-RUN: `git push fork libretro`.)

---

## Task 5: Phase 1 — load the core in RetroArch (x86_64) and prove the run loop

**Files:** none (frontend verification).

- [ ] **Step 1: Get an x86_64 RetroArch**

Download the macOS **Intel** RetroArch build from https://www.retroarch.com/?page=platforms (the Intel `.dmg`), mount it, and copy `RetroArch.app` to `/Applications` (or note its path). Verify arch:
```bash
file /Applications/RetroArch.app/Contents/MacOS/RetroArch
```
Expected: includes `x86_64` (a universal binary is fine — it'll run x86_64 under Rosetta to match the core).

- [ ] **Step 2: Load the core with verbose logging (no content)**

```bash
DY=$(find /Users/mark/Documents/Projects/rpcs3-libretro/build -name 'rpcs3_libretro.dylib' | head -1)
arch -x86_64 /Applications/RetroArch.app/Contents/MacOS/RetroArch \
  -L "$DY" --verbose > /tmp/ra_rpcs3.log 2>&1 &
sleep 6
pkill -f RetroArch
```
(The core sets `SET_SUPPORT_NO_GAME`, so it can start with no ROM. If your RetroArch build refuses no-content start, pass any small dummy file path after `-L "$DY"`.)

- [ ] **Step 3: Verify the log shows the core initialised and ran a black loop, no crash**

```bash
grep -E "rpcs3-libretro|environ .*PIXEL_FORMAT|Found .*rpcs3|Using core|retro_run|Content ready" /tmp/ra_rpcs3.log | head -30
grep -iE "crash|segfault|exception|abort|dylib.*(not loaded|incompatible)" /tmp/ra_rpcs3.log || echo "no crash signatures ✓"
```
Expected: the log shows RetroArch loaded the core (system info name "RPCS3 (libretro M1 stub)"), `[rpcs3-libretro] retro_init` appears, the pixel format was accepted, and there are **no** crash/load-failure signatures. This is the **committed Milestone-1 acceptance**.

- [ ] **Step 4: Record the result**

Append a short "M1 committed criteria met" note (with the RetroArch version + key log lines) to `rpcs3/rpcs3_libretro/RECON.md`, then:
```bash
git add rpcs3/rpcs3_libretro/RECON.md
git commit -m "rpcs3_libretro: Phase 1 verified — core loads + black run loop in RetroArch x86_64"
```

---

## Task 6 (STRETCH): Phase 1b — headless Emu boot probe

**Files:**
- Modify: `rpcs3/rpcs3_libretro/libretro.cpp` (use symbols from Task 3 RECON.md)
- Modify: `rpcs3/rpcs3_libretro/CMakeLists.txt` (link the `Emu` core target)

> This is a **probe, not a requirement**. Reaching a clean "fails at GS-frame creation" (or similar) with a clear log IS success — it tells Milestone 2 exactly what to tackle. Do not let this block M1 sign-off.

- [ ] **Step 1: Link the Emu core into the target**

Using the link target/library names recorded in `RECON.md` (Task 3 Step 3), add to `rpcs3/rpcs3_libretro/CMakeLists.txt`:
```cmake
# Phase 1b: link RPCS3's emulator core (target name from RECON.md).
target_link_libraries(rpcs3_libretro PRIVATE <emu-core-target-from-RECON>)
```
Replace `<emu-core-target-from-RECON>` with the actual CMake target that builds `rpcs3/Emu/` (do not guess — use the recorded name).

- [ ] **Step 2: In `retro_load_game`, attempt a headless boot using the recorded API**

Extend `retro_load_game` to: include the emulator header (from RECON.md), construct/minimal-stub the required callbacks struct (members from RECON.md — stub the GS-frame factory to return null/no-op and log if it's invoked), then call the recorded boot method with `game->path`. Wrap in try/catch and log each stage:
```cpp
// pseudocode shape — fill exact names/signature from RECON.md:
//   log "[rpcs3-libretro] M1b: configuring headless EmuCallbacks"
//   <Emu>.SetCallbacks(make_headless_callbacks());
//   log "[rpcs3-libretro] M1b: booting %s", game->path
//   auto rc = <Emu>.BootGame(game->path /*, args per RECON*/);
//   log "[rpcs3-libretro] M1b: BootGame returned %d", (int)rc
```
Keep `retro_run()` as the black loop (no render yet).

- [ ] **Step 3: Rebuild the target**

```bash
arch -x86_64 env PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
  /usr/local/bin/cmake --build /Users/mark/Documents/Projects/rpcs3-libretro/build --target rpcs3_libretro -j
```
Expected: compiles + links. If it fails to link (Emu pulls in Qt/GUI symbols), that itself is the key finding — record it and stop the stretch here.

- [ ] **Step 4: Run in RetroArch with a real PS3 path and capture how far boot gets**

```bash
DY=$(find /Users/mark/Documents/Projects/rpcs3-libretro/build -name 'rpcs3_libretro.dylib' | head -1)
arch -x86_64 /Applications/RetroArch.app/Contents/MacOS/RetroArch \
  -L "$DY" "/path/to/a/ps3/eboot.bin" --verbose > /tmp/ra_rpcs3_boot.log 2>&1 &
sleep 12; pkill -f RetroArch
grep -E "rpcs3-libretro|M1b|Boot|GS|Emu" /tmp/ra_rpcs3_boot.log | head -40
```
Expected: log shows the boot attempt and the **exact stage it stops** (RPCS3 also writes its own log under its config dir — note that path too).

- [ ] **Step 5: Record the probe outcome and commit**

Document in `RECON.md`: whether `Emu` linked headlessly, the boot stage reached, and the first blocker (this is the input to the Milestone 2 spec). Then:
```bash
git add rpcs3/rpcs3_libretro/libretro.cpp rpcs3/rpcs3_libretro/CMakeLists.txt rpcs3/rpcs3_libretro/RECON.md
git commit -m "rpcs3_libretro: Phase 1b probe — headless Emu boot attempt + findings"
```

---

## Self-review notes

- **Spec coverage:** committed success criteria 1–3 ↔ Tasks 1,2,4,5; stretch criterion 4 ↔ Task 6; Phase 0 toolchain/recon ↔ Tasks 2,3; "out of scope" items (render, run-loop, input, audio, RA, RetroNest integration, CI, arm64) appear in NO task — correct.
- **Placeholders:** the only intentionally-deferred specifics are RPCS3-internal symbol names in Task 6, which are produced by Task 3's recon and explicitly marked "from RECON.md" rather than invented — this is by design (you cannot know RPCS3's private API without reading the cloned source). Task 4's stub uses only the stable public libretro API and is complete.
- **Dep list (Task 2 Step 1)** is a known-good starting set with the authoritative wiki cited + a tuning loop (Step 2b) — same pattern that worked for the Dolphin CI build.
- **Human/auto-mode:** Task 1 Step 1 (`gh repo fork`) and all `git push` steps are USER-RUN; everything else the assistant prepares.
- **Stop gates:** Task 2 (stock build must succeed or revisit platform) and Task 6 Step 3 (if Emu won't link headlessly, that's the finding — stop the stretch).
