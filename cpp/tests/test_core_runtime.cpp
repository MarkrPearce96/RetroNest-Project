#include <QtTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <dlfcn.h>
#include "core/libretro/core_runtime.h"
#include "core/libretro/declared_options.h"

class TestCoreRuntime : public QObject {
    Q_OBJECT
private:
    QString fakeCorePath() const {
        return QCoreApplication::applicationDirPath() + "/fake_libretro_core.dylib";
    }
private slots:
    void initTestCase() { qputenv("SDL_AUDIODRIVER", "dummy"); }
    void testStartEmitsStartedThenStopEmitsFinished() {
        QTemporaryDir d;
        QTemporaryFile rom; QVERIFY(rom.open()); rom.write("data"); rom.close();
        CoreRuntime rt;
        QSignalSpy started(&rt, &CoreRuntime::started);
        QSignalSpy finished(&rt, &CoreRuntime::finished);
        CoreRuntime::StartConfig cfg;
        cfg.emuId = "fake_core";
        cfg.corePath = fakeCorePath();
        cfg.romPath = rom.fileName();
        cfg.systemDir = d.path() + "/sys";
        cfg.saveDir = d.path() + "/save";
        cfg.optionsJsonPath = d.path() + "/options.json";
        QVERIFY(rt.start(cfg));
        QVERIFY(started.wait(2000));
        QTest::qWait(200);  // let core run a few frames
        rt.stop();
        // stop() joins the worker thread, so finished may already be in the spy.
        // Qt6 QSignalSpy::wait() only returns true for signals emitted *during* the
        // wait period, so check count() first as the primary assertion.
        QVERIFY(finished.count() > 0 || finished.wait(3000));
    }
    // Packet 7 Stage 2: every session start persists the core's declared
    // options (full metadata) beside options.json — the settings UI's
    // offline schema source.
    void testSessionWritesDeclaredOptionsSidecar() {
        QTemporaryDir d;
        QTemporaryFile rom; QVERIFY(rom.open()); rom.write("data"); rom.close();
        CoreRuntime rt;
        QSignalSpy started(&rt, &CoreRuntime::started);
        QSignalSpy finished(&rt, &CoreRuntime::finished);
        CoreRuntime::StartConfig cfg;
        cfg.emuId = "fake_core";
        cfg.corePath = fakeCorePath();
        cfg.romPath = rom.fileName();
        cfg.systemDir = d.path() + "/sys";
        cfg.saveDir = d.path() + "/save";
        cfg.optionsJsonPath = d.path() + "/options.json";
        QVERIFY(rt.start(cfg));
        QVERIFY(started.wait(2000));
        rt.stop();
        QVERIFY(finished.count() > 0 || finished.wait(3000));

        const auto doc = DeclaredOptionsDoc::load(d.path() + "/declared_options.json");
        QVERIFY(doc.has_value());
        QCOMPARE(doc->options.size(), 2);
        QCOMPARE(doc->options[0].key, QString("fake_speed"));
        QCOMPARE(doc->options[0].values.size(), 2);
        QCOMPARE(doc->options[0].values[1].label, QString("Double"));
        QCOMPARE(doc->options[1].key, QString("fake_bool"));
        QCOMPARE(doc->coreLibraryVersion, QString("1.0"));
    }
    void testFrameReadyEmittedFromCoreThread() {
        QTemporaryDir d;
        QTemporaryFile rom; QVERIFY(rom.open()); rom.write("data"); rom.close();
        CoreRuntime rt;
        QSignalSpy frames(&rt, &CoreRuntime::frameReady);
        CoreRuntime::StartConfig cfg;
        cfg.emuId = "fake_core";
        cfg.corePath = fakeCorePath();
        cfg.romPath = rom.fileName();
        cfg.systemDir = d.path() + "/sys";
        cfg.saveDir = d.path() + "/save";
        cfg.optionsJsonPath = d.path() + "/options.json";
        QVERIFY(rt.start(cfg));
        QTest::qWait(500);
        rt.stop();
        QVERIFY(frames.count() > 0);
    }
    void testPauseCallsRetronestSetPaused() {
        CoreRuntime rt;
        QString err;
        // Open the loader directly without starting a worker thread —
        // this test only exercises the symbol dispatch path in pause()
        // and resume(), not the worker loop.
        QVERIFY2(rt.loader().open(fakeCorePath(), &err), qPrintable(err));

        // Resolve test accessor symbols from the loaded dylib handle.
        void* h = rt.loader().handle();
        auto reset  = reinterpret_cast<void(*)(void)>(dlsym(h, "retronest_test_reset_pause_counter"));
        auto count  = reinterpret_cast<int(*)(void)>(dlsym(h, "retronest_test_pause_call_count"));
        auto last   = reinterpret_cast<int(*)(void)>(dlsym(h, "retronest_test_last_pause_value"));
        QVERIFY(reset && count && last);
        reset();

        rt.pause();
        QCOMPARE(count(), 1);
        QCOMPARE(last(), 1);

        rt.resume();
        QCOMPARE(count(), 2);
        QCOMPARE(last(), 0);
    }
    void testPauseSkipsCallWhenSymbolAbsent() {
        // CoreRuntime with no core opened — symbol pointer is null.
        // pause()/resume() must not crash and must not increment the
        // counter. (The pointer guard `if (auto fn = ...)` is the
        // production safety net we're verifying here.)
        CoreRuntime rt;
        rt.pause();   // should not crash
        rt.resume();  // should not crash
        QVERIFY(true);  // explicit: reached pause+resume without crash
    }
};
QTEST_MAIN(TestCoreRuntime)
#include "test_core_runtime.moc"
