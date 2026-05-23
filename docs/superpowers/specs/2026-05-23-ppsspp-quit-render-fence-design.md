# PPSSPP Quit render-fence — design

**Date:** 2026-05-23
**Sub-project:** Eliminate the PPSSPP Quit / Save & Quit crash by inserting a render-thread synchronization gate before `VideoHardwareGL` teardown.
**Status:** Design — pending implementation plan

## Summary

Today, when the user clicks **Quit** or **Save & Quit** in the in-game menu while PPSSPP is running, the whole RetroNest app crashes back to the dock. The crash signature is `EXC_BAD_ACCESS` in `-[AGXG16GFamilyRenderContext setFragmentTextures:withRange:]` on `QSGRenderThread`. Save-on-Quit completes before the crash (resume file written), so no data loss — but the user has to relaunch RetroNest after every PPSSPP quit.

The root cause is a teardown ordering race. PPSSPP renders via OpenGL into an IOSurface owned by `VideoHardwareGL`. `LibretroGLItem` imports that IOSurface as a Metal texture for Qt 6's Metal RHI scene graph. On Quit, `CoreRuntime`'s worker thread destroys `VideoHardwareGL` (releasing the IOSurface) while `QSGRenderThread` is still composing render passes that reference the wrapping MTLTexture. The MTLTexture's backing memory disappears mid-pass.

This design inserts a synchronization gate on the main thread, in `GameSession::kill()` and `GameSession::terminate()`, that:
1. Calls `LibretroGLItem::setVideoHardware(nullptr)`, which disconnects the item from `VideoHardwareGL`, nils its cached MTLTexture reference, and schedules a paint.
2. Waits (bounded) for two `QQuickWindow::afterRendering` signals — long enough for the scene graph sync to delete the `QSGSimpleTextureNode` and for any in-flight Metal command buffer that referenced it to drain on the GPU.
3. Only then signals the worker to tear down.

The threading model is unchanged: `VideoHardwareGL` still lives on the worker thread and is still destroyed there. The fix is *when* — after the scene graph has confirmed it is no longer referencing the IOSurface.

## Motivation

Crash signature (verified 2026-05-23, multiple reports in `~/Library/Logs/DiagnosticReports/RetroNest-2026-05-23-*.ips`):

```
EXC_BAD_ACCESS  KERN_INVALID_ADDRESS at 0x0000000000000078
thread: QSGRenderThread
  AGXMetalG16G_B0: -[AGXG16GFamilyRenderContext setFragmentTextures:withRange:]+154
  QtQuick: QSGBatchRenderer::Renderer::renderMergedBatch(...)+128
  QtQuick: QSGBatchRenderer::Renderer::recordRenderPass(...)+254
  QtQuick: QSGBatchRenderer::Renderer::render()+117
  QtQuick: QSGRenderer::renderScene()+185
  QtQuick: QQuickWindowPrivate::renderSceneGraph()+1285
```

The +154 offset, +0x78 null-derived offset combination indicates the AGX driver is reading a member of a freed MTLTexture-backing object. The texture handle in the command buffer is dangling — its IOSurface backing was released on another thread mid-render-pass.

Two surgical fixes have already shipped:

- **`80c73ef`** — Call `context_destroy` BEFORE `retro_unload_game` so PPSSPP's internal `Libretro::ctx` is still alive when its `ContextDestroy` callback fires. Without this, the worker crashes inside `ppsspp_libretro.dylib::context_destroy()+12` and never reaches the GL/Metal race. Symptom-shifting only.
- **`f6d8474`** — `LibretroGLItem::m_hw` is now `QPointer<VideoHardwareGL>` so the QML `Component.onDestruction → setVideoHardware(null)` handler doesn't deref a dangling sender pointer on disconnect. Another symptom shift — once disconnect doesn't crash, the render thread's parallel race becomes visible.

Both are real, focused fixes and stay in place. This design is the architectural fix that sits on top of them.

A prior attempt to defer `m_videoHW` destruction to `~CoreRuntime` on the main thread (the spirit of "Approach B" in the deferred-memory note) broke first-game video on fresh launch and was reverted before commit. The cause was not isolated. That history is the reason this design keeps the threading model unchanged — it gates destruction with a render-sync fence rather than relocating it.

## Approach

The crash exists because both of these are true simultaneously:

1. `VideoHardwareGL::shutdown()` calls `CFRelease(m_impl->ioSurface)`, dropping the only strong reference to the IOSurface.
2. `QSGRenderThread` is still drawing a `QSGSimpleTextureNode` whose `QSGTexture` wraps an MTLTexture imported via `[device newTextureWithDescriptor:iosurface:plane:]`. Metal does **not** retain the IOSurface — the MTLTexture's backing is invalidated the moment `CFRelease` brings the refcount to zero.

The fence makes #2 false before #1 happens. Specifically, before the worker enters teardown:

- Drop `LibretroGLItem`'s strong ARC reference to the MTLTexture (`m_impl->mtlTexture = nil`) — done by `setVideoHardware(nullptr)`.
- Allow one scene graph sync to call `updatePaintNode`, which returns `nullptr` because `m_hw` is null; this deletes the `QSGSimpleTextureNode` and (because `setOwnsTexture(true)`) its owned `QSGTexture`, dropping the last ARC reference to the MTLTexture.
- Allow one more frame for any GPU command buffer that captured the MTLTexture before the clear to complete and release its references.

After both frames have rendered, no MTLTexture wraps the IOSurface, no command buffer references the MTLTexture, and the worker can `CFRelease` the IOSurface with no consumer left.

The fence runs on the main thread (where the QML scene graph driver lives), so it can safely block on a local `QEventLoop` while waiting for `afterRendering` signals from the render thread. A 500 ms hard cap ensures we never hang on degenerate paths (window hidden, rendering paused, app already quitting).

Approaches considered and rejected:

- **Move VideoHardwareGL ownership to the main thread.** Architecturally cleaner but the 2026-05-23 attempt at this broke startup with unclear root cause. Same sync gate is still needed even with ownership relocated. Higher risk, no algorithmic advantage.
- **`BlockingQueuedConnection` from worker to main pre-shutdown.** Deadlock risk: if the main thread is anywhere in a `GameSession::finished` slot path or other blocking call, the worker hangs forever. The render-thread wait mechanism is identical to Approach A; only the calling thread changes.
- **Defer the fix entirely; accept the crash.** Rejected — every PPSSPP Quit requires an app relaunch today, and the in-game menu Quit is the only documented exit path.

## Architecture

### Threading + lifetime diagram

```
Main thread                Worker thread (CoreRuntime)          QSGRenderThread
─────────────              ─────────────────────────             ───────────────
user clicks Quit            runLoop: retro_run × N
in in-game menu                (still running)
  │
  ▼
GameSession::kill()
  preShutdownRenderFence():
    setVideoHardware(null)
    item.update()
    eventLoop.exec()
        │
        ├─ wait for afterRendering #1 ─────────────────────────→ render: skips deleted node
        │                                                        afterRendering ↩
        ├─ wait for afterRendering #2 ─────────────────────────→ render: GPU drains prior cmd buf
        │                                                        afterRendering ↩
        └─ (loop quits)
  ▼ (fence done)
  CoreRuntime::stop()  ──→  m_stopRequested = true
                            runLoop exits, teardown begins:
                            ─ flushPendingSaveState
                            ─ context_destroy()
                            ─ retro_unload_game
                            ─ m_videoHW->shutdown()  ← SAFE
                            ─ m_videoHW.reset()
                            ─ emit finished(...)
GameSession::finished slot ←─┘
  pop EmulationView from stack
  glItem destructor (no-op,
    m_hw QPointer already null)
```

### Components changed

**`GameSession` (`cpp/src/core/game_session.{h,cpp}`):**
- Add private field `QPointer<LibretroGLItem> m_libretroGLItem`.
- Add `Q_INVOKABLE void registerLibretroGLItem(QObject* item)` — QML calls this from the glItem's `Component.onCompleted` with `this`, and from `Component.onDestruction` with `null`. `QPointer` ensures the field auto-clears even if QML forgets to send the null.
- Add private helper `void preShutdownRenderFence()`.
- Both `kill()` and `terminate()` call `preShutdownRenderFence()` as their first line, before any worker-side stop signalling.

**`EmulationView.qml` (`cpp/qml/AppUI/EmulationView.qml`):**
- In `glComponent`'s `LibretroGLItem`, extend `Component.onCompleted` to call `root.session.registerLibretroGLItem(glItem)` after `rewire()`.
- Replace `Component.onDestruction: glItem.setVideoHardware(null)` with two calls: `glItem.setVideoHardware(null); root.session.registerLibretroGLItem(null)`. By the time this destruction handler fires, the fence has already run (kill/terminate happen *before* `EmulationView` is popped from the stack), so neither call has a load-bearing role in the Quit flow — they exist for tidy state cleanup and for paths where the game ends without going through the fence (the deferred `RETRO_ENVIRONMENT_SHUTDOWN` case).

**`LibretroGLItem`, `VideoHardwareGL`, `CoreRuntime`:** no changes.

### The fence — exact mechanics

```cpp
// In game_session.cpp, called from kill() and terminate()
void GameSession::preShutdownRenderFence() {
    if (m_libretroBackend != QStringLiteral("gl")) return;
    if (!m_libretroGLItem) return;

    LibretroGLItem* item = m_libretroGLItem.data();
    QQuickWindow* w = item->window();
    if (!w) return;

    // Drop our strong refs and request a sync. After the next sync,
    // updatePaintNode sees m_hw == nullptr and returns nullptr, which
    // deletes the QSGSimpleTextureNode and its owned QSGTexture —
    // releasing the QSGMetalTexture wrapper's last strong ARC ref to
    // the MTLTexture.
    item->setVideoHardware(nullptr);
    item->update();

    // Wait two render passes:
    //   #1 — the sync that processes the cleared updatePaintNode and
    //        deletes the node + texture.
    //   #2 — covers any GPU command buffer that captured the MTLTexture
    //        before the clear and is still draining.
    QEventLoop loop;
    int framesSeen = 0;
    auto conn = QObject::connect(
        w, &QQuickWindow::afterRendering, &loop,
        [&framesSeen, &loop]() {
            if (++framesSeen >= 2) loop.quit();
        },
        Qt::QueuedConnection);   // afterRendering fires on QSGRenderThread

    QTimer::singleShot(500, &loop, &QEventLoop::quit);  // hard cap
    loop.exec();

    QObject::disconnect(conn);
}
```

### QML registration

```qml
// EmulationView.qml — glComponent
LibretroGLItem {
    id: glItem
    // ...existing aspect / nativeAspect / integerScale bindings...

    function rewire() {
        if (root.session)
            glItem.setVideoHardware(root.session.videoHardware())
    }

    Component.onCompleted: {
        rewire()
        if (root.session)
            root.session.registerLibretroGLItem(glItem)
    }

    Connections {
        target: root.session
        function onLibretroAspectRatioChanged() { glItem.rewire() }
        function onLibretroBackendChanged()     { glItem.rewire() }
    }

    Component.onDestruction: {
        glItem.setVideoHardware(null)
        if (root.session)
            root.session.registerLibretroGLItem(null)
    }
}
```

## Coverage matrix

| Stop path | Funnel | Fence fires |
|---|---|---|
| In-game menu → Quit | `AppController` → `stopGame()` → `GameService::stopGame()` → `GameSession::kill()` | ✓ |
| In-game menu → Save & Quit | `AppController` → `saveAndStopGame(1)` → `GameService::saveAndStopGame()` → `GameSession::terminate()` | ✓ |
| App quit (Cmd+Q) while game running | `AppController` shutdown → `stopGame()` | ✓ |
| Save-and-Quit watchdog timeout (10s) | `GameService::m_terminateTimer` → `GameSession::kill()` | ✓ (idempotent — `m_libretroGLItem` already null after first kill if `EmulationView` was popped; fence becomes no-op) |
| Game self-exit via `RETRO_ENVIRONMENT_SHUTDOWN` | Worker exits `runLoop` and tears down `VideoHardwareGL` synchronously before `emit finished` — no main-thread stop call | **Not covered** (see Deferred) |
| Adapter mismatch / load failure before HW init | `m_libretroGLItem` never registered; fence is no-op | N/A (no race exists) |

## Deferred

### Game self-exit via libretro `RETRO_ENVIRONMENT_SHUTDOWN`

PPSSPP's libretro core does not call this in practice. A future core that did would race the same way it does today — the worker would tear down `VideoHardwareGL` before any main-thread sync. If/when this becomes a real failure mode, the fix is to have `CoreRuntime::runLoop` post a `QMetaObject::invokeMethod(GameSession, "preShutdownRenderFence", Qt::BlockingQueuedConnection)` before the teardown block — *but only on the self-exit path*, since the kill/terminate paths already fence on the main thread before stopping the worker.

Not implemented now because:
- It's a non-observed path.
- A blocking-queued call from the worker has the same deadlock concerns as Approach C — needs a careful audit of what the main thread could be doing when this fires.

Document the gap in `cpp/src/core/libretro/core_runtime.cpp` near the teardown block.

## Preserved fixes

- **`80c73ef`** (`context_destroy` before `retro_unload_game`) stays exactly as-is. The fence does not change ordering inside the worker teardown.
- **`f6d8474`** (`QPointer<VideoHardwareGL>` for `LibretroGLItem::m_hw`) stays exactly as-is. The fence works whether or not the QPointer auto-clears, but the QPointer is still the safety net for the post-teardown destruction path.

## Testing plan

Manual (no automated harness for renderer races):

- **Golden path — Quit:** Start PPSSPP (any ISO that exercises the GL HW-render path; the Persona 3 ISO used during prior diagnosis is fine). Open in-game menu → Quit. App returns to game library cleanly. Repeat **20 times** in one session.
- **Golden path — Save & Quit:** Same, but with Save & Quit. Verify resume file is written and reloading restores state. 20 repeats.
- **Stress — fast quit:** Start PPSSPP, wait less than 1 second, immediately open menu → Quit. Tests the fence under render-thread pressure (first IOSurface allocation may still be in flight). 10 repeats.
- **App quit while in-game:** Start PPSSPP, press Cmd+Q from the dock menu (or close the window). Process exits cleanly, no crash report.
- **Non-GL backends — no regression:** Start mGBA (software backend), then PCSX2 libretro (metal backend). Quit each. Fence must be skipped (verify by temporarily adding `qInfo` at fence entry and confirming it does not fire for these paths). Restore the log line removal before commit.
- **Crash report check:** After the test session, check `~/Library/Logs/DiagnosticReports/` for any new `RetroNest-*.ips` files. None should exist.

## Rollback signals

If we ship and the AGX crash returns:

- **Same signature, less frequent:** Fence drained partially. Bump frame count from 2 → 3, or extend the timeout cap. Diagnostic: add a `qInfo` recording `framesSeen` at fence exit; if it consistently reaches 2 but the crash still occurs, the issue is GPU-side command buffer draining and we need to wait on `afterFrameEnd` (Qt 6.4+) instead of `afterRendering`.
- **New null-deref crash inside `LibretroGLItem`:** Registration ordering is wrong. The fence is calling into a freed item. Check the QML `Component.onDestruction` ordering; the destruction-time `registerLibretroGLItem(null)` must happen before the item is freed (it does, given QML's destruction order).
- **Fence timeout fires (`framesSeen < 2` after 500 ms):** Window is hidden / app is in background. Acceptable degraded behavior — the worker proceeds with teardown and we are at the same risk as before the fix. Bumping the cap is the wrong fix; the right fix is to detect the no-render case and skip the wait entirely. Defer until observed.

## Open questions

None — all design choices resolved during brainstorming.
