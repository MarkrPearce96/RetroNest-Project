# DuckStation libretro — save states + memory cards Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make PS1 progress persist in RetroNest's DuckStation libretro core — working save-state slots + resume-on-launch/exit, and persistent (file-backed) memory cards.

**Architecture:** Implement the three stub `retro_serialize*` functions against DuckStation's span-based save API; add one small public `System::LoadStateDataFromBuffer` to the fork (no public load-from-buffer exists); switch the memcard from `NonPersistent` to a file-backed `PerGameTitle` card whose directory comes from libretro's `GET_SAVE_DIRECTORY`; and add `findResumeFile()` + `pathsDefs()` to the RetroNest adapter. RetroNest's libretro save plumbing (slots, resume request/flush) is already generic and drives all of this.

**Tech Stack:** C++20 (DuckStation fork + libretro frontend), C++/Qt6 (RetroNest adapter), CMake, libretro ABI.

**Spec:** `docs/superpowers/specs/2026-06-03-duckstation-libretro-savestates-design.md`

**Verification model:** Like Phase 1, this is integration code verified by **build + manual smoke through RetroNest** — there's no meaningful pure-logic unit to test. Each task ends in a compile/link gate; Task 5 is the end-to-end manual verification.

**Build recipe (fork, machine-specific — from `BUILD_NOTES.md`):**
```sh
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS"
cmake -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DENABLE_OPENGL=OFF -DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON -DENABLE_LIBRETRO=ON
```
(Note: the fork is now at `duckstation-libretro/` directly — no `duckstation-src/`. If `build-arm64` doesn't exist yet, the above configure creates it; the flatten removed stale build dirs so the first build is from scratch.)

**RetroNest build (from `cpp/`):** `cmake --build build --target RetroNest`

---

## Task 0: Branch RetroNest

**Files:** none (git).

- [ ] **Step 1: Create a feature branch off main**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git checkout main
git checkout -b feat/duckstation-libretro-savestates
git branch --show-current   # expect: feat/duckstation-libretro-savestates
```

(The fork works directly on `master` as before — no branch needed there.)

---

## Task 1: Core — add `System::LoadStateDataFromBuffer`

**Files:**
- Modify: `duckstation-libretro/src/core/system.h` (declare, near `SaveStateDataToBuffer` at :299)
- Modify: `duckstation-libretro/src/core/system.cpp` (define, after `SaveStateDataToBuffer` ~:3309-3330)

- [ ] **Step 1: Declare in `system.h`**

In `namespace System`, immediately after the `SaveStateDataToBuffer` declaration (line 299):

```cpp
/// Loads a raw (headerless) save state from a memory buffer — the counterpart to
/// SaveStateDataToBuffer. Used by the libretro frontend's retro_unserialize.
bool LoadStateDataFromBuffer(std::span<const u8> data, Error* error);
```

- [ ] **Step 2: Define in `system.cpp`**

Immediately after `System::SaveStateDataToBuffer(...)` ends (~line 3330). It mirrors the save path with `Mode::Read`:

```cpp
bool System::LoadStateDataFromBuffer(std::span<const u8> data, Error* error)
{
  if (IsShutdown()) [[unlikely]]
  {
    Error::SetStringView(error, "System is invalid.");
    return false;
  }

  StateWrapper sw(data, StateWrapper::Mode::Read, SAVE_STATE_VERSION);
  if (!DoState(sw, /*update_display=*/true))
  {
    Error::SetStringView(error, "DoState() failed");
    return false;
  }

  return true;
}
```

Verify while editing: (a) `StateWrapper` has a `std::span<const u8>` + `Mode::Read` constructor (`util/state_wrapper.h` — delta §5 says it wraps `span<u8>`/`span<const u8>`); (b) `System::DoState(StateWrapper&, bool)` is the same member `SaveStateDataToBuffer` calls (confirmed: `system.cpp:2314`, called unqualified as `DoState(sw, false)` in the save path). If `update_display=true` causes an immediate-present issue in the inline model, fall back to `false` (the next `retro_run` renders regardless) — note which you used.

- [ ] **Step 3: Build the core to verify it compiles**

```bash
cd "$DS"
cmake -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DENABLE_OPENGL=OFF -DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON -DENABLE_LIBRETRO=ON
cmake --build build-arm64 --target core 2>&1 | tail -5
```
Expected: `libcore.a` relinks, no errors.

- [ ] **Step 4: Commit (fork master)**

```bash
cd "$DS"
git add src/core/system.h src/core/system.cpp
git commit -m "feat(core): add System::LoadStateDataFromBuffer for libretro retro_unserialize"
```

---

## Task 2: Core — implement `retro_serialize_size` / `retro_serialize` / `retro_unserialize`

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro.cpp` (replace the three stubs at ~:287-289)

- [ ] **Step 1: Ensure includes**

At the top of `libretro.cpp`, confirm `#include <span>` is present (add if missing) and that `core/system.h` and `common/error.h` are included (they are, from the boot code). `u8`/`s16` come from `common/types.h` (already transitively included).

- [ ] **Step 2: Replace the three stubs**

Replace:
```cpp
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void*, size_t) { return false; }
RETRO_API bool retro_unserialize(const void*, size_t) { return false; }
```
with:
```cpp
RETRO_API size_t retro_serialize_size(void)
{
  // Worst case (8 MB RAM enabled) so the buffer is always large enough and the
  // reported size is stable across the session, regardless of the RAM setting.
  return System::GetMaxSaveStateSize(/*enable_8mb_ram=*/true);
}

RETRO_API bool retro_serialize(void* data, size_t size)
{
  if (!System::IsValid())
    return false;
  Error error;
  size_t written = 0;
  std::span<u8> sp{static_cast<u8*>(data), size};
  if (!System::SaveStateDataToBuffer(sp, &written, &error))
  {
    if (g_log) g_log(RETRO_LOG_ERROR, "retro_serialize failed: %s\n", error.GetDescription().c_str());
    return false;
  }
  return true;
}

RETRO_API bool retro_unserialize(const void* data, size_t size)
{
  if (!System::IsValid())
    return false;
  Error error;
  std::span<const u8> sp{static_cast<const u8*>(data), size};
  if (!System::LoadStateDataFromBuffer(sp, &error))
  {
    if (g_log) g_log(RETRO_LOG_ERROR, "retro_unserialize failed: %s\n", error.GetDescription().c_str());
    return false;
  }
  return true;
}
```
(`g_log` is the cached `retro_log_printf_t` from `retro_init`; if the symbol differs in `libretro.cpp`, use the existing log mechanism. `GetMaxSaveStateSize`, `SaveStateDataToBuffer`, `IsValid` are all in `core/system.h`.)

- [ ] **Step 3: Build the dylib**

```bash
cd "$DS"
cmake --build build-arm64 --target duckstation_libretro 2>&1 | tail -6
nm -gU build-arm64/src/duckstation-libretro/duckstation_libretro.dylib | grep -c _retro_   # >= 22
```
Expected: links clean; symbol count unchanged (≥22).

- [ ] **Step 4: Commit (fork master)**

```bash
git add src/duckstation-libretro/libretro.cpp
git commit -m "feat(libretro): implement retro_serialize/unserialize/serialize_size"
```

---

## Task 3: Core — persistent memory card in `ApplySettings`

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro_settings.cpp` (`ApplySettings`)

- [ ] **Step 1: Switch the memcard type + set the memcards directory**

In `ApplySettings`, find the base-layer setting (from Phase 1):
```cpp
si->SetStringValue("MemoryCards", "Card1Type", "NonPersistent");
```
Replace with a file-backed per-game card:
```cpp
si->SetStringValue("MemoryCards", "Card1Type", "PerGameTitle");
```
Then, near the `EmuFolders` assignments, set the memcards directory from the libretro save directory (which RetroNest sets to `…/emulators/duckstation/psx/`):
```cpp
const char* save_dir = nullptr;
if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir && *save_dir)
  EmuFolders::MemoryCards = Path::Combine(save_dir, "memcards");
```
Place this before the existing `EmuFolders::EnsureFoldersExist();` so the memcards dir is created. `environ_cb` is `ApplySettings`'s parameter; `RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY` is in `libretro.h`; `Path::Combine` is in `common/path.h` (already used for `EmuFolders::Bios`).

- [ ] **Step 2: Verify the memcard-type name string**

Confirm `"PerGameTitle"` is the exact name `Settings::Load` parses for `MemoryCardType::PerGameTitle`:
```bash
cd "$DS"
grep -n "s_memory_card_type_names\|PerGameTitle" src/core/settings.cpp | head
```
If the parsed name differs (e.g. a display vs identifier name), use the exact identifier string from `s_memory_card_type_names`.

- [ ] **Step 3: Build the dylib**

```bash
cmake --build build-arm64 --target duckstation_libretro 2>&1 | tail -5
```
Expected: links clean.

- [ ] **Step 4: Commit (fork master)**

```bash
git add src/duckstation-libretro/libretro_settings.cpp
git commit -m "feat(libretro): persistent PerGameTitle memory card via GET_SAVE_DIRECTORY"
```

---

## Task 4: RetroNest — adapter `pathsDefs()` + `findResumeFile()`

**Files:**
- Modify: `RetroNest-Project/cpp/src/adapters/libretro/duckstation_libretro_adapter.h` (declare two overrides)
- Modify: `RetroNest-Project/cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp` (implement)

- [ ] **Step 1: Declare the overrides in the header**

In `duckstation_libretro_adapter.h`, inside the class (after the existing controller overrides):
```cpp
    QVector<PathDef> pathsDefs() const override;
    QString findResumeFile(const QString& serial) const override;
```
(`PathDef` and `QString` are already visible via `libretro_adapter.h` → `emulator_adapter.h`, as in `pcsx2_libretro_adapter.h`.)

- [ ] **Step 2: Implement in the .cpp**

Add to `duckstation_libretro_adapter.cpp` (mirror `Pcsx2LibretroAdapter`). Add includes at top if not present:
```cpp
#include "core/path_overrides_store.h"
#include "core/paths.h"
#include <QDir>
```
Implementations:
```cpp
QVector<PathDef> DuckStationLibretroAdapter::pathsDefs() const {
    return {
        { "Memory Cards", "libretro", "MemoryCards", "memcards",   PathBase::EmulatorData },
        { "Save States",  "libretro", "SaveStates",  "savestates", PathBase::EmulatorData },
    };
}

// Resume-on-launch: GameSession::terminate writes "<serial>.resume" under the
// DuckStation SaveStates dir; locate it here so GameSession can feed it to
// cfg.resumeStatePath (loaded post-retro_load_game via retro_unserialize).
// Base id "duckstation", systemId "psx". Mirrors Pcsx2LibretroAdapter::findResumeFile.
QString DuckStationLibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    QString dir = PathOverridesStore::instance().read("duckstation", "SaveStates");
    if (dir.isEmpty())
        dir = Paths::emulatorDataDir("duckstation", "psx") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    if (!entries.isEmpty())
        return d.absoluteFilePath(entries.first());
    return {};
}
```

- [ ] **Step 3: Build RetroNest**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build --target RetroNest 2>&1 | tail -6
```
Expected: `[100%] Built target RetroNest` (AUTOMOC already handles the header; the .cpp is already in SOURCES from Phase 1).

- [ ] **Step 4: Commit (feature branch)**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/duckstation_libretro_adapter.h cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp
git commit -m "feat(adapter): DuckStation save-state paths + resume-file lookup"
```

---

## Task 5: Package, deploy, and verify end-to-end

**Files:** none (build + manual verification).

- [ ] **Step 1: Build universal + deploy the core**

```bash
/Users/mark/Documents/Projects/duckstation-libretro/src/duckstation-libretro/package.sh
```
Expected: universal dylib built + deployed to `~/Documents/RetroNest/emulators/libretro/cores/`, resources + Frameworks libs in place. (First run after the flatten is a full build.)

- [ ] **Step 2: Launch RetroNest (user-run, captures log)**

The merged RetroNest binary must be the one built in Task 4. Launch from a terminal:
```bash
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1
```

- [ ] **Step 3: Verify — save-state slot**

In a PS1 game: save to a slot, change game state, load the slot → state is restored (no crash). Confirm `/tmp/rn.log` shows no `retro_serialize`/`retro_unserialize failed` errors.

- [ ] **Step 4: Verify — resume-on-exit/launch**

Quit the game mid-play, then relaunch the same game → it resumes where you left off (not a cold BIOS boot). Confirm a `<serial>.resume` file exists under `~/Documents/RetroNest/emulators/duckstation/psx/savestates/`.

- [ ] **Step 5: Verify — persistent memory card**

Make an in-game save (e.g. Crash's save screen), quit, relaunch cold (don't resume) → the in-game save is present. Confirm a `.mcd` exists under `~/Documents/RetroNest/emulators/duckstation/psx/memcards/`.

- [ ] **Step 6: Regression — clean exit still works**

Quit normally → no crash/assertion (Phase 1 teardown still good).

- [ ] **Step 7: Report results**

If all pass, the feature is complete. If a step fails, capture `/tmp/rn.log` (and any `~/Library/Logs/DiagnosticReports/RetroNest-*.ips`) for diagnosis.

---

## Self-review notes
- **Spec coverage:** §1 save-state serialization → Tasks 1-2; §2 memcards → Task 3 (core) + Task 4 (`pathsDefs`); §3 resume → Task 4 (`findResumeFile`) + RetroNest's existing generic path; done-criteria → Task 5. All four spec "open items" resolved with real values above.
- **Out of scope (unchanged):** `retro_get_memory_data/size` stay no-ops; no rewind/runahead; no SAVE_RAM.
- **Core-touches:** Task 1 adds the fifth intentional fork core modification (`LoadStateDataFromBuffer`), consistent with the prior four; everything else is in the libretro frontend or the RetroNest adapter.

## Finishing
After Task 5 passes: merge `feat/duckstation-libretro-savestates` → `main` and push to the private RetroNest repo (same as Phase 1); the fork's save-state commits stay local-only on `master`. Update the spec with an "Implementation Outcome" note. Use the finishing-a-development-branch skill.
