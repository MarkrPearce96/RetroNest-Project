import QtQuick
import QtQuick.Window
import QtMultimedia

/**
 * LibretroOverlayPanel — fullscreen transparent Window that hosts all
 * game-time overlays for Pattern B HW-render libretro cores (PCSX2). The
 * containing C++ instance (LibretroOverlayPanel) attaches this Window as
 * a macOS child of RetroNest's main NSWindow so it tracks geometry +
 * Spaces automatically.
 *
 * The seven overlays (one menu, three action toasts, RA toast, RA
 * indicator bar) are duplicated from AppWindow.qml — they have to be
 * separate QQuickItem instances because each QQuickWindow has its own
 * scene graph. Anchors and visuals are copied 1:1 so the look is
 * bit-identical to the in-scene path mGBA uses today.
 */
Window {
    id: panelWindow
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: false

    // Forwarded from the embedded InGameMenu to the C++ panel, which
    // forwards them to AppController -> GameSession.
    signal resumeRequested()
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()
    signal saveStateRequested()
    signal loadStateRequested()
    signal toggleFastForwardRequested()

    function openMenu() {
        inGameMenu.open();
        inGameMenu.forceActiveFocus();
    }

    function closeMenu() {
        inGameMenu.close();
    }

    // ── In-game menu (bottom-center HUD pill) ──
    InGameMenu {
        id: inGameMenu
        onResumeRequested:           panelWindow.resumeRequested()
        onExitWithSaveRequested:     panelWindow.exitWithSaveRequested()
        onExitWithoutSaveRequested:  panelWindow.exitWithoutSaveRequested()
        onSaveStateRequested:        panelWindow.saveStateRequested()
        onLoadStateRequested:        panelWindow.loadStateRequested()
        onToggleFastForwardRequested:panelWindow.toggleFastForwardRequested()
    }

    // ── Top-right action toasts (FF / save / load) ──
    Column {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 32
        anchors.rightMargin: 32
        spacing: 8
        z: 220

        ActionToast {
            id: ffToast
            iconSource: "images/hud/fast_forward.svg"
            label: "4×"
            sticky: true
        }
        ActionToast {
            id: saveToast
            iconSource: "images/hud/save_state.svg"
            label: "Saved"
        }
        ActionToast {
            id: loadToast
            iconSource: "images/hud/load_state.svg"
            label: "Loaded"
        }
    }

    // ── Achievement toast (top-right, separate from the small Column) ──
    AchievementToast {
        id: achievementToast
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 32
        anchors.rightMargin: 32
        z: 220
    }

    // ── Achievement unlock chime ──
    SoundEffect {
        id: unlockSound
        source: "sounds/Libretro_Achievement_Unlock.wav"
        volume: 1.0
    }

    // ── RA indicator bar (bottom-left) ──
    RAIndicatorBar {
        id: raIndicators
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 24
        anchors.bottomMargin: 24
        z: 220
    }

    // ── Drop persistent toasts when the game ends ──
    Connections {
        target: app
        function onGameRunningChanged() {
            if (!app.gameRunning) {
                ffToast.hide();
                saveToast.hide();
                loadToast.hide();
                raIndicators.clear();
            }
        }
    }

    // ── RA: achievement unlock ──
    Connections {
        target: app
        function onRaAchievementUnlocked(id, title, description, imageUrl) {
            if (!app.gameUsesHardwareRender()) return;
            achievementToast.show(title, description, imageUrl);
            if (app.raSoundEffects()) unlockSound.play();
        }
    }

    // ── RA: launch banner + other info toasts ──
    Connections {
        target: app
        function onRaInfoToast(header, title, description, imageUrl, durationMs) {
            if (!app.gameUsesHardwareRender()) return;
            achievementToast.show(title, description, imageUrl);
        }
    }

    // ── RA: persistent indicator chips ──
    Connections {
        target: app
        function onRaIndicator(kind, data) {
            if (!app.gameUsesHardwareRender()) return;
            raIndicators.dispatch(kind, data);
        }
    }

    // ── Local hookups for menu-triggered toasts ──
    // Mirrors AppWindow.qml: Save State pops a "Saved" pill, Load State
    // pops "Loaded". The Fast Forward toggle is handled by AppController:
    // it calls gameSession.toggleFastForwardLibretro() which returns the
    // new state; AppController will tell us which to show via a future
    // signal (for now FF state lives in the menu).
    Connections {
        target: panelWindow
        function onSaveStateRequested() { saveToast.show(); }
        function onLoadStateRequested() { loadToast.show(); }
    }
}
