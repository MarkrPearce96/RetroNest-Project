import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var emuList: []
    property int _v: 0

    // focusEmu: which emulator card has keyboard focus
    // focusPill: which pill within that card
    property int focusEmu: 0
    property int focusPill: 0

    focus: true

    function refresh() {
        emuList = emulators.allEmulators().filter(function(e) { return e.selected })
        _v++
        focusEmu = 0
        focusPill = 0
    }

    function visibleEmuCount() {
        var c = 0
        for (var i = 0; i < emuList.length; i++) {
            if (emulators.aspectRatioOptions(emuList[i].id).length > 0) c++
        }
        return c
    }

    Keys.onUpPressed: {
        if (focusEmu > 0) { focusEmu--; focusPill = 0 }
    }
    Keys.onDownPressed: {
        if (focusEmu < visibleEmuCount() - 1) { focusEmu++; focusPill = 0 }
    }
    Keys.onLeftPressed: {
        if (focusPill > 0) focusPill--
    }
    Keys.onRightPressed: {
        var opts = focusEmu < emuList.length ? emulators.aspectRatioOptions(emuList[focusEmu].id) : []
        if (focusPill < opts.length - 1) focusPill++
    }
    Keys.onReturnPressed: root.selectFocused()
    Keys.onEnterPressed: root.selectFocused()

    function selectFocused() {
        if (focusEmu >= emuList.length) return
        var emuId = emuList[focusEmu].id
        var opts = emulators.aspectRatioOptions(emuId)
        if (focusPill < opts.length) {
            emulators.setAspectRatio(emuId, opts[focusPill].label)
            root._v++
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        contentHeight: contentCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: 20

            Text {
                text: "Aspect Ratio"
                color: WizardTheme.textPrimary
                font.pixelSize: 28
                font.weight: Font.Bold
            }

            Text {
                text: "Choose the display aspect ratio for each system. 4:3 is original, 16:9 fills widescreen displays."
                color: WizardTheme.textMuted
                font.pixelSize: 15
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // Emulator cards
            GridLayout {
                columns: 2
                columnSpacing: 20
                rowSpacing: 20
                Layout.fillWidth: true

                Repeater {
                    model: root.emuList

                    delegate: Rectangle {
                        id: card
                        Layout.fillWidth: true
                        Layout.preferredHeight: 220
                        radius: 16
                        color: WizardTheme.surface
                        border.width: (root.activeFocus && root.focusEmu === index) ? 2 : 1
                        border.color: (root.activeFocus && root.focusEmu === index)
                                      ? WizardTheme.accent : WizardTheme.divider

                        Behavior on border.color { ColorAnimation { duration: WizardTheme.animFast } }
                        Behavior on border.width { NumberAnimation { duration: WizardTheme.animFast } }

                        property string emuId: modelData.id
                        property var arOpts: emulators.aspectRatioOptions(emuId)
                        property int cardIndex: index
                        visible: arOpts.length > 0

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 0
                            spacing: 0

                            // Preview area showing aspect ratio visualization
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 100
                                radius: 16
                                color: Qt.darker(WizardTheme.accent, 2.5)

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: parent.radius
                                    color: parent.color
                                }

                                // Aspect ratio preview boxes
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 16

                                    // 4:3 box
                                    Rectangle {
                                        width: 48
                                        height: 36
                                        radius: 4
                                        color: "transparent"
                                        border.width: 2
                                        border.color: {
                                            void(root._v)
                                            return emulators.chosenAspectRatio(card.emuId) === "4:3"
                                                ? WizardTheme.accent : Qt.rgba(1,1,1,0.2)
                                        }
                                        opacity: {
                                            void(root._v)
                                            return emulators.chosenAspectRatio(card.emuId) === "4:3" ? 1.0 : 0.5
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "4:3"
                                            color: WizardTheme.textPrimary
                                            font.pixelSize: 10
                                            opacity: 0.7
                                        }
                                    }

                                    // 16:9 box
                                    Rectangle {
                                        width: 64
                                        height: 36
                                        radius: 4
                                        color: "transparent"
                                        border.width: 2
                                        border.color: {
                                            void(root._v)
                                            return emulators.chosenAspectRatio(card.emuId) === "16:9"
                                                ? WizardTheme.accent : Qt.rgba(1,1,1,0.2)
                                        }
                                        opacity: {
                                            void(root._v)
                                            return emulators.chosenAspectRatio(card.emuId) === "16:9" ? 1.0 : 0.5
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "16:9"
                                            color: WizardTheme.textPrimary
                                            font.pixelSize: 10
                                            opacity: 0.7
                                        }
                                    }
                                }

                                // System label overlay
                                Text {
                                    anchors.bottom: parent.bottom
                                    anchors.left: parent.left
                                    anchors.margins: 16
                                    text: modelData.systems
                                    color: WizardTheme.textPrimary
                                    font.pixelSize: 20
                                    font.weight: Font.Bold
                                }
                            }

                            // Pills
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.margins: 16
                                spacing: 12

                                Text {
                                    text: modelData.name + " Aspect Ratio"
                                    color: WizardTheme.textDim
                                    font.pixelSize: 13
                                }

                                Row {
                                    spacing: 8

                                    Repeater {
                                        model: card.arOpts
                                        delegate: PillButton {
                                            label: modelData.label
                                            selected: {
                                                void(root._v)
                                                return emulators.chosenAspectRatio(card.emuId) === modelData.label
                                            }
                                            isFocused: root.activeFocus
                                                       && root.focusEmu === card.cardIndex
                                                       && root.focusPill === index
                                            onClicked: {
                                                root.focusEmu = card.cardIndex
                                                root.focusPill = index
                                                emulators.setAspectRatio(card.emuId, modelData.label)
                                                root._v++
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item { height: 20 }
        }
    }
}
