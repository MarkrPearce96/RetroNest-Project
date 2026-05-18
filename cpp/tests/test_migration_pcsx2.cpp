#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "core/migration_pcsx2.h"
#include "core/paths.h"

class TestMigrationPcsx2 : public QObject {
    Q_OBJECT
private slots:

    void noop_whenNothingPresent() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators");

        QVERIFY(MigrationPcsx2::runIfNeeded());
        QVERIFY(QFile::exists(tmp.path() + "/emulators/.sp8-migrated"));
        QVERIFY(!QDir(tmp.path() + "/emulators/.archive").exists());
    }

    void idempotent_skipsWhenSentinelPresent() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators");

        QFile sentinel(tmp.path() + "/emulators/.sp8-migrated");
        QVERIFY(sentinel.open(QIODevice::WriteOnly));
        sentinel.close();

        // Create a fake standalone install — migration should NOT touch it
        QDir(tmp.path()).mkpath("emulators/pcsx2");
        QFile portable(tmp.path() + "/emulators/pcsx2/portable.txt");
        QVERIFY(portable.open(QIODevice::WriteOnly)); portable.close();

        QVERIFY(MigrationPcsx2::runIfNeeded());
        QVERIFY(QFile::exists(tmp.path() + "/emulators/pcsx2/portable.txt"));
        QVERIFY(!QDir(tmp.path() + "/emulators/.archive").exists());
    }

    void archivesStandalone_andPromotesLibretro() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());

        // Standalone install layout
        QDir(tmp.path()).mkpath("emulators/pcsx2/ps2/memcards");
        QFile portable(tmp.path() + "/emulators/pcsx2/portable.txt");
        QVERIFY(portable.open(QIODevice::WriteOnly)); portable.close();
        QFile mcStandalone(tmp.path() + "/emulators/pcsx2/ps2/memcards/Mcd001.ps2");
        QVERIFY(mcStandalone.open(QIODevice::WriteOnly));
        mcStandalone.write("standalone"); mcStandalone.close();

        // Libretro data dir
        QDir(tmp.path()).mkpath("emulators/pcsx2-libretro/ps2/memcards");
        QFile mcLibretro(tmp.path() + "/emulators/pcsx2-libretro/ps2/memcards/Mcd001.ps2");
        QVERIFY(mcLibretro.open(QIODevice::WriteOnly));
        mcLibretro.write("libretro"); mcLibretro.close();

        QVERIFY(MigrationPcsx2::runIfNeeded());

        // Archive directory exists with standalone marker inside
        QDir archives(tmp.path() + "/emulators/.archive");
        QVERIFY(archives.exists());
        const auto entries = archives.entryList(QStringList() << "pcsx2-standalone-*", QDir::Dirs);
        QCOMPARE(entries.size(), 1);
        QVERIFY(QFile::exists(archives.absolutePath() + "/" + entries.first() + "/portable.txt"));

        // emulators/pcsx2/ now contains libretro data
        QFile promoted(tmp.path() + "/emulators/pcsx2/ps2/memcards/Mcd001.ps2");
        QVERIFY(promoted.open(QIODevice::ReadOnly));
        QCOMPARE(promoted.readAll(), QByteArray("libretro"));
        promoted.close();

        // libretro dir is gone
        QVERIFY(!QDir(tmp.path() + "/emulators/pcsx2-libretro").exists());

        // Sentinel touched
        QVERIFY(QFile::exists(tmp.path() + "/emulators/.sp8-migrated"));
    }

    void promotesLibretroOnly_whenNoStandalone() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators/pcsx2-libretro/ps2");
        QFile f(tmp.path() + "/emulators/pcsx2-libretro/ps2/marker");
        QVERIFY(f.open(QIODevice::WriteOnly)); f.close();

        QVERIFY(MigrationPcsx2::runIfNeeded());

        QVERIFY(QFile::exists(tmp.path() + "/emulators/pcsx2/ps2/marker"));
        QVERIFY(!QDir(tmp.path() + "/emulators/pcsx2-libretro").exists());
        QVERIFY(!QDir(tmp.path() + "/emulators/.archive").exists());
    }
};

QTEST_MAIN(TestMigrationPcsx2)
#include "test_migration_pcsx2.moc"
