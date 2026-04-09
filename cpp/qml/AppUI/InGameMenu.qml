import QtQuick
import QtQuick.Controls

/**
 * InGameMenu — in-game pause/quit overlay.
 * Two states: "main" (Resume / Quit Game) and "quit" (submenu).
 * Emulator is paused while this menu is open.
 */
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 200

    // State: "main" or "quit"
    property string menuState: "main"
    property int focusIndex: 0

    // Whether the currently running emulator supports save-on-exit. Refreshed
    // each time the menu opens. PPSSPP does not support this; PCSX2 and
    // DuckStation do.
    property bool supportsSaveOnExit: true

    signal resumeRequested()
    signal achievementsRequested(int raGameId, string gameTitle)
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()

    function open() {
        menuState = "main";
        focusIndex = 0;

        // Look up RA game ID + save-on-exit capability for the currently running game
        raGameId = 0;
        raGameTitle = "";
        var gameInfo = app.currentGameInfo();
        supportsSaveOnExit = gameInfo.supportsSaveOnExit === true;
        if (app.hasRACredentials() && gameInfo.title) {
            raGameTitle = gameInfo.title;
            raGameId = app.raFindGameId(gameInfo.title, gameInfo.system || "");
        }

        visible = true;
        forceActiveFocus();
    }

    function close() {
        visible = false;
    }

    // ── Scrim ──
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.visible ? 0.7 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        MouseArea { anchors.fill: parent } // Block clicks through
    }

    // ── Card ──
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 400
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
            spacing: 4

            // ── Title ──
            Text {
                text: menuState === "main" ? "Paused" : "Quit Game"
                font.pixelSize: 20
                font.bold: true
                color: "#ffffff"
                bottomPadding: 12
            }

            // ── Main Menu Items ──
            Repeater {
                model: menuState === "main" ? mainMenuModel : quitMenuModel

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
                        color: modelData.destructive
                               ? (root.focusIndex === index ? "#ff6666" : "#ff4444")
                               : (root.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.6))
                    }
                }
            }
        }
    }

    // ── Menu Models ──
    property int raGameId: 0
    property string raGameTitle: ""

    property var mainMenuModel: {
        var items = [
            { label: "Resume Game", action: "resume", destructive: false }
        ];
        if (raGameId > 0) {
            items.push({ label: "Achievements", action: "achievements", destructive: false });
        }
        items.push({ label: "Quit Game", action: "quit", destructive: false });
        return items;
    }

    property var quitMenuModel: {
        var items = [
            { label: "Back to Pause Menu", action: "back", destructive: false }
        ];
        if (supportsSaveOnExit) {
            items.push({ label: "Exit & Save State", action: "exitSave", destructive: false });
        }
        items.push({ label: "Exit Without Saving", action: "exitNoSave", destructive: true });
        return items;
    }

    property var currentModel: menuState === "main" ? mainMenuModel : quitMenuModel

    // ── Input Handling ──
    Keys.onPressed: function(event) {
        if (!visible) return;

        if (event.key === Qt.Key_Up) {
            focusIndex = Math.max(0, focusIndex - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            focusIndex = Math.min(currentModel.length - 1, focusIndex + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return) {
            executeAction(currentModel[focusIndex].action);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            if (menuState === "quit") {
                menuState = "main";
                focusIndex = 0;
            } else {
                resumeRequested();
            }
            event.accepted = true;
        }
    }

    function executeAction(action) {
        switch (action) {
        case "resume":
            resumeRequested();
            break;
        case "achievements":
            achievementsRequested(raGameId, raGameTitle);
            break;
        case "quit":
            menuState = "quit";
            focusIndex = 0;
            break;
        case "back":
            menuState = "main";
            focusIndex = 0;
            break;
        case "exitSave":
            exitWithSaveRequested();
            break;
        case "exitNoSave":
            exitWithoutSaveRequested();
            break;
        }
    }
}
