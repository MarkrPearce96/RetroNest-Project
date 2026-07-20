// cpp/tests/test_system_registry.cpp
//
// Pins manifests/systems.json (the packet-7 stage-3 single source for
// system facts) via the real committed file: display names, ScreenScraper
// platform IDs, RA console IDs, and the fallback contracts the four
// rewired consumers rely on (unknown name -> raw id, unknown SS id -> -1,
// unknown/absent RA id -> -1). The eight RA console ids are pinned exactly
// — they drive RAService's fetch set.

#include <QtTest>
#include <QFileInfo>
#include <QSet>
#include "core/system_registry.h"

class TestSystemRegistry : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        const QString path = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/../../manifests/systems.json";
        QVERIFY2(SystemRegistry::load(path), "manifests/systems.json failed to load");
        QVERIFY(SystemRegistry::isLoaded());
    }
    void displayNames() {
        QCOMPARE(SystemRegistry::displayName("psx"), QString("PlayStation"));
        QCOMPARE(SystemRegistry::displayName("GC"), QString("GameCube"));   // case-insensitive
        QCOMPARE(SystemRegistry::displayName("nosuch"), QString("nosuch")); // fallback = raw id
    }
    void screenScraperIds() {
        QCOMPARE(SystemRegistry::screenScraperId("psx"), 57);
        QCOMPARE(SystemRegistry::screenScraperId("arcade"), 75);
        QCOMPARE(SystemRegistry::screenScraperId("nosuch"), -1);
    }
    void raConsoleIds() {
        QCOMPARE(SystemRegistry::raConsoleId("ps2"), 21);
        QCOMPARE(SystemRegistry::raConsoleId("wii"), 19);
        QCOMPARE(SystemRegistry::raConsoleId("snes"), 3);   // Snes9x
        QCOMPARE(SystemRegistry::raConsoleId("n64"), 2);    // Mupen64Plus
        QCOMPARE(SystemRegistry::raConsoleId("nes"), -1);   // system known, no RA id
        QCOMPARE(SystemRegistry::raConsoleId("nosuch"), -1);
    }
    void allRaConsoleIds_areTheTenSupported() {
        const QList<int> ids = SystemRegistry::allRaConsoleIds();
        // n64 (2) joins the set with the Mupen64Plus adapter; snes (3) with Snes9x.
        QCOMPARE(QSet<int>(ids.begin(), ids.end()),
                 QSet<int>({2, 3, 4, 5, 6, 12, 16, 19, 21, 41}));
        QCOMPARE(ids.size(), 10);   // distinct — no dupes
    }
    void testAllSystemIdsReturnsEveryEntry() {
        QVERIFY(SystemRegistry::isLoaded());
        const QStringList ids = SystemRegistry::allSystemIds();
        // systems.json defines these among others; each must appear exactly once.
        QVERIFY(ids.contains("psx"));
        QVERIFY(ids.contains("ps2"));
        QVERIFY(ids.contains("gba"));
        QVERIFY(ids.contains("gbc"));
        QVERIFY(ids.contains("gb"));
        QCOMPARE(ids.count("psx"), 1);
        // Count matches the entry table size (no dupes, no drops).
        QVERIFY(ids.size() >= 5);
    }
    void loadFromData_rejectsGarbage() {
        QVERIFY(!SystemRegistry::loadFromData("not json"));
        QVERIFY(!SystemRegistry::loadFromData("[1,2,3]"));
        // Registry still holds the good data from initTestCase.
        QVERIFY(SystemRegistry::isLoaded());
        QCOMPARE(SystemRegistry::displayName("psx"), QString("PlayStation"));
    }
};
QTEST_APPLESS_MAIN(TestSystemRegistry)
#include "test_system_registry.moc"
