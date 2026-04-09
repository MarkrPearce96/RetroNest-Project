import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var emuList: []
    property int _v: 0

    // focusEmu: which emulator row has keyboard focus
    // focusPill: which pill within that row
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
            if (emulators.resolutionOptions(emuList[i].id).length > 0) c++
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
        var opts = focusEmu < emuList.length ? emulators.resolutionOptions(emuList[focusEmu].id) : []
        if (focusPill < opts.length - 1) focusPill++
    }
    Keys.onReturnPressed: root.selectFocused()
    Keys.onEnterPressed: root.selectFocused()

    function selectFocused() {
        if (focusEmu >= emuList.length) return
        var emuId = emuList[focusEmu].id
        var opts = emulators.resolutionOptions(emuId)
        if (focusPill < opts.length) {
            emulators.setResolution(emuId, opts[focusPill].value)
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
            spacing: 0

            Text {
                text: "Configure Resolution"
                color: WizardTheme.textPrimary
                font.pixelSize: 28
                font.weight: Font.Bold
            }

            Text {
                text: "Select a resolution for each system. Higher resolutions look sharper but require more GPU power."
                color: WizardTheme.textMuted
                font.pixelSize: 15
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.bottomMargin: 16
            }

            Repeater {
                model: root.emuList

                delegate: Column {
                    Layout.fillWidth: true
                    spacing: 0

                    property string emuId: modelData.id
                    property var resOpts: emulators.resolutionOptions(emuId)
                    property int emuIndex: index

                    visible: resOpts.length > 0

                    Rectangle {
                        visible: index > 0
                        width: parent.width
                        height: 1
                        color: WizardTheme.divider
                    }

                    // Emulator label row — highlighted when focused
                    Rectangle {
                        width: parent.width
                        height: labelCol.height
                        color: (root.activeFocus && root.focusEmu === emuIndex)
                               ? Qt.rgba(WizardTheme.accent.r, WizardTheme.accent.g, WizardTheme.accent.b, 0.06)
                               : "transparent"
                        radius: 6

                        Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }

                        Column {
                            id: labelCol
                            topPadding: 16
                            bottomPadding: 4
                            leftPadding: 8

                            Text {
                                text: modelData.systems
                                color: WizardTheme.textPrimary
                                font.pixelSize: 18
                                font.weight: Font.Bold
                            }

                            Text {
                                text: modelData.name
                                color: WizardTheme.textDim
                                font.pixelSize: 13
                            }
                        }
                    }

                    Row {
                        spacing: 8
                        leftPadding: 8
                        topPadding: 8
                        bottomPadding: 16

                        Repeater {
                            model: resOpts
                            delegate: PillButton {
                                label: modelData.label
                                selected: {
                                    void(root._v)
                                    return emulators.chosenResolution(emuId) === modelData.value
                                }
                                isFocused: root.activeFocus
                                           && root.focusEmu === emuIndex
                                           && root.focusPill === index
                                onClicked: {
                                    root.focusEmu = emuIndex
                                    root.focusPill = index
                                    emulators.setResolution(emuId, modelData.value)
                                    root._v++
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
