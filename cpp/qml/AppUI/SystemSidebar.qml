import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "#16162a"

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.divider
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Text {
            text: "SYSTEMS"
            color: Theme.textDim
            font.pixelSize: 11
            font.weight: Font.Bold
            font.letterSpacing: 1
            Layout.margins: 16
            Layout.bottomMargin: 6
        }

        ListView {
            id: systemList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            clip: true
            spacing: 2

            model: {
                var items = ["All Games"]
                var systems = app.systems
                for (var i = 0; i < systems.length; i++)
                    items.push(systems[i])
                return items
            }

            delegate: Rectangle {
                width: systemList.width
                height: 36
                radius: 8
                color: {
                    var isSelected = (index === 0 && app.currentSystem === "")
                        || (index > 0 && modelData === app.currentSystem)
                    return isSelected ? Theme.surfaceHover : "transparent"
                }

                property bool isSelected: (index === 0 && app.currentSystem === "")
                    || (index > 0 && modelData === app.currentSystem)

                Behavior on color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    text: modelData
                    color: isSelected ? Theme.textPrimary : Theme.textMuted
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        app.currentSystem = index === 0 ? "" : modelData
                    }
                    onEntered: if (!isSelected) parent.color = Theme.surface
                    onExited: parent.color = isSelected ? Theme.surfaceHover : "transparent"
                }
            }
        }
    }
}
