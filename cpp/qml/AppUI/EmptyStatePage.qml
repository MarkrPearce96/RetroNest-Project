import QtQuick
import QtQuick.Controls

Item {
    id: root
    focus: true

    Component.onCompleted: root.forceActiveFocus()
    StackView.onActivated: root.forceActiveFocus()

    property int focusIndex: 0  // 0 = Open ROM Folder, 1 = Scan, 2 = Import
    property var hints: [{action: "start", label: "Settings"}]

    // Keyboard/controller navigation
    Keys.onLeftPressed: {
        if (root.focusIndex === 2) root.focusIndex = 1
    }
    Keys.onRightPressed: {
        if (root.focusIndex === 1) root.focusIndex = 2
    }
    Keys.onUpPressed: {
        if (root.focusIndex > 0) root.focusIndex = 0
    }
    Keys.onDownPressed: {
        if (root.focusIndex === 0) root.focusIndex = 1
    }
    Keys.onReturnPressed: root.activateButton()
    Keys.onEnterPressed: root.activateButton()

    function activateButton() {
        if (focusIndex === 0) app.openRomFolder()
        else if (focusIndex === 1) app.scanRomFolders()
        else if (focusIndex === 2) app.importRoms()
    }

    // Background image
    Image {
        anchors.fill: parent
        source: "images/empty-state-bg.webp"
        fillMode: Image.PreserveAspectCrop
    }

    // Dark scrim overlay
    Rectangle {
        anchors.fill: parent
        color: "#1a1a2e"
        opacity: 0.85
    }

    // Content
    Column {
        anchors.centerIn: parent
        spacing: 24

        // Title
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "No games found"
            color: "#ffffff"
            font.pixelSize: 26
            font.weight: Font.Bold
        }

        // Instructions
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Add ROMs to your system folders, then scan to discover them.\nUse Open ROM Folder to find the right directory."
            color: Qt.rgba(1, 1, 1, 0.7)
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.6
        }

        // Primary button: Open ROM Folder
        Rectangle {
            id: openFolderBtn
            anchors.horizontalCenter: parent.horizontalCenter
            width: openFolderText.implicitWidth + 48
            height: 50
            radius: 10
            color: openFolderMa.containsMouse || root.focusIndex === 0
                   ? "#6b6be6" : "#3d3d8a"
            Behavior on color { ColorAnimation { duration: 100 } }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                radius: parent.radius + 4
                color: "transparent"
                border.width: 2
                border.color: "#5b5bd6"
                opacity: root.focusIndex === 0 ? 0.4 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }

            Text {
                id: openFolderText
                anchors.centerIn: parent
                text: "Open ROM Folder"
                color: "#ffffff"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: openFolderMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: app.openRomFolder()
            }
        }

        // Secondary buttons row
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            // Scan for Games
            Rectangle {
                id: scanBtn
                width: scanText.implicitWidth + 40
                height: 46
                radius: 10
                color: scanMa.containsMouse || root.focusIndex === 1
                       ? "#363660" : Qt.rgba(0.17, 0.17, 0.31, 0.8)
                border.color: Qt.rgba(0.36, 0.36, 0.84, 0.35)
                border.width: 1
                Behavior on color { ColorAnimation { duration: 100 } }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: parent.radius + 4
                    color: "transparent"
                    border.width: 2
                    border.color: "#5b5bd6"
                    opacity: root.focusIndex === 1 ? 0.4 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                Text {
                    id: scanText
                    anchors.centerIn: parent
                    text: "Scan for Games"
                    color: Qt.rgba(1, 1, 1, 0.75)
                    font.pixelSize: 14
                }
                MouseArea {
                    id: scanMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: app.scanRomFolders()
                }
            }

            // Import ROMs
            Rectangle {
                id: importBtn
                width: importText.implicitWidth + 40
                height: 46
                radius: 10
                color: importMa.containsMouse || root.focusIndex === 2
                       ? "#363660" : Qt.rgba(0.17, 0.17, 0.31, 0.8)
                border.color: Qt.rgba(0.36, 0.36, 0.84, 0.35)
                border.width: 1
                Behavior on color { ColorAnimation { duration: 100 } }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: parent.radius + 4
                    color: "transparent"
                    border.width: 2
                    border.color: "#5b5bd6"
                    opacity: root.focusIndex === 2 ? 0.4 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                Text {
                    id: importText
                    anchors.centerIn: parent
                    text: "Import ROMs"
                    color: Qt.rgba(1, 1, 1, 0.75)
                    font.pixelSize: 14
                }
                MouseArea {
                    id: importMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: app.importRoms()
                }
            }
        }

        // Navigation hint
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "↑ ↓ Navigate    Enter Select"
            color: Qt.rgba(1, 1, 1, 0.35)
            font.pixelSize: 11
        }
    }
}
