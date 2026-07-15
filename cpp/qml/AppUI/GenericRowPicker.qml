import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// GenericRowPicker — a card-per-emulator variant of GenericMultiCardPicker with
// no preview image. Each installed emulator gets a full-width card: a console
// badge + name on the left, a row of option pills on the right. Used where a
// preview can't convey the choice (e.g. Resolution). Same backend hooks +
// auto-save as the card picker; keyboard nav is 2-D (Up/Down between emulator
// cards, Left/Right between pills).
FocusScope {
    id: root
    focus: true

    // --- Required backend hooks (same contract as GenericMultiCardPicker) ---
    required property var optionsLoader     // (emuId) => [{label, value}, ...]
    required property var currentLoader     // (emuId) => chosenKey string
    required property var applyChoices      // (choices: {emuId: chosenKey}) => void
    required property string optionKeyField // "value" or "label"

    // No Save button — picking a pill applies immediately (with the backend's toast).

    // --- State ---
    property var emuCards: []
    property var pendingChoices: ({})
    property int focusRow: 0
    property int focusPill: 0

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

    function selectPill(rowIndex, pillIndex) {
        var card = emuCards[rowIndex]
        var chosen = card.options[pillIndex][root.optionKeyField]
        var choices = Object.assign({}, pendingChoices)
        choices[card.emuId] = chosen
        pendingChoices = choices
        focusRow = rowIndex
        focusPill = pillIndex
        var one = {}
        one[card.emuId] = chosen
        root.applyChoices(one)
    }

    Keys.onUpPressed: { if (focusRow > 0) { focusRow--; focusPill = 0 } }
    Keys.onDownPressed: { if (focusRow < emuCards.length - 1) { focusRow++; focusPill = 0 } }
    Keys.onLeftPressed: { if (emuCards.length > 0 && focusPill > 0) focusPill-- }
    Keys.onRightPressed: {
        if (emuCards.length === 0) return
        if (focusPill < emuCards[focusRow].options.length - 1) focusPill++
    }
    function _activateFocused() {
        if (focusRow < emuCards.length) selectPill(focusRow, focusPill)
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

        Column {
            id: contentCol
            width: parent.width
            spacing: 12
            topPadding: 20

            Repeater {
                model: root.emuCards

                delegate: Rectangle {
                    id: rowCard
                    x: 24
                    width: parent.width - 48
                    height: rowInner.implicitHeight + 28
                    radius: 12
                    color: SettingsTheme.card

                    property int rowIndex: index
                    property var cardData: modelData
                    property string selectedKey: root.pendingChoices[cardData.emuId] || cardData.current
                    property bool rowFocused: root.activeFocus && root.focusRow === rowIndex

                    border.width: 1
                    border.color: rowFocused
                                  ? Qt.rgba(SettingsTheme.accent.r, SettingsTheme.accent.g,
                                            SettingsTheme.accent.b, 0.5)
                                  : SettingsTheme.border
                    Behavior on border.color { ColorAnimation { duration: SettingsTheme.animFast } }

                    RowLayout {
                        id: rowInner
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 18

                        // Left: console badge + emulator name
                        Column {
                            Layout.preferredWidth: 150
                            spacing: 6

                            Rectangle {
                                width: badgeText.implicitWidth + 14
                                height: badgeText.implicitHeight + 6
                                radius: 5
                                color: Qt.rgba(SettingsTheme.accent.r, SettingsTheme.accent.g,
                                               SettingsTheme.accent.b, 0.14)

                                Text {
                                    id: badgeText
                                    anchors.centerIn: parent
                                    text: rowCard.cardData.systems.toUpperCase()
                                    color: SettingsTheme.accent
                                    font.pixelSize: 10
                                    font.letterSpacing: 0.4
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                text: rowCard.cardData.name
                                color: SettingsTheme.text
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                        }

                        // Right: option pills — fill remaining width, evenly divided
                        PillRow {
                            Layout.fillWidth: true
                            options: rowCard.cardData.options
                            optionKeyField: root.optionKeyField
                            selectedKey: rowCard.selectedKey
                            pillHeight: 36
                            focusScale: 1.03
                            focusedIndex: (root.activeFocus && root.focusRow === rowCard.rowIndex)
                                          ? root.focusPill : -1
                            onPillActivated: (index) => root.selectPill(rowCard.rowIndex, index)
                        }
                    }
                }
            }
        }
    }
}
