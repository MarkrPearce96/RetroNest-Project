#pragma once
#include <atomic>
#include <cstdint>

struct _SDL_AudioStream;
typedef struct _SDL_AudioStream SDL_AudioStream;

class AudioSink {
public:
    AudioSink() = default;
    ~AudioSink();

    bool open(int sourceSampleRate);
    void close();
    bool isOpen() const { return m_dev != 0; }

    /** writeSamples: stereo int16 frames; `frames` is per-channel pair count. */
    void writeSamples(const int16_t* data, int frames);
    uint64_t totalFramesWritten() const { return m_totalFrames.load(); }

private:
    uint32_t m_dev = 0;
    SDL_AudioStream* m_stream = nullptr;
    std::atomic<uint64_t> m_totalFrames{0};
    int m_sourceRate = 0;
    int m_deviceRate = 0;
};
