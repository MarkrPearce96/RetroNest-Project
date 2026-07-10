import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root

    property bool isCurrentPage: false

    focus: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        spacing: 0

        Item { Layout.fillHeight: true }

        Text {
            text: "WELCOME TO RETRONEST"
            color: WizardTheme.textDim
            font.pixelSize: 13
            font.letterSpacing: 3
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase

            opacity: root.isCurrentPage ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animSlow } }
        }

        Text {
            text: "Your all-in-one\nretro gaming setup"
            color: WizardTheme.textPrimary
            font.pixelSize: 48
            font.weight: Font.ExtraBold
            font.letterSpacing: -1.4
            lineHeight: 1.04
            lineHeightMode: Text.ProportionalHeight
            Layout.fillWidth: true
            Layout.topMargin: 16
            wrapMode: Text.WordWrap

            opacity: root.isCurrentPage ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animSlow } }
        }

        Text {
            text: "Let's get everything configured in a few quick steps — storage, emulators, achievements, and box art."
            color: WizardTheme.textSecondary
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.topMargin: 16
            Layout.bottomMargin: 40

            opacity: root.isCurrentPage ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animSlow; easing.type: Easing.OutCubic } }
        }

        // Hero CTA — mirrors the persistent NavBar "Get Started" pill for a
        // stronger landing moment. Advancing to the next step is owned by
        // Main.qml/NavBar's Continue button (same no-op pattern as the Skip
        // pills on RetroAchievementsPage/ScreenScraperPage) — this pill is
        // a visual echo, not a second navigation path.
        Rectangle {
            id: getStartedPill
            Layout.preferredWidth: 220
            Layout.preferredHeight: WizardTheme.pillHeight
            radius: WizardTheme.pillRadius
            color: WizardTheme.ctaBg

            opacity: root.isCurrentPage ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animSlow } }

            scale: getStartedMa.pressed ? 0.97 : 1.0
            Behavior on scale { NumberAnimation { duration: 100 } }

            Text {
                anchors.centerIn: parent
                text: "Get Started"
                color: WizardTheme.ctaText
                font.pixelSize: 15
                font.weight: Font.Bold
            }

            MouseArea {
                id: getStartedMa
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {} // no-op — see comment above
            }
        }

        Item { Layout.fillHeight: true }
    }
}
