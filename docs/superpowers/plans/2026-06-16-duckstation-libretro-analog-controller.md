# DuckStation libretro — Analog (DualShock) Controller Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Player 1 a full DualShock (`AnalogController`) in the DuckStation libretro core — analog sticks + rumble — defaulting to analog with a Digital/Analog core option, by wiring existing-but-unused pieces on both the core and host sides.

**Architecture:** Core-wiring only. (1) A `duckstation_pad1_type` core option drives the `Pad1/Type` setting (default `AnalogController`); (2) `UpdateControllers()` reads `RETRO_DEVICE_ANALOG` axes and feeds them to the `AnalogController` via half-axis binds; (3) rumble is polled from the controller each frame and pushed to the libretro rumble interface. Deadzone is owned by RetroNest (core stick deadzone set to 0). Analog mode is forced on at boot (`ForceAnalogOnReset`), no runtime toggle. Player 1 only.

**Tech Stack:** C++20, DuckStation fork core (`duckstation-libretro/`, git `master`, **local-only — never push**), RetroNest host (Qt), x86_64/Rosetta build.

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-06-16-duckstation-libretro-analog-controller-design.md`

**Conventions:**
- `$DS` = `/Users/mark/Documents/Projects/duckstation-libretro`. `$RN` = `/Users/mark/Documents/Projects/RetroNest-Project`.
- Commit core changes to the **core repo** (`$DS`, git `master`, never push). Commit the adapter change to `$RN`.
- Verified facts (file:line) from source: `AnalogController` binds — `Button` 0–16, `HALFAXIS_BIND_START_INDEX = 17`, `HalfAxis{LLeft=0,LRight=1,LDown=2,LUp=3,RLeft=4,RRight=5,RDown=6,RUp=7}` (`src/core/analog_controller.h:15-105`); `SetBindState(u32 index, float value)` (half-axis value is 0..1); `GetMotorStrength(u32) const` is **private** (`analog_controller.h:127`), motor indices small=0/large=1; `LoadSettings` reads `"ForceAnalogOnReset"` (default true), `"AnalogDeadzone"`, `"AnalogSensitivity"` from section `"Pad1"` (`analog_controller.cpp:889-906`); `Controller::GetType()` → `ControllerType` (`controller.h:60`); `System::GetController(0)` → `Controller*` (`system.cpp:3786`); core options defined in `libretro_core_options.cpp` (`out.push_back({...})`, example :12-19) and read in `libretro_settings.cpp ApplyCoreOptions` via a `query(key)` lambda (:96-108), called at :396 **after** the hardcoded `si->SetStringValue("Pad1","Type","DigitalController")` at :391; `g_environ` global at `libretro.cpp:36`; rumble interface is **not** currently requested.

---

### Task 1: Pad-type selection — core option + settings layer + adapter row

Makes the controller type selectable (default `AnalogController`), with analog mode forced on and core stick deadzone zeroed. No analog axes fed yet — that's Task 2.

**Files:**
- Modify: `$DS/src/duckstation-libretro/libretro_core_options.cpp` (add option, near :12-19)
- Modify: `$DS/src/duckstation-libretro/libretro_settings.cpp` (Pad1 base settings near :391; `ApplyCoreOptions` read near :108-115)
- Modify: `$RN/cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp` (`controllerTypes()` :15-19; `settingsSchema()` rows)

- [ ] **Step 1: Add the core option definition**

In `$DS/src/duckstation-libretro/libretro_core_options.cpp`, alongside the existing `out.push_back({...})` blocks (mirror the `duckstation_console_region` block at ~:12-19), add:

```cpp
out.push_back({
  "duckstation_pad1_type", "Controller Type (Pad 1)", nullptr,
  "Controller type for Player 1. Analog Controller is a DualShock with sticks and rumble.", nullptr, nullptr,
  {{"AnalogController", "Analog Controller (DualShock)"}, {"DigitalController", "Digital Controller"},
   {nullptr, nullptr}},
  "AnalogController",
});
```

- [ ] **Step 2: Drive `Pad1/Type` from the option + force analog mode + zero deadzone**

In `$DS/src/duckstation-libretro/libretro_settings.cpp`, change the hardcoded digital line (currently `:391`):

```cpp
si->SetStringValue("Pad1", "Type", "DigitalController");
```
to default analog and add the per-pad analog settings:
```cpp
// Default to the DualShock (AnalogController); a duckstation_pad1_type core option can
// override this below in ApplyCoreOptions. Force analog mode on at boot, and zero the
// emulated stick deadzone because RetroNest already deadzones the axes it sends.
si->SetStringValue("Pad1", "Type", "AnalogController");
si->SetBoolValue("Pad1", "ForceAnalogOnReset", true);
si->SetFloatValue("Pad1", "AnalogDeadzone", 0.0f);
```

Then, inside `ApplyCoreOptions(...)` (after the `query`/`queryBool` lambdas, alongside the other `query(...)` reads such as the `duckstation_console_region` read), add:

```cpp
if (const char* v = query("duckstation_pad1_type"))
  si->SetStringValue("Pad1", "Type", v);
```

- [ ] **Step 3: Add AnalogController to the adapter's controller types + settings row**

In `$RN/cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp`, change `controllerTypes()` (:15-19):

```cpp
QVector<ControllerTypeDef> DuckStationLibretroAdapter::controllerTypes() const {
    return {
        {"AnalogController", "Analog Controller", ""},
        {"DigitalController", "Digital Controller", ""},
    };
}
```

And in `settingsSchema()`, alongside the existing `s.append(opt(...))` rows, add a pad-type row. **Mirror the exact value/label orientation and category/group conventions of the sibling `opt(...)` rows in this file** (the orientation must satisfy the fidelity check in Step 4):

```cpp
s.append(opt(
    "Console", "Controllers",
    "duckstation_pad1_type", "Controller Type (Pad 1)", "AnalogController",
    {{"AnalogController", "Analog Controller (DualShock)"}, {"DigitalController", "Digital Controller"}},
    "Controller type for Player 1. Analog adds sticks and rumble."
));
```

- [ ] **Step 4: Run the schema-fidelity check (the contract gate)**

Run:
```bash
cd "$DS" && python3 src/duckstation-libretro/tools/check_schema_fidelity.py \
  --core src/duckstation-libretro/libretro_core_options.cpp \
  --host "$RN/cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp"
```
Expected: exit 0, no drift. If it reports a mismatch on `duckstation_pad1_type` (key/default/values), the most likely cause is the value/label pair orientation in Step 3 — swap each `{a, b}` to `{b, a}` to match the sibling rows, and re-run until it passes.

- [ ] **Step 5: Run the adapter schema QtTest (catch first-run-default regressions)**

The adapter has an existing schema test at `$RN/cpp/tests/test_duckstation_libretro_schema.cpp` (verifies every default is in its options list, no duplicate keys, first-run defaults). Build and run it:
```bash
cd "$RN" && arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_duckstation_libretro_schema -j 6 \
  && arch -x86_64 ./cpp/build-x86_64/test_duckstation_libretro_schema
```
Expected: PASS. If the new `duckstation_pad1_type` row trips an assertion (e.g. a per-category key count, or a "no controller option" expectation), update that test to expect the new row — the row is intended, so the test is what needs to learn about it. (If the exact target name differs, find it with `grep -rl duckstation_libretro_schema "$RN/cpp" --include=CMakeLists.txt`.)

- [ ] **Step 6: Build the core**

Run:
```bash
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS" && src/duckstation-libretro/package.sh
```
Expected: builds + deploys universal core, no errors.

- [ ] **Step 7: Commit (two repos)**

```bash
cd "$DS" && git add src/duckstation-libretro/libretro_core_options.cpp src/duckstation-libretro/libretro_settings.cpp
git commit -m "libretro: pad-type core option (default AnalogController) + analog base settings

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
cd "$RN" && git add cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp
git commit -m "duckstation adapter: expose AnalogController + pad1 type option

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Feed analog stick axes in `UpdateControllers()`

**Files:**
- Create: `$DS/src/duckstation-libretro/libretro_analog.h` (pure axis→half-axis helper)
- Create: `$DS/src/duckstation-libretro/libretro_analog_test.cpp` (standalone test)
- Modify: `$DS/src/duckstation-libretro/libretro.cpp` (`UpdateControllers()` ~:73-88; add include)

- [ ] **Step 1: Write the failing test**

Create `$DS/src/duckstation-libretro/libretro_analog_test.cpp`:

```cpp
// Standalone unit test for SplitAxis (analog axis -> half-axis magnitudes).
// Build & run:
//   clang++ -std=c++20 -I src src/duckstation-libretro/libretro_analog_test.cpp -o /tmp/libretro_analog_test && /tmp/libretro_analog_test
#include "duckstation-libretro/libretro_analog.h"

#include <cassert>
#include <cmath>
#include <cstdio>

static bool close(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main()
{
  // center -> both zero
  auto c = SplitAxis(0);
  assert(close(c.neg, 0.0f) && close(c.pos, 0.0f));

  // full positive -> pos=1, neg=0
  auto p = SplitAxis(32767);
  assert(close(p.neg, 0.0f) && close(p.pos, 1.0f));

  // full negative (INT16_MIN) -> neg clamped to 1, pos=0
  auto n = SplitAxis(-32768);
  assert(close(n.neg, 1.0f) && close(n.pos, 0.0f));

  // half positive -> pos≈0.5
  auto h = SplitAxis(16384);
  assert(close(h.neg, 0.0f) && close(h.pos, 0.5f));

  std::printf("libretro_analog_test: OK\n");
  return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails (no header yet)**

Run:
```bash
cd "$DS" && clang++ -std=c++20 -I src src/duckstation-libretro/libretro_analog_test.cpp -o /tmp/libretro_analog_test
```
Expected: FAIL — `fatal error: 'duckstation-libretro/libretro_analog.h' file not found`.

- [ ] **Step 3: Write the helper header**

Create `$DS/src/duckstation-libretro/libretro_analog.h`:

```cpp
// SPDX-License-Identifier: CC-BY-NC-ND-4.0
// Converts a signed libretro analog-axis value (-32768..32767, center 0) into the two
// AnalogController half-axis magnitudes (0..1). Exactly one is non-zero (or both zero at
// center). For X axes: pos = right deflection, neg = left. For Y axes (libretro Y+ = down):
// pos = down, neg = up. Pure + dependency-free so it is unit-testable in isolation.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>

struct HalfAxisPair
{
  float neg;
  float pos;
};

inline HalfAxisPair SplitAxis(std::int16_t v)
{
  const float mag = std::min(static_cast<float>(std::abs(static_cast<int>(v))) / 32767.0f, 1.0f);
  return (v >= 0) ? HalfAxisPair{0.0f, mag} : HalfAxisPair{mag, 0.0f};
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run:
```bash
cd "$DS" && clang++ -std=c++20 -I src src/duckstation-libretro/libretro_analog_test.cpp -o /tmp/libretro_analog_test && /tmp/libretro_analog_test
```
Expected: prints `libretro_analog_test: OK`, exit 0.

- [ ] **Step 5: Feed the axes in `UpdateControllers()`**

In `$DS/src/duckstation-libretro/libretro.cpp`, add includes near the other `core/` includes:

```cpp
#include "core/analog_controller.h"
#include "duckstation-libretro/libretro_analog.h"
```

Then extend `UpdateControllers()` (currently ~:73-88) so that, after the existing digital-button loop and gated on the controller being analog, it feeds the four sticks. Replace the body so it reads:

```cpp
static void UpdateControllers()
{
  Controller* c = System::GetController(0);
  if (!c || !g_input_state)
    return;

  for (unsigned id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id)
  {
    const int bind = MapRetroPadToDigital(id);
    if (bind < 0)
      continue;

    const int16_t pressed = g_input_state(0, RETRO_DEVICE_JOYPAD, 0, id);
    c->SetBindState(static_cast<u32>(bind), pressed ? 1.0f : 0.0f);
  }

  // Analog sticks: only for an AnalogController. Half-axis bind indices are
  // HALFAXIS_BIND_START_INDEX + HalfAxis::*. RetroNest already deadzones these axes.
  if (c->GetType() != ControllerType::AnalogController)
    return;

  using HA = AnalogController::HalfAxis;
  constexpr u32 kBase = AnalogController::HALFAXIS_BIND_START_INDEX;
  const auto feed = [&](unsigned index, unsigned axis_id, HA neg_dir, HA pos_dir) {
    const int16_t v = g_input_state(0, RETRO_DEVICE_ANALOG, index, axis_id);
    const HalfAxisPair p = SplitAxis(v);
    c->SetBindState(kBase + static_cast<u32>(neg_dir), p.neg);
    c->SetBindState(kBase + static_cast<u32>(pos_dir), p.pos);
  };

  // Left stick
  feed(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, HA::LLeft, HA::LRight);
  feed(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, HA::LUp, HA::LDown);  // Y+ = down
  // Right stick
  feed(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, HA::RLeft, HA::RRight);
  feed(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, HA::RUp, HA::RDown);  // Y+ = down
}
```

- [ ] **Step 6: Build the core**

Run:
```bash
export MACOSX_DEPLOYMENT_TARGET=13.3; cd "$DS" && src/duckstation-libretro/package.sh
```
Expected: builds + deploys, no errors.

- [ ] **Step 7: Commit**

```bash
cd "$DS" && git add src/duckstation-libretro/libretro_analog.h src/duckstation-libretro/libretro_analog_test.cpp src/duckstation-libretro/libretro.cpp
git commit -m "libretro: feed analog stick axes to AnalogController

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Rumble

Request the libretro rumble interface and poll the controller's motor strength each frame.

**Files:**
- Modify: `$DS/src/core/analog_controller.h` (add a public motor-strength accessor)
- Modify: `$DS/src/duckstation-libretro/libretro.cpp` (request interface; poll in `retro_run`)

- [ ] **Step 1: Expose the motor strength publicly**

In `$DS/src/core/analog_controller.h`, in the **public** section (e.g. just after the `SetMotorState` declaration at ~:126), add:

```cpp
  /// Current vibration strength (0..1) for the given motor (small=0, large=1).
  /// Used by the libretro frontend to drive the host rumble interface each frame.
  float GetVibrationMotorStrength(u32 motor) const { return GetMotorStrength(motor); }
```

(`GetMotorStrength` is private; this public inline wrapper in the same class may call it.)

- [ ] **Step 2: Add the rumble callback global + request the interface**

In `$DS/src/duckstation-libretro/libretro.cpp`, near the other globals (e.g. by `g_environ` at :36), add:

```cpp
static retro_set_rumble_state_t g_rumble_set_state = nullptr;
```

In `retro_load_game(...)`, after the system boots successfully (near the `SET_GAME_IDENTITY` block), request the interface:

```cpp
  // Rumble: AnalogController updates its motor state from the game's vibration command;
  // we poll it each frame (retro_run) and push it to the frontend via this interface.
  if (g_environ)
  {
    retro_rumble_interface rumble{};
    if (g_environ(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble))
      g_rumble_set_state = rumble.set_rumble_state;
  }
```

- [ ] **Step 3: Poll and push rumble each frame**

In `$DS/src/duckstation-libretro/libretro.cpp` `retro_run()`, after `System::RunFrame();` (and after the existing `GpuProf` block if present), add:

```cpp
  // Drive rumble from the emulated DualShock's current motor state.
  if (g_rumble_set_state)
  {
    Controller* rc = System::GetController(0);
    if (rc && rc->GetType() == ControllerType::AnalogController)
    {
      auto* ac = static_cast<AnalogController*>(rc);
      constexpr u32 kSmallMotor = 0, kLargeMotor = 1; // AnalogController motor indices
      const auto to_u16 = [](float s) -> uint16_t {
        return static_cast<uint16_t>(std::clamp(s, 0.0f, 1.0f) * 65535.0f);
      };
      g_rumble_set_state(0, RETRO_RUMBLE_STRONG, to_u16(ac->GetVibrationMotorStrength(kLargeMotor)));
      g_rumble_set_state(0, RETRO_RUMBLE_WEAK, to_u16(ac->GetVibrationMotorStrength(kSmallMotor)));
    }
  }
```

(`AnalogController` and `Controller`/`ControllerType` are already included via Task 2's `#include "core/analog_controller.h"`. `<algorithm>` is needed for `std::clamp` — add the include if not already present.)

- [ ] **Step 4: Build the core**

Run:
```bash
export MACOSX_DEPLOYMENT_TARGET=13.3; cd "$DS" && src/duckstation-libretro/package.sh
```
Expected: builds + deploys, no errors.

- [ ] **Step 5: Commit**

```bash
cd "$DS" && git add src/core/analog_controller.h src/duckstation-libretro/libretro.cpp
git commit -m "libretro: drive rumble from AnalogController motor state

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Build RetroNest + user verification

**Files:** none (build + manual verification).

- [ ] **Step 1: Build + deploy RetroNest (x86_64)**

Run:
```bash
cd "$RN"
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
```
Expected: builds. The `Cannot resolve rpath libspirv-cross-c-shared` line is a known cosmetic warning.

- [ ] **Step 2: User verification (TCC blocks the agent from the GUI)**

Hand to the user. They launch from their own Terminal (needs Documents permission):
```bash
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1
```
Verify:
- **Sticks move** in a stick-using game (Ape Escape = both sticks; Spyro / Crash Team Racing = left stick).
- **D-pad + face buttons** still work normally.
- **Rumble fires** in a vibration game (CTR, Crash, a racing title).
- **Digital/Analog setting:** set Pad 1 to `Digital Controller` in RetroNest settings → sticks go dead, pad behaves as before; back to `Analog` → sticks return.
- **Regression:** save-and-exit → resume is clean with the analog pad; and a mid-session pad-type switch doesn't crash.

- [ ] **Step 3: No commit** (build artifacts + manual verification only).

---

## Notes for the implementer

- **Why poll rumble instead of using `InputManager`:** the core's `InputManager::SetPadVibrationIntensity` dispatches through registered `InputSource`s, and the libretro build registers none — so that path is dead. `AnalogController` updates `m_motor_state` directly from the game's vibration command regardless, so polling `GetVibrationMotorStrength` each frame is the correct, minimal route (the SwanStation approach).
- **Why analog is the default and still safe for digital players:** the DualShock's D-pad and face buttons behave identically in analog mode, so forcing analog mode on changes nothing for players who ignore the sticks; the `Digital` option remains the escape hatch for the rare analog-hostile game.
- **Deadzone:** `AnalogDeadzone=0` in the base layer means the core passes RetroNest's (already-deadzoned) axis values straight through — keep deadzone tuning in RetroNest, matching PCSX2/Dolphin.
- **L2/R2 stay digital** (PS1 DualShock shoulder buttons are not pressure-sensitive); they remain in the digital loop and the host's L2/R2 *axis* values are ignored.
- **Out of scope:** port 1 / 2-player, multitap, other peripherals, a runtime analog-mode toggle, SAVE_RAM.
