# Dolphin Controller Mapping Migration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate Dolphin to the schema-driven `ControllerBindingsView`. Surface two new actions on the Dolphin detail page — **GameCube Controller** and **Wii Remote** — each opening the standard mapping page for that controller. Bindings persist into the existing `GCPadNew.ini` / `WiimoteNew.ini` files; edits round-trip with Dolphin's native UI.

**Architecture:** Extend the shared infra with three small additions (slot-title overrides, type-aware bindings-storage overloads, device-header hook). Soften the view's single-type assertion to a `find_if(types, id)` filter. Thread an explicit `controllerTypeId` through `ConfigService::{save,clear,clearAll,autoMap}BindingForPort` so the active dialog's type is the source of truth. Dolphin overrides the new hooks to route GCPad-vs-Wiimote and to write Dolphin's per-section `Device =` line.

**Tech Stack:** C++17, Qt6 (Widgets + QML), QtTest. Reuses existing `ControllerBindingsView`, `ControllerMappingPage`, `ConfigService`, `AppController`, `IniFile`.

**Reference spec:** `docs/superpowers/specs/2026-05-08-dolphin-controller-mapping-migration-design.md`.

**Reference recipe:** `docs/superpowers/specs/controller-mapping-migration-prompt.md` (steps 4 + 6 are reused for SVG calibration and schema tests).

---

## File Structure

**New files:**

| Path | Responsibility |
|---|---|
| `cpp/tests/test_dolphin_controller_schema.cpp` | Pin Dolphin's two-controller schema (types, routing, slot/spotlight, format-binding translation table, slot-title overrides). |

**Modified files:**

| Path | Change |
|---|---|
| `cpp/src/core/controller_type_def.h` | Add `slotTitleOverrides` field. |
| `cpp/src/adapters/emulator_adapter.h` | Add type-aware `controllerBindingsConfigFilePath(type)` and `controllerBindingsSection(port,type)` overloads (delegating defaults). Add `writeBindingDeviceHeader(...)` hook (no-op default). |
| `cpp/src/services/config_service.h` | Change signatures of `saveBindingForPort` / `clearBindingForPort` / `clearAllBindingsForPort` / `autoMapControllerForPort` to thread `controllerTypeId` (and optional `deviceIndex` on save). |
| `cpp/src/services/config_service.cpp` | Update implementations: route via type-aware adapter overloads, call `writeBindingDeviceHeader` after writes. |
| `cpp/src/ui/app_controller.h` | Mirror new signatures. Add `showControllerMapping(emuId, controllerTypeId)` overload. |
| `cpp/src/ui/app_controller.cpp` | Update method bodies; new overload picks/forwards type. |
| `cpp/src/ui/settings/controller_bindings_view.h` | Add `controllerTypeId` ctor arg + member. |
| `cpp/src/ui/settings/controller_bindings_view.cpp` | Soften assertion, look up type by id, consult `slotTitleOverrides` in `titleForSlot()`. |
| `cpp/src/ui/settings/controller_mapping_page.h` | Add `controllerTypeId` ctor arg + member. Update capture handlers to pass type+devIdx through. |
| `cpp/src/ui/settings/controller_mapping_page.cpp` | Pass type to view; update `saveBindingForPort` / `clearBindingForPort` / etc. call sites with new args. |
| `cpp/src/adapters/dolphin_adapter.h` | Switch `controllerTypes()` / `controllerBindingDefs()` from empty to real. Declare `controllerBindingDefsForType`, type-aware `controllerBindingsConfigFilePath` + `controllerBindingsSection`, `formatBinding`, `writeBindingDeviceHeader`. |
| `cpp/src/adapters/dolphin_adapter.cpp` | Implement all of the above. |
| `cpp/qml/AppUI/EmulatorDetailPage.qml` | For Dolphin, swap the single "Controller Mapping" row for two rows (GameCube / Wii) and update `maxIndex` + the action-handler switch. |
| `cpp/CMakeLists.txt` | Wire `test_dolphin_controller_schema` after the existing `test_dolphin_schema` block. |

**Deleted files:** none.

---

## Phase 1 — Shared infra extensions (additive)

These are no-op for PCSX2/DuckStation/PPSSPP — every existing test must still pass after each task.

### Task 1: Add `slotTitleOverrides` to `ControllerTypeDef`

**Files:**
- Modify: `cpp/src/core/controller_type_def.h`

- [ ] **Step 1: Edit `controller_type_def.h`**

Replace the file contents with:

```cpp
#pragma once

#include <QHash>
#include <QString>

/**
 * ControllerTypeDef — describes an available controller type for an emulator.
 *
 * `slotTitleOverrides` lets an emulator override the per-slot title rendered
 * by ControllerBindingsView (e.g. the Wii Remote shows "TILT" instead of
 * "LEFT ANALOG" above its tilt-axis bindings). Empty map → use the view's
 * built-in titles.
 */
struct ControllerTypeDef {
    QString id;
    QString displayName;
    QString svgResource;
    QHash<QString, QString> slotTitleOverrides = {};
};
```

- [ ] **Step 2: Build to confirm no breakage**

Run: `cd cpp && cmake --build build`
Expected: success — every existing call site uses brace-init with three fields, and the new field is defaulted.

- [ ] **Step 3: Run all tests**

Run: `ctest --test-dir cpp/build --output-on-failure`
Expected: all tests pass (32+ existing tests).

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/controller_type_def.h
git commit -m "controller-type: add slotTitleOverrides field for per-emulator slot labels

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Add type-aware overloads for bindings storage

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h`

- [ ] **Step 1: Add type-aware overloads after the existing `controllerBindingsSection(int port)` override block**

Find the existing code (around line 197–207):

```cpp
    virtual QString controllerBindingsConfigFilePath() const { return configFilePath(); }

    virtual QString controllerBindingsSection(int port) const {
        return QString("Pad%1").arg(port);
    }
```

Insert the following overloads **after** that block (keeping the existing methods untouched):

```cpp
    /**
     * Type-aware overload: where bindings for `controllerTypeId` are stored.
     * Default delegates to the no-arg form; override when an emulator routes
     * different controller types to different files (e.g. Dolphin: GameCube
     * → GCPadNew.ini, Wii Remote → WiimoteNew.ini).
     */
    virtual QString controllerBindingsConfigFilePath(const QString& controllerTypeId) const {
        Q_UNUSED(controllerTypeId);
        return controllerBindingsConfigFilePath();
    }

    /**
     * Type-aware overload: section name for `controllerTypeId` at `port`.
     * Default delegates to the port-only form; override when the section
     * depends on the controller type (e.g. Dolphin: "GCPad1" vs "Wiimote1").
     */
    virtual QString controllerBindingsSection(int port, const QString& controllerTypeId) const {
        Q_UNUSED(controllerTypeId);
        return controllerBindingsSection(port);
    }
```

- [ ] **Step 2: Build + tests**

Run: `cd cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all tests pass — these methods are not yet called from anywhere.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "adapter: add type-aware overloads for controllerBindingsConfigFilePath/Section

Default delegates to the existing no-arg forms so PCSX2/DuckStation/PPSSPP
inherit unchanged behaviour. Dolphin will override both to route GCPad
bindings to GCPadNew.ini and Wiimote bindings to WiimoteNew.ini.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Add `writeBindingDeviceHeader` hook

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h`

- [ ] **Step 1: Add forward-decl + hook**

At the top of `emulator_adapter.h`, near the existing forward decls (`class IniFile;` etc.), ensure these are declared:

```cpp
class IniFile;
class SdlInputManager;
```

Then immediately after the type-aware overloads added in Task 2, append:

```cpp
    /**
     * Optional hook called after `ConfigService` writes per-binding values
     * into `section` of the bindings file. Default is a no-op — most
     * emulators encode the device in each binding string (e.g. "SDL-0/A").
     *
     * Dolphin overrides this to write a section-wide
     * `Device = SDL/{deviceIndex}/{deviceName}` line, since Dolphin's
     * INI format keeps the device separate from the per-key element name.
     *
     * `deviceIndex < 0` means "no device captured" (e.g. clear-all flow,
     * or a keyboard-only capture). Adapters should treat `< 0` as a no-op.
     */
    virtual void writeBindingDeviceHeader(IniFile& ini,
                                          const QString& section,
                                          int deviceIndex,
                                          SdlInputManager* input) const {
        Q_UNUSED(ini); Q_UNUSED(section);
        Q_UNUSED(deviceIndex); Q_UNUSED(input);
    }
```

- [ ] **Step 2: Build + tests**

Run: `cd cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: pass.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "adapter: add writeBindingDeviceHeader hook for emulators with section-wide device

Default no-op; Dolphin will override to write 'Device = SDL/N/Name' into
the active GCPad/Wiimote section after the ConfigService finishes writing
per-binding values.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 2 — `ControllerBindingsView` + `ControllerMappingPage` updates

### Task 4: Honor `slotTitleOverrides` in the view's `titleForSlot()`

**Files:**
- Modify: `cpp/src/ui/settings/controller_bindings_view.cpp`

- [ ] **Step 1: Find `titleForSlot()` (around lines 60–75) and replace with an override-aware version**

Locate this code:

```cpp
static QString titleForSlot(const QString& slot) {
    if (slot == "DPad")             return "D-PAD";
    if (slot == "FaceButtons")      return "FACE BUTTONS";
    if (slot == "LeftAnalog")       return "LEFT ANALOG";
    if (slot == "RightAnalog")      return "RIGHT ANALOG";
    if (slot == "Shoulders")        return "SHOULDERS";
    if (slot == "LeftShoulders")    return "SHOULDERS";
    if (slot == "RightShoulders")   return "SHOULDERS";
    return {};
}
```

Replace with (note: takes `overrides` arg):

```cpp
static QString titleForSlot(const QString& slot,
                            const QHash<QString, QString>& overrides) {
    if (auto it = overrides.constFind(slot); it != overrides.constEnd())
        return it.value();
    if (slot == "DPad")             return "D-PAD";
    if (slot == "FaceButtons")      return "FACE BUTTONS";
    if (slot == "LeftAnalog")       return "LEFT ANALOG";
    if (slot == "RightAnalog")      return "RIGHT ANALOG";
    if (slot == "Shoulders")        return "SHOULDERS";
    if (slot == "LeftShoulders")    return "SHOULDERS";
    if (slot == "RightShoulders")   return "SHOULDERS";
    return {};
}
```

- [ ] **Step 2: Update the two call sites to pass the overrides map**

In the same file, search for `titleForSlot(` and update each call. There are two call sites — both inside `buildSlots()`. They currently look like:

```cpp
const QString title = titleForSlot(slot);
```

Replace each with:

```cpp
const QString title = titleForSlot(slot, m_slotTitleOverrides);
```

- [ ] **Step 3: Add the `m_slotTitleOverrides` member**

In `controller_bindings_view.h`, add after `int m_port;` (around line 97):

```cpp
    QHash<QString, QString> m_slotTitleOverrides;
```

- [ ] **Step 4: Initialise it in the ctor**

In `controller_bindings_view.cpp`, find where the ctor reads `types.first()` (around lines where `Q_ASSERT_X(types.size() == 1)` lives — exact line will shift in Task 5). For now, add right after the type is resolved:

```cpp
    m_slotTitleOverrides = type.slotTitleOverrides;
```

(Where `type` is the resolved `ControllerTypeDef`.)

- [ ] **Step 5: Build + tests**

Run: `cd cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: pass — no existing emulator sets `slotTitleOverrides`, so behavior is unchanged.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/ui/settings/controller_bindings_view.h cpp/src/ui/settings/controller_bindings_view.cpp
git commit -m "controller-view: honor ControllerTypeDef::slotTitleOverrides in slot titles

Defaults unchanged for every existing emulator; Wii Remote will populate
LeftAnalog→TILT, RightAnalog→IR POINTING, LeftShoulders→SHAKE.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Add `controllerTypeId` ctor arg to `ControllerBindingsView`; soften single-type assertion

**Files:**
- Modify: `cpp/src/ui/settings/controller_bindings_view.h`
- Modify: `cpp/src/ui/settings/controller_bindings_view.cpp`

- [ ] **Step 1: Update ctor signature in the header**

In `controller_bindings_view.h`, change:

```cpp
    ControllerBindingsView(SdlInputManager* inputManager,
                           AppController* appController,
                           const QString& emuId,
                           int port,
                           QWidget* parent = nullptr);
```

to:

```cpp
    ControllerBindingsView(SdlInputManager* inputManager,
                           AppController* appController,
                           const QString& emuId,
                           const QString& controllerTypeId,
                           int port,
                           QWidget* parent = nullptr);
```

And add a member:

```cpp
    QString m_controllerTypeId;
```

- [ ] **Step 2: Update ctor in the .cpp**

In `controller_bindings_view.cpp`, find the ctor head:

```cpp
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
```

Change to:

```cpp
ControllerBindingsView::ControllerBindingsView(SdlInputManager* inputManager,
                                                AppController* appController,
                                                const QString& emuId,
                                                const QString& controllerTypeId,
                                                int port,
                                                QWidget* parent)
    : QWidget(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_controllerTypeId(controllerTypeId)
    , m_port(port)
```

- [ ] **Step 3: Soften the type assertion**

Find the ctor body block (early in the function) that looks like:

```cpp
    auto types = appController->controllerTypes(emuId);
    Q_ASSERT_X(types.size() == 1, "ControllerBindingsView",
               "expected exactly one ControllerTypeDef per emulator");
    const auto first = types.first().toMap();
    const QString svgResource = first.value("svgResource").toString();
```

Replace with:

```cpp
    auto types = appController->controllerTypes(emuId);

    // Find the requested type. If the caller passed an empty id (legacy
    // single-type adapters), pick the first entry — preserves prior behavior.
    QVariantMap typeMap;
    if (controllerTypeId.isEmpty() && !types.isEmpty()) {
        typeMap = types.first().toMap();
    } else {
        for (const auto& v : types) {
            const auto m = v.toMap();
            if (m.value("id").toString() == controllerTypeId) {
                typeMap = m;
                break;
            }
        }
    }
    Q_ASSERT_X(!typeMap.isEmpty(), "ControllerBindingsView",
               qPrintable(QString("controller type '%1' not found in adapter list for '%2'")
                          .arg(controllerTypeId, emuId)));

    const QString svgResource = typeMap.value("svgResource").toString();
```

(Replace later references to `first.value(...)` similarly with `typeMap.value(...)`.)

- [ ] **Step 4: Wire `m_slotTitleOverrides` from the resolved type**

`AppController::controllerTypes` currently returns a `QVariantList` of `QVariantMap`s. The map needs a `slotTitleOverrides` key. Skip wiring through the QVariant chain for now and read directly from the adapter:

Replace the `m_slotTitleOverrides = type.slotTitleOverrides;` line added in Task 4 with a direct adapter read:

```cpp
    if (auto* adapter = AdapterRegistry::instance().adapterFor(emuId)) {
        for (const auto& t : adapter->controllerTypes()) {
            if (t.id == m_controllerTypeId
                || (m_controllerTypeId.isEmpty() && !adapter->controllerTypes().isEmpty()
                    && t.id == adapter->controllerTypes().first().id)) {
                m_slotTitleOverrides = t.slotTitleOverrides;
                break;
            }
        }
    }
```

Add the include at the top of `controller_bindings_view.cpp`:

```cpp
#include "adapters/adapter_registry.h"
```

- [ ] **Step 5: Update schema lookup in the ctor to use the explicit type**

Find:

```cpp
    const QString typeId = types.front().id;
    const QString svg    = types.front().svgResource;
    m_bindings = adapter->controllerBindingDefsForType(typeId);
```

Replace with (uses the resolved `typeMap` from Step 3):

```cpp
    const QString typeId = typeMap.value("id").toString();
    const QString svg    = typeMap.value("svgResource").toString();
    m_bindings = adapter->controllerBindingDefsForType(typeId);
```

The ctor's existing `m_imageArea->setControllerSvg(svg);` call below this block stays as-is.

- [ ] **Step 6: Update `reloadBindings()` to pass the controller type id**

Find (around line 696):

```cpp
        const QVariantList raw = m_appController->controllerBindingsForPort(m_emuId, m_port);
```

Replace with:

```cpp
        const QVariantList raw = m_appController->controllerBindingsForPort(
            m_emuId, m_port, m_controllerTypeId);
```

(The new third arg is added in Task 7.)

- [ ] **Step 7: Build**

Run: `cd cpp && cmake --build build`
Expected: build will fail at `ControllerMappingPage` (its only caller, ctor signature changed) and at `controllerBindingsForPort` (signature changes in Task 7). Both are intended — they're fixed by Tasks 6 and 7.

- [ ] **Step 8: Commit (deferred — wait for Task 9)**

Don't commit yet; the build is broken until Task 9.

---

### Task 6: Add `controllerTypeId` ctor arg to `ControllerMappingPage`

**Files:**
- Modify: `cpp/src/ui/settings/controller_mapping_page.h`
- Modify: `cpp/src/ui/settings/controller_mapping_page.cpp`

- [ ] **Step 1: Update header signature + member**

In `controller_mapping_page.h`, change:

```cpp
    ControllerMappingPage(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          QWidget* parent = nullptr);
```

to:

```cpp
    ControllerMappingPage(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          const QString& controllerTypeId,
                          QWidget* parent = nullptr);
```

Add member after `QString m_emuId;`:

```cpp
    QString m_controllerTypeId;
```

- [ ] **Step 2: Update ctor in `.cpp`**

Match the new signature:

```cpp
ControllerMappingPage::ControllerMappingPage(SdlInputManager* inputManager,
                                              AppController* appController,
                                              const QString& emuId,
                                              const QString& controllerTypeId,
                                              QWidget* parent)
    : QDialog(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_controllerTypeId(controllerTypeId)
```

- [ ] **Step 3: Forward type id to the view**

Find:

```cpp
    m_view = new ControllerBindingsView(inputManager, appController, emuId, /*port=*/1, this);
```

Replace with:

```cpp
    m_view = new ControllerBindingsView(inputManager, appController, emuId,
                                          controllerTypeId, /*port=*/1, this);
```

- [ ] **Step 4: Title chrome — pick the right type's displayName**

Find the title block:

```cpp
    if (auto types = m_appController->controllerTypes(emuId); !types.isEmpty()) {
        title->setText(types.first().toMap().value("displayName").toString().toUpper());
    }
```

Replace with:

```cpp
    const auto types = m_appController->controllerTypes(emuId);
    QString displayName;
    if (!controllerTypeId.isEmpty()) {
        for (const auto& v : types) {
            const auto m = v.toMap();
            if (m.value("id").toString() == controllerTypeId) {
                displayName = m.value("displayName").toString();
                break;
            }
        }
    } else if (!types.isEmpty()) {
        displayName = types.first().toMap().value("displayName").toString();
    }
    if (!displayName.isEmpty()) title->setText(displayName.toUpper());
```

- [ ] **Step 5: Build**

Run: `cd cpp && cmake --build build`
Expected: build still fails at `AppController::showControllerMapping` (Task 8) because the page ctor signature changed. Continue.

---

## Phase 3 — `ConfigService` / `AppController` signature updates

### Task 7: Update `ConfigService` to thread `controllerTypeId` + `deviceIndex`

**Files:**
- Modify: `cpp/src/services/config_service.h`
- Modify: `cpp/src/services/config_service.cpp`

- [ ] **Step 1: Update header signatures**

In `config_service.h`, replace:

```cpp
    QVariantList controllerBindingsForPort(const QString& emuId, int port) const;
    void saveBindingForPort(const QString& emuId, int port,
                             const QString& key, const QString& value);
    void clearBindingForPort(const QString& emuId, int port, const QString& key);
    void clearAllBindingsForPort(const QString& emuId, int port);
    void autoMapControllerForPort(const QString& emuId, int port, int deviceIndex);
```

with:

```cpp
    QVariantList controllerBindingsForPort(const QString& emuId, int port,
                                            const QString& controllerTypeId) const;
    void saveBindingForPort(const QString& emuId, int port,
                             const QString& controllerTypeId,
                             const QString& key, const QString& value,
                             int deviceIndex = -1);
    void clearBindingForPort(const QString& emuId, int port,
                              const QString& controllerTypeId,
                              const QString& key);
    void clearAllBindingsForPort(const QString& emuId, int port,
                                  const QString& controllerTypeId);
    void autoMapControllerForPort(const QString& emuId, int port,
                                   const QString& controllerTypeId,
                                   int deviceIndex);
```

- [ ] **Step 2: Update method bodies in `config_service.cpp`**

Replace the five implementations (around lines 549–659) with versions that use the explicit `controllerTypeId` everywhere instead of looking it up via `controllerType(emuId, port)`. Pass an `SdlInputManager*` into `writeBindingDeviceHeader` — `ConfigService` already has access via `m_inputManager`.

Replace the existing `controllerBindingsForPort` body (around line 549) with:

```cpp
QVariantList ConfigService::controllerBindingsForPort(const QString& emuId, int port,
                                                       const QString& controllerTypeId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString bindingsPath = adapter->controllerBindingsConfigFilePath(controllerTypeId);
    IniFile bindingsIni;
    if (!bindingsPath.isEmpty())
        bindingsIni.load(bindingsPath);

    QString section = adapter->controllerBindingsSection(port, controllerTypeId);

    QVariantList list;
    for (const auto& def : adapter->controllerBindingDefsForType(controllerTypeId)) {
        QVariantMap item;
        item["label"] = def.label;
        item["group"] = def.group;
        item["section"] = section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;
        item["currentValue"] = bindingsIni.value(section, def.key);
        list.append(item);
    }
    return list;
}
```

Replace the existing `saveBindingForPort` body with:

```cpp
void ConfigService::saveBindingForPort(const QString& emuId, int port,
                                        const QString& controllerTypeId,
                                        const QString& key, const QString& value,
                                        int deviceIndex) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath(controllerTypeId);
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = adapter->controllerBindingsSection(port, controllerTypeId);
    ini.setValue(section, key, value);

    if (deviceIndex >= 0)
        adapter->writeBindingDeviceHeader(ini, section, deviceIndex, m_inputManager);

    if (!ini.save(configPath))
        emit statusMessage("Failed to save binding.");
}
```

Replace `clearBindingForPort` with:

```cpp
void ConfigService::clearBindingForPort(const QString& emuId, int port,
                                         const QString& controllerTypeId,
                                         const QString& key) {
    saveBindingForPort(emuId, port, controllerTypeId, key, "", /*deviceIndex=*/-1);
}
```

Replace `clearAllBindingsForPort` with:

```cpp
void ConfigService::clearAllBindingsForPort(const QString& emuId, int port,
                                             const QString& controllerTypeId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath(controllerTypeId);
    if (configPath.isEmpty()) return;

    QString section = adapter->controllerBindingsSection(port, controllerTypeId);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(controllerTypeId))
        ini.setValue(section, def.key, "");

    if (ini.save(configPath))
        emit statusMessage("Bindings cleared.");
    else
        emit statusMessage("Failed to clear bindings.");
}
```

Replace `autoMapControllerForPort` with:

```cpp
void ConfigService::autoMapControllerForPort(const QString& emuId, int port,
                                              const QString& controllerTypeId,
                                              int deviceIndex) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath(controllerTypeId);
    if (configPath.isEmpty()) return;

    QString section = adapter->controllerBindingsSection(port, controllerTypeId);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(controllerTypeId)) {
        QString mapped = def.defaultValue;
        if (!mapped.isEmpty())
            mapped.replace("SDL-0/", QString("SDL-%1/").arg(deviceIndex));
        ini.setValue(section, def.key, mapped);
    }

    adapter->writeBindingDeviceHeader(ini, section, deviceIndex, m_inputManager);

    if (ini.save(configPath))
        emit statusMessage("Controller auto-mapped.");
    else
        emit statusMessage("Failed to auto-map controller.");
}
```

- [ ] **Step 3: Update `restoreDefaultsForPort` to iterate all controller types**

The "Reset Configuration" flow calls `restoreDefaultsForPort(emuId, port)` and currently uses the no-arg `controllerBindingsConfigFilePath()` plus a single type from `controllerType(emuId, port)`. For Dolphin that would only reset GCPad and silently leave Wiimote bindings untouched. Iterate over the adapter's controller types instead.

Replace the existing `restoreDefaultsForPort` body (around line 661) with:

```cpp
void ConfigService::restoreDefaultsForPort(const QString& emuId, int port) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    const auto types = adapter->controllerTypes();
    if (types.isEmpty()) {
        // Adapter has no in-app remap UI — nothing to reset here.
        // (Settings-only adapters: ensureConfig has already re-seeded
        //  any binding files via their own create-only logic.)
    }

    for (const auto& t : types) {
        QString bindingsPath = adapter->controllerBindingsConfigFilePath(t.id);
        if (bindingsPath.isEmpty()) continue;
        IniFile bindingsIni;
        bindingsIni.load(bindingsPath);
        QString bindingsSection = adapter->controllerBindingsSection(port, t.id);
        for (const auto& def : adapter->controllerBindingDefsForType(t.id))
            bindingsIni.setValue(bindingsSection, def.key, def.defaultValue);
        bindingsIni.save(bindingsPath);
    }

    QString configPath = adapter->configFilePath();
    if (!configPath.isEmpty()) {
        IniFile ini;
        ini.load(configPath);
        QString section = adapter->controllerSettingsSection(port);
        // Settings still keyed by the stored type — keep existing behavior.
        QString type = controllerType(emuId, port);
        for (const auto& def : adapter->controllerSettingDefsForType(type))
            ini.setValue(section, def.key, def.defaultValue);
        ini.save(configPath);
    }

    emit statusMessage("Controller defaults restored.");
}
```

- [ ] **Step 4: Build**

Run: `cd cpp && cmake --build build`
Expected: build still fails at `AppController` (Task 8 fixes it).

---

### Task 8: Update `AppController` signatures + add type-aware `showControllerMapping`

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Update header signatures**

In `app_controller.h`, replace:

```cpp
    Q_INVOKABLE QVariantList controllerBindingsForPort(const QString& emuId, int port) const;
    Q_INVOKABLE void saveBindingForPort(const QString& emuId, int port, const QString& key, const QString& value);
    Q_INVOKABLE void clearBindingForPort(const QString& emuId, int port, const QString& key);
    Q_INVOKABLE void clearAllBindingsForPort(const QString& emuId, int port);
    Q_INVOKABLE void autoMapControllerForPort(const QString& emuId, int port, int deviceIndex);
```

with:

```cpp
    Q_INVOKABLE QVariantList controllerBindingsForPort(const QString& emuId, int port,
                                                        const QString& controllerTypeId) const;
    Q_INVOKABLE void saveBindingForPort(const QString& emuId, int port,
                                          const QString& controllerTypeId,
                                          const QString& key, const QString& value,
                                          int deviceIndex = -1);
    Q_INVOKABLE void clearBindingForPort(const QString& emuId, int port,
                                           const QString& controllerTypeId,
                                           const QString& key);
    Q_INVOKABLE void clearAllBindingsForPort(const QString& emuId, int port,
                                                const QString& controllerTypeId);
    Q_INVOKABLE void autoMapControllerForPort(const QString& emuId, int port,
                                                const QString& controllerTypeId,
                                                int deviceIndex);
```

And replace:

```cpp
    Q_INVOKABLE void showControllerMapping(const QString& emuId);
```

with:

```cpp
    Q_INVOKABLE void showControllerMapping(const QString& emuId);
    Q_INVOKABLE void showControllerMapping(const QString& emuId,
                                            const QString& controllerTypeId);
```

- [ ] **Step 2: Update method bodies in `app_controller.cpp`**

Find the existing one-line forwarders (around lines 484–487 plus `controllerBindingsForPort`). Replace with:

```cpp
QVariantList AppController::controllerBindingsForPort(const QString& emuId, int port,
                                                       const QString& controllerTypeId) const {
    return m_configService.controllerBindingsForPort(emuId, port, controllerTypeId);
}
void AppController::saveBindingForPort(const QString& emuId, int port,
                                        const QString& controllerTypeId,
                                        const QString& key, const QString& value,
                                        int deviceIndex) {
    m_configService.saveBindingForPort(emuId, port, controllerTypeId, key, value, deviceIndex);
}
void AppController::clearBindingForPort(const QString& emuId, int port,
                                         const QString& controllerTypeId,
                                         const QString& key) {
    m_configService.clearBindingForPort(emuId, port, controllerTypeId, key);
}
void AppController::clearAllBindingsForPort(const QString& emuId, int port,
                                             const QString& controllerTypeId) {
    m_configService.clearAllBindingsForPort(emuId, port, controllerTypeId);
}
void AppController::autoMapControllerForPort(const QString& emuId, int port,
                                              const QString& controllerTypeId,
                                              int deviceIndex) {
    m_configService.autoMapControllerForPort(emuId, port, controllerTypeId, deviceIndex);
}
```

- [ ] **Step 3: Update `showControllerMapping`**

Replace the existing implementation:

```cpp
void AppController::showControllerMapping(const QString& emuId) {
    if (!m_inputManager) {
        qWarning() << "[AppController] No SdlInputManager set";
        return;
    }
    auto* dialog = new ControllerMappingPage(m_inputManager, this, emuId);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
```

with two methods:

```cpp
void AppController::showControllerMapping(const QString& emuId) {
    // Single-type emulators: pick the first/only controller type.
    QString typeId;
    const auto types = controllerTypes(emuId);
    if (!types.isEmpty()) typeId = types.first().toMap().value("id").toString();
    showControllerMapping(emuId, typeId);
}

void AppController::showControllerMapping(const QString& emuId,
                                           const QString& controllerTypeId) {
    if (!m_inputManager) {
        qWarning() << "[AppController] No SdlInputManager set";
        return;
    }
    auto* dialog = new ControllerMappingPage(m_inputManager, this, emuId,
                                              controllerTypeId);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
```

- [ ] **Step 4: Build**

Run: `cd cpp && cmake --build build`
Expected: build still fails at `controller_mapping_page.cpp`'s capture handlers (they call the old 4-arg `saveBindingForPort`). Task 9 fixes it.

---

### Task 9: Update `ControllerMappingPage` capture handlers

**Files:**
- Modify: `cpp/src/ui/settings/controller_mapping_page.cpp`

- [ ] **Step 1: Replace the three call sites that touch `saveBindingForPort` / `clearBindingForPort` / `clearAllBindingsForPort` / `autoMapControllerForPort`**

Find the SDL `bindingCaptured` lambda (around lines 90–108):

```cpp
    connect(m_inputManager, &SdlInputManager::bindingCaptured, this,
        [this](int devIdx, const QString& element, bool isAxis, bool positive){
            if (m_capturingKey.isEmpty()) return;
            const QString formatted = m_appController->formatCapturedBinding(
                m_emuId, devIdx, element, isAxis, positive);
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_capturingKey, formatted);
            ...
```

Replace the `saveBindingForPort` call with:

```cpp
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_controllerTypeId,
                                                  m_capturingKey, formatted,
                                                  /*deviceIndex=*/devIdx);
```

Find the `keyboardCaptured` lambda just below (around lines 109–121):

```cpp
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_capturingKey, keyString);
```

Replace with:

```cpp
            m_appController->saveBindingForPort(m_emuId, /*port=*/1,
                                                  m_controllerTypeId,
                                                  m_capturingKey, keyString,
                                                  /*deviceIndex=*/-1);
```

Find `onClearRequested` (around line 153):

```cpp
void ControllerMappingPage::onClearRequested(const BindingDef& b) {
    m_appController->clearBindingForPort(m_emuId, /*port=*/1, b.key);
    m_view->reloadBindings();
}
```

Replace with:

```cpp
void ControllerMappingPage::onClearRequested(const BindingDef& b) {
    m_appController->clearBindingForPort(m_emuId, /*port=*/1,
                                          m_controllerTypeId, b.key);
    m_view->reloadBindings();
}
```

Find `onAutoMapRequested` (around lines 158–194). Update the three calls inside it:

```cpp
    if (activeType == 0) {
        ...
        m_appController->clearAllBindingsForPort(m_emuId, /*port=*/1);
        ...
    } else {
        ...
            m_appController->clearAllBindingsForPort(m_emuId, /*port=*/1);
        ...
            m_appController->autoMapControllerForPort(m_emuId, /*port=*/1, devIdx);
```

to:

```cpp
    if (activeType == 0) {
        ...
        m_appController->clearAllBindingsForPort(m_emuId, /*port=*/1, m_controllerTypeId);
        ...
    } else {
        ...
            m_appController->clearAllBindingsForPort(m_emuId, /*port=*/1, m_controllerTypeId);
        ...
            m_appController->autoMapControllerForPort(m_emuId, /*port=*/1,
                                                        m_controllerTypeId, devIdx);
```

- [ ] **Step 2: Build**

Run: `cd cpp && cmake --build build`
Expected: success.

- [ ] **Step 3: Run all tests**

Run: `ctest --test-dir cpp/build --output-on-failure`
Expected: all tests pass — PCSX2/DuckStation/PPSSPP behaviour is unchanged because `showControllerMapping(emuId)` now picks the first/only type and forwards it.

- [ ] **Step 4: Commit Tasks 5–9 as one logical change**

```bash
git add cpp/src/ui/settings/controller_bindings_view.h \
        cpp/src/ui/settings/controller_bindings_view.cpp \
        cpp/src/ui/settings/controller_mapping_page.h \
        cpp/src/ui/settings/controller_mapping_page.cpp \
        cpp/src/services/config_service.h \
        cpp/src/services/config_service.cpp \
        cpp/src/ui/app_controller.h \
        cpp/src/ui/app_controller.cpp
git commit -m "controller-mapping: thread controllerTypeId through view + service

ControllerBindingsView and ControllerMappingPage gain a controllerTypeId
ctor arg; the view's single-type assertion is softened to 'requested type
exists in adapter list' (legacy single-type adapters still resolve via the
first/only entry). ConfigService::{save,clear,clearAll,autoMap}BindingForPort
take the type explicitly instead of looking it up — necessary for Dolphin
where the open dialog (GameCube vs Wii) is the source of truth, not a
stored preference. saveBindingForPort gains an optional deviceIndex used
by adapters that need to update a section-wide Device line.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 5: Smoke test PCSX2/DuckStation/PPSSPP mapping pages didn't regress**

Run: `open ./cpp/build/RetroNest.app`
Open PCSX2 → Controller Mapping → focus a card → press a button → confirm save. Repeat for DuckStation and PPSSPP. Close the app.

---

## Phase 4 — Dolphin schema (TDD)

### Task 10: Write failing schema test

**Files:**
- Create: `cpp/tests/test_dolphin_controller_schema.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create the test file with the full test set**

```cpp
// cpp/tests/test_dolphin_controller_schema.cpp
#include <QtTest>
#include <QSet>
#include "adapters/dolphin_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"

// Pins Dolphin's two-controller schema. Every binding clusters into a
// known cardSlot; physical buttons have non-zero spotlights; bindings
// route to GCPadNew.ini vs WiimoteNew.ini by type id; formatBinding
// translates SDL element names into Dolphin's expression syntax.

class TestDolphinControllerSchema : public QObject {
    Q_OBJECT

private:
    DolphinAdapter adapter_;

private slots:
    void testTwoControllerTypes() {
        const auto types = adapter_.controllerTypes();
        QCOMPARE(types.size(), 2);

        QSet<QString> ids;
        for (const auto& t : types) ids.insert(t.id);
        QCOMPARE(ids, QSet<QString>({"GCPad1", "Wiimote1"}));

        for (const auto& t : types) {
            if (t.id == "GCPad1") {
                QCOMPARE(t.displayName, QString("GameCube Controller"));
                QVERIFY(t.svgResource.endsWith("GameCube.svg"));
                QVERIFY(t.slotTitleOverrides.isEmpty());
            } else {
                QCOMPARE(t.displayName, QString("Wii Remote"));
                QVERIFY(t.svgResource.endsWith("Wii.svg"));
                QCOMPARE(t.slotTitleOverrides.value("LeftAnalog"),    QString("TILT"));
                QCOMPARE(t.slotTitleOverrides.value("RightAnalog"),   QString("IR POINTING"));
                QCOMPARE(t.slotTitleOverrides.value("LeftShoulders"), QString("SHAKE"));
            }
        }
    }

    void testNoControllerSettings() {
        QVERIFY(adapter_.controllerSettingDefs().isEmpty());
        QVERIFY(adapter_.controllerSettingDefsForType("GCPad1").isEmpty());
        QVERIFY(adapter_.controllerSettingDefsForType("Wiimote1").isEmpty());
    }

    void testGcPadBindingsCount() {
        QCOMPARE(adapter_.controllerBindingDefsForType("GCPad1").size(), 23);
    }

    void testWiimoteBindingsCount() {
        QCOMPARE(adapter_.controllerBindingDefsForType("Wiimote1").size(), 23);
    }

    void testBindingsHaveCardSlot() {
        const QSet<QString> validSlots{
            "DPad", "FaceButtons", "LeftAnalog", "RightAnalog",
            "LeftShoulders", "RightShoulders", "System",
        };
        for (const QString& type : {"GCPad1", "Wiimote1"}) {
            for (const auto& b : adapter_.controllerBindingDefsForType(type)) {
                QVERIFY2(!b.cardSlot.isEmpty(),
                    qPrintable(QString("[%1] '%2' has empty cardSlot").arg(type, b.label)));
                QVERIFY2(validSlots.contains(b.cardSlot),
                    qPrintable(QString("[%1] '%2' has unknown cardSlot '%3'")
                               .arg(type, b.label, b.cardSlot)));
            }
        }
    }

    void testGcPadPhysicalButtonsHaveSpotlight() {
        const QSet<QString> mustHaveSpotlight{
            "Up", "Down", "Left", "Right",
            "A", "B", "X", "Y",
            "Main Stick Up", "Main Stick Down", "Main Stick Left", "Main Stick Right",
            "C-Stick Up", "C-Stick Down", "C-Stick Left", "C-Stick Right",
            "L (digital)", "L-Analog", "R (digital)", "R-Analog", "Z",
            "Start",
        };
        for (const auto& b : adapter_.controllerBindingDefsForType("GCPad1")) {
            if (!mustHaveSpotlight.contains(b.label)) continue;
            QVERIFY2(b.spotlightR > 0,
                qPrintable(QString("GCPad '%1' must have spotlightR > 0").arg(b.label)));
        }
    }

    void testWiimotePhysicalButtonsHaveSpotlight() {
        // Shake/* and Rumble/Motor are abstract — exempt.
        const QSet<QString> mustHaveSpotlight{
            "Up", "Down", "Left", "Right",
            "A", "B", "1", "2",
            "Tilt Forward", "Tilt Backward", "Tilt Left", "Tilt Right",
            "IR Up", "IR Down", "IR Left", "IR Right",
            "Minus", "Plus", "Home",
        };
        for (const auto& b : adapter_.controllerBindingDefsForType("Wiimote1")) {
            if (!mustHaveSpotlight.contains(b.label)) continue;
            QVERIFY2(b.spotlightR > 0,
                qPrintable(QString("Wiimote '%1' must have spotlightR > 0").arg(b.label)));
        }
    }

    void testGcPadRoutesToGcPadFile() {
        QVERIFY(adapter_.controllerBindingsConfigFilePath("GCPad1").endsWith("GCPadNew.ini"));
        QCOMPARE(adapter_.controllerBindingsSection(1, "GCPad1"), QString("GCPad1"));
    }

    void testWiimoteRoutesToWiimoteFile() {
        QVERIFY(adapter_.controllerBindingsConfigFilePath("Wiimote1").endsWith("WiimoteNew.ini"));
        QCOMPARE(adapter_.controllerBindingsSection(1, "Wiimote1"), QString("Wiimote1"));
    }

    void testFormatBindingTranslation() {
        struct Case {
            QString sdlElement; bool isAxis; bool positive; QString expected;
        };
        const QVector<Case> cases = {
            {"FaceSouth",     false, false, "`Button S`"},
            {"FaceEast",      false, false, "`Button E`"},
            {"FaceWest",      false, false, "`Button W`"},
            {"FaceNorth",     false, false, "`Button N`"},
            {"DPadUp",        false, false, "`Pad N`"},
            {"DPadDown",      false, false, "`Pad S`"},
            {"DPadLeft",      false, false, "`Pad W`"},
            {"DPadRight",     false, false, "`Pad E`"},
            {"LeftShoulder",  false, false, "`Shoulder L`"},
            {"RightShoulder", false, false, "`Shoulder R`"},
            {"LeftTrigger",   true,  true,  "`Trigger L`"},
            {"RightTrigger",  true,  true,  "`Trigger R`"},
            {"LeftStick",     false, false, "`Thumb L`"},
            {"RightStick",    false, false, "`Thumb R`"},
            {"Back",          false, false, "`Back`"},
            {"Start",         false, false, "`Start`"},
            {"Guide",         false, false, "`Guide`"},
            {"LeftX",         true,  true,  "`Left X+`"},
            {"LeftX",         true,  false, "`Left X-`"},
            {"LeftY",         true,  true,  "`Left Y+`"},
            {"LeftY",         true,  false, "`Left Y-`"},
            {"RightX",        true,  true,  "`Right X+`"},
            {"RightX",        true,  false, "`Right X-`"},
            {"RightY",        true,  true,  "`Right Y+`"},
            {"RightY",        true,  false, "`Right Y-`"},
        };
        for (const auto& c : cases) {
            QCOMPARE(adapter_.formatBinding(0, c.sdlElement, c.isAxis, c.positive),
                     c.expected);
        }
    }
};

QTEST_MAIN(TestDolphinControllerSchema)
#include "test_dolphin_controller_schema.moc"
```

- [ ] **Step 2: Wire the test into `cpp/CMakeLists.txt`**

Find the existing `add_executable(test_dolphin_schema ...)` block (around line 546). Immediately after the `add_test(NAME DolphinSchema ...)` line for it, insert:

```cmake
add_executable(test_dolphin_controller_schema
    tests/test_dolphin_controller_schema.cpp
    src/adapters/dolphin_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_dolphin_controller_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_dolphin_controller_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_dolphin_controller_schema PRIVATE Qt6::Core Qt6::Network Qt6::Test chdr-static)
add_test(NAME DolphinControllerSchema COMMAND test_dolphin_controller_schema)
```

- [ ] **Step 3: Run only the new test (will fail to build until we implement the methods)**

Run: `cd cpp && cmake --build build --target test_dolphin_controller_schema 2>&1 | tail -40`
Expected: build fails with errors about `controllerBindingDefsForType("GCPad1")` returning unexpected results, types being empty, etc. — this is the expected failing-test starting state for the next tasks.

- [ ] **Step 4: Do not commit yet** — failing test, no impl. Commit comes at the end of Task 17.

---

### Task 11: Add Dolphin controllerTypes() returning two entries

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.h`
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`

- [ ] **Step 1: Update header**

In `dolphin_adapter.h`, replace these two empty inline definitions:

```cpp
    QVector<ControllerTypeDef> controllerTypes() const override { return {}; }
    QVector<BindingDef> controllerBindingDefs() const override { return {}; }
```

with declarations (move bodies to .cpp):

```cpp
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefs() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> controllerSettingDefs() const override { return {}; }
    QVector<SettingDef> controllerSettingDefsForType(const QString& type) const override {
        Q_UNUSED(type); return {};
    }

    QString controllerBindingsConfigFilePath(const QString& controllerTypeId) const override;
    QString controllerBindingsSection(int port, const QString& controllerTypeId) const override;

    QString formatBinding(int deviceIndex, const QString& element,
                          bool isAxis, bool positive) const override;

    void writeBindingDeviceHeader(IniFile& ini, const QString& section,
                                  int deviceIndex, SdlInputManager* input) const override;
```

Also update the doc comment block above the controllers section (the multi-line comment in the header that says "Controllers: default profiles are baked..." → "controllerTypes() returns empty"). Replace that paragraph with:

```cpp
    /**
     * Controllers — Dolphin exposes two: GameCube + Wii Remote. Each
     * routes bindings into its own INI file and section:
     *   GCPad1   → GCPadNew.ini   [GCPad1]
     *   Wiimote1 → WiimoteNew.ini [Wiimote1]
     * Default profiles are still baked at install time (create-only,
     * never overwritten); the in-app schema-driven UI writes through to
     * the same files from then on.
     */
```

Add the necessary forward decl at the top of the header:

```cpp
class IniFile;
class SdlInputManager;
```

- [ ] **Step 2: Implement controllerTypes() in `dolphin_adapter.cpp`**

Add at the end of `dolphin_adapter.cpp` (after `patchHotkeysIni` or near the end):

```cpp
// ============================================================================
// Controller types
// ============================================================================

QVector<ControllerTypeDef> DolphinAdapter::controllerTypes() const {
    ControllerTypeDef gcpad{
        "GCPad1", "GameCube Controller",
        ":/AppUI/qml/AppUI/images/controllers/GameCube.svg",
        /*slotTitleOverrides=*/{}
    };
    ControllerTypeDef wii{
        "Wiimote1", "Wii Remote",
        ":/AppUI/qml/AppUI/images/controllers/Wii.svg",
        /*slotTitleOverrides=*/{
            {"LeftAnalog",    "TILT"},
            {"RightAnalog",   "IR POINTING"},
            {"LeftShoulders", "SHAKE"},
        }
    };
    return {gcpad, wii};
}

QVector<BindingDef> DolphinAdapter::controllerBindingDefs() const {
    return controllerBindingDefsForType("GCPad1");
}
```

- [ ] **Step 3: Run schema test** *(only the type-related cases will pass; routing/bindings tests still fail)*

Run: `cd cpp && cmake --build build --target test_dolphin_controller_schema && ./build/test_dolphin_controller_schema -functions`
Expected: lists all the test methods.

Run: `./cpp/build/test_dolphin_controller_schema testTwoControllerTypes`
Expected: PASS.

Run: `./cpp/build/test_dolphin_controller_schema testNoControllerSettings`
Expected: PASS.

Other tests still fail until later tasks — that's expected.

---

### Task 12: Implement type-aware bindings-storage routing

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`

- [ ] **Step 1: Add `controllerBindingsConfigFilePath` + `controllerBindingsSection` overrides**

Add after the controllerTypes() block:

```cpp
QString DolphinAdapter::controllerBindingsConfigFilePath(const QString& controllerTypeId) const {
    if (controllerTypeId == "Wiimote1") return wiimoteIniPath();
    return gcpadIniPath();  // default for "GCPad1" or empty
}

QString DolphinAdapter::controllerBindingsSection(int port, const QString& controllerTypeId) const {
    Q_UNUSED(port);  // v1: port always 1
    return controllerTypeId.isEmpty() ? "GCPad1" : controllerTypeId;
}
```

- [ ] **Step 2: Run routing tests**

Run: `./cpp/build/test_dolphin_controller_schema testGcPadRoutesToGcPadFile testWiimoteRoutesToWiimoteFile`
Expected: PASS.

---

### Task 13: Implement `controllerBindingDefsForType("GCPad1")` (without spotlights)

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`

- [ ] **Step 1: Add the GCPad branch**

Append:

```cpp
QVector<BindingDef> DolphinAdapter::controllerBindingDefsForType(const QString& type) const {
    if (type == "GCPad1") return gcPadBindings();
    if (type == "Wiimote1") return wiimoteBindings();
    return {};
}
```

Then add the GCPad helper. Spotlight coords are placeholders (`1, 1, 1`) — Task 15 calibrates them against the SVG. Using `1, 1, 1` instead of `0, 0, 0` keeps the schema test for "physical buttons have spotlight" green; we'll replace with real numbers once we read the SVG.

Add to `dolphin_adapter.cpp` (near the controllerTypes block):

```cpp
namespace {

QVector<BindingDef> gcPadBindings() {
    return {
        // D-Pad
        {BindingDef::Button, "Up",    "D-Pad", "GCPad1", "D-Pad/Up",    "`Pad N`",
            "DPad", 1, 1, 1},
        {BindingDef::Button, "Down",  "D-Pad", "GCPad1", "D-Pad/Down",  "`Pad S`",
            "DPad", 1, 1, 1},
        {BindingDef::Button, "Left",  "D-Pad", "GCPad1", "D-Pad/Left",  "`Pad W`",
            "DPad", 1, 1, 1},
        {BindingDef::Button, "Right", "D-Pad", "GCPad1", "D-Pad/Right", "`Pad E`",
            "DPad", 1, 1, 1},

        // Face Buttons
        {BindingDef::Button, "A", "Face Buttons", "GCPad1", "Buttons/A", "`Button S`",
            "FaceButtons", 1, 1, 1},
        {BindingDef::Button, "B", "Face Buttons", "GCPad1", "Buttons/B", "`Button E`",
            "FaceButtons", 1, 1, 1},
        {BindingDef::Button, "X", "Face Buttons", "GCPad1", "Buttons/X", "`Button W`",
            "FaceButtons", 1, 1, 1},
        {BindingDef::Button, "Y", "Face Buttons", "GCPad1", "Buttons/Y", "`Button N`",
            "FaceButtons", 1, 1, 1},

        // Main Stick
        {BindingDef::Axis, "Main Stick Up",    "Main Stick", "GCPad1", "Main Stick/Up",    "`Left Y-`",
            "LeftAnalog", 1, 1, 1},
        {BindingDef::Axis, "Main Stick Down",  "Main Stick", "GCPad1", "Main Stick/Down",  "`Left Y+`",
            "LeftAnalog", 1, 1, 1},
        {BindingDef::Axis, "Main Stick Left",  "Main Stick", "GCPad1", "Main Stick/Left",  "`Left X-`",
            "LeftAnalog", 1, 1, 1},
        {BindingDef::Axis, "Main Stick Right", "Main Stick", "GCPad1", "Main Stick/Right", "`Left X+`",
            "LeftAnalog", 1, 1, 1},

        // C-Stick
        {BindingDef::Axis, "C-Stick Up",    "C-Stick", "GCPad1", "C-Stick/Up",    "`Right Y-`",
            "RightAnalog", 1, 1, 1},
        {BindingDef::Axis, "C-Stick Down",  "C-Stick", "GCPad1", "C-Stick/Down",  "`Right Y+`",
            "RightAnalog", 1, 1, 1},
        {BindingDef::Axis, "C-Stick Left",  "C-Stick", "GCPad1", "C-Stick/Left",  "`Right X-`",
            "RightAnalog", 1, 1, 1},
        {BindingDef::Axis, "C-Stick Right", "C-Stick", "GCPad1", "C-Stick/Right", "`Right X+`",
            "RightAnalog", 1, 1, 1},

        // Triggers / shoulder
        {BindingDef::Button, "L (digital)", "Triggers", "GCPad1", "Triggers/L",        "`Trigger L`",
            "LeftShoulders", 1, 1, 1},
        {BindingDef::Axis,   "L-Analog",    "Triggers", "GCPad1", "Triggers/L-Analog", "`Trigger L`",
            "LeftShoulders", 1, 1, 1},
        {BindingDef::Button, "R (digital)", "Triggers", "GCPad1", "Triggers/R",        "`Trigger R`",
            "RightShoulders", 1, 1, 1},
        {BindingDef::Axis,   "R-Analog",    "Triggers", "GCPad1", "Triggers/R-Analog", "`Trigger R`",
            "RightShoulders", 1, 1, 1},
        {BindingDef::Button, "Z",           "Buttons",  "GCPad1", "Buttons/Z",         "`Shoulder R`",
            "RightShoulders", 1, 1, 1},

        // System
        {BindingDef::Button, "Start", "System", "GCPad1", "Buttons/Start", "`Start`",
            "System", 1, 1, 1},
        {BindingDef::Axis,   "Rumble/Motor", "System", "GCPad1", "Rumble/Motor", "`Motor`",
            "System", 0, 0, 0},
    };
}

QVector<BindingDef> wiimoteBindings();  // forward — defined in next task

}  // namespace
```

Add the corresponding declarations (private helpers) to `dolphin_adapter.h` is unnecessary — they're file-local in the anonymous namespace.

- [ ] **Step 2: Run GCPad-specific tests**

Run: `cd cpp && cmake --build build --target test_dolphin_controller_schema && ./build/test_dolphin_controller_schema testGcPadBindingsCount testGcPadPhysicalButtonsHaveSpotlight`
Expected: PASS.

---

### Task 14: Implement `wiimoteBindings()` (without spotlights)

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`

- [ ] **Step 1: Replace the `wiimoteBindings()` forward declaration with the implementation**

In the same anonymous namespace as `gcPadBindings()`, replace `QVector<BindingDef> wiimoteBindings();` with:

```cpp
QVector<BindingDef> wiimoteBindings() {
    return {
        // D-Pad
        {BindingDef::Button, "Up",    "D-Pad", "Wiimote1", "D-Pad/Up",    "`Pad N`",
            "DPad", 1, 1, 1},
        {BindingDef::Button, "Down",  "D-Pad", "Wiimote1", "D-Pad/Down",  "`Pad S`",
            "DPad", 1, 1, 1},
        {BindingDef::Button, "Left",  "D-Pad", "Wiimote1", "D-Pad/Left",  "`Pad W`",
            "DPad", 1, 1, 1},
        {BindingDef::Button, "Right", "D-Pad", "Wiimote1", "D-Pad/Right", "`Pad E`",
            "DPad", 1, 1, 1},

        // Buttons
        {BindingDef::Button, "A", "Buttons", "Wiimote1", "Buttons/A", "`Button S`",
            "FaceButtons", 1, 1, 1},
        {BindingDef::Button, "B", "Buttons", "Wiimote1", "Buttons/B", "`Button E`",
            "FaceButtons", 1, 1, 1},
        {BindingDef::Button, "1", "Buttons", "Wiimote1", "Buttons/1", "`Button W`",
            "FaceButtons", 1, 1, 1},
        {BindingDef::Button, "2", "Buttons", "Wiimote1", "Buttons/2", "`Button N`",
            "FaceButtons", 1, 1, 1},

        // Tilt → LeftAnalog (override title TILT)
        {BindingDef::Axis, "Tilt Forward",  "Tilt", "Wiimote1", "Tilt/Forward",  "`Left Y-`",
            "LeftAnalog", 1, 1, 1},
        {BindingDef::Axis, "Tilt Backward", "Tilt", "Wiimote1", "Tilt/Backward", "`Left Y+`",
            "LeftAnalog", 1, 1, 1},
        {BindingDef::Axis, "Tilt Left",     "Tilt", "Wiimote1", "Tilt/Left",     "`Left X-`",
            "LeftAnalog", 1, 1, 1},
        {BindingDef::Axis, "Tilt Right",    "Tilt", "Wiimote1", "Tilt/Right",    "`Left X+`",
            "LeftAnalog", 1, 1, 1},

        // IR → RightAnalog (override title IR POINTING)
        {BindingDef::Axis, "IR Up",    "IR", "Wiimote1", "IR/Up",    "`Right Y-`",
            "RightAnalog", 1, 1, 1},
        {BindingDef::Axis, "IR Down",  "IR", "Wiimote1", "IR/Down",  "`Right Y+`",
            "RightAnalog", 1, 1, 1},
        {BindingDef::Axis, "IR Left",  "IR", "Wiimote1", "IR/Left",  "`Right X-`",
            "RightAnalog", 1, 1, 1},
        {BindingDef::Axis, "IR Right", "IR", "Wiimote1", "IR/Right", "`Right X+`",
            "RightAnalog", 1, 1, 1},

        // Shake → LeftShoulders (override title SHAKE) — abstract, no spotlight
        {BindingDef::Button, "Shake X", "Shake", "Wiimote1", "Shake/X", "`Shoulder L`",
            "LeftShoulders", 0, 0, 0},
        {BindingDef::Button, "Shake Y", "Shake", "Wiimote1", "Shake/Y", "`Shoulder L`",
            "LeftShoulders", 0, 0, 0},
        {BindingDef::Button, "Shake Z", "Shake", "Wiimote1", "Shake/Z", "`Shoulder L`",
            "LeftShoulders", 0, 0, 0},

        // System
        {BindingDef::Button, "Minus", "System", "Wiimote1", "Buttons/-",    "`Back`",
            "System", 1, 1, 1},
        {BindingDef::Button, "Plus",  "System", "Wiimote1", "Buttons/+",    "`Start`",
            "System", 1, 1, 1},
        {BindingDef::Button, "Home",  "System", "Wiimote1", "Buttons/Home", "`Guide`",
            "System", 1, 1, 1},
        {BindingDef::Axis,   "Rumble/Motor", "System", "Wiimote1", "Rumble/Motor", "`Motor`",
            "System", 0, 0, 0},
    };
}
```

- [ ] **Step 2: Run Wiimote-specific tests**

Run: `cd cpp && cmake --build build --target test_dolphin_controller_schema && ./build/test_dolphin_controller_schema testWiimoteBindingsCount testWiimotePhysicalButtonsHaveSpotlight testBindingsHaveCardSlot`
Expected: PASS.

---

### Task 15: Implement `formatBinding()` translation table

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`

- [ ] **Step 1: Append the implementation**

```cpp
// ============================================================================
// formatBinding — Dolphin expression syntax: bare element name in backticks,
// device communicated separately via the section's Device = line (see
// writeBindingDeviceHeader). Axis polarity is encoded in the element name
// for sticks; triggers stay polarity-less ("Trigger L" not "+Trigger L").
// ============================================================================

QString DolphinAdapter::formatBinding(int /*deviceIndex*/, const QString& element,
                                       bool isAxis, bool positive) const {
    // Stick axes: "Left X-" / "Left X+" etc. Triggers / non-stick axes stay
    // polarity-less.
    static const QHash<QString, QString> stickAxisRoot{
        {"LeftX",  "Left X"},
        {"LeftY",  "Left Y"},
        {"RightX", "Right X"},
        {"RightY", "Right Y"},
    };
    if (isAxis) {
        if (auto it = stickAxisRoot.constFind(element); it != stickAxisRoot.constEnd()) {
            const QString polarity = positive ? "+" : "-";
            return QString("`%1%2`").arg(it.value(), polarity);
        }
        // Non-stick axis (Trigger L/R)
        if (element == "LeftTrigger")  return "`Trigger L`";
        if (element == "RightTrigger") return "`Trigger R`";
    }

    static const QHash<QString, QString> buttonNames{
        {"FaceSouth",     "Button S"},
        {"FaceEast",      "Button E"},
        {"FaceWest",      "Button W"},
        {"FaceNorth",     "Button N"},
        {"DPadUp",        "Pad N"},
        {"DPadDown",      "Pad S"},
        {"DPadLeft",      "Pad W"},
        {"DPadRight",     "Pad E"},
        {"LeftShoulder",  "Shoulder L"},
        {"RightShoulder", "Shoulder R"},
        {"LeftStick",     "Thumb L"},
        {"RightStick",    "Thumb R"},
        {"Back",          "Back"},
        {"Start",         "Start"},
        {"Guide",         "Guide"},
    };
    if (auto it = buttonNames.constFind(element); it != buttonNames.constEnd())
        return QString("`%1`").arg(it.value());

    qWarning() << "[DolphinAdapter] unknown SDL element for formatBinding:" << element;
    return {};
}
```

- [ ] **Step 2: Run translation tests**

Run: `cd cpp && cmake --build build --target test_dolphin_controller_schema && ./build/test_dolphin_controller_schema testFormatBindingTranslation`
Expected: PASS.

- [ ] **Step 3: Run all schema tests**

Run: `./cpp/build/test_dolphin_controller_schema`
Expected: every method PASS (the spotlight tests pass with `1, 1, 1` placeholders — Task 17 calibrates real coords).

---

### Task 16: Implement `writeBindingDeviceHeader()`

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`

- [ ] **Step 1: Add includes near the top**

```cpp
#include "core/ini_file.h"
#include "core/sdl_input_manager.h"
```

If they're already there, skip.

- [ ] **Step 2: Implement after `formatBinding`**

```cpp
void DolphinAdapter::writeBindingDeviceHeader(IniFile& ini, const QString& section,
                                               int deviceIndex, SdlInputManager* input) const {
    if (deviceIndex < 0 || !input) return;

    QString deviceName;
    const auto controllers = input->connectedControllers();
    for (const auto& v : controllers) {
        const auto m = v.toMap();
        if (m.value("deviceIndex").toInt() == deviceIndex) {
            deviceName = m.value("name").toString();
            break;
        }
    }
    if (deviceName.isEmpty()) {
        // Couldn't resolve — leave the existing Device line untouched rather
        // than writing "SDL/N/" with a blank name (would break Dolphin's
        // parser).
        return;
    }
    ini.setValue(section, "Device", QString("SDL/%1/%2").arg(deviceIndex).arg(deviceName));
}
```

- [ ] **Step 3: Build + run all tests**

Run: `cd cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all 33+ tests pass (32 existing + new dolphin schema test).

- [ ] **Step 4: Commit Tasks 10–16 as a single coherent change**

```bash
git add cpp/src/adapters/dolphin_adapter.h \
        cpp/src/adapters/dolphin_adapter.cpp \
        cpp/tests/test_dolphin_controller_schema.cpp \
        cpp/CMakeLists.txt
git commit -m "dolphin: schema for GameCube + Wii Remote controller bindings

controllerTypes() returns GCPad1 + Wiimote1 (Wiimote populates
slotTitleOverrides for TILT / IR POINTING / SHAKE).
controllerBindingDefsForType() returns 23 bindings each, mirroring the
default profiles already baked at install time.

Type-aware controllerBindingsConfigFilePath/Section route GCPad1 to
GCPadNew.ini and Wiimote1 to WiimoteNew.ini.

formatBinding() translates SDL element names to Dolphin's expression
syntax (bare name in backticks, axis polarity for sticks only).

writeBindingDeviceHeader() updates the section-wide Device = line
when a binding capture or auto-map provides a device.

Spotlight coords are placeholder (1,1,1) for physical buttons; Task 17
calibrates them against the real SVG viewBox coordinates.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 17: Calibrate spotlight coordinates against the real SVGs

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`

- [ ] **Step 1: Inspect GameCube.svg for labelled elements**

Run:

```bash
grep -nE 'id="[A-Za-z_-]+"|cx="[0-9.]+" cy="[0-9.]+"' \
    cpp/qml/AppUI/images/controllers/GameCube.svg | head -60
```

Cross-reference labelled IDs (D-Pad cluster, Main Stick centre, C-Stick centre, A/B/X/Y face buttons, L/R triggers, Z, Start). The viewBox is `0 0 1802 1361`. Note centre coordinates and approximate radii (radius ≈ visible button size / 2 in viewBox units).

- [ ] **Step 2: Replace GCPad placeholder spotlights**

For each `1, 1, 1` triple in `gcPadBindings()`, replace with calibrated `(x, y, r)` extracted from the SVG. Convention:
- D-Pad: each direction sits on a circle around the cross's centre. Use centre + offset of approx the cluster radius.
- Main Stick / C-Stick: stick directions sit on a smaller circle around the stick centre.
- Face buttons (A/B/X/Y): each at the centre of its drawn shape.
- L / R / Z: at the rendered shoulder shape's centre.
- Start: at the small Start button centre.
- Rumble/Motor: leave `0, 0, 0` (abstract).
- L (digital) and L-Analog share the same physical input → use the same `(x, y, r)`. Same for R / R-Analog.

If you can't find a labelled element for a given binding, eyeball the position from the viewBox by opening the SVG in a browser. Record actual numbers — visually-plausible-but-wrong coordinates will misalign the spotlight (per the migration prompt).

- [ ] **Step 3: Inspect Wii.svg for labelled elements**

Run:

```bash
grep -nE 'id="[A-Za-z_-]+"|cx="[0-9.]+" cy="[0-9.]+"' \
    cpp/qml/AppUI/images/controllers/Wii.svg | head -60
```

Wiimote viewBox is `0 0 777 1614` (portrait). Cross-reference: D-Pad cluster (top), A button (centre face, just below D-Pad), B button (back trigger — visible if the SVG includes the underside), 1/2 buttons (lower face), -, Home, +.

- [ ] **Step 4: Replace Wiimote placeholder spotlights**

For each `1, 1, 1` triple in `wiimoteBindings()`, replace with calibrated coordinates. Conventions:
- Tilt: spotlight the Wiimote body / accelerometer area (rough centre of the controller).
- IR: spotlight the IR sensor at the front (top of the portrait SVG).
- Shake: leave at `0, 0, 0` (abstract — already done).
- Rumble/Motor: leave `0, 0, 0`.
- A, B, 1, 2, +, -, Home, D-Pad: actual button centres.

- [ ] **Step 5: Run the spotlight tests + manual sanity check**

Run: `cd cpp && cmake --build build --target test_dolphin_controller_schema && ./build/test_dolphin_controller_schema`
Expected: every test PASS (the spotlight tests still pass — they only check `R > 0`, not the actual values).

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp
git commit -m "dolphin: calibrate spotlight coords against GameCube.svg / Wii.svg viewBoxes

Replaces the (1,1,1) placeholders with real (x,y,r) triples extracted from
the SVGs. Spotlight will now align with the focused button on both
controllers' artwork.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 5 — UI wiring

### Task 18: Update `EmulatorDetailPage.qml` for Dolphin

**Files:**
- Modify: `cpp/qml/AppUI/EmulatorDetailPage.qml`

- [ ] **Step 1: Compute action count**

Find the `actionOffset` property and `Keys.onPressed`'s `maxIndex` (around line 31):

```qml
    property int actionOffset: biosButtonVisible ? 1 : 0

    // ── Keyboard / controller navigation ────────────────────────────────
    Keys.onPressed: function(event) {
        var maxIndex = root.emuInfo.installed ? (actionOffset + 5) : 0
```

Replace `maxIndex` with a Dolphin-aware version:

```qml
        var actionRows = root.emuId === "dolphin" ? 6 : 5
        var maxIndex = root.emuInfo.installed ? (actionOffset + actionRows) : 0
```

- [ ] **Step 2: Update `activateFocused()` switch**

Find (around line 58):

```qml
        var idx = root.focusIndex - actionOffset
        switch (idx) {
        case 0: app.showEmulatorSettings(root.emuId); break
        case 1: app.showControllerMapping(root.emuId); break
        case 2: app.showHotkeySettings(root.emuId); break
        case 3: root.beginInstall(); break
        case 4: resetDialog.open(); break
        case 5: uninstallDialog.open(); break
        }
```

Replace with:

```qml
        var idx = root.focusIndex - actionOffset
        if (root.emuId === "dolphin") {
            switch (idx) {
            case 0: app.showEmulatorSettings(root.emuId); break
            case 1: app.showControllerMapping(root.emuId, "GCPad1"); break
            case 2: app.showControllerMapping(root.emuId, "Wiimote1"); break
            case 3: app.showHotkeySettings(root.emuId); break
            case 4: root.beginInstall(); break
            case 5: resetDialog.open(); break
            case 6: uninstallDialog.open(); break
            }
        } else {
            switch (idx) {
            case 0: app.showEmulatorSettings(root.emuId); break
            case 1: app.showControllerMapping(root.emuId); break
            case 2: app.showHotkeySettings(root.emuId); break
            case 3: root.beginInstall(); break
            case 4: resetDialog.open(); break
            case 5: uninstallDialog.open(); break
            }
        }
```

- [ ] **Step 3: Replace the single `Controller Mapping` row with three Dolphin-aware rows**

Find (around lines 350–356):

```qml
                    DetailButton {
                        label: "Controller Mapping"
                        bgColor: SettingsTheme.card
                        textColor: SettingsTheme.text
                        isFocused: root.focusIndex === root.actionOffset + 1
                        onClicked: app.showControllerMapping(root.emuId)
                    }
```

Replace with:

```qml
                    DetailButton {
                        visible: root.emuId !== "dolphin"
                        label: "Controller Mapping"
                        bgColor: SettingsTheme.card
                        textColor: SettingsTheme.text
                        isFocused: root.focusIndex === root.actionOffset + 1
                        onClicked: app.showControllerMapping(root.emuId)
                    }

                    DetailButton {
                        visible: root.emuId === "dolphin"
                        label: "GameCube Controller"
                        bgColor: SettingsTheme.card
                        textColor: SettingsTheme.text
                        isFocused: root.focusIndex === root.actionOffset + 1
                        onClicked: app.showControllerMapping(root.emuId, "GCPad1")
                    }

                    DetailButton {
                        visible: root.emuId === "dolphin"
                        label: "Wii Remote"
                        bgColor: SettingsTheme.card
                        textColor: SettingsTheme.text
                        isFocused: root.focusIndex === root.actionOffset + 2
                        onClicked: app.showControllerMapping(root.emuId, "Wiimote1")
                    }
```

- [ ] **Step 4: Bump focus indices on subsequent rows for Dolphin**

The remaining buttons (Hotkeys, Reinstall, Reset, Uninstall) currently use indices `actionOffset + 2 / 3 / 4 / 5`. For Dolphin, those need to shift up by 1. The cleanest fix: set their `isFocused` expression to be Dolphin-aware.

For each of the four buttons (Hotkeys, Reinstall / Update, Reset Configuration, Uninstall) replace:

```qml
                        isFocused: root.focusIndex === root.actionOffset + N
```

with:

```qml
                        isFocused: root.focusIndex === root.actionOffset + (root.emuId === "dolphin" ? N + 1 : N)
```

(N = 2 for Hotkeys, 3 for Reinstall, 4 for Reset, 5 for Uninstall.)

- [ ] **Step 5: Build + test launch**

Run: `cd cpp && cmake --build build`
Expected: success.

Run: `open ./cpp/build/RetroNest.app`. Navigate to Dolphin's detail page. Confirm:
- "GameCube Controller" and "Wii Remote" appear instead of "Controller Mapping"
- Up/Down navigation cycles through 7 action rows (Settings, GameCube, Wii, Hotkeys, Reinstall, Reset, Uninstall)
- Clicking each opens the right page (controller pages launched with correct controller type id)

Other emulators (PCSX2, DuckStation, PPSSPP) should still show the single "Controller Mapping" row and 6 action rows.

Close the app.

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/AppUI/EmulatorDetailPage.qml
git commit -m "ui: surface separate GameCube + Wii Remote rows on Dolphin's detail page

Other emulators keep their single 'Controller Mapping' row. Dolphin's
detail page now exposes both controllers as direct entries instead of
hiding the choice behind a picker.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 6 — Smoke test + final verification

### Task 19: End-to-end smoke test

**Files:** none.

- [ ] **Step 1: Clean rebuild**

Run: `cd cpp && cmake --build build --clean-first && ctest --test-dir build --output-on-failure`
Expected: build success, all tests pass.

- [ ] **Step 2: Launch the app**

Run: `open ./cpp/build/RetroNest.app`

- [ ] **Step 3: Test the GameCube Controller mapping page**

Navigate: Dolphin detail page → GameCube Controller. Confirm:

- Title chrome reads `DOLPHIN — GAMECUBE CONTROLLER`
- 7-slot grid populated; SVG visible in the centre
- Slot titles read: D-PAD, FACE BUTTONS, LEFT ANALOG, RIGHT ANALOG, SHOULDERS, SHOULDERS, SYSTEM (the two SHOULDERS slots are intentional — left/right share that title)
- Focus a card → the focused button on the GameCube SVG lights up via spotlight
- Press A (Cross / Enter) → "Press a button" capture state activates
- Press a controller button → binding saves (card label updates), Device line in `~/.../emulators/dolphin/User/Config/GCPadNew.ini` updates
- Press B / Backspace → clears binding
- Press Y / M → auto-map → toast "Auto-mapped to {device name}"; all 23 bindings + Device line populated
- Press X → close

Open `~/.../emulators/dolphin/User/Config/GCPadNew.ini` in a text editor. Confirm `[GCPad1]` section has `Device = SDL/N/{name}` and the per-key `Buttons/X = \`Button N\``-style values.

- [ ] **Step 4: Test the Wii Remote mapping page**

Navigate: Dolphin detail page → Wii Remote. Confirm:

- Title reads `DOLPHIN — WII REMOTE`
- Slot titles read: D-PAD, FACE BUTTONS, **TILT**, **IR POINTING**, **SHAKE**, (empty right-shoulder slot), SYSTEM
- Wii.svg renders portrait
- Focus, rebind, clear, auto-map, close — same flow as GameCube
- Confirm `~/.../emulators/dolphin/User/Config/WiimoteNew.ini` `[Wiimote1]` section gets the new bindings
- Confirm `Source = 1` and `Options/Sideways Wiimote = True` are still present after auto-map (those keys are not in our schema, so they should pass through untouched)

- [ ] **Step 5: Test that PCSX2 / DuckStation / PPSSPP didn't regress**

Open each emulator's mapping page. Focus a card, rebind, clear, auto-map. Should behave identically to before this change.

- [ ] **Step 6: Close the app + final tests pass**

Run: `ctest --test-dir cpp/build --output-on-failure`
Expected: PASS.

- [ ] **Step 7: Commit only if any final tweaks were needed**

If smoke testing exposed any issues, commit a follow-up. Otherwise, no commit — Phase 6 is purely verification.
