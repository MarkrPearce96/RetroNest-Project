import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root
    focus: true

    Component.onCompleted: {
        root.forceActiveFocus()
        if (gameModel.count > 0) {
            root.listIndex = 0
            root.selectCurrentGame()
        }
    }
    StackView.onActivated: root.forceActiveFocus()

    // ── State ──
    property int listIndex: 0
    property var selectedDetails: ({})
    property bool hasDetails: (selectedDetails.description || "").length > 0
                           || (selectedDetails.developer   || "").length > 0
                           || (selectedDetails.rating      || 0) > 0

    property var hints: [
        {action: "navigate_ud", label: "Browse"},
        {action: "confirm",     label: "Launch"},
        {action: "action",      label: "Actions"},
        {action: "back",        label: "Back", keyboardKey: "Backspace"},
        {action: "start",       label: "Settings"}
    ]

    Component.onDestruction: themeContext.currentFocusedGameId = -1

    // Video delay timer — starts video after 5 seconds on same game
    property bool showVideo: false
    Timer {
        id: videoDelayTimer
        interval: 5000
        repeat: false
        onTriggered: {
            var vp = root.selectedDetails.videoPath || ""
            if (vp.length > 0)
                root.showVideo = true
        }
    }

    // Stop the preview video (video + audio) while ANY game is running.
    // Covers both libretro (in-window) and process-backed (external window)
    // launches via themeContext.gameRunning. Re-arm the delay timer when the
    // game ends so the preview picks back up on the same selection.
    Connections {
        target: themeContext
        function onGameRunningChanged() {
            if (themeContext.gameRunning) {
                root.showVideo = false
                videoPlayer.stop()
                videoPlayer.source = ""
                videoDelayTimer.stop()
            } else if (root.selectedDetails && root.selectedDetails.id !== undefined) {
                videoDelayTimer.restart()
            }
        }
    }

    // ── Helpers ──
    function selectCurrentGame() {
        root.showVideo = false
        videoPlayer.stop()
        videoPlayer.source = ""
        videoDelayTimer.stop()
        var d = themeContext.gameDetailsByIndex(root.listIndex)
        if (d && d.id !== undefined) {
            root.selectedDetails = d
            gameList.positionViewAtIndex(root.listIndex, ListView.Visible)
            videoDelayTimer.restart()
            themeContext.currentFocusedGameId = d.id
        } else {
            themeContext.currentFocusedGameId = -1
        }
    }

    function fanartSource() {
        var fa = root.selectedDetails.fanartPath || ""
        if (fa.length > 0) return "file://" + fa
        return "assets/artwork/" + themeContext.currentSystem + ".webp"
    }

    function mediaPath(field) {
        var p = root.selectedDetails[field] || ""
        if (p.length > 0) return "file://" + p
        return ""
    }

    // ── Keyboard navigation ──
    Keys.onUpPressed:    { if (root.listIndex > 0) { root.listIndex--; root.selectCurrentGame() } }
    Keys.onDownPressed:  { if (root.listIndex < gameModel.count - 1) { root.listIndex++; root.selectCurrentGame() } }
    Keys.onReturnPressed: {
        if (root.selectedDetails && root.selectedDetails.id !== undefined)
            themeContext.launchGame(root.selectedDetails.id, root.selectedDetails.romPath, root.selectedDetails.emulatorId)
    }
    Keys.onEnterPressed: {
        if (root.selectedDetails && root.selectedDetails.id !== undefined)
            themeContext.launchGame(root.selectedDetails.id, root.selectedDetails.romPath, root.selectedDetails.emulatorId)
    }

    // ── Refresh on data change ──
    Connections {
        target: themeContext
        function onGamesChanged() {
            if (gameModel.count > 0 && root.listIndex >= gameModel.count)
                root.listIndex = gameModel.count - 1
            if (root.selectedDetails && root.selectedDetails.id !== undefined) {
                // Track the selected game's new position after re-sort (e.g. favorite toggle)
                var newIdx = gameModel.indexForGameId(root.selectedDetails.id)
                if (newIdx >= 0)
                    root.listIndex = newIdx
                root.selectedDetails = themeContext.gameDetails(root.selectedDetails.id)
            }
        }
    }

    // ════════════════════════════════════════════════════════════════
    // BACKGROUND — fanart of selected game (or system artwork fallback)
    // ════════════════════════════════════════════════════════════════
    Image {
        id: bgArt
        anchors.fill: parent
        source: root.fanartSource()
        fillMode: Image.PreserveAspectCrop

        onStatusChanged: {
            if (status === Image.Error)
                source = "assets/artwork/" + themeContext.currentSystem + ".webp"
        }

        Behavior on source {
            PropertyAnimation { target: bgArt; property: "opacity"; from: 0.6; to: 1.0; duration: 400 }
        }
    }

    // Darken overlay
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.50)
    }

    // ════════════════════════════════════════════════════════════════
    // LEFT DETAIL PANEL — ~47% width
    // ════════════════════════════════════════════════════════════════
    Item {
        id: detailPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: parent.width * 0.47
        anchors.topMargin: 20
        anchors.leftMargin: 24
        anchors.bottomMargin: 20

        // ── Media Showcase Area ──
        Item {
            id: mediaArea
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height * 0.40

            // Screenshot image (contained, centered in area)
            Image {
                id: screenshotImg
                anchors.centerIn: parent
                width: parent.width * 0.85
                height: parent.height * 0.90
                source: root.mediaPath("screenshotPath")
                fillMode: Image.PreserveAspectFit
                visible: !root.showVideo && source.toString().length > 0
            }

            // Video player (replaces screenshot after 5s)
            VideoOutput {
                id: videoOutput
                anchors.centerIn: parent
                width: parent.width * 0.85
                height: parent.height * 0.96
                fillMode: VideoOutput.PreserveAspectFit
                visible: root.showVideo && videoPlayer.hasVideo
            }

            MediaPlayer {
                id: videoPlayer
                videoOutput: videoOutput
                audioOutput: AudioOutput { volume: 0.5 }
                loops: MediaPlayer.Infinite

                onErrorOccurred: function(error, errorString) {
                    console.warn("[GameListPage] Video error:", error, errorString)
                }
                onPlaybackStateChanged: {
                    console.log("[GameListPage] Video state:", playbackState, "source:", source)
                }
            }

            Connections {
                target: root
                function onShowVideoChanged() {
                    if (root.showVideo) {
                        var vp = root.selectedDetails.videoPath || ""
                        if (vp.length > 0) {
                            var url = "file://" + vp
                            console.log("[GameListPage] Starting video:", url)
                            videoPlayer.source = url
                            videoPlayer.play()
                        }
                    } else {
                        videoPlayer.stop()
                        videoPlayer.source = ""
                    }
                }
            }

            // No-media placeholder
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.85
                height: parent.height * 0.90
                color: Qt.rgba(0, 0, 0, 0.3)
                radius: 4
                visible: !screenshotImg.visible && !videoOutput.visible
                Text {
                    anchors.centerIn: parent
                    text: "No Media"
                    color: Qt.rgba(1, 1, 1, 0.3)
                    font.pixelSize: 16
                }
            }

            // 3D Box art (overlaps bottom-left of media area, like ES-DE)
            Image {
                id: box3dArt
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -20
                width: parent.width * 0.30
                height: parent.height * 0.80
                source: root.mediaPath("box3dPath")
                fillMode: Image.PreserveAspectFit
                visible: source.toString().length > 0 && !root.showVideo
                z: 2
            }

            // Physical media disc (right next to the 3D box)
            Image {
                id: physicalMedia
                anchors.left: box3dArt.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: -10
                anchors.bottomMargin: -10
                width: parent.width * 0.14
                height: parent.width * 0.14
                source: root.mediaPath("physicalmediaPath")
                fillMode: Image.PreserveAspectFit
                visible: source.toString().length > 0 && !root.showVideo
                z: 1
            }
        }

        // ── Title Bar ──
        Rectangle {
            id: titleBar
            anchors.top: mediaArea.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 35
            height: 50
            radius: 8
            color: Qt.rgba(0, 0, 0, 0.65)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                text: "\uD83C\uDFAE  " + (root.selectedDetails.title || "")
                color: "#ffffff"
                font.pixelSize: 20
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        // ── Metadata Box — 3 separate rows with gaps ──
        Column {
            id: metadataColumn
            anchors.top: titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 6
            spacing: 6
            visible: root.hasDetails

            // Row 1: Rating | Platform
            Row {
                width: parent.width
                height: 35
                spacing: 6

                Rectangle {
                    width: (parent.width - 4) * 0.45
                    height: parent.height
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.65)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        text: {
                            var r = Math.round(root.selectedDetails.rating || 0)
                            return "\u2B50 " + "\u2605".repeat(r) + "\u2606".repeat(5 - r)
                        }
                        color: "#e0e0e0"; font.pixelSize: 18
                    }
                }
                Rectangle {
                    width: (parent.width - 4) * 0.55
                    height: parent.height
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.65)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        text: "\uD83D\uDCCD " + (themeContext.systemNames[themeContext.currentSystem] || themeContext.currentSystem)
                        color: "#e0e0e0"; font.pixelSize: 18
                        elide: Text.ElideRight
                    }
                }
            }

            // Row 2: Release Date | Developer | Last Played
            Row {
                width: parent.width
                height: 35
                spacing: 6

                Rectangle {
                    width: (parent.width - 8) * 0.30
                    height: parent.height
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.65)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 4
                        text: "\uD83D\uDCC5 " + (root.selectedDetails.releaseDate || "").substring(0, 7)
                        color: "#e0e0e0"; font.pixelSize: 18
                        elide: Text.ElideRight
                    }
                }
                Rectangle {
                    width: (parent.width - 8) * 0.38
                    height: parent.height
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.65)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 4
                        text: "\u2699 " + (root.selectedDetails.developer || "")
                        color: "#e0e0e0"; font.pixelSize: 18
                        elide: Text.ElideRight
                    }
                }
                Rectangle {
                    width: (parent.width - 8) * 0.32
                    height: parent.height
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.65)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 4
                        text: "\uD83D\uDC65 " + (root.selectedDetails.lastPlayed || "Never")
                        color: "#e0e0e0"; font.pixelSize: 18
                        elide: Text.ElideRight
                    }
                }
            }

            // Row 3: Players | Publisher | Play Count
            Row {
                width: parent.width
                height: 35
                spacing: 6

                Rectangle {
                    width: (parent.width - 8) * 0.30
                    height: parent.height
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.65)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        text: "\uD83D\uDC64 " + (root.selectedDetails.players || "1")
                        color: "#e0e0e0"; font.pixelSize: 18
                    }
                }
                Rectangle {
                    width: (parent.width - 8) * 0.38
                    height: parent.height
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.65)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 4
                        text: "\uD83C\uDFEC " + (root.selectedDetails.publisher || "")
                        color: "#e0e0e0"; font.pixelSize: 18
                        elide: Text.ElideRight
                    }
                }
                Rectangle {
                    width: (parent.width - 8) * 0.32
                    height: parent.height
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.65)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 4
                        text: "\u23F1 " + (root.selectedDetails.playCount > 0 ? root.selectedDetails.playCount + " plays" : "Unplayed")
                        color: "#e0e0e0"; font.pixelSize: 18
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // ── Description Box (auto-scrolling) ──
        Rectangle {
            id: descriptionBox
            anchors.top: metadataColumn.visible ? metadataColumn.bottom : titleBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 6
            height: Math.min(descText.implicitHeight + 20, parent.height * 0.15)
            radius: 6
            color: Qt.rgba(0, 0, 0, 0.65)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            visible: (root.selectedDetails.description || "").length > 0
            clip: true

            Flickable {
                id: descFlickable
                anchors.fill: parent
                anchors.margins: 10
                contentHeight: descText.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: descText
                    width: descFlickable.width
                    text: root.selectedDetails.description || ""
                    color: Qt.rgba(1, 1, 1, 0.70)
                    font.pixelSize: 18
                    lineHeight: 1.4
                    wrapMode: Text.WordWrap
                }

                NumberAnimation on contentY {
                    id: autoScroll
                    running: false
                    from: 0
                    to: 0
                    duration: 0
                }
            }

            Timer {
                id: scrollStartTimer
                interval: 3000
                repeat: false
                onTriggered: {
                    var overflow = descFlickable.contentHeight - descFlickable.height
                    if (overflow > 0) {
                        autoScroll.from = 0
                        autoScroll.to = overflow
                        autoScroll.duration = overflow * 40
                        autoScroll.running = true
                    }
                }
            }

            onVisibleChanged: {
                if (visible) { descFlickable.contentY = 0; autoScroll.running = false; scrollStartTimer.restart() }
            }

            Connections {
                target: root
                function onSelectedDetailsChanged() {
                    descFlickable.contentY = 0
                    autoScroll.running = false
                    scrollStartTimer.restart()
                }
            }
        }

        // ── System Logo (large, like ES-DE) ──
        Image {
            id: systemLogo
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: -50
            width: parent.width * 0.70
            height: 350
            source: "assets/gamepage-logos/" + themeContext.currentSystem + ".webp"
            fillMode: Image.PreserveAspectFit
            visible: status === Image.Ready
        }
    }

    // ════════════════════════════════════════════════════════════════
    // RIGHT GAME LIST — inside dark rounded box
    // ════════════════════════════════════════════════════════════════
    Rectangle {
        id: gameListBox
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 20
        anchors.rightMargin: 24
        anchors.bottomMargin: 20
        width: parent.width * 0.47
        radius: 12
        color: Qt.rgba(0, 0, 0, 0.60)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        ListView {
            id: gameList
            anchors.fill: parent
            anchors.margins: 8
            clip: true
            spacing: 5

            model: gameModel
            currentIndex: root.listIndex

            highlightFollowsCurrentItem: true
            highlightMoveDuration: 150

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                width: 6
            }

            delegate: Item {
                id: listItem
                width: gameList.width
                height: 50

                required property int    gameId
                required property string title
                required property string romPath
                required property string emulatorId
                required property int    index
                required property bool   favorite

                property bool isCurrent: root.listIndex === index

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    radius: 4
                    color: listItem.isCurrent ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
                    border.color: listItem.isCurrent ? Qt.rgba(1, 1, 1, 0.25) : "transparent"
                    border.width: listItem.isCurrent ? 1 : 0
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 14
                    anchors.rightMargin: 36
                    text: listItem.title
                    color: listItem.isCurrent ? "#ffffff" : Qt.rgba(1, 1, 1, 0.70)
                    font.pixelSize: 25
                    font.weight: listItem.isCurrent ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    text: "\u2605"
                    color: "#ffc107"
                    font.pixelSize: 18
                    visible: listItem.favorite
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        root.listIndex = listItem.index
                        root.selectCurrentGame()
                    }
                    onDoubleClicked: {
                        themeContext.launchGame(listItem.gameId, listItem.romPath, listItem.emulatorId)
                    }
                }
            }
        }
    }

}
