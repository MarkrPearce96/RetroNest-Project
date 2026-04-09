import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Settings Sidebar
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
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
                    text: "SETTINGS"
                    color: Theme.textDim
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: 1
                    Layout.margins: 16
                    Layout.bottomMargin: 6
                }

                ListView {
                    id: settingsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    clip: true
                    spacing: 2

                    model: ["Emulator Manage", "Paths", "Scraper"]

                    delegate: Rectangle {
                        width: settingsList.width
                        height: 36
                        radius: 8
                        color: app.settingsCategory === index ? Theme.surfaceHover : "transparent"

                        Behavior on color { ColorAnimation { duration: 100 } }

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            text: modelData
                            color: app.settingsCategory === index ? Theme.textPrimary : Theme.textMuted
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            verticalAlignment: Text.AlignVCenter
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: app.settingsCategory = index
                            onEntered: if (app.settingsCategory !== index) parent.color = Theme.surface
                            onExited: parent.color = app.settingsCategory === index ? Theme.surfaceHover : "transparent"
                        }
                    }
                }
            }
        }

        // Settings Content
        StackLayout {
            currentIndex: app.settingsCategory
            Layout.fillWidth: true
            Layout.fillHeight: true

            EmulatorManagePage {}
            PathsSettings {}
            ScraperSettings {}
        }
    }
}
