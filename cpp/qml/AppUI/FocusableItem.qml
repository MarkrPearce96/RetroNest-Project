import QtQuick

Rectangle {
    id: root

    property bool isFocused: false
    property bool isHovered: false

    radius: SettingsTheme.cardRadius
    color: SettingsTheme.card

    border.width: isFocused ? 2 : 1
    border.color: isFocused ? SettingsTheme.focusBorder
                             : (isHovered ? SettingsTheme.textGhost : SettingsTheme.border)

    // Glow effect using a shadow Rectangle behind
    Rectangle {
        id: glowRect
        anchors.fill: parent
        anchors.margins: -4
        radius: parent.radius + 4
        color: "transparent"
        border.width: 2
        border.color: SettingsTheme.focusBorder
        opacity: root.isFocused ? 0.3 : 0
        z: -1
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: SettingsTheme.animFast }
        }
    }

    Behavior on border.color {
        ColorAnimation { duration: SettingsTheme.animFast }
    }

    Behavior on border.width {
        NumberAnimation { duration: SettingsTheme.animFast }
    }

    // Mouse hover support
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.NoButton  // Pass clicks through — parent handles them
        onContainsMouseChanged: root.isHovered = containsMouse
    }
}
