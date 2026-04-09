# DuckStation Completeness Design

Complete DuckStation adapter to match PCSX2's feature set: controller types with per-type bindings/settings/UI, in-game menu support, resume state compatibility, and expanded hotkeys.

## 1. Controller Types

Return 8 entries from `controllerTypes()`:

| ID | Display Name | SVG |
|---|---|---|
| `None` | Not Connected | (none) |
| `DigitalController` | Digital Controller | `digital_controller.svg` |
| `AnalogController` | Analog Controller | `analog_controller.svg` |
| `AnalogJoystick` | Analog Joystick | `analog_controller.svg` |
| `NeGcon` | NeGcon | `negcon.svg` |
| `NeGconRumble` | NeGcon (Rumble) | `analog_controller.svg` |
| `JogCon` | JogCon | `Jogcon.svg` (existing PCSX2) |
| `PopnController` | Pop'n Controller | `Popn.svg` (existing PCSX2) |

Excluded: GunCon, Justifier, PlayStationMouse, DDGoController (require pointer/mouse input unsupported by binding capture).

All types use `[Controller1]` INI section with `Type = {id}`.

## 2. Per-Type Bindings

**BUG FIX:** The existing `controllerBindingDefs()` uses wrong INI key names (`ButtonUp`, `ButtonCross`, `AxisLeftX`, etc.). DuckStation's actual INI keys are plain names: `Up`, `Cross`, `LeftX`, etc. The binding name from the source code IS the INI key. This must be fixed for all binding defs.

`controllerBindingDefsForType()` returns type-specific binding sets. All use DuckStation's actual INI key names in `[Controller1]` section.

### DigitalController (14 buttons, no axes)
- D-Pad: `Up`, `Down`, `Left`, `Right`
- Face: `Cross`, `Circle`, `Square`, `Triangle`
- Shoulders: `L1`, `R1`, `L2`, `R2`
- System: `Start`, `Select`

### AnalogController (22 bindings: 17 buttons + 2 axes + 1 toggle + 2 motors)
- D-Pad: `Up`, `Down`, `Left`, `Right`
- Face: `Cross`, `Circle`, `Square`, `Triangle`
- Shoulders: `L1`, `R1`, `L2`, `R2`
- Stick buttons: `L3`, `R3`
- System: `Start`, `Select`
- Special: `Analog` (analog toggle)
- Left stick half-axes: `LLeft`, `LRight`, `LUp`, `LDown`
- Right stick half-axes: `RLeft`, `RRight`, `RUp`, `RDown`
- Motors: `LargeMotor`, `SmallMotor`

### AnalogJoystick (20 bindings: 17 buttons + 2 axes + 1 toggle)
- Same buttons as AnalogController but `Mode` instead of `Analog`
- Same stick half-axes: `LLeft`/`LRight`/`LUp`/`LDown`, `RLeft`/`RRight`/`RUp`/`RDown`
- No motors

### NeGcon (13 bindings)
- D-Pad: `Up`, `Down`, `Left`, `Right`
- Buttons: `A`, `B`, `R`, `Start`
- Half-axes: `SteeringLeft`, `SteeringRight` (twist)
- Analog half-axes: `I`, `II`, `L`

### NeGconRumble (16 bindings + 2 motors)
- D-Pad: `Up`, `Down`, `Left`, `Right`
- Buttons: `A`, `B`, `R`, `Start`, `Analog`
- Half-axes: `SteeringLeft`, `SteeringRight`
- Analog half-axes: `I`, `II`, `L`
- Motors: `LargeMotor`, `SmallMotor`

### JogCon (17 bindings + motor)
- D-Pad: `Up`, `Down`, `Left`, `Right`
- Face: `Cross`, `Circle`, `Square`, `Triangle`
- Shoulders: `L1`, `R1`, `L2`, `R2`
- System: `Start`, `Select`, `Mode`
- Half-axes: `SteeringLeft`, `SteeringRight`
- Motor: `Motor`

### PopnController (11 buttons)
- Color buttons: `LeftWhite`, `LeftYellow`, `LeftGreen`, `LeftBlue`, `MiddleRed`, `RightBlue`, `RightGreen`, `RightYellow`, `RightWhite`
- System: `Select`, `Start`

## 3. Per-Type Settings

`controllerSettingDefsForType()` returns INI settings for each type:

### AnalogController (9 settings)
- `ForceAnalogOnReset` (bool, default true)
- `AnalogDPadInDigitalMode` (bool, default true)
- `AnalogDeadzone` (float 0.0-1.0, default 0)
- `AnalogSensitivity` (float 0.01-2.0, default 1.33)
- `ButtonDeadzone` (float 0.01-1.0, default 0.25)
- `InvertLeftStick` (combo 0-3: None/H-Flip/V-Flip/Both, default 0)
- `InvertRightStick` (combo 0-3, default 0)
- `LargeMotorVibrationBias` (int -255 to 255, default 8)
- `SmallMotorVibrationBias` (int -255 to 255, default 8)

### AnalogJoystick (4 settings)
- `AnalogDeadzone`, `AnalogSensitivity`, `InvertLeftStick`, `InvertRightStick`

### NeGcon (16 settings, 4 per analog axis)
- For each of Steering, I, II, L: `{Axis}Deadzone`, `{Axis}Saturation`, `{Axis}Linearity`, `{Axis}Scaling`

### NeGconRumble (4 settings)
- `SteeringDeadzone`, `SteeringSensitivity`, `LargeMotorVibrationBias`, `SmallMotorVibrationBias`

### JogCon (4 settings)
- `AnalogDeadzone`, `AnalogSensitivity`, `ButtonDeadzone`, `SteeringHoldDeadzone`

### DigitalController, PopnController
No settings.

## 4. Binding UI Widgets

7 new QWidget subclasses matching DuckStation's native layout. Each follows the existing pattern: absolute positioning via `relayout()`, BindBtn with capture, loadBindings/startCapture/finishCapture.

### SVG Assets
Copy from `references/duckstation/src/duckstation-qt/resources/controllers/`:
- `digital_controller.svg`
- `analog_controller.svg`
- `negcon.svg`

Reuse existing PCSX2 SVGs:
- `Jogcon.svg` (for JogCon)
- `Popn.svg` (for PopnController)

### New Files

| File | Type | Layout |
|---|---|---|
| `digital_bindings_widget.*` | DigitalController | 3-col: D-pad / SVG / face buttons. L1/L2/R1/R2 + Select/Start center. No sticks. |
| `analog_bindings_widget.*` | AnalogController | 3-col: D-pad+L-stick / SVG / face+R-stick. Shoulders, Select/Start, L3/R3/Analog center. Motors. |
| `analog_joystick_bindings_widget.*` | AnalogJoystick | Same as Analog but Mode instead of Analog, no motors. |
| `ds_negcon_bindings_widget.*` | NeGcon | 3-col: D-pad / SVG / A/B/R. Steering half-axes + I/II/L below. |
| `ds_negcon_rumble_bindings_widget.*` | NeGconRumble | NeGcon layout + Analog button + motors. |
| `ds_jogcon_bindings_widget.*` | JogCon | 3-col: D-pad / SVG / face. Shoulders, system, Mode, steering, motor. |
| `ds_popn_bindings_widget.*` | PopnController | Horizontal row of 9 color buttons + Start/Select. |

### controller_mapping_page.cpp Update
Add type-to-widget mapping in `createBindingsWidget()`:
```cpp
if (type == "DigitalController")  return new DigitalBindingsWidget(...)
if (type == "AnalogController")   return new AnalogBindingsWidget(...)
if (type == "AnalogJoystick")     return new AnalogJoystickBindingsWidget(...)
if (type == "NeGcon")             return new DSNegconBindingsWidget(...)
if (type == "NeGconRumble")       return new DSNegconRumbleBindingsWidget(...)
if (type == "JogCon")             return new DSJogconBindingsWidget(...)
if (type == "PopnController")     return new DSPopnBindingsWidget(...)
```

## 5. In-Game Menu Support

### INI Settings

| Section | Key | Value | Purpose |
|---|---|---|---|
| `Main` | `PauseOnFocusLoss` | `true` | Auto-pause when our app takes focus |
| `Main` | `SaveStateOnExit` | `true` | Save resume state on SIGTERM/graceful shutdown |
| `Hotkeys` | `OpenPauseMenu` | (empty) | Suppress native pause overlay |
| `Hotkeys` | `TogglePause` | (empty) | Suppress — app handles pause |
| `Hotkeys` | `ToggleFullscreen` | (empty) | Suppress — must stay fullscreen |

### createDefaultConfig() Changes
Add to `[Main]` section:
```ini
PauseOnFocusLoss = true
SaveStateOnExit = true
```

Add new `[Hotkeys]` section:
```ini
[Hotkeys]
OpenPauseMenu =
TogglePause =
ToggleFullscreen =
```

### patchExistingConfig() Changes
Add 5 keys to the `patchIniKeys()` call:
```cpp
{"Main", "PauseOnFocusLoss", "true"},
{"Main", "SaveStateOnExit", "true"},
{"Hotkeys", "OpenPauseMenu", ""},
{"Hotkeys", "TogglePause", ""},
{"Hotkeys", "ToggleFullscreen", ""},
```

## 6. Resume State Compatibility

### File Pattern Fix
DuckStation uses `{serial}_resume.sav` (underscore separator, not dot). Update `game_service.cpp` glob:
```cpp
dir.entryList({"*.resume.*", "*_resume.*"}, QDir::Files, QDir::Time)
```

### CLI Flag
DuckStation uses `-resume` (auto-finds game's resume state) not `-statefile <path>`.
`launchGameResume()` in `theme_context.cpp` is emulator-aware: DuckStation gets `{"-resume"}`,
PCSX2 gets `{"-statefile", stateFile}`. Extra args are inserted before `--` separator in
`game_session.cpp` so emulators treat them as flags, not filenames.

### SIGTERM Behavior
`SaveStateOnExit = true` triggers resume state save on graceful shutdown. DuckStation's Qt shutdown path calls `System::ShutdownSystem(save_state_on_exit)`, which SIGTERM triggers via Qt's `aboutToQuit`.

## 7. Expanded Hotkeys

Expand from 8 to ~38 hotkeys. TogglePause, ToggleFullscreen, and OpenPauseMenu are excluded (suppressed in INI, not exposed to user).

### Interface
- `Screenshot` — `Keyboard/F10`

### System
- `FastForward` — `Keyboard/Tab`
- `ToggleFastForward` — (none)
- `Turbo` — (none)
- `ToggleTurbo` — (none)
- `FrameStep` — (none)
- `Rewind` — (none)
- `IncreaseEmulationSpeed` — (none)
- `DecreaseEmulationSpeed` — (none)
- `ResetEmulationSpeed` — (none)
- `PowerOff` — (none)
- `Reset` — (none)
- `ChangeDisc` — (none)
- `SwitchToPreviousDisc` — (none)
- `SwitchToNextDisc` — (none)
- `SwapMemoryCards` — (none)

### Audio
- `AudioMute` — (none)
- `AudioCDAudioMute` — (none)
- `AudioVolumeUp` — (none)
- `AudioVolumeDown` — (none)

### Graphics
- `ToggleSoftwareRendering` — (none)
- `ToggleWidescreen` — (none)
- `TogglePostProcessing` — (none)
- `IncreaseResolutionScale` — (none)
- `DecreaseResolutionScale` — (none)

### Save States
- `LoadSelectedSaveState` — `Keyboard/F1`
- `SaveSelectedSaveState` — `Keyboard/F2`
- `SelectPreviousSaveStateSlot` — `Keyboard/F3`
- `SelectNextSaveStateSlot` — `Keyboard/F4`
- `SaveStateAndSelectNextSlot` — (none)
- `UndoLoadState` — (none)
- `LoadGameState1` through `LoadGameState10` — (none)
- `SaveGameState1` through `SaveGameState10` — (none)

All hotkeys go in `[Hotkeys]` INI section.

## Files to Modify

### Must modify:
- `cpp/src/adapters/duckstation_adapter.h` — add method declarations
- `cpp/src/adapters/duckstation_adapter.cpp` — implement controllerTypes(), controllerBindingDefsForType(), controllerSettingDefsForType(), expand hotkeyBindingDefs(), update createDefaultConfig()/patchExistingConfig()
- `cpp/src/services/game_service.cpp` — add `*_resume.*` to glob pattern
- `cpp/src/ui/settings/controller_mapping_page.cpp` — add DuckStation type branches in createBindingsWidget()
- `cpp/src/ui/settings/controller_mapping_page.h` — (no changes expected, forward declarations only)
- `cpp/CMakeLists.txt` — add new widget source/header files + SVG resources

### New files (14):
- `cpp/src/ui/settings/digital_bindings_widget.h`
- `cpp/src/ui/settings/digital_bindings_widget.cpp`
- `cpp/src/ui/settings/analog_bindings_widget.h`
- `cpp/src/ui/settings/analog_bindings_widget.cpp`
- `cpp/src/ui/settings/analog_joystick_bindings_widget.h`
- `cpp/src/ui/settings/analog_joystick_bindings_widget.cpp`
- `cpp/src/ui/settings/ds_negcon_bindings_widget.h`
- `cpp/src/ui/settings/ds_negcon_bindings_widget.cpp`
- `cpp/src/ui/settings/ds_negcon_rumble_bindings_widget.h`
- `cpp/src/ui/settings/ds_negcon_rumble_bindings_widget.cpp`
- `cpp/src/ui/settings/ds_jogcon_bindings_widget.h`
- `cpp/src/ui/settings/ds_jogcon_bindings_widget.cpp`
- `cpp/src/ui/settings/ds_popn_bindings_widget.h`
- `cpp/src/ui/settings/ds_popn_bindings_widget.cpp`

### SVG assets to copy (3):
- `digital_controller.svg`
- `analog_controller.svg`
- `negcon.svg` (DuckStation version — may differ from PCSX2's Negcon.svg)
