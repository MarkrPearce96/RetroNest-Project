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
    /** Pause/resume the underlying SDL audio device. While paused, the
     *  device produces silence regardless of what writeSamples queues —
     *  prevents the in-game menu from leaking residual / looping audio
     *  while the libretro worker thread is blocked on the pause cond. */
    void setPaused(bool paused);

    /** writeSamples: stereo int16 frames; `frames` is per-channel pair count.
     *  Returns the number of frames accepted (== frames in steady state).
     *  Returns 0 when the SDL queue is at the defensive ceiling — callers
     *  must respect this (libretro's audio_batch_cb contract: the core
     *  re-queues unaccepted samples). */
    int writeSamples(const int16_t* data, int frames);
    uint64_t totalFramesWritten() const { return m_totalFrames.load(); }

private:
    uint32_t m_dev = 0;
    SDL_AudioStream* m_stream = nullptr;
    std::atomic<uint64_t> m_totalFrames{0};
    int m_sourceRate = 0;
    int m_deviceRate = 0;
};
