import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string errorMessage: ""

    // Each credential is applied independently — a user may set the web API
    // key, the password, both, or neither. Tri-state result per part:
    // null = not attempted (or stale from before the field was cleared),
    // true = last attempt succeeded, false = last attempt failed.
    property bool apiKeyPending: false
    property var apiKeyOk: null
    property bool passwordPending: false
    property var passwordOk: null

    readonly property bool busy: apiKeyPending || passwordPending

    focus: true

    // "Apply whatever is filled" — the web API key powers the RA Settings
    // page (raService.login), the password powers in-game achievement
    // tracking (raService.loginWithPassword). Independent network calls;
    // either, both, or neither may be filled in. Re-calling either is safe
    // — each re-validates from scratch.
    function doApply() {
        var u = userField.text.trim(), key = keyField.text.trim(), pass = passField.text
        if (!key && !pass) return // nothing to do — user should just Skip / Continue
        if (!u) { root.errorMessage = "Enter your username"; return }
        root.apiKeyPending = key.length > 0
        root.passwordPending = pass.length > 0
        root.errorMessage = ""
        if (key) raService.login(u, key)
        if (pass) raService.loginWithPassword(u, pass)
    }

    // RAService's two credential flows are independent and surface through
    // three signals: loginCompleted() belongs to the web-API-key login()
    // path, loginTokenChanged()/loginFailed() belong to the separate
    // loginWithPassword() (libretro token) path — verified by reading
    // ra_service.h/ra_service.cpp before wiring this up.
    Connections {
        target: raService
        function onLoginCompleted(success, message) {
            root.apiKeyPending = false
            root.apiKeyOk = success
            if (!success) root.errorMessage = message || "Web API key sign-in failed."
        }
        function onLoginTokenChanged() {
            root.passwordPending = false
            root.passwordOk = true
        }
        function onLoginFailed(message) {
            root.passwordPending = false
            root.passwordOk = false
            root.errorMessage = message || "Sign-in failed. Check your username and password."
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: raContent.implicitHeight + WizardTheme.pageTopMargin + WizardTheme.pageMargin
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
        id: raContent
        x: WizardTheme.pageMargin
        y: WizardTheme.pageTopMargin
        width: root.width - 2 * WizardTheme.pageMargin
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
            text: "Enter your username and web API key from retroachievements.org/settings to unlock catalogs and dashboards, and your password to sync in-game achievement unlocks. Both are optional — you can always add them later in Settings."
            color: WizardTheme.textSecondary
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.topMargin: 14
            Layout.bottomMargin: 36
        }

        // ── Credentials form ──
        ColumnLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 460
            spacing: 22

            LoginField {
                id: userField
                Layout.fillWidth: true
                labelText: "Username"
                placeholderText: "Your RetroAchievements username"
                onAccepted: keyField.focusInput()
            }

            LoginField {
                id: keyField
                Layout.fillWidth: true
                labelText: "Web API Key"
                placeholderText: "Your RetroAchievements web API key"
                hintText: "Find your Web API key at retroachievements.org/settings"
                statusPending: root.apiKeyPending
                statusOk: root.apiKeyOk
                onAccepted: passField.focusInput()
            }

            LoginField {
                id: passField
                Layout.fillWidth: true
                labelText: "Password"
                placeholderText: "Your RetroAchievements password"
                echoMode: TextInput.Password
                statusPending: root.passwordPending
                statusOk: root.passwordOk
                onAccepted: root.doApply()
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

        // ── CTA row: white pill "Log in" + ghost "Skip" ──
        RowLayout {
            Layout.topMargin: 24
            spacing: 20

            Rectangle {
                id: loginPill
                Layout.preferredWidth: 180
                Layout.preferredHeight: WizardTheme.pillHeight
                radius: WizardTheme.pillRadius
                color: WizardTheme.ctaBg
                opacity: root.busy ? 0.75 : 1.0

                Behavior on opacity { NumberAnimation { duration: WizardTheme.animFast } }

                Text {
                    anchors.centerIn: parent
                    text: root.busy ? "Signing in…" : "Log in"
                    color: WizardTheme.ctaText
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }

                scale: loginMa.pressed ? 0.97 : 1.0
                Behavior on scale { NumberAnimation { duration: 100 } }

                MouseArea {
                    id: loginMa
                    anchors.fill: parent
                    cursorShape: root.busy ? Qt.ArrowCursor : Qt.PointingHandCursor
                    onClicked: if (!root.busy) root.doApply()
                }
            }

            // Prominent ghost Skip action — same footprint as the pill CTA,
            // outline only. No-op: skipping persists nothing. Advancing past
            // this page is owned by Main.qml/NavBar's Continue button — this
            // page never advances the SwipeView itself, so a partial or
            // failed sign-in never blocks Continue.
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
    }

    // ========== COMPONENTS ==========

    // A labeled glass text field (username/API key/password), styled after
    // StorageLocationsPage.qml's GlassField but editable rather than a
    // read-only path + Browse button. Optionally shows a hint line below
    // the field and a small pending/success/failure indicator beside the
    // label, so independent async results (API key vs. password) are each
    // visible without a full-page state swap.
    component LoginField: ColumnLayout {
        id: field
        property string labelText: ""
        property string placeholderText: ""
        property string hintText: ""
        property alias text: input.text
        property alias echoMode: input.echoMode
        // Tri-state result of the last attempt for this field: null = none
        // yet, true = succeeded, false = failed. statusPending overrides
        // the icon with a neutral "in progress" marker while a request is
        // in flight.
        property bool statusPending: false
        property var statusOk: null
        signal accepted()

        // ColumnLayout (a plain Item) isn't a FocusScope, so
        // forceActiveFocus() on the outer instance would focus the
        // wrapper, not the actual TextInput — expose focus explicitly.
        function focusInput() { input.forceActiveFocus() }

        spacing: 11

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: field.labelText
                color: WizardTheme.textDim
                font.pixelSize: 13
                font.letterSpacing: 2
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
            }

            Item { Layout.fillWidth: true }

            Text {
                visible: field.statusPending
                text: "…"
                color: WizardTheme.textMuted
                font.pixelSize: 14
                font.weight: Font.Bold
            }
            Text {
                visible: !field.statusPending && field.statusOk === true
                text: "✓"
                color: WizardTheme.success
                font.pixelSize: 14
                font.weight: Font.Bold
            }
            Text {
                visible: !field.statusPending && field.statusOk === false
                text: "✗"
                color: WizardTheme.error
                font.pixelSize: 14
                font.weight: Font.Bold
            }
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
                activeFocusOnTab: true
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
