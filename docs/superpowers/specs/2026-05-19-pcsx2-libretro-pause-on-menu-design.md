# PCSX2 libretro pause-on-menu — design

**Date:** 2026-05-19
**Sub-project:** Make PCSX2 libretro halt internal EE/MTGS/MTVU threads when the in-game menu opens, so the on-screen game actually freezes (matches mGBA libretro behavior).
**Status:** Design — pending implementation plan

## Summary

Today, when the in-game menu opens during a PCSX2 libretro session, RetroNest calls `CoreRuntime::pause()`. That stops the worker thread from calling `retro_run` and silences SDL audio. For mGBA this is sufficient — mGBA's emulation runs entirely inside `retro_run`, so stopping the call stops the game. For PCSX2 it isn't — emulation runs on PCSX2's own EE/MTGS/MTVU threads inside the dylib, decoupled from `retro_run`. PCSX2 uses libretro hardware-render mode and renders directly to the Metal NSView, so frames keep appearing on screen even with `retro_run` blocked. Audio is muted by `CoreRuntime`, but gameplay animation does not freeze.

This sub-project adds a custom exported symbol on the pcsx2-libretro side — `retronest_set_paused(bool)` — that calls `VMManager::SetPaused(true/false)` directly. RetroNest's `CoreLoader` probes the dylib for the symbol via `dlsym`; `CoreRuntime::pause()/resume()` calls it before/after the existing pause flow. Cores that don't export the symbol (mGBA, future libretro adapters that don't need it) fall through to the current behavior unchanged.

## Motivation

User-reported observation on PCSX2 (R&C 2): "the gameplay animation keeps going but the sound stops" when the in-game menu opens. The mGBA equivalent freezes cleanly. The architectural cause is that PCSX2's internal threads are not stoppable by halting the libretro frame pump:

- **mGBA** is single-threaded inside `retro_run`. `retro_run` returns → emulation paused.
- **PCSX2** spawns EE / MTGS / MTVU threads inside the dylib. `retro_run` is just a frame-pump that signals `g_present_cv` and processes input. MTGS renders to the Metal NSView directly via the libretro HW-render bridge, bypassing RetroNest's frame pipeline. Stopping `retro_run` doesn't stop them.

To get a true pause, the host needs a way to tell pcsx2-libretro "stop your internal threads." libretro itself has no `retro_pause` API — the spec assumes stopping `retro_run` is enough.

## Approach

Custom exported symbol — `retronest_set_paused(bool)`:

- pcsx2-libretro adds the export. Implementation calls `VMManager::SetPaused(paused)` and (on `paused=true`) waits for `VMState::Paused` to be observed by EmuThread before returning, so the host knows pause has actually taken effect when the call returns.
- RetroNest's `CoreLoader` adds the symbol to its function-pointer table. Resolved with the non-required dlsym helper, so a missing symbol just leaves the pointer null without erroring the load.
- `CoreRuntime::pause()` checks the pointer; if non-null, calls `retronest_set_paused(true)` BEFORE setting `m_paused`, so PCSX2's threads have stopped by the time the worker loop blocks on `m_pauseCv`.
- `CoreRuntime::resume()` mirrors: clear `m_paused`, then call `retronest_set_paused(false)`.

Approaches considered and rejected:

- **Private libretro env enum** with a polled flag. Awkward state-transition dance — CoreRuntime would have to keep pumping `retro_run` past the pause moment to give the core a chance to read the flag. Custom symbol is cleaner.
- **No code change** (just document the current behavior). Doesn't actually solve the user's problem.

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│ GameSession::pauseEmulation() / resumeEmulation()            │
│   (called from AppController::openLibretroOverlayMenu and    │
│    AppWindow.qml inGameMenu open/close)                      │
└────────────────────────────┬─────────────────────────────────┘
                             ▼
┌──────────────────────────────────────────────────────────────┐
│ CoreRuntime::pause()                                         │
│   1. NEW: if (loader.syms().retronest_set_paused) call(true) │
│      (blocks until VM observes Paused, ≤500 ms)              │
│   2. existing: m_paused = true                               │
│   3. existing: m_audio.setPaused(true)                       │
│   4. existing: clear rumble                                  │
│                                                              │
│ CoreRuntime::resume()                                        │
│   1. existing: m_paused = false; m_pauseCv.notify_all()      │
│   2. existing: m_audio.setPaused(false)                      │
│   3. NEW: if (loader.syms().retronest_set_paused) call(false)│
└────────────────────────────┬─────────────────────────────────┘
                             ▼ dlsym-resolved, PCSX2 only
┌──────────────────────────────────────────────────────────────┐
│ pcsx2-libretro: retronest_set_paused(bool paused)            │
│   paused=true:  VMManager::SetPaused(true);                  │
│                  WaitForVmPaused(timeout=500 ms);             │
│                  on timeout: qWarning + return.              │
│   paused=false: VMManager::SetPaused(false); return.         │
└──────────────────────────────────────────────────────────────┘
```

## Components

### RetroNest side

**`cpp/src/core/libretro/core_loader.h`** — add a function-pointer field for the new symbol:

```cpp
using retronest_set_paused_t = void (*)(bool);
struct LibretroSymbols {
    // ...existing fields...
    retronest_set_paused_t retronest_set_paused = nullptr;  // NEW (optional)
};
```

**`cpp/src/core/libretro/core_loader.cpp`** — resolve via the existing non-required dlsym helper. If the symbol isn't exported (mGBA, RetroArch's other cores), the pointer stays null and load succeeds.

**`cpp/src/core/libretro/core_runtime.cpp`** — modify `pause()` and `resume()`:

```cpp
void CoreRuntime::pause() {
    // NEW: hand control to the core so its internal threads can park.
    // For mGBA / cores that don't export the symbol, this is a no-op.
    if (auto fn = m_loader.symbols().retronest_set_paused)
        fn(true);

    m_paused = true;
    m_audio.setPaused(true);
    // ...existing rumble clear...
}

void CoreRuntime::resume() {
    {
        std::lock_guard<std::mutex> l(m_pauseMx);
        m_paused = false;
    }
    m_audio.setPaused(false);
    m_pauseCv.notify_all();

    // NEW: unblock the core's internal threads. Comes AFTER our worker
    // wakes up so retro_run can resume immediately on the next tick.
    if (auto fn = m_loader.symbols().retronest_set_paused)
        fn(false);
}
```

### pcsx2-libretro side

**`pcsx2-libretro/LibretroFrontend.cpp`** — new export. Lives near the existing `retro_*` exports; no header changes required (it's only consumed via dlsym).

```cpp
extern "C" RETRO_API void retronest_set_paused(bool paused);
void retronest_set_paused(bool paused)
{
    if (paused)
    {
        VMManager::SetPaused(true);
        // Reuse the helper from LibretroSaveState.cpp that already polls
        // VMState. 500 ms is plenty — in practice EmuThread observes
        // Paused within one frame (~16 ms). On timeout we log + return
        // so the host doesn't deadlock.
        if (!WaitForVmPaused(/*timeout_ms=*/500))
        {
            FrontendLog(RETRO_LOG_WARN,
                "retronest_set_paused: VM did not enter Paused within 500ms");
        }
    }
    else
    {
        VMManager::SetPaused(false);
        // EmuThread's loop will exit VMState::Paused on its next tick
        // (the loop sleeps 1ms while Paused, per the SP6.5 Task 2 fix).
    }
}
```

`WaitForVmPaused` is currently `static` inside `LibretroSaveState.cpp`. Move it to a shared header (e.g. `LibretroSaveState.h`) or expose via the `Pcsx2Libretro` namespace so the new export can call it. Keep the timeout parameterized.

### Symbol-export visibility on macOS

`pcsx2-libretro` builds as a Mach-O bundle (`MODULE` library in CMake). Symbols need `RETRO_API` (which expands to `__attribute__((visibility("default")))`) to be visible to `dlsym`. The existing `retro_*` exports already use this; the new symbol matches.

## Error handling

| Failure | Behavior |
|---|---|
| Symbol missing (mGBA, RetroArch's own cores) | `CoreLoader` resolves to `nullptr`. `CoreRuntime::pause()/resume()` skips the call. Existing pause path (stop `retro_run` + mute audio) applies. mGBA continues to pause correctly via its single-threaded model. |
| `WaitForVmPaused` timeout | `qWarning` + return. Visual pause may briefly lag the menu opening (worst case ~500 ms). Audio still muted on time. |
| Pause called during save state | Safe — `VMManager::SetPaused(true)` is already part of the SP6.5 save-state pause-handshake. Reentrant. |
| Pause called twice | Idempotent — `SetPaused(true)` is no-op when already paused. |
| Game shuts down while paused | Existing `RequestShutdown` flips `m_stop_requested` which wakes EmuThread out of `VMState::Paused` (the loop checks both flags each tick). |
| Resume called when not paused | Idempotent — `SetPaused(false)` is no-op when not paused. |

## Testing

**RetroNest**
- Extend `cpp/tests/test_core_loader.cpp` (or add `cpp/tests/test_core_runtime.cpp` cases) to verify:
  - `retronest_set_paused` resolves to a non-null pointer when the dylib exports it.
  - Stays `nullptr` when the dylib does not (use a mock dylib or stub at link).
- Sanity: `CoreRuntime::pause()` doesn't crash when the symbol is `nullptr` (mGBA path).

**pcsx2-libretro**
- New standalone test `pcsx2-libretro/tools/test_pause_on_menu.cpp` along the pattern of `tools/test_settings_overrides.cpp` (clang++ direct compile, no PCSX2 link). Mocks `VMManager::SetPaused` + `WaitForVmPaused`; verifies:
  - `retronest_set_paused(true)` calls `SetPaused(true)` then `WaitForVmPaused`.
  - Timeout path emits the warning, doesn't crash.
  - `retronest_set_paused(false)` calls `SetPaused(false)` only — no wait.
  - Double-pause / double-resume are idempotent.

**Integration smoke**
- Launch RetroNest, boot R&C 2 (PCSX2).
- Open in-game menu (Cmd+Shift+Esc or controller hotkey).
- Verify: gameplay animation freezes immediately (was broken; this is the user-reported bug).
- Close menu → animation resumes from the same frame.
- Repeat with mGBA + a GBA game: verify pause/resume still works, no regression.
- Open menu + quickly close + open again → no audio glitch, no crash.

## Out of scope

- **Pause-on-focus-loss** — different UX trigger (window loses key focus). Currently unimplemented for libretro paths; this sub-project doesn't add it.
- **Other libretro cores with decoupled emulation threads** — none currently in our build need this. Pattern is reusable when one appears: that core exports `retronest_set_paused`, `CoreLoader` already resolves it.
- **Fast-forward / slow-mo** — already separate per-emulator hotkeys.
- **A QML/UI pause indicator** — the menu being open is the visible indicator; no extra "paused" badge needed.

## Code volume estimate

- **RetroNest** (~30 LOC source + ~25 LOC test):
  - `CoreLoader` field + resolve: ~5 LOC
  - `CoreRuntime` pause/resume modifications: ~10 LOC
  - Header forward decls: ~5 LOC
  - Comments / docstrings: ~10 LOC
  - Tests: ~25 LOC
- **pcsx2-libretro** (~30 LOC source + ~30 LOC test):
  - `retronest_set_paused` implementation: ~20 LOC
  - `WaitForVmPaused` move-to-header: ~10 LOC
  - Standalone unit test: ~30 LOC

Total: ~115 LOC across both repos.

## Linked memories
- `[[session-handoff-host-runtime-parity-shipped]]` — `WaitForVmPaused` and the VMState handshake pattern were introduced in SP6.5.
- `[[session-handoff-core-osd-toast-shipped]]` — weak-stub-bridge pattern; not directly reused here (this design uses dlsym instead of weak linkage), but the test-isolation lessons apply.
- `[[rebuild-before-debugging-regressions]]` — both binaries need to rebuild and reinstall to verify the live behavior.
