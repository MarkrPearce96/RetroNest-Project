# Controller Mapping Redesign — PCSX2 (pilot)

**Date:** 2026-05-07
**Status:** Spec — pending implementation plan
**Pilot emulator:** PCSX2 (DualShock 2 only)
**Successors (out of scope here):** DuckStation, PPSSPP, Dolphin, libretro

---

## 1. Why

The controller mapping dialog (`controller_mapping_page.cpp` + 12 hand-built per-controller-type binding widgets) is the last UI surface that has not been pulled into the schema-driven, theme-aligned pattern used everywhere else. It currently:

- Hand-positions 28 buttons per controller type using pixel literals in 12 sibling files (~2,500 lines of duplicated layout code).
- Uses a separate purple/navy palette (`binding_widget_common.h`) that has nothing to do with the warm-grey + amber `SettingsDialogTheme` the redesigned settings dialogs adopted in 2026-04-11.
- Carries surfaces nobody asked for: a "Settings" sub-tab (deadzone / vibration scale tunings), a profile system (Shared / New / Apply / Rename / Delete), a Port 2 sidebar, and a controller-type combo that switches between DualShock 2 / Guitar / Jogcon / NeGcon / Pop'n.

The redesign collapses the dialog to a single schema-driven view that shows one controller, with one set of bindings, in the same visual language as the rest of the app — and replaces the hand-positioned button mosaic with a focus-driven *spotlight* on a real controller image (OpenEmu pattern). When a card is focused, the rest of the controller dims and a circular bright cutout lights up just the physical button being edited.

Because the new view consumes pure schema (`BindingDef` list + a controller image), bringing a second emulator into this UI later is "declare one `ControllerTypeDef`, annotate each `BindingDef` with spotlight coordinates" — no widget code at all.

## 2. Goals & non-goals

### Goals

- Replace the 12 per-controller-type binding widgets with one generic, schema-driven `ControllerBindingsView`.
- Match the existing `SettingsDialogTheme` palette (warm grey body, amber accent, `SettingsCard`-shaped boxes, `SettingsDescriptionBar` footer chrome).
- Add an OpenEmu-style spotlight: dimming overlay over the controller image with a bright cutout + amber pulse ring at the focused button.
- Be controller-friendly: every binding is a `SettingsCard` (whose existing `keyPressEvent` handles spatial d-pad navigation), and the bottom strip shows colored gamepad action hints (A / B / X / Y).
- Slim PCSX2 to a single controller type (DualShock 2). Drop the Settings sub-tab. Drop the profile system. Drop the Port 2 sidebar.
- Keep PCSX2 the pilot — but put all the new infrastructure under reusable `core/` + `ui/settings/` types so the next emulator gets the page for free.

### Non-goals (this spec)

- Rebinding hotkeys (`hotkey_settings_page.cpp`). It's a separate page and stays as-is.
- Migrating DuckStation / PPSSPP / Dolphin / libretro to the new view — handled by follow-up specs once PCSX2 is shipped.
- A live "press a real button to test it" preview area. The current rebind-on-click flow stays.
- Mouse-driven drag-to-reposition. Cards are placed by the schema, not the user.
- A real Port 2 controller. Single-player only for now.

## 3. Final visual design (locked v8)

The mockup pushed to the visual companion at `.superpowers/brainstorm/<session>/content/layout-v8.html` is the agreed reference. Summary:

- **Top chrome (56 px tall, `#4a4642`):** breadcrumb `PCSX2` in muted grey + amber title `DUALSHOCK 2`, uppercase, letter-spaced. No action buttons here — bottom strip owns those.
- **Body (`#585450`):** controller image centered. Six binding-card slots:
  - Left column top: `D-Pad` section header + 4 cards (Up / Down / Left / Right).
  - Left column bottom: `Left Analog` section header + 4 cards.
  - Right column top: `Face Buttons` section header + 4 cards (Triangle / Circle / Cross / Square).
  - Right column bottom: `Right Analog` section header + 4 cards.
  - Top center strip: `Shoulders` section header + 4 cards (L2 / L1 / R1 / R2).
  - Bottom center strip: `System` section header + 3 cards (Select / Analog / Start).
- **Each card:** matches `SettingsDialogTheme::cardQss()` (`#646058` bg, `#706c66` 1 px border, 8 px radius). Two-line content: small uppercase label in `textSecondary` over a 13 px `textPrimary` value. Empty bindings show "Not bound" in muted italic. Focused card: 1 px amber border + 2 px @ 30 % alpha amber halo (matching existing `SettingsCard::paintEvent`).
- **Spotlight:** when any card has focus, a `#000` @ 62 % alpha overlay is painted over the controller image, with a soft circular hole punched at the focused binding's `(spotlightX, spotlightY)` of radius `spotlightR` × ~1.5 (image coords). On top, an amber pulse ring (`#f59e0b`, 3 px stroke, ~8 px outer drop-shadow glow, 1.6 s ease-in-out radius/opacity pulse) sits at the cutout boundary. When no card has focus, no overlay is drawn — controller shown bright.
- **Footer (130 px tall, `#4a4642` with 3 px amber left accent — same rules as `SettingsDescriptionBar`):**
  - Left block: small uppercase `NOW EDITING` label, then `<Label> → <Value>` in 30 px / weight 300 / amber arrow + amber value.
  - Right block: four colored gamepad face hints (28 px circles): green A `Rebind`, red B `Clear`, yellow Y `Auto-Map`, blue X `Close`. 13 px label after each.
- **Empty focus state:** if no card has focus, the footer left block reads `READY` instead of `NOW EDITING` and no spotlight is drawn. (Cards still animate focus on first arrival.)

The dialog's outer frame is the existing `EmulatorSettingsDialogBase` — title bar, ESC-to-close, sizing — so framing matches the redesigned settings dialogs.

## 4. Schema changes

Two existing structs grow optional fields. No callers need to be touched until the new fields are populated.

### 4.1 `BindingDef` (`core/binding_def.h`)

```cpp
struct BindingDef {
    enum Kind { Button, Axis };

    Kind kind;
    QString label;
    QString group;
    QString section;
    QString key;
    QString defaultValue;

    // NEW — card placement on the page.
    // The view exposes six fixed slots: "DPad", "FaceButtons",
    // "LeftAnalog", "RightAnalog", "Shoulders", "System".
    // Cards inside the same slot stack vertically in the order they
    // appear in the BindingDef list.
    // Empty: fall back to matching `group` against the slot names
    // case-insensitively + ignoring spaces ("Left Analog" → "LeftAnalog").
    QString cardSlot = {};

    // NEW — spotlight target on the controller image, in the SVG's
    // intrinsic viewBox coordinate system. {0, 0, 0} means "no spotlight"
    // and the dimming overlay is suppressed when this binding is focused
    // (e.g. abstract bindings like "Pressure Modifier" with no physical
    // button, or motor-only entries).
    int spotlightX = 0;
    int spotlightY = 0;
    int spotlightR = 0;
};
```

These three fields are pure metadata for the new view. Adapters that haven't migrated yet ignore them; the existing code path doesn't read them.

### 4.2 `ControllerTypeDef` (`core/controller_type_def.h`)

No struct change. The existing `svgResource` path is what the new view loads. PCSX2's adapter shrinks to a single entry:

```cpp
QVector<ControllerTypeDef> PCSX2Adapter::controllerTypes() const {
    return { {"DualShock2", "DualShock 2",
              ":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg"} };
}
```

(Note: `NotConnected` is dropped too — there's nothing for the user to "switch to" anymore.)

### 4.3 PCSX2 `controllerBindingDefsForType` rewrite

The DualShock 2 list (currently in the type-agnostic `controllerBindingDefs()` and re-exported by `controllerBindingDefsForType("DualShock2")`) is rewritten to populate the three new fields. Indicative shape:

```cpp
{ BindingDef::Button, "Up",       "D-Pad",        "Pad", "Up",      "SDL-0/DPadUp",
  /* cardSlot */ "DPad",         /* spotlight */ 113, 195,  18 },
{ BindingDef::Button, "Down",     "D-Pad",        "Pad", "Down",    "SDL-0/DPadDown",
  "DPad",         113, 235,  18 },
// … etc, one entry per binding
{ BindingDef::Button, "L2",       "Shoulders",    "Pad", "L2",      "SDL-0/LeftTrigger",
  "Shoulders",    140,  50,  20 },
{ BindingDef::Button, "Pressure Modifier", "System", "Pad", "PressureMod", "",
  "System",         0,   0,   0 },   // no physical button → no spotlight
{ BindingDef::Axis,   "LargeMotor", "Motors",     "Pad", "LargeMotor", "",
  "System",         0,   0,   0 },   // motors are invisible — no spotlight
```

The exact `(x, y, r)` values are determined empirically against the live `DualShock_2.svg` viewBox (974 × 664.8) during implementation. The spec promises the *shape* of the data, not the numbers.

The Guitar / Jogcon / NeGcon / Pop'n branches in `controllerBindingDefsForType()` and their counterparts in `controllerSettingDefsForType()` are deleted entirely.

### 4.4 `controllerSettingDefs` / `controllerSettingDefsForType` for PCSX2

Both return `{}`. The 8 DualShock 2 settings (deadzone, axis-scale, button deadzone, vibration scale × 2, pressure modifier, axis source, invert L/R) are removed from the schema. Per-game tuning still works via the emulator's own defaults.

`AppController::controllerSettingsForPort` and friends survive (used today by the legacy widget) but become callable-with-empty-results for PCSX2. The new view never asks for them. Once every emulator drops their settings schemas the API can be retired in a follow-up; not in scope here.

## 5. Architecture & components

### 5.1 New types

- **`ControllerBindingsView : QWidget`** — `cpp/src/ui/settings/controller_bindings_view.{h,cpp}`. Owns the controller image + grid of binding cards + footer. Public API:
  - `ControllerBindingsView(SdlInputManager*, AppController*, QString emuId, int port, QWidget* parent)`.
  - Loads `controllerTypes()` (expects exactly one entry for now), reads `svgResource`, calls `controllerBindingDefsForPort(emuId, port)` to get the binding list, builds the page.
  - Re-emits `bindingFocused(BindingDef)` and `bindingActivated(BindingDef)` to its parent dialog (so the dialog can map A/B/X/Y face actions onto Auto-Map / Clear / etc.).
- **`ControllerImageArea : QWidget`** — private inner class inside `controller_bindings_view.cpp`. Renders the SVG + spotlight + amber pulse ring in `paintEvent`. Has one method: `setFocusedBinding(const BindingDef&)`. A `nullopt` value clears the overlay.
- **`ControllerBindingCard : SettingsCard`** — thin subclass that knows its `BindingDef`, displays Label + Value, and emits `clearRequested()` / `rebindRequested()` for the dialog's A/B handlers. Inherits all the spatial-d-pad keyPress logic from `SettingsCard`.

### 5.2 Files deleted

```
cpp/src/ui/settings/
  bindings_widget_base.{h,cpp}
  binding_widget_common.h
  ds2_bindings_widget.{h,cpp}
  guitar_bindings_widget.{h,cpp}
  jogcon_bindings_widget.{h,cpp}
  negcon_bindings_widget.{h,cpp}
  digital_bindings_widget.{h,cpp}
  analog_bindings_widget.{h,cpp}
  analog_joystick_bindings_widget.{h,cpp}
  ds_negcon_bindings_widget.{h,cpp}
  ds_negcon_rumble_bindings_widget.{h,cpp}
  ds_jogcon_bindings_widget.{h,cpp}
  popn_bindings_widget.{h,cpp}
  psp_bindings_widget.{h,cpp}
  controller_settings_widget.{h,cpp}
```

These are referenced exclusively from `controller_mapping_page.cpp::createBindingsWidget()` and its `Settings` tab handling — both go away.

### 5.3 Files shrunk

- **`controller_mapping_page.cpp`** — drops from ~513 lines to a thin shell:
  - Owns the dialog (title, sizing, ESC-to-close — same as today).
  - Builds top chrome (title + Auto-Map menu only).
  - Embeds one `ControllerBindingsView` for the emulator's single controller type.
  - Builds the footer description bar with the gamepad action hints.
  - Routes `Y` / Auto-Map menu entries to `AppController::autoMapControllerForPort(emuId, 1, deviceIdx)`.
  - Routes `B` / Clear All to `AppController::clearAllBindingsForPort(emuId, 1)`.
  - Routes `A` / Rebind to the focused card's `rebindRequested()`.
  - Routes `X` / Close to `accept()`.
  - No more port sidebar, no type combo, no Bindings/Settings tab strip, no profile bar, no `darkInputDialog` / `darkConfirmDialog`, no `createBindingsWidget()` switch.

- **`PCSX2Adapter::controllerTypes()`** — single entry only.
- **`PCSX2Adapter::controllerBindingDefsForType()`** — DS2 branch only, with the new `cardSlot` + `spotlight*` fields.
- **`PCSX2Adapter::controllerSettingDefsForType()`** — returns `{}` for all types.

### 5.4 Code removed from `AppController`

The profile-management methods are deleted:

- `controllerProfiles(emuId)`
- `createControllerProfile(emuId, name)`
- `applyControllerProfile(emuId, name)`
- `renameControllerProfile(emuId, oldName, newName)`
- `deleteControllerProfile(emuId, name)`

…together with any backing adapter / service / file IO under `{root}/config/controller_profiles/`. `controller_profiles/` directories that already exist on disk are left alone (no migration / cleanup) — the user can delete them by hand.

### 5.5 `AppController::controllerBindingsForPort` shape

Already returns a `QVariantList` of maps with `key`, `label`, `currentValue`. The new view consumes the `BindingDef` directly from `EmulatorAdapter::controllerBindingDefsForType()` (so it sees the new `cardSlot` + spotlight fields), and uses `controllerBindingsForPort()` only for `currentValue` lookup. No signature change — but we add a small helper that joins the two sources into a `(BindingDef, currentValue)` tuple list internal to the view.

## 6. Data flow

### 6.1 Initial load

```
ControllerMappingPage(emuId)
  └─ ControllerBindingsView(emuId, port=1)
       ├─ adapter->controllerTypes()       → [DualShock 2]
       ├─ adapter->controllerBindingDefsForType("DualShock2") → list of 28 BindingDef
       ├─ appController->controllerBindingsForPort(emuId, 1)  → list of {key, currentValue}
       └─ joins by `key`, builds 28 ControllerBindingCard, places by cardSlot,
          loads ControllerImageArea with svgResource
```

### 6.2 Focus → spotlight

```
user presses Down on a card
  └─ SettingsCard::keyPressEvent — finds spatially-nearest sibling card
        emit focused(SettingDef) — but we need BindingDef
```

→ `ControllerBindingCard` overrides this to emit a `bindingFocused(BindingDef)` signal carrying its `BindingDef`. The view connects it to:

```
view::onBindingFocused(BindingDef b)
  ├─ m_imageArea->setFocusedBinding(b)
  └─ m_footer->setNowEditing(b.label, currentValueFor(b))
```

`ControllerImageArea::setFocusedBinding` stores `b`, calls `update()`, and `paintEvent` draws (1) the SVG, (2) the dim overlay + radial-gradient cutout at `(b.spotlightX, b.spotlightY, b.spotlightR)` mapped to widget coords, (3) the amber pulse ring on top. If `b.spotlightR == 0`, only the SVG is drawn — no overlay, no ring.

### 6.3 Rebind

```
user presses A (or Enter, or clicks card)
  └─ ControllerBindingCard::activated() emit
       └─ view::rebindRequested(BindingDef)
            ├─ card->setText("Press a button…")  + capturing visual style
            └─ inputManager->startCapture()

inputManager->bindingCaptured(devIdx, element, isAxis, positive)  OR
inputManager->keyboardCaptured(keyString)
  └─ adapter->formatCapturedBinding(emuId, devIdx, element, isAxis, positive)  → "SDL-0/+LeftY"
       └─ appController->saveBindingForPort(emuId, 1, key, formatted)
            └─ view->reloadBindings()  → cards refresh their currentValue text
```

Same flow as today — only the UI shell changed.

### 6.4 Clear / Auto-Map

- `B` (`Clear`) on a focused card → `appController->clearBindingForPort(emuId, 1, focusedBinding.key)` then reload.
- `B` (`Clear All`) without a focused card / from the menu → `appController->clearAllBindingsForPort(emuId, 1)`.
- `Y` (`Auto-Map`) → reuses today's QMenu pop-up of "Keyboard" + connected SDL devices. Handlers are unchanged: `clearAllBindingsForPort` for keyboard, `autoMapControllerForPort` for SDL devices. The menu is restyled to match `SettingsDialogTheme`.

### 6.5 Resize

`ControllerImageArea::resizeEvent` recomputes the SVG-viewBox-to-widget-pixel transform (a single QTransform). The next paint draws the SVG and the spotlight at the correct scale. Cards are placed by a `QGridLayout` with fixed slot widths so column / row positions are stable across resizes.

## 7. Spotlight rendering details

Implemented inside `ControllerImageArea::paintEvent(QPaintEvent*)`:

```cpp
// 1. SVG.
m_renderer.render(&p, imageRectInWidgetCoords);

if (!m_focused.has_value() || m_focused->spotlightR == 0) return;

// 2. Map (x, y, r) from SVG viewBox → widget coords.
const QPointF c = m_viewBoxToWidget.map(QPointF(m_focused->spotlightX,
                                                  m_focused->spotlightY));
const qreal r = m_focused->spotlightR * m_viewBoxScale;

// 3. Dim overlay with radial-gradient cutout.
QRadialGradient grad(c, r * 1.5);
grad.setColorAt(0.0,  QColor(0, 0, 0, 0));      // transparent at the button
grad.setColorAt(0.55, QColor(0, 0, 0, 0));
grad.setColorAt(1.0,  QColor(0, 0, 0, 158));    // 62% alpha at the edge
p.fillRect(imageRectInWidgetCoords, grad);

// 4. Amber pulse ring on top.
const qreal phase = sin(m_pulseClock.elapsed() / 254.6) * 0.5 + 0.5;  // 1.6s ease-in-out
const qreal ringR = r * 1.4 + phase * 2.0;
const qreal ringAlpha = 0.85 + phase * 0.15;
QPen ring(QColor(245, 158, 11, int(ringAlpha * 255)), 3);
p.setPen(ring);
p.drawEllipse(c, ringR, ringR);
// (Drop-shadow glow is added by drawing the ring twice — once with a wide,
//  semi-transparent stroke for the glow, once with the sharp 3 px stroke.)
```

A 30 fps `QTimer` connected to `update()` drives the pulse phase. The timer is stopped whenever `setFocusedBinding({})` is called, so an unfocused page burns no CPU.

## 8. Controller-friendly navigation

Inherits `SettingsCard`'s existing spatial-d-pad logic verbatim — the slot-based grid produces exactly the geometry it expects: cards in the same column are spatially Up/Down from each other; cards across the body are Left/Right. The view doesn't need its own focus controller.

Mapping of gamepad buttons (consistent with the rest of the app's unified input — see `CLAUDE.md` "Input System"):

| Gamepad | Keyboard | Action |
|---|---|---|
| A / Cross | Enter | Rebind focused card |
| B / Circle | Esc / Backspace | Clear focused card (or Clear All if no focus) |
| Y / Triangle | M | Open Auto-Map menu |
| X / Square | (footer click) | Close dialog |

A dialog-level `QShortcut` for each handles keyboard fallback. The colored face-button glyphs in the footer make the mapping discoverable.

## 9. Out-of-scope / follow-ups

- **DuckStation / PPSSPP / Dolphin / libretro migration** — each is its own follow-up spec. They will:
  - Drop alternate controller types (NeGcon variants, Jogcon, Pop'n, AnalogJoystick, etc.) the same way PCSX2 does.
  - Annotate their primary controller's `BindingDef` list with `cardSlot` + spotlight coordinates.
  - Reuse `ControllerBindingsView` unchanged.
- **Hotkey settings page** — separate page, separate spec when its turn comes.
- **Profile system migration** — `controller_profiles/` files left on disk get cleaned up by the user manually. If a future spec needs profile-like behaviour (per-game overrides), it'll redesign that surface from scratch rather than reviving the old dropdown.
- **Live "press a button to test" preview** — can be a paint-time enhancement on `ControllerImageArea` later: subscribe to `SdlInputManager::buttonPressed` and briefly highlight whichever controller button matches the live input. Not in this spec.

## 10. Risk register

- **Spotlight coordinate drift.** The `(spotlightX, spotlightY, spotlightR)` triples are tied to the controller SVG's viewBox. If the SVG is replaced (different artwork), every entry needs new coordinates. **Mitigation:** keep the coordinates inline with the binding declaration in the adapter — same place the artwork is referenced from `ControllerTypeDef::svgResource`. A reviewer touching the SVG sees the coordinates next door.
- **Cards overflowing their slot.** With 4–8 cards per slot and a 130 px footer, a small dialog could push the grid past the available height. **Mitigation:** dialog sets a sensible minimum height (e.g. 720 px — same as the v8 mockup); no scroll area is introduced. Adapters with materially more bindings than DS2 are out of scope; if they appear later, the slot grid grows.
- **Profile data loss.** Removing the profile API means existing user profiles stop being reachable. **Mitigation:** files on disk are not touched; users can copy-paste binding lines from a profile INI back into the active config if they care. Documented in release notes for the milestone.
- **`SettingsCard` keyPress assumes a flat sibling list.** Today's spatial logic walks `parent->findChildren<SettingsCard*>(Qt::FindDirectChildrenOnly)`. With `ControllerBindingCard`s as children of slot containers (not directly of the body), the navigation jumps inside a slot only. **Mitigation:** keep cards as direct children of the body widget and place via `QGridLayout` with fixed slot positions (no intermediate slot containers).
- **PCSX2 binding format quirks.** PCSX2 uses `SDL-0/FaceSouth` (generic names, no `+` for buttons) — formatting handled by the existing `EmulatorAdapter::formatBinding` override. The new view doesn't see binding strings; it just hands the captured event back to `AppController::saveBindingForPort`. No regression risk here.

## 11. Acceptance criteria

The redesign is done when:

1. PCSX2 controller mapping page opens with the v8 layout: warm-grey body, amber title, six binding-card slots, controller image centered, amber-accent footer with `Now editing` + colored face hints.
2. Focusing any card shows the OpenEmu spotlight: the rest of the controller dims, a circular cutout brightens the focused button, an amber pulse ring sits on the cutout edge.
3. D-pad / arrow keys move card focus spatially across the page; the spotlight follows.
4. A / Enter rebinds; B / Esc clears; Y / M opens Auto-Map; X / footer click closes.
5. The 12 binding-widget files, `bindings_widget_base.*`, `binding_widget_common.h`, and `controller_settings_widget.*` are deleted from the tree.
6. PCSX2 adapter's `controllerTypes()` returns one entry; `controllerSettingDefs()` returns `{}`; non-DS2 branches in `controllerBindingDefsForType` / `controllerSettingDefsForType` are deleted.
7. `AppController` profile methods are deleted; `controller_mapping_page.cpp` no longer references them.
8. The build is green; the existing `formatBinding` / `formatCapturedBinding` paths still work (verified by binding a key and confirming PCSX2.ini gets the right `Pad1/<Key> = SDL-0/FaceSouth` line).

## 12. References

- v8 mockup: `.superpowers/brainstorm/<session>/content/layout-v8.html`
- Existing settings dialog palette: `cpp/src/ui/settings/settings_dialog_theme.h`
- Existing card focus + spatial nav: `cpp/src/ui/settings/widgets/settings_card.cpp`
- Existing description bar pattern: `cpp/src/ui/settings/widgets/settings_description_bar.{h,cpp}`
- Today's controller mapping (to be replaced): `cpp/src/ui/settings/controller_mapping_page.{h,cpp}` and the 12 `*_bindings_widget.*` siblings
- Inspiration for spotlight: OpenEmu controller mapping page (SNES screenshot supplied during design)
