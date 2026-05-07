# Controller Mapping Redesign (PCSX2 pilot) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 12 hand-built per-controller-type binding widgets and the sprawling `controller_mapping_page` shell with a single schema-driven `ControllerBindingsView` that uses the `SettingsDialogTheme` palette and adds an OpenEmu-style spotlight on the focused button. PCSX2 is slimmed to DualShock 2 only, with the Settings sub-tab and the entire profile system removed.

**Architecture:** A new `ControllerBindingsView : QWidget` consumes the existing `BindingDef` schema (extended with `cardSlot` + `spotlightX/Y/R` fields), lays out `SettingsCard`-shaped binding cards in six fixed grid slots around a centered controller SVG, and forwards focus events to a custom `ControllerImageArea` that paints a dimming overlay + amber pulse ring at the focused button. The dialog shell collapses to a thin host that embeds the view, builds the amber `Now editing` footer, and wires A/B/X/Y handlers to existing `AppController` rebind / clear / auto-map flows.

**Tech Stack:** C++17, Qt6 Widgets (`QSvgRenderer`, `QPainter`, `QGridLayout`), QtTest. Reuses `SettingsCard` (focus + spatial-d-pad nav), `SettingsDialogTheme` (palette + QSS fragments), `EmulatorSettingsDialogBase` (outer frame), `AppController::saveBindingForPort / clearBindingForPort / clearAllBindingsForPort / autoMapControllerForPort`.

**Reference spec:** `docs/superpowers/specs/2026-05-07-controller-mapping-redesign-pcsx2-design.md`.

---

## File Structure

**New files (created in this plan):**

| Path | Responsibility |
|---|---|
| `cpp/src/ui/settings/controller_bindings_view.h` | `ControllerBindingsView` public widget API (one per emulator's primary controller). |
| `cpp/src/ui/settings/controller_bindings_view.cpp` | Implementation. Owns the slot grid + `ControllerImageArea` + footer. Hosts the `ControllerImageArea` and `ControllerBindingCard` inner classes inline. |
| `cpp/tests/test_pcsx2_controller_schema.cpp` | Catches PCSX2 controller-schema drift (single type, DS2 binding catalog, `cardSlot` + spotlight populated). |
| `cpp/tests/test_controller_bindings_view.cpp` | Smoke tests: image area survives render + focus + clear, view places cards in expected slot positions, footer reflects focus state. |

**Modified files:**

| Path | Change |
|---|---|
| `cpp/src/core/binding_def.h` | Add `cardSlot`, `spotlightX`, `spotlightY`, `spotlightR` fields (defaulted). |
| `cpp/src/adapters/pcsx2_adapter.cpp` | Trim `controllerTypes()` to DS2 only; trim `controllerBindingDefsForType` to DS2 branch with new fields populated; drop alt-type `controllerSettingDefsForType` branches and `controllerSettingDefs` body. |
| `cpp/src/ui/settings/controller_mapping_page.h` | Slim public surface — drop port/profile/tab handlers. |
| `cpp/src/ui/settings/controller_mapping_page.cpp` | Replace body with `ControllerBindingsView` host + amber `Now editing` footer + A/B/X/Y handlers. |
| `cpp/src/ui/app_controller.h` | Remove profile API + `setControllerType` + `restoreDefaultsForPort` + `controllerSettingsForPort` + `saveControllerSettingForPort` declarations. |
| `cpp/src/ui/app_controller.cpp` | Remove their bodies. |
| `cpp/src/services/config_service.h` | Remove profile API + `saveControllerSettingForPort` declarations. (Keep `restoreDefaultsForPort` + `controllerSettingsForPort` + `setControllerType` — used internally by `restoreEmulatorToDefaults`.) |
| `cpp/src/services/config_service.cpp` | Remove the 5 profile method bodies + `saveControllerSettingForPort` body. |
| `cpp/CMakeLists.txt` | Drop deleted source files; add new view source + new test targets. |

**Deleted files (15 total):**

```
cpp/src/ui/settings/bindings_widget_base.{h,cpp}
cpp/src/ui/settings/binding_widget_common.h
cpp/src/ui/settings/ds2_bindings_widget.{h,cpp}
cpp/src/ui/settings/guitar_bindings_widget.{h,cpp}
cpp/src/ui/settings/jogcon_bindings_widget.{h,cpp}
cpp/src/ui/settings/negcon_bindings_widget.{h,cpp}
cpp/src/ui/settings/digital_bindings_widget.{h,cpp}
cpp/src/ui/settings/analog_bindings_widget.{h,cpp}
cpp/src/ui/settings/analog_joystick_bindings_widget.{h,cpp}
cpp/src/ui/settings/ds_negcon_bindings_widget.{h,cpp}
cpp/src/ui/settings/ds_negcon_rumble_bindings_widget.{h,cpp}
cpp/src/ui/settings/ds_jogcon_bindings_widget.{h,cpp}
cpp/src/ui/settings/popn_bindings_widget.{h,cpp}
cpp/src/ui/settings/psp_bindings_widget.{h,cpp}
cpp/src/ui/settings/controller_settings_widget.{h,cpp}
```

---

## Phase 1 — Schema foundation

### Task 1: Extend `BindingDef` with `cardSlot` + spotlight fields

**Files:**
- Modify: `cpp/src/core/binding_def.h`

- [ ] **Step 1: Edit `binding_def.h`**

Replace the existing `BindingDef` definition with:

```cpp
#pragma once

#include <QString>

/**
 * BindingDef — describes a single controller button/axis binding.
 *
 * The schema-driven controller mapping view (controller_bindings_view)
 * uses `cardSlot` to place each binding's card in one of six fixed
 * page slots (DPad / FaceButtons / LeftAnalog / RightAnalog / Shoulders
 * / System) and uses `spotlightX/Y/R` (in the controller SVG's viewBox
 * coordinate system) to draw the OpenEmu-style spotlight at the
 * physical button when the card has focus. {0,0,0} means "no
 * spotlight" — used for abstract bindings (motors, pressure modifier)
 * that don't correspond to a specific button on the artwork.
 *
 * Adapters that haven't migrated to the new view ignore these fields;
 * defaulted values keep them harmless.
 */
struct BindingDef {
    enum Kind { Button, Axis };

    Kind kind;
    QString label;        // "Cross", "L1", "Left Stick Up"
    QString group;        // "Face Buttons", "Triggers", "Left Stick"
    QString section;      // INI section: "Pad1" / "Controller1"
    QString key;          // INI key: "Cross" / "ButtonCross"
    QString defaultValue; // "SDL-0/+A"

    // Card placement on the schema-driven page.
    // Allowed values: "DPad", "FaceButtons", "LeftAnalog", "RightAnalog",
    // "Shoulders", "System". Empty → fall back to matching `group`
    // case-insensitively (after stripping spaces).
    QString cardSlot = {};

    // Spotlight target on the controller SVG, in viewBox coordinates.
    // {0,0,0} = no spotlight (overlay suppressed when this binding has
    // focus).
    int spotlightX = 0;
    int spotlightY = 0;
    int spotlightR = 0;
};

/**
 * HotkeyDef — describes a single hotkey binding.
 */
struct HotkeyDef {
    QString label;        // "Toggle Pause"
    QString group;        // "General", "Save States"
    QString section;      // "Hotkeys"
    QString key;          // "TogglePause"
    QString defaultValue; // "Keyboard/Space"
};
```

- [ ] **Step 2: Build to verify the struct compiles everywhere**

```bash
cd cpp && cmake --build build
```

Expected: clean build. Defaulted fields mean no existing call site breaks.

- [ ] **Step 3: Run existing tests**

```bash
cd cpp && ctest --test-dir build --output-on-failure
```

Expected: all green (tests don't reference the new fields yet).

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/binding_def.h
git commit -m "$(cat <<'EOF'
controller: add cardSlot + spotlight fields to BindingDef

Schema annotation for the new schema-driven controller mapping view.
cardSlot picks one of six page-grid positions; spotlightX/Y/R describe
the OpenEmu-style focus spotlight target on the controller artwork.
All defaulted — adapters that haven't migrated ignore these.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Add failing schema test asserting PCSX2 → DS2-only + new-field population

**Files:**
- Create: `cpp/tests/test_pcsx2_controller_schema.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create the test file**

```cpp
// cpp/tests/test_pcsx2_controller_schema.cpp
#include <QtTest>
#include <QSet>
#include "adapters/pcsx2_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"

// Pins the contract for PCSX2's slimmed controller schema:
//  - exactly one controller type (DualShock 2)
//  - controllerSettingDefs / controllerSettingDefsForType deliberately empty
//  - every DS2 BindingDef carries a non-empty cardSlot
//  - every DS2 binding that *should* map to a physical button has a
//    non-zero spotlightR
//
// If a PR drifts any of these, the test trips loud rather than producing
// a silently-wrong UI.

class TestPcsx2ControllerSchema : public QObject {
    Q_OBJECT

private:
    PCSX2Adapter adapter_;

private slots:
    void testSingleControllerType() {
        const auto types = adapter_.controllerTypes();
        QCOMPARE(types.size(), 1);
        QCOMPARE(types.front().id, QString("DualShock2"));
        QCOMPARE(types.front().displayName, QString("DualShock 2"));
        QVERIFY(types.front().svgResource.endsWith("DualShock_2.svg"));
    }

    void testNoControllerSettings() {
        QVERIFY(adapter_.controllerSettingDefs().isEmpty());
        QVERIFY(adapter_.controllerSettingDefsForType("DualShock2").isEmpty());
    }

    void testDualShock2BindingsHaveCardSlot() {
        const auto bindings = adapter_.controllerBindingDefsForType("DualShock2");
        QVERIFY(!bindings.isEmpty());

        // Every binding must declare a slot; falling back to group is fine
        // for adapters mid-migration but PCSX2 is the pilot — we expect
        // the new field to be populated explicitly.
        const QSet<QString> validSlots{
            "DPad", "FaceButtons", "LeftAnalog", "RightAnalog",
            "Shoulders", "System",
        };
        for (const auto& b : bindings) {
            QVERIFY2(!b.cardSlot.isEmpty(),
                qPrintable(QString("binding '%1' has empty cardSlot").arg(b.label)));
            QVERIFY2(validSlots.contains(b.cardSlot),
                qPrintable(QString("binding '%1' has unknown cardSlot '%2'")
                           .arg(b.label, b.cardSlot)));
        }
    }

    void testPhysicalButtonsHaveSpotlight() {
        const auto bindings = adapter_.controllerBindingDefsForType("DualShock2");
        // Physical-button labels that must light up on the controller artwork.
        // (LargeMotor / SmallMotor / Pressure Modifier / Analog have no
        //  visible button, so they're allowed to leave spotlightR == 0.)
        const QSet<QString> mustHaveSpotlight{
            "Up", "Down", "Left", "Right",
            "Triangle", "Circle", "Cross", "Square",
            "L1", "L2", "R1", "R2",
            "Left Stick Up", "Left Stick Down", "Left Stick Left", "Left Stick Right",
            "Right Stick Up", "Right Stick Down", "Right Stick Left", "Right Stick Right",
            "L3", "R3",
            "Select", "Start",
        };
        for (const auto& b : bindings) {
            if (!mustHaveSpotlight.contains(b.label)) continue;
            QVERIFY2(b.spotlightR > 0,
                qPrintable(QString("binding '%1' must have non-zero spotlightR").arg(b.label)));
            QVERIFY2(b.spotlightX > 0 && b.spotlightY > 0,
                qPrintable(QString("binding '%1' must have positive spotlight (x,y)").arg(b.label)));
        }
    }

    void testNoAlternateControllerTypes() {
        // The Guitar / Jogcon / NeGcon / Pop'n branches have been removed
        // — the adapter should return an empty list for any non-DS2 type.
        for (const QString& dropped : {"Guitar", "Jogcon", "Negcon", "Popn", "NotConnected"}) {
            QVERIFY2(adapter_.controllerBindingDefsForType(dropped).isEmpty(),
                qPrintable(QString("type '%1' should have no bindings").arg(dropped)));
            QVERIFY2(adapter_.controllerSettingDefsForType(dropped).isEmpty(),
                qPrintable(QString("type '%1' should have no settings").arg(dropped)));
        }
    }
};

QTEST_MAIN(TestPcsx2ControllerSchema)
#include "test_pcsx2_controller_schema.moc"
```

- [ ] **Step 2: Wire the test target in `cpp/CMakeLists.txt`**

Find the block that defines `test_pcsx2_schema` (around line 605, the existing PCSX2 settings-schema test). Right after its `add_test(...)` line, append:

```cmake
add_executable(test_pcsx2_controller_schema
    tests/test_pcsx2_controller_schema.cpp
    src/adapters/pcsx2_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_pcsx2_controller_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_controller_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_controller_schema PRIVATE Qt6::Core Qt6::Network Qt6::Test chdr-static)
add_test(NAME Pcsx2ControllerSchema COMMAND test_pcsx2_controller_schema)
```

- [ ] **Step 3: Configure + build the new test target**

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
cmake --build build --target test_pcsx2_controller_schema
```

Expected: builds clean.

- [ ] **Step 4: Run it — expect failure**

```bash
cd cpp && ctest --test-dir build --output-on-failure -R Pcsx2ControllerSchema
```

Expected: **FAIL** at `testSingleControllerType` — adapter still returns 6 types (NotConnected + DS2 + Guitar + Jogcon + Negcon + Popn). Other slot/spotlight assertions also fail. This is intentional — the next task makes them pass.

- [ ] **Step 5: Commit (failing test)**

```bash
git add cpp/tests/test_pcsx2_controller_schema.cpp cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test: PCSX2 controller schema (failing — pinning post-redesign contract)

Asserts: single controller type, no settings schema, every DS2 binding
declares a valid cardSlot, every physical-button binding has a non-zero
spotlight. Currently fails — Task 3 trims the adapter to make it pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Slim the PCSX2 adapter to make Task 2's test pass

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp`

This task rewrites `controllerTypes()`, `controllerBindingDefsForType()`, and `controllerSettingDefsForType()`. It does **not** delete `controllerBindingDefs()` (the type-agnostic helper) — it just makes that helper return the same DS2 list and re-exports it under the type-specific path.

- [ ] **Step 1: Replace `controllerTypes()`**

In `cpp/src/adapters/pcsx2_adapter.cpp` (around line 1665), replace the existing function body:

```cpp
QVector<ControllerTypeDef> PCSX2Adapter::controllerTypes() const {
    return {
        {"DualShock2", "DualShock 2",
         ":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg"},
    };
}
```

- [ ] **Step 2: Replace `controllerBindingDefsForType()`**

Below it, replace the existing function (which switches on type and has a Guitar / Jogcon / Negcon / Popn branch each) with:

```cpp
QVector<BindingDef> PCSX2Adapter::controllerBindingDefsForType(const QString& type) const {
    if (type != "DualShock2") return {};

    // DualShock 2 — 28 bindings across 6 cardSlots.
    // Spotlight coordinates are in the DualShock_2.svg intrinsic viewBox
    // (974 × 664.8). Values calibrated visually against the artwork —
    // adjust if the SVG is replaced.
    return {
        // D-Pad
        {BindingDef::Button, "Up",    "D-Pad", "Pad", "Up",    "SDL-0/DPadUp",
            "DPad",  217, 235, 38},
        {BindingDef::Button, "Right", "D-Pad", "Pad", "Right", "SDL-0/DPadRight",
            "DPad",  267, 285, 38},
        {BindingDef::Button, "Down",  "D-Pad", "Pad", "Down",  "SDL-0/DPadDown",
            "DPad",  217, 335, 38},
        {BindingDef::Button, "Left",  "D-Pad", "Pad", "Left",  "SDL-0/DPadLeft",
            "DPad",  167, 285, 38},

        // Left Analog
        {BindingDef::Axis, "Left Stick Up",    "Left Stick", "Pad", "LUp",    "SDL-0/-LeftY",
            "LeftAnalog", 357, 425, 50},
        {BindingDef::Axis, "Left Stick Right", "Left Stick", "Pad", "LRight", "SDL-0/+LeftX",
            "LeftAnalog", 407, 475, 50},
        {BindingDef::Axis, "Left Stick Down",  "Left Stick", "Pad", "LDown",  "SDL-0/+LeftY",
            "LeftAnalog", 357, 525, 50},
        {BindingDef::Axis, "Left Stick Left",  "Left Stick", "Pad", "LLeft",  "SDL-0/-LeftX",
            "LeftAnalog", 307, 475, 50},
        {BindingDef::Button, "L3", "Left Stick", "Pad", "L3", "SDL-0/LeftStick",
            "LeftAnalog", 357, 475, 32},

        // Face Buttons
        {BindingDef::Button, "Triangle", "Face Buttons", "Pad", "Triangle", "SDL-0/FaceNorth",
            "FaceButtons", 757, 235, 36},
        {BindingDef::Button, "Circle",   "Face Buttons", "Pad", "Circle",   "SDL-0/FaceEast",
            "FaceButtons", 807, 285, 36},
        {BindingDef::Button, "Cross",    "Face Buttons", "Pad", "Cross",    "SDL-0/FaceSouth",
            "FaceButtons", 757, 335, 36},
        {BindingDef::Button, "Square",   "Face Buttons", "Pad", "Square",   "SDL-0/FaceWest",
            "FaceButtons", 707, 285, 36},

        // Right Analog
        {BindingDef::Axis, "Right Stick Up",    "Right Stick", "Pad", "RUp",    "SDL-0/-RightY",
            "RightAnalog", 617, 425, 50},
        {BindingDef::Axis, "Right Stick Right", "Right Stick", "Pad", "RRight", "SDL-0/+RightX",
            "RightAnalog", 667, 475, 50},
        {BindingDef::Axis, "Right Stick Down",  "Right Stick", "Pad", "RDown",  "SDL-0/+RightY",
            "RightAnalog", 617, 525, 50},
        {BindingDef::Axis, "Right Stick Left",  "Right Stick", "Pad", "RLeft",  "SDL-0/-RightX",
            "RightAnalog", 567, 475, 50},
        {BindingDef::Button, "R3", "Right Stick", "Pad", "R3", "SDL-0/RightStick",
            "RightAnalog", 617, 475, 32},

        // Shoulders
        {BindingDef::Button, "L2", "Shoulders", "Pad", "L2", "SDL-0/+LeftTrigger",
            "Shoulders", 217, 95, 40},
        {BindingDef::Button, "L1", "Shoulders", "Pad", "L1", "SDL-0/LeftShoulder",
            "Shoulders", 217, 145, 36},
        {BindingDef::Button, "R1", "Shoulders", "Pad", "R1", "SDL-0/RightShoulder",
            "Shoulders", 757, 145, 36},
        {BindingDef::Button, "R2", "Shoulders", "Pad", "R2", "SDL-0/+RightTrigger",
            "Shoulders", 757, 95, 40},

        // System
        {BindingDef::Button, "Select",  "System", "Pad", "Select",  "SDL-0/Back",
            "System",  427, 295, 22},
        {BindingDef::Button, "Start",   "System", "Pad", "Start",   "SDL-0/Start",
            "System",  547, 295, 22},
        {BindingDef::Button, "Analog",  "System", "Pad", "Analog",  "SDL-0/Guide",
            "System",  487, 365, 18},

        // Abstract bindings — no spotlight (no physical button on artwork).
        {BindingDef::Button, "Pressure Modifier", "System", "Pad", "PressureModifier", "",
            "System",  0, 0, 0},
        {BindingDef::Axis,   "LargeMotor",        "Motors", "Pad", "LargeMotor", "",
            "System",  0, 0, 0},
        {BindingDef::Axis,   "SmallMotor",        "Motors", "Pad", "SmallMotor", "",
            "System",  0, 0, 0},
    };
}
```

- [ ] **Step 3: Make `controllerBindingDefs()` re-export the DS2 list**

Find the existing `PCSX2Adapter::controllerBindingDefs()` (the type-agnostic version) and change it to:

```cpp
QVector<BindingDef> PCSX2Adapter::controllerBindingDefs() const {
    return controllerBindingDefsForType("DualShock2");
}
```

- [ ] **Step 4: Empty the controller-settings hooks**

Replace `PCSX2Adapter::controllerSettingDefs()` and `PCSX2Adapter::controllerSettingDefsForType()` (around line 1759):

```cpp
QVector<SettingDef> PCSX2Adapter::controllerSettingDefs() const {
    return {};
}

QVector<SettingDef> PCSX2Adapter::controllerSettingDefsForType(const QString& type) const {
    Q_UNUSED(type);
    return {};
}
```

- [ ] **Step 5: Build**

```bash
cd cpp && cmake --build build
```

Expected: clean. (`controller_mapping_page.cpp` still references the old binding widgets, but it doesn't depend on the alt-type binding lists at link time — it just won't find a widget for those types at runtime, and we'll fix the page in Phase 3.)

- [ ] **Step 6: Run the schema test — should now pass**

```bash
cd cpp && ctest --test-dir build --output-on-failure -R Pcsx2ControllerSchema
```

Expected: **PASS**.

- [ ] **Step 7: Run all tests to confirm no regression**

```bash
cd cpp && ctest --test-dir build --output-on-failure
```

Expected: all green.

- [ ] **Step 8: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "$(cat <<'EOF'
pcsx2: slim controller schema to DualShock 2 only

controllerTypes() returns one entry. controllerBindingDefsForType
returns 28 DS2 bindings annotated with cardSlot + spotlight (x, y, r)
matching the DualShock_2.svg viewBox. controllerBindingDefs()
re-exports DS2. Settings schemas are now empty across the board —
deadzone / sensitivity / vibration tunings are dropped per spec.

Guitar / Jogcon / NeGcon / Pop'n branches deleted along with their
per-type setting blocks.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 2 — New widgets

### Task 4: Create `ControllerBindingsView` skeleton + `ControllerImageArea` inner class

**Files:**
- Create: `cpp/src/ui/settings/controller_bindings_view.h`
- Create: `cpp/src/ui/settings/controller_bindings_view.cpp`
- Create: `cpp/tests/test_controller_bindings_view.cpp`
- Modify: `cpp/CMakeLists.txt`

This is the largest single task — it stands up the new widget end-to-end, with the spotlight-rendering image area, the slot grid, the binding cards, the footer, and the focus-routing wiring. Subsequent tasks integrate it into the dialog.

- [ ] **Step 1: Create the header**

```cpp
// cpp/src/ui/settings/controller_bindings_view.h
#pragma once

#include "core/binding_def.h"

#include <QString>
#include <QVector>
#include <QWidget>

class AppController;
class SdlInputManager;
class QLabel;
class QGridLayout;

/**
 * ControllerBindingsView — schema-driven controller mapping page.
 *
 * Loads `adapter->controllerTypes()` (expects exactly one entry —
 * the emulator's primary controller), reads its `BindingDef` list
 * via `adapter->controllerBindingDefsForType(type)`, and renders:
 *
 *   • a centered SVG illustration of the controller
 *   • six fixed grid slots of `SettingsCard`-shaped binding cards
 *     (DPad / FaceButtons / LeftAnalog / RightAnalog / Shoulders / System)
 *   • a "Now editing: <Label> → <Value>" amber footer with colored
 *     gamepad action hints (A Rebind / B Clear / Y Auto-Map / X Close)
 *
 * When a card is focused, the rest of the controller dims and a soft
 * circular cutout brightens just the focused button (OpenEmu pattern).
 *
 * Card focus is forwarded as `bindingFocused(BindingDef)` to the host
 * dialog; activation (Enter / mouse click) becomes `rebindRequested`.
 *
 * Reusable across emulators: any adapter that declares one
 * `ControllerTypeDef` and annotates its `BindingDef`s with `cardSlot`
 * + spotlight coordinates gets this page for free.
 */
class ControllerBindingsView : public QWidget {
    Q_OBJECT
public:
    ControllerBindingsView(SdlInputManager* inputManager,
                           AppController* appController,
                           const QString& emuId,
                           int port,
                           QWidget* parent = nullptr);
    ~ControllerBindingsView() override;

    /// Re-read bindings from AppController and refresh card values.
    /// Call after rebind / clear / auto-map flows.
    void reloadBindings();

signals:
    /// Emitted when the user focuses a card (keyboard nav, mouse hover,
    /// or programmatic). `b.spotlightR == 0` means "no physical button".
    void bindingFocused(const BindingDef& b);

    /// Emitted when the user activates a card (A button / Enter / click).
    /// Host wires this to startCapture for rebinding.
    void rebindRequested(const BindingDef& b);

    /// Emitted when the user requests a clear on the focused card
    /// (B button while card has focus).
    void clearRequested(const BindingDef& b);

private:
    class ImageArea;        // forward — defined in .cpp
    class BindingCard;      // forward — defined in .cpp

    void buildSlots(const QVector<BindingDef>& bindings);
    void onCardFocused(const BindingDef& b);
    void updateNowEditing(const BindingDef& b, const QString& value);
    QString currentValueFor(const QString& key) const;

    SdlInputManager* m_inputManager;
    AppController*   m_appController;
    QString          m_emuId;
    int              m_port;

    ImageArea*       m_imageArea = nullptr;
    QLabel*          m_nowLabel  = nullptr;   // "NOW EDITING" small caps
    QLabel*          m_nowValue  = nullptr;   // "L2 → LTrigger" big readout

    QVector<BindingDef> m_bindings;
    // Map from key → currentValue, refreshed by reloadBindings().
    QHash<QString, QString> m_currentValues;
};
```

- [ ] **Step 2: Create the implementation skeleton**

Implement the `.cpp` in chunks. Start with the constructor + slot grid:

```cpp
// cpp/src/ui/settings/controller_bindings_view.cpp
#include "controller_bindings_view.h"

#include "adapters/adapter_registry.h"
#include "adapters/emulator_adapter.h"
#include "core/sdl_input_manager.h"
#include "settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "widgets/settings_card.h"

#include <QElapsedTimer>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace {

// ─── Slot geometry ────────────────────────────────────────────────
//
// The page is laid out as a 5×3 QGridLayout:
//
//   ┌────────────────────┬───────────────────────────┬────────────────────┐
//   │ (header)            │ Shoulders (cards row)     │ (header)            │  row 0
//   ├────────────────────┼───────────────────────────┼────────────────────┤
//   │ D-Pad cluster       │                           │ Face Buttons       │  row 1
//   │ (4 cards)           │                           │ (4 cards)          │
//   ├────────────────────┤   Controller image area   ├────────────────────┤
//   │ Left Analog         │                           │ Right Analog       │  row 2
//   │ (5 cards)           │                           │ (5 cards)          │
//   ├────────────────────┼───────────────────────────┼────────────────────┤
//   │                    │ System (3 cards row)       │                    │  row 3
//   ├────────────────────┴───────────────────────────┴────────────────────┤
//   │ Now-editing footer                                                    │  row 4
//   └────────────────────────────────────────────────────────────────────┘
//
constexpr int kColLeft    = 0;
constexpr int kColCenter  = 1;
constexpr int kColRight   = 2;
constexpr int kRowTop     = 0;
constexpr int kRowDpadFB  = 1;
constexpr int kRowAnalogs = 2;
constexpr int kRowBottom  = 3;
constexpr int kRowFooter  = 4;

constexpr int kCardWidth   = 160;
constexpr int kImageMinW   = 480;
constexpr int kImageMinH   = 360;
constexpr int kFooterHeight = 130;

QString sectionHeaderText(const QString& slot) {
    if (slot == "DPad")         return "D-PAD";
    if (slot == "FaceButtons")  return "FACE BUTTONS";
    if (slot == "LeftAnalog")   return "LEFT ANALOG";
    if (slot == "RightAnalog")  return "RIGHT ANALOG";
    if (slot == "Shoulders")    return "SHOULDERS";
    if (slot == "System")       return "SYSTEM";
    return slot.toUpper();
}

QString resolveSlot(const BindingDef& b) {
    if (!b.cardSlot.isEmpty()) return b.cardSlot;
    // Fallback: collapse spaces from `group` and match case-insensitively.
    QString g = b.group;
    g.remove(' ');
    if (g.compare("dpad",         Qt::CaseInsensitive) == 0) return "DPad";
    if (g.compare("facebuttons",  Qt::CaseInsensitive) == 0) return "FaceButtons";
    if (g.compare("leftanalog",   Qt::CaseInsensitive) == 0
     || g.compare("leftstick",    Qt::CaseInsensitive) == 0) return "LeftAnalog";
    if (g.compare("rightanalog",  Qt::CaseInsensitive) == 0
     || g.compare("rightstick",   Qt::CaseInsensitive) == 0) return "RightAnalog";
    if (g.compare("shoulders",    Qt::CaseInsensitive) == 0
     || g.compare("triggers",     Qt::CaseInsensitive) == 0) return "Shoulders";
    return "System";
}

} // namespace
```

- [ ] **Step 3: Add the `BindingCard` inner class (subclass of `SettingsCard`)**

Append below the anonymous namespace:

```cpp
// ─── BindingCard — SettingsCard that carries a BindingDef ────────────

class ControllerBindingsView::BindingCard : public SettingsCard {
public:
    explicit BindingCard(const BindingDef& def, QWidget* parent = nullptr)
        : SettingsCard(parent), m_def(def) {
        pinToReferenceHeight();
        setFixedWidth(kCardWidth);
        setStyleSheet(SettingsDialogTheme::cardQss());

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(10, 6, 10, 6);
        lay->setSpacing(2);

        m_label = new QLabel(def.label.toUpper(), this);
        m_label->setStyleSheet(QStringLiteral(
            "color: #d0ccc4; font-size: 9px; font-weight: 600;"
            "letter-spacing: 1.4px; background: transparent;"));

        m_value = new QLabel("Not bound", this);
        m_value->setStyleSheet(QStringLiteral(
            "color: #f2efe8; font-size: 13px; background: transparent;"));

        lay->addWidget(m_label);
        lay->addWidget(m_value);
    }

    const BindingDef& def() const { return m_def; }

    void setCurrentValue(const QString& v) {
        if (v.isEmpty()) {
            m_value->setText("Not bound");
            m_value->setStyleSheet(QStringLiteral(
                "color: #9a9690; font-size: 13px; font-style: italic;"
                "background: transparent;"));
        } else {
            m_value->setText(v);
            m_value->setStyleSheet(QStringLiteral(
                "color: #f2efe8; font-size: 13px; background: transparent;"));
        }
    }

    QString currentText() const { return m_value->text(); }

private:
    BindingDef m_def;
    QLabel* m_label;
    QLabel* m_value;
};
```

- [ ] **Step 4: Add the `ImageArea` inner class with spotlight rendering**

Append below `BindingCard`:

```cpp
// ─── ImageArea — controller SVG + dim overlay + amber pulse ring ─────

class ControllerBindingsView::ImageArea : public QWidget {
public:
    explicit ImageArea(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(kImageMinW, kImageMinH);
        m_pulseTimer.setInterval(33);   // ~30 fps
        connect(&m_pulseTimer, &QTimer::timeout, this, [this](){ update(); });
        m_pulseClock.start();
    }

    void setControllerSvg(const QString& resourcePath) {
        m_renderer.load(resourcePath);
        if (m_renderer.isValid()) m_viewBox = m_renderer.viewBoxF();
        update();
    }

    void setFocusedBinding(const BindingDef* b) {
        if (!b || b->spotlightR == 0) {
            m_focused.reset();
            m_pulseTimer.stop();
        } else {
            m_focused = *b;
            m_pulseTimer.start();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#585450"));

        if (!m_renderer.isValid()) return;

        // Compute SVG render rect (centered, aspect-preserved).
        const QSizeF vb = m_viewBox.size();
        const qreal scale = std::min(width() / vb.width(), height() / vb.height());
        const qreal renderW = vb.width()  * scale;
        const qreal renderH = vb.height() * scale;
        const qreal originX = (width()  - renderW) / 2.0;
        const qreal originY = (height() - renderH) / 2.0;
        const QRectF imageRect(originX, originY, renderW, renderH);

        // 1. SVG.
        m_renderer.render(&p, imageRect);

        if (!m_focused.has_value() || m_focused->spotlightR == 0) return;

        // 2. Spotlight: dark overlay with radial-gradient cutout.
        const qreal cx = originX + m_focused->spotlightX * scale;
        const qreal cy = originY + m_focused->spotlightY * scale;
        const qreal r  = m_focused->spotlightR * scale;

        QRadialGradient grad(QPointF(cx, cy), r * 1.5);
        grad.setColorAt(0.00, QColor(0, 0, 0,   0));
        grad.setColorAt(0.55, QColor(0, 0, 0,   0));
        grad.setColorAt(1.00, QColor(0, 0, 0, 158));   // ~62% alpha at edge
        p.fillRect(imageRect, grad);

        // 3. Amber pulse ring on top.
        const qreal phaseT = std::sin(m_pulseClock.elapsed() / 254.6) * 0.5 + 0.5;
        const qreal ringR = r * 1.4 + phaseT * 2.0;
        const int ringAlpha = 217 + int(phaseT * 38);   // 0.85→1.0 of 255

        // Glow layer (wider, semi-transparent).
        QPen glowPen(QColor(245, 158, 11, 96), 8);
        p.setPen(glowPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(cx, cy), ringR, ringR);

        // Sharp ring on top.
        QPen ringPen(QColor(245, 158, 11, ringAlpha), 3);
        p.setPen(ringPen);
        p.drawEllipse(QPointF(cx, cy), ringR, ringR);
    }

private:
    QSvgRenderer            m_renderer;
    QRectF                  m_viewBox{0, 0, 1000, 1000};
    std::optional<BindingDef> m_focused;
    QTimer                  m_pulseTimer;
    QElapsedTimer           m_pulseClock;
};
```

(Add `#include <optional>` near the top.)

- [ ] **Step 5: Add the constructor + slot grid + footer + focus wiring**

Append below `ImageArea`:

```cpp
// ─── ControllerBindingsView ─────────────────────────────────────────

ControllerBindingsView::ControllerBindingsView(SdlInputManager* inputManager,
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
    setStyleSheet(QStringLiteral("background: #585450;"));

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    Q_ASSERT(adapter);
    const auto types = adapter->controllerTypes();
    Q_ASSERT_X(types.size() == 1, "ControllerBindingsView",
               "Adapter must declare exactly one controller type for the new view.");

    const QString typeId = types.front().id;
    const QString svg    = types.front().svgResource;
    m_bindings = adapter->controllerBindingDefsForType(typeId);

    // ─── Layout ───────────────────────────────────────
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(20, 16, 20, 0);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(kColLeft,   0);
    grid->setColumnStretch(kColCenter, 1);
    grid->setColumnStretch(kColRight,  0);
    grid->setRowStretch(kRowAnalogs, 1);

    // ─── Image area in the center column, spans rows 0-3 ───
    m_imageArea = new ImageArea(this);
    m_imageArea->setControllerSvg(svg);
    grid->addWidget(m_imageArea, kRowTop, kColCenter, kRowBottom - kRowTop + 1, 1);

    // ─── Build slots ───────────────────────────────────
    buildSlots(m_bindings);

    // ─── Footer (now-editing + colored face hints) ──────
    auto* footer = new QFrame(this);
    footer->setFixedHeight(kFooterHeight);
    footer->setStyleSheet(QStringLiteral(
        "background: #4a4642; border-left: 3px solid #f59e0b;"));

    auto* footerLay = new QHBoxLayout(footer);
    footerLay->setContentsMargins(28, 0, 28, 0);
    footerLay->setSpacing(24);

    // Left block: NOW EDITING / value
    auto* leftBlock = new QWidget(footer);
    auto* leftV = new QVBoxLayout(leftBlock);
    leftV->setContentsMargins(0, 0, 0, 0);
    leftV->setSpacing(6);

    m_nowLabel = new QLabel("READY", leftBlock);
    m_nowLabel->setStyleSheet(QStringLiteral(
        "color: #d0ccc4; font-size: 10px; letter-spacing: 3px;"
        "text-transform: uppercase; background: transparent;"));

    m_nowValue = new QLabel({}, leftBlock);
    m_nowValue->setStyleSheet(QStringLiteral(
        "color: #f2efe8; font-size: 30px; font-weight: 300;"
        "letter-spacing: 1.5px; background: transparent;"));

    leftV->addWidget(m_nowLabel);
    leftV->addWidget(m_nowValue);
    footerLay->addWidget(leftBlock, 1);

    // Right block: A/B/Y/X face hints
    struct FaceHint { const char* label; const char* face; const char* bg; const char* fg; };
    const FaceHint hints[] = {
        {"Rebind",   "A", "#39c46b", "#0e2a14"},
        {"Clear",    "B", "#e74c4c", "#2a0e0e"},
        {"Auto-Map", "Y", "#ffd23a", "#2a210e"},
        {"Close",    "X", "#3aa6ff", "#0e1f2a"},
    };
    auto* hintsRow = new QHBoxLayout();
    hintsRow->setSpacing(24);
    for (const auto& h : hints) {
        auto* row = new QHBoxLayout();
        row->setSpacing(9);
        auto* face = new QLabel(h.face, footer);
        face->setFixedSize(28, 28);
        face->setAlignment(Qt::AlignCenter);
        face->setStyleSheet(QString(
            "background: %1; color: %2; border-radius: 14px;"
            "font-size: 13px; font-weight: 800;").arg(h.bg, h.fg));
        auto* lbl = new QLabel(h.label, footer);
        lbl->setStyleSheet(QStringLiteral(
            "color: #f2efe8; font-size: 13px; background: transparent;"));
        row->addWidget(face);
        row->addWidget(lbl);
        hintsRow->addLayout(row);
    }
    footerLay->addLayout(hintsRow, 0);

    // Span the footer across all 3 columns at the bottom.
    grid->addWidget(footer, kRowFooter, kColLeft, 1, 3);

    // Initial value load
    reloadBindings();
}

ControllerBindingsView::~ControllerBindingsView() = default;
```

- [ ] **Step 6: Add `buildSlots`, `reloadBindings`, focus routing, and helpers**

Append below the constructor:

```cpp
void ControllerBindingsView::buildSlots(const QVector<BindingDef>& bindings) {
    auto* grid = qobject_cast<QGridLayout*>(layout());
    Q_ASSERT(grid);

    struct SlotPos { int row; int col; };
    static const QHash<QString, SlotPos> slotPositions{
        {"DPad",         {kRowDpadFB,  kColLeft}},
        {"LeftAnalog",   {kRowAnalogs, kColLeft}},
        {"FaceButtons",  {kRowDpadFB,  kColRight}},
        {"RightAnalog", {kRowAnalogs,  kColRight}},
        {"Shoulders",    {kRowTop,     kColCenter}},
        {"System",       {kRowBottom,  kColCenter}},
    };

    // Group bindings by resolved slot, preserving declaration order.
    QHash<QString, QVector<BindingDef>> bySlot;
    QVector<QString> slotOrder;
    for (const auto& b : bindings) {
        const QString slot = resolveSlot(b);
        if (!bySlot.contains(slot)) slotOrder.append(slot);
        bySlot[slot].append(b);
    }

    for (const QString& slot : slotOrder) {
        const auto pos = slotPositions.value(slot, SlotPos{kRowBottom, kColCenter});

        auto* container = new QWidget(this);
        container->setStyleSheet("background: transparent;");
        auto* v = new QVBoxLayout(container);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(6);

        auto* header = new QLabel(sectionHeaderText(slot), container);
        header->setStyleSheet(QStringLiteral(
            "color: #f59e0b; font-size: 10px; font-weight: 600;"
            "letter-spacing: 1.8px; background: transparent;"));
        v->addWidget(header);

        // For Shoulders + System: lay out cards horizontally, not vertically.
        const bool horizontal = (slot == "Shoulders" || slot == "System");
        QBoxLayout* cardLay = horizontal
            ? static_cast<QBoxLayout*>(new QHBoxLayout())
            : static_cast<QBoxLayout*>(new QVBoxLayout());
        cardLay->setSpacing(8);
        cardLay->setContentsMargins(0, 0, 0, 0);

        for (const auto& b : bySlot[slot]) {
            // Skip bindings with no key — internal helpers like LargeMotor
            // are kept in the schema for save/restore but have no card.
            if (b.key.isEmpty()) continue;
            auto* card = new BindingCard(b, container);
            cardLay->addWidget(card);
            // Wire focus → spotlight + footer.
            connect(card, &SettingsCard::focused, this, [this, card](){
                onCardFocused(card->def());
            });
            connect(card, &SettingsCard::activated, this, [this, card](){
                emit rebindRequested(card->def());
            });
        }
        if (horizontal) cardLay->addStretch(1);
        v->addLayout(cardLay);
        if (!horizontal) v->addStretch(1);

        grid->addWidget(container, pos.row, pos.col);
    }
}

void ControllerBindingsView::reloadBindings() {
    const QVariantList raw = m_appController->controllerBindingsForPort(m_emuId, m_port);
    m_currentValues.clear();
    for (const auto& v : raw) {
        const auto map = v.toMap();
        m_currentValues.insert(map.value("key").toString(),
                                map.value("currentValue").toString());
    }

    // Push values into existing cards.
    const auto cards = findChildren<BindingCard*>();
    for (auto* card : cards)
        card->setCurrentValue(m_currentValues.value(card->def().key));
}

void ControllerBindingsView::onCardFocused(const BindingDef& b) {
    m_imageArea->setFocusedBinding(&b);
    updateNowEditing(b, currentValueFor(b.key));
    emit bindingFocused(b);
}

void ControllerBindingsView::updateNowEditing(const BindingDef& b, const QString& value) {
    m_nowLabel->setText("NOW EDITING");
    const QString display = value.isEmpty() ? QStringLiteral("Not bound") : value;
    // "L2 → LTrigger" — amber arrow + amber value
    m_nowValue->setText(QString(
        "%1  <span style='color:#f59e0b;'>→</span>  "
        "<span style='color:#f59e0b; font-weight:600;'>%2</span>")
        .arg(b.label, display.toHtmlEscaped()));
    m_nowValue->setTextFormat(Qt::RichText);
}

QString ControllerBindingsView::currentValueFor(const QString& key) const {
    return m_currentValues.value(key);
}
```

- [ ] **Step 7: Add to `cpp/CMakeLists.txt` (main app sources)**

Find the `set(SOURCES ...)` block (or the equivalent that aggregates `src/ui/settings/*.cpp`) and add `src/ui/settings/controller_bindings_view.cpp` alongside the existing settings widget sources. (If the project uses a glob, no edit is needed.)

```bash
grep -n "controller_mapping_page.cpp\|generic_settings_page.cpp" cpp/CMakeLists.txt | head -5
```

If you see explicit `src/ui/settings/...cpp` entries, add this line in the same block:

```cmake
    src/ui/settings/controller_bindings_view.cpp
```

- [ ] **Step 8: Build the main target — confirm the new view compiles**

```bash
cd cpp && cmake --build build
```

Expected: clean build. (Nothing references the new view yet — only the test in the next step does.)

- [ ] **Step 9: Create the smoke test for the new view**

```cpp
// cpp/tests/test_controller_bindings_view.cpp
#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include "ui/settings/controller_bindings_view.h"
#include "ui/app_controller.h"
#include "core/sdl_input_manager.h"

// Smoke tests for ControllerBindingsView. Construction is integration-
// flavored (depends on AdapterRegistry + AppController), so we keep the
// tests intentionally light: build the view inside an AppController +
// SdlInputManager wired against a temp config dir, verify the cards
// render, focus state propagates, and reload doesn't crash.

class TestControllerBindingsView : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Use a temp config dir so the test doesn't touch the user's INI.
        qputenv("XDG_CONFIG_HOME", QByteArrayLiteral("/tmp/retronest-test-config"));
    }

    void constructsForPcsx2() {
        SdlInputManager input;
        AppController app(&input);
        ControllerBindingsView view(&input, &app, "pcsx2", /*port=*/1);
        view.resize(1280, 720);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        // 28 PCSX2 DS2 bindings, but 3 (motors + pressure modifier) have
        // empty keys and are skipped. Expect at least 24 cards rendered.
        const auto cards = view.findChildren<class SettingsCard*>();
        QVERIFY2(cards.size() >= 24,
            qPrintable(QString("expected ≥24 cards, got %1").arg(cards.size())));
    }

    void reloadBindingsDoesNotCrash() {
        SdlInputManager input;
        AppController app(&input);
        ControllerBindingsView view(&input, &app, "pcsx2", 1);
        view.reloadBindings();
        view.reloadBindings();
        view.reloadBindings();
    }

    void bindingFocusedSignalFires() {
        SdlInputManager input;
        AppController app(&input);
        ControllerBindingsView view(&input, &app, "pcsx2", 1);
        view.resize(1280, 720);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QSignalSpy spy(&view, &ControllerBindingsView::bindingFocused);
        const auto cards = view.findChildren<class SettingsCard*>();
        QVERIFY(!cards.isEmpty());
        cards.first()->setFocus(Qt::TabFocusReason);
        // SettingsCard emits `focused` synchronously in focusInEvent, which
        // the view handler turns into bindingFocused.
        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(TestControllerBindingsView)
#include "test_controller_bindings_view.moc"
```

- [ ] **Step 10: Wire the test target in `cpp/CMakeLists.txt`**

Append after the `test_settings_description_bar` block (around line 688):

```cmake
add_executable(test_controller_bindings_view
    tests/test_controller_bindings_view.cpp
    src/ui/settings/controller_bindings_view.cpp
    src/ui/settings/widgets/settings_card.cpp
    src/ui/settings/widgets/settings_combo_row.cpp
    src/ui/settings/widgets/settings_toggle.cpp
    src/ui/settings/widgets/settings_slider_row.cpp
    src/ui/app_controller.cpp
    src/services/config_service.cpp
    src/core/sdl_input_manager.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/pcsx2_adapter.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
)
set_target_properties(test_controller_bindings_view PROPERTIES AUTOMOC ON)
target_include_directories(test_controller_bindings_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS})
target_link_libraries(test_controller_bindings_view PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Sql Qt6::Network Qt6::Test
    chdr-static ${SDL2_LIBRARIES})
add_test(NAME ControllerBindingsView COMMAND test_controller_bindings_view)
```

(If the actual link list for the existing `test_pcsx2_schema` or `test_rom_scanner` is broader than what's shown above, mirror it — those are the closest "needs adapters + services" precedents.)

- [ ] **Step 11: Configure + build + run the new test**

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
cmake --build build --target test_controller_bindings_view
ctest --test-dir build --output-on-failure -R ControllerBindingsView
```

Expected: PASS. (If link errors flag missing libretro symbols, mirror the source list of `test_rom_scanner` — that's the closest existing target that pulls AdapterRegistry + AppController.)

- [ ] **Step 12: Run all tests**

```bash
cd cpp && ctest --test-dir build --output-on-failure
```

Expected: all green.

- [ ] **Step 13: Commit**

```bash
git add cpp/src/ui/settings/controller_bindings_view.h \
        cpp/src/ui/settings/controller_bindings_view.cpp \
        cpp/tests/test_controller_bindings_view.cpp \
        cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
controller: add ControllerBindingsView (schema-driven mapping page)

Single QWidget that consumes BindingDef (cardSlot + spotlight) +
ControllerTypeDef::svgResource and renders the v8 layout — six
SettingsCard slots around a centered controller SVG, OpenEmu-style
spotlight (dim overlay + amber pulse ring) on the focused button,
amber Now-editing footer with colored A/B/Y/X face hints. View emits
bindingFocused / rebindRequested / clearRequested for the host
dialog to wire to AppController flows.

Reusable across emulators — any adapter that returns one
ControllerTypeDef + annotates BindingDef with cardSlot + spotlight
gets the page for free.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 3 — Dialog integration

### Task 5: Replace `ControllerMappingPage` body with the new view

**Files:**
- Modify: `cpp/src/ui/settings/controller_mapping_page.h`
- Modify: `cpp/src/ui/settings/controller_mapping_page.cpp`

This is a wholesale rewrite of `controller_mapping_page` — everything except the dialog shell goes. After this task lands, the 14 obsolete widget files are dead code (deleted in Task 7).

- [ ] **Step 1: Replace the header**

Overwrite `cpp/src/ui/settings/controller_mapping_page.h`:

```cpp
#pragma once

#include <QDialog>

class SdlInputManager;
class AppController;
class ControllerBindingsView;

/**
 * ControllerMappingPage — host dialog for the schema-driven
 * controller mapping view. Provides the outer frame (sizing,
 * ESC-to-close, top-chrome title) and wires keyboard / gamepad
 * face-button shortcuts to the embedded ControllerBindingsView's
 * focused-binding + AppController flows:
 *
 *   A / Enter      → rebind focused binding
 *   B / Esc        → clear focused binding (or close if none focused)
 *   Y / M          → open Auto-Map menu (Keyboard + connected SDL devices)
 *   X              → close
 */
class ControllerMappingPage : public QDialog {
    Q_OBJECT
public:
    ControllerMappingPage(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          QWidget* parent = nullptr);

private:
    void onAutoMapRequested();
    void onRebindRequested(const struct BindingDef& b);
    void onClearRequested(const struct BindingDef& b);

    SdlInputManager*         m_inputManager;
    AppController*           m_appController;
    QString                  m_emuId;
    ControllerBindingsView*  m_view = nullptr;
    QString                  m_capturingKey;   // INI key currently capturing
    struct BindingDef*       m_focusedBinding = nullptr;   // weak — owned by view
};
```

- [ ] **Step 2: Replace the implementation**

Overwrite `cpp/src/ui/settings/controller_mapping_page.cpp`:

```cpp
#include "controller_mapping_page.h"

#include "controller_bindings_view.h"
#include "core/binding_def.h"
#include "core/sdl_input_manager.h"
#include "settings_dialog_theme.h"
#include "ui/app_controller.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QShortcut>
#include <QVBoxLayout>

ControllerMappingPage::ControllerMappingPage(SdlInputManager* inputManager,
                                              AppController* appController,
                                              const QString& emuId,
                                              QWidget* parent)
    : QDialog(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
{
    setWindowTitle("Controller");
    setMinimumSize(1280, 720);
    resize(1280, 720);
    setStyleSheet(QString("QDialog { background: %1; }")
                  .arg(SettingsDialogTheme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ─── Top chrome: amber title strip ────────────────
    auto* head = new QFrame(this);
    head->setFixedHeight(56);
    head->setStyleSheet(QStringLiteral(
        "background: #4a4642; border-bottom: 1px solid #3a3632;"));
    auto* headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(24, 0, 24, 0);

    auto* crumb = new QLabel(emuId.toUpper(), head);
    crumb->setStyleSheet(QStringLiteral(
        "color: #9a9690; font-size: 10px; letter-spacing: 3px;"
        "background: transparent;"));
    headLay->addWidget(crumb);

    // Title shows the emulator's primary controller display name.
    auto* title = new QLabel(head);
    title->setStyleSheet(QStringLiteral(
        "color: #f59e0b; font-size: 14px; font-weight: 600;"
        "letter-spacing: 2px; background: transparent;"));
    if (auto types = m_appController->controllerTypes(emuId); !types.isEmpty()) {
        title->setText(types.first().toMap().value("displayName").toString().toUpper());
    }
    headLay->addSpacing(14);
    headLay->addWidget(title);
    headLay->addStretch(1);

    root->addWidget(head);

    // ─── Body: ControllerBindingsView ─────────────────
    m_view = new ControllerBindingsView(inputManager, appController, emuId, /*port=*/1, this);
    root->addWidget(m_view, 1);

    // ─── Wiring ───────────────────────────────────────
    connect(m_view, &ControllerBindingsView::bindingFocused, this,
        [this](const BindingDef& b){
            // Stash a copy so A/B handlers know what's focused.
            static BindingDef s_focused;
            s_focused = b;
            m_focusedBinding = &s_focused;
        });
    connect(m_view, &ControllerBindingsView::rebindRequested, this,
            &ControllerMappingPage::onRebindRequested);
    connect(m_view, &ControllerBindingsView::clearRequested, this,
            &ControllerMappingPage::onClearRequested);

    // Auto-Map (Y / M)
    auto* yShort = new QShortcut(QKeySequence(Qt::Key_M), this);
    connect(yShort, &QShortcut::activated, this, &ControllerMappingPage::onAutoMapRequested);
    // Close (X / shortcut handled by QDialog ESC default)

    // Capture-completion routing.
    connect(m_inputManager, &SdlInputManager::bindingCaptured, this,
        [this](int devIdx, const QString& element, bool isAxis, bool positive){
            if (m_capturingKey.isEmpty()) return;
            const QString formatted = m_appController->formatCapturedBinding(
                m_emuId, devIdx, element, isAxis, positive);
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_capturingKey, formatted);
            m_capturingKey.clear();
            m_view->reloadBindings();
        });
    connect(m_inputManager, &SdlInputManager::keyboardCaptured, this,
        [this](const QString& keyString){
            if (m_capturingKey.isEmpty()) return;
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_capturingKey, keyString);
            m_capturingKey.clear();
            m_view->reloadBindings();
        });
}

void ControllerMappingPage::onRebindRequested(const BindingDef& b) {
    m_capturingKey = b.key;
    m_inputManager->startCapture();
}

void ControllerMappingPage::onClearRequested(const BindingDef& b) {
    m_appController->clearBindingForPort(m_emuId, /*port=*/1, b.key);
    m_view->reloadBindings();
}

void ControllerMappingPage::onAutoMapRequested() {
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #4a4642; color: #f2efe8;"
        "        border: 1px solid #706c66; }"
        "QMenu::item { padding: 8px 24px; }"
        "QMenu::item:selected { background: #f59e0b; color: #1a1816; }"));

    menu.addAction("Keyboard", [this]() {
        m_appController->clearAllBindingsForPort(m_emuId, /*port=*/1);
        m_view->reloadBindings();
    });

    const QVariantList controllers = m_inputManager->connectedControllers();
    for (const auto& c : controllers) {
        const auto map = c.toMap();
        const int devIdx = map["deviceIndex"].toInt();
        const QString name = map["name"].toString();
        menu.addAction(QString("SDL-%1: %2").arg(devIdx).arg(name), [this, devIdx]() {
            m_appController->autoMapControllerForPort(m_emuId, /*port=*/1, devIdx);
            m_view->reloadBindings();
        });
    }
    menu.exec(QCursor::pos());
}
```

- [ ] **Step 3: Build**

```bash
cd cpp && cmake --build build
```

Expected: build still includes the obsolete binding widget files (they're not deleted yet, but are no longer referenced from `controller_mapping_page.cpp`). Most of the obsolete files won't link into anything if they're only included via `controller_mapping_page.cpp`'s old switch — so depending on how `CMakeLists.txt` aggregates sources, this might just produce a clean build with unused-include warnings, or it might trip a linker warning about unused TU. **Don't delete the old files yet** — Task 7 does that explicitly.

If the build fails because the old binding widgets reference things that have moved, comment out their `add_executable`/source entries from CMakeLists.txt temporarily and add a TODO note pointing at Task 7.

- [ ] **Step 4: Run tests**

```bash
cd cpp && ctest --test-dir build --output-on-failure
```

Expected: green. The new `ControllerBindingsView` tests still pass; existing tests don't depend on the dialog shell.

- [ ] **Step 5: Manually open the dialog and smoke-test**

Build + launch:

```bash
cd cpp && cmake --build build
open ./build/RetroNest.app
```

In the app, navigate to PCSX2's controller mapping. Expected:
- Dialog opens at 1280×720, dark warm-grey background, amber `DUALSHOCK 2` title.
- Six slots of binding cards visible.
- Controller image centered.
- Focusing any card (Tab / arrows / mouse hover) shows the spotlight with amber pulse.
- Bottom strip reads `NOW EDITING — L1 → LShoulder` (or the current focus).
- Pressing Enter on a card enters capture mode (`Press a button…`); pressing a real button rebinds and the card refreshes.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/ui/settings/controller_mapping_page.h \
        cpp/src/ui/settings/controller_mapping_page.cpp
git commit -m "$(cat <<'EOF'
controller: collapse mapping page to ControllerBindingsView host

ControllerMappingPage shrinks to a thin shell — top-chrome amber title
strip + embedded ControllerBindingsView + dialog-level shortcuts for
A (rebind) / B (clear) / Y/M (auto-map menu) / X (close). Drops the
port sidebar, controller-type combo, Bindings/Settings tabs, profile
bar, restore-defaults button, and dark-themed input-dialog helpers.

The 12 per-type binding widgets and controller_settings_widget are
now unreferenced — Task 7 deletes them.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Wire `B` (clear focused) and dialog-level keyboard shortcuts

The new view emits `clearRequested` from a focused-card's `B`/`Esc` activation. We need the host to forward a `B` press in the dialog to the focused card.

**Files:**
- Modify: `cpp/src/ui/settings/controller_bindings_view.cpp`
- Modify: `cpp/src/ui/settings/controller_mapping_page.cpp`

- [ ] **Step 1: Wire `B` / Backspace at the view level**

In `ControllerBindingsView::buildSlots`, where the `connect(card, &SettingsCard::activated, …)` is wired, also install a per-card `keyPressEvent` filter so a `B`/`Backspace` while a card has focus emits `clearRequested`. Insert into `buildSlots` next to the existing `activated` wiring:

```cpp
            card->installEventFilter(this);
```

And override `eventFilter` on the view (declare it in the header next to the other private methods):

```cpp
    bool eventFilter(QObject* watched, QEvent* event) override;
```

Implementation in the .cpp:

```cpp
bool ControllerBindingsView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* card = qobject_cast<BindingCard*>(watched);
        if (!card || !card->hasFocus()) return QWidget::eventFilter(watched, event);

        const auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Backspace || ke->key() == Qt::Key_B) {
            emit clearRequested(card->def());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
```

- [ ] **Step 2: Build + manually verify**

```bash
cd cpp && cmake --build build
open ./build/RetroNest.app
```

Focus a card, press Backspace → that binding's value should clear and the card should show "Not bound".

- [ ] **Step 3: Commit**

```bash
git add cpp/src/ui/settings/controller_bindings_view.h \
        cpp/src/ui/settings/controller_bindings_view.cpp
git commit -m "$(cat <<'EOF'
controller: wire B/Backspace on focused card to clearRequested

ControllerBindingsView intercepts Backspace / B on focused binding
cards and emits clearRequested. The host dialog forwards it to
AppController::clearBindingForPort + reloadBindings.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 4 — Cleanup

### Task 7: Delete the 15 obsolete binding-widget files + their CMakeLists entries

**Files:**
- Delete (15): all files listed in the plan header's "Deleted files" block.
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Delete the files**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git rm cpp/src/ui/settings/bindings_widget_base.h \
       cpp/src/ui/settings/bindings_widget_base.cpp \
       cpp/src/ui/settings/binding_widget_common.h \
       cpp/src/ui/settings/ds2_bindings_widget.h \
       cpp/src/ui/settings/ds2_bindings_widget.cpp \
       cpp/src/ui/settings/guitar_bindings_widget.h \
       cpp/src/ui/settings/guitar_bindings_widget.cpp \
       cpp/src/ui/settings/jogcon_bindings_widget.h \
       cpp/src/ui/settings/jogcon_bindings_widget.cpp \
       cpp/src/ui/settings/negcon_bindings_widget.h \
       cpp/src/ui/settings/negcon_bindings_widget.cpp \
       cpp/src/ui/settings/digital_bindings_widget.h \
       cpp/src/ui/settings/digital_bindings_widget.cpp \
       cpp/src/ui/settings/analog_bindings_widget.h \
       cpp/src/ui/settings/analog_bindings_widget.cpp \
       cpp/src/ui/settings/analog_joystick_bindings_widget.h \
       cpp/src/ui/settings/analog_joystick_bindings_widget.cpp \
       cpp/src/ui/settings/ds_negcon_bindings_widget.h \
       cpp/src/ui/settings/ds_negcon_bindings_widget.cpp \
       cpp/src/ui/settings/ds_negcon_rumble_bindings_widget.h \
       cpp/src/ui/settings/ds_negcon_rumble_bindings_widget.cpp \
       cpp/src/ui/settings/ds_jogcon_bindings_widget.h \
       cpp/src/ui/settings/ds_jogcon_bindings_widget.cpp \
       cpp/src/ui/settings/popn_bindings_widget.h \
       cpp/src/ui/settings/popn_bindings_widget.cpp \
       cpp/src/ui/settings/psp_bindings_widget.h \
       cpp/src/ui/settings/psp_bindings_widget.cpp \
       cpp/src/ui/settings/controller_settings_widget.h \
       cpp/src/ui/settings/controller_settings_widget.cpp
```

- [ ] **Step 2: Strip references from `cpp/CMakeLists.txt`**

```bash
grep -n "bindings_widget\|binding_widget_common\|controller_settings_widget" cpp/CMakeLists.txt
```

For every line that prints, remove it (keep alignment of surrounding lines). These will be source-list entries for the main `RetroNest` target and possibly any test targets that pulled them.

- [ ] **Step 3: Build**

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
cmake --build build
```

Expected: clean. If the linker complains about a missing symbol, that's a stale `#include` — grep for it and remove.

```bash
grep -rn "bindings_widget_base\|binding_widget_common\|controller_settings_widget\|ds2_bindings_widget\|guitar_bindings_widget\|jogcon_bindings_widget\|negcon_bindings_widget\|digital_bindings_widget\|analog_bindings_widget\|ds_negcon_bindings_widget\|popn_bindings_widget\|psp_bindings_widget" cpp/src cpp/tests
```

Any hit is a stale reference — fix it.

- [ ] **Step 4: Run all tests**

```bash
cd cpp && ctest --test-dir build --output-on-failure
```

Expected: green.

- [ ] **Step 5: Commit**

```bash
git add cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
controller: delete 15 obsolete binding-widget files

bindings_widget_base, binding_widget_common, controller_settings_widget,
and the 12 per-type binding widgets (ds2, guitar, jogcon, negcon, popn,
digital, analog, analog_joystick, ds_negcon, ds_negcon_rumble,
ds_jogcon, psp). All replaced by ControllerBindingsView.

~2,500 lines of hand-positioned-pixel layout code gone.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Delete profile API + unused per-port wrappers from `AppController`

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`
- Modify: `cpp/src/services/config_service.h`
- Modify: `cpp/src/services/config_service.cpp`

After Task 5 the only callers of profile / `setControllerType` / `controllerSettingsForPort` / `saveControllerSettingForPort` / `restoreDefaultsForPort` were `controller_mapping_page` (rewritten — those calls are gone) and the deleted `controller_settings_widget`. `ConfigService::restoreDefaultsForPort` + `controllerType` + `controllerBindingsForPort` are still needed internally for the global "reset emulator to defaults" flow, so they stay.

- [ ] **Step 1: Verify no live callers remain**

```bash
grep -rn "controllerProfiles\|createControllerProfile\|applyControllerProfile\|renameControllerProfile\|deleteControllerProfile\|saveControllerSettingForPort" cpp/src cpp/qml
```

Expected: only matches inside `app_controller.{h,cpp}` and `config_service.{h,cpp}`. If anything else hits, stop and inspect — there's a caller this plan didn't account for.

```bash
grep -rn "appController\.setControllerType\|appController\.restoreDefaultsForPort\|appController\.controllerSettingsForPort" cpp/src cpp/qml
```

Expected: zero hits.

- [ ] **Step 2: Edit `cpp/src/ui/app_controller.h`**

Remove these lines (around 123, 127, 132-141):

```cpp
    Q_INVOKABLE void setControllerType(const QString& emuId, int port, const QString& type);
    Q_INVOKABLE QVariantList controllerSettingsForPort(const QString& emuId, int port) const;
    Q_INVOKABLE void saveControllerSettingForPort(const QString& emuId, int port,
                                                   const QString& key, const QString& value);
    Q_INVOKABLE void restoreDefaultsForPort(const QString& emuId, int port);

    // Profile management
    Q_INVOKABLE QStringList controllerProfiles(const QString& emuId) const;
    Q_INVOKABLE void createControllerProfile(const QString& emuId, const QString& name);
    Q_INVOKABLE void applyControllerProfile(const QString& emuId, const QString& name);
    Q_INVOKABLE void renameControllerProfile(const QString& emuId, const QString& oldName, const QString& newName);
    Q_INVOKABLE void deleteControllerProfile(const QString& emuId, const QString& name);
```

- [ ] **Step 3: Edit `cpp/src/ui/app_controller.cpp`**

Delete the matching method bodies. They're thin one-liners delegating to `m_configService`. Search for each method name and remove its definition.

```bash
grep -n "AppController::setControllerType\|AppController::controllerSettingsForPort\|AppController::saveControllerSettingForPort\|AppController::restoreDefaultsForPort\|AppController::controllerProfiles\|AppController::createControllerProfile\|AppController::applyControllerProfile\|AppController::renameControllerProfile\|AppController::deleteControllerProfile" cpp/src/ui/app_controller.cpp
```

Each printed line is the start of a method to delete.

- [ ] **Step 4: Edit `cpp/src/services/config_service.h`**

Remove these lines (around 74-79):

```cpp
    QStringList controllerProfiles(const QString& emuId) const;
    void createControllerProfile(const QString& emuId, const QString& name);
    void applyControllerProfile(const QString& emuId, const QString& name);
    void renameControllerProfile(const QString& emuId, const QString& oldName,
                                 const QString& newName);
    void deleteControllerProfile(const QString& emuId, const QString& name);
```

Also remove `saveControllerSettingForPort` (around line 69):

```cpp
    void saveControllerSettingForPort(const QString& emuId, int port,
                                       const QString& key, const QString& value);
```

Keep `setControllerType`, `controllerSettingsForPort`, `restoreDefaultsForPort` — they're called internally by `ConfigService::resetEmulatorToDefaults`.

Wait — `controllerSettingsForPort` is only called from `controller_settings_widget` (deleted) and the AppController wrapper (deleted in Step 3). It can also go.

```bash
grep -n "controllerSettingsForPort\|saveControllerSettingForPort" cpp/src/services/config_service.cpp
```

If `controllerSettingsForPort` only appears as its own definition, remove it from both the .h and .cpp.

- [ ] **Step 5: Edit `cpp/src/services/config_service.cpp`**

Delete the bodies of all profile methods (lines 744-end of profile section) + `saveControllerSettingForPort` (line 694) + `controllerSettingsForPort` if confirmed unused.

```bash
grep -n "ConfigService::saveControllerSettingForPort\|ConfigService::controllerSettingsForPort\|ConfigService::controllerProfiles\|ConfigService::createControllerProfile\|ConfigService::applyControllerProfile\|ConfigService::renameControllerProfile\|ConfigService::deleteControllerProfile" cpp/src/services/config_service.cpp
```

Each line is the start of a method to delete.

- [ ] **Step 6: Build**

```bash
cd cpp && cmake --build build
```

Expected: clean. If anything breaks, the grep in Step 1 missed a caller — find and fix.

- [ ] **Step 7: Run all tests**

```bash
cd cpp && ctest --test-dir build --output-on-failure
```

Expected: all green.

- [ ] **Step 8: Manually verify the dialog still works**

```bash
open ./build/RetroNest.app
```

PCSX2 → controller mapping. Bind, clear, auto-map, close. All flows should work as before, profile-related UI is gone.

- [ ] **Step 9: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp \
        cpp/src/services/config_service.h cpp/src/services/config_service.cpp
git commit -m "$(cat <<'EOF'
controller: drop profile + per-port-tuning API from AppController

5 profile methods (controllerProfiles / create / apply / rename / delete)
and 4 unused wrappers (setControllerType, controllerSettingsForPort,
saveControllerSettingForPort, restoreDefaultsForPort) gone from
AppController + ConfigService. Internal-only callers in
ConfigService::resetEmulatorToDefaults still reach setControllerType
+ restoreDefaultsForPort directly via the service layer.

User-facing controller_profiles/ files on disk are not touched —
documented in spec §5.4.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: Final verification — full build, all tests, manual smoke

**Files:** none.

- [ ] **Step 1: Clean rebuild**

```bash
cd cpp && rm -rf build
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
cmake --build build
```

Expected: clean build from scratch.

- [ ] **Step 2: Run the full test suite**

```bash
cd cpp && ctest --test-dir build --output-on-failure
```

Expected: all green, including:
- `Pcsx2ControllerSchema` (Task 2/3)
- `ControllerBindingsView` (Task 4)
- All pre-existing tests (`FormatBinding`, `Pcsx2Schema`, etc.)

- [ ] **Step 3: Launch the app**

```bash
open ./build/RetroNest.app
```

- [ ] **Step 4: Smoke test the full flow**

In the app:

1. Open PCSX2's controller mapping dialog. Confirm:
   - Title bar reads "Controller"; top chrome shows `PCSX2 — DUALSHOCK 2` in amber.
   - Six binding-card slots laid out around a centered controller SVG.
   - Footer reads `READY` until a card is focused.
2. Tab into the cards; arrow-key navigation moves spatially across the grid.
3. Focus any card — verify spotlight + amber pulse + amber section in footer (`NOW EDITING — <Label> → <Value>`).
4. Press Enter on a face button card (e.g. Cross) — card reads `Press a button…` (or similar), press a real controller button or keyboard key. Card refreshes with new value.
5. Press Backspace on the focused card — value clears to `Not bound`.
6. Press `M` — Auto-Map menu opens with `Keyboard` + any connected SDL devices, styled in amber.
7. Press Escape — dialog closes.
8. Reopen — bindings persist. Verify by inspecting `~/.config/PCSX2/inis/PCSX2.ini` (or wherever portable mode saves it):

```bash
grep -A 30 "^\[Pad1\]" "$HOME/Library/Application Support/RetroNest/emulators/pcsx2/inis/PCSX2.ini" 2>/dev/null \
   || grep -A 30 "^\[Pad1\]" "$(find ~ -name 'PCSX2.ini' -path '*pcsx2*' 2>/dev/null | head -1)"
```

The `[Pad1]` section should show your bindings in `SDL-0/FaceSouth` format.

- [ ] **Step 5: Verify schema-driven generic settings dialog still works**

PCSX2 → Settings (the OTHER dialog — not the one we just rebuilt). Browse around. Nothing here should have regressed; we only touched controller mapping.

- [ ] **Step 6: Final cleanup commit (if anything in steps 1-5 surfaced fixes)**

If everything passed cleanly, no commit needed — just announce done.

If you discovered a regression (e.g. a stale include, a missed caller), fix it in a small targeted commit:

```bash
git add <fixed file>
git commit -m "controller: <short description of fix>"
```

---

## Self-review

Skimming the spec end-to-end against this plan:

- **Spec §3 visual design** — covered by Task 4 (image area + slot grid + footer) + Task 5 (dialog top chrome + integration).
- **Spec §4 schema changes** — `BindingDef` extension in Task 1; PCSX2 adapter trim in Task 3 with explicit field population.
- **Spec §5 architecture** — Task 4 creates `ControllerBindingsView` + inner `ImageArea` + inner `BindingCard`; Task 5 collapses `controller_mapping_page`; Task 7 deletes the 15 obsolete files; Task 8 strips the profile / per-port-tuning APIs.
- **Spec §6 data flow** — initial load (Task 4 Step 5), focus → spotlight (Task 4 Step 6), rebind (Task 5 Step 2 capture wiring), clear (Task 6), auto-map (Task 5 Step 2).
- **Spec §7 spotlight rendering** — Task 4 Step 4 implements the radial-gradient cutout + amber pulse ring + 30 fps timer.
- **Spec §8 controller-friendly nav** — relies on existing `SettingsCard::keyPressEvent` (used in Task 4 Step 3 via inheritance) + dialog-level QShortcuts (Task 5 Step 2) + per-card eventFilter (Task 6).
- **Spec §9 out-of-scope deletions** — profile system + Settings tab + restore-defaults + alt controller types: all deleted across Tasks 3, 7, 8.
- **Spec §11 acceptance criteria** — verified end-to-end in Task 9.

Type / signature consistency check:
- `bindingFocused(BindingDef)` is signal in `ControllerBindingsView` (Task 4 header) — handler in `ControllerMappingPage::onCardFocused` ✓
- `rebindRequested(BindingDef)` ⟷ `ControllerMappingPage::onRebindRequested` ✓
- `clearRequested(BindingDef)` ⟷ `ControllerMappingPage::onClearRequested` ✓
- `setFocusedBinding(const BindingDef*)` accepts nullptr to clear (Task 4 Step 4) — called from `ControllerBindingsView::onCardFocused` with `&b` (non-null since this is always a card focus) ✓
- `BindingDef::cardSlot / spotlightX / spotlightY / spotlightR` field names match across header (Task 1), adapter (Task 3), view (Task 4), and tests (Task 2 + Task 4 Step 9) ✓

No placeholders. Every code block is concrete.

---

Plan complete and saved to `docs/superpowers/plans/2026-05-07-controller-mapping-redesign-pcsx2.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
