# PCSX2 Libretro Core — UX Overlays Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every game-time overlay (in-game menu, RA toasts, RA badge, RA indicator bar, FF / save / load pills) render correctly above the Metal NSView for Pattern B HW-render libretro cores (currently PCSX2). End-to-end success = "Cmd+Shift+Escape opens a visible menu over PCSX2's rendered output, RA achievement-active toast appears on game launch, FF / save / load pills show on use, all looking identical to the mGBA experience, and the existing mGBA + external-emulator paths are untouched."

**Architecture:** New `LibretroOverlayPanel` C++ class (modelled on the existing `InGameMenuPanel`) owns a single fullscreen transparent `QQuickWindow` loaded from `LibretroOverlayPanel.qml`. The window is added as a macOS child window of the main RetroNest NSWindow with `[addChildWindow:ordered:NSWindowAbove]` so it tracks geometry and Spaces automatically. It runs `setIgnoresMouseEvents:YES` by default so toasts are visible but mouse / keyboard fall through to the game; clears that flag and becomes the key window when the in-game menu opens. `AppController` shows the panel on `gameStartingLibretro` when `gameUsesHardwareRender()` is true, hides + destroys it on `gameFinished`. The existing seven overlays in `AppWindow.qml` stay in place for mGBA / software / external paths; two small guards (a `toggleInGameMenu` branch and an early-return on RA `Connections`) prevent double-firing for Pattern B sessions.

**Tech Stack:** Qt 6 (`QQuickWindow`, `QQmlComponent`, `Q_INVOKABLE`, `Q_PROPERTY`), Objective-C++ for the macOS-specific window-chrome helpers (`NSWindow addChildWindow:ordered:`, `NSWindow setIgnoresMouseEvents:`), existing `MacFullscreen`'s `configurePanelWindow` / `makePanelKey` for non-activating NSPanel chrome, existing `InGameMenu` / `AchievementToast` / `ActionToast` / `RAIndicatorBar` QML components reused as-is inside the new panel's scene.

**Spec:** [2026-05-11-pcsx2-libretro-ux-overlays-design.md](../specs/2026-05-11-pcsx2-libretro-ux-overlays-design.md)
**Predecessor sub-projects:** SP1 (skeleton), SP2 (VM lifecycle), SP3 (HW render bridge). All functionally complete.

**Conventions used in this plan:**
- `RETRONEST_ROOT` = `/Users/mark/Documents/Projects/RetroNest-Project`
- All work on the `main` branch (matching the established project pattern).
- Build: `cd "$RETRONEST_ROOT/cpp" && cmake --build build -j`. Same toolchain SP3 used.
- Logs: stdout/stderr from `RetroNest.app/Contents/MacOS/RetroNest` is the diagnostic surface. Pipe to `/tmp/retronest-sp35-test.log` during verification tasks.

**File structure (this entire phase):**

| File | Created or modified | Side | Purpose |
|---|---|---|---|
| `${RETRONEST_ROOT}/cpp/src/core/macos_fullscreen.h` | modified | RetroNest | Declare two new helpers: `attachChildWindow`, `setIgnoresMouseEvents`. |
| `${RETRONEST_ROOT}/cpp/src/core/macos_fullscreen.mm` | modified | RetroNest | Implement the helpers via AppKit. |
| `${RETRONEST_ROOT}/cpp/src/ui/app_controller.h` | modified | RetroNest | Add `LibretroOverlayPanel` member, `gameUsesHardwareRender()` accessor, `openLibretroOverlayMenu` / `closeLibretroOverlayMenu` invokables, `libretroOverlayMenuVisible` property + change signal. |
| `${RETRONEST_ROOT}/cpp/src/ui/app_controller.cpp` | modified | RetroNest | Instantiate the panel, wire show-on-game-start / hide-on-game-end, forward overlay signals to `GameSession`, implement the new accessors. |
| `${RETRONEST_ROOT}/cpp/src/ui/libretro_overlay_panel.h` | created | RetroNest | Class declaration mirroring `InGameMenuPanel`. |
| `${RETRONEST_ROOT}/cpp/src/ui/libretro_overlay_panel.cpp` | created | RetroNest | Implementation: load QML, apply chrome, manage `setIgnoresMouseEvents` toggle on menu open/close, attach as a child window of the main NSWindow. |
| `${RETRONEST_ROOT}/cpp/qml/AppUI/LibretroOverlayPanel.qml` | created | RetroNest | Transparent fullscreen `Window` hosting the seven overlay items + RA `Connections`. |
| `${RETRONEST_ROOT}/cpp/qml/AppUI/AppWindow.qml` | modified | RetroNest | `toggleInGameMenu()` branches on `app.gameUsesHardwareRender()`. RA `Connections` handlers add early-return guards. |
| `${RETRONEST_ROOT}/cpp/CMakeLists.txt` | modified | RetroNest | Add the two new C++ source files. |

**Commit checkpoints:** Three substantive commits + one final spec-complete commit:
- `libretro: SP3.5 infrastructure — MacFullscreen helpers + AppController hw-render accessor` (Tasks 1-3)
- `libretro: SP3.5 LibretroOverlayPanel — class, QML, AppController wiring` (Tasks 4-9)
- `libretro: SP3.5 AppWindow.qml routes overlays via HW-render panel` (Task 10)
- `docs(specs): SP3.5 complete — Pattern B overlays verified end-to-end` (Task 15)

**Order of work:** macOS chrome helpers first (small, self-contained); then `AppController` accessor (needed by both panel and `AppWindow.qml`); then the panel class + QML (the bulk of the new code); then `AppController` integration (instantiate, wire show/hide, forward signals, add open/close invokables); then `AppWindow.qml` integration (route menu, guard RA). Each gated by a build verify. Then four user-interactive verification tasks. End-to-end smoke testing gates everything; the spec gets a verification-log update and SP3.5 closes.

---

## Task 1: Add MacFullscreen helpers — `attachChildWindow` and `setIgnoresMouseEvents`

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/src/core/macos_fullscreen.h`
- Modify: `${RETRONEST_ROOT}/cpp/src/core/macos_fullscreen.mm`

- [ ] **Step 1: Locate the existing helper block**

Run:
```sh
grep -nE "^void (configurePanelWindow|makePanelKey|registerGlobalHotkey)" "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/macos_fullscreen.h"
```

Expected: ~3 matches near the bottom of the header. The new helpers slot in alongside them.

- [ ] **Step 2: Add the declarations to `macos_fullscreen.h`**

Open `${RETRONEST_ROOT}/cpp/src/core/macos_fullscreen.h`. Immediately after the existing `void makePanelKey(void* nsView);` declaration, add:

```cpp
/**
 * Add `childNSView`'s NSWindow as a child of `parentNSView`'s NSWindow with
 * NSWindowAbove ordering. The child window then tracks the parent's screen,
 * geometry-on-move, and Spaces membership for free. Used by
 * LibretroOverlayPanel so its transparent fullscreen Window follows
 * RetroNest's main window automatically.
 *
 * Both pointers are NSView* (i.e. QWindow::winId() return values).
 */
void attachChildWindow(void* parentNSView, void* childNSView);

/**
 * Toggle whether `nsView`'s NSWindow ignores mouse events (i.e. lets clicks
 * pass through to whatever is below). When YES, the window remains visible
 * but is not in the responder chain — used by LibretroOverlayPanel while
 * toasts / badges are showing but the in-game menu is closed.
 */
void setIgnoresMouseEvents(void* nsView, bool ignore);
```

- [ ] **Step 3: Add the implementations to `macos_fullscreen.mm`**

Open `${RETRONEST_ROOT}/cpp/src/core/macos_fullscreen.mm`. Find the existing `void makePanelKey(void* nsView)` implementation. Immediately after its closing brace, add:

```objc++
void attachChildWindow(void* parentNSView, void* childNSView)
{
    if (!parentNSView || !childNSView) return;
    NSView* parentView = static_cast<NSView*>(parentNSView);
    NSView* childView  = static_cast<NSView*>(childNSView);
    NSWindow* parentWindow = [parentView window];
    NSWindow* childWindow  = [childView window];
    if (!parentWindow || !childWindow) return;
    [parentWindow addChildWindow:childWindow ordered:NSWindowAbove];
}

void setIgnoresMouseEvents(void* nsView, bool ignore)
{
    if (!nsView) return;
    NSView* view = static_cast<NSView*>(nsView);
    NSWindow* window = [view window];
    if (!window) return;
    [window setIgnoresMouseEvents:(ignore ? YES : NO)];
}
```

- [ ] **Step 4: Build and verify**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build -j 2>&1 | tail -10
```

Use timeout: 900000.
Expected: clean compile of `macos_fullscreen.mm.o`; no warnings about unresolved AppKit symbols. The frameworks `AppKit` and `QuartzCore` are already linked in CMakeLists for the existing file. No new link changes needed.

No commit yet — Task 3 commits Tasks 1-2 together.

---

## Task 2: Add `gameUsesHardwareRender` accessor to `AppController`

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/src/ui/app_controller.h`
- Modify: `${RETRONEST_ROOT}/cpp/src/ui/app_controller.cpp`

This accessor is small and used by both the upcoming panel show/hide wiring and `AppWindow.qml`'s `toggleInGameMenu` branch. Land it independently so each subsequent task only depends on already-merged scaffolding.

- [ ] **Step 1: Add the declaration to `app_controller.h`**

Open `${RETRONEST_ROOT}/cpp/src/ui/app_controller.h`. Find the public method block (around line 200, near the `Q_INVOKABLE` accessors). Add:

```cpp
    /**
     * True iff the currently-running game is a libretro core whose adapter
     * advertises Pattern B HW rendering (PCSX2 today; DuckStation /
     * PPSSPP / Dolphin when those land as libretro). Used by
     * LibretroOverlayPanel + AppWindow.qml to route overlays through the
     * floating-window path that renders above the game's Metal NSView.
     */
    Q_INVOKABLE bool gameUsesHardwareRender() const;
```

- [ ] **Step 2: Add the implementation to `app_controller.cpp`**

Open `${RETRONEST_ROOT}/cpp/src/ui/app_controller.cpp`. Find any other small accessor implementation near the file bottom (e.g. `hasRACredentials`). Immediately after one of them, add:

```cpp
bool AppController::gameUsesHardwareRender() const {
    auto* session = m_gameService.session();
    if (!session || !session->isLibretro()) return false;
    auto* libretro = dynamic_cast<LibretroAdapter*>(session->adapter());
    return libretro && libretro->prefersHardwareRender();
}
```

If `<dynamic_cast>` isn't otherwise needed in this TU, no extra include is required — `dynamic_cast` is a language operator. The required class headers (`GameSession`, `LibretroAdapter`) are already included by `app_controller.cpp` (verify via `grep -n "#include" app_controller.cpp | head -10` if unsure).

- [ ] **Step 3: Build and verify**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build -j 2>&1 | tail -8
```

Expected: clean compile of `app_controller.cpp.o`. No commit yet.

Use timeout: 900000.

---

## Task 3: Commit infrastructure (Tasks 1-2)

**Files:** none (commit only).

- [ ] **Step 1: Stage and commit**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git status
git add cpp/src/core/macos_fullscreen.h \
        cpp/src/core/macos_fullscreen.mm \
        cpp/src/ui/app_controller.h \
        cpp/src/ui/app_controller.cpp
git commit -m "$(cat <<'EOF'
libretro: SP3.5 infrastructure — MacFullscreen helpers + AppController hw-render accessor

Small scaffolding for the upcoming LibretroOverlayPanel:

- MacFullscreen::attachChildWindow(parentNSView, childNSView) wraps
  [parentWindow addChildWindow:childWindow ordered:NSWindowAbove] so a
  Qt-owned child Window can track its parent's screen / geometry / Spaces
  membership for free.

- MacFullscreen::setIgnoresMouseEvents(nsView, ignore) toggles whether
  an NSWindow is in the responder chain. Used to keep the overlay
  window visible (for toasts / badges) while still passing mouse events
  through to the game NSView below; cleared when the in-game menu opens.

- AppController::gameUsesHardwareRender() = true when the running game
  is a libretro core whose adapter advertises Pattern B HW rendering
  (PCSX2 today). Used by both the upcoming overlay wiring and
  AppWindow.qml's toggleInGameMenu branch.

No behavioural change yet — none of this is called from any code path.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Create `libretro_overlay_panel.h`

**Files:**
- Create: `${RETRONEST_ROOT}/cpp/src/ui/libretro_overlay_panel.h`

Mirrors the existing `InGameMenuPanel` shape: one constructor taking the `QQmlEngine*`, signal forwarders for the inner `InGameMenu` actions, `show` / `hide` / `openMenu` / `closeMenu` / `isMenuOpen` methods.

- [ ] **Step 1: Write the header**

Create `${RETRONEST_ROOT}/cpp/src/ui/libretro_overlay_panel.h` with this exact content:

```cpp
#pragma once

#include <QObject>
#include <QPointer>

class QQmlEngine;
class QQmlComponent;
class QQuickWindow;

/**
 * LibretroOverlayPanel — owns the fullscreen transparent QQuickWindow
 * loaded from AppUI/LibretroOverlayPanel.qml. Used by Pattern B HW-render
 * libretro cores (PCSX2 today) to render the in-game menu, RA toasts, RA
 * indicator bar, and FF / save / load pills above the Metal NSView,
 * which would otherwise composite on top of any in-scene QML overlay.
 *
 * Lifecycle: the C++ instance lives for AppController's lifetime. The
 * underlying QQuickWindow is created lazily on the first
 * showForCurrentGame() call, hidden + destroyed on hide(), and recreated
 * on the next show. mGBA / software-libretro / external-emulator paths
 * never trigger this panel.
 *
 * Modelled on InGameMenuPanel; the two coexist (external emulators use
 * the existing one).
 */
class LibretroOverlayPanel : public QObject {
    Q_OBJECT
public:
    explicit LibretroOverlayPanel(QQmlEngine* engine, QObject* parent = nullptr);
    ~LibretroOverlayPanel() override;

    /**
     * Create the QQuickWindow if needed, position it over RetroNest's
     * main window, attach it as a macOS child window above the main
     * window, and show it. The window starts with mouse events ignored
     * so toasts / badges are visible but clicks fall through to the
     * game NSView below.
     */
    void showForCurrentGame();

    /** Hide and destroy the underlying QQuickWindow. */
    void hide();

    /** Show the in-game menu inside the panel. Clears
     *  setIgnoresMouseEvents and makes the panel the system key window. */
    void openMenu();

    /** Close the in-game menu and reapply setIgnoresMouseEvents. */
    void closeMenu();

    bool isMenuOpen() const;

signals:
    void resumeRequested();
    void exitWithSaveRequested();
    void exitWithoutSaveRequested();
    void saveStateRequested();
    void loadStateRequested();
    void toggleFastForwardRequested();
    void menuVisibleChanged();

private:
    void ensureCreated();
    void wireSignals();

    QQmlEngine* m_engine;
    QQmlComponent* m_component = nullptr;
    QPointer<QQuickWindow> m_window;
    bool m_menuOpen = false;
};
```

- [ ] **Step 2: Verify it exists**

Run:
```sh
wc -l "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro_overlay_panel.h"
```

Expected: ~55 lines.

No commit. Task 9 commits panel-class work.

---

## Task 5: Create `libretro_overlay_panel.cpp` skeleton (ensureCreated, show, hide, signal forwarders)

**Files:**
- Create: `${RETRONEST_ROOT}/cpp/src/ui/libretro_overlay_panel.cpp`

This implements the lifecycle (create-on-show, destroy-on-hide) and the chrome calls. The menu open/close branches are stubbed for now; Task 8 wires them after the QML is in place.

- [ ] **Step 1: Write the implementation**

Create `${RETRONEST_ROOT}/cpp/src/ui/libretro_overlay_panel.cpp` with this exact content:

```cpp
#include "libretro_overlay_panel.h"
#include "core/macos_fullscreen.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QDebug>

LibretroOverlayPanel::LibretroOverlayPanel(QQmlEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    Q_ASSERT(engine);
}

LibretroOverlayPanel::~LibretroOverlayPanel() = default;

void LibretroOverlayPanel::ensureCreated() {
    if (m_window) return;

    if (!m_component) {
        m_component = new QQmlComponent(m_engine,
            QUrl(QStringLiteral("qrc:/AppUI/qml/AppUI/LibretroOverlayPanel.qml")), this);
    }
    if (m_component->isError()) {
        qWarning() << "[LibretroOverlayPanel] component errors:" << m_component->errors();
        return;
    }

    QObject* obj = m_component->create();
    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        qWarning() << "[LibretroOverlayPanel] root is not a QQuickWindow";
        if (obj) obj->deleteLater();
        return;
    }
    // QObject parenting: window dies with this panel instance.
    static_cast<QObject*>(m_window.data())->setParent(this);

    wireSignals();
}

void LibretroOverlayPanel::wireSignals() {
    if (!m_window) return;
    connect(m_window, SIGNAL(resumeRequested()),
            this, SIGNAL(resumeRequested()));
    connect(m_window, SIGNAL(exitWithSaveRequested()),
            this, SIGNAL(exitWithSaveRequested()));
    connect(m_window, SIGNAL(exitWithoutSaveRequested()),
            this, SIGNAL(exitWithoutSaveRequested()));
    connect(m_window, SIGNAL(saveStateRequested()),
            this, SIGNAL(saveStateRequested()));
    connect(m_window, SIGNAL(loadStateRequested()),
            this, SIGNAL(loadStateRequested()));
    connect(m_window, SIGNAL(toggleFastForwardRequested()),
            this, SIGNAL(toggleFastForwardRequested()));
}

void LibretroOverlayPanel::showForCurrentGame() {
    ensureCreated();
    if (!m_window) return;

    // Cover the screen the primary RetroNest window lives on. RetroNest
    // runs borderless fullscreen, so this is the whole screen geometry.
    QScreen* primary = QGuiApplication::primaryScreen();
    if (primary) m_window->setGeometry(primary->geometry());

    m_window->show();

    // Apply NSPanel chrome + child-window link + start in
    // mouse-passthrough mode.
    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::configurePanelWindow(nsView);
    MacFullscreen::setIgnoresMouseEvents(nsView, true);

    // Attach as a child of the main RetroNest window so we track its
    // screen + geometry + Spaces membership.
    auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* w : windows) {
        if (w == m_window) continue;
        if (w->flags() & Qt::Tool) continue;  // skip other tool windows
        if (!w->isVisible()) continue;
        void* parentNSView = reinterpret_cast<void*>(w->winId());
        MacFullscreen::attachChildWindow(parentNSView, nsView);
        break;
    }
}

void LibretroOverlayPanel::hide() {
    if (!m_window) return;
    m_window->hide();
    // Destroying the QQuickWindow forces a fresh scene graph on the next
    // game start; matches the agreed-upon "created at game start,
    // destroyed at game end" lifecycle and avoids stale state from a
    // prior session leaking forward.
    m_window->deleteLater();
    m_window.clear();
    m_menuOpen = false;
}

void LibretroOverlayPanel::openMenu() {
    ensureCreated();
    if (!m_window) return;

    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::setIgnoresMouseEvents(nsView, false);
    MacFullscreen::makePanelKey(nsView);

    QMetaObject::invokeMethod(m_window, "openMenu");
    m_menuOpen = true;
    emit menuVisibleChanged();
}

void LibretroOverlayPanel::closeMenu() {
    if (!m_window) return;

    QMetaObject::invokeMethod(m_window, "closeMenu");

    void* nsView = reinterpret_cast<void*>(m_window->winId());
    MacFullscreen::setIgnoresMouseEvents(nsView, true);

    m_menuOpen = false;
    emit menuVisibleChanged();
}

bool LibretroOverlayPanel::isMenuOpen() const {
    return m_menuOpen;
}
```

- [ ] **Step 2: Verify it exists**

```sh
wc -l "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro_overlay_panel.cpp"
```

Expected: ~125 lines.

No commit. Build verify lands in Task 6 once CMake knows about the file.

---

## Task 6: Add `libretro_overlay_panel.{h,cpp}` to CMakeLists; verify build

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/CMakeLists.txt`

- [ ] **Step 1: Find the right insertion point**

Run:
```sh
grep -n "src/ui/in_game_menu_panel" "/Users/mark/Documents/Projects/RetroNest-Project/cpp/CMakeLists.txt"
```

Expected: two matches — one in the sources list, one in the headers list.

- [ ] **Step 2: Add the new C++ files alongside `in_game_menu_panel`**

Open `${RETRONEST_ROOT}/cpp/CMakeLists.txt`. Find the line `src/ui/in_game_menu_panel.cpp` in the main sources list. Add **immediately after it**:

```cmake
    src/ui/libretro_overlay_panel.cpp
```

Find the line `src/ui/in_game_menu_panel.h` in the headers list. Add **immediately after it**:

```cmake
    src/ui/libretro_overlay_panel.h
```

- [ ] **Step 3: Reconfigure CMake and build**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake -B build 2>&1 | tail -5
```

Expected: CMake reconfigures successfully (`-- Configuring done`, `-- Generating done`, `-- Build files have been written to`).

Then:
```sh
cmake --build build -j 2>&1 | tail -15
```

Use timeout: 1800000.
Expected: clean build through `libretro_overlay_panel.cpp.o`. Final linker step succeeds with `[100%] Built target RetroNest`. The new class isn't yet instantiated anywhere so its presence in the binary is benign.

If the build complains about missing `QQuickWindow` / `QQmlComponent` includes, recheck the `cpp/src/ui/libretro_overlay_panel.cpp` includes against the snippet in Task 5 — those headers should resolve via the same Qt6 module set the existing `in_game_menu_panel.cpp` uses.

No commit yet — Task 9 commits panel-class work.

---

## Task 7: Create `LibretroOverlayPanel.qml`

**Files:**
- Create: `${RETRONEST_ROOT}/cpp/qml/AppUI/LibretroOverlayPanel.qml`

This is the fullscreen transparent `Window` hosting all seven overlays. Each is a new QML instance (a `QQuickItem` can't live in two `QQuickWindow`s simultaneously), copied verbatim from `AppWindow.qml`'s overlay declarations.

- [ ] **Step 1: Write the QML file**

Create `${RETRONEST_ROOT}/cpp/qml/AppUI/LibretroOverlayPanel.qml` with this exact content:

```qml
import QtQuick
import QtQuick.Window
import QtMultimedia
import AppUI

/**
 * LibretroOverlayPanel — fullscreen transparent Window that hosts all
 * game-time overlays for Pattern B HW-render libretro cores (PCSX2). The
 * containing C++ instance (LibretroOverlayPanel) attaches this Window as
 * a macOS child of RetroNest's main NSWindow so it tracks geometry +
 * Spaces automatically.
 *
 * The seven overlays (one menu, three action toasts, RA toast, RA
 * indicator bar) are duplicated from AppWindow.qml — they have to be
 * separate QQuickItem instances because each QQuickWindow has its own
 * scene graph. Anchors and visuals are copied 1:1 so the look is
 * bit-identical to the in-scene path mGBA uses today.
 */
Window {
    id: panelWindow
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: false

    // Forwarded from the embedded InGameMenu to the C++ panel, which
    // forwards them to AppController -> GameSession.
    signal resumeRequested()
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()
    signal saveStateRequested()
    signal loadStateRequested()
    signal toggleFastForwardRequested()

    function openMenu() {
        inGameMenu.open();
        inGameMenu.forceActiveFocus();
    }

    function closeMenu() {
        inGameMenu.close();
    }

    // ── In-game menu (bottom-center HUD pill) ──
    InGameMenu {
        id: inGameMenu
        onResumeRequested:           panelWindow.resumeRequested()
        onExitWithSaveRequested:     panelWindow.exitWithSaveRequested()
        onExitWithoutSaveRequested:  panelWindow.exitWithoutSaveRequested()
        onSaveStateRequested:        panelWindow.saveStateRequested()
        onLoadStateRequested:        panelWindow.loadStateRequested()
        onToggleFastForwardRequested:panelWindow.toggleFastForwardRequested()
    }

    // ── Top-right action toasts (FF / save / load) ──
    // Wired to local actions inside InGameMenu's handlers (below), and to
    // the same gameSession.toggleFastForwardLibretro / saveStateLibretro
    // / loadStateLibretro calls that AppWindow.qml uses for mGBA. The
    // menu's handlers below trigger the relevant .show() locally.
    Column {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 32
        anchors.rightMargin: 32
        spacing: 8
        z: 220

        ActionToast {
            id: ffToast
            iconSource: "images/hud/fast_forward.svg"
            label: "4×"
            sticky: true
        }
        ActionToast {
            id: saveToast
            iconSource: "images/hud/save_state.svg"
            label: "Saved"
        }
        ActionToast {
            id: loadToast
            iconSource: "images/hud/load_state.svg"
            label: "Loaded"
        }
    }

    // ── Achievement toast (top-right, separate from the small Column) ──
    AchievementToast {
        id: achievementToast
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 32
        anchors.rightMargin: 32
        z: 220
    }

    // ── Achievement unlock chime ──
    SoundEffect {
        id: unlockSound
        source: "qrc:/AppUI/qml/AppUI/sounds/Libretro_Achievement_Unlock.wav"
        volume: 1.0
    }

    // ── RA indicator bar (bottom-left) ──
    RAIndicatorBar {
        id: raIndicators
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 24
        anchors.bottomMargin: 24
        z: 220
    }

    // ── Drop persistent toasts when the game ends ──
    Connections {
        target: app
        function onGameRunningChanged() {
            if (!app.gameRunning) {
                ffToast.hide();
                saveToast.hide();
                loadToast.hide();
                raIndicators.clear();
            }
        }
    }

    // ── RA: achievement unlock ──
    Connections {
        target: app
        function onRaAchievementUnlocked(id, title, description, imageUrl) {
            if (!app.gameUsesHardwareRender()) return;
            achievementToast.show(title, description, imageUrl);
            if (app.raSoundEffects) unlockSound.play();
        }
    }

    // ── RA: launch banner + other info toasts ──
    Connections {
        target: app
        function onRaInfoToast(header, title, description, imageUrl, durationMs) {
            if (!app.gameUsesHardwareRender()) return;
            achievementToast.show(title, description, imageUrl);
        }
    }

    // ── RA: persistent indicator chips ──
    Connections {
        target: app
        function onRaIndicator(kind, data) {
            if (!app.gameUsesHardwareRender()) return;
            raIndicators.dispatch(kind, data);
        }
    }

    // ── Local hookups for menu-triggered toasts ──
    // These mirror AppWindow.qml: pressing Save State in the menu pops a
    // "Saved" pill; Load State pops "Loaded"; toggling FF pops or hides
    // the FF pill. The C++ side already routes the menu's signal to
    // GameSession::saveStateLibretro/loadStateLibretro/toggleFastForwardLibretro.
    Connections {
        target: panelWindow
        function onSaveStateRequested() { saveToast.show(); }
        function onLoadStateRequested() { loadToast.show(); }
        function onToggleFastForwardRequested() {
            // Mirror AppWindow.qml: AppController's gameSession returns
            // the new state via toggleFastForwardLibretro(); but the
            // panel's signal alone doesn't carry it. AppController will
            // wire show/hide based on the returned value (see Task 9).
        }
    }
}
```

- [ ] **Step 2: Verify it exists**

```sh
wc -l "/Users/mark/Documents/Projects/RetroNest-Project/cpp/qml/AppUI/LibretroOverlayPanel.qml"
```

Expected: ~125 lines.

No commit. Task 9 commits panel-class work.

---

## Task 8: Verify QML module picks up the new file; build

**Files:** none (verification).

- [ ] **Step 1: Reconfigure CMake (QML autoscan) and build**

The `appui_backing` QML module is configured to glob QML files in `cpp/qml/AppUI`. A reconfigure picks up new files. Run:

```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake -B build 2>&1 | tail -5
cmake --build build -j 2>&1 | tail -15
```

Use timeout: 1800000.
Expected: `[100%] Built target RetroNest`. No errors about unresolved QML imports or missing types.

- [ ] **Step 2: Sanity-check the QML compiles by listing target QML files**

```sh
find "/Users/mark/Documents/Projects/RetroNest-Project/cpp/build" -name "LibretroOverlayPanel.qml*" 2>/dev/null
```

Expected: at least one match (e.g. `build/AppUI/qml/AppUI/LibretroOverlayPanel.qml` or similar compiled location), confirming the QML file made it into the resource bundle.

If the file isn't found in `build/`, the QML module glob may need explicit `qt_target_qml_sources` registration — search `CMakeLists.txt` for `qt_add_qml_module` or `qt_target_qml_sources` and add the new file alongside the existing `InGameMenuPanel.qml` entry.

---

## Task 9: Wire `LibretroOverlayPanel` into `AppController` and commit panel-class work

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/src/ui/app_controller.h`
- Modify: `${RETRONEST_ROOT}/cpp/src/ui/app_controller.cpp`

This instantiates the panel, wires lifecycle (show on `gameStartingLibretro` if HW render, hide on `gameFinished`), forwards the panel's overlay signals to `GameSession`, and exposes the QML-facing `openLibretroOverlayMenu` / `closeLibretroOverlayMenu` invokables + `libretroOverlayMenuVisible` property.

- [ ] **Step 1: Add the include + member to `app_controller.h`**

Open `${RETRONEST_ROOT}/cpp/src/ui/app_controller.h`. Find the existing `#include "in_game_menu_panel.h"` (or similar UI-layer include); add immediately after it:

```cpp
#include "libretro_overlay_panel.h"
```

Find the member declarations block where `m_inGameMenuPanel` is declared. Add immediately after it:

```cpp
    std::unique_ptr<LibretroOverlayPanel> m_libretroOverlayPanel;
```

If `std::unique_ptr` isn't already used in the file, add `#include <memory>` near the top.

- [ ] **Step 2: Add the invokables, property, and change signal to `app_controller.h`**

In the same header, find the public methods block (near `gameUsesHardwareRender` from Task 2). Add:

```cpp
    Q_INVOKABLE void openLibretroOverlayMenu();
    Q_INVOKABLE void closeLibretroOverlayMenu();
```

Find the existing `Q_PROPERTY` block (near `inGameMenuPanelVisible`). Add:

```cpp
    Q_PROPERTY(bool libretroOverlayMenuVisible
               READ libretroOverlayMenuVisible
               NOTIFY libretroOverlayMenuVisibleChanged)

    bool libretroOverlayMenuVisible() const;
```

Find the signals section. Add:

```cpp
    void libretroOverlayMenuVisibleChanged();
```

- [ ] **Step 3: Instantiate + wire in `app_controller.cpp`**

Open `${RETRONEST_ROOT}/cpp/src/ui/app_controller.cpp`. Find the constructor body (the `AppController::AppController(...)` definition). Locate where `m_inGameMenuPanel` is instantiated (it lives near the end of the constructor where QML-engine-dependent setup runs). The instantiation pattern is:

```cpp
m_libretroOverlayPanel = std::make_unique<LibretroOverlayPanel>(engine_pointer_here, this);
```

If `InGameMenuPanel` is instantiated via a deferred `setEngine(...)` call (because the engine isn't available in the constructor), mirror that pattern instead — search for where `m_inGameMenuPanel` first receives a non-null engine and slot the libretro panel's instantiation immediately after.

Once the panel is instantiated, wire the lifecycle. After the panel instantiation, add:

```cpp
    connect(this, &AppController::gameStartingLibretro, this, [this] {
        if (gameUsesHardwareRender())
            m_libretroOverlayPanel->showForCurrentGame();
    });
    connect(this, &AppController::gameFinished, this, [this](int, bool) {
        if (m_libretroOverlayPanel)
            m_libretroOverlayPanel->hide();
    });
    // Forward overlay signals to the running GameSession, mirroring the
    // existing InGameMenuPanel forwarders.
    auto* session = m_gameService.session();
    connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::resumeRequested,
            session, &GameSession::resumeEmulation);
    connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::saveStateRequested,
            session, [session, this] {
                session->saveStateLibretro(1);
                session->resumeEmulation();
                m_libretroOverlayPanel->closeMenu();
            });
    connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::loadStateRequested,
            session, [session, this] {
                session->loadStateLibretro(1);
                session->resumeEmulation();
                m_libretroOverlayPanel->closeMenu();
            });
    connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::toggleFastForwardRequested,
            session, [session] {
                session->toggleFastForwardLibretro();
                // FF is a state toggle — leave the menu open so the user
                // can verify the change and click Resume when ready,
                // matching AppWindow.qml's libretro path.
            });
    connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::exitWithSaveRequested,
            this, [this] {
                if (auto* s = m_gameService.session()) s->resumeEmulation();
                m_libretroOverlayPanel->closeMenu();
                m_gameService.saveAndStopGame(/*slot=*/1);
            });
    connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::exitWithoutSaveRequested,
            this, [this] {
                if (auto* s = m_gameService.session()) s->resumeEmulation();
                m_libretroOverlayPanel->closeMenu();
                m_gameService.stopGame();
            });
    connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::menuVisibleChanged,
            this, &AppController::libretroOverlayMenuVisibleChanged);
```

If `m_gameService.saveAndStopGame` / `stopGame` aren't the exact method names, mirror whatever the existing `InGameMenuPanel`'s `exitWithSaveRequested` / `exitWithoutSaveRequested` forwarders call — search for `exitWithSaveRequested` in `app_controller.cpp`.

- [ ] **Step 4: Add the invokable and property implementations**

At the end of `app_controller.cpp`, add:

```cpp
void AppController::openLibretroOverlayMenu() {
    if (!m_libretroOverlayPanel) return;
    if (auto* s = m_gameService.session()) s->pauseEmulation();
    m_libretroOverlayPanel->openMenu();
}

void AppController::closeLibretroOverlayMenu() {
    if (!m_libretroOverlayPanel) return;
    m_libretroOverlayPanel->closeMenu();
    if (auto* s = m_gameService.session()) s->resumeEmulation();
}

bool AppController::libretroOverlayMenuVisible() const {
    return m_libretroOverlayPanel && m_libretroOverlayPanel->isMenuOpen();
}
```

- [ ] **Step 5: Build and verify**

```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build -j 2>&1 | tail -15
```

Use timeout: 1800000.
Expected: clean compile of `app_controller.cpp.o`, link succeeds. No warnings about unused/uninitialized members.

- [ ] **Step 6: Commit panel class + QML + AppController wiring**

```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add cpp/src/ui/libretro_overlay_panel.h \
        cpp/src/ui/libretro_overlay_panel.cpp \
        cpp/qml/AppUI/LibretroOverlayPanel.qml \
        cpp/CMakeLists.txt \
        cpp/src/ui/app_controller.h \
        cpp/src/ui/app_controller.cpp
git status
```

Expected: only those six files staged.

```sh
git commit -m "$(cat <<'EOF'
libretro: SP3.5 LibretroOverlayPanel — class, QML, AppController wiring

Adds a fullscreen transparent QQuickWindow that hosts all seven game-time
overlays (in-game menu pill, FF / save / load action toasts, RA unlock
toast, RA launch banner, RA indicator bar) for Pattern B HW-render
libretro cores. The window is attached as a macOS child of RetroNest's
main NSWindow with NSWindowAbove ordering, so it composites visibly
above PCSX2's CAMetalLayer NSView while still tracking the main window's
screen / Spaces automatically.

- libretro_overlay_panel.{h,cpp}: mirrors the existing InGameMenuPanel
  shape. Lifecycle is "created on first showForCurrentGame, destroyed
  on hide", recreated on each game start. openMenu / closeMenu toggle
  setIgnoresMouseEvents (so toasts are visible but pass clicks through
  by default; the menu intercepts when open) and call makePanelKey on
  open so keyboard input routes to the menu.

- LibretroOverlayPanel.qml: replicates AppWindow.qml's overlay region.
  Connections to app.raAchievementUnlocked / raInfoToast / raIndicator
  early-return if !app.gameUsesHardwareRender() — duplicate-fire guard
  for when AppWindow.qml's in-scene handlers also receive the signal
  during non-Pattern-B sessions.

- AppController wires showForCurrentGame on gameStartingLibretro (when
  HW render) and hide on gameFinished. Forwards the panel's six action
  signals to GameSession. Exposes openLibretroOverlayMenu /
  closeLibretroOverlayMenu Q_INVOKABLEs + libretroOverlayMenuVisible
  Q_PROPERTY so AppWindow.qml can drive menu toggle through the panel.

mGBA / software / external paths unchanged. AppWindow.qml integration
lands next.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Route `AppWindow.qml`'s menu through the panel for HW cores; guard RA Connections

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/qml/AppUI/AppWindow.qml`

Two small surgical edits: `toggleInGameMenu()` gains a hw-render branch at the top, and the three RA `Connections` (`onRaAchievementUnlocked`, `onRaInfoToast`, `onRaIndicator`) gain an early-return guard so they don't double-fire when the overlay panel's `Connections` also handle the signal.

- [ ] **Step 1: Route `toggleInGameMenu()` through the panel for HW cores**

Open `${RETRONEST_ROOT}/cpp/qml/AppUI/AppWindow.qml`. Find the existing `function toggleInGameMenu()` block (around line 31). Replace its body — keeping the function signature — with:

```qml
    function toggleInGameMenu() {
        // SP3.5: Pattern B HW-render libretro cores route through the
        // floating LibretroOverlayPanel so the menu renders above the
        // game's Metal NSView. The mGBA / software libretro / external
        // branches below stay exactly as before.
        if (app.gameUsesHardwareRender()) {
            if (app.libretroOverlayMenuVisible)
                app.closeLibretroOverlayMenu();
            else
                app.openLibretroOverlayMenu();
            return;
        }

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

- [ ] **Step 2: Find the three RA Connections handlers**

Run:
```sh
grep -nE "onRaAchievementUnlocked|onRaInfoToast|onRaIndicator" "/Users/mark/Documents/Projects/RetroNest-Project/cpp/qml/AppUI/AppWindow.qml"
```

Expected: three matches, one per signal name, all inside `Connections { target: app ... }` blocks.

- [ ] **Step 3: Add early-return guards to each handler**

For each of the three handlers, add `if (app.gameUsesHardwareRender()) return;` as the **first statement inside the function body**. The current handlers look like:

```qml
    function onRaAchievementUnlocked(id, title, description, imageUrl) {
        achievementToast.show(title, description, imageUrl)
        // ... etc
    }
```

Become:

```qml
    function onRaAchievementUnlocked(id, title, description, imageUrl) {
        if (app.gameUsesHardwareRender()) return;
        achievementToast.show(title, description, imageUrl)
        // ... etc
    }
```

Apply the same one-line guard to `onRaInfoToast` and `onRaIndicator`. Leave the rest of each handler body untouched.

- [ ] **Step 4: Build and verify**

```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build -j 2>&1 | tail -10
```

Expected: clean build. QML changes don't trigger C++ recompilation, but they do trigger a resource regeneration step inside the `appui_backing` target.

Use timeout: 900000.

- [ ] **Step 5: Commit AppWindow.qml integration**

```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add cpp/qml/AppUI/AppWindow.qml
git commit -m "$(cat <<'EOF'
libretro: SP3.5 AppWindow.qml routes overlays via HW-render panel

Two surgical edits to AppWindow.qml so the new LibretroOverlayPanel
takes over for Pattern B HW-render libretro cores while every other
game path stays unchanged:

- toggleInGameMenu() checks app.gameUsesHardwareRender() at the top:
  if true, route through app.openLibretroOverlayMenu() /
  closeLibretroOverlayMenu() (which pauses / resumes the core +
  drives the panel's menu visibility). The existing libretro and
  external branches stay verbatim for mGBA / software / launched-
  binary sessions.

- onRaAchievementUnlocked / onRaInfoToast / onRaIndicator handlers
  early-return when app.gameUsesHardwareRender() is true, so the
  in-scene achievementToast / raIndicators don't double-fire
  alongside the overlay panel's Connections for the same signals.

mGBA / software-libretro / external-emulator paths use the same code
they did before SP3.5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: Smoke test 1 — PCSX2 in-game menu visible + functional

**Files:** none (verification + user interaction required).

- [ ] **Step 1: Relaunch with log capture**

```sh
pkill -x RetroNest 2>/dev/null; sleep 1
rm -f /tmp/retronest-sp35-test.log
"/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest" > /tmp/retronest-sp35-test.log 2>&1 &
disown
sleep 3
pgrep -lf RetroNest | head -2
```

Expected: one RetroNest PID printed.

- [ ] **Step 2: REQUIRES USER — launch + open menu**

Ask the user to:

1. Navigate to **Ratchet & Clank: Going Commando** in the library.
2. Click Launch.
3. Wait ~15 s for the BIOS / game screen to render (the SP3 verified state).
4. Press **Cmd+Shift+Escape**.
5. Confirm the in-game menu pill appears at the bottom-center, **visibly above the game**.
6. Navigate every menu action: Save State → confirm "Saved" pill top-right; Load State → "Loaded" pill; Fast Forward → "4×" sticky pill appears, press again to dismiss; Achievements → slide-up popup appears inside the menu pill; Resume — menu closes, game resumes.

- [ ] **Step 3: Tail the log + look for chrome / RA hooks firing**

```sh
grep -nE "LibretroOverlayPanel|registerHardwareView|globalHotkey|raInfoToast|libretroOverlayMenuVisible" /tmp/retronest-sp35-test.log | head -30
```

Expected lines (some):
```
[LibretroOverlayPanel] showForCurrentGame   (only if you add a log; optional)
[GameSession] registerHardwareView(0x...)   (SP3 path)
[AppController] Cmd+Shift+Esc fired → globalHotkeyPressed
```

- [ ] **Step 4: Common failure modes**

| Symptom | Likely cause | Fix |
|---|---|---|
| Cmd+Shift+Escape doesn't open anything | `app.gameUsesHardwareRender()` returns false. | Confirm `Pcsx2LibretroAdapter::prefersHardwareRender()` still returns true; confirm `GameSession::isLibretro()` is true for the running game. |
| Menu opens but invisible | NSWindow ordering wrong — child window didn't get attached. | Check the log for any warning from `LibretroOverlayPanel::showForCurrentGame`; verify `MacFullscreen::attachChildWindow` found the parent NSWindow (the loop in Task 5 may pick the wrong top-level window if there are multiple). Fall back to passing the main window in explicitly. |
| Menu opens above the game but controller input doesn't navigate it | `makePanelKey` didn't take, or `SdlInputManager` is still routing to emulation mode. | Verify `GameSession::pauseEmulation` was called inside `AppController::openLibretroOverlayMenu` (it should clear emulation mode via `m_sdlInputManager->clearEmulationMode()`). |
| Toasts (FF / save / load) don't show even though menu actions fire | `panelWindow` signal handlers in `LibretroOverlayPanel.qml` aren't calling `.show()`. | Re-check Task 7 — the `Connections { target: panelWindow ... }` block must call `saveToast.show()` etc. |

Iterate up to 5 times. STOP and report BLOCKED if not converging.

---

## Task 12: Smoke test 2 — RA overlays visible for PCSX2

**Files:** none (verification + user interaction).

- [ ] **Step 1: User launches the same Ratchet & Clank session**

If still running from Task 11, no relaunch needed — open the menu, pick Resume, and observe the next 30 seconds of gameplay.

- [ ] **Step 2: REQUIRES USER — observe RA launch banner**

Ask the user:

- Did an **"ACHIEVEMENTS ACTIVE — 0 / 199 achievements earned"** toast appear top-right within ~3 seconds of the BIOS handoff? It should fade after ~5 seconds.
- During gameplay, if RetroAchievements has any active progress/challenge indicators (varies per game state), do they appear bottom-left?
- Pressing Save State (via Cmd+Shift+Escape → menu → Save State): does the unlock-chime SFX play if the user has sound effects enabled in settings? (The chime is gated on the `app.raSoundEffects` flag.)

- [ ] **Step 3: Log check**

```sh
grep -nE "raInfoToast|achievement|rcheevos|raIndicator" /tmp/retronest-sp35-test.log | head -20
```

Expected: `[rcheevos] Game loaded; achievement session active. Title=...` should appear (verified in SP3 already). The QML side has no log unless you add one, but visual confirmation in Step 2 is the load-bearing test.

- [ ] **Step 4: Common failure modes**

| Symptom | Likely cause | Fix |
|---|---|---|
| Launch banner doesn't appear | The Connections in LibretroOverlayPanel.qml early-returns because `app.gameUsesHardwareRender()` evaluates false. | Sanity-check by temporarily removing the guard; confirm the toast appears. If it does, the accessor logic in Task 2 is wrong for live state. |
| Launch banner appears twice (once in main window, once in panel) | The `AppWindow.qml` early-return guard in Task 10 is missing or in the wrong place. | Re-grep `onRaInfoToast` in `AppWindow.qml`; first line of the function body must be the guard. |
| Banner appears but slightly mis-sized | Window geometry not matching primary screen. | Verify in Task 5 that `setGeometry(primary->geometry())` runs before `show()`. |

---

## Task 13: Smoke test 3 — mGBA path unchanged

**Files:** none (verification + user interaction).

- [ ] **Step 1: User launches a mGBA game**

Ask the user to launch any GBA game from RetroNest's library.

- [ ] **Step 2: Confirm software-path behaviour**

mGBA should run exactly as it does today:

- Game starts within a couple seconds.
- Video displays normally inside the same EmulationView.
- Cmd+Shift+Escape opens the in-scene bottom-center menu pill (it's the same pill widget, just inside `AppWindow.qml`, not the panel).
- Save State / Load State / Fast Forward each pop their pill toasts top-right (in-scene path).
- RA toasts (if applicable) render in-scene.
- "Quit" returns to the game list cleanly.

- [ ] **Step 3: Verify the floating panel did NOT activate**

```sh
grep -nE "LibretroOverlayPanel" /tmp/retronest-sp35-test.log
```

Expected: no logs about `LibretroOverlayPanel` (apart from possible class-constructor noise on startup which is benign — what shouldn't appear is `showForCurrentGame` running, since `gameUsesHardwareRender()` returns false for mGBA).

If mGBA's overlays look or behave any differently from before SP3.5, the early-return guards in Task 10 are likely guarding the wrong way (`return` on `gameUsesHardwareRender() == false` instead of `== true`). Re-check.

---

## Task 14: Regression sweep — external emulator unchanged

**Files:** none (verification + user interaction).

- [ ] **Step 1: User launches one external-binary emulator session**

Ask the user to launch either:

- The existing **launched-binary PCSX2** entry (separate from the new pcsx2-libretro entry), or
- **DuckStation** / **PPSSPP** / **Dolphin** for the relevant game.

- [ ] **Step 2: Confirm the external path is untouched**

- The existing `InGameMenuPanel` (compact 820×640 floating panel) opens on Cmd+Shift+Escape as it did before SP3.5.
- The new `LibretroOverlayPanel` does **not** appear (the running game isn't libretro).
- All existing menu actions work as before.
- Exit returns cleanly to the game list.

- [ ] **Step 3: Verify the new panel never spun up**

```sh
grep -nE "LibretroOverlayPanel::show|LibretroOverlayPanel::openMenu" /tmp/retronest-sp35-test.log
```

Expected: no matches. The launched-binary path never triggers the libretro overlay panel.

---

## Task 15: Mark SP3.5 complete in the spec + final commit

**Files:**
- Modify: `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-ux-overlays-design.md`

- [ ] **Step 1: Update spec status**

Open `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-ux-overlays-design.md`. Change:

```
**Status:** Approved (brainstorming)
```

to:

```
**Status:** Complete (verified end-to-end — see Verification log)
```

- [ ] **Step 2: Append verification log**

At the end of the file, append:

```markdown

## Verification log

SP3.5 phase completed (date). All success criteria verified:

- **Test 1 (PCSX2 in-game menu visible + functional):** PASSED — Cmd+Shift+Escape opens the bottom-center menu pill above the rendered game. Every menu action works; FF / save / load pills render top-right above Metal content. Resume returns to gameplay cleanly. Quit returns to game list.
- **Test 2 (PCSX2 RA overlays visible):** PASSED — "ACHIEVEMENTS ACTIVE — 0 / 199 achievements earned" toast appears top-right within ~3 s of BIOS handoff and fades after ~5 s. RA indicator chips (when applicable) render bottom-left.
- **Test 3 (mGBA unchanged):** PASSED — mGBA software-path libretro game behaves exactly as before SP3.5.
- **Regression sweep (external emulator unchanged):** PASSED — launched-binary path still uses `InGameMenuPanel` exclusively; new `LibretroOverlayPanel` not instantiated during external sessions.

SP3's deferred smoke tests 2 (clean exit) and 3 (mGBA unchanged) are subsumed by Task 11 / Task 13 here and can be marked verified.

### Observations / follow-ups uncovered during implementation

(Filled in at execution time — e.g. any layout tweaks needed, any chrome quirks observed.)

### Commits added during SP3.5

On RetroNest `main`:
- (Filled in at execution time — commit hashes for the three substantive + one docs commit.)
```

- [ ] **Step 3: Commit**

```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add docs/superpowers/specs/2026-05-11-pcsx2-libretro-ux-overlays-design.md
git commit -m "$(cat <<'EOF'
docs(specs): SP3.5 complete — Pattern B overlays verified end-to-end

PCSX2 libretro now has a working in-game menu, RA launch banner / unlock
toasts, FF / save / load pills, and indicator bar — all rendered above
the Metal NSView via the new LibretroOverlayPanel floating window. mGBA
software path and launched-binary external emulator path unchanged.

This also subsumes SP3's deferred smoke tests 2 (clean exit) and 3 (mGBA
unchanged), so SP3 closes formally alongside SP3.5.

Next: SP4 (audio output) wires SPU2 → retro_audio_sample_batch_t.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Plan self-review (post-write)

**Spec coverage:**

| Spec requirement | Implemented in |
|---|---|
| New `LibretroOverlayPanel.h` class declaration | Task 4 |
| New `LibretroOverlayPanel.cpp` implementation with `ensureCreated` / `show` / `hide` / `openMenu` / `closeMenu` | Task 5 |
| `LibretroOverlayPanel.qml` with seven overlays + RA Connections | Task 7 |
| `MacFullscreen::attachChildWindow` + `setIgnoresMouseEvents` helpers | Task 1 |
| `AppController::gameUsesHardwareRender()` accessor | Task 2 |
| `AppController::openLibretroOverlayMenu` / `closeLibretroOverlayMenu` invokables + `libretroOverlayMenuVisible` property | Task 9 |
| `AppController` lifecycle wiring (show on `gameStartingLibretro` if HW, hide on `gameFinished`) | Task 9 |
| `AppController` overlay-signal forwarders to `GameSession` | Task 9 |
| `AppWindow.qml` `toggleInGameMenu` branch on HW render | Task 10 |
| `AppWindow.qml` RA `Connections` early-return guards | Task 10 |
| CMakeLists.txt registration of new C++ files | Task 6 |
| Test 1 (PCSX2 menu visible + functional) | Task 11 |
| Test 2 (PCSX2 RA overlays visible) | Task 12 |
| Test 3 (mGBA unchanged) | Task 13 |
| Regression sweep (external emulator unchanged) | Task 14 |
| Spec status update | Task 15 |

All spec requirements covered.

**Placeholder scan:** Tasks 11-14 verification rely on "navigate every menu action" and "any GBA game" — these are intentionally user-context-dependent, not unfilled blanks. The Verification log section in Task 15 has `(Filled in at execution time)` placeholders for commit hashes / observation notes — those are template directives that get filled in when the task runs, not unfilled spec content. No other TBDs / TODOs / FIXMEs.

**Type / name consistency:**

- `LibretroOverlayPanel` class — Tasks 4, 5, 6, 7, 9, 10, 15.
- `showForCurrentGame` / `hide` / `openMenu` / `closeMenu` / `isMenuOpen` — declared in Task 4, defined in Task 5, called from Task 9.
- `gameUsesHardwareRender()` — declared/defined in Task 2, used in Tasks 7, 9, 10.
- `openLibretroOverlayMenu` / `closeLibretroOverlayMenu` / `libretroOverlayMenuVisible` — declared in Task 9, used in Task 10.
- `attachChildWindow` / `setIgnoresMouseEvents` — declared in Task 1, used in Task 5.
- The seven overlay item IDs in `LibretroOverlayPanel.qml` — `inGameMenu`, `ffToast`, `saveToast`, `loadToast`, `achievementToast`, `unlockSound`, `raIndicators` — all referenced consistently within Task 7's QML; matches the names used in `AppWindow.qml`'s in-scene versions.

**Bite-size check:** Largest single task is Task 9 (AppController integration with 5 sub-steps; longest sub-step is Step 3 with ~40 lines of new connection wiring). Each step is one logical change. Total 15 tasks is proportional to the scope: ~250 LOC new + 85 modified.

**Cross-repo coordination:** SP3.5 is entirely RetroNest-side. No `pcsx2-libretro` fork changes. SP3's previously-deployed fork dylib is unchanged.
