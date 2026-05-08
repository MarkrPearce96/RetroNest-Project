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

int screenIndexForProcess(int64_t pid) {
    @autoreleasepool {
        // Skip menu-bar items, tooltips, and other small windows owned by
        // the same PID — only the emulator's main render window is large.
        const CGFloat kMinEmulatorWindowExtent = 200.0;

        CFArrayRef windows = CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
            kCGNullWindowID);
        if (!windows) return -1;

        NSArray<NSScreen*>* allScreens = [NSScreen screens];
        CGFloat primaryHeight = [[allScreens firstObject] frame].size.height;

        int result = -1;
        CFIndex count = CFArrayGetCount(windows);
        // CGWindowListCopyWindowInfo returns windows in front-to-back order
        // for kCGWindowListOptionOnScreenOnly, so the first matching window
        // for `pid` is the topmost.
        for (CFIndex i = 0; i < count; ++i) {
            CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(windows, i);
            CFNumberRef ownerPidRef = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowOwnerPID);
            if (!ownerPidRef) continue;
            int64_t ownerPid = 0;
            if (!CFNumberGetValue(ownerPidRef, kCFNumberSInt64Type, &ownerPid)) continue;
            if (ownerPid != pid) continue;

            CFDictionaryRef boundsDict = (CFDictionaryRef)CFDictionaryGetValue(info, kCGWindowBounds);
            if (!boundsDict) continue;
            CGRect bounds;
            if (!CGRectMakeWithDictionaryRepresentation(boundsDict, &bounds)) continue;
            if (bounds.size.width < kMinEmulatorWindowExtent ||
                bounds.size.height < kMinEmulatorWindowExtent) continue;

            NSPoint windowCenterCG = NSMakePoint(
                bounds.origin.x + bounds.size.width / 2.0,
                bounds.origin.y + bounds.size.height / 2.0);
            NSPoint windowCenter = NSMakePoint(
                windowCenterCG.x,
                primaryHeight - windowCenterCG.y);

            for (NSUInteger si = 0; si < [allScreens count]; ++si) {
                NSScreen* screen = [allScreens objectAtIndex:si];
                if (NSPointInRect(windowCenter, [screen frame])) {
                    result = (int)si;
                    break;
                }
            }
            if (result >= 0) break;
        }
        CFRelease(windows);

        return result;
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

        // NSWindowStyleMaskNonactivatingPanel is documented as valid only
        // on NSPanel. Qt backs QWindow with QNSWindow (an NSWindow subclass),
        // not NSPanel — so the bit may be silently dropped. Detect this at
        // runtime so Task 5's smoke test surfaces it immediately if the HUD
        // ends up activating our app on show.
        NSWindowStyleMask appliedMask = [window styleMask];
        if (!(appliedMask & NSWindowStyleMaskNonactivatingPanel)) {
            fprintf(stderr,
                "[MacFullscreen] WARNING: NSWindowStyleMaskNonactivatingPanel was "
                "not applied (window is %s, expected NSPanel). The HUD panel will "
                "activate the app on show. Fix: reparent contentView into an NSPanel.\n",
                [[[window class] description] UTF8String]);
        }

        [window setLevel:NSStatusWindowLevel];
        [window setCollectionBehavior:
            (NSWindowCollectionBehaviorCanJoinAllSpaces |
             NSWindowCollectionBehaviorFullScreenAuxiliary |
             NSWindowCollectionBehaviorTransient)];
        [window setOpaque:NO];
        [window setBackgroundColor:[NSColor clearColor]];
        [window setHasShadow:NO];
        [window setHidesOnDeactivate:NO];
        // HUD is positioned by C++; prevent accidental drag-to-reposition.
        [window setMovableByWindowBackground:NO];
    }
}

void makePanelKey(void* nsViewPtr) {
    @autoreleasepool {
        if (!nsViewPtr) return;
        NSView* view = (__bridge NSView*)nsViewPtr;
        NSWindow* window = [view window];
        if (!window) return;
        [window makeKeyWindow];
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
int screenIndexForProcess(int64_t) { return -1; }
void configurePanelWindow(void*) {}
void makePanelKey(void*) {}
}
#endif
