import QtQuick
import QtQuick.Window

/**
 * InGameMenuPanel — Window host for the in-game HUD when an external
 * emulator is running. Sized and positioned by C++ (InGameMenuPanel)
 * via setGeometry(). Configured as a non-activating NSPanel by
 * MacFullscreen::configurePanelWindow() — that's what prevents the
 * panel from activating our app while still allowing it to become
 * the system key window (which is required for the emulator's
 * PauseOnFocusLoss to fire).
 */
Window {
    id: panelWindow
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: false

    // Re-emitted from the embedded HUD so AppController can wire them
    // to the same handlers used by the in-window path. Achievements
    // is handled inline by the HUD's slide-up popup, so it doesn't
    // need to leave the panel context.
    signal resumeRequested()
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()
    signal saveStateRequested()
    signal loadStateRequested()
    signal toggleFastForwardRequested()

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
        onExitWithSaveRequested: panelWindow.exitWithSaveRequested()
        onExitWithoutSaveRequested: panelWindow.exitWithoutSaveRequested()
        onSaveStateRequested: panelWindow.saveStateRequested()
        onLoadStateRequested: panelWindow.loadStateRequested()
        onToggleFastForwardRequested: panelWindow.toggleFastForwardRequested()
    }
}
