import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: WizardTheme.navHeight
    color: WizardTheme.navBackground

    property int currentIndex: 0
    property int pageCount: 7
    property bool canContinue: true

    signal backClicked()
    signal continueClicked()

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: WizardTheme.divider
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24

        // Controller hints (left side)
        Row {
            spacing: 16
            Text {
                text: "L1/R1 Page"
                color: "#4a4640"
                font.pixelSize: 10
                visible: root.currentIndex > 0 && root.currentIndex < root.pageCount - 1
            }
            Text {
                text: "\u{1F170} Select"
                color: "#4a4640"
                font.pixelSize: 10
            }
            Text {
                text: "\u{1F171} Back"
                color: "#4a4640"
                font.pixelSize: 10
                visible: root.currentIndex > 0
            }
        }

        Item { Layout.fillWidth: true }

        // Back button
        Rectangle {
            visible: root.currentIndex > 0 && root.currentIndex < root.pageCount - 1
            width: 100; height: 36; radius: 6
            color: backMa.containsMouse ? WizardTheme.surfaceHover : WizardTheme.surface

            Text {
                anchors.centerIn: parent
                text: "Back"
                color: WizardTheme.textMuted
                font.pixelSize: 13
            }
            MouseArea {
                id: backMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.backClicked()
            }

            Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
        }

        // Continue button
        Rectangle {
            visible: root.currentIndex < root.pageCount - 1
            width: 140; height: 36; radius: 6
            opacity: root.canContinue ? 1.0 : 0.5
            color: contMa.containsMouse && root.canContinue ? WizardTheme.accentLight : WizardTheme.accent

            Text {
                anchors.centerIn: parent
                text: root.currentIndex === 0 ? "Get Started"
                    : (root.currentIndex === root.pageCount - 2 ? "Finish" : "Continue")
                color: WizardTheme.background
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: contMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: root.canContinue ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (root.canContinue) root.continueClicked()
            }

            Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
        }
    }

    // L1/R1 key handling for page navigation
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_PageUp) {
            if (root.currentIndex > 0 && root.currentIndex < root.pageCount - 1)
                root.backClicked()
            event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            if (root.currentIndex < root.pageCount - 1 && root.canContinue)
                root.continueClicked()
            event.accepted = true
        }
    }
}
