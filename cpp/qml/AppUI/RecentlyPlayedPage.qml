import QtQuick
import QtQuick.Layouts
import AppUI

GenericListPage {
    id: listPage

    property var recentGames: []

    model: recentGames
    headerText: recentGames.length + " recently played games"
    emptyText: "No recently played games"
    itemSpacing: 6

    onActivated: (index) => {
        var g = recentGames[index]
        if (g && g.gameId > 0 && typeof panelStack !== 'undefined')
            panelStack.push(achievementsPageComponent, { raGameId: g.gameId, gameTitle: g.title })
    }

    delegate: Rectangle {
        id: rowRect
        required property var modelData
        required property int index

        width: ListView.view.width
        height: 60
        radius: 10
        color: SettingsTheme.card
        border.width: ListView.isCurrentItem ? 2 : 1
        border.color: ListView.isCurrentItem ? SettingsTheme.focusBorder : SettingsTheme.border

        Behavior on border.color { ColorAnimation { duration: SettingsTheme.animFast } }
        Behavior on border.width { NumberAnimation { duration: SettingsTheme.animFast } }

        // Focus glow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.width: 2
            border.color: SettingsTheme.focusBorder
            opacity: rowRect.ListView.isCurrentItem ? 0.3 : 0
            z: -1
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
        }

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

            // Achievement count + progress
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
            onClicked: listPage.activate(rowRect.index)
        }
    }
}
