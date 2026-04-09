# PCSX2 Controller Mapping Page — Design Spec

## Overview

Enhance the Qt Widgets `ControllerMappingPage` dialog to match PCSX2's native controller settings interface. The dialog gains a left sidebar for port selection, a controller type dropdown that swaps the entire binding layout, a Settings tab alongside Bindings, profile management, and support for all 6 PCSX2 controller types.

## Scope

**In scope:**
- Left sidebar with Controller Port 1 and Port 2
- Controller type dropdown per port (Not Connected, DualShock 2, Guitar, Jogcon, NeGcon, Pop'n Music)
- Bindings tab with per-type visual layout and SVG controller image
- Settings tab with per-type controller settings (sliders/combos)
- Bottom profile bar (Editing Profile, New/Apply/Rename/Delete, Mapping Settings, Restore Defaults, Close)
- SVG images copied from PCSX2 references (GPL-3.0)

**Out of scope:**
- Macros tab (removed — low usage)
- USB ports (steering wheels, Buzz controllers, etc.)
- Hotkeys (separate existing page)

## Layout Structure

```
┌─────────────────────────────────────────────────────────────────┐
│  Left Sidebar  │  Toolbar: [Type ▼] [Bindings] [Settings]  ... │
│                │           ... [Automatic Mapping] [Clear]      │
│  Port 1  ◄──  │───────────────────────────────────────────────│
│  Port 2       │  Content Area (Bindings or Settings)           │
│                │                                                │
│                │                                                │
├────────────────┴────────────────────────────────────────────────┤
│  Profile: [Shared ▼] [+New] [Apply] [Rename] [Delete]          │
│                          [Mapping Settings] [Restore] [Close]   │
└─────────────────────────────────────────────────────────────────┘
```

## Left Sidebar

- Two entries: Controller Port 1, Controller Port 2
- Each shows a controller icon and the current type name (or "Not Connected")
- Selected port is highlighted (accent color background)
- Clicking a port loads that port's controller type, bindings, and settings into the right content area
- Port data stored in INI sections `[Pad1]` and `[Pad2]`

## Toolbar

- **Controller type dropdown** (QComboBox): Not Connected, DualShock 2, Guitar, Jogcon, NeGcon, Pop'n Music
  - Changing type: writes `Type` key to the port's INI section, swaps the bindings widget and settings widget, clears existing bindings for that port
  - "Not Connected" disables all tabs and shows empty content
- **Bindings tab** (QToolButton, checkable): shows the per-type binding layout
- **Settings tab** (QToolButton, checkable): shows the per-type settings form
- **Automatic Mapping** (QToolButton): visible only on Bindings tab. Opens QMenu with Keyboard + connected SDL controllers
- **Clear Mapping** (QToolButton): visible only on Bindings tab. Clears all bindings for current port

## Bindings Tab — Per-Type Layouts

### DualShock 2 (28 bindings)
Three-column layout matching PCSX2 native:
- **Left column:** D-Pad (diamond: Up/Left+Right/Down), Left Analog (diamond), Large Motor
- **Center:** L2/R2 row, L1/Select/Start/R1 row, SVG controller image (DualShock_2.svg), L3/Pressure Modifier/R3 row, Analog button
- **Right column:** Face Buttons (diamond: Triangle/Square+Circle/Cross), Right Analog (diamond), Small Motor

Bindings (all in `[Pad1]` or `[Pad2]`):
- D-Pad: Up, Down, Left, Right (Button)
- Face: Cross, Circle, Square, Triangle (Button)
- Shoulders: L1, R1 (Button)
- Triggers: L2, R2 (HalfAxis)
- Stick buttons: L3, R3 (Button)
- Left Stick: LUp, LDown, LLeft, LRight (HalfAxis)
- Right Stick: RUp, RDown, RLeft, RRight (HalfAxis)
- System: Start, Select (Button)
- Special: Analog, Pressure (Button)
- Motors: LargeMotor, SmallMotor (Motor)

### Guitar (11 bindings)
Centered SVG image (Guitar.svg) with flat button grid below:
- Start, Select, Green, Red, Yellow, Blue, Orange (Button)
- Strum Up, Strum Down (Button)
- Whammy (HalfAxis), Tilt (Button)
- No motors

### Jogcon (16 bindings)
Similar to DualShock 2 but with Dial Left/Right instead of analog sticks:
- D-Pad: Up, Down, Left, Right (Button)
- Face: Cross, Circle, Square, Triangle (Button)
- L1, L2, R1, R2 (Button — not HalfAxis for Jogcon)
- Select, Start (Button)
- Dial Left, Dial Right (HalfAxis)
- LargeMotor, SmallMotor (Motor)
- SVG: Jogcon.svg

### NeGcon (14 bindings)
Simplified layout with twist controls:
- D-Pad: Up, Down, Left, Right (Button)
- Face: A, B, I, II (Button)
- Start (Button), L, R (Button)
- Twist Left, Twist Right (HalfAxis)
- LargeMotor, SmallMotor (Motor)
- SVG: Negcon.svg

### Pop'n Music (11 bindings)
Flat row of colored buttons:
- Yellow Left/Right, Blue Left/Right, White Left/Right, Green Left/Right, Red (Button)
- Start, Select (Button)
- No motors, no analog
- SVG: Popn.svg

### Not Connected
Empty content area. All toolbar buttons disabled except the controller type dropdown.

## Settings Tab — Per-Type Settings

### DualShock 2 (8 settings, all in port's Pad section)

| Label | INI Key | Type | Default | Range | Suffix |
|-------|---------|------|---------|-------|--------|
| Invert Left Stick | InvertL | Combo | 0 | Not Inverted / X / Y / Both | |
| Invert Right Stick | InvertR | Combo | 0 | Not Inverted / X / Y / Both | |
| Analog Deadzone | Deadzone | Float slider | 0 | 0–100 | % |
| Analog Sensitivity | AxisScale | Float slider | 133 | 0–200 | % |
| Large Motor Vibration Scale | LargeMotorScale | Float slider | 100 | 0–200 | % |
| Small Motor Vibration Scale | SmallMotorScale | Float slider | 100 | 0–200 | % |
| Button/Trigger Deadzone | ButtonDeadzone | Float slider | 0 | 0–100 | % |
| Pressure Modifier Amount | PressureModifier | Float slider | 50 | 0–100 | % |

### Guitar (2 settings)

| Label | INI Key | Type | Default | Range | Suffix |
|-------|---------|------|---------|-------|--------|
| Whammy Bar Deadzone | Deadzone | Float slider | 0 | 0–100 | % |
| Whammy Bar Sensitivity | AxisScale | Float slider | 100 | 0–200 | % |

### Jogcon (2 settings)

| Label | INI Key | Type | Default | Range | Suffix |
|-------|---------|------|---------|-------|--------|
| Dial Deadzone | Deadzone | Float slider | 0 | 0–100 | % |
| Dial Sensitivity | AxisScale | Float slider | 100 | 0–200 | % |

### NeGcon (2 settings)

| Label | INI Key | Type | Default | Range | Suffix |
|-------|---------|------|---------|-------|--------|
| Twist Deadzone | Deadzone | Float slider | 0 | 0–100 | % |
| Twist Sensitivity | AxisScale | Float slider | 100 | 0–200 | % |

### Pop'n Music
No settings.

## Bottom Profile Bar

Manages controller mapping profiles stored as separate INI files.

- **Editing Profile** (QComboBox): "Shared" (default), plus any user-created profiles
- **+ New Profile**: Creates a new named profile (copies current bindings)
- **Apply Profile**: Loads the selected profile's bindings into the current port
- **Rename Profile**: Renames the selected profile
- **Delete Profile**: Deletes the selected profile (not "Shared")
- **Mapping Settings**: Opens a sub-dialog for mapping configuration (SDL options, input sources)
- **Restore Defaults**: Resets current port's bindings and settings to adapter defaults
- **Close**: Closes the dialog

Profile storage: `{root}/config/controller_profiles/{name}.ini` — each profile stores bindings for all ports.

"Shared" profile = the emulator's main config INI (existing behavior). Named profiles are separate files that get copied into the main INI on "Apply".

## Backend Changes Required

### PCSX2 Adapter

1. **Controller type support**: New `controllerTypes()` method returning available types with display names
2. **Per-type binding definitions**: `controllerBindingDefs()` already returns DualShock 2 bindings. Need to accept a `controllerType` parameter or add separate methods per type.
3. **Per-type settings**: `controllerSettingDefs()` similarly needs type awareness
4. **Per-type SVG path**: New method returning the resource path for the controller image
5. **Port support**: All binding/settings methods need a `port` parameter (1 or 2). INI section changes from hardcoded `Pad1` to `Pad{port}`.
6. **Type storage**: Read/write `Type` key in `[Pad{port}]` section

### AppController

1. **Port-aware APIs**: `controllerBindings(emuId, port)`, `controllerSettings(emuId, port)`, `saveBinding(emuId, port, ...)`, etc.
2. **Controller type management**: `controllerTypes(emuId)`, `setControllerType(emuId, port, type)`, `controllerType(emuId, port)`
3. **Profile management**: `controllerProfiles(emuId)`, `createProfile(emuId, name)`, `applyProfile(emuId, name)`, `renameProfile(emuId, old, new)`, `deleteProfile(emuId, name)`, `restoreDefaults(emuId, port)`

### New Adapter Methods (EmulatorAdapter base)

```cpp
// Controller type definitions
struct ControllerTypeDef {
    QString id;          // "DualShock2", "Guitar", etc.
    QString displayName; // "DualShock 2", "Guitar", etc.
    QString svgResource; // ":/images/DualShock_2.svg"
};

virtual QVector<ControllerTypeDef> controllerTypes() const { return {}; }
virtual QVector<BindingDef> controllerBindingDefs(const QString& type) const;
virtual QVector<SettingDef> controllerSettingDefs(const QString& type) const;
```

## SVG Resources

Copy from `references/pcsx2-master/pcsx2-qt/resources/images/`:
- DualShock_2.svg
- Guitar.svg
- Jogcon.svg
- Negcon.svg
- Popn.svg

Add to `cpp/qml/AppUI/images/controllers/` and register in CMakeLists.txt resources.

## INI Structure

```ini
[Pad1]
Type = DualShock2
Up = SDL-0/+DPadUp
Down = SDL-0/+DPadDown
...
LargeMotor = SDL-0/LargeMotor
SmallMotor = SDL-0/SmallMotor
Deadzone = 0
AxisScale = 133
InvertL = 0
...

[Pad2]
Type = NotConnected
```

## Dialog Sizing

- Minimum size: 1280 x 750 (same as current)
- Left sidebar width: 180px fixed
- Side binding columns: 24% of content width each
- Center column: fills remaining space
- Bottom bar: fixed height ~44px
