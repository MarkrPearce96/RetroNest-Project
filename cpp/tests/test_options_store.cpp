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
    void testSchemaDefaultOverridesUpstream() {
        QTemporaryDir d;
        OptionsStore s;
        QString path = d.path() + "/options.json";
        QVector<CoreOption> coreOpts = {
            {"mgba_skip_bios", "Skip BIOS", "OFF", {"OFF","ON"}},
        };
        QHash<QString, QString> schemaDefaults = { {"mgba_skip_bios", "ON"} };
        QVERIFY(s.load(path, coreOpts, schemaDefaults));
        // Schema default overrides upstream's "OFF" on first load.
        QCOMPARE(s.get("mgba_skip_bios"), QString("ON"));
    }
    void testExistingValueBeatsSchemaDefault() {
        QTemporaryDir d;
        QString path = d.path() + "/options.json";
        QVector<CoreOption> coreOpts = {
            {"mgba_solar_sensor_level", "Solar", "0", {"0","1","2","3"}},
        };
        // Seed an existing on-disk value distinct from both upstream "0" and schema "2".
        {
            OptionsStore s;
            s.load(path, coreOpts);
            s.set("mgba_solar_sensor_level", "3");
            s.save();
        }
        // Schema says "2", upstream says "0", existing says "3". Existing must win.
        OptionsStore s2;
        QHash<QString, QString> schemaDefaults = { {"mgba_solar_sensor_level", "2"} };
        QVERIFY(s2.load(path, coreOpts, schemaDefaults));
        QCOMPARE(s2.get("mgba_solar_sensor_level"), QString("3"));
    }
    void testSchemaDefaultAppliedInMemoryMode() {
        OptionsStore s;
        QVector<CoreOption> coreOpts = {
            {"mgba_skip_bios", "Skip BIOS", "OFF", {"OFF","ON"}},
        };
        QHash<QString, QString> schemaDefaults = { {"mgba_skip_bios", "ON"} };
        QVERIFY(s.load(":memory:", coreOpts, schemaDefaults));
        QCOMPARE(s.get("mgba_skip_bios"), QString("ON"));
    }
    void testInvalidSchemaDefaultFallsBackToUpstream() {
        QTemporaryDir d;
        OptionsStore s;
        QString path = d.path() + "/options.json";
        QVector<CoreOption> coreOpts = {
            {"mgba_skip_bios", "Skip BIOS", "OFF", {"OFF","ON"}},
        };
        // Schema names a value upstream doesn't list.
        QHash<QString, QString> schemaDefaults = { {"mgba_skip_bios", "MAYBE"} };
        QVERIFY(s.load(path, coreOpts, schemaDefaults));
        // Invalid schema default → silent fall-through to upstream default.
        QCOMPARE(s.get("mgba_skip_bios"), QString("OFF"));
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
QTEST_MAIN(TestOptionsStore)
#include "test_options_store.moc"
