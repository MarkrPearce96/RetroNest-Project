# SP7c Phase 0 Implementation Plan — Foundation Refactor

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor SP7b's monolithic `CoreOptions.{h,cpp}` into the per-category aggregator pattern that SP7c Phases 1–5 will scale up. The three existing SP7b knobs (renderer / mtvu / fast_boot) migrate into a new `CoreOptionsEmulation.{h,cpp}` module; `CoreOptions.cpp` becomes a thin aggregator. Add a Python schema-fidelity check script. Live behavior is byte-identical to SP7b — same three core options, same renderer/MTVU/FastBoot tweak flow.

**Architecture:** Each future category card will own a sibling module (`CoreOptionsGraphics.{h,cpp}`, etc.). Each module exposes a free function `AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)` and a typed `Parse(retro_environment_t cb, Resolved::<Category>& out)` helper, plus an apply helper `ApplyDefaults(SettingsInterface& si, const Resolved::<Category>& opts)`. `CoreOptions.cpp` builds the master definitions vector once at first `EmitCoreOptionsV2` call and caches it as a function-local `static const std::vector` (pointer-stable for the core's lifetime). `Resolved` becomes a struct of per-category structs. The schema-fidelity Python script asserts that every `(key, value)` declared by the host adapter (`RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`) appears in the core's `kEmulationDefinitions[]` (and future category arrays).

**Tech Stack:**
- pcsx2-libretro: C++20, clang, libretro.h v2 core-options API.
- RetroNest-Project (read-only in this phase): C++20, Qt 6.
- Schema check: Python 3.
- Build: CMake (pcsx2-libretro is gated by `-DENABLE_LIBRETRO=ON`).
- Standalone unit test: clang++ with a `-DCORE_OPTIONS_TEST_ONLY` gate that stubs the FrontendLog dependency.

**Repo locations:**
- pcsx2-libretro: `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `retronest-libretro`).
- RetroNest-Project: `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`).

**Scope guard:** This phase adds **zero** new user-visible knobs. The live smoke gate at Task 9 verifies SP7b behavior reproduces exactly. If any tweak gains or loses an effect, fix it before declaring Phase 0 complete.

**Out-of-phase scope:** Anything in `pcsx2_libretro_adapter.cpp` (host side) stays at 3 rows. The host adapter changes start in Phase 1.

---

## File structure

### Changed in this phase

| File | Action | Responsibility |
|---|---|---|
| `pcsx2-libretro/CoreOptions.h` | Modify | Public interface: `struct Resolved` (now nested), `EmitCoreOptionsV2`, `ReadResolved`. |
| `pcsx2-libretro/CoreOptions.cpp` | Modify (rewrite as thin aggregator, ~80 LOC) | Builds master definitions vector by calling each category's `AppendDefinitions`; dispatches `SET_CORE_OPTIONS_V2`; dispatches `ReadResolved` to each category's `Parse` helper. |
| `pcsx2-libretro/CoreOptionsEmulation.h` | Create | Declares `struct Resolved::Emulation`, `AppendDefinitions`, `Parse`, `ApplyDefaults`. |
| `pcsx2-libretro/CoreOptionsEmulation.cpp` | Create | Owns `kEmulationDefinitions[]` (the 3 SP7b knobs); implements `AppendDefinitions`, `Parse`, `ApplyDefaults`. |
| `pcsx2-libretro/Settings.cpp` | Modify (3 apply sites) | `InitializeDefaults` delegates the 3 apply sites to `Emulation::ApplyDefaults`. |
| `pcsx2-libretro/LibretroFrontend.cpp` | Modify (one line) | `params.fast_boot = resolved.emulation.fast_boot;` instead of `resolved.fast_boot`. |
| `pcsx2-libretro/CMakeLists.txt` | Modify (+1 line) | Add `CoreOptionsEmulation.cpp` to `target_sources`. Add `check_schema_fidelity` custom target. |
| `pcsx2-libretro/tools/test_core_options.cpp` | Modify (rewrite test bodies for new shape) | All existing cases survive; one new structural test asserts `BuildDefinitions().size() == 3 + 1` (3 knobs + terminator) and key names are right. |
| `pcsx2-libretro/tools/check_schema_fidelity.py` | Create | Compares core's `kEmulationDefinitions[]` strings vs host's `pcsx2_libretro_adapter.cpp` SettingDef rows. Exits 1 on drift. |

### Untouched in this phase

- `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — host adapter stays at 3 SP7b rows.
- `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/*` — dialog + hub stay 1-card.
- All other pcsx2-libretro modules (`EmuThread.cpp`, `LibretroAudioStream.cpp`, etc.).

---

## Task 0: Baseline current state

**Files:** none.

- [ ] **Step 1: Confirm clean working tree on pcsx2-libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git status --short
git log --oneline -3
```
Expected: working tree clean (or only untracked test binaries under `tools/`), HEAD at `1b9e649c8` ("SP7b polish: correct EmitCoreOptionsV2's WARN message wording") or a descendant.

If anything is dirty, stop and resolve before proceeding.

- [ ] **Step 2: Confirm clean working tree on RetroNest-Project**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git status --short
git log --oneline -3
```
Expected: HEAD at `45643d3` ("docs: SP7c design — full PCSX2 libretro settings parity") or descendant. Only `?? cpp/build-*` untracked is fine. No staged or unstaged code changes.

- [ ] **Step 3: Run SP7b's existing standalone test to baseline-pass**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
./test_core_options
```

Expected output ends with:
```
0 failure(s)
```

If non-zero, stop — SP7b's baseline is broken and this plan can't proceed.

- [ ] **Step 4: Confirm pcsx2_libretro target builds**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -10
```

Expected: `[100%] Built target pcsx2_libretro`. If the build dir doesn't exist or is from an older toolchain, regenerate it per the per-fork build docs first.

- [ ] **Step 5: No commit. Baseline only.**

---

## Task 1: TDD — assert a future `BuildDefinitions()` matches the existing 3-knob table

This task adds the test before implementation. The test won't compile yet (the function doesn't exist). That's the failing-test step in TDD.

**Files:**
- Test: `pcsx2-libretro/tools/test_core_options.cpp` (modify — add Case 6).

- [ ] **Step 1: Open `test_core_options.cpp` and add a new structural test case after Case 5**

Insert this block after the existing Case 5 block (after line 160, before `std::printf("\n%d failure(s)\n"...`), and update the includes at the top:

Add `#include <span>` if not present.

Add the using-declaration after the existing `using` block (~line 25):
```cpp
using Pcsx2Libretro::CoreOptions::BuildDefinitions;
```

Insert the new test case before the trailing `std::printf("\n%d failure(s)\n", failures);`:

```cpp
    // -------- Case 6: BuildDefinitions returns the expected category structure --------
    //
    // SP7c Phase 0: assert that the master definitions table built by
    // BuildDefinitions() contains exactly the 3 SP7b knobs in the documented
    // order, plus the libretro terminator. Future SP7c phases extend this.
    {
        const auto& defs = BuildDefinitions();
        check_int("Case 6 count = 4 (3 knobs + terminator)",
                  static_cast<int>(defs.size()), 4);

        if (defs.size() >= 4) {
            check_bool("Case 6 [0].key = pcsx2_renderer",
                       defs[0].key && std::strcmp(defs[0].key, "pcsx2_renderer") == 0, true);
            check_bool("Case 6 [1].key = pcsx2_mtvu",
                       defs[1].key && std::strcmp(defs[1].key, "pcsx2_mtvu") == 0, true);
            check_bool("Case 6 [2].key = pcsx2_fast_boot",
                       defs[2].key && std::strcmp(defs[2].key, "pcsx2_fast_boot") == 0, true);
            check_bool("Case 6 [3] terminator (key == nullptr)",
                       defs[3].key == nullptr, true);
        }
    }
```

- [ ] **Step 2: Try to compile the test — expect a failure**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options 2>&1 | head -10
```

Expected: compile error mentioning `BuildDefinitions` not declared in `Pcsx2Libretro::CoreOptions`. This is the failing-test step.

- [ ] **Step 3: No commit. Test stays uncommitted until Task 3 lands the implementation.**

---

## Task 2: Create `CoreOptionsEmulation.{h,cpp}` skeleton with no logic yet

**Files:**
- Create: `pcsx2-libretro/CoreOptionsEmulation.h`
- Create: `pcsx2-libretro/CoreOptionsEmulation.cpp`

- [ ] **Step 1: Create `CoreOptionsEmulation.h`**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 0: Emulation-category core options.
//
// Owns the kEmulationDefinitions[] slice of the master core-options table.
// CoreOptions.cpp aggregates this module's slice (plus future siblings)
// into the single table dispatched via SET_CORE_OPTIONS_V2.

#pragma once

#include "libretro.h"

#include <vector>

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }
class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions::Emulation
{

// Per-category resolved values. Aggregated by struct Resolved.
//
// Fields preserve the SP7a-era defaults so a missing/empty options.json
// produces identical behavior to today.
struct Values
{
    int  renderer  = -1;    // GSRendererType: -1=Auto, 17=Metal, 13=SW, 11=Null
    bool mtvu      = true;  // EmuCore/Speedhacks/vuThread
    bool fast_boot = true;  // EmuCore/EnableFastBoot AND VMBootParameters.fast_boot
};

// Append this category's option definitions to the master vector.
// Called once from CoreOptions::BuildDefinitions on first emit.
// Does NOT append the libretro terminator — that is the master
// aggregator's responsibility.
void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out);

// Read this category's resolved values from the host. Called from
// CoreOptions::ReadResolved.
void Parse(retro_environment_t cb, Values& out);

// Apply this category's resolved values to the settings interface.
// Called from Pcsx2Libretro::Settings::InitializeDefaults.
void ApplyDefaults(MemorySettingsInterface& si, const Values& v);

} // namespace Pcsx2Libretro::CoreOptions::Emulation
```

- [ ] **Step 2: Create `CoreOptionsEmulation.cpp` with empty function bodies**

Note: `ApplyDefaults` calls `MemorySettingsInterface` methods that aren't available in the test-only standalone compile (PCSX2's `common/MemorySettingsInterface.h` pulls in the rest of `SettingsInterface`). So the function body is gated; the test build leaves `ApplyDefaults` undefined and the test never calls it.

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsEmulation.h"

#ifdef CORE_OPTIONS_TEST_ONLY
#include <cstdarg>
#include <cstdio>
static void FrontendLog(int /*level*/, const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
}
#else
#include "LibretroFrontend.h"                  // FrontendLog
#include "common/MemorySettingsInterface.h"    // MemorySettingsInterface + SettingsInterface base
#endif

#include <cstring>

namespace Pcsx2Libretro::CoreOptions::Emulation
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // Filled in Task 3.
    (void)out;
}

void Parse(retro_environment_t cb, Values& out)
{
    // Filled in Task 4.
    (void)cb;
    (void)out;
}

#ifndef CORE_OPTIONS_TEST_ONLY
// Body gated because MemorySettingsInterface's SetIntValue/SetBoolValue
// aren't available in the standalone-test compile chain. test_core_options
// never calls ApplyDefaults (it tests Parse + Emit only); the apply path
// is exercised at the live-smoke level via Settings::InitializeDefaults.
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // Filled in Task 5.
    (void)si;
    (void)v;
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Emulation
```

- [ ] **Step 3: Register the new .cpp in CMakeLists.txt**

Edit `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CMakeLists.txt`. Find the `target_sources(pcsx2_libretro PRIVATE` block (around line 11–21) and add `CoreOptionsEmulation.cpp` directly after `CoreOptions.cpp`:

```cmake
target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    Settings.cpp
    EmuThread.cpp
    LibretroAudioStream.cpp
    LibretroInputSource.cpp
    LibretroSaveState.cpp
    CoreOptions.cpp
    CoreOptionsEmulation.cpp
    CoreResources.cpp
)
```

- [ ] **Step 4: Verify the target builds with the new file**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -10
```

Expected: success. The new .cpp's empty function bodies link cleanly.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsEmulation.h pcsx2-libretro/CoreOptionsEmulation.cpp pcsx2-libretro/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP7c Phase 0 Task 2: scaffold CoreOptionsEmulation module

Empty module skeleton ready for the SP7b 3-knob migration.
struct Values, AppendDefinitions, Parse, ApplyDefaults declared;
implementations follow in Tasks 3-5. Registered in CMakeLists.
EOF
)"
```

---

## Task 3: Move the 3 SP7b definitions into `Emulation::AppendDefinitions`

**Files:**
- Modify: `pcsx2-libretro/CoreOptionsEmulation.cpp` (fill in `AppendDefinitions`).
- Modify: `pcsx2-libretro/CoreOptions.cpp` (delete the 3 definitions from `kDefinitions[]`, add `BuildDefinitions()`).
- Modify: `pcsx2-libretro/CoreOptions.h` (declare `BuildDefinitions`, rename gate macro to `CORE_OPTIONS_TEST_ONLY`).

- [ ] **Step 1: Declare `BuildDefinitions` in `CoreOptions.h`**

Edit `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions.h`.

After the `Resolved ReadResolved(retro_environment_t cb);` line and before the closing `} // namespace ...`, add:

```cpp
// Build (or return the cached) master option-definitions vector.
// First call concatenates each category's AppendDefinitions output and
// appends the libretro terminator. Subsequent calls return the same
// vector by reference — the pointer-to-storage stays valid for the
// process lifetime (function-local static).
//
// Exposed for test_core_options.cpp's structural assertions. Production
// code reaches it indirectly through EmitCoreOptionsV2.
const std::vector<retro_core_option_v2_definition>& BuildDefinitions();
```

Also add `#include <vector>` at the top, after `#include "libretro.h"`.

Update the file-top comment block to reflect the migration. Replace the existing comment lines from `// Three knobs (smallest valuable cut...)` through `// the FrontendLog dependency on the rest of pcsx2-libretro.` (lines 10–17) with:

```cpp
// SP7c Phase 0 onwards: each category card owns a sibling module
// (CoreOptionsEmulation, CoreOptionsGraphics, ...) that exposes its slice
// of the definitions table plus a Parse helper. This file is now the
// thin aggregator that concatenates them at first-emit time.
//
// Standalone unit-test gate: define CORE_OPTIONS_TEST_ONLY when compiling
// any CoreOptions*.cpp directly into tools/test_core_options.cpp to skip
// the FrontendLog and MemorySettingsInterface dependencies on the rest of
// pcsx2-libretro.
```

- [ ] **Step 2: Implement `Emulation::AppendDefinitions` in `CoreOptionsEmulation.cpp`**

Replace the empty `AppendDefinitions` body with the three definitions previously in `kDefinitions[]`. The block now becomes:

```cpp
void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // Field order per libretro.h:6646-6763:
    //   key, desc, desc_categorized, info, info_categorized, category_key,
    //   values[RETRO_NUM_CORE_OPTION_VALUES_MAX], default_value.
    //
    // NULL for desc_categorized/info_categorized/category_key tells the
    // frontend to display these uncategorized — RetroNest places them
    // under SettingDef.category on the host side.
    out.push_back({
        "pcsx2_renderer",
        "GS Renderer",
        nullptr,
        "PCSX2 graphics backend. Auto picks Metal on macOS. "
        "Software runs on CPU only (much slower; useful for debugging "
        "rendering bugs or for games with hardware-renderer regressions).",
        nullptr,
        nullptr,
        {
            { "auto",     "Auto" },
            { "metal",    "Metal" },
            { "software", "Software" },
            { "null",     "Null" },
            { nullptr,    nullptr },
        },
        "auto",
    });

    out.push_back({
        "pcsx2_mtvu",
        "Multi-Threaded VU1",
        nullptr,
        "Run the VU1 microprogram on its own thread instead of the EE thread. "
        "Compatible with the vast majority of games; significantly reduces "
        "EE-thread saturation on Apple Silicon's interpreter-only path. "
        "Disable only if a specific game shows MTVU-related glitches.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_fast_boot",
        "Fast Boot",
        nullptr,
        "Skip the PS2 BIOS Sony intro and region-check screen on launch. "
        "Disable if you want to see the BIOS screen (e.g. to verify your "
        "BIOS region or to use the BIOS browser).",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });
}
```

- [ ] **Step 3: Rewrite `CoreOptions.cpp` as the thin aggregator**

Replace the entire contents of `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions.cpp` with:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptions.h"
#include "CoreOptionsEmulation.h"

#ifdef CORE_OPTIONS_TEST_ONLY
#include <cstdarg>
#include <cstdio>
static void FrontendLog(int /*level*/, const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
}
#else
#include "LibretroFrontend.h"
#endif

#include <cstring>

namespace Pcsx2Libretro::CoreOptions
{

const std::vector<retro_core_option_v2_definition>& BuildDefinitions()
{
    // Function-local static — initialized once on first call, lives for the
    // process lifetime, addresses are stable. libretro's SET_CORE_OPTIONS_V2
    // requires the definitions array (and the strings it points at) to
    // remain valid until retro_deinit. The strings are all literal — static
    // by construction. The array storage lives here.
    static const std::vector<retro_core_option_v2_definition> kAll = [] {
        std::vector<retro_core_option_v2_definition> v;
        v.reserve(8);  // tiny pre-reserve; future phases expand this.
        Emulation::AppendDefinitions(v);
        // libretro terminator — must be the final entry per libretro.h:6787.
        v.push_back({
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            {{nullptr, nullptr}},
            nullptr
        });
        return v;
    }();
    return kAll;
}

bool EmitCoreOptionsV2(retro_environment_t cb)
{
    if (!cb) return false;

    // SET_CORE_OPTIONS_V2 wants a retro_core_options_v2 (categories +
    // definitions). categories=nullptr → uncategorized; RetroNest's host
    // adapter places these under SettingDef.category on its side.
    retro_core_options_v2 opts{};
    opts.categories  = nullptr;
    opts.definitions = const_cast<retro_core_option_v2_definition*>(
        BuildDefinitions().data());

    const bool ok = cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts);
    if (!ok) {
        // Per libretro.h:2340, false means the host doesn't support option
        // CATEGORIES — options themselves are still registered and
        // GET_VARIABLE will work. We pass categories=nullptr anyway, so this
        // is purely informational; user values still flow.
        FrontendLog(RETRO_LOG_WARN,
            "[CoreOptions] Host does not support core-option categories "
            "(options are still registered and GET_VARIABLE will work)");
    }
    return ok;
}

Resolved ReadResolved(retro_environment_t cb)
{
    Resolved r{};
    if (!cb) return r;

    Emulation::Parse(cb, r.emulation);

    // Future phases append Graphics::Parse, Audio::Parse, MemoryCards::Parse here.

    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] renderer=%d mtvu=%s fast_boot=%s",
        r.emulation.renderer,
        r.emulation.mtvu ? "on" : "off",
        r.emulation.fast_boot ? "on" : "off");

    return r;
}

} // namespace Pcsx2Libretro::CoreOptions
```

(Note: `Resolved` becomes a nested-struct shape in Task 4 below — at this point `r.emulation` refers to `Emulation::Values`. The `CoreOptions.h` change in Task 4 also adds the nested struct.)

- [ ] **Step 4: Update `Resolved` in `CoreOptions.h` to the nested shape (advance Task 4 a step early to keep the build green)**

Edit `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions.h`. Replace the existing `struct Resolved { int renderer = -1; bool mtvu = true; bool fast_boot = true; };` with:

```cpp
struct Resolved
{
    Pcsx2Libretro::CoreOptions::Emulation::Values emulation{};
    // Future phases append:
    //   Pcsx2Libretro::CoreOptions::Graphics::Values    graphics{};
    //   Pcsx2Libretro::CoreOptions::Audio::Values       audio{};
    //   Pcsx2Libretro::CoreOptions::MemoryCards::Values memory_cards{};
};
```

Add `#include "CoreOptionsEmulation.h"` at the top of `CoreOptions.h` (after `#include "libretro.h"` and `#include <vector>`).

- [ ] **Step 5: Update callers — `Settings.cpp` and `LibretroFrontend.cpp` — to use the nested fields**

These changes are minimal so the build stays green between tasks; the larger Settings.cpp refactor (delegating to `ApplyDefaults`) happens in Task 5.

Edit `pcsx2-libretro/Settings.cpp`:
- Line 221 (currently `const int renderer = options ? options->renderer : -1;`) becomes:
  ```cpp
  const int renderer = options ? options->emulation.renderer : -1;
  ```
- Line 249 (currently `const bool fast_boot = options ? options->fast_boot : true;`) becomes:
  ```cpp
  const bool fast_boot = options ? options->emulation.fast_boot : true;
  ```
- Line 263 (currently `const bool mtvu = options ? options->mtvu : true;`) becomes:
  ```cpp
  const bool mtvu = options ? options->emulation.mtvu : true;
  ```

Edit `pcsx2-libretro/LibretroFrontend.cpp:512`:
- Change `params.fast_boot = resolved.fast_boot;` to:
  ```cpp
  params.fast_boot = resolved.emulation.fast_boot;
  ```

- [ ] **Step 6: Build pcsx2_libretro target**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -10
```

Expected: build success. If a compile error mentions `mtvu` / `fast_boot` / `renderer` not being members of `Resolved`, that's a missed caller — search:
```bash
grep -rn "resolved\.\(renderer\|mtvu\|fast_boot\)" pcsx2-libretro/
grep -rn "options->\(renderer\|mtvu\|fast_boot\)" pcsx2-libretro/
```
Both should return 0 hits. Fix any straggler before continuing.

- [ ] **Step 7: Build the standalone test with the renamed gate macro**

The test currently uses `-DSP7B_TEST_CORE_OPTIONS_ONLY`. Now the gate is `CORE_OPTIONS_TEST_ONLY`. The standalone-compile command becomes:

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
```

If this fails with `Parse` not implemented, that's expected — Task 4 is next. For Step 7 just verify the Case 1–5 + Case 6 structural assertions compile; runtime failure of Cases 1–4 (which exercise `Parse` via `ReadResolved`) is expected at this point.

The test BINARY may not run correctly yet, but the BUILD should succeed.

- [ ] **Step 8: Update the existing `test_core_options.cpp` header comment to reflect the new gate macro**

Edit the test file's top comment block. Change line 12's `SP7B_TEST_CORE_OPTIONS_ONLY` to `CORE_OPTIONS_TEST_ONLY`, and update the example command in lines 7–9 to include `CoreOptionsEmulation.cpp` in the source list. The new header reads:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for Pcsx2Libretro::CoreOptions.
// Not built as part of pcsx2_libretro target — manual compile.
//
//   cd pcsx2-libretro/tools
//   clang++ -std=c++20 -I.. test_core_options.cpp \
//       ../CoreOptions.cpp ../CoreOptionsEmulation.cpp \
//       -DCORE_OPTIONS_TEST_ONLY -o test_core_options
//   ./test_core_options
//
// CORE_OPTIONS_TEST_ONLY gates each CoreOptions*.cpp's FrontendLog and
// MemorySettingsInterface dependencies so the test links without the
// rest of pcsx2-libretro.
```

- [ ] **Step 9: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptions.h pcsx2-libretro/CoreOptions.cpp \
        pcsx2-libretro/CoreOptionsEmulation.cpp \
        pcsx2-libretro/Settings.cpp pcsx2-libretro/LibretroFrontend.cpp \
        pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 0 Task 3: aggregator pattern + Emulation owns 3 SP7b knobs

CoreOptions.cpp becomes a thin aggregator: BuildDefinitions() concatenates
each category module's slice (currently Emulation::AppendDefinitions only)
into a function-local static vector and appends the libretro terminator.
Pointer-stable for process lifetime. EmitCoreOptionsV2 dispatches it via
SET_CORE_OPTIONS_V2; ReadResolved calls Emulation::Parse (impl in next task).

struct Resolved becomes nested: callers in Settings.cpp + LibretroFrontend.cpp
updated to options->emulation.<field>. Standalone-test gate macro renamed
SP7B_TEST_CORE_OPTIONS_ONLY -> CORE_OPTIONS_TEST_ONLY (generic across
category modules).

Emulation::Parse + ApplyDefaults are still stubs — Tasks 4-5.
EOF
)"
```

---

## Task 4: Implement `Emulation::Parse`

**Files:**
- Modify: `pcsx2-libretro/CoreOptionsEmulation.cpp` (fill in `Parse`).
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (update existing Case 1–4 to assert against `Values` instead of `Resolved` directly; verify pass).

- [ ] **Step 1: Implement `Parse` in `CoreOptionsEmulation.cpp`**

Replace the empty `Parse` body:

```cpp
void Parse(retro_environment_t cb, Values& out)
{
    if (!cb) return;

    auto query = [&cb](const char* key) -> const char* {
        retro_variable var{};
        var.key = key;
        if (cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
            return var.value;
        return nullptr;
    };

    if (const char* v = query("pcsx2_renderer")) {
        if      (std::strcmp(v, "auto")     == 0) out.renderer = -1;
        else if (std::strcmp(v, "metal")    == 0) out.renderer = 17;
        else if (std::strcmp(v, "software") == 0) out.renderer = 13;
        else if (std::strcmp(v, "null")     == 0) out.renderer = 11;
        else {
            FrontendLog(RETRO_LOG_WARN,
                "[CoreOptions] Unknown renderer '%s'; defaulting to auto", v);
            out.renderer = -1;
        }
    }

    if (const char* v = query("pcsx2_mtvu"))
        out.mtvu = (std::strcmp(v, "enabled") == 0);

    if (const char* v = query("pcsx2_fast_boot"))
        out.fast_boot = (std::strcmp(v, "enabled") == 0);
}
```

- [ ] **Step 2: Update the existing test cases to reference `r.emulation.<field>`**

Edit `pcsx2-libretro/tools/test_core_options.cpp`. The Case 1–4 blocks currently read `r.renderer`, `r.mtvu`, `r.fast_boot`. Update all three across all four cases to `r.emulation.renderer`, `r.emulation.mtvu`, `r.emulation.fast_boot`.

For example, the Case 1 block:
```cpp
    Resolved r = ReadResolved(&fake_env_cb);
    check_int ("Case 1 renderer",  r.renderer,  17);
    check_bool("Case 1 mtvu",      r.mtvu,      false);
    check_bool("Case 1 fast_boot", r.fast_boot, false);
```
becomes:
```cpp
    Resolved r = ReadResolved(&fake_env_cb);
    check_int ("Case 1 renderer",  r.emulation.renderer,  17);
    check_bool("Case 1 mtvu",      r.emulation.mtvu,      false);
    check_bool("Case 1 fast_boot", r.emulation.fast_boot, false);
```

Apply the same `.emulation.` prefix to Cases 2, 3, and 4 (lines ~115, 126, 137 in the current file). Case 5 (Emit dispatching) doesn't touch Resolved fields — leave it.

Case 6 (structural BuildDefinitions assertion from Task 1) doesn't reference Resolved either — leave it.

- [ ] **Step 3: Compile and run the test**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp \
    ../CoreOptions.cpp ../CoreOptionsEmulation.cpp \
    -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: all cases pass.
```
[PASS] Case 1 renderer: got=17 want=17
[PASS] Case 1 mtvu: got=false want=false
[PASS] Case 1 fast_boot: got=false want=false
... (Cases 2–6, all PASS)
0 failure(s)
```

If any fail, the most likely cause is a missing `.emulation.` prefix; grep:
```bash
grep -n "r\.\(renderer\|mtvu\|fast_boot\)\|r\.emulation" test_core_options.cpp
```

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsEmulation.cpp pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 0 Task 4: implement Emulation::Parse + update test cases

Emulation::Parse pulls pcsx2_renderer / pcsx2_mtvu / pcsx2_fast_boot from
the libretro environment callback, with the same enum-fallback + WARN
behavior as the original SP7b ReadResolved. test_core_options.cpp cases
1-4 now check r.emulation.<field> instead of r.<field>; cases 5-6 unchanged.
All 6 cases PASS standalone.
EOF
)"
```

---

## Task 5: Implement `Emulation::ApplyDefaults` and delegate Settings.cpp's apply sites

**Files:**
- Modify: `pcsx2-libretro/CoreOptionsEmulation.cpp` (fill in `ApplyDefaults`).
- Modify: `pcsx2-libretro/Settings.cpp` (delegate the 3 apply sites to the helper).

- [ ] **Step 1: Implement `ApplyDefaults` in `CoreOptionsEmulation.cpp`**

Replace the empty body inside the existing `#ifndef CORE_OPTIONS_TEST_ONLY` block (the gate stays — `ApplyDefaults` is still excluded from the standalone-test compile). `MemorySettingsInterface` inherits `SetIntValue`/`SetBoolValue` from `SettingsInterface`.

```cpp
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // SP3: Renderer was Null (11) at first to bring up without a display
    // surface; SP3 added Pattern B with a real CAMetalLayer so Auto (-1)
    // works. Supported per pcsx2/Config.h:271-281:
    //   Auto = -1, Null = 11, SW = 13, Metal = 17.
    si.SetIntValue("EmuCore/GS", "Renderer", v.renderer);

    // Fast boot — also wired in LibretroFrontend.cpp via
    // VMBootParameters.fast_boot, which overrides this INI at boot time.
    // Both layers MUST get the same value.
    si.SetBoolValue("EmuCore", "EnableFastBoot", v.fast_boot);

    // Multi-Threaded VU1 — default on (SP5 perf rationale: Apple Silicon
    // interpreters saturate the EE thread otherwise). Disable only for
    // games with documented MTVU glitches.
    si.SetBoolValue("EmuCore/Speedhacks", "vuThread", v.mtvu);
}
```

- [ ] **Step 2: Delegate in `Settings.cpp`**

Edit `pcsx2-libretro/Settings.cpp`. Three changes:

First, add the include at the top of the file (find the existing `#include "CoreOptions.h"` and add the new line after it):

```cpp
#include "CoreOptionsEmulation.h"
```

Then replace the 3 apply blocks (currently using the per-field reads via `options->emulation.*`) with a single delegated call. The current blocks span:
- Lines ~213–222: renderer block (comment + `const int renderer = ...; g_si.SetIntValue(...)`).
- Lines ~243–250: fast_boot block (comment + `const bool fast_boot = ...; g_si.SetBoolValue(...)`).
- Lines ~254–264: mtvu block (comment + `const bool mtvu = ...; g_si.SetBoolValue(...)`).

Replace each of the three blocks with the inline-only stub described below, then a single new block at the end of the per-knob writes that delegates to the helper:

For the renderer block (lines ~213–222), delete the entire SP3/SP7b comment and the two-line read/write, leaving the SPU2 backend lines that follow it untouched.

For the fast_boot block, delete the comment + read + write similarly.

For the mtvu block, delete the comment + read + write similarly.

After the existing logging settings (around line 274, after `g_si.SetBoolValue("Logging", "EnableVerbose", false);`), add the delegation block. Find the existing line:
```cpp
    g_si.SetBoolValue("Logging", "EnableVerbose", false);
```
and immediately after it, insert:

```cpp

    // SP7c Phase 0: delegate per-category override-application to each
    // category module. Defaults written above by VMManager::SetDefaultSettings;
    // anything below this point either overrides those defaults with
    // libretro-host-required values (Folders, Backend, etc.) or applies the
    // user's core-option choices on top.
    {
        // SP7b/SP7c: renderer / MTVU / fast_boot live in the Emulation card.
        // When options is null, default Values{} writes the SP7a-era hardcoded
        // defaults — preserves pre-SP7b behavior for any caller that omits options.
        const CoreOptions::Emulation::Values defaults{};
        CoreOptions::Emulation::ApplyDefaults(
            g_si, options ? options->emulation : defaults);
    }
```

- [ ] **Step 3: Verify Settings.cpp still has all three INI writes via the new path**

Grep for the old direct-write keys to confirm they no longer appear in Settings.cpp:

```bash
grep -n "EmuCore/GS.*Renderer\|EmuCore.*EnableFastBoot\|EmuCore/Speedhacks.*vuThread" pcsx2-libretro/Settings.cpp
```

Expected: zero hits (those writes now live in `CoreOptionsEmulation.cpp::ApplyDefaults`).

Then verify they DO appear in `CoreOptionsEmulation.cpp`:

```bash
grep -n "EmuCore/GS.*Renderer\|EmuCore.*EnableFastBoot\|EmuCore/Speedhacks.*vuThread" pcsx2-libretro/CoreOptionsEmulation.cpp
```

Expected: 3 hits — one per knob.

- [ ] **Step 4: Build pcsx2_libretro**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -10
```

Expected: success.

- [ ] **Step 5: Lipo the universal dylib and copy to RetroNest's cores dir for smoke**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
arch -x86_64 /usr/local/bin/cmake --build build-x86_64 --target pcsx2_libretro 2>&1 | tail -5
lipo -create -output ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib \
    build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
    build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib
```

(Note: the resources sibling-dir rsync from the upstream-update process docs isn't needed unless `pcsx2-libretro/bin/resources/` has changed since the last sync.)

- [ ] **Step 6: Commit**

```bash
git add pcsx2-libretro/CoreOptionsEmulation.cpp pcsx2-libretro/Settings.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 0 Task 5: Emulation::ApplyDefaults owns the 3 INI writes

Renderer / EnableFastBoot / vuThread writes move from inline blocks in
Settings::InitializeDefaults to CoreOptionsEmulation::ApplyDefaults.
InitializeDefaults calls ApplyDefaults once after the libretro-required
overrides (Folders, SPU2 backend, Achievements), passing either the
user's options->emulation values or a default-constructed Values{} (which
reproduces the SP7a-era hardcoded defaults).
EOF
)"
```

---

## Task 6: Write `check_schema_fidelity.py`

**Files:**
- Create: `pcsx2-libretro/tools/check_schema_fidelity.py`

- [ ] **Step 1: Create the script**

Create `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py`:

```python
#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
# SPDX-License-Identifier: GPL-3.0+
"""
Schema-fidelity check between pcsx2-libretro's CoreOptions*.cpp and
RetroNest-Project's Pcsx2LibretroAdapter::settingsSchema().

Why this exists:
  - libretro's host (RetroNest) reconciles the user's stored options.json
    against the core's declared values list at retro_set_environment time.
    Any (key, value) pair in the host adapter that doesn't match the core's
    declared values gets silently dropped. That = silent loss of user
    settings. This script catches drift before it reaches options.json.

Usage:
  check_schema_fidelity.py
    --core <glob to CoreOptions*.cpp under pcsx2-libretro/>
    --host <path to pcsx2_libretro_adapter.cpp under RetroNest-Project/>

Exit 0 on full match; exit 1 with a diff report on any drift.
"""
import argparse
import glob
import re
import sys
from pathlib import Path


# Match an out.push_back({ ... key ... values{...} ... }) block.
# We're greedy on the top-level structure but parse the string literals
# inside. The values list is wrapped in {{...}, ...}.
CORE_BLOCK_RE = re.compile(
    r'out\.push_back\(\{\s*'
    r'"(?P<key>[^"]+)"\s*,\s*'        # key
    r'"[^"]*"\s*,\s*'                  # desc
    r'nullptr\s*,\s*'                  # desc_categorized
    r'(?:"[^"]*"\s*)+?,\s*'            # info (may span lines as adjacent string literals)
    r'nullptr\s*,\s*'                  # info_categorized
    r'nullptr\s*,\s*'                  # category_key
    r'\{(?P<values>.*?)\}\s*,\s*'      # values { {a,b}, {c,d}, ... }
    r'"(?P<default>[^"]+)"\s*,?\s*'    # default_value
    r'\}\)',
    re.DOTALL,
)

# Match each {"stored_value", "Display"} pair inside the values block.
# The terminator pair is {nullptr, nullptr} — we skip those.
VALUE_PAIR_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*"[^"]*"\s*\}')

# Host-side: s.append(opt(...)) with positional args. The current
# pcsx2_libretro_adapter.cpp uses a helper:
#   s.append(opt("pcsx2_renderer", "GS Renderer", "auto",
#                {{"Auto", "auto"}, {"Metal", "metal"}, ...},
#                "tooltip..."));
# We pull the key, the default, and the {label, stored_value} pairs from the
# initializer list (pairs in host are (label, value) — opposite order from
# core's (value, label)).
HOST_BLOCK_RE = re.compile(
    r's\.append\(\s*opt\(\s*'
    r'"(?P<key>[^"]+)"\s*,\s*'         # key
    r'"[^"]*"\s*,\s*'                  # label
    r'"(?P<default>[^"]+)"\s*,\s*'     # default value
    r'\{(?P<values>.*?)\}\s*,\s*'      # values list {{"Label", "value"}, ...}
    r'(?:"[^"]*"\s*)+'                 # tooltip (one or more adjacent string literals)
    r'\)\s*\)',
    re.DOTALL,
)

HOST_PAIR_RE = re.compile(r'\{\s*"[^"]*"\s*,\s*"([^"]+)"\s*\}')


def parse_core(paths):
    """Return {key: {"default": str, "values": set[str]}} from all matched .cpp files."""
    found = {}
    for path in paths:
        text = Path(path).read_text()
        for m in CORE_BLOCK_RE.finditer(text):
            key = m.group("key")
            default = m.group("default")
            values = {v for v in VALUE_PAIR_RE.findall(m.group("values"))}
            if key in found:
                print(f"ERROR: duplicate core key '{key}' in {path}", file=sys.stderr)
                sys.exit(1)
            found[key] = {"default": default, "values": values, "source": path}
    return found


def parse_host(path):
    """Return {key: {"default": str, "values": set[str]}}."""
    found = {}
    text = Path(path).read_text()
    for m in HOST_BLOCK_RE.finditer(text):
        key = m.group("key")
        default = m.group("default")
        values = {v for v in HOST_PAIR_RE.findall(m.group("values"))}
        # Multiple host rows can reference the same core key (Recommended
        # re-displays detailed-card rows). Merge values; flag mismatch on default.
        if key in found:
            if found[key]["default"] != default:
                print(
                    f"ERROR: host has two rows for key '{key}' with different defaults: "
                    f"'{found[key]['default']}' vs '{default}'",
                    file=sys.stderr,
                )
                sys.exit(1)
            found[key]["values"] |= values
        else:
            found[key] = {"default": default, "values": values}
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--core", required=True,
                    help="glob for core CoreOptions*.cpp files")
    ap.add_argument("--host", required=True,
                    help="path to host pcsx2_libretro_adapter.cpp")
    args = ap.parse_args()

    core_paths = sorted(glob.glob(args.core))
    if not core_paths:
        print(f"ERROR: --core glob '{args.core}' matched no files", file=sys.stderr)
        return 1

    core = parse_core(core_paths)
    host = parse_host(args.host)

    if not core:
        print(f"ERROR: parsed 0 core options from {core_paths}", file=sys.stderr)
        return 1
    if not host:
        print(f"ERROR: parsed 0 host SettingDef rows from {args.host}", file=sys.stderr)
        return 1

    drift = []

    # Every host key must exist in core.
    for hkey, hrow in host.items():
        if hkey not in core:
            drift.append(f"host declares key '{hkey}' not present in core")
            continue
        crow = core[hkey]
        # Default must match.
        if hrow["default"] != crow["default"]:
            drift.append(
                f"key '{hkey}': default differs — host='{hrow['default']}' core='{crow['default']}'"
            )
        # Every host value must be in core's values list.
        missing = hrow["values"] - crow["values"]
        if missing:
            drift.append(
                f"key '{hkey}': host has values not declared in core: {sorted(missing)}"
            )
        # Every core value must be exposed by at least one host row.
        # (We only check at the per-key level — a single host row covers it.)
        unexposed = crow["values"] - hrow["values"]
        if unexposed:
            drift.append(
                f"key '{hkey}': core declares values not exposed in host: {sorted(unexposed)}"
            )

    # Every core key must appear in host (some host row, anywhere, references it).
    for ckey in core:
        if ckey not in host:
            drift.append(f"core declares key '{ckey}' with no host row")

    if drift:
        print("SCHEMA DRIFT DETECTED:", file=sys.stderr)
        for line in drift:
            print(f"  - {line}", file=sys.stderr)
        print(f"\n{len(drift)} drift entries; check both sides match exactly.", file=sys.stderr)
        return 1

    print(f"Schema fidelity OK: {len(core)} core keys, "
          f"{len(host)} host keys, byte-for-byte match.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: `chmod +x` the script**

```bash
chmod +x /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py
```

- [ ] **Step 3: Run it against the current 3-knob state — expect PASS**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
./pcsx2-libretro/tools/check_schema_fidelity.py \
    --core "pcsx2-libretro/CoreOptions*.cpp" \
    --host /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```

Expected output:
```
Schema fidelity OK: 3 core keys, 3 host keys, byte-for-byte match.
```

If you get drift entries, the most likely cause is regex mismatch on a value list that spans an unusual number of lines. Debug by adding `print(text[m.start():m.end()][:200])` after each `for m in CORE_BLOCK_RE.finditer(text)` line.

- [ ] **Step 4: Confirm drift is detected — simulate a one-character change in the host**

```bash
cp /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp /tmp/pcsx2_libretro_adapter.cpp.bak
# Change "metal" to "metalx" in the host file:
sed -i.bak 's/{"Metal", "metal"}/{"Metal", "metalx"}/' /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp

./pcsx2-libretro/tools/check_schema_fidelity.py \
    --core "pcsx2-libretro/CoreOptions*.cpp" \
    --host /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp

# Expect: exit 1 with a drift entry like:
#   key 'pcsx2_renderer': host has values not declared in core: ['metalx']
#   key 'pcsx2_renderer': core declares values not exposed in host: ['metal']

# Revert:
cp /tmp/pcsx2_libretro_adapter.cpp.bak /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
rm /tmp/pcsx2_libretro_adapter.cpp.bak
rm /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp.bak

# Confirm the host is back:
./pcsx2-libretro/tools/check_schema_fidelity.py \
    --core "pcsx2-libretro/CoreOptions*.cpp" \
    --host /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
# Expect: Schema fidelity OK.
```

If the post-revert check doesn't say OK, the sed-revert went wrong — restore manually from git.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/tools/check_schema_fidelity.py
git commit -m "$(cat <<'EOF'
SP7c Phase 0 Task 6: schema-fidelity Python check script

Compares core CoreOptions*.cpp string literals against RetroNest-Project's
Pcsx2LibretroAdapter::settingsSchema() string literals. Asserts every
(key, default, values[]) tuple matches on both sides. Detects: missing
keys on either side, default mismatches, value-list drift, duplicate host
rows with conflicting defaults.

Smoke-verified live on the current 3-knob state (passes); injected drift
(metal -> metalx) correctly flagged as failure.

Manual run:
  ./pcsx2-libretro/tools/check_schema_fidelity.py \\
      --core "pcsx2-libretro/CoreOptions*.cpp" \\
      --host /path/to/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp

CMake wiring follows in Task 7.
EOF
)"
```

---

## Task 7: Wire `check_schema_fidelity` into CMake

**Files:**
- Modify: `pcsx2-libretro/CMakeLists.txt`

- [ ] **Step 1: Add the custom target**

Edit `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CMakeLists.txt`. After the `install(TARGETS pcsx2_libretro ...)` block at the bottom (around line 47–51), add:

```cmake

# SP7c Phase 0: schema-fidelity check between this core's CoreOptions*.cpp
# and RetroNest-Project's Pcsx2LibretroAdapter::settingsSchema().
#
# Invoked via:    cmake --build build-arm64 --target check_schema_fidelity
# (or)            make -C build-arm64 check_schema_fidelity
#
# The script's --host path defaults to the user's known-good location; pass
# -DRETRONEST_PCSX2_LIBRETRO_ADAPTER=<absolute path> at configure time to
# override (e.g. for CI runners).
set(RETRONEST_PCSX2_LIBRETRO_ADAPTER
    "$ENV{HOME}/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp"
    CACHE PATH "Host-side libretro adapter for schema-fidelity check"
)

add_custom_target(check_schema_fidelity
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_schema_fidelity.py
            --core "${CMAKE_CURRENT_SOURCE_DIR}/CoreOptions*.cpp"
            --host "${RETRONEST_PCSX2_LIBRETRO_ADAPTER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking core/host SettingDef schema fidelity"
    VERBATIM
)
```

- [ ] **Step 2: Reconfigure cmake to pick up the new target**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake -B build-arm64 -S . -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF 2>&1 | tail -5
```

Expected: `-- Configuring done` / `-- Generating done` / `-- Build files have been written to: ...`.

- [ ] **Step 3: Run the target**

```bash
cmake --build build-arm64 --target check_schema_fidelity 2>&1 | tail -5
```

Expected: ends with `Schema fidelity OK: 3 core keys, 3 host keys, byte-for-byte match.` and exit 0.

- [ ] **Step 4: Confirm a failed check propagates as a build failure**

Temporarily inject drift again (same metal→metalx sed pattern as Task 6 Step 4), run the cmake target, observe non-zero exit. Revert.

- [ ] **Step 5: Commit**

```bash
git add pcsx2-libretro/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP7c Phase 0 Task 7: CMake target check_schema_fidelity

cmake --build build-arm64 --target check_schema_fidelity runs the Python
diff script with the right --core / --host args. Host path defaults to
$HOME/Documents/Projects/RetroNest-Project/... — override via
-DRETRONEST_PCSX2_LIBRETRO_ADAPTER=<path> at configure time for CI.

Target fails the build on drift; passes silently with the OK summary line.
EOF
)"
```

---

## Task 8: Final structural test pass

**Files:**
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (add data-driven structural checks).

The existing Case 6 only checks count + key names. Add structural assertions that scale: every definition's `default_value` is in its `values[]`, no duplicate keys, terminator is at the end. These shake out class-wide bugs that would otherwise only surface much later in Phase 4.

- [ ] **Step 1: Add Case 7 — every definition is internally consistent**

After Case 6 (the last block before the trailing failure-count printf), add:

```cpp
    // -------- Case 7: structural sanity for every definition --------
    //
    // For every non-terminator definition:
    //   - key is non-NULL
    //   - desc is non-NULL
    //   - default_value is non-NULL and appears in values[]
    //   - values[] has at least one non-terminator entry
    //   - values[] is itself terminated by {nullptr, nullptr}
    //
    // This catches accidental missing fields when adding new options.
    {
        const auto& defs = BuildDefinitions();
        std::map<std::string, int> key_counts;

        for (size_t i = 0; i + 1 < defs.size(); ++i) {
            const auto& d = defs[i];
            check_bool("Case 7 key non-null",   d.key != nullptr, true);
            check_bool("Case 7 desc non-null",  d.desc != nullptr, true);
            check_bool("Case 7 default non-null", d.default_value != nullptr, true);

            // Look up default in values[].
            bool default_in_values = false;
            int values_count = 0;
            for (const auto& vp : d.values) {
                if (vp.value == nullptr) break;
                ++values_count;
                if (d.default_value && std::strcmp(vp.value, d.default_value) == 0)
                    default_in_values = true;
            }
            check_bool("Case 7 values has at least 1 entry", values_count >= 1, true);
            check_bool("Case 7 default appears in values", default_in_values, true);

            if (d.key)
                ++key_counts[d.key];
        }

        // No duplicate keys.
        for (const auto& [k, c] : key_counts) {
            check_int(("Case 7 unique key " + k).c_str(), c, 1);
        }

        // The last entry must be the terminator.
        if (!defs.empty()) {
            const auto& last = defs.back();
            check_bool("Case 7 last entry is terminator", last.key == nullptr, true);
        }
    }
```

- [ ] **Step 2: Compile and run the test**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp \
    ../CoreOptions.cpp ../CoreOptionsEmulation.cpp \
    -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: all Cases 1–7 pass with `0 failure(s)` final line.

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 0 Task 8: data-driven structural test pass for BuildDefinitions

Case 7 loops over every BuildDefinitions() entry and asserts: key non-null,
desc non-null, default_value non-null and present in values[], values[]
has >=1 entry, no duplicate keys across the table, terminator at the end.

These shake out class-wide bugs (e.g. accidentally adding a Phase-4
definition where default_value doesn't appear in values[]) before live
smoke. Cheap up-front investment for catching regressions across the
~90 entries that future phases add.
EOF
)"
```

---

## Task 9: Live smoke + Phase 0 close-out

**Files:** none modified in this task — verification only.

- [ ] **Step 1: Final build — universal dylib + copy**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -5
arch -x86_64 /usr/local/bin/cmake --build build-x86_64 --target pcsx2_libretro 2>&1 | tail -5
lipo -create -output ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib \
    build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
    build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib

# Verify the lipo'd dylib has both architectures:
lipo -info ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
# Expect: Architectures in the fat file: ... are: x86_64 arm64
```

- [ ] **Step 2: Run schema-fidelity check one more time**

```bash
cmake --build build-arm64 --target check_schema_fidelity 2>&1 | tail -3
```

Expected: `Schema fidelity OK: 3 core keys, 3 host keys, byte-for-byte match.`

- [ ] **Step 3: Run test_core_options.cpp standalone**

```bash
cd pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp \
    ../CoreOptions.cpp ../CoreOptionsEmulation.cpp \
    -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options 2>&1 | tail -10
```

Expected: `0 failure(s)`.

- [ ] **Step 4: Live smoke on R&C 2 — verify SP7b behavior reproduces**

Launch RetroNest with stderr captured:
```bash
arch -x86_64 /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest 2>&1 | tee /tmp/sp7c-phase0-smoke.log
```

Manually inside the running app:
1. Launch R&C 2 (auto-routes to pcsx2-libretro).
2. Confirm the game boots and plays normally (≥30s of in-game play).
3. From the in-game menu, exit to home.
4. Open R&C 2's per-emulator settings → PCSX2 (libretro) → Recommended. Confirm exactly **3 rows** still visible (GS Renderer / Multi-Threaded VU1 / Fast Boot). No new rows.
5. Toggle Fast Boot to Disabled. Re-launch R&C 2.
6. Confirm the BIOS Sony intro + region-check screen appears (Fast Boot off effect).
7. Toggle Fast Boot back to Enabled. Re-launch. Confirm intro is skipped.

In `/tmp/sp7c-phase0-smoke.log`, grep for the SP7b log lines:
```bash
grep "\[CoreOptions\]" /tmp/sp7c-phase0-smoke.log
```
Expected: at least two `[CoreOptions] renderer=-1 mtvu=on fast_boot=on` lines (one per launch) plus one `[CoreOptions] renderer=-1 mtvu=on fast_boot=off` line (from the Fast Boot toggle launch).

If any of these checks fail, debug before declaring Phase 0 complete — Phase 0's whole point is "byte-identical to SP7b".

- [ ] **Step 5: Update memory — Phase 0 shipped note**

Edit (do NOT replace) `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/sp7c_kickoff.md` to add a note at the top of the file (immediately under the frontmatter `---`) that Phase 0 has shipped and links to the design + plan. Add:

```markdown
## Phase 0 SHIPPED 2026-MM-DD

Aggregator refactor + schema-fidelity script complete. SP7b live-smoke
reproduces byte-identical. Branch `retronest-libretro` HEAD <commit-hash>;
spec at `[[2026-05-13-pcsx2-libretro-sp7c-settings-parity-design]]`; plan
at `docs/superpowers/plans/2026-05-13-pcsx2-libretro-sp7c-phase0-foundation.md`.

Phase 1 (Emulation card — 14 new knobs) is next. See spec §"Phase plan".
```

Replace `2026-MM-DD` with the actual date the smoke passed, and `<commit-hash>` with `git -C /Users/mark/Documents/Projects/pcsx2-libretro log -1 --format=%h`.

(Memory edits are file-only — no commit needed.)

- [ ] **Step 6: Push the branch**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git log --oneline retronest-libretro -7
# Expect to see Tasks 2-8 commits + the pre-existing SP7b polish at the bottom.
git push origin retronest-libretro
```

Expected: pushes ~7 new commits cleanly.

- [ ] **Step 7: Phase 0 complete. No new code commit; this task is verification + memory + push only.**

---

## Self-review notes (already incorporated)

This plan was self-reviewed against the SP7c design spec before commit:

- **Spec coverage:** Phase 0's scope (refactor + schema-fidelity script) maps to spec §"Phase plan" → Phase 0 entry. The 5 sub-tasks listed in the spec (migrate, refactor aggregator, nest Resolved, write script, rewrite test_core_options) are covered by Tasks 2–8 here.
- **Placeholder scan:** no TBD / TODO / placeholder strings; every step has full code or full command.
- **Type consistency:** `Pcsx2Libretro::CoreOptions::Emulation::Values`, `AppendDefinitions`, `Parse`, `ApplyDefaults` used consistently. `BuildDefinitions()` referenced by the same name throughout.
- **Out of scope held firm:** no host-adapter changes, no new core options.

---

## What follows Phase 0

After Phase 0 ships, **each subsequent phase gets its own plan** in its own brainstorm-free session (the spec already brainstormed scope per-card). Recommended sequence:

1. **Phase 1 plan** — Emulation card (14 new knobs across Speed Control / System Settings / Frame Pacing).
2. **Phase 2 plan** — Audio card (~6 verified knobs after the verify-during-implementation drop).
3. **Phase 3 plan** — Memory Cards (5 knobs).
4. **Phase 4 plan** — Graphics card (62 knobs across 5 sub-tabs — likely the longest single plan).
5. **Phase 5 plan** — Recommended card (host-only, 14 cross-refs).
6. **Phase 6 plan** — Finalization, full live smoke, session handoff memory.

Each subsequent plan uses Phase 0's pattern as the template:
- One TDD task per knob group (or per logical commit).
- `kXxxDefinitions` array + `Parse` + `ApplyDefaults` per category.
- Test cases extend `test_core_options.cpp` data-driven.
- `check_schema_fidelity` runs at every commit.
- Per-phase live smoke gate.
