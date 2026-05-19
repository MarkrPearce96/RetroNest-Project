# PCSX2 libretro pause-on-menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When the in-game menu opens on a PCSX2 libretro session, halt PCSX2's internal EE/MTGS/MTVU threads so gameplay animation freezes (currently only audio mutes because PCSX2 renders to the Metal NSView directly via HW-render, decoupled from `retro_run`).

**Architecture:** pcsx2-libretro exports a new C symbol `retronest_set_paused(bool)`. RetroNest's `CoreLoader` resolves it via the existing `resolveOptional` (silent if missing). `CoreRuntime::pause()` calls the symbol before setting `m_paused`; `resume()` calls after clearing it. mGBA / future cores that don't export the symbol fall through to current behavior. The pcsx2-libretro impl just delegates to the existing `WaitForVmPaused` / `ResumeVm` helpers in `LibretroSaveState.h` — both already handle idempotency and the VM-state handshake from SP6.5.

**Tech Stack:** C++20, Qt 6 (`QtTest`), libretro C ABI, `dlsym` via existing `CoreLoader` helpers, PCSX2 fork at `/Users/mark/Documents/Projects/pcsx2-libretro/`.

**Spec:** `docs/superpowers/specs/2026-05-19-pcsx2-libretro-pause-on-menu-design.md` (commit `147c393`).

**Repos:**
- `/Users/mark/Documents/Projects/RetroNest-Project/` (origin/main, push when done)
- `/Users/mark/Documents/Projects/pcsx2-libretro/` (local-only fork, no push)

---

## Task 1: CoreLoader — add optional `retronest_set_paused` symbol

**Files:**
- Modify: `cpp/src/core/libretro/core_loader.h`
- Modify: `cpp/src/core/libretro/core_loader.cpp`

- [ ] **Step 1: Add typedef + field to the symbol struct**

In `cpp/src/core/libretro/core_loader.h`, find the existing function-pointer struct (the one containing `retro_api_version`, `retro_init`, etc. — likely called `LibretroSymbols` or similar; if the actual name differs, mirror it). Add the new typedef near the existing ones, and add the field at the end of the struct:

```cpp
using retronest_set_paused_t = void (*)(bool);
```

Then in the symbol struct itself (alongside existing function pointers):
```cpp
// Optional. PCSX2 libretro exports this; mGBA and other cores do
// not. CoreLoader resolves via resolveOptional, so this stays
// nullptr when not exported. CoreRuntime checks for null before
// calling.
retronest_set_paused_t retronest_set_paused = nullptr;
```

- [ ] **Step 2: Resolve the symbol in `open()`**

In `cpp/src/core/libretro/core_loader.cpp`, locate the block of `resolveRequired(...)` calls that runs after `dlopen` succeeds (starts around line 28). After the last `resolveRequired`, add:
```cpp
// Optional libretro extensions — silent if the core doesn't export them.
resolveOptional(m_handle, "retronest_set_paused", m_syms.retronest_set_paused);
```

If there are already `resolveOptional` calls present (e.g. for `retronest_get_metrics` or similar private symbols), add this one next to them in alphabetical-ish order. Otherwise add it as the first optional after the required block.

- [ ] **Step 3: Build to verify**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target RetroNest -j4 2>&1 | grep -E "error|Built target RetroNest$" | tail -5
```
Expected: `[100%] Built target RetroNest`. No errors. Existing tests that depend on `CoreLoader` (test_core_loader, test_core_runtime) should still build.

- [ ] **Step 4: Verify existing tests still pass**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target test_core_loader test_core_runtime -j4
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/test_core_loader
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/test_core_runtime
```
Expected: both binaries print `Totals: N passed, 0 failed`. The fake core doesn't yet export `retronest_set_paused`, so `m_syms.retronest_set_paused` resolves to `nullptr` — which is the explicit fallback behavior for non-PCSX2 cores. No test should regress.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/core_loader.h cpp/src/core/libretro/core_loader.cpp
git commit -m "$(cat <<'EOF'
feat(libretro/core-loader): probe optional retronest_set_paused symbol

Adds an optional function-pointer field to the libretro symbol struct
plus a resolveOptional call after dlopen. PCSX2 libretro exports this
symbol so the host can halt PCSX2's internal EE/MTGS/MTVU threads
when the in-game menu opens (current pause path only stops retro_run,
which is insufficient because PCSX2's emulation threads are decoupled
and render directly to the Metal NSView via HW-render).

mGBA and other libretro cores don't export the symbol — resolveOptional
leaves the field nullptr and CoreRuntime checks for null before calling.

CoreRuntime wiring lands in a follow-up task.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: fake_libretro_core — export retronest_set_paused for testing

**Files:**
- Modify: `cpp/tests/fixtures/fake_libretro_core.c`

The CoreRuntime test in Task 3 needs a stub that proves it called `retronest_set_paused` — so we add the export to the fake core with a counter accessor.

- [ ] **Step 1: Add export + counter accessor**

In `cpp/tests/fixtures/fake_libretro_core.c`, add near the bottom of the file (after existing `retro_*` definitions):

```c
// Pause symbol — counter for tests. retronest_set_paused increments
// the counter on every call; tests verify the pointer resolves AND
// is the right function. retronest_test_pause_call_count + reset
// give tests read/reset access between cases.
static int s_pause_call_count = 0;
static int s_last_pause_value = -1;

void retronest_set_paused(bool paused) {
    s_pause_call_count++;
    s_last_pause_value = paused ? 1 : 0;
}

int retronest_test_pause_call_count(void) {
    return s_pause_call_count;
}

int retronest_test_last_pause_value(void) {
    return s_last_pause_value;
}

void retronest_test_reset_pause_counter(void) {
    s_pause_call_count = 0;
    s_last_pause_value = -1;
}
```

(`bool` requires `#include <stdbool.h>` — check whether it's already in the file; if not, add at the top.)

- [ ] **Step 2: Rebuild the fake core**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target fake_libretro_core -j4 2>&1 | grep -E "error|Built target" | tail -3
```
Expected: `Built target fake_libretro_core`.

- [ ] **Step 3: Verify the symbol is exported**

```
nm -gU /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/fake_libretro_core.dylib | grep -E "retronest_set_paused|retronest_test_"
```
Expected: 4 symbols listed (`_retronest_set_paused`, `_retronest_test_pause_call_count`, `_retronest_test_last_pause_value`, `_retronest_test_reset_pause_counter`).

- [ ] **Step 4: Commit**

```bash
git add cpp/tests/fixtures/fake_libretro_core.c
git commit -m "$(cat <<'EOF'
test(fixtures): fake libretro core exports retronest_set_paused

Stub implementation that increments a counter on each call plus
accessors for tests to verify both the pointer resolution and the
call argument. Used by test_core_runtime cases that exercise the
pause/resume code path.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: CoreRuntime — wire pause/resume to call the symbol

**Files:**
- Modify: `cpp/src/core/libretro/core_runtime.cpp`
- Modify: `cpp/tests/test_core_runtime.cpp`

- [ ] **Step 1: Write the failing tests**

In `cpp/tests/test_core_runtime.cpp`, add two new test slots inside the existing test class (after the last `private slots:` test, before the closing `};`).

First, add forward decls of the test accessors at the top of the file (under existing includes):
```cpp
extern "C" {
    int retronest_test_pause_call_count(void);
    int retronest_test_last_pause_value(void);
    void retronest_test_reset_pause_counter(void);
}
```

Then the test cases:
```cpp
    void testPauseCallsRetronestSetPaused() {
        retronest_test_reset_pause_counter();
        CoreRuntime rt;
        CoreRuntime::StartConfig cfg;
        cfg.emuId = "fake_core";
        cfg.corePath = fakeCorePath();
        // Don't fully start — just open the core so symbols resolve.
        QString err;
        QVERIFY2(rt.loader().open(cfg.corePath, &err), qPrintable(err));

        rt.pause();
        QCOMPARE(retronest_test_pause_call_count(), 1);
        QCOMPARE(retronest_test_last_pause_value(), 1);

        rt.resume();
        QCOMPARE(retronest_test_pause_call_count(), 2);
        QCOMPARE(retronest_test_last_pause_value(), 0);
    }
    void testPauseSkipsCallWhenSymbolAbsent() {
        // CoreRuntime with no core opened — symbol pointer is null.
        // pause()/resume() must not crash.
        CoreRuntime rt;
        rt.pause();
        rt.resume();
        QVERIFY(true);  // reached without crash
    }
```

If `CoreRuntime::loader()` doesn't currently exist as a public accessor, check `core_runtime.h` — most likely `loader()` is private. If so, the test needs to go through `start()` properly. As a fallback if direct test access is awkward: collapse the two tests into one that drives a full mini-session and observes counter behavior.

Read the existing `test_core_runtime.cpp` first to see how the fixture starts a session. If there's already a helper that does the start dance, reuse it. If not, the test can call `rt.start(cfg)` and immediately `rt.stop()` after — but make sure `rt.pause()` is called between, before any worker iteration runs.

- [ ] **Step 2: Run tests to verify they fail**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target test_core_runtime -j4 2>&1 | tail -3
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/test_core_runtime
```
Expected: `testPauseCallsRetronestSetPaused` FAILS with `Compared values are not the same: Actual (...): 0  Expected (1): 1` (counter doesn't increment because `pause()` doesn't call the symbol yet). `testPauseSkipsCallWhenSymbolAbsent` passes (no-op currently).

- [ ] **Step 3: Modify `pause()`**

In `cpp/src/core/libretro/core_runtime.cpp`, find `CoreRuntime::pause()` (around line 187). Replace the body with:
```cpp
void CoreRuntime::pause() {
    // NEW: ask the core to halt its internal threads. PCSX2 exports
    // retronest_set_paused which calls VMManager::SetPaused(true)
    // and waits for VMState::Paused via WaitForVmPaused. mGBA / other
    // cores don't export this symbol — the pointer stays null and the
    // existing stop-retro_run + mute-audio behavior is enough.
    if (auto fn = m_loader.symbols().retronest_set_paused)
        fn(true);

    m_paused = true;
    // Silence the SDL audio device too — without this the pre-pause
    // tail of samples queued for playback bleeds through while the
    // worker is blocked on the pause cond.
    m_audio.setPaused(true);
    // Stop any active rumble so motors don't keep running while paused.
    if (m_sdlInput) {
        for (int port = 0; port < InputRouter::NUM_PORTS; ++port) {
            m_sdlInput->setRumbleMotor(port, RETRO_RUMBLE_STRONG, 0);
            m_sdlInput->setRumbleMotor(port, RETRO_RUMBLE_WEAK,   0);
        }
    }
}
```

- [ ] **Step 4: Modify `resume()`**

In the same file, find `CoreRuntime::resume()` (around line 202). Replace with:
```cpp
void CoreRuntime::resume() {
    {
        std::lock_guard<std::mutex> l(m_pauseMx);
        m_paused = false;
    }
    m_audio.setPaused(false);
    m_pauseCv.notify_all();

    // NEW: unblock the core's internal threads. Comes AFTER our worker
    // wakes up so retro_run resumes immediately on the next tick.
    if (auto fn = m_loader.symbols().retronest_set_paused)
        fn(false);
}
```

- [ ] **Step 5: Build and run tests to verify they pass**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target test_core_runtime -j4 2>&1 | grep -E "error|Built target" | tail -3
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/test_core_runtime
```
Expected: both new tests pass. Existing tests continue to pass.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/libretro/core_runtime.cpp cpp/tests/test_core_runtime.cpp
git commit -m "$(cat <<'EOF'
feat(core-runtime): call retronest_set_paused on pause/resume

CoreRuntime::pause() now calls m_loader.symbols().retronest_set_paused
(true) before flipping m_paused, and CoreRuntime::resume() calls it
(false) after clearing m_paused. Ordering matters: the core's threads
should be parked before our worker stops calling retro_run (otherwise
PCSX2's MTGS continues rendering to the Metal NSView via HW-render).
On the resume side, our worker wakes first so retro_run resumes
immediately when the core's threads come back online.

For cores that don't export the symbol (mGBA, RetroArch's stock
cores), the pointer is nullptr and the call is skipped — existing
stop-retro_run + mute-audio behavior is preserved.

Two new test_core_runtime cases pin the contract: counter increments
twice (once per direction) with the right argument value, and a
CoreRuntime with no opened core handles pause/resume without crashing.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: pcsx2-libretro — export `retronest_set_paused`

**This task lives in the `pcsx2-libretro` repo (separate working tree, no `origin` remote — local-only fork).**

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`

- [ ] **Step 1: Add the export**

In `pcsx2-libretro/LibretroFrontend.cpp`, near the bottom of the `extern "C" { ... }` block where the other `RETRO_API` functions live (find by searching for `retro_deinit` or `retro_run`), add:

```cpp
// Host→core pause signal. Not part of libretro spec; RetroNest probes
// for this symbol via dlsym and calls it when the in-game menu opens
// (PCSX2's EE/MTGS/MTVU threads are decoupled from retro_run, so
// pausing the frame pump alone isn't enough — the host needs an
// explicit way to halt internal emulation threads).
//
// Implementation delegates to the SP6.5 save-state pause-handshake
// helpers in LibretroSaveState.h, which already handle idempotency
// and the VMState polling deadline correctly.
//
// Thread expectations: called from the RetroNest GUI thread (Qt slot
// fired by AppController::openLibretroOverlayMenu →
// GameSession::pauseEmulation). s_prev_state is a function-local
// static touched only from that thread.
RETRO_API void retronest_set_paused(bool paused);
void retronest_set_paused(bool paused)
{
    static VMState s_prev_state = VMState::Shutdown;
    if (paused)
    {
        s_prev_state = Pcsx2Libretro::WaitForVmPaused();
    }
    else
    {
        Pcsx2Libretro::ResumeVm(s_prev_state);
        s_prev_state = VMState::Shutdown;
    }
}
```

Ensure `LibretroSaveState.h` is included near the top of `LibretroFrontend.cpp` — if not, add `#include "LibretroSaveState.h"` to the include block.

Also ensure `VMState` is in scope. `LibretroSaveState.h` already includes `pcsx2/VMManager.h` which defines `VMState`, so just including `LibretroSaveState.h` is enough.

- [ ] **Step 2: Rebuild the dylib**

```
cmake --build /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64 --target pcsx2_libretro -j4 2>&1 | grep -E "error|Built target pcsx2_libretro$" | tail -3
```
Expected: `Built target pcsx2_libretro`. No errors.

- [ ] **Step 3: Verify symbol export**

```
nm -gU /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib | grep retronest_set_paused
```
Expected: one line `XXXXXXXXXX T _retronest_set_paused` (the leading `_` is the C name mangling for `extern "C"` symbols on Mach-O).

- [ ] **Step 4: Install over the runtime copy**

```
cp /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
stat -f "%Sm %z bytes" ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```
Expected: mtime updates to current timestamp.

- [ ] **Step 5: Commit (in pcsx2-libretro repo)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/LibretroFrontend.cpp
git commit -m "$(cat <<'EOF'
feat(libretro): export retronest_set_paused for host pause-on-menu

RetroNest probes for this symbol via dlsym and calls it when the
in-game menu opens or closes. Lets the host halt PCSX2's internal
EE/MTGS/MTVU threads — required because PCSX2 renders directly to
the Metal NSView via HW-render mode, so stopping retro_run alone
doesn't freeze gameplay animation. Audio was already muted by
RetroNest; this closes the visual gap.

Implementation delegates to the SP6.5 WaitForVmPaused/ResumeVm
helpers in LibretroSaveState.h. WaitForVmPaused already handles
idempotency (returns immediately if VM is not Running) and the
200 ms polling deadline; ResumeVm only un-pauses if WaitForVmPaused
was the one that paused (prev_state == Running). Saved as a
function-local static so prev_state propagates across paired
pause/resume calls.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

(No push — pcsx2-libretro is a local-only fork.)

---

## Task 5: Build + integration smoke

This task closes out the sub-project. No code changes.

- [ ] **Step 1: Confirm both binaries are fresh**

```
stat -f "%Sm  %N" \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
  ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```
Both mtimes should be from this session (after Tasks 3 + 4 build/install).

- [ ] **Step 2: Run the full RetroNest unit test suite**

```
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64
ctest --output-on-failure 2>&1 | tail -20
```
Expected: same pass rate as before the sub-project. Pre-existing `HotkeyDefs` failure (out of scope for this sub-project) may persist; everything else should pass including the new test_core_runtime cases.

- [ ] **Step 3: Live smoke — PCSX2 pause works**

Launch RetroNest with logging:
```
~/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest 2>&1 | tee /tmp/retronest_pause_smoke.log
```

In RetroNest:
- Launch a PS2 game (R&C 2 if available).
- Once in-game, open the in-game menu (Cmd+Shift+Esc or controller hotkey).
- **Confirm**: gameplay animation freezes immediately (was previously continuing).
- Close the menu.
- **Confirm**: animation resumes from the same point, no glitch or audio pop.
- Open + close menu rapidly 3–5 times — confirm no crash, no audio bleed-through.

- [ ] **Step 4: Live smoke — mGBA still pauses correctly (no regression)**

Without quitting, switch to a GBA ROM in RetroNest (any mGBA-supported game).
- Once in-game, open the in-game menu.
- Confirm gameplay still freezes (mGBA's behavior — unchanged).
- Close menu, confirm resume.
- Quit RetroNest.

- [ ] **Step 5: Push the RetroNest commits**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git log --oneline @{u}..HEAD   # show what's about to ship
git push origin main
```

pcsx2-libretro commits stay local — no remote to push to.

---

## Self-review against the spec

After completing all tasks, do the inline self-review (writing-plans skill, "Self-Review" section). For this plan specifically:

1. **Spec coverage**:
   - CoreLoader symbol field + resolveOptional → Task 1 ✓
   - CoreRuntime pause/resume wiring → Task 3 ✓
   - pcsx2-libretro `retronest_set_paused` export → Task 4 ✓
   - Test coverage (RetroNest side) → Task 2 + Task 3 ✓
   - Integration smoke → Task 5 ✓
   - mGBA non-regression check → Task 5 Step 4 ✓
   - Standalone pcsx2-libretro unit test → **intentionally skipped**; mocking VMManager requires heavy scaffolding, and the dylib build + integration smoke validate the same behaviors.

2. **Placeholder scan**: All code blocks contain real implementations. The one conditional (Task 3 Step 1's "if `loader()` is private, fall back to driving a full mini-session") is a real branch the implementer evaluates against the actual code, not a placeholder.

3. **Type consistency**:
   - `retronest_set_paused_t` typedef → Task 1.
   - `m_syms.retronest_set_paused` field → Task 1, used in Task 3.
   - `Pcsx2Libretro::WaitForVmPaused()` returns `VMState` → matches `VMState s_prev_state` declaration in Task 4.
   - `Pcsx2Libretro::ResumeVm(VMState)` argument matches the static's type.
   - Fake core's `retronest_set_paused(bool)` signature matches the typedef.

Plan complete and saved to `docs/superpowers/plans/2026-05-19-pcsx2-libretro-pause-on-menu.md`.
