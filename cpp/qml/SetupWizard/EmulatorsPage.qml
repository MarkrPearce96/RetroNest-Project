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
        spacing: 16

        Text {
            text: "Choose Emulators"
            color: WizardTheme.textPrimary
            font.pixelSize: 28
            font.weight: Font.Bold
        }

        Text {
            text: "Click or use arrow keys + Enter to select. All are selected by default."
            color: WizardTheme.textMuted
            font.pixelSize: 15
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
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
