#pragma once

#include "core_loader.h"
#include "environment_callbacks.h"
#include "video_software.h"
#include "audio_sink.h"
#include "input_router.h"
#include "options_store.h"

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
    };

    explicit CoreRuntime(QObject* parent = nullptr);
    ~CoreRuntime() override;

    bool start(const StartConfig& cfg);
    void stop();
    void pause();
    void resume();
    bool saveState(const QString& path);

    InputRouter& input() { return m_input; }
    OptionsStore& options() { return m_options; }

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

    StartConfig m_cfg;
    CoreLoader m_loader;
    EnvironmentContext m_envCtx;
    VideoSoftware m_video;
    AudioSink m_audio;
    InputRouter m_input;
    OptionsStore m_options;

    QThread* m_thread = nullptr;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_paused{false};
    std::mutex m_pauseMx;
    std::condition_variable m_pauseCv;

    double m_frameDurationSec = 1.0 / 60.0;
    int m_sampleRate = 48000;
};
