import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root
    focus: true

    // --- Required backend hooks ---
    required property var optionsLoader     // (emuId) => [{label, value}, ...]
    required property var currentLoader     // (emuId) => chosenKey string
    required property var applyChoices      // (choices: {emuId: chosenKey}) => void
    required property string optionKeyField // "value" or "label"

    // --- Optional content ---
    property var previewImages: ({})        // { emuId: { chosenKey: imagePath } }

    // --- Layout knobs ---
    property int cardWidth: 385
    property int colCount: 2
    property real previewAspect: 14/9       // height = width / previewAspect

    // --- State ---
    property var emuCards: []
    property var pendingChoices: ({})
    property int focusCard: 0
    property int focusPill: 0
    property bool focusSave: false

    Component.onCompleted: { loadCards(); root.forceActiveFocus() }
    StackView.onActivated: root.forceActiveFocus()

    function loadCards() {
        var all = app.allEmulatorStatus()
        var cards = []
        for (var i = 0; i < all.length; i++) {
            if (!all[i].installed) continue
            var opts = root.optionsLoader(all[i].id)
            if (opts.length === 0) continue
            cards.push({
                emuId: all[i].id,
                name: all[i].name,
                systems: all[i].systems,
                options: opts,
                current: root.currentLoader(all[i].id)
            })
        }
        emuCards = cards

        var choices = {}
        for (var j = 0; j < cards.length; j++)
            choices[cards[j].emuId] = cards[j].current
        pendingChoices = choices
    }

    function selectPill(cardIndex, pillIndex) {
        var card = emuCards[cardIndex]
        var choices = Object.assign({}, pendingChoices)
        choices[card.emuId] = card.options[pillIndex][root.optionKeyField]
        pendingChoices = choices
        focusCard = cardIndex
        focusPill = pillIndex
    }

    function save() { root.applyChoices(pendingChoices) }

    function previewSource(emuId, chosenKey) {
        if (previewImages[emuId] && previewImages[emuId][chosenKey])
            return previewImages[emuId][chosenKey]
        return ""
    }

    Keys.onUpPressed: {
        if (focusSave) {
            focusSave = false
        } else if (focusCard - colCount >= 0) {
            focusCard -= colCount; focusPill = 0
        }
    }
    Keys.onDownPressed: {
        if (focusSave) return
        if (focusCard + colCount < emuCards.length) {
            focusCard += colCount; focusPill = 0
        } else {
            focusSave = true
        }
    }
    Keys.onLeftPressed: {
        if (focusSave || emuCards.length === 0) return
        if (focusPill > 0) {
            focusPill--
        } else if (focusCard > 0) {
            focusCard--
            focusPill = emuCards[focusCard].options.length - 1
        }
    }
    Keys.onRightPressed: {
        if (focusSave || emuCards.length === 0) return
        var opts = emuCards[focusCard].options
        if (focusPill < opts.length - 1) {
            focusPill++
        } else if (focusCard < emuCards.length - 1) {
            focusCard++
            focusPill = 0
        }
    }
    function _activateFocused() {
        if (focusSave) { save(); return }
        if (focusCard < emuCards.length) selectPill(focusCard, focusPill)
    }
    Keys.onReturnPressed: _activateFocused()
    Keys.onEnterPressed: _activateFocused()

    Flickable {
        anchors.fill: parent
        contentHeight: contentCol.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 4
                radius: 2
                color: SettingsTheme.textGhost
                opacity: 0.6
            }
            background: Rectangle { color: "transparent" }
        }

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: 0

            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 20
                spacing: 28

                Repeater {
                    model: root.emuCards

                    delegate: Item {
                        width: root.cardWidth
                        height: cardCol.height

                        property int cardIndex: index
                        property var cardData: modelData
                        property string selectedKey: root.pendingChoices[cardData.emuId] || cardData.current

                        Column {
                            id: cardCol
                            width: parent.width
                            spacing: 0

                            Row {
                                spacing: 6
                                bottomPadding: 8

                                Text {
                                    text: cardData.systems
                                    color: SettingsTheme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: cardData.name
                                    color: SettingsTheme.textDim
                                    font.pixelSize: 12
                                    anchors.baseline: parent.children[0].baseline
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: width / root.previewAspect
                                radius: 8
                                color: SettingsTheme.background

                                Image {
                                    anchors.fill: parent
                                    source: root.previewSource(cardData.emuId, selectedKey)
                                    fillMode: Image.PreserveAspectCrop
                                    visible: source !== ""
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "Preview"
                                    color: SettingsTheme.textGhost
                                    font.pixelSize: 13
                                    visible: root.previewSource(cardData.emuId, selectedKey) === ""
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6
                                topPadding: 10

                                Repeater {
                                    model: cardData.options

                                    delegate: Rectangle {
                                        width: (parent.width - (cardData.options.length - 1) * 6) / cardData.options.length
                                        height: 32
                                        radius: SettingsTheme.pillRadius

                                        property bool isSelected: selectedKey === modelData[root.optionKeyField]
                                        property bool isFocused: root.activeFocus
                                                                 && !root.focusSave
                                                                 && root.focusCard === cardIndex
                                                                 && root.focusPill === index

                                        color: isSelected ? SettingsTheme.accent : SettingsTheme.border
                                        border.width: (isFocused || pillMa.containsMouse) ? 3 : 0
                                        border.color: (isFocused || pillMa.containsMouse) ? SettingsTheme.text : "transparent"
                                        scale: (isFocused || pillMa.containsMouse) ? 1.05 : 1.0

                                        Behavior on scale { NumberAnimation { duration: 100 } }
                                        Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.label
                                            color: isSelected ? SettingsTheme.background : SettingsTheme.textMuted
                                            font.pixelSize: 13
                                            font.weight: isSelected ? Font.DemiBold : Font.Normal
                                            Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }
                                        }

                                        MouseArea {
                                            id: pillMa
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.selectPill(cardIndex, index)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.bottomMargin: 20
                Layout.rightMargin: 24
                height: 40

                Rectangle {
                    anchors.right: parent.right
                    width: 100
                    height: 36
                    radius: SettingsTheme.buttonRadius
                    color: (root.focusSave || saveMa.containsMouse)
                           ? Qt.lighter(SettingsTheme.accent, 1.2) : SettingsTheme.accent
                    border.width: root.focusSave ? 2 : 0
                    border.color: SettingsTheme.focusBorder

                    Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }

                    Text {
                        anchors.centerIn: parent
                        text: "Save"
                        color: SettingsTheme.background
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: saveMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.save()
                    }
                }
            }
        }
    }
}
