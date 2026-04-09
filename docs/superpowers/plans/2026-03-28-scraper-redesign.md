# Scraper Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single-cover scraper with an ES-DE-style media system supporting 11 media types, build-time dev credentials, credential validation, multi-page settings UI, batch scraping with filters, and a progress modal.

**Architecture:** Bottom-up: CMake credentials → Paths → Database migration → GameRecord/GameListModel → Scraper core (multi-media + validation) → ScraperService (batch + threading) → AppController (new Q_INVOKABLE methods) → QML UI (multi-screen ScraperSettings + progress modal). Each layer builds on the previous.

**Tech Stack:** C++17, Qt6 (QML + Widgets), CMake, SQLite, ScreenScraper API, QtConcurrent

---

## File Map

### New files
- `cpp/dev_credentials.cmake` — gitignored, user creates with real credentials

### Modified files
| File | Responsibility |
|------|---------------|
| `.gitignore` | Add `cpp/dev_credentials.cmake` |
| `cpp/CMakeLists.txt` | Add compile definitions for dev credentials |
| `cpp/src/core/paths.h` | Replace `coversDir()` with `mediaDir()` overloads |
| `cpp/src/core/paths.cpp` | Implement `mediaDir()`, update `ensureDirectories()` |
| `cpp/src/core/database.h` | Add media path fields to `GameRecord` |
| `cpp/src/core/database.cpp` | Migration v2→v3, update `updateGameMetadata()`, `GAME_SELECT_COLUMNS`, `recordFromQuery()` |
| `cpp/src/core/scraper.h` | Expand `ScrapeResult`, new `scrapeGame()` signature, add `validateCredentials()` |
| `cpp/src/core/scraper.cpp` | Multi-media download loop, credential validation, build-time dev credentials |
| `cpp/src/services/scraper_credentials.h` | Remove hardcoded dev credentials, dev fields become compile-time |
| `cpp/src/services/scraper_credentials.cpp` | Use compile-time defines for dev fields |
| `cpp/src/services/scraper_service.h` | Add `ScrapeOptions`, `scrapeBatch()`, `validateAndSaveCredentials()`, `signOut()`, new signals |
| `cpp/src/services/scraper_service.cpp` | Batch scraping with threading, validation, signOut |
| `cpp/src/ui/game_list_model.h` | Add 10 new roles for media paths |
| `cpp/src/ui/game_list_model.cpp` | Implement new roles in `data()`, `roleNames()` |
| `cpp/src/ui/app_controller.h` | New Q_INVOKABLE methods + signals for scraper |
| `cpp/src/ui/app_controller.cpp` | Implement new scraper methods, remove `saveScraperCredentials()` |
| `cpp/qml/AppUI/ScraperSettings.qml` | Complete rewrite: multi-screen with 7 states |
| `cpp/qml/AppUI/SettingsOverlay.qml` | Update category subtitle |

---

### Task 1: Build-Time Developer Credentials

**Files:**
- Create: `cpp/dev_credentials.cmake` (template, gitignored)
- Modify: `.gitignore`
- Modify: `cpp/CMakeLists.txt:1-10`
- Modify: `cpp/src/services/scraper_credentials.h:12-14`
- Modify: `cpp/src/services/scraper_credentials.cpp:30-37`
- Modify: `cpp/src/core/scraper.cpp:18-22`

- [ ] **Step 1: Add `cpp/dev_credentials.cmake` to `.gitignore`**

Open `.gitignore` and add:

```
# ScreenScraper developer credentials (build-time secret)
cpp/dev_credentials.cmake
```

- [ ] **Step 2: Add compile definitions to `cpp/CMakeLists.txt`**

After line 9 (`find_package(SDL2 REQUIRED)`), add:

```cmake
# ScreenScraper developer credentials (build-time, not in repo)
set(SCREENSCRAPER_DEV_ID "" CACHE STRING "ScreenScraper dev ID")
set(SCREENSCRAPER_DEV_PASSWORD "" CACHE STRING "ScreenScraper dev password")
set(SCREENSCRAPER_SOFTNAME "EmulatorFrontend" CACHE STRING "ScreenScraper software name")
include(dev_credentials.cmake OPTIONAL)
```

After the `target_include_directories` block (after line 87), add:

```cmake
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SCREENSCRAPER_DEV_ID="${SCREENSCRAPER_DEV_ID}"
    SCREENSCRAPER_DEV_PASSWORD="${SCREENSCRAPER_DEV_PASSWORD}"
    SCREENSCRAPER_SOFTNAME="${SCREENSCRAPER_SOFTNAME}"
)
```

- [ ] **Step 3: Update `scraper_credentials.h` to use compile-time dev credentials**

Replace the hardcoded defaults in `scraper_credentials.h`:

```cpp
class ScraperCredentials {
public:
    QString devId;
    QString devPassword;
    QString softname;
    QString ssId;
    QString ssPassword;

    /** Load from disk. Returns true if file existed and was read. */
    bool load();

    /** Save to disk. Returns true on success. */
    bool save() const;

    /** Clear user credentials. */
    void clearUser();

    /** True if user credentials are set. */
    bool hasUserCredentials() const { return !ssId.isEmpty(); }

private:
    static QString filePath();
};
```

- [ ] **Step 4: Update `scraper_credentials.cpp` to use compile-time defines**

Remove dev credential fields from `save()` and `load()` — they are no longer persisted. Only user credentials are stored in `scraper.json`:

```cpp
#include "scraper_credentials.h"
#include "core/paths.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>

// Stringify macro for compile-time defines
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

QString ScraperCredentials::filePath() {
    return Paths::configDir() + "/scraper.json";
}

bool ScraperCredentials::load() {
    // Dev credentials from compile-time defines
    devId = QStringLiteral(STRINGIFY(SCREENSCRAPER_DEV_ID));
    devPassword = QStringLiteral(STRINGIFY(SCREENSCRAPER_DEV_PASSWORD));
    softname = QStringLiteral(STRINGIFY(SCREENSCRAPER_SOFTNAME));

    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    ssId = obj["ssid"].toString();
    ssPassword = obj["sspassword"].toString();

    return true;
}

bool ScraperCredentials::save() const {
    QJsonObject obj;
    obj["ssid"] = ssId;
    obj["sspassword"] = ssPassword;

    QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    f.write(QJsonDocument(obj).toJson());
    f.close();
    return true;
}

void ScraperCredentials::clearUser() {
    ssId.clear();
    ssPassword.clear();

    // Delete the credentials file
    QFile::remove(filePath());
}
```

- [ ] **Step 5: Update `scraper.cpp` constructor to use credentials from ScraperCredentials**

The `Scraper` constructor no longer sets hardcoded dev credentials — they are injected via `setCredentials()` which is already called by `ScraperService::loadCredentials()`. Remove the hardcoded values:

```cpp
Scraper::Scraper(QObject* parent) : QObject(parent) {
}
```

- [ ] **Step 6: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Compiles successfully. Dev credentials are empty strings (no `dev_credentials.cmake` file), which is fine.

- [ ] **Step 7: Commit**

```bash
git add .gitignore cpp/CMakeLists.txt cpp/src/services/scraper_credentials.h cpp/src/services/scraper_credentials.cpp cpp/src/core/scraper.cpp
git commit -m "feat: move dev credentials to build-time injection via CMake"
```

---

### Task 2: Paths — Replace `coversDir()` with `mediaDir()`

**Files:**
- Modify: `cpp/src/core/paths.h:26`
- Modify: `cpp/src/core/paths.cpp:57-59,69-85`
- Modify: `cpp/src/services/scraper_service.cpp:38,68`
- Modify: `cpp/src/ui/game_list_model.cpp:87-89,96-125`
- Modify: `cpp/src/ui/game_list_model.h:45`

- [ ] **Step 1: Update `paths.h` — replace `coversDir()` with `mediaDir()` overloads**

Replace:
```cpp
static QString coversDir();
```

With:
```cpp
/** Media directory for scraped content (ES-DE style). */
static QString mediaDir();
static QString mediaDir(const QString& system);
static QString mediaDir(const QString& system, const QString& mediaType);
```

- [ ] **Step 2: Update `paths.cpp` — implement `mediaDir()`, update `ensureDirectories()`**

Replace the `coversDir()` implementation:

```cpp
QString Paths::mediaDir() {
    return s_root + "/downloaded_media";
}

QString Paths::mediaDir(const QString& system) {
    return s_root + "/downloaded_media/" + system;
}

QString Paths::mediaDir(const QString& system, const QString& mediaType) {
    return s_root + "/downloaded_media/" + system + "/" + mediaType;
}
```

In `ensureDirectories()`, replace `coversDir()` with `mediaDir()`:

```cpp
void Paths::ensureDirectories() {
    QStringList dirs = {
        s_root,
        emulatorsDir(),
        biosDir(),
        savesDir(),
        cacheDir(),
        romsDir(),
        mediaDir(),
        configDir(),
        themesDir(),
    };
    for (const auto& d : dirs) {
        QDir().mkpath(d);
    }
    qInfo() << "[Paths] Root:" << s_root;
}
```

- [ ] **Step 3: Update `scraper_service.cpp` — replace `Paths::coversDir()` references**

In `scrapeGame()` (line 38), replace:
```cpp
auto result = m_scraper->scrapeGame(target, Paths::coversDir());
```
With:
```cpp
auto result = m_scraper->scrapeGame(target, Paths::mediaDir());
```

In `scrapeAll()` (line 68), replace:
```cpp
QString coversDir = Paths::coversDir();
```
With:
```cpp
QString mediaBaseDir = Paths::mediaDir();
```

And update the call on line 74:
```cpp
auto result = m_scraper->scrapeGame(toScrape[i], mediaBaseDir);
```

- [ ] **Step 4: Update `game_list_model.h` — rename `setCoversDir` and field**

Replace:
```cpp
void setCoversDir(const QString& dir);
```
With:
```cpp
void setMediaDir(const QString& dir);
```

Replace the private field:
```cpp
QString m_coversDir;
```
With:
```cpp
QString m_mediaDir;
```

And rename the private method:
```cpp
QString resolveCoverPath(const GameRecord& game) const;
```
To:
```cpp
QString resolveCoverPath(const GameRecord& game) const;  // unchanged name, but will use m_mediaDir
```

- [ ] **Step 5: Update `game_list_model.cpp` — use `m_mediaDir`**

Replace:
```cpp
void GameListModel::setCoversDir(const QString& dir) {
    m_coversDir = dir;
}
```
With:
```cpp
void GameListModel::setMediaDir(const QString& dir) {
    m_mediaDir = dir;
}
```

In `resolveCoverPath()`, replace all `m_coversDir` references with `m_mediaDir`, and update the fallback search to look in `downloaded_media/{system}/covers/`:

```cpp
QString GameListModel::resolveCoverPath(const GameRecord& game) const {
    // 1. Database cover_path
    if (!game.cover_path.isEmpty() && QFileInfo::exists(game.cover_path))
        return game.cover_path;

    if (m_mediaDir.isEmpty()) return {};

    QStringList exts = {"png", "jpg", "jpeg", "webp"};

    // 2. downloaded_media/{system}/covers/{title}
    for (const auto& ext : exts) {
        QString path = m_mediaDir + "/" + game.system + "/covers/" + game.title + "." + ext;
        if (QFileInfo::exists(path)) return path;
    }

    // 3. downloaded_media/{system}/covers/{rom_filename}
    QString romBase = QFileInfo(game.rom_path).completeBaseName();
    for (const auto& ext : exts) {
        QString path = m_mediaDir + "/" + game.system + "/covers/" + romBase + "." + ext;
        if (QFileInfo::exists(path)) return path;
    }

    return {};
}
```

- [ ] **Step 6: Update any callers of `setCoversDir`**

Search for `setCoversDir` in the codebase. It's called in `main.cpp` or wherever the model is initialized. Update the call to `setMediaDir(Paths::mediaDir())`.

Run:
```bash
grep -rn "setCoversDir\|coversDir" cpp/src/
```

Update each reference found.

- [ ] **Step 7: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Compiles with no errors. No references to `coversDir` remain.

- [ ] **Step 8: Commit**

```bash
git add cpp/src/core/paths.h cpp/src/core/paths.cpp cpp/src/services/scraper_service.cpp cpp/src/ui/game_list_model.h cpp/src/ui/game_list_model.cpp
git commit -m "feat: replace coversDir with mediaDir for ES-DE style media structure"
```

---

### Task 3: Database Migration — Add Media Path Columns

**Files:**
- Modify: `cpp/src/core/database.h:6-27`
- Modify: `cpp/src/core/database.cpp:41,56-73,117-148,180-204,288-306`

- [ ] **Step 1: Add media path fields to `GameRecord` in `database.h`**

Add after `QString players;` (line 21):

```cpp
    // Media paths (populated by scraper)
    QString screenshot_path;
    QString titlescreen_path;
    QString marquee_path;
    QString fanart_path;
    QString box3d_path;
    QString backcover_path;
    QString miximage_path;
    QString physicalmedia_path;
    QString manual_path;
    QString video_path;
```

- [ ] **Step 2: Update `CURRENT_SCHEMA_VERSION` to 3**

In `database.cpp`, line 41, change:
```cpp
static const int CURRENT_SCHEMA_VERSION = 2;
```
To:
```cpp
static const int CURRENT_SCHEMA_VERSION = 3;
```

- [ ] **Step 3: Update `createTables()` — add new columns to CREATE TABLE**

In `database.cpp`, add the new columns to the CREATE TABLE statement (after `favorite` column, before the closing `")`):

```cpp
    if (!q.exec(
        "CREATE TABLE IF NOT EXISTS games ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL DEFAULT '',"
        "  rom_path TEXT NOT NULL UNIQUE,"
        "  system TEXT NOT NULL DEFAULT '',"
        "  emulator_id TEXT NOT NULL DEFAULT '',"
        "  cover_path TEXT NOT NULL DEFAULT '',"
        "  description TEXT NOT NULL DEFAULT '',"
        "  developer TEXT NOT NULL DEFAULT '',"
        "  publisher TEXT NOT NULL DEFAULT '',"
        "  release_date TEXT NOT NULL DEFAULT '',"
        "  genres TEXT NOT NULL DEFAULT '',"
        "  rating REAL NOT NULL DEFAULT 0.0,"
        "  players TEXT NOT NULL DEFAULT '',"
        "  last_played TEXT NOT NULL DEFAULT '',"
        "  play_count INTEGER NOT NULL DEFAULT 0,"
        "  favorite INTEGER NOT NULL DEFAULT 0,"
        "  screenshot_path TEXT NOT NULL DEFAULT '',"
        "  titlescreen_path TEXT NOT NULL DEFAULT '',"
        "  marquee_path TEXT NOT NULL DEFAULT '',"
        "  fanart_path TEXT NOT NULL DEFAULT '',"
        "  box3d_path TEXT NOT NULL DEFAULT '',"
        "  backcover_path TEXT NOT NULL DEFAULT '',"
        "  miximage_path TEXT NOT NULL DEFAULT '',"
        "  physicalmedia_path TEXT NOT NULL DEFAULT '',"
        "  manual_path TEXT NOT NULL DEFAULT '',"
        "  video_path TEXT NOT NULL DEFAULT ''"
        ")")) {
```

- [ ] **Step 4: Add v2→v3 migration in `runMigrations()`**

After the existing `if (current < 2)` block, add:

```cpp
    if (current < 3) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        QSqlQuery q(db);
        const QStringList alterStatements = {
            "ALTER TABLE games ADD COLUMN screenshot_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN titlescreen_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN marquee_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN fanart_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN box3d_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN backcover_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN miximage_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN physicalmedia_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN manual_path TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN video_path TEXT NOT NULL DEFAULT ''",
        };
        for (const auto& sql : alterStatements) {
            if (!q.exec(sql)) {
                qCritical() << "[Database] Migration v2→v3 failed:" << q.lastError().text() << "SQL:" << sql;
                return false;
            }
        }
        qInfo() << "[Database] Migrated schema v2 → v3 (added media path columns)";
    }
```

- [ ] **Step 5: Update `GAME_SELECT_COLUMNS` and `recordFromQuery()`**

Update the select columns string:

```cpp
static const char* GAME_SELECT_COLUMNS =
    "id, title, rom_path, system, emulator_id, cover_path, "
    "description, developer, publisher, release_date, genres, "
    "rating, players, last_played, play_count, favorite, "
    "screenshot_path, titlescreen_path, marquee_path, fanart_path, "
    "box3d_path, backcover_path, miximage_path, physicalmedia_path, "
    "manual_path, video_path";
```

Update `recordFromQuery()` to read the new columns:

```cpp
static GameRecord recordFromQuery(QSqlQuery& q) {
    GameRecord g;
    g.id                = q.value(0).toInt();
    g.title             = q.value(1).toString();
    g.rom_path          = q.value(2).toString();
    g.system            = q.value(3).toString();
    g.emulator_id       = q.value(4).toString();
    g.cover_path        = q.value(5).toString();
    g.description       = q.value(6).toString();
    g.developer         = q.value(7).toString();
    g.publisher         = q.value(8).toString();
    g.release_date      = q.value(9).toString();
    g.genres            = q.value(10).toString();
    g.rating            = q.value(11).toDouble();
    g.players           = q.value(12).toString();
    g.last_played       = q.value(13).toString();
    g.play_count        = q.value(14).toInt();
    g.favorite          = q.value(15).toInt();
    g.screenshot_path   = q.value(16).toString();
    g.titlescreen_path  = q.value(17).toString();
    g.marquee_path      = q.value(18).toString();
    g.fanart_path       = q.value(19).toString();
    g.box3d_path        = q.value(20).toString();
    g.backcover_path    = q.value(21).toString();
    g.miximage_path     = q.value(22).toString();
    g.physicalmedia_path= q.value(23).toString();
    g.manual_path       = q.value(24).toString();
    g.video_path        = q.value(25).toString();
    return g;
}
```

- [ ] **Step 6: Update `updateGameMetadata()` to write new columns**

Replace the existing method:

```cpp
bool Database::updateGameMetadata(int id, const GameRecord& metadata) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET description=?, developer=?, publisher=?, release_date=?, "
              "genres=?, rating=?, players=?, cover_path=?, "
              "screenshot_path=?, titlescreen_path=?, marquee_path=?, fanart_path=?, "
              "box3d_path=?, backcover_path=?, miximage_path=?, physicalmedia_path=?, "
              "manual_path=?, video_path=? WHERE id=?");
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

- [ ] **Step 7: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Compiles successfully.

- [ ] **Step 8: Commit**

```bash
git add cpp/src/core/database.h cpp/src/core/database.cpp
git commit -m "feat: add media path columns to database (schema v3 migration)"
```

---

### Task 4: GameListModel — Add Media Path Roles

**Files:**
- Modify: `cpp/src/ui/game_list_model.h:12-29`
- Modify: `cpp/src/ui/game_list_model.cpp:15-59`

- [ ] **Step 1: Add new roles to the enum in `game_list_model.h`**

After `FavoriteRole,` add:

```cpp
        ScreenshotPathRole,
        TitlescreenPathRole,
        MarqueePathRole,
        FanartPathRole,
        Box3dPathRole,
        BackcoverPathRole,
        MiximagePathRole,
        PhysicalmediaPathRole,
        ManualPathRole,
        VideoPathRole,
```

- [ ] **Step 2: Add cases to `data()` in `game_list_model.cpp`**

Before the `default:` case, add:

```cpp
    case ScreenshotPathRole:    return g.screenshot_path;
    case TitlescreenPathRole:   return g.titlescreen_path;
    case MarqueePathRole:       return g.marquee_path;
    case FanartPathRole:        return g.fanart_path;
    case Box3dPathRole:         return g.box3d_path;
    case BackcoverPathRole:     return g.backcover_path;
    case MiximagePathRole:      return g.miximage_path;
    case PhysicalmediaPathRole: return g.physicalmedia_path;
    case ManualPathRole:        return g.manual_path;
    case VideoPathRole:         return g.video_path;
```

- [ ] **Step 3: Add role names to `roleNames()` in `game_list_model.cpp`**

Add to the hash:

```cpp
        {ScreenshotPathRole,    "screenshotPath"},
        {TitlescreenPathRole,   "titlescreenPath"},
        {MarqueePathRole,       "marqueePath"},
        {FanartPathRole,        "fanartPath"},
        {Box3dPathRole,         "box3dPath"},
        {BackcoverPathRole,     "backcoverPath"},
        {MiximagePathRole,      "miximagePath"},
        {PhysicalmediaPathRole, "physicalmediaPath"},
        {ManualPathRole,        "manualPath"},
        {VideoPathRole,         "videoPath"},
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Compiles successfully.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/game_list_model.h cpp/src/ui/game_list_model.cpp
git commit -m "feat: add media path roles to GameListModel for theme access"
```

---

### Task 5: Scraper Core — Multi-Media Download & Credential Validation

**Files:**
- Modify: `cpp/src/core/scraper.h`
- Modify: `cpp/src/core/scraper.cpp`

- [ ] **Step 1: Update `scraper.h` with new ScrapeResult and method signatures**

Replace the entire header:

```cpp
#pragma once

#include "database.h"
#include <QString>
#include <QObject>
#include <QMap>
#include <QStringList>

/**
 * Scraper — fetches game metadata and media from ScreenScraper.
 *
 * API docs: https://www.screenscraper.fr/webapi2.php
 * Requires developer credentials and optionally user credentials.
 * Matches games by ROM filename against the ScreenScraper database.
 */
class Scraper : public QObject {
    Q_OBJECT

public:
    explicit Scraper(QObject* parent = nullptr);

    void setCredentials(const QString& devId, const QString& devPassword,
                        const QString& softname);
    void setUserCredentials(const QString& userId, const QString& userPassword);

    struct ScrapeResult {
        bool success = false;
        QString error;

        // Metadata
        QString title;
        QString description;
        QString developer;
        QString publisher;
        QString release_date;
        QString genres;
        double  rating = 0.0;
        QString players;

        // Media paths: mediaType -> local file path
        QMap<QString, QString> mediaPaths;
    };

    /**
     * Scrape a single game. Downloads requested media types to
     * mediaBaseDir/{system}/{mediaType}/.
     */
    ScrapeResult scrapeGame(const GameRecord& game,
                            const QString& mediaBaseDir,
                            const QStringList& mediaTypes);

    /** Validate user credentials against ScreenScraper API. */
    bool validateCredentials(const QString& userId, const QString& userPassword);

    /** All supported media type names. */
    static QStringList allMediaTypes();

signals:
    void progress(int current, int total, const QString& gameName);

private:
    static int systemToScreenScraperId(const QString& system);

    // Map our media type folder names to ScreenScraper media type strings
    static QStringList screenScraperMediaTypes(const QString& mediaType);

    QByteArray httpGet(const QString& url);
    bool downloadFile(const QString& url, const QString& destPath);

    QString m_devId;
    QString m_devPassword;
    QString m_softname;
    QString m_userId;
    QString m_userPassword;
};
```

- [ ] **Step 2: Implement the updated `scraper.cpp`**

Replace the entire file. Key changes:
- `scrapeGame()` now accepts `mediaBaseDir` and `mediaTypes` list
- Iterates through requested media types, finds matching media in API response
- Downloads each to `{mediaBaseDir}/{system}/{mediaType}/{title}.{ext}`
- Populates `mediaPaths` map
- New `validateCredentials()` hits `ssuserInfos.php`
- New `allMediaTypes()` returns the 11 type names
- New `screenScraperMediaTypes()` maps folder names to API type strings

```cpp
#include "scraper.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QUrlQuery>
#include <QFile>

static const QString API_BASE = "https://www.screenscraper.fr/api2/";

Scraper::Scraper(QObject* parent) : QObject(parent) {
}

void Scraper::setCredentials(const QString& devId, const QString& devPassword,
                              const QString& softname) {
    m_devId = devId;
    m_devPassword = devPassword;
    m_softname = softname;
}

void Scraper::setUserCredentials(const QString& userId, const QString& userPassword) {
    m_userId = userId;
    m_userPassword = userPassword;
}

QStringList Scraper::allMediaTypes() {
    return {"covers", "screenshots", "titlescreens", "3dboxes", "backcovers",
            "fanart", "marquees", "miximages", "physicalmedia", "manuals", "videos"};
}

QStringList Scraper::screenScraperMediaTypes(const QString& mediaType) {
    static const QHash<QString, QStringList> map = {
        {"covers",        {"box-2D"}},
        {"screenshots",   {"ss"}},
        {"titlescreens",  {"sstitle"}},
        {"3dboxes",       {"box-3D"}},
        {"backcovers",    {"box-2D-back"}},
        {"fanart",        {"fanart"}},
        {"marquees",      {"screenmarquee"}},
        {"miximages",     {"mixrbv1", "mixrbv2"}},
        {"physicalmedia", {"support-2D"}},
        {"manuals",       {"manuel"}},
        {"videos",        {"video", "video-normalized"}},
    };
    return map.value(mediaType);
}

int Scraper::systemToScreenScraperId(const QString& system) {
    static const QHash<QString, int> map = {
        {"psx",     57},   {"ps2",     58},   {"psp",     61},
        {"nes",     3},    {"snes",    4},    {"n64",     14},
        {"gb",      9},    {"gbc",     10},   {"gba",     12},
        {"nds",     15},   {"3ds",     17},   {"gc",      13},
        {"wii",     16},   {"wiiu",    18},   {"switch",  225},
        {"genesis", 1},    {"saturn",  22},   {"dreamcast", 23},
        {"gamegear", 21},  {"mastersystem", 2},
        {"atari2600", 26}, {"atari7800", 41},
        {"lynx",    28},   {"jaguar",  27},
        {"pcengine", 31},  {"neogeo",  142},  {"arcade",  75},
    };
    return map.value(system.toLower(), -1);
}

bool Scraper::validateCredentials(const QString& userId, const QString& userPassword) {
    QUrlQuery query;
    query.addQueryItem("devid", m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname", m_softname);
    query.addQueryItem("output", "json");
    query.addQueryItem("ssid", userId);
    query.addQueryItem("sspassword", userPassword);

    QString url = API_BASE + "ssuserInfos.php?" + query.toString(QUrl::FullyEncoded);
    QByteArray data = httpGet(url);
    if (data.isEmpty()) return false;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;

    QJsonObject root = doc.object();
    // API returns "response" > "ssuser" on success
    return root.contains("response") &&
           root["response"].toObject().contains("ssuser");
}

Scraper::ScrapeResult Scraper::scrapeGame(const GameRecord& game,
                                           const QString& mediaBaseDir,
                                           const QStringList& mediaTypes) {
    ScrapeResult result;

    int systemId = systemToScreenScraperId(game.system);
    if (systemId < 0) {
        result.error = "Unknown system: " + game.system;
        return result;
    }

    // Build API URL
    QString romFileName = QFileInfo(game.rom_path).fileName();

    QUrlQuery query;
    query.addQueryItem("devid", m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname", m_softname);
    query.addQueryItem("ssid", m_userId);
    query.addQueryItem("sspassword", m_userPassword);
    query.addQueryItem("output", "json");
    query.addQueryItem("systemeid", QString::number(systemId));
    query.addQueryItem("romnom", romFileName);

    QString url = API_BASE + "jeuInfos.php?" + query.toString(QUrl::FullyEncoded);

    qInfo() << "[Scraper] Looking up:" << romFileName;

    QByteArray data = httpGet(url);
    if (data.isEmpty()) {
        result.error = "No response from ScreenScraper";
        return result;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull()) {
        result.error = "JSON parse error: " + parseErr.errorString();
        return result;
    }

    QJsonObject root = doc.object();
    QJsonObject response = root["response"].toObject();
    QJsonObject jeu = response["jeu"].toObject();

    if (jeu.isEmpty()) {
        result.error = "Game not found on ScreenScraper";
        return result;
    }

    // ── Extract metadata ──
    // Title (prefer English)
    QJsonArray noms = jeu["noms"].toArray();
    for (const auto& nom : noms) {
        QJsonObject n = nom.toObject();
        if (n["region"].toString() == "us" || n["region"].toString() == "wor") {
            result.title = n["text"].toString();
            break;
        }
    }
    if (result.title.isEmpty() && !noms.isEmpty())
        result.title = noms[0].toObject()["text"].toString();

    // Description (prefer English)
    QJsonArray synopsis = jeu["synopsis"].toArray();
    for (const auto& s : synopsis) {
        QJsonObject obj = s.toObject();
        if (obj["langue"].toString() == "en") {
            result.description = obj["text"].toString();
            break;
        }
    }
    if (result.description.isEmpty() && !synopsis.isEmpty())
        result.description = synopsis[0].toObject()["text"].toString();

    // Developer
    QJsonObject developpeur = jeu["developpeur"].toObject();
    if (!developpeur.isEmpty())
        result.developer = developpeur["text"].toString();

    // Publisher
    QJsonObject editeur = jeu["editeur"].toObject();
    if (!editeur.isEmpty())
        result.publisher = editeur["text"].toString();

    // Release date
    QJsonArray dates = jeu["dates"].toArray();
    if (!dates.isEmpty())
        result.release_date = dates[0].toObject()["text"].toString();

    // Genres
    QJsonArray genres = jeu["genres"].toArray();
    QStringList genreList;
    for (const auto& genre : genres) {
        QJsonArray noms_genre = genre.toObject()["noms"].toArray();
        for (const auto& nom : noms_genre) {
            QJsonObject n = nom.toObject();
            if (n["langue"].toString() == "en") {
                genreList.append(n["text"].toString());
                break;
            }
        }
        if (genreList.isEmpty() && !noms_genre.isEmpty())
            genreList.append(noms_genre[0].toObject()["text"].toString());
    }
    result.genres = genreList.join(", ");

    // Rating (ScreenScraper 0-20 → 0-5)
    QString noteStr = jeu["note"].toObject()["text"].toString();
    if (!noteStr.isEmpty()) {
        bool ok;
        double note = noteStr.toDouble(&ok);
        if (ok) result.rating = qBound(0.0, note / 4.0, 5.0);
    }

    // Players
    result.players = jeu["joueurs"].toObject()["text"].toString();

    // ── Download media ──
    QJsonArray medias = jeu["medias"].toArray();
    QStringList regionPriority = {"us", "wor", "eu", "jp"};

    QString gameTitle = result.title.isEmpty() ? game.title : result.title;

    for (const auto& mediaType : mediaTypes) {
        QStringList ssTypes = screenScraperMediaTypes(mediaType);
        if (ssTypes.isEmpty()) continue;

        // Find the best matching media URL
        QString mediaUrl;
        for (const auto& ssType : ssTypes) {
            // Try each region in priority order
            for (const auto& region : regionPriority) {
                for (const auto& media : medias) {
                    QJsonObject m = media.toObject();
                    if (m["type"].toString() == ssType && m["region"].toString() == region) {
                        mediaUrl = m["url"].toString();
                        break;
                    }
                }
                if (!mediaUrl.isEmpty()) break;
            }
            // Fallback: any region
            if (mediaUrl.isEmpty()) {
                for (const auto& media : medias) {
                    QJsonObject m = media.toObject();
                    if (m["type"].toString() == ssType) {
                        mediaUrl = m["url"].toString();
                        break;
                    }
                }
            }
            if (!mediaUrl.isEmpty()) break;
        }

        if (mediaUrl.isEmpty()) continue;

        // Determine file extension
        QString ext = "png";
        if (mediaUrl.contains(".jpg", Qt::CaseInsensitive) ||
            mediaUrl.contains(".jpeg", Qt::CaseInsensitive))
            ext = "jpg";
        else if (mediaUrl.contains(".mp4", Qt::CaseInsensitive))
            ext = "mp4";
        else if (mediaUrl.contains(".pdf", Qt::CaseInsensitive))
            ext = "pdf";

        // Create destination directory
        QString destDir = mediaBaseDir + "/" + game.system + "/" + mediaType;
        QDir().mkpath(destDir);

        QString destPath = destDir + "/" + gameTitle + "." + ext;

        // Add auth to URL
        QString authUrl = mediaUrl;
        if (!authUrl.contains("?")) authUrl += "?";
        else authUrl += "&";
        authUrl += "devid=" + m_devId + "&devpassword=" + m_devPassword
                 + "&softname=" + m_softname
                 + "&ssid=" + m_userId + "&sspassword=" + m_userPassword;

        if (downloadFile(authUrl, destPath)) {
            result.mediaPaths[mediaType] = destPath;
            qInfo() << "[Scraper] Downloaded" << mediaType << "for:" << gameTitle;
        }
    }

    result.success = !result.mediaPaths.isEmpty() || !result.title.isEmpty();
    return result;
}

QByteArray Scraper::httpGet(const QString& url) {
    QNetworkAccessManager mgr;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, m_softname);

    QNetworkReply* reply = mgr.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray data;
    if (reply->error() == QNetworkReply::NoError) {
        data = reply->readAll();
    } else {
        qWarning() << "[Scraper] HTTP error:" << reply->errorString();
    }

    reply->deleteLater();
    return data;
}

bool Scraper::downloadFile(const QString& url, const QString& destPath) {
    QByteArray data = httpGet(url);
    if (data.isEmpty()) return false;

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[Scraper] Cannot write:" << destPath;
        return false;
    }
    file.write(data);
    file.close();
    return true;
}
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Build will fail because `scraper_service.cpp` still calls the old `scrapeGame()` signature. That's expected — we fix it in Task 6.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/scraper.h cpp/src/core/scraper.cpp
git commit -m "feat: multi-media scraper with credential validation"
```

---

### Task 6: ScraperService — Batch Scraping with Threading

**Files:**
- Modify: `cpp/src/services/scraper_service.h`
- Modify: `cpp/src/services/scraper_service.cpp`

- [ ] **Step 1: Rewrite `scraper_service.h`**

```cpp
#pragma once

#include "core/database.h"
#include "core/scraper.h"
#include "services/scraper_credentials.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QAtomicInt>

class ScraperService : public QObject {
    Q_OBJECT

public:
    ScraperService(Database* db, QObject* parent = nullptr);

    /** Load credentials from disk and configure the scraper. */
    void loadCredentials();

    /** Validate user credentials against ScreenScraper API, save if valid. */
    bool validateAndSaveCredentials(const QString& ssId, const QString& ssPassword);

    /** Clear stored user credentials. */
    void signOut();

    /** True if user credentials are configured. */
    bool hasCredentials() const { return m_creds.hasUserCredentials(); }

    /** Access current credentials (read-only). */
    const ScraperCredentials& credentials() const { return m_creds; }

    /** Scrape a single game by ID (all media types). */
    struct ScrapeResult {
        bool success = false;
        QString message;
        int mediaDownloaded = 0;
    };
    ScrapeResult scrapeGame(int gameId);

    /** Batch scrape options. */
    struct ScrapeOptions {
        QStringList mediaTypes;
        QStringList systems;
        enum Filter { AllGames, UnscrapedOnly, FavoritesOnly };
        Filter gameFilter = AllGames;
    };

    /** Start a batch scrape on a background thread. */
    void startBatchScrape(const ScrapeOptions& options);

    /** Cancel a running batch scrape. */
    void cancelScrape();

signals:
    void statusMessage(const QString& msg);
    void scrapeProgress(int current, int total, const QString& gameName, const QString& status);
    void scrapeFinished(int succeeded, int failed, int skipped);
    void credentialsValidated(bool success, const QString& message);

private:
    void applyResultToDb(int gameId, const Scraper::ScrapeResult& result);

    Database* m_db;
    Scraper* m_scraper;
    ScraperCredentials m_creds;
    QAtomicInt m_cancelFlag;
};
```

- [ ] **Step 2: Rewrite `scraper_service.cpp`**

```cpp
#include "scraper_service.h"
#include "core/paths.h"

#include <QFileInfo>
#include <QThread>
#include <QtConcurrent>

ScraperService::ScraperService(Database* db, QObject* parent)
    : QObject(parent), m_db(db), m_scraper(new Scraper(this))
{
    m_cancelFlag.storeRelaxed(0);
}

void ScraperService::loadCredentials() {
    m_creds.load();
    m_scraper->setCredentials(m_creds.devId, m_creds.devPassword, m_creds.softname);
    if (m_creds.hasUserCredentials())
        m_scraper->setUserCredentials(m_creds.ssId, m_creds.ssPassword);
}

bool ScraperService::validateAndSaveCredentials(const QString& ssId, const QString& ssPassword) {
    // Ensure dev credentials are loaded
    m_scraper->setCredentials(m_creds.devId, m_creds.devPassword, m_creds.softname);

    bool valid = m_scraper->validateCredentials(ssId, ssPassword);
    if (valid) {
        m_creds.ssId = ssId;
        m_creds.ssPassword = ssPassword;
        m_creds.save();
        m_scraper->setUserCredentials(ssId, ssPassword);
        emit credentialsValidated(true, "Credentials validated successfully.");
    } else {
        emit credentialsValidated(false, "Invalid credentials. Please check your username and password.");
    }
    return valid;
}

void ScraperService::signOut() {
    m_creds.clearUser();
    m_scraper->setUserCredentials("", "");
    emit statusMessage("Signed out of ScreenScraper.");
}

void ScraperService::applyResultToDb(int gameId, const Scraper::ScrapeResult& result) {
    GameRecord metadata;
    metadata.description  = result.description;
    metadata.developer    = result.developer;
    metadata.publisher    = result.publisher;
    metadata.release_date = result.release_date;
    metadata.genres       = result.genres;
    metadata.rating       = result.rating;
    metadata.players      = result.players;

    // Map media paths to GameRecord fields
    metadata.cover_path        = result.mediaPaths.value("covers");
    metadata.screenshot_path   = result.mediaPaths.value("screenshots");
    metadata.titlescreen_path  = result.mediaPaths.value("titlescreens");
    metadata.marquee_path      = result.mediaPaths.value("marquees");
    metadata.fanart_path       = result.mediaPaths.value("fanart");
    metadata.box3d_path        = result.mediaPaths.value("3dboxes");
    metadata.backcover_path    = result.mediaPaths.value("backcovers");
    metadata.miximage_path     = result.mediaPaths.value("miximages");
    metadata.physicalmedia_path = result.mediaPaths.value("physicalmedia");
    metadata.manual_path       = result.mediaPaths.value("manuals");
    metadata.video_path        = result.mediaPaths.value("videos");

    m_db->updateGameMetadata(gameId, metadata);
}

ScraperService::ScrapeResult ScraperService::scrapeGame(int gameId) {
    GameRecord target = m_db->gameById(gameId);
    if (target.id == 0)
        return {false, "Game not found", 0};

    emit statusMessage("Scraping: " + target.title + "...");

    auto result = m_scraper->scrapeGame(target, Paths::mediaDir(), Scraper::allMediaTypes());

    if (result.success) {
        applyResultToDb(gameId, result);
        int count = result.mediaPaths.size();
        return {true, QString("Scraped: %1 (%2 media)").arg(target.title).arg(count), count};
    }
    return {false, "Scrape failed: " + result.error, 0};
}

void ScraperService::startBatchScrape(const ScrapeOptions& options) {
    m_cancelFlag.storeRelaxed(0);

    QtConcurrent::run([this, options]() {
        // Build game list based on filters
        QVector<GameRecord> allGames;
        for (const auto& system : options.systems) {
            auto games = m_db->gamesBySystem(system);
            allGames.append(games);
        }

        // Apply game filter
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

        int succeeded = 0, failed = 0, skipped = 0;
        QString mediaBaseDir = Paths::mediaDir();

        for (int i = 0; i < toScrape.size(); i++) {
            if (m_cancelFlag.loadRelaxed()) {
                skipped = toScrape.size() - i;
                break;
            }

            const auto& game = toScrape[i];
            emit scrapeProgress(i + 1, toScrape.size(), game.title, "scraping");

            auto result = m_scraper->scrapeGame(game, mediaBaseDir, options.mediaTypes);

            if (result.success) {
                applyResultToDb(game.id, result);
                int count = result.mediaPaths.size();
                int requested = options.mediaTypes.size();
                if (count < requested) {
                    emit scrapeProgress(i + 1, toScrape.size(), game.title,
                        QString("%1 media (%2 not available)").arg(count).arg(requested - count));
                } else {
                    emit scrapeProgress(i + 1, toScrape.size(), game.title,
                        QString("%1 media downloaded").arg(count));
                }
                succeeded++;
            } else {
                emit scrapeProgress(i + 1, toScrape.size(), game.title,
                    "failed: " + result.error);
                failed++;
            }

            // Rate limiting — ScreenScraper requires delays between requests
            if (i < toScrape.size() - 1 && !m_cancelFlag.loadRelaxed())
                QThread::msleep(1200);
        }

        emit scrapeFinished(succeeded, failed, skipped);
    });
}

void ScraperService::cancelScrape() {
    m_cancelFlag.storeRelaxed(1);
}
```

- [ ] **Step 3: Add `Concurrent` to CMakeLists.txt**

In `cpp/CMakeLists.txt`, update the `find_package` line:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Qml Quick QuickControls2 Concurrent)
```

And add to `target_link_libraries`:

```cmake
    Qt6::Concurrent
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Build will fail because `app_controller.cpp` still calls `saveScraperCredentials()` and old `scrapeAll()`. Fixed in Task 7.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/scraper_service.h cpp/src/services/scraper_service.cpp cpp/CMakeLists.txt
git commit -m "feat: batch scraping with threading, validation, and cancel support"
```

---

### Task 7: AppController — New Scraper Methods

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Update `app_controller.h`**

Replace the scraper-related section. Remove `scrapeAll()` and `saveScraperCredentials()`. Add new methods and signals.

Remove these lines:
```cpp
    Q_INVOKABLE void scrapeAll();
```
```cpp
    Q_INVOKABLE void saveScraperCredentials(const QString& user, const QString& pass);
```

Add these methods (in the scraper section):
```cpp
    // Scraper
    Q_INVOKABLE void scrapeGame(int gameId);
    Q_INVOKABLE void validateScraperCredentials(const QString& user, const QString& pass);
    Q_INVOKABLE void scraperSignOut();
    Q_INVOKABLE bool hasScraperCredentials() const;
    Q_INVOKABLE QString scraperUsername() const;
    Q_INVOKABLE void startBatchScrape(const QStringList& mediaTypes,
                                       const QStringList& systems,
                                       const QString& gameFilter);
    Q_INVOKABLE void cancelScrape();
    Q_INVOKABLE QVariantList scrapableSystems() const;
    Q_INVOKABLE QStringList allMediaTypes() const;
```

Add new signals:
```cpp
    void scraperCredentialsValidated(bool success, const QString& message);
    void scraperSignedOut();
    void scrapeProgress(int current, int total, const QString& gameName, const QString& status);
    void scrapeFinished(int succeeded, int failed, int skipped);
```

- [ ] **Step 2: Update `app_controller.cpp` — replace scraper methods**

Replace the entire scraper section (from `// ── Scraper Settings ──` to end of file):

```cpp
// ── Scraper ───────────────────────────────────────────────

void AppController::scrapeGame(int gameId) {
    auto result = m_scraperService.scrapeGame(gameId);
    setStatus(result.message);
    if (result.success) emit gamesChanged();
}

void AppController::validateScraperCredentials(const QString& user, const QString& pass) {
    setStatus("Validating credentials...");
    bool valid = m_scraperService.validateAndSaveCredentials(user, pass);
    emit scraperCredentialsValidated(valid,
        valid ? "Connected to ScreenScraper." : "Invalid credentials.");
    setStatus(valid ? "Connected to ScreenScraper." : "Invalid credentials.");
}

void AppController::scraperSignOut() {
    m_scraperService.signOut();
    emit scraperSignedOut();
    setStatus("Signed out of ScreenScraper.");
}

bool AppController::hasScraperCredentials() const {
    return m_scraperService.hasCredentials();
}

QString AppController::scraperUsername() const {
    return m_scraperService.credentials().ssId;
}

void AppController::startBatchScrape(const QStringList& mediaTypes,
                                      const QStringList& systems,
                                      const QString& gameFilter) {
    ScraperService::ScrapeOptions options;
    options.mediaTypes = mediaTypes;
    options.systems = systems;

    if (gameFilter == "unscraped")
        options.gameFilter = ScraperService::ScrapeOptions::UnscrapedOnly;
    else if (gameFilter == "favorites")
        options.gameFilter = ScraperService::ScrapeOptions::FavoritesOnly;
    else
        options.gameFilter = ScraperService::ScrapeOptions::AllGames;

    // Forward signals from service to QML
    connect(&m_scraperService, &ScraperService::scrapeProgress,
            this, &AppController::scrapeProgress, Qt::UniqueConnection);
    connect(&m_scraperService, &ScraperService::scrapeFinished,
            this, [this](int succeeded, int failed, int skipped) {
                emit scrapeFinished(succeeded, failed, skipped);
                emit gamesChanged();
            }, Qt::UniqueConnection);

    m_scraperService.startBatchScrape(options);
}

void AppController::cancelScrape() {
    m_scraperService.cancelScrape();
    setStatus("Scrape cancelled.");
}

QVariantList AppController::scrapableSystems() const {
    QVariantList list;
    auto counts = m_db->systemGameCounts();
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        QVariantMap item;
        item["id"] = it.key();
        item["name"] = it.key();  // ThemeContext::systemDisplayName is private; use raw ID
        item["count"] = it.value();
        list.append(item);
    }
    return list;
}

QStringList AppController::allMediaTypes() const {
    return Scraper::allMediaTypes();
}
```

- [ ] **Step 3: Connect scraper service signals in constructor**

In the `AppController` constructor, add after the existing connects:

```cpp
    connect(&m_scraperService, &ScraperService::credentialsValidated,
            this, &AppController::scraperCredentialsValidated);
```

- [ ] **Step 4: Remove `scraperPassword()` from header**

Remove:
```cpp
    Q_INVOKABLE QString scraperPassword() const;
```

(The password should not be exposed to QML for security — the login page will store it in local QML state only.)

- [ ] **Step 5: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: May fail due to QML references to `scraperPassword()` and `saveScraperCredentials()` in the old `ScraperSettings.qml`. That's fine — we rewrite it in Task 8.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "feat: add scraper validation, batch scrape, and sign-out to AppController"
```

---

### Task 8: ScraperSettings.qml — Complete Rewrite

**Files:**
- Modify: `cpp/qml/AppUI/ScraperSettings.qml`
- Modify: `cpp/qml/AppUI/SettingsOverlay.qml:320`

- [ ] **Step 1: Update SettingsOverlay.qml subtitle**

In `SettingsOverlay.qml`, line 320, change:
```qml
ListElement { name: "Scraper";       icon: "\u{1F50D}"; subtitle: "ScreenScraper login";  action: "page"; widgetAction: "" }
```
To:
```qml
ListElement { name: "Scraper";       icon: "\u{1F50D}"; subtitle: "Media & metadata";     action: "page"; widgetAction: "" }
```

- [ ] **Step 2: Rewrite ScraperSettings.qml**

Replace the entire file with the multi-screen component. The QML uses internal `state` property to navigate between screens: `"login"`, `"hub"`, `"account"`, `"content"`, `"systems"`, `"scrape"`, `"progress"`.

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string screenState: app.hasScraperCredentials() ? "hub" : "login"

    // Content settings (reset each time content page is entered)
    property var selectedMedia: []
    // System settings
    property var selectedSystems: []
    // Game filter
    property string gameFilter: "all"

    // Progress tracking
    property int progressCurrent: 0
    property int progressTotal: 0
    property string progressCurrentGame: ""
    property bool scrapeRunning: false

    ListModel { id: progressLog }

    Component.onCompleted: {
        resetMediaSelection()
        resetSystemSelection()
    }

    function resetMediaSelection() {
        selectedMedia = app.allMediaTypes()
    }

    function resetSystemSelection() {
        var systems = app.scrapableSystems()
        var ids = []
        for (var i = 0; i < systems.length; i++)
            ids.push(systems[i].id)
        selectedSystems = ids
    }

    function isMediaSelected(type) {
        return selectedMedia.indexOf(type) >= 0
    }

    function toggleMedia(type) {
        var list = selectedMedia.slice()
        var idx = list.indexOf(type)
        if (idx >= 0) list.splice(idx, 1)
        else list.push(type)
        selectedMedia = list
    }

    function isSystemSelected(id) {
        return selectedSystems.indexOf(id) >= 0
    }

    function toggleSystem(id) {
        var list = selectedSystems.slice()
        var idx = list.indexOf(id)
        if (idx >= 0) list.splice(idx, 1)
        else list.push(id)
        selectedSystems = list
    }

    Connections {
        target: app
        function onScraperCredentialsValidated(success, message) {
            if (success) {
                root.screenState = "hub"
                loginError.text = ""
            } else {
                loginError.text = message
                loginError.visible = true
            }
            signInBtn.enabled = true
        }
        function onScraperSignedOut() {
            root.screenState = "login"
        }
        function onScrapeProgress(current, total, gameName, status) {
            root.progressCurrent = current
            root.progressTotal = total
            root.progressCurrentGame = gameName
            progressLog.append({
                "gameName": gameName,
                "status": status,
                "isSuccess": status.indexOf("media downloaded") >= 0 || status.indexOf("media (") >= 0,
                "isPartial": status.indexOf("not available") >= 0,
                "isFailed": status.indexOf("failed") >= 0
            })
        }
        function onScrapeFinished(succeeded, failed, skipped) {
            root.scrapeRunning = false
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: {
            switch (root.screenState) {
                case "login": return 0
                case "hub": return 1
                case "account": return 2
                case "content": return 3
                case "systems": return 4
                case "scrape": return 5
                case "progress": return 6
                default: return 0
            }
        }

        // ── Screen 0: Login ──
        Flickable {
            contentHeight: loginCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: loginCol
                width: parent.width
                spacing: 16

                Text {
                    text: "Scraper"
                    color: Theme.textPrimary
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                }

                Text {
                    text: "Enter your ScreenScraper.fr credentials to download media and metadata for your games."
                    color: Theme.textMuted
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                }

                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 6
                    Text { text: "Username"; color: Theme.textSecondary; font.pixelSize: 13 }
                    TextField {
                        id: loginUserField
                        Layout.preferredWidth: 300
                        placeholderText: "screenscraper.fr username"
                        placeholderTextColor: Theme.textDim
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        background: Rectangle {
                            radius: 6; color: Theme.surface
                            border.width: 1; border.color: loginUserField.activeFocus ? Theme.accent : Theme.divider
                        }
                    }
                }

                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 6
                    Text { text: "Password"; color: Theme.textSecondary; font.pixelSize: 13 }
                    TextField {
                        id: loginPassField
                        Layout.preferredWidth: 300
                        placeholderText: "screenscraper.fr password"
                        placeholderTextColor: Theme.textDim
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        echoMode: TextInput.Password
                        background: Rectangle {
                            radius: 6; color: Theme.surface
                            border.width: 1; border.color: loginPassField.activeFocus ? Theme.accent : Theme.divider
                        }
                    }
                }

                Button {
                    id: signInBtn
                    Layout.leftMargin: 24
                    implicitWidth: 120; implicitHeight: 36
                    background: Rectangle { radius: 6; color: signInBtn.hovered ? Theme.accentLight : Theme.accent }
                    contentItem: Text {
                        text: "Sign In"; color: Theme.textPrimary
                        font.pixelSize: 13; font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        signInBtn.enabled = false
                        loginError.visible = false
                        app.validateScraperCredentials(loginUserField.text, loginPassField.text)
                    }
                }

                Text {
                    id: loginError
                    visible: false
                    color: "#ef4444"
                    font.pixelSize: 12
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Item { height: 24 }
            }
        }

        // ── Screen 1: Hub ──
        Flickable {
            contentHeight: hubCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: hubCol
                width: parent.width
                spacing: 16

                Text {
                    text: "Scraper"
                    color: Theme.textPrimary
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                }

                RowLayout {
                    Layout.leftMargin: 24
                    spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: "#22c55e" }
                    Text {
                        text: "Connected as <b>" + app.scraperUsername() + "</b>"
                        color: "#22c55e"
                        font.pixelSize: 12
                        textFormat: Text.RichText
                    }
                }

                GridLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 12

                    Repeater {
                        model: ListModel {
                            ListElement { name: "Account"; icon: "\u{1F464}"; sub: "Change login details"; target: "account" }
                            ListElement { name: "Content"; icon: "\u{1F4E6}"; sub: "Choose media types"; target: "content" }
                            ListElement { name: "Systems"; icon: "\u{1F3AE}"; sub: "Select consoles"; target: "systems" }
                            ListElement { name: "Start Scraping"; icon: "\u{1F680}"; sub: "Game filter & run"; target: "scrape" }
                        }

                        Rectangle {
                            width: 180; height: 120; radius: 10
                            color: hubCardMa.containsMouse ? Theme.surfaceHover : Theme.surface
                            border.color: model.target === "scrape" ? Theme.accent : (hubCardMa.containsMouse ? Theme.accent : "transparent")
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 100 } }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                Text { text: model.icon; font.pixelSize: 24; Layout.alignment: Qt.AlignHCenter }
                                Text { text: model.name; color: Theme.textPrimary; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.alignment: Qt.AlignHCenter }
                                Text { text: model.sub; color: Theme.textMuted; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
                            }

                            MouseArea {
                                id: hubCardMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (model.target === "content") root.resetMediaSelection()
                                    if (model.target === "systems") root.resetSystemSelection()
                                    root.screenState = model.target
                                }
                            }
                        }
                    }
                }

                Item { height: 24 }
            }
        }

        // ── Screen 2: Account ──
        Flickable {
            contentHeight: accountCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: accountCol
                width: parent.width
                spacing: 16

                RowLayout {
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                    spacing: 12
                    Rectangle {
                        width: 32; height: 32; radius: 8
                        color: accountBackMa.containsMouse ? Theme.surfaceHover : "transparent"
                        Text { anchors.centerIn: parent; text: "\u2190"; color: Theme.textPrimary; font.pixelSize: 18 }
                        MouseArea { id: accountBackMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.screenState = "hub" }
                    }
                    Text { text: "Account"; color: Theme.textPrimary; font.pixelSize: 18; font.weight: Font.Bold }
                }

                ColumnLayout {
                    Layout.leftMargin: 24; Layout.rightMargin: 24; spacing: 6
                    Text { text: "Username"; color: Theme.textSecondary; font.pixelSize: 13 }
                    TextField {
                        id: acctUserField
                        Layout.preferredWidth: 300
                        text: app.scraperUsername()
                        color: Theme.textPrimary; font.pixelSize: 13
                        background: Rectangle { radius: 6; color: Theme.surface; border.width: 1; border.color: acctUserField.activeFocus ? Theme.accent : Theme.divider }
                    }
                }

                ColumnLayout {
                    Layout.leftMargin: 24; Layout.rightMargin: 24; spacing: 6
                    Text { text: "Password"; color: Theme.textSecondary; font.pixelSize: 13 }
                    TextField {
                        id: acctPassField
                        Layout.preferredWidth: 300
                        placeholderText: "Enter new password"
                        placeholderTextColor: Theme.textDim
                        color: Theme.textPrimary; font.pixelSize: 13
                        echoMode: TextInput.Password
                        background: Rectangle { radius: 6; color: Theme.surface; border.width: 1; border.color: acctPassField.activeFocus ? Theme.accent : Theme.divider }
                    }
                }

                RowLayout {
                    Layout.leftMargin: 24; spacing: 10
                    Button {
                        implicitWidth: 160; implicitHeight: 36
                        background: Rectangle { radius: 6; color: parent.parent.children[0].hovered ? Theme.accentLight : Theme.accent }
                        contentItem: Text { text: "Update & Validate"; color: Theme.textPrimary; font.pixelSize: 13; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: app.validateScraperCredentials(acctUserField.text, acctPassField.text)
                    }
                    Button {
                        implicitWidth: 100; implicitHeight: 36
                        background: Rectangle { radius: 6; color: "#333" }
                        contentItem: Text { text: "Sign Out"; color: "#ef4444"; font.pixelSize: 13; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: app.scraperSignOut()
                    }
                }

                Item { height: 24 }
            }
        }

        // ── Screen 3: Content ──
        Flickable {
            contentHeight: contentCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: contentCol
                width: parent.width
                spacing: 16

                RowLayout {
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                    spacing: 12
                    Rectangle {
                        width: 32; height: 32; radius: 8
                        color: contentBackMa.containsMouse ? Theme.surfaceHover : "transparent"
                        Text { anchors.centerIn: parent; text: "\u2190"; color: Theme.textPrimary; font.pixelSize: 18 }
                        MouseArea { id: contentBackMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.screenState = "hub" }
                    }
                    Text { text: "Content Settings"; color: Theme.textPrimary; font.pixelSize: 18; font.weight: Font.Bold }
                }

                Text {
                    text: "Select which media types to download. These selections reset each session."
                    color: Theme.textMuted; font.pixelSize: 12
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                GridLayout {
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    columns: 2; columnSpacing: 8; rowSpacing: 6

                    Repeater {
                        model: ListModel {
                            ListElement { name: "Covers"; type: "covers" }
                            ListElement { name: "Screenshots"; type: "screenshots" }
                            ListElement { name: "Title Screens"; type: "titlescreens" }
                            ListElement { name: "3D Boxes"; type: "3dboxes" }
                            ListElement { name: "Back Covers"; type: "backcovers" }
                            ListElement { name: "Fan Art"; type: "fanart" }
                            ListElement { name: "Marquees"; type: "marquees" }
                            ListElement { name: "Mix Images"; type: "miximages" }
                            ListElement { name: "Physical Media"; type: "physicalmedia" }
                            ListElement { name: "Manuals"; type: "manuals" }
                            ListElement { name: "Videos"; type: "videos" }
                        }

                        Rectangle {
                            Layout.fillWidth: true; height: 36; radius: 6
                            color: Theme.surface

                            RowLayout {
                                anchors.fill: parent; anchors.margins: 8; spacing: 8

                                Rectangle {
                                    width: 16; height: 16; radius: 3
                                    color: root.isMediaSelected(model.type) ? Theme.accent : "transparent"
                                    border.width: root.isMediaSelected(model.type) ? 0 : 1
                                    border.color: Theme.textDim
                                    Text {
                                        anchors.centerIn: parent
                                        text: "\u2713"; color: "white"; font.pixelSize: 10
                                        visible: root.isMediaSelected(model.type)
                                    }
                                }
                                Text { text: model.name; color: Theme.textPrimary; font.pixelSize: 13 }
                            }

                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleMedia(model.type)
                            }
                        }
                    }
                }

                Item { height: 24 }
            }
        }

        // ── Screen 4: Systems ──
        Flickable {
            contentHeight: systemsCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: systemsCol
                width: parent.width
                spacing: 16

                RowLayout {
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                    spacing: 12
                    Rectangle {
                        width: 32; height: 32; radius: 8
                        color: sysBackMa.containsMouse ? Theme.surfaceHover : "transparent"
                        Text { anchors.centerIn: parent; text: "\u2190"; color: Theme.textPrimary; font.pixelSize: 18 }
                        MouseArea { id: sysBackMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.screenState = "hub" }
                    }
                    Text { text: "Systems"; color: Theme.textPrimary; font.pixelSize: 18; font.weight: Font.Bold }
                }

                Text {
                    text: "Select which consoles to scrape. Only systems with imported games are shown."
                    color: Theme.textMuted; font.pixelSize: 12
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                RowLayout {
                    Layout.leftMargin: 24; spacing: 8
                    Text {
                        text: "Select All"; color: Theme.accent; font.pixelSize: 12
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: root.resetSystemSelection()
                        }
                    }
                    Text { text: "|"; color: Theme.textDim; font.pixelSize: 12 }
                    Text {
                        text: "Deselect All"; color: Theme.textMuted; font.pixelSize: 12
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedSystems = []
                        }
                    }
                }

                ColumnLayout {
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    spacing: 4

                    Repeater {
                        model: app.scrapableSystems()

                        Rectangle {
                            Layout.fillWidth: true; height: 42; radius: 6
                            color: Theme.surface

                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 10

                                Rectangle {
                                    width: 16; height: 16; radius: 3
                                    color: root.isSystemSelected(modelData.id) ? Theme.accent : "transparent"
                                    border.width: root.isSystemSelected(modelData.id) ? 0 : 1
                                    border.color: Theme.textDim
                                    Text {
                                        anchors.centerIn: parent
                                        text: "\u2713"; color: "white"; font.pixelSize: 10
                                        visible: root.isSystemSelected(modelData.id)
                                    }
                                }
                                Text {
                                    text: modelData.name + " (" + modelData.count + " games)"
                                    color: root.isSystemSelected(modelData.id) ? Theme.textPrimary : Theme.textMuted
                                    font.pixelSize: 13
                                }
                            }

                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleSystem(modelData.id)
                            }
                        }
                    }
                }

                Item { height: 24 }
            }
        }

        // ── Screen 5: Start Scraping ──
        Flickable {
            contentHeight: scrapeCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: scrapeCol
                width: parent.width
                spacing: 16

                RowLayout {
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                    spacing: 12
                    Rectangle {
                        width: 32; height: 32; radius: 8
                        color: scrapeBackMa.containsMouse ? Theme.surfaceHover : "transparent"
                        Text { anchors.centerIn: parent; text: "\u2190"; color: Theme.textPrimary; font.pixelSize: 18 }
                        MouseArea { id: scrapeBackMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.screenState = "hub" }
                    }
                    Text { text: "Start Scraping"; color: Theme.textPrimary; font.pixelSize: 18; font.weight: Font.Bold }
                }

                Text {
                    text: "Choose which games to scrape, then start."
                    color: Theme.textMuted; font.pixelSize: 12
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                }

                // Game filter radio buttons
                ColumnLayout {
                    Layout.leftMargin: 24; Layout.rightMargin: 24; spacing: 4

                    Text { text: "GAME FILTER"; color: Theme.textMuted; font.pixelSize: 11 }

                    Repeater {
                        model: ListModel {
                            ListElement { label: "All Games"; value: "all" }
                            ListElement { label: "Unscraped Games Only"; value: "unscraped" }
                            ListElement { label: "Favorites Only"; value: "favorites" }
                        }

                        Rectangle {
                            Layout.fillWidth: true; Layout.maximumWidth: 300
                            height: 42; radius: 6
                            color: Theme.surface
                            border.color: root.gameFilter === model.value ? Theme.accent : "transparent"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 10

                                Rectangle {
                                    width: 16; height: 16; radius: 8
                                    border.width: 2
                                    border.color: root.gameFilter === model.value ? Theme.accent : Theme.textDim
                                    color: "transparent"
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 8; height: 8; radius: 4
                                        color: Theme.accent
                                        visible: root.gameFilter === model.value
                                    }
                                }
                                Text {
                                    text: model.label
                                    color: root.gameFilter === model.value ? Theme.textPrimary : Theme.textMuted
                                    font.pixelSize: 13
                                }
                            }

                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: root.gameFilter = model.value
                            }
                        }
                    }
                }

                // Summary
                Rectangle {
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    Layout.fillWidth: true; Layout.maximumWidth: 360
                    height: summaryCol.height + 24; radius: 8
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.3)
                    border.width: 1

                    ColumnLayout {
                        id: summaryCol
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 12
                        spacing: 2
                        Text {
                            text: "Ready to scrape across <b>" + root.selectedSystems.length + " systems</b>"
                            color: Theme.textSecondary; font.pixelSize: 12
                            textFormat: Text.RichText
                        }
                        Text {
                            text: root.selectedMedia.length + " media types selected"
                            color: Theme.textDim; font.pixelSize: 11
                        }
                    }
                }

                Button {
                    Layout.leftMargin: 24
                    implicitWidth: 200; implicitHeight: 40
                    enabled: root.selectedSystems.length > 0 && root.selectedMedia.length > 0
                    background: Rectangle {
                        radius: 8
                        color: parent.enabled ? (parent.hovered ? Theme.accentLight : Theme.accent) : Theme.surface
                    }
                    contentItem: Text {
                        text: "Start Scraping"; color: Theme.textPrimary
                        font.pixelSize: 14; font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        progressLog.clear()
                        root.progressCurrent = 0
                        root.progressTotal = 0
                        root.scrapeRunning = true
                        root.screenState = "progress"
                        app.startBatchScrape(root.selectedMedia, root.selectedSystems, root.gameFilter)
                    }
                }

                Item { height: 24 }
            }
        }

        // ── Screen 6: Progress ──
        Flickable {
            contentHeight: progressCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: progressCol
                width: parent.width
                spacing: 16

                RowLayout {
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                    Text {
                        text: root.scrapeRunning ? "Scraping Games..." : "Scraping Complete"
                        color: Theme.textPrimary; font.pixelSize: 18; font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.progressCurrent + " / " + root.progressTotal
                        color: Theme.textMuted; font.pixelSize: 14
                        visible: root.progressTotal > 0
                    }
                }

                // Progress bar
                Rectangle {
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    Layout.fillWidth: true; height: 6; radius: 3
                    color: Theme.surface

                    Rectangle {
                        width: root.progressTotal > 0 ? parent.width * (root.progressCurrent / root.progressTotal) : 0
                        height: parent.height; radius: 3; color: Theme.accent
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }

                Text {
                    text: root.scrapeRunning ? "Currently: " + root.progressCurrentGame : ""
                    color: Theme.textMuted; font.pixelSize: 12
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    visible: root.scrapeRunning
                }

                // Log
                Rectangle {
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    Layout.fillWidth: true; height: 250; radius: 8
                    color: "#0d0d1a"

                    ListView {
                        id: logView
                        anchors.fill: parent; anchors.margins: 12
                        clip: true; spacing: 3
                        model: progressLog

                        delegate: Text {
                            width: logView.width
                            text: {
                                var prefix = model.isFailed ? "\u2717 " : (model.isPartial ? "\u26A0 " : "\u2713 ")
                                return prefix + model.gameName + " \u2014 " + model.status
                            }
                            color: model.isFailed ? "#ef4444" : (model.isPartial ? "#eab308" : "#22c55e")
                            font.pixelSize: 12; font.family: "monospace"
                            wrapMode: Text.WordWrap
                        }

                        onCountChanged: positionViewAtEnd()
                    }
                }

                RowLayout {
                    Layout.leftMargin: 24; spacing: 10

                    Button {
                        implicitWidth: 120; implicitHeight: 36
                        background: Rectangle { radius: 6; color: "#333" }
                        contentItem: Text {
                            text: root.scrapeRunning ? "Cancel" : "Done"
                            color: root.scrapeRunning ? "#ef4444" : Theme.textPrimary
                            font.pixelSize: 13; font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
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

                Item { height: 24 }
            }
        }
    }
}
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Full successful build.

- [ ] **Step 4: Manual test**

Run `./build/EmulatorFrontend`, press Escape to open settings, click "Scraper". Verify:
- Login page appears (if no saved credentials)
- All sub-pages navigate correctly with back buttons
- Content page shows 11 media type checkboxes
- Systems page shows installed systems with game counts

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/ScraperSettings.qml cpp/qml/AppUI/SettingsOverlay.qml
git commit -m "feat: multi-screen scraper settings UI with batch scraping"
```

---

### Task 9: Final Integration & Cleanup

**Files:**
- Verify: all files from Tasks 1-8
- Modify: `cpp/src/main.cpp` (if needed — update `setCoversDir` → `setMediaDir`)

- [ ] **Step 1: Search for remaining references to old APIs**

Run:
```bash
grep -rn "coversDir\|setCoversDir\|saveScraperCredentials\|scraperPassword\|scrapeAll" cpp/src/ cpp/qml/
```

Fix any remaining references.

- [ ] **Step 2: Search for references in theme files**

Run:
```bash
grep -rn "scrapeAll\|coversDir\|coverPath" themes/
```

`coverPath` references in themes are fine — the role still exists. Verify themes don't call removed methods.

- [ ] **Step 3: Full build**

Run:
```bash
cd cpp && rm -rf build && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Clean build with no warnings about removed methods.

- [ ] **Step 4: Manual end-to-end test**

1. Launch app
2. Open Settings → Scraper → verify login page
3. If credentials work: verify hub, all sub-pages, navigation
4. Right-click a game → Scrape → verify it downloads media
5. Check `downloaded_media/{system}/` directory structure
6. Verify game detail panel still shows cover art

- [ ] **Step 5: Commit any cleanup**

```bash
git add -A
git commit -m "chore: final scraper redesign cleanup"
```
