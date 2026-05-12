# PCSX2 Libretro Core — Save State (Sub-project 6.5)

**Date:** 2026-05-12
**Status:** Design — implementation pending
**Owner:** mark
**Scope:** Implements `retro_serialize_size` / `retro_serialize` / `retro_unserialize` for the PCSX2 libretro core by wrapping PCSX2's canonical `SaveState_DownloadState` path in an **in-memory libzip container**. Forces uncompressed entries (`ZIP_CM_STORE`) for a deterministic, probe-once buffer size. No upstream PCSX2 file changes. No RetroNest source changes.
**Predecessors:** [SP6 — Memory Cards, Memory Map & Reset](2026-05-12-pcsx2-libretro-savestate-memcard-design.md). SP6 shipped and verified end-to-end on R&C 2; save state was the deferred fourth task.

## Context

SP6 shipped `retro_reset`, memory cards, and `SET_MEMORY_MAPS`. The fourth SP6 task — wiring save state via `memSavingState` / `memLoadingState` directly — was implemented (commit `2eddc63de`) and reverted (`f164c4f5f`) when code review caught a missing canonical step. The reverted code called only `FreezeBios()` + `FreezeInternals()`. The full PCSX2 save path (`pcsx2/SaveState.cpp:713-755`) also iterates a `SavestateEntries[]` table of 14 components — EE main RAM (`SavestateEntry_EmotionMemory`), IOP RAM, HW registers, IOP HW registers, Scratchpad, VU0/VU1 mem + program, SPU2, USB, PAD, GS, and Achievements — calling `entry->FreezeOut(saveme)` for each. Without that loop, saves contain BIOS + a handful of internal PCSX2 registers and **none of the actual game state**; round-trips would appear to succeed but the game would not resume.

The symmetric load is **fundamentally zip-coupled**: `BaseSavestateEntry::FreezeIn(zip_file_t*)` (`SaveState.cpp:469`) takes a libzip file handle, not a `SaveStateBase&`. Per-entry data is read with `zip_fread`; there is no flat-buffer `FreezeIn` equivalent. Any save-state implementation that uses the canonical `SavestateEntries[]` path on the way in is forced into one of:

- **(A) In-memory libzip.** Wrap a `std::vector<u8>` behind libzip's `zip_source_function` callback API. Build the zip in memory using the same `SaveState_DownloadState` → `ArchiveEntryList` flow as PCSX2-Qt; force `ZIP_CM_STORE` to keep output deterministic. Load: open a `zip_t*` from the caller's read-only buffer and reuse a forked copy of `SaveState_UnzipFromDisk`'s logic that accepts a `zip_t*` instead of a filename.
- **(B) Disk-temp-file.** Write the zip to `EmuFolders::Cache + "/libretro_state.tmp"` via `SaveState_ZipToDisk`, read bytes back; reverse with `SaveState_UnzipFromDisk`. ~30 lines, but adds ~50–100 ms of disk I/O per save/load on SSD and requires (1) wiring `EmuFolders::Cache` in `pcsx2-libretro/Settings.cpp` and (2) a new env-handler in RetroNest for `RETRO_ENVIRONMENT_SET_SAVE_STATE_INFO_OVERRIDE` to deal with compressed/variable size.

**SP6.5 adopts Option A.** Driving reasons:

1. **Run-ahead and netplay are first-class libretro use cases** that call `retro_serialize` every frame. 50–100 ms of disk I/O per call makes those features unusable; in-memory keeps the door open even though SP6.5 doesn't ship either feature today.
2. **Stays inside `pcsx2-libretro/`.** Option B grows scope into both `Settings.cpp` (Cache folder) and RetroNest (`RETRO_ENVIRONMENT_SET_SAVE_STATE_INFO_OVERRIDE` env handler, currently unhandled).
3. **The libzip-in-memory pattern is already in this file.** `SaveState_CompressScreenshot` (`SaveState.cpp:778-839`) uses `zip_source_buffer_create` + `zip_source_write` to write a zip to memory. SP6.5's source wrapper is the same idea, slightly generalized for write+read+seek.
4. **Probe-once is deterministic by construction.** With `ZIP_CM_STORE` everywhere, the output size is `sum(entry_bytes) + sum(local_file_headers) + central_directory + EOCD` — fixed per (build, game). No "worst-case bound + pad" workaround needed.

What carries over from the reverted commit `2eddc63de` (verified by code review at the time):

- `WaitForVmPaused` / `ResumeVm` helpers (200 ms ceiling, 1 ms polling, mirrors PCSX2-Qt's "save while running" handshake).
- The probe-once atomic + reset-on-`retro_unload_game` pattern.
- Five of six `RETRONEST_STATE_TRACE` boundaries (the `retro_reset` boundary already shipped in SP6 Task 5 / `803262791`).

All of those are lift-and-shift. SP6.5's new code is the libzip plumbing and the per-entry zip-walk on load.

## Goal

R&C 2 (and arbitrary PS2 games) in RetroNest:

- **Mid-session round-trip:** save state during gameplay, advance 30 s, load state — character position, inventory, audio context, and rendering all snap back. No crash, no audio desync that doesn't recover within 1 s.
- **Quit-resume:** RetroNest's `GameSession::terminate → CoreRuntime::requestSaveState` writes a working state on close; reopen restores it via the existing `findResumeFile` path. (SP3 wired this; SP6.5 makes it functional for PCSX2.)
- **Frontend save slots:** `retro_serialize_size` returns a stable non-zero value once VM is Running; saves/loads through that interface round-trip cleanly.
- **Determinism observable:** probe-once size logged on first call; a `RETRONEST_STATE_TRACE` boundary confirms the size is unchanged across at least 5 subsequent saves.

## Out of scope

- **Run-ahead and netplay enablement.** Option A makes the latency profile compatible with these, but SP6.5 doesn't ship either. They're separate libretro frontend features and would need their own brainstorm (e.g. is one EmuThread instance enough? Determinism guarantees?).
- **Save-state thumbnails.** Libretro has an optional thumbnail API; not in scope.
- **Cross-frontend `.p2s` interchange.** PCSX2-Qt's `.p2s` is a zipped ArchiveEntryList with version header + screenshot + user compression. SP6.5's output is the same conceptually but with forced `ZIP_CM_STORE` and no screenshot — readable by `SaveState_UnzipFromDisk` if dumped to disk, but cross-frontend interchange is not a goal and isn't tested.
- **Save state during BIOS (pre-Running).** `retro_serialize_size` returns 0 until VM is Running. Spec-legal; the frontend retries on next call.
- **`SET_SAVE_STATE_INFO_OVERRIDE` env in RetroNest.** Option A doesn't need it. Adding it would only be necessary if a future core picks Option B-shape semantics; that's tracked separately.
- **Compression of the saved blob.** With `ZIP_CM_STORE` our wire format is roughly the same size as raw memory. A future setting could allow `ZIP_CM_ZSTD` for disk-stored saves at the cost of probe-once determinism, but SP6.5 keeps it simple.
- **Save-state size cap.** PCSX2's `SaveState_DownloadState` pre-allocates 64 MB into `destlist->GetBuffer()` (`SaveState.cpp:716`). Final size is typically smaller. SP6.5 inherits that bound; if a future PCSX2 update grows past 64 MB, that's an upstream change we'll inherit and react to.
- **Achievements hardcore-mode rejection of save state.** Hardcore is enforced by `RcheevosRuntime` on the RetroNest side; SP6.5 doesn't gate `retro_serialize`/`retro_unserialize` itself.

## Architecture & components

All changes live in `pcsx2-libretro/`. **No upstream PCSX2 files touched.** No RetroNest source changes. Rebase discipline preserved: the only upstream-edit exceptions remain SP4's `AudioStreamTypes.h`/`AudioStream.cpp` and SP5's `InputManager.{h,cpp}`. SP6.5 introduces no new ones.

### New files

- **`pcsx2-libretro/LibretroSaveState.h`** — declares `Pcsx2Libretro::SaveStateProbeSize()`, `SerializeToBuffer(void* dst, size_t len)`, `DeserializeFromBuffer(const void* src, size_t len)`, plus the existing `WaitForVmPaused` / `ResumeVm` handshake helpers (lifted from `LibretroFrontend.cpp` in the reverted commit so they're reusable and the frontend file doesn't grow by 250 lines).
- **`pcsx2-libretro/LibretroSaveState.cpp`** — the implementation: `MemoryZipSink` source-function callback, the save/load orchestration, and a forked-from-`SaveState_UnzipFromDisk` zip-walk that accepts a `zip_t*`.

### Modified files

- **`pcsx2-libretro/LibretroFrontend.cpp`** — three retro_* stubs become one-line delegates to `LibretroSaveState.{h,cpp}`. The `g_serialize_size` atomic moves to `LibretroSaveState.cpp` (still reset on `retro_unload_game` via a small `ResetSerializeSizeCache()` helper).
- **`pcsx2-libretro/CMakeLists.txt`** — add the two new files to the target sources.

### `MemoryZipSink` (the libzip source-function wrapper)

A `zip_source_function` callback dispatches on opcode and operates over a `std::vector<u8>` + cursor position. Two construction modes:

- **Write mode**: starts with an empty `std::vector<u8>`, grows on `ZIP_SOURCE_WRITE` / `ZIP_SOURCE_BEGIN_WRITE`. Used by serialize and probe.
- **Read mode**: takes a `const void* src, size_t len` and exposes that buffer read-only. Used by deserialize.

The callback handles the opcodes libzip needs: `OPEN`, `READ`, `WRITE`, `CLOSE`, `STAT`, `TELL`, `SEEK`, `ERROR`, `FREE`, `SUPPORTS`, `BEGIN_WRITE`, `COMMIT_WRITE`, `ROLLBACK_WRITE`, `TELL_WRITE`, `SEEK_WRITE`. The standard pattern is documented under `zip_source_function(3)`; the file `pcsx2/SaveState.cpp:778-839` shows the lighter `zip_source_buffer_create` pattern (sufficient for write-only-then-finalize, which is what serialize needs). The deserialize path needs read-with-seek, which `zip_source_function` provides cleanly.

**Implementation budget:** ~80 lines for the callback + struct + two factory helpers (`MakeWriteSink()`, `MakeReadSource(const void*, size_t)`).

### `retro_serialize_size` flow

```
if cached_size != 0: return cached_size
if !HasValidVM || state != Running: return 0
prev = WaitForVmPaused()
if prev == Shutdown: return 0
{
    Error err
    auto srclist = SaveState_DownloadState(&err)
    if !srclist: log + skip
    else:
        MemoryZipSink sink            // write mode
        zip_t* zf = zip_open_from_source(sink.AsSource(), ZIP_CREATE | ZIP_TRUNCATE, &ze)
        for entry in srclist->Entries():
            zip_source_t* zs = zip_source_buffer(zf, srclist.GetBufferData() + entry.DataIndex, entry.DataSize, 0)
            s64 fi = zip_file_add(zf, entry.GetFilename(), zs, ZIP_FL_ENC_UTF_8)
            zip_set_file_compression(zf, fi, ZIP_CM_STORE, 0)
        // also write the StateVersion entry (mirrors SaveState_AddToZip's version block)
        zip_close(zf)                   // finalizes into sink.bytes
        probed = sink.bytes.size()
}
ResumeVm(prev)
if probed != 0: cached_size = probed; log; return probed
return 0
```

The StateVersion entry (a `{u32 save_version, char version[STATE_PCSX2_VERSION_SIZE]}` struct, identical to what `SaveState_AddToZip` writes at `SaveState.cpp:976-1010`) is included so loads can run the same `CheckVersion` shape used by `SaveState_UnzipFromDisk`. **Cross-frontend interchange isn't a goal**, but the version-check defends against accidentally loading a save written by a different PCSX2 build — exactly the case the existing `CheckVersion` (`SaveState.cpp:1094-1131`) was written for.

### `retro_serialize` flow

Identical to the probe flow above, except: after `zip_close`, `memcpy(dst, sink.bytes.data(), sink.bytes.size())`. If `sink.bytes.size() > len`, return false with a loud `RETRO_LOG_ERROR` (probe-once assumption violated; should not happen). If `sink.bytes.size() < len`, zero-pad the tail. Return true.

### `retro_unserialize` flow

```
if !src || len == 0 || !HasValidVM: return false
prev = WaitForVmPaused()
if prev == Shutdown: return false
{
    MemoryZipSink source                       // read mode over src/len
    zip_t* zf = zip_open_from_source(source.AsSource(), ZIP_RDONLY, &ze)
    if !zf: log + bail
    else:
        ok = UnzipFromMemory(zf, &err)         // see below
        if !ok: VMManager::Reset()             // mirror SaveState.cpp:1239
        zip_close(zf)
}
ResumeVm(prev)
return ok
```

### `UnzipFromMemory(zip_t* zf, Error* error)`

A forked copy of `SaveState_UnzipFromDisk` (`SaveState.cpp:1175-1246`), modified only at the entry point: takes a `zip_t*` directly instead of opening one from a filename. Everything downstream — `CheckVersion`, `CheckFileExistsInState`, `LoadInternalStructuresState`, the `SavestateEntries[]` `FreezeIn(zip_file_t*)` loop, `PreLoadPrep` / `PostLoadPrep`, the failure → `VMManager::Reset()` path — is identical. ~50 lines of fork.

**Why fork rather than refactor upstream:** SP6.5 must not touch upstream files outside the existing 4-line block in top-level `CMakeLists.txt`. The fork is one function and lives entirely in `pcsx2-libretro/LibretroSaveState.cpp`. If a future PCSX2 update changes the load semantics, monthly rebase will surface the divergence as a manual sync; that's acceptable for one function. Cleanly upstreaming a `zip_t*`-taking variant would be a separate PR to upstream PCSX2 — out of scope.

`CheckVersion`, `CheckFileExistsInState`, `LoadInternalStructuresState`, `PreLoadPrep`, `PostLoadPrep` are file-static in `pcsx2/SaveState.cpp`, so they aren't directly callable from `LibretroSaveState.cpp`. The fork inlines each one inside `UnzipFromMemory`. Each is small (~15–30 lines); duplication cost is low and the duplicated logic is stable upstream (last touched by `BaseSavestateEntry` refactor in late 2024 per git log).

### Pause-stable handshake

Lifted verbatim from reverted commit `2eddc63de`:

- `VMState WaitForVmPaused()` — returns prior state; pauses VM via `VMManager::SetPaused(true)`; polls `VMManager::GetState()` in 1 ms increments up to 200 ms; returns `VMState::Shutdown` sentinel on timeout/abort.
- `void ResumeVm(VMState prev_state)` — un-pauses if prev was `Running`.

The 200 ms ceiling is a defensive bound. PCSX2's EE thread reaches the next event-test typically within one frame (~16 ms). 200 ms catches a deeply stalled MTGS or recompiler edge case without locking up the host indefinitely. On timeout we log a `RETRO_LOG_WARN` and return false from the libretro entry — the frontend's save attempt fails cleanly rather than hanging.

### Trace boundaries

`RETRONEST_STATE_TRACE=1` env-gated (mirrors SP4's audio trace, SP5's input trace, and SP6's `retro_reset` boundary). Six boundaries:

1. `retro_serialize_size: probe start` — VMState, cached?
2. `retro_serialize_size: probe done` — size in bytes, ms elapsed
3. `retro_serialize: start` — len param
4. `retro_serialize: done` — bytes written, ms elapsed, ok flag
5. `retro_unserialize: start` — len param
6. `retro_unserialize: done` — ms elapsed, ok flag, error string if !ok

(Boundary 1 from `retro_reset` already exists in `LibretroFrontend.cpp` from SP6 Task 5; the count above is fresh boundaries for SP6.5.)

Zero overhead when `RETRONEST_STATE_TRACE` is unset (single `getenv` at first use, cached `bool`).

### Memory ownership and lifetimes

- `MemoryZipSink::bytes` is a `std::vector<u8>` owned by the sink; freed when sink goes out of scope (end of `retro_serialize_size` / `retro_serialize`).
- For read mode, the sink holds a non-owning `(const void*, size_t)` pair. The caller (the libretro frontend, owned by RetroAch frontend) guarantees the buffer outlives the `retro_unserialize` call.
- `zip_source_t*` ownership follows libzip rules: if `zip_open_from_source` succeeds, the source is owned by the `zip_t*` and freed by `zip_close`; if it fails, we must `zip_source_free` ourselves. Per-entry `zip_source_buffer` ownership is identical (owned by `zf` on success of `zip_file_add`). The save flow uses `ScopedGuard`-style cleanup to handle both paths, matching `SaveState.cpp:790-792`'s precedent.
- `ArchiveEntryList` returned by `SaveState_DownloadState` owns its `VmStateBuffer`; we hold it via `std::unique_ptr` for the duration of the serialize call.

### Error handling

| Path | Failure | Response |
|---|---|---|
| Pause handshake timeout | `WaitForVmPaused` returns Shutdown sentinel | log WARN, return 0/false |
| `SaveState_DownloadState` returns null | Error already populated | log WARN with err string, return 0/false |
| `zip_open_from_source` fails on write | zip_error_t populated | log WARN, free source, return 0/false |
| `zip_file_add` returns -1 | per-entry failure | log WARN with zip_strerror, abort save, return 0/false |
| `zip_close` returns non-zero | zip serialization failure | log WARN, `zip_discard(zf)`, return 0/false |
| Caller buffer too small (`buf.size() > len`) | probe-once violation | log ERROR (loud), return false; do NOT write partial |
| `zip_open_from_source` fails on read (corrupt save) | zip_error_t populated | log WARN, return false; VM untouched |
| `CheckVersion` mismatch | save from different PCSX2 build | log WARN, return false; VM untouched (no Reset because nothing was loaded yet) |
| Per-entry `FreezeIn` failure mid-load | partial state applied | call `VMManager::Reset()`, log ERROR, return false (matches SaveState.cpp:1239 behavior) |

The "mid-load failure → Reset" behavior is non-recoverable but preferable to a half-loaded VM. The rationale matches upstream: once any entry has applied, EE / IOP / GS state is partial and the only safe response is to reset. The user sees the game return to BIOS / fast-boot. SP6.5 inherits this, doesn't try to do better.

### Threading

`retro_serialize_size`, `retro_serialize`, `retro_unserialize` all run on the libretro frontend thread (the same thread that called `retro_run`). The pause handshake ensures the EE thread is at a stable event-test boundary before we read PCSX2's state. No additional locking needed; PCSX2's internal save-state primitives are designed for this single-pauser model (PCSX2-Qt uses the same one).

### Cache invalidation

`g_serialize_size` is reset to 0 in `retro_unload_game` (lifted from reverted commit `2eddc63de`). Different games can produce different sizes (different ELF, different chip configurations); we must re-probe per game load. Within a single game session, size is invariant by construction (`ZIP_CM_STORE` + fixed entry sizes + fixed entry count).

## Data flow

```
retro_serialize_size  ─► [pause VM]
                          │
                          ▼
                     SaveState_DownloadState
                          │   ArchiveEntryList { buffer: VmStateBuffer, entries[] }
                          ▼
                     MemoryZipSink (write mode)
                          │   <empty std::vector<u8>>
                          ▼
                     zip_open_from_source(sink.AsSource(), ZIP_CREATE)
                          │
                          ▼
                     for each entry:
                          zip_source_buffer(zf, &buf[entry.DataIndex], entry.DataSize)
                          zip_file_add(zf, entry.Filename, source, ZIP_FL_ENC_UTF_8)
                          zip_set_file_compression(zf, fi, ZIP_CM_STORE, 0)
                          │
                          ▼
                     zip_close(zf)
                          │   sink.bytes now contains the finalized zip
                          ▼
                     [resume VM]
                          │
                          ▼
                     g_serialize_size = sink.bytes.size()
                          │
                          ▼
                     return cached size


retro_unserialize ───► [pause VM]
                          │
                          ▼
                     MemoryZipSink (read mode over (src, len))
                          │
                          ▼
                     zip_open_from_source(sink.AsSource(), ZIP_RDONLY)
                          │
                          ▼
                     UnzipFromMemory(zf, &err):
                          ├─ CheckVersion(zf)
                          ├─ CheckFileExistsInState for each required entry
                          ├─ PreLoadPrep()
                          ├─ LoadInternalStructuresState(zf, internal_index)
                          ├─ for each SavestateEntries[i]:
                          │     zip_fopen_index_managed(zf, entryIndices[i])
                          │     SavestateEntries[i]->FreezeIn(zff.get())
                          │     │
                          │     └─ failure ► VMManager::Reset(); return false
                          ├─ PostLoadPrep()
                          └─ return true
                          │
                          ▼
                     [resume VM]
```

## Testing

### Smoke tests on R&C 2 (RetroNest, Rosetta `arch -x86_64`)

1. **Probe-once stability.** Boot R&C 2 to Running. Trigger 5 save-state operations through RetroNest's UI. Confirm `RETRONEST_STATE_TRACE` log shows `probe_size=N` once, then `cached_size=N` (same N) for subsequent calls. After `retro_unload_game`, re-load R&C 2 and trigger a save — confirm probe runs again and reports the same N.

2. **Mid-session round-trip.** Save state at start of Level 1. Advance 30 s of gameplay (move, jump, optionally shoot). Load state. Verify:
   - Character position visually returns to save point.
   - Inventory state matches save point.
   - Audio is correct within 1 s (resync is normal; persistent desync is not).
   - No crash, no Console.Error, no GS device complaint.

3. **Quit-resume.** With game running, close RetroNest cleanly via the close button. Confirm `GameSession::terminate → CoreRuntime::requestSaveState` log line indicating the resume file was written. Reopen RetroNest, launch R&C 2 again. Verify:
   - `findResumeFile` log line indicates the resume file was found.
   - Game returns to approximately where it was, not the title screen.

4. **Bad input rejection.** Manually construct a 256-byte buffer of zeros and call `retro_unserialize`. Confirm it returns false, logs a WARN about zip open failure or version mismatch, and the VM continues running unaffected.

### Negative cases (not user-driven; verified by log inspection during step 2/3)

- Pause-handshake timeout path: forcibly stall MTGS by leaving game paused in PCSX2 internal state — confirm `WaitForVmPaused` warns and returns 0/false without hanging. (Optional; only triggered if step 2 produces unexpected behavior.)
- Mid-load `FreezeIn` failure: not easily triggered without a corrupt save; covered indirectly by step 4.

### Verification before committing

- `cmake --build build --target pcsx2_libretro` succeeds with no new warnings.
- `nm build/pcsx2-libretro/pcsx2_libretro.dylib | grep retro_serialize` shows the three symbols still exported.
- Manual diff vs. reverted `2eddc63de` confirms the pause/resume helpers are byte-identical (they should be — no reason to change them).

## Risks

- **`zip_source_function` callback complexity.** The 15-opcode dispatch is mechanical but easy to get subtle bugs wrong (off-by-one in seek; misuse of `command_data`; forgetting to handle a supported opcode). Mitigation: keep the implementation small, exercise read-mode and write-mode paths during the round-trip smoke test, and use the existing `SaveState_CompressScreenshot` pattern as the lower bound for "what libzip in memory looks like in this file". The subagent code-quality review (SP6 cycle pattern) is the second mitigation.
- **`zip_close` cost.** The comment at `SaveState.cpp:1069` calls it "the expensive part with libzip". For our use case it's pure memory operations over a ~5–10 MB zip (most entries are uncompressed RAM regions). Expected cost: under 10 ms. If profiling shows this exceeds 30 ms, fold a future SP6.6 to investigate (e.g. preallocate `sink.bytes.reserve(probed_size)` after the first probe to skip vector growth).
- **Fork of `SaveState_UnzipFromDisk`.** Roughly 50 lines of duplicated logic. Monthly rebase will surface any upstream changes as a manual merge. We accept this; the alternative (touching upstream `SaveState.cpp` to extract a `zip_t*`-taking variant) violates the rebase-discipline rule documented in `project_pcsx2_libretro_port.md` ("never modify upstream files outside the single 4-line block in top-level CMakeLists.txt").
- **64 MB upper bound** on `destlist->GetBuffer()` is set in `SaveState_DownloadState`. If a future PCSX2 update grows past it, both upstream and our libretro path break together — not a new risk.
- **Save state during BIOS / fast-boot.** SP6 settings hardcode `params.fast_boot = true` (`LibretroFrontend.cpp:284`). Until VM reaches Running, `retro_serialize_size` returns 0 and the frontend retries. No risk of writing a half-initialized state.

## Sub-tasks (one commit each)

The implementation breaks into four commits, mirroring SP6's per-task cadence:

1. **`LibretroSaveState.{h,cpp}` skeleton.** Move `WaitForVmPaused`/`ResumeVm`/`g_serialize_size`/`ResetSerializeSizeCache` out of `LibretroFrontend.cpp` into the new files. `retro_serialize_size`/`retro_serialize`/`retro_unserialize` become one-line delegates returning 0/false. CMakeLists.txt updated. Builds clean. (Mechanical refactor — no behavior change vs. current stubs.)
2. **`MemoryZipSink` source-function callback** + factory helpers. Internal-only; not yet called from the retro_ entry points. Adds a developer-only unit-test-shaped check in `LibretroSaveState.cpp` (compile-time disabled by default — comment-out path) that verifies write→read round-trip on a 4-byte sentinel buffer. (Optional; if it bloats the diff, drop it.)
3. **`retro_serialize_size` + `retro_serialize` implementation.** Wires the save flow. After this commit, save attempts produce a real zip blob; loads still return false. Smoke test: save state from RetroNest UI, confirm log line `probe_size=N` appears and is stable across repeated saves.
4. **`retro_unserialize` + `UnzipFromMemory` fork.** Wires the load flow. After this commit, round-trip works. Run all four smoke tests above.

Each commit lands clean, builds, can be reverted in isolation. The reviewer's earlier observation that "save state is one unit, not three" (from SP6 retrospective) is respected at the spec level: the unit is SP6.5 as a whole; the sub-tasks are just safe progression checkpoints.

## Open questions

None at design time. Two items to **verify during implementation** rather than guess at now:

- Whether `zip_source_function` requires more or fewer opcodes than the 15 listed above for write+read+seek to work — confirm by reading `zip_source_function(3)` and the libzip header against the in-tree version (`3rdparty/libzip` or system).
- Whether `ZIP_FL_ENC_UTF_8` is needed for our filenames (which are ASCII-only). Likely irrelevant for our case; `SaveState.cpp:1002` uses it, so we'll match for consistency.

Both are details surfaced in the first commit's diff review.
