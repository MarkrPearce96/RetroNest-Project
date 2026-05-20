import QtQuick
import QtQuick.Controls

BaseModalCard {
    id: popup
    cardWidth: 400
    cardHeight: titleText.anchors.topMargin + titleText.height + 8 +
                (popupState === "actions" ? actionColumn.height : confirmColumn.height) + 16

    property int targetGameId: -1
    property bool isFavorite: false
    property string gameTitle: ""

    // State: "actions" shows action list, "confirm" shows delete confirmation
    property string popupState: "actions"
    property int focusIndex: 0
    property bool canScrape: themeContext.hasScraperCredentials()
    property int raGameId: 0

    // Single source of truth for the action list — used both by the visible
    // Repeater and the keyboard handler.
    readonly property var actions: [
        { label: "Scrape", actionId: "scrape", destructive: false },
        { label: "Achievements", actionId: "achievements", destructive: false },
        { label: popup.isFavorite ? "Remove from Favorites" : "Add to Favorites",
          actionId: "favorite", destructive: false },
        { label: "Open ROM Folder", actionId: "openFolder", destructive: false },
        { label: "Remove from Library", actionId: "remove", destructive: true }
    ]

    function openForGame(gameId) {
        var details = themeContext.gameDetails(gameId)
        targetGameId = gameId
        isFavorite = details.favorite === 1
        gameTitle = details.title
        canScrape = themeContext.hasScraperCredentials()
        raGameId = 0
        if (themeContext.hasRACredentials()) {
            themeContext.raRequestGameIdLookup(details.title, details.system || "")
        }
        popupState = "actions"
        focusIndex = 0
        open()                              // BaseModalCard.open()
    }

    onCloseRequested: {
        visible = false
        targetGameId = -1
        // Return focus to theme page (unless settings overlay took over)
        if (!settingsOverlay.visible && mainStack.currentItem)
            mainStack.currentItem.forceActiveFocus()
    }

    Connections {
        target: themeContext
        function onRaGameIdLookupReady(title, lookedUpId) {
            if (title === popup.gameTitle) popup.raGameId = lookedUpId
        }
    }

    // ── Card content: title + actions + confirm + ButtonHints ──
    // Each is a direct child of BaseModalCard's card Rectangle (via the
    // default-property alias to card.data), so existing anchors against
    // `parent` continue to mean the card.

    Text {
        id: titleText
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter
        text: popup.gameTitle
        color: Qt.rgba(1, 1, 1, 0.6)
        font.pixelSize: 13
        elide: Text.ElideRight
        width: parent.width - 32
        horizontalAlignment: Text.AlignHCenter
    }

    // Action List
    Column {
        id: actionColumn
        anchors.top: titleText.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 32
        spacing: 2
        visible: popupState === "actions"

        Repeater {
            model: popup.actions

            delegate: Rectangle {
                required property var modelData
                required property int index

                property bool disabled: (modelData.actionId === "scrape" && !popup.canScrape)
                                     || (modelData.actionId === "achievements" && popup.raGameId === 0)

                width: actionColumn.width
                height: disabled ? 54 : 44
                radius: 6
                color: disabled ? "transparent"
                     : popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        color: disabled ? Qt.rgba(1, 1, 1, 0.3)
                             : modelData.destructive ? "#ff4444"
                             : (popup.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.7))
                        font.pixelSize: 16
                        font.weight: !disabled && popup.focusIndex === index ? Font.DemiBold : Font.Normal
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: disabled
                        text: modelData.actionId === "scrape" ? "Requires ScreenScraper login"
                            : modelData.actionId === "achievements" ? (themeContext.hasRACredentials() ? "Not found on RetroAchievements" : "Requires RetroAchievements login")
                            : ""
                        color: Qt.rgba(1, 1, 1, 0.25)
                        font.pixelSize: 11
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: disabled ? Qt.ArrowCursor : Qt.PointingHandCursor
                    onClicked: if (!parent.disabled) popup.executeAction(modelData.actionId)
                }
            }
        }
    }

    // Confirm Delete
    Column {
        id: confirmColumn
        anchors.top: titleText.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 32
        spacing: 12
        visible: popupState === "confirm"

        Text {
            width: parent.width
            text: "Delete ROM and remove from library?"
            color: "#ff4444"
            font.pixelSize: 15
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 16

            Repeater {
                model: ["Yes", "No"]

                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    width: 120
                    height: 44
                    radius: 6
                    color: popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                    border.color: popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: index === 0 ? "#ff4444" :
                               (popup.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.7))
                        font.pixelSize: 16
                        font.weight: popup.focusIndex === index ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (index === 0) popup.confirmRemove()
                            else popup.cancelRemove()
                        }
                    }
                }
            }
        }
    }

    // Button hints at bottom of card (floats 36px below via negative margin)
    ButtonHints {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -36
        anchors.horizontalCenter: parent.horizontalCenter
        hints: popupState === "confirm"
            ? [{action: "confirm", label: "Select"}, {action: "back", label: "Cancel"}]
            : [{action: "navigate_ud", label: "Navigate"}, {action: "confirm", label: "Select"}, {action: "back", label: "Close"}]
    }

    function executeAction(actionId) {
        switch (actionId) {
        case "scrape":
            themeContext.scrapeGameWithProgress(targetGameId)
            popup.close()
            break
        case "achievements":
            var raId = popup.raGameId
            var title = popup.gameTitle
            popup.close()
            settingsOverlay.navigateToAchievements(raId, title)
            break
        case "favorite":
            themeContext.toggleFavorite(targetGameId)
            popup.close()
            break
        case "openFolder":
            themeContext.openGameRomFolder(targetGameId)
            popup.close()
            break
        case "remove":
            popupState = "confirm"
            focusIndex = 1  // Default to "No"
            break
        }
    }

    function close() {
        // Mirrors pre-migration: trigger the BaseModalCard close path.
        closeRequested()
    }

    function confirmRemove() {
        themeContext.removeGame(targetGameId)
        close()
    }

    function cancelRemove() {
        popupState = "actions"
        focusIndex = 0
    }

    Keys.onPressed: function(event) {
        if (!visible) return

        if (popupState === "actions") {
            var actionCount = popup.actions.length
            if (event.key === Qt.Key_Up) {
                focusIndex = (focusIndex - 1 + actionCount) % actionCount
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                focusIndex = (focusIndex + 1) % actionCount
                event.accepted = true
            } else if (event.key === Qt.Key_Return) {
                var actionId = popup.actions[focusIndex].actionId
                if ((actionId === "scrape" && !canScrape) ||
                    (actionId === "achievements" && raGameId === 0))
                    return
                executeAction(actionId)
                event.accepted = true
            } else if (event.key === Qt.Key_M) {
                close()
                event.accepted = true
            }
            // Escape/Back handled by BaseModalCard → emits closeRequested
        } else if (popupState === "confirm") {
            if (event.key === Qt.Key_Left) {
                focusIndex = 0
                event.accepted = true
            } else if (event.key === Qt.Key_Right) {
                focusIndex = 1
                event.accepted = true
            } else if (event.key === Qt.Key_Return) {
                if (focusIndex === 0) confirmRemove()
                else cancelRemove()
                event.accepted = true
            }
            // Escape/Back in confirm: BaseModalCard's closeRequested closes
            // the dialog entirely. Pre-migration would cancel back to actions;
            // new behavior is simpler. To restore: override onCloseRequested
            // to dispatch on popupState (see plan for the override snippet).
        }
    }
}
