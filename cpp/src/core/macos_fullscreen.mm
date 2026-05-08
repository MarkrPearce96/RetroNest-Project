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

void* screenForProcess(int64_t pid) {
    @autoreleasepool {
        CFArrayRef windows = CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
            kCGNullWindowID);
        if (!windows) return nullptr;

        NSScreen* result = nullptr;
        CFIndex count = CFArrayGetCount(windows);
        for (CFIndex i = 0; i < count; ++i) {
            CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(windows, i);
            CFNumberRef ownerPidRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowOwnerPID);
            if (!ownerPidRef) continue;
            int64_t ownerPid = 0;
            CFNumberGetValue(ownerPidRef, kCFNumberSInt64Type, &ownerPid);
            if (ownerPid != pid) continue;

            CFDictionaryRef boundsDict = (CFDictionaryRef)CFDictionaryGetValue(info, kCGWindowBounds);
            if (!boundsDict) continue;
            CGRect bounds;
            if (!CGRectMakeWithDictionaryRepresentation(boundsDict, &bounds)) continue;
            if (bounds.size.width < 200 || bounds.size.height < 200) continue;

            NSPoint windowCenterCG = NSMakePoint(
                bounds.origin.x + bounds.size.width / 2.0,
                bounds.origin.y + bounds.size.height / 2.0);
            CGFloat primaryHeight = [[[NSScreen screens] firstObject] frame].size.height;
            NSPoint windowCenter = NSMakePoint(
                windowCenterCG.x,
                primaryHeight - windowCenterCG.y);

            for (NSScreen* screen in [NSScreen screens]) {
                if (NSPointInRect(windowCenter, [screen frame])) {
                    result = screen;
                    break;
                }
            }
            if (result) break;
        }
        CFRelease(windows);

        if (!result) result = [NSScreen mainScreen];
        return (__bridge void*)result;
    }
}

void configurePanelWindow(void* nsViewPtr) {
    @autoreleasepool {
        if (!nsViewPtr) return;
        NSView* view = (__bridge NSView*)nsViewPtr;
        NSWindow* window = [view window];
        if (!window) return;

        [window setStyleMask:(NSWindowStyleMaskBorderless |
                              NSWindowStyleMaskNonactivatingPanel)];
        [window setLevel:NSStatusWindowLevel];
        [window setCollectionBehavior:
            (NSWindowCollectionBehaviorCanJoinAllSpaces |
             NSWindowCollectionBehaviorFullScreenAuxiliary |
             NSWindowCollectionBehaviorTransient)];
        [window setOpaque:NO];
        [window setBackgroundColor:[NSColor clearColor]];
        [window setHasShadow:NO];
        [window setHidesOnDeactivate:NO];
        [window setMovableByWindowBackground:NO];
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
void* screenForProcess(int64_t) { return nullptr; }
void configurePanelWindow(void*) {}
}
#endif
