import QtQuick
import QtQuick.Controls

Item {
    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 16

        cellWidth: 174
        cellHeight: 256
        clip: true

        model: gameModel

        delegate: GameCard {
            width: grid.cellWidth - 12
            height: grid.cellHeight - 12
        }

        // Empty state
        Text {
            anchors.centerIn: parent
            visible: gameModel.count === 0
            text: "No games yet.\nImport ROMs to get started."
            color: Theme.textDim
            font.pixelSize: 15
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.5
        }
    }
}
