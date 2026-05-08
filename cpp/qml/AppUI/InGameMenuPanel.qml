import QtQuick
import QtQuick.Window

/**
 * InGameMenuPanel — Window host for the in-game HUD when an external
 * emulator is running. Sized and positioned by C++ (InGameMenuPanel)
 * via setGeometry(). Configured as a non-activating NSPanel by
 * MacFullscreen::configurePanelWindow().
 */
Window {
    id: panelWindow
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: false

    // Re-emitted from the embedded HUD so AppController can wire them
    // to the same handlers used by the in-window path.
    signal resumeRequested()
    signal achievementsRequested(int raGameId, string gameTitle)
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()

    function openMenu() {
        visible = true;
        hud.open();
        hud.forceActiveFocus();
    }

    function closeMenu() {
        hud.close();
        visible = false;
    }

    InGameMenu {
        id: hud
        anchors.fill: parent
        onResumeRequested: panelWindow.resumeRequested()
        onAchievementsRequested: function(rid, title) {
            panelWindow.achievementsRequested(rid, title);
        }
        onExitWithSaveRequested: panelWindow.exitWithSaveRequested()
        onExitWithoutSaveRequested: panelWindow.exitWithoutSaveRequested()
    }
}
