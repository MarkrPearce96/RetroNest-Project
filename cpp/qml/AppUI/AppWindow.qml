import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import "EmulatorLogos.js" as EmulatorLogos

ApplicationWindow {
    id: window
    visible: true
    title: "RetroNest"
    color: Theme.background
    flags: Qt.Window | Qt.FramelessWindowHint
    // Prevent ApplicationWindow safe-area/titlebar padding from exposing the
    // root window color as a strip above the themed page content.
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    // Track whether the empty state page is currently shown
    property bool showingEmptyState: false

    // Toggle the in-game menu. Used by the global Cmd+Shift+Escape
    // Carbon hotkey, the SDL Select+Start combo, and the DualSense/
    // DualShock 4 Touchpad press.
    // True when a libretro game is the current top of mainStack.
    function isLibretroGame() {
        return mainStack.currentItem && mainStack.currentItem.isEmulationView === true;
    }

    function toggleInGameMenu() {
        // HW-render libretro + external emulators both flow through
        // InGameMenuController on the C++ side — one open/close pair
        // picks the right backing window (transparent QQuickWindow vs
        // floating NSPanel). SW-render libretro (mGBA) stays on the
        // in-scene branch below since it shares this window's scene.
        if (app.gameUsesHardwareRender() || !isLibretroGame()) {
            if (app.inGameMenuOpen)
                app.closeInGameMenu();
            else
                app.openInGameMenu();
            return;
        }

        // SW-render libretro path: in-window HUD. Pause/resume the core
        // explicitly because we don't get PauseOnFocusLoss.
        if (inGameMenu.visible) {
            inGameMenu.close();
            if (app.gameSession) app.gameSession.resumeEmulation();
        } else {
            if (app.gameSession) app.gameSession.pauseEmulation();
            app.activateApp();
            inGameMenu.open();
        }
    }

    function updateMainPage() {
        var hasGames = themeContext.systems.length > 0
        if (hasGames && showingEmptyState) {
            // Games appeared — switch to theme
            mainStack.clear()
            var url = themeManager.resolve("systemBrowser").toString()
            if (url !== "") mainStack.push(url)
            showingEmptyState = false
        } else if (!hasGames && !showingEmptyState) {
            // No games — switch to empty state
            mainStack.clear()
            mainStack.push("EmptyStatePage.qml")
            showingEmptyState = true
        }
    }

    // Borderless fullscreen: fill the entire screen without using native macOS fullscreen.
    // This prevents the menu bar and traffic lights from ever appearing.
    Component.onCompleted: {
        var screen = window.screen
        window.x = screen.virtualX
        window.y = screen.virtualY
        window.width = screen.width
        window.height = screen.height
    }

    // Theme page fills entire window
    StackView {
        id: mainStack
        anchors.fill: parent

        Component.onCompleted: {
            if (themeContext.systems.length > 0) {
                var url = themeManager.resolve("systemBrowser").toString()
                if (url !== "") {
                    mainStack.push(url)
                } else {
                    console.warn("[AppWindow] No systemBrowser URL resolved — is a theme installed?")
                }
            } else {
                mainStack.push("EmptyStatePage.qml")
                window.showingEmptyState = true
            }
            app.checkForUpdates()
        }
    }

    // Navigation driven by ThemeContext signals + transition between empty
    // state and theme when games/systems change.
    Connections {
        target: themeContext
        function onNavigateToSystemRequested(systemId) {
            if (window.showingEmptyState) return
            var pageUrl = themeManager.resolve("gameList").toString()
            if (pageUrl !== "") {
                mainStack.push(pageUrl)
            }
        }
        function onNavigateBackRequested() {
            if (mainStack.depth > 1) {
                mainStack.pop()
            }
        }
        function onSystemsChanged() { window.updateMainPage() }
        function onGamesChanged() { window.updateMainPage() }
    }

    // Reload theme pages when theme changes
    Connections {
        target: themeManager
        function onCurrentThemeChanged() {
            if (!window.showingEmptyState) {
                mainStack.clear()
                mainStack.push(themeManager.resolve("systemBrowser").toString())
            }
        }
    }

    // Settings overlay (Escape key)
    SettingsOverlay {
        id: settingsOverlay
        // While the settings overlay is shown, suppress libretro hotkey
        // dispatch so Esc/arrows/etc reach the overlay instead of
        // triggering ToggleMenu, save state, etc. Tied to panelOpen (the
        // user-intent flag) rather than `visible`, because `visible` only
        // clears after the slide-out animation completes and can race.
        onPanelOpenChanged: app.libretroHotkeysSuppressed = panelOpen
    }

    // Game action popup (M key / Triangle button)
    GameActionPopup {
        id: gameActionPopup
    }

    Component {
        id: achievementsPageComponent
        AchievementsPage {}
    }

    InGameMenu {
        id: inGameMenu

        // This in-window InGameMenu instance is the libretro path only
        // (external emulators get their own InGameMenuPanel). All paths
        // here resume the libretro core before doing anything else —
        // the core thread was paused on menu open and stays paused
        // until we explicitly call resumeEmulation, otherwise the game
        // (and any save-state work) is frozen.
        onResumeRequested: {
            if (app.gameSession) app.gameSession.resumeEmulation();
            inGameMenu.close();
        }

        onExitWithSaveRequested: {
            if (app.gameSession) app.gameSession.resumeEmulation();
            inGameMenu.close();
            themeContext.saveAndStopGame(1);
        }

        onExitWithoutSaveRequested: {
            if (app.gameSession) app.gameSession.resumeEmulation();
            inGameMenu.close();
            themeContext.stopGame();
        }

        // Libretro save / load drive CoreRuntime directly (external
        // emulators use synthesized hotkeys; libretro is in-process).
        // Resume the core first so the worker can flush the
        // requested action between frames.
        onSaveStateRequested: {
            if (app.gameSession) {
                app.gameSession.saveStateLibretro(app.gameSession.currentSaveSlot);
                app.gameSession.resumeEmulation();
            }
            inGameMenu.close();
            saveToast.show();
        }

        onLoadStateRequested: {
            if (app.gameSession) {
                app.gameSession.loadStateLibretro(app.gameSession.currentSaveSlot);
                app.gameSession.resumeEmulation();
            }
            inGameMenu.close();
            loadToast.show();
        }

        // Fast Forward is a STATE TOGGLE, not a one-shot action — keep
        // the menu open so the user explicitly clicks Resume when
        // they're ready to return. The new multiplier takes effect on
        // the worker's next iteration after resume; toggling while
        // paused stages the speed without unpausing.
        onToggleFastForwardRequested: {
            if (!app.gameSession) return;
            var ffOn = app.gameSession.toggleFastForwardLibretro();
            if (ffOn) ffToast.show(); else ffToast.hide();
        }
    }

    // When the game ends, drop any persistent in-game indicators —
    // otherwise the FF toast (or future sticky toasts) would stick
    // around on the main app UI after the user quits.
    Connections {
        target: app
        function onGameRunningChanged() {
            if (!app.gameRunning) {
                ffToast.hide();
                saveToast.hide();
                loadToast.hide();
            }
        }
    }

    // Top-right HUD notifications for libretro save/load/FF actions.
    // External emulators show their own OSD natively, so these are
    // libretro-only — they happen to also no-op when the libretro
    // path isn't active because nothing emits show().
    //
    // FF sits at the top (it's sticky); save/load drop below it and
    // collapse up against the screen edge when FF is off.
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

    // Achievement-unlock toast lives outside the small-action Column so
    // it can be wider/taller without disturbing the FF / save / load
    // pills. Anchored to the same top-right corner.
    AchievementToast {
        id: achievementToast
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 32
        anchors.rightMargin: 32
        z: 220
    }

    // Achievement-unlock chime. Sourced from RetroAchievements'
    // canonical libretro unlock SFX. Listed under appui_backing
    // RESOURCES in CMakeLists.txt so it's bundled into the QRC.
    // The play() call below is gated on app.raSoundEffects so users
    // who prefer silent unlocks can opt out from the settings UI.
    SoundEffect {
        id: unlockSound
        source: "sounds/Libretro_Achievement_Unlock.wav"
        volume: 1.0
    }

    // Persistent RA indicator stack — challenge / progress chips and
    // disconnection banner. Anchored bottom-left so it doesn't fight
    // the achievement toast (top-right) or the small action toasts.
    RAIndicatorBar {
        id: raIndicators
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 24
        anchors.bottomMargin: 24
        z: 220
    }

    ResumeStateDialog {
        id: resumeStateDialog

        onResumeChosen: {
            // Launch with -statefile pointing to the .resume.p2s file
            themeContext.launchGameResume(resumeStateDialog.pendingGameId,
                                          resumeStateDialog.pendingRomPath,
                                          resumeStateDialog.pendingEmuId);
            resumeStateDialog.close();
        }

        onStartFreshChosen: {
            // Clear the resume file and launch fresh
            themeContext.clearResumeState(resumeStateDialog.pendingRomPath,
                                          resumeStateDialog.pendingEmuId);
            themeContext.launchGameDirect(resumeStateDialog.pendingGameId,
                                          resumeStateDialog.pendingRomPath,
                                          resumeStateDialog.pendingEmuId);
            resumeStateDialog.close();
        }
    }

    Connections {
        target: themeContext
        function onGameActionsRequested(gameId) {
            gameActionPopup.open(gameId)
        }
        function onScrapeGameRequested(gameId) {
            settingsOverlay.navigateToScraper()
        }
        function onResumeStateFound(gameId, romPath, emuId) {
            resumeStateDialog.openForGame(gameId, romPath, emuId)
        }
    }

    // RA first-launch login prompt
    Connections {
        target: app
        function onRaEmulatorLoginPrompt(emulatorName) {
            raLoginPrompt.emulatorName = emulatorName
            raLoginPrompt.visible = true
            app.setCursorVisible(true)
            raLoginPrompt.forceActiveFocus()
        }
    }

    // RA achievement unlock toast — fires when an in-process libretro
    // core triggers an achievement. Shows a richer top-right card with
    // the achievement title + description for ~6 s.
    Connections {
        target: app
        function onRaAchievementUnlocked(id, title, description, imageUrl) {
            // SP3.5: skip when LibretroOverlayPanel handles overlays so
            // we don't double-fire the toast for Pattern B HW-render cores.
            if (app.gameUsesHardwareRender()) return;
            achievementToast.show(title, description, imageUrl)
            // Chime — gated on the user's "Sound Effects" preference
            // (Settings → RetroAchievements). Source-length check is
            // a defensive belt: if the WAV asset ever goes missing
            // from the bundle, fail silent rather than log spam.
            if (app.raSoundEffects && unlockSound.source.toString().length > 0)
                unlockSound.play()
        }
        // Generic info-toast — drives the same component for game-start
        // banner, game-mastered celebration, hardcore reset notice, and
        // server-error notice. The C++ side picks the header text.
        function onRaInfoToast(header, title, description, imageUrl, durationMs) {
            // SP3.5: skip for Pattern B HW-render cores — LibretroOverlayPanel handles it.
            if (app.gameUsesHardwareRender()) return;
            achievementToast.showWithHeader(header, title, description,
                                             imageUrl, durationMs)
        }
        // Persistent indicator updates — RAIndicatorBar dispatches
        // internally based on `kind` (rc_client event-type integers).
        function onRaIndicator(kind, data) {
            // SP3.5: skip for Pattern B HW-render cores — LibretroOverlayPanel handles it.
            if (app.gameUsesHardwareRender()) return;
            raIndicators.dispatch(kind, data)
        }
    }

    // RA login prompt dialog
    Item {
        id: raLoginPrompt
        anchors.fill: parent
        visible: false
        z: 250

        property string emulatorName: ""

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)
            MouseArea { anchors.fill: parent }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 440
            height: promptCol.height + 48
            radius: 12
            color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
            border.color: Qt.rgba(1, 1, 1, 0.1)
            border.width: 1

            Column {
                id: promptCol
                anchors {
                    left: parent.left; right: parent.right
                    top: parent.top; margins: 24
                }
                spacing: 12

                Text {
                    text: "RetroAchievements"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#ffffff"
                }

                Text {
                    text: "To earn achievements, log into RetroAchievements in " +
                          raLoginPrompt.emulatorName + "'s settings.\n\n" +
                          "Open the emulator's Settings > Achievements and click Login."
                    color: Qt.rgba(1, 1, 1, 0.6)
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    width: parent.width
                    lineHeight: 1.3
                }

                Item { width: 1; height: 4 }

                Rectangle {
                    width: parent.width
                    height: 40
                    radius: 6
                    color: Qt.rgba(1, 1, 1, 0.15)

                    Text {
                        anchors.centerIn: parent
                        text: "Got it"
                        color: "#ffffff"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            raLoginPrompt.visible = false
                            app.setCursorVisible(false)
                            app.raProceedAfterLoginPrompt()
                        }
                    }
                }
            }
        }

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                raLoginPrompt.visible = false
                app.setCursorVisible(false)
                app.raProceedAfterLoginPrompt()
                event.accepted = true
            }
        }
    }

    // Global Cmd+Shift+Escape hotkey — works even when an external emulator has focus
    Connections {
        target: app
        // Cmd+Shift+Esc — only acts when a STANDALONE emulator is running.
        // Libretro games drive their menu from their own bindings via
        // onLibretroMenuToggleRequested below.
        function onGlobalHotkeyPressed() {
            if (!app.gameRunning) return;
            if (isLibretroGame() || app.gameUsesHardwareRender()) return;
            window.toggleInGameMenu();
        }
        // Libretro matcher fires this whenever the user's ToggleMenu
        // binding is pressed in-game. Toggles open/closed — the in-scene
        // mGBA menu can also self-close via its own Keys.onPressed Esc
        // handler (focus tree path), but LibretroOverlayPanel runs in a
        // separate QQuickWindow whose Esc key events flow through the
        // app-level matcher, so without the close branch here PCSX2's
        // overlay had no way to dismiss via Esc.
        function onLibretroMenuToggleRequested() {
            if (!app.gameRunning) return;
            if (app.gameUsesHardwareRender()) {
                if (app.inGameMenuOpen)
                    app.closeInGameMenu();
                else
                    app.openInGameMenu();
                return;
            }
            if (isLibretroGame()) {
                if (inGameMenu.visible) {
                    inGameMenu.close();
                    if (app.gameSession) app.gameSession.resumeEmulation();
                } else {
                    if (app.gameSession) app.gameSession.pauseEmulation();
                    app.activateApp();
                    inGameMenu.open();
                }
            }
        }
    }

    // Update notification toast at top
    UpdateNotification {
        id: updateNotification
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter

        // Show the cursor while the notification is up so the user can click
        // Update/Close. Skip toggling if another overlay still needs it.
        onVisibleChanged: {
            if (visible) {
                app.setCursorVisible(true)
            } else if (!settingsOverlay.visible && !raLoginPrompt.visible
                    && !updateConfirm.visible && !updateProgressPopup.visible) {
                app.setCursorVisible(false)
            }
        }
    }

    Connections {
        target: app
        function onUpdateAvailable(emuId, currentVersion, latestVersion) {
            updateNotification.showUpdate(emuId, currentVersion, latestVersion)
        }
    }

    // "Update now?" confirmation shown when the user clicks the notification.
    Connections {
        target: updateNotification
        function onUpdateRequested(emuId, emuName, latestVersion) {
            updateConfirm.emuId = emuId
            updateConfirm.emuName = emuName
            updateConfirm.latestVersion = latestVersion
            updateConfirm.visible = true
            app.setCursorVisible(true)
            updateConfirm.forceActiveFocus()
        }
    }

    Item {
        id: updateConfirm
        anchors.fill: parent
        visible: false
        z: 260

        property string emuId: ""
        property string emuName: ""
        property string latestVersion: ""

        function cancel() {
            visible = false
            if (!settingsOverlay.visible && !updateNotification.visible) {
                app.setCursorVisible(false)
            }
        }

        function confirm() {
            visible = false
            updateProgressPopup.title = "Updating " + emuName
            updateProgressPopup.subtitle = "Downloading latest release..."
            updateProgressPopup.progressValue = -1
            updateProgressPopup.progressText = "Please wait..."
            updateProgressPopup.accentColor = Theme.accent
            updateProgressPopup.logoSource = EmulatorLogos.logoForEmu(emuId)
            updateProgressPopup.showCloseButton = false
            updateProgressPopup.emuId = emuId
            updateProgressPopup.open()
            app.installEmulator(emuId)
        }

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                updateConfirm.cancel()
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                updateConfirm.confirm()
                event.accepted = true
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)
            MouseArea { anchors.fill: parent }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 420
            height: confirmCol.height + 48
            radius: 12
            color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
            border.color: Qt.rgba(1, 1, 1, 0.1)
            border.width: 1

            Column {
                id: confirmCol
                anchors {
                    left: parent.left; right: parent.right
                    top: parent.top; margins: 24
                }
                spacing: 14

                Text {
                    text: "Update " + updateConfirm.emuName + "?"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#ffffff"
                }

                Text {
                    text: "Version " + updateConfirm.latestVersion + " is available. This will download and replace the current install."
                    color: Qt.rgba(1, 1, 1, 0.65)
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    width: parent.width
                    lineHeight: 1.3
                }

                Item { width: 1; height: 4 }

                Row {
                    anchors.right: parent.right
                    spacing: 10

                    Rectangle {
                        width: 96; height: 36; radius: 6
                        color: cancelMa.containsMouse ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(1, 1, 1, 0.1)
                        Text {
                            anchors.centerIn: parent
                            text: "Cancel"
                            color: "#ffffff"
                            font.pixelSize: 13
                        }
                        MouseArea {
                            id: cancelMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: updateConfirm.cancel()
                        }
                    }

                    Rectangle {
                        width: 96; height: 36; radius: 6
                        color: updateMa.containsMouse ? Theme.accentLight : Theme.accent
                        Text {
                            anchors.centerIn: parent
                            text: "Update"
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        MouseArea {
                            id: updateMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: updateConfirm.confirm()
                        }
                    }
                }
            }
        }
    }

    // Window-level progress popup for the update flow. EmulatorDetailPage has
    // its own instance for installs initiated there; they don't overlap because
    // the detail page's filters by matching emuId.
    ProgressPopup {
        id: updateProgressPopup
        property string emuId: ""
    }

    Connections {
        target: app
        function onInstallProgress(emuId, progress, phase, detail) {
            if (!updateProgressPopup.visible || emuId !== updateProgressPopup.emuId) return
            updateProgressPopup.progressValue = progress
            updateProgressPopup.subtitle = phase === "Downloading"
                ? "Downloading latest release..." : "Extracting files..."
            updateProgressPopup.progressText = progress >= 0 ? detail : "Please wait..."
        }
        function onInstallFinished(emuId, success, message) {
            if (!updateProgressPopup.visible || emuId !== updateProgressPopup.emuId) return
            if (success) {
                updateProgressPopup.close()
                if (!settingsOverlay.visible) app.setCursorVisible(false)
            } else {
                updateProgressPopup.title = "Update Failed"
                updateProgressPopup.subtitle = message
                updateProgressPopup.progressValue = 0
                updateProgressPopup.progressText = ""
                updateProgressPopup.showCloseButton = true
            }
        }
    }

    // Status toast overlay at bottom-left
    StatusBar {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.bottomMargin: 16
        width: Math.min(statusMsgMetrics.advanceWidth + 24, parent.width * 0.5)
        z: 50

        TextMetrics {
            id: statusMsgMetrics
            text: app.statusMessage
            font.pixelSize: 12
        }
    }

    // Floating button hints (above theme content, below overlays)
    ButtonHints {
        id: mainHints
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        z: 50
        // Hide while a libretro game is rendering in our window (EmulationView
        // is on top of the stack). Process-backed emulators show their own
        // window, so the app hints over our background window are harmless.
        // The InGameMenu owns its own hints when open.
        visible: !settingsOverlay.visible && !gameActionPopup.visible
                 && !(mainStack.currentItem && mainStack.currentItem.isEmulationView)
        hints: {
            if (window.showingEmptyState)
                return [{action: "start", label: "Settings"}]
            if (mainStack.depth > 1)
                return [{action: "navigate_ud", label: "Browse"}, {action: "confirm", label: "Launch"}, {action: "action", label: "Actions"}, {action: "back", label: "Back", keyboardKey: "Backspace"}, {action: "start", label: "Settings"}]
            return [{action: "navigate_lr", label: "Browse"}, {action: "confirm", label: "Select"}, {action: "start", label: "Settings"}]
        }
    }

    // Controller Start button toggles settings (signal, not key injection)
    Connections {
        target: inputManager
        enabled: !inputManager.virtualKeyboardOpen && !gameActionPopup.visible
                 && !app.inGameMenuOpen
        function onNavigateStart() {
            if (settingsOverlay.visible) {
                if (settingsOverlay.isBusy()) {
                    app.cancelScrape()
                } else {
                    settingsOverlay.close()
                }
            } else {
                settingsOverlay.open()
            }
        }
        function onInGameMenuRequested() {
            if (app.gameRunning) {
                // Select+Start / Touchpad combo: reserved for STANDALONE
                // emulators. Libretro games drive their menu from the
                // user's own binding in the Libretro Hotkeys page.
                if (isLibretroGame() || app.gameUsesHardwareRender()) return;
                window.toggleInGameMenu();
                return;
            }
            // Escape pressed via SDL when no game running — toggle settings
            if (settingsOverlay.visible) {
                if (settingsOverlay.isBusy()) {
                    app.cancelScrape()
                } else if (settingsOverlay.canGoBack()) {
                    settingsOverlay.goBack()
                } else {
                    settingsOverlay.close()
                }
            } else {
                settingsOverlay.open()
            }
        }
    }

    // SP3 launch-ordering fix: push EmulationView BEFORE retro_load_game runs.
    //
    // gameStartingLibretro fires from GameSession::startLibretro before
    // rt->start (and thus before Host::AcquireRenderWindow queries the host
    // for an NSView). Pushing the view here lets the Loader instantiate
    // LibretroMetalItem synchronously, which realises its child QWindow's
    // NSView and immediately calls registerHardwareView() back into
    // CoreRuntime — satisfying RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW by
    // the time the core's spin-wait inside startLibretro resolves.
    //
    // Idempotent vs gameStartedLibretro: we guard so we don't push twice.
    Connections {
        target: app
        function onGameStartingLibretro() {
            if (mainStack.currentItem && mainStack.currentItem.isEmulationView)
                return
            var view = mainStack.push("EmulationView.qml")
            view.session = app.gameSession
            view.inGameMenuRequested.connect(window.toggleInGameMenu)
        }
        function onGameStartedLibretro() {
            // After SP3 the view is already pushed by onGameStartingLibretro,
            // but keep this branch for software-backend launches that don't
            // need the pre-push (still cheap and idempotent thanks to the
            // isEmulationView guard).
            if (mainStack.currentItem && mainStack.currentItem.isEmulationView)
                return
            var view = mainStack.push("EmulationView.qml")
            view.session = app.gameSession
            view.inGameMenuRequested.connect(window.toggleInGameMenu)
        }
    }

    // Game lifecycle
    Connections {
        target: themeContext
        function onGameFinished(exitCode, crashed) {
            inGameMenu.close();
            resumeStateDialog.close();
            // If an EmulationView was pushed (libretro path), pop it now
            if (mainStack.depth > 1 && mainStack.currentItem
                    && mainStack.currentItem.isEmulationView)
                mainStack.pop()
            // Restore focus to the theme page so controller/keyboard nav works
            if (mainStack.currentItem)
                mainStack.currentItem.forceActiveFocus();
            // Drop any active challenge / progress chips so they don't
            // bleed into the next session.
            raIndicators.clear()
        }
    }

    // Escape key toggles settings overlay or in-game menu depending on game state
    Shortcut {
        sequence: "Escape"
        enabled: !gameActionPopup.visible && !resumeStateDialog.visible
                 && !app.inGameMenuOpen
        onActivated: {
            if (app.gameRunning) {
                // Shortcut fires when this app has focus — i.e. the
                // libretro path. Resume the core when closing the menu;
                // pause it when opening. (External-emulator path is
                // handled via the panel + Cmd+Shift+Esc Carbon hotkey,
                // not this Shortcut.)
                if (inGameMenu.visible) {
                    if (app.gameSession) app.gameSession.resumeEmulation();
                    inGameMenu.close();
                } else {
                    if (app.gameSession) app.gameSession.pauseEmulation();
                    inGameMenu.open();
                }
                return
            }
            if (settingsOverlay.visible) {
                if (settingsOverlay.isBusy()) {
                    // Cancel the running operation
                    app.cancelScrape()
                } else if (settingsOverlay.canGoBack()) {
                    settingsOverlay.goBack()
                } else {
                    settingsOverlay.close()
                }
            } else {
                settingsOverlay.open()
            }
        }
    }
}
