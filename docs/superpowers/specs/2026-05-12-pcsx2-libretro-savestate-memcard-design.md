# PCSX2 Libretro Core — Save States, Memory Cards, Memory Map & Reset (Sub-project 6)

**Date:** 2026-05-12
**Status:** Design — pending implementation plan
**Owner:** mark
**Scope:** Wires up the four VMManager-state-lifecycle items that share the same architectural seam: in-memory save states, memory-card persistence, `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` for PS2 RetroAchievements, and `retro_reset → VMManager::Reset()`.
**Predecessors:** [SP5.5 — Analog Input & Rumble](2026-05-12-pcsx2-libretro-analog-input-design.md), [SP5 — Input (digital)](2026-05-11-pcsx2-libretro-input-design.md), [SP4 — Audio Output](2026-05-11-pcsx2-libretro-audio-output-design.md), [SP3 — HW render bridge](2026-05-11-pcsx2-libretro-video-bridge-design.md). All complete.

## Context

After SP5.5 the libretro core renders, plays audio, accepts digital + analog input, and rumbles. What it still can't do:

1. **Save state.** `retro_serialize_size`/`retro_serialize`/`retro_unserialize` are stubs returning `0`/`false`/`false` (`pcsx2-libretro/LibretroFrontend.cpp:260-262`). Closing RetroNest with a session in flight loses everything since the last memcard save. RetroNest's `GameSession::terminate → CoreRuntime::requestSaveState` already calls into PCSX2 expecting a working serialize path — it just gets `false` back today.
2. **Memory cards.** Explicitly disabled in `pcsx2-libretro/Settings.cpp:266-274` with the comment "SP6 will wire these properly". PCSX2 boots with no memcard inserted; games that auto-save mid-mission see "memory card not detected" and either refuse to save or crash.
3. **PS2 RetroAchievements.** `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` is never issued by pcsx2-libretro, so RetroNest's already-shipped env handler (`environment_callbacks.cpp:133-163`) captures nothing for this core. `RcheevosRuntime` therefore has no PS2 memory to read — every PS2 cheevo silently fails to trigger.
4. **`retro_reset`.** A logging no-op (`LibretroFrontend.cpp:188-191`). The frontend's hardware-reset path (and RA hardcore-reset event 14, which `RcheevosRuntime` already invokes via `g_syms->retro_reset()`) doesn't reset the VM — it just logs and continues with the existing state.

The RetroNest side is **fully wired already**. SP3/SP3.5 shipped:

- `CoreRuntime::saveState(path)` synchronous path for stop-then-write callers (`core_runtime.cpp:209-222`).
- `CoreRuntime::requestSaveState(path)` / `flushPendingSaveState` async path running between frames on the worker thread (`core_runtime.cpp:224-241`).
- `CoreRuntime::requestLoadState(path)` / `flushPendingLoadState` mirror (`core_runtime.cpp:243-257`).
- `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` env handler that captures descriptors + addrspaces with proper lifetime management (`environment_callbacks.cpp:133-163`, `environment_callbacks.h:32-39`).
- `retro_reset` loaded as a required symbol (`core_loader.cpp:40`), already invoked by `RcheevosRuntime` on hardcore reset (`rcheevos_runtime.cpp:189-190`).

So SP6 is **entirely a pcsx2-libretro-side sub-project.** Zero RetroNest source changes anticipated.

PCSX2 exposes the primitives we need:

- **In-memory save state.** `pcsx2/SaveState.h:338,350` defines `memSavingState` / `memLoadingState` over `VmStateBuffer = std::vector<u8>`. `SaveState_DownloadState` (`SaveState.cpp:713`) shows the canonical usage: construct, call `FreezeBios()` + `FreezeInternals(&err)`, the buffer holds the raw uncompressed state. No zip step, no disk I/O.
- **EE RAM pointer.** `eeMem->Main` is the 32 MB region (`MemoryTypes.h:70` + `Ps2MemSize::MainRam = _32mb`).
- **VM reset.** `VMManager::Reset()` is internally safe to call from a non-VM thread while running — it flips state to `Resetting` and returns immediately; the EE thread does the actual reset at its next event-test boundary (`VMManager.cpp:1762-1814`).
- **Memcard config keys.** `EmuFolders::MemoryCards` (directory) + `MemoryCards/Slot{N}_Enable` (bool) + `Slot{N}_Filename` (relative path) + `Slot{N}_Type` (enum). PCSX2 auto-creates the file on first save.

## Goal

R&C 2 (and arbitrary PS2 games) in RetroNest:

- Quit-and-resume works without a memcard save: closing the session writes a state, reopening restores it (the SP3/3.5 `GameSession::terminate` path becomes functional).
- The libretro frontend's save-state slots (or RetroNest UI-level "Save State"/"Load State" actions) round-trip cleanly mid-session.
- The game can save to memory card slot 1 and a subsequent session sees the save in the in-game load list.
- Memory-card files persist per-game (each game keeps its own memcard image, scoped via the libretro `save_dir` mechanism).
- `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` is issued once on successful load. RetroNest's `RcheevosRuntime` reads `EnvironmentContext.memoryMap` and PS2 cheevos start triggering against EE main RAM.
- Hardware reset (frontend-driven or RA hardcore-mode-enable event 14) returns the VM to BIOS / fast-boot startup state without restarting the libretro core.

## Out of scope

- **Folder memcards.** PCSX2 has a "folder memcard" type that exposes save files as a filesystem instead of a fixed-size image, supporting unlimited saves per card. Defer: file-image cards match libretro convention (one blob, cloud-syncable) and avoid PCSX2 internal complexity.
- **Slot 2 memcard.** Single-slot is the sensible libretro default; the SP6 code leaves slot 2 disabled. Future SP7 settings UI can expose a toggle.
- **IOP RAM / Scratchpad / VU RAM in `SET_MEMORY_MAPS`.** EE main RAM unlocks ~all existing PS2 cheevos. The other regions can be added incrementally if/when specific cheevos need them.
- **Save-state thumbnails.** Libretro has an optional thumbnail API; not in scope.
- **`RETRO_ENVIRONMENT_SET_SAVE_STATE_INFO_OVERRIDE` for variable-size states.** SP6 uses probe-once → fixed-size. The override is only needed if probe-once turns out unstable, which the design assumes it isn't.
- **Cross-frontend save compatibility (`.p2s` interchange).** Libretro saves are raw uncompressed memSavingState bytes; PCSX2-Qt `.p2s` is a zipped ArchiveEntryList. Saves are not interchangeable across frontends — acceptable per the libretro convention.
- **Save state during BIOS (pre-Running).** `retro_serialize_size` returns 0 until VM is Running; spec-legal and the frontend retries.
- **Tuning compressed save-state size on disk.** RetroNest stores whatever libretro hands us; SP6 doesn't change that.
- **`retro_get_region` / `av_info` fps correction.** Tracked separately under SP7's region detection follow-up. SP6 doesn't touch them.

## Architecture & components

All changes are inside `pcsx2-libretro/`. **No upstream PCSX2 files touched.** Rebase discipline preserved (the only upstream-edit exceptions remain SP4's `AudioStreamTypes.h`/`AudioStream.cpp` and SP5's `InputManager.{h,cpp}` — SP6 introduces no new ones).

### `pcsx2-libretro/LibretroFrontend.cpp`

- **`retro_reset`** (current stub at line 188-191): replaced with a one-line `if (VMManager::HasValidVM()) VMManager::Reset();`. Internal-state transitions handled by VMManager.
- **`retro_serialize_size`** (current stub at line 260): probe-once strategy. A file-scope `static std::atomic<size_t> g_serialize_size{0};` is set on first successful call while VM is Running. Returns 0 pre-Running; returns the cached size otherwise. The probe runs `memSavingState` against a scratch `VmStateBuffer` once and records `buffer.size()`. If a subsequent real serialize produces a larger buffer (shouldn't happen — raw state is deterministic per build+ELF), we log a warning and return false from the affected `retro_serialize` call.
- **`retro_serialize(void* dst, size_t len)`** (current stub at line 261): if `g_serialize_size == 0` or `len < g_serialize_size`, returns false. Otherwise pauses the VM via the path described below, constructs a local `VmStateBuffer`, runs `memSavingState s(buf); s.FreezeBios(); s.FreezeInternals(&err);`, resumes the VM, copies the buffer into `dst`, zero-pads any tail (`buf.size() < len`), returns true on success.
- **`retro_unserialize(const void* src, size_t len)`** (current stub at line 262): if `!VMManager::HasValidVM()` returns false. Otherwise pauses the VM, copies `[src, src+len)` into a `VmStateBuffer`, runs `memLoadingState s(buf); s.FreezeBios(); s.FreezeInternals(&err);`, resumes the VM. Returns `!err`. Note: a failed unserialize leaves the VM in a possibly-corrupt state — the spec contract is that the frontend's expected response is to load a different state or reset; we don't tear down.
- **`retro_run` first-frame-Running gate**: `SET_MEMORY_MAPS` is issued from inside `retro_run` on the first iteration where `VMManager::GetState() == VMState::Running`, gated by a `static std::atomic<bool> g_memory_map_issued{false};` that is reset on `retro_unload_game`. We can't issue from `retro_load_game` end-of-success because `eeMem->Main` isn't guaranteed allocated until the VM has begun executing (PCSX2 lazily allocates the EE memory block during boot). The "first frame Running" gate co-locates with the existing one-shot `g_logged_running` block at line 199-209.

### `pcsx2-libretro/Settings.cpp`

- **`InitializeDefaults`** — replace the SP6-deferred disable block at lines 266-274 with the real wiring:
  - `EmuFolders::MemoryCards` directory set from libretro `save_dir` (queried via `RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY`) joined with a `memcards/` subdirectory. The libretro adapter on RetroNest's side puts each game's `save_dir` under `{game_id}/`, giving per-game isolation for free.
  - `MemoryCards/Slot1_Enable = true`
  - `MemoryCards/Slot1_Filename = "Mcd001.ps2"` (PCSX2 standard 8 MB image; PCSX2 auto-creates on first save)
  - `MemoryCards/Slot1_Type = MemoryCardType::File` (PS2-format 8 MB file image)
  - `MemoryCards/Slot2_Enable = false` (explicitly disabled; UI exposure deferred to SP7)
- Same call site as the existing input/audio settings block; no new initialization order changes.
- During implementation planning, verify that PCSX2's existing memcard plumbing (`pcsx2/SIO/Memcard/MemoryCardFile.cpp`) reads from `EmuFolders::MemoryCards` and `MemoryCards/Slot{N}_*` keys without additional wiring. If a missing seam surfaces, add a minimal Settings.cpp glue function — but not a new upstream-edit exception.

### Pause-stable handshake (TBD — prototype in plan Task 1)

`memSavingState::FreezeInternals` walks deep PCSX2 internal state (EE, IOP, MTGS, SPU2, VU, IOP DMA, ...). Calling it while any of those threads are mid-instruction is undefined behavior. PCSX2-Qt's `VMManager::Internal::DoSaveState` calls it from the EE thread itself at an event-test boundary, which is the natural quiescent point.

From the libretro callback thread (the frontend thread that calls `retro_serialize`), the safe options are:

- **(a)** Issue `VMManager::SetPaused(true)`, wait for `VMManager::GetState() == VMState::Paused`, do the serialize, then `SetPaused(false)`. This is the same handshake PCSX2-Qt uses for its "save state" UI when the game is running.
- **(b)** Post a job onto the EE thread's event-test queue (similar to how `VMManager::Reset()` defers from outside threads via `state = Resetting`).

Plan Task 1 prototypes (a) and verifies that `MTGS::WaitGS()` after `SetPaused` is sufficient to quiesce the GS thread. If (a) is unstable, fall back to (b). The choice affects ~30 lines of LibretroFrontend code but not any other component.

This is the one piece of SP6 that carries real architectural risk. Everything else is mechanical glue.

### `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` descriptor

Single descriptor for EE main RAM:

```c
retro_memory_descriptor desc = {};
desc.ptr       = eeMem->Main;
desc.start     = 0x00000000;           // PS2-physical
desc.len       = Ps2MemSize::MainRam;  // 32 MB
desc.select    = 0;                    // RA infers from start+len
desc.flags     = RETRO_MEMDESC_SYSTEM_RAM;
desc.addrspace = "";                   // unnamed default

retro_memory_map mm = { .descriptors = &desc, .num_descriptors = 1 };
g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mm);
```

The libretro spec requires the FRONTEND to copy the descriptor array + addrspace strings before returning; RetroNest's handler already does this (`environment_callbacks.cpp:146-159`). PCSX2-libretro can pass stack-allocated structs.

## Data flow

### Save (probe-once, then steady state)

```
First retro_serialize_size call while VM.State == Running:
  1. acquire VM pause + wait for Paused state    (handshake above)
  2. VmStateBuffer probe;
  3. memSavingState s(probe);
  4. s.FreezeBios(); s.FreezeInternals(&err);    (if err: resume, return 0)
  5. release VM pause
  6. g_serialize_size = probe.size()             (~32-50 MB depending on ELF)
  7. return g_serialize_size

Subsequent retro_serialize_size: just return g_serialize_size atomic.
Pre-Running: return 0 (spec-legal).

retro_serialize(dst, len):
  if (g_serialize_size == 0 || len < g_serialize_size) return false
  acquire VM pause
  VmStateBuffer buf;
  memSavingState s(buf); s.FreezeBios(); s.FreezeInternals(&err);
  release VM pause
  if (err || buf.size() > len) return false
  memcpy(dst, buf.data(), buf.size())
  if (buf.size() < len) memset(dst + buf.size(), 0, len - buf.size())
  return true
```

### Load

```
retro_unserialize(src, len):
  if (!VMManager::HasValidVM()) return false
  acquire VM pause
  VmStateBuffer buf(src_bytes, src_bytes + len)   // copy
  memLoadingState s(buf); s.FreezeBios(); s.FreezeInternals(&err);
  release VM pause
  return !err
```

### Memory cards

PCSX2's own subsystems handle the lifecycle once Settings.cpp configures the slots:

```
On VM init: MemoryCardFile reads Slot1_Enable, Slot1_Filename
            → opens (or creates) {save_dir}/memcards/Mcd001.ps2
            → registers with SIO/Mcd plumbing

In-game save:
  game ⇒ SIO ⇒ MemoryCardFile ⇒ {save_dir}/memcards/Mcd001.ps2 (write-through)

In-game load (next session):
  same file, game's auto-detect surfaces existing saves.
```

No new code on the data path; just the Settings.cpp configuration.

### Memory map

```
retro_load_game:
  ... VMManager::Initialize ...
  ... boot to Running (existing wait in retro_run) ...

retro_run (first frame after Running):
  if (!g_memory_map_issued && VMManager::GetState() == VMState::Running) {
    retro_memory_descriptor desc = { eeMem->Main, ..., 32MB, ... };
    retro_memory_map mm = { &desc, 1 };
    g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mm);
    g_memory_map_issued = true;
  }
  ...
```

Issued from inside `retro_run` rather than from `retro_load_game` because `eeMem->Main` isn't guaranteed valid until the VM has actually started executing (PCSX2 lazily allocates the EE memory block during boot). One-time gate via a static atomic flag, reset on `retro_unload_game`.

### Reset

```
retro_reset:
  if (VMManager::HasValidVM()) VMManager::Reset();
  // VMManager flips state to Resetting if Running; EE thread does the
  // actual reset at next event-test. If state is Paused at call time,
  // Reset() does the reset inline (also safe from any thread).
```

## Error handling

| Failure | Behavior | Reason |
|---|---|---|
| `retro_serialize_size` called pre-Running | Return 0 | Spec-legal; frontend retries when it next polls |
| `retro_serialize` `len < g_serialize_size` | Return false, no work done | Frontend's allocation bug; we don't truncate |
| `retro_serialize` `FreezeInternals` errors | Resume VM, return false, log warning | Probe was stale or PCSX2 internal failure; frontend tries again next slot |
| `retro_serialize` produces > `g_serialize_size` bytes | Resume VM, return false, log warning at FATAL level | Should not happen — indicates a probe-once assumption violation; warrants investigation |
| `retro_serialize` produces < `g_serialize_size` bytes | Zero-pad the tail, return true | Spec-legal; subsequent `retro_unserialize` ignores the padding |
| `retro_unserialize` `FreezeInternals` errors | Resume VM, return false | VM state is now possibly corrupt; frontend's expected response is to load a different state or `retro_reset`. We don't tear down. |
| `retro_unserialize` called pre-VM | Return false | No VM to load into |
| memcard file I/O failure | PCSX2 existing handling (in-game "memcard not detected") | We don't add new error paths; matches PCSX2-Qt behavior |
| `SET_MEMORY_MAPS` env_cb returns false | Log warning, continue | RA won't work but game runs; not a fatal condition |
| `retro_reset` with no valid VM | No-op | Frontend may call reset before/after a load_game in edge cases |

## Testing

**Unit-testable:** none — all changes are PCSX2-side glue without a unit-test target in this fork (mirrors SP4/SP5/SP5.5 precedent).

**Manual smoke tests** (in the SP5.5 Task 5 cadence; run after each task lands):

1. **Boot persistence.** Launch R&C 2 → progress past intro → close RetroNest cleanly → reopen → game resumes at the same point. (Validates the SP3 quit-resume path now that `requestSaveState` returns useful data.)
2. **Round-trip save/load mid-session.** In R&C 2: save to a libretro slot, advance gameplay, load that slot → verify position/inventory match the save point. No audio/video desync, no crash.
3. **`retro_reset`.** Trigger via the frontend's hardware-reset UI (or RetroAchievements hardcore mode toggle). Verify the VM returns to BIOS / fast-boot startup screen without restarting the libretro core.
4. **Memory card persistence.** In R&C 2: save in-game to memory card slot 1. Close session. Reopen → verify the in-game save list shows the entry, and loading the save resumes correctly.
5. **PS2 RetroAchievements.** Enable RA on R&C 2 → confirm the "RA: 0 of N achievements unlocked" toast appears (proves the memory map reached RA). Trigger a known cheevo (e.g. early enemy kill count) → verify unlock toast.

**Diagnostic instrumentation:** new env-gated `RETRONEST_STATE_TRACE=1` (mirrors `RETRONEST_AUDIO_TRACE` / `RETRONEST_INPUT_TRACE`). Zero overhead when unset (`std::getenv` once, cached `bool`). Logs at six boundaries:

- `retro_serialize_size` probe entry/exit (with measured size, elapsed ms)
- `retro_serialize` entry/exit (with len, elapsed ms, success/fail)
- `retro_unserialize` entry/exit (with len, elapsed ms, success/fail)
- `retro_reset` invocation (with current VMState)
- `SET_MEMORY_MAPS` issue (with ptr, len, env_cb return)
- pause-stable handshake transitions (acquire/release, elapsed)

**Performance budget:** `retro_serialize` should complete within one host frame (16 ms) to avoid visible hitches. 32-50 MB raw `memcpy` is sub-ms; the bottleneck is `FreezeInternals` walking PCSX2 internal state. Measured during plan Task 1's probe-once prototype; if it consistently exceeds budget, consider running the serialize on a worker thread inside `retro_serialize` (still synchronous to the caller, but parallel to the next frame's video — discuss with user before implementing).

## References

- **PCSX2 in-memory state primitives:**
  - `pcsx2/SaveState.h:338,350` — `memSavingState` / `memLoadingState` class declarations
  - `pcsx2/SaveState.cpp:296-323` — implementations (just `FreezeMem` over the vector)
  - `pcsx2/SaveState.cpp:713-735` — `SaveState_DownloadState` (canonical usage pattern; SP6 replicates this minus the zip)
  - `pcsx2/SaveState.cpp:1165-1175` — load-from-buffer reference (`memLoadingState` then `FreezeInternals`)
- **PCSX2 memory & reset:**
  - `pcsx2/MemoryTypes.h:7-27,70` — `Ps2MemSize::MainRam` (32 MB) + `extern eeMem` declaration
  - `pcsx2/VMManager.cpp:1762-1814` — `VMManager::Reset()` (thread-safe via state flip)
- **Current pcsx2-libretro stubs:**
  - `pcsx2-libretro/LibretroFrontend.cpp:188-191` — `retro_reset` logging stub
  - `pcsx2-libretro/LibretroFrontend.cpp:260-262` — serialize stubs
  - `pcsx2-libretro/Settings.cpp:266-274` — disabled memcard block
- **RetroNest-side (already wired, no changes needed):**
  - `RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp:209-257` — save/load state plumbing
  - `RetroNest-Project/cpp/src/core/libretro/environment_callbacks.cpp:133-163` — `SET_MEMORY_MAPS` handler
  - `RetroNest-Project/cpp/src/core/libretro/rcheevos_runtime.cpp:189-190` — `retro_reset` invocation on RA hardcore reset
  - `RetroNest-Project/cpp/src/core/libretro/core_loader.cpp:40` — `retro_reset` as a required symbol
- **Libretro spec:**
  - `pcsx2-libretro/pcsx2-libretro/libretro.h:7752-7775` — `retro_serialize_size`/`retro_serialize`/`retro_unserialize` contracts
  - `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` (enum 36): the frontend must copy descriptors + addrspaces before returning
- **Predecessor specs / plans for cadence reference:**
  - `2026-05-12-pcsx2-libretro-analog-input-design.md` (SP5.5 — same shape, similar size)
  - `2026-05-11-pcsx2-libretro-audio-output-design.md` (SP4 — env-gated trace pattern)
