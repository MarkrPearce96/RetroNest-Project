#pragma once

#include "manifest.h"
#include <QImage>
#include <QObject>
#include <QProcess>

class EmulatorAdapter;
class LibretroAdapter;
class SdlInputManager;
class RAService;

/**
 * GameSession — manages an async emulator process or libretro core.
 * Only one session at a time. Owned by GameService.
 */
class GameSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    explicit GameSession(QObject* parent = nullptr);
    ~GameSession() override;

    /** Wire up the SDL input manager (called once at app startup). */
    void setSdlInputManager(SdlInputManager* mgr) { m_sdlInputManager = mgr; }

    /** Wire up the RA service (called once at app startup). */
    void setRaService(RAService* svc) { m_raService = svc; }

    /** Launch the emulator. Returns false if already running or start fails. */
    bool start(const EmulatorManifest& manifest,
               EmulatorAdapter* adapter,
               const QString& romPath,
               const QStringList& extraArgs = {});

    /** Kill the emulator process immediately (SIGKILL / runtime stop). */
    void kill();

    /** Terminate the emulator process gracefully (SIGTERM / save-on-quit + stop). */
    void terminate();

    bool isRunning() const;
    qint64 pid() const;

    /** True when the running game uses the libretro (in-process) backend. */
    bool isLibretro() const { return m_backend == Backend::Libretro; }

    /** Pause emulation for the in-game menu. Libretro path: stops the core
     *  thread's retro_run loop and routes SDL events back to QML navigation
     *  by clearing emulation mode. No-op for process emulators (they pause
     *  via focus loss). */
    Q_INVOKABLE void pauseEmulation();

    /** Reverse pauseEmulation. Restores emulation-mode SDL routing and
     *  resumes the core thread. */
    Q_INVOKABLE void resumeEmulation();

    /** The adapter for the currently running emulator. Null if not running. */
    EmulatorAdapter* adapter() const { return m_adapter; }

    /** The manifest for the currently running emulator. */
    const EmulatorManifest* manifest() const { return m_manifest; }

signals:
    void runningChanged();
    void started();
    void finished(int exitCode, bool crashed);
    void errorOccurred(const QString& error);
    /** Emitted for each rendered frame (libretro path only). Used by EmulationView. */
    void frameReady(const QImage& frame);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyRead();

private:
    enum class Backend { Process, Libretro };
    Backend m_backend = Backend::Process;

    bool startProcess(const EmulatorManifest& manifest,
                      EmulatorAdapter* adapter,
                      const QString& romPath,
                      const QStringList& extraArgs);
    bool startLibretro(const EmulatorManifest& manifest,
                       EmulatorAdapter* adapter,
                       const QString& romPath);

    QProcess* m_process = nullptr;
    EmulatorAdapter* m_adapter = nullptr;
    const EmulatorManifest* m_manifest = nullptr;
    QString m_emuId;
    QString m_currentRomPath;
    LibretroAdapter* m_libretroAdapter = nullptr;

    SdlInputManager* m_sdlInputManager = nullptr;
    RAService* m_raService = nullptr;
};
