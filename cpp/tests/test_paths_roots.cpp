#include <QtTest>
#include "core/paths.h"

class TestPathsRoots : public QObject {
    Q_OBJECT
private slots:
    void defaultsDeriveFromRoot() {
        Paths::setRoot("/tmp/rn-test");
        Paths::setRomsRoot("");   // unset ⇒ default
        Paths::setBiosRoot("");
        QCOMPARE(Paths::romsDir("gba"), QString("/tmp/rn-test/roms/gba"));
        QCOMPARE(Paths::biosDir(), QString("/tmp/rn-test/bios"));
    }
    void customRootsOverrideAndNormalize() {
        Paths::setRoot("/tmp/rn-test");
        Paths::setRomsRoot("/Volumes/USB//Games");   // doubled slash
        Paths::setBiosRoot("/Volumes/USB/bios/");     // trailing slash
        QCOMPARE(Paths::romsDir("psx"), QString("/Volumes/USB/Games/psx"));
        QCOMPARE(Paths::biosDir(), QString("/Volumes/USB/bios"));
    }
    void emptyResetsToDefault() {
        Paths::setRoot("/tmp/rn-test");
        Paths::setRomsRoot("/Volumes/USB/Games");
        Paths::setRomsRoot("");    // back to default
        QCOMPARE(Paths::romsDir(""), QString("/tmp/rn-test/roms"));
    }
};
QTEST_APPLESS_MAIN(TestPathsRoots)
#include "test_paths_roots.moc"
