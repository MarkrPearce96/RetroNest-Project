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
            label: "2×"
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

    // ── RA launch banner + other info toasts ──
    Connections {
        target: app
        function onInfoToast(header, title, description, imageUrl, durationMs) {
            if (!app.gameUsesHardwareRender()) return;
            // showWithHeader preserves the C++-supplied header label
            // (e.g. "GAME MASTERED", "HARDCORE MODE", "RA SERVER ERROR",
            // "Fast Forward") instead of falling back to the default
            // "ACHIEVEMENT UNLOCKED" used by show().
            achievementToast.showWithHeader(header, title, description,
                                             imageUrl, durationMs);
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

    // ── Saved/Loaded pills ──
    // Driven by GameSession (like the FF pill below) so every trigger
    // source — menu buttons AND hotkeys — pops the same pill.
    Connections {
        target: app.gameSession
        function onStateSaveRequested() { saveToast.show(); }
        function onStateLoadRequested() { loadToast.show(); }
    }

    // Fast Forward — driven by the GameSession property change signal so
    // both menu-driven and hotkey-driven toggles surface the pill. The
    // C++ side (AppController + InGameMenuController) handles the toggle
    // itself; this binding just mirrors the resulting state into the
    // sticky 2× HUD pill.
    Connections {
        target: app.gameSession
        function onLibretroFastForwardChanged() {
            if (app.gameSession && app.gameSession.libretroFastForward) {
                ffToast.show();
            } else {
                ffToast.hide();
            }
        }
    }
}
