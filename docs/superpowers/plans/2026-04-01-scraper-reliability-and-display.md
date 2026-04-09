# Scraper Reliability & Display Fixes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix batch scraper so all DB writes are guaranteed before UI refresh, and each game shows only one complete progress update.

**Architecture:** Both fixes are in `ScraperService::startBatchScrape()`. Instead of firing individual async DB writes per game, accumulate results in a vector and apply them all in a single main-thread call that also emits `scrapeFinished`. Remove the mid-scrape `onMetadata` callback to reduce progress signals from 3 to 2 per game.

**Tech Stack:** C++17, Qt6 (QtConcurrent, QMetaObject::invokeMethod, signals/slots)

---

### Task 1: Rewrite `startBatchScrape()` — batch DB writes + remove onMetadata

**Files:**
- Modify: `cpp/src/services/scraper_service.cpp:88-208` (the `startBatchScrape` method)

- [ ] **Step 1: Replace the `startBatchScrape` method body**

Replace the entire `startBatchScrape` method (lines 88–208 of `scraper_service.cpp`) with this implementation. The two changes from the original are: (1) `pendingWrites` vector replaces per-game `QMetaObject::invokeMethod` calls, with a single batched call at the end that also emits `scrapeFinished`; (2) the `onMetadata` callback and its `scrapeProgress` emission are removed.

```cpp
void ScraperService::startBatchScrape(const ScrapeOptions& options) {
    m_cancelFlag.storeRelaxed(0);

    // Gather game list on the main thread (DB connection is thread-bound)
    QVector<GameRecord> allGames;
    for (const auto& system : options.systems) {
        auto games = m_db->gamesBySystem(system);
        allGames.append(games);
    }

    // Apply game filter on main thread
    QVector<GameRecord> toScrape;
    for (const auto& g : allGames) {
        switch (options.gameFilter) {
        case ScrapeOptions::UnscrapedOnly:
            if (g.cover_path.isEmpty() || !QFileInfo::exists(g.cover_path))
                toScrape.append(g);
            break;
        case ScrapeOptions::FavoritesOnly:
            if (g.favorite)
                toScrape.append(g);
            break;
        case ScrapeOptions::AllGames:
        default:
            toScrape.append(g);
            break;
        }
    }

    if (toScrape.isEmpty()) {
        emit scrapeFinished(0, 0, 0);
        return;
    }

    QtConcurrent::run([this, toScrape, options]() {
        int succeeded = 0, failed = 0, skipped = 0;
        QString mediaBaseDir = Paths::mediaDir();

        // Accumulate successful scrape results for batched DB write
        QVector<QPair<int, Scraper::ScrapeResult>> pendingWrites;

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
                pendingWrites.append({game.id, result});

                int count = result.mediaPaths.size();
                int requested = options.mediaTypes.size();
                if (count < requested) {
                    gameData["status"] = QString("%1 media (%2 not available)").arg(count).arg(requested - count);
                } else {
                    gameData["status"] = QString("%1 media downloaded").arg(count);
                }

                // Full metadata + media paths for final update
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

        // Bounce ALL DB writes + scrapeFinished to the main thread in one call.
        // This guarantees every write completes before gamesChanged() fires.
        QMetaObject::invokeMethod(this, [this, pendingWrites, succeeded, failed, skipped]() {
            for (const auto& pending : pendingWrites) {
                applyResultToDb(pending.first, pending.second);
            }
            emit scrapeFinished(succeeded, failed, skipped);
        }, Qt::QueuedConnection);
    });
}
```

- [ ] **Step 2: Build and verify it compiles**

Run:
```bash
cd /Users/mark/Documents/EmuFront-Project/cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```

Expected: Clean compile with no errors or warnings related to `scraper_service.cpp`.

- [ ] **Step 3: Manual test — batch scrape all PS2 games**

1. Launch the app: `./build/EmulatorFrontend`
2. Open Settings (Escape) → Scraper
3. Run batch scrape with "All Games" filter for PS2
4. Observe: each game should show "scraping" then one final status (no intermediate "downloading" flash)
5. After scrape completes, navigate to the game list
6. Verify: all 9 PS2 games (including Looney Tunes, Rayman Arena, Wacky Races) show metadata

- [ ] **Step 4: Manual test — cancel mid-scrape**

1. Start a batch scrape
2. Cancel after 2-3 games complete
3. Verify: games that completed before cancel have their metadata saved in the DB

Run:
```bash
sqlite3 "/Users/mark/Documents/EmuFront/config/emulator-frontend.db" "SELECT id, title, developer FROM games WHERE system='ps2' ORDER BY title;"
```

Expected: Games scraped before cancel have populated metadata fields.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/services/scraper_service.cpp
git commit -m "fix: batch DB writes in scraper to prevent lost metadata

Accumulate scrape results during batch scrape and apply all DB writes
in a single main-thread call before emitting scrapeFinished. This
guarantees all metadata is persisted before the UI refresh.

Also removes the mid-scrape onMetadata callback so each game emits
only 2 progress signals (starting + complete) instead of 3, preventing
confusing partial data flashing in the UI."
```
