# DuckStation libretro — 2-player (Player 2 / port 1) support

**Date:** 2026-06-23
**Status:** Design approved, pending spec review
**Scope:** Add Player 2 support to the DuckStation libretro core + RetroNest adapter. Multitap (3–8 players) is explicitly out of scope.

## Background

The DuckStation libretro integration currently supports **Player 1 only**:

- `libretro.cpp` `UpdateControllers()` reads input from **port 0 only** (`g_input_state(0, …)`) and pushes it onto the slot-0 controller (`System::GetController(0)`).
- `retro_set_controller_port_device(unsigned, unsigned)` is a **no-op stub** (`libretro.cpp:226`).
- Base settings create `Pad1` (AnalogController by default); nothing is created for `Pad2`.
- Rumble is driven for port 0 only.

The **host (RetroNest) is already multi-port ready**:

- `InputRouter::NUM_PORTS == 4`; `inputStateTrampoline(unsigned port, …)` passes the port through and queries `m_input.buttonPressed(port, slot)` / `m_input.axis(port, axis)`.
- `SdlInputManager::openController()` assigns each connected controller the "lowest available device index" (0,1,2,3 — PCSX2 player_id scheme) and uses that device index **as the port** (`setButtonPressed(devIdx, …)`).
- Disconnect (`closeController`) zeroes the device's axes so straggler events don't leak to port 0, and emits `controllersChanged()`.
- `retro_set_controller_port_device` **is resolved as a symbol** (`core_loader.cpp:39`) but the host **never calls it** today.

So a second physical controller's input already arrives at the core on port 1 — the core simply ignores it. The missing pieces are: the core reading/creating Pad2, and the host telling the core when port 1 gains/loses a pad.

## Goals

1. When a second physical controller is connected, Player 2 plays via the PS1 port-2 controller.
2. Presence is **auto-detected** — no manual toggle to "enable" Player 2.
3. **Full hot-plug**: connecting/disconnecting the 2nd pad mid-game creates/removes the emulated Pad2 live.
4. Player 2 controller type is configurable (Analog default, or Digital) via a core option.
5. Shipped Player 1 behavior must not regress.

## Non-goals

- Multitap (3–8 players).
- A dedicated memory card for Player 2 (slot 2 stays empty; can be added later).
- Per-port memory-card configuration UI.

## Design decisions

| Decision | Choice |
| --- | --- |
| Port-2 presence | Auto-detect via `retro_set_controller_port_device` callback |
| Detection timing | Full hot-plug (connect/disconnect any time) |
| Pad 2 type | AnalogController default, `duckstation_pad2_type` core option (Analog/Digital) |
| Pad 2 memory card | None (slot 2 empty) |
| Port 0 (P1) | Left exactly as today; callback governs **port 1 only** |

## Architecture

### Core (`duckstation-libretro/src/duckstation-libretro/`)

1. **Per-port input read** — `UpdateControllers()` (`libretro.cpp`): generalize from the hardcoded slot 0 to loop over ports 0–1. For each port, fetch `System::GetController(port)`; skip if null (no controller in that slot). Read RetroPad buttons via `g_input_state(port, RETRO_DEVICE_JOYPAD, 0, id)` and, when the slot's controller is an `AnalogController`, feed the analog half-axes from `g_input_state(port, RETRO_DEVICE_ANALOG, …)`. The existing port-0 mapping logic is reused unchanged per port.

2. **`retro_set_controller_port_device(unsigned port, unsigned device)`** — replace the no-op stub. Behavior:
   - For **port 1**: map `device` → a desired `ControllerType`:
     - `RETRO_DEVICE_NONE` → `ControllerType::None`
     - any other device → the type selected by `duckstation_pad2_type` (Analog or Digital)
   - Store the desired type as a **pending request** (atomic / simple guarded field). Do **not** mutate emulator state inside this function — it may be called from the host's Qt/SDL thread.
   - For **port 0**: ignore (P1 stays governed by settings, preventing regression).

3. **Apply pending change on the core thread** — at the **top of `retro_run`** (before `System::RunFrame()`), if a pending port-1 type differs from the current `g_settings.controller_types[1]`, write the new type and call `System::UpdateControllers()`. This is the same recreation path the standalone app uses on a settings change (`system.cpp:4676` → `UpdateControllers()`), so it is known-safe.

4. **Rumble for port 1** — extend the existing rumble block in `retro_run`: in addition to port 0, when `System::GetController(1)` is an `AnalogController`, drive `g_rumble_set_state(1, RETRO_RUMBLE_STRONG/WEAK, …)` from its motor strengths.

5. **`duckstation_pad2_type` core option** — add to `libretro_core_options.{h,cpp}` (values: `AnalogController`, `DigitalController`; default `AnalogController`). Read in `libretro_settings.cpp` so the core knows the **type** to instantiate when port 1 becomes present. (Presence comes from the callback; this option only chooses the type.)

6. **Base settings** — leave `Pad2` unset (slot 2 starts as `None`) and `MemoryCards`/`Card2` empty.

### Host (`RetroNest-Project/cpp/`)

1. **Port-device notification (new wire)** — drive `retro_set_controller_port_device` from the host:
   - Compute per-port presence (ports 0–1) from the connected-device set (`SdlInputManager` device indices).
   - On game boot **and** on `controllersChanged()`, for each port whose presence changed, call the loaded core's `retro_set_controller_port_device(port, device)` with `RETRO_DEVICE_NONE` (absent) or `RETRO_DEVICE_JOYPAD` (present).
   - The call crosses into the core; correctness relies on the core deferring the actual `UpdateControllers()` to its own thread (core change #3), so the host side does not need to block the core loop. Confirm the exact call site (CoreRuntime) and thread context during planning.

2. **Adapter settings** (`adapters/libretro/duckstation_libretro_adapter.cpp`) — add `duckstation_pad2_type` to `settingsSchema()`, mirroring the existing `duckstation_pad1_type` row (Controllers group). Update Recommended view if appropriate.

3. **Adapter controller bindings** — add Pad2 binding defs (mirror Pad1, section `Pad2`) to `controllerBindingDefsForType()` for the controller-config UI, **pending a planning check** (see Open question) on whether device-1 bindings are already produced generically by the shared libretro input layer.

## Testing

- **Core fidelity gate** — extend `tools/check_schema_fidelity.py` for the added `duckstation_pad2_type` option (host↔core byte match).
- **Core unit test** — device→type mapping in `retro_set_controller_port_device` (`NONE`→`None`; present→option type) and that a differing pending type triggers an `UpdateControllers()` request.
- **Host schema test** — extend `cpp/tests/test_duckstation_libretro_schema.cpp` for the new option and its default.
- **Manual GUI verification (user)** — TCC blocks the agent:
  1. Connect 2 controllers, boot a 2-player PS1 game → both players control independently.
  2. Unplug P2 mid-game → emulated Pad2 disappears (game sees port-2 controller removed).
  3. Replug P2 → Pad2 returns and works.
  4. P2 rumble fires (analog).
  5. P1-only smoke: single controller still works exactly as before.

## Risks & mitigations

- **Mid-run controller recreation** (the main risk): mitigated by reusing the proven settings-change recreation path and by deferring the swap to the core thread at the top of `retro_run` — no cross-thread mutation of emulator state.
- **P1 regression**: avoided by scoping the callback to port 1 only; port 0 stays settings-driven exactly as shipped.
- **Hot-plug straggler input**: the host already zeroes a disconnected device's axes (`closeController`), so removing Pad2 won't leave phantom input.

## Open questions (resolve during planning)

1. Does the host's shared libretro input layer already load bindings for device index 1, or are explicit Pad2/device-1 default bindings needed? (PCSX2 uses the same infra, so likely already handled — confirm.)
2. Exact host call site and thread for invoking `retro_set_controller_port_device` on the active `CoreRuntime`.

## Build / run

x86_64 under Rosetta — build `cpp/build-x86_64` with `--target RetroNest`; rebuild the DuckStation core universal via its `package.sh` (no `--arm64-only`). See `RetroNest-Project/CLAUDE.md` → "Current run mode".
