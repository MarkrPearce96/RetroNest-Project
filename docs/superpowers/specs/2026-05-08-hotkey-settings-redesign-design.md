# Hotkey Settings Redesign — chrome and palette alignment

**Date:** 2026-05-08
**Status:** Spec — pending implementation plan
**Successor to:** `2026-04-04-hotkey-settings-design.md` (the spec that produced today's `HotkeySettingsPage`).

---

## 1. Why

The hotkey dialog (`hotkey_settings_page.{h,cpp}`, 534 lines) is the last per-emulator settings surface that has not been pulled into the warm-grey + amber `SettingsDialogTheme` and the shared `EmulatorSettingsDialogBase` chrome. It is already schema-driven at the data layer — every adapter feeds a single shared page via `hotkeyBindingDefs()` — but the page itself:

- Uses a hard-coded dark-navy palette (`#1e1e3a`, `#3a3a60`, hand-rolled scrollbar QSS) that has nothing to do with `SettingsDialogTheme`. This is the only Qt Widgets dialog still on the old palette.
- Hand-rolls its outer chrome: dialog framing, ESC handling, bottom bar, font / colour decisions. The settings dialogs and the controller mapping page get all of this from `EmulatorSettingsDialogBase` / `ControllerMappingPage`.
- Lays out categories as a left sidebar (`QListWidget`), the only such sidebar in the app. Settings uses a card-grid hub; controller mapping uses cards-in-slots on a single page.
- Has no focused-binding description bar — settings and controller mapping both have a `SettingsDescriptionBar` along the bottom that names the focused item.

This redesign rebuilds the dialog on the same scaffolding the other two surfaces use, so all three feel like one product. The data layer (`HotkeyDef`, `hotkeyBindingDefs()`, the capture flow inside `SdlInputManager`) does not change — adapters do not need to be touched.

## 2. Goals & non-goals

### Goals

- Replace the bespoke hotkey palette with `SettingsDialogTheme`. No more hard-coded `#1e1e3a` / `#3a3a60`.
- Replace the bespoke chrome with `EmulatorSettingsDialogBase`. Inherit history-stack navigation, controller-friendly key handling, and the `SettingsDescriptionBar` footer.
- Replace the left sidebar with a single scrollable page that groups bindings by category using `SettingsSectionHeader`, mirroring the controller-mapping layout.
- Render each binding as a row inside a `SettingsCard`, matching settings rows visually.
- Show focus-driven hotkey context in the bottom description bar (label → current binding) and four colour-coded face-button hints (A rebind / B clear / X close / Y add-alternate).
- Preserve every existing behaviour: click-to-rebind, Shift+click for additional bindings, right-click to clear, Restore Defaults, multi-binding display via `displayMultiBinding`.
- Delete `hotkey_settings_page.{h,cpp}` after the rewrite. No two-system overlap.

### Non-goals

- Touching `HotkeyDef` or `hotkeyBindingDefs()` in any adapter. The schema is already fine and adapters already feed it correctly.
- Introducing a "canonical hotkey catalog" (the cross-emulator deduplication idea raised in the brainstorm). That is a separate, smaller-payoff redesign and is not blocked by this spec.
- Changing the capture flow inside `SdlInputManager` (countdown, accumulation, `bindingCaptured` / `keyboardCaptured` signals). It is reused as-is.
- Adding sub-tabs, hub navigation, or any multi-page navigation inside the hotkey dialog. The chrome supports a stack but this dialog only ever pushes one page.
- Migrating the hotkey dialog onto `SettingDef` / `SettingsPageBuilder`. Hotkeys are a single widget type; routing them through the settings widget pipeline adds complexity without payoff.

## 3. Final visual design

```
┌─ Hotkey Settings ──────────────────────────────────┐
│  Hotkey Settings                                   │  ← title row, SettingsDialogTheme::titleBarBg()
├────────────────────────────────────────────────────┤
│                                                    │
│  SPEED CONTROL                                     │  ← SettingsSectionHeader (amber, uppercase)
│  ┌──────────────────────────────────────────────┐  │
│  │ Toggle Turbo               [ Period         ]│  │  ← row inside SettingsCard
│  │ Frame Advance              [ Not bound      ]│  │
│  │ Toggle Slow Motion         [ Shift+Backspace]│  │
│  └──────────────────────────────────────────────┘  │
│                                                    │
│  SYSTEM                                            │
│  ┌──────────────────────────────────────────────┐  │
│  │ Reset Virtual Machine      [ Not bound      ]│  │
│  │ Reload Patches             [ Not bound      ]│  │
│  └──────────────────────────────────────────────┘  │
│                                                    │
│  SAVE STATES                                       │
│  ┌──────────────────────────────────────────────┐  │
│  │ Select Previous Save Slot  [ Shift+F2       ]│  │
│  │ Select Next Save Slot      [ F2             ]│  │
│  │ ...                                           │  │
│  └──────────────────────────────────────────────┘  │
│                                                    │
├────────────────────────────────────────────────────┤
│ ┃  NOW EDITING                                     │  ← SettingsDescriptionBar
│ ┃  Toggle Turbo  →  Period                         │
│ ┃                                                  │
│ ┃  ●A Rebind   ●B Clear   ●Y Add   ●X Close        │  ← face-button hints
└────────────────────────────────────────────────────┘
```

Specifics:

- **Outer frame:** `EmulatorSettingsDialogBase` with `setupChrome("Hotkey Settings", QSize(900, 720), SettingsDialogTheme::windowBg())`. Inherits the title bar, ESC-to-close, controller-friendly key handling.
- **Body:** a single scrollable `QScrollArea` containing one `GenericHotkeyPage` widget. The dialog's `m_stack` stays effectively single-entry — we do not need a hub for hotkeys.
- **Section headers:** `SettingsSectionHeader` between groups (amber, uppercase, 12 px, weight 600). Group order is preserved from the adapter's `hotkeyBindingDefs()` declaration order — the adapter is the source of truth for ordering.
- **Binding row:** label on the left (220 px fixed width, `textPrimary`, 13 px), binding button on the right (`SettingsDialogTheme::cardQss()`-styled, identical metrics to `SettingsToggleRow`). Empty rows show "Not bound" in `textMuted` italic.
- **Focus halo:** binding rows inherit the focus halo from the existing `SettingsCard` paint event — 1 px amber border + 2 px @ 30 % alpha amber halo. The same focus model used by settings rows.
- **Description bar:** existing `SettingsDescriptionBar` along the bottom. When a binding row has focus, shows `NOW EDITING` over `<Label>  →  <CurrentBinding>` (amber arrow + amber value, mirroring the controller-mapping footer). When no row has focus, shows `READY`.
- **Face-button hints:** four colour-coded circles in the bottom-right of the description bar — green A `Rebind`, red B `Clear`, yellow Y `Add`, blue X `Close`. Same component used by the controller-mapping footer; reused, not re-implemented.
- **Restore Defaults:** moves from a dedicated bottom bar into the bottom-left of the description-bar row, opposite the face-button hints. Single text button styled with the same `#4a4642 / #706c66` chrome the hub uses for "Open Native Settings". This drops the dedicated bottom bar entirely — the description bar is the only footer chrome.

## 4. Schema changes

**None.** `HotkeyDef` (`core/binding_def.h:47`) already carries everything the new page needs: `label`, `group`, `section`, `key`, `defaultValue`. The new page reads these the same way the current page does — through `AppController::hotkeyBindings(emuId)`.

The `group` field stays named `group` (rather than being renamed to `category` for word-level consistency with `SettingDef::category`). Adapter source files would otherwise need a churn pass touching ~80 entries for zero behavioural benefit.

## 5. Code structure

### 5.1 New files

- `cpp/src/ui/settings/hotkey_settings_dialog.{h,cpp}` — extends `EmulatorSettingsDialogBase`. Constructor pushes a single `GenericHotkeyPage` onto the stack. Wires `bindingFocused` → `setFocusedSetting`-equivalent slot that drives the description bar with hotkey content rather than `SettingDef` content. Wires the face-button hints (A/B/X/Y) to capture / clear / close / add-alternate.
- `cpp/src/ui/settings/generic_hotkey_page.{h,cpp}` — the schema-driven content page. Constructor takes `(SdlInputManager*, AppController*, QString emuId, QWidget* parent)`. Internally:
  - Reads `m_appController->hotkeyBindings(emuId)` once at construction.
  - Iterates entries in declaration order, emitting a `SettingsSectionHeader` whenever the group changes.
  - Builds a `SettingsCard` per group containing one `HotkeyBindingRow` per entry.
  - Owns the capture state machine (mode, countdown, captured-bindings list) — moved verbatim from today's `HotkeySettingsPage`.
  - Emits `bindingFocused(HotkeyDef)` and `bindingCleared(HotkeyDef)` so the dialog can drive the description bar.
- `cpp/src/ui/settings/widgets/hotkey_binding_row.{h,cpp}` — small custom row widget: label (left) + binding button (right). The button is a `BindBtn` (existing class from `binding_widget_common.h`) restyled to match `SettingsDialogTheme`. The widget is focusable; on focus it emits `focused()` so the parent page can forward to the dialog.

### 5.2 Files deleted

- `cpp/src/ui/settings/hotkey_settings_page.{h,cpp}`. Every behaviour it implements is reproduced in the new files.

### 5.3 Files modified

- `cpp/src/ui/app_controller.cpp:464` — replace `auto* dialog = new HotkeySettingsPage(...)` with `auto* dialog = new HotkeySettingsDialog(...)`. One-line change.
- `cpp/src/ui/settings/CMakeLists.txt` (or whichever `.txt` lists settings sources) — drop the old page, add the three new files.

### 5.4 Files unchanged

- `cpp/src/core/binding_def.h` — `HotkeyDef` is unchanged.
- Every adapter (`pcsx2_adapter`, `duckstation_adapter`, `ppsspp_adapter`, `dolphin_adapter`) — none of them need to be touched. `dolphin_adapter` continues to return `{}` from `hotkeyBindingDefs()` and the dialog will simply have no rows when opened against Dolphin (or, equivalently, the entry point can be hidden when the list is empty — see §7).
- `cpp/src/core/sdl_input_manager.{h,cpp}` — capture flow stays as-is.
- `cpp/src/ui/settings/binding_widget_common.h` and `binding_display.h` — reused as-is.

## 6. Behaviour preservation checklist

The new dialog must preserve every behaviour of today's `HotkeySettingsPage`:

- [x] Click a binding row → start 5-second capture (`startCapture(key, false)`).
- [x] Shift+click a binding row → start capture in append mode (`startCapture(key, true)`).
- [x] Right-click a binding row → clear that binding (`AppController::clearHotkey`).
- [x] Capture accepts SDL controller events and keyboard events; modifier keys (Shift/Ctrl/Alt/Meta) accumulate, do not commit.
- [x] On capture timeout or commit, save via the existing `AppController` flow.
- [x] Multi-binding display: `"SDL-0/Back & SDL-0/RightShoulder"` → `"SDL-0 Create + SDL-0 R1"` via `displayMultiBinding` (reused unchanged).
- [x] Tooltip on each binding button: current display + the three-line "Left click / Shift / Right click" guide.
- [x] Restore Defaults restores every binding to `HotkeyDef::defaultValue`.
- [x] Closing the dialog while a capture is in progress cancels the capture cleanly.

## 7. Edge cases

- **Adapter returns an empty `hotkeyBindingDefs()` (Dolphin today).** Primary fix: the hotkey entry point in the in-game menu / settings is hidden when `hotkeyBindingDefs().isEmpty()` for the active emulator, so the dialog is never opened in this state. Defence in depth: if it is opened anyway (e.g. via a stale code path), `GenericHotkeyPage` renders a single centred "This emulator does not expose hotkey bindings." label instead of an empty section list.
- **A `HotkeyDef::group` is empty.** Default to `"General"` (matches today's behaviour at `hotkey_settings_page.cpp:88`).
- **Capture is in progress when the dialog closes (ESC, X, etc.).** Cancel the capture (`stopCapture(false)`) before the dialog accepts. Already guarded today; keep the guard.
- **Adapter changes binding-set ordering between sessions.** Section headers are derived from the data on each open, so reordering is handled automatically.

## 8. Out of scope (and why)

- **Hub-and-stack navigation for hotkeys.** Considered; rejected because hotkey data shape (~13 to 40 entries per emulator, 3-4 categories) fits comfortably on a single scrollable page, exactly like the controller-mapping page. Adding a hub would add a click without categorising any further.
- **Canonical hotkey catalog.** Mentioned in the brainstorm as a separate idea — collapse `Save State Slot 1-10` and the other near-universal hotkeys into a shared catalog with stable IDs so each adapter only provides a mapping table. Worth doing, but independent of this redesign and lower payoff. Leave for a follow-up spec.
- **Per-game hotkey overrides.** Hotkeys remain emulator-wide. No per-game scoping is introduced or implied.
- **Re-emitting hotkey events as Qt key injections (parallel to controller bindings).** Hotkeys are consumed by the running emulator process, not by our own QML — no injection is needed.

## 9. Migration sequence

1. Add the three new files under `cpp/src/ui/settings/`. Wire them into the build.
2. Switch `app_controller.cpp:464` to instantiate `HotkeySettingsDialog`.
3. Manually verify: open hotkey settings against PCSX2, DuckStation, PPSSPP. Confirm rebind / clear / restore-defaults / multi-binding display / face-button hints all work.
4. Delete `hotkey_settings_page.{h,cpp}` and remove its build entry.
5. Update `MEMORY.md` with a `hotkey-settings-redesign.md` entry summarising the new architecture (parallel to `controller-mapping-redesign.md`).

No staged rollout is needed — the data layer is unchanged, so the cutover is one source-file swap.

## 10. Open questions

None at spec time. All shape decisions are locked above; the layout option was confirmed against a side-by-side preview during brainstorming.
