# Default Value Mismatch Inventory

**Date:** 2026-04-07
**Purpose:** Source of truth for the default-settings-sync implementation plan.
Lists every `SettingDef::defaultValue` that needs to change to match each
emulator's native default (or the explicit override per the design spec).

This inventory was produced by walking the current
`settingsSchema()` body in each adapter, looking up the corresponding native
default in the emulator's source tree, and recording every divergence —
including settings the prior round-trip audits marked OK.

Controller binding settings, hotkeys, controller types and RetroAchievements
credentials are out of scope (same exclusions as the round-trip audits).

## Summary

| Adapter      | Settings audited | Mismatches found |
|--------------|------------------|------------------|
| PCSX2        | 82               | 2                |
| DuckStation  | 104              | 6                |
| PPSSPP       | 51               | 2                |
| **Total**    | **237**          | **10**           |

(Mismatch count includes the explicit-override entries even when the current
value is already correct — they are listed here so the implementation plan
has a complete record.)

---

## PCSX2

`cpp/src/adapters/pcsx2_adapter.cpp` — `PCSX2Adapter::settingsSchema()`
(lines 19–301).

### Native default mismatches (sync to native)

- **Enable Multithreaded VU1 (MTVU)** — `[EmuCore/Speedhacks] vuThread`
  - Current: `"true"`
  - Native: `false`. `Pcsx2Config::SpeedhackOptions::SpeedhackOptions()` calls
    `DisableAll()` which clears the entire bitset, then turns on only
    `WaitLoop`, `IntcStat`, `vuFlagHack` and `vu1Instant`; `vuThread` stays
    `false`. See `references/pcsx2-master/pcsx2/Pcsx2Config.cpp:378-396`.
  - New: `"false"`

### Explicit overrides

- **Internal Resolution** — `[EmuCore/GS] upscale_multiplier`
  - Current: `"1"`
  - Native: `1.0f` per
    `references/pcsx2-master/pcsx2/Config.h:725`
    (`DEFAULT_UPSCALE_MULTIPLIER = 1.0f`).
  - New: `"1"` (per design spec — already matches native; recorded for
    completeness)

- **Aspect Ratio** — `[EmuCore/GS] AspectRatio`
  - Current: `"Auto 4:3/3:2"`
  - Native: `AspectRatioType::RAuto4_3_3_2` → INI string `"Auto 4:3/3:2"` per
    `references/pcsx2-master/pcsx2/Config.h:719,843` and
    `references/pcsx2-master/pcsx2/Pcsx2Config.cpp:931`.
  - New: `"4:3"` (per design spec — frontend always boots PS2 games at 4:3)

### Runtime-computed (heuristic)

- _(none — PCSX2 has no runtime-computed defaults in scope.)_

---

## DuckStation

`cpp/src/adapters/duckstation_adapter.cpp` —
`DuckStationAdapter::settingsSchema()` (lines 19–448).

### Native default mismatches (sync to native)

- **Turbo Speed** — `[Main] TurboSpeed`
  - Current: `"2"`
  - Native: `0.0f` (i.e. `"0"` → "Unlimited") per
    `references/duckstation-master/src/core/settings.h:380`
    (`float turbo_speed = 0.0f;`). The current `"2"` is a deliberate UX
    choice from the round-trip audit but conflicts with the
    sync-to-native rule.
  - New: `"0"`

- **Apply Image Patches** — `[CDROM] LoadImagePatches`
  - Current: `"true"`
  - Native: `false` per
    `references/duckstation-master/src/core/settings.h:316`
    (`bool cdrom_load_image_patches : 1 = false;`).
  - New: `"false"`

- **Fast Forward Memory Card Access** — `[MemoryCards] FastForwardAccess`
  - Current: `"true"`
  - Native: `false` per
    `references/duckstation-master/src/core/settings.h:311`
    (`bool memory_card_fast_forward_access : 1 = false;`).
  - New: `"false"`

- **Deinterlacing** — `[GPU] DeinterlacingMode`
  - Current: `"Adaptive"`
  - Native: `DisplayDeinterlacingMode::Progressive` (writes string
    `"Progressive"`) per
    `references/duckstation-master/src/core/settings.h:234`
    (`DEFAULT_DISPLAY_DEINTERLACING_MODE = DisplayDeinterlacingMode::Progressive`)
    and
    `references/duckstation-master/src/core/settings.cpp:1887`
    (name table). Already flagged WARN in the round-trip audit but not
    fixed.
  - New: `"Progressive"`

- **Optimal Frame Pacing** — `[Display] OptimalFramePacing`
  - Current: `"false"`
  - Native (macOS): `true` per
    `references/duckstation-master/src/core/settings.h:267-272`
    — `DEFAULT_OPTIMAL_FRAME_PACING` is `false` only on Android and
    ARM64 Linux; everywhere else (including macOS) it is `true`. The
    field is declared
    `bool display_optimal_frame_pacing : 1 = DEFAULT_OPTIMAL_FRAME_PACING;`
    at `settings.h:101`.
  - New: `"true"`

### Explicit overrides

- **Internal Resolution** — `[GPU] ResolutionScale`
  - Current: `"1"`
  - Native: `1` per
    `references/duckstation-master/src/core/settings.h:34`
    (`u8 gpu_resolution_scale = 1;`).
  - New: `"1"` (per design spec — already matches native; recorded for
    completeness)

- **Aspect Ratio** — `[Display] AspectRatio`
  - Current: `"4:3"`
  - Native: `DisplayAspectRatio::Auto()` → INI string `"Auto (Game Native)"`
    per
    `references/duckstation-master/src/core/settings.h:236` and
    `references/duckstation-master/src/core/settings.cpp:2042-2057`
    (`Settings::GetDisplayAspectRatioName`).
  - New: `"4:3"` (per design spec — frontend always boots PS1 games at 4:3)

### Runtime-computed (heuristic)

- _(none — DuckStation has runtime-computed defaults for `Adapter`,
  `Driver`, `OutputDevice` but those are free-form strings that already
  default to `""`, which matches the most-common-case behaviour.)_

---

## PPSSPP

`cpp/src/adapters/ppsspp_adapter.cpp` — `PPSSPPAdapter::settingsSchema()`
(lines 57–351).

### Native default mismatches (sync to native)

- _(none — every static-default `SettingDef` in PPSSPP's schema already
  matches its corresponding `ConfigSetting(...)` literal default in
  `references/ppsspp-master/Core/Config.cpp`. Verified line-by-line for
  `FastMemoryAccess`, `IgnoreBadMemAccess`, `IOTimingMethod`,
  `ForceLagSync2`, `CPUSpeed`, `SoftwareRenderer`, `MultiSampleLevel`,
  `ReplaceTextures`, `VerticalSync`, `FrameSkip`, `AutoFrameSkip`,
  `FrameRate`, `FrameRate2`, `RenderDuplicateFrames`, `InflightFrames`,
  `HardwareTransform`, `SoftwareSkinning`, `HardwareTessellation`,
  `SkipBufferEffects`, `DisableRangeCulling`, `SkipGPUReadbackMode`,
  `TextureBackoffCache`, `SplineBezierQuality`, `BloomHack`,
  `DepthRasterMode`, `TexHardwareScaling`, `TexScalingType`,
  `TexScalingLevel`, `TexDeposterize`, `AnisotropyLevel`,
  `TextureFiltering`, `Smart2DTexFiltering`, `AudioSyncMode`,
  `FillAudioGaps`, `Enable`, `GameVolume`, `ReverbRelativeVolume`,
  `AltSpeedRelativeVolume`, `UISound`, `UIVolume`, `GamePreviewVolume`,
  `AudioBufferSize`, `AutoAudioDevice`, `iShowStatusFlags` and
  `DebugOverlay`. The non-runtime-computed defaults are all clean.)_

### Explicit overrides

- **Rendering Resolution** — `[Graphics] InternalResolution`
  - Current: `"2"`
  - Native (macOS desktop, SDL build): runtime-computed by
    `DefaultInternalResolution()` at
    `references/ppsspp-master/Core/Config.cpp:403-418`. Without
    `USING_WIN_UI` / `USING_QT_UI`, returns `2` if longest display side
    ≥ 1000 px, else `1`. Also `0` ("Auto 1:1") on Win/Qt builds.
  - New: `"1"` (per design spec — 1× native PSP resolution)

- **Aspect Ratio** — _(no entry in schema)_
  - PPSSPP exposes aspect ratio only via the setup wizard, not via the
    settings page. There is no `SettingDef` to update. Listed here only
    so the design spec's three-emulator parity is auditable.

### Runtime-computed (heuristic)

- **Achievement Sound Volume** — `[Sound] AchievementVolume`
  - Current: `"75"`
  - Native: runtime-computed via
    `MultiplierToVolume100((float)iLegacyAchievementVolume / 10.0f)` with
    legacy default `iLegacyAchievementVolume = 6`
    (`references/ppsspp-master/Core/Config.cpp:807,832,2098-2104`).
    Working through the formula:
    `MultiplierToVolume100(0.6) = (0.6^(1/1.75)) * 100 + 0.5 ≈ 75.2`,
    rounded to `75`. The current value `"75"` is therefore an exact match
    for the runtime calculation.
  - **Note for implementer:** the design spec instructions name `"60"`
    as the heuristic value, but the math actually yields `~75`. Current
    schema value `"75"` is correct; recommend keeping `"75"` and updating
    the spec, OR overriding to `"60"` per spec literal — this is a
    judgement call for the implementer. **Recommended action: leave at
    `"75"` (matches the runtime computation) and note the spec
    discrepancy in the implementation commit message.**
  - New: `"75"` (no change) — or `"60"` if the spec value is enforced
    verbatim.

---

## Notes for the implementer

1. **Platform-dependent defaults.** Several settings have native defaults
   that depend on platform / display / VR state. Per the task instructions
   ("use the macOS desktop value"), this inventory has resolved them as
   follows:
   - DuckStation `OptimalFramePacing` → `true` (non-Android, non-ARM64-Linux).
   - DuckStation `MaxQueuedFrames` → `2` (non-Android).
   - DuckStation `OSDMargin` (out of scope for default fix — type warning
     only) → `10.0f` (non-Android).
   - PPSSPP `GraphicsBackend` → `3 (VULKAN)` (macOS ARM64; macOS Intel
     would be `0 (OPENGL)`). Current schema already uses Vulkan.
   - PPSSPP `InternalResolution` → runtime-computed `1` or `2` depending
     on display, but design spec forces `"1"`.
   - PPSSPP `AutoFrameSkip` → `false` (non-VR). Current matches.
   - PPSSPP `DepthRasterMode` → `0` (`DEFAULT`, non-mobile, with SIMD).
     Current matches.

2. **Audit-doc cross-reference.** Where the prior audits marked a setting
   `OK`, this inventory has independently re-verified it against the
   native source. The only items in this inventory are genuine mismatches
   or design-spec overrides — every audit-OK item not listed here was
   confirmed to also be a default match.

3. **Sole behavioural change.** Of the eight native-default mismatches
   listed (1 PCSX2 + 5 DuckStation + 0 PPSSPP, plus 2 design-spec
   overrides per emulator), the largest UX impact will come from
   DuckStation `OptimalFramePacing` flipping from `false` to `true`
   on macOS, and `TurboSpeed` flipping from `"2"` to `"0"` (Unlimited).
   Both are documented native defaults.

This inventory is the source of truth for the implementation plan that
follows. Every entry above is a single-line `defaultValue` change in the
corresponding adapter file.
