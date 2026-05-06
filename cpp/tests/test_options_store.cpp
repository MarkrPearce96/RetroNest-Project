#include <QtTest>
#include <QTemporaryDir>
#include "core/libretro/options_store.h"

class TestOptionsStore : public QObject {
    Q_OBJECT
private slots:
    void testReconcileSeedsDefaults() {
        QTemporaryDir d;
        OptionsStore s;
        QString path = d.path() + "/options.json";
        QVector<CoreOption> coreOpts = {
            {"mgba_skip_bios", "Skip BIOS", "OFF", {"OFF","ON"}},
            {"mgba_solar_sensor_level", "Solar Sensor", "0", {"0","1","2","3"}},
        };
        QVERIFY(s.load(path, coreOpts));
        QCOMPARE(s.get("mgba_skip_bios"), QString("OFF"));
        QCOMPARE(s.get("mgba_solar_sensor_level"), QString("0"));
    }
    void testRoundTripPersistsUserValue() {
        QTemporaryDir d;
        QString path = d.path() + "/options.json";
        QVector<CoreOption> coreOpts = {
            {"mgba_skip_bios","Skip BIOS","OFF",{"OFF","ON"}}
        };
        {
            OptionsStore s;
            s.load(path, coreOpts);
            s.set("mgba_skip_bios", "ON");
            s.save();
        }
        OptionsStore s2;
        s2.load(path, coreOpts);
        QCOMPARE(s2.get("mgba_skip_bios"), QString("ON"));
    }
    void testReconcileAppendsNewCoreKeys() {
        QTemporaryDir d;
        QString path = d.path() + "/options.json";
        OptionsStore s;
        s.load(path, {{"a","A","x",{"x","y"}}});
        s.set("a", "y");
        s.save();
        OptionsStore s2;
        s2.load(path, {
            {"a","A","x",{"x","y"}},
            {"b","B","p",{"p","q"}}
        });
        QCOMPARE(s2.get("a"), QString("y"));
        QCOMPARE(s2.get("b"), QString("p"));
    }
    void testDirtyFlagAndConsume() {
        QTemporaryDir d;
        OptionsStore s;
        s.load(d.path() + "/options.json", {{"a","A","x",{"x","y"}}});
        QVERIFY(!s.consumeDirty());
        s.set("a", "y");
        QVERIFY(s.consumeDirty());
        QVERIFY(!s.consumeDirty());
    }
};
QTEST_APPLESS_MAIN(TestOptionsStore)
#include "test_options_store.moc"
