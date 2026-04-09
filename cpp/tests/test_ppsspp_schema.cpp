#include <QtTest>
#include <QSet>
#include "adapters/ppsspp_adapter.h"
#include "core/setting_def.h"

class TestPPSSPPSchema : public QObject {
    Q_OBJECT

private:
    QVector<SettingDef> schema_;

private slots:
    void initTestCase() {
        PPSSPPAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void testCategoriesAreGraphicsAudioOverlay() {
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories, QSet<QString>({"Graphics", "Audio", "Overlay"}));
    }

    void testGraphicsSubcategories() {
        QSet<QString> subs;
        for (const auto& d : schema_)
            if (d.category == "Graphics") subs.insert(d.subcategory);
        QCOMPARE(subs, QSet<QString>({
            "Emulation", "Rendering", "Frame Pacing",
            "Performance", "Textures", "Post-Processing"
        }));
    }

    void testEmulationSettingsLiveUnderGraphics() {
        // FastMemoryAccess used to live under category="Emulation".
        // It must now be under Graphics → Emulation sub-tab.
        bool found = false;
        for (const auto& d : schema_) {
            if (d.key == "FastMemoryAccess") {
                QCOMPARE(d.category, QString("Graphics"));
                QCOMPARE(d.subcategory, QString("Emulation"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    void testLensFlareOcclusionExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.key == "DepthRasterMode") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Graphics"));
        QCOMPARE(found->subcategory, QString("Performance"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QCOMPARE(found->options.size(), 4);  // Auto / Low / Off / Always on
    }

    void testPostProcessingShaderExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "PostShaderList" && d.key == "PostShader1") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Graphics"));
        QCOMPARE(found->subcategory, QString("Post-Processing"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QVERIFY(found->options.size() >= 10);  // expect a healthy shader list
        // First option must be the "Off" sentinel
        QCOMPARE(found->options.first().second, QString("Off"));
    }

    void testOverlayBitmaskCheckboxes() {
        struct Expected { QString label; int bit; };
        const QVector<Expected> expected = {
            {"Show FPS Counter", 2},
            {"Show Speed",       4},
            {"Show Battery %",   8},
        };
        for (const auto& exp : expected) {
            const SettingDef* found = nullptr;
            for (const auto& d : schema_) {
                if (d.label == exp.label) { found = &d; break; }
            }
            QVERIFY2(found != nullptr, qPrintable("missing: " + exp.label));
            QCOMPARE(found->category, QString("Overlay"));
            QCOMPARE(found->key, QString("iShowStatusFlags"));
            QCOMPARE(int(found->type), int(SettingDef::Bool));
            QCOMPARE(found->bitmask, exp.bit);
        }
    }

    void testDebugOverlayExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.key == "DebugOverlay") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Overlay"));
        QCOMPARE(found->section, QString("General"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
    }
};

QTEST_GUILESS_MAIN(TestPPSSPPSchema)
#include "test_ppsspp_schema.moc"
