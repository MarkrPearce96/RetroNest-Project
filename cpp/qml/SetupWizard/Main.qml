import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 900; height: 650
    minimumWidth: 720; minimumHeight: 540
    title: "RetroNest Setup"
    color: "#131210"

    property int pageCount: 7

    // Dark gradient background
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#252320" }
            GradientStop { position: 1.0; color: "#131210" }
        }
    }

    // Centered card
    Rectangle {
        id: wizardCard
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.85, 800)
        height: Math.min(parent.height * 0.85, 550)
        radius: 14
        color: WizardTheme.surface
        border.width: 1
        border.color: WizardTheme.divider

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24; Layout.rightMargin: 24
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
                Layout.leftMargin: 24; Layout.rightMargin: 24
                height: 2; radius: 1
                color: WizardTheme.divider

                Rectangle {
                    width: parent.width * ((swipeView.currentIndex + 1) / root.pageCount)
                    height: parent.height; radius: 1
                    color: WizardTheme.accent
                    Behavior on width { NumberAnimation { duration: WizardTheme.animNormal } }
                }
            }

            // SwipeView — 7 pages
            SwipeView {
                id: swipeView
                Layout.fillWidth: true; Layout.fillHeight: true
                interactive: false; clip: true

                WelcomePage { isCurrentPage: SwipeView.isCurrentItem }
                FolderPage {}
                EmulatorsPage { id: emulatorsPage }
                ResolutionPage { id: resolutionPage }
                AspectRatioPage { id: aspectRatioPage }
                FilesPage { id: filesPage }
                InstallPage { id: installPage; isCurrentPage: SwipeView.isCurrentItem }
            }

            // NavBar
            NavBar {
                Layout.fillWidth: true
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

                    if (cur === 2) {
                        resolutionPage.refresh()
                        aspectRatioPage.refresh()
                    }
                    // Refresh Files page when about to enter it
                    if (cur === 4) {
                        filesPage.refresh()
                        wizard.ensureRomDirs(emulators.availableSystems())
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
    }

    function pageTitleForIndex(index) {
        var titles = ["Welcome", "Choose Data Folder", "Select Emulators",
                      "Display Resolution", "Aspect Ratio", "Files", "Installing"]
        return titles[index] || ""
    }
}
