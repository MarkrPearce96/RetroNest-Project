# Unified Controller Input — Design Spec

**Date:** 2026-04-02
**Status:** Draft

## Overview

Replace the dual input system (Qt keyboard events + custom `inputManager` signals) with a single unified approach: the controller injects Qt key events, so every QML page that handles keyboard navigation automatically handles controller navigation too. No per-page `Connections` wiring required.

## Goals

- Controller works everywhere keyboard works, with zero per-page setup
- Remove all navigation-related `Connections { target: inputManager }` blocks
- Remove the `settingsOpen` and `virtualKeyboardActive` flag plumbing
- Keep binding capture (`startCapture`, `bindingCaptured`) unchanged — it needs raw device info

## Design Decisions

- **Scope:** Navigation only. Binding capture stays as custom signals.
- **Injection method:** `QGuiApplication::sendEvent()` with `QKeyEvent` into the focused window.
- **Start button:** Mapped to `Key_F1` (not Escape, which means "back/cancel").

## Button Mapping

| Controller | Qt Key | Purpose |
|---|---|---|
| D-pad Up / Left Stick Up | `Qt::Key_Up` | Navigate up |
| D-pad Down / Left Stick Down | `Qt::Key_Down` | Navigate down |
| D-pad Left / Left Stick Left | `Qt::Key_Left` | Navigate left |
| D-pad Right / Left Stick Right | `Qt::Key_Right` | Navigate right |
| A (Xbox) / Cross (PS) | `Qt::Key_Return` | Select / Activate |
| B (Xbox) / Circle (PS) | `Qt::Key_Escape` | Back / Cancel |
| Start | `Qt::Key_F1` | Toggle settings overlay |
| X (Xbox) / Square (PS) | `Qt::Key_Backspace` | Delete |
| R2 / Right Trigger | `Qt::Key_Shift` | Shift modifier |

## Section 1: SdlInputManager Changes

Modify `SdlInputManager::pollEvents()` to inject `QKeyEvent` objects instead of emitting custom signals.

**New requirement:** SdlInputManager needs a `QWindow*` pointer to send events to. Passed in at startup from `main.cpp` after the QML engine loads.

**Navigation mode (not capturing):**
- D-pad button press → `QKeyEvent(QEvent::KeyPress, mapped_key, Qt::NoModifier)` sent via `QGuiApplication::sendEvent(window, &event)`
- D-pad button release → `QKeyEvent(QEvent::KeyRelease, mapped_key, Qt::NoModifier)`
- Left stick threshold crossing → same key press/release injection
- R2 trigger threshold crossing → `Key_Shift` press/release

**Capture mode (unchanged):**
- Still emits `bindingCaptured(deviceIndex, element, isAxis, positive)` for raw device info
- Still emits `keyboardCaptured(keyString)` for keyboard binding
- `startCapture()` / `cancelCapture()` unchanged

**Signals removed:** `navigateLeft`, `navigateRight`, `navigateUp`, `navigateDown`, `navigateAccept`, `navigateBack`, `navigateStart`, `navigateDelete`, `navigateShift`.

**Signals kept:** `capturingChanged`, `controllersChanged`, `bindingCaptured`, `keyboardCaptured`.

## Section 2: Cleanup — What Gets Removed

| File | Removal |
|---|---|
| `sdl_input_manager.h` | 9 navigation signal declarations |
| `sdl_input_manager.cpp` | All `emit navigate*()` calls, replaced with `sendEvent()` |
| `themes/modern/SystemPage.qml` | `Connections { target: inputManager }` block, `enabled: !app.settingsOpen` guard |
| `themes/modern/GameListPage.qml` | `Connections { target: inputManager }` block, `enabled: !app.settingsOpen` guard |
| `cpp/qml/AppUI/SettingsOverlay.qml` | Both `Connections { target: inputManager }` blocks (category nav + B-button) |
| `cpp/qml/AppUI/ScraperSettings.qml` | Login + dashboard `Connections { target: inputManager }` blocks, `lastInputWasController` property |
| `cpp/qml/AppUI/VirtualKeyboard.qml` | `Connections { target: inputManager }` block |
| `cpp/qml/AppUI/AppWindow.qml` | `Connections { target: inputManager; onNavigateStart }` block |
| `cpp/src/ui/app_controller.h/cpp` | `settingsOpen` and `virtualKeyboardActive` properties, signals, members |
| `cpp/qml/AppUI/AppWindow.qml` | `onVisibleChanged: app.settingsOpen = visible` on SettingsOverlay |

## Section 3: AppWindow — Settings Toggle via F1

Add a `Shortcut { sequence: "F1" }` in AppWindow.qml that toggles the settings overlay open/closed. Unlike Escape (which also navigates back within settings), F1 is a pure toggle:

- Settings closed → open settings
- Settings open → close settings (does not go back within sub-pages)

The existing Escape Shortcut changes slightly: it must be disabled when the VirtualKeyboard is active, otherwise it intercepts Escape before `Keys.onPressed` can handle it. Since we're removing `app.virtualKeyboardActive`, we use the VirtualKeyboard's own visibility: the Escape Shortcut gets `enabled: !virtualKeyboardActive` where `virtualKeyboardActive` is a simple bool property on AppWindow, set by a binding to check if any child VirtualKeyboard is open. Alternatively, the Escape Shortcut can be replaced with `Keys.onPressed` handling in the SettingsOverlay and AppWindow, which respects focus order. The implementation plan will determine the cleanest approach.

## Section 4: VirtualKeyboard — Pure Keys.onPressed

The VirtualKeyboard's `Connections { target: inputManager }` block is removed entirely. All input comes through `Keys.onPressed`:

- `Key_Up/Down/Left/Right` → grid navigation (focusRow/focusCol)
- `Key_Return` → type the focused key (handleKeyPress)
- `Key_Escape` → cancel (restore initial text, close)
- `Key_Backspace` → delete character
- `Key_Shift` → toggle shift
- `Key_F1` → done (accept text, close)
- `event.text` → physical keyboard passthrough (direct character insertion)

Focus routing is automatic — when the VirtualKeyboard calls `forceActiveFocus()` on open, all key events route to it. When it closes, focus returns to the parent page. No flags needed.

## Section 5: Input Source Detection

The `lastInputWasController` property used for auto-detecting whether to open the virtual keyboard is affected by this change. Since controller input now arrives as keyboard events, we can't distinguish them from physical keyboard presses via `Keys.onPressed`.

**Solution:** SdlInputManager sets a `lastInputWasController` flag (exposed as a Q_PROPERTY) to `true` whenever it injects a key event, and the QML application's `Keys.onPressed` handlers set it to `false` when they receive a key press from the actual keyboard. Since injected events and real keyboard events both arrive through `Keys.onPressed`, we rely on the C++ side to mark the flag before sending the event.

The flow:
1. Controller button pressed → SdlInputManager sets `inputManager.lastInputWasController = true`, then injects QKeyEvent
2. Physical keyboard pressed → SdlInputManager sees the SDL_KEYDOWN, sets `inputManager.lastInputWasController = false` (but does NOT inject — Qt handles real keyboard events natively)
3. QML code checks `inputManager.lastInputWasController` when deciding whether to open the virtual keyboard

This replaces the per-page `lastInputWasController` property with a single centralized one on `inputManager`.

## Files Touched

| File | Action |
|---|---|
| `cpp/src/core/sdl_input_manager.h` | Remove 9 nav signals, add `QWindow*` member + setter, add `lastInputWasController` property |
| `cpp/src/core/sdl_input_manager.cpp` | Replace signal emissions with `QGuiApplication::sendEvent()`, set lastInputWasController flag |
| `cpp/src/main.cpp` | Pass QWindow* to SdlInputManager after QML engine loads |
| `cpp/src/ui/app_controller.h/cpp` | Remove `settingsOpen` and `virtualKeyboardActive` properties |
| `cpp/qml/AppUI/AppWindow.qml` | Remove inputManager Connections, remove settingsOpen binding, add F1 Shortcut |
| `cpp/qml/AppUI/SettingsOverlay.qml` | Remove both inputManager Connections blocks |
| `cpp/qml/AppUI/ScraperSettings.qml` | Remove inputManager Connections, remove lastInputWasController property, use inputManager.lastInputWasController |
| `cpp/qml/AppUI/VirtualKeyboard.qml` | Remove inputManager Connections, consolidate into Keys.onPressed |
| `themes/modern/SystemPage.qml` | Remove inputManager Connections block + settingsOpen guard |
| `themes/modern/GameListPage.qml` | Remove inputManager Connections block + settingsOpen guard |
