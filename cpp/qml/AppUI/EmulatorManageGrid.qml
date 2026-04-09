import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import "EmulatorLogos.js" as EmulatorLogos

Item {
    id: root

    signal emulatorSelected(string emuId)

    property var emuList: app.allEmulatorStatus()
    property int focusIndex: 0

    focus: true

    Connections {
        target: app
        function onEmulatorInstalled() {
            root.emuList = app.allEmulatorStatus()
        }
    }

    function _activateFocused() {
        if (emuList.length > 0) root.emulatorSelected(emuList[focusIndex].id)
    }

    Keys.onUpPressed:    focusIndex = focusIndex > 0 ? focusIndex - 1 : emuList.length - 1
    Keys.onDownPressed:  focusIndex = focusIndex < emuList.length - 1 ? focusIndex + 1 : 0
    Keys.onReturnPressed: _activateFocused()
    Keys.onEnterPressed: _activateFocused()

    Flickable {
        anchors.fill: parent
        contentHeight: listCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: listCol
            width: parent.width
            spacing: SettingsTheme.itemSpacing

            // Top padding
            Item { height: 12 }

            // Emulator rows
            Repeater {
                model: root.emuList

                delegate: FocusableItem {
                    id: rowItem
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.preferredHeight: 108
                    isFocused: index === root.focusIndex

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 18

                        // Logo area
                        Rectangle {
                            width: 72
                            height: 72
                            radius: 10
                            color: SettingsTheme.border

                            Image {
                                id: logoImg
                                anchors.fill: parent
                                source: EmulatorLogos.logoForEmu(modelData.id)
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                mipmap: true
                                visible: false
                            }

                            Rectangle {
                                id: logoMask
                                anchors.fill: parent
                                radius: 10
                                visible: false
                            }

                            OpacityMask {
                                anchors.fill: parent
                                source: logoImg
                                maskSource: logoMask
                                visible: EmulatorLogos.logoForEmu(modelData.id) !== ""
                            }

                            // Fallback emoji if no logo
                            Text {
                                anchors.centerIn: parent
                                text: "\uD83C\uDFAE"
                                font.pixelSize: 32
                                visible: EmulatorLogos.logoForEmu(modelData.id) === ""
                            }
                        }

                        // Name / system / description
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: modelData.name || modelData.id
                                color: SettingsTheme.text
                                font.pixelSize: 22
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: modelData.system || ""
                                color: SettingsTheme.textDim
                                font.pixelSize: 16
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text !== ""
                            }
                            Text {
                                text: modelData.description || ""
                                color: SettingsTheme.textFaint
                                font.pixelSize: 15
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text !== ""
                            }
                        }

                        // Install status badge
                        Rectangle {
                            width: badgeLabel.width + 28
                            height: 36
                            radius: SettingsTheme.pillRadius
                            color: modelData.installed ? SettingsTheme.successDim : SettingsTheme.accentDim

                            Text {
                                id: badgeLabel
                                anchors.centerIn: parent
                                text: modelData.installed ? "Installed" : "Not Installed"
                                color: modelData.installed ? SettingsTheme.success : SettingsTheme.accent
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                        }

                        // Chevron
                        Text {
                            text: "\u203A"
                            color: SettingsTheme.textGhost
                            font.pixelSize: 26
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onContainsMouseChanged: parent.isHovered = containsMouse
                        onClicked: {
                            root.focusIndex = index
                            root.emulatorSelected(modelData.id)
                        }
                    }
                }
            }

            // "Coming Soon" placeholder row
            FocusableItem {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.preferredHeight: 108
                isFocused: false
                opacity: 0.35

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 18

                    Rectangle {
                        width: 72
                        height: 72
                        radius: 10
                        color: SettingsTheme.border

                        Text {
                            anchors.centerIn: parent
                            text: "\u2795"
                            font.pixelSize: 32
                            color: SettingsTheme.textGhost
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: "More Emulators"
                            color: SettingsTheme.text
                            font.pixelSize: 22
                            font.weight: Font.Medium
                        }
                        Text {
                            text: "Coming Soon"
                            color: SettingsTheme.textDim
                            font.pixelSize: 16
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "\u203A"
                        color: SettingsTheme.textGhost
                        font.pixelSize: 26
                    }
                }
            }

            // Bottom padding
            Item { height: 16 }
        }
    }
}
