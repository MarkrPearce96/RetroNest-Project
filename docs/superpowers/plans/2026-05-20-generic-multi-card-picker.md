# GenericMultiCardPicker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land a single `GenericMultiCardPicker.qml` component and migrate both `ResolutionSettings.qml` and `AspectRatioSettings.qml` onto it.

**Architecture:** One QML `FocusScope`-based component owns the 2D card×pill keyboard focus model, the `Flow` of preview-image cards, the pill-button row, the Save button, and the pending-choices buffer. Caller pages supply four backend hooks (options loader, current loader, apply choices, option key field) plus an optional preview-images map.

**Tech Stack:** Qt 6 / QML, CMake (`qt_add_qml_module`). Zero new dependencies. No QML unit-test infrastructure in this codebase — verification is `cmake --build cpp/build-x86_64` followed by `macdeployqt` + `codesign --force --deep --sign -` per the `build-cmake-needs-macdeployqt` memory entry, then manual smoke test of each page.

**Spec:** `docs/superpowers/specs/2026-05-20-generic-multi-card-picker-design.md`

---

## File Structure

**Create (1):**
- `cpp/qml/AppUI/GenericMultiCardPicker.qml` — the new component

**Modify (3):**
- `cpp/qml/AppUI/ResolutionSettings.qml` — collapse onto GenericMultiCardPicker
- `cpp/qml/AppUI/AspectRatioSettings.qml` — collapse onto GenericMultiCardPicker
- `cpp/CMakeLists.txt` — register `GenericMultiCardPicker.qml` in the AppUI module's `QML_FILES` list

**Out of scope (per spec):**
- `cpp/qml/SetupWizard/ResolutionPage.qml` — wizard, different theme/backend/visual
- `cpp/qml/SetupWizard/AspectRatioPage.qml` — wizard, different theme/backend/visual

---

## Task 1: Create `GenericMultiCardPicker.qml` and wire into CMake

**Files:**
- Create: `cpp/qml/AppUI/GenericMultiCardPicker.qml`
- Modify: `cpp/CMakeLists.txt` (around lines 343–344 — add new file to AppUI module's `QML_FILES`)

- [ ] **Step 1: Write `GenericMultiCardPicker.qml`**

Create `cpp/qml/AppUI/GenericMultiCardPicker.qml` with this exact content:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root
    focus: true

    // --- Required backend hooks ---
    required property var optionsLoader     // (emuId) => [{label, value}, ...]
    required property var currentLoader     // (emuId) => chosenKey string
    required property var applyChoices      // (choices: {emuId: chosenKey}) => void
    required property string optionKeyField // "value" or "label"

    // --- Optional content ---
    property var previewImages: ({})        // { emuId: { chosenKey: imagePath } }

    // --- Layout knobs ---
    property int cardWidth: 385
    property int colCount: 2
    property real previewAspect: 14/9       // height = width / previewAspect

    // --- State ---
    property var emuCards: []
    property var pendingChoices: ({})
    property int focusCard: 0
    property int focusPill: 0
    property bool focusSave: false

    Component.onCompleted: { loadCards(); root.forceActiveFocus() }
    StackView.onActivated: root.forceActiveFocus()

    function loadCards() {
        var all = app.allEmulatorStatus()
        var cards = []
        for (var i = 0; i < all.length; i++) {
            if (!all[i].installed) continue
            var opts = root.optionsLoader(all[i].id)
            if (opts.length === 0) continue
            cards.push({
                emuId: all[i].id,
                name: all[i].name,
                systems: all[i].systems,
                options: opts,
                current: root.currentLoader(all[i].id)
            })
        }
        emuCards = cards

        var choices = {}
        for (var j = 0; j < cards.length; j++)
            choices[cards[j].emuId] = cards[j].current
        pendingChoices = choices
    }

    function selectPill(cardIndex, pillIndex) {
        var card = emuCards[cardIndex]
        var choices = Object.assign({}, pendingChoices)
        choices[card.emuId] = card.options[pillIndex][root.optionKeyField]
        pendingChoices = choices
        focusCard = cardIndex
        focusPill = pillIndex
    }

    function save() { root.applyChoices(pendingChoices) }

    function previewSource(emuId, chosenKey) {
        if (previewImages[emuId] && previewImages[emuId][chosenKey])
            return previewImages[emuId][chosenKey]
        return ""
    }

    Keys.onUpPressed: {
        if (focusSave) {
            focusSave = false
        } else if (focusCard - colCount >= 0) {
            focusCard -= colCount; focusPill = 0
        }
    }
    Keys.onDownPressed: {
        if (focusSave) return
        if (focusCard + colCount < emuCards.length) {
            focusCard += colCount; focusPill = 0
        } else {
            focusSave = true
        }
    }
    Keys.onLeftPressed: {
        if (focusSave || emuCards.length === 0) return
        if (focusPill > 0) {
            focusPill--
        } else if (focusCard > 0) {
            focusCard--
            focusPill = emuCards[focusCard].options.length - 1
        }
    }
    Keys.onRightPressed: {
        if (focusSave || emuCards.length === 0) return
        var opts = emuCards[focusCard].options
        if (focusPill < opts.length - 1) {
            focusPill++
        } else if (focusCard < emuCards.length - 1) {
            focusCard++
            focusPill = 0
        }
    }
    function _activateFocused() {
        if (focusSave) { save(); return }
        if (focusCard < emuCards.length) selectPill(focusCard, focusPill)
    }
    Keys.onReturnPressed: _activateFocused()
    Keys.onEnterPressed: _activateFocused()

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

            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 20
                spacing: 28

                Repeater {
                    model: root.emuCards

                    delegate: Item {
                        width: root.cardWidth
                        height: cardCol.height

                        property int cardIndex: index
                        property var cardData: modelData
                        property string selectedKey: root.pendingChoices[cardData.emuId] || cardData.current

                        Column {
                            id: cardCol
                            width: parent.width
                            spacing: 0

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

                            Rectangle {
                                width: parent.width
                                height: width / root.previewAspect
                                radius: 8
                                color: SettingsTheme.background

                                Image {
                                    anchors.fill: parent
                                    source: root.previewSource(cardData.emuId, selectedKey)
                                    fillMode: Image.PreserveAspectCrop
                                    visible: source !== ""
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "Preview"
                                    color: SettingsTheme.textGhost
                                    font.pixelSize: 13
                                    visible: root.previewSource(cardData.emuId, selectedKey) === ""
                                }
                            }

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

                                        property bool isSelected: selectedKey === modelData[root.optionKeyField]
                                        property bool isFocused: root.activeFocus
                                                                 && !root.focusSave
                                                                 && root.focusCard === cardIndex
                                                                 && root.focusPill === index

                                        color: isSelected ? SettingsTheme.accent : SettingsTheme.border
                                        border.width: (isFocused || pillMa.containsMouse) ? 3 : 0
                                        border.color: (isFocused || pillMa.containsMouse) ? SettingsTheme.text : "transparent"
                                        scale: (isFocused || pillMa.containsMouse) ? 1.05 : 1.0

                                        Behavior on scale { NumberAnimation { duration: 100 } }
                                        Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.label
                                            color: isSelected ? SettingsTheme.background : SettingsTheme.textMuted
                                            font.pixelSize: 13
                                            font.weight: isSelected ? Font.DemiBold : Font.Normal
                                            Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }
                                        }

                                        MouseArea {
                                            id: pillMa
                                            anchors.fill: parent
                                            hoverEnabled: true
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
                    color: (root.focusSave || saveMa.containsMouse)
                           ? Qt.lighter(SettingsTheme.accent, 1.2) : SettingsTheme.accent
                    border.width: root.focusSave ? 2 : 0
                    border.color: SettingsTheme.focusBorder

                    Behavior on color { ColorAnimation { duration: SettingsTheme.animFast } }

                    Text {
                        anchors.centerIn: parent
                        text: "Save"
                        color: SettingsTheme.background
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: saveMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.save()
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 2: Register the new file in `cpp/CMakeLists.txt`**

Open `cpp/CMakeLists.txt`. After the line `qml/AppUI/GenericListPage.qml` (around line 344, added in the previous refactor), add `qml/AppUI/GenericMultiCardPicker.qml`. The block should look like:

```cmake
        qml/AppUI/ButtonHints.qml
        qml/AppUI/GenericListPage.qml
        qml/AppUI/GenericMultiCardPicker.qml
        qml/AppUI/FocusableItem.qml
```

- [ ] **Step 3: Build to confirm the new QML compiles**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. Any QML compile error (typo, missing import, malformed binding) shows up here. Fix and re-run if needed.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/GenericMultiCardPicker.qml cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(ui): add GenericMultiCardPicker QML component

Skeleton component for collapsing ResolutionSettings + AspectRatio
Settings behind one shared shell. Owns the 2D card×pill focus model,
Flow of preview-image cards, pill row, and Save button. Backend wiring
via four function-property hooks (optionsLoader, currentLoader,
applyChoices, optionKeyField) and an optional previewImages map.

Not yet consumed by any caller; migrations follow.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Migrate `ResolutionSettings.qml`

**Files:**
- Modify: `cpp/qml/AppUI/ResolutionSettings.qml` (full rewrite, 299 → ~25 LOC)

- [ ] **Step 1: Replace `ResolutionSettings.qml`**

Overwrite the entire file with:

```qml
import QtQuick

GenericMultiCardPicker {
    optionsLoader:  (emuId) => app.quickResolutionOptions(emuId)
    currentLoader:  (emuId) => app.currentResolution(emuId)
    applyChoices:   (choices) => app.applyQuickResolution(choices)
    optionKeyField: "value"

    previewImages: ({
        "duckstation": {
            "2": "images/res/duckstation-720p.webp",
            "3": "images/res/duckstation-1080p.webp",
            "4": "images/res/duckstation-1440p.webp",
            "6": "images/res/duckstation-4k.webp"
        }
    })
}
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. Fix and re-run if QML compile errors appear.

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/ResolutionSettings.qml
git commit -m "$(cat <<'EOF'
refactor(ui): migrate ResolutionSettings onto GenericMultiCardPicker

299 → ~25 LOC. Caller now declares four backend hooks
(optionsLoader, currentLoader, applyChoices, optionKeyField) and the
preview-images dictionary. All 2D focus model, Flow chrome, pill
rendering, and Save button live in GenericMultiCardPicker.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Migrate `AspectRatioSettings.qml`

**Files:**
- Modify: `cpp/qml/AppUI/AspectRatioSettings.qml` (full rewrite, 299 → ~20 LOC)

- [ ] **Step 1: Replace `AspectRatioSettings.qml`**

Overwrite the entire file with:

```qml
import QtQuick

GenericMultiCardPicker {
    optionsLoader:  (emuId) => app.quickAspectRatioOptions(emuId)
    currentLoader:  (emuId) => app.currentAspectRatio(emuId)
    applyChoices:   (choices) => app.applyQuickAspectRatio(choices)
    optionKeyField: "label"

    previewImages: ({
        "pcsx2":       { "4:3": "images/ar/pcsx2-4x3.webp",       "16:9": "images/ar/pcsx2-16x9.webp" },
        "duckstation": { "4:3": "images/ar/duckstation-4x3.webp", "16:9": "images/ar/duckstation-16x9.webp" }
    })
}
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/AspectRatioSettings.qml
git commit -m "$(cat <<'EOF'
refactor(ui): migrate AspectRatioSettings onto GenericMultiCardPicker

299 → ~20 LOC. Same shape as the Resolution migration but with
optionKeyField "label" (AR options are keyed by their display label,
not a numeric value).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Deploy, sign, smoke-test, update roadmap memory

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`

- [ ] **Step 1: Kill any running RetroNest instance**

```bash
pkill -f "build-x86_64/RetroNest.app" 2>/dev/null
```

- [ ] **Step 2: Run macdeployqt + ad-hoc resign**

Per the `build-cmake-needs-macdeployqt` memory entry, this step is mandatory after `cmake --build` — without it, the bundle loads both Homebrew Qt and bundled Qt simultaneously and aborts.

```bash
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
```

Verify the binary's Qt refs are now relative:

```bash
otool -L cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest | grep -c "@executable_path/.*Qt"
```

Expected: a number ≥ 8 (Qt frameworks all loading via `@executable_path/../Frameworks/...`). If any path shows `/usr/local/Cellar/` or `/opt/homebrew/`, the deploy didn't finish.

- [ ] **Step 3: Launch and confirm not crashed**

```bash
open cpp/build-x86_64/RetroNest.app
sleep 5
pgrep -fl "build-x86_64/RetroNest.app/Contents/MacOS/RetroNest"
```

Expected: a `pgrep` hit. If empty (crashed), check the newest crash report at `~/Library/Logs/DiagnosticReports/RetroNest-*.ips` and investigate before continuing.

- [ ] **Step 4: Smoke test — verify the user drives this**

The controller running this plan should hand off to the user with these explicit checks:

**Settings → Resolution:**
1. Cards render for installed emulators that expose `quickResolutionOptions`.
2. Up/Down moves focus by full row of 2 cards. Down past the last row lands on Save (Save shows a 2-px accent border).
3. Left/Right cycles pills within the focused card. Right from the last pill of card N moves to the first pill of card N+1; Left from the first pill of card N+1 moves to the last pill of card N.
4. Selecting a DuckStation resolution pill (720p/1080p/1440p/4K) swaps the preview image.
5. Pressing Enter on Save calls `app.applyQuickResolution(...)` and writes the choices.
6. Mouse click on a pill selects it and moves focus.

**Settings → Aspect Ratio:**
1. Cards render for PCSX2 + DuckStation (the two emulators with AR options in the project right now).
2. Same Up/Down/Left/Right behavior as Resolution.
3. Selecting 4:3 vs 16:9 pills swaps the preview image (4:3 wide bars vs 16:9 fill).
4. Save calls `app.applyQuickAspectRatio(...)`.

If anything is broken, STOP and report it.

- [ ] **Step 5: Update the refactor-roadmap memory**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`:

Replace the line:
```
5. **`GenericMultiCardPicker.qml`** — `cpp/qml/AppUI/ResolutionSettings.qml` and `AspectRatioSettings.qml` ...
```

with:
```
5. ✅ **`GenericMultiCardPicker.qml`** — shipped 2026-05-20. New `cpp/qml/AppUI/GenericMultiCardPicker.qml` owns the 2D card×pill focus model, Flow of cards, pill row, Save button, and pending-choices buffer. Migrated: ResolutionSettings, AspectRatioSettings (both 299 → ~25 LOC). Caller pages now declare four backend hooks + a preview-images map. **Out of scope (deferred):** SetupWizard/ResolutionPage + AspectRatioPage — they use WizardTheme, `emulators.*` backend, immediate-write semantics, and visually different layouts (row-list vs grid-of-mini-preview-cards). Logged below as a follow-up if wizard parity is wanted later. Spec: `docs/superpowers/specs/2026-05-20-generic-multi-card-picker-design.md`. Plan: `docs/superpowers/plans/2026-05-20-generic-multi-card-picker.md`. Net ≈ −303 LOC.
```

Also update the file's frontmatter `description:` line to reflect items 1–5 shipped:
```
description: Ongoing generalization/cleanup roadmap for RetroNest. Tier 1 items 1-5 shipped 2026-05-20; items 6+ pending. Resume here when starting a new session on this work.
```

And add a new entry under the "Logged follow-ups" section:
```
- **SetupWizard quick-setting parity** — `SetupWizard/ResolutionPage.qml` + `AspectRatioPage.qml` use a different theme (`WizardTheme`), backend (`emulators.*`), write semantics (immediate, no Save button), and visual layout (one is a vertical row-list, the other is a 2-col grid with hand-drawn mini-previews). Could either (a) extract a thin wizard-flavored shared kernel between just those two files, or (b) extend `GenericMultiCardPicker` with theme+writeMode props. Design needed before implementation.
```

Update the MEMORY.md index entry to reflect the new state — find the `refactor-roadmap` line in `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md` and update it:

Replace:
```
- [Refactor roadmap](refactor-roadmap.md) — Multi-session generalization program. Tier 1 #1-4 shipped 2026-05-20; #5 GenericMultiCardPicker / Tier 2 items pending. Open here when resuming refactor work.
```

with:
```
- [Refactor roadmap](refactor-roadmap.md) — Multi-session generalization program. Tier 1 #1-5 shipped 2026-05-20; Tier 2 items (#6 HotkeyService extract, #7 BaseNotification, #8 AppController, #9 hotkeyVirtualKeyCode default, #10 NavigableGrid mixin) pending. Open here when resuming refactor work.
```

(Memory files live outside the repo — no git commit needed.)

---

## Self-review

**Spec coverage:**
- Component API (FocusScope, all required + optional props, layout knobs) → Task 1 ✓
- Internal behavior (loadCards, selectPill, save, previewSource) → Task 1 ✓
- 2D focus / keyboard nav rules → Task 1 ✓
- Save button behavior → Task 1 ✓
- ResolutionSettings caller code → Task 2 ✓
- AspectRatioSettings caller code → Task 3 ✓
- CMakeLists.txt registration → Task 1 ✓
- Build-and-launch reminder (macdeployqt + codesign) → Task 4 ✓
- Smoke-test checklist → Task 4 ✓
- Wizard pages out of scope → not in any code task (correctly absent); logged as memory follow-up → Task 4 ✓

**Placeholder scan:** No TBDs, no "TODO", no "implement later", no "similar to Task N". Every code step contains the full content the engineer needs.

**Type / name consistency:**
- `optionsLoader`, `currentLoader`, `applyChoices`, `optionKeyField` — same names in Task 1 (component), Task 2 (Resolution caller), Task 3 (AR caller).
- `previewImages` — same name across all tasks.
- `selectedKey` is the per-card local property; resolves via `pendingChoices[emuId] || cardData.current`.
- `optionKeyField` used in both `selectPill` (line writes the chosen key into pendingChoices) and the pill's `isSelected` binding (compares modelData[optionKeyField] to selectedKey). Consistent.

**One real risk:** The `Component.onCompleted: loadCards()` runs on every instantiation. If the caller pages are pushed/popped on `panelStack`, the component is destroyed and recreated each time — same behavior as before the refactor (the original files did the same), so no regression. Worth noting only as a future perf observation, not blocking.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-20-generic-multi-card-picker.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks. Fast iteration; each migration page verified independently.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch with checkpoints.

Which approach?
