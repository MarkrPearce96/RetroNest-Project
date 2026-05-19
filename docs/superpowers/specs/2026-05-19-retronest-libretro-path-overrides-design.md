# RetroNest libretro path overrides — design

**Date:** 2026-05-19
**Sub-project:** Functional path overrides for libretro adapters (PCSX2 + mGBA)
**Status:** Design — pending implementation plan

## Summary

Make the existing per-emulator Paths UI actually persist and propagate path overrides for libretro adapters. Today the UI lists three rows for mGBA and zero for PCSX2; the Save button calls `ConfigService::savePaths` which writes to `adapter->configFilePath()`'s INI. Libretro adapters return an empty `configFilePath()`, so the write early-exits and overrides silently fail to persist. This sub-project introduces a libretro-specific persistence backend and wires the stored overrides through to both RetroNest-controlled paths (savestates, screenshots) and core-controlled paths (PCSX2 memcards/textures via two new private env enums, mGBA saves via the existing libretro `save_dir`).

In scope:
- **PCSX2**: 3 overridable paths — Memory Cards, Save States, Textures.
- **mGBA**: 3 overridable paths — Saves, Save States, Screenshots (the existing 3 rows, now made functional).

Out of scope:
- Native-adapter persistence (Dolphin, DuckStation, PPSSPP) — unchanged, still uses each adapter's INI.
- BIOS dir override — stays shared at `Paths::biosDir()` for all PS2/PSX emulators.
- Migrating existing files — going-forward semantics only.
- Mid-session live reload — overrides apply at next game launch.

## Motivation

Power users keep emulator data on external drives, NAS mounts, or shared dirs alongside other frontends (RetroArch, standalone PCSX2). The Paths UI implies they can redirect emulator data, but for libretro adapters that promise is currently broken — Browse-and-Save updates the field for the in-flight session and then forgets, because there's no working persistence backend.

Native adapters (Dolphin/DuckStation/PPSSPP) work today because each writes overrides into a real emulator INI that the underlying process reads on launch. Libretro adapters need a parallel mechanism, since the libretro core gets a single `save_dir` and `system_dir` from the host and no INI of its own to consult.

## Scope decisions (locked during brainstorm)

### PCSX2 overridable paths: 3 rows (no BIOS)
- ✅ Memory Cards — `<save_dir>/memcards`, owned by `pcsx2-libretro/Settings.cpp:InitializeDefaults`.
- ✅ Save States — RetroNest computes the slot path; in-memory zip from `retro_serialize` is written there.
- ✅ Textures — `<save_dir>/textures/<serial>/`, owned by `pcsx2-libretro/Settings.cpp:InitializeDefaults`.
- ❌ BIOS — stays at `Paths::biosDir()`, shared across all PS2 emulators. Splitting per-emulator complicates future PS2 emulator additions; no real user demand.

### mGBA overridable paths: keep existing 3
- ✅ Saves (battery), Save states, Screenshots — already in `MgbaLibretroAdapter::pathsDefs()`, just need real persistence and propagation.

### Migration semantics: going-forward only
- Files already at the old default location stay there. New writes land at the override.
- No prompt, no copy step, no rollback path. Matches PCSX2 standalone's path-change UX.

### Apply timing: next game launch
- PCSX2's `EmuFolders` is set up once per `retro_load_game` via `Settings::InitializeDefaults`. Mid-session change of memcards/textures dirs would require reinitializing those subsystems — invasive and unnecessary for the user value.
- For RetroNest-controlled paths the override is consulted each time a path is computed, so they'd technically apply immediately — but the consistent "restart the game to apply" rule simplifies the UX and matches PCSX2.

### Storage format: single JSON in user data dir
- One file at `~/Library/Application Support/RetroNest/path_overrides.json`, sectioned by emulator id.
- One file, easy to inspect/back up, sits next to the existing `config.json`. Per-emulator JSON sidecars considered (mirrors `options.json` pattern) and rejected for proliferation.

## Architecture

### Component map

```
QML PathsSettings.qml
  └─ app.pathDefs(emuId)            ─┐
  └─ app.pathValue(emuId, sect, key) ├─→ ConfigService
  └─ app.savePaths(emuId, values)   ─┘      │
                                            │ if adapter->configFilePath() empty:
                                            ├──→ PathOverridesStore (new)
                                            │       └─ path_overrides.json (read/write)
                                            └──→ existing IniFile path (native adapters)

Runtime consumption (RetroNest):
  GameSession::libretroSlotPath  → consults PathOverridesStore for SaveStates
  GameSession::terminate         → consults PathOverridesStore for SaveStates
  CoreRuntime::start (saveDir)   → consults PathOverridesStore for mGBA Saves
  Screenshot writer (TBD path)   → consults PathOverridesStore for Screenshots

Runtime consumption (pcsx2-libretro core):
  Settings::InitializeDefaults   → queries 2 new private env enums for Memcards/Textures
                                   environmentDispatch (RetroNest) ↔ PathOverridesStore
```

### New components

**`cpp/src/core/path_overrides_store.{h,cpp}`** (new) — singleton-style helper around `path_overrides.json`:
```cpp
class PathOverridesStore {
public:
    static PathOverridesStore& instance();

    QString read(const QString& emuId, const QString& key) const;
    void    write(const QString& emuId, const QString& key, const QString& path);
    void    clear(const QString& emuId, const QString& key);

private:
    void load();
    bool save() const;
    QJsonObject m_root;        // {emuId: {key: path}}
    QString     m_filePath;    // ~/Library/Application Support/RetroNest/path_overrides.json
};
```
Thread model: read on any thread (GUI for QML, worker for env_callbacks); writes only from GUI (Save button). Internal `QMutex` for safety; reload-on-write keeps the file the source of truth.

**Two new private libretro env enums** (in `pcsx2-libretro/libretro.h` and `RetroNest-Project/cpp/src/core/libretro/environment_callbacks.h`):
```cpp
RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR = 0x20003  // data: const char**
RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR = 0x20004  // data: const char**
```
Returns `true` and writes the override into `*data` if one is set; returns `false` otherwise. Same lifetime/no-clear convention as `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH = 0x20002`.

### Modified components

**`Pcsx2LibretroAdapter::pathsDefs()`** (new override) — returns 3 entries with section=`"libretro"` and keys `MemoryCards`/`SaveStates`/`Textures`:
```cpp
QVector<PathDef> Pcsx2LibretroAdapter::pathsDefs() const {
    return {
        { "Memory Cards", "libretro", "MemoryCards", "memcards",   PathBase::EmulatorData },
        { "Save States",  "libretro", "SaveStates",  "savestates", PathBase::EmulatorData },
        { "Textures",     "libretro", "Textures",    "textures",   PathBase::EmulatorData },
    };
}
```

**`MgbaLibretroAdapter::pathsDefs()`** (modified) — keep 3 rows, but populate section/key (currently empty):
```cpp
{ "Saves",       "libretro", "Saves",       "saves",       PathBase::EmulatorData },
{ "Save states", "libretro", "SaveStates",  "savestates",  PathBase::EmulatorData },
{ "Screenshots", "libretro", "Screenshots", "screenshots", PathBase::EmulatorData },
```

**`ConfigService::pathValue` / `ConfigService::savePaths`** — branch on `adapter->configFilePath().isEmpty()`:
- Empty → use `PathOverridesStore`. Read returns `store.read(emuId, key)`. Save iterates `values` and calls `store.write(emuId, key, value)` for each.
- Non-empty → existing INI code path, unchanged.

**`GameSession::libretroSlotPath`** + **`GameSession::terminate`** (resume file write) — wrap the existing `Paths::emulatorDataDir(...)+"/savestates"` computation in a `PathOverridesStore::read(emuId, "SaveStates")` lookup with the default as fallback.

**`CoreRuntime::start`** — for mGBA Saves, when computing the `save_dir` passed via env, consult `PathOverridesStore::read(emuId, "Saves")` first. Note: this means mGBA's libretro `save_dir` may differ from PCSX2's, which is fine — `save_dir` is per-session.

**Screenshot writer** — same wrapping where the screenshot file path is constructed (path TBD during implementation plan; current screenshot code lives at one site).

**`environmentDispatch`** (RetroNest) — adds two new cases:
```cpp
case RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR:
case RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR: {
    const QString key = (cmd == ...MEMCARDS...) ? "MemoryCards" : "Textures";
    const QString path = PathOverridesStore::instance().read("pcsx2", key);
    if (path.isEmpty()) return false;
    static thread_local QByteArray cached;     // lifetime-stable storage like bootStatePath
    cached = path.toUtf8();
    *static_cast<const char**>(data) = cached.constData();
    return true;
}
```

**`pcsx2-libretro/Settings.cpp:InitializeDefaults`** — after the existing save_dir-based default block, query the two env enums and overwrite `Folders/MemoryCards` / `Folders/Textures` if returned. Falls through to current default if the env returns false (mGBA / older RetroNest / non-RetroNest hosts).

### Data flow on path change

1. User opens Settings → Paths → PCSX2 tab → Browse on Memory Cards row.
2. File picker → `pathField.text` updates to new dir.
3. User clicks Save → QML walks `pathRepeater`, builds `values = {"libretro/MemoryCards": "/new/path", ...}`, calls `app.savePaths("pcsx2", values)`.
4. `ConfigService::savePaths` sees `Pcsx2LibretroAdapter::configFilePath().isEmpty()` is true → routes to `PathOverridesStore::write("pcsx2", "MemoryCards", "/new/path")`.
5. JSON file is rewritten on disk.
6. **Next game launch**: `retro_load_game` → pcsx2-libretro queries `GET_MEMCARDS_DIR` env → RetroNest reads override from `PathOverridesStore` → returns `/new/path` → `Settings::InitializeDefaults` sets `Folders/MemoryCards = /new/path`. PCSX2's `EmuFolders` system picks it up; per-serial subdir is appended underneath.

## Trade-offs

- **Single JSON file vs per-emulator sidecars**: chose single file for inspectability and simpler backup story. Acceptable because overrides are sparse (rarely all 6 set) and the schema is shallow.
- **New private env enums vs reusing save_dir for PCSX2**: chose new enums because users may want to override memcards-only OR textures-only, which a single `save_dir` redirect can't express. Cost: two new env numbers (0x20003, 0x20004) added to a fork-private range.
- **Going-forward semantics vs migration**: chose no migration. Migration code is risky (cross-volume, partial failure, permission errors) and adds substantial scope. Users moving emulator dirs are typically comfortable with manual copy.
- **Adapter-side `configFilePath().isEmpty()` branching vs new virtual method**: chose branching to avoid adding a new virtual to the EmulatorAdapter interface. The branch is contained to two methods in ConfigService.

## Testing

**Unit tests**
- `tests/test_path_overrides_store.cpp` (new) — round-trip write → read → clear; JSON file format stability; missing-file produces empty reads, not errors; concurrent reads thread-safe.
- `tests/test_environment_callbacks.cpp` (extend) — `GET_MEMCARDS_DIR` and `GET_TEXTURES_DIR` return override when set, return false when not.
- `pcsx2-libretro/tools/test_settings_overrides.cpp` (new standalone) — `Settings::InitializeDefaults` applies override when env returns it; falls back to save_dir-suffix default when env returns false.

**Integration smoke** (per phase, manual)
- PCSX2: set Memory Cards override → launch R&C 2 → memcard write goes to override dir; existing memcard at default location not touched.
- PCSX2: set Textures override → toggle `pcsx2_load_texture_replacements` → confirm subdir resolution `<override>/<serial>/`.
- mGBA: set Saves override → launch a GBA ROM with battery saves → `.srm` lands at override path.
- Both: clear an override → next launch falls back to default cleanly.

## Code volume estimate

- **RetroNest** (~230 LOC source + ~120 LOC test):
  - `PathOverridesStore`: ~80 LOC
  - `ConfigService` routing: ~25 LOC
  - Adapter `pathsDefs()` overrides: ~20 LOC (PCSX2 new, mGBA edited)
  - Runtime consumption sites (libretroSlotPath / terminate / CoreRuntime::start / screenshot writer): ~40 LOC
  - `environmentDispatch` cases: ~30 LOC
  - Header / forward declarations: ~10 LOC
  - Wiring through `ConfigService::pathDefs` for value display (no change expected)
  - Tests: ~120 LOC
- **pcsx2-libretro** (~40 LOC source + ~30 LOC test):
  - New env enum constants: ~5 LOC
  - `Settings::InitializeDefaults` env queries: ~25 LOC
  - Standalone unit test: ~30 LOC

## Open questions for the plan stage

- **Screenshot path resolution site** — where exactly is the screenshot file path constructed today? Need to locate during the plan stage so the runtime-consumption wiring covers it.
- **mGBA Saves persistence model** — verify in the plan stage whether mGBA libretro writes `.srm` directly to `save_dir` or exposes the buffer via `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` and RetroNest persists. If the former, override propagates via `save_dir`; if the latter, RetroNest's persistence-write path takes the override directly. User-facing behavior identical either way.
- **`PathOverridesStore` reload semantics** — if a user edits `path_overrides.json` by hand while RetroNest is running, do reads pick up the change? Default: cache the parsed JSON in memory, reload only on `write`. Note in implementation plan; revisit if user reports a need.
- **Empty-string override vs unset** — treat empty string in JSON as equivalent to "no override" (fall back to default) rather than "use empty path". Document explicitly in `PathOverridesStore::read`.

## Linked memories
- `[[session-handoff-host-runtime-parity-shipped]]` — `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH = 0x20002` precedent for private env enum design.
- `[[session-handoff-core-osd-toast-shipped]]` — weak-stub-bridge pattern is reusable if any of the new env handlers grow heavy enough to need test isolation.
- `[[rebuild-before-debugging-regressions]]` — once this lands, smoke verification MUST start with mtime checks on both binaries.
