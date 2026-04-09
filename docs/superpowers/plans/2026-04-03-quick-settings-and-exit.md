# Quick Settings (Resolution, Aspect Ratio) & Exit — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Resolution, Aspect Ratio, and Exit entries to the settings overlay — two card-based quick-settings pages and a confirmation-gated app exit.

**Architecture:** New Q_INVOKABLE methods on AppController wrap existing adapter `resolutionOptions()` / `aspectRatioOptions()` structs. Two new QML pages (ResolutionSettings, AspectRatioSettings) show cards per installed emulator with pill selectors and a Save button. Exit uses an inline confirmation dialog. SettingsOverlay grows from 4 to 7 category entries.

**Tech Stack:** C++17, Qt6 QML, existing adapter/IniFile infrastructure

---

## File Map

| Action | File | Responsibility |
|--------|------|---------------|
| Modify | `cpp/src/ui/app_controller.h` | Add 6 new Q_INVOKABLE method declarations |
| Modify | `cpp/src/ui/app_controller.cpp` | Implement the 6 new methods |
| Create | `cpp/qml/AppUI/ResolutionSettings.qml` | Resolution card grid page |
| Create | `cpp/qml/AppUI/AspectRatioSettings.qml` | Aspect ratio card grid page |
| Modify | `cpp/qml/AppUI/SettingsOverlay.qml` | Add 3 categories, 2 page components, exit dialog |
| Modify | `cpp/CMakeLists.txt` | Register 2 new QML files |

---

### Task 1: Add Quick Settings Methods to AppController

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Add method declarations to app_controller.h**

Add these 6 declarations in the `// Config settings` section, after the `resetConfiguration` declaration (line 62):

```cpp
    // Quick settings (resolution / aspect ratio)
    Q_INVOKABLE QVariantList quickResolutionOptions(const QString& emuId) const;
    Q_INVOKABLE QString currentResolution(const QString& emuId) const;
    Q_INVOKABLE void applyQuickResolution(const QVariantMap& choices);
    Q_INVOKABLE QVariantList quickAspectRatioOptions(const QString& emuId) const;
    Q_INVOKABLE QString currentAspectRatio(const QString& emuId) const;
    Q_INVOKABLE void applyQuickAspectRatio(const QVariantMap& choices);
```

- [ ] **Step 2: Implement quickResolutionOptions in app_controller.cpp**

Add after the `resetConfiguration` method (around line 353):

```cpp
// ── Quick Settings (Resolution / Aspect Ratio) ───────────

QVariantList AppController::quickResolutionOptions(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    auto opts = adapter->resolutionOptions();
    for (const auto& opt : opts.options) {
        QVariantMap item;
        item["label"] = opt.label;
        item["value"] = opt.value;
        list.append(item);
    }
    return list;
}
```

- [ ] **Step 3: Implement currentResolution**

```cpp
QString AppController::currentResolution(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto opts = adapter->resolutionOptions();
    if (opts.options.isEmpty()) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return opts.defaultValue;

    IniFile ini;
    ini.load(configPath);
    QString val = ini.value(opts.section, opts.key);
    return val.isEmpty() ? opts.defaultValue : val;
}
```

- [ ] **Step 4: Implement applyQuickResolution**

```cpp
void AppController::applyQuickResolution(const QVariantMap& choices) {
    for (auto it = choices.constBegin(); it != choices.constEnd(); ++it) {
        const QString& emuId = it.key();
        const QString value = it.value().toString();

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        auto opts = adapter->resolutionOptions();
        if (opts.options.isEmpty()) continue;

        QString configPath = adapter->configFilePath();
        if (configPath.isEmpty()) continue;

        IniFile ini;
        ini.load(configPath);
        ini.setValue(opts.section, opts.key, value);
        ini.save(configPath);
    }
    setStatus("Resolution settings saved.");
}
```

- [ ] **Step 5: Implement quickAspectRatioOptions**

```cpp
QVariantList AppController::quickAspectRatioOptions(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    auto opts = adapter->aspectRatioOptions();
    for (const auto& opt : opts.options) {
        QVariantMap item;
        item["label"] = opt.label;
        list.append(item);
    }
    return list;
}
```

- [ ] **Step 6: Implement currentAspectRatio**

```cpp
QString AppController::currentAspectRatio(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto opts = adapter->aspectRatioOptions();
    if (opts.options.isEmpty()) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return opts.defaultLabel;

    IniFile ini;
    ini.load(configPath);

    // Check which option matches the current INI state by comparing the first patch key
    for (const auto& opt : opts.options) {
        if (opt.patches.isEmpty()) continue;
        const auto& firstPatch = opt.patches.first();
        QString val = ini.value(firstPatch.section, firstPatch.key);
        if (val == firstPatch.value)
            return opt.label;
    }
    return opts.defaultLabel;
}
```

- [ ] **Step 7: Implement applyQuickAspectRatio**

```cpp
void AppController::applyQuickAspectRatio(const QVariantMap& choices) {
    for (auto it = choices.constBegin(); it != choices.constEnd(); ++it) {
        const QString& emuId = it.key();
        const QString label = it.value().toString();

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        auto opts = adapter->aspectRatioOptions();

        // Find the matching option by label
        for (const auto& opt : opts.options) {
            if (opt.label != label) continue;

            QString configPath = adapter->configFilePath();
            if (configPath.isEmpty()) break;

            IniFile ini;
            ini.load(configPath);

            // Write ALL patches for this option (e.g. aspect ratio + widescreen patches)
            for (const auto& patch : opt.patches)
                ini.setValue(patch.section, patch.key, patch.value);

            ini.save(configPath);
            break;
        }
    }
    setStatus("Aspect ratio settings saved.");
}
```

- [ ] **Step 8: Build to verify compilation**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds with no errors.

- [ ] **Step 9: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "feat: add quick resolution/aspect ratio methods to AppController"
```

---

### Task 2: Create ResolutionSettings.qml

**Files:**
- Create: `cpp/qml/AppUI/ResolutionSettings.qml`

- [ ] **Step 1: Create the resolution settings page**

Create `cpp/qml/AppUI/ResolutionSettings.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root

    // Installed emulators that have resolution options
    property var emuCards: []
    // Pending choices: { emuId: value }
    property var pendingChoices: ({})
    // 2D focus: which card and which pill
    property int focusCard: 0
    property int focusPill: 0
    // Reactivity trigger
    property int _v: 0

    Component.onCompleted: loadCards()

    function loadCards() {
        var all = app.allEmulatorStatus()
        var cards = []
        for (var i = 0; i < all.length; i++) {
            if (!all[i].installed) continue
            var opts = app.quickResolutionOptions(all[i].id)
            if (opts.length === 0) continue
            var current = app.currentResolution(all[i].id)
            cards.push({
                emuId: all[i].id,
                name: all[i].name,
                systems: all[i].systems,
                options: opts,
                current: current
            })
        }
        emuCards = cards

        // Initialize pending choices to current values
        var choices = {}
        for (var j = 0; j < cards.length; j++)
            choices[cards[j].emuId] = cards[j].current
        pendingChoices = choices
        _v++
    }

    function selectPill(cardIndex, pillIndex) {
        var card = emuCards[cardIndex]
        var choices = pendingChoices
        choices[card.emuId] = card.options[pillIndex].value
        pendingChoices = choices
        focusCard = cardIndex
        focusPill = pillIndex
        _v++
    }

    function save() {
        app.applyQuickResolution(pendingChoices)
    }

    // Preview image mapping — fill in paths per emuId + value to add images later
    // e.g. "pcsx2": { "2": "images/res/pcsx2-720p.png", "3": "images/res/pcsx2-1080p.png" }
    property var previewImages: ({})

    function previewSource(emuId, value) {
        if (previewImages[emuId] && previewImages[emuId][value])
            return previewImages[emuId][value]
        return ""
    }

    // Keyboard / controller navigation
    Keys.onUpPressed: {
        if (focusCard > 0) { focusCard--; focusPill = 0 }
    }
    Keys.onDownPressed: {
        if (focusCard < emuCards.length - 1) { focusCard++; focusPill = 0 }
    }
    Keys.onLeftPressed: {
        if (focusPill > 0) focusPill--
    }
    Keys.onRightPressed: {
        var opts = focusCard < emuCards.length ? emuCards[focusCard].options : []
        if (focusPill < opts.length - 1) focusPill++
    }
    Keys.onReturnPressed: {
        if (focusCard < emuCards.length) selectPill(focusCard, focusPill)
    }
    Keys.onEnterPressed: {
        if (focusCard < emuCards.length) selectPill(focusCard, focusPill)
    }

    Flickable {
        anchors.fill: parent
        contentHeight: contentCol.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 4
                radius: 2
                color: SettingsTheme.textGhost
                opacity: 0.6
            }
            background: Rectangle { color: "transparent" }
        }

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: 0

            // Cards flow
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 20
                spacing: 28

                Repeater {
                    model: root.emuCards

                    delegate: Item {
                        width: 420
                        height: cardCol.height

                        property int cardIndex: index
                        property var cardData: modelData
                        property string selectedValue: {
                            void(root._v)
                            return root.pendingChoices[cardData.emuId] || cardData.current
                        }

                        Column {
                            id: cardCol
                            width: parent.width
                            spacing: 0

                            // Emulator label
                            Row {
                                spacing: 6
                                bottomPadding: 8

                                Text {
                                    text: cardData.systems
                                    color: SettingsTheme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: cardData.name
                                    color: SettingsTheme.textDim
                                    font.pixelSize: 12
                                    anchors.baseline: parent.children[0].baseline
                                }
                            }

                            // Preview image area (14:9 aspect ratio)
                            Rectangle {
                                width: parent.width
                                height: width * 9 / 14
                                radius: 8
                                color: "#0a0a14"

                                Image {
                                    anchors.fill: parent
                                    source: root.previewSource(cardData.emuId, selectedValue)
                                    fillMode: Image.PreserveAspectCrop
                                    visible: source !== ""
                                }

                                // Placeholder when no image
                                Text {
                                    anchors.centerIn: parent
                                    text: "Preview"
                                    color: SettingsTheme.textGhost
                                    font.pixelSize: 13
                                    visible: root.previewSource(cardData.emuId, selectedValue) === ""
                                }
                            }

                            // Pill buttons
                            Row {
                                width: parent.width
                                spacing: 6
                                topPadding: 10

                                Repeater {
                                    model: cardData.options

                                    delegate: Rectangle {
                                        width: (parent.width - (cardData.options.length - 1) * 6) / cardData.options.length
                                        height: 32
                                        radius: SettingsTheme.pillRadius

                                        property bool isSelected: {
                                            void(root._v)
                                            return selectedValue === modelData.value
                                        }
                                        property bool isFocused: root.activeFocus
                                                                 && root.focusCard === cardIndex
                                                                 && root.focusPill === index

                                        color: isSelected ? SettingsTheme.accent : SettingsTheme.card
                                        border.width: isFocused ? 2 : 1
                                        border.color: isFocused ? SettingsTheme.focusBorder
                                                                 : (isSelected ? SettingsTheme.accent : SettingsTheme.border)

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.label
                                            color: isSelected ? "#ffffff" : SettingsTheme.textMuted
                                            font.pixelSize: 13
                                            font.weight: isSelected ? Font.DemiBold : Font.Normal
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.selectPill(cardIndex, index)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Save button
            Item {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.bottomMargin: 20
                Layout.rightMargin: 24
                height: 40

                Rectangle {
                    anchors.right: parent.right
                    width: 100
                    height: 36
                    radius: SettingsTheme.buttonRadius
                    color: SettingsTheme.accent

                    Text {
                        anchors.centerIn: parent
                        text: "Save"
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.save()
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 2: Build to verify no QML syntax errors**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Note: The file won't be part of the build yet (CMakeLists not updated), but ensure no obvious issues.

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/ResolutionSettings.qml
git commit -m "feat: add ResolutionSettings.qml card-based page"
```

---

### Task 3: Create AspectRatioSettings.qml

**Files:**
- Create: `cpp/qml/AppUI/AspectRatioSettings.qml`

- [ ] **Step 1: Create the aspect ratio settings page**

Create `cpp/qml/AppUI/AspectRatioSettings.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root

    // Installed emulators that have aspect ratio options
    property var emuCards: []
    // Pending choices: { emuId: label }
    property var pendingChoices: ({})
    // 2D focus: which card and which pill
    property int focusCard: 0
    property int focusPill: 0
    // Reactivity trigger
    property int _v: 0

    Component.onCompleted: loadCards()

    function loadCards() {
        var all = app.allEmulatorStatus()
        var cards = []
        for (var i = 0; i < all.length; i++) {
            if (!all[i].installed) continue
            var opts = app.quickAspectRatioOptions(all[i].id)
            if (opts.length === 0) continue
            var current = app.currentAspectRatio(all[i].id)
            cards.push({
                emuId: all[i].id,
                name: all[i].name,
                systems: all[i].systems,
                options: opts,
                current: current
            })
        }
        emuCards = cards

        // Initialize pending choices to current values
        var choices = {}
        for (var j = 0; j < cards.length; j++)
            choices[cards[j].emuId] = cards[j].current
        pendingChoices = choices
        _v++
    }

    function selectPill(cardIndex, pillIndex) {
        var card = emuCards[cardIndex]
        var choices = pendingChoices
        choices[card.emuId] = card.options[pillIndex].label
        pendingChoices = choices
        focusCard = cardIndex
        focusPill = pillIndex
        _v++
    }

    function save() {
        app.applyQuickAspectRatio(pendingChoices)
    }

    // Preview image mapping — fill in paths per emuId + label to add images later
    // e.g. "pcsx2": { "4:3": "images/ar/pcsx2-4x3.png", "16:9": "images/ar/pcsx2-16x9.png" }
    property var previewImages: ({})

    function previewSource(emuId, label) {
        if (previewImages[emuId] && previewImages[emuId][label])
            return previewImages[emuId][label]
        return ""
    }

    // Keyboard / controller navigation
    Keys.onUpPressed: {
        if (focusCard > 0) { focusCard--; focusPill = 0 }
    }
    Keys.onDownPressed: {
        if (focusCard < emuCards.length - 1) { focusCard++; focusPill = 0 }
    }
    Keys.onLeftPressed: {
        if (focusPill > 0) focusPill--
    }
    Keys.onRightPressed: {
        var opts = focusCard < emuCards.length ? emuCards[focusCard].options : []
        if (focusPill < opts.length - 1) focusPill++
    }
    Keys.onReturnPressed: {
        if (focusCard < emuCards.length) selectPill(focusCard, focusPill)
    }
    Keys.onEnterPressed: {
        if (focusCard < emuCards.length) selectPill(focusCard, focusPill)
    }

    Flickable {
        anchors.fill: parent
        contentHeight: contentCol.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 4
                radius: 2
                color: SettingsTheme.textGhost
                opacity: 0.6
            }
            background: Rectangle { color: "transparent" }
        }

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: 0

            // Cards flow
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 20
                spacing: 28

                Repeater {
                    model: root.emuCards

                    delegate: Item {
                        width: 420
                        height: cardCol.height

                        property int cardIndex: index
                        property var cardData: modelData
                        property string selectedLabel: {
                            void(root._v)
                            return root.pendingChoices[cardData.emuId] || cardData.current
                        }

                        Column {
                            id: cardCol
                            width: parent.width
                            spacing: 0

                            // Emulator label
                            Row {
                                spacing: 6
                                bottomPadding: 8

                                Text {
                                    text: cardData.systems
                                    color: SettingsTheme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: cardData.name
                                    color: SettingsTheme.textDim
                                    font.pixelSize: 12
                                    anchors.baseline: parent.children[0].baseline
                                }
                            }

                            // Preview image area (14:9 aspect ratio)
                            Rectangle {
                                width: parent.width
                                height: width * 9 / 14
                                radius: 8
                                color: "#0a0a14"

                                Image {
                                    anchors.fill: parent
                                    source: root.previewSource(cardData.emuId, selectedLabel)
                                    fillMode: Image.PreserveAspectCrop
                                    visible: source !== ""
                                }

                                // Placeholder when no image
                                Text {
                                    anchors.centerIn: parent
                                    text: "Preview"
                                    color: SettingsTheme.textGhost
                                    font.pixelSize: 13
                                    visible: root.previewSource(cardData.emuId, selectedLabel) === ""
                                }
                            }

                            // Pill buttons
                            Row {
                                width: parent.width
                                spacing: 6
                                topPadding: 10

                                Repeater {
                                    model: cardData.options

                                    delegate: Rectangle {
                                        width: (parent.width - (cardData.options.length - 1) * 6) / cardData.options.length
                                        height: 32
                                        radius: SettingsTheme.pillRadius

                                        property bool isSelected: {
                                            void(root._v)
                                            return selectedLabel === modelData.label
                                        }
                                        property bool isFocused: root.activeFocus
                                                                 && root.focusCard === cardIndex
                                                                 && root.focusPill === index

                                        color: isSelected ? SettingsTheme.accent : SettingsTheme.card
                                        border.width: isFocused ? 2 : 1
                                        border.color: isFocused ? SettingsTheme.focusBorder
                                                                 : (isSelected ? SettingsTheme.accent : SettingsTheme.border)

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.label
                                            color: isSelected ? "#ffffff" : SettingsTheme.textMuted
                                            font.pixelSize: 13
                                            font.weight: isSelected ? Font.DemiBold : Font.Normal
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.selectPill(cardIndex, index)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Save button
            Item {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.bottomMargin: 20
                Layout.rightMargin: 24
                height: 40

                Rectangle {
                    anchors.right: parent.right
                    width: 100
                    height: 36
                    radius: SettingsTheme.buttonRadius
                    color: SettingsTheme.accent

                    Text {
                        anchors.centerIn: parent
                        text: "Save"
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.save()
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add cpp/qml/AppUI/AspectRatioSettings.qml
git commit -m "feat: add AspectRatioSettings.qml card-based page"
```

---

### Task 4: Register New QML Files in CMakeLists.txt

**Files:**
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Add the two new QML files to the AppUI module**

In `cpp/CMakeLists.txt`, find the `QML_FILES` list under the `qt_add_qml_module(appui_backing ...)` block. After the line `qml/AppUI/SettingsOverlay.qml` (line 178), add:

```cmake
        qml/AppUI/ResolutionSettings.qml
        qml/AppUI/AspectRatioSettings.qml
```

- [ ] **Step 2: Build to verify registration**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/CMakeLists.txt
git commit -m "build: register ResolutionSettings and AspectRatioSettings in CMakeLists"
```

---

### Task 5: Update SettingsOverlay with New Categories, Pages, and Exit Dialog

**Files:**
- Modify: `cpp/qml/AppUI/SettingsOverlay.qml`

- [ ] **Step 1: Update categoryCount from 4 to 7**

In `SettingsOverlay.qml`, change line 14:

```qml
    readonly property int categoryCount: 7
```

- [ ] **Step 2: Add exit dialog visibility property**

After the `categoryCount` line, add:

```qml
    property bool exitDialogVisible: false
```

- [ ] **Step 3: Update the header text mapping**

Replace the header text block (lines 221–228) with:

```qml
                    Text {
                        text: {
                            if (overlay.selectedCategory === 0) return "Emulators"
                            if (overlay.selectedCategory === 1) return "Paths"
                            if (overlay.selectedCategory === 2) return "Scraper"
                            if (overlay.selectedCategory === 3) return "Themes"
                            if (overlay.selectedCategory === 4) return "Resolution"
                            if (overlay.selectedCategory === 5) return "Aspect Ratio"
                            return "Settings"
                        }
                        color: SettingsTheme.text
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }
```

- [ ] **Step 4: Add three new ListElement entries to the category ListModel**

After the Themes `ListElement` (line 355), add:

```qml
                        ListElement { name: "Resolution";    icon: "\uD83D\uDDA5"; subtitle: "Quick resolution settings";    catIndex: 4 }
                        ListElement { name: "Aspect Ratio";  icon: "\u2B1C";       subtitle: "Quick aspect ratio settings";  catIndex: 5 }
                        ListElement { name: "Exit";          icon: "\u23FB";        subtitle: "Close the application";        catIndex: 6 }
```

- [ ] **Step 5: Update selectCategory to handle new indices**

Replace the `selectCategory` function body (lines 326–333) with:

```qml
            function selectCategory(idx) {
                overlay._savedFocusIndex = focusIndex
                overlay.selectedCategory = idx
                if (idx === 0) panelStack.push(emuPageComponent)
                else if (idx === 1) panelStack.push(pathsPageComponent)
                else if (idx === 2) panelStack.push(scraperPageComponent)
                else if (idx === 3) panelStack.push(themesPageComponent)
                else if (idx === 4) panelStack.push(resolutionPageComponent)
                else if (idx === 5) panelStack.push(aspectRatioPageComponent)
                else if (idx === 6) overlay.exitDialogVisible = true
            }
```

- [ ] **Step 6: Add the Exit chevron suppression**

In the FocusableItem delegate for the category Repeater, the chevron `Text` element (line 394) should be hidden for the Exit entry. Replace:

```qml
                            Text {
                                text: "\u203A"
                                color: SettingsTheme.textDim
                                font.pixelSize: 22
                            }
```

With:

```qml
                            Text {
                                text: "\u203A"
                                color: SettingsTheme.textDim
                                font.pixelSize: 22
                                visible: model.catIndex !== 6
                            }
```

- [ ] **Step 7: Add the two new page components**

After the `themesPageComponent` Component block (line 434), add:

```qml
    Component {
        id: resolutionPageComponent
        ResolutionSettings {}
    }

    Component {
        id: aspectRatioPageComponent
        AspectRatioSettings {}
    }
```

- [ ] **Step 8: Add the exit confirmation dialog**

Before the final closing brace of the FocusScope (end of file), add:

```qml
    // --- Exit confirmation dialog ---
    Rectangle {
        id: exitDialog
        anchors.fill: parent
        color: "#000000"
        opacity: overlay.exitDialogVisible ? 0.7 : 0
        visible: opacity > 0
        z: 200

        Behavior on opacity { NumberAnimation { duration: SettingsTheme.animFast } }

        MouseArea {
            anchors.fill: parent
            onClicked: overlay.exitDialogVisible = false
        }

        FocusScope {
            anchors.centerIn: parent
            width: 320
            height: dialogCol.height + 48
            focus: overlay.exitDialogVisible
            z: 201

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                    overlay.exitDialogVisible = false
                    event.accepted = true
                }
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    Qt.quit()
                    event.accepted = true
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: 12
                color: SettingsTheme.surface
                border.width: 1
                border.color: SettingsTheme.border

                Column {
                    id: dialogCol
                    anchors.centerIn: parent
                    spacing: 24

                    Text {
                        text: "Exit Application?"
                        color: SettingsTheme.text
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Row {
                        spacing: 12
                        anchors.horizontalCenter: parent.horizontalCenter

                        // Cancel button
                        Rectangle {
                            width: 100
                            height: 36
                            radius: SettingsTheme.buttonRadius
                            color: SettingsTheme.card
                            border.width: 1
                            border.color: SettingsTheme.border

                            Text {
                                anchors.centerIn: parent
                                text: "Cancel"
                                color: SettingsTheme.text
                                font.pixelSize: 14
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: overlay.exitDialogVisible = false
                            }
                        }

                        // Exit button
                        Rectangle {
                            width: 100
                            height: 36
                            radius: SettingsTheme.buttonRadius
                            color: SettingsTheme.accent

                            Text {
                                anchors.centerIn: parent
                                text: "Exit"
                                color: "#ffffff"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: Qt.quit()
                            }
                        }
                    }
                }
            }
        }
    }
```

- [ ] **Step 9: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds.

- [ ] **Step 10: Manual test**

Run the app:
```bash
cd cpp && ./build/EmulatorFrontend
```

Verify:
1. Settings overlay shows 7 categories: Emulators, Paths, Scraper, Themes, Resolution, Aspect Ratio, Exit
2. Resolution page shows cards for installed emulators with pill buttons
3. Aspect Ratio page shows cards for installed emulators with pill buttons
4. Selecting pills updates the highlight, Save writes to INI
5. Exit shows confirmation dialog, Cancel dismisses, Exit closes the app
6. Escape/B-button dismisses the exit dialog
7. Controller navigation works (d-pad between cards/pills, A to select)

- [ ] **Step 11: Commit**

```bash
git add cpp/qml/AppUI/SettingsOverlay.qml
git commit -m "feat: add Resolution, Aspect Ratio, and Exit to settings overlay"
```

---

Plan complete and saved to `docs/superpowers/plans/2026-04-03-quick-settings-and-exit.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?