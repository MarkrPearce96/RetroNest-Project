# Libretro hotkeys — design

**Date:** 2026-05-19
**Status:** design / pending implementation plan
**Scope:** RetroNest host (cpp/). No changes to any libretro core (pcsx2-libretro, mgba-libretro, or any future core).

## Goal

Give the user a single, app-wide hotkey page that controls **every libretro core RetroNest loads**, today and in the future. Keyboard and gamepad-combo bindings drive a fixed RetroArch-style action set (save state, load state, fast-forward, pause, menu, reset, mute, volume, screenshot, slot cycling).

Standalone emulators (DuckStation, PPSSPP, Dolphin) keep their existing per-emulator hotkey page and Carbon-keystroke-synthesis dispatch unchanged. As an emulator gets ported to libretro, its per-emulator hotkey tab is dropped and the global libretro page takes over.

## Non-goals

- No per-core hotkey extensions. PCSX2-specific actions (Reload Patches, Swap Memcards, Toggle Software Renderer, Turbo, Slow Motion) are out of scope for this design — they belong as core options or in-game menu actions.
- No env calls into any libretro core. The host owns matching and dispatch end-to-end.
- No changes to the existing per-emulator `EmulatorAdapter::hotkeyBindingDefs()` interface or to standalone-adapter dispatch.
- No new save-state mechanism. `GameSession::saveStateLibretro(int slot)` / `loadStateLibretro(int slot)` already exist with multi-slot support; the matcher just calls them.
- No rewind, frame-advance, disk-swap, show-FPS-toggle, or slow-motion in v1 — each needs separate underlying plumbing.

## Architecture overview

```
┌─ RetroNest main settings ──────────────────────────────────────────┐
│                                                                    │
│  ▸ Hotkeys (NEW global page — applies to all libretro cores)       │
│      Reuses GenericHotkeyPage with LibretroHotkeys def list.       │
│      Bindings stored once at <config>/libretro_hotkeys.json.       │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘

┌─ Per-emulator "Manage" page ───────────────────────────────────────┐
│                                                                    │
│  Standalone adapters (DuckStation, PPSSPP, Dolphin):               │
│    ▸ Hotkeys (existing per-emulator page — unchanged)              │
│                                                                    │
│  Libretro adapters (mgba-libretro, pcsx2-libretro, future):        │
│    (no Hotkeys tab — global page applies)                          │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘

           Runtime — only the libretro path:

  RetroNest input router (keyboard + gamepad poll, already exists)
      │
      └─► HotkeyMatcher (new, host-side)
            consults the global libretro bindings
            on press-edge: fires built-in common handlers
              ─ retro_serialize / retro_unserialize (slot N file)
              ─ toggleFastForwardLibretro()
              ─ retronest_set_paused
              ─ retro_reset
              ─ open in-game menu
              ─ host audio mute / ±volume / screenshot
            suppresses combo modifiers from gamepad state delivered
            to the core trampoline
```

### Key architectural choices

- **One matcher in the host.** All input flows through RetroNest's existing input router. Single matcher means no double-fire, one combo-suppression implementation, one keyboard poll loop. Works for any libretro core without per-core code.
- **Fixed RetroArch-style action set.** Same actions whether the core is mGBA, PCSX2, or a hypothetical future Snes9x/DuckStation/Citra libretro port. Per-core specifics stay out of the hotkey surface entirely.
- **Bindings stored globally**, not per-emulator. Single JSON file. One place to look, one place to edit.
- **Standalone path untouched.** Existing `EmulatorAdapter::hotkeyBindingDefs()` + Carbon-synth dispatch continues to drive DuckStation/PPSSPP/Dolphin. Gradual migration: when an emulator becomes libretro, its adapter drops its hotkey defs and the global page applies.
- **Reuse existing UI infrastructure.** `GenericHotkeyPage`, `HotkeyBindingRow`, `HotkeyDef`, and the capture state machine already exist (built for standalone adapters). The new global page is a second consumer of the same widget — no new UI primitive.

## Components

### Host-side new code (cpp/)

1. **`LibretroHotkeys` constant** — single static `QVector<HotkeyDef>` enumerating the v1 action set with stable `key` strings, groups, and sensible defaults. Lives in a new `cpp/src/core/libretro/hotkey_defs.{h,cpp}` (or sibling), declared once and consumed by both the matcher and the global settings page.

2. **`HotkeyBindingStore` (global flavor)** — JSON-backed store at `<config>/libretro_hotkeys.json`. Schema: `{ action_key: [binding_string, ...] }`. Loaded at app start; written on UI save. Distinct from the per-emulator hotkey storage that standalone adapters use.

3. **`HotkeyMatcher`** — host-side service. Subscribes to keyboard events and to gamepad polls from the input router. Internal state:
   - Parsed bindings per action (cached on load / reload).
   - Per-action `pressed` boolean for press-edge detection.
   - Combo-modifier mask: which gamepad buttons are currently acting as combo modifiers.
   - `m_active` flag flipped by game-start / game-end transitions.
   - `m_currentSaveSlot` (defaults to 1; mutated by `NextSlot`/`PrevSlot` actions).

   API:
   - `setActive(bool)` — toggled when a libretro game starts/stops.
   - `reload()` — re-parse after settings save.
   - Internal `dispatch(action_key)` — routes to the handler.

4. **Action handlers (~23 user-facing rows, all thin wrappers around existing entry points)**:

   | Action key | Default | Handler |
   |---|---|---|
   | `ToggleMenu` | `Keyboard/Escape` | open in-game menu (existing UI path) |
   | `FastForwardToggle` | `Keyboard/Space` | `GameSession::toggleFastForwardLibretro()` |
   | `FastForwardHold` | (unbound) | press→FF on; release→FF off |
   | `Pause` | `Keyboard/P` | toggle `retronest_set_paused` |
   | `Reset` | `Keyboard/H` | `CoreRuntime::reset()` (calls `retro_reset`) |
   | `SaveState` | `Keyboard/F2` | `GameSession::saveStateLibretro(m_currentSaveSlot)` |
   | `LoadState` | `Keyboard/F4` | `GameSession::loadStateLibretro(m_currentSaveSlot)` |
   | `NextSlot` | `Keyboard/F6` | `++m_currentSaveSlot` (max 5); OSD toast `Slot N` via `raInfoToast` |
   | `PrevSlot` | `Keyboard/F7` | `--m_currentSaveSlot` (min 1); OSD toast |
   | `SaveStateSlot1`..`5` | `Keyboard/F2`..`F5`+modifier (e.g. Shift) — exact defaults TBD in plan | `saveStateLibretro(N)` |
   | `LoadStateSlot1`..`5` | parallel | `loadStateLibretro(N)` |
   | `Mute` | `Keyboard/M` | host `AudioSink` mute toggle |
   | `VolumeUp` | `Keyboard/+` | host audio volume +N% |
   | `VolumeDown` | `Keyboard/-` | host audio volume −N% |
   | `Screenshot` | `Keyboard/F8` | existing screenshot path |

   Row breakdown: 9 base actions + 5 SaveStateSlot1..5 + 5 LoadStateSlot1..5 + 4 audio/screenshot = 23 rows total, grouped in the UI as Navigation (2), Speed (2), System (2), Save States (12), Audio (3), Misc (2).

5. **`LibretroHotkeySettingsPage`** — new screen registered in main app settings. Reuses `GenericHotkeyPage` widget with `LibretroHotkeys` as its def list. Persistence target = global `HotkeyBindingStore`.

6. **Per-emulator dialog conditional** — for libretro adapters, omit the hotkey tab. One-line `if (adapter.isLibretro()) skip hotkey tab` in the per-emulator dialog assembly. Standalone adapters keep theirs unchanged.

### Things explicitly NOT in this design

- No new env calls (`RETRONEST_ENVIRONMENT_*`).
- No `INVOKE_HOTKEY` dispatch into cores.
- No per-core action manifest.
- No changes to `EmulatorAdapter::hotkeyBindingDefs()` or its existing callers.
- No new pcsx2-libretro or mgba-libretro code.
- No expansion to multi-port gamepad hotkeys (port 0 only in v1).

## Data flow

**At app start**
1. `HotkeyBindingStore` loads `libretro_hotkeys.json`. If missing, seeded from `LibretroHotkeys` defaults and written.
2. `HotkeyMatcher` parses each binding string once into typed bindings (`Keyboard{keycode}`, `GamepadButton{port,btn}`, `GamepadCombo{port,modifier,btn}`).

**At game start (libretro path only)**
1. `GameSession::startLibretro` calls `HotkeyMatcher::setActive(true)`.
2. `m_currentSaveSlot` reset to 1.

**Each input event / poll cycle**
1. Input router receives keyboard event OR gamepad poll cycle completes.
2. `HotkeyMatcher` updates per-action `pressed` state.
3. For any action whose binding(s) just transitioned `false → true`: dispatch its handler.
4. For hold-style actions (`FastForwardHold`): on `true → false`, fire the release variant.
5. Combo-modifier mask applied: any gamepad button that's a modifier of a currently-matched combo is removed from `RetroPadState` before delivery to the core trampoline. Prevents modifier reaching the game.

**At game end**
1. `HotkeyMatcher::setActive(false)`. Pressed state cleared. Any held FF released.

**On settings save**
1. UI writes JSON via `HotkeyBindingStore::save()`.
2. `HotkeyMatcher::reload()` re-parses bindings. No game restart required.

## Error handling

- **Conflicting bindings**: if two actions are bound to the same key/combo, both fire. UI surfaces a yellow-highlight warning row but doesn't block save. (Matches RetroArch behaviour.)
- **Invalid binding strings in JSON**: ignored, logged once via `qWarning`. Action treated as unbound.
- **Save/load with no game running**: hotkey is a no-op (matcher inactive).
- **Save state before emulator boot-finishes**: handler defers to `GameSession::saveStateLibretro`, which already guards.
- **Mute / volume with audio sink uninitialized**: silent no-op.
- **Modifier-held suppression**: must NOT strip the button from `RetroPadState` if no combo actually matched this frame. Track "matched a combo this frame" — only suppress when so.
- **Single-button gamepad bindings** (no modifier): UI emits a warning at bind time ("this button will not reach the game while bound"); user can confirm.

## Testing

### Unit tests

- **`HotkeyMatcher` press-edge detection**: false→true fires once, held repeats don't refire; release fires hold-style.
- **`HotkeyMatcher` conflict**: two actions on same key both fire.
- **`HotkeyMatcher` combo suppression**: modifier+button fires action AND removes modifier from RetroPad state for that frame; modifier-only press does NOT suppress.
- **`HotkeyBindingStore`**: JSON round-trip, default-seed when file missing, malformed-binding skip.

All pure-C++, no Qt event loop required. Located alongside existing `cpp/tests/` patterns.

### Integration tests

- Programmatically inject key event into `HotkeyMatcher` with a mock adapter; verify expected handler invoked. Use existing `MockAdapter` pattern from `test_libretro_*`.
- Verify standalone-adapter dialogs still render their hotkey page (regression guard).

### Manual smoke tests

Following the project's existing `[[rebuild-before-debugging-regressions]]` discipline — rebuild RetroNest before testing.

- **On R&C 2 (NTSC) under pcsx2-libretro**:
  - F2 saves slot 1; F4 loads slot 1.
  - F6/F7 cycle save slot; OSD toast shows new slot.
  - Save Slot 1..5 / Load Slot 1..5 hotkeys land in the matching slot files.
  - Space toggles FF (existing pill appears).
  - Esc opens in-game menu.
  - P toggles pause (RetroNest's pause-on-menu state).
  - M mutes audio; +/− adjust volume.
  - F8 takes screenshot.
- **Same suite on mGBA**: every action works identically with zero mgba-adapter changes. Proves the libretro-uniform claim.
- **Standalone DuckStation**: per-emulator hotkey page still appears and still works (regression check).

## Open items resolved in implementation plan

- Exact default key for each `SaveStateSlot1..5` / `LoadStateSlot1..5` (RetroArch uses different conventions across versions — pick a coherent default).
- Volume step size (5%? 10%?).
- OSD toast wording for `NextSlot`/`PrevSlot`.
- Whether `HotkeyMatcher` lives in `cpp/src/core/libretro/` or `cpp/src/core/`.
- Exact wiring of the global settings page into the main settings tree.

## Dependencies on existing code

- `GenericHotkeyPage`, `HotkeyBindingRow`, `HotkeyDef`, capture state machine — reused as-is.
- `GameSession::saveStateLibretro(int slot)` / `loadStateLibretro(int slot)` — already exist.
- `GameSession::toggleFastForwardLibretro` — already exists.
- `retronest_set_paused` symbol (shipped 2026-05-19) — exported by libretro cores that opt in; pause hotkey degrades gracefully when symbol absent.
- `CoreRuntime::reset` — calls `retro_reset`, already exists.
- `raInfoToast` — existing OSD-toast surface, used for slot-cycling feedback.
- RetroNest input router (keyboard + gamepad poll path).

## Risks

- **Input poll cadence drift.** Hotkey matcher must observe the same poll cycle the core trampoline uses, otherwise combo-modifier suppression could leak the modifier to the game for one frame. Implementation must hook the matcher BEFORE the trampoline reads from `RetroPadState`, not after.
- **Carbon-synth standalone adapters share `HotkeyDef`-typed storage.** Confirm during implementation that the global libretro store is isolated and cannot accidentally mutate standalone-emulator bindings.
- **Multi-slot UX collision with `.resume` file.** The existing `findResumeFile` path (SP6.5) writes a `.resume` for save-and-exit. Multi-slot hotkey saves write per-slot files. Verify both paths coexist (different filename conventions in `<save_dir>`).
