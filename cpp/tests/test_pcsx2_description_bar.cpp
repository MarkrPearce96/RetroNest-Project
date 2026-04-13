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
    void comboRecommendedTranslatesToLabel() {
        Pcsx2DescriptionBar bar;
        SettingDef d;
        d.tooltip = "t";
        d.type = SettingDef::Combo;
        d.recommendedValue = "1";
        d.options = {{"100% [60 FPS]", "1"}, {"200%", "2"}};
        bar.setSetting(d);
        QCOMPARE(bar.recommendedText(), QString("Recommended: 100% [60 FPS]"));
    }

    void setHintsStoresHints() {
        Pcsx2DescriptionBar bar;
        QVector<Pcsx2DescriptionBar::ButtonHint> hints = {
            {"navigate_ud", "Navigate"},
            {"confirm", "Select"},
            {"back", "Close"},
        };
        bar.setHints(hints);
        QCOMPARE(bar.hints().size(), 3);
        QCOMPARE(bar.hints()[0].action, QString("navigate_ud"));
        QCOMPARE(bar.hints()[0].label, QString("Navigate"));
        QCOMPARE(bar.hints()[2].action, QString("back"));
    }

    void clearHintsRemovesAll() {
        Pcsx2DescriptionBar bar;
        bar.setHints({{"confirm", "Select"}});
        QCOMPARE(bar.hints().size(), 1);
        bar.clearHints();
        QCOMPARE(bar.hints().size(), 0);
    }
};
QTEST_MAIN(TestPcsx2DescriptionBar)
#include "test_pcsx2_description_bar.moc"
