# GenericListPage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land a single `GenericListPage.qml` component, migrate all four list/grid pages (AllGames, RecentlyPlayed, EmulatorManageGrid, Themes) behind it, and delete the orphaned `GamesPage.qml` + `GameGridView.qml`.

**Architecture:** One QML `Item`-based component owns the ListView + arrow-key wrap-around focus + Enter activation + Escape back + optional header/empty/footer. Each page becomes a delegate Component and a small set of property bindings.

**Tech Stack:** Qt 6 / QML, CMake (qt_add_qml_module), zero new dependencies. No QML unit tests exist in this codebase — verification is `cmake --build build-x86_64` + manual smoke test of each page. Default to x86-only dev iteration per the `build-prefer-x86-only` memory.

**Spec:** `docs/superpowers/specs/2026-05-20-generic-list-page-design.md`

---

## File Structure

**Create (1):**
- `cpp/qml/AppUI/GenericListPage.qml` — the new component

**Modify (5):**
- `cpp/qml/AppUI/AllGamesPage.qml` — collapse onto GenericListPage; delegate gains focused-border state
- `cpp/qml/AppUI/RecentlyPlayedPage.qml` — collapse; delegate gains focused-border state
- `cpp/qml/AppUI/EmulatorManageGrid.qml` — collapse; delegate switches to `ListView.isCurrentItem`
- `cpp/qml/AppUI/ThemesPage.qml` — collapse + wrap in ColumnLayout with ButtonHints; delegate switches to `ListView.isCurrentItem`
- `cpp/CMakeLists.txt` — register `GenericListPage.qml`, remove `GamesPage.qml` and `GameGridView.qml` entries

**Delete (2):**
- `cpp/qml/AppUI/GamesPage.qml` — dead, orphaned by theme system
- `cpp/qml/AppUI/GameGridView.qml` — dead, only consumer was GamesPage

---

## Task 1: Create `GenericListPage.qml` and wire into CMake

**Files:**
- Create: `cpp/qml/AppUI/GenericListPage.qml`
- Modify: `cpp/CMakeLists.txt:343-382` (the AppUI module's `QML_FILES` list)

- [ ] **Step 1: Write `GenericListPage.qml`**

Create `cpp/qml/AppUI/GenericListPage.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true

    // --- Required ---
    property alias model: listView.model
    required property Component delegate

    // --- Optional content ---
    property string headerText: ""
    property string emptyText: ""
    property Component listFooter: null

    // --- Optional layout knobs ---
    property real listMargins: 20
    property real itemSpacing: 6

    // --- Signals ---
    signal activated(int index)
    signal backRequested()

    // Public API for delegate click handlers
    function activate(index) {
        listView.currentIndex = index
        activated(index)
    }

    // Header
    Text {
        id: headerLabel
        visible: root.headerText.length > 0
        text: root.headerText
        color: SettingsTheme.text
        font.pixelSize: 18
        font.weight: Font.Bold
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.leftMargin: root.listMargins
    }

    // List
    ListView {
        id: listView
        anchors.top: headerLabel.visible ? headerLabel.bottom : parent.top
        anchors.topMargin: headerLabel.visible ? 16 : 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.listMargins
        anchors.bottomMargin: root.listMargins
        spacing: root.itemSpacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        focus: true
        currentIndex: 0
        delegate: root.delegate
        footer: root.listFooter

        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
    }

    // Empty state
    Text {
        anchors.centerIn: listView
        visible: root.emptyText.length > 0 && listView.count === 0
        text: root.emptyText
        color: SettingsTheme.textDim
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
    }

    // Keyboard navigation
    Keys.onUpPressed: {
        if (listView.count > 0)
            listView.currentIndex = listView.currentIndex > 0
                ? listView.currentIndex - 1
                : listView.count - 1
    }
    Keys.onDownPressed: {
        if (listView.count > 0)
            listView.currentIndex = listView.currentIndex < listView.count - 1
                ? listView.currentIndex + 1
                : 0
    }
    Keys.onReturnPressed: if (listView.count > 0) root.activate(listView.currentIndex)
    Keys.onEnterPressed:  if (listView.count > 0) root.activate(listView.currentIndex)
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            event.accepted = true
            root.backRequested()
        }
    }

    // Default back handler — pops panelStack if available
    onBackRequested: {
        if (typeof panelStack !== 'undefined' && panelStack.depth > 1)
            panelStack.pop()
    }
}
```

- [ ] **Step 2: Register the new file in CMakeLists.txt**

Open `cpp/CMakeLists.txt`. After line 343 (`qml/AppUI/ButtonHints.qml`), add `qml/AppUI/GenericListPage.qml` so the AppUI module picks it up. Alphabetical-ish ordering not enforced; just add the line:

```cmake
        qml/AppUI/ButtonHints.qml
        qml/AppUI/GenericListPage.qml
        qml/AppUI/FocusableItem.qml
```

- [ ] **Step 3: Build to confirm the new QML compiles**

Run from the repo root:

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. Any QML compile error from `GenericListPage.qml` (e.g. typos, missing imports) shows up here.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/GenericListPage.qml cpp/CMakeLists.txt
git commit -m "feat(ui): add GenericListPage QML component

Skeleton component for collapsing AllGamesPage, RecentlyPlayedPage,
EmulatorManageGrid, ThemesPage behind one shared list shell. Owns
ListView + wrap-around arrow nav + Enter/Return activation + Escape
backRequested + optional header/empty/footer.

Not yet consumed by any page; migrations follow.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Migrate `ThemesPage.qml` (cleanest 1:1 swap)

**Why first:** ThemesPage already has wrap-around focus nav, already uses `ListView.currentIndex`, and its delegate already renders a focused-state border. The migration is a clean structural swap — no new visual states to design — which proves the GenericListPage API works before we add new behavior elsewhere.

**Files:**
- Modify: `cpp/qml/AppUI/ThemesPage.qml` (full rewrite, 227 → ~100 LOC)

- [ ] **Step 1: Read the current file to keep the delegate body intact**

```bash
sed -n '40,210p' cpp/qml/AppUI/ThemesPage.qml
```

The delegate body (color preview + theme info + Active/Apply badge) stays as-is. Only the focused-state source changes from `index === root.focusIndex` to `ListView.isCurrentItem`.

- [ ] **Step 2: Replace `ThemesPage.qml` with the migrated version**

Overwrite `cpp/qml/AppUI/ThemesPage.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    function applyTheme(idx) {
        var theme = themeManager.availableThemes[idx]
        if (theme && themeManager.currentThemeId !== theme.id) {
            themeManager.currentThemeId = theme.id
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        GenericListPage {
            id: listPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            listMargins: 16
            itemSpacing: 8
            model: themeManager.availableThemes
            onActivated: (index) => root.applyTheme(index)

            delegate: Item {
                id: delegateRoot
                width: ListView.view.width
                height: 72

                property bool isFocused: ListView.isCurrentItem
                property bool isActive:  modelData.id === themeManager.currentThemeId

                // Glow behind
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: SettingsTheme.cardRadius + 4
                    color: "transparent"
                    border.width: 2
                    border.color: SettingsTheme.focusBorder
                    opacity: delegateRoot.isFocused ? 0.3 : 0
                    z: -1
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
                }

                Rectangle {
                    id: rowRect
                    anchors.fill: parent
                    radius: SettingsTheme.cardRadius
                    color: delegateRoot.isActive
                        ? Qt.rgba(0.91, 0.66, 0.22, 0.06)
                        : SettingsTheme.card

                    border.width: delegateRoot.isFocused ? 2 : 1
                    border.color: delegateRoot.isFocused
                        ? SettingsTheme.focusBorder
                        : (rowHover.containsMouse ? SettingsTheme.textGhost : SettingsTheme.border)

                    Behavior on border.color { ColorAnimation { duration: SettingsTheme.animFast } }
                    Behavior on border.width { NumberAnimation { duration: SettingsTheme.animFast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 16

                        // Color preview swatch
                        Rectangle {
                            width: 64
                            height: 48
                            radius: 6
                            border.width: 1
                            border.color: SettingsTheme.border
                            clip: true

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#1a1917" }
                                    GradientStop { position: 1.0; color: "#0d0c0a" }
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 10
                                radius: 0
                                color: SettingsTheme.accent
                                opacity: 0.6
                            }
                        }

                        // Theme info
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: modelData.name || modelData.id
                                color: SettingsTheme.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: modelData.description || ""
                                color: SettingsTheme.textDim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text.length > 0
                            }

                            Text {
                                text: modelData.author ? "by " + modelData.author : ""
                                color: SettingsTheme.textFaint
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text.length > 0
                            }
                        }

                        // Active badge or Apply button
                        Loader {
                            active: true
                            sourceComponent: delegateRoot.isActive ? activeBadge : applyButton

                            Component {
                                id: activeBadge
                                Row {
                                    spacing: 6
                                    Rectangle {
                                        width: 8; height: 8; radius: 4
                                        color: SettingsTheme.success
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        text: "Active"
                                        color: SettingsTheme.success
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }

                            Component {
                                id: applyButton
                                Rectangle {
                                    width: 70; height: 32
                                    radius: SettingsTheme.buttonRadius
                                    color: SettingsTheme.card
                                    border.width: 1
                                    border.color: SettingsTheme.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: "Apply"
                                        color: SettingsTheme.textMuted
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                    }
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: rowHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: listPage.activate(index)
                    }
                }
            }
        }

        ButtonHints {
            Layout.fillWidth: true
            hints: [
                {action: "navigate_ud", label: "Navigate"},
                {action: "confirm",     label: "Apply Theme"},
                {action: "back",        label: "Back"}
            ]
        }
    }
}
```

- [ ] **Step 2.5: Verify the page is rebuilt with no QML compile errors**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds.

- [ ] **Step 3: Smoke-test the Themes page in the running app**

```bash
open ./cpp/build-x86_64/RetroNest.app
```

In the app: open the in-game menu (Cmd+Shift+Escape from any context, or use the settings overlay) → **Settings → Themes**.

Verify:
1. Arrow Up/Down wraps around top↔bottom
2. Focused row shows the accent-colored 2px border + soft outer glow
3. Pressing Enter (or A on a controller) applies the focused theme — the "Active" indicator moves
4. Mouse click on a row also applies (and moves focus to that row)
5. `ButtonHints` appears below the list as before
6. Escape/B from this page leaves it (panelStack.pop behavior unchanged)

If any of these fail, fix before committing.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ThemesPage.qml
git commit -m "refactor(ui): migrate ThemesPage onto GenericListPage

ThemesPage now wraps GenericListPage + ButtonHints in a ColumnLayout.
Removes the duplicated wrap-around arrow-key handler, focusIndex state,
and positionViewAtIndex onCurrentIndexChanged plumbing — all now lives
in GenericListPage. Delegate switches focused-state source from
'index === root.focusIndex' to 'ListView.isCurrentItem'.

Net −120 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Migrate `EmulatorManageGrid.qml` (tests `listFooter` and external signal)

**Why second:** EmulatorManageGrid is the only page that uses the `listFooter` prop (the "Coming Soon" non-data row). Doing it second proves the footer wiring works before we tackle the achievement pages.

**Files:**
- Modify: `cpp/qml/AppUI/EmulatorManageGrid.qml` (full rewrite, 234 → ~110 LOC)

- [ ] **Step 1: Replace `EmulatorManageGrid.qml`**

Overwrite `cpp/qml/AppUI/EmulatorManageGrid.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import "EmulatorLogos.js" as EmulatorLogos

Item {
    id: root

    signal emulatorSelected(string emuId)

    property var emuList: app.allEmulatorStatus()

    Connections {
        target: app
        function onEmulatorInstalled() {
            root.emuList = app.allEmulatorStatus()
        }
    }

    GenericListPage {
        id: listPage
        anchors.fill: parent
        listMargins: 20
        itemSpacing: SettingsTheme.itemSpacing
        model: root.emuList
        onActivated: (index) => {
            if (root.emuList.length > 0)
                root.emulatorSelected(root.emuList[index].id)
        }

        delegate: FocusableItem {
            id: rowItem
            width: ListView.view.width - 40
            x: 20
            height: 108
            isFocused: ListView.isCurrentItem

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 18

                // Logo area
                Rectangle {
                    width: 72
                    height: 72
                    radius: 10
                    color: SettingsTheme.border

                    Image {
                        id: logoImg
                        anchors.fill: parent
                        source: EmulatorLogos.logoForEmu(modelData.id)
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                        visible: false
                    }

                    Rectangle {
                        id: logoMask
                        anchors.fill: parent
                        radius: 10
                        visible: false
                    }

                    OpacityMask {
                        anchors.fill: parent
                        source: logoImg
                        maskSource: logoMask
                        visible: EmulatorLogos.logoForEmu(modelData.id) !== ""
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "🎮"
                        font.pixelSize: 32
                        visible: EmulatorLogos.logoForEmu(modelData.id) === ""
                    }
                }

                // Name / system / description
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: modelData.name || modelData.id
                        color: SettingsTheme.text
                        font.pixelSize: 22
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: modelData.system || ""
                        color: SettingsTheme.textDim
                        font.pixelSize: 16
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        visible: text !== ""
                    }
                    Text {
                        text: modelData.description || ""
                        color: SettingsTheme.textFaint
                        font.pixelSize: 15
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        visible: text !== ""
                    }
                }

                // Install status badge
                Rectangle {
                    width: badgeLabel.width + 28
                    height: 36
                    radius: SettingsTheme.pillRadius
                    color: modelData.installed ? SettingsTheme.successDim : SettingsTheme.accentDim

                    Text {
                        id: badgeLabel
                        anchors.centerIn: parent
                        text: modelData.installed ? "Installed" : "Not Installed"
                        color: modelData.installed ? SettingsTheme.success : SettingsTheme.accent
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                }

                // Chevron
                Text {
                    text: "›"
                    color: SettingsTheme.textGhost
                    font.pixelSize: 26
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onContainsMouseChanged: parent.isHovered = containsMouse
                onClicked: listPage.activate(index)
            }
        }

        listFooter: Component {
            FocusableItem {
                width: ListView.view ? ListView.view.width - 40 : 0
                x: 20
                height: 108
                isFocused: false
                opacity: 0.35

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 18

                    Rectangle {
                        width: 72
                        height: 72
                        radius: 10
                        color: SettingsTheme.border

                        Text {
                            anchors.centerIn: parent
                            text: "➕"
                            font.pixelSize: 32
                            color: SettingsTheme.textGhost
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: "More Emulators"
                            color: SettingsTheme.text
                            font.pixelSize: 22
                            font.weight: Font.Medium
                        }
                        Text {
                            text: "Coming Soon"
                            color: SettingsTheme.textDim
                            font.pixelSize: 16
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "›"
                        color: SettingsTheme.textGhost
                        font.pixelSize: 26
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds.

- [ ] **Step 3: Smoke-test Manage Emulators in the running app**

```bash
open ./cpp/build-x86_64/RetroNest.app
```

In the app: **Settings → Manage Emulators**.

Verify:
1. Arrow Up/Down moves focus through emulator rows with wrap-around
2. Focused row shows the `FocusableItem` highlight (existing visual, now driven by `ListView.isCurrentItem`)
3. "Coming Soon" row is visible at the bottom and is NOT focusable (arrow keys can't land on it — `ListView.footer` is outside the navigation model)
4. Pressing Enter / A drills into the focused emulator (`emulatorSelected` signal fires; the EmulatorDetailPage opens)
5. Mouse click on a row also drills in
6. After installing an emulator from the detail page, returning to this grid shows the updated "Installed" badge (the `Connections { onEmulatorInstalled }` still fires)

If the footer is reachable by arrow keys, fall back to rendering "Coming Soon" as a second delegate appended to the model array — see Open Risks in the spec.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/EmulatorManageGrid.qml
git commit -m "refactor(ui): migrate EmulatorManageGrid onto GenericListPage

Removes the bespoke Flickable + Repeater + focusIndex + wrap-around
Keys.on* block. 'Coming Soon' placeholder moves into the
GenericListPage listFooter slot, which is rendered after the last
delegate but is not part of the focus model — preserving the
'visible-but-not-focusable' behavior.

Net −124 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: Migrate `RecentlyPlayedPage.qml` (gains focus nav)

**Files:**
- Modify: `cpp/qml/AppUI/RecentlyPlayedPage.qml` (full rewrite, 131 → ~75 LOC)

- [ ] **Step 1: Replace `RecentlyPlayedPage.qml`**

Overwrite `cpp/qml/AppUI/RecentlyPlayedPage.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import AppUI

GenericListPage {
    id: listPage

    property var recentGames: []

    model: recentGames
    headerText: recentGames.length + " recently played games"
    emptyText: "No recently played games"
    itemSpacing: 6

    onActivated: (index) => {
        var g = recentGames[index]
        if (g && g.gameId > 0 && typeof panelStack !== 'undefined')
            panelStack.push(achievementsPageComponent, { raGameId: g.gameId, gameTitle: g.title })
    }

    delegate: Rectangle {
        id: rowRect
        required property var modelData
        required property int index

        width: ListView.view.width
        height: 60
        radius: 10
        color: SettingsTheme.card
        border.width: ListView.isCurrentItem ? 2 : 1
        border.color: ListView.isCurrentItem ? SettingsTheme.focusBorder : SettingsTheme.border

        Behavior on border.color { ColorAnimation { duration: SettingsTheme.animFast } }
        Behavior on border.width { NumberAnimation { duration: SettingsTheme.animFast } }

        // Focus glow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.width: 2
            border.color: SettingsTheme.focusBorder
            opacity: rowRect.ListView.isCurrentItem ? 0.3 : 0
            z: -1
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12

            // Game icon
            Rectangle {
                width: 40; height: 40; radius: 8
                color: SettingsTheme.surface

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: modelData.imageIcon ? "https://retroachievements.org" + modelData.imageIcon : ""
                    fillMode: Image.PreserveAspectFit
                    visible: status === Image.Ready
                }
            }

            // Title and console
            Column {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: modelData.title || ""
                    color: SettingsTheme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    width: parent.width
                }

                Text {
                    text: modelData.consoleName || ""
                    color: SettingsTheme.textDim
                    font.pixelSize: 11
                }
            }

            // Achievement count + progress
            Column {
                Layout.alignment: Qt.AlignRight
                spacing: 2

                Text {
                    text: (modelData.numAchieved || 0) + " / " + (modelData.numPossible || 0)
                    color: SettingsTheme.textMuted
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight
                }

                Rectangle {
                    width: 80; height: 3; radius: 2
                    color: SettingsTheme.border

                    Rectangle {
                        width: parent.width * Math.min(1, (modelData.numAchieved || 0) / Math.max(1, modelData.numPossible || 1))
                        height: parent.height; radius: 2
                        color: SettingsTheme.accent
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: listPage.activate(rowRect.index)
        }
    }
}
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds.

- [ ] **Step 3: Smoke-test Recently Played in the running app**

```bash
open ./cpp/build-x86_64/RetroNest.app
```

In the app: **Achievements panel → Recently Played**.

Verify:
1. Header reads "N recently played games" in 18px Bold (visual upgrade vs the old 12px dim header)
2. Arrow Up/Down moves focus with wrap-around — **new behavior**
3. Focused row gains accent border + glow — **new behavior**
4. Enter / A drills into that game's achievements (push achievementsPageComponent with `raGameId: gameId`)
5. Mouse click still drills in
6. Escape / B pops back out
7. Empty-state text "No recently played games" appears if there are no recent games

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/RecentlyPlayedPage.qml
git commit -m "refactor(ui): migrate RecentlyPlayedPage onto GenericListPage

Gains controller/keyboard focus navigation (previously mouse-only).
Header upgrades from 12px dim ListView.header to 18px Bold above the
list for consistency with AllGamesPage. Delegate gains focused-state
border + glow matching the pattern in ThemesPage.

Net −56 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Migrate `AllGamesPage.qml` (gains focus nav)

**Files:**
- Modify: `cpp/qml/AppUI/AllGamesPage.qml` (full rewrite, 131 → ~75 LOC)

- [ ] **Step 1: Replace `AllGamesPage.qml`**

Overwrite `cpp/qml/AppUI/AllGamesPage.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import AppUI

GenericListPage {
    id: listPage

    property var allGames: []

    model: allGames
    headerText: "All Games (" + allGames.length + ")"
    emptyText: "No games"
    itemSpacing: 6

    onActivated: (index) => {
        var g = allGames[index]
        if (g && typeof panelStack !== 'undefined')
            panelStack.push(achievementsPageComponent, { raGameId: g.raGameId, gameTitle: g.title })
    }

    delegate: Rectangle {
        id: rowRect
        required property var modelData
        required property int index

        width: ListView.view.width
        height: 56
        radius: 8
        color: gameMa.containsMouse ? Qt.lighter(SettingsTheme.card, 1.15) : SettingsTheme.card
        border.width: ListView.isCurrentItem ? 2 : 1
        border.color: ListView.isCurrentItem ? SettingsTheme.focusBorder : SettingsTheme.border

        Behavior on border.color { ColorAnimation { duration: SettingsTheme.animFast } }
        Behavior on border.width { NumberAnimation { duration: SettingsTheme.animFast } }

        // Focus glow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.width: 2
            border.color: SettingsTheme.focusBorder
            opacity: rowRect.ListView.isCurrentItem ? 0.3 : 0
            z: -1
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }
        }

        MouseArea {
            id: gameMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: listPage.activate(rowRect.index)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 12

            // Game icon
            Rectangle {
                width: 40
                height: 40
                radius: 6
                color: SettingsTheme.surface

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: modelData.imageIcon ? "https://retroachievements.org" + modelData.imageIcon : ""
                    fillMode: Image.PreserveAspectFit
                    visible: status === Image.Ready
                }
            }

            // Title + console
            Column {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: modelData.title || ""
                    color: SettingsTheme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    width: parent.width
                }

                Text {
                    text: modelData.consoleName || ""
                    color: SettingsTheme.textDim
                    font.pixelSize: 11
                }
            }

            // Achievement count
            Text {
                text: (modelData.numAwarded || 0) + " / " + (modelData.numAchievements || 0)
                color: SettingsTheme.textMuted
                font.pixelSize: 12
            }

            // Progress bar
            Rectangle {
                width: 60
                height: 4
                radius: 2
                color: SettingsTheme.border

                Rectangle {
                    width: parent.width * Math.min(1, (modelData.numAwarded || 0) / Math.max(1, modelData.numAchievements || 1))
                    height: parent.height
                    radius: 2
                    color: modelData.mastered ? SettingsTheme.success : SettingsTheme.accent
                }
            }
        }
    }
}
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds.

- [ ] **Step 3: Smoke-test All Games in the running app**

```bash
open ./cpp/build-x86_64/RetroNest.app
```

In the app: **Achievements panel → All Games** (entry point is `RetroAchievementsSettings.qml:632`).

Verify:
1. Header reads "All Games (N)" in 18px Bold
2. Arrow Up/Down moves focus with wrap-around — **new behavior**
3. Focused row gains accent border + glow — **new behavior**
4. Enter / A drills into that game's achievements (push achievementsPageComponent with `raGameId: raGameId`)
5. Mouse click still drills in (and now also sets focus to the clicked row)
6. Hover lightening still works (`gameMa.containsMouse` lightens the card)
7. Escape / B pops back out

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/AllGamesPage.qml
git commit -m "refactor(ui): migrate AllGamesPage onto GenericListPage

Gains controller/keyboard focus navigation (previously mouse-only).
Title moves from a free Text above the ListView into GenericListPage's
headerText. Delegate gains focused-state border + glow; mouse hover
lightening preserved.

Net −56 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Delete the orphaned `GamesPage.qml` + `GameGridView.qml`

**Files:**
- Delete: `cpp/qml/AppUI/GamesPage.qml`
- Delete: `cpp/qml/AppUI/GameGridView.qml`
- Modify: `cpp/CMakeLists.txt:349,351` (remove the two entries)

- [ ] **Step 1: Confirm zero live references before deletion**

Run a fresh grep across live source (not build cache):

```bash
grep -rn "GamesPage\|GameGridView" \
  cpp/qml/ cpp/src/ cpp/themes/ themes/ \
  --include="*.qml" --include="*.cpp" --include="*.h" --include="*.mm" --include="*.json" \
  2>/dev/null
```

Expected: only `cpp/qml/AppUI/GamesPage.qml:84` (the file's own self-reference to `GameGridView`) appears. No other matches. If any other file references either name, STOP and investigate — do not delete.

- [ ] **Step 2: Remove the two lines from `CMakeLists.txt`**

Open `cpp/CMakeLists.txt`. Delete these two lines (currently lines 349 and 351):

```cmake
        qml/AppUI/GamesPage.qml
```
and:
```cmake
        qml/AppUI/GameGridView.qml
```

- [ ] **Step 3: Delete the files**

```bash
git rm cpp/qml/AppUI/GamesPage.qml cpp/qml/AppUI/GameGridView.qml
```

- [ ] **Step 4: Build to confirm nothing was secretly depending on them**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. If any reference to `GamesPage` or `GameGridView` was missed by the grep (e.g. in a generated source), the linker/QML compiler will catch it here.

- [ ] **Step 5: Commit**

```bash
git add cpp/CMakeLists.txt
git commit -m "chore(ui): delete dead GamesPage.qml and GameGridView.qml

Both orphaned by the theme system — themes/modern/GameListPage.qml
is the live games browser. Confirmed via grep: zero live references
outside the files' self-reference.

Net −136 LOC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: Final full smoke test and memory update

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`

- [ ] **Step 1: Run the full smoke test from the spec**

Launch the app:

```bash
open ./cpp/build-x86_64/RetroNest.app
```

Run through all four pages in one session:

1. **Achievements → All Games**: arrow keys wrap, focused row has accent border + glow, Enter drills in, Escape pops out
2. **Achievements → Recently Played**: same as above; empty-state appears if no recent games
3. **Settings → Manage Emulators**: arrow keys wrap, "Coming Soon" row visible at bottom but not focusable, Enter drills in, mouse click works
4. **Settings → Themes**: arrow keys + Enter applies a theme, ButtonHints visible below, mouse click applies

If any step fails, FIX before continuing.

- [ ] **Step 2: Update the refactor-roadmap memory**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`:

Find the line:
```
4. **`GenericListPage.qml`** — `cpp/qml/AppUI/GamesPage.qml`, `AllGamesPage.qml`, ...
```

Replace it with:
```
4. ✅ **`GenericListPage.qml`** — shipped 2026-05-20. New `cpp/qml/AppUI/GenericListPage.qml` owns ListView + wrap-around arrow nav + Enter activation + Escape back + optional header/empty/listFooter. AllGamesPage, RecentlyPlayedPage, EmulatorManageGrid, ThemesPage migrated. Dead GamesPage.qml + GameGridView.qml deleted. Achievement pages gained controller focus nav for the first time. Net −459 LOC.
```

Also update the front-matter description to reflect Tier 1 items 1–4 are now shipped.

- [ ] **Step 3: Commit the memory update**

The memory file lives outside the repo, so no git commit needed there. The file edit is enough — future sessions will see the updated state.

---

## Self-review

**Spec coverage:**
- API (component properties, signals, internal behavior) → Task 1 ✓
- Per-page conversion for all 4 pages → Tasks 2–5 ✓
- Focused-state visual contract for AllGames + RecentlyPlayed → Tasks 4, 5 (focus glow + 2px focusBorder) ✓
- Mouse interaction contract (activate() routes both paths) → all migration tasks use `listPage.activate(index)` from MouseArea ✓
- Dead code removal → Task 6 ✓
- Smoke-test checklist → Task 7 ✓

**Placeholder scan:** No TBDs, no "implement later", no "similar to Task N". Every code step contains the full file content the engineer needs.

**Type / name consistency:**
- `function activate(index)` is the same in Task 1 (definition) and Tasks 2–5 (call sites: `listPage.activate(index)` or `listPage.activate(rowRect.index)`)
- `signal activated(int index)` matches all `onActivated:` handlers
- `headerText` / `emptyText` / `listFooter` / `listMargins` / `itemSpacing` property names match between API and call sites
- `ListView.isCurrentItem` (Qt attached property) used consistently for focused-state in all delegates
- `panelStack.depth > 1` check guards the default back handler (Task 1) and is not duplicated in pages — pages rely on the default

**One known-fragile spot:** Task 3's `listFooter` Component uses `ListView.view.width - 40` for sizing. If `ListView.view` isn't available at footer-creation time (rare edge case), the spec's open-risk fallback kicks in: render Coming Soon as a real model entry instead. Documented in the spec's Open Risks section.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-20-generic-list-page.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Best for catching delegate-layout regressions early since each migration page is verified independently before moving on.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
