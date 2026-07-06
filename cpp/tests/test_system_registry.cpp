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
        QCOMPARE(SystemRegistry::raConsoleId("nes"), -1);   // system known, no RA id
        QCOMPARE(SystemRegistry::raConsoleId("nosuch"), -1);
    }
    void allRaConsoleIds_areTheEightSupported() {
        const QList<int> ids = SystemRegistry::allRaConsoleIds();
        QCOMPARE(QSet<int>(ids.begin(), ids.end()),
                 QSet<int>({4, 5, 6, 12, 16, 19, 21, 41}));
        QCOMPARE(ids.size(), 8);   // distinct — no dupes
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
