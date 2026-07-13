import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ConfirmDialog — a modal "are you sure?" prompt with a keyboard/controller-
// navigable Cancel/Confirm button pair. Cancel is focused first (the safe
// default) so a controller user mashing Enter can't accidentally trigger a
// destructive action; Left/Right move between the two buttons, Enter/Space
// activates the focused one, Esc (or clicking outside) cancels.
//
// This replaces the hand-rolled confirm dialogs whose Confirm button was
// reachable only by mouse (uninstall, reset-config, exit). Callers set the
// copy and wire onConfirmed; the component owns all of the navigation.
//
//   ConfirmDialog {
//       id: fooDialog
//       title: "Do the thing?"
//       message: "This cannot be undone."
//       confirmText: "Do it"
//       destructive: true        // red Confirm for irreversible actions
//       onConfirmed: app.doTheThing()
//   }
//   ...
//   fooDialog.open()
Popup {
    id: dialog

    // ── Public API ───────────────────────────────────────────────────────
    property string title: ""
    property string message: ""
    property string cancelText: "Cancel"
    property string confirmText: "Confirm"
    property bool destructive: false     // red Confirm vs accent
    signal confirmed()
    signal cancelled()

    // ── Internal focus state: 0 = Cancel (safe default), 1 = Confirm ───────
    property int _focus: 0
    property bool _confirmed: false

    modal: true
    dim: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    width: 360
    padding: 24

    background: Rectangle {
        radius: 12
        color: SettingsTheme.surface
        border.width: 1
        border.color: SettingsTheme.border
    }

    // Reset to the safe default on every open; emit cancelled() for any close
    // that wasn't an explicit confirm (Esc, click-outside, Cancel button).
    onAboutToShow: { _confirmed = false; _focus = 0 }
    onClosed: if (!_confirmed) dialog.cancelled()

    function _confirm() { _confirmed = true; confirmed(); close() }
    function _cancel()  { close() }   // cancelled() fires from onClosed

    contentItem: FocusScope {
        focus: true
        implicitHeight: col.implicitHeight

        Keys.onPressed: function(event) {
            switch (event.key) {
            case Qt.Key_Left:  dialog._focus = 0; event.accepted = true; break
            case Qt.Key_Right: dialog._focus = 1; event.accepted = true; break
            case Qt.Key_Return:
            case Qt.Key_Enter:
            case Qt.Key_Space:
                if (dialog._focus === 1) dialog._confirm(); else dialog._cancel()
                event.accepted = true; break
            case Qt.Key_Escape:
            case Qt.Key_Back:
                dialog._cancel(); event.accepted = true; break
            }
        }

        ColumnLayout {
            id: col
            width: parent.width
            spacing: 16

            Text {
                text: dialog.title
                color: SettingsTheme.text
                font.pixelSize: 16
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text {
                visible: dialog.message.length > 0
                text: dialog.message
                color: SettingsTheme.textMuted
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 12

                Item { Layout.fillWidth: true }

                // Cancel button (focus index 0)
                Rectangle {
                    implicitWidth: 100
                    implicitHeight: 36
                    radius: SettingsTheme.buttonRadius
                    color: cancelMa.containsMouse ? Qt.lighter(SettingsTheme.card, 1.15)
                                                  : SettingsTheme.card
                    border.width: dialog._focus === 0 ? 2 : 1
                    border.color: dialog._focus === 0 ? SettingsTheme.focusBorder
                                                      : SettingsTheme.border

                    Behavior on border.color { ColorAnimation { duration: SettingsTheme.animFast } }

                    Text {
                        anchors.centerIn: parent
                        text: dialog.cancelText
                        color: SettingsTheme.text
                        font.pixelSize: 13
                    }

                    MouseArea {
                        id: cancelMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: dialog._focus = 0
                        onClicked: dialog._cancel()
                    }
                }

                // Confirm button (focus index 1)
                Rectangle {
                    implicitWidth: 120
                    implicitHeight: 36
                    radius: SettingsTheme.buttonRadius

                    property color baseColor: dialog.destructive ? SettingsTheme.error
                                                                 : SettingsTheme.accent
                    color: confirmMa.containsMouse ? Qt.lighter(baseColor, 1.12) : baseColor
                    border.width: dialog._focus === 1 ? 2 : 0
                    border.color: SettingsTheme.focusBorder

                    Behavior on border.width { NumberAnimation { duration: SettingsTheme.animFast } }

                    Text {
                        anchors.centerIn: parent
                        text: dialog.confirmText
                        // Destructive (red) bg reads best with light text; accent
                        // bg uses the dark background colour, matching the old
                        // hand-rolled dialogs.
                        color: dialog.destructive ? SettingsTheme.text : SettingsTheme.background
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: confirmMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: dialog._focus = 1
                        onClicked: dialog._confirm()
                    }
                }
            }
        }
    }
}
