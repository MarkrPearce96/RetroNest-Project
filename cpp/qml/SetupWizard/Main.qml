import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1180; height: 720
    minimumWidth: 960; minimumHeight: 620
    title: "RetroNest Setup"
    color: WizardTheme.background

    property int pageCount: 6

    // B2 "Sunset premium" gradient backdrop — full-bleed, no inner card.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: WizardTheme.gradTop }
            GradientStop { position: 0.5; color: WizardTheme.gradMid }
            GradientStop { position: 1.0; color: WizardTheme.gradBottom }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: WizardTheme.pageMargin; Layout.rightMargin: WizardTheme.pageMargin
            Layout.topMargin: 20; Layout.bottomMargin: 8

            Text {
                text: pageTitleForIndex(swipeView.currentIndex)
                font.pixelSize: 18; font.weight: Font.DemiBold
                color: WizardTheme.textPrimary
                Layout.fillWidth: true
            }

            Text {
                text: (swipeView.currentIndex + 1) + " / " + pageCount
                font.pixelSize: 12
                color: WizardTheme.textDim
            }
        }

        // Progress bar (replaces StepIndicator dots)
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: WizardTheme.pageMargin; Layout.rightMargin: WizardTheme.pageMargin
            height: 2; radius: 1
            color: WizardTheme.divider

            Rectangle {
                width: parent.width * ((swipeView.currentIndex + 1) / root.pageCount)
                height: parent.height; radius: 1
                color: WizardTheme.accent
                Behavior on width { NumberAnimation { duration: WizardTheme.animNormal } }
            }
        }

        // SwipeView — 6 pages
        SwipeView {
            id: swipeView
            Layout.fillWidth: true; Layout.fillHeight: true
            interactive: false; clip: true

            WelcomePage {
                isCurrentPage: SwipeView.isCurrentItem
                onGetStartedClicked: swipeView.incrementCurrentIndex()
            }
            StorageLocationsPage { id: storagePage }
            EmulatorsPage { id: emulatorsPage }
            RetroAchievementsPage { id: raPage }
            ScreenScraperPage { id: scraperPage }
            InstallPage { id: installPage; isCurrentPage: SwipeView.isCurrentItem }
        }

        // NavBar
        NavBar {
            Layout.fillWidth: true
            visible: swipeView.currentIndex !== 0
            currentIndex: swipeView.currentIndex
            pageCount: root.pageCount
            canContinue: {
                if (swipeView.currentIndex === 1) return wizard.rootPath !== ""
                return true
            }
            onBackClicked: swipeView.currentIndex--
            onContinueClicked: {
                var cur = swipeView.currentIndex
                if (cur === 1 && !wizard.rootPath) return

                // Leaving the Storage step: commit the chosen root now so the
                // RetroAchievements/ScreenScraper steps can save credentials
                // into {root}/config/. Idempotent — safe to call again on accept().
                if (cur === 1) {
                    wizard.applyStorageLocations()
                }

                if (cur < pageCount - 1) {
                    swipeView.currentIndex = cur + 1
                }
                if (swipeView.currentIndex === pageCount - 1) {
                    installPage.startInstall()
                }
            }
        }
    }

    function pageTitleForIndex(index) {
        var titles = ["Welcome", "Storage Locations", "Select Emulators",
                      "RetroAchievements", "ScreenScraper", "Install"]
        return titles[index] || ""
    }
}
