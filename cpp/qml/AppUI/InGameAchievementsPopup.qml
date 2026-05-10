import QtQuick
import QtQuick.Controls

/**
 * InGameAchievementsPopup — slide-up card showing the achievement
 * list for the currently running game, anchored above the in-game
 * HUD pill. Stays inside the game/menu context (no navigation away
 * to the main app's settings overlay).
 *
 * Hosted inside InGameMenu.qml; opens when the user activates the
 * HUD's Achievements icon. Tab bar at the top filters between All /
 * Earned / Unearned / Missable so users can jump straight to the
 * subset they care about (RA's website has the same shape).
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

    /** Tab selection index. 0=All, 1=Earned, 2=Unearned, 3=Missable.
     *  Used by the filter that drives the visible list. Reset to 0
     *  whenever a fresh load completes. */
    property int currentTab: 0
    readonly property var tabLabels: ["All", "Earned", "Unearned", "Missable"]

    /** Per-tab counts shown next to each tab label. Computed by
     *  applying the same filter the visible list uses. */
    readonly property int countAll: achievements.length
    readonly property int countEarned: {
        var n = 0;
        for (var i = 0; i < achievements.length; ++i)
            if (achievements[i].earned) ++n;
        return n;
    }
    readonly property int countUnearned: countAll - countEarned
    readonly property int countMissable: {
        var n = 0;
        for (var i = 0; i < achievements.length; ++i)
            if (achievements[i].missable) ++n;
        return n;
    }

    /** Achievements filtered by the current tab — the model the
     *  ListView actually renders. */
    readonly property var visibleAchievements: {
        if (currentTab === 0) return achievements;
        var out = [];
        for (var i = 0; i < achievements.length; ++i) {
            var a = achievements[i];
            if (currentTab === 1 && a.earned) out.push(a);
            else if (currentTab === 2 && !a.earned) out.push(a);
            else if (currentTab === 3 && a.missable) out.push(a);
        }
        return out;
    }

    /** "loading" until the data path resolves, then "loaded" (with
     *  achievements possibly empty) or "timeout" after ~5 s of no
     *  response. */
    property string loadState: "loading"

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
        loadState = "loading";
        currentTab = 0;
        list.currentIndex = 0;

        // Libretro path: rc_client already has the achievement list in
        // memory after the game-session load completed. Render directly,
        // skipping the redundant RA web API call.
        if (app.libretroAchievementsReady()) {
            var local = app.libretroAchievementList();
            if (local && local.length > 0) {
                achievements = local;
                var n = 0;
                for (var i = 0; i < achievements.length; ++i)
                    if (achievements[i].earned) ++n;
                totalEarned = n;
                loadState = "loaded";
                return;
            }
        }

        // External-emulator path: fall back to the RA web API.
        if (raGameId > 0) {
            app.raRequestGameDetail(raGameId);
            loadTimeout.restart();
        }
    }

    Timer {
        id: loadTimeout
        interval: 5000
        repeat: false
        onTriggered: {
            if (root.loadState === "loading") root.loadState = "timeout";
        }
    }

    /** D-pad / arrow-key navigation hooks called by InGameMenu while
     *  the popup is open. Up/Down move the list cursor; Left/Right
     *  switch the active tab. */
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
    function nextTab() {
        currentTab = (currentTab + 1) % tabLabels.length;
        list.currentIndex = 0;
    }
    function prevTab() {
        currentTab = (currentTab - 1 + tabLabels.length) % tabLabels.length;
        list.currentIndex = 0;
    }

    onRaGameIdChanged: refresh()
    onOpenChanged: { if (open) refresh(); }

    Connections {
        target: app
        function onRaGameDetailReady(rid, detail) {
            if (rid !== root.raGameId) return;
            loadTimeout.stop();
            root.loadState = "loaded";
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
        // Wider + taller than the original to fit the tab bar plus
        // ~6 visible rows of content without forcing scroll on every
        // open.
        width: 560
        height: 460
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
            spacing: 6

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

            // Tab bar — clickable buttons + arrow-key driven via
            // nextTab() / prevTab() which InGameMenu wires to L/R.
            Row {
                spacing: 4
                Repeater {
                    model: root.tabLabels
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        readonly property bool active: root.currentTab === index
                        readonly property int tabCount: {
                            switch (index) {
                            case 0: return root.countAll;
                            case 1: return root.countEarned;
                            case 2: return root.countUnearned;
                            case 3: return root.countMissable;
                            }
                            return 0;
                        }
                        width: tabText.implicitWidth + 22
                        height: 28
                        radius: 6
                        color: active
                               ? Qt.rgba(1, 0.84, 0.30, 0.18)
                               : Qt.rgba(1, 1, 1, 0.04)
                        border.color: active
                                      ? Qt.rgba(1, 0.84, 0.30, 0.55)
                                      : Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                        Text {
                            id: tabText
                            anchors.centerIn: parent
                            text: modelData + " (" + tabCount + ")"
                            color: active ? "#ffffff" : Qt.rgba(1, 1, 1, 0.6)
                            font.pixelSize: 11
                            font.weight: active ? Font.DemiBold : Font.Normal
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.currentTab = index;
                                list.currentIndex = 0;
                            }
                        }
                    }
                }
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
            model: root.visibleAchievements
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
                        width: parent.width - 36 - 60 - 20 - missableTag.implicitWidth - 8
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

                    // "Missable" tag — visible only on missable
                    // achievements. Helps spot them in the All /
                    // Earned / Unearned views without switching tab.
                    Rectangle {
                        id: missableTag
                        anchors.verticalCenter: parent.verticalCenter
                        visible: modelData.missable === true
                        width: visible ? (missableLabel.implicitWidth + 12) : 0
                        height: 20
                        radius: 4
                        color: Qt.rgba(0.95, 0.55, 0.10, 0.18)
                        border.color: Qt.rgba(0.95, 0.55, 0.10, 0.5)
                        border.width: 1
                        Text {
                            id: missableLabel
                            anchors.centerIn: parent
                            text: "MISSABLE"
                            color: Qt.rgba(0.95, 0.65, 0.20, 1)
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            font.letterSpacing: 0.5
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

        // Loading / empty / timeout state
        Text {
            anchors.centerIn: list
            visible: list.count === 0
            text: root.loadState === "loading" ? "Loading achievements…"
                  : root.loadState === "timeout" ? "Couldn't load achievements"
                  : root.achievements.length === 0
                      ? "No achievements found for this game"
                      : "No achievements in this category"
            color: Qt.rgba(1, 1, 1, 0.4)
            font.pixelSize: 13
        }
    }
}
