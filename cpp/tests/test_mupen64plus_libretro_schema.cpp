// cpp/tests/test_mupen64plus_libretro_schema.cpp
//
// Shape guard for Mupen64PlusLibretroAdapter::settingsSchema(). Locks the
// curated N64 schema (renders from the core's declared table × the adapter
// overlay) so drift — a renamed core option, a changed default, an
// accidentally-exposed footgun — trips loud instead of shipping a silently
// wrong settings UI.

#include <QtTest>
#include <QFileInfo>
#include <QSet>
#include "adapters/libretro/mupen64plus_libretro_adapter.h"
#include "core/setting_def.h"

class TestMupen64PlusLibretroSchema : public QObject {
    Q_OBJECT
    QVector<SettingDef> schema_;
private slots:
    void initTestCase() {
        Mupen64PlusLibretroAdapter adapter;
        // Hermetic: inject the committed fixture (recorded from the installed
        // v2026.07.21 universal core's sidecar) instead of touching the live
        // sidecar / prober.
        const QString fixture = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/fixtures/declared/mupen64plus_declared_options.json";
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
    void testQuickTabKeysExistInSchema() {
        // The Resolution / Aspect quick-settings tabs read these keys.
        Mupen64PlusLibretroAdapter a;
        QSet<QString> keys;
        for (const auto& d : schema_) keys.insert(d.key);
        QVERIFY(keys.contains(a.resolutionOptionKey()));
        QVERIFY(keys.contains(a.aspectRatioOptionKey()));
    }
    void testCoreDefaultsFlowThrough() {
        auto def = [&](const QString& key) {
            for (const auto& d : schema_) if (d.key == key) return d.defaultValue;
            return QString("<missing>");
        };
        // Deliberate RetroNest defaultOverride: factor "2" == the core's
        // effective stock quality (factor 0 → 640x480 = 2x native) without
        // the "Disabled"-reads-hidden-legacy-lists state.
        QCOMPARE(def("mupen64plus-EnableNativeResFactor"), QString("2"));
        // Core default flows through untouched.
        QCOMPARE(def("mupen64plus-aspect"), QString("4:3"));
    }
    void testFootgunsStayHidden() {
        // Deliberately uncurated: exposing these has bitten us before
        // (cpucore seeding) or is inert/unbuilt on macOS (Overscan, plugin
        // selection, ThreadedRenderer).
        for (const auto& d : schema_) {
            QVERIFY2(d.key != "mupen64plus-cpucore", "cpucore must stay hidden");
            QVERIFY2(!d.key.startsWith("mupen64plus-Overscan"),
                     "Overscan rows must stay hidden (pass disabled on macOS)");
            QVERIFY2(d.key != "mupen64plus-EnableOverscan",
                     "EnableOverscan must stay hidden");
            QVERIFY2(d.key != "mupen64plus-rdp-plugin" &&
                     d.key != "mupen64plus-rsp-plugin",
                     "plugin selection must stay hidden");
            QVERIFY2(d.key != "mupen64plus-ThreadedRenderer",
                     "ThreadedRenderer must stay hidden");
        }
    }
    void testResolutionFactorExcludesDisabled() {
        // "0"/Disabled reads the hidden legacy size lists — trimmed from the
        // row so the UI has no dead value (1x..8x only).
        bool found = false;
        for (const auto& d : schema_) {
            if (d.key != "mupen64plus-EnableNativeResFactor") continue;
            found = true;
            for (const auto& p : d.options)
                QVERIFY2(p.second != "0", "factor value '0' must be excluded");
            QCOMPARE(d.options.size(), 8);  // 1x..8x
        }
        QVERIFY(found);
    }
    void testLegacySizeListsStayHidden() {
        // The fixed-size lists only act when the native-res factor is 0;
        // with the factor defaulted to its 2x equivalent they are pure
        // confusion — resolution has exactly one UI control.
        for (const auto& d : schema_) {
            QVERIFY2(d.key != "mupen64plus-43screensize" &&
                     d.key != "mupen64plus-169screensize",
                     "legacy fixed-size resolution lists must stay hidden");
        }
    }
    void testEveryDefaultIsInOptions() {
        for (const auto& d : schema_) {
            if (d.options.isEmpty()) continue;
            bool found = false;
            for (const auto& p : d.options)
                if (p.second == d.defaultValue) { found = true; break; }
            QVERIFY2(found, qPrintable(QString("default '%1' of %2 not in its value list")
                                           .arg(d.defaultValue, d.key)));
        }
    }
    void testHubCardCategoriesAllHaveRows() {
        Mupen64PlusLibretroAdapter a;
        QSet<QString> cats;
        for (const auto& d : schema_) cats.insert(d.category);
        for (const auto& c : a.settingsHubCards())
            QVERIFY2(cats.contains(c.categoryKey),
                     qPrintable(QString("hub card '%1' has no schema rows").arg(c.categoryKey)));
    }
};

QTEST_MAIN(TestMupen64PlusLibretroSchema)
#include "test_mupen64plus_libretro_schema.moc"
