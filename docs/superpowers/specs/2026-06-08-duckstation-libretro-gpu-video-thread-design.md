# DuckStation libretro — GPU video thread (`GPU/UseThread = true`)

**Date:** 2026-06-08
**Status:** Design — measurement-gated
**Feature:** Run the GPU backend on its own thread in the DuckStation libretro core, so CPU emulation (`System::RunFrame`) and GPU command submission/present overlap instead of running synchronously inline on the core thread. This is the "possible later: GPU video thread — requires run-loop rework" item from the HW-renderer spec (§8).

## 1. Goal

Decouple GPU work from CPU emulation in the DuckStation libretro core so the two overlap, for a higher/steadier framerate and less core-thread stall — especially at high internal resolution scales (4×–8×) where per-pixel GPU work dominates. End state (if justified): `GPU/UseThread = true` works correctly, and the #3 settings schema can expose "Threaded Rendering" as a real toggle (currently excluded because the core forces the setting off).

**This design is measurement-gated.** It does not assume the win is real. Phase 0 measures the actual inline GPU/present cost and applies an explicit go/no-go gate before any rework is built. An honest "measured, not worth it" is a valid and acceptable outcome of Phase 0.

## 2. Background — the current inline model (verified facts)

The libretro core today is built around **synchronous, single-thread** execution:

1. **Host run loop** — `RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp` `runLoop()` (~:330) runs on a worker `QThread`. It calls `s.retro_run()` once per host-frame iteration (~:510), each wrapped in a per-frame `mac::AutoreleaseScope` (~:504, `autorelease_scope.h`) — the **resume-crash fix** that drains autoreleased Metal/ObjC objects every frame so none outlive `CoreLoader::close()`'s `dlclose`. Pacing is deadline-based `sleep_until(next)` against a wall-clock target derived from av_info fps (no vsync wait); the emulator also has its own internal framelimiter.
2. **`retro_run`** — `src/duckstation-libretro/libretro.cpp` (~:202–225): input poll → `System::RunFrame()` → a **cosmetic** `video_refresh` heartbeat (RetroNest composites the `CAMetalLayer` directly, not heartbeat pixels) → `DrainAudio()`.
3. **GPU commands run INLINE** — `src/core/video_thread.cpp` `VideoThread::PushCommand` (~:259): a real persistent video/worker thread already exists in the engine, but when `!use_thread` `PushCommand` dispatches **synchronously** to `gpu_backend->HandleCommand(cmd)` on the calling (core) thread. So GPU submission AND present happen inline during `RunFrame`.
4. **Present happens inline** — `VideoPresenter::PresentFrame` (`src/core/video_presenter.cpp`, ~:1579–1608) is called from inside `HandleSubmitFrameCommand`/`HandleUpdateDisplayCommand` (`src/core/gpu_backend.cpp` ~:587) onto the NSView swapchain. `Host::FrameDoneOnVideoThread` (`libretro_host.cpp:261`) **deliberately does NOT present** — the inline handler already did; a second present would desync `RestoreDeviceContext`. It only samples display size for the heartbeat.
5. **Frame boundary** — `System::RunFrame()` (a single-frame driver added to the fork) returns when `InterruptExecution()` fires at the frame boundary, set by `Host::PumpMessagesOnCoreThread()` (`libretro_host.cpp:341`).
6. **ImGui/OSD** — inited in `video_thread.cpp` `CreateDeviceOnThread`, drawn inside the same inline `PresentFrame` (why #3's OSD toggles need no extra rendering code).

**Why `UseThread=false` is forced today** (`libretro_settings.cpp:363`): every item above assumes GPU commands execute synchronously on the core thread and present happens inline during `RunFrame`. The NSView present path, the resume autorelease-pool fix, the inline audio drain, and the `FrameDoneOnVideoThread`-doesn't-present invariant are all built on it.

## 3. Feasibility findings (grounding, 2026-06-08)

Three findings from reading the code materially shape this design:

- **The GPU worker thread already exists in the engine.** `VideoThread` always starts a persistent worker; when `use_thread=true` `PushCommand` enqueues to a FIFO the worker consumes. The libretro core merely *forces* `use_thread=false`. So this is less "build a thread" and more "stop forcing it off + fix the host-side invariants that assumed inline." Lowers build risk.
- **Steady-state off-thread present looks feasible.** Per-frame present is `nextDrawable`/`presentDrawable` on the `CAMetalLayer` — Core Animation calls, thread-safe once the layer is installed. The NSView attach (`setWantsLayer`/`setLayer:`) happens **once** on the main thread via `RunOnMainThread` during `AcquireRenderWindow`. The real off-thread hazard is **not** steady-state present.
- **The real off-thread hazard is a resize-race in the host.** RetroNest's `LibretroMetalItem::syncContentsScale()` (`cpp/src/ui/libretro/libretro_metal_item.mm` ~:182–219) reads `view.layer` and mutates `CAMetalLayer.drawableSize` / sublayers' `contentsScale` from the **Qt render thread** on geometry/DPR change. Today nothing else touches the layer concurrently; a GPU worker presenting concurrently introduces a data race on resize. Must be addressed in the rework (Phase 4), not Phase 0.

## 4. Approach — measure-first, single spec, Phase-0 gate

Chosen over (B) a separate throwaway spike spec and (C) skipping measurement. Rationale: the grounding lowered feasibility uncertainty enough that a separate spike-spec cycle is pure overhead, but the win is genuinely unproven — the host already paces to a wall-clock deadline and the emulator self-framelimits, so at low/medium scales the inline GPU cost may be entirely hidden under the frame budget, making the overlap worth zero framerate. One spec; Phase 0 is a measurement+feasibility gate with an explicit go/no-go; Phases 1+ are conditioned on passing it.

## 5. Phase 0 — measurement spike (the committed work)

### 5.1 What we measure

When `use_thread=false`, the time the core thread spends in the synchronous GPU path is *exactly* the time that would move off-thread when threaded. Three lightweight timers (static accumulators, averaged and logged every 60 frames as a `[GPU_PROF]` line to `/tmp/rn.log`, mirroring the existing `[AUDIO_TRACE]` pattern in `core_runtime.cpp` ~:534–554):

1. **`runframe_ms`** — wraps the whole `System::RunFrame()` call in `retro_run` (`libretro.cpp`). Total per-frame core-thread time.
2. **`gpu_cmd_ms`** — accumulates time in the synchronous `gpu_backend->HandleCommand()` dispatch inside `PushCommand`'s `!use_thread` branch (`video_thread.cpp` ~:259). All inline GPU work.
3. **`present_ms`** — wraps `VideoPresenter::PresentFrame` (`video_presenter.cpp` ~:1579). A subset of `gpu_cmd_ms`, reported separately because present-blocking (drawable acquire / vsync / GPU wait) behaves differently from command-building.

Derived: **CPU-emulation ≈ runframe − gpu_cmd**; **off-thread-recoverable ≈ gpu_cmd (incl. present)**. The log line also prints the frame budget (`1000 / av_info.timing.fps` ms) so the fraction is readable at a glance.

`[GPU_PROF]` line format (every 60 frames):
```
[GPU_PROF] scale=Nx frame=<n> budget_ms=<b> runframe_ms=<r> gpu_cmd_ms=<g> present_ms=<p> gpu_frac=<g/r> over_budget=<r>b?
```

### 5.2 Implementation notes

- Instrumentation only, sitting behind the existing inline `!use_thread` path. **Zero behavior change**, trivially revertible. This is the safe part of the feature.
- Accumulators reset at frame start; emit on a 60-frame cadence to avoid log spam (same cadence as `[AUDIO_TRACE]`).
- Gate emission behind a runtime/env flag (reuse the `audioTraceEnabled()` pattern, e.g. a `GPU_PROF`-style toggle) so it is off by default and adds no cost to normal runs.
- `scale` is read from the active internal-resolution setting so each log line is self-labeling across the 1×/4×/8× runs.

### 5.3 Measurement procedure (user-run; TCC blocks the agent from the GUI)

1. Obtain a **free, self-running PS1 homebrew demo** loadable as a `.exe`/`.ps-exe` (DuckStation boots these with no disc). Recommended: PSn00bSDK `n00bdemo`, or a GPU-heavy demoscene demo from pouet.net. Self-running = no input, reproducible scene.
2. Launch RetroNest with the demo. Using the #3 settings, set internal resolution to **1×**; let it run ~30s; capture the `[GPU_PROF]` lines from `/tmp/rn.log`.
3. Repeat at **4×** and **8×** (the same scene).
4. Report the three sets of averages. Three scales reveal the **cost slope**, not a single number.

### 5.4 The homebrew-floor caveat (how to read a weak result)

A lot of inline GPU cost — the upscale blit + present — is content-independent and scales with framebuffer pixel count, so even a light demo at 8× reveals the present-path floor and its 1×→4×→8× growth. Therefore:
- A **GO** signal from a modest demo is trustworthy (real games are heavier — only worse).
- A **NO-GO** signal from a modest demo is a **floor, not a ceiling**: report it as "measured cheap on available content; worst-case games unverified," not a hard no — leaving the door open if a GPU-heavy title is obtained later.

## 6. Go/no-go gate

Keyed off the worst scale measured (8×). The threading win is real only when the core thread is blocked on GPU work *past the frame budget*; if the system already hits full speed (sleeps at end of frame), overlap recovers nothing for framerate.

**GO** — both must hold at 8×:
- `runframe_ms` > frame budget (frames over budget — not hitting full speed), **and**
- `gpu_cmd_ms` ≥ ~30% of `runframe_ms` (the over-budget cause is GPU, not pure CPU emulation — enough to overlap meaningfully).

**MARGINAL (user decides)** — `runframe_ms` ≈ 80–100% of budget at 8× **and** `gpu_cmd_ms` ≥ ~40%. Threading buys headroom and steadier pacing but not raw fps. Surface the numbers; user calls it.

**NO-GO (document and stop)** — `runframe_ms` < ~80% of budget at 8× (comfortably full speed). Threading won't raise framerate. Record the measured floor and the honest conclusion; do not build the rework. *Exception:* reducing main-thread stall / input latency is a separate, independently-decided goal — not the framerate win this feature is scoped around.

Tunable knobs (chosen conservatively): the **30% / 40%** GPU-fraction cutoffs and the **80%** full-speed margin.

## 7. Phases 1+ — the rework (conditional on a GO/MARGINAL gate)

Outline only; each gets detailed treatment in the implementation plan, written *after* the gate passes. Treat the resume/dlclose path as the top regression risk throughout, and validate everything under **x86/Rosetta** (the current run mode).

- **Phase 1 — flip threading on.** Stop forcing `use_thread=false` (`libretro_settings.cpp:363`); let `PushCommand` enqueue to the FIFO and the existing worker consume/render/present. Get a frame on screen threaded.
- **Phase 2 — autorelease/dlclose discipline on the worker.** Give the GPU worker its own per-frame `AutoreleaseScope`. Guarantee the worker is **stopped + drained + joined before `CoreLoader::close` dlcloses the dylib** — the canonical "Metal object outlives dlclose" resume crash gets sharper with a second thread.
- **Phase 3 — frame-boundary + pacing reconciliation.** Decide whether `retro_run`/`runLoop` blocks on GPU-thread completion or runs ahead; reconcile the heartbeat `video_refresh`; re-examine the `FrameDoneOnVideoThread`-doesn't-present invariant (with a real worker thread, presenting *there* may now be the correct design — get it wrong → `RestoreDeviceContext` desync / double-present).
- **Phase 4 — off-thread present hardening.** Resolve the `syncContentsScale` resize-race (§3): serialize host-thread layer mutation against the worker's present (dispatch layer/`drawableSize` changes to the main thread and/or synchronize with the worker). Confirm `AcquireRenderWindow` and RetroNest's `LibretroMetalItem` compositing are safe with the worker presenting.
- **Phase 5 — resume / save-state re-verification.** Re-verify save, load, and save-and-exit→resume are clean under threading (the resume-crash territory).
- **Phase 6 — pause/shutdown coordination.** Reconcile `PumpMessagesOnCoreThread`/`InterruptExecution` and the host pause/stop path (`core_runtime.cpp` stop-`retro_run` + mute) with the worker's lifecycle.
- **Phase 7 — expose the setting.** Add a "Threaded Rendering" row to the #3 schema, re-run `tools/check_schema_fidelity.py`, and decide the default (keep `false` until proven solid; flip once stable).

## 8. Risks

- **Resume regression** — the autorelease-pool-before-dlclose crash is the canonical landmine; a worker thread reopens it. Verify resume + save/load clean under threading. (Phase 2/5.)
- **Off-thread present / resize-race** — steady-state present is fine; the `syncContentsScale` Qt-render-thread layer mutation racing the worker is the concrete hazard. (Phase 4.)
- **Double-present / desync** — `FrameDoneOnVideoThread` currently must NOT present; under threading that may invert. Wrong → `RestoreDeviceContext` desync. (Phase 3.)
- **Marginal real-world gain** — if host pacing / CPU emulation dominates, the rework moves framerate little. Phase 0 exists to catch this before building.
- **x86/Rosetta** — current run mode; threading + timing assumptions must hold there too.

## 9. Build / deploy / test recipe (x86/Rosetta-current)

```sh
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS" && src/duckstation-libretro/package.sh        # universal + self-contained + no-SDL, deploys to shared cores dir

cd /Users/mark/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
```
The user launches `cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1` (TCC blocks the agent from the GUI). The `macdeployqt … Cannot resolve rpath libspirv-cross-c-shared` line is a known cosmetic non-fatal warning (the lib is bundled + symlinked in `Contents/Frameworks/`).

**Phase 0 acceptance:** `[GPU_PROF]` lines appear in `/tmp/rn.log` at 1×/4×/8× with sensible values; the go/no-go is computed from them. **Phases 1+ acceptance (if reached):** game renders correctly (no flicker/tearing/desync), framerate equal-or-better, **save-and-exit → resume is clean**, `/tmp/rn.log` shows the GPU thread active without errors.

## 10. Key references

- **Core run loop / present:** `duckstation-libretro/src/duckstation-libretro/libretro.cpp` (`retro_run` ~:202), `libretro_host.cpp` (`FrameDoneOnVideoThread`:261, `PumpMessagesOnCoreThread`/`InterruptExecution`:341), `libretro_settings.cpp` (`UseThread` forced false :363).
- **Engine threading:** `duckstation-libretro/src/core/video_thread.cpp` (`PushCommand` sync-vs-FIFO :259, `use_thread` path, `CreateDeviceOnThread`), `src/core/gpu_backend.cpp` (`HandleSubmitFrameCommand` → `PresentFrame` ~:587), `src/core/video_presenter.cpp` (`PresentFrame` ~:1579).
- **Host run loop + resume fix:** `RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp` (`runLoop`:330, per-frame `AutoreleaseScope`:504, `retro_run`:510, `[AUDIO_TRACE]`:534–554), `cpp/src/core/libretro/autorelease_scope.h`, `CoreLoader::close` (dlclose ordering).
- **Host compositing / resize-race:** `RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm` (`syncContentsScale` ~:182–219).
- **Prior specs:** `…/specs/2026-06-03-duckstation-libretro-hw-renderer-design.md` (§8 names this feature; "Implementation Outcome" documents the resume-crash root cause), `…/specs/2026-06-04-duckstation-libretro-settings-design.md` (`UseThread` is the excluded knob; add the "Threaded Rendering" row once this lands).
- **Handoff:** `duckstation-libretro/docs/gpu-video-thread-handoff-2026-06-04.md`.
- **Run mode:** `RetroNest-Project/CLAUDE.md` → "Current run mode: x86_64 (Rosetta)".
