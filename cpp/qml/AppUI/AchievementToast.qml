import QtQuick

/**
 * AchievementToast — richer top-right toast shown when an in-process
 * libretro core unlocks an achievement. Fired by the
 * AppController::raAchievementUnlocked signal handled in AppWindow.qml.
 *
 * Two text rows: a small "ACHIEVEMENT UNLOCKED" header, the achievement
 * title (large, bold), and a description line that wraps if long. Larger
 * footprint than ActionToast (used for Save State / Load State / FF) so
 * the small action pills stay compact.
 *
 * Same slide-down + fade-in animation as ActionToast for visual
 * consistency.
 */
Item {
    id: root

    width: card.width
    height: card.height
    visible: visibleState || card.opacity > 0.0

    // Public API — call show(title, description, imageUrl) for the
    // achievement-unlock case (default header, default duration), or
    // showWithHeader(...) to drive the toast for game-start banners,
    // game-mastered celebrations, hardcore reset notices, and server
    // error notices.
    property string header: "ACHIEVEMENT UNLOCKED"
    property string title: ""
    property string description: ""
    property string imageUrl: ""
    property int duration: 6000
    property bool visibleState: false

    function show(t, d, url) {
        showWithHeader("ACHIEVEMENT UNLOCKED", t, d, url, 6000);
    }

    function showWithHeader(h, t, d, url, durationMs) {
        header = h || "ACHIEVEMENT UNLOCKED";
        title = t || "";
        description = d || "";
        imageUrl = url || "";
        duration = (durationMs && durationMs > 0) ? durationMs : 6000;
        hideTimer.stop();
        visibleState = true;
        hideTimer.restart();
    }

    Timer {
        id: hideTimer
        interval: root.duration
        repeat: false
        onTriggered: root.visibleState = false
    }

    Rectangle {
        id: card
        // Width = trophy + padding + text column with a sane max so
        // long descriptions wrap rather than push the toast off-screen.
        width: 380
        // Height grows with content (title is fixed, description wraps).
        height: contentRow.implicitHeight + 24
        radius: 14
        color: Qt.rgba(0.08, 0.08, 0.10, 0.92)
        border.color: Qt.rgba(1, 0.84, 0.30, 0.45)   // soft gold border
        border.width: 1
        opacity: root.visibleState ? 1.0 : 0.0
        transform: Translate {
            y: root.visibleState ? 0 : -10
            Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        Row {
            id: contentRow
            anchors {
                left: parent.left; right: parent.right
                top: parent.top; topMargin: 12
                leftMargin: 12; rightMargin: 12
            }
            spacing: 12

            // Achievement badge from RetroAchievements when available;
            // falls back to the trophy glyph if URL is empty or the
            // remote fetch fails (offline, RA CDN hiccup, etc).
            Item {
                id: badgeBox
                anchors.verticalCenter: parent.verticalCenter
                width: 56
                height: 56

                Image {
                    id: badgeImage
                    anchors.fill: parent
                    source: root.imageUrl
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                    cache: true
                    visible: status === Image.Ready
                }
                Image {
                    id: trophyFallback
                    anchors.fill: parent
                    source: "images/hud/achievements.svg"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: !badgeImage.visible
                }
            }

            Column {
                width: parent.width - 56 - parent.spacing
                spacing: 2

                Text {
                    text: root.header
                    color: Qt.rgba(1, 0.84, 0.30, 1)   // gold
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: 1.2
                }
                Text {
                    width: parent.width
                    text: root.title
                    color: "#ffffff"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
                Text {
                    width: parent.width
                    text: root.description
                    color: Qt.rgba(1, 1, 1, 0.78)
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    visible: root.description.length > 0
                }
            }
        }
    }
}
