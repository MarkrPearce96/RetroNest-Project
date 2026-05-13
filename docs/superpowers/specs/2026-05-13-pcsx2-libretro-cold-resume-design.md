# PCSX2 Libretro Core — Cold-Resume on Launch (Task 4.5)

**Date:** 2026-05-13
**Status:** Design — implementation pending
**Owner:** mark
**Scope:** Closes the cold-resume execution gap left by SP6.5 Task 4. RetroNest's `cfg.resumeStatePath` path will reach the libretro core *before* `retro_load_game`, the core sets it on `VMBootParameters::save_state`, and PCSX2's `VMManager::Initialize` runs `DoLoadState` after full BIOS init / ELF discovery. Mid-session save/load (already verified live in SP6.5) is untouched. Backward-compatible: mGBA and any other libretro core that doesn't know about the new env call falls through to the existing post-load `retro_unserialize` path.
**Predecessors:** [SP6.5 — Save state via in-memory libzip](2026-05-12-pcsx2-libretro-savestate-sp65-design.md). SP6.5 shipped 2026-05-13; six commits on `pcsx2-libretro@retronest-libretro` (`e0024bd33`..`2764d75d2`) plus `Pcsx2LibretroAdapter::findResumeFile` override on `RetroNest-Project@main` (`6825394`). End-to-end mid-session save/load verified live on R&C 2 (5+ round-trips, probe-once size stable at 42354227 bytes).

## Context

SP6.5 implemented `retro_serialize` / `retro_unserialize` for the PCSX2 libretro core. Mid-session save/load works perfectly. The remaining gap surfaced during the SP6.5 Task 4 smoke test:

1. User clicks **Save & Exit** in the in-game menu.
2. `GameSession::terminate` runs `requestSaveState(resumePath)` then `runtime()->stop()`. The worker's `flushPendingSaveState` on its way through teardown invokes `retro_serialize`, which writes a valid `ZIP_CM_STORE` zip to `~/Documents/RetroNest/emulators/pcsx2-libretro/ps2/savestates/{serial}.resume` (verified 42 MB, `PK\x03\x04` magic).
3. User force-recovers from the SP3.6 documented quit hang, relaunches RetroNest, launches the same game.
4. `Pcsx2LibretroAdapter::findResumeFile` resolves the file. `ResumeStateDialog` appears. User picks Resume. `CoreRuntime::runLoop` sets `m_cfg.resumeStatePath` and, at `core_runtime.cpp:319`, reads the file and calls `retro_unserialize(state.constData(), state.size())` directly — **after `retro_load_game` returned, but before the first `retro_run` iteration**.
5. Our `retro_unserialize` reports `ok=1 in 84ms` (verified). But the EE immediately enters a TLB-miss loop on PS2 I/O register region (0x10002000) and the game window stays black.

**Root cause** (verified live with `RETRONEST_STATE_TRACE` + crash analysis on commit `d240b462a`): when `retro_load_game` returns, the VM is mid-BIOS-init. `params.fast_boot = true` was honored, the EE has executed a small number of instructions, but PS2 kernel data structures (TLB, COP0, mmap fastmem fixups) are still being initialized. `SaveState_UnzipFromZip`'s `PreLoadPrep` clears the recompiler caches and `PostLoadPrep` diffs the freshly-init TLB against the loaded TLB — but the freshly-init TLB hasn't reached the steady state the saved game expected, so the diff is invalid. The EE then jumps into game code that assumes the saved-state mappings are live, hits I/O addresses the half-initialized memory map doesn't have wired, and TLB-misses repeatedly.

**Canonical PCSX2 fix:** PCSX2-Qt loads launch-time save states via `VMBootParameters::save_state`. `VMManager::Initialize` (`pcsx2/VMManager.cpp:1340-1647`) reads `boot_params.save_state` at line 1382 into `state_to_load`, sets up the VM through BIOS init / ELF discovery / `s_fast_boot_requested` / SetEmuThreadAffinities, then at line 1636-1643 (the very end of Initialize, before the function returns success) calls `DoLoadState(state_to_load.c_str(), error)`. Load failure shuts the VM down with `Shutdown(false)` + returns `VMBootResult::StartupFailure`. This is the only ordering that produces a runnable VM for cold-resume.

The libretro API doesn't expose a built-in way for the frontend to hand a "boot state path" to the core before `retro_load_game`. Standard core options can't carry free-form paths (they're enum-style values). The clean libretro-idiomatic mechanism is a **private env callback** that the core polls during `retro_load_game`, which the frontend's env dispatcher responds to.

## Goal

R&C 2 cold-resume on launch:

- `ResumeStateDialog` appears as it does today (no change to the SP6.5 `findResumeFile` flow).
- User picks Resume → game launches and **resumes at the saved position**, not BIOS / title.
- No TLB-miss spam in the log; `RETRONEST_STATE_TRACE` shows the load happening *inside* `retro_load_game` (one line: `retro_load_game: cold-boot via save state {path}`), not via a separate `[STATE_TRACE] Unserialize` boundary.
- Mid-session save/load behavior (in-game-menu save/load) is bit-for-bit unchanged — the new env call is only queried during `retro_load_game`.
- mGBA behavior is bit-for-bit unchanged — mGBA doesn't query the new env call, RetroNest falls through to the existing `retro_unserialize` path.

## Out of scope

- **Changing PCSX2's `DoLoadState` body or any other upstream PCSX2 code.** Task 4.5 introduces no new upstream-edit exceptions. The SP6.5 Task 2 exception (`SaveState_UnzipFromMemory`) remains the only save-state-related upstream edit.
- **Changing the resume-file format.** The `.resume` file written by `Serialize` / read by the cold-resume path is the same `ZIP_CM_STORE` zip that SP6.5 produces. `DoLoadState` calls into `SaveState_UnzipFromDisk` which uses the same shared `SaveState_UnzipFromZip` helper that `SaveState_UnzipFromMemory` uses. The format is round-trip compatible by construction.
- **Mid-session load via the new path.** `requestLoadState` from the in-game menu remains on the `retro_unserialize` path because the VM is already running and warm (no BIOS-init race).
- **Changing the quit-resume save side.** SP6.5's `requestSaveState` → `Serialize` path still writes the `.resume` file. We're only changing the *read* side for the cold case.
- **Making mGBA's cold-resume use `params.save_state`.** mGBA's cold-resume works today via the existing post-load `retro_unserialize` (GBA has trivial BIOS-init compared to PS2). No reason to migrate it.
- **A new "preload state" libretro standard.** This stays in the `RETRO_ENVIRONMENT_PRIVATE` (`0x20000`) namespace as a RetroNest↔pcsx2-libretro extension. If a future libretro core needs the same mechanism, lifting it to a shared header in RetroNest is trivial; the on-the-wire shape stays the same.

## Architecture & components

### New private env call

A new private env constant defined in **both** `pcsx2-libretro/LibretroFrontend.cpp` (or a small new header) and `RetroNest-Project/cpp/src/core/libretro/environment_callbacks.h`. The numerical value matches on both sides.

```cpp
// Private RetroNest ↔ pcsx2-libretro extension. Frontend stores a UTF-8
// path string; the libretro core polls during retro_load_game. If the
// core reads (returns true), the frontend marks the path consumed so
// it doesn't fall back to the post-load retro_unserialize compat path.
#define RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH \
    (0x02 | RETRO_ENVIRONMENT_PRIVATE)
```

`RETRO_ENVIRONMENT_PRIVATE` is `0x20000` per `libretro.h`. `0x20001` is already taken by `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` (per `environment_callbacks.h:12`); `0x20002` is the next free RetroNest-private slot and can't collide with any standard libretro env call.

The protocol is one-shot per `retro_load_game`:

1. RetroNest sets `m_envCtx.bootStatePath = m_cfg.resumeStatePath.toUtf8()` and `m_envCtx.bootStatePathConsumed = false` before calling `retro_load_game`.
2. The core may call `environ_cb(RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH, &out)`. If `bootStatePath` is non-empty and not already consumed, the env handler stores its `constData()` pointer at `*out`, **sets `bootStatePathConsumed = true` to mark consumed**, and returns `true`. The QByteArray storage stays alive.
3. If `bootStatePath` is empty or `bootStatePathConsumed` is true, the handler returns `false`.
4. After `retro_load_game` returns, RetroNest checks `m_envCtx.bootStatePathConsumed`. If true, the core consumed the path and the legacy `retro_unserialize` block is skipped. If false (mGBA / cores that don't query), fall back to the existing `retro_unserialize` block.

**Implementation note — deviation from earlier draft (commit `907fb12`):** an initial draft of this spec used `bootStatePath.clear()` inside the env handler as the consumed marker, and gated the legacy block on `bootStatePath.isEmpty()`. Code review caught a latent lifetime bug: `m_cfg.resumeStatePath.toUtf8()` returns a `QByteArray` with refcount=1, and `clear()` on a unique QByteArray frees the internal buffer immediately. The caller's `*out` pointer would then dangle as soon as the env handler returned. The fix introduced a separate `bool bootStatePathConsumed` flag — the QByteArray is never mutated inside the handler, so `constData()` stays valid for the caller's synchronous use, and the consumed-flag tracks one-shot semantics. Test coverage in `cpp/tests/test_environment_callbacks.cpp::testGetBootStatePathOneShot` pins this invariant against regression.

### Files

**`pcsx2-libretro/` (modify only):**

- `LibretroFrontend.cpp` — define `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH`; in `retro_load_game`, after the existing BIOS-path resolution but before `params.fast_boot = true` (line 405), query the env callback. If a non-empty path comes back, set `params.save_state = path` and emit one `[STATE_TRACE]`-prefixed log line (`retro_load_game: cold-boot via save state %s`). No other changes — `params.fast_boot = true` stays. PCSX2's `s_fast_boot_requested` is independently honored by `VMManager::Initialize`; `DoLoadState` runs after BIOS init regardless.

**`RetroNest-Project/cpp/src/core/libretro/` (modify only):**

- `environment_callbacks.h` — declare `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH` and add two fields to `EnvironmentContext`: `QByteArray bootStatePath` (plain UTF-8 bytes; `constData()` is stable for the env_cb call) and `bool bootStatePathConsumed = false` (one-shot marker; see Implementation note above).
- `environment_callbacks.cpp` — add one case in the dispatch:
  ```cpp
  case RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH: {
      if (!data) return false;
      if (ctx->bootStatePathConsumed || ctx->bootStatePath.isEmpty())
          return false;
      auto** out = static_cast<const char**>(data);
      *out = ctx->bootStatePath.constData();
      // DO NOT call clear() here — toUtf8() returns a refcount=1
      // QByteArray; clear() would free the buffer that *out points at
      // and dangle the caller's pointer. Set the flag instead; the
      // QByteArray remains alive for the duration of EnvironmentContext.
      ctx->bootStatePathConsumed = true;
      return true;
  }
  ```
- `core_runtime.cpp` — two small changes:
  1. Before `retro_load_game`: `m_envCtx.bootStatePath = m_cfg.resumeStatePath.toUtf8();` and `m_envCtx.bootStatePathConsumed = false;`. (Both reset every `runLoop` invocation.)
  2. Gate the existing `retro_unserialize` block at line 315-321 on `!m_envCtx.bootStatePathConsumed` (NOT `bootStatePath.isEmpty()`, per the lifetime fix above) — only fall through when the core didn't consume the path.

That's the entire diff: ~15 lines on the pcsx2-libretro side, ~35 lines on the RetroNest side.

### Data flow

```
RetroNest CoreRuntime::runLoop start
  │
  ├─ m_envCtx.bootStatePath = m_cfg.resumeStatePath.toUtf8()  // empty unless user picked Resume
  │
  ▼
retro_load_game(game)
  │  (libretro core)
  ├─ resolve BIOS via system_dir
  ├─ environ_cb(RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH, &out)
  │    └─ env_cb handler: if ctx.bootStatePath empty, return false
  │                       else: *out = data; clear; return true
  ├─ if got path: params.save_state = path
  ├─ params.fast_boot = true
  ├─ emu.Start(params)
  │   └─ EmuThread → VMManager::Initialize(params)
  │       ├─ BIOS init / ELF discovery / s_fast_boot_requested = computed
  │       ├─ ... (full normal init) ...
  │       └─ if (!state_to_load.empty()) DoLoadState(state_to_load)
  │           └─ SaveState_UnzipFromDisk → SaveState_UnzipFromZip (SP6.5 shared body)
  │               └─ CheckVersion + PreLoadPrep + per-entry FreezeIn + PostLoadPrep
  │       (VM in Paused; ready to run)
  ├─ TryIssueMemoryMaps()  // SP6 path, unchanged
  │
  ▼
retro_load_game returns true
  │
  ├─ if (m_envCtx.bootStatePath.isEmpty()) {     // core consumed → cold-resume done
  │      // no-op; nothing more to do
  │  } else {                                    // core didn't consume (mGBA etc.)
  │      QFile f(m_cfg.resumeStatePath); ...
  │      s.retro_unserialize(state.constData(), size);
  │  }
  │
  ▼
m_rcheevos.beginSession + emit started()
  │
  ▼
runloop iterations (retro_run, etc.)
```

### Memory ownership and lifetimes

- `m_envCtx.bootStatePath` is owned by RetroNest's `EnvironmentContext` (per-runtime). It exists for the duration of one `retro_load_game` call. After consumption it's empty; if never consumed, it's cleared at the start of the next `runLoop` (next game session).
- The `const char*` returned via the env callback points into the `QByteArray`'s buffer. libretro callees must use it synchronously — the pointer is valid for the duration of the env-callback call. Our `retro_load_game` copies into `params.save_state` (a `std::string`), so the pointer is no longer needed after the env call returns.
- `params.save_state` is a `std::string` owned by `VMBootParameters`. `VMManager::Initialize` copies it to `state_to_load` (`VMManager.cpp:1382`). Lifetime is bounded by `Initialize`'s execution.

### Error handling

| Path | Failure | Response |
|---|---|---|
| Frontend has no resume path | normal launch | `bootStatePath` stays empty; env handler returns false; core skips `params.save_state = …`; VM boots fresh through fast_boot path |
| Core queries env, no path set | normal launch with non-PCSX2 core (mGBA) | env handler returns false; core takes default fresh-boot path |
| `DoLoadState` fails inside `VMManager::Initialize` | corrupt/incompatible state | `Initialize` calls `Shutdown(false)` + returns `VMBootResult::StartupFailure`; our `emu.Start(params)` returns false; `retro_load_game` returns false with the existing error-toast path (the SP6.5 `m_init_success.load()` check at `EmuThread.cpp:72`). User sees "VM init failed" toast — recoverable via Start Fresh on next launch. |
| Env call succeeds but core silently fails to read it | implementation bug | The path is cleared on the frontend side; RetroNest skips the fallback. `retro_load_game` runs with `params.save_state = ""` → fresh boot. Silent regression vs. user expectation. Mitigation: the `[STATE_TRACE]` log line `retro_load_game: cold-boot via save state %s` makes this visible in logs immediately. |
| User picks Resume but file is missing or zero bytes | shouldn't happen | `findResumeFile` filters by `QDir::Files`, so the path comes from a real directory entry. If the file vanishes between resolution and launch (race), `DoLoadState` reports the missing file. Caught by the `Initialize` failure path above. |

### Backward compatibility

- **mGBA**: doesn't include `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH` in its env-call repertoire → frontend's `bootStatePath` stays set after `retro_load_game` returns → RetroNest's legacy `retro_unserialize` block fires → mGBA's cold-resume works as before. Bit-for-bit unchanged.
- **Any future libretro core**: opts in by querying the env call. Opting out (or never knowing about it) keeps the existing behavior.
- **No upstream PCSX2 edits.** Task 4.5 introduces zero new upstream-exception hunks. The SP6.5 Task 2 exception (`SaveState_UnzipFromMemory` + shared `SaveState_UnzipFromZip`) remains the only save-state-related upstream change.

### `RETRONEST_STATE_TRACE` coverage

One new boundary: `retro_load_game: cold-boot via save state {path}` at `RETRO_LOG_INFO` (always logged on the cold-resume path, not env-gated, since it's a normal-mode informational line). The existing six SP6.5 boundaries (probe-start, paused, probed, serialize-start/done, unserialize-start/done) continue to fire only for in-session operations.

The new line is at INFO level, not under `[STATE_TRACE]`, so a future maintainer reading a log can quickly tell whether a session went through the cold-resume path or a fresh boot.

## Testing

### Smoke test on R&C 2 (RetroNest, Rosetta `arch -x86_64`, `RETRONEST_STATE_TRACE=1`)

1. **Cold-resume restore (primary)**:
   - Launch R&C 2 fresh, get to a recognizable position (note: post-BIOS, in actual gameplay if possible — but even FMV / menu state is acceptable for the test).
   - In-game menu → Save & Exit. Confirm the resume file is written under `~/Documents/RetroNest/emulators/pcsx2-libretro/ps2/savestates/{serial}.resume`.
   - Force-recover from the SP3.6 quit hang (`pkill -9 -f MacOS/RetroNest`).
   - Relaunch RetroNest. Launch R&C 2.
   - **Expect**: `ResumeStateDialog` appears. User picks Resume.
   - **Expect**: log line `retro_load_game: cold-boot via save state /Users/.../SCUS_972.68.resume` fires inside `retro_load_game`.
   - **Expect**: `VM RUNNING` fires AFTER the cold-boot line.
   - **Expect**: NO `[STATE_TRACE] Unserialize: start` lines (the legacy path is skipped).
   - **Expect**: NO TLB-miss spam.
   - **Expect**: game visually resumes at the save position, plays normally.

2. **Fresh boot regression**:
   - Same R&C 2, but pick "Start Fresh" in the Resume dialog.
   - **Expect**: log line `retro_load_game: cold-boot via save state …` does NOT fire.
   - **Expect**: game cold-boots through BIOS normally.

3. **In-session save/load regression (verifies SP6.5 path unaffected)**:
   - In any session, trigger save state from in-game menu → wait 30 s → load state.
   - **Expect**: existing `[STATE_TRACE] SerializeSize/Serialize/Unserialize` lines fire exactly as in SP6.5.
   - **Expect**: no new cold-boot log line (we're not in `retro_load_game`).

4. **mGBA cold-resume regression**:
   - Launch a GBA game. Save & Exit. Force-recover. Relaunch and Resume.
   - **Expect**: same behavior as before Task 4.5 — mGBA's env handler returns false for `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH` (it doesn't query), RetroNest falls through to the legacy `retro_unserialize` block, mGBA resumes cleanly.

5. **Universal build parity**: rebuild RetroNest.app (both slices via `scripts/build-universal.sh`), confirm both arm64 and x86_64 paths smoke-test pass at least item 1 above.

### Build verification

- `cmake --build` clean for both `pcsx2_libretro` and `RetroNest` targets on both arches.
- No new warnings.
- Existing libretro env unit tests in `cpp/src/core/libretro/test_environment_callbacks.cpp` (if any) continue to pass.

## Risks

- **Env number collision.** `0x20002` is in `RETRO_ENVIRONMENT_PRIVATE` namespace (`0x20000`) per libretro.h convention. `0x20001` is taken by `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW`; `0x20002` is the next free slot. Standard libretro env calls top out around `0x4F` (76), so the 0x20000 range is comfortably above. Other private cores using the same number would only matter if they shared the same RetroNest binary AND queried with a different intent — none of RetroNest's shipped cores (mGBA at minimum) use private env calls in this range today. Low risk; documenting the constant + `// pcsx2-libretro: SP6.5 Task 4.5` comment marker is sufficient.
- **`VMManager::Initialize` returning failure after `DoLoadState` fails.** A corrupt resume file would cause `retro_load_game` to return false, surfacing as a launch failure. The mitigation is that the resume file was written by our own `Serialize` minutes earlier; corruption between write and read is unlikely in normal operation. If it does happen, the user picks Start Fresh in the dialog and proceeds. No data loss (the resume file is supplementary to the user's memcard saves).
- **MTGS framebuffer presentation after cold-boot load.** SP6.5 mid-session loads showed the game visually resuming cleanly. The cold-boot load path runs through the SAME `DoLoadState` → `SaveState_UnzipFromZip` → `SavestateEntries[]` loop, so the same MTGS state-restore happens. Verified by the existing PCSX2-Qt launch-with-state flow being well-tested upstream.
- **Race between `bootStatePath` write and core query.** Both happen on the runtime worker thread synchronously: write before `retro_load_game`, query during. No threads involved; no race.

## Sub-tasks (one commit per item)

The implementation breaks into three small commits, mirroring the SP6.5 cadence. Each touches at most two repos and lands a logical unit independently.

1. **`environment_callbacks.{h,cpp}` — declare env constant + add handler case.** RetroNest-Project only. After this commit, `bootStatePath` is plumbed but never set or consumed; the legacy `retro_unserialize` path still handles cold-resume. No behavior change. Verifies the env-handler addition compiles + RetroNest's existing libretro-core tests still pass.

2. **`core_runtime.cpp` — set `m_envCtx.bootStatePath` before `retro_load_game` + gate the legacy block.** RetroNest-Project only. After this commit, RetroNest is fully wired but no core consumes the path → bootStatePath always stays set → legacy path always fires. Still no behavior change. Smoke-test: in-session save/load still works on R&C 2 + on a GBA game (mGBA).

3. **`LibretroFrontend.cpp` — query env + set `params.save_state`.** pcsx2-libretro only. This is the behavior-changing commit. After this commit, pcsx2-libretro consumes the path → RetroNest skips legacy path → cold-resume goes through `VMBootParameters::save_state`. Run smoke tests 1–5 from the Testing section. Commit only after they all pass.

This ordering keeps each commit's blast radius small. The cold-resume behavior change is isolated to a single ~15-line diff on the pcsx2-libretro side; the RetroNest side is pure plumbing that exists in the prior commits without firing yet.

## Open questions

None at design time. Two micro-details to confirm during implementation, not blockers:

- The exact format of the env handler dispatch in `environment_callbacks.cpp` (which switch case ordering, how `ctx` is reached). Adjacent cases (`SET_MEMORY_MAPS`, `GET_VARIABLE`) show the pattern.
- Whether `RETRO_ENVIRONMENT_PRIVATE` is already defined in the bundled `libretro.h` at `pcsx2-libretro/pcsx2-libretro/libretro.h` — if not (older header), use the literal `0x20000` and add a comment noting the source.
