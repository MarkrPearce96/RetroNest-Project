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

// Return the index into [NSScreen screens] of the screen displaying the
// frontmost large on-screen window owned by `pid`. Returns -1 if the
// process / window cannot be located. Index form (vs. NSScreen*) keeps
// this header C++-only and lets callers index QGuiApplication::screens()
// directly — Qt mirrors NSScreen ordering on macOS.
int screenIndexForProcess(int64_t pid);

// Apply NSPanel-style configuration to the NSWindow backing a Qt
// top-level QWindow. The argument is the QWindow's winId() — on
// macOS this is the NSView* of the underlying content view.
// Sets style mask, level, collection behavior, and transparency
// for an OpenEmu-style HUD panel that floats above other apps
// without activating our app.
void configurePanelWindow(void* nsViewPtr);

} // namespace MacFullscreen
