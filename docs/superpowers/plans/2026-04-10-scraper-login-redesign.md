# Scraper Login Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rework the ScreenScraper login page so its visual layout mirrors the RetroAchievements login card, replace the placeholder drawn "RA" square on the RetroAchievements login card with the real PNG logo, and register both new PNG logos as Qt resources.

**Architecture:** Three QML files touched (`RetroAchievementsSettings.qml`, `ScraperSettings.qml`, `cpp/CMakeLists.txt`), plus two PNG assets moved from `assets/` to `cpp/qml/AppUI/images/` and renamed to match the existing `*_logo.png` convention. The scraper login's existing focus state machine, virtual-keyboard integration, and credential-validation slots are preserved byte-for-byte; only the visual layout of "State 0: LOGIN" changes.

**Tech Stack:** Qt6 QML (Quick + Quick Controls + Layouts), CMake 3.16+, Qt resource system via `qt_add_qml_module(... RESOURCES ...)`.

**Spec:** `docs/superpowers/specs/2026-04-10-scraper-login-redesign-design.md`

**Critical behavior that must NOT be broken in any task:**
- The scraper login preserves `loginUserField`, `loginPassField`, `signInBtn`, and `loginError` as element IDs. These are referenced from outside the login block — search hits on lines 38–43, 312–328, 447–453, and 1901–1903 of the current file. Renaming any of them is a bug.
- `signInBtn` must expose a `bool enabled` property; `loginError` must expose `visible` and `text`.
- The scraper's `activateLoginFocused()` function at lines 308–331 must keep working unchanged. It branches on `inputManager.lastInputWasController` and either opens the `virtualKeyboard` or calls `forceActiveFocus()`. Do not touch this function.
- The scraper's `Connections { target: loginUserField/loginPassField }` blocks at lines 37–44 call `_maybeReclaimLoginFocus()` when focus is lost. These must continue to fire.

---

## File Structure

- **Move + rename:**
  - `assets/RetroAchievments Logo.png` → `cpp/qml/AppUI/images/retroachievements_logo.png`
  - `assets/ScreenScraper Logo.png` → `cpp/qml/AppUI/images/screenscraper_logo.png`
  - Then delete the empty `assets/` directory (including its untracked `.DS_Store`).
- **Modify:**
  - `cpp/CMakeLists.txt` — register the two new PNGs under `appui_backing`'s `RESOURCES`
  - `cpp/qml/AppUI/RetroAchievementsSettings.qml:112-129` — swap drawn "RA" square for the real PNG
  - `cpp/qml/AppUI/ScraperSettings.qml:527-741` — replace "State 0: LOGIN" block with a centered card mirroring RA

No new files. No C++ changes. No header changes.

---

## Task 1: Move and rename the logo PNGs

**Files:**
- Delete (via git mv): `assets/RetroAchievments Logo.png`, `assets/ScreenScraper Logo.png`
- Create (via git mv): `cpp/qml/AppUI/images/retroachievements_logo.png`, `cpp/qml/AppUI/images/screenscraper_logo.png`
- Delete: `assets/.DS_Store` (untracked), then the empty `assets/` directory

- [ ] **Step 1: Confirm the target directory exists**

Run:

```sh
ls /Users/mark/Documents/RetroNest-Project/cpp/qml/AppUI/images/
```

Expected: lists `pcsx2_logo.png`, `duckstation_logo.png`, `ppsspp_logo.png`, `empty-state-bg.webp`, and the `ar/`, `res/`, `controllers/` subdirectories. If this fails, stop — something is wrong with the working tree.

- [ ] **Step 2: git mv the RetroAchievements logo**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project && \
  git mv "assets/RetroAchievments Logo.png" "cpp/qml/AppUI/images/retroachievements_logo.png"
```

Expected: no output. This both moves the file and renames it (fixing the `Achievments` misspelling and removing the space).

- [ ] **Step 3: git mv the ScreenScraper logo**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project && \
  git mv "assets/ScreenScraper Logo.png" "cpp/qml/AppUI/images/screenscraper_logo.png"
```

Expected: no output.

- [ ] **Step 4: Remove the orphaned `.DS_Store` and empty `assets/` directory**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project && \
  rm -f assets/.DS_Store && rmdir assets
```

Expected: no output. `rmdir` will fail if `assets/` is non-empty — if so, list what's inside with `ls -la assets/` and stop; the plan did not anticipate additional files there.

- [ ] **Step 5: Verify the working tree state**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project && git status --short
```

Expected output should show two renames and (depending on earlier uncommitted state) may show other unrelated changes. The relevant lines are:

```
R  assets/RetroAchievments Logo.png -> cpp/qml/AppUI/images/retroachievements_logo.png
R  assets/ScreenScraper Logo.png -> cpp/qml/AppUI/images/screenscraper_logo.png
```

If you see a `D` (deleted) plus an `??` (untracked new file) instead of `R` (rename), git didn't detect the rename — that's fine, the end result is the same, but note it in the commit message.

---

## Task 2: Register the PNGs as Qt resources

**Files:**
- Modify: `cpp/CMakeLists.txt` around line 289 (the `RESOURCES` block of `qt_add_qml_module(appui_backing …)`)

- [ ] **Step 1: Read the current RESOURCES block to confirm line numbers**

Read `cpp/CMakeLists.txt` lines 280–312. You should see:

```cmake
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
        qml/AppUI/images/empty-state-bg.webp
```

followed by the `ar/`, `res/`, and `controllers/` entries, then `RESOURCE_PREFIX /)`.

- [ ] **Step 2: Add the two new PNGs to the RESOURCES list**

Edit `cpp/CMakeLists.txt`:

Old string:

```cmake
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
        qml/AppUI/images/empty-state-bg.webp
```

New string:

```cmake
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
        qml/AppUI/images/retroachievements_logo.png
        qml/AppUI/images/screenscraper_logo.png
        qml/AppUI/images/empty-state-bg.webp
```

Notes:
- Only the `appui_backing` block (around line 288) needs updating, not the `${PROJECT_NAME}` one at line 170 or the `setupwizard_backing` one at line 238.
- If the existing block's whitespace differs from what's shown above, preserve the file's existing indentation exactly — copy the indent from the adjacent `pcsx2_logo.png` line.

- [ ] **Step 3: Commit Tasks 1 + 2 together**

Review the diff:

```sh
cd /Users/mark/Documents/RetroNest-Project && git diff --stat && git diff cpp/CMakeLists.txt
```

Expected: the rename entries for the two PNGs, plus a small `cpp/CMakeLists.txt` hunk adding the two lines.

Stage and commit:

```sh
cd /Users/mark/Documents/RetroNest-Project && \
  git add cpp/CMakeLists.txt "cpp/qml/AppUI/images/retroachievements_logo.png" "cpp/qml/AppUI/images/screenscraper_logo.png" && \
  git add -u "assets/RetroAchievments Logo.png" "assets/ScreenScraper Logo.png" 2>/dev/null; \
  git commit -m "$(cat <<'EOF'
assets: relocate RA + ScreenScraper logos under qml/AppUI/images and register in CMake

Moves the two PNGs from the repo-root assets/ directory to
cpp/qml/AppUI/images/ (the convention for existing Qt-resource-backed
images like pcsx2_logo.png) and registers them in the appui_backing
qt_add_qml_module RESOURCES block. Also fixes the misspelling in the
original filename ("Achievments" -> "Achievements") and drops the space
from the filename so QML relative paths work cleanly. The now-empty
assets/ directory and its .DS_Store are removed.

No QML code references the new resources yet; that follows in the next
commits.
EOF
)"
```

Expected: one commit created containing the file renames and the CMake edit.

---

## Task 3: Build to verify the CMake change resolves cleanly

**Files:** none (build verification only)

- [ ] **Step 1: Run cmake build**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project/cpp && cmake --build build 2>&1 | tail -30
```

Expected: build completes successfully. The appui_backing `.rcc` or resource object regenerates to include the two new PNG paths. No errors referencing the new PNGs.

If the build fails with "cannot find file" on one of the new PNGs, double-check the filenames in `cpp/qml/AppUI/images/` match exactly (case-sensitive): `retroachievements_logo.png` and `screenscraper_logo.png`.

---

## Task 4: Swap RetroAchievements logo to the real PNG

**Files:**
- Modify: `cpp/qml/AppUI/RetroAchievementsSettings.qml:112-129`

- [ ] **Step 1: Read current code to confirm exact text**

Read `cpp/qml/AppUI/RetroAchievementsSettings.qml` lines 110–131. Expected:

```qml
                // RA Logo
                Rectangle {
                    width: 64
                    height: 64
                    radius: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: SettingsTheme.accent }
                        GradientStop { position: 1.0; color: Qt.darker(SettingsTheme.accent, 1.3) }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "RA"
                        color: SettingsTheme.text
                        font.pixelSize: 24
                        font.weight: Font.Bold
                    }
                }
```

- [ ] **Step 2: Replace with an Image node**

Edit `cpp/qml/AppUI/RetroAchievementsSettings.qml`:

Old string:

```qml
                // RA Logo
                Rectangle {
                    width: 64
                    height: 64
                    radius: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: SettingsTheme.accent }
                        GradientStop { position: 1.0; color: Qt.darker(SettingsTheme.accent, 1.3) }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "RA"
                        color: SettingsTheme.text
                        font.pixelSize: 24
                        font.weight: Font.Bold
                    }
                }
```

New string:

```qml
                // RA Logo
                Image {
                    width: 64
                    height: 64
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "images/retroachievements_logo.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
```

Notes:
- Relative path `"images/retroachievements_logo.png"` works because the QML file lives in `cpp/qml/AppUI/` and the resource prefix `/` plus the CMake `RESOURCES` entry makes this resolve to `qrc:/AppUI/qml/AppUI/images/retroachievements_logo.png` automatically. This is exactly how `EmptyStatePage.qml` loads `images/empty-state-bg.webp`.
- Do NOT wrap the Image in an outer Rectangle. The spec is explicit: drop the gradient background entirely.
- The dashboard-state avatar Rectangle (around line 284) keeps its gradient + letter fallback — do not touch it. Only the login-state logo (lines 112–129) is swapped.

- [ ] **Step 3: Build and verify**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project/cpp && cmake --build build 2>&1 | tail -10
```

Expected: build succeeds. If the QML parser reports an error on `RetroAchievementsSettings.qml`, re-read the edit region and fix indentation/braces.

- [ ] **Step 4: Commit**

```sh
cd /Users/mark/Documents/RetroNest-Project && \
  git add cpp/qml/AppUI/RetroAchievementsSettings.qml && \
  git commit -m "$(cat <<'EOF'
feat(ra-login): use real PNG logo in place of drawn RA square

Replaces the 64x64 gradient Rectangle + "RA" text node in the
RetroAchievements login card with an Image node loading the newly
registered retroachievements_logo.png. Login-state only; the
dashboard-state avatar (which still uses a gradient + initial letter
fallback when no user pic is available) is unchanged.
EOF
)"
```

Expected: clean commit, one file modified.

---

## Task 5: Redesign the scraper login state to match the RA card

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:527-741`

This is the largest edit. It replaces the entire "State 0: LOGIN" block (the first child of the `StackLayout` at line 516) — currently a `Flickable` containing a `ColumnLayout` — with a centered card `Item` that mirrors the RetroAchievements login layout.

- [ ] **Step 1: Read the current LOGIN state block**

Read `cpp/qml/AppUI/ScraperSettings.qml` lines 527–741 in full. Confirm the structure is:

```
        // ====================================================================
        // State 0: LOGIN
        // ====================================================================
        Flickable {
            contentHeight: loginCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: loginCol
                width: parent.width
                spacing: 16

                Item { height: 8 }
                Text { text: "Scraper" ... }         // 18px heading — to be removed
                Text { text: "Enter your ScreenScraper.fr credentials..." }
                ColumnLayout { ... Username field ... }
                ColumnLayout { ... Password field ... }
                Rectangle { id: signInBtn ... }
                Text { id: loginError ... }
                Item { height: 24 }
            }
        }
```

The id `loginCol` is referenced only on line 531 (`contentHeight: loginCol.height`) — nowhere else in the file. Safe to drop when we replace the Flickable.

- [ ] **Step 2: Replace the entire LOGIN state block**

Edit `cpp/qml/AppUI/ScraperSettings.qml`. The `old_string` is the Flickable block from the line beginning `        Flickable {` (line 530) through its matching closing `        }` (line 741). The `new_string` is a centered card `Item` containing the card Rectangle.

Old string (must match the file exactly — note the 8-space left indentation inside the StackLayout):

```qml
        Flickable {
            contentHeight: loginCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: loginCol
                width: parent.width
                spacing: 16

                Item { height: 8 }

                Text {
                    text: "Scraper"
                    color: SettingsTheme.text
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                }

                Text {
                    text: "Enter your ScreenScraper.fr credentials to download media and metadata for your games."
                    color: SettingsTheme.textMuted
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                }

                // Username
                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 6

                    Text {
                        text: "Username"
                        color: SettingsTheme.textMuted
                        font.pixelSize: 13
                    }
                    Rectangle {
                        Layout.preferredWidth: 300
                        height: 36
                        radius: 6
                        color: SettingsTheme.card
                        border.width: (root.screenState === "login" && root.loginFocusIndex === 0) ? 2 : 1
                        border.color: (root.screenState === "login" && root.loginFocusIndex === 0) || loginUserField.activeFocus
                            ? SettingsTheme.focusBorder : SettingsTheme.border

                        // Focus glow
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -4
                            radius: parent.radius + 4
                            color: "transparent"
                            border.width: 2
                            border.color: SettingsTheme.focusBorder
                            opacity: (root.screenState === "login" && root.loginFocusIndex === 0) ? 0.3 : 0
                            z: -1
                            visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                        }

                        TextField {
                            id: loginUserField
                            anchors.fill: parent
                            placeholderText: "screenscraper.fr username"
                            placeholderTextColor: SettingsTheme.textDim
                            color: SettingsTheme.text
                            font.pixelSize: 13
                            background: Item {}
                            leftPadding: 10

                            function _moveToPassword() {
                                loginFocusIndex = 1
                                loginPassField.forceActiveFocus()
                            }

                            Keys.onTabPressed: loginUserField._moveToPassword()
                            Keys.onReturnPressed: loginUserField._moveToPassword()
                            Keys.onEnterPressed: loginUserField._moveToPassword()
                        }
                    }
                }

                // Password
                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 6

                    Text {
                        text: "Password"
                        color: SettingsTheme.textMuted
                        font.pixelSize: 13
                    }
                    Rectangle {
                        Layout.preferredWidth: 300
                        height: 36
                        radius: 6
                        color: SettingsTheme.card
                        border.width: (root.screenState === "login" && root.loginFocusIndex === 1) ? 2 : 1
                        border.color: (root.screenState === "login" && root.loginFocusIndex === 1) || loginPassField.activeFocus
                            ? SettingsTheme.focusBorder : SettingsTheme.border

                        // Focus glow
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -4
                            radius: parent.radius + 4
                            color: "transparent"
                            border.width: 2
                            border.color: SettingsTheme.focusBorder
                            opacity: (root.screenState === "login" && root.loginFocusIndex === 1) ? 0.3 : 0
                            z: -1
                            visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                        }

                        TextField {
                            id: loginPassField
                            anchors.fill: parent
                            placeholderText: "screenscraper.fr password"
                            placeholderTextColor: SettingsTheme.textDim
                            color: SettingsTheme.text
                            font.pixelSize: 13
                            echoMode: TextInput.Password
                            background: Item {}
                            leftPadding: 10

                            function _submitLogin() {
                                if (signInBtn.enabled) {
                                    signInBtn.enabled = false
                                    loginError.visible = false
                                    app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                                }
                            }

                            Keys.onTabPressed: {
                                loginFocusIndex = 2
                                root.forceActiveFocus()
                            }
                            Keys.onReturnPressed: loginPassField._submitLogin()
                            Keys.onEnterPressed: loginPassField._submitLogin()
                        }
                    }
                }

                // Sign In button
                Rectangle {
                    id: signInBtn
                    property bool enabled: true
                    property bool isFocused: root.screenState === "login" && root.loginFocusIndex === 2
                    Layout.leftMargin: 24
                    width: 120
                    height: 36
                    radius: 6
                    color: enabled ? SettingsTheme.accent : SettingsTheme.card
                    opacity: enabled ? 1.0 : 0.5
                    border.width: isFocused ? 2 : 0
                    border.color: SettingsTheme.text

                    // Focus glow
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        radius: parent.radius + 4
                        color: "transparent"
                        border.width: 2
                        border.color: SettingsTheme.focusBorder
                        opacity: signInBtn.isFocused ? 0.3 : 0
                        z: -1
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "Sign In"
                        color: SettingsTheme.background
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        enabled: signInBtn.enabled
                        onClicked: {
                            signInBtn.enabled = false
                            loginError.visible = false
                            app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                        }
                    }
                }

                Text {
                    id: loginError
                    visible: false
                    color: SettingsTheme.error
                    font.pixelSize: 12
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Item { height: 24 }
            }
        }
```

New string (centered card mirroring RetroAchievementsSettings lines 92–253):

```qml
        Item {
            id: scraperLoginPage

            Rectangle {
                anchors.centerIn: parent
                width: 360
                height: loginCardCol.height + 48
                radius: 12
                color: SettingsTheme.card
                border.width: 1
                border.color: SettingsTheme.border

                Column {
                    id: loginCardCol
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 16

                    // ScreenScraper Logo
                    Image {
                        width: 64
                        height: 64
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "images/screenscraper_logo.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        text: "ScreenScraper"
                        color: SettingsTheme.text
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: "Enter your ScreenScraper.fr credentials to download media and metadata for your games."
                        color: SettingsTheme.textMuted
                        font.pixelSize: 13
                        width: parent.width
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Item { width: 1; height: 4 }

                    // Username field
                    Column {
                        width: parent.width
                        spacing: 4

                        Text {
                            text: "Username"
                            color: SettingsTheme.textMuted
                            font.pixelSize: 12
                        }

                        Rectangle {
                            width: parent.width
                            height: 40
                            radius: 8
                            color: SettingsTheme.base
                            border.width: 1
                            border.color: (root.screenState === "login" && root.loginFocusIndex === 0) || loginUserField.activeFocus
                                ? SettingsTheme.accent : SettingsTheme.border

                            TextField {
                                id: loginUserField
                                anchors.fill: parent
                                placeholderText: "screenscraper.fr username"
                                placeholderTextColor: SettingsTheme.textDim
                                color: SettingsTheme.text
                                font.pixelSize: 14
                                background: Item {}
                                leftPadding: 10
                                rightPadding: 10

                                function _moveToPassword() {
                                    loginFocusIndex = 1
                                    loginPassField.forceActiveFocus()
                                }

                                Keys.onTabPressed: loginUserField._moveToPassword()
                                Keys.onReturnPressed: loginUserField._moveToPassword()
                                Keys.onEnterPressed: loginUserField._moveToPassword()
                            }
                        }
                    }

                    // Password field
                    Column {
                        width: parent.width
                        spacing: 4

                        Text {
                            text: "Password"
                            color: SettingsTheme.textMuted
                            font.pixelSize: 12
                        }

                        Rectangle {
                            width: parent.width
                            height: 40
                            radius: 8
                            color: SettingsTheme.base
                            border.width: 1
                            border.color: (root.screenState === "login" && root.loginFocusIndex === 1) || loginPassField.activeFocus
                                ? SettingsTheme.accent : SettingsTheme.border

                            TextField {
                                id: loginPassField
                                anchors.fill: parent
                                placeholderText: "screenscraper.fr password"
                                placeholderTextColor: SettingsTheme.textDim
                                color: SettingsTheme.text
                                font.pixelSize: 14
                                echoMode: TextInput.Password
                                background: Item {}
                                leftPadding: 10
                                rightPadding: 10

                                function _submitLogin() {
                                    if (signInBtn.enabled) {
                                        signInBtn.enabled = false
                                        loginError.visible = false
                                        app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                                    }
                                }

                                Keys.onTabPressed: {
                                    loginFocusIndex = 2
                                    root.forceActiveFocus()
                                }
                                Keys.onReturnPressed: loginPassField._submitLogin()
                                Keys.onEnterPressed: loginPassField._submitLogin()
                            }
                        }
                    }

                    // Connect button
                    Rectangle {
                        id: signInBtn
                        property bool enabled: true
                        width: parent.width
                        height: 42
                        radius: 8
                        color: (root.screenState === "login" && root.loginFocusIndex === 2)
                            ? Qt.lighter(SettingsTheme.accent, 1.2)
                            : SettingsTheme.accent
                        opacity: enabled ? 1.0 : 0.6

                        Text {
                            anchors.centerIn: parent
                            text: signInBtn.enabled ? "Connect" : "Validating..."
                            color: SettingsTheme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            enabled: signInBtn.enabled
                            onClicked: {
                                signInBtn.enabled = false
                                loginError.visible = false
                                app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                            }
                        }
                    }

                    // Error text
                    Text {
                        id: loginError
                        visible: false
                        text: ""
                        color: SettingsTheme.error
                        font.pixelSize: 12
                        width: parent.width
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
```

Notes on preserved semantics:
- `loginUserField`, `loginPassField`, `signInBtn`, and `loginError` ids are preserved.
- `signInBtn.enabled` is still a custom `property bool enabled: true` so existing code at lines 326, 453, 663–664, and 721 continues to toggle it.
- `loginError.visible` / `loginError.text` are still assignable the same way.
- The password field still has `echoMode: TextInput.Password`.
- `Keys.onTabPressed` / `Keys.onReturnPressed` / `Keys.onEnterPressed` on both fields preserve the existing behavior (`_moveToPassword()` on username, `_submitLogin()` on password).
- The `activateLoginFocused()` function at lines 308–331 (outside the edit region) is untouched and still drives controller-path activation, including `virtualKeyboard.open(...)`.
- The `Connections { target: loginUserField/loginPassField }` blocks at lines 37–44 (outside the edit region) still fire on focus loss because the ids are preserved.
- The focus-glow decorations (nested `anchors.margins: -4` Rectangles) are intentionally removed, replaced by the simpler `border.color` highlight approach used by RetroAchievementsSettings.
- The 18px "Scraper" in-body heading is removed; the top nav bar already displays "Scraper".
- The button color approach mirrors RetroAchievementsSettings: `Qt.lighter(accent, 1.2)` when focus-highlighted, plain accent otherwise, with `opacity: 0.6` during validation.

- [ ] **Step 3: Build**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project/cpp && cmake --build build 2>&1 | tail -30
```

Expected: build succeeds.

If you see errors like:
- `Cannot assign to non-existent property "enabled"` — you forgot the `property bool enabled: true` on `signInBtn`.
- `loginUserField is not defined` in one of the Connections blocks — your `id:` lines got dropped in the replacement; re-check.
- QML parse errors pointing at brace mismatches — re-check that the new string's braces all balance; the structure is 1 outer `Item` → 1 `Rectangle` → 1 `Column` → 6 children (Image, Text, Text, Item spacer, Column×2, Rectangle, Text).

Do not proceed until build is green.

- [ ] **Step 4: Commit**

```sh
cd /Users/mark/Documents/RetroNest-Project && \
  git add cpp/qml/AppUI/ScraperSettings.qml && \
  git commit -m "$(cat <<'EOF'
feat(scraper-login): redesign login page to match RetroAchievements card

Replaces the full-width left-aligned login form with a centered 360px
card mirroring RetroAchievementsSettings.qml: ScreenScraper PNG logo,
title, description, Username + Password fields, full-width Connect
button with "Validating..." disabled state, error text below.

Preserves:
- loginUserField / loginPassField / signInBtn / loginError ids (still
  referenced by activateLoginFocused(), the virtual keyboard handlers
  at line ~1901, and the onScraperCredentialsValidated slot)
- signInBtn.enabled custom property contract
- TextField (not TextInput) so the virtual keyboard integration at
  inputManager.lastInputWasController in activateLoginFocused() still
  works for controller navigation
- Focus state machine (loginFocusIndex 0/1/2) and its Keys.onPressed
  handlers at the root level

Drops:
- The in-body "Scraper" 18px heading (redundant with the top nav bar)
- The anchors.margins: -4 focus glow decorations, in favor of the
  simpler border-color-on-focus approach used by RA
EOF
)"
```

Expected: clean commit, one file modified.

---

## Task 6: Final build + manual verification

**Files:** none

- [ ] **Step 1: Clean rebuild**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project/cpp && cmake --build build 2>&1 | tail -20
```

Expected: build completes, no warnings or errors from `RetroAchievementsSettings.qml` or `ScraperSettings.qml`.

- [ ] **Step 2: Flag manual verification as user-side**

Manual GUI verification cannot run headless. Document in the final report that the following steps are pending user-side verification:

1. Launch the built `RetroNest.app` bundle.
2. Open Settings → RetroAchievements. If signed in, sign out first. Confirm the real PNG logo renders in place of the drawn "RA" square.
3. Open Settings → Scraper. If signed in, sign out first. Confirm:
   - Centered 360px card (not full-width left-aligned).
   - Logo, "ScreenScraper" title, description, Username field, Password field, "Connect" button in order.
   - Visually matches the RetroAchievements card.
4. Press Down/Up on keyboard (or a controller) to cycle focus through Username → Password → Connect. Confirm accent borders highlight without the old focus glow.
5. Activate the Username field via controller — confirm the virtual keyboard opens with `USERNAME` label (regression check on the virtual keyboard integration).
6. Enter deliberately wrong credentials → Connect → confirm button text switches to "Validating..." then back to "Connect", and the error text appears below the button.
7. Enter correct credentials → confirm the dashboard state loads as before (regression check).
