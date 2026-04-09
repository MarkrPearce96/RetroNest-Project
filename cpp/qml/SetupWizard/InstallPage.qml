import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root

    property bool isCurrentPage: false

    focus: true

    // Allow Enter to activate "Open App" once install is done
    Keys.onReturnPressed: if (installer.installDone) wizard.accept()
    Keys.onEnterPressed: if (installer.installDone) wizard.accept()

    function startInstall() {
        installer.startInstall(wizard.rootPath)
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width * 0.6
        spacing: 20

        Text {
            text: installer.installDone ? "Setup Complete!" : "Setting Up..."
            color: WizardTheme.textPrimary
            font.pixelSize: 28
            font.weight: Font.Bold
            Layout.alignment: Qt.AlignHCenter
        }

        ProgressBar {
            id: progressBar
            Layout.fillWidth: true
            Layout.preferredHeight: 6
            from: 0
            to: 1
            value: installer.progress

            background: Rectangle {
                radius: 3
                color: WizardTheme.surface
            }

            contentItem: Item {
                Rectangle {
                    width: progressBar.visualPosition * parent.width
                    height: parent.height
                    radius: 3
                    color: installer.installDone ? WizardTheme.success : WizardTheme.accent

                    Behavior on width { NumberAnimation { duration: WizardTheme.animNormal } }
                    Behavior on color { ColorAnimation { duration: WizardTheme.animSlow } }
                }
            }
        }

        Text {
            text: installer.installStatus
            color: installer.installDone ? WizardTheme.success : WizardTheme.textMuted
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter

            Behavior on color { ColorAnimation { duration: WizardTheme.animSlow } }
        }

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 208
            Layout.preferredHeight: 52
            visible: installer.installDone
            opacity: installer.installDone ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animSlow } }

            // Focus ring
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

            Button {
                id: openAppBtn
                anchors.fill: parent

                background: Rectangle {
                    radius: 6
                    color: openAppBtn.hovered ? WizardTheme.accentLight : WizardTheme.accent
                    Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
                }

                contentItem: Text {
                    text: "Open App"
                    color: WizardTheme.background
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                scale: pressed ? 0.96 : 1.0
                Behavior on scale { NumberAnimation { duration: 100 } }

                onClicked: wizard.accept()
            }
        }
    }
}
