import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: overlay
    anchors.fill: parent
    visible: false
    z: 100

    property int selectedCategory: -1   // -1 = category list, 0+ = category page
    property int _savedFocusIndex: 0
    readonly property int categoryCount: 9
    property bool exitDialogVisible: false

    // Category titles shown in the sub-page header (index matches selectedCategory)
    readonly property var _categoryTitles: [
        "Emulators", "Paths", "Scraper", "Themes",
        "Resolution", "Aspect Ratio", "Achievements", "Libretro Hotkeys", "Settings"
    ]

    onExitDialogVisibleChanged: {
        if (exitDialogVisible) {
            exitDialogFocus.forceActiveFocus()
        } else {
            categoryListFocusTimer.start()
        }
    }

    // Cursor visibility while the overlay is open comes from AppWindow's
    // cursorNeeded policy (bound to panelOpen) — no calls here.
    function open() {
        selectedCategory = -1
        panelStack.clear()
        panelStack.push(categoryListComponent)
        visible = true
        panelOpen = true
        overlay.forceActiveFocus()
        categoryListFocusTimer.start()
    }

    function close() {
        panelOpen = false
        // visibility cleared by slide-out animation end
        // Return focus to the main content after slide-out
        focusReturnTimer.start()
    }

    Timer {
        id: focusReturnTimer
        interval: 300  // after slide-out animation completes
        onTriggered: {
            if (mainStack.currentItem) {
                mainStack.currentItem.forceActiveFocus()
            }
        }
    }

    function isBusy() {
        // Check if the current page has an active operation that shouldn't be interrupted
        var current = panelStack.currentItem
        return current && current.scrapeRunning !== undefined && current.scrapeRunning
    }

    function canGoBack() {
        if (isBusy()) return false
        var current = panelStack.currentItem
        if (current && current.canGoBackInternal === true) return true
        return panelStack.depth > 1
    }

    function goBack() {
        // Page-internal navigation first — a page exposing
        // canGoBackInternal/goBackInternal() consumes back before the
        // stack pops (same current-page convention as isBusy). Needed for
        // EmulatorManagePage, whose grid↔detail drill-down is internal
        // state on ONE stack entry: popping instead would skip the grid
        // and land on the category list.
        var current = panelStack.currentItem
        if (current && current.canGoBackInternal === true
                && typeof current.goBackInternal === "function") {
            current.goBackInternal()
            return
        }
        if (panelStack.depth > 1) {
            panelStack.pop()
            if (panelStack.depth <= 1) {
                // Back to category list
                selectedCategory = -1
                categoryListFocusTimer.start()
            } else {
                // Still in sub-pages, keep current page focused
                scraperFocusTimer.start()
            }
        }
    }

    function navigateToScraper() {
        if (!visible) {
            // Open directly to scraper page
            panelStack.clear()
            panelStack.push(scraperPageComponent)
            visible = true
            panelOpen = true
            overlay.forceActiveFocus()
        } else {
            panelStack.push(scraperPageComponent)
        }
        selectedCategory = 2
        scraperFocusTimer.start()
    }

    Timer {
        id: scraperFocusTimer
        interval: 50
        onTriggered: {
            if (panelStack.currentItem)
                panelStack.currentItem.forceActiveFocus()
        }
    }

    function navigateToAchievements(raGameId, gameTitle) {
        if (!visible) {
            panelStack.clear()
            panelStack.push(raPageComponent)
            panelStack.push(achievementsPageComponent, { raGameId: raGameId, gameTitle: gameTitle })
            visible = true
            panelOpen = true
            overlay.forceActiveFocus()
        } else {
            panelStack.push(achievementsPageComponent, { raGameId: raGameId, gameTitle: gameTitle })
        }
        selectedCategory = 6
        scraperFocusTimer.start()
    }

    // --- private ---
    property bool panelOpen: false

    Timer {
        id: categoryListFocusTimer
        interval: 50
        onTriggered: {
            var item = panelStack.currentItem
            if (item && item.objectName === "categoryList") {
                item.forceActiveFocus()
            }
        }
    }

    // Semi-transparent left area
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: overlay.panelOpen ? 0.65 : 0
        Behavior on opacity { NumberAnimation { duration: SettingsTheme.animSlide } }

        MouseArea {
            anchors.fill: parent
            onClicked: overlay.close()
        }
    }

    // Shadow behind panel
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: panel.width + 24
        x: panel.x - 24
        color: "transparent"

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.4
            radius: 0
        }
    }

    // Slide panel
    Rectangle {
        id: panel
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * SettingsTheme.panelWidthPercent / 100
        x: overlay.panelOpen ? parent.width - width : parent.width
        color: SettingsTheme.surface

        Behavior on x {
            NumberAnimation {
                duration: SettingsTheme.animSlide
                easing.type: Easing.OutCubic
            }
        }

        // When slide-out completes, hide the overlay
        onXChanged: {
            if (!overlay.panelOpen && x >= overlay.width) {
                overlay.visible = false
                panelStack.clear()
            }
        }

        // Left border
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: SettingsTheme.border
        }

        // Prevent click-through
        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            // Header (visible when depth > 1)
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 56 : 0
                visible: panelStack.depth > 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12

                    // Back button
                    Rectangle {
                        width: 32
                        height: 32
                        radius: 6
                        color: backMa.containsMouse ? Qt.lighter(SettingsTheme.card, 1.2) : SettingsTheme.card

                        Text {
                            anchors.centerIn: parent
                            text: "\u2190"
                            color: SettingsTheme.accent
                            font.pixelSize: 16
                        }
                        MouseArea {
                            id: backMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: overlay.goBack()
                        }
                    }

                    Text {
                        text: overlay._categoryTitles[overlay.selectedCategory] || "Settings"
                        color: SettingsTheme.text
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    Item { Layout.fillWidth: true }
                }

                // Bottom border
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: SettingsTheme.border
                }
            }

            // StackView for content pages
            StackView {
                id: panelStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                // Focus is managed by dedicated timers (categoryListFocusTimer,
                // scraperFocusTimer) which fire after the push animation settles.

                pushEnter: Transition {
                    NumberAnimation { property: "x"; from: panelStack.width; to: 0; duration: SettingsTheme.animNormal; easing.type: Easing.OutCubic }
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: SettingsTheme.animNormal }
                }
                pushExit: Transition {
                    NumberAnimation { property: "x"; from: 0; to: -panelStack.width * 0.3; duration: SettingsTheme.animNormal; easing.type: Easing.OutCubic }
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SettingsTheme.animNormal }
                }
                popEnter: Transition {
                    NumberAnimation { property: "x"; from: -panelStack.width * 0.3; to: 0; duration: SettingsTheme.animNormal; easing.type: Easing.OutCubic }
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: SettingsTheme.animNormal }
                }
                popExit: Transition {
                    NumberAnimation { property: "x"; from: 0; to: panelStack.width; duration: SettingsTheme.animNormal; easing.type: Easing.OutCubic }
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SettingsTheme.animNormal }
                }

                // B-button (Escape) goes back from sub-pages
                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Escape || event.key === Qt.Key_Back) && overlay.canGoBack()) {
                        overlay.goBack()
                        event.accepted = true
                    }
                }
            }

            // Button hints
            ButtonHints {
                Layout.fillWidth: true
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
        }
    }

    // --- Category list component ---
    Component {
        id: categoryListComponent

        Item {
            objectName: "categoryList"
            property int focusIndex: overlay._savedFocusIndex

            Keys.onUpPressed: {
                focusIndex = (focusIndex - 1 + overlay.categoryCount) % overlay.categoryCount
            }
            Keys.onDownPressed: {
                focusIndex = (focusIndex + 1) % overlay.categoryCount
            }
            Keys.onReturnPressed: selectCategory(focusIndex)
            Keys.onEnterPressed: selectCategory(focusIndex)
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                    overlay.close()
                    event.accepted = true
                }
            }

            function selectCategory(idx) {
                overlay._savedFocusIndex = focusIndex
                overlay.selectedCategory = idx
                // Index 7 opens the global Libretro Hotkeys dialog directly
                // (not a pushed sub-page). Index 8 is the Exit entry.
                if (idx === 7) {
                    app.showLibretroHotkeySettings()
                    return
                }
                if (idx === 8) {
                    overlay.exitDialogVisible = true
                    return
                }
                var pages = [emuPageComponent, pathsPageComponent, scraperPageComponent,
                             themesPageComponent, resolutionPageComponent,
                             aspectRatioPageComponent, raPageComponent]
                if (idx >= 0 && idx < pages.length)
                    panelStack.push(pages[idx])
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 0

                // Settings title
                Text {
                    text: "Settings"
                    color: SettingsTheme.text
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    Layout.bottomMargin: 20
                }

                // Category items
                Repeater {
                    model: ListModel {
                        ListElement { name: "Emulators";  icon: "\uD83C\uDFAE"; subtitle: "Manage installations & BIOS"; catIndex: 0 }
                        ListElement { name: "Paths";      icon: "\uD83D\uDCC1"; subtitle: "Configure folder locations";  catIndex: 1 }
                        ListElement { name: "Scraper";    icon: "\uD83D\uDD0D"; subtitle: "Download metadata & artwork"; catIndex: 2 }
                        ListElement { name: "Themes";     icon: "\uD83C\uDFA8"; subtitle: "Choose visual theme";         catIndex: 3 }
                        ListElement { name: "Resolution";    icon: "\uD83D\uDDA5"; subtitle: "Quick resolution settings";    catIndex: 4 }
                        ListElement { name: "Aspect Ratio";  icon: "\u2B1C";       subtitle: "Quick aspect ratio settings";  catIndex: 5 }
                        ListElement { name: "Achievements";  icon: "\uD83C\uDFC6"; subtitle: "RetroAchievements login & progress"; catIndex: 6 }
                        ListElement { name: "Libretro Hotkeys"; icon: "\u2328";    subtitle: "Keyboard & gamepad shortcuts for libretro cores"; catIndex: 7 }
                        ListElement { name: "Exit";          icon: "\u23FB";        subtitle: "Close the application";        catIndex: 8 }
                    }

                    FocusableItem {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        Layout.bottomMargin: 10
                        isFocused: focusIndex === model.catIndex

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 22
                            anchors.rightMargin: 22
                            spacing: 20

                            Text {
                                text: model.icon
                                font.pixelSize: 32
                            }

                            ColumnLayout {
                                spacing: 3

                                Text {
                                    text: model.name
                                    color: SettingsTheme.text
                                    font.pixelSize: 18
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: model.subtitle
                                    color: SettingsTheme.textMuted
                                    font.pixelSize: 13
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: "\u203A"
                                color: SettingsTheme.textDim
                                font.pixelSize: 22
                                visible: model.catIndex !== 8
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onContainsMouseChanged: parent.isHovered = containsMouse
                            onClicked: selectCategory(model.catIndex)
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    // --- Page components ---
    Component {
        id: emuPageComponent
        EmulatorManagePage {}
    }

    Component {
        id: pathsPageComponent
        PathsSettings {}
    }

    Component {
        id: scraperPageComponent
        ScraperSettings {}
    }

    Component {
        id: themesPageComponent
        ThemesPage {}
    }

    Component {
        id: resolutionPageComponent
        ResolutionSettings {}
    }

    Component {
        id: aspectRatioPageComponent
        AspectRatioSettings {}
    }

    // RA pages navigate via explicit signals (pushRequested/backRequested)
    // — the overlay owns panelStack and the page components; the pages
    // themselves never reach into these ids by dynamic scoping.
    function pushRaPage(page, props) {
        if (page === "achievements")
            panelStack.push(achievementsPageComponent, props)
        else if (page === "allGames")
            panelStack.push(allGamesPageComponent, props)
        else if (page === "recentlyPlayed")
            panelStack.push(recentlyPlayedPageComponent, props)
        else
            console.warn("[SettingsOverlay] Unknown RA page:", page)
    }

    Component {
        id: raPageComponent
        RetroAchievementsSettings {
            onPushRequested: (page, props) => overlay.pushRaPage(page, props)
        }
    }

    Component {
        id: achievementsPageComponent
        AchievementsPage {
            onBackRequested: overlay.goBack()
        }
    }

    Component {
        id: allGamesPageComponent
        AllGamesPage {
            onPushRequested: (page, props) => overlay.pushRaPage(page, props)
        }
    }

    Component {
        id: recentlyPlayedPageComponent
        RecentlyPlayedPage {
            onPushRequested: (page, props) => overlay.pushRaPage(page, props)
        }
    }

    // --- Exit confirmation dialog ---
    Rectangle {
        id: exitDialog
        anchors.fill: parent
        color: "#000000"
        opacity: overlay.exitDialogVisible ? 0.7 : 0
        visible: opacity > 0
        z: 200

        Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }

        MouseArea {
            anchors.fill: parent
            onClicked: overlay.exitDialogVisible = false
        }

        FocusScope {
            id: exitDialogFocus
            anchors.centerIn: parent
            width: 320
            height: dialogCol.height + 48
            focus: overlay.exitDialogVisible
            z: 201

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                    overlay.exitDialogVisible = false
                    event.accepted = true
                }
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    Qt.quit()
                    event.accepted = true
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: 12
                color: SettingsTheme.surface
                border.width: 1
                border.color: SettingsTheme.border

                Column {
                    id: dialogCol
                    anchors.centerIn: parent
                    spacing: 24

                    Text {
                        text: "Exit Application?"
                        color: SettingsTheme.text
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Row {
                        spacing: 12
                        anchors.horizontalCenter: parent.horizontalCenter

                        // Cancel button
                        Rectangle {
                            width: 100
                            height: 36
                            radius: SettingsTheme.buttonRadius
                            color: SettingsTheme.card
                            border.width: 1
                            border.color: SettingsTheme.border

                            Text {
                                anchors.centerIn: parent
                                text: "Cancel"
                                color: SettingsTheme.text
                                font.pixelSize: 14
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: overlay.exitDialogVisible = false
                            }
                        }

                        // Exit button
                        Rectangle {
                            width: 100
                            height: 36
                            radius: SettingsTheme.buttonRadius
                            color: SettingsTheme.accent

                            Text {
                                anchors.centerIn: parent
                                text: "Exit"
                                color: "#ffffff"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: Qt.quit()
                            }
                        }
                    }
                }
            }
        }
    }
}
