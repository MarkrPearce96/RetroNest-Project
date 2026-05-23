# PPSSPP Quit render-fence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate the PPSSPP Quit / Save & Quit crash by adding a render-thread synchronization gate in `GameSession::kill()` and `GameSession::terminate()` that clears `LibretroGLItem` and waits for the Qt scene graph to discard the MTLTexture-wrapping node before letting the worker tear down `VideoHardwareGL`.

**Architecture:** New `GameSession::preShutdownRenderFence()` helper runs only for the libretro GL backend. It calls `LibretroGLItem::setVideoHardware(nullptr)`, then runs a bounded `QEventLoop` that quits after two `QQuickWindow::afterRendering` signals (with a 500 ms hard cap). The QML side registers/unregisters the `LibretroGLItem*` via a new `Q_INVOKABLE` on `GameSession`. Threading model is unchanged — `VideoHardwareGL` still lives on and is destroyed by the worker.

**Tech Stack:** C++17, Qt 6 (QObject, QPointer, QEventLoop, QTimer, QQuickWindow, QSG), QML (Component.onCompleted/onDestruction), CMake build via macdeployqt + ad-hoc codesign.

**Reference docs:**
- Design spec: `docs/superpowers/specs/2026-05-23-ppsspp-quit-render-fence-design.md`
- Memory note: `~/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/libretro-gl-metal-teardown-race.md`

---

## File map

| File | Action | Responsibility |
|---|---|---|
| `cpp/src/core/game_session.h` | Modify | Forward-declare `LibretroGLItem`; add `m_libretroGLItem` field, `registerLibretroGLItem` Q_INVOKABLE, `preShutdownRenderFence` private helper |
| `cpp/src/core/game_session.cpp` | Modify | Include `libretro_gl_item.h`, `<QTimer>`, `<QQuickWindow>`; implement `registerLibretroGLItem` + `preShutdownRenderFence`; call fence from `kill()` and `terminate()` |
| `cpp/qml/AppUI/EmulationView.qml` | Modify | In `glComponent`'s `LibretroGLItem`, register with GameSession on `Component.onCompleted`; clear registration on `Component.onDestruction` |

No CMake changes (both files are already in build targets, verified).

---

## Task 1: Add registration API to GameSession (header)

**Files:**
- Modify: `cpp/src/core/game_session.h`

- [ ] **Step 1: Add forward declaration**

After the existing forward declarations (currently at lines 10-13: `EmulatorAdapter`, `LibretroAdapter`, `SdlInputManager`, `FrontendSettingsStore`), add:

```cpp
class LibretroGLItem;
```

- [ ] **Step 2: Add `<QPointer>` include**

In the `#include` block near the top (after `<QObject>`), add:

```cpp
#include <QPointer>
```

- [ ] **Step 3: Add `registerLibretroGLItem` declaration**

Find the existing `Q_INVOKABLE void registerHardwareView(qulonglong view_ptr);` declaration (around line 124 in current file). Immediately after it, add:

```cpp
    /** Register (or clear) the LibretroGLItem* for the active GL hardware-render
     *  item. Called from QML via Component.onCompleted; pass nullptr from
     *  Component.onDestruction. Used by preShutdownRenderFence() on kill() /
     *  terminate() to synchronize scene graph cleanup with worker-side
     *  VideoHardwareGL destruction (fixes the PPSSPP Quit crash — see
     *  docs/superpowers/specs/2026-05-23-ppsspp-quit-render-fence-design.md).
     */
    Q_INVOKABLE void registerLibretroGLItem(QObject* item);
```

- [ ] **Step 4: Add `m_libretroGLItem` field**

Find the private section near the bottom of the class. Add this field (group it near other libretro-specific fields like `m_libretroAdapter` if such grouping exists; otherwise add it before the closing `};`):

```cpp
    // Set by registerLibretroGLItem from QML. Used only by
    // preShutdownRenderFence() — QPointer auto-clears if the item is
    // destroyed before we register null.
    QPointer<LibretroGLItem> m_libretroGLItem;
```

- [ ] **Step 5: Add `preShutdownRenderFence` private declaration**

In the same private section, near the other private helpers, add:

```cpp
    /** Pre-teardown synchronization for the libretro GL backend. Clears the
     *  LibretroGLItem's reference to VideoHardwareGL and waits (bounded) for
     *  the scene graph to discard the QSGSimpleTextureNode that wraps the
     *  IOSurface as an MTLTexture. Without this, QSGRenderThread can render
     *  a stale MTLTexture after the worker has released the backing IOSurface
     *  (EXC_BAD_ACCESS in AGX setFragmentTextures+154). No-op for non-GL
     *  backends or when no glItem is registered. */
    void preShutdownRenderFence();
```

- [ ] **Step 6: Verify header still parses**

Build only — don't run yet. From the repo root:

```bash
cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -40
```

Expected: compile error for unimplemented `registerLibretroGLItem` is fine (next task implements it). If you see any *other* error — unknown type, missing include, multiple-definition — fix before moving on.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/core/game_session.h
git commit -m "$(cat <<'EOF'
feat(libretro): declare GameSession render-fence API for GL backend

Adds registerLibretroGLItem (Q_INVOKABLE) and preShutdownRenderFence (private)
declarations. Implementation lands in the next commit. Part of the architectural
fix for the PPSSPP Quit crash documented in
docs/superpowers/specs/2026-05-23-ppsspp-quit-render-fence-design.md.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Implement registerLibretroGLItem

**Files:**
- Modify: `cpp/src/core/game_session.cpp`

- [ ] **Step 1: Add includes**

Find the existing `#include` block (lines 1-20 of current file). Add these in alphabetic position:

After `#include "core/libretro/video_hardware_gl.h"` (which is already there, line 9), add:

```cpp
#include "ui/libretro/libretro_gl_item.h"
```

After `#include <QEventLoop>` (line 17), add (alphabetic):

```cpp
#include <QQuickWindow>
#include <QTimer>
```

- [ ] **Step 2: Implement `registerLibretroGLItem`**

Place this implementation immediately after the existing `void GameSession::registerHardwareView(qulonglong view_ptr) { ... }` definition. Find `registerHardwareView` first (grep for it in the file), then add the new function below it:

```cpp
void GameSession::registerLibretroGLItem(QObject* item) {
    // qobject_cast returns nullptr if item is null or not a LibretroGLItem.
    // QPointer accepts that directly — the field self-clears on destruction
    // too, so the explicit null call from QML's Component.onDestruction is
    // belt-and-suspenders.
    m_libretroGLItem = qobject_cast<LibretroGLItem*>(item);
    if (item && !m_libretroGLItem) {
        qWarning() << "[GameSession] registerLibretroGLItem: object is not a "
                      "LibretroGLItem, ignoring";
    }
}
```

- [ ] **Step 3: Build**

```bash
cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -40
```

Expected: build succeeds. Symbol for `preShutdownRenderFence` will be missing because it's declared but not yet defined — that's fine if you didn't call it from anywhere yet (we haven't). If the linker errors anyway, check that no other file references the symbol.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/game_session.cpp
git commit -m "$(cat <<'EOF'
feat(libretro): implement GameSession::registerLibretroGLItem

QML registers / unregisters the LibretroGLItem* used by the upcoming
preShutdownRenderFence. Stores via QPointer so destruction auto-clears.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Implement preShutdownRenderFence

**Files:**
- Modify: `cpp/src/core/game_session.cpp`

- [ ] **Step 1: Implement the fence**

Place this implementation immediately after `registerLibretroGLItem` from Task 2:

```cpp
void GameSession::preShutdownRenderFence() {
    // Only the libretro GL backend has the IOSurface→MTLTexture coupling
    // that races against worker-side VideoHardwareGL teardown. Software
    // (mGBA) and Metal-direct (PCSX2 libretro) paths skip this entirely.
    if (m_libretroBackend != QStringLiteral("gl")) return;
    if (!m_libretroGLItem) return;

    LibretroGLItem* item = m_libretroGLItem.data();
    QQuickWindow* w = item->window();
    if (!w) {
        qWarning() << "[GameSession] preShutdownRenderFence: glItem has no "
                      "window, skipping fence (degraded — same risk as before "
                      "the fix)";
        return;
    }

    // Drop the LibretroGLItem's strong ARC ref to the MTLTexture and
    // disconnect its VideoHardwareGL signals. After the next sync,
    // updatePaintNode sees m_hw == nullptr and returns nullptr, deleting
    // the QSGSimpleTextureNode and its owned QSGTexture — releasing the
    // QSGMetalTexture wrapper's last strong ARC ref to the MTLTexture.
    item->setVideoHardware(nullptr);
    item->update();

    // Wait two render passes. Frame 1 covers the sync that processes the
    // cleared updatePaintNode and deletes the node + texture. Frame 2
    // covers any GPU command buffer that captured the MTLTexture before
    // the clear and is still draining on the GPU.
    QEventLoop loop;
    int framesSeen = 0;
    auto conn = QObject::connect(
        w, &QQuickWindow::afterRendering, &loop,
        [&framesSeen, &loop]() {
            if (++framesSeen >= 2) loop.quit();
        },
        Qt::QueuedConnection);   // afterRendering fires on QSGRenderThread

    // Hard cap — covers degenerate cases (window hidden, rendering paused,
    // app already quitting). At worst we're at the same risk as before the
    // fix; the cap doesn't make anything worse.
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(conn);

    qInfo() << "[GameSession] preShutdownRenderFence drained" << framesSeen
            << "frame(s) before stop";
}
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -40
```

Expected: compiles cleanly. The fence is defined but not called yet — that's the next task.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/core/game_session.cpp
git commit -m "$(cat <<'EOF'
feat(libretro): implement preShutdownRenderFence

Waits up to two QQuickWindow::afterRendering signals (capped at 500ms)
after clearing LibretroGLItem's hardware ref, so the scene graph deletes
the IOSurface-wrapping MTLTexture node before the worker tears down
VideoHardwareGL. No callers yet — wired in next commit.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Wire fence into kill() and terminate()

**Files:**
- Modify: `cpp/src/core/game_session.cpp` (lines around 426-463)

- [ ] **Step 1: Insert fence call at top of `kill()`**

Find `void GameSession::kill() {` (around line 426). Modify to call the fence first:

Before:
```cpp
void GameSession::kill() {
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->stop();
    else if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Killing emulator process";
        m_process->kill();
    }
}
```

After:
```cpp
void GameSession::kill() {
    preShutdownRenderFence();   // no-op for non-GL paths
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->stop();
    else if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Killing emulator process";
        m_process->kill();
    }
}
```

- [ ] **Step 2: Insert fence call at top of `terminate()`**

Find `void GameSession::terminate() {` (around line 435). Add the same fence call as the first line:

Before:
```cpp
void GameSession::terminate() {
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime()) {
        // Save-on-quit: ...
```

After:
```cpp
void GameSession::terminate() {
    preShutdownRenderFence();   // no-op for non-GL paths
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime()) {
        // Save-on-quit: ...
```

(Keep the rest of `terminate()` unchanged.)

- [ ] **Step 3: Build**

```bash
cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -40
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/game_session.cpp
git commit -m "$(cat <<'EOF'
feat(libretro): call preShutdownRenderFence from kill/terminate

GL-only fence runs before the worker is told to stop, draining the scene
graph of references to the IOSurface-wrapping MTLTexture. No-op for
software (mGBA) and Metal (PCSX2 libretro) backends.

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Wire QML registration

**Files:**
- Modify: `cpp/qml/AppUI/EmulationView.qml` (lines 91-123)

- [ ] **Step 1: Update `glComponent`'s `Component.onCompleted` and `Component.onDestruction`**

Find the `glComponent` definition (around line 91-123 of current file). The relevant block currently looks like this:

```qml
Component {
    id: glComponent
    LibretroGLItem {
        id: glItem
        anchors.fill: parent
        aspectMode: root.session ? root.session.libretroAspectMode : "native"
        integerScale: root.session ? root.session.libretroIntegerScale : false
        nativeAspect: root.session ? root.session.libretroAspectRatio : (16.0 / 9.0)
        // VideoHardwareGL is created lazily inside CoreRuntime's
        // installHwRender callback during retro_load_game. By the
        // time aboutToStartLibretro fires (which is what pushes
        // this view), session.videoHardware() returns null. We
        // poll on libretroBackendChanged and aspectRatioReported
        // (the latter is emitted right after retro_get_system_av_info,
        // by which point installHwRender has completed and the
        // VideoHardwareGL exists).
        function rewire() {
            if (root.session)
                glItem.setVideoHardware(root.session.videoHardware())
        }
        Component.onCompleted: rewire()
        Connections {
            target: root.session
            // libretroAspectRatioChanged is the GameSession signal that
            // fires after CoreRuntime emits aspectRatioReported — by
            // which point installHwRender has completed and
            // session.videoHardware() returns non-null.
            function onLibretroAspectRatioChanged() { glItem.rewire() }
            function onLibretroBackendChanged()     { glItem.rewire() }
        }
        Component.onDestruction: glItem.setVideoHardware(null)
    }
}
```

Replace `Component.onCompleted: rewire()` with:

```qml
        Component.onCompleted: {
            rewire()
            if (root.session)
                root.session.registerLibretroGLItem(glItem)
        }
```

Replace `Component.onDestruction: glItem.setVideoHardware(null)` with:

```qml
        Component.onDestruction: {
            glItem.setVideoHardware(null)
            if (root.session)
                root.session.registerLibretroGLItem(null)
        }
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -40
```

Expected: build succeeds. QML files are bundled by Qt's rcc — the build target should pick up the change automatically.

- [ ] **Step 3: Deploy + sign (REQUIRED before Task 6 launches the app)**

Per `build-cmake-needs-macdeployqt` memory: `cmake --build` produces a binary that crashes on launch with duplicate Qt loading until macdeployqt + ad-hoc codesign run. Build-only verification in Tasks 1-4 is fine without this; *launching* the app requires it.

```bash
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
```

Verify Qt is now bundled, not Homebrew-linked:

```bash
otool -L cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest | grep Qt
```

Expected: every Qt path starts with `@executable_path/../Frameworks/`. Any `/opt/homebrew/...` or `/usr/local/Cellar/...` line means macdeployqt did not finish — re-run before launching.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/EmulationView.qml
git commit -m "$(cat <<'EOF'
feat(libretro): register LibretroGLItem with GameSession for render fence

Wires Component.onCompleted to register the glItem* with GameSession so the
new preShutdownRenderFence can reach it from C++ on kill/terminate.
Component.onDestruction clears the registration and the hardware ref —
belt-and-suspenders; the fence has already run by the time this fires on
in-game-menu Quit, but covers paths that don't go through the fence
(e.g. game self-exit via RETRO_ENVIRONMENT_SHUTDOWN — deferred).

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Manual verification

This is the gate that determines whether the fix actually works. There is no automated harness for renderer races; the design spec calls this out explicitly.

**Files:** none (no code changes).

- [ ] **Step 1: Clean launch**

```bash
open ./cpp/build-x86_64/RetroNest.app
```

Wait for the app to fully launch. Open Console.app and filter for "RetroNest" to capture any `qInfo` / `qWarning` output, especially the new `[GameSession] preShutdownRenderFence drained …` line.

- [ ] **Step 2: Golden path — Quit (20 iterations)**

Each iteration:
1. Navigate to PSP library, launch a PSP game (use the title that previously triggered the crash — Persona 3 ISO if available; any PPSSPP-launchable ISO works).
2. Wait for the game to display (PPSSPP boot logo or in-game).
3. Open in-game menu (Cmd+Shift+Escape, or Select+Start on a controller, or Touchpad on DS4/DualSense).
4. Click **Quit**.
5. Verify: app returns to PSP library cleanly. **No app crash.**
6. Check Console: `[GameSession] preShutdownRenderFence drained 2 frame(s)` should appear. `framesSeen` of 1 is acceptable but unusual; 0 with no warning suggests the fence skipped — investigate.

Repeat **20 times in one session** without relaunching the app. Tally any crashes, hangs, or unexpected log output.

Expected: 20/20 success. Zero crash reports in `~/Library/Logs/DiagnosticReports/` matching `RetroNest-2026-05-23*` from this test session.

- [ ] **Step 3: Golden path — Save & Quit (20 iterations)**

Same as Step 2, but click **Save & Quit** instead of Quit. After each iteration, verify the resume file was written:

```bash
ls -la ~/Documents/RetroNest/emulators/ppsspp/psp/PPSSPP_STATE/*.resume 2>/dev/null
```

Reload the most recent .resume by selecting **Resume** on the game tile to confirm state restoration works.

Expected: 20/20 success. Resume file is non-empty (>1 KB) and reloads to the same point.

- [ ] **Step 4: Stress — fast quit (10 iterations)**

For each iteration:
1. Launch the PSP game.
2. Within ~1 second (before settling at title screen), open in-game menu.
3. Click Quit.

Expected: 10/10 success. The fence should still drain 2 frames; if it consistently drains 1 (visible in logs), the IOSurface may not have been allocated yet — that's still safe (no MTLTexture to race against) but note it.

- [ ] **Step 5: App quit while in-game**

1. Launch PPSSPP.
2. Press Cmd+Q (or close the window).

Expected: app exits cleanly, no crash report.

- [ ] **Step 6: Non-GL regression — mGBA**

1. Launch a GB/GBA game (software backend).
2. Quit via in-game menu.

Check Console: `[GameSession] preShutdownRenderFence drained …` should **NOT** appear (`m_libretroBackend != "gl"` short-circuits before the log line). If it does, the early-return is broken — fix before proceeding.

Expected: app behaves identically to before the change. No regression.

- [ ] **Step 7: Non-GL regression — PCSX2 libretro (Metal backend)**

1. Launch a PS2 game (Metal backend via LibretroMetalItem).
2. Quit via in-game menu.

Check Console: fence log should not appear.

Expected: identical to before. No regression.

- [ ] **Step 8: Crash report sweep**

```bash
ls -la ~/Library/Logs/DiagnosticReports/ | grep -i retronest | head -20
```

Expected: no new `.ips` files dated within this test session.

- [ ] **Step 9: Stop here if any test failed**

If any iteration in Steps 2-7 crashed, hung, or showed unexpected logs:
- Capture the crash report (`~/Library/Logs/DiagnosticReports/RetroNest-*.ips`)
- Capture Console output around the failure
- Refer to the "Rollback signals" section of the spec — match the failure mode to one of the documented diagnostics
- Do not proceed to the universal build / cleanup tasks; report back.

If 100% of iterations passed:

- [ ] **Step 10: Update memory note**

The deferred memory `libretro-gl-metal-teardown-race.md` says the architectural fix is pending. Update its status:

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/libretro-gl-metal-teardown-race.md`:
- Change top-of-file `**Status as of 2026-05-23:**` line to indicate the fix has shipped, referencing the new commits.
- Add a closing section noting which approach was taken (Approach A with afterRendering fence) and what was deferred (game self-exit via RETRO_ENVIRONMENT_SHUTDOWN).

Do this only after Steps 2-8 all passed. The memory is the durable record that future sessions will use to know the state.

---

## Task 7: Universal build verification (only if Task 6 passes)

The day-to-day builds use `cpp/build-x86_64`. Before considering the work done, verify the universal arm64+x86_64 build also builds and runs.

**Files:** none.

- [ ] **Step 1: Universal build**

```bash
./scripts/build-universal.sh 2>&1 | tail -40
```

Expected: completes without error. The merged `.app` lands at `cpp/build-universal/RetroNest.app`.

- [ ] **Step 2: Verify universal artifact**

```bash
./scripts/verify-universal.sh
```

Expected: passes.

- [ ] **Step 3: Smoke-test arm64 slice**

Default macOS launch picks the native arm64 slice on Apple Silicon:

```bash
open ./cpp/build-universal/RetroNest.app
```

Launch the same PSP game, Quit via in-game menu once. Verify no crash and the fence log line appears.

- [ ] **Step 4: Smoke-test x86_64 slice (Rosetta) — optional**

Per the universal-build policy in CLAUDE.md, the x86_64 slice is the one PCSX2 perf parity needs. PPSSPP runs natively on both, but verifying the fence works under Rosetta is a low-cost extra check:

1. Finder → cpp/build-universal/RetroNest.app → Get Info → check "Open using Rosetta".
2. Launch the app, launch a PSP game, Quit via in-game menu.
3. Uncheck "Open using Rosetta" when done.

Expected: no crash on either slice.

- [ ] **Step 5: No commit needed**

Tasks 1-5 already committed. This task is verification only.

---

## Done criteria

All of the following must be true:

1. Tasks 1-5 committed; `git log` shows 5 new commits since `b20f40e` (the spec commit).
2. Task 6 Steps 2-7 all passed with zero crashes.
3. Task 6 Step 8: no new crash reports.
4. Task 6 Step 10: memory note updated.
5. Task 7 universal-build smoke test passes (arm64 slice at minimum; x86_64 optional).

If any criterion fails, the work is not done. Refer to the spec's "Rollback signals" section.
