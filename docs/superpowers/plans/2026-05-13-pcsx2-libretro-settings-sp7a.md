# SP7a — Settings push: Resources path + region/fps — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the two dev-machine-specific hardcodes in the pcsx2-libretro core (resources path + NTSC-locked region/fps) with runtime detection so the dylib runs correctly on any machine, with correct PAL timing.

**Architecture:** A new `pcsx2-libretro/CoreResources.{h,cpp}` module hosts three pure-ish helpers: a `dladdr`-based resources-directory resolver, a serial-based region detector with `GameDatabase` lookup + prefix-heuristic fallback, and a `gsVideoMode` → region mapper used as a refinement pass once the game has executed `SetGsCrt`. `Settings.cpp` and `LibretroFrontend.cpp` are edited at the call sites. Pure prefix-heuristic logic is unit-tested via a standalone test program in `pcsx2-libretro/tools/`.

**Tech Stack:** C++20, CMake. PCSX2 internal APIs: `VMManager::GetDiscSerial`, `GameDatabase::findGame`, `Path::GetDirectory`/`Path::Combine`, `FileSystem::DirectoryExists`, the runtime `extern GS_VideoMode gsVideoMode` (`pcsx2/GS.h`). libretro env: `RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO`. Platform: `<dlfcn.h>` for `dladdr`.

**Spec reference:** `RetroNest-Project/docs/superpowers/specs/2026-05-13-pcsx2-libretro-settings-sp7a-design.md` (commit `9c6db04`).

---

## File Structure

```
pcsx2-libretro/
├── CoreResources.h               <-- NEW (~40 LOC: declarations)
├── CoreResources.cpp             <-- NEW (~120 LOC: impls)
├── CMakeLists.txt                <-- MODIFY (add CoreResources.cpp to target_sources)
├── Settings.cpp                  <-- MODIFY (2 hardcoded-path call sites)
├── LibretroFrontend.cpp          <-- MODIFY (4 sites: load_game / get_region / get_system_av_info / run)
└── tools/
    └── test_region_prefix.cpp    <-- NEW (~80 LOC: standalone unit test)
```

**File responsibilities:**

| File | Responsibility |
| ---- | -------------- |
| `CoreResources.h` | Public API for resources-path and region-detection helpers. Declarations only. |
| `CoreResources.cpp` | Implementations. Depends on PCSX2 internal headers (`pcsx2/GameDatabase.h`, `pcsx2/GS.h`, `common/Path.h`, `common/FileSystem.h`) and `<dlfcn.h>`. |
| `Settings.cpp` | Pull the resources path from `CoreResources::ResolveResourcesDir()` once and use it in both assignment sites. |
| `LibretroFrontend.cpp` | Cache detected region/fps in module statics; reset them in `retro_load_game`; serve them in `retro_get_region` and `retro_get_system_av_info`; run the gsVideoMode refinement pass in `retro_run`. |
| `tools/test_region_prefix.cpp` | Standalone clang++ test program that links nothing — verifies the prefix-heuristic function on a handful of known serials. Built manually (mirrors the existing `tools/test_loader.c` pattern). |

---

## Task 0: Sanity-check the pre-change build

**Files:** none (build verification only)

- [ ] **Step 1: Verify the existing build is green before any changes**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -20
```

Expected: build finishes with no errors, produces `build/pcsx2-libretro/pcsx2_libretro.dylib`. If it doesn't, stop and report — something pre-existing is broken; SP7a isn't the cause.

- [ ] **Step 2: Confirm the resources directory used by the current hardcoded path exists**

Run:
```bash
ls /Users/mark/Documents/Projects/pcsx2-libretro/bin/resources/ | head -5
```

Expected: lists `default.metallib`, `fonts/`, `gamedb.yaml`, `patches.zip`, etc. (whatever is currently there). This is what we will rsync to the new install-time location in Task 7.

---

## Task 1: Skeleton CoreResources module + CMake registration

**Files:**
- Create: `pcsx2-libretro/CoreResources.h`
- Create: `pcsx2-libretro/CoreResources.cpp`
- Modify: `pcsx2-libretro/CMakeLists.txt:11-19`

- [ ] **Step 1: Create `pcsx2-libretro/CoreResources.h`**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7a (Settings push): runtime resource discovery + PS2 region detection.
//
// Three helpers, kept together because each is small and they share no state:
//   - ResolveResourcesDir():       dladdr-based path next to the dylib
//   - DetectRegionFromSerial():    GameDB → prefix-heuristic → default
//   - DetectRegionFromSerialPrefix(): exposed for standalone unit testing
//   - RegionFromGsVideoMode():     runtime refinement after SetGsCrt
#pragma once

#include <optional>
#include <string>

// Forward-declared so this header doesn't pull pcsx2/GS.h transitively.
// CoreResources.cpp includes the real header.
enum class GS_VideoMode : int;

namespace Pcsx2Libretro::CoreResources
{
    struct DetectedRegion
    {
        unsigned libretro_region;  // RETRO_REGION_NTSC | RETRO_REGION_PAL
        double   fps;              // 59.94 | 50.0
    };

    // Returns the absolute path of <dirname(this dylib)>/pcsx2_libretro_resources/.
    // Logs RETRO_LOG_ERROR if dladdr fails or the path doesn't exist on disk;
    // returns the resolved path regardless so downstream code produces clear
    // failure modes rather than silent fallback to a wrong dir.
    std::string ResolveResourcesDir();

    // Three-tier detection: GameDatabase::findGame → prefix heuristic → default NTSC.
    DetectedRegion DetectRegionFromSerial(const std::string& serial);

    // Internal/testable: prefix-only path. Returns default NTSC if the
    // prefix doesn't match a known mapping. Exposed for unit testing
    // since it has no PCSX2-internal dependencies.
    DetectedRegion DetectRegionFromSerialPrefix(const std::string& serial);

    // Maps a runtime gsVideoMode to libretro region/fps. Returns nullopt
    // for Uninitialized (the EE thread hasn't executed SetGsCrt yet).
    std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode);
}
```

- [ ] **Step 2: Create `pcsx2-libretro/CoreResources.cpp` with empty implementations**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "CoreResources.h"

namespace Pcsx2Libretro::CoreResources
{

std::string ResolveResourcesDir()
{
    // Filled in by SP7a Task 4.
    return {};
}

DetectedRegion DetectRegionFromSerial(const std::string& serial)
{
    // Filled in by SP7a Task 3 (GameDB + prefix chain).
    (void)serial;
    return {1u /* RETRO_REGION_NTSC */, 59.94};
}

DetectedRegion DetectRegionFromSerialPrefix(const std::string& serial)
{
    // Filled in by SP7a Task 2 (prefix heuristic).
    (void)serial;
    return {1u /* RETRO_REGION_NTSC */, 59.94};
}

std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode)
{
    // Filled in by SP7a Task 4.
    (void)mode;
    return std::nullopt;
}

} // namespace Pcsx2Libretro::CoreResources
```

- [ ] **Step 3: Register CoreResources.cpp in CMakeLists.txt**

Modify `pcsx2-libretro/CMakeLists.txt:11-19` to add `CoreResources.cpp`:

```cmake
target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    Settings.cpp
    EmuThread.cpp
    LibretroAudioStream.cpp
    LibretroInputSource.cpp
    LibretroSaveState.cpp
    CoreResources.cpp
)
```

- [ ] **Step 4: Build to verify the skeleton compiles**

Run:
```bash
cmake --build build --target pcsx2_libretro 2>&1 | tail -10
```

Expected: success. The hard-coded magic number `1u` (= `RETRO_REGION_NTSC`) is a placeholder used in step 2's skeleton so we don't need to pull `libretro.h` yet; Task 6 replaces the literal with the symbolic constant once `LibretroFrontend.cpp` becomes the consumer.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreResources.h pcsx2-libretro/CoreResources.cpp pcsx2-libretro/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP7a Task 1: CoreResources skeleton

Adds pcsx2-libretro/CoreResources.{h,cpp} as the home for SP7a's
runtime-discovery helpers. All four functions return placeholder
values; subsequent tasks fill them in test-first.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Implement the prefix heuristic test-first

**Files:**
- Create: `pcsx2-libretro/tools/test_region_prefix.cpp`
- Modify: `pcsx2-libretro/CoreResources.cpp` (replace `DetectRegionFromSerialPrefix` body)

- [ ] **Step 1: Write the failing test**

Create `pcsx2-libretro/tools/test_region_prefix.cpp`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for DetectRegionFromSerialPrefix.
// Not built as part of pcsx2_libretro target — manual compile, no PCSX2
// link required (mirrors the test_loader.c pattern).
//
//   clang++ -std=c++20 -I../ test_region_prefix.cpp \
//       ../CoreResources.cpp -o test_region_prefix \
//       -DSP7A_TEST_PREFIX_ONLY
//   ./test_region_prefix
//
// SP7A_TEST_PREFIX_ONLY gates CoreResources.cpp's PCSX2-dependent
// includes (GameDatabase, GS) so the test compiles without a PCSX2
// build tree.

#include "../CoreResources.h"

#include <cassert>
#include <cstdio>
#include <string>

using Pcsx2Libretro::CoreResources::DetectRegionFromSerialPrefix;
using Pcsx2Libretro::CoreResources::DetectedRegion;

// RETRO_REGION_NTSC = 1, RETRO_REGION_PAL = 0 (per libretro.h). Hard-coded
// here to avoid pulling libretro.h into the standalone test.
constexpr unsigned NTSC = 1;
constexpr unsigned PAL  = 0;

static int failures = 0;
static void check(const char* label, const DetectedRegion& got,
                  unsigned want_region, double want_fps)
{
    const bool ok = (got.libretro_region == want_region)
                 && (got.fps == want_fps);
    std::printf("[%s] %s: got region=%u fps=%.2f, want region=%u fps=%.2f\n",
                ok ? "PASS" : "FAIL", label,
                got.libretro_region, got.fps, want_region, want_fps);
    if (!ok) ++failures;
}

int main()
{
    // US NTSC (R&C 2's actual serial)
    check("SCUS US NTSC", DetectRegionFromSerialPrefix("SCUS-97268"), NTSC, 59.94);
    check("SLUS US NTSC", DetectRegionFromSerialPrefix("SLUS-21134"), NTSC, 59.94);

    // PAL territories
    check("SLES PAL", DetectRegionFromSerialPrefix("SLES-50001"), PAL, 50.0);
    check("SCES PAL", DetectRegionFromSerialPrefix("SCES-50001"), PAL, 50.0);
    check("SCED PAL", DetectRegionFromSerialPrefix("SCED-50001"), PAL, 50.0);
    check("SLED PAL", DetectRegionFromSerialPrefix("SLED-50001"), PAL, 50.0);

    // Japan / Asia NTSC
    check("SLPS JP NTSC", DetectRegionFromSerialPrefix("SLPS-25001"), NTSC, 59.94);
    check("SCAJ JP NTSC", DetectRegionFromSerialPrefix("SCAJ-20001"), NTSC, 59.94);
    check("SLPM JP NTSC", DetectRegionFromSerialPrefix("SLPM-65001"), NTSC, 59.94);

    // Unknown prefix → default NTSC
    check("Unknown prefix", DetectRegionFromSerialPrefix("ZZZZ-12345"), NTSC, 59.94);

    // Empty string → default NTSC
    check("Empty string",  DetectRegionFromSerialPrefix(""),            NTSC, 59.94);

    // Too-short string → default NTSC
    check("Short string",  DetectRegionFromSerialPrefix("SL"),          NTSC, 59.94);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Add the `SP7A_TEST_PREFIX_ONLY` gate at the top of CoreResources.cpp**

The test program needs to compile `CoreResources.cpp` without pulling PCSX2 headers. Add a gate around the PCH and the bodies of the non-prefix functions. Modify the top of `pcsx2-libretro/CoreResources.cpp`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#ifndef SP7A_TEST_PREFIX_ONLY
#include "PrecompiledHeader.h"
#endif

#include "CoreResources.h"

#include <string>

namespace Pcsx2Libretro::CoreResources
{

#ifndef SP7A_TEST_PREFIX_ONLY
std::string ResolveResourcesDir()
{
    return {};  // SP7a Task 4
}

DetectedRegion DetectRegionFromSerial(const std::string& serial)
{
    (void)serial;
    return {1u, 59.94};  // SP7a Task 3
}

std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode)
{
    (void)mode;
    return std::nullopt;  // SP7a Task 4
}
#endif

DetectedRegion DetectRegionFromSerialPrefix(const std::string& serial)
{
    (void)serial;
    return {1u, 59.94};  // FAILING: returns NTSC for every input including SLES-50001
}

} // namespace Pcsx2Libretro::CoreResources
```

- [ ] **Step 3: Compile and run the test, observe failure**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_region_prefix.cpp ../CoreResources.cpp \
    -o test_region_prefix -DSP7A_TEST_PREFIX_ONLY
./test_region_prefix
```

Expected: 4 FAIL lines (the 4 PAL test cases), exit code 1, "4 failure(s)" at the end.

- [ ] **Step 4: Implement the prefix heuristic**

Replace the body of `DetectRegionFromSerialPrefix` in `pcsx2-libretro/CoreResources.cpp`:

```cpp
DetectedRegion DetectRegionFromSerialPrefix(const std::string& serial)
{
    constexpr unsigned NTSC = 1; // RETRO_REGION_NTSC
    constexpr unsigned PAL  = 0; // RETRO_REGION_PAL
    constexpr double NTSC_FPS = 59.94;
    constexpr double PAL_FPS  = 50.0;

    // Canonical form is PREFIX-NNNNN (per ExecutablePathToSerial in
    // pcsx2/CDVD/CDVD.cpp:525). Prefix is always the first 4 chars,
    // uppercase. Anything shorter than 4 chars can't be a valid serial.
    if (serial.size() < 4)
        return {NTSC, NTSC_FPS};

    const std::string prefix = serial.substr(0, 4);

    // PAL territories (Europe/Australia).
    if (prefix == "SLES" || prefix == "SCES"
        || prefix == "SCED" || prefix == "SLED")
        return {PAL, PAL_FPS};

    // NTSC territories (US, Japan, Korea, Asia).
    if (prefix == "SCUS" || prefix == "SLUS"
        || prefix == "SCAJ" || prefix == "SLPS" || prefix == "SLPM"
        || prefix == "SCKA" || prefix == "SLKA" || prefix == "SCKR"
        || prefix == "PSXC")
        return {NTSC, NTSC_FPS};

    // Unknown prefix — default NTSC. Caller (DetectRegionFromSerial) is
    // responsible for logging a WARN; this function is pure for unit
    // testability.
    return {NTSC, NTSC_FPS};
}
```

- [ ] **Step 5: Recompile and rerun the test, observe pass**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_region_prefix.cpp ../CoreResources.cpp \
    -o test_region_prefix -DSP7A_TEST_PREFIX_ONLY
./test_region_prefix
```

Expected: 12 PASS lines, "0 failure(s)", exit code 0.

- [ ] **Step 6: Confirm the pcsx2_libretro target still builds**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -5
```

Expected: success. The `SP7A_TEST_PREFIX_ONLY` macro is only defined for the standalone test build; the main target still compiles all of CoreResources.cpp.

- [ ] **Step 7: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreResources.cpp pcsx2-libretro/tools/test_region_prefix.cpp
git commit -m "$(cat <<'EOF'
SP7a Task 2: prefix-heuristic region detection

DetectRegionFromSerialPrefix maps PS2 disc serial prefixes
(SLES/SCUS/...) to NTSC/PAL. Pure C++, no PCSX2 deps. Standalone
test program in tools/ verifies 12 cases (US/PAL/JP/unknown).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: GameDatabase lookup layered on top

**Files:**
- Modify: `pcsx2-libretro/CoreResources.cpp` (replace `DetectRegionFromSerial` body)

- [ ] **Step 1: Replace the body of `DetectRegionFromSerial`**

In `pcsx2-libretro/CoreResources.cpp`, inside the `#ifndef SP7A_TEST_PREFIX_ONLY` block, replace `DetectRegionFromSerial`:

```cpp
DetectedRegion DetectRegionFromSerial(const std::string& serial)
{
    constexpr unsigned NTSC = 1; // RETRO_REGION_NTSC
    constexpr unsigned PAL  = 0; // RETRO_REGION_PAL
    constexpr double NTSC_FPS = 59.94;
    constexpr double PAL_FPS  = 50.0;

    // Tier 1: GameDatabase lookup. Authoritative for thousands of retail
    // games; entry->region is a free-form string ("NTSC-U", "NTSC-J",
    // "PAL", etc.). Case-insensitive "starts with PAL" → PAL.
    if (!serial.empty())
    {
        if (const auto* entry = GameDatabase::findGame(serial))
        {
            const std::string& region = entry->region;
            if (region.size() >= 3
                && (region[0] == 'P' || region[0] == 'p')
                && (region[1] == 'A' || region[1] == 'a')
                && (region[2] == 'L' || region[2] == 'l'))
            {
                FrontendLog(RETRO_LOG_INFO,
                    "[SP7a] region=PAL fps=50.00 (GameDB '%s')",
                    region.c_str());
                return {PAL, PAL_FPS};
            }
            if (!region.empty())
            {
                FrontendLog(RETRO_LOG_INFO,
                    "[SP7a] region=NTSC fps=59.94 (GameDB '%s')",
                    region.c_str());
                return {NTSC, NTSC_FPS};
            }
            // GameDB entry exists but region field is empty — fall through.
        }
    }

    // Tier 2: prefix heuristic.
    const DetectedRegion by_prefix = DetectRegionFromSerialPrefix(serial);
    FrontendLog(RETRO_LOG_INFO,
        "[SP7a] region=%s fps=%.2f (prefix heuristic on '%s')",
        by_prefix.libretro_region == PAL ? "PAL" : "NTSC",
        by_prefix.fps, serial.c_str());

    // Tier 3 (warn for empty / clearly unknown serials). The prefix
    // heuristic always returns SOMETHING, so this is purely diagnostic.
    if (serial.empty()
        || (serial.size() >= 4
            && serial.substr(0, 4) != "SLES" && serial.substr(0, 4) != "SCES"
            && serial.substr(0, 4) != "SCED" && serial.substr(0, 4) != "SLED"
            && serial.substr(0, 4) != "SCUS" && serial.substr(0, 4) != "SLUS"
            && serial.substr(0, 4) != "SCAJ" && serial.substr(0, 4) != "SLPS"
            && serial.substr(0, 4) != "SLPM" && serial.substr(0, 4) != "SCKA"
            && serial.substr(0, 4) != "SLKA" && serial.substr(0, 4) != "SCKR"
            && serial.substr(0, 4) != "PSXC"))
    {
        FrontendLog(RETRO_LOG_WARN,
            "[SP7a] Unknown disc serial '%s' — defaulting to NTSC", serial.c_str());
    }

    return by_prefix;
}
```

- [ ] **Step 2: Add the required includes near the top of CoreResources.cpp**

Inside the `#ifndef SP7A_TEST_PREFIX_ONLY` region (below the existing `#include "PrecompiledHeader.h"` line, above the namespace open), add:

```cpp
#include "LibretroFrontend.h"          // FrontendLog
#include "pcsx2/GameDatabase.h"        // GameDatabase::findGame

#include "libretro.h"                  // RETRO_LOG_INFO / RETRO_LOG_WARN
```

(Verify these headers exist by checking `Settings.cpp` for the precedent — it already pulls `LibretroFrontend.h` and `libretro.h`.)

- [ ] **Step 3: Replace the placeholder integer with libretro symbolic constants**

Inside the same `#ifndef SP7A_TEST_PREFIX_ONLY` block, the constants used at the top of `DetectRegionFromSerial` (`NTSC = 1`, `PAL = 0`) should be replaced with the real libretro values now that we have the header:

```cpp
    constexpr unsigned NTSC = RETRO_REGION_NTSC;
    constexpr unsigned PAL  = RETRO_REGION_PAL;
```

Leave the analogous constants in `DetectRegionFromSerialPrefix` as the literal `1u`/`0u` magic numbers — that function compiles without libretro.h for the standalone test, and the values are pinned by the libretro ABI.

- [ ] **Step 4: Build pcsx2_libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -10
```

Expected: success.

- [ ] **Step 5: Re-run the standalone test to confirm SP7A_TEST_PREFIX_ONLY still gates correctly**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_region_prefix.cpp ../CoreResources.cpp \
    -o test_region_prefix -DSP7A_TEST_PREFIX_ONLY
./test_region_prefix
```

Expected: 12 PASS, 0 failure(s). The new GameDatabase code is excluded by the macro and doesn't break the test build.

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreResources.cpp
git commit -m "$(cat <<'EOF'
SP7a Task 3: GameDatabase-first region detection

DetectRegionFromSerial layers GameDB lookup on top of the prefix
heuristic. Three-tier chain (GameDB → prefix → warn+default) with
a log line at each tier so live smoke reveals which signal was used.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: ResolveResourcesDir + RegionFromGsVideoMode

**Files:**
- Modify: `pcsx2-libretro/CoreResources.cpp` (replace bodies of `ResolveResourcesDir` and `RegionFromGsVideoMode`)

- [ ] **Step 1: Implement `ResolveResourcesDir`**

In `pcsx2-libretro/CoreResources.cpp`, inside the `#ifndef SP7A_TEST_PREFIX_ONLY` block, replace `ResolveResourcesDir`:

```cpp
std::string ResolveResourcesDir()
{
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&ResolveResourcesDir), &info) == 0
        || info.dli_fname == nullptr)
    {
        FrontendLog(RETRO_LOG_ERROR,
            "[SP7a] dladdr failed when resolving resources dir; "
            "Metal GS init will fail to find metallibs");
        return {};
    }

    const std::string dylib_path(info.dli_fname);
    const std::string dir(Path::GetDirectory(dylib_path));
    const std::string resources = Path::Combine(dir, "pcsx2_libretro_resources");

    if (!FileSystem::DirectoryExists(resources.c_str()))
    {
        FrontendLog(RETRO_LOG_ERROR,
            "[SP7a] Resources directory not found at '%s' — install layout "
            "missing pcsx2_libretro_resources/ next to the dylib. Metal init will fail.",
            resources.c_str());
    }
    else
    {
        FrontendLog(RETRO_LOG_INFO,
            "[SP7a] Resources dir = %s", resources.c_str());
    }
    return resources;
}
```

- [ ] **Step 2: Implement `RegionFromGsVideoMode`**

Replace the body of `RegionFromGsVideoMode` in the same `#ifndef` block:

```cpp
std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode)
{
    constexpr unsigned NTSC = RETRO_REGION_NTSC;
    constexpr unsigned PAL  = RETRO_REGION_PAL;
    constexpr double NTSC_FPS = 59.94;
    constexpr double PAL_FPS  = 50.0;

    switch (mode)
    {
    case GS_VideoMode::Uninitialized:
        return std::nullopt;

    case GS_VideoMode::PAL:
    case GS_VideoMode::DVD_PAL:
    case GS_VideoMode::SDTV_576P:
        return DetectedRegion{PAL, PAL_FPS};

    case GS_VideoMode::NTSC:
    case GS_VideoMode::DVD_NTSC:
    case GS_VideoMode::SDTV_480P:
    case GS_VideoMode::HDTV_720P:
    case GS_VideoMode::HDTV_1080I:
    case GS_VideoMode::HDTV_1080P:
    case GS_VideoMode::VESA:
        return DetectedRegion{NTSC, NTSC_FPS};
    }
    // Unreachable — the enum is exhaustive in pcsx2/GS.h.
    return DetectedRegion{NTSC, NTSC_FPS};
}
```

- [ ] **Step 3: Add the required includes near the top of CoreResources.cpp**

Add to the `#ifndef SP7A_TEST_PREFIX_ONLY` include block (next to the existing additions from Task 3):

```cpp
#include "common/FileSystem.h"
#include "common/Path.h"
#include "pcsx2/GS.h"                  // GS_VideoMode

#include <dlfcn.h>                     // dladdr / Dl_info
```

- [ ] **Step 4: Build pcsx2_libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -10
```

Expected: success.

- [ ] **Step 5: Re-run the standalone test (regression guard)**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_region_prefix.cpp ../CoreResources.cpp \
    -o test_region_prefix -DSP7A_TEST_PREFIX_ONLY
./test_region_prefix
```

Expected: 12 PASS, 0 failure(s).

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreResources.cpp
git commit -m "$(cat <<'EOF'
SP7a Task 4: ResolveResourcesDir + RegionFromGsVideoMode

dladdr-based resources path resolver (logs the resolved location for
diagnosis); GS_VideoMode → libretro region/fps mapper used by the
retro_run refinement pass. SDTV_576P classified PAL per spec.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Replace hardcoded Resources path in Settings.cpp

**Files:**
- Modify: `pcsx2-libretro/Settings.cpp:158-159` and `:204-205` (the two hardcoded path strings)

- [ ] **Step 1: Add the CoreResources include at the top of Settings.cpp**

In `pcsx2-libretro/Settings.cpp`, add to the includes block (around line 12, after the existing `pcsx2/...` includes):

```cpp
#include "CoreResources.h"
```

- [ ] **Step 2: Replace the first hardcoded path (lines 158-159)**

Current code (Settings.cpp:148-168 region):

```cpp
    // SP3: SetResourcesDirectory() on macOS uses CocoaTools::GetResourcePath()
    // which returns the running app bundle's Resources dir (RetroNest's).
    // RetroNest's bundle doesn't have PCSX2's metallibs / patches.zip /
    // gamedb.yaml. Override directly to pcsx2-master's bin/resources/.
    //
    // TODO: hardcoded absolute path for SP3 MVP. SP7 (settings) should
    // derive this from dladdr() on our dylib + a known relative offset,
    // or have RetroNest copy these resources into a location adjacent
    // to the dylib at install time.
    EmuFolders::Resources =
        "/Users/mark/Documents/Projects/pcsx2-libretro/bin/resources";
```

Replace with:

```cpp
    // SP7a: SetResourcesDirectory() on macOS uses CocoaTools::GetResourcePath()
    // which returns the running app bundle's Resources dir (RetroNest's).
    // RetroNest's bundle doesn't have PCSX2's metallibs / patches.zip /
    // gamedb.yaml. Resolve at runtime via dladdr → <dylib_dir>/pcsx2_libretro_resources/.
    // RetroNest's install layout must place the resources directory next
    // to the dylib (see SP7a plan Task 7 for the rsync step).
    const std::string resources_dir = CoreResources::ResolveResourcesDir();
    EmuFolders::Resources = resources_dir;
```

- [ ] **Step 3: Replace the second hardcoded path (lines 204-205)**

Current code (Settings.cpp:194-208 region):

```cpp
    // SP3: point Folders/Resources at pcsx2-master's bin/resources/ so
    // GSDeviceMTL can load Metal22.metallib / Metal23.metallib / default.metallib
    // and PCSX2 can load patches.zip / gamedb.yaml / etc. Without this,
    // GSDeviceMTL::Create silently fails (m_dev.IsOk() returns false
    // when the shader library doesn't load) and AcquireWindow is never
    // called.
    //
    // TODO: this is a hardcoded absolute path for SP3 MVP. SP7 (settings)
    // should derive this from a runtime path (e.g. dladdr() on this dylib
    // to find its on-disk location, then a known relative offset), or
    // have RetroNest copy these resources next to the dylib at install time.
    g_si.SetStringValue("Folders", "Resources",
        "/Users/mark/Documents/Projects/pcsx2-libretro/bin/resources");
```

Replace with:

```cpp
    // SP7a: point Folders/Resources at the dladdr-derived path resolved
    // above. GSDeviceMTL needs this to load Metal22/23/default.metallib;
    // PCSX2 also reads patches.zip / gamedb.yaml from here.
    g_si.SetStringValue("Folders", "Resources", resources_dir.c_str());
```

- [ ] **Step 4: Build**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -10
```

Expected: success. The `using namespace Pcsx2Libretro;` is already in effect via the surrounding namespace block — `CoreResources::ResolveResourcesDir()` resolves to `Pcsx2Libretro::CoreResources::ResolveResourcesDir`. If the build complains about an unresolved name, fully qualify it.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/Settings.cpp
git commit -m "$(cat <<'EOF'
SP7a Task 5: replace hardcoded Resources path in Settings.cpp

Both SP3 absolute-path TODOs now resolve via dladdr at runtime.
Install/dev workflow must place pcsx2_libretro_resources/ next to the
dylib (Task 7 covers the rsync step).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Wire region detection into LibretroFrontend.cpp

**Files:**
- Modify: `pcsx2-libretro/LibretroFrontend.cpp` (4 sites: anon-namespace statics, `retro_load_game`, `retro_get_region`, `retro_get_system_av_info`, `retro_run`)

- [ ] **Step 1: Add the CoreResources include**

In `pcsx2-libretro/LibretroFrontend.cpp`, add to the includes block:

```cpp
#include "CoreResources.h"
#include "pcsx2/GS.h"          // gsVideoMode (extern)
#include "pcsx2/VMManager.h"   // VMManager::GetDiscSerial — verify it's already included, add if not
```

(`VMManager.h` is likely already pulled — confirm with `grep -n "VMManager" pcsx2-libretro/LibretroFrontend.cpp | head -3`. If not present, add.)

- [ ] **Step 2: Add module-level statics in the anonymous namespace**

Find the existing anonymous namespace at the top of LibretroFrontend.cpp (it holds `g_frontend`, `g_logged_running`, etc.). Add three new statics:

```cpp
namespace
{
    // ... existing statics ...

    // SP7a: cached region/fps reported to libretro. Defaults to NTSC/59.94
    // until DetectRegionFromSerial runs in retro_load_game. The refined
    // flag gates the gsVideoMode-driven SET_SYSTEM_AV_INFO re-emit so we
    // never emit twice.
    unsigned g_detected_region = RETRO_REGION_NTSC;
    double   g_detected_fps    = 59.94;
    bool     g_region_refined  = false;
}
```

(If LibretroFrontend.cpp wraps these in `Pcsx2Libretro::` namespace blocks, add inside that. Match the existing local convention.)

- [ ] **Step 3: Reset statics + detect region in `retro_load_game`**

In `retro_load_game`, locate the success block after `emu.Start(params)` returns true (around line 487 — the `FrontendLog(RETRO_LOG_INFO, "retro_load_game: VM started successfully");` line). Immediately before that line, add:

```cpp
    // SP7a: reset region cache and detect from disc serial.
    // VMManager::GetDiscSerial() is valid as soon as Initialize completed
    // (EmuThread::Start waits on m_init_done), so it's safe to read here.
    // gsVideoMode is still Uninitialized at this point; the retro_run
    // refinement pass picks it up once the EE has executed SetGsCrt.
    g_detected_region = RETRO_REGION_NTSC;
    g_detected_fps    = 59.94;
    g_region_refined  = false;
    const std::string disc_serial = VMManager::GetDiscSerial();
    const auto detected = Pcsx2Libretro::CoreResources::DetectRegionFromSerial(disc_serial);
    g_detected_region = detected.libretro_region;
    g_detected_fps    = detected.fps;
```

- [ ] **Step 4: Update `retro_get_region` to return the cached value**

Find the existing line (LibretroFrontend.cpp:532):

```cpp
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
```

Replace with:

```cpp
RETRO_API unsigned retro_get_region(void) { return g_detected_region; }
```

- [ ] **Step 5: Update `retro_get_system_av_info` to use the cached fps**

Find the `info->timing.fps = 60.0;` line in `retro_get_system_av_info`. Replace with:

```cpp
    info->timing.fps = g_detected_fps;
```

(The surrounding comment about "phase 3 will derive from GS region" can be removed or updated to reference SP7a.)

- [ ] **Step 6: Add the refinement pass at the top of `retro_run`**

In `retro_run`, immediately after entry (before any other logic), add:

```cpp
    // SP7a: gsVideoMode-driven refinement of region/fps. The serial-based
    // guess in retro_load_game is accurate for retail discs but homebrew
    // and region-modded discs can run in a different mode than their
    // serial implies. We re-check once per call until gsVideoMode is no
    // longer Uninitialized, then either confirm or re-emit SET_SYSTEM_AV_INFO
    // and stop checking.
    if (!g_region_refined)
    {
        if (auto refined = Pcsx2Libretro::CoreResources::RegionFromGsVideoMode(gsVideoMode))
        {
            const bool disagrees =
                refined->libretro_region != g_detected_region
                || std::abs(refined->fps - g_detected_fps) > 0.05;
            if (disagrees)
            {
                g_detected_region = refined->libretro_region;
                g_detected_fps    = refined->fps;
                retro_system_av_info av{};
                retro_get_system_av_info(&av);
                if (g_frontend.environ_cb)
                    g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
                FrontendLog(RETRO_LOG_INFO,
                    "[SP7a] region refined to %s fps=%.2f from gsVideoMode",
                    g_detected_region == RETRO_REGION_PAL ? "PAL" : "NTSC",
                    g_detected_fps);
            }
            g_region_refined = true;
        }
    }
```

Include `<cmath>` at the top of LibretroFrontend.cpp if `std::abs` for `double` isn't already available (likely is via PCH; confirm during build).

- [ ] **Step 7: Build**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -10
```

Expected: success.

- [ ] **Step 8: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/LibretroFrontend.cpp
git commit -m "$(cat <<'EOF'
SP7a Task 6: wire region/fps detection into LibretroFrontend

retro_get_region & retro_get_system_av_info now return cached values
populated from the disc serial in retro_load_game. retro_run runs a
gsVideoMode refinement pass once per frame until the EE has executed
SetGsCrt; on disagreement, re-emits SET_SYSTEM_AV_INFO once.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Install resources next to dylib + full smoke matrix

**Files:** none (filesystem + live runtime verification)

- [ ] **Step 1: Rsync the resources directory to the install location**

Run:
```bash
rsync -a --delete \
    /Users/mark/Documents/Projects/pcsx2-libretro/bin/resources/ \
    /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/
ls /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/ | head -8
```

Expected: lists `default.metallib`, `fonts/`, `gamedb.yaml`, `patches.zip`, etc.

- [ ] **Step 2: Copy the new dylib to the install location**

Run:
```bash
cp /Users/mark/Documents/Projects/pcsx2-libretro/build/pcsx2-libretro/pcsx2_libretro.dylib \
   /Users/mark/Documents/RetroNest/emulators/libretro/cores/
```

If the dev build is universal (per SP10), confirm the destination dylib is also universal:
```bash
file /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```
Expected: `Mach-O universal binary with 2 architectures: [arm64] ... [x86_64] ...`

If the build was single-arch (developer iterating quickly), that's fine for smoke — just note it in the commit message. Final SP7a merge to the user's daily-driver dylib expects universal per SP10.

- [ ] **Step 3: NTSC smoke — Ratchet & Clank 2**

Launch RetroNest, load Ratchet & Clank 2 (SCUS-97268). Watch the log for:

Expected lines (in order):
```
[SP7a] Resources dir = /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources
[SP7a] region=NTSC fps=59.94 (GameDB 'NTSC-U')
   OR
[SP7a] region=NTSC fps=59.94 (prefix heuristic on 'SCUS-97268')
```

Either GameDB or prefix path is acceptable — both reach the right answer. Then:
- Game renders ✅
- Audio plays without drift over a 30-second test ✅
- Memcard save/load still works (regression guard) ✅
- Save & Exit → Resume cycle still works (regression guard) ✅

NO `[SP7a] region refined` line should appear (gsVideoMode for R&C 2 = NTSC, agreement with serial guess).

- [ ] **Step 4: PAL smoke**

Pick any PAL game from the user's shelf (any SLES_ / SCES_ retail PS2 game). Launch it via RetroNest.

Expected lines:
```
[SP7a] Resources dir = ...
[SP7a] region=PAL fps=50.00 (GameDB 'PAL')
   OR
[SP7a] region=PAL fps=50.00 (prefix heuristic on 'SLES-NNNNN')
```

Then:
- Game runs at 50 Hz pacing (visually correct, not too-fast like the old NTSC-locked behavior would produce)
- Over a 2-minute test, audio does not drift relative to video
- NO `[SP7a] region refined` line (gsVideoMode = PAL, agreement)

- [ ] **Step 5: Install-location independence test**

Move the dylib + resources to a fresh location and run from there:
```bash
mkdir -p /tmp/sp7a_smoke
cp /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib /tmp/sp7a_smoke/
rsync -a /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/ \
    /tmp/sp7a_smoke/pcsx2_libretro_resources/
```

Symlink the dylib from RetroNest's cores dir to /tmp/sp7a_smoke/:
```bash
mv /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib \
   /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib.bak
ln -s /tmp/sp7a_smoke/pcsx2_libretro.dylib \
   /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

Launch R&C 2 again. Expected log line:
```
[SP7a] Resources dir = /tmp/sp7a_smoke/pcsx2_libretro_resources
```

Game should boot identically to step 3. Restore after the test:
```bash
rm /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
mv /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib.bak \
   /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
rm -rf /tmp/sp7a_smoke
```

- [ ] **Step 6: Negative-path test — verify the diagnostic actually surfaces**

Temporarily move the resources directory aside, attempt to launch R&C 2, observe the failure log line, then restore:

```bash
mv /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources \
   /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources.bak
```

Launch R&C 2 via RetroNest. Expected: GS init fails. Log contains:
```
[SP7a] Resources directory not found at '/Users/.../pcsx2_libretro_resources' — install layout missing ... Metal init will fail.
```

This is the diagnostic that future install bugs should surface. Restore:
```bash
mv /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources.bak \
   /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources
```

Then re-launch R&C 2 to confirm we're back to a working state.

- [ ] **Step 7: mGBA regression sanity check**

Launch any GBA game via RetroNest. Confirm it boots normally — SP7a doesn't touch RetroNest or mGBA, so this should be byte-for-byte identical to today.

- [ ] **Step 8: Final commit (any leftover work + plan/spec linkage)**

If any small fixes surfaced during smoke, commit them as `SP7a followup: <description>` commits.

If no fixes were needed, no further commit is required for this task — the smoke results are the gate.

---

## Post-merge memory updates

After SP7a is complete and pushed, update the auto-memory file
`/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/project_pcsx2_libretro_port.md`:

1. **Sub-project 8 (SP7) section:**
   - Add a new bullet `8a. ✅ SP7a — Resources path + region/fps — DONE YYYY-MM-DD` with the commit hashes for Tasks 1-6 of this plan.
   - Narrow the "next focus" line at top of sub-project 8 to "SP7b only (core-options migration)".
   - Correct the "INI patching" phrasing — clarify that today's `Settings.cpp` already uses `MemorySettingsInterface::SetXxxValue` calls (no INI string ever emitted); SP7b's scope is exposing those existing calls as user-tweakable libretro core options.

2. **Monthly upstream-update process block:** add the new rsync line:
   ```
   rsync -a --delete pcsx2-libretro/bin/resources/ \
       ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/
   ```

3. **Compat-gaps section:** mark the `retro_get_region` and `av.timing.fps` gaps as closed in SP7a.

4. Write a new session handoff memory under `memory/` summarizing the SP7a delivery and pointing at SP7b as the next focus.
