# PCSX2 Libretro Core — Input (Sub-project 5 of 8)

**Date:** 2026-05-11
**Status:** Design — pending implementation plan
**Owner:** mark
**Scope:** Fifth sub-project of the multi-phase PCSX2-to-libretro port.
**Predecessors:** [Skeleton (SP1)](2026-05-11-pcsx2-libretro-skeleton-design.md), [VM Lifecycle (SP2)](2026-05-11-pcsx2-libretro-vm-lifecycle-design.md), [HW Render Bridge (SP3)](2026-05-11-pcsx2-libretro-video-bridge-design.md), [UX Overlays (SP3.5)](2026-05-11-pcsx2-libretro-ux-overlays-design.md), [Audio Output (SP4)](2026-05-11-pcsx2-libretro-audio-output-design.md). All complete.

## Context

After SP4 the user can launch Ratchet & Clank in RetroNest and see + hear it, but cannot play it — `InputSources/SDL/XInput/DInput/RawInput` are all forced to `false` in `pcsx2-libretro/Settings.cpp` (lines 196–199), so PCSX2's PAD subsystem has no source of input events. The libretro callbacks `input_poll_cb` and `input_state_cb` are cached at core init (`LibretroFrontend.cpp:100–101`) but never read.

SP5 wires `retro_input_state_t` into PCSX2's PAD subsystem by introducing a new `InputSource` subclass — `LibretroInputSource` — that PCSX2's `InputManager` polls each frame via the existing `VMManager::PollSources` call site. The subclass reads libretro state via the cached callbacks and feeds events into `InputManager::InvokeEvents`, which routes them to the PAD/DualShock 2 emulation.

## Architectural starting point

PCSX2's input plumbing:

- `pcsx2/Input/InputSource.{h,cpp}` — base class. 13 pure virtuals (lifecycle, polling, binding parse/convert, device enumeration, generic-binding mapping, controller layout, motor state).
- `pcsx2/Input/InputManager.{h,cpp}` — owns sources via `static std::array<std::unique_ptr<InputSource>, InputSourceType::Count> s_input_sources`. Fixed-size: any new source type means a new enum entry.
  - `InputManager::ReloadSources(si, lock)` dispatches `UpdateInputSourceState<T>(si, lock, InputSourceType::X)` per known type. SDL is unconditional; DInput/XInput are `#ifdef _WIN32`.
  - `InputManager::PollSources()` iterates `s_input_sources[]` and calls `PollEvents()` on each initialized source. Called per-frame from `VMManager::PollSources` (`pcsx2/VMManager.cpp:2777` and `:2935`).
  - `InputManager::InvokeEvents(key, value, generic_key)` is the ingress: PreprocessEvent then route to bound consumers (PAD or hotkeys).
- `pcsx2/SIO/Pad/Pad.{h,cpp}` — PS2 controller emulation; receives events from InputManager via the binding map.
- Existing concrete sources: `SDLInputSource`, `DInputSource`, `XInputSource`. The canonical reference is `SDLInputSource` — it implements all virtuals, uses `MakeGenericControllerButtonKey(InputSourceType::SDL, player_id, button)` to construct keys, and calls `InputManager::InvokeEvents` from inside its `PollEvents`.

The `GenericInputBinding` enum (`pcsx2/Config.h:114`) covers the full DualShock 2 surface — digital, both analog sticks, L2/R2 analog triggers, plus `LargeMotor`/`SmallMotor`. Maps 1:1 onto libretro's joypad + analog button + analog stick model.

**Already wired (no work needed):**

- `pcsx2-libretro/LibretroFrontend.h:26–27` — `input_poll_cb` and `input_state_cb` are fields in `FrontendState`.
- `pcsx2-libretro/LibretroFrontend.cpp:100–101` — `retro_set_input_poll` and `retro_set_input_state` cache the cbs.
- `RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp:86–95` — `inputStateTrampoline` routes `RETRO_DEVICE_JOYPAD` and `RETRO_DEVICE_ANALOG` queries through `m_input.buttonPressed` / `m_input.analogAxis`.
- `RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp:225` — `retro_set_input_state` is called at init.

SP5's job is to read from those cached callbacks. `VMManager::PollSources` already polls every frame, so `retro_run` does not need modification — `PollEvents()` on our new source gets driven automatically.

## Goal

A real PS2 game launched through RetroNest is fully playable using a connected gamepad (or RetroNest's keyboard-to-virtual-joypad mapping). Both controller ports are wired; the full DualShock 2 surface (digital + analog sticks + analog triggers) is exposed.

**Definition of done:**

1. Launching Ratchet & Clank in RetroNest: title screen advances on Start; in-game movement responds to the left stick; camera responds to the right stick; jump/attack/strafe (Cross/Square/L2/R2) respond.
2. The new `LibretroInputSource` is registered via a new `InputSourceType::Libretro` enum entry and is enabled by `InputSources/Libretro = true` in our `Settings.cpp`.
3. Settings.cpp writes 24 PAD binding lines × 2 ports (`Pad1/*`, `Pad2/*`) mapping `Libretro-N/…` keys to PS2 actions before `VMManager::Internal::LoadStartupSettings()`.
4. Per-frame `VMManager::PollSources` reaches our `LibretroInputSource::PollEvents`, which calls `g_frontend.input_poll_cb()`, queries `g_frontend.input_state_cb` per port for digital + analog, diffs against cached state, and emits `InputManager::InvokeEvents` only on changes.
5. Pad2 is independent of Pad1. A second connected libretro input plays as Player 2.
6. Pause via Cmd+Shift+Escape continues to work (input poll is gated by VM running state — handled by VMManager).
7. Load/unload/reload a game without crash; inputs work on the second load (verifies destructor cleanup).
8. Existing SP3 video and SP4 audio paths are unchanged. mGBA input is unchanged.

## Non-goals (deferred to later sub-projects or out of scope)

- **Rumble / vibration.** Decided during brainstorm. `UpdateMotorState` is a no-op; `EnumerateMotors` returns empty. SP5.5 picks this up: implements `UpdateMotorState`, adds `RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE` env query, audits RetroNest's libretro adapter for `retro_rumble_interface` forwarding.
- **Multitap (ports 3–8).** PCSX2 + libretro both support up to 8 ports. Niche; SP5 stops at ports 1–2.
- **Hotkeys via libretro joypad** (fast-forward, save state, etc.). RetroNest's frontend hotkey system owns these. SP7 (settings push) can wire core-side hotkey binding configuration if user requests it.
- **Pressure-sensitive face buttons (DS2 RAPI).** Libretro doesn't expose per-button pressure. Rare in real games. Out of scope.
- **Analog/digital mode toggle.** We force analog (`RETRO_DEVICE_ANALOG`) on both ports. Standard for modern PS2 emulation; games that introspect see "analog always on".
- **Pointer / lightgun / keyboard-as-keyboard.** Whatever RetroNest maps to libretro joypad+analog is what we consume. We don't read `RETRO_DEVICE_KEYBOARD` or `RETRO_DEVICE_POINTER` directly. Future PS2 USB-keyboard / GunCon support is its own sub-project.
- **Hotplug.** `ReloadDevices` is a no-op. Libretro doesn't surface controller-connect/disconnect events.
- **SP3.6 quit crash.** Unaffected by input.

## Architecture

```
                    ┌────────────────────────────────────┐
                    │ VMManager (EE/MTGS thread)         │
                    │   per-frame: PollSources()         │
                    └─────────────────┬──────────────────┘
                                      │ iterates s_input_sources[]
                                      ▼
              ┌──────────────────────────────────────────────┐
              │ LibretroInputSource::PollEvents()            │
              │                                              │
              │  1. g_frontend.input_poll_cb()               │
              │  2. for port in {0, 1}:                      │
              │       digital: query 16 RETRO_DEVICE_JOYPAD_*│
              │                buttons; diff vs cached state;│
              │                InvokeEvents on edges         │
              │       analog : query 4 RETRO_DEVICE_ANALOG   │
              │                axes (L stick X/Y, R stick X/Y)│
              │                + L2/R2 analog buttons via    │
              │                ANALOG_BUTTON device subtype; │
              │                InvokeEvents on value change  │
              └─────────────────┬────────────────────────────┘
                                ▼
             InputManager::InvokeEvents(key, value, generic)
                                ▼
                        PAD / DualShock2 emulation
```

No new threads, no new sync primitives. Polling cadence matches existing `VMManager::PollSources` cadence (~60 Hz when VM running).

### File layout

| Path | Change | Notes |
|---|---|---|
| `pcsx2/Input/InputManager.h` | +1 enum entry | Add `Libretro` to `InputSourceType`. Placed after `SDL` and before the `#ifdef _WIN32 DInput/XInput` block, so it precedes `Count` on all platforms. Comment-flagged `// pcsx2-libretro… (SP5)`. |
| `pcsx2/Input/InputManager.cpp` | +~8 lines (4 touches) | (a) `s_input_class_names` adds `"Libretro"`. (b) `GetInputSourceDefaultEnabled` adds `case Libretro: return false;` (we enable explicitly in Settings.cpp). (c) `ReloadSources` adds `UpdateInputSourceState<LibretroInputSource>(si, settings_lock, InputSourceType::Libretro);` plus the `#include "pcsx2-libretro/LibretroInputSource.h"`. (d) Any other complete-coverage switch over `InputSourceType` (none found beyond the above). All comment-flagged. |
| `pcsx2-libretro/LibretroInputSource.h` | NEW | Subclass declaration; per-port cached digital bitmask + analog int16 axes; libretro→generic-binding mapping tables. |
| `pcsx2-libretro/LibretroInputSource.cpp` | NEW | Implementation. Modeled on `SDLInputSource` shape. Uses `MakeGenericControllerButtonKey(InputSourceType::Libretro, port, button_id)` / `MakeGenericControllerAxisKey(InputSourceType::Libretro, port, axis_id)` for key construction. |
| `pcsx2-libretro/CMakeLists.txt` | +1 line | Add `LibretroInputSource.cpp` to `target_sources`. |
| `pcsx2-libretro/Settings.cpp` | +~30 lines | Add `InputSources/Libretro = true` next to the existing four `InputSources/* = false` lines. Add a helper that writes 24 PAD binding lines per port for `Pad1` and `Pad2`. All writes occur *before* `VMManager::Internal::LoadStartupSettings()`. |
| `pcsx2-libretro/LibretroFrontend.cpp` | +~3 lines | `retro_set_controller_port_device` handler: accept `RETRO_DEVICE_ANALOG` (or `RETRO_DEVICE_JOYPAD`) for ports 0 and 1; log + ignore other types. Optional `RETRO_ENVIRONMENT_SET_CONTROLLER_INFO` declaration on init so the frontend lists analog as the controller type. |

**Total upstream-file deviation:** ~9 lines across 2 files (`InputManager.h`, `InputManager.cpp`), all within `pcsx2/Input/`. About double SP4's 5-line footprint, but same "single dispatch table extension" character — same comment-flag convention.

### `LibretroInputSource` virtual surface

| Virtual | Implementation |
|---|---|
| `Initialize(si, lock)` | Read `InputSources/Libretro` enabled flag. Cache per-port enabled state (both ports enabled by default in SP5). Set `m_initialized = true`. |
| `UpdateSettings(si, lock)` | Re-read enabled state. No-op if unchanged. |
| `ReloadDevices` | No-op (libretro doesn't surface hotplug). Return false. |
| `Shutdown` | Clear cached state. Set `m_initialized = false`. |
| `IsInitialized` | Return `m_initialized`. |
| `PollEvents` | The real work — see Architecture diagram. |
| `ParseKeyString(device, binding)` | `("Libretro-0", "Cross")` → `InputBindingKey{ source=Libretro, source_index=0, subtype=ControllerButton, data=button_id }`. Supports: `Button{N}` (raw libretro RETRO_DEVICE_ID_JOYPAD_*), the 14 generic-name aliases (`Up`, `Down`, `Left`, `Right`, `Cross`, `Circle`, `Square`, `Triangle`, `L1`, `R1`, `L3`, `R3`, `Start`, `Select`), the four analog stick half-axes (`+LeftX`, `-LeftX`, `+LeftY`, `-LeftY`, `+RightX`, `-RightX`, `+RightY`, `-RightY`), and the two analog triggers (`+L2`, `+R2`). |
| `ConvertKeyToString(key, display, migration)` | Inverse of `ParseKeyString`. |
| `ConvertKeyToIcon(key)` | Empty `TinyString` (no icon font for libretro). |
| `EnumerateDevices` | Static: `{"Libretro-0", "Libretro Pad 0"}, {"Libretro-1", "Libretro Pad 1"}`. |
| `EnumerateMotors` | Empty vector. |
| `GetGenericBindingMapping(device, mapping)` | Static 24-entry table per device: `Libretro-N/Cross → Cross`, `Libretro-N/+LeftY → LeftStickDown`, etc. |
| `GetControllerLayout(index)` | `InputLayout::Playstation`. |
| `UpdateMotorState(key, intensity)` | No-op (rumble deferred to SP5.5). |

### Lifecycle

| Event | Behavior |
|---|---|
| `retro_init` → `Settings::InitializeDefaults` | Writes `InputSources/Libretro = true` (next to the existing four `false` lines), writes 24 binding lines × 2 ports into `Pad1`/`Pad2` sections. Then `VMManager::Internal::LoadStartupSettings()` applies the layer (existing call at Settings.cpp:212). |
| `retro_set_controller_port_device(port, device_type)` | Frontend (RetroNest) may call before/during load to select controller type. We accept `RETRO_DEVICE_JOYPAD` and `RETRO_DEVICE_ANALOG`, default analog. Log + ignore other types. |
| `retro_load_game` → `VMManager::Initialize` | Existing path. `InputManager::ReloadSources` is invoked during VM init; it sees `InputSources/Libretro = true`, constructs `LibretroInputSource`, calls `Initialize(si, lock)`. `InputManager::ReloadBindings` parses `Pad1`/`Pad2` and registers our `Libretro-N/…` keys in the PAD binding map. |
| Per video frame: `VMManager::PollSources` | Iterates `s_input_sources[]`. Our `PollEvents` runs: calls `g_frontend.input_poll_cb()`, then for each enabled port queries libretro state, diffs against cached snapshot, fires `InputManager::InvokeEvents` on edges only. |
| `retro_unload_game` → `InputManager::CloseSources` | Our `Shutdown` runs; clears cached state. PCSX2 releases the `unique_ptr`; dtor runs. |
| `retro_deinit` | If sources weren't already closed, the global teardown closes them. Idempotent. |

**Critical ordering invariant:** `InputSources/Libretro = true` and all PAD bindings must be written *before* `LoadStartupSettings()` in `Settings::InitializeDefaults`. The existing SDL/XInput/DInput/RawInput `false` lines (Settings.cpp:196–199) follow this ordering — we slot in next to them.

**Verification step before implementation:** Implementation plan step 1 confirms `InputManager::ReloadSources` is auto-invoked during VM init. If not, Settings.cpp explicitly invokes `InputManager::ReloadSources(g_si, lock)` after `LoadStartupSettings()`.

### Binding configuration (Settings.cpp adds)

The 24 GenericInputBinding entries map 1:1 between libretro joypad/analog and PS2 DualShock 2. Generated by a static helper to avoid a wall of `SetStringValue` calls.

```
// Section "Pad1" (port 0), then "Pad2" (port 1).
// Key = Value (binding string that LibretroInputSource::ParseKeyString understands)
Up        = Libretro-N/Up
Right     = Libretro-N/Right
Down      = Libretro-N/Down
Left      = Libretro-N/Left
Cross     = Libretro-N/Cross
Circle    = Libretro-N/Circle
Square    = Libretro-N/Square
Triangle  = Libretro-N/Triangle
L1        = Libretro-N/L1
R1        = Libretro-N/R1
L2        = Libretro-N/+L2        // analog button, full-axis positive
R2        = Libretro-N/+R2
L3        = Libretro-N/L3
R3        = Libretro-N/R3
Start     = Libretro-N/Start
Select    = Libretro-N/Select
LUp       = Libretro-N/-LeftY     // analog axis, negative half
LDown     = Libretro-N/+LeftY
LLeft     = Libretro-N/-LeftX
LRight    = Libretro-N/+LeftX
RUp       = Libretro-N/-RightY
RDown     = Libretro-N/+RightY
RLeft     = Libretro-N/-RightX
RRight    = Libretro-N/+RightX
```

**Pre-implementation verification:** Implementation plan step 1 confirms the PAD section name format (`Pad1`/`Pad2`) and the action-key names (`Cross`, `LUp`, etc.) against `pcsx2/SIO/Pad/Pad.cpp` and `pcsx2-qt`'s PAD config. If the convention has shifted, step 1 adjusts the table before any code lands.

### Value conversion

| Source | libretro returns | We emit to `InvokeEvents` |
|---|---|---|
| Digital button (`RETRO_DEVICE_JOYPAD`, ID 0–15) | `int16` — 0 or 1 | 0.0 or 1.0 |
| Analog stick axis (`RETRO_DEVICE_ANALOG`, INDEX_LEFT/RIGHT, ID X/Y) | `int16 [-32768, 32767]` | `NormalizeS16(v)` (reuse `SDLInputSource`'s helper; clamps and divides by 32767). Sign retained — the binding's `+/-` prefix in `ParseKeyString` direction-encodes which half. |
| Analog trigger L2/R2 (`RETRO_DEVICE_ANALOG`, INDEX_BUTTON, ID L2/R2) | `int16 [0, 32767]` | `value / 32767.0f`. Bound as `+L2`/`+R2` (full positive axis). |

**Edge detection.** Per port we cache `prev_digital_bitmask` (16 bits) and `prev_analog[6]` (4 stick axes + 2 trigger). Diff each poll; emit only on change. Digital threshold: any bit flip. Analog threshold: change ≥ 64 int16 units (~0.2% of range — under hardware noise floor). Avoids flooding `InvokeEvents` with sub-LSB jitter; matches SDL source's behavior pattern.

**Initial-state quirk.** First `PollEvents` after `Initialize` treats all cached state as "all zeros" — first poll therefore emits zero events unless the user is actively holding something. Acceptable; same as SDL.

## Error handling

| Failure | Behavior |
|---|---|
| `g_frontend.input_poll_cb` / `input_state_cb` null at `PollEvents` time | Skip the poll cleanly (early return after a one-shot warning log). VM continues; user sees a non-responsive controller and a single line in the log. Should not happen — both are cached during `retro_set_input_poll` / `retro_set_input_state` at core init. |
| `LibretroInputSource::Initialize` called before frontend cached the callbacks | Allow it. We don't read the cb pointers during init; they're only read in `PollEvents`. If the first few polls run before the frontend has set them, they're null-checked above. |
| Frontend doesn't honor `retro_set_controller_port_device(_, RETRO_DEVICE_ANALOG)` | Libretro spec guarantees `RETRO_DEVICE_ANALOG` queries still work on a `RETRO_DEVICE_JOYPAD` port — analog values just won't move on frontends that don't synthesize analog. Digital path remains functional. Not our concern to mitigate. |
| Port 2 unbound (only one physical controller connected) | RetroNest returns 0 for all port-1 queries. Our edge detection emits nothing for port 1. PAD subsystem treats Pad2 as "controller connected, no inputs" — same as a real PS2 with two pads connected and the second idle. Acceptable. |
| Binding parse failure on `Libretro-N/Foo` (typo in Settings.cpp) | `ParseKeyString` returns `std::nullopt`; `InputManager::ReloadBindings` logs and drops that binding. One missing button on the pad. Smoke test catches it. |
| `Pad1`/`Pad2` section doesn't match PCSX2's expected convention | PAD subsystem ignores our bindings; controller appears completely dead. Mitigation: implementation plan step 1 reads `pcsx2/SIO/Pad/Pad.cpp` and `pcsx2-qt`'s PAD config to confirm the section/key format before writing any bindings. Verified before code lands. |
| `InputManager::ReloadSources` not auto-called after `Settings::InitializeDefaults` | Implementation plan step 1 verifies the call path. If not auto-called, Settings.cpp explicitly invokes `InputManager::ReloadSources(g_si, lock)` after `LoadStartupSettings()`. |
| `s_input_sources` array size mismatch after enum addition | The array is sized by `InputSourceType::Count`. Adding `Libretro` before `Count` automatically resizes; no other change needed. Build error if any code statically asserts on the old size — none found in audit. |

## Verification (testing strategy)

Following SP3/SP4: integration-verified on a real game, log-driven for the rest.

1. **Build & link** — `cmake --build build --target pcsx2_libretro` succeeds. No undefined symbols on `LibretroInputSource`. `s_input_sources` array size matches the new `InputSourceType::Count`.
2. **End-to-end smoke (digital)** — Launch R&C 2 in RetroNest with FastBoot enabled. Press Start to skip the title splash, navigate the start menu with the D-pad, press Cross to launch a save. Each button visibly responds.
3. **End-to-end smoke (analog)** — In-game: left stick moves Ratchet, right stick moves the camera. Both analog axes track smoothly (no stair-stepping from edge-detection threshold).
4. **L2/R2 analog** — Strafe or aim with L2/R2. Partial press registers as partial axis (verifiable in-game; if not, the diagnostic log line at #6 shows partial values).
5. **Two ports** — If a two-player game is on hand, simulate two libretro inputs; verify Pad2 responds independently. If not, log-only validation: synthesize a Pad2 button via `retro_input_state_cb` mock and confirm `Libretro-1` PollEvents fires.
6. **Diagnostic log line on first event** — `INFO: LibretroInputSource first event: port=%u key=%s value=%.3f` — one-shot, confirms wiring without spamming.
7. **Lifecycle** — Load game → unload → load same game. Inputs work on the second load (verifies `Shutdown`/destructor cleanup; no leaked binding state).
8. **No regressions** — mGBA input still works in RetroNest (we don't touch the mGBA path). PCSX2 binary launcher (existing non-libretro path RetroNest still uses) still works (we don't touch `pcsx2-qt`).

Explicitly **not in scope** for SP5 testing:

- Rumble (deferred to SP5.5).
- Multitap / ports 3–8 (deferred).
- Hotkeys via libretro joypad (RetroNest frontend owns these).
- Pressure-sensitive face buttons (out of scope; libretro doesn't expose).

## Known limitations (carry into project memory after ship)

- **Bindings are hardcoded in Settings.cpp.** No user-facing rebinding. SP7 (settings push) makes these user-overridable.
- **Rumble is silent.** `UpdateMotorState` is a no-op. SP5.5 wires it.
- **Two ports only.** Multitap not supported.
- **Analog-mode forced on.** Games that explicitly require digital-mode (very rare) see analog. No mid-session toggle.
- **No hotplug.** A libretro frontend that surfaces device-changed events between polls is ignored.

## Discipline note (project memory update)

Project memory currently states the SP4 exception: AudioBackend dispatch table extension. SP5 extends this same pattern to InputSourceType — 1 enum entry + ~4 ancillary touches in `pcsx2/Input/InputManager.{h,cpp}`. All comment-flagged with `// pcsx2-libretro… (SP5)`.

The deviation is bounded and intentional:

- The integration seam is genuinely not in `Host::` — `InputSource` is a class with a static array dispatch keyed by enum, with no public hook.
- Alternatives considered and rejected during brainstorming: (a) direct `InvokeEvents` from `retro_run` with synthesized Keyboard-source keys (architecturally mismatched; analog support awkward); (b) hijack the `SDL` enum slot (confusing diff; brittle rebases).
- Each edit is comment-flagged inline.
- The rule remains in force for everything else. SP5 widens the "subsystem dispatch table extension" exception narrowly to cover input alongside audio. After SP5 ships, update `project_pcsx2_libretro_port.md` to generalize the SP4 audio-backend exception into a "subsystem dispatch table" exception.

## Order-of-operations summary

1. Confirm `Pad1`/`Pad2` section/key format and `InputManager::ReloadSources` auto-invocation path. Read-only step, no code.
2. Add `InputSourceType::Libretro` enum entry + the ~4 InputManager.cpp ancillary touches (all comment-flagged). Build to confirm enum-size propagation.
3. Implement `LibretroInputSource.{h,cpp}` — stub all virtuals first; verify build links.
4. Flesh out `PollEvents`, parse/convert helpers, and `GetGenericBindingMapping`.
5. Wire `Settings.cpp` — `InputSources/Libretro = true` + the 24×2 bindings, all written before `LoadStartupSettings()`.
6. Wire `LibretroFrontend.cpp` — `retro_set_controller_port_device` handler advertising `RETRO_DEVICE_ANALOG`.
7. Build, run R&C 2, walk the 8-step verification.
8. Ship; update project memory + create SP5-shipped session-handoff memory.

## Predecessors and successors

- **Predecessors:** SP1 (skeleton), SP2 (VM lifecycle), SP3 (HW render bridge), SP3.5 (UX overlays), SP4 (audio output). All complete.
- **Successors:** SP6 (save states + memcards), SP7 (settings push — makes our hardcoded SP5 bindings user-overridable, and retires the hardcoded `EmuFolders::Resources` + `params.fast_boot` from earlier SPs), SP8 (RetroNest adapter rewrite). SP5.5 (rumble) is a natural follow-up if user wants it before SP6. SP3.6 (quit crash) remains parked.
