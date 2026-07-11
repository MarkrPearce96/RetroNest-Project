import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: WizardTheme.pillWidth
    height: WizardTheme.pillHeight

    property string label: ""
    property bool selected: false
    property bool isFocused: false

    signal clicked()

    // Focus glow behind the pill
    Rectangle {
        anchors.fill: bg
        anchors.margins: -4
        radius: bg.radius + 4
        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(WizardTheme.accent.r, WizardTheme.accent.g, WizardTheme.accent.b, 0.4)
        opacity: root.isFocused ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: WizardTheme.animFast } }
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: WizardTheme.pillRadius
        color: root.selected ? WizardTheme.cardSelected : WizardTheme.surface
        border.width: root.isFocused || root.selected ? 2 : 1
        border.color: root.isFocused ? WizardTheme.accent : (root.selected ? WizardTheme.accent : WizardTheme.surfaceBorder)

        Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
        Behavior on border.color { ColorAnimation { duration: WizardTheme.animFast } }
        Behavior on border.width { NumberAnimation { duration: WizardTheme.animFast } }

        Text {
            anchors.centerIn: parent
            text: root.label
            color: root.selected ? WizardTheme.textPrimary : WizardTheme.textMuted
            font.pixelSize: 14
            font.weight: root.selected ? Font.DemiBold : Font.Medium

            Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
        }

        // Check badge
        Rectangle {
            visible: root.selected
            width: 18
            height: 18
            radius: 9
            color: WizardTheme.accent
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: -4
            anchors.topMargin: -4

            opacity: root.selected ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animFast } }

            Text {
                anchors.centerIn: parent
                text: "\u2713"
                color: WizardTheme.textPrimary
                font.pixelSize: 11
                font.weight: Font.Bold
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        onPressed: bg.scale = 0.96
        onReleased: bg.scale = 1.0
    }

    Behavior on scale { NumberAnimation { duration: 100 } }
}
