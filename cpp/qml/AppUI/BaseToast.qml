import QtQuick

Item {
    id: root
    default property alias contentItem: contentHolder.data

    property int duration: 1500
    property bool sticky: false
    property bool visibleState: false

    width: contentHolder.childrenRect.width
    height: contentHolder.childrenRect.height
    visible: visibleState || contentHolder.opacity > 0.0

    function show() {
        hideTimer.stop()
        visibleState = true
        if (!sticky) hideTimer.restart()
    }

    function hide() {
        hideTimer.stop()
        visibleState = false
    }

    function restartDismissTimer() {
        hideTimer.stop()
        if (!sticky) hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: root.duration
        repeat: false
        onTriggered: root.visibleState = false
    }

    Item {
        id: contentHolder
        width: childrenRect.width
        height: childrenRect.height
        opacity: root.visibleState ? 1.0 : 0.0
        transform: Translate {
            y: root.visibleState ? 0 : -10
            Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }
}
