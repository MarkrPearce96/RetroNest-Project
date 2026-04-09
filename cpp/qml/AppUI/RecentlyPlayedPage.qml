import QtQuick
import QtQuick.Layouts
import AppUI

Item {
    id: root
    focus: true

    property var recentGames: []

    ListView {
        id: gameList
        anchors.fill: parent
        anchors.margins: 20
        spacing: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: recentGames

        header: Text {
            text: recentGames.length + " recently played games"
            color: SettingsTheme.textDim
            font.pixelSize: 12
            bottomPadding: 12
        }

        delegate: Rectangle {
            required property var modelData
            required property int index

            width: gameList.width
            height: 60
            radius: 10
            color: SettingsTheme.card
            border.width: 1
            border.color: SettingsTheme.border

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                // Game icon
                Rectangle {
                    width: 40; height: 40; radius: 8
                    color: SettingsTheme.surface

                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: modelData.imageIcon ? "https://retroachievements.org" + modelData.imageIcon : ""
                        fillMode: Image.PreserveAspectFit
                        visible: status === Image.Ready
                    }
                }

                // Title and console
                Column {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: modelData.title || ""
                        color: SettingsTheme.text
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        width: parent.width
                    }

                    Text {
                        text: modelData.consoleName || ""
                        color: SettingsTheme.textDim
                        font.pixelSize: 11
                    }
                }

                // Achievement count
                Column {
                    Layout.alignment: Qt.AlignRight
                    spacing: 2

                    Text {
                        text: (modelData.numAchieved || 0) + " / " + (modelData.numPossible || 0)
                        color: SettingsTheme.textMuted
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight
                    }

                    Rectangle {
                        width: 80; height: 3; radius: 2
                        color: SettingsTheme.border

                        Rectangle {
                            width: parent.width * Math.min(1, (modelData.numAchieved || 0) / Math.max(1, modelData.numPossible || 1))
                            height: parent.height; radius: 2
                            color: SettingsTheme.accent
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (modelData.gameId > 0 && typeof panelStack !== 'undefined')
                        panelStack.push(achievementsPageComponent, { raGameId: modelData.gameId, gameTitle: modelData.title })
                }
            }
        }

        // Empty state
        Text {
            anchors.centerIn: parent
            visible: recentGames.length === 0
            text: "No recently played games"
            color: SettingsTheme.textDim
            font.pixelSize: 14
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            event.accepted = true
            if (typeof panelStack !== 'undefined' && panelStack.depth > 1)
                panelStack.pop()
        }
    }
}
