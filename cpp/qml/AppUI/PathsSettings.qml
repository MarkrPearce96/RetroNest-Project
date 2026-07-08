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

    property int focusIndex: 0

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
            var maxIdx = pathRepeater.count - 1
            if (root.focusIndex < maxIdx) root.focusIndex++
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
            // Open browse for focused card
            var card = pathRepeater.itemAt(root.focusIndex)
            if (card) card.triggerBrowse()
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

                        // Expose browse action so keyboard handler can call it
                        function triggerBrowse() {
                            var dir = app.browsePath("Choose " + modelData.label + " folder")
                            if (dir) pathField.text = dir
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

                // Save button
                Rectangle {
                    id: saveRect
                    width: saveLabel.implicitWidth + 24
                    height: 34
                    radius: SettingsTheme.buttonRadius
                    color: SettingsTheme.accent

                    Text {
                        id: saveLabel
                        anchors.centerIn: parent
                        text: "Save"
                        color: SettingsTheme.background
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var values = {}
                            for (var i = 0; i < pathRepeater.count; i++) {
                                var item = pathRepeater.itemAt(i)
                                if (item) {
                                    // Navigate: item (delegate Item) → cardRect (FocusableItem)
                                    // → cardInner (ColumnLayout) → row (RowLayout) → TextField rect → TextInput
                                    // Easier: walk children to find the TextInput with .section property
                                    var found = findField(item)
                                    if (found && found.section)
                                        values[found.section + "/" + found.key] = found.text
                                }
                            }
                            app.savePaths(root.currentEmuId, values)
                        }

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
                    }
                }

                // Reset button
                Rectangle {
                    id: resetRect
                    width: resetLabel.implicitWidth + 24
                    height: 34
                    radius: SettingsTheme.buttonRadius
                    color: SettingsTheme.card
                    border.width: 1
                    border.color: SettingsTheme.border

                    Text {
                        id: resetLabel
                        anchors.centerIn: parent
                        text: "Reset"
                        color: SettingsTheme.textMuted
                        font.pixelSize: 11
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // Clear overrides (libretro) or write defaults to
                            // INI (native) — backend picks the right one based
                            // on adapter->configFilePath().isEmpty().
                            app.resetPaths(root.currentEmuId)

                            // After resetPaths, pathValue() returns the cleared
                            // state for libretro / the default for native. But
                            // the visible TextInput.text was imperatively set
                            // by the Browse callback, which broke the original
                            // binding. The currentEmu = -1 / = tmp trick that
                            // used to live here gets coalesced by Qt when both
                            // assignments happen in one tick, so the Repeater
                            // delegates aren't always recreated. Walk the
                            // delegates and reset each field.text to its
                            // current default path — guaranteed visible refresh.
                            var defs = app.pathDefs(root.currentEmuId)
                            for (var i = 0; i < pathRepeater.count; i++) {
                                var item = pathRepeater.itemAt(i)
                                if (!item || i >= defs.length) continue
                                var field = findField(item)
                                if (field) field.text = defs[i].defaultPath
                            }
                        }

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
