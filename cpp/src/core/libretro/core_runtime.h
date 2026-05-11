#pragma once

#include "core_loader.h"
#include "environment_callbacks.h"
#include "video_software.h"
#include "audio_sink.h"
#include "input_router.h"
#include "options_store.h"
#include "rcheevos_runtime.h"

#include <QObject>
#include <QThread>
#include <QImage>
#include <atomic>
#include <condition_variable>
#include <mutex>

class CoreRuntime : public QObject {
    Q_OBJECT
public:
    struct StartConfig {
        QString corePath;
        QString romPath;
        QString systemDir;
        QString saveDir;
        QString optionsJsonPath;
        QString resumeStatePath;   // optional; if non-empty, retro_unserialize after load
        int raConsoleId = 0;
        QString raUsername;
        QString raToken;
        bool raHardcore = false;
        bool raEncore = false;   // see LibretroRaConfig::encore
    };

    explicit CoreRuntime(QObject* parent = nullptr);
    ~CoreRuntime() override;

    /**
     * Spawn the worker thread and begin libretro lifecycle. Returns false if
     * already running. If start() returns true but the underlying runLoop fails
     * (e.g. dlopen, retro_load_game), `errorOccurred` is emitted followed by
     * `finished(true)`, and stop() must be called before start() can be invoked
     * again.
     */
    bool start(const StartConfig& cfg);
    void stop();
    void pause();
    void resume();
    /**
     * Serialize the current core state to `path`. Caller MUST only call this
     * when the worker thread is NOT running (i.e. after stop() has returned, or
     * before start() has been called). retro_serialize is not reentrant with
     * retro_run; calling this while the worker is active is undefined behaviour.
     * Returns false if the core is not loaded or serialization fails.
     *
     * When a game is actively running, use requestSaveState() instead — it
     * schedules the write onto the worker thread where it is safe.
     */
    bool saveState(const QString& path);

    /**
     * Asynchronously schedule a save-state write. The worker thread will write
     * the state to `path` between frames (or during post-loop teardown, before
     * retro_unload_game). Safe to call from any thread while the worker is
     * running. Returns immediately; the write is guaranteed to complete before
     * the `finished` signal fires. Only the most recently requested path is
     * kept — calling again before the worker services the first request
     * overwrites it.
     */
    void requestSaveState(const QString& path);

    /**
     * Asynchronously schedule a load-state read from `path`. Mirrors
     * requestSaveState — the worker reads the file and calls retro_unserialize
     * between frames. Silently no-ops if the file doesn't exist (e.g. user
     * clicked Load State before ever saving). Safe to call from any thread.
     */
    void requestLoadState(const QString& path);

    /**
     * Set a frame-pacing multiplier. 1.0 = native speed; 4.0 = 4× fast forward.
     * Atomic — safe to call from any thread. The worker reads it each frame
     * to compute its sleep_until interval. Values ≤ 0 are clamped to 1.0.
     */
    void setSpeedMultiplier(double multiplier);

    /**
     * Register a native NSView pointer for the libretro core to consume via
     * RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW. Call before retro_load_game when
     * the active adapter prefers hardware rendering. Pass nullptr to clear.
     *
     * Stored as void* so this header doesn't drag in Objective-C++. The actual
     * NSView* is provided by LibretroMetalItem on macOS.
     */
    void setActiveNSView(void* ns_view);
    void* activeNSView() const;

    InputRouter& input() { return m_input; }
    OptionsStore& options() { return m_options; }
    RcheevosRuntime& rcheevos() { return m_rcheevos; }

signals:
    void started();
    void finished(bool crashed);
    void errorOccurred(const QString& message);
    void frameReady(const QImage& frame);

private:
    static EnvironmentContext* tlsCtx();
    static bool envTrampoline(unsigned cmd, void* data);
    static void videoTrampoline(const void* data, unsigned w, unsigned h, size_t pitch);
    static size_t audioBatchTrampoline(const int16_t* data, size_t frames);
    static void audioSampleTrampoline(int16_t l, int16_t r);
    static void inputPollTrampoline();
    static int16_t inputStateTrampoline(unsigned port, unsigned device,
                                        unsigned index, unsigned id);

    void runLoop();
    void flushPendingSaveState(const CoreSymbols& s);
    void flushPendingLoadState(const CoreSymbols& s);

    StartConfig m_cfg;
    CoreLoader m_loader;
    EnvironmentContext m_envCtx;
    VideoSoftware m_video;
    AudioSink m_audio;
    InputRouter m_input;
    OptionsStore m_options;
    RcheevosRuntime m_rcheevos;

    QThread* m_thread = nullptr;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_paused{false};
    std::mutex m_pauseMx;
    std::condition_variable m_pauseCv;

    std::mutex m_saveMx;
    QString m_pendingSavePath;
    std::mutex m_loadMx;
    QString m_pendingLoadPath;

    std::atomic<double> m_speedMultiplier{1.0};
    std::atomic<void*> m_active_ns_view{nullptr};

    double m_frameDurationSec = 1.0 / 60.0;
    int m_sampleRate = 48000;
};
