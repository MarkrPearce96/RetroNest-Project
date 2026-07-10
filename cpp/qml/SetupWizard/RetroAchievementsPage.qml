import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // "login" — username/password form; "success" — signed-in confirmation.
    property string screenState: "login"
    property bool loggingIn: false
    property string errorMessage: ""

    focus: true

    function doLogin() {
        if (root.loggingIn || root.screenState === "success") return
        var u = userField.text.trim()
        var p = passField.text
        if (u === "" || p === "") {
            root.errorMessage = "Enter your username and password"
            return
        }
        root.errorMessage = ""
        root.loggingIn = true
        raService.loginWithPassword(u, p)
    }

    // loginWithPassword() is the libretro/rcheevos token flow — on success it
    // emits loginTokenChanged() (no args), on failure loginFailed(message).
    // (RAService::loginCompleted(success, message) belongs to the separate
    // web-API-key login() path, not loginWithPassword() — verified by
    // reading ra_service.cpp/app_controller.cpp before wiring this up.)
    Connections {
        target: raService
        function onLoginTokenChanged() {
            root.loggingIn = false
            root.errorMessage = ""
            root.screenState = "success"
            passField.text = ""
        }
        function onLoginFailed(message) {
            root.loggingIn = false
            root.errorMessage = message || "Sign-in failed. Check your username and password."
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        spacing: 0

        Text {
            text: "RETROACHIEVEMENTS"
            color: WizardTheme.textDim
            font.pixelSize: 13
            font.letterSpacing: 3
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
        }

        Text {
            text: "Track your achievements"
            color: WizardTheme.textPrimary
            font.pixelSize: 40
            font.weight: Font.ExtraBold
            font.letterSpacing: -1.2
            Layout.fillWidth: true
            Layout.topMargin: 14
            wrapMode: Text.WordWrap
        }

        Text {
            text: "Sign in to sync your achievement unlocks while you play. You can always add this later in Settings."
            color: WizardTheme.textSecondary
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.topMargin: 14
            Layout.bottomMargin: 36
        }

        // ── Signed-in confirmation ──
        Rectangle {
            visible: root.screenState === "success"
            Layout.preferredWidth: successRow.implicitWidth + 48
            Layout.preferredHeight: 64
            radius: 16
            color: WizardTheme.surface
            border.width: 1
            border.color: WizardTheme.surfaceBorder

            RowLayout {
                id: successRow
                anchors.centerIn: parent
                spacing: 12

                Text {
                    text: "✓"
                    color: WizardTheme.success
                    font.pixelSize: 20
                    font.weight: Font.Bold
                }
                Text {
                    text: "Signed in ✓"
                    color: WizardTheme.textPrimary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
            }
        }

        // ── Login form ──
        ColumnLayout {
            visible: root.screenState === "login"
            Layout.fillWidth: true
            Layout.maximumWidth: 460
            spacing: 22

            LoginField {
                id: userField
                Layout.fillWidth: true
                labelText: "Username"
                placeholderText: "Your RetroAchievements username"
                onAccepted: passField.focusInput()
            }

            LoginField {
                id: passField
                Layout.fillWidth: true
                labelText: "Password"
                placeholderText: "Your RetroAchievements password"
                echoMode: TextInput.Password
                onAccepted: root.doLogin()
            }

            Text {
                visible: root.errorMessage !== ""
                text: root.errorMessage
                color: WizardTheme.error
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Item { Layout.fillHeight: true }

        // ── CTA row: white pill "Log in" + ghost "Skip" ──
        RowLayout {
            visible: root.screenState === "login"
            Layout.topMargin: 24
            spacing: 20

            Rectangle {
                id: loginPill
                Layout.preferredWidth: 180
                Layout.preferredHeight: WizardTheme.pillHeight
                radius: WizardTheme.pillRadius
                color: WizardTheme.ctaBg
                opacity: root.loggingIn ? 0.75 : 1.0

                Behavior on opacity { NumberAnimation { duration: WizardTheme.animFast } }

                Text {
                    anchors.centerIn: parent
                    text: root.loggingIn ? "Signing in…" : "Log in"
                    color: WizardTheme.ctaText
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }

                scale: loginMa.pressed ? 0.97 : 1.0
                Behavior on scale { NumberAnimation { duration: 100 } }

                MouseArea {
                    id: loginMa
                    anchors.fill: parent
                    cursorShape: root.loggingIn ? Qt.ArrowCursor : Qt.PointingHandCursor
                    onClicked: root.doLogin()
                }
            }

            // Prominent ghost Skip action — same footprint as the pill CTA,
            // outline only. No-op: skipping persists nothing. Advancing past
            // this page is owned by Main.qml/NavBar's Continue button
            // (wired in a later task).
            Rectangle {
                id: skipPill
                Layout.preferredWidth: 140
                Layout.preferredHeight: WizardTheme.pillHeight
                radius: WizardTheme.pillRadius
                color: skipMa.containsMouse ? WizardTheme.surfaceHover : "transparent"
                border.width: 1
                border.color: WizardTheme.surfaceBorder

                Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }

                Text {
                    anchors.centerIn: parent
                    text: "Skip"
                    color: WizardTheme.textSecondary
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: skipMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {} // no-op — see comment above
                }
            }
        }
    }

    // ========== COMPONENTS ==========

    // A labeled glass text field (username/password), styled after
    // StorageLocationsPage.qml's GlassField but editable rather than a
    // read-only path + Browse button.
    component LoginField: ColumnLayout {
        id: field
        property string labelText: ""
        property string placeholderText: ""
        property alias text: input.text
        property alias echoMode: input.echoMode
        signal accepted()

        // ColumnLayout (a plain Item) isn't a FocusScope, so
        // forceActiveFocus() on the outer instance would focus the
        // wrapper, not the actual TextInput — expose focus explicitly.
        function focusInput() { input.forceActiveFocus() }

        spacing: 11

        Text {
            text: field.labelText
            color: WizardTheme.textDim
            font.pixelSize: 13
            font.letterSpacing: 2
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: 16
            color: WizardTheme.surface
            border.width: input.activeFocus ? 2 : 1
            border.color: input.activeFocus ? WizardTheme.accentLight : WizardTheme.surfaceBorder

            Behavior on border.color { ColorAnimation { duration: WizardTheme.animFast } }

            TextInput {
                id: input
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                verticalAlignment: TextInput.AlignVCenter
                color: WizardTheme.textPrimary
                font.pixelSize: 15
                selectByMouse: true
                clip: true

                onAccepted: field.accepted()

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: field.placeholderText
                    color: WizardTheme.textMuted
                    font: input.font
                    visible: input.text.length === 0 && !input.activeFocus
                }
            }
        }
    }
}
