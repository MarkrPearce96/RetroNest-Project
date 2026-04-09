import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string emuId: ""
    property var biosList: app.biosStatus(emuId)

    visible: biosList.length > 0
    spacing: 4

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Theme.divider
        Layout.topMargin: 4
    }

    RowLayout {
        Layout.fillWidth: true

        Text {
            text: "BIOS Files"
            color: Theme.textMuted
            font.pixelSize: 11
            font.weight: Font.Bold
        }

        Item { Layout.fillWidth: true }

        Text {
            text: "Open Folder"
            color: Theme.accent
            font.pixelSize: 11
            font.weight: Font.Medium

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: app.openBiosFolder()
            }
        }

        Text {
            text: "Refresh"
            color: Theme.accent
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 12

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.biosList = []
                    root.biosList = app.biosStatus(root.emuId)
                }
            }
        }
    }

    Repeater {
        model: root.biosList

        delegate: RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: modelData.found ? "\u2705" : (modelData.required ? "\u274C" : "\u26A0\uFE0F")
                font.pixelSize: 12
            }

            Text {
                text: modelData.filename
                color: Theme.textPrimary
                font.pixelSize: 12
                font.weight: Font.Medium
            }

            Text {
                text: modelData.description
                color: Theme.textMuted
                font.pixelSize: 12
                Layout.fillWidth: true
            }

            Text {
                text: modelData.found ? "Found" : (modelData.required ? "Missing" : "Optional")
                color: modelData.found ? Theme.success
                     : modelData.required ? Theme.error
                     : "#aa8844"
                font.pixelSize: 11
                font.weight: Font.Medium
            }
        }
    }
}
