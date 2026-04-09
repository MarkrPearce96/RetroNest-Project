# Emulator Path Restructure Design

**Date:** 2026-04-02
**Status:** Approved

## Problem

The current folder structure creates top-level directories for every emulator-specific path type (cache, screenshots, cheats, textures, videos, covers), each with emulator subfolders. This clutters the root directory.

## Current Structure

```
EmuFront/
  bios/
  saves/ps2/          (savestates, memcards as suffixes)
  saves/psx/
  cache/ps2/
  cache/psx/
  screenshots/ps2/
  screenshots/psx/
  cheats/ps2/
  cheats/psx/
  textures/ps2/
  textures/psx/
  videos/ps2/
  covers/             (shared)
  emulators/
  roms/
  config/
  themes/
  downloaded_media/
```

## New Structure

```
EmuFront/
  bios/                          (shared, unchanged)
  saves/                         (shared, unchanged)
    ps2/
      savestates/
      memcards/
    psx/
      savestates/
      memcards/
  emulators/
    pcsx2/
      cache/
      screenshots/
      cheats/
      textures/
      videos/
    duckstation/
      cache/
      screenshots/
      cheats/
      textures/
  roms/                          (unchanged)
  config/                        (unchanged)
  themes/                        (unchanged)
  downloaded_media/              (unchanged)
```

### Key Decisions

- **BIOS** stays shared at root — all emulators share it.
- **Saves** stays shared at root with system subfolders — all emulators have saves/savestates/memcards.
- **Emulator-specific paths** (cache, screenshots, cheats, textures, videos) move into `emulators/{emuId}/`.
- **Covers** removed from `pathsDefs()` — already handled by `downloaded_media/`.
- **ROMs** stays at root — user-configured at install time.
- **Config** stays at root — app-level, not emulator-specific.

## Approach: Extend PathDef with PathBase Enum

Replace `bool usesBiosDir` with an enum to support three resolution bases.

### 1. PathDef Struct (emulator_adapter.h)

```cpp
enum class PathBase { Bios, Saves, Emulator };

struct PathDef {
    QString label;
    QString section;
    QString key;
    QString defaultSuffix;
    PathBase base = PathBase::Saves;
};
```

### 2. Adapter pathsDefs()

**PCSX2:**
```cpp
{"BIOS",         "Folders", "Bios",        "",             PathBase::Bios},
{"Save States",  "Folders", "Savestates",  "savestates",   PathBase::Saves},
{"Memory Cards", "Folders", "MemoryCards", "memcards",     PathBase::Saves},
{"Screenshots",  "Folders", "Screenshots", "screenshots",  PathBase::Emulator},
{"Cache",        "Folders", "Cache",       "cache",        PathBase::Emulator},
{"Cheats",       "Folders", "Cheats",      "cheats",       PathBase::Emulator},
{"Textures",     "Folders", "Textures",    "textures",     PathBase::Emulator},
{"Videos",       "Folders", "Videos",      "videos",       PathBase::Emulator},
```

**DuckStation:**
```cpp
{"BIOS",         "BIOS",        "SearchDirectory", "",            PathBase::Bios},
{"Memory Cards", "MemoryCards", "Directory",       "memcards",    PathBase::Saves},
{"Save States",  "Folders",     "SaveStates",      "savestates",  PathBase::Saves},
{"Screenshots",  "Folders",     "Screenshots",     "screenshots", PathBase::Emulator},
{"Cache",        "Folders",     "Cache",            "cache",       PathBase::Emulator},
{"Cheats",       "Folders",     "Cheats",           "cheats",      PathBase::Emulator},
{"Textures",     "Folders",     "Textures",         "textures",    PathBase::Emulator},
```

### 3. Path Resolution (paths_page.cpp)

Two locations change (initial load + reset to defaults):

```cpp
QString defaultPath;
switch (pd.base) {
    case PathBase::Bios:
        defaultPath = Paths::biosDir();
        break;
    case PathBase::Saves:
        defaultPath = Paths::savesDir(systemId) + "/" + pd.defaultSuffix;
        break;
    case PathBase::Emulator:
        defaultPath = Paths::emulatorsDir(emu.id) + "/" + pd.defaultSuffix;
        break;
}
```

The reset lambda needs to capture `emu.id` in addition to `systemId`.

### 4. ensureConfig Path Updates (both adapters)

Both `createDefaultConfig()` and `patchExistingConfig()` in each adapter hardcode the old paths (e.g. `rootPath + "/screenshots/ps2"`). These must be updated to use the new emulator-based paths (e.g. `Paths::emulatorsDir(emuId) + "/screenshots"`).

Affected paths in both adapters: screenshots, cache, cheats, textures, videos (PCSX2 only), covers (remove).

## Files Changed

1. `cpp/src/adapters/emulator_adapter.h` — add `PathBase` enum, update `PathDef` struct
2. `cpp/src/adapters/pcsx2_adapter.cpp` — update `pathsDefs()`, `createDefaultConfig()`, `patchExistingConfig()`
3. `cpp/src/adapters/duckstation_adapter.cpp` — update `pathsDefs()`, `createDefaultConfig()`, `patchExistingConfig()`
4. `cpp/src/ui/settings/paths_page.cpp` — switch-based path resolution
