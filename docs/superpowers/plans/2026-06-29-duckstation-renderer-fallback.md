# DuckStation libretro HW→Software Renderer Fallback — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When the DuckStation libretro core's hardware renderer fails to initialize at boot, automatically retry once with the software renderer (announced via OSD + log) instead of failing to launch.

**Architecture:** Entirely in the core's `retro_load_game` (`libretro.cpp`). A dependency-free pure helper decides whether to fall back; the boot path attempts the configured renderer, and on failure (when a hardware renderer was configured) reboots once with `SystemBootParameters::force_software_renderer = true`, posting a native OSD message. One-shot — nothing persisted. An env-gated test hook (`DUCKSTATION_FORCE_GPU_FAIL`) makes the path exercisable on hardware where the GPU never actually fails.

**Tech Stack:** C++20, DuckStation core (`duckstation-libretro/src/duckstation-libretro/`). Standalone `assert` unit test compiled with clang++ (mirrors `libretro_pad2_test.cpp`).

## Global Constraints

- **Core repo only:** `/Users/mark/Documents/Projects/duckstation-libretro` (git `master`, **no remote**). No host changes.
- **Boot-time only.** No mid-game renderer recovery.
- **One-shot.** No persisted state; the next launch retries the configured (hardware) renderer.
- **Never silent.** A successful fallback must post a native OSD message AND a `RETRO_LOG_WARN` log line.
- **Gate:** fall back only when the configured renderer is NOT Software (`g_settings.gpu_renderer != GPURenderer::Software`); Automatic counts as hardware-intent.
- **Test hook `DUCKSTATION_FORCE_GPU_FAIL`** is inert unless the env var is set, and only takes effect when a hardware renderer is configured.
- **OSD copy (verbatim):** `Hardware renderer unavailable — using Software renderer (reduced quality).`
- **Build/run:** x86_64 under Rosetta; universal core via `package.sh` (no `--arm64-only`). Per-TU arm64 ninja compile is an acceptable interim compile check (state which you ran).

---

## Task 1: Pure fallback-decision helper

A dependency-free predicate plus a standalone unit test, mirroring `libretro_pad2.h` / `libretro_analog.h`.

**Files:**
- Create: `src/duckstation-libretro/libretro_renderer_fallback.h`
- Test: `src/duckstation-libretro/libretro_renderer_fallback_test.cpp`

**Interfaces:**
- Produces: `bool ShouldFallBackToSoftware(bool configuredIsSoftware, bool bootSucceeded)` — returns `!bootSucceeded && !configuredIsSoftware`. Consumed by Task 2.

- [ ] **Step 1: Write the failing test**

Create `src/duckstation-libretro/libretro_renderer_fallback_test.cpp`:

```cpp
// Standalone unit test for ShouldFallBackToSoftware (HW->Software renderer fallback decision).
// Build & run:
//   clang++ -std=c++20 -I src src/duckstation-libretro/libretro_renderer_fallback_test.cpp -o /tmp/libretro_renderer_fallback_test && /tmp/libretro_renderer_fallback_test
#include "duckstation-libretro/libretro_renderer_fallback.h"

#include <cassert>
#include <cstdio>

int main()
{
  // Hardware renderer configured + boot failed -> fall back to software.
  assert(ShouldFallBackToSoftware(/*configuredIsSoftware=*/false, /*bootSucceeded=*/false) == true);
  // Hardware renderer configured + boot succeeded -> no fallback.
  assert(ShouldFallBackToSoftware(false, true) == false);
  // Software already configured + boot failed -> no fallback (nothing to fall back from).
  assert(ShouldFallBackToSoftware(true, false) == false);
  // Software already configured + boot succeeded -> no fallback.
  assert(ShouldFallBackToSoftware(true, true) == false);

  std::printf("libretro_renderer_fallback_test: OK\n");
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /Users/mark/Documents/Projects/duckstation-libretro && clang++ -std=c++20 -I src src/duckstation-libretro/libretro_renderer_fallback_test.cpp -o /tmp/libretro_renderer_fallback_test`
Expected: FAIL — `fatal error: 'duckstation-libretro/libretro_renderer_fallback.h' file not found`.

- [ ] **Step 3: Write minimal implementation**

Create `src/duckstation-libretro/libretro_renderer_fallback.h`:

```cpp
// Pure decision helper for the DuckStation libretro hardware->software renderer
// fallback. Deliberately dependency-free (no core headers) so it compiles
// standalone in libretro_renderer_fallback_test.cpp, mirroring libretro_pad2.h.
#pragma once

// True when a failed boot should be retried with the software renderer: only
// when the boot failed AND the configured renderer was not already Software
// (if the user already chose Software, there is nothing to fall back from).
inline bool ShouldFallBackToSoftware(bool configuredIsSoftware, bool bootSucceeded)
{
  return !bootSucceeded && !configuredIsSoftware;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `clang++ -std=c++20 -I src src/duckstation-libretro/libretro_renderer_fallback_test.cpp -o /tmp/libretro_renderer_fallback_test && /tmp/libretro_renderer_fallback_test`
Expected: PASS — prints `libretro_renderer_fallback_test: OK`.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/duckstation-libretro
git add src/duckstation-libretro/libretro_renderer_fallback.h src/duckstation-libretro/libretro_renderer_fallback_test.cpp
git commit -m "libretro: pure HW->Software renderer fallback decision helper + test"
```

---

## Task 2: Boot-with-fallback in `retro_load_game`

Replace the single `BootSystem` call with the fallback logic: attempt the configured renderer, and on failure (hardware configured) retry once forcing software, with OSD + log.

**Files:**
- Modify: `src/duckstation-libretro/libretro.cpp` (includes near the top; the boot block at lines 390–395)

**Interfaces:**
- Consumes: `ShouldFallBackToSoftware(bool, bool)` (Task 1); `g_settings.gpu_renderer` / `GPURenderer::Software` (`core/types.h`, already included); `System::BootSystem` + `SystemBootParameters::force_software_renderer` (`core/system.h`, already included); `Error` (`common/error.h`, already included); `Host::AddOSDMessage` + `OSDMessageType` (`util/imgui_manager.h`).

- [ ] **Step 1: Add includes**

In `src/duckstation-libretro/libretro.cpp`, add to the `#include "core/..."` / `#include "duckstation-libretro/..."` block:

```cpp
#include "duckstation-libretro/libretro_renderer_fallback.h"
#include "util/imgui_manager.h"
```

and add `#include <cstdlib>` to the `<...>` standard-header block (for `std::getenv`).

- [ ] **Step 2: Replace the boot block**

Find this block in `retro_load_game` (currently around lines 390–395):

```cpp
  SystemBootParameters params(std::string(game->path));
  if (!System::BootSystem(std::move(params), &error))
  {
    if (g_log)
      g_log(RETRO_LOG_ERROR, "DuckStation: BootSystem failed: %s\n", error.GetDescription().c_str());
    return false;
  }
```

Replace it with (reuses the `Error error;` already declared earlier in `retro_load_game`):

```cpp
  // Boot with automatic hardware->software renderer fallback. If a hardware
  // renderer was configured and the boot fails (e.g. Metal init failure), retry
  // once forcing the software renderer rather than failing to launch. One-shot:
  // nothing is persisted, so the next launch retries the configured renderer.
  const bool configured_is_software = (g_settings.gpu_renderer == GPURenderer::Software);

  // Test hook: DUCKSTATION_FORCE_GPU_FAIL skips the hardware attempt so the
  // fallback path can be exercised on hardware where the GPU never fails. Only
  // honored when a hardware renderer is configured; inert otherwise.
  const bool force_gpu_fail =
    !configured_is_software && (std::getenv("DUCKSTATION_FORCE_GPU_FAIL") != nullptr);

  bool booted = false;
  if (!force_gpu_fail)
  {
    SystemBootParameters params(std::string(game->path));
    booted = System::BootSystem(std::move(params), &error);
  }

  if (!booted)
  {
    if (g_log)
      g_log(force_gpu_fail ? RETRO_LOG_WARN : RETRO_LOG_ERROR,
            "DuckStation: hardware boot %s: %s\n",
            force_gpu_fail ? "skipped (DUCKSTATION_FORCE_GPU_FAIL)" : "failed",
            error.GetDescription().c_str());

    if (ShouldFallBackToSoftware(configured_is_software, booted))
    {
      if (g_log)
        g_log(RETRO_LOG_WARN, "DuckStation: falling back to Software renderer\n");

      Error sw_error;
      SystemBootParameters sw_params(std::string(game->path));
      sw_params.force_software_renderer = true;
      booted = System::BootSystem(std::move(sw_params), &sw_error);

      if (booted)
      {
        Host::AddOSDMessage(OSDMessageType::Warning,
                            "Hardware renderer unavailable — using Software renderer (reduced quality).");
      }
      else
      {
        if (g_log)
          g_log(RETRO_LOG_ERROR, "DuckStation: Software renderer fallback also failed: %s\n",
                sw_error.GetDescription().c_str());
        return false;
      }
    }
    else
    {
      return false;
    }
  }
```

- [ ] **Step 3: Build the core**

Run: `cd /Users/mark/Documents/Projects/duckstation-libretro && ./package.sh 2>&1 | tail -20`
Expected: clean build of the universal core. (An interim arm64 ninja `duckstation_libretro` compile+link is acceptable as the compile check — state which you ran. The link must resolve `Host::AddOSDMessage`, `System::BootSystem`, `g_settings`, `ShouldFallBackToSoftware`.)

- [ ] **Step 4: Re-run the pure unit tests** (regression guard)

Run:
```bash
clang++ -std=c++20 -I src src/duckstation-libretro/libretro_renderer_fallback_test.cpp -o /tmp/libretro_renderer_fallback_test && /tmp/libretro_renderer_fallback_test
clang++ -std=c++20 -I src src/duckstation-libretro/libretro_pad2_test.cpp -o /tmp/libretro_pad2_test && /tmp/libretro_pad2_test
```
Expected: both print `OK`.

- [ ] **Step 5: Commit**

```bash
git add src/duckstation-libretro/libretro.cpp
git commit -m "libretro: auto HW->Software renderer fallback on boot failure"
```

---

## Manual verification (USER — agent cannot launch the app)

After both tasks, build the universal core (`package.sh`, no `--arm64-only`) so RetroNest loads it, then:

1. **Fallback path:** launch the x86_64 RetroNest with `DUCKSTATION_FORCE_GPU_FAIL=1` set in the environment, boot a PS1 game →
   - the game still **boots and plays**;
   - the OSD shows **"Hardware renderer unavailable — using Software renderer (reduced quality)."**;
   - the picture is **native 1×** (no internal upscaling, even if scale is set to 4×);
   - `/tmp/rn.log` shows the `WARN … falling back to Software renderer` line.
2. **Normal path unaffected:** launch normally (no env var) → hardware renderer is used, internal upscaling works, **no** OSD toast.

(`/tmp/rn.log` is produced by the direct-exec run line in `RetroNest-Project/CLAUDE.md`.)

---

## Self-review notes

- Spec coverage: gate (`!= Software`), one-shot (no persisted state), OSD `Warning` + WARN log, env test hook, pure helper + standalone test, manual verification — all present.
- The `Error error;` reused in Task 2 is the one already declared earlier in `retro_load_game` (used for the CoreThreadInitialize/BootSystem error reporting). The fallback uses a fresh `Error sw_error;`.
- `BootSystem` fully tears down on failure (`system.cpp:1672–1676`), so the second attempt is safe.
