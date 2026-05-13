# SP7c Phase 4 Implementation Plan — Graphics Card (62 new knobs, 5 sub-tabs)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the ~62 Graphics knobs that the standalone PCSX2 dialog shows under its Graphics card. Phase 4 is the largest phase of SP7c (Phases 0–3 shipped a combined 28 knobs; this phase ~4× that). Implementation splits per-sub-tab (Display, Rendering, Texture Replacement, Post-Processing, OSD) with one core+host commit pair per sub-tab. Mirror standalone's sub-tab UX via `hasSubTabs = (category == "Graphics")` in the dialog plus `SettingDef::subcategory` on each row — both already supported by the shared dialog infrastructure (`pcsx2_settings_dialog.cpp:35-36`).

**Architecture:** Each knob touches the same five places as Phase 1/2/3's per-knob workflow:
1. New module `pcsx2-libretro/CoreOptionsGraphics.{h,cpp}` — owns `kGraphicsDefinitions[]`, `Graphics::Parse`, `Graphics::ApplyDefaults`. `Values` is nested by sub-tab (`Display`, `Rendering`, `TextureReplacement`, `PostProcessing`, `Osd`) so each sub-tab task adds its slice in isolation.
2. `pcsx2-libretro/CoreOptions.h` — add `Graphics::Values graphics{}` to `Resolved`.
3. `pcsx2-libretro/CoreOptions.cpp` — aggregate `Graphics::AppendDefinitions` + `Graphics::Parse`; bump `v.reserve(32)` → `v.reserve(96)` (28 prior + 62 new + terminator + small headroom); extend `[CoreOptions]` echo log with 5 new lines (one per sub-tab).
4. `pcsx2-libretro/Settings.cpp` — call `Graphics::ApplyDefaults` from the per-call user-options block.
5. `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — append ~62 new rows under `category="Graphics"` with `subcategory ∈ {"Display","Rendering","Texture Replacement","Post-Processing","On-Screen Display"}`.
6. `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` — add Graphics card at grid `(1, 2)` and move Memory Cards to row 2 col 0.
7. `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp` — flip the hardcoded `/*hasSubTabs=*/false` to `(category == "Graphics")` so the Graphics page renders sub-tab chrome (the `subcategory` field already drives sub-tab detection in `GenericSettingsPage`).

Schema fidelity verified by `tools/check_schema_fidelity.py`. Round-trip parsing verified by `tools/test_core_options.cpp` with one round-trip Case per sub-tab (Case 13–17).

**Tech Stack:**
- pcsx2-libretro: C++20, clang, libretro.h v2 core-options API.
- RetroNest-Project: C++20, Qt 6 (read/append-only on `settingsSchema`).
- Schema check: Python 3 (`tools/check_schema_fidelity.py` exists from Phase 0).
- Build: CMake (`build-arm64` is the dev build; universal lipo + RetroNest copy at close-out).
- Standalone unit test: `clang++` with `-DCORE_OPTIONS_TEST_ONLY`.

**Repo locations:**
- pcsx2-libretro: `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `retronest-libretro`, currently at HEAD `faf5c0faf`). Fork has only `upstream` remote — local commits only.
- RetroNest-Project: `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`, currently at HEAD `3f5e549`). 2 commits ahead of `origin/main`.

**Scope guard:**
- 62 net new core option keys, 62 net new host SettingDef rows under `category="Graphics"`.
- Phase 4 does NOT add the Recommended-card cross-reference rows (Phase 5) nor touch Network & HDD / Achievements (deferred indefinitely — see spec Decision 1).
- Hub gains exactly one card (Graphics) in Phase 4.
- `git push origin retronest-libretro` is forbidden — fork has no `origin`.

---

## Design decisions (resolved at plan-writing time)

These five decisions were open at kickoff (see `memory/sp7c_kickoff.md` Phase 4 section). All are resolved here in line with the kickoff's recommended defaults so the per-task work proceeds without re-litigation.

### D1 — Inline `kBoolValues` per knob; defer `CORE_BLOCK_RE` hardening (high confidence)

Phase 4 introduces 30+ bool knobs that all share the standard `{{"enabled","Enabled"},{"disabled","Disabled"},{nullptr,nullptr}}` value list. The kickoff recommended hardening `tools/check_schema_fidelity.py`'s `CORE_BLOCK_RE` to accept a shared `kBoolValues[]` identifier (mirroring the host-side `HOST_VALUES_REF_RE` resolver) so the duplication collapses to one declaration.

**Decision:** Defer the script hardening. Keep the inline duplication in Phase 4.

Reasoning:
- Schema-fidelity gates every commit; a script change carries breakage risk that compounds with 10+ per-sub-tab commits. A green-throughout Phase 4 matters more than 300 saved lines of repetition.
- Inline form is also what Phase 1/2/3 used; consistency across files makes the regex change a low-priority follow-up rather than a mid-phase blocker.
- Phase 5 or a focused tooling commit AFTER Phase 4 ships can revisit. By then we have real numbers (vs. estimates) on duplication cost.

If a sub-task hits an unexpected regex edge case (e.g. multi-line `enabled/disabled` blocks parse weirdly), the fix is on the per-knob inline form, not the script. Document at task time.

### D2 — Sub-tabs via `hasSubTabs` + `subcategory` (high confidence — spec already specifies this)

Standalone PCSX2's Graphics widget renders sub-tabs (Display, Rendering, Texture Replacement, Post-Processing, On-Screen Display) via the shared `GenericSettingsPage::SettingsGraphicsSubTabBar`, which auto-detects sub-tabs from distinct `SettingDef::subcategory` values (`generic_settings_page.cpp:124-126`) when the parent dialog passes `hasSubTabs=true` to `pushPage`. The standalone dialog already does `const bool hasSubTabs = (category == "Graphics"); pushPage(page, hasSubTabs);` (`pcsx2_settings_dialog.cpp:35-36`).

**Decision:** Mirror standalone. The libretro dialog flips its one hardcoded `pushPage(page, /*hasSubTabs=*/false)` (`pcsx2_libretro_settings_dialog.cpp:45`) to the same `(category == "Graphics")` form. Each Graphics SettingDef row carries `subcategory ∈ {"Display","Rendering","Texture Replacement","Post-Processing","On-Screen Display"}`. No new dialog plumbing — the chrome already exists.

Media Capture sub-tab is dropped entirely (no `subcategory == "Media Capture"` rows) per spec Decision 1.

### D3 — Per-knob libretro-applicability audit happens AT sub-tab-task time, not plan-time

Some standalone Graphics rows MAY be inert in the libretro path (X11/Wayland-specific renderer-internal flags, frontend-owned window settings, etc.). Phase 2 (Audio) hit the same situation with Cubeb-only knobs.

**Decision:** Each sub-tab task does a pre-implementation pass:
1. Read each row's INI key in `cpp/src/adapters/pcsx2_adapter.cpp` (source of truth).
2. Grep the PCSX2 source tree for the INI key's consumer.
3. If the consumer is unreachable from the libretro path (e.g. X11-only code, Cubeb-only init), drop the knob and log the drop in the task's commit message.
4. Otherwise, include the knob with the standalone default.

Plan does NOT pre-decide drops — the 62-count is the upper bound and may shrink. The plan tracks "expected ~N, actual M" per sub-task in commit messages so the post-phase memory captures real numbers.

### D4 — Int-slider knobs become Combo with enumerated stops; defaults always reachable

Standalone PCSX2 uses int sliders for several Graphics knobs (upscale_multiplier, MaxAnisotropy, blend accuracy). libretro core options are Combo-only — no free-form numeric entry. Each int-slider knob becomes a Combo whose `values[]` is the enumerated stops the standalone slider exposes.

**Decision:** Pick stops at sub-tab-task time, with these rules:
- Standalone's default value MUST be in the stops list.
- Standalone's slider min/max MUST be in the stops list (so users can reach the extremes).
- Intermediate stops mirror standalone's slider tick marks if specified; otherwise standard halves (e.g. 1×, 1.5×, 2×, 3×, 4×, 6×, 8× for upscale_multiplier).

Each task documents the stops chosen in its commit message. Schema-fidelity check enforces "default is in values" structurally (Case 7 in `test_core_options.cpp`) so a bad pick fails the test.

### D5 — Plan structure: one task per sub-tab + scaffold + hub + lipo (8–9 commits)

Phase 1 (15 knobs) shipped 5 commits + 2 follow-ups. Phase 4 (62 knobs) at the same density would shipto ~20 commits — too noisy. Sub-tab batching keeps each commit reviewable while reducing total commit count.

**Decision:**

| Task | Commits | Repo |
|---|---|---|
| Task 0 — verify baseline green | 0 (verification only) | both |
| Task 1 — scaffold `CoreOptionsGraphics` module | 1 | pcsx2-libretro |
| Task 2 — Display sub-tab (17 knobs → audit; expected ~13-17) | 2 (1 core + 1 host) | both |
| Task 3 — Rendering sub-tab (7 knobs → audit) | 2 | both |
| Task 4 — Texture Replacement sub-tab (6 knobs → audit) | 2 | both |
| Task 5 — Post-Processing sub-tab (9 knobs → audit) | 2 | both |
| Task 6 — On-Screen Display sub-tab (23 knobs → audit) | 2 | both |
| Task 7 — Graphics hub card + dialog `hasSubTabs` flip | 1 | RetroNest |
| Task 8 — universal RetroNest.app refresh + live smoke gate | 0 (manual; user runs) | host artifact |

**Total:** 12 commits (1 core scaffold + 5×2 sub-tabs + 1 host hub/dialog) across both repos. Matches the kickoff's 8-12 estimate.

### D6 — Schema-fidelity reserve bump (high confidence)

Phase 3 set `v.reserve(32)` (28 phase 1-3 knobs + terminator + headroom). Phase 4 adds up to 62. New reserve: `v.reserve(96)` = 28 + 62 + terminator + 5 headroom. The Phase 2 followup commit (`d1bf6f40c`) taught us to size to FINAL capacity, not running total. Bump happens in Task 1 (scaffold).

---

## Sub-tab inventory (counted 2026-05-13 from `cpp/src/adapters/pcsx2_adapter.cpp`)

All rows below are the standalone PCSX2 dialog's Graphics-card sub-tabs. Phase 4 mirrors them row-for-row, with per-knob applicability audit happening at sub-tab-task time (D3). Counts below are upper bounds; actual count may be slightly lower after audit drops.

### Display (~17 knobs) — Task 2

Source: `pcsx2_adapter.cpp` Graphics/Display rows. INI section: `EmuCore/GS` unless noted.

| Core option key | INI key | Type | Standalone default |
|---|---|---|---|
| `pcsx2_aspect_ratio` | `EmuCore/GS/AspectRatio` | Combo | `Auto 4:3/3:2` |
| `pcsx2_fmv_aspect_ratio` | `EmuCore/GS/FMVAspectRatioSwitch` | Combo | `Off` |
| `pcsx2_deinterlace_mode` | `EmuCore/GS/deinterlace_mode` | Combo | `0` (Automatic) |
| `pcsx2_stretch_y` | `EmuCore/GS/StretchY` | Combo | `100` |
| `pcsx2_crop_left` | `EmuCore/GS/CropLeft` | Combo | `0` |
| `pcsx2_crop_top` | `EmuCore/GS/CropTop` | Combo | `0` |
| `pcsx2_crop_right` | `EmuCore/GS/CropRight` | Combo | `0` |
| `pcsx2_crop_bottom` | `EmuCore/GS/CropBottom` | Combo | `0` |
| `pcsx2_pcrtc_antiblur` | `EmuCore/GS/pcrtc_antiblur` | Bool | `true` |
| `pcsx2_integer_scaling` | `EmuCore/GS/IntegerScaling` | Bool | `false` |
| `pcsx2_pcrtc_offsets` | `EmuCore/GS/pcrtc_offsets` | Bool | `false` |
| `pcsx2_disable_interlace_offset` | `EmuCore/GS/disable_interlace_offset` | Bool | `false` |
| `pcsx2_pcrtc_overscan` | `EmuCore/GS/pcrtc_overscan` | Bool | `false` |
| `pcsx2_linear_present_mode` | `EmuCore/GS/linear_present_mode` | Combo | `Bilinear (Smooth)` |
| `pcsx2_enable_widescreen_patches` | `EmuCore/EnableWideScreenPatches` | Bool | `false` |
| `pcsx2_enable_no_interlacing_patches` | `EmuCore/EnableNoInterlacingPatches` | Bool | `false` |
| `pcsx2_fmv_aspect_ratio_switch` (duplicate of above) | — | — | covered by `pcsx2_fmv_aspect_ratio` |

(Renderer already in Phase 0 under `category=Recommended`; Phase 5 will cross-list under Graphics/Display.)

Group: `"Display"` (single group for all 17 — sub-tab IS the visual grouping).
Subcategory: `"Display"`.

**Audit notes:** Crop / aspect-ratio / integer-scaling rows all feed PCSX2's GS dispatcher, which the libretro variant exercises identically to standalone (libretro just owns the final framebuffer copy). Patches rows feed `VMManager`'s patch loader, also unchanged. No expected drops.

### Rendering (~7 knobs) — Task 3

Source: `pcsx2_adapter.cpp` Graphics/Rendering rows. All under `EmuCore/GS`.

| Core option key | INI key | Type | Standalone default |
|---|---|---|---|
| `pcsx2_upscale_multiplier` | `EmuCore/GS/upscale_multiplier` | Combo | `1.0` |
| `pcsx2_filter` | `EmuCore/GS/filter` | Combo | `2` (Auto) |
| `pcsx2_tri_filter` | `EmuCore/GS/TriFilter` | Combo | `0` (Off) |
| `pcsx2_max_anisotropy` | `EmuCore/GS/MaxAnisotropy` | Combo | `0` (Off) |
| `pcsx2_dithering_ps2` | `EmuCore/GS/dithering_ps2` | Combo | `2` (Unscaled) |
| `pcsx2_accurate_blending_unit` | `EmuCore/GS/accurate_blending_unit` | Combo | `1` (Basic) |
| `pcsx2_hw_mipmap` | `EmuCore/GS/hw_mipmap` | Bool | `true` |

**Audit notes:** All 7 are GS-internal — applicable verbatim. `upscale_multiplier` stops: `0.5×, 1×, 1.5×, 2×, 2.5×, 3×, 4×, 5×, 6×, 8×` (standalone slider stops).

### Texture Replacement (~6 knobs) — Task 4

Source: `pcsx2_adapter.cpp` Graphics/Texture Replacement rows. INI section: `EmuCore/GS`.

| Core option key | INI key | Type | Standalone default |
|---|---|---|---|
| `pcsx2_load_texture_replacements` | `EmuCore/GS/LoadTextureReplacements` | Bool | `false` |
| `pcsx2_load_texture_replacements_async` | `EmuCore/GS/LoadTextureReplacementsAsync` | Bool | `true` |
| `pcsx2_precache_texture_replacements` | `EmuCore/GS/PrecacheTextureReplacements` | Bool | `false` |
| `pcsx2_dump_replaceable_textures` | `EmuCore/GS/DumpReplaceableTextures` | Bool | `false` |
| `pcsx2_dump_replaceable_mipmaps` | `EmuCore/GS/DumpReplaceableMipmaps` | Bool | `false` |
| `pcsx2_dump_textures_with_fmv_active` | `EmuCore/GS/DumpTexturesWithFMVActive` | Bool | `false` |

**Audit notes:** Replacement loader reads from `texture_replacements/{game_id}/` under `DataRoot`; standalone behavior. The libretro variant's `EmuFolders` setup at SP1 routed `Textures` to `save_dir + "/textures"` — confirm at task time. Audit may also flag that dump rows write to disk and may need a per-game scope check.

### Post-Processing (~9 knobs) — Task 5

Source: `pcsx2_adapter.cpp` Graphics/Post-Processing rows. INI section: `EmuCore/GS`.

| Core option key | INI key | Type | Standalone default |
|---|---|---|---|
| `pcsx2_cas_mode` | `EmuCore/GS/CASMode` | Combo | `0` (None) |
| `pcsx2_cas_sharpness` | `EmuCore/GS/CASSharpness` | Combo | `50` |
| `pcsx2_fxaa` | `EmuCore/GS/fxaa` | Bool | `false` |
| `pcsx2_tv_shader` | `EmuCore/GS/TVShader` | Combo | `0` (None) |
| `pcsx2_shade_boost` | `EmuCore/GS/ShadeBoost` | Bool | `false` |
| `pcsx2_shade_boost_brightness` | `EmuCore/GS/ShadeBoost_Brightness` | Combo | `50` |
| `pcsx2_shade_boost_contrast` | `EmuCore/GS/ShadeBoost_Contrast` | Combo | `50` |
| `pcsx2_shade_boost_saturation` | `EmuCore/GS/ShadeBoost_Saturation` | Combo | `50` |
| `pcsx2_shade_boost_gamma` | `EmuCore/GS/ShadeBoost_Gamma` | Combo | `100` |

**Audit notes:** CAS / FXAA / ShadeBoost / TV-shader are GS post-pass shaders — all in the libretro path. Sliders (CASSharpness, ShadeBoost_*) need Combo-with-stops; standalone uses 0-100 range, propose stops `0, 25, 50, 75, 100` (5 stops covers extremes + midpoints).

`dependsOn` chains: `pcsx2_cas_sharpness` depends on `pcsx2_cas_mode != 0`; `pcsx2_shade_boost_{brightness,contrast,saturation,gamma}` depend on `pcsx2_shade_boost == enabled`. Document at task time.

### On-Screen Display (~23 knobs) — Task 6

Source: `pcsx2_adapter.cpp` Graphics/On-Screen Display rows (lines 1779-1801 enumerate them). INI section: `EmuCore`.

| Core option key | INI key | Type | Standalone default |
|---|---|---|---|
| `pcsx2_osd_scale` | `EmuCore/OsdScale` | Combo | `100` |
| `pcsx2_osd_margin` | `EmuCore/OsdMargin` | Combo | `10` |
| `pcsx2_osd_messages_pos` | `EmuCore/OsdMessagesPos` | Combo | `0` (Top-Left) |
| `pcsx2_osd_performance_pos` | `EmuCore/OsdPerformancePos` | Combo | `1` (Top-Right) |
| `pcsx2_osd_bold_text` | `EmuCore/OsdBoldText` | Bool | `false` |
| `pcsx2_osd_show_speed` | `EmuCore/OsdShowSpeed` | Bool | `false` |
| `pcsx2_osd_show_fps` | `EmuCore/OsdShowFPS` | Bool | `false` |
| `pcsx2_osd_show_vps` | `EmuCore/OsdShowVPS` | Bool | `false` |
| `pcsx2_osd_show_resolution` | `EmuCore/OsdShowResolution` | Bool | `false` |
| `pcsx2_osd_show_gs_stats` | `EmuCore/OsdShowGSStats` | Bool | `false` |
| `pcsx2_osd_show_cpu` | `EmuCore/OsdShowCPU` | Bool | `false` |
| `pcsx2_osd_show_gpu` | `EmuCore/OsdShowGPU` | Bool | `false` |
| `pcsx2_osd_show_indicators` | `EmuCore/OsdShowIndicators` | Bool | `true` |
| `pcsx2_osd_show_frame_times` | `EmuCore/OsdShowFrameTimes` | Bool | `false` |
| `pcsx2_osd_show_hardware_info` | `EmuCore/OsdShowHardwareInfo` | Bool | `false` |
| `pcsx2_osd_show_version` | `EmuCore/OsdShowVersion` | Bool | `false` |
| `pcsx2_osd_show_settings` | `EmuCore/OsdShowSettings` | Bool | `false` |
| `pcsx2_osd_show_inputs` | `EmuCore/OsdShowInputs` | Bool | `false` |
| `pcsx2_osd_show_video_capture` | `EmuCore/OsdShowVideoCapture` | Bool | `true` |
| `pcsx2_osd_show_input_rec` | `EmuCore/OsdShowInputRec` | Bool | `true` |
| `pcsx2_osd_show_texture_replacements` | `EmuCore/OsdShowTextureReplacements` | Bool | `false` |
| `pcsx2_osd_show_patches` | `EmuCore/OsdshowPatches` | Bool | `false` |
| `pcsx2_warn_about_unsafe_settings` | `EmuCore/WarnAboutUnsafeSettings` | Bool | `true` |

**Audit notes:** Three OSD rows are tied to features the libretro variant doesn't drive:
- `OsdShowVideoCapture` — libretro variant has no FFmpeg capture (Media Capture dropped); flag but keep (ImGui still renders the OSD line if other capture mechanisms set it).
- `OsdShowInputRec` — libretro variant doesn't drive PCSX2's input recording UI; same flag-but-keep reasoning.
- `OsdShowTextureReplacements` — only meaningful when `LoadTextureReplacements=true` (Texture Replacement sub-tab). Keep with implicit dependsOn.

`WarnAboutUnsafeSettings` is in `EmuCore` section (not `EmuCore/GS`) but visually belongs to OSD; matches standalone placement.

Largest single sub-task; commit pair will be the longest in the phase.

---

## File structure

### Created in this phase

| File | Purpose | Lands in |
|---|---|---|
| `pcsx2-libretro/CoreOptionsGraphics.h` | Mirrors `CoreOptionsMemoryCards.h` shape — nested `struct Values { struct Display{...} display; struct Rendering{...} rendering; struct TextureReplacement{...} texture_replacement; struct PostProcessing{...} post_processing; struct Osd{...} osd; }`; `AppendDefinitions`/`Parse`/`ApplyDefaults` decls. | Task 1 |
| `pcsx2-libretro/CoreOptionsGraphics.cpp` | Empty `AppendDefinitions`/`Parse` (filled by sub-tab tasks); `ApplyDefaults` (no-op until knobs land). | Task 1 |

### Modified in this phase

| File | Action | Lands in |
|---|---|---|
| `pcsx2-libretro/CoreOptions.h` | Add `Graphics::Values graphics{}` to `Resolved`; `#include "CoreOptionsGraphics.h"`. | Task 1 |
| `pcsx2-libretro/CoreOptions.cpp` | `#include "CoreOptionsGraphics.h"`; bump `v.reserve(32)` → `v.reserve(96)`; call `Graphics::AppendDefinitions(v)` + `Graphics::Parse(cb, r.graphics)`; extend `[CoreOptions]` echo with 5 graphics lines (one per sub-tab — filled as sub-tab tasks land). | Task 1 (skeleton) + each sub-tab task (echo extension) |
| `pcsx2-libretro/Settings.cpp` | Call `Graphics::ApplyDefaults(g_si, options ? options->graphics : Graphics::Values{})` from the per-call user-options block (same shape as the other 3 modules). | Task 1 |
| `pcsx2-libretro/CMakeLists.txt` | Append `CoreOptionsGraphics.cpp` to target sources. | Task 1 |
| `pcsx2-libretro/tools/test_core_options.cpp` | Header compile example: add `../CoreOptionsGraphics.cpp`. | Task 1; per-sub-tab tasks add Case 13-17 round-trip tests. |
| `pcsx2-libretro/CoreOptionsGraphics.cpp` | Sub-tab tasks append `out.push_back({...})` blocks + `Parse` branches + `ApplyDefaults` writes. | Tasks 2-6 (one per sub-tab) |
| `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` | Sub-tab tasks append rows under `category="Graphics"` with the matching `subcategory`. May introduce a `gopt(subcategory, group, key, label, def, values, tooltip, dependsOn={})` helper that wraps the existing `opt()` and sets `d.subcategory` afterward — see Task 2 step 2 for the helper landing decision. | Tasks 2-6 |
| `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` | Add Graphics card at grid `(1, 2)`; move Memory Cards from `(1, 2)` to `(2, 0)`. Update leading comment. | Task 7 |
| `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp` | Replace `pushPage(page, /*hasSubTabs=*/false)` (line 45) with `const bool hasSubTabs = (category == "Graphics"); pushPage(page, hasSubTabs);`. | Task 7 |

No upstream PCSX2 file edits.

---

## Task 0 — Confirm baseline is green

**Files:** none (verification only).

- [ ] **Step 1: pcsx2-libretro HEAD + clean tree.**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git log -1 --oneline retronest-libretro
git status
```

Expected: HEAD is `faf5c0faf SP7c Phase 3 Task 2 (core): Memory Cards knobs`. Working tree clean except for the listed untracked tools/ artifacts (`__pycache__`, `resources`, `test_core_options`, `test_rcheevos_hash`, `test_region_prefix`).

- [ ] **Step 2: RetroNest-Project HEAD + clean tree.**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git log -1 --oneline main
git status
```

Expected: HEAD is `3f5e549` (Phase 3 host + plan docs). Working tree clean.

- [ ] **Step 3: schema-fidelity check on baseline.**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```

Expected: green — "28 / 28 byte-for-byte match".

- [ ] **Step 4: standalone test on baseline.**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp \
  ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
  ../CoreOptionsMemoryCards.cpp \
  -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: all Cases pass.

- [ ] **Step 5: pcsx2-libretro arm64 build.**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro -j$(sysctl -n hw.ncpu)
```

Expected: green build.

---

## Task 1 — Scaffold CoreOptionsGraphics module (1 commit)

**Goal:** Land empty `CoreOptionsGraphics` module with all wiring in place, schema-fidelity still green at 28/28, build green. No new user-visible knobs. Mirrors Phase 3 Task 1 (commit `f96afc3b5`).

**Files modified:**
- `pcsx2-libretro/CoreOptionsGraphics.h` (new)
- `pcsx2-libretro/CoreOptionsGraphics.cpp` (new)
- `pcsx2-libretro/CoreOptions.h`
- `pcsx2-libretro/CoreOptions.cpp`
- `pcsx2-libretro/Settings.cpp`
- `pcsx2-libretro/CMakeLists.txt`

- [ ] **Step 1: Create `CoreOptionsGraphics.h`.**

Shape: mirror `CoreOptionsMemoryCards.h` but with nested sub-tab structs. The nesting is deliberate — each sub-tab task adds its slice of fields without touching siblings. Leave field bodies empty for now; sub-tab tasks fill them.

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 4: Graphics-category core options.
//
// Owns the kGraphicsDefinitions[] slice of the master core-options
// table. CoreOptions.cpp aggregates this module's slice into the single
// table dispatched via SET_CORE_OPTIONS_V2.
//
// Values is nested by sub-tab (Display, Rendering, TextureReplacement,
// PostProcessing, Osd) so each sub-tab task in Phase 4 adds its slice
// of fields in isolation. The sub-tab names mirror the standalone
// PCSX2 dialog's Graphics widget sub-tabs.

#pragma once

#include "libretro.h"

#include <vector>

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }
class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions::Graphics
{

// Per-sub-tab resolved values. Each nested struct owns the fields its
// sub-tab task populates.
struct Values
{
    struct Display {
        // Phase 4 Task 2 fills these.
    } display;

    struct Rendering {
        // Phase 4 Task 3 fills these.
    } rendering;

    struct TextureReplacement {
        // Phase 4 Task 4 fills these.
    } texture_replacement;

    struct PostProcessing {
        // Phase 4 Task 5 fills these.
    } post_processing;

    struct Osd {
        // Phase 4 Task 6 fills these.
    } osd;
};

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out);
void Parse(retro_environment_t cb, Values& out);
void ApplyDefaults(MemorySettingsInterface& si, const Values& v);

} // namespace Pcsx2Libretro::CoreOptions::Graphics
```

- [ ] **Step 2: Create `CoreOptionsGraphics.cpp` with empty bodies.**

Mirror `CoreOptionsMemoryCards.cpp`'s file shell + empty function bodies. The `#ifndef CORE_OPTIONS_TEST_ONLY` gate around `ApplyDefaults` is required (test compile doesn't link MemorySettingsInterface).

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsGraphics.h"

#ifdef CORE_OPTIONS_TEST_ONLY
#include <cstdarg>
#include <cstdio>
static void FrontendLog(int /*level*/, const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
}
#else
#include "LibretroFrontend.h"
#include "common/MemorySettingsInterface.h"
#endif

#include <cstring>

namespace Pcsx2Libretro::CoreOptions::Graphics
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& /*out*/)
{
    // SP7c Phase 4 — per-sub-tab tasks populate this:
    //   Task 2: Display sub-tab (~17 knobs)
    //   Task 3: Rendering sub-tab (~7 knobs)
    //   Task 4: Texture Replacement sub-tab (~6 knobs)
    //   Task 5: Post-Processing sub-tab (~9 knobs)
    //   Task 6: On-Screen Display sub-tab (~23 knobs)
}

void Parse(retro_environment_t /*cb*/, Values& /*out*/)
{
    // SP7c Phase 4 — per-sub-tab tasks fill this with one branch per knob.
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& /*si*/, const Values& /*v*/)
{
    // SP7c Phase 4 — per-sub-tab tasks fill this with the matching
    // si.Set{Bool,Int,Float,String}Value calls per knob.
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Graphics
```

- [ ] **Step 3: Extend `CoreOptions.h`.**

Add `#include "CoreOptionsGraphics.h"` and `Graphics::Values graphics{}` field. Remove the old `// Future phases append: Graphics::Values graphics{}` comment.

- [ ] **Step 4: Extend `CoreOptions.cpp`.**

- `#include "CoreOptionsGraphics.h"`.
- Bump `v.reserve(32)` → `v.reserve(96)` with updated arithmetic comment.
- Add `Graphics::AppendDefinitions(v);` after `MemoryCards::AppendDefinitions(v);`.
- Add `Graphics::Parse(cb, r.graphics);` after `MemoryCards::Parse(...)`.
- Remove the `// Future phases append Graphics::Parse here.` placeholder.
- Echo log lines (the 5 sub-tab summary lines) stay deferred until sub-tab tasks populate the `Values` sub-structs — scaffold task does NOT add echo lines yet because there are no fields to log.

- [ ] **Step 5: Extend `Settings.cpp`.**

- Add `#include "CoreOptionsGraphics.h"` at the top.
- After the `MemoryCards::ApplyDefaults` call in the per-call user-options block (around line 320), add:

```cpp
const CoreOptions::Graphics::Values gfx_defaults{};
CoreOptions::Graphics::ApplyDefaults(
    g_si, options ? options->graphics : gfx_defaults);
```

- [ ] **Step 6: Extend `CMakeLists.txt`.**

Append `CoreOptionsGraphics.cpp` to the `target_sources(pcsx2_libretro PRIVATE ...)` list (alphabetically: it sits between `CoreOptionsEmulation.cpp` and `CoreOptionsMemoryCards.cpp`).

- [ ] **Step 7: Update `tools/test_core_options.cpp` header comment.**

Add `../CoreOptionsGraphics.cpp` to the compile example. No new test cases yet — the scaffold has nothing to round-trip.

- [ ] **Step 8: Verify arm64 build green.**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro -j$(sysctl -n hw.ncpu)
```

- [ ] **Step 9: Verify standalone test still passes.**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp \
  ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
  ../CoreOptionsMemoryCards.cpp ../CoreOptionsGraphics.cpp \
  -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: all Cases pass (Graphics has zero knobs so its `Parse` is a no-op).

- [ ] **Step 10: Verify schema-fidelity still 28/28 green.**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```

Expected: 28 / 28 byte-for-byte match (Graphics module declares zero keys, no host rows added — count unchanged).

- [ ] **Step 11: Commit.**

```
SP7c Phase 4 Task 1: scaffold CoreOptionsGraphics module

Mirrors the Phase 3 scaffold (commit f96afc3b5): empty
AppendDefinitions/Parse/ApplyDefaults bodies; aggregated by
CoreOptions.{h,cpp}; Resolved gains a Graphics::Values graphics{}
field; Settings.cpp's per-call block calls Graphics::ApplyDefaults
(currently a no-op); CMakeLists.txt includes the new TU.

Bumps BuildDefinitions reserve from 32 → 96 (target capacity = 28
prior + 62 Phase 4 + terminator + 5 headroom). The Phase 2 followup
(commit d1bf6f40c) lesson — v.reserve(N) must size to FINAL capacity —
applies here too.

Values is nested by sub-tab (Display, Rendering, TextureReplacement,
PostProcessing, Osd) so each sub-tab task adds its slice in isolation.
Sub-tab names mirror the standalone PCSX2 dialog's Graphics widget
sub-tabs.

Schema fidelity still 28/28 (no new keys this commit). Sub-tab
knob bodies land in Tasks 2-6.
```

---

## Tasks 2-6 — Per-sub-tab core+host commits (10 commits total)

Each sub-tab task follows the same shape, parametrized by the sub-tab's knob inventory. The sequence is fixed (Display → Rendering → Texture Replacement → Post-Processing → On-Screen Display) so the easier tabs land first and the longest (OSD) lands last.

**Per-sub-tab task shape:**

- [ ] **Sub-step A: Audit.** For each row in the sub-tab inventory above, grep the PCSX2 source for the INI key's consumer; if unreachable from the libretro path, drop the knob and note in the commit message. Lock the final knob list for the task.

- [ ] **Sub-step B: Core commit.**
  - Add fields to the matching `Values::<SubTab>` nested struct.
  - Append one literal `out.push_back({...})` block per knob in `AppendDefinitions`.
  - Append one branch per knob in `Parse` (mirror MemoryCards's strcmp pattern for bools; mirror Emulation's per-knob lookup for combos with custom-keyed enum strings).
  - Append one `si.Set{Bool,Int,Float,String}Value` per knob in `ApplyDefaults`.
  - Extend the `[CoreOptions]` echo log in `CoreOptions.cpp` with one summary line for this sub-tab.
  - Add Case 13-17 (one per sub-tab) round-trip assertion in `test_core_options.cpp`.
  - Build + test_core_options + commit. **Schema-fidelity is intentionally RED at this commit** (host side lands in sub-step C); this is the same two-commit pattern Phase 3 used (lesson from `memory/sp7c_kickoff.md`).

- [ ] **Sub-step C: Host commit.**
  - Append `s.append(gopt("<subcategory>", "<group>", "pcsx2_<key>", ...))` rows under `category="Graphics"` in the libretro adapter. (Task 2 introduces `gopt()`; later tasks reuse it.)
  - Run `cmake --build build-universal --target check_schema_fidelity` — must go green now (count matches both sides).
  - Build host arm64 (sanity), then commit.

**Per-task commit pair count:** 2 (1 core RED + 1 host GREEN). Total across Tasks 2-6: 10 commits.

### Task 2 specifics (Display, 17 knobs) — introduces `gopt()` helper

In addition to the per-sub-tab shape above, Task 2 introduces the `gopt()` helper lambda in the libretro adapter. Definition mirrors `opt()` but adds `subcategory` as the first parameter and hardcodes `category="Graphics"`:

```cpp
auto gopt = [](const QString& subcategory,
               const QString& group,
               const QString& key,
               const QString& label,
               const QString& def,
               const QVector<QPair<QString,QString>>& valuesAndLabels,
               const QString& tooltip,
               const QString& dependsOn = {}) -> SettingDef {
    SettingDef d;
    d.storage = SettingDef::Storage::LibretroOption;
    d.category = "Graphics";
    d.subcategory = subcategory;
    d.group = group;
    d.key = key;
    d.label = label;
    d.defaultValue = def;
    d.tooltip = tooltip;
    d.type = SettingDef::Combo;
    d.options = valuesAndLabels;
    d.dependsOn = dependsOn;
    return d;
};
```

`tools/check_schema_fidelity.py` HOST_BLOCK_RE must be widened to match `s.append(gopt(...))` blocks (subcategory string in the position currently occupied by category — pattern becomes `s.append((?:opt|gopt)(...))` with an optional leading subcategory string for the `gopt` variant). This is a small, one-time Python change paired with the Task 2 host commit; document it as part of the host commit's diff.

### Task 6 specifics (OSD, 23 knobs)

OSD is the largest sub-tab. Its commit pair (core + host) will produce the longest diff in Phase 4 — accept that and don't split further (per-sub-tab batching is the recurring unit). Audit notes flag 3 rows (`OsdShowVideoCapture`, `OsdShowInputRec`, `OsdShowTextureReplacements`) for awareness but not auto-drop.

`pcsx2_warn_about_unsafe_settings` is structurally an OSD row even though its INI section is `EmuCore` not `EmuCore/GS`. Place it in the same sub-tab.

---

## Task 7 — Graphics hub card + dialog `hasSubTabs` flip (1 commit, host-only)

**Files modified:**
- `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp`
- `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp`

- [ ] **Step 1: Update the hub grid.**

Current layout (4 cards):
```
Row 0: Recommended (full-width)
Row 1: Emulation | Audio | Memory Cards
```

New layout (5 cards):
```
Row 0: Recommended (full-width)
Row 1: Emulation | Graphics | Audio
Row 2: Memory Cards
```

Insert Graphics card at `(1, 1)`, push Audio to `(1, 2)`, move Memory Cards from `(1, 2)` to `(2, 0)`. Update the leading comment to reflect Phase 4 having shipped (Phases 0–4 active, Phase 5 remaining for the Recommended-card reorg).

Card glyph: 🎨 (palette) — matches `Pcsx2Adapter::subcategoryIcon` Rendering glyph; clearer than 🖥️ which is the Display sub-tab glyph. Card blurb: `"Aspect ratio, upscaling, post-FX, OSD, textures"` — covers all 5 sub-tabs in one line.

- [ ] **Step 2: Flip `hasSubTabs` in the dialog.**

In `pcsx2_libretro_settings_dialog.cpp`, replace:
```cpp
pushPage(page, /*hasSubTabs=*/false);
```
with:
```cpp
const bool hasSubTabs = (category == "Graphics");
pushPage(page, hasSubTabs);
```
matching `pcsx2_settings_dialog.cpp:35-36`.

- [ ] **Step 3: Verify schema-fidelity still green** (no new keys, but the host file changed — sanity).

- [ ] **Step 4: Build RetroNest arm64 + verify the Graphics card renders with all 5 sub-tabs (`countSettings("Graphics")` should equal 62 minus any audit drops).**

- [ ] **Step 5: Commit.**

```
SP7c Phase 4 Task 7: Graphics hub card + dialog hasSubTabs flip

Hub gains a Graphics card at grid (1, 1); Memory Cards moves to
(2, 0) to make room. Dialog flips its hardcoded
pushPage(..., /*hasSubTabs=*/false) to
pushPage(..., (category == "Graphics")), matching the standalone
pcsx2_settings_dialog. The subcategory field on Graphics rows
drives sub-tab detection in GenericSettingsPage without any new
plumbing.

Closes Phase 4 host-side wiring. Sub-tab knob content lands in
Tasks 2-6 (interleaved with this commit during Phase 4 — see plan).
```

---

## Task 8 — Universal RetroNest.app refresh + live smoke gate (manual)

**No commits — the user runs this end-to-end after all preceding tasks land.**

- [ ] **Step 1: Rebuild universal RetroNest.app.**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
./scripts/build-universal.sh
```

(Or the equivalent `cmake --build build-universal-x86_64` + `macdeployqt` + `scripts/lipo-merge-app.sh` sequence — see `memory/sp7c_kickoff.md` Phase 3 close-out for the recipe.)

- [ ] **Step 2: Rebuild pcsx2_libretro universal dylib + copy into the app.**

Build arm64 + x86_64 separately, lipo-merge, copy to `cpp/build-universal/RetroNest.app/Contents/Resources/cores/`.

- [ ] **Step 3: Live smoke (one tweak per sub-tab on R&C 2 NTSC).**

Launch RetroNest from Finder (avoids the hardened-runtime codesign issue documented in `memory/sp7c_kickoff.md`).

Smoke matrix:
- **Display:** Set `AspectRatio` 4:3 → Stretch. Confirm widescreen appears in-game.
- **Rendering:** Set `upscale_multiplier` 1× → 4×. Confirm visible resolution bump.
- **Texture Replacement:** Toggle `LoadTextureReplacements` on. Confirm `[Replacements]` log line.
- **Post-Processing:** Toggle `fxaa` on. Confirm visible smoothing.
- **OSD:** Toggle `OsdShowFPS` on. Confirm FPS counter visible.

Each tweak should round-trip in `options.json` and the `[CoreOptions]` echo log should show the value reaching the core.

- [ ] **Step 4: PAL coverage smoke (one tweak on DBZ TT2 EU).**

DB-flip DBZ TT2 → libretro adapter. Verify Graphics card renders. Tweak one knob. Confirm `[CoreOptions]` echo shows the value. Switch back to standalone after.

- [ ] **Step 5: Regression check.**

Verify the Phase 0-3 knobs (renderer, MTVU, fast-boot, Emulation knobs, Audio knobs, Memory Cards knobs) still tweak correctly. Spot-check a few — full regression is overkill; the structural test covers schema, smoke catches obvious behavioral regressions.

---

## Phase 4 ground rules (carrying forward from Phases 1-3)

- HUB CARD MANDATORY IN-PHASE (Phase 1 hub-gap lesson). Task 7 covers this.
- `CORE_BLOCK_RE` only matches LITERAL `out.push_back({...})` form. No lambda helpers in `AppendDefinitions`. Inline the `{"enabled","Enabled"},{"disabled","Disabled"},{nullptr,nullptr}` value list per bool knob (D1 deferred hardening).
- `HOST_BLOCK_RE` widens to also match `s.append(gopt(...))` blocks (one-time Python change in Task 2).
- No `/*dependsOn=*/` inline comments in host adapter rows.
- `opt(...)` lambda is 8-arg (positional + dependsOn={} default); `gopt(...)` is also 8-arg with subcategory replacing category.
- `v.reserve(N)` sized to FINAL capacity in Task 1.
- No `git push origin retronest-libretro` (fork has no `origin`).
- Standalone test compile needs `../CoreOptionsGraphics.cpp` added.
- Trust `cmake --build` + `./test_core_options`, NOT clangd diagnostics. clangd's compile_commands.json staleness produces false-positive errors (e.g. `'common/MemorySettingsInterface.h' file not found`, `Unknown type name 'QString'`) — Phase 3 documented this and all 3 prior phases experienced it. Ignore those diagnostics if the cmake build is green.
- Universal RetroNest.app refresh: when host-side changes land (Tasks 2-6 host + Task 7), also rebuild RetroNest x86_64 + macdeployqt + lipo-merge-app.sh (or run `scripts/build-universal.sh`). Universal dylib alone is not enough.
- Rosetta launch recipe for live-smoke logging: `arch -x86_64 env DYLD_FRAMEWORK_PATH="$APP/Contents/Frameworks" QT_PLUGIN_PATH="$APP/Contents/PlugIns" "$APP/Contents/MacOS/RetroNest"` against `cpp/build-universal/RetroNest.app`. Pair with Monitor + `tail -F | grep -E` filter on `[CoreOptions]|FATAL|ERROR` for clean signal capture.

---

## Estimated commit count and timeline

**Total commits:** 12 across both repos.
- pcsx2-libretro: 1 scaffold + 5 sub-tab core commits = 6.
- RetroNest-Project: 5 sub-tab host commits + 1 hub/dialog commit = 6.

**Per-task wall time estimate (with audit + build + test + commit, no smoke):**
- Task 0: 5 min.
- Task 1: 30-45 min.
- Tasks 2-6 (per sub-tab): 60-120 min each, scaling with knob count. OSD (23 knobs) is the longest.
- Task 7: 30 min.
- Task 8: 30 min smoke + universal rebuild.

**Phase total:** ~8-12 hours of focused work. Spans multiple sessions naturally; live smoke after each sub-tab is the recommended pause point.

---

## Open questions for the user (none expected)

The 5 kickoff design decisions all resolved with the kickoff's recommended defaults (see D1-D5 above). If the user redirects on any of them at plan-review gate, this plan amends in place and Task 1 starts after the amendment.
