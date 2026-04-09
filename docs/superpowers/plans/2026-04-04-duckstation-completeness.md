# DuckStation Completeness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete DuckStation adapter to match PCSX2's feature set — controller types, per-type bindings/settings/UI, in-game menu, resume state, expanded hotkeys.

**Architecture:** The DuckStation adapter (`duckstation_adapter.cpp`) gains new virtual method overrides for controller types, per-type bindings, and per-type settings. Seven new binding widget classes (one per controller type) are added under `ui/settings/`. Config methods are updated for in-game menu INI keys. The resume state glob in `game_service.cpp` is widened to match DuckStation's `_resume.sav` naming.

**Tech Stack:** C++17, Qt6 Widgets, SDL2, CMake

---

### Task 1: Fix existing binding key names and add controllerTypes()

The existing `controllerBindingDefs()` uses wrong INI key names (`ButtonUp`, `ButtonCross`, `AxisLeftX`). DuckStation uses plain names (`Up`, `Cross`, `LLeft`/`LRight`). This task fixes the bug and adds `controllerTypes()`.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.h`
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:635-665` (controllerBindingDefs)

- [ ] **Step 1: Add method declarations to header**

In `cpp/src/adapters/duckstation_adapter.h`, add these overrides after the existing `hotkeyBindingDefs()` declaration (line 31):

```cpp
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> controllerSettingDefsForType(const QString& type) const override;
```

- [ ] **Step 2: Fix controllerBindingDefs() key names**

In `cpp/src/adapters/duckstation_adapter.cpp`, replace the entire `controllerBindingDefs()` method (lines 635-666) with corrected INI key names. DuckStation uses plain names — the binding info `name` field in the source IS the INI key:

```cpp
QVector<BindingDef> DuckStationAdapter::controllerBindingDefs() const {
    return {
        // D-Pad
        {BindingDef::Button, "Up",       "D-Pad",        "Controller1", "Up",    "SDL-0/DPadUp"},
        {BindingDef::Button, "Down",     "D-Pad",        "Controller1", "Down",  "SDL-0/DPadDown"},
        {BindingDef::Button, "Left",     "D-Pad",        "Controller1", "Left",  "SDL-0/DPadLeft"},
        {BindingDef::Button, "Right",    "D-Pad",        "Controller1", "Right", "SDL-0/DPadRight"},
        // Face Buttons
        {BindingDef::Button, "Cross",    "Face Buttons",  "Controller1", "Cross",    "SDL-0/A"},
        {BindingDef::Button, "Circle",   "Face Buttons",  "Controller1", "Circle",   "SDL-0/B"},
        {BindingDef::Button, "Square",   "Face Buttons",  "Controller1", "Square",   "SDL-0/X"},
        {BindingDef::Button, "Triangle", "Face Buttons",  "Controller1", "Triangle", "SDL-0/Y"},
        // Shoulders
        {BindingDef::Button, "L1", "Shoulders", "Controller1", "L1", "SDL-0/LeftShoulder"},
        {BindingDef::Button, "R1", "Shoulders", "Controller1", "R1", "SDL-0/RightShoulder"},
        // Triggers
        {BindingDef::Axis,   "L2", "Triggers", "Controller1", "L2", "SDL-0/+LeftTrigger"},
        {BindingDef::Axis,   "R2", "Triggers", "Controller1", "R2", "SDL-0/+RightTrigger"},
        // Stick Buttons
        {BindingDef::Button, "L3", "Stick Buttons", "Controller1", "L3", "SDL-0/LeftStick"},
        {BindingDef::Button, "R3", "Stick Buttons", "Controller1", "R3", "SDL-0/RightStick"},
        // Left Stick (half-axes)
        {BindingDef::Axis, "Left Stick Left",  "Left Stick",  "Controller1", "LLeft",  "SDL-0/LeftX"},
        {BindingDef::Axis, "Left Stick Right", "Left Stick",  "Controller1", "LRight", "SDL-0/LeftX"},
        {BindingDef::Axis, "Left Stick Up",    "Left Stick",  "Controller1", "LUp",    "SDL-0/LeftY"},
        {BindingDef::Axis, "Left Stick Down",  "Left Stick",  "Controller1", "LDown",  "SDL-0/LeftY"},
        // Right Stick (half-axes)
        {BindingDef::Axis, "Right Stick Left",  "Right Stick", "Controller1", "RLeft",  "SDL-0/RightX"},
        {BindingDef::Axis, "Right Stick Right", "Right Stick", "Controller1", "RRight", "SDL-0/RightX"},
        {BindingDef::Axis, "Right Stick Up",    "Right Stick", "Controller1", "RUp",    "SDL-0/RightY"},
        {BindingDef::Axis, "Right Stick Down",  "Right Stick", "Controller1", "RDown",  "SDL-0/RightY"},
        // System
        {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
        {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
        // Toggle
        {BindingDef::Button, "Analog", "System", "Controller1", "Analog", ""},
        // Motors
        {BindingDef::Axis,   "LargeMotor", "Motors", "Controller1", "LargeMotor", ""},
        {BindingDef::Axis,   "SmallMotor", "Motors", "Controller1", "SmallMotor", ""},
    };
}
```

- [ ] **Step 3: Add controllerTypes()**

Add this method in `duckstation_adapter.cpp` right before `controllerBindingDefs()`:

```cpp
// ============================================================================
// Controller types
// ============================================================================

QVector<ControllerTypeDef> DuckStationAdapter::controllerTypes() const {
    return {
        {"None",              "Not Connected",    ""},
        {"DigitalController", "Digital Controller",":/AppUI/qml/AppUI/images/controllers/digital_controller.svg"},
        {"AnalogController",  "Analog Controller", ":/AppUI/qml/AppUI/images/controllers/analog_controller.svg"},
        {"AnalogJoystick",    "Analog Joystick",   ":/AppUI/qml/AppUI/images/controllers/analog_controller.svg"},
        {"NeGcon",            "NeGcon",            ":/AppUI/qml/AppUI/images/controllers/ds_negcon.svg"},
        {"NeGconRumble",      "NeGcon (Rumble)",   ":/AppUI/qml/AppUI/images/controllers/analog_controller.svg"},
        {"JogCon",            "JogCon",            ":/AppUI/qml/AppUI/images/controllers/Jogcon.svg"},
        {"PopnController",    "Pop'n Controller",  ":/AppUI/qml/AppUI/images/controllers/Popn.svg"},
    };
}
```

- [ ] **Step 4: Build and verify**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds (SVG files don't exist yet but are only loaded at runtime, not compile time).

- [ ] **Step 5: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.h cpp/src/adapters/duckstation_adapter.cpp
git commit -m "fix: correct DuckStation binding key names and add controllerTypes()"
```

---

### Task 2: Add controllerBindingDefsForType()

Per-type binding definitions for all 7 controller types.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp`

- [ ] **Step 1: Implement controllerBindingDefsForType()**

Add after `controllerBindingDefs()` in `duckstation_adapter.cpp`:

```cpp
QVector<BindingDef> DuckStationAdapter::controllerBindingDefsForType(const QString& type) const {
    if (type == "AnalogController")
        return controllerBindingDefs(); // default layout is AnalogController

    if (type == "DigitalController") {
        return {
            {BindingDef::Button, "Up",       "D-Pad",        "Controller1", "Up",       "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",     "D-Pad",        "Controller1", "Down",     "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",     "D-Pad",        "Controller1", "Left",     "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right",    "D-Pad",        "Controller1", "Right",    "SDL-0/DPadRight"},
            {BindingDef::Button, "Cross",    "Face Buttons",  "Controller1", "Cross",    "SDL-0/A"},
            {BindingDef::Button, "Circle",   "Face Buttons",  "Controller1", "Circle",   "SDL-0/B"},
            {BindingDef::Button, "Square",   "Face Buttons",  "Controller1", "Square",   "SDL-0/X"},
            {BindingDef::Button, "Triangle", "Face Buttons",  "Controller1", "Triangle", "SDL-0/Y"},
            {BindingDef::Button, "L1", "Shoulders", "Controller1", "L1", "SDL-0/LeftShoulder"},
            {BindingDef::Button, "R1", "Shoulders", "Controller1", "R1", "SDL-0/RightShoulder"},
            {BindingDef::Button, "L2", "Triggers",  "Controller1", "L2", "SDL-0/+LeftTrigger"},
            {BindingDef::Button, "R2", "Triggers",  "Controller1", "R2", "SDL-0/+RightTrigger"},
            {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
            {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
        };
    }

    if (type == "AnalogJoystick") {
        return {
            {BindingDef::Button, "Up",       "D-Pad",        "Controller1", "Up",       "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",     "D-Pad",        "Controller1", "Down",     "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",     "D-Pad",        "Controller1", "Left",     "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right",    "D-Pad",        "Controller1", "Right",    "SDL-0/DPadRight"},
            {BindingDef::Button, "Cross",    "Face Buttons",  "Controller1", "Cross",    "SDL-0/A"},
            {BindingDef::Button, "Circle",   "Face Buttons",  "Controller1", "Circle",   "SDL-0/B"},
            {BindingDef::Button, "Square",   "Face Buttons",  "Controller1", "Square",   "SDL-0/X"},
            {BindingDef::Button, "Triangle", "Face Buttons",  "Controller1", "Triangle", "SDL-0/Y"},
            {BindingDef::Button, "L1", "Shoulders", "Controller1", "L1", "SDL-0/LeftShoulder"},
            {BindingDef::Button, "R1", "Shoulders", "Controller1", "R1", "SDL-0/RightShoulder"},
            {BindingDef::Axis,   "L2", "Triggers",  "Controller1", "L2", "SDL-0/+LeftTrigger"},
            {BindingDef::Axis,   "R2", "Triggers",  "Controller1", "R2", "SDL-0/+RightTrigger"},
            {BindingDef::Button, "L3", "Stick Buttons", "Controller1", "L3", "SDL-0/LeftStick"},
            {BindingDef::Button, "R3", "Stick Buttons", "Controller1", "R3", "SDL-0/RightStick"},
            {BindingDef::Axis, "Left Stick Left",  "Left Stick",  "Controller1", "LLeft",  "SDL-0/LeftX"},
            {BindingDef::Axis, "Left Stick Right", "Left Stick",  "Controller1", "LRight", "SDL-0/LeftX"},
            {BindingDef::Axis, "Left Stick Up",    "Left Stick",  "Controller1", "LUp",    "SDL-0/LeftY"},
            {BindingDef::Axis, "Left Stick Down",  "Left Stick",  "Controller1", "LDown",  "SDL-0/LeftY"},
            {BindingDef::Axis, "Right Stick Left",  "Right Stick", "Controller1", "RLeft",  "SDL-0/RightX"},
            {BindingDef::Axis, "Right Stick Right", "Right Stick", "Controller1", "RRight", "SDL-0/RightX"},
            {BindingDef::Axis, "Right Stick Up",    "Right Stick", "Controller1", "RUp",    "SDL-0/RightY"},
            {BindingDef::Axis, "Right Stick Down",  "Right Stick", "Controller1", "RDown",  "SDL-0/RightY"},
            {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
            {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
            {BindingDef::Button, "Mode",   "System", "Controller1", "Mode",   ""},
        };
    }

    if (type == "NeGcon") {
        return {
            {BindingDef::Button, "Up",    "D-Pad",   "Controller1", "Up",    "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",  "D-Pad",   "Controller1", "Down",  "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",  "D-Pad",   "Controller1", "Left",  "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right", "D-Pad",   "Controller1", "Right", "SDL-0/DPadRight"},
            {BindingDef::Button, "A",     "Buttons",  "Controller1", "A",     "SDL-0/A"},
            {BindingDef::Button, "B",     "Buttons",  "Controller1", "B",     "SDL-0/B"},
            {BindingDef::Button, "R",     "Buttons",  "Controller1", "R",     "SDL-0/RightShoulder"},
            {BindingDef::Button, "Start", "System",   "Controller1", "Start", "SDL-0/Start"},
            {BindingDef::Axis,   "Steering Left",  "Steering", "Controller1", "SteeringLeft",  "SDL-0/LeftX"},
            {BindingDef::Axis,   "Steering Right", "Steering", "Controller1", "SteeringRight", "SDL-0/LeftX"},
            {BindingDef::Axis,   "I",  "Analog Buttons", "Controller1", "I",  "SDL-0/+RightTrigger"},
            {BindingDef::Axis,   "II", "Analog Buttons", "Controller1", "II", "SDL-0/+LeftTrigger"},
            {BindingDef::Axis,   "L",  "Analog Buttons", "Controller1", "L",  "SDL-0/LeftShoulder"},
        };
    }

    if (type == "NeGconRumble") {
        return {
            {BindingDef::Button, "Up",    "D-Pad",   "Controller1", "Up",    "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",  "D-Pad",   "Controller1", "Down",  "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",  "D-Pad",   "Controller1", "Left",  "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right", "D-Pad",   "Controller1", "Right", "SDL-0/DPadRight"},
            {BindingDef::Button, "A",     "Buttons",  "Controller1", "A",     "SDL-0/A"},
            {BindingDef::Button, "B",     "Buttons",  "Controller1", "B",     "SDL-0/B"},
            {BindingDef::Button, "R",     "Buttons",  "Controller1", "R",     "SDL-0/RightShoulder"},
            {BindingDef::Button, "Start", "System",   "Controller1", "Start", "SDL-0/Start"},
            {BindingDef::Button, "Analog","System",   "Controller1", "Analog",""},
            {BindingDef::Axis,   "Steering Left",  "Steering", "Controller1", "SteeringLeft",  "SDL-0/LeftX"},
            {BindingDef::Axis,   "Steering Right", "Steering", "Controller1", "SteeringRight", "SDL-0/LeftX"},
            {BindingDef::Axis,   "I",  "Analog Buttons", "Controller1", "I",  "SDL-0/+RightTrigger"},
            {BindingDef::Axis,   "II", "Analog Buttons", "Controller1", "II", "SDL-0/+LeftTrigger"},
            {BindingDef::Axis,   "L",  "Analog Buttons", "Controller1", "L",  "SDL-0/LeftShoulder"},
            {BindingDef::Axis,   "LargeMotor", "Motors", "Controller1", "LargeMotor", ""},
            {BindingDef::Axis,   "SmallMotor", "Motors", "Controller1", "SmallMotor", ""},
        };
    }

    if (type == "JogCon") {
        return {
            {BindingDef::Button, "Up",       "D-Pad",        "Controller1", "Up",       "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",     "D-Pad",        "Controller1", "Down",     "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",     "D-Pad",        "Controller1", "Left",     "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right",    "D-Pad",        "Controller1", "Right",    "SDL-0/DPadRight"},
            {BindingDef::Button, "Cross",    "Face Buttons",  "Controller1", "Cross",    "SDL-0/A"},
            {BindingDef::Button, "Circle",   "Face Buttons",  "Controller1", "Circle",   "SDL-0/B"},
            {BindingDef::Button, "Square",   "Face Buttons",  "Controller1", "Square",   "SDL-0/X"},
            {BindingDef::Button, "Triangle", "Face Buttons",  "Controller1", "Triangle", "SDL-0/Y"},
            {BindingDef::Button, "L1", "Shoulders", "Controller1", "L1", "SDL-0/LeftShoulder"},
            {BindingDef::Button, "R1", "Shoulders", "Controller1", "R1", "SDL-0/RightShoulder"},
            {BindingDef::Axis,   "L2", "Triggers",  "Controller1", "L2", "SDL-0/+LeftTrigger"},
            {BindingDef::Axis,   "R2", "Triggers",  "Controller1", "R2", "SDL-0/+RightTrigger"},
            {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
            {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
            {BindingDef::Button, "Mode",   "System", "Controller1", "Mode",   ""},
            {BindingDef::Axis,   "Steering Left",  "Steering", "Controller1", "SteeringLeft",  "SDL-0/LeftX"},
            {BindingDef::Axis,   "Steering Right", "Steering", "Controller1", "SteeringRight", "SDL-0/LeftX"},
            {BindingDef::Axis,   "Motor",          "Motors",   "Controller1", "Motor",          ""},
        };
    }

    if (type == "PopnController") {
        return {
            {BindingDef::Button, "Left White",   "Buttons", "Controller1", "LeftWhite",    ""},
            {BindingDef::Button, "Left Yellow",  "Buttons", "Controller1", "LeftYellow",   ""},
            {BindingDef::Button, "Left Green",   "Buttons", "Controller1", "LeftGreen",    ""},
            {BindingDef::Button, "Left Blue",    "Buttons", "Controller1", "LeftBlue",     ""},
            {BindingDef::Button, "Middle Red",   "Buttons", "Controller1", "MiddleRed",    ""},
            {BindingDef::Button, "Right Blue",   "Buttons", "Controller1", "RightBlue",    ""},
            {BindingDef::Button, "Right Green",  "Buttons", "Controller1", "RightGreen",   ""},
            {BindingDef::Button, "Right Yellow", "Buttons", "Controller1", "RightYellow",  ""},
            {BindingDef::Button, "Right White",  "Buttons", "Controller1", "RightWhite",   ""},
            {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
            {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
        };
    }

    // None or unknown
    return {};
}
```

- [ ] **Step 2: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "feat: add DuckStation per-type controller binding definitions"
```

---

### Task 3: Add controllerSettingDefsForType()

Per-type controller settings matching DuckStation's source defaults.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp`

- [ ] **Step 1: Implement controllerSettingDefsForType()**

Add after `controllerBindingDefsForType()` in `duckstation_adapter.cpp`:

```cpp
QVector<SettingDef> DuckStationAdapter::controllerSettingDefsForType(const QString& type) const {
    const QVector<QPair<QString,QString>> invertOpts = {
        {"None", "0"}, {"Horizontal Flip", "1"}, {"Vertical Flip", "2"}, {"Both", "3"},
    };

    if (type == "AnalogController") {
        return {
            {"", "", "", "Controller1", "ForceAnalogOnReset",
             "Force Analog On Reset", "Automatically enables analog mode on reset.",
             SettingDef::Bool, "true", {}, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "AnalogDPadInDigitalMode",
             "Use Analog Sticks for D-Pad in Digital Mode", "",
             SettingDef::Bool, "true", {}, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "AnalogDeadzone",
             "Analog Deadzone", "",
             SettingDef::Float, "0", {}, 0.0, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "AnalogSensitivity",
             "Analog Sensitivity", "",
             SettingDef::Float, "1.33", {}, 0.01, 2.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "ButtonDeadzone",
             "Button/Trigger Deadzone", "",
             SettingDef::Float, "0.25", {}, 0.01, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "InvertLeftStick",
             "Invert Left Stick", "",
             SettingDef::Combo, "0", invertOpts, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "InvertRightStick",
             "Invert Right Stick", "",
             SettingDef::Combo, "0", invertOpts, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "LargeMotorVibrationBias",
             "Large Motor Vibration Bias", "",
             SettingDef::Int, "8", {}, -255, 255, 1, "slider", ""},
            {"", "", "", "Controller1", "SmallMotorVibrationBias",
             "Small Motor Vibration Bias", "",
             SettingDef::Int, "8", {}, -255, 255, 1, "slider", ""},
        };
    }

    if (type == "AnalogJoystick") {
        return {
            {"", "", "", "Controller1", "AnalogDeadzone",
             "Analog Deadzone", "",
             SettingDef::Float, "0", {}, 0.0, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "AnalogSensitivity",
             "Analog Sensitivity", "",
             SettingDef::Float, "1.33", {}, 0.01, 2.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "InvertLeftStick",
             "Invert Left Stick", "",
             SettingDef::Combo, "0", invertOpts, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "InvertRightStick",
             "Invert Right Stick", "",
             SettingDef::Combo, "0", invertOpts, 0, 0, 0, "", ""},
        };
    }

    if (type == "NeGcon") {
        return {
            {"", "", "Steering", "Controller1", "SteeringDeadzone",    "Deadzone",    "", SettingDef::Float, "0",   {}, 0.0,   0.99, 0.01, "slider", ""},
            {"", "", "Steering", "Controller1", "SteeringSaturation",  "Saturation",  "", SettingDef::Float, "1",   {}, 0.01,  1.0,  0.01, "slider", ""},
            {"", "", "Steering", "Controller1", "SteeringLinearity",   "Linearity",   "", SettingDef::Float, "0",   {}, -2.0,  2.0,  0.01, "slider", ""},
            {"", "", "Steering", "Controller1", "SteeringScaling",     "Scaling",     "", SettingDef::Float, "1",   {}, 0.01, 10.0,  0.01, "slider", ""},
            {"", "", "I Button", "Controller1", "IDeadzone",     "Deadzone",    "", SettingDef::Float, "0",   {}, 0.0,   0.99, 0.01, "slider", ""},
            {"", "", "I Button", "Controller1", "ISaturation",   "Saturation",  "", SettingDef::Float, "1",   {}, 0.01,  1.0,  0.01, "slider", ""},
            {"", "", "I Button", "Controller1", "ILinearity",    "Linearity",   "", SettingDef::Float, "0",   {}, -2.0,  2.0,  0.01, "slider", ""},
            {"", "", "I Button", "Controller1", "IScaling",      "Scaling",     "", SettingDef::Float, "1",   {}, 0.01, 10.0,  0.01, "slider", ""},
            {"", "", "II Button","Controller1", "IIDeadzone",    "Deadzone",    "", SettingDef::Float, "0",   {}, 0.0,   0.99, 0.01, "slider", ""},
            {"", "", "II Button","Controller1", "IISaturation",  "Saturation",  "", SettingDef::Float, "1",   {}, 0.01,  1.0,  0.01, "slider", ""},
            {"", "", "II Button","Controller1", "IILinearity",   "Linearity",   "", SettingDef::Float, "0",   {}, -2.0,  2.0,  0.01, "slider", ""},
            {"", "", "II Button","Controller1", "IIScaling",     "Scaling",     "", SettingDef::Float, "1",   {}, 0.01, 10.0,  0.01, "slider", ""},
            {"", "", "L Trigger","Controller1", "LDeadzone",     "Deadzone",    "", SettingDef::Float, "0",   {}, 0.0,   0.99, 0.01, "slider", ""},
            {"", "", "L Trigger","Controller1", "LSaturation",   "Saturation",  "", SettingDef::Float, "1",   {}, 0.01,  1.0,  0.01, "slider", ""},
            {"", "", "L Trigger","Controller1", "LLinearity",    "Linearity",   "", SettingDef::Float, "0",   {}, -2.0,  2.0,  0.01, "slider", ""},
            {"", "", "L Trigger","Controller1", "LScaling",      "Scaling",     "", SettingDef::Float, "1",   {}, 0.01, 10.0,  0.01, "slider", ""},
        };
    }

    if (type == "NeGconRumble") {
        return {
            {"", "", "", "Controller1", "SteeringDeadzone",
             "Steering Deadzone", "",
             SettingDef::Float, "0", {}, 0.0, 0.99, 0.01, "slider", ""},
            {"", "", "", "Controller1", "SteeringSensitivity",
             "Steering Sensitivity", "",
             SettingDef::Float, "1", {}, 0.01, 2.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "LargeMotorVibrationBias",
             "Large Motor Vibration Bias", "",
             SettingDef::Int, "8", {}, -255, 255, 1, "slider", ""},
            {"", "", "", "Controller1", "SmallMotorVibrationBias",
             "Small Motor Vibration Bias", "",
             SettingDef::Int, "8", {}, -255, 255, 1, "slider", ""},
        };
    }

    if (type == "JogCon") {
        return {
            {"", "", "", "Controller1", "AnalogDeadzone",
             "Analog Deadzone", "",
             SettingDef::Float, "0", {}, 0.0, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "AnalogSensitivity",
             "Analog Sensitivity", "",
             SettingDef::Float, "1.33", {}, 0.01, 2.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "ButtonDeadzone",
             "Button Deadzone", "",
             SettingDef::Float, "0.25", {}, 0.01, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "SteeringHoldDeadzone",
             "Steering Hold Deadzone", "",
             SettingDef::Float, "0.03", {}, 0.01, 1.0, 0.01, "slider", ""},
        };
    }

    // DigitalController, PopnController, None — no settings
    return {};
}
```

- [ ] **Step 2: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "feat: add DuckStation per-type controller settings"
```

---

### Task 4: In-game menu INI settings

Add PauseOnFocusLoss, SaveStateOnExit, and hotkey suppression to config methods.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:480-570`

- [ ] **Step 1: Update createDefaultConfig()**

In `duckstation_adapter.cpp`, find the `createDefaultConfig()` method. Add `PauseOnFocusLoss` and `SaveStateOnExit` to the `[Main]` section, and add a `[Hotkeys]` section. Replace the `QStringList lines` block (lines 496-523):

```cpp
    QStringList lines = {
        "[Main]",
        "SetupWizardComplete = true",
        "SetupWizardIncomplete = false",
        "ConfirmPowerOff = false",
        "StartFullscreen = true",
        "PauseOnFocusLoss = true",
        "SaveStateOnExit = true",
        "",
        "[BIOS]",
        "SearchDirectory = " + biosPath,
        "",
        "[Display]",
        "Fullscreen = true",
        "",
        "[MemoryCards]",
        "Directory = " + memcardsPath,
        "Card1Path = " + memcardsPath + "/shared_card_1.mcd",
        "Card2Path = " + memcardsPath + "/shared_card_2.mcd",
        "",
        "[Folders]",
        "SaveStates = " + savestatesPath,
        "Screenshots = " + screenshotsPath,
        "Cache = " + cachePath,
        "Cheats = " + cheatsPath,
        "Textures = " + texturesPath,
        "",
        "[Hotkeys]",
        "OpenPauseMenu =",
        "TogglePause =",
        "ToggleFullscreen =",
        "",
        "[Controller1]",
        "",
    };
```

- [ ] **Step 2: Update patchExistingConfig()**

In the `patchExistingConfig()` method, add the in-game menu keys to the existing `QVector<IniKeyPatch> folders` block. After the existing folder patches (line 555), add:

```cpp
    // Ensure in-game menu settings
    QVector<IniKeyPatch> inGamePatches = {
        {"Main", "PauseOnFocusLoss", "true"},
        {"Main", "SaveStateOnExit", "true"},
        {"Hotkeys", "OpenPauseMenu", ""},
        {"Hotkeys", "TogglePause", ""},
        {"Hotkeys", "ToggleFullscreen", ""},
    };
    if (patchIniKeys(content, inGamePatches))
        changed = true;
```

- [ ] **Step 3: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "feat: add DuckStation in-game menu INI settings"
```

---

### Task 5: Resume state glob fix

Widen the resume state glob to match DuckStation's `_resume.sav` pattern.

**Files:**
- Modify: `cpp/src/services/game_service.cpp:36`

- [ ] **Step 1: Update the glob pattern**

In `game_service.cpp`, find line 36:
```cpp
                    for (const auto& entry : dir.entryList({"*.resume.*"}, QDir::Files, QDir::Time)) {
```

Replace with:
```cpp
                    for (const auto& entry : dir.entryList({"*.resume.*", "*_resume.*"}, QDir::Files, QDir::Time)) {
```

- [ ] **Step 2: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/services/game_service.cpp
git commit -m "fix: widen resume state glob to match DuckStation _resume.sav pattern"
```

---

### Task 6: Expanded hotkeys

Replace the 8-hotkey list with ~38 hotkeys across all DuckStation categories.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:668-679`

- [ ] **Step 1: Replace hotkeyBindingDefs()**

Replace the entire `hotkeyBindingDefs()` method:

```cpp
QVector<HotkeyDef> DuckStationAdapter::hotkeyBindingDefs() const {
    return {
        // Interface
        {"Screenshot",              "Interface",   "Hotkeys", "Screenshot",              "Keyboard/F10"},

        // System
        {"Fast Forward (Hold)",     "System",      "Hotkeys", "FastForward",             "Keyboard/Tab"},
        {"Fast Forward (Toggle)",   "System",      "Hotkeys", "ToggleFastForward",       ""},
        {"Turbo (Hold)",            "System",      "Hotkeys", "Turbo",                   ""},
        {"Turbo (Toggle)",          "System",      "Hotkeys", "ToggleTurbo",             ""},
        {"Frame Step",              "System",      "Hotkeys", "FrameStep",               ""},
        {"Rewind",                  "System",      "Hotkeys", "Rewind",                  ""},
        {"Increase Emulation Speed","System",      "Hotkeys", "IncreaseEmulationSpeed",  ""},
        {"Decrease Emulation Speed","System",      "Hotkeys", "DecreaseEmulationSpeed",  ""},
        {"Reset Emulation Speed",   "System",      "Hotkeys", "ResetEmulationSpeed",     ""},
        {"Power Off",               "System",      "Hotkeys", "PowerOff",                ""},
        {"Reset",                   "System",      "Hotkeys", "Reset",                   ""},
        {"Change Disc",             "System",      "Hotkeys", "ChangeDisc",              ""},
        {"Previous Disc",           "System",      "Hotkeys", "SwitchToPreviousDisc",    ""},
        {"Next Disc",               "System",      "Hotkeys", "SwitchToNextDisc",        ""},
        {"Swap Memory Cards",       "System",      "Hotkeys", "SwapMemoryCards",         ""},

        // Audio
        {"Mute Audio",              "Audio",       "Hotkeys", "AudioMute",               ""},
        {"Mute CD Audio",           "Audio",       "Hotkeys", "AudioCDAudioMute",        ""},
        {"Volume Up",               "Audio",       "Hotkeys", "AudioVolumeUp",           ""},
        {"Volume Down",             "Audio",       "Hotkeys", "AudioVolumeDown",         ""},

        // Graphics
        {"Toggle Software Rendering","Graphics",   "Hotkeys", "ToggleSoftwareRendering", ""},
        {"Toggle Widescreen",       "Graphics",    "Hotkeys", "ToggleWidescreen",        ""},
        {"Toggle Post-Processing",  "Graphics",    "Hotkeys", "TogglePostProcessing",    ""},
        {"Increase Resolution",     "Graphics",    "Hotkeys", "IncreaseResolutionScale", ""},
        {"Decrease Resolution",     "Graphics",    "Hotkeys", "DecreaseResolutionScale", ""},

        // Save States
        {"Load Selected State",     "Save States", "Hotkeys", "LoadSelectedSaveState",          "Keyboard/F1"},
        {"Save Selected State",     "Save States", "Hotkeys", "SaveSelectedSaveState",          "Keyboard/F2"},
        {"Previous Save Slot",      "Save States", "Hotkeys", "SelectPreviousSaveStateSlot",    "Keyboard/F3"},
        {"Next Save Slot",          "Save States", "Hotkeys", "SelectNextSaveStateSlot",        "Keyboard/F4"},
        {"Save & Next Slot",        "Save States", "Hotkeys", "SaveStateAndSelectNextSlot",     ""},
        {"Undo Load State",         "Save States", "Hotkeys", "UndoLoadState",                  ""},
        {"Load Game State 1",       "Save States", "Hotkeys", "LoadGameState1",  ""},
        {"Save Game State 1",       "Save States", "Hotkeys", "SaveGameState1",  ""},
        {"Load Game State 2",       "Save States", "Hotkeys", "LoadGameState2",  ""},
        {"Save Game State 2",       "Save States", "Hotkeys", "SaveGameState2",  ""},
        {"Load Game State 3",       "Save States", "Hotkeys", "LoadGameState3",  ""},
        {"Save Game State 3",       "Save States", "Hotkeys", "SaveGameState3",  ""},
        {"Load Game State 4",       "Save States", "Hotkeys", "LoadGameState4",  ""},
        {"Save Game State 4",       "Save States", "Hotkeys", "SaveGameState4",  ""},
        {"Load Game State 5",       "Save States", "Hotkeys", "LoadGameState5",  ""},
        {"Save Game State 5",       "Save States", "Hotkeys", "SaveGameState5",  ""},
        {"Load Game State 6",       "Save States", "Hotkeys", "LoadGameState6",  ""},
        {"Save Game State 6",       "Save States", "Hotkeys", "SaveGameState6",  ""},
        {"Load Game State 7",       "Save States", "Hotkeys", "LoadGameState7",  ""},
        {"Save Game State 7",       "Save States", "Hotkeys", "SaveGameState7",  ""},
        {"Load Game State 8",       "Save States", "Hotkeys", "LoadGameState8",  ""},
        {"Save Game State 8",       "Save States", "Hotkeys", "SaveGameState8",  ""},
        {"Load Game State 9",       "Save States", "Hotkeys", "LoadGameState9",  ""},
        {"Save Game State 9",       "Save States", "Hotkeys", "SaveGameState9",  ""},
        {"Load Game State 10",      "Save States", "Hotkeys", "LoadGameState10", ""},
        {"Save Game State 10",      "Save States", "Hotkeys", "SaveGameState10", ""},
    };
}
```

- [ ] **Step 2: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "feat: expand DuckStation hotkeys to ~50 across all categories"
```

---

### Task 7: Copy SVG assets and update CMakeLists.txt

Copy DuckStation controller SVGs and register them as Qt resources.

**Files:**
- Copy: 3 SVG files to `cpp/qml/AppUI/images/controllers/`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Copy SVG files**

```bash
cp references/duckstation/src/duckstation-qt/resources/controllers/digital_controller.svg cpp/qml/AppUI/images/controllers/digital_controller.svg
cp references/duckstation/src/duckstation-qt/resources/controllers/analog_controller.svg cpp/qml/AppUI/images/controllers/analog_controller.svg
cp references/duckstation/src/duckstation-qt/resources/controllers/negcon.svg cpp/qml/AppUI/images/controllers/ds_negcon.svg
```

Note: DuckStation's `negcon.svg` is copied as `ds_negcon.svg` to avoid conflict with PCSX2's existing `Negcon.svg`.

- [ ] **Step 2: Add SVGs to CMakeLists.txt resources**

In `cpp/CMakeLists.txt`, find the SVG resource block (around line 217-221). Add the 3 new SVGs after the existing `Popn.svg` line:

```cmake
        qml/AppUI/images/controllers/digital_controller.svg
        qml/AppUI/images/controllers/analog_controller.svg
        qml/AppUI/images/controllers/ds_negcon.svg
```

- [ ] **Step 3: Build and verify**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/images/controllers/digital_controller.svg cpp/qml/AppUI/images/controllers/analog_controller.svg cpp/qml/AppUI/images/controllers/ds_negcon.svg cpp/CMakeLists.txt
git commit -m "feat: add DuckStation controller SVG assets"
```

---

### Task 8: DigitalController binding widget

The simplest layout — 14 buttons, no sticks, no motors.

**Files:**
- Create: `cpp/src/ui/settings/digital_bindings_widget.h`
- Create: `cpp/src/ui/settings/digital_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create header**

Create `cpp/src/ui/settings/digital_bindings_widget.h`:

```cpp
#pragma once

#include <QWidget>
#include <QMap>
#include <QPushButton>
#include <QLabel>
#include <QString>

class SdlInputManager;
class AppController;

class DigitalBindingsWidget : public QWidget {
    Q_OBJECT
public:
    DigitalBindingsWidget(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          int port,
                          QWidget* parent = nullptr);

    void loadBindings();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void relayout();
    void startCapture(const QString& label);
    void onBindingCaptured(int deviceIndex, const QString& element, bool isAxis, bool positive);
    void onKeyboardCaptured(const QString& keyString);
    void finishCapture(const QString& formatted);

    SdlInputManager* m_inputManager;
    AppController* m_appController;
    QString m_emuId;
    int m_port;

    QLabel* m_imgLabel = nullptr;
    QWidget* m_dpadBox = nullptr;
    QWidget* m_faceBox = nullptr;

    QMap<QString, QPushButton*> m_bindingButtons;
    QString m_capturingLabel;
};
```

- [ ] **Step 2: Create implementation**

Create `cpp/src/ui/settings/digital_bindings_widget.cpp`:

```cpp
#include "digital_bindings_widget.h"
#include "binding_widget_common.h"
#include "binding_display.h"
#include "core/sdl_input_manager.h"
#include "ui/app_controller.h"

#include <QResizeEvent>
#include <QPixmap>
#include <QMouseEvent>

static constexpr int kTy = 16;

#define LBL(name) findChild<QLabel*>(name)

DigitalBindingsWidget::DigitalBindingsWidget(SdlInputManager* inputManager,
                                             AppController* appController,
                                             const QString& emuId,
                                             int port,
                                             QWidget* parent)
    : QWidget(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_port(port)
{
    setStyleSheet(QString("background: %1;").arg(kBg));

    auto* canvas = this;

    // -- LEFT: D-Pad --
    auto* dpadTitle = makeLabel(canvas, "D-Pad", 15, true);
    m_dpadBox = makeBox(canvas);
    auto* dpadUp_l = makeLabel(canvas, "Up");
    auto* dpadUp   = new BindBtn(canvas);
    auto* dpadL_l  = makeLabel(canvas, "Left");
    auto* dpadR_l  = makeLabel(canvas, "Right");
    auto* dpadLeft = new BindBtn(canvas);
    auto* dpadRight= new BindBtn(canvas);
    auto* dpadDn_l = makeLabel(canvas, "Down");
    auto* dpadDown = new BindBtn(canvas);

    // -- RIGHT: Face Buttons --
    auto* fbTitle  = makeLabel(canvas, "Face Buttons", 15, true);
    m_faceBox      = makeBox(canvas);
    auto* triLbl   = makeLabel(canvas, "Triangle");
    auto* tri      = new BindBtn(canvas);
    auto* sqLbl    = makeLabel(canvas, "Square");
    auto* cirLbl   = makeLabel(canvas, "Circle");
    auto* sq       = new BindBtn(canvas);
    auto* cir      = new BindBtn(canvas);
    auto* crossLbl = makeLabel(canvas, "Cross");
    auto* cross    = new BindBtn(canvas);

    // -- CENTER --
    auto* l2Lbl    = makeLabel(canvas, "L2");
    auto* l2       = new BindBtn(canvas);
    auto* r2Lbl    = makeLabel(canvas, "R2");
    auto* r2       = new BindBtn(canvas);
    auto* l1Lbl    = makeLabel(canvas, "L1");
    auto* l1       = new BindBtn(canvas);
    auto* selLbl   = makeLabel(canvas, "Select");
    auto* sel      = new BindBtn(canvas);
    auto* startLbl = makeLabel(canvas, "Start");
    auto* start    = new BindBtn(canvas);
    auto* r1Lbl    = makeLabel(canvas, "R1");
    auto* r1       = new BindBtn(canvas);

    // Controller image
    m_imgLabel = new QLabel(canvas);
    QPixmap pix(":/AppUI/qml/AppUI/images/controllers/digital_controller.svg");
    if (!pix.isNull())
        m_imgLabel->setPixmap(pix.scaled(420, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imgLabel->setAlignment(Qt::AlignCenter);
    m_imgLabel->setStyleSheet("background: transparent; border: none;");

    // Style all binding buttons
    auto setupBtn = [&](BindBtn* b, const QString& label) {
        b->setStyleSheet(kBtnStyle);
        b->setCursor(Qt::PointingHandCursor);
        b->setText("Not bound");
        b->setFixedHeight(kBtnH);
        m_bindingButtons[label] = b;
        connect(b, &QPushButton::clicked, this, [this, label]() { startCapture(label); });
        b->onRightClick = [this, label]() {
            auto* btn = m_bindingButtons[label];
            m_appController->clearBindingForPort(m_emuId, m_port,
                btn->property("iniKey").toString());
            loadBindings();
        };
    };

    setupBtn(dpadUp, "Up"); setupBtn(dpadDown, "Down");
    setupBtn(dpadLeft, "Left"); setupBtn(dpadRight, "Right");
    setupBtn(tri, "Triangle"); setupBtn(cross, "Cross");
    setupBtn(sq, "Square"); setupBtn(cir, "Circle");
    setupBtn(l2, "L2"); setupBtn(r2, "R2");
    setupBtn(l1, "L1"); setupBtn(r1, "R1");
    setupBtn(sel, "Select"); setupBtn(start, "Start");

    connect(m_inputManager, &SdlInputManager::bindingCaptured,
            this, &DigitalBindingsWidget::onBindingCaptured);
    connect(m_inputManager, &SdlInputManager::keyboardCaptured,
            this, &DigitalBindingsWidget::onKeyboardCaptured);

    dpadTitle->setObjectName("dpadTitle");
    dpadUp_l->setObjectName("dpadUp_l"); dpadL_l->setObjectName("dpadL_l");
    dpadR_l->setObjectName("dpadR_l"); dpadDn_l->setObjectName("dpadDn_l");
    fbTitle->setObjectName("fbTitle");
    triLbl->setObjectName("triLbl"); sqLbl->setObjectName("sqLbl");
    cirLbl->setObjectName("cirLbl"); crossLbl->setObjectName("crossLbl");
    l2Lbl->setObjectName("l2Lbl"); r2Lbl->setObjectName("r2Lbl");
    l1Lbl->setObjectName("l1Lbl"); r1Lbl->setObjectName("r1Lbl");
    selLbl->setObjectName("selLbl"); startLbl->setObjectName("startLbl");

    loadBindings();
    relayout();
}

void DigitalBindingsWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayout();
}

void DigitalBindingsWidget::relayout() {
    int W = width();
    int margin = 16;
    int colW = qMax(280, static_cast<int>(W * 0.24));
    int btnW = qMin(160, colW - 40);
    int btnSmW = qMin(130, (colW - 30) / 2);
    int boxH = 230;

    int lCol = margin;
    int rColX = W - margin - colW;
    int cx = W / 2;
    int cLeft = lCol + colW + 10;
    int cRight = rColX - 10;

    // ── LEFT: D-Pad ──
    int lMid = lCol + colW / 2;
    if (auto* t = LBL("dpadTitle")) t->move(lCol, kTy + 16);
    m_dpadBox->setGeometry(lCol, kTy + 38, colW, boxH);

    if (auto* l = LBL("dpadUp_l")) l->move(lMid - 10, kTy + 56);
    m_bindingButtons["Up"]->setGeometry(lMid - btnW/2, kTy + 74, btnW, kBtnH);

    if (auto* l = LBL("dpadL_l")) l->move(lCol + 12, kTy + 120);
    if (auto* l = LBL("dpadR_l")) l->move(lCol + colW - 40, kTy + 120);
    m_bindingButtons["Left"]->setGeometry(lCol + 10, kTy + 138, btnSmW, kBtnH);
    m_bindingButtons["Right"]->setGeometry(lCol + colW - btnSmW - 10, kTy + 138, btnSmW, kBtnH);

    if (auto* l = LBL("dpadDn_l")) l->move(lMid - 15, kTy + 184);
    m_bindingButtons["Down"]->setGeometry(lMid - btnW/2, kTy + 202, btnW, kBtnH);

    // ── RIGHT: Face Buttons ──
    int rMid = rColX + colW / 2;
    if (auto* t = LBL("fbTitle")) t->move(rColX + 6, kTy + 16);
    m_faceBox->setGeometry(rColX, kTy + 38, colW, boxH);

    if (auto* l = LBL("triLbl")) l->move(rMid - 25, kTy + 56);
    m_bindingButtons["Triangle"]->setGeometry(rMid - btnW/2, kTy + 74, btnW, kBtnH);

    if (auto* l = LBL("sqLbl")) l->move(rColX + 14, kTy + 120);
    if (auto* l = LBL("cirLbl")) l->move(rColX + colW - 46, kTy + 120);
    m_bindingButtons["Square"]->setGeometry(rColX + 10, kTy + 138, btnSmW, kBtnH);
    m_bindingButtons["Circle"]->setGeometry(rColX + colW - btnSmW - 10, kTy + 138, btnSmW, kBtnH);

    if (auto* l = LBL("crossLbl")) l->move(rMid - 18, kTy + 184);
    m_bindingButtons["Cross"]->setGeometry(rMid - btnW/2, kTy + 202, btnW, kBtnH);

    // ── CENTER: L2/R2 ──
    if (auto* l = LBL("l2Lbl")) l->move(cLeft + 10, kTy + 16);
    m_bindingButtons["L2"]->setGeometry(cLeft, kTy + 36, btnW, kBtnH);

    if (auto* l = LBL("r2Lbl")) l->move(cRight - btnW + 10, kTy + 16);
    m_bindingButtons["R2"]->setGeometry(cRight - btnW, kTy + 36, btnW, kBtnH);

    // ── CENTER: L1, Select, Start, R1 ──
    if (auto* l = LBL("l1Lbl")) l->move(cLeft + 10, kTy + 100);
    m_bindingButtons["L1"]->setGeometry(cLeft, kTy + 120, btnSmW, kBtnH);

    if (auto* l = LBL("selLbl")) l->move(cx - 70, kTy + 100);
    m_bindingButtons["Select"]->setGeometry(cx - btnSmW - 5, kTy + 120, btnSmW, kBtnH);

    if (auto* l = LBL("startLbl")) l->move(cx + 30, kTy + 100);
    m_bindingButtons["Start"]->setGeometry(cx + 5, kTy + 120, btnSmW, kBtnH);

    if (auto* l = LBL("r1Lbl")) l->move(cRight - btnSmW + 10, kTy + 100);
    m_bindingButtons["R1"]->setGeometry(cRight - btnSmW, kTy + 120, btnSmW, kBtnH);

    // ── CENTER: Controller image ──
    int imgW = qMin(420, cRight - cLeft);
    int imgH = static_cast<int>(imgW * 0.71);
    m_imgLabel->setGeometry(cx - imgW/2, kTy + 180, imgW, imgH);
}

void DigitalBindingsWidget::loadBindings() {
    QVariantList bindings = m_appController->controllerBindingsForPort(m_emuId, m_port);
    for (const auto& b : bindings) {
        auto map = b.toMap();
        QString label = map["label"].toString();
        QString currentValue = map["currentValue"].toString();
        auto it = m_bindingButtons.find(label);
        if (it != m_bindingButtons.end()) {
            QPushButton* btn = it.value();
            auto detailedType = SdlInputManager::TypeStandard;
            int devIdx = deviceIndexFromBinding(currentValue);
            if (devIdx >= 0)
                detailedType = m_inputManager->detailedControllerTypeForDevice(devIdx);
            QString display = displayBinding(currentValue, detailedType);
            btn->setText(display.isEmpty() ? "Not bound" : display);
            btn->setStyleSheet(kBtnStyle);
            btn->setProperty("iniKey", map["key"].toString());
        }
    }
}

void DigitalBindingsWidget::startCapture(const QString& label) {
    if (!m_capturingLabel.isEmpty()) loadBindings();
    m_capturingLabel = label;
    auto it = m_bindingButtons.find(label);
    if (it != m_bindingButtons.end()) {
        it.value()->setText("Press a button...");
        it.value()->setStyleSheet(kCapturingStyle);
    }
    m_inputManager->startCapture();
}

void DigitalBindingsWidget::onBindingCaptured(int deviceIndex, const QString& element,
                                               bool isAxis, bool positive) {
    if (m_capturingLabel.isEmpty()) return;
    finishCapture(m_appController->formatCapturedBinding(m_emuId, deviceIndex, element, isAxis, positive));
}

void DigitalBindingsWidget::onKeyboardCaptured(const QString& keyString) {
    if (m_capturingLabel.isEmpty()) return;
    finishCapture(keyString);
}

void DigitalBindingsWidget::finishCapture(const QString& formatted) {
    auto it = m_bindingButtons.find(m_capturingLabel);
    if (it != m_bindingButtons.end()) {
        auto* btn = it.value();
        m_appController->saveBindingForPort(m_emuId, m_port,
                                            btn->property("iniKey").toString(), formatted);
    }
    m_capturingLabel.clear();
    loadBindings();
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `cpp/CMakeLists.txt`, add to SOURCES (after `popn_bindings_widget.cpp`, around line 48):
```cmake
    src/ui/settings/digital_bindings_widget.cpp
```

Add to HEADERS (after `popn_bindings_widget.h`, around line 91):
```cmake
    src/ui/settings/digital_bindings_widget.h
```

- [ ] **Step 4: Build and verify**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/settings/digital_bindings_widget.h cpp/src/ui/settings/digital_bindings_widget.cpp cpp/CMakeLists.txt
git commit -m "feat: add DigitalController binding widget"
```

---

### Task 9: AnalogController binding widget

Same layout as DS2 but with DuckStation's half-axis sticks, Analog toggle, and motors.

**Files:**
- Create: `cpp/src/ui/settings/analog_bindings_widget.h`
- Create: `cpp/src/ui/settings/analog_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create header**

Create `cpp/src/ui/settings/analog_bindings_widget.h` — identical structure to `digital_bindings_widget.h` but class name `AnalogBindingsWidget`, with additional member variables `m_lAnalogBox` and `m_rAnalogBox`:

```cpp
#pragma once

#include <QWidget>
#include <QMap>
#include <QPushButton>
#include <QLabel>
#include <QString>

class SdlInputManager;
class AppController;

class AnalogBindingsWidget : public QWidget {
    Q_OBJECT
public:
    AnalogBindingsWidget(SdlInputManager* inputManager,
                         AppController* appController,
                         const QString& emuId,
                         int port,
                         QWidget* parent = nullptr);

    void loadBindings();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void relayout();
    void startCapture(const QString& label);
    void onBindingCaptured(int deviceIndex, const QString& element, bool isAxis, bool positive);
    void onKeyboardCaptured(const QString& keyString);
    void finishCapture(const QString& formatted);

    SdlInputManager* m_inputManager;
    AppController* m_appController;
    QString m_emuId;
    int m_port;

    QLabel* m_imgLabel = nullptr;
    QWidget* m_dpadBox = nullptr;
    QWidget* m_lAnalogBox = nullptr;
    QWidget* m_faceBox = nullptr;
    QWidget* m_rAnalogBox = nullptr;

    QMap<QString, QPushButton*> m_bindingButtons;
    QString m_capturingLabel;
};
```

- [ ] **Step 2: Create implementation**

Create `cpp/src/ui/settings/analog_bindings_widget.cpp`. This follows the exact same pattern as `ds2_bindings_widget.cpp` — 3-column layout with D-Pad + Left Stick on left, Face + Right Stick on right, shoulders/system/image/L3/R3/Analog/motors in center. The only differences from DS2:
- SVG: `analog_controller.svg` instead of `DualShock_2.svg`
- Class name: `AnalogBindingsWidget`
- Uses half-axis labels: `Left Stick Left`/`Right`/`Up`/`Down` and `Right Stick Left`/`Right`/`Up`/`Down` (same as DS2)
- Bottom row: L3, R3, Analog (no Pressure Modifier)
- Extra bottom row: LargeMotor, SmallMotor

Follow `ds2_bindings_widget.cpp` exactly for the constructor and relayout, changing:
1. Class name references
2. SVG path to `":/AppUI/qml/AppUI/images/controllers/analog_controller.svg"`
3. Replace "Pressure Modifier" button with nothing (only L3, Analog, R3 in bottom center row)
4. Add LargeMotor/SmallMotor row below Analog

- [ ] **Step 3: Add to CMakeLists.txt**

Add `analog_bindings_widget.cpp` to SOURCES and `analog_bindings_widget.h` to HEADERS.

- [ ] **Step 4: Build and verify**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/settings/analog_bindings_widget.h cpp/src/ui/settings/analog_bindings_widget.cpp cpp/CMakeLists.txt
git commit -m "feat: add AnalogController binding widget"
```

---

### Task 10: AnalogJoystick binding widget

Same as AnalogController but Mode instead of Analog, no motors.

**Files:**
- Create: `cpp/src/ui/settings/analog_joystick_bindings_widget.h`
- Create: `cpp/src/ui/settings/analog_joystick_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create header and implementation**

Follow exact same pattern as `analog_bindings_widget.*` with these changes:
- Class name: `AnalogJoystickBindingsWidget`
- SVG: `analog_controller.svg` (same — analog joystick looks the same)
- Bottom center row: L3, Mode, R3 (Mode replaces Analog)
- No LargeMotor/SmallMotor row

- [ ] **Step 2: Add to CMakeLists.txt**

Add both files to SOURCES and HEADERS.

- [ ] **Step 3: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/settings/analog_joystick_bindings_widget.h cpp/src/ui/settings/analog_joystick_bindings_widget.cpp cpp/CMakeLists.txt
git commit -m "feat: add AnalogJoystick binding widget"
```

---

### Task 11: DuckStation NeGcon binding widget

DuckStation NeGcon — D-pad left, A/B/I/II right, R/Start/Steering/L center. No motors.

**Files:**
- Create: `cpp/src/ui/settings/ds_negcon_bindings_widget.h`
- Create: `cpp/src/ui/settings/ds_negcon_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create header and implementation**

Follow `negcon_bindings_widget.*` (the existing PCSX2 NeGcon widget) as a template. Changes:
- Class name: `DSNegconBindingsWidget`
- SVG: `":/AppUI/qml/AppUI/images/controllers/ds_negcon.svg"`
- Right panel: A (top), B (left), I (right), II (bottom) — same diamond layout
- Center top: L (left shoulder analog) and R (right trigger), Start
- Center bottom: Steering Left, Steering Right
- No motors (DuckStation NeGcon has no motors, unlike PCSX2's which showed LargeMotor/SmallMotor)

- [ ] **Step 2: Add to CMakeLists.txt**

Add both files.

- [ ] **Step 3: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/settings/ds_negcon_bindings_widget.h cpp/src/ui/settings/ds_negcon_bindings_widget.cpp cpp/CMakeLists.txt
git commit -m "feat: add DuckStation NeGcon binding widget"
```

---

### Task 12: DuckStation NeGconRumble binding widget

Same as NeGcon but adds Analog button + motors.

**Files:**
- Create: `cpp/src/ui/settings/ds_negcon_rumble_bindings_widget.h`
- Create: `cpp/src/ui/settings/ds_negcon_rumble_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create header and implementation**

Follow `ds_negcon_bindings_widget.*` from Task 11 as template. Changes:
- Class name: `DSNegconRumbleBindingsWidget`
- SVG: `analog_controller.svg` (rumble variant looks more like analog pad)
- Add `Analog` button in center area (near Start)
- Add LargeMotor/SmallMotor row below Steering

- [ ] **Step 2: Add to CMakeLists.txt, build, commit**

Same pattern as previous tasks.

---

### Task 13: DuckStation JogCon binding widget

D-pad left, face buttons right, shoulders/system/Mode/steering/motor center.

**Files:**
- Create: `cpp/src/ui/settings/ds_jogcon_bindings_widget.h`
- Create: `cpp/src/ui/settings/ds_jogcon_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create header and implementation**

Follow `jogcon_bindings_widget.*` (existing PCSX2 JogCon) as template. Changes:
- Class name: `DSJogconBindingsWidget`
- SVG: `":/AppUI/qml/AppUI/images/controllers/Jogcon.svg"` (same as PCSX2)
- Center: L2/R2 top, L1/Select/Start/R1, image, Steering Left/Right
- Bottom: Mode button, Motor (single motor, not Large/Small pair)

- [ ] **Step 2: Add to CMakeLists.txt, build, commit**

Same pattern.

---

### Task 14: DuckStation Pop'n binding widget

Horizontal row of 9 color buttons + Start/Select.

**Files:**
- Create: `cpp/src/ui/settings/ds_popn_bindings_widget.h`
- Create: `cpp/src/ui/settings/ds_popn_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create header and implementation**

Follow `popn_bindings_widget.*` (existing PCSX2 Pop'n) as template. Changes:
- Class name: `DSPopnBindingsWidget`
- SVG: `":/AppUI/qml/AppUI/images/controllers/Popn.svg"` (same as PCSX2)
- Buttons use DuckStation labels: Left White, Left Yellow, Left Green, Left Blue, Middle Red, Right Blue, Right Green, Right Yellow, Right White (same display names as PCSX2)
- Row 2: Select, Start (same order)

- [ ] **Step 2: Add to CMakeLists.txt, build, commit**

Same pattern.

---

### Task 15: Wire up controller_mapping_page.cpp

Add DuckStation type → widget mappings to `createBindingsWidget()`.

**Files:**
- Modify: `cpp/src/ui/settings/controller_mapping_page.cpp`

- [ ] **Step 1: Add includes**

At the top of `controller_mapping_page.cpp`, after the existing binding widget includes (line 6), add:

```cpp
#include "digital_bindings_widget.h"
#include "analog_bindings_widget.h"
#include "analog_joystick_bindings_widget.h"
#include "ds_negcon_bindings_widget.h"
#include "ds_negcon_rumble_bindings_widget.h"
#include "ds_jogcon_bindings_widget.h"
#include "ds_popn_bindings_widget.h"
```

- [ ] **Step 2: Add type branches to createBindingsWidget()**

In the `createBindingsWidget()` method (around line 339-344), after the existing PCSX2 type branches and before the `// NotConnected` fallback, add:

```cpp
    // DuckStation controller types
    if (type == "DigitalController") return new DigitalBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "AnalogController")  return new AnalogBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "AnalogJoystick")    return new AnalogJoystickBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "NeGcon")            return new DSNegconBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "NeGconRumble")      return new DSNegconRumbleBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "JogCon")            return new DSJogconBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
    if (type == "PopnController")    return new DSPopnBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
```

**Important:** The existing PCSX2 types (`Negcon`, `Jogcon`, `Popn`) use different type IDs than DuckStation (`NeGcon`, `JogCon`, `PopnController`), so there's no conflict. Both sets of branches coexist.

- [ ] **Step 3: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/settings/controller_mapping_page.cpp
git commit -m "feat: wire DuckStation controller types to binding widgets"
```

---

### Task 16: Final build and verification

Full clean build to verify everything compiles.

**Files:** None (verification only)

- [ ] **Step 1: Clean build**

```bash
cd cpp && rm -rf build && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

Expected: Build succeeds with no errors.

- [ ] **Step 2: Run tests (if they exist)**

```bash
test -f cpp/build/tests && ./cpp/build/tests || echo "No test binary found"
```

- [ ] **Step 3: Verify no PCSX2 regression**

Quick sanity check — verify PCSX2 adapter still compiles and its methods are unchanged:
```bash
cd cpp && grep -c "controllerTypes\|controllerBindingDefsForType\|controllerSettingDefsForType" src/adapters/pcsx2_adapter.cpp
```

Expected: Output shows the PCSX2 methods are still there (count > 0).
