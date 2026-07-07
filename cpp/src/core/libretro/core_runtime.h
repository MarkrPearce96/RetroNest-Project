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
#include <map>
#include <memory>
#include <mutex>

class HotkeyMatcher;
class SdlInputManager;
class VideoHardwareGL;

class CoreRuntime : public QObject {
    // (sticky wedge flag declared below in the public section)
    Q_OBJECT
public:
    /** True once ANY core in this process reported a wedged shutdown
     *  (retronest_shutdown_wedged). Sticky for the process lifetime.
     *  main.cpp consults it at exit: a wedged core's dylib stays mapped, so
     *  normal exit() would run its static destructors under live detached
     *  threads (observed segv: ~GSTextureCache in __cxa_finalize,
     *  2026-07-03 16:29 crash report) — the process must _exit() instead. */
    static bool anyCoreWedged();

    struct StartConfig {
        QString emuId;
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
        QHash<QString, QString> schemaOptionDefaults;  // {libretro_key → adapter schema default}
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
    /** True iff the worker is currently in paused state (between pause() and
     *  resume()). Thread-safe atomic. Used by hotkey dispatch to toggle. */
    bool isPaused() const { return m_paused.load(); }
    /** Synchronously invoke retro_reset on the running core. No-op if the
     *  core is not loaded. Safe to call from the Qt thread — retro_reset
     *  is not part of the per-frame critical section, but cores generally
     *  expect it from the same thread that drives retro_run. For the
     *  libretro hotkey path we accept the cross-thread call as best-effort:
     *  PCSX2 and mGBA both tolerate it. */
    void reset();
    /** Accessor for the audio sink. Used by AppController's libretro hotkey
     *  routing (Mute / VolumeUp / VolumeDown). The sink lives as long as
     *  CoreRuntime; callers must not store the pointer past stop(). */
    AudioSink* audioSink() { return &m_audio; }
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
     * Asynchronously schedule a libretro controller-port device change (e.g.
     * attach/detach Player 2). Safe to call from any thread; the worker applies
     * it between frames via retro_set_controller_port_device. The latest device
     * for a given port wins. `device` is a RETRO_DEVICE_* id (RETRO_DEVICE_NONE
     * to detach).
     */
    void requestControllerPortDevice(unsigned port, unsigned device);

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
    // Game serial reported by the core via SET_GAME_IDENTITY (empty until set).
    QString detectedGameSerial() const { return QString::fromUtf8(m_envCtx.gameSerial); }
    // Exposed for unit tests (test_core_runtime) so tests can open the loader
    // directly without spawning a worker thread.
    CoreLoader& loader() { return m_loader; }

    /**
     * Register the SdlInputManager that backs this runtime's input. The
     * runtime does not own the pointer; the caller (GameSession) ensures
     * the manager outlives the runtime. Used by the rumble bridge to
     * route libretro's set_rumble_state calls to SDL_GameControllerRumble.
     * Pass nullptr to clear.
     */
    void setSdlInputManager(SdlInputManager* sdl) { m_sdlInput = sdl; }
    SdlInputManager* sdlInputManager() const { return m_sdlInput; }

    /**
     * Explicit injection of the host hotkey matcher (may be nullptr).
     * The input trampoline consults it on the WORKER thread to mask
     * buttons currently acting as the modifier of a matched hotkey
     * combo from the core's view of the gamepad. Injected by
     * GameSession at session start (which gets it from
     * LibretroHotkeyController via AppController) — replaces the old
     * hidden HotkeyMatcher::s_active static.
     */
    void setHotkeyMatcher(HotkeyMatcher* matcher) {
        m_hotkeyMatcher.store(matcher, std::memory_order_relaxed);
    }

    /**
     * Install a libretro hardware render context. Called from the env
     * handler when the core requests RETRO_ENVIRONMENT_SET_HW_RENDER.
     * Returns true if the request was honoured; false if we can't grant
     * (unsupported context type, NSOpenGL creation failure, etc.). On
     * success the runtime owns a VideoHardwareGL instance and the
     * callback struct is mutated in place to point its get_proc_address
     * and get_current_framebuffer fields at our thunks.
     *
     * Called on the libretro worker thread during retro_load_game.
     */
    bool installHwRender(retro_hw_render_callback* cb);

    /** Live VideoHardwareGL when a HW core is loaded; nullptr otherwise.
     *  Caller must not store the pointer past stop(). */
    VideoHardwareGL* videoHW() const { return m_videoHW.get(); }

signals:
    void started();
    void finished(bool crashed);
    void errorOccurred(const QString& message);
    void frameReady(const QImage& frame);
    // Emitted once per session right after retro_get_system_av_info, with
    // the value the core filled into av_info.geometry.aspect_ratio. PCSX2
    // reports 4/3 for PS2 content; mGBA reports the GBA's native 3:2.
    // GameSession routes this into m_libretroAspectRatio so the QML
    // HW-render bridge (LibretroMetalItem.nativeAspect) letterboxes
    // correctly without hardcoding per-core constants.
    void aspectRatioReported(qreal aspectRatio);

    // Emitted when the core surfaces an OSD message via
    // RETRO_ENVIRONMENT_SET_MESSAGE or SET_MESSAGE_EXT. GameSession forwards
    // this onto raInfoToast so the message lands in the standard user-facing
    // toast queue. `durationMs == 0` means "frontend default duration".
    // The bridge runs on the libretro worker thread; the signal crosses
    // into the GUI thread via Qt::AutoConnection (queued).
    void coreMessage(const QString& text, int durationMs);

private:
    SdlInputManager* m_sdlInput = nullptr;
    std::atomic<HotkeyMatcher*> m_hotkeyMatcher{nullptr};

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
    void flushPendingControllerDevices(const CoreSymbols& s);

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

    std::mutex m_portDevMx;
    std::map<unsigned, unsigned> m_pendingPortDevices;  // port -> RETRO_DEVICE_* (latest wins)

    std::atomic<double> m_speedMultiplier{1.0};
    std::atomic<void*> m_active_ns_view{nullptr};

    double m_frameDurationSec = 1.0 / 60.0;
    int m_sampleRate = 48000;

    // Hardware render path — populated by installHwRender() when the
    // core asks for SET_HW_RENDER with a context type we can grant.
    // m_hwRenderCb is the stashed copy of the core-supplied struct
    // (we need context_reset / context_destroy for later lifecycle).
    std::unique_ptr<VideoHardwareGL> m_videoHW;
    retro_hw_render_callback m_hwRenderCb{};
};
