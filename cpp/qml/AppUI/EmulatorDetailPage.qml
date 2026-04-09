import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import "EmulatorLogos.js" as EmulatorLogos

Item {
    id: root
    focus: true

    property string emuId: ""
    signal back()

    property var emuList: app.allEmulatorStatus()
    property var emuInfo: {
        for (var i = 0; i < emuList.length; i++) {
            if (emuList[i].id === emuId) return emuList[i]
        }
        return { name: "", systems: "", installed: false, description: "",
                 biosRequired: false, biosDetected: false, version: "" }
    }
    property int focusIndex: 0

    // Whether the "Open BIOS Folder" button is visible
    property bool biosButtonVisible: root.emuInfo.installed && root.emuInfo.biosRequired && !root.emuInfo.biosDetected
    // Action buttons start after BIOS button (if visible)
    property int actionOffset: biosButtonVisible ? 1 : 0

    // ── Keyboard / controller navigation ────────────────────────────────
    Keys.onPressed: function(event) {
        var maxIndex = root.emuInfo.installed ? (actionOffset + 5) : 0

        if (event.key === Qt.Key_Up) {
            root.focusIndex = Math.max(0, root.focusIndex - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            root.focusIndex = Math.min(maxIndex, root.focusIndex + 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            activateFocused()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            root.back()
            event.accepted = true
        }
    }

    function activateFocused() {
        if (!root.emuInfo.installed) {
            if (root.focusIndex === 0) installBtn.clicked()
            return
        }
        // BIOS button at index 0 when visible
        if (biosButtonVisible && root.focusIndex === 0) {
            app.openBiosFolder()
            return
        }
        var idx = root.focusIndex - actionOffset
        switch (idx) {
        case 0: app.showEmulatorSettings(root.emuId); break
        case 1: app.showControllerMapping(root.emuId); break
        case 2: app.showHotkeySettings(root.emuId); break
        case 3: root.beginInstall(); break
        case 4: resetDialog.open(); break
        case 5: uninstallDialog.open(); break
        }
    }

    // Shared setup for the "Installing..." progress popup
    function beginInstall() {
        progressPopup.title = "Installing " + root.emuInfo.name
        progressPopup.subtitle = "Downloading latest release..."
        progressPopup.progressValue = -1
        progressPopup.progressText = "Please wait..."
        progressPopup.accentColor = SettingsTheme.accent
        progressPopup.logoSource = EmulatorLogos.logoForEmu(root.emuId)
        progressPopup.showCloseButton = false
        progressPopup.open()
        app.installEmulator(root.emuId)
    }

    onEmuIdChanged: root.emuList = app.allEmulatorStatus()

    Connections {
        target: app
        function onEmulatorInstalled() {
            root.emuList = app.allEmulatorStatus()
        }
        function onInstallProgress(emuId, progress, phase, detail) {
            if (emuId !== root.emuId) return
            progressPopup.progressValue = progress
            progressPopup.subtitle = phase === "Downloading"
                ? "Downloading latest release..." : "Extracting files..."
            progressPopup.progressText = progress >= 0 ? detail : "Please wait..."
        }
        function onInstallFinished(emuId, success, message) {
            if (emuId !== root.emuId) return
            if (success) {
                progressPopup.close()
            } else {
                progressPopup.title = "Install Failed"
                progressPopup.subtitle = message
                progressPopup.progressValue = 0
                progressPopup.progressText = ""
                progressPopup.showCloseButton = true
            }
        }
        function onUninstallFinished(emuId, success, message) {
            if (emuId !== root.emuId) return
            if (success) {
                progressPopup.close()
                root.back()
            } else {
                progressPopup.title = "Uninstall Failed"
                progressPopup.subtitle = message
                progressPopup.progressValue = 0
                progressPopup.progressText = ""
                progressPopup.showCloseButton = true
            }
        }
    }

    ProgressPopup {
        id: progressPopup
    }

    // --- Main scrollable content ---
    Flickable {
        anchors.fill: parent
        contentHeight: mainColumn.height + 32
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: mainColumn
            width: parent.width
            spacing: 0

            // ========== HEADER ==========
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 20
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: 12
                spacing: 10

                // Back button
                Rectangle {
                    width: 32
                    height: 32
                    radius: 6
                    color: backMouse.containsMouse ? Qt.lighter(SettingsTheme.card, 1.15) : SettingsTheme.card

                    Text {
                        anchors.centerIn: parent
                        text: "\u2190"
                        color: SettingsTheme.accent
                        font.pixelSize: 18
                    }

                    MouseArea {
                        id: backMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: root.back()
                    }
                }

                // Emulator name
                Text {
                    text: root.emuInfo.name
                    color: SettingsTheme.text
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }

                Item { Layout.fillWidth: true }

                // Status badge pill
                Rectangle {
                    id: statusBadge
                    width: badgeText.implicitWidth + 16
                    height: 22
                    radius: SettingsTheme.pillRadius
                    color: root.emuInfo.installed ? SettingsTheme.successDim : SettingsTheme.accentDim

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: root.emuInfo.installed ? "Installed" : "Not Installed"
                        color: root.emuInfo.installed ? SettingsTheme.success : SettingsTheme.accent
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                }
            }

            // ========== LOGO ==========
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                Layout.bottomMargin: 20

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 120
                    height: 120
                    radius: 12
                    color: SettingsTheme.card

                    Image {
                        id: logoImg
                        anchors.centerIn: parent
                        width: parent.width - 16
                        height: parent.height - 16
                        source: EmulatorLogos.logoForEmu(root.emuId)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: false
                    }

                    Rectangle {
                        id: logoMask
                        anchors.fill: logoImg
                        radius: 10
                        visible: false
                    }

                    OpacityMask {
                        anchors.fill: logoImg
                        source: logoImg
                        maskSource: logoMask
                    }
                }
            }

            // ========== INFO SECTION ==========
            SectionLabel {
                text: "INFO"
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: SettingsTheme.sectionSpacing
                implicitHeight: infoCol.height
                radius: 8
                color: SettingsTheme.card
                border.width: 1
                border.color: SettingsTheme.border

                ColumnLayout {
                    id: infoCol
                    width: parent.width
                    spacing: 0

                    // System row
                    InfoRow {
                        label: "System"
                        value: root.emuInfo.systems || ""
                        showDivider: true
                    }

                    // Version row (installed only)
                    InfoRow {
                        visible: root.emuInfo.installed && root.emuInfo.version !== ""
                        label: "Version"
                        value: root.emuInfo.version || ""
                        showDivider: true
                    }

                    // Description row
                    InfoRow {
                        label: "Description"
                        value: root.emuInfo.description || ""
                        showDivider: false
                        wrapValue: true
                    }
                }
            }

            // ========== BIOS SECTION ==========
            SectionLabel {
                text: "BIOS"
            }

            // Installed: BIOS detected / not detected
            Loader {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: SettingsTheme.sectionSpacing
                active: root.emuInfo.installed
                visible: active
                sourceComponent: root.emuInfo.biosRequired ? biosStatusComponent : noBiosRequiredComponent
            }

            // Not installed: neutral note
            Rectangle {
                visible: !root.emuInfo.installed
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: SettingsTheme.sectionSpacing
                height: 40
                radius: 8
                color: SettingsTheme.card
                border.width: 1
                border.color: SettingsTheme.border

                Text {
                    anchors.centerIn: parent
                    text: "BIOS files can be added after installation."
                    color: SettingsTheme.textMuted
                    font.pixelSize: 12
                }
            }

            // ========== ACTIONS / GET STARTED SECTION ==========

            // --- Installed: ACTIONS ---
            ColumnLayout {
                visible: root.emuInfo.installed
                Layout.fillWidth: true
                spacing: 0

                SectionLabel {
                    text: "ACTIONS"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.bottomMargin: 20
                    spacing: 6

                    DetailButton {
                        label: "Emulator Settings"
                        bgColor: SettingsTheme.accent
                        textColor: SettingsTheme.background
                        isFocused: root.focusIndex === root.actionOffset + 0
                        onClicked: app.showEmulatorSettings(root.emuId)
                    }

                    DetailButton {
                        label: "Controller Mapping"
                        bgColor: SettingsTheme.card
                        textColor: SettingsTheme.text
                        isFocused: root.focusIndex === root.actionOffset + 1
                        onClicked: app.showControllerMapping(root.emuId)
                    }

                    DetailButton {
                        label: "Hotkeys"
                        bgColor: SettingsTheme.card
                        textColor: SettingsTheme.text
                        isFocused: root.focusIndex === root.actionOffset + 2
                        onClicked: app.showHotkeySettings(root.emuId)
                    }

                    DetailButton {
                        label: "Reinstall / Update"
                        bgColor: SettingsTheme.accentDim
                        textColor: SettingsTheme.accent
                        isFocused: root.focusIndex === root.actionOffset + 3
                        onClicked: root.beginInstall()
                    }

                    DetailButton {
                        label: "Reset Configuration"
                        bgColor: SettingsTheme.card
                        textColor: SettingsTheme.text
                        isFocused: root.focusIndex === root.actionOffset + 4
                        onClicked: resetDialog.open()
                    }

                    DetailButton {
                        label: "Uninstall"
                        bgColor: SettingsTheme.errorDim
                        textColor: SettingsTheme.error
                        isFocused: root.focusIndex === root.actionOffset + 5
                        onClicked: uninstallDialog.open()
                    }
                }
            }

            // --- Not Installed: GET STARTED ---
            ColumnLayout {
                visible: !root.emuInfo.installed
                Layout.fillWidth: true
                spacing: 0

                SectionLabel {
                    text: "GET STARTED"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.bottomMargin: 20
                    spacing: 8

                    Button {
                        id: installBtn
                        Layout.fillWidth: true
                        implicitHeight: 48

                        background: Rectangle {
                            radius: 8
                            color: installBtn.hovered ? Qt.lighter(SettingsTheme.accent, 1.1) : SettingsTheme.accent

                            layer.enabled: root.focusIndex === 0 && !root.emuInfo.installed
                            layer.effect: Glow {
                                color: SettingsTheme.focusGlow
                                radius: SettingsTheme.focusGlowRadius
                                samples: 17
                                spread: 0.3
                            }
                        }

                        contentItem: Text {
                            text: "Install " + root.emuInfo.name
                            color: SettingsTheme.background
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: root.beginInstall()
                    }

                    Text {
                        text: "Downloads the latest release from GitHub"
                        color: SettingsTheme.textFaint
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            // Bottom spacer
            Item { height: 16 }
        }
    }

    // ========== COMPONENTS ==========

    // Section label
    component SectionLabel: Text {
        Layout.fillWidth: true
        Layout.leftMargin: 20
        Layout.rightMargin: 20
        Layout.bottomMargin: 8
        font.pixelSize: 10
        font.weight: Font.DemiBold
        font.letterSpacing: 1
        font.capitalization: Font.AllUppercase
        color: SettingsTheme.textMuted
    }

    // Info card key-value row
    component InfoRow: Item {
        property string label: ""
        property string value: ""
        property bool showDivider: true
        property bool wrapValue: false

        Layout.fillWidth: true
        implicitHeight: rowContent.height + (showDivider ? 1 : 0)

        RowLayout {
            id: rowContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            height: Math.max(32, valueText.implicitHeight + 16)
            spacing: 8

            Text {
                text: label
                color: SettingsTheme.textDim
                font.pixelSize: 12
                Layout.preferredWidth: 80
            }

            Text {
                id: valueText
                text: value
                color: SettingsTheme.text
                font.pixelSize: 12
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                wrapMode: wrapValue ? Text.WordWrap : Text.NoWrap
                elide: wrapValue ? Text.ElideNone : Text.ElideRight
            }
        }

        // Divider
        Rectangle {
            visible: showDivider
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            height: 1
            color: SettingsTheme.border
        }
    }

    // Action button with focus glow
    component DetailButton: Button {
        id: detailBtn
        property string label: ""
        property color bgColor: SettingsTheme.card
        property color textColor: SettingsTheme.text
        property bool isFocused: false

        Layout.fillWidth: true
        implicitHeight: 40
        padding: 12

        background: Rectangle {
            radius: 8
            color: detailBtn.hovered ? Qt.lighter(detailBtn.bgColor, 1.3) : detailBtn.bgColor

            border.width: detailBtn.isFocused ? 2 : 0
            border.color: SettingsTheme.focusBorder

            layer.enabled: detailBtn.isFocused
            layer.effect: Glow {
                color: SettingsTheme.focusGlow
                radius: SettingsTheme.focusGlowRadius
                samples: 17
                spread: 0.3
            }
        }

        contentItem: Text {
            text: detailBtn.label
            color: detailBtn.textColor
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // BIOS status component (when BIOS is required)
    Component {
        id: biosStatusComponent

        Rectangle {
            property bool detected: root.emuInfo.biosDetected
            implicitHeight: biosInnerCol.height + 20
            radius: 8
            color: detected
                ? Qt.rgba(SettingsTheme.success.r, SettingsTheme.success.g, SettingsTheme.success.b, 0.12)
                : Qt.rgba(SettingsTheme.error.r, SettingsTheme.error.g, SettingsTheme.error.b, 0.12)
            border.width: 1
            border.color: detected
                ? Qt.rgba(SettingsTheme.success.r, SettingsTheme.success.g, SettingsTheme.success.b, 0.3)
                : Qt.rgba(SettingsTheme.error.r, SettingsTheme.error.g, SettingsTheme.error.b, 0.3)

            ColumnLayout {
                id: biosInnerCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 10

                RowLayout {
                    spacing: 8

                    // Status dot
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: detected ? SettingsTheme.success : SettingsTheme.error

                        layer.enabled: detected
                        layer.effect: Glow {
                            color: SettingsTheme.success
                            radius: 6
                            samples: 13
                            spread: 0.5
                        }
                    }

                    Text {
                        text: detected ? "BIOS Detected" : "No BIOS Detected"
                        color: detected ? SettingsTheme.success : SettingsTheme.error
                        font.pixelSize: 13
                        font.weight: Font.Medium
                    }
                }

                // Open BIOS Folder button (only when not detected)
                Button {
                    id: biosFolderBtn
                    visible: !detected
                    Layout.fillWidth: true
                    implicitHeight: 32

                    property bool btnFocused: root.focusIndex === 0 && root.biosButtonVisible

                    background: Rectangle {
                        radius: 6
                        color: biosFolderBtn.hovered ? Qt.lighter(SettingsTheme.card, 1.15) : SettingsTheme.card
                        border.width: biosFolderBtn.btnFocused ? 2 : 0
                        border.color: SettingsTheme.focusBorder

                        Rectangle {
                            anchors.fill: parent; anchors.margins: -4; radius: parent.radius + 4
                            color: "transparent"; border.width: 2; border.color: SettingsTheme.focusBorder
                            opacity: biosFolderBtn.btnFocused ? 0.3 : 0
                            z: -1; visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                        }
                    }

                    contentItem: Text {
                        text: "Open BIOS Folder"
                        color: SettingsTheme.accent
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: app.openBiosFolder()
                }
            }
        }
    }

    // No BIOS required component
    Component {
        id: noBiosRequiredComponent

        Rectangle {
            height: 40
            radius: 8
            color: SettingsTheme.card
            border.width: 1
            border.color: SettingsTheme.border

            Text {
                anchors.centerIn: parent
                text: "No additional BIOS required."
                color: SettingsTheme.textMuted
                font.pixelSize: 12
            }
        }
    }

    // ========== DIALOGS ==========

    // Uninstall confirmation dialog
    Popup {
        id: uninstallDialog
        anchors.centerIn: parent
        width: 360
        height: confirmCol.height + 48
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 12
            color: SettingsTheme.surface
            border.width: 1
            border.color: SettingsTheme.border
        }

        ColumnLayout {
            id: confirmCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            Text {
                text: "Uninstall " + root.emuInfo.name + "?"
                color: SettingsTheme.text
                font.pixelSize: 16
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            Text {
                text: "This will remove the emulator files. Your games, saves, and BIOS files will not be affected."
                color: SettingsTheme.textMuted
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                Button {
                    id: cancelBtn
                    implicitWidth: 100
                    implicitHeight: 36

                    background: Rectangle {
                        radius: 6
                        color: cancelBtn.hovered ? Qt.lighter(SettingsTheme.card, 1.15) : SettingsTheme.card
                    }

                    contentItem: Text {
                        text: "Cancel"
                        color: SettingsTheme.text
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: uninstallDialog.close()
                }

                Button {
                    id: confirmBtn
                    implicitWidth: 120
                    implicitHeight: 36

                    background: Rectangle {
                        radius: 6
                        color: confirmBtn.hovered ? Qt.lighter(SettingsTheme.error, 1.15) : SettingsTheme.error
                    }

                    contentItem: Text {
                        text: "Uninstall"
                        color: SettingsTheme.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        uninstallDialog.close()
                        progressPopup.title = "Uninstalling " + root.emuInfo.name
                        progressPopup.subtitle = "Removing files..."
                        progressPopup.progressValue = -1
                        progressPopup.progressText = ""
                        progressPopup.accentColor = SettingsTheme.error
                        progressPopup.logoSource = EmulatorLogos.logoForEmu(root.emuId)
                        progressPopup.showCloseButton = false
                        progressPopup.open()
                        app.uninstallEmulator(root.emuId)
                    }
                }
            }
        }
    }

    // Reset confirmation dialog
    Popup {
        id: resetDialog
        anchors.centerIn: parent
        width: 360
        height: resetCol.height + 48
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 12
            color: SettingsTheme.surface
            border.width: 1
            border.color: SettingsTheme.border
        }

        ColumnLayout {
            id: resetCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            Text {
                text: "Reset " + root.emuInfo.name + " Configuration?"
                color: SettingsTheme.text
                font.pixelSize: 16
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            Text {
                text: "This will reset all emulator settings, controller mappings, and hotkeys to their install defaults."
                color: SettingsTheme.textMuted
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                Button {
                    id: resetCancelBtn
                    implicitWidth: 100
                    implicitHeight: 36

                    background: Rectangle {
                        radius: 6
                        color: resetCancelBtn.hovered ? Qt.lighter(SettingsTheme.card, 1.15) : SettingsTheme.card
                    }

                    contentItem: Text {
                        text: "Cancel"
                        color: SettingsTheme.text
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: resetDialog.close()
                }

                Button {
                    id: resetConfirmBtn
                    implicitWidth: 120
                    implicitHeight: 36

                    background: Rectangle {
                        radius: 6
                        color: resetConfirmBtn.hovered ? Qt.lighter(SettingsTheme.accent, 1.1) : SettingsTheme.accent
                    }

                    contentItem: Text {
                        text: "Reset"
                        color: SettingsTheme.background
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        app.resetConfiguration(root.emuId)
                        root.emuList = app.allEmulatorStatus()
                        resetDialog.close()
                    }
                }
            }
        }
    }

}
