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
};
QTEST_GUILESS_MAIN(TestPcsx2RecommendedValues)
#include "test_pcsx2_recommended_values.moc"
