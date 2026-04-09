#pragma once

#include <cstdint>

// macOS-specific helpers for fullscreen, Dock/menu bar, and app switching.
// On non-Apple platforms these are no-ops.

namespace MacFullscreen {

// Hide the menu bar and Dock so they never appear on mouse hover.
void hideMenuBarAndDock();

// Restore normal menu bar and Dock visibility.
void restoreMenuBarAndDock();

// Activate our app (switches macOS to our Space if needed).
void activateOurApp();

// Activate the process with the given PID (switches to its Space).
void activateProcess(int64_t pid);

// Register Cmd+Escape as a system-wide hotkey (no permissions needed).
// Uses Carbon RegisterEventHotKey — works even when another app has focus.
// Calls the callback on the main thread when the hotkey is pressed.
using HotkeyCallback = void(*)();
void registerGlobalHotkey(HotkeyCallback callback);

// Unregister the global hotkey.
void unregisterGlobalHotkey();

} // namespace MacFullscreen
