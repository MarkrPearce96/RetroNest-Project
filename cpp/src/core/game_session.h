#pragma once

#include "manifest.h"
#include <QImage>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QVariantMap>

class EmulatorAdapter;
class LibretroAdapter;
class SdlInputManager;
class FrontendSettingsStore;

/**
 * LibretroRaConfig — RetroAchievements values needed by the libretro start
 * path. Populated by services before each start() call. Lives in core so
 * GameSession does not need to depend on services/ra_service.h.
 */
struct LibretroRaConfig {
    QString username;
    QString loginToken;   // libretro/rcheevos session token
    QString apiKey;       // web API key (used only for the "no token" warning)
    bool hardcore = false;
    bool encore = false;  // re-fire unlock events for already-earned achievements
    int  consoleId = 0;   // populated per-launch from the adapter
    bool valid = false;   // false → no RA wiring; treated as "not signed in"
};

/**
 * GameSession — manages an async emulator process or libretro core.
 * Only one session at a time. Owned by GameService.
 */
class GameSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString libretroAspectMode READ libretroAspectMode NOTIFY libretroFrontendChanged)
    Q_PROPERTY(bool libretroIntegerScale READ libretroIntegerScale NOTIFY libretroFrontendChanged)
    Q_PROPERTY(QString libretroBackend READ libretroBackend NOTIFY libretroBackendChanged)

public:
    explicit GameSession(QObject* parent = nullptr);
    ~GameSession() override;

    /** Wire up the SDL input manager (called once at app startup). */
    void setSdlInputManager(SdlInputManager* mgr) { m_sdlInputManager = mgr; }

    /** Provide the RA values for the next libretro start. Caller fills the
     *  struct from its RA service; GameSession itself does not depend on
     *  services/. Set `.valid = true` to wire achievements. */
    void setLibretroRaConfig(const LibretroRaConfig& cfg) { m_raConfig = cfg; }

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

    /** Current aspect mode from the libretro frontend settings store.
     *  Returns "native" when no libretro game is running or no store. */
    QString libretroAspectMode() const;
    /** Current integer-scale flag from the libretro frontend settings store. */
    bool libretroIntegerScale() const;
    /** Current render backend for the libretro core: "software" or "metal". */
    QString libretroBackend() const { return m_libretroBackend; }

    /** Pause emulation for the in-game menu. Libretro path: stops the core
     *  thread's retro_run loop and routes SDL events back to QML navigation
     *  by clearing emulation mode. No-op for process emulators (they pause
     *  via focus loss). */
    Q_INVOKABLE void pauseEmulation();

    /** Reverse pauseEmulation. Restores emulation-mode SDL routing and
     *  resumes the core thread. */
    Q_INVOKABLE void resumeEmulation();

    /** Schedule an async save-state write to slot `slot` (1-based) for the
     *  running libretro game. No-op for process emulators (they handle save
     *  state via synthesized hotkeys). The state file path is derived from
     *  the adapter's serial-extraction so it matches resume-state conventions:
     *  `{savestates}/{serial}_slot{N}.state`. */
    Q_INVOKABLE void saveStateLibretro(int slot = 1);

    /** Schedule an async load-state read from slot `slot`. Silent no-op if
     *  no save exists for that slot. Libretro path only. */
    Q_INVOKABLE void loadStateLibretro(int slot = 1);

    /** Toggle 4× fast-forward on the libretro worker. Returns the new state
     *  (true = FF on). No-op for process emulators. */
    Q_INVOKABLE bool toggleFastForwardLibretro();

    /** Register (or clear) the NSView pointer for the active hardware-render
     *  item. Called from QML via LibretroMetalItem.nativeView(). Pass 0 to
     *  clear when the item is destroyed. */
    Q_INVOKABLE void registerHardwareView(qulonglong view_ptr);

    /** The adapter for the currently running emulator. Null if not running. */
    EmulatorAdapter* adapter() const { return m_adapter; }

    /** The manifest for the currently running emulator. */
    const EmulatorManifest* manifest() const { return m_manifest; }

signals:
    void runningChanged();
    void started();
    /** Emitted at the top of startLibretro, BEFORE the libretro core is
     *  dlopened / retro_load_game runs. Used by AppController/AppWindow to
     *  pre-push EmulationView so LibretroMetalItem realises its NSView
     *  before the core queries it via AcquireRenderWindow. The signal fires
     *  for every libretro start (both software and metal backends); the
     *  software path simply doesn't need the NSView. */
    void aboutToStartLibretro();
    void finished(int exitCode, bool crashed);
    void libretroBackendChanged();
    void errorOccurred(const QString& error);
    /** Emitted for each rendered frame (libretro path only). Used by EmulationView. */
    void frameReady(const QImage& frame);
    /** Emitted when a libretro frontend setting (aspect mode, integer scale) changes. */
    void libretroFrontendChanged();
    /** Forwarded from the in-process rcheevos runtime (libretro path only).
     *  GameService re-emits this onto RAService::notifyAchievementUnlocked
     *  via a queued connection set up in service-layer wiring — keeps
     *  core/ free of services/ includes.
     *  `imageUrl` is the unlocked-state badge URL provided by rc_client;
     *  may be empty if the runtime couldn't resolve it. */
    void achievementUnlocked(const QString& id, const QString& title,
                             const QString& description,
                             const QString& imageUrl);
    /** Forwarded from the in-process rcheevos runtime — generic toast
     *  request used for game-start banner, game-mastered celebration,
     *  hardcore reset notice, and server-error notice. AppController
     *  re-emits onto QML via RAService. */
    void raInfoToast(const QString& header, const QString& title,
                     const QString& description, const QString& imageUrl,
                     int durationMs);
    /** Forwarded from the in-process rcheevos runtime — persistent
     *  indicator-bar updates (challenge/progress chips + connection
     *  status). See RcheevosRuntime::raIndicator for the kind/data
     *  contract. */
    void raIndicator(int kind, const QVariantMap& data);

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

    QString libretroSlotPath(int slot) const;

    QProcess* m_process = nullptr;
    EmulatorAdapter* m_adapter = nullptr;
    const EmulatorManifest* m_manifest = nullptr;
    QString m_emuId;
    QString m_currentRomPath;
    LibretroAdapter* m_libretroAdapter = nullptr;
    bool m_libretroFastForward = false;
    QString m_libretroBackend = QStringLiteral("software"); // "software" | "metal"

    // SP10: gate the "switch to Rosetta for full speed" notice to one
    // emission per RetroNest session.
    bool m_slowModeNoticeShown = false;

    SdlInputManager* m_sdlInputManager = nullptr;
    LibretroRaConfig m_raConfig;
};
