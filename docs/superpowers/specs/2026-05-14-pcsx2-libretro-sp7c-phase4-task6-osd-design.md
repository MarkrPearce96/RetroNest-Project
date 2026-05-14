# SP7c Phase 4 Task 6 — On-Screen Display sub-tab (design)

**Date:** 2026-05-14
**Sub-project:** SP7c (PCSX2 libretro core-options parity, Phase 4: Graphics)
**Task:** 6 of 6 in Phase 4 — **closes Phase 4**
**Scope:** Expose the 23 OSD knobs from PCSX2's `Graphics → On-Screen Display` sub-tab as libretro core options, plus matching host rows in RetroNest's PCSX2 per-emulator settings dialog.

Predecessor context lives in `[[sp7c-kickoff]]` and `[[phase4-task6-prep]]` (auto-memory). This spec supersedes both for Task 6 execution.

## Goals

1. Add 23 OSD core options to `pcsx2-libretro` (one struct, definitions, parse branches, `ApplyDefaults` writes, per-launch echo, two `test_core_options` cases).
2. Add 23 corresponding host rows to RetroNest's PCSX2 Graphics card under a new `"On-Screen Display"` sub-tab with 5 groups and the dependsOn fan-out specified below.
3. After both commits, `check_schema_fidelity` reports **89 / 89** and `test_core_options` reports **0 failures across 42 cases**.
4. Live-smoke gate (user-driven, 12 steps) passes.
5. Phase 4 closes; SP7c kickoff memory + phase4-task6-prep memory updated accordingly.

## Non-goals

- No Phase 5 work (cross-listing `pcsx2_renderer`, dialog-level `masterValues` promotion).
- No fix to the standalone PCSX2 adapter's incorrect `OsdBoldText default="true"` — filed as a separate SP7c followup, **not Task 6 scope**.
- No new core option not currently in the standalone OSD sub-tab. `OsdShowGPUDebug` (Config.h:775) exists in the bitfield but isn't exposed by the standalone adapter — it stays out of our 23.

## Inventory (23 knobs)

All knobs verified against PCSX2 source at pickup time (2026-05-14, `retronest-libretro` HEAD `c64835e08`):
- Enum: `pcsx2/Config.h:341-353` (`OsdOverlayPos`: `None=0, TopLeft=1, TopCenter=2, TopRight=3, CenterLeft=4, Center=5, CenterRight=6, BottomLeft=7, BottomCenter=8, BottomRight=9`).
- DEFAULT_ constants: `Config.h:730-733`.
- Bitfield: `Config.h:768-786`.
- Init: `Pcsx2Config.cpp:720-745` (GS bitfields) + `Pcsx2Config.cpp:1943` (EmuCore `WarnAboutUnsafeSettings`).
- Standalone adapter: `RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp:797-898`.

### Group: On-Screen Display (5 rows, no dependsOn)

| Core key | INI section / key | Type | Default | Source |
|---|---|---|---|---|
| `pcsx2_osd_scale` | `EmuCore/GS / OsdScale` | Combo (8-stop slider→combo) | `100` | `Config.h:730 DEFAULT_OSD_SCALE = 100.0f` |
| `pcsx2_osd_margin` | `EmuCore/GS / OsdMargin` | Combo (8-stop slider→combo) | `10` | `Config.h:731 DEFAULT_OSD_MARGIN = 10.0f` |
| `pcsx2_osd_messages_pos` | `EmuCore/GS / OsdMessagesPos` | Combo (10-stop enum) | `1` (TopLeft) | `Config.h:732 DEFAULT_OSD_MESSAGE_POS = TopLeft` |
| `pcsx2_osd_performance_pos` | `EmuCore/GS / OsdPerformancePos` | Combo (10-stop enum) | `3` (TopRight) | `Config.h:733 DEFAULT_OSD_PERFORMANCE_POS = TopRight` |
| `pcsx2_osd_bold_text` | `EmuCore/GS / OsdBoldText` | Bool | **`false`** | bitfield zero-init (no line in `Defaults()` body lines 720-745) |

### Group: Performance Stats (9 rows, all `dependsOn = "pcsx2_osd_performance_pos!=0"`)

All `EmuCore/GS`, Bool. Defaults from `Pcsx2Config.cpp:728-737`.

| Core key | INI key | Default |
|---|---|---|
| `pcsx2_osd_show_speed` | `OsdShowSpeed` | `false` |
| `pcsx2_osd_show_fps` | `OsdShowFPS` | `false` |
| `pcsx2_osd_show_vps` | `OsdShowVPS` | `false` |
| `pcsx2_osd_show_resolution` | `OsdShowResolution` | `false` |
| `pcsx2_osd_show_gs_stats` | `OsdShowGSStats` | `false` |
| `pcsx2_osd_show_cpu` | `OsdShowCPU` | `false` |
| `pcsx2_osd_show_gpu` | `OsdShowGPU` | `false` |
| `pcsx2_osd_show_indicators` | `OsdShowIndicators` | **`true`** ← only non-false in this group |
| `pcsx2_osd_show_frame_times` | `OsdShowFrameTimes` | `false` |

### Group: System Information (2 rows, both `dependsOn = "pcsx2_osd_performance_pos!=0"`)

All `EmuCore/GS`, Bool, default `false`.

| Core key | INI key |
|---|---|
| `pcsx2_osd_show_hardware_info` | `OsdShowHardwareInfo` |
| `pcsx2_osd_show_version` | `OsdShowVersion` |

### Group: Settings & Inputs (6 rows, mixed dependsOn)

All `EmuCore/GS`, Bool.

| Core key | INI key | Default | dependsOn |
|---|---|---|---|
| `pcsx2_osd_show_settings` | `OsdShowSettings` | `false` | `pcsx2_osd_messages_pos!=0` |
| `pcsx2_osd_show_patches` | **`OsdshowPatches`** (PCSX2 INI typo — lowercase 's') | `false` | `pcsx2_osd_show_settings` (master-bool chain) |
| `pcsx2_osd_show_inputs` | `OsdShowInputs` | `false` | (none) |
| `pcsx2_osd_show_video_capture` | `OsdShowVideoCapture` | **`true`** | (none) — libretro-inert; kept for parity (D-Task6-3 Option A) |
| `pcsx2_osd_show_input_rec` | `OsdShowInputRec` | **`true`** | (none) — libretro-inert; kept for parity (D-Task6-3 Option A) |
| `pcsx2_osd_show_texture_replacements` | `OsdShowTextureReplacements` | `false` | `pcsx2_load_texture_replacements` (D-Task6-4 Option A — within-Graphics chain) |

### Group: Messages (1 row, `dependsOn = "pcsx2_osd_messages_pos!=0"`)

| Core key | INI section / key | Type | Default | Source |
|---|---|---|---|---|
| `pcsx2_warn_about_unsafe_settings` | **`EmuCore / WarnAboutUnsafeSettings`** (NOT `EmuCore/GS`) | Bool | **`true`** | `Pcsx2Config.cpp:1943` |

**Total: 23. 22 under `[EmuCore/GS]`, 1 under `[EmuCore]`** — same split pattern as Task 2's patches rows.

## Plan/adapter/source mismatches resolved at brainstorm time

Re-verified against current source. The Phase 4 plan tables (`...phase4-graphics.md:197-234`) were drafted before Task 5's source-verification discipline became routine. All 5 mismatches resolved in this spec; plan text is not edited.

| ID | Mismatch | Resolution |
|---|---|---|
| **M1** | Plan §199 says whole sub-tab is `EmuCore`. | **22 rows under `EmuCore/GS`, 1 row (`WarnAboutUnsafeSettings`) under `EmuCore`.** Matches the standalone adapter and PCSX2 source layout (`GSOptions` vs `EmuCoreOptions`). |
| **M2** | Plan §205 says `OsdMessagesPos` default `0 (Top-Left)`. | TopLeft is enum index **1** (None=0). **Use `1`.** |
| **M3** | Plan §206 says `OsdPerformancePos` default `1 (Top-Right)`. | TopRight is enum index **3**. **Use `3`.** |
| **M4** | Plan §207 says `OsdBoldText` default `false`. Adapter line 821 says `"true"`. Source has NO `OsdBoldText = X;` line in `Defaults()` body (Pcsx2Config.cpp:720-745). | Bitfield zero-init → **`false`**. The adapter is wrong. Task 6 ships `false`. Adapter fix is a deferred SP7c followup. |
| **M5** | Plan §203/§204 lists `OsdScale` and `OsdMargin` as Combo. Standalone adapter has them as `SettingDef::Int` sliders (25-500 step 25 and 0-100 step 1). | Libretro v2 is Combo-only — convert to enumerated stops per D-Task6-5 below. |

## Locked design decisions

### D-Task6-1 — `OsdBoldText` default = `false`

The bitfield zero-initializes to `false` and the `Defaults()` body has no explicit assignment for `OsdBoldText`. Use `false` for standalone runtime parity. Use this row as the **Case 17b anchor** for "adapter says true but source says false" detection. Adapter fix is **out of scope** for Task 6.

### D-Task6-2 — `OsdshowPatches` INI key typo preserved verbatim

PCSX2's actual INI key is `OsdshowPatches` (lowercase 's' in 'show') — confirmed at `Config.h:781` and `Pcsx2Config.cpp:741`. PCSX2 silently ignores the correctly-cased key on read. The libretro **core option key** is the natural `pcsx2_osd_show_patches`; only the **INI key string** passed to `SetBoolValue` in `ApplyDefaults` carries the typo: `si.SetBoolValue("EmuCore/GS", "OsdshowPatches", ...)`.

### D-Task6-3 — `OsdShowVideoCapture` + `OsdShowInputRec` exposed as inert rows

The libretro variant drives neither FFmpeg capture nor input recording, so both indicators have no underlying state to display — toggling does nothing visible. Two options were considered:
- **Option A (chosen):** Expose as core options anyway. UX cost: two checkboxes that "do nothing visible". Schema fidelity is preserved (89/89). Same trade-off Task 4 made for renderer-gated texture-replacement rows.
- **Option B (rejected):** Drop from libretro entirely. Would require an exception list in `check_schema_fidelity.py` and create a parity gap.

### D-Task6-4 — `OsdShowTextureReplacements` gated on `pcsx2_load_texture_replacements`

Adapter has no dependsOn on this row; plan §230 suggests one. Two options:
- **Option A (chosen):** Add `dependsOn = "pcsx2_load_texture_replacements"`. Master-bool chain, both rows under the same Graphics card → `findChildren` resolves correctly (same shape as Task 4's three Load→{Async,Precache,DumpMipmaps,DumpFmv} chains). UX wins: row is correctly greyed when texture replacements are disabled.
- **Option B (rejected):** No dependsOn, match standalone behavior exactly (row stays editable but silently inert). Rejected because Option A is strictly better UX with zero new risk — within-Graphics is the safe shape.

### D-Task6-5 — Slider→Combo stops for `OsdScale` and `OsdMargin`

Libretro v2 is Combo-only. Both knobs are sliders standalone-side. Stops chosen to keep dropdowns compact (≤10 entries) while covering useful range and putting the default on a stop.

| Knob | Standalone | Combo stops (8 each) | Default |
|---|---|---|---|
| `pcsx2_osd_scale` | Int 25..500 step 25 (20 values) | `50, 75, 100, 125, 150, 200, 300, 500` | `100` |
| `pcsx2_osd_margin` | Int 0..100 step 1 (101 values) | `0, 5, 10, 15, 20, 30, 50, 100` | `10` |

Precedent: Task 2 Crop (11 stops) and Task 5 CAS Sharpness (8 stops). Inline at each call site; no shared-const extraction for 2 sites.

## Architecture & components

### Core side (`pcsx2-libretro` repo)

1. **`Values::Osd` struct** — replaces the empty stub at `CoreOptionsGraphics.h:106-108` (line range will shift to match the actual current location).

   ```cpp
   struct Osd {
       // 23 knobs mirroring standalone PCSX2 Graphics/On-Screen Display
       // sub-tab. 22 fields stored under [EmuCore/GS];
       // warn_about_unsafe_settings stored under [EmuCore]. Defaults match
       // PCSX2 source verbatim (Config.h:730-733,768-786 +
       // Pcsx2Config.cpp:720-745,1943).
       int  osd_scale            = 100;   // px-scale, neutral=100
       int  osd_margin           = 10;    // px from screen edge
       int  osd_messages_pos     = 1;     // OsdOverlayPos::TopLeft
       int  osd_performance_pos  = 3;     // OsdOverlayPos::TopRight
       bool osd_bold_text        = false; // bitfield zero-init (adapter says true, source says false)
       // Performance Stats group (9)
       bool osd_show_speed         = false;
       bool osd_show_fps           = false;
       bool osd_show_vps           = false;
       bool osd_show_resolution    = false;
       bool osd_show_gs_stats      = false;
       bool osd_show_cpu           = false;
       bool osd_show_gpu           = false;
       bool osd_show_indicators    = true;
       bool osd_show_frame_times   = false;
       // System Information group (2)
       bool osd_show_hardware_info = false;
       bool osd_show_version       = false;
       // Settings & Inputs group (6)
       bool osd_show_settings              = false;
       bool osd_show_patches               = false;
       bool osd_show_inputs                = false;
       bool osd_show_video_capture         = true;
       bool osd_show_input_rec             = true;
       bool osd_show_texture_replacements  = false;
       // Messages group (1) — [EmuCore] section
       bool warn_about_unsafe_settings = true;
   } osd;
   ```

2. **`AppendDefinitions`** — 23 `out.push_back({...})` blocks following Phase 4 conventions:
   - 2 Combos for `osd_scale` / `osd_margin` with 8 enumerated stops each.
   - 2 Combos for `osd_messages_pos` / `osd_performance_pos` with 10 stops mirroring `OsdOverlayPos`.
   - 19 Bools using the standard enabled/disabled pair. Per Task 5's cosmetic-fix lesson, every Bool combo carries `(Default)` on whichever entry matches the default.

3. **Parse branches** — 23 `else if (id == "pcsx2_osd_...")` branches in the option-callback dispatch. Combos → `parse_int`; Bools → `parse_bool`.

4. **`ApplyDefaults`** — 23 `si.SetXValue(...)` calls. **22 use `"EmuCore/GS"`**; **1 (`warn_about_unsafe_settings`) uses `"EmuCore"`**. `OsdshowPatches` lowercase-s INI key is used verbatim.

5. **Per-launch echo** — multi-line for readability (single line exceeds ~140-char terminal width). Format:
   ```
   [CoreOptions] graphics.osd: scale=%d margin=%d msg_pos=%d perf_pos=%d bold=%s
   [CoreOptions] graphics.osd.perf:  spd=%s fps=%s vps=%s res=%s gs=%s cpu=%s gpu=%s ind=%s ft=%s
   [CoreOptions] graphics.osd.sys:   hw=%s ver=%s
   [CoreOptions] graphics.osd.input: set=%s pat=%s inp=%s vc=%s ir=%s tr=%s
   [CoreOptions] graphics.osd.warn:  unsafe=%s
   ```
   5 lines is generous but greppable; each line is independently meaningful. Follows Task 5's "multi-line echo when knob count exceeds ~8" pattern.

6. **`test_core_options.cpp`** — two new cases bringing total from 40 → 42:
   - **Case 17** — non-default flips covering each value flavor: `osd_scale=200`, `osd_margin=30`, `osd_messages_pos=5` (Center), `osd_performance_pos=9` (BottomRight), `osd_bold_text=enabled`, `osd_show_fps=enabled`, `osd_show_indicators=disabled` (flips true→false), `osd_show_settings=enabled`, `osd_show_patches=enabled`, `warn_about_unsafe_settings=disabled`. (Tests Parse, not dependsOn resolution.)
   - **Case 17b** — defaults-when-unset: assert all 23 field defaults explicitly. Anchored on the **4 non-false bool defaults** (`osd_show_indicators=true`, `osd_show_video_capture=true`, `osd_show_input_rec=true`, `warn_about_unsafe_settings=true`) plus **`osd_bold_text=false`** (catches anyone who follows the adapter's incorrect `default="true"`).

### Host side (`RetroNest-Project` repo)

23 `s.append(gopt(...))` rows in the PCSX2 per-emulator adapter (libretro), under category `"Graphics"`, subcategory `"On-Screen Display"`, in 5 groups matching the core-side structure. dependsOn fan-out:

- **Group "On-Screen Display"** (5 rows): no dependsOn.
- **Group "Performance Stats"** (9 rows): all carry `dependsOn = "pcsx2_osd_performance_pos!=0"`.
- **Group "System Information"** (2 rows): both carry `dependsOn = "pcsx2_osd_performance_pos!=0"`.
- **Group "Settings & Inputs"** (6 rows):
  - `osd_show_settings`: `dependsOn = "pcsx2_osd_messages_pos!=0"`
  - `osd_show_patches`: `dependsOn = "pcsx2_osd_show_settings"` (master-bool chain — chained gating: settings master gates patches, which is in turn gated by messages_pos!=0 transitively)
  - `osd_show_inputs`: (none)
  - `osd_show_video_capture`: (none)
  - `osd_show_input_rec`: (none)
  - `osd_show_texture_replacements`: `dependsOn = "pcsx2_load_texture_replacements"` (master-bool from Task 4, within-Graphics)
- **Group "Messages"** (1 row): `warn_about_unsafe_settings` carries `dependsOn = "pcsx2_osd_messages_pos!=0"`.

**Cross-category audit:** Every dependsOn key (`pcsx2_osd_performance_pos`, `pcsx2_osd_messages_pos`, `pcsx2_osd_show_settings`, `pcsx2_load_texture_replacements`) lives inside the **Graphics** category card. The cross-category limitation `[[cross-category-dependson-limitation]]` does NOT apply.

## Data flow

Identical to Task 5:

```
RetroNest user toggles host row
  ↓ (PerEmulatorSettingsManager writes value to QSettings)
  ↓
RetroNest launch path serializes selected option values into RETRO_ENVIRONMENT_SET_VARIABLES
  ↓
libretro core option callback fires for each changed key
  ↓
Parse branch updates Values::Osd field
  ↓
ApplyDefaults writes corresponding INI key (EmuCore/GS or EmuCore) via SettingsInterface
  ↓
PCSX2 GS reads OSD bitfield at next frame → overlay updates
```

No persistent INI on disk (libretro variant). The only verification surface besides UI behavior is the `[CoreOptions]` echo log.

## Error handling

Same as all prior Phase 4 tasks:
- Unknown variable name in dispatch → falls through (handled by existing default branch).
- Unparseable combo value → falls back to the field's struct default (existing Parse helpers).
- INI write failures → silently ignored (PCSX2 in-memory only).
- No new error paths introduced.

## Testing

1. **Unit:** `test_core_options` Cases 17 + 17b. Compiled against the same minimal harness as Cases 16/16b.
2. **Schema fidelity:** `check_schema_fidelity.py` between the two commits — RED after core, GREEN at 89/89 after host. Same pattern as every prior Phase 4 task.
3. **Build:** `./scripts/build-universal.sh` clean (universal + Rosetta).
4. **Live-smoke gate (user-driven, 12 steps):**
   1. Build universal.
   2. Launch via Rosetta with stdout capture (recipe in `[[phase4-task6-prep]]`).
   3. Graphics card → On-Screen Display sub-tab → 23 rows visible in 5 groups.
   4. **Position-master gate:** `OsdPerformancePos = None` greys 11 rows (9 PerfStats + 2 SysInfo); restore to TopRight → all 11 ungrey.
   5. **Messages-master gate:** `OsdMessagesPos = None` greys `OsdShowSettings` + `WarnAboutUnsafeSettings`; restore to TopLeft → both ungrey.
   6. **Settings master-chain:** `OsdShowSettings = Enabled` ungreys `OsdshowPatches`; toggle off greys it.
   7. **Texture-replacement cross-sub-tab chain:** Texture Replacement sub-tab → `Load Textures = Disabled` → return to OSD sub-tab → `OsdShowTextureReplacements` greyed. Toggle Load Textures on → ungreys.
   8. **Visual gate 1:** `Show FPS = Enabled` + `OsdPerformancePos = TopLeft` → launch game → FPS overlay top-left (not default top-right).
   9. **Visual gate 2:** `OsdScale = 200` → launch → FPS overlay double-size.
   10. **Visual gate 3:** `Show Speed = Enabled` + trigger fast-forward → green speed >100% on overlay.
   11. **Warn-about-unsafe smoke:** `WarnAboutUnsafeSettings = Enabled` AND `EE Cycle Rate = -3 (50% Underclock)` → launch → unsafe-settings warning toast appears.
   12. **OsdBoldText visual:** `OSD Text Style (Bold) = Enabled` → text becomes visibly bolder.

## File touch-list

**`pcsx2-libretro` core commit (one commit):**
- `pcsx2-libretro/CoreOptionsGraphics.h` — replace empty Osd stub with 23-field struct.
- `pcsx2-libretro/CoreOptionsGraphics.cpp` — `AppendDefinitions` (23 blocks), Parse dispatch (23 branches), `ApplyDefaults` (23 writes, EmuCore/GS for 22 + EmuCore for 1), per-launch echo (5 new lines).
- `pcsx2-libretro/tools/test_core_options.cpp` — Cases 17 + 17b.

**`RetroNest-Project` host commit (one commit):**
- `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — 23 `gopt(...)` rows under `subcategory="On-Screen Display"`, 5 groups, dependsOn fan-out as documented.

## Commit-message templates

**Core:**
```
SP7c Phase 4 Task 6 (core): On-Screen Display sub-tab knobs (23)

[breakdown: 23 knobs across 5 groups (On-Screen Display 5 +
 Performance Stats 9 + System Information 2 + Settings & Inputs 6 +
 Messages 1). 22 under [EmuCore/GS], WarnAboutUnsafeSettings under
 [EmuCore] (split-section like Task 2 patches rows). Defaults
 verified against pcsx2/Config.h:730-733,768-786 + Pcsx2Config.cpp:
 720-745,1943 — 4 non-false bool defaults (OsdShowIndicators,
 OsdShowVideoCapture, OsdShowInputRec, WarnAboutUnsafeSettings).
 OsdBoldText defaults to false (bitfield zero-init), correcting
 standalone adapter's incorrect default="true". OsdshowPatches
 lowercase-s typo preserved verbatim in INI write.]

Schema fidelity intentionally RED at this commit -- 23 core keys
declared with no host row yet. Matching host commit restores green
at 89/89.

test_core_options 42/42 0 failures.
```

**Host:**
```
SP7c Phase 4 Task 6 (host): On-Screen Display sub-tab rows (23)

[breakdown: 23 rows under subcategory="On-Screen Display" across 5
 groups. 11 rows gated on pcsx2_osd_performance_pos!=0 (PerfStats 9
 + SysInfo 2). 1 row gated on pcsx2_osd_messages_pos!=0
 (OsdShowSettings). 1 row chained on pcsx2_osd_show_settings
 (OsdshowPatches — INI typo preserved). 1 row chained on
 pcsx2_load_texture_replacements (OsdShowTextureReplacements,
 master-bool from Task 4, also within-Graphics). 1 row
 (warn_about_unsafe_settings) gated on pcsx2_osd_messages_pos!=0.
 All dependsOn within-Graphics — cross-category limitation does
 not apply.]

schema-fidelity OK: 89 core keys, 89 host keys, byte-for-byte match.

PHASE 4 COMPLETE.
```

## Phase 4 close-out (after Task 6 ships + smoke-verified)

- Schema-fidelity: **89/89** (66 + 23).
- `test_core_options`: **42 cases, 0 failures**.
- Phase 4 sub-tasks: **ALL SHIPPED**. Phase 4 closes.
- Update `[[sp7c-kickoff]]`: add Task 6 SHIPPED section, mark Phase 4 complete, point next focus at Phase 5.
- Mark `[[phase4-task6-prep]]` as superseded.
- Optionally file SP7c followup tickets: (a) standalone adapter `OsdBoldText default="true"` → `"false"`, (b) `EmuFolders::Textures` save-dir rooting (surfaced in Task 4), (c) dialog-level `masterStates`/`masterValues` promotion ([[cross-category-dependson-limitation]]).

## Risks & precedent

| Concern | Mitigation | Precedent |
|---|---|---|
| Split-section `ApplyDefaults` (22 EmuCore/GS + 1 EmuCore) gets miswritten | Explicit per-row section literal at each `SetBoolValue` call site; both sections appear in a Case 17 assertion where observable | Task 2 patches rows (`EmuCore` for widescreen / no-interlacing, `EmuCore/GS` for the others) shipped clean |
| `OsdshowPatches` typo silently regressed to `OsdShowPatches` | Hard-code the typo verbatim in the `SetBoolValue` literal with an inline comment citing `Config.h:781`; live-smoke gate step 6 catches a non-functional row | None — first encounter |
| Adapter→source disagreement on `OsdBoldText` propagates to libretro core | Case 17b explicitly asserts `osd_bold_text=false`; comment in the struct + spec calls it out | Task 5 ShadeBoost_Gamma adapter/source disagreement (caught at brainstorm time, shipped 50) |
| Slider→Combo stops chosen poorly (default not on a stop, or too few useful options) | Default sits on a stop for both knobs (`100` in scale, `10` in margin); coverage spans useful UX range; 8 entries fits the dropdown comfortably | Task 2 Crop (11 stops) + Task 5 CAS Sharpness (8 stops) |
| Cross-category dependsOn trap recurs | All dependsOn keys verified within-Graphics in the host audit table above | Task 4 ate this lesson; Task 5 stayed within Graphics |
| Largest single-commit pair in Phase 4 (~46 changes total) | Bundle A (core) and Bundle B (host) are split, each commits independently | Tasks 1-5 all shipped with the same core+host pattern |

## Estimated effort

Roughly 2× Task 5: ~half a session for Task 5, so plan ~one session for Task 6 including the 12-step live-smoke gate.
