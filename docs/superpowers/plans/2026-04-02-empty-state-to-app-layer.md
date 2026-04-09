# Move Empty State to App Layer — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the "No games found" empty state from the theme layer into the app layer so themes never need to implement it.

**Architecture:** A new `EmptyStatePage.qml` in `qml/AppUI/` owns the empty state UI. `AppWindow.qml` conditionally pushes either the empty state or the theme's systemBrowser based on `themeContext.systems.length`. `AppController` gains an `openRomFolder()` method. The theme's empty state code (~170 lines) and dead GameListPage placeholder are removed.

**Tech Stack:** Qt6 QML, C++17

---

### Task 1: Add `openRomFolder()` to AppController

**Files:**
- Modify: `cpp/src/ui/app_controller.h:53` (near existing `openBiosFolder()`)
- Modify: `cpp/src/ui/app_controller.cpp:212` (after `openBiosFolder()` implementation)

- [ ] **Step 1: Add declaration to header**

In `cpp/src/ui/app_controller.h`, add the new method right after the `openBiosFolder()` declaration (line 53):

```cpp
    Q_INVOKABLE void openRomFolder();
```

- [ ] **Step 2: Add implementation**

In `cpp/src/ui/app_controller.cpp`, add the implementation right after `openBiosFolder()` (after line 216):

```cpp
void AppController::openRomFolder() {
    QString dir = Paths::romsDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
```

- [ ] **Step 3: Build to verify**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "feat: add openRomFolder() to AppController

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Copy background image to app assets

**Files:**
- Create: `cpp/qml/AppUI/images/empty-state-bg.webp` (copy from `themes/modern/assets/artwork/_default.webp`)

- [ ] **Step 1: Copy the image**

```bash
cp /Users/mark/Documents/EmuFront-Project/themes/modern/assets/artwork/_default.webp \
   /Users/mark/Documents/EmuFront-Project/cpp/qml/AppUI/images/empty-state-bg.webp
```

- [ ] **Step 2: Add to CMakeLists.txt RESOURCES**

In `cpp/CMakeLists.txt`, add the image to the AppUI module's RESOURCES block (after `qml/AppUI/images/ps5_controller.png` on line 185):

```cmake
        qml/AppUI/images/empty-state-bg.webp
```

- [ ] **Step 3: Build to verify**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/images/empty-state-bg.webp cpp/CMakeLists.txt
git commit -m "feat: add empty state background image to app assets

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Create EmptyStatePage.qml

**Files:**
- Create: `cpp/qml/AppUI/EmptyStatePage.qml`
- Modify: `cpp/CMakeLists.txt` (add to QML_FILES)

- [ ] **Step 1: Create EmptyStatePage.qml**

Create `cpp/qml/AppUI/EmptyStatePage.qml`:

```qml
import QtQuick
import QtQuick.Controls

Item {
    id: root
    focus: true

    Component.onCompleted: root.forceActiveFocus()
    StackView.onActivated: root.forceActiveFocus()

    property int focusIndex: 0  // 0 = Open ROM Folder, 1 = Scan, 2 = Import

    // Keyboard/controller navigation
    Keys.onLeftPressed: {
        if (root.focusIndex === 2) root.focusIndex = 1
    }
    Keys.onRightPressed: {
        if (root.focusIndex === 1) root.focusIndex = 2
    }
    Keys.onUpPressed: {
        if (root.focusIndex > 0) root.focusIndex = 0
    }
    Keys.onDownPressed: {
        if (root.focusIndex === 0) root.focusIndex = 1
    }
    Keys.onReturnPressed: root.activateButton()
    Keys.onEnterPressed: root.activateButton()

    function activateButton() {
        if (focusIndex === 0) app.openRomFolder()
        else if (focusIndex === 1) app.scanRomFolders()
        else if (focusIndex === 2) app.importRoms()
    }

    // Background image
    Image {
        anchors.fill: parent
        source: "images/empty-state-bg.webp"
        fillMode: Image.PreserveAspectCrop
    }

    // Dark scrim overlay
    Rectangle {
        anchors.fill: parent
        color: "#1a1a2e"
        opacity: 0.85
    }

    // Content
    Column {
        anchors.centerIn: parent
        spacing: 24

        // Title
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "No games found"
            color: "#ffffff"
            font.pixelSize: 26
            font.weight: Font.Bold
        }

        // Instructions
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Add ROMs to your system folders, then scan to discover them.\nUse Open ROM Folder to find the right directory."
            color: Qt.rgba(1, 1, 1, 0.7)
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.6
        }

        // Primary button: Open ROM Folder
        Rectangle {
            id: openFolderBtn
            anchors.horizontalCenter: parent.horizontalCenter
            width: openFolderText.implicitWidth + 48
            height: 50
            radius: 10
            color: openFolderMa.containsMouse || root.focusIndex === 0
                   ? "#6b6be6" : "#3d3d8a"
            Behavior on color { ColorAnimation { duration: 100 } }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                radius: parent.radius + 4
                color: "transparent"
                border.width: 2
                border.color: "#5b5bd6"
                opacity: root.focusIndex === 0 ? 0.4 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }

            Text {
                id: openFolderText
                anchors.centerIn: parent
                text: "Open ROM Folder"
                color: "#ffffff"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: openFolderMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: app.openRomFolder()
            }
        }

        // Secondary buttons row
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            // Scan for Games
            Rectangle {
                id: scanBtn
                width: scanText.implicitWidth + 40
                height: 46
                radius: 10
                color: scanMa.containsMouse || root.focusIndex === 1
                       ? "#363660" : Qt.rgba(0.17, 0.17, 0.31, 0.8)
                border.color: Qt.rgba(0.36, 0.36, 0.84, 0.35)
                border.width: 1
                Behavior on color { ColorAnimation { duration: 100 } }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: parent.radius + 4
                    color: "transparent"
                    border.width: 2
                    border.color: "#5b5bd6"
                    opacity: root.focusIndex === 1 ? 0.4 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                Text {
                    id: scanText
                    anchors.centerIn: parent
                    text: "Scan for Games"
                    color: Qt.rgba(1, 1, 1, 0.75)
                    font.pixelSize: 14
                }
                MouseArea {
                    id: scanMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: app.scanRomFolders()
                }
            }

            // Import ROMs
            Rectangle {
                id: importBtn
                width: importText.implicitWidth + 40
                height: 46
                radius: 10
                color: importMa.containsMouse || root.focusIndex === 2
                       ? "#363660" : Qt.rgba(0.17, 0.17, 0.31, 0.8)
                border.color: Qt.rgba(0.36, 0.36, 0.84, 0.35)
                border.width: 1
                Behavior on color { ColorAnimation { duration: 100 } }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: parent.radius + 4
                    color: "transparent"
                    border.width: 2
                    border.color: "#5b5bd6"
                    opacity: root.focusIndex === 2 ? 0.4 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                Text {
                    id: importText
                    anchors.centerIn: parent
                    text: "Import ROMs"
                    color: Qt.rgba(1, 1, 1, 0.75)
                    font.pixelSize: 14
                }
                MouseArea {
                    id: importMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: app.importRoms()
                }
            }
        }

        // Navigation hint
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "↑ ↓ Navigate    Enter Select"
            color: Qt.rgba(1, 1, 1, 0.35)
            font.pixelSize: 11
        }
    }
}
```

- [ ] **Step 2: Add to CMakeLists.txt QML_FILES**

In `cpp/CMakeLists.txt`, add EmptyStatePage.qml to the AppUI module's QML_FILES block (after `qml/AppUI/AppWindow.qml` on line 159):

```cmake
        qml/AppUI/EmptyStatePage.qml
```

- [ ] **Step 3: Build to verify**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/EmptyStatePage.qml cpp/CMakeLists.txt
git commit -m "feat: add EmptyStatePage.qml to app layer

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Update AppWindow routing logic

**Files:**
- Modify: `cpp/qml/AppUI/AppWindow.qml`

- [ ] **Step 1: Add helper property and function**

In `AppWindow.qml`, add a property to track whether we're showing the empty state, and a function to handle the transition. Add these right after the `bottomPadding: 0` line (line 16), before `Component.onCompleted`:

```qml
    // Track whether the empty state page is currently shown
    property bool showingEmptyState: false

    function updateMainPage() {
        var hasGames = themeContext.systems.length > 0
        if (hasGames && showingEmptyState) {
            // Games appeared — switch to theme
            mainStack.clear()
            var url = themeManager.resolve("systemBrowser").toString()
            if (url !== "") mainStack.push(url)
            showingEmptyState = false
        } else if (!hasGames && !showingEmptyState) {
            // No games — switch to empty state
            mainStack.clear()
            mainStack.push("EmptyStatePage.qml")
            showingEmptyState = true
        }
    }
```

- [ ] **Step 2: Update Component.onCompleted to route conditionally**

Replace the existing `Component.onCompleted` block inside the StackView (lines 33-41) with:

```qml
        Component.onCompleted: {
            if (themeContext.systems.length > 0) {
                var url = themeManager.resolve("systemBrowser").toString()
                if (url !== "") {
                    mainStack.push(url)
                } else {
                    console.warn("[AppWindow] No systemBrowser URL resolved — is a theme installed?")
                }
            } else {
                mainStack.push("EmptyStatePage.qml")
                window.showingEmptyState = true
            }
            app.checkForUpdates()
        }
```

- [ ] **Step 3: Add Connections for systems/games changes**

Add a new `Connections` block after the existing themeContext Connections block (after line 58). This handles transitions between empty state and theme:

```qml
    // Transition between empty state and theme when games change
    Connections {
        target: themeContext
        function onSystemsChanged() { window.updateMainPage() }
        function onGamesChanged() { window.updateMainPage() }
    }
```

- [ ] **Step 4: Guard theme change handler**

Update the existing `onCurrentThemeChanged` handler (lines 62-68) to only reload if we're currently showing a theme page:

```qml
    // Reload theme pages when theme changes
    Connections {
        target: themeManager
        function onCurrentThemeChanged() {
            if (!window.showingEmptyState) {
                mainStack.clear()
                mainStack.push(themeManager.resolve("systemBrowser").toString())
            }
        }
    }
```

- [ ] **Step 5: Build to verify**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 6: Manual test**

1. Launch with no games — should show the empty state page with retro arcade background
2. Press Escape to open settings, add ROMs via settings, scan — should transition to theme's system browser
3. Remove all games — should return to empty state

- [ ] **Step 7: Commit**

```bash
git add cpp/qml/AppUI/AppWindow.qml
git commit -m "feat: route AppWindow between empty state and theme based on game count

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Remove empty state from theme SystemPage

**Files:**
- Modify: `themes/modern/SystemPage.qml`

- [ ] **Step 1: Remove emptyFocusIndex property**

Delete line 22:

```qml
    property int emptyFocusIndex: 0  // 0 = Open ROM Folder, 1 = Scan, 2 = Import
```

- [ ] **Step 2: Simplify keyboard handlers**

Replace the keyboard handlers (lines 46-73) with simplified versions that assume systems always exist:

```qml
    // Keyboard navigation
    Keys.onLeftPressed: root.carouselPrev()
    Keys.onRightPressed: root.carouselNext()
    Keys.onReturnPressed: themeContext.navigateToSystem(systemList[root.carouselIndex])
    Keys.onEnterPressed: themeContext.navigateToSystem(systemList[root.carouselIndex])
```

- [ ] **Step 3: Remove activateEmptyButton function**

Delete lines 75-79:

```qml
    function activateEmptyButton() {
        if (emptyFocusIndex === 0) themeContext.openRomFolder()
        else if (emptyFocusIndex === 1) themeContext.scanRomFolders()
        else if (emptyFocusIndex === 2) themeContext.importRoms()
    }
```

- [ ] **Step 4: Remove the entire empty state block**

Delete the entire empty state Rectangle and its contents (lines 348-523):

```qml
    // ─── Empty state ──────────────────────────────────────────────────────────
    Rectangle {
        id: emptyState
        ...
    }
```

This is everything from the `// ─── Empty state` comment to the closing `}` of the `emptyState` Rectangle, including the scrim, content column, all three buttons, focus glows, mouse areas, and the navigation hint.

- [ ] **Step 5: Build to verify**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add themes/modern/SystemPage.qml
git commit -m "refactor: remove empty state from theme SystemPage

The empty state is now handled by the app-level EmptyStatePage.
SystemPage can assume systems always exist when it loads.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Remove dead "No games found" from GameListPage

**Files:**
- Modify: `themes/modern/GameListPage.qml`

- [ ] **Step 1: Remove the placeholder text**

Delete the "No games found" Text element inside the ListView (lines 574-580 of GameListPage.qml):

```qml
            Text {
                anchors.centerIn: parent
                text: "No games found"
                color: Qt.rgba(1, 1, 1, 0.50)
                font.pixelSize: 25
                visible: gameModel.count === 0
            }
```

- [ ] **Step 2: Build to verify**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add themes/modern/GameListPage.qml
git commit -m "refactor: remove dead 'No games found' placeholder from GameListPage

This text was never visible — systems with zero games don't appear
in the system list, so GameListPage never shows with an empty model.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: Final integration test

- [ ] **Step 1: Full build**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -10`
Expected: Clean build, no warnings related to empty state.

- [ ] **Step 2: Test empty state flow**

1. Start app with no games in database — verify EmptyStatePage shows with retro arcade background
2. Use keyboard arrows to navigate between the three buttons — verify focus glow moves correctly
3. Press Enter on "Open ROM Folder" — verify Finder opens the roms directory
4. Press Escape to open settings overlay — verify it opens normally over the empty state
5. Close settings — verify empty state regains focus

- [ ] **Step 3: Test transition to theme**

1. From empty state, press Escape → go to settings → scan for games (with ROMs present)
2. Close settings — verify the theme's SystemPage (carousel) now shows instead of empty state
3. Verify carousel navigation works normally

- [ ] **Step 4: Test transition back to empty state**

1. Remove all games (if possible via settings/CLI)
2. Verify the app transitions back to the empty state page

- [ ] **Step 5: Test theme switching**

1. With games present (theme showing), switch themes in settings
2. Verify theme reloads correctly
3. With no games (empty state showing), switch themes
4. Verify empty state remains (no crash or theme load attempt)
