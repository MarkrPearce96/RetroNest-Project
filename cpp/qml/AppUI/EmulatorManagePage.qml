import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string selectedEmuId: ""

    // SettingsOverlay.goBack() consults these before popping the stack —
    // the grid↔detail drill-down is page-internal state on one stack
    // entry, so keyboard Esc (routed through the app-level Shortcut →
    // handleBack → goBack) must step detail→grid here instead of popping
    // straight to the category list. Controller B already worked: the
    // Back shortcut is gated on panelOpen, so Key_Back reaches
    // EmulatorDetailPage's own Keys handler → back() → grid.
    readonly property bool canGoBackInternal: selectedEmuId !== ""
    function goBackInternal() { selectedEmuId = "" }

    StackLayout {
        anchors.fill: parent
        currentIndex: selectedEmuId === "" ? 0 : 1

        EmulatorManageGrid {
            id: emuGrid
            onEmulatorSelected: function(emuId) {
                root.selectedEmuId = emuId
            }
        }

        EmulatorDetailPage {
            id: detailPage
            emuId: root.selectedEmuId
            onBack: root.selectedEmuId = ""
        }
    }

    // Focus whichever child is currently active
    function _focusActiveChild() {
        if (selectedEmuId === "")
            emuGrid.forceActiveFocus()
        else
            detailPage.forceActiveFocus()
    }

    // Propagate focus down to the active child
    onFocusChanged: if (focus || activeFocus) _focusActiveChild()
    onVisibleChanged: if (visible) _focusActiveChild()
    onSelectedEmuIdChanged: _focusActiveChild()
}
