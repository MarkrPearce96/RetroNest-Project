// cpp/tests/test_duckstation_libretro_schema.cpp
//
// Phase 5 shape guard for DuckStationLibretroAdapter::settingsSchema().
// Locks in schema structure and feature-#1 first-run defaults so any drift
// (renamed key, changed default, missing sub-tab) trips loud rather than
// producing a silently-wrong UI.

#include <QtTest>
#include <QFileInfo>
#include <QSet>
#include "adapters/libretro/duckstation_libretro_adapter.h"
#include "core/setting_def.h"

class TestDuckStationLibretroSchema : public QObject {
    Q_OBJECT
    QVector<SettingDef> schema_;
private slots:
    void initTestCase() {
        DuckStationLibretroAdapter adapter;
        // Packet 7 Stage 2: the schema renders from the core's declared
        // option table — hermetic tests inject the committed fixture
        // (recorded by test_schema_parity's snapshot mode) instead of
        // touching the live sidecar / prober.
        const QString fixture = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/fixtures/declared/duckstation_declared_options.json";
        const auto doc = DeclaredOptionsDoc::load(fixture);
        QVERIFY2(doc.has_value(), "declared fixture missing");
        adapter.setDeclaredDocForTest(*doc);
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }
    void testAllRowsAreLibretroOptions() {
        for (const auto& d : schema_)
            QCOMPARE(d.storage, SettingDef::Storage::LibretroOption);
    }
    void testMaxLibretroPlayersIsTwo() {
        QCOMPARE(DuckStationLibretroAdapter().maxLibretroPlayers(), 2);
    }
    void testGraphicsHasSubTabs() {
        // Use schema_ for the row scan so we don't rebuild the adapter;
        // construct inline just for the settingsCategoriesWithSubTabs() call.
        QVERIFY(DuckStationLibretroAdapter().settingsCategoriesWithSubTabs().contains("Graphics"));
        bool hasGraphicsRow = false;
        QSet<QString> subs;
        for (const auto& d : schema_) {
            if (d.category == "Graphics") {
                hasGraphicsRow = true;
                subs.insert(d.subcategory);
            }
        }
        QVERIFY2(hasGraphicsRow, "no Graphics rows found in schema");
        QVERIFY(subs.contains("Rendering"));
    }
    void testFirstRunDefaultsMatchFeature1Profile() {
        auto def = [&](const QString& key) {
            for (const auto& d : schema_) if (d.key == key) return d.defaultValue;
            return QString("<missing>");
        };
        QCOMPARE(def("duckstation_gpu_resolution_scale"), QString("4"));
        QCOMPARE(def("duckstation_gpu_pgxp_enable"), QString("true"));
        QCOMPARE(def("duckstation_gpu_dithering"), QString("TrueColor"));
        QCOMPARE(def("duckstation_gpu_renderer"), QString("Automatic"));
    }
    void testRendererExcludesUnwiredBackends() {
        bool found = false;
        for (const auto& d : schema_) {
            if (d.key != "duckstation_gpu_renderer") continue;
            found = true;
            QSet<QString> vals;
            for (const auto& p : d.options) vals.insert(p.second);
            QCOMPARE(vals, QSet<QString>({"Automatic","Metal","Software"}));
        }
        QVERIFY2(found, "duckstation_gpu_renderer not found in schema");
    }
    void testNoUseThreadOption() {
        for (const auto& d : schema_)
            QVERIFY2(!d.key.contains("use_thread"), "UseThread must not be user-exposed");
    }
    void testEveryDefaultIsInOptions() {
        for (const auto& d : schema_) {
            bool found = false;
            for (const auto& opt : d.options)
                if (opt.second == d.defaultValue) { found = true; break; }
            QVERIFY2(found, qPrintable(QString("row '%1' default '%2' not in its options")
                                           .arg(d.key).arg(d.defaultValue)));
        }
    }
    void testNoDuplicateKeysWithinACategory() {
        // Recommended deliberately re-references keys that live in other
        // categories, so global uniqueness no longer holds — enforce
        // uniqueness within each category instead.
        QSet<QString> seen;  // "category/key"
        for (const auto& d : schema_) {
            const QString id = d.category + "/" + d.key;
            QVERIFY2(!seen.contains(id),
                qPrintable(QString("duplicate key '%1' in category '%2'").arg(d.key).arg(d.category)));
            seen.insert(id);
        }
    }
    void testPad2TypeOptionPresentWithAnalogDefault() {
        QString def = "<missing>";
        for (const auto& d : schema_) if (d.key == "duckstation_pad2_type") def = d.defaultValue;
        QCOMPARE(def, QString("AnalogController"));
    }
    void testRecommendedRowsExistElsewhere() {
        QSet<QString> nonRecKeys;
        for (const auto& d : schema_)
            if (d.category != "Recommended") nonRecKeys.insert(d.key);
        for (const auto& d : schema_) {
            if (d.category == "Recommended")
                QVERIFY2(nonRecKeys.contains(d.key),
                    qPrintable(QString("Recommended row '%1' has no home row in another category").arg(d.key)));
        }
    }
};
QTEST_GUILESS_MAIN(TestDuckStationLibretroSchema)
#include "test_duckstation_libretro_schema.moc"
