# Unified Controller Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace custom `inputManager` navigation signals with Qt key event injection so controller input works everywhere keyboard input works, with zero per-page wiring.

**Architecture:** SdlInputManager injects `QKeyEvent` objects into the focused QML window instead of emitting custom signals. All per-page `Connections { target: inputManager }` blocks are removed. Qt's focus system handles routing automatically. Binding capture mode stays unchanged.

**Tech Stack:** C++17, Qt6 (QGuiApplication::sendEvent, QKeyEvent), SDL2, QML

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `cpp/src/core/sdl_input_manager.h` | Modify | Remove 9 nav signals, add QWindow* + lastInputWasController |
| `cpp/src/core/sdl_input_manager.cpp` | Modify | Replace signal emissions with QKeyEvent injection |
| `cpp/src/main.cpp` | Modify | Pass QWindow* to SdlInputManager after QML loads |
| `cpp/src/ui/app_controller.h` | Modify | Remove settingsOpen + virtualKeyboardActive properties |
| `cpp/src/ui/app_controller.cpp` | Modify | Remove settingsOpen + virtualKeyboardActive implementations |
| `cpp/qml/AppUI/AppWindow.qml` | Modify | Remove inputManager Connections, remove settingsOpen binding, add F1 Shortcut |
| `cpp/qml/AppUI/SettingsOverlay.qml` | Modify | Remove both inputManager Connections blocks |
| `cpp/qml/AppUI/ScraperSettings.qml` | Modify | Remove inputManager Connections, use inputManager.lastInputWasController |
| `cpp/qml/AppUI/VirtualKeyboard.qml` | Modify | Remove inputManager Connections, consolidate into Keys.onPressed |
| `themes/modern/SystemPage.qml` | Modify | Remove inputManager Connections block |
| `themes/modern/GameListPage.qml` | Modify | Remove inputManager Connections block |

---

### Task 1: Modify SdlInputManager to inject Qt key events

**Files:**
- Modify: `cpp/src/core/sdl_input_manager.h`
- Modify: `cpp/src/core/sdl_input_manager.cpp`

- [ ] **Step 1: Update the header file**

Replace the entire contents of `cpp/src/core/sdl_input_manager.h` with:

```cpp
#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QMap>
#include <QWindow>
#include <QGuiApplication>
#include <QKeyEvent>
#include <SDL2/SDL.h>

/**
 * SdlInputManager — polls SDL2 for gamepad/keyboard events.
 * Injects Qt key events for navigation; provides "capture mode" for press-to-bind UI.
 */
class SdlInputManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool capturing READ isCapturing NOTIFY capturingChanged)
    Q_PROPERTY(QVariantList connectedControllers READ connectedControllers NOTIFY controllersChanged)
    Q_PROPERTY(bool lastInputWasController READ lastInputWasController NOTIFY lastInputWasControllerChanged)

public:
    explicit SdlInputManager(QObject* parent = nullptr);
    ~SdlInputManager();

    bool isCapturing() const;
    QVariantList connectedControllers() const;
    bool lastInputWasController() const;

    void setWindow(QWindow* window);

    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void cancelCapture();

    /** Convert SDL button/axis name to canonical name (e.g. "a" -> "A", "dpup" -> "DPadUp") */
    static QString canonicalName(const char* sdlName);

signals:
    void capturingChanged();
    void controllersChanged();
    void lastInputWasControllerChanged();
    void bindingCaptured(int deviceIndex, const QString& element, bool isAxis, bool positive);
    void keyboardCaptured(const QString& keyString);

private:
    void pollEvents();
    void injectKey(int qtKey, QEvent::Type type);
    void openController(int joystickIndex);
    void closeController(SDL_JoystickID instanceId);

    QTimer* m_pollTimer = nullptr;
    QWindow* m_window = nullptr;
    bool m_capturing = false;
    bool m_sdlInitialized = false;
    bool m_lastInputWasController = false;

    // instance ID -> controller
    QMap<SDL_JoystickID, SDL_GameController*> m_controllers;
    // instance ID -> device index (0-based)
    QMap<SDL_JoystickID, int> m_deviceIndices;
};
```

- [ ] **Step 2: Update the implementation file**

Replace the entire contents of `cpp/src/core/sdl_input_manager.cpp` with:

```cpp
#include "sdl_input_manager.h"
#include <QDebug>
#include <QVariantMap>
#include <cmath>

// Deadzone threshold for axis capture (out of 32767)
static constexpr int kAxisDeadzone = 16000;

// SDL button/axis name -> canonical name mapping
static const QMap<QString, QString>& canonicalMap() {
    static const QMap<QString, QString> map = {
        // Buttons
        {"a", "A"}, {"b", "B"}, {"x", "X"}, {"y", "Y"},
        {"back", "Back"}, {"guide", "Guide"}, {"start", "Start"},
        {"leftstick", "LeftStick"}, {"rightstick", "RightStick"},
        {"leftshoulder", "LeftShoulder"}, {"rightshoulder", "RightShoulder"},
        {"dpup", "DPadUp"}, {"dpdown", "DPadDown"},
        {"dpleft", "DPadLeft"}, {"dpright", "DPadRight"},
        // Axes
        {"leftx", "LeftX"}, {"lefty", "LeftY"},
        {"rightx", "RightX"}, {"righty", "RightY"},
        {"lefttrigger", "LeftTrigger"}, {"righttrigger", "RightTrigger"},
    };
    return map;
}

// SDL keycode -> key name for INI format
static QString keyName(SDL_Keycode key) {
    switch (key) {
    case SDLK_RETURN:    return "Return";
    case SDLK_ESCAPE:    return "Escape";
    case SDLK_BACKSPACE: return "Backspace";
    case SDLK_TAB:       return "Tab";
    case SDLK_SPACE:     return "Space";
    case SDLK_DELETE:    return "Delete";
    case SDLK_LSHIFT: case SDLK_RSHIFT: return "Shift";
    case SDLK_LCTRL:  case SDLK_RCTRL:  return "Control";
    case SDLK_LALT:   case SDLK_RALT:   return "Alt";
    case SDLK_PERIOD:    return "Period";
    case SDLK_COMMA:     return "Comma";
    case SDLK_SLASH:     return "Slash";
    default: break;
    }
    if (key >= SDLK_F1 && key <= SDLK_F12)
        return QString("F%1").arg(key - SDLK_F1 + 1);
    const char* name = SDL_GetKeyName(key);
    if (name && name[0] != '\0')
        return QString(name);
    return QString("Key%1").arg(key);
}

// Controller button -> Qt key mapping
static int mapButtonToKey(SDL_GameControllerButton btn) {
    switch (btn) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    return Qt::Key_Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return Qt::Key_Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return Qt::Key_Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Qt::Key_Right;
    case SDL_CONTROLLER_BUTTON_A:          return Qt::Key_Return;
    case SDL_CONTROLLER_BUTTON_B:          return Qt::Key_Escape;
    case SDL_CONTROLLER_BUTTON_X:          return Qt::Key_Backspace;
    case SDL_CONTROLLER_BUTTON_START:      return Qt::Key_F1;
    default: return 0;
    }
}

SdlInputManager::SdlInputManager(QObject* parent)
    : QObject(parent)
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        qWarning() << "[SDL] Failed to init GameController:" << SDL_GetError();
        return;
    }
    m_sdlInitialized = true;
    qInfo() << "[SDL] Initialized GameController subsystem";

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i))
            openController(i);
    }

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &SdlInputManager::pollEvents);
    m_pollTimer->start(16); // ~60 Hz
}

SdlInputManager::~SdlInputManager() {
    if (m_pollTimer) m_pollTimer->stop();
    for (auto* ctrl : m_controllers)
        SDL_GameControllerClose(ctrl);
    if (m_sdlInitialized)
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

bool SdlInputManager::isCapturing() const { return m_capturing; }

bool SdlInputManager::lastInputWasController() const { return m_lastInputWasController; }

void SdlInputManager::setWindow(QWindow* window) { m_window = window; }

QVariantList SdlInputManager::connectedControllers() const {
    QVariantList list;
    for (auto it = m_controllers.constBegin(); it != m_controllers.constEnd(); ++it) {
        QVariantMap entry;
        entry["instanceId"] = it.key();
        entry["deviceIndex"] = m_deviceIndices.value(it.key(), 0);
        entry["name"] = QString(SDL_GameControllerName(it.value()));
        list.append(entry);
    }
    return list;
}

void SdlInputManager::startCapture() {
    m_capturing = true;
    emit capturingChanged();
}

void SdlInputManager::cancelCapture() {
    m_capturing = false;
    emit capturingChanged();
}

QString SdlInputManager::canonicalName(const char* sdlName) {
    if (!sdlName) return {};
    return canonicalMap().value(QString(sdlName).toLower(), QString(sdlName));
}

void SdlInputManager::injectKey(int qtKey, QEvent::Type type) {
    if (!m_window) return;
    if (!m_lastInputWasController) {
        m_lastInputWasController = true;
        emit lastInputWasControllerChanged();
    }
    QKeyEvent event(type, qtKey, Qt::NoModifier);
    QGuiApplication::sendEvent(m_window, &event);
}

void SdlInputManager::openController(int joystickIndex) {
    SDL_GameController* ctrl = SDL_GameControllerOpen(joystickIndex);
    if (!ctrl) {
        qWarning() << "[SDL] Failed to open controller" << joystickIndex << ":" << SDL_GetError();
        return;
    }
    SDL_Joystick* joy = SDL_GameControllerGetJoystick(ctrl);
    SDL_JoystickID id = SDL_JoystickInstanceID(joy);
    m_controllers.insert(id, ctrl);
    m_deviceIndices.insert(id, m_deviceIndices.size());
    qInfo() << "[SDL] Opened controller:" << SDL_GameControllerName(ctrl)
            << "as device" << m_deviceIndices[id];
    emit controllersChanged();
}

void SdlInputManager::closeController(SDL_JoystickID instanceId) {
    if (auto* ctrl = m_controllers.value(instanceId, nullptr)) {
        qInfo() << "[SDL] Controller disconnected:" << SDL_GameControllerName(ctrl);
        SDL_GameControllerClose(ctrl);
        m_controllers.remove(instanceId);
        m_deviceIndices.remove(instanceId);
        emit controllersChanged();
    }
}

void SdlInputManager::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED:
            openController(event.cdevice.which);
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            closeController(event.cdevice.which);
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            if (m_capturing) {
                int devIdx = m_deviceIndices.value(event.cbutton.which, 0);
                const char* btnName = SDL_GameControllerGetStringForButton(
                    static_cast<SDL_GameControllerButton>(event.cbutton.button));
                QString element = canonicalName(btnName);
                m_capturing = false;
                emit capturingChanged();
                emit bindingCaptured(devIdx, element, false, true);
            } else {
                auto btn = static_cast<SDL_GameControllerButton>(event.cbutton.button);
                int qtKey = mapButtonToKey(btn);
                if (qtKey)
                    injectKey(qtKey, QEvent::KeyPress);
            }
            break;

        case SDL_CONTROLLERBUTTONUP: {
            auto btn = static_cast<SDL_GameControllerButton>(event.cbutton.button);
            int qtKey = mapButtonToKey(btn);
            if (qtKey && !m_capturing)
                injectKey(qtKey, QEvent::KeyRelease);
            break;
        }

        case SDL_CONTROLLERAXISMOTION: {
            int value = event.caxis.value;
            if (m_capturing && std::abs(value) > kAxisDeadzone) {
                int devIdx = m_deviceIndices.value(event.caxis.which, 0);
                const char* axisName = SDL_GameControllerGetStringForAxis(
                    static_cast<SDL_GameControllerAxis>(event.caxis.axis));
                QString element = canonicalName(axisName);
                bool positive = value > 0;
                m_capturing = false;
                emit capturingChanged();
                emit bindingCaptured(devIdx, element, true, positive);
            } else if (!m_capturing && std::abs(value) > kAxisDeadzone) {
                auto axis = static_cast<SDL_GameControllerAxis>(event.caxis.axis);
                SDL_JoystickID jid = event.caxis.which;

                static QMap<QPair<SDL_JoystickID, int>, bool> axisActive;
                auto key = qMakePair(jid, static_cast<int>(axis));

                if (!axisActive.value(key, false)) {
                    axisActive[key] = true;
                    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
                        injectKey(value > 0 ? Qt::Key_Right : Qt::Key_Left, QEvent::KeyPress);
                    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        injectKey(value > 0 ? Qt::Key_Down : Qt::Key_Up, QEvent::KeyPress);
                    } else if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                        injectKey(Qt::Key_Shift, QEvent::KeyPress);
                    }
                }
            } else if (!m_capturing && std::abs(value) <= kAxisDeadzone / 2) {
                auto axis = static_cast<SDL_GameControllerAxis>(event.caxis.axis);
                SDL_JoystickID jid = event.caxis.which;
                static QMap<QPair<SDL_JoystickID, int>, bool> axisActive;
                auto key = qMakePair(jid, static_cast<int>(axis));
                if (axisActive.value(key, false)) {
                    axisActive[key] = false;
                    // Send key release for the axis direction
                    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
                        injectKey(Qt::Key_Left, QEvent::KeyRelease);
                        injectKey(Qt::Key_Right, QEvent::KeyRelease);
                    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        injectKey(Qt::Key_Up, QEvent::KeyRelease);
                        injectKey(Qt::Key_Down, QEvent::KeyRelease);
                    } else if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                        injectKey(Qt::Key_Shift, QEvent::KeyRelease);
                    }
                }
            }
            break;
        }

        case SDL_KEYDOWN:
            // Mark that a physical keyboard was used (not controller)
            if (!m_capturing) {
                if (m_lastInputWasController) {
                    m_lastInputWasController = false;
                    emit lastInputWasControllerChanged();
                }
            }
            if (m_capturing) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
                    key == SDLK_LCTRL || key == SDLK_RCTRL ||
                    key == SDLK_LALT || key == SDLK_RALT) {
                    break;
                }
                if (key == SDLK_ESCAPE) {
                    cancelCapture();
                    break;
                }
                QString keyStr = keyName(key);
                SDL_Keymod mod = SDL_GetModState();
                QString full;
                if (mod & KMOD_ALT)
                    full = "Keyboard/Alt & Keyboard/" + keyStr;
                else if (mod & KMOD_SHIFT)
                    full = "Keyboard/Shift & Keyboard/" + keyStr;
                else if (mod & KMOD_CTRL)
                    full = "Keyboard/Control & Keyboard/" + keyStr;
                else
                    full = "Keyboard/" + keyStr;

                m_capturing = false;
                emit capturingChanged();
                emit keyboardCaptured(full);
            }
            break;

        default:
            break;
        }
    }
}
```

- [ ] **Step 3: Verify build compiles**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/sdl_input_manager.h cpp/src/core/sdl_input_manager.cpp
git commit -m "feat: replace inputManager nav signals with Qt key event injection"
```

---

### Task 2: Wire QWindow pointer in main.cpp

**Files:**
- Modify: `cpp/src/main.cpp`

- [ ] **Step 1: Pass the QWindow to SdlInputManager after QML engine loads**

In `cpp/src/main.cpp`, find the line after `engine.loadFromModule("AppUI", "AppWindow");` and the `rootObjects().isEmpty()` check. After the isEmpty check succeeds (meaning the window loaded), add:

```cpp
        // Pass the QML window to SdlInputManager for key event injection
        if (!engine.rootObjects().isEmpty()) {
            QWindow* window = qobject_cast<QWindow*>(engine.rootObjects().first());
            if (window)
                inputManager.setWindow(window);
        }
```

This goes right after the existing:
```cpp
        if (engine.rootObjects().isEmpty()) {
            qCritical() << "Failed to load QML app";
            db.close();
            return 1;
        }
```

- [ ] **Step 2: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/main.cpp
git commit -m "feat: pass QWindow to SdlInputManager for key injection"
```

---

### Task 3: Remove settingsOpen and virtualKeyboardActive from AppController

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Remove properties from header**

In `cpp/src/ui/app_controller.h`, remove these two Q_PROPERTY lines:
```cpp
    Q_PROPERTY(bool settingsOpen READ settingsOpen WRITE setSettingsOpen NOTIFY settingsOpenChanged)
    Q_PROPERTY(bool virtualKeyboardActive READ virtualKeyboardActive WRITE setVirtualKeyboardActive NOTIFY virtualKeyboardActiveChanged)
```

Remove these method declarations:
```cpp
    bool settingsOpen() const;
    void setSettingsOpen(bool open);
    bool virtualKeyboardActive() const;
    void setVirtualKeyboardActive(bool active);
```

Remove these signal declarations:
```cpp
    void settingsOpenChanged();
    void virtualKeyboardActiveChanged();
```

Remove these member variables:
```cpp
    bool m_settingsOpen = false;
    bool m_virtualKeyboardActive = false;
```

- [ ] **Step 2: Remove implementations from cpp**

In `cpp/src/ui/app_controller.cpp`, remove the four method implementations:
```cpp
bool AppController::settingsOpen() const { return m_settingsOpen; }
void AppController::setSettingsOpen(bool open) {
    if (m_settingsOpen != open) {
        m_settingsOpen = open;
        emit settingsOpenChanged();
    }
}

bool AppController::virtualKeyboardActive() const { return m_virtualKeyboardActive; }
void AppController::setVirtualKeyboardActive(bool active) {
    if (m_virtualKeyboardActive != active) {
        m_virtualKeyboardActive = active;
        emit virtualKeyboardActiveChanged();
    }
}
```

- [ ] **Step 3: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds (QML references to app.settingsOpen will fail at runtime but not at compile time).

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "refactor: remove settingsOpen and virtualKeyboardActive from AppController"
```

---

### Task 4: Update AppWindow.qml — remove inputManager Connections, add F1 Shortcut

**Files:**
- Modify: `cpp/qml/AppUI/AppWindow.qml`

- [ ] **Step 1: Remove the inputManager Connections block**

Remove this entire block (lines 107-125):
```qml
    // Controller Start button toggles settings overlay
    Connections {
        target: inputManager
        function onNavigateStart() {
            // Don't toggle settings when virtual keyboard is using Start for "Done"
            if (app.virtualKeyboardActive) return
            if (settingsOverlay.visible) {
                if (settingsOverlay.canGoBack()) {
                    settingsOverlay.goBack()
                } else {
                    settingsOverlay.close()
                    app.setCursorVisible(false)
                }
            } else {
                settingsOverlay.open()
                app.setCursorVisible(true)
            }
        }
    }
```

- [ ] **Step 2: Remove settingsOpen binding from SettingsOverlay**

Change:
```qml
    SettingsOverlay {
        id: settingsOverlay
        onVisibleChanged: app.settingsOpen = visible
    }
```
To:
```qml
    SettingsOverlay {
        id: settingsOverlay
    }
```

- [ ] **Step 3: Replace Escape Shortcut with Keys.onPressed + add F1 Shortcut**

Qt Shortcuts fire BEFORE Keys.onPressed, which means the Escape Shortcut would intercept B-button (Escape) before the VirtualKeyboard can handle it. Fix: replace the Escape Shortcut with a `Keys.onPressed` handler on the root ApplicationWindow, which respects QML focus chain — VirtualKeyboard handles Escape first if it has focus.

Remove the existing Escape Shortcut block:
```qml
    // Escape key toggles settings overlay
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (settingsOverlay.visible) {
                if (settingsOverlay.canGoBack()) {
                    settingsOverlay.goBack()
                } else {
                    settingsOverlay.close()
                    app.setCursorVisible(false)
                }
            } else {
                settingsOverlay.open()
                app.setCursorVisible(true)
            }
        }
    }
```

Add F1 Shortcut (no conflict since nothing else uses F1) and a `Keys.onPressed` handler for Escape on the root ApplicationWindow. Place after the StatusBar block:

```qml
    // F1 key (mapped from controller Start button) toggles settings
    Shortcut {
        sequence: "F1"
        onActivated: {
            if (settingsOverlay.visible) {
                settingsOverlay.close()
                app.setCursorVisible(false)
            } else {
                settingsOverlay.open()
                app.setCursorVisible(true)
            }
        }
    }

    // Escape / B-button: toggle settings or go back
    // Uses Keys.onPressed (not Shortcut) so VirtualKeyboard gets Escape first via focus chain
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            if (settingsOverlay.visible) {
                if (settingsOverlay.canGoBack()) {
                    settingsOverlay.goBack()
                } else {
                    settingsOverlay.close()
                    app.setCursorVisible(false)
                }
            } else {
                settingsOverlay.open()
                app.setCursorVisible(true)
            }
            event.accepted = true
        }
    }
```

- [ ] **Step 4: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/AppWindow.qml
git commit -m "refactor: replace inputManager Connections with F1 Shortcut in AppWindow"
```

---

### Task 5: Update SettingsOverlay.qml — remove inputManager Connections

**Files:**
- Modify: `cpp/qml/AppUI/SettingsOverlay.qml`

- [ ] **Step 1: Remove the B-button Connections block**

Remove this entire block (around lines 68-81):
```qml
    // Controller B-button: go back or close settings
    Connections {
        target: inputManager
        enabled: overlay.visible && !app.virtualKeyboardActive

        function onNavigateBack() {
            if (overlay.canGoBack()) {
                overlay.goBack()
            } else {
                overlay.close()
                app.setCursorVisible(false)
            }
        }
    }
```

- [ ] **Step 2: Remove the category list controller Connections block**

In the categoryListComponent (around lines 286-299), remove:
```qml
            // Controller navigation for category list
            Connections {
                target: inputManager
                enabled: overlay.visible && overlay.selectedCategory === -1

                function onNavigateUp() {
                    focusIndex = (focusIndex - 1 + 4) % 4
                }
                function onNavigateDown() {
                    focusIndex = (focusIndex + 1) % 4
                }
                function onNavigateAccept() {
                    selectCategory(focusIndex)
                }
            }
```

- [ ] **Step 3: Add Escape key handling to category list for B-button back**

The category list already has `Keys.onReturnPressed` and `Keys.onEnterPressed`. Add Escape handling so B-button (which injects Escape) works. After the existing `Keys.onEnterPressed` line, add:

```qml
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape) {
                    overlay.close()
                    app.setCursorVisible(false)
                    event.accepted = true
                }
            }
```

- [ ] **Step 4: Add Escape key handling to SettingsOverlay for sub-page B-button back**

The SettingsOverlay's `panelStack` needs to handle Escape when a sub-page is active (depth > 1). Add a `Keys.onPressed` handler to the panelStack's `onCurrentItemChanged` — actually, the simplest approach is to add it to each pushed page. But since we can't modify the page components easily, we handle it at the SettingsOverlay level.

Add a focus-scoped key handler on the panel StackView. Find the `StackView { id: panelStack` block and add after the `popExit` transition:

```qml
                // B-button (Escape) goes back from sub-pages
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Escape && overlay.canGoBack()) {
                        overlay.goBack()
                        event.accepted = true
                    }
                }
```

- [ ] **Step 5: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/AppUI/SettingsOverlay.qml
git commit -m "refactor: replace inputManager Connections with Keys handlers in SettingsOverlay"
```

---

### Task 6: Update VirtualKeyboard.qml — consolidate into Keys.onPressed

**Files:**
- Modify: `cpp/qml/AppUI/VirtualKeyboard.qml`

- [ ] **Step 1: Remove the inputManager Connections block**

Remove this entire block (lines 117-156):
```qml
    // ── Controller navigation via inputManager ──
    Connections {
        target: inputManager
        enabled: root.visible

        function onNavigateUp() { ... }
        function onNavigateDown() { ... }
        function onNavigateLeft() { ... }
        function onNavigateRight() { ... }
        function onNavigateAccept() { ... }
        function onNavigateBack() { ... }
        function onNavigateDelete() { ... }
        function onNavigateShift() { ... }
        function onNavigateStart() { ... }
    }
```

- [ ] **Step 2: Remove the app.virtualKeyboardActive lines**

In the `open()` function, remove:
```qml
        app.virtualKeyboardActive = true
```

In the `close()` function, remove:
```qml
        app.virtualKeyboardActive = false
```

- [ ] **Step 3: Replace the Keys.onPressed handler with unified version**

Replace the existing `Keys.onPressed` handler (lines 158-178) with:

```qml
    // ── All input (keyboard + controller) via Keys.onPressed ──
    Keys.onPressed: function(event) {
        if (!visible) return

        if (event.key === Qt.Key_Up) {
            if (focusRow > 0) focusRow--
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            if (focusRow < currentRows.length - 1) focusRow++
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            if (focusCol > 0) focusCol--
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            var maxCol = (focusRow === currentRows.length - 1)
                ? bottomRowCells - 1
                : currentRows[focusRow].length - 1
            if (focusCol < maxCol) focusCol++
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            var row = currentRows[focusRow]
            if (row) handleKeyPress(row[focusCol])
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            text = _initialText
            cancelled()
            close()
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            doBackspace()
            event.accepted = true
        } else if (event.key === Qt.Key_Shift) {
            shifted = !shifted
            event.accepted = true
        } else if (event.key === Qt.Key_F1) {
            accepted()
            close()
            event.accepted = true
        } else if (event.text.length > 0 && event.text.charCodeAt(0) >= 32) {
            text += event.text
            event.accepted = true
        }
    }
```

- [ ] **Step 4: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/VirtualKeyboard.qml
git commit -m "refactor: consolidate VirtualKeyboard input into Keys.onPressed"
```

---

### Task 7: Update ScraperSettings.qml — remove inputManager Connections, use centralized lastInputWasController

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml`

- [ ] **Step 1: Remove the login inputManager Connections block**

Remove this entire block (around lines 31-48):
```qml
    // Track input source + login screen controller navigation
    Connections {
        target: inputManager
        enabled: root.screenState === "login" && !virtualKeyboard.visible

        function onNavigateUp() {
            root.lastInputWasController = true
            root.loginFocusIndex = (root.loginFocusIndex - 1 + 3) % 3
        }
        function onNavigateDown() {
            root.lastInputWasController = true
            root.loginFocusIndex = (root.loginFocusIndex + 1) % 3
        }
        function onNavigateAccept() {
            root.lastInputWasController = true
            root.activateLoginFocused()
        }
    }
```

- [ ] **Step 2: Remove the dashboard account editing inputManager Connections block**

Remove this entire block (around lines 66-80):
```qml
    // Controller navigation for dashboard account editing
    Connections {
        target: inputManager
        enabled: root.screenState === "dashboard" && root.accountEditing && !virtualKeyboard.visible

        function onNavigateAccept() {
            root.lastInputWasController = true
            if (acctUserField.activeFocus) {
                virtualKeyboard.open(acctUserField.text, false, "USERNAME")
            } else if (acctPassField.activeFocus) {
                virtualKeyboard.open(acctPassField.text, true, "PASSWORD")
            }
        }
    }
```

- [ ] **Step 3: Remove the local lastInputWasController property**

Remove this line:
```qml
    property bool lastInputWasController: false
```

- [ ] **Step 4: Update Keys.onPressed to not set lastInputWasController**

In the `Keys.onPressed` handler, remove the line:
```qml
            root.lastInputWasController = false
```

The `lastInputWasController` flag is now managed centrally by `inputManager`.

- [ ] **Step 5: Update activateLoginFocused to use inputManager.lastInputWasController**

In the `activateLoginFocused()` function, change both occurrences of `lastInputWasController` to `inputManager.lastInputWasController`:

```qml
    function activateLoginFocused() {
        if (loginFocusIndex === 0) {
            if (inputManager.lastInputWasController) {
                virtualKeyboard.open(loginUserField.text, false, "USERNAME")
            } else {
                loginUserField.forceActiveFocus()
            }
        } else if (loginFocusIndex === 1) {
            if (inputManager.lastInputWasController) {
                virtualKeyboard.open(loginPassField.text, true, "PASSWORD")
            } else {
                loginPassField.forceActiveFocus()
            }
        } else if (loginFocusIndex === 2) {
            if (signInBtn.enabled) {
                signInBtn.enabled = false
                loginError.visible = false
                app.validateScraperCredentials(loginUserField.text, loginPassField.text)
            }
        }
    }
```

- [ ] **Step 6: Update virtualKeyboard.visible guards**

In the focus recovery Connections, change `!virtualKeyboard.visible` to just check without the old flag:
The Connections for `loginUserField` and `loginPassField` reference `virtualKeyboard.visible` which is still valid — keep those as-is.

- [ ] **Step 7: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 8: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "refactor: remove inputManager Connections from ScraperSettings, use centralized lastInputWasController"
```

---

### Task 8: Update theme pages — remove inputManager Connections

**Files:**
- Modify: `themes/modern/SystemPage.qml`
- Modify: `themes/modern/GameListPage.qml`

- [ ] **Step 1: Remove inputManager Connections from SystemPage.qml**

Remove this entire block (lines 40-50):
```qml
    // Controller navigation
    Connections {
        target: inputManager
        enabled: !app.settingsOpen
        function onNavigateLeft()   { root.carouselPrev() }
        function onNavigateRight()  { root.carouselNext() }
        function onNavigateAccept() {
            if (systemList.length > 0)
                themeContext.navigateToSystem(systemList[root.carouselIndex])
        }
    }
```

The existing `Keys.onLeftPressed`, `Keys.onRightPressed`, `Keys.onReturnPressed`, and `Keys.onEnterPressed` handlers already cover all the same navigation. Controller input now arrives as key events.

- [ ] **Step 2: Remove inputManager Connections from GameListPage.qml**

Remove this entire block (lines 65-82):
```qml
    // ── Controller navigation ──
    Connections {
        target: inputManager
        enabled: !app.settingsOpen
        function onNavigateUp() {
            if (root.listIndex > 0) { root.listIndex--; root.selectCurrentGame() }
        }
        function onNavigateDown() {
            if (root.listIndex < gameModel.count - 1) { root.listIndex++; root.selectCurrentGame() }
        }
        function onNavigateAccept() {
            if (root.selectedDetails && root.selectedDetails.id !== undefined)
                themeContext.launchGame(root.selectedDetails.id,
                                        root.selectedDetails.romPath,
                                        root.selectedDetails.emulatorId)
        }
        function onNavigateBack() { themeContext.navigateBack() }
    }
```

The existing `Keys.onUpPressed`, `Keys.onDownPressed`, `Keys.onReturnPressed`, `Keys.onEnterPressed`, and `Keys.onPressed` (for Backspace→navigateBack) already cover all navigation. Note: B-button now injects Escape, and the existing `Keys.onPressed` handles Backspace for back navigation. Add Escape handling for B-button:

After the existing `Keys.onPressed` handler, change:
```qml
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Backspace) { themeContext.navigateBack(); event.accepted = true }
    }
```
To:
```qml
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Escape) {
            themeContext.navigateBack()
            event.accepted = true
        }
    }
```

- [ ] **Step 3: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 4: Run the app and test**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

Test:
1. Controller D-pad navigates system carousel (SystemPage)
2. Controller A-button selects a system
3. Controller D-pad navigates game list (GameListPage)
4. Controller B-button goes back to system carousel
5. Controller Start opens settings
6. Controller D-pad navigates settings categories
7. Controller A-button enters a category
8. Controller B-button goes back from category to list
9. Controller B-button from category list closes settings
10. Navigate to Scraper login, D-pad between fields, A-button opens virtual keyboard (controller) or focuses field (keyboard)
11. Virtual keyboard: D-pad navigates grid, A types, B cancels, X backspace, R2 shift, Start done

- [ ] **Step 5: Commit**

```bash
git add themes/modern/SystemPage.qml themes/modern/GameListPage.qml
git commit -m "refactor: remove inputManager Connections from theme pages — controller uses key injection"
```
