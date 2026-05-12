#include "audio_sink.h"
#include <SDL2/SDL.h>
#include <QDebug>
#include <atomic>
#include <cstdlib>
#include <vector>

namespace {
// Env-gated audio diagnostic (RETRONEST_AUDIO_TRACE=1). Read once on first
// call; zero overhead when unset.
bool audioTraceEnabled() {
    static const bool v = (std::getenv("RETRONEST_AUDIO_TRACE") != nullptr);
    return v;
}
} // namespace

AudioSink::~AudioSink() { close(); }

bool AudioSink::open(int sourceSampleRate) {
    if (isOpen()) close();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        qWarning() << "[AudioSink] SDL_InitSubSystem(AUDIO) failed:" << SDL_GetError();
        return false;
    }
    SDL_AudioSpec want = {};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = nullptr;
    SDL_AudioSpec have = {};
    m_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (m_dev == 0) {
        qWarning() << "[AudioSink] SDL_OpenAudioDevice failed:" << SDL_GetError();
        return false;
    }
    m_deviceRate = have.freq;
    m_sourceRate = sourceSampleRate;
    m_stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, sourceSampleRate,
                                  AUDIO_S16SYS, 2, m_deviceRate);
    if (!m_stream) {
        qWarning() << "[AudioSink] SDL_NewAudioStream failed:" << SDL_GetError();
        SDL_CloseAudioDevice(m_dev); m_dev = 0;
        return false;
    }
    SDL_PauseAudioDevice(m_dev, 0);
    return true;
}

void AudioSink::close() {
    if (m_stream) { SDL_FreeAudioStream(m_stream); m_stream = nullptr; }
    if (m_dev)    { SDL_CloseAudioDevice(m_dev); m_dev = 0; }
    m_totalFrames.store(0, std::memory_order_relaxed);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioSink::setPaused(bool paused) {
    if (!m_dev) return;
    // Drop any queued samples on pause so resume doesn't burst the
    // pre-pause tail when SDL re-enables the device.
    if (paused) SDL_ClearQueuedAudio(m_dev);
    SDL_PauseAudioDevice(m_dev, paused ? 1 : 0);
}

int AudioSink::writeSamples(const int16_t* data, int frames) {
    if (!m_dev || !m_stream || !data || frames <= 0) return 0;

    // SP4.x defensive: refuse new samples once SDL has > 150 ms queued.
    // In steady state with the matched-drain-rate fix this is never hit;
    // it only fires on transient hitches (e.g. a slow GS frame letting
    // PCSX2 catch up and burst audio). Returning 0 lets the core's
    // pending-buffer absorb the burst until the queue drains, instead of
    // letting it grow unboundedly into multi-second latency. 150 ms is
    // well above the steady-state ~50 ms so we don't refuse normal flow.
    constexpr uint32_t kMaxQueueMs = 150;
    if (m_deviceRate > 0) {
        const uint32_t max_bytes =
            (static_cast<uint32_t>(m_deviceRate) * kMaxQueueMs / 1000) * 2 * sizeof(int16_t);
        if (SDL_GetQueuedAudioSize(m_dev) > max_bytes) {
            if (audioTraceEnabled()) {
                static std::atomic<uint64_t> refused{0};
                const uint64_t n = refused.fetch_add(1, std::memory_order_relaxed);
                if ((n % 60) == 0) {
                    qWarning().nospace().noquote()
                        << "[AUDIO_TRACE] sink_refuse idx=" << n
                        << " frames_offered=" << frames
                        << " sdl_queue_bytes=" << SDL_GetQueuedAudioSize(m_dev);
                }
            }
            return 0;
        }
    }

    SDL_AudioStreamPut(m_stream, data, frames * 2 * sizeof(int16_t));
    m_totalFrames.fetch_add(frames);

    // Drain stream into device queue
    int avail = SDL_AudioStreamAvailable(m_stream);
    if (avail > 0) {
        std::vector<uint8_t> buf(avail);
        int got = SDL_AudioStreamGet(m_stream, buf.data(), avail);
        if (got > 0) SDL_QueueAudio(m_dev, buf.data(), got);
    }

    if (audioTraceEnabled()) {
        static std::atomic<uint64_t> count{0};
        const uint64_t n = count.fetch_add(1, std::memory_order_relaxed);
        if ((n % 60) == 0) {
            const uint32_t qBytes = SDL_GetQueuedAudioSize(m_dev);
            const int qFrames = static_cast<int>(qBytes / (2 * sizeof(int16_t)));
            const double qMs = (m_deviceRate > 0)
                ? (1000.0 * qFrames / m_deviceRate) : 0.0;
            qWarning().nospace().noquote()
                << "[AUDIO_TRACE] sink_write idx=" << n
                << " frames_in=" << frames
                << " sdl_queue_bytes=" << qBytes
                << " sdl_queue_frames=" << qFrames
                << " sdl_queue_ms=" << QString::number(qMs, 'f', 2)
                << " device_rate=" << m_deviceRate
                << " source_rate=" << m_sourceRate;
        }
    }

    return frames;
}
