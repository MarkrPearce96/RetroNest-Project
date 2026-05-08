import QtQuick
import QtQuick.Controls

/**
 * InGameMenu — horizontal HUD anchored to bottom-center of its parent.
 * Used by both the libretro in-window path and the external-emulator
 * panel path. Emulator is paused while this menu is open.
 */
FocusScope {
    id: root
    anchors.fill: parent
    visible: false
    z: 200

    property int focusIndex: 0

    // Whether the currently running emulator supports save-on-exit.
    // Refreshed each open(). PPSSPP does not support this; PCSX2 +
    // DuckStation + Dolphin do.
    property bool supportsSaveOnExit: true

    // RA state (filled in async after open()).
    property int raGameId: 0
    property string raGameTitle: ""

    signal resumeRequested()
    signal achievementsRequested(int raGameId, string gameTitle)
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()

    function open() {
        focusIndex = 0;
        raGameId = 0;
        raGameTitle = "";
        var gameInfo = app.currentGameInfo();
        supportsSaveOnExit = gameInfo.supportsSaveOnExit === true;
        if (app.hasRACredentials() && gameInfo.title) {
            raGameTitle = gameInfo.title;
            app.raRequestGameIdLookup(gameInfo.title, gameInfo.system || "");
        }
        visible = true;
        forceActiveFocus();
    }

    function close() {
        visible = false;
    }

    Connections {
        target: app
        function onRaGameIdLookupReady(title, lookedUpId) {
            if (title === root.raGameTitle) root.raGameId = lookedUpId;
        }
    }

    // Visible icon-button list, rebuilt when raGameId or
    // supportsSaveOnExit changes.
    property var hudModel: {
        var items = [
            { icon: "images/hud/resume.svg",       label: "Resume",      action: "resume",     destructive: false }
        ];
        if (raGameId > 0) {
            items.push({ icon: "images/hud/achievements.svg", label: "Achievements", action: "achievements", destructive: false });
        }
        if (supportsSaveOnExit) {
            items.push({ icon: "images/hud/save_quit.svg", label: "Save & Quit", action: "exitSave", destructive: false });
        }
        items.push({ icon: "images/hud/quit.svg", label: "Quit", action: "exitNoSave", destructive: true });
        return items;
    }

    // Clamp focusIndex when the model length changes (e.g. RA lookup fills in).
    onHudModelChanged: {
        if (focusIndex >= hudModel.length) focusIndex = hudModel.length - 1;
        if (focusIndex < 0) focusIndex = 0;
    }

    // ── HUD pill ──
    Rectangle {
        id: pill
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 32
        anchors.horizontalCenter: parent.horizontalCenter
        height: hudRow.implicitHeight + 16
        width: hudRow.implicitWidth + 32
        radius: 14
        color: Qt.rgba(0.08, 0.08, 0.10, 0.88)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        Row {
            id: hudRow
            anchors.centerIn: parent
            spacing: 12

            Repeater {
                model: root.hudModel

                delegate: Rectangle {
                    width: 92
                    height: 76
                    radius: 10
                    color: root.focusIndex === index
                           ? (modelData.destructive
                              ? Qt.rgba(1.0, 0.30, 0.30, 0.18)
                              : Qt.rgba(1.0, 1.0, 1.0, 0.12))
                           : "transparent"

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Image {
                            width: 28
                            height: 28
                            anchors.horizontalCenter: parent.horizontalCenter
                            source: modelData.icon
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            opacity: root.focusIndex === index ? 1.0 : 0.7
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            font.pixelSize: 12
                            font.weight: root.focusIndex === index ? Font.DemiBold : Font.Normal
                            color: modelData.destructive
                                   ? (root.focusIndex === index ? "#ff8080" : "#ff5050")
                                   : (root.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.65))
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.focusIndex = index
                        onClicked: root.executeAction(modelData.action)
                    }
                }
            }
        }
    }

    // ── Input ──
    Keys.onPressed: function(event) {
        if (!visible) return;
        if (event.key === Qt.Key_Left) {
            focusIndex = Math.max(0, focusIndex - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Right) {
            focusIndex = Math.min(hudModel.length - 1, focusIndex + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            executeAction(hudModel[focusIndex].action);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            resumeRequested();
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
        case "exitSave":
            exitWithSaveRequested();
            break;
        case "exitNoSave":
            exitWithoutSaveRequested();
            break;
        }
    }
}
