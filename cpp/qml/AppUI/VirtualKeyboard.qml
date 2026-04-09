import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    anchors.fill: parent
    color: "#CC000000"
    visible: false
    z: 200

    // ── Public API ──
    property string text: ""
    property string label: ""
    property bool isPassword: false
    property bool showPassword: false

    signal accepted()
    signal cancelled()

    property string _initialText: ""

    function open(initialText, passwordMode, fieldLabel) {
        _initialText = initialText
        text = initialText
        isPassword = passwordMode
        showPassword = false
        label = fieldLabel || (passwordMode ? "PASSWORD" : "TEXT")
        focusRow = 0
        focusCol = 0
        shifted = false
        numbersMode = false
        visible = true
        inputManager.virtualKeyboardOpen = true
        root.forceActiveFocus()
    }

    function close() {
        visible = false
        inputManager.virtualKeyboardOpen = false
    }

    // ── Internal state ──
    property int focusRow: 0
    property int focusCol: 0
    property bool shifted: false
    property bool numbersMode: false

    // Uniform 10-column grid for clean controller navigation
    property var letterRows: [
        ["a","b","c","d","e","f","g","h","i","j"],
        ["k","l","m","n","o","p","q","r","s","t"],
        ["u","v","w","x","y","z","-","_",".","@"],
        ["123"," ","\u2713","","","","","","",""]
    ]

    property var numberRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["!","@","#","$","%","^","&","*","(",")"],
        ["-","+","=","_",".",",",":",";","'","\""],
        ["abc"," ","\u2713","","","","","","",""]
    ]

    // Bottom row active cells count (the rest are empty/hidden)
    property int bottomRowCells: 3

    property var currentRows: numbersMode ? numberRows : letterRows

    function getDisplayChar(ch) {
        if (ch === "\u2713") return "Done"
        if (ch === " ") return "Space"
        if (shifted && !numbersMode && ch.length === 1 && ch >= "a" && ch <= "z")
            return ch.toUpperCase()
        return ch
    }

    function handleKeyPress(ch) {
        if (ch === "" ) return
        if (ch === "\u2713") {
            accepted()
            close()
            return
        }
        if (ch === "123") {
            numbersMode = true
            focusRow = 0; focusCol = 0
            return
        }
        if (ch === "abc") {
            numbersMode = false
            focusRow = 0; focusCol = 0
            return
        }

        var typed = ch
        if (shifted && !numbersMode && ch.length === 1 && ch >= "a" && ch <= "z") {
            typed = ch.toUpperCase()
            shifted = false
        }
        text += typed
    }

    function doBackspace() {
        if (text.length > 0)
            text = text.substring(0, text.length - 1)
    }

    // Clamp focusCol when switching rows
    onFocusRowChanged: {
        var row = currentRows[focusRow]
        if (!row) return
        // Bottom row has fewer active cells
        var maxCol = (focusRow === currentRows.length - 1) ? bottomRowCells - 1 : row.length - 1
        if (focusCol > maxCol)
            focusCol = maxCol
    }

    // ── Controller Start (done) and R2 (shift) via signals ──
    Connections {
        target: inputManager
        enabled: root.visible

        function onNavigateStart() {
            root.accepted()
            root.close()
        }
        function onNavigateShift() {
            root.shifted = !root.shifted
        }
    }

    // ── All input (keyboard + controller) via Keys.onPressed ──
    Keys.onPressed: function(event) {
        if (!visible) return

        if (event.key === Qt.Key_Up) {
            if (focusRow > 0) focusRow--
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            if (focusRow < currentRows.length - 1) focusRow++
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            if (focusCol > 0) focusCol--
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            var maxCol = (focusRow === currentRows.length - 1)
                ? bottomRowCells - 1
                : currentRows[focusRow].length - 1
            if (focusCol < maxCol) focusCol++
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            var row = currentRows[focusRow]
            if (row) handleKeyPress(row[focusCol])
            event.accepted = true
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            text = _initialText
            cancelled()
            close()
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            doBackspace()
            event.accepted = true
        } else if (event.text.length > 0 && event.text.charCodeAt(0) >= 32) {
            text += event.text
            event.accepted = true
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.text = root._initialText
            root.cancelled()
            root.close()
        }
    }

    // ── Visual layout ──
    Rectangle {
        id: keyboardPanel
        anchors.centerIn: parent
        width: 440
        height: contentCol.implicitHeight + 40
        radius: 12
        color: SettingsTheme.surface
        border.width: 1
        border.color: SettingsTheme.border

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: contentCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 12

            // Field label
            Text {
                text: root.label
                color: SettingsTheme.textMuted
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
                Layout.alignment: Qt.AlignHCenter
            }

            // Text preview + password toggle
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    radius: 6
                    color: SettingsTheme.card
                    border.width: 2
                    border.color: SettingsTheme.focusBorder

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: {
                            var display = (root.isPassword && !root.showPassword)
                                ? "\u2022".repeat(root.text.length)
                                : root.text
                            return display + "\u2502"
                        }
                        color: SettingsTheme.text
                        font.pixelSize: 16
                        elide: Text.ElideLeft
                    }
                }

                Rectangle {
                    visible: root.isPassword
                    width: 40
                    height: 40
                    radius: 6
                    color: eyeMa.containsMouse ? Qt.lighter(SettingsTheme.card, 1.2) : SettingsTheme.card
                    border.width: 1
                    border.color: SettingsTheme.border

                    Text {
                        anchors.centerIn: parent
                        text: root.showPassword ? "\uD83D\uDC41" : "\uD83D\uDC41\u200D\uD83D\uDDE8"
                        font.pixelSize: 16
                    }

                    MouseArea {
                        id: eyeMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showPassword = !root.showPassword
                    }
                }
            }

            // Shift indicator
            Text {
                visible: root.shifted
                text: "SHIFT"
                color: SettingsTheme.accent
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 2.0
                Layout.alignment: Qt.AlignHCenter
            }

            // Uniform keyboard grid
            Column {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4

                Repeater {
                    model: root.currentRows.length

                    Row {
                        id: keyRow
                        spacing: 4
                        property int rowIndex: index

                        // Center the bottom row
                        anchors.horizontalCenter: parent.horizontalCenter

                        Repeater {
                            model: root.currentRows[keyRow.rowIndex].length

                            Rectangle {
                                id: keyRect
                                property string keyChar: root.currentRows[keyRow.rowIndex][index]
                                property bool isEmpty: keyChar === ""
                                property bool isFocused: root.focusRow === keyRow.rowIndex && root.focusCol === index
                                property bool isSpecial: keyChar === "123" || keyChar === "abc" || keyChar === "\u2713"
                                property bool isDone: keyChar === "\u2713"
                                property bool isSpace: keyChar === " "

                                visible: !isEmpty
                                width: isSpace ? 200 : (isSpecial ? 80 : 36)
                                height: 36
                                radius: 4
                                color: {
                                    if (isDone) return isFocused ? Qt.lighter(SettingsTheme.accent, 1.1) : SettingsTheme.accent
                                    if (isFocused) return Qt.lighter(SettingsTheme.card, 1.4)
                                    if (isSpecial) return Qt.lighter(SettingsTheme.card, 1.1)
                                    return SettingsTheme.card
                                }
                                border.width: isFocused ? 2 : 1
                                border.color: isFocused ? SettingsTheme.focusBorder : SettingsTheme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: root.getDisplayChar(keyRect.keyChar)
                                    color: {
                                        if (keyRect.isDone) return SettingsTheme.background
                                        if (keyRect.isSpecial) return SettingsTheme.accent
                                        return SettingsTheme.text
                                    }
                                    font.pixelSize: 14
                                    font.weight: keyRect.isSpecial ? Font.DemiBold : Font.Normal
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.handleKeyPress(keyRect.keyChar)
                                }

                                // Focus glow
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -3
                                    radius: parent.radius + 3
                                    color: "transparent"
                                    border.width: 2
                                    border.color: SettingsTheme.focusBorder
                                    opacity: keyRect.isFocused ? 0.3 : 0
                                    z: -1
                                    visible: opacity > 0
                                    Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                                }
                            }
                        }
                    }
                }
            }

            // Controller button hints
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 16

                Text { text: "\uD83C\uDD70 Type"; color: SettingsTheme.textGhost; font.pixelSize: 10 }
                Text { text: "\uD83C\uDD71 Cancel"; color: SettingsTheme.textGhost; font.pixelSize: 10 }
                Text { text: "\u2327 Delete"; color: SettingsTheme.textGhost; font.pixelSize: 10 }
                Text { text: "R2 Shift"; color: SettingsTheme.textGhost; font.pixelSize: 10 }
                Text { text: "Start Done"; color: SettingsTheme.textGhost; font.pixelSize: 10 }
            }
        }
    }
}
