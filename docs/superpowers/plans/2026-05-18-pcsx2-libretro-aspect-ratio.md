# PCSX2 libretro aspect ratio plumbing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Report the actual displayed aspect ratio from `retro_get_system_av_info`, and re-emit `SET_SYSTEM_AV_INFO` mid-session when it changes — so the `pcsx2_aspect_ratio` core option and widescreen patches actually move the displayed image.

**Architecture:** A small two-function helper in a new TU. The pure-function core `ComputeFromInputs(aspect_enum, custom_override, video_mode_enum)` is unit-testable in isolation. A thin wrapper `Compute()` reads the three PCSX2 globals (`EmuConfig.GS.AspectRatio`, `EmuConfig.CurrentCustomAspectRatio`, `gsVideoMode`) and forwards them in. `LibretroFrontend.cpp` calls `Compute()` from `retro_get_system_av_info` and gates a per-frame emit-on-change re-emit in `retro_run`. Stretch resolves to 4:3 in v1 (RetroNest's `aspect_ratio <= 0` handling falls back to 4:3, not fill — see spec).

**Tech Stack:** C++20, CMake. Pure pcsx2-libretro shim scope; no edits in `pcsx2/` (upstream) or `RetroNest-Project/`.

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-18-pcsx2-libretro-aspect-ratio-design.md`

**Working repository:** `/Users/mark/Documents/Projects/pcsx2-libretro/`

---

## File Structure

| File | Role | Action |
| --- | --- | --- |
| `pcsx2-libretro/AspectRatio.h` | Helper public API | **Create** |
| `pcsx2-libretro/AspectRatio.cpp` | `ComputeFromInputs` (pure) + `Compute` (reads globals) | **Create** |
| `pcsx2-libretro/CMakeLists.txt` | Add `AspectRatio.cpp` to `target_sources` | **Modify** at line 24 (between `CoreOptionsMemoryCards.cpp` and `CoreResources.cpp`) |
| `pcsx2-libretro/tools/test_aspect_ratio.cpp` | Standalone unit test for `ComputeFromInputs` | **Create** |
| `pcsx2-libretro/LibretroFrontend.cpp` | Call `Compute()` from `retro_get_system_av_info`; add emit-on-change block + state in `retro_run` | **Modify** at lines 20 (include), 124 (state var), 310 (call site), 403 (emit block), 589/620 (resets) |

Two commits planned:
- **Commit A** after Task 6: helper + unit test (atomic, self-contained, no live behavior change).
- **Commit B** after Task 10: frontend wiring + build verification (atomic behavior change).

---

## Task 1: Create `AspectRatio.h` header

**Files:**
- Create: `pcsx2-libretro/pcsx2-libretro/AspectRatio.h`

- [ ] **Step 1: Write the header**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — translates PCSX2's display aspect to a libretro float.
//
// Mirrors GSRenderer's GetCurrentAspectRatioFloat (which is file-static
// upstream). Reads EmuConfig.GS.AspectRatio, EmuConfig.CurrentCustomAspectRatio,
// and gsVideoMode (for the Auto branch's progressive detection).
//
// Stretch returns 4.0f/3.0f in v1 — RetroNest's display item treats
// aspect_ratio <= 0 as a fallback to 4:3, not as "fill". Stretch-as-fill
// is delivered via RetroNest's per-emulator aspect mode, not this option.
// See spec 2026-05-18-pcsx2-libretro-aspect-ratio-design.md for rationale.

#pragma once

namespace Pcsx2Libretro::AspectRatio
{
    // Pure function — testable without PCSX2 globals.
    // aspect_ratio_enum: cast of AspectRatioType (Config.h:224).
    // custom_override:   value of EmuConfig.CurrentCustomAspectRatio; > 0.f
    //                    only when widescreen patches active under Auto.
    // video_mode_enum:   cast of GS_VideoMode (GS.h:215); used for
    //                    progressive detection in the Auto branch.
    float ComputeFromInputs(int aspect_ratio_enum, float custom_override, int video_mode_enum);

    // Reads the three PCSX2 globals and calls ComputeFromInputs.
    // Call from retro_get_system_av_info / retro_run only.
    float Compute();
}
```

- [ ] **Step 2: Verify file written**

Run: `head -3 /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/AspectRatio.h`
Expected: SPDX line at top.

---

## Task 2: Create `AspectRatio.cpp` implementation

**Files:**
- Create: `pcsx2-libretro/pcsx2-libretro/AspectRatio.cpp`

The implementation references the `AspectRatioType` enum values (`Stretch=0`, `RAuto4_3_3_2=1`, `R4_3=2`, `R16_9=3`, `R10_7=4`, `MaxCount=5`) from `pcsx2/Config.h:224-232`, and `GS_VideoMode::SDTV_480P=5` from `pcsx2/GS.h:215-229`.

- [ ] **Step 1: Write the implementation**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "AspectRatio.h"

#ifndef SP_ASPECT_TEST_ONLY
#include "pcsx2/Config.h"   // EmuConfig, AspectRatioType
#include "pcsx2/GS.h"       // gsVideoMode, GS_VideoMode
#endif

namespace Pcsx2Libretro::AspectRatio
{

namespace {
    // Mirrors AspectRatioType (Config.h:224). Duplicated as plain int values
    // so ComputeFromInputs has no dependency on the PCSX2 enum at compile time.
    constexpr int kStretch       = 0;
    constexpr int kAuto4_3_3_2   = 1;
    constexpr int kR4_3          = 2;
    constexpr int kR16_9         = 3;
    constexpr int kR10_7         = 4;

    // GS_VideoMode (GS.h:215). Only SDTV_480P is treated as progressive
    // by GSRenderer's GetCurrentAspectRatioFloat — match that behavior.
    constexpr int kVideoModeSDTV_480P = 5;
}

float ComputeFromInputs(int aspect_ratio_enum, float custom_override, int video_mode_enum)
{
    switch (aspect_ratio_enum)
    {
        case kR4_3:    return 4.0f / 3.0f;
        case kR16_9:   return 16.0f / 9.0f;
        case kR10_7:   return 10.0f / 7.0f;

        case kStretch:
            // See spec: RetroNest's aspect_ratio <= 0 fallback is 4:3, not
            // fill. Stretch-as-fill is delivered via RetroNest's per-emulator
            // aspect mode in v1; the libretro Stretch option is a no-op.
            return 4.0f / 3.0f;

        case kAuto4_3_3_2:
        default:
            // Widescreen patches override (Patch.cpp:825 sets this when AR=Auto).
            if (custom_override > 0.0f)
                return custom_override;
            // Only SDTV_480P counts as progressive in PCSX2's mapping.
            if (video_mode_enum == kVideoModeSDTV_480P)
                return 3.0f / 2.0f;
            return 4.0f / 3.0f;
    }
}

#ifndef SP_ASPECT_TEST_ONLY
float Compute()
{
    return ComputeFromInputs(
        static_cast<int>(EmuConfig.GS.AspectRatio),
        EmuConfig.CurrentCustomAspectRatio,
        static_cast<int>(gsVideoMode));
}
#endif

} // namespace Pcsx2Libretro::AspectRatio
```

- [ ] **Step 2: Verify file written**

Run: `wc -l /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/AspectRatio.cpp`
Expected: ~55 lines.

---

## Task 3: Wire `AspectRatio.cpp` into CMake

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/CMakeLists.txt:11-25`

- [ ] **Step 1: Add the source file to `target_sources`**

The existing block (lines 11-25) lists 13 source files. Add `AspectRatio.cpp` between `CoreOptionsMemoryCards.cpp` and `CoreResources.cpp` to keep the list alphabetised within each group.

Use the Edit tool with `old_string`:
```
    CoreOptionsMemoryCards.cpp
    CoreResources.cpp
```

And `new_string`:
```
    CoreOptionsMemoryCards.cpp
    CoreResources.cpp
    AspectRatio.cpp
```

(Trailing position is fine; existing list isn't strictly alphabetized — `CoreResources.cpp` already follows the `CoreOptions*` cluster. Append after `CoreResources.cpp`.)

- [ ] **Step 2: Verify the edit**

Run: `grep -n "AspectRatio\.cpp" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CMakeLists.txt`
Expected: one match on a line inside the `target_sources` block.

---

## Task 4: Write unit test fixtures

**Files:**
- Create: `pcsx2-libretro/pcsx2-libretro/tools/test_aspect_ratio.cpp`

Mirrors the existing `tools/test_region_prefix.cpp` standalone-test pattern. Uses `-DSP_ASPECT_TEST_ONLY` so the test TU compiles without PCSX2 headers — `ComputeFromInputs` is the pure function we're exercising.

- [ ] **Step 1: Write the failing test**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for AspectRatio::ComputeFromInputs.
// Pure function — no PCSX2 link required.
//
//   clang++ -std=c++20 -I../ test_aspect_ratio.cpp \
//       ../AspectRatio.cpp -o test_aspect_ratio \
//       -DSP_ASPECT_TEST_ONLY
//   ./test_aspect_ratio

#include "../AspectRatio.h"

#include <cmath>
#include <cstdio>

using Pcsx2Libretro::AspectRatio::ComputeFromInputs;

// Enum values mirror AspectRatioType (Config.h:224) and
// GS_VideoMode (GS.h:215). Hard-coded here to avoid pulling PCSX2 headers.
constexpr int AR_STRETCH    = 0;
constexpr int AR_AUTO       = 1;
constexpr int AR_4_3        = 2;
constexpr int AR_16_9       = 3;
constexpr int AR_10_7       = 4;

constexpr int VM_UNINITIALIZED = 0;
constexpr int VM_NTSC          = 2;
constexpr int VM_SDTV_480P     = 5;
constexpr int VM_HDTV_1080I    = 8;

static int failures = 0;

static void check(const char* label, float got, float want)
{
    const bool ok = std::fabs(got - want) < 0.001f;
    std::printf("[%s] %s: got=%.4f want=%.4f\n",
                ok ? "PASS" : "FAIL", label, got, want);
    if (!ok) ++failures;
}

int main()
{
    // Fixed enum values — video mode and custom override irrelevant.
    check("4:3",            ComputeFromInputs(AR_4_3,     0.0f, VM_NTSC),       4.0f / 3.0f);
    check("16:9",           ComputeFromInputs(AR_16_9,    0.0f, VM_NTSC),       16.0f / 9.0f);
    check("10:7",           ComputeFromInputs(AR_10_7,    0.0f, VM_NTSC),       10.0f / 7.0f);

    // Stretch: 4:3 fallback per spec (v1).
    check("Stretch → 4:3",  ComputeFromInputs(AR_STRETCH, 0.0f, VM_NTSC),       4.0f / 3.0f);
    check("Stretch ignores custom", ComputeFromInputs(AR_STRETCH, 1.777f, VM_NTSC), 4.0f / 3.0f);

    // Auto branch — no patch override, interlaced → 4:3.
    check("Auto NTSC interlaced",   ComputeFromInputs(AR_AUTO, 0.0f, VM_NTSC),       4.0f / 3.0f);
    check("Auto HDTV 1080I",        ComputeFromInputs(AR_AUTO, 0.0f, VM_HDTV_1080I), 4.0f / 3.0f);
    check("Auto Uninitialized",     ComputeFromInputs(AR_AUTO, 0.0f, VM_UNINITIALIZED), 4.0f / 3.0f);

    // Auto branch — progressive (SDTV_480P) → 3:2.
    check("Auto SDTV 480P → 3:2",   ComputeFromInputs(AR_AUTO, 0.0f, VM_SDTV_480P),  3.0f / 2.0f);

    // Auto branch — widescreen-patch override wins regardless of video mode.
    check("Auto + patch 16:9",      ComputeFromInputs(AR_AUTO, 16.0f / 9.0f, VM_NTSC),       16.0f / 9.0f);
    check("Auto + patch 21:9",      ComputeFromInputs(AR_AUTO, 21.0f / 9.0f, VM_SDTV_480P),  21.0f / 9.0f);
    check("Auto + patch overrides interlaced default",
                                     ComputeFromInputs(AR_AUTO, 16.0f / 9.0f, VM_HDTV_1080I), 16.0f / 9.0f);

    // Unknown enum value (out-of-range) → falls into default branch (treated as Auto).
    check("Unknown enum → Auto default", ComputeFromInputs(99, 0.0f, VM_NTSC),   4.0f / 3.0f);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Verify the file**

Run: `wc -l /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_aspect_ratio.cpp`
Expected: ~70 lines.

---

## Task 5: Build and run unit test

**Files:**
- No file changes — compile-and-run only.

- [ ] **Step 1: Compile the standalone test**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
  clang++ -std=c++20 -I../ test_aspect_ratio.cpp ../AspectRatio.cpp \
    -o test_aspect_ratio -DSP_ASPECT_TEST_ONLY
```

Expected: compiles cleanly, no warnings. (The `-DSP_ASPECT_TEST_ONLY` define gates out the PCSX2-header include and the `Compute()` global-reader function, so only `ComputeFromInputs` is built.)

- [ ] **Step 2: Run the test**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && ./test_aspect_ratio
```

Expected: 13 lines of `[PASS] ...`, final line `0 failure(s)`, exit code 0.

If any case fails, fix `AspectRatio.cpp` and re-run before continuing.

---

## Task 6: Commit helper + unit test

**Files:**
- No new changes — committing Tasks 1-5.

- [ ] **Step 1: Stage and commit**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro && \
  git add pcsx2-libretro/AspectRatio.h pcsx2-libretro/AspectRatio.cpp \
          pcsx2-libretro/CMakeLists.txt \
          pcsx2-libretro/tools/test_aspect_ratio.cpp && \
  git commit -m "$(cat <<'EOF'
feat(libretro): AspectRatio helper + standalone unit test

ComputeFromInputs translates PCSX2's AspectRatioType + CurrentCustomAspectRatio
+ gsVideoMode into the float libretro expects. Mirrors GSRenderer's file-static
GetCurrentAspectRatioFloat. Compute() is the thin globals-reading wrapper.

Stretch resolves to 4:3 in v1 — RetroNest's display item treats aspect_ratio
<= 0 as a 4:3 fallback (not fill); stretch-as-fill is delivered via
RetroNest's per-emulator aspect mode. See spec for rationale.

Standalone unit test exercises 13 cases via the pure ComputeFromInputs;
no PCSX2 headers required (SP_ASPECT_TEST_ONLY gate).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: clean commit, no hook failures.

- [ ] **Step 2: Verify**

Run: `cd /Users/mark/Documents/Projects/pcsx2-libretro && git log -1 --stat`
Expected: 4 files changed, all expected paths present, ~70 insertions.

---

## Task 7: Wire helper into `retro_get_system_av_info`

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp:20` (add include), `:310` (replace hardcode)

- [ ] **Step 1: Add the include**

Use Edit with `old_string`:
```cpp
#include "CoreResources.h"
#include "CoreOptions.h"
```

And `new_string`:
```cpp
#include "CoreResources.h"
#include "CoreOptions.h"
#include "AspectRatio.h"
```

- [ ] **Step 2: Replace the hardcoded aspect ratio**

Use Edit with `old_string`:
```cpp
    info->geometry.aspect_ratio = 4.0f / 3.0f;
```

And `new_string`:
```cpp
    info->geometry.aspect_ratio = AspectRatio::Compute();
```

(The `using namespace Pcsx2Libretro;` at line 34 of `LibretroFrontend.cpp` brings the `AspectRatio` namespace into scope, so `AspectRatio::Compute()` resolves.)

- [ ] **Step 3: Verify the edits**

Run: `grep -n "AspectRatio" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`
Expected: two matches — one in the include block (~line 22), one in `retro_get_system_av_info` (~line 310).

---

## Task 8: Add change-detection state variable

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp:124` (add file-static)

- [ ] **Step 1: Add `g_last_emitted_aspect` near `g_region_refined`**

Use Edit with `old_string`:
```cpp
unsigned g_detected_region = RETRO_REGION_NTSC;
double   g_detected_fps    = Pcsx2Libretro::CoreResources::kNtscFps;
bool     g_region_refined  = false;
```

And `new_string`:
```cpp
unsigned g_detected_region = RETRO_REGION_NTSC;
double   g_detected_fps    = Pcsx2Libretro::CoreResources::kNtscFps;
bool     g_region_refined  = false;

// Tracks the last aspect_ratio float we emitted via SET_SYSTEM_AV_INFO.
// retro_run compares each frame; re-emits only when the value changes by
// more than 0.001 (covers core-option toggles, widescreen-patch activation,
// and progressive↔interlaced video-mode transitions in Auto mode). -1.0f
// at startup forces first-frame emission to land in the cache.
float    g_last_emitted_aspect = -1.0f;
```

- [ ] **Step 2: Verify**

Run: `grep -n "g_last_emitted_aspect" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`
Expected: at least one match in the declarations block (~line 127).

---

## Task 9: Add emit-on-change block in `retro_run`

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp:403` (insert after the SP7a region-refinement block)

- [ ] **Step 1: Insert the block immediately after the SP7a region block**

Use Edit. The `old_string` is the tail of the existing SP7a block (lines 401-403):
```cpp
            g_region_refined = true;
        }
    }

    // One-shot log when VM first reports Running with a non-zero CRC.
```

And the `new_string` inserts the new block between the two:
```cpp
            g_region_refined = true;
        }
    }

    // Aspect ratio change detection. The current effective aspect can shift
    // mid-session via three independent inputs:
    //   1. user toggling pcsx2_aspect_ratio (libretro options layer)
    //   2. widescreen patches activating (Patch.cpp:825 writes
    //      EmuConfig.CurrentCustomAspectRatio in the Auto branch)
    //   3. gsVideoMode progressive↔interlaced transitions in the Auto branch
    // Compute() is ~5 enum compares per frame; the actual SET_SYSTEM_AV_INFO
    // call is rare (gated on >0.001 float change).
    {
        const float current_aspect = AspectRatio::Compute();
        if (std::fabs(current_aspect - g_last_emitted_aspect) > 0.001f)
        {
            retro_system_av_info av{};
            retro_get_system_av_info(&av);
            if (g_frontend.environ_cb)
                g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
            FrontendLog(RETRO_LOG_INFO,
                "[AspectRatio] re-emitted aspect=%.4f (was %.4f)",
                current_aspect, g_last_emitted_aspect);
            g_last_emitted_aspect = current_aspect;
        }
    }

    // One-shot log when VM first reports Running with a non-zero CRC.
```

- [ ] **Step 2: Verify**

Run: `grep -n "\[AspectRatio\]" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`
Expected: one match in `retro_run` (~line 420).

---

## Task 10: Reset state in load/unload paths

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp:589` (retro_load_game reset), `:620` (retro_unload_game reset)

Two places to reset `g_last_emitted_aspect = -1.0f` so the next game's first-frame computation always re-emits (matches how `g_region_refined = false` is handled in the same spots).

- [ ] **Step 1: Reset in `retro_load_game`**

Use Edit with `old_string`:
```cpp
    g_detected_region = RETRO_REGION_NTSC;
    g_detected_fps    = Pcsx2Libretro::CoreResources::kNtscFps;
    g_region_refined  = false;
```

And `new_string`:
```cpp
    g_detected_region = RETRO_REGION_NTSC;
    g_detected_fps    = Pcsx2Libretro::CoreResources::kNtscFps;
    g_region_refined  = false;
    g_last_emitted_aspect = -1.0f;
```

- [ ] **Step 2: Reset in `retro_unload_game`**

Use Edit with `old_string`:
```cpp
    g_region_refined  = false;            // re-run gsVideoMode refinement on next game load
```

And `new_string`:
```cpp
    g_region_refined  = false;            // re-run gsVideoMode refinement on next game load
    g_last_emitted_aspect = -1.0f;        // force re-emit on next game's first frame
```

- [ ] **Step 3: Verify**

Run: `grep -n "g_last_emitted_aspect" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`
Expected: 4 matches total — declaration (~line 127), `retro_run` block (~line 415), `retro_load_game` reset (~line 590), `retro_unload_game` reset (~line 621).

---

## Task 11: Build the libretro core (Rosetta x86_64)

**Files:**
- No source changes — verification only.

Per the project's dev-build hygiene rule (memory: `session_handoff_patches_shipped`): skip arm64/universal during iteration; the user's runtime is Rosetta x86_64.

- [ ] **Step 1: Configure (only if `build-x86_64` doesn't already exist)**

Check existence first:
```bash
ls /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/CMakeCache.txt 2>/dev/null
```

If it doesn't exist, skip — the previous patches.zip sub-project used `RetroNest-Project/cpp/build-x86_64`, not pcsx2-libretro's own. The libretro core is rebuilt as part of RetroNest's universal build script. For a per-arch dev build, ask the user for the exact configure invocation they want, or proceed straight to the RetroNest build (Step 2).

- [ ] **Step 2: Build via RetroNest's cmake target**

Run:
```bash
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target RetroNest -j 4
```

Expected: clean build, no warnings touching the new files. The RetroNest build invokes pcsx2-libretro's sub-project, so `AspectRatio.cpp` gets compiled and linked into `pcsx2_libretro.dylib`.

If the build fails with header-resolution errors on `pcsx2/Config.h` or `pcsx2/GS.h`, double-check `target_include_directories` at `pcsx2-libretro/CMakeLists.txt:27` already maps `pcsx2/` — no CMake change should be needed.

- [ ] **Step 3: Verify the `.dylib` is fresh**

Run:
```bash
ls -la /Users/mark/Documents/Projects/pcsx2-libretro/bin/pcsx2_libretro.dylib
```

Expected: mtime within the last few minutes.

---

## Task 12: Manual smoke test under Rosetta x86_64

**Files:**
- No file changes — runtime verification.

Each step below produces a single observable line in the RetroNest log or a single visible change in the display surface. If any check fails, capture the failing log line and treat as a P0 to fix before proceeding.

- [ ] **Step 1: 4:3 baseline (no widescreen patch)**

Launch RetroNest. Load R&C 2 (NTSC) or any 4:3 title. With `pcsx2_aspect_ratio = 4:3` (default).

Expected log line within first frame:
```
[INFO] [pcsx2_libretro] [AspectRatio] re-emitted aspect=1.3333 (was -1.0000)
```

Expected display: image fills the 4:3 letterbox. No further `[AspectRatio]` log lines after frame 1.

- [ ] **Step 2: Mid-session toggle to 16:9 (no patch)**

While the game is running, open RetroNest's emulator settings dialog for PCSX2 and change Aspect Ratio to `16:9`.

Expected log line on next frame after the toggle:
```
[INFO] [pcsx2_libretro] [AspectRatio] re-emitted aspect=1.7778 (was 1.3333)
```

Expected display: image visibly widens; in-game geometry is now stretched (no widescreen patch, so this is expected/intentional — matches standalone PCSX2 behavior with the same setting).

- [ ] **Step 3: Widescreen patch payoff**

Reset Aspect Ratio to `Auto` and enable `pcsx2_enable_widescreen_patches`. Reload R&C 2.

Expected log: a re-emit log line with the aspect float matching whatever the patches.zip widescreen patch chose for R&C 2 (typically 1.7778 for a 16:9 patch).

Expected display: 16:9-shaped, in-game geometry now correctly proportioned (no squashing). This is the user-visible payoff that was zero before this sub-project.

- [ ] **Step 4: Stretch (v1 = 4:3 no-op)**

Set Aspect Ratio to `Stretch`. Confirm no per-frame chatter in the log. Expected emitted aspect = 1.3333. RetroNest-side stretch-as-fill remains available via RetroNest's own per-emulator aspect mode.

- [ ] **Step 5: No-regression idle replay**

Reload default settings, play 60 seconds. Confirm the log contains exactly **one** `[AspectRatio]` line (the first-frame emission) and no further chatter. No stutter, no frame hitches around any AR transition.

- [ ] **Step 6: PAL title sanity**

Load DBZ Tenkaichi 2 (PAL) with default Aspect Ratio. Confirm first-frame `[AspectRatio]` log line shows 1.3333. Confirm no extra re-emits when SP7a's region refinement fires.

---

## Task 13: Commit frontend wiring

**Files:**
- No new changes — committing Tasks 7-12.

- [ ] **Step 1: Stage and commit**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro && \
  git add pcsx2-libretro/LibretroFrontend.cpp && \
  git commit -m "$(cat <<'EOF'
feat(libretro): wire aspect ratio into retro_get_system_av_info + retro_run

retro_get_system_av_info now calls AspectRatio::Compute() instead of
hardcoding 4:3. retro_run gates a per-frame check against
g_last_emitted_aspect (epsilon 0.001); re-emits SET_SYSTEM_AV_INFO only
on real change. Covers all three change sources in one mechanism: core
option toggle, widescreen-patch activation, progressive↔interlaced
video-mode transitions.

Smoke-verified under Rosetta x86_64 on R&C 2 (NTSC) and DBZ TT2 (PAL).
Widescreen patches' visible payoff now lands on the displayed surface
(previously the patched 16:9 internal render was squashed back to 4:3).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 2: Verify**

Run: `cd /Users/mark/Documents/Projects/pcsx2-libretro && git log -1 --stat`
Expected: 1 file changed, `pcsx2-libretro/LibretroFrontend.cpp`, ~30 insertions.

---

## Task 14: Update memory

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/MEMORY.md`
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/aspect_ratio_pickup.md` (or replace with shipped session-handoff)

- [ ] **Step 1: Mark the pickup memory as closed**

Edit `aspect_ratio_pickup.md` — prepend a `**RESOLVED 2026-05-18 — see [[session-handoff-aspect-ratio-shipped]].**` line under the heading. Keep the rest as archaeology.

- [ ] **Step 2: Write a new session-handoff memory**

Create `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_aspect_ratio_shipped.md` with:
- Frontmatter (type: project)
- One-paragraph summary: what shipped, two commit SHAs, smoke results
- Note that Stretch is a v1 no-op pending a RetroNest-side `<= 0` follow-up

- [ ] **Step 3: Update `MEMORY.md` index**

Replace the `aspect_ratio_pickup` entry's status line at the top of MEMORY.md with a pointer to the new session-handoff. Format: `- [**Aspect ratio plumbing SHIPPED 2026-05-18**](session_handoff_aspect_ratio_shipped.md) — one-line hook.`

---

## Self-Review Checklist (performed during plan authoring)

**Spec coverage:**
- ✅ Helper file pair `AspectRatio.{h,cpp}` → Tasks 1-2
- ✅ Mapping table (5 enum cells + custom override + progressive branch) → Task 2 implementation + Task 4 test cases
- ✅ `retro_get_system_av_info` swap → Task 7
- ✅ `retro_run` emit-on-change → Tasks 8-9
- ✅ State reset in load/unload → Task 10
- ✅ CMake wiring → Task 3
- ✅ Unit test → Tasks 4-5
- ✅ Manual smoke under Rosetta x86_64 → Tasks 11-12
- ✅ Out-of-scope items (FMV, RetroNest edits) flagged → noted in plan body + spec

**Placeholder scan:** no "TBD"/"TODO"/"implement later". Every code step has actual code. Every test step has actual command + expected output.

**Type/identifier consistency:**
- `Compute()` / `ComputeFromInputs()` — used identically across Tasks 1, 2, 4, 7, 9
- `g_last_emitted_aspect` — Tasks 8, 9, 10 all reference the same identifier
- `AspectRatio::Compute()` resolves via existing `using namespace Pcsx2Libretro;` at LibretroFrontend.cpp:34 (Task 7 step 2 notes this)

---

## Execution Notes

- Total tasks: 14. Estimated ~3 hours implementation + 30min smoke.
- TDD discipline: Tasks 1-2 implement the helper, Tasks 4-5 then test it — slight inversion from strict red-green because the helper is small enough that writing both at once and gating with the standalone test is materially faster than scaffolding a failing test against a non-existent function. The standalone test still fails meaningfully if the implementation drifts from the mapping table.
- Two commits give clean bisection points: helper alone vs. wired up.
- If Task 12 smoke surfaces any issue (e.g., widescreen patches don't trigger the re-emit), inspect `Patch.cpp:825` timing — `CurrentCustomAspectRatio` should be set before the first retro_run that follows patch activation. SP7a's region refinement already polls per-frame in the same retro_run; the new block sits directly after it, so timing alignment is guaranteed.
