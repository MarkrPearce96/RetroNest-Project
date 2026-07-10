import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true

    property string screenState: app.hasScraperCredentials() ? "dashboard" : "login"

    onScreenStateChanged: {
        if (screenState === "login") {
            loginFocusIndex = 0
        }
    }

    // Content settings
    property var selectedMedia: []
    // System settings
    property var selectedSystems: []
    // Game filter
    property string gameFilter: "all"

    // Login form focus
    property int loginFocusIndex: 0

    // Single flat focus index across all interactive items on the dashboard
    property int focusIndex: 0

    // Reclaim focus when either login TextField loses active focus
    // (e.g., click away or Escape)
    function _maybeReclaimLoginFocus(field) {
        if (!field.activeFocus && screenState === "login" && !virtualKeyboard.visible)
            root.forceActiveFocus()
    }

    Connections {
        target: loginUserField
        function onActiveFocusChanged() { root._maybeReclaimLoginFocus(loginUserField) }
    }
    Connections {
        target: loginPassField
        function onActiveFocusChanged() { root._maybeReclaimLoginFocus(loginPassField) }
    }

    // Build flat list of all focusable items
    // Returns: [{type: "accountEdit"},
    //           {type: "system", index: i}, {type: "sysSelectAll"}, {type: "sysDeselectAll"},
    //           {type: "media", index: i}, {type: "mediaSelectAll"}, {type: "mediaDeselectAll"},
    //           {type: "filter", value: "all"}, ..., {type: "start"}]
    function buildFocusList() {
        var list = []
        // Account edit button
        list.push({type: "accountEdit"})
        // System pills
        for (var i = 0; i < scrapableSystemsList.length; i++)
            list.push({type: "system", index: i})
        // System select/deselect
        list.push({type: "sysSelectAll"})
        list.push({type: "sysDeselectAll"})
        // Media pills
        var mt = app.allMediaTypes()
        for (var j = 0; j < mt.length; j++)
            list.push({type: "media", index: j})
        // Media select/deselect
        list.push({type: "mediaSelectAll"})
        list.push({type: "mediaDeselectAll"})
        // Filter
        list.push({type: "filter", value: "all"})
        list.push({type: "filter", value: "unscraped"})
        list.push({type: "filter", value: "favorites"})
        // Start
        list.push({type: "start"})
        return list
    }

    // Group Repeater items into visual rows by their rendered y-coordinate.
    // offset is added to each index so it maps to the correct flat focus index.
    function groupByVisualRow(repeater, count, offset) {
        var buckets = [] // [{y, indices}]
        for (var i = 0; i < count; i++) {
            var item = repeater.itemAt(i)
            if (!item) continue
            var y = Math.round(item.y)
            var found = false
            for (var b = 0; b < buckets.length; b++) {
                if (Math.abs(buckets[b].y - y) < 5) {
                    buckets[b].indices.push(offset + i)
                    found = true
                    break
                }
            }
            if (!found)
                buckets.push({ y: y, indices: [offset + i] })
        }
        buckets.sort(function(a, b) { return a.y - b.y })
        var result = []
        for (var j = 0; j < buckets.length; j++)
            result.push(buckets[j].indices)
        return result
    }

    // Build visual rows of flat focus indices for spatial navigation.
    // Uses rendered y-coordinates to detect Flow wrapping, so rows
    // match what the user actually sees on screen.
    function getNavigationRows() {
        var N = scrapableSystemsList.length
        var M = app.allMediaTypes().length
        var rows = []

        // Account Edit button (index 0)
        rows.push([0])

        // System pills grouped by visual row; Select/Deselect All on first row
        // Offset by 1 because accountEdit is index 0
        var sysRows = groupByVisualRow(sysRepeater, N, 1)
        if (sysRows.length > 0) {
            sysRows[0].push(1 + N)      // sysSelectAll
            sysRows[0].push(1 + N + 1)  // sysDeselectAll
        } else {
            sysRows = [[1 + N, 1 + N + 1]]
        }
        for (var i = 0; i < sysRows.length; i++) rows.push(sysRows[i])

        // Media pills grouped by visual row; Select/Deselect All on first row
        var mediaRows = groupByVisualRow(mediaRepeater, M, 1 + N + 2)
        if (mediaRows.length > 0) {
            mediaRows[0].push(1 + N + 2 + M)      // mediaSelectAll
            mediaRows[0].push(1 + N + 2 + M + 1)  // mediaDeselectAll
        } else {
            mediaRows = [[1 + N + 2 + M, 1 + N + 2 + M + 1]]
        }
        for (var j = 0; j < mediaRows.length; j++) rows.push(mediaRows[j])

        // Filter options (each on its own visual line)
        var filterBase = 1 + N + 2 + M + 2
        rows.push([filterBase])
        rows.push([filterBase + 1])
        rows.push([filterBase + 2])
        // Start button
        rows.push([filterBase + 3])

        return rows
    }

    function findRowCol(rows, idx) {
        for (var r = 0; r < rows.length; r++) {
            var c = rows[r].indexOf(idx)
            if (c !== -1) return { row: r, col: c }
        }
        return { row: 0, col: 0 }
    }

    // Map a column position from one row to another.
    // Select/Deselect All items align from the right edge so they
    // always map to each other across rows.
    function mapColumn(fromRow, fromCol, toRow) {
        var N = scrapableSystemsList.length
        var M = app.allMediaTypes().length
        var idx = fromRow[fromCol]
        var isSelDesel = (idx === 1 + N || idx === 1 + N + 1 ||
                          idx === 1 + N + 2 + M || idx === 1 + N + 2 + M + 1)
        if (isSelDesel && toRow.length >= 2) {
            var fromEnd = fromRow.length - 1 - fromCol
            return toRow.length - 1 - fromEnd
        }
        return Math.min(fromCol, toRow.length - 1)
    }

    Keys.onPressed: function(event) {
        if (virtualKeyboard.visible) return

        // Login screen keyboard navigation
        if (screenState === "login") {
            if (event.key === Qt.Key_Down) {
                loginFocusIndex = (loginFocusIndex + 1) % 3
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                loginFocusIndex = (loginFocusIndex - 1 + 3) % 3
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                activateLoginFocused()
                event.accepted = true
            } else if (event.key === Qt.Key_Tab) {
                loginFocusIndex = (loginFocusIndex + 1) % 3
                event.accepted = true
            }
            return
        }

        // Progress screen: Return = Stop/Done, Back (controller B) = Stop/Done
        // Note: physical Escape is handled by AppWindow Shortcut, not here
        if (screenState === "progress") {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter ||
                event.key === Qt.Key_Back) {
                if (root.scrapeRunning)
                    app.cancelScrape()
                else
                    root.screenState = "dashboard"
            }
            // Consume all keys to prevent background interaction
            event.accepted = true
            return
        }

        if (screenState !== "dashboard") return

        // Account edit mode navigation
        if (root.accountEditing) {
            if (event.key === Qt.Key_Down) {
                editFocusIndex = Math.min(editFocusIndex + 1, 4)
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                editFocusIndex = Math.max(editFocusIndex - 1, 0)
                event.accepted = true
            } else if (event.key === Qt.Key_Right) {
                // Update(3) <-> SignOut(4)
                if (editFocusIndex === 3) editFocusIndex = 4
                event.accepted = true
            } else if (event.key === Qt.Key_Left) {
                if (editFocusIndex === 4) editFocusIndex = 3
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                activateEditFocused()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                root.accountEditing = false
                root.forceActiveFocus()
                event.accepted = true
            }
            return
        }

        var rows = getNavigationRows()
        var pos = findRowCol(rows, focusIndex)

        if (event.key === Qt.Key_Right) {
            var row = rows[pos.row]
            if (pos.col < row.length - 1)
                focusIndex = row[pos.col + 1]
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            var rowL = rows[pos.row]
            if (pos.col > 0)
                focusIndex = rowL[pos.col - 1]
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            if (pos.row < rows.length - 1) {
                var next = rows[pos.row + 1]
                focusIndex = next[mapColumn(rows[pos.row], pos.col, next)]
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            if (pos.row > 0) {
                var prev = rows[pos.row - 1]
                focusIndex = prev[mapColumn(rows[pos.row], pos.col, prev)]
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            activateFocused()
            event.accepted = true
        }
    }

    function activateFocused() {
        var list = buildFocusList()
        if (focusIndex >= list.length) return
        var item = list[focusIndex]
        if (item.type === "accountEdit") {
            root.accountEditing = !root.accountEditing
            root.editFocusIndex = 0
            root.forceActiveFocus()
        } else if (item.type === "system") {
            var sys = scrapableSystemsList[item.index]
            if (sys) toggleSystem(sys.id)
        } else if (item.type === "sysSelectAll") {
            resetSystemSelection()
        } else if (item.type === "sysDeselectAll") {
            selectedSystems = []
        } else if (item.type === "media") {
            var mt = app.allMediaTypes()
            if (item.index < mt.length) toggleMedia(mt[item.index])
        } else if (item.type === "mediaSelectAll") {
            resetMediaSelection()
        } else if (item.type === "mediaDeselectAll") {
            selectedMedia = []
        } else if (item.type === "filter") {
            gameFilter = item.value
        } else if (item.type === "start") {
            if (selectedSystems.length > 0 && selectedMedia.length > 0) {
                progressLog.clear()
                progressCurrent = 0; progressTotal = 0
                scrapeRunning = true
                scrapeTitle = ""; scrapeDescription = ""
                scrapeCoverPath = ""; scrapeScreenshotPath = ""
                scrapeStatus = ""
                apiRequestsToday = 0; apiMaxRequests = 0
                scrapeSucceeded = 0; scrapeFailed = 0; scrapeSkipped = 0
                var systemNames = selectedSystems.join(", ").toUpperCase()
                scrapeSystemLabel = systemNames
                scrapeSystemGameCount = selectedSystems.length
                screenState = "progress"
                app.startBatchScrape(selectedMedia, selectedSystems, gameFilter)
            }
        }
    }

    function activateLoginFocused() {
        if (loginFocusIndex === 0) {
            // Username field
            if (inputManager.lastInputWasController) {
                virtualKeyboard.open(loginUserField.text, false, "USERNAME")
            } else {
                loginUserField.forceActiveFocus()
            }
        } else if (loginFocusIndex === 1) {
            // Password field
            if (inputManager.lastInputWasController) {
                virtualKeyboard.open(loginPassField.text, true, "PASSWORD")
            } else {
                loginPassField.forceActiveFocus()
            }
        } else if (loginFocusIndex === 2) {
            // Sign In button
            if (signInBtn._enabled) {
                signInBtn._enabled = false
                loginError.visible = false
                app.validateScraperCredentials(loginUserField.text, loginPassField.text)
            }
        }
    }

    function activateEditFocused() {
        if (editFocusIndex === 0) {
            // Cancel — close edit mode
            root.accountEditing = false
            root.forceActiveFocus()
        } else if (editFocusIndex === 1) {
            // Username field
            if (inputManager.lastInputWasController) {
                virtualKeyboard.open(acctUserField.text, false, "USERNAME")
            } else {
                acctUserField.forceActiveFocus()
            }
        } else if (editFocusIndex === 2) {
            // Password field
            if (inputManager.lastInputWasController) {
                virtualKeyboard.open(acctPassField.text, true, "PASSWORD")
            } else {
                acctPassField.forceActiveFocus()
            }
        } else if (editFocusIndex === 3) {
            // Update button
            app.validateScraperCredentials(acctUserField.text, acctPassField.text)
        } else if (editFocusIndex === 4) {
            // Sign Out button
            app.scraperSignOut()
        }
    }

    // Progress tracking
    property int progressCurrent: 0
    property int progressTotal: 0
    property string progressCurrentGame: ""
    property bool scrapeRunning: false

    // Live game detail (from rich signal)
    property string scrapeTitle: ""
    property string scrapeDescription: ""
    property string scrapeDeveloper: ""
    property string scrapePublisher: ""
    property string scrapeReleaseDate: ""
    property string scrapeGenres: ""
    property real scrapeRating: 0.0
    property string scrapePlayers: ""
    property string scrapeCoverPath: ""
    property string scrapeScreenshotPath: ""
    property string scrapeStatus: ""
    property int apiRequestsToday: 0
    property int apiMaxRequests: 0

    // Completion summary
    property int scrapeSucceeded: 0
    property int scrapeFailed: 0
    property int scrapeSkipped: 0

    // System info for header
    property string scrapeSystemLabel: ""
    property int scrapeSystemGameCount: 0

    // Dynamic systems list (refreshed on navigation)
    property var scrapableSystemsList: []

    // Account editing toggle
    property bool accountEditing: false
    property int editFocusIndex: 0  // 0=cancel, 1=username, 2=password, 3=update, 4=signOut

    ListModel { id: progressLog }

    Component.onCompleted: {
        resetMediaSelection()
        resetSystemSelection()
    }

    function resetMediaSelection() {
        selectedMedia = app.allMediaTypes()
    }

    function resetSystemSelection() {
        var systems = app.scrapableSystems()
        root.scrapableSystemsList = systems
        var ids = []
        for (var i = 0; i < systems.length; i++)
            ids.push(systems[i].id)
        selectedSystems = ids
    }

    function isMediaSelected(type) {
        return selectedMedia.indexOf(type) >= 0
    }

    function toggleMedia(type) {
        var list = selectedMedia.slice()
        var idx = list.indexOf(type)
        if (idx >= 0) list.splice(idx, 1)
        else list.push(type)
        selectedMedia = list
    }

    function isSystemSelected(id) {
        return selectedSystems.indexOf(id) >= 0
    }

    function toggleSystem(id) {
        var list = selectedSystems.slice()
        var idx = list.indexOf(id)
        if (idx >= 0) list.splice(idx, 1)
        else list.push(id)
        selectedSystems = list
    }

    Connections {
        target: app
        function onScraperCredentialsValidated(success, message) {
            if (success) {
                root.screenState = "dashboard"
                loginError.text = ""
                root.accountEditing = false
            } else {
                loginError.text = message
                loginError.visible = true
            }
            signInBtn._enabled = true
        }
        function onScraperSignedOut() {
            root.screenState = "login"
        }
        function onScrapeProgress(current, total, gameData) {
            // Auto-switch to progress view (e.g. when scrape triggered from game action popup)
            if (root.screenState !== "progress") {
                root.scrapeRunning = true
                root.screenState = "progress"
            }
            root.progressCurrent = current
            root.progressTotal = total
            root.progressCurrentGame = gameData.gameName || ""
            root.scrapeStatus = gameData.status || ""

            // Skip the initial "scraping" signal (no data yet)
            if (gameData.status === "scraping")
                return

            // Update metadata fields (from both "downloading" and final signals)
            if (gameData.scrapedTitle) {
                root.scrapeTitle = gameData.scrapedTitle || ""
                root.scrapeDescription = gameData.description || ""
                root.scrapeDeveloper = gameData.developer || ""
                root.scrapePublisher = gameData.publisher || ""
                root.scrapeReleaseDate = gameData.releaseDate || ""
                root.scrapeGenres = gameData.genres || ""
                root.scrapeRating = gameData.rating || 0.0
                root.scrapePlayers = gameData.players || ""
            }

            // Update cover/screenshot (only available in final signal after download)
            if (gameData.coverPath)
                root.scrapeCoverPath = gameData.coverPath
            if (gameData.screenshotPath)
                root.scrapeScreenshotPath = gameData.screenshotPath

            if (gameData.requestsToday !== undefined) {
                root.apiRequestsToday = gameData.requestsToday
                root.apiMaxRequests = gameData.maxRequests
            }

            // Append to log only on final signals (not "downloading")
            if (gameData.status !== "downloading") {
                var status = gameData.status || ""
                progressLog.append({
                    "gameName": gameData.gameName || "",
                    "status": status,
                    "isSuccess": status.indexOf("media downloaded") >= 0 || status.indexOf("media (") >= 0,
                    "isPartial": status.indexOf("not available") >= 0,
                    "isFailed": status.indexOf("failed") >= 0
                })
            }
        }
        function onScrapeFinished(succeeded, failed, skipped) {
            root.scrapeRunning = false
            root.scrapeSucceeded = succeeded
            root.scrapeFailed = failed
            root.scrapeSkipped = skipped
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: {
            switch (root.screenState) {
                case "login": return 0
                case "dashboard": return 1
                case "progress": return 2
                default: return 0
            }
        }

        // ====================================================================
        // State 0: LOGIN
        // ====================================================================
        Item {
            id: scraperLoginPage

            Rectangle {
                anchors.centerIn: parent
                width: 360
                height: loginCardCol.height + 48
                radius: 12
                color: SettingsTheme.card
                border.width: 1
                border.color: SettingsTheme.border

                Column {
                    id: loginCardCol
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 16

                    // ScreenScraper Logo
                    Image {
                        width: 64
                        height: 64
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "images/screenscraper_logo.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        text: "ScreenScraper"
                        color: SettingsTheme.text
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: "Enter your ScreenScraper.fr credentials to download media and metadata for your games."
                        color: SettingsTheme.textMuted
                        font.pixelSize: 13
                        width: parent.width
                        wrapMode: Text.WordWrap
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
                            border.color: (root.screenState === "login" && root.loginFocusIndex === 0) || loginUserField.activeFocus
                                ? SettingsTheme.accent : SettingsTheme.border

                            TextField {
                                id: loginUserField
                                anchors.fill: parent
                                placeholderText: "screenscraper.fr username"
                                placeholderTextColor: SettingsTheme.textDim
                                color: SettingsTheme.text
                                font.pixelSize: 14
                                background: Item {}
                                leftPadding: 10
                                rightPadding: 10

                                function _moveToPassword() {
                                    loginFocusIndex = 1
                                    loginPassField.forceActiveFocus()
                                }

                                Keys.onTabPressed: loginUserField._moveToPassword()
                                Keys.onReturnPressed: loginUserField._moveToPassword()
                                Keys.onEnterPressed: loginUserField._moveToPassword()
                            }
                        }
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
                            border.color: (root.screenState === "login" && root.loginFocusIndex === 1) || loginPassField.activeFocus
                                ? SettingsTheme.accent : SettingsTheme.border

                            TextField {
                                id: loginPassField
                                anchors.fill: parent
                                placeholderText: "screenscraper.fr password"
                                placeholderTextColor: SettingsTheme.textDim
                                color: SettingsTheme.text
                                font.pixelSize: 14
                                echoMode: TextInput.Password
                                background: Item {}
                                leftPadding: 10
                                rightPadding: 10

                                function _submitLogin() {
                                    if (signInBtn._enabled) {
                                        signInBtn._enabled = false
                                        loginError.visible = false
                                        app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                                    }
                                }

                                Keys.onTabPressed: {
                                    loginFocusIndex = 2
                                    root.forceActiveFocus()
                                }
                                Keys.onReturnPressed: loginPassField._submitLogin()
                                Keys.onEnterPressed: loginPassField._submitLogin()
                            }
                        }
                    }

                    // Connect button
                    Rectangle {
                        id: signInBtn
                        property bool _enabled: true
                        width: parent.width
                        height: 42
                        radius: 8
                        color: (root.screenState === "login" && root.loginFocusIndex === 2)
                            ? Qt.lighter(SettingsTheme.accent, 1.2)
                            : SettingsTheme.accent
                        opacity: _enabled ? 1.0 : 0.6

                        Text {
                            anchors.centerIn: parent
                            text: signInBtn._enabled ? "Connect" : "Validating..."
                            color: SettingsTheme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            enabled: signInBtn._enabled
                            onClicked: {
                                signInBtn._enabled = false
                                loginError.visible = false
                                app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                            }
                        }
                    }

                    // Error text
                    Text {
                        id: loginError
                        visible: false
                        text: ""
                        color: SettingsTheme.error
                        font.pixelSize: 12
                        width: parent.width
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        // ====================================================================
        // State 1: DASHBOARD
        // ====================================================================
        Flickable {
            contentHeight: dashCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: dashCol
                width: parent.width
                spacing: SettingsTheme.sectionSpacing

                Item { height: 4 }

                // ── ACCOUNT section ─────────────────────────────────────
                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 8

                    Text {
                        text: "ACCOUNT"
                        color: SettingsTheme.textMuted
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.0
                    }

                    // Account card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: accountCardContent.implicitHeight + 24
                        radius: SettingsTheme.cardRadius
                        color: SettingsTheme.card
                        border.width: 1
                        border.color: SettingsTheme.border

                        ColumnLayout {
                            id: accountCardContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 10

                            // Connected row (always visible)
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    color: SettingsTheme.success
                                }
                                Text {
                                    text: app.scraperUsername()
                                    color: SettingsTheme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: "Connected"
                                    color: SettingsTheme.success
                                    font.pixelSize: 12
                                }

                                Item { Layout.fillWidth: true }

                                Rectangle {
                                    id: editBtn
                                    property bool isFocused: root.screenState === "dashboard"
                                        && ((root.accountEditing && root.editFocusIndex === 0)
                                            || (!root.accountEditing && root.focusIndex === 0))
                                    width: editLabel.implicitWidth + 16
                                    height: editLabel.implicitHeight + 8
                                    radius: 4
                                    color: "transparent"
                                    border.width: isFocused ? 2 : 0
                                    border.color: SettingsTheme.focusBorder

                                    Text {
                                        id: editLabel
                                        anchors.centerIn: parent
                                        text: root.accountEditing ? "Cancel" : "Edit"
                                        color: SettingsTheme.accent
                                        font.pixelSize: 12
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            root.accountEditing = !root.accountEditing
                                            root.forceActiveFocus()
                                        }
                                    }

                                    // Focus glow
                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.margins: -3
                                        radius: parent.radius + 3
                                        color: "transparent"
                                        border.width: 2
                                        border.color: SettingsTheme.focusBorder
                                        opacity: editBtn.isFocused ? 0.3 : 0
                                        z: -1
                                        visible: opacity > 0
                                        Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                                    }
                                }
                            }

                            // Edit fields (shown when editing)
                            ColumnLayout {
                                visible: root.accountEditing
                                Layout.fillWidth: true
                                spacing: 8

                                // Username field
                                ColumnLayout {
                                    spacing: 4
                                    Text {
                                        text: "Username"
                                        color: SettingsTheme.textMuted
                                        font.pixelSize: 11
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 32
                                        radius: 4
                                        color: SettingsTheme.base
                                        border.width: (root.accountEditing && root.editFocusIndex === 1) ? 2 : 1
                                        border.color: (root.accountEditing && root.editFocusIndex === 1) || acctUserField.activeFocus
                                            ? SettingsTheme.focusBorder : SettingsTheme.border

                                        Rectangle {
                                            anchors.fill: parent; anchors.margins: -4; radius: parent.radius + 4
                                            color: "transparent"; border.width: 2; border.color: SettingsTheme.focusBorder
                                            opacity: (root.accountEditing && root.editFocusIndex === 1) ? 0.3 : 0
                                            z: -1; visible: opacity > 0
                                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                                        }

                                        TextField {
                                            id: acctUserField
                                            anchors.fill: parent
                                            text: app.scraperUsername()
                                            color: SettingsTheme.text
                                            font.pixelSize: 12
                                            background: Item {}
                                            leftPadding: 8
                                        }
                                    }
                                }

                                // Password field
                                ColumnLayout {
                                    spacing: 4
                                    Text {
                                        text: "Password"
                                        color: SettingsTheme.textMuted
                                        font.pixelSize: 11
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 32
                                        radius: 4
                                        color: SettingsTheme.base
                                        border.width: (root.accountEditing && root.editFocusIndex === 2) ? 2 : 1
                                        border.color: (root.accountEditing && root.editFocusIndex === 2) || acctPassField.activeFocus
                                            ? SettingsTheme.focusBorder : SettingsTheme.border

                                        Rectangle {
                                            anchors.fill: parent; anchors.margins: -4; radius: parent.radius + 4
                                            color: "transparent"; border.width: 2; border.color: SettingsTheme.focusBorder
                                            opacity: (root.accountEditing && root.editFocusIndex === 2) ? 0.3 : 0
                                            z: -1; visible: opacity > 0
                                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                                        }

                                        TextField {
                                            id: acctPassField
                                            anchors.fill: parent
                                            placeholderText: "Enter new password"
                                            placeholderTextColor: SettingsTheme.textDim
                                            color: SettingsTheme.text
                                            font.pixelSize: 12
                                            echoMode: TextInput.Password
                                            background: Item {}
                                            leftPadding: 8
                                        }
                                    }
                                }

                                // Buttons row
                                RowLayout {
                                    spacing: 8

                                    Rectangle {
                                        id: updateBtn
                                        property bool isFocused: root.accountEditing && root.editFocusIndex === 3
                                        width: updateLabel.implicitWidth + 24
                                        height: 32
                                        radius: SettingsTheme.buttonRadius
                                        color: SettingsTheme.accent
                                        border.width: isFocused ? 2 : 0
                                        border.color: SettingsTheme.text

                                        Rectangle {
                                            anchors.fill: parent; anchors.margins: -4; radius: parent.radius + 4
                                            color: "transparent"; border.width: 2; border.color: SettingsTheme.focusBorder
                                            opacity: updateBtn.isFocused ? 0.3 : 0
                                            z: -1; visible: opacity > 0
                                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                                        }

                                        Text {
                                            id: updateLabel
                                            anchors.centerIn: parent
                                            text: "Update"
                                            color: SettingsTheme.background
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: app.validateScraperCredentials(acctUserField.text, acctPassField.text)
                                        }
                                    }

                                    Rectangle {
                                        id: signOutBtn
                                        property bool isFocused: root.accountEditing && root.editFocusIndex === 4
                                        width: signOutLabel.implicitWidth + 24
                                        height: 32
                                        radius: SettingsTheme.buttonRadius
                                        color: SettingsTheme.card
                                        border.width: isFocused ? 2 : 1
                                        border.color: isFocused ? SettingsTheme.focusBorder : SettingsTheme.border

                                        Rectangle {
                                            anchors.fill: parent; anchors.margins: -4; radius: parent.radius + 4
                                            color: "transparent"; border.width: 2; border.color: SettingsTheme.focusBorder
                                            opacity: signOutBtn.isFocused ? 0.3 : 0
                                            z: -1; visible: opacity > 0
                                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                                        }

                                        Text {
                                            id: signOutLabel
                                            anchors.centerIn: parent
                                            text: "Sign Out"
                                            color: SettingsTheme.error
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: app.scraperSignOut()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ── SYSTEMS section ─────────────────────────────────────
                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "SYSTEMS"
                            color: SettingsTheme.textMuted
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.0
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: selAllSysText.implicitWidth + 16
                            height: selAllSysText.implicitHeight + 8
                            radius: 4
                            color: root.focusIndex === (1 + root.scrapableSystemsList.length) ? SettingsTheme.card : "transparent"
                            border.width: root.focusIndex === (1 + root.scrapableSystemsList.length) ? 2 : 0
                            border.color: SettingsTheme.text
                            Text {
                                id: selAllSysText
                                anchors.centerIn: parent
                                text: "Select All"
                                color: SettingsTheme.accent
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.resetSystemSelection()
                            }
                        }
                        Rectangle {
                            width: deselAllSysText.implicitWidth + 16
                            height: deselAllSysText.implicitHeight + 8
                            radius: 4
                            color: root.focusIndex === (1 + root.scrapableSystemsList.length + 1) ? SettingsTheme.card : "transparent"
                            border.width: root.focusIndex === (1 + root.scrapableSystemsList.length + 1) ? 2 : 0
                            border.color: SettingsTheme.text
                            Text {
                                id: deselAllSysText
                                anchors.centerIn: parent
                                text: "Deselect All"
                                color: SettingsTheme.accent
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.selectedSystems = []
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: false
                        Layout.preferredWidth: parent.width * 0.75
                        spacing: 6

                        Repeater {
                            id: sysRepeater
                            model: root.scrapableSystemsList
                            delegate: Rectangle {
                                property bool active: root.isSystemSelected(modelData.id)
                                property bool pillFocused: root.focusIndex === (1 + index)
                                width: sysPillText.implicitWidth + 48
                                height: 42
                                radius: 12
                                color: active ? SettingsTheme.accent : SettingsTheme.border
                                border.width: pillFocused ? 3 : 0
                                border.color: pillFocused ? SettingsTheme.text : "transparent"
                                scale: pillFocused ? 1.05 : 1.0
                                Behavior on scale { NumberAnimation { duration: 100 } }
                                Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }

                                Text {
                                    id: sysPillText
                                    anchors.centerIn: parent
                                    text: modelData.name
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: active ? SettingsTheme.background : SettingsTheme.textMuted
                                    Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.toggleSystem(modelData.id)
                                }
                            }
                        }
                    }
                }

                // ── MEDIA section ───────────────────────────────────────
                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "MEDIA"
                            color: SettingsTheme.textMuted
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.0
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            property var _mediaTypes: app.allMediaTypes()
                            width: selAllMediaText.implicitWidth + 16
                            height: selAllMediaText.implicitHeight + 8
                            radius: 4
                            property int _off: 1 + root.scrapableSystemsList.length + 2 + _mediaTypes.length
                            color: root.focusIndex === _off ? SettingsTheme.card : "transparent"
                            border.width: root.focusIndex === _off ? 2 : 0
                            border.color: SettingsTheme.text
                            Text {
                                id: selAllMediaText
                                anchors.centerIn: parent
                                text: "Select All"
                                color: SettingsTheme.accent
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.resetMediaSelection()
                            }
                        }
                        Rectangle {
                            property var _mediaTypes2: app.allMediaTypes()
                            width: deselAllMediaText.implicitWidth + 16
                            height: deselAllMediaText.implicitHeight + 8
                            radius: 4
                            property int _off2: 1 + root.scrapableSystemsList.length + 2 + _mediaTypes2.length + 1
                            color: root.focusIndex === _off2 ? SettingsTheme.card : "transparent"
                            border.width: root.focusIndex === _off2 ? 2 : 0
                            border.color: SettingsTheme.text
                            Text {
                                id: deselAllMediaText
                                anchors.centerIn: parent
                                text: "Deselect All"
                                color: SettingsTheme.accent
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.selectedMedia = []
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: false
                        Layout.preferredWidth: parent.width * 0.75
                        spacing: 6

                        Repeater {
                            id: mediaRepeater
                            model: app.allMediaTypes()
                            delegate: Rectangle {
                                property bool active: root.isMediaSelected(modelData)
                                property bool pillFocused: root.focusIndex === (1 + root.scrapableSystemsList.length + 2 + index)
                                width: mediaPillText.implicitWidth + 48
                                height: 42
                                radius: 12
                                color: active ? SettingsTheme.accent : SettingsTheme.border
                                border.width: pillFocused ? 3 : 0
                                border.color: pillFocused ? SettingsTheme.text : "transparent"
                                scale: pillFocused ? 1.05 : 1.0
                                Behavior on scale { NumberAnimation { duration: 100 } }
                                Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }

                                Text {
                                    id: mediaPillText
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: active ? SettingsTheme.background : SettingsTheme.textMuted
                                    Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.toggleMedia(modelData)
                                }
                            }
                        }
                    }
                }

                // ── FILTER section ──────────────────────────────────────
                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 4

                    Text {
                        text: "FILTER"
                        color: SettingsTheme.textMuted
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.0
                        Layout.bottomMargin: 4
                    }

                    Repeater {
                        model: [
                            { label: "All Games", value: "all" },
                            { label: "Unscraped Only", value: "unscraped" },
                            { label: "Favorites Only", value: "favorites" }
                        ]
                        delegate: Rectangle {
                            property bool active: root.gameFilter === modelData.value
                            property int _filterOff: 1 + root.scrapableSystemsList.length + 2 + app.allMediaTypes().length + 2
                            property bool rowFocused: root.focusIndex === (_filterOff + index)
                            width: filterRowContent.implicitWidth + 32
                            height: 38
                            radius: 8
                            color: SettingsTheme.card
                            border.width: rowFocused ? 2 : 1
                            border.color: rowFocused ? SettingsTheme.focusBorder : SettingsTheme.border

                            Row {
                                id: filterRowContent
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: active ? SettingsTheme.accent : SettingsTheme.border

                                    Rectangle {
                                        visible: active
                                        anchors.centerIn: parent
                                        width: 14; height: 14; radius: 7
                                        color: "transparent"
                                        border.width: 0
                                        // Glow effect
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 16; height: 16; radius: 8
                                            color: SettingsTheme.accent
                                            opacity: 0.3
                                            visible: active
                                        }
                                    }
                                }

                                Text {
                                    text: modelData.label
                                    font.pixelSize: 13
                                    color: active ? SettingsTheme.text : SettingsTheme.textMuted
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.gameFilter = modelData.value
                            }
                        }
                    }
                }

                // ── GAME COUNT summary ─────────────────────────────────
                Text {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    horizontalAlignment: Text.AlignHCenter
                    text: {
                        var count = app.scrapeGameCount(root.selectedSystems, root.gameFilter)
                        if (count === 0) return "No games match this selection"
                        return count + (count === 1 ? " game" : " games") + " will be scraped"
                    }
                    color: SettingsTheme.textMuted
                    font.pixelSize: 13
                }

                // ── START SCRAPING button ───────────────────────────────
                Rectangle {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                    height: 48
                    radius: 6
                    property int _startOff: 1 + root.scrapableSystemsList.length + 2 + app.allMediaTypes().length + 2 + 3
                    property bool _focused: root.focusIndex === _startOff
                    property bool _enabled: root.selectedSystems.length > 0 && root.selectedMedia.length > 0
                    border.width: _focused ? 3 : 0
                    border.color: _focused ? SettingsTheme.text : "transparent"
                    color: _enabled ? (_focused ? Qt.lighter(SettingsTheme.accent, 1.3) : SettingsTheme.accent)
                                    : SettingsTheme.card
                    opacity: _enabled ? 1.0 : 0.5
                    scale: _focused ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent
                        text: "Start Scraping"
                        color: SettingsTheme.background
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        enabled: root.selectedSystems.length > 0 && root.selectedMedia.length > 0
                        onClicked: {
                            progressLog.clear()
                            root.progressCurrent = 0
                            root.progressTotal = 0
                            root.scrapeRunning = true
                            root.scrapeTitle = ""
                            root.scrapeDescription = ""
                            root.scrapeCoverPath = ""
                            root.scrapeScreenshotPath = ""
                            root.scrapeStatus = ""
                            root.apiRequestsToday = 0
                            root.apiMaxRequests = 0
                            root.scrapeSucceeded = 0
                            root.scrapeFailed = 0
                            root.scrapeSkipped = 0

                            // Build system label for header
                            var systemNames = root.selectedSystems.join(", ").toUpperCase()
                            root.scrapeSystemLabel = systemNames
                            root.scrapeSystemGameCount = root.selectedSystems.length

                            root.screenState = "progress"
                            app.startBatchScrape(root.selectedMedia, root.selectedSystems, root.gameFilter)
                        }
                    }
                }

                Item { height: 24 }
            }
        }

        // ====================================================================
        // State 2: PROGRESS
        // ====================================================================
        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // ── Header ──────────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 24
                    spacing: 4

                    Text {
                        text: root.scrapeRunning ? "SCRAPING IN PROGRESS" : "SCRAPING COMPLETE"
                        color: root.scrapeRunning ? SettingsTheme.text : SettingsTheme.success
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.letterSpacing: 1.5
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: root.scrapeSystemLabel + " [" + root.selectedSystems.length + (root.selectedSystems.length === 1 ? " SYSTEM]" : " SYSTEMS]")
                        color: SettingsTheme.textMuted
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                        visible: root.scrapeSystemLabel !== ""
                    }

                    Text {
                        text: root.progressTotal > 0
                            ? "GAME " + root.progressCurrent + " OF " + root.progressTotal + " \u2014 " + root.progressCurrentGame
                            : ""
                        color: SettingsTheme.textDim
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignHCenter
                        visible: root.progressTotal > 0
                    }

                    // Progress bar
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Layout.topMargin: 8
                        height: 4
                        radius: 2
                        color: SettingsTheme.border

                        Rectangle {
                            width: root.progressTotal > 0 ? parent.width * (root.progressCurrent / root.progressTotal) : 0
                            height: parent.height
                            radius: 2
                            color: root.scrapeRunning ? SettingsTheme.accent : SettingsTheme.success
                            Behavior on width { NumberAnimation { duration: 200 } }
                        }
                    }
                }

                // ── Divider ─────────────────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    height: 1
                    color: SettingsTheme.border
                }

                // ── Detail Card (Option F — large cover + stacked metadata) ──
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.topMargin: 16

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 8
                        visible: root.scrapeTitle !== "" || root.scrapeStatus.indexOf("failed") >= 0

                        // Game title
                        Text {
                            text: (root.scrapeTitle || root.progressCurrentGame).toUpperCase()
                            color: SettingsTheme.text
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            font.letterSpacing: 0.3
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.bottomMargin: 8
                        }

                        // Cover + Metadata side by side
                        Row {
                            Layout.fillWidth: true
                            spacing: 16

                            // Cover art
                            Rectangle {
                                width: 300
                                height: 330
                                radius: SettingsTheme.cardRadius
                                color: "transparent"
                                clip: true

                                Image {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    source: root.scrapeCoverPath !== "" ? "file://" + root.scrapeCoverPath : ""
                                    fillMode: Image.PreserveAspectFit
                                    verticalAlignment: Image.AlignTop
                                    asynchronous: true
                                    cache: false
                                    visible: root.scrapeCoverPath !== ""
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "COVER"
                                    color: SettingsTheme.textGhost
                                    font.pixelSize: 10
                                    visible: root.scrapeCoverPath === ""
                                }
                            }

                            // Stacked metadata
                            Column {
                                spacing: 10
                                width: parent.width - 300 - parent.spacing

                                // Rating
                                Row {
                                    spacing: 2
                                    visible: root.scrapeRating > 0
                                    Repeater {
                                        model: 5
                                        Text {
                                            text: {
                                                var filled = Math.floor(root.scrapeRating)
                                                var half = root.scrapeRating - filled >= 0.5
                                                if (index < filled) return "\u2605"
                                                if (index === filled && half) return "\u2605"
                                                return "\u2606"
                                            }
                                            color: index < Math.ceil(root.scrapeRating) ? SettingsTheme.accent : SettingsTheme.textDim
                                            font.pixelSize: 17
                                        }
                                    }
                                }

                                // Released
                                Column {
                                    spacing: 1
                                    visible: root.scrapeReleaseDate !== ""
                                    Text { text: "RELEASED"; color: SettingsTheme.textFaint; font.pixelSize: 11; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapeReleaseDate; color: SettingsTheme.textMuted; font.pixelSize: 14 }
                                }

                                // Developer
                                Column {
                                    spacing: 1
                                    visible: root.scrapeDeveloper !== ""
                                    Text { text: "DEVELOPER"; color: SettingsTheme.textFaint; font.pixelSize: 11; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapeDeveloper; color: SettingsTheme.textMuted; font.pixelSize: 14 }
                                }

                                // Publisher
                                Column {
                                    spacing: 1
                                    visible: root.scrapePublisher !== ""
                                    Text { text: "PUBLISHER"; color: SettingsTheme.textFaint; font.pixelSize: 11; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapePublisher; color: SettingsTheme.textMuted; font.pixelSize: 14 }
                                }

                                // Genre
                                Column {
                                    spacing: 1
                                    visible: root.scrapeGenres !== ""
                                    Text { text: "GENRE"; color: SettingsTheme.textFaint; font.pixelSize: 11; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapeGenres; color: SettingsTheme.textMuted; font.pixelSize: 14 }
                                }

                                // Players
                                Column {
                                    spacing: 1
                                    visible: root.scrapePlayers !== ""
                                    Text { text: "PLAYERS"; color: SettingsTheme.textFaint; font.pixelSize: 11; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapePlayers; color: SettingsTheme.textMuted; font.pixelSize: 14 }
                                }

                                // Status
                                Text {
                                    text: {
                                        if (root.scrapeStatus === "scraping" || root.scrapeStatus === "downloading")
                                            return "Downloading media..."
                                        return root.scrapeStatus
                                    }
                                    color: {
                                        if (root.scrapeStatus.indexOf("failed") >= 0) return SettingsTheme.error
                                        if (root.scrapeStatus.indexOf("not available") >= 0) return SettingsTheme.warning
                                        if (root.scrapeStatus === "downloading") return SettingsTheme.textMuted
                                        return SettingsTheme.success
                                    }
                                    font.pixelSize: 13
                                    visible: root.scrapeStatus !== "" && root.scrapeStatus !== "scraping"
                                }
                            }
                        }

                        // Divider before description
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: SettingsTheme.border
                            visible: root.scrapeDescription !== ""
                        }

                        // Description (auto-scrolling)
                        Flickable {
                            id: descFlickable
                            Layout.fillWidth: true
                            Layout.preferredHeight: 200
                            contentWidth: width
                            contentHeight: descText.height
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            visible: root.scrapeDescription !== ""

                            Text {
                                id: descText
                                width: descFlickable.width
                                text: root.scrapeDescription
                                color: SettingsTheme.textDim
                                font.pixelSize: 13
                                lineHeight: 1.5
                                wrapMode: Text.WordWrap
                            }

                            // Auto-scroll animation
                            SequentialAnimation {
                                id: descScrollAnim
                                loops: Animation.Infinite

                                // Pause at top
                                PauseAnimation { duration: 2000 }

                                // Scroll down
                                NumberAnimation {
                                    target: descFlickable
                                    property: "contentY"
                                    from: 0
                                    to: Math.max(0, descText.height - descFlickable.height)
                                    duration: Math.max(0, (descText.height - descFlickable.height) * 15)
                                    easing.type: Easing.Linear
                                }

                                // Pause at bottom
                                PauseAnimation { duration: 2000 }

                                // Reset to top
                                NumberAnimation {
                                    target: descFlickable
                                    property: "contentY"
                                    to: 0
                                    duration: 300
                                    easing.type: Easing.OutQuad
                                }
                            }

                            // Start/restart scroll when description changes and overflows
                            onContentHeightChanged: {
                                descScrollAnim.stop()
                                contentY = 0
                                if (descText.height > height)
                                    descScrollAnim.start()
                            }
                        }
                    }

                    // Placeholder when no data yet
                    Text {
                        anchors.centerIn: parent
                        text: root.scrapeRunning ? "Waiting for first result..." : ""
                        color: SettingsTheme.textDim
                        font.pixelSize: 14
                        visible: root.scrapeTitle === "" && root.scrapeStatus.indexOf("failed") < 0
                    }
                }

                // ── Footer ──────────────────────────────────────────────

                // Stats row (only visible when complete)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    visible: !root.scrapeRunning && root.progressTotal > 0

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: SettingsTheme.border
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        // Total
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 50

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "" + (root.scrapeSucceeded + root.scrapeFailed + root.scrapeSkipped)
                                    color: SettingsTheme.text
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "TOTAL"
                                    color: SettingsTheme.textFaint
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }

                            // Right border
                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: SettingsTheme.border
                            }
                        }

                        // Succeeded
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 50

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "" + root.scrapeSucceeded
                                    color: SettingsTheme.success
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "SUCCEEDED"
                                    color: SettingsTheme.success
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }

                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: SettingsTheme.border
                                visible: root.scrapeFailed > 0 || root.scrapeSkipped > 0
                            }
                        }

                        // Failed (only if > 0)
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 50
                            visible: root.scrapeFailed > 0

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "" + root.scrapeFailed
                                    color: SettingsTheme.error
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "FAILED"
                                    color: SettingsTheme.error
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }

                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: SettingsTheme.border
                                visible: root.scrapeSkipped > 0
                            }
                        }

                        // Skipped (only if > 0)
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 50
                            visible: root.scrapeSkipped > 0

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "" + root.scrapeSkipped
                                    color: SettingsTheme.textMuted
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "SKIPPED"
                                    color: SettingsTheme.textFaint
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }
                }

                // API + button row (always visible)
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: SettingsTheme.border
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 16
                    spacing: 12

                    // API quota
                    Text {
                        text: root.apiMaxRequests > 0
                            ? "API CALLS: " + root.apiRequestsToday + "/" + root.apiMaxRequests
                            : ""
                        color: SettingsTheme.textFaint
                        font.pixelSize: 11
                        font.letterSpacing: 0.5
                        visible: root.apiMaxRequests > 0
                    }

                    Item { Layout.fillWidth: true }

                    // Progress counter
                    Text {
                        text: root.progressTotal > 0 ? root.progressCurrent + " / " + root.progressTotal : ""
                        color: SettingsTheme.textMuted
                        font.pixelSize: 12
                        visible: root.progressTotal > 0
                    }

                    // Stop / Done button
                    Rectangle {
                        width: 120
                        height: 36
                        radius: 6
                        color: root.scrapeRunning ? SettingsTheme.errorDim : SettingsTheme.accentDim
                        border.width: 1
                        border.color: root.scrapeRunning ? SettingsTheme.error : SettingsTheme.accent

                        Text {
                            anchors.centerIn: parent
                            text: root.scrapeRunning ? "STOP" : "DONE"
                            color: root.scrapeRunning ? SettingsTheme.error : SettingsTheme.accent
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.5
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.scrapeRunning) {
                                    app.cancelScrape()
                                } else {
                                    root.screenState = "dashboard"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Virtual Keyboard ──
    VirtualKeyboard {
        id: virtualKeyboard

        onAccepted: {
            // Write text back to the active field
            if (screenState === "login") {
                if (loginFocusIndex === 0) {
                    loginUserField.text = virtualKeyboard.text
                } else if (loginFocusIndex === 1) {
                    loginPassField.text = virtualKeyboard.text
                }
            } else if (screenState === "dashboard") {
                if (virtualKeyboard.label === "USERNAME") {
                    acctUserField.text = virtualKeyboard.text
                } else if (virtualKeyboard.label === "PASSWORD") {
                    acctPassField.text = virtualKeyboard.text
                }
            }
            root.forceActiveFocus()
        }

        onCancelled: {
            root.forceActiveFocus()
        }
    }
}
