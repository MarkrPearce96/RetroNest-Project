#include <QtTest>
#include "core/setting_def.h"

class TestSettingDefStorage : public QObject {
    Q_OBJECT
private slots:
    void testDefaultIsIni() {
        SettingDef def;
        QCOMPARE(static_cast<int>(def.storage), static_cast<int>(SettingDef::Storage::Ini));
    }
    void testStorageCanBeLibretroOption() {
        SettingDef def;
        def.storage = SettingDef::Storage::LibretroOption;
        QCOMPARE(static_cast<int>(def.storage), static_cast<int>(SettingDef::Storage::LibretroOption));
    }
};
QTEST_APPLESS_MAIN(TestSettingDefStorage)
#include "test_setting_def_storage.moc"
