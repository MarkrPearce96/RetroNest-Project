import QtQuick
import QtQuick.Controls

/**
 * ResumeStateDialog — shown on game launch if a save state exists.
 * Asks user whether to resume from save state or start fresh.
 */
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 200

    property int focusIndex: 0
    property int pendingGameId: -1
    property string pendingRomPath: ""
    property string pendingEmuId: ""

    signal resumeChosen()
    signal startFreshChosen()

    function openForGame(gameId, romPath, emuId) {
        pendingGameId = gameId;
        pendingRomPath = romPath;
        pendingEmuId = emuId;
        focusIndex = 0;
        visible = true;
        forceActiveFocus();
    }

    function close() {
        visible = false;
        // Return focus to the theme page
        if (mainStack.currentItem)
            mainStack.currentItem.forceActiveFocus();
    }

    // ── Scrim ──
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.visible ? 0.7 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        MouseArea { anchors.fill: parent }
    }

    // ── Card ──
    Rectangle {
        anchors.centerIn: parent
        width: 420
        height: contentCol.implicitHeight + 48
        radius: 12
        color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1

        Column {
            id: contentCol
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

            // ── Buttons ──
            Repeater {
                model: [
                    { label: "Resume", action: "resume" },
                    { label: "Start Fresh", action: "fresh" }
                ]

                delegate: Rectangle {
                    width: contentCol.width
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
    }

    // ── Input Handling ──
    Keys.onPressed: function(event) {
        if (!visible) return;

        if (event.key === Qt.Key_Up) {
            focusIndex = Math.max(0, focusIndex - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            focusIndex = Math.min(1, focusIndex + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return) {
            if (focusIndex === 0) {
                resumeChosen();
            } else {
                startFreshChosen();
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            close();
            event.accepted = true;
        }
    }
}
