import QtQuick
import QtQuick.Controls

Item {
    id: root

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 4
        radius: 8
        color: "transparent"

        // Cover Image
        Item {
            id: coverContainer
            width: parent.width
            height: parent.width * 1.41  // ~box art ratio

            // Cover art (if available)
            Image {
                id: coverImg
                anchors.fill: parent
                source: coverPath ? "file://" + coverPath : ""
                fillMode: Image.PreserveAspectCrop
                visible: status === Image.Ready
                smooth: true
                mipmap: true

                layer.enabled: true
                layer.effect: Item {
                    // Rounded corners via OpacityMask alternative
                }
            }

            // Rounded clip
            Rectangle {
                id: coverMask
                anchors.fill: parent
                radius: 8
                visible: false
            }

            // Placeholder (when no cover)
            Rectangle {
                anchors.fill: parent
                radius: 8
                visible: coverImg.status !== Image.Ready
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#2a2a4e" }
                    GradientStop { position: 1.0; color: "#1a1a2e" }
                }
                border.width: 1
                border.color: "#3a3a5e"

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    // System badge
                    Rectangle {
                        width: 48
                        height: 20
                        radius: 10
                        color: "#3a3a5e"
                        anchors.horizontalCenter: parent.horizontalCenter

                        Text {
                            anchors.centerIn: parent
                            text: system.toUpperCase()
                            color: Theme.textMuted
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }

                    // Disc icon
                    Rectangle {
                        width: 56
                        height: 56
                        radius: 28
                        color: "#303050"
                        anchors.horizontalCenter: parent.horizontalCenter

                        Rectangle {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            radius: 10
                            color: "#3a3a5e"
                        }
                    }
                }

                // Title at bottom of placeholder
                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 20
                    text: title
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
            }

            // Cover with rounded corners (when image loaded)
            Rectangle {
                anchors.fill: parent
                radius: 8
                color: "transparent"
                border.width: 1
                border.color: mouseArea.containsMouse ? Theme.accent : "transparent"
                visible: coverImg.status === Image.Ready

                Behavior on border.color { ColorAnimation { duration: 100 } }

                Image {
                    anchors.fill: parent
                    anchors.margins: 0
                    source: coverPath ? "file://" + coverPath : ""
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true

                    // Clip to rounded rect
                    layer.enabled: true
                    layer.smooth: true
                }
            }
        }

        // Title below cover
        Text {
            anchors.top: coverContainer.bottom
            anchors.topMargin: 6
            anchors.left: parent.left
            anchors.right: parent.right
            text: title
            color: Theme.textSecondary
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            visible: coverImg.status === Image.Ready
        }

        // Hover + click
        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onDoubleClicked: function(mouse) {
                if (mouse.button === Qt.LeftButton)
                    app.launchGame(gameId, romPath, emulatorId)
            }

            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    contextMenu.popup()
                }
            }
        }

        Menu {
            id: contextMenu

            background: Rectangle {
                color: Theme.surface
                border.width: 1
                border.color: Theme.divider
                radius: 6
            }

            MenuItem {
                text: "Scrape Cover Art"
                onTriggered: {
                    app.scrapeGame(gameId)
                    gameModel.reload()
                }

                background: Rectangle {
                    color: parent.highlighted ? Theme.surfaceHover : "transparent"
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: Theme.textPrimary
                    font.pixelSize: 13
                }
            }

            MenuItem {
                text: "Remove from Library"
                onTriggered: {
                    app.removeGame(gameId)
                }

                background: Rectangle {
                    color: parent.highlighted ? Theme.surfaceHover : "transparent"
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: Theme.textPrimary
                    font.pixelSize: 13
                }
            }
        }

        // Hover scale
        scale: mouseArea.containsMouse ? 1.03 : 1.0
        Behavior on scale { NumberAnimation { duration: 100 } }
    }
}
