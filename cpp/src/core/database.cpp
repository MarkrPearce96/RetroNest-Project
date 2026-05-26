#include "database.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>

static const char* DB_CONNECTION = "emulator_frontend";

bool Database::open(const QString& dbPath) {
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    auto db = QSqlDatabase::addDatabase("QSQLITE", DB_CONNECTION);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qCritical() << "[Database] Failed to open:" << db.lastError().text();
        return false;
    }

    m_dbPath = dbPath;

    if (!createTables()) {
        qCritical() << "[Database] Failed to create tables";
        return false;
    }
    qInfo() << "[Database] Opened:" << dbPath;
    return true;
}

bool Database::backupBeforeMigration(int fromVersion) {
    if (m_dbPath.isEmpty()) return false;
    if (!QFileInfo::exists(m_dbPath)) return false;  // fresh DB; nothing to back up
    const QString backupPath = QString("%1.bak-v%2").arg(m_dbPath).arg(fromVersion);
    if (QFileInfo::exists(backupPath))
        QFile::remove(backupPath);   // overwrite previous backup at same version
    if (!QFile::copy(m_dbPath, backupPath)) {
        qWarning() << "[Database] Failed to back up DB to" << backupPath
                   << "before migration; aborting migration to avoid data loss";
        return false;
    }
    qInfo() << "[Database] Pre-migration backup written to" << backupPath;
    return true;
}

void Database::close() {
    {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        if (db.isOpen()) db.close();
    }
    QSqlDatabase::removeDatabase(DB_CONNECTION);
}

static const int CURRENT_SCHEMA_VERSION = 7;

bool Database::createTables() {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);

    if (!q.exec(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "  version INTEGER NOT NULL"
        ")")) {
        qCritical() << "[Database] Failed to create schema_version table:" << q.lastError().text();
        return false;
    }

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
        "  video_path TEXT NOT NULL DEFAULT '',"
        "  disc_count INTEGER NOT NULL DEFAULT 0,"
        "  serial TEXT NOT NULL DEFAULT ''"
        ")")) {
        qCritical() << "[Database] Failed to create games table:" << q.lastError().text();
        return false;
    }

    int version = schemaVersion();
    if (version < 0) {
        qCritical() << "[Database] Failed to read schema version";
        return false;
    }
    if (version == 0) {
        return setSchemaVersion(CURRENT_SCHEMA_VERSION);
    } else {
        return runMigrations();
    }
}

int Database::schemaVersion() {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    if (!q.exec("SELECT version FROM schema_version LIMIT 1")) {
        qWarning() << "[Database] schemaVersion query failed:" << q.lastError().text();
        return -1;
    }
    if (q.next()) return q.value(0).toInt();
    return 0;  // table exists but is empty — fresh database
}

bool Database::setSchemaVersion(int version) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    if (!q.exec("DELETE FROM schema_version")) {
        qWarning() << "[Database] Failed to clear schema_version:" << q.lastError().text();
        return false;
    }
    q.prepare("INSERT INTO schema_version (version) VALUES (?)");
    q.addBindValue(version);
    if (!q.exec()) {
        qWarning() << "[Database] Failed to set schema version:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::runMigrations() {
    int current = schemaVersion();

    // Pre-migration safety: copy the DB file before touching the schema, so
    // a partial-failure leaves the user with a recoverable copy at the
    // pre-migration version. Skipped on fresh DBs (open() just created them
    // and there is nothing to lose).
    if (current > 0 && current < CURRENT_SCHEMA_VERSION) {
        if (!backupBeforeMigration(current))
            return false;  // refuse to run migrations without a backup
    }

    if (current < 2) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        if (!db.transaction()) {
            qCritical() << "[Database] Failed to begin transaction for v1→v2 migration";
            return false;
        }
        QSqlQuery q(db);
        const QStringList alterStatements = {
            "ALTER TABLE games ADD COLUMN description TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN developer TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN publisher TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN release_date TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN genres TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN rating REAL NOT NULL DEFAULT 0.0",
            "ALTER TABLE games ADD COLUMN players TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN last_played TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE games ADD COLUMN play_count INTEGER NOT NULL DEFAULT 0",
            "ALTER TABLE games ADD COLUMN favorite INTEGER NOT NULL DEFAULT 0",
        };
        for (const auto& sql : alterStatements) {
            if (!q.exec(sql)) {
                qCritical() << "[Database] Migration v1→v2 failed:" << q.lastError().text() << "SQL:" << sql;
                db.rollback();
                return false;
            }
        }
        if (!db.commit()) {
            qCritical() << "[Database] Failed to commit v1→v2 migration";
            db.rollback();
            return false;
        }
        qInfo() << "[Database] Migrated schema v1 → v2 (added metadata columns)";
    }

    if (current < 3) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        if (!db.transaction()) {
            qCritical() << "[Database] Failed to begin transaction for v2→v3 migration";
            return false;
        }
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
                db.rollback();
                return false;
            }
        }
        if (!db.commit()) {
            qCritical() << "[Database] Failed to commit v2→v3 migration";
            db.rollback();
            return false;
        }
        qInfo() << "[Database] Migrated schema v2 → v3 (added media path columns)";
    }

    if (current < 4) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        if (!db.transaction()) {
            qCritical() << "[Database] Failed to begin transaction for v3→v4 migration";
            return false;
        }
        QSqlQuery q(db);
        if (!q.exec("ALTER TABLE games ADD COLUMN disc_count INTEGER NOT NULL DEFAULT 0")) {
            qCritical() << "[Database] Migration v3→v4 failed:" << q.lastError().text();
            db.rollback();
            return false;
        }
        if (!db.commit()) {
            qCritical() << "[Database] Failed to commit v3→v4 migration";
            db.rollback();
            return false;
        }
        qInfo() << "[Database] Migrated schema v3 → v4 (added disc_count column)";
    }

    // v4→v5 migration removed — RA tables were never used. Schema version kept at 5
    // to avoid re-running migrations on existing databases.

    if (current < 6) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        if (!db.transaction()) {
            qCritical() << "[Database] Failed to begin transaction for v5→v6 migration";
            return false;
        }
        QSqlQuery q(db);
        if (!q.exec("ALTER TABLE games ADD COLUMN serial TEXT NOT NULL DEFAULT ''")) {
            qCritical() << "[Database] Migration v5→v6 failed:" << q.lastError().text();
            db.rollback();
            return false;
        }
        if (!db.commit()) {
            qCritical() << "[Database] Failed to commit v5→v6 migration";
            db.rollback();
            return false;
        }
        qInfo() << "[Database] Migrated schema v5 → v6 (added serial column)";
    }

    if (current < 7) {
        // SP8: pcsx2-libretro id retired in favour of plain "pcsx2".
        // Rewrite every scanned game's emulator_id so existing libraries
        // keep launching under the renamed adapter.
        auto db = QSqlDatabase::database(DB_CONNECTION);
        if (!db.transaction()) {
            qCritical() << "[Database] Failed to begin transaction for v6→v7 migration";
            return false;
        }
        QSqlQuery q(db);
        if (!q.exec("UPDATE games SET emulator_id = 'pcsx2' WHERE emulator_id = 'pcsx2-libretro'")) {
            qCritical() << "[Database] Migration v6→v7 failed:" << q.lastError().text();
            db.rollback();
            return false;
        }
        if (!db.commit()) {
            qCritical() << "[Database] Failed to commit v6→v7 migration";
            db.rollback();
            return false;
        }
        qInfo() << "[Database] Migrated schema v6 → v7 (renamed pcsx2-libretro → pcsx2)";
    }

    if (current < CURRENT_SCHEMA_VERSION) {
        if (!setSchemaVersion(CURRENT_SCHEMA_VERSION)) return false;
    }
    return true;
}

int Database::addGame(const GameRecord& game) {
    auto db = QSqlDatabase::database(DB_CONNECTION);

    // Normalize the ROM path to a canonical form to avoid duplicates
    // from symlinks, relative paths, or casing differences
    const QString normalizedPath = QFileInfo(game.rom_path).canonicalFilePath();
    const QString romPath = normalizedPath.isEmpty() ? game.rom_path : normalizedPath;

    // Check for existing entry before insert so we can report it
    if (gameExistsByPath(romPath)) {
        qInfo() << "[Database] Skipped duplicate:" << game.title;
        return 0;
    }

    QSqlQuery q(db);
    q.prepare("INSERT INTO games (title, rom_path, system, emulator_id, cover_path, disc_count, serial) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(game.title);
    q.addBindValue(romPath);
    q.addBindValue(game.system);
    q.addBindValue(game.emulator_id);
    q.addBindValue(game.cover_path.isNull() ? QString("") : game.cover_path);
    q.addBindValue(game.disc_count);
    q.addBindValue(game.serial.isNull() ? QString("") : game.serial);

    if (!q.exec()) {
        qWarning() << "[Database] addGame failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

static const char* GAME_SELECT_COLUMNS =
    "id, title, rom_path, system, emulator_id, cover_path, "
    "description, developer, publisher, release_date, genres, "
    "rating, players, last_played, play_count, favorite, "
    "screenshot_path, titlescreen_path, marquee_path, fanart_path, "
    "box3d_path, backcover_path, miximage_path, physicalmedia_path, "
    "manual_path, video_path, disc_count, serial";

static GameRecord recordFromQuery(QSqlQuery& q) {
    GameRecord g;
    g.id           = q.value(0).toInt();
    g.title        = q.value(1).toString();
    g.rom_path     = q.value(2).toString();
    g.system       = q.value(3).toString();
    g.emulator_id  = q.value(4).toString();
    g.cover_path   = q.value(5).toString();
    g.description  = q.value(6).toString();
    g.developer    = q.value(7).toString();
    g.publisher    = q.value(8).toString();
    g.release_date = q.value(9).toString();
    g.genres       = q.value(10).toString();
    g.rating       = q.value(11).toDouble();
    g.players      = q.value(12).toString();
    g.last_played  = q.value(13).toString();
    g.play_count   = q.value(14).toInt();
    g.favorite     = q.value(15).toInt();
    g.screenshot_path    = q.value(16).toString();
    g.titlescreen_path   = q.value(17).toString();
    g.marquee_path       = q.value(18).toString();
    g.fanart_path        = q.value(19).toString();
    g.box3d_path         = q.value(20).toString();
    g.backcover_path     = q.value(21).toString();
    g.miximage_path      = q.value(22).toString();
    g.physicalmedia_path = q.value(23).toString();
    g.manual_path        = q.value(24).toString();
    g.video_path         = q.value(25).toString();
    g.disc_count         = q.value(26).toInt();
    g.serial             = q.value(27).toString();
    return g;
}

QVector<GameRecord> Database::allGames() {
    QVector<GameRecord> result;
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    if (!q.exec(QString("SELECT %1 FROM games ORDER BY favorite DESC, title").arg(GAME_SELECT_COLUMNS))) {
        qWarning() << "[Database] allGames query failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(recordFromQuery(q));
    }
    return result;
}

QVector<GameRecord> Database::gamesBySystem(const QString& system) {
    QVector<GameRecord> result;
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare(QString("SELECT %1 FROM games WHERE system = ? ORDER BY favorite DESC, title").arg(GAME_SELECT_COLUMNS));
    q.addBindValue(system);
    if (!q.exec()) {
        qWarning() << "[Database] gamesBySystem query failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(recordFromQuery(q));
    }
    return result;
}

bool Database::gameExistsByPath(const QString& romPath) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    const QString normalized = QFileInfo(romPath).canonicalFilePath();
    const QString path = normalized.isEmpty() ? romPath : normalized;
    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM games WHERE rom_path = ?");
    q.addBindValue(path);
    if (!q.exec()) {
        qWarning() << "[Database] gameExistsByPath query failed:" << q.lastError().text();
        return false;
    }
    return q.next();
}

bool Database::removeGame(int id) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("DELETE FROM games WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "[Database] removeGame failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::removeGameByPath(const QString& romPath) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    const QString normalized = QFileInfo(romPath).canonicalFilePath();
    const QString path = normalized.isEmpty() ? romPath : normalized;
    QSqlQuery q(db);
    q.prepare("DELETE FROM games WHERE rom_path = ?");
    q.addBindValue(path);
    if (!q.exec()) {
        qWarning() << "[Database] removeGameByPath failed:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

int Database::removeStaleGames() {
    // Targeted SELECT — pulling every column just to check QFileInfo::exists
    // was ~10x more data than needed on large libraries.
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    if (!q.exec("SELECT id, rom_path FROM games")) {
        qWarning() << "[Database] removeStaleGames query failed:" << q.lastError().text();
        return 0;
    }
    QVector<int> staleIds;
    while (q.next()) {
        if (!QFileInfo::exists(q.value(1).toString()))
            staleIds.append(q.value(0).toInt());
    }
    int removed = 0;
    for (int id : staleIds)
        if (removeGame(id)) ++removed;
    if (removed > 0)
        qInfo() << "[Database] Removed" << removed << "stale games";
    return removed;
}

bool Database::updateCoverPath(int id, const QString& coverPath) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET cover_path = ? WHERE id = ?");
    q.addBindValue(coverPath);
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "[Database] updateCoverPath failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateGameMetadata(int id, const GameRecord& metadata) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET title=?, description=?, developer=?, publisher=?, release_date=?, "
              "genres=?, rating=?, players=?, cover_path=?, "
              "screenshot_path=?, titlescreen_path=?, marquee_path=?, fanart_path=?, "
              "box3d_path=?, backcover_path=?, miximage_path=?, physicalmedia_path=?, "
              "manual_path=?, video_path=? WHERE id=?");
    // Null QStrings bind as SQL NULL, violating NOT NULL constraints.
    // Coalesce to empty string for all text fields.
    auto bindText = [&q](const QString& s) { q.addBindValue(s.isNull() ? QString("") : s); };
    bindText(metadata.title);
    bindText(metadata.description);
    bindText(metadata.developer);
    bindText(metadata.publisher);
    bindText(metadata.release_date);
    bindText(metadata.genres);
    q.addBindValue(metadata.rating);
    bindText(metadata.players);
    bindText(metadata.cover_path);
    bindText(metadata.screenshot_path);
    bindText(metadata.titlescreen_path);
    bindText(metadata.marquee_path);
    bindText(metadata.fanart_path);
    bindText(metadata.box3d_path);
    bindText(metadata.backcover_path);
    bindText(metadata.miximage_path);
    bindText(metadata.physicalmedia_path);
    bindText(metadata.manual_path);
    bindText(metadata.video_path);
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "[Database] updateGameMetadata failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateSerial(int id, const QString& serial) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET serial = ? WHERE id = ?");
    q.addBindValue(serial.isNull() ? QString("") : serial);
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "[Database] updateSerial failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QString Database::serialForRomPath(const QString& romPath) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("SELECT serial FROM games WHERE rom_path = ?");
    q.addBindValue(romPath);
    if (!q.exec() || !q.next()) return {};
    return q.value(0).toString();
}

bool Database::updateSerialForRomPath(const QString& romPath, const QString& serial) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET serial = ? "
              "WHERE rom_path = ? AND (serial IS NULL OR serial = '')");
    q.addBindValue(serial);
    q.addBindValue(romPath);
    if (!q.exec()) {
        qWarning() << "[Database] updateSerialForRomPath failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::toggleFavorite(int id) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET favorite = CASE WHEN favorite = 0 THEN 1 ELSE 0 END WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "[Database] toggleFavorite failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::recordGameLaunch(int id) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET play_count = play_count + 1, last_played = ? WHERE id = ?");
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "[Database] recordGameLaunch failed:" << q.lastError().text();
        return false;
    }
    return true;
}

GameRecord Database::gameById(int id) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare(QString("SELECT %1 FROM games WHERE id = ?").arg(GAME_SELECT_COLUMNS));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        qWarning() << "[Database] gameById failed for id:" << id;
        return {};
    }
    return recordFromQuery(q);
}

QMap<QString, int> Database::systemGameCounts() {
    QMap<QString, int> result;
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    if (!q.exec("SELECT system, COUNT(*) FROM games GROUP BY system ORDER BY system")) {
        qWarning() << "[Database] systemGameCounts query failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.insert(q.value(0).toString(), q.value(1).toInt());
    }
    return result;
}

QMap<QString, int> Database::systemFavoriteCounts() {
    QMap<QString, int> result;
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    if (!q.exec("SELECT system, COUNT(*) FROM games WHERE favorite = 1 GROUP BY system ORDER BY system")) {
        qWarning() << "[Database] systemFavoriteCounts query failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.insert(q.value(0).toString(), q.value(1).toInt());
    }
    return result;
}

QStringList Database::allSystems() {
    QStringList result;
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    if (!q.exec("SELECT DISTINCT system FROM games ORDER BY system")) {
        qWarning() << "[Database] allSystems query failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(q.value(0).toString());
    }
    return result;
}
