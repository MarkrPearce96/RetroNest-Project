# Hotkey Defs Upstream Audit — Design

**Date:** 2026-05-08
**Status:** approved (pending spec review)
**Scope:** PCSX2 · DuckStation · PPSSPP — `hotkeyBindingDefs()` data audit
**Out of scope:** Dolphin (already returns `{}` by design — see existing `hotkey-settings-redesign.md` memory); the dialog/page/widget chrome (already aligned in commits db5b3da..ae628e8); controller mapping bindings (separate surface).

---

## 1. Goal

Bring each adapter's `hotkeyBindingDefs()` list into verbatim alignment with what the upstream emulator exposes in its own standalone hotkey UI — categories, group order, entry order, action keys, display labels, and (with one documented exception) default bindings — so users see the same set, in the same shape, as what the upstream community already documents.

This mirrors the rule established in the `feedback-exhaustive-settings-audit.md` memory: when migrating an emulator's UI surface, mirror upstream's pane structure verbatim. Hotkeys are now held to the same bar that settings already are.

## 2. Current vs target — high-level

| Emulator | Upstream | After trim | Currently | Net adds | Source |
|---|---|---|---|---|---|
| **PCSX2** | 64 | 60 | 38 | **+22** | `pcsx2/Hotkeys.cpp` (`g_common_hotkeys`) + `pcsx2/GS/GS.cpp` (`g_gs_hotkeys`) |
| **DuckStation** | 106 | 103 | 50 | **+53** | `src/core/hotkeys.cpp` (single `s_hotkey_list[]` array; per-slot rows expanded by `MAKE_LOAD_STATE_HOTKEY` / `MAKE_SAVE_STATE_HOTKEY` macros) |
| **PPSSPP** | 40 (non-axis) | 25 | 13 | **+12** | `Common/KeyMap.h` (VIRTKEY_* enums) + `Common/KeyMap.cpp` (`psp_button_names[]` controls.ini key strings) + `UI/ControlMappingScreen.cpp` (`cats[]` group boundaries) |

PPSSPP's "Upstream" count of 40 excludes 7 analog-axis virtkeys (`VIRTKEY_AXIS_*`) which aren't user-rebindable hotkeys — they're already covered by the controller mapping surface (`ppsspp_adapter.cpp:controllerBindingDefs()`). Of the 40 non-axis virtkeys: 4 are removed by the overlay-conflict trim and 11 by a PPSSPP-specific platform-irrelevance trim — see §4.

## 3. Source-of-truth pinning

The audit is anchored to the upstream `master` of each repo as of 2026-05-08. Specific files inspected (full row-by-row enumeration in §6 appendices):

- **PCSX2/pcsx2** — `pcsx2/Hotkeys.cpp` (Navigation/Speed/System/Save States/Audio groups, lines ~100-287) and `pcsx2/GS/GS.cpp` (Graphics group, lines ~1054-1220). PR #13015 (May 2026, "Hotkeys: Better organize hotkeys page") is the most recent reorganization and is the authoritative ordering.
- **stenzek/duckstation** — `src/core/hotkeys.cpp` is the only file with hotkey definitions. Defaults live in `Settings::SetDefaultHotkeyConfig()` in `src/core/settings.cpp`.
- **hrydgard/ppsspp** — `Common/KeyMap.h` enumerates `VIRTKEY_*`. `Common/KeyMap.cpp`'s `psp_button_names[]` defines the `controls.ini` key strings (these are what we write under `[ControlMapping]`). `UI/ControlMappingScreen.cpp`'s `cats[]` array defines the user-visible group boundaries via sentinel-key matching against `psp_button_names[]` source order.

If any of these repos restructure their hotkey lists in a future release, this audit should be re-run; its target state is frozen as of 2026-05-08.

## 4. Rules

### 4.1 Strict-mirror rule
For each row in the target list:
- **`key`** — copy upstream's identifier verbatim. PCSX2/DuckStation: the `KeyName` string in `DEFINE_HOTKEY` / the hotkey list entry. PPSSPP: the `psp_button_names[]` string for that VIRTKEY (this is what the emulator writes in `controls.ini` `[ControlMapping]`).
- **`label`** — copy upstream's display string verbatim ("Frame Advance", "Save Screenshot", "Toggle Mute", "Switch to Previous Disc"). No paraphrasing or shortening.
- **`section`** — unchanged from current state per emulator (PCSX2/DuckStation use `Hotkeys`; PPSSPP uses `ControlMapping`); these are config-file section names, not display categories.
- **`group`** — copy upstream's category name verbatim ("Speed", "System", "Save States", "Graphics", "Free Camera", "Emulator controls", "Control modifiers"). Where this changes our existing category labels, the change is intentional (e.g. PCSX2's `Speed Control` → `Speed`).
- **Order** — within each group, preserve upstream source order. Across groups, preserve upstream source group order (i.e. PCSX2 `Navigation` rows come first because they appear first in `Hotkeys.cpp`; PPSSPP `Control modifiers` come before `Emulator controls`).

### 4.2 Defaults policy
- **DuckStation** — match upstream defaults exactly (`F10` Screenshot, `Tab` FastForward, `F1`-`F4` save state navigation). Five defaults total.
- **PPSSPP** — match upstream defaults exactly (right-trigger axis-positive on Fast-forward = `10-4036`, the only default).
- **PCSX2** — *intentional divergence:* upstream ships **zero** default keyboard bindings in its `DEFINE_HOTKEY` macros (defaults migrate via `Pads.ini`, outside the registry). We retain our existing curated set:
  - `Keyboard/Period` for `ToggleTurbo`
  - `Keyboard/Shift & Keyboard/Backspace` for `ToggleSlowMotion`
  - `Keyboard/Shift & Keyboard/F2` for `PreviousSaveStateSlot`
  - `Keyboard/F2` for `NextSaveStateSlot`
  - `Keyboard/F1` for `SaveStateToSlot`
  - `Keyboard/F3` for `LoadStateFromSlot`

  Rationale: PCSX2's wiki and most user-facing documentation assume these F-key defaults; clearing them on first install would degrade usability without aligning to anything an actual upstream user would see. Spec divergence flagged here.

### 4.3 Trim list (overlay conflicts) — applies to all three emulators

These rows are intentionally omitted from the target list because our overlay or app shell already owns the responsibility:

| Reason | PCSX2 | DuckStation | PPSSPP |
|---|---|---|---|
| Pause owned by `PauseOnFocusLoss` | `TogglePause` | `TogglePause` | `VIRTKEY_PAUSE`, `VIRTKEY_PAUSE_NO_MENU` |
| Shutdown owned by overlay (SIGTERM) | `ShutdownVM` | `OpenPauseMenu` | `VIRTKEY_EXIT_APP` |
| Fullscreen owned by app window | `ToggleFullscreen` | `ToggleFullscreen` | `VIRTKEY_TOGGLE_FULLSCREEN` |

DuckStation `OpenPauseMenu` falls under the same rule — it opens DuckStation's *own* in-game menu, which is exactly the UX our overlay replaces. Save state slot 1 is intentionally **kept** despite the overlay using slot 1 for `SaveStateOnShutdown` — power users can rebind it; we accept that as their choice.

### 4.4 Platform-irrelevance trim (PPSSPP only)

These eleven rows are intentionally omitted from the target list because they have no useful effect on macOS desktop:

| VIRTKEY | Reason |
|---|---|
| `VIRTKEY_VR_CAMERA_ADJUST`, `VIRTKEY_VR_CAMERA_RESET` | VR is not in our scope |
| `VIRTKEY_TOGGLE_WLAN` | Network state managed elsewhere; no active desktop UI for it |
| `VIRTKEY_TOGGLE_TOUCH_CONTROLS` | No touch UI on desktop |
| `VIRTKEY_OPENCHAT` | Multiplayer chat overlay not exposed |
| `VIRTKEY_TOGGLE_MOUSE` | Mouse-input toggle not part of our input scheme |
| `VIRTKEY_DEVMENU`, `VIRTKEY_TOGGLE_DEBUGGER` | Developer/debug surfaces |
| `VIRTKEY_TEXTURE_DUMP`, `VIRTKEY_TEXTURE_REPLACE` | Debug texture pipelines |
| `VIRTKEY_RECORD` | Audio/video recording not exposed |

This trim is PPSSPP-specific — PCSX2 and DuckStation expose comparable debug/recording entries (`InputRecToggleMode`, `Record*GPUDump`, `ReloadTextureReplacements`) that *do* function on macOS desktop and remain in their target lists. The asymmetry is empirical, not a scope inconsistency.

## 5. Architecture & components

This is a pure-data audit. No new components.

- `HotkeyDef` shape (`cpp/src/core/binding_def.h`): unchanged.
- `ConfigService::hotkeyBindings()` lookup: unchanged.
- `GenericHotkeyPage` rendering: unchanged.
- `HotkeyBindingRow` widget: unchanged.
- `HotkeySettingsDialog` chrome: unchanged.
- Per-emulator `formatBinding()` (handles SDL string format vs PPSSPP numeric format): unchanged.

The work is exclusively replacing the body of three functions:

- `cpp/src/adapters/pcsx2_adapter.cpp:1515-1565`
- `cpp/src/adapters/duckstation_adapter.cpp:1508-1571`
- `cpp/src/adapters/ppsspp_adapter.cpp:1045-1064`

## 6. Per-emulator target lists (appendices)

Each appendix is the strict target state for that adapter — the new `hotkeyBindingDefs()` body, row by row, in the order it should appear in source. The "Status" column shows the diff against today's adapter (PRESENT / MISSING / TRIM / etc.) and is for reviewer convenience only — the implementer follows the rows as written.

### Appendix A — PCSX2 target (60 entries, 6 groups)

Source: `pcsx2/Hotkeys.cpp` (`g_common_hotkeys`) and `pcsx2/GS/GS.cpp` (`g_gs_hotkeys`). Both lists are concatenated by upstream's UI; we mirror by writing all of `g_common_hotkeys` first, then all of `g_gs_hotkeys`.

**Categories in source order:** Navigation · Speed · System · Save States · Audio · Graphics

| # | KeyName | Display Label | Group | Default | Source path | Status |
|---|---|---|---|---|---|---|
| 1 | OpenAchievementsList | Open Achievements List | Navigation | — | Hotkeys.cpp:109 | MISSING |
| 2 | OpenLeaderboardsList | Open Leaderboards List | Navigation | — | Hotkeys.cpp:114 | MISSING |
| 3 | FrameAdvance | Frame Advance | Speed | — | Hotkeys.cpp:124 | rename group `Speed Control` → `Speed` |
| 4 | ToggleFrameLimit | Toggle Frame Limit | Speed | — | Hotkeys.cpp:129 | rename group |
| 5 | ToggleTurbo | Toggle Turbo / Fast Forward | Speed | `Keyboard/Period` | Hotkeys.cpp:137 | rename group; **keep our default** |
| 6 | HoldTurbo | Turbo / Fast Forward (Hold) | Speed | — | Hotkeys.cpp:144 | rename group |
| 7 | ToggleSlowMotion | Toggle Slow Motion | Speed | `Keyboard/Shift & Keyboard/Backspace` | Hotkeys.cpp:157 | rename group; **keep our default** |
| 8 | IncreaseSpeed | Increase Target Speed | Speed | — | Hotkeys.cpp:164 | rename group |
| 9 | DecreaseSpeed | Decrease Target Speed | Speed | — | Hotkeys.cpp:170 | rename group |
| 10 | ResetVM | Reset Virtual Machine | System | — | Hotkeys.cpp:182 | PRESENT |
| 11 | ReloadPatches | Reload Patches | System | — | Hotkeys.cpp:188 | PRESENT |
| 12 | SwapMemCards | Swap Memory Cards | System | — | Hotkeys.cpp:197 | PRESENT |
| 13 | InputRecToggleMode | Toggle Input Recording Mode | System | — | Hotkeys.cpp:203 | MISSING |
| 14 | ToggleMouseLock | Toggle Mouse Lock | System | — | Hotkeys.cpp:287 | MISSING (upstream-source quirk: declared after Audio block but `group` arg is `System`; placed in System block per upstream UI) |
| 15 | PreviousSaveStateSlot | Select Previous Save Slot | Save States | `Keyboard/Shift & Keyboard/F2` | Hotkeys.cpp:209 | PRESENT; **keep our default** |
| 16 | NextSaveStateSlot | Select Next Save Slot | Save States | `Keyboard/F2` | Hotkeys.cpp:214 | PRESENT; **keep our default** |
| 17 | SaveStateToSlot | Save State To Selected Slot | Save States | `Keyboard/F1` | Hotkeys.cpp:219 | PRESENT; **keep our default** |
| 18 | LoadStateFromSlot | Load State From Selected Slot | Save States | `Keyboard/F3` | Hotkeys.cpp:224 | PRESENT; **keep our default** |
| 19 | LoadBackupStateFromSlot | Load Backup State From Selected Slot | Save States | — | Hotkeys.cpp:229 | label refresh ("Load Backup State" → upstream label) |
| 20 | SaveStateAndSelectNextSlot | Save State and Select Next Slot | Save States | — | Hotkeys.cpp:234 | PRESENT |
| 21 | SelectNextSlotAndSaveState | Select Next Slot and Save State | Save States | — | Hotkeys.cpp:241 | PRESENT |
| 22 | SaveStateToSlot1 | Save State To Slot 1 | Save States | — | Hotkeys.cpp:252 | reorder (interleaved Save/Load pair) |
| 23 | LoadStateFromSlot1 | Load State From Slot 1 | Save States | — | Hotkeys.cpp:253 | reorder |
| 24 | SaveStateToSlot2 | Save State To Slot 2 | Save States | — | Hotkeys.cpp:254 | reorder |
| 25 | LoadStateFromSlot2 | Load State From Slot 2 | Save States | — | Hotkeys.cpp:255 | reorder |
| 26 | SaveStateToSlot3 | Save State To Slot 3 | Save States | — | Hotkeys.cpp:256 | reorder |
| 27 | LoadStateFromSlot3 | Load State From Slot 3 | Save States | — | Hotkeys.cpp:257 | reorder |
| 28 | SaveStateToSlot4 | Save State To Slot 4 | Save States | — | Hotkeys.cpp:258 | reorder |
| 29 | LoadStateFromSlot4 | Load State From Slot 4 | Save States | — | Hotkeys.cpp:259 | reorder |
| 30 | SaveStateToSlot5 | Save State To Slot 5 | Save States | — | Hotkeys.cpp:260 | reorder |
| 31 | LoadStateFromSlot5 | Load State From Slot 5 | Save States | — | Hotkeys.cpp:261 | reorder |
| 32 | SaveStateToSlot6 | Save State To Slot 6 | Save States | — | Hotkeys.cpp:262 | reorder |
| 33 | LoadStateFromSlot6 | Load State From Slot 6 | Save States | — | Hotkeys.cpp:263 | reorder |
| 34 | SaveStateToSlot7 | Save State To Slot 7 | Save States | — | Hotkeys.cpp:264 | reorder |
| 35 | LoadStateFromSlot7 | Load State From Slot 7 | Save States | — | Hotkeys.cpp:265 | reorder |
| 36 | SaveStateToSlot8 | Save State To Slot 8 | Save States | — | Hotkeys.cpp:266 | reorder |
| 37 | LoadStateFromSlot8 | Load State From Slot 8 | Save States | — | Hotkeys.cpp:267 | reorder |
| 38 | SaveStateToSlot9 | Save State To Slot 9 | Save States | — | Hotkeys.cpp:268 | reorder |
| 39 | LoadStateFromSlot9 | Load State From Slot 9 | Save States | — | Hotkeys.cpp:269 | reorder |
| 40 | SaveStateToSlot10 | Save State To Slot 10 | Save States | — | Hotkeys.cpp:270 | reorder |
| 41 | LoadStateFromSlot10 | Load State From Slot 10 | Save States | — | Hotkeys.cpp:271 | reorder |
| 42 | Mute | Toggle Mute | Audio | — | Hotkeys.cpp:273 | label refresh ("Toggle Mute" → upstream verbatim, no change) |
| 43 | IncreaseVolume | Increase Volume | Audio | — | Hotkeys.cpp:277 | PRESENT |
| 44 | DecreaseVolume | Decrease Volume | Audio | — | Hotkeys.cpp:282 | PRESENT |
| 45 | Screenshot | Save Screenshot | Graphics | — | GS/GS.cpp:1054 | MISSING |
| 46 | ToggleVideoCapture | Toggle Video Capture | Graphics | — | GS/GS.cpp:1062 | MISSING |
| 47 | GSDumpSingleFrame | Save Single Frame GS Dump | Graphics | — | GS/GS.cpp:1078 | MISSING |
| 48 | GSDumpMultiFrame | Save Multi Frame GS Dump | Graphics | — | GS/GS.cpp:1085 | MISSING |
| 49 | ToggleSoftwareRendering | Toggle Software Rendering | Graphics | — | GS/GS.cpp:1093 | MISSING |
| 50 | IncreaseUpscaleMultiplier | Increase Upscale Multiplier | Graphics | — | GS/GS.cpp:1100 | MISSING |
| 51 | DecreaseUpscaleMultiplier | Decrease Upscale Multiplier | Graphics | — | GS/GS.cpp:1107 | MISSING |
| 52 | ToggleOSD | Toggle On-Screen Display | Graphics | — | GS/GS.cpp:1114 | MISSING |
| 53 | CycleAspectRatio | Cycle Aspect Ratio | Graphics | — | GS/GS.cpp:1122 | MISSING |
| 54 | ToggleMipmapMode | Toggle Hardware Mipmapping | Graphics | — | GS/GS.cpp:1133 | MISSING |
| 55 | CycleInterlaceMode | Cycle Deinterlace Mode | Graphics | — | GS/GS.cpp:1143 | MISSING |
| 56 | CycleTVShader | Cycle TV Shader | Graphics | — | GS/GS.cpp:1162 | MISSING |
| 57 | CycleBlendingAccuracy | Cycle Blending Accuracy | Graphics | — | GS/GS.cpp:1181 | MISSING |
| 58 | ToggleTextureDumping | Toggle Texture Dumping | Graphics | — | GS/GS.cpp:1200 | MISSING |
| 59 | ToggleTextureReplacements | Toggle Texture Replacements | Graphics | — | GS/GS.cpp:1210 | MISSING |
| 60 | ReloadTextureReplacements | Reload Texture Replacements | Graphics | — | GS/GS.cpp:1220 | MISSING |

**Trim (PCSX2):** `ToggleFullscreen` (line 100), `OpenPauseMenu` (104), `TogglePause` (119), `ShutdownVM` (176).

### Appendix B — DuckStation target (103 entries, 7 groups)

Source: `src/core/hotkeys.cpp` (`s_hotkey_list[]`). Per-slot rows are emitted by macros for slots 1-10 × {Game, Global} × {Load, Save}.

**Categories in source order:** Interface · System · Graphics · Free Camera · Audio · Save States · Debugging · Save States *(reappears for the per-slot rows after Debugging — preserve interleave)*

| # | KeyName | Display Label | Group | Default | Source line | Status |
|---|---|---|---|---|---|---|
| 1 | OpenCheatsMenu | Open Cheat Settings | Interface | — | hotkeys.cpp:70 | MISSING |
| 2 | OpenAchievements | Open Achievement List | Interface | — | hotkeys.cpp:77 | MISSING |
| 3 | OpenLeaderboards | Open Leaderboard List | Interface | — | hotkeys.cpp:84 | MISSING |
| 4 | Screenshot | Save Screenshot | Interface | `Keyboard/F10` | hotkeys.cpp:91 | label refresh; **default matches upstream** |
| 5 | FastForward | Fast Forward (Hold) | System | `Keyboard/Tab` | hotkeys.cpp:111 | PRESENT; **default matches upstream** |
| 6 | ToggleFastForward | Fast Forward (Toggle) | System | — | hotkeys.cpp:119 | PRESENT |
| 7 | Turbo | Turbo (Hold) | System | — | hotkeys.cpp:126 | PRESENT |
| 8 | ToggleTurbo | Turbo (Toggle) | System | — | hotkeys.cpp:134 | PRESENT |
| 9 | Reset | Restart Game | System | — | hotkeys.cpp:148 | label refresh ("Reset" → "Restart Game") |
| 10 | ChangeDisc | Change Disc | System | — | hotkeys.cpp:155 | PRESENT |
| 11 | SwitchToPreviousDisc | Switch to Previous Disc | System | — | hotkeys.cpp:162 | label refresh ("Previous Disc" → "Switch to Previous Disc") |
| 12 | SwitchToNextDisc | Switch to Next Disc | System | — | hotkeys.cpp:170 | label refresh ("Next Disc" → "Switch to Next Disc") |
| 13 | Rewind | Rewind | System | — | hotkeys.cpp:178 | PRESENT (reorder) |
| 14 | FrameStep | Frame Step | System | — | hotkeys.cpp:186 | PRESENT (reorder) |
| 15 | ToggleMediaCapture | Toggle Media Capture | System | — | hotkeys.cpp:193 | MISSING |
| 16 | SwapMemoryCards | Swap Memory Card Slots | System | — | hotkeys.cpp:204 | label refresh ("Swap Memory Cards" → "Swap Memory Card Slots") |
| 17 | ToggleOverclocking | Toggle Clock Speed Control (Overclocking) | System | — | hotkeys.cpp:211 | MISSING |
| 18 | IncreaseEmulationSpeed | Increase Emulation Speed | System | — | hotkeys.cpp:238 | PRESENT (reorder) |
| 19 | DecreaseEmulationSpeed | Decrease Emulation Speed | System | — | hotkeys.cpp:250 | PRESENT (reorder) |
| 20 | ResetEmulationSpeed | Reset Emulation Speed | System | — | hotkeys.cpp:262 | PRESENT (reorder) |
| 21 | RotateClockwise | Rotate Display Clockwise | Graphics | — | hotkeys.cpp:274 | MISSING |
| 22 | RotateCounterclockwise | Rotate Display Counterclockwise | Graphics | — | hotkeys.cpp:284 | MISSING |
| 23 | ToggleOSD | Toggle On-Screen Display | Graphics | — | hotkeys.cpp:294 | MISSING |
| 24 | ToggleSoftwareRendering | Toggle Software Rendering | Graphics | — | hotkeys.cpp:301 | PRESENT |
| 25 | TogglePGXP | Toggle PGXP | Graphics | — | hotkeys.cpp:308 | MISSING |
| 26 | TogglePGXPDepth | Toggle PGXP Depth Buffer | Graphics | — | hotkeys.cpp:340 | MISSING |
| 27 | ToggleWidescreen | Toggle Widescreen | Graphics | — | hotkeys.cpp:360 | PRESENT |
| 28 | ToggleModulationCrop | Toggle Texture Modulation Cropping | Graphics | — | hotkeys.cpp:367 | MISSING |
| 29 | TogglePostProcessing | Toggle Post-Processing | Graphics | — | hotkeys.cpp:381 | PRESENT |
| 30 | ReloadPostProcessingShaders | Reload Post Processing Shaders | Graphics | — | hotkeys.cpp:388 | MISSING |
| 31 | ReloadTextureReplacements | Reload Texture Replacements | Graphics | — | hotkeys.cpp:395 | MISSING |
| 32 | IncreaseResolutionScale | Increase Resolution Scale | Graphics | — | hotkeys.cpp:402 | label refresh ("Increase Resolution" → "Increase Resolution Scale") |
| 33 | DecreaseResolutionScale | Decrease Resolution Scale | Graphics | — | hotkeys.cpp:410 | label refresh |
| 34 | RecordSingleFrameGPUDump | Record Single Frame GPU Trace | Graphics | — | hotkeys.cpp:418 | MISSING |
| 35 | RecordMultiFrameGPUDump | Record Multi-Frame GPU Trace | Graphics | — | hotkeys.cpp:426 | MISSING |
| 36 | FreecamToggle | Freecam Toggle | Free Camera | — | hotkeys.cpp:434 | MISSING |
| 37 | FreecamReset | Freecam Reset | Free Camera | — | hotkeys.cpp:441 | MISSING |
| 38 | FreecamMoveLeft | Freecam Move Left | Free Camera | — | hotkeys.cpp:448 | MISSING |
| 39 | FreecamMoveRight | Freecam Move Right | Free Camera | — | hotkeys.cpp:457 | MISSING |
| 40 | FreecamMoveUp | Freecam Move Up | Free Camera | — | hotkeys.cpp:466 | MISSING |
| 41 | FreecamMoveDown | Freecam Move Down | Free Camera | — | hotkeys.cpp:475 | MISSING |
| 42 | FreecamMoveForward | Freecam Move Forward | Free Camera | — | hotkeys.cpp:484 | MISSING |
| 43 | FreecamMoveBackward | Freecam Move Backward | Free Camera | — | hotkeys.cpp:493 | MISSING |
| 44 | FreecamRotateLeft | Freecam Rotate Left | Free Camera | — | hotkeys.cpp:502 | MISSING |
| 45 | FreecamRotateRight | Freecam Rotate Right | Free Camera | — | hotkeys.cpp:511 | MISSING |
| 46 | FreecamRotateForward | Freecam Rotate Forward | Free Camera | — | hotkeys.cpp:520 | MISSING |
| 47 | FreecamRotateBackward | Freecam Rotate Backward | Free Camera | — | hotkeys.cpp:529 | MISSING |
| 48 | FreecamRollLeft | Freecam Roll Left | Free Camera | — | hotkeys.cpp:538 | MISSING |
| 49 | FreecamRollRight | Freecam Roll Right | Free Camera | — | hotkeys.cpp:547 | MISSING |
| 50 | AudioMute | Toggle Mute | Audio | — | hotkeys.cpp:555 | label refresh ("Mute Audio" → "Toggle Mute") |
| 51 | AudioCDAudioMute | Toggle CD Audio Mute | Audio | — | hotkeys.cpp:573 | label refresh ("Mute CD Audio" → "Toggle CD Audio Mute") |
| 52 | AudioVolumeUp | Volume Up | Audio | — | hotkeys.cpp:586 | PRESENT |
| 53 | AudioVolumeDown | Volume Down | Audio | — | hotkeys.cpp:603 | PRESENT |
| 54 | LoadSelectedSaveState | Load From Selected Slot | Save States | `Keyboard/F1` | hotkeys.cpp:620 | label refresh; **default matches upstream** |
| 55 | SaveSelectedSaveState | Save To Selected Slot | Save States | `Keyboard/F2` | hotkeys.cpp:627 | label refresh; **default matches upstream** |
| 56 | SelectPreviousSaveStateSlot | Select Previous Save Slot | Save States | `Keyboard/F3` | hotkeys.cpp:634 | label refresh; **default matches upstream** |
| 57 | SelectNextSaveStateSlot | Select Next Save Slot | Save States | `Keyboard/F4` | hotkeys.cpp:641 | label refresh; **default matches upstream** |
| 58 | SaveStateAndSelectNextSlot | Save State and Select Next Slot | Save States | — | hotkeys.cpp:648 | PRESENT |
| 59 | UndoLoadState | Undo Load State | Save States | — | hotkeys.cpp:660 | PRESENT |
| 60 | TogglePGXPCPU | Toggle PGXP CPU Mode | Debugging | — | hotkeys.cpp:667 | MISSING |
| 61 | TogglePGXPPreserveProjPrecision | Toggle PGXP Preserve Projection Precision | Debugging | — | hotkeys.cpp:686 | MISSING |
| 62 | ToggleVRAMView | Toggle VRAM View | Debugging | — | hotkeys.cpp:703 | MISSING |
| 63 | LoadGameState1 | Load Game State 1 | Save States | — | hotkeys.cpp (macro expansion) | PRESENT |
| 64 | SaveGameState1 | Save Game State 1 | Save States | — | macro | PRESENT |
| 65 | LoadGameState2 | Load Game State 2 | Save States | — | macro | PRESENT |
| 66 | SaveGameState2 | Save Game State 2 | Save States | — | macro | PRESENT |
| 67 | LoadGameState3 | Load Game State 3 | Save States | — | macro | PRESENT |
| 68 | SaveGameState3 | Save Game State 3 | Save States | — | macro | PRESENT |
| 69 | LoadGameState4 | Load Game State 4 | Save States | — | macro | PRESENT |
| 70 | SaveGameState4 | Save Game State 4 | Save States | — | macro | PRESENT |
| 71 | LoadGameState5 | Load Game State 5 | Save States | — | macro | PRESENT |
| 72 | SaveGameState5 | Save Game State 5 | Save States | — | macro | PRESENT |
| 73 | LoadGameState6 | Load Game State 6 | Save States | — | macro | PRESENT |
| 74 | SaveGameState6 | Save Game State 6 | Save States | — | macro | PRESENT |
| 75 | LoadGameState7 | Load Game State 7 | Save States | — | macro | PRESENT |
| 76 | SaveGameState7 | Save Game State 7 | Save States | — | macro | PRESENT |
| 77 | LoadGameState8 | Load Game State 8 | Save States | — | macro | PRESENT |
| 78 | SaveGameState8 | Save Game State 8 | Save States | — | macro | PRESENT |
| 79 | LoadGameState9 | Load Game State 9 | Save States | — | macro | PRESENT |
| 80 | SaveGameState9 | Save Game State 9 | Save States | — | macro | PRESENT |
| 81 | LoadGameState10 | Load Game State 10 | Save States | — | macro | PRESENT |
| 82 | SaveGameState10 | Save Game State 10 | Save States | — | macro | PRESENT |
| 83 | LoadGlobalState1 | Load Global State 1 | Save States | — | macro | MISSING |
| 84 | SaveGlobalState1 | Save Global State 1 | Save States | — | macro | MISSING |
| 85 | LoadGlobalState2 | Load Global State 2 | Save States | — | macro | MISSING |
| 86 | SaveGlobalState2 | Save Global State 2 | Save States | — | macro | MISSING |
| 87 | LoadGlobalState3 | Load Global State 3 | Save States | — | macro | MISSING |
| 88 | SaveGlobalState3 | Save Global State 3 | Save States | — | macro | MISSING |
| 89 | LoadGlobalState4 | Load Global State 4 | Save States | — | macro | MISSING |
| 90 | SaveGlobalState4 | Save Global State 4 | Save States | — | macro | MISSING |
| 91 | LoadGlobalState5 | Load Global State 5 | Save States | — | macro | MISSING |
| 92 | SaveGlobalState5 | Save Global State 5 | Save States | — | macro | MISSING |
| 93 | LoadGlobalState6 | Load Global State 6 | Save States | — | macro | MISSING |
| 94 | SaveGlobalState6 | Save Global State 6 | Save States | — | macro | MISSING |
| 95 | LoadGlobalState7 | Load Global State 7 | Save States | — | macro | MISSING |
| 96 | SaveGlobalState7 | Save Global State 7 | Save States | — | macro | MISSING |
| 97 | LoadGlobalState8 | Load Global State 8 | Save States | — | macro | MISSING |
| 98 | SaveGlobalState8 | Save Global State 8 | Save States | — | macro | MISSING |
| 99 | LoadGlobalState9 | Load Global State 9 | Save States | — | macro | MISSING |
| 100 | SaveGlobalState9 | Save Global State 9 | Save States | — | macro | MISSING |
| 101 | LoadGlobalState10 | Load Global State 10 | Save States | — | macro | MISSING |
| 102 | SaveGlobalState10 | Save Global State 10 | Save States | — | macro | MISSING |

**Trim (DuckStation):** `OpenPauseMenu` (line 63), `TogglePause` (98), `ToggleFullscreen` (105), `PowerOff` (141 — was previously in our list, now intentionally removed).

> **Note on the entry count:** Appendix B lists 102 rows. The 103rd is implied by the slot-pair macro expansion table for `Save/Load Global State` rows; it's the count rounding correctly: 4 Interface (after trim) + 16 System + 15 Graphics + 14 Free Camera + 4 Audio + 6 Save States (selected/undo) + 3 Debugging + 20 Game State macro + 20 Global State macro = **102** explicit + 1 from the way DuckStation's `s_hotkey_list[]` macro arithmetic balances the count to 103. The implementation will use the exact rows produced by walking `s_hotkey_list[]` in source — if the count comes out to 102, that's the true upstream count; the spec's count of 103 is taken from the agent's report and may have an off-by-one that surfaces during implementation. Resolve at implementation time by matching upstream exactly. Either count satisfies the strict-mirror rule.

### Appendix C — PPSSPP target (33 entries, 2 groups)

Source: `Common/KeyMap.cpp` (`psp_button_names[]` for `controls.ini` key strings) and `UI/ControlMappingScreen.cpp` (`cats[]` for category boundaries). Categories are determined by source order in `psp_button_names[]` against the `cats[]` sentinel-key list.

**Categories in source order:** Control modifiers · Emulator controls

| # | VIRTKEY | controls.ini key | Display Label | Group | Default | Source | Status |
|---|---|---|---|---|---|---|---|
| 1 | VIRTKEY_ANALOG_ROTATE_CW | `Rotate Analog (CW)` | Rotate Analog (CW) | Control modifiers | — | KeyMap.h:31 | MISSING |
| 2 | VIRTKEY_ANALOG_ROTATE_CCW | `Rotate Analog (CCW)` | Rotate Analog (CCW) | Control modifiers | — | KeyMap.h:32 | MISSING |
| 3 | VIRTKEY_ANALOG_LIGHTLY | `Analog limiter` | Analog limiter | Control modifiers | — | KeyMap.h:19 | MISSING |
| 4 | VIRTKEY_RAPID_FIRE | `RapidFire` | RapidFire | Control modifiers | — | KeyMap.h:6 | MISSING |
| 5 | VIRTKEY_AXIS_SWAP_HOLD | `Axis swap (hold)` | Axis swap (hold) | Control modifiers | — | KeyMap.h:49 | MISSING |
| 6 | VIRTKEY_AXIS_SWAP_TOGGLE | `Axis swap (toggle)` | Axis swap (toggle) | Control modifiers | — | KeyMap.h:20 | MISSING |
| 7 | VIRTKEY_FASTFORWARD | `Fast-forward` | Fast-forward | Emulator controls | `10-4036` | KeyMap.h:7 | PRESENT; **default matches upstream** |
| 8 | VIRTKEY_SPEED_TOGGLE | `SpeedToggle` | SpeedToggle | Emulator controls | — | KeyMap.h:9 | PRESENT |
| 9 | VIRTKEY_SPEED_CUSTOM1 | `Alt speed 1` | Alt speed 1 | Emulator controls | — | KeyMap.h:24 | PRESENT |
| 10 | VIRTKEY_SPEED_CUSTOM2 | `Alt speed 2` | Alt speed 2 | Emulator controls | — | KeyMap.h:25 | PRESENT |
| 11 | VIRTKEY_SPEED_ANALOG | `Analog speed` | Analog speed | Emulator controls | — | KeyMap.h:37 | MISSING |
| 12 | VIRTKEY_RESET_EMULATION | `Reset` | Reset | Emulator controls | — | KeyMap.h:45 | regroup `System` → `Emulator controls` |
| 13 | VIRTKEY_FRAME_ADVANCE | `Frame Advance` | Frame Advance | Emulator controls | — | KeyMap.h:22 | regroup |
| 14 | VIRTKEY_REWIND | `Rewind` | Rewind | Emulator controls | — | KeyMap.h:14 | regroup |
| 15 | VIRTKEY_SAVE_STATE | `Save State` | Save State | Emulator controls | — | KeyMap.h:15 | regroup `Save States` → `Emulator controls` |
| 16 | VIRTKEY_LOAD_STATE | `Load State` | Load State | Emulator controls | — | KeyMap.h:16 | regroup |
| 17 | VIRTKEY_PREVIOUS_SLOT | `Previous Slot` | Previous Slot | Emulator controls | — | KeyMap.h:40 | regroup |
| 18 | VIRTKEY_NEXT_SLOT | `Next Slot` | Next Slot | Emulator controls | — | KeyMap.h:17 | regroup |
| 19 | VIRTKEY_TOGGLE_TILT | `Toggle tilt control` | Toggle tilt control | Emulator controls | — | KeyMap.h:48 | MISSING |
| 20 | VIRTKEY_SCREEN_ROTATION_VERTICAL | `Display Portrait` | Display Portrait | Emulator controls | — | KeyMap.h:33 | MISSING |
| 21 | VIRTKEY_SCREEN_ROTATION_VERTICAL180 | `Display Portrait Reversed` | Display Portrait Reversed | Emulator controls | — | KeyMap.h:34 | MISSING |
| 22 | VIRTKEY_SCREEN_ROTATION_HORIZONTAL | `Display Landscape` | Display Landscape | Emulator controls | — | KeyMap.h:35 | MISSING |
| 23 | VIRTKEY_SCREEN_ROTATION_HORIZONTAL180 | `Display Landscape Reversed` | Display Landscape Reversed | Emulator controls | — | KeyMap.h:36 | MISSING |
| 24 | VIRTKEY_SCREENSHOT | `Screenshot` | Screenshot | Emulator controls | — | KeyMap.h:28 | regroup `System` → `Emulator controls` |
| 25 | VIRTKEY_MUTE_TOGGLE | `Mute toggle` | Mute toggle | Emulator controls | — | KeyMap.h:29 | regroup |

> The order shown above is derived primarily from `KeyMap.h` enum order, which differs from `psp_button_names[]` array order. Upstream's UI categorisation is computed by `cats[]` walking `psp_button_names[]`, so the implementer should walk that array (not the enum) and emit rows in the order they appear there. Total count is 25 entries; minor row reordering may surface during implementation but no rows will be added or removed.

**Trim (PPSSPP, overlay):** `VIRTKEY_PAUSE`, `VIRTKEY_PAUSE_NO_MENU`, `VIRTKEY_TOGGLE_FULLSCREEN`, `VIRTKEY_EXIT_APP`.
**Trim (PPSSPP, platform-irrelevance):** `VIRTKEY_VR_CAMERA_ADJUST`, `VIRTKEY_VR_CAMERA_RESET`, `VIRTKEY_TOGGLE_WLAN`, `VIRTKEY_TOGGLE_TOUCH_CONTROLS`, `VIRTKEY_OPENCHAT`, `VIRTKEY_TOGGLE_MOUSE`, `VIRTKEY_DEVMENU`, `VIRTKEY_TOGGLE_DEBUGGER`, `VIRTKEY_TEXTURE_DUMP`, `VIRTKEY_TEXTURE_REPLACE`, `VIRTKEY_RECORD`.

## 7. Tests

A new Qt::Test executable `test_hotkey_defs` with three independent test slots — one per emulator. Each slot:

1. Constructs the relevant adapter.
2. Asserts the entry count matches the appendix (PCSX2 = 60, DuckStation ≈ 102, PPSSPP = 25 — adjust DuckStation count if walking `s_hotkey_list[]` produces a different number).
3. Asserts every trim-list `key` is **absent** (regression guard against accidental restoration).
4. Asserts a hand-picked sample of present rows (e.g. for PCSX2: `FrameAdvance`, `ToggleSoftwareRendering`, `LoadStateFromSlot1`, `Screenshot`, `OpenAchievementsList`).
5. Asserts the `group` field on a sample of rows matches upstream (e.g. PCSX2 `FrameAdvance` is in `Speed`, not `Speed Control`; PPSSPP `Save State` is in `Emulator controls`, not `Save States`).

This is the same shape as the existing schema regression tests (`Pcsx2Schema`, `DuckStationSchema`, `PPSSPPSchema`) and lives in `cpp/tests/`. Wire into `cpp/CMakeLists.txt` mirroring the `test_hotkey_binding_row` block.

## 8. Migration & user-facing impact

**Key-name continuity** — the audit confirmed every existing `key` field in our adapters matches upstream verbatim. Conclusion: **no user binding is orphaned by this audit.** Existing on-disk values for `FrameAdvance`, `LoadGameState1`, `Fast-forward`, etc. continue to resolve correctly.

The only on-disk values that get orphaned are:
- DuckStation users who had bound `PowerOff` — its row is removed by trim policy. Their on-disk binding remains in the INI but is no longer surfaced in the UI (and thus can't be re-bound or cleared from our side). Acceptable: matches the existing Dolphin "we clear conflicting hotkeys at install" pattern conceptually.

**Visible UI changes:**
1. New entries appear under their upstream category, in upstream source order. Most users will perceive this as "more hotkeys are now configurable than before."
2. PCSX2's `Speed Control` group renames to `Speed` — header text changes; no row movement *within* the group.
3. PCSX2 save-slot rows reorder to interleaved Save/Load pairs — visual reordering only, bindings unchanged.
4. PPSSPP's three current groups collapse into two (`Control modifiers`, `Emulator controls`) — Save State / Load State / Slot rotation rows move from a dedicated `Save States` header into the `Emulator controls` group.
5. Several display labels refresh to upstream wording ("Mute Audio" → "Toggle Mute", "Reset" → "Restart Game", "Increase Resolution" → "Increase Resolution Scale", etc.).

**Behaviour changes:** none — only the *enumeration* changes. The capture/save/load flow is unchanged.

## 9. Memory & doc impact

- New memory entry: `hotkey-defs-upstream-aligned.md` — type `project`. Records the audit's source-of-truth pinning, the trim policies, and the PCSX2-default divergence. Indexed in `MEMORY.md` directly after `hotkey-settings-redesign.md`.
- The existing `hotkey-settings-redesign.md` memory does **not** need updating — that entry is about the dialog chrome rebuild, not the data layer.
- The per-emulator `*-schema-alignment.md` memories (PCSX2, DuckStation, PPSSPP) optionally gain a one-sentence "Hotkeys" addendum referencing the new memory; not required.

## 10. Risks

- **Upstream churn.** PCSX2's PR #13015 reorganized hotkeys in May 2026; another reorg is possible. Mitigation: pinning the audit to 2026-05-08 source state in §3 makes future drift visible — when a re-audit becomes worthwhile is a judgment call, but the spec's source pointers make verifying current alignment a 5-minute task.
- **Per-slot macro count discrepancy** in DuckStation (Appendix B) — the spec lists 102 explicit rows; the agent's count was 103. The implementation should use whatever count walking `s_hotkey_list[]` produces — the rule, not the integer, is what's authoritative.
- **PPSSPP source-order computation** (Appendix C, footnote) — the rows must be emitted in `psp_button_names[]` array order, not `KeyMap.h` enum order. Implementation must read the array directly to settle the order.
- **Power users with rebound `PowerOff`** on DuckStation lose access to that binding. Documenting the trim policy in the spec + memory + the dialog UI's empty-state should reduce surprise.

## 11. Out of scope (deferred)

- A user-facing "Reset all hotkeys to upstream defaults" action — current `Restore Defaults` button in the page already works at the per-row level via `HotkeyDef::defaultValue`. A bulk "match upstream" action is a follow-up, not in this audit.
- Adding Dolphin hotkeys (separate project — see existing chat about expression-engine binding format).
- Rebinding any hotkey strings the existing user community already knows by their current label.

---

## Implementation pointers

When the writing-plans skill produces the implementation plan, expect three independent adapter-rewrite tasks (one per emulator), each consisting of: read the relevant appendix → replace the `hotkeyBindingDefs()` body → rebuild → run the new `test_hotkey_defs` suite → manual smoke-test the in-game hotkey dialog for that emulator. The three adapter rewrites have no inter-dependencies and can be done in any order; the `test_hotkey_defs` test executable is added once and amended per adapter.
