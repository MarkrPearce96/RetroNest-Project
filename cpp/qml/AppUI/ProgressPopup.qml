import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: progressPopup

    property string title: ""
    property string subtitle: ""
    property real progressValue: -1  // 0.0-1.0 for determinate, -1 for indeterminate
    property string progressText: ""
    property color accentColor: Theme.accent
    property string logoSource: ""
    property bool showCloseButton: false

    anchors.centerIn: parent
    width: 360
    height: contentColumn.height + 56
    modal: true
    closePolicy: Popup.NoAutoClose
    padding: 28

    background: Rectangle {
        radius: 12
        color: Theme.surface
        border.width: 1
        border.color: Theme.divider
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        Image {
            source: progressPopup.logoSource
            visible: progressPopup.logoSource !== ""
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 4
            fillMode: Image.PreserveAspectFit
        }

        Text {
            text: progressPopup.title
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.Bold
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            text: progressPopup.subtitle
            color: Theme.textDim
            font.pixelSize: 13
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            Layout.bottomMargin: 12
        }

        // Progress bar container
        Rectangle {
            Layout.fillWidth: true
            height: 8
            radius: 4
            color: Theme.background
            clip: true

            // Determinate fill
            Rectangle {
                visible: progressPopup.progressValue >= 0
                width: parent.width * Math.max(0, Math.min(1, progressPopup.progressValue))
                height: parent.height
                radius: 4
                color: progressPopup.accentColor
                Behavior on width { NumberAnimation { duration: 150 } }
            }

            // Indeterminate sliding bar
            Rectangle {
                id: indeterminateBar
                visible: progressPopup.progressValue < 0
                width: parent.width * 0.4
                height: parent.height
                radius: 4
                color: progressPopup.accentColor

                SequentialAnimation on x {
                    loops: Animation.Infinite
                    running: indeterminateBar.visible
                    NumberAnimation {
                        from: 0
                        to: indeterminateBar.parent.width * 0.6
                        duration: 1200
                        easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        from: indeterminateBar.parent.width * 0.6
                        to: 0
                        duration: 1200
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }

        Text {
            text: progressPopup.progressText
            color: Theme.textDim
            font.pixelSize: 11
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            visible: progressPopup.progressText !== ""
        }

        Rectangle {
            visible: progressPopup.showCloseButton
            Layout.fillWidth: true
            Layout.topMargin: 12
            height: 36
            radius: 6
            color: closeArea.containsMouse ? Qt.lighter(Theme.surface, 1.2) : Theme.surface
            border.width: 1
            border.color: Theme.divider

            Text {
                anchors.centerIn: parent
                text: "Close"
                color: Theme.textPrimary
                font.pixelSize: 13
            }

            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    progressPopup.showCloseButton = false
                    progressPopup.close()
                }
            }
        }
    }
}
