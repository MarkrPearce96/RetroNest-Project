#include <QtTest>
#include "adapters/pcsx2_adapter.h"
#include "core/setting_def.h"

class TestPcsx2RecommendedValues : public QObject {
    Q_OBJECT
private:
    QVector<SettingDef> schema_;
private slots:
    void initTestCase() {
        PCSX2Adapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }
    void testEmulationSettingsAllHaveRecommended() {
        int emuCount = 0;
        for (const auto& d : schema_) {
            if (d.category != "Emulation") continue;
            ++emuCount;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Emulation/%1").arg(d.key)));
        }
        QVERIFY(emuCount >= 13);
    }
    void testAudioSettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Audio") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Audio/%1").arg(d.key)));
        }
        QCOMPARE(count, 11);
    }
    void testMemoryCardsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Memory Cards") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Memory Cards/%1").arg(d.key)));
        }
        QCOMPARE(count, 7);
    }
    void testGraphicsRenderingSettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Graphics") continue;
            if (d.subcategory != "Rendering") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Graphics/Rendering/%1").arg(d.key)));
        }
        QVERIFY2(count >= 7,
                 qPrintable(QString("expected >= 7 Graphics/Rendering settings, got %1").arg(count)));
    }
    void testGraphicsPostProcessingSettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Graphics") continue;
            if (d.subcategory != "Post-Processing") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Graphics/Post-Processing/%1").arg(d.key)));
        }
        QVERIFY2(count >= 7,
                 qPrintable(QString("expected >= 7 Graphics/Post-Processing settings, got %1").arg(count)));
    }
    void testGraphicsDisplaySettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Graphics") continue;
            if (d.subcategory != "Display") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Graphics/Display/%1").arg(d.key)));
            QVERIFY2(!d.tooltip.isEmpty(),
                     qPrintable(QString("missing tooltip for Graphics/Display/%1").arg(d.key)));
        }
        QVERIFY2(count >= 15,
                 qPrintable(QString("expected >= 15 Graphics/Display settings, got %1").arg(count)));
    }
};
QTEST_GUILESS_MAIN(TestPcsx2RecommendedValues)
#include "test_pcsx2_recommended_values.moc"
