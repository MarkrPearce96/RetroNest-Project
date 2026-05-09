# In-Game Menu Overlay Implementation Plan

> **⚠️ Historical document — significantly diverged from what shipped.**
> This plan was the starting point for the feature, but iterative discoveries during implementation changed the architecture in non-trivial ways:
> - Pause mechanism: the plan assumed `PauseOnFocusLoss` would do the work via the panel becoming key. In practice, that mechanism is app-active level not window-key level for most emulators, so each adapter now binds its own TogglePause hotkey to Space and AppController synthesizes Space via `CGEventPostToPid`. SIGSTOP/SIGCONT remained as a fallback.
> - Pause hotkey trigger: changed from `Cmd+Escape` to `Cmd+Shift+Escape` to avoid macOS Sonoma+'s Game Mode HUD claiming the key. Controller trigger is `Select+Start`, plus DualSense Touchpad as a single-button alternative.
> - Achievements: originally routed back to the main app's settings overlay; ships as an inline slide-up popup that keeps the user in the in-game menu context.
> - Close-side input bleed: solved via SDL state polling for A/B/X/Y release before SIGCONT, not via fixed delay.
>
> See the spec (`2026-05-09-in-game-menu-overlay-design.md`, "Pause behavior" section) for the as-shipped pause table and the merged commit list (15+ commits on `main` after `db070ff`) for the actual implementation. Use this plan as background reading only — task numbering, file paths, and code snippets reflect the original design, not what's in the tree.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the in-game menu visually appear *over* the running game for external (process-backed) emulators by hosting it in a non-activating `NSPanel`, and redesign the menu itself as an OpenEmu-style horizontal HUD.

**Architecture:** Two delivery paths share a common HUD component (`InGameMenu.qml`). The libretro path keeps rendering it inside the main window over `EmulationView`. The external-emulator path hosts it in a new floating `NSPanel`-backed `QQuickWindow` (`InGameMenuPanel`) that floats above the emulator without activating our app — pause is triggered by the panel becoming the system key window, which fires each emulator's existing `PauseOnFocusLoss` config.

**Tech Stack:** C++17, Qt6 (QML + Quick), Objective-C++ for AppKit/Carbon bridging, CMake.

**Spec:** `docs/superpowers/specs/2026-05-09-in-game-menu-overlay-design.md`

**Manual-verification testing:** This codebase has no automated test framework — verification is build-success + targeted manual smoke tests on each emulator. Each task ends with the exact build command and the smoke test to perform.

**Build command (used at every verification step):**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build
```

If `build/` doesn't exist yet, configure first:

```sh
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
```

**Run command:** `open ./build/RetroNest.app` (or `./build/RetroNest.app/Contents/MacOS/RetroNest` to see Qt log output in the terminal).

---

## Task 1: Add `screenForProcess` and `configurePanelWindow` helpers

**Files:**
- Modify: `cpp/src/core/macos_fullscreen.h`
- Modify: `cpp/src/core/macos_fullscreen.mm`

These two helpers are pure macOS bridging — they have no Qt dependencies and can be wired in before the panel exists. Verifying them in isolation reduces risk later.

- [ ] **Step 1: Add public declarations to `macos_fullscreen.h`**

Append the two declarations inside `namespace MacFullscreen { ... }` after `unregisterGlobalHotkey()`:

```cpp
// Locate the NSScreen displaying the main window of `pid`.
// Returns a pointer to the NSScreen for the emulator's window, or
// nullptr if the process / window cannot be located. The pointer
// type is opaque (void*) so this header stays C++-only — callers in
// .mm files cast it to NSScreen*.
void* screenForProcess(int64_t pid);

// Apply NSPanel-style configuration to the NSWindow backing a Qt
// top-level QWindow. The argument is the QWindow's winId() — on
// macOS this is the NSView* of the underlying content view.
// Sets style mask, level, collection behavior, and transparency
// for an OpenEmu-style HUD panel that floats above other apps
// without activating our app.
void configurePanelWindow(void* nsViewPtr);
```

- [ ] **Step 2: Implement `configurePanelWindow` in `macos_fullscreen.mm`**

Add inside the `#ifdef __APPLE__ ... namespace MacFullscreen { ... }` block, before the closing `}`:

```objc
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
        // Make sure the panel can receive keyboard input even though
        // it's a non-activating panel.
        [window setMovableByWindowBackground:NO];
    }
}
```

- [ ] **Step 3: Implement `screenForProcess` in `macos_fullscreen.mm`**

Add inside the same `namespace MacFullscreen { ... }` block:

```objc
void* screenForProcess(int64_t pid) {
    @autoreleasepool {
        // Iterate on-screen windows; find the topmost one owned by `pid`,
        // then return the NSScreen whose frame contains that window's
        // origin.
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
            // Skip tiny windows (menus, tooltips).
            if (bounds.size.width < 200 || bounds.size.height < 200) continue;

            // CGWindowList uses top-left origin; NSScreen uses bottom-left.
            // For "which screen contains this window," compare the window's
            // center point against each screen's frame in NSScreen coordinates.
            NSPoint windowCenterCG = NSMakePoint(
                bounds.origin.x + bounds.size.width / 2.0,
                bounds.origin.y + bounds.size.height / 2.0);
            // Convert from CG (top-left) to NSScreen (bottom-left) using primary screen height.
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
```

- [ ] **Step 4: Add non-Apple stubs**

In the `#else` branch at the bottom of `macos_fullscreen.mm`:

```cpp
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
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build build`
Expected: clean build (no new warnings, no errors). The new helpers are unused at this point, but the project must still compile.

- [ ] **Step 6: Commit**

```sh
git add cpp/src/core/macos_fullscreen.h cpp/src/core/macos_fullscreen.mm
git commit -m "macos_fullscreen: add screenForProcess + configurePanelWindow helpers"
```

---

## Task 2: Create placeholder HUD SVG icons

**Files:**
- Create: `cpp/qml/AppUI/images/hud/resume.svg`
- Create: `cpp/qml/AppUI/images/hud/achievements.svg`
- Create: `cpp/qml/AppUI/images/hud/save_quit.svg`
- Create: `cpp/qml/AppUI/images/hud/quit.svg`

Simple monochrome glyphs in a 24×24 viewBox. Replaceable with finished art later — no code change needed.

- [ ] **Step 1: Write `resume.svg` (play triangle)**

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="white" stroke-width="2" stroke-linejoin="round" stroke-linecap="round">
  <path d="M7 5l12 7-12 7V5z" fill="white"/>
</svg>
```

- [ ] **Step 2: Write `achievements.svg` (trophy)**

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="white" stroke-width="2" stroke-linejoin="round" stroke-linecap="round">
  <path d="M8 4h8v5a4 4 0 0 1-8 0V4z"/>
  <path d="M5 5h3v3a3 3 0 0 1-3-3z"/>
  <path d="M16 5h3a3 3 0 0 1-3 3V5z"/>
  <path d="M10 14h4l-.5 4h-3L10 14z"/>
  <path d="M8 20h8"/>
</svg>
```

- [ ] **Step 3: Write `save_quit.svg` (floppy + arrow)**

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="white" stroke-width="2" stroke-linejoin="round" stroke-linecap="round">
  <path d="M4 5h11l4 4v10H4V5z"/>
  <path d="M8 5v4h7V5"/>
  <rect x="8" y="13" width="8" height="6"/>
</svg>
```

- [ ] **Step 4: Write `quit.svg` (power)**

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="white" stroke-width="2" stroke-linejoin="round" stroke-linecap="round">
  <path d="M12 4v8"/>
  <path d="M7.5 6.5a7 7 0 1 0 9 0"/>
</svg>
```

- [ ] **Step 5: Register the SVGs in `cpp/CMakeLists.txt`**

Add to the `qt_add_qml_module(appui_backing ...)` block's `RESOURCES` (or the `QML_FILES` block — match wherever existing controller SVGs are listed; check `images/controllers/*.svg` registration first by searching the file). If the controller SVGs are added via `qt_add_resources` or a `RESOURCES` line, follow the same pattern.

Run: `grep -n "controllers/.*\\.svg\\|images/controllers" cpp/CMakeLists.txt | head -20` to find the existing pattern, then add the four new files alongside.

- [ ] **Step 6: Build and verify**

Run: `cmake --build build`
Expected: clean build. SVGs are bundled into the QML module. Not yet referenced from QML.

- [ ] **Step 7: Commit**

```sh
git add cpp/qml/AppUI/images/hud/ cpp/CMakeLists.txt
git commit -m "qml: add placeholder HUD icons (resume, achievements, save_quit, quit)"
```

---

## Task 3: Rewrite `InGameMenu.qml` as a horizontal HUD

**Files:**
- Modify: `cpp/qml/AppUI/InGameMenu.qml`

Visual rewrite. Keep the public API the same so `AppWindow.qml`'s usage doesn't change yet: same `signal resumeRequested()`, `achievementsRequested(int, string)`, `exitWithSaveRequested()`, `exitWithoutSaveRequested()`, same `open()` / `close()` functions, same `supportsSaveOnExit` property.

This change is verifiable on the libretro path immediately — no panel needed.

- [ ] **Step 1: Replace the entire contents of `cpp/qml/AppUI/InGameMenu.qml`**

```qml
import QtQuick
import QtQuick.Controls

/**
 * InGameMenu — horizontal HUD anchored to bottom-center of its parent.
 * Used by both the libretro in-window path and the external-emulator
 * panel path. Emulator is paused while this menu is open.
 */
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 200

    property int focusIndex: 0

    // Whether the currently running emulator supports save-on-exit.
    // Refreshed each open(). PPSSPP does not support this; PCSX2 +
    // DuckStation + Dolphin do.
    property bool supportsSaveOnExit: true

    // RA state (filled in async after open()).
    property int raGameId: 0
    property string raGameTitle: ""

    signal resumeRequested()
    signal achievementsRequested(int raGameId, string gameTitle)
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()

    function open() {
        focusIndex = 0;
        raGameId = 0;
        raGameTitle = "";
        var gameInfo = app.currentGameInfo();
        supportsSaveOnExit = gameInfo.supportsSaveOnExit === true;
        if (app.hasRACredentials() && gameInfo.title) {
            raGameTitle = gameInfo.title;
            app.raRequestGameIdLookup(gameInfo.title, gameInfo.system || "");
        }
        visible = true;
        forceActiveFocus();
    }

    function close() {
        visible = false;
    }

    Connections {
        target: app
        function onRaGameIdLookupReady(title, lookedUpId) {
            if (title === root.raGameTitle) root.raGameId = lookedUpId;
        }
    }

    // Visible icon-button list, rebuilt when state changes.
    property var hudModel: {
        var items = [
            { icon: "images/hud/resume.svg",       label: "Resume",      action: "resume",     destructive: false }
        ];
        if (raGameId > 0) {
            items.push({ icon: "images/hud/achievements.svg", label: "Achievements", action: "achievements", destructive: false });
        }
        if (supportsSaveOnExit) {
            items.push({ icon: "images/hud/save_quit.svg", label: "Save & Quit", action: "exitSave", destructive: false });
        }
        items.push({ icon: "images/hud/quit.svg", label: "Quit", action: "exitNoSave", destructive: true });
        return items;
    }

    // Clamp focusIndex when the model length changes (e.g. RA lookup fills in).
    onHudModelChanged: {
        if (focusIndex >= hudModel.length) focusIndex = hudModel.length - 1;
        if (focusIndex < 0) focusIndex = 0;
    }

    // ── HUD pill ──
    Rectangle {
        id: pill
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 32
        anchors.horizontalCenter: parent.horizontalCenter
        height: hudRow.implicitHeight + 16
        width: hudRow.implicitWidth + 32
        radius: 14
        color: Qt.rgba(0.08, 0.08, 0.10, 0.88)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        Row {
            id: hudRow
            anchors.centerIn: parent
            spacing: 12

            Repeater {
                model: root.hudModel

                delegate: Rectangle {
                    width: 92
                    height: 76
                    radius: 10
                    color: root.focusIndex === index
                           ? (modelData.destructive
                              ? Qt.rgba(1.0, 0.30, 0.30, 0.18)
                              : Qt.rgba(1.0, 1.0, 1.0, 0.12))
                           : "transparent"

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Image {
                            width: 28
                            height: 28
                            anchors.horizontalCenter: parent.horizontalCenter
                            source: modelData.icon
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            // Tint destructive items red on focus.
                            opacity: root.focusIndex === index ? 1.0 : 0.7
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            font.pixelSize: 12
                            font.weight: root.focusIndex === index ? Font.DemiBold : Font.Normal
                            color: modelData.destructive
                                   ? (root.focusIndex === index ? "#ff8080" : "#ff5050")
                                   : (root.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.65))
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.focusIndex = index
                        onClicked: root.executeAction(modelData.action)
                    }
                }
            }
        }
    }

    // ── Input ──
    Keys.onPressed: function(event) {
        if (!visible) return;
        if (event.key === Qt.Key_Left) {
            focusIndex = Math.max(0, focusIndex - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Right) {
            focusIndex = Math.min(hudModel.length - 1, focusIndex + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            executeAction(hudModel[focusIndex].action);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            resumeRequested();
            event.accepted = true;
        }
    }

    function executeAction(action) {
        switch (action) {
        case "resume":
            resumeRequested();
            break;
        case "achievements":
            achievementsRequested(raGameId, raGameTitle);
            break;
        case "exitSave":
            exitWithSaveRequested();
            break;
        case "exitNoSave":
            exitWithoutSaveRequested();
            break;
        }
    }
}
```

- [ ] **Step 2: Build and verify**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Manual smoke test on libretro**

Launch a libretro game (mGBA core) via `open ./build/RetroNest.app`. Press Cmd+Escape (or Select+Circle on a controller). Expected:
- HUD pill appears at the bottom-center of the screen with "Resume", and "Quit" icons (no "Save & Quit" — mGBA libretro doesn't set `supportsSaveOnExit` per current logic; that's fine for this smoke test). Achievements only if logged in.
- Left/Right cycles through icons; the focused icon highlights.
- Return / A activates the focused icon.
- Esc / B closes the menu (resume).
- The libretro frame is visible behind (no scrim, since the rewrite removed it).

Note: this task does not yet introduce the panel — for external emulators, the menu still uses the in-window path and is technically broken in the same way it was before (over the app page). That's fixed in Task 7.

- [ ] **Step 4: Commit**

```sh
git add cpp/qml/AppUI/InGameMenu.qml
git commit -m "InGameMenu: rewrite as bottom-anchored HUD pill with icon row"
```

---

## Task 4: Create `InGameMenuPanel.qml`

**Files:**
- Create: `cpp/qml/AppUI/InGameMenuPanel.qml`
- Modify: `cpp/CMakeLists.txt`

A `Window` component that wraps the HUD for the external-emulator path. The Window's frame is small (just the pill + bottom margin); the C++ side positions it bottom-center on the emulator's screen.

- [ ] **Step 1: Write `cpp/qml/AppUI/InGameMenuPanel.qml`**

```qml
import QtQuick
import QtQuick.Window

/**
 * InGameMenuPanel — Window host for the in-game HUD when an external
 * emulator is running. Sized and positioned by C++ (InGameMenuPanel)
 * via setGeometry(). Configured as a non-activating NSPanel by
 * MacFullscreen::configurePanelWindow().
 */
Window {
    id: panelWindow
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: false

    // Re-emitted from the embedded HUD so AppController can wire them
    // to the same handlers used by the in-window path.
    signal resumeRequested()
    signal achievementsRequested(int raGameId, string gameTitle)
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()

    function openMenu() {
        visible = true;
        hud.open();
        hud.forceActiveFocus();
    }

    function closeMenu() {
        hud.close();
        visible = false;
    }

    InGameMenu {
        id: hud
        anchors.fill: parent
        onResumeRequested: panelWindow.resumeRequested()
        onAchievementsRequested: function(rid, title) {
            panelWindow.achievementsRequested(rid, title);
        }
        onExitWithSaveRequested: panelWindow.exitWithSaveRequested()
        onExitWithoutSaveRequested: panelWindow.exitWithoutSaveRequested()
    }
}
```

Note: `Qt.WindowDoesNotAcceptFocus` is intentionally there as a Qt-side hint; the AppKit-level `NSWindowStyleMaskNonactivatingPanel` (set in Task 1) is what actually controls the macOS behavior. Both flags exist to prevent the panel's appearance from activating our app.

- [ ] **Step 2: Register the new QML file in `cpp/CMakeLists.txt`**

Locate the `qt_add_qml_module(appui_backing ...)` block (around line 329 per earlier search). Inside its `QML_FILES` list, add:

```
        qml/AppUI/InGameMenuPanel.qml
```

Place it after `qml/AppUI/InGameMenu.qml` to keep it adjacent.

- [ ] **Step 3: Build and verify**

Run: `cmake --build build`
Expected: clean build. The Window component is registered but not yet instantiated (Task 5 wires it up in C++).

- [ ] **Step 4: Commit**

```sh
git add cpp/qml/AppUI/InGameMenuPanel.qml cpp/CMakeLists.txt
git commit -m "qml: add InGameMenuPanel.qml (Window host for external-emulator HUD)"
```

---

## Task 5: Create `InGameMenuPanel` C++ host class

**Files:**
- Create: `cpp/src/ui/in_game_menu_panel.h`
- Create: `cpp/src/ui/in_game_menu_panel.cpp`
- Modify: `cpp/CMakeLists.txt`

Owns a `QQmlComponent` that loads `InGameMenuPanel.qml`, instantiates the `Window` root, configures its underlying `NSWindow` as a non-activating panel, and exposes show/hide + signals.

- [ ] **Step 1: Write `cpp/src/ui/in_game_menu_panel.h`**

```cpp
#pragma once

#include <QObject>
#include <QPointer>

class QQmlEngine;
class QQmlComponent;
class QQuickWindow;

/**
 * InGameMenuPanel — owns a transient QQuickWindow loaded from
 * AppUI/InGameMenuPanel.qml. The window is configured as a
 * non-activating NSPanel that floats above other apps. Show/hide
 * controls visibility; positioning is computed at show() time
 * relative to the emulator's screen.
 */
class InGameMenuPanel : public QObject {
    Q_OBJECT
public:
    explicit InGameMenuPanel(QQmlEngine* engine, QObject* parent = nullptr);
    ~InGameMenuPanel() override;

    /**
     * Show the panel positioned at the bottom-center of the screen
     * displaying the emulator with the given pid. The panel is sized
     * to the HUD's implicit width × ~140px, with ~32px margin from
     * the bottom of that screen.
     */
    void showOverEmulator(int64_t emulatorPid);

    /** Hide the panel (does not destroy it). */
    void hide();

    bool isVisible() const;

signals:
    void resumeRequested();
    void achievementsRequested(int raGameId, const QString& gameTitle);
    void exitWithSaveRequested();
    void exitWithoutSaveRequested();
    void visibilityChanged();

private:
    void ensureCreated();
    void wireSignals();
    void applyPanelChrome();

    QQmlEngine* m_engine;
    QQmlComponent* m_component = nullptr;
    QPointer<QQuickWindow> m_window;
    bool m_chromeApplied = false;
};
```

- [ ] **Step 2: Write `cpp/src/ui/in_game_menu_panel.cpp`**

```cpp
#include "in_game_menu_panel.h"
#include "core/macos_fullscreen.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QMetaObject>
#include <QDebug>

InGameMenuPanel::InGameMenuPanel(QQmlEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {}

InGameMenuPanel::~InGameMenuPanel() = default;

void InGameMenuPanel::ensureCreated() {
    if (m_window) return;

    if (!m_component) {
        m_component = new QQmlComponent(m_engine, QUrl("qrc:/qt/qml/AppUI/InGameMenuPanel.qml"), this);
    }
    if (m_component->isError()) {
        qWarning() << "[InGameMenuPanel] component errors:" << m_component->errors();
        return;
    }

    QObject* obj = m_component->create();
    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        qWarning() << "[InGameMenuPanel] root is not a QQuickWindow";
        if (obj) obj->deleteLater();
        return;
    }
    m_window->setParent(this);

    // Realize the QWindow once so winId() returns the NSView*. We hide
    // immediately after — applyPanelChrome() runs once after the first
    // show() (Step in showOverEmulator below).
    wireSignals();
}

void InGameMenuPanel::wireSignals() {
    if (!m_window) return;
    connect(m_window, SIGNAL(resumeRequested()),
            this, SIGNAL(resumeRequested()));
    connect(m_window, SIGNAL(achievementsRequested(int, QString)),
            this, SIGNAL(achievementsRequested(int, QString)));
    connect(m_window, SIGNAL(exitWithSaveRequested()),
            this, SIGNAL(exitWithSaveRequested()));
    connect(m_window, SIGNAL(exitWithoutSaveRequested()),
            this, SIGNAL(exitWithoutSaveRequested()));
    connect(m_window, &QWindow::visibleChanged,
            this, &InGameMenuPanel::visibilityChanged);
}

void InGameMenuPanel::applyPanelChrome() {
    if (!m_window || m_chromeApplied) return;
    // QWindow::winId() on macOS returns NSView*.
    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::configurePanelWindow(nsView);
    m_chromeApplied = true;
}

void InGameMenuPanel::showOverEmulator(int64_t emulatorPid) {
    ensureCreated();
    if (!m_window) return;

    // Determine target screen.
    void* screenPtr = MacFullscreen::screenForProcess(emulatorPid);
    // Find the matching QScreen by topLeft position.
    QScreen* targetQScreen = QGuiApplication::primaryScreen();
    if (screenPtr) {
        // Walk QGuiApplication::screens() and pick the one whose
        // geometry origin matches; QScreen does not expose NSScreen*
        // directly, so we match via geometry. screensForProcess returns
        // the NSScreen of the emulator's main window — its frame matches
        // the QScreen geometry for that display.
        // (QScreen geometry uses the same coordinate system Qt has set
        // up for the active platform, typically matching NSScreen.)
        for (QScreen* s : QGuiApplication::screens()) {
            // Best-effort: prefer the screen Qt currently considers
            // primary if we can't disambiguate. Geometry equality is a
            // good enough match given typical multi-monitor layouts.
            if (s == QGuiApplication::primaryScreen()) {
                targetQScreen = s;
                // Don't break — keep walking in case a non-primary
                // screen contains the cursor; we'll match below.
            }
        }
        // Refine: pick the screen whose geometry contains the cursor
        // (fallback heuristic when CGWindow / QScreen mapping is fuzzy).
        QPoint cursorPos = QCursor::pos();
        for (QScreen* s : QGuiApplication::screens()) {
            if (s->geometry().contains(cursorPos)) {
                targetQScreen = s;
                break;
            }
        }
    }

    const QRect screenGeom = targetQScreen->geometry();
    const int panelW = 480;
    const int panelH = 140;
    const int bottomMargin = 32;
    const int x = screenGeom.x() + (screenGeom.width() - panelW) / 2;
    const int y = screenGeom.y() + screenGeom.height() - panelH - bottomMargin;
    m_window->setGeometry(x, y, panelW, panelH);

    // Show, then apply NSPanel chrome (winId() must be valid; show
    // realizes it). Calling it on every show is harmless because we
    // gate with m_chromeApplied.
    m_window->show();
    applyPanelChrome();

    // Invoke the QML-side openMenu(): brings up the HUD content +
    // forces focus inside the panel's scene.
    QMetaObject::invokeMethod(m_window, "openMenu");
}

void InGameMenuPanel::hide() {
    if (!m_window) return;
    QMetaObject::invokeMethod(m_window, "closeMenu");
    m_window->hide();
}

bool InGameMenuPanel::isVisible() const {
    return m_window && m_window->isVisible();
}
```

Note on the screen-resolution heuristic: `screenForProcess` returns an `NSScreen*`, which Qt's `QScreen` doesn't expose 1:1. The implementation above falls back to "screen containing the cursor" — a reasonable approximation since the user just pressed a hotkey, the cursor is on the active display. If multi-monitor edge cases surface during testing, this can be tightened.

- [ ] **Step 3: Register the source in `cpp/CMakeLists.txt`**

Inside the `set(SOURCES ...)` block, after `src/ui/app_controller.cpp`, add:

```
    src/ui/in_game_menu_panel.cpp
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build`
Expected: clean build. The class compiles but is not yet constructed by anyone.

- [ ] **Step 5: Commit**

```sh
git add cpp/src/ui/in_game_menu_panel.h cpp/src/ui/in_game_menu_panel.cpp cpp/CMakeLists.txt
git commit -m "ui: add InGameMenuPanel C++ host (NSPanel-backed QQuickWindow)"
```

---

## Task 6: Wire `InGameMenuPanel` into `AppController`

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`
- Modify: `cpp/src/main.cpp`

Add ownership, expose `Q_INVOKABLE` open/close + a property for QML visibility gating, and wire panel signals into the same handlers `AppWindow.qml` uses today.

- [ ] **Step 1: Add forward declaration + member + Q_INVOKABLEs to `cpp/src/ui/app_controller.h`**

After existing forward declarations near the top of the file:

```cpp
class InGameMenuPanel;
```

Inside the `Q_PROPERTY` block (after the `gameRunning` property), add:

```cpp
    Q_PROPERTY(bool inGameMenuPanelVisible READ inGameMenuPanelVisible NOTIFY inGameMenuPanelVisibleChanged)
```

Inside the public section (group with the `activateApp` / `activateEmulator` block), add:

```cpp
    // In-game menu panel (external emulators)
    Q_INVOKABLE void openInGameMenuPanel();
    Q_INVOKABLE void closeInGameMenuPanel();
    bool inGameMenuPanelVisible() const;

    // Inject the QML engine after construction so the panel can be
    // built lazily on first open. Called from main.cpp after the
    // engine is loaded.
    void setQmlEngine(class QQmlEngine* engine);
```

In the `signals:` block, add:

```cpp
    void inGameMenuPanelVisibleChanged();
    void inGameMenuPanelAchievementsRequested(int raGameId, const QString& gameTitle);
```

In the `private:` section, add:

```cpp
    QQmlEngine* m_qmlEngine = nullptr;
    InGameMenuPanel* m_inGameMenuPanel = nullptr;
```

- [ ] **Step 2: Update `cpp/src/ui/app_controller.cpp`**

Add includes near the top:

```cpp
#include "in_game_menu_panel.h"
#include "core/macos_fullscreen.h"
#include "core/game_session.h"
#include <QQmlEngine>
```

(Skip any include already present.)

Add the new method implementations (at the bottom of the file):

```cpp
void AppController::setQmlEngine(QQmlEngine* engine) {
    m_qmlEngine = engine;
}

void AppController::openInGameMenuPanel() {
    if (!m_qmlEngine) {
        qWarning() << "[AppController] openInGameMenuPanel before QML engine set";
        return;
    }
    if (!m_inGameMenuPanel) {
        m_inGameMenuPanel = new InGameMenuPanel(m_qmlEngine, this);
        connect(m_inGameMenuPanel, &InGameMenuPanel::resumeRequested,
                this, [this]() {
                    closeInGameMenuPanel();
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::achievementsRequested,
                this, [this](int raGameId, const QString& title) {
                    closeInGameMenuPanel();
                    // Bring our app forward so the settings overlay
                    // (in the main window) is visible.
                    MacFullscreen::activateOurApp();
                    emit inGameMenuPanelAchievementsRequested(raGameId, title);
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::exitWithSaveRequested,
                this, [this]() {
                    closeInGameMenuPanel();
                    saveAndStopGame(1);
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::exitWithoutSaveRequested,
                this, [this]() {
                    closeInGameMenuPanel();
                    stopGame();
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::visibilityChanged,
                this, &AppController::inGameMenuPanelVisibleChanged);
    }

    int64_t pid = 0;
    if (auto* sess = gameSession()) {
        pid = sess->pid();
    }
    m_inGameMenuPanel->showOverEmulator(pid);
}

void AppController::closeInGameMenuPanel() {
    if (m_inGameMenuPanel) m_inGameMenuPanel->hide();
}

bool AppController::inGameMenuPanelVisible() const {
    return m_inGameMenuPanel && m_inGameMenuPanel->isVisible();
}
```

If `gameSession()` does not have a `pid()` accessor, look in `cpp/src/core/game_session.h` for the existing method that returns the running emulator's process id — earlier exploration showed `MacFullscreen::activateProcess(session->pid())` is already called in `AppController::activateEmulator()`, so the accessor exists.

- [ ] **Step 3: Hook `setQmlEngine` from `cpp/src/main.cpp`**

Find the line `engine.rootContext()->setContextProperty("app", &appController);` in main.cpp. Immediately after it, add:

```cpp
        appController.setQmlEngine(&engine);
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 5: Manual smoke test (no UI invocation yet)**

Launch the app via `open ./build/RetroNest.app`. The new code paths are not yet reachable from QML. Just verify the app boots normally — system browser shows, no regressions in launching libretro or external games.

- [ ] **Step 6: Commit**

```sh
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp cpp/src/main.cpp
git commit -m "AppController: own InGameMenuPanel + expose open/close to QML"
```

---

## Task 7: Route `toggleInGameMenu` in `AppWindow.qml`

**Files:**
- Modify: `cpp/qml/AppUI/AppWindow.qml`

Make `toggleInGameMenu()` decide between the in-window menu (libretro) and the panel (external). Wire the panel's "go to achievements" signal back to the existing settings-overlay navigation. Gate the global-input `Connections` so the panel doesn't double-fire from the main window's handlers.

- [ ] **Step 1: Replace the `toggleInGameMenu()` function (around line 25-38)**

Old:

```qml
    function toggleInGameMenu() {
        if (inGameMenu.visible) {
            app.activateEmulator();
            inGameMenu.close();
            // Resume the libretro core (no-op for process emulators).
            if (app.gameSession) app.gameSession.resumeEmulation();
        } else {
            // Pause the libretro core BEFORE opening the menu so input events
            // routed through the menu don't also reach the running game.
            if (app.gameSession) app.gameSession.pauseEmulation();
            app.activateApp();
            inGameMenu.open();
        }
    }
```

New:

```qml
    // True when a libretro game is the current top of mainStack.
    function isLibretroGame() {
        return mainStack.currentItem && mainStack.currentItem.isEmulationView === true;
    }

    function toggleInGameMenu() {
        if (isLibretroGame()) {
            // Libretro path: in-window HUD. Pause/resume the core
            // explicitly because we don't get PauseOnFocusLoss.
            if (inGameMenu.visible) {
                inGameMenu.close();
                if (app.gameSession) app.gameSession.resumeEmulation();
            } else {
                if (app.gameSession) app.gameSession.pauseEmulation();
                app.activateApp();
                inGameMenu.open();
            }
            return;
        }

        // External-emulator path: floating panel. Pause is triggered
        // by the panel becoming the system key window (each emulator's
        // PauseOnFocusLoss config handles it).
        if (app.inGameMenuPanelVisible) {
            app.closeInGameMenuPanel();
        } else {
            app.openInGameMenuPanel();
        }
    }
```

- [ ] **Step 2: Wire the panel's achievements signal**

Find the existing `Connections { target: app ... }` block that handles `onRaEmulatorLoginPrompt` (around line 193). Add a new function inside the same `Connections` block:

```qml
        function onInGameMenuPanelAchievementsRequested(raGameId, gameTitle) {
            settingsOverlay.navigateToAchievements(raGameId, gameTitle);
        }
```

This mirrors the in-window `inGameMenu.onAchievementsRequested` handler (around line 141) so achievements navigation works identically from both menu hosts.

- [ ] **Step 3: Gate the global hotkey + `inputManager.onInGameMenuRequested` against the panel**

Find the global hotkey `Connections` block (around line 293):

```qml
    Connections {
        target: app
        function onGlobalHotkeyPressed() {
            if (app.gameRunning) window.toggleInGameMenu();
        }
    }
```

No change needed — `toggleInGameMenu()` itself already routes correctly.

Find the `Connections { target: inputManager }` block (around line 537):

```qml
    Connections {
        target: inputManager
        enabled: !inputManager.virtualKeyboardOpen && !gameActionPopup.visible
```

Add `&& !app.inGameMenuPanelVisible` to the `enabled` condition:

```qml
    Connections {
        target: inputManager
        enabled: !inputManager.virtualKeyboardOpen && !gameActionPopup.visible
                 && !app.inGameMenuPanelVisible
```

This stops the main-window input handlers from reacting while the panel owns input — the panel's QML scene has its own SDL flow via Qt's focused-window dispatch.

Rationale: when the panel is visible and key, SDL key events route to it via Qt's focus, and the panel's own `inputManager` connection (added implicitly because `inputManager` is a global context property) would still fire from the main window's scene. Gating on `inGameMenuPanelVisible` prevents the main window from also handling Start / B-button / Cmd+Escape from the SDL side.

- [ ] **Step 4: Update the existing Escape `Shortcut` so it doesn't fire while the panel is visible**

Find the `Shortcut { sequence: "Escape" ... }` block at the bottom of the file (around line 600). Update the `enabled` clause:

Old:

```qml
        enabled: !gameActionPopup.visible && !resumeStateDialog.visible
```

New:

```qml
        enabled: !gameActionPopup.visible && !resumeStateDialog.visible
                 && !app.inGameMenuPanelVisible
```

Reason: Qt `Shortcut` only fires when our app has focus. While the panel is open and our app is technically not active (the emulator is foreground), the shortcut won't fire — but if the user clicks back into the main window for any reason, we want Esc to be a no-op rather than open the wrong menu.

- [ ] **Step 5: Build and verify**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 6: Manual smoke test — libretro path (regression check)**

Launch the app via `open ./build/RetroNest.app`. Launch a libretro game (mGBA core). Press Cmd+Escape. Expected:
- HUD pill appears at the bottom-center over the emulation frame (no scrim).
- Left/Right cycles icons; Return/A activates; Esc/B closes (resume).
- Quit and Save & Quit (if shown) end the game cleanly.
- Achievements (if RA logged in) navigates to the achievements page in the settings overlay.

This verifies that splitting the toggle into libretro/external didn't regress libretro.

- [ ] **Step 7: Manual smoke test — external path (PCSX2 first)**

Launch a PCSX2 game. Verify:
- Game runs in PCSX2's own window (today's behavior — unchanged).
- Press Cmd+Escape: a floating HUD pill appears bottom-center over PCSX2's window.
- PCSX2's audio/video freezes (PauseOnFocusLoss kicks in).
- Left/Right cycles icons; Return/A activates the focused icon.
- Esc/B closes the panel; PCSX2 resumes.
- "Save & Quit" cleanly saves state + exits.
- "Quit" exits without saving.
- "Achievements" (if RA logged in) closes the panel, brings RetroNest to front, shows the achievements page.

If PCSX2 doesn't pause when the panel opens, that's a Risk #1 hit — proceed to Task 8 anyway and revisit pausing in a follow-up.

- [ ] **Step 8: Manual smoke test — DuckStation, PPSSPP, Dolphin**

Repeat Step 7 with each emulator. Note any differences in behavior:
- **DuckStation** — expected to pause (low risk).
- **PPSSPP** — historically finicky; if it does *not* pause when the panel opens, capture that result and continue. Don't fix it in this task; it's covered by Task 8's verification matrix.
- **Dolphin** — expected to pause (low risk).

- [ ] **Step 9: Commit**

```sh
git add cpp/qml/AppUI/AppWindow.qml
git commit -m "AppWindow: route toggleInGameMenu by emulator path; gate input on panel"
```

---

## Task 8: Per-emulator pause verification + remediation

**Files (only if any emulator fails to pause):**
- Modify: `cpp/src/adapters/<emu>_adapter.cpp` — confirm `PauseOnFocusLoss` is being written to the right INI key.
- Or modify: `cpp/src/ui/in_game_menu_panel.cpp` — synthesize the emulator's pause hotkey on panel open as a fallback.

This task only does work if Task 7's manual smoke tests revealed an emulator that fails to pause. Otherwise it's a no-op verification step.

- [ ] **Step 1: Walk the verification matrix**

For each emulator, repeat the panel-open smoke test and record:

| Emulator | Pauses on panel open? | Resumes on panel close? | Notes |
|----------|----------------------|------------------------|-------|
| PCSX2    |                      |                        |       |
| DuckStation |                   |                        |       |
| PPSSPP   |                      |                        |       |
| Dolphin  |                      |                        |       |

- [ ] **Step 2: For any "no" — confirm the emulator's own setting is correct**

Open the emulator's INI file under `{root}/emulators/{emuId}/` and confirm the pause-on-focus-loss setting matches the spec table:

- PCSX2: `[EmuCore] PauseOnFocusLoss=true` in `PCSX2.ini`.
- DuckStation: `[Main] PauseOnFocusLoss=true` in `settings.ini`.
- PPSSPP: `[General] iPauseOnLostFocus=1` in `ppsspp.ini`.
- Dolphin: `[General] PauseOnFocusLost=True` in `Dolphin.ini`.

If the setting is missing, modify the relevant `<emu>_adapter.cpp` to ensure `ensureConfig()` writes it, then rebuild and re-test.

- [ ] **Step 3: If the setting is correct but the emulator still doesn't pause — fallback path**

The most likely culprit is that the emulator's pause-on-focus-loss is keyed off Cocoa's `applicationDidResignActive` (app-level) rather than `windowDidResignKey` (window-level). In that case, our non-activating panel does not trigger pause.

Fallback: synthesize the emulator's own pause hotkey to its window when the panel opens. In `cpp/src/ui/in_game_menu_panel.cpp`, after `m_window->show()` and before `applyPanelChrome()`, add:

```cpp
    // If pause-on-focus-loss didn't trigger for this emulator, the
    // adapter can opt into hotkey-synthesis fallback by registering
    // a pause-keycode for the running pid. (Not implemented here; this
    // is a stub for the fallback if needed during Task 8.)
```

If actually needed, add a `std::function<void(int64_t)> m_pauseFallback` member set by `AppController` based on the running emulator id, and call it here.

- [ ] **Step 4: Document outcomes in a memory entry**

If verification reveals an emulator-specific quirk (e.g. PPSSPP requires a specific INI key value that differs from the default), capture it in user memory under `feedback_panel_pause_<emu>.md` so future-us doesn't re-litigate it.

- [ ] **Step 5: Commit any changes from this task**

```sh
git add -p   # interactively stage only the diffs from Task 8
git commit -m "<emu>: ensure PauseOnFocusLoss is written / fallback for panel pause"
```

If no changes were made (all emulators paused correctly out of the box), there's nothing to commit — just record the verification result in a brief note.

---

## Task 9: Final integration smoke test + cleanup

**Files:** none (verification only).

- [ ] **Step 1: End-to-end smoke (each emulator)**

For each of mGBA (libretro), PCSX2, DuckStation, PPSSPP, Dolphin:

1. Launch the app: `open ./build/RetroNest.app`.
2. Launch a game.
3. Open in-game menu via Cmd+Escape and via Select+Circle (controller).
4. Cycle through HUD icons with Left/Right.
5. Activate Resume — game continues.
6. Re-open menu, activate Save & Quit (where supported) — clean exit, returns to system browser, resume state recorded.
7. Re-launch the same game — ResumeStateDialog appears, resume works.
8. Re-open menu, activate Quit — clean exit, no resume state.
9. Re-open menu (with RA logged in), activate Achievements — settings overlay opens with achievements page; close the overlay; system browser is shown.

- [ ] **Step 2: Multi-monitor sanity check (only if a second display is available)**

Move the emulator window to the secondary display before opening the menu. Verify the panel appears on the *same* display as the emulator, not the primary.

- [ ] **Step 3: macOS native fullscreen sanity check**

If any emulator supports macOS native fullscreen mode, enable it (Cmd+Ctrl+F or the green title-bar button if visible), then open the in-game menu. Verify the panel appears on top of the fullscreened emulator (not pushed behind, not on a different Space).

- [ ] **Step 4: Final cleanup**

Search for any TODO / FIXME / `qDebug()` left in the new code:

```sh
grep -rn "TODO\|FIXME\|qDebug" cpp/src/ui/in_game_menu_panel.* cpp/qml/AppUI/InGameMenu.qml cpp/qml/AppUI/InGameMenuPanel.qml
```

Resolve or remove each.

- [ ] **Step 5: Commit any cleanup**

```sh
git add -p
git commit -m "in-game menu overlay: cleanup + final integration"
```

---

## Self-review checklist

- **Spec coverage:**
  - Two-path architecture (libretro in-window, external panel) — Tasks 4, 5, 7. ✓
  - `NSPanel` configuration (style mask, level, collection behavior, transparency) — Task 1 step 2. ✓
  - `screenForProcess` helper — Task 1 step 3. ✓
  - HUD visual redesign (bottom-center pill, no scrim, no submenu, icons + labels, Left/Right nav) — Task 3. ✓
  - 4 placeholder SVGs in `images/hud/` — Task 2. ✓
  - `InGameMenuPanel` C++ host — Task 5. ✓
  - `InGameMenuPanel.qml` Window wrapper — Task 4. ✓
  - `AppController` ownership + Q_INVOKABLE open/close + visibility property — Task 6. ✓
  - `AppWindow.toggleInGameMenu()` router — Task 7 step 1. ✓
  - Achievements navigation back through settings overlay — Task 7 step 2. ✓
  - Input gating via `inGameMenuPanelVisible` — Task 7 step 3. ✓
  - Per-emulator pause verification matrix — Task 8. ✓
  - Risk-1 fallback (synthesize pause hotkey if PauseOnFocusLoss fails) — Task 8 step 3. ✓
- **Type/name consistency:**
  - `inGameMenuPanelVisible` — used identically in `AppController` Q_PROPERTY (Task 6 step 1), `AppWindow.qml` gates (Task 7 step 3), and `Shortcut.enabled` (Task 7 step 4). ✓
  - `openInGameMenuPanel` / `closeInGameMenuPanel` — defined in Task 6, called from Task 7. ✓
  - `inGameMenuPanelAchievementsRequested` — emitted in Task 6 (AppController) and consumed in Task 7 (AppWindow `Connections`). ✓
  - `showOverEmulator(int64_t)` — defined in Task 5, called from Task 6. ✓
- **Placeholder scan:** No "TBD" / "TODO" / "implement later" in steps. The Task 8 fallback is conditional and explicitly stubbed only if needed during verification. ✓

---

**Plan complete and saved to `docs/superpowers/plans/2026-05-09-in-game-menu-overlay.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
