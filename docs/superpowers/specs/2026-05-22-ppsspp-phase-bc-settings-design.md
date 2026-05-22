# PPSSPP Libretro — Phase B+C: Settings Schema & Hub Cards Design

**Date:** 2026-05-22
**Status:** Design approved, ready for implementation plan
**Predecessor:** Phase A (controller bindings) shipped in commits `c52bfd7` + `abfd87f` on main

## Goal

Make the Settings → PSP page render a populated hub with categorised cards, so users can actually tweak PPSSPP libretro core options through the RetroNest UI. Currently `PpssppLibretroAdapter` returns the base-class empty defaults for both `settingsSchema()` and `settingsHubCards()`, so opening Settings → PSP from `AppController::showEmulatorSettings()` triggers the early-return "exposes no settings hub cards — skipping" path.

## Non-goals

- **Network options** (23 upstream options — WLAN, adhoc, UPnP, MAC parts, server IP parts). Deferred to a later phase. PSP multiplayer is half-broken and the 12-part MAC / 12-part server IP entry surfaces don't render usefully as comboboxes anyway.
- **`resolutionOptions()` / `aspectRatioOptions()` overrides**. mgba and PCSX2 libretro adapters both return `{}` for these — the data shape (section/key/IniPatch) was designed for standalone adapters that write INI files and doesn't map cleanly to libretro core options (which use `Storage::LibretroOption` via `SettingDef`). PPSSPP follows the same pattern: `ppsspp_internal_resolution` lives in the Video card as a `SettingDef`, not as a separate resolution dropdown. This is what the prior phase roadmap memo called "Phase C" — it collapses into one Phase B bullet.
- **BIOS file declarations** (`biosFiles()`) — Phase D. Files happen to exist at `bios/PPSSPP/` from the user's old standalone install; fresh-install validation is out of scope here.
- **Frontend setting defaults** (`frontendSettingDefaults()`) — Phase E. RetroNest-side overrides like default aspect_mode are a separate concern.
- **PPSSPP-specific hotkeys** (`hotkeyDefs()`) — Phase G. Shared 22 hotkeys from `libretro_hotkey_defs.cpp` are probably sufficient.

## Overrides

Three methods on `PpssppLibretroAdapter`:

1. `QVector<SettingDef> settingsSchema() const override;`
2. `QVector<SettingsHubCard> settingsHubCards() const override;`
3. (No override needed) `resolutionOptions()` / `aspectRatioOptions()` keep base-class `{}` return per mgba/PCSX2 pattern.

## Hub card layout

5 cards on a 3-column QGridLayout (mirroring `mgba_libretro_adapter.cpp:385-412`):

```
+---------------------------------------------------------------+
|  Row 0  | Recommended         (1×3 spanning card)             |
+---------+---------+-----------------+---------------+---------+
|  Row 1  | System  |     Video      |     Input      |        |
+---------+---------+-----------------+---------------+---------+
|  Row 2  | Hacks   |                 |               |         |
+---------+---------+-----------------+---------------+---------+
```

Card defs (using mgba's emoji glyph convention from `mgba_libretro_adapter.cpp`):

| Icon | Title | Descriptor | categoryKey | row | col | rowSpan | colSpan |
|---|---|---|---|---|---|---|---|
| 💡 | Recommended | Most-tweaked settings — resolution, performance, compat | Recommended | 0 | 0 | 1 | 3 |
| 💾 | System | CPU core, memory, PSP model, language | System | 1 | 0 | 1 | 1 |
| 🖼 | Video | Resolution, MSAA, frameskip, texture scaling | Video | 1 | 1 | 1 | 1 |
| 🎮 | Input | Confirm button, analog deadzone & sensitivity | Input | 1 | 2 | 1 | 1 |
| ⚡ | Hacks | Speed hacks — skip buffer effects, disable culling, lazy textures | Hacks | 2 | 0 | 1 | 1 |

## Schema composition

**Duplication pattern** (matches mgba — verified at `mgba_libretro_adapter.cpp:145-150` vs the same key under `"System"` later): the 10 Recommended settings appear twice in the schema — once with `category = "Recommended"` and once with their natural category. Both rows mutate the same libretro option via the shared `key` and `Storage::LibretroOption` storage routing. The duplication is intentional UX: Recommended is a curated shortcut card; the natural-category cards are the full picture.

Total `SettingDef` entries: **53** (10 Recommended duplicates + 43 per-category originals).

### Recommended (10)

| # | Key | Label | Why "Recommended" |
|---|---|---|---|
| 1 | `ppsspp_internal_resolution` | Rendering Resolution | Headline visual setting (1x → 10x) |
| 2 | `ppsspp_cpu_core` | CPU Core | Biggest performance lever (JIT / IR / Interpreter) |
| 3 | `ppsspp_frameskip` | Frameskip | Performance fallback for slow hardware |
| 4 | `ppsspp_auto_frameskip` | Auto Frameskip | Companion to manual frameskip |
| 5 | `ppsspp_texture_scaling_level` | Texture Upscaling Level | Visual quality (xBRZ, bicubic) |
| 6 | `ppsspp_texture_anisotropic_filtering` | Anisotropic Filtering | Visual quality on slanted textures |
| 7 | `ppsspp_mulitsample_level` | MSAA Antialiasing | Visual quality (Vulkan-only at runtime) |
| 8 | `ppsspp_skip_buffer_effects` | Skip Buffer Effects | Speed hack — compat-sensitive |
| 9 | `ppsspp_cropto16x9` | Crop to 16x9 | Display preference |
| 10 | `ppsspp_psp_model` | PSP Model | Some games behave differently on 1000 vs 2000/3000 |

### System (11)

`ppsspp_cpu_core` (★), `ppsspp_fast_memory`, `ppsspp_ignore_bad_memory_access`, `ppsspp_io_timing_method`, `ppsspp_force_lag_sync`, `ppsspp_locked_cpu_speed`, `ppsspp_memstick_inserted`, `ppsspp_cache_iso`, `ppsspp_cheats`, `ppsspp_language`, `ppsspp_psp_model` (★).

★ = also in Recommended.

### Video (22)

`ppsspp_internal_resolution` (★), `ppsspp_mulitsample_level` (★), `ppsspp_cropto16x9` (★), `ppsspp_frameskip` (★), `ppsspp_auto_frameskip` (★), `ppsspp_texture_scaling_level` (★), `ppsspp_texture_anisotropic_filtering` (★), `ppsspp_backend`, `ppsspp_software_rendering`, `ppsspp_frameskiptype`, `ppsspp_frame_duplication`, `ppsspp_detect_vsync_swap_interval`, `ppsspp_inflight_frames`, `ppsspp_gpu_hardware_transform`, `ppsspp_software_skinning`, `ppsspp_hardware_tesselation`, `ppsspp_texture_scaling_type`, `ppsspp_texture_deposterize`, `ppsspp_texture_shader`, `ppsspp_texture_filtering`, `ppsspp_smart_2d_texture_filtering`, `ppsspp_texture_replacement`.

### Input (4)

`ppsspp_button_preference`, `ppsspp_analog_is_circular`, `ppsspp_analog_deadzone`, `ppsspp_analog_sensitivity`.

(Memory note had "5" — recount of `libretro_core_options.h` shows 4 active Input options.)

### Hacks (6)

`ppsspp_skip_buffer_effects` (★), `ppsspp_disable_range_culling`, `ppsspp_skip_gpu_readbacks`, `ppsspp_lazy_texture_caching`, `ppsspp_spline_quality`, `ppsspp_lower_resolution_for_effects`.

## SettingDef construction

Follow mgba's `opt()` lambda pattern at `mgba_libretro_adapter.cpp:82-145`. A local helper inside `settingsSchema()` that takes `(key, label, default, options, category, tooltip)` and stamps `Storage::LibretroOption` + `Type::Combo` automatically. For the two slider options (`ppsspp_analog_deadzone`, `ppsspp_analog_sensitivity`), use a separate slider helper with min/max/step and `layout = "slider"`.

**Value strings come verbatim from `libretro/libretro_core_options.h`.** Don't translate or normalise. Examples:
- `ppsspp_internal_resolution` values are `"480x272"`, `"960x544"`, etc. — display labels are `"1x (480x272)"`, `"2x (960x544)"`, etc. (matching the upstream `desc` strings).
- `ppsspp_cpu_core` values are `"JIT"`, `"IR JIT"`, `"Interpreter"` — labels match values.
- `ppsspp_locked_cpu_speed` is a special case — 30 values from `"disabled"` to `"999MHz"`. Keep all 30 entries to match upstream semantics.

## Tooltips

Each `SettingDef` gets a 1-line tooltip pulled verbatim from the `info` string in `libretro/libretro_core_options.h`. mgba does this — see `mgba_libretro_adapter.cpp:145-150`. Acceptable to truncate at the first sentence if a tooltip runs >150 chars.

## Defaults

Sourced from the `default_value` field in `libretro_core_options.h`. Don't override defaults unless there's a documented RetroNest-side reason (none for this phase). E.g., `ppsspp_internal_resolution` default stays at `"480x272"` (1x) — Phase E may later override this to `"960x544"` (2x) via `frontendSettingDefaults`, but that's not in scope here.

## Regression test: `test_ppsspp_libretro_schema.cpp`

QtTest target registered next to `test_ppsspp_libretro_bindings` in `cpp/CMakeLists.txt`. Same source-list pattern (libretro adapter dependencies).

Assertions:

| # | Slot | Asserts |
|---|---|---|
| 1 | `totalCount` | `settingsSchema().size() == 53` (10 Recommended + 43 per-category) |
| 2 | `everyKey_hasPpssppPrefix` | Every `.key` starts with `"ppsspp_"` |
| 3 | `everyKey_isKnownUpstream` | Every `.key` is in a hardcoded allow-list of the 43 upstream keys we expose. (Catches typos and stale entries when upstream renames an option.) |
| 4 | `everyDefault_isInOptions` | For each Combo entry, `.defaultValue` appears in `.options` as one of the value strings. |
| 5 | `recommendedHasNaturalDupe` | For each entry with `category == "Recommended"`, there exists another entry with the same `.key` and a different `.category` from the {System, Video, Input, Hacks} set. |
| 6 | `hubCards_referencedByEntries` | For each `SettingsHubCard.categoryKey`, ≥1 `SettingDef` has that as its `.category`. |
| 7 | `allEntries_useLibretroOption` | Every `SettingDef.storage == Storage::LibretroOption`. |

This is a data-shape regression guard, not a behavioural test — no runtime libretro core needed. Test links the adapter sources only (same minimal set as `test_ppsspp_libretro_bindings`).

## File map

| Path | Action | Responsibility |
|---|---|---|
| `cpp/src/adapters/libretro/ppsspp_libretro_adapter.h` | MODIFY (+2 lines) | Declare `settingsSchema()` + `settingsHubCards()` overrides. |
| `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp` | MODIFY (+~250 LOC) | Implement the two methods with the `opt()` / `slider()` helper-lambda pattern. |
| `cpp/tests/test_ppsspp_libretro_schema.cpp` | CREATE | 7-slot regression guard (above). |
| `cpp/CMakeLists.txt` | MODIFY (+~12 LOC) | Register `test_ppsspp_libretro_schema` add_executable + add_test. |

## Verification plan

Implementation-time verification (mostly the same as Phase A's verification-before-completion checkpoint):

1. **Build**: `cmake --build . --target test_ppsspp_libretro_schema -j8` succeeds.
2. **New test**: `ctest -R "^PpssppLibretroSchema$"` — all 7 slots pass.
3. **Full suite**: `ctest -j4` — 43/44 pass (the +1 from Phase A goes to 44/45; pre-existing `HotkeyDefs::duckstation_completeness` failure stays untouched).
4. **Runtime smoke** (manual): launch RetroNest → Settings → PSP. The page renders 5 cards (not the previous "skipping" no-op). Each card opens a settings view with the expected rows. Changing a setting persists across re-open. Launching a game with `ppsspp_internal_resolution = 2x` increases the rendered res.
5. **Bonus regression**: existing Phase A `test_ppsspp_libretro_bindings` still passes.

## Out-of-scope reminders (post-B+C roadmap)

From the existing phase roadmap memo:

- **Phase D**: `biosFiles()` for compat.ini, flash0 fonts, ppge_atlas.zim, lang/*.ini, adhoc-servers.json.
- **Phase E**: `frontendSettingDefaults()` — RetroNest-side overrides like aspect_mode, integer_scale.
- **Phase F**: Settings audit pass — write a doc mirroring `docs/superpowers/audits/2026-04-07-ppsspp-settings-audit.md` against the new schema.
- **Phase G**: PPSSPP-specific hotkey overrides (probably skip — shared 22 in `libretro_hotkey_defs.cpp` is sufficient).
- **Network options**: 23 deferred. If shipped, would likely be a single Network card with the 3 main toggles (WLAN, adhoc server, UPnP), and the MAC / IP entry surfaces handled by a different widget (text field with split-on-display, not a 12-row combobox stack).
