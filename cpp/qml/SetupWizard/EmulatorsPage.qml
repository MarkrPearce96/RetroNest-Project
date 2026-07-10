import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root

    property var emuList: []
    property int focusIndex: 0

    focus: true

    function refresh() {
        emuList = emulators.allEmulators()
    }

    Component.onCompleted: refresh()

    Connections {
        target: emulators
        function onSelectedEmulatorsChanged() { root.refresh() }
    }

    // Keyboard navigation
    readonly property int columns: Math.max(1, Math.floor(grid.width / grid.cellWidth))
    readonly property int count: emuList.length

    Keys.onLeftPressed: {
        if (focusIndex > 0) focusIndex--
    }
    Keys.onRightPressed: {
        if (focusIndex < count - 1) focusIndex++
    }
    Keys.onUpPressed: {
        if (focusIndex - columns >= 0) focusIndex -= columns
    }
    Keys.onDownPressed: {
        if (focusIndex + columns < count) focusIndex += columns
    }
    function _toggleFocused() {
        if (count > 0) emulators.toggleEmulator(emuList[focusIndex].id)
    }

    Keys.onReturnPressed: _toggleFocused()
    Keys.onEnterPressed: _toggleFocused()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        spacing: 0

        Text {
            text: "YOUR CONSOLES"
            color: WizardTheme.textDim
            font.pixelSize: 13
            font.letterSpacing: 3
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
        }

        Text {
            text: "Choose your emulators"
            color: WizardTheme.textPrimary
            font.pixelSize: 40
            font.weight: Font.ExtraBold
            font.letterSpacing: -1.2
            Layout.fillWidth: true
            Layout.topMargin: 14
            wrapMode: Text.WordWrap
        }

        Text {
            text: "Click a tile or use arrow keys + Enter to select. All are selected by default — you can change this later in Settings."
            color: WizardTheme.textSecondary
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.topMargin: 14
            Layout.bottomMargin: 30
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: 156
            cellHeight: 156
            clip: true
            interactive: true

            model: root.emuList

            delegate: EmulatorCard {
                width: 140
                height: 140
                emuId: modelData.id
                emuName: modelData.name
                systems: modelData.systems
                selected: modelData.selected
                isFocused: index === root.focusIndex && root.activeFocus

                onClicked: {
                    root.focusIndex = index
                    emulators.toggleEmulator(modelData.id)
                }
            }
        }
    }
}
