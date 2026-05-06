#include <QtTest>
#include "core/libretro/audio_sink.h"

class TestAudioSink : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Ensure SDL audio subsystem can init under test (offscreen ok)
        qputenv("SDL_AUDIODRIVER", "dummy");
    }
    void testOpenAndClose() {
        AudioSink sink;
        QVERIFY(sink.open(/*sourceRate=*/32000));
        QVERIFY(sink.isOpen());
        sink.close();
        QVERIFY(!sink.isOpen());
    }
    void testWriteSamplesDoesNotCrashWhenClosed() {
        AudioSink sink;
        int16_t buf[200] = {};
        sink.writeSamples(buf, 100);
    }
    void testWriteSamplesIncrementsCounter() {
        AudioSink sink;
        QVERIFY(sink.open(32000));
        int16_t buf[200] = {};
        sink.writeSamples(buf, 100);
        QCOMPARE(sink.totalFramesWritten(), uint64_t(100));
        sink.close();
    }
};
QTEST_MAIN(TestAudioSink)
#include "test_audio_sink.moc"
