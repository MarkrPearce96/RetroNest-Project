#include <QtTest>
#include "ui/settings/widgets/settings_description_bar.h"
#include "core/setting_def.h"

class TestSettingsDescriptionBar : public QObject {
    Q_OBJECT
private slots:
    void setSettingFillsTextAndPill() {
        SettingsDescriptionBar bar;
        SettingDef d;
        d.tooltip = "Runs VU1 on a second thread.";
        d.defaultValue = "true";
        d.recommendedValue = "true";
        bar.setSetting(d);
        QCOMPARE(bar.descText(), QString("Runs VU1 on a second thread."));
        QCOMPARE(bar.recommendedText(), QString("Recommended: true"));
    }
    void emptyRecommendedFallsBackToDefault() {
        SettingsDescriptionBar bar;
        SettingDef d;
        d.tooltip = "t";
        d.defaultValue = "42";
        bar.setSetting(d);
        QCOMPARE(bar.recommendedText(), QString("Recommended: 42"));
    }
    void clearShowsPlaceholder() {
        SettingsDescriptionBar bar;
        bar.clear();
        QVERIFY(bar.descText().contains("Focus"));
    }
    void comboRecommendedTranslatesToLabel() {
        SettingsDescriptionBar bar;
        SettingDef d;
        d.tooltip = "t";
        d.type = SettingDef::Combo;
        d.recommendedValue = "1";
        d.options = {{"100% [60 FPS]", "1"}, {"200%", "2"}};
        bar.setSetting(d);
        QCOMPARE(bar.recommendedText(), QString("Recommended: 100% [60 FPS]"));
    }

    void setHintsStoresHints() {
        SettingsDescriptionBar bar;
        QVector<SettingsDescriptionBar::ButtonHint> hints = {
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
        SettingsDescriptionBar bar;
        bar.setHints({{"confirm", "Select"}});
        QCOMPARE(bar.hints().size(), 1);
        bar.clearHints();
        QCOMPARE(bar.hints().size(), 0);
    }
};
QTEST_MAIN(TestSettingsDescriptionBar)
#include "test_pcsx2_description_bar.moc"
