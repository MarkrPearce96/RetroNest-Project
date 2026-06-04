// cpp/tests/test_duckstation_libretro_schema.cpp
//
// Phase 5 shape guard for DuckStationLibretroAdapter::settingsSchema().
// Locks in schema structure and feature-#1 first-run defaults so any drift
// (renamed key, changed default, missing sub-tab) trips loud rather than
// producing a silently-wrong UI.

#include <QtTest>
#include <QSet>
#include "adapters/libretro/duckstation_libretro_adapter.h"
#include "core/setting_def.h"

class TestDuckStationLibretroSchema : public QObject {
    Q_OBJECT
    QVector<SettingDef> schema_;
private slots:
    void initTestCase() {
        DuckStationLibretroAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }
    void testAllRowsAreLibretroOptions() {
        for (const auto& d : schema_)
            QCOMPARE(d.storage, SettingDef::Storage::LibretroOption);
    }
    void testGraphicsHasSubTabs() {
        DuckStationLibretroAdapter adapter;
        QVERIFY(adapter.settingsCategoriesWithSubTabs().contains("Graphics"));
        QSet<QString> subs;
        for (const auto& d : schema_) if (d.category == "Graphics") subs.insert(d.subcategory);
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
        for (const auto& d : schema_) if (d.key == "duckstation_gpu_renderer") {
            // options pairs are {label, value}; .second is the libretro VALUE
            QSet<QString> vals; for (const auto& p : d.options) vals.insert(p.second);
            QCOMPARE(vals, QSet<QString>({"Automatic","Metal","Software"}));
        }
    }
    void testNoUseThreadOption() {
        for (const auto& d : schema_)
            QVERIFY2(!d.key.contains("use_thread"), "UseThread must not be user-exposed");
    }
};
QTEST_GUILESS_MAIN(TestDuckStationLibretroSchema)
#include "test_duckstation_libretro_schema.moc"
