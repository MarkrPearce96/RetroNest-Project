# Scraper UI Enhancements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dynamic game count to the scraper dashboard and redesign the progress page with a larger cover, stacked metadata, and a footer summary on completion.

**Architecture:** One new C++ method on `AppController` for game counting. All other changes are QML-only — replacing the progress page layout and adding a completion footer. No new files needed.

**Tech Stack:** C++17, Qt6/QML, existing `SettingsTheme` singleton

---

### Task 1: Add `scrapeGameCount` method to AppController

**Files:**
- Modify: `cpp/src/ui/app_controller.h:112` (add declaration after `allMediaTypes`)
- Modify: `cpp/src/ui/app_controller.cpp:836` (add implementation after `allMediaTypes`)

- [ ] **Step 1: Add the declaration to app_controller.h**

Add after line 112 (`Q_INVOKABLE QStringList allMediaTypes() const;`):

```cpp
    Q_INVOKABLE int scrapeGameCount(const QStringList& systems, const QString& gameFilter) const;
```

- [ ] **Step 2: Add the implementation to app_controller.cpp**

Add after the `allMediaTypes()` method (after line 838):

```cpp
int AppController::scrapeGameCount(const QStringList& systems, const QString& gameFilter) const {
    int count = 0;
    for (const auto& system : systems) {
        auto games = m_db->gamesBySystem(system);
        for (const auto& g : games) {
            if (gameFilter == "unscraped") {
                if (g.cover_path.isEmpty() || !QFileInfo::exists(g.cover_path))
                    count++;
            } else if (gameFilter == "favorites") {
                if (g.favorite)
                    count++;
            } else {
                count++;
            }
        }
    }
    return count;
}
```

- [ ] **Step 3: Add QFileInfo include if missing**

Check top of `app_controller.cpp` — it already includes `<QFileInfo>` via `scraper_service.h`. No action needed unless build fails.

- [ ] **Step 4: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```

Expected: Build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "feat: add scrapeGameCount method to AppController"
```

---

### Task 2: Add dynamic game count line to dashboard

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:994` (insert text above Start Scraping button)

- [ ] **Step 1: Add game count text above the Start Scraping button**

Insert before line 994 (`// ── START SCRAPING button ───────────────────────────────`), within the dashboard `ColumnLayout`:

```qml
                // ── GAME COUNT summary ─────────────────────────────────
                Text {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    horizontalAlignment: Text.AlignHCenter
                    text: {
                        var count = app.scrapeGameCount(root.selectedSystems, root.gameFilter)
                        if (count === 0) return "No games match this selection"
                        return count + (count === 1 ? " game" : " games") + " will be scraped"
                    }
                    color: SettingsTheme.textMuted
                    font.pixelSize: 13
                }
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```

Expected: Build succeeds.

- [ ] **Step 3: Run the app and test**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

Verify:
1. Open Settings → Scraper (must be signed in with games imported)
2. The count text appears above "Start Scraping"
3. Toggle system pills — count updates
4. Switch filter between All/Unscraped/Favorites — count updates
5. Deselect all systems — shows "No games match this selection"

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: show dynamic game count on scraper dashboard"
```

---

### Task 3: Redesign progress page layout (Option F)

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:1122-1296` (replace Detail Card section)

- [ ] **Step 1: Replace the Detail Card section**

Replace lines 1122–1296 (from `// ── Detail Card ──` through the placeholder Text and its closing `}`) with the new Option F layout:

```qml
                // ── Detail Card (Option F — large cover + stacked metadata) ──
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.topMargin: 16

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        visible: root.scrapeTitle !== "" || root.scrapeStatus.indexOf("failed") >= 0

                        // Game title
                        Text {
                            text: (root.scrapeTitle || root.progressCurrentGame).toUpperCase()
                            color: SettingsTheme.text
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            font.letterSpacing: 0.3
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Cover + Metadata side by side
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 16

                            // Large cover (~46% width)
                            Rectangle {
                                Layout.preferredWidth: parent.width * 0.46
                                Layout.fillHeight: true
                                radius: SettingsTheme.cardRadius
                                color: SettingsTheme.card
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    source: root.scrapeCoverPath !== "" ? "file://" + root.scrapeCoverPath : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: false
                                    visible: root.scrapeCoverPath !== ""
                                }

                                // Placeholder when no cover yet
                                Text {
                                    anchors.centerIn: parent
                                    text: "COVER"
                                    color: SettingsTheme.textGhost
                                    font.pixelSize: 10
                                    visible: root.scrapeCoverPath === ""
                                }
                            }

                            // Stacked metadata
                            ColumnLayout {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                spacing: 10

                                // Rating
                                Row {
                                    spacing: 2
                                    visible: root.scrapeRating > 0
                                    Repeater {
                                        model: 5
                                        Text {
                                            text: {
                                                var filled = Math.floor(root.scrapeRating)
                                                var half = root.scrapeRating - filled >= 0.5
                                                if (index < filled) return "\u2605"
                                                if (index === filled && half) return "\u2605"
                                                return "\u2606"
                                            }
                                            color: index < Math.ceil(root.scrapeRating) ? SettingsTheme.accent : SettingsTheme.textDim
                                            font.pixelSize: 15
                                        }
                                    }
                                }

                                // Released
                                ColumnLayout {
                                    spacing: 1
                                    visible: root.scrapeReleaseDate !== ""
                                    Text { text: "RELEASED"; color: SettingsTheme.textFaint; font.pixelSize: 9; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapeReleaseDate; color: SettingsTheme.textMuted; font.pixelSize: 12 }
                                }

                                // Developer
                                ColumnLayout {
                                    spacing: 1
                                    visible: root.scrapeDeveloper !== ""
                                    Text { text: "DEVELOPER"; color: SettingsTheme.textFaint; font.pixelSize: 9; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapeDeveloper; color: SettingsTheme.textMuted; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                                }

                                // Publisher
                                ColumnLayout {
                                    spacing: 1
                                    visible: root.scrapePublisher !== ""
                                    Text { text: "PUBLISHER"; color: SettingsTheme.textFaint; font.pixelSize: 9; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapePublisher; color: SettingsTheme.textMuted; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                                }

                                // Genre
                                ColumnLayout {
                                    spacing: 1
                                    visible: root.scrapeGenres !== ""
                                    Text { text: "GENRE"; color: SettingsTheme.textFaint; font.pixelSize: 9; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapeGenres; color: SettingsTheme.textMuted; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                                }

                                // Players
                                ColumnLayout {
                                    spacing: 1
                                    visible: root.scrapePlayers !== ""
                                    Text { text: "PLAYERS"; color: SettingsTheme.textFaint; font.pixelSize: 9; font.letterSpacing: 0.5 }
                                    Text { text: root.scrapePlayers; color: SettingsTheme.textMuted; font.pixelSize: 12 }
                                }

                                // Status
                                Text {
                                    text: {
                                        if (root.scrapeStatus === "scraping" || root.scrapeStatus === "downloading")
                                            return "Downloading media..."
                                        return root.scrapeStatus
                                    }
                                    color: {
                                        if (root.scrapeStatus.indexOf("failed") >= 0) return SettingsTheme.error
                                        if (root.scrapeStatus.indexOf("not available") >= 0) return SettingsTheme.warning
                                        if (root.scrapeStatus === "downloading") return SettingsTheme.textMuted
                                        return SettingsTheme.success
                                    }
                                    font.pixelSize: 11
                                    visible: root.scrapeStatus !== "" && root.scrapeStatus !== "scraping"
                                    Layout.topMargin: 4
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }

                        // Divider before description
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: SettingsTheme.border
                            visible: root.scrapeDescription !== ""
                        }

                        // Description
                        Flickable {
                            id: descFlickable
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            contentWidth: width
                            contentHeight: descText.height
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            visible: root.scrapeDescription !== ""

                            Text {
                                id: descText
                                width: descFlickable.width
                                text: root.scrapeDescription
                                color: SettingsTheme.textDim
                                font.pixelSize: 11
                                lineHeight: 1.5
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // Placeholder when no data yet
                    Text {
                        anchors.centerIn: parent
                        text: root.scrapeRunning ? "Waiting for first result..." : ""
                        color: SettingsTheme.textDim
                        font.pixelSize: 14
                        visible: root.scrapeTitle === "" && root.scrapeStatus.indexOf("failed") < 0
                    }
                }
```

- [ ] **Step 2: Update header color to use SettingsTheme.success**

On line 1069, change the complete color from `"#22c55e"` to `SettingsTheme.success`:

```qml
                        color: root.scrapeRunning ? SettingsTheme.text : SettingsTheme.success
```

- [ ] **Step 3: Update progress bar color to use SettingsTheme.success**

On line 1108, change from `"#22c55e"` to `SettingsTheme.success`:

```qml
                            color: root.scrapeRunning ? SettingsTheme.accent : SettingsTheme.success
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```

Expected: Build succeeds.

- [ ] **Step 5: Run the app and test**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

Verify:
1. Start a scrape
2. Cover takes up roughly half the panel width
3. Metadata is stacked vertically on the right with small labels above values
4. Description appears below a divider
5. All fields populate as scraping proceeds

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: redesign scraper progress page with large cover layout"
```

---

### Task 4: Add completion summary to footer

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml` — root properties (~line 209) and footer section (~line 1298)

- [ ] **Step 1: Add summary properties to root**

Add after the existing `property int apiMaxRequests: 0` (line 228):

```qml
    // Completion summary
    property int scrapeSucceeded: 0
    property int scrapeFailed: 0
    property int scrapeSkipped: 0
```

- [ ] **Step 2: Update the onScrapeFinished handler**

Replace the existing handler at line 345:

```qml
        function onScrapeFinished(succeeded, failed, skipped) {
            root.scrapeRunning = false
            root.scrapeSucceeded = succeeded
            root.scrapeFailed = failed
            root.scrapeSkipped = skipped
        }
```

- [ ] **Step 3: Reset summary properties when starting a new scrape**

In the `activateFocused()` function, inside the `"start"` branch (around line 193), add after `root.apiMaxRequests = 0`:

```qml
                scrapeSucceeded = 0; scrapeFailed = 0; scrapeSkipped = 0
```

Also add the same line in the MouseArea onClicked handler for the Start Scraping button (around line 1036), after `root.apiMaxRequests = 0`:

```qml
                            root.scrapeSucceeded = 0
                            root.scrapeFailed = 0
                            root.scrapeSkipped = 0
```

- [ ] **Step 4: Replace the footer section**

Replace the footer (from `// ── Footer ──` at line 1298 through the closing `}` of the RowLayout at line 1361) with:

```qml
                // ── Footer ──────────────────────────────────────────────

                // Stats row (only visible when complete)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    visible: !root.scrapeRunning && root.progressTotal > 0

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: SettingsTheme.border
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        // Total
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 50

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "" + (root.scrapeSucceeded + root.scrapeFailed + root.scrapeSkipped)
                                    color: SettingsTheme.text
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "TOTAL"
                                    color: SettingsTheme.textFaint
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }

                            // Right border
                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: SettingsTheme.border
                            }
                        }

                        // Succeeded
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 50

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "" + root.scrapeSucceeded
                                    color: SettingsTheme.success
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "SUCCEEDED"
                                    color: SettingsTheme.success
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }

                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: SettingsTheme.border
                                visible: root.scrapeFailed > 0 || root.scrapeSkipped > 0
                            }
                        }

                        // Failed (only if > 0)
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 50
                            visible: root.scrapeFailed > 0

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "" + root.scrapeFailed
                                    color: SettingsTheme.error
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "FAILED"
                                    color: SettingsTheme.error
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }

                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: SettingsTheme.border
                                visible: root.scrapeSkipped > 0
                            }
                        }

                        // Skipped (only if > 0)
                        Item {
                            Layout.fillWidth: true
                            implicitHeight: 50
                            visible: root.scrapeSkipped > 0

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "" + root.scrapeSkipped
                                    color: SettingsTheme.textMuted
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "SKIPPED"
                                    color: SettingsTheme.textFaint
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }
                }

                // API + button row (always visible)
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: SettingsTheme.border
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 16
                    spacing: 12

                    // API quota
                    Text {
                        text: root.apiMaxRequests > 0
                            ? "API CALLS: " + root.apiRequestsToday + "/" + root.apiMaxRequests
                            : ""
                        color: SettingsTheme.textFaint
                        font.pixelSize: 11
                        font.letterSpacing: 0.5
                        visible: root.apiMaxRequests > 0
                    }

                    Item { Layout.fillWidth: true }

                    // Progress counter
                    Text {
                        text: root.progressTotal > 0 ? root.progressCurrent + " / " + root.progressTotal : ""
                        color: SettingsTheme.textMuted
                        font.pixelSize: 12
                        visible: root.progressTotal > 0
                    }

                    // Stop / Done button
                    Rectangle {
                        width: 120
                        height: 36
                        radius: 6
                        color: root.scrapeRunning ? SettingsTheme.errorDim : SettingsTheme.accentDim
                        border.width: 1
                        border.color: root.scrapeRunning ? SettingsTheme.error : SettingsTheme.accent

                        Text {
                            anchors.centerIn: parent
                            text: root.scrapeRunning ? "STOP" : "DONE"
                            color: root.scrapeRunning ? SettingsTheme.error : SettingsTheme.accent
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.5
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.scrapeRunning) {
                                    app.cancelScrape()
                                } else {
                                    root.screenState = "dashboard"
                                }
                            }
                        }
                    }
                }
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```

Expected: Build succeeds.

- [ ] **Step 6: Run the app and test**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

Verify:
1. Start a scrape — footer shows only the API/counter/STOP row (no stats row)
2. Let scrape complete — stats row appears above the API/DONE row
3. Stats show correct totals for succeeded, failed, skipped
4. Failed/Skipped cells only appear if their count > 0
5. DONE button returns to dashboard
6. Start another scrape — stats reset, stats row hides

- [ ] **Step 7: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: add completion summary stats to scraper footer"
```
