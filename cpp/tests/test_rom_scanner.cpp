#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "core/rom_scanner.h"
#include "core/manifest_loader.h"
#include "core/database.h"

class TestRomScanner : public QObject {
    Q_OBJECT

private:
    void writeFile(const QString& path, const QString& content = "") {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << content;
    }

private slots:

    void testParseM3u_relativePaths() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Create disc files
        writeFile(tmp.filePath("game (Disc 1).chd"));
        writeFile(tmp.filePath("game (Disc 2).chd"));

        // Create M3U referencing them
        writeFile(tmp.filePath("game.m3u"),
                  "game (Disc 1).chd\ngame (Disc 2).chd\n");

        QSet<QString> result = RomScanner::parseM3u(tmp.filePath("game.m3u"));
        QCOMPARE(result.size(), 2);
        QVERIFY(result.contains(QFileInfo(tmp.filePath("game (Disc 1).chd")).canonicalFilePath()));
        QVERIFY(result.contains(QFileInfo(tmp.filePath("game (Disc 2).chd")).canonicalFilePath()));
    }

    void testParseM3u_subfolder() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Create subfolder with discs
        QDir(tmp.path()).mkdir("game");
        writeFile(tmp.filePath("game/game (Disc 1).chd"));
        writeFile(tmp.filePath("game/game (Disc 2).chd"));

        // M3U at top level references subfolder
        writeFile(tmp.filePath("game.m3u"),
                  "game/game (Disc 1).chd\ngame/game (Disc 2).chd\n");

        QSet<QString> result = RomScanner::parseM3u(tmp.filePath("game.m3u"));
        QCOMPARE(result.size(), 2);
        QVERIFY(result.contains(QFileInfo(tmp.filePath("game/game (Disc 1).chd")).canonicalFilePath()));
        QVERIFY(result.contains(QFileInfo(tmp.filePath("game/game (Disc 2).chd")).canonicalFilePath()));
    }

    void testParseM3u_commentsAndBlankLines() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("disc1.bin"));

        writeFile(tmp.filePath("game.m3u"),
                  "# This is a comment\n"
                  "\n"
                  "disc1.bin\n"
                  "\n"
                  "# Another comment\n");

        QSet<QString> result = RomScanner::parseM3u(tmp.filePath("game.m3u"));
        QCOMPARE(result.size(), 1);
        QVERIFY(result.contains(QFileInfo(tmp.filePath("disc1.bin")).canonicalFilePath()));
    }

    void testParseM3u_nonExistentFilesIgnored() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("disc1.bin"));

        writeFile(tmp.filePath("game.m3u"),
                  "disc1.bin\ndisc2_missing.bin\n");

        QSet<QString> result = RomScanner::parseM3u(tmp.filePath("game.m3u"));
        QCOMPARE(result.size(), 1);
        QVERIFY(result.contains(QFileInfo(tmp.filePath("disc1.bin")).canonicalFilePath()));
    }

    void testParseM3u_absolutePaths() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("disc1.bin"));
        QString absPath = QFileInfo(tmp.filePath("disc1.bin")).canonicalFilePath();

        writeFile(tmp.filePath("game.m3u"), absPath + "\n");

        QSet<QString> result = RomScanner::parseM3u(tmp.filePath("game.m3u"));
        QCOMPARE(result.size(), 1);
        QVERIFY(result.contains(absPath));
    }

    void testScan_m3uSuppressesDiscFiles() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Create a system folder structure
        QDir(tmp.path()).mkpath("roms/psx");
        QString romDir = tmp.filePath("roms/psx");

        // Create disc files and M3U
        writeFile(romDir + "/FF7 (Disc 1).bin");
        writeFile(romDir + "/FF7 (Disc 2).bin");
        writeFile(romDir + "/FF7.m3u",
                  "FF7 (Disc 1).bin\nFF7 (Disc 2).bin\n");
        // A standalone game with no M3U
        writeFile(romDir + "/Crash Bandicoot.bin");

        // Set up a minimal manifest loader with bin + m3u extensions
        ManifestLoader loader;
        EmulatorManifest manifest;
        manifest.id = "duckstation";
        manifest.systems = {"psx"};
        manifest.rom_extensions = {"bin", "m3u"};
        loader.injectManifest(manifest);

        // Set up database
        QTemporaryDir dbDir;
        Database db;
        QVERIFY(db.open(dbDir.filePath("test.db")));

        auto result = RomScanner::scan(romDir, loader, db, "psx");

        // Should add FF7.m3u + Crash Bandicoot.bin = 2 games
        QCOMPARE(result.added, 2);

        // Verify the disc files were NOT added
        auto games = db.allGames();
        QCOMPARE(games.size(), 2);

        QSet<QString> titles;
        for (const auto& g : games)
            titles.insert(g.title);
        QVERIFY(titles.contains("FF7"));
        QVERIFY(titles.contains("Crash Bandicoot"));
    }

    void testScan_m3uSubfolderLayout() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QDir(tmp.path()).mkpath("roms/psx/FF7");
        QString romDir = tmp.filePath("roms/psx");

        // Discs in subfolder, M3U at system level
        writeFile(romDir + "/FF7/FF7 (Disc 1).chd");
        writeFile(romDir + "/FF7/FF7 (Disc 2).chd");
        writeFile(romDir + "/FF7.m3u",
                  "FF7/FF7 (Disc 1).chd\nFF7/FF7 (Disc 2).chd\n");
        // Auto-generate would also create FF7/FF7.m3u for the subfolder group,
        // so pre-create it to prevent a duplicate
        writeFile(romDir + "/FF7/FF7.m3u",
                  "FF7 (Disc 1).chd\nFF7 (Disc 2).chd\n");

        ManifestLoader loader;
        EmulatorManifest manifest;
        manifest.id = "duckstation";
        manifest.systems = {"psx"};
        manifest.rom_extensions = {"chd", "m3u"};
        loader.injectManifest(manifest);

        QTemporaryDir dbDir;
        Database db;
        QVERIFY(db.open(dbDir.filePath("test.db")));

        auto result = RomScanner::scan(romDir, loader, db, "psx");

        // Both M3Us should be added (system-level + subfolder), disc files suppressed
        QCOMPARE(result.added, 2);
        auto games = db.allGames();
        QCOMPARE(games.size(), 2);

        // Both should be titled "FF7"
        for (const auto& g : games)
            QCOMPARE(g.title, QString("FF7"));
    }

    void testAutoGenerate_basicDiscGroup() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("FF7 (Disc 1).chd"));
        writeFile(tmp.filePath("FF7 (Disc 2).chd"));
        writeFile(tmp.filePath("FF7 (Disc 3).chd"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 1);

        // Verify M3U was created
        QString m3uPath = tmp.filePath("FF7.m3u");
        QVERIFY(QFileInfo::exists(m3uPath));

        // Verify contents are sorted by disc number
        QFile f(m3uPath);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QStringList lines;
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) lines << line;
        }
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0], QString("FF7 (Disc 1).chd"));
        QCOMPARE(lines[1], QString("FF7 (Disc 2).chd"));
        QCOMPARE(lines[2], QString("FF7 (Disc 3).chd"));
    }

    void testAutoGenerate_diskSpelling() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("Game (Disk 1).bin"));
        writeFile(tmp.filePath("Game (Disk 2).bin"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 1);
        QVERIFY(QFileInfo::exists(tmp.filePath("Game.m3u")));
    }

    void testAutoGenerate_cdPattern() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("Game (CD1).bin"));
        writeFile(tmp.filePath("Game (CD2).bin"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 1);
        QVERIFY(QFileInfo::exists(tmp.filePath("Game.m3u")));
    }

    void testAutoGenerate_discOfPattern() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("Game (Disc 1 of 2).iso"));
        writeFile(tmp.filePath("Game (Disc 2 of 2).iso"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 1);
        QVERIFY(QFileInfo::exists(tmp.filePath("Game.m3u")));
    }

    void testAutoGenerate_skipsIfM3uExists() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("FF7 (Disc 1).chd"));
        writeFile(tmp.filePath("FF7 (Disc 2).chd"));
        writeFile(tmp.filePath("FF7.m3u"), "existing content\n");

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 0);

        // Verify existing M3U was NOT overwritten
        QFile f(tmp.filePath("FF7.m3u"));
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(f.readAll().trimmed(), QByteArray("existing content"));
    }

    void testAutoGenerate_singleDiscIgnored() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("Game (Disc 1).chd"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 0);
    }

    void testAutoGenerate_mixedExtensionsNotGrouped() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("Game (Disc 1).chd"));
        writeFile(tmp.filePath("Game (Disc 2).bin"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 0);
    }

    void testAutoGenerate_regionTagsPreserved() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("FF7 (USA) (Disc 1).chd"));
        writeFile(tmp.filePath("FF7 (USA) (Disc 2).chd"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 1);
        QVERIFY(QFileInfo::exists(tmp.filePath("FF7 (USA).m3u")));
    }

    void testAutoGenerate_caseInsensitive() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("Game (disc 1).chd"));
        writeFile(tmp.filePath("Game (DISC 2).chd"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 1);
        QVERIFY(QFileInfo::exists(tmp.filePath("Game.m3u")));
    }

    void testAutoGenerate_multipleGroups() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        writeFile(tmp.filePath("FF7 (Disc 1).chd"));
        writeFile(tmp.filePath("FF7 (Disc 2).chd"));
        writeFile(tmp.filePath("FF8 (Disc 1).chd"));
        writeFile(tmp.filePath("FF8 (Disc 2).chd"));
        writeFile(tmp.filePath("FF8 (Disc 3).chd"));

        int created = RomScanner::autoGenerateM3u(tmp.path());
        QCOMPARE(created, 2);
        QVERIFY(QFileInfo::exists(tmp.filePath("FF7.m3u")));
        QVERIFY(QFileInfo::exists(tmp.filePath("FF8.m3u")));
    }

    void testScan_autoGeneratesM3uAndSuppresses() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QDir(tmp.path()).mkpath("roms/psx");
        QString romDir = tmp.filePath("roms/psx");

        // Multi-disc files with no M3U
        writeFile(romDir + "/FF7 (Disc 1).chd");
        writeFile(romDir + "/FF7 (Disc 2).chd");
        writeFile(romDir + "/FF7 (Disc 3).chd");
        // Standalone game
        writeFile(romDir + "/Crash.bin");

        ManifestLoader loader;
        EmulatorManifest manifest;
        manifest.id = "duckstation";
        manifest.systems = {"psx"};
        manifest.rom_extensions = {"chd", "bin", "m3u"};
        loader.injectManifest(manifest);

        QTemporaryDir dbDir;
        Database db;
        QVERIFY(db.open(dbDir.filePath("test.db")));

        auto result = RomScanner::scan(romDir, loader, db, "psx");

        // Should auto-generate FF7.m3u, suppress 3 disc files, add FF7.m3u + Crash.bin
        QCOMPARE(result.added, 2);
        auto games = db.allGames();
        QCOMPARE(games.size(), 2);

        QSet<QString> titles;
        for (const auto& g : games)
            titles.insert(g.title);
        QVERIFY(titles.contains("FF7"));
        QVERIFY(titles.contains("Crash"));

        // Verify auto-generated M3U file exists
        QVERIFY(QFileInfo::exists(romDir + "/FF7.m3u"));
    }

    void testScan_discCountSetForM3u() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QDir(tmp.path()).mkpath("roms/psx");
        QString romDir = tmp.filePath("roms/psx");

        writeFile(romDir + "/FF7 (Disc 1).chd");
        writeFile(romDir + "/FF7 (Disc 2).chd");
        writeFile(romDir + "/FF7 (Disc 3).chd");

        ManifestLoader loader;
        EmulatorManifest manifest;
        manifest.id = "duckstation";
        manifest.systems = {"psx"};
        manifest.rom_extensions = {"chd", "m3u"};
        loader.injectManifest(manifest);

        QTemporaryDir dbDir;
        Database db;
        QVERIFY(db.open(dbDir.filePath("test.db")));

        RomScanner::scan(romDir, loader, db, "psx");

        auto games = db.allGames();
        QCOMPARE(games.size(), 1);
        QCOMPARE(games[0].title, QString("FF7"));
        QCOMPARE(games[0].disc_count, 3);
    }

    void testScan_discCountZeroForNonM3u() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QDir(tmp.path()).mkpath("roms/psx");
        QString romDir = tmp.filePath("roms/psx");

        writeFile(romDir + "/Crash.bin");

        ManifestLoader loader;
        EmulatorManifest manifest;
        manifest.id = "duckstation";
        manifest.systems = {"psx"};
        manifest.rom_extensions = {"bin", "m3u"};
        loader.injectManifest(manifest);

        QTemporaryDir dbDir;
        Database db;
        QVERIFY(db.open(dbDir.filePath("test.db")));

        RomScanner::scan(romDir, loader, db, "psx");

        auto games = db.allGames();
        QCOMPARE(games.size(), 1);
        QCOMPARE(games[0].disc_count, 0);
    }

    void testScan_staleDiscEntriesRemoved() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QDir(tmp.path()).mkpath("roms/psx");
        QString romDir = tmp.filePath("roms/psx");

        // Create disc files
        writeFile(romDir + "/FF7 (Disc 1).chd");
        writeFile(romDir + "/FF7 (Disc 2).chd");

        ManifestLoader loader;
        EmulatorManifest manifest;
        manifest.id = "duckstation";
        manifest.systems = {"psx"};
        manifest.rom_extensions = {"chd", "m3u"};
        loader.injectManifest(manifest);

        QTemporaryDir dbDir;
        Database db;
        QVERIFY(db.open(dbDir.filePath("test.db")));

        // Manually add disc entries to simulate pre-existing imports
        GameRecord d1;
        d1.title = "FF7 (Disc 1)";
        d1.rom_path = QFileInfo(romDir + "/FF7 (Disc 1).chd").canonicalFilePath();
        d1.system = "psx";
        d1.emulator_id = "duckstation";
        db.addGame(d1);

        GameRecord d2;
        d2.title = "FF7 (Disc 2)";
        d2.rom_path = QFileInfo(romDir + "/FF7 (Disc 2).chd").canonicalFilePath();
        d2.system = "psx";
        d2.emulator_id = "duckstation";
        db.addGame(d2);

        QCOMPARE(db.allGames().size(), 2);

        // Rescan — auto-generate creates M3U, suppresses discs, removes stale entries
        auto result = RomScanner::scan(romDir, loader, db, "psx");

        // The M3U was added, the 2 disc entries should have been cleaned up
        QCOMPARE(result.added, 1);  // FF7.m3u
        auto games = db.allGames();
        QCOMPARE(games.size(), 1);
        QCOMPARE(games[0].title, QString("FF7"));
        QVERIFY(games[0].rom_path.endsWith(".m3u"));
    }

    void testScan_singleDiscFileShowsNormally() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QDir(tmp.path()).mkpath("roms/psx");
        QString romDir = tmp.filePath("roms/psx");

        // A single disc file (not part of a multi-disc group) — should appear as-is
        writeFile(romDir + "/FF7 (Disc 1).bin");

        ManifestLoader loader;
        EmulatorManifest manifest;
        manifest.id = "duckstation";
        manifest.systems = {"psx"};
        manifest.rom_extensions = {"bin", "m3u"};
        loader.injectManifest(manifest);

        QTemporaryDir dbDir;
        Database db;
        QVERIFY(db.open(dbDir.filePath("test.db")));

        auto result = RomScanner::scan(romDir, loader, db, "psx");
        // Single disc files are not grouped, so no M3U auto-generated
        QCOMPARE(result.added, 1);
        auto games = db.allGames();
        QCOMPARE(games.size(), 1);
        QCOMPARE(games[0].title, QString("FF7 (Disc 1)"));
    }
};

QTEST_MAIN(TestRomScanner)
#include "test_rom_scanner.moc"
