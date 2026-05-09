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

    // Per-emulator capability flags. Refreshed each open() from the
    // running adapter. Drives which icons appear in hudModel.
    //   supportsSaveOnExit  — PCSX2/DuckStation/Dolphin yes, PPSSPP no.
    //   supportsSaveState / supportsLoadState — every shipped adapter
    //     binds keyboard F5/F7 to its native save/load slot 1 hotkey.
    //   supportsFastForward — true only when the adapter exposes a
    //     real toggle keystroke (PCSX2/DuckStation). PPSSPP and
    //     Dolphin's "fast-forward" hotkeys are hold-style and would
    //     misbehave from a single synthesized press, so they return 0
    //     and the icon is hidden.
    property bool supportsSaveOnExit: true
    property bool supportsSaveState: false
    property bool supportsLoadState: false
    property bool supportsFastForward: false

    // RA state (filled in async after open()).
    property int raGameId: 0
    property string raGameTitle: ""

    signal resumeRequested()
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()
    signal saveStateRequested()
    signal loadStateRequested()
    signal toggleFastForwardRequested()

    function open() {
        focusIndex = 0;
        raGameId = 0;
        raGameTitle = "";
        var gameInfo = app.currentGameInfo();
        supportsSaveOnExit = gameInfo.supportsSaveOnExit === true;
        supportsSaveState = gameInfo.supportsSaveState === true;
        supportsLoadState = gameInfo.supportsLoadState === true;
        supportsFastForward = gameInfo.supportsFastForward === true;
        if (app.hasRACredentials() && gameInfo.title) {
            raGameTitle = gameInfo.title;
            app.raRequestGameIdLookup(gameInfo.title, gameInfo.system || "");
        }
        visible = true;
        forceActiveFocus();
    }

    function close() {
        achievementsPopupOpen = false;
        visible = false;
    }

    // True while the slide-up Achievements popup is showing on top
    // of the HUD. B / Back closes the popup first; only when closed
    // does B / Back actually resume.
    property bool achievementsPopupOpen: false

    Connections {
        target: app
        function onRaGameIdLookupReady(title, lookedUpId) {
            if (title === root.raGameTitle) root.raGameId = lookedUpId;
        }
    }

    // Visible icon-button list, rebuilt when raGameId or any of the
    // capability flags change. Order: Resume → Save State → Load
    // State → Fast Forward → Achievements → Save & Quit → Quit.
    property var hudModel: {
        var items = [
            { icon: "images/hud/resume.svg", label: "Resume", action: "resume", destructive: false }
        ];
        if (supportsSaveState) {
            items.push({ icon: "images/hud/save_state.svg", label: "Save State", action: "saveState", destructive: false });
        }
        if (supportsLoadState) {
            items.push({ icon: "images/hud/load_state.svg", label: "Load State", action: "loadState", destructive: false });
        }
        if (supportsFastForward) {
            items.push({ icon: "images/hud/fast_forward.svg", label: "Fast Forward", action: "fastForward", destructive: false });
        }
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

    // ── Achievements slide-up popup ──
    InGameAchievementsPopup {
        id: achievementsPopup
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: pill.top
        anchors.bottomMargin: 12
        height: 360
        raGameId: root.raGameId
        open: root.achievementsPopupOpen
    }

    // ── Input ──
    Keys.onPressed: function(event) {
        if (!visible) return;

        // While the achievements popup is open, route navigation
        // into the list instead of cycling HUD icons.
        if (achievementsPopupOpen) {
            if (event.key === Qt.Key_Up) {
                achievementsPopup.scrollUp();
                event.accepted = true;
                return;
            } else if (event.key === Qt.Key_Down) {
                achievementsPopup.scrollDown();
                event.accepted = true;
                return;
            } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                achievementsPopupOpen = false;
                event.accepted = true;
                return;
            }
            // Other keys (Left/Right, Return, etc.) fall through and
            // are absorbed below — we don't want HUD nav while the
            // popup is up.
            event.accepted = true;
            return;
        }

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
            // Stay in the game/menu context — slide the achievements
            // list up over the HUD instead of routing back to the
            // main app's settings overlay.
            achievementsPopupOpen = !achievementsPopupOpen;
            break;
        case "saveState":
            saveStateRequested();
            break;
        case "loadState":
            loadStateRequested();
            break;
        case "fastForward":
            toggleFastForwardRequested();
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
