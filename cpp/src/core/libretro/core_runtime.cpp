#include "core_runtime.h"
#include "core/sdl_input_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
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
    if (!g_current || !data) return;
    auto pf = g_current->m_envCtx.pixelFormat;
    VideoSoftware::PixelFormat ours =
        (pf == RETRO_PIXEL_FORMAT_RGB565)   ? VideoSoftware::PixelFormat::RGB565   :
        (pf == RETRO_PIXEL_FORMAT_XRGB8888) ? VideoSoftware::PixelFormat::XRGB8888 :
                                              VideoSoftware::PixelFormat::ARGB1555;
    g_current->m_video.setPixelFormat(ours);
    g_current->m_video.submitFrame(data, static_cast<int>(w), static_cast<int>(h), pitch);
}

size_t CoreRuntime::audioBatchTrampoline(const int16_t* data, size_t frames) {
    if (g_current) g_current->m_audio.writeSamples(data, static_cast<int>(frames));
    return frames;
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

void CoreRuntime::setSpeedMultiplier(double multiplier) {
    if (multiplier <= 0.0) multiplier = 1.0;
    m_speedMultiplier.store(multiplier);
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
    m_envCtx.saveDirectory   = m_cfg.saveDir.toUtf8();
    m_envCtx.options         = &m_options;
    m_envCtx.runtime         = static_cast<void*>(this);

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
        m_options.load(m_cfg.optionsJsonPath, m_envCtx.declaredOptions);

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
    m_audio.open(static_cast<int>(av.timing.sample_rate));
    m_frameDurationSec = (av.timing.fps > 0.0) ? (1.0 / av.timing.fps) : (1.0 / 60.0);

    if (!m_cfg.resumeStatePath.isEmpty()) {
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
    m_rcheevos.beginSession(s, m_cfg.romPath, m_cfg.raConsoleId,
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
        // Apply a load-state request BEFORE retro_run so the next frame
        // emits the loaded state's video/audio (post-run flush would
        // discard the just-rendered frame).
        flushPendingLoadState(s);
        s.retro_run();
        m_rcheevos.frame();
        flushPendingSaveState(s);
        const double mult = m_speedMultiplier.load();
        next += std::chrono::nanoseconds(static_cast<long long>(m_frameDurationSec * 1e9 / mult));
        std::this_thread::sleep_until(next);
    }

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
    s.retro_unload_game();
    s.retro_deinit();
    m_audio.close();
    m_loader.close();
    g_current = nullptr;
    emit finished(false);
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
