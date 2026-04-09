import QtQuick
import QtQuick.Controls

Button {
    id: root
    property string label: ""

    implicitWidth: contentText.implicitWidth + 28
    implicitHeight: 32

    background: Rectangle {
        radius: 6
        color: root.hovered ? Qt.lighter(Theme.surface, 1.2) : Theme.surface
        border.width: 1
        border.color: Theme.divider

        Behavior on color { ColorAnimation { duration: 100 } }
    }

    contentItem: Text {
        id: contentText
        text: root.label
        color: root.hovered ? Theme.textPrimary : Theme.textSecondary
        font.pixelSize: 12
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
