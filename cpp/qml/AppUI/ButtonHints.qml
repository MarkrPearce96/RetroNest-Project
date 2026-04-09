import QtQuick
import QtQuick.Layouts

Item {
    id: root
    height: 40
    implicitWidth: hintsRow.width

    // Pass hints as array of {action: "confirm", label: "Select"} objects
    // Valid actions: confirm, back, action, delete, navigate_lr, navigate_ud, start
    // Optional: keyboardKey overrides the keyboard glyph text (e.g. {action: "back", label: "Back", keyboardKey: "Backspace"})
    property var hints: []

    // 0 = Keyboard, 1 = Xbox, 2 = PlayStation
    property int inputType: inputManager.controllerType

    // Glyph definitions per action per input type
    // Each entry: { text, bg, fg, border }
    function glyphFor(action) {
        if (inputType === 2) {
            // PlayStation
            switch (action) {
            case "confirm":     return { text: "\u2715", bg: "#2a3a6a", fg: "#6d9ddc", border: "#3a5a8a", size: 18 }
            case "back":        return { text: "\u25CB", bg: "#5c2a3a", fg: "#dc6d8d", border: "#7a3a5a", size: 18 }
            case "action":      return { text: "\u25B3", bg: "#2a5c5c", fg: "#6ddcb0", border: "#3a7a6a", size: 18 }
            case "delete":      return { text: "\u25A1", bg: "#5c2a5c", fg: "#dc6ddc", border: "#7a3a7a", size: 18 }
            case "navigate_lr": return { text: "D-Pad \u25C2\u25B8", bg: "#333333", fg: "#cccccc", border: "#555555", size: 16 }
            case "navigate_ud": return { text: "D-Pad \u25B4\u25BE", bg: "#333333", fg: "#cccccc", border: "#555555", size: 16 }
            case "start":       return { text: "Start", bg: "#333333", fg: "#cccccc", border: "#555555" }
            default:            return { text: "?", bg: "#333333", fg: "#cccccc", border: "#555555" }
            }
        } else if (inputType === 1) {
            // Xbox
            switch (action) {
            case "confirm":     return { text: "A", bg: "#2a5c2a", fg: "#6ddc6d", border: "#3a7a3a" }
            case "back":        return { text: "B", bg: "#5c2a2a", fg: "#dc6d6d", border: "#7a3a3a" }
            case "action":      return { text: "Y", bg: "#5c5c2a", fg: "#dcdc6d", border: "#7a7a3a" }
            case "delete":      return { text: "X", bg: "#2a3a6a", fg: "#6d9ddc", border: "#3a5a8a" }
            case "navigate_lr": return { text: "D-Pad \u25C2\u25B8", bg: "#333333", fg: "#cccccc", border: "#555555", size: 16 }
            case "navigate_ud": return { text: "D-Pad \u25B4\u25BE", bg: "#333333", fg: "#cccccc", border: "#555555", size: 16 }
            case "start":       return { text: "Start", bg: "#333333", fg: "#cccccc", border: "#555555" }
            default:            return { text: "?", bg: "#333333", fg: "#cccccc", border: "#555555" }
            }
        } else {
            // Keyboard
            switch (action) {
            case "confirm":     return { text: "Enter", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "back":        return { text: "Esc", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "action":      return { text: "M", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "delete":      return { text: "Backspace", bg: "#333333", fg: "#cccccc", border: "#555555" }
            case "navigate_lr": return { text: "\u2190\u2192", bg: "#333333", fg: "#cccccc", border: "#555555", size: 18 }
            case "navigate_ud": return { text: "\u2191\u2193", bg: "#333333", fg: "#cccccc", border: "#555555", size: 18 }
            case "start":       return { text: "Esc", bg: "#333333", fg: "#cccccc", border: "#555555" }
            default:            return { text: "?", bg: "#333333", fg: "#cccccc", border: "#555555" }
            }
        }
    }

    Row {
        id: hintsRow
        anchors.centerIn: parent
        spacing: 20

        Repeater {
            model: root.hints

            Row {
                id: hintDelegate
                spacing: 5
                property var glyph: {
                    var g = root.glyphFor(modelData.action)
                    if (root.inputType === 0 && modelData.keyboardKey)
                        return { text: modelData.keyboardKey, bg: g.bg, fg: g.fg, border: g.border }
                    return g
                }

                Rectangle {
                    width: glyphText.implicitWidth + 16
                    height: 28
                    radius: 5
                    color: hintDelegate.glyph.bg
                    border.color: hintDelegate.glyph.border
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: glyphText
                        anchors.centerIn: parent
                        text: hintDelegate.glyph.text
                        color: hintDelegate.glyph.fg
                        font.pixelSize: hintDelegate.glyph.size || 14
                        font.weight: Font.Bold
                    }
                }

                Text {
                    text: modelData.label
                    color: "#dddddd"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    anchors.verticalCenter: parent.verticalCenter
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.5)
                }
            }
        }
    }
}
