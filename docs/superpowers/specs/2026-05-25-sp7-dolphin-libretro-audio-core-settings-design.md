# SP7 — Dolphin libretro Audio + Core/system settings schema

**Date:** 2026-05-25
**Status:** Design approved, ready for plan
**Repos:** `dolphin-libretro` (`libretro` branch) — core options content; `RetroNest-Project` (`main`) — host adapter schema
**Predecessor:** SP6 (Graphics settings, shipped). **Successor:** SP8 (RetroAchievements + packaging).
**Parent specs:** `2026-05-23-dolphin-libretro-conversion-design.md` (§ "Settings schema mapping"); `2026-05-25-sp6-dolphin-libretro-graphics-settings-design.md` (the infra SP7 builds on).

## Goal

Expose Dolphin's **Audio** category plus the **Core/system** settings (the standalone's
**General** / **Advanced** / **GameCube** / **Wii** panes) as libretro core options
(`dolphin_` prefix) plus host `SettingDef`s, and add the cross-category **Recommended**
rollup card. This is **purely additive** content on the core-options infrastructure SP6
built (emit / read / apply pipeline, fidelity tooling, host schema overrides) — **no infra
refactor**. Options take effect on the next launch, matching SP6 and the other libretro
adapters.

SP6 shipped **53 curated Graphics options**. SP7 adds **37 new core options** across the
five new categories, bringing the total to **90 core options**, plus a host-only
Recommended view that re-references existing keys (no new core options).

## Scope

**In scope:** Audio + General + Advanced + GameCube + Wii core-option content (new
`CoreOptionsAudio`/`CoreOptionsCore` modules), the host `SettingDef` rows for all five
categories, the Recommended rollup card, extension of all three SP6 verification guards,
and boot-time readback logging for the new categories.

**Out of scope (deferred):**

- **SD/NAND/memory-card file *paths*** — the core hardcodes its user directory to
  `/tmp/dolphin-libretro-user` (`LibretroFrontend.cpp:123`) and the host adapter's
  `ensureConfig` manages only `controls.ini`. The GameCube/Wii *device-type* and SD
  *settings* apply cleanly as core options, but the files they reference live under that
  fixed `/tmp` dir and do not persist robustly (`/tmp` is subject to periodic cleanup).
  Giving the core a stable RetroNest-managed user dir is **SP8 packaging** work. SP7
  documents this limitation; it does not fix it.
- **SYSCONF-backed Wii settings** (Language, Widescreen, PAL60, Sound, Sensor-bar,
  Speaker volume, Screensaver) — those persist in the Wii's emulated SYSCONF binary, not
  an INI/Config key. Reached via Dolphin's native UI. (Same skip the standalone made.)
- **Memory-size override** (RAMOverrideEnable + MEM1Size + MEM2Size) — dropped as a group:
  without the size sliders the enable toggle is inert, and non-retail RAM sizes stop most
  games booting (homebrew-only).
- **Custom RTC** (EnableCustomRTC) — dropped: there is no date/time widget for the actual
  `CustomRTCValue`, so the lone toggle is inert/confusing.
- **Audio Backend combo**, **Discord presence**, **Auto Disc Change** — dropped (see
  curation rationale below).
- **Per-game INIs**, **live in-game option updates** — same deferrals as SP6.

## Architecture (additive on SP6 infra)

Same two-sided mirror as SP6. The only new data is two more `Values` members on `Resolved`
and two more `Apply` calls:

```
Core: retro_set_environment → CoreOptions::EmitCoreOptionsV2(cb)
        BuildDefinitions() = Graphics::AppendDefinitions          (SP6)
                           + Audio::AppendDefinitions              (SP7, new)
                           + Core::AppendDefinitions               (SP7, new)
                           + terminator
        ↓
Host: settingsSchema() renders all 90 rows + the Recommended view
      user edits → {root}/emulators/libretro/dolphin/options.json
        ↓
Core: retro_load_game → ReadResolved(cb) → Resolved{ graphics, audio, core }
        → Graphics::Apply(r.graphics)   (SP6, unchanged)
        → Audio::Apply(r.audio)         (SP7, new)
        → Core::Apply(r.core)           (SP7, new)
        → Config::SetCurrent(Config::MAIN_*, …)  on the CurrentRun layer
        ↓
      BootManager::BootCore — subsystems read the already-set Config layer
```

Nearly every SP7 key lives in **Dolphin.ini's `[Core]`/`[DSP]` section → `Config::MAIN_*`**
(`Source/Core/Core/Config/MainSettings.{h,cpp}`), in contrast to SP6's `GFX_*` keys. The
`CurrentRun` layer write wins over the Base layer (loaded by `UICommon::Init` in
`retro_init`) because `BootCore` does not reload it — the same mechanism SP6 relies on.

### Core-module decomposition

Two new modules next to `CoreOptionsGraphics.{h,cpp}`:

- **`CoreOptionsAudio.{h,cpp}`** — 9 options, one `Values` struct.
- **`CoreOptionsCore.{h,cpp}`** — 28 options across four sub-structs (`General`,
  `Advanced`, `GameCube`, `Wii`), mirroring how `CoreOptionsGraphics` holds five
  sub-structs in one file.

Each module follows the SP6 shape exactly: `AppendDefinitions` (literal `out.push_back({...})`
8-field blocks, no helper, so `CORE_BLOCK_RE` matches), a primitive-typed `Values` struct
(int/bool/std::string, no engine dependency), `Parse(cb, Values&)`, and `Apply(const Values&)`
guarded by `#ifndef CORE_OPTIONS_TEST_ONLY`. `CoreOptions.h`'s `Resolved` gains
`Audio::Values audio` + `Core::Values core`; `BuildDefinitions()` and `ReadResolved()` gain
the two new `AppendDefinitions`/`Parse` calls; `retro_load_game` gains the two `Apply`
calls beside the existing `Graphics::Apply`.

(Alternative considered: three modules — Audio / System / Devices. Rejected: `CoreOptionsCore`
at 28 options is well under Graphics' 53, so the extra files buy nothing.)

### Complex-combo fan-out (DSP engine)

Like SP6's Anti-Aliasing and Texture-Filtering combos, the **DSP Emulation Engine** is one
host combo that writes **two** Config keys. The fan-out lives in the core's `Audio::Parse`
(string → two `Values` fields), and `Audio::Apply` is 1:1:

| Combo value | `Config::MAIN_DSP_HLE` | `Config::MAIN_DSP_JIT` |
|---|---|---|
| `HLE` | true | (ignored) |
| `LLE Recompiler` | false | true |
| `LLE Interpreter` | false | false |

The host declares one plain combo `dolphin_dsp_engine`; the standalone's `saveTransform`/
`loadTransform` logic moves into `Parse`. DPL2 Decoder/Quality remain plain options (their
upstream enable-gating is host-UI cosmetics, not a value transform).

## Audio option set (9)

Recover exact keys/value-lists/defaults from
`git -C RetroNest-Project show 03f48ae^:cpp/src/adapters/dolphin_adapter.cpp` (Audio section).

| # | host key | label | type | default | notes |
|---|---|---|---|---|---|
| 1 | `dolphin_dsp_engine` | DSP Emulation Engine | combo `HLE` / `LLE Recompiler` / `LLE Interpreter` | `HLE` | **fan-out** → `MAIN_DSP_HLE` + `MAIN_DSP_JIT` |
| 2 | `dolphin_audio_latency` | Audio Latency | combo (ms) | `20` | `MAIN_AUDIO_LATENCY`; only active with OpenAL backend |
| 3 | `dolphin_dpl2_decoder` | Dolby Pro Logic II Decoder | bool | `disabled` | `MAIN_DPL2_DECODER` |
| 4 | `dolphin_dpl2_quality` | DPL2 Decoding Quality | combo `0`/`1`/`2`/`3` | `2` | `MAIN_DPL2_QUALITY` |
| 5 | `dolphin_audio_buffer_size` | Audio Buffer Size | combo (ms) | `80` | `MAIN_AUDIO_BUFFER_SIZE` |
| 6 | `dolphin_audio_fill_gaps` | Fill Audio Gaps | bool | `enabled` | `MAIN_AUDIO_FILL_GAPS` |
| 7 | `dolphin_audio_preserve_pitch` | Preserve Audio Pitch | bool | `disabled` | `MAIN_AUDIO_STRETCH` / preserve-pitch key |
| 8 | `dolphin_audio_mute_on_unthrottle` | Mute When Unthrottled | bool | `disabled` | `MAIN_AUDIO_MUTE_ON_DISABLED_SPEED_LIMIT` |
| 9 | `dolphin_volume` | Volume | combo (%) | `100` | `MAIN_AUDIO_VOLUME` |

**Dropped:** Audio **Backend** combo — libretro owns audio output (the core feeds samples to
the frontend), so the Dolphin backend is fixed; exactly parallel to SP6 dropping the
renderer/`GFXBackend` combo.

## Core/system option set (28)

### General (6)

| # | host key | label | type | default | notes |
|---|---|---|---|---|---|
| 1 | `dolphin_cpu_thread` | Dual Core (speedhack) | bool | **Dolphin's own** (see Defaults) | `MAIN_CPU_THREAD` |
| 2 | `dolphin_enable_cheats` | Enable Cheats | bool | `disabled` | `MAIN_ENABLE_CHEATS` |
| 3 | `dolphin_load_game_into_memory` | Load Whole Game Into Memory | bool | `disabled` | |
| 4 | `dolphin_override_region_settings` | Allow Mismatched Region Settings | bool | `disabled` | `MAIN_OVERRIDE_REGION_SETTINGS` |
| 5 | `dolphin_emulation_speed` | Speed Limit | combo (Unlimited + 10–200%) | `1.000000` | `MAIN_EMULATION_SPEED` (float multiplier) |
| 6 | `dolphin_fallback_region` | Fallback Region | combo (NTSC-J/U, PAL, …) | resolve | `MAIN_FALLBACK_REGION` |

**Dropped:** `UseDiscordPresence` (inert under RetroNest's shell; build likely lacks
Discord-RPC), `AutoDiscChange` (RetroNest's own M3U handling runs at launch — redundant/
conflicting).

### Advanced (11)

| # | host key | label | type | default | notes |
|---|---|---|---|---|---|
| 1 | `dolphin_cpu_core` | CPU Emulation Engine | combo (Interpreter / Cached Interpreter / JIT) | **Dolphin's own = JIT** | `MAIN_CPU_CORE` (`PowerPC::CPUCore` enum) |
| 2 | `dolphin_mmu` | Enable MMU | bool | `disabled` | `MAIN_MMU` |
| 3 | `dolphin_pause_on_panic` | Pause on Panic | bool | `disabled` | |
| 4 | `dolphin_accurate_cpu_cache` | Enable Write-Back Cache (slow) | bool | `disabled` | `MAIN_ACCURATE_CPU_CACHE` |
| 5 | `dolphin_correct_time_drift` | Correct Time Drift | bool | `disabled` | |
| 6 | `dolphin_rush_frame_presentation` | Rush Frame Presentation | bool | `disabled` | |
| 7 | `dolphin_smooth_early_presentation` | Smooth Early Presentation | bool | `disabled` | |
| 8 | `dolphin_overclock_enable` | Enable CPU Clock Override | bool | `disabled` | `MAIN_OVERCLOCK_ENABLE` |
| 9 | `dolphin_overclock` | CPU Overclock Multiplier | combo `1`/`2`/`3`/`4` | `1` | `MAIN_OVERCLOCK` (float; combo string parsed to float in Apply) |
| 10 | `dolphin_vi_overclock_enable` | Enable VBI Frequency Override | bool | `disabled` | |
| 11 | `dolphin_vi_overclock` | VI Overclock Multiplier | combo `1`/`2`/`3`/`4` | `1` | `MAIN_VI_OVERCLOCK` (float) |

**Dropped:** Memory Override group (RAMOverrideEnable + MEM1Size + MEM2Size), Custom RTC
group (EnableCustomRTC) — see Scope.

### GameCube (5)

Device-**type** combos and language/IPL settings only; the files they reference live under
the `/tmp` user dir (SP8 caveat). Numeric combo values match `EXIDeviceType` (`<Nothing>` = 255).

| # | host key | label | type | default | notes |
|---|---|---|---|---|---|
| 1 | `dolphin_skip_ipl` | Skip Main Menu | bool | `enabled` | `MAIN_SKIP_IPL` |
| 2 | `dolphin_gc_language` | System Language | combo (English…Dutch, `0`–`5`) | `0` | `MAIN_GC_LANGUAGE` |
| 3 | `dolphin_slot_a` | Slot A | combo (Nothing/Dummy/Memory Card/GCI Folder/USB Gecko/AGP/Microphone) | `8` (GCI Folder) | `MAIN_SLOT_A` |
| 4 | `dolphin_slot_b` | Slot B | combo (same as Slot A) | `255` (Nothing) | `MAIN_SLOT_B` |
| 5 | `dolphin_serial_port_1` | SP1 | combo (Nothing/Dummy/BBA variants/Modem/Triforce) | `255` (Nothing) | `MAIN_SERIAL_PORT_1` |

### Wii (6)

| # | host key | label | type | default | notes |
|---|---|---|---|---|---|
| 1 | `dolphin_wii_keyboard` | Connect USB Keyboard | bool | `disabled` | `MAIN_WII_KEYBOARD` |
| 2 | `dolphin_enable_wiilink` | Enable WiiConnect24 via WiiLink | bool | `disabled` | |
| 3 | `dolphin_wii_sd_card` | Insert SD Card | bool | `enabled` | `MAIN_WII_SD_CARD` |
| 4 | `dolphin_wii_sd_card_writes` | Allow Writes to SD Card | bool | `enabled` | `MAIN_WII_SD_CARD_ALLOW_WRITES` |
| 5 | `dolphin_wii_sd_card_folder_sync` | Auto-Sync SD with Folder | bool | `disabled` | |
| 6 | `dolphin_wii_sd_card_size` | SD Card File Size | combo (Auto + 64 MiB…32 GiB, byte values) | `0` (Auto) | `MAIN_WII_SD_CARD_FILESIZE` |

## Recommended rollup card (host-only, ~16 rows)

A curated cross-category **view**, not new options. Each Recommended row re-references an
**existing** core option key with the **same default and value list** — the fidelity script
merges duplicate host rows for a key and only flags a default mismatch (it already supports
this: see `parse_host` "Multiple host rows can reference the same core key"). So the
Recommended card adds **zero** core options and **zero** new keys.

Curated subset (grouped as the standalone did — Performance / Performance Hacks / Visual
Quality / Audio / Convenience):

- Graphics: `dolphin_internal_resolution`, `dolphin_aspect_ratio`, `dolphin_antialiasing`,
  `dolphin_texture_filtering`, `dolphin_widescreen_hack`, `dolphin_shader_compilation`,
  `dolphin_wait_for_shaders`, `dolphin_store_efb_to_texture`, `dolphin_store_xfb_to_texture`,
  `dolphin_skip_efb_access`, `dolphin_texcache_accuracy`
- Audio: `dolphin_dsp_engine`, `dolphin_volume`
- General: `dolphin_cpu_thread`, `dolphin_enable_cheats`
- GameCube: `dolphin_skip_ipl`

The standalone's Recommended also listed `GFXBackend` (the renderer) — **dropped**, since
that is not a core option (Metal-only; SP6 already dropped it).

## Int sliders → enumerated combos

libretro v2 options are Combo-only, so the standalone's int sliders become enumerated
combos. Each list **includes the default**:

- **Volume** `%`: 0,10,20,30,40,50,60,70,80,90,100 — default `100`.
- **Audio Latency** `ms`: 0,10,20,40,60,80,100,150,200 — default `20`.
- **Audio Buffer Size** `ms`: 32,48,64,80,96,128,160,256,512 — default `80`.
- **CPU / VI Overclock** `×`: 1,2,3,4 — default `1`. Stored as the bare integer string;
  `Apply` parses it to the float `Config::MAIN_OVERCLOCK`/`MAIN_VI_OVERCLOCK` expects
  (`"2"` → `2.0` = +100%).
- **Speed Limit**: keep the standalone's float-multiplier combo (Unlimited =
  `0.000000`, then `0.100000`…`2.000000`) — default `1.000000`.

## Defaults & the correctness gotcha

Unlike SP6's purely-visual Graphics knobs, several SP7 options affect whether a game
**boots** and how it behaves (CPU core, dual-core, MMU, region, overclock). Policy:

- **Defer to Dolphin's own default** where the standalone diverged or where we are unsure —
  *not* the standalone's presented value. The plan dispatches a subagent to pin each
  `Config::Info<T>` symbol + type + Dolphin's own default before any code is written (the
  recipe: `grep '"<key>"' Source/Core/Core/Config/MainSettings.cpp` → the `MAIN_*` symbol on
  that line; the `Info<T>` initializer carries the default). This removes placeholder risk,
  exactly as it did in SP6.
- **Divergence watch-list** (resolve + document inline, like SP6 did for
  `disable_copy_filter`): **`dolphin_cpu_thread`** (standalone presented `False`; Dolphin's
  own default is dual-core **on** — default to Dolphin's own and validate in smoke; if it
  regresses boot/compat on a test title, fall back to disabled and document why),
  **`dolphin_cpu_core`** (Dolphin's own default is the JIT for the host arch — JITARM64 on
  Apple Silicon, mapped to the combo's "JIT" value), **`dolphin_fallback_region`** and
  **`dolphin_gc_language`** (confirm enum index ↔ Config value).

## Host side (`RetroNest-Project`, `main`)

Extend the **existing** `cpp/src/adapters/libretro/dolphin_libretro_adapter.{h,cpp}` — no
new class:

- Add an **`opt(category, group, key, label, default, values, tooltip[, dependsOn])`** helper
  (explicit `category`, `subcategory=""`) for the new **flat** categories, keeping the SP6
  `gopt(subcategory, …)` helper (category hardcoded to `"Graphics"`) for Graphics rows. The
  fidelity script's `HOST_BLOCK_RE` already matches `(?:opt|gopt)` and treats arg-1 as opaque,
  so both work without a regex change.
- `settingsSchema()` — append the 37 new rows (Audio/General/Advanced/GameCube/Wii) + the
  ~16 Recommended rows (re-referencing existing keys). Plain combos; no transforms (the DSP
  fan-out is core-side).
- `settingsCategoriesWithSubTabs()` — **unchanged `{ "Graphics" }`**. The new categories are
  flat (no sub-tabs), matching the standalone (`subcategory=""`).
- `settingsHubCards()` — add cards for Audio, General, Advanced, GameCube, Wii, and
  Recommended, mirroring the standalone's hub-card icons/labels.
- `previewSpec()` — no new previews required (Graphics OSD preview stays); leave empty
  `PreviewSpec` for the new categories.

**Persistence is free** from the base class (`{root}/emulators/libretro/dolphin/options.json`).

## Schema fidelity & testing

All three SP6 guards extend automatically — grow each:

- **Build-time `check_schema_fidelity`** — now globs `CoreOptions*.cpp` (already does), so
  the new modules are picked up. Expect **"90 core keys, 90 host keys, byte-for-byte match"**
  (the Recommended rows merge into their referenced keys). No script change expected; if the
  `opt()` signature differs from the assumed `(cat, group, key, label, default, values,
  tooltip)` order, adjust `HOST_BLOCK_RE` and update the comment.
- **Core unit test `tools/test_core_options.cpp`** — add `Audio::Parse` cases (incl. the
  **DSP fan-out**: `HLE` → `dsp_hle=true`; `LLE Recompiler` → `dsp_hle=false, dsp_jit=true`;
  `LLE Interpreter` → `dsp_hle=false, dsp_jit=false`) and `Core::Parse` cases (enum/combo
  mappings, overclock string→multiplier). Update the `BuildDefinitions` size assertion to
  **91** (90 options + terminator). `Apply` stays compiled out under `CORE_OPTIONS_TEST_ONLY`.
- **Host test `test_dolphin_libretro_schema.cpp`** — extend the shape fixture: new keys carry
  the `dolphin_` prefix, every default ∈ its options, each new hub card's `categoryKey`
  resolves to ≥1 row. **Relax `noDuplicateKeys()`** from its current **global** uniqueness
  check to **per-category** uniqueness — the Recommended card deliberately re-references keys
  that live in other categories, so a key may appear once per category. Add an assertion that
  every Recommended row's key resolves to a non-Recommended row (it is a view, never the home
  of a key).

**Manual smoke (heavier than SP6 — the correctness gotcha):** build the universal dylib,
deploy, and exercise:

- **NTSC + PAL** and **GameCube + Wii** titles — verify each still **boots**.
- Audio: toggle DSP HLE↔LLE Recompiler, change Volume, confirm audible effect.
- Core/Advanced: confirm a game boots with the resolved CPU-core/dual-core defaults; spot-
  check MMU on/off does not regress a known-good title.
- Launch with `RETRONEST_DOLPHIN_LOG=1` and confirm the new boot logs
  **`[CoreOptions] resolved audio: …`** and **`[CoreOptions] resolved core: …`** match the UI
  selections (add these alongside SP6's `resolved graphics:` line).
- Confirm edits persist to `options.json` across a relaunch.

## Error handling

Identical to SP6: host lacking `SET_CORE_OPTIONS_V2` → `EmitCoreOptionsV2` logs once,
defaults still apply; unknown/NULL value at read → `ReadResolved` falls back to the `Values`
default with a WARN; schema drift → caught at build (`check_schema_fidelity`) and test time.

## Known limitation carried forward

GameCube memory cards, Wii NAND, and the SD image live under the core's fixed
`/tmp/dolphin-libretro-user` dir, so saves these settings touch are **not robustly
persistent** until SP8 gives the core a stable RetroNest-managed user directory. The SP7
settings themselves apply correctly; only the on-disk persistence of what they reference is
affected. Documented here as an SP8 packaging item.

## Sequencing (informs the plan)

1. **Resolve symbols/defaults** — subagent pins every `MAIN_*`/`DSP` `Config::Info<T>`
   symbol + type + Dolphin's own default for all 37 keys (removes placeholder risk).
2. **Core `CoreOptionsAudio`** — `AppendDefinitions` + `Values` + `Parse` (incl. DSP
   fan-out) + `Apply`; wire into `CoreOptions.{h,cpp}` `Resolved`/`BuildDefinitions`/
   `ReadResolved` + the `retro_load_game` apply site. Drive `Parse` TDD-style in
   `test_core_options.cpp`.
3. **Core `CoreOptionsCore`** — same, with the four sub-structs (General/Advanced/
   GameCube/Wii).
4. **Boot readback logs** — add `resolved audio:` / `resolved core:` lines.
5. **Host schema** — `opt()` helper + 37 rows + Recommended card + hub cards; extend the
   host shape test.
6. **Fidelity** — run `check_schema_fidelity` to 90/90; fix drift; extend the core unit
   test size assertion + Parse cases.
7. **Build universal dylib, deploy, heavier smoke** on NTSC/PAL × GC/Wii.
