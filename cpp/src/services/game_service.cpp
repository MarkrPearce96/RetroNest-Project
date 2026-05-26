#include "game_service.h"
#include "rom_scanner.h"
#include "core/paths.h"
#include "adapters/adapter_registry.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QtConcurrent>

GameService::GameService(ManifestLoader* loader, Database* db, QObject* parent)
    : QObject(parent), m_loader(loader), m_db(db)
{
    connect(&m_session, &GameSession::started, this, [this]() {
        emit gameRunningChanged();
        emit gameStarted();
        // Lazily fill the DB serial for formats the scanner couldn't read (RVZ:
        // EmulatorAdapter::extractSerial fails on the compressed header). The core
        // reports it via SET_GAME_IDENTITY; updateSerialForRomPath only writes
        // when the stored serial is still empty.
        const QString detectedSerial = m_session.detectedGameSerial();
        if (!detectedSerial.isEmpty())
            m_db->updateSerialForRomPath(m_currentRomPath, detectedSerial);
    });

    connect(&m_session, &GameSession::finished, this, [this](int exitCode, bool crashed) {
        m_terminateTimer.stop();

        if (!m_pendingSaveRomPath.isEmpty()) {
            if (crashed)
                qWarning() << "[GameService] Game crashed during save-and-quit";
            else
                qInfo() << "[GameService] Save-and-quit completed for" << m_pendingSaveRomPath;
            m_pendingSaveRomPath.clear();
        }

        m_currentRomPath.clear();
        emit gameRunningChanged();
        emit gameFinished(exitCode, crashed);
        if (crashed)
            emit statusMessage("Emulator crashed");
        else
            emit statusMessage("Game exited (code " + QString::number(exitCode) + ")");
    });

    connect(&m_session, &GameSession::errorOccurred, this, [this](const QString& error) {
        emit gameError(error);
        emit statusMessage("Launch failed: " + error);
    });

    // Escalation timer: SIGTERM → SIGKILL after timeout
    m_terminateTimer.setSingleShot(true);
    connect(&m_terminateTimer, &QTimer::timeout, this, [this]() {
        if (m_session.isRunning()) {
            qWarning() << "[GameService] Emulator did not exit after SIGTERM, sending SIGKILL";
            m_session.kill();
        }
    });
}

GameService::ImportResult GameService::importRoms(const QString& directory, const QString& systemFilter) {
    emit statusMessage("Scanning: " + directory + "...");

    auto result = RomScanner::scan(directory, *m_loader, *m_db, systemFilter);

    return {
        result.added,
        result.skipped,
        QString("Import complete: %1 added, %2 skipped").arg(result.added).arg(result.skipped)
    };
}


GameService::ImportResult GameService::scanRomFolders() {
    emit statusMessage("Scanning ROM folders...");

    // Remove games whose ROM files no longer exist on disk
    const int removed = m_db->removeStaleGames();

    auto result = RomScanner::scanStructured(Paths::romsDir(), *m_loader, *m_db);

    QString msg = QString("Scan complete: %1 added, %2 skipped").arg(result.added).arg(result.skipped);
    if (removed > 0)
        msg += QString(", %1 removed").arg(removed);

    return {result.added, result.skipped, msg};
}

void GameService::removeGame(int gameId, bool deleteRomFile) {
    if (deleteRomFile) {
        GameRecord g = m_db->gameById(gameId);
        if (!g.rom_path.isEmpty() && QFile::exists(g.rom_path)) {
            if (!QFile::remove(g.rom_path)) {
                qWarning() << "[GameService] Failed to delete ROM file:" << g.rom_path;
            }
        }
    }
    m_db->removeGame(gameId);
}

QStringList GameService::importableSystems() const {
    QStringList systems;
    systems << "Auto-detect";
    for (const auto& emu : m_loader->allEmulators()) {
        for (const auto& sys : emu.systems) {
            if (!systems.contains(sys))
                systems << sys;
        }
    }
    return systems;
}

bool GameService::isGameRunning() const {
    return m_session.isRunning();
}

bool GameService::startGame(const QString& romPath, const QString& emuId,
                            const QStringList& extraArgs) {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest) {
        emit gameError("No manifest for: " + emuId);
        return false;
    }

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) {
        emit gameError("No adapter for: " + emuId);
        return false;
    }

    if (!adapter->isInstalled(*manifest)) {
        emit gameError(manifest->name + " not installed. Go to Settings > Emulators to install.");
        return false;
    }

    if (!QFileInfo::exists(romPath)) {
        emit gameError("ROM not found: " + romPath);
        return false;
    }

    m_currentRomPath = romPath;
    emit statusMessage("Launching: " + QFileInfo(romPath).completeBaseName());

    if (!m_session.start(*manifest, adapter, romPath, extraArgs)) {
        m_currentRomPath.clear();
        return false;
    }

    return true;
}

void GameService::stopGame() {
    m_session.kill();
}

void GameService::saveAndStopGame(int /*slot*/) {
    m_pendingSaveRomPath = m_currentRomPath;
    qInfo() << "[GameService] Terminating emulator (SIGTERM) for save-on-shutdown";
    m_session.terminate();
    m_terminateTimer.start(10000);
}

bool GameService::hasResumeState(const QString& romPath, const QString& emuId) const {
    return !resumeStateFile(romPath, emuId).isEmpty();
}

QString GameService::resumeStateFile(const QString& romPath, const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    // Key by the DB serial, or the ROM base name when no serial is known (e.g.
    // RVZ discs the host can't parse). This MUST mirror the save side
    // (GameSession::terminate) and the launch side (GameSession::prepareConfig),
    // which key the .resume file the same way — otherwise hasResumeState() and
    // clearResumeState() miss the file, so the Resume/Start-Fresh dialog never
    // appears for serial-less titles even though the launch path resumes them.
    QString serial = m_db->serialForRomPath(romPath);
    const QString key =
        serial.isEmpty() ? QFileInfo(romPath).completeBaseName() : serial;
    return adapter->findResumeFile(key);
}

void GameService::clearResumeState(const QString& romPath, const QString& emuId) {
    const QString stateFile = resumeStateFile(romPath, emuId);
    if (!stateFile.isEmpty() && QFile::exists(stateFile)) {
        QFile::remove(stateFile);
        qInfo() << "[GameService] Deleted resume state file:" << stateFile;
    }
}

void GameService::backfillSerials() {
    // Snapshot the games whose serials need filling on the GUI thread
    // (database access is single-threaded).
    auto allGames = m_db->allGames();
    struct PendingExtract { int id; QString rom_path; QString emuId; };
    QVector<PendingExtract> pending;
    for (const auto& g : allGames) {
        if (!g.serial.isEmpty()) continue;
        pending.append({g.id, g.rom_path, g.emulator_id});
    }
    if (pending.isEmpty()) return;

    // Run the heavy ISO/SFO reads on a worker thread; queue each DB write
    // back to the main thread one at a time. Pattern matches ScraperService.
    Database* db = m_db;
    QtConcurrent::run([pending, db]() {
        int filled = 0;
        for (const auto& p : pending) {
            auto* adapter = AdapterRegistry::instance().adapterFor(p.emuId);
            if (!adapter) continue;
            QString serial = adapter->extractSerial(p.rom_path);
            if (serial.isEmpty()) continue;
            QMetaObject::invokeMethod(qApp, [db, id = p.id, serial]() {
                db->updateSerial(id, serial);
            }, Qt::QueuedConnection);
            ++filled;
        }
        if (filled > 0) {
            QMetaObject::invokeMethod(qApp, [filled]() {
                qInfo() << "[GameService] Backfilled serials for" << filled << "games";
            }, Qt::QueuedConnection);
        }
    });
}
