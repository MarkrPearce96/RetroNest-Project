# Elementerial Theme Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the Modern theme to match the ES-DE Elementerial style — full-screen artwork backgrounds, bottom system carousel with logos, text-based game list, and a minimal detail section that fills in when scrape data is available.

**Architecture:** Theme-only changes — no C++ modifications needed. Copy system logos, artwork, and star icons from `/references/elementerial-es-de/` into `themes/modern/assets/`. Rewrite `SystemPage.qml` and `GameListPage.qml` to match the Elementerial layout. The ThemeContext API and GameListModel roles remain unchanged.

**Tech Stack:** QML (Qt6), WebP images, SVG icons

---

## File Structure

```
themes/modern/
  theme.json              — no changes needed
  SystemPage.qml          — REWRITE: full-screen artwork, bottom carousel with logos
  GameListPage.qml        — REWRITE: text list, minimal detail that fills with scrape data
  assets/
    logos/                 — system logo WebP files (copied from references)
      psx.webp
      ps2.webp
      _default.webp        — fallback for unknown systems
    artwork/               — full-screen system artwork WebP files (copied from references)
      psx.webp
      ps2.webp
      _default.webp        — fallback
    icons/
      star-filled.svg      — rating star (filled)
      star-unfilled.svg    — rating star (outline)
```

### Asset Mapping

The app uses system IDs from manifests (`psx`, `ps2`). The reference theme uses identical filenames (`psx.webp`, `ps2.webp`) — direct copy, no renaming needed.

QML files are loaded from the theme directory via `themeManager.resolve()`, so relative paths like `"assets/logos/" + systemId + ".webp"` resolve correctly.

---

### Task 1: Copy Assets from Reference Theme

**Files:**
- Create: `themes/modern/assets/logos/psx.webp`
- Create: `themes/modern/assets/logos/ps2.webp`
- Create: `themes/modern/assets/artwork/psx.webp`
- Create: `themes/modern/assets/artwork/ps2.webp`
- Create: `themes/modern/assets/artwork/_default.webp`
- Create: `themes/modern/assets/icons/star-filled.svg`
- Create: `themes/modern/assets/icons/star-unfilled.svg`

- [ ] **Step 1: Create asset directories**

```bash
mkdir -p themes/modern/assets/{logos,artwork,icons}
```

- [ ] **Step 2: Copy system logos**

```bash
cp references/elementerial-es-de/_inc/systems/logos/psx.webp themes/modern/assets/logos/
cp references/elementerial-es-de/_inc/systems/logos/ps2.webp themes/modern/assets/logos/
```

- [ ] **Step 3: Copy system artwork**

```bash
cp "references/elementerial-es-de/_inc/systems/artwork (modern)/psx.webp" themes/modern/assets/artwork/
cp "references/elementerial-es-de/_inc/systems/artwork (modern)/ps2.webp" themes/modern/assets/artwork/
cp "references/elementerial-es-de/_inc/systems/artwork (modern)/_default.webp" themes/modern/assets/artwork/
```

- [ ] **Step 4: Copy star icons**

```bash
cp references/elementerial-es-de/_inc/images/icon-star-filled.svg themes/modern/assets/icons/star-filled.svg
cp references/elementerial-es-de/_inc/images/icon-star-unfilled.svg themes/modern/assets/icons/star-unfilled.svg
```

- [ ] **Step 5: Verify all assets exist**

```bash
ls -la themes/modern/assets/logos/
ls -la themes/modern/assets/artwork/
ls -la themes/modern/assets/icons/
```

Expected: 2 logos, 3 artworks (psx, ps2, _default), 2 SVGs.

- [ ] **Step 6: Commit**

```bash
git add themes/modern/assets/
git commit -m "feat: add Elementerial-style assets (logos, artwork, star icons)"
```

---

### Task 2: Rewrite SystemPage.qml

**Files:**
- Modify: `themes/modern/SystemPage.qml` (full rewrite)

**Design spec (matches ES-DE screenshots):**
- Full-screen system artwork as background image, crossfades when carousel index changes
- Dark gradient overlay on bottom-left (transparent top → dark bottom) for text readability
- System name in large bold white text (bottom-left area, above carousel)
- Game count + favourites count below system name in lighter text
- Horizontal card carousel pinned to bottom ~25% of screen
- Selected card: bright gradient (per-system color), shows system logo prominently
- Unselected cards: dark blue/navy, logo visible but muted
- Cards are ~300×200 rectangular with rounded corners
- Keyboard/controller navigation preserved (left/right arrows, enter to select)

**Per-system colors** (matching Elementerial palette):
- `psx` → green gradient (`#68B723` → `#4A9A10`)
- `ps2` → blue gradient (`#3689E6` → `#2570C7`)
- Fallback → purple (`#A56DE2` → `#7C3FC4`)

- [ ] **Step 1: Write the new SystemPage.qml**

Full rewrite — replace entire file with new Elementerial-style layout:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true

    Component.onCompleted: root.forceActiveFocus()
    StackView.onActivated: root.forceActiveFocus()

    property var systemList: themeContext.systems
    property var systemNames: themeContext.systemNames
    property var systemCounts: themeContext.systemGameCounts

    // Per-system accent colors
    function systemColor(sysId) {
        var colors = {
            "psx":  ["#68B723", "#4A9A10"],
            "ps2":  ["#3689E6", "#2570C7"],
            "psp":  ["#F37329", "#D45A10"],
            "nes":  ["#ED5353", "#CC3333"],
            "snes": ["#A56DE2", "#7C3FC4"],
            "n64":  ["#DE3E80", "#B82060"],
            "gb":   ["#68B723", "#4A9A10"],
            "gbc":  ["#F9C440", "#D4A520"],
            "gba":  ["#3689E6", "#2570C7"],
            "genesis": ["#ED5353", "#CC3333"],
            "dreamcast": ["#28BCA3", "#1A9A85"],
            "arcade": ["#F37329", "#D45A10"]
        }
        return colors[sysId] || ["#A56DE2", "#7C3FC4"]
    }

    // Re-read when systems or games change
    Connections {
        target: themeContext
        function onSystemsChanged() {
            root.systemList = themeContext.systems
            root.systemNames = themeContext.systemNames
            root.systemCounts = themeContext.systemGameCounts
        }
        function onGamesChanged() {
            root.systemList = themeContext.systems
            root.systemNames = themeContext.systemNames
            root.systemCounts = themeContext.systemGameCounts
        }
    }

    // Keyboard navigation
    Keys.onLeftPressed:  { if (systemList.length > 0) carousel.decrementCurrentIndex() }
    Keys.onRightPressed: { if (systemList.length > 0) carousel.incrementCurrentIndex() }
    Keys.onReturnPressed: {
        if (systemList.length > 0)
            themeContext.navigateToSystem(systemList[carousel.currentIndex])
    }
    Keys.onEnterPressed: {
        if (systemList.length > 0)
            themeContext.navigateToSystem(systemList[carousel.currentIndex])
    }

    // Controller navigation
    Connections {
        target: inputManager
        function onNavigateLeft()   { if (systemList.length > 0) carousel.decrementCurrentIndex() }
        function onNavigateRight()  { if (systemList.length > 0) carousel.incrementCurrentIndex() }
        function onNavigateAccept() {
            if (systemList.length > 0)
                themeContext.navigateToSystem(systemList[carousel.currentIndex])
        }
    }

    // === BACKGROUND ARTWORK ===
    // Two images for crossfade
    Image {
        id: bgArtA
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: 1.0
        source: ""
    }
    Image {
        id: bgArtB
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: 0.0
        source: ""
    }

    property bool useArtA: true
    property string currentArtworkSystem: ""

    function updateArtwork() {
        if (systemList.length === 0) return
        var sysId = systemList[carousel.currentIndex]
        if (sysId === currentArtworkSystem) return
        currentArtworkSystem = sysId

        var artPath = "assets/artwork/" + sysId + ".webp"
        var fallback = "assets/artwork/_default.webp"

        if (useArtA) {
            bgArtB.source = artPath
            bgArtB.opacity = 1.0
            bgArtA.opacity = 0.0
            useArtA = false
        } else {
            bgArtA.source = artPath
            bgArtA.opacity = 1.0
            bgArtB.opacity = 0.0
            useArtA = true
        }
    }

    Behavior on Item { }  // placeholder
    // Crossfade behaviors on both images
    Connections {
        target: bgArtA
        // Use onStatusChanged to swap to fallback if image fails
        function onStatusChanged() {
            if (bgArtA.status === Image.Error && bgArtA.source.toString().indexOf("_default") === -1) {
                bgArtA.source = "assets/artwork/_default.webp"
            }
        }
    }
    Connections {
        target: bgArtB
        function onStatusChanged() {
            if (bgArtB.status === Image.Error && bgArtB.source.toString().indexOf("_default") === -1) {
                bgArtB.source = "assets/artwork/_default.webp"
            }
        }
    }

    Behavior on opacity { NumberAnimation { duration: 400 } }

    // Crossfade behaviors
    NumberAnimation { id: fadeInA;  target: bgArtA; property: "opacity"; duration: 500; easing.type: Easing.InOutQuad }
    NumberAnimation { id: fadeInB;  target: bgArtB; property: "opacity"; duration: 500; easing.type: Easing.InOutQuad }

    // Dark gradient overlay (bottom half)
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 0.4; color: "#80000000" }
            GradientStop { position: 0.7; color: "#CC111111" }
            GradientStop { position: 1.0; color: "#FF111111" }
        }
    }

    // === SYSTEM INFO TEXT (bottom-left, above carousel) ===
    Column {
        anchors.left: parent.left
        anchors.leftMargin: 80
        anchors.bottom: carouselContainer.top
        anchors.bottomMargin: 24
        spacing: 4
        visible: systemList.length > 0

        Text {
            text: root.systemNames[systemList[carousel.currentIndex]] || ""
            color: "#ffffff"
            font.pixelSize: 42
            font.weight: Font.Bold
            font.family: "Inter"
        }

        Text {
            text: {
                var count = root.systemCounts[systemList[carousel.currentIndex]] || 0
                return count + " Games (0 Favourites)"
            }
            color: "#bbbbbb"
            font.pixelSize: 18
            font.family: "Inter"
        }
    }

    // === BOTTOM CAROUSEL ===
    Item {
        id: carouselContainer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        height: 200

        ListView {
            id: carousel
            anchors.fill: parent
            anchors.leftMargin: 60
            anchors.rightMargin: 60
            orientation: ListView.Horizontal
            spacing: 20
            clip: false
            model: systemList
            currentIndex: 0
            highlightFollowsCurrentItem: true
            highlightMoveDuration: 300
            preferredHighlightBegin: 0
            preferredHighlightEnd: carousel.width * 0.4

            onCurrentIndexChanged: root.updateArtwork()
            Component.onCompleted: root.updateArtwork()

            delegate: Item {
                id: cardDel
                width: 300
                height: 180

                required property string modelData
                required property int index

                property bool isCurrent: ListView.isCurrentItem
                property var colors: root.systemColor(modelData)

                Rectangle {
                    anchors.fill: parent
                    radius: 16

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: cardDel.isCurrent ? cardDel.colors[0] : "#1E2D5B" }
                        GradientStop { position: 1.0; color: cardDel.isCurrent ? cardDel.colors[1] : "#162248" }
                    }

                    opacity: cardDel.isCurrent ? 1.0 : 0.6

                    Behavior on opacity { NumberAnimation { duration: 200 } }

                    // System logo
                    Image {
                        id: logoImg
                        anchors.centerIn: parent
                        width: parent.width * 0.6
                        height: parent.height * 0.6
                        fillMode: Image.PreserveAspectFit
                        source: "assets/logos/" + cardDel.modelData + ".webp"
                        asynchronous: true
                        opacity: cardDel.isCurrent ? 1.0 : 0.4

                        Behavior on opacity { NumberAnimation { duration: 200 } }

                        // Fallback to text if no logo
                        visible: status === Image.Ready
                    }

                    // Text fallback when no logo
                    Text {
                        anchors.centerIn: parent
                        text: root.systemNames[cardDel.modelData] || cardDel.modelData
                        color: "#ffffff"
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        visible: logoImg.status !== Image.Ready
                        opacity: cardDel.isCurrent ? 1.0 : 0.5
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (cardDel.isCurrent) {
                                themeContext.navigateToSystem(cardDel.modelData)
                            } else {
                                carousel.currentIndex = cardDel.index
                            }
                        }
                        onDoubleClicked: {
                            themeContext.navigateToSystem(cardDel.modelData)
                        }
                    }
                }
            }
        }
    }

    // === EMPTY STATE ===
    Column {
        anchors.centerIn: parent
        spacing: 24
        visible: systemList.length === 0

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "No games found"
            color: "#ffffff"
            font.pixelSize: 28
            font.weight: Font.Bold
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Add ROMs to your system folders and scan to get started."
            color: "#888888"
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            Rectangle {
                width: scanText.implicitWidth + 32
                height: 44
                radius: 10
                color: scanMa.containsMouse ? "#7BC735" : "#68B723"

                Text {
                    id: scanText
                    anchors.centerIn: parent
                    text: "Scan ROM Folders"
                    color: "#ffffff"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    id: scanMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: themeContext.scanRomFolders()
                }
            }

            Rectangle {
                width: importText.implicitWidth + 32
                height: 44
                radius: 10
                color: importMa.containsMouse ? "#333344" : "#222233"
                border.color: "#444455"
                border.width: 1

                Text {
                    id: importText
                    anchors.centerIn: parent
                    text: "Import ROMs"
                    color: "#aaaaaa"
                    font.pixelSize: 14
                }
                MouseArea {
                    id: importMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: themeContext.importRoms()
                }
            }
        }
    }
}
```

- [ ] **Step 2: Build and verify**

```bash
cd cpp && cmake --build build
./build/EmulatorFrontend
```

Expected: Full-screen artwork behind system carousel at bottom with logo cards.

- [ ] **Step 3: Commit**

```bash
git add themes/modern/SystemPage.qml
git commit -m "feat: redesign SystemPage with Elementerial-style artwork and carousel"
```

---

### Task 3: Rewrite GameListPage.qml

**Files:**
- Modify: `themes/modern/GameListPage.qml` (full rewrite)

**Design spec (matches ES-DE screenshots):**
- Full-screen system artwork as background (same artwork as system page)
- Dark gradient overlay on left side for text readability
- System name in bold white at top-left
- Simple text list of games on the left side (~40% of screen width)
- Selected game highlighted with a colored bar (coral/salmon: `#E8686A`)
- Non-selected games in plain white text
- Right side: minimal detail area — empty when no scrape data
- When scraped: shows game description, developer, release date, rating stars
- Star rating at bottom-right using the star SVGs
- No header bar, no toolbar buttons — clean fullscreen experience
- Context menu on right-click preserved for Scrape/Favorite/Remove
- Keyboard/controller navigation: up/down through list, enter to launch, backspace to go back

**Detail section behavior:**
- If game has NO scraped data (no description, no developer, no rating): just show the title, otherwise blank — minimal
- If game HAS scraped data: show description, developer, publisher, release date, genres, rating stars, players
- Title always shows the `title` field (which the scraper updates to the proper game name)

- [ ] **Step 1: Write the new GameListPage.qml**

Full rewrite — replace entire file with new Elementerial-style layout:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true

    Component.onCompleted: root.forceActiveFocus()
    StackView.onActivated: root.forceActiveFocus()

    property int listIndex: 0
    property var selectedDetails: ({})
    property bool hasDetails: {
        var d = selectedDetails
        return (d.description || "") !== "" ||
               (d.developer || "") !== "" ||
               (d.rating || 0) > 0
    }

    function selectCurrentGame() {
        var details = themeContext.gameDetailsByIndex(root.listIndex)
        if (details && details.id !== undefined) {
            root.selectedDetails = details
        } else {
            root.selectedDetails = {}
        }
    }

    Component.onCompleted: {
        if (gameModel.count > 0) selectCurrentGame()
    }

    // Controller navigation
    Connections {
        target: inputManager
        function onNavigateUp() {
            if (root.listIndex > 0) { root.listIndex--; root.selectCurrentGame() }
        }
        function onNavigateDown() {
            if (root.listIndex < gameModel.count - 1) { root.listIndex++; root.selectCurrentGame() }
        }
        function onNavigateAccept() {
            if (selectedDetails.id !== undefined)
                themeContext.launchGame(selectedDetails.id, selectedDetails.romPath, selectedDetails.emulatorId)
        }
        function onNavigateBack() {
            themeContext.navigateBack()
        }
    }

    // Keyboard navigation
    Keys.onUpPressed: {
        if (root.listIndex > 0) { root.listIndex--; root.selectCurrentGame() }
    }
    Keys.onDownPressed: {
        if (root.listIndex < gameModel.count - 1) { root.listIndex++; root.selectCurrentGame() }
    }
    Keys.onReturnPressed: {
        if (selectedDetails.id !== undefined)
            themeContext.launchGame(selectedDetails.id, selectedDetails.romPath, selectedDetails.emulatorId)
    }
    Keys.onEnterPressed: {
        if (selectedDetails.id !== undefined)
            themeContext.launchGame(selectedDetails.id, selectedDetails.romPath, selectedDetails.emulatorId)
    }
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Backspace) {
            themeContext.navigateBack()
            event.accepted = true
        }
    }

    // === BACKGROUND ARTWORK ===
    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        source: "assets/artwork/" + themeContext.currentSystem + ".webp"
        asynchronous: true

        onStatusChanged: {
            if (status === Image.Error) {
                source = "assets/artwork/_default.webp"
            }
        }
    }

    // Dark gradient overlay (left side heavy)
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#E0111111" }
            GradientStop { position: 0.5; color: "#A0111111" }
            GradientStop { position: 0.8; color: "#60111111" }
            GradientStop { position: 1.0; color: "#40111111" }
        }
    }

    // Additional vertical gradient for bottom
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 0.85; color: "#80111111" }
            GradientStop { position: 1.0; color: "#CC111111" }
        }
    }

    // === SYSTEM TITLE ===
    Text {
        id: systemTitle
        anchors.top: parent.top
        anchors.topMargin: 40
        anchors.left: parent.left
        anchors.leftMargin: 60
        text: themeContext.systemNames[themeContext.currentSystem] || themeContext.currentSystem
        color: "#ffffff"
        font.pixelSize: 36
        font.weight: Font.Bold
        font.family: "Inter"
    }

    // === GAME LIST (left side) ===
    ListView {
        id: gameList
        anchors.top: systemTitle.bottom
        anchors.topMargin: 24
        anchors.left: parent.left
        anchors.leftMargin: 60
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 80
        width: parent.width * 0.35

        model: gameModel
        currentIndex: root.listIndex
        clip: true
        spacing: 2
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 150

        onCurrentIndexChanged: {
            root.listIndex = currentIndex
            root.selectCurrentGame()
        }

        delegate: Item {
            width: gameList.width
            height: 40

            required property int gameId
            required property string title
            required property string romPath
            required property string emulatorId
            required property int index

            property bool isCurrent: root.listIndex === index

            // Highlight bar
            Rectangle {
                anchors.fill: parent
                color: "#E8686A"
                opacity: isCurrent ? 0.9 : 0.0
                radius: 2

                Behavior on opacity { NumberAnimation { duration: 120 } }
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 8
                text: title
                color: "#ffffff"
                font.pixelSize: 16
                font.weight: isCurrent ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        contextMenu.gameId = gameId
                        contextMenu.popup()
                    } else {
                        root.listIndex = index
                        root.selectCurrentGame()
                    }
                }
                onDoubleClicked: {
                    themeContext.launchGame(gameId, romPath, emulatorId)
                }
            }
        }

        // Empty state
        Text {
            anchors.centerIn: parent
            text: "No games found.\nUse Escape → Settings to scan or import."
            color: "#888888"
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            visible: gameModel.count === 0
        }
    }

    // === DETAIL SECTION (right side, minimal) ===
    // Only shows content when scraped data exists
    Column {
        anchors.right: parent.right
        anchors.rightMargin: 60
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        width: parent.width * 0.35
        spacing: 12
        visible: root.hasDetails

        // Description
        Text {
            width: parent.width
            text: root.selectedDetails.description || ""
            color: "#cccccc"
            font.pixelSize: 14
            font.family: "Inter"
            wrapMode: Text.WordWrap
            maximumLineCount: 6
            elide: Text.ElideRight
            lineHeight: 1.4
            visible: (root.selectedDetails.description || "") !== ""
        }

        // Metadata row: Developer / Publisher / Year
        Text {
            width: parent.width
            text: {
                var parts = []
                if (root.selectedDetails.developer)
                    parts.push(root.selectedDetails.developer)
                if (root.selectedDetails.publisher &&
                    root.selectedDetails.publisher !== root.selectedDetails.developer)
                    parts.push(root.selectedDetails.publisher)
                if (root.selectedDetails.releaseDate) {
                    var year = root.selectedDetails.releaseDate.substring(0, 4)
                    if (year) parts.push(year)
                }
                return parts.join("  ·  ")
            }
            color: "#999999"
            font.pixelSize: 13
            font.family: "Inter"
            visible: text !== ""
        }

        // Genre tags
        Text {
            width: parent.width
            text: root.selectedDetails.genres || ""
            color: "#888888"
            font.pixelSize: 12
            font.family: "Inter"
            visible: (root.selectedDetails.genres || "") !== ""
        }

        // Players
        Text {
            text: (root.selectedDetails.players || "") !== "" ?
                  root.selectedDetails.players + " Player(s)" : ""
            color: "#888888"
            font.pixelSize: 12
            visible: text !== ""
        }
    }

    // === STAR RATING (bottom-right) ===
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 60
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 30
        spacing: 4
        visible: (root.selectedDetails.rating || 0) > 0

        Repeater {
            model: 5

            Image {
                width: 28
                height: 28
                source: (index < Math.round(root.selectedDetails.rating || 0)) ?
                    "assets/icons/star-filled.svg" : "assets/icons/star-unfilled.svg"
                opacity: 0.8
            }
        }
    }

    // Rating stars for when NO rating data but game is selected
    // (shows empty stars like in the ES-DE screenshot)
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 60
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 30
        spacing: 4
        visible: (root.selectedDetails.rating || 0) === 0 &&
                 root.selectedDetails.id !== undefined

        Repeater {
            model: 5

            Image {
                width: 28
                height: 28
                source: "assets/icons/star-unfilled.svg"
                opacity: 0.5
            }
        }
    }

    // === CONTEXT MENU ===
    Menu {
        id: contextMenu
        property int gameId: -1

        MenuItem {
            text: "Scrape"
            onTriggered: {
                themeContext.scrapeGame(contextMenu.gameId)
                root.selectCurrentGame()
            }
        }
        MenuItem {
            text: "Toggle Favorite"
            onTriggered: {
                themeContext.toggleFavorite(contextMenu.gameId)
                root.selectCurrentGame()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Remove from Library"
            onTriggered: {
                themeContext.removeGame(contextMenu.gameId)
                root.selectedDetails = {}
            }
        }
    }

    // Refresh when games change (e.g. after scrape)
    Connections {
        target: themeContext
        function onGamesChanged() {
            root.selectCurrentGame()
        }
    }
}
```

- [ ] **Step 2: Build and verify**

```bash
cd cpp && cmake --build build
./build/EmulatorFrontend
```

Expected: Text-based game list on left, artwork background, minimal detail that shows when scraped data exists, star rating at bottom-right.

- [ ] **Step 3: Commit**

```bash
git add themes/modern/GameListPage.qml
git commit -m "feat: redesign GameListPage with Elementerial-style text list and minimal detail"
```

---

### Task 4: Polish and Visual Tweaks

**Files:**
- Modify: `themes/modern/SystemPage.qml` (tweaks if needed)
- Modify: `themes/modern/GameListPage.qml` (tweaks if needed)

- [ ] **Step 1: Run the app and verify both pages**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```

Check:
- SystemPage: artwork loads, crossfades between systems, logos show in cards, colors match per-system
- GameListPage: text list scrolls, selection bar works, detail fills when game has scraped data, stars show
- Navigation: arrow keys, enter, backspace, escape all work
- Controller: d-pad, accept, back buttons work

- [ ] **Step 2: Fix any visual issues found during testing**

Adjust spacing, font sizes, opacity, gradient stops as needed based on actual rendering.

- [ ] **Step 3: Final commit**

```bash
git add themes/modern/
git commit -m "feat: polish Elementerial-style theme visuals"
```
