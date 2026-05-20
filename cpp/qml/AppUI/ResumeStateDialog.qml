import QtQuick
import QtQuick.Controls

/**
 * ResumeStateDialog — shown on game launch if a save state exists.
 * Asks user whether to resume from save state or start fresh.
 *
 * Scrim, centered card, Escape/Back handler, and close-on-scrim-click
 * come from BaseModalCard. Up/Down focus + Enter activation stay here
 * (no wrap-around — a 2-button list).
 */
BaseModalCard {
    id: root
    cardWidth: 420

    property int focusIndex: 0
    property int pendingGameId: -1
    property string pendingRomPath: ""
    property string pendingEmuId: ""

    signal resumeChosen()
    signal startFreshChosen()

    function openForGame(gameId, romPath, emuId) {
        pendingGameId = gameId
        pendingRomPath = romPath
        pendingEmuId = emuId
        focusIndex = 0
        open()                              // BaseModalCard.open()
    }

    // Public close() — callers (AppWindow.qml resumeChosen/startFreshChosen
    // handlers) hide the dialog this way; routes through BaseModalCard's
    // closeRequested signal so the focus-restoration handler below runs.
    function close() {
        closeRequested()
    }

    onCloseRequested: {
        visible = false
        // Return focus to the theme page
        if (mainStack.currentItem)
            mainStack.currentItem.forceActiveFocus()
    }

    // Card content (children of BaseModalCard's card Rectangle)
    Column {
        anchors {
            left: parent.left; right: parent.right
            top: parent.top
            margins: 24
        }
        spacing: 8

        Text {
            text: "Save State Found"
            font.pixelSize: 20
            font.bold: true
            color: "#ffffff"
        }

        Text {
            text: "A save state was found. Resume from where you left off?"
            font.pixelSize: 14
            color: Qt.rgba(1, 1, 1, 0.6)
            wrapMode: Text.WordWrap
            width: parent.width
            bottomPadding: 8
        }

        // Buttons
        Repeater {
            model: [
                { label: "Resume",     action: "resume" },
                { label: "Start Fresh", action: "fresh" }
            ]

            delegate: Rectangle {
                width: parent.width
                height: 44
                radius: 6
                color: root.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    text: modelData.label
                    font.pixelSize: 15
                    font.bold: root.focusIndex === index
                    color: root.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.6)
                }
            }
        }
    }

    Keys.onPressed: function(event) {
        if (!visible) return
        if (event.key === Qt.Key_Up) {
            focusIndex = Math.max(0, focusIndex - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            focusIndex = Math.min(1, focusIndex + 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return) {
            if (focusIndex === 0) resumeChosen()
            else startFreshChosen()
            event.accepted = true
        }
        // Escape/Back handled by BaseModalCard → emits closeRequested
    }
}
