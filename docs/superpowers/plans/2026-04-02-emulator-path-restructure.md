# Emulator Path Restructure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move emulator-specific paths (cache, screenshots, cheats, textures, videos) into `emulators/{emuId}/` and remove covers from path definitions, keeping bios and saves at root.

**Architecture:** Replace the `bool usesBiosDir` field in `PathDef` with a `PathBase` enum (`Bios`, `Saves`, `Emulator`) so each path definition declares which base directory it resolves from. Update both adapters' `pathsDefs()`, `createDefaultConfig()`, and `patchExistingConfig()` to use the new paths. Update `paths_page.cpp` resolution logic to handle the three bases.

**Tech Stack:** C++17, Qt6

---

### Task 1: Add PathBase enum and update PathDef struct

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h:66-75`

- [ ] **Step 1: Replace usesBiosDir with PathBase enum**

In `cpp/src/adapters/emulator_adapter.h`, add the enum before `PathDef` and update the struct:

```cpp
/**
 * PathBase — which root directory a PathDef resolves from.
 */
enum class PathBase {
    Bios,      // Paths::biosDir()
    Saves,     // Paths::savesDir(systemId) + "/" + suffix
    Emulator,  // Paths::emulatorsDir(emuId) + "/" + suffix
};

/**
 * PathDef — describes a configurable folder path for an emulator.
 */
struct PathDef {
    QString label;          // e.g. "BIOS"
    QString section;        // INI section, e.g. "Folders" or "BIOS"
    QString key;            // INI key, e.g. "Bios" or "SearchDirectory"
    QString defaultSuffix;  // appended to base dir, e.g. "savestates"
    PathBase base = PathBase::Saves;
};
```

- [ ] **Step 2: Build to verify no compile errors**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: Compile errors in `pcsx2_adapter.cpp` and `duckstation_adapter.cpp` because they still pass a `bool` as the 5th `PathDef` field. This confirms the struct change is picked up.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "refactor: replace PathDef usesBiosDir bool with PathBase enum"
```

---

### Task 2: Update PCSX2Adapter pathsDefs()

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:615-627`

- [ ] **Step 1: Update pathsDefs() to use PathBase enum and remove covers**

Replace the `pathsDefs()` return block:

```cpp
QVector<PathDef> PCSX2Adapter::pathsDefs() const {
    return {
        {"BIOS",         "Folders", "Bios",        "",            PathBase::Bios},
        {"Save States",  "Folders", "Savestates",  "savestates",  PathBase::Saves},
        {"Memory Cards", "Folders", "MemoryCards", "memcards",    PathBase::Saves},
        {"Screenshots",  "Folders", "Screenshots", "screenshots", PathBase::Emulator},
        {"Cache",        "Folders", "Cache",       "cache",       PathBase::Emulator},
        {"Cheats",       "Folders", "Cheats",      "cheats",      PathBase::Emulator},
        {"Textures",     "Folders", "Textures",    "textures",    PathBase::Emulator},
        {"Videos",       "Folders", "Videos",      "videos",      PathBase::Emulator},
    };
}
```

- [ ] **Step 2: Build to verify compile succeeds for this file**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: `pcsx2_adapter.cpp` compiles. `duckstation_adapter.cpp` still fails (expected — fixed in Task 3).

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "refactor: update PCSX2 pathsDefs to use PathBase enum, remove covers"
```

---

### Task 3: Update DuckStationAdapter pathsDefs()

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:677-688`

- [ ] **Step 1: Update pathsDefs() to use PathBase enum and remove covers**

Replace the `pathsDefs()` return block:

```cpp
QVector<PathDef> DuckStationAdapter::pathsDefs() const {
    return {
        {"BIOS",         "BIOS",        "SearchDirectory", "",            PathBase::Bios},
        {"Memory Cards", "MemoryCards", "Directory",       "memcards",    PathBase::Saves},
        {"Save States",  "Folders",     "SaveStates",      "savestates",  PathBase::Saves},
        {"Screenshots",  "Folders",     "Screenshots",     "screenshots", PathBase::Emulator},
        {"Cache",        "Folders",     "Cache",           "cache",       PathBase::Emulator},
        {"Cheats",       "Folders",     "Cheats",          "cheats",      PathBase::Emulator},
        {"Textures",     "Folders",     "Textures",        "textures",    PathBase::Emulator},
    };
}
```

- [ ] **Step 2: Build to verify full compile succeeds**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: Full compile success — all `PathDef` usages now use the enum.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "refactor: update DuckStation pathsDefs to use PathBase enum, remove covers"
```

---

### Task 4: Update paths_page.cpp resolution logic

**Files:**
- Modify: `cpp/src/ui/settings/paths_page.cpp:67-73` (initial load)
- Modify: `cpp/src/ui/settings/paths_page.cpp:111-118` (reset to defaults)

- [ ] **Step 1: Update initial path resolution to use switch**

In `paths_page.cpp`, replace the path resolution block (lines 67-73):

Old code:
```cpp
            QString defaultPath;
            if (pd.usesBiosDir) {
                defaultPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
            } else {
                defaultPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath() + "/" + pd.defaultSuffix;
            }
```

New code:
```cpp
            QString defaultPath;
            switch (pd.base) {
                case PathBase::Bios:
                    defaultPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
                    break;
                case PathBase::Saves:
                    defaultPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath() + "/" + pd.defaultSuffix;
                    break;
                case PathBase::Emulator:
                    defaultPath = QFileInfo(Paths::emulatorsDir(emu.id)).absoluteFilePath() + "/" + pd.defaultSuffix;
                    break;
            }
```

- [ ] **Step 2: Update reset-to-defaults handler to use switch**

Replace the reset handler block (lines 111-118). Also capture `emuId` in the lambda:

Old code:
```cpp
        connect(resetBtn, &QPushButton::clicked, this, [pathEntries, systemId]() {
            for (auto& entry : *pathEntries) {
                QString defaultPath;
                if (entry.def.usesBiosDir) {
                    defaultPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
                } else {
                    defaultPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath() + "/" + entry.def.defaultSuffix;
                }
                entry.edit->setText(defaultPath);
            }
        });
```

New code:
```cpp
        QString emuId = emu.id;
        connect(resetBtn, &QPushButton::clicked, this, [pathEntries, systemId, emuId]() {
            for (auto& entry : *pathEntries) {
                QString defaultPath;
                switch (entry.def.base) {
                    case PathBase::Bios:
                        defaultPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
                        break;
                    case PathBase::Saves:
                        defaultPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath() + "/" + entry.def.defaultSuffix;
                        break;
                    case PathBase::Emulator:
                        defaultPath = QFileInfo(Paths::emulatorsDir(emuId)).absoluteFilePath() + "/" + entry.def.defaultSuffix;
                        break;
                }
                entry.edit->setText(defaultPath);
            }
        });
```

- [ ] **Step 3: Build to verify compile succeeds**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: Full compile success.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/settings/paths_page.cpp
git commit -m "refactor: update paths_page resolution logic for PathBase enum"
```

---

### Task 5: Update PCSX2Adapter createDefaultConfig() paths

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:390-464`

- [ ] **Step 1: Update path variables in createDefaultConfig()**

Replace the path variable block (lines 396-409):

Old code:
```cpp
    const QString rootPath = QFileInfo(Paths::root()).absoluteFilePath();
    const QString savestatesPath = savesPath + "/savestates";
    const QString screenshotsPath = rootPath + "/screenshots/ps2";
    const QString cachePath = rootPath + "/cache/ps2";
    const QString cheatsPath = rootPath + "/cheats/ps2";
    const QString coversPath = rootPath + "/covers";
    const QString memcardsPath = savesPath + "/memcards";
    const QString snapsPath = screenshotsPath;
    const QString videosPath = rootPath + "/videos/ps2";
    const QString texturesPath = rootPath + "/textures/ps2";
    const QString logsPath = rootPath + "/logs/ps2";
    const QString inputProfilesPath = savesPath + "/inputprofiles";
    const QString patchesPath = rootPath + "/patches/ps2";
    const QString gamSettingsPath = rootPath + "/gamesettings/ps2";
```

New code:
```cpp
    const QString emuDir = QFileInfo(Paths::emulatorsDir("pcsx2")).absoluteFilePath();
    const QString savestatesPath = savesPath + "/savestates";
    const QString memcardsPath = savesPath + "/memcards";
    const QString screenshotsPath = emuDir + "/screenshots";
    const QString cachePath = emuDir + "/cache";
    const QString cheatsPath = emuDir + "/cheats";
    const QString snapsPath = screenshotsPath;
    const QString videosPath = emuDir + "/videos";
    const QString texturesPath = emuDir + "/textures";
    const QString logsPath = emuDir + "/logs";
    const QString inputProfilesPath = savesPath + "/inputprofiles";
    const QString patchesPath = emuDir + "/patches";
    const QString gamSettingsPath = emuDir + "/gamesettings";
```

- [ ] **Step 2: Remove Covers line from the INI template**

In the `QStringList lines` block, remove this line:
```cpp
        "Covers = " + coversPath,
```

- [ ] **Step 3: Build to verify compile succeeds**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: Compile success.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "refactor: update PCSX2 createDefaultConfig paths to emulators dir"
```

---

### Task 6: Update PCSX2Adapter patchExistingConfig() paths

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:536-553`

- [ ] **Step 1: Update folder entries in patchExistingConfig()**

Replace the path variable and folders block (lines 537-554):

Old code:
```cpp
    const QString rootPath = QFileInfo(Paths::root()).absoluteFilePath();
    struct FolderEntry { QString key; QString value; };
    QVector<FolderEntry> folders = {
        {"Bios", biosPath},
        {"Savestates", savesPath + "/savestates"},
        {"Screenshots", rootPath + "/screenshots/ps2"},
        {"Cache", rootPath + "/cache/ps2"},
        {"Cheats", rootPath + "/cheats/ps2"},
        {"Covers", rootPath + "/covers"},
        {"MemoryCards", savesPath + "/memcards"},
        {"Snapshots", rootPath + "/screenshots/ps2"},
        {"Videos", rootPath + "/videos/ps2"},
        {"Textures", rootPath + "/textures/ps2"},
        {"Logs", rootPath + "/logs/ps2"},
        {"InputProfiles", savesPath + "/inputprofiles"},
        {"Patches", rootPath + "/patches/ps2"},
        {"GameSettings", rootPath + "/gamesettings/ps2"},
    };
```

New code:
```cpp
    const QString emuDir = QFileInfo(Paths::emulatorsDir("pcsx2")).absoluteFilePath();
    struct FolderEntry { QString key; QString value; };
    QVector<FolderEntry> folders = {
        {"Bios", biosPath},
        {"Savestates", savesPath + "/savestates"},
        {"MemoryCards", savesPath + "/memcards"},
        {"Screenshots", emuDir + "/screenshots"},
        {"Cache", emuDir + "/cache"},
        {"Cheats", emuDir + "/cheats"},
        {"Snapshots", emuDir + "/screenshots"},
        {"Videos", emuDir + "/videos"},
        {"Textures", emuDir + "/textures"},
        {"Logs", emuDir + "/logs"},
        {"InputProfiles", savesPath + "/inputprofiles"},
        {"Patches", emuDir + "/patches"},
        {"GameSettings", emuDir + "/gamesettings"},
    };
```

- [ ] **Step 2: Build to verify compile succeeds**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: Compile success.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "refactor: update PCSX2 patchExistingConfig paths to emulators dir"
```

---

### Task 7: Update DuckStationAdapter createDefaultConfig() paths

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:518-579`

- [ ] **Step 1: Update path variables in createDefaultConfig()**

Replace the path variable block (lines 524-532):

Old code:
```cpp
    const QString rootPath = QFileInfo(Paths::root()).absoluteFilePath();
    const QString memcardsPath = savesPath + "/memcards";
    const QString savestatesPath = savesPath + "/savestates";
    const QString screenshotsPath = rootPath + "/screenshots/psx";
    const QString coversPath = rootPath + "/covers";
    const QString cachePath = rootPath + "/cache/psx";
    const QString cheatsPath = rootPath + "/cheats/psx";
    const QString texturesPath = rootPath + "/textures/psx";
    const QString videosPath = rootPath + "/videos/psx";
```

New code:
```cpp
    const QString emuDir = QFileInfo(Paths::emulatorsDir("duckstation")).absoluteFilePath();
    const QString memcardsPath = savesPath + "/memcards";
    const QString savestatesPath = savesPath + "/savestates";
    const QString screenshotsPath = emuDir + "/screenshots";
    const QString cachePath = emuDir + "/cache";
    const QString cheatsPath = emuDir + "/cheats";
    const QString texturesPath = emuDir + "/textures";
```

- [ ] **Step 2: Remove Covers and Videos lines from the INI template**

In the `QStringList lines` block, remove these lines:
```cpp
        "Covers = " + coversPath,
        "Videos = " + videosPath,
```

- [ ] **Step 3: Build to verify compile succeeds**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: Compile success.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "refactor: update DuckStation createDefaultConfig paths to emulators dir"
```

---

### Task 8: Update DuckStationAdapter patchExistingConfig() paths

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:617-629`

- [ ] **Step 1: Update folder entries in patchExistingConfig()**

Replace the path variable and folders block (lines 617-629):

Old code:
```cpp
    const QString rootPath = QFileInfo(Paths::root()).absoluteFilePath();
    struct FolderEntry { QString section; QString key; QString value; };
    QVector<FolderEntry> folders = {
        {"BIOS", "SearchDirectory", biosPath},
        {"MemoryCards", "Directory", savesPath + "/memcards"},
        {"Folders", "SaveStates", savesPath + "/savestates"},
        {"Folders", "Screenshots", rootPath + "/screenshots/psx"},
        {"Folders", "Covers", rootPath + "/covers"},
        {"Folders", "Cache", rootPath + "/cache/psx"},
        {"Folders", "Cheats", rootPath + "/cheats/psx"},
        {"Folders", "Textures", rootPath + "/textures/psx"},
        {"Folders", "Videos", rootPath + "/videos/psx"},
    };
```

New code:
```cpp
    const QString emuDir = QFileInfo(Paths::emulatorsDir("duckstation")).absoluteFilePath();
    struct FolderEntry { QString section; QString key; QString value; };
    QVector<FolderEntry> folders = {
        {"BIOS", "SearchDirectory", biosPath},
        {"MemoryCards", "Directory", savesPath + "/memcards"},
        {"Folders", "SaveStates", savesPath + "/savestates"},
        {"Folders", "Screenshots", emuDir + "/screenshots"},
        {"Folders", "Cache", emuDir + "/cache"},
        {"Folders", "Cheats", emuDir + "/cheats"},
        {"Folders", "Textures", emuDir + "/textures"},
    };
```

- [ ] **Step 2: Build to verify full compile succeeds**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: Full compile success.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "refactor: update DuckStation patchExistingConfig paths to emulators dir"
```

---

### Task 9: Remove stale top-level path directories from ensureDirectories()

**Files:**
- Modify: `cpp/src/core/paths.cpp:77-93`

- [ ] **Step 1: Check if ensureDirectories() creates any of the removed top-level dirs**

Read `cpp/src/core/paths.cpp` lines 77-93. The current `ensureDirectories()` creates: root, emulators, bios, saves, cache, roms, downloaded_media, config, themes.

The `cacheDir()` method at line 48 still points to `root + "/cache"` — this is a stale top-level path. However, `cacheDir()` may be used elsewhere, so check for usages first.

Run:
```bash
cd cpp && grep -rn "Paths::cacheDir\|cacheDir()" src/ --include="*.cpp" --include="*.h"
```

If `cacheDir()` is only used from adapters via `pathsDefs()` (which now uses `PathBase::Emulator`), it's dead code. If nothing else uses it, remove it from `ensureDirectories()`.

- [ ] **Step 2: Remove cacheDir from ensureDirectories if unused elsewhere**

If `cacheDir()` is only referenced in `paths.h`, `paths.cpp`, and adapter `pathsDefs()`, remove it from the `ensureDirectories()` dirs list:

Old code in `ensureDirectories()`:
```cpp
    QStringList dirs = {
        s_root,
        emulatorsDir(),
        biosDir(),
        savesDir(),
        cacheDir(),
        romsDir(),
        mediaDir(),
        configDir(),
        themesDir(),
    };
```

New code:
```cpp
    QStringList dirs = {
        s_root,
        emulatorsDir(),
        biosDir(),
        savesDir(),
        romsDir(),
        mediaDir(),
        configDir(),
        themesDir(),
    };
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | head -30
```

Expected: Compile success.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/paths.cpp
git commit -m "cleanup: remove stale cacheDir from ensureDirectories"
```

---

### Task 10: Final build and manual verification

- [ ] **Step 1: Clean build**

Run:
```bash
cd cpp && cmake --build build --clean-first 2>&1 | tail -5
```

Expected: Build succeeds with no errors or warnings related to paths.

- [ ] **Step 2: Launch and verify paths in settings**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

Open Settings > Paths. Verify:
- PCSX2 tab: BIOS points to `bios/`, Save States to `saves/ps2/savestates`, Memory Cards to `saves/ps2/memcards`, Screenshots/Cache/Cheats/Textures/Videos to `emulators/pcsx2/{folder}`
- DuckStation tab: same pattern with `saves/psx/` and `emulators/duckstation/{folder}`
- No "Covers" row in either tab
- "Reset to Defaults" works correctly for all three path types

- [ ] **Step 3: Commit all if any remaining changes**

```bash
git add -A
git commit -m "feat: restructure emulator paths — emulator-specific dirs under emulators/{id}"
```
