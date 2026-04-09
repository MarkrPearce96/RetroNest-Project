#pragma once

#include "manifest.h"
#include <QObject>
#include <QProcess>

class EmulatorAdapter;

/**
 * GameSession — manages an async emulator process.
 * Only one session at a time. Owned by GameService.
 */
class GameSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    explicit GameSession(QObject* parent = nullptr);
    ~GameSession() override;

    /** Launch the emulator. Returns false if already running or start fails. */
    bool start(const EmulatorManifest& manifest,
               EmulatorAdapter* adapter,
               const QString& romPath,
               const QStringList& extraArgs = {});

    /** Kill the emulator process immediately (SIGKILL). */
    void kill();

    /** Terminate the emulator process gracefully (SIGTERM). */
    void terminate();

    bool isRunning() const;
    qint64 pid() const;

    /** The adapter for the currently running emulator. Null if not running. */
    EmulatorAdapter* adapter() const { return m_adapter; }

    /** The manifest for the currently running emulator. */
    const EmulatorManifest* manifest() const { return m_manifest; }

signals:
    void runningChanged();
    void started();
    void finished(int exitCode, bool crashed);
    void errorOccurred(const QString& error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyRead();

private:
    QProcess* m_process = nullptr;
    EmulatorAdapter* m_adapter = nullptr;
    const EmulatorManifest* m_manifest = nullptr;
    QString m_emuId;
};
