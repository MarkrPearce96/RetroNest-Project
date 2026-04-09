import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property int focusIndex: 0

    Keys.onUpPressed: root.focusIndex = root.focusIndex > 0 ? root.focusIndex - 1 : Math.max(0, themeList.count - 1)
    Keys.onDownPressed: root.focusIndex = root.focusIndex < themeList.count - 1 ? root.focusIndex + 1 : 0
    Keys.onReturnPressed: root.applyTheme(root.focusIndex)
    Keys.onEnterPressed: root.applyTheme(root.focusIndex)

    function applyTheme(idx) {
        var theme = themeManager.availableThemes[idx]
        if (theme && themeManager.currentThemeId !== theme.id) {
            themeManager.currentThemeId = theme.id
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Theme list
        ListView {
            id: themeList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 4
            spacing: 8
            clip: true
            model: themeManager.availableThemes
            currentIndex: root.focusIndex
            focus: true

            delegate: Item {
                id: delegateRoot
                width: themeList.width
                height: 72

                property bool isFocused: index === root.focusIndex
                property bool isActive:  modelData.id === themeManager.currentThemeId

                // Glow behind
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: SettingsTheme.cardRadius + 4
                    color: "transparent"
                    border.width: 2
                    border.color: SettingsTheme.focusBorder
                    opacity: delegateRoot.isFocused ? 0.3 : 0
                    z: -1
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                }

                Rectangle {
                    id: rowRect
                    anchors.fill: parent
                    radius: SettingsTheme.cardRadius
                    color: delegateRoot.isActive
                        ? Qt.rgba(0.91, 0.66, 0.22, 0.06)
                        : SettingsTheme.card

                    border.width: delegateRoot.isFocused ? 2 : 1
                    border.color: delegateRoot.isFocused
                        ? SettingsTheme.focusBorder
                        : (rowHover.containsMouse ? SettingsTheme.textGhost : SettingsTheme.border)

                    Behavior on border.color { ColorAnimation { duration: SettingsTheme.animFast } }
                    Behavior on border.width { NumberAnimation { duration: SettingsTheme.animFast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 16

                        // Color preview swatch
                        Rectangle {
                            width: 64
                            height: 48
                            radius: 6
                            border.width: 1
                            border.color: SettingsTheme.border
                            clip: true

                            // Preview gradient
                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius

                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#1a1917" }
                                    GradientStop { position: 1.0; color: "#0d0c0a" }
                                }
                            }

                            // Accent bar at bottom
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 10
                                radius: 0
                                color: SettingsTheme.accent
                                opacity: 0.6
                            }
                        }

                        // Theme info
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: modelData.name || modelData.id
                                color: SettingsTheme.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: modelData.description || ""
                                color: SettingsTheme.textDim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text.length > 0
                            }

                            Text {
                                text: modelData.author ? "by " + modelData.author : ""
                                color: SettingsTheme.textFaint
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text.length > 0
                            }
                        }

                        // Active badge or Apply button
                        Loader {
                            active: true
                            sourceComponent: delegateRoot.isActive ? activeBadge : applyButton

                            Component {
                                id: activeBadge
                                Row {
                                    spacing: 6

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: SettingsTheme.success
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Text {
                                        text: "Active"
                                        color: SettingsTheme.success
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }

                            Component {
                                id: applyButton
                                Rectangle {
                                    width: 70
                                    height: 32
                                    radius: SettingsTheme.buttonRadius
                                    color: SettingsTheme.card
                                    border.width: 1
                                    border.color: SettingsTheme.border

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Apply"
                                        color: SettingsTheme.textMuted
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                    }
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: rowHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.focusIndex = index
                            root.applyTheme(index)
                        }
                    }
                }
            }

            // Scroll focused item into view
            onCurrentIndexChanged: {
                positionViewAtIndex(currentIndex, ListView.Contain)
            }
        }

        ButtonHints {
            Layout.fillWidth: true
            hints: [
                {action: "navigate_ud", label: "Navigate"},
                {action: "confirm",     label: "Apply Theme"},
                {action: "back",        label: "Back"}
            ]
        }
    }
}
