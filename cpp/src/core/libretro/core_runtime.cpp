#include "core_runtime.h"
#include <QDebug>
#include <QFile>
#include <chrono>
#include <thread>

namespace {
thread_local CoreRuntime* g_current = nullptr;
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
    if (m_thread) { stop(); }
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
                                          unsigned /*index*/, unsigned id) {
    // NOTE: InputRouter::lookup() is NOT used from the core thread; lookups
    // happen on the Qt thread (via SdlInputManager) which then writes the
    // atomic bitmask via setButtonPressed(). We only call buttonPressed() here,
    // which reads an atomic<uint32_t> — safe across threads without a lock.
    if (!g_current || device != RETRO_DEVICE_JOYPAD) return 0;
    auto slot = static_cast<RetroPadSlot>(id);
    return g_current->m_input.buttonPressed(static_cast<int>(port), slot) ? 1 : 0;
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
    m_thread->wait(5000);
    delete m_thread;
    m_thread = nullptr;
}

void CoreRuntime::pause() {
    m_paused = true;
}

void CoreRuntime::resume() {
    {
        std::lock_guard<std::mutex> l(m_pauseMx);
        m_paused = false;
    }
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
    m_rcheevos.beginSession(s, m_cfg.romPath, m_cfg.raConsoleId,
                            m_cfg.raUsername, m_cfg.raToken, m_cfg.raHardcore);

    emit started();

    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    while (!m_stopRequested.load()) {
        {
            std::unique_lock<std::mutex> lk(m_pauseMx);
            m_pauseCv.wait(lk, [this] { return !m_paused.load() || m_stopRequested.load(); });
        }
        if (m_stopRequested.load()) break;
        s.retro_run();
        m_rcheevos.frame();
        flushPendingSaveState(s);
        next += std::chrono::nanoseconds(static_cast<long long>(m_frameDurationSec * 1e9));
        std::this_thread::sleep_until(next);
    }

    // Drain any save-state request that arrived after the last retro_run
    // (e.g. from GameSession::terminate → requestSaveState + stop).
    flushPendingSaveState(s);

    m_rcheevos.endSession();
    s.retro_unload_game();
    s.retro_deinit();
    m_audio.close();
    m_loader.close();
    g_current = nullptr;
    emit finished(false);
}
