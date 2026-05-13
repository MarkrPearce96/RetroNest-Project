# PCSX2 Libretro Cold-Resume (SP6.5 Task 4.5) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire RetroNest's resume-state path through `VMBootParameters::save_state` so PCSX2's `VMManager::Initialize` loads the state at the correct point (after full BIOS init / ELF discovery), closing the SP6.5 Task 4 cold-resume gap that produces a TLB-miss loop on R&C 2.

**Architecture:** New private env call `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH = 0x02 | RETRO_ENVIRONMENT_PRIVATE` (= `0x20002`). RetroNest writes the resume path into `EnvironmentContext::bootStatePath` before `retro_load_game`; pcsx2-libretro queries the env during `retro_load_game` and, if a non-empty path comes back, sets `params.save_state = path`. The env handler clears the path on read to mark consumed — RetroNest's existing post-load `retro_unserialize` block only fires when the path is still set (preserves mGBA cold-resume behavior bit-for-bit).

**Tech Stack:** C++17, Qt 6 (`QByteArray`, RetroNest side), libretro env-callback ABI, PCSX2 `VMBootParameters::save_state` mechanism. Build: `cmake` per slice (arm64 + x86_64), then macdeployqt + lipo-merge into the universal `.app` (RetroNest changes) or lipo-merge directly into `~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib` (pcsx2-libretro changes). Smoke test: R&C 2 inside RetroNest under `arch -x86_64` with `RETRONEST_STATE_TRACE=1`.

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-13-pcsx2-libretro-cold-resume-design.md` (commit `69779f6`).

**Working directories:**
- pcsx2-libretro: `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `retronest-libretro`, base `2764d75d2`)
- RetroNest-Project: `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`, base `69779f6`)

---

## Task 1: RetroNest env-handler scaffolding (no behavior change)

**What this builds:** Declares the new private env constant, adds `QByteArray bootStatePath` field to `EnvironmentContext`, and adds the dispatch case in `environment_callbacks.cpp`. After this commit, the env call is plumbed but never queried — `bootStatePath` stays empty forever, so nothing changes at runtime. Verifies the header + dispatch compile cleanly and don't break the existing libretro-env unit tests (if any).

**Files:**
- Modify: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/environment_callbacks.h` (add #define + field)
- Modify: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/environment_callbacks.cpp` (add dispatch case)

- [ ] **Step 1.1: Add the env constant + field to `environment_callbacks.h`**

In `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/environment_callbacks.h`, locate the existing `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` block (lines 5-12). Append a second private env constant right after it (before `#include <QByteArray>` at line 14):

```cpp
// 0x20002 — RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH
//           Used by pcsx2_libretro to receive a resume-state path from
//           RetroNest BEFORE the core's VM init runs, so PCSX2 can load
//           the state via VMBootParameters::save_state — i.e., after
//           full BIOS init / ELF discovery, which is the only ordering
//           that produces a runnable VM for cold-resume on launch.
//           Output is a `const char**` written with a UTF-8 path. The
//           env handler clears EnvironmentContext::bootStatePath on
//           read to mark consumed; RetroNest's CoreRuntime then knows
//           to skip the legacy post-load retro_unserialize block.
//           Returns false if no path is set (mGBA / fresh-boot cases).
#define RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH (2 | RETRO_ENVIRONMENT_PRIVATE)
```

Then locate the `EnvironmentContext` struct (line 21-41) and add a new field. Place it next to the other path/byte-array fields near the top of the struct so it sits with peers conceptually. Insert after the `saveDirectory` line (line 23):

```cpp
    // SP6.5 Task 4.5: resume-state path the libretro core consumes
    // synchronously during retro_load_game via
    // RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH. Set by CoreRuntime::runLoop
    // before retro_load_game; cleared on first env query (one-shot). If
    // still set after retro_load_game returns, CoreRuntime falls back
    // to the legacy post-load retro_unserialize path (mGBA / cores that
    // don't query the new env).
    QByteArray bootStatePath;
```

- [ ] **Step 1.2: Add the dispatch case to `environment_callbacks.cpp`**

In `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/environment_callbacks.cpp`, locate the existing `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` case in `environmentDispatch` (lines 91-108). Insert a new case immediately after it, before `case RETRO_ENVIRONMENT_GET_VARIABLE:` at line 109:

```cpp
        case RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH: {
            // SP6.5 Task 4.5: one-shot delivery of a resume-state path
            // to the libretro core during retro_load_game. Returning
            // true marks the path consumed (cleared in-place) so
            // CoreRuntime knows to skip the legacy post-load
            // retro_unserialize block.
            if (!data) {
                qWarning("[libretro/env] GET_BOOT_STATE_PATH: data=null");
                return false;
            }
            if (ctx->bootStatePath.isEmpty()) {
                // No path set, or already consumed by a prior call.
                return false;
            }
            auto** out = static_cast<const char**>(data);
            *out = ctx->bootStatePath.constData();
            // NOTE: the constData() pointer is only stable until the
            // QByteArray is mutated. Caller must use synchronously
            // before this function returns to the env-callback site.
            // Mark consumed AFTER setting *out — clear() is a no-op
            // on the pointer the caller now holds (QByteArray COW)
            // but the next call will return false.
            ctx->bootStatePath.clear();
            qInfo("[libretro/env] GET_BOOT_STATE_PATH: handed off path to core");
            return true;
        }
```

- [ ] **Step 1.3: Build arm64 to verify the headers compile**

Run:

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -arm64 /opt/homebrew/bin/cmake --build cpp/build-arm64 -j 4 2>&1 | tail -8
```

Expected: `[100%] Built target RetroNest`. No errors related to `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH` or `bootStatePath`. (Linker warnings about macOS version mismatches in homebrew dylibs are pre-existing baseline noise — ignore.)

- [ ] **Step 1.4: Build x86_64**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 4 2>&1 | tail -5
```

Expected: `[100%] Built target RetroNest`.

- [ ] **Step 1.5: Verify the existing libretro-env tests still pass**

There's a `test_environment_callbacks` unit-test target compiled from `cpp/src/core/libretro/test_environment_callbacks.cpp`. Confirm it still builds and runs cleanly with the new dispatch case:

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -arm64 /opt/homebrew/bin/cmake --build cpp/build-arm64 --target test_environment_callbacks -j 4 2>&1 | tail -5
```

If the target builds successfully and produces a binary at `cpp/build-arm64/test_environment_callbacks` (or similar — check via `find cpp/build-arm64 -name test_environment_callbacks -type f 2>/dev/null | head -1`), run it:

```bash
$(find /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64 -name test_environment_callbacks -type f -perm +111 2>/dev/null | head -1)
```

Expected: tests pass (exit code 0). If the test target doesn't exist or doesn't have any tests touching our new case, this step is a no-op — proceed to commit.

- [ ] **Step 1.6: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/environment_callbacks.h cpp/src/core/libretro/environment_callbacks.cpp
git commit -m "$(cat <<'EOF'
SP6.5 Task 4.5: env-handler scaffolding for cold-resume

Declares RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH = 0x20002 in the
RetroNest-private env namespace (0x20000) — 0x20001 is already taken
by RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW; 0x20002 is the next free
slot. Adds QByteArray bootStatePath to EnvironmentContext and the
dispatch case in environment_callbacks.cpp.

One-shot protocol: env handler returns the path's constData() pointer
to the caller and immediately clears bootStatePath. Mid-call lifetime
is bounded by the synchronous env-callback chain; the caller (the
core's retro_load_game) is expected to copy the bytes into
VMBootParameters::save_state before returning.

No behavior change in this commit — bootStatePath is never set, so
the new dispatch case always returns false. CoreRuntime wiring and
the pcsx2-libretro consumer land in follow-up commits.

Spec: docs/superpowers/specs/2026-05-13-pcsx2-libretro-cold-resume-design.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: RetroNest core_runtime wiring (no behavior change)

**What this builds:** Sets `m_envCtx.bootStatePath` from `m_cfg.resumeStatePath` before `retro_load_game`, then gates the existing post-load `retro_unserialize` block on the path *not* being consumed. Nothing yet queries the env call (the pcsx2-libretro side lands in Task 3), so `bootStatePath` is set, stays set through `retro_load_game`, and the existing legacy block continues to fire. Behavior remains identical for both PCSX2 and mGBA.

**Files:**
- Modify: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp` (two small edits)

- [ ] **Step 2.1: Set `bootStatePath` before `retro_load_game`**

In `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp`, locate lines 274-277 (the `m_envCtx` initialization block right before `auto& s = m_loader.symbols();` at line 279). Add a new line so the block reads:

```cpp
    m_envCtx.systemDirectory = m_cfg.systemDir.toUtf8();
    m_envCtx.saveDirectory   = m_cfg.saveDir.toUtf8();
    m_envCtx.options         = &m_options;
    m_envCtx.runtime         = static_cast<void*>(this);
    // SP6.5 Task 4.5: one-shot delivery to the libretro core via
    // RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH during retro_load_game.
    // Cleared in the env handler when consumed; left set if the core
    // doesn't query (mGBA), which makes the legacy retro_unserialize
    // block below fire as before.
    m_envCtx.bootStatePath   = m_cfg.resumeStatePath.toUtf8();
```

- [ ] **Step 2.2: Gate the legacy `retro_unserialize` block**

Still in `core_runtime.cpp`, locate the existing block at lines 315-321:

```cpp
    if (!m_cfg.resumeStatePath.isEmpty()) {
        QFile f(m_cfg.resumeStatePath);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            QByteArray state = f.readAll();
            s.retro_unserialize(state.constData(), static_cast<size_t>(state.size()));
        }
    }
```

Replace it with the consumption-aware version:

```cpp
    // SP6.5 Task 4.5: cold-resume fallback path.
    //
    // If the core consumed RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH during
    // retro_load_game (pcsx2-libretro does), m_envCtx.bootStatePath was
    // cleared and the VM is already in the loaded state via
    // VMBootParameters::save_state → VMManager::Initialize → DoLoadState.
    // Skip the post-load retro_unserialize.
    //
    // If the core didn't query (mGBA and any non-PCSX2 libretro core),
    // bootStatePath is still set — fall back to the legacy
    // retro_unserialize path that loads the state AFTER retro_load_game.
    // mGBA's BIOS-init is trivial enough that this works for it.
    if (!m_cfg.resumeStatePath.isEmpty() && !m_envCtx.bootStatePath.isEmpty()) {
        QFile f(m_cfg.resumeStatePath);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            QByteArray state = f.readAll();
            s.retro_unserialize(state.constData(), static_cast<size_t>(state.size()));
        }
    }
```

The first guard (`!m_cfg.resumeStatePath.isEmpty()`) preserves the existing no-resume fast path. The second (`!m_envCtx.bootStatePath.isEmpty()`) is the new "core didn't consume" check.

- [ ] **Step 2.3: Build arm64**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -arm64 /opt/homebrew/bin/cmake --build cpp/build-arm64 -j 4 2>&1 | tail -5
```

Expected: `[100%] Built target RetroNest`.

- [ ] **Step 2.4: Build x86_64**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 4 2>&1 | tail -5
```

Expected: `[100%] Built target RetroNest`.

- [ ] **Step 2.5: Re-run macdeployqt on each slice and lipo-merge**

Incremental rebuilds need macdeployqt to re-fix the Qt rpath, otherwise the launched binary tries to load the system Qt at `/usr/local/Cellar/qtbase/...` in parallel with the bundled Qt — produces objc "Class implemented in both" warnings and cocoa plugin init failure.

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
/opt/homebrew/opt/qt/bin/macdeployqt cpp/build-arm64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite 2>&1 | tail -3
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite 2>&1 | tail -3
./scripts/lipo-merge-app.sh \
    cpp/build-arm64/RetroNest.app \
    cpp/build-x86_64/RetroNest.app \
    cpp/build-universal/RetroNest.app \
    cpp/resources/RetroNest.entitlements 2>&1 | tail -3
```

Expected: each macdeployqt completes without crashing; lipo-merge ends with `✓ wrote universal cpp/build-universal/RetroNest.app`.

- [ ] **Step 2.6: Smoke test — confirm no regression on mGBA + PCSX2**

Goal: prove this commit changes nothing at runtime. Both mGBA and PCSX2 cold-resume should behave exactly as they did at the SP6.5 head — mGBA resumes cleanly via the legacy `retro_unserialize` fallback (because nothing consumes the env yet, `bootStatePath` stays set), and PCSX2 reproduces the SP6.5 Task 4 TLB-miss black-screen (which Task 3 will fix).

This is a structural-no-op verification. If you have time:

```bash
pkill -9 -f "MacOS/RetroNest" 2>/dev/null; sleep 1; rm -f /tmp/retronest_sp65_t45_t2.log
arch -x86_64 env RETRONEST_STATE_TRACE=1 /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > /tmp/retronest_sp65_t45_t2.log 2>&1 &
disown
```

Launch any game (mGBA preferred since PCSX2's cold-resume still fails). Save & exit. Force-kill RetroNest (`pkill -9`). Relaunch, Resume.

Inspect log:

```bash
grep -E "Unserialize:|GET_BOOT_STATE_PATH|VM RUNNING|cold-boot via" /tmp/retronest_sp65_t45_t2.log | head -10
```

Expected:
- For mGBA-based games: legacy `retro_unserialize` block fires (no PCSX2 `[STATE_TRACE]` lines because mGBA doesn't trace; resume works visually).
- NO `[libretro/env] GET_BOOT_STATE_PATH` lines because nothing queries the new env yet.
- NO `cold-boot via save state` log lines (the pcsx2-libretro consumer doesn't exist yet).

If the smoke test isn't easy to run (e.g., no mGBA games on hand), proceed to commit — the build + macdeployqt + lipo-merge succeeding is sufficient evidence for a no-op commit. The full round-trip happens after Task 3.

- [ ] **Step 2.7: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/core_runtime.cpp
git commit -m "$(cat <<'EOF'
SP6.5 Task 4.5: CoreRuntime wiring for cold-resume env handoff

Sets EnvironmentContext::bootStatePath from m_cfg.resumeStatePath
before retro_load_game so the libretro core can consume it via
RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH. Gates the existing
post-load retro_unserialize block on bootStatePath being non-empty
(i.e., NOT consumed by the core).

No behavior change in this commit — nothing queries the env call yet.
PCSX2 reproduces the SP6.5 Task 4 cold-resume black screen via the
legacy fallback; mGBA cold-resume works via the legacy fallback,
which is its only path. The pcsx2-libretro consumer that triggers
the new code path lands in the next commit.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: pcsx2-libretro query + `params.save_state` (behavior change)

**What this builds:** pcsx2-libretro's `retro_load_game` queries `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH`. If a non-empty path comes back, sets `params.save_state = path` before calling `emu.Start(params)`. `VMManager::Initialize` then runs `DoLoadState` at the end of full BIOS init / ELF discovery (per `VMManager.cpp:1636-1643`), the only ordering that produces a runnable VM for cold-resume. This is the commit that flips the cold-resume path from broken to working.

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp` (define env constant + query + assign `params.save_state`)

- [ ] **Step 3.1: Define the env constant near other libretro env-related code**

In `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`, locate the namespace scope (around line 33 where `namespace {` opens, or wherever's idiomatic for adding a file-local `constexpr`). Add the constant just below the existing includes/atomics block (e.g., right after `std::atomic<bool> g_memory_map_issued{false};` at line 119, before `IsStateTraceEnabled()` at line 128):

```cpp
// SP6.5 Task 4.5: private env call agreed with RetroNest. RetroNest
// stores the resume-state path in EnvironmentContext::bootStatePath
// before retro_load_game; we query during retro_load_game and, if a
// path comes back, set params.save_state so VMManager::Initialize
// loads the state via DoLoadState after full BIOS init / ELF discovery
// (pcsx2/VMManager.cpp:1636-1643) — the only ordering that produces a
// runnable VM for cold-resume on launch.
//
// Number must match RetroNest's environment_callbacks.h define exactly.
// RETRO_ENVIRONMENT_PRIVATE = 0x20000 per libretro.h; 0x20001 is
// already used by RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW; 0x20002 is
// the next free RetroNest-private slot.
constexpr unsigned RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH =
    (2u | RETRO_ENVIRONMENT_PRIVATE);
```

`RETRO_ENVIRONMENT_PRIVATE` comes from `libretro.h` which is already included at line 11.

- [ ] **Step 3.2: Query the env and set `params.save_state` in `retro_load_game`**

Still in `LibretroFrontend.cpp`, locate the `retro_load_game` block where `VMBootParameters` is built (lines 403-406):

```cpp
    // 3. Build VMBootParameters and start the emu thread.
    VMBootParameters params{};
    params.filename = game->path;
    params.fast_boot = true;
```

Insert the env query between `params.fast_boot = true;` and the `EmuThread& emu = ...` call. The block becomes:

```cpp
    // 3. Build VMBootParameters and start the emu thread.
    VMBootParameters params{};
    params.filename = game->path;
    params.fast_boot = true;

    // SP6.5 Task 4.5: cold-resume on launch.
    //
    // RetroNest stashes a resume-state path in its EnvironmentContext
    // BEFORE calling retro_load_game (CoreRuntime::runLoop). We query
    // it here; if a non-empty path comes back, we set params.save_state
    // so VMManager::Initialize runs DoLoadState at the end of full VM
    // init (after BIOS init / ELF discovery / s_fast_boot_requested) —
    // exactly the ordering PCSX2-Qt uses when launching with a state.
    //
    // The env handler clears the path on read so RetroNest's post-
    // retro_load_game legacy retro_unserialize block skips itself; the
    // load happened inside VMManager::Initialize and re-loading would
    // race the freshly-loaded VM state.
    //
    // mGBA and other cores that don't define this env call get
    // false from the env_cb, take the empty-path fast path here, and
    // RetroNest's legacy retro_unserialize block handles their cold-
    // resume as before. Backward-compatible by construction.
    if (g_frontend.environ_cb)
    {
        const char* boot_state = nullptr;
        if (g_frontend.environ_cb(RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH,
                                  &boot_state) &&
            boot_state && boot_state[0] != '\0')
        {
            // Copy into params.save_state immediately — the env handler's
            // returned pointer is only stable for the duration of this
            // env_cb call (the QByteArray was cleared on read in
            // environment_callbacks.cpp; constData() may move when
            // QByteArray reallocates after a later mutation).
            params.save_state = boot_state;
            FrontendLog(RETRO_LOG_INFO,
                "retro_load_game: cold-boot via save state %s",
                params.save_state.c_str());
        }
    }

    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    const bool ok = emu.Start(params);
```

Do NOT change `params.fast_boot = true` — PCSX2's `DoLoadState` runs after fast_boot logic regardless; the loaded state's PC overrides the fast-boot ELF entry point.

- [ ] **Step 3.3: Build arm64**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
arch -arm64 /opt/homebrew/bin/cmake --build build-arm64 --target pcsx2_libretro -j 4 2>&1 | tail -8
```

Expected: `[100%] Built target pcsx2_libretro`. (clangd LSP diagnostics for PCSX2 internal headers are baseline noise per project memory — only the cmake build counts.)

- [ ] **Step 3.4: Build x86_64**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
arch -x86_64 /usr/local/bin/cmake --build build-x86_64 --target pcsx2_libretro -j 4 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`.

- [ ] **Step 3.5: Lipo-merge into the install path**

```bash
/Users/mark/Documents/Projects/RetroNest-Project/scripts/lipo-merge-dylib.sh \
    /Users/mark/Documents/Projects/pcsx2-libretro/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
    /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib \
    /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib 2>&1 | tail -3
```

Expected: `✓ wrote universal /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib`. The Task 2 RetroNest.app is already installed at `cpp/build-universal/RetroNest.app` — no need to rebuild RetroNest for this commit.

- [ ] **Step 3.6: Smoke test 1 — cold-resume restore (primary)**

```bash
pkill -9 -f "MacOS/RetroNest" 2>/dev/null; sleep 1; rm -f /tmp/retronest_sp65_t45_t3.log
arch -x86_64 env RETRONEST_STATE_TRACE=1 /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > /tmp/retronest_sp65_t45_t3.log 2>&1 &
disown
```

In RetroNest:
1. Launch R&C 2. Wait for game to boot past the BIOS into the title screen / first FMV / actual gameplay (any post-VM-RUNNING point).
2. Note a recognizable visual cue (FMV frame number, menu position, etc.).
3. In-game menu (Cmd+Shift+Escape) → Save & Exit.
4. **The game session will hang on close (pre-existing SP3.6 limitation).** Force-recover:
   ```bash
   pkill -9 -f "MacOS/RetroNest" 2>/dev/null
   ```
5. Relaunch RetroNest from the same terminal command above (start a new log path so we capture only the resume launch — easier to read):
   ```bash
   rm -f /tmp/retronest_sp65_t45_t3_resume.log
   arch -x86_64 env RETRONEST_STATE_TRACE=1 /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > /tmp/retronest_sp65_t45_t3_resume.log 2>&1 &
   disown
   ```
6. Launch R&C 2 from RetroNest's home screen.
7. **Expect** the `ResumeStateDialog` to appear. Pick **Resume**.

Inspect the resume log:

```bash
grep -E "cold-boot via|retro_load_game|VM RUNNING|GET_BOOT_STATE_PATH|Unserialize:|TLB Miss" /tmp/retronest_sp65_t45_t3_resume.log | head -20
```

**Expected output:**
- `[libretro/env] GET_BOOT_STATE_PATH: handed off path to core` — RetroNest's env handler logged the consumption.
- `[core] [pcsx2_libretro] retro_load_game: cold-boot via save state /Users/mark/Documents/RetroNest/emulators/pcsx2-libretro/ps2/savestates/SCUS_972.68.resume` — our log line confirming `params.save_state` was set.
- `[core] [pcsx2_libretro] retro_load_game: VM started successfully` — VMManager::Initialize returned StartupSuccess (the DoLoadState inside Initialize completed).
- `[core] [pcsx2_libretro] VM RUNNING — title=Ratchet & Clank 2 - Going Commando` — EE thread reached steady state.
- **NO `[STATE_TRACE] Unserialize: start`** lines (the legacy post-load path was correctly skipped).
- **NO `TLB Miss`** lines.

**Visual expectation:** game window opens directly to the save-point position (no BIOS / Sony intro / title screen if those were past at save time). Game plays normally.

If the log shows `cold-boot via save state` but the game window is still black, something is wrong INSIDE PCSX2's `DoLoadState` for the saved-state file format. Capture the full log and stop — do NOT commit until this is investigated.

- [ ] **Step 3.7: Smoke test 2 — fresh-boot regression (Start Fresh path)**

Pre-condition: a valid resume file exists (from step 3.6). If you didn't reach step 3.6 yet, save & exit one more time so we have a fresh `.resume` file.

Restart RetroNest. Launch R&C 2. When the Resume dialog appears, pick **Start Fresh**.

Inspect:

```bash
grep -E "cold-boot via|retro_load_game|VM RUNNING|GET_BOOT_STATE_PATH" /tmp/retronest_sp65_t45_t3_resume.log | tail -10
```

**Expected:**
- NO `cold-boot via save state` line (because RetroNest didn't set `m_cfg.resumeStatePath` for the Start Fresh path, so `bootStatePath` was empty, env handler returned false).
- `retro_load_game: VM started successfully` and `VM RUNNING` fire normally.
- Game boots through Sony/BIOS intro from scratch.

If you see `cold-boot via save state` on the Start Fresh path, that's a RetroNest-side bug (something is still setting `cfg.resumeStatePath` for that path). Capture the log and stop.

- [ ] **Step 3.8: Smoke test 3 — in-session save/load regression**

After step 3.7 reaches VM RUNNING (fresh boot), trigger save state via in-game menu. Advance gameplay ~10 s. Trigger load state.

Inspect:

```bash
grep -E "SerializeSize:|Serialize:|Unserialize:|cold-boot via" /tmp/retronest_sp65_t45_t3_resume.log | tail -15
```

**Expected:**
- `[STATE_TRACE] SerializeSize: probe start` + `probed=42354227 bytes` — same as SP6.5.
- `[STATE_TRACE] Serialize: start len=42354227` + `done ok=1` — same as SP6.5.
- `[STATE_TRACE] Unserialize: start len=42354227` + `done ok=1` — same as SP6.5.
- NO additional `cold-boot via save state` line for the load (we're not in `retro_load_game`).

Game visually round-trips. If anything in this regression test fails, it means Task 4.5 broke the SP6.5 in-session path — stop and investigate before committing.

- [ ] **Step 3.9: Smoke test 4 — mGBA cold-resume regression (if you have a GBA game on hand)**

Optional but recommended. Launch any GBA game. Save & Exit. Force-recover (`pkill -9`). Relaunch. Resume.

Inspect:

```bash
grep -E "cold-boot via|GET_BOOT_STATE_PATH|Unserialize|mgba" /tmp/retronest_sp65_t45_t3_resume.log | tail -10
```

**Expected:**
- NO `cold-boot via save state` line (mGBA doesn't define the env call).
- NO `GET_BOOT_STATE_PATH: handed off` line (mGBA never queries it).
- Game resumes at save position via mGBA's legacy `retro_unserialize` path.

If mGBA's cold-resume breaks, that means the Task 2 gate on `bootStatePath.isEmpty()` got the polarity wrong — investigate before committing.

If you don't have a GBA game readily available, skip this smoke test. The protocol's backward-compatibility comes from the fact that mGBA simply doesn't issue the new env call — there's no way for Task 4.5 to affect mGBA paths unless RetroNest's plumbing has a polarity bug, which Task 2's no-op verification should have already caught.

- [ ] **Step 3.10: Commit (only if smoke tests 1, 2, 3 all passed)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/LibretroFrontend.cpp
git commit -m "$(cat <<'EOF'
SP6.5 Task 4.5: cold-resume via params.save_state

retro_load_game queries RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH
(0x20002 in the RetroNest-private 0x20000 env range). If RetroNest
handed off a non-empty resume-state path, set params.save_state so
VMManager::Initialize runs DoLoadState at the end of full VM init
(pcsx2/VMManager.cpp:1636-1643) — after BIOS init / ELF discovery,
before VM enters Running. This is the only ordering that produces
a runnable VM for cold-resume on launch.

Closes the SP6.5 Task 4 known limitation:
  - Pre-fix: retro_unserialize ran AFTER retro_load_game returned,
    against a freshly fast-booted VM mid-BIOS-init. The loaded
    state's TLB/COP0 stamps clashed with the half-initialized
    memory map → infinite TLB-miss loop on 0x10002000, black screen.
  - Post-fix: DoLoadState runs INSIDE VMManager::Initialize after
    BIOS init is complete. Same canonical path PCSX2-Qt uses when
    launching with a save state.

Backward-compatible: mGBA and any other libretro core that doesn't
query the new env call gets false from env_cb, falls through to the
empty-path branch here, and RetroNest's legacy post-load
retro_unserialize block handles their cold-resume as before.

Smoke-tested on R&C 2 (under Rosetta arch -x86_64):
  - Cold-resume: Save & Exit, relaunch, Resume → game resumes at
    save position. retro_load_game log line "cold-boot via save
    state /.../SCUS_972.68.resume" fires; VM RUNNING fires; NO
    legacy [STATE_TRACE] Unserialize line; NO TLB Miss spam.
  - Fresh boot regression: Start Fresh from Resume dialog → no
    cold-boot log line; normal Sony/BIOS intro plays.
  - In-session save/load regression: SP6.5 main feature still works
    cleanly (probe-once 42354227 bytes, Serialize ok=1, Unserialize
    ok=1, mid-session round-trip).

SP6.5 fully complete with Task 4.5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Post-implementation cleanup

After Task 3 commits cleanly:

- **Update memory:** edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_sp65_shipped.md` and `project_pcsx2_libretro_port.md` to mark Task 4.5 ✅ DONE. Move the known-limitation note from "Task 4.5 next" to "Task 4.5 done — cold-resume working end-to-end."
- **Documentation marker:** if there's a CHANGELOG / NEWS / release-notes file, add a line. (Project memory says there isn't one; skip.)
- **Universal build smoke-verify:** the lipo-merged dylib should ship in `~/Documents/RetroNest/emulators/libretro/cores/`. Confirm `file` reports `universal binary with 2 architectures`.

## Risks and mitigations recap

| Risk | Mitigation |
|---|---|
| Env number collision | `0x20002` documented as next free RetroNest-private slot; comment markers in both files name the constant |
| `DoLoadState` failure on corrupt resume file | `VMManager::Initialize` shuts the VM down + returns `StartupFailure`; our `retro_load_game` returns false; existing toast path surfaces the failure |
| Lifetime of `const char*` returned by env_cb | Spec'd as synchronous-use-only; our `retro_load_game` copies into `params.save_state` (std::string) before the env call returns |
| mGBA cold-resume regression | mGBA doesn't issue the new env call → bootStatePath stays set → legacy block fires unchanged |
| Polarity bug in Task 2's gate | Task 2 no-op smoke test (step 2.6) verifies mGBA still resumes |
| Fast-boot + state-load interaction | `s_fast_boot_requested` is set INDEPENDENTLY of `state_to_load`; `DoLoadState` runs AFTER fast-boot logic and overwrites the state — see VMManager.cpp:1471, 1636-1643 |

## Definition of done

- [ ] All 3 task commits land on `retronest-libretro` (pcsx2-libretro) and `main` (RetroNest-Project).
- [ ] Universal `pcsx2_libretro.dylib` installed and lipo-verified.
- [ ] Universal `RetroNest.app` rebuilt + macdeployqt'd + lipo-merged.
- [ ] Smoke test 1 (cold-resume restore) passes on R&C 2.
- [ ] Smoke test 2 (Start Fresh fresh-boot regression) passes on R&C 2.
- [ ] Smoke test 3 (in-session save/load regression) passes on R&C 2.
- [ ] Both repos clean; only pre-existing untracked entries (`pcsx2-libretro/tools/resources` symlink) remain.
