import QtQuick
import QtQuick.Layouts

Rectangle {
    height: 48
    color: Theme.navBackground

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.divider
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        spacing: 0

        Repeater {
            model: ["Games", "Settings"]

            delegate: Rectangle {
                Layout.preferredWidth: tabText.implicitWidth + 48
                Layout.fillHeight: true
                color: "transparent"

                property bool active: app.currentTab === index

                Text {
                    id: tabText
                    anchors.centerIn: parent
                    text: modelData
                    color: active ? Theme.textPrimary : Theme.textDim
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 2
                    color: active ? Theme.accent : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: app.currentTab = index
                    onEntered: if (!active) tabText.color = Theme.textSecondary
                    onExited: tabText.color = active ? Theme.textPrimary : Theme.textDim
                }
            }
        }

        Item { Layout.fillWidth: true }
    }
}
