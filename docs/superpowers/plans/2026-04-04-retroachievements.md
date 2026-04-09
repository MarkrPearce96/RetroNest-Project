# RetroAchievements Integration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable users to log into RetroAchievements, configure emulators to earn achievements, and browse per-game achievement data within the app.

**Architecture:** Credentials stored in JSON, patched into emulator INIs during ensureConfig(). RA Web API client fetches achievement data into SQLite. Background sync on login, app launch, and game exit. Two new QML pages (settings dashboard + per-game achievement list). New "Achievements" action in game popup.

**Tech Stack:** C++17, Qt6 (Network, Sql, Concurrent, QML), SQLite, RetroAchievements Web API

---

### Task 1: RetroAchievements Credentials

**Files:**
- Create: `cpp/src/core/ra_credentials.h`
- Create: `cpp/src/core/ra_credentials.cpp`

- [ ] **Step 1: Create ra_credentials.h**

```cpp
// cpp/src/core/ra_credentials.h
#pragma once

#include <QString>

/**
 * RACredentials — wraps JSON I/O for RetroAchievements credentials.
 * File at {config}/retroachievements.json.
 * Only username + token are persisted (password never stored).
 */
class RACredentials {
public:
    QString username;
    QString token;     // connect token (doubles as web API key)

    /** Load from disk. Returns true if file was read (even if empty). */
    bool load();

    /** Save to disk. Returns true on success. */
    bool save() const;

    /** Clear credentials and remove the JSON file. */
    void clearUser();

    /** True if credentials are set. */
    bool hasCredentials() const { return !username.isEmpty() && !token.isEmpty(); }

private:
    static QString filePath();
};
```

- [ ] **Step 2: Create ra_credentials.cpp**

```cpp
// cpp/src/core/ra_credentials.cpp
#include "ra_credentials.h"
#include "core/paths.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

QString RACredentials::filePath() {
    return Paths::configDir() + "/retroachievements.json";
}

bool RACredentials::load() {
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    username = obj["username"].toString();
    token    = obj["token"].toString();
    return true;
}

bool RACredentials::save() const {
    QJsonObject obj;
    obj["username"] = username;
    obj["token"]    = token;

    QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    f.write(QJsonDocument(obj).toJson());
    f.close();
    return true;
}

void RACredentials::clearUser() {
    username.clear();
    token.clear();
    QFile::remove(filePath());
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `cpp/CMakeLists.txt`, add `src/core/ra_credentials.cpp` to the SOURCES list (after `src/core/rom_scanner.cpp` at line 31) and `src/core/ra_credentials.h` to the HEADERS list (after `src/core/rom_scanner.h` at line 80).

- [ ] **Step 4: Build to verify compilation**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/ra_credentials.h cpp/src/core/ra_credentials.cpp cpp/CMakeLists.txt
git commit -m "feat(ra): add RACredentials for RetroAchievements JSON I/O"
```

---

### Task 2: RA Web API Client

**Files:**
- Create: `cpp/src/core/ra_client.h`
- Create: `cpp/src/core/ra_client.cpp`

- [ ] **Step 1: Create ra_client.h**

```cpp
// cpp/src/core/ra_client.h
#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QAtomicInt>
#include <QMap>

/**
 * RAClient — RetroAchievements Web API client.
 * Handles login, game ID resolution, achievement fetching, and user profile.
 * All methods are synchronous (designed to be called from background threads).
 */
class RAClient : public QObject {
    Q_OBJECT

public:
    explicit RAClient(QObject* parent = nullptr);

    /** Set credentials for API calls. */
    void setCredentials(const QString& username, const QString& token);

    // ── Authentication ──

    struct LoginResult {
        bool success = false;
        QString username;
        QString token;
        QString errorMessage;
        int score = 0;
        int softcoreScore = 0;
    };

    /** Login with username + password. Returns connect token on success. */
    LoginResult login(const QString& username, const QString& password);

    // ── Game ID Resolution ──

    struct GameHashEntry {
        int raGameId;
        QString md5;
    };

    /**
     * Fetch all game hashes for a console.
     * Returns a map of MD5 hash → RA game ID.
     */
    QMap<QString, int> fetchGameHashes(int consoleId, QAtomicInt* cancelFlag = nullptr);

    // ── Achievements ──

    struct Achievement {
        int id = 0;
        QString title;
        QString description;
        int points = 0;
        int trueRatio = 0;
        QString badgeName;
        QString type;       // "core" or other
        bool earned = false;
        bool earnedHardcore = false;
        QString earnedDate;
    };

    struct GameProgress {
        int raGameId = 0;
        QString title;
        int numAchievements = 0;
        int numEarned = 0;
        int numEarnedHardcore = 0;
        QString completionPct;
        QVector<Achievement> achievements;
    };

    /** Fetch achievements and user progress for a game. */
    GameProgress fetchGameProgress(int raGameId, QAtomicInt* cancelFlag = nullptr);

    // ── User Profile ──

    struct UserSummary {
        bool success = false;
        int totalPoints = 0;
        int totalSoftcorePoints = 0;
        int totalTruePoints = 0;
        int rank = 0;
        QString memberSince;
        int recentAchievementCount = 0;
    };

    /** Fetch user profile summary. */
    UserSummary fetchUserSummary(QAtomicInt* cancelFlag = nullptr);

    // ── ROM Hashing ──

    /** Compute MD5 hash of a file. For M3U files, hashes the first disc entry. */
    static QString hashRom(const QString& filePath);

    // ── Console ID Mapping ──

    /** Map EmuFront system ID to RA console ID. Returns -1 if unknown. */
    static int raConsoleId(const QString& systemId);

private:
    QByteArray httpGet(const QString& url, QAtomicInt* cancelFlag = nullptr);
    QJsonDocument httpGetJson(const QString& url, QAtomicInt* cancelFlag = nullptr);

    QString m_username;
    QString m_token;
};
```

- [ ] **Step 2: Create ra_client.cpp**

```cpp
// cpp/src/core/ra_client.cpp
#include "ra_client.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <QThread>

static const char* RA_API_BASE = "https://retroachievements.org/API/";
static const char* RA_CONNECT_BASE = "https://retroachievements.org/dorequest.php";
static const int HTTP_TIMEOUT_MS = 30000;

RAClient::RAClient(QObject* parent) : QObject(parent) {}

void RAClient::setCredentials(const QString& username, const QString& token) {
    m_username = username;
    m_token = token;
}

// ── HTTP Helpers ──

QByteArray RAClient::httpGet(const QString& url, QAtomicInt* cancelFlag) {
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(url));
    req.setHeader(QNetworkRequest::UserAgentHeader, "EmuFront/1.0");

    auto* reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(HTTP_TIMEOUT_MS);

    loop.exec();

    if (cancelFlag && cancelFlag->loadRelaxed()) {
        reply->abort();
        reply->deleteLater();
        return {};
    }

    if (!timer.isActive()) {
        qWarning() << "[RAClient] HTTP timeout:" << url;
        reply->abort();
        reply->deleteLater();
        return {};
    }
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[RAClient] HTTP error:" << reply->errorString();
        reply->deleteLater();
        return {};
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

QJsonDocument RAClient::httpGetJson(const QString& url, QAtomicInt* cancelFlag) {
    QByteArray data = httpGet(url, cancelFlag);
    if (data.isEmpty()) return {};
    return QJsonDocument::fromJson(data);
}

// ── Authentication ──

RAClient::LoginResult RAClient::login(const QString& username, const QString& password) {
    QString url = QString("%1?r=login2&u=%2&p=%3")
        .arg(RA_CONNECT_BASE,
             QUrl::toPercentEncoding(username),
             QUrl::toPercentEncoding(password));

    auto doc = httpGetJson(url);
    if (doc.isNull())
        return {false, {}, {}, "Network error — could not reach RetroAchievements."};

    QJsonObject obj = doc.object();
    if (!obj["Success"].toBool()) {
        QString error = obj["Error"].toString("Invalid username or password.");
        return {false, {}, {}, error};
    }

    LoginResult result;
    result.success = true;
    result.username = obj["User"].toString();
    result.token = obj["Token"].toString();
    result.score = obj["Score"].toInt();
    result.softcoreScore = obj["SoftcoreScore"].toInt();
    return result;
}

// ── Game ID Resolution ──

QMap<QString, int> RAClient::fetchGameHashes(int consoleId, QAtomicInt* cancelFlag) {
    QString url = QString("%1API_GetGameList.php?i=%2&h=1&y=%3")
        .arg(RA_API_BASE).arg(consoleId).arg(m_token);

    auto doc = httpGetJson(url, cancelFlag);
    if (doc.isNull()) return {};

    QMap<QString, int> hashMap;
    QJsonArray games = doc.array();
    for (const auto& gameVal : games) {
        QJsonObject game = gameVal.toObject();
        int gameId = game["ID"].toInt();
        QJsonArray hashes = game["Hashes"].toArray();
        for (const auto& hashVal : hashes) {
            QString md5 = hashVal.toString().toLower();
            if (!md5.isEmpty())
                hashMap[md5] = gameId;
        }
    }

    qInfo() << "[RAClient] Fetched" << hashMap.size() << "hashes for console" << consoleId;
    return hashMap;
}

// ── Achievements ──

RAClient::GameProgress RAClient::fetchGameProgress(int raGameId, QAtomicInt* cancelFlag) {
    QString url = QString("%1API_GetGameInfoAndUserProgress.php?g=%2&u=%3&y=%4&a=1")
        .arg(RA_API_BASE).arg(raGameId).arg(m_username, m_token);

    auto doc = httpGetJson(url, cancelFlag);
    if (doc.isNull()) return {};

    QJsonObject obj = doc.object();

    GameProgress gp;
    gp.raGameId = raGameId;
    gp.title = obj["Title"].toString();
    gp.numAchievements = obj["NumAchievements"].toInt();
    gp.numEarned = obj["NumAwardedToUser"].toInt();
    gp.numEarnedHardcore = obj["NumAwardedToUserHardcore"].toInt();
    gp.completionPct = obj["UserCompletion"].toString();

    QJsonObject achObj = obj["Achievements"].toObject();
    for (auto it = achObj.begin(); it != achObj.end(); ++it) {
        QJsonObject a = it.value().toObject();
        Achievement ach;
        ach.id = a["ID"].toInt();
        ach.title = a["Title"].toString();
        ach.description = a["Description"].toString();
        ach.points = a["Points"].toInt();
        ach.trueRatio = a["TrueRatio"].toInt();
        ach.badgeName = a["BadgeName"].toString();
        ach.type = a["type"].toString();

        QString dateEarned = a["DateEarned"].toString();
        QString dateEarnedHardcore = a["DateEarnedHardcore"].toString();
        ach.earned = !dateEarned.isEmpty();
        ach.earnedHardcore = !dateEarnedHardcore.isEmpty();
        ach.earnedDate = dateEarnedHardcore.isEmpty() ? dateEarned : dateEarnedHardcore;

        gp.achievements.append(ach);
    }

    return gp;
}

// ── User Profile ──

RAClient::UserSummary RAClient::fetchUserSummary(QAtomicInt* cancelFlag) {
    QString url = QString("%1API_GetUserSummary.php?u=%2&y=%3&g=0&a=5")
        .arg(RA_API_BASE, m_username, m_token);

    auto doc = httpGetJson(url, cancelFlag);
    if (doc.isNull()) return {};

    QJsonObject obj = doc.object();

    UserSummary us;
    us.success = true;
    us.totalPoints = obj["TotalPoints"].toInt();
    us.totalSoftcorePoints = obj["TotalSoftcorePoints"].toInt();
    us.totalTruePoints = obj["TotalTruePoints"].toInt();
    us.rank = obj["Rank"].toInt();
    us.memberSince = obj["MemberSince"].toString();

    QJsonObject recent = obj["RecentAchievements"].toObject();
    int count = 0;
    for (auto it = recent.begin(); it != recent.end(); ++it)
        count += it.value().toObject().size();
    us.recentAchievementCount = count;

    return us;
}

// ── ROM Hashing ──

QString RAClient::hashRom(const QString& filePath) {
    // For M3U files, hash the first disc entry
    if (filePath.endsWith(".m3u", Qt::CaseInsensitive)) {
        QFile m3u(filePath);
        if (!m3u.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        QTextStream stream(&m3u);
        QString dir = QFileInfo(filePath).absolutePath();
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            QString discPath = QFileInfo(line).isAbsolute() ? line : dir + "/" + line;
            if (QFileInfo::exists(discPath))
                return hashRom(discPath);  // recurse with the actual disc file
            break;
        }
        return {};
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QCryptographicHash hasher(QCryptographicHash::Md5);
    if (!hasher.addData(&f))
        return {};

    return hasher.result().toHex().toLower();
}

// ── Console ID Mapping ──

int RAClient::raConsoleId(const QString& systemId) {
    static const QMap<QString, int> mapping = {
        {"psx", 12},
        {"ps2", 21},
    };
    return mapping.value(systemId, -1);
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/core/ra_client.cpp` to SOURCES and `src/core/ra_client.h` to HEADERS in `cpp/CMakeLists.txt`.

- [ ] **Step 4: Build to verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/ra_client.h cpp/src/core/ra_client.cpp cpp/CMakeLists.txt
git commit -m "feat(ra): add RAClient for RetroAchievements Web API"
```

---

### Task 3: Database Schema — RA Tables

**Files:**
- Modify: `cpp/src/core/database.h`
- Modify: `cpp/src/core/database.cpp`

- [ ] **Step 1: Add RA structs and methods to database.h**

After the `GameRecord` struct (line 43), add:

```cpp
struct RAGame {
    int game_id = 0;          // FK to games table
    int ra_game_id = 0;       // RA's numeric game ID
    QString rom_hash;          // MD5 hash
    int num_achievements = 0;
    int num_earned = 0;
    int num_earned_hardcore = 0;
    QString completion_pct;
    QString last_synced;
};

struct RAAchievement {
    int ra_achievement_id = 0; // PK — RA's achievement ID
    int ra_game_id = 0;
    QString title;
    QString description;
    int points = 0;
    int true_ratio = 0;
    QString badge_name;
    QString type;              // "core", etc.
    int earned = 0;
    int earned_hardcore = 0;
    QString earned_date;
};
```

Add these public methods to the `Database` class (after `allSystems()` at line 70):

```cpp
    // RetroAchievements
    bool upsertRAGame(const RAGame& ra);
    RAGame raGameByGameId(int gameId);
    RAGame raGameByRAGameId(int raGameId);
    QVector<RAGame> allRAGames();
    bool clearRAData();

    bool upsertRAAchievement(const RAAchievement& ach);
    bool replaceAchievementsForGame(int raGameId, const QVector<RAAchievement>& achievements);
    QVector<RAAchievement> achievementsForGame(int raGameId);

    bool hasRAAchievements(int gameId);
```

- [ ] **Step 2: Bump schema version and add v4→v5 migration**

In `database.cpp`, change `CURRENT_SCHEMA_VERSION` from `4` to `5` (line 41).

After the `if (current < 4)` block (line 208), add the v4→v5 migration:

```cpp
    if (current < 5) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        if (!db.transaction()) {
            qCritical() << "[Database] Failed to begin transaction for v4→v5 migration";
            return false;
        }
        QSqlQuery q(db);
        const QStringList statements = {
            "CREATE TABLE IF NOT EXISTS ra_games ("
            "  game_id INTEGER PRIMARY KEY,"
            "  ra_game_id INTEGER NOT NULL,"
            "  rom_hash TEXT NOT NULL DEFAULT '',"
            "  num_achievements INTEGER NOT NULL DEFAULT 0,"
            "  num_earned INTEGER NOT NULL DEFAULT 0,"
            "  num_earned_hardcore INTEGER NOT NULL DEFAULT 0,"
            "  completion_pct TEXT NOT NULL DEFAULT '',"
            "  last_synced TEXT NOT NULL DEFAULT ''"
            ")",
            "CREATE INDEX IF NOT EXISTS idx_ra_games_ra_id ON ra_games(ra_game_id)",
            "CREATE TABLE IF NOT EXISTS ra_achievements ("
            "  ra_achievement_id INTEGER PRIMARY KEY,"
            "  ra_game_id INTEGER NOT NULL,"
            "  title TEXT NOT NULL DEFAULT '',"
            "  description TEXT NOT NULL DEFAULT '',"
            "  points INTEGER NOT NULL DEFAULT 0,"
            "  true_ratio INTEGER NOT NULL DEFAULT 0,"
            "  badge_name TEXT NOT NULL DEFAULT '',"
            "  type TEXT NOT NULL DEFAULT '',"
            "  earned INTEGER NOT NULL DEFAULT 0,"
            "  earned_hardcore INTEGER NOT NULL DEFAULT 0,"
            "  earned_date TEXT NOT NULL DEFAULT ''"
            ")",
            "CREATE INDEX IF NOT EXISTS idx_ra_ach_game ON ra_achievements(ra_game_id)",
        };
        for (const auto& sql : statements) {
            if (!q.exec(sql)) {
                qCritical() << "[Database] Migration v4→v5 failed:" << q.lastError().text() << "SQL:" << sql;
                db.rollback();
                return false;
            }
        }
        if (!db.commit()) {
            qCritical() << "[Database] Failed to commit v4→v5 migration";
            db.rollback();
            return false;
        }
        qInfo() << "[Database] Migrated schema v4 → v5 (added RetroAchievements tables)";
    }
```

- [ ] **Step 3: Implement RA database methods**

Add at the end of `database.cpp`, before the closing:

```cpp
// ── RetroAchievements ──────────────────────────────────────────

bool Database::upsertRAGame(const RAGame& ra) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO ra_games (game_id, ra_game_id, rom_hash, num_achievements, "
        "num_earned, num_earned_hardcore, completion_pct, last_synced) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(game_id) DO UPDATE SET "
        "ra_game_id=excluded.ra_game_id, rom_hash=excluded.rom_hash, "
        "num_achievements=excluded.num_achievements, num_earned=excluded.num_earned, "
        "num_earned_hardcore=excluded.num_earned_hardcore, "
        "completion_pct=excluded.completion_pct, last_synced=excluded.last_synced");
    q.addBindValue(ra.game_id);
    q.addBindValue(ra.ra_game_id);
    q.addBindValue(ra.rom_hash);
    q.addBindValue(ra.num_achievements);
    q.addBindValue(ra.num_earned);
    q.addBindValue(ra.num_earned_hardcore);
    q.addBindValue(ra.completion_pct);
    q.addBindValue(ra.last_synced);
    if (!q.exec()) {
        qWarning() << "[Database] upsertRAGame failed:" << q.lastError().text();
        return false;
    }
    return true;
}

Database::RAGame Database::raGameByGameId(int gameId) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("SELECT * FROM ra_games WHERE game_id = ?");
    q.addBindValue(gameId);
    if (!q.exec() || !q.next()) return {};
    RAGame ra;
    ra.game_id = q.value("game_id").toInt();
    ra.ra_game_id = q.value("ra_game_id").toInt();
    ra.rom_hash = q.value("rom_hash").toString();
    ra.num_achievements = q.value("num_achievements").toInt();
    ra.num_earned = q.value("num_earned").toInt();
    ra.num_earned_hardcore = q.value("num_earned_hardcore").toInt();
    ra.completion_pct = q.value("completion_pct").toString();
    ra.last_synced = q.value("last_synced").toString();
    return ra;
}

Database::RAGame Database::raGameByRAGameId(int raGameId) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("SELECT * FROM ra_games WHERE ra_game_id = ?");
    q.addBindValue(raGameId);
    if (!q.exec() || !q.next()) return {};
    RAGame ra;
    ra.game_id = q.value("game_id").toInt();
    ra.ra_game_id = q.value("ra_game_id").toInt();
    ra.rom_hash = q.value("rom_hash").toString();
    ra.num_achievements = q.value("num_achievements").toInt();
    ra.num_earned = q.value("num_earned").toInt();
    ra.num_earned_hardcore = q.value("num_earned_hardcore").toInt();
    ra.completion_pct = q.value("completion_pct").toString();
    ra.last_synced = q.value("last_synced").toString();
    return ra;
}

QVector<Database::RAGame> Database::allRAGames() {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    QVector<RAGame> result;
    if (!q.exec("SELECT * FROM ra_games")) return result;
    while (q.next()) {
        RAGame ra;
        ra.game_id = q.value("game_id").toInt();
        ra.ra_game_id = q.value("ra_game_id").toInt();
        ra.rom_hash = q.value("rom_hash").toString();
        ra.num_achievements = q.value("num_achievements").toInt();
        ra.num_earned = q.value("num_earned").toInt();
        ra.num_earned_hardcore = q.value("num_earned_hardcore").toInt();
        ra.completion_pct = q.value("completion_pct").toString();
        ra.last_synced = q.value("last_synced").toString();
        result.append(ra);
    }
    return result;
}

bool Database::clearRAData() {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    if (!q.exec("DELETE FROM ra_achievements")) {
        qWarning() << "[Database] clearRAData achievements:" << q.lastError().text();
        return false;
    }
    if (!q.exec("DELETE FROM ra_games")) {
        qWarning() << "[Database] clearRAData games:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::upsertRAAchievement(const RAAchievement& ach) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO ra_achievements (ra_achievement_id, ra_game_id, title, description, "
        "points, true_ratio, badge_name, type, earned, earned_hardcore, earned_date) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(ra_achievement_id) DO UPDATE SET "
        "title=excluded.title, description=excluded.description, "
        "points=excluded.points, true_ratio=excluded.true_ratio, "
        "badge_name=excluded.badge_name, type=excluded.type, "
        "earned=excluded.earned, earned_hardcore=excluded.earned_hardcore, "
        "earned_date=excluded.earned_date");
    q.addBindValue(ach.ra_achievement_id);
    q.addBindValue(ach.ra_game_id);
    q.addBindValue(ach.title);
    q.addBindValue(ach.description);
    q.addBindValue(ach.points);
    q.addBindValue(ach.true_ratio);
    q.addBindValue(ach.badge_name);
    q.addBindValue(ach.type);
    q.addBindValue(ach.earned);
    q.addBindValue(ach.earned_hardcore);
    q.addBindValue(ach.earned_date);
    if (!q.exec()) {
        qWarning() << "[Database] upsertRAAchievement failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::replaceAchievementsForGame(int raGameId, const QVector<RAAchievement>& achievements) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    if (!db.transaction()) return false;

    QSqlQuery q(db);
    q.prepare("DELETE FROM ra_achievements WHERE ra_game_id = ?");
    q.addBindValue(raGameId);
    if (!q.exec()) { db.rollback(); return false; }

    for (const auto& ach : achievements) {
        if (!upsertRAAchievement(ach)) { db.rollback(); return false; }
    }

    return db.commit();
}

QVector<Database::RAAchievement> Database::achievementsForGame(int raGameId) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("SELECT * FROM ra_achievements WHERE ra_game_id = ? "
              "ORDER BY earned DESC, earned_date DESC, ra_achievement_id ASC");
    q.addBindValue(raGameId);
    QVector<RAAchievement> result;
    if (!q.exec()) return result;
    while (q.next()) {
        RAAchievement ach;
        ach.ra_achievement_id = q.value("ra_achievement_id").toInt();
        ach.ra_game_id = q.value("ra_game_id").toInt();
        ach.title = q.value("title").toString();
        ach.description = q.value("description").toString();
        ach.points = q.value("points").toInt();
        ach.true_ratio = q.value("true_ratio").toInt();
        ach.badge_name = q.value("badge_name").toString();
        ach.type = q.value("type").toString();
        ach.earned = q.value("earned").toInt();
        ach.earned_hardcore = q.value("earned_hardcore").toInt();
        ach.earned_date = q.value("earned_date").toString();
        result.append(ach);
    }
    return result;
}

bool Database::hasRAAchievements(int gameId) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM ra_games WHERE game_id = ? AND num_achievements > 0 LIMIT 1");
    q.addBindValue(gameId);
    return q.exec() && q.next();
}
```

- [ ] **Step 4: Build to verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/database.h cpp/src/core/database.cpp
git commit -m "feat(ra): add RA database tables and v4→v5 migration"
```

---

### Task 4: RetroAchievements Service

**Files:**
- Create: `cpp/src/services/ra_service.h`
- Create: `cpp/src/services/ra_service.cpp`

- [ ] **Step 1: Create ra_service.h**

```cpp
// cpp/src/services/ra_service.h
#pragma once

#include "core/database.h"
#include "core/ra_client.h"
#include "core/ra_credentials.h"
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QAtomicInt>

/**
 * RAService — orchestrates RetroAchievements login, sync, and data access.
 * Exposes signals for QML UI binding.
 */
class RAService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool syncing READ isSyncing NOTIFY syncingChanged)

public:
    RAService(Database* db, QObject* parent = nullptr);

    /** Load credentials from disk. */
    void loadCredentials();

    /** Login with username + password. Emits loginCompleted. */
    void login(const QString& username, const QString& password);

    /** Sign out and clear credentials + cached RA data. */
    void signOut();

    /** True if user is logged in. */
    bool hasCredentials() const { return m_creds.hasCredentials(); }

    /** Current username. */
    QString username() const { return m_creds.username; }

    /** True if sync is in progress. */
    bool isSyncing() const { return m_syncing; }

    // ── Sync ──

    /** Full sync: hash ROMs, resolve game IDs, fetch progress, download badges. */
    void startFullSync();

    /** Incremental sync: re-fetch progress for all matched games. */
    void startIncrementalSync();

    /** Targeted sync: re-fetch progress for one game by local game ID. */
    void startGameSync(int gameId);

    /** Cancel running sync. */
    void cancelSync();

    // ── Data Access (for QML) ──

    /** User profile summary as QVariantMap. */
    QVariantMap userSummary();

    /** All RA-matched games with progress, as QVariantList of QVariantMap. */
    QVariantList gameProgressList();

    /** Achievements for a specific RA game ID, as QVariantList of QVariantMap. */
    QVariantList achievementsForGame(int raGameId);

    /** Recent unlocks across all games (up to count). */
    QVariantList recentUnlocks(int count = 5);

    /** Check if a game has RA achievements. */
    bool hasAchievements(int gameId) { return m_db->hasRAAchievements(gameId); }

    /** Get the RA game ID for a local game ID (0 if not matched). */
    int raGameId(int gameId);

    // ── Settings ──

    /** Get/set hardcore mode preference. */
    bool hardcoreMode() const;
    void setHardcoreMode(bool enabled);

    bool notifications() const;
    void setNotifications(bool enabled);

    bool soundEffects() const;
    void setSoundEffects(bool enabled);

signals:
    void loginCompleted(bool success, const QString& message);
    void signedOut();
    void syncingChanged();
    void syncCompleted();
    void gameSyncCompleted(int gameId);
    void syncProgress(const QString& message);

private:
    void runFullSync();
    void runIncrementalSync();
    void runGameSync(int gameId);
    void downloadBadges(const QVector<QString>& badgeNames, QAtomicInt* cancelFlag);
    QString badgePath(const QString& badgeName) const;

    Database* m_db;
    RAClient* m_client;
    RACredentials m_creds;
    QAtomicInt m_cancelFlag;
    bool m_syncing = false;

    // Settings stored in retroachievements.json alongside credentials
    bool m_hardcoreMode = false;
    bool m_notifications = true;
    bool m_soundEffects = true;

    void loadSettings();
    void saveSettings() const;
};
```

- [ ] **Step 2: Create ra_service.cpp**

```cpp
// cpp/src/services/ra_service.cpp
#include "ra_service.h"
#include "core/paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QThread>
#include <QtConcurrent>
#include <QDebug>

RAService::RAService(Database* db, QObject* parent)
    : QObject(parent), m_db(db), m_client(new RAClient(this))
{
    m_cancelFlag.storeRelaxed(0);
}

void RAService::loadCredentials() {
    m_creds.load();
    if (m_creds.hasCredentials())
        m_client->setCredentials(m_creds.username, m_creds.token);
    loadSettings();
}

void RAService::login(const QString& username, const QString& password) {
    QtConcurrent::run([this, username, password]() {
        auto result = m_client->login(username, password);
        QMetaObject::invokeMethod(this, [this, result]() {
            if (result.success) {
                m_creds.username = result.username;
                m_creds.token = result.token;
                m_creds.save();
                m_client->setCredentials(m_creds.username, m_creds.token);
                saveSettings();
                emit loginCompleted(true, "Logged in as " + result.username);
                startFullSync();
            } else {
                emit loginCompleted(false, result.errorMessage);
            }
        });
    });
}

void RAService::signOut() {
    cancelSync();
    m_creds.clearUser();
    m_client->setCredentials("", "");
    m_db->clearRAData();
    emit signedOut();
}

// ── Sync ──

void RAService::startFullSync() {
    if (m_syncing) return;
    m_cancelFlag.storeRelaxed(0);
    m_syncing = true;
    emit syncingChanged();

    // Gather games on main thread (DB is thread-bound)
    QVector<GameRecord> allGames = m_db->allGames();

    QtConcurrent::run([this, allGames]() {
        runFullSync();
        QMetaObject::invokeMethod(this, [this]() {
            m_syncing = false;
            emit syncingChanged();
            emit syncCompleted();
        });
    });
}

void RAService::startIncrementalSync() {
    if (m_syncing || !m_creds.hasCredentials()) return;
    m_cancelFlag.storeRelaxed(0);
    m_syncing = true;
    emit syncingChanged();

    QtConcurrent::run([this]() {
        runIncrementalSync();
        QMetaObject::invokeMethod(this, [this]() {
            m_syncing = false;
            emit syncingChanged();
            emit syncCompleted();
        });
    });
}

void RAService::startGameSync(int gameId) {
    if (!m_creds.hasCredentials()) return;
    m_cancelFlag.storeRelaxed(0);

    QtConcurrent::run([this, gameId]() {
        runGameSync(gameId);
        QMetaObject::invokeMethod(this, [this, gameId]() {
            emit gameSyncCompleted(gameId);
        });
    });
}

void RAService::cancelSync() {
    m_cancelFlag.storeRelaxed(1);
}

// ── Sync Implementation ──

void RAService::runFullSync() {
    if (m_cancelFlag.loadRelaxed()) return;

    QMetaObject::invokeMethod(this, [this]() {
        emit syncProgress("Fetching game lists...");
    });

    // Step 1: Fetch all game hashes per console
    QMap<QString, int> hashToRaId;
    QStringList systems = m_db->allSystems();
    for (const auto& sys : systems) {
        int consoleId = RAClient::raConsoleId(sys);
        if (consoleId < 0) continue;
        auto hashes = m_client->fetchGameHashes(consoleId, &m_cancelFlag);
        for (auto it = hashes.begin(); it != hashes.end(); ++it)
            hashToRaId[it.key()] = it.value();
        if (m_cancelFlag.loadRelaxed()) return;
        QThread::msleep(100);  // rate limiting
    }

    QMetaObject::invokeMethod(this, [this]() {
        emit syncProgress("Matching ROMs...");
    });

    // Step 2-3: Hash ROMs and match to RA game IDs
    QVector<GameRecord> allGames = m_db->allGames();
    QVector<QPair<int, int>> matched;  // game_id, ra_game_id

    for (const auto& game : allGames) {
        if (m_cancelFlag.loadRelaxed()) return;

        // Check if already matched
        RAGame existing = m_db->raGameByGameId(game.id);
        if (existing.ra_game_id > 0) {
            matched.append({game.id, existing.ra_game_id});
            continue;
        }

        QString hash = RAClient::hashRom(game.rom_path);
        if (hash.isEmpty()) continue;

        int raId = hashToRaId.value(hash, 0);
        if (raId > 0) {
            RAGame ra;
            ra.game_id = game.id;
            ra.ra_game_id = raId;
            ra.rom_hash = hash;
            m_db->upsertRAGame(ra);
            matched.append({game.id, raId});
        }
    }

    // Step 4: Fetch progress for each matched game
    QVector<QString> badgeNames;
    int count = 0;
    for (const auto& [gameId, raId] : matched) {
        if (m_cancelFlag.loadRelaxed()) return;

        count++;
        QMetaObject::invokeMethod(this, [this, count, total = matched.size()]() {
            emit syncProgress(QString("Syncing achievements... %1/%2").arg(count).arg(total));
        });

        auto gp = m_client->fetchGameProgress(raId, &m_cancelFlag);
        if (gp.raGameId == 0) continue;

        // Update ra_games
        RAGame ra;
        ra.game_id = gameId;
        ra.ra_game_id = raId;
        ra.rom_hash = m_db->raGameByGameId(gameId).rom_hash;
        ra.num_achievements = gp.numAchievements;
        ra.num_earned = gp.numEarned;
        ra.num_earned_hardcore = gp.numEarnedHardcore;
        ra.completion_pct = gp.completionPct;
        ra.last_synced = QDateTime::currentDateTime().toString(Qt::ISODate);
        m_db->upsertRAGame(ra);

        // Update ra_achievements
        QVector<RAAchievement> dbAchs;
        for (const auto& ach : gp.achievements) {
            RAAchievement dbAch;
            dbAch.ra_achievement_id = ach.id;
            dbAch.ra_game_id = raId;
            dbAch.title = ach.title;
            dbAch.description = ach.description;
            dbAch.points = ach.points;
            dbAch.true_ratio = ach.trueRatio;
            dbAch.badge_name = ach.badgeName;
            dbAch.type = ach.type;
            dbAch.earned = ach.earned ? 1 : 0;
            dbAch.earned_hardcore = ach.earnedHardcore ? 1 : 0;
            dbAch.earned_date = ach.earnedDate;
            dbAchs.append(dbAch);

            if (!ach.badgeName.isEmpty())
                badgeNames.append(ach.badgeName);
        }
        m_db->replaceAchievementsForGame(raId, dbAchs);

        QThread::msleep(100);  // rate limiting
    }

    // Step 5: Download badges
    if (!badgeNames.isEmpty()) {
        QMetaObject::invokeMethod(this, [this]() {
            emit syncProgress("Downloading badge images...");
        });
        downloadBadges(badgeNames, &m_cancelFlag);
    }
}

void RAService::runIncrementalSync() {
    QVector<RAGame> raGames = m_db->allRAGames();
    QVector<QString> badgeNames;
    int count = 0;

    for (const auto& ra : raGames) {
        if (m_cancelFlag.loadRelaxed()) return;

        count++;
        QMetaObject::invokeMethod(this, [this, count, total = raGames.size()]() {
            emit syncProgress(QString("Syncing %1/%2...").arg(count).arg(total));
        });

        auto gp = m_client->fetchGameProgress(ra.ra_game_id, &m_cancelFlag);
        if (gp.raGameId == 0) continue;

        RAGame updated = ra;
        updated.num_achievements = gp.numAchievements;
        updated.num_earned = gp.numEarned;
        updated.num_earned_hardcore = gp.numEarnedHardcore;
        updated.completion_pct = gp.completionPct;
        updated.last_synced = QDateTime::currentDateTime().toString(Qt::ISODate);
        m_db->upsertRAGame(updated);

        QVector<RAAchievement> dbAchs;
        for (const auto& ach : gp.achievements) {
            RAAchievement dbAch;
            dbAch.ra_achievement_id = ach.id;
            dbAch.ra_game_id = ra.ra_game_id;
            dbAch.title = ach.title;
            dbAch.description = ach.description;
            dbAch.points = ach.points;
            dbAch.true_ratio = ach.trueRatio;
            dbAch.badge_name = ach.badgeName;
            dbAch.type = ach.type;
            dbAch.earned = ach.earned ? 1 : 0;
            dbAch.earned_hardcore = ach.earnedHardcore ? 1 : 0;
            dbAch.earned_date = ach.earnedDate;
            dbAchs.append(dbAch);

            if (!ach.badgeName.isEmpty())
                badgeNames.append(ach.badgeName);
        }
        m_db->replaceAchievementsForGame(ra.ra_game_id, dbAchs);

        QThread::msleep(100);
    }

    if (!badgeNames.isEmpty())
        downloadBadges(badgeNames, &m_cancelFlag);
}

void RAService::runGameSync(int gameId) {
    RAGame ra = m_db->raGameByGameId(gameId);
    if (ra.ra_game_id == 0) return;

    auto gp = m_client->fetchGameProgress(ra.ra_game_id, &m_cancelFlag);
    if (gp.raGameId == 0) return;

    ra.num_achievements = gp.numAchievements;
    ra.num_earned = gp.numEarned;
    ra.num_earned_hardcore = gp.numEarnedHardcore;
    ra.completion_pct = gp.completionPct;
    ra.last_synced = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_db->upsertRAGame(ra);

    QVector<RAAchievement> dbAchs;
    QVector<QString> badgeNames;
    for (const auto& ach : gp.achievements) {
        RAAchievement dbAch;
        dbAch.ra_achievement_id = ach.id;
        dbAch.ra_game_id = ra.ra_game_id;
        dbAch.title = ach.title;
        dbAch.description = ach.description;
        dbAch.points = ach.points;
        dbAch.true_ratio = ach.trueRatio;
        dbAch.badge_name = ach.badgeName;
        dbAch.type = ach.type;
        dbAch.earned = ach.earned ? 1 : 0;
        dbAch.earned_hardcore = ach.earnedHardcore ? 1 : 0;
        dbAch.earned_date = ach.earnedDate;
        dbAchs.append(dbAch);

        if (!ach.badgeName.isEmpty())
            badgeNames.append(ach.badgeName);
    }
    m_db->replaceAchievementsForGame(ra.ra_game_id, dbAchs);

    if (!badgeNames.isEmpty())
        downloadBadges(badgeNames, &m_cancelFlag);
}

// ── Badge Downloads ──

QString RAService::badgePath(const QString& badgeName) const {
    return Paths::mediaDir() + "/ra_badges/" + badgeName + ".png";
}

void RAService::downloadBadges(const QVector<QString>& badgeNames, QAtomicInt* cancelFlag) {
    QString dir = Paths::mediaDir() + "/ra_badges";
    QDir().mkpath(dir);

    for (const auto& name : badgeNames) {
        if (cancelFlag && cancelFlag->loadRelaxed()) return;

        QString path = badgePath(name);
        if (QFileInfo::exists(path)) continue;  // already cached

        QString url = "https://media.retroachievements.org/Badge/" + name + ".png";
        QByteArray data = m_client->httpGet(url, cancelFlag);
        if (data.isEmpty()) continue;

        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        }
    }
}

// ── Data Access ──

QVariantMap RAService::userSummary() {
    if (!m_creds.hasCredentials()) return {};

    auto us = m_client->fetchUserSummary();
    QVariantMap map;
    map["username"] = m_creds.username;
    map["totalPoints"] = us.totalPoints;
    map["totalSoftcorePoints"] = us.totalSoftcorePoints;
    map["totalTruePoints"] = us.totalTruePoints;
    map["rank"] = us.rank;
    map["memberSince"] = us.memberSince;

    // Compute derived stats from DB
    auto raGames = m_db->allRAGames();
    int gamesStarted = 0, gamesMastered = 0, totalAch = 0, totalEarned = 0;
    for (const auto& ra : raGames) {
        if (ra.num_earned > 0) gamesStarted++;
        if (ra.num_achievements > 0 && ra.num_earned >= ra.num_achievements)
            gamesMastered++;
        totalAch += ra.num_achievements;
        totalEarned += ra.num_earned;
    }
    map["gamesStarted"] = gamesStarted;
    map["gamesMastered"] = gamesMastered;
    map["totalAchievements"] = totalEarned;
    map["avgCompletion"] = totalAch > 0
        ? QString::number(totalEarned * 100 / totalAch) + "%"
        : "0%";
    map["hardcorePoints"] = us.totalPoints;
    map["softcorePoints"] = us.totalSoftcorePoints;

    return map;
}

QVariantList RAService::gameProgressList() {
    QVariantList list;
    auto raGames = m_db->allRAGames();
    for (const auto& ra : raGames) {
        if (ra.num_achievements == 0) continue;
        auto game = m_db->gameById(ra.game_id);
        QVariantMap map;
        map["gameId"] = ra.game_id;
        map["raGameId"] = ra.ra_game_id;
        map["title"] = game.title;
        map["system"] = game.system;
        map["numAchievements"] = ra.num_achievements;
        map["numEarned"] = ra.num_earned;
        map["numEarnedHardcore"] = ra.num_earned_hardcore;
        map["completionPct"] = ra.completion_pct;
        map["mastered"] = (ra.num_earned >= ra.num_achievements);
        list.append(map);
    }
    return list;
}

QVariantList RAService::achievementsForGame(int raGameId) {
    QVariantList list;
    auto achs = m_db->achievementsForGame(raGameId);
    for (const auto& ach : achs) {
        QVariantMap map;
        map["id"] = ach.ra_achievement_id;
        map["title"] = ach.title;
        map["description"] = ach.description;
        map["points"] = ach.points;
        map["trueRatio"] = ach.true_ratio;
        map["badgePath"] = badgePath(ach.badge_name);
        map["type"] = ach.type;
        map["earned"] = (ach.earned != 0);
        map["earnedHardcore"] = (ach.earned_hardcore != 0);
        map["earnedDate"] = ach.earned_date;
        list.append(map);
    }
    return list;
}

QVariantList RAService::recentUnlocks(int count) {
    QVariantList list;
    // Query all earned achievements, sorted by date descending
    auto db = QSqlDatabase::database("emulator_frontend");
    QSqlQuery q(db);
    q.prepare(
        "SELECT a.*, g.game_id FROM ra_achievements a "
        "JOIN ra_games g ON a.ra_game_id = g.ra_game_id "
        "WHERE a.earned = 1 OR a.earned_hardcore = 1 "
        "ORDER BY a.earned_date DESC LIMIT ?");
    q.addBindValue(count);
    if (!q.exec()) return list;

    while (q.next()) {
        int localGameId = q.value("game_id").toInt();
        auto game = m_db->gameById(localGameId);
        QVariantMap map;
        map["title"] = q.value("title").toString();
        map["gameTitle"] = game.title;
        map["badgePath"] = badgePath(q.value("badge_name").toString());
        map["earnedDate"] = q.value("earned_date").toString();
        map["points"] = q.value("points").toInt();
        list.append(map);
    }
    return list;
}

int RAService::raGameId(int gameId) {
    auto ra = m_db->raGameByGameId(gameId);
    return ra.ra_game_id;
}

// ── Settings ──

bool RAService::hardcoreMode() const { return m_hardcoreMode; }
void RAService::setHardcoreMode(bool enabled) {
    m_hardcoreMode = enabled;
    saveSettings();
}

bool RAService::notifications() const { return m_notifications; }
void RAService::setNotifications(bool enabled) {
    m_notifications = enabled;
    saveSettings();
}

bool RAService::soundEffects() const { return m_soundEffects; }
void RAService::setSoundEffects(bool enabled) {
    m_soundEffects = enabled;
    saveSettings();
}

void RAService::loadSettings() {
    QString path = Paths::configDir() + "/retroachievements.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    m_hardcoreMode = obj["hardcoreMode"].toBool(false);
    m_notifications = obj["notifications"].toBool(true);
    m_soundEffects = obj["soundEffects"].toBool(true);
}

void RAService::saveSettings() const {
    // Read existing JSON and merge settings
    QString path = Paths::configDir() + "/retroachievements.json";
    QJsonObject obj;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
    }
    obj["hardcoreMode"] = m_hardcoreMode;
    obj["notifications"] = m_notifications;
    obj["soundEffects"] = m_soundEffects;

    QDir().mkpath(QFileInfo(path).absolutePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson());
        f.close();
    }
}
```

- [ ] **Step 3: Make httpGet public on RAClient**

In `cpp/src/core/ra_client.h`, move `httpGet` from `private:` to `public:` (the service needs it for badge downloads):

Change line (in the private section):
```cpp
    QByteArray httpGet(const QString& url, QAtomicInt* cancelFlag = nullptr);
```
Move it to the public section (after `static int raConsoleId`).

- [ ] **Step 4: Add to CMakeLists.txt**

Add `src/services/ra_service.cpp` to SOURCES and `src/services/ra_service.h` to HEADERS.

- [ ] **Step 5: Build to verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/services/ra_service.h cpp/src/services/ra_service.cpp \
       cpp/src/core/ra_client.h cpp/CMakeLists.txt
git commit -m "feat(ra): add RAService for sync orchestration and data access"
```

---

### Task 5: Adapter INI Patching for RetroAchievements

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h`
- Modify: `cpp/src/adapters/duckstation_adapter.h`
- Modify: `cpp/src/adapters/duckstation_adapter.cpp`
- Modify: `cpp/src/adapters/pcsx2_adapter.h`
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp`

- [ ] **Step 1: Add virtual method to base adapter**

In `cpp/src/adapters/emulator_adapter.h`, after the `resumeLaunchArgs` method (line 210), add:

```cpp
    /**
     * Whether this emulator has built-in RetroAchievements support.
     */
    virtual bool supportsRetroAchievements() const { return false; }

    /**
     * Patch RA credentials and settings into the emulator's config.
     * Called from ensureConfig() when RA credentials are available.
     */
    virtual void patchRetroAchievements(const QString& username, const QString& token,
                                         bool hardcore, bool notifications, bool sounds) {
        Q_UNUSED(username); Q_UNUSED(token);
        Q_UNUSED(hardcore); Q_UNUSED(notifications); Q_UNUSED(sounds);
    }
```

- [ ] **Step 2: Add DuckStation RA support**

In `cpp/src/adapters/duckstation_adapter.h`, add to the public section:

```cpp
    bool supportsRetroAchievements() const override { return true; }
    void patchRetroAchievements(const QString& username, const QString& token,
                                 bool hardcore, bool notifications, bool sounds) override;
```

In `cpp/src/adapters/duckstation_adapter.cpp`, add at the end (before the closing), after the last method:

```cpp
// ============================================================================
// patchRetroAchievements — inject RA credentials into [Cheevos] section
// ============================================================================

void DuckStationAdapter::patchRetroAchievements(const QString& username,
                                                  const QString& token,
                                                  bool hardcore,
                                                  bool notifications,
                                                  bool sounds) {
    const QString path = portableDir() + "/settings.ini";

    QString content;
    if (!readConfigFile(path, content, "DuckStation"))
        return;

    QVector<IniKeyPatch> patches = {
        {"Cheevos", "Enabled", "true"},
        {"Cheevos", "Username", username},
        {"Cheevos", "Token", token},
        {"Cheevos", "ChallengeMode", hardcore ? "true" : "false"},
        {"Cheevos", "Notifications", notifications ? "true" : "false"},
        {"Cheevos", "SoundEffects", sounds ? "true" : "false"},
    };

    if (patchIniKeys(content, patches))
        writeConfigFile(path, content, "DuckStation");
}
```

- [ ] **Step 3: Add PCSX2 RA support**

In `cpp/src/adapters/pcsx2_adapter.h`, add to the public section:

```cpp
    bool supportsRetroAchievements() const override { return true; }
    void patchRetroAchievements(const QString& username, const QString& token,
                                 bool hardcore, bool notifications, bool sounds) override;
```

In `cpp/src/adapters/pcsx2_adapter.cpp`, add at the end:

```cpp
// ============================================================================
// patchRetroAchievements — inject RA settings into PCSX2.ini + secrets.ini
// ============================================================================

void PCSX2Adapter::patchRetroAchievements(const QString& username,
                                            const QString& token,
                                            bool hardcore,
                                            bool notifications,
                                            bool sounds) {
    // Patch main INI (PCSX2.ini) — settings only, no credentials
    const QString mainPath = configFilePath();
    QString mainContent;
    if (readConfigFile(mainPath, mainContent, "PCSX2")) {
        QVector<IniKeyPatch> mainPatches = {
            {"Achievements", "Enabled", "true"},
            {"Achievements", "HardcoreMode", hardcore ? "true" : "false"},
            {"Achievements", "Notifications", notifications ? "true" : "false"},
            {"Achievements", "SoundEffects", sounds ? "true" : "false"},
        };
        if (patchIniKeys(mainContent, mainPatches))
            writeConfigFile(mainPath, mainContent, "PCSX2");
    }

    // Patch secrets.ini — credentials only
    const QString secretsPath = QFileInfo(mainPath).absolutePath() + "/secrets.ini";

    QString secretsContent;
    if (QFileInfo::exists(secretsPath)) {
        readConfigFile(secretsPath, secretsContent, "PCSX2-secrets");
    }

    // Ensure [Achievements] section exists
    if (!secretsContent.contains("[Achievements]"))
        secretsContent.append("\n[Achievements]\n");

    QVector<IniKeyPatch> secretsPatches = {
        {"Achievements", "Username", username},
        {"Achievements", "Token", token},
    };

    if (patchIniKeys(secretsContent, secretsPatches))
        writeConfigFile(secretsPath, secretsContent, "PCSX2-secrets");
}
```

- [ ] **Step 4: Build to verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h \
       cpp/src/adapters/duckstation_adapter.h cpp/src/adapters/duckstation_adapter.cpp \
       cpp/src/adapters/pcsx2_adapter.h cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "feat(ra): add RA INI patching to DuckStation and PCSX2 adapters"
```

---

### Task 6: Wire RAService into AppController

**Files:**
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Add RA methods and signals to app_controller.h**

Add include at the top:
```cpp
#include "services/ra_service.h"
```

Add Q_INVOKABLE methods after the scraper section (after line 157):

```cpp
    // RetroAchievements
    Q_INVOKABLE void raLogin(const QString& username, const QString& password);
    Q_INVOKABLE void raSignOut();
    Q_INVOKABLE bool hasRACredentials() const;
    Q_INVOKABLE QString raUsername() const;
    Q_INVOKABLE QVariantMap raUserSummary();
    Q_INVOKABLE QVariantList raGameProgressList();
    Q_INVOKABLE QVariantList raAchievementsForGame(int raGameId);
    Q_INVOKABLE QVariantList raRecentUnlocks(int count = 5);
    Q_INVOKABLE bool raHasAchievements(int gameId);
    Q_INVOKABLE int raGameIdForGame(int gameId);
    Q_INVOKABLE void raStartSync();
    Q_INVOKABLE bool raIsSyncing() const;
    Q_INVOKABLE bool raHardcoreMode() const;
    Q_INVOKABLE void raSetHardcoreMode(bool enabled);
    Q_INVOKABLE bool raNotifications() const;
    Q_INVOKABLE void raSetNotifications(bool enabled);
    Q_INVOKABLE bool raSoundEffects() const;
    Q_INVOKABLE void raSetSoundEffects(bool enabled);
```

Add signals (after `updateAvailable` signal):

```cpp
    void raLoginCompleted(bool success, const QString& message);
    void raSignedOut();
    void raSyncCompleted();
    void raSyncingChanged();
    void raSyncProgress(const QString& message);
```

Add member variable (after `m_emuService`):

```cpp
    RAService m_raService;
```

- [ ] **Step 2: Wire up in app_controller.cpp**

In the constructor, after `m_scraperService.loadCredentials();` (line 32), add:

```cpp
    m_raService.loadCredentials();
```

Change the `m_emuService(loader)` initializer to add `m_raService(db)` after it:

```cpp
    , m_emuService(loader)
    , m_raService(db)
```

After the emulator service signal connections (around line 73), add:

```cpp
    // Forward RA signals
    connect(&m_raService, &RAService::loginCompleted, this, &AppController::raLoginCompleted);
    connect(&m_raService, &RAService::signedOut, this, &AppController::raSignedOut);
    connect(&m_raService, &RAService::syncCompleted, this, &AppController::raSyncCompleted);
    connect(&m_raService, &RAService::syncingChanged, this, &AppController::raSyncingChanged);
    connect(&m_raService, &RAService::syncProgress, this, &AppController::raSyncProgress);

    // Trigger incremental RA sync on startup if logged in
    if (m_raService.hasCredentials())
        m_raService.startIncrementalSync();

    // Trigger targeted RA sync after game exit
    connect(&m_gameService, &GameService::gameFinished, this, [this](int, bool) {
        // Find the last-played game and sync its achievements
        // The game_service tracks the current ROM path internally
        // We rely on the incremental sync catching up
    });
```

Add method implementations at the end of `app_controller.cpp`:

```cpp
// ── RetroAchievements ──────────────────────────────────────────

void AppController::raLogin(const QString& username, const QString& password) {
    m_raService.login(username, password);
}

void AppController::raSignOut() {
    m_raService.signOut();
    // Disable RA in emulator configs
    for (const auto& manifest : m_loader->manifests()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(manifest.id);
        if (adapter && adapter->supportsRetroAchievements()) {
            adapter->patchRetroAchievements("", "", false, false, false);
        }
    }
}

bool AppController::hasRACredentials() const { return m_raService.hasCredentials(); }
QString AppController::raUsername() const { return m_raService.username(); }
QVariantMap AppController::raUserSummary() { return m_raService.userSummary(); }
QVariantList AppController::raGameProgressList() { return m_raService.gameProgressList(); }
QVariantList AppController::raAchievementsForGame(int raGameId) { return m_raService.achievementsForGame(raGameId); }
QVariantList AppController::raRecentUnlocks(int count) { return m_raService.recentUnlocks(count); }
bool AppController::raHasAchievements(int gameId) { return m_raService.hasAchievements(gameId); }
int AppController::raGameIdForGame(int gameId) { return m_raService.raGameId(gameId); }

void AppController::raStartSync() { m_raService.startFullSync(); }
bool AppController::raIsSyncing() const { return m_raService.isSyncing(); }

bool AppController::raHardcoreMode() const { return m_raService.hardcoreMode(); }
void AppController::raSetHardcoreMode(bool enabled) { m_raService.setHardcoreMode(enabled); }
bool AppController::raNotifications() const { return m_raService.notifications(); }
void AppController::raSetNotifications(bool enabled) { m_raService.setNotifications(enabled); }
bool AppController::raSoundEffects() const { return m_raService.soundEffects(); }
void AppController::raSetSoundEffects(bool enabled) { m_raService.setSoundEffects(enabled); }
```

- [ ] **Step 3: Patch RA credentials during game launch**

In `app_controller.cpp`, find the `launchGame()` method. Before the `m_gameService.startGame()` call, add:

```cpp
    // Patch RA credentials into emulator config if logged in
    if (m_raService.hasCredentials()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (adapter && adapter->supportsRetroAchievements()) {
            adapter->patchRetroAchievements(
                m_raService.username(),
                m_raService.credentials().token,
                m_raService.hardcoreMode(),
                m_raService.notifications(),
                m_raService.soundEffects());
        }
    }
```

Note: You'll need to add a `credentials()` accessor to RAService. Add to `ra_service.h`:

```cpp
    const RACredentials& credentials() const { return m_creds; }
```

- [ ] **Step 4: Build to verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp \
       cpp/src/services/ra_service.h
git commit -m "feat(ra): wire RAService into AppController with QML methods"
```

---

### Task 7: Wire RA into ThemeContext and GameListModel

**Files:**
- Modify: `cpp/src/ui/theme_context.h`
- Modify: `cpp/src/ui/theme_context.cpp`
- Modify: `cpp/src/ui/game_list_model.h`
- Modify: `cpp/src/ui/game_list_model.cpp`

- [ ] **Step 1: Add RA methods to ThemeContext**

In `cpp/src/ui/theme_context.h`, add after `hasScraperCredentials()` (line 50):

```cpp
    Q_INVOKABLE bool hasRACredentials() const;
    Q_INVOKABLE bool raHasAchievements(int gameId) const;
    Q_INVOKABLE int raGameIdForGame(int gameId) const;
```

In `cpp/src/ui/theme_context.cpp`, implement them (they delegate to AppController):

```cpp
bool ThemeContext::hasRACredentials() const {
    return m_app->hasRACredentials();
}

bool ThemeContext::raHasAchievements(int gameId) const {
    return m_app->raHasAchievements(gameId);
}

int ThemeContext::raGameIdForGame(int gameId) const {
    return m_app->raGameIdForGame(gameId);
}
```

- [ ] **Step 2: Add HasAchievementsRole to GameListModel**

In `cpp/src/ui/game_list_model.h`, add to the `Roles` enum after `DiscCountRole` (line 39):

```cpp
        HasAchievementsRole,
```

In `cpp/src/ui/game_list_model.cpp`, in the `roleNames()` method, add:

```cpp
    {HasAchievementsRole, "hasAchievements"},
```

In the `data()` method, add a case for the new role:

```cpp
    case HasAchievementsRole: return m_db->hasRAAchievements(game.id);
```

- [ ] **Step 3: Build to verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/theme_context.h cpp/src/ui/theme_context.cpp \
       cpp/src/ui/game_list_model.h cpp/src/ui/game_list_model.cpp
git commit -m "feat(ra): add RA methods to ThemeContext and HasAchievementsRole to GameListModel"
```

---

### Task 8: Trigger Game Sync After Exit

**Files:**
- Modify: `cpp/src/ui/app_controller.cpp`
- Modify: `cpp/src/services/game_service.h`

- [ ] **Step 1: Expose current ROM path from GameService**

In `cpp/src/services/game_service.h`, add a public accessor:

```cpp
    /** Return the ROM path of the currently/last running game. */
    QString currentRomPath() const { return m_currentRomPath; }
```

- [ ] **Step 2: Wire game exit to targeted RA sync**

In `cpp/src/ui/app_controller.cpp`, replace the placeholder `gameFinished` lambda (the one we added in Task 6) with:

```cpp
    connect(&m_gameService, &GameService::gameFinished, this, [this](int, bool) {
        if (!m_raService.hasCredentials()) return;
        // Find the game ID for the ROM that just finished
        QString romPath = m_gameService.currentRomPath();
        if (romPath.isEmpty()) return;
        auto games = m_db->allGames();
        for (const auto& game : games) {
            if (game.rom_path == romPath) {
                m_raService.startGameSync(game.id);
                break;
            }
        }
    });
```

- [ ] **Step 3: Build to verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/app_controller.cpp cpp/src/services/game_service.h
git commit -m "feat(ra): trigger targeted RA sync after game exit"
```

---

### Task 9: RetroAchievements Settings QML Page

**Files:**
- Create: `cpp/qml/AppUI/RetroAchievementsSettings.qml`
- Modify: `cpp/qml/AppUI/SettingsOverlay.qml`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create RetroAchievementsSettings.qml**

```qml
// cpp/qml/AppUI/RetroAchievementsSettings.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    property string screenState: app.hasRACredentials() ? "dashboard" : "login"
    property int loginFocusIndex: 0
    property int dashFocusIndex: 0
    property var userSummary: ({})
    property var gameProgress: []
    property var recentUnlocks: []
    property bool syncing: false

    Component.onCompleted: {
        if (screenState === "dashboard") refreshData()
    }

    function refreshData() {
        userSummary = app.raUserSummary()
        gameProgress = app.raGameProgressList()
        recentUnlocks = app.raRecentUnlocks(5)
        syncing = app.raIsSyncing()
    }

    Connections {
        target: app
        function onRaLoginCompleted(success, message) {
            if (success) {
                root.screenState = "dashboard"
                refreshData()
            }
            loginError.text = success ? "" : message
        }
        function onRaSignedOut() {
            root.screenState = "login"
            loginUserField.text = ""
            loginPassField.text = ""
        }
        function onRaSyncCompleted() {
            syncing = false
            refreshData()
        }
        function onRaSyncingChanged() {
            syncing = app.raIsSyncing()
        }
    }

    // ── Login Screen ──
    Item {
        anchors.fill: parent
        visible: screenState === "login"

        Rectangle {
            anchors.centerIn: parent
            width: 420
            height: loginCol.height + 80
            radius: 16
            color: Qt.rgba(0.09, 0.09, 0.12, 1)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            Column {
                id: loginCol
                anchors.centerIn: parent
                width: parent.width - 80
                spacing: 16

                // Logo
                Rectangle {
                    width: 56; height: 56; radius: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#f59e0b" }
                        GradientStop { position: 1; color: "#ef4444" }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "RA"; color: "#fff"
                        font.pixelSize: 22; font.weight: Font.ExtraBold
                    }
                }

                Text {
                    text: "RetroAchievements"
                    color: "#ffffff"
                    font.pixelSize: 20; font.weight: Font.DemiBold
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "Sign in to track your achievements"
                    color: Qt.rgba(1, 1, 1, 0.4)
                    font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                // Username
                Column {
                    width: parent.width; spacing: 4
                    Text { text: "USERNAME"; color: Qt.rgba(1,1,1,0.4); font.pixelSize: 11 }
                    Rectangle {
                        width: parent.width; height: 40; radius: 8
                        color: Qt.rgba(0.05, 0.05, 0.08, 1)
                        border.color: loginFocusIndex === 0 ? "#6366f1" : Qt.rgba(1,1,1,0.1)
                        border.width: 1
                        TextInput {
                            id: loginUserField
                            anchors.fill: parent; anchors.margins: 10
                            color: "#fff"; font.pixelSize: 14
                            clip: true
                        }
                    }
                }

                // Password
                Column {
                    width: parent.width; spacing: 4
                    Text { text: "PASSWORD"; color: Qt.rgba(1,1,1,0.4); font.pixelSize: 11 }
                    Rectangle {
                        width: parent.width; height: 40; radius: 8
                        color: Qt.rgba(0.05, 0.05, 0.08, 1)
                        border.color: loginFocusIndex === 1 ? "#6366f1" : Qt.rgba(1,1,1,0.1)
                        border.width: 1
                        TextInput {
                            id: loginPassField
                            anchors.fill: parent; anchors.margins: 10
                            color: "#fff"; font.pixelSize: 14
                            echoMode: TextInput.Password; clip: true
                        }
                    }
                }

                // Error
                Text {
                    id: loginError
                    width: parent.width
                    color: "#ff4444"; font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                // Sign In button
                Rectangle {
                    width: parent.width; height: 44; radius: 8
                    color: loginFocusIndex === 2 ? "#5558e6" : "#6366f1"
                    Text {
                        anchors.centerIn: parent
                        text: "Sign In"; color: "#fff"
                        font.pixelSize: 15; font.weight: Font.DemiBold
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: doLogin()
                    }
                }
            }
        }

        function doLogin() {
            if (loginUserField.text.trim() === "" || loginPassField.text === "") return
            loginError.text = ""
            app.raLogin(loginUserField.text.trim(), loginPassField.text)
        }
    }

    // ── Dashboard Screen ──
    Flickable {
        anchors.fill: parent
        visible: screenState === "dashboard"
        contentHeight: dashCol.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: dashCol
            width: parent.width
            spacing: 20
            padding: 20

            // Profile Header
            Rectangle {
                width: parent.width - 40; height: 80; radius: 16
                color: Qt.rgba(0.09, 0.09, 0.12, 1)
                border.color: Qt.rgba(1,1,1,0.08); border.width: 1

                RowLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 16

                    // Avatar
                    Rectangle {
                        width: 48; height: 48; radius: 24
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#6366f1" }
                            GradientStop { position: 1; color: "#8b5cf6" }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: (userSummary.username || "?").charAt(0).toUpperCase()
                            color: "#fff"; font.pixelSize: 20; font.weight: Font.Bold
                        }
                    }

                    Column {
                        spacing: 4; Layout.fillWidth: true
                        Text {
                            text: userSummary.username || ""
                            color: "#fff"; font.pixelSize: 18; font.weight: Font.DemiBold
                        }
                        Row {
                            spacing: 16
                            Text { text: "<font color='#f59e0b'>" + (userSummary.totalPoints || 0) + "</font> points"; color: "#888"; font.pixelSize: 12; textFormat: Text.RichText }
                            Text { text: "Rank <font color='#f59e0b'>#" + (userSummary.rank || 0) + "</font>"; color: "#888"; font.pixelSize: 12; textFormat: Text.RichText }
                        }
                    }

                    // Sign Out
                    Rectangle {
                        width: 80; height: 32; radius: 8
                        color: "transparent"
                        border.color: Qt.rgba(1,1,1,0.15); border.width: 1
                        Text { anchors.centerIn: parent; text: "Sign Out"; color: Qt.rgba(1,1,1,0.5); font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: app.raSignOut()
                        }
                    }
                }
            }

            // Stats Grid Row 1
            Row {
                spacing: 12; anchors.horizontalCenter: parent.horizontalCenter
                Repeater {
                    model: [
                        { value: userSummary.totalPoints || 0, label: "TOTAL POINTS", color: "#f59e0b" },
                        { value: userSummary.totalAchievements || 0, label: "ACHIEVEMENTS", color: "#8b5cf6" },
                        { value: userSummary.gamesMastered || 0, label: "GAMES MASTERED", color: "#22c55e" },
                        { value: userSummary.gamesStarted || 0, label: "GAMES STARTED", color: "#6366f1" }
                    ]
                    Rectangle {
                        required property var modelData
                        width: (root.width - 100) / 4; height: 80; radius: 12
                        color: Qt.rgba(0.09, 0.09, 0.12, 1)
                        border.color: Qt.rgba(1,1,1,0.08); border.width: 1
                        Column {
                            anchors.centerIn: parent; spacing: 4
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; color: modelData.color; font.pixelSize: 24; font.weight: Font.Bold }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Qt.rgba(1,1,1,0.35); font.pixelSize: 10 }
                        }
                    }
                }
            }

            // Stats Grid Row 2
            Row {
                spacing: 12; anchors.horizontalCenter: parent.horizontalCenter
                Repeater {
                    model: [
                        { value: userSummary.hardcorePoints || 0, label: "HARDCORE POINTS", color: "#f59e0b" },
                        { value: userSummary.softcorePoints || 0, label: "SOFTCORE POINTS", color: "#aaa" },
                        { value: userSummary.avgCompletion || "0%", label: "AVG COMPLETION", color: "#8b5cf6" }
                    ]
                    Rectangle {
                        required property var modelData
                        width: (root.width - 88) / 3; height: 70; radius: 12
                        color: Qt.rgba(0.09, 0.09, 0.12, 1)
                        border.color: Qt.rgba(1,1,1,0.08); border.width: 1
                        Column {
                            anchors.centerIn: parent; spacing: 4
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.value; color: modelData.color; font.pixelSize: 20; font.weight: Font.Bold }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Qt.rgba(1,1,1,0.35); font.pixelSize: 10 }
                        }
                    }
                }
            }

            // Recent Unlocks
            Rectangle {
                width: parent.width - 40; radius: 16
                height: recentCol.height + 48
                color: Qt.rgba(0.09, 0.09, 0.12, 1)
                border.color: Qt.rgba(1,1,1,0.08); border.width: 1

                Column {
                    id: recentCol
                    anchors.top: parent.top; anchors.topMargin: 16
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.margins: 20; spacing: 6

                    Text { text: "RECENT UNLOCKS"; color: Qt.rgba(1,1,1,0.35); font.pixelSize: 11 }

                    Repeater {
                        model: recentUnlocks
                        Rectangle {
                            required property var modelData
                            width: parent.width; height: 44; radius: 8
                            color: Qt.rgba(0.1, 0.1, 0.15, 1)

                            RowLayout {
                                anchors.fill: parent; anchors.margins: 8; spacing: 10

                                Image {
                                    source: modelData.badgePath ? "file://" + modelData.badgePath : ""
                                    Layout.preferredWidth: 28; Layout.preferredHeight: 28
                                    fillMode: Image.PreserveAspectFit
                                    visible: source != ""
                                }

                                Column {
                                    spacing: 1; Layout.fillWidth: true
                                    Text { text: modelData.title || ""; color: "#e0e0e0"; font.pixelSize: 13; elide: Text.ElideRight; width: parent.width }
                                    Text { text: modelData.gameTitle || ""; color: Qt.rgba(1,1,1,0.35); font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
                                }

                                Text { text: modelData.earnedDate || ""; color: Qt.rgba(1,1,1,0.25); font.pixelSize: 11 }
                            }
                        }
                    }

                    Text {
                        visible: recentUnlocks.length === 0
                        text: "No achievements earned yet"
                        color: Qt.rgba(1,1,1,0.25); font.pixelSize: 13
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            // Game Progress Grid
            Rectangle {
                width: parent.width - 40; radius: 16
                height: gpCol.height + 48
                color: Qt.rgba(0.09, 0.09, 0.12, 1)
                border.color: Qt.rgba(1,1,1,0.08); border.width: 1

                Column {
                    id: gpCol
                    anchors.top: parent.top; anchors.topMargin: 16
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.margins: 20; spacing: 12

                    Text { text: "GAME PROGRESS"; color: Qt.rgba(1,1,1,0.35); font.pixelSize: 11 }

                    Grid {
                        columns: 3; spacing: 10; width: parent.width

                        Repeater {
                            model: gameProgress
                            Rectangle {
                                required property var modelData
                                width: (gpCol.width - 20) / 3; height: 90; radius: 10
                                color: Qt.rgba(0.1, 0.1, 0.15, 1)
                                border.color: modelData.mastered ? Qt.rgba(0.13, 0.77, 0.37, 0.2) : Qt.rgba(1,1,1,0.05)
                                border.width: 1

                                Column {
                                    anchors.fill: parent; anchors.margins: 12; spacing: 4

                                    Text {
                                        text: (modelData.mastered ? "\uD83C\uDFC5 " : "") + (modelData.title || "")
                                        color: modelData.mastered ? "#22c55e" : "#e0e0e0"
                                        font.pixelSize: 13; font.weight: Font.DemiBold
                                        elide: Text.ElideRight; width: parent.width
                                    }
                                    Text {
                                        text: modelData.system || ""
                                        color: Qt.rgba(1,1,1,0.25); font.pixelSize: 10
                                    }

                                    Item { width: 1; height: 4 }

                                    Rectangle {
                                        width: parent.width; height: 4; radius: 2
                                        color: Qt.rgba(1,1,1,0.1)
                                        Rectangle {
                                            width: parent.width * Math.min(1, (modelData.numEarned || 0) / Math.max(1, modelData.numAchievements || 1))
                                            height: parent.height; radius: 2
                                            color: modelData.mastered ? "#22c55e"
                                                 : (modelData.numEarned / Math.max(1, modelData.numAchievements) > 0.3) ? "#6366f1" : "#f59e0b"
                                        }
                                    }

                                    RowLayout {
                                        width: parent.width
                                        Text { text: (modelData.numEarned || 0) + " / " + (modelData.numAchievements || 0); color: Qt.rgba(1,1,1,0.5); font.pixelSize: 11 }
                                        Item { Layout.fillWidth: true }
                                        Text { text: modelData.completionPct || "0%"; color: Qt.rgba(1,1,1,0.35); font.pixelSize: 11 }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        // Navigate to achievement list — push onto settings panelStack
                                        if (typeof panelStack !== 'undefined')
                                            panelStack.push(achievementsPageComponent, { raGameId: modelData.raGameId, gameTitle: modelData.title })
                                    }
                                }
                            }
                        }

                        Text {
                            visible: gameProgress.length === 0
                            text: syncing ? "Syncing..." : "No games matched yet"
                            color: Qt.rgba(1,1,1,0.25); font.pixelSize: 13
                        }
                    }
                }
            }

            // Options
            Rectangle {
                width: parent.width - 40; radius: 16
                height: optCol.height + 48
                color: Qt.rgba(0.09, 0.09, 0.12, 1)
                border.color: Qt.rgba(1,1,1,0.08); border.width: 1

                Column {
                    id: optCol
                    anchors.top: parent.top; anchors.topMargin: 16
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.margins: 20; spacing: 0

                    Text { text: "OPTIONS"; color: Qt.rgba(1,1,1,0.35); font.pixelSize: 11; bottomPadding: 12 }

                    // Hardcore Mode
                    RowLayout {
                        width: parent.width; height: 50
                        Column {
                            spacing: 2; Layout.fillWidth: true
                            Text { text: "Hardcore Mode"; color: "#e0e0e0"; font.pixelSize: 14 }
                            Text { text: "No save states, cheats, or fast-forward"; color: Qt.rgba(1,1,1,0.3); font.pixelSize: 11 }
                        }
                        Rectangle {
                            width: 44; height: 24; radius: 12
                            color: app.raHardcoreMode() ? "#6366f1" : Qt.rgba(1,1,1,0.15)
                            Rectangle {
                                width: 18; height: 18; radius: 9; color: "#fff"
                                x: app.raHardcoreMode() ? parent.width - 21 : 3; y: 3
                                Behavior on x { NumberAnimation { duration: 150 } }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: app.raSetHardcoreMode(!app.raHardcoreMode()) }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Qt.rgba(1,1,1,0.05) }

                    // Notifications
                    RowLayout {
                        width: parent.width; height: 50
                        Column {
                            spacing: 2; Layout.fillWidth: true
                            Text { text: "Notifications"; color: "#e0e0e0"; font.pixelSize: 14 }
                            Text { text: "Show achievement unlock popups during gameplay"; color: Qt.rgba(1,1,1,0.3); font.pixelSize: 11 }
                        }
                        Rectangle {
                            width: 44; height: 24; radius: 12
                            color: app.raNotifications() ? "#6366f1" : Qt.rgba(1,1,1,0.15)
                            Rectangle {
                                width: 18; height: 18; radius: 9; color: "#fff"
                                x: app.raNotifications() ? parent.width - 21 : 3; y: 3
                                Behavior on x { NumberAnimation { duration: 150 } }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: app.raSetNotifications(!app.raNotifications()) }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Qt.rgba(1,1,1,0.05) }

                    // Sound Effects
                    RowLayout {
                        width: parent.width; height: 50
                        Column {
                            spacing: 2; Layout.fillWidth: true
                            Text { text: "Sound Effects"; color: "#e0e0e0"; font.pixelSize: 14 }
                            Text { text: "Play sounds on achievement unlock"; color: Qt.rgba(1,1,1,0.3); font.pixelSize: 11 }
                        }
                        Rectangle {
                            width: 44; height: 24; radius: 12
                            color: app.raSoundEffects() ? "#6366f1" : Qt.rgba(1,1,1,0.15)
                            Rectangle {
                                width: 18; height: 18; radius: 9; color: "#fff"
                                x: app.raSoundEffects() ? parent.width - 21 : 3; y: 3
                                Behavior on x { NumberAnimation { duration: 150 } }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: app.raSetSoundEffects(!app.raSoundEffects()) }
                        }
                    }
                }
            }

            // Sync Bar
            Rectangle {
                width: parent.width - 40; height: 50; radius: 16
                color: Qt.rgba(0.09, 0.09, 0.12, 1)
                border.color: Qt.rgba(1,1,1,0.08); border.width: 1

                RowLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    Text {
                        text: syncing ? "Syncing..." : "Last synced: just now"
                        color: Qt.rgba(1,1,1,0.35); font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        width: 90; height: 30; radius: 8
                        color: "transparent"
                        border.color: Qt.rgba(1,1,1,0.15); border.width: 1
                        opacity: syncing ? 0.5 : 1
                        Text { anchors.centerIn: parent; text: "Sync Now"; color: Qt.rgba(1,1,1,0.5); font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: if (!syncing) app.raStartSync()
                        }
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 2: Add to SettingsOverlay.qml**

In `cpp/qml/AppUI/SettingsOverlay.qml`:

Update the `categoryCount` (line 14) from its current value to add 1 more.

In the ListModel (lines 365-372), add a new entry before the Exit entry:

```qml
ListElement { name: "Achievements"; icon: "\uD83C\uDFC6"; subtitle: "RetroAchievements login & progress"; catIndex: 6 }
```

Update the Exit entry's catIndex from 6 to 7.

In the `selectCategory` function, add the new case before the exit case:

```qml
else if (idx === 6) panelStack.push(raPageComponent)
else if (idx === 7) overlay.exitDialogVisible = true
```

Update the existing category index checks (line 340-346) accordingly — bump the exit index from 6 to 7.

At the bottom with other Component definitions, add:

```qml
Component {
    id: raPageComponent
    RetroAchievementsSettings {}
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

In the AppUI QML_FILES section, add:

```
qml/AppUI/RetroAchievementsSettings.qml
```

- [ ] **Step 4: Build to verify**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/RetroAchievementsSettings.qml cpp/qml/AppUI/SettingsOverlay.qml cpp/CMakeLists.txt
git commit -m "feat(ra): add RetroAchievements settings page to settings overlay"
```

---

### Task 10: Achievements Page QML (Per-Game View)

**Files:**
- Create: `cpp/qml/AppUI/AchievementsPage.qml`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create AchievementsPage.qml**

```qml
// cpp/qml/AppUI/AchievementsPage.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    property int raGameId: 0
    property string gameTitle: ""
    property var achievements: []
    property int totalPoints: 0
    property int earnedPoints: 0
    property int numEarned: 0
    property int numTotal: 0

    Component.onCompleted: loadData()

    function loadData() {
        achievements = app.raAchievementsForGame(raGameId)
        numTotal = achievements.length
        totalPoints = 0; earnedPoints = 0; numEarned = 0
        for (var i = 0; i < achievements.length; i++) {
            totalPoints += achievements[i].points
            if (achievements[i].earned) {
                earnedPoints += achievements[i].points
                numEarned++
            }
        }
    }

    // Header
    Rectangle {
        id: header
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 100; color: "transparent"

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 20; spacing: 8

            Text {
                text: gameTitle
                color: "#fff"; font.pixelSize: 20; font.weight: Font.DemiBold
                elide: Text.ElideRight; Layout.fillWidth: true
            }

            RowLayout {
                spacing: 16
                Text {
                    text: "<font color='#f59e0b'>" + numEarned + "</font> / " + numTotal + " achievements"
                    color: "#888"; font.pixelSize: 13; textFormat: Text.RichText
                }
                Text {
                    text: "<font color='#f59e0b'>" + earnedPoints + "</font> / " + totalPoints + " points"
                    color: "#888"; font.pixelSize: 13; textFormat: Text.RichText
                }
            }

            // Progress bar
            Rectangle {
                Layout.fillWidth: true; height: 6; radius: 3
                color: Qt.rgba(1,1,1,0.1)
                Rectangle {
                    width: parent.width * (numTotal > 0 ? numEarned / numTotal : 0)
                    height: parent.height; radius: 3
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#6366f1" }
                        GradientStop { position: 1; color: "#8b5cf6" }
                    }
                }
            }

            Text {
                text: numTotal > 0 ? Math.round(numEarned * 100 / numTotal) + "% complete" : "No achievements"
                color: Qt.rgba(1,1,1,0.35); font.pixelSize: 11
            }
        }
    }

    // Achievement list
    ListView {
        id: achList
        anchors.top: header.bottom; anchors.left: parent.left
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 20; anchors.topMargin: 0
        spacing: 6; clip: true
        model: achievements
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            required property var modelData
            required property int index

            width: achList.width; height: 70; radius: 10
            color: Qt.rgba(0.09, 0.09, 0.12, 1)
            border.color: Qt.rgba(1,1,1,0.05); border.width: 1
            opacity: modelData.earned ? 1.0 : 0.5

            RowLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 12

                // Badge
                Rectangle {
                    Layout.preferredWidth: 44; Layout.preferredHeight: 44; radius: 10
                    color: modelData.earned ? "transparent" : Qt.rgba(1,1,1,0.08)

                    Image {
                        anchors.fill: parent; anchors.margins: 2
                        source: modelData.badgePath ? "file://" + modelData.badgePath : ""
                        fillMode: Image.PreserveAspectFit
                        visible: source != ""
                        opacity: modelData.earned ? 1.0 : 0.3
                    }

                    // Lock overlay for unearned
                    Text {
                        anchors.centerIn: parent
                        text: "\uD83D\uDD12"; font.pixelSize: 18
                        visible: !modelData.earned && modelData.badgePath === ""
                    }
                }

                // Details
                ColumnLayout {
                    spacing: 2; Layout.fillWidth: true

                    Text {
                        text: modelData.title || ""
                        color: modelData.earned ? "#fff" : Qt.rgba(1,1,1,0.5)
                        font.pixelSize: 14; font.weight: Font.DemiBold
                        elide: Text.ElideRight; Layout.fillWidth: true
                    }
                    Text {
                        text: modelData.description || ""
                        color: Qt.rgba(1,1,1,0.4); font.pixelSize: 12
                        elide: Text.ElideRight; Layout.fillWidth: true
                    }
                }

                // Points + Rarity + Date
                ColumnLayout {
                    spacing: 2; Layout.alignment: Qt.AlignRight

                    Text {
                        text: (modelData.points || 0) + " pts"
                        color: modelData.earned ? "#f59e0b" : Qt.rgba(1,1,1,0.3)
                        font.pixelSize: 14; font.weight: Font.Bold
                        horizontalAlignment: Text.AlignRight; Layout.alignment: Qt.AlignRight
                    }

                    Text {
                        property int ratio: modelData.trueRatio || 0
                        property int pts: modelData.points || 1
                        text: ratio > 0 ? (Math.round(pts * 100 / ratio)) + "% of players" : ""
                        color: Qt.rgba(1,1,1,0.25); font.pixelSize: 10
                        horizontalAlignment: Text.AlignRight; Layout.alignment: Qt.AlignRight
                    }

                    Text {
                        text: modelData.earned ? ("Earned " + (modelData.earnedDate || "")) : ""
                        color: "#6366f1"; font.pixelSize: 10
                        visible: modelData.earned
                        horizontalAlignment: Text.AlignRight; Layout.alignment: Qt.AlignRight
                    }
                }
            }
        }

        // Empty state
        Text {
            anchors.centerIn: parent
            visible: achievements.length === 0
            text: app.hasRACredentials() ? "No achievements available for this game" : "Log in to RetroAchievements to track progress"
            color: Qt.rgba(1,1,1,0.3); font.pixelSize: 14
        }
    }

    // Keyboard/controller navigation
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            event.accepted = true
            // Pop back
            if (typeof panelStack !== 'undefined' && panelStack.depth > 1)
                panelStack.pop()
            else if (typeof mainStack !== 'undefined' && mainStack.depth > 1)
                mainStack.pop()
        }
    }
}
```

- [ ] **Step 2: Add to CMakeLists.txt**

In the AppUI QML_FILES section, add:

```
qml/AppUI/AchievementsPage.qml
```

- [ ] **Step 3: Build to verify**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/AchievementsPage.qml cpp/CMakeLists.txt
git commit -m "feat(ra): add per-game AchievementsPage QML"
```

---

### Task 11: Add "Achievements" to Game Action Popup

**Files:**
- Modify: `cpp/qml/AppUI/GameActionPopup.qml`

- [ ] **Step 1: Add achievements action to popup model**

In `cpp/qml/AppUI/GameActionPopup.qml`, add a `canViewAchievements` property (after `canScrape` at line 17):

```qml
property bool canViewAchievements: false
```

In the `open()` function (line 19), after `canScrape = themeContext.hasScraperCredentials()` (line 24), add:

```qml
canViewAchievements = themeContext.raHasAchievements(gameId)
```

In the Repeater model array (lines 89-95), add a new entry after "Scrape":

```qml
{ label: "Achievements", actionId: "achievements", destructive: false },
```

The full model becomes:
```qml
model: [
    { label: "Scrape", actionId: "scrape", destructive: false },
    { label: "Achievements", actionId: "achievements", destructive: false },
    { label: popup.isFavorite ? "Remove from Favorites" : "Add to Favorites",
      actionId: "favorite", destructive: false },
    { label: "Open ROM Folder", actionId: "openFolder", destructive: false },
    { label: "Remove from Library", actionId: "remove", destructive: true }
]
```

- [ ] **Step 2: Update disabled logic and action count**

In the delegate's `disabled` property (line 101), update to also handle achievements:

```qml
property bool disabled: (modelData.actionId === "scrape" && !popup.canScrape)
                     || (modelData.actionId === "achievements" && !popup.canViewAchievements)
```

Add a subtitle for when achievements are disabled. In the subtitle Text element (lines 123-128), update the text:

```qml
Text {
    anchors.horizontalCenter: parent.horizontalCenter
    visible: disabled
    text: modelData.actionId === "scrape" ? "Requires ScreenScraper login"
        : modelData.actionId === "achievements" ? "No achievements available"
        : ""
    color: Qt.rgba(1, 1, 1, 0.25)
    font.pixelSize: 11
}
```

- [ ] **Step 3: Update action count and key handler**

In the `Keys.onPressed` handler (line 244), update `actionCount` from 4 to 5:

```qml
var actionCount = 5
```

Update the actions array (line 253):

```qml
var actions = ["scrape", "achievements", "favorite", "openFolder", "remove"]
```

Update the skip-disabled check (line 255):

```qml
if ((actionId === "scrape" && !canScrape) || (actionId === "achievements" && !canViewAchievements))
    return
```

- [ ] **Step 4: Handle achievements action**

In `executeAction()` (line 210), add a new case:

```qml
case "achievements":
    var raId = themeContext.raGameIdForGame(targetGameId)
    var title = popup.gameTitle
    popup.close()
    // Push achievements page onto main stack
    mainStack.push(achievementsPageComponent, { raGameId: raId, gameTitle: title })
    break
```

- [ ] **Step 5: Add AchievementsPage component to AppWindow**

In `cpp/qml/AppUI/AppWindow.qml`, add a Component declaration (alongside other Component declarations):

```qml
Component {
    id: achievementsPageComponent
    AchievementsPage {}
}
```

- [ ] **Step 6: Build to verify**

Run: `cd cpp && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 7: Commit**

```bash
git add cpp/qml/AppUI/GameActionPopup.qml cpp/qml/AppUI/AppWindow.qml
git commit -m "feat(ra): add Achievements action to game popup"
```

---

### Task 12: Integration Testing and Cleanup

- [ ] **Step 1: Full build from clean**

```bash
cd cpp && rm -rf build && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -10
```

Expected: Clean build with no warnings related to RA code.

- [ ] **Step 2: Run existing tests**

```bash
cd cpp && cd build && ctest --output-on-failure
```

Expected: All existing tests pass (IniFile, RomScanner).

- [ ] **Step 3: Run the app and verify settings overlay**

```bash
./build/EmulatorFrontend
```

Manual verification:
- Press Escape → Settings overlay should now show "Achievements" category with trophy icon
- Clicking it should show the RA login screen
- All other settings categories should still work (indices shifted by 1)

- [ ] **Step 4: Commit any fixes**

If any issues found during testing, fix and commit:

```bash
git add -A
git commit -m "fix(ra): integration test fixes"
```
