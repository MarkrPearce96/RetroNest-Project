# PCSX2 Controller Mapping Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enhance the Qt Widgets ControllerMappingPage to match PCSX2's native controller settings: left sidebar port selector, controller type dropdown with per-type binding layouts (6 types), settings tab, and profile management bar.

**Architecture:** The existing single-class `ControllerMappingPage` is restructured into a shell dialog (sidebar + toolbar + stacked content + profile bar) that hosts swappable binding widgets per controller type and a shared settings widget. Backend adapter methods gain type and port parameters. Profile management stores named INI files under `{root}/config/controller_profiles/`.

**Tech Stack:** C++17, Qt6 Widgets (QDialog, QStackedWidget, QComboBox, QSlider, QLabel, QPushButton), SVG rendering (QSvgWidget), IniFile (existing hash-indexed reader/writer)

**Spec:** `docs/superpowers/specs/2026-04-03-controller-mapping-design.md`

**Build command:**
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```

---

## File Structure

### New files
| File | Responsibility |
|------|---------------|
| `cpp/src/core/controller_type_def.h` | `ControllerTypeDef` struct (id, displayName, svgResource) |
| `cpp/src/ui/settings/ds2_bindings_widget.h` | DualShock 2 bindings widget (header) |
| `cpp/src/ui/settings/ds2_bindings_widget.cpp` | DualShock 2 bindings widget (extracted from current dialog) |
| `cpp/src/ui/settings/guitar_bindings_widget.h` | Guitar bindings widget (header) |
| `cpp/src/ui/settings/guitar_bindings_widget.cpp` | Guitar bindings: centered SVG + flat button grid |
| `cpp/src/ui/settings/jogcon_bindings_widget.h` | Jogcon bindings widget (header) |
| `cpp/src/ui/settings/jogcon_bindings_widget.cpp` | Jogcon bindings: DS2-like but with dial controls |
| `cpp/src/ui/settings/negcon_bindings_widget.h` | NeGcon bindings widget (header) |
| `cpp/src/ui/settings/negcon_bindings_widget.cpp` | NeGcon bindings: simplified DS2-like layout |
| `cpp/src/ui/settings/popn_bindings_widget.h` | Pop'n Music bindings widget (header) |
| `cpp/src/ui/settings/popn_bindings_widget.cpp` | Pop'n bindings: centered SVG + flat 9-button row |
| `cpp/src/ui/settings/controller_settings_widget.h` | Controller settings tab widget (header) |
| `cpp/src/ui/settings/controller_settings_widget.cpp` | Settings tab: renders sliders/combos from SettingDef data |
| `cpp/qml/AppUI/images/controllers/DualShock_2.svg` | DualShock 2 controller diagram |
| `cpp/qml/AppUI/images/controllers/Guitar.svg` | Guitar controller diagram |
| `cpp/qml/AppUI/images/controllers/Jogcon.svg` | Jogcon controller diagram |
| `cpp/qml/AppUI/images/controllers/Negcon.svg` | NeGcon controller diagram |
| `cpp/qml/AppUI/images/controllers/Popn.svg` | Pop'n Music controller diagram |

### Modified files
| File | Changes |
|------|---------|
| `cpp/src/core/binding_def.h` | Add `ControllerTypeDef` include |
| `cpp/src/adapters/emulator_adapter.h` | Add `controllerTypes()`, overloaded `controllerBindingDefs(type)`, `controllerSettingDefs(type)` |
| `cpp/src/adapters/pcsx2_adapter.h` | Override new type-aware methods |
| `cpp/src/adapters/pcsx2_adapter.cpp` | Add Guitar/Jogcon/NeGcon/Pop'n bindings+settings, `controllerTypes()` |
| `cpp/src/ui/app_controller.h` | Add port-aware APIs, type management, profile management |
| `cpp/src/ui/app_controller.cpp` | Implement port-aware APIs, type management, profile management |
| `cpp/src/ui/settings/controller_mapping_page.h` | Restructure: sidebar, toolbar, stacked content, profile bar |
| `cpp/src/ui/settings/controller_mapping_page.cpp` | Rewrite: shell dialog with port/type/tab switching |
| `cpp/CMakeLists.txt` | Add new sources, headers, SVG resources |

---

### Task 1: Add ControllerTypeDef and SVG resources

**Files:**
- Create: `cpp/src/core/controller_type_def.h`
- Create: `cpp/qml/AppUI/images/controllers/DualShock_2.svg` (copy from references)
- Create: `cpp/qml/AppUI/images/controllers/Guitar.svg` (copy)
- Create: `cpp/qml/AppUI/images/controllers/Jogcon.svg` (copy)
- Create: `cpp/qml/AppUI/images/controllers/Negcon.svg` (copy)
- Create: `cpp/qml/AppUI/images/controllers/Popn.svg` (copy)
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create ControllerTypeDef struct**

Create `cpp/src/core/controller_type_def.h`:

```cpp
#pragma once

#include <QString>

/**
 * ControllerTypeDef — describes an available controller type for an emulator.
 */
struct ControllerTypeDef {
    QString id;          // "DualShock2", "Guitar", "NotConnected", etc.
    QString displayName; // "DualShock 2", "Guitar", etc.
    QString svgResource; // resource path: ":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg"
};
```

- [ ] **Step 2: Copy SVG resources from PCSX2 references**

```bash
mkdir -p cpp/qml/AppUI/images/controllers
cp references/pcsx2-master/pcsx2-qt/resources/images/DualShock_2.svg cpp/qml/AppUI/images/controllers/
cp references/pcsx2-master/pcsx2-qt/resources/images/Guitar.svg cpp/qml/AppUI/images/controllers/
cp references/pcsx2-master/pcsx2-qt/resources/images/Jogcon.svg cpp/qml/AppUI/images/controllers/
cp references/pcsx2-master/pcsx2-qt/resources/images/Negcon.svg cpp/qml/AppUI/images/controllers/
cp references/pcsx2-master/pcsx2-qt/resources/images/Popn.svg cpp/qml/AppUI/images/controllers/
```

- [ ] **Step 3: Register in CMakeLists.txt**

In `cpp/CMakeLists.txt`, add to the `HEADERS` list:
```
    src/core/controller_type_def.h
```

In the `appui_backing` RESOURCES section, add:
```
        qml/AppUI/images/controllers/DualShock_2.svg
        qml/AppUI/images/controllers/Guitar.svg
        qml/AppUI/images/controllers/Jogcon.svg
        qml/AppUI/images/controllers/Negcon.svg
        qml/AppUI/images/controllers/Popn.svg
```

- [ ] **Step 4: Build to verify**

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```

Expected: compiles with no errors.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/controller_type_def.h cpp/qml/AppUI/images/controllers/ cpp/CMakeLists.txt
git commit -m "feat: add ControllerTypeDef struct and controller SVG resources"
```

---

### Task 2: Add per-type adapter methods to EmulatorAdapter base

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h`

- [ ] **Step 1: Add new virtual methods**

In `emulator_adapter.h`, add `#include "core/controller_type_def.h"` to the includes.

Add these new virtual methods to the `EmulatorAdapter` class (after the existing `controllerBindingDefs()` method):

```cpp
    /**
     * Return available controller types for this emulator.
     * First entry is typically "NotConnected".
     */
    virtual QVector<ControllerTypeDef> controllerTypes() const { return {}; }

    /**
     * Return controller bindings for a specific controller type.
     * Default delegates to the type-agnostic controllerBindingDefs().
     */
    virtual QVector<BindingDef> controllerBindingDefsForType(const QString& type) const {
        Q_UNUSED(type);
        return controllerBindingDefs();
    }

    /**
     * Return controller settings for a specific controller type.
     * Default delegates to the type-agnostic controllerSettingDefs().
     */
    virtual QVector<SettingDef> controllerSettingDefsForType(const QString& type) const {
        Q_UNUSED(type);
        return controllerSettingDefs();
    }
```

- [ ] **Step 2: Build to verify**

```bash
cd cpp && cmake --build build
```

Expected: compiles — new methods have default implementations.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "feat: add per-type controller methods to EmulatorAdapter base"
```

---

### Task 3: Implement per-type bindings and settings in PCSX2Adapter

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.h`
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp`

- [ ] **Step 1: Add method declarations to header**

In `pcsx2_adapter.h`, add overrides after the existing declarations:

```cpp
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> controllerSettingDefsForType(const QString& type) const override;
```

- [ ] **Step 2: Implement controllerTypes()**

Add to `pcsx2_adapter.cpp`:

```cpp
QVector<ControllerTypeDef> PCSX2Adapter::controllerTypes() const {
    return {
        {"NotConnected", "Not Connected", ""},
        {"DualShock2",   "DualShock 2",   ":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg"},
        {"Guitar",       "Guitar",        ":/AppUI/qml/AppUI/images/controllers/Guitar.svg"},
        {"Jogcon",       "Jogcon",        ":/AppUI/qml/AppUI/images/controllers/Jogcon.svg"},
        {"Negcon",       "NeGcon",        ":/AppUI/qml/AppUI/images/controllers/Negcon.svg"},
        {"Popn",         "Pop'n Music",   ":/AppUI/qml/AppUI/images/controllers/Popn.svg"},
    };
}
```

- [ ] **Step 3: Implement controllerBindingDefsForType()**

Add to `pcsx2_adapter.cpp`. This dispatches to the right binding set. The `section` parameter in each `BindingDef` uses `"Pad"` as a placeholder — the caller (AppController) prepends the port number at runtime to form `"Pad1"` or `"Pad2"`.

```cpp
QVector<BindingDef> PCSX2Adapter::controllerBindingDefsForType(const QString& type) const {
    if (type == "DualShock2")
        return controllerBindingDefs(); // existing DS2 bindings

    if (type == "Guitar") {
        return {
            {BindingDef::Button, "Strum Up",   "Strum",   "Pad", "Up",     "SDL-0/+DPadUp"},
            {BindingDef::Button, "Strum Down",  "Strum",   "Pad", "Down",   "SDL-0/+DPadDown"},
            {BindingDef::Button, "Select",      "System",  "Pad", "Select", "SDL-0/+Back"},
            {BindingDef::Button, "Start",       "System",  "Pad", "Start",  "SDL-0/+Start"},
            {BindingDef::Button, "Green",       "Frets",   "Pad", "Green",  "SDL-0/+A"},
            {BindingDef::Button, "Red",         "Frets",   "Pad", "Red",    "SDL-0/+B"},
            {BindingDef::Button, "Yellow",      "Frets",   "Pad", "Yellow", "SDL-0/+Y"},
            {BindingDef::Button, "Blue",        "Frets",   "Pad", "Blue",   "SDL-0/+X"},
            {BindingDef::Button, "Orange",      "Frets",   "Pad", "Orange", "SDL-0/+LeftShoulder"},
            {BindingDef::Axis,   "Whammy",      "Analog",  "Pad", "Whammy", "SDL-0/+LeftY"},
            {BindingDef::Button, "Tilt",        "Analog",  "Pad", "Tilt",   "SDL-0/+LeftTrigger"},
        };
    }

    if (type == "Jogcon") {
        return {
            {BindingDef::Button, "Up",       "D-Pad",        "Pad", "Up",        "SDL-0/+DPadUp"},
            {BindingDef::Button, "Down",     "D-Pad",        "Pad", "Down",      "SDL-0/+DPadDown"},
            {BindingDef::Button, "Left",     "D-Pad",        "Pad", "Left",      "SDL-0/+DPadLeft"},
            {BindingDef::Button, "Right",    "D-Pad",        "Pad", "Right",     "SDL-0/+DPadRight"},
            {BindingDef::Button, "Triangle", "Face Buttons", "Pad", "Triangle",  "SDL-0/+Y"},
            {BindingDef::Button, "Circle",   "Face Buttons", "Pad", "Circle",    "SDL-0/+B"},
            {BindingDef::Button, "Cross",    "Face Buttons", "Pad", "Cross",     "SDL-0/+A"},
            {BindingDef::Button, "Square",   "Face Buttons", "Pad", "Square",    "SDL-0/+X"},
            {BindingDef::Button, "Select",   "System",       "Pad", "Select",    "SDL-0/+Back"},
            {BindingDef::Button, "Start",    "System",       "Pad", "Start",     "SDL-0/+Start"},
            {BindingDef::Button, "L1",       "Shoulders",    "Pad", "L1",        "SDL-0/+LeftShoulder"},
            {BindingDef::Button, "L2",       "Shoulders",    "Pad", "L2",        "SDL-0/+LeftTrigger"},
            {BindingDef::Button, "R1",       "Shoulders",    "Pad", "R1",        "SDL-0/+RightShoulder"},
            {BindingDef::Button, "R2",       "Shoulders",    "Pad", "R2",        "SDL-0/+RightTrigger"},
            {BindingDef::Axis,   "Dial Left",  "Dial",       "Pad", "DialLeft",  "SDL-0/-LeftX"},
            {BindingDef::Axis,   "Dial Right", "Dial",       "Pad", "DialRight", "SDL-0/+LeftX"},
        };
    }

    if (type == "Negcon") {
        return {
            {BindingDef::Button, "Up",    "D-Pad",        "Pad", "Up",         "SDL-0/+DPadUp"},
            {BindingDef::Button, "Down",  "D-Pad",        "Pad", "Down",       "SDL-0/+DPadDown"},
            {BindingDef::Button, "Left",  "D-Pad",        "Pad", "Left",       "SDL-0/+DPadLeft"},
            {BindingDef::Button, "Right", "D-Pad",        "Pad", "Right",      "SDL-0/+DPadRight"},
            {BindingDef::Button, "A",     "Face Buttons",  "Pad", "A",          "SDL-0/+A"},
            {BindingDef::Button, "B",     "Face Buttons",  "Pad", "B",          "SDL-0/+B"},
            {BindingDef::Button, "I",     "Face Buttons",  "Pad", "I",          "SDL-0/+X"},
            {BindingDef::Button, "II",    "Face Buttons",  "Pad", "II",         "SDL-0/+Y"},
            {BindingDef::Button, "Start", "System",        "Pad", "Start",      "SDL-0/+Start"},
            {BindingDef::Button, "L",     "Shoulders",     "Pad", "L",          "SDL-0/+LeftShoulder"},
            {BindingDef::Button, "R",     "Shoulders",     "Pad", "R",          "SDL-0/+RightShoulder"},
            {BindingDef::Axis,   "Twist Left",  "Twist",   "Pad", "TwistLeft",  "SDL-0/-LeftX"},
            {BindingDef::Axis,   "Twist Right", "Twist",   "Pad", "TwistRight", "SDL-0/+LeftX"},
            {BindingDef::Axis,   "LargeMotor",  "Motors",  "Pad", "LargeMotor", ""},
            {BindingDef::Axis,   "SmallMotor",  "Motors",  "Pad", "SmallMotor", ""},
        };
    }

    if (type == "Popn") {
        return {
            {BindingDef::Button, "Yellow Left",  "Buttons", "Pad", "YellowLeft",  ""},
            {BindingDef::Button, "Yellow Right", "Buttons", "Pad", "YellowRight", ""},
            {BindingDef::Button, "Blue Left",    "Buttons", "Pad", "BlueLeft",    ""},
            {BindingDef::Button, "Blue Right",   "Buttons", "Pad", "BlueRight",   ""},
            {BindingDef::Button, "White Left",   "Buttons", "Pad", "WhiteLeft",   ""},
            {BindingDef::Button, "White Right",  "Buttons", "Pad", "WhiteRight",  ""},
            {BindingDef::Button, "Green Left",   "Buttons", "Pad", "GreenLeft",   ""},
            {BindingDef::Button, "Green Right",  "Buttons", "Pad", "GreenRight",  ""},
            {BindingDef::Button, "Red",          "Buttons", "Pad", "Red",         ""},
            {BindingDef::Button, "Start",        "System",  "Pad", "Start",       "SDL-0/+Start"},
            {BindingDef::Button, "Select",       "System",  "Pad", "Select",      "SDL-0/+Back"},
        };
    }

    // NotConnected or unknown
    return {};
}
```

- [ ] **Step 4: Implement controllerSettingDefsForType()**

Add to `pcsx2_adapter.cpp`:

```cpp
QVector<SettingDef> PCSX2Adapter::controllerSettingDefsForType(const QString& type) const {
    if (type == "DualShock2")
        return controllerSettingDefs(); // existing 8 settings

    if (type == "Guitar") {
        return {
            {"", "", "", "Pad", "Deadzone",
             "Whammy Bar Deadzone", "Sets the whammy bar deadzone.",
             SettingDef::Int, "0", {}, 0, 100, 1, "", "%"},
            {"", "", "", "Pad", "AxisScale",
             "Whammy Bar Sensitivity", "Sets the whammy bar axis scaling factor.",
             SettingDef::Int, "100", {}, 0, 200, 1, "", "%"},
        };
    }

    if (type == "Jogcon") {
        return {
            {"", "", "", "Pad", "Deadzone",
             "Dial Deadzone", "Sets the dial deadzone.",
             SettingDef::Int, "0", {}, 0, 100, 1, "", "%"},
            {"", "", "", "Pad", "AxisScale",
             "Dial Sensitivity", "Sets the dial axis scaling factor.",
             SettingDef::Int, "100", {}, 0, 200, 1, "", "%"},
        };
    }

    if (type == "Negcon") {
        return {
            {"", "", "", "Pad", "Deadzone",
             "Twist Deadzone", "Sets the twist axis deadzone.",
             SettingDef::Int, "0", {}, 0, 100, 1, "", "%"},
            {"", "", "", "Pad", "AxisScale",
             "Twist Sensitivity", "Sets the twist axis scaling factor.",
             SettingDef::Int, "100", {}, 0, 200, 1, "", "%"},
        };
    }

    // Popn, NotConnected — no settings
    return {};
}
```

- [ ] **Step 5: Build to verify**

```bash
cd cpp && cmake --build build
```

Expected: compiles with no errors.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.h cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "feat: add per-type controller bindings and settings to PCSX2Adapter"
```

---

### Task 4: Add port-aware AppController APIs

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Add new method declarations to header**

Add to `app_controller.h` after the existing controller bindings section:

```cpp
    // Controller types (per-emulator)
    Q_INVOKABLE QVariantList controllerTypes(const QString& emuId) const;
    Q_INVOKABLE QString controllerType(const QString& emuId, int port) const;
    Q_INVOKABLE void setControllerType(const QString& emuId, int port, const QString& type);

    // Port-aware controller bindings
    Q_INVOKABLE QVariantList controllerBindingsForPort(const QString& emuId, int port) const;
    Q_INVOKABLE QVariantList controllerSettingsForPort(const QString& emuId, int port) const;
    Q_INVOKABLE void saveBindingForPort(const QString& emuId, int port, const QString& key, const QString& value);
    Q_INVOKABLE void clearBindingForPort(const QString& emuId, int port, const QString& key);
    Q_INVOKABLE void clearAllBindingsForPort(const QString& emuId, int port);
    Q_INVOKABLE void autoMapControllerForPort(const QString& emuId, int port, int deviceIndex);
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

- [ ] **Step 2: Implement controller type methods**

Add to `app_controller.cpp`:

```cpp
// ── Controller Types ────────────────────────────────────────

QVariantList AppController::controllerTypes(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QVariantList list;
    for (const auto& t : adapter->controllerTypes()) {
        QVariantMap item;
        item["id"] = t.id;
        item["displayName"] = t.displayName;
        item["svgResource"] = t.svgResource;
        list.append(item);
    }
    return list;
}

QString AppController::controllerType(const QString& emuId, int port) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return "NotConnected";

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return "NotConnected";

    IniFile ini;
    ini.load(configPath);
    QString section = QString("Pad%1").arg(port);
    QString type = ini.value(section, "Type");
    return type.isEmpty() ? "DualShock2" : type;
}

void AppController::setControllerType(const QString& emuId, int port, const QString& type) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = QString("Pad%1").arg(port);
    ini.setValue(section, "Type", type);

    if (ini.save(configPath))
        setStatus(QString("Controller type set to %1.").arg(type));
    else
        setStatus("Failed to save controller type.");
}
```

- [ ] **Step 3: Implement port-aware binding methods**

Add to `app_controller.cpp`:

```cpp
// ── Port-Aware Controller Bindings ──────────────────────────

QVariantList AppController::controllerBindingsForPort(const QString& emuId, int port) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    IniFile ini;
    if (!configPath.isEmpty())
        ini.load(configPath);

    QString type = ini.value(QString("Pad%1").arg(port), "Type");
    if (type.isEmpty()) type = "DualShock2";

    QString section = QString("Pad%1").arg(port);

    QVariantList list;
    for (const auto& def : adapter->controllerBindingDefsForType(type)) {
        QVariantMap item;
        item["label"] = def.label;
        item["group"] = def.group;
        item["section"] = section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;

        QString val = ini.value(section, def.key);
        item["currentValue"] = val.isEmpty() ? def.defaultValue : val;
        list.append(item);
    }
    return list;
}

QVariantList AppController::controllerSettingsForPort(const QString& emuId, int port) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    IniFile ini;
    if (!configPath.isEmpty())
        ini.load(configPath);

    QString type = ini.value(QString("Pad%1").arg(port), "Type");
    if (type.isEmpty()) type = "DualShock2";

    QString section = QString("Pad%1").arg(port);

    QVariantList list;
    for (const auto& def : adapter->controllerSettingDefsForType(type)) {
        QVariantMap item;
        item["label"] = def.label;
        item["tooltip"] = def.tooltip;
        item["section"] = section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;
        item["type"] = static_cast<int>(def.type);
        item["suffix"] = def.suffix;
        item["minVal"] = def.minVal;
        item["maxVal"] = def.maxVal;
        item["step"] = def.step;

        QVariantList opts;
        for (const auto& opt : def.options) {
            QVariantMap o;
            o["label"] = opt.first;
            o["value"] = opt.second;
            opts.append(o);
        }
        item["options"] = opts;

        QString val = ini.value(section, def.key);
        item["currentValue"] = val.isEmpty() ? def.defaultValue : val;
        list.append(item);
    }
    return list;
}

void AppController::saveBindingForPort(const QString& emuId, int port,
                                        const QString& key, const QString& value) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = QString("Pad%1").arg(port);
    ini.setValue(section, key, value);

    if (!ini.save(configPath))
        setStatus("Failed to save binding.");
}

void AppController::clearBindingForPort(const QString& emuId, int port, const QString& key) {
    saveBindingForPort(emuId, port, key, "");
}

void AppController::clearAllBindingsForPort(const QString& emuId, int port) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    QString type = controllerType(emuId, port);
    QString section = QString("Pad%1").arg(port);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(type))
        ini.setValue(section, def.key, "");

    if (ini.save(configPath))
        setStatus("Bindings cleared.");
    else
        setStatus("Failed to clear bindings.");
}

void AppController::autoMapControllerForPort(const QString& emuId, int port, int deviceIndex) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    QString type = controllerType(emuId, port);
    QString section = QString("Pad%1").arg(port);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(type)) {
        QString mapped = def.defaultValue;
        if (!mapped.isEmpty() && deviceIndex != 0) {
            mapped.replace("SDL-0/", QString("SDL-%1/").arg(deviceIndex));
        }
        ini.setValue(section, def.key, mapped);
    }

    if (ini.save(configPath))
        setStatus("Controller auto-mapped.");
    else
        setStatus("Failed to auto-map controller.");
}

void AppController::saveControllerSettingForPort(const QString& emuId, int port,
                                                  const QString& key, const QString& value) {
    saveBindingForPort(emuId, port, key, value); // same INI section
}

void AppController::restoreDefaultsForPort(const QString& emuId, int port) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    QString type = controllerType(emuId, port);
    QString section = QString("Pad%1").arg(port);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(type))
        ini.setValue(section, def.key, def.defaultValue);

    for (const auto& def : adapter->controllerSettingDefsForType(type))
        ini.setValue(section, def.key, def.defaultValue);

    if (ini.save(configPath))
        setStatus("Controller defaults restored.");
    else
        setStatus("Failed to restore defaults.");
}
```

- [ ] **Step 4: Implement profile management methods**

Add to `app_controller.cpp`:

```cpp
// ── Controller Profile Management ───────────────────────────

QStringList AppController::controllerProfiles(const QString& emuId) const {
    Q_UNUSED(emuId);
    QString profileDir = Paths::rootDir() + "/config/controller_profiles";
    QDir dir(profileDir);
    QStringList profiles;
    if (dir.exists()) {
        for (const auto& entry : dir.entryList({"*.ini"}, QDir::Files))
            profiles.append(entry.chopped(4)); // remove .ini
    }
    return profiles;
}

void AppController::createControllerProfile(const QString& emuId, const QString& name) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString profileDir = Paths::rootDir() + "/config/controller_profiles";
    QDir().mkpath(profileDir);

    QString srcPath = adapter->configFilePath();
    QString dstPath = profileDir + "/" + name + ".ini";

    if (srcPath.isEmpty()) return;

    // Copy current pad sections to profile
    IniFile src;
    src.load(srcPath);

    IniFile dst;
    for (int port = 1; port <= 2; port++) {
        QString section = QString("Pad%1").arg(port);
        // Copy all keys from this section
        for (const auto& key : src.keys(section))
            dst.setValue(section, key, src.value(section, key));
    }

    if (dst.save(dstPath))
        setStatus(QString("Profile '%1' created.").arg(name));
    else
        setStatus("Failed to create profile.");
}

void AppController::applyControllerProfile(const QString& emuId, const QString& name) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString profileDir = Paths::rootDir() + "/config/controller_profiles";
    QString profilePath = profileDir + "/" + name + ".ini";
    QString configPath = adapter->configFilePath();

    if (configPath.isEmpty() || !QFileInfo::exists(profilePath)) return;

    IniFile profile;
    profile.load(profilePath);

    IniFile config;
    config.load(configPath);

    for (int port = 1; port <= 2; port++) {
        QString section = QString("Pad%1").arg(port);
        for (const auto& key : profile.keys(section))
            config.setValue(section, key, profile.value(section, key));
    }

    if (config.save(configPath))
        setStatus(QString("Profile '%1' applied.").arg(name));
    else
        setStatus("Failed to apply profile.");
}

void AppController::renameControllerProfile(const QString& emuId, const QString& oldName,
                                             const QString& newName) {
    Q_UNUSED(emuId);
    QString profileDir = Paths::rootDir() + "/config/controller_profiles";
    QString oldPath = profileDir + "/" + oldName + ".ini";
    QString newPath = profileDir + "/" + newName + ".ini";

    if (QFile::rename(oldPath, newPath))
        setStatus(QString("Profile renamed to '%1'.").arg(newName));
    else
        setStatus("Failed to rename profile.");
}

void AppController::deleteControllerProfile(const QString& emuId, const QString& name) {
    Q_UNUSED(emuId);
    QString profileDir = Paths::rootDir() + "/config/controller_profiles";
    QString path = profileDir + "/" + name + ".ini";

    if (QFile::remove(path))
        setStatus(QString("Profile '%1' deleted.").arg(name));
    else
        setStatus("Failed to delete profile.");
}
```

- [ ] **Step 5: Add necessary includes to app_controller.cpp**

At the top of `app_controller.cpp`, add (if not already present):

```cpp
#include <QDir>
#include <QFileInfo>
```

- [ ] **Step 6: Check if IniFile has a keys() method**

The profile code uses `ini.keys(section)`. Read `cpp/src/core/ini_file.h` to check if this method exists. If not, add it:

```cpp
QStringList keys(const QString& section) const;
```

With implementation in `ini_file.cpp` that returns all keys within a section.

- [ ] **Step 7: Build to verify**

```bash
cd cpp && cmake --build build
```

Expected: compiles with no errors. Fix any missing includes or method signatures.

- [ ] **Step 8: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp cpp/src/core/ini_file.h cpp/src/core/ini_file.cpp
git commit -m "feat: add port-aware controller APIs and profile management to AppController"
```

---

### Task 5: Create the Controller Settings Widget

**Files:**
- Create: `cpp/src/ui/settings/controller_settings_widget.h`
- Create: `cpp/src/ui/settings/controller_settings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create the header**

Create `cpp/src/ui/settings/controller_settings_widget.h`:

```cpp
#pragma once

#include <QWidget>
#include <QMap>
#include <QSlider>
#include <QComboBox>
#include <QLabel>

class AppController;

/**
 * ControllerSettingsWidget — renders controller settings (sliders/combos)
 * from the adapter's controllerSettingDefsForType() data.
 */
class ControllerSettingsWidget : public QWidget {
    Q_OBJECT
public:
    ControllerSettingsWidget(AppController* appController,
                             const QString& emuId,
                             int port,
                             QWidget* parent = nullptr);

    void reload();

private:
    void buildUI();
    void onSliderChanged(const QString& key, int value);
    void onComboChanged(const QString& key, int index);

    AppController* m_appController;
    QString m_emuId;
    int m_port;

    struct SliderRow {
        QSlider* slider;
        QLabel* valueLabel;
        QString suffix;
    };

    struct ComboRow {
        QComboBox* combo;
        QVector<QString> values; // INI values indexed by combo index
    };

    QMap<QString, SliderRow> m_sliders;
    QMap<QString, ComboRow> m_combos;
};
```

- [ ] **Step 2: Create the implementation**

Create `cpp/src/ui/settings/controller_settings_widget.cpp`:

```cpp
#include "controller_settings_widget.h"
#include "ui/app_controller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVariantMap>

static const QString kBg        = "#242440";
static const QString kBoxColor  = "#2c2c4e";
static const QString kBoxBorder = "#3a3a60";
static const QString kAccent    = "#6c5ce7";
static const QString kTextPrimary   = "#e8e8ff";
static const QString kTextSecondary = "#a8a8cc";
static const QString kTextDim       = "#666666";

static const QString kSliderStyle = QStringLiteral(
    "QSlider::groove:horizontal { background: #353558; height: 6px; border-radius: 3px; }"
    "QSlider::handle:horizontal { background: %1; width: 14px; height: 14px;"
    "  margin: -4px 0; border-radius: 7px; }"
    "QSlider::sub-page:horizontal { background: %1; border-radius: 3px; }"
).arg(kAccent);

ControllerSettingsWidget::ControllerSettingsWidget(AppController* appController,
                                                   const QString& emuId,
                                                   int port,
                                                   QWidget* parent)
    : QWidget(parent)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_port(port)
{
    setStyleSheet(QString("background: %1;").arg(kBg));
    buildUI();
}

void ControllerSettingsWidget::buildUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 24, 32, 24);
    layout->setSpacing(16);

    QVariantList settings = m_appController->controllerSettingsForPort(m_emuId, m_port);

    for (const auto& s : settings) {
        auto map = s.toMap();
        QString label = map["label"].toString();
        QString tooltip = map["tooltip"].toString();
        QString key = map["key"].toString();
        QString suffix = map["suffix"].toString();
        int type = map["type"].toInt();
        QString currentValue = map["currentValue"].toString();
        double minVal = map["minVal"].toDouble();
        double maxVal = map["maxVal"].toDouble();

        if (type == 4) { // Combo
            auto* row = new QHBoxLayout();
            row->setSpacing(16);

            auto* labelWidget = new QWidget();
            auto* labelLayout = new QVBoxLayout(labelWidget);
            labelLayout->setContentsMargins(0, 0, 0, 0);
            labelLayout->setSpacing(2);

            auto* nameLabel = new QLabel(label);
            nameLabel->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(kTextPrimary));
            labelLayout->addWidget(nameLabel);

            if (!tooltip.isEmpty()) {
                auto* tipLabel = new QLabel(tooltip);
                tipLabel->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(kTextDim));
                tipLabel->setWordWrap(true);
                labelLayout->addWidget(tipLabel);
            }

            row->addWidget(labelWidget, 1);

            auto* combo = new QComboBox();
            combo->setFixedWidth(200);
            combo->setStyleSheet(QString(
                "QComboBox { background: #353558; color: %1; border: 1px solid %2;"
                "  border-radius: 4px; padding: 4px 8px; font-size: 12px; }"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView { background: #1e1e3a; color: %1;"
                "  selection-background-color: %3; }"
            ).arg(kTextPrimary, kBoxBorder, kAccent));

            QVariantList opts = map["options"].toList();
            ComboRow cr;
            for (const auto& opt : opts) {
                auto optMap = opt.toMap();
                combo->addItem(optMap["label"].toString());
                cr.values.append(optMap["value"].toString());
            }
            cr.combo = combo;

            // Set current value
            int idx = cr.values.indexOf(currentValue);
            if (idx >= 0) combo->setCurrentIndex(idx);

            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this, key](int index) { onComboChanged(key, index); });

            row->addWidget(combo);
            m_combos[key] = cr;
            layout->addLayout(row);

        } else { // Int/Float slider
            auto* nameLabel = new QLabel(label);
            nameLabel->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(kTextPrimary));

            if (!tooltip.isEmpty()) {
                auto* tipLabel = new QLabel(tooltip);
                tipLabel->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(kTextDim));
                tipLabel->setWordWrap(true);
                layout->addWidget(nameLabel);
                layout->addWidget(tipLabel);
            } else {
                layout->addWidget(nameLabel);
            }

            auto* sliderRow = new QHBoxLayout();
            sliderRow->setSpacing(12);

            auto* slider = new QSlider(Qt::Horizontal);
            slider->setMinimum(static_cast<int>(minVal));
            slider->setMaximum(static_cast<int>(maxVal));
            slider->setValue(currentValue.toInt());
            slider->setStyleSheet(kSliderStyle);

            auto* valueLabel = new QLabel(currentValue + suffix);
            valueLabel->setFixedWidth(50);
            valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            valueLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(kTextSecondary));

            connect(slider, &QSlider::valueChanged, this, [this, key, valueLabel, suffix](int val) {
                valueLabel->setText(QString::number(val) + suffix);
                onSliderChanged(key, val);
            });

            sliderRow->addWidget(slider, 1);
            sliderRow->addWidget(valueLabel);

            m_sliders[key] = {slider, valueLabel, suffix};
            layout->addLayout(sliderRow);
        }
    }

    layout->addStretch();
}

void ControllerSettingsWidget::reload() {
    QVariantList settings = m_appController->controllerSettingsForPort(m_emuId, m_port);
    for (const auto& s : settings) {
        auto map = s.toMap();
        QString key = map["key"].toString();
        QString currentValue = map["currentValue"].toString();

        auto sliderIt = m_sliders.find(key);
        if (sliderIt != m_sliders.end()) {
            sliderIt->slider->blockSignals(true);
            sliderIt->slider->setValue(currentValue.toInt());
            sliderIt->valueLabel->setText(currentValue + sliderIt->suffix);
            sliderIt->slider->blockSignals(false);
        }

        auto comboIt = m_combos.find(key);
        if (comboIt != m_combos.end()) {
            int idx = comboIt->values.indexOf(currentValue);
            if (idx >= 0) {
                comboIt->combo->blockSignals(true);
                comboIt->combo->setCurrentIndex(idx);
                comboIt->combo->blockSignals(false);
            }
        }
    }
}

void ControllerSettingsWidget::onSliderChanged(const QString& key, int value) {
    m_appController->saveControllerSettingForPort(m_emuId, m_port, key, QString::number(value));
}

void ControllerSettingsWidget::onComboChanged(const QString& key, int index) {
    auto it = m_combos.find(key);
    if (it != m_combos.end() && index >= 0 && index < it->values.size())
        m_appController->saveControllerSettingForPort(m_emuId, m_port, key, it->values[index]);
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/controller_settings_widget.cpp
```

Add to HEADERS:
```
    src/ui/settings/controller_settings_widget.h
```

- [ ] **Step 4: Build to verify**

```bash
cd cpp && cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/settings/controller_settings_widget.h cpp/src/ui/settings/controller_settings_widget.cpp cpp/CMakeLists.txt
git commit -m "feat: add ControllerSettingsWidget for per-type settings tab"
```

---

### Task 6: Extract DualShock 2 bindings into standalone widget

**Files:**
- Create: `cpp/src/ui/settings/ds2_bindings_widget.h`
- Create: `cpp/src/ui/settings/ds2_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create the header**

Create `cpp/src/ui/settings/ds2_bindings_widget.h`:

```cpp
#pragma once

#include <QWidget>
#include <QMap>
#include <QPushButton>
#include <QLabel>
#include <QString>

class SdlInputManager;
class AppController;

/**
 * DS2BindingsWidget — DualShock 2 visual binding layout.
 *
 * Extracted from the original ControllerMappingPage. Positions all 28
 * binding buttons in the PCSX2-native diamond + center + motor layout.
 */
class DS2BindingsWidget : public QWidget {
    Q_OBJECT
public:
    DS2BindingsWidget(SdlInputManager* inputManager,
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

- [ ] **Step 2: Create the implementation**

Create `cpp/src/ui/settings/ds2_bindings_widget.cpp`. This is largely extracted from the existing `controller_mapping_page.cpp`, with the following changes:
- Constructor takes `port` parameter
- Uses `m_appController->controllerBindingsForPort(m_emuId, m_port)` instead of `controllerBindings(m_emuId)`
- Uses `m_appController->saveBindingForPort()` and `clearBindingForPort()` instead of the old methods
- Uses the SVG `DualShock_2.svg` resource instead of `ps5_controller.png`
- No toolbar (toolbar is in the parent dialog)

The code is the same visual layout as the existing `controller_mapping_page.cpp` lines 84–455 (constructor + relayout), plus lines 459–541 (loadBindings, capture, finish). The key differences are:

1. Replace `m_appController->controllerBindings(m_emuId)` with `m_appController->controllerBindingsForPort(m_emuId, m_port)`
2. Replace `m_appController->saveBinding(m_emuId, ...)` with `m_appController->saveBindingForPort(m_emuId, m_port, key, formatted)`
3. Replace `m_appController->clearBinding(m_emuId, section, key)` with `m_appController->clearBindingForPort(m_emuId, m_port, key)`
4. Replace the PNG image loading with SVG:
   ```cpp
   m_imgLabel = new QLabel(this);
   QPixmap pix(":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg");
   if (!pix.isNull())
       m_imgLabel->setPixmap(pix.scaled(420, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
   ```
5. Remove all toolbar-related code (toolbar is now in the parent dialog)
6. Remove the dialog setup (setWindowTitle, setMinimumSize, etc. — this is a plain QWidget, not a QDialog)

Copy the full constructor, widget creation, `relayout()`, `loadBindings()`, `startCapture()`, `onBindingCaptured()`, `onKeyboardCaptured()`, and `finishCapture()` methods from `controller_mapping_page.cpp`, applying the substitutions above. Also copy the static helper functions (`makeLabel`, `makeBox`, `BindBtn` class) and colour constants.

- [ ] **Step 3: Add to CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/ds2_bindings_widget.cpp
```

Add to HEADERS:
```
    src/ui/settings/ds2_bindings_widget.h
```

- [ ] **Step 4: Build to verify**

```bash
cd cpp && cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/settings/ds2_bindings_widget.h cpp/src/ui/settings/ds2_bindings_widget.cpp cpp/CMakeLists.txt
git commit -m "feat: extract DualShock 2 bindings into standalone DS2BindingsWidget"
```

---

### Task 7: Create simple bindings widgets for Guitar, Jogcon, NeGcon, Pop'n

**Files:**
- Create: `cpp/src/ui/settings/guitar_bindings_widget.h`
- Create: `cpp/src/ui/settings/guitar_bindings_widget.cpp`
- Create: `cpp/src/ui/settings/jogcon_bindings_widget.h`
- Create: `cpp/src/ui/settings/jogcon_bindings_widget.cpp`
- Create: `cpp/src/ui/settings/negcon_bindings_widget.h`
- Create: `cpp/src/ui/settings/negcon_bindings_widget.cpp`
- Create: `cpp/src/ui/settings/popn_bindings_widget.h`
- Create: `cpp/src/ui/settings/popn_bindings_widget.cpp`
- Modify: `cpp/CMakeLists.txt`

Each of these follows the same pattern: a QWidget with a centered SVG image and a grid of binding buttons below it. They share the same `BindBtn` class, colour constants, `startCapture`/`finishCapture` pattern, and `loadBindings()` approach as DS2BindingsWidget but with simpler layouts.

- [ ] **Step 1: Create Guitar bindings widget**

**Header** (`guitar_bindings_widget.h`): Same structure as `ds2_bindings_widget.h` but class name `GuitarBindingsWidget`. No box members (no diamond groups). Just `m_imgLabel` and `m_bindingButtons`.

**Implementation** (`guitar_bindings_widget.cpp`):
- Centered `Guitar.svg` image at top (full width, height ~320)
- Below image: flat grid layout (QGridLayout) with 11 bindings in 2 rows:
  - Row 1: Start, Select, Orange, Whammy, Tilt
  - Row 2: Green, Red, Yellow, Blue (fret buttons)
- Each binding: group box with label + BindBtn button
- Same capture/save pattern as DS2, using `m_appController->controllerBindingsForPort()` and `saveBindingForPort()`
- No relayout needed — use QGridLayout (no absolute positioning)

- [ ] **Step 2: Create Jogcon bindings widget**

**Implementation** (`jogcon_bindings_widget.cpp`):
- Similar to DS2 3-column layout but simpler:
  - Left: D-Pad diamond (4 buttons)
  - Center: SVG image (`Jogcon.svg`), L1/L2/R1/R2, Select/Start, Dial Left/Right
  - Right: Face buttons diamond (4 buttons)
- Bottom: Large Motor, Small Motor
- Uses the same diamond layout helper pattern as DS2 but fewer groups

- [ ] **Step 3: Create NeGcon bindings widget**

**Implementation** (`negcon_bindings_widget.cpp`):
- Left: D-Pad diamond (4 buttons)
- Center: SVG image (`Negcon.svg`), L/R, Start, Twist Left/Right
- Right: Face buttons (A, B, I, II in diamond)
- Bottom: Large Motor, Small Motor

- [ ] **Step 4: Create Pop'n bindings widget**

**Implementation** (`popn_bindings_widget.cpp`):
- Centered `Popn.svg` image at top
- Below: flat horizontal row of 9 button bindings (Yellow L/R, Blue L/R, White L/R, Green L/R, Red)
- Below that: Start, Select
- No motors, no groups

- [ ] **Step 5: Add all to CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/guitar_bindings_widget.cpp
    src/ui/settings/jogcon_bindings_widget.cpp
    src/ui/settings/negcon_bindings_widget.cpp
    src/ui/settings/popn_bindings_widget.cpp
```

Add to HEADERS:
```
    src/ui/settings/guitar_bindings_widget.h
    src/ui/settings/jogcon_bindings_widget.h
    src/ui/settings/negcon_bindings_widget.h
    src/ui/settings/popn_bindings_widget.h
```

- [ ] **Step 6: Build to verify**

```bash
cd cpp && cmake --build build
```

- [ ] **Step 7: Commit**

```bash
git add cpp/src/ui/settings/guitar_bindings_widget.* cpp/src/ui/settings/jogcon_bindings_widget.* cpp/src/ui/settings/negcon_bindings_widget.* cpp/src/ui/settings/popn_bindings_widget.* cpp/CMakeLists.txt
git commit -m "feat: add Guitar, Jogcon, NeGcon, Pop'n binding widgets"
```

---

### Task 8: Rewrite ControllerMappingPage as the shell dialog

**Files:**
- Modify: `cpp/src/ui/settings/controller_mapping_page.h`
- Modify: `cpp/src/ui/settings/controller_mapping_page.cpp`

This is the main restructure. The dialog becomes a shell containing: left sidebar, toolbar, stacked content area (bindings/settings), and bottom profile bar.

- [ ] **Step 1: Rewrite the header**

Replace `cpp/src/ui/settings/controller_mapping_page.h` with:

```cpp
#pragma once

#include <QDialog>
#include <QComboBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QString>

class SdlInputManager;
class AppController;
class ControllerSettingsWidget;

/**
 * ControllerMappingPage — full controller settings dialog matching PCSX2 native.
 *
 * Layout: left sidebar (port list) | right content (toolbar + stacked bindings/settings)
 * Bottom: profile management bar.
 */
class ControllerMappingPage : public QDialog {
    Q_OBJECT

public:
    ControllerMappingPage(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          QWidget* parent = nullptr);

private slots:
    void onPortChanged(int row);
    void onTypeChanged(int index);
    void onBindingsClicked();
    void onSettingsClicked();
    void onAutoMap();
    void onClearMapping();
    void onRestoreDefaults();

    // Profile management
    void onNewProfile();
    void onApplyProfile();
    void onRenameProfile();
    void onDeleteProfile();

private:
    void buildUI();
    void loadPort(int port);
    void switchTab(int tab); // 0 = bindings, 1 = settings
    QWidget* createBindingsWidget(const QString& type);
    void updateSidebar();
    void refreshProfiles();

    SdlInputManager* m_inputManager;
    AppController* m_appController;
    QString m_emuId;
    int m_currentPort = 1;
    int m_currentTab = 0; // 0 = bindings, 1 = settings
    QString m_currentType;

    // Sidebar
    QListWidget* m_portList = nullptr;

    // Toolbar
    QComboBox* m_typeCombo = nullptr;
    QToolButton* m_bindingsBtn = nullptr;
    QToolButton* m_settingsBtn = nullptr;
    QToolButton* m_autoMapBtn = nullptr;
    QToolButton* m_clearMapBtn = nullptr;

    // Content
    QStackedWidget* m_contentStack = nullptr; // index 0 = bindings, 1 = settings
    QWidget* m_bindingsWidget = nullptr;
    ControllerSettingsWidget* m_settingsWidget = nullptr;

    // Profile bar
    QComboBox* m_profileCombo = nullptr;
    QPushButton* m_applyProfileBtn = nullptr;
    QPushButton* m_renameProfileBtn = nullptr;
    QPushButton* m_deleteProfileBtn = nullptr;
};
```

- [ ] **Step 2: Rewrite the implementation**

Replace `cpp/src/ui/settings/controller_mapping_page.cpp`. The new implementation:

1. **Constructor** — calls `buildUI()`, then `loadPort(1)`
2. **`buildUI()`** — creates the full layout:
   - Outer `QHBoxLayout` → sidebar (left) + right content `QVBoxLayout`
   - **Sidebar**: `QListWidget` with 2 items ("Controller Port 1", "Controller Port 2"), 180px fixed width, styled items with controller icon
   - **Toolbar**: `QHBoxLayout` with `QComboBox` (controller types) + `QToolButton` (Bindings, Settings) + spacer + `QToolButton` (Auto Map, Clear)
   - **Content**: `QStackedWidget` with index 0 = bindings widget, index 1 = settings widget
   - **Profile bar**: `QHBoxLayout` at bottom — "Editing Profile:" label, `QComboBox` (profiles), New/Apply/Rename/Delete buttons, spacer, Mapping Settings/Restore Defaults/Close buttons
3. **`loadPort(int port)`** — reads controller type from INI, populates type combo, creates bindings widget + settings widget for that port/type
4. **`switchTab(int tab)`** — sets `m_contentStack` index, toggles `QToolButton` checked state, shows/hides Auto Map + Clear buttons
5. **`createBindingsWidget(type)`** — factory that returns the right binding widget based on type string:
   ```cpp
   if (type == "DualShock2") return new DS2BindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
   if (type == "Guitar")     return new GuitarBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
   if (type == "Jogcon")     return new JogconBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
   if (type == "Negcon")     return new NegconBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
   if (type == "Popn")       return new PopnBindingsWidget(m_inputManager, m_appController, m_emuId, m_currentPort, this);
   return new QWidget(this); // NotConnected = empty
   ```
6. **`onTypeChanged(int index)`** — calls `setControllerType()`, rebuilds content widgets
7. **`onPortChanged(int row)`** — `loadPort(row + 1)`
8. **`updateSidebar()`** — updates sidebar item text with current type names
9. **Profile methods** — delegate to `m_appController` profile management methods

Style the dialog with the same colour scheme as the existing code (`kBg`, `kBoxColor`, etc.).

Dialog setup:
```cpp
setWindowTitle("Controller Settings");
setMinimumSize(1280, 750);
resize(1280, 750);
```

- [ ] **Step 3: Update includes**

At the top of `controller_mapping_page.cpp`, include the new widget headers:

```cpp
#include "ds2_bindings_widget.h"
#include "guitar_bindings_widget.h"
#include "jogcon_bindings_widget.h"
#include "negcon_bindings_widget.h"
#include "popn_bindings_widget.h"
#include "controller_settings_widget.h"
```

- [ ] **Step 4: Build to verify**

```bash
cd cpp && cmake --build build
```

- [ ] **Step 5: Run the app and test**

```bash
./cpp/build/EmulatorFrontend
```

Navigate to an emulator's detail page and click "Controller Mapping". Verify:
- Left sidebar shows Port 1 and Port 2
- Controller type dropdown works (switching between types changes the binding layout)
- Bindings tab shows the correct visual layout for DualShock 2
- Settings tab shows sliders and combos
- "Not Connected" shows empty content
- Profile bar buttons are present (functionality can be tested after)

- [ ] **Step 6: Commit**

```bash
git add cpp/src/ui/settings/controller_mapping_page.h cpp/src/ui/settings/controller_mapping_page.cpp
git commit -m "feat: rewrite ControllerMappingPage with sidebar, tabs, and profile bar"
```

---

### Task 9: Update showControllerMapping() call site

**Files:**
- Modify: `cpp/src/ui/app_controller.cpp` (the `showControllerMapping` method)

- [ ] **Step 1: Verify the existing call site still works**

The existing `showControllerMapping()` creates a `ControllerMappingPage` dialog — the constructor signature hasn't changed (still takes `SdlInputManager*`, `AppController*`, `emuId`, `parent`). Verify this compiles and runs correctly.

If the constructor signature changed in Task 8, update the call site:

```cpp
void AppController::showControllerMapping(const QString& emuId) {
    auto* dialog = new ControllerMappingPage(m_inputManager, this, emuId);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
```

- [ ] **Step 2: Build and run**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```

- [ ] **Step 3: Commit (if changes needed)**

```bash
git add cpp/src/ui/app_controller.cpp
git commit -m "fix: update showControllerMapping call site for new dialog structure"
```

---

### Task 10: Clean up and remove old ps5_controller.png reference

**Files:**
- Modify: `cpp/CMakeLists.txt` (optionally remove `ps5_controller.png` from RESOURCES if no longer used elsewhere)

- [ ] **Step 1: Check if ps5_controller.png is used elsewhere**

Search for `ps5_controller` in the codebase. If the only reference was in the old `controller_mapping_page.cpp` (now replaced by SVGs), remove it from the RESOURCES section in CMakeLists.txt. If it's used elsewhere (e.g. QML ControllerSettings.qml), leave it.

- [ ] **Step 2: Final build and smoke test**

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
./build/EmulatorFrontend
```

Full verification checklist:
- [ ] Dialog opens from emulator detail page
- [ ] Left sidebar shows 2 ports with correct type names
- [ ] Port switching loads different configs
- [ ] Controller type dropdown lists all 6 types
- [ ] Changing type swaps the binding layout + SVG image
- [ ] DualShock 2 bindings: all 28 buttons positioned correctly, click-to-bind works
- [ ] Guitar bindings: image + flat button grid
- [ ] Settings tab: sliders/combos render and save on change
- [ ] "Not Connected" shows empty content, disables tabs
- [ ] Auto Map menu lists connected controllers
- [ ] Clear Mapping resets all bindings
- [ ] Restore Defaults resets bindings and settings
- [ ] Profile bar: create, apply, rename, delete profiles
- [ ] Close button closes the dialog

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "chore: clean up old controller page references and finalize"
```
