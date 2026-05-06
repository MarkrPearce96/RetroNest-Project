#include "game_session.h"
#include "paths.h"
#include "adapters/emulator_adapter.h"
#include "adapters/libretro/libretro_adapter.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

GameSession::GameSession(QObject* parent)
    : QObject(parent) {}

GameSession::~GameSession() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool GameSession::start(const EmulatorManifest& manifest,
                        EmulatorAdapter* adapter,
                        const QString& romPath,
                        const QStringList& extraArgs) {
    if (isRunning()) {
        qWarning() << "[GameSession] Already running";
        return false;
    }
    if (!QFileInfo::exists(romPath)) {
        emit errorOccurred("ROM file not found: " + romPath);
        return false;
    }

    const QString systemId = Paths::systemIdFor(manifest.id, manifest.systems);
    const QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
    const QString dataPath = QFileInfo(Paths::emulatorDataDir(manifest.id, systemId)).absoluteFilePath();
    QDir().mkpath(dataPath);

    if (!adapter->ensureConfig(manifest, biosPath, dataPath))
        qWarning() << "[GameSession] ensureConfig failed for" << manifest.name;

    m_adapter = adapter;
    m_manifest = &manifest;
    m_emuId = manifest.id;
    m_currentRomPath = romPath;

    if (manifest.backend == "libretro") {
        m_backend = Backend::Libretro;
        return startLibretro(manifest, adapter, romPath);
    }
    m_backend = Backend::Process;
    return startProcess(manifest, adapter, romPath, extraArgs);
}

bool GameSession::startProcess(const EmulatorManifest& manifest,
                               EmulatorAdapter* adapter,
                               const QString& romPath,
                               const QStringList& extraArgs) {
    // Resolve executable
    const QString installPath = Paths::emulatorsDir(manifest.install_folder);
    const QString execPath = QFileInfo(adapter->resolveExecutable(manifest, installPath)).absoluteFilePath();

    if (!QFileInfo::exists(execPath)) {
        emit errorOccurred(manifest.name + " is not installed. Executable not found: " + execPath);
        return false;
    }

    // Build arguments
    QStringList args = adapter->buildLaunchArgs(manifest, romPath);
    if (!extraArgs.isEmpty()) {
        // Insert extra args before "--" separator (if present) so they're
        // treated as flags, not positional arguments / filenames
        int sepIdx = args.indexOf("--");
        if (sepIdx >= 0) {
            for (int i = extraArgs.size() - 1; i >= 0; --i)
                args.insert(sepIdx, extraArgs[i]);
        } else {
            args.append(extraArgs);
        }
    }

    // Resolve working directory
    QString cwd;
#if defined(Q_OS_MACOS)
    static const QRegularExpression appRe("^(.+\\.app)/");
    auto match = appRe.match(execPath);
    if (match.hasMatch()) {
        cwd = QFileInfo(match.captured(1)).absolutePath();
    }
#endif
    if (cwd.isEmpty()) {
        cwd = QFileInfo(execPath).absolutePath();
    }

    // Create and configure process
    delete m_process;
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(cwd);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::finished, this, &GameSession::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GameSession::onProcessError);
    connect(m_process, &QProcess::readyRead, this, &GameSession::onReadyRead);

    qInfo().noquote() << "[GameSession]" << manifest.name << ":" << execPath << args.join(" ");
    qInfo().noquote() << "[GameSession] CWD:" << cwd;

    m_process->start(execPath, args);

    if (!m_process->waitForStarted(5000)) {
        emit errorOccurred("Failed to start process: " + m_process->errorString());
        return false;
    }

    qInfo() << "[GameSession] PID:" << m_process->processId();
    emit runningChanged();
    emit started();
    return true;
}

bool GameSession::startLibretro(const EmulatorManifest& manifest,
                                EmulatorAdapter* adapter,
                                const QString& romPath) {
    auto* lr = dynamic_cast<LibretroAdapter*>(adapter);
    if (!lr) { emit errorOccurred("Adapter is not LibretroAdapter"); return false; }
    m_libretroAdapter = lr;
    lr->prepareRuntime();
    auto* rt = lr->runtime();

    CoreRuntime::StartConfig cfg;
    cfg.corePath = lr->resolveExecutable(manifest, Paths::emulatorsDir(manifest.install_folder));
    cfg.romPath = romPath;
    cfg.systemDir = Paths::biosDir();
    const QString systemId = Paths::systemIdFor(manifest.id, manifest.systems);
    cfg.saveDir = Paths::emulatorDataDir(manifest.id, systemId);
    cfg.optionsJsonPath = Paths::emulatorsDir("libretro") + "/" + lr->coreId() + "/options.json";

    connect(rt, &CoreRuntime::started, this, [this] {
        emit runningChanged(); emit started();
    }, Qt::UniqueConnection);
    connect(rt, &CoreRuntime::finished, this, [this](bool crashed) {
        m_adapter = nullptr; m_manifest = nullptr;
        emit runningChanged();
        emit finished(crashed ? -1 : 0, crashed);
        if (m_libretroAdapter) m_libretroAdapter->releaseRuntime();
        m_libretroAdapter = nullptr;
    }, Qt::UniqueConnection);
    connect(rt, &CoreRuntime::errorOccurred, this, [this](const QString& m) {
        emit errorOccurred(m);
    }, Qt::UniqueConnection);
    connect(rt, &CoreRuntime::frameReady, this, &GameSession::frameReady,
            Qt::UniqueConnection);

    return rt->start(cfg);
}

void GameSession::kill() {
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->stop();
    else if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Killing emulator process";
        m_process->kill();
    }
}

void GameSession::terminate() {
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime()) {
        // Save-on-quit: pause the runtime, write resume file, then stop
        const auto* mf = m_manifest;
        if (mf) {
            const QString systemId = Paths::systemIdFor(mf->id, mf->systems);
            const QString romBase = QFileInfo(m_currentRomPath).completeBaseName();
            const QString resumePath = Paths::emulatorDataDir(mf->id, systemId)
                + "/savestates/" + romBase + ".resume";
            QDir().mkpath(QFileInfo(resumePath).absolutePath());
            // saveState requires the runtime to be paused (per Task 7.1 fix)
            m_libretroAdapter->runtime()->pause();
            m_libretroAdapter->runtime()->saveState(resumePath);
        }
        m_libretroAdapter->runtime()->stop();
    } else if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Terminating emulator process (SIGTERM)";
        m_process->terminate();
    }
}

bool GameSession::isRunning() const {
    if (m_backend == Backend::Libretro)
        return m_libretroAdapter && m_libretroAdapter->runtime();
    return m_process && m_process->state() != QProcess::NotRunning;
}

qint64 GameSession::pid() const {
    return m_process ? m_process->processId() : -1;
}

void GameSession::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    bool crashed = (exitStatus == QProcess::CrashExit);
    if (crashed) {
        qWarning() << "[GameSession]" << m_emuId << "crashed.";
    } else {
        qInfo() << "[GameSession]" << m_emuId << "exited with code" << exitCode;
    }
    m_adapter = nullptr;
    m_manifest = nullptr;
    emit runningChanged();
    emit finished(exitCode, crashed);
}

void GameSession::onProcessError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        emit errorOccurred("Process failed to start: " + m_process->errorString());
        m_adapter = nullptr;
        m_manifest = nullptr;
        emit runningChanged();
    }
}

void GameSession::onReadyRead() {
    QByteArray output = m_process->readAll();
    for (const auto& line : output.split('\n')) {
        if (!line.trimmed().isEmpty()) {
            qInfo().noquote() << "  [" + m_emuId + "]" << line.trimmed();
        }
    }
}
