# Login Page Keyboard & Controller Navigation

**Date:** 2026-04-02
**Status:** Draft

## Overview

Add keyboard and gamepad controller navigation to the ScraperSettings login page, plus a reusable virtual keyboard overlay for controller-only text input. The login page is currently mouse-only; the dashboard page already has full focus navigation that serves as the pattern to follow.

## Goals

- Navigate the login form (username, password, sign in) with arrow keys and gamepad D-pad
- Type into text fields with a physical keyboard or an on-screen virtual keyboard (for controller users)
- Build the virtual keyboard as a reusable QML component for any text field in the app
- Match the existing visual focus style (golden border/glow via FocusableItem and SettingsTheme)

## Design Decisions

- **Approach:** Pure QML (Approach 1) — no C++ changes needed, follows the dashboard's existing focusIndex + inputManager pattern
- **Virtual keyboard style:** Popup QWERTY overlay — modal over the settings panel, gives the keyboard maximum space, keeps the form clean underneath
- **Input detection:** Auto-detect last input source (controller vs keyboard). Controller input opens the virtual keyboard; keyboard input gives the field direct focus. The virtual keyboard never blocks physical keyboard input — keystrokes pass through while the overlay is open.
- **Password masking:** Password field preview shows dots by default with a show/hide toggle accessible via controller

## Section 1: Login Form Focus Navigation

The login page (StackLayout state 0 in ScraperSettings.qml) gets a focusIndex system matching the dashboard:

- **3 focusable items:** Username field (0), Password field (1), Sign In button (2)
- **Up/Down** arrow keys and D-pad move between items, wrapping at edges
- **A-button / Enter** activates the focused item:
  - Text field: auto-detects input source. Controller → opens virtual keyboard. Physical keyboard → field gets forceActiveFocus() for direct typing.
  - Sign In button: triggers the sign-in action
- **B-button / Escape** from the login form: goes back to category list (existing SettingsOverlay behavior)
- Visual focus uses existing FocusableItem golden border/glow styling
- The back arrow at the top is not in the focus list — reachable via B-button

## Section 2: VirtualKeyboard.qml — Reusable Component

A new `VirtualKeyboard.qml` file in `cpp/qml/AppUI/`.

### API

```
property string text         — bound to the target field's text (two-way)
property bool isPassword     — controls masking in the preview
property bool showPassword   — toggles password reveal (default false)
signal accepted()            — emitted when user presses Done
signal cancelled()           — emitted when user presses B-button
function open(initialText, isPassword) — shows the overlay
```

### Layout

- Semi-transparent black backdrop covering the settings panel
- Centered content area:
  - Label at top (e.g. "USERNAME" or "PASSWORD")
  - Text preview with blinking cursor, masked if password (eye toggle on the right, controller-reachable)
  - Full QWERTY grid
  - Bottom row: `123` (switch to numbers/symbols), `@`, `space`, `.`, `Done`

### Keyboard Grid Navigation

- `focusRow` + `focusCol` track the currently highlighted key
- D-pad Up/Down/Left/Right moves between keys
- A-button types the highlighted character
- B-button dismisses the keyboard (calls `cancelled()`, resets `text` back to the `initialText` passed to `open()`)
- Shift toggles uppercase (one-shot: reverts after typing one character)
- `123` button swaps to numbers/symbols layout; button label changes to `abc` to swap back

### Physical Keyboard Passthrough

While the overlay is open, physical key presses insert directly into the text:

- Character keys → insert into text at cursor
- Backspace → delete character
- Enter → acts as Done (emits `accepted()`)
- Escape → acts as cancel (emits `cancelled()`)

The overlay never blocks physical keyboard users.

### Input Source Detection

- `property bool lastInputWasController` on the page
- Updated by inputManager Connections (set true) and Keys.onPressed (set false)
- When activating a text field: if true → open VirtualKeyboard; if false → forceActiveFocus() on the TextField

## Section 3: Integration Points

### ScraperSettings.qml Changes

- Add `focusIndex` property and `Keys.onPressed` handler to the login state (state 0)
- Add `lastInputWasController` property, wired to inputManager signals and key events
- Add a single `VirtualKeyboard` instance (shared between both text fields)
- On A-button/Enter on a text field: check `lastInputWasController`, open virtual keyboard or focus the field
- On `accepted()`: write text back to field, close overlay, return focus to focusIndex
- On `cancelled()`: discard changes, close overlay, return focus

### Dashboard Edit Fields

The same VirtualKeyboard instance serves the account editing username/password fields with identical auto-detect logic.

### No C++ Changes

Everything hooks into existing `inputManager` signals and `SettingsTheme` styling.

## Files Touched

| File | Change |
|------|--------|
| `cpp/qml/AppUI/VirtualKeyboard.qml` | New — reusable keyboard overlay component |
| `cpp/qml/AppUI/ScraperSettings.qml` | Add login focus nav, input detection, virtual keyboard integration |
| `cpp/qml/AppUI/CMakeLists.txt` | Register new QML file if needed |
