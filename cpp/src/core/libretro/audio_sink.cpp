#include "audio_sink.h"
#include <SDL2/SDL.h>
#include <QDebug>
#include <vector>

AudioSink::~AudioSink() { close(); }

bool AudioSink::open(int sourceSampleRate) {
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
    m_totalFrames = 0;
}

void AudioSink::writeSamples(const int16_t* data, int frames) {
    if (!m_dev || !m_stream || !data || frames <= 0) return;
    SDL_AudioStreamPut(m_stream, data, frames * 2 * sizeof(int16_t));
    m_totalFrames.fetch_add(frames);

    // Drain stream into device queue
    int avail = SDL_AudioStreamAvailable(m_stream);
    if (avail > 0) {
        std::vector<uint8_t> buf(avail);
        int got = SDL_AudioStreamGet(m_stream, buf.data(), avail);
        if (got > 0) SDL_QueueAudio(m_dev, buf.data(), got);
    }
}
