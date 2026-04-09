# Dynamic Button Hints Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add contextual button hint glyphs that dynamically switch between Xbox, PlayStation, and keyboard labels based on input device, shown on all navigable pages.

**Architecture:** A `controllerType` enum on `SdlInputManager` detects Xbox vs PlayStation from `SDL_GameControllerName()`. A new `ButtonHints.qml` component maps action IDs to styled glyphs per input mode. The component is placed in `AppWindow.qml` (floating over theme pages), `SettingsOverlay.qml` (replacing `ControllerHints`), and `GameActionPopup.qml`.

**Tech Stack:** C++17, Qt6 QML, SDL2

**Spec:** `docs/superpowers/specs/2026-04-02-dynamic-button-hints-design.md`

---

### Task 1: Add controllerType to SdlInputManager

**Files:**
- Modify: `cpp/src/core/sdl_input_manager.h`
- Modify: `cpp/src/core/sdl_input_manager.cpp`

- [ ] **Step 1: Add the ControllerType enum, property, and member variables to the header**

In `cpp/src/core/sdl_input_manager.h`, add the enum before the class definition won't work since it needs Q_ENUM — add it inside the class. Add these members:

```cpp
// In the public section, after Q_PROPERTY(bool virtualKeyboardOpen ...):
public:
    enum ControllerType { Keyboard, Xbox, PlayStation };
    Q_ENUM(ControllerType)

// Add Q_PROPERTY after the virtualKeyboardOpen property:
    Q_PROPERTY(int controllerType READ controllerType NOTIFY controllerTypeChanged)

// Add public method:
    int controllerType() const;

// Add signal:
    void controllerTypeChanged();

// Add private members:
    ControllerType m_activeControllerType = Xbox;
    QMap<SDL_JoystickID, ControllerType> m_controllerTypes;
```

The full header changes:

After line 21 (`Q_PROPERTY(bool virtualKeyboardOpen ...)`), add:
```cpp
    Q_PROPERTY(int controllerType READ controllerType NOTIFY controllerTypeChanged)
```

After line 24 (`explicit SdlInputManager ...`), within the public section, add:
```cpp
    enum ControllerType { Keyboard, Xbox, PlayStation };
    Q_ENUM(ControllerType)
```

After line 29 (`void setVirtualKeyboardOpen ...`), add:
```cpp
    int controllerType() const;
```

After line 45 (`void keyboardCaptured ...`), add:
```cpp
    void controllerTypeChanged();
```

After line 66 (`QMap<SDL_JoystickID, int> m_deviceIndices`), add:
```cpp
    ControllerType m_activeControllerType = Xbox;
    QMap<SDL_JoystickID, ControllerType> m_controllerTypes;
```

- [ ] **Step 2: Implement controller type detection in the .cpp file**

In `cpp/src/core/sdl_input_manager.cpp`, add the detection helper function after the `mapButtonToKey` function (after line 66):

```cpp
static SdlInputManager::ControllerType detectControllerType(const char* name) {
    if (!name) return SdlInputManager::Xbox;
    QString lower = QString(name).toLower();
    if (lower.contains("ps3") || lower.contains("ps4") || lower.contains("ps5") ||
        lower.contains("dualshock") || lower.contains("dualsense") ||
        lower.contains("playstation")) {
        return SdlInputManager::PlayStation;
    }
    return SdlInputManager::Xbox;
}
```

Add the `controllerType()` getter after the `setVirtualKeyboardOpen` method (after line 106):

```cpp
int SdlInputManager::controllerType() const {
    if (!m_lastInputWasController) return Keyboard;
    return static_cast<int>(m_activeControllerType);
}
```

- [ ] **Step 3: Store detected type when controller connects**

In `openController()`, after line 156 (`m_deviceIndices.insert(id, m_deviceIndices.size())`), add:

```cpp
    m_controllerTypes.insert(id, detectControllerType(SDL_GameControllerName(ctrl)));
```

In `closeController()`, after line 167 (`m_deviceIndices.remove(instanceId)`), add:

```cpp
    m_controllerTypes.remove(instanceId);
```

- [ ] **Step 4: Update active type on button press and emit changes**

In `pollEvents()`, in the `SDL_CONTROLLERBUTTONDOWN` case, in the non-capturing branch (around line 194), before the `mapButtonToKey` call, add:

```cpp
                auto type = m_controllerTypes.value(event.cbutton.which, Xbox);
                if (type != m_activeControllerType || !m_lastInputWasController) {
                    m_activeControllerType = type;
                    emit controllerTypeChanged();
                }
```

Also in the `SDL_CONTROLLERAXISMOTION` case, inside the `!m_capturing && std::abs(value) > kAxisDeadzone` branch (around line 225), when `!m_axisActive.value(key, false)`, before the axis-to-key mapping, add:

```cpp
                    auto type = m_controllerTypes.value(event.caxis.which, Xbox);
                    if (type != m_activeControllerType || !m_lastInputWasController) {
                        m_activeControllerType = type;
                        emit controllerTypeChanged();
                    }
```

In the `SDL_KEYDOWN` case (around line 260), inside the `if (m_lastInputWasController)` block, also emit `controllerTypeChanged()`:

```cpp
                if (m_lastInputWasController) {
                    m_lastInputWasController = false;
                    emit lastInputWasControllerChanged();
                    emit controllerTypeChanged();
                }
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Builds successfully with no errors.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/sdl_input_manager.h cpp/src/core/sdl_input_manager.cpp
git commit -m "feat: add controllerType detection to SdlInputManager (Xbox/PlayStation/Keyboard)"
```

---

### Task 2: Create ButtonHints.qml component

**Files:**
- Create: `cpp/qml/AppUI/ButtonHints.qml`
- Modify: `cpp/CMakeLists.txt:157` (add to QML_FILES)

- [ ] **Step 1: Create the ButtonHints.qml component**

Create `cpp/qml/AppUI/ButtonHints.qml`:

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    height: 30
    width: hintsRow.width

    // Pass hints as array of {action: "confirm", label: "Select"} objects
    // Valid actions: confirm, back, action, delete, navigate_lr, navigate_ud, start
    property var hints: []

    // 0 = Keyboard, 1 = Xbox, 2 = PlayStation
    property int inputType: inputManager.controllerType

    // Glyph definitions per action per input type
    // Each entry: { text, bg, fg }
    function glyphFor(action) {
        if (inputType === 2) {
            // PlayStation
            switch (action) {
            case "confirm":     return { text: "\u2715", bg: "#2a3a6a", fg: "#6d9ddc", border: "#3a5a8a" }
            case "back":        return { text: "\u25CB", bg: "#5c2a3a", fg: "#dc6d8d", border: "#7a3a5a" }
            case "action":      return { text: "\u25B3", bg: "#2a5c5c", fg: "#6ddcb0", border: "#3a7a6a" }
            case "delete":      return { text: "\u25A1", bg: "#5c2a5c", fg: "#dc6ddc", border: "#7a3a7a" }
            case "navigate_lr": return { text: "D-Pad \u25C2\u25B8", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "navigate_ud": return { text: "D-Pad \u25B4\u25BE", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "start":       return { text: "Start", bg: "#333333", fg: "#cccccc", border: "#555555" }
            default:            return { text: "?", bg: "#333333", fg: "#cccccc", border: "#555555" }
            }
        } else if (inputType === 1) {
            // Xbox
            switch (action) {
            case "confirm":     return { text: "A", bg: "#2a5c2a", fg: "#6ddc6d", border: "#3a7a3a" }
            case "back":        return { text: "B", bg: "#5c2a2a", fg: "#dc6d6d", border: "#7a3a3a" }
            case "action":      return { text: "Y", bg: "#5c5c2a", fg: "#dcdc6d", border: "#7a7a3a" }
            case "delete":      return { text: "X", bg: "#2a3a6a", fg: "#6d9ddc", border: "#3a5a8a" }
            case "navigate_lr": return { text: "D-Pad \u25C2\u25B8", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "navigate_ud": return { text: "D-Pad \u25B4\u25BE", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "start":       return { text: "Start", bg: "#333333", fg: "#cccccc", border: "#555555" }
            default:            return { text: "?", bg: "#333333", fg: "#cccccc", border: "#555555" }
            }
        } else {
            // Keyboard
            switch (action) {
            case "confirm":     return { text: "Enter", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "back":        return { text: "Esc", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "action":      return { text: "M", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "delete":      return { text: "Backspace", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "navigate_lr": return { text: "\u2190\u2192", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "navigate_ud": return { text: "\u2191\u2193", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "start":       return { text: "Esc", bg: "#333333", fg: "#cccccc", border: "#555555" }
            default:            return { text: "?", bg: "#333333", fg: "#cccccc", border: "#555555" }
            }
        }
    }

    Row {
        id: hintsRow
        anchors.centerIn: parent
        spacing: 20

        Repeater {
            model: root.hints

            Row {
                spacing: 5

                Rectangle {
                    width: glyphText.implicitWidth + 12
                    height: 22
                    radius: 4
                    color: root.glyphFor(modelData.action).bg
                    border.color: root.glyphFor(modelData.action).border
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: glyphText
                        anchors.centerIn: parent
                        text: root.glyphFor(modelData.action).text
                        color: root.glyphFor(modelData.action).fg
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }
                }

                Text {
                    text: modelData.label
                    color: "#dddddd"
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    anchors.verticalCenter: parent.verticalCenter
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.5)
                }
            }
        }
    }
}
```

- [ ] **Step 2: Add ButtonHints.qml to CMakeLists.txt**

In `cpp/CMakeLists.txt`, at line 157 (the `QML_FILES` list for `appui_backing`), replace `qml/AppUI/ControllerHints.qml` with `qml/AppUI/ButtonHints.qml`:

Replace:
```
        qml/AppUI/ControllerHints.qml
```
With:
```
        qml/AppUI/ButtonHints.qml
```

- [ ] **Step 3: Delete ControllerHints.qml**

```bash
rm cpp/qml/AppUI/ControllerHints.qml
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Build fails — `SettingsOverlay.qml` still references `ControllerHints`. That's expected; we fix it in Task 3.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/ButtonHints.qml cpp/CMakeLists.txt
git rm cpp/qml/AppUI/ControllerHints.qml
git commit -m "feat: add ButtonHints.qml component, remove ControllerHints.qml"
```

---

### Task 3: Integrate ButtonHints into SettingsOverlay

**Files:**
- Modify: `cpp/qml/AppUI/SettingsOverlay.qml:282-288`

- [ ] **Step 1: Replace ControllerHints with ButtonHints in SettingsOverlay**

In `cpp/qml/AppUI/SettingsOverlay.qml`, replace lines 282-288:

```qml
            // Controller hints
            ControllerHints {
                Layout.fillWidth: true
                hints: panelStack.depth > 1
                    ? [{icon: "\u2195", text: "Navigate"}, {icon: "\uD83C\uDD70", text: "Select"}, {icon: "\uD83C\uDD71", text: "Back"}]
                    : [{icon: "\u2195", text: "Navigate"}, {icon: "\uD83C\uDD70", text: "Select"}, {icon: "\uD83C\uDD71", text: "Close"}]
            }
```

With:

```qml
            // Button hints
            ButtonHints {
                Layout.fillWidth: true
                anchors.horizontalCenter: parent.horizontalCenter
                hints: {
                    var current = panelStack.currentItem
                    // Scrape progress page has special hints
                    if (current && current.scrapeRunning !== undefined) {
                        if (current.scrapeRunning)
                            return [{action: "back", label: "Stop"}]
                        if (current.progressTotal > 0)
                            return [{action: "confirm", label: "Done"}, {action: "back", label: "Back"}]
                    }
                    // Default: category list vs sub-page
                    if (panelStack.depth > 1)
                        return [{action: "navigate_ud", label: "Navigate"}, {action: "confirm", label: "Select"}, {action: "back", label: "Back"}]
                    return [{action: "navigate_ud", label: "Navigate"}, {action: "confirm", label: "Select"}, {action: "back", label: "Close"}]
                }
            }
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Builds successfully.

- [ ] **Step 3: Run the app and verify settings overlay hints**

Run: `./build/EmulatorFrontend`

Verify:
1. Press Escape to open settings — hints show at bottom of panel with keyboard labels (↑↓ Navigate, Enter Select, Esc Close)
2. Select a category — hints change to (↑↓ Navigate, Enter Select, Esc Back)
3. If a controller is connected, verify hints switch to Xbox or PlayStation glyphs

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/SettingsOverlay.qml
git commit -m "feat: replace ControllerHints with ButtonHints in SettingsOverlay"
```

---

### Task 4: Add ButtonHints to AppWindow (theme pages)

**Files:**
- Modify: `cpp/qml/AppUI/AppWindow.qml`

- [ ] **Step 1: Add floating ButtonHints to AppWindow**

In `cpp/qml/AppUI/AppWindow.qml`, after the `StatusBar` block (after line 152), add:

```qml
    // Floating button hints (above theme content, below overlays)
    ButtonHints {
        id: mainHints
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        z: 50
        visible: !settingsOverlay.visible && !gameActionPopup.visible
        hints: {
            if (window.showingEmptyState)
                return [{action: "start", label: "Settings"}]
            if (mainStack.depth > 1)
                return [{action: "navigate_ud", label: "Browse"}, {action: "confirm", label: "Launch"}, {action: "action", label: "Actions"}, {action: "back", label: "Back"}, {action: "start", label: "Settings"}]
            return [{action: "navigate_lr", label: "Browse"}, {action: "confirm", label: "Select"}, {action: "start", label: "Settings"}]
        }
    }
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Builds successfully.

- [ ] **Step 3: Run the app and verify theme page hints**

Run: `./build/EmulatorFrontend`

Verify:
1. System page shows: ←→ Browse, Enter Select, Esc Settings (keyboard mode)
2. Navigate into a system — game list shows: ↑↓ Browse, Enter Launch, M Actions, Esc Back, Esc Settings
3. Open settings overlay — floating hints disappear
4. Close settings — floating hints reappear
5. Open game action popup — floating hints disappear

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/AppWindow.qml
git commit -m "feat: add floating ButtonHints to AppWindow for theme pages"
```

---

### Task 5: Add ButtonHints to GameActionPopup

**Files:**
- Modify: `cpp/qml/AppUI/GameActionPopup.qml`

- [ ] **Step 1: Add ButtonHints inside the popup card**

In `cpp/qml/AppUI/GameActionPopup.qml`, after the `confirmColumn` closing brace (after line 177, inside the `card` Rectangle), add:

```qml
        // Button hints at bottom of card
        ButtonHints {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -36
            anchors.horizontalCenter: parent.horizontalCenter
            hints: popupState === "confirm"
                ? [{action: "confirm", label: "Select"}, {action: "back", label: "Cancel"}]
                : [{action: "navigate_ud", label: "Navigate"}, {action: "confirm", label: "Select"}, {action: "back", label: "Close"}]
        }
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Builds successfully.

- [ ] **Step 3: Run the app and verify popup hints**

Run: `./build/EmulatorFrontend`

Verify:
1. Navigate to a game, press M to open game actions — hints show below the popup card (↑↓ Navigate, Enter Select, Esc Close)
2. Select "Remove from Library" — hints change to (Enter Select, Esc Cancel)
3. Press Escape to cancel — hints revert to action list hints

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/GameActionPopup.qml
git commit -m "feat: add ButtonHints to GameActionPopup"
```

