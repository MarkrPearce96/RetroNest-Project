# Scraper Redesign — ES-DE Style Media & Settings

## Overview

Replace the current single-cover scraper with a full ES-DE-style media scraping system. This includes a new `downloaded_media/` folder structure supporting 11 media types, a multi-page scraper settings UI with credential validation, content/system selection, game filtering, and a dedicated progress modal.

The existing right-click "Scrape" on a single game is preserved and downloads all media types. The new settings page becomes the primary way to configure and run batch scrapes.

## Media Folder Structure

Replace `covers/` with `downloaded_media/`:

```
downloaded_media/
  {system}/           # e.g. psx, ps2, gba
    3dboxes/
    backcovers/
    covers/
    fanart/
    manuals/
    marquees/
    miximages/
    physicalmedia/
    screenshots/
    titlescreens/
    videos/
```

Files are named `{game_title}.{ext}` (same as current cover naming).

### Media Type to ScreenScraper API Mapping

| Folder          | ScreenScraper `type` value(s)         | File types  |
|-----------------|---------------------------------------|-------------|
| 3dboxes         | `box-3D`                              | png         |
| backcovers      | `box-2D-back`                         | png, jpg    |
| covers          | `box-2D`                              | png, jpg    |
| fanart          | `fanart`                              | png, jpg    |
| manuals         | `manuel`                              | pdf         |
| marquees        | `screenmarquee`                       | png         |
| miximages       | `mixrbv1`, `mixrbv2`                  | png         |
| physicalmedia   | `support-2D`                          | png         |
| screenshots     | `ss`                                  | png, jpg    |
| titlescreens    | `sstitle`                             | png, jpg    |
| videos          | `video`, `video-normalized`           | mp4         |

Region priority for all media: `us` > `wor` > `eu` > `jp` > any.

## Paths Changes

### Remove
- `Paths::coversDir()` — replaced by `mediaDir()`

### Add
- `Paths::mediaDir()` — returns `{root}/downloaded_media`
- `Paths::mediaDir(system)` — returns `{root}/downloaded_media/{system}`
- `Paths::mediaDir(system, mediaType)` — returns `{root}/downloaded_media/{system}/{mediaType}`

### Update
- `Paths::ensureDirectories()` — create `downloaded_media/` instead of `covers/`
- Media subdirectories are created on-demand during scraping (per-system, per-type)

## Database Changes

### New columns on `games` table

| Column              | Type    | Description                    |
|---------------------|---------|--------------------------------|
| `screenshot_path`   | TEXT    | Path to screenshot image       |
| `titlescreen_path`  | TEXT    | Path to title screen image     |
| `marquee_path`      | TEXT    | Path to marquee image          |
| `fanart_path`       | TEXT    | Path to fan art image          |
| `box3d_path`        | TEXT    | Path to 3D box image           |
| `backcover_path`    | TEXT    | Path to back cover image       |
| `miximage_path`     | TEXT    | Path to mix image              |
| `physicalmedia_path`| TEXT    | Path to physical media image   |
| `manual_path`       | TEXT    | Path to manual PDF             |
| `video_path`        | TEXT    | Path to video file             |

`cover_path` stays as-is for backward compatibility.

### Migration
- Add columns via `ALTER TABLE` in `runMigrations()` (new schema version)
- Existing `cover_path` values remain valid — no data migration needed
- Future scrapes will populate `cover_path` using the new `downloaded_media/{system}/covers/` path

### GameListModel new roles
Add roles for each new media path so themes can access them: `screenshotPath`, `titlescreenPath`, `marqueePath`, `fanartPath`, `box3dPath`, `backcoverPath`, `miximagePath`, `physicalmediaPath`, `manualPath`, `videoPath`.

## Scraper Core Changes (`scraper.h` / `scraper.cpp`)

### ScrapeResult struct — expanded

```cpp
struct ScrapeResult {
    bool success = false;
    QString error;

    // Metadata (unchanged)
    QString title;
    QString description;
    QString developer;
    QString publisher;
    QString release_date;
    QString genres;
    double  rating = 0.0;
    QString players;

    // Media paths (new)
    QMap<QString, QString> mediaPaths;  // mediaType -> local file path
};
```

### New method signature

```cpp
ScrapeResult scrapeGame(const GameRecord& game,
                        const QString& mediaBaseDir,
                        const QStringList& mediaTypes);
```

- `mediaBaseDir` = `Paths::mediaDir()` (the `downloaded_media/` root)
- `mediaTypes` = list of requested types (e.g. `{"covers", "screenshots", "videos"}`)
- For each requested type, find the matching media in the API response, download to `{mediaBaseDir}/{system}/{mediaType}/{title}.{ext}`
- Populate `mediaPaths` map with successful downloads

### Credential validation method

```cpp
bool validateCredentials(const QString& userId, const QString& userPassword);
```

Hits ScreenScraper's `ssuserInfos.php` endpoint. Returns true if the API responds with valid user data.

## ScraperService Changes

### New methods

```cpp
// Validate credentials against ScreenScraper API
bool validateAndSaveCredentials(const QString& ssId, const QString& ssPassword);

// Sign out (clear stored credentials)
void signOut();

// Batch scrape with options
struct ScrapeOptions {
    QStringList mediaTypes;    // which media to download
    QStringList systems;       // which systems to scrape
    enum Filter { AllGames, UnscrapedOnly, FavoritesOnly };
    Filter gameFilter = AllGames;
};
QString scrapeBatch(const ScrapeOptions& options);
```

### Updated `scrapeGame(int gameId)`
Downloads all 11 media types (used by right-click single-game scrape).

### Updated `scrapeAll()`
Replaced by `scrapeBatch()` which accepts filtering options.

### Signals

```cpp
signals:
    void statusMessage(const QString& msg);
    void scrapeProgress(int current, int total, const QString& gameName, const QString& status);
    void scrapeFinished(int succeeded, int failed, int skipped);
    void credentialsValidated(bool success, const QString& message);
```

## AppController Changes

### New Q_INVOKABLE methods

```cpp
// Credential validation
Q_INVOKABLE void validateScraperCredentials(const QString& user, const QString& pass);
Q_INVOKABLE void scraperSignOut();
Q_INVOKABLE bool hasScraperCredentials() const;

// Batch scrape
Q_INVOKABLE void startBatchScrape(const QStringList& mediaTypes,
                                   const QStringList& systems,
                                   const QString& gameFilter);
Q_INVOKABLE void cancelScrape();

// Data for UI
Q_INVOKABLE QVariantList scrapableSystems() const;  // systems with games + game counts
```

### New signals

```cpp
signals:
    void scraperCredentialsValidated(bool success, const QString& message);
    void scrapeProgress(int current, int total, const QString& gameName, const QString& status);
    void scrapeFinished(int succeeded, int failed, int skipped);
```

### Remove
- `saveScraperCredentials()` — replaced by `validateScraperCredentials()`

## QML UI Changes

### ScraperSettings.qml — complete rewrite

Becomes a multi-screen component with internal navigation state:

```
state: "login" | "hub" | "account" | "content" | "systems" | "scrape" | "progress"
```

**Login screen** (state: `login`):
- Username + password fields
- "Sign In" button calls `app.validateScraperCredentials(user, pass)`
- On success signal → transition to `hub`
- On failure → show inline error message
- Shown when `app.hasScraperCredentials()` is false

**Hub screen** (state: `hub`):
- Shows "Connected as {username}" with green dot
- 2x2 card grid: Account, Content, Systems, Start Scraping
- Clicking a card sets state to that sub-page

**Account sub-page** (state: `account`):
- Pre-filled username/password fields
- "Update & Validate" button (re-validates with API)
- "Sign Out" button — clears credentials, returns to login state
- Back arrow → hub

**Content sub-page** (state: `content`):
- 2-column checkbox grid for all 11 media types
- All checked by default (resets each time page is entered)
- Subtitle: "Select which media types to download. These selections reset each session."
- Back arrow → hub

**Systems sub-page** (state: `systems`):
- "Select All / Deselect All" toggle
- Vertical checklist of installed systems with game counts
- Only shows systems that have imported games (from `app.scrapableSystems()`)
- All checked by default
- Back arrow → hub

**Start Scraping sub-page** (state: `scrape`):
- Radio buttons: All Games / Unscraped Games Only / Favorites Only
- Summary box: "Ready to scrape X games across Y systems — Z media types selected"
- "Start Scraping" button → calls `app.startBatchScrape()`, transitions to `progress`
- Back arrow → hub

**Progress screen** (state: `progress`):
- Displayed as a modal overlay (same z-level as settings)
- Progress bar + "X / Y" counter
- "Currently: {game name} ({system})"
- Scrollable log with color-coded entries:
  - Green (success): "Game Name — N media downloaded"
  - Yellow (partial): "Game Name — N media (M not available)"
  - Red (failure): "Game Name — error message"
- "Cancel" button — stops scraping, stays on progress screen showing results
- When complete, Cancel button changes to "Done" which returns to hub

### SettingsOverlay.qml
No structural changes needed — ScraperSettings is already at index 3 in the StackLayout. The category subtitle could change from "ScreenScraper login" to "Media & metadata" to reflect the expanded scope.

## Threading

Batch scraping must run on a background thread to keep the UI responsive. The current `scrapeAll()` uses `QThread::msleep()` on the main thread which blocks the UI.

- `ScraperService::scrapeBatch()` runs in a `QThread` (or `QtConcurrent::run`)
- Progress signals are emitted from the worker and connected via `Qt::QueuedConnection`
- Cancel is implemented via an atomic flag checked between games
- The 1200ms delay between API calls is preserved (ScreenScraper rate limiting)

## Right-Click Single Game Scrape

The existing `ThemeContext::scrapeGame(int gameId)` continues to work. Updated to:
- Download all 11 media types (not just covers)
- Store all paths in the database
- Show progress in the status bar (not the modal — it's a single game)

## "Unscraped" Definition

A game is considered "unscraped" if its `cover_path` is empty or the file does not exist. This keeps the check simple and consistent with the current behavior. Even if some other media types were partially downloaded, the game shows as "unscraped" until it has at least a cover.

## Database `updateGameMetadata` Update

The existing `Database::updateGameMetadata(int id, const GameRecord& metadata)` method is updated to also write the new media path columns. Each column is only written if the corresponding field in `metadata` is non-empty, so partial scrapes don't erase existing data.

## Migration Path for Existing Data

- Existing `cover_path` values pointing to `covers/{system}/{title}.ext` continue to work
- No automatic migration of existing cover files to the new folder structure
- When a game is re-scraped, the new cover goes to `downloaded_media/{system}/covers/` and `cover_path` is updated
- Old `covers/` directory can be manually deleted by the user after re-scraping

## Developer Credentials (Build-Time Injection)

ScreenScraper requires developer credentials (devId, devPassword, softname) for all API calls. These are separate from user credentials and must be embedded in the app but never committed to git.

### Setup

1. Create `cpp/dev_credentials.cmake` (gitignored):
   ```cmake
   set(SCREENSCRAPER_DEV_ID "your_dev_id")
   set(SCREENSCRAPER_DEV_PASSWORD "your_dev_password")
   set(SCREENSCRAPER_SOFTNAME "YourAppName")
   ```

2. Add to `.gitignore`:
   ```
   cpp/dev_credentials.cmake
   ```

3. In `cpp/CMakeLists.txt`, include the file and pass as compile definitions:
   ```cmake
   # ScreenScraper developer credentials (build-time, not in repo)
   set(SCREENSCRAPER_DEV_ID "" CACHE STRING "ScreenScraper dev ID")
   set(SCREENSCRAPER_DEV_PASSWORD "" CACHE STRING "ScreenScraper dev password")
   set(SCREENSCRAPER_SOFTNAME "EmulatorFrontend" CACHE STRING "ScreenScraper software name")
   include(dev_credentials.cmake OPTIONAL)

   target_compile_definitions(EmulatorFrontend PRIVATE
       SCREENSCRAPER_DEV_ID="${SCREENSCRAPER_DEV_ID}"
       SCREENSCRAPER_DEV_PASSWORD="${SCREENSCRAPER_DEV_PASSWORD}"
       SCREENSCRAPER_SOFTNAME="${SCREENSCRAPER_SOFTNAME}"
   )
   ```

4. In `scraper.cpp`, replace hardcoded values:
   ```cpp
   Scraper::Scraper(QObject* parent) : QObject(parent) {
       m_devId = QStringLiteral(SCREENSCRAPER_DEV_ID);
       m_devPassword = QStringLiteral(SCREENSCRAPER_DEV_PASSWORD);
       m_softname = QStringLiteral(SCREENSCRAPER_SOFTNAME);
   }
   ```

### How to add credentials later

Once you have a ScreenScraper dev account, create `cpp/dev_credentials.cmake` with your credentials and rebuild. No code changes needed — just create the file and run cmake.

### Fallback behavior

If `dev_credentials.cmake` is missing, the `CACHE STRING` defaults provide empty strings. The scraper will fail API calls gracefully and report an error to the user. This allows the project to build without credentials (for contributors, CI, etc.).
