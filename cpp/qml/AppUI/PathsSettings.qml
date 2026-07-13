import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // Only show emulators whose adapter actually exposes configurable paths.
    // Emulators like PPSSPP that hardcode their directory layout (no INI
    // overrides) return an empty pathDefs() and are filtered out here so we
    // don't offer UI that can't take effect.
    property var emuList: app.emulatorStatus.filter(function(emu) {
        return app.pathDefs(emu.id).length > 0
    })
    property int currentEmu: 0
    property string currentEmuId: emuList.length > 0 ? emuList[currentEmu].id : ""

    // Focus ring: indices 0..pathRepeater.count-1 are the path cards, and one
    // past the last card (== pathRepeater.count) is the Reset button, so the
    // keyboard can reach Reset too.
    property int focusIndex: 0
    property int resetIndex: pathRepeater.count
    property bool resetFocused: focusIndex === resetIndex

    // Reset every path for the current emulator back to its default location.
    // Shared by the Reset button's mouse click and the keyboard (Enter on the
    // focused Reset button). resetPaths() clears the overrides; we then walk the
    // delegates and set each field.text to its default, because the Browse
    // callback broke the original text binding (see the note below).
    function performReset() {
        app.resetPaths(root.currentEmuId)

        function findField(parent) {
            for (var i = 0; i < parent.children.length; i++) {
                var c = parent.children[i]
                if (c.section !== undefined && c.key !== undefined)
                    return c
                var f = findField(c)
                if (f) return f
            }
            return null
        }

        var defs = app.pathDefs(root.currentEmuId)
        for (var i = 0; i < pathRepeater.count; i++) {
            var item = pathRepeater.itemAt(i)
            if (!item || i >= defs.length) continue
            var field = findField(item)
            if (field) field.text = defs[i].defaultPath
        }
    }

    // ── Keyboard navigation ──────────────────────────────────────────────
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_PageUp || event.key === Qt.Key_Left) {
            // L1 / Left — prev emulator tab
            if (root.currentEmu > 0) {
                root.currentEmu--
                root.focusIndex = 0
            }
            event.accepted = true
        } else if (event.key === Qt.Key_PageDown || event.key === Qt.Key_Right) {
            // R1 / Right — next emulator tab
            if (root.currentEmu < root.emuList.length - 1) {
                root.currentEmu++
                root.focusIndex = 0
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            if (root.focusIndex > 0) root.focusIndex--
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            // Down walks the cards and then onto the Reset button (resetIndex).
            if (root.focusIndex < root.resetIndex) root.focusIndex++
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
            if (root.focusIndex === root.resetIndex) {
                // Reset button focused — reset to defaults.
                root.performReset()
            } else {
                // Open browse for the focused card.
                var card = pathRepeater.itemAt(root.focusIndex)
                if (card) card.triggerBrowse()
            }
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Pill Tabs ────────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            height: 56

            Flickable {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                contentWidth: pillRow.implicitWidth
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Row {
                    id: pillRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Repeater {
                        model: root.emuList
                        delegate: Rectangle {
                            id: pillRect
                            property bool active: root.currentEmu === index

                            height: 32
                            width: pillLabel.implicitWidth + 24
                            radius: SettingsTheme.pillRadius

                            color: active ? SettingsTheme.accent : SettingsTheme.card
                            border.width: active ? 0 : 1
                            border.color: SettingsTheme.border

                            Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }

                            Text {
                                id: pillLabel
                                anchors.centerIn: parent
                                text: modelData.name
                                font.pixelSize: 11
                                font.weight: active ? Font.DemiBold : Font.Normal
                                color: active ? SettingsTheme.background : SettingsTheme.textMuted
                                Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.currentEmu = index
                                    root.focusIndex = 0
                                    root.forceActiveFocus()
                                }
                            }
                        }
                    }
                }
            }

            // Bottom border
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: SettingsTheme.border
            }
        }

        // ── Scrollable Path Cards ────────────────────────────────────────
        Flickable {
            id: pathFlickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: pathCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: pathCol
                width: parent.width
                spacing: 10

                Item { height: 8 }

                Repeater {
                    id: pathRepeater
                    model: app.pathDefs(root.currentEmuId)

                    delegate: Item {
                        id: cardItem
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        height: cardRect.height

                        property bool cardFocused: root.focusIndex === index

                        // Expose browse action so keyboard handler can call it.
                        // Auto-save the moment a folder is chosen — no Save button.
                        function triggerBrowse() {
                            var dir = app.browsePath("Choose " + modelData.label + " folder")
                            if (dir) {
                                pathField.text = dir
                                commitField()
                            }
                        }

                        // Auto-save this one field, then re-sync the visible text
                        // from the store (the source of truth). If the save was
                        // rejected — e.g. a pathological folder tripped the guard
                        // in ConfigService::savePaths — pathValue() returns the
                        // previous value and the field snaps back, while that
                        // rejection message surfaces as a toast (StatusBar binds
                        // app.statusMessage). On success it shows the committed
                        // path and savePaths emits the "Paths saved." toast.
                        function commitField() {
                            // Skip when nothing changed — a focus-out can re-fire
                            // editingFinished with the same value, and we don't
                            // want a redundant save or a spurious "saved" toast.
                            var stored = app.pathValue(root.currentEmuId,
                                                       modelData.section, modelData.key)
                                         || modelData.defaultPath
                            if (pathField.text === stored)
                                return
                            var values = {}
                            values[modelData.section + "/" + modelData.key] = pathField.text
                            app.savePaths(root.currentEmuId, values)
                            pathField.text = app.pathValue(root.currentEmuId,
                                                           modelData.section, modelData.key)
                                             || modelData.defaultPath
                        }

                        // Glow layer (behind card)
                        Rectangle {
                            id: glowRect
                            anchors.fill: cardRect
                            anchors.margins: -4
                            radius: cardRect.radius + 4
                            color: "transparent"
                            border.width: 2
                            border.color: SettingsTheme.focusBorder
                            opacity: cardItem.cardFocused ? 0.3 : 0
                            z: -1
                            visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                        }

                        FocusableItem {
                            id: cardRect
                            anchors.left: parent.left
                            anchors.right: parent.right
                            isFocused: cardItem.cardFocused
                            height: cardInner.height + 24

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.focusIndex = index
                                    root.forceActiveFocus()
                                }
                            }

                            ColumnLayout {
                                id: cardInner
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 12
                                spacing: 6

                                // Label
                                Text {
                                    text: modelData.label.toUpperCase()
                                    color: SettingsTheme.textMuted
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                    font.letterSpacing: 0.8
                                }

                                // Value row
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    // TextField
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 30
                                        radius: 4
                                        color: SettingsTheme.base
                                        border.width: 1
                                        border.color: pathField.activeFocus
                                            ? SettingsTheme.accent
                                            : SettingsTheme.border

                                        TextInput {
                                            id: pathField
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            anchors.verticalCenter: parent.verticalCenter
                                            verticalAlignment: Text.AlignVCenter
                                            clip: true

                                            text: app.pathValue(root.currentEmuId, modelData.section, modelData.key)
                                                  || modelData.defaultPath

                                            color: SettingsTheme.text
                                            font.pixelSize: 11
                                            selectByMouse: true

                                            // Custom properties for save logic
                                            property string section: modelData.section
                                            property string key: modelData.key

                                            // Auto-save on Enter / focus-out when
                                            // the path is typed rather than browsed.
                                            onEditingFinished: cardItem.commitField()
                                        }
                                    }

                                    // Browse button
                                    Rectangle {
                                        id: browseRect
                                        width: browseLabel.implicitWidth + 16
                                        height: 30
                                        radius: 4
                                        color: SettingsTheme.card
                                        border.width: 1
                                        border.color: SettingsTheme.border

                                        Text {
                                            id: browseLabel
                                            anchors.centerIn: parent
                                            text: "Browse"
                                            color: SettingsTheme.accent
                                            font.pixelSize: 9
                                            font.weight: Font.DemiBold
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: cardItem.triggerBrowse()
                                        }
                                    }
                                }
                            }
                        }

                        // Ensure focused card scrolls into view
                        onCardFocusedChanged: {
                            if (cardFocused) {
                                var yTop = cardItem.y
                                var yBot = cardItem.y + cardItem.height
                                var visTop = pathFlickable.contentY
                                var visBot = pathFlickable.contentY + pathFlickable.height
                                if (yTop < visTop + 16)
                                    pathFlickable.contentY = Math.max(0, yTop - 16)
                                else if (yBot > visBot - 16)
                                    pathFlickable.contentY = yBot - pathFlickable.height + 16
                            }
                        }
                    }
                }

                Item { height: 8 }
            }
        }

        // ── Bottom Bar ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: SettingsTheme.background

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: SettingsTheme.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                // No Save button — each field auto-saves on change (Browse
                // returns / Enter / focus-out) via cardItem.commitField(), and
                // savePaths emits the "Paths saved." toast. Reset stays: it's the
                // one-click way back to the default location.

                // Reset button — keyboard-focusable (root.resetFocused, the last
                // stop in the focus ring) as well as clickable.
                Rectangle {
                    id: resetRect
                    width: resetLabel.implicitWidth + 24
                    height: 34
                    radius: SettingsTheme.buttonRadius
                    color: root.resetFocused ? SettingsTheme.base : SettingsTheme.card
                    border.width: root.resetFocused ? 2 : 1
                    border.color: root.resetFocused ? SettingsTheme.focusBorder
                                                    : SettingsTheme.border

                    Behavior on border.color { ColorAnimation { duration: SettingsTheme.animFast } }

                    Text {
                        id: resetLabel
                        anchors.centerIn: parent
                        text: "Reset"
                        color: root.resetFocused ? SettingsTheme.text : SettingsTheme.textMuted
                        font.pixelSize: 11
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // Focus Reset (so keyboard + mouse stay in sync) and
                            // reset all paths to their defaults via the shared
                            // root.performReset().
                            root.focusIndex = root.resetIndex
                            root.forceActiveFocus()
                            root.performReset()
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Button hints
                ButtonHints {
                    hints: [
                        {action: "confirm", label: "Browse"},
                        {action: "back",    label: "Back"}
                    ]
                    height: parent.height
                }
            }
        }
    }
}
