import QtQuick
import QtQuick.Layouts
Item {
    id: root

    property bool isCurrentPage: false

    focus: true

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        width: parent.width * 0.7

        Text {
            text: "RetroNest"
            color: WizardTheme.textPrimary
            font.pixelSize: 36
            font.weight: Font.Bold
            Layout.alignment: Qt.AlignHCenter

            opacity: root.isCurrentPage ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animSlow } }
        }

        Text {
            text: "Your all-in-one retro gaming setup.\nLet's get everything configured in a few quick steps."
            color: WizardTheme.textMuted
            font.pixelSize: 15
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            lineHeight: 1.5
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true

            opacity: root.isCurrentPage ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animSlow; easing.type: Easing.OutCubic } }
        }
    }
}
