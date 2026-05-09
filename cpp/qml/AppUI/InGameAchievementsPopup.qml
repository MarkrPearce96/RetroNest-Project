import QtQuick
import QtQuick.Controls

/**
 * InGameAchievementsPopup — slide-up card showing the achievement
 * list for the currently running game, anchored above the in-game
 * HUD pill. Stays inside the game/menu context (no navigation away
 * to the main app's settings overlay).
 *
 * Hosted inside InGameMenu.qml; opens when the user activates the
 * HUD's Achievements icon.
 */
Item {
    id: root

    /** RetroAchievements game id to fetch achievements for. */
    property int raGameId: 0

    /** Set true to open the popup, false to close. Drives the
     *  slide+fade animation. */
    property bool open: false

    /** Cached achievement list returned by raRequestGameDetail. */
    property var achievements: []
    property int totalEarned: 0

    /** Hide entirely when fully closed so it doesn't intercept input. */
    visible: opacity > 0.01

    opacity: open ? 1.0 : 0
    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

    // Slight upward slide for the card during the fade.
    transform: Translate { id: slide; y: open ? 0 : 16 }
    Behavior on transform { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

    function refresh() {
        achievements = [];
        totalEarned = 0;
        if (raGameId > 0) app.raRequestGameDetail(raGameId);
    }

    /** D-pad / arrow-key navigation hooks called by InGameMenu while
     *  the popup is open. Keeps the focused row visible. */
    function scrollUp() {
        if (list.count <= 0) return;
        list.currentIndex = Math.max(0, list.currentIndex - 1);
        list.positionViewAtIndex(list.currentIndex, ListView.Contain);
    }
    function scrollDown() {
        if (list.count <= 0) return;
        list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1);
        list.positionViewAtIndex(list.currentIndex, ListView.Contain);
    }

    onRaGameIdChanged: refresh()
    onOpenChanged: { if (open) refresh(); }

    Connections {
        target: app
        function onRaGameDetailReady(rid, detail) {
            if (rid !== root.raGameId) return;
            if (!detail || !detail.achievements) return;
            root.achievements = detail.achievements;
            var n = 0;
            for (var i = 0; i < root.achievements.length; ++i)
                if (root.achievements[i].earned) ++n;
            root.totalEarned = n;
        }
    }

    // ── Card ──
    Rectangle {
        id: card
        width: 480
        height: Math.min(360, headerCol.height + list.contentHeight + 24)
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter

        radius: 14
        color: Qt.rgba(0.08, 0.08, 0.10, 0.94)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        Column {
            id: headerCol
            anchors {
                left: parent.left; right: parent.right; top: parent.top
                margins: 14
            }
            spacing: 2

            Text {
                text: "Achievements"
                color: "#ffffff"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            Text {
                visible: root.achievements.length > 0
                text: root.totalEarned + " / " + root.achievements.length + " earned"
                color: Qt.rgba(1, 1, 1, 0.5)
                font.pixelSize: 11
            }
        }

        ListView {
            id: list
            anchors {
                left: parent.left; right: parent.right; bottom: parent.bottom
                top: headerCol.bottom
                leftMargin: 14; rightMargin: 14; bottomMargin: 14; topMargin: 8
            }
            clip: true
            spacing: 4
            boundsBehavior: Flickable.StopAtBounds
            model: root.achievements
            currentIndex: 0
            highlightMoveDuration: 120
            highlightFollowsCurrentItem: true

            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 52
                radius: 8
                readonly property bool current: ListView.isCurrentItem
                color: current
                       ? Qt.rgba(1, 1, 1, 0.14)
                       : Qt.rgba(1, 1, 1, modelData.earned ? 0.06 : 0.03)
                opacity: modelData.earned ? 1.0 : 0.55
                border.color: current ? Qt.rgba(1, 1, 1, 0.30) : "transparent"
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 36; height: 36
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

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 36 - 60 - 20
                        spacing: 1
                        Text {
                            text: modelData.title || ""
                            color: "#ffffff"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            width: parent.width
                        }
                        Text {
                            text: modelData.description || ""
                            color: Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: (modelData.points || 0) + " pts"
                        color: "#f59e0b"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        // Loading / empty state
        Text {
            anchors.centerIn: list
            visible: root.achievements.length === 0
            text: "Loading achievements…"
            color: Qt.rgba(1, 1, 1, 0.4)
            font.pixelSize: 13
        }
    }
}
