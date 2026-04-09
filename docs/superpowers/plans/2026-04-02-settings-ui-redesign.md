# Settings UI & Setup Wizard Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign all settings UI pages and the setup wizard to be controller/keyboard friendly with a warm charcoal + amber color palette, right slide panel layout, and smooth animations.

**Architecture:** Pure QML redesign — no C++ changes. A new `SettingsTheme.qml` singleton provides the color palette for settings pages. A `FocusableItem.qml` component provides shared focus/glow behavior. The settings overlay becomes a right-anchored slide panel with replace-in-place navigation. The setup wizard becomes a wide centered card with progress bar. All pages get controller hints and D-pad/keyboard focus navigation.

**Tech Stack:** Qt6 QML, QtQuick.Controls, QtQuick.Layouts

**Design Spec:** `docs/superpowers/specs/2026-04-02-settings-ui-redesign-design.md`

**Visual Mockups:** `.superpowers/brainstorm/` directory contains HTML mockups for reference.

---

### Task 1: Settings Theme Singleton

Create the shared color palette singleton for the settings UI.

**Files:**
- Create: `cpp/qml/AppUI/SettingsTheme.qml`
- Modify: `cpp/qml/AppUI/qmldir` (if it exists, to register the singleton)

- [ ] **Step 1: Create the SettingsTheme singleton**

Create `cpp/qml/AppUI/SettingsTheme.qml`:

```qml
pragma Singleton
import QtQuick

QtObject {
    // Backgrounds
    readonly property color background:    "#131210"
    readonly property color base:          "#1a1917"
    readonly property color surface:       "#201f1c"
    readonly property color card:          "#282621"
    readonly property color border:        "#353330"

    // Accent & status
    readonly property color accent:        "#e8a838"
    readonly property color accentDim:     "#2a2518"
    readonly property color success:       "#6a9b4a"
    readonly property color successDim:    "#1e2a1a"
    readonly property color error:         "#c85040"
    readonly property color errorDim:      "#2a1a18"
    readonly property color warning:       "#aa8844"
    readonly property color warningDim:    "#2a2518"

    // Text
    readonly property color text:          "#e0ddd6"
    readonly property color textMuted:     "#8a8680"
    readonly property color textDim:       "#6a6660"
    readonly property color textFaint:     "#5a5650"
    readonly property color textGhost:     "#4a4640"

    // Focus glow
    readonly property color focusBorder:   "#e8a838"
    readonly property real focusGlowRadius: 15
    readonly property color focusGlow:     Qt.rgba(0.91, 0.66, 0.22, 0.3)

    // Sizing
    readonly property int panelWidthPercent: 50
    readonly property int cardRadius:      10
    readonly property int buttonRadius:    8
    readonly property int pillRadius:      20
    readonly property int itemSpacing:     8
    readonly property int sectionSpacing:  20

    // Animation durations (ms)
    readonly property int animFast:        150
    readonly property int animNormal:      200
    readonly property int animSlide:       250
}
```

- [ ] **Step 2: Register in qmldir (if applicable)**

Check if `cpp/qml/AppUI/qmldir` exists. If it does, add:
```
singleton SettingsTheme 1.0 SettingsTheme.qml
```

If no qmldir exists, the singleton will be available via the `pragma Singleton` directive and QML module auto-discovery.

- [ ] **Step 3: Verify it builds**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds with no QML errors.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/SettingsTheme.qml
git commit -m "feat: add SettingsTheme singleton with warm charcoal + amber palette"
```

---

### Task 2: Shared Components — ControllerHints and FocusableItem

Create reusable components for focus glow behavior and controller hint bars.

**Files:**
- Create: `cpp/qml/AppUI/ControllerHints.qml`
- Create: `cpp/qml/AppUI/FocusableItem.qml`

- [ ] **Step 1: Create ControllerHints component**

Create `cpp/qml/AppUI/ControllerHints.qml`:

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    height: 40

    // Pass hints as an array of {icon: "🅰", text: "Select"} objects
    property var hints: []

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: SettingsTheme.border
        }

        RowLayout {
            anchors.centerIn: parent
            spacing: 20

            Repeater {
                model: root.hints

                Text {
                    text: modelData.icon + " " + modelData.text
                    color: SettingsTheme.textGhost
                    font.pixelSize: 10
                }
            }
        }
    }
}
```

- [ ] **Step 2: Create FocusableItem component**

Create `cpp/qml/AppUI/FocusableItem.qml`:

```qml
import QtQuick

Rectangle {
    id: root

    property bool isFocused: false
    property bool isHovered: false

    radius: SettingsTheme.cardRadius
    color: SettingsTheme.card

    border.width: isFocused ? 2 : 1
    border.color: isFocused ? SettingsTheme.focusBorder
                             : (isHovered ? SettingsTheme.textGhost : SettingsTheme.border)

    layer.enabled: isFocused
    layer.effect: null  // We'll use box-shadow equivalent via states

    // Glow effect using a shadow Rectangle behind
    Rectangle {
        id: glowRect
        anchors.fill: parent
        anchors.margins: -4
        radius: parent.radius + 4
        color: "transparent"
        border.width: 2
        border.color: SettingsTheme.focusBorder
        opacity: root.isFocused ? 0.3 : 0
        z: -1
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: SettingsTheme.animFast }
        }
    }

    Behavior on border.color {
        ColorAnimation { duration: SettingsTheme.animFast }
    }

    Behavior on border.width {
        NumberAnimation { duration: SettingsTheme.animFast }
    }

    // Mouse hover support
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.NoButton  // Pass clicks through — parent handles them
        onContainsMouseChanged: root.isHovered = containsMouse
    }
}
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ControllerHints.qml cpp/qml/AppUI/FocusableItem.qml
git commit -m "feat: add ControllerHints and FocusableItem shared components"
```

---

### Task 3: Settings Overlay — Right Slide Panel Shell

Rewrite the SettingsOverlay from a centered modal to a right slide panel with vertical category list and replace-in-place navigation.

**Files:**
- Rewrite: `cpp/qml/AppUI/SettingsOverlay.qml`
- Modify: `cpp/qml/AppUI/AppWindow.qml` (update Escape key handler for new navigation)

- [ ] **Step 1: Read current files**

Read the full contents of:
- `cpp/qml/AppUI/SettingsOverlay.qml`
- `cpp/qml/AppUI/AppWindow.qml`

Note all backend connections: `app.*`, `themeManager.*` calls, signals, and the `navigateToEmulator()` function.

- [ ] **Step 2: Rewrite SettingsOverlay.qml**

Replace the entire contents of `cpp/qml/AppUI/SettingsOverlay.qml` with the new slide panel design. Key requirements:

**Structure:**
```
Rectangle (full-screen overlay, semi-transparent dimmer on left side)
├── MouseArea (click left side to close)
└── Rectangle (panel, anchored to right edge, 50% width, full height)
    ├── Slide-in animation (x property, 250ms ease-out)
    └── ColumnLayout
        ├── Header (back button + title)
        ├── StackView (id: panelStack)
        │   ├── Initial: CategoryList (vertical list of categories)
        │   └── Push/pop: content pages
        └── ControllerHints
```

**Critical details to preserve:**
- `id: overlay` (referenced by AppWindow and EmulatorManagePage)
- `property int selectedCategory: -1` — keep for compatibility, but drive from StackView depth
- `property string targetEmuId: ""` — keep for `navigateToEmulator()`
- `function toggle()`, `function open()`, `function close()` — keep signatures
- `function navigateToEmulator(emuId)` — keep signature, push EmulatorManagePage directly

**Category list items** (as a Component defined inline or in the initialItem):
- Use `FocusableItem` for each row
- Track `currentFocusIndex` for D-pad navigation
- Keys.onUpPressed / Keys.onDownPressed to move focus
- Keys.onReturnPressed / Keys.onPressed (gamepad A) to select

**Slide animation:**
```qml
Rectangle {
    id: panel
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.right: parent.right
    width: parent.width * SettingsTheme.panelWidthPercent / 100
    color: SettingsTheme.surface
    x: overlay.visible ? 0 : panel.width  // Slide from right

    Behavior on x {
        NumberAnimation {
            duration: SettingsTheme.animSlide
            easing.type: Easing.OutCubic
        }
    }
}
```

**StackView transitions** (replace-in-place):
```qml
StackView {
    id: panelStack
    pushEnter: Transition {
        NumberAnimation { property: "x"; from: panelStack.width; to: 0; duration: SettingsTheme.animNormal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: SettingsTheme.animNormal }
    }
    pushExit: Transition {
        NumberAnimation { property: "x"; from: 0; to: -panelStack.width * 0.3; duration: SettingsTheme.animNormal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SettingsTheme.animNormal }
    }
    popEnter: Transition {
        NumberAnimation { property: "x"; from: -panelStack.width * 0.3; to: 0; duration: SettingsTheme.animNormal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: SettingsTheme.animNormal }
    }
    popExit: Transition {
        NumberAnimation { property: "x"; from: 0; to: panelStack.width; duration: SettingsTheme.animNormal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SettingsTheme.animNormal }
    }
}
```

**Category pages mapping** — when a category is selected, push the corresponding component:
- Index 0: EmulatorManagePage
- Index 1: PathsSettings
- Index 2: ScraperSettings
- Index 3: Themes page (can be inline or a new component)

**Themes page** — currently inline in SettingsOverlay. Extract the theme ListView into the push target. Must preserve:
- `model: themeManager.availableThemes`
- `themeManager.currentThemeId` read/write
- Each row: preview swatch + name + description + author + Apply/Active indicator
- Style with SettingsTheme colors and FocusableItem for rows

- [ ] **Step 3: Update AppWindow.qml Escape handler**

The Escape handler needs to work with StackView instead of selectedCategory:

```qml
Shortcut {
    sequence: "Escape"
    onActivated: {
        if (settingsOverlay.visible) {
            if (settingsOverlay.canGoBack()) {
                settingsOverlay.goBack()
            } else {
                settingsOverlay.close()
                app.setCursorVisible(false)
            }
        } else {
            settingsOverlay.open()
            app.setCursorVisible(true)
        }
    }
}
```

Add `canGoBack()`, `goBack()`, and focus restore to SettingsOverlay:
```qml
property int savedFocusIndex: -1

function canGoBack() {
    return panelStack.depth > 1
}

function goBack() {
    panelStack.pop()
    // Restore focus index from before the push
    if (savedFocusIndex >= 0) {
        categoryList.focusIndex = savedFocusIndex
        savedFocusIndex = -1
    }
}
```

When pushing a category page, save the current focus index:
```qml
// In category selection handler:
savedFocusIndex = categoryList.focusIndex
panelStack.push(componentForCategory)
```

- [ ] **Step 4: Build and test**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```

Then launch to verify:
```bash
./build/EmulatorFrontend
```
- Press Escape: panel should slide in from right
- Click a category: content should slide in, replacing category list
- Press Escape/B: should go back to category list
- Press Escape/B again: panel should slide out

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/SettingsOverlay.qml cpp/qml/AppUI/AppWindow.qml
git commit -m "feat: replace settings modal with right slide panel"
```

---

### Task 4: Emulator Manage Page — List View Redesign

Rewrite EmulatorManagePage and EmulatorManageGrid to use a vertical list layout with the new palette.

**Files:**
- Rewrite: `cpp/qml/AppUI/EmulatorManagePage.qml`
- Rewrite: `cpp/qml/AppUI/EmulatorManageGrid.qml`

- [ ] **Step 1: Read current files**

Read the full contents of:
- `cpp/qml/AppUI/EmulatorManagePage.qml`
- `cpp/qml/AppUI/EmulatorManageGrid.qml`

Note: EmulatorManagePage uses a StackLayout switching between grid (index 0) and detail (index 1). Keep this pattern but update the grid to a list.

- [ ] **Step 2: Rewrite EmulatorManageGrid.qml as a vertical list**

Replace the Flow-based card grid with a vertical list. Key requirements:

**Each emulator row must contain:**
- Logo area (48x48px, `SettingsTheme.border` background, rounded 8px)
- Name (15px, medium weight, `SettingsTheme.text`)
- System (12px, `SettingsTheme.textDim`)
- Description (11px, `SettingsTheme.textFaint`)
- Install status badge:
  - Installed: `SettingsTheme.success` text on `SettingsTheme.successDim` bg
  - Not Installed: `SettingsTheme.accent` text on `SettingsTheme.accentDim` bg
- Chevron `›` in `SettingsTheme.textGhost`

**Focus management:**
- `property int focusIndex: 0`
- `FocusableItem` for each row with `isFocused: index === root.focusIndex`
- Keys.onUpPressed: `focusIndex = focusIndex > 0 ? focusIndex - 1 : count - 1` (wraps to bottom)
- Keys.onDownPressed: `focusIndex = focusIndex < count - 1 ? focusIndex + 1 : 0` (wraps to top)
- Keys.onReturnPressed: emit `emulatorSelected(emuList[focusIndex].id)`
- MouseArea onClick: set focusIndex and emit signal

**Preserve:**
- `signal emulatorSelected(string emuId)`
- `property var emuList: app.allEmulatorStatus()`
- `Connections { target: app; onEmulatorInstalled: ... }`
- `logoForEmu(emuId)` function

- [ ] **Step 3: Update EmulatorManagePage.qml**

Minor updates:
- Keep the StackLayout pattern (grid index 0, detail index 1)
- Keep `selectedEmuId` property and `settingsOverlay.targetEmuIdChanged` connection
- Ensure the grid/list receives focus when the page is shown

- [ ] **Step 4: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
- Open settings → Emulators: should see vertical list
- Arrow keys should move focus with amber glow
- Enter/click should drill into detail page
- Verify emulator logos display correctly

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/EmulatorManagePage.qml cpp/qml/AppUI/EmulatorManageGrid.qml
git commit -m "feat: redesign emulator manage page as vertical list"
```

---

### Task 5: Emulator Detail Page Redesign

Rewrite EmulatorDetailPage with the new layout: logo, info card, simplified BIOS status, and stacked action buttons.

**Files:**
- Rewrite: `cpp/qml/AppUI/EmulatorDetailPage.qml`

- [ ] **Step 1: Read current file**

Read `cpp/qml/AppUI/EmulatorDetailPage.qml` fully. Critical things to preserve:
- `property string emuId`
- `signal back()`
- `property var emuList` / `property var emuInfo`
- `property int _v` version tracker
- All `Connections { target: app }` signal handlers (onEmulatorInstalled, onInstallProgress, onInstallFinished, onUninstallFinished)
- ProgressPopup component and all its bindings
- Uninstall confirmation dialog
- Reset configuration dialog
- `logoForEmu()` function
- All `app.*` calls: `installEmulator`, `uninstallEmulator`, `resetConfiguration`, `showEmulatorSettings`, `showControllerMapping`, `showHotkeySettings`, `openBiosFolder`

- [ ] **Step 2: Rewrite the layout**

Replace the two-column layout with a single-column scrollable layout:

**Installed state — sections top to bottom:**
1. Header: back button (32x32, `SettingsTheme.card`, amber arrow) + emulator name (20px) + status badge
2. Logo: 120x120px centered, `SettingsTheme.card` bg, rounded 12px
3. INFO section: section label + info card with rows (System, Version, Description)
4. BIOS section: section label + single status box
   - Detected: green-tinted bg, green dot (8px), "BIOS Detected" text
   - Not detected: red-tinted bg, red dot, "No BIOS Detected" text + "Open BIOS Folder" button
5. ACTIONS section: section label + stacked full-width buttons
   - Emulator Settings (accent), Controller Mapping (surface), Hotkeys (surface)
   - Reinstall/Update (warning), Reset Configuration (surface), Uninstall (danger)

**Not installed state:**
1. Same header + logo
2. INFO section: System + Description only
3. BIOS note: neutral card "BIOS files can be added after installation."
4. Get Started: large accent Install button + helper text

**Focus management for action buttons:**
- Track `focusIndex` across all focusable items (BIOS button if visible, then action buttons)
- D-pad up/down moves between buttons
- Enter/A activates the focused button

**Keep all existing:**
- ProgressPopup with all signal handling
- Uninstall and Reset dialogs (restyle with SettingsTheme colors)
- All `app.*` backend calls unchanged

- [ ] **Step 3: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
- Navigate to emulator detail (both installed and not installed states)
- Verify BIOS status shows simplified green/red indicator
- Verify all action buttons work (Settings, Controller Mapping, Hotkeys, Install, Uninstall, etc.)
- Test arrow key navigation between action buttons

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/EmulatorDetailPage.qml
git commit -m "feat: redesign emulator detail page with simplified BIOS and stacked actions"
```

---

### Task 6: Paths Settings Page Redesign

Rewrite PathsSettings with pill tabs and stacked path cards.

**Files:**
- Rewrite: `cpp/qml/AppUI/PathsSettings.qml`

- [ ] **Step 1: Read current file**

Read `cpp/qml/AppUI/PathsSettings.qml` fully. Preserve:
- `property var emuList: app.allEmulatorStatus()`
- `property int currentEmu: 0`
- `property string currentEmuId` computed from emuList
- Tab switching logic
- Path repeater model: `app.pathDefs(currentEmuId)` returning `{label, section, key, defaultPath}`
- Path value loading: `app.pathValue(emuId, section, key)`
- Browse button: `app.browsePath(label)`
- Save logic: collects all TextField values into `{section/key: value}` map, calls `app.savePaths(currentEmuId, values)`
- Reset logic: collects default values, calls same `app.savePaths`

- [ ] **Step 2: Rewrite the layout**

**Pill tabs at top:**
```qml
Row {
    spacing: 6
    Repeater {
        model: root.emuList.filter(e => e.installed)
        Rectangle {
            width: pillText.width + 28
            height: 30
            radius: SettingsTheme.pillRadius
            color: index === root.currentEmu ? SettingsTheme.accent : SettingsTheme.card
            border.width: index === root.currentEmu ? 0 : 1
            border.color: SettingsTheme.border

            Text {
                id: pillText
                anchors.centerIn: parent
                text: modelData.name
                font.pixelSize: 12
                font.weight: Font.Medium
                color: index === root.currentEmu ? SettingsTheme.background : SettingsTheme.textMuted
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.currentEmu = index
            }
        }
    }
}
```

**L1/R1 tab switching:**
```qml
Keys.onPressed: function(event) {
    if (event.key === Qt.Key_BracketLeft || event.key === Qt.Key_F1) {  // L1 mapping
        root.currentEmu = Math.max(0, root.currentEmu - 1)
        event.accepted = true
    } else if (event.key === Qt.Key_BracketRight || event.key === Qt.Key_F2) {  // R1 mapping
        root.currentEmu = Math.min(installedCount - 1, root.currentEmu + 1)
        event.accepted = true
    }
}
```

Note: The actual gamepad button mapping depends on how SDL2 maps shoulder buttons to Qt key events in this project. Check `app_controller.cpp` or the input handling code for the exact mapping. If gamepad events come through as different keys, adjust accordingly.

**Path cards** — each path as a `FocusableItem`:
```qml
Repeater {
    id: pathRepeater
    model: app.pathDefs(root.currentEmuId)

    FocusableItem {
        width: parent.width
        height: pathContent.height + 24
        isFocused: root.focusIndex === index

        // Store section/key for save logic
        property string section: modelData.section
        property string key: modelData.key

        ColumnLayout {
            id: pathContent
            anchors.left: parent.left; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 12
            spacing: 6

            Text {
                text: modelData.label.toUpperCase()
                font.pixelSize: 9
                color: SettingsTheme.textMuted
                font.letterSpacing: 0.5
            }

            RowLayout {
                spacing: 8
                TextField {
                    id: pathField
                    Layout.fillWidth: true
                    text: app.pathValue(root.currentEmuId, modelData.section, modelData.key)
                          || modelData.defaultPath
                    font.pixelSize: 11
                    color: SettingsTheme.text
                    // ... styling with SettingsTheme colors
                }
                Rectangle {
                    // Browse button
                    width: browseText.width + 20
                    height: 28
                    radius: 4
                    color: SettingsTheme.card
                    Text {
                        id: browseText
                        anchors.centerIn: parent
                        text: "Browse"
                        font.pixelSize: 9
                        color: SettingsTheme.accent
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var result = app.browsePath(modelData.label)
                            if (result) pathField.text = result
                        }
                    }
                }
            }
        }
    }
}
```

**Bottom bar** with Save, Reset, and ControllerHints.

- [ ] **Step 3: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
- Open Settings → Paths
- Verify pill tabs show installed emulators
- Switch tabs (click, L1/R1 if mapped)
- Navigate path cards with arrow keys
- Click Browse, verify file dialog opens
- Save and Reset buttons work

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/PathsSettings.qml
git commit -m "feat: redesign paths settings with pill tabs and stacked cards"
```

---

### Task 7: Scraper Settings — Dashboard Redesign

Rewrite ScraperSettings as a single-page dashboard (login + hub + systems + media + start all on one page when logged in).

**Files:**
- Rewrite: `cpp/qml/AppUI/ScraperSettings.qml`

- [ ] **Step 1: Read current file**

Read `cpp/qml/AppUI/ScraperSettings.qml` fully. This is the largest file (~1040 lines). Critical things to preserve:

**All properties** (state, progress, scrape detail, API quota — copy them all).

**All functions:** `resetMediaSelection()`, `resetSystemSelection()`, `isMediaSelected()`, `toggleMedia()`, `isSystemSelected()`, `toggleSystem()`

**All Connections { target: app } handlers:**
- `onScraperCredentialsValidated(success, message)`
- `onScraperSignedOut()`
- `onScrapeProgress(current, total, gameData)` — complex handler that updates 15+ properties
- `onScrapeFinished(succeeded, failed, skipped)`

**All backend calls:**
- `app.hasScraperCredentials()`
- `app.validateScraperCredentials(user, pass)`
- `app.scraperSignOut()`
- `app.scraperUsername()`
- `app.allMediaTypes()`
- `app.scrapableSystems()`
- `app.startBatchScrape(media, systems, filter)`
- `app.cancelScrape()`

- [ ] **Step 2: Rewrite with two-state layout**

Replace the 7-screen state machine with a simpler two-state model:

```qml
property string screenState: app.hasScraperCredentials() ? "dashboard" : "login"
```

**State "login":** Username + password fields + Sign In button (same logic, new styling).

**State "dashboard":** Single scrollable page with sections:
1. ACCOUNT section: username + "Connected" green text + "Edit" link (clicking Edit shows inline username/password/update fields + sign out)
2. SYSTEMS section: wrapping Flow of toggle pills using FocusableItem
3. MEDIA section: same wrapping Flow of toggle pills
4. FILTER section: pill buttons for "All Games", "Unscraped Only", "Favorites Only"
5. "Start Scraping" large accent button

**State "progress":** Same as current progress screen but restyled with SettingsTheme colors. Preserve all the detail card structure (cover art, metadata grid, description, status indicators, API quota, STOP/DONE button).

**Toggle pills** — reusable pattern for systems and media:
```qml
Flow {
    spacing: 4
    Repeater {
        model: root.scrapableSystemsList
        Rectangle {
            width: sysText.width + 20
            height: 26
            radius: 12
            color: root.isSystemSelected(modelData.id)
                   ? SettingsTheme.accent : SettingsTheme.border
            Text {
                id: sysText
                anchors.centerIn: parent
                text: modelData.name
                font.pixelSize: 10
                font.weight: Font.Medium
                color: root.isSystemSelected(modelData.id)
                       ? SettingsTheme.background : SettingsTheme.textMuted
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleSystem(modelData.id)
            }
        }
    }
}
```

**Focus navigation for pills** — this is tricky since pills wrap in a Flow. Use a flat index across all focusable items on the page (account card, each system pill, each media pill, filter pills, start button). D-pad left/right moves between pills in the same section, up/down jumps between sections.

- [ ] **Step 3: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
- Test login flow (if no credentials saved)
- Test dashboard: account status, system pills toggle, media pills toggle
- Start scraping: verify progress view shows correctly
- Verify all metadata displays during scrape
- Test STOP and DONE buttons

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: redesign scraper as single-page dashboard with progress view"
```

---

### Task 8: Themes Page Redesign

The themes page was inline in SettingsOverlay. It's now pushed as a separate page from the category list. If Task 3 already extracted it, update the styling. If not, create it as a standalone component.

**Files:**
- Create or modify: `cpp/qml/AppUI/ThemesPage.qml` (new component, or inline in SettingsOverlay's push logic)

- [ ] **Step 1: Create ThemesPage.qml**

Create `cpp/qml/AppUI/ThemesPage.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property int focusIndex: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 24
            Layout.bottomMargin: 16
            spacing: 12

            Text {
                text: "Themes"
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: SettingsTheme.text
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: SettingsTheme.border
        }

        // Theme list
        ListView {
            id: themeList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 8
            clip: true
            model: themeManager.availableThemes
            currentIndex: root.focusIndex
            focus: true

            Keys.onUpPressed: root.focusIndex = Math.max(0, root.focusIndex - 1)
            Keys.onDownPressed: root.focusIndex = Math.min(themeList.count - 1, root.focusIndex + 1)
            Keys.onReturnPressed: {
                if (themeManager.currentThemeId !== themeManager.availableThemes[root.focusIndex].id) {
                    themeManager.currentThemeId = themeManager.availableThemes[root.focusIndex].id
                }
            }

            delegate: FocusableItem {
                width: themeList.width
                height: 72
                isFocused: index === root.focusIndex
                color: themeManager.currentThemeId === modelData.id
                       ? Qt.rgba(0.91, 0.66, 0.22, 0.06) : SettingsTheme.card

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 16

                    // Color preview swatch
                    Rectangle {
                        width: 64; height: 48; radius: 6
                        color: SettingsTheme.card
                        border.width: 1
                        border.color: SettingsTheme.border
                        clip: true

                        // Gradient bg (derived from theme name or hardcoded)
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#1a1a2e" }
                                GradientStop { position: 1.0; color: "#16213e" }
                            }
                        }
                        // Accent bar
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width; height: 10
                            color: SettingsTheme.accent
                            opacity: 0.6
                        }
                    }

                    // Theme info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            text: modelData.name
                            font.pixelSize: 14; font.weight: Font.Medium
                            color: SettingsTheme.text
                        }
                        Text {
                            text: modelData.description || ""
                            font.pixelSize: 11
                            color: SettingsTheme.textDim
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: "by " + (modelData.author || "Unknown")
                            font.pixelSize: 10
                            color: SettingsTheme.textFaint
                        }
                    }

                    // Active indicator or Apply button
                    Loader {
                        active: true
                        sourceComponent: themeManager.currentThemeId === modelData.id
                            ? activeIndicator : applyButton
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.focusIndex = index
                        if (themeManager.currentThemeId !== modelData.id) {
                            themeManager.currentThemeId = modelData.id
                        }
                    }
                }
            }
        }

        // Controller hints
        ControllerHints {
            Layout.fillWidth: true
            hints: [
                {icon: "↕", text: "Navigate"},
                {icon: "🅰", text: "Apply Theme"},
                {icon: "🅱", text: "Back"}
            ]
        }
    }

    Component {
        id: activeIndicator
        Row {
            spacing: 6
            Rectangle {
                width: 8; height: 8; radius: 4
                color: SettingsTheme.success
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "Active"
                font.pixelSize: 12; font.weight: Font.DemiBold
                color: SettingsTheme.success
            }
        }
    }

    Component {
        id: applyButton
        Rectangle {
            width: 70; height: 32; radius: 6
            color: SettingsTheme.card
            border.width: 1; border.color: SettingsTheme.border
            Text {
                anchors.centerIn: parent
                text: "Apply"
                font.pixelSize: 12
                color: SettingsTheme.text
            }
        }
    }
}
```

- [ ] **Step 2: Wire into SettingsOverlay**

In the category list's select handler, push ThemesPage when "Themes" is selected:

```qml
panelStack.push(themesPageComponent)
```

Ensure the component is declared:
```qml
Component {
    id: themesPageComponent
    ThemesPage {}
}
```

- [ ] **Step 3: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
- Open Settings → Themes
- Verify theme list shows with preview swatches
- Arrow keys navigate with focus glow
- Enter/click applies a theme
- Active theme shows green dot indicator

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ThemesPage.qml cpp/qml/AppUI/SettingsOverlay.qml
git commit -m "feat: add redesigned themes page with preview swatches"
```

---

### Task 9: Delete Unused EmulatorsSettings.qml

The old `EmulatorsSettings.qml` was the install/status page that's been replaced by the EmulatorManagePage redesign.

**Files:**
- Delete: `cpp/qml/AppUI/EmulatorsSettings.qml`

- [ ] **Step 1: Verify EmulatorsSettings is no longer referenced**

Search the codebase for any references to `EmulatorsSettings`:
```bash
grep -r "EmulatorsSettings" cpp/
```

If still referenced somewhere, update those references first.

- [ ] **Step 2: Delete the file**

```bash
rm cpp/qml/AppUI/EmulatorsSettings.qml
```

- [ ] **Step 3: Build and verify**

```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds with no missing component errors.

- [ ] **Step 4: Commit**

```bash
git add -u cpp/qml/AppUI/EmulatorsSettings.qml
git commit -m "chore: remove unused EmulatorsSettings.qml"
```

---

### Task 10: Setup Wizard Theme Update

Update WizardTheme.qml to use the new warm charcoal + amber palette.

**Files:**
- Modify: `cpp/qml/SetupWizard/WizardTheme.qml`

- [ ] **Step 1: Read current file**

Read `cpp/qml/SetupWizard/WizardTheme.qml`.

- [ ] **Step 2: Update all color values**

Replace the color properties while keeping the size and animation properties:

```qml
pragma Singleton
import QtQuick

QtObject {
    // Colors — warm charcoal + amber
    readonly property color background:     "#131210"
    readonly property color surface:        "#201f1c"
    readonly property color surfaceHover:   "#2e2c28"
    readonly property color accent:         "#e8a838"
    readonly property color accentLight:    "#f0b848"
    readonly property color navBackground:  "#1a1917"
    readonly property color cardSelected:   "#2a2518"
    readonly property color textPrimary:    "#e0ddd6"
    readonly property color textSecondary:  "#c8c4b8"
    readonly property color textMuted:      "#8a8680"
    readonly property color textDim:        "#6a6660"
    readonly property color divider:        "#2e2c28"
    readonly property color success:        "#6a9b4a"
    readonly property color error:          "#c85040"

    // Sizes — keep existing
    readonly property int pageMargin: 48
    readonly property int pageTopMargin: 40
    readonly property int cardWidth: 160
    readonly property int cardHeight: 110
    readonly property int cardRadius: 14
    readonly property int cardSpacing: 16
    readonly property int pillWidth: 120
    readonly property int pillHeight: 50
    readonly property int pillRadius: 25
    readonly property int navHeight: 64

    // Animation — keep existing
    readonly property int animFast: 150
    readonly property int animNormal: 200
    readonly property int animSlow: 300
}
```

- [ ] **Step 3: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
Verify the wizard shows with the warm palette (if you can trigger it — may need to reset first-run state or launch with a flag).

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/SetupWizard/WizardTheme.qml
git commit -m "feat: update wizard theme to warm charcoal + amber palette"
```

---

### Task 11: Setup Wizard Shell — Wide Card + Progress Bar

Rewrite Main.qml for the wide centered card layout, NavBar for new styling + controller hints, and StepIndicator as a progress bar.

**Files:**
- Rewrite: `cpp/qml/SetupWizard/Main.qml`
- Rewrite: `cpp/qml/SetupWizard/NavBar.qml`
- Rewrite: `cpp/qml/SetupWizard/StepIndicator.qml`

- [ ] **Step 1: Read current files**

Read all three files. Critical to preserve from Main.qml:
- `property int pageCount` — change from 8 to 7 (merging BIOS+ROMs)
- The SwipeView page order (adjusted for merged FilesPage)
- The `onContinueClicked` validation logic (folder path check, refresh calls, auto-install)
- Backend calls: `wizard.rootPath`, `wizard.ensureRomDirs(systems)`, `emulators.availableSystems()`, `installPage.startInstall()`

- [ ] **Step 2: Rewrite Main.qml as a wide centered card**

Key changes:
- `color: WizardTheme.background` on the ApplicationWindow — change to dark radial gradient (use a Rectangle with gradient behind the card)
- Replace ColumnLayout with a centered card:

```qml
ApplicationWindow {
    id: root
    visible: true
    width: 900; height: 650
    minimumWidth: 720; minimumHeight: 540
    title: "EmuFront Setup"
    color: "#131210"

    property int pageCount: 7  // Was 8, now 7 (merged Files page)

    // Dark gradient background
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#252320" }
            GradientStop { position: 1.0; color: "#131210" }
        }
    }

    // Centered card
    Rectangle {
        id: wizardCard
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.85, 800)
        height: Math.min(parent.height * 0.85, 550)
        radius: 14
        color: WizardTheme.surface
        border.width: 1
        border.color: WizardTheme.divider

        layer.enabled: true
        layer.effect: /* DropShadow or just use Rectangle shadow trick */

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header with progress bar
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 24
                Layout.bottomMargin: 8

                // Back arrow (visible when depth > 0 and not on last page)
                // ... back button if needed

                Text {
                    text: pageTitleForIndex(swipeView.currentIndex)
                    font.pixelSize: 18; font.weight: Font.DemiBold
                    color: WizardTheme.textPrimary
                    Layout.fillWidth: true
                }

                Text {
                    text: (swipeView.currentIndex + 1) + " / " + pageCount
                    font.pixelSize: 12
                    color: WizardTheme.textDim
                }
            }

            // Thin progress bar
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 24; Layout.rightMargin: 24
                height: 2; radius: 1
                color: WizardTheme.divider

                Rectangle {
                    width: parent.width * ((swipeView.currentIndex + 1) / pageCount)
                    height: parent.height; radius: 1
                    color: WizardTheme.accent

                    Behavior on width {
                        NumberAnimation { duration: WizardTheme.animNormal }
                    }
                }
            }

            // SwipeView — 7 pages
            SwipeView {
                id: swipeView
                Layout.fillWidth: true; Layout.fillHeight: true
                interactive: false; clip: true

                WelcomePage { isCurrentPage: SwipeView.isCurrentItem }
                FolderPage {}
                EmulatorsPage { id: emulatorsPage }
                ResolutionPage { id: resolutionPage }
                AspectRatioPage { id: aspectRatioPage }
                FilesPage { id: filesPage }      // NEW: merged BIOS + ROMs
                InstallPage { id: installPage; isCurrentPage: SwipeView.isCurrentItem }
            }

            // NavBar
            NavBar {
                Layout.fillWidth: true
                currentIndex: swipeView.currentIndex
                pageCount: root.pageCount
                canContinue: {
                    if (swipeView.currentIndex === 1) return wizard.rootPath !== ""
                    return true
                }
                onBackClicked: swipeView.currentIndex--
                onContinueClicked: {
                    var cur = swipeView.currentIndex
                    if (cur === 1 && !wizard.rootPath) return

                    if (cur === 2) {
                        resolutionPage.refresh()
                        aspectRatioPage.refresh()
                    }
                    // Refresh Files page when about to enter it (leaving AspectRatio)
                    if (cur === 4) {
                        filesPage.refresh()
                        wizard.ensureRomDirs(emulators.availableSystems())
                    }

                    if (cur < pageCount - 1) {
                        swipeView.currentIndex = cur + 1
                    }
                    if (swipeView.currentIndex === pageCount - 1) {
                        installPage.startInstall()
                    }
                }
            }
        }
    }

    function pageTitleForIndex(index) {
        var titles = ["Welcome", "Choose Data Folder", "Select Emulators",
                      "Display Resolution", "Aspect Ratio", "Files", "Installing"]
        return titles[index] || ""
    }
}
```

- [ ] **Step 3: Rewrite NavBar.qml**

Update styling with WizardTheme warm colors. Add controller hints at the bottom:

```qml
Rectangle {
    id: root
    height: WizardTheme.navHeight
    color: WizardTheme.navBackground

    property int currentIndex: 0
    property int pageCount: 7
    property bool canContinue: true

    signal backClicked()
    signal continueClicked()

    Rectangle {
        anchors.top: parent.top
        width: parent.width; height: 1
        color: WizardTheme.divider
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 24; anchors.rightMargin: 24

        // Controller hints (left side)
        Row {
            spacing: 16
            Text { text: "L1/R1 Page"; color: "#4a4640"; font.pixelSize: 10; visible: root.currentIndex > 0 && root.currentIndex < root.pageCount - 1 }
            Text { text: "🅰 Select"; color: "#4a4640"; font.pixelSize: 10 }
            Text { text: "🅱 Back"; color: "#4a4640"; font.pixelSize: 10; visible: root.currentIndex > 0 }
        }

        Item { Layout.fillWidth: true }

    // L1/R1 key handling for page navigation
    Keys.onPressed: function(event) {
        // L1 = go back a page
        if (event.key === Qt.Key_PageUp) {
            if (root.currentIndex > 0 && root.currentIndex < root.pageCount - 1)
                root.backClicked()
            event.accepted = true
        }
        // R1 = advance a page
        else if (event.key === Qt.Key_PageDown) {
            if (root.currentIndex < root.pageCount - 1 && root.canContinue)
                root.continueClicked()
            event.accepted = true
        }
    }

        // Back button
        Rectangle {
            visible: root.currentIndex > 0 && root.currentIndex < root.pageCount - 1
            width: 100; height: 36; radius: 6
            color: backMa.containsMouse ? WizardTheme.surfaceHover : WizardTheme.surface
            Text { anchors.centerIn: parent; text: "Back"; color: WizardTheme.textMuted; font.pixelSize: 13 }
            MouseArea { id: backMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.backClicked() }
        }

        // Continue button
        Rectangle {
            visible: root.currentIndex < root.pageCount - 1
            width: 140; height: 36; radius: 6
            opacity: root.canContinue ? 1.0 : 0.5
            color: contMa.containsMouse && root.canContinue ? WizardTheme.accentLight : WizardTheme.accent
            Text {
                anchors.centerIn: parent
                text: root.currentIndex === 0 ? "Get Started" : (root.currentIndex === root.pageCount - 2 ? "Finish" : "Continue")
                color: WizardTheme.background
                font.pixelSize: 13; font.weight: Font.DemiBold
            }
            MouseArea {
                id: contMa; anchors.fill: parent; hoverEnabled: true
                cursorShape: root.canContinue ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (root.canContinue) root.continueClicked()
            }
        }
    }
}
```

- [ ] **Step 4: Rewrite StepIndicator.qml**

No longer needed as a dots indicator — the progress bar is now in Main.qml. Either:
- Delete StepIndicator.qml entirely if Main.qml no longer references it
- Or replace it with a thin wrapper that Main.qml can still instantiate (empty item)

Simplest: remove the `StepIndicator` instantiation from Main.qml and delete the file. If Main.qml still references it, just make it an empty `Item { }`.

- [ ] **Step 5: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
Verify the wizard shows as a wide centered card with progress bar. The individual pages may still have old styling — that's fine, they'll be updated in the next tasks.

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/SetupWizard/Main.qml cpp/qml/SetupWizard/NavBar.qml cpp/qml/SetupWizard/StepIndicator.qml
git commit -m "feat: redesign wizard shell as wide centered card with progress bar"
```

---

### Task 12: Wizard Shared Components — PillButton and EmulatorCard

Update PillButton and EmulatorCard with the new palette and add focus/keyboard support.

**Files:**
- Rewrite: `cpp/qml/SetupWizard/PillButton.qml`
- Rewrite: `cpp/qml/SetupWizard/EmulatorCard.qml`

- [ ] **Step 1: Read current files**

Read both files. Preserve all properties and signals:
- PillButton: `label`, `selected`, `signal clicked()`
- EmulatorCard: `emuId`, `emuName`, `systems`, `selected`, `signal clicked()`, `logoForEmu()`

- [ ] **Step 2: Update PillButton.qml**

Add `property bool isFocused: false` property. Update all colors from WizardTheme old palette to new:

```qml
Item {
    id: root
    width: WizardTheme.pillWidth
    height: WizardTheme.pillHeight

    property string label: ""
    property bool selected: false
    property bool isFocused: false

    signal clicked()

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: WizardTheme.pillRadius
        color: root.selected ? Qt.lighter(WizardTheme.surface, 1.3) : WizardTheme.surface
        border.width: root.isFocused ? 2 : (root.selected ? 2 : 1)
        border.color: root.isFocused ? WizardTheme.accent
                      : (root.selected ? WizardTheme.accent : WizardTheme.divider)

        Behavior on color { ColorAnimation { duration: WizardTheme.animFast } }
        Behavior on border.color { ColorAnimation { duration: WizardTheme.animFast } }

        // Focus glow
        Rectangle {
            anchors.fill: parent; anchors.margins: -3
            radius: parent.radius + 3; color: "transparent"
            border.width: 2; border.color: WizardTheme.accent
            opacity: root.isFocused ? 0.3 : 0; visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animFast } }
        }

        Text {
            anchors.centerIn: parent
            text: root.label
            font.pixelSize: 14; font.weight: Font.Medium
            color: root.selected ? WizardTheme.textPrimary : WizardTheme.textMuted
        }

        // Check badge
        Rectangle {
            anchors.top: parent.top; anchors.right: parent.right
            anchors.margins: -4
            width: 18; height: 18; radius: 9
            color: WizardTheme.accent
            visible: root.selected
            opacity: root.selected ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: WizardTheme.animFast } }
            Text { anchors.centerIn: parent; text: "✓"; font.pixelSize: 11; color: WizardTheme.background }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
            onPressed: bg.scale = 0.96
            onReleased: bg.scale = 1.0
        }

        Behavior on scale { NumberAnimation { duration: 100 } }
    }
}
```

- [ ] **Step 3: Update EmulatorCard.qml**

Add `property bool isFocused: false`. Update colors:

```qml
Item {
    id: root
    width: 140; height: 140

    property string emuId: ""
    property string emuName: ""
    property string systems: ""
    property bool selected: false
    property bool isFocused: false

    signal clicked()

    function logoForEmu(id) {
        var logos = {
            "pcsx2": "qrc:/SetupWizard/qml/AppUI/images/pcsx2_logo.png",
            "duckstation": "qrc:/SetupWizard/qml/AppUI/images/duckstation_logo.png"
        }
        return logos[id] || ""
    }

    // Glow border
    Rectangle {
        anchors.fill: card; anchors.margins: -4
        radius: card.radius + 4; color: "transparent"
        border.width: 2; border.color: WizardTheme.accent
        opacity: root.isFocused ? 0.3 : (root.selected ? 0.2 : 0)
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: WizardTheme.animNormal } }
    }

    Rectangle {
        id: card
        anchors.fill: parent; radius: 12
        color: root.selected ? WizardTheme.cardSelected : WizardTheme.surface
        border.width: root.isFocused ? 2 : (root.selected ? 2 : 1)
        border.color: root.isFocused ? WizardTheme.accent
                      : (root.selected ? WizardTheme.accent : WizardTheme.divider)

        Behavior on color { ColorAnimation { duration: WizardTheme.animNormal } }
        Behavior on border.color { ColorAnimation { duration: WizardTheme.animNormal } }

        // Logo image (with OpacityMask) — keep existing pattern
        Image {
            id: logoImg
            anchors.fill: parent; anchors.margins: 24
            source: root.logoForEmu(root.emuId)
            fillMode: Image.PreserveAspectFit
            visible: false
        }
        Rectangle {
            id: logoMask; anchors.fill: logoImg; radius: 10; visible: false
        }
        // OpacityMask — requires Qt5Compat.GraphicalEffects import
        // ... keep existing mask pattern

        // Fallback text if no logo
        Text {
            anchors.centerIn: parent
            text: root.emuName
            font.pixelSize: 14; font.weight: Font.Bold
            color: WizardTheme.textPrimary
            visible: root.logoForEmu(root.emuId) === ""
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
            onPressed: card.scale = 0.97
            onReleased: card.scale = 1.0
        }

        Behavior on scale { NumberAnimation { duration: 100 } }
    }
}
```

- [ ] **Step 4: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
Launch wizard and verify the emulator cards and pill buttons show with the warm palette.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/SetupWizard/PillButton.qml cpp/qml/SetupWizard/EmulatorCard.qml
git commit -m "feat: update PillButton and EmulatorCard with warm palette and focus support"
```

---

### Task 13: Wizard Pages Restyle — Welcome, Folder, Emulators

Restyle the first three wizard pages with the warm palette and add focus/keyboard support.

**Files:**
- Modify: `cpp/qml/SetupWizard/WelcomePage.qml`
- Modify: `cpp/qml/SetupWizard/FolderPage.qml`
- Modify: `cpp/qml/SetupWizard/EmulatorsPage.qml`

- [ ] **Step 1: Read all three files**

Read each file. Note the structure and all backend calls.

- [ ] **Step 2: Restyle WelcomePage.qml**

Update all color references from old WizardTheme properties to new warm values. The structure stays the same (title, description, animation). Colors change to warm palette.

- [ ] **Step 3: Restyle FolderPage.qml**

Update colors. Add focus handling for the Browse button:
- `property int focusIndex: 0` (0 = path card/browse)
- `Keys.onReturnPressed` triggers browse
- Visual focus glow on the path card when focused

Preserve: `wizard.rootPath`, `wizard.openFolder()`, browse dialog logic.

- [ ] **Step 4: Restyle EmulatorsPage.qml**

Update colors. Add focus handling for the EmulatorCard grid:
- `property int focusIndex: 0`
- Pass `isFocused: index === focusIndex` to each EmulatorCard
- D-pad navigation (left/right for columns, up/down for rows in a grid)
- `Keys.onReturnPressed` toggles selection on focused card

Preserve: `emulators.allEmulators()`, selection logic, `selected` property binding.

- [ ] **Step 5: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/SetupWizard/WelcomePage.qml cpp/qml/SetupWizard/FolderPage.qml cpp/qml/SetupWizard/EmulatorsPage.qml
git commit -m "feat: restyle welcome, folder, and emulators wizard pages"
```

---

### Task 14: Wizard Pages Restyle — Resolution, AspectRatio, Install

Restyle the remaining wizard pages.

**Files:**
- Modify: `cpp/qml/SetupWizard/ResolutionPage.qml`
- Modify: `cpp/qml/SetupWizard/AspectRatioPage.qml`
- Modify: `cpp/qml/SetupWizard/InstallPage.qml`

- [ ] **Step 1: Read all three files**

Read each file. Note pill button usage, per-emulator sections, and all backend calls.

- [ ] **Step 2: Restyle ResolutionPage.qml**

Update colors. Add focus handling for PillButtons:
- Pass `isFocused` to each PillButton
- D-pad left/right between pills in the same row
- `Keys.onReturnPressed` selects the focused pill

Preserve: `emulators.allEmulators()`, `emulators.setResolution()`, per-emulator resolution logic.

- [ ] **Step 3: Restyle AspectRatioPage.qml**

Same pattern as ResolutionPage. Update colors, add focus to PillButtons. Preserve visual preview rectangles and aspect ratio logic.

- [ ] **Step 4: Restyle InstallPage.qml**

Update colors. This page is mostly display-only (progress bar, status text, Finish button). Minimal focus changes — just the Finish button needs to be focusable.

Preserve: `startInstall()` function, all `emulators.*` / `wizard.*` / `app.*` calls.

- [ ] **Step 5: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/SetupWizard/ResolutionPage.qml cpp/qml/SetupWizard/AspectRatioPage.qml cpp/qml/SetupWizard/InstallPage.qml
git commit -m "feat: restyle resolution, aspect ratio, and install wizard pages"
```

---

### Task 15: New FilesPage — Merged BIOS + ROMs

Create the new FilesPage that combines BiosPage and RomsPage, then delete RomsPage.

**Files:**
- Create: `cpp/qml/SetupWizard/FilesPage.qml`
- Delete: `cpp/qml/SetupWizard/RomsPage.qml`
- Modify: `cpp/qml/SetupWizard/BiosPage.qml` (delete or redirect)

- [ ] **Step 1: Read BiosPage.qml and RomsPage.qml**

Read both files fully. Note all backend calls:
- BiosPage: `emulators.allEmulators()`, `emulators.biosStatus(emuId)`, `wizard.rootPath`, `wizard.openFolder(path)`
- RomsPage: `emulators.availableSystems()`, `wizard.romsDir`, `wizard.openFolder(path)`

- [ ] **Step 2: Create FilesPage.qml**

Combine both pages into one:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var emuList: []

    function refresh() {
        emuList = emulators.allEmulators().filter(function(e) { return e.selected })
    }

    Flickable {
        anchors.fill: parent
        anchors.leftMargin: WizardTheme.pageMargin
        anchors.rightMargin: WizardTheme.pageMargin
        anchors.topMargin: WizardTheme.pageTopMargin
        contentWidth: width
        contentHeight: contentCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: 16

            Text {
                text: "Files"
                font.pixelSize: 28; font.weight: Font.Bold
                color: WizardTheme.textPrimary
            }

            Text {
                text: "Check BIOS status and ROM folder locations."
                font.pixelSize: 15; color: WizardTheme.textMuted
                wrapMode: Text.WordWrap; Layout.fillWidth: true
            }

            // ── BIOS Sub-Section ──
            Text {
                text: "BIOS"
                font.pixelSize: 13; font.weight: Font.Bold
                color: WizardTheme.textMuted
                Layout.topMargin: 8
            }

            Repeater {
                model: root.emuList

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: {
                        var bios = emulators.biosStatus(modelData.id)
                        return bios && bios.length > 0
                    }

                    Text {
                        text: modelData.name
                        font.pixelSize: 14; font.weight: Font.DemiBold
                        color: WizardTheme.textPrimary
                    }

                    // Simplified status — detected or not
                    Rectangle {
                        Layout.fillWidth: true
                        height: 40; radius: 8
                        color: {
                            var bios = emulators.biosStatus(modelData.id)
                            var found = bios.some(function(b) { return b.found })
                            return found ? Qt.rgba(0.42, 0.61, 0.29, 0.12)
                                         : Qt.rgba(0.78, 0.31, 0.25, 0.12)
                        }
                        border.width: 1
                        border.color: {
                            var bios = emulators.biosStatus(modelData.id)
                            var found = bios.some(function(b) { return b.found })
                            return found ? Qt.rgba(0.42, 0.61, 0.29, 0.25)
                                         : Qt.rgba(0.78, 0.31, 0.25, 0.25)
                        }

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left; anchors.leftMargin: 12
                            spacing: 10

                            Rectangle {
                                width: 8; height: 8; radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: {
                                    var bios = emulators.biosStatus(modelData.id)
                                    var found = bios.some(function(b) { return b.found })
                                    return found ? WizardTheme.success : WizardTheme.error
                                }
                            }
                            Text {
                                text: {
                                    var bios = emulators.biosStatus(modelData.id)
                                    var found = bios.some(function(b) { return b.found })
                                    return found ? "BIOS Detected" : "No BIOS Detected"
                                }
                                font.pixelSize: 12; font.weight: Font.Medium
                                color: {
                                    var bios = emulators.biosStatus(modelData.id)
                                    var found = bios.some(function(b) { return b.found })
                                    return found ? WizardTheme.success : WizardTheme.error
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                spacing: 8
                Button {
                    text: "Open BIOS Folder"
                    onClicked: wizard.openFolder(wizard.rootPath + "/bios")
                    // Style with WizardTheme
                    implicitHeight: 36
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? WizardTheme.surfaceHover : WizardTheme.surface
                        border.width: 1; border.color: WizardTheme.divider
                    }
                    contentItem: Text {
                        text: parent.text; font.pixelSize: 12
                        color: WizardTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    text: "Refresh"
                    onClicked: root.refresh()
                    implicitHeight: 36
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? WizardTheme.surfaceHover : WizardTheme.surface
                        border.width: 1; border.color: WizardTheme.divider
                    }
                    contentItem: Text {
                        text: parent.text; font.pixelSize: 12
                        color: WizardTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // ── ROM Folders Sub-Section ──
            Text {
                text: "ROM FOLDERS"
                font.pixelSize: 13; font.weight: Font.Bold
                color: WizardTheme.textMuted
                Layout.topMargin: 16
            }

            Text {
                text: "Place your ROM files in the system folders below."
                font.pixelSize: 13; color: WizardTheme.textDim
                wrapMode: Text.WordWrap; Layout.fillWidth: true
            }

            ListView {
                Layout.fillWidth: true
                height: Math.min(contentHeight, 200)
                spacing: 6; clip: true
                model: emulators.availableSystems()

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 44; radius: 6
                    color: WizardTheme.surface
                    border.width: 1; border.color: WizardTheme.divider

                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10
                        Text {
                            text: modelData.toUpperCase()
                            font.pixelSize: 12; font.weight: Font.Bold
                            color: WizardTheme.textPrimary
                            Layout.preferredWidth: 60
                        }
                        Text {
                            text: wizard.romsDir + "/" + modelData + "/"
                            font.pixelSize: 11
                            color: WizardTheme.textDim
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Button {
                text: "Open ROM Folder"
                onClicked: wizard.openFolder(wizard.romsDir)
                implicitHeight: 36
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? WizardTheme.surfaceHover : WizardTheme.surface
                    border.width: 1; border.color: WizardTheme.divider
                }
                contentItem: Text {
                    text: parent.text; font.pixelSize: 12
                    color: WizardTheme.textPrimary
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }

            Item { height: 20 }
        }
    }
}
```

- [ ] **Step 3: Delete RomsPage.qml and BiosPage.qml**

```bash
rm cpp/qml/SetupWizard/RomsPage.qml
rm cpp/qml/SetupWizard/BiosPage.qml
```

Update Main.qml if not already done in Task 11 — replace the BiosPage and RomsPage entries in SwipeView with the single FilesPage.

- [ ] **Step 4: Build and test**

```bash
cd cpp && cmake --build build && ./build/EmulatorFrontend
```
Verify the Files page shows both BIOS status and ROM folders.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/SetupWizard/FilesPage.qml
git add -u cpp/qml/SetupWizard/RomsPage.qml cpp/qml/SetupWizard/BiosPage.qml
git add cpp/qml/SetupWizard/Main.qml
git commit -m "feat: merge BIOS and ROM pages into unified FilesPage"
```

---

### Task 16: Final Integration — Verify All Pages and Add .gitignore Entry

Run through the entire settings UI and wizard to verify everything works end-to-end.

**Files:**
- Modify: `.gitignore` (add `.superpowers/` if not already there)

- [ ] **Step 1: Add .gitignore entry**

Check if `.superpowers/` is in `.gitignore`:
```bash
grep -c ".superpowers" .gitignore
```

If not present, add it:
```
.superpowers/
```

- [ ] **Step 2: Full build**

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```

- [ ] **Step 3: Launch and verify settings overlay**

```bash
./build/EmulatorFrontend
```

Test checklist:
- [ ] Escape opens settings slide panel from right
- [ ] Category list shows 4 items with warm palette
- [ ] Arrow keys move focus with amber glow
- [ ] Enter selects a category, content slides in
- [ ] B/Escape goes back to category list
- [ ] B/Escape from category list closes panel
- [ ] Mouse click works on all items
- [ ] Click outside panel closes it

- [ ] **Step 4: Verify each settings page**

- [ ] Emulators → list view, arrow keys, drill into detail
- [ ] Emulator detail → info card, BIOS status (green/red), all action buttons work
- [ ] Paths → pill tabs, path cards, Browse dialogs, Save/Reset
- [ ] Scraper → login (if no credentials), dashboard, toggle pills, Start Scraping, progress view
- [ ] Themes → list with swatches, Apply, Active indicator

- [ ] **Step 5: Verify setup wizard**

If possible, trigger the wizard (delete config or use a flag). Test:
- [ ] Wide centered card on dark background
- [ ] Progress bar advances with pages
- [ ] All 7 pages display correctly with warm palette
- [ ] L1/R1 (if gamepad connected) or Back/Continue buttons navigate
- [ ] Files page shows both BIOS status and ROM folders
- [ ] Install page starts installation correctly

- [ ] **Step 6: Commit .gitignore**

```bash
git add .gitignore
git commit -m "chore: add .superpowers/ to gitignore"
```

---

## Implementation Notes

**Known items to address during implementation:**

1. **Gamepad L1/R1 key mapping:** The plan uses `Qt.Key_PageUp`/`Qt.Key_PageDown` as placeholders for shoulder buttons. Check how SDL2 gamepad events are mapped to Qt key events in the project's input handling code (`app_controller.cpp` or similar) and adjust key codes accordingly.

2. **Focus wrapping:** All lists should wrap focus (last item → first, first → last). The plan shows this pattern for the emulator list but it should be applied consistently to every focusable list.

3. **Button press animations:** The spec requires scale-down (0.97) on press for all interactive items, not just PillButton and EmulatorCard. Apply the same pattern to settings action buttons, category items, and other clickable elements.

4. **Theme preview swatches:** Each theme in the Themes page should have a unique gradient in its preview swatch. During implementation, either read colors from `theme.json` or hardcode per-theme gradients.

5. **Wizard background:** The spec calls for a radial gradient. QML's `Rectangle.gradient` is linear. Use `RadialGradient` from `Qt5Compat.GraphicalEffects` or a similar approach.

6. **Card/panel shadows:** The spec requires drop shadows on the wizard card and settings panel. Use `DropShadow` from `Qt5Compat.GraphicalEffects` or a shadow Rectangle technique.

7. **BiosSection.qml cleanup:** After Task 5 (EmulatorDetailPage redesign), check if `BiosSection.qml` is still referenced anywhere. If not, delete it.

8. **Scraper stop confirmation:** When B/Circle is pressed during active scraping, show a confirmation dialog before stopping (spec requirement).
