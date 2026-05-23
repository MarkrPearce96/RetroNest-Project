# Dolphin libretro conversion — Design

**Date:** 2026-05-23
**Author:** Mark Pearce (with Claude)
**Status:** Approved for planning

## Goal

Convert the existing standalone Dolphin adapter in RetroNest into a libretro core, mirroring what was done for PCSX2 and PPSSPP. The user installs Dolphin (now bundled as `dolphin_libretro.dylib`) via RetroNest, scans GC/Wii ROMs, launches games, and Dolphin runs in-process — no separate Dolphin window, no Qt UI, no IPC for hotkeys.

This is a multi-month project. The spec is intentionally cross-repo (the core in `dolphin-libretro/`, the host adapter in `RetroNest-Project/`) and decomposes into 9 sub-projects (SP0–SP8) that each get their own implementation plan.

## Acceptance criteria

- Clean build: `cmake --build` succeeds for both `dolphin-libretro/` (Dolphin + the new libretro target) and `RetroNest-Project/cpp/`.
- All existing tests pass — RetroNest's existing test suite (`test_pcsx2_libretro_*`, `test_ppsspp_libretro_*`, `test_game_session_*`) must not regress.
- `dolphin` manifest's `backend` is `libretro` and the standalone `DolphinAdapter` is deleted from the registry.
- The dolphin libretro core builds as `dolphin_libretro.dylib` with the standard `retro_*` C ABI exported (visibility hidden otherwise).
- Schema-fidelity CMake target (`check_schema_fidelity`) passes — every key in the core's `CoreOptions*.cpp` matches a `SettingDef` in `DolphinLibretroAdapter::settingsSchema()` value-for-value.
- DolphinQt (standalone build) continues to build and run unchanged — no regression for non-libretro Dolphin users.
- Manual smoke (user runs end-to-end):
  - Launch a GameCube ROM via the normal in-app game-launch flow.
  - Verify controller input, audio, and rendering all work.
  - Switch `dolphin_renderer` setting between `Metal` and `Vulkan`, relaunch, verify the change took effect.
  - Save & Quit → relaunch from the tile's Resume button → verify state restored.
  - Repeat for a Wii ROM.

## Decisions baked in

These were settled with the user during brainstorming. Each is intentional and shapes the design downstream.

1. **Build from this vanilla Dolphin tree, mirroring PCSX2's pattern.** Not forking libretro/dolphin upstream. A new `Source/Core/DolphinLibretro/` target sits alongside `DolphinQt` / `DolphinNoGUI` and produces `dolphin_libretro.dylib`.
2. **Both Metal and Vulkan render backends ship in v1**, user-switchable via a `dolphin_renderer` core option (locked at session start, takes effect on next launch). Vulkan is the cross-platform path that future-proofs the planned Windows port; Metal is the macOS-optimal path that mirrors the proven PCSX2 NSView pattern.
3. **The standalone `DolphinAdapter` is deleted 1:1** when the libretro one ships. Same migration PCSX2 followed. The manifest id stays `"dolphin"` — only the `backend` field flips.
4. **Full feature parity with libretro's shared infra** — Save & Quit + Resume, Save/Load State hotkeys + in-game-menu icons, Fast-Forward toggle + HUD pill, in-app controller remap (GameCube + Wii Classic), RetroAchievements via RetroNest's `RcheevosRuntime` (not Dolphin's native `RetroAchievements.ini`).
5. **Full settings parity with the existing standalone schema.** All ~60 setting defs across Graphics (General/Enhancements/Hacks/Advanced/OSD), Audio, Advanced/Core, plus the Recommended rollup view. Each becomes a libretro core option *and* a host `SettingDef`, kept in sync by the schema-fidelity check.
6. **One design doc (this one), then the implementation plan decomposes into sub-projects** (SP0–SP8). Same shape PCSX2 used. Each sub-project gets its own writing-plans → executing-plans cycle.

## Investigation summary

| Question | Answer |
|---|---|
| Dolphin source tree state | Vanilla Dolphin checkout, not a git repo, no pre-existing libretro hooks. The directory is named `dolphin-libretro` only because that's the project the user is starting. |
| Existing standalone adapter | `RetroNest-Project/cpp/src/adapters/dolphin_adapter.{h,cpp}` (~2300 lines), registered in `adapter_registry.cpp:18`. Uses INI patching (Dolphin.ini, GFX.ini, GCPadNew.ini, WiimoteNew.ini, Hotkeys.ini, RetroAchievements.ini). Originally specced in `docs/superpowers/specs/2026-05-03-dolphin-adapter-design.md`. |
| Existing libretro pattern in RetroNest | Three adapters (mGBA software, PCSX2 Metal NSView, PPSSPP GL). All inherit from `LibretroAdapter`. Required-per-adapter overrides documented in `RetroNest-Project/memory/libretro-adapter-inventory.md`. |
| Dolphin video backends available | `Source/Core/VideoBackends/{D3D, D3D12, Metal, OGL, Vulkan, Software, Null}`. Metal and Vulkan are the targets for this project; the others are untouched. |
| Existing options.json infra | Already in place. `LibretroAdapter::libretroOptionsStore()` returns the live runtime's `OptionsStore` when a game is running, or a persistent fallback seeded from `optionsJsonPath()` (`{root}/emulators/libretro/{coreId}/options.json`). Dolphin's libretro adapter inherits this for free. |
| PCSX2 schema-fidelity check | Lives at `pcsx2-libretro/tools/check_schema_fidelity.py`, invoked via `cmake --build ... --target check_schema_fidelity`. Diffs core `CoreOptions*.cpp` against host `Pcsx2LibretroAdapter::settingsSchema()`. We port it for Dolphin. |
| Dolphin pause primitives | `Core::SetState(State::Paused)` halts CPU + Fifo threads cleanly. No `retronest_set_paused` custom symbol needed (PCSX2 needed one because EE/MTGS/MTVU kept running through libretro's pause). |
| RA console IDs | GameCube = 16, Wii = 19. Already mapped in `RetroNest-Project/cpp/src/services/ra_client.cpp` per the standalone-adapter spec. |

## Architecture

### Repository layout

```
dolphin-libretro/                          (this tree — Dolphin source + new libretro target)
├── Source/Core/
│   ├── DolphinLibretro/                   NEW — sibling to DolphinQt / DolphinNoGUI
│   │   ├── CMakeLists.txt                 MODULE library → dolphin_libretro.dylib
│   │   ├── LibretroFrontend.cpp/h         retro_* entrypoints
│   │   ├── EmuThread.cpp/h                BootManager + Core::Init/Stop coordinator
│   │   ├── Settings.cpp/h                 maps core options → Dolphin Config:: vars
│   │   ├── CoreOptions.cpp/h              option table dispatcher
│   │   ├── CoreOptionsGraphics.cpp/h      ~45 graphics options (General/Enhancements/Hacks/Advanced/OSD)
│   │   ├── CoreOptionsAudio.cpp/h         ~5 audio options
│   │   ├── CoreOptionsCore.cpp/h          ~10 core/CPU options
│   │   ├── LibretroAudioStream.cpp/h      SoundStream → retro_audio_sample_batch
│   │   ├── LibretroInputSource.cpp/h      ControllerInterface backend reading RetroPad
│   │   ├── LibretroSaveState.cpp/h        State::SaveToBuffer / LoadFromBuffer wrap
│   │   ├── LibretroMetalContext.cpp/h     Metal NSView handover
│   │   ├── LibretroVulkanContext.cpp/h    Vulkan via RETRO_HW_CONTEXT_VULKAN
│   │   ├── HostStubs.cpp                  Host_* implementations (no Qt)
│   │   ├── MacNSViewMetrics.mm            NSView size/scale queries
│   │   ├── libretro.h, libretro_vulkan.h, libretro-common/
│   │   └── tools/
│   │       ├── check_schema_fidelity.py   ported from pcsx2-libretro
│   │       └── test_settings_apply.cpp    unit test for option→Config:: mapping
│   └── VideoBackends/
│       ├── Metal/                         MODIFIED — accept host-provided NSView
│       └── Vulkan/                        MODIFIED — accept host-created VkInstance/VkDevice

RetroNest-Project/                         (host)
├── cpp/src/adapters/
│   ├── dolphin_adapter.{h,cpp}            DELETE
│   └── libretro/
│       ├── dolphin_libretro_adapter.{h,cpp}   NEW
│       └── libretro_adapter.h                 MODIFIED — HardwareRenderBackend::Vulkan
├── cpp/src/ui/libretro/
│   ├── libretro_metal_item.{h,mm}         existing (reused for Dolphin Metal path)
│   └── libretro_vulkan_item.{h,mm}        NEW
├── cpp/src/core/libretro/
│   └── core_runtime.{h,cpp}               MODIFIED — Vulkan HW-render branch
├── cpp/src/core/
│   └── game_session.{h,cpp}               MODIFIED — extend backend-string gates to "vulkan"
├── qml/AppUI/EmulationView.qml            MODIFIED — Vulkan Loader arm
├── manifests/dolphin.json                 MODIFIED — backend: "libretro"
└── cpp/tests/
    ├── test_dolphin_libretro_schema.cpp           NEW
    ├── test_dolphin_libretro_controller_schema.cpp NEW
    └── test_dolphin_libretro_resume.cpp           NEW
```

### Renderer selection mechanism

A single core option drives both sides:

- Core option `dolphin_renderer ∈ {"Metal", "Vulkan"}`, default `"Metal"`.
- Read by the core in `retro_load_game` to instantiate the chosen `VideoBackend`. Locked for the session — changing it takes effect on next launch (matches how Dolphin's own UI handles renderer switches).
- `DolphinLibretroAdapter::hardwareRenderBackend()` reads the same value from `libretroOptionsStore()` and returns `HardwareRenderBackend::MetalNSView` or `HardwareRenderBackend::Vulkan`. The QML Loader in `EmulationView.qml` picks the right host item (`LibretroMetalItem` vs the new `LibretroVulkanItem`) before the core boots.
- `GameSession::m_libretroBackend` (string-valued) takes `"metal"` or `"vulkan"` and is reset between sessions by the existing `CoreRuntime::finished` lambda (no new code needed beyond extending the gate set).

### Threading model

Dolphin owns its own CPU thread + Fifo/GPU thread + audio thread. The libretro adapter doesn't reimplement those — it gates them:

- `retro_load_game` calls `BootManager::BootCore(...)`, which spins up Dolphin's threads in their normal configuration.
- `retro_run` blocks the libretro-caller's thread for one frame: signal Dolphin's CPU thread to advance, wait for the video frame to arrive, drain audio samples to `retro_audio_sample_batch_t`, return.
- Pause: `Core::SetState(State::Paused)`. Dolphin's primitives handle the rest.
- `retro_unload_game`: `Core::Stop()` + `BootManager::Stop()`, tear down render context, uninstall input/audio backends.

## Components

### Core internals (`Source/Core/DolphinLibretro/`)

**Lifecycle (`LibretroFrontend.cpp`):**

| libretro entry | Dolphin-side action |
|---|---|
| `retro_init` | `UICommon::Init()`, `UICommon::CreateDirectories()`, point portable mode at libretro's `system/dolphin/` (via `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY`) |
| `retro_set_environment` | Declare core options, declare HW-render preference based on `dolphin_renderer`, request `RETRO_ENVIRONMENT_GET_VFS_INTERFACE` + RetroNest-specific extensions |
| `retro_load_game` | `BootManager::BootCore(BootParameters::GenerateFromFile(rom))`. Install `LibretroInputSource` into `ControllerInterface`, `LibretroAudioStream` into AudioCommon, hand chosen render context (Metal NSView or Vulkan) to `g_video_backend->Initialize()` |
| `retro_run` | Poll input → push pad state → advance one frame → drain video + audio → return |
| `retro_serialize` / `retro_unserialize` | `Core::RunOnCPUThread(...State::SaveToBuffer/LoadFromBuffer)`. Dolphin already supports in-memory savestates. |
| `retro_unload_game` | `Core::Stop()`, `BootManager::Stop()`, tear down render context, uninstall I/O backends |
| `retro_deinit` | `UICommon::Shutdown()` |

**`HostStubs.cpp`** implements the `Host_*` surface (normally provided by DolphinQt/DolphinNoGUI) as no-ops or libretro-routed equivalents — `Host_Message` routes to `retro_log_cb`, `Host_RequestRenderWindowSize` ignored (host owns the window), etc. Direct analog to `PlatformHeadless.cpp`.

**`EmuThread.cpp`** is a thin coordinator owning the BootManager handle, pause flag, and per-frame signal. Doesn't spawn threads itself — Dolphin's internal threading is preserved.

**`LibretroAudioStream.cpp`** subclasses `SoundStream`, buffers samples Dolphin generates, drains them via `retro_audio_sample_batch_t` each `retro_run`. Native sample rate stays Dolphin-default (32 kHz DSP-HLE, 48 kHz LLE); `retro_get_system_av_info` reports the matching rate.

**`LibretroInputSource.cpp`** registers a `ControllerInterface` backend named `Libretro` exposing virtual devices `Libretro/0/...`, `Libretro/1/...`. Default profiles (written by RetroNest at install time) use Dolphin's expression syntax — e.g. `` `Libretro/0/Button A` ``. Each `retro_run` polls `input_state_cb(port, RETRO_DEVICE_JOYPAD, ...)` and stores values for the backend to read.

**`LibretroSaveState.cpp`** wraps Dolphin's `State::SaveToBuffer` / `LoadFromBuffer`. `retro_serialize_size` reports the conservative max for the current `STATE_VERSION`.

**One thing to verify during SP2:** Dolphin's `BootManager` assumes a single-shot lifecycle within a process. Libretro frontends load/unload freely; latent leaks or stale-state issues may surface. PCSX2 hit the same thing and resolved it via EmuThread state-machine guarantees — analog likely needed.

### Render backends

#### Metal NSView path (mirrors PCSX2)

RetroNest creates the NSView (`LibretroMetalItem`, existing PCSX2 component, reused) and hands it to the core via `RETRO_ENVIRONMENT_GET_MACOS_NSVIEW` (extension already defined in RetroNest's `libretro.h`). Dolphin's Metal backend draws directly into the NSView's CAMetalLayer — no FBO, no IOSurface, no copy.

**Core-side modifications:**
- `LibretroMetalContext.cpp` requests the host NSView via the extension.
- `VideoBackends/Metal/MetalSwapChain.{h,mm}` gains `CreateFromHostView(NSView*)`. Existing `Create(WindowSystemInfo)` path stays for DolphinQt builds.
- `VideoBackends/Metal/MetalMain.mm`'s `VideoBackend::Initialize` discriminates: `WindowSystemType::Headless` + libretro-supplied NSView → host-view path. Anything else → existing path.

**Host-side:**
- No new QML item. `LibretroMetalItem` is render-backend-agnostic.
- `DolphinLibretroAdapter::hardwareRenderBackend()` returns `MetalNSView` when `dolphin_renderer == "Metal"`.

#### Vulkan path (new ground for RetroNest)

Standard libretro `RETRO_HW_CONTEXT_VULKAN`. The frontend creates the `VkInstance` / `VkDevice` (or accepts the core's choice via `retro_hw_render_context_negotiation_interface_vulkan`), the core renders into a `VkImage`, and the host imports that image into Qt's render pipeline.

**Core-side:**
- `LibretroVulkanContext.cpp` modeled on PPSSPP-libretro's existing `LibretroVulkanContext.cpp` (working reference in `ppsspp-libretro/libretro/`).
- Implements the negotiation interface so Dolphin's required device extensions can be declared up front.
- `VideoBackends/Vulkan/VulkanContext.cpp` gains `CreateFromExternal(VkInstance, VkDevice, queue_family_indices)` factory; existing self-creating path stays.
- `VideoBackends/Vulkan/SwapChain.{h,cpp}` gets a libretro mode that presents into a `VkImage` (handed to the frontend via `video_cb`) instead of a `VkSurfaceKHR`-backed swapchain.

**Host-side (new work):**
- `core_runtime.cpp` gains a Vulkan HW-render branch parallel to the existing GL one. Handles `RETRO_ENVIRONMENT_SET_HW_RENDER` with `RETRO_HW_CONTEXT_VULKAN`, installs the negotiation interface, creates/accepts the `VkInstance` + `VkDevice`, sets up per-frame `VkImage` retrieval via `retro_hw_render_interface_vulkan`.
- `LibretroVulkanItem.{h,mm}` — new QQuickItem. Holds the Vulkan-rendered `VkImage`, imports it as `MTLTexture` for Qt scene graph composite. Import path (direct MoltenVK→Metal vs IOSurface bridge) is decided by SP0 spike.
- `EmulationView.qml` Loader gets a `"vulkan"` branch.
- `LibretroAdapter::HardwareRenderBackend` enum extended with `Vulkan`.
- `GameSession::preShutdownRenderFence` gate currently `m_libretroBackend == "gl"`; extend to `gl || vulkan` if SP4 confirms the same teardown-race shape. (Metal path skips the fence — Dolphin's Metal backend composites synchronously, no separate render thread.)

### Settings schema mapping

The standalone adapter's ~60 setting defs across 4 categories all become libretro core options with the `dolphin_` prefix.

**Mapping shape:**

| Standalone field | Core option | Host `SettingDef` |
|---|---|---|
| `section=Settings`, `key=EFBScale` (GFX.ini) | `dolphin_efb_scale ∈ {"1x","2x",...,"8x"}` | `key="dolphin_efb_scale"`, `storage=LibretroOption` |
| `section=Core`, `key=CPUCore` (Dolphin.ini) | `dolphin_cpu_core ∈ {"Interpreter","Cached Interpreter","JIT"}` | matching |
| `section=Settings`, `key=MSAA` (GFX.ini) | `dolphin_msaa ∈ {"None","2x","4x","8x"}` | matching |
| ... ~60 total ... | | |

**Core-side mapping (`Settings.cpp`):** each libretro option key maps to a Dolphin `Config::Info<T>` setter:

```cpp
Config::SetCurrent(Main::DEFAULT_GFX_EFB_SCALE,
                   ParseEFBScale(GetOption("dolphin_efb_scale")));
Config::SetCurrent(Main::DEFAULT_CPU_CORE,
                   ParseCPUCore(GetOption("dolphin_cpu_core")));
// ... one mapping per option
```

This replaces the standalone adapter's GFX.ini/Dolphin.ini patching. The core writes directly to Dolphin's runtime `Config::` layer — the source of truth Dolphin's subsystems already read from.

**Persistence:** uses the existing `OptionsStore` / `options.json` infra. `LibretroAdapter::libretroOptionsStore()` provides the live + fallback stores; `optionsJsonPath()` returns `{root}/emulators/libretro/dolphin/options.json`. Dolphin inherits this from the base class — no new persistence code.

**Schema-fidelity guarantee:** the core's `CoreOptions*.cpp` tables and the host adapter's `settingsSchema()` MUST stay value-for-value in sync, else `OptionsStore::load`'s whitelist silently drops unrecognized values and users lose settings. We port PCSX2's `check_schema_fidelity.py` as a CMake target and add a host-side unit test (`test_dolphin_libretro_schema.cpp`) that loads the built `dolphin_libretro.dylib` and diffs at test time.

**What translates 1:1:**
- Graphics → General (resolution, vsync, force aspect/progressive, ...)
- Graphics → Enhancements (MSAA, anisotropic, postprocessing shader, scaled EFB copies, ...)
- Graphics → Hacks (skip EFB access, ignore format changes, XFB modes, ...)
- Graphics → Advanced (wireframe, show stats, validation layer when Vulkan, ...)
- Graphics → On-Screen Display (FPS, frame times, input display, ...)
- Audio (DSP HLE/LLE recompiler/LLE interpreter; audio backend fixed to libretro)
- Advanced/Core (CPU core, MMU, fast-mem, sync GPU, ...)
- Recommended rollup is a *view* — host adapter synthesizes it by re-listing a curated subset, identical shape to the standalone.

**What drops or moves:**
- RetroAchievements settings → RetroNest's shared `RcheevosRuntime` (drops `RetroAchievements.ini` patching).
- Interface/UI settings → not applicable (no Dolphin Qt UI).
- Per-game settings (`User/GameSettings/{ID}.ini`) → out of scope, same as standalone.
- Aspect ratio, FF, save/load hotkeys, pause-on-focus-loss → handled by RetroNest's shared `LibretroAdapter` infra.

### Host adapter (`DolphinLibretroAdapter`)

Concrete subclass of `LibretroAdapter`. Required overrides per the adapter inventory:

| Override | Returns |
|---|---|
| `coreId()` | `"dolphin"` |
| `hardwareRenderBackend()` | Reads `dolphin_renderer` from `libretroOptionsStore()`. `"Metal"` → `MetalNSView`, `"Vulkan"` → `Vulkan`. Default `"Metal"`. |
| `raConsoleId(systemId)` | `16` for `gc`, `19` for `wii`, `0` otherwise |
| `extractSerial(romPath)` | Reads 6-char Game ID at disc header offset `0x000`. Same for GC + Wii. |
| `findResumeFile(serial)` | `{PathOverridesStore("dolphin","SaveStates") ?? Paths::emulatorDataDir("dolphin",systemId) + "/savestates"}/{serial}.resume` |
| `controllerTypes()` | `[{"GCPad","GameCube Controller", svg}, {"WiiClassic","Wii Classic Controller", svg}]` |
| `controllerBindingDefsForType(type)` | GCPad: 6 buttons + d-pad + 2 sticks + L/R analog + Z + Start. WiiClassic: Classic Controller layout (ZL/ZR triggers, no Z). |
| `settingsSchema()` | ~60 `SettingDef`s — full standalone parity |
| `settingsHubCards()` | Graphics, Audio, Advanced, Recommended (4 cards) |
| `frontendSettingDefaults()` | Aspect mode, integer scale, etc. |
| `hotkeyBindingDefs()` | Pause, Save State, Load State, Toggle FF — walks the libretro hotkey registry |
| `pathsDefs()` | Save States, Screenshots, Memory Cards (GC), NAND (Wii) — 4 overrides |
| `biosFiles()` | Optional GameCube IPL per region (`gc/{jap,usa,eur}/IPL.bin`). Marked optional. |
| `aspectRatioOptions()`, `resolutionOptions()` | Override base — Dolphin resolution scale `1x`–`8x`, aspect `Auto/4:3/16:9/Stretch`. |

### Manifest

```json
{
  "id": "dolphin",
  "name": "Dolphin",
  "description": "Dolphin is a GameCube and Wii emulator...",
  "systems": ["gc", "wii"],
  "backend": "libretro",
  "libretro_core": "dolphin_libretro.dylib",
  "rom_extensions": ["iso","gcm","gcz","ciso","wbfs","rvz","wad","wia","nkit","m3u","dol","elf","tgc"]
}
```

Standalone-specific fields (`executable`, `install_folder`, `launch_args`, `github_repo`) are removed. The dylib ships bundled with RetroNest.

## Data flow

```
User launches GC/Wii ROM in RetroNest
   ↓
GameSession reads manifests/dolphin.json → backend="libretro"
   ↓
DolphinLibretroAdapter::hardwareRenderBackend() reads dolphin_renderer option
   ↓
m_libretroBackend = "metal" or "vulkan"
   ↓
EmulationView.qml Loader instantiates LibretroMetalItem or LibretroVulkanItem
   ↓
CoreRuntime loads dolphin_libretro.dylib, calls retro_init / retro_set_environment
   ↓
core declares options table, requests HW render context type
   ↓
core_runtime.cpp accepts (Metal NSView via extension, or Vulkan via SET_HW_RENDER)
   ↓
retro_load_game(rom_path)
   ↓
LibretroFrontend.cpp → BootManager::BootCore() → Dolphin's CPU/Fifo/Audio threads spin up
   ↓
LibretroInputSource installed into ControllerInterface
LibretroAudioStream installed into AudioCommon
chosen video backend (Metal or Vulkan) initialized with host-supplied context
   ↓
loop:
  retro_run() → poll input → advance one frame → drain video + audio → return
  RetroNest composites the frame via the host item
   ↓
User triggers Save & Quit:
  GameSession calls retro_serialize → writes {serial}.resume
  retro_unload_game → BootManager::Stop, render context torn down
  CoreRuntime::finished resets m_libretroBackend
```

## Error handling

Aligned with the existing libretro adapter conventions:

- **Core dylib missing / fails to load**: `CoreRuntime` already raises a typed error; user sees the standard "Core unavailable" surface. No Dolphin-specific handling.
- **ROM unsupported / corrupt**: Dolphin's `BootManager::BootCore` returns failure. Core reports via `retro_log_cb`; `retro_load_game` returns false; RetroNest surfaces as load-failed toast.
- **Render context creation fails** (Vulkan device unavailable, Metal device unavailable): core falls back to logging the error and returning false from `retro_load_game`. User sees load-failed. v1 doesn't auto-switch backends; user manually toggles the `dolphin_renderer` option.
- **Save state version mismatch**: Dolphin's `State::LoadFromBuffer` returns failure; `retro_unserialize` returns false; RetroNest cold-boots through BIOS instead of resuming.
- **Schema drift between core and host**: caught at build time by `check_schema_fidelity` target. Build fails with a diff. Caught at test time by `test_dolphin_libretro_schema`.

## Testing

| Layer | What it catches | Mechanism |
|---|---|---|
| Build-time schema fidelity | Core options drifting from host `settingsSchema()` | `check_schema_fidelity` CMake target — every build |
| Host unit tests | Adapter overrides return sane values; binding defs resolve; resume path correct | `test_dolphin_libretro_{schema, controller_schema, resume}.cpp` |
| Core unit tests | `Settings::apply()` maps each option to right `Config::Info<T>` setter | `dolphin-libretro/Source/Core/DolphinLibretro/tools/test_settings_apply.cpp` — links Common+Core only, no ROM needed |
| Smoke (manual) | Game boots, renders, takes input on both backends, both systems | Per-checkpoint script: Wind Waker (GC) + Mario Galaxy (Wii) on Metal then Vulkan; verify input + audio + save/load + resume |
| Regression | Vulkan enum + game_session changes don't break existing adapters | Existing `test_game_session_*`, `test_pcsx2_libretro_*`, `test_ppsspp_libretro_*` must continue to pass |
| DolphinQt regression | `VideoBackends/{Metal,Vulkan}` in-place changes don't break standalone Dolphin | Standalone DolphinQt smoke after each backend modification |

## Sub-project decomposition

Each gets its own `writing-plans`-produced plan + `executing-plans` cycle, executed in order.

| # | Sub-project | Ships | Gate |
|---|---|---|---|
| **SP0** | Spike — MoltenVK / Metal interop | 1-day timeboxed investigation. Resolves whether `LibretroVulkanItem` can import a MoltenVK-rendered `VkImage` directly as `MTLTexture`, or needs IOSurface bridging. | Written spike note + working PoC either way |
| **SP1** | Skeleton libretro core (Metal stub) | `DolphinLibretro/` target builds. Exports `retro_*` ABI. `retro_load_game` returns "no game" cleanly. Proves build wiring, HostStubs, EmuThread coordinator, cmake plumbing. | `dolphin_libretro.dylib` builds. RetroArch smoke loads it without crash. |
| **SP2** | Metal NSView render path | `LibretroMetalContext` + `VideoBackends/Metal` modifications. Wires `LibretroAudioStream` + `LibretroInputSource` at minimum-viable level. Hardcoded options. | A GameCube ISO boots and renders inside RetroNest via Metal path. Sound + input work. |
| **SP3** | Host adapter scaffold | `DolphinLibretroAdapter` exists. Manifest flips. Standalone `DolphinAdapter` deleted. Registry swapped. Resume + save-state hotkeys wired (shared infra). | RetroNest launches Dolphin libretro core end-to-end via the normal in-app game-launch flow. Resume button works. |
| **SP4** | Vulkan render path | `LibretroVulkanContext` + `VideoBackends/Vulkan` external-context support. `core_runtime` Vulkan HW-render branch. `LibretroVulkanItem` (using SP0's result). `EmulationView.qml` Loader extended. `dolphin_renderer` option becomes functional. | User can switch Metal ↔ Vulkan via core option. Both backends boot the same ROMs. |
| **SP5** | Controller mapping | `controllerTypes()` returns GCPad + WiiClassic. Full `controllerBindingDefsForType`. `LibretroInputSource` reads resolved RetroPad bindings each frame. | Controller mapping page shows both controllers. Remapping persists. |
| **SP6** | Settings schema — Graphics | Translates Graphics category (General/Enhancements/Hacks/Advanced/OSD, ~45 options) into core options + host schema. Schema-fidelity check passes. | All Graphics settings present in UI and take effect on next launch. |
| **SP7** | Settings schema — Audio + Advanced/Core | Audio (~5) + Advanced/Core (~10) options. Recommended rollup synthesized. | Full settings parity with standalone achieved. |
| **SP8** | RetroAchievements + polish | rcheevos end-to-end on both systems (RA console IDs 16/19). FF HUD verification. Hotkey suppression verification. Bundled-dylib install flow. All inventory pitfalls addressed. | Full feature parity. Manual acceptance test passes for both GC + Wii on both backends. |

**Horizon:** comparable to PCSX2's 5-month conversion. Each SP is a 1–3 week chunk for active development; SP6 and SP7 may be longer due to surface area.

## Risk register

| # | Risk | Mitigation |
|---|---|---|
| 1 | **MoltenVK ↔ Metal texture sharing** is unknown territory. Direct MTLTexture import via MoltenVK's `MVK_KHR_external_memory_metal` may not exist on target macOS versions. | SP0 spike resolves before any Vulkan integration commits. IOSurface bridge always works as fallback (adds a copy). |
| 2 | **Dolphin `BootManager` single-shot lifecycle assumption** — libretro frontends load/unload freely. Latent leaks or stale state may surface. | EmuThread state-machine guard (analog of PCSX2's solution). Will manifest during SP2/SP3, mitigation patterned on PCSX2. |
| 3 | **`VideoBackends/{Metal,Vulkan}` in-place modifications** touch Dolphin's hot path. Risk of regressing DolphinQt's standalone build. | Factory-method discrimination on `WindowSystemType` — DolphinQt's existing path is byte-for-byte unchanged. Verified by DolphinQt smoke after each backend modification. |
| 4 | **Schema drift between core and host** silently drops user settings. | Build-time CMake target + test-time unit test, both catching drift. |
| 5 | **CMake build undoes macdeployqt** (pitfall #2 in adapter inventory). Affects RetroNest builds, not Dolphin. | Re-run macdeployqt + codesign after every CMake build. Document in new-adapter checklist update. |
| 6 | **Vulkan teardown race shape unknown** — may or may not need the `preShutdownRenderFence` gate extension. | Verify during SP4 smoke. If observed, extend gate to `gl || vulkan`. |
| 7 | **`extractSerial` for GC/Wii** may need format-specific handling (`.iso` vs `.rvz` vs `.wbfs`). | SP3 verifies against a representative ROM set; falls back to delegating to `DiscIO` via a small helper in the dylib if header-only read is insufficient. |

## Reference files

- `RetroNest-Project/cpp/src/adapters/libretro/libretro_adapter.{h,cpp}` — base class
- `RetroNest-Project/cpp/src/adapters/libretro/{mgba,pcsx2,ppsspp}_libretro_adapter.{h,cpp}` — concrete adapters to mirror
- `RetroNest-Project/cpp/src/adapters/dolphin_adapter.{h,cpp}` — the standalone adapter being replaced (source of truth for settings schema until SP6/SP7 port it)
- `RetroNest-Project/cpp/src/core/libretro/{core_runtime, options_store, frontend_settings_store}.{h,cpp}`
- `RetroNest-Project/cpp/src/core/game_session.{h,cpp}` — session lifecycle, fence, FF state, resume cfg
- `RetroNest-Project/manifests/{pcsx2,ppsspp,mgba}.json` — libretro manifest shape to mirror
- `RetroNest-Project/memory/libretro-adapter-inventory.md` — required overrides + pitfalls
- `RetroNest-Project/docs/superpowers/specs/2026-05-03-dolphin-adapter-design.md` — standalone Dolphin spec (the surface being preserved)
- `pcsx2-libretro/pcsx2-libretro/` — closest reference for a complex emulator converted via this pattern
- `pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py` — ported for Dolphin
- `ppsspp-libretro/libretro/LibretroVulkanContext.cpp` — Vulkan-context reference for SP4
- `dolphin-libretro/Source/Core/DolphinNoGUI/` — Host_* surface implementation pattern for `HostStubs.cpp`
- `dolphin-libretro/Source/Core/VideoBackends/{Metal,Vulkan}/` — backends modified in-place
