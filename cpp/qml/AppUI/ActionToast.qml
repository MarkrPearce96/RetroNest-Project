import QtQuick

/**
 * ActionToast — small pill anchored top-right that appears when an
 * in-game action fires (Save State, Load State, Fast Forward). Two
 * modes:
 *
 *   Transient (sticky == false): show() displays the pill, then it
 *     auto-hides after `duration` ms.
 *   Sticky (sticky == true): show() displays it indefinitely; hide()
 *     dismisses it. Used for the Fast Forward indicator while FF is
 *     on.
 *
 * Visual treatment matches the in-game HUD pill (same translucent
 * dark background, white text, 14 px corner radius). Slides down
 * from above the screen edge and fades in.
 */
Item {
    id: root
    // Caller decides positioning (anchors or layout container). The
    // item sizes itself to the pill's intrinsic dimensions.
    width: pill.width
    height: pill.height
    // Hidden items in a Column / Row should still consume zero space.
    visible: visibleState || pill.opacity > 0.0

    property string iconSource: ""
    property string label: ""
    property int duration: 1500
    property bool sticky: false
    property bool visibleState: false

    function show() {
        hideTimer.stop();
        visibleState = true;
        if (!sticky) hideTimer.restart();
    }

    function hide() {
        hideTimer.stop();
        visibleState = false;
    }

    Timer {
        id: hideTimer
        interval: root.duration
        repeat: false
        onTriggered: root.visibleState = false
    }

    Rectangle {
        id: pill
        width: row.implicitWidth + 24
        height: row.implicitHeight + 16
        radius: 14
        color: Qt.rgba(0.08, 0.08, 0.10, 0.88)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1
        opacity: root.visibleState ? 1.0 : 0.0
        // Slide down from -10 px when hidden so the appearance has
        // direction; the easing settles it to its anchored position.
        transform: Translate {
            y: root.visibleState ? 0 : -10
            Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                source: root.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: root.iconSource !== ""
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.label
                color: "#ffffff"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                visible: root.label !== ""
            }
        }
    }
}
