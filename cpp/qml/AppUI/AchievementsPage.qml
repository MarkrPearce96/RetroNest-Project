import QtQuick
import QtQuick.Layouts

Item {
    id: root
    focus: true

    // Explicit navigation contract — the host (SettingsOverlay) connects
    // this and owns the pop; the page never touches the host StackView.
    signal backRequested()

    property int raGameId: 0
    property string gameTitle: ""

    property var achievements: []
    property int totalEarned: 0
    property int totalPoints: 0
    property int earnedPoints: 0
    property int completionPercent: 0
    property bool loading: true

    Component.onCompleted: {
        loading = true
        app.raRequestGameDetail(raGameId)
    }

    Connections {
        target: app
        function onRaGameDetailReady(returnedRaGameId, detail) {
            if (returnedRaGameId !== raGameId) return
            if (detail && detail.achievements) {
                achievements = detail.achievements
                if (detail.title) gameTitle = detail.title
            }
            computeTotals()
            loading = false
        }
    }

    function computeTotals() {
        var earned = 0
        var pts = 0
        var earnedPts = 0
        for (var i = 0; i < achievements.length; i++) {
            var a = achievements[i]
            pts += (a.points || 0)
            if (a.earned) {
                earned++
                earnedPts += (a.points || 0)
            }
        }
        totalEarned = earned
        totalPoints = pts
        earnedPoints = earnedPts
        completionPercent = achievements.length > 0 ? Math.round(earned * 100 / achievements.length) : 0
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            event.accepted = true
            root.backRequested()
        }
    }

    // Loading state
    Text {
        anchors.centerIn: parent
        visible: loading
        text: "Loading achievements..."
        color: Qt.rgba(1, 1, 1, 0.4)
        font.pixelSize: 16
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16
        visible: !loading

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: headerCol.height + 24
            radius: 12
            color: Qt.rgba(0.09, 0.09, 0.12, 1)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.08)

            Column {
                id: headerCol
                anchors.top: parent.top
                anchors.topMargin: 12
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                spacing: 8

                Text {
                    text: gameTitle
                    color: "#ffffff"
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    width: parent.width
                }

                RowLayout {
                    width: parent.width
                    spacing: 16

                    Text {
                        text: totalEarned + " / " + achievements.length + " achievements"
                        color: Qt.rgba(1, 1, 1, 0.4)
                        font.pixelSize: 13
                    }

                    Text {
                        text: earnedPoints + " / " + totalPoints + " points"
                        color: "#f59e0b"
                        font.pixelSize: 13
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: completionPercent + "% complete"
                        color: completionPercent === 100 ? "#22c55e" : "#6366f1"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }

                // Progress bar
                Rectangle {
                    width: parent.width
                    height: 6
                    radius: 3
                    color: Qt.rgba(1, 1, 1, 0.08)

                    Rectangle {
                        width: parent.width * Math.min(1, completionPercent / 100)
                        height: parent.height
                        radius: 3
                        color: completionPercent === 100 ? "#22c55e" : "#6366f1"

                        Behavior on width { NumberAnimation { duration: 300 } }
                    }
                }
            }
        }

        // Achievements list
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: achievements
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                required property var modelData
                required property int index

                width: ListView.view.width
                height: 64
                radius: 6
                color: Qt.rgba(0.09, 0.09, 0.12, 1)
                opacity: modelData.earned ? 1.0 : 0.5

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 12

                    // Badge
                    Rectangle {
                        width: 40
                        height: 40
                        radius: 6
                        color: Qt.rgba(1, 1, 1, 0.05)

                        Image {
                            anchors.fill: parent
                            anchors.margins: 2
                            source: modelData.badgeUrl || ""
                            fillMode: Image.PreserveAspectFit
                            visible: status === Image.Ready
                        }
                    }

                    // Title + description
                    Column {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: modelData.title || ""
                            color: "#ffffff"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            width: parent.width
                        }

                        Text {
                            text: modelData.description || ""
                            color: Qt.rgba(1, 1, 1, 0.4)
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    // Rarity
                    Text {
                        visible: (modelData.trueRatio || 0) > 0
                        text: Math.round((modelData.points || 0) * 100 / (modelData.trueRatio || 1)) + "% of players"
                        color: Qt.rgba(1, 1, 1, 0.25)
                        font.pixelSize: 11
                    }

                    // Points
                    Text {
                        text: (modelData.points || 0) + " pts"
                        color: "#f59e0b"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    // Earned date
                    Text {
                        visible: modelData.earned && (modelData.earnedDate || "") !== ""
                        text: modelData.earnedDate || ""
                        color: Qt.rgba(1, 1, 1, 0.25)
                        font.pixelSize: 11
                    }
                }
            }

            // Empty state
            Text {
                anchors.centerIn: parent
                visible: achievements.length === 0
                text: "No achievements found for this game"
                color: Qt.rgba(1, 1, 1, 0.25)
                font.pixelSize: 14
            }
        }
    }
}
