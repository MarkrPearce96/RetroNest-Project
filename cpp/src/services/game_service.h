#pragma once

#include "core/manifest_loader.h"
#include "core/database.h"
#include "core/game_session.h"
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class CoreRuntime;

/**
 * GameService — orchestrates import, remove, and launch workflows.
 * No UI dependencies — emits signals for status updates.
 */
class GameService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)

public:
    GameService(ManifestLoader* loader, Database* db, QObject* parent = nullptr);

    struct ImportResult {
        int added = 0;
        int skipped = 0;
        QString message;
    };

    /** Import ROMs from a directory, optionally filtered to a system. */
    ImportResult importRoms(const QString& directory, const QString& systemFilter);

    /** Scan per-system ROM subdirectories under Paths::romsDir(). */
    ImportResult scanRomFolders();

    /** Backfill serial numbers for games that have none (e.g. after schema upgrade). */
    void backfillSerials();

    /** Remove a game from the database, optionally deleting the ROM file from disk. */
    void removeGame(int gameId, bool deleteRomFile = false);

    /** Get available system names for import dialog. */
    QStringList importableSystems() const;

    /** Start a game asynchronously. Returns false if start fails
     *  immediately. `gameId` is the DB id — recorded as the session
     *  identity so currentGameInfo() needn't rescan the DB (0 if
     *  unknown, e.g. a re-launch after the RA login prompt). */
    bool startGame(int gameId, const QString& romPath, const QString& emuId);

    /** Check if a resume save state exists for this ROM via serial-based detection. */
    bool hasResumeState(const QString& romPath, const QString& emuId) const;

    /** Get the resume state file path for a ROM via serial-based detection. */
    QString resumeStateFile(const QString& romPath, const QString& emuId) const;

    /** Delete the resume state file for a ROM. */
    void clearResumeState(const QString& romPath, const QString& emuId);

    /** Stop the running game (kill process). */
    void stopGame();

    /** Save state to slot and then stop. */
    void saveAndStopGame(int slot);

    bool isGameRunning() const;

    /** Return the ROM path of the currently/last running game. */
    QString currentRomPath() const { return m_currentRomPath; }

    /** Identity + capability flags of the running game, built from the
     *  record cached at launch — no per-call DB scan (review P6).
     *  Empty when nothing is running. */
    QVariantMap currentGameInfo() const;

    /** The active libretro core runtime, or nullptr (review P6). */
    CoreRuntime* libretroRuntime() const { return m_session.libretroRuntime(); }

    /** Access the game session (for adapter control). */
    GameSession* session() { return &m_session; }

signals:
    void statusMessage(const QString& msg);
    void gameRunningChanged();
    void gameStarted();
    void gameFinished(int exitCode, bool crashed);
    void gameError(const QString& error);

private:
    ManifestLoader* m_loader;
    Database* m_db;
    GameSession m_session;
    QString m_currentRomPath;       // ROM path of the running game
    GameRecord m_currentGame;       // DB record cached at launch (identity)
    QString m_pendingSaveRomPath;   // ROM path when save-and-quit is in progress
    QTimer m_terminateTimer;        // Escalates SIGTERM → SIGKILL after timeout
};
