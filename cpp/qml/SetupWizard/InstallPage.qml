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
        anchors.fill: parent
        anchors.margins: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        spacing: 0

        Text {
            text: installer.installDone ? "ALL SET" : "SETTING UP"
            color: WizardTheme.textDim
            font.pixelSize: 13
            font.letterSpacing: 3
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
        }

        Text {
            text: installer.installDone ? "You're ready to play" : "Setting things up…"
            color: WizardTheme.textPrimary
            font.pixelSize: 40
            font.weight: Font.ExtraBold
            font.letterSpacing: -1.2
            Layout.fillWidth: true
            Layout.topMargin: 14
            wrapMode: Text.WordWrap
        }

        Text {
            text: installer.installStatus
            color: installer.installDone ? WizardTheme.success : WizardTheme.textSecondary
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.topMargin: 14
            Layout.bottomMargin: 32

            Behavior on color { ColorAnimation { duration: WizardTheme.animSlow } }
        }

        // Progress bar — glass track, accent fill (success once done)
        Rectangle {
            id: progressTrack
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.preferredHeight: 6
            radius: 3
            color: WizardTheme.surface
            border.width: 1
            border.color: WizardTheme.surfaceBorder

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * installer.progress
                radius: 3
                color: installer.installDone ? WizardTheme.success : WizardTheme.accent

                Behavior on width { NumberAnimation { duration: WizardTheme.animNormal } }
                Behavior on color { ColorAnimation { duration: WizardTheme.animSlow } }
            }
        }

        // ── Completion info: where your ROMs/BIOS live, and how to add games ──
        ColumnLayout {
            visible: installer.installDone
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.topMargin: 28
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: WizardTheme.surface
                border.width: 1
                border.color: WizardTheme.surfaceBorder
                implicitHeight: foldersCol.implicitHeight + 32

                ColumnLayout {
                    id: foldersCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    spacing: 16

                    FolderRow {
                        Layout.fillWidth: true
                        iconText: "🎮"
                        labelText: "ROMs folder"
                        pathText: wizard.romsRoot
                        onOpenRequested: wizard.openFolder(wizard.romsRoot)
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: WizardTheme.divider }

                    FolderRow {
                        Layout.fillWidth: true
                        iconText: "🧩"
                        labelText: "BIOS folder"
                        pathText: wizard.biosRoot
                        onOpenRequested: wizard.openFolder(wizard.biosRoot)
                    }
                }
            }

            Text {
                text: "✨ Drop your games into the ROMs folder above and RetroNest will find them automatically."
                textFormat: Text.StyledText
                color: WizardTheme.textSecondary
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Item { Layout.fillHeight: true }

        // Open App CTA — white pill, same language as the other pages' primary CTA
        Rectangle {
            id: openAppPill
            Layout.preferredWidth: 220
            Layout.preferredHeight: WizardTheme.pillHeight
            radius: WizardTheme.pillRadius
            color: WizardTheme.ctaBg
            visible: installer.installDone
            opacity: installer.installDone ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animSlow } }

            scale: openAppMa.pressed ? 0.97 : 1.0
            Behavior on scale { NumberAnimation { duration: 100 } }

            Text {
                anchors.centerIn: parent
                text: "Open RetroNest"
                color: WizardTheme.ctaText
                font.pixelSize: 15
                font.weight: Font.Bold
            }

            MouseArea {
                id: openAppMa
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: wizard.accept()
            }
        }
    }

    // ========== COMPONENTS ==========

    // A read-only folder row: icon + uppercase label + path, click-to-open in Finder.
    // Root is a plain Item (not a RowLayout) so the whole-row MouseArea can sit
    // as a sibling overlay with anchors.fill — Layout items ignore anchors.
    component FolderRow: Item {
        id: row
        property string iconText: ""
        property string labelText: ""
        property string pathText: ""
        signal openRequested()

        implicitHeight: contentRow.implicitHeight

        RowLayout {
            id: contentRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 13

            Text {
                text: row.iconText
                font.pixelSize: 18
                opacity: 0.85
                color: WizardTheme.textPrimary
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: row.labelText
                    color: WizardTheme.textDim
                    font.pixelSize: 12
                    font.letterSpacing: 1.5
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                }
                Text {
                    text: row.pathText
                    color: WizardTheme.textPrimary
                    font.pixelSize: 14
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }

            Text {
                text: "Open ↗"
                color: rowMa.containsMouse ? WizardTheme.textPrimary : WizardTheme.textMuted
                font.pixelSize: 13
                font.weight: Font.DemiBold

                Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
            }
        }

        MouseArea {
            id: rowMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: row.openRequested()
        }
    }
}
