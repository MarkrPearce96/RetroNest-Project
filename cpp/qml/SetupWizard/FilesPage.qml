import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var emuList: []

    function refresh() {
        emuList = emulators.allEmulators().filter(function(e) { return e.selected })
    }

    Flickable {
        anchors.fill: parent
        anchors.leftMargin: WizardTheme.pageMargin
        anchors.rightMargin: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        contentWidth: width
        contentHeight: contentCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: 16

            // Title
            Text {
                text: "Files"
                font.pixelSize: 28; font.weight: Font.Bold
                color: WizardTheme.textPrimary
            }

            Text {
                text: "Check BIOS status and ROM folder locations."
                font.pixelSize: 15; color: WizardTheme.textMuted
                wrapMode: Text.WordWrap; Layout.fillWidth: true
            }

            // ── BIOS Sub-Section ──
            Text {
                text: "BIOS"
                font.pixelSize: 13; font.weight: Font.Bold
                color: WizardTheme.textMuted
                Layout.topMargin: 8
            }

            // Per-emulator BIOS status (simplified: green/red indicator)
            Repeater {
                model: root.emuList

                ColumnLayout {
                    // Cache BIOS status once so each delegate reads it only once
                    readonly property var biosList: emulators.biosStatus(modelData.id)
                    readonly property bool biosFound: biosList && biosList.some(function(b) { return b.found })

                    Layout.fillWidth: true
                    spacing: 8
                    visible: biosList && biosList.length > 0

                    Text {
                        text: modelData.name
                        font.pixelSize: 14; font.weight: Font.DemiBold
                        color: WizardTheme.textPrimary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 40; radius: 8
                        color: biosFound ? Qt.rgba(0.42, 0.61, 0.29, 0.12) : Qt.rgba(0.78, 0.31, 0.25, 0.12)
                        border.width: 1
                        border.color: biosFound ? Qt.rgba(0.42, 0.61, 0.29, 0.25) : Qt.rgba(0.78, 0.31, 0.25, 0.25)

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left; anchors.leftMargin: 12
                            spacing: 10

                            Rectangle {
                                width: 8; height: 8; radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: biosFound ? WizardTheme.success : WizardTheme.error
                            }
                            Text {
                                text: biosFound ? "BIOS Detected" : "No BIOS Detected"
                                font.pixelSize: 12; font.weight: Font.Medium
                                color: biosFound ? WizardTheme.success : WizardTheme.error
                            }
                        }
                    }
                }
            }

            // BIOS action buttons
            RowLayout {
                spacing: 8

                Button {
                    text: "Open BIOS Folder"
                    onClicked: wizard.openFolder(wizard.rootPath + "/bios")
                    implicitHeight: 36
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? WizardTheme.surfaceHover : WizardTheme.surface
                        border.width: 1; border.color: WizardTheme.divider
                    }
                    contentItem: Text {
                        text: parent.text; font.pixelSize: 12
                        color: WizardTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    text: "Refresh"
                    onClicked: root.refresh()
                    implicitHeight: 36
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? WizardTheme.surfaceHover : WizardTheme.surface
                        border.width: 1; border.color: WizardTheme.divider
                    }
                    contentItem: Text {
                        text: parent.text; font.pixelSize: 12
                        color: WizardTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // ── ROM Folders Sub-Section ──
            Text {
                text: "ROM FOLDERS"
                font.pixelSize: 13; font.weight: Font.Bold
                color: WizardTheme.textMuted
                Layout.topMargin: 16
            }

            Text {
                text: "Place your ROM files in the system folders below."
                font.pixelSize: 13; color: WizardTheme.textDim
                wrapMode: Text.WordWrap; Layout.fillWidth: true
            }

            Repeater {
                model: emulators.availableSystems()

                Rectangle {
                    Layout.fillWidth: true
                    height: 44; radius: 6
                    color: WizardTheme.surface
                    border.width: 1; border.color: WizardTheme.divider

                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10
                        Text {
                            text: modelData.toUpperCase()
                            font.pixelSize: 12; font.weight: Font.Bold
                            color: WizardTheme.textPrimary
                            Layout.preferredWidth: 60
                        }
                        Text {
                            text: wizard.romsDir + "/" + modelData + "/"
                            font.pixelSize: 11
                            color: WizardTheme.textDim
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Button {
                text: "Open ROM Folder"
                onClicked: wizard.openFolder(wizard.romsDir)
                implicitHeight: 36
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? WizardTheme.surfaceHover : WizardTheme.surface
                    border.width: 1; border.color: WizardTheme.divider
                }
                contentItem: Text {
                    text: parent.text; font.pixelSize: 12
                    color: WizardTheme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { height: 20 }
        }
    }
}
