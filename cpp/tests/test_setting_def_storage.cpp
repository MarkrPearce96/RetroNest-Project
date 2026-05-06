#include <QtTest>
#include "core/setting_def.h"

class TestSettingDefStorage : public QObject {
    Q_OBJECT
private slots:
    void testDefaultIsIni() {
        SettingDef def;
        QCOMPARE(def.storage, SettingDef::Storage::Ini);
    }
    void testStorageCanBeLibretroOption() {
        SettingDef def;
        def.storage = SettingDef::Storage::LibretroOption;
        QCOMPARE(def.storage, SettingDef::Storage::LibretroOption);
    }
};
QTEST_MAIN(TestSettingDefStorage)
#include "test_setting_def_storage.moc"
