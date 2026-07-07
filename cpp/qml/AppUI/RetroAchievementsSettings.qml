import QtQuick
import QtQuick.Layouts
import AppUI

Item {
    id: root
    focus: true

    // Explicit navigation contract: the page never reaches into its host's
    // StackView/components via dynamic scoping. SettingsOverlay connects
    // this signal and owns the actual push. Pages: "achievements",
    // "allGames", "recentlyPlayed".
    signal pushRequested(string page, var props)

    property string screenState: app.hasRACredentials() ? "dashboard" : "login"
    property int loginFocusIndex: 0
    property string loginError: ""
    property bool loggingIn: false

    // Dashboard data
    property var userSummary: ({})
    property var userGames: []
    property bool loading: false

    // Settings
    property bool hardcoreMode: app.raHardcoreMode()
    property bool encoreMode:   app.raEncoreMode()
    property bool notificationsEnabled: app.raNotifications()
    property bool soundsEnabled: app.raSoundEffects()

    // Libretro login state
    property bool libretroLoggingIn: false
    property string libretroLoginError: ""
    property bool libretroTokenPresent: app.raHasLibretroToken()

    onScreenStateChanged: {
        if (screenState === "dashboard") refreshTimer.start()
        if (screenState === "login") loginFocusIndex = 0
    }

    Component.onCompleted: {
        if (screenState === "dashboard") refreshTimer.start()
    }

    // Delay data fetch so StackView animation completes first
    Timer {
        id: refreshTimer
        interval: 300
        onTriggered: refreshDashboard()
    }

    property int _pendingFetches: 0

    function refreshDashboard() {
        loading = true
        _pendingFetches = 2
        app.raRequestUserSummary()
        app.raRequestUserGames()
    }

    Connections {
        target: app
        function onRaLoginCompleted(success, error) {
            loggingIn = false
            if (success) {
                loginError = ""
                screenState = "dashboard"
            } else {
                loginError = error || "Login failed"
            }
        }
        function onRaSignedOut() {
            screenState = "login"
            userSummary = {}
            userGames = []
            libretroTokenPresent = false
        }
        function onRaUserSummaryReady(summary) {
            userSummary = summary
            if (--_pendingFetches <= 0) loading = false
        }
        function onRaUserGamesReady(games) {
            userGames = games
            if (--_pendingFetches <= 0) loading = false
        }
        function onRaLoginTokenChanged() {
            libretroLoggingIn = false
            libretroLoginError = ""
            libretroTokenPresent = true
            libretroPasswordField.text = ""
        }
        function onRaLoginFailed(message) {
            libretroLoggingIn = false
            libretroLoginError = message || "Sign-in failed"
            libretroPasswordField.text = ""
        }
    }

    Keys.onPressed: function(event) {
        if (screenState !== "login") return
        if (event.key === Qt.Key_Down || event.key === Qt.Key_Tab) {
            loginFocusIndex = (loginFocusIndex + 1) % 3
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            loginFocusIndex = (loginFocusIndex - 1 + 3) % 3
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (loginFocusIndex === 0) raUserField.forceActiveFocus()
            else if (loginFocusIndex === 1) raKeyField.forceActiveFocus()
            else doLogin()
            event.accepted = true
        }
    }

    function doLogin() {
        if (raUserField.text.trim() === "" || raKeyField.text.trim() === "") {
            loginError = "Please enter both username and API key"
            return
        }
        loggingIn = true
        loginError = ""
        app.raLogin(raUserField.text.trim(), raKeyField.text.trim())
    }

    function doLibretroLogin() {
        libretroLoggingIn = true
        libretroLoginError = ""
        app.raLoginWithPassword(app.raUsername(), libretroPasswordField.text)
        // Password is cleared by onRaLoginTokenChanged / onRaLoginFailed handlers
    }

    // ── Login State ──
    Item {
        anchors.fill: parent
        visible: screenState === "login"

        Rectangle {
            anchors.centerIn: parent
            width: 360
            height: loginCol.height + 48
            radius: 12
            color: SettingsTheme.card
            border.width: 1
            border.color: SettingsTheme.border

            Column {
                id: loginCol
                anchors.centerIn: parent
                width: parent.width - 48
                spacing: 16

                // RA Logo
                Image {
                    width: 64
                    height: 64
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "images/retroachievements_logo.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Text {
                    text: "RetroAchievements"
                    color: SettingsTheme.text
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "Enter your username and web API key from\nretroachievements.org/settings"
                    color: SettingsTheme.textMuted
                    font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { width: 1; height: 4 }

                // Username field
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Username"
                        color: SettingsTheme.textMuted
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: SettingsTheme.base
                        border.width: 1
                        border.color: loginFocusIndex === 0 ? SettingsTheme.accent : SettingsTheme.border

                        TextInput {
                            id: raUserField
                            anchors.fill: parent
                            anchors.margins: 10
                            color: SettingsTheme.text
                            font.pixelSize: 14
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            Keys.onTabPressed: { loginFocusIndex = 1; raKeyField.forceActiveFocus() }
                            Keys.onReturnPressed: { loginFocusIndex = 1; raKeyField.forceActiveFocus() }
                            onActiveFocusChanged: { if (activeFocus) loginFocusIndex = 0 }
                        }
                    }
                }

                // API Key field
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "Web API Key"
                        color: SettingsTheme.textMuted
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: SettingsTheme.base
                        border.width: 1
                        border.color: loginFocusIndex === 1 ? SettingsTheme.accent : SettingsTheme.border

                        TextInput {
                            id: raKeyField
                            anchors.fill: parent
                            anchors.margins: 10
                            color: SettingsTheme.text
                            font.pixelSize: 14
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            Keys.onTabPressed: { loginFocusIndex = 2; root.forceActiveFocus() }
                            Keys.onReturnPressed: doLogin()
                            onActiveFocusChanged: { if (activeFocus) loginFocusIndex = 1 }
                        }
                    }
                }

                // Sign In button
                Rectangle {
                    width: parent.width
                    height: 42
                    radius: 8
                    color: loginFocusIndex === 2 ? Qt.lighter(SettingsTheme.accent, 1.2) : SettingsTheme.accent
                    opacity: loggingIn ? 0.6 : 1

                    Text {
                        anchors.centerIn: parent
                        text: loggingIn ? "Validating..." : "Connect"
                        color: SettingsTheme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (!loggingIn) doLogin()
                    }
                }

                // Error text
                Text {
                    visible: loginError !== ""
                    text: loginError
                    color: "#ef4444"
                    font.pixelSize: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // ── Dashboard State ──
    Flickable {
        anchors.fill: parent
        anchors.margins: 20
        contentHeight: dashboardCol.height
        clip: true
        visible: screenState === "dashboard"
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: dashboardCol
            width: parent.width
            spacing: 20

            // Profile header
            Rectangle {
                width: parent.width
                height: 80
                radius: 12
                color: SettingsTheme.card
                border.width: 1
                border.color: SettingsTheme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 16

                    // Avatar — real image with letter-circle fallback
                    Rectangle {
                        width: 48
                        height: 48
                        radius: 24
                        clip: true
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: SettingsTheme.accent }
                            GradientStop { position: 1.0; color: Qt.darker(SettingsTheme.accent, 1.3) }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: (userSummary.username || "?").charAt(0).toUpperCase()
                            color: SettingsTheme.text
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            visible: avatarImg.status !== Image.Ready
                        }

                        Image {
                            id: avatarImg
                            anchors.fill: parent
                            source: userSummary.userPic || ""
                            fillMode: Image.PreserveAspectCrop
                            visible: status === Image.Ready
                        }
                    }

                    Column {
                        spacing: 2
                        Layout.fillWidth: true

                        Text {
                            text: userSummary.username || ""
                            color: SettingsTheme.text
                            font.pixelSize: 18
                            font.weight: Font.Bold
                        }

                        Text {
                            text: {
                                var parts = []
                                if (userSummary.rank > 0) parts.push("Rank #" + userSummary.rank)
                                else parts.push("Unranked")
                                if (userSummary.memberSince) {
                                    var d = new Date(userSummary.memberSince)
                                    if (!isNaN(d.getTime()))
                                        parts.push("Member since " + d.toLocaleDateString(Qt.locale(), "MMM yyyy"))
                                    else
                                        parts.push("Member since " + userSummary.memberSince.substring(0, 10))
                                }
                                return parts.join("  \u00B7  ")
                            }
                            color: SettingsTheme.textMuted
                            font.pixelSize: 13
                        }
                    }

                    // Sign out
                    Rectangle {
                        width: 80
                        height: 32
                        radius: 6
                        color: SettingsTheme.border

                        Text {
                            anchors.centerIn: parent
                            text: "Sign Out"
                            color: SettingsTheme.textMuted
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: app.raSignOut()
                        }
                    }
                }
            }

            // Stats grid
            Text {
                text: "Stats"
                color: SettingsTheme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            GridLayout {
                width: parent.width
                columns: 4
                rowSpacing: 8
                columnSpacing: 8

                Repeater {
                    model: [
                        { label: "Total Points", value: ((userSummary.totalPoints || 0) + (userSummary.softcorePoints || 0)), accent: SettingsTheme.accent },
                        { label: "Hardcore", value: userSummary.totalPoints || "0", accent: SettingsTheme.textMuted },
                        { label: "True Points", value: userSummary.totalTruePoints || "0", accent: SettingsTheme.accent },
                        { label: "Games Played", value: userGames.length || "0", accent: SettingsTheme.accent }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        radius: 8
                        color: SettingsTheme.card
                        border.width: 1
                        border.color: SettingsTheme.border

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value
                                color: modelData.accent
                                font.pixelSize: 20
                                font.weight: Font.Bold
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: SettingsTheme.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }

            // Last Played section
            Text {
                text: "Last Played"
                color: SettingsTheme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
                visible: (userSummary.lastGameTitle || "") !== ""
            }

            Rectangle {
                width: parent.width
                height: 64
                radius: 12
                color: SettingsTheme.card
                border.width: 1
                border.color: SettingsTheme.border
                visible: (userSummary.lastGameTitle || "") !== ""

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Rectangle {
                        width: 40
                        height: 40
                        radius: 6
                        color: SettingsTheme.surface

                        Image {
                            anchors.fill: parent
                            anchors.margins: 2
                            source: userSummary.lastGameIcon || ""
                            fillMode: Image.PreserveAspectFit
                            visible: status === Image.Ready
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: userSummary.lastGameTitle || ""
                            color: SettingsTheme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            width: parent.width
                        }

                        Text {
                            text: "Last Played"
                            color: SettingsTheme.textDim
                            font.pixelSize: 11
                            font.weight: Font.Normal
                            textFormat: Text.PlainText
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (userSummary.lastGameId > 0)
                            root.pushRequested("achievements", { raGameId: userSummary.lastGameId, gameTitle: userSummary.lastGameTitle })
                    }
                }
            }

            // Recent achievements
            Text {
                text: "Recent Achievements"
                color: SettingsTheme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
                visible: (userSummary.recentAchievements || []).length > 0
            }

            Rectangle {
                width: parent.width
                height: recentCol.height + 16
                radius: 12
                color: SettingsTheme.card
                border.width: 1
                border.color: SettingsTheme.border
                visible: (userSummary.recentAchievements || []).length > 0

                Column {
                    id: recentCol
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    spacing: 4

                    Repeater {
                        model: userSummary.recentAchievements || []

                        Rectangle {
                            width: parent.width
                            height: 48
                            radius: 6
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 12

                                // Badge
                                Rectangle {
                                    width: 32
                                    height: 32
                                    radius: 4
                                    color: SettingsTheme.surface

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: modelData.badgeName
                                            ? "https://media.retroachievements.org/Badge/" + modelData.badgeName + ".png"
                                            : ""
                                        fillMode: Image.PreserveAspectFit
                                        visible: status === Image.Ready
                                    }
                                }

                                Column {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: modelData.title || ""
                                        color: SettingsTheme.text
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                        width: parent.width
                                    }

                                    Text {
                                        text: modelData.gameTitle || ""
                                        color: SettingsTheme.textMuted
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        width: parent.width
                                    }
                                }

                                Text {
                                    text: (modelData.points || "0") + " pts"
                                    color: SettingsTheme.accent
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }
                }
            }

            // Game progress header with View All link
            RowLayout {
                width: parent.width
                spacing: 8

                Text {
                    text: "Game Progress"
                    color: SettingsTheme.text
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }

                Text {
                    text: "View All (" + userGames.length + ") \u203A"
                    color: SettingsTheme.accent
                    font.pixelSize: 13
                    visible: userGames.length > 0

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.pushRequested("allGames", { allGames: userGames })
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: 3
                rowSpacing: 8
                columnSpacing: 8
                visible: userGames.length > 0

                Repeater {
                    model: userGames.slice(0, Math.min(userGames.length, 3))

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        radius: 8
                        color: SettingsTheme.card
                        border.width: 1
                        border.color: SettingsTheme.border

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.pushRequested("achievements", { raGameId: modelData.raGameId, gameTitle: modelData.title })
                        }

                        Column {
                            anchors.centerIn: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            width: parent.width - 16
                            spacing: 6

                            // Game icon
                            Image {
                                width: 48; height: 48
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: modelData.imageIcon ? "https://retroachievements.org" + modelData.imageIcon : ""
                                fillMode: Image.PreserveAspectFit
                                visible: status === Image.Ready
                            }

                            Text {
                                text: modelData.title || ""
                                color: SettingsTheme.text
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text {
                                text: modelData.consoleName || ""
                                color: SettingsTheme.textDim
                                font.pixelSize: 10
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            // Progress bar
                            Rectangle {
                                width: parent.width
                                height: 4
                                radius: 2
                                color: SettingsTheme.border

                                Rectangle {
                                    width: parent.width * Math.min(1, (modelData.numAwarded || 0) / Math.max(1, modelData.numAchievements || 1))
                                    height: parent.height
                                    radius: 2
                                    color: (modelData.mastered) ? SettingsTheme.success : SettingsTheme.accent
                                }
                            }

                            Text {
                                text: (modelData.numAwarded || 0) + " / " + (modelData.numAchievements || 0)
                                color: SettingsTheme.textMuted
                                font.pixelSize: 11
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }
                }
            }

            Text {
                visible: userGames.length === 0
                text: loading ? "Loading..." : "No game progress yet"
                color: SettingsTheme.textFaint
                font.pixelSize: 13
            }

            // Recently Played header with View All link
            RowLayout {
                width: parent.width
                spacing: 8
                visible: (userSummary.recentGames || []).length > 0

                Text {
                    text: "Recently Played"
                    color: SettingsTheme.text
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }

                Text {
                    text: "View All \u203A"
                    color: SettingsTheme.accent
                    font.pixelSize: 13

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.pushRequested("recentlyPlayed", { recentGames: userSummary.recentGames || [] })
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: 3
                rowSpacing: 8
                columnSpacing: 8
                visible: (userSummary.recentGames || []).length > 0

                Repeater {
                    model: (userSummary.recentGames || []).slice(0, 3)

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        radius: 8
                        color: SettingsTheme.card
                        border.width: 1
                        border.color: SettingsTheme.border

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (modelData.gameId > 0)
                                    root.pushRequested("achievements", { raGameId: modelData.gameId, gameTitle: modelData.title })
                            }
                        }

                        Column {
                            anchors.centerIn: parent
                            width: parent.width - 16
                            spacing: 6

                            // Game icon
                            Image {
                                width: 48; height: 48
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: modelData.imageIcon ? "https://retroachievements.org" + modelData.imageIcon : ""
                                fillMode: Image.PreserveAspectFit
                                visible: status === Image.Ready
                            }

                            Text {
                                text: modelData.title || ""
                                color: SettingsTheme.text
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text {
                                text: modelData.consoleName || ""
                                color: SettingsTheme.textDim
                                font.pixelSize: 10
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }
                }
            }

            // Options
            Text {
                text: "Options"
                color: SettingsTheme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Rectangle {
                width: parent.width
                height: optionsCol.height + 24
                radius: 12
                color: SettingsTheme.card
                border.width: 1
                border.color: SettingsTheme.border

                Column {
                    id: optionsCol
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 16
                    spacing: 12

                    // Hardcore mode
                    RowLayout {
                        width: parent.width
                        spacing: 12

                        Column {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: "Hardcore Mode"
                                color: SettingsTheme.text
                                font.pixelSize: 14
                            }

                            Text {
                                text: "Disable save states and cheats for verified achievements"
                                color: SettingsTheme.textMuted
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }

                            // Frontend-approval status. Shown until RetroNest
                            // is registered on RA's approved-frontends list,
                            // so users understand local hardcore unlocks
                            // won't validate server-side yet.
                            Text {
                                text: "⚠ Unverified frontend — local hardcore unlocks will fire but won't count on your RA profile until RetroNest is approved by RetroAchievements."
                                color: Qt.rgba(0.95, 0.55, 0.10, 1)   // amber
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                width: parent.width
                                topPadding: 4
                            }
                        }

                        Rectangle {
                            width: 44
                            height: 24
                            radius: 12
                            color: hardcoreMode ? SettingsTheme.accent : SettingsTheme.border

                            Rectangle {
                                x: hardcoreMode ? parent.width - width - 3 : 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: SettingsTheme.text

                                Behavior on x { NumberAnimation { duration: 150 } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    hardcoreMode = !hardcoreMode
                                    app.raSetHardcoreMode(hardcoreMode)
                                }
                            }
                        }
                    }

                    // Separator
                    Rectangle { width: parent.width; height: 1; color: SettingsTheme.border }

                    // Encore mode
                    RowLayout {
                        width: parent.width
                        spacing: 12

                        Column {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: "Encore Mode"
                                color: SettingsTheme.text
                                font.pixelSize: 14
                            }

                            Text {
                                text: "Replay unlock notifications for achievements you've already earned (your account record is unchanged)"
                                color: SettingsTheme.textMuted
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                        }

                        Rectangle {
                            width: 44
                            height: 24
                            radius: 12
                            color: encoreMode ? SettingsTheme.accent : SettingsTheme.border

                            Rectangle {
                                x: encoreMode ? parent.width - width - 3 : 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: SettingsTheme.text

                                Behavior on x { NumberAnimation { duration: 150 } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    encoreMode = !encoreMode
                                    app.raSetEncoreMode(encoreMode)
                                }
                            }
                        }
                    }

                    // Separator
                    Rectangle { width: parent.width; height: 1; color: SettingsTheme.border }

                    // Notifications
                    RowLayout {
                        width: parent.width
                        spacing: 12

                        Column {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: "Notifications"
                                color: SettingsTheme.text
                                font.pixelSize: 14
                            }

                            Text {
                                text: "Show on-screen notifications when achievements unlock"
                                color: SettingsTheme.textMuted
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                        }

                        Rectangle {
                            width: 44
                            height: 24
                            radius: 12
                            color: notificationsEnabled ? SettingsTheme.accent : SettingsTheme.border

                            Rectangle {
                                x: notificationsEnabled ? parent.width - width - 3 : 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: SettingsTheme.text

                                Behavior on x { NumberAnimation { duration: 150 } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    notificationsEnabled = !notificationsEnabled
                                    app.raSetNotifications(notificationsEnabled)
                                }
                            }
                        }
                    }

                    // Separator
                    Rectangle { width: parent.width; height: 1; color: SettingsTheme.border }

                    // Sounds
                    RowLayout {
                        width: parent.width
                        spacing: 12

                        Column {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: "Sounds"
                                color: SettingsTheme.text
                                font.pixelSize: 14
                            }

                            Text {
                                text: "Play a sound effect when achievements unlock"
                                color: SettingsTheme.textMuted
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                        }

                        Rectangle {
                            width: 44
                            height: 24
                            radius: 12
                            color: soundsEnabled ? SettingsTheme.accent : SettingsTheme.border

                            Rectangle {
                                x: soundsEnabled ? parent.width - width - 3 : 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: SettingsTheme.text

                                Behavior on x { NumberAnimation { duration: 150 } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    soundsEnabled = !soundsEnabled
                                    app.raSetSoundEffects(soundsEnabled)
                                }
                            }
                        }
                    }

                    // Separator
                    Rectangle { width: parent.width; height: 1; color: SettingsTheme.border }

                    // ── Libretro sign-in ──────────────────────────────────
                    Column {
                        width: parent.width
                        spacing: 8

                        Text {
                            text: "Libretro Achievements Sign-in"
                            color: SettingsTheme.text
                            font.pixelSize: 14
                        }

                        Text {
                            text: "Required for achievement unlocks during in-process emulation. Not needed for standalone emulators."
                            color: SettingsTheme.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }

                        // Password field
                        Column {
                            width: parent.width
                            spacing: 4

                            Text {
                                text: "Password"
                                color: SettingsTheme.textMuted
                                font.pixelSize: 12
                            }

                            Rectangle {
                                width: parent.width
                                height: 40
                                radius: 8
                                color: SettingsTheme.base
                                border.width: 1
                                border.color: libretroPasswordField.activeFocus
                                    ? SettingsTheme.accent : SettingsTheme.border

                                TextInput {
                                    id: libretroPasswordField
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    color: SettingsTheme.text
                                    font.pixelSize: 14
                                    verticalAlignment: TextInput.AlignVCenter
                                    clip: true
                                    echoMode: TextInput.Password
                                    Keys.onReturnPressed: {
                                        if (!libretroLoggingIn
                                                && app.raUsername() !== ""
                                                && libretroPasswordField.text !== "")
                                            doLibretroLogin()
                                    }
                                }
                            }

                            Text {
                                text: "Used once to obtain a libretro login token. Not stored."
                                color: SettingsTheme.textFaint
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                                width: parent.width
                            }
                        }

                        // Sign-in button
                        Rectangle {
                            width: parent.width
                            height: 38
                            radius: 8
                            color: SettingsTheme.accent
                            opacity: (libretroLoggingIn
                                      || libretroPasswordField.text === ""
                                      || app.raUsername() === "") ? 0.4 : 1

                            Text {
                                anchors.centerIn: parent
                                text: libretroLoggingIn ? "Signing in..." : "Sign in for libretro achievements"
                                color: SettingsTheme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (!libretroLoggingIn
                                            && libretroPasswordField.text !== ""
                                            && app.raUsername() !== "")
                                        doLibretroLogin()
                                }
                            }
                        }

                        // Status / error
                        RowLayout {
                            width: parent.width
                            spacing: 6

                            Text {
                                text: libretroTokenPresent ? "✓ Signed in" : "○ Not signed in"
                                color: libretroTokenPresent ? SettingsTheme.success : SettingsTheme.textMuted
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            visible: libretroLoginError !== ""
                            text: libretroLoginError
                            color: "#ef4444"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }
                    }
                }
            }

            // Refresh button
            Rectangle {
                width: parent.width
                height: 48
                radius: 8
                color: SettingsTheme.accent
                opacity: loading ? 0.6 : 1

                Text {
                    anchors.centerIn: parent
                    text: loading ? "Loading..." : "Refresh"
                    color: SettingsTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (!loading) refreshDashboard()
                }
            }

            // Bottom spacer
            Item { width: 1; height: 20 }
        }
    }
}
