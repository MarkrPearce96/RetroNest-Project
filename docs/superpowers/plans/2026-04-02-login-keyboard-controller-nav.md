# Login Keyboard & Controller Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add keyboard/gamepad navigation to the ScraperSettings login page and build a reusable virtual keyboard overlay for controller-only text input.

**Architecture:** Pure QML approach using the existing `focusIndex` pattern from the dashboard, `inputManager` signals for gamepad input, and `FocusableItem`/`SettingsTheme` for consistent focus styling. A new `VirtualKeyboard.qml` component provides a popup QWERTY overlay that auto-opens for controller users and passes through physical keyboard input.

**Tech Stack:** QML (Qt6), SDL2 gamepad input via existing `SdlInputManager` C++ class.

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `cpp/qml/AppUI/VirtualKeyboard.qml` | Create | Reusable popup QWERTY overlay — grid navigation, physical keyboard passthrough, password masking with show/hide toggle |
| `cpp/qml/AppUI/ScraperSettings.qml` | Modify | Add login form focus navigation (focusIndex), input source detection, VirtualKeyboard integration |
| `cpp/CMakeLists.txt` | Modify | Register `VirtualKeyboard.qml` in the AppUI QML module |

---

### Task 1: Register VirtualKeyboard.qml in CMakeLists.txt

**Files:**
- Modify: `cpp/CMakeLists.txt:153-176` (AppUI QML module)

- [ ] **Step 1: Add VirtualKeyboard.qml to the QML_FILES list**

In `cpp/CMakeLists.txt`, add the new file to the AppUI module's `QML_FILES` list. Insert it after `ScraperSettings.qml` (line 171):

```cmake
        qml/AppUI/ScraperSettings.qml
        qml/AppUI/VirtualKeyboard.qml
        qml/AppUI/ThemesPage.qml
```

- [ ] **Step 2: Create an empty VirtualKeyboard.qml placeholder**

Create `cpp/qml/AppUI/VirtualKeyboard.qml` with a minimal stub so the build doesn't break:

```qml
import QtQuick

Item {
    id: root
    visible: false
}
```

- [ ] **Step 3: Verify build compiles**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add cpp/CMakeLists.txt cpp/qml/AppUI/VirtualKeyboard.qml
git commit -m "feat: register VirtualKeyboard.qml stub in AppUI module"
```

---

### Task 2: Build VirtualKeyboard.qml — Layout and Keyboard Grid

**Files:**
- Create: `cpp/qml/AppUI/VirtualKeyboard.qml`

- [ ] **Step 1: Write the full VirtualKeyboard.qml component**

Replace the stub with the complete component. This is a large file so it's written in one step:

```qml
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    anchors.fill: parent
    color: "#CC000000"
    visible: false
    z: 200

    // ── Public API ──
    property string text: ""
    property string label: ""
    property bool isPassword: false
    property bool showPassword: false

    signal accepted()
    signal cancelled()

    property string _initialText: ""

    function open(initialText, passwordMode, fieldLabel) {
        _initialText = initialText
        text = initialText
        isPassword = passwordMode
        showPassword = false
        label = fieldLabel || (passwordMode ? "PASSWORD" : "TEXT")
        focusRow = 0
        focusCol = 0
        shifted = false
        numbersMode = false
        visible = true
        root.forceActiveFocus()
    }

    function close() {
        visible = false
    }

    // ── Internal state ──
    property int focusRow: 0
    property int focusCol: 0
    property bool shifted: false
    property bool numbersMode: false

    property var qwertyRows: [
        ["q","w","e","r","t","y","u","i","o","p"],
        ["a","s","d","f","g","h","j","k","l"],
        ["\u21E7","z","x","c","v","b","n","m","\u232B"],
        ["123","@"," ",".","\u2713"]
    ]

    property var numberRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["-","/",":",";","(",")","$","&","\""],
        ["#","=","+","!","?","%","^","*","\u232B"],
        ["abc","@"," ",".","\u2713"]
    ]

    property var currentRows: numbersMode ? numberRows : qwertyRows

    function getDisplayChar(ch) {
        if (ch === "\u21E7") return shifted ? "\u21E7" : "\u21E7"  // Shift arrow
        if (ch === "\u232B") return "\u232B"  // Backspace
        if (ch === "\u2713") return "Done"
        if (ch === " ") return "space"
        if (shifted && !numbersMode && ch.length === 1 && ch >= "a" && ch <= "z")
            return ch.toUpperCase()
        return ch
    }

    function getKeyWidth(ch) {
        if (ch === " ") return 160
        if (ch === "\u21E7" || ch === "\u232B") return 48
        if (ch === "123" || ch === "abc") return 48
        if (ch === "\u2713") return 64
        return 36
    }

    function handleKeyPress(ch) {
        if (ch === "\u21E7") {
            shifted = !shifted
            return
        }
        if (ch === "\u232B") {
            if (text.length > 0)
                text = text.substring(0, text.length - 1)
            return
        }
        if (ch === "\u2713") {
            accepted()
            close()
            return
        }
        if (ch === "123") {
            numbersMode = true
            focusRow = 0; focusCol = 0
            return
        }
        if (ch === "abc") {
            numbersMode = false
            focusRow = 0; focusCol = 0
            return
        }

        // Type the character
        var typed = ch
        if (shifted && !numbersMode && ch.length === 1 && ch >= "a" && ch <= "z") {
            typed = ch.toUpperCase()
            shifted = false  // one-shot shift
        }
        text += typed
    }

    // Clamp focusCol when switching rows
    onFocusRowChanged: {
        var row = currentRows[focusRow]
        if (row && focusCol >= row.length)
            focusCol = row.length - 1
    }

    // ── Controller navigation via inputManager ──
    Connections {
        target: inputManager
        enabled: root.visible

        function onNavigateUp() {
            if (root.focusRow > 0) root.focusRow--
        }
        function onNavigateDown() {
            if (root.focusRow < root.currentRows.length - 1) root.focusRow++
        }
        function onNavigateLeft() {
            if (root.focusCol > 0) root.focusCol--
        }
        function onNavigateRight() {
            var row = root.currentRows[root.focusRow]
            if (row && root.focusCol < row.length - 1) root.focusCol++
        }
        function onNavigateAccept() {
            var row = root.currentRows[root.focusRow]
            if (row) root.handleKeyPress(row[root.focusCol])
        }
        function onNavigateBack() {
            root.text = root._initialText
            root.cancelled()
            root.close()
        }
    }

    // ── Physical keyboard passthrough ──
    Keys.onPressed: function(event) {
        if (!visible) return

        if (event.key === Qt.Key_Escape) {
            text = _initialText
            cancelled()
            close()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            accepted()
            close()
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            if (text.length > 0)
                text = text.substring(0, text.length - 1)
            event.accepted = true
        } else if (event.text.length > 0 && event.text.charCodeAt(0) >= 32) {
            text += event.text
            event.accepted = true
        }
    }

    // Click backdrop to cancel
    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.text = root._initialText
            root.cancelled()
            root.close()
        }
    }

    // ── Visual layout ──
    Rectangle {
        id: keyboardPanel
        anchors.centerIn: parent
        width: 420
        height: contentCol.implicitHeight + 40
        radius: 12
        color: SettingsTheme.surface
        border.width: 1
        border.color: SettingsTheme.border

        // Prevent click-through to backdrop
        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: contentCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 12

            // Field label
            Text {
                text: root.label
                color: SettingsTheme.textMuted
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
                Layout.alignment: Qt.AlignHCenter
            }

            // Text preview
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    radius: 6
                    color: SettingsTheme.card
                    border.width: 2
                    border.color: SettingsTheme.focusBorder

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: {
                            var display = (root.isPassword && !root.showPassword)
                                ? "\u2022".repeat(root.text.length)
                                : root.text
                            return display + "\u2502"  // cursor
                        }
                        color: SettingsTheme.text
                        font.pixelSize: 16
                        elide: Text.ElideLeft
                    }
                }

                // Show/hide toggle (password only)
                Rectangle {
                    visible: root.isPassword
                    width: 40
                    height: 40
                    radius: 6
                    color: eyeMa.containsMouse ? Qt.lighter(SettingsTheme.card, 1.2) : SettingsTheme.card
                    border.width: 1
                    border.color: SettingsTheme.border

                    Text {
                        anchors.centerIn: parent
                        text: root.showPassword ? "\uD83D\uDC41" : "\uD83D\uDC41\u200D\uD83D\uDDE8"
                        font.pixelSize: 16
                    }

                    MouseArea {
                        id: eyeMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showPassword = !root.showPassword
                    }
                }
            }

            // Keyboard grid
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4

                Repeater {
                    model: root.currentRows.length

                    RowLayout {
                        id: keyRow
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 4

                        property int rowIndex: index

                        Repeater {
                            model: root.currentRows[keyRow.rowIndex].length

                            Rectangle {
                                id: keyRect
                                property string keyChar: root.currentRows[keyRow.rowIndex][index]
                                property bool isFocused: root.focusRow === keyRow.rowIndex && root.focusCol === index
                                property bool isSpecial: keyChar === "\u21E7" || keyChar === "\u232B" || keyChar === "123" || keyChar === "abc" || keyChar === "\u2713"
                                property bool isShiftActive: keyChar === "\u21E7" && root.shifted

                                width: root.getKeyWidth(keyChar)
                                height: 36
                                radius: 4
                                color: {
                                    if (keyChar === "\u2713") return isFocused ? Qt.lighter(SettingsTheme.accent, 1.1) : SettingsTheme.accent
                                    if (isShiftActive) return SettingsTheme.accentDim
                                    if (isFocused) return Qt.lighter(SettingsTheme.card, 1.4)
                                    if (isSpecial) return Qt.lighter(SettingsTheme.card, 1.1)
                                    return SettingsTheme.card
                                }
                                border.width: isFocused ? 2 : 1
                                border.color: isFocused ? SettingsTheme.focusBorder : SettingsTheme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: root.getDisplayChar(keyRect.keyChar)
                                    color: {
                                        if (keyRect.keyChar === "\u2713") return SettingsTheme.background
                                        if (keyRect.isSpecial) return SettingsTheme.accent
                                        return SettingsTheme.text
                                    }
                                    font.pixelSize: keyRect.keyChar === " " ? 11 : 14
                                    font.weight: keyRect.isSpecial ? Font.DemiBold : Font.Normal
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.handleKeyPress(keyRect.keyChar)
                                }

                                // Focus glow
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -3
                                    radius: parent.radius + 3
                                    color: "transparent"
                                    border.width: 2
                                    border.color: SettingsTheme.focusBorder
                                    opacity: keyRect.isFocused ? 0.3 : 0
                                    z: -1
                                    visible: opacity > 0
                                    Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                                }
                            }
                        }
                    }
                }
            }

            // Hint text
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "\u2190\u2192\u2191\u2193 Navigate  \u2022  \uD83C\uDD70 Type  \u2022  \uD83C\uDD71 Cancel"
                color: SettingsTheme.textGhost
                font.pixelSize: 10
            }
        }
    }
}
```

- [ ] **Step 2: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/VirtualKeyboard.qml
git commit -m "feat: implement VirtualKeyboard.qml with QWERTY grid, controller nav, and keyboard passthrough"
```

---

### Task 3: Add Login Form Focus Navigation to ScraperSettings.qml

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:1-9` (root properties)
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:138-169` (Keys.onPressed handler)
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:415-433` (username field wrapper)
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:448-466` (password field wrapper)
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:470-498` (sign in button)

- [ ] **Step 1: Add login focus properties to the root Item**

After the existing `property string gameFilter: "all"` line (line 16), add:

```qml
    // Login form focus
    property int loginFocusIndex: 0
    property bool lastInputWasController: false
```

- [ ] **Step 2: Add inputManager Connections for controller detection and login navigation**

After the new properties, add a Connections block for input source tracking and login-screen navigation:

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

- [ ] **Step 3: Extend Keys.onPressed to handle login state**

The existing `Keys.onPressed` at line 138 starts with `if (screenState !== "dashboard") return`. Replace that guard and add login handling at the top of the handler:

Change the beginning of the `Keys.onPressed` handler from:

```qml
    Keys.onPressed: function(event) {
        if (screenState !== "dashboard") return
```

To:

```qml
    Keys.onPressed: function(event) {
        if (virtualKeyboard.visible) return

        // Login screen keyboard navigation
        if (screenState === "login") {
            root.lastInputWasController = false
            if (event.key === Qt.Key_Down) {
                loginFocusIndex = (loginFocusIndex + 1) % 3
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                loginFocusIndex = (loginFocusIndex - 1 + 3) % 3
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                activateLoginFocused()
                event.accepted = true
            } else if (event.key === Qt.Key_Tab) {
                loginFocusIndex = (loginFocusIndex + 1) % 3
                event.accepted = true
            }
            return
        }

        if (screenState !== "dashboard") return
```

- [ ] **Step 4: Add activateLoginFocused function**

Add this function after the existing `activateFocused()` function (after line 208):

```qml
    function activateLoginFocused() {
        if (loginFocusIndex === 0) {
            // Username field
            if (lastInputWasController) {
                virtualKeyboard.open(loginUserField.text, false, "USERNAME")
            } else {
                loginUserField.forceActiveFocus()
            }
        } else if (loginFocusIndex === 1) {
            // Password field
            if (lastInputWasController) {
                virtualKeyboard.open(loginPassField.text, true, "PASSWORD")
            } else {
                loginPassField.forceActiveFocus()
            }
        } else if (loginFocusIndex === 2) {
            // Sign In button
            if (signInBtn.enabled) {
                signInBtn.enabled = false
                loginError.visible = false
                app.validateScraperCredentials(loginUserField.text, loginPassField.text)
            }
        }
    }
```

- [ ] **Step 5: Add focus styling to the username field wrapper**

Change the username field wrapper Rectangle (around line 415-420) from:

```qml
                    Rectangle {
                        Layout.preferredWidth: 300
                        height: 36
                        radius: 6
                        color: SettingsTheme.card
                        border.width: 1
                        border.color: loginUserField.activeFocus ? SettingsTheme.accent : SettingsTheme.border
```

To:

```qml
                    Rectangle {
                        Layout.preferredWidth: 300
                        height: 36
                        radius: 6
                        color: SettingsTheme.card
                        border.width: (root.screenState === "login" && root.loginFocusIndex === 0) ? 2 : 1
                        border.color: (root.screenState === "login" && root.loginFocusIndex === 0) || loginUserField.activeFocus
                            ? SettingsTheme.focusBorder : SettingsTheme.border

                        // Focus glow
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -4
                            radius: parent.radius + 4
                            color: "transparent"
                            border.width: 2
                            border.color: SettingsTheme.focusBorder
                            opacity: (root.screenState === "login" && root.loginFocusIndex === 0) ? 0.3 : 0
                            z: -1
                            visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                        }
```

- [ ] **Step 6: Add focus styling to the password field wrapper**

Change the password field wrapper Rectangle (around line 448-453) from:

```qml
                    Rectangle {
                        Layout.preferredWidth: 300
                        height: 36
                        radius: 6
                        color: SettingsTheme.card
                        border.width: 1
                        border.color: loginPassField.activeFocus ? SettingsTheme.accent : SettingsTheme.border
```

To:

```qml
                    Rectangle {
                        Layout.preferredWidth: 300
                        height: 36
                        radius: 6
                        color: SettingsTheme.card
                        border.width: (root.screenState === "login" && root.loginFocusIndex === 1) ? 2 : 1
                        border.color: (root.screenState === "login" && root.loginFocusIndex === 1) || loginPassField.activeFocus
                            ? SettingsTheme.focusBorder : SettingsTheme.border

                        // Focus glow
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -4
                            radius: parent.radius + 4
                            color: "transparent"
                            border.width: 2
                            border.color: SettingsTheme.focusBorder
                            opacity: (root.screenState === "login" && root.loginFocusIndex === 1) ? 0.3 : 0
                            z: -1
                            visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                        }
```

- [ ] **Step 7: Add focus styling to the Sign In button**

Change the Sign In button Rectangle (around line 470-478) from:

```qml
                Rectangle {
                    id: signInBtn
                    property bool enabled: true
                    Layout.leftMargin: 24
                    width: 120
                    height: 36
                    radius: 6
                    color: enabled ? SettingsTheme.accent : SettingsTheme.card
                    opacity: enabled ? 1.0 : 0.5
```

To:

```qml
                Rectangle {
                    id: signInBtn
                    property bool enabled: true
                    property bool isFocused: root.screenState === "login" && root.loginFocusIndex === 2
                    Layout.leftMargin: 24
                    width: 120
                    height: 36
                    radius: 6
                    color: enabled ? SettingsTheme.accent : SettingsTheme.card
                    opacity: enabled ? 1.0 : 0.5
                    border.width: isFocused ? 2 : 0
                    border.color: SettingsTheme.text

                    // Focus glow
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        radius: parent.radius + 4
                        color: "transparent"
                        border.width: 2
                        border.color: SettingsTheme.focusBorder
                        opacity: signInBtn.isFocused ? 0.3 : 0
                        z: -1
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                    }
```

- [ ] **Step 8: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 9: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: add keyboard/controller focus navigation to login form"
```

---

### Task 4: Integrate VirtualKeyboard with Login Form

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml` (add VirtualKeyboard instance and wire it up)

- [ ] **Step 1: Add the VirtualKeyboard instance**

At the end of the root Item (just before the final closing `}`), add the VirtualKeyboard and wire its signals:

```qml
    // ── Virtual Keyboard ──
    VirtualKeyboard {
        id: virtualKeyboard

        onAccepted: {
            // Write text back to the active field
            if (loginFocusIndex === 0) {
                loginUserField.text = virtualKeyboard.text
            } else if (loginFocusIndex === 1) {
                loginPassField.text = virtualKeyboard.text
            }
            root.forceActiveFocus()
        }

        onCancelled: {
            root.forceActiveFocus()
        }
    }
```

- [ ] **Step 2: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 3: Run the app and manually test**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

Test sequence:
1. Press Escape to open Settings
2. Navigate to Scraper (arrow keys down, Enter)
3. On login page: Up/Down moves golden focus between Username, Password, Sign In
4. Press Enter on Username — if using keyboard, field gets direct focus for typing
5. If using a controller: D-pad to Scraper, press A on Username → virtual keyboard opens
6. Type with virtual keyboard, press Done → text appears in field
7. Press B on virtual keyboard → text reverts, keyboard closes
8. Navigate to Password, open virtual keyboard → shows dots, eye toggle reveals text
9. Navigate to Sign In, press Enter/A → triggers sign-in

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: wire VirtualKeyboard to login form with auto-detect input source"
```

---

### Task 5: Polish and Edge Cases

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml`
- Modify: `cpp/qml/AppUI/VirtualKeyboard.qml`

- [ ] **Step 1: Reset loginFocusIndex when entering login state**

In ScraperSettings.qml, add a handler so focus resets when the screen state changes to login. Add this after the `screenState` property:

```qml
    onScreenStateChanged: {
        if (screenState === "login") {
            loginFocusIndex = 0
        }
    }
```

- [ ] **Step 2: Ensure focus returns to root after TextField loses focus**

When a user clicks away from a TextField or presses Escape while a TextField has focus, we need focus to return to the root Item so arrow keys work again. Add this after the `onScreenStateChanged` handler:

```qml
    // Reclaim focus when TextFields lose active focus (e.g., click away or Escape)
    Connections {
        target: loginUserField
        function onActiveFocusChanged() {
            if (!loginUserField.activeFocus && screenState === "login" && !virtualKeyboard.visible)
                root.forceActiveFocus()
        }
    }
    Connections {
        target: loginPassField
        function onActiveFocusChanged() {
            if (!loginPassField.activeFocus && screenState === "login" && !virtualKeyboard.visible)
                root.forceActiveFocus()
        }
    }
```

- [ ] **Step 3: Add Tab key support in TextFields to move to next field**

Add `Keys.onTabPressed` handlers to both login TextFields so Tab moves focus to the next form element. In the `loginUserField` TextField (around line 423), add:

```qml
                        TextField {
                            id: loginUserField
                            anchors.fill: parent
                            placeholderText: "screenscraper.fr username"
                            placeholderTextColor: SettingsTheme.textDim
                            color: SettingsTheme.text
                            font.pixelSize: 13
                            background: Item {}
                            leftPadding: 10
                            Keys.onTabPressed: {
                                loginFocusIndex = 1
                                loginPassField.forceActiveFocus()
                            }
                            Keys.onReturnPressed: {
                                loginFocusIndex = 1
                                loginPassField.forceActiveFocus()
                            }
                            Keys.onEnterPressed: {
                                loginFocusIndex = 1
                                loginPassField.forceActiveFocus()
                            }
                        }
```

In the `loginPassField` TextField (around line 455), add:

```qml
                        TextField {
                            id: loginPassField
                            anchors.fill: parent
                            placeholderText: "screenscraper.fr password"
                            placeholderTextColor: SettingsTheme.textDim
                            color: SettingsTheme.text
                            font.pixelSize: 13
                            echoMode: TextInput.Password
                            background: Item {}
                            leftPadding: 10
                            Keys.onTabPressed: {
                                loginFocusIndex = 2
                                root.forceActiveFocus()
                            }
                            Keys.onReturnPressed: {
                                // Submit from password field
                                if (signInBtn.enabled) {
                                    signInBtn.enabled = false
                                    loginError.visible = false
                                    app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                                }
                            }
                            Keys.onEnterPressed: {
                                if (signInBtn.enabled) {
                                    signInBtn.enabled = false
                                    loginError.visible = false
                                    app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                                }
                            }
                        }
```

- [ ] **Step 4: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 5: Run the app and test edge cases**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

Test:
1. Tab from username → password → sign-in button (focus cycles through)
2. Enter on password field → triggers sign-in
3. Open virtual keyboard, press physical keys → text appears directly
4. Open virtual keyboard for password → dots shown, click eye → revealed
5. Press B/Escape on virtual keyboard → text reverts to original
6. Click backdrop behind virtual keyboard → cancels and closes
7. Navigate away from Scraper and back → loginFocusIndex resets to 0

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: add login form polish — Tab support, Enter-to-submit, focus recovery"
```

---

### Task 6: Wire VirtualKeyboard to Dashboard Account Edit Fields

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml` (dashboard account editing section, around lines 596-710)

The dashboard already has its own `focusIndex` system for pill navigation. The account edit fields (`acctUserField`, `acctPassField`) are shown when `root.accountEditing` is true. We wire them to the same `VirtualKeyboard` instance.

- [ ] **Step 1: Add inputManager Connections for dashboard edit fields**

Add a second Connections block for controller input when the dashboard edit fields are visible. Place it after the login Connections block:

```qml
    // Controller navigation for dashboard account editing
    Connections {
        target: inputManager
        enabled: root.screenState === "dashboard" && root.accountEditing && !virtualKeyboard.visible

        function onNavigateAccept() {
            root.lastInputWasController = true
            // If focus is on one of the account edit fields, open virtual keyboard
            if (acctUserField.activeFocus) {
                virtualKeyboard.open(acctUserField.text, false, "USERNAME")
            } else if (acctPassField.activeFocus) {
                virtualKeyboard.open(acctPassField.text, true, "PASSWORD")
            }
        }
    }
```

- [ ] **Step 2: Update VirtualKeyboard onAccepted to handle dashboard fields**

Modify the `VirtualKeyboard` `onAccepted` handler in ScraperSettings.qml to also write back to the dashboard edit fields. Change the existing handler from:

```qml
        onAccepted: {
            // Write text back to the active field
            if (loginFocusIndex === 0) {
                loginUserField.text = virtualKeyboard.text
            } else if (loginFocusIndex === 1) {
                loginPassField.text = virtualKeyboard.text
            }
            root.forceActiveFocus()
        }
```

To:

```qml
        onAccepted: {
            // Write text back to the active field
            if (screenState === "login") {
                if (loginFocusIndex === 0) {
                    loginUserField.text = virtualKeyboard.text
                } else if (loginFocusIndex === 1) {
                    loginPassField.text = virtualKeyboard.text
                }
            } else if (screenState === "dashboard") {
                if (virtualKeyboard.label === "USERNAME") {
                    acctUserField.text = virtualKeyboard.text
                } else if (virtualKeyboard.label === "PASSWORD") {
                    acctPassField.text = virtualKeyboard.text
                }
            }
            root.forceActiveFocus()
        }
```

- [ ] **Step 3: Verify build compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: wire VirtualKeyboard to dashboard account edit fields"
```
