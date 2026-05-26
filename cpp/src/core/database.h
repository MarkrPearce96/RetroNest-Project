#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct GameRecord {
    int id = 0;
    QString title;
    QString rom_path;
    QString system;       // e.g. "psx", "ps2"
    QString emulator_id;  // manifest id that handles this system
    QString cover_path;

    // Rich metadata (populated by scraper)
    QString description;
    QString developer;
    QString publisher;
    QString release_date;
    QString genres;       // comma-separated
    double  rating = 0.0; // 0.0–5.0
    QString players;

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

    int disc_count = 0;  // number of discs (0 = single-disc or non-M3U)
    QString serial;      // game serial (e.g. "SLUS_200.62"), extracted from ROM

    // User data
    QString last_played;
    int     play_count = 0;
    int     favorite = 0;
};

/**
 * Database — SQLite storage for games and emulator install state.
 * One file at {root}/config/retronest.db.
 */
class Database {
public:
    bool open(const QString& dbPath);
    void close();

    // Games
    /** Returns: positive ID on success, 0 if duplicate (skipped), -1 on SQL error. */
    int addGame(const GameRecord& game);
    QVector<GameRecord> allGames();
    QVector<GameRecord> gamesBySystem(const QString& system);
    bool gameExistsByPath(const QString& romPath);
    bool removeGame(int id);
    bool removeGameByPath(const QString& romPath);
    int removeStaleGames();  // removes games whose ROM files no longer exist
    bool updateCoverPath(int id, const QString& coverPath);
    bool updateGameMetadata(int id, const GameRecord& metadata);
    bool updateSerial(int id, const QString& serial);
    QString serialForRomPath(const QString& romPath);
    // Set the serial for the game at romPath, but only if it currently has none
    // (used to lazily fill serials the scanner couldn't read, e.g. RVZ).
    bool updateSerialForRomPath(const QString& romPath, const QString& serial);
    bool toggleFavorite(int id);
    bool recordGameLaunch(int id);
    GameRecord gameById(int id);
    QMap<QString, int> systemGameCounts();
    QMap<QString, int> systemFavoriteCounts();
    QStringList allSystems();

private:
    bool createTables();
    bool runMigrations();
    int schemaVersion();
    bool setSchemaVersion(int version);
    /** Copy the current DB file to "<dbPath>.bak-v<N>" before any migration
     *  runs, so a partial-failure leaves the user with a recoverable copy. */
    bool backupBeforeMigration(int fromVersion);

    QString m_dbPath;  // remembered between open() and runMigrations() for backup
};
