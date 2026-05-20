# `BaseToast` + `BaseModalCard` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land two QML base components (`BaseToast`, `BaseModalCard`) and migrate four callers (`ActionToast`, `AchievementToast`, `ResumeStateDialog`, `GameActionPopup`) onto them. `UpdateNotification` and `ProgressPopup` stay as-is (different patterns).

**Architecture:** `BaseToast` owns the slide-from-(-10 px)+fade transform, `Behavior` animations, dismiss `Timer`, and `show()`/`hide()` lifecycle. `BaseModalCard` owns the full-screen scrim, centered dark card (with smooth `Behavior on height`), close-on-scrim-click default, and `Escape`/`Back` handling. Callers declare their bespoke content using QML's default-property pattern; focus navigation stays per-caller because navigation models differ.

**Tech Stack:** Qt 6 / QML, CMake. No new dependencies. No QML unit-test infrastructure in this codebase — verification is `cmake --build cpp/build-x86_64` + `macdeployqt` + `codesign --force --deep --sign -` per the `build-cmake-needs-macdeployqt` memory, then manual smoke test.

**Spec:** `docs/superpowers/specs/2026-05-20-base-toast-and-modal-card-design.md`

---

## File Structure

**Create (2):**
- `cpp/qml/AppUI/BaseToast.qml` — slide+fade+timer chrome
- `cpp/qml/AppUI/BaseModalCard.qml` — scrim+card+Escape chrome

**Modify (5):**
- `cpp/qml/AppUI/ActionToast.qml` — collapse onto BaseToast
- `cpp/qml/AppUI/AchievementToast.qml` — collapse onto BaseToast
- `cpp/qml/AppUI/ResumeStateDialog.qml` — collapse onto BaseModalCard
- `cpp/qml/AppUI/GameActionPopup.qml` — collapse onto BaseModalCard
- `cpp/CMakeLists.txt` — register the two new files in the AppUI module's `QML_FILES`

**Out of scope (per spec):**
- `cpp/qml/AppUI/UpdateNotification.qml` — inline pill with queue + buttons; no clean shared kernel
- `cpp/qml/AppUI/ProgressPopup.qml` — Qt built-in `Popup` type; different mechanism

---

## Task 1: Create `BaseToast.qml` + `BaseModalCard.qml` + register in CMake

After this commit, both base components exist and compile. No caller uses them yet (ActionToast/AchievementToast/ResumeStateDialog/GameActionPopup still have their original implementations).

**Files:**
- Create: `cpp/qml/AppUI/BaseToast.qml`
- Create: `cpp/qml/AppUI/BaseModalCard.qml`
- Modify: `cpp/CMakeLists.txt` (add both to the AppUI module's `QML_FILES`)

- [ ] **Step 1: Create `cpp/qml/AppUI/BaseToast.qml`**

```qml
import QtQuick

Item {
    id: root
    default property alias contentItem: contentHolder.data

    property int duration: 1500
    property bool sticky: false
    property bool visibleState: false

    width: contentHolder.childrenRect.width
    height: contentHolder.childrenRect.height
    visible: visibleState || contentHolder.opacity > 0.0

    function show() {
        hideTimer.stop()
        visibleState = true
        if (!sticky) hideTimer.restart()
    }

    function hide() {
        hideTimer.stop()
        visibleState = false
    }

    Timer {
        id: hideTimer
        interval: root.duration
        repeat: false
        onTriggered: root.visibleState = false
    }

    Item {
        id: contentHolder
        width: childrenRect.width
        height: childrenRect.height
        opacity: root.visibleState ? 1.0 : 0.0
        transform: Translate {
            y: root.visibleState ? 0 : -10
            Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }
}
```

- [ ] **Step 2: Create `cpp/qml/AppUI/BaseModalCard.qml`**

```qml
import QtQuick

Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 150

    default property alias cardContent: card.data
    property int cardWidth: 400
    property real cardHeight: 0      // > 0: explicit; 0: auto (children's bounding box + 48 padding)
    property bool closeOnScrimClick: true

    signal closeRequested()

    function open() {
        visible = true
        forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.7)
        MouseArea {
            anchors.fill: parent
            onClicked: { if (root.closeOnScrimClick) root.closeRequested() }
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: root.cardWidth
        height: root.cardHeight > 0 ? root.cardHeight : childrenRect.height + 48
        radius: 12
        color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
        Behavior on height { NumberAnimation { duration: 150 } }
        // Default-property alias points here (card.data). Caller children
        // land directly in the card Rectangle — their `parent` is the card,
        // so existing anchors like `parent.bottom; bottomMargin: -36` work
        // unchanged. The card does not `clip: true`, so negative-margin
        // overflow (e.g. ButtonHints floating below the card) renders.
    }

    Keys.onPressed: function(event) {
        if (!visible) return
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            event.accepted = true
            root.closeRequested()
        }
    }
}
```

- [ ] **Step 3: Register both files in `cpp/CMakeLists.txt`**

Find the AppUI module's `QML_FILES` block (the same one that contains `GenericListPage.qml` and `GenericMultiCardPicker.qml`). Add the two new lines adjacent to those — order alphabetical-ish:

```cmake
        qml/AppUI/BaseModalCard.qml
        qml/AppUI/BaseToast.qml
        qml/AppUI/ButtonHints.qml
        qml/AppUI/GenericListPage.qml
        qml/AppUI/GenericMultiCardPicker.qml
```

(Exact existing ordering may vary; the key is that both new entries land inside the same `QML_FILES` list as `GenericListPage.qml`.)

- [ ] **Step 4: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. Both new components compile but have no callers yet.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/BaseToast.qml cpp/qml/AppUI/BaseModalCard.qml cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(ui): add BaseToast and BaseModalCard QML components

Two new QML base components for the popup/notification consolidation:

- BaseToast: slide-from-(-10px)+fade transform, auto-dismiss Timer,
  show()/hide() lifecycle. Default-property alias places caller
  children inside the animated wrapper. Used by ActionToast and
  AchievementToast (migration commits follow).

- BaseModalCard: full-screen scrim with optional close-on-click,
  centered dark card (radius 12, rgba 0.12/0.12/0.14/0.95, border
  rgba 1/1/1/0.1, Behavior on height 150ms), Escape/Back handler
  emitting closeRequested signal. Default-property alias places
  caller children inside the card Rectangle (not a wrapping Column),
  so existing anchored layouts and negative-margin overflows in
  GameActionPopup's ButtonHints continue to resolve correctly.
  Used by ResumeStateDialog and GameActionPopup (migration commits
  follow).

Not yet consumed by any caller.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Migrate `ActionToast.qml` onto `BaseToast`

**Files:**
- Modify: `cpp/qml/AppUI/ActionToast.qml` (93 → ~50 LOC)

- [ ] **Step 1: Replace `cpp/qml/AppUI/ActionToast.qml` with the migrated version**

```qml
import QtQuick

/**
 * ActionToast — small pill anchored top-right that appears when an
 * in-game action fires (Save State, Load State, Fast Forward). Two
 * modes:
 *
 *   Transient (sticky == false): show() displays the pill, then it
 *     auto-hides after `duration` ms.
 *   Sticky (sticky == true): show() displays it indefinitely; hide()
 *     dismisses it. Used for the Fast Forward indicator while FF is
 *     on.
 *
 * Slide+fade animation and timer lifecycle come from BaseToast.
 */
BaseToast {
    id: root
    duration: 1500

    property string iconSource: ""
    property string label: ""

    Rectangle {
        id: pill
        width: row.implicitWidth + 24
        height: row.implicitHeight + 16
        radius: 14
        color: Qt.rgba(0.08, 0.08, 0.10, 0.88)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                source: root.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: root.iconSource !== ""
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.label
                color: "#ffffff"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                visible: root.label !== ""
            }
        }
    }
}
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. `ActionToast.qml` now imports zero animation/timer code; the `pill` Rectangle is a direct child of the BaseToast (placed into `contentHolder` via the default-property alias).

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/ActionToast.qml
git commit -m "$(cat <<'EOF'
refactor(ui): migrate ActionToast onto BaseToast

ActionToast inherits BaseToast (slide+fade transform, dismiss Timer,
show()/hide() lifecycle). Caller keeps the `iconSource`/`label`
properties and the pill Rectangle visual; all the animation+timer
plumbing is gone. The `sticky` property surfaces from BaseToast
unchanged — Fast Forward path still calls show() with sticky=true.

93 → ~50 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Migrate `AchievementToast.qml` onto `BaseToast`

**Files:**
- Modify: `cpp/qml/AppUI/AchievementToast.qml` (146 → ~95 LOC)

- [ ] **Step 1: Replace `cpp/qml/AppUI/AchievementToast.qml` with the migrated version**

```qml
import QtQuick

/**
 * AchievementToast — richer top-right toast shown when an in-process
 * libretro core unlocks an achievement. Fired by the
 * AppController::raAchievementUnlocked signal handled in AppWindow.qml.
 *
 * Two text rows: a small "ACHIEVEMENT UNLOCKED" header, the achievement
 * title (large, bold), and a description line that wraps if long. Larger
 * footprint than ActionToast (used for Save State / Load State / FF) so
 * the small action pills stay compact.
 *
 * Slide+fade animation and timer lifecycle come from BaseToast.
 */
BaseToast {
    id: root
    duration: 6000

    property string header: "ACHIEVEMENT UNLOCKED"
    property string title: ""
    property string description: ""
    property string imageUrl: ""

    // Public API — call show(title, description, imageUrl) for the
    // achievement-unlock case (default header, default duration), or
    // showWithHeader(...) to drive the toast for game-start banners,
    // game-mastered celebrations, hardcore reset notices, and server
    // error notices.
    function show(t, d, url) {
        showWithHeader("ACHIEVEMENT UNLOCKED", t, d, url, 6000)
    }

    function showWithHeader(h, t, d, url, durationMs) {
        header = h || "ACHIEVEMENT UNLOCKED"
        title = t || ""
        description = d || ""
        imageUrl = url || ""
        duration = (durationMs && durationMs > 0) ? durationMs : 6000
        // Delegate visibility + dismiss-timer lifecycle to BaseToast.
        BaseToast.prototype.show.call(this)
    }

    Rectangle {
        id: card
        // Width = trophy + padding + text column with a sane max so
        // long descriptions wrap rather than push the toast off-screen.
        width: 380
        // Height grows with content (title is fixed, description wraps).
        height: contentRow.implicitHeight + 24
        radius: 14
        color: Qt.rgba(0.08, 0.08, 0.10, 0.92)
        border.color: Qt.rgba(1, 0.84, 0.30, 0.45)   // soft gold border
        border.width: 1

        Row {
            id: contentRow
            anchors {
                left: parent.left; right: parent.right
                top: parent.top; topMargin: 12
                leftMargin: 12; rightMargin: 12
            }
            spacing: 12

            // Achievement badge from RetroAchievements when available;
            // falls back to the trophy glyph if URL is empty or the
            // remote fetch fails (offline, RA CDN hiccup, etc).
            Item {
                id: badgeBox
                anchors.verticalCenter: parent.verticalCenter
                width: 56
                height: 56

                Image {
                    id: badgeImage
                    anchors.fill: parent
                    source: root.imageUrl
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                    cache: true
                    visible: status === Image.Ready
                }
                Image {
                    id: trophyFallback
                    anchors.fill: parent
                    source: "images/hud/achievements.svg"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: !badgeImage.visible
                }
            }

            Column {
                width: parent.width - 56 - parent.spacing
                spacing: 2

                Text {
                    text: root.header
                    color: Qt.rgba(1, 0.84, 0.30, 1)   // gold
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: 1.2
                }
                Text {
                    width: parent.width
                    text: root.title
                    color: "#ffffff"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
                Text {
                    width: parent.width
                    text: root.description
                    color: Qt.rgba(1, 1, 1, 0.78)
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    visible: root.description.length > 0
                }
            }
        }
    }
}
```

**Notes:**
- The original `show(t, d, url)` and `showWithHeader(...)` functions wrapped `visibleState = true; hideTimer.restart()`. After the migration those calls become `BaseToast.prototype.show.call(this)` — QML's pattern for explicitly invoking a base type's function when the derived type defines a same-named override. If that pattern fails at runtime (some Qt 6 versions don't expose `prototype` on QML base types), the fallback is to set `BaseToast`'s state directly: `visibleState = true; if (!sticky) Qt.callLater(... restart timer ...)`. Verify which form Qt accepts during the build step; if the `.prototype.` form errors at compile, use the explicit `visibleState = true` approach instead and document in the commit message.
- `duration` is a property on `BaseToast`; the derived class assigns to it via the `duration = ...` line inside `showWithHeader`. This works because `duration` is exposed as a writable property on the base.

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. If the `BaseToast.prototype.show.call(this)` line errors, replace with:

```qml
        visibleState = true
        // BaseToast handles the dismiss timer automatically when visibleState becomes true
        // — see BaseToast.qml's hideTimer + the show() function for the canonical path.
```

If that ALSO doesn't reset the timer correctly (because there's no `show()` invocation triggering `hideTimer.restart()`), expose a re-callable `restartHideTimer()` helper on `BaseToast` and call it from `showWithHeader`. Document any deviation in the commit message so the reviewer knows.

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/AchievementToast.qml
git commit -m "$(cat <<'EOF'
refactor(ui): migrate AchievementToast onto BaseToast

AchievementToast inherits BaseToast (slide+fade transform, dismiss
Timer, show()/hide() lifecycle). Caller keeps the
`header`/`title`/`description`/`imageUrl` properties and the gold-
bordered card visual with badge + text column. The custom
`show(t,d,url)` and `showWithHeader(...)` parameter-handling
functions delegate to BaseToast's show() for the visibility/timer
side.

146 → ~95 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Migrate `ResumeStateDialog.qml` onto `BaseModalCard`

**Files:**
- Modify: `cpp/qml/AppUI/ResumeStateDialog.qml` (131 → ~85 LOC)

**Behavior changes (intentional, documented in spec):**
- Clicking the scrim now closes the dialog (`closeOnScrimClick` defaults to true; the original `MouseArea` blocked-but-didn't-close).
- `z` drops from `200` (original) to `150` (BaseModalCard default) — same as GameActionPopup's existing value.

- [ ] **Step 1: Replace `cpp/qml/AppUI/ResumeStateDialog.qml` with the migrated version**

```qml
import QtQuick
import QtQuick.Controls

/**
 * ResumeStateDialog — shown on game launch if a save state exists.
 * Asks user whether to resume from save state or start fresh.
 *
 * Scrim, centered card, Escape/Back handler, and close-on-scrim-click
 * come from BaseModalCard. Up/Down focus + Enter activation stay here
 * (no wrap-around — a 2-button list).
 */
BaseModalCard {
    id: root
    cardWidth: 420

    property int focusIndex: 0
    property int pendingGameId: -1
    property string pendingRomPath: ""
    property string pendingEmuId: ""

    signal resumeChosen()
    signal startFreshChosen()

    function openForGame(gameId, romPath, emuId) {
        pendingGameId = gameId
        pendingRomPath = romPath
        pendingEmuId = emuId
        focusIndex = 0
        open()                              // BaseModalCard.open()
    }

    onCloseRequested: {
        visible = false
        // Return focus to the theme page
        if (mainStack.currentItem)
            mainStack.currentItem.forceActiveFocus()
    }

    // Card content (children of BaseModalCard's card Rectangle)
    Column {
        anchors {
            left: parent.left; right: parent.right
            top: parent.top
            margins: 24
        }
        spacing: 8

        Text {
            text: "Save State Found"
            font.pixelSize: 20
            font.bold: true
            color: "#ffffff"
        }

        Text {
            text: "A save state was found. Resume from where you left off?"
            font.pixelSize: 14
            color: Qt.rgba(1, 1, 1, 0.6)
            wrapMode: Text.WordWrap
            width: parent.width
            bottomPadding: 8
        }

        // Buttons
        Repeater {
            model: [
                { label: "Resume",     action: "resume" },
                { label: "Start Fresh", action: "fresh" }
            ]

            delegate: Rectangle {
                width: parent.width
                height: 44
                radius: 6
                color: root.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    text: modelData.label
                    font.pixelSize: 15
                    font.bold: root.focusIndex === index
                    color: root.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.6)
                }
            }
        }
    }

    Keys.onPressed: function(event) {
        if (!visible) return
        if (event.key === Qt.Key_Up) {
            focusIndex = Math.max(0, focusIndex - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            focusIndex = Math.min(1, focusIndex + 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return) {
            if (focusIndex === 0) resumeChosen()
            else startFreshChosen()
            event.accepted = true
        }
        // Escape/Back handled by BaseModalCard → emits closeRequested
    }
}
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/ResumeStateDialog.qml
git commit -m "$(cat <<'EOF'
refactor(ui): migrate ResumeStateDialog onto BaseModalCard

ResumeStateDialog inherits BaseModalCard (scrim, centered card,
Escape/Back handler, close-on-scrim-click). Card content (title,
body, Resume/Start-Fresh buttons) declared inside a Column anchored
to the card top with 24px margins. Up/Down/Return focus model stays
in the caller — 2-button list with no wrap.

Two intentional behavior changes per the spec:
- Clicking the scrim now closes the dialog (was: scrim consumed
  click but didn't close). closeOnScrimClick defaults to true.
- z drops from 200 to 150 — matches GameActionPopup's existing z;
  both dialogs now layer consistently over the game grid.

131 → ~85 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Migrate `GameActionPopup.qml` onto `BaseModalCard`

**Files:**
- Modify: `cpp/qml/AppUI/GameActionPopup.qml` (308 → ~230 LOC)

**Critical detail:** caller sets `cardHeight` explicitly to its existing dynamic formula. `BaseModalCard` has `Behavior on height { NumberAnimation { duration: 150 } }` baked in, so the actions↔confirm transition animates smoothly without per-caller `Behavior` declaration.

- [ ] **Step 1: Replace `cpp/qml/AppUI/GameActionPopup.qml` with the migrated version**

```qml
import QtQuick
import QtQuick.Controls

BaseModalCard {
    id: popup
    cardWidth: 400
    cardHeight: titleText.anchors.topMargin + titleText.height + 8 +
                (popupState === "actions" ? actionColumn.height : confirmColumn.height) + 16

    property int targetGameId: -1
    property bool isFavorite: false
    property string gameTitle: ""

    // State: "actions" shows action list, "confirm" shows delete confirmation
    property string popupState: "actions"
    property int focusIndex: 0
    property bool canScrape: themeContext.hasScraperCredentials()
    property int raGameId: 0

    // Single source of truth for the action list — used both by the visible
    // Repeater and the keyboard handler.
    readonly property var actions: [
        { label: "Scrape", actionId: "scrape", destructive: false },
        { label: "Achievements", actionId: "achievements", destructive: false },
        { label: popup.isFavorite ? "Remove from Favorites" : "Add to Favorites",
          actionId: "favorite", destructive: false },
        { label: "Open ROM Folder", actionId: "openFolder", destructive: false },
        { label: "Remove from Library", actionId: "remove", destructive: true }
    ]

    function openForGame(gameId) {
        var details = themeContext.gameDetails(gameId)
        targetGameId = gameId
        isFavorite = details.favorite === 1
        gameTitle = details.title
        canScrape = themeContext.hasScraperCredentials()
        // raGameId resolved asynchronously via onRaGameIdLookupReady below.
        raGameId = 0
        if (themeContext.hasRACredentials()) {
            themeContext.raRequestGameIdLookup(details.title, details.system || "")
        }
        popupState = "actions"
        focusIndex = 0
        open()                              // BaseModalCard.open()
    }

    onCloseRequested: {
        visible = false
        targetGameId = -1
        // Return focus to theme page (unless settings overlay took over)
        if (!settingsOverlay.visible && mainStack.currentItem)
            mainStack.currentItem.forceActiveFocus()
    }

    Connections {
        target: themeContext
        function onRaGameIdLookupReady(title, lookedUpId) {
            if (title === popup.gameTitle) popup.raGameId = lookedUpId
        }
    }

    // ── Card content: title + actions + confirm + ButtonHints ──
    // Each is a direct child of BaseModalCard's card Rectangle (via the
    // default-property alias to card.data), so existing anchors against
    // `parent` continue to mean the card.

    Text {
        id: titleText
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter
        text: popup.gameTitle
        color: Qt.rgba(1, 1, 1, 0.6)
        font.pixelSize: 13
        elide: Text.ElideRight
        width: parent.width - 32
        horizontalAlignment: Text.AlignHCenter
    }

    // Action List
    Column {
        id: actionColumn
        anchors.top: titleText.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 32
        spacing: 2
        visible: popupState === "actions"

        Repeater {
            model: popup.actions

            delegate: Rectangle {
                required property var modelData
                required property int index

                property bool disabled: (modelData.actionId === "scrape" && !popup.canScrape)
                                     || (modelData.actionId === "achievements" && popup.raGameId === 0)

                width: actionColumn.width
                height: disabled ? 54 : 44
                radius: 6
                color: disabled ? "transparent"
                     : popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        color: disabled ? Qt.rgba(1, 1, 1, 0.3)
                             : modelData.destructive ? "#ff4444"
                             : (popup.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.7))
                        font.pixelSize: 16
                        font.weight: !disabled && popup.focusIndex === index ? Font.DemiBold : Font.Normal
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: disabled
                        text: modelData.actionId === "scrape" ? "Requires ScreenScraper login"
                            : modelData.actionId === "achievements" ? (themeContext.hasRACredentials() ? "Not found on RetroAchievements" : "Requires RetroAchievements login")
                            : ""
                        color: Qt.rgba(1, 1, 1, 0.25)
                        font.pixelSize: 11
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: disabled ? Qt.ArrowCursor : Qt.PointingHandCursor
                    onClicked: if (!parent.disabled) popup.executeAction(modelData.actionId)
                }
            }
        }
    }

    // Confirm Delete
    Column {
        id: confirmColumn
        anchors.top: titleText.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 32
        spacing: 12
        visible: popupState === "confirm"

        Text {
            width: parent.width
            text: "Delete ROM and remove from library?"
            color: "#ff4444"
            font.pixelSize: 15
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 16

            Repeater {
                model: ["Yes", "No"]

                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    width: 120
                    height: 44
                    radius: 6
                    color: popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                    border.color: popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: index === 0 ? "#ff4444" :
                               (popup.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.7))
                        font.pixelSize: 16
                        font.weight: popup.focusIndex === index ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (index === 0) popup.confirmRemove()
                            else popup.cancelRemove()
                        }
                    }
                }
            }
        }
    }

    // Button hints at bottom of card (floats 36px below via negative margin)
    ButtonHints {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -36
        anchors.horizontalCenter: parent.horizontalCenter
        hints: popupState === "confirm"
            ? [{action: "confirm", label: "Select"}, {action: "back", label: "Cancel"}]
            : [{action: "navigate_ud", label: "Navigate"}, {action: "confirm", label: "Select"}, {action: "back", label: "Close"}]
    }

    function executeAction(actionId) {
        switch (actionId) {
        case "scrape":
            themeContext.scrapeGameWithProgress(targetGameId)
            popup.close()
            break
        case "achievements":
            var raId = popup.raGameId
            var title = popup.gameTitle
            popup.close()
            settingsOverlay.navigateToAchievements(raId, title)
            break
        case "favorite":
            themeContext.toggleFavorite(targetGameId)
            popup.close()
            break
        case "openFolder":
            themeContext.openGameRomFolder(targetGameId)
            popup.close()
            break
        case "remove":
            popupState = "confirm"
            focusIndex = 1  // Default to "No"
            break
        }
    }

    function close() {
        // Mirrors the pre-migration behavior; closeRequested handler does the
        // actual hide + focus restoration.
        closeRequested()
    }

    function confirmRemove() {
        themeContext.removeGame(targetGameId)
        close()
    }

    function cancelRemove() {
        popupState = "actions"
        focusIndex = 0
    }

    Keys.onPressed: function(event) {
        if (!visible) return

        if (popupState === "actions") {
            var actionCount = popup.actions.length
            if (event.key === Qt.Key_Up) {
                focusIndex = (focusIndex - 1 + actionCount) % actionCount
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                focusIndex = (focusIndex + 1) % actionCount
                event.accepted = true
            } else if (event.key === Qt.Key_Return) {
                var actionId = popup.actions[focusIndex].actionId
                if ((actionId === "scrape" && !canScrape) ||
                    (actionId === "achievements" && raGameId === 0))
                    return
                executeAction(actionId)
                event.accepted = true
            } else if (event.key === Qt.Key_M) {
                close()
                event.accepted = true
            }
            // Escape/Back handled by BaseModalCard → emits closeRequested
        } else if (popupState === "confirm") {
            if (event.key === Qt.Key_Left) {
                focusIndex = 0
                event.accepted = true
            } else if (event.key === Qt.Key_Right) {
                focusIndex = 1
                event.accepted = true
            } else if (event.key === Qt.Key_Return) {
                if (focusIndex === 0) confirmRemove()
                else cancelRemove()
                event.accepted = true
            }
            // Escape/Back in confirm state: BaseModalCard's closeRequested
            // closes the whole dialog. The pre-migration behavior was for
            // Escape to cancel back to actions state; the new behavior closes
            // the dialog entirely. This is the safer default (Escape always
            // closes) but if you prefer the old behavior, override
            // closeRequested to call cancelRemove() when popupState ===
            // "confirm" and only hide otherwise.
        }
    }
}
```

**Important behavior note in the migration:** the pre-migration code had `Escape`/`Back` in confirm state cancel back to the actions list (via `cancelRemove()`), not close the whole dialog. The migrated code lets `BaseModalCard`'s default close-on-escape behavior take over — Escape closes the dialog regardless of sub-state. This is a deliberate UX simplification (Escape always closes; users use the explicit "No" or "Cancel" to navigate back within the popup). If the user complains, override `onCloseRequested` to dispatch on `popupState`:

```qml
onCloseRequested: {
    if (popupState === "confirm") {
        cancelRemove()
        return
    }
    visible = false
    targetGameId = -1
    if (!settingsOverlay.visible && mainStack.currentItem)
        mainStack.currentItem.forceActiveFocus()
}
```

The plan ships with the simpler behavior; the override is documented here for revert if needed.

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/GameActionPopup.qml
git commit -m "$(cat <<'EOF'
refactor(ui): migrate GameActionPopup onto BaseModalCard

GameActionPopup inherits BaseModalCard (scrim, centered card,
Escape/Back handler, close-on-scrim-click, Behavior on height for
smooth actions↔confirm transition). cardHeight is set to the existing
dynamic formula. Title text, actionColumn, confirmColumn, and
ButtonHints (floating 36px below the card via negative margin) are
direct children of the BaseModalCard, so existing anchor expressions
against `parent` continue to mean the card.

The popupState state machine (actions ↔ confirm), 2D focus model,
and all action handlers stay in the caller. Escape in confirm state
now closes the dialog entirely (was: cancelled back to actions); use
the documented onCloseRequested override to restore the per-state
behavior if undesired.

308 → ~230 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Deploy, sign, smoke test, memory update

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md` — mark #7 shipped
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md` — update Tier 2 progress note

- [ ] **Step 1: Kill any running RetroNest**

```bash
pkill -f "build-x86_64/RetroNest.app/Contents/MacOS/RetroNest" 2>/dev/null
```

- [ ] **Step 2: Deploy + resign**

Per the `build-cmake-needs-macdeployqt` memory, after every `cmake --build` the binary must be re-deployed and re-signed before launching.

```bash
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
```

Verify the Qt refs are `@executable_path/...`:

```bash
otool -L cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest | grep -c "@executable_path/.*Qt"
```

Expected: ≥ 8.

- [ ] **Step 3: Launch and confirm running**

```bash
open cpp/build-x86_64/RetroNest.app
sleep 5
pgrep -fl "build-x86_64/RetroNest.app/Contents/MacOS/RetroNest"
```

Expected: a process line. If empty (crashed), check `~/Library/Logs/DiagnosticReports/RetroNest-*.ips` for the newest crash and investigate.

- [ ] **Step 4: Hand off smoke test to the user**

The controller running this plan should ask the user to verify each migrated dialog:

**ActionToast** (per-action transient pills + sticky FF):
1. Launch a libretro game.
2. Open the in-game menu (Cmd+Shift+Escape), trigger Save State (F5). A top-right pill should slide in, hold ~1.5s, and slide+fade out.
3. Trigger Load State (F7). Same behavior.
4. Toggle Fast Forward on. The sticky FF pill stays visible until FF turns off.

**AchievementToast** (game-start banner, unlock card, mastered celebration):
5. Launch a game with RetroAchievements enabled. A game-start banner should appear as a top-right card with the gold border.
6. Unlock an achievement (or wait for one — depends on the game). The unlock card appears with badge image, title, description; holds ~6s.

**ResumeStateDialog** (full-screen scrim + 2-button modal):
7. Launch a game that has an existing save state. Scrim+card appears. Up/Down toggles between Resume / Start Fresh; Enter triggers the right path; Escape closes.
8. Click the scrim (NEW behavior) — verify it closes. (If undesired, the migration commit's reverse path is `closeOnScrimClick: false`.)

**GameActionPopup** (full-screen scrim + action list + confirm sub-state):
9. Long-press / right-click a game card to open. Scrim+card appears with 5 actions.
10. Up/Down wraps. Enter activates each (Scrape, Achievements, Favorite, Open Folder, Remove).
11. For Remove, the card smoothly resizes between the action list and the Yes/No confirmation (the 150ms Behavior animation). Left/Right navigates Yes/No. Enter on Yes deletes. Escape (NEW: closes entirely instead of cancelling back to actions).

**No regressions** — UpdateNotification and ProgressPopup were not touched; they should still work as before:
12. Trigger an update notification (or check it appears at app launch if an emulator has an update available).
13. Trigger a scrape (Scrape action from GameActionPopup → ProgressPopup appears mid-scrape).

If any step regresses, STOP and report which dialog + action + observed behavior.

- [ ] **Step 5: Update `refactor-roadmap` memory**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`. Find:

```
7. **`BaseNotification` / `PopupBase.qml`** — `cpp/qml/AppUI/{ActionToast,AchievementToast,UpdateNotification,ProgressPopup,ResumeStateDialog,GameActionPopup}.qml` all reimplement same scrim + card + slide/fade + timer/Escape dismissal. `ActionToast.qml:44–72` and `AchievementToast.qml` duplicate animation constants verbatim.
```

Replace with:

```
7. ✅ **BaseToast + BaseModalCard** — shipped 2026-05-20. The original "one BaseNotification across 6 files" framing was wrong; audit showed the 6 files split across 3 categories. Shipped as two narrower components: `cpp/qml/AppUI/BaseToast.qml` (slide+fade transform + dismiss Timer + show/hide lifecycle) consumed by ActionToast and AchievementToast; `cpp/qml/AppUI/BaseModalCard.qml` (scrim + centered card + Behavior on height + Escape/Back handler + close-on-scrim-click default) consumed by ResumeStateDialog and GameActionPopup. UpdateNotification (inline queued pill) and ProgressPopup (Qt built-in Popup) stay as-is — different patterns, no clean shared kernel. Two intentional behavior changes: ResumeStateDialog gains close-on-scrim-click and moves from z:200 to z:150 to match its sibling; GameActionPopup's Escape-in-confirm-state now closes the whole dialog (was: cancelled back to actions — documented revert path in commit). Net ≈ −96 LOC. Spec: `docs/superpowers/specs/2026-05-20-base-toast-and-modal-card-design.md`. Plan: `docs/superpowers/plans/2026-05-20-base-toast-and-modal-card.md`.
```

Update the front-matter `description:`:

```
description: Ongoing generalization/cleanup roadmap for RetroNest. Tier 1 #1-5 and Tier 2 #6, #7, #9 shipped 2026-05-20; #10 retired (overscoped); #8 pending (incremental). Resume here when starting a new session on this work.
```

Update the "Suggested next step" section:

```
## Suggested next step (after #6, #7, #9; #10 retired)

Tier 1 done. Tier 2 #6, #7, #9 done. Only #8 remains as Tier 2 — but it's incremental ("don't grow the AppController facade further when adding new features"), not a one-shot refactor.

The bigger remaining work lives in the logged follow-ups above:
- `RetroAchievementsSettings.qml` keyboard-nav redesign (substantial, own brainstorm)
- `AchievementsPage.qml` migration (after extending `GenericListPage`)
- SetupWizard quick-setting parity
- The two small hotkey follow-ups (footer-shortcuts, bindingdefs drift)
```

- [ ] **Step 6: Update `MEMORY.md` index**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md`. Find:

```
- [Refactor roadmap](refactor-roadmap.md) — Multi-session generalization program. Tier 1 #1-5 + Tier 2 #6 and #9 shipped 2026-05-20; #10 retired (overscoped); remaining Tier 2 (#7 BaseNotification, #8 AppController) pending. Open here when resuming refactor work.
```

Replace with:

```
- [Refactor roadmap](refactor-roadmap.md) — Multi-session generalization program. Tier 1 #1-5 + Tier 2 #6, #7, and #9 shipped 2026-05-20; #10 retired (overscoped); #8 pending (incremental). Open here when resuming refactor work.
```

Memory files live outside the repo — no git commit needed.

---

## Self-review

**Spec coverage:**
- New `BaseToast.qml` matches spec API → Task 1 ✓
- New `BaseModalCard.qml` matches spec API (including `cardHeight` property and `Behavior on height`) → Task 1 ✓
- `ActionToast` migration preserves `iconSource`/`label`/`sticky` semantics → Task 2 ✓
- `AchievementToast` migration preserves `show(t,d,url)` + `showWithHeader(...)` API + 6000ms default duration → Task 3 ✓
- `ResumeStateDialog` migration with cardWidth 420 + intentional z + scrim-click changes → Task 4 ✓
- `GameActionPopup` migration with explicit `cardHeight` formula, `ButtonHints` parent-anchor preserved, popupState state machine intact → Task 5 ✓
- CMakeLists.txt entries for both new files → Task 1 ✓
- Smoke test covers all 4 migrated dialogs + verifies UpdateNotification and ProgressPopup don't regress → Task 6 ✓
- Memory updates → Task 6 ✓

**Placeholder scan:** No TBDs. Every code block contains exact content. Two places use conditional fallback language (`BaseToast.prototype.show.call(this)` fallback in Task 3, `onCloseRequested` per-state override in Task 5) — both fully document the alternative form the engineer would use, not vague "implement appropriate fallback" instructions.

**Type / name consistency:**
- `BaseToast.show()` and `BaseToast.hide()` referenced the same way across Task 1 (def), Task 2 (consumer), Task 3 (consumer).
- `BaseModalCard.open()`, `closeRequested` signal, `cardWidth`, `cardHeight`, `closeOnScrimClick` all consistent across Task 1, Task 4, Task 5.
- Property names in caller files (`iconSource`, `label`, `header`, `title`, `description`, `imageUrl`, `focusIndex`, `pendingGameId`, `popupState`, etc.) all match the pre-migration originals — call sites in QML elsewhere don't need to change.

**One residual question:** Task 3's `BaseToast.prototype.show.call(this)` pattern — Qt 6 sometimes does and sometimes doesn't support this. The plan documents the fallback (assign `visibleState = true` directly) but the implementer must verify which path Qt accepts. If both fail, the plan also describes adding a `restartHideTimer()` helper. The fallback is fully spelled out — no "figure it out" gap.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-20-base-toast-and-modal-card.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task with two-stage review. Six tasks; Task 5 (`GameActionPopup`) is the biggest and benefits most from independent review.

**2. Inline Execution** — same session, batch with checkpoints.

Which approach?
