#include "game_session.h"
#include "paths.h"
#include "adapters/emulator_adapter.h"

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
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qWarning() << "[GameSession] Already running, cannot start another";
        return false;
    }

    // Verify ROM exists
    if (!QFileInfo::exists(romPath)) {
        emit errorOccurred("ROM file not found: " + romPath);
        return false;
    }

    // Resolve executable
    const QString installPath = Paths::emulatorsDir(manifest.install_folder);
    const QString execPath = QFileInfo(adapter->resolveExecutable(manifest, installPath)).absoluteFilePath();

    if (!QFileInfo::exists(execPath)) {
        emit errorOccurred(manifest.name + " is not installed. Executable not found: " + execPath);
        return false;
    }

    // Ensure config
    const QString systemId = Paths::systemIdFor(manifest.id, manifest.systems);
    const QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
    const QString savesPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath();
    QDir().mkpath(savesPath);

    if (!adapter->ensureConfig(manifest, biosPath, savesPath)) {
        qWarning() << "[GameSession] Config creation/patching failed for" << manifest.name;
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

    // Store state
    m_adapter = adapter;
    m_manifest = &manifest;
    m_emuId = manifest.id;

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

void GameSession::kill() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Killing emulator process";
        m_process->kill();
    }
}

void GameSession::terminate() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Terminating emulator process (SIGTERM)";
        m_process->terminate();
    }
}

bool GameSession::isRunning() const {
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
