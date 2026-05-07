# Dolphin — Controller Mapping Migration Design

**Date:** 2026-05-08
**Status:** Proposed
**Predecessors:** PCSX2 / DuckStation / PPSSPP migrations (commits `01ed2f7`, `0c9f1af`, `b707506`)
**Related:** `docs/superpowers/specs/controller-mapping-migration-prompt.md` (reusable recipe)

## Problem

Dolphin is the only installed emulator whose `controllerTypes()` returns empty — controller remapping is currently only available through Dolphin's native UI (a UX violation of the "one window" rule). The reusable migration recipe assumes one controller type per emulator and a single bindings file with PCSX2-style `SDL-0/A` values; Dolphin diverges on **all three** axes:

1. **Two consoles in one emulator** — GameCube and Wii each have their own controller, their own INI file (`GCPadNew.ini` / `WiimoteNew.ini`), and their own section (`[GCPad1]` / `[Wiimote1]`).
2. **Different binding format** — Dolphin uses an expression-syntax string (e.g. `` `Button S` ``, `` `Pad N` ``, `` `Left Y-` ``) plus a per-section `Device = SDL/0/Wireless Controller` header line — not the inline `SDL-0/A` form that PCSX2/DuckStation/PPSSPP use.
3. **Wiimote is not a stick-and-shoulder controller** — its expressive bindings are Tilt / IR / Shake (motion mapped to gamepad sticks), which fit awkwardly under the existing slot titles.

This spec migrates Dolphin to `ControllerBindingsView` while preserving its idiosyncrasies, and makes the minimum extensions to the shared infra needed to support a multi-controller emulator.

## Goals

- Two new actions on the Dolphin detail page: **GameCube Controller** and **Wii Remote** — each opens the schema-driven `ControllerBindingsView` for that controller.
- Same UX vocabulary as PCSX2/DuckStation/PPSSPP: A rebind / B clear / Y auto-map / X close, spotlight on focused button, status banner.
- Bindings persist into the same INI files Dolphin already reads (`GCPadNew.ini` / `WiimoteNew.ini`) — no shadow file, no merge logic. Edits made through the new UI are visible to Dolphin's native UI and vice versa.
- 23 GameCube bindings + 23 Wii Remote bindings — exactly mirroring the default profiles already baked at install time.
- Slot titles read honestly on the Wii Remote page ("TILT" / "IR POINTING" / "SHAKE" instead of "LEFT ANALOG" / "RIGHT ANALOG" / "SHOULDERS").

## Non-Goals (deferred)

- **Wiimote extensions** (Nunchuk, Classic Controller, GH guitar) — remap via Dolphin's native UI.
- **Hotkeys + advanced motion** (Swing, Accelerometer, Gyro, D-Pad mode toggles) — native UI only.
- **Keyboard binding capture** — defaults are SDL gamepad-only; keyboard support deferred (no precedent in PCSX2/DuckStation/PPSSPP either, this is a project-wide gap).
- **Port 2** — view hard-codes `port=1` (multi-pad is its own effort, called out in the migration prompt).
- **Per-controller settings** (deadzone / sensitivity / vibration scale) — empty, mirroring the other migrated emulators.

---

## Architecture

### User flow

```
EmulatorDetailPage (Dolphin)
  └── "GameCube Controller"  → showControllerMapping("dolphin", "GCPad1")
  └── "Wii Remote"           → showControllerMapping("dolphin", "Wiimote1")
                                       │
                                       ▼
                       ControllerMappingPage(emuId, controllerTypeId)
                                       │
                                       ▼
                ControllerBindingsView(emuId, controllerTypeId, port=1)
                  • title = type.displayName
                  • slot titles = type.slotTitleOverrides ∪ defaults
                  • bindings = adapter->controllerBindingDefsForType(type)
                  • SVG = type.svgResource
                                       │
                                       ▼
              ConfigService routes save/clear/autoMap to the right
              file/section based on controllerTypeId, via the new
              type-aware overloads on EmulatorAdapter.
```

### Shared-infra changes (small)

Three additions, all backward-compatible:

**1. `ControllerTypeDef` gains an optional `slotTitleOverrides` map.**

```cpp
struct ControllerTypeDef {
    QString id;
    QString displayName;
    QString svgResource;
    QHash<QString, QString> slotTitleOverrides; // e.g. {"LeftAnalog": "TILT"}
};
```

`ControllerBindingsView::titleForSlot()` consults this map first, falling back to today's hard-coded titles. Default-constructed for every other emulator → zero behavioural change for PCSX2/DuckStation/PPSSPP.

**2. `EmulatorAdapter` gains type-aware overloads for the bindings-storage methods.**

```cpp
virtual QString controllerBindingsConfigFilePath(const QString& type) const {
    Q_UNUSED(type);
    return controllerBindingsConfigFilePath();   // delegate to existing no-arg
}
virtual QString controllerBindingsSection(int port, const QString& type) const {
    Q_UNUSED(type);
    return controllerBindingsSection(port);
}
```

`ConfigService::{save,clear,clearAll,autoMap}BindingForPort` already calls `controllerType(emuId, port)` to resolve the type; switch each call site to the new type-aware overloads. PCSX2/DuckStation/PPSSPP inherit the delegating defaults — no change. Dolphin overrides both to return GCPad-vs-Wiimote paths/sections.

**3. `EmulatorAdapter` gains an optional device-header hook.**

```cpp
virtual void writeBindingDeviceHeader(IniFile& ini, const QString& section,
                                      int deviceIndex,
                                      SdlInputManager* input) const {
    Q_UNUSED(ini); Q_UNUSED(section); Q_UNUSED(deviceIndex); Q_UNUSED(input);
    // default: no-op — most emulators encode device in each binding string
}
```

`ConfigService::saveBindingForPort` and `autoMapControllerForPort` call this after writing per-binding values, passing the captured/auto-map device index. Dolphin overrides it to write `Device = SDL/{idx}/{deviceName}` into the active section. Other adapters inherit the no-op.

**4. `ConfigService` binding methods thread an explicit `controllerType`.**

The active controller type comes from the open dialog (the user clicked "GameCube Controller" or "Wii Remote"), not from a stored preference — so the save/clear/auto-map calls must pass it explicitly. The existing `controllerType(emuId, port)` lookup is the wrong source: it returns whatever was last stored, not what the user is currently editing.

```cpp
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

`ControllerBindingsView` / `ControllerMappingPage` already know the type (it's a ctor arg) and pass it through. `deviceIndex = -1` on save means "don't update device header" (keyboard / cancel paths). Internally each method:

1. Resolves `path = adapter->controllerBindingsConfigFilePath(controllerTypeId)`.
2. Resolves `section = adapter->controllerBindingsSection(port, controllerTypeId)`.
3. Loads, mutates, and saves the INI.
4. After mutation: if `deviceIndex >= 0`, calls `adapter->writeBindingDeviceHeader(ini, section, deviceIndex, input)` so adapters that rely on a section-wide Device line stay in sync.

`AppController` mirrors the new signatures (it's a thin pass-through to `ConfigService`).

**5. `ControllerBindingsView`'s single-type assertion is softened.**

```cpp
// Before:
Q_ASSERT_X(types.size() == 1, ...);
const auto& type = types.first();

// After:
const auto match = std::find_if(types.begin(), types.end(),
    [&](const auto& t){ return t.id == m_typeId; });
Q_ASSERT_X(match != types.end(), "ControllerBindingsView",
           "requested controller type not found in adapter list");
const auto& type = *match;
```

`ControllerMappingPage` and `ControllerBindingsView` ctors gain a `controllerTypeId` parameter. Existing call sites (PCSX2/DuckStation/PPSSPP) pass `adapter->controllerTypes().first().id` — still works because they declare exactly one type.

### Dolphin adapter changes

**`controllerTypes()`** returns two entries:

```cpp
{"GCPad1",   "GameCube Controller",
 ":/AppUI/qml/AppUI/images/controllers/GameCube.svg",
 /* slotTitleOverrides: */ {}},
{"Wiimote1", "Wii Remote",
 ":/AppUI/qml/AppUI/images/controllers/Wii.svg",
 /* slotTitleOverrides: */
 {{"LeftAnalog",    "TILT"},
  {"RightAnalog",   "IR POINTING"},
  {"LeftShoulders", "SHAKE"}}},
```

The type ID (`GCPad1` / `Wiimote1`) doubles as the INI section name — convenient.

**`controllerBindingDefsForType(type)`** returns the 23 bindings for the requested type. See the [Slot Layout](#slot-layout) section below for the full list.

**`controllerBindingsConfigFilePath(type)`** routes:

```cpp
if (type == "Wiimote1") return wiimoteIniPath();
return gcpadIniPath();    // "GCPad1" or default
```

**`controllerBindingsSection(port, type)`** returns `type` (since the type IDs are the section names; `port` is always 1 in v1).

**`formatBinding(devIdx, element, isAxis, positive)`** translates SDL element names to Dolphin's expression syntax (bare element name in backticks):

| SDL element                | Dolphin output     |
|----------------------------|--------------------|
| FaceSouth                  | `` `Button S` ``   |
| FaceEast                   | `` `Button E` ``   |
| FaceWest                   | `` `Button W` ``   |
| FaceNorth                  | `` `Button N` ``   |
| DPadUp                     | `` `Pad N` ``      |
| DPadDown                   | `` `Pad S` ``      |
| DPadLeft                   | `` `Pad W` ``      |
| DPadRight                  | `` `Pad E` ``      |
| LeftShoulder               | `` `Shoulder L` `` |
| RightShoulder              | `` `Shoulder R` `` |
| LeftTrigger (axis)         | `` `Trigger L` ``  |
| RightTrigger (axis)        | `` `Trigger R` ``  |
| LeftStick (button)         | `` `Thumb L` ``    |
| RightStick (button)        | `` `Thumb R` ``    |
| Back                       | `` `Back` ``       |
| Start                      | `` `Start` ``      |
| Guide                      | `` `Guide` ``      |
| LeftX (axis, positive)     | `` `Left X+` ``    |
| LeftX (axis, negative)     | `` `Left X-` ``    |
| LeftY (axis, ±)            | `` `Left Y±` ``    |
| RightX / RightY (axis, ±)  | `` `Right X±` `` / `` `Right Y±` `` |

Device index does **not** appear in the formatted value — it's communicated separately via the Device header. An unknown SDL element returns an empty string (won't bind) and emits a `qWarning` with the element name.

**`writeBindingDeviceHeader(...)`** writes `Device = SDL/{idx}/{deviceName}` into the section when called with a real `deviceIndex >= 0`. Device name is looked up via `SdlInputManager::deviceNameForIndex(deviceIndex)` (already exists for the auto-map flow's controller-name notification).

**`controllerSettingDefs()` / `controllerSettingDefsForType(type)`** both return empty — matches PCSX2/DuckStation/PPSSPP precedent.

**`writeGcPadDefaultsIfMissing()` / `writeWiimoteDefaultsIfMissing()`** are unchanged. The "create-only, never overwrite" rule still holds for first-install seeding; the new UI writes through to the live file from then on.

### UI plumbing

**`AppController::showControllerMapping(emuId, controllerTypeId)`** — new signature. The single-arg form (used by every other emulator) becomes a thin wrapper that picks the first/only type:

```cpp
Q_INVOKABLE void showControllerMapping(const QString& emuId);
Q_INVOKABLE void showControllerMapping(const QString& emuId,
                                       const QString& controllerTypeId);
```

**`EmulatorDetailPage.qml`** — for `emuId === "dolphin"`, replace the single `Controller Mapping` row with two rows:

```qml
DetailButton {
    visible: root.emuId !== "dolphin"
    label: "Controller Mapping"
    onClicked: app.showControllerMapping(root.emuId)
}
DetailButton {
    visible: root.emuId === "dolphin"
    label: "GameCube Controller"
    onClicked: app.showControllerMapping(root.emuId, "GCPad1")
}
DetailButton {
    visible: root.emuId === "dolphin"
    label: "Wii Remote"
    onClicked: app.showControllerMapping(root.emuId, "Wiimote1")
}
```

`actionCount` (and the focus-index logic that depends on it) becomes a small computed expression: `emuInfo.installed ? (root.emuId === "dolphin" ? 7 : 6) : 1`. The detail page action handler's `case 1`/`case 2` switch grows a Dolphin-specific branch.

---

## Slot Layout

### GameCube — `[GCPad1]` in `GCPadNew.ini` (23 bindings)

| Slot              | Title           | Bindings (key → default value)                                                                                                |
|-------------------|-----------------|--------------------------------------------------------------------------------------------------------------------------------|
| `DPad`            | D-PAD           | `D-Pad/Up`→`Pad N`, `D-Pad/Down`→`Pad S`, `D-Pad/Left`→`Pad W`, `D-Pad/Right`→`Pad E`                                          |
| `FaceButtons`     | FACE BUTTONS    | `Buttons/A`→`Button S`, `Buttons/B`→`Button E`, `Buttons/X`→`Button W`, `Buttons/Y`→`Button N`                                 |
| `LeftAnalog`      | LEFT ANALOG     | `Main Stick/{Up,Down,Left,Right}` → `Left Y-`, `Left Y+`, `Left X-`, `Left X+`                                                 |
| `RightAnalog`     | RIGHT ANALOG    | `C-Stick/{Up,Down,Left,Right}` → `Right Y-`, `Right Y+`, `Right X-`, `Right X+`                                                |
| `LeftShoulders`   | SHOULDERS       | `Triggers/L`→`Trigger L`, `Triggers/L-Analog`→`Trigger L`                                                                     |
| `RightShoulders`  | SHOULDERS       | `Triggers/R`→`Trigger R`, `Triggers/R-Analog`→`Trigger R`, `Buttons/Z`→`Shoulder R`                                            |
| `System`          | SYSTEM          | `Buttons/Start`→`Start`, `Rumble/Motor`→`Motor` *(no spotlight)*                                                              |

Z lives in `RightShoulders` because it's a shoulder-cluster button on the physical pad and pairs visually with R. Both `Triggers/L` (digital threshold) and `Triggers/L-Analog` (analog axis) are exposed — they bind to the same physical input by default; the upstream Dolphin UI exposes both as well.

### Wii Remote — `[Wiimote1]` in `WiimoteNew.ini` (23 bindings)

| Slot              | Title (override)  | Bindings                                                                                                                       |
|-------------------|-------------------|--------------------------------------------------------------------------------------------------------------------------------|
| `DPad`            | D-PAD             | `D-Pad/{Up,Down,Left,Right}` → `Pad N`, `Pad S`, `Pad W`, `Pad E`                                                              |
| `FaceButtons`     | FACE BUTTONS      | `Buttons/A`→`Button S`, `Buttons/B`→`Button E`, `Buttons/1`→`Button W`, `Buttons/2`→`Button N`                                 |
| `LeftAnalog`      | **TILT**          | `Tilt/{Forward,Backward,Left,Right}` → `Left Y-`, `Left Y+`, `Left X-`, `Left X+`                                              |
| `RightAnalog`     | **IR POINTING**   | `IR/{Up,Down,Left,Right}` → `Right Y-`, `Right Y+`, `Right X-`, `Right X+`                                                     |
| `LeftShoulders`   | **SHAKE**         | `Shake/X`→`Shoulder L`, `Shake/Y`→`Shoulder L`, `Shake/Z`→`Shoulder L`                                                         |
| `RightShoulders`  | *(empty)*         | —                                                                                                                              |
| `System`          | SYSTEM            | `Buttons/-`→`Back`, `Buttons/+`→`Start`, `Buttons/Home`→`Guide`, `Rumble/Motor`→`Motor` *(no spotlight)*                       |

`Source = 1` and `Options/Sideways Wiimote = True` are not exposed (they're configuration, not bindings) — they remain seeded by `writeWiimoteDefaultsIfMissing()`.

### Spotlight coordinates

Coordinates are derived during implementation by `grep`-ing the SVG's labelled elements (per the migration prompt's recipe), not pinned in this spec. Conventions:

- Buttons map to a single `(cx, cy, r)` triple in the SVG's intrinsic viewBox.
- Stick directions (Up/Down/Left/Right) sit on a circle around the stick centre with a smaller radius.
- Tilt and IR clusters spotlight the Wiimote's accelerometer / IR sensor regions.
- Abstract bindings (`Rumble/Motor`, all three `Shake/*`) use `(0,0,0)` → no spotlight rendered; the Shake card focuses spotlight at the Wiimote shaft / motion-sensor region as a softer "you're aiming at the whole thing" indicator. (To revisit if it looks off in the smoke test.)

---

## Save flow walk-through

User focuses the **A** card on the GameCube page and presses physical SDL button on device idx 2:

1. `SdlInputManager::bindingCaptured(devIdx=2, element="FaceSouth", isAxis=false, positive=false)` fires.
2. `ControllerMappingPage` lambda calls `app->formatCapturedBinding("dolphin", 2, "FaceSouth", false, false)` → returns `` `Button S` ``.
3. Same lambda calls `app->saveBindingForPort("dolphin", port=1, controllerTypeId="GCPad1", "Buttons/A", "`Button S`", deviceIndex=2)` — the type comes from the open dialog, not a stored preference.
4. `ConfigService::saveBindingForPort` resolves:
   - `path = adapter->controllerBindingsConfigFilePath("GCPad1")` → `…/GCPadNew.ini`
   - `section = adapter->controllerBindingsSection(1, "GCPad1")` → `"GCPad1"`
5. Writes `Buttons/A = `Button S`` into `[GCPad1]`.
6. Calls `adapter->writeBindingDeviceHeader(ini, "GCPad1", deviceIndex=2, input)` → Dolphin override writes `Device = SDL/2/{deviceName}` into `[GCPad1]`.
7. Saves the INI; `m_view->reloadBindings()` re-reads to refresh the card label.

Auto-map flow is the same shape: write all 23 default values into the section, then write the device header once. Clear flow writes empty values without touching Device.

---

## Tests

`cpp/tests/test_dolphin_controller_schema.cpp` — mirrors `test_pcsx2_controller_schema.cpp`. Tests cover **both** controller types where applicable:

- **`testTwoControllerTypes`** — exactly two entries with IDs `GCPad1` and `Wiimote1`; SVG paths end with `GameCube.svg` / `Wii.svg`.
- **`testNoControllerSettings`** — both setting-defs methods return empty for both types.
- **`testBindingsHaveCardSlot`** — every binding has a non-empty `cardSlot` from the valid set.
- **`testPhysicalButtonsHaveSpotlight`** — every binding in the "physical" set (D-Pad / Face / Triggers / shoulder buttons / Start / Minus / Plus / Home) has `spotlightR > 0`. Motors and all three Shake bindings are exempt.
- **`testGcPadBindingsRouteToGcPadFile`** — `controllerBindingsConfigFilePath("GCPad1")` ends with `GCPadNew.ini`; `controllerBindingsSection(1, "GCPad1") == "GCPad1"`.
- **`testWiimoteBindingsRouteToWiimoteFile`** — same for `Wiimote1` → `WiimoteNew.ini`.
- **`testFormatBindingTranslation`** — table-driven over the SDL→Dolphin element map.
- **`testSlotTitleOverridesPresentForWiimote`** — Wiimote's `slotTitleOverrides` includes `LeftAnalog → TILT`, `RightAnalog → IR POINTING`, `LeftShoulders → SHAKE`; GameCube's map is empty.

Wired into `cpp/CMakeLists.txt` immediately after the existing `test_dolphin_schema` block.

---

## Risks & mitigations

| Risk | Mitigation |
|------|-----------|
| Softening the `Q_ASSERT(types.size()==1)` regresses single-type emulators if a future bug returns N>1 silently. | The new assertion still fires when the `controllerTypeId` doesn't appear in the list. Existing callers explicitly pass `controllerTypes().first().id`, so any drift is caught locally. |
| Per-section `Device =` line gets out of sync if the user rebinds half their buttons from device A and half from device B. | v1 always rewrites Device to the most-recently-captured device. Auto-map writes all 23 from one device → consistent. Mixed-device mapping is an edge case Dolphin's own UI also struggles with; document and move on. |
| `Triggers/L` (digital) and `Triggers/L-Analog` (analog) both bound to the same physical input look like duplicates. | Card label disambiguates: "L (digital)" vs "L-Analog". Mirrors how upstream Dolphin's UI displays them. |
| Wii.svg viewBox is portrait (777×1614); cards on either side of the SVG pane may push the dialog tall. | The SVG pane already self-tunes via aspect-ratio — see commit `a1c84ce` ("size image area from SVG aspect, not hard-coded 1.46"). Verify in smoke test; if the page exceeds 1280×780 minimum, scale down the SVG max-height. |
| Wiimote default writes `Device = SDL/0/Wireless Controller` to a literal name; auto-map needs to overwrite this with the actual connected-device name. | `writeBindingDeviceHeader` always writes the looked-up name from `SdlInputManager`; no special-casing needed. |

---

## Out of scope (explicit, for the implementation plan)

- New SVG artwork — using the already-committed `GameCube.svg` and `Wii.svg`.
- Hotkey settings page (`hotkey_settings_page.cpp`) — separate dialog, not touched here.
- Refactoring PCSX2/DuckStation/PPSSPP adapters — they inherit the new defaults and need zero changes (verified by their existing tests still passing).
- Settings-pane changes — Dolphin's settings dialog is unchanged.
