# Emulator Manage Grid — Larger Rows Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Scale up the rows on the Emulators management page (`EmulatorManageGrid.qml`) by approximately 1.5× so each emulator is easier to read.

**Architecture:** Single QML file, two row templates (`Repeater` delegate and the "More Emulators / Coming Soon" placeholder) receive the same set of sizing changes. Pure visual tuning — no structural, behavioral, or routing changes. Focus navigation, `MouseArea` clicks, and the `FocusableItem` focus ring all adapt to the new row height automatically.

**Tech Stack:** Qt6 QML (Quick + Layouts).

**Spec:** `docs/superpowers/specs/2026-04-10-emulator-manage-grid-larger-rows-design.md`

---

## File Structure

- **Modify:** `cpp/qml/AppUI/EmulatorManageGrid.qml`
  - Delegate row for real emulators (`Repeater` delegate, lines ~49–156)
  - "More Emulators / Coming Soon" placeholder row (lines ~160–213)

No new files. No header changes. No C++ or CMake changes.

---

## Task 1: Scale up the real-emulator delegate row

**Files:**
- Modify: `cpp/qml/AppUI/EmulatorManageGrid.qml:49-156`

- [ ] **Step 1: Read the current delegate to confirm exact text**

Read `cpp/qml/AppUI/EmulatorManageGrid.qml` lines 49–156. Confirm the delegate structure is:

```
delegate: FocusableItem {
    id: rowItem
    Layout.fillWidth: true
    Layout.leftMargin: 20
    Layout.rightMargin: 20
    Layout.preferredHeight: 72
    ...
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 14

        Rectangle { width: 48; height: 48; radius: 8; ... }   // logo
        ColumnLayout { ... Title 15 / system 12 / description 11 ... }
        Rectangle { ... Badge height 26, badgeLabel.width + 20, 11px ... }
        Text { text: "\u203A"; font.pixelSize: 20 }            // chevron
    }
    ...
}
```

- [ ] **Step 2: Replace the delegate with the scaled version**

Edit `cpp/qml/AppUI/EmulatorManageGrid.qml`.

Old string:

```qml
                delegate: FocusableItem {
                    id: rowItem
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.preferredHeight: 72
                    isFocused: index === root.focusIndex

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 14

                        // Logo area
                        Rectangle {
                            width: 48
                            height: 48
                            radius: 8
                            color: SettingsTheme.border

                            Image {
                                anchors.centerIn: parent
                                width: parent.width - 10
                                height: parent.height - 10
                                source: EmulatorLogos.logoForEmu(modelData.id)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                                visible: source !== ""
                            }

                            // Fallback emoji if no logo
                            Text {
                                anchors.centerIn: parent
                                text: "\uD83C\uDFAE"
                                font.pixelSize: 22
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
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: modelData.system || ""
                                color: SettingsTheme.textDim
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text !== ""
                            }
                            Text {
                                text: modelData.description || ""
                                color: SettingsTheme.textFaint
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text !== ""
                            }
                        }

                        // Install status badge
                        Rectangle {
                            width: badgeLabel.width + 20
                            height: 26
                            radius: SettingsTheme.pillRadius
                            color: modelData.installed ? SettingsTheme.successDim : SettingsTheme.accentDim

                            Text {
                                id: badgeLabel
                                anchors.centerIn: parent
                                text: modelData.installed ? "Installed" : "Not Installed"
                                color: modelData.installed ? SettingsTheme.success : SettingsTheme.accent
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }

                        // Chevron
                        Text {
                            text: "\u203A"
                            color: SettingsTheme.textGhost
                            font.pixelSize: 20
                        }
                    }
```

New string:

```qml
                delegate: FocusableItem {
                    id: rowItem
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.preferredHeight: 108
                    isFocused: index === root.focusIndex

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
                                anchors.centerIn: parent
                                width: parent.width - 12
                                height: parent.height - 12
                                source: EmulatorLogos.logoForEmu(modelData.id)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                                visible: source !== ""
                            }

                            // Fallback emoji if no logo
                            Text {
                                anchors.centerIn: parent
                                text: "\uD83C\uDFAE"
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
                            text: "\u203A"
                            color: SettingsTheme.textGhost
                            font.pixelSize: 26
                        }
                    }
```

Notes:
- Every change is purely numeric; no properties added or removed.
- `MouseArea` block immediately after this region (`hoverEnabled`, `onClicked` that sets `focusIndex` and emits `emulatorSelected`) is **not** part of the replacement — leave it exactly as is.
- `FocusableItem`'s `isFocused: index === root.focusIndex` stays unchanged.

---

## Task 2: Scale up the "Coming Soon" placeholder row

**Files:**
- Modify: `cpp/qml/AppUI/EmulatorManageGrid.qml:160-213`

- [ ] **Step 1: Read the current placeholder to confirm exact text**

Read `cpp/qml/AppUI/EmulatorManageGrid.qml` lines 160–213. Confirm the structure is:

```
// "Coming Soon" placeholder row
FocusableItem {
    Layout.fillWidth: true
    Layout.leftMargin: 20
    Layout.rightMargin: 20
    Layout.preferredHeight: 72
    isFocused: false
    opacity: 0.35

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 14

        Rectangle { width: 48; height: 48; radius: 8; color: border; Text { text: "\u2795"; font.pixelSize: 20 } }
        ColumnLayout { Text { "More Emulators" 15 Medium } Text { "Coming Soon" 12 } }
        Item { Layout.fillWidth: true }
        Text { "\u203A" 20 }
    }
}
```

- [ ] **Step 2: Replace the placeholder with the scaled version**

Edit `cpp/qml/AppUI/EmulatorManageGrid.qml`.

Old string:

```qml
            // "Coming Soon" placeholder row
            FocusableItem {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.preferredHeight: 72
                isFocused: false
                opacity: 0.35

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 14

                    Rectangle {
                        width: 48
                        height: 48
                        radius: 8
                        color: SettingsTheme.border

                        Text {
                            anchors.centerIn: parent
                            text: "\u2795"
                            font.pixelSize: 20
                            color: SettingsTheme.textGhost
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: "More Emulators"
                            color: SettingsTheme.text
                            font.pixelSize: 15
                            font.weight: Font.Medium
                        }
                        Text {
                            text: "Coming Soon"
                            color: SettingsTheme.textDim
                            font.pixelSize: 12
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "\u203A"
                        color: SettingsTheme.textGhost
                        font.pixelSize: 20
                    }
                }
            }
```

New string:

```qml
            // "Coming Soon" placeholder row
            FocusableItem {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.preferredHeight: 108
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
                            text: "\u2795"
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
                        text: "\u203A"
                        color: SettingsTheme.textGhost
                        font.pixelSize: 26
                    }
                }
            }
```

Notes:
- The placeholder has no description line (unlike the delegate), so the `15px` description size doesn't apply here.
- `opacity: 0.35` (the greyed-out look) is preserved.
- The trailing `Item { Layout.fillWidth: true }` spacer before the chevron is preserved — it's what pushes the chevron to the right edge since the placeholder has no badge.

---

## Task 3: Build and commit

**Files:** none (verification + git)

- [ ] **Step 1: Build**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project/cpp && cmake --build build 2>&1 | tail -20
```

Expected: build succeeds with no errors or warnings from `EmulatorManageGrid.qml`. If the QML parser reports an error, re-read the edit regions and fix brace/indent mismatches.

- [ ] **Step 2: Review the diff**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project && git diff cpp/qml/AppUI/EmulatorManageGrid.qml
```

Expected: two hunks, one for each row template, each containing only numeric changes (heights, widths, radii, margins, spacings, font sizes). No color changes, no new properties, no removed properties.

- [ ] **Step 3: Stage and commit**

```sh
cd /Users/mark/Documents/RetroNest-Project && \
  git add cpp/qml/AppUI/EmulatorManageGrid.qml && \
  git commit -m "$(cat <<'EOF'
feat(settings): enlarge Emulators management page rows by ~1.5x

Scale EmulatorManageGrid.qml rows so each emulator is easier to read:
row height 72->108, logo tile 48->72 (with larger inner image and
bigger fallback emoji), title 15->22, system 12->16, description
11->15, badge height 26->36 with bigger horizontal padding and 15px
label, chevron 20->26, and inner row padding/spacing 14->18.

Applied identically to the Repeater delegate and the "Coming Soon"
placeholder row so both remain visually consistent. No behavior
changes — focus navigation, mouse clicks, and the FocusableItem
focus ring all adapt to the new row height automatically.
EOF
)"
```

Expected: single commit, one file modified.

---

## Task 4: Manual GUI verification (user-side)

**Files:** none

- [ ] **Step 1: Note the pending manual verification**

Launching the app and eyeballing the page cannot be done headless. After the build passes and the commit is created, report that the following steps are pending user-side verification:

1. Launch the app.
2. Navigate to Settings → Emulators.
3. Confirm each row is visibly larger — row ~108px tall, logo tile 72×72, title noticeably bigger (22px), system and description lines legible (16px / 15px), "Installed / Not Installed" badge text readable (15px) inside a 36px pill with more horizontal padding.
4. Move focus up/down with keyboard and a controller. Confirm the focus ring still wraps each row cleanly with no clipping or overshoot.
5. Confirm the chevron still renders on the right edge of each row without clipping or overlapping the badge.
6. Confirm the "Coming Soon" placeholder row visually matches the three installed rows in height and text scale.
