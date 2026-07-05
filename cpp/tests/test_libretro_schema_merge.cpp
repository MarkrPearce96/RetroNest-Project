// Packet 7 Stage 2: generic declared-options × overlay → SettingDef merge,
// plus the no-runtime OptionsStore fallback synthesized from the declared doc.
#include <QtTest>
#include <QTemporaryDir>
#include "adapters/libretro/libretro_adapter.h"
#include "core/paths.h"

namespace {

class TestAdapter : public LibretroAdapter {
public:
    QString coreId() const override { return "fake"; }
    QVector<OptionOverlay> optionOverlays() const override { return m_overlays; }
    QVector<SettingDef> extraSettings() const override { return m_extras; }

    QVector<OptionOverlay> m_overlays;
    QVector<SettingDef> m_extras;
};

DeclaredOptionsDoc fixtureDoc()
{
    DeclaredOptionsDoc doc;
    doc.coreLibraryVersion = "fake-1.0";
    DeclaredOption speed;
    speed.key = "fake_speed";
    speed.label = "Emulation Speed";
    speed.info = "How fast.";
    speed.defaultValue = "1";
    speed.values = { { "1", "Normal" }, { "2", "Double" } };
    DeclaredOption toggle;
    toggle.key = "fake_bool";
    toggle.label = "Fake Toggle";
    toggle.defaultValue = "disabled";
    toggle.values = { { "enabled", "" }, { "disabled", "" } };
    DeclaredOption hidden;
    hidden.key = "fake_uncurated";
    hidden.label = "Never Shown";
    hidden.defaultValue = "x";
    hidden.values = { { "x", "" }, { "y", "" } };
    doc.options = { speed, toggle, hidden };
    return doc;
}

} // namespace

class TestLibretroSchemaMerge : public QObject {
    Q_OBJECT
private slots:
    void adoptCoreWording() {
        TestAdapter a;
        a.setDeclaredDocForTest(fixtureDoc());
        OptionOverlay ov;
        ov.key = "fake_speed";
        ov.categories = { "Emulation" };
        a.m_overlays = { ov };

        const auto rows = a.settingsSchema();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].key, QString("fake_speed"));
        QCOMPARE(rows[0].label, QString("Emulation Speed"));   // core wording
        QCOMPARE(rows[0].tooltip, QString("How fast."));       // core info
        QCOMPARE(rows[0].defaultValue, QString("1"));          // core default
        QCOMPARE(rows[0].category, QString("Emulation"));
        QCOMPARE(rows[0].type, SettingDef::Combo);
        QVERIFY(rows[0].storage == SettingDef::Storage::LibretroOption);
        // Value labels flow into the (display, value) pairs.
        QCOMPARE(rows[0].options.size(), 2);
        QCOMPARE(rows[0].options[0], (QPair<QString, QString>{ "Normal", "1" }));
        QCOMPARE(rows[0].options[1], (QPair<QString, QString>{ "Double", "2" }));
    }

    void overridesWin() {
        TestAdapter a;
        a.setDeclaredDocForTest(fixtureDoc());
        OptionOverlay ov;
        ov.key = "fake_speed";
        ov.categories = { "Emulation" };
        ov.labelOverride = "Speed (curated)";
        ov.tooltipOverride = "Curated tooltip.";
        ov.defaultOverride = "2";
        ov.hasTypeOverride = true;
        ov.typeOverride = SettingDef::Int;
        ov.minVal = 1; ov.maxVal = 2; ov.step = 1;
        ov.layout = "slider";
        ov.dependsOn = "fake_bool";
        ov.recommendedValue = "2";
        a.m_overlays = { ov };

        const auto rows = a.settingsSchema();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].label, QString("Speed (curated)"));
        QCOMPARE(rows[0].tooltip, QString("Curated tooltip."));
        QCOMPARE(rows[0].defaultValue, QString("2"));
        QCOMPARE(rows[0].type, SettingDef::Int);
        QCOMPARE(rows[0].minVal, 1.0);
        QCOMPARE(rows[0].maxVal, 2.0);
        QCOMPARE(rows[0].layout, QString("slider"));
        QCOMPARE(rows[0].dependsOn, QString("fake_bool"));
        QCOMPARE(rows[0].recommendedValue, QString("2"));
    }

    void recommendedCrossListing() {
        TestAdapter a;
        a.setDeclaredDocForTest(fixtureDoc());
        OptionOverlay ov;
        ov.key = "fake_bool";
        ov.categories = { "Recommended", "System" };
        a.m_overlays = { ov };

        const auto rows = a.settingsSchema();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[0].category, QString("Recommended"));
        QCOMPARE(rows[1].category, QString("System"));
        QCOMPARE(rows[0].key, rows[1].key);   // same OptionsStore key
        // No declared value labels → value doubles as display text.
        QCOMPARE(rows[0].options[0], (QPair<QString, QString>{ "enabled", "enabled" }));
    }

    void uncuratedHiddenAndMissingSkipped() {
        TestAdapter a;
        a.setDeclaredDocForTest(fixtureDoc());
        OptionOverlay good;
        good.key = "fake_speed";
        good.categories = { "Emulation" };
        OptionOverlay stale;
        stale.key = "fake_removed_upstream";   // not in the declared table
        stale.categories = { "Emulation" };
        a.m_overlays = { good, stale };

        const auto rows = a.settingsSchema();
        QCOMPARE(rows.size(), 1);                       // stale skipped
        QCOMPARE(rows[0].key, QString("fake_speed"));   // fake_uncurated hidden
    }

    void extraSettingsAppended() {
        TestAdapter a;
        a.setDeclaredDocForTest(fixtureDoc());
        OptionOverlay ov;
        ov.key = "fake_speed";
        ov.categories = { "Emulation" };
        a.m_overlays = { ov };
        SettingDef extra;
        extra.storage = SettingDef::Storage::FrontendSetting;
        extra.key = "aspect_mode";
        extra.label = "Aspect Ratio";
        extra.category = "Video";
        extra.type = SettingDef::Combo;
        a.m_extras = { extra };

        const auto rows = a.settingsSchema();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[1].key, QString("aspect_mode"));
        QVERIFY(rows[1].storage == SettingDef::Storage::FrontendSetting);
    }

    // Packet 7 Stage 2 Task 5: with a declared doc available, the no-runtime
    // fallback store validates against ALL declared options (superset of the
    // curated overlay), so uncurated keys keep their persisted values.
    void fallbackStoreFromDeclaredDoc() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(Paths::setRoot(tmp.path()));

        TestAdapter a;
        a.setDeclaredDocForTest(fixtureDoc());
        OptionOverlay ov;
        ov.key = "fake_speed";           // only ONE key curated...
        ov.categories = { "Emulation" };
        a.m_overlays = { ov };

        OptionsStore* store = a.libretroOptionsStore();
        QVERIFY(store);
        // ...but ALL THREE declared keys are valid in the store.
        QCOMPARE(store->get("fake_speed"), QString("1"));
        QCOMPARE(store->get("fake_bool"), QString("disabled"));
        QCOMPARE(store->get("fake_uncurated"), QString("x"));
    }
};

QTEST_APPLESS_MAIN(TestLibretroSchemaMerge)
#include "test_libretro_schema_merge.moc"
