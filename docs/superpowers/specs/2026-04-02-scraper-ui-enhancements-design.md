# Scraper UI Enhancements — Dynamic Game Count & Progress Redesign

## Overview

Two enhancements to the scraper settings page:

1. **Dynamic game count** on the dashboard — a summary line above the "Start Scraping" button showing how many games will be scraped based on the current system + filter selection.
2. **Redesigned progress page** — larger cover art with metadata beside it, description below, and an expanded footer summary when scraping completes.

## 1. Dashboard — Dynamic Game Count

### Behavior

A text line appears just above the "Start Scraping" button showing the count of games matching the current selection. It updates dynamically whenever `selectedSystems` or `gameFilter` changes.

- Format: `"24 games will be scraped"` (muted text, centered)
- When count is 0: `"No games match this selection"`
- The Start Scraping button remains disabled when count is 0 (existing behavior — systems or media not selected)

### Backend

New method on `AppController`:

```cpp
Q_INVOKABLE int scrapeGameCount(const QStringList& systems, const QString& gameFilter);
```

This queries the database using the same logic as `ScraperService::startBatchScrape`:
- Fetches games via `Database::gamesBySystem()` for each selected system
- Applies the filter:
  - `"all"` — count all games
  - `"unscraped"` — count games where `cover_path` is empty or file doesn't exist
  - `"favorites"` — count games where `favorite == 1`
- Returns the total count

### QML Changes

In `ScraperSettings.qml` dashboard state:
- Add a `Text` element above the Start Scraping button
- Bind it to call `app.scrapeGameCount(selectedSystems, gameFilter)` reactively
- Use `SettingsTheme.textMuted` color, `font.pixelSize: 13`

## 2. Progress Page — Redesigned Layout (Option F)

### Layout Structure (top to bottom)

**Header** (centered):
- "SCRAPING IN PROGRESS" — bold, letter-spaced
- System label: e.g. "PS2, PSX [2 SYSTEMS]"
- Current game: "GAME 1 OF 18 — Game Name"
- Progress bar in accent color

**Divider line**

**Game Title** — bold, uppercase, full width

**Cover + Metadata** (side by side):
- Left: Cover art taking ~46% of the panel width, rounded corners (`cardRadius`)
- Right: Metadata stacked vertically with small labels above values:
  - Rating (star icons)
  - Released
  - Developer
  - Publisher
  - Genre
  - Players
  - Status message (e.g. "11 media downloaded") in success color

**Divider line**

**Description** — `textDim` color, wrapping text

**Footer**:
- API calls count on the left
- Progress counter + STOP button on the right

### What Changes From Current Layout

- Removed: Three-column layout (cover | metadata grid | description side-by-side), vertical divider, screenshot area
- Added: Much larger cover (~46% width instead of ~140px fixed), metadata stacked vertically with label-above-value style, description in its own section below

### No Backend Changes Needed

All data is already provided by the existing `scrapeProgress` signal with `gameData` map.

## 3. Scraping Complete — Footer Summary

### Behavior

When `scrapeFinished` fires:
- Header changes to "SCRAPING COMPLETE" in success color
- Progress bar fills to 100% and turns success color
- Last scraped game's detail (cover, metadata, description) stays visible
- Footer expands to show a stats row above the existing API/button row

### Footer Stats Row

Four cells separated by vertical dividers:

| TOTAL | SUCCEEDED | FAILED | SKIPPED |
|-------|-----------|--------|---------|
| 18    | 15 (green)| 2 (red)| 1 (muted)|

Each cell: large number on top, small uppercase label below. Color-coded:
- Total: `text` color
- Succeeded: `success` color
- Failed: `error` color (only shown if > 0)
- Skipped: `textMuted` color (only shown if > 0)

Below the stats row: API calls count + "18 / 18" counter + DONE button (accent-colored).

### Backend

No new backend work. The existing `scrapeFinished(succeeded, failed, skipped)` signal provides all three values. Total is computed as `succeeded + failed + skipped`.

### QML Changes

New properties on root:
- `scrapeSucceeded`, `scrapeFailed`, `scrapeSkipped` (int) — set from `onScrapeFinished`

Footer becomes conditional:
- During scraping: current single-row footer (API + counter + STOP)
- After completion: two-row footer (stats row + API/counter/DONE row)

## Visual References

Mockups saved in `.superpowers/brainstorm/55808-1775096292/content/`:
- `progress-designs-v5.html` — Option F layout (selected)
- `scraping-complete-v2.html` — Footer summary on completion

## Theme Colors Reference

All UI uses existing `SettingsTheme` singleton values — no new colors needed.
