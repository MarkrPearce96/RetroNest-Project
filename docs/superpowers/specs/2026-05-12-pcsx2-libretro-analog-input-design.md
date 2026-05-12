# PCSX2 Libretro Core — Analog Sticks, Triggers & Rumble (Sub-project 5.5)

**Date:** 2026-05-12
**Status:** Design — pending implementation plan
**Owner:** mark
**Scope:** Closes the analog-input gap left by SP5. Wires up analog sticks, L2/R2 analog triggers, rumble, port 2 (player 2), and RetroNest-side deadzone plumbing.
**Predecessors:** [SP5 — Input (digital)](2026-05-11-pcsx2-libretro-input-design.md), [SP4 — Audio Output](2026-05-11-pcsx2-libretro-audio-output-design.md), and the SP4.x audio-sync follow-up. All complete.

## Context

After SP5 the user can play R&C 2 with digital input only — face buttons, D-Pad, L1/R1, Start/Select work. The right stick (camera control), L2/R2 (strafe), and rumble are all dead. R&C 2 is barely playable without analog. SP5.5 closes that gap.

The PCSX2 side is already wired (SP5 work):

- `pcsx2-libretro/LibretroInputSource.cpp` queries `RETRO_DEVICE_ANALOG` with `RETRO_DEVICE_INDEX_ANALOG_{LEFT, RIGHT, BUTTON}` plus X/Y axes — returns 0 today because RetroNest's trampoline rejects non-JOYPAD queries.
- `pcsx2-libretro/Settings.cpp:106–113` writes Pad1/Pad2 analog half-axis bindings (`-LeftY`, `+LeftY`, `-LeftX`, `+LeftX`, `-RightY`, `+RightY`, `-RightX`, `+RightX`).
- `pcsx2-libretro/Settings.cpp:99–100` binds L2/R2 to `+L2`/`+R2` analog triggers.
- Controller type set to `DualShock2` for both pads (`Settings.cpp:121`).
- PCSX2's pad deadzone defaults are `0.0f` (`pcsx2/SIO/Pad/PadTypes.h:102,106`). RetroNest-side deadzone application carries no double-deadzone risk.

Three pieces are missing on the RetroNest side:

1. **`InputRouter`** is a 16-bit-per-port atomic bitmask. It stores press/release only; analog magnitudes are dropped.
2. **`inputStateTrampoline`** (`core_runtime.cpp:99–108`) only handles `RETRO_DEVICE_JOYPAD`. Anything else returns 0.
3. **Rumble**: PCSX2 will query `RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE`. `environment_callbacks.cpp` has no handler — PCSX2 sees no rumble support and never tries to fire it.

The existing `SdlInputManager::SDL_CONTROLLERAXISMOTION` branch already routes `+axis`/`-axis` digital-emulation writes to `InputRouter::setButtonPressed` (so D-Pad-bound-to-stick works), but the magnitude itself is discarded. SP5.5 augments that path with magnitude propagation rather than replacing it.

## Goal

R&C 2 in RetroNest:

- Right stick rotates the camera smoothly with proportional speed.
- Left stick moves Ratchet in any direction (not 8-way digital).
- L2/R2 strafe; trigger pressure registers as analog magnitude on the PS2 pad.
- Rumble fires when shooting weapons; STRONG and WEAK motors driven independently.
- Pausing the game stops active rumble.
- Port 2 wiring exists at the data layer (P1+P2 titles work without UI port-assignment).
- A first-pass deadzone (default 0.15 inner, radial for sticks, per-axis for triggers) is applied in RetroNest with settings-layer storage ready for a future UI to bind to.

## Out of scope

- **Per-game deadzone tuning UI** — data layer ships in SP5.5; UI deferred to SP7 or later.
- **Hot-plug enhancements beyond what SDL already does** — SDL's existing `SDL_CONTROLLERDEVICEADDED`/`REMOVED` handling is left as-is. First-come-first-served port assignment.
- **DualShock 2 pressure-sensitive face buttons** — no modern SDL controller exposes per-button pressure; ~no users affected.
- **Custom port-assignment UI** — first-opened controller → port 0, second → port 1. SP7 UI can override later.
- **Tuning PCSX2's own deadzone keys** — left at the `0.0f` default; deadzone is RetroNest-side only.

## Architecture & components

All changes are additive RetroNest-side. **No upstream PCSX2 files touched.** Rebase discipline preserved.

### `RetroNest-Project/cpp/src/core/libretro/input_router.{h,cpp}`

- New `enum class RetroPadAxis : int { LeftX = 0, LeftY = 1, RightX = 2, RightY = 3, L2 = 4, R2 = 5, Count = 6 };`
- New member `std::array<std::atomic<int16_t>, NUM_PORTS * 6> m_axes{};` alongside the existing `m_state` bitmask. Default-initialized to zero.
- New writer `void setAxis(int port, RetroPadAxis axis, int16_t raw);` — called from the Qt/SDL thread, stores raw SDL value (`-32768..32767`) via `memory_order_relaxed`.
- New reader `int16_t axis(int port, RetroPadAxis axis) const;` — called from the core thread, applies deadzone (radial for stick pairs, per-axis for triggers) and returns the rescaled value.
- New deadzone storage `std::atomic<float> m_innerDeadzone{0.15f};` plus `void setInnerDeadzone(float v);` (clamped to `[0.0, 0.5]`). This is the data-layer property a future UI will bind to.
- Threading invariant from SP5 preserved: writes on Qt thread, reads on core thread, lock-free `atomic<int16_t>` per axis. No mutex.

### `RetroNest-Project/cpp/src/core/sdl_input_manager.{h,cpp}`

- **No new map needed.** The existing `m_deviceIndices` (populated in `openController` at `sdl_input_manager.cpp:360-366` via lowest-free-index assignment) already provides the per-controller port number. PCSX2 uses `Libretro-0/...`, `Libretro-1/...` bindings keyed by the same number, so device-index and port are the same value.
- Existing button-write sites (`sdl_input_manager.cpp:434, 509, 552, 557, 562, 564`) change from hardcoded `0` to the local `devIdx` variable already computed at each call site from `m_deviceIndices.value(event.cbutton.which, 0)` / `event.caxis.which`. `InputRouter::setButtonPressed` silently no-ops for `port >= NUM_PORTS` (existing behavior), so a 5th+ controller is harmless.
- `SDL_CONTROLLERAXISMOTION` branch: in addition to the existing `+axis`/`-axis` digital-emulation writes, call `m_emulationTarget->setAxis(devIdx, mapped_axis, event.caxis.value)`. Raw value, no deadzone applied at write time.
- A small static helper `RetroPadAxis sdlAxisToRetroPadAxis(SDL_GameControllerAxis)` maps SDL's six axes (`LEFTX/Y`, `RIGHTX/Y`, `TRIGGERLEFT/RIGHT`) onto our enum.
- New public `bool setRumbleMotor(int port, retro_rumble_effect motor, uint16_t strength);` — accepts a single motor update (matches libretro's per-motor `set_rumble_state` contract), updates the per-port cache, then calls `SDL_GameControllerRumble(controller, cache.low, cache.high, kRumbleDurationMs)`. Resolves `port → SDL_JoystickID` via reverse lookup on `m_deviceIndices` (port is the value, jid is the key). Returns `false` if the port has no live controller.
- Per-port rumble state cache (two `uint16_t` per port: last STRONG → low-freq, last WEAK → high-freq) lives inside `SdlInputManager`. PCSX2's per-motor calls merge here before reaching SDL.
- `static constexpr uint32_t kRumbleDurationMs = 100;` — duration passed to `SDL_GameControllerRumble`. PCSX2 re-issues rumble updates frequently while an effect is active, so SDL retriggers before duration expires.

### `RetroNest-Project/cpp/src/core/libretro/core_runtime.{h,cpp}`

- `inputStateTrampoline` gains a `RETRO_DEVICE_ANALOG` branch alongside the existing `RETRO_DEVICE_JOYPAD` branch:
  - `index == RETRO_DEVICE_INDEX_ANALOG_LEFT`, `id == RETRO_DEVICE_ID_ANALOG_X` → `axis(port, LeftX)`
  - `index == RETRO_DEVICE_INDEX_ANALOG_LEFT`, `id == RETRO_DEVICE_ID_ANALOG_Y` → `axis(port, LeftY)`
  - `index == RETRO_DEVICE_INDEX_ANALOG_RIGHT`, `id == ANALOG_X` → `axis(port, RightX)`
  - `index == RETRO_DEVICE_INDEX_ANALOG_RIGHT`, `id == ANALOG_Y` → `axis(port, RightY)`
  - `index == RETRO_DEVICE_INDEX_ANALOG_BUTTON`, `id == RETRO_DEVICE_ID_JOYPAD_L2` → `axis(port, L2)`
  - `index == RETRO_DEVICE_INDEX_ANALOG_BUTTON`, `id == RETRO_DEVICE_ID_JOYPAD_R2` → `axis(port, R2)`
  - Anything else under `RETRO_DEVICE_ANALOG` → return 0 (forward-compat).
- `CoreRuntime` holds a new non-owning `SdlInputManager*` pointer (set by the caller alongside the existing InputRouter pointer) so the rumble thunk can locate the manager via `g_current`.
- `pause()` and `stop()` paths fire `setRumble(port, 0, 0, 0)` for every mapped port to silence stuck motors. Mirrors the existing audio `setPaused` pattern in `pause()`.

### `RetroNest-Project/cpp/src/core/libretro/environment_callbacks.{h,cpp}`

- New case for `RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE` (cmd 23). Fill the caller's `retro_rumble_interface` with a static `set_rumble_state` thunk and return `true`.
- The thunk resolves `g_current->m_sdlInput` and forwards to `setRumbleMotor(port, effect, strength)`. Duration is owned by `SdlInputManager` (`kRumbleDurationMs`).

## Data flow

### Write path — analog axes (Qt thread)

```
SDL_PollEvent → SDL_CONTROLLERAXISMOTION
  ↓
SdlInputManager::pollEvents (existing branch, sdl_input_manager.cpp:523)
  devIdx = m_deviceIndices.value(event.caxis.which, 0)  // existing; was passed as port=0 to setButtonPressed
  axis   = sdlAxisToRetroPadAxis(event.caxis.axis)      // new helper
  ↓
  [existing, FIXED] +axis/-axis digital-emulation writes to InputRouter::setButtonPressed(devIdx, slot, ...)
  [NEW]             InputRouter::setAxis(devIdx, axis, event.caxis.value)   // raw -32768..32767
```

The existing digital-emulation path stays active: bindings of D-Pad to a stick still register as button presses; PCSX2's analog bindings (`-LeftY`, `+L2`, etc.) now also receive magnitude.

### Read path — analog query (core thread)

```
PCSX2 LibretroInputSource → input_state_cb(port, RETRO_DEVICE_ANALOG, index, id)
  ↓
core_runtime.cpp::inputStateTrampoline
  device == RETRO_DEVICE_ANALOG?
    map (index, id) → RetroPadAxis a
    return g_current->m_input.axis(port, a)
  ↓
InputRouter::axis(port, a):
  raw = m_axes[port*6 + a].load(relaxed)
  dz  = m_innerDeadzone.load(relaxed) * 32767       // ~4915 at 0.15
  if a is L2 or R2 (per-axis 1D):
    return |raw| < dz ? 0 : rescale(raw, dz)
  else (stick pair):
    pair = paired axis of a (LeftX↔LeftY, RightX↔RightY)
    other = m_axes[port*6 + pair].load(relaxed)
    mag   = sqrt(raw*raw + other*other)
    if mag < dz: return 0
    // Sign-preserving radial rescale of just this component
    scaled_mag = (mag - dz) / (32767 - dz) * 32767
    return raw * (scaled_mag / mag)
```

Two atomic loads + one sqrt per stick-axis read. With PCSX2 polling once per frame at 60 fps and four stick axes total, this is negligible cost.

### Read path — digital query (unchanged)

`RETRO_DEVICE_JOYPAD` falls through to `buttonPressed()` exactly as it does in SP5. No behavior change for the existing digital path.

### Rumble path

```
At core startup (core thread):
  PCSX2 → retro_environment(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &iface)
    environment_callbacks fills iface.set_rumble_state = &rumbleThunk, returns true.

In-game (core thread):
  PCSX2 → iface.set_rumble_state(port, RETRO_RUMBLE_STRONG, strength)
  PCSX2 → iface.set_rumble_state(port, RETRO_RUMBLE_WEAK,   strength)
    ↓
  rumbleThunk(port, effect, strength)
    g_current->m_sdlInput->setRumbleMotor(port, effect, strength)
    ↓
  SdlInputManager::setRumbleMotor(port, effect, strength):
    update m_rumbleCache[port].low or .high      // per-motor cache, atomic
    jid = m_deviceIndices.key(port, -1)           // existing map, reverse lookup
    controller = m_controllers.value(jid, nullptr)
    if controller:
      SDL_GameControllerRumble(controller,
                               m_rumbleCache[port].low,
                               m_rumbleCache[port].high,
                               kRumbleDurationMs)
```

PCSX2 fires STRONG and WEAK as two separate calls. The per-port cache merges them so the second call doesn't overwrite the first motor's state. SDL's `SDL_GameControllerRumble` is thread-safe per SDL docs; no extra synchronization needed.

### Port-to-device mapping (Qt thread, on connect/disconnect)

No code changes. The existing `m_deviceIndices` insert/remove in `openController` (`sdl_input_manager.cpp:360-366`) and `closeController` (`:379`) already runs the lowest-free-index algorithm and is keyed by `SDL_JoystickID`. SP5.5 just *uses* this map: writes pass `devIdx` as the port; rumble reverse-looks-up via `m_deviceIndices.key(port, -1)`.

### Threading invariant

All InputRouter writes happen on the Qt thread (SDL event pump). All InputRouter reads happen on the core thread (libretro polling). Atomic loads/stores with `memory_order_relaxed` are sufficient: there is no ordering dependency between axes (PS2 hardware samples sticks independently too), and one-frame data tearing across axes is invisible at 60 Hz poll. Same model as SP5's button path.

## Error handling & edge cases

- **Unmapped device** — `m_deviceIndices.value(jid, 0)` falls back to port 0 (current behavior). For SDL events arriving after a controller is closed (race window), the InputRouter write to port 0 silently writes to a now-empty port, which is harmless. Matches existing button-path behavior.
- **Rumble on a port with no live controller** — `setRumble` returns `false`; thunk discards. The libretro `set_rumble_state` contract has no failure signal; silent drop is correct.
- **Port reassignment on hot-plug** — On reconnect, SDL issues a new `SDL_JoystickID`. The reconnecting device gets the lowest free port. A P1+P2 game where P1 disconnects and reconnects after P2 has taken port 0 will land on port 1. Acceptable for SP5.5; SP7 UI later allows manual assignment.
- **Trampoline called before `setEmulationMode` is active** — In practice cannot happen (`setEmulationMode` is called before `CoreRuntime::start`). Defensively: `g_current == nullptr` → return 0, matching existing button-path guard at `core_runtime.cpp:105`.
- **Out-of-range deadzone from future UI** — `setInnerDeadzone` clamps to `[0.0, 0.5]`. Radial-rescale math past 0.5 produces a negative denominator and would invert behavior; clamping prevents that.
- **Active rumble during pause/stop** — `CoreRuntime::pause()` and `stop()` iterate live ports and zero both motors via `setRumbleMotor(port, RETRO_RUMBLE_STRONG, 0)` + `setRumbleMotor(port, RETRO_RUMBLE_WEAK, 0)` (which results in an `SDL_GameControllerRumble(0, 0, ...)` per port). Without this, motors keep running until SDL's duration expires (could be seconds).
- **Concurrent per-motor rumble calls from PCSX2** — Two `std::atomic<uint16_t>` per port (last low + last high). Merged on each `setRumble` call. Lock-free; no race.
- **Invalid `(index, id)` combination under `RETRO_DEVICE_ANALOG`** — Whitelist the six valid pairs; everything else returns 0. Prevents out-of-bounds array access if a future libretro spec extends `ANALOG`.
- **Port 1 with no controller bound** — All axis reads return 0 (default-initialized atomic), all button reads return 0 (existing behavior). PS2 sees a quiet DualShock 2 on port 2 — same as today.

## Testing strategy

### Unit tests (`cpp/tests/`)

`InputRouter` is the only piece worth unit-testing — pure data structure, no Qt/SDL coupling:

- `setAxis(0, LeftX, 16000)` → `axis(0, LeftX)` returns the rescaled-from-deadzone value (non-zero).
- `setAxis(0, LeftX, 3000)` → `axis(0, LeftX)` returns 0 (below `0.15 * 32767 ≈ 4915` deadzone).
- Radial: `setAxis(0, LeftX, 3000)` + `setAxis(0, LeftY, 3000)` → both axes return 0 (vector magnitude ≈ 4243 < 4915).
- Radial: `setAxis(0, LeftX, 4000)` + `setAxis(0, LeftY, 4000)` → both return non-zero rescaled values (magnitude ≈ 5657 > 4915).
- `setAxis(0, L2, 16000)` → trigger is 1D, paired-axis lookup not performed.
- `setInnerDeadzone(0.0)` → raw passthrough.
- `setInnerDeadzone(0.6)` → clamped to 0.5.
- Port isolation: `setAxis(0, LeftX, 30000)` leaves `axis(1, LeftX)` at 0.

No tests for `SdlInputManager`'s new write path or `environment_callbacks` rumble handler — those need a real SDL controller and a real PCSX2 load. SP3-SP5 pattern is to validate them via the in-game smoke test rather than mocks; mocking SDL+PCSX2 buys nothing here.

### Smoke test (the gate for "SP5.5 is done")

R&C 2 in this order:

1. **Main-menu Controller test screen** — all six analog values (LeftX/Y, RightX/Y, L2, R2) track physical inputs 1:1.
2. **In-game right stick rotates camera smoothly**, with proportional speed at partial deflection.
3. **L2/R2 strafe** at varying pressure.
4. **Left stick** — Ratchet walks in any direction, not 8-way digital.
5. **Rumble fires** on weapon use, with perceptible difference between STRONG and WEAK motors.
6. **Pause/unpause during active rumble** — rumble stops on pause, no stuck-motor.

### Regression gate

Existing button paths (face buttons, D-Pad, Start/Select, in-game menu chord) must continue to work. Same SdlInputManager code, but the port arg at write sites changes from `0` to `portForDevice(jid)` — single-controller case should be functionally identical.

### Trace gate (`RETRONEST_INPUT_TRACE=1`)

Env-gated tracer parallels SP4.x's `RETRONEST_AUDIO_TRACE`:

- One line per `setAxis` write: `[sdl] port=0 axis=LeftX raw=12345`
- One line per `axis()` read, rate-limited (every 60th read or only on value change): `[input] port=0 axis=LeftX rd=8500 (raw=12345 dz=4915)`
- One line per rumble fire: `[rumble] port=0 low=32000 high=15000`

Off by default. Available for debugging if smoke test surfaces anything odd.

### Verification before claiming done

- All InputRouter unit tests green.
- R&C 2 smoke test items 1–6 manually verified.
- Manual sanity check on existing digital bindings to confirm no regression from the `portForDevice` change.

## Known limitations carried forward

- **Hot-plug port assignment is first-come-first-served.** P1 disconnecting and reconnecting after P2 has filled port 0 will land on port 1. SP7 UI resolves this.
- **Deadzone has no UI yet.** Default 0.15 is hardcoded; storage layer is ready for SP7 UI binding.
- **Rumble duration is fixed** at `kRumbleDurationMs = 100`. Relies on PCSX2 re-issuing updates to sustain a rumble effect. PCSX2 does this in normal operation; if a future game pattern emerges that breaks the assumption, the constant becomes a tuning knob.

## References

- [`pcsx2-libretro/LibretroInputSource.cpp`](../../../../pcsx2-master/pcsx2-libretro/LibretroInputSource.cpp) — PCSX2 side, already wired in SP5.
- [`pcsx2-libretro/Settings.cpp:80-128`](../../../../pcsx2-master/pcsx2-libretro/Settings.cpp) — pad bindings + `DualShock2` type.
- [`pcsx2/SIO/Pad/PadTypes.h:101-106`](../../../../pcsx2-master/pcsx2/SIO/Pad/PadTypes.h) — PCSX2's default deadzones (`0.0f`).
- [`RetroNest-Project/cpp/src/core/libretro/input_router.h`](../../../cpp/src/core/libretro/input_router.h) — current shape.
- [`RetroNest-Project/cpp/src/core/libretro/core_runtime.cpp:99-108`](../../../cpp/src/core/libretro/core_runtime.cpp) — `inputStateTrampoline` to extend.
- [`RetroNest-Project/cpp/src/core/sdl_input_manager.cpp:523-590`](../../../cpp/src/core/sdl_input_manager.cpp) — existing axis branch.
- [SP5 design](2026-05-11-pcsx2-libretro-input-design.md) — digital input predecessor.
- libretro spec — `retro_rumble_interface`, `RETRO_DEVICE_ANALOG`.
