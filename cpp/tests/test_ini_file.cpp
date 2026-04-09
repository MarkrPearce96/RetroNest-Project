#include <QtTest>
#include <QTemporaryFile>
#include <QTextStream>
#include "core/ini_file.h"

class TestIniFile : public QObject {
    Q_OBJECT

private:
    QString writeTempIni(const QString& content) {
        QTemporaryFile* tmp = new QTemporaryFile(this);
        tmp->setAutoRemove(true);
        (void)tmp->open();
        QTextStream out(tmp);
        out << content;
        out.flush();
        return tmp->fileName();
    }

private slots:
    void testLoadAndReadValues() {
        QString path = writeTempIni(
            "[UI]\n"
            "StartFullscreen = true\n"
            "Theme = dark\n"
            "\n"
            "[Folders]\n"
            "Bios = /path/to/bios\n"
        );
        IniFile ini;
        QVERIFY(ini.load(path));
        QCOMPARE(ini.value("UI", "StartFullscreen"), "true");
        QCOMPARE(ini.value("UI", "Theme"), "dark");
        QCOMPARE(ini.value("Folders", "Bios"), "/path/to/bios");
    }

    void testDefaultValue() {
        QString path = writeTempIni("[UI]\nTheme = dark\n");
        IniFile ini;
        QVERIFY(ini.load(path));
        QCOMPARE(ini.value("UI", "Missing", "fallback"), "fallback");
        QCOMPARE(ini.value("NoSection", "NoKey", "default"), "default");
    }

    void testContainsKey() {
        QString path = writeTempIni("[UI]\nTheme = dark\n");
        IniFile ini;
        QVERIFY(ini.load(path));
        QVERIFY(ini.containsKey("UI", "Theme"));
        QVERIFY(!ini.containsKey("UI", "Missing"));
        QVERIFY(!ini.containsKey("NoSection", "Theme"));
    }

    void testSetValueExistingKey() {
        QString path = writeTempIni("[UI]\nTheme = dark\n");
        IniFile ini;
        QVERIFY(ini.load(path));
        ini.setValue("UI", "Theme", "light");
        QCOMPARE(ini.value("UI", "Theme"), "light");
    }

    void testSetValueNewKey() {
        QString path = writeTempIni("[UI]\nTheme = dark\n");
        IniFile ini;
        QVERIFY(ini.load(path));
        ini.setValue("UI", "NewKey", "newval");
        QCOMPARE(ini.value("UI", "NewKey"), "newval");
    }

    void testSetValueNewSection() {
        QString path = writeTempIni("[UI]\nTheme = dark\n");
        IniFile ini;
        QVERIFY(ini.load(path));
        ini.setValue("NewSection", "Key", "val");
        QCOMPARE(ini.value("NewSection", "Key"), "val");
    }

    void testRoundTrip() {
        QString path = writeTempIni(
            "; comment line\n"
            "[UI]\n"
            "StartFullscreen = true\n"
            "\n"
            "[Folders]\n"
            "Bios = /path/to/bios\n"
        );
        IniFile ini;
        QVERIFY(ini.load(path));
        ini.setValue("UI", "StartFullscreen", "false");

        QTemporaryFile out;
        (void)out.open();
        QString outPath = out.fileName();
        out.close();

        QVERIFY(ini.save(outPath));

        // Reload and verify
        IniFile ini2;
        QVERIFY(ini2.load(outPath));
        QCOMPARE(ini2.value("UI", "StartFullscreen"), "false");
        QCOMPARE(ini2.value("Folders", "Bios"), "/path/to/bios");
    }

    void testKeys() {
        QString path = writeTempIni(
            "[UI]\n"
            "A = 1\n"
            "B = 2\n"
            "C = 3\n"
        );
        IniFile ini;
        QVERIFY(ini.load(path));
        QStringList k = ini.keys("UI");
        QCOMPARE(k.size(), 3);
        QVERIFY(k.contains("A"));
        QVERIFY(k.contains("B"));
        QVERIFY(k.contains("C"));
    }

    void testLoadNonexistentFile() {
        IniFile ini;
        QVERIFY(!ini.load("/nonexistent/path/file.ini"));
    }

    void testEmptyFile() {
        QString path = writeTempIni("");
        IniFile ini;
        QVERIFY(ini.load(path));
        QCOMPARE(ini.value("Any", "Key", "default"), "default");
    }
};

QTEST_MAIN(TestIniFile)
#include "test_ini_file.moc"
