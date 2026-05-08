import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

    // Toggle the in-game menu, activating the app window so controller/keyboard
    // focus comes back from the emulator. Used by both the global Cmd+Escape
    // hotkey and the SDL Select+Circle combo.
    // True when a libretro game is the current top of mainStack.
    function isLibretroGame() {
        return mainStack.currentItem && mainStack.currentItem.isEmulationView === true;
    }

    function toggleInGameMenu() {
        if (isLibretroGame()) {
            // Libretro path: in-window HUD. Pause/resume the core
            // explicitly because we don't get PauseOnFocusLoss.
            if (inGameMenu.visible) {
                inGameMenu.close();
                if (app.gameSession) app.gameSession.resumeEmulation();
            } else {
                if (app.gameSession) app.gameSession.pauseEmulation();
                app.activateApp();
                inGameMenu.open();
            }
            return;
        }

        // External-emulator path: floating panel. Pause is triggered
        // by the panel becoming the system key window (each emulator's
        // PauseOnFocusLoss config handles it).
        if (app.inGameMenuPanelVisible) {
            app.closeInGameMenuPanel();
        } else {
            app.openInGameMenuPanel();
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

        onResumeRequested: {
            app.activateEmulator();
            inGameMenu.close();
        }

        onAchievementsRequested: function(raGameId, gameTitle) {
            inGameMenu.close();
            settingsOverlay.navigateToAchievements(raGameId, gameTitle);
        }

        onExitWithSaveRequested: {
            inGameMenu.close();
            themeContext.saveAndStopGame(1);
        }

        onExitWithoutSaveRequested: {
            inGameMenu.close();
            themeContext.stopGame();
        }
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
        function onInGameMenuPanelAchievementsRequested(raGameId, gameTitle) {
            settingsOverlay.navigateToAchievements(raGameId, gameTitle);
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

    // Global Cmd+Escape hotkey — works even when PCSX2 has focus
    Connections {
        target: app
        function onGlobalHotkeyPressed() {
            if (app.gameRunning) window.toggleInGameMenu();
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
                 && !app.inGameMenuPanelVisible
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

    // Libretro game start — push EmulationView on top of the theme
    Connections {
        target: app
        function onGameStartedLibretro() {
            var view = mainStack.push("EmulationView.qml")
            // Bind the session so frameReady flows through
            view.session = app.gameSession
            // In-game menu toggle (keyboard Esc path inside the view)
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
        }
    }

    // Escape key toggles settings overlay or in-game menu depending on game state
    Shortcut {
        sequence: "Escape"
        enabled: !gameActionPopup.visible && !resumeStateDialog.visible
                 && !app.inGameMenuPanelVisible
        onActivated: {
            if (app.gameRunning) {
                // Shortcut only fires when the app already has focus, so we
                // don't call activateApp() — inGameMenu.open() is enough.
                if (inGameMenu.visible) {
                    app.activateEmulator();
                    inGameMenu.close();
                } else {
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
