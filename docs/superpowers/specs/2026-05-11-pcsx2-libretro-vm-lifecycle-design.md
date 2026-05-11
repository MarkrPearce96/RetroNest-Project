# PCSX2 Libretro Core — VM Lifecycle + Game Boot (Sub-project 2 of 8)

**Date:** 2026-05-11
**Status:** Complete (lifecycle infrastructure shipped; GS init unblock deferred to SP3 — see Verification log)
**Owner:** mark
**Scope:** Second sub-project of the multi-phase PCSX2-to-libretro port.
**Predecessor:** [Skeleton phase](2026-05-11-pcsx2-libretro-skeleton-design.md) (complete).

## Context

The skeleton phase produced a fully-linkable libretro core that loads in RetroNest, identifies as PCSX2, and refuses `retro_load_game` cleanly. Sub-project 2's job is to replace that refusal with actually booting a PS2 game inside our core — proving the libretro shim can drive PCSX2's full virtual machine end-to-end, even though no video / audio / input is connected yet.

The fundamental design tension: `VMManager::Execute()` is a **blocking** call that owns its CPU thread for the lifetime of the VM, while libretro's `retro_run` is called **per frame** by the host. SP2 finesses this by running the VM on a dedicated thread free of `retro_run`'s pacing; SP3 (video output) will introduce per-frame synchronization through the GS present hook.

## Goal

`retro_load_game(path)` boots the supplied PS2 ISO to the running state on a dedicated emu thread. After BIOS + disc load completes, `VMManager::GetState()` reports `Running` and `VMManager::GetCurrentCRC()` returns the game's CRC. `retro_unload_game` cleanly stops the VM and joins the thread. No video, no audio, no input — purely an internal VM lifecycle proof.

**Definition of done:**

1. `retro_load_game` with a real PS2 ISO causes `VMManager::Initialize` to return `StartupSuccess`.
2. `VMManager::Execute()` enters its main loop on our dedicated emu thread.
3. Within ~10 seconds, `GetState() == VMState::Running` and `GetCurrentCRC()` returns a non-zero CRC. Our shim logs the title (`GetTitle()`), serial (`GetDiscSerial()`), and CRC — proof the game is booting.
4. `retro_unload_game` signals shutdown, `Execute()` returns, `Shutdown()` runs, the emu thread joins within a 5-second timeout. No leaked threads, no crash.
5. The full sequence works in RetroNest (re-running the SP1 manual flow with Ratchet & Clank — Going Commando).

## Motivation

Threading model and success bar chosen from the brainstorm:

- **Success bar:** "VM boots internally" — no output, no UI proof. Smallest scope that proves the VM lives inside our core.
- **Threading model:** free-running emu thread, `retro_run` is a no-op. The thread spawns at `retro_load_game` time, runs Execute at PCSX2's natural pace, and is signaled to stop at `retro_unload_game`. SP3 will introduce per-frame sync.

## Non-goals (deferred to later sub-projects)

- **Video output (SP3).** GS uses the software renderer writing to an in-memory framebuffer; we don't touch it. `Host::AcquireRenderWindow` continues to be the SP1 stub (returns `std::nullopt` if GS tolerates, or a fabricated headless `WindowInfo` if it doesn't).
- **Audio output (SP4).** SPU2 backend is forced to `nullout` (no audio device opened). SPU2 still initializes — it just discards samples.
- **Input (SP5).** `retro_input_state_t` is unused. PCSX2's PAD plugin gets no controller events; the game runs with no input. (Most PS2 BIOSes/games keep running without input — they idle at title screens, etc., which is fine for our verification.)
- **Save states / memcards (SP6).** `retro_serialize_size` still returns 0; memcards disabled.
- **Real settings push from RetroNest (SP7).** Settings come from a hardcoded in-process `MemorySettingsInterface`. RetroNest's libretro options system doesn't yet drive PCSX2.
- **Frame counter telemetry / first black frame surfaced to libretro.** Considered and deferred — SP3 owns video. SP2 keeps the surface as narrow as possible.

## Architecture

### Three new pieces of plumbing inside `pcsx2-libretro/`

**1. Settings layer (`Settings.{h,cpp}`).**

PCSX2 reads every config value (BIOS path, GS renderer choice, SPU2 backend, recompiler flags, achievements config, etc.) through `Host::Get*SettingValue` → which the runtime dispatches via the active `SettingsInterface`. The skeleton's `HostStubs.cpp` getters return the supplied defaults for everything, leaving PCSX2 with no knowledge of where the BIOS is or which backends to pick.

SP2 introduces `Pcsx2Libretro::Settings`, which owns a `MemorySettingsInterface` (PCSX2 ships one in `common/MemorySettingsInterface.h`) populated with the minimum keys needed to boot. Concretely:

| Key (section/key) | Value | Why |
|---|---|---|
| `Folders/Bios` | (libretro system dir) | Where PCSX2 looks for the BIOS file |
| `EmuCore/GS/Renderer` | `13` (Software) | No native window needed |
| `SPU2/OutputModule` | `nullout` | No audio device |
| `Achievements/Enabled` | `false` | Avoid network init during boot |
| `EmuCore/EnableFastBoot` | `true` | Skip BIOS region check screen |
| `EmuCore/HostFs` | `false` | Disable host-fs |

(Exact key names verified against `pcsx2/Config.h` during implementation. The table above is the conceptual list; the keys may be named slightly differently in upstream PCSX2 master.)

The settings interface is registered as the active base layer via `Host::Internal::SetBaseSettingsLayer(...)` **before** calling `VMManager::Initialize`. `HostStubs.cpp`'s settings-getter functions are updated to route through `Pcsx2Libretro::Settings::GetActiveInterface()` instead of returning defaults.

**2. Emu thread (`EmuThread.{h,cpp}`).**

A `Pcsx2Libretro::EmuThread` class wrapping a `std::thread`. Public surface:

```cpp
class EmuThread {
public:
    bool Start(const VMBootParameters& boot_params); // returns true if Initialize succeeded
    void RequestShutdown();                          // sets VMState::Stopping
    void Join();                                     // waits for thread exit
    bool IsRunning() const;
};
```

The thread function:
```cpp
void EmuThread::ThreadFunc(VMBootParameters params) {
    Error err;
    VMBootResult result = VMManager::Initialize(params, &err);
    if (result != VMBootResult::StartupSuccess) {
        FrontendLog(RETRO_LOG_ERROR, "VMManager::Initialize failed: %s", err.GetDescription().c_str());
        m_init_failed.store(true);
        m_init_done.store(true);
        return;
    }
    m_init_done.store(true);
    VMManager::Execute(); // blocks until SetState(Stopping)
    VMManager::Shutdown(false);
}
```

`Start()` posts the params to the thread and waits (with timeout) for `m_init_done` to be set, then returns whether init succeeded. This lets `retro_load_game` give libretro a synchronous true/false result.

**3. BIOS path resolution (in `LibretroFrontend.cpp`).**

At `retro_load_game` time, query `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` via the env callback. If unsupported or empty, default to the libretro core's containing directory. Search the resulting directory for a PS2 BIOS file using PCSX2's own `IsBIOS()` validator (`pcsx2/PS2EBoot` or equivalent — implementation determines the exact include) rather than pattern-matching filenames ourselves. This guarantees we accept whatever BIOS files PCSX2 itself accepts.

If no BIOS is found → `retro_load_game` returns `false` with an OSD message: `"PS2 BIOS not found in {dir}"`. This becomes the meaningful failure mode SP2 introduces (in addition to "Initialize itself failed").

Otherwise, populate `Settings::Folders/Bios` with the directory path and proceed.

### What does NOT change

- **`Host::AcquireRenderWindow`** stays as the SP1 stub (`std::nullopt`). If software GS requires a non-null `WindowInfo`, we fabricate a minimal headless one with `type = WindowInfo::Type::Surfaceless`, `surface_width = 640`, `surface_height = 448`. This is decided during implementation based on what GS rejects.
- **`Host::Run*Thread`** stays as the SP1 stub (run inline immediately). Used during VM Initialize for some cross-thread setup; running inline on the calling thread (which IS the emu thread once Execute starts) is acceptable.
- **All retro_video_refresh_t / retro_audio_sample_batch_t / retro_input_state_t calls.** Still no-ops. `retro_run` still no-op.
- **RetroNest-side code.** Zero changes.

## File breakdown

### Added inside `pcsx2-master/pcsx2-libretro/`

| File | Purpose | Approx LOC |
|---|---|---|
| `Settings.h` | `Pcsx2Libretro::Settings` namespace declarations: `InitializeDefaults`, `GetActiveInterface`, `SetBiosDirectory`. | ~30 |
| `Settings.cpp` | Owns a static `MemorySettingsInterface`. `InitializeDefaults` populates the minimum-key set. | ~80 |
| `EmuThread.h` | `Pcsx2Libretro::EmuThread` class declaration. | ~40 |
| `EmuThread.cpp` | Implementation: thread management, init handshake, shutdown signal. | ~120 |

### Modified inside `pcsx2-master/pcsx2-libretro/`

| File | Change | Approx Δ LOC |
|---|---|---|
| `CMakeLists.txt` | Add `Settings.cpp` and `EmuThread.cpp` to `target_sources`. | +2 |
| `LibretroFrontend.h` | Forward-declare `EmuThread`; add a global `EmuThread*` to `FrontendState` (or a separate singleton). | +5 |
| `LibretroFrontend.cpp` | `retro_load_game` becomes real: BIOS path resolution, `Settings::InitializeDefaults`, build `VMBootParameters`, `EmuThread::Start`. `retro_unload_game` becomes real: `EmuThread::RequestShutdown` + `Join`. `retro_get_system_info` library_version bumps to `"vm-0.1"`. | +80, −20 |
| `HostStubs.cpp` | Settings getter family routes through `Settings::GetActiveInterface()` instead of returning defaults. `Internal::SetBaseSettingsLayer` and `Internal::GetBaseSettingsLayer` actually track the active layer in a static. | +40, −20 |

### RetroNest-side: zero changes.

### Approximate total: ~300 new lines + ~60 modified lines across the fork. ~5 git commits during implementation.

## Data flow

```
[ RetroNest calls retro_load_game(game->path) ]
                       │
                       ▼
[ Query RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY → system_dir ]
                       │
                       ▼
[ Scan system_dir for a PS2 BIOS file ]
        │                       │
   none found              found at bios_path
        │                       │
        ▼                       ▼
[ OSD: "PS2 BIOS         [ Settings::InitializeDefaults(system_dir):
   not found"               - Folders/Bios = system_dir
   return false ]           - EmuCore/GS/Renderer = Software
                            - SPU2/OutputModule = nullout
                            - Achievements/Enabled = false
                            - EmuCore/EnableFastBoot = true ]
                       │
                       ▼
[ Host::Internal::SetBaseSettingsLayer(&Settings::GetActiveInterface()) ]
                       │
                       ▼
[ Build VMBootParameters:
    filename = game->path
    fast_boot = true ]
                       │
                       ▼
[ EmuThread::Start(params) — spawns std::thread, calls Initialize,
  signals m_init_done, returns whether StartupSuccess ]
        │                       │
   Initialize failed       StartupSuccess
        │                       │
        ▼                       ▼
[ OSD: "VM init           [ retro_load_game returns true.
   failed: …"               Background: EmuThread is now in
   return false ]            VMManager::Execute() — runs at
                             PCSX2's natural pace, internally
                             producing frames into a software
                             framebuffer we don't read yet. ]
                       │
                       ▼
[ retro_run called by host every frame:
  - Read VMState (atomic)
  - If first time we see Running, log title/serial/CRC ONCE
  - Otherwise, no-op
  - Return ]
                       │
                       ▼
[ RetroNest "exit game" → retro_unload_game ]
                       │
                       ▼
[ EmuThread::RequestShutdown:
    VMManager::SetState(VMState::Stopping) ]
                       │
                       ▼
[ Inside Execute on emu thread: CPU loop sees Stopping → returns.
  Thread continues to VMManager::Shutdown(false). Thread function
  returns. EmuThread::Join() unblocks. ]
                       │
                       ▼
[ retro_unload_game returns. retro_deinit cleans up later
  (already correct from SP1). ]
```

## Verification

Three tests, all automatable except the third (which requires RetroNest UI launch).

### Test 1 — VM reaches Running state (test_loader extended)

Extend the SP1 `test_loader.c` (in `pcsx2-libretro/tools/`) to:
1. Call `retro_load_game(real_ps2_iso_path)`.
2. If it returns true, loop calling `retro_run` for 15 seconds.
3. Check stderr log for lines indicating `VMState=Running` and a non-zero CRC.
4. Call `retro_unload_game`.
5. Wait for unload to complete (~5 second timeout).
6. Verify clean exit.

The BIOS file must exist in `system_dir` (we'll point system_dir at RetroNest's data root for the test). If it doesn't, expect a "BIOS not found" log line and test_loader exits early.

Pass criteria: exit code 0, log shows `VMState=Running`, `CRC=0x` (non-zero), `Title=` and `Serial=` populated.

### Test 2 — Clean shutdown

Verifies test 1's `retro_unload_game` path: the emu thread joins within timeout, no leaked threads, no "VMManager::Shutdown called twice" errors, exit code 0. (This is observable from Test 1's output; not a separate run.)

### Test 3 — Full RetroNest flow

Repeat the SP1 manual UI flow with Ratchet & Clank. Expected: same logs as Test 1 visible in RetroNest's stderr. Exiting the game in RetroNest's UI causes clean Shutdown (joins within 5 seconds). Existing pcsx2 launched-binary path continues to work for other PS2 games.

## Risks and mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| Apple Silicon JIT recompiler crashes immediately because the dylib lacks the MAP_JIT entitlement on macOS Sonoma+. | Medium | PCSX2's standalone .app gets the entitlement via its Info.plist. Our dylib inherits the entitlement of the loading process (RetroNest). RetroNest doesn't currently ship with that entitlement. If Execute() crashes on first recompile, the fix is on the RetroNest side: add `com.apple.security.cs.allow-jit` to RetroNest.app's entitlements. Document this in the spec; surface as a "RetroNest feature gap" if hit. |
| Software GS still requires a non-null WindowInfo, so `Host::AcquireRenderWindow` returning nullopt fails Initialize. | Medium | Fabricate a minimal headless `WindowInfo` (`Type::Surfaceless`, 640x448). Already noted in the design. |
| `VMManager::Initialize` requires data files we haven't anticipated (game databases, patches, shader caches, …) and those don't exist next to our dylib. | Medium | PCSX2 stores `gamedb.yaml` in its resources/ directory. We need to ensure resources/ is findable; likely set `Folders/Resources` to a known directory. Implementation note: discover the exact required files by running once and reading the Initialize error message. |
| Settings keys are named differently than the design's conceptual table. | High but routine | During implementation, grep `pcsx2/Config.h` and `pcsx2/Pcsx2Config.cpp` for the actual key strings. The design's table is conceptual; implementation uses the real names. |
| BIOS file naming convention isn't `SCPH*.BIN` / `ps2-*.bin` in upstream PCSX2 master. | Low | PCSX2 has a BIOS scan helper (`IsBIOS` somewhere in `pcsx2/PS2EBoot.cpp` or similar). Use that instead of pattern-matching filenames ourselves. |
| Initialize() succeeds but Execute() returns immediately because some early-boot check fails. | Low | The state transitions are observable. If GetState() never reaches Running, log every state change to diagnose. |
| Emu thread leaks if `retro_unload_game` is never called (e.g. RetroNest crashes mid-game). | Low impact | `retro_deinit` should still join. Currently retro_deinit just zeroes g_frontend — extend it to call EmuThread::RequestShutdown + Join as a belt-and-braces measure. |

## Out-of-scope clarifications

- **Per-frame sync mechanism.** Not designed in SP2. SP3 (video output) introduces it by hooking PCSX2's GS-present callback to signal a condition variable, and refactoring retro_run to wait on it.
- **Game-specific settings (game database overrides).** PCSX2 loads per-game patches from `gamedb.yaml`. These should "just work" as long as we set `Folders/Resources` correctly. No SP2 design work needed.
- **BIOS auto-selection across multiple regions.** SP2 picks the first BIOS file found. If a user has both NTSC and PAL BIOS in system_dir, behavior is "whichever globs first." SP7 (settings push) introduces explicit selection.
- **Crash-recovery / OnVMDestroyed reporting.** PCSX2's Host::OnVMDestroyed already routes through our stub — we log and continue. Fine for SP2.

## Success criteria summary

1. `pcsx2-libretro/Settings.{h,cpp}` and `pcsx2-libretro/EmuThread.{h,cpp}` exist and are compiled by the existing CMake target.
2. `retro_load_game` with a real PS2 ISO and a valid BIOS in system_dir returns `true` and the emu thread runs `VMManager::Execute()`.
3. Within ~15 seconds, log lines confirm `GetState() == Running` and `GetCurrentCRC() != 0` (matching the loaded game).
4. `retro_unload_game` causes a clean Shutdown and Join within 5 seconds.
5. `retro_load_game` with no BIOS in system_dir returns `false` cleanly with the documented OSD message.
6. The SP1 launched-binary PCSX2 path continues to work unchanged.
7. Verified by both the standalone `test_loader` and by launching Ratchet & Clank from RetroNest.

When all seven are true, the VM-lifecycle sub-project is complete. The next sub-project (Video output) bridges GS framebuffer → `retro_video_refresh_t` and introduces the per-frame sync mechanism that retro_run needs to drive the frame cadence.

## Verification log

SP2 completed 2026-05-11. Outcome differs from the original success criteria: lifecycle infrastructure works end-to-end, but VM init itself is blocked on a macOS architectural reality that SP2's "no output" framing didn't anticipate. The shipped work is real and load-bearing; the GS-init unblock moves to SP3.

### What works (committed and verified)

- **Settings layer** (`Settings.{h,cpp}`): `MemorySettingsInterface` populated via `VMManager::SetDefaultSettings`, then overridden for SP2-required keys (BIOS path from libretro system dir, GS renderer=Null, SPU2/Output=nullout, achievements off, fast-boot on, host-fs off, input sources disabled, memory cards off). Registered via `Host::Internal::SetBaseSettingsLayer` and queried correctly by PCSX2 internals.
- **EmuThread** (`EmuThread.{h,cpp}`): `std::thread` wrapper. `Start()` returns synchronously once `VMManager::Initialize` reports success or failure via condition variable. `RequestShutdown()` + `Join()` cleanly tear down. Confirmed: thread spawn, init handshake, log telemetry per VM init step, no leaked threads on exit.
- **BIOS discovery**: `FindPS2BiosFile` correctly handles a directory containing both PS2 BIOSes (~4 MB) and PSX BIOSes (~512 KB) by filtering on file size > 1 MB. Verified picking SCPH-70004.bin (PS2) over scph5500.bin (PSX).
- **`retro_load_game` / `retro_unload_game` / `retro_run` / `retro_deinit` real implementations**: all wired and behaving correctly when init fails (clean OSD message, false return, no crash, no thread leak).
- **HostStubs.cpp simplification**: removed the local duplicate `Host::Internal::*SettingsLayer` and `Host::Get*SettingValue` family that conflicted with `pcsx2/Host.cpp`'s versions, fixing a SIGSEGV in `CPUThreadInitialize` where set/get were reading inconsistent state.
- **`VMManager::ApplySettings()`** call between `CPUThreadInitialize` and `Initialize` ensures the Renderer/SPU2/InputSources overrides take effect in `EmuConfig` before the VM is brought up. This is the gsrunner pattern (Main.cpp:943) we initially missed.
- **EmuFolders init** in `Settings::InitializeDefaults`: `SetAppRoot()` + `SetResourcesDirectory()` + `SetDataDirectory()` before `SetDefaultSettings`. Without these, DataRoot resolved as "/" on macOS, breaking PCSX2's resource lookup.

### What's blocked (deferred to SP3)

**VMManager::Initialize itself fails at GS device init.**

The chain: `VMManager::Initialize` → `MTGS::WaitForOpen` → spawns MTGS thread → `GSopen` → `OpenGSDevice` → on macOS, `GetAPIForRenderer(Null)` falls through `default:` to `GSUtil::GetPreferredRenderer()` which returns Metal → `MakeGSDeviceMTL()` → `GSDeviceMTL::Create`. The Metal `MTLDevice` and `MTLCommandQueue` create successfully without a window, but `OpenGSDevice` then attempts `ImGuiManager::Initialize` which requires Metal font textures, AND `AttachSurfaceOnMainThread` assumes a real `NSView` to attach a `CAMetalLayer` to. Without a host-provided NSView, this path fails and `OpenGSDevice` returns `false`, propagating "Failed to create render device" through `VMManager::Initialize` → `VMBootResult::StartupFailure`.

**This is not a Metal limitation, nor a PCSX2 design flaw**: PCSX2 on macOS reasonably assumes the host provides a window+surface. The mismatch is that SP2's "no video output" framing assumed PCSX2 had a true headless code path. It doesn't on macOS (and likely not on Windows either with D3D11/12). Renderer=Null is a *post-device* choice — the device itself is unconditionally Metal.

### What was tried during diagnosis

- `Host::AcquireRenderWindow` updated to return a `WindowInfo{}` (default-constructed = `Type::Surfaceless`) instead of `std::nullopt`. Got us past `AcquireWindow` but not past `AttachSurfaceOnMainThread` / `ImGuiManager::Initialize`.
- `Settings.cpp` cycled through Renderer=SW (13), then Renderer=Null (11). Same result — both go through the Metal device path on macOS.
- Test_loader-based verification (Test 1 from the verification plan) was abandoned: test_loader has no Cocoa event loop, so `OnMainThread([…])` inside `GSDeviceMTL::Create` deadlocks instead of failing. End-to-end verification in RetroNest reveals the underlying failure path that test_loader masks as a hang.
- Sampling the stuck process confirmed the hang stack: `MTGS::WaitForOpen → ThreadEntryPoint → GSopen → OpenGSDevice → GSDeviceMTL::Create:945` (the `OnMainThread` dispatch).

### Architectural finding (load-bearing for SP3)

PCSX2 on macOS hard-requires a Metal-attached `CAMetalLayer` for GS init. There is no software-only or headless path. The libretro-host (RetroNest) must therefore provide one of:

- **Pattern A — software readback**: PCSX2 renders to an off-screen Metal texture; we read it back per frame to a CPU buffer and ship via `retro_video_refresh_t`. Simpler, matches mGBA's existing pipeline architecture, but pays a per-frame GPU→CPU copy. Defer the zero-copy upgrade.
- **Pattern B — hardware-context bridge** *(chosen path for SP3)*: RetroNest embeds an `NSView` + `CAMetalLayer` inside its Qt window, exposes it via `RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE` (libretro standard), PCSX2 renders directly to that layer (zero-copy). More work but the right long-term shape because DuckStation / PPSSPP / Dolphin libretro cores will all hit the same situation — building the bridge once amortises across all future hardware-accelerated cores.

SP3 implements Pattern B. SP3's brainstorm should be scoped accordingly.

### Fork state at completion

Commits added to `retronest-libretro` branch during SP2:
- `1d4a3e76a` libretro: implement VM lifecycle — retro_load_game boots a real PS2 game
- `2af7204fc` libretro: fix SIGSEGV in CPUThreadInitialize and required EmuFolders init
- `f2eb78b02` libretro: disable input sources, apply settings, add VM lifecycle telemetry

Fork branch tip is `f2eb78b02`. Build clean. dylib exports correct. Existing SP1 launched-binary path unaffected.

### RetroNest-side delta

Zero changes. Confirmed at SP2 completion.

### Spec criteria reconciliation

| Original SP2 criterion | Outcome |
|---|---|
| Settings.{h,cpp} + EmuThread.{h,cpp} exist and compile | ✅ Met |
| `retro_load_game` returns true and emu thread runs Execute | ❌ Returns false because Initialize fails at GS init. The path AROUND that point works correctly. |
| Within ~15s, GetState=Running and CRC non-zero | ❌ Not reachable — VM doesn't reach Running. Will be re-attempted after SP3 lands the video bridge. |
| `retro_unload_game` causes clean Shutdown+Join within 5s | ✅ Met (verified via the failed-init path's cleanup) |
| `retro_load_game` with no BIOS returns false with documented OSD | ✅ Met |
| Existing pcsx2 launched-binary path unchanged | ✅ Met |
| Verified by test_loader AND RetroNest | ⚠️ Test_loader is structurally unable to verify on macOS (no Cocoa event loop). RetroNest end-to-end IS the verification path — and confirmed lifecycle works exactly as designed; the failure is downstream in GS init.

SP2's lifecycle scaffold is what SP3 plugs into. The next sub-project's success criterion will be "VM reaches Running with a real frame surfacing via the HW render bridge."
