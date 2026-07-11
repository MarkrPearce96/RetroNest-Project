import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // Optional ROMs/BIOS section starts expanded — matches approved mockup.
    property bool expanded: true

    focus: true

    function browseDataFolder() {
        var p = wizard.browseFolder("Choose Data Folder")
        if (p) wizard.rootPath = p + "/RetroNest"
    }
    function browseRomsFolder() {
        var p = wizard.browseFolder("Choose ROMs folder")
        if (p) wizard.romsRoot = p
    }
    function browseBiosFolder() {
        var p = wizard.browseFolder("Choose BIOS folder")
        if (p) wizard.biosRoot = p
    }

    Keys.onReturnPressed: root.browseDataFolder()
    Keys.onEnterPressed: root.browseDataFolder()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        spacing: 0

        Text {
            text: "Where should we\nkeep everything?"
            color: WizardTheme.textPrimary
            font.pixelSize: 40
            font.weight: Font.ExtraBold
            font.letterSpacing: -1.2
            lineHeight: 1.04
            lineHeightMode: Text.ProportionalHeight
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Text {
            text: "RetroNest keeps your library, saves, and artwork here. You can put your games on a USB drive if you like."
            color: WizardTheme.textSecondary
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.maximumWidth: 700
            Layout.topMargin: 14
            Layout.bottomMargin: 30
        }

        // Data folder (always visible)
        GlassField {
            Layout.fillWidth: true
            labelText: "Data folder"
            iconText: "📁" // 📁
            valueText: wizard.rootPath
            placeholderText: "Choose where RetroNest keeps your library…"
            onBrowseRequested: root.browseDataFolder()
        }

        // Collapsible "Customize storage locations" section
        Item {
            Layout.fillWidth: true
            Layout.topMargin: 32
            implicitHeight: advHeaderRow.implicitHeight

            RowLayout {
                id: advHeaderRow
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 13

                Text {
                    text: root.expanded ? "▾" : "▸"
                    color: WizardTheme.textDim
                    font.pixelSize: 14
                }
                Text {
                    text: "Customize storage locations (optional)"
                    color: WizardTheme.textDim
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 24
            spacing: 28
            visible: root.expanded

            GlassField {
                Layout.fillWidth: true
                labelText: "ROMs folder"
                iconText: "🎮" // 🎮
                valueText: wizard.romsRoot
                hintText: "Default: inside your data folder. Point to a USB to keep games there."
                onBrowseRequested: root.browseRomsFolder()
            }

            GlassField {
                Layout.fillWidth: true
                labelText: "BIOS folder"
                iconText: "🧩" // 🧩
                valueText: wizard.biosRoot
                hintText: "Already have a BIOS folder? Point at it and we'll use it in place."
                onBrowseRequested: root.browseBiosFolder()
            }
        }

        Item { Layout.fillHeight: true }

        // "We'll create a folder for every console" note
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 28
            radius: 16
            color: WizardTheme.surface
            border.width: 1
            border.color: WizardTheme.divider
            implicitHeight: noteRow.implicitHeight + 36

            RowLayout {
                id: noteRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 12

                Text {
                    text: "✨" // ✨
                    font.pixelSize: 16
                }
                Text {
                    text: "We'll create a correctly-named folder for <b>every console</b> — just drop your games in."
                    textFormat: Text.StyledText
                    color: WizardTheme.textSecondary
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }

    // ========== COMPONENTS ==========

    // A labeled glass path field + Browse button, with an optional hint line.
    component GlassField: ColumnLayout {
        id: field
        property string labelText: ""
        property string iconText: ""
        property string valueText: ""
        property string placeholderText: ""
        property string hintText: ""
        signal browseRequested()

        spacing: 11

        Text {
            text: field.labelText
            color: WizardTheme.textDim
            font.pixelSize: 13
            font.letterSpacing: 2
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                radius: 16
                color: WizardTheme.surface
                border.width: 1
                border.color: WizardTheme.surfaceBorder

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    spacing: 13

                    Text {
                        text: field.iconText
                        font.pixelSize: 18
                        opacity: 0.85
                        color: WizardTheme.textPrimary
                    }
                    Text {
                        text: field.valueText || field.placeholderText
                        color: field.valueText ? WizardTheme.textPrimary : WizardTheme.textMuted
                        font.pixelSize: 15
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 120
                Layout.preferredHeight: 56
                radius: 16
                color: browseMa.containsMouse ? WizardTheme.surfaceHover : WizardTheme.divider
                border.width: 1
                border.color: WizardTheme.surfaceBorder

                Text {
                    anchors.centerIn: parent
                    text: "Browse…"
                    color: WizardTheme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: browseMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: field.browseRequested()
                }

                Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
            }
        }

        Text {
            visible: field.hintText !== ""
            text: field.hintText
            color: WizardTheme.textMuted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.topMargin: 2
        }
    }
}
