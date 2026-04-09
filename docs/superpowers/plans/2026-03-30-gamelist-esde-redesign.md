# GameList Page ES-DE Style Redesign

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the GameListPage.qml to match ES-DE's detailed layout — left detail panel with media showcase (cover art, screenshot, physical media, video after 5s), metadata grid in dark rounded boxes, auto-scrolling description, and a right-side game list inside a semi-transparent dark box with white highlight. Fanart as background.

**Architecture:** The theme's `GameListPage.qml` is a runtime-loaded QML file in `themes/modern/`. It accesses game data via `themeContext.gameDetailsByIndex()` which returns a `QVariantMap`. Media paths (screenshot, fanart, box3d, physicalmedia, video) are already stored in the DB and exposed via `GameListModel` roles but NOT yet returned by `gameDetails()` — Task 1 fixes that. Video playback requires adding `Qt6::Multimedia` to CMake. The rest is pure QML layout work.

**Tech Stack:** Qt6 QML, Qt6 Multimedia (for video), C++17 (minor backend change)

---

### Task 1: Add media paths to gameDetails() and CMake Multimedia dependency

**Files:**
- Modify: `cpp/src/ui/theme_context.cpp:90-110`
- Modify: `cpp/CMakeLists.txt:8` and `101-112`

- [ ] **Step 1: Add media paths to gameDetails()**

In `cpp/src/ui/theme_context.cpp`, add these lines inside `gameDetails()` after line 108 (`map["favorite"]`):

```cpp
    map["screenshotPath"]    = g.screenshot_path;
    map["titlescreenPath"]   = g.titlescreen_path;
    map["marqueePath"]       = g.marquee_path;
    map["fanartPath"]        = g.fanart_path;
    map["box3dPath"]         = g.box3d_path;
    map["backcoverPath"]     = g.backcover_path;
    map["miximagePath"]      = g.miximage_path;
    map["physicalmediaPath"] = g.physicalmedia_path;
    map["manualPath"]        = g.manual_path;
    map["videoPath"]         = g.video_path;
```

- [ ] **Step 2: Add Qt6 Multimedia to CMakeLists.txt**

In `cpp/CMakeLists.txt` line 8, add `Multimedia` to the find_package:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Qml Quick QuickControls2 Concurrent Multimedia)
```

And in the `target_link_libraries` block (around line 101), add:

```cmake
    Qt6::Multimedia
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
cmake --build build 2>&1 | tail -5
```

Expected: Build succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/theme_context.cpp cpp/CMakeLists.txt
git commit -m "feat: expose media paths in gameDetails() and add Qt6 Multimedia"
```

---

### Task 2: Create gamepage-logos asset folder

**Files:**
- Create: `themes/modern/assets/gamepage-logos/.gitkeep`

- [ ] **Step 1: Create the directory**

```bash
mkdir -p /Users/mark/Documents/EmuFront-Project/themes/modern/assets/gamepage-logos
touch /Users/mark/Documents/EmuFront-Project/themes/modern/assets/gamepage-logos/.gitkeep
```

The user will add their own logo images (e.g. `ps2.webp`, `psx.webp`) to this folder. The QML will reference them as `"assets/gamepage-logos/" + themeContext.currentSystem + ".webp"`.

- [ ] **Step 2: Commit**

```bash
git add themes/modern/assets/gamepage-logos/.gitkeep
git commit -m "feat: add gamepage-logos asset directory for system logos on game list page"
```

---

### Task 3: Redesign GameListPage.qml — Layout structure and game list

This is the main QML rewrite. We split it into two tasks: this one handles the overall structure, background, and game list (right side). Task 4 handles the detail panel (left side).

**Files:**
- Modify: `themes/modern/GameListPage.qml` (full rewrite)

- [ ] **Step 1: Write the new GameListPage.qml with layout + game list**

Replace the entire `themes/modern/GameListPage.qml` with the following. This step includes the full file — the left detail panel (Task 4 content) is included inline so the file is always complete and buildable.

```qml
import QtQuick
import QtQuick.Controls
import QtMultimedia

Item {
    id: root
    focus: true

    Component.onCompleted: {
        root.forceActiveFocus()
        if (gameModel.count > 0) {
            root.listIndex = 0
            root.selectCurrentGame()
        }
    }
    StackView.onActivated: root.forceActiveFocus()

    // ── State ──
    property int listIndex: 0
    property var selectedDetails: ({})
    property bool hasDetails: (selectedDetails.description || "").length > 0
                           || (selectedDetails.developer   || "").length > 0
                           || (selectedDetails.rating      || 0) > 0

    // Video delay timer — starts video after 5 seconds on same game
    property bool showVideo: false
    Timer {
        id: videoDelayTimer
        interval: 5000
        repeat: false
        onTriggered: {
            if ((root.selectedDetails.videoPath || "").length > 0)
                root.showVideo = true
        }
    }

    // ── Helpers ──
    function selectCurrentGame() {
        root.showVideo = false
        videoDelayTimer.stop()
        var d = themeContext.gameDetailsByIndex(root.listIndex)
        if (d && d.id !== undefined) {
            root.selectedDetails = d
            gameList.positionViewAtIndex(root.listIndex, ListView.Visible)
            videoDelayTimer.restart()
        }
    }

    function fanartSource() {
        var fa = root.selectedDetails.fanartPath || ""
        if (fa.length > 0) return "file://" + fa
        return "assets/artwork/" + themeContext.currentSystem + ".webp"
    }

    // ── Controller navigation ──
    Connections {
        target: inputManager
        function onNavigateUp() {
            if (root.listIndex > 0) {
                root.listIndex--
                root.selectCurrentGame()
            }
        }
        function onNavigateDown() {
            if (root.listIndex < gameModel.count - 1) {
                root.listIndex++
                root.selectCurrentGame()
            }
        }
        function onNavigateAccept() {
            if (root.selectedDetails && root.selectedDetails.id !== undefined)
                themeContext.launchGame(root.selectedDetails.id,
                                        root.selectedDetails.romPath,
                                        root.selectedDetails.emulatorId)
        }
        function onNavigateBack() {
            themeContext.navigateBack()
        }
    }

    // ── Keyboard navigation ──
    Keys.onUpPressed: {
        if (root.listIndex > 0) { root.listIndex--; root.selectCurrentGame() }
    }
    Keys.onDownPressed: {
        if (root.listIndex < gameModel.count - 1) { root.listIndex++; root.selectCurrentGame() }
    }
    Keys.onReturnPressed: {
        if (root.selectedDetails && root.selectedDetails.id !== undefined)
            themeContext.launchGame(root.selectedDetails.id,
                                    root.selectedDetails.romPath,
                                    root.selectedDetails.emulatorId)
    }
    Keys.onEnterPressed: {
        if (root.selectedDetails && root.selectedDetails.id !== undefined)
            themeContext.launchGame(root.selectedDetails.id,
                                    root.selectedDetails.romPath,
                                    root.selectedDetails.emulatorId)
    }
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Backspace) {
            themeContext.navigateBack()
            event.accepted = true
        }
    }

    // ── Refresh on data change ──
    Connections {
        target: themeContext
        function onGamesChanged() {
            if (gameModel.count > 0 && root.listIndex >= gameModel.count)
                root.listIndex = gameModel.count - 1
            if (root.selectedDetails && root.selectedDetails.id !== undefined)
                root.selectedDetails = themeContext.gameDetails(root.selectedDetails.id)
        }
    }

    // ════════════════════════════════════════════════════════════════
    // BACKGROUND — fanart of selected game (or system artwork fallback)
    // ════════════════════════════════════════════════════════════════
    Image {
        id: bgArt
        anchors.fill: parent
        source: root.fanartSource()
        fillMode: Image.PreserveAspectCrop
        opacity: 1.0

        onStatusChanged: {
            if (status === Image.Error)
                source = "assets/artwork/" + themeContext.currentSystem + ".webp"
        }

        Behavior on source { PropertyAnimation { target: bgArt; property: "opacity"; from: 0.6; to: 1.0; duration: 400 } }
    }

    // Darken overlay for readability
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
    }

    // ════════════════════════════════════════════════════════════════
    // LEFT DETAIL PANEL — ~47% width
    // ════════════════════════════════════════════════════════════════
    Item {
        id: detailPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: parent.width * 0.47
        anchors.margins: 24

        // ── Media Showcase Area ──
        Item {
            id: mediaArea
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height * 0.42

            // Screenshot / fanart background image
            Image {
                id: screenshotImg
                anchors.fill: parent
                source: {
                    var ss = root.selectedDetails.screenshotPath || ""
                    if (ss.length > 0) return "file://" + ss
                    return ""
                }
                fillMode: Image.PreserveAspectCrop
                visible: !root.showVideo && source.toString().length > 0
                opacity: visible ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 300 } }
            }

            // Video player (replaces screenshot after 5s)
            MediaPlayer {
                id: videoPlayer
                source: {
                    if (root.showVideo) {
                        var vp = root.selectedDetails.videoPath || ""
                        if (vp.length > 0) return "file://" + vp
                    }
                    return ""
                }
                loops: MediaPlayer.Infinite
                videoOutput: videoOutput
                onSourceChanged: {
                    if (source.toString().length > 0)
                        play()
                    else
                        stop()
                }
            }

            VideoOutput {
                id: videoOutput
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectCrop
                visible: root.showVideo && videoPlayer.playbackState === MediaPlayer.PlayingState
            }

            // No-media placeholder
            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(0, 0, 0, 0.3)
                visible: !screenshotImg.visible && !videoOutput.visible
                Text {
                    anchors.centerIn: parent
                    text: "No Media"
                    color: Qt.rgba(1, 1, 1, 0.3)
                    font.pixelSize: 16
                }
            }

            // Cover art overlay (bottom-left of media area)
            Image {
                id: coverArt
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 12
                anchors.bottomMargin: 12
                width: parent.width * 0.28
                height: parent.height * 0.75
                source: {
                    var cp = root.selectedDetails.coverPath || ""
                    if (cp.length > 0) return "file://" + cp
                    return ""
                }
                fillMode: Image.PreserveAspectFit
                visible: source.toString().length > 0 && !root.showVideo
            }

            // Physical media overlay (bottom-right of media area)
            Image {
                id: physicalMedia
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 12
                anchors.bottomMargin: 12
                width: parent.width * 0.15
                height: parent.width * 0.15
                source: {
                    var pm = root.selectedDetails.physicalmediaPath || ""
                    if (pm.length > 0) return "file://" + pm
                    return ""
                }
                fillMode: Image.PreserveAspectFit
                visible: source.toString().length > 0 && !root.showVideo
            }
        }

        // ── Title Bar ──
        Rectangle {
            id: titleBar
            anchors.top: mediaArea.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            height: 36
            radius: 6
            color: Qt.rgba(0, 0, 0, 0.65)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                text: "\uD83C\uDFAE  " + (root.selectedDetails.title || "")
                color: "#ffffff"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        // ── Metadata Grid ──
        Rectangle {
            id: metadataBox
            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 6
            height: metadataGrid.implicitHeight + 16
            radius: 6
            color: Qt.rgba(0, 0, 0, 0.65)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            visible: root.hasDetails

            Grid {
                id: metadataGrid
                anchors.fill: parent
                anchors.margins: 8
                columns: 3
                rowSpacing: 2
                columnSpacing: 4

                // Row 1
                Text {
                    width: metadataGrid.width / 3
                    text: {
                        var r = Math.round(root.selectedDetails.rating || 0)
                        var filled = "\u2605"   // ★
                        var empty  = "\u2606"   // ☆
                        return "\u2B50 " + filled.repeat(r) + empty.repeat(5 - r)
                    }
                    color: "#e0e0e0"; font.pixelSize: 12
                }
                Text {
                    width: metadataGrid.width / 3
                    text: "\uD83D\uDCCD " + (themeContext.systemNames[themeContext.currentSystem] || themeContext.currentSystem)
                    color: "#e0e0e0"; font.pixelSize: 12
                    elide: Text.ElideRight
                }
                Text {
                    width: metadataGrid.width / 3
                    color: "#e0e0e0"; font.pixelSize: 12
                    text: " "  // spacer for row alignment
                }

                // Row 1 separator
                Rectangle { width: metadataGrid.width; height: 1; color: Qt.rgba(1,1,1,0.08); Layout.columnSpan: 3 }
                Item { width: 1; height: 0 }
                Item { width: 1; height: 0 }

                // Row 2
                Text {
                    width: metadataGrid.width / 3
                    text: "\uD83D\uDCC5 " + (root.selectedDetails.releaseDate || "").substring(0, 7)
                    color: "#e0e0e0"; font.pixelSize: 12
                    elide: Text.ElideRight
                }
                Text {
                    width: metadataGrid.width / 3
                    text: "\u2699 " + (root.selectedDetails.developer || "")
                    color: "#e0e0e0"; font.pixelSize: 12
                    elide: Text.ElideRight
                }
                Text {
                    width: metadataGrid.width / 3
                    text: "\uD83D\uDC65 " + (root.selectedDetails.lastPlayed || "Never")
                    color: "#e0e0e0"; font.pixelSize: 12
                    elide: Text.ElideRight
                }

                // Row 2 separator
                Rectangle { width: metadataGrid.width; height: 1; color: Qt.rgba(1,1,1,0.08); Layout.columnSpan: 3 }
                Item { width: 1; height: 0 }
                Item { width: 1; height: 0 }

                // Row 3
                Text {
                    width: metadataGrid.width / 3
                    text: "\uD83D\uDC64 " + (root.selectedDetails.players || "1")
                    color: "#e0e0e0"; font.pixelSize: 12
                }
                Text {
                    width: metadataGrid.width / 3
                    text: "\uD83C\uDFEC " + (root.selectedDetails.publisher || "")
                    color: "#e0e0e0"; font.pixelSize: 12
                    elide: Text.ElideRight
                }
                Text {
                    width: metadataGrid.width / 3
                    text: "\u23F1 " + (root.selectedDetails.playCount > 0 ? root.selectedDetails.playCount + " plays" : "Unplayed")
                    color: "#e0e0e0"; font.pixelSize: 12
                }
            }
        }

        // ── Description Box (auto-scrolling) ──
        Rectangle {
            id: descriptionBox
            anchors.top: metadataBox.visible ? metadataBox.bottom : titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 6
            height: Math.min(descFlickable.contentHeight + 16, parent.height * 0.18)
            radius: 6
            color: Qt.rgba(0, 0, 0, 0.65)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            visible: (root.selectedDetails.description || "").length > 0
            clip: true

            Flickable {
                id: descFlickable
                anchors.fill: parent
                anchors.margins: 8
                contentHeight: descText.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: descText
                    width: descFlickable.width
                    text: root.selectedDetails.description || ""
                    color: Qt.rgba(1, 1, 1, 0.70)
                    font.pixelSize: 13
                    lineHeight: 1.4
                    wrapMode: Text.WordWrap
                }

                // Auto-scroll animation
                NumberAnimation on contentY {
                    id: autoScroll
                    running: false
                    from: 0
                    to: 0
                    duration: 0
                }
            }

            // Start auto-scroll when description overflows
            Timer {
                id: scrollStartTimer
                interval: 3000
                repeat: false
                onTriggered: {
                    var overflow = descFlickable.contentHeight - descFlickable.height
                    if (overflow > 0) {
                        autoScroll.from = 0
                        autoScroll.to = overflow
                        autoScroll.duration = overflow * 40  // ~40ms per pixel
                        autoScroll.running = true
                    }
                }
            }

            onVisibleChanged: {
                if (visible) {
                    descFlickable.contentY = 0
                    autoScroll.running = false
                    scrollStartTimer.restart()
                }
            }

            Connections {
                target: root
                function onSelectedDetailsChanged() {
                    descFlickable.contentY = 0
                    autoScroll.running = false
                    scrollStartTimer.restart()
                }
            }
        }

        // ── System Logo ──
        Image {
            id: systemLogo
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.bottomMargin: 8
            width: parent.width * 0.35
            height: 60
            source: "assets/gamepage-logos/" + themeContext.currentSystem + ".webp"
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignLeft
            visible: status === Image.Ready
        }
    }

    // ════════════════════════════════════════════════════════════════
    // RIGHT GAME LIST — inside dark rounded box
    // ════════════════════════════════════════════════════════════════
    Rectangle {
        id: gameListBox
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 24
        anchors.rightMargin: 24
        anchors.bottomMargin: 24
        width: parent.width * 0.47
        radius: 12
        color: Qt.rgba(0, 0, 0, 0.60)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        ListView {
            id: gameList
            anchors.fill: parent
            anchors.margins: 8
            clip: true

            model: gameModel
            currentIndex: root.listIndex

            highlightFollowsCurrentItem: true
            highlightMoveDuration: 150

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                width: 6
            }

            // Empty state
            Text {
                anchors.centerIn: parent
                text: "No games found"
                color: Qt.rgba(1, 1, 1, 0.50)
                font.pixelSize: 16
                visible: gameModel.count === 0
            }

            delegate: Item {
                id: listItem
                width: gameList.width
                height: 38

                required property int    gameId
                required property string title
                required property string romPath
                required property string emulatorId
                required property string coverPath
                required property int    index

                property bool isCurrent: root.listIndex === index

                // Highlight bar — white/light style like ES-DE
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    radius: 4
                    color: listItem.isCurrent ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
                    border.color: listItem.isCurrent ? Qt.rgba(1, 1, 1, 0.25) : "transparent"
                    border.width: listItem.isCurrent ? 1 : 0
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    text: listItem.title
                    color: listItem.isCurrent ? "#ffffff" : Qt.rgba(1, 1, 1, 0.70)
                    font.pixelSize: 15
                    font.weight: listItem.isCurrent ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor

                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            contextMenu.targetGameId = listItem.gameId
                            contextMenu.popup()
                        } else {
                            root.listIndex = listItem.index
                            root.selectCurrentGame()
                        }
                    }
                    onDoubleClicked: {
                        themeContext.launchGame(listItem.gameId, listItem.romPath, listItem.emulatorId)
                    }
                }
            }
        }
    }

    // ════════════════════════════════════════════════════════════════
    // CONTEXT MENU
    // ════════════════════════════════════════════════════════════════
    Menu {
        id: contextMenu
        property int targetGameId: -1

        MenuItem {
            text: "Scrape"
            onTriggered: {
                themeContext.scrapeGame(contextMenu.targetGameId)
                if (root.selectedDetails && root.selectedDetails.id === contextMenu.targetGameId)
                    root.selectedDetails = themeContext.gameDetails(contextMenu.targetGameId)
            }
        }
        MenuItem {
            text: "Toggle Favorite"
            onTriggered: {
                themeContext.toggleFavorite(contextMenu.targetGameId)
                if (root.selectedDetails && root.selectedDetails.id === contextMenu.targetGameId)
                    root.selectedDetails = themeContext.gameDetails(contextMenu.targetGameId)
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Remove from Library"
            onTriggered: {
                themeContext.removeGame(contextMenu.targetGameId)
                if (root.selectedDetails && root.selectedDetails.id === contextMenu.targetGameId)
                    root.selectedDetails = ({})
            }
        }
    }
}
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build 2>&1 | tail -5
```

Expected: Build succeeds. Note: QML is loaded at runtime so build only validates C++ changes.

- [ ] **Step 3: Run the app and visually verify**

```bash
./build/EmulatorFrontend
```

Check:
- Background shows fanart of selected game (or system artwork fallback)
- Game list appears in dark rounded box on right side
- Selected game has white/light highlight (not red)
- Left panel shows media showcase, title bar, metadata grid, description
- Video plays after 5 seconds if video file exists
- Description auto-scrolls if text overflows
- System logo appears at bottom-left if image exists in `assets/gamepage-logos/`
- Navigation (keyboard up/down/enter, controller) still works
- Context menu (right-click) still works

- [ ] **Step 4: Commit**

```bash
git add themes/modern/GameListPage.qml
git commit -m "feat: redesign GameListPage to ES-DE style with media showcase, metadata grid, and video playback"
```
