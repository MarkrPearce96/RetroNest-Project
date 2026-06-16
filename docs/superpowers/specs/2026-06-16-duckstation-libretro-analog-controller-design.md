# DuckStation libretro — Analog (DualShock) controller support

**Date:** 2026-06-16
**Status:** Design — approved, ready for plan
**Feature:** Upgrade the DuckStation libretro core's Player-1 controller from digital-only to a full **AnalogController (DualShock)**: analog sticks + rumble, with a Digital/Analog core option as an escape hatch.

## 1. Goal

The core currently boots a hardcoded `DigitalController` and `UpdateControllers()` reads only `RETRO_DEVICE_JOYPAD` digital buttons. PS1 games that use the analog sticks (e.g. Ape Escape, Spyro, Crash Team Racing) get no stick input, and there is no rumble. This feature wires up the existing-but-unused pieces so Player 1 is a DualShock by default.

This is a **core-wiring feature, not new emulation logic.** Both sides already have everything needed:
- DuckStation's `AnalogController` class is fully implemented (stick axes, analog-mode handling, rumble via `SetMotorState`).
- RetroNest already feeds the core `RETRO_DEVICE_ANALOG` axis values (deadzoned) via `CoreRuntime::inputStateTrampoline` (`core_runtime.cpp:129–177`) and already implements the libretro **rumble interface** (`RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE` → SDL haptics, `environment_callbacks.cpp`).

The core simply never asks for any of it.

## 2. Design decisions (from brainstorm)

- **Pad type:** `AnalogController` (DualShock) by **default**, with a `Digital/Analog` libretro **core option** (`duckstation_pad1_type`) as the escape hatch for the rare game that misbehaves with an analog pad. `retro_set_controller_port_device` stays a no-op.
- **Analog mode:** locked **on** at boot (sticks active immediately), **no runtime toggle**. The DualShock D-pad and face buttons work identically in analog mode, so players who ignore the sticks lose nothing.
- **Rumble:** included.
- **Ports:** **Player 1 only** (port 0). 2-player and multitap are out of scope.
- **Deadzone:** owned by **RetroNest** (it already deadzones the axes it returns, as it does for PCSX2/Dolphin). The emulated `AnalogController` stick deadzone is set to **0** so values pass straight through — deadzone tuning stays in one place.

## 3. Touch-points (architecture)

Five changes, four in the core + one in the RetroNest adapter:

1. **`UpdateControllers()` — `src/duckstation-libretro/libretro.cpp` (~:73–88).** After the existing digital-button loop, when the port-0 controller is analog-capable, read the four stick axes from `g_input_state(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT/RIGHT, RETRO_DEVICE_ID_ANALOG_X/Y)` and feed each into `AnalogController` via `SetBindState` on the analog **half-axis** bind indices.
2. **Controller type — `src/duckstation-libretro/libretro_settings.cpp` (~:391).** Replace the hardcoded `SetStringValue("Pad1","Type","DigitalController")` with a value driven by a new `duckstation_pad1_type` core option (default `AnalogController`), read in `ApplyCoreOptions()`.
3. **Analog-mode-on-at-boot — core controller creation.** Force the created `AnalogController` into analog mode at boot and do **not** expose the analog-button toggle bind. Also set its stick deadzone to 0 (deadzone owned by host). *(Exact setting keys / API resolved in the plan — see §7.)*
4. **Rumble — `libretro.cpp` + core `InputManager` vibration hook.** Request `RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE`, store the `set_rumble_state` callback, and route `AnalogController::SetMotorState` (via the core's `InputManager` vibration hook) to it.
5. **RetroNest adapter — `cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp`.** Add `AnalogController` to `controllerTypes()`, add the pad-type row to `settingsSchema()`, and keep `tools/check_schema_fidelity.py` green.

## 4. Data flow

**Sticks (new):** SDL/Qt thread → `InputRouter.setAxis` (atomic) → core thread `g_input_state(0, RETRO_DEVICE_ANALOG, …)` → RetroNest trampoline returns a **deadzoned** int16 (−32768…32767, center 0) → `UpdateControllers()` converts each axis to the active half-axis magnitude (0.0–1.0) → `AnalogController::SetBindState(HALFAXIS_BIND_START_INDEX + halfAxis, mag)` → core merges half-axes into LeftX/Y · RightX/Y (core deadzone 0) → emulated DualShock reports to the game.

**Rumble (new):** game writes vibration → `AnalogController::SetMotorState(motor, val)` → core `InputManager` vibration hook → stored libretro `set_rumble_state` → RetroNest `rumbleThunk → coreRuntimeSetRumbleMotor → SdlInputManager::setRumbleMotor` → SDL haptics.

**Digital buttons:** unchanged (existing `MapRetroPadToDigital` loop).

**Axis-mapping notes (for the plan):**
- `AnalogController` half-axis binds start at `HALFAXIS_BIND_START_INDEX` (= `Button::Count` = 17), ordered `LLeft, LRight, LDown, LUp, RLeft, RRight, RDown, RUp`. For each axis, set the active half-axis to `|value|/32767` and the opposite half-axis to 0.
- libretro analog **Y-positive = down** — map to the correct down/up half-axis.

## 5. Edge cases

- **Digital-only game:** user selects `Digital` in the core option → core creates a `DigitalController`; `UpdateControllers()` **gates** analog-axis feeding on the port-0 controller being analog-capable, so stick binds are never pushed at a digital pad.
- **Host reports no analog** (returns 0): sticks read centered → no movement. Harmless, no special handling.
- **Rumble on a `DigitalController`:** no motors → silently does nothing.
- **L2/R2 stay digital:** on PS1 the DualShock's L2/R2 are digital buttons (pressure sensitivity is a PS2 DualShock 2 feature). Only the four stick axes are analog; L2/R2 remain in the digital loop. The host's L2/R2 axis values are ignored.
- **Switching the Digital/Analog setting mid-session:** a controller-type change that reconfigures the controller via the existing #3 settings-apply path. Works, but it is a state change adjacent to the resume/save-state path — covered by the regression tests in §6.

## 6. Testing & verification

**Automated (agent-runnable):**
- `tools/check_schema_fidelity.py` stays green (new `duckstation_pad1_type` option ↔ `settingsSchema()` row match).
- Extend the #3 adapter QtTest (if present) to assert `AnalogController` is offered and the pad-type option round-trips; otherwise add a small standalone test of the axis→half-axis mapping math.
- Builds: core via `package.sh` (universal), RetroNest x86_64.

**User GUI verification (TCC blocks the agent):**
- **Sticks move** in a stick-using game — Ape Escape (both sticks), Spyro / Crash Team Racing (left stick).
- **D-pad/buttons unchanged** — digital-style players unaffected.
- **Rumble fires** in a vibration game (CTR, Crash, a racing title).
- **Digital/Analog setting:** switch to `Digital` → sticks dead, pad behaves as before; switch back to `Analog` → sticks return.
- **Regression:** save-and-exit → resume clean with the analog pad (the canonical landmine), plus a mid-session pad-type switch.

## 7. Open implementation details (resolved during planning — not design risks)

These are exact APIs to confirm by reading the core when writing the plan; the design intent is fixed:
- The exact mechanism/setting key to force `AnalogController` analog mode on at boot and to suppress the analog-button toggle.
- The exact setting key to set the emulated stick deadzone to 0.
- The exact core `InputManager` vibration hook to implement/route for libretro rumble (where `SetPadVibrationIntensity` lands in this build).
- The exact `duckstation_pad1_type` core-option definition site (mirror an existing #3 option).

## 8. Risks

- **Resume/save-state regression** — the autorelease-pool-before-dlclose crash is the standing landmine; re-verify save/resume with the analog pad and across a pad-type switch.
- **Deadzone double-application** — mitigated by design (core deadzone 0, host owns it); verify the stick "feel" isn't sluggish.
- **Game compatibility** — the rare analog-hostile game is handled by the `Digital` option; no per-game logic needed.

## 9. Out of scope

- 2-player (port 1) and multitap (3–8 players).
- Other PS1 peripherals (GunCon, NeGcon, mouse, etc.).
- A runtime analog-mode toggle (button/combo/hotkey).
- Pressure-sensitive buttons (PS2-only).

## 10. References

- **Core input:** `src/duckstation-libretro/libretro.cpp` (`UpdateControllers` ~:73, `retro_set_controller_port_device` stub :202), `libretro_map.cpp` (`MapRetroPadToDigital`), `libretro_settings.cpp` (hardcoded `Pad1/Type` ~:391, `ApplyCoreOptions`).
- **Controller classes:** `src/core/analog_controller.h/.cpp` (`SetBindState` half-axis indices, `SetMotorState`, `HALFAXIS_BIND_START_INDEX`, `Button::Analog`), `src/core/controller.h`, `types.h` (`ControllerType`).
- **Host input + rumble:** `RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp` (`inputStateTrampoline` :129–177, `coreRuntimeSetRumbleMotor`), `environment_callbacks.cpp` (`GET_RUMBLE_INTERFACE`), `input_router.h`.
- **Adapter / settings contract:** `cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp` (`controllerTypes`, `settingsSchema`), `tools/check_schema_fidelity.py`.
- **#3 settings spec** `…/specs/2026-06-04-duckstation-libretro-settings-design.md` (the core-option + settingsSchema + fidelity pattern this mirrors).
