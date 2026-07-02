// Migration matrix for Database::open()/runMigrations().
//
// Regression focus: the pre-2026-07 scheme committed each migration step in
// its own transaction but stamped schema_version once at the END — a crash
// between steps left applied DDL with a stale version stamp, and every
// subsequent open() died on "duplicate column" (a bricked database). Steps now
// stamp their target version inside their own transaction AND skip ADD COLUMN
// statements whose column already exists, so both legit intermediate states
// and historically-damaged databases open cleanly.
//
// Fixtures are built with raw SQL on a scratch connection, matching the real
// schema at each historical version.

#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>
#include <QTemporaryDir>

#include "core/database.h"

namespace {

const char* kFixtureConn = "migration_fixture";

// v1 schema: the games table before any migration.
const QStringList kV1Schema = {
    "CREATE TABLE schema_version (version INTEGER NOT NULL)",
    "INSERT INTO schema_version (version) VALUES (1)",
    "CREATE TABLE games ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  title TEXT NOT NULL DEFAULT '',"
    "  rom_path TEXT NOT NULL UNIQUE,"
    "  system TEXT NOT NULL DEFAULT '',"
    "  emulator_id TEXT NOT NULL DEFAULT '',"
    "  cover_path TEXT NOT NULL DEFAULT ''"
    ")",
};

// Runs `statements` against a fresh SQLite file at `path`.
bool execFixture(const QString& path, const QStringList& statements)
{
    bool ok = true;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", kFixtureConn);
        db.setDatabaseName(path);
        if (!db.open()) {
            QSqlDatabase::removeDatabase(kFixtureConn);
            return false;
        }
        QSqlQuery q(db);
        for (const auto& sql : statements) {
            if (!q.exec(sql)) {
                qWarning() << "fixture SQL failed:" << q.lastError().text() << sql;
                ok = false;
                break;
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(kFixtureConn);
    return ok;
}

// Reads column names of `games` and the stored schema version from the file.
struct DbInspection {
    QSet<QString> columns;
    int version = -1;
};

DbInspection inspect(const QString& path)
{
    DbInspection r;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", kFixtureConn);
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery q(db);
            if (q.exec("PRAGMA table_info(games)"))
                while (q.next())
                    r.columns.insert(q.value(1).toString());
            if (q.exec("SELECT version FROM schema_version LIMIT 1") && q.next())
                r.version = q.value(0).toInt();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(kFixtureConn);
    return r;
}

int countGamesWithEmulatorId(const QString& path, const QString& emuId)
{
    int n = -1;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", kFixtureConn);
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery q(db);
            q.prepare("SELECT COUNT(*) FROM games WHERE emulator_id = ?");
            q.addBindValue(emuId);
            if (q.exec() && q.next())
                n = q.value(0).toInt();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(kFixtureConn);
    return n;
}

// All columns the fully-migrated (v7) games table must have.
const QSet<QString> kV7Columns = {
    "id", "title", "rom_path", "system", "emulator_id", "cover_path",
    "description", "developer", "publisher", "release_date", "genres",
    "rating", "players", "last_played", "play_count", "favorite",
    "screenshot_path", "titlescreen_path", "marquee_path", "fanart_path",
    "box3d_path", "backcover_path", "miximage_path", "physicalmedia_path",
    "manual_path", "video_path", "disc_count", "serial",
};

} // namespace

class TestDatabaseMigrations : public QObject {
    Q_OBJECT

private slots:
    void freshDatabaseStampsCurrentVersion();
    void v1MigratesToCurrentAndPreservesData();
    void halfAppliedStepSelfHeals();
    void intermediateVersionResumes();
    void v6RewritesPcsx2LibretroId();
    void backupWrittenBeforeMigration();
    void newerVersionLeftUntouched();
};

void TestDatabaseMigrations::freshDatabaseStampsCurrentVersion()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("retronest.db");

    Database db;
    QVERIFY(db.open(path));
    db.close();

    const auto ins = inspect(path);
    QCOMPARE(ins.version, 7);
    QCOMPARE(ins.columns, kV7Columns);
}

void TestDatabaseMigrations::v1MigratesToCurrentAndPreservesData()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("retronest.db");
    QStringList fixture = kV1Schema;
    fixture << "INSERT INTO games (title, rom_path, system, emulator_id) "
               "VALUES ('Crash', '/roms/crash.cue', 'psx', 'duckstation')";
    QVERIFY(execFixture(path, fixture));

    Database db;
    QVERIFY(db.open(path));
    const auto games = db.allGames();
    db.close();

    const auto ins = inspect(path);
    QCOMPARE(ins.version, 7);
    QCOMPARE(ins.columns, kV7Columns);
    QCOMPARE(games.size(), 1);
    QCOMPARE(games[0].title, QStringLiteral("Crash"));
    QCOMPARE(games[0].rom_path, QStringLiteral("/roms/crash.cue"));
}

void TestDatabaseMigrations::halfAppliedStepSelfHeals()
{
    // The historical brick: the v1→v2 step crashed after adding SOME columns,
    // and (under the old scheme) the version stamp never moved. Reopening used
    // to die on "duplicate column: description" forever.
    QTemporaryDir dir;
    const QString path = dir.filePath("retronest.db");
    QStringList fixture = kV1Schema;
    fixture << "ALTER TABLE games ADD COLUMN description TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN developer TEXT NOT NULL DEFAULT ''";
    QVERIFY(execFixture(path, fixture));

    Database db;
    QVERIFY(db.open(path));  // must not brick
    db.close();

    const auto ins = inspect(path);
    QCOMPARE(ins.version, 7);
    QCOMPARE(ins.columns, kV7Columns);
}

void TestDatabaseMigrations::intermediateVersionResumes()
{
    // Legit intermediate state under the per-step-stamp scheme: v2+v3 applied
    // and stamped 3, then a crash before the v4 step. Reopening must resume
    // from v3 and complete.
    QTemporaryDir dir;
    const QString path = dir.filePath("retronest.db");
    QStringList fixture = kV1Schema;
    fixture << "ALTER TABLE games ADD COLUMN description TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN developer TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN publisher TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN release_date TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN genres TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN rating REAL NOT NULL DEFAULT 0.0"
            << "ALTER TABLE games ADD COLUMN players TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN last_played TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN play_count INTEGER NOT NULL DEFAULT 0"
            << "ALTER TABLE games ADD COLUMN favorite INTEGER NOT NULL DEFAULT 0"
            << "ALTER TABLE games ADD COLUMN screenshot_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN titlescreen_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN marquee_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN fanart_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN box3d_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN backcover_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN miximage_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN physicalmedia_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN manual_path TEXT NOT NULL DEFAULT ''"
            << "ALTER TABLE games ADD COLUMN video_path TEXT NOT NULL DEFAULT ''"
            << "UPDATE schema_version SET version = 3";
    QVERIFY(execFixture(path, fixture));

    Database db;
    QVERIFY(db.open(path));
    db.close();

    const auto ins = inspect(path);
    QCOMPARE(ins.version, 7);
    QVERIFY(ins.columns.contains("disc_count"));
    QVERIFY(ins.columns.contains("serial"));
}

void TestDatabaseMigrations::v6RewritesPcsx2LibretroId()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("retronest.db");
    // Full v6 schema = v7 columns (v6→v7 is data-only), stamped 6.
    QStringList fixture = kV1Schema;
    for (const auto& col : kV7Columns) {
        if (col == "id" || col == "title" || col == "rom_path" || col == "system" ||
            col == "emulator_id" || col == "cover_path")
            continue;
        const bool numeric = (col == "rating" || col == "play_count" ||
                              col == "favorite" || col == "disc_count");
        fixture << QString("ALTER TABLE games ADD COLUMN %1 %2")
                       .arg(col, numeric ? "INTEGER NOT NULL DEFAULT 0"
                                         : "TEXT NOT NULL DEFAULT ''");
    }
    fixture << "INSERT INTO games (title, rom_path, emulator_id) "
               "VALUES ('GT4', '/roms/gt4.iso', 'pcsx2-libretro')"
            << "INSERT INTO games (title, rom_path, emulator_id) "
               "VALUES ('MGS', '/roms/mgs.cue', 'duckstation')"
            << "UPDATE schema_version SET version = 6";
    QVERIFY(execFixture(path, fixture));

    Database db;
    QVERIFY(db.open(path));
    db.close();

    QCOMPARE(inspect(path).version, 7);
    QCOMPARE(countGamesWithEmulatorId(path, "pcsx2-libretro"), 0);
    QCOMPARE(countGamesWithEmulatorId(path, "pcsx2"), 1);
    QCOMPARE(countGamesWithEmulatorId(path, "duckstation"), 1);  // untouched
}

void TestDatabaseMigrations::backupWrittenBeforeMigration()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("retronest.db");
    QVERIFY(execFixture(path, kV1Schema));

    Database db;
    QVERIFY(db.open(path));
    db.close();

    QVERIFY(QFileInfo::exists(path + ".bak-v1"));
    // The backup must still be at the pre-migration schema.
    QCOMPARE(inspect(path + ".bak-v1").version, 1);
}

void TestDatabaseMigrations::newerVersionLeftUntouched()
{
    // A database from a future build: no migration step applies, and the
    // stamp must not be rewound.
    QTemporaryDir dir;
    const QString path = dir.filePath("retronest.db");
    QStringList fixture = kV1Schema;
    fixture << "UPDATE schema_version SET version = 99";
    QVERIFY(execFixture(path, fixture));

    Database db;
    QVERIFY(db.open(path));
    db.close();

    QCOMPARE(inspect(path).version, 99);
}

QTEST_GUILESS_MAIN(TestDatabaseMigrations)
#include "test_database_migrations.moc"
