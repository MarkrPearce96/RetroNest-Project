import QtQuick
import QtQuick.Layouts
import AppUI

Item {
    id: root
    anchors.fill: parent
    focus: true

    property var allGames: []

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            if (typeof panelStack !== 'undefined') panelStack.pop()
            event.accepted = true
        }
    }

    // Title
    Text {
        id: titleText
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.leftMargin: 20
        text: "All Games (" + allGames.length + ")"
        color: SettingsTheme.text
        font.pixelSize: 18
        font.weight: Font.Bold
    }

    ListView {
        id: gamesList
        anchors.top: titleText.bottom
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        clip: true
        spacing: 6
        model: allGames
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            width: gamesList.width
            height: 56
            radius: 8
            color: gameMa.containsMouse ? Qt.lighter(SettingsTheme.card, 1.15) : SettingsTheme.card
            border.width: 1
            border.color: SettingsTheme.border

            MouseArea {
                id: gameMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof panelStack !== 'undefined')
                        panelStack.push(achievementsPageComponent, { raGameId: modelData.raGameId, gameTitle: modelData.title })
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 12

                // Game icon
                Rectangle {
                    width: 40
                    height: 40
                    radius: 6
                    color: SettingsTheme.surface

                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: modelData.imageIcon ? "https://retroachievements.org" + modelData.imageIcon : ""
                        fillMode: Image.PreserveAspectFit
                        visible: status === Image.Ready
                    }
                }

                // Title + console
                Column {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: modelData.title || ""
                        color: SettingsTheme.text
                        font.pixelSize: 13
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
                Text {
                    text: (modelData.numAwarded || 0) + " / " + (modelData.numAchievements || 0)
                    color: SettingsTheme.textMuted
                    font.pixelSize: 12
                }

                // Progress bar
                Rectangle {
                    width: 60
                    height: 4
                    radius: 2
                    color: SettingsTheme.border

                    Rectangle {
                        width: parent.width * Math.min(1, (modelData.numAwarded || 0) / Math.max(1, modelData.numAchievements || 1))
                        height: parent.height
                        radius: 2
                        color: modelData.mastered ? SettingsTheme.success : SettingsTheme.accent
                    }
                }
            }
        }
    }
}
