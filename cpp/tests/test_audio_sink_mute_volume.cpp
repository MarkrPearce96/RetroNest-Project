#include <QtTest>
#include "core/libretro/audio_sink.h"

class TestAudioSinkMuteVolume : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Ensure SDL audio subsystem can init under test (offscreen ok)
        qputenv("SDL_AUDIODRIVER", "dummy");
    }
    void mutedSamplesAreZero() {
        AudioSink s;
        s.setMuted(true);
        int16_t in[8]  = {1000, -1000, 500, -500, 800, -800, 200, -200};
        int16_t out[8] = {0};
        int written = s.applyGainAndMute(in, out, 4);  // 4 stereo frames = 8 samples
        QCOMPARE(written, 4);
        for (int i = 0; i < 8; ++i) QCOMPARE(out[i], int16_t(0));
    }
    void volumeScalesSamples() {
        AudioSink s;
        s.setMuted(false);
        s.setVolume(0.5f);
        int16_t in[2]  = {1000, -1000};
        int16_t out[2] = {0};
        s.applyGainAndMute(in, out, 1);
        QCOMPARE(out[0], int16_t(500));
        QCOMPARE(out[1], int16_t(-500));
    }
    void volumeClampsTo01() {
        AudioSink s;
        s.setVolume(2.0f);  QCOMPARE(s.volume(), 1.0f);
        s.setVolume(-1.0f); QCOMPARE(s.volume(), 0.0f);
    }
    void defaultIsUnityNoMute() {
        AudioSink s;
        QCOMPARE(s.isMuted(), false);
        QCOMPARE(s.volume(), 1.0f);
    }
    void inPlaceSafeWhenMuted() {
        AudioSink s;
        s.setMuted(true);
        int16_t buf[4] = {100, 200, 300, 400};
        s.applyGainAndMute(buf, buf, 2);
        for (int i = 0; i < 4; ++i) QCOMPARE(buf[i], int16_t(0));
    }
};

QTEST_APPLESS_MAIN(TestAudioSinkMuteVolume)
#include "test_audio_sink_mute_volume.moc"
