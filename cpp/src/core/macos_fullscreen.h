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

// Register Cmd+Shift+Escape as a system-wide hotkey (no permissions needed).
// Uses Carbon RegisterEventHotKey — works even when another app has focus.
// Calls the callback on the main thread when the hotkey is pressed.
// Cmd+Escape alone is claimed by macOS Sonoma+'s Game Mode HUD when a
// fullscreen game is active; Shift is the lowest-friction modifier
// that avoids that and Cmd+Opt+Esc (Force Quit dialog).
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

// Make the NSWindow backing a Qt top-level QWindow the system key
// window. Required after every show() of a non-activating panel —
// macOS does not promote it to key just because it was ordered front.
// Used so the emulator's window loses key (firing PauseOnFocusLoss)
// and so SDL/Qt input routes into the panel's QML scene. The
// argument is the QWindow's winId() (NSView* on macOS).
void makePanelKey(void* nsViewPtr);

// Synthesize a keyboard press+release delivered to `pid`'s event
// stream via CGEventPostToPid. Used to toggle the emulator's own
// TogglePause hotkey when the in-game menu opens/closes — the
// emulator pauses itself, suspending its audio thread cleanly
// (no CoreAudio buffer-cut artifacts).
void sendKeyToProcess(int64_t pid, int virtualKeyCode);

// Suspend a process at the OS level via SIGSTOP. Universal pause
// that works for any emulator, but cuts CoreAudio mid-buffer
// (audible click on each transition). Use sendKeyToProcess to
// the emulator's own pause hotkey when possible.
void pauseProcess(int64_t pid);

// Resume a process previously suspended with pauseProcess() (SIGCONT).
void resumeProcess(int64_t pid);

} // namespace MacFullscreen
