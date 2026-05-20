# `hotkeyVirtualKeyCode()` — base class default

**Date:** 2026-05-20
**Status:** Approved (brainstorming)
**Roadmap item:** Tier 2 #9 (`refactor-roadmap.md`)

## Problem

The Carbon `kVK_*` virtual keycodes that AppController synthesizes for in-game-menu actions (Pause / SaveState / LoadState / ToggleFastForward) are declared by every external-process adapter via `hotkeyVirtualKeyCode(HotkeyAction)`. Looking at what each adapter actually returns:

| Adapter | TogglePause | SaveState | LoadState | ToggleFastForward |
|--------|------|------|------|------|
| Base (`emulator_adapter.h:393`) | 0 | 0 | 0 | 0 |
| DuckStation (`duckstation_adapter.h:40`) | Space | F5 | F7 | F8 |
| PPSSPP (`ppsspp_adapter.h:41`) | Space | F5 | F7 | 0 |
| Dolphin (`dolphin_adapter.h:85`) | Space | 0 | 0 | 0 |
| Libretro adapters | (inherit 0) | — | — | — |

`Space / F5 / F7 / F8` is the standard pattern. DuckStation matches it exactly; PPSSPP diverges only on ToggleFastForward (PPSSPP's FF is hold-style with no clean toggle hotkey); Dolphin diverges on three (save/load/FF are intentionally disabled — Save/Load because Dolphin's UTI claims route LS opens around our portable-mode mechanism; FF because it isn't wired). The duplication carries no information about what each adapter is doing differently — only DuckStation contributes the canonical default; the others would be more legible if they declared only their opt-outs.

## Goal

Move the `Space / F5 / F7 / F8` default into the `EmulatorAdapter` base class. Each subclass overrides only the actions it disables (returning 0) or remaps (returning a different `kVK_*`).

Outcome:

- `DuckStation`'s override is removed entirely (it matched the default).
- `PPSSPP`'s override shrinks to one disabled action (`ToggleFastForward`) plus a `return EmulatorAdapter::hotkeyVirtualKeyCode(action)` fall-through for the rest.
- `Dolphin`'s override stays roughly the same size but its `case` arms now read as "explicitly disabled", and TogglePause falls through to base.

Adding a new external-process adapter no longer requires writing the Space/F5/F7/F8 boilerplate — only opt-outs.

## Non-goals

- No change to the libretro adapters. They inherit the new default's return values but the libretro pause/save/load paths (`s->saveStateLibretro`, `s->toggleFastForwardLibretro`) bypass `synthesizeHotkey()` entirely (every call site in `app_controller.cpp` is gated by `if (m_inGameMenu->currentBackendIsLibretro()) { ... return; }`), and `supportsSaveState`-style flags use `isLibretro || synthSave` so libretro never reads these return values. Confirmed safe — verified by reading `app_controller.cpp:895-916` and `:1124-1170`.
- No change to `pauseHotkeyVirtualKeyCode()` (the back-compat alias) or any caller of `hotkeyVirtualKeyCode()`.
- No change to behavior, only the source-of-truth location.

## Component changes

### `cpp/src/adapters/emulator_adapter.h:393` — base default

Replace:

```cpp
virtual int hotkeyVirtualKeyCode(HotkeyAction action) const {
    Q_UNUSED(action);
    return 0;
}
```

with:

```cpp
/**
 * Default kVK_* codes that match the standard external-emulator
 * convention (Space / F5 / F7 / F8). The corresponding emulator
 * hotkey must be bound to this key in createDefaultConfig /
 * patchExistingConfig — otherwise the synthesized keystroke
 * reaches the emulator but does nothing.
 *
 * Adapters override only to disable an action (return 0) or
 * remap one. New external-process adapters get the standard
 * synthesis behavior for free.
 */
virtual int hotkeyVirtualKeyCode(HotkeyAction action) const {
    switch (action) {
    case HotkeyAction::TogglePause:       return 0x31; // kVK_Space
    case HotkeyAction::SaveState:         return 0x60; // kVK_F5
    case HotkeyAction::LoadState:         return 0x62; // kVK_F7
    case HotkeyAction::ToggleFastForward: return 0x64; // kVK_F8
    }
    return 0;
}
```

### `cpp/src/adapters/duckstation_adapter.h:37-48` — remove override

Delete lines 37–48 entirely (the comment and the override). DuckStation now inherits the base default, which matches its previous behavior exactly. The "force-bound to F5/F7/F8 and removed from `hotkeyBindingDefs()`" note is preserved on the base class doc-comment in a generalized form.

### `cpp/src/adapters/ppsspp_adapter.h:37-49` — shrink override to one opt-out

Replace lines 37–49 with:

```cpp
// PPSSPP fast-forward is hold-style with no clean toggle hotkey,
// so we return 0 for ToggleFastForward — the in-game menu hides
// the FF action. Save/Load/Pause use the standard base defaults
// (Space / F5 / F7), bound in controls.ini.
int hotkeyVirtualKeyCode(HotkeyAction action) const override {
    if (action == HotkeyAction::ToggleFastForward) return 0;
    return EmulatorAdapter::hotkeyVirtualKeyCode(action);
}
```

### `cpp/src/adapters/dolphin_adapter.h:78-93` — keep override, defer to base for one action

Replace lines 78–93 with:

```cpp
// Dolphin save/load state are disabled (returning 0 hides the
// icons from the in-game menu via currentGameInfo's
// supportsSaveState/supportsLoadState flags). Fast-forward isn't
// wired. Pause uses the standard base default (Space) — works
// because PauseOnFocusLost handles focus changes too; the
// synthesized Space is a no-op for Dolphin but harmless.
int hotkeyVirtualKeyCode(HotkeyAction action) const override {
    switch (action) {
    case HotkeyAction::SaveState:         return 0;
    case HotkeyAction::LoadState:         return 0;
    case HotkeyAction::ToggleFastForward: return 0;
    default:                              return EmulatorAdapter::hotkeyVirtualKeyCode(action);
    }
}
```

(`default` covers `TogglePause` and any future `HotkeyAction` values added; matches the spirit of "only disable what we know to disable".)

## Estimated LOC impact

| File | Delta |
|------|-------|
| `emulator_adapter.h` | +~8 lines (switch with 4 cases + comment) |
| `duckstation_adapter.h` | −12 lines (override deleted) |
| `ppsspp_adapter.h` | −9 lines (shrinks to 2-line body + comment) |
| `dolphin_adapter.h` | roughly even (~10 lines, same shape but defers TogglePause to base) |
| **Net** | **≈ −13 LOC** |

## Smoke test

Build, deploy, sign, launch x86_64 (per `build-cmake-needs-macdeployqt` memory). Exercise:

1. **DuckStation** (the adapter that loses its override entirely — highest-risk smoke test):
   1. Launch a PS1 game via DuckStation.
   2. Cmd+Shift+Escape → in-game menu appears, game pauses. Resume → game resumes (Space synthesized via base default).
   3. Save State button → DuckStation saves to the current slot (F5 synthesized via base default).
   4. Load State button → DuckStation loads (F7 synthesized via base default).
   5. Fast Forward toggle → speed doubles (F8 synthesized via base default).
2. **PPSSPP** spot-check:
   1. Launch a PSP game; verify Pause/Save/Load still work; verify the Fast Forward action is hidden from the in-game menu (this gates on the 0 return for FF).
3. **Dolphin** spot-check:
   1. Launch a GameCube/Wii game; verify Pause works (Space synthesized via inherited default); verify Save State / Load State / Fast Forward are hidden from the in-game menu.

If any of these regress, STOP and investigate before proceeding.

## Out of scope

- C++ unit tests covering `hotkeyVirtualKeyCode()`. No existing tests reference this method directly; behavior is exercised end-to-end via the in-game menu only. Adding a unit test now isn't warranted — the smoke test covers the only non-trivial integration.
- Renaming `hotkeyVirtualKeyCode` or restructuring the `HotkeyAction` enum.
- Touching `pauseHotkeyVirtualKeyCode()` back-compat alias.

## Follow-ups (logged on the roadmap, not part of this work)

None new from this refactor — it's a pure cleanup with no surfaced design questions.
