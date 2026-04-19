import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: notification
    width: notifRow.width + 32
    height: 48
    radius: 10
    color: Theme.surface
    border.width: 1
    border.color: Theme.accent
    opacity: 0.0
    visible: opacity > 0
    z: 200

    property string emuId: ""
    property string emuName: ""
    property string currentVersion: ""
    property string latestVersion: ""
    property var updateQueue: []

    signal updateRequested(string emuId, string emuName, string latestVersion)

    function showUpdate(emuId, currentVersion, latestVersion) {
        var emulators = app.allEmulatorStatus()
        var name = emuId
        for (var i = 0; i < emulators.length; i++) {
            if (emulators[i].id === emuId) {
                name = emulators[i].name
                break
            }
        }

        if (notification.opacity > 0) {
            updateQueue.push({ emuId: emuId, name: name,
                              current: currentVersion, latest: latestVersion })
            return
        }

        notification.emuId = emuId
        notification.emuName = name
        notification.currentVersion = currentVersion
        notification.latestVersion = latestVersion
        showAnim.start()
        autoDismissTimer.restart()
    }

    function dismiss() {
        hideAnim.start()
    }

    function showNext() {
        if (updateQueue.length > 0) {
            var next = updateQueue.shift()
            notification.emuId = next.emuId
            notification.emuName = next.name
            notification.currentVersion = next.current
            notification.latestVersion = next.latest
            showAnim.start()
            autoDismissTimer.restart()
        }
    }

    NumberAnimation on opacity {
        id: showAnim
        from: 0.0; to: 1.0; duration: 300
        running: false
    }

    NumberAnimation on opacity {
        id: hideAnim
        from: 1.0; to: 0.0; duration: 300
        running: false
        onFinished: notification.showNext()
    }

    Timer {
        id: autoDismissTimer
        interval: 10000
        onTriggered: notification.dismiss()
    }

    RowLayout {
        id: notifRow
        anchors.centerIn: parent
        spacing: 12

        Text {
            text: notification.emuName + " " + notification.latestVersion +
                  " available (you have " + notification.currentVersion + ")"
            color: Theme.textPrimary
            font.pixelSize: 13
        }

        Rectangle {
            width: 60
            height: 28
            radius: 6
            color: viewMa.containsMouse ? Theme.accentLight : Theme.accent

            Text {
                anchors.centerIn: parent
                text: "Update"
                color: Theme.textPrimary
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: viewMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    notification.updateRequested(notification.emuId,
                                                 notification.emuName,
                                                 notification.latestVersion)
                    notification.dismiss()
                }
            }
        }

        Rectangle {
            width: 24
            height: 24
            radius: 4
            color: closeMa.containsMouse ? Theme.surfaceHover : "transparent"

            Text {
                anchors.centerIn: parent
                text: "\u2715"
                color: Theme.textDim
                font.pixelSize: 12
            }

            MouseArea {
                id: closeMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: notification.dismiss()
            }
        }
    }
}
