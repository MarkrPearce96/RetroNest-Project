# PCSX2 Libretro Core — Audio Output (Sub-project 4 of 8)

**Date:** 2026-05-11
**Status:** Design — pending implementation plan
**Owner:** mark
**Scope:** Fourth sub-project of the multi-phase PCSX2-to-libretro port.
**Predecessors:** [Skeleton (SP1)](2026-05-11-pcsx2-libretro-skeleton-design.md), [VM Lifecycle (SP2)](2026-05-11-pcsx2-libretro-vm-lifecycle-design.md), [HW Render Bridge (SP3)](2026-05-11-pcsx2-libretro-video-bridge-design.md), [UX Overlays (SP3.5)](2026-05-11-pcsx2-libretro-ux-overlays-design.md). All complete.

## Context

SP3 made a real PS2 game render its video into RetroNest's Metal view. SP3.5 layered on the in-game UX overlays. Audio is still off — `EmuConfig.SPU2.Backend` defaults to `AudioBackend::Cubeb`, which on macOS inside a libretro core either fails to initialise (no host audio device chosen) or fights the libretro frontend's own audio device. The user sees a fully working game, in silence.

SP4 routes PCSX2's SPU2 output through libretro's `retro_audio_sample_batch_t` callback so that audio plays through whichever audio backend the libretro frontend (RetroNest) already owns — same pattern mGBA uses today.

## Architectural starting point

PCSX2's audio integration point is the `AudioStream` class in `pcsx2/Host/AudioStream.{h,cpp}`. SPU2 calls `AudioStream::CreateStream(EmuConfig.SPU2.Backend, …)`, which dispatches to one of three concrete subclasses:

- `AudioBackend::Cubeb` → `CubebAudioStream` (default, cross-platform native)
- `AudioBackend::SDL` → `SDLAudioStream` (SDL3 fallback)
- `AudioBackend::Null` → base class `AudioStream` with no consumer thread

All three are **pull-model**: the backend's audio device thread invokes a callback (`SDLAudioStream::AudioCallback`, etc.) that calls `AudioStream::ReadFrames(buf, n)` to drain a lock-free ring buffer. SPU2's EE thread fills the same ring buffer via `s_output_stream->WriteChunk(chunk)` (64-frame stereo float chunks). The base class also owns the timestretch (SoundTouch) and channel-expansion (FreeSurroundDecoder) machinery — both are applied during the `WriteChunk → ring-buffer` path, before any backend reads.

Critical fact: there is **no `Host::` callback for audio output**. Unlike SP3 where `Host::AcquireRenderWindow` was a clean integration seam, SP4's seam is a class subclass behind a static factory. `s_output_stream` is `static` inside `spu2.cpp` — no public accessor.

Libretro's audio surface is **push-model**: the core calls `audio_sample_batch_cb(const int16_t* data, size_t frames)` (interleaved stereo) whenever it has samples ready. The frontend buffers and presents via its own host audio device.

## Goal

A real PS2 game launched through RetroNest plays audio in sync with video, through RetroNest's existing libretro audio path, with PCSX2's full timestretch + (forced-stereo) audio pipeline intact.

**Definition of done:**

1. Launching Ratchet & Clank in RetroNest produces audible audio: BIOS boot chime is heard, in-game music and SFX play in sync with video for ≥30 seconds without audible glitches.
2. The new `LibretroAudioStream` subclass is selected via `EmuConfig.SPU2.Backend = AudioBackend::Libretro` set in our `Settings.cpp`.
3. `retro_run` drains accumulated audio frames once per video frame after waiting on `g_present_cv`, converts float→int16, and pushes via the cached `audio_sample_batch_cb`.
4. PCSX2's SoundTouch timestretch path is exercised under fast-forward / VM speed adjustment: audio time-stretches (preserved pitch, faster) rather than chipmunks or skips.
5. Pause via Cmd+Shift+Escape stops audio cleanly; close-menu resumes it.
6. Save state + load state mid-game does not corrupt audio output.
7. `retro_unload_game` → reload same game produces audio on the second load (verifies destructor cleanup).
8. Existing SP3 video pacing is unchanged. mGBA's audio path is unchanged.

## Non-goals (deferred to later sub-projects or out of scope)

- **Surround / channel expansion.** Forced to `AudioExpansionMode::Disabled`. Libretro's batch callback is stereo-only; emitting expanded frames into it would corrupt the stream. Most libretro frontends don't surface multichannel anyway. If a future sub-project needs surround, it's a separable feature.
- **PSX 44.1 kHz support.** Sample rate hardcoded to 48000 in `retro_get_system_av_info`. PSX titles via PCSX2 are a fringe path; would play slightly off-pitch. Defer to SP4.x if it ever matters.
- **Mid-session sample-rate changes.** Tied to the above. `RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO` is fragile across frontends; not worth the complexity for the rare edge case.
- **Audio capture / video capture audio track.** SPU2's `IsAudioCaptureActive` path feeds `GSCapture::DeliverAudioPacket`, which is orthogonal — capture isn't supported in the libretro core yet (SP6 territory).
- **Volume controls.** RetroNest manages frontend-side volume. PCSX2's `EmuConfig.SPU2` volume settings still apply pre-callback; they're additive and harmless. No volume UI changes in SP4.
- **SP3.6 shutdown crash mitigation.** Audio teardown order is unaffected by the recompiler-JIT race; the crash is in `VMManager::SetState(Stopping)`, not SPU2. Out of scope.

## Architecture

```
              ┌──────────────────────────┐
              │  PCSX2 EE thread          │
              │  (Mixer.cpp produces      │
              │   64-frame stereo chunks) │
              └────────────┬──────────────┘
                           │ s_output_stream->WriteChunk(chunk)
                           │ (inherited from AudioStream base —
                           │  applies SoundTouch stretch, then
                           │  writes into the lock-free ring buffer)
                           ▼
            ┌──────────────────────────────┐
            │  AudioStream base ring buffer │   ← atomic rpos/wpos
            │  (50 ms @ 48 kHz = 2400 fr.)  │     (proven cross-thread by
            │  Owned by LibretroAudioStream │      Cubeb/SDL backends)
            └────────────┬─────────────────┘
                         │
                         │ ReadFrames(buf, n) — called by:
                         ▼
          ┌─────────────────────────────────────┐
          │  RetroNest main thread (retro_run)   │
          │                                      │
          │  1. wait on g_present_cv              │  ← signal from
          │  2. drain audio:                     │     Host::BeginPresentFrame
          │       n = ReadFrames(float_buf, max)│     (existing SP3 mechanism)
          │       float → int16 stereo          │
          │       audio_sample_batch_cb(buf, n) │
          │  3. retro_video_refresh(...)        │
          │  4. return                          │
          └─────────────────────────────────────┘
```

The drain piggybacks on the existing video-frame `g_present_cv` signal — same cadence libretro frontends expect. No new threads, no new synchronisation primitives.

### File layout

| Path | Change | Notes |
|---|---|---|
| `pcsx2/Host/AudioStreamTypes.h` | +1 line | Add `Libretro` to the `AudioBackend` enum, before `Count`. |
| `pcsx2/Host/AudioStream.cpp` | +4 lines (3 touches) | (a) Add `"Libretro"` to `s_backend_names`. (b) Add `TRANSLATE_NOOP("AudioStream", "Libretro")` to `s_backend_display_names`. (c) Add `case AudioBackend::Libretro: return CreateLibretroAudioStream(sample_rate, parameters, stretch_enabled, error);` to `CreateStream`, plus a one-line `extern std::unique_ptr<AudioStream> CreateLibretroAudioStream(...)` forward declaration in the same translation unit. `GetBackendName` / `GetBackendDisplayName` / `ParseBackendName` need no changes — they index the arrays. No header (`AudioStream.h`) edit needed: `CreateLibretroAudioStream` is a free function, defined in `pcsx2-libretro/LibretroAudioStream.cpp`, declared `extern` at the top of `AudioStream.cpp`. |
| `pcsx2-libretro/LibretroAudioStream.h` | NEW | Subclass declaration; public `DrainToLibretroCallback(retro_audio_sample_batch_t cb, u32 max_frames)`; static `s_active_stream` accessor. |
| `pcsx2-libretro/LibretroAudioStream.cpp` | NEW | Implementation. Mirrors `SDLAudioStream.cpp` shape: ctor, dtor sets/clears singleton; `BaseInitialize(StereoSampleReaderImpl, stretch_enabled)` since expansion is forced off. |
| `pcsx2-libretro/CMakeLists.txt` | +1 line | Add `LibretroAudioStream.cpp` to `target_sources`. |
| `pcsx2-libretro/Settings.cpp` | +~3 lines | Force `EmuConfig.SPU2.Backend = AudioBackend::Libretro`; force `EmuConfig.SPU2.StreamParameters.expansion_mode = AudioExpansionMode::Disabled`. |
| `pcsx2-libretro/LibretroFrontend.cpp` | +~10 lines | `retro_get_system_av_info`: set `timing.sample_rate = 48000.0`; `retro_set_audio_sample_batch`: cache cb in `g_frontend.audio_batch_cb`; `retro_run`: after present_cv signals, call `LibretroAudioStream::ActiveStream()->DrainToLibretroCallback(g_frontend.audio_batch_cb, MAX_FRAMES_PER_DRAIN)` if both non-null. |

**Total upstream-file deviation:** 5 lines across 2 files (`AudioStreamTypes.h`, `AudioStream.cpp`). All within `pcsx2/Host/`. Documented as the first sub-project where the integration seam is not in `Host::`. Discipline rule (project memory) updated to reflect that SP4 introduces a controlled, comment-flagged exception for the AudioBackend dispatch table specifically — no broader relaxation.

### Lifecycle

| Event | Behavior |
|---|---|
| `retro_set_audio_sample_batch` (frontend → core) | Cache cb in `Pcsx2Libretro::g_frontend.audio_batch_cb`. |
| `retro_load_game` | Settings.cpp runs (existing SP2 hook); sets `Backend=Libretro`, `expansion_mode=Disabled`. VMManager::Initialize → `SPU2::CreateOutputStream` → `AudioStream::CreateStream(Libretro, …)` → constructs `LibretroAudioStream`; ctor sets `s_active_stream`. |
| `retro_run` (per video frame) | Wait on `g_present_cv`. If `LibretroAudioStream::ActiveStream()` and `audio_batch_cb` are both non-null → drain available frames (capped at `MAX_FRAMES_PER_DRAIN`). |
| `SPU2::UpdateSampleRate` (rare; mid-game console-rate change) | PCSX2 destroys + recreates `s_output_stream` (verified order in `spu2.cpp:117`: `s_output_stream.reset()` precedes `CreateStream`). Old `LibretroAudioStream` dtor clears `s_active_stream` *before* the new ctor sets it. One frame of silence in the gap; acceptable. |
| `retro_unload_game` / `retro_deinit` | VMManager teardown → SPU2 destroys `s_output_stream` → `~LibretroAudioStream` clears `s_active_stream`. retro_run drain check sees null, skips cleanly. |
| Pause (Cmd+Shift+Escape → in-game menu) | `AudioStream::SetPaused(true)` from `VMManager::SetState(Paused)`. SPU2 stops calling `WriteChunk`; drain reads zero frames; libretro frontend's buffer naturally drains to silence. No subclass override needed — base `SetPaused` is a no-op for our case. |

### Error handling

| Failure | Behavior |
|---|---|
| `audio_batch_cb` not yet set when `retro_run` drains | Skip drain. Samples accumulate in ring (50 ms / 2400 frames headroom). Resolves on the first valid drain. |
| `s_active_stream` null (pre-VM-init or post-shutdown) | Skip drain. No crash. |
| Ring buffer overrun (drain consistently slower than produce) | AudioStream's existing behavior: `wpos` advances over `rpos`; oldest frames lost; libretro frontend hears a glitch. With 60 Hz drain at 48 kHz, ~800 frames produced per drain ≪ 2400 frame capacity — should never trigger in normal operation. Logged as a one-shot warning if detected (we sample a counter inside `DrainToLibretroCallback`). |
| `audio_batch_cb` returns less than requested | Libretro spec allows partial consumption. Re-queue the unconsumed remainder via a small staging buffer (≤ `MAX_FRAMES_PER_DRAIN × 2 × sizeof(int16_t)`, stack-allocated) so the next `retro_run` retries. |
| `CreateLibretroAudioStream` fails | Falls back to `CreateNullStream` via the existing `SPU2::CreateOutputStream` error path (already handles Cubeb/SDL failures the same way). VM keeps running silent; `Host::ReportErrorAsync` surfaces a toast. |

### Sample format conversion

- AudioStream emits `float SampleType` (nominal range ±1.0; clipping possible).
- Libretro wants `int16_t` interleaved stereo.
- Conversion per frame: `int16_t(std::clamp(f * 32767.0f, -32768.0f, 32767.0f))`.
- Staging buffer: stack-allocated per `retro_run` call (`int16_t staging[MAX_FRAMES_PER_DRAIN * 2]`), no heap churn. With `MAX_FRAMES_PER_DRAIN = 2048` that's 8 KB stack — comfortable.

### Frame budget

48000 / 60 = 800 stereo frames per video frame. We drain *all available* frames each call (capped at `MAX_FRAMES_PER_DRAIN = 2048` for safety; never exceeded in normal operation), not exactly 800 — the ring buffer naturally smooths jitter. Underrun (zero frames available) → push nothing; libretro frontend's own buffering hides it.

## Verification (testing strategy)

Following the SP3 pattern of "verify the integration end-to-end on a real game, log-driven correctness for the rest":

1. **Build & link sanity** — `cmake --build build --target pcsx2_libretro` succeeds; no undefined symbols for the free function `CreateLibretroAudioStream` (defined in `pcsx2-libretro/LibretroAudioStream.cpp`, declared `extern` in `AudioStream.cpp`).
2. **End-to-end smoke** — Launch Ratchet & Clank in RetroNest. Audio plays in sync with video. No glitches in the first 30 seconds. BIOS boot chime audible.
3. **Lifecycle** — Load game → unload → load again. No crash, audio works on the second load (verifies destructor cleanup of `s_active_stream` singleton).
4. **Pause/resume** — In-game menu (Cmd+Shift+Escape) → audio stops cleanly → close menu → audio resumes.
5. **Save state** — Save state mid-game, load it back. Audio continues without corruption.
6. **Stretch verification** — If RetroNest's libretro layer exposes a turbo / fast-forward path, verify audio is time-stretched (faster, preserved pitch) rather than chipmunked or skipped — confirms SoundTouch path is live.
7. **Diagnostic log line at first drain** — `INFO: LibretroAudioStream first drain: %u frames pushed` — one-shot, confirms wiring without spamming the log.

Explicitly **not in scope** for SP4 testing:

- Multi-channel/surround (deferred — `expansion_mode = Disabled`).
- PSX 44.1 kHz (deferred — see Non-goals).
- Audio capture / video capture audio track (orthogonal SPU2 path).

## Known limitations (carry into project memory after ship)

- **Stereo only.** 5.1 / 7.1 expansion forced off via `Settings.cpp`.
- **Sample rate hardcoded 48000.** PSX titles will play slightly off-pitch.
- **Forces `AudioBackend::Libretro`** unconditionally in our `Settings.cpp`. The user-visible PCSX2 audio-backend setting (when SP7 wires it through) must hide / disable backend selection in the libretro path, since switching to Cubeb/SDL inside the core would silently break audio routing.

## Discipline note (project memory update)

Project memory currently states: "never modify upstream files outside the single 4-line block in top-level CMakeLists.txt". SP4 is the first sub-project where this is technically violated — `pcsx2/Host/AudioStreamTypes.h` (+1 line) and `pcsx2/Host/AudioStream.cpp` (+5 lines, 4 small touches) are both edited.

The deviation is bounded and intentional:

- The integration seam is genuinely not in `Host::` — `AudioStream` is a class with a static factory baked into libPCSX2, with no public hook.
- Alternatives considered and rejected during brainstorming: (a) upstream PR adding a `Host::CreateExternalAudioStream` hook (blocks SP4 on upstream review timing); (b) weak-symbol replacement of `AudioStream::CreateStream` (fragile across compilers / rebases).
- Each edit is comment-flagged inline (`// pcsx2-libretro: …`) so the rebase reviewer immediately sees the deviation and can re-apply if conflict-resolution overwrites it.
- The rule remains in force for everything else. SP4 introduces a single, narrowly scoped exception, not a broader relaxation.

After SP4 ships, update `project_pcsx2_libretro_port.md` to note this exception and the reasoning, so future sub-projects don't accidentally widen it.

## Order-of-operations summary

1. Add `AudioBackend::Libretro` enum entry + dispatch case + name handling (upstream-file edits, comment-flagged).
2. Implement `LibretroAudioStream.{h,cpp}` mirroring `SDLAudioStream.cpp`'s shape, using stereo `BaseInitialize` and a singleton `s_active_stream` pointer.
3. Wire `Settings.cpp` to force `Backend = Libretro` and `expansion_mode = Disabled`.
4. Wire `LibretroFrontend.cpp`: cache `audio_batch_cb`, set `system_av_info.timing.sample_rate = 48000.0`, drain in `retro_run` after `g_present_cv`.
5. Build, run Ratchet & Clank, verify the seven-step verification list.
6. Ship; update project memory + create SP4-shipped session-handoff memory.

## Predecessors and successors

- **Predecessors:** SP1 (skeleton), SP2 (VM lifecycle), SP3 (HW render bridge), SP3.5 (UX overlays). All complete.
- **Successors:** SP5 (input — `retro_input_state_t` → PAD), then SP6 (save states + memcards), SP7 (settings push), SP8 (RetroNest adapter rewrite). SP3.6 (shutdown crash) remains parked; SP4 doesn't unblock it.
