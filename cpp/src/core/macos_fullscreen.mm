#include "macos_fullscreen.h"

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <objc/runtime.h>
#include <signal.h>
#include <sys/types.h>

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

    // Register Cmd+Shift+Escape. Cmd+Escape alone is claimed by
    // macOS Sonoma+'s Game Mode HUD when an app is fullscreen-gaming,
    // and Cmd+Opt+Escape is the system Force Quit dialog. Shift is
    // the lowest-friction modifier that avoids both.
    EventHotKeyID hotkeyID = { 'EMUF', 1 };
    OSStatus err = RegisterEventHotKey(kVK_Escape, cmdKey | shiftKey, hotkeyID,
                                        GetApplicationEventTarget(),
                                        0, &s_hotkeyRef);
    if (err == noErr) {
        fprintf(stderr, "[MacFullscreen] Registered Cmd+Shift+Escape global hotkey\n");
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

// Promote a Qt-created NSWindow to a runtime subclass that returns YES
// from canBecomeKeyWindow. Frameless NSWindows return NO by default —
// without this, [window makeKeyWindow] is a no-op and keyboard input
// flows to the (still-foreground) emulator instead of our HUD panel.
// Isa-swizzling rather than allocating a new NSWindow keeps Qt's view
// hierarchy and event delivery intact. The dynamic subclass is created
// once per original class (cached) so multiple panels share it.
static void promoteToKeyEnabled(NSWindow* window) {
    Class originalClass = [window class];
    NSString* subclassName = [NSString stringWithFormat:@"%@_RNKeyEnabled",
                              NSStringFromClass(originalClass)];
    Class subclass = NSClassFromString(subclassName);
    if (!subclass) {
        subclass = objc_allocateClassPair(originalClass,
                                           [subclassName UTF8String], 0);
        if (!subclass) return;
        IMP yesImp = imp_implementationWithBlock(^BOOL(id /*self*/){ return YES; });
        Method protoMethod = class_getInstanceMethod([NSWindow class],
                                                       @selector(canBecomeKeyWindow));
        const char* typeEnc = method_getTypeEncoding(protoMethod);
        class_addMethod(subclass, @selector(canBecomeKeyWindow), yesImp, typeEnc);
        objc_registerClassPair(subclass);
    }
    object_setClass(window, subclass);
}

void configurePanelWindow(void* nsViewPtr) {
    @autoreleasepool {
        if (!nsViewPtr) return;
        NSView* view = (__bridge NSView*)nsViewPtr;
        NSWindow* window = [view window];
        if (!window) return;

        // Override canBecomeKeyWindow so [window makeKeyWindow] actually
        // promotes this window to key. Required for keyboard input
        // (and SDL controller events routed via Qt focusWindow) to land
        // in the panel's QML scene instead of the emulator.
        promoteToKeyEnabled(window);

        [window setStyleMask:(NSWindowStyleMaskBorderless |
                              NSWindowStyleMaskNonactivatingPanel)];

        // NSWindowStyleMaskNonactivatingPanel is documented as valid only
        // on NSPanel. Qt backs QWindow with QNSWindow (an NSWindow subclass),
        // not NSPanel — so the bit may be silently dropped. Detect this at
        // runtime so smoke tests surface it immediately if the HUD ends
        // up activating our app on show.
        NSWindowStyleMask appliedMask = [window styleMask];
        if (!(appliedMask & NSWindowStyleMaskNonactivatingPanel)) {
            fprintf(stderr,
                "[MacFullscreen] WARNING: NSWindowStyleMaskNonactivatingPanel was "
                "not applied (window is %s). The HUD panel will activate the "
                "app on show. Fix: reparent contentView into an NSPanel.\n",
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

void sendKeyToProcess(int64_t pid, int virtualKeyCode) {
    if (pid <= 0) return;
    @autoreleasepool {
        CGEventRef keyDown = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)virtualKeyCode, true);
        if (keyDown) {
            CGEventPostToPid((pid_t)pid, keyDown);
            CFRelease(keyDown);
        }
        CGEventRef keyUp = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)virtualKeyCode, false);
        if (keyUp) {
            CGEventPostToPid((pid_t)pid, keyUp);
            CFRelease(keyUp);
        }
    }
}

void pauseProcess(int64_t pid) {
    if (pid <= 0) return;
    kill((pid_t)pid, SIGSTOP);
}

void resumeProcess(int64_t pid) {
    if (pid <= 0) return;
    kill((pid_t)pid, SIGCONT);
}

} // namespace MacFullscreen

#else
// Non-Apple stubs
namespace MacFullscreen {
void hideMenuBarAndDock() {}
void restoreMenuBarAndDock() {}
void activateOurApp() {}
void activateProcess(int64_t) {}
void sendKeyToProcess(int64_t, int) {}
void pauseProcess(int64_t) {}
void resumeProcess(int64_t) {}
void registerGlobalHotkey(HotkeyCallback) {}
void unregisterGlobalHotkey() {}
int screenIndexForProcess(int64_t) { return -1; }
void configurePanelWindow(void*) {}
void makePanelKey(void*) {}
}
#endif
