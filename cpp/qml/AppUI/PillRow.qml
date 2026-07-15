import QtQuick

// PillRow — the shared row of equal-width "pills" used by the quick-settings
// pickers (GenericMultiCardPicker's card + GenericRowPicker's row). Renders
// `options` ({label, value}) as pills, highlights the selected one, and reports
// clicks. Keyboard focus is owned by the host: it sets `focusedIndex` to the
// index of the keyboard-focused pill in this row, or -1 when this row has no
// focused pill.
Row {
    id: pillRow

    property var options: []
    property string optionKeyField: "value"   // which field identifies a pill
    property string selectedKey: ""            // options[i][optionKeyField] that is selected
    property int focusedIndex: -1              // keyboard-focused pill index, or -1
    property int pillHeight: 36
    property real focusScale: 1.05

    signal pillActivated(int index)

    spacing: 6

    Repeater {
        model: pillRow.options

        delegate: Rectangle {
            width: (pillRow.width - (pillRow.options.length - 1) * pillRow.spacing)
                   / pillRow.options.length
            height: pillRow.pillHeight
            radius: SettingsTheme.pillRadius

            property bool isSelected: pillRow.selectedKey === modelData[pillRow.optionKeyField]
            property bool isFocused: pillRow.focusedIndex === index

            color: isSelected ? SettingsTheme.accent : SettingsTheme.border
            border.width: (isFocused || pillMa.containsMouse) ? 3 : 0
            border.color: (isFocused || pillMa.containsMouse) ? SettingsTheme.text : "transparent"
            scale: (isFocused || pillMa.containsMouse) ? pillRow.focusScale : 1.0

            Behavior on scale { NumberAnimation { duration: 100 } }
            Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }

            Text {
                anchors.centerIn: parent
                text: modelData.label
                color: isSelected ? SettingsTheme.background : SettingsTheme.textMuted
                font.pixelSize: 13
                font.weight: isSelected ? Font.DemiBold : Font.Normal
                Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }
            }

            MouseArea {
                id: pillMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: pillRow.pillActivated(index)
            }
        }
    }
}
