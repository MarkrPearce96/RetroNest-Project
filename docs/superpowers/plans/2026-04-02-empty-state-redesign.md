# Empty State Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the "No games found" empty state on SystemPage.qml with a dark scrim overlay, three clearly labeled action buttons in a stacked layout, and full keyboard/controller navigation.

**Architecture:** Add `openRomFolder()` to ThemeContext (C++ backend), then replace the empty state Column in SystemPage.qml with a full-screen scrim overlay and a focusIndex-based nav system. The root Item keeps focus and routes key events to either the carousel (when games exist) or empty state buttons (when no games).

**Tech Stack:** C++17, Qt6/QML, QDesktopServices

---

### Task 1: Add `openRomFolder()` to ThemeContext

**Files:**
- Modify: `cpp/src/ui/theme_context.h:43-44`
- Modify: `cpp/src/ui/theme_context.cpp:128-134`

- [ ] **Step 1: Add the method declaration to the header**

In `cpp/src/ui/theme_context.h`, add the new method after `scanRomFolders()` (line 44):

```cpp
    Q_INVOKABLE void openRomFolder();
```

- [ ] **Step 2: Add the include and implementation to the cpp file**

In `cpp/src/ui/theme_context.cpp`, add these includes at the top (after the existing includes around line 5):

```cpp
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include "core/paths.h"
```

Then add the implementation after the `scanRomFolders()` method (after line 134):

```cpp
void ThemeContext::openRomFolder() {
    QString dir = Paths::romsDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
```

- [ ] **Step 3: Build and verify it compiles**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/theme_context.h cpp/src/ui/theme_context.cpp
git commit -m "feat: add openRomFolder() to ThemeContext"
```

---

### Task 2: Redesign the empty state in SystemPage.qml

**Files:**
- Modify: `themes/modern/SystemPage.qml:41-50` (root keyboard handlers)
- Modify: `themes/modern/SystemPage.qml:319-403` (empty state block)

- [ ] **Step 1: Add the `emptyFocusIndex` property to the root Item**

At the top of the root Item (after line 23, near the other properties), add:

```qml
    property int emptyFocusIndex: 0  // 0 = Open ROM Folder, 1 = Scan, 2 = Import
```

- [ ] **Step 2: Replace the root keyboard handlers**

Replace lines 41-50 (the existing `Keys.onLeftPressed` through `Keys.onEnterPressed`) with:

```qml
    // Keyboard navigation — routes to carousel or empty state
    Keys.onLeftPressed: {
        if (systemList.length > 0) root.carouselPrev()
        else if (root.emptyFocusIndex === 2) root.emptyFocusIndex = 1
    }
    Keys.onRightPressed: {
        if (systemList.length > 0) root.carouselNext()
        else if (root.emptyFocusIndex === 1) root.emptyFocusIndex = 2
    }
    Keys.onUpPressed: {
        if (systemList.length === 0 && root.emptyFocusIndex > 0)
            root.emptyFocusIndex = 0
    }
    Keys.onDownPressed: {
        if (systemList.length === 0 && root.emptyFocusIndex === 0)
            root.emptyFocusIndex = 1
    }
    Keys.onReturnPressed: {
        if (systemList.length > 0)
            themeContext.navigateToSystem(systemList[root.carouselIndex])
        else
            root.activateEmptyButton()
    }
    Keys.onEnterPressed: {
        if (systemList.length > 0)
            themeContext.navigateToSystem(systemList[root.carouselIndex])
        else
            root.activateEmptyButton()
    }

    function activateEmptyButton() {
        if (emptyFocusIndex === 0) themeContext.openRomFolder()
        else if (emptyFocusIndex === 1) themeContext.scanRomFolders()
        else if (emptyFocusIndex === 2) themeContext.importRoms()
    }
```

- [ ] **Step 3: Replace the empty state block**

Replace lines 319-403 (the entire `// ─── Empty state ───` section including all content down to and including the final `}` of the Column before line 404's root closing brace) with:

```qml
    // ─── Empty state ──────────────────────────────────────────────────────────
    Rectangle {
        id: emptyState
        anchors.fill: parent
        visible: systemList.length === 0
        color: "transparent"

        // Dark scrim overlay
        Rectangle {
            anchors.fill: parent
            color: "#1a1a2e"
            opacity: 0.85
        }

        // Content
        Column {
            anchors.centerIn: parent
            spacing: 24

            // Title
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "No games found"
                color: "#ffffff"
                font.pixelSize: 26
                font.weight: Font.Bold
            }

            // Instructions
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Add ROMs to your system folders, then scan to discover them.\nUse Open ROM Folder to find the right directory."
                color: Qt.rgba(1, 1, 1, 0.7)
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.6
            }

            // Primary button: Open ROM Folder
            Rectangle {
                id: openFolderBtn
                anchors.horizontalCenter: parent.horizontalCenter
                width: openFolderText.implicitWidth + 48
                height: 50
                radius: 10
                color: openFolderMa.containsMouse || root.emptyFocusIndex === 0
                       ? "#6b6be6" : "#5b5bd6"
                Behavior on color { ColorAnimation { duration: 100 } }

                // Focus glow
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: parent.radius + 4
                    color: "transparent"
                    border.width: 2
                    border.color: "#5b5bd6"
                    opacity: root.emptyFocusIndex === 0 ? 0.4 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                Text {
                    id: openFolderText
                    anchors.centerIn: parent
                    text: "Open ROM Folder"
                    color: "#ffffff"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    id: openFolderMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: themeContext.openRomFolder()
                }
            }

            // Secondary buttons row
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                // Scan for Games
                Rectangle {
                    id: scanBtn
                    width: scanText.implicitWidth + 40
                    height: 46
                    radius: 10
                    color: scanMa.containsMouse || root.emptyFocusIndex === 1
                           ? "#363660" : Qt.rgba(0.17, 0.17, 0.31, 0.8)
                    border.color: Qt.rgba(0.36, 0.36, 0.84, 0.35)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 100 } }

                    // Focus glow
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        radius: parent.radius + 4
                        color: "transparent"
                        border.width: 2
                        border.color: "#5b5bd6"
                        opacity: root.emptyFocusIndex === 1 ? 0.4 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    Text {
                        id: scanText
                        anchors.centerIn: parent
                        text: "Scan for Games"
                        color: Qt.rgba(1, 1, 1, 0.75)
                        font.pixelSize: 14
                    }
                    MouseArea {
                        id: scanMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: themeContext.scanRomFolders()
                    }
                }

                // Import ROMs
                Rectangle {
                    id: importBtn
                    width: importText.implicitWidth + 40
                    height: 46
                    radius: 10
                    color: importMa.containsMouse || root.emptyFocusIndex === 2
                           ? "#363660" : Qt.rgba(0.17, 0.17, 0.31, 0.8)
                    border.color: Qt.rgba(0.36, 0.36, 0.84, 0.35)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 100 } }

                    // Focus glow
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        radius: parent.radius + 4
                        color: "transparent"
                        border.width: 2
                        border.color: "#5b5bd6"
                        opacity: root.emptyFocusIndex === 2 ? 0.4 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    Text {
                        id: importText
                        anchors.centerIn: parent
                        text: "Import ROMs"
                        color: Qt.rgba(1, 1, 1, 0.75)
                        font.pixelSize: 14
                    }
                    MouseArea {
                        id: importMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: themeContext.importRoms()
                    }
                }
            }

            // Navigation hint
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "↑ ↓ Navigate    Enter Select"
                color: Qt.rgba(1, 1, 1, 0.35)
                font.pixelSize: 11
            }
        }
    }
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds (only C++ changes need compilation; QML is loaded at runtime).

- [ ] **Step 5: Run the app and visually verify**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

Verify:
- Dark scrim covers the background image
- "No games found" title is clearly readable
- Instructions text is visible
- Three buttons displayed: "Open ROM Folder" (top, purple), "Scan for Games" and "Import ROMs" (below, side-by-side)
- "Press Escape to open Settings" text is gone
- Up/Down arrow keys move focus between primary and secondary row
- Left/Right arrow keys move between the two secondary buttons
- Enter activates the focused button
- "Open ROM Folder" opens Finder to the roms directory
- Mouse hover still highlights buttons
- Focus glow appears on the currently focused button

- [ ] **Step 6: Commit**

```bash
git add themes/modern/SystemPage.qml
git commit -m "feat: redesign empty state with dark scrim, stacked buttons, and keyboard nav"
```
