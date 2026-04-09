# M3U Multi-Disc Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add M3U playlist support so multi-disc games appear as a single entry in the game list, with disc files suppressed from scanning.

**Architecture:** Two-pass scan in `RomScanner::scan` — first pass collects M3U-referenced files into a suppression set, second pass is the existing scan that skips suppressed files. M3U gets added to manifest `rom_extensions` so it flows through the existing extension-matching logic.

**Tech Stack:** C++17, Qt6 (QFile, QDir, QSet, QFileInfo)

**Spec:** `docs/superpowers/specs/2026-04-04-m3u-multidisc-support-design.md`

---

### Task 1: Add M3U parsing helper and tests

**Files:**
- Create: `cpp/tests/test_rom_scanner.cpp`
- Modify: `cpp/src/core/rom_scanner.h`
- Modify: `cpp/src/core/rom_scanner.cpp`
- Modify: `cpp/CMakeLists.txt`

This task adds a static helper `parseM3u()` that reads an M3U file and returns the canonical paths of referenced files. We test it in isolation before wiring it into the scan.

- [ ] **Step 1: Add `parseM3u` declaration to `rom_scanner.h`**

Add this private helper below the existing `scanStructured` declaration, just before the closing `};`:

```cpp
/** Parse an M3U file and return canonical absolute paths of referenced files.
 *  Skips comments (#), empty lines, and non-existent entries. */
static QSet<QString> parseM3u(const QString& m3uPath);
```

- [ ] **Step 2: Write the test file**

Create `cpp/tests/test_rom_scanner.cpp`:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "core/rom_scanner.h"

class TestRomScanner : public QObject {
    Q_OBJECT

private:
    void writeFile(const QString& path, const QString& content = "") {
        QFile f(path);
        f.open(QIODevice::WriteOnly);
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
};

QTEST_MAIN(TestRomScanner)
#include "test_rom_scanner.moc"
```

- [ ] **Step 3: Add test target to CMakeLists.txt**

Add at the bottom of `cpp/CMakeLists.txt`, after the existing `test_ini_file` block:

```cmake
add_executable(test_rom_scanner
    tests/test_rom_scanner.cpp
    src/core/rom_scanner.cpp
    src/core/manifest_loader.cpp
    src/core/paths.cpp
    src/core/database.cpp
)
target_include_directories(test_rom_scanner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_rom_scanner PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
add_test(NAME RomScanner COMMAND test_rom_scanner)
```

- [ ] **Step 4: Build and verify tests fail**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build --target test_rom_scanner
```

Expected: Linker error — `RomScanner::parseM3u` is declared but not defined.

- [ ] **Step 5: Implement `parseM3u` in `rom_scanner.cpp`**

Add this function in `cpp/src/core/rom_scanner.cpp`, after the `titleFromFilename` helper and before `RomScanner::scan`:

```cpp
QSet<QString> RomScanner::parseM3u(const QString& m3uPath) {
    QSet<QString> paths;
    QFile file(m3uPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[Scanner] Cannot open M3U file:" << m3uPath;
        return paths;
    }

    const QDir m3uDir = QFileInfo(m3uPath).absoluteDir();
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        // Resolve relative paths against the M3U file's directory
        QString resolved;
        if (QDir::isAbsolutePath(line))
            resolved = line;
        else
            resolved = m3uDir.filePath(line);

        QString canonical = QFileInfo(resolved).canonicalFilePath();
        if (!canonical.isEmpty())
            paths.insert(canonical);
    }

    return paths;
}
```

Also add `#include <QSet>` and `#include <QFile>` and `#include <QTextStream>` to the includes at the top of `rom_scanner.cpp` (QFile and QTextStream are already there — just add `#include <QSet>`). Also add `#include <QSet>` to `rom_scanner.h`.

- [ ] **Step 6: Build and run tests**

Run:
```bash
cd cpp && cmake --build build --target test_rom_scanner && ./build/test_rom_scanner
```

Expected: All 5 tests pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/core/rom_scanner.h cpp/src/core/rom_scanner.cpp cpp/tests/test_rom_scanner.cpp cpp/CMakeLists.txt
git commit -m "feat: add M3U file parser for multi-disc support"
```

---

### Task 2: Wire suppression into scan and update manifests

**Files:**
- Modify: `cpp/src/core/rom_scanner.cpp` (the `scan` method)
- Modify: `cpp/tests/test_rom_scanner.cpp` (add integration tests)
- Modify: `manifests/pcsx2.json`
- Modify: `manifests/duckstation.json`

- [ ] **Step 1: Add integration tests for M3U suppression**

Add these test slots to the `TestRomScanner` class in `cpp/tests/test_rom_scanner.cpp`, before the closing `};`:

```cpp
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

        // Only the M3U should be added
        QCOMPARE(result.added, 1);
        auto games = db.allGames();
        QCOMPARE(games.size(), 1);
        QCOMPARE(games[0].title, QString("FF7"));
    }

    void testScan_noM3uDiscsShowNormally() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QDir(tmp.path()).mkpath("roms/psx");
        QString romDir = tmp.filePath("roms/psx");

        // Discs with no M3U — should all appear
        writeFile(romDir + "/FF7 (Disc 1).bin");
        writeFile(romDir + "/FF7 (Disc 2).bin");

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
        QCOMPARE(result.added, 2);
    }
```

- [ ] **Step 2: Check if ManifestLoader supports test injection**

The tests above use `loader.injectManifest(manifest)`. We need to check if `ManifestLoader` has this method. If not, we need to add a simple test helper.

Read `cpp/src/core/manifest_loader.h` — if there is no `injectManifest` method, add one:

```cpp
/** Test helper — inject a manifest directly without loading from disk. */
void injectManifest(const EmulatorManifest& m) { m_manifests.append(m); }
```

This goes in the public section of `ManifestLoader` in `manifest_loader.h`.

- [ ] **Step 3: Build and verify new tests fail**

Run:
```bash
cd cpp && cmake --build build --target test_rom_scanner && ./build/test_rom_scanner
```

Expected: The 3 new integration tests fail because `scan` doesn't do M3U suppression yet. The 5 `parseM3u` tests still pass.

- [ ] **Step 4: Add suppression logic to `RomScanner::scan`**

In `cpp/src/core/rom_scanner.cpp`, modify the `scan` method. Add the suppression set construction **after** the extension map is built and **before** the main file iteration.

After the line `const QString biosDir = QDir(Paths::biosDir()).canonicalPath();` and before the line `// Scan directory recursively`, insert:

```cpp
    // Pass 1: collect files referenced by M3U playlists (suppression set)
    QSet<QString> suppressedPaths;
    {
        QDirIterator m3uIt(directory, QDir::Files, QDirIterator::Subdirectories);
        while (m3uIt.hasNext()) {
            m3uIt.next();
            if (m3uIt.fileInfo().suffix().toLower() == "m3u")
                suppressedPaths.unite(parseM3u(m3uIt.filePath()));
        }
    }
```

Then, in the existing file iteration loop, add the suppression check. After the line `if (!biosDir.isEmpty() && fi.canonicalPath().startsWith(biosDir)) continue;` and before `const QString ext = fi.suffix().toLower();`, insert:

```cpp
        // Skip files referenced by an M3U playlist
        if (suppressedPaths.contains(fi.canonicalFilePath()))
            continue;
```

- [ ] **Step 5: Build and run all tests**

Run:
```bash
cd cpp && cmake --build build --target test_rom_scanner && ./build/test_rom_scanner
```

Expected: All 8 tests pass (5 parseM3u + 3 integration).

- [ ] **Step 6: Update manifests**

In `manifests/pcsx2.json`, change `rom_extensions`:
```json
"rom_extensions": ["iso", "bin", "chd", "gz", "cso", "m3u"]
```

In `manifests/duckstation.json`, change `rom_extensions`:
```json
"rom_extensions": ["bin", "cue", "iso", "img", "pbp", "chd", "m3u"]
```

- [ ] **Step 7: Build the full app to verify no regressions**

Run:
```bash
cd cpp && cmake --build build
```

Expected: Clean build, no warnings related to the changes.

- [ ] **Step 8: Commit**

```bash
git add cpp/src/core/rom_scanner.cpp cpp/src/core/manifest_loader.h cpp/tests/test_rom_scanner.cpp manifests/pcsx2.json manifests/duckstation.json
git commit -m "feat: M3U multi-disc support — suppress disc files when playlist exists"
```
