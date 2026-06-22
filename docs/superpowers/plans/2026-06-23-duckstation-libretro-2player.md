# DuckStation libretro — 2-player (Player 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Player 2 (PS1 port 1) support to the DuckStation libretro core and RetroNest adapter, auto-detected from a second physical controller with full hot-plug.

**Architecture:** The host already routes a second controller's input to libretro port 1 (`InputRouter`, device-index-as-port) but never binds device 1 and never tells the core a pad is present. This plan (1) makes the core read port 1, create/destroy a Player-2 controller via a port-1-only swap, and drive port-1 rumble; (2) adds a `duckstation_pad2_type` option; (3) adds a generic `maxLibretroPlayers()` adapter hook that gates the host into binding device 1 and notifying the core of port presence via `retro_set_controller_port_device`. Player 1 (port 0) is never touched, so it cannot regress.

**Tech Stack:** C++20. Core: DuckStation libretro (`duckstation-libretro/src/duckstation-libretro/`), standalone `assert` unit tests compiled with clang++. Host: Qt/C++ (`RetroNest-Project/cpp/`), QtTest unit tests.

## Global Constraints

- **Two repos.** Core lives in `/Users/mark/Documents/Projects/duckstation-libretro` (git `master`, **no remote**). Host lives in `/Users/mark/Documents/Projects/RetroNest-Project` (git `main`, has `origin`). Commit core changes in the core repo and host changes in the host repo — never mix.
- **Player 1 (port 0) behavior is frozen.** The callback governs port 1 only; do not change port-0 controller creation, input, or rumble logic beyond loop generalization that leaves port 0 identical.
- **Pad 2 default type:** `AnalogController`. **Pad 2 memory card:** none (slot 2 empty).
- **Multitap is out of scope** (`maxLibretroPlayers()` returns 2 for DuckStation; the loops are written generically but only port 1 is exercised).
- **Host↔core option parity:** every `duckstation_*` core option must appear identically (key, label, value list, default) in both `libretro_core_options.cpp` and the host `settingsSchema()`; the fidelity gate `tools/check_schema_fidelity.py` enforces this.
- **Build/run mode is x86_64 under Rosetta** (see `RetroNest-Project/CLAUDE.md`). The DuckStation core must be built universal (its `package.sh`, **no** `--arm64-only`). Host app target: `cmake --build cpp/build-x86_64 --target RetroNest`.
- **Manual GUI verification is the user's job** (TCC blocks the agent from launching the app). Tasks that can only be verified in-app end with an explicit manual checklist, not an automated assertion.

---

# Part A — Core changes (`duckstation-libretro`)

All paths in Part A are relative to `/Users/mark/Documents/Projects/duckstation-libretro`.

## Task 1: Pure Player-2 decision helpers

Two pure functions the rest of the core (and a standalone test) rely on: "is a device present?" and "what canonical type name does the pad2 option select?".

**Files:**
- Create: `src/duckstation-libretro/libretro_pad2.h`
- Test: `src/duckstation-libretro/libretro_pad2_test.cpp`

**Interfaces:**
- Produces:
  - `bool Pad2DeviceConnected(unsigned device)` — true iff `device != RETRO_DEVICE_NONE` (which is 0).
  - `const char* NormalizePad2TypeName(const char* option_value)` — returns `"DigitalController"` only for the exact value `"DigitalController"`, otherwise `"AnalogController"` (null/unknown default).

- [ ] **Step 1: Write the failing test**

Create `src/duckstation-libretro/libretro_pad2_test.cpp`:

```cpp
// Standalone unit test for the pure Player-2 decision helpers.
// Build & run:
//   clang++ -std=c++20 -I src src/duckstation-libretro/libretro_pad2_test.cpp -o /tmp/libretro_pad2_test && /tmp/libretro_pad2_test
#include "duckstation-libretro/libretro_pad2.h"

#include <cassert>
#include <cstring>
#include <cstdio>

int main()
{
  // RETRO_DEVICE_NONE (0) => not connected; any non-zero device => connected.
  assert(!Pad2DeviceConnected(0));
  assert(Pad2DeviceConnected(1));   // RETRO_DEVICE_JOYPAD
  assert(Pad2DeviceConnected(5));

  // Type option normalization: only the exact "DigitalController" maps to digital.
  assert(std::strcmp(NormalizePad2TypeName(nullptr), "AnalogController") == 0);
  assert(std::strcmp(NormalizePad2TypeName("AnalogController"), "AnalogController") == 0);
  assert(std::strcmp(NormalizePad2TypeName("DigitalController"), "DigitalController") == 0);
  assert(std::strcmp(NormalizePad2TypeName("garbage"), "AnalogController") == 0);

  std::printf("libretro_pad2_test: OK\n");
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /Users/mark/Documents/Projects/duckstation-libretro && clang++ -std=c++20 -I src src/duckstation-libretro/libretro_pad2_test.cpp -o /tmp/libretro_pad2_test`
Expected: FAIL — `fatal error: 'duckstation-libretro/libretro_pad2.h' file not found`.

- [ ] **Step 3: Write minimal implementation**

Create `src/duckstation-libretro/libretro_pad2.h`:

```cpp
// Pure decision helpers for Player-2 (libretro port 1) hot-plug support.
// Deliberately dependency-free (no core headers) so it compiles standalone in
// libretro_pad2_test.cpp, mirroring libretro_analog.h.
#pragma once

#include <cstring>

// True iff a libretro device id means "a controller is present" on this port.
// RETRO_DEVICE_NONE == 0; any other device id (RETRO_DEVICE_JOYPAD == 1, etc.)
// means a pad is plugged in.
inline bool Pad2DeviceConnected(unsigned device)
{
  return device != 0u;
}

// Canonical DuckStation controller-type name selected by the duckstation_pad2_type
// core option. Defaults to "AnalogController" for null/unknown values; only the
// exact "DigitalController" selects the digital pad.
inline const char* NormalizePad2TypeName(const char* option_value)
{
  if (option_value && std::strcmp(option_value, "DigitalController") == 0)
    return "DigitalController";
  return "AnalogController";
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `clang++ -std=c++20 -I src src/duckstation-libretro/libretro_pad2_test.cpp -o /tmp/libretro_pad2_test && /tmp/libretro_pad2_test`
Expected: PASS — prints `libretro_pad2_test: OK`.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/duckstation-libretro
git add src/duckstation-libretro/libretro_pad2.h src/duckstation-libretro/libretro_pad2_test.cpp
git commit -m "libretro: pure Player-2 device/type decision helpers + test"
```

---

## Task 2: `duckstation_pad2_type` core option + Pad2 base settings

Declare the new option and wire it so the core knows (a) which type to instantiate when port 1 becomes present, and (b) that Pad2 should boot in analog mode with no emulated deadzone when created. Pad2 is **not** auto-created here — presence is runtime-driven (Task 3).

**Files:**
- Modify: `src/duckstation-libretro/libretro_core_options.cpp` (after the `duckstation_pad1_type` block, ~line 126)
- Modify: `src/duckstation-libretro/libretro_internal.h` (add the shared type-name global)
- Modify: `src/duckstation-libretro/libretro.cpp` (define the global)
- Modify: `src/duckstation-libretro/libretro_settings.cpp` (read the option; seed Pad2 base settings)

**Interfaces:**
- Produces: `extern std::string g_pad2_type_name;` (canonical name to use when Pad2 is created; default `"AnalogController"`). Consumed by Task 3.

- [ ] **Step 1: Declare the option** in `libretro_core_options.cpp`, immediately after the `duckstation_pad1_type` `out.push_back({...});` block (the one ending with `"AnalogController", });` around line 126):

```cpp
  out.push_back({
    "duckstation_pad2_type", "Controller Type (Pad 2)", nullptr,
    "Controller type for Player 2, used when a second controller is connected. Analog Controller is a DualShock with sticks and rumble.", nullptr, nullptr,
    {{"AnalogController", "Analog Controller (DualShock)"}, {"DigitalController", "Digital Controller"},
     {nullptr, nullptr}},
    "AnalogController",
  });
```

- [ ] **Step 2: Declare the shared global** in `libretro_internal.h`. Add near the top (after existing includes; add `#include <string>` if not present):

```cpp
// Canonical DuckStation controller-type name to instantiate for Player 2 (port 1)
// when a second controller is connected. Set from the duckstation_pad2_type core
// option in ApplyCoreOptions; read on the worker thread when Pad2 is created.
// Single-threaded access (boot / options-change, then worker-thread reads).
extern std::string g_pad2_type_name;
```

- [ ] **Step 3: Define the global** in `libretro.cpp`, in the anonymous/file scope near the other `g_*` globals (e.g. just after `bool g_core_thread_initialized = false;`). Ensure `#include <string>` is present:

```cpp
// Player-2 controller type to create when port 1 gains a pad (see retro_set_controller_port_device).
std::string g_pad2_type_name = "AnalogController";
```

- [ ] **Step 4: Read the option** in `libretro_settings.cpp` `ApplyCoreOptions`. Add `#include "duckstation-libretro/libretro_pad2.h"` and `#include "duckstation-libretro/libretro_internal.h"` at the top if not already included. In the `// ── Controllers ──` section, immediately after the existing `duckstation_pad1_type` handling (`si->SetStringValue("Pad1", "Type", v);`), add:

```cpp
  if (const char* v = query("duckstation_pad2_type"))
    g_pad2_type_name = NormalizePad2TypeName(v);
```

- [ ] **Step 5: Seed Pad2 base settings** in `libretro_settings.cpp`, in the base-layer block right after the three `si->Set*Value("Pad1", ...)` lines (around line 396):

```cpp
    // Player 2 (port 1) is NOT auto-created — presence is driven at runtime by
    // retro_set_controller_port_device when a 2nd controller connects. Seed the
    // section so that when Pad2 IS created as an AnalogController it boots in
    // analog mode with no emulated deadzone, matching Pad1. Type stays "None"
    // so the core's own settings reload never spawns Pad2 on its own.
    si->SetStringValue("Pad2", "Type", "None");
    si->SetBoolValue("Pad2", "ForceAnalogOnReset", true);
    si->SetFloatValue("Pad2", "AnalogDeadzone", 0.0f);
```

- [ ] **Step 6: Build the core** to confirm it compiles.

Run: `cd /Users/mark/Documents/Projects/duckstation-libretro && ./package.sh 2>&1 | tail -20`
Expected: builds cleanly; produces the universal core dylib (no `--arm64-only`). If `package.sh` is long, an interim compile check of just the changed TUs is acceptable, but the full build must pass before Task 4's commit.

- [ ] **Step 7: Commit**

```bash
git add src/duckstation-libretro/libretro_core_options.cpp src/duckstation-libretro/libretro_internal.h src/duckstation-libretro/libretro.cpp src/duckstation-libretro/libretro_settings.cpp
git commit -m "libretro: add duckstation_pad2_type option + Pad2 base settings"
```

---

## Task 3: Hot-plug — implement `retro_set_controller_port_device` (port-1-only swap)

Replace the no-op stub so the host can attach/detach Player 2 at runtime. The actual controller rebuild happens on the worker thread at the top of `retro_run`, and rebuilds **only port 1** (never the `UpdateControllers()` all-ports path, which would disturb Pad1).

**Files:**
- Modify: `src/duckstation-libretro/libretro.cpp`

**Interfaces:**
- Consumes: `g_pad2_type_name` (Task 2); `Pad2DeviceConnected` (Task 1).
- Produces: a working `retro_set_controller_port_device(unsigned port, unsigned device)`; an internal `ApplyPad2(bool present)`.

- [ ] **Step 1: Add includes** at the top of `libretro.cpp` (alongside the existing `#include "core/..."` block): `#include "core/pad.h"` and `#include "duckstation-libretro/libretro_pad2.h"`. Ensure `#include <atomic>` is present.

- [ ] **Step 2: Add the request latch global** in the file-scope globals (near `g_pad2_type_name`):

```cpp
// Pending Player-2 hot-plug request set by retro_set_controller_port_device and
// drained at the top of retro_run on the worker thread. -1 = no request,
// 0 = detach (None), 1 = attach (g_pad2_type_name). Atomic: the latch may be
// set from the host's calling thread.
std::atomic<int> g_pad2_request{-1};
```

- [ ] **Step 3: Add `ApplyPad2`** as a static function above `UpdateControllers()` in `libretro.cpp`:

```cpp
// Rebuild ONLY port 1 (Player 2) to match `present`. Reuses the same pieces as
// System::UpdateControllers but never touches port 0, so Player 1 is undisturbed.
// Must run on the worker thread between frames (called from retro_run), never
// during System::RunFrame.
static void ApplyPad2(bool present)
{
  ControllerType type = ControllerType::None;
  if (present)
  {
    const ControllerInfo* info = Controller::GetControllerInfo(g_pad2_type_name);
    type = info ? info->type : ControllerType::AnalogController;
  }

  auto lock = Core::GetSettingsLock();
  g_settings.controller_types[1] = type;
  Pad::SetController(1, nullptr);
  if (type != ControllerType::None)
  {
    std::unique_ptr<Controller> controller = Controller::Create(type, 1);
    if (controller)
    {
      controller->LoadSettings(*Core::GetSettingsInterface(),
                               Controller::GetSettingsSection(1).c_str(), true);
      Pad::SetController(1, std::move(controller));
    }
  }
}
```

- [ ] **Step 4: Replace the stub.** Change `RETRO_API void retro_set_controller_port_device(unsigned, unsigned) {}` (around line 226) to:

```cpp
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
  // Player 1 (port 0) stays governed by settings and is never rebuilt here, so
  // the shipped P1 path cannot regress. Only port 1 (Player 2) is hot-plug driven.
  if (port != 1)
    return;
  g_pad2_request.store(Pad2DeviceConnected(device) ? 1 : 0, std::memory_order_relaxed);
}
```

- [ ] **Step 5: Drain the request in `retro_run`.** In `retro_run`, immediately before `System::RunFrame();` (around line 241), add:

```cpp
  // Apply any pending Player-2 hot-plug request on the worker thread, between
  // frames (never during RunFrame).
  if (const int r = g_pad2_request.exchange(-1, std::memory_order_relaxed); r >= 0)
    ApplyPad2(r == 1);

```

- [ ] **Step 6: Build the core.**

Run: `cd /Users/mark/Documents/Projects/duckstation-libretro && ./package.sh 2>&1 | tail -20`
Expected: clean build. (Resolves `Pad::SetController`, `Controller::Create`, `Controller::GetControllerInfo`, `Core::GetSettingsLock`, `Core::GetSettingsInterface`, `g_settings` — all already used by `core/system.cpp`.)

- [ ] **Step 7: Commit**

```bash
git add src/duckstation-libretro/libretro.cpp
git commit -m "libretro: implement retro_set_controller_port_device as port-1 hot-plug"
```

---

## Task 4: Read Player-2 input + drive Player-2 rumble

Generalize the port-0-only input read and rumble to loop ports 0–1. Port 0's behavior is byte-for-byte identical to today (it's the first loop iteration).

**Files:**
- Modify: `src/duckstation-libretro/libretro.cpp` (`UpdateControllers()` and the rumble block in `retro_run`)

- [ ] **Step 1: Generalize `UpdateControllers()`.** Replace the entire body of `UpdateControllers()` with the per-port loop:

```cpp
static void UpdateControllers()
{
  if (!g_input_state)
    return;

  for (unsigned port = 0; port <= 1; ++port)
  {
    Controller* c = System::GetController(port);
    if (!c)
      continue;

    for (unsigned id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id)
    {
      const int bind = MapRetroPadToDigital(id);
      if (bind < 0)
        continue;
      const int16_t pressed = g_input_state(port, RETRO_DEVICE_JOYPAD, 0, id);
      c->SetBindState(static_cast<u32>(bind), pressed ? 1.0f : 0.0f);
    }

    if (c->GetType() != ControllerType::AnalogController)
      continue;

    using HA = AnalogController::HalfAxis;
    constexpr u32 kBase = static_cast<u32>(AnalogController::Button::Count);
    const auto feed = [&](unsigned index, unsigned axis_id, HA neg_dir, HA pos_dir) {
      const int16_t v = g_input_state(port, RETRO_DEVICE_ANALOG, index, axis_id);
      const HalfAxisPair p = SplitAxis(v);
      c->SetBindState(kBase + static_cast<u32>(neg_dir), p.neg);
      c->SetBindState(kBase + static_cast<u32>(pos_dir), p.pos);
    };

    feed(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, HA::LLeft, HA::LRight);
    feed(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, HA::LUp, HA::LDown);
    feed(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, HA::RLeft, HA::RRight);
    feed(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, HA::RUp, HA::RDown);
  }
}
```

- [ ] **Step 2: Generalize the rumble block** in `retro_run`. Replace the existing `if (g_rumble_set_state) { Controller* rc = System::GetController(0); ... }` block (around lines 244–257) with:

```cpp
  // Drive rumble from each emulated DualShock's current motor state (ports 0–1).
  if (g_rumble_set_state)
  {
    for (unsigned port = 0; port <= 1; ++port)
    {
      Controller* rc = System::GetController(port);
      if (rc && rc->GetType() == ControllerType::AnalogController)
      {
        auto* ac = static_cast<AnalogController*>(rc);
        constexpr u32 kSmallMotor = 0, kLargeMotor = 1; // AnalogController motor indices
        const auto to_u16 = [](float s) -> uint16_t {
          return static_cast<uint16_t>(std::clamp(s, 0.0f, 1.0f) * 65535.0f);
        };
        g_rumble_set_state(port, RETRO_RUMBLE_STRONG, to_u16(ac->GetVibrationMotorStrength(kLargeMotor)));
        g_rumble_set_state(port, RETRO_RUMBLE_WEAK, to_u16(ac->GetVibrationMotorStrength(kSmallMotor)));
      }
    }
  }
```

- [ ] **Step 3: Build the core and re-run the pure unit tests** (regression guard for the analog logic these loops reuse).

Run:
```bash
cd /Users/mark/Documents/Projects/duckstation-libretro
./package.sh 2>&1 | tail -20
clang++ -std=c++20 -I src src/duckstation-libretro/libretro_analog_test.cpp -o /tmp/libretro_analog_test && /tmp/libretro_analog_test
clang++ -std=c++20 -I src src/duckstation-libretro/libretro_pad2_test.cpp -o /tmp/libretro_pad2_test && /tmp/libretro_pad2_test
```
Expected: clean build; both tests print `OK`.

- [ ] **Step 4: Commit**

```bash
git add src/duckstation-libretro/libretro.cpp
git commit -m "libretro: read Player-2 input and drive Player-2 rumble (ports 0-1)"
```

---

# Part B — Host changes (`RetroNest-Project`)

All paths in Part B are relative to `/Users/mark/Documents/Projects/RetroNest-Project`. Build/test with the x86_64 tree (`cpp/build-x86_64`).

## Task 5: `maxLibretroPlayers()` adapter hook

A generic per-core player count. Base `LibretroAdapter` returns 1 (current behavior — only device 0 bound, no port notifications). DuckStation returns 2. This single hook gates both binding replication (Task 7) and port notifications (Task 9), so other cores are untouched.

**Files:**
- Modify: `cpp/src/adapters/libretro/libretro_adapter.h`
- Modify: `cpp/src/adapters/libretro/duckstation_libretro_adapter.h`
- Test: `cpp/tests/test_duckstation_libretro_schema.cpp`

**Interfaces:**
- Produces: `virtual int LibretroAdapter::maxLibretroPlayers() const` (default `1`); `int DuckStationLibretroAdapter::maxLibretroPlayers() const override` (`2`). Consumed by Tasks 7 and 9.

- [ ] **Step 1: Write the failing test.** In `cpp/tests/test_duckstation_libretro_schema.cpp`, add a new private slot (e.g. after `testAllRowsAreLibretroOptions`):

```cpp
    void testMaxLibretroPlayersIsTwo() {
        QCOMPARE(DuckStationLibretroAdapter().maxLibretroPlayers(), 2);
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build cpp/build-x86_64 --target test_duckstation_libretro_schema 2>&1 | tail -20`
Expected: FAIL — compile error `no member named 'maxLibretroPlayers'`.

- [ ] **Step 3: Add the base virtual** in `cpp/src/adapters/libretro/libretro_adapter.h`, in the public section of `class LibretroAdapter` (near the other binding accessors around line 69):

```cpp
    /**
     * Number of player ports this core supports through the libretro input
     * path. Default 1 (Player 1 only). Cores that support more return >1; the
     * host then binds the extra controllers (device indices 1..N-1) and tells
     * the core via retro_set_controller_port_device when each port gains/loses
     * a pad. Multitap is NOT modeled here.
     */
    virtual int maxLibretroPlayers() const { return 1; }
```

- [ ] **Step 4: Override in DuckStation** in `cpp/src/adapters/libretro/duckstation_libretro_adapter.h`, in the public section of `class DuckStationLibretroAdapter`:

```cpp
    int maxLibretroPlayers() const override { return 2; }
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build cpp/build-x86_64 --target test_duckstation_libretro_schema 2>&1 | tail -5 && ./cpp/build-x86_64/test_duckstation_libretro_schema 2>&1 | tail -20`
Expected: PASS — the new slot passes (alongside the existing ones).

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/libretro_adapter.h cpp/src/adapters/libretro/duckstation_libretro_adapter.h cpp/tests/test_duckstation_libretro_schema.cpp
git commit -m "host: add maxLibretroPlayers() adapter hook (DuckStation=2)"
```

---

## Task 6: Expose `duckstation_pad2_type` in the host schema

Mirror the core option (Task 2) in the adapter's `settingsSchema()` so the setting appears in the RetroNest UI and the fidelity gate stays green.

**Files:**
- Modify: `cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp` (`settingsSchema()`, Controllers group)
- Test: `cpp/tests/test_duckstation_libretro_schema.cpp`

- [ ] **Step 1: Write the failing test.** In `cpp/tests/test_duckstation_libretro_schema.cpp`, add a private slot:

```cpp
    void testPad2TypeOptionPresentWithAnalogDefault() {
        QString def = "<missing>";
        for (const auto& d : schema_) if (d.key == "duckstation_pad2_type") def = d.defaultValue;
        QCOMPARE(def, QString("AnalogController"));
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build cpp/build-x86_64 --target test_duckstation_libretro_schema 2>&1 | tail -5 && ./cpp/build-x86_64/test_duckstation_libretro_schema 2>&1 | tail -20`
Expected: FAIL — `Actual: "<missing>"` vs `Expected: "AnalogController"`.

- [ ] **Step 3: Add the schema row** in `cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp`, immediately after the `duckstation_pad1_type` `s.append(opt(...));` block in the `// Controllers group` section:

```cpp
    s.append(opt(
        "Console", "Controllers",
        "duckstation_pad2_type", "Controller Type (Pad 2)", "AnalogController",
        {{"Analog Controller (DualShock)", "AnalogController"}, {"Digital Controller", "DigitalController"}},
        "Controller type for Player 2, used when a second controller is connected. Analog adds sticks and rumble."
    ));
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build cpp/build-x86_64 --target test_duckstation_libretro_schema 2>&1 | tail -5 && ./cpp/build-x86_64/test_duckstation_libretro_schema 2>&1 | tail -20`
Expected: PASS — all slots pass.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp cpp/tests/test_duckstation_libretro_schema.cpp
git commit -m "host: expose duckstation_pad2_type in settingsSchema"
```

---

## Task 7: Bind the second controller (device-index replication)

Today the startup binding loader binds only device index 0 (`controls.ini` uses `SDL-0/...`), so a 2nd pad's events resolve to no slot. Replicate each parsed element→slot mapping onto device indices `0..maxLibretroPlayers-1`, so controller N drives port N with the standard layout. For `maxLibretroPlayers()==1` (every non-DuckStation core) the behavior is identical to today.

**Files:**
- Modify: `cpp/src/core/game_session.cpp` (the `controls.ini` load loop in `startGame`, ~lines 409–437)

**Interfaces:**
- Consumes: `LibretroAdapter::maxLibretroPlayers()` (Task 5); `InputRouter::bind(int deviceIdx, const QString&, RetroPadSlot)` (existing).

- [ ] **Step 1: Add the player count** at the top of the binding-load block (just inside the `{ InputRouter& router = rt->input(); ... }` scope, after `router.clearBindings();`):

```cpp
        const int maxPlayers = lr->maxLibretroPlayers();
```

- [ ] **Step 2: Replicate the binding.** Replace the final binding line inside the loop:

```cpp
                const RetroPadSlot slot = retroPadSlotFromKey(def.key);
                if (slot != RetroPadSlot::None)
                    router.bind(deviceIdx, element, slot);
```

with:

```cpp
                const RetroPadSlot slot = retroPadSlotFromKey(def.key);
                if (slot == RetroPadSlot::None)
                    continue;
                // Bind the configured device, and replicate the same element->slot
                // mapping onto each additional player's device index so a 2nd/3rd
                // controller drives its own port with the standard layout. For
                // single-player cores (maxPlayers == 1) this binds only device 0,
                // exactly as before.
                router.bind(deviceIdx, element, slot);
                for (int p = 0; p < maxPlayers; ++p)
                    if (p != deviceIdx)
                        router.bind(p, element, slot);
```

- [ ] **Step 3: Build the host app** to confirm it compiles.

Run: `cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -20`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/game_session.cpp
git commit -m "host: replicate controller bindings across player device indices"
```

---

## Task 8: CoreRuntime — schedule controller-port-device changes onto the worker thread

Add a thread-safe request method mirroring `requestSaveState`/`requestLoadState`, drained at the top of the worker loop (between frames) where calling into the core is safe.

**Files:**
- Modify: `cpp/src/core/libretro/core_runtime.h`
- Modify: `cpp/src/core/libretro/core_runtime.cpp`

**Interfaces:**
- Produces: `void CoreRuntime::requestControllerPortDevice(unsigned port, unsigned device)` (thread-safe). Consumed by Task 9.
- Consumes: `CoreSymbols::retro_set_controller_port_device` (already resolved in `core_loader.cpp`).

- [ ] **Step 1: Declare the public method** in `cpp/src/core/libretro/core_runtime.h`, near `requestLoadState` (~line 100):

```cpp
    /**
     * Asynchronously schedule a libretro controller-port device change (e.g.
     * attach/detach Player 2). Safe to call from any thread; the worker applies
     * it between frames via retro_set_controller_port_device. The latest device
     * for a given port wins. `device` is a RETRO_DEVICE_* id (RETRO_DEVICE_NONE
     * to detach).
     */
    void requestControllerPortDevice(unsigned port, unsigned device);
```

- [ ] **Step 2: Declare the flush + storage** in the private section of `core_runtime.h` (near the pending save/load members). Ensure `#include <map>` and `#include <mutex>` are present (add if missing):

```cpp
    void flushPendingControllerDevices(const CoreSymbols& s);
    std::mutex m_portDevMx;
    std::map<unsigned, unsigned> m_pendingPortDevices;  // port -> RETRO_DEVICE_* (latest wins)
```

- [ ] **Step 3: Implement both methods** in `core_runtime.cpp`, next to `flushPendingLoadState` (~line 316):

```cpp
void CoreRuntime::requestControllerPortDevice(unsigned port, unsigned device) {
    std::lock_guard<std::mutex> l(m_portDevMx);
    m_pendingPortDevices[port] = device;
}

void CoreRuntime::flushPendingControllerDevices(const CoreSymbols& s) {
    std::map<unsigned, unsigned> pending;
    {
        std::lock_guard<std::mutex> l(m_portDevMx);
        pending.swap(m_pendingPortDevices);
    }
    if (!s.retro_set_controller_port_device)
        return;
    for (const auto& [port, device] : pending)
        s.retro_set_controller_port_device(port, device);
}
```

- [ ] **Step 4: Drain in the worker loop.** In `core_runtime.cpp` `runLoop`, inside the per-frame `mac::AutoreleaseScope` block, add the flush immediately before `flushPendingLoadState(s);` (~line 505):

```cpp
            flushPendingControllerDevices(s);
            flushPendingLoadState(s);
```

- [ ] **Step 5: Build the host app.**

Run: `cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -20`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/libretro/core_runtime.h cpp/src/core/libretro/core_runtime.cpp
git commit -m "host: CoreRuntime.requestControllerPortDevice scheduled on worker thread"
```

---

## Task 9: GameSession — notify the core of port presence (initial + hot-plug)

Add a `connectedDeviceIndices()` accessor to `SdlInputManager`, then in `GameSession::startGame` (after emulation input routing is wired) do an initial port-presence sync and connect `controllersChanged` so connect/disconnect updates the core live. Gated on `maxLibretroPlayers() > 1`, so only DuckStation is affected.

**Files:**
- Modify: `cpp/src/core/sdl_input_manager.h`
- Modify: `cpp/src/core/sdl_input_manager.cpp`
- Modify: `cpp/src/core/game_session.cpp`

**Interfaces:**
- Produces: `QList<int> SdlInputManager::connectedDeviceIndices() const`.
- Consumes: `CoreRuntime::requestControllerPortDevice` (Task 8); `LibretroAdapter::maxLibretroPlayers()` (Task 5); `SdlInputManager::controllersChanged()` (existing signal).

- [ ] **Step 1: Declare the accessor** in `cpp/src/core/sdl_input_manager.h`, in the public section near `connectedControllers()` (ensure `#include <QList>` is present):

```cpp
    /** Device indices (0-based, lowest-available scheme) of all currently
     *  connected controllers. Used to compute per-port presence for the
     *  libretro controller-port-device notification. */
    QList<int> connectedDeviceIndices() const;
```

- [ ] **Step 2: Implement it** in `cpp/src/core/sdl_input_manager.cpp`, next to `connectedControllers()` (~line 303). It must take the same lock that guards `m_deviceIndices`:

```cpp
QList<int> SdlInputManager::connectedDeviceIndices() const {
    std::lock_guard<std::mutex> lock(m_controllerMx);
    QList<int> indices;
    for (auto it = m_deviceIndices.constBegin(); it != m_deviceIndices.constEnd(); ++it)
        indices.append(it.value());
    return indices;
}
```

(If `m_controllerMx` is declared `mutable`, this compiles as-is; it is already locked by `connectedControllers()`/`setRumbleMotor`, which are const-callable. If `connectedControllers()` does not lock, match its exact style instead.)

- [ ] **Step 3: Verify RETRO_DEVICE_* visibility** in `game_session.cpp`.

Run: `cd /Users/mark/Documents/Projects/RetroNest-Project && grep -n "RETRO_DEVICE\|core_runtime.h\|libretro_adapter.h" cpp/src/core/game_session.cpp | head`
Expected: `libretro_adapter.h` is included (it transitively pulls `core_runtime.h` → `core_loader.h` → `libretro.h`, where `RETRO_DEVICE_JOYPAD`/`RETRO_DEVICE_NONE` are defined). If a later build (Step 5) errors on those identifiers, add `#include "libretro.h"` to `game_session.cpp`.

- [ ] **Step 4: Wire the sync + hot-plug connection** in `cpp/src/core/game_session.cpp`, in `startGame`, immediately after the existing emulation-routing line `m_sdlInputManager->setEmulationMode(&rt->input());` and before the final `return true;` (~line 446):

```cpp
    // Player 2+ support: tell the core which ports currently have a pad, and
    // keep it updated on hot-plug. Gated to multiplayer-capable cores so
    // single-player cores are unaffected. `rt` is the connection context, so
    // the lambda auto-disconnects when the runtime is destroyed on game end.
    if (m_sdlInputManager && lr->maxLibretroPlayers() > 1) {
        const int maxPlayers = lr->maxLibretroPlayers();
        SdlInputManager* sdl = m_sdlInputManager;
        auto syncPorts = [rt, sdl, maxPlayers]() {
            const QList<int> connected = sdl->connectedDeviceIndices();
            for (int port = 0; port < maxPlayers; ++port) {
                const bool present = connected.contains(port);
                rt->requestControllerPortDevice(static_cast<unsigned>(port),
                                                 present ? RETRO_DEVICE_JOYPAD : RETRO_DEVICE_NONE);
            }
        };
        syncPorts();  // initial snapshot at boot
        QObject::connect(sdl, &SdlInputManager::controllersChanged, rt, syncPorts);
    }
```

- [ ] **Step 5: Build the host app.**

Run: `cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -20`
Expected: clean build. (If it errors on `RETRO_DEVICE_*`, apply the include from Step 3 and rebuild.)

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/sdl_input_manager.h cpp/src/core/sdl_input_manager.cpp cpp/src/core/game_session.cpp
git commit -m "host: notify DuckStation core of port presence with hot-plug"
```

---

# Part C — Integration & verification

## Task 10: Fidelity gate, full build, and manual GUI verification

**Files:** none modified (verification only). If the fidelity gate fails, fix the mismatch in whichever side drifted (core `libretro_core_options.cpp` or host `settingsSchema()`).

- [ ] **Step 1: Run the host↔core schema-fidelity gate.**

Run: `cd /Users/mark/Documents/Projects/duckstation-libretro && python3 tools/check_schema_fidelity.py`
Expected: PASS — the option count is now **62** (was 61) and `duckstation_pad2_type` matches on both sides (key, label `Controller Type (Pad 2)`, value list, default `AnalogController`).

- [ ] **Step 2: Run the host schema unit test (full).**

Run: `cd /Users/mark/Documents/Projects/RetroNest-Project && cmake --build cpp/build-x86_64 --target test_duckstation_libretro_schema 2>&1 | tail -5 && ctest --test-dir cpp/build-x86_64 -R DuckStationLibretroSchema --output-on-failure`
Expected: `DuckStationLibretroSchema` passes, including `testMaxLibretroPlayersIsTwo` and `testPad2TypeOptionPresentWithAnalogDefault`.

- [ ] **Step 3: Build everything from clean.**

Run:
```bash
cd /Users/mark/Documents/Projects/duckstation-libretro && ./package.sh 2>&1 | tail -10
cd /Users/mark/Documents/Projects/RetroNest-Project && cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -10
```
Expected: both build clean. Ensure the freshly built universal core is the one RetroNest loads (per `CLAUDE.md` core install/copy step, if any).

- [ ] **Step 4: Manual GUI verification (USER — agent cannot launch the app).**

Launch the x86_64 RetroNest app and verify, in order:
  1. **P1-only regression check:** one controller connected → boot a PS1 game → Player 1 plays exactly as before (buttons, analog sticks, rumble). `/tmp/rn.log` shows the GPU_HW init line.
  2. **Boot with two pads:** connect 2 controllers, boot a 2-player game (e.g. a fighting/racing title) → both players control their own character independently.
  3. **Hot-plug attach:** with the game running and only P1 connected, plug in a 2nd controller → Player 2 becomes active (game registers a controller in port 2).
  4. **Hot-plug detach:** unplug the 2nd controller mid-game → Pad2 disappears (no phantom P2 input; P1 unaffected).
  5. **P2 rumble:** trigger a P2 rumble event → the 2nd controller vibrates.
  6. **Type option:** set Controller Type (Pad 2) = Digital in settings, relaunch → P2 works as a digital pad (no stick/rumble); set back to Analog → sticks/rumble return.
  7. **Other cores unaffected (smoke):** launch one non-DuckStation libretro core (e.g. PPSSPP or PCSX2) with a single controller → P1 still works (confirms the `maxLibretroPlayers()==1` gate left them untouched).

- [ ] **Step 5: Push the host repo (after the user confirms Step 4).**

Run: `cd /Users/mark/Documents/Projects/RetroNest-Project && git push origin main`
Expected: pushed. (The core repo has no remote — leave it on local `master`.)

---

## Notes & deferred items

- **Per-Player-2 remapping UI is deferred.** P2 reuses P1's button layout via device-index binding replication (Task 7); no separate Pad2 binding screen is added. If per-P2 remapping is wanted later, that's a follow-up that adds `Pad2`-section binding defs + a UI affordance.
- **Multitap (3–8 players)** remains out of scope. The loops in Tasks 4/7/9 are written generically, so extending `maxLibretroPlayers()` and the core's port loops is the future path — but the core's `retro_set_controller_port_device` currently handles port 1 only by design.
- **Player-2 memory card** (slot 2) is intentionally empty; add a `Card2` core option later if 2-player saves are desired.
