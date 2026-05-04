#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/setting_def.h"
#include "core/ini_file.h"

// Smoke test of just the field — full ConfigService routing is exercised
// via DolphinSchema's Graphics keys writing to GFX.ini in the manual smoke.
class TestSettingDefIniRouting : public QObject {
    Q_OBJECT
private slots:
    void testIniFilePathFieldDefaultsEmpty() {
        SettingDef d;
        QVERIFY(d.iniFilePath.isEmpty());
    }

    void testIniFilePathRoundTrip() {
        SettingDef d;
        d.iniFilePath = "/tmp/example.ini";
        QCOMPARE(d.iniFilePath, QString("/tmp/example.ini"));
    }
};

QTEST_MAIN(TestSettingDefIniRouting)
#include "test_setting_def_ini_routing.moc"
