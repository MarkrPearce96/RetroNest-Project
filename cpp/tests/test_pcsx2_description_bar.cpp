#include <QtTest>
#include "ui/settings/pcsx2/widgets/pcsx2_description_bar.h"
#include "core/setting_def.h"

class TestPcsx2DescriptionBar : public QObject {
    Q_OBJECT
private slots:
    void setSettingFillsTextAndPill() {
        Pcsx2DescriptionBar bar;
        SettingDef d;
        d.tooltip = "Runs VU1 on a second thread.";
        d.defaultValue = "true";
        d.recommendedValue = "true";
        bar.setSetting(d);
        QCOMPARE(bar.descText(), QString("Runs VU1 on a second thread."));
        QCOMPARE(bar.recommendedText(), QString("Recommended: true"));
    }
    void emptyRecommendedFallsBackToDefault() {
        Pcsx2DescriptionBar bar;
        SettingDef d;
        d.tooltip = "t";
        d.defaultValue = "42";
        bar.setSetting(d);
        QCOMPARE(bar.recommendedText(), QString("Recommended: 42"));
    }
    void clearShowsPlaceholder() {
        Pcsx2DescriptionBar bar;
        bar.clear();
        QVERIFY(bar.descText().contains("Focus"));
    }
};
QTEST_MAIN(TestPcsx2DescriptionBar)
#include "test_pcsx2_description_bar.moc"
