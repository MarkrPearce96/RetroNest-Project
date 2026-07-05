#include "core_runtime.h"
#include "core/libretro/autorelease_scope.h"
#include "core/libretro/hotkey_matcher.h"
#include "core/libretro/video_hardware_gl.h"
#include "core/path_overrides_store.h"
#include "core/sdl_input_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace {
// Plain static (not thread_local): PCSX2 spawns its own MTGS render thread
// inside VMManager::Initialize and that thread is what calls
// Host::AcquireRenderWindow → env_cb. A thread_local g_current is null on
// the MTGS thread because we only ever set it on the CoreRuntime worker
// thread (runLoop), which silently breaks the SP3 NSView env command.
// Cross-thread visibility is provided by QThread::start's happens-before
// edge (this assignment runs in runLoop before any core code spawns
// downstream threads). We run exactly one CoreRuntime at a time.
CoreRuntime* g_current = nullptr;

// Sticky for the process: set when any core reports a wedged shutdown
// (retronest_shutdown_wedged). Read by CoreRuntime::anyCoreWedged() —
// main.cpp must _exit() instead of exit() once this is set (the wedged
// dylib stays mapped and its static destructors crash under live threads).
static std::atomic<bool> g_anyCoreWedged{false};

// Env-gated audio diagnostic (RETRONEST_AUDIO_TRACE=1).
bool audioTraceEnabled() {
    static const bool v = (std::getenv("RETRONEST_AUDIO_TRACE") != nullptr);
    return v;
}

// Env-gated input diagnostic (RETRONEST_INPUT_TRACE=1).
bool inputTraceEnabled() {
    static const bool v = (std::getenv("RETRONEST_INPUT_TRACE") != nullptr);
    return v;
}
}

CoreRuntime::CoreRuntime(QObject* parent) : QObject(parent) {
    connect(&m_video, &VideoSoftware::frameReady, this, &CoreRuntime::frameReady);
    // AutoConnection: m_video lives on the constructing thread (main thread).
    // When submitFrame() is called from the worker thread, frameReady is emitted
    // from the worker thread. Qt's AutoConnection queues the signal to CoreRuntime
    // on the main thread, which in turn re-emits frameReady to any downstream
    // connections — also queued across the thread boundary. This is correct and
    // intentional; do NOT move m_video to the worker thread.
}

CoreRuntime::~CoreRuntime() {
    // SP3.5: stop() is non-blocking. This destructor runs from the event
    // loop after the worker has signalled finished() and our owner has
    // deleteLater'd us, so the QThread is guaranteed exited by now. Still
    // request stop defensively, then join + delete the QThread.
    if (m_thread) {
        m_stopRequested = true;
        resume();
        // Bounded wait — by here the worker has already exited in the
        // common path; the timeout is just a safety net for unexpected
        // paths (e.g. direct destruction without prior stop()).
        m_thread->wait(2000);
        if (m_thread->isRunning()) {
            qWarning() << "[CoreRuntime] worker still running at destructor; "
                          "terminating forcibly";
            m_thread->terminate();
            m_thread->wait(500);
        }
        delete m_thread;
        m_thread = nullptr;
    }
}

EnvironmentContext* CoreRuntime::tlsCtx() {
    return g_current ? &g_current->m_envCtx : nullptr;
}

bool CoreRuntime::envTrampoline(unsigned cmd, void* data) {
    return environmentDispatch(tlsCtx(), cmd, data);
}

void CoreRuntime::videoTrampoline(const void* data, unsigned w, unsigned h, size_t pitch) {
    if (!g_current) return;

    // Task #7 step 6: HW frames arrive with `data == RETRO_HW_FRAME_BUFFER_VALID`
    // (the libretro sentinel meaning "I rendered into the FBO returned by
    // get_current_framebuffer; ignore the data pointer"). RETRO_HW_FRAME_BUFFER_VALID
    // is `(void*)-1`, so the old `!data` early-out passed it through and
    // VideoSoftware::convert tried to memcpy from 0xff...ff — crash.
    // Branch on the sentinel before any pointer dereference.
    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        if (g_current->m_videoHW) {
            g_current->m_videoHW->submitFrame(static_cast<int>(w),
                                              static_cast<int>(h));
        }
        return;
    }

    // Software path (mGBA + any future SW core). nullptr is the libretro
    // "duped frame" signal — just no-op.
    if (!data) return;
    auto pf = g_current->m_envCtx.pixelFormat;
    VideoSoftware::PixelFormat ours =
        (pf == RETRO_PIXEL_FORMAT_RGB565)   ? VideoSoftware::PixelFormat::RGB565   :
        (pf == RETRO_PIXEL_FORMAT_XRGB8888) ? VideoSoftware::PixelFormat::XRGB8888 :
                                              VideoSoftware::PixelFormat::ARGB1555;
    g_current->m_video.setPixelFormat(ours);
    g_current->m_video.submitFrame(data, static_cast<int>(w), static_cast<int>(h), pitch);
}

size_t CoreRuntime::audioBatchTrampoline(const int16_t* data, size_t frames) {
    // SP4.x: return real acceptance so PCSX2's pending-buffer absorbs any
    // overflow when the sink's queue ceiling kicks in. Without backpressure
    // a single overdraining frame can put the queue into a state where it
    // never recovers (was the original SP4 bug — see SP4.x trace evidence).
    if (!g_current) return frames; // no sink yet → claim accepted (avoid spin)
    return static_cast<size_t>(
        g_current->m_audio.writeSamples(data, static_cast<int>(frames)));
}

void CoreRuntime::audioSampleTrampoline(int16_t l, int16_t r) {
    int16_t pair[2] = {l, r};
    if (g_current) g_current->m_audio.writeSamples(pair, 1);
}

void CoreRuntime::inputPollTrampoline() {}

int16_t CoreRuntime::inputStateTrampoline(unsigned port, unsigned device,
                                          unsigned index, unsigned id) {
    // NOTE: InputRouter::lookup() is NOT used from the core thread; lookups
    // happen on the Qt thread (via SdlInputManager) which then writes the
    // atomic state. We only call buttonPressed() / axis() here, both of which
    // read atomics — safe across threads without a lock.
    if (!g_current) return 0;

    if (device == RETRO_DEVICE_JOYPAD) {
        auto slot = static_cast<RetroPadSlot>(id);
        // Combo-modifier suppression: mask buttons currently acting as the
        // modifier of a matched libretro hotkey combo from the core's view.
        if (auto* hk = HotkeyMatcher::s_active.load(std::memory_order_relaxed)) {
            if (hk->isSuppressed(static_cast<int>(port), static_cast<int>(slot)))
                return 0;
        }
        return g_current->m_input.buttonPressed(static_cast<int>(port), slot) ? 1 : 0;
    }

    if (device == RETRO_DEVICE_ANALOG) {
        // Map (index, id) → RetroPadAxis. Unknown combinations return 0
        // (forward-compat with future libretro spec extensions).
        RetroPadAxis a = RetroPadAxis::Count;
        if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
            if      (id == RETRO_DEVICE_ID_ANALOG_X) a = RetroPadAxis::LeftX;
            else if (id == RETRO_DEVICE_ID_ANALOG_Y) a = RetroPadAxis::LeftY;
        } else if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) {
            if      (id == RETRO_DEVICE_ID_ANALOG_X) a = RetroPadAxis::RightX;
            else if (id == RETRO_DEVICE_ID_ANALOG_Y) a = RetroPadAxis::RightY;
        } else if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON) {
            if      (id == RETRO_DEVICE_ID_JOYPAD_L2) a = RetroPadAxis::L2;
            else if (id == RETRO_DEVICE_ID_JOYPAD_R2) a = RetroPadAxis::R2;
        }
        if (a == RetroPadAxis::Count) return 0;
        const int16_t rd = g_current->m_input.axis(static_cast<int>(port), a);
        if (inputTraceEnabled()) {
            // Rate-limit: log only every Nth read to keep output sane.
            // 6 axes × 60 fps = 360 reads/sec without limit.
            static thread_local int counter = 0;
            if ((counter++ % 60) == 0) {
                qDebug("[input] port=%u axis=%d rd=%d", port,
                       static_cast<int>(a), rd);
            }
        }
        return rd;
    }

    return 0;
}

bool CoreRuntime::start(const StartConfig& cfg) {
    if (m_thread) return false;
    m_cfg = cfg;
    m_stopRequested = false;
    m_paused = false;
    m_thread = QThread::create([this] { runLoop(); });
    m_thread->start();
    return true;
}

void CoreRuntime::stop() {
    if (!m_thread) return;
    m_stopRequested = true;
    resume();  // unblock condvar if paused

    // SP3.5: non-blocking. The worker exits on its own and emits the
    // CoreRuntime::finished() Qt signal (queued to the main thread).
    // GameSession's slot for that signal calls m_libretroAdapter->
    // releaseRuntime() which deleteLater's this CoreRuntime; the
    // ~CoreRuntime destructor then waits for and deletes the QThread.
    //
    // Why non-blocking: PCSX2's MTGS shutdown posts Metal-layer release
    // work via Host::OnMainThread(). Any synchronous wait here blocks
    // the main thread, those callbacks never run, and the worker
    // deadlocks. Pumping events inside the wait avoids the deadlock but
    // re-enters QML/Cocoa timer state mid-shutdown and corrupts
    // CFRunLoopTimer arrays (verified via two distinct EXC_BAD_ACCESS
    // crash reports). Returning immediately lets the main event loop
    // service OnMainThread() callbacks naturally between Qt events.
    //
    // mGBA isn't affected by either failure mode (no OnMainThread
    // usage); for it the worker exits immediately after we set the
    // stop flag, and finished() fires before this function returns.
}

void CoreRuntime::pause() {
    // NEW: ask the core to halt its internal threads. PCSX2 exports
    // retronest_set_paused which calls VMManager::SetPaused(true) and
    // waits for VMState::Paused via WaitForVmPaused. mGBA / other cores
    // don't export this symbol — the pointer stays null and the existing
    // stop-retro_run + mute-audio behavior is enough.
    //
    // Comes BEFORE m_paused so PCSX2's threads are halted before our
    // worker stops calling retro_run. fn(true) blocks until the VM
    // reaches VMState::Paused, so additional retro_run calls during
    // the brief handshake window are harmless no-ops.
    if (auto fn = m_loader.symbols().retronest_set_paused)
        fn(true);

    m_paused = true;
    // Silence the SDL audio device too — without this the pre-pause
    // tail of samples queued for playback bleeds through while the
    // worker is blocked on the pause cond.
    m_audio.setPaused(true);
    // Stop any active rumble so motors don't keep running while paused.
    if (m_sdlInput) {
        for (int port = 0; port < InputRouter::NUM_PORTS; ++port) {
            m_sdlInput->setRumbleMotor(port, RETRO_RUMBLE_STRONG, 0);
            m_sdlInput->setRumbleMotor(port, RETRO_RUMBLE_WEAK,   0);
        }
    }
}

void CoreRuntime::resume() {
    {
        std::lock_guard<std::mutex> l(m_pauseMx);
        m_paused = false;
    }
    m_audio.setPaused(false);
    m_pauseCv.notify_all();

    // NEW: unblock the core's internal threads. Comes AFTER our worker
    // wakes up so retro_run resumes immediately on the next tick.
    if (auto fn = m_loader.symbols().retronest_set_paused)
        fn(false);
}

void CoreRuntime::reset() {
    // Best-effort cross-thread retro_reset for the libretro hotkey routing.
    // Most cores treat retro_reset as a synchronous state-machine reset and
    // tolerate being called from a non-worker thread between frames; if a
    // future core fails this assumption we can move the call onto a queued
    // request that the worker drains between retro_run ticks (mirroring the
    // save-state pattern). Silent no-op when the core is not loaded.
    if (!m_loader.isOpen()) return;
    if (auto fn = m_loader.symbols().retro_reset)
        fn();
}

bool CoreRuntime::saveState(const QString& path) {
    Q_ASSERT_X(!m_thread,
               "CoreRuntime::saveState",
               "worker thread is still running — use requestSaveState() instead");
    if (!m_loader.isOpen()) return false;
    size_t n = m_loader.symbols().retro_serialize_size();
    if (n == 0) return false;
    QByteArray buf(static_cast<int>(n), 0);
    if (!m_loader.symbols().retro_serialize(buf.data(), n)) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(buf);
    return true;
}

void CoreRuntime::requestSaveState(const QString& path) {
    std::lock_guard<std::mutex> l(m_saveMx);
    m_pendingSavePath = path;
}

void CoreRuntime::flushPendingSaveState(const CoreSymbols& s) {
    std::lock_guard<std::mutex> l(m_saveMx);
    if (m_pendingSavePath.isEmpty()) return;
    size_t n = s.retro_serialize_size();
    if (n > 0) {
        QByteArray buf(static_cast<int>(n), 0);
        if (s.retro_serialize(buf.data(), n)) {
            QFile f(m_pendingSavePath);
            if (f.open(QIODevice::WriteOnly)) f.write(buf);
        }
    }
    m_pendingSavePath.clear();
}

void CoreRuntime::requestLoadState(const QString& path) {
    std::lock_guard<std::mutex> l(m_loadMx);
    m_pendingLoadPath = path;
}

void CoreRuntime::flushPendingLoadState(const CoreSymbols& s) {
    std::lock_guard<std::mutex> l(m_loadMx);
    if (m_pendingLoadPath.isEmpty()) return;
    QFile f(m_pendingLoadPath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QByteArray state = f.readAll();
        s.retro_unserialize(state.constData(), static_cast<size_t>(state.size()));
    }
    m_pendingLoadPath.clear();
}

void CoreRuntime::requestControllerPortDevice(unsigned port, unsigned device) {
    std::lock_guard<std::mutex> l(m_portDevMx);
    m_pendingPortDevices[port] = device;
}

void CoreRuntime::flushPendingControllerDevices(const CoreSymbols& s) {
    std::map<unsigned, unsigned> pending;
    {
        std::lock_guard<std::mutex> l(m_portDevMx);
        pending.swap(m_pendingPortDevices);
    }
    if (!s.retro_set_controller_port_device)
        return;
    for (const auto& [port, device] : pending)
        s.retro_set_controller_port_device(port, device);
}

void CoreRuntime::setSpeedMultiplier(double multiplier) {
    if (multiplier <= 0.0) multiplier = 1.0;
    m_speedMultiplier.store(multiplier);
    // Cores like PCSX2 pace emulation internally and don't speed up
    // just because retro_run is called more often. If the core
    // exports retronest_set_fast_forward, drive its in-VM Turbo
    // limiter directly. Synchronous cores (mGBA etc.) don't export
    // it and rely on the standard speed-multiplier path above.
    if (auto fn = m_loader.symbols().retronest_set_fast_forward)
        fn(multiplier > 1.0);
}

void CoreRuntime::runLoop() {
    g_current = this;
    QString err;
    if (!m_loader.open(m_cfg.corePath, &err)) {
        emit errorOccurred("dlopen failed: " + err);
        emit finished(true);
        g_current = nullptr;
        return;
    }

    m_envCtx.systemDirectory = m_cfg.systemDir.toUtf8();
    // Path overrides for libretro Saves (mGBA writes .srm directly to
    // the libretro save_dir; redirecting save_dir is the propagation
    // path). Only honored when the user has set the "Saves" override
    // for this emulator. PCSX2 doesn't route memcards through save_dir
    // (those go via the dedicated GET_MEMCARDS_DIR env enum), so a
    // save_dir override here is mGBA-only in practice — but the lookup
    // is generic per emuId so any future libretro adapter with a Saves
    // PathDef gets the same propagation for free.
    QString saveDir = m_cfg.saveDir;
    {
        const QString savesOverride = PathOverridesStore::instance().read(m_cfg.emuId, "Saves");
        if (!savesOverride.isEmpty()) {
            if (QDir().mkpath(savesOverride)) {
                saveDir = savesOverride;
            } else {
                qWarning() << "[CoreRuntime] Cannot create Saves override dir; "
                              "falling back to default:" << savesOverride;
            }
        }
    }
    m_envCtx.saveDirectory   = saveDir.toUtf8();
    m_envCtx.options         = &m_options;
    m_envCtx.runtime         = static_cast<void*>(this);
    // SP6.5 Task 4.5: one-shot delivery to the libretro core via
    // RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH during retro_load_game.
    // Set bootStatePathConsumed=false explicitly here — the EnvironmentContext
    // is a member that persists across runs of runLoop, so we must reset the
    // flag for each session. If the core consumes the path during
    // retro_load_game (pcsx2-libretro will, mGBA won't), the env handler
    // sets bootStatePathConsumed=true; we check that flag below to decide
    // whether to skip the legacy post-load retro_unserialize block.
    m_envCtx.bootStatePath        = m_cfg.resumeStatePath.toUtf8();
    m_envCtx.bootStatePathConsumed = false;

    auto& s = m_loader.symbols();
    s.retro_set_environment(&CoreRuntime::envTrampoline);
    s.retro_set_video_refresh(&CoreRuntime::videoTrampoline);
    s.retro_set_audio_sample(&CoreRuntime::audioSampleTrampoline);
    s.retro_set_audio_sample_batch(&CoreRuntime::audioBatchTrampoline);
    s.retro_set_input_poll(&CoreRuntime::inputPollTrampoline);
    s.retro_set_input_state(&CoreRuntime::inputStateTrampoline);

    s.retro_init();

    // Reconcile core options now that the core has declared them via SET_CORE_OPTIONS_V2.
    if (!m_envCtx.declaredOptions.isEmpty())
        m_options.load(m_cfg.optionsJsonPath, m_envCtx.declaredOptions, m_cfg.schemaOptionDefaults);

    // Packet 7 Stage 2: persist the full declared-option metadata beside
    // options.json — the settings UI's offline schema source (seeded by
    // CoreProber before any first session; refreshed here on every start so
    // a core update re-captures its table).
    if (!m_envCtx.declaredDoc.isEmpty()) {
        retro_system_info sysinfo{};
        s.retro_get_system_info(&sysinfo);
        m_envCtx.declaredDoc.coreLibraryVersion =
            QString::fromUtf8(sysinfo.library_version ? sysinfo.library_version : "");
        const QString sidecar =
            QFileInfo(m_cfg.optionsJsonPath).dir().filePath("declared_options.json");
        if (!m_envCtx.declaredDoc.save(sidecar))
            qWarning() << "[CoreRuntime] failed to write declared-options sidecar:" << sidecar;
    }

    retro_game_info info{};
    QByteArray romPathBytes = m_cfg.romPath.toUtf8();
    info.path = romPathBytes.constData();
    info.data = nullptr;
    info.size = 0;
    if (!s.retro_load_game(&info)) {
        emit errorOccurred("retro_load_game failed");
        s.retro_deinit();
        emit finished(true);
        g_current = nullptr;
        return;
    }

    retro_system_av_info av{};
    s.retro_get_system_av_info(&av);
    m_video.setGeometry(static_cast<int>(av.geometry.base_width),
                        static_cast<int>(av.geometry.base_height),
                        static_cast<int>(av.geometry.max_width),
                        static_cast<int>(av.geometry.max_height));

    // Task #7 step 5: HW render bootstrap. installHwRender (called from
    // env_cb during retro_load_game above) already created the
    // NSOpenGLContext pair if the core requested OPENGL_CORE. Now that we
    // have av_info geometry, allocate the FBO and trigger context_reset
    // on the HW context — that's the callback PPSSPP uses to run
    // glewInit + CreateDrawContext, which boots its GL render manager.
    //
    // Order matters: allocateFbo runs GL calls on the main context (it
    // calls makeMainCurrent internally). Then we switch to the HW
    // context before context_reset so PPSSPP's glewInit and resource
    // creation land on hwCtx. The two contexts share resources, so the
    // FBO + textures created on mainCtx are visible from hwCtx via the
    // NSOpenGL share-group — same pattern RetroArch uses on macOS.
    if (m_videoHW && m_videoHW->isReady()) {
        const bool fboOk = m_videoHW->allocateFbo(
            static_cast<int>(av.geometry.base_width),
            static_cast<int>(av.geometry.base_height),
            m_hwRenderCb.depth);
        if (!fboOk) {
            qCritical("[CoreRuntime] HW FBO allocation failed — frames "
                      "won't reach the composite path");
        } else if (m_hwRenderCb.context_reset) {
            m_videoHW->makeHwCurrent();
            qInfo("[CoreRuntime] calling hw_render.context_reset() — PPSSPP "
                  "runs glewInit + CreateDrawContext from here");
            m_hwRenderCb.context_reset();
        }
    }

    // Surface the core's display-aspect hint to GameSession (it forwards
    // to QML's LibretroMetalItem.nativeAspect so the HW render bridge
    // letterboxes correctly). Zero is a valid "not specified" sentinel —
    // GameSession::setLibretroAspectRatio handles the fallback.
    emit aspectRatioReported(static_cast<qreal>(av.geometry.aspect_ratio));
    m_audio.open(static_cast<int>(av.timing.sample_rate));
    m_frameDurationSec = (av.timing.fps > 0.0) ? (1.0 / av.timing.fps) : (1.0 / 60.0);

    // SP6.5 Task 4.5: cold-resume fallback path.
    //
    // If the core consumed RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH during
    // retro_load_game (pcsx2-libretro does), m_envCtx.bootStatePathConsumed
    // is true and the VM is already in the loaded state via
    // VMBootParameters::save_state → VMManager::Initialize → DoLoadState.
    // Skip the post-load retro_unserialize in that case.
    //
    // If the core didn't query (mGBA and any non-PCSX2 libretro core),
    // bootStatePathConsumed stays false — fall back to the legacy
    // retro_unserialize path that loads the state AFTER retro_load_game.
    // mGBA's BIOS-init is trivial enough that this works for it.
    if (!m_cfg.resumeStatePath.isEmpty() && !m_envCtx.bootStatePathConsumed) {
        QFile f(m_cfg.resumeStatePath);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            QByteArray state = f.readAll();
            s.retro_unserialize(state.constData(), static_cast<size_t>(state.size()));
        }
    }

    // rcheevos session — best-effort; failure is non-fatal.
    // Pass the captured memory map so rc_libretro can translate RA
    // addresses through the core's descriptors (essential for cores
    // like mGBA where IWRAM and EWRAM are non-contiguous).
    const retro_memory_map* mmap =
        m_envCtx.memoryMapSet ? &m_envCtx.memoryMap : nullptr;
    m_rcheevos.beginSession(s, m_cfg.romPath, QString::fromUtf8(m_envCtx.raHash),
                            m_cfg.raConsoleId,
                            m_cfg.raUsername, m_cfg.raToken, m_cfg.raHardcore,
                            m_cfg.raEncore, mmap);

    emit started();

    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    while (!m_stopRequested.load()) {
        bool wasPaused = false;
        {
            std::unique_lock<std::mutex> lk(m_pauseMx);
            if (m_paused.load()) {
                wasPaused = true;
                m_pauseCv.wait(lk, [this] { return !m_paused.load() || m_stopRequested.load(); });
            }
        }
        if (m_stopRequested.load()) break;
        // Re-anchor the frame deadline after a pause. Otherwise `next`
        // is still set to a moment from before the pause, sleep_until
        // returns immediately for hundreds of iterations, and the burst
        // of retro_run() calls floods AudioSink → SDL_QueueAudio with
        // seconds of samples that then play out at 1× — the resume
        // audio-lag the user hears.
        if (wasPaused) next = clock::now();
        // Drain autoreleased Metal/ObjC objects every frame so they never
        // accumulate on this QThread's lifetime autorelease pool and outlive
        // the core dylib at dlclose() (CoreLoader::close). See
        // autorelease_scope.h for the resume-crash root cause this prevents.
        clock::time_point frame_start, frame_end;
        {
            mac::AutoreleaseScope arpool;
            // Apply a load-state request BEFORE retro_run so the next frame
            // emits the loaded state's video/audio (post-run flush would
            // discard the just-rendered frame).
            flushPendingControllerDevices(s);
            flushPendingLoadState(s);
            frame_start = clock::now();
            s.retro_run();
            frame_end = clock::now();
            m_rcheevos.frame();
            flushPendingSaveState(s);
        }
        const double mult = m_speedMultiplier.load();
        const long long target_ns =
            static_cast<long long>(m_frameDurationSec * 1e9 / mult);
        next += std::chrono::nanoseconds(target_ns);

        // SP4.x: bound the deadline-vs-real-time slip. retro_run is
        // internally wall-clock-paced by PCSX2's framelimiter via
        // g_present_cv, so a runaway `next` doesn't cause audio drift
        // (sleep_until just no-ops while next is in the past). But the
        // slip grows monotonically when exec_ms slightly exceeds target
        // (visible as bursts_so_far ≈ frame count in AUDIO_TRACE).
        // Resetting at 50 ms means the loop self-recovers from any
        // single-frame spike without piling up phantom debt.
        const auto behind = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now() - next);
        if (behind > std::chrono::milliseconds(50)) {
            next = clock::now();
        }

        if (audioTraceEnabled()) {
            static std::atomic<uint64_t> count{0};
            static std::atomic<uint64_t> bursts{0};
            const auto exec_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    frame_end - frame_start).count();
            const auto overrun_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    clock::now() - next).count();
            if (overrun_ns > 0)
                bursts.fetch_add(1, std::memory_order_relaxed);
            const uint64_t n = count.fetch_add(1, std::memory_order_relaxed);
            if ((n % 60) == 0) {
                qWarning().nospace().noquote()
                    << "[AUDIO_TRACE] runloop frame=" << n
                    << " exec_ms=" << QString::number(exec_ns / 1e6, 'f', 2)
                    << " target_ms=" << QString::number(target_ns / 1e6, 'f', 2)
                    << " overrun_ms=" << QString::number(overrun_ns / 1e6, 'f', 2)
                    << " bursts_so_far=" << bursts.load(std::memory_order_relaxed);
            }
        }

        std::this_thread::sleep_until(next);
    }

    // Run all teardown (the final save-state serialize, retro_unload_game,
    // retro_deinit → Metal device destroy) inside an autorelease pool that we
    // pop *before* m_loader.close() dlclose()s the core. Otherwise objects
    // autoreleased here linger on this QThread's lifetime pool and are released
    // only at thread exit — after the dylib is unmapped — crashing in
    // objc_autoreleasePoolPop (the resume SIGILL). See autorelease_scope.h.
    // This region is linear and non-throwing, so a manual push/pop (vs. a
    // scoped guard that would force re-indenting the whole block) suffices.
    void* const teardownPool = objc_autoreleasePoolPush();

    // Drain any save-state request that arrived after the last retro_run
    // (e.g. from GameSession::terminate → requestSaveState + stop).
    flushPendingSaveState(s);

    // Zero all rumble motors on teardown so motors don't keep running
    // after the game exits.
    if (m_sdlInput) {
        for (int port = 0; port < InputRouter::NUM_PORTS; ++port) {
            m_sdlInput->setRumbleMotor(port, RETRO_RUMBLE_STRONG, 0);
            m_sdlInput->setRumbleMotor(port, RETRO_RUMBLE_WEAK,   0);
        }
    }

    m_rcheevos.endSession();

    // HW-context callback must run BEFORE retro_unload_game.
    //
    // PPSSPP's retro_unload_game (libretro.cpp:1591-1592) does
    // `delete ctx; ctx = nullptr` on the LibretroHWRenderContext, but
    // the context_destroy static callback at
    // LibretroGraphicsContext.cpp:23 dereferences `Libretro::ctx`:
    //
    //     static void context_destroy() {
    //         ((LibretroHWRenderContext *)Libretro::ctx)->ContextDestroy();
    //     }
    //
    // Calling context_destroy AFTER retro_unload_game crashes with
    // EXC_BAD_ACCESS at context_destroy+12 (verified against three
    // crash reports 2026-05-22). A previous comment in this file
    // claimed the AFTER-order "mirrors RetroArch" — but RetroArch
    // does not in fact drive this exact ordering against PPSSPP's
    // HW-render path, and PPSSPP's internal lifecycle assumes the
    // BEFORE-order. Must be on the HW context (makeHwCurrent) so
    // PPSSPP's ContextDestroy uses the same NSOpenGL context that
    // ContextReset bound.
    if (m_videoHW && m_videoHW->isReady() && m_hwRenderCb.context_destroy) {
        m_videoHW->makeHwCurrent();
        m_hwRenderCb.context_destroy();
    }

    s.retro_unload_game();

    // Wedge check (PCSX2): if the core detached a stuck VM shutdown thread,
    // that thread is still executing inside the dylib. retro_deinit would
    // tear state down under it and dlclose would unmap its code (the
    // documented SIGBUS). Leave the core loaded for the rest of the process;
    // the core itself refuses further retro_load_game calls while wedged.
    const bool coreWedged =
        s.retronest_shutdown_wedged && s.retronest_shutdown_wedged();
    if (coreWedged) {
        g_anyCoreWedged.store(true);
        qCritical() << "[CoreRuntime] core shutdown WEDGED — skipping"
                    << "retro_deinit + dlclose; dylib stays mapped. Restart"
                    << "RetroNest before launching this emulator again.";
    }

    // Now tear down our HW-side objects (FBO, NSOpenGL contexts).
    // retro_unload_game's internal cleanup is done; nothing else needs
    // the GL context to be current.
    if (m_videoHW && m_videoHW->isReady()) {
        m_videoHW->shutdown();
        m_videoHW.reset();
        m_hwRenderCb = {};
    }

    if (!coreWedged)
        s.retro_deinit();
    m_audio.close();

    // Drain the teardown pool while the core dylib (and Metal device) are still
    // mapped, then unload the core.
    objc_autoreleasePoolPop(teardownPool);
    if (!coreWedged)
        m_loader.close();
    g_current = nullptr;
    emit finished(false);
}

bool CoreRuntime::anyCoreWedged() {
    return g_anyCoreWedged.load();
}

void CoreRuntime::setActiveNSView(void* ns_view) {
    m_active_ns_view.store(ns_view, std::memory_order_release);
}

void* CoreRuntime::activeNSView() const {
    return m_active_ns_view.load(std::memory_order_acquire);
}

// Strong implementation of weak stub from environment_callbacks.cpp
// This function is called by environmentDispatch for RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW
extern "C" void* coreRuntimeGetActiveNSView(void* runtime_opaque) {
    if (!runtime_opaque) return nullptr;
    auto* runtime = static_cast<CoreRuntime*>(runtime_opaque);
    return runtime->activeNSView();
}

// Strong implementation of weak stub from environment_callbacks.cpp.
// Routes libretro's set_rumble_state calls through the SdlInputManager
// registered via setSdlInputManager().
extern "C" bool coreRuntimeSetRumbleMotor(void* runtime_opaque,
                                          unsigned port,
                                          unsigned effect,
                                          uint16_t strength) {
    if (!runtime_opaque) return false;
    auto* rt = static_cast<CoreRuntime*>(runtime_opaque);
    auto* sdl = rt->sdlInputManager();
    if (!sdl) return false;
    return sdl->setRumbleMotor(static_cast<int>(port), effect, strength);
}

// Strong implementation of weak stub from environment_callbacks.cpp.
// Called from the libretro worker thread; emits coreMessage which crosses
// to the GUI thread via Qt::AutoConnection (queued, since CoreRuntime lives
// on a different thread than its env handler).
extern "C" void coreRuntimeEmitMessage(void* runtime_opaque,
                                       const char* text,
                                       int durationMs) {
    if (!runtime_opaque || !text) return;
    auto* rt = static_cast<CoreRuntime*>(runtime_opaque);
    emit rt->coreMessage(QString::fromUtf8(text), durationMs);
}

bool CoreRuntime::installHwRender(retro_hw_render_callback* cb) {
    if (!cb) {
        qWarning("[CoreRuntime] installHwRender: null callback");
        return false;
    }
    // Only OPENGL_CORE today (PPSSPP's preferred ask). The fallback
    // chain (compat OPENGL / VULKAN / D3D11) lands in future phases —
    // until then PPSSPP retries with OPENGL after we reject OPENGL_CORE,
    // which we'd also reject without this gate. Return false for
    // unsupported types so the core's CreateGraphicsContext loop can
    // walk to its next preference.
    if (cb->context_type != RETRO_HW_CONTEXT_OPENGL_CORE) {
        qInfo("[CoreRuntime] installHwRender: context_type=%u not yet "
              "supported (only OPENGL_CORE for now) — returning false so "
              "the core can try its next fallback",
              static_cast<unsigned>(cb->context_type));
        return false;
    }

    if (!m_videoHW) {
        m_videoHW = std::make_unique<VideoHardwareGL>();
    }
    if (!m_videoHW->init()) {
        qCritical("[CoreRuntime] installHwRender: VideoHardwareGL::init() failed; "
                  "dropping HW context and falling back to software path");
        m_videoHW.reset();
        return false;
    }

    // Stash the callback (we need context_reset / context_destroy for
    // lifecycle in subsequent steps), then overwrite the core's
    // function-pointer fields to point at our static thunks. The
    // mutation in place follows the libretro spec — RetroArch does the
    // same at runloop.c:3413-3425.
    m_hwRenderCb = *cb;
    cb->get_current_framebuffer = &VideoHardwareGL::getCurrentFramebufferThunk;
    cb->get_proc_address         = &VideoHardwareGL::getProcAddressThunk;

    qInfo("[CoreRuntime] installHwRender: OPENGL_CORE %u.%u depth=%d "
          "stencil=%d bottom_left_origin=%d cache_context=%d — granted",
          m_hwRenderCb.version_major, m_hwRenderCb.version_minor,
          static_cast<int>(m_hwRenderCb.depth),
          static_cast<int>(m_hwRenderCb.stencil),
          static_cast<int>(m_hwRenderCb.bottom_left_origin),
          static_cast<int>(m_hwRenderCb.cache_context));
    return true;
}

// Strong implementation of weak stub from environment_callbacks.cpp.
// Called by environmentDispatch when the core invokes SET_HW_RENDER.
extern "C" bool coreRuntimeInstallHwRender(void* runtime_opaque,
                                            retro_hw_render_callback* cb) {
    if (!runtime_opaque || !cb) return false;
    auto* rt = static_cast<CoreRuntime*>(runtime_opaque);
    return rt->installHwRender(cb);
}
