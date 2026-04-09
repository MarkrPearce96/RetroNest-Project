import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root

    focus: true

    Keys.onReturnPressed: root.browse()
    Keys.onEnterPressed: root.browse()

    function browse() {
        var dir = wizard.browseFolder("Choose Data Folder")
        if (dir) {
            wizard.rootPath = dir + "/RetroNest"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        spacing: 16

        Text {
            text: "Choose Data Folder"
            color: WizardTheme.textPrimary
            font.pixelSize: 28
            font.weight: Font.Bold
        }

        Text {
            text: "All emulators, BIOS files, saves, and configuration will be stored here. You can change this later in Settings > Paths."
            color: WizardTheme.textMuted
            font.pixelSize: 15
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Item { height: 8 }

        Rectangle {
            Layout.fillWidth: true
            height: 48
            radius: 8
            color: WizardTheme.surfaceHover

            Text {
                anchors.fill: parent
                anchors.margins: 16
                text: wizard.rootPath || "No folder selected"
                color: wizard.rootPath ? WizardTheme.textPrimary : WizardTheme.textMuted
                font.pixelSize: 13
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideMiddle
            }
        }

        Button {
            id: browseBtn
            Layout.preferredWidth: 180
            Layout.preferredHeight: 40

            // Focus ring — visible when keyboard focus is on this page
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                radius: 10
                color: "transparent"
                border.width: 2
                border.color: Qt.rgba(WizardTheme.accent.r, WizardTheme.accent.g, WizardTheme.accent.b, 0.5)
                opacity: root.activeFocus ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: WizardTheme.animFast } }
            }

            background: Rectangle {
                radius: 6
                color: browseBtn.hovered ? WizardTheme.accentLight : WizardTheme.accent
                Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
            }

            contentItem: Text {
                text: "Choose Folder..."
                color: WizardTheme.background
                font.pixelSize: 13
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            scale: pressed ? 0.96 : 1.0
            Behavior on scale { NumberAnimation { duration: 100 } }

            onClicked: root.browse()
        }

        Item { Layout.fillHeight: true }
    }
}
