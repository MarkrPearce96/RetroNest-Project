import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // System Sidebar
        SystemSidebar {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
        }

        // Main Area
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Toolbar
            Rectangle {
                Layout.fillWidth: true
                height: 56
                color: Theme.background

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.divider
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 12

                    Text {
                        text: "Games"
                        color: Theme.textPrimary
                        font.pixelSize: 20
                        font.weight: Font.Bold
                    }

                    Text {
                        text: gameModel.count + " games"
                        color: Theme.textDim
                        font.pixelSize: 12
                        Layout.leftMargin: 4
                    }

                    Item { Layout.fillWidth: true }

                    ToolBtn {
                        label: "Scan Games"
                        onClicked: {
                            app.scanRomFolders()
                            gameModel.reload()
                        }
                    }

                    ToolBtn {
                        label: "Import ROMs"
                        onClicked: {
                            app.importRoms()
                            gameModel.reload()
                        }
                    }

                    ToolBtn {
                        label: "Scrape"
                        onClicked: {
                            app.setSettingsCategory(3)
                            app.currentTab = 1
                        }
                    }
                }
            }

            // Game Grid
            GameGridView {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    Connections {
        target: app
        function onGamesChanged() { gameModel.reload() }
        function onCurrentSystemChanged() {
            if (app.currentSystem === "")
                gameModel.loadAll()
            else
                gameModel.loadBySystem(app.currentSystem)
        }
    }

    Component.onCompleted: gameModel.loadAll()
}
