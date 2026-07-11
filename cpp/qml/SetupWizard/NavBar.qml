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
        anchors.leftMargin: WizardTheme.pageMargin
        anchors.rightMargin: WizardTheme.pageMargin
        spacing: 20

        // Setup wizard is mouse + keyboard only — no controller hints.
        Item { Layout.fillWidth: true }

        // Back — ghost pill (transparent, outline only)
        Rectangle {
            id: backPill
            visible: root.currentIndex > 0 && root.currentIndex < root.pageCount - 1
            width: 110
            height: WizardTheme.pillHeight
            radius: WizardTheme.pillRadius
            color: backMa.containsMouse ? WizardTheme.surfaceHover : "transparent"
            border.width: 1
            border.color: WizardTheme.surfaceBorder

            Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }

            Text {
                anchors.centerIn: parent
                text: "Back"
                color: WizardTheme.textSecondary
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: backMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.backClicked()
            }
        }

        // Continue — white pill CTA (label changes: Get Started / Continue / Finish)
        Rectangle {
            id: continuePill
            visible: root.currentIndex < root.pageCount - 1
            width: Math.max(150, continueLabel.implicitWidth + 56)
            height: WizardTheme.pillHeight
            radius: WizardTheme.pillRadius
            opacity: root.canContinue ? 1.0 : 0.5
            color: WizardTheme.ctaBg

            Behavior on opacity { NumberAnimation { duration: WizardTheme.animFast } }

            scale: contMa.pressed && root.canContinue ? 0.97 : 1.0
            Behavior on scale { NumberAnimation { duration: 100 } }

            Text {
                id: continueLabel
                anchors.centerIn: parent
                text: root.currentIndex === 0 ? "Get Started"
                    : (root.currentIndex === root.pageCount - 2 ? "Finish" : "Continue")
                color: WizardTheme.ctaText
                font.pixelSize: 15
                font.weight: Font.Bold
            }
            MouseArea {
                id: contMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: root.canContinue ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (root.canContinue) root.continueClicked()
            }
        }
    }

    // Page Up / Page Down keyboard navigation
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
