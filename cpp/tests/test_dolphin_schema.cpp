#include <QtTest>
#include <QSet>
#include "adapters/dolphin_adapter.h"
#include "core/setting_def.h"

class TestDolphinSchema : public QObject {
    Q_OBJECT

private:
    QVector<SettingDef> schema_;

private slots:
    void initTestCase() {
        DolphinAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void testTopLevelCategories() {
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories, QSet<QString>({"Interface", "Audio", "Core"}));
    }

    void testPauseOnFocusLostExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.key == "PauseOnFocusLost") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Interface"));
        QCOMPARE(found->section, QString("Interface"));
        QCOMPARE(int(found->type), int(SettingDef::Bool));
        QCOMPARE(found->defaultValue, QString("True"));
    }

    void testAudioBackendIsCubebDefault() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "DSP" && d.key == "Backend") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QCOMPARE(found->defaultValue, QString("Cubeb"));
        QVERIFY(found->options.size() >= 2);
    }

    void testCpuCoreCombo() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "Core" && d.key == "CPUCore") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QVERIFY(found->options.size() >= 3);
    }

    void testBoolValuesAreCapitalized() {
        // Dolphin writes True/False (Common/StringUtil.cpp:289-292), not true/false.
        for (const auto& d : schema_) {
            if (d.type != SettingDef::Bool) continue;
            QVERIFY2(d.defaultValue == "True" || d.defaultValue == "False",
                     qPrintable(QString("Bool '%1' has non-capitalized default '%2'")
                                .arg(d.key, d.defaultValue)));
        }
    }

    void testResolutionOptionsTargetGfxIni() {
        DolphinAdapter adapter;
        ResolutionOptions opts = adapter.resolutionOptions();
        QVERIFY(!opts.options.isEmpty());
        QVERIFY2(!opts.iniFilePath.isEmpty(),
                 "Resolution must target GFX.ini via the iniFilePath field");
        QVERIFY(opts.iniFilePath.endsWith("GFX.ini"));
        QCOMPARE(opts.section, QString("Settings"));
        QCOMPARE(opts.key, QString("InternalResolution"));
    }

    void testAspectRatioOptionsTargetGfxIni() {
        DolphinAdapter adapter;
        AspectRatioOptions opts = adapter.aspectRatioOptions();
        QVERIFY(!opts.options.isEmpty());
        for (const auto& opt : opts.options) {
            QVERIFY(!opt.patches.isEmpty());
            for (const auto& patch : opt.patches) {
                QVERIFY2(!patch.iniFilePath.isEmpty(),
                         qPrintable(QString("Aspect '%1' patch missing iniFilePath").arg(opt.label)));
                QVERIFY(patch.iniFilePath.endsWith("GFX.ini"));
            }
        }
    }
};

QTEST_MAIN(TestDolphinSchema)
#include "test_dolphin_schema.moc"
