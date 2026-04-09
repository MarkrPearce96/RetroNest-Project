# Multi-Disc Auto-Detect & Disc Count Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Auto-detect multi-disc ROM groups and generate M3U playlists during scanning, store disc count as metadata, and expose it through the game list model for themes.

**Architecture:** A new `autoGenerateM3u()` static helper on `RomScanner` detects disc-indicator patterns in filenames, groups them, and writes M3U files before the existing suppression pass. Disc count is stored in a new DB column and flows through `GameListModel` and `ThemeContext`. Stale disc entries are cleaned up after the main scan loop.

**Tech Stack:** C++17, Qt6 (QRegularExpression, QFile, QDir, QSet, QSqlQuery)

**Spec:** `docs/superpowers/specs/2026-04-04-multidisc-autodetect-disccount-design.md`

---

### Task 1: Add disc_count to database schema and GameRecord

**Files:**
- Modify: `cpp/src/core/database.h`
- Modify: `cpp/src/core/database.cpp`

This task adds the `disc_count` field to `GameRecord`, creates the v3→v4 migration, and updates all DB read/write operations.

- [ ] **Step 1: Add `disc_count` field to `GameRecord` in `database.h`**

In `cpp/src/core/database.h`, add after the `video_path` field (line 35) and before the `// User data` comment:

```cpp
    int disc_count = 0;  // number of discs (0 = single-disc or non-M3U)
```

Also add a new method to the `Database` class public section:

```cpp
    bool removeGameByPath(const QString& romPath);
```

- [ ] **Step 2: Update `CURRENT_SCHEMA_VERSION` to 4**

In `cpp/src/core/database.cpp`, change line 41:

```cpp
static const int CURRENT_SCHEMA_VERSION = 4;
```

- [ ] **Step 3: Add v3→v4 migration**

In `cpp/src/core/database.cpp`, in the `runMigrations()` method, add after the `if (current < 3)` block and before the final `if (current < CURRENT_SCHEMA_VERSION)`:

```cpp
    if (current < 4) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        QSqlQuery q(db);
        if (!q.exec("ALTER TABLE games ADD COLUMN disc_count INTEGER NOT NULL DEFAULT 0")) {
            qCritical() << "[Database] Migration v3→v4 failed:" << q.lastError().text();
            return false;
        }
        qInfo() << "[Database] Migrated schema v3 → v4 (added disc_count column)";
    }
```

- [ ] **Step 4: Update `GAME_SELECT_COLUMNS` and `recordFromQuery`**

In `cpp/src/core/database.cpp`, update the `GAME_SELECT_COLUMNS` string (line 234) to add `disc_count` at the end:

```cpp
static const char* GAME_SELECT_COLUMNS =
    "id, title, rom_path, system, emulator_id, cover_path, "
    "description, developer, publisher, release_date, genres, "
    "rating, players, last_played, play_count, favorite, "
    "screenshot_path, titlescreen_path, marquee_path, fanart_path, "
    "box3d_path, backcover_path, miximage_path, physicalmedia_path, "
    "manual_path, video_path, disc_count";
```

In `recordFromQuery` (line 242), add after `g.video_path` (the last field, index 25):

```cpp
    g.disc_count     = q.value(26).toInt();
```

- [ ] **Step 5: Update `addGame` to include `disc_count`**

In `cpp/src/core/database.cpp`, update the `addGame` method (line 218). Change the INSERT statement:

```cpp
    q.prepare("INSERT INTO games (title, rom_path, system, emulator_id, cover_path, disc_count) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(game.title);
    q.addBindValue(romPath);
    q.addBindValue(game.system);
    q.addBindValue(game.emulator_id);
    q.addBindValue(game.cover_path.isNull() ? QString("") : game.cover_path);
    q.addBindValue(game.disc_count);
```

- [ ] **Step 6: Implement `removeGameByPath`**

Add to `cpp/src/core/database.cpp`, after the `removeGame` method:

```cpp
bool Database::removeGameByPath(const QString& romPath) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    const QString normalized = QFileInfo(romPath).canonicalFilePath();
    const QString path = normalized.isEmpty() ? romPath : normalized;
    QSqlQuery q(db);
    q.prepare("DELETE FROM games WHERE rom_path = ?");
    q.addBindValue(path);
    if (!q.exec()) {
        qWarning() << "[Database] removeGameByPath failed:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}
```

- [ ] **Step 7: Build to verify compilation**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```

Expected: Clean build. Existing tests still pass.

- [ ] **Step 8: Commit**

```bash
git add cpp/src/core/database.h cpp/src/core/database.cpp
git commit -m "feat: add disc_count column to games table (schema v4)

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Add auto-detect and auto-generate M3U logic with tests

**Files:**
- Modify: `cpp/src/core/rom_scanner.h`
- Modify: `cpp/src/core/rom_scanner.cpp`
- Modify: `cpp/tests/test_rom_scanner.cpp`

This task adds the `autoGenerateM3u()` helper that detects multi-disc groups and writes M3U files.

- [ ] **Step 1: Add `autoGenerateM3u` declaration to `rom_scanner.h`**

Add after the `parseM3u` declaration, before the closing `};`:

```cpp
    /** Detect multi-disc ROM groups in a directory and auto-generate M3U playlists.
     *  Only generates for groups of 2+ files with matching disc-indicator patterns,
     *  same extension, same directory, and no existing M3U. Returns number of M3Us created. */
    static int autoGenerateM3u(const QString& directory);
```

- [ ] **Step 2: Write tests for auto-detect**

Add these test slots to `cpp/tests/test_rom_scanner.cpp`, before the closing `};`:

```cpp
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
```

- [ ] **Step 3: Build and verify tests fail**

Run:
```bash
cd cpp && cmake --build build --target test_rom_scanner && ./build/test_rom_scanner
```

Expected: Linker error — `autoGenerateM3u` not defined.

- [ ] **Step 4: Implement `autoGenerateM3u`**

Add `#include <QRegularExpression>` to the includes at the top of `cpp/src/core/rom_scanner.cpp`.

Add the implementation after `parseM3u` and before `scan`:

```cpp
int RomScanner::autoGenerateM3u(const QString& directory) {
    // Regex matches: (Disc N), (Disk N), (CD N), (Disc N of M) — case-insensitive
    static const QRegularExpression discPattern(
        R"(\((?:Disc|Disk|CD)\s*(\d+)(?:\s*of\s*\d+)?\))",
        QRegularExpression::CaseInsensitiveOption);

    // Group key: baseName + extension + directory
    struct DiscEntry {
        int discNumber;
        QString fileName;
    };
    // key = "dirPath|baseName|ext"
    QHash<QString, QVector<DiscEntry>> groups;

    QDirIterator it(directory, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        const QString fileName = fi.fileName();

        auto match = discPattern.match(fileName);
        if (!match.hasMatch()) continue;

        int discNum = match.captured(1).toInt();

        // Strip the disc indicator to get the base name
        QString baseName = fileName.left(match.capturedStart())
                         + fileName.mid(match.capturedEnd());
        // Remove extension to get the base title
        baseName = QFileInfo(baseName).completeBaseName().trimmed();
        const QString ext = fi.suffix().toLower();
        const QString dir = fi.absolutePath();

        QString key = dir + "|" + baseName + "|" + ext;
        groups[key].append({discNum, fileName});
    }

    int created = 0;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const auto& entries = it.value();
        if (entries.size() < 2) continue;

        // Parse the key to get dir and baseName
        const QStringList parts = it.key().split('|');
        const QString dir = parts[0];
        const QString baseName = parts[1];

        // Check if M3U already exists
        const QString m3uPath = dir + "/" + baseName + ".m3u";
        if (QFileInfo::exists(m3uPath)) continue;

        // Sort by disc number
        QVector<DiscEntry> sorted = entries;
        std::sort(sorted.begin(), sorted.end(),
                  [](const DiscEntry& a, const DiscEntry& b) { return a.discNumber < b.discNumber; });

        // Write M3U
        QFile m3uFile(m3uPath);
        if (!m3uFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[Scanner] Cannot create M3U file:" << m3uPath;
            continue;
        }
        QTextStream out(&m3uFile);
        for (const auto& entry : sorted)
            out << entry.fileName << "\n";

        created++;
        qInfo() << "[Scanner] Auto-generated:" << baseName + ".m3u"
                << "(" << sorted.size() << "discs)";
    }

    return created;
}
```

- [ ] **Step 5: Build and run tests**

Run:
```bash
cd cpp && cmake --build build --target test_rom_scanner && ./build/test_rom_scanner
```

Expected: All tests pass (5 parseM3u + 3 integration + 10 autoGenerate = 18).

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/rom_scanner.h cpp/src/core/rom_scanner.cpp cpp/tests/test_rom_scanner.cpp
git commit -m "feat: auto-detect multi-disc groups and generate M3U playlists

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Wire auto-generate, disc count, and stale cleanup into scan()

**Files:**
- Modify: `cpp/src/core/rom_scanner.cpp` (the `scan` method)
- Modify: `cpp/tests/test_rom_scanner.cpp` (integration tests)

This task wires everything together: auto-generate before suppression, disc count during file iteration, and stale cleanup after.

- [ ] **Step 1: Add integration tests**

Add these test slots to `cpp/tests/test_rom_scanner.cpp`, before the closing `};`:

```cpp
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

        // First scan: no M3U, disc files imported individually
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

        // First scan — picks up individual discs (auto-generate creates M3U)
        // Actually, auto-generate will trigger on first scan too, so we need
        // to simulate the case where discs were imported BEFORE auto-generate existed.
        // We'll manually add the disc entries, then create the M3U, then rescan.

        // Manually add disc entries
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

        // Now rescan — auto-generate creates M3U, suppresses discs, removes stale entries
        auto result = RomScanner::scan(romDir, loader, db, "psx");

        // The M3U was added, the 2 disc entries should have been cleaned up
        QCOMPARE(result.added, 1);  // FF7.m3u
        auto games = db.allGames();
        QCOMPARE(games.size(), 1);
        QCOMPARE(games[0].title, QString("FF7"));
        QVERIFY(games[0].rom_path.endsWith(".m3u"));
    }
```

- [ ] **Step 2: Build and verify new tests fail**

Run:
```bash
cd cpp && cmake --build build --target test_rom_scanner && ./build/test_rom_scanner
```

Expected: The 4 new integration tests fail because `scan()` doesn't call `autoGenerateM3u()`, doesn't set `disc_count`, and doesn't clean up stale entries.

- [ ] **Step 3: Wire `autoGenerateM3u` into `scan()`**

In `cpp/src/core/rom_scanner.cpp`, in the `scan()` method, add the auto-generate call **after** the bios dir setup (line 95) and **before** the M3U suppression pass (line 97). Insert:

```cpp
    // Auto-detect multi-disc groups and generate M3U playlists
    autoGenerateM3u(directory);
```

- [ ] **Step 4: Add disc count logic to the file iteration loop**

In `cpp/src/core/rom_scanner.cpp`, in the `scan()` method, after the line `game.emulator_id = it->emuId;` (line 138) and before `int addResult = db.addGame(game);` (line 140), add:

```cpp
        // Set disc count for M3U files
        if (ext == "m3u") {
            game.disc_count = static_cast<int>(parseM3u(romPath).size());
        }
```

- [ ] **Step 5: Add stale entry cleanup after the main scan loop**

In `cpp/src/core/rom_scanner.cpp`, in the `scan()` method, after the main `while (dirIt.hasNext())` loop ends and before the `qInfo() << "[Scanner] Scan complete:"` line, add:

```cpp
    // Remove stale DB entries for files that are now suppressed by M3U playlists
    if (!suppressedPaths.isEmpty()) {
        for (const QString& suppressed : suppressedPaths) {
            if (db.removeGameByPath(suppressed)) {
                qInfo() << "[Scanner] Removed stale disc entry:" << suppressed;
            }
        }
    }
```

- [ ] **Step 6: Build and run all tests**

Run:
```bash
cd cpp && cmake --build build --target test_rom_scanner && ./build/test_rom_scanner
```

Expected: All tests pass (5 parseM3u + 3 scan integration + 10 autoGenerate + 4 new integration = 22).

- [ ] **Step 7: Build full app to verify no regressions**

Run:
```bash
cd cpp && cmake --build build
```

Expected: Clean build.

- [ ] **Step 8: Commit**

```bash
git add cpp/src/core/rom_scanner.cpp cpp/tests/test_rom_scanner.cpp
git commit -m "feat: wire auto-generate, disc count, and stale cleanup into scan

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Expose disc count through GameListModel and ThemeContext

**Files:**
- Modify: `cpp/src/ui/game_list_model.h`
- Modify: `cpp/src/ui/game_list_model.cpp`
- Modify: `cpp/src/ui/theme_context.cpp`

This task makes `disc_count` available to themes via the game list model and the theme context detail map.

- [ ] **Step 1: Add `DiscCountRole` to `GameListModel`**

In `cpp/src/ui/game_list_model.h`, add to the `Roles` enum after `VideoPathRole`:

```cpp
        DiscCountRole,
```

- [ ] **Step 2: Add the role to `data()` in `game_list_model.cpp`**

In `cpp/src/ui/game_list_model.cpp`, in the `data()` method's switch statement, add before `default:`:

```cpp
    case DiscCountRole:   return g.disc_count;
```

- [ ] **Step 3: Add the role name mapping in `roleNames()`**

In `cpp/src/ui/game_list_model.cpp`, in the `roleNames()` method, add after the `VideoPathRole` entry:

```cpp
        {DiscCountRole,         "discCount"},
```

- [ ] **Step 4: Add `discCount` to `ThemeContext::gameDetails()`**

In `cpp/src/ui/theme_context.cpp`, in the `gameDetails()` method, add after `map["videoPath"]`:

```cpp
    map["discCount"]         = g.disc_count;
```

- [ ] **Step 5: Build to verify compilation**

Run:
```bash
cd cpp && cmake --build build
```

Expected: Clean build.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/ui/game_list_model.h cpp/src/ui/game_list_model.cpp cpp/src/ui/theme_context.cpp
git commit -m "feat: expose disc count through GameListModel and ThemeContext

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```
