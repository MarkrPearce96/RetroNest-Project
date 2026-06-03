# DuckStation libretro — save states + memory cards (design spec)

**Date:** 2026-06-03
**Status:** Approved design, pending implementation plan.
**Follows:** `2026-06-01-duckstation-libretro-skeleton-design.md` (Phase 1 skeleton — complete). This is the first follow-on feature spec.
**Companion:** delta report §5 (save state) + §6 (BIOS/memcard) — `duckstation-libretro/docs/swanstation-delta-2026-06-01.md`.

## Goal (one line)
Make PS1 progress persist in RetroNest's DuckStation libretro core: working **save-state slots** + **resume-on-launch/exit**, and **persistent memory cards** (in-game saves survive across sessions), matching how RetroNest's other libretro cores behave.

## Scope

**In scope:**
1. Save-state serialization: real `retro_serialize_size` / `retro_serialize` / `retro_unserialize` (currently stubs returning 0/false).
2. A small fork core addition: `System::LoadStateDataFromBuffer` (there is no public load-from-buffer).
3. Resume-on-launch/exit via RetroNest's existing generic plumbing + a `findResumeFile()` adapter override.
4. Persistent memory cards: file-backed memcard written by DuckStation into a RetroNest-managed per-game-system folder, exposed/overridable via the adapter.

**Out of scope (deferred to later specs):**
- Rewind / runahead memory-state fast path (DuckStation's `MemorySaveState` with GPU-texture handling — delta §5 flags this as the most involved piece).
- `RETRO_MEMORY_SAVE_RAM` exposure (`retro_get_memory_data/size` stay no-ops — DuckStation persists the memcard file itself).
- RetroAchievements hardcore-mode save-state restrictions.
- Cross-build / cross-version save-state compatibility (same-binary contract only).

## Context (why most of this is small)

**RetroNest's libretro save plumbing is already generic** (powers PCSX2/PPSSPP/Dolphin), in `cpp/src/core/libretro/core_runtime.cpp`:
- `saveState`/`requestSaveState`/`flushPendingSaveState` → `retro_serialize_size()` then `retro_serialize(buf, n)`; thread-safe via request/flush on the core thread.
- `requestLoadState`/`flushPendingLoadState` → `retro_unserialize(buf, size)`.
- Resume-on-launch: `GameSession` sets `cfg.resumeStatePath = adapter->findResumeFile(serial)` (`game_session.cpp:251`); after `retro_load_game`, if a resume path is set and the boot-state-path env wasn't consumed, `CoreRuntime` calls `retro_unserialize` on it (`core_runtime.cpp:457-461`).
- Resume-on-exit: `GameSession::terminate` → `requestSaveState(<serial>.resume)` into the SaveStates dir (`game_session.cpp:471-479`).

So the core side is just the stub `retro_serialize*` functions, and the only new RetroNest code is a `findResumeFile()` override.

**DuckStation save API (current, from delta §5), in `duckstation-libretro/src/core/`:**
- `size_t System::GetMaxSaveStateSize(bool enable_8mb_ram)` (`system.h:283`).
- `bool System::SaveStateDataToBuffer(std::span<u8> data, size_t* out_size, Error*)` (`system.h:299`) — writes the raw `DoState` stream (no `SAVE_STATE_HEADER`).
- **No public load-from-buffer** — `LoadStateFromBuffer`/`DoState`/`SaveStateBuffer` are file-`static` in `system.cpp`; public `LoadState` only takes a path.
- `StateWrapper` is span-based (`util/state_wrapper.h`); `SAVE_STATE_VERSION` in `core/save_state_version.h`.
- Memcards (delta §6): `memory_card_types[]` in `Settings`; `MemoryCardType` enum `{None, Shared, PerGame, PerGameTitle, PerGameFileTitle, NonPersistent}`; `System::UpdateMemoryCards()` creates them from settings + `EmuFolders::MemoryCards`.

## Design

### 1. Save-state serialization (core)
In `src/duckstation-libretro/libretro.cpp`, replace the stubs:
- `retro_serialize_size()` → return `System::GetMaxSaveStateSize(/*enable_8mb_ram=*/true)` — the **8 MB-RAM worst case, returned consistently** so the buffer is always large enough regardless of the runtime RAM setting and stable across the session. (RetroNest re-queries size before each serialize, but a stable worst-case size is the safe libretro contract.)
- `retro_serialize(data, size)` → `std::span<u8> sp{static_cast<u8*>(data), size}; size_t written; return System::SaveStateDataToBuffer(sp, &written, &error);`
- `retro_unserialize(data, size)` → `return System::LoadStateDataFromBuffer(std::span<const u8>{static_cast<const u8*>(data), size}, &error);` (new function below).

**Fork core-touch — `System::LoadStateDataFromBuffer`** (the fifth intentional core modification, consistent with the prior four): add a public function in `core/system.h` + `core/system.cpp` that mirrors the existing file-private raw load path:
```cpp
bool System::LoadStateDataFromBuffer(std::span<const u8> data, Error* error)
{
  StateWrapper sw(data, StateWrapper::Mode::Read, SAVE_STATE_VERSION);
  return DoState(sw, /* … same args the static LoadStateFromBuffer path passes … */);
}
```
Confirm the exact `DoState` signature/args against `system.cpp`'s existing static load (delta §5 cites lines ~2880-2885). Same-binary contract: version is assumed `SAVE_STATE_VERSION` (RetroNest always loads states written by the same build). Headerless, matching `SaveStateDataToBuffer`'s raw write.

### 2. Persistent memory cards (core + adapter)
- **Core (`ApplySettings`, `libretro_settings.cpp`):** change the skeleton's `MemoryCards/Card1Type=NonPersistent` to a **file-backed type — `PerGameTitle`** (DuckStation's default; per-game cards, no cross-game clutter). Set `EmuFolders::MemoryCards` to a persistent, RetroNest-managed directory.
- **Memcards directory source:** per RetroNest's folder convention (`emulators/{emuId}/{systemId}/memcards/`), the card lives at `…/emulators/duckstation/psx/memcards/`, user-overridable. The core obtains this path from RetroNest — **confirm the mechanism in the plan**: likely `RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY` (PCSX2's frontend uses it), or RetroNest's per-emulator path plumbing; if neither yields the per-game-system memcards path directly, derive it from the known root + `EmuFolders` and document the choice.
- **`retro_get_memory_data/size`** stay no-ops — DuckStation owns and persists the `.mcd`.
- **Adapter (`DuckStationLibretroAdapter`):** add a `pathsDefs()` override exposing **MemoryCards** and **SaveStates** as user-overridable folders (mirror `Pcsx2LibretroAdapter::pathsDefs()`), so the Paths UI lists them and `PathOverridesStore` can relocate them.

### 3. Resume + adapter (`findResumeFile`)
- **Manual slots** and **resume-on-exit** work automatically once §1 lands (RetroNest's generic save/load path + `GameSession::terminate`).
- **Resume-on-launch — one new override:** `DuckStationLibretroAdapter::findResumeFile(const QString& serial)` (mirror `Pcsx2LibretroAdapter::findResumeFile`) locates `<serial>.resume` under the DuckStation SaveStates dir (`PathOverridesStore::read("duckstation","SaveStates")`, else default `Paths::emulatorDataDir("duckstation","psx") + "/savestates"`). `GameSession` feeds the result to `cfg.resumeStatePath`; `CoreRuntime` loads it post-`retro_load_game` via `retro_unserialize`.
- **Resume mechanism decision:** use the **post-load `retro_unserialize`** path (zero extra core env code; loading a DuckStation state right after `BootSystem` is well-defined). The boot-state-path private env callback (`RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH`, PCSX2's model) is a documented fallback if post-load ordering misbehaves.
- **Serial format:** confirm in the plan that the serial RetroNest passes to `findResumeFile`/uses for the `.resume` name matches DuckStation's serial (CLAUDE.md notes per-emulator conversion like `SCUS_949.00 → SCUS-94900`).

## Files touched

**Fork (`duckstation-libretro/`, `master`, local-only):**
| File | Change |
|---|---|
| `src/core/system.h` + `src/core/system.cpp` | Add public `System::LoadStateDataFromBuffer(std::span<const u8>, Error*)` (core-touch). |
| `src/duckstation-libretro/libretro.cpp` | Implement `retro_serialize_size`/`retro_serialize`/`retro_unserialize`. |
| `src/duckstation-libretro/libretro_settings.cpp` | Memcard type `PerGameTitle` + `EmuFolders::MemoryCards` path. |

**RetroNest (`feat/...savestates` branch off `main`):**
| File | Change |
|---|---|
| `cpp/src/adapters/libretro/duckstation_libretro_adapter.{h,cpp}` | Add `findResumeFile()` + `pathsDefs()` (MemoryCards, SaveStates) overrides. |

## Open items to confirm during planning
1. Exact `DoState(...)` argument list for `LoadStateDataFromBuffer` (read `system.cpp`'s static load path).
2. Memcards directory source: `GET_SAVE_DIRECTORY` vs RetroNest per-emulator path plumbing; ensure it resolves to `emulators/duckstation/psx/memcards/`.
3. Serial-format match between RetroNest's `findResumeFile` key and the `.resume` filename / DuckStation serial.
4. Whether `SaveStateDataToBuffer` requires the system to be in a particular state (e.g. not mid-`RunFrame`) — RetroNest already gates save/load via request/flush on the core thread, so confirm that satisfies it.

## Testing / done criteria
Through RetroNest, with a PS1 game:
1. **Save-state slot:** save to a slot mid-game, change something, load the slot → state restored.
2. **Resume-on-exit/launch:** quit mid-game, relaunch the same game → resumes where you left off (no cold BIOS boot).
3. **Memory card:** save in-game (e.g. Crash's save screen), quit, relaunch cold → the in-game save is present; the `.mcd` exists under `emulators/duckstation/psx/memcards/`.
4. Clean exit/re-load still works (no regression to Phase 1).
5. Unit-testable pure logic, if any emerges (e.g. serial normalization) gets a test; most of this is integration verified manually like Phase 1.

## Deferred (future specs)
Rewind/runahead memory-state fast path; `SAVE_RAM` exposure; RA hardcore save-state rules; cross-version state compatibility; multi-slot UI polish.

## References
- Skeleton spec + outcome: `docs/superpowers/specs/2026-06-01-duckstation-libretro-skeleton-design.md`
- Delta §5 (save state) / §6 (memcard): `duckstation-libretro/docs/swanstation-delta-2026-06-01.md`
- RetroNest save plumbing: `cpp/src/core/libretro/core_runtime.cpp` (`saveState`/`requestLoadState`/resume), `cpp/src/core/game_session.cpp:251,471-479`
- Reference adapter: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` (`findResumeFile`, `pathsDefs`)
- DuckStation save API: `src/core/system.h` (`GetMaxSaveStateSize`, `SaveStateDataToBuffer`), `src/util/state_wrapper.h`, `src/core/save_state_version.h`
