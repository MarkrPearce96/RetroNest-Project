#include "macos_fullscreen.h"

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

static EventHotKeyRef s_hotkeyRef = nullptr;
static MacFullscreen::HotkeyCallback s_hotkeyCallback = nullptr;

// Carbon event handler for the global hotkey
static OSStatus hotkeyHandler(EventHandlerCallRef /*nextHandler*/,
                               EventRef /*event*/,
                               void* /*userData*/) {
    if (s_hotkeyCallback) {
        // Dispatch to main thread
        dispatch_async(dispatch_get_main_queue(), ^{
            s_hotkeyCallback();
        });
    }
    return noErr;
}

namespace MacFullscreen {

void hideMenuBarAndDock() {
    @autoreleasepool {
        [NSApp setPresentationOptions:
            NSApplicationPresentationHideDock |
            NSApplicationPresentationHideMenuBar];
    }
}

void restoreMenuBarAndDock() {
    @autoreleasepool {
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];
    }
}

void activateOurApp() {
    @autoreleasepool {
        NSRunningApplication* self = [NSRunningApplication currentApplication];
        [self activateWithOptions:NSApplicationActivateAllWindows |
                                  NSApplicationActivateIgnoringOtherApps];
    }
}

void activateProcess(int64_t pid) {
    @autoreleasepool {
        NSRunningApplication* app =
            [NSRunningApplication runningApplicationWithProcessIdentifier:(pid_t)pid];
        if (app) {
            [app activateWithOptions:NSApplicationActivateAllWindows |
                                     NSApplicationActivateIgnoringOtherApps];
        }
    }
}

void registerGlobalHotkey(HotkeyCallback callback) {
    if (s_hotkeyRef) return; // Already registered

    s_hotkeyCallback = callback;

    // Install Carbon event handler for hotkey events
    EventTypeSpec eventType = { kEventClassKeyboard, kEventHotKeyPressed };
    InstallApplicationEventHandler(&hotkeyHandler, 1, &eventType, nullptr, nullptr);

    // Register Cmd+Escape (kVK_Escape = 53, cmdKey modifier)
    EventHotKeyID hotkeyID = { 'EMUF', 1 };
    OSStatus err = RegisterEventHotKey(kVK_Escape, cmdKey, hotkeyID,
                                        GetApplicationEventTarget(),
                                        0, &s_hotkeyRef);
    if (err == noErr) {
        fprintf(stderr, "[MacFullscreen] Registered Cmd+Escape global hotkey\n");
    } else {
        fprintf(stderr, "[MacFullscreen] Failed to register global hotkey: %d\n", (int)err);
        s_hotkeyRef = nullptr;
    }
}

void unregisterGlobalHotkey() {
    if (s_hotkeyRef) {
        UnregisterEventHotKey(s_hotkeyRef);
        s_hotkeyRef = nullptr;
        s_hotkeyCallback = nullptr;
        fprintf(stderr, "[MacFullscreen] Unregistered global hotkey\n");
    }
}

} // namespace MacFullscreen

#else
// Non-Apple stubs
namespace MacFullscreen {
void hideMenuBarAndDock() {}
void restoreMenuBarAndDock() {}
void activateOurApp() {}
void activateProcess(int64_t) {}
void registerGlobalHotkey(HotkeyCallback) {}
void unregisterGlobalHotkey() {}
}
#endif
