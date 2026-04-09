import QtQuick

Rectangle {
    id: statusBar
    height: 32
    color: "#CC222222"
    radius: 6
    opacity: 0.0
    visible: opacity > 0

    anchors.leftMargin: 16
    anchors.bottomMargin: 16

    Behavior on opacity { NumberAnimation { duration: 300 } }

    Text {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        text: app.statusMessage
        color: "#dddddd"
        font.pixelSize: 12
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    // Show briefly when status message changes, then fade out
    Connections {
        target: app
        function onStatusMessageChanged() {
            if (app.statusMessage !== "") {
                statusBar.opacity = 1.0
                hideTimer.restart()
            } else {
                statusBar.opacity = 0.0
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 4000
        onTriggered: statusBar.opacity = 0.0
    }
}
