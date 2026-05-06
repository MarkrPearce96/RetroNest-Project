#include <QtTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include "core/libretro/core_runtime.h"

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
        QTemporaryFile rom; rom.open(); rom.write("data"); rom.close();
        CoreRuntime rt;
        QSignalSpy started(&rt, &CoreRuntime::started);
        QSignalSpy finished(&rt, &CoreRuntime::finished);
        CoreRuntime::StartConfig cfg;
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
    void testFrameReadyEmittedFromCoreThread() {
        QTemporaryDir d;
        QTemporaryFile rom; rom.open(); rom.write("data"); rom.close();
        CoreRuntime rt;
        QSignalSpy frames(&rt, &CoreRuntime::frameReady);
        CoreRuntime::StartConfig cfg;
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
};
QTEST_MAIN(TestCoreRuntime)
#include "test_core_runtime.moc"
