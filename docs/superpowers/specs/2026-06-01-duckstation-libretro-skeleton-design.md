# DuckStation libretro core — Phase 1 skeleton boot (design spec)

**Date:** 2026-06-01
**Status:** Approved design, pending implementation plan.
**Companion:** `duckstation-libretro/docs/swanstation-delta-2026-06-01.md` (the drift investigation that gates this).

## Goal (one line)

Build `duckstation_libretro.dylib` (universal arm64+x86_64) and the minimal RetroNest wiring so a PS1 game boots end-to-end through RetroNest — load, render, sound, digital-pad input, clean exit — replacing the current standalone DuckStation integration with an in-process libretro core, the same way PCSX2/PPSSPP/Dolphin/mGBA already load.

This is the **skeleton-boot phase only.** It deliberately defers settings-schema migration, RetroAchievements, full controller support, save states, hotkeys, and resume — each is its own follow-on spec.

## Hard constraints (from the project handoff — do not relitigate)

1. **License (CC BY-NC-ND 4.0).** This DuckStation fork and the produced `.dylib` **never leave the user's machines** — no push to any remote (public or private), no sharing, no bundling into any distributed artifact. The public/commercial RetroNest variant uses SwanStation instead and never ships this core. Personal-use-only reproduction is within the license's NonCommercial grant.
2. **Source base = Option B, reframed.** Fork **current upstream DuckStation** (`duckstation-libretro/duckstation-src/`). SwanStation's libretro shim (`swanstation/`, pre-relicense GPLv3) is a **reference only** — its `HostInterface`/`HostDisplay` base classes no longer exist upstream, so the shim cannot be ported as code (see delta report). We write fresh frontend code against the current `Host::`/`Core::`/`System::` + `GPUDevice` surface, using SwanStation for the libretro-facing shape and `pcsx2-libretro` as the modern structural template.
3. **Structural template = `pcsx2-libretro`.** Closest of the three reference forks (PS-class complexity, in-tree libretro frontend, `HardwareRenderBackend::MetalNSView` on macOS).

## Decisions locked during brainstorming (2026-06-01)

| # | Decision | Choice |
|---|---|---|
| 1 | Bring-up path / done criteria | **Straight to RetroNest** — no intermediate standalone-RetroArch milestone; build to RetroNest's libretro contract and test through RetroNest. |
| 2 | Install / distribution model | **Local-only** — omit `github_repo` from the manifest; hand-place the dylib; guard RetroNest's update-check loop against empty `github_repo`. Honors the never-push rule. |
| 3 | Renderer path | **MetalNSView direct-present** — DuckStation's `MetalDevice` builds its `CAMetalLayer` on RetroNest's provided NSView and presents directly; no software-framebuffer / `retro_video_refresh` pixel path. Matches PCSX2. |
| 4 | Run loop / threading | **Single-threaded inline** — force `VideoThread` non-threaded, run `System::Execute()` and interrupt at `FrameDone` so one frame runs per `retro_run`, presenting inline. |

## Architecture & file layout

### DuckStation fork (the core)

New in-tree libretro frontend target at `duckstation-src/src/duckstation-libretro/`, mirroring `pcsx2-libretro/pcsx2-libretro/`.

| File | Role |
|---|---|
| `libretro.cpp` | All `retro_*` entry points: init/boot/run/unload, environment-callback plumbing, the single-frame driver. (Analogue of PCSX2's `LibretroFrontend.cpp`.) |
| `libretro_host.cpp` | Implements the fixed `Host::` callback contract the core links against (`OnSystem*` lifecycle, OSD, error reporting, resource/thread/settings hooks). **The main net-new surface vs. SwanStation** — these were class virtuals before, are free functions now. |
| `libretro_settings.cpp` | Populates a stock `INISettingsInterface` BASE layer from libretro core options; sets `EmuFolders::Bios`. |
| `libretro_audio.cpp` | Bridges DuckStation's `AudioStream` to `retro_audio_sample_batch`. Model on PCSX2's `LibretroAudioStream`. |
| `libretro_core_options.h` | Phase-1-minimal core-option table (renderer, region, a couple of video knobs). Pared down from SwanStation's 102 KB version. |
| `CMakeLists.txt` | The `duckstation_libretro` dylib target. |

**No `libretro_host_display` file.** The renderer is driven by handing the existing `GPUDevice`/`MetalDevice` RetroNest's NSView via `WindowInfo` — this is the payoff of decision #3 and removes the single largest rewrite the delta report identified.

### RetroNest (the frontend) — three small touches

- `cpp/src/adapters/libretro/duckstation_libretro_adapter.{h,cpp}` — skeleton subclass of `LibretroAdapter`, modeled on `Pcsx2LibretroAdapter`'s skeleton phase.
- Registration in `cpp/src/adapters/adapter_registry.cpp`.
- `manifests/duckstation.json` swapped to libretro form (no `github_repo`) + a guard in `cpp/src/services/emulator_service.cpp`.

The existing standalone `cpp/src/adapters/duckstation_adapter.{h,cpp}` (1660 lines) stays in place during Phase 1 as the reference for the eventual UI surface; it is retired only at feature parity (mirroring how Dolphin was retired).

## Data flow

### A. `retro_set_environment`
Register the core-option table; query `GET_LOG_INTERFACE`; declare `MetalNSView` HW intent; set pixel format; cache `environ_cb`.

### B. `retro_load_game` (init + boot)
1. **Settings:** map libretro core options → `INISettingsInterface::SetStringValue(section,key,…)` on the BASE layer (delta §3 path #1). Set `EmuFolders::Bios` from `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` (delta §6). Set `memory_card_types[0] = NonPersistent` (delta §6 — `MemoryCardType::Libretro` is gone). Renderer left `Automatic` → Metal on macOS.
2. **NSView:** `environ_cb(RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW /*0x20001*/, &view)`; store into `WindowInfo{ type = MacOS, window_handle = view }`.
3. **Boot:** `System::BootSystem(SystemBootParameters{path}, &error)` (delta §1 — new `Error*`). DuckStation's boot path loads the BIOS via `BIOS::GetBIOSImage(region)` and creates the `GPUDevice`/`MetalDevice`, which builds its `CAMetalLayer` on our NSView (delta §2, confirmed `metal_device.mm:222`). Force `VideoThread` inline.
4. Report geometry via `SET_SYSTEM_AV_INFO` from the booted region's framebuffer dimensions.

### C. `retro_run` (one frame, single-threaded inline)
1. `retro_input_poll`; translate RetroPad → `Controller::SetBindState(index, value)` on `System::GetController(0)` (delta §4: digital buttons = 0/1).
2. Run exactly one frame: enter `System::Execute()`, interrupt at the `FrameDone` boundary via `System::InterruptExecution()` so it returns after one frame (delta §1). Present happens inline to the Metal layer.
3. Signal frame-ready: `video_refresh(RETRO_HW_FRAME_BUFFER_VALID, w, h, 0)` (pixels already on the layer; matches RetroNest's HW-frame handling at `core_runtime.cpp:91`).
4. Drain one host-frame of audio from `AudioStream` → `retro_audio_sample_batch`.
5. Re-emit `SET_SYSTEM_AV_INFO`/geometry only on region/aspect change.

### D. `retro_unload_game` / `retro_deinit`
`System::ShutdownSystem(false)` (delta §1 rename); tear down `GPUDevice`; detach from NSView. RetroNest's `LibretroMetalItem` dtor already defers QWindow teardown, so cross-boundary ordering is handled on its side.

### Frontend-provided `Host::` callbacks (`libretro_host.cpp`)
`Host::OnSystem*` lifecycle hooks → log/no-op for the skeleton; OSD → log; errors → log + `RETRO_ENVIRONMENT_SET_MESSAGE`. This fixed contract (`system_private.h:73-115`, `host.h`) replaces SwanStation's overridden virtuals.

## Input (skeleton scope)
One controller on port 0. Attach via `Pad::SetController(0, Controller::Create(ControllerType::DigitalController, 0))` (delta §4 — `Create` signature unchanged). Map the 16-bit RetroPad bitmask to `SetBindState(static_cast<u32>(DigitalController::Button::Cross), pressed ? 1.0f : 0.0f)` (and the rest of the digital button enum) each frame. **DigitalController only** for the skeleton (matches PCSX2's skeleton). Analog/`AnalogController` (half-axis splitting), multitap, and rumble (base `GetVibrationMotorStrength` is gone) are deferred.

## Build system
New `duckstation_libretro` CMake target → `duckstation_libretro.dylib`, built **universal arm64+x86_64** (RetroNest all-cores-universal policy; matches `pcsx2-libretro`'s per-arch build + `lipo`). Links DuckStation `core` + `util` + `common`. **Open sub-task:** confirm what runtime resources `MetalDevice` needs (e.g. a compiled Metal shader library) and bundle them next to the dylib the way PCSX2 ships `pcsx2_libretro_resources/`.

## RetroNest wiring detail
1. **Adapter** (`duckstation_libretro_adapter.{h,cpp}`): `coreId()="duckstation"`, `hardwareRenderBackend()=MetalNSView`, `raConsoleId()=0` (deferred). No controller/settings schema overrides yet (base returns empty — same as the PCSX2 skeleton).
2. **Registry:** add to `adapter_registry.cpp`.
3. **Manifest** (`manifests/duckstation.json`): `id:"duckstation"`, `systems:["psx"]`, `backend:"libretro"`, `executable:"duckstation_libretro.dylib"`, `core_dylib:"duckstation_libretro.dylib"`, `install_folder:"libretro"`, `launch_args:[]`, `rom_extensions` unchanged, **no `github_repo`**.
4. **Installer guard** (`emulator_service.cpp`, ~line 282): skip the `GitHubClient::fetchLatestRelease(item.githubRepo)` update check when `github_repo` is empty, and don't surface an Install/Update action for such cores. `adapter->isInstalled()` already keys off dylib presence, so a hand-placed dylib reads as installed.

## Definition of done (Phase 1)
Launch a PS1 game from RetroNest and observe, through RetroNest directly:
1. Boots through BIOS to the game.
2. Video renders in the RetroNest emulation view via the Metal layer.
3. Audio plays.
4. The digital pad controls the game.
5. Exit/unload is clean (no crash on view teardown).

## Out of scope (deferred to follow-on specs)
- The 1660-line standalone settings-schema migration (its own multi-week sub-project).
- RetroAchievements (`raConsoleId`, `SET_MEMORY_MAPS`).
- Full controller types, analog axes, multitap, rumble.
- **Save states** — delta §5: `retro_serialize` maps cleanly to `System::SaveStateDataToBuffer`, but `retro_unserialize` has **no public load-from-buffer** entry point; needs a small (~5–10 line) `System::LoadStateDataFromBuffer` core addition. Flag for that spec.
- Hotkeys, resume-on-launch.
- The public/SwanStation RetroNest variant.

## Flagged risks (carry into the implementation plan)
1. **Renderer assumption (validate first).** Confirm DuckStation's `MetalDevice` accepts RetroNest's NSView (via `WindowInfo.window_handle`, `WindowInfoType::MacOS`) and presents correctly inline. If it insists on owning the layer in a way that conflicts with RetroNest's compositing, fall back to driving `VideoPresenter::RenderDisplay(offscreen GPUTexture)` + handing the `MTLTexture` to RetroNest (delta §2 fallback).
2. **One-frame stepping.** Confirm inline `VideoThread` mode + `InterruptExecution`-at-`FrameDone` yields clean one-frame-per-`retro_run` stepping without throttle/pacing fighting the host (delta §1). Threaded + present-CV handshake (PCSX2's model) is the fallback if inline stepping misbehaves.
3. **Metal runtime resources** — see build sub-task; a missing shader library would fail GPU init silently.

## Reference paths
- Delta report: `duckstation-libretro/docs/swanstation-delta-2026-06-01.md`
- Current DuckStation fork: `duckstation-libretro/duckstation-src/` (CC BY-NC-ND — never push)
- SwanStation reference (delete after implementation): `swanstation/`
- PCSX2 template: `pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`, `cpp/src/adapters/libretro/pcsx2_libretro_adapter.{h,cpp}`
- RetroNest libretro base: `cpp/src/adapters/libretro/libretro_adapter.{h,cpp}`; MetalNSView consumer `cpp/src/ui/libretro/libretro_metal_item.mm`; HW-frame handling `cpp/src/core/libretro/core_runtime.cpp:91`; NSView callback `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW = 0x20001`.

---

# Implementation Outcome — Phase 1 COMPLETE (2026-06-03)

Phase 1 was implemented and **verified working through RetroNest**: a PS1 game (Crash Bandicoot) boots, renders (Metal→NSView), plays sound (libretro audio path), takes digital controller input with correct binds, and exits/re-loads cleanly. Universal arm64+x86_64 dylib, reproducibly built+deployed via `package.sh`. Below is what was actually built, including deviations from this spec discovered during implementation.

## Fork core-touches (DuckStation source modifications — the spec assumed minimal/none)
All in `duckstation-libretro/duckstation-src/`, branch `master`, local-only (never pushed):
1. **`System::RunFrame()`** (`core/system.h` + `core/system.cpp`, commit `69c74a9`) — a thin single-frame driver (the body of `Execute()`'s `Running` case, run once). Needed because `System::Execute()` is a `for(;;)` loop that never yields per-frame; this was the spec's pre-approved risk-#2 fallback. `retro_run` calls `RunFrame()`; `Host::PumpMessagesOnCoreThread`→`System::InterruptExecution()` at `FrameDone` makes the inner `CPU::Execute()` return after one frame.
2. **Audio capture mode** (`util/core_audio_stream.{h,cpp}` + `core/spu.cpp` routing, commit `3a93303`) — a libretro-flagged capture mode in `CoreAudioStream`: opens no hardware device (no Cubeb), keeps SPU samples flowing into a capture ring, drained synchronously each `retro_run` into `retro_audio_sample_batch`. Guarded by a runtime flag; the normal Qt/regtest audio path is untouched. (Plain `AudioBackend::Null` sets `m_paused` and drops samples, so "Null + read ring" was not viable.)
3. **Cross/Circle face-button swap** (`libretro_map.cpp`, commit `c7e5d47`) — user preference; our original mapping was the standard libretro PS1 default (B→Cross, A→Circle), swapped to A→Cross, B→Circle on request.

## New libretro frontend (in `duckstation-src/src/duckstation-libretro/`)
`libretro.cpp` (retro_* entry points, run loop, RetroPad→controller, lifecycle bootstrap), `libretro_host.cpp` (full `Host::` contract), `libretro_settings.{h,cpp}` (`ApplySettings`), `libretro_map.cpp` (`MapRetroPadToDigital`, unit-tested), `libretro_audio.{h,cpp}` (capture drain + `FramesPerHostFrame`), `libretro_window.mm` (`Host::AcquireRenderWindow` → NSView), `libretro_core_options.h`, `libretro_internal.h`, `libretro.h`, `CMakeLists.txt`, `libretro_settings_test.cpp`, `package.sh`, `BUILD_NOTES.md`.

## Key implementation facts (corrections to the spec's assumptions)
- **Startup bootstrap is required** (the spec under-specified this). `retro_init` → `Core::ProcessStartup` (allocates the guest memory arena + JIT + ryml — without it `LoadBIOS` segfaults on a null `Bus::g_bios`); `retro_load_game` → `ApplySettings` → `Core::CoreThreadInitialize` (populates `g_settings`, starts the video thread / command FIFO — without it `CreateGPUBackend` crashes) → `System::BootSystem`; `retro_unload_game` → `System::ShutdownSystem(false)` (per-game, re-bootable — without it teardown asserts on a live `g_gpu_device`); `retro_deinit` → `Core::CoreThreadShutdown` + `Core::ProcessShutdown`. Mirrors `duckstation-regtest`.
- **Host contract**: linked against `core util common scmversion`. `Host::GetCoreThreadHandle/IsOnCoreThread/QueueAsyncTask/WaitForAllAsyncTasks` are **core-owned** in this revision (NOT frontend-provided — the spec/plan listed them as ours). Renderer present: `Host::FrameDoneOnVideoThread` is a **no-op** — `GPUBackend::HandleSubmitFrameCommand` self-presents to the swapchain once the Metal device exists on our NSView; `Host::AcquireRenderWindow` returns `WindowInfo{MacOS, window_handle=<NSView from 0x20001>}` and `MetalDevice` builds its own `CAMetalLayer` on it.
- **Settings injection** (`ApplySettings`): `Core::InitializeBaseSettingsLayer("")` (empty path → defaults, no disk I/O), then base-layer keys: `Console/Region=Auto`, `GPU/Renderer=Software`, `GPU/UseThread=false` (inline), `MemoryCards/Card1Type=NonPersistent`, `BIOS/SearchDirectory=<libretro system dir>` (absolute — `EmuFolders::LoadConfig` reads this, NOT the `EmuFolders::Bios` var), `Audio/Backend=Null`. Plus `EmuFolders::DataRoot = parent-of-system-dir` (writable), `EmuFolders::Resources = <dir-of-dylib>/duckstation_libretro_resources` (dladdr).

## RetroNest-side changes (branch `feat/duckstation-libretro-skeleton`)
- `cpp/src/core/manifest_loader.cpp` — allow empty `github_repo` when `backend=="libretro"`.
- `cpp/src/services/emulator_service.cpp` — skip the update-check loop for empty `github_repo`.
- `cpp/src/adapters/libretro/duckstation_libretro_adapter.{h,cpp}` — skeleton adapter: `coreId`, `hardwareRenderBackend=MetalNSView`, `raConsoleId=0`, **`controllerTypes()` + `controllerBindingDefsForType()`** (14 PS1 buttons — required for input; the InputRouter needs binding defs or nothing routes).
- `cpp/src/adapters/adapter_registry.cpp` — register libretro adapter (was process `DuckStationAdapter`).
- `manifests/duckstation.json` — libretro form, no `github_repo`.

## Build + deploy (reproducible)
- **Build flags (machine-specific, in `BUILD_NOTES.md`):** `MACOSX_DEPLOYMENT_TARGET=13.3`, `-DENABLE_OPENGL=OFF`, `-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON` (works around a Homebrew libpng-without-APNG conflict on this machine), `-DENABLE_LIBRETRO=ON`. Prebuilt deps at `dep/prebuilt/macos-universal/`.
- **`package.sh`** builds universal (arm64+x86_64 lipo), compiles `metal_shaders.metallib`, assembles `duckstation_libretro_resources/` (= `data/resources/` + metallib), and deploys 3 artifacts:
  1. `duckstation_libretro.dylib` → `<RetroNest root>/emulators/libretro/cores/`
  2. `duckstation_libretro_resources/` → beside the dylib (core reads `EmuFolders::Resources`; `MetalDevice::LoadShaders` reads the metallib)
  3. `libshaderc_shared.dylib` + `libspirv-cross-c-shared` chain → `<RetroNest.app>/Contents/Frameworks/` (DuckStation's ImGuiManager loads these dynamically, resolved relative to the host exe's `../Frameworks`)
- **Caveat:** rebuilding RetroNest regenerates the `.app` and wipes the Frameworks libs → re-run `package.sh`. Long-term fix (deferred): bundle shaderc/spirv-cross into RetroNest's personal-variant build.

## Deferred to follow-on specs
RetroAchievements (`raConsoleId`/`SET_MEMORY_MAPS`); **save states** (`retro_serialize` maps to `System::SaveStateDataToBuffer`; `retro_unserialize` needs a small `System::LoadStateDataFromBuffer` core addition — delta §5); analog/multitap/rumble controllers; hotkeys; resume-on-launch; the 1660-line settings-schema migration; RetroNest volume-control verification (depends on RetroNest audio settings UI); bundling shaderc/spirv-cross into RetroNest's build; controller-label alignment after the Cross/Circle swap; the public/SwanStation variant.

## Commit trail (fork `master`)
`17afd38` build recipe → `bfe142f` stub → `852b9a9` Host link → `6ff9751` options+mapping → `b7b8723` settings → `8b0425f`/`86078c0`/`69c74a9` boot+run-loop+RunFrame → `cd9d240` NSView render → `a9d7fe2`/`1e16270`/`8a1f16d`/`bf644e7` boot-bringup fixes → `7c5e090` teardown → `3a93303` audio → `eb1d47b` input → `c7e5d47` Cross/Circle → `1238772` package.sh. (RetroNest changes on `feat/duckstation-libretro-skeleton`.)
