import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string selectedEmuId: ""

    Connections {
        target: settingsOverlay
        function onTargetEmuIdChanged() {
            if (settingsOverlay.targetEmuId !== "") {
                root.selectedEmuId = settingsOverlay.targetEmuId
                settingsOverlay.targetEmuId = ""
            }
        }
    }

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
