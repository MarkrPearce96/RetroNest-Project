# PPSSPP Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add full PPSSPP (PSP emulator) support following the existing adapter pattern.

**Architecture:** New `PPSSPPAdapter` class inheriting `EmulatorAdapter`, plus a standalone SFO binary parser for serial extraction from PSP ISOs. Config patching targets two files: `ppsspp.ini` (settings) and `controls.ini` (bindings). Controller bindings use PPSSPP's native `d:{index}/{button}` format.

**Tech Stack:** C++17, Qt6 (QFile, QDir, QRegularExpression, QTest)

---

### Task 1: SFO Parser — Test & Implementation

**Files:**
- Create: `cpp/src/core/sfo_parser.h`
- Create: `cpp/src/core/sfo_parser.cpp`
- Create: `cpp/tests/test_sfo_parser.cpp`
- Modify: `cpp/CMakeLists.txt:29-93` (SOURCES/HEADERS) and `271-305` (tests)

The SFO binary format (used in PSP's PARAM.SFO) has:
- Header: magic `0x00505346` ("PSF\0" LE), version, key_table_start, data_table_start, index_count
- Index table: array of entries (key_offset, data_format, data_len, data_max_len, data_offset)
- Key table: null-terminated strings
- Data table: values (UTF-8 strings or 32-bit ints)

- [ ] **Step 1: Write SFO parser header**

```cpp
// cpp/src/core/sfo_parser.h
#pragma once

#include <QByteArray>
#include <QString>

namespace SfoParser {
    /**
     * Parse a PARAM.SFO binary blob and extract a string value by key.
     * Returns empty string if parsing fails or key not found.
     */
    QString extractStringValue(const QByteArray& sfoData, const QString& key);

    /**
     * Convenience: extract DISC_ID from PARAM.SFO data.
     */
    inline QString extractDiscId(const QByteArray& sfoData) {
        return extractStringValue(sfoData, "DISC_ID");
    }
}
```

- [ ] **Step 2: Write failing tests**

```cpp
// cpp/tests/test_sfo_parser.cpp
#include <QtTest>
#include "core/sfo_parser.h"

class TestSfoParser : public QObject {
    Q_OBJECT

private:
    // Build a minimal valid SFO binary with one UTF-8 string entry.
    // SFO format: Header (20 bytes) + IndexTable (16 bytes per entry)
    //             + key table + data table
    static QByteArray buildSfo(const QByteArray& key, const QByteArray& value) {
        // Header: 20 bytes
        // magic(4) + version(4) + key_table_start(4) + data_table_start(4) + index_count(4)
        const quint32 magic = 0x00505346;     // "PSF\0" little-endian
        const quint32 version = 0x00000101;   // 1.1
        const quint32 indexCount = 1;
        const quint32 headerSize = 20;
        const quint32 indexSize = 16;         // one entry

        // Key table starts after header + index entries
        const quint32 keyTableStart = headerSize + (indexSize * indexCount);
        // Key is null-terminated
        const quint32 keyLen = key.size() + 1;
        // Align data table to 4 bytes
        const quint32 dataTableStart = keyTableStart + ((keyLen + 3) & ~3u);

        // Index entry: key_offset(2) + data_format(2) + data_len(4) + data_max_len(4) + data_offset(4)
        const quint16 keyOffset = 0;
        const quint16 dataFormat = 0x0204;    // UTF-8 string
        const quint32 dataLen = value.size() + 1;  // includes null terminator
        const quint32 dataMaxLen = (dataLen + 3) & ~3u;
        const quint32 dataOffset = 0;

        QByteArray sfo;
        QDataStream ds(&sfo, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::LittleEndian);

        // Header
        ds << magic << version << keyTableStart << dataTableStart << indexCount;

        // Index table entry
        ds << keyOffset << dataFormat << dataLen << dataMaxLen << dataOffset;

        // Key table (null-terminated, padded to 4-byte alignment)
        sfo.append(key);
        sfo.append('\0');
        while (static_cast<quint32>(sfo.size()) < dataTableStart)
            sfo.append('\0');

        // Data table (null-terminated string, padded)
        sfo.append(value);
        sfo.append('\0');
        while (sfo.size() % 4 != 0)
            sfo.append('\0');

        return sfo;
    }

private slots:
    void testExtractDiscId() {
        QByteArray sfo = buildSfo("DISC_ID", "ULES00151");
        QCOMPARE(SfoParser::extractDiscId(sfo), QString("ULES00151"));
    }

    void testExtractTitle() {
        QByteArray sfo = buildSfo("TITLE", "LocoRoco");
        QCOMPARE(SfoParser::extractStringValue(sfo, "TITLE"), QString("LocoRoco"));
    }

    void testKeyNotFound() {
        QByteArray sfo = buildSfo("DISC_ID", "ULES00151");
        QCOMPARE(SfoParser::extractStringValue(sfo, "MISSING_KEY"), QString());
    }

    void testEmptyData() {
        QCOMPARE(SfoParser::extractDiscId(QByteArray()), QString());
    }

    void testTruncatedHeader() {
        QByteArray truncated(10, '\0');  // too short for header
        QCOMPARE(SfoParser::extractDiscId(truncated), QString());
    }

    void testBadMagic() {
        QByteArray sfo = buildSfo("DISC_ID", "ULES00151");
        sfo[0] = 'X';  // corrupt magic
        QCOMPARE(SfoParser::extractDiscId(sfo), QString());
    }
};

QTEST_GUILESS_MAIN(TestSfoParser)
#include "test_sfo_parser.moc"
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build --target test_sfo_parser && ./build/test_sfo_parser`
Expected: Compilation fails — `sfo_parser.h` has no implementation yet.

- [ ] **Step 4: Write SFO parser implementation**

```cpp
// cpp/src/core/sfo_parser.cpp
#include "sfo_parser.h"

#include <QDataStream>
#include <QDebug>

namespace SfoParser {

struct SfoHeader {
    quint32 magic;
    quint32 version;
    quint32 keyTableStart;
    quint32 dataTableStart;
    quint32 indexCount;
};

struct SfoIndexEntry {
    quint16 keyOffset;
    quint16 dataFormat;
    quint32 dataLen;
    quint32 dataMaxLen;
    quint32 dataOffset;
};

static const quint32 SFO_MAGIC = 0x00505346;  // "PSF\0" little-endian
static const quint16 SFO_FORMAT_UTF8 = 0x0204;

QString extractStringValue(const QByteArray& sfoData, const QString& key) {
    if (sfoData.size() < 20)  // minimum header size
        return {};

    QDataStream ds(sfoData);
    ds.setByteOrder(QDataStream::LittleEndian);

    SfoHeader header;
    ds >> header.magic >> header.version >> header.keyTableStart
       >> header.dataTableStart >> header.indexCount;

    if (header.magic != SFO_MAGIC)
        return {};

    const auto totalSize = static_cast<quint32>(sfoData.size());
    if (header.keyTableStart > totalSize || header.dataTableStart > totalSize)
        return {};

    const QByteArray keyBytes = key.toUtf8();

    for (quint32 i = 0; i < header.indexCount; ++i) {
        SfoIndexEntry entry;
        ds >> entry.keyOffset >> entry.dataFormat >> entry.dataLen
           >> entry.dataMaxLen >> entry.dataOffset;

        if (ds.status() != QDataStream::Ok)
            return {};

        // Read key from key table
        const quint32 keyPos = header.keyTableStart + entry.keyOffset;
        if (keyPos >= totalSize)
            continue;

        // Extract null-terminated key string
        const char* keyStart = sfoData.constData() + keyPos;
        const int maxKeyLen = totalSize - keyPos;
        QByteArray entryKey(keyStart, qstrnlen(keyStart, maxKeyLen));

        if (entryKey != keyBytes)
            continue;

        // Found matching key — extract UTF-8 string value
        if (entry.dataFormat != SFO_FORMAT_UTF8)
            return {};

        const quint32 dataPos = header.dataTableStart + entry.dataOffset;
        if (dataPos >= totalSize || dataPos + entry.dataLen > totalSize)
            return {};

        // dataLen includes null terminator
        return QString::fromUtf8(sfoData.constData() + dataPos, entry.dataLen - 1);
    }

    return {};
}

}  // namespace SfoParser
```

- [ ] **Step 5: Add SFO parser to CMakeLists.txt**

In `cpp/CMakeLists.txt`:
- Add `src/core/sfo_parser.cpp` to `SOURCES` (after line 38, near other core files)
- Add `src/core/sfo_parser.h` to `HEADERS` (after line 88, near other core files)
- Add test executable at the end of the tests section (after line 305):

```cmake
add_executable(test_sfo_parser
    tests/test_sfo_parser.cpp
    src/core/sfo_parser.cpp
)
target_include_directories(test_sfo_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_sfo_parser PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SfoParser COMMAND test_sfo_parser)
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build --target test_sfo_parser && ./build/test_sfo_parser`
Expected: All 6 tests pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/core/sfo_parser.h cpp/src/core/sfo_parser.cpp cpp/tests/test_sfo_parser.cpp cpp/CMakeLists.txt
git commit -m "feat: add SFO binary parser for PSP PARAM.SFO serial extraction"
```

---

### Task 2: Manifest & System Mappings

**Files:**
- Create: `manifests/ppsspp.json`
- Modify: `cpp/src/core/ra_client.cpp:295-301` (add PSP console ID)

Note: `scraper.cpp` already has `{"psp", 61}` and `theme_context.cpp` already has `{"psp", "PSP"}`.

- [ ] **Step 1: Create manifest**

```json
{
  "id": "ppsspp",
  "name": "PPSSPP",
  "description": "PPSSPP is a PSP emulator. Play PSP games in HD with save states, controller support, and custom configurations.",
  "systems": ["psp"],
  "github_repo": "hrydgard/ppsspp",
  "executable": "PPSSPPQt",
  "install_folder": "ppsspp",
  "rom_extensions": ["iso", "cso", "chd", "pbp", "elf", "prx"],
  "launch_args": ["--fullscreen", "--escape-exit", "{rom_path}"]
}
```

- [ ] **Step 2: Add PSP console ID to RA mapping**

In `cpp/src/core/ra_client.cpp`, add `{"psp", 41}` to the `consoleIdMapping()`:

```cpp
static const QMap<QString, int>& consoleIdMapping() {
    static const QMap<QString, int> mapping = {
        {"psx", 12},
        {"ps2", 21},
        {"psp", 41},
    };
    return mapping;
}
```

- [ ] **Step 3: Build to verify no compilation errors**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`
Expected: Compiles successfully.

- [ ] **Step 4: Commit**

```bash
git add manifests/ppsspp.json cpp/src/core/ra_client.cpp
git commit -m "feat: add PPSSPP manifest and PSP RetroAchievements console mapping"
```

---

### Task 3: PPSSPP Adapter — Header & Registration

**Files:**
- Create: `cpp/src/adapters/ppsspp_adapter.h`
- Modify: `cpp/src/adapters/adapter_registry.cpp:1-15`
- Modify: `cpp/CMakeLists.txt:29-93` (SOURCES/HEADERS)

- [ ] **Step 1: Create adapter header**

```cpp
// cpp/src/adapters/ppsspp_adapter.h
#pragma once

#include "emulator_adapter.h"

/**
 * PPSSPPAdapter — handles PPSSPP-specific config patching and executable resolution.
 *
 * Config lives in two files:
 * - ppsspp.ini — main settings (graphics, audio, system, paths)
 * - controls.ini — controller bindings (ControlMapping section)
 *
 * Binding format: d:{deviceIndex}/{button} (e.g., d:0/BUTTON_A, d:0/AXIS_X+)
 */
class PPSSPPAdapter : public EmulatorAdapter {
public:
    bool ensureConfig(const EmulatorManifest& manifest,
                      const QString& biosPath,
                      const QString& savesPath) override;

    QString resolveExecutable(const EmulatorManifest& manifest,
                              const QString& installPath) override;

    QVector<SettingDef> settingsSchema() const override;
    QString configFilePath() const override;
    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
    ResolutionOptions resolutionOptions() const override;
    AspectRatioOptions aspectRatioOptions() const override;
    QVector<BindingDef> controllerBindingDefs() const override;
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<SettingDef> controllerSettingDefs() const override;
    bool supportsRetroAchievements() const override { return true; }
    void patchRetroAchievements(const QString& username, const QString& token,
                                 bool enabled, bool hardcore,
                                 bool notifications, bool sounds) override;
    QString matchAsset(const QStringList& assetNames) const override;
    QString extractSerial(const QString& romPath) const override;
    QString findResumeFile(const QString& serial, const QString& savesRoot) const override;
    QStringList resumeLaunchArgs(const QString& stateFilePath) const override;
    QString formatBinding(int deviceIndex, const QString& element,
                           bool isAxis, bool positive) const override;

private:
    static QString configDir();
    static QString iniPath();
    static QString controlsIniPath();

    bool createDefaultConfig(const QString& path,
                             const QString& biosPath,
                             const QString& savesPath);
    bool patchExistingConfig(const QString& path,
                             const QString& biosPath,
                             const QString& savesPath);
    bool createDefaultControlsConfig(const QString& path);
    bool patchExistingControlsConfig(const QString& path);
};
```

- [ ] **Step 2: Register adapter**

In `cpp/src/adapters/adapter_registry.cpp`, add include and registration:

```cpp
#include "adapter_registry.h"
#include "pcsx2_adapter.h"
#include "duckstation_adapter.h"
#include "ppsspp_adapter.h"

// ...

void AdapterRegistry::registerBuiltinAdapters() {
    registerAdapter("pcsx2", std::make_unique<PCSX2Adapter>());
    registerAdapter("duckstation", std::make_unique<DuckStationAdapter>());
    registerAdapter("ppsspp", std::make_unique<PPSSPPAdapter>());
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `cpp/CMakeLists.txt`:
- Add `src/adapters/ppsspp_adapter.cpp` to SOURCES (after line 38)
- Add `src/adapters/ppsspp_adapter.h` to HEADERS (after line 88)
- Add `src/adapters/ppsspp_adapter.cpp` to `test_rom_scanner` SOURCES (after line 293)

- [ ] **Step 4: Create stub implementation for compilation**

Create `cpp/src/adapters/ppsspp_adapter.cpp` with empty method stubs so the build succeeds:

```cpp
// cpp/src/adapters/ppsspp_adapter.cpp
#include "ppsspp_adapter.h"
#include "core/sfo_parser.h"
#include "core/iso9660_reader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>

static const char* PPSSPP_INSTALL_FOLDER = "ppsspp";

QString PPSSPPAdapter::configDir() {
    return Paths::emulatorsDir(PPSSPP_INSTALL_FOLDER);
}

QString PPSSPPAdapter::iniPath() {
    return configDir() + "/ppsspp.ini";
}

QString PPSSPPAdapter::controlsIniPath() {
    return configDir() + "/controls.ini";
}

QString PPSSPPAdapter::configFilePath() const {
    return iniPath();
}

bool PPSSPPAdapter::ensureConfig(const EmulatorManifest&, const QString&, const QString&) { return true; }
QString PPSSPPAdapter::resolveExecutable(const EmulatorManifest& manifest, const QString& installPath) {
    return resolveExecutableInDir(manifest, installPath, "PPSSPPQt");
}
QVector<SettingDef> PPSSPPAdapter::settingsSchema() const { return {}; }
QVector<BiosDef> PPSSPPAdapter::biosFiles() const { return {}; }
QVector<PathDef> PPSSPPAdapter::pathsDefs() const { return {}; }
ResolutionOptions PPSSPPAdapter::resolutionOptions() const { return {}; }
AspectRatioOptions PPSSPPAdapter::aspectRatioOptions() const { return {}; }
QVector<BindingDef> PPSSPPAdapter::controllerBindingDefs() const { return {}; }
QVector<HotkeyDef> PPSSPPAdapter::hotkeyBindingDefs() const { return {}; }
QVector<ControllerTypeDef> PPSSPPAdapter::controllerTypes() const { return {}; }
QVector<SettingDef> PPSSPPAdapter::controllerSettingDefs() const { return {}; }
void PPSSPPAdapter::patchRetroAchievements(const QString&, const QString&, bool, bool, bool, bool) {}
QString PPSSPPAdapter::matchAsset(const QStringList&) const { return {}; }
QString PPSSPPAdapter::extractSerial(const QString&) const { return {}; }
QString PPSSPPAdapter::findResumeFile(const QString&, const QString&) const { return {}; }
QStringList PPSSPPAdapter::resumeLaunchArgs(const QString&) const { return {}; }
QString PPSSPPAdapter::formatBinding(int, const QString&, bool, bool) const { return {}; }
bool PPSSPPAdapter::createDefaultConfig(const QString&, const QString&, const QString&) { return true; }
bool PPSSPPAdapter::patchExistingConfig(const QString&, const QString&, const QString&) { return true; }
bool PPSSPPAdapter::createDefaultControlsConfig(const QString&) { return true; }
bool PPSSPPAdapter::patchExistingControlsConfig(const QString&) { return true; }
```

- [ ] **Step 5: Build and run existing tests**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: Build succeeds, all existing tests pass.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/ppsspp_adapter.h cpp/src/adapters/ppsspp_adapter.cpp cpp/src/adapters/adapter_registry.cpp cpp/CMakeLists.txt
git commit -m "feat: add PPSSPP adapter skeleton with registration"
```

---

### Task 4: Config Patching — ensureConfig, createDefaultConfig, patchExistingConfig

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp`

- [ ] **Step 1: Implement ensureConfig**

Replace the stub `ensureConfig` in `ppsspp_adapter.cpp`:

```cpp
bool PPSSPPAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                 const QString& biosPath,
                                 const QString& savesPath) {
    const QString dir = configDir();
    if (!QDir().mkpath(dir)) {
        qWarning() << "[PPSSPP] Failed to create config directory:" << dir;
        return false;
    }

    // Main config (ppsspp.ini)
    const QString mainPath = iniPath();
    if (QFileInfo::exists(mainPath)) {
        if (!patchExistingConfig(mainPath, biosPath, savesPath))
            return false;
    } else {
        if (!createDefaultConfig(mainPath, biosPath, savesPath))
            return false;
    }

    // Controls config (controls.ini)
    const QString ctrlPath = controlsIniPath();
    if (QFileInfo::exists(ctrlPath)) {
        if (!patchExistingControlsConfig(ctrlPath))
            return false;
    } else {
        if (!createDefaultControlsConfig(ctrlPath))
            return false;
    }

    return true;
}
```

- [ ] **Step 2: Implement createDefaultConfig**

Replace the stub `createDefaultConfig`:

```cpp
bool PPSSPPAdapter::createDefaultConfig(const QString& path,
                                        const QString& biosPath,
                                        const QString& savesPath) {
    const QString emuDir = QFileInfo(Paths::dataDir("ppsspp")).absoluteFilePath();
    const QString savestatesPath = savesPath + "/savestates";
    const QString memcardsPath = savesPath + "/memcards";

    QStringList lines = {
        "[General]",
        "FirstRun = False",
        "AutoLoadSaveState = 0",
        "EnableStateUndo = True",
        "FlashFirmwarePath = " + biosPath + "/",
        "SaveStatePath = " + savestatesPath + "/",
        "MemStickSavePath = " + memcardsPath + "/",
        "ScreenshotPath = " + emuDir + "/screenshots/",
        "CheatsPath = " + emuDir + "/cheats/",
        "TextureReplacementPath = " + emuDir + "/textures/",
        "",
        "[Graphics]",
        "FullScreen = True",
        "InternalResolution = 2",
        "",
        "[Sound]",
        "Enable = True",
        "",
    };

    return writeConfigFile(path, lines.join("\n"), "PPSSPP");
}
```

- [ ] **Step 3: Implement patchExistingConfig**

Replace the stub `patchExistingConfig`:

```cpp
bool PPSSPPAdapter::patchExistingConfig(const QString& path,
                                        const QString& biosPath,
                                        const QString& savesPath) {
    QString content;
    if (!readConfigFile(path, content, "PPSSPP"))
        return false;

    bool changed = false;

    const QString emuDir = QFileInfo(Paths::dataDir("ppsspp")).absoluteFilePath();

    // Suppress first-run behavior
    QVector<IniKeyPatch> generalPatches = {
        {"General", "FirstRun", "False"},
        {"General", "AutoLoadSaveState", "0"},
        {"General", "EnableStateUndo", "True"},
    };
    if (patchIniKeys(content, generalPatches))
        changed = true;

    // Force fullscreen
    QVector<IniKeyPatch> gfxPatches = {
        {"Graphics", "FullScreen", "True"},
    };
    if (patchIniKeys(content, gfxPatches))
        changed = true;

    // Ensure folder paths
    QVector<IniKeyPatch> folderPatches = {
        {"General", "FlashFirmwarePath", biosPath + "/"},
        {"General", "SaveStatePath", savesPath + "/savestates/"},
        {"General", "MemStickSavePath", savesPath + "/memcards/"},
        {"General", "ScreenshotPath", emuDir + "/screenshots/"},
        {"General", "CheatsPath", emuDir + "/cheats/"},
        {"General", "TextureReplacementPath", emuDir + "/textures/"},
    };
    if (patchIniKeys(content, folderPatches))
        changed = true;

    if (changed) {
        if (!writeConfigFile(path, content, "PPSSPP"))
            return false;
    }
    return true;
}
```

- [ ] **Step 4: Implement createDefaultControlsConfig**

Replace the stub `createDefaultControlsConfig`:

```cpp
bool PPSSPPAdapter::createDefaultControlsConfig(const QString& path) {
    QStringList lines = {
        "[ControlMapping]",
        "Up = d:0/DPAD_UP",
        "Down = d:0/DPAD_DOWN",
        "Left = d:0/DPAD_LEFT",
        "Right = d:0/DPAD_RIGHT",
        "Cross = d:0/BUTTON_A",
        "Circle = d:0/BUTTON_B",
        "Square = d:0/BUTTON_X",
        "Triangle = d:0/BUTTON_Y",
        "Start = d:0/BUTTON_START",
        "Select = d:0/BUTTON_BACK",
        "L = d:0/LEFT_SHOULDER",
        "R = d:0/RIGHT_SHOULDER",
        "An.Up = d:0/AXIS_Y-",
        "An.Down = d:0/AXIS_Y+",
        "An.Left = d:0/AXIS_X-",
        "An.Right = d:0/AXIS_X+",
        "Fast-forward = d:0/RIGHT_TRIGGER",
        "Save State =",
        "Load State =",
        "Pause =",
        "",
    };

    return writeConfigFile(path, lines.join("\n"), "PPSSPP");
}
```

- [ ] **Step 5: Implement patchExistingControlsConfig**

Replace the stub `patchExistingControlsConfig`:

```cpp
bool PPSSPPAdapter::patchExistingControlsConfig(const QString& path) {
    QString content;
    if (!readConfigFile(path, content, "PPSSPP"))
        return false;

    bool changed = false;

    // Clear conflicting hotkeys to prevent interference with our overlay
    QVector<IniKeyPatch> patches = {
        {"ControlMapping", "Pause", ""},
        {"ControlMapping", "Save State", ""},
        {"ControlMapping", "Load State", ""},
    };
    if (patchIniKeys(content, patches))
        changed = true;

    if (changed) {
        if (!writeConfigFile(path, content, "PPSSPP"))
            return false;
    }
    return true;
}
```

- [ ] **Step 6: Build and run tests**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: Build succeeds, all tests pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "feat: implement PPSSPP config patching (ppsspp.ini + controls.ini)"
```

---

### Task 5: Paths, BIOS, Resolution, Aspect Ratio

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp`

- [ ] **Step 1: Implement pathsDefs**

Replace the stub `pathsDefs`:

```cpp
QVector<PathDef> PPSSPPAdapter::pathsDefs() const {
    return {
        {"BIOS",                "General", "FlashFirmwarePath",       "",            PathBase::Bios},
        {"Save States",         "General", "SaveStatePath",           "savestates",  PathBase::Saves},
        {"Memory Stick Saves",  "General", "MemStickSavePath",        "memcards",    PathBase::Saves},
        {"Screenshots",         "General", "ScreenshotPath",          "screenshots", PathBase::Data},
        {"Cheats",              "General", "CheatsPath",              "cheats",      PathBase::Data},
        {"Textures",            "General", "TextureReplacementPath",  "textures",    PathBase::Data},
    };
}
```

- [ ] **Step 2: Implement biosFiles**

Replace the stub `biosFiles`:

```cpp
QVector<BiosDef> PPSSPPAdapter::biosFiles() const {
    // PSP emulation is fully HLE — no required BIOS files
    return {
        {"ppge_atlas.zim", "PSP UI font atlas (optional)", false, ""},
    };
}
```

- [ ] **Step 3: Implement resolutionOptions**

Replace the stub `resolutionOptions`:

```cpp
ResolutionOptions PPSSPPAdapter::resolutionOptions() const {
    return {"Graphics", "InternalResolution",
            {{"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"}, {"3x (1440x816)", "3"},
             {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"}, {"10x (4800x2720)", "10"}},
            "2"};
}
```

- [ ] **Step 4: Implement aspectRatioOptions**

Replace the stub `aspectRatioOptions`:

```cpp
AspectRatioOptions PPSSPPAdapter::aspectRatioOptions() const {
    return {{
        {"Stretch to Display", {
            {"Graphics", "DisplayAspectRatio", "1.000000"},
        }},
        {"16:9 Widescreen", {
            {"Graphics", "DisplayAspectRatio", "1.777778"},
        }},
        {"PSP Native", {
            {"Graphics", "DisplayAspectRatio", "1.764706"},
        }},
    }, "16:9 Widescreen"};
}
```

- [ ] **Step 5: Build and run tests**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: Build succeeds, all tests pass.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "feat: add PPSSPP paths, BIOS, resolution, and aspect ratio definitions"
```

---

### Task 6: Settings Schema

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp`

- [ ] **Step 1: Implement settingsSchema**

Replace the stub `settingsSchema`:

```cpp
QVector<SettingDef> PPSSPPAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    // ═══════════════════════════════════════════════════════════════════════
    // Emulation
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Emulation", "", "Performance", "Graphics", "FrameSkip", "Frame Skip",
              "Number of frames to skip to maintain speed.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"1", "1"}, {"2", "2"}, {"3", "3"}}, 0, 0, 0});
    s.append({"Emulation", "", "Performance", "Graphics", "FrameSkipType", "Frame Skip Type",
              "How frame skip is applied.",
              SettingDef::Combo, "0",
              {{"Number of Frames", "0"}, {"Percent of FPS", "1"}}, 0, 0, 0});
    s.append({"Emulation", "", "Performance", "Graphics", "AutoFrameSkip", "Auto Frameskip",
              "Automatically skip frames to maintain speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Emulation", "", "Performance", "CPU", "FastMemory", "Fast Memory (Unstable)",
              "Uses faster but less accurate memory access. May cause crashes in some games.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Emulation", "", "Performance", "CPU", "IOTimingMethod", "I/O Timing Method",
              "Controls how UMD I/O timing is handled.",
              SettingDef::Combo, "0",
              {{"Fast", "0"}, {"Host", "1"}, {"Simulate UMD Delays", "2"}}, 0, 0, 0});
    s.append({"Emulation", "", "Performance", "CPU", "CPUThread", "Multithreaded (Unstable)",
              "Run CPU and GPU on separate threads. Can improve speed but may cause issues.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "", "Rendering", "Graphics", "GraphicsBackend", "Rendering Backend",
              "Graphics API used for rendering.",
              SettingDef::Combo, "3",
              {{"OpenGL", "0"}, {"Vulkan", "3"},
#if defined(Q_OS_MACOS)
               {"Metal", "4"},
#endif
               {"Software", "1"}}, 0, 0, 0});
    s.append({"Graphics", "", "Rendering", "Graphics", "InternalResolution", "Internal Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "2",
              {{"1x (480x272)", "1"}, {"2x (960x544)", "2"}, {"3x (1440x816)", "3"},
               {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"}, {"6x (2880x1632)", "6"},
               {"7x (3360x1904)", "7"}, {"8x (3840x2176)", "8"}, {"9x (4320x2448)", "9"},
               {"10x (4800x2720)", "10"}}, 0, 0, 0});
    s.append({"Graphics", "", "Rendering", "Graphics", "TextureFiltering", "Texture Filtering",
              "Filtering applied to textures.",
              SettingDef::Combo, "1",
              {{"Auto", "1"}, {"Nearest", "2"}, {"Linear", "3"}, {"Auto Max Quality", "4"}}, 0, 0, 0});
    s.append({"Graphics", "", "Rendering", "Graphics", "AnisotropyLevel", "Anisotropic Filtering",
              "Improves texture quality at oblique angles.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"}, {"16x", "4"}}, 0, 0, 0});
    s.append({"Graphics", "", "Rendering", "Graphics", "VSyncInterval", "VSync",
              "Synchronize rendering to display refresh rate.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "", "Texture Enhancement", "Graphics", "TexScalingLevel", "Texture Scaling Level",
              "Upscale texture resolution.",
              SettingDef::Combo, "1",
              {{"Off", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x", "4"}, {"5x", "5"}}, 0, 0, 0});
    s.append({"Graphics", "", "Texture Enhancement", "Graphics", "TexScalingType", "Texture Scaling Type",
              "Algorithm used for texture upscaling.",
              SettingDef::Combo, "0",
              {{"xBRZ", "0"}, {"Hybrid", "1"}, {"Bicubic", "2"}, {"Hybrid+Bicubic", "3"}}, 0, 0, 0});
    s.append({"Graphics", "", "Rendering", "Graphics", "HardwareTransform", "Hardware Transform",
              "Uses hardware geometry transformation. Disable only for debugging.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "", "Rendering", "Graphics", "SoftwareSkinning", "Software Skinning",
              "Performs vertex skinning on the CPU for better accuracy.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "", "Texture Enhancement", "Graphics", "TexDeposterize", "Deposterize",
              "Smooths color banding in textures.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Audio
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Audio", "", "", "Sound", "GlobalVolume", "Global Volume",
              "Master audio volume.", SettingDef::Int, "6", {}, 0, 10, 1, "slider", ""});
    s.append({"Audio", "", "", "Sound", "AltSpeedVolume", "Alt Speed Volume",
              "Volume when using fast-forward or slow motion. -1 keeps global volume.",
              SettingDef::Int, "-1", {}, -1, 10, 1, "slider", ""});
    s.append({"Audio", "", "", "Sound", "AudioBackend", "Audio Backend",
              "Audio output method.",
              SettingDef::Combo, "0",
              {{"Auto", "0"}, {"SDL", "1"}}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // System
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"System", "", "", "SystemParam", "Language", "PSP Language",
              "System language reported to games.",
              SettingDef::Combo, "1",
              {{"Japanese", "0"}, {"English", "1"}, {"French", "2"}, {"Spanish", "3"},
               {"German", "4"}, {"Italian", "5"}, {"Dutch", "6"}, {"Portuguese", "7"},
               {"Russian", "8"}, {"Korean", "9"}, {"Chinese (Traditional)", "10"},
               {"Chinese (Simplified)", "11"}}, 0, 0, 0});
    s.append({"System", "", "", "SystemParam", "NickName", "PSP Nickname",
              "Nickname shown in supported games.",
              SettingDef::String, "PPSSPP", {}, 0, 0, 0});
    s.append({"System", "", "", "SystemParam", "ButtonPreference", "Confirm Button",
              "Which button acts as confirm in the PSP system UI.",
              SettingDef::Combo, "0",
              {{"Cross", "0"}, {"Circle", "1"}}, 0, 0, 0});

    return s;
}
```

- [ ] **Step 2: Build and run tests**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: Build succeeds, all tests pass.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "feat: add PPSSPP settings schema (~25 settings)"
```

---

### Task 7: Controller Bindings, Hotkeys, formatBinding

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp`

- [ ] **Step 1: Implement controllerTypes**

Replace the stub `controllerTypes`:

```cpp
QVector<ControllerTypeDef> PPSSPPAdapter::controllerTypes() const {
    return {
        {"NotConnected", "Not Connected", ""},
        {"Standard",     "PSP Controller", ""},
    };
}
```

- [ ] **Step 2: Implement controllerBindingDefs**

Replace the stub `controllerBindingDefs`. Note: PPSSPP bindings use `ControlMapping` section (in `controls.ini`), not `Pad1`.

```cpp
QVector<BindingDef> PPSSPPAdapter::controllerBindingDefs() const {
    return {
        // D-Pad
        {BindingDef::Button, "Up",       "D-Pad",        "ControlMapping", "Up",       "d:0/DPAD_UP"},
        {BindingDef::Button, "Down",     "D-Pad",        "ControlMapping", "Down",     "d:0/DPAD_DOWN"},
        {BindingDef::Button, "Left",     "D-Pad",        "ControlMapping", "Left",     "d:0/DPAD_LEFT"},
        {BindingDef::Button, "Right",    "D-Pad",        "ControlMapping", "Right",    "d:0/DPAD_RIGHT"},
        // Face Buttons
        {BindingDef::Button, "Cross",    "Face Buttons",  "ControlMapping", "Cross",    "d:0/BUTTON_A"},
        {BindingDef::Button, "Circle",   "Face Buttons",  "ControlMapping", "Circle",   "d:0/BUTTON_B"},
        {BindingDef::Button, "Square",   "Face Buttons",  "ControlMapping", "Square",   "d:0/BUTTON_X"},
        {BindingDef::Button, "Triangle", "Face Buttons",  "ControlMapping", "Triangle", "d:0/BUTTON_Y"},
        // Triggers
        {BindingDef::Button, "L", "Triggers", "ControlMapping", "L", "d:0/LEFT_SHOULDER"},
        {BindingDef::Button, "R", "Triggers", "ControlMapping", "R", "d:0/RIGHT_SHOULDER"},
        // System
        {BindingDef::Button, "Start",  "System", "ControlMapping", "Start",  "d:0/BUTTON_START"},
        {BindingDef::Button, "Select", "System", "ControlMapping", "Select", "d:0/BUTTON_BACK"},
        // Analog Stick
        {BindingDef::Axis, "Up",    "Analog Stick", "ControlMapping", "An.Up",    "d:0/AXIS_Y-"},
        {BindingDef::Axis, "Down",  "Analog Stick", "ControlMapping", "An.Down",  "d:0/AXIS_Y+"},
        {BindingDef::Axis, "Left",  "Analog Stick", "ControlMapping", "An.Left",  "d:0/AXIS_X-"},
        {BindingDef::Axis, "Right", "Analog Stick", "ControlMapping", "An.Right", "d:0/AXIS_X+"},
    };
}
```

- [ ] **Step 3: Implement controllerSettingDefs**

Replace the stub `controllerSettingDefs`. PPSSPP controller settings live in `ppsspp.ini` `[Control]` section:

```cpp
QVector<SettingDef> PPSSPPAdapter::controllerSettingDefs() const {
    return {
        {"", "", "", "Control", "AnalogDeadzone",
         "Analog Deadzone", "Sets the analog stick deadzone.",
         SettingDef::Int, "15", {}, 0, 100, 1, "", "%"},

        {"", "", "", "Control", "AnalogSensitivity",
         "Analog Sensitivity", "Sets the analog stick sensitivity.",
         SettingDef::Int, "110", {}, 0, 200, 1, "", "%"},
    };
}
```

- [ ] **Step 4: Implement hotkeyBindingDefs**

Replace the stub `hotkeyBindingDefs`. PPSSPP hotkeys are in the `ControlMapping` section of `controls.ini`:

```cpp
QVector<HotkeyDef> PPSSPPAdapter::hotkeyBindingDefs() const {
    return {
        // Speed
        {"Fast-forward",       "Speed",       "ControlMapping", "Fast-forward",  "d:0/RIGHT_TRIGGER"},
        {"Speed Toggle",       "Speed",       "ControlMapping", "SpeedToggle",   ""},
        {"Alt Speed 1",        "Speed",       "ControlMapping", "Alt speed 1",   ""},
        {"Alt Speed 2",        "Speed",       "ControlMapping", "Alt speed 2",   ""},
        {"Frame Advance",      "Speed",       "ControlMapping", "Frame Advance", ""},
        // System
        {"Rewind",             "System",      "ControlMapping", "Rewind",        ""},
        {"Screenshot",         "System",      "ControlMapping", "Screenshot",    ""},
        {"Mute Toggle",        "System",      "ControlMapping", "Mute toggle",   ""},
        {"Reset",              "System",      "ControlMapping", "Reset",         ""},
        // Save States
        {"Save State",         "Save States", "ControlMapping", "Save State",    ""},
        {"Load State",         "Save States", "ControlMapping", "Load State",    ""},
        {"Previous Slot",      "Save States", "ControlMapping", "Previous Slot", ""},
        {"Next Slot",          "Save States", "ControlMapping", "Next Slot",     ""},
    };
}
```

- [ ] **Step 5: Implement formatBinding**

Replace the stub `formatBinding`:

```cpp
QString PPSSPPAdapter::formatBinding(int deviceIndex, const QString& element,
                                      bool isAxis, bool positive) const {
    // PPSSPP format: d:{index}/{element} for buttons, d:{index}/{element}{+/-} for axes
    if (isAxis) {
        QString suffix = positive ? "+" : "-";
        return QString("d:%1/%2%3").arg(deviceIndex).arg(element, suffix);
    }
    return QString("d:%1/%2").arg(deviceIndex).arg(element);
}
```

- [ ] **Step 6: Build and run tests**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: Build succeeds, all tests pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "feat: add PPSSPP controller bindings, hotkeys, and formatBinding override"
```

---

### Task 8: Serial Extraction, Resume State, RetroAchievements, Asset Matching

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp`

- [ ] **Step 1: Implement extractSerial**

Replace the stub `extractSerial`:

```cpp
QString PPSSPPAdapter::extractSerial(const QString& romPath) const {
    // PSP ISOs store game metadata in PSP_GAME/PARAM.SFO (binary SFO format)
    QByteArray sfoData = Iso9660::readFile(romPath, "PSP_GAME/PARAM.SFO");
    if (sfoData.isEmpty()) {
        qWarning() << "[PPSSPP] Failed to read PSP_GAME/PARAM.SFO from:" << romPath;
        return {};
    }
    QString discId = SfoParser::extractDiscId(sfoData);
    if (discId.isEmpty()) {
        qWarning() << "[PPSSPP] No DISC_ID found in PARAM.SFO for:" << romPath;
    }
    return discId;
}
```

- [ ] **Step 2: Implement findResumeFile**

Replace the stub `findResumeFile`:

```cpp
QString PPSSPPAdapter::findResumeFile(const QString& serial, const QString& savesRoot) const {
    if (serial.isEmpty()) return {};

    // PPSSPP save states: {serial}-{slot}.ppst (e.g., ULES00151-0.ppst)
    // We use slot 0 as the dedicated resume slot
    const QString statesDir = savesRoot + "/psp/savestates";
    QDir dir(statesDir);
    if (!dir.exists()) return {};

    const QString resumeFile = serial + "-0.ppst";
    if (dir.exists(resumeFile)) {
        return statesDir + "/" + resumeFile;
    }
    return {};
}
```

- [ ] **Step 3: Implement resumeLaunchArgs**

Replace the stub `resumeLaunchArgs`:

```cpp
QStringList PPSSPPAdapter::resumeLaunchArgs(const QString& stateFilePath) const {
    return {"--state=" + stateFilePath};
}
```

- [ ] **Step 4: Implement patchRetroAchievements**

Replace the stub `patchRetroAchievements`:

```cpp
void PPSSPPAdapter::patchRetroAchievements(const QString& username,
                                            const QString& token,
                                            bool enabled,
                                            bool hardcore,
                                            bool notifications,
                                            bool sounds) {
    Q_UNUSED(username);
    Q_UNUSED(token);
    Q_UNUSED(notifications);
    // No credential patching — PPSSPP stores its own RA login.
    const QString mainPath = configFilePath();
    QString content;
    if (readConfigFile(mainPath, content, "PPSSPP")) {
        QVector<IniKeyPatch> patches = {
            {"Achievements", "AchievementsEnable", enabled ? "True" : "False"},
            {"Achievements", "AchievementsHardcoreMode", hardcore ? "True" : "False"},
            {"Achievements", "AchievementsSoundEffects", sounds ? "True" : "False"},
        };
        if (patchIniKeys(content, patches))
            writeConfigFile(mainPath, content, "PPSSPP");
    }
}
```

- [ ] **Step 5: Implement matchAsset**

Replace the stub `matchAsset`:

```cpp
QString PPSSPPAdapter::matchAsset(const QStringList& assetNames) const {
    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
#if defined(Q_OS_MACOS)
        if (lower.contains("macos") && name.endsWith(".zip"))
            return name;
#elif defined(Q_OS_WIN)
        if (lower.contains("windows") && lower.contains("x64") && name.endsWith(".zip"))
            return name;
#else
        if (name.endsWith(".AppImage"))
            return name;
        if (lower.contains("linux") && (name.endsWith(".tar.gz") || name.endsWith(".tar.xz")))
            return name;
#endif
    }
    return EmulatorAdapter::matchAsset(assetNames);
}
```

- [ ] **Step 6: Build and run all tests**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: Build succeeds, all tests pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "feat: add PPSSPP serial extraction, resume state, RA support, and asset matching"
```

---

### Task 9: Logo & UI Registration

**Files:**
- Create: `cpp/qml/AppUI/images/ppsspp_logo.png` (must be provided or copied)
- Modify: `cpp/qml/AppUI/EmulatorLogos.js:1-7`
- Modify: `cpp/qml/SetupWizard/EmulatorCard.qml:17-23`
- Modify: `cpp/CMakeLists.txt:196-199,245-247`

- [ ] **Step 1: Add logo PNG file**

Place a PPSSPP logo PNG at `cpp/qml/AppUI/images/ppsspp_logo.png`. If no logo file is available, create a placeholder or source one.

- [ ] **Step 2: Update EmulatorLogos.js**

In `cpp/qml/AppUI/EmulatorLogos.js`, add the PPSSPP entry:

```javascript
function logoForEmu(emuId) {
    var logos = {
        "pcsx2":       "qrc:/AppUI/qml/AppUI/images/pcsx2_logo.png",
        "duckstation": "qrc:/AppUI/qml/AppUI/images/duckstation_logo.png",
        "ppsspp":      "qrc:/AppUI/qml/AppUI/images/ppsspp_logo.png"
    }
    return logos[emuId] || ""
}
```

- [ ] **Step 3: Update EmulatorCard.qml**

In `cpp/qml/SetupWizard/EmulatorCard.qml`, add to `logoForEmu()`:

```qml
function logoForEmu(id) {
    var logos = {
        "pcsx2": "qrc:/SetupWizard/qml/AppUI/images/pcsx2_logo.png",
        "duckstation": "qrc:/SetupWizard/qml/AppUI/images/duckstation_logo.png",
        "ppsspp": "qrc:/SetupWizard/qml/AppUI/images/ppsspp_logo.png"
    }
    return logos[id] || ""
}
```

- [ ] **Step 4: Update CMakeLists.txt RESOURCES**

Add logo to both QML module resource sections:

In the SetupWizard RESOURCES block (around line 197):
```cmake
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
```

In the AppUI RESOURCES block (around line 246):
```cmake
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
```

- [ ] **Step 5: Build and run all tests**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: Build succeeds, all tests pass. (If logo PNG is missing, build may warn but should still succeed.)

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/AppUI/images/ppsspp_logo.png cpp/qml/AppUI/EmulatorLogos.js cpp/qml/SetupWizard/EmulatorCard.qml cpp/CMakeLists.txt
git commit -m "feat: add PPSSPP logo and UI registration"
```

---

### Task 10: Final Verification

**Files:** None (verification only)

- [ ] **Step 1: Full clean build**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && rm -rf build && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build`
Expected: Clean build succeeds with no errors.

- [ ] **Step 2: Run all tests**

Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && ctest --test-dir build --output-on-failure`
Expected: All tests pass (IniFile, RomScanner, Iso9660Reader, SfoParser).

- [ ] **Step 3: Verify adapter registration**

Check that the adapter registry includes PPSSPP by searching the compiled binary:
Run: `strings /Users/mark/Documents/EmuFront-Project/cpp/build/EmulatorFrontend | grep -i ppsspp`
Expected: Shows "ppsspp", "PPSSPP" strings confirming the adapter is linked.

- [ ] **Step 4: Verify manifest loads**

Run: `ls -la /Users/mark/Documents/EmuFront-Project/manifests/ppsspp.json && cat /Users/mark/Documents/EmuFront-Project/manifests/ppsspp.json | python3 -m json.tool`
Expected: Valid JSON, all fields present.
