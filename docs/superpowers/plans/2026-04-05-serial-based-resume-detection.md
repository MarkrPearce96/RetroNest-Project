# Serial-Based Resume State Detection — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `resume_states.json` marker system with serial-based detection so resume states survive app reinstalls and saves folder transfers.

**Architecture:** Extract game serial numbers from ROM disc images at import time (via ISO9660 parsing), store serials in the database, and scan savestates directories at launch to match resume files by serial. Each adapter implements its own serial extraction and resume file matching — the base class provides empty defaults for emulators that don't support resume.

**Tech Stack:** C++17, Qt6, CMake FetchContent, libchdr (MIT, for CHD file support)

---

### Task 1: Add libchdr Build Dependency

**Files:**
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Add FetchContent for libchdr**

Add after the existing `find_package` calls (after line 11):

```cmake
include(FetchContent)
FetchContent_Declare(
    libchdr
    GIT_REPOSITORY https://github.com/rtissera/libchdr.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libchdr)
```

- [ ] **Step 2: Link libchdr to the main target**

In the `target_link_libraries(${PROJECT_NAME} PRIVATE ...)` block (line 146), add `chdr-static` after the SDL2 line:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Multimedia
    ${SDL2_LIBRARIES}
    chdr-static
    "-framework AppKit"
    "-framework Carbon"
)
```

- [ ] **Step 3: Build to verify libchdr integrates**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```
Expected: Build succeeds (libchdr downloads and compiles).

- [ ] **Step 4: Commit**

```bash
git add cpp/CMakeLists.txt
git commit -m "build: add libchdr dependency for CHD file reading"
```

---

### Task 2: Create ISO9660 Reader

**Files:**
- Create: `cpp/src/core/iso9660_reader.h`
- Create: `cpp/src/core/iso9660_reader.cpp`
- Create: `cpp/tests/test_iso9660_reader.cpp`
- Modify: `cpp/CMakeLists.txt` (add to SOURCES/HEADERS + test target)

- [ ] **Step 1: Write the test file**

Create `cpp/tests/test_iso9660_reader.cpp`:

```cpp
#include <QtTest>
#include "core/iso9660_reader.h"

class TestIso9660Reader : public QObject {
    Q_OBJECT

private slots:
    void testParseSystemCnfPs2() {
        // PS2 format: BOOT2 = cdrom0:\SLUS_200.62;1
        QByteArray content = "BOOT2 = cdrom0:\\SLUS_200.62;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString("SLUS_200.62"));
    }

    void testParseSystemCnfPs1() {
        // PS1 format: BOOT = cdrom:\SCUS_941.83;1
        QByteArray content = "BOOT = cdrom:\\SCUS_941.83;1\r\nTCB = 4\r\nEVENT = 10\r\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString("SCUS_941.83"));
    }

    void testParseSystemCnfPs2NoBackslash() {
        // Some discs use forward slash
        QByteArray content = "BOOT2 = cdrom0:/SLPS_251.23;1\r\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString("SLPS_251.23"));
    }

    void testParseSystemCnfEmpty() {
        QByteArray content = "";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString());
    }

    void testParseSystemCnfGarbage() {
        QByteArray content = "NOT_A_BOOT_LINE = something\r\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString());
    }

    void testResolveCueFile() {
        // Create a temp CUE file pointing to a BIN
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Write a fake CUE sheet
        QString cuePath = dir.path() + "/game.cue";
        QFile cue(cuePath);
        QVERIFY(cue.open(QIODevice::WriteOnly));
        cue.write("FILE \"game.bin\" BINARY\r\n  TRACK 01 MODE2/2352\r\n    INDEX 01 00:00:00\r\n");
        cue.close();

        QCOMPARE(Iso9660::resolveToDataFile(cuePath), dir.path() + "/game.bin");
    }

    void testResolveM3uFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Write a fake M3U
        QString m3uPath = dir.path() + "/game.m3u";
        QFile m3u(m3uPath);
        QVERIFY(m3u.open(QIODevice::WriteOnly));
        m3u.write("game (Disc 1).iso\r\n");
        m3u.close();

        // Create the referenced file so resolution works
        QFile disc(dir.path() + "/game (Disc 1).iso");
        disc.open(QIODevice::WriteOnly);
        disc.close();

        QCOMPARE(Iso9660::resolveToDataFile(m3uPath), dir.path() + "/game (Disc 1).iso");
    }

    void testResolveIsoPassthrough() {
        QCOMPARE(Iso9660::resolveToDataFile("/some/game.iso"), QString("/some/game.iso"));
    }

    void testDetectSectorSizeCooked() {
        // First 12 bytes NOT matching sync pattern → 2048-byte cooked
        QByteArray data(2352, '\0');
        data[0] = 0x01;  // not 0x00, so no sync pattern
        QCOMPARE(Iso9660::detectSectorSize(data), 2048);
    }

    void testDetectSectorSizeRaw() {
        // First 12 bytes matching the CD-ROM sync pattern
        QByteArray data(2352, '\0');
        data[0] = 0x00;
        for (int i = 1; i <= 10; i++) data[i] = static_cast<char>(0xFF);
        data[11] = 0x00;
        QCOMPARE(Iso9660::detectSectorSize(data), 2352);
    }
};

QTEST_MAIN(TestIso9660Reader)
#include "test_iso9660_reader.moc"
```

- [ ] **Step 2: Write the header**

Create `cpp/src/core/iso9660_reader.h`:

```cpp
#pragma once

#include <QByteArray>
#include <QString>

/**
 * Iso9660 — minimal ISO9660 reader for extracting files from disc images.
 * Supports cooked (.iso, 2048-byte sectors), raw (.bin, 2352-byte sectors),
 * and compressed (.chd via libchdr).
 */
namespace Iso9660 {

    /**
     * Read a file from an ISO9660 disc image by name (e.g. "SYSTEM.CNF").
     * Auto-detects sector format. Returns file contents or empty on failure.
     */
    QByteArray readFile(const QString& imagePath, const QString& filename);

    /**
     * Parse SYSTEM.CNF contents and extract the game serial.
     * Handles both PS1 (BOOT=) and PS2 (BOOT2=) formats.
     * Returns the serial (e.g. "SLUS_200.62") or empty string.
     */
    QString parseSystemCnfSerial(const QByteArray& content);

    /**
     * Resolve .cue/.m3u files to the underlying data file path.
     * For .cue: parses the FILE line to find the BIN path.
     * For .m3u: reads the first non-comment entry.
     * For anything else: returns the input path unchanged.
     */
    QString resolveToDataFile(const QString& path);

    /**
     * Detect sector size from the first bytes of a disc image.
     * Returns 2352 if the CD-ROM sync pattern is found, 2048 otherwise.
     */
    int detectSectorSize(const QByteArray& firstBytes);

} // namespace Iso9660
```

- [ ] **Step 3: Write the implementation**

Create `cpp/src/core/iso9660_reader.cpp`:

```cpp
#include "iso9660_reader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <libchdr/chd.h>

#include <cstring>

// CD-ROM sync pattern: 00 FF FF FF FF FF FF FF FF FF FF 00
static const unsigned char SYNC_PATTERN[12] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

static constexpr int COOKED_SECTOR = 2048;
static constexpr int RAW_SECTOR = 2352;
static constexpr int RAW_HEADER = 16;  // sync(12) + MSF(3) + mode(1)
static constexpr int PVD_SECTOR = 16;  // Primary Volume Descriptor is at sector 16

// ── Sector reading abstraction ──────────────────────────────────────

/**
 * Abstract interface for reading sectors from a disc image.
 */
class SectorReader {
public:
    virtual ~SectorReader() = default;
    /// Read one sector of user data (2048 bytes) at the given sector index.
    virtual QByteArray readSector(int sectorIndex) = 0;
    /// Read multiple consecutive sectors of user data.
    QByteArray readSectors(int startSector, int count) {
        QByteArray result;
        for (int i = 0; i < count; i++) {
            result.append(readSector(startSector + i));
        }
        return result;
    }
};

/**
 * Reads from a raw file (.iso or .bin) with a given sector size.
 */
class FileSectorReader : public SectorReader {
public:
    FileSectorReader(QFile* file, int sectorSize)
        : m_file(file), m_sectorSize(sectorSize) {}

    QByteArray readSector(int sectorIndex) override {
        qint64 offset = static_cast<qint64>(sectorIndex) * m_sectorSize;
        if (m_sectorSize == RAW_SECTOR)
            offset += RAW_HEADER;  // skip sync + header to get user data
        m_file->seek(offset);
        return m_file->read(COOKED_SECTOR);
    }

private:
    QFile* m_file;
    int m_sectorSize;
};

/**
 * Reads from a CHD file via libchdr.
 */
class ChdSectorReader : public SectorReader {
public:
    ChdSectorReader() = default;
    ~ChdSectorReader() override {
        if (m_chd) chd_close(m_chd);
    }

    bool open(const QString& path) {
        chd_error err = chd_open(path.toUtf8().constData(), CHD_OPEN_READ, nullptr, &m_chd);
        if (err != CHDERR_NONE) {
            qWarning() << "[Iso9660] Failed to open CHD:" << path << "error:" << err;
            return false;
        }
        const chd_header* header = chd_get_header(m_chd);
        m_hunkSize = header->hunkbytes;
        m_unitSize = header->unitbytes;  // typically 2448 or 2352 for CD CHDs
        m_hunkBuffer.resize(m_hunkSize);
        return true;
    }

    QByteArray readSector(int sectorIndex) override {
        if (!m_chd) return {};

        // Each "unit" is one sector (raw). Calculate which hunk contains this sector.
        int unitsPerHunk = m_hunkSize / m_unitSize;
        int hunkIndex = sectorIndex / unitsPerHunk;
        int unitInHunk = sectorIndex % unitsPerHunk;

        if (hunkIndex != m_cachedHunk) {
            chd_error err = chd_read(m_chd, hunkIndex, m_hunkBuffer.data());
            if (err != CHDERR_NONE) return {};
            m_cachedHunk = hunkIndex;
        }

        // Extract user data from raw sector (skip 16-byte header)
        const char* sectorStart = m_hunkBuffer.constData() + (unitInHunk * m_unitSize);
        return QByteArray(sectorStart + RAW_HEADER, COOKED_SECTOR);
    }

private:
    chd_file* m_chd = nullptr;
    int m_hunkSize = 0;
    int m_unitSize = 0;
    int m_cachedHunk = -1;
    QByteArray m_hunkBuffer;
};

// ── ISO9660 filesystem parsing ──────────────────────────────────────

struct DirectoryEntry {
    QString name;
    uint32_t lba;
    uint32_t dataLength;
    bool isDirectory;
};

static uint32_t readLE32(const char* data) {
    const auto* p = reinterpret_cast<const unsigned char*>(data);
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static QVector<DirectoryEntry> parseDirectoryEntries(const QByteArray& data) {
    QVector<DirectoryEntry> entries;
    int offset = 0;

    while (offset < data.size()) {
        int recordLen = static_cast<unsigned char>(data[offset]);
        if (recordLen == 0) {
            // End of entries in this sector — skip to next sector boundary
            int nextSector = ((offset / COOKED_SECTOR) + 1) * COOKED_SECTOR;
            if (nextSector >= data.size()) break;
            offset = nextSector;
            continue;
        }
        if (offset + recordLen > data.size()) break;

        DirectoryEntry entry;
        entry.lba = readLE32(data.constData() + offset + 2);
        entry.dataLength = readLE32(data.constData() + offset + 10);
        entry.isDirectory = (data[offset + 25] & 0x02) != 0;

        int nameLen = static_cast<unsigned char>(data[offset + 32]);
        entry.name = QString::fromLatin1(data.constData() + offset + 33, nameLen);

        // Skip . and .. entries (single byte identifiers 0x00 and 0x01)
        if (nameLen == 1 && (data[offset + 33] == 0x00 || data[offset + 33] == 0x01)) {
            offset += recordLen;
            continue;
        }

        entries.append(entry);
        offset += recordLen;
    }

    return entries;
}

// ── Public API ──────────────────────────────────────────────────────

int Iso9660::detectSectorSize(const QByteArray& firstBytes) {
    if (firstBytes.size() >= 12 &&
        std::memcmp(firstBytes.constData(), SYNC_PATTERN, 12) == 0) {
        return RAW_SECTOR;
    }
    return COOKED_SECTOR;
}

QString Iso9660::resolveToDataFile(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "cue") {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};

        // Parse FILE "filename" BINARY line
        static const QRegularExpression fileRx(R"(FILE\s+"([^"]+)"\s+BINARY)",
                                                QRegularExpression::CaseInsensitiveOption);
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            auto match = fileRx.match(line);
            if (match.hasMatch()) {
                QString binName = match.captured(1);
                QDir cueDir = QFileInfo(path).absoluteDir();
                return cueDir.absoluteFilePath(binName);
            }
        }
        return {};
    }

    if (ext == "m3u") {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};

        QDir m3uDir = QFileInfo(path).absoluteDir();
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;

            // First non-comment entry — resolve and recurse
            QString resolved = QDir::isAbsolutePath(line)
                ? line : m3uDir.absoluteFilePath(line);

            // Recurse in case the M3U references a .cue
            return resolveToDataFile(resolved);
        }
        return {};
    }

    // .iso, .bin, .chd, .img — return as-is
    return path;
}

QString Iso9660::parseSystemCnfSerial(const QByteArray& content) {
    if (content.isEmpty()) return {};

    // Match: BOOT2 = cdrom0:\SLUS_200.62;1  or  BOOT = cdrom:\SCUS_941.83;1
    // Also handles forward slashes and optional colon after cdrom
    static const QRegularExpression rx(
        R"(BOOT2?\s*=\s*cdrom0?:?[/\\]?([^;\s]+);)",
        QRegularExpression::CaseInsensitiveOption);

    auto match = rx.match(QString::fromLatin1(content));
    if (!match.hasMatch()) return {};

    QString captured = match.captured(1);

    // The captured group might be the full path like "cdrom0:\SLUS_200.62"
    // Take just the filename part after the last slash/backslash
    int lastSlash = captured.lastIndexOf('/');
    int lastBackslash = captured.lastIndexOf('\\');
    int pos = qMax(lastSlash, lastBackslash);
    if (pos >= 0)
        captured = captured.mid(pos + 1);

    return captured.trimmed();
}

QByteArray Iso9660::readFile(const QString& imagePath, const QString& filename) {
    // Resolve .cue/.m3u to underlying data file
    const QString dataPath = resolveToDataFile(imagePath);
    if (dataPath.isEmpty()) {
        qWarning() << "[Iso9660] Could not resolve data file for:" << imagePath;
        return {};
    }

    const QString ext = QFileInfo(dataPath).suffix().toLower();

    // Create appropriate sector reader
    std::unique_ptr<SectorReader> reader;

    if (ext == "chd") {
        auto chdReader = std::make_unique<ChdSectorReader>();
        if (!chdReader->open(dataPath))
            return {};
        reader = std::move(chdReader);
    } else {
        auto file = std::make_unique<QFile>(dataPath);
        if (!file->open(QIODevice::ReadOnly)) {
            qWarning() << "[Iso9660] Cannot open:" << dataPath;
            return {};
        }

        // Detect sector size from first bytes
        QByteArray header = file->read(RAW_SECTOR);
        int sectorSize = detectSectorSize(header);

        reader = std::make_unique<FileSectorReader>(file.release(), sectorSize);
    }

    // Read Primary Volume Descriptor (sector 16)
    QByteArray pvd = reader->readSector(PVD_SECTOR);
    if (pvd.size() < COOKED_SECTOR) return {};

    // Verify PVD signature: byte 0 = 0x01, bytes 1-5 = "CD001"
    if (static_cast<unsigned char>(pvd[0]) != 0x01 ||
        pvd.mid(1, 5) != "CD001") {
        qWarning() << "[Iso9660] Invalid PVD signature in:" << dataPath;
        return {};
    }

    // Root directory record starts at offset 156, length 34
    // LBA of root directory extent is at offset 156+2 (LE32)
    // Data length of root directory is at offset 156+10 (LE32)
    uint32_t rootLba = readLE32(pvd.constData() + 156 + 2);
    uint32_t rootLen = readLE32(pvd.constData() + 156 + 10);

    // Read root directory
    int rootSectors = (rootLen + COOKED_SECTOR - 1) / COOKED_SECTOR;
    QByteArray rootDir = reader->readSectors(rootLba, rootSectors);

    // Parse directory entries
    auto entries = parseDirectoryEntries(rootDir);

    // Find the requested file (case-insensitive, handle ;1 version suffix)
    QString target = filename.toUpper();
    if (!target.contains(';'))
        target += ";1";

    for (const auto& entry : entries) {
        if (entry.name.toUpper() == target) {
            // Read file data
            int fileSectors = (entry.dataLength + COOKED_SECTOR - 1) / COOKED_SECTOR;
            QByteArray data = reader->readSectors(entry.lba, fileSectors);
            // Trim to actual file size
            return data.left(entry.dataLength);
        }
    }

    qWarning() << "[Iso9660] File not found in image:" << filename << "in" << dataPath;
    return {};
}
```

- [ ] **Step 4: Add to CMakeLists.txt**

Add `src/core/iso9660_reader.cpp` to SOURCES and `src/core/iso9660_reader.h` to HEADERS in `cpp/CMakeLists.txt`.

Add the test target after the existing test targets (after line 278):

```cmake
add_executable(test_iso9660_reader
    tests/test_iso9660_reader.cpp
    src/core/iso9660_reader.cpp
)
target_include_directories(test_iso9660_reader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_iso9660_reader PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME Iso9660Reader COMMAND test_iso9660_reader)
```

- [ ] **Step 5: Run tests**

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build --target test_iso9660_reader && ./build/test_iso9660_reader
```
Expected: All tests pass (the SYSTEM.CNF parsing, CUE/M3U resolution, and sector size detection tests).

- [ ] **Step 6: Build the main target to verify integration**

```bash
cd cpp && cmake --build build
```
Expected: Full build succeeds.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/core/iso9660_reader.h cpp/src/core/iso9660_reader.cpp cpp/tests/test_iso9660_reader.cpp cpp/CMakeLists.txt
git commit -m "feat: add ISO9660 reader for extracting files from disc images"
```

---

### Task 3: Add Database Serial Column (Schema v6)

**Files:**
- Modify: `cpp/src/core/database.h` — add `serial` field to `GameRecord`, add `updateSerial()` method
- Modify: `cpp/src/core/database.cpp` — migration v5→v6, update `addGame`, `recordFromQuery`, `GAME_SELECT_COLUMNS`

- [ ] **Step 1: Add serial field to GameRecord**

In `cpp/src/core/database.h`, add the serial field to `GameRecord` after the `disc_count` field (line 37):

```cpp
    int disc_count = 0;  // number of discs (0 = single-disc or non-M3U)
    QString serial;      // game serial (e.g. "SLUS_200.62"), extracted from ROM
```

Add the `updateSerial` method declaration to the `Database` class (after `updateGameMetadata` around line 64):

```cpp
    bool updateSerial(int id, const QString& serial);
    QString serialForRomPath(const QString& romPath);
```

- [ ] **Step 2: Update CURRENT_SCHEMA_VERSION**

In `cpp/src/core/database.cpp`, change the schema version constant (line 41):

```cpp
static const int CURRENT_SCHEMA_VERSION = 6;
```

- [ ] **Step 3: Add the serial column to CREATE TABLE**

In `cpp/src/core/database.cpp`, add after the `disc_count` line in the CREATE TABLE statement (line 83):

```cpp
        "  disc_count INTEGER NOT NULL DEFAULT 0,"
        "  serial TEXT NOT NULL DEFAULT ''"
```

- [ ] **Step 4: Add v5→v6 migration**

In `cpp/src/core/database.cpp`, add after the v3→v4 migration block (after line 208, before the `if (current < CURRENT_SCHEMA_VERSION)` line):

```cpp
    if (current < 6) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        QSqlQuery q(db);
        if (!q.exec("ALTER TABLE games ADD COLUMN serial TEXT NOT NULL DEFAULT ''")) {
            qCritical() << "[Database] Migration v5→v6 failed:" << q.lastError().text();
            return false;
        }
        qInfo() << "[Database] Migrated schema v5 → v6 (added serial column)";
    }
```

- [ ] **Step 5: Update addGame to include serial**

In `cpp/src/core/database.cpp`, update the `addGame()` INSERT statement (around line 234):

```cpp
    q.prepare("INSERT INTO games (title, rom_path, system, emulator_id, cover_path, disc_count, serial) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(game.title);
    q.addBindValue(romPath);
    q.addBindValue(game.system);
    q.addBindValue(game.emulator_id);
    q.addBindValue(game.cover_path.isNull() ? QString("") : game.cover_path);
    q.addBindValue(game.disc_count);
    q.addBindValue(game.serial.isNull() ? QString("") : game.serial);
```

- [ ] **Step 6: Update GAME_SELECT_COLUMNS and recordFromQuery**

In `cpp/src/core/database.cpp`, update `GAME_SELECT_COLUMNS` (line 250) to add `serial` at the end:

```cpp
static const char* GAME_SELECT_COLUMNS =
    "id, title, rom_path, system, emulator_id, cover_path, "
    "description, developer, publisher, release_date, genres, "
    "rating, players, last_played, play_count, favorite, "
    "screenshot_path, titlescreen_path, marquee_path, fanart_path, "
    "box3d_path, backcover_path, miximage_path, physicalmedia_path, "
    "manual_path, video_path, disc_count, serial";
```

In `recordFromQuery()` (line 258), add after the `disc_count` line (after line 286):

```cpp
    g.disc_count         = q.value(26).toInt();
    g.serial             = q.value(27).toString();
```

- [ ] **Step 7: Add updateSerial and serialForRomPath methods**

In `cpp/src/core/database.cpp`, add after `updateGameMetadata()` (after line 422):

```cpp
bool Database::updateSerial(int id, const QString& serial) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("UPDATE games SET serial = ? WHERE id = ?");
    q.addBindValue(serial.isNull() ? QString("") : serial);
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "[Database] updateSerial failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QString Database::serialForRomPath(const QString& romPath) {
    auto db = QSqlDatabase::database(DB_CONNECTION);
    QSqlQuery q(db);
    q.prepare("SELECT serial FROM games WHERE rom_path = ?");
    q.addBindValue(romPath);
    if (!q.exec() || !q.next()) return {};
    return q.value(0).toString();
}
```

- [ ] **Step 8: Build to verify**

```bash
cd cpp && cmake --build build
```
Expected: Build succeeds.

- [ ] **Step 9: Commit**

```bash
git add cpp/src/core/database.h cpp/src/core/database.cpp
git commit -m "feat: add serial column to games table (schema v6)"
```

---

### Task 4: Add Adapter Virtual Methods

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h`

- [ ] **Step 1: Add extractSerial and findResumeFile virtual methods**

In `cpp/src/adapters/emulator_adapter.h`, add after the `resumeLaunchArgs()` method (after line 210):

```cpp
    /**
     * Extract game serial/ID from a ROM file.
     * Returns the serial (e.g. "SLUS_200.62") or empty string if not supported.
     */
    virtual QString extractSerial(const QString& romPath) const { return {}; }

    /**
     * Find resume state file for a given serial in the saves directories.
     * Returns the full path to the resume file, or empty string if not found.
     */
    virtual QString findResumeFile(const QString& serial, const QString& savesRoot) const {
        Q_UNUSED(serial); Q_UNUSED(savesRoot);
        return {};
    }
```

- [ ] **Step 2: Build to verify**

```bash
cd cpp && cmake --build build
```
Expected: Build succeeds (virtual methods have default empty implementations).

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "feat: add extractSerial() and findResumeFile() to EmulatorAdapter"
```

---

### Task 5: Implement PCSX2Adapter Serial Extraction and Resume File Matching

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.h`
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp`

- [ ] **Step 1: Add method declarations to header**

In `cpp/src/adapters/pcsx2_adapter.h`, add after the existing `resumeLaunchArgs` override (or wherever the public overrides are grouped):

```cpp
    QString extractSerial(const QString& romPath) const override;
    QString findResumeFile(const QString& serial, const QString& savesRoot) const override;
```

- [ ] **Step 2: Add the include for iso9660_reader**

In `cpp/src/adapters/pcsx2_adapter.cpp`, add the include:

```cpp
#include "core/iso9660_reader.h"
```

- [ ] **Step 3: Implement extractSerial**

In `cpp/src/adapters/pcsx2_adapter.cpp`, add:

```cpp
QString PCSX2Adapter::extractSerial(const QString& romPath) const {
    QByteArray content = Iso9660::readFile(romPath, "SYSTEM.CNF");
    if (content.isEmpty()) {
        qWarning() << "[PCSX2Adapter] Failed to read SYSTEM.CNF from:" << romPath;
        return {};
    }
    QString serial = Iso9660::parseSystemCnfSerial(content);
    if (serial.isEmpty()) {
        qWarning() << "[PCSX2Adapter] No serial found in SYSTEM.CNF for:" << romPath;
    }
    return serial;
}
```

- [ ] **Step 4: Implement findResumeFile**

In `cpp/src/adapters/pcsx2_adapter.cpp`, add:

```cpp
QString PCSX2Adapter::findResumeFile(const QString& serial, const QString& savesRoot) const {
    if (serial.isEmpty()) return {};

    // PCSX2 resume files: {serial} ({crc}).resume.p2s in saves/ps2/savestates/
    const QString statesDir = savesRoot + "/ps2/savestates";
    QDir dir(statesDir);
    if (!dir.exists()) return {};

    // Match files starting with the serial and ending with .resume.p2s
    for (const auto& entry : dir.entryList(QDir::Files)) {
        if (entry.startsWith(serial) && entry.endsWith(".resume.p2s")) {
            return statesDir + "/" + entry;
        }
    }
    return {};
}
```

- [ ] **Step 5: Build to verify**

```bash
cd cpp && cmake --build build
```
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.h cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "feat: implement extractSerial() and findResumeFile() for PCSX2"
```

---

### Task 6: Implement DuckStationAdapter Serial Extraction and Resume File Matching

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.h`
- Modify: `cpp/src/adapters/duckstation_adapter.cpp`

- [ ] **Step 1: Add method declarations to header**

In `cpp/src/adapters/duckstation_adapter.h`, add with the other overrides:

```cpp
    QString extractSerial(const QString& romPath) const override;
    QString findResumeFile(const QString& serial, const QString& savesRoot) const override;
```

- [ ] **Step 2: Add the include for iso9660_reader**

In `cpp/src/adapters/duckstation_adapter.cpp`, add the include:

```cpp
#include "core/iso9660_reader.h"
```

- [ ] **Step 3: Implement extractSerial**

In `cpp/src/adapters/duckstation_adapter.cpp`, add:

```cpp
QString DuckStationAdapter::extractSerial(const QString& romPath) const {
    QByteArray content = Iso9660::readFile(romPath, "SYSTEM.CNF");
    if (content.isEmpty()) {
        qWarning() << "[DuckStationAdapter] Failed to read SYSTEM.CNF from:" << romPath;
        return {};
    }
    QString serial = Iso9660::parseSystemCnfSerial(content);
    if (serial.isEmpty()) {
        qWarning() << "[DuckStationAdapter] No serial found in SYSTEM.CNF for:" << romPath;
    }
    return serial;
}
```

- [ ] **Step 4: Implement findResumeFile**

In `cpp/src/adapters/duckstation_adapter.cpp`, add:

```cpp
QString DuckStationAdapter::findResumeFile(const QString& serial, const QString& savesRoot) const {
    if (serial.isEmpty()) return {};

    // DuckStation resume files: {serial}_resume.sav in saves/psx/savestates/
    const QString statesDir = savesRoot + "/psx/savestates";
    QDir dir(statesDir);
    if (!dir.exists()) return {};

    // Exact match for DuckStation's naming convention
    const QString expected = serial + "_resume.sav";
    if (dir.exists(expected)) {
        return statesDir + "/" + expected;
    }
    return {};
}
```

- [ ] **Step 5: Build to verify**

```bash
cd cpp && cmake --build build
```
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.h cpp/src/adapters/duckstation_adapter.cpp
git commit -m "feat: implement extractSerial() and findResumeFile() for DuckStation"
```

---

### Task 7: Extract Serials at Import Time

**Files:**
- Modify: `cpp/src/core/rom_scanner.h` — add AdapterRegistry include
- Modify: `cpp/src/core/rom_scanner.cpp` — extract serial after addGame

- [ ] **Step 1: Add serial extraction to the scan loop**

In `cpp/src/core/rom_scanner.cpp`, add include at the top:

```cpp
#include "adapters/adapter_registry.h"
```

In the `scan()` method, after the `db.addGame(game)` call and the success check (after line 226), add serial extraction:

```cpp
        int addResult = db.addGame(game);
        if (addResult > 0) {
            result.added++;
            qInfo() << "[Scanner] Added:" << game.title << "[" << game.system << "]";

            // Extract serial from ROM and store in DB
            auto* adapter = AdapterRegistry::instance().adapterFor(game.emulator_id);
            if (adapter) {
                QString serial = adapter->extractSerial(romPath);
                if (!serial.isEmpty()) {
                    db.updateSerial(addResult, serial);
                    qInfo() << "[Scanner] Serial:" << serial << "for" << game.title;
                }
            }
        } else if (addResult == 0) {
```

- [ ] **Step 2: Build to verify**

```bash
cd cpp && cmake --build build
```
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/core/rom_scanner.cpp
git commit -m "feat: extract game serial from ROM at import time"
```

---

### Task 8: Replace JSON-Based Resume Detection with Serial-Based

**Files:**
- Modify: `cpp/src/services/game_service.h`
- Modify: `cpp/src/services/game_service.cpp`
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`
- Modify: `cpp/src/ui/theme_context.h`
- Modify: `cpp/src/ui/theme_context.cpp`
- Modify: `cpp/qml/AppUI/AppWindow.qml`

- [ ] **Step 1: Update GameService — change method signatures**

In `cpp/src/services/game_service.h`, update the resume methods to take emuId:

Replace lines 43–50:
```cpp
    /** Check if a resume save state exists for this ROM (set after save-and-quit). */
    bool hasResumeState(const QString& romPath) const;

    /** Get the resume state file path for a ROM (empty if none). */
    QString resumeStateFile(const QString& romPath) const;

    /** Clear the resume state marker for a ROM and delete its specific resume file. */
    void clearResumeState(const QString& romPath);
```

With:
```cpp
    /** Check if a resume save state exists for this ROM via serial-based detection. */
    bool hasResumeState(const QString& romPath, const QString& emuId) const;

    /** Get the resume state file path for a ROM via serial-based detection. */
    QString resumeStateFile(const QString& romPath, const QString& emuId) const;

    /** Delete the resume state file for a ROM. */
    void clearResumeState(const QString& romPath, const QString& emuId);
```

Remove the `resumeMarkerPath()` static method declaration (line 74):
```cpp
    // DELETE this line:
    static QString resumeMarkerPath();
```

- [ ] **Step 2: Update GameService — rewrite implementation**

In `cpp/src/services/game_service.cpp`:

First, remove the `#include <QJsonDocument>` and `#include <QJsonObject>` includes (lines 9–10) as they are no longer needed.

Replace the entire `GameSession::finished` lambda (the `connect` block starting at line 20). Remove the resume_states.json write block inside the `if (!m_pendingSaveRomPath.isEmpty())` section. Replace it with just logging:

```cpp
    connect(&m_session, &GameSession::finished, this, [this](int exitCode, bool crashed) {
        m_terminateTimer.stop();

        if (!m_pendingSaveRomPath.isEmpty()) {
            if (!crashed) {
                qInfo() << "[GameService] Save-and-quit completed for" << m_pendingSaveRomPath;
            } else {
                qWarning() << "[GameService] Game crashed during save-and-quit";
            }
            m_pendingSaveRomPath.clear();
        }

        m_currentRomPath.clear();
        emit gameRunningChanged();
        emit gameFinished(exitCode, crashed);
        if (crashed)
            emit statusMessage("Emulator crashed");
        else
            emit statusMessage("Game exited (code " + QString::number(exitCode) + ")");
    });
```

Replace `resumeMarkerPath()`, `hasResumeState()`, `resumeStateFile()`, and `clearResumeState()` (lines 199–248) with:

```cpp
bool GameService::hasResumeState(const QString& romPath, const QString& emuId) const {
    return !resumeStateFile(romPath, emuId).isEmpty();
}

QString GameService::resumeStateFile(const QString& romPath, const QString& emuId) const {
    QString serial = m_db->serialForRomPath(romPath);
    if (serial.isEmpty()) return {};

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    const QString savesRoot = QFileInfo(Paths::savesDir("")).absoluteFilePath();
    return adapter->findResumeFile(serial, savesRoot);
}

void GameService::clearResumeState(const QString& romPath, const QString& emuId) {
    const QString stateFile = resumeStateFile(romPath, emuId);
    if (!stateFile.isEmpty() && QFile::exists(stateFile)) {
        QFile::remove(stateFile);
        qInfo() << "[GameService] Deleted resume state file:" << stateFile;
    }
}
```

- [ ] **Step 3: Update AppController — update method signatures**

In `cpp/src/ui/app_controller.h`, update (around lines 55–57):

```cpp
    Q_INVOKABLE bool hasResumeState(const QString& romPath, const QString& emuId);
    Q_INVOKABLE QString resumeStateFile(const QString& romPath, const QString& emuId);
    Q_INVOKABLE void clearResumeState(const QString& romPath, const QString& emuId);
```

In `cpp/src/ui/app_controller.cpp`, update the implementations (around lines 190–200):

```cpp
bool AppController::hasResumeState(const QString& romPath, const QString& emuId) {
    return m_gameService.hasResumeState(romPath, emuId);
}

QString AppController::resumeStateFile(const QString& romPath, const QString& emuId) {
    return m_gameService.resumeStateFile(romPath, emuId);
}

void AppController::clearResumeState(const QString& romPath, const QString& emuId) {
    m_gameService.clearResumeState(romPath, emuId);
}
```

- [ ] **Step 4: Update ThemeContext — pass emuId through**

In `cpp/src/ui/theme_context.h`, update (around lines 58–59):

```cpp
    Q_INVOKABLE bool hasResumeState(const QString& romPath, const QString& emuId);
    Q_INVOKABLE void clearResumeState(const QString& romPath, const QString& emuId);
```

In `cpp/src/ui/theme_context.cpp`, update `launchGame()` (around line 86–91):

```cpp
void ThemeContext::launchGame(int gameId, const QString& romPath, const QString& emuId) {
    // Check for a resume save state before launching
    if (m_app->hasResumeState(romPath, emuId)) {
        emit resumeStateFound(gameId, romPath, emuId);
        return;
    }
```

Update `launchGameResume()` (around line 103–118):

```cpp
void ThemeContext::launchGameResume(int gameId, const QString& romPath, const QString& emuId) {
    m_db->recordGameLaunch(gameId);

    const QString stateFile = m_app->resumeStateFile(romPath, emuId);
    if (!stateFile.isEmpty() && QFileInfo::exists(stateFile)) {
        qInfo() << "[ThemeContext] Resuming with state file:" << stateFile;
        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        QStringList resumeArgs = adapter ? adapter->resumeLaunchArgs(stateFile)
                                         : QStringList{"-statefile", stateFile};
        m_app->launchGame(gameId, romPath, emuId, resumeArgs);
    } else {
        qWarning() << "[ThemeContext] Resume state file not found, launching normally";
        m_app->launchGame(gameId, romPath, emuId);
    }
}
```

Update `hasResumeState()` and `clearResumeState()` (around lines 132–138):

```cpp
bool ThemeContext::hasResumeState(const QString& romPath, const QString& emuId) {
    return m_app->hasResumeState(romPath, emuId);
}

void ThemeContext::clearResumeState(const QString& romPath, const QString& emuId) {
    m_app->clearResumeState(romPath, emuId);
}
```

- [ ] **Step 5: Update QML — pass emuId to clearResumeState**

In `cpp/qml/AppUI/AppWindow.qml`, update the `onStartFreshChosen` handler (around line 153–158):

```qml
        onStartFreshChosen: {
            // Clear the resume file and launch fresh
            themeContext.clearResumeState(resumeStateDialog.pendingRomPath,
                                          resumeStateDialog.pendingEmuId);
            themeContext.launchGameDirect(resumeStateDialog.pendingGameId,
                                          resumeStateDialog.pendingRomPath,
                                          resumeStateDialog.pendingEmuId);
            resumeStateDialog.close();
        }
```

- [ ] **Step 6: Build to verify**

```bash
cd cpp && cmake --build build
```
Expected: Build succeeds with no errors.

- [ ] **Step 7: Run existing tests**

```bash
cd cpp && ctest --test-dir build --output-on-failure
```
Expected: All existing tests pass (IniFile, RomScanner, Iso9660Reader).

- [ ] **Step 8: Commit**

```bash
git add cpp/src/services/game_service.h cpp/src/services/game_service.cpp \
        cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp \
        cpp/src/ui/theme_context.h cpp/src/ui/theme_context.cpp \
        cpp/qml/AppUI/AppWindow.qml
git commit -m "feat: replace resume_states.json with serial-based resume detection"
```

---

### Task 9: Backfill Serials for Existing Games

**Files:**
- Modify: `cpp/src/services/game_service.h`
- Modify: `cpp/src/services/game_service.cpp`
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`
- Modify: `cpp/src/main.cpp`

This task ensures that when upgrading from the old schema, existing games get their serials extracted without requiring a full rescan.

- [ ] **Step 1: Add backfillSerials method**

In `cpp/src/services/game_service.h`, add after `scanRomFolders()`:

```cpp
    /** Backfill serial numbers for games that have none (e.g. after schema upgrade). */
    void backfillSerials();
```

- [ ] **Step 2: Implement backfillSerials**

In `cpp/src/services/game_service.cpp`, add:

```cpp
void GameService::backfillSerials() {
    auto games = m_db->allGames();
    int filled = 0;
    for (const auto& game : games) {
        if (!game.serial.isEmpty()) continue;

        auto* adapter = AdapterRegistry::instance().adapterFor(game.emulator_id);
        if (!adapter) continue;

        QString serial = adapter->extractSerial(game.rom_path);
        if (!serial.isEmpty()) {
            m_db->updateSerial(game.id, serial);
            filled++;
        }
    }
    if (filled > 0) {
        qInfo() << "[GameService] Backfilled serials for" << filled << "games";
    }
}
```

- [ ] **Step 3: Call backfillSerials on startup**

In `cpp/src/main.cpp`, add the backfill call right before the existing `scanRomFolders()` call at line 156:

```cpp
        // Backfill serials for games imported before schema v6
        appController.backfillSerials();

        // Auto-scan ROM folders on startup so games appear immediately
        appController.scanRomFolders();
```

Also add `backfillSerials()` as a public method in `AppController` that delegates to `m_gameService.backfillSerials()`:

In `cpp/src/ui/app_controller.h`, add:
```cpp
    void backfillSerials();
```

In `cpp/src/ui/app_controller.cpp`, add:
```cpp
void AppController::backfillSerials() {
    m_gameService.backfillSerials();
}
```

- [ ] **Step 4: Build and verify**

```bash
cd cpp && cmake --build build
```
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/game_service.h cpp/src/services/game_service.cpp \
        cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp cpp/src/main.cpp
git commit -m "feat: backfill serials for existing games on startup"
```

---

### Task 10: Clean Up Unused Code

**Files:**
- Modify: `cpp/src/services/game_service.cpp` — remove unused includes if any remain

- [ ] **Step 1: Verify no references to resume_states.json remain**

Search the codebase for any remaining references:

```bash
cd cpp && grep -r "resume_states" src/ qml/ --include="*.cpp" --include="*.h" --include="*.qml"
```
Expected: No matches found.

```bash
cd cpp && grep -r "resumeMarkerPath" src/ --include="*.cpp" --include="*.h"
```
Expected: No matches found.

- [ ] **Step 2: Verify m_pendingSaveRomPath is still used**

```bash
cd cpp && grep -r "m_pendingSaveRomPath" src/ --include="*.cpp" --include="*.h"
```
Expected: Still used in `saveAndStopGame()` and the finished handler for logging. If it's only used for logging, consider whether to keep it (it's fine for debugging).

- [ ] **Step 3: Final build and test**

```bash
cd cpp && cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: Full build succeeds, all tests pass.

- [ ] **Step 4: Commit (if any cleanup was done)**

```bash
git add -u cpp/src/
git commit -m "refactor: clean up unused resume_states.json references"
```

---

### Task 11: Manual Integration Test

- [ ] **Step 1: Launch the app and import some ROMs**

```bash
cd cpp && ./build/EmulatorFrontend
```

Import a few PS1/PS2 games. Check the console output for serial extraction logs:
```
[Scanner] Serial: SLUS_200.62 for Game Title
```

- [ ] **Step 2: Verify resume detection works normally**

1. Launch a game
2. Use the in-game menu to "Exit & Save State"
3. Launch the same game again — the "Resume?" dialog should appear
4. Choose "Start Fresh" — the resume file should be deleted

- [ ] **Step 3: Simulate reinstall scenario**

1. Exit the app
2. Delete `{config}/resume_states.json` if it still exists (it shouldn't)
3. Launch a game, save-and-quit to create a resume file
4. Delete the database file (`{config}/emulator-frontend.db`)
5. Relaunch the app — it should re-import games and extract serials
6. The resume file should now be detected for the correct game

- [ ] **Step 4: Test with CHD files (if available)**

If you have CHD ROM files, import them and verify the serial is extracted:
```
[Scanner] Serial: SCUS_941.83 for Game Title
```
