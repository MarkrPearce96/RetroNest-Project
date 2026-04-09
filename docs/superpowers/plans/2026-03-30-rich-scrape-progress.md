# Rich Scrape Progress Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the basic scrape progress log with an ES-DE-style live detail view showing cover art, metadata, description, and API quota per game as it scrapes — plus fix the cancel button and save scraped game titles to the database.

**Architecture:** Enrich the `scrapeProgress` signal to carry a full `QVariantMap` of metadata (title, cover path, rating, developer, publisher, genre, players, description, API quota). The worker thread emits this map after each game scrapes. QML Screen 6 binds to these properties and renders a two-column detail card. Cancel is fixed by passing the atomic flag into `Scraper::scrapeGame()` so it checks between each media download.

**Tech Stack:** C++17, Qt6 (QML + Widgets), ScreenScraper API

---

### Task 1: Save scraped game title to database

The ScreenScraper API returns clean game names (e.g. "Crash Bandicoot" instead of "Crash Bandicoot (USA).bin") but `applyResultToDb()` never saves the title, and `updateGameMetadata()` SQL doesn't include it.

**Files:**
- Modify: `cpp/src/core/database.cpp:335-367` (updateGameMetadata SQL)
- Modify: `cpp/src/services/scraper_service.cpp:44-68` (applyResultToDb)

- [ ] **Step 1: Add title to updateGameMetadata SQL**

In `cpp/src/core/database.cpp`, update the `updateGameMetadata` method to include `title` in the UPDATE statement:

```cpp
bool Database::updateGameMetadata(int id, const GameRecord& metadata) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET title=?, description=?, developer=?, publisher=?, release_date=?, "
              "genres=?, rating=?, players=?, cover_path=?, "
              "screenshot_path=?, titlescreen_path=?, marquee_path=?, fanart_path=?, "
              "box3d_path=?, backcover_path=?, miximage_path=?, physicalmedia_path=?, "
              "manual_path=?, video_path=? WHERE id=?");
    q.addBindValue(metadata.title);
    q.addBindValue(metadata.description);
    q.addBindValue(metadata.developer);
    q.addBindValue(metadata.publisher);
    q.addBindValue(metadata.release_date);
    q.addBindValue(metadata.genres);
    q.addBindValue(metadata.rating);
    q.addBindValue(metadata.players);
    q.addBindValue(metadata.cover_path);
    q.addBindValue(metadata.screenshot_path);
    q.addBindValue(metadata.titlescreen_path);
    q.addBindValue(metadata.marquee_path);
    q.addBindValue(metadata.fanart_path);
    q.addBindValue(metadata.box3d_path);
    q.addBindValue(metadata.backcover_path);
    q.addBindValue(metadata.miximage_path);
    q.addBindValue(metadata.physicalmedia_path);
    q.addBindValue(metadata.manual_path);
    q.addBindValue(metadata.video_path);
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "[Database] updateGameMetadata failed:" << q.lastError().text();
        return false;
    }
    return true;
}
```

- [ ] **Step 2: Set title in applyResultToDb**

In `cpp/src/services/scraper_service.cpp`, add the title assignment in `applyResultToDb`:

```cpp
void ScraperService::applyResultToDb(int gameId, const Scraper::ScrapeResult& result) {
    GameRecord metadata;
    metadata.title        = result.title;  // <-- ADD THIS LINE
    metadata.description  = result.description;
    // ... rest unchanged ...
```

Only add the single line `metadata.title = result.title;` before `metadata.description`. Do not touch other lines.

- [ ] **Step 3: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/database.cpp cpp/src/services/scraper_service.cpp
git commit -m "fix: save scraped game title from ScreenScraper to database

updateGameMetadata now includes title in the UPDATE SQL, and
applyResultToDb maps result.title to the GameRecord.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 2: Fix cancel button — pass cancel flag into Scraper

The cancel flag is only checked between games, but `Scraper::scrapeGame()` downloads multiple media files synchronously. A single game with 11 media types can block for 30+ seconds. Fix: pass the `QAtomicInt*` cancel flag into `scrapeGame()` so it checks between each media download.

**Files:**
- Modify: `cpp/src/core/scraper.h:35-36` (scrapeGame signature)
- Modify: `cpp/src/core/scraper.cpp:103-313` (scrapeGame implementation — add cancel checks)
- Modify: `cpp/src/services/scraper_service.cpp:77,134` (pass cancel flag)

- [ ] **Step 1: Add cancelFlag parameter to scrapeGame**

In `cpp/src/core/scraper.h`, change the `scrapeGame` signature:

```cpp
    ScrapeResult scrapeGame(const GameRecord& game,
                            const QString& mediaBaseDir,
                            const QStringList& mediaTypes,
                            QAtomicInt* cancelFlag = nullptr);
```

Add `#include <QAtomicInt>` at the top of scraper.h if not already present.

- [ ] **Step 2: Add cancel checks in scrapeGame implementation**

In `cpp/src/core/scraper.cpp`, update the signature to match, then add cancel checks at two points inside `scrapeGame()`:

**A) After metadata extraction, before the media download loop (after line 225):**

```cpp
    // Check cancel before starting media downloads
    if (cancelFlag && cancelFlag->loadRelaxed()) {
        result.success = !result.title.isEmpty();
        return result;
    }
```

**B) Inside the media download loop, at the start of each iteration (inside `for (const QString& mediaType : mediaTypes)` at line 238):**

```cpp
    for (const QString& mediaType : mediaTypes) {
        // Check cancel between each media download
        if (cancelFlag && cancelFlag->loadRelaxed())
            break;

        QStringList apiTypes = screenScraperMediaTypes(mediaType);
        // ... rest unchanged ...
```

- [ ] **Step 3: Pass cancel flag from ScraperService**

In `cpp/src/services/scraper_service.cpp`, update both call sites:

**A) In `scrapeGame()` (single-game scrape, line 77):**
```cpp
    auto result = m_scraper->scrapeGame(target, Paths::mediaDir(), Scraper::allMediaTypes(), &m_cancelFlag);
```

**B) In `startBatchScrape()` lambda (line 134):**
```cpp
            auto result = m_scraper->scrapeGame(game, mediaBaseDir, options.mediaTypes, &m_cancelFlag);
```

- [ ] **Step 4: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/scraper.h cpp/src/core/scraper.cpp cpp/src/services/scraper_service.cpp
git commit -m "fix: cancel button now interrupts mid-game media downloads

Pass the atomic cancel flag into Scraper::scrapeGame() so it checks
between each media file download, not just between games.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 3: Extract API quota from ScreenScraper responses

The ScreenScraper API returns user quota info in `response.ssuser` on every `jeuInfos` call. Extract `requeststoday` and `maxrequestsperday` and include them in `ScrapeResult`.

**Files:**
- Modify: `cpp/src/core/scraper.h:19-33` (add quota fields to ScrapeResult)
- Modify: `cpp/src/core/scraper.cpp:145-152` (extract quota from response)

- [ ] **Step 1: Add quota fields to ScrapeResult**

In `cpp/src/core/scraper.h`, add two fields to `ScrapeResult`:

```cpp
    struct ScrapeResult {
        bool success = false;
        QString error;

        QString title;
        QString description;
        QString developer;
        QString publisher;
        QString release_date;
        QString genres;
        double  rating = 0.0;
        QString players;

        QMap<QString, QString> mediaPaths;

        // API quota (from ssuser in response)
        int requestsToday = 0;
        int maxRequestsPerDay = 0;
    };
```

- [ ] **Step 2: Extract quota from API response**

In `cpp/src/core/scraper.cpp`, after the line `QJsonObject jeu = response["jeu"].toObject();` (line 147), add:

```cpp
    // Extract API quota from ssuser
    QJsonObject ssuser = response["ssuser"].toObject();
    if (!ssuser.isEmpty()) {
        result.requestsToday = ssuser["requeststoday"].toString().toInt();
        result.maxRequestsPerDay = ssuser["maxrequestsperday"].toString().toInt();
    }
```

Note: ScreenScraper returns these as string values in JSON, hence `.toString().toInt()`.

- [ ] **Step 3: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/scraper.h cpp/src/core/scraper.cpp
git commit -m "feat: extract API quota from ScreenScraper responses

ScrapeResult now includes requestsToday and maxRequestsPerDay
extracted from the ssuser object in the API response.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 4: Enrich scrapeProgress signal with full metadata

Replace the simple `(int, int, QString, QString)` signal with a richer one that carries a `QVariantMap` containing all scraped metadata, cover path, and API quota. This gives QML everything it needs for the ES-DE-style display.

**Files:**
- Modify: `cpp/src/services/scraper_service.h:56` (change signal signature)
- Modify: `cpp/src/services/scraper_service.cpp:121-163` (emit rich data)
- Modify: `cpp/src/ui/app_controller.h:124` (change forwarded signal)
- Modify: `cpp/src/ui/app_controller.cpp:33-34` (update connect)

- [ ] **Step 1: Change ScraperService signal**

In `cpp/src/services/scraper_service.h`, replace the `scrapeProgress` signal:

```cpp
signals:
    void statusMessage(const QString& msg);
    void scrapeProgress(int current, int total, const QVariantMap& gameData);
    void scrapeFinished(int succeeded, int failed, int skipped);
    void credentialsValidated(bool success, const QString& message);
```

Add `#include <QVariantMap>` at the top of the header.

- [ ] **Step 2: Change AppController signal and connect**

In `cpp/src/ui/app_controller.h`, update the signal:

```cpp
    void scrapeProgress(int current, int total, const QVariantMap& gameData);
```

In `cpp/src/ui/app_controller.cpp`, the existing connect at line 33-34 already forwards by matching signatures, so it should work unchanged:

```cpp
    connect(&m_scraperService, &ScraperService::scrapeProgress,
            this, &AppController::scrapeProgress);
```

Verify this still compiles — both signals now have the same `(int, int, QVariantMap)` signature.

- [ ] **Step 3: Build the QVariantMap in startBatchScrape**

In `cpp/src/services/scraper_service.cpp`, replace the entire lambda body inside `QtConcurrent::run` with:

```cpp
    QtConcurrent::run([this, toScrape, options]() {
        int succeeded = 0, failed = 0, skipped = 0;
        QString mediaBaseDir = Paths::mediaDir();

        for (int i = 0; i < toScrape.size(); i++) {
            if (m_cancelFlag.loadRelaxed()) {
                skipped = toScrape.size() - i;
                break;
            }

            const auto& game = toScrape[i];

            // Emit "scraping" state with game name
            QVariantMap startData;
            startData["gameName"] = game.title;
            startData["status"] = "scraping";
            emit scrapeProgress(i + 1, toScrape.size(), startData);

            auto result = m_scraper->scrapeGame(game, mediaBaseDir, options.mediaTypes, &m_cancelFlag);

            QVariantMap gameData;
            gameData["gameName"] = game.title;

            if (result.success) {
                // Bounce DB write back to the main thread
                QMetaObject::invokeMethod(this, [this, id = game.id, result]() {
                    applyResultToDb(id, result);
                }, Qt::QueuedConnection);

                int count = result.mediaPaths.size();
                int requested = options.mediaTypes.size();
                if (count < requested) {
                    gameData["status"] = QString("%1 media (%2 not available)").arg(count).arg(requested - count);
                } else {
                    gameData["status"] = QString("%1 media downloaded").arg(count);
                }

                // Metadata for live display
                gameData["scrapedTitle"] = result.title;
                gameData["description"] = result.description;
                gameData["developer"] = result.developer;
                gameData["publisher"] = result.publisher;
                gameData["releaseDate"] = result.release_date;
                gameData["genres"] = result.genres;
                gameData["rating"] = result.rating;
                gameData["players"] = result.players;
                gameData["coverPath"] = result.mediaPaths.value("covers");
                gameData["screenshotPath"] = result.mediaPaths.value("screenshots");

                // API quota
                gameData["requestsToday"] = result.requestsToday;
                gameData["maxRequests"] = result.maxRequestsPerDay;

                succeeded++;
            } else {
                gameData["status"] = "failed: " + result.error;
                failed++;
            }

            emit scrapeProgress(i + 1, toScrape.size(), gameData);

            // Rate limiting
            if (i < toScrape.size() - 1 && !m_cancelFlag.loadRelaxed())
                QThread::msleep(1200);
        }

        emit scrapeFinished(succeeded, failed, skipped);
    });
```

- [ ] **Step 4: Build and verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds. There may be QML warnings at runtime until Task 5 updates the QML handler.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/scraper_service.h cpp/src/services/scraper_service.cpp \
       cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "feat: enrich scrapeProgress signal with full metadata map

Signal now carries a QVariantMap with scraped title, description,
developer, publisher, genre, rating, players, cover path, and
API quota — everything QML needs for the ES-DE-style progress view.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 5: Redesign QML progress page (ES-DE style)

Replace Screen 6 in ScraperSettings.qml with a rich detail card layout: header with system/progress counter, two-column body (cover + metadata left, description right), API quota at bottom, and cancel/done button.

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml:17-23,66-96,675-772` (properties, signal handler, Screen 6)

- [ ] **Step 1: Update properties and signal handler**

In `cpp/qml/AppUI/ScraperSettings.qml`, replace the progress tracking properties (lines 17-23) with:

```qml
    // Progress tracking
    property int progressCurrent: 0
    property int progressTotal: 0
    property string progressCurrentGame: ""
    property bool scrapeRunning: false

    // Live game detail (from rich signal)
    property string scrapeTitle: ""
    property string scrapeDescription: ""
    property string scrapeDeveloper: ""
    property string scrapePublisher: ""
    property string scrapeReleaseDate: ""
    property string scrapeGenres: ""
    property real scrapeRating: 0.0
    property string scrapePlayers: ""
    property string scrapeCoverPath: ""
    property string scrapeScreenshotPath: ""
    property string scrapeStatus: ""
    property int apiRequestsToday: 0
    property int apiMaxRequests: 0

    // System info for header
    property string scrapeSystemLabel: ""
    property int scrapeSystemGameCount: 0
```

Replace the `ListModel { id: progressLog }` line to keep it (it's still used for the log).

- [ ] **Step 2: Update the signal handler**

Replace the `onScrapeProgress` handler in the Connections block (lines 81-91) with:

```qml
        function onScrapeProgress(current, total, gameData) {
            root.progressCurrent = current
            root.progressTotal = total
            root.progressCurrentGame = gameData.gameName || ""
            root.scrapeStatus = gameData.status || ""

            // Only update detail fields on completion signals (not "scraping" start signals)
            if (gameData.status !== "scraping") {
                root.scrapeTitle = gameData.scrapedTitle || ""
                root.scrapeDescription = gameData.description || ""
                root.scrapeDeveloper = gameData.developer || ""
                root.scrapePublisher = gameData.publisher || ""
                root.scrapeReleaseDate = gameData.releaseDate || ""
                root.scrapeGenres = gameData.genres || ""
                root.scrapeRating = gameData.rating || 0.0
                root.scrapePlayers = gameData.players || ""
                root.scrapeCoverPath = gameData.coverPath || ""
                root.scrapeScreenshotPath = gameData.screenshotPath || ""

                if (gameData.requestsToday !== undefined) {
                    root.apiRequestsToday = gameData.requestsToday
                    root.apiMaxRequests = gameData.maxRequests
                }

                // Append to log
                var status = gameData.status || ""
                progressLog.append({
                    "gameName": gameData.gameName || "",
                    "status": status,
                    "isSuccess": status.indexOf("media downloaded") >= 0 || status.indexOf("media (") >= 0,
                    "isPartial": status.indexOf("not available") >= 0,
                    "isFailed": status.indexOf("failed") >= 0
                })
            }
        }
```

- [ ] **Step 3: Update the Start Scraping button to capture system info**

In Screen 5 (Start Scraping), update the onClicked handler of the start button (around line 661) to capture system label info before navigating:

```qml
                    onClicked: {
                        progressLog.clear()
                        root.progressCurrent = 0
                        root.progressTotal = 0
                        root.scrapeRunning = true
                        root.scrapeTitle = ""
                        root.scrapeDescription = ""
                        root.scrapeCoverPath = ""
                        root.scrapeScreenshotPath = ""
                        root.scrapeStatus = ""
                        root.apiRequestsToday = 0
                        root.apiMaxRequests = 0

                        // Build system label for header
                        var systemNames = root.selectedSystems.join(", ").toUpperCase()
                        root.scrapeSystemLabel = systemNames
                        root.scrapeSystemGameCount = root.selectedSystems.length

                        root.screenState = "progress"
                        app.startBatchScrape(root.selectedMedia, root.selectedSystems, root.gameFilter)
                    }
```

- [ ] **Step 4: Replace Screen 6 (Progress) with ES-DE-style layout**

Replace the entire `// -- Screen 6: Progress --` block (lines 675-772) with:

```qml
        // ── Screen 6: Progress ──
        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // ── Header ──
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 24
                    spacing: 4

                    Text {
                        text: root.scrapeRunning ? "SCRAPING IN PROGRESS" : "SCRAPING COMPLETE"
                        color: Theme.textPrimary
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        font.letterSpacing: 1
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: root.scrapeSystemLabel + " [" + root.selectedSystems.length + (root.selectedSystems.length === 1 ? " SYSTEM]" : " SYSTEMS]")
                        color: Theme.textMuted
                        font.pixelSize: 13
                        font.letterSpacing: 0.5
                        Layout.alignment: Qt.AlignHCenter
                        visible: root.scrapeSystemLabel !== ""
                    }

                    Text {
                        text: root.progressTotal > 0
                            ? "GAME " + root.progressCurrent + " OF " + root.progressTotal + " - " + root.progressCurrentGame
                            : ""
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                        visible: root.progressTotal > 0
                    }

                    // Progress bar
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 24; Layout.rightMargin: 24
                        Layout.topMargin: 8
                        height: 4; radius: 2
                        color: Theme.surface

                        Rectangle {
                            width: root.progressTotal > 0 ? parent.width * (root.progressCurrent / root.progressTotal) : 0
                            height: parent.height; radius: 2; color: Theme.accent
                            Behavior on width { NumberAnimation { duration: 200 } }
                        }
                    }
                }

                // ── Divider ──
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    height: 1; color: Theme.divider
                }

                // ── Detail Card ──
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 24

                    // Show detail when we have scraped data
                    visible: root.scrapeTitle !== "" || root.scrapeStatus.indexOf("failed") >= 0

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        // Game title
                        Text {
                            text: (root.scrapeTitle || root.progressCurrentGame).toUpperCase()
                            color: Theme.textPrimary
                            font.pixelSize: 16
                            font.weight: Font.Bold
                            font.letterSpacing: 0.5
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Two-column body
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 16

                            // ── Left column: cover + metadata ──
                            ColumnLayout {
                                Layout.fillHeight: true
                                Layout.preferredWidth: parent.width * 0.55
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    // Cover art
                                    Rectangle {
                                        Layout.preferredWidth: 120
                                        Layout.preferredHeight: 160
                                        radius: 6
                                        color: Theme.surface
                                        visible: root.scrapeCoverPath !== ""

                                        Image {
                                            anchors.fill: parent
                                            anchors.margins: 2
                                            source: root.scrapeCoverPath !== "" ? "file://" + root.scrapeCoverPath : ""
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true
                                            cache: false
                                        }
                                    }

                                    // Metadata grid
                                    GridLayout {
                                        columns: 2
                                        columnSpacing: 10
                                        rowSpacing: 4
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignTop

                                        // Rating
                                        Text {
                                            text: "RATING:"
                                            color: Theme.textMuted; font.pixelSize: 11
                                            visible: root.scrapeRating > 0
                                        }
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
                                                    color: index < Math.ceil(root.scrapeRating) ? "#f5c518" : Theme.textDim
                                                    font.pixelSize: 14
                                                }
                                            }
                                        }

                                        // Released
                                        Text { text: "RELEASED:"; color: Theme.textMuted; font.pixelSize: 11; visible: root.scrapeReleaseDate !== "" }
                                        Text { text: root.scrapeReleaseDate; color: Theme.textSecondary; font.pixelSize: 12; visible: root.scrapeReleaseDate !== "" }

                                        // Developer
                                        Text { text: "DEVELOPER:"; color: Theme.textMuted; font.pixelSize: 11; visible: root.scrapeDeveloper !== "" }
                                        Text { text: root.scrapeDeveloper; color: Theme.textSecondary; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true; visible: root.scrapeDeveloper !== "" }

                                        // Publisher
                                        Text { text: "PUBLISHER:"; color: Theme.textMuted; font.pixelSize: 11; visible: root.scrapePublisher !== "" }
                                        Text { text: root.scrapePublisher; color: Theme.textSecondary; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true; visible: root.scrapePublisher !== "" }

                                        // Genre
                                        Text { text: "GENRE:"; color: Theme.textMuted; font.pixelSize: 11; visible: root.scrapeGenres !== "" }
                                        Text { text: root.scrapeGenres; color: Theme.textSecondary; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true; visible: root.scrapeGenres !== "" }

                                        // Players
                                        Text { text: "PLAYERS:"; color: Theme.textMuted; font.pixelSize: 11; visible: root.scrapePlayers !== "" }
                                        Text { text: root.scrapePlayers; color: Theme.textSecondary; font.pixelSize: 12; visible: root.scrapePlayers !== "" }
                                    }
                                }

                                // Status indicator
                                Text {
                                    text: {
                                        if (root.scrapeStatus === "scraping") return "Downloading..."
                                        if (root.scrapeStatus.indexOf("failed") >= 0) return root.scrapeStatus
                                        return root.scrapeStatus
                                    }
                                    color: {
                                        if (root.scrapeStatus.indexOf("failed") >= 0) return "#ef4444"
                                        if (root.scrapeStatus.indexOf("not available") >= 0) return "#eab308"
                                        return "#22c55e"
                                    }
                                    font.pixelSize: 11
                                    visible: root.scrapeStatus !== "" && root.scrapeStatus !== "scraping"
                                    Layout.topMargin: 4
                                }

                                Item { Layout.fillHeight: true }
                            }

                            // ── Vertical divider ──
                            Rectangle {
                                Layout.fillHeight: true
                                width: 1; color: Theme.divider
                                visible: root.scrapeDescription !== ""
                            }

                            // ── Right column: description ──
                            Flickable {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                contentHeight: descText.height
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                visible: root.scrapeDescription !== ""

                                Text {
                                    id: descText
                                    width: parent.width
                                    text: root.scrapeDescription
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                    lineHeight: 1.4
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    // Placeholder when no data yet
                    Text {
                        anchors.centerIn: parent
                        text: root.scrapeRunning ? "Waiting for first result..." : ""
                        color: Theme.textDim
                        font.pixelSize: 14
                        visible: root.scrapeTitle === "" && root.scrapeStatus.indexOf("failed") < 0
                    }
                }

                // ── Footer ──
                Rectangle {
                    Layout.fillWidth: true
                    height: 1; color: Theme.divider
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
                        color: Theme.textDim
                        font.pixelSize: 11
                        font.letterSpacing: 0.5
                        visible: root.apiMaxRequests > 0
                    }

                    Item { Layout.fillWidth: true }

                    // Progress counter
                    Text {
                        text: root.progressTotal > 0 ? root.progressCurrent + " / " + root.progressTotal : ""
                        color: Theme.textMuted
                        font.pixelSize: 12
                        visible: root.progressTotal > 0
                    }

                    // Cancel / Done button
                    Button {
                        implicitWidth: 120; implicitHeight: 36
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? Theme.surfaceHover : Theme.surface
                            border.width: 1
                            border.color: root.scrapeRunning ? "#ef4444" : Theme.accent
                        }
                        contentItem: Text {
                            text: root.scrapeRunning ? "STOP" : "DONE"
                            color: root.scrapeRunning ? "#ef4444" : Theme.textPrimary
                            font.pixelSize: 13; font.weight: Font.DemiBold
                            font.letterSpacing: 0.5
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (root.scrapeRunning) {
                                app.cancelScrape()
                            } else {
                                root.screenState = "hub"
                            }
                        }
                    }
                }
            }
        }
```

- [ ] **Step 5: Build and test visually**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

Then run the app and test scraping a few games to verify:
- Header shows system name and game counter
- Cover art appears after each game scrapes
- Metadata fields populate (rating stars, released, developer, publisher, genre, players)
- Description shows on the right
- API quota shows at bottom left
- STOP button cancels mid-scrape (even during media downloads)
- After completion, button changes to DONE and returns to hub

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml
git commit -m "feat: ES-DE-style rich scrape progress page

Replace the simple progress log with a live detail card showing
cover art, metadata (rating, released, developer, publisher, genre,
players), description, and API quota counter. The view updates in
real-time as each game is scraped.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 6: Final integration test and edge cases

Verify all pieces work together end-to-end and handle edge cases.

**Files:**
- Possibly modify: `cpp/qml/AppUI/ScraperSettings.qml` (minor fixes from testing)

- [ ] **Step 1: Test with multiple systems selected**

Run the app, select multiple systems, start scraping. Verify the header shows the combined system names and the counter increments across all systems.

- [ ] **Step 2: Test cancel mid-download**

Start a scrape with all 11 media types. While a game is downloading media, click STOP. Verify it stops within a few seconds (not after the entire game finishes). Verify the scrapeFinished signal fires with the correct skipped count.

- [ ] **Step 3: Test game title update**

After scraping, go to the game list and verify that game titles have been updated from filenames (e.g., "Crash Bandicoot (USA)") to clean ScreenScraper names (e.g., "Crash Bandicoot").

- [ ] **Step 4: Test with no media selected (metadata only)**

Deselect all media types except none — actually, you must have at least one. Select only "covers" and verify that only the cover shows in the progress view, and other media fields show as "not available" where appropriate.

- [ ] **Step 5: Test unscraped-only filter**

Scrape once, then scrape again with "Unscraped Only" filter. Verify already-scraped games are skipped.

- [ ] **Step 6: Final commit if any fixes needed**

If any adjustments were made during testing:

```bash
git add -u
git commit -m "fix: address edge cases in rich scrape progress

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```
