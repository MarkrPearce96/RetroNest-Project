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

    // Hardcore-mode lockdown. True when the user has hardcore enabled
    // AND a libretro game with active rcheevos session is running. In
    // this state, save state / load state / fast-forward are hidden
    // because RA's hardcore rules forbid them — using any of these
    // would invalidate the session's hardcore unlocks server-side.
    // External-emulator games (PCSX2/DuckStation/etc) have their own
    // RA integration handling these rules independently, so we don't
    // gate them here.
    property bool hardcoreLockdown: false

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
        // One-shot evaluation: hardcore can't be toggled mid-session
        // (settings live outside the game). Recompute every menu-open
        // anyway so a settings round-trip between sessions picks it up.
        hardcoreLockdown = app.raHardcoreMode() && app.libretroAchievementsReady();
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

    // Visible icon-button list, rebuilt when raGameId, the capability
    // flags, or hardcoreLockdown change. Order: Resume → Save State →
    // Load State → Fast Forward → Achievements → Save & Quit → Quit.
    // Save / Load / Fast Forward are hidden in hardcore lockdown — RA
    // forbids them during a hardcore session and using any would void
    // unlocks for that session.
    property var hudModel: {
        var items = [
            { icon: "images/hud/resume.svg", label: "Resume", action: "resume", destructive: false }
        ];
        if (supportsSaveState && !hardcoreLockdown) {
            items.push({ icon: "images/hud/save_state.svg", label: "Save State", action: "saveState", destructive: false });
        }
        if (supportsLoadState && !hardcoreLockdown) {
            items.push({ icon: "images/hud/load_state.svg", label: "Load State", action: "loadState", destructive: false });
        }
        if (supportsFastForward && !hardcoreLockdown) {
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

    // ── Hardcore-mode indicator ──
    // Sits just above the HUD pill while hardcore lockdown is active so
    // the user understands why save state / load state / fast forward
    // are missing from the menu.
    Rectangle {
        id: hardcoreBadge
        visible: hardcoreLockdown
        anchors.bottom: pill.top
        anchors.bottomMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        height: 26
        width: hardcoreRow.implicitWidth + 16
        radius: 13
        color: Qt.rgba(0.45, 0.05, 0.05, 0.92)
        border.color: Qt.rgba(1, 0.4, 0.3, 0.55)
        border.width: 1

        Row {
            id: hardcoreRow
            anchors.centerIn: parent
            spacing: 6
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "🔒"
                font.pixelSize: 12
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "HARDCORE MODE — save / load / fast-forward disabled"
                color: "#ffffff"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 0.4
            }
        }
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
        // Matches the card's fixed height (560×460 inside) plus a
        // little breathing room so the slide-in transform doesn't
        // clip on the way up.
        height: 470
        raGameId: root.raGameId
        open: root.achievementsPopupOpen
    }

    // ── Input ──
    Keys.onPressed: function(event) {
        if (!visible) return;

        // While the achievements popup is open, route navigation
        // into the popup instead of cycling HUD icons. Up/Down
        // scroll the list; Left/Right cycle the filter tabs.
        if (achievementsPopupOpen) {
            if (event.key === Qt.Key_Up) {
                achievementsPopup.scrollUp();
                event.accepted = true;
                return;
            } else if (event.key === Qt.Key_Down) {
                achievementsPopup.scrollDown();
                event.accepted = true;
                return;
            } else if (event.key === Qt.Key_Left) {
                achievementsPopup.prevTab();
                event.accepted = true;
                return;
            } else if (event.key === Qt.Key_Right) {
                achievementsPopup.nextTab();
                event.accepted = true;
                return;
            } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                achievementsPopupOpen = false;
                event.accepted = true;
                return;
            }
            // Other keys (Return, etc.) fall through and are absorbed
            // below — we don't want HUD nav while the popup is up.
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
