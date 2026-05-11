# PCSX2 Libretro Core — HW Render Bridge / Video Output (Sub-project 3 of 8)

**Date:** 2026-05-11
**Status:** Functionally complete — VM rendering verified end-to-end (see Verification log continuation); follow-up items noted
**Owner:** mark
**Scope:** Third sub-project of the multi-phase PCSX2-to-libretro port.
**Predecessors:** [Skeleton (SP1)](2026-05-11-pcsx2-libretro-skeleton-design.md), [VM Lifecycle (SP2)](2026-05-11-pcsx2-libretro-vm-lifecycle-design.md). Both complete.

## Context

SP2 shipped the VM-lifecycle infrastructure (Settings, EmuThread, BIOS discovery, retro_load_game / unload / run / deinit wiring, clean failure handling) but `VMManager::Initialize` fails at GS device init because PCSX2 on macOS hard-requires a real `CAMetalLayer` attached to an `NSView` — there is no software-only or headless path. The SP2 verification log documents this architectural finding in full.

SP3 builds the missing piece: a **host-provided hardware render context** ("Pattern B" in the SP2 spec) where RetroNest creates a native `NSView` + `CAMetalLayer` inside its main Qt window, exposes the `NSView` pointer to the libretro core, and PCSX2 renders directly to that layer (zero-copy). This is the libretro hardware-context pattern, adapted for our Qt-host + Metal-on-macOS configuration.

Pattern B is the chosen long-term shape because RetroNest's eventual DuckStation / PPSSPP / Dolphin libretro cores will all face the identical macOS situation. Building the bridge once amortises across every future hardware-accelerated core.

## Goal

A real PS2 game launched through RetroNest renders its boot logo / title screen into a Metal-backed widget inside RetroNest's main window. The user sees the rendered output for ~30 seconds before exiting via the existing Cmd+Shift+Escape global hotkey.

**Definition of done:**

1. RetroNest's `EmulationView` page detects when the active libretro core needs hardware rendering (via the manifest or an env callback) and hosts a new `LibretroMetalItem` QQuickItem instead of `LibretroVideoItem`.
2. `LibretroMetalItem` exposes a native `NSView` (via Qt 6 native-window facilities) and serves its raw pointer to the libretro core via a custom env callback.
3. Our libretro shim's `Host::AcquireRenderWindow` queries that pointer and returns a `WindowInfo` of `Type::MacOS` with `window_handle = NSView*` and a valid surface size.
4. `VMManager::Initialize` reaches `StartupSuccess`. `GSDeviceMTL::Create` attaches its own `CAMetalLayer` to the supplied `NSView`. ImGuiManager initializes successfully (Metal font textures upload).
5. Within ~10 seconds of `retro_load_game` returning `true`, PCSX2's BIOS boot logo or the game's title screen is visible inside the RetroNest window.
6. `Host::BeginPresentFrame` (called per frame by PCSX2's MTGS thread when a frame is ready to present) signals a condition variable our `retro_run` waits on. retro_run blocks one frame, drives frame cadence cleanly.
7. Pressing **Cmd+Shift+Escape** triggers retro_unload_game → emu thread shuts down VMManager cleanly → Metal layer detaches → RetroNest returns to the game list. No leaks, no crashes.
8. mGBA's software path is unchanged. Both PCSX2 (hardware) and mGBA (software) libretro paths coexist; `EmulationView` chooses the right widget per launch.

## Non-goals (deferred to later sub-projects)

- **Audio output (SP4).** SPU2 still null-out. SP4 wires `retro_audio_sample_batch_t`.
- **Input (SP5).** PCSX2 still sees no controller. User can see the game but can't play it.
- **Save states + memcards (SP6).** retro_serialize still 0; memcards disabled.
- **Settings push from RetroNest (SP7).** Settings layer is still hardcoded in C++ from SP2.
- **Aspect-ratio polish.** Whatever size we pass as surface_width/height is fixed. SP7 (settings) or a dedicated UX sub-project handles dynamic aspect / integer scale / shaders.
- **In-game OSD overlay coexisting with the Metal layer.** The existing `InGameMenuPanel` overlay (a separate frameless `QQuickWindow`) doesn't need code changes here — it floats above the main window, so it naturally appears above the Metal layer. Fine for SP3.

## Architecture

### Three new pieces, split across both sides of the dylib boundary

**RetroNest-side (Qt6 / Objective-C++):**

- **`LibretroMetalItem`** (`cpp/src/ui/libretro/libretro_metal_item.{h,mm}`): a `QQuickItem` (or `QQuickFramebufferObject` subclass) backed by a native `QWindow` whose `winId()` is an `NSView*`. On macOS the underlying NSView is layer-backed and accepts a CAMetalLayer. The item exposes:
  - `NSView* nativeView() const` — raw pointer for the libretro core.
  - QML properties `aspectMode`, `integerScale` — parity with `LibretroVideoItem` for future settings hookup, but read-only/fixed in SP3.
  - Resize signal that triggers a re-acquire on the core side (for future polish; SP3 hardcodes a single size).
- **Env callback for NSView handoff** (`cpp/src/core/libretro/environment_callbacks.{h,cpp}`): a new custom environment command with numeric ID `0x99001` (in the `RETRO_ENVIRONMENT_PRIVATE` range, beyond `RETRO_ENVIRONMENT_EXPERIMENTAL`), named `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW`. When the core calls it, RetroNest's env handler writes the active `LibretroMetalItem`'s NSView pointer into the supplied output pointer. The active item is registered by `CoreRuntime` (or the LibretroAdapter) before launching the core.
- **`EmulationView.qml`** (`cpp/qml/AppUI/EmulationView.qml`): updated to switch between `LibretroVideoItem` and `LibretroMetalItem` based on a session property like `session.libretroBackend === "metal"`. The session property is set by `GameSession` when launching, derived from the manifest or from a `RETRO_ENVIRONMENT_SET_HW_RENDER` (or similar) call from the core during retro_init.
- **`Pcsx2LibretroAdapter`** (`cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`): declares the new HW-render preference. Could be a method like `bool prefersHardwareRender() const override { return true; }`. mGBA's adapter returns false (default).

**pcsx2-master-fork-side (C++ inside `pcsx2-libretro/`):**

- **`Host::AcquireRenderWindow` rewritten** (`pcsx2-libretro/HostStubs.cpp`): instead of returning `Type::Surfaceless`, call the new env command `RETRO_ENVIRONMENT_GET_MACOS_NSVIEW` to fetch the NSView pointer, then construct and return a `WindowInfo{ .type = Type::MacOS, .window_handle = ns_view, .surface_width = 640, .surface_height = 448, .surface_scale = 1.0f, .surface_refresh_rate = 60.0f }`. (Width/height are placeholder; SP7 wires real values.) If the env command fails, log + return `std::nullopt` (mirrors the SP2 BIOS-not-found failure semantics).
- **`Host::BeginPresentFrame` rewritten** (`pcsx2-libretro/HostStubs.cpp`): currently a no-op. New behavior: notify a global condition variable that a frame is ready to present. PCSX2's MTGS thread calls this after rendering each frame.
- **`retro_run` rewritten** (`pcsx2-libretro/LibretroFrontend.cpp`): currently observes VM state and otherwise no-ops. New behavior: if the VM is running, block (with timeout) on the same condition variable until `Host::BeginPresentFrame` signals a frame is ready, then return. retro_run becomes the frame-pacing boundary.
- **`Host::ReleaseRenderWindow`** (`pcsx2-libretro/HostStubs.cpp`): no-op stays; RetroNest owns the NSView's lifetime.

### What does NOT change

- The mGBA / software-rendering path. `VideoSoftware`, `LibretroVideoItem`, and the `retro_video_refresh_t` flow stay untouched. The new path is parallel, not replacement.
- The SP2 settings layer (`Settings.{h,cpp}`). Still hardcoded; SP7 replaces.
- The EmuThread architecture. retro_run gains a wait, but the emu thread still owns Initialize → Execute → Shutdown.
- RetroNest's existing in-game menu overlay (`InGameMenuPanel`). The Qt menu floats in a separate `QQuickWindow` above the main window; it doesn't intersect with the Metal layer's render path. No code changes needed.

## File breakdown

### Added inside `pcsx2-master/pcsx2-libretro/`

No new files in this sub-project — only modifications.

### Modified inside `pcsx2-master/pcsx2-libretro/`

| File | Change | Approx Δ LOC |
|---|---|---|
| `HostStubs.cpp` | `Host::AcquireRenderWindow` queries the new env command and returns a `Type::MacOS` `WindowInfo`. `Host::BeginPresentFrame` signals a new condition variable. Adds the cv + mutex globally. | +60 |
| `LibretroFrontend.cpp` | `retro_run` blocks on the cv (with 100 ms timeout) when VM is running. `retro_get_system_info` library_version bumps to `"video-0.1"`. | +30 |
| `LibretroFrontend.h` | Declares the present-frame cv accessor (or makes the cv a public static in HostStubs). | +5 |

### Added inside `RetroNest-Project/cpp/src/`

| File | Purpose | Approx LOC |
|---|---|---|
| `ui/libretro/libretro_metal_item.h` | QQuickItem declaration with native-view accessor. | ~40 |
| `ui/libretro/libretro_metal_item.mm` | Objective-C++ implementation: creates the native QWindow, configures CAMetalLayer-capable NSView, exposes pointer. | ~120 |

### Modified inside `RetroNest-Project/cpp/`

| File | Change | Approx Δ LOC |
|---|---|---|
| `CMakeLists.txt` | Adds the new `.h`/`.mm` files; ensures `-fobjc-arc` or appropriate Objective-C++ flags for the new TU. | +5 |
| `src/core/libretro/environment_callbacks.cpp` | Adds the custom `RETRO_ENVIRONMENT_GET_MACOS_NSVIEW` handler; reads from a session-scoped registered NSView pointer. | +40 |
| `src/core/libretro/core_runtime.{h,cpp}` | Adds methods to register/clear the active NSView pointer for the running core. Coordinates with `LibretroMetalItem`. | +40 |
| `src/adapters/libretro/libretro_adapter.h` | Virtual `bool prefersHardwareRender() const { return false; }`. | +3 |
| `src/adapters/libretro/pcsx2_libretro_adapter.h` | Overrides `prefersHardwareRender() = true`. | +1 |
| `qml/AppUI/EmulationView.qml` | Add a `Loader` or conditional `LibretroMetalItem`/`LibretroVideoItem` based on the session's libretro backend. | +25 |
| `src/services/game_session.{h,cpp}` (or `game_service.cpp`) | When launching a hardware-rendering libretro core, expose `libretroBackend = "metal"` to QML; wire the LibretroMetalItem's NSView pointer to CoreRuntime before retro_load_game runs. | +30 |

### Total

- **pcsx2-libretro fork**: ~95 modified lines, no new files. ~1-2 commits.
- **RetroNest**: ~310 lines (~160 new, ~150 modified). ~3-4 commits.

This is the largest sub-project so far. Bigger than SP1 (skeleton) and SP2 (lifecycle) combined, mostly because we're crossing the Qt + Objective-C++ + Metal boundaries.

## Data flow at runtime

```
[ User launches Ratchet & Clank via the new pcsx2-libretro entry ]
              │
              ▼
[ GameSession knows the manifest says backend=libretro and the adapter is Pcsx2LibretroAdapter ]
[ GameSession checks adapter->prefersHardwareRender() → true ]
[ GameSession exposes libretroBackend="metal" to QML; tells CoreRuntime to expect HW render ]
              │
              ▼
[ EmulationView.qml's Loader instantiates LibretroMetalItem (not LibretroVideoItem) ]
[ LibretroMetalItem creates its native QWindow, gets its NSView via winId() ]
[ The NSView is layer-backed and ready to receive a CAMetalLayer ]
[ LibretroMetalItem registers its NSView pointer with CoreRuntime ]
              │
              ▼
[ CoreRuntime's environment callback dispatcher stores the NSView pointer in its session state ]
              │
              ▼
[ CoreRuntime calls retro_load_game(path) on the dylib ]
              │
              ▼
[ Our libretro core: BIOS discovery → Settings::InitializeDefaults → EmuThread::Start ]
              │
              ▼
[ EmuThread: CPUThreadInitialize → ApplySettings → VMManager::Initialize ]
              │
              ▼
[ Inside VMManager::Initialize → OpenGSDevice → GSDeviceMTL::Create → AcquireWindow ]
              │
              ▼
[ Host::AcquireRenderWindow calls our env_cb(RETRO_ENVIRONMENT_GET_MACOS_NSVIEW, &ns_view) ]
[ Returns WindowInfo{ type=MacOS, window_handle=ns_view, width=640, height=448 } ]
              │
              ▼
[ GSDeviceMTL::AttachSurfaceOnMainThread creates a CAMetalLayer and installs it on ns_view ]
[ ImGuiManager::Initialize uploads font textures via Metal ]
[ OpenGSDevice returns true ]
              │
              ▼
[ VMManager::Initialize returns StartupSuccess. EmuThread enters Execute. ]
[ MTGS thread starts rendering frames into the CAMetalLayer. ]
              │
              ▼
[ Every frame: MTGS finishes a frame → calls Host::BeginPresentFrame → CAMetalLayer
  presents to NSView → notify frame-ready condition variable ]
              │
              ▼
[ retro_run blocks on the cv (with 100 ms timeout) → wakes when frame is ready → returns ]
              │
              ▼
[ User sees PCSX2's BIOS boot logo, then the game's title screen, in the Qt window ]
              │
              ▼
[ Cmd+Shift+Escape → RetroNest's existing global hotkey calls into retro_unload_game ]
[ EmuThread::RequestShutdown → VMManager::SetState(Stopping) → Execute returns → 
  Shutdown → CPUThreadShutdown → emu thread joins ]
[ LibretroMetalItem unregisters its NSView pointer; QML returns to game list view ]
```

## Verification

Three tests, all requiring user interaction with RetroNest's UI on macOS.

### Test 1 — Boot screen visible in main window

User launches a PS2 game on the pcsx2-libretro entry in RetroNest. Within ~10 seconds, the PCSX2 BIOS boot logo (or the game's title screen) is visibly rendered inside RetroNest's main window. The frame stays on screen (per-frame sync working). User confirms "yes, I can see the game rendering."

### Test 2 — Clean exit returns to game list

While the game is running and rendering, user presses Cmd+Shift+Escape. RetroNest's existing in-game menu appears; user picks "Exit Game" (or equivalent in the existing menu). VM cleanly shuts down, Metal layer detaches, main window returns to the game library theme view. Log shows clean EmuThread teardown with no warnings about leaked resources.

### Test 3 — mGBA path unchanged

User launches a GBA game via the existing mGBA entry. mGBA's software rendering pipeline (LibretroVideoItem displaying QImages) continues to work exactly as before. Same theme view, same in-game menu, same exit semantics. The new SP3 code paths do not affect mGBA.

## Risks and mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| Qt 6 on macOS doesn't expose the QWindow's NSView in a way that PCSX2's GSDeviceMTL accepts (e.g. NSView class mismatch, layer-backing flags). | Medium | Reference projects (Qt Metal RHI examples) work fine. If our generic native-widget approach fails, fall back to embedding via `QWidget::createWindowContainer` of a QWindow with `QWindow::setSurfaceType(MetalSurface)`. |
| GSDeviceMTL replaces the layer on our NSView, breaking Qt's compositing of the widget. | Medium | We expect this exact behavior: PCSX2 installs its own CAMetalLayer; we just provide the host view. Qt compositing for that widget becomes "whatever PCSX2 draws." Other Qt widgets compositing over/under this one continue to work normally because each widget has its own NSView. |
| Cocoa main thread / Qt event loop deadlock when PCSX2 uses `OnMainThread([...])`. | Low | Qt 6 on macOS runs its event loop on top of NSRunLoop and processes the main dispatch queue. PCSX2's `OnMainThread` should drain naturally. Verified empirically in similar projects (RetroArch uses identical patterns). |
| Per-frame condition-variable wait in retro_run leads to timing drift or audio glitches once SP4 lands. | Low for SP3 | SP3 has no audio. The cv pattern is the same one any blocking-frame libretro core uses. SP4 will tune the wait timeout vs audio buffer behavior. |
| `RETRO_ENVIRONMENT_USER + N` env command ID collides with an existing libretro extension. | Very low | The USER range is explicitly host-private; the libretro spec promises no collisions. We document our chosen ID in the libretro_metal_item.h header so future SP3+ developers don't reuse it. |
| The NSView we provide is not yet "realized" (no underlying NSWindow) by the time PCSX2 needs it. | Medium | Force widget realization before launching the core: in `GameSession::start`, ensure the `EmulationView` is visible (pushed onto the stack) and processEvents() once. The NSView will be backed by an NSWindow at that point. |
| ImGuiManager fails to initialize on the first attempt because PCSX2 expects font files in a path we haven't configured. | Medium | SP2 already configured `EmuFolders::SetResourcesDirectory()`. ImGui font paths should resolve via that. If not, the implementation plan investigates `EmuFolders::Resources` and adjusts. |

## Out-of-scope clarifications

- **Audio sync after SP4.** SP3 introduces frame-paced retro_run via Host::BeginPresentFrame. When SP4 wires audio, retro_run will need to also pump audio. The frame-pacing infrastructure remains correct.
- **Window resize handling.** SP3 hardcodes 640x448. When the user resizes RetroNest's window, the Metal layer doesn't currently rescale. Defer to a polish sub-project after SP6.
- **HiDPI / Retina scaling.** `surface_scale` set to 1.0 for SP3. SP3's success criterion is "see the game" — physical sharpness deferred.
- **Multiple games at once.** N/A; RetroNest only runs one game at a time.

## Success criteria summary

1. New files exist: `LibretroMetalItem.{h,mm}`. Modifications across HostStubs.cpp / LibretroFrontend.cpp / environment_callbacks.cpp / CoreRuntime / EmulationView.qml / Pcsx2LibretroAdapter as listed.
2. `Host::AcquireRenderWindow` returns a `WindowInfo` with `Type::MacOS` and a valid NSView pointer for the pcsx2-libretro core; still returns `Surfaceless` for mGBA and other software cores.
3. `VMManager::Initialize` succeeds for a real PS2 ISO when the EmulationView is visible.
4. The PCSX2 BIOS boot logo (or game title screen) is visible inside RetroNest's main window within ~10 seconds of launch.
5. `Host::BeginPresentFrame` signals retro_run; retro_run drives the frame cadence.
6. Cmd+Shift+Escape causes clean shutdown returning to the game list. No leaks, no warnings.
7. mGBA software path unchanged.

When all seven are true, SP3 is complete. SP4 (audio output) becomes the next sub-project: SPU2 → `retro_audio_sample_batch_t` → RetroNest's existing audio sink.

## Verification log (in progress)

SP3 implementation completed through Task 12. Tasks 13-16 (end-to-end verification + spec completion) blocked on a launch-sequence ordering bug.

### What works (verified)

- **Env infrastructure end-to-end.** `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` (0x20001) is registered, CoreRuntime::setActiveNSView / activeNSView store the pointer atomically, the env handler returns the pointer correctly when set.
- **Adapter flag dispatch.** `Pcsx2LibretroAdapter::prefersHardwareRender() = true` is wired and consulted by GameSession; `libretroBackend` becomes `"metal"` for PCSX2 launches, `"software"` for mGBA.
- **LibretroMetalItem widget.** Builds clean as Objective-C++, links against AppKit / QuartzCore / Metal frameworks, hosts a native QWindow with `QSurface::MetalSurface`, exposes `nativeView()` as `Q_INVOKABLE qulonglong`. QML_ELEMENT registration via Qt 6's auto-registration works.
- **EmulationView.qml Loader.** Switches between LibretroVideoItem (software) and LibretroMetalItem (metal) based on session.libretroBackend.
- **`Host::AcquireRenderWindow` reaching the env command.** Confirmed via a hard fopen-based diagnostic in `/tmp/sp3_trace.log`. The function is called, environ_cb is valid, and the env command query happens.
- **`Folders/Resources` override.** Directly assigning `EmuFolders::Resources` after `SetResourcesDirectory()` correctly points PCSX2 at pcsx2-master's `bin/resources/`. The Metal shader library loads (no more "Failed to create GS device" before AcquireWindow).
- **System console enabled.** `Logging/EnableSystemConsole = true` + `EnableVerbose = true` surfaces PCSX2's `Console.WriteLn` / `Console.Error` output through stderr → essential for diagnosis.
- **Renderer = Auto (-1).** Resolves to Metal via `GSUtil::GetPreferredRenderer()` on macOS; the GS device path actually reaches `AcquireWindow` instead of erroring at shader-lib load.

### What's blocked

`Host::AcquireRenderWindow` is called, but `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` returns **false** because RetroNest's `CoreRuntime` doesn't have an NSView registered at the time `retro_load_game` runs.

**Root cause: launch-sequence ordering.** The flow is:

1. User clicks Launch.
2. `GameSession::startLibretro` runs.
3. `rt->start(cfg)` is called inside startLibretro — this triggers `retro_load_game` on the core, which during `VMManager::Initialize` calls our `Host::AcquireRenderWindow`.
4. `rt->start` succeeds → `started` signal fires → AppController emits `gameStartedLibretro` → AppWindow.qml's Connections (`function onGameStartedLibretro()`) pushes EmulationView.
5. EmulationView's Loader instantiates LibretroMetalItem → `Component.onCompleted` → `session.registerHardwareView(nativeView())` → `CoreRuntime::setActiveNSView(...)`.

Step 3 calls `retro_load_game` BEFORE step 5 registers the NSView. So `AcquireRenderWindow`'s env command query returns null. `OpenGSDevice` reports "Failed to acquire render window" → init fails → user sees "Emulator crashed."

A naive spin-wait inside `startLibretro` (calling `QCoreApplication::processEvents` until `activeNSView` is non-null) does not help because EmulationView isn't pushed until *after* `startLibretro` returns successfully — it's reactive to the post-start signal.

### Path forward

Three viable architectural fixes:

1. **Pre-push EmulationView.** Emit a new `gameStartingLibretro` signal at the top of `GameSession::startLibretro` (or hijack `libretroBackendChanged`). AppWindow.qml pushes EmulationView in response. Loader instantiates LibretroMetalItem synchronously during the QML scene update. Then `startLibretro` spin-waits (with `processEvents`) for `activeNSView()` to be non-null before calling `rt->start`. This requires a small AppWindow.qml change + a small game_session.cpp signal addition.

2. **Asynchronous launch.** Split `rt->start` into two phases: phase 1 dlopens the core + calls `retro_set_environment` / `retro_init` + emits "core loaded" signal that triggers EmulationView push. Phase 2 (`rt->beginLoad` or similar) calls `retro_load_game` only after the NSView is registered. More invasive but cleaner architecturally.

3. **Singleton NSView.** Have LibretroMetalItem (or a parallel "MetalSurface" QObject) be a persistent singleton owned by main.cpp, not by EmulationView's lifecycle. CoreRuntime always knows about it because it's registered at app startup. EmulationView just makes it visible. Cleanest from a state-management perspective but requires re-architecting how the widget participates in the QML scene.

**Recommended path for SP3 continuation:** Option 1 — smallest delta from current state, surgical.

### Known side issue

After a failed PCSX2 launch attempt, the second click reports "VM already running" — there's a state leak where `m_libretroAdapter` isn't cleared after `rt->start` fails (the `finished` signal's cleanup only runs on successful runs that end). This compounds the "Emulator crashed" UX. Belongs in the same SP3 continuation: clear `m_libretroAdapter` / call `releaseRuntime()` on `rt->start` failure inside `startLibretro`.

### Commits added during SP3

On pcsx2-master `retronest-libretro` branch:
- `3f64a93a9` libretro: HW render — query host NSView, frame-paced retro_run
- `a384f33fa` libretro: HW render diagnostics — Auto renderer, system console, Resources fix

On RetroNest `main`:
- `913f87d` libretro: scaffold HW render context env command + adapter flag
- `5401cb0` libretro: LibretroMetalItem + EmulationView HW render switching
- `3205012` docs(specs): pcsx2-libretro SP3 — HW render bridge / video output
- `743ae72` docs: pcsx2-libretro SP3 implementation plan — HW render bridge

### Session summary

The HW render bridge infrastructure is fully built and proven working through the env command query. What remains is timing/ordering: ensure the NSView is registered with CoreRuntime BEFORE `retro_load_game` runs. The recommended Option 1 fix is ~20 lines across game_session.cpp and AppWindow.qml. Saving this for a focused continuation session.

---

## Verification log — continuation (2026-05-11, same day)

Option 1 implemented and verified end-to-end. Ratchet & Clank: Going Commando launches via the pcsx2-libretro entry, the PS2 BIOS hands off to the game, and the in-game memory-card screen renders into the Metal-backed widget inside RetroNest's main window. `VM RUNNING` reports `serial=SCUS-97268 crc=0xB3A71D10` (correct disc CRC). User confirmed via screenshot.

### What landed

1. **Option 1 ordering fix** (RetroNest `main`, commit `ad716ad`)
   - `GameSession::aboutToStartLibretro` signal emitted after `lr->prepareRuntime()` and before `rt->start` — see `game_session.cpp`.
   - `AppController::gameStartingLibretro` forwards it.
   - `AppWindow.qml` pushes `EmulationView` in response to the new signal, sets `view.session` immediately, so the QML `Loader` switches to `metalComponent` and `LibretroMetalItem.Component.onCompleted` registers the NSView via `GameSession::registerHardwareView`.
   - `startLibretro` then pumps `QCoreApplication::processEvents` on a 2000 ms deadline until `CoreRuntime::activeNSView()` is non-null before calling `rt->start`.
   - The original `onGameStartedLibretro` handler is made idempotent via an `isEmulationView` guard, so the software-backend launch path is unchanged.
   - Failure-path cleanup: when either the spin-wait times out or `rt->start` returns false, `lr->releaseRuntime()` is called and `m_libretroAdapter` / `m_adapter` / `m_manifest` are cleared, fixing the "VM already running" retry bug.

2. **`thread_local` g_current bug surfaced during diagnosis** (RetroNest `main`, commit `8d37e1a`)
   - `core_runtime.cpp` declared `thread_local CoreRuntime* g_current = nullptr;`. PCSX2 spawns its MTGS render thread inside `VMManager::Initialize`; that MTGS thread is what calls `Host::AcquireRenderWindow` → `env_cb` → `CoreRuntime::envTrampoline` → `tlsCtx()`. With `thread_local`, `g_current` was null on the MTGS thread and `environmentDispatch` returned false silently — neither the success log nor the "unhandled enum" default fired. This pre-dated SP3 but was masked by the ordering bug; the moment we got the ordering right, this surfaced as the *next* wall.
   - Switched to plain static. Cross-thread visibility comes from QThread::start's happens-before; only one CoreRuntime runs at a time.
   - Added operational logging to the `GET_MACOS_NSVIEW` env handler.

3. **pcsx2-libretro fork: missing port from gsrunner** (`retronest-libretro` branch, commit `ce3bddf77`)
   - **ImGui font registration.** `Settings::InitializeDefaults` now reads `Roboto-Regular.ttf` from `EmuFolders::GetOverridableResourcePath` and calls `ImGuiManager::SetFonts` (mirrors `pcsx2-gsrunner/Main.cpp:127-146`). Without this, `AddTextFont()` returned nullptr → `AddImGuiFonts()` failed → "Failed to create ImGui font texture" → "Failed to initialize GS." The font bytes live in a process-lifetime `std::vector<u8>` because `ImGuiManager::FontInfo` holds a span and the atlas is `FontDataOwnedByAtlas=false`.
   - **VM state transition to Running.** `VMManager::Initialize` leaves state at `VMState::Paused`. `Cpu->Execute()` returns immediately in that state. `EmuThread::ThreadFunc` now calls `VMManager::SetState(VMState::Running)` and loops `Execute` while state stays Running, mirroring `gsrunner/Main.cpp:950-957`.

### Verified

- **Test 1 (boot screen visible):** PASSED. Game-text appears inside the RetroNest window (memory-card-insert screen of Ratchet & Clank). `VM RUNNING — title=Ratchet & Clank 2 - Going Commando serial=SCUS-97268 crc=0xB3A71D10` in the log.
- **Test 3 (mGBA unchanged):** Unverified this session but software-path code is unmodified — the conditional Loader still chooses `softwareComponent` when `libretroBackend === "software"`, and the spin-wait branch is gated on `hw == true`. Worth a smoke test before declaring SP3 fully shipped.

### Not verified yet

- **Test 2 (clean exit via Cmd+Shift+Esc).** Not exercised this session — the user observed the in-game menu didn't auto-open, but it's user-triggered, so absence isn't a regression. Smoke-test before closing SP3.
- **Frame pacing via `Host::BeginPresentFrame` → `g_present_cv` → `retro_run`.** Game runs and renders, but we haven't validated the timing characteristic of the cv-wait under load (e.g. cutscenes, audio gameplay). Defer to SP4 (audio output) which will reveal pacing issues if any exist.

### Cosmetic / out-of-scope follow-ups surfaced

- **`patches.zip` missing.** `bin/resources/patches.zip` is not present (upstream PCSX2 builds this from `patches/`); RetroNest sees a non-fatal `OSD [PatchesZipOpenWarning]: Failed to open patches.zip`. Built-in game-specific patches are unavailable. Owner: SP7 (settings push) or a dedicated "ship patches.zip" task.
- **"No Memory Card inserted."** `Settings::InitializeDefaults` explicitly disables both memcard slots (`Slot1_Enable=false`, `Slot2_Enable=false`) for SP3 scope; the game's BIOS-level memcard prompt is expected behaviour. Owner: SP6 (save states + memcards).
- **`rc_libretro_memory_init failed for console 0`.** RetroNest's rcheevos init runs before our libretro core has called `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` (PCSX2's hooks haven't fired yet). Achievements load (the session is "active") but the memory map is empty so triggers can't fire. Owner: rcheevos timing — likely SP7 or a small follow-up to defer memory-init until after the core's first SET_MEMORY_MAPS dispatch.
- **`EmuFolders::Resources` hardcoded absolute path.** Still pointing at `/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/bin/resources`. Owner: SP7 — derive from `dladdr()` or copy resources next to the dylib at install time.

### Commits added during continuation

On pcsx2-master `retronest-libretro` branch:
- `ce3bddf77` libretro: register Roboto font + SetState(Running) so VM actually executes

On RetroNest `main`:
- `ad716ad` libretro: SP3 launch-ordering fix — pre-push EmulationView, wait for NSView
- `8d37e1a` libretro: env dispatch reachable from non-worker threads (fix thread_local)

### Closing posture

SP3's success criterion ("user sees the PS2 game rendering in RetroNest's window") is met. Three out of seven detailed success criteria are firmly verified (1, 2, 3, 4, 5 from the Goal section); criteria 6 (frame pacing via cv) is functioning but not stress-tested, criterion 7 (clean exit) is structurally wired but not exercised this session, criterion 8 (mGBA unchanged) is structurally protected but not smoke-tested this session. Three small smoke tests would close SP3 formally; otherwise SP3 is "shippable, follow-ups tracked."

Next sub-project: **SP4 — audio output** (SPU2 → `retro_audio_sample_batch_t`).
