#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "core/migration_ppsspp.h"
#include "core/paths.h"

namespace {

bool touch(const QString& path, const QByteArray& contents = {}) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    if (!contents.isEmpty()) f.write(contents);
    f.close();
    return true;
}

} // namespace

class TestMigrationPpsspp : public QObject {
    Q_OBJECT
private slots:

    void noop_whenNothingPresent() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators");

        QVERIFY(MigrationPpsspp::runIfNeeded());
        QVERIFY(QFile::exists(tmp.path() + "/emulators/.ppsspp-libretro-migrated"));
        QVERIFY(!QDir(tmp.path() + "/emulators/ppsspp").exists());
    }

    void idempotent_skipsWhenSentinelPresent() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators");

        QVERIFY(touch(tmp.path() + "/emulators/.ppsspp-libretro-migrated"));

        // Fake standalone install: PPSSPPSDL.app + PSP/ data — migration must NOT touch it.
        QDir(tmp.path()).mkpath("emulators/ppsspp/PPSSPPSDL.app/Contents/MacOS");
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"));
        QDir(tmp.path()).mkpath("emulators/ppsspp/PSP/SAVEDATA");

        QVERIFY(MigrationPpsspp::runIfNeeded());
        QVERIFY(QDir(tmp.path() + "/emulators/ppsspp/PPSSPPSDL.app").exists());
        QVERIFY(QDir(tmp.path() + "/emulators/ppsspp/PSP/SAVEDATA").exists());
    }

    void removesStandaloneApp_andDropsVersionJson() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators/ppsspp/PPSSPPSDL.app/Contents/MacOS");
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"));
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/.version.json", "{\"version\":\"1.18\"}"));

        QVERIFY(MigrationPpsspp::runIfNeeded());

        QVERIFY(!QDir(tmp.path() + "/emulators/ppsspp/PPSSPPSDL.app").exists());
        QVERIFY(!QFile::exists(tmp.path() + "/emulators/ppsspp/.version.json"));
        QVERIFY(QFile::exists(tmp.path() + "/emulators/.ppsspp-libretro-migrated"));
    }

    void preservesUserDataAndDropsStandaloneInis() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators/ppsspp/PPSSPPSDL.app/Contents/MacOS");
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"));

        // User data the libretro core also reads — must survive.
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PSP/SAVEDATA/ULUS01234/SAVE.BIN", "save-bytes"));
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PSP/PPSSPP_STATE/slot1.ppst", "state-bytes"));
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PSP/Cheats/ULUS01234.ini", "cheat-bytes"));
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PSP/TEXTURES/ULUS01234/replace.png", "tex-bytes"));

        // Standalone INI files that libretro doesn't read — must be dropped.
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PSP/SYSTEM/ppsspp.ini", "[Foo]\nBar=1\n"));
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/PSP/SYSTEM/controls.ini", "[ControlMapping]\n"));

        QVERIFY(MigrationPpsspp::runIfNeeded());

        // The data dir is now lowercase. (On case-insensitive volumes both resolve;
        // we assert via the lowercase form because that's what libretro writes to.)
        const QString psp = tmp.path() + "/emulators/ppsspp/psp";
        QFile save(psp + "/SAVEDATA/ULUS01234/SAVE.BIN");
        QVERIFY(save.open(QIODevice::ReadOnly));
        QCOMPARE(save.readAll(), QByteArray("save-bytes"));
        save.close();

        QVERIFY(QFile::exists(psp + "/PPSSPP_STATE/slot1.ppst"));
        QVERIFY(QFile::exists(psp + "/Cheats/ULUS01234.ini"));
        QVERIFY(QFile::exists(psp + "/TEXTURES/ULUS01234/replace.png"));

        // Standalone INI files are gone.
        QVERIFY(!QFile::exists(psp + "/SYSTEM/ppsspp.ini"));
        QVERIFY(!QFile::exists(psp + "/SYSTEM/controls.ini"));
    }

    void migratesEvenWithoutDataDir() {
        // Standalone install whose user never launched a game: PPSSPPSDL.app
        // exists, .version.json exists, but no PSP/ data directory yet.
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators/ppsspp/PPSSPPSDL.app");
        QVERIFY(touch(tmp.path() + "/emulators/ppsspp/.version.json"));

        QVERIFY(MigrationPpsspp::runIfNeeded());

        QVERIFY(!QDir(tmp.path() + "/emulators/ppsspp/PPSSPPSDL.app").exists());
        QVERIFY(!QFile::exists(tmp.path() + "/emulators/ppsspp/.version.json"));
        QVERIFY(QFile::exists(tmp.path() + "/emulators/.ppsspp-libretro-migrated"));
    }
};

QTEST_MAIN(TestMigrationPpsspp)
#include "test_migration_ppsspp.moc"
