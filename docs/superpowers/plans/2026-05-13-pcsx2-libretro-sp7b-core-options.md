# SP7b — Core Options Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose three hardcoded values from `pcsx2-libretro/Settings.cpp` (GS renderer, MTVU, FastBoot) as libretro core options the user edits from RetroNest's per-emulator settings dialog.

**Architecture:** New `CoreOptions` module on the core side declares options via `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2` in `retro_set_environment`, then reads user values via `GET_VARIABLE` once at the top of `retro_load_game` and feeds them into `Settings::InitializeDefaults` + `VMBootParameters.fast_boot`. New `settingsSchema()` override on `Pcsx2LibretroAdapter` declares matching `SettingDef::Storage::LibretroOption` rows so the existing host-side dialog renders them. All host infrastructure (`OptionsStore`, `environment_callbacks.cpp` dispatch, `generic_settings_page.cpp` rendering) is already in place; SP7b is purely additive.

**Tech Stack:** C++20, libretro core API, PCSX2 internals (`MemorySettingsInterface`, `VMBootParameters`), Qt 6 / RetroNest C++ host, CMake, libretro core options v2 (`libretro.h` enum 67).

**Spec:** `docs/superpowers/specs/2026-05-13-pcsx2-libretro-sp7b-core-options-design.md`

**Branch state:** `retronest-libretro` HEAD = `eece07804` (SP7a shipped). Working tree clean modulo two pre-existing untracked tools dirs (`pcsx2-libretro/tools/resources` symlink, `pcsx2-libretro/tools/test_region_prefix` binary, `pcsx2-libretro/tools/test_rcheevos_hash`). Don't touch those.

---

## File map

### Core repo: `/Users/mark/Documents/Projects/pcsx2-libretro/`

| Path | Action | Responsibility |
|---|---|---|
| `pcsx2-libretro/CoreOptions.h` | CREATE | `Resolved` POD struct + `EmitCoreOptionsV2()` + `ReadResolved()` decls. No PCSX2 deps. |
| `pcsx2-libretro/CoreOptions.cpp` | CREATE | Definitions table, emit impl, GET_VARIABLE parse impl, single info log line on resolve. |
| `pcsx2-libretro/tools/test_core_options.cpp` | CREATE | Standalone test (gate `-DSP7B_TEST_CORE_OPTIONS_ONLY`) — 5 cases. |
| `pcsx2-libretro/CMakeLists.txt` | MODIFY (+1 line) | Register `CoreOptions.cpp` in `target_sources`. |
| `pcsx2-libretro/Settings.h` | MODIFY (+5/-1) | Forward-decl `Pcsx2Libretro::CoreOptions::Resolved`, extend `InitializeDefaults` signature. |
| `pcsx2-libretro/Settings.cpp` | MODIFY (+15/-3) | Replace three hardcoded `g_si.Set*Value` writes with conditional reads from `Resolved`. |
| `pcsx2-libretro/LibretroFrontend.cpp` | MODIFY (+8/-1) | Call `EmitCoreOptionsV2` in `retro_set_environment`; call `ReadResolved` in `retro_load_game`; replace hardcoded `params.fast_boot = true`. |

### Host repo: `/Users/mark/Documents/Projects/RetroNest-Project/`

| Path | Action | Responsibility |
|---|---|---|
| `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h` | MODIFY (+1 line) | Declare `settingsSchema() const override`. |
| `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` | MODIFY (+~45 LOC) | Implement `settingsSchema()` with three `LibretroOption`-storage rows. |

---

## Task overview

1. **Task 1** — Scaffold `CoreOptions.{h,cpp}` with stub bodies; register in CMake; confirm core still builds.
2. **Task 2** — TDD `ReadResolved` happy path (all values returned by env_cb).
3. **Task 3** — TDD `ReadResolved` edge cases (NULL key, unknown renderer enum, defaults).
4. **Task 4** — TDD `EmitCoreOptionsV2` (definitions table + emit dispatch).
5. **Task 5** — Wire `Settings.cpp` to accept and apply `Resolved`.
6. **Task 6** — Wire `LibretroFrontend.cpp` (emit + read + boot params).
7. **Task 7** — Build universal core dylib + install to RetroNest.
8. **Task 8** — Add `Pcsx2LibretroAdapter::settingsSchema()` host-side override.
9. **Task 9** — Build universal RetroNest.app + verify host unit tests green.
10. **Task 10** — Live smoke on R&C 2 (NTSC).
11. **Task 11** — Live smoke on DBZ Budokai Tenkaichi 2 (PAL).
12. **Task 12** — Update auto-memory with SP7b-shipped status.

---

## Task 1: Scaffold CoreOptions module

**Files:**
- Create: `pcsx2-libretro/CoreOptions.h`
- Create: `pcsx2-libretro/CoreOptions.cpp`
- Modify: `pcsx2-libretro/CMakeLists.txt` (after line 19, add `CoreOptions.cpp` to `target_sources`)

- [ ] **Step 1: Create `CoreOptions.h`**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7b: Libretro core options for pcsx2-libretro.
//
// Declares the option schema emitted via SET_CORE_OPTIONS_V2 at
// retro_set_environment time, and the typed result of reading the
// host's stored user values at retro_load_game time.
//
// Three knobs (smallest valuable cut from the SP7b spec):
//   * pcsx2_renderer  — GS renderer (auto/metal/software/null)
//   * pcsx2_mtvu      — Multi-Threaded VU1
//   * pcsx2_fast_boot — Fast boot (skip PS2 BIOS intro/region screen)
//
// Standalone unit-test gate: define SP7B_TEST_CORE_OPTIONS_ONLY when
// compiling this .cpp directly into tools/test_core_options.cpp to skip
// the FrontendLog dependency on the rest of pcsx2-libretro.

#pragma once

// libretro.h is a single-header public API with no PCSX2 deps — safe to
// include from the header. The test binary picks up the same header via
// the -I flag in the test's compile command.
#include "libretro.h"

namespace Pcsx2Libretro::CoreOptions
{

// Resolved values to feed into Settings.cpp / VMBootParameters.
// Defaults match the SP7a-era hardcoded behavior so an old/missing
// options.json or a host that doesn't support SET_CORE_OPTIONS_V2
// produces identical results to today.
struct Resolved
{
    int  renderer  = -1;    // GSRendererType integer: -1=Auto, 17=Metal, 13=SW, 11=Null
    bool mtvu      = true;  // EmuCore/Speedhacks/vuThread
    bool fast_boot = true;  // EmuCore/EnableFastBoot AND VMBootParameters.fast_boot
};

// Emit the option schema to the host. Call once from retro_set_environment
// after stashing the env_cb pointer. Returns false if the host doesn't
// support SET_CORE_OPTIONS_V2 (logged once, not fatal — defaults still apply).
bool EmitCoreOptionsV2(retro_environment_t cb);

// Query the host for current user values. Call once at the top of
// retro_load_game (after BIOS resolution, before Settings::InitializeDefaults).
// NULL returns / unknown enum strings fall back to Resolved's defaults
// with a WARN logged. The fact of reading + the resolved triple are
// logged at INFO once per call.
Resolved ReadResolved(retro_environment_t cb);

} // namespace Pcsx2Libretro::CoreOptions
```

- [ ] **Step 2: Create `CoreOptions.cpp` with stub bodies**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptions.h"

#ifdef SP7B_TEST_CORE_OPTIONS_ONLY
// Standalone test mode: stub FrontendLog so this compiles without
// the rest of pcsx2-libretro. The retro_log_level enum still comes
// from libretro.h (included via CoreOptions.h).
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
#include "LibretroFrontend.h"  // FrontendLog
#endif

#include <cstring>

namespace Pcsx2Libretro::CoreOptions
{

bool EmitCoreOptionsV2(retro_environment_t /*cb*/)
{
    // Task 4: real implementation lands here. Stub returns false so
    // callers fall through to compile-time defaults during scaffolding.
    return false;
}

Resolved ReadResolved(retro_environment_t /*cb*/)
{
    // Task 2/3: real implementation lands here. Stub returns built-in
    // defaults so the rest of the system behaves exactly like SP7a.
    return Resolved{};
}

} // namespace Pcsx2Libretro::CoreOptions
```

- [ ] **Step 3: Register `CoreOptions.cpp` in `pcsx2-libretro/CMakeLists.txt`**

Find the `target_sources` block (lines 11-20). Add `CoreOptions.cpp` between `LibretroSaveState.cpp` and `CoreResources.cpp`. The block must look like:

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
    CoreResources.cpp
)
```

- [ ] **Step 4: Build to verify the scaffold compiles**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -20
```

Expected: `[100%] Built target pcsx2_libretro`. If the build-arm64 dir doesn't exist, the executor's prior session set it up — refer to the kickoff memory for cmake configure flags. No code paths actually USE CoreOptions yet, so behavior is unchanged.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptions.h pcsx2-libretro/CoreOptions.cpp pcsx2-libretro/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP7b Task 1: scaffold CoreOptions module

New CoreOptions.{h,cpp} with Resolved struct + EmitCoreOptionsV2 /
ReadResolved decls. Stub bodies return defaults so scaffolding doesn't
change runtime behavior; tasks 2-4 land the real implementations
via TDD against tools/test_core_options.cpp.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: TDD ReadResolved happy path

**Files:**
- Create: `pcsx2-libretro/tools/test_core_options.cpp`
- Modify: `pcsx2-libretro/CoreOptions.cpp` (implement `ReadResolved` happy path)

- [ ] **Step 1: Create the failing test**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for Pcsx2Libretro::CoreOptions.
// Not built as part of pcsx2_libretro target — manual compile.
//
//   cd pcsx2-libretro/tools
//   clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp \
//       -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
//   ./test_core_options
//
// SP7B_TEST_CORE_OPTIONS_ONLY gates CoreOptions.cpp's FrontendLog
// dependency so the test links without the rest of pcsx2-libretro.

#include "../CoreOptions.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using Pcsx2Libretro::CoreOptions::ReadResolved;
using Pcsx2Libretro::CoreOptions::EmitCoreOptionsV2;
using Pcsx2Libretro::CoreOptions::Resolved;

// ---------- Fake env_cb plumbing ----------
//
// libretro's environ_cb is a free C function pointer; we cannot pass
// captures into it. Use file-scope state and a regular function.

namespace fake {
    std::map<std::string, std::string> variables;  // GET_VARIABLE responses
    bool null_for_key = false;                     // if true, return NULL for variables[key]
    std::string null_key;
    bool emit_seen = false;                        // SET_CORE_OPTIONS_V2 was called
    std::vector<std::string> emitted_keys;         // captured for emit-test
    bool emit_returns = true;                      // what the host returns from SET_CORE_OPTIONS_V2

    void reset() {
        variables.clear();
        null_for_key = false;
        null_key.clear();
        emit_seen = false;
        emitted_keys.clear();
        emit_returns = true;
    }
}

static bool fake_env_cb(unsigned cmd, void* data)
{
    if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE) {
        auto* v = static_cast<retro_variable*>(data);
        if (!v || !v->key) return false;
        if (fake::null_for_key && fake::null_key == v->key) {
            v->value = nullptr;
            return true;
        }
        auto it = fake::variables.find(v->key);
        if (it == fake::variables.end()) {
            v->value = nullptr;
            return false;
        }
        v->value = it->second.c_str();
        return true;
    }
    if (cmd == RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2) {
        fake::emit_seen = true;
        auto* opts = static_cast<retro_core_options_v2*>(data);
        if (opts && opts->definitions) {
            for (auto* d = opts->definitions; d->key != nullptr; ++d)
                fake::emitted_keys.emplace_back(d->key);
        }
        return fake::emit_returns;
    }
    return false;
}

static int failures = 0;
static void check_int(const char* label, int got, int want)
{
    const bool ok = (got == want);
    std::printf("[%s] %s: got=%d want=%d\n", ok ? "PASS" : "FAIL", label, got, want);
    if (!ok) ++failures;
}
static void check_bool(const char* label, bool got, bool want)
{
    const bool ok = (got == want);
    std::printf("[%s] %s: got=%s want=%s\n", ok ? "PASS" : "FAIL", label,
                got ? "true" : "false", want ? "true" : "false");
    if (!ok) ++failures;
}

int main()
{
    // -------- Case 1: happy path — all three keys present, non-default --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "metal";
    fake::variables["pcsx2_mtvu"]      = "disabled";
    fake::variables["pcsx2_fast_boot"] = "disabled";

    Resolved r = ReadResolved(&fake_env_cb);
    check_int ("Case 1 renderer",  r.renderer,  17);
    check_bool("Case 1 mtvu",      r.mtvu,      false);
    check_bool("Case 1 fast_boot", r.fast_boot, false);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Build the test and run — expect FAIL**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp \
    -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
./test_core_options
```

Expected output (stub returns defaults, so all three assertions fail):
```
[FAIL] Case 1 renderer: got=-1 want=17
[FAIL] Case 1 mtvu: got=true want=false
[FAIL] Case 1 fast_boot: got=true want=false

3 failure(s)
```

- [ ] **Step 3: Implement `ReadResolved` to handle the happy path**

Replace the stub body in `pcsx2-libretro/CoreOptions.cpp`:

```cpp
Resolved ReadResolved(retro_environment_t cb)
{
    Resolved r{};  // defaults: renderer=-1, mtvu=true, fast_boot=true
    if (!cb) return r;

    auto query = [&cb](const char* key) -> const char* {
        retro_variable var{};
        var.key = key;
        if (cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
            return var.value;
        return nullptr;
    };

    if (const char* v = query("pcsx2_renderer")) {
        if      (std::strcmp(v, "auto")     == 0) r.renderer = -1;
        else if (std::strcmp(v, "metal")    == 0) r.renderer = 17;
        else if (std::strcmp(v, "software") == 0) r.renderer = 13;
        else if (std::strcmp(v, "null")     == 0) r.renderer = 11;
    }

    if (const char* v = query("pcsx2_mtvu"))
        r.mtvu = (std::strcmp(v, "enabled") == 0);

    if (const char* v = query("pcsx2_fast_boot"))
        r.fast_boot = (std::strcmp(v, "enabled") == 0);

    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] renderer=%d mtvu=%s fast_boot=%s",
        r.renderer, r.mtvu ? "on" : "off", r.fast_boot ? "on" : "off");

    return r;
}
```

- [ ] **Step 4: Rebuild and run test — expect PASS**

```bash
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp \
    -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
./test_core_options
```

Expected:
```
[PASS] Case 1 renderer: got=17 want=17
[PASS] Case 1 mtvu: got=false want=false
[PASS] Case 1 fast_boot: got=false want=false

0 failure(s)
```

(There will also be an `[CoreOptions] renderer=17 mtvu=off fast_boot=off` line on stderr from the impl's INFO log — that's expected, stub still routes through the test-mode FrontendLog stub.)

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptions.cpp pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7b Task 2: TDD ReadResolved happy path

Standalone test_core_options.cpp mirrors test_region_prefix.cpp pattern
(SP7B_TEST_CORE_OPTIONS_ONLY gate, no PCSX2 link). First case: all three
keys present, non-default values, ReadResolved returns the expected
Resolved triple via GET_VARIABLE queries.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: TDD ReadResolved edge cases

**Files:**
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (add cases 2, 3, 4)
- Modify: `pcsx2-libretro/CoreOptions.cpp` (handle unknown enum + log WARN)

- [ ] **Step 1: Add three failing edge-case tests**

Append into `main()` of `pcsx2-libretro/tools/test_core_options.cpp`, between Case 1 and the final printf:

```cpp
    // -------- Case 2: NULL value for one key — that field stays at default --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "software";  // 13
    fake::null_for_key = true;
    fake::null_key = "pcsx2_mtvu";                    // → default true
    fake::variables["pcsx2_fast_boot"] = "disabled";  // → false

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 2 renderer",  r.renderer,  13);
    check_bool("Case 2 mtvu",      r.mtvu,      true);   // default
    check_bool("Case 2 fast_boot", r.fast_boot, false);

    // -------- Case 3: unknown renderer enum → default -1 (Auto) --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "vulkan";        // not in our schema
    fake::variables["pcsx2_mtvu"]      = "enabled";
    fake::variables["pcsx2_fast_boot"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 3 renderer (unknown)", r.renderer, -1);
    check_bool("Case 3 mtvu",               r.mtvu,      true);
    check_bool("Case 3 fast_boot",          r.fast_boot, true);

    // -------- Case 4: all defaults — every key returns its declared default string --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "auto";
    fake::variables["pcsx2_mtvu"]      = "enabled";
    fake::variables["pcsx2_fast_boot"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 4 renderer",  r.renderer,  -1);
    check_bool("Case 4 mtvu",      r.mtvu,      true);
    check_bool("Case 4 fast_boot", r.fast_boot, true);
```

Replace the existing `Resolved r = ReadResolved(...)` in Case 1 with `Resolved r = ReadResolved(...);` (keeping the same `r` variable for reassignment in later cases).

Update the line in Case 1 to use the reassignment form too:
```cpp
    Resolved r = ReadResolved(&fake_env_cb);  // declares — keep as-is in Case 1
```
And in Cases 2, 3, 4: `r = ReadResolved(&fake_env_cb);` (no type — already declared).

- [ ] **Step 2: Rebuild and run — expect Case 3's renderer assertion to FAIL**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp \
    -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
./test_core_options
```

Expected: Cases 1, 2, 4 all PASS. Case 3 renderer FAILS (current impl leaves `r.renderer` at its default through the `else if` chain, which IS `-1`… so actually Case 3 will already pass — verify; if it does, no WARN log was emitted for the unknown value, which the spec requires. Add the WARN log next.)

If Case 3 already passes silently, that proves the *value* fallback works but the spec's "WARN with offending value" hasn't been wired. Verify the implementation emits the WARN — search stderr output. If no `[CoreOptions] Unknown renderer` line is present, the test passes by accident — proceed to step 3 to add the WARN.

- [ ] **Step 3: Add WARN logging for unknown renderer enum**

Modify the renderer parsing block in `pcsx2-libretro/CoreOptions.cpp` to add an explicit else with WARN:

```cpp
    if (const char* v = query("pcsx2_renderer")) {
        if      (std::strcmp(v, "auto")     == 0) r.renderer = -1;
        else if (std::strcmp(v, "metal")    == 0) r.renderer = 17;
        else if (std::strcmp(v, "software") == 0) r.renderer = 13;
        else if (std::strcmp(v, "null")     == 0) r.renderer = 11;
        else {
            FrontendLog(RETRO_LOG_WARN,
                "[CoreOptions] Unknown renderer '%s'; defaulting to auto", v);
            r.renderer = -1;
        }
    }
```

- [ ] **Step 4: Rebuild + run — expect 0 failures AND visible WARN line on stderr**

```bash
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp \
    -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
./test_core_options 2>&1 | grep -E "(PASS|FAIL|Unknown|failure)"
```

Expected: 12 `[PASS]` lines (3 per case × 4 cases) + a `[CoreOptions] Unknown renderer 'vulkan'; defaulting to auto` line + `0 failure(s)`.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptions.cpp pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7b Task 3: TDD ReadResolved edge cases

Adds three cases: NULL value falls back to default, unknown renderer
enum falls back to Auto with WARN, and the all-defaults path returns
Resolved's compile-time defaults exactly.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: TDD EmitCoreOptionsV2

**Files:**
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (add case 5)
- Modify: `pcsx2-libretro/CoreOptions.cpp` (implement `EmitCoreOptionsV2` + the `kDefinitions` table)

- [ ] **Step 1: Add a failing emit test (Case 5)**

Append into `main()` before the final printf:

```cpp
    // -------- Case 5: EmitCoreOptionsV2 dispatches all three keys --------
    fake::reset();
    fake::emit_returns = true;
    const bool emit_ok = EmitCoreOptionsV2(&fake_env_cb);
    check_bool("Case 5 emit returned true", emit_ok, true);
    check_int ("Case 5 emit was seen",
               static_cast<int>(fake::emit_seen ? 1 : 0), 1);
    check_int ("Case 5 emitted 3 keys",
               static_cast<int>(fake::emitted_keys.size()), 3);
    // Order matches the kDefinitions[] table order in CoreOptions.cpp.
    auto str_eq = [](const std::string& a, const char* b) { return a == b; };
    check_bool("Case 5 key 0 = pcsx2_renderer",
               !fake::emitted_keys.empty()
               && str_eq(fake::emitted_keys[0], "pcsx2_renderer"), true);
    check_bool("Case 5 key 1 = pcsx2_mtvu",
               fake::emitted_keys.size() > 1
               && str_eq(fake::emitted_keys[1], "pcsx2_mtvu"), true);
    check_bool("Case 5 key 2 = pcsx2_fast_boot",
               fake::emitted_keys.size() > 2
               && str_eq(fake::emitted_keys[2], "pcsx2_fast_boot"), true);
```

- [ ] **Step 2: Rebuild + run — expect Case 5 to FAIL**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp \
    -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
./test_core_options
```

Expected: Cases 1-4 PASS. Case 5's first assertion fails (`emit_ok` is `false` from the stub) and downstream assertions cascade-fail. ~6 failures total in Case 5.

- [ ] **Step 3: Implement `EmitCoreOptionsV2`**

Replace the stub body in `pcsx2-libretro/CoreOptions.cpp` and add the file-scope `kDefinitions` table just above the `EmitCoreOptionsV2` function. The `info` strings reuse the rationale already in `Settings.cpp`'s inline comments at lines 213-216 (renderer), 244-248 (MTVU), 237 (FastBoot).

```cpp
namespace
{

// Option schema. Field order per libretro.h:6646-6763:
//   key, desc, desc_categorized, info, info_categorized, category_key,
//   values[], default_value.
//
// Terminator: a fully-zeroed entry (the libretro spec requires this).
//
// Note: NULL for desc_categorized/info_categorized/category_key tells
// the frontend to display these options uncategorized — RetroNest places
// them under its own "Recommended" tab via SettingDef.category in the
// host adapter.
const retro_core_option_v2_definition kDefinitions[] = {
    {
        "pcsx2_renderer",
        "GS Renderer",
        nullptr,                          // desc_categorized
        "PCSX2 graphics backend. Auto picks Metal on macOS. "
        "Software runs on CPU only (much slower; useful for debugging "
        "rendering bugs or for games with hardware-renderer regressions).",
        nullptr,                          // info_categorized
        nullptr,                          // category_key
        {
            { "auto",     "Auto" },
            { "metal",    "Metal" },
            { "software", "Software" },
            { "null",     "Null" },
            { nullptr,    nullptr },      // terminator
        },
        "auto",                           // default_value
    },
    {
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
    },
    {
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
    },
    // Terminator — zeroed entry per libretro.h:6787.
    { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{nullptr,nullptr}}, nullptr },
};

const retro_core_options_v2 kCoreOptionsV2 = {
    nullptr,                              // categories — uncategorized
    const_cast<retro_core_option_v2_definition*>(kDefinitions),
};

} // namespace

bool EmitCoreOptionsV2(retro_environment_t cb)
{
    if (!cb) return false;
    const bool ok = cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
                       const_cast<retro_core_options_v2*>(&kCoreOptionsV2));
    if (!ok) {
        FrontendLog(RETRO_LOG_WARN,
            "[CoreOptions] SET_CORE_OPTIONS_V2 not supported by host; "
            "core will use built-in defaults");
    }
    return ok;
}
```

- [ ] **Step 4: Rebuild + run — expect all PASS**

```bash
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp \
    -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
./test_core_options 2>&1 | grep -E "(PASS|FAIL|failure)"
```

Expected: 18 `[PASS]` lines + `0 failure(s)`.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptions.cpp pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7b Task 4: TDD EmitCoreOptionsV2

kDefinitions[] table for the three knobs (renderer / mtvu / fast_boot)
with values and defaults; EmitCoreOptionsV2 dispatches the table to the
host. Test verifies emit was seen, returned ok, and contained all three
keys in declaration order.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Wire Settings.cpp

**Files:**
- Modify: `pcsx2-libretro/Settings.h` (signature change + forward decl)
- Modify: `pcsx2-libretro/Settings.cpp` (apply `Resolved` when provided)

- [ ] **Step 1: Update `Settings.h` to accept an optional `Resolved`**

Replace the declaration in `pcsx2-libretro/Settings.h` (currently lines 30-31) so the file looks like:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <string>

class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }

namespace Pcsx2Libretro::Settings
{

// Populate the underlying MemorySettingsInterface with PCSX2 defaults
// and the SP2-required overrides. Must be called exactly once before
// VMManager::Initialize.
//
// system_dir: libretro system directory (where BIOS lives).
// save_dir:   libretro save directory (where memcards live).
// options:    optional pointer to user-resolved core options. When
//             non-null, overrides the hardcoded defaults for Renderer,
//             vuThread, and EnableFastBoot. When null, the SP7a-era
//             hardcoded defaults are written (defensive fallback).
void InitializeDefaults(const std::string& system_dir,
                        const std::string& save_dir,
                        const CoreOptions::Resolved* options = nullptr);

MemorySettingsInterface* GetActiveInterface();

} // namespace Pcsx2Libretro::Settings
```

- [ ] **Step 2: Modify `Settings.cpp` to apply the resolved values**

Add `#include "CoreOptions.h"` near the existing `#include "CoreResources.h"` (right after it).

Update the function definition signature at line 131 to match the header:
```cpp
void InitializeDefaults(const std::string& system_dir,
                        const std::string& save_dir,
                        const CoreOptions::Resolved* options)
```

Replace the three hardcoded writes with conditional reads from `options`. Find these existing lines:

At `Settings.cpp:216`:
```cpp
g_si.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(-1));
```
Replace with:
```cpp
// SP7b: user-tweakable via core option pcsx2_renderer.
// -1 (Auto) matches PCSX2's GSRendererType::Auto and resolves to Metal
// on macOS via GSUtil::GetPreferredRenderer(). Other supported values
// per pcsx2/Config.h:271-281: Null=11, SW=13, Metal=17.
const int renderer = options ? options->renderer : -1;
g_si.SetIntValue("EmuCore/GS", "Renderer", renderer);
```

At `Settings.cpp:238`:
```cpp
g_si.SetBoolValue("EmuCore", "EnableFastBoot", true);
```
Replace with:
```cpp
// SP7b: user-tweakable via core option pcsx2_fast_boot. Note that
// VMBootParameters.fast_boot in LibretroFrontend.cpp::retro_load_game
// overrides this INI value at runtime, and is wired to the same
// resolved.fast_boot. Both must match for the user's choice to apply.
const bool fast_boot = options ? options->fast_boot : true;
g_si.SetBoolValue("EmuCore", "EnableFastBoot", fast_boot);
```

At `Settings.cpp:248`:
```cpp
g_si.SetBoolValue("EmuCore/Speedhacks", "vuThread", true);
```
Replace with:
```cpp
// SP7b: user-tweakable via core option pcsx2_mtvu. Default on for the
// SP5 perf rationale; only disable if a specific game has MTVU glitches.
const bool mtvu = options ? options->mtvu : true;
g_si.SetBoolValue("EmuCore/Speedhacks", "vuThread", mtvu);
```

- [ ] **Step 3: Build to verify Settings.cpp compiles**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -15
```

Expected: `[100%] Built target pcsx2_libretro`. (`LibretroFrontend.cpp` still calls the old 2-arg form; the optional third argument defaults to nullptr so call sites compile unchanged.)

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/Settings.h pcsx2-libretro/Settings.cpp
git commit -m "$(cat <<'EOF'
SP7b Task 5: thread Resolved through Settings::InitializeDefaults

Optional 3rd parameter; when non-null, the three target writes
(Renderer, EnableFastBoot, vuThread) source from CoreOptions::Resolved
instead of hardcoded literals. Default-null preserves SP7a behavior so
this commit is a no-op at runtime until LibretroFrontend wires the
ReadResolved call in Task 6.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Wire LibretroFrontend.cpp

**Files:**
- Modify: `pcsx2-libretro/LibretroFrontend.cpp` (3 edits)

- [ ] **Step 1: Add CoreOptions include**

Near the existing `#include "CoreResources.h"` (line 20), add:
```cpp
#include "CoreOptions.h"
```

- [ ] **Step 2: Emit options in `retro_set_environment`**

Find line 231:
```cpp
RETRO_API void retro_set_environment(retro_environment_t cb)        { g_frontend.environ_cb     = cb; }
```

Replace with:
```cpp
RETRO_API void retro_set_environment(retro_environment_t cb)
{
    g_frontend.environ_cb = cb;
    // SP7b: declare core options as soon as the env_cb is available.
    // This is the only legal time per the libretro spec (before retro_init).
    CoreOptions::EmitCoreOptionsV2(cb);
}
```

- [ ] **Step 3: Read resolved values in `retro_load_game` and wire them**

In `retro_load_game` (starts at line 452 per the spec), insert the `ReadResolved` call between the BIOS-found log line (around line 487) and the `Pcsx2Libretro::Settings::InitializeDefaults(...)` call (around line 491). The block currently reads:

```cpp
    FrontendLog(RETRO_LOG_INFO, "Found PS2 BIOS: %s", bios_path.c_str());

    // 2. Populate the in-memory settings layer.
    const std::string save_dir = GetSaveDirectory();
    Pcsx2Libretro::Settings::InitializeDefaults(system_dir, save_dir);
```

Replace with:
```cpp
    FrontendLog(RETRO_LOG_INFO, "Found PS2 BIOS: %s", bios_path.c_str());

    // SP7b: read user-tweaked core options once at load time. Renderer,
    // MTVU, and FastBoot all take effect at VM init / boot — none are
    // safe to swap mid-run, so "restart to apply" UX is intentional.
    const auto resolved = CoreOptions::ReadResolved(g_frontend.environ_cb);

    // 2. Populate the in-memory settings layer.
    const std::string save_dir = GetSaveDirectory();
    Pcsx2Libretro::Settings::InitializeDefaults(system_dir, save_dir, &resolved);
```

Then find the hardcoded `params.fast_boot = true;` at line 496:
```cpp
    VMBootParameters params{};
    params.filename = game->path;
    params.fast_boot = true;
```

Replace with:
```cpp
    VMBootParameters params{};
    params.filename = game->path;
    // SP7b: VMBootParameters.fast_boot overrides the INI EnableFastBoot
    // value (PCSX2's VMManager::BootSystem reads this field directly).
    // Wire it to the same resolved.fast_boot the INI write uses, so the
    // user's choice applies at both layers.
    params.fast_boot = resolved.fast_boot;
```

- [ ] **Step 4: Build and verify**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -15
```

Expected: clean build. No new warnings (the `resolved` variable is now consumed at two sites).

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/LibretroFrontend.cpp
git commit -m "$(cat <<'EOF'
SP7b Task 6: wire CoreOptions into retro_set_environment + retro_load_game

retro_set_environment now emits the V2 options table to the host.
retro_load_game reads the resolved triple via GET_VARIABLE before
InitializeDefaults and passes it through; the formerly-hardcoded
params.fast_boot is now sourced from resolved.fast_boot too (the
two-site coupling the spec calls out).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Build universal dylib + install

**Files:** None modified — build orchestration only.

- [ ] **Step 1: Build the arm64 target**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -10
```

Expected: `[100%] Built target pcsx2_libretro`.

- [ ] **Step 2: Build the x86_64 target**

```bash
arch -x86_64 /usr/local/bin/cmake --build build-x86_64 --target pcsx2_libretro 2>&1 | tail -10
```

Expected: `[100%] Built target pcsx2_libretro`.

If `build-x86_64` doesn't exist yet, the executor should run the Rosetta-setup steps from `[[project_pcsx2_libretro_port]]` memory — including the `setup-x86_64-toolchain.sh` deps (sdl3, rapidyaml, plutosvg, plutovg, shaderc). The kickoff calls this out as a known footgun.

- [ ] **Step 3: lipo into the installed dylib**

```bash
lipo -create -output ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib \
    /Users/mark/Documents/Projects/pcsx2-libretro/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
    /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib
```

Verify both architectures are present:
```bash
lipo -archs ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```
Expected: `arm64 x86_64`.

- [ ] **Step 4: (Skipped — no rsync needed)**

SP7a's monthly-update process includes an `rsync` step for `pcsx2_libretro_resources/`. SP7b changes no PCSX2 resources (no metallib, no gamedb, no patches). Skip.

- [ ] **Step 5: Commit (no source changes — build orchestration only)**

Nothing to commit at this task. Build artefacts live in `build-*` directories already ignored by git.

---

## Task 8: Add Pcsx2LibretroAdapter::settingsSchema host-side override

**Files:**
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`

- [ ] **Step 1: Declare the override in the header**

Modify `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`. Add a `settingsSchema()` declaration. The class currently ends just after `findResumeFile`; insert before the closing brace:

```cpp
    // SP7b: declare libretro core options as user-tweakable rows in the
    // per-emulator settings dialog. Three knobs (renderer / MTVU / FastBoot)
    // are exposed; values mirror pcsx2-libretro/CoreOptions.cpp's
    // kDefinitions[] table exactly so OptionsStore::load's whitelist check
    // accepts persisted values.
    QVector<SettingDef> settingsSchema() const override;
```

- [ ] **Step 2: Implement the override**

Append to `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` (the existing file has 64 lines; append at the end):

```cpp
// SP7b: libretro-option-backed rows for the per-emulator settings dialog.
// Pattern mirrors MgbaLibretroAdapter::settingsSchema (same file path
// pattern, sibling adapter). The three keys and their values exactly
// match pcsx2-libretro/CoreOptions.cpp's kDefinitions[] — OptionsStore::load
// reconciles host options.json against the core-declared values list and
// drops any value not on the list, so divergence here silently wipes user
// settings.
QVector<SettingDef> Pcsx2LibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    auto opt = [](const QString& key, const QString& label,
                  const QString& def,
                  const QVector<QPair<QString,QString>>& valuesAndLabels,
                  const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.category = "Recommended";   // PPSSPP/mGBA pattern
        d.subcategory = "";
        d.group = "Emulation";
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.tooltip = tooltip;
        d.type = SettingDef::Combo;
        d.options = valuesAndLabels;  // (value, label) pairs per SettingDef contract
        return d;
    };

    s.append(opt(
        "pcsx2_renderer", "GS Renderer", "auto",
        {{"Auto", "auto"},
         {"Metal", "metal"},
         {"Software", "software"},
         {"Null", "null"}},
        "PCSX2 graphics backend. Auto picks Metal on macOS. Software is "
        "CPU-only and much slower; useful for debugging rendering bugs "
        "or working around hardware-renderer regressions in specific games. "
        "Takes effect on next launch."));

    s.append(opt(
        "pcsx2_mtvu", "Multi-Threaded VU1", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Run the VU1 microprogram on its own thread instead of the EE "
        "thread. Compatible with the vast majority of games and "
        "significantly reduces EE-thread saturation on Apple Silicon. "
        "Disable only if a specific game shows MTVU-related glitches. "
        "Takes effect on next launch."));

    s.append(opt(
        "pcsx2_fast_boot", "Fast Boot", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Skip the PS2 BIOS Sony intro and region-check screen on launch. "
        "Disable if you want to see the BIOS screen (e.g. to verify your "
        "BIOS region or to use the BIOS browser). Takes effect on next launch."));

    return s;
}
```

Note: `SettingDef::options` is `QVector<QPair<QString, QString>>` with the contract `(display label, stored value)`. The mGBA adapter uses `{ v, v }` (label == value) for already-human-readable strings; here we want the labels to differ from the values (`"Auto"` ↔ `"auto"`), so the explicit pair form is used.

- [ ] **Step 3: Build the host**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -15
```

Expected: `[100%] Built target RetroNest`.

- [ ] **Step 4: Run the host unit tests that touch options/env**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64
ctest -R 'test_options_store|test_environment_callbacks' --output-on-failure
```

Expected: both tests PASS. SP7b is purely additive at this layer — no behavioral changes expected.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.h cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
adapters: Pcsx2LibretroAdapter::settingsSchema for SP7b core options

Three LibretroOption-backed rows (renderer / MTVU / FastBoot) under
the "Recommended" tab. Keys and values mirror pcsx2-libretro/
CoreOptions.cpp's kDefinitions[] exactly; OptionsStore::load reconciles
options.json against this list so any divergence silently wipes user
settings.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Build & install universal RetroNest.app

**Files:** None modified.

- [ ] **Step 1: Run the universal build script**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
./scripts/build-universal.sh 2>&1 | tail -20
```

Expected: arm64 + x86_64 RetroNest.app built; lipo merges them; installs into the standard location. The script's tail output should show "Universal build complete" or equivalent. If the script fails on x86_64 deps, the kickoff memory's footgun list (`sdl3 rapidyaml plutosvg plutovg shaderc` missing from `setup-x86_64-toolchain.sh`) is the first thing to check.

- [ ] **Step 2: Verify the installed binary is universal**

```bash
lipo -archs /Applications/RetroNest.app/Contents/MacOS/RetroNest 2>/dev/null \
    || lipo -archs ~/Applications/RetroNest.app/Contents/MacOS/RetroNest
```
Expected: `arm64 x86_64`.

- [ ] **Step 3: No commit needed**

Build artefacts only.

---

## Task 10: Live smoke — R&C 2 (NTSC)

**Files:** None modified. User-driven verification.

This task is a checklist of manual steps the user runs. Each `[ ]` step the user must report on before the task is "completed".

- [ ] **Step 1: Launch RetroNest in Rosetta mode**

```bash
arch -x86_64 /Applications/RetroNest.app/Contents/MacOS/RetroNest 2>&1 | tee /tmp/sp7b-smoke-rc2.log
```

Or if RetroNest installs into `~/Applications/`, adjust the path. Per the project memory the user keeps "Open using Rosetta" toggled ON on `RetroNest.app` already, so a normal launch via Finder also works — but `tee`-capturing stderr from the terminal makes log-checking easier.

- [ ] **Step 2: Open the per-emulator settings dialog for pcsx2-libretro**

Navigate to settings for the pcsx2-libretro core (the path varies by RetroNest UI; typically "Settings → PCSX2 (libretro)"). Verify three rows are visible under "Recommended":

- **GS Renderer** — combo with Auto / Metal / Software / Null. Defaults to Auto.
- **Multi-Threaded VU1** — combo with Enabled / Disabled. Defaults to Enabled.
- **Fast Boot** — combo with Enabled / Disabled. Defaults to Enabled.

Tooltips on hover should show the strings from `Pcsx2LibretroAdapter::settingsSchema`.

- [ ] **Step 3: First launch sanity — all defaults**

Launch R&C 2. Confirm:
- Game boots to the title screen via FastBoot path (no Sony intro, no region-check screen).
- Verify in `/tmp/sp7b-smoke-rc2.log`:
  - `[CoreOptions] renderer=-1 mtvu=on fast_boot=on` (or equivalent — `renderer=-1` means Auto).
  - `[CoreResources] Resources dir = ...` (SP7a regression check).
  - `[Region] region=NTSC fps=59.94 (GameDB 'NTSC-U')` (SP7a regression check).
  - `SET_MEMORY_MAPS captured 2 descriptors` (SP6 regression check).

Stop the game cleanly (Save & Exit). Verify `.resume` written under the expected savestate dir (SP6.5 Task 4.6 regression check).

- [ ] **Step 4: Toggle FastBoot off → relaunch → expect BIOS screen**

In settings, change Fast Boot to Disabled. Save. Launch R&C 2 again.

- The PS2 Sony Computer Entertainment intro should appear, then the system browser briefly, before the game starts (or stays in browser if the game doesn't auto-start — that's expected).
- Log line confirms: `[CoreOptions] renderer=-1 mtvu=on fast_boot=off`.

Restore FastBoot = Enabled before the next step.

- [ ] **Step 5: Toggle MTVU off → relaunch → look for EE-thread saturation**

In settings, change Multi-Threaded VU1 to Disabled. Save. Launch R&C 2 again.

- Game should boot normally (FastBoot=on again).
- Log line: `[CoreOptions] renderer=-1 mtvu=off fast_boot=on`.
- Optional perf observation: in PCSX2 logs or Activity Monitor, EE thread shows higher utilization than with MTVU on. Known ~5-10% perf delta on R&C 2 per SP5 measurements.

Restore MTVU = Enabled.

- [ ] **Step 6: Toggle Renderer to Software → relaunch → expect huge slowdown**

Change GS Renderer to Software. Save. Launch.

- Game renders correctly but at ~10-15 fps during gameplay (CPU rasterization of PS2 geometry is brutally slow).
- Log line: `[CoreOptions] renderer=13 mtvu=on fast_boot=on`.
- Audio should still work (SPU2 is unaffected).

Restore Renderer = Auto.

- [ ] **Step 7: Toggle Renderer to Null → relaunch → black screen + working audio**

Change GS Renderer to Null. Save. Launch.

- Black screen (no GS output) but audio works and game state advances.
- Log line: `[CoreOptions] renderer=11 mtvu=on fast_boot=on`.

Restore Renderer = Auto.

- [ ] **Step 8: Final verification — all back to defaults, full feature regression**

With all three options at defaults, launch R&C 2 one more time. Verify:
- Settings.cpp's existing rcheevos integration still active: `[RAService] Achievement unlocked: "300336" "Cold Opening"` should fire when the player passes the opening cutscene (SP6.5 Task 4.7 regression check). If the player has already unlocked Cold Opening on this account, replay the opening scene from save or pick a different cheevo to verify.
- Save state operations work (in-session save/load — SP6.5 main feature).
- Save & Exit + Resume cycle works (SP6.5 Task 4.5 cold-resume + Task 4.8 EmuThread reuse).

- [ ] **Step 9: No commit needed (manual verification)**

Report findings (PASS/FAIL per sub-step) in the session. If any check fails, do not proceed to Task 11 — investigate.

---

## Task 11: Live smoke — DBZ Budokai Tenkaichi 2 (PAL)

**Files:** None modified. User-driven verification.

- [ ] **Step 1: Route the DBZ disc through the libretro adapter**

```bash
sqlite3 ~/Documents/RetroNest/config/retronest.db \
    "UPDATE games SET emulator_id='pcsx2-libretro' WHERE id=6;"
```

(Per the SP7a session notes, the DBZ row was previously id=6. Verify with `SELECT id, title, emulator_id FROM games WHERE title LIKE '%Tenkaichi%';` first if uncertain.)

- [ ] **Step 2: Launch RetroNest + DBZ TT2 with default options**

```bash
arch -x86_64 /Applications/RetroNest.app/Contents/MacOS/RetroNest 2>&1 | tee /tmp/sp7b-smoke-dbz.log
```

Launch DBZ Budokai Tenkaichi 2. Confirm:
- `[CoreOptions] renderer=-1 mtvu=on fast_boot=on` in the log.
- `[Region] region=PAL fps=50.00 (GameDB 'PAL-M6')` (SP7a regression for PAL).
- Game boots and runs at 50 fps. Try a quick fight to verify gameplay normality.

- [ ] **Step 3: Toggle MTVU off → relaunch → verify takes effect**

Change MTVU to Disabled. Save. Relaunch DBZ. Log shows `mtvu=off`. Game should still play (DBZ is MTVU-tolerant).

Restore MTVU = Enabled.

- [ ] **Step 4: Toggle FastBoot off → relaunch → expect EU BIOS screen**

Change FastBoot to Disabled. Save. Relaunch DBZ. PAL BIOS intro should appear. Log: `fast_boot=off`.

Restore FastBoot = Enabled.

- [ ] **Step 5: Confirm options.json structure**

```bash
cat ~/Documents/RetroNest/config/options.json 2>/dev/null || \
    find ~/Documents/RetroNest -name 'options.json' -exec cat {} \;
```

The pcsx2-libretro options.json should contain the three keys with their last-set values. If RetroNest uses a per-core directory layout (likely), the path will be something like `~/Documents/RetroNest/emulators/libretro/cores/pcsx2-libretro/options.json` or under the adapter's `optionsJsonPath()` — find by recursive search.

Expected content (after the toggle cycle):
```json
{
    "pcsx2_renderer": "auto",
    "pcsx2_mtvu": "enabled",
    "pcsx2_fast_boot": "enabled"
}
```

- [ ] **Step 6: Restore DBZ DB row**

Revert the DB column so DBZ goes back through standalone pcsx2 (the kickoff and SP7a notes confirm this is the default):

```bash
sqlite3 ~/Documents/RetroNest/config/retronest.db \
    "UPDATE games SET emulator_id='pcsx2' WHERE id=6;"
```

- [ ] **Step 7: No commit needed (manual verification)**

Report findings. If all PASS, the implementation is shipped.

---

## Task 12: Update auto-memory with SP7b-shipped status

**Files:** Auto-memory in `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/`.

- [ ] **Step 1: Mark the project memory entry**

Update `project_pcsx2_libretro_port.md`:
- Sub-project 8b ("SP7b — Core-options migration") status changes from `⏳` to `✅`.
- Replace the "REMAINING SP7 scope" body with a SHIPPED summary: the three knobs, the new `CoreOptions` module path, the host-side schema location, the two smoke-verified games.
- Commit-hash range: `git log --oneline eece07804..HEAD` from the pcsx2-libretro repo gives the SP7b commit range.

- [ ] **Step 2: Write a new session_handoff_sp7b_shipped memory file**

Path: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_sp7b_shipped.md`.

Body should cover:
- HEAD commit at SP7b end, commit hash range
- Files changed (mirror spec's "Files changed" table)
- Live smoke summary (which knobs verified on which games)
- Anything unexpected discovered during smoke
- "Next focus" pointer (SP5.5 analog or SP8 RetroNest adapter rewrite — whichever the user picks)
- Add `[[session_handoff_sp7b_shipped]]` link from MEMORY.md

- [ ] **Step 3: Mark the old kickoff memory superseded**

Update `sp7b_kickoff.md`: prepend a "SUPERSEDED — see [[session_handoff_sp7b_shipped]]" note at the top so a future session reading kickoff first immediately routes to the post-ship handoff.

- [ ] **Step 4: No git commit (auto-memory lives outside git)**

Auto-memory is in the user's `~/.claude` tree, not the repos. No commit.

---

## Self-review (run after writing)

Going through the spec sections against the plan:

- **Scope — three knobs** → Tasks 1-6 implement, Task 8 declares schema. ✓
- **Vocabulary clarification (programmatic writes, not INI patching)** → Implementation uses `MemorySettingsInterface::Set*Value`; no INI emission introduced. ✓
- **Out of scope (live updates, per-game, 3 deferred knobs, cleanup carry-overs)** → Plan never touches any. ✓
- **Host-side support already wired** → Plan reads/uses but never modifies `OptionsStore`, `environment_callbacks.cpp`, `generic_settings_page.cpp`. ✓
- **FastBoot two-site coupling** → Task 5 (Settings.cpp INI) AND Task 6 (`params.fast_boot`) both wire to `resolved.fast_boot`. ✓
- **Renderer enum values** → Auto=-1, Metal=17, SW=13, Null=11 — matches `Config.h:271-281`. ✓
- **New module: CoreOptions** → Task 1 creates header + .cpp; Tasks 2-4 fill behavior via TDD. ✓
- **Settings.cpp integration** → Task 5. ✓
- **LibretroFrontend.cpp integration** → Task 6. ✓
- **Host-side `settingsSchema` override** → Task 8. ✓
- **Data flow (boot)** → Task 6 step 3 implements `retro_load_game` exactly per the spec sequence. ✓
- **Error handling** → Task 3 (NULL key, unknown enum) + Task 4 (emit failure WARN) cover all four spec rows. ✓
- **TDD unit test** → Tasks 2-4 build cumulatively (5 cases total) using standalone compile gate `-DSP7B_TEST_CORE_OPTIONS_ONLY`. ✓
- **Live smoke ≥ 2 games** → Tasks 10 (R&C 2 NTSC) + 11 (DBZ PAL). ✓
- **Host build verification** → Task 8 step 3 + step 4. ✓
- **Files changed (estimate)** → Matches plan's File map exactly. ✓
- **Plan-time decisions (UI category, tooltip wording, Bool vs Combo)** → Task 8 picks "Recommended" + writes tooltips from Settings.cpp's existing comments + uses Combo for all three (consistent with mGBA). ✓

**Placeholder scan:** No `TODO`, `TBD`, "appropriate", "similar to", or "etc." within action steps. Comments inside code blocks are intentional (kept terse, explain non-obvious rationale).

**Type consistency:** `Resolved` struct fields (`renderer:int, mtvu:bool, fast_boot:bool`) used identically in Tasks 1-6. `EmitCoreOptionsV2` / `ReadResolved` signatures consistent throughout. Test variable `r` is `Resolved` everywhere it appears. `kDefinitions` named consistently.

**One latent ambiguity surfaced and resolved here:** Task 8's `SettingDef::options` ordering is `(label, value)` not `(value, label)` — I double-checked `mgba_libretro_adapter.cpp` which uses `{ v, v }` for already-symmetric strings (label == value), and the convention in the codebase (see `ppsspp_adapter.cpp:166-171`) is `{display, stored}` per the comment in `setting_def.h:39` `"list of (display label, INI value) pairs"`. The Task 8 code uses `{{"Auto", "auto"}, ...}` matching this contract — display label first.
