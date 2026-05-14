# SP7c Phase 4 Task 5 — Post-Processing sub-tab (9 knobs)

**Sub-project:** SP7c (settings parity)
**Phase / Task:** Phase 4 (Graphics card) Task 5 (Post-Processing sub-tab)
**Date:** 2026-05-14
**Predecessors:** Tasks 1/2/3/4/7-bumped shipped + live-smoke verified (sp7c_kickoff memory)
**Successor:** Phase 4 Task 6 (On-Screen Display, ~23 knobs)

## Goal

Expose the 9 GS post-pass shader knobs in PCSX2's standalone "Graphics → Post-Processing" tab as libretro core options, wired into the RetroNest host adapter under `Graphics → Post-Processing`. After landing, schema-fidelity goes from 57/57 to **66/66**; `test_core_options` from 38 to **40 cases** (Case 16 + 16b).

## Non-goals

- Not implementing on-screen-display (Task 6).
- Not addressing `patches.zip` follow-up or Qt-on-Rosetta teardown SIGABRT (separate tracked issues).
- Not promoting `masterValues` to dialog level (deferred SP7c follow-up — not needed here, all dependsOn within-category).
- Not touching the existing `pcsx2_renderer` or any other pre-Phase-4 knob.

## Inventory — the 9 knobs

All under INI section `EmuCore/GS`. Two groups within the Post-Processing sub-tab.

### Group: "Sharpening/Anti-Aliasing" (3 rows)

| Core key | INI key | Type | Default | dependsOn | PCSX2 source default |
|---|---|---|---|---|---|
| `pcsx2_cas_mode` | `CASMode` | Combo (3 stops) | `0` (Disabled) | — | `Config.h:723` `GSCASMode::Disabled` |
| `pcsx2_cas_sharpness` | `CASSharpness` | Combo (5 stops: 0/25/50/75/100) | `50` | `pcsx2_cas_mode!=0` | `Config.h:894` `u8 = 50` |
| `pcsx2_fxaa` | `fxaa` | Bool | `false` | — | bitfield zero-init |

### Group: "Filters" (6 rows)

| Core key | INI key | Type | Default | dependsOn | PCSX2 source default |
|---|---|---|---|---|---|
| `pcsx2_tv_shader` | `TVShader` | Combo (8 stops, 0–7) | `0` (None) | — | `Config.h:870` `u8 = 0` |
| `pcsx2_shade_boost` | `ShadeBoost` | Bool (master) | `false` | — | bitfield zero-init |
| `pcsx2_shade_boost_brightness` | `ShadeBoost_Brightness` | Combo (5 stops: 0/25/50/75/100) | `50` | `pcsx2_shade_boost` | `Config.h:741` |
| `pcsx2_shade_boost_contrast` | `ShadeBoost_Contrast` | Combo (5 stops) | `50` | `pcsx2_shade_boost` | `Config.h:742` |
| `pcsx2_shade_boost_saturation` | `ShadeBoost_Saturation` | Combo (5 stops) | `50` | `pcsx2_shade_boost` | `Config.h:744` |
| `pcsx2_shade_boost_gamma` | `ShadeBoost_Gamma` | Combo (5 stops) | `50` | `pcsx2_shade_boost` | `Config.h:743` |

**Defaults note (correction to prep memory):** All 4 ShadeBoost sliders default to **50**, not 100. Verified against PCSX2 master `Config.h:741-744`. The shader formula is `value * (1/50)`, so 50 is the neutral midpoint for all four (Brightness/Contrast/Saturation/Gamma all become 1.0× at 50). `RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp:643` already shows the correct `"50"` standalone default — no host-side correction required.

## Architecture

Same two-commit shape as Tasks 2/3/4:

1. **Core RED commit** (`pcsx2-libretro` repo, branch `retronest-libretro`): declare 9 keys + `Values::PostProcessing` struct + parse + apply + per-launch echo + 2 test cases. Schema-fidelity intentionally RED at this commit (9 unmatched core keys).
2. **Host GREEN commit** (`RetroNest-Project` repo, branch `main`): add 9 `gopt(...)` rows to `pcsx2_adapter.cpp`. Schema-fidelity returns to GREEN at 66/66.

### Core side — `CoreOptionsGraphics`

**`pcsx2-libretro/libretro/core_options/CoreOptionsGraphics.h`** — extend the `Values` struct:

```cpp
struct PostProcessing {
    int  cas_mode               = 0;     // GSCASMode::Disabled
    int  cas_sharpness          = 50;
    bool fxaa                   = false;
    int  tv_shader              = 0;     // TV shader off
    bool shade_boost            = false;
    int  shade_boost_brightness = 50;
    int  shade_boost_contrast   = 50;
    int  shade_boost_saturation = 50;
    int  shade_boost_gamma      = 50;
} post_processing;
```

**`CoreOptionsGraphics.cpp`** — three additions:

- `AppendDefinitions`: 9 literal `out.push_back({...})` blocks.
  - 3 Combos with explicit value lists (CASMode 3 stops, TVShader 8 stops, CASSharpness 5 stops).
  - 4 Combos with shared 5-stop `0/25/50/75/100` value list (the ShadeBoost sliders) — **inlined at each call site**, mirroring Task 2's Crop{Left,Top,Right,Bottom} pattern. (Extracting to a shared const is allowed by `HOST_VALUES_REF_RE` but not worth it for 4 sites × 5 stops; inline keeps grep-ability.)
  - 2 Bools using the standard `{enabled, disabled}` pair (FXAA, ShadeBoost).
- `Parse`: 9 branches — combos via `parse_int`, bools via `parse_bool`.
- `ApplyDefaults`: 9 `si.SetXValue` writes under `EmuCore/GS` — combos via `SetIntValue`, bools via `SetBoolValue`.

Per-launch echo line appended to existing graphics echo in `core.cpp` (or wherever Phase 4 has been logging — same place as `graphics.rendering` line in Task 3):

```
[CoreOptions] graphics.postproc: cas=%d cas_sharp=%d fxaa=%s tv=%d shade=%s sb_br=%d sb_co=%d sb_sa=%d sb_ga=%d
```

Booleans serialize as the strings `"on"`/`"off"` (Task 4 precedent).

### Host side — `pcsx2_adapter.cpp`

The 9 rows already exist as `SettingDef` entries (lines 583-645). The Task 5 host commit adds matching `gopt(...)` calls in the `settingsSchema()` function under the existing Post-Processing sub-tab block. Each `gopt` row mirrors the SettingDef's category/subcategory/group/INI section + Combo value list, and uses libretro core key names (`pcsx2_cas_mode`, etc.).

Group ordering preserved from `pcsx2_adapter.cpp:584-645`:
1. Sharpening/Anti-Aliasing: cas_mode, cas_sharpness, fxaa
2. Filters: tv_shader, shade_boost, shade_boost_brightness, shade_boost_contrast, shade_boost_saturation, shade_boost_gamma

dependsOn strings (libretro-key form):
- `pcsx2_cas_sharpness`: `"pcsx2_cas_mode!=0"` (value-equality, Task 3 precedent: `pcsx2_tri_filter!=2`)
- 4× ShadeBoost sliders: `"pcsx2_shade_boost"` (bare-key master-bool, Task 4 precedent)

All 5 dependsOn keys live within the Graphics card → `findChildren` in `GenericSettingsPage::refreshDependencies()` resolves them correctly. The cross-category limitation (`cross_category_dependson_limitation` memory) does **not** apply here.

## Testing

### Existing test extended

`pcsx2-libretro/tools/test_core_options.cpp` — add Case 16 + Case 16b:

**Case 16 — "Post-Processing round-trip":** flip a representative selection that covers every value-type and every group:

| Key | Set value | Why this value |
|---|---|---|
| `pcsx2_cas_mode` | `1` (SharpenOnly) | 3-stop Combo, non-default |
| `pcsx2_cas_sharpness` | `75` | Combo-with-stops, non-default, depends on cas_mode flip |
| `pcsx2_fxaa` | `enabled` | Bool, non-default |
| `pcsx2_tv_shader` | `3` | 8-stop Combo, non-default |
| `pcsx2_shade_boost` | `enabled` | Bool master, non-default |
| `pcsx2_shade_boost_brightness` | `25` | Combo-with-stops, non-default |
| `pcsx2_shade_boost_gamma` | `0` | Edge value (min stop) |

Leave `shade_boost_contrast` and `shade_boost_saturation` at 50 to demonstrate selective-flip preserves untouched defaults.

After `Parse → ApplyDefaults`, assert each `Values::PostProcessing` field matches and assert each corresponding INI write hit `EmuCore/GS` with the expected value.

**Case 16b — "Post-Processing defaults when unset":** call `Parse` with no Post-Processing keys present, then assert all 9 fields match the documented defaults (cas_mode=0, cas_sharpness=50, fxaa=false, tv_shader=0, shade_boost=false, shade_boost_{brightness,contrast,saturation,gamma}=50). This catches drift in any field's initializer — there is no single "anchor" default in this sub-tab, so the test asserts the full default vector.

After Task 5: `test_core_options` reports **40 cases, 0 failures**.

### Schema-fidelity gate

After both commits land, `tools/check_schema_fidelity.py` (or whichever Phase 4 entry point Task 4 used) reports **66 core keys, 66 host keys, byte-for-byte match**.

### Live-smoke gate (user-driven, after both commits ship)

1. `./scripts/build-universal.sh` from `RetroNest-Project/` root.
2. Launch RetroNest under Rosetta with `[CoreOptions]` stdout filter (recipe in `phase4_task5_prep` memory; do NOT compose `APP=...; $APP/...` inline).
3. Open Graphics card → Post-Processing sub-tab. Verify 9 rows visible across the 2 groups in correct order.
4. With `CAS Mode = Disabled`, confirm `CASSharpness` row is greyed out.
5. Set `CAS Mode = Sharpen Only`. Confirm `CASSharpness` un-greys. Confirm next-launch echo line includes `cas=1`.
6. With `Shade Boost = Disabled`, confirm all 4 ShadeBoost slider rows greyed out.
7. Set `Shade Boost = Enabled`. Confirm all 4 rows un-grey.
8. Visual-effect check 1: `Shade Boost = Enabled` + `Brightness = 100`. Picture noticeably brighter.
9. Visual-effect check 2: `Shade Boost = Enabled` + `Gamma = 0`. Picture noticeably darker.
10. Optional visual-effect check 3: `TV Shader = Scanline Filter`. Scanline overlay visible.

**Greying preserves stored value** (Task 4 finding): when a master toggles off, dependent values stay in the echo log even though greyed in the UI. PCSX2 ignores them at the core level when their master is off — functionally inert. No action required, just expected.

## Error handling

Same as Tasks 2/3/4 — there is none specific to Post-Processing. Combo values come pre-validated by libretro frontend (only declared values selectable). Out-of-range from a hand-edited INI is silently clamped by PCSX2's `u8` storage. `parse_int` / `parse_bool` already handle malformed strings by leaving the field at its initialized default (Tasks 2/3/4 verified).

## Risks

| Risk | Mitigation |
|---|---|
| Off-by-one in ShadeBoost slider stops affecting visual smoke results | Stops `0/25/50/75/100` validated against shader formula (`value/50` → 0/0.5/1.0/1.5/2.0× multipliers) — 50 is unity, all stops are visually distinguishable. |
| `clangd` false-positive `'common/MemorySettingsInterface.h' not found` etc. during edit | Ignore — `cmake --build` is the truth. Same noise hit Tasks 2/3/4. |
| `cas_mode!=0` dependsOn string form regression | Identical mechanism to Task 3's `pcsx2_tri_filter!=2`; both within-Graphics. No new ground. |
| Adapter row order / group naming mismatch on host commit | `pcsx2_adapter.cpp:583-645` is the canonical row order and group strings; copy them verbatim into the gopt block. |

## Out-of-scope follow-ups (do not address in Task 5)

- `patches_zip_followup` — separate tracked issue.
- `qt_rosetta_teardown_crash` — SIGABRT will fire again at app exit during smoke; ignore.
- Promote `masterStates`/`masterValues` to dialog level — deferred SP7c follow-up (not needed for Task 5).
- `EmuFolders::Textures` not save_dir-rooted — tracked as potential SP1 followup.

## Commit-message templates

**Core (RED):**
```
SP7c Phase 4 Task 5 (core): Post-Processing sub-tab knobs (9)

[breakdown: 9 knobs across Sharpening/AA (CAS×2, FXAA) +
 Filters (TV Shader, ShadeBoost master + 4 ShadeBoost sliders).
 All slider defaults = 50 (verified against pcsx2/Config.h:741-744,894).]

Schema fidelity intentionally RED at this commit — 9 core keys
declared with no host row yet. Matching host commit restores green.
```

**Host (GREEN):**
```
SP7c Phase 4 Task 5 (host): Post-Processing sub-tab rows (9)

[breakdown: 3 rows in Sharpening/Anti-Aliasing group +
 6 rows in Filters group. CASSharpness gated on cas_mode!=0;
 4 ShadeBoost sliders gated on shade_boost (master-bool).
 All dependsOn chains within Graphics page → resolve cleanly.]

schema-fidelity OK: 66 core keys, 66 host keys, byte-for-byte match.
```
