# Scraper Reliability & Display Fixes

**Date:** 2026-04-01
**Status:** Approved

## Problem

Two issues with the batch scraper:

1. **Missing metadata for specific games:** The scraper finds all games and downloads all media, but 3 specific games consistently have blank metadata in the database. Media files exist on disk but the DB has no title, description, developer, publisher, or media paths. Root cause: the batch scrape fires individual async DB writes via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` from a background thread, and `scrapeFinished` is emitted from the background thread separately. Some writes are lost or not processed before the UI refresh triggered by `gamesChanged()`.

2. **Confusing piecemeal display during scraping:** Each game emits `scrapeProgress` 3 times (starting, metadata parsed, media complete). The UI updates on each signal, causing partial data to flash on screen before the full result arrives.

## Fix 1: Batch DB Writes with Guaranteed Ordering

### Current flow (broken)

```
Background thread:
  for each game:
    scrape → QMetaObject::invokeMethod(applyResultToDb, QueuedConnection)  // fire-and-forget
  emit scrapeFinished()  // from background thread

Main thread event queue:
  [applyResultToDb #1] [applyResultToDb #2] ... [scrapeFinished handler → gamesChanged()]
  // Some applyResultToDb calls may be lost or not yet processed
```

### New flow

```
Background thread:
  QVector<QPair<int, ScrapeResult>> pendingWrites;
  for each game:
    scrape → pendingWrites.append({gameId, result})
  QMetaObject::invokeMethod(this, [pendingWrites, counts]() {
    for each pending write:
      applyResultToDb(gameId, result)  // with qWarning on failure
    emit scrapeFinished(succeeded, failed, skipped)
  }, Qt::QueuedConnection);

Main thread event queue:
  [single call: all DB writes + scrapeFinished]  // atomic, guaranteed ordering
```

### Key properties

- All DB writes complete before `scrapeFinished` / `gamesChanged()` fires
- No individual writes can be dropped or reordered
- Zero impact on scrape speed (writes are still async, just batched)
- Cancel mid-scrape: all games completed before cancel are still saved
- `scrapeFinished` now emits on the main thread (cleaner than cross-thread)
- Failed DB writes logged with `qWarning`

### Files changed

- `cpp/src/services/scraper_service.cpp` — `startBatchScrape()` method only

## Fix 2: Single Progress Signal Per Game

### Current signals per game (3)

1. `scrapeProgress` with `status: "scraping"` — game is starting
2. `scrapeProgress` with `status: "downloading"` — metadata parsed, media downloading (via `onMetadata` callback)
3. `scrapeProgress` with final status — media complete

### New signals per game (2)

1. `scrapeProgress` with `status: "scraping"` — game is starting
2. `scrapeProgress` with final status — all metadata AND media complete

The `onMetadata` callback and its `scrapeProgress` emission are removed. The final signal already includes all metadata fields (`scrapedTitle`, `description`, `developer`, etc.) so no data is lost.

### Files changed

- `cpp/src/services/scraper_service.cpp` — remove `onMetadata` lambda and its signal emission in `startBatchScrape()`

## Scope

Both fixes are contained to a single file: `scraper_service.cpp`, specifically the `startBatchScrape()` method. No changes to:
- `Scraper` (API client) — the `MetadataCallback` parameter becomes unused in batch mode but remains available for single-game scraping
- `Database` — write logic unchanged
- `GameListModel` / `ThemeContext` — read-only, unaffected
- QML themes — consume the same `scrapeProgress` signal, just fewer emissions
- `AppController` — signal connections unchanged (the `scrapeFinished` connection lambda still works, it just receives the signal from the main thread now instead of cross-thread)
