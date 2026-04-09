# PPSSPP Graphics Settings Reorganization — Design

**Date:** 2026-04-07
**Scope:** PPSSPP adapter only. No PCSX2 / DuckStation changes.

## Goal

Reorganize the PPSSPP settings UI for easier navigation, add settings that are
present in native PPSSPP but missing from our app, replace the unimplemented
"Display layout & effects" placeholder with a Post-Processing shader picker, and
move the Show FPS / Speed / Battery overlay options into their own sidebar tab.

## Changes at a glance

1. Sidebar collapses to **Graphics · Audio · Overlay** (was: Emulation, Graphics,
   Audio, Controllers, …). Emulation moves into Graphics as a sub-tab.
2. Graphics gets six horizontal sub-tabs with cleaner groupings.
3. Three settings added: **Lens Flare Occlusion**, **Post-Processing Shader**,
   **Debug Overlay**.
4. New **Overlay** sidebar entry with **Show FPS / Show Speed / Show Battery %**
   plus the Debug Overlay combo.
5. Small framework change: `SettingDef` gains an optional `bitmask` field so a
   `Bool` widget can target one bit of an int-valued INI key (needed because
   PPSSPP packs the three overlay flags into `iShowStatusFlags`).

## Sidebar and sub-tab layout

### Sidebar (3 entries)

| Entry    | Notes                                                  |
|----------|--------------------------------------------------------|
| Graphics | 6 horizontal sub-tabs (see below)                      |
| Audio    | 4 sub-tabs unchanged                                   |
| Overlay  | New. Single page, no sub-tabs.                         |

Other categories (Controllers, Hotkeys, etc.) are not affected — this work only
touches what the PPSSPP adapter declares via `settingsSchema()`.

### Graphics sub-tabs (left → right)

1. **Emulation** — moved from sidebar
2. **Rendering**
3. **Frame Pacing**
4. **Performance** *(one tab containing both perf-tuning and speed-hack groups)*
5. **Textures**
6. **Post-Processing** — new

## Settings per sub-tab

The keys below come from PPSSPP's `Core/Config.cpp`. Section is `[Graphics]`
unless noted.

### Graphics → Emulation
*(unchanged settings, moved category from `Emulation` to `Graphics`,
subcategory `Emulation`)*

| Key                  | Section  | Type | Label                            |
|----------------------|----------|------|----------------------------------|
| FastMemoryAccess     | CPU      | Bool | Fast Memory (Unstable)           |
| IgnoreBadMemAccess   | General  | Bool | Ignore Bad Memory Accesses       |
| IOTimingMethod       | CPU      | Combo| I/O Timing Method                |
| ForceLagSync2        | General  | Bool | Force Real Clock Sync            |
| CPUSpeed             | CPU      | Int  | CPU Clock (MHz)                  |

### Graphics → Rendering

| Key                | Type  | Label                                     |
|--------------------|-------|-------------------------------------------|
| GraphicsBackend    | Combo | Backend                                   |
| InternalResolution | Combo | Rendering Resolution                      |
| SoftwareRenderer   | Bool  | Software Rendering (slow, accurate)       |
| MultiSampleLevel   | Combo | Antialiasing (MSAA)                       |
| ReplaceTextures    | Bool  | Replace Textures                          |

**Fullscreen toggle is intentionally not exposed.** Our app's hard rule is one
window — `createDefaultConfig` already forces `FullScreen=True`. Letting users
toggle it would break embedding.

### Graphics → Frame Pacing

| Key                   | Type  | Label                                |
|-----------------------|-------|--------------------------------------|
| VerticalSync          | Bool  | VSync                                |
| FrameSkip             | Combo | Frame Skipping                       |
| AutoFrameSkip         | Bool  | Auto Frameskip                       |
| FrameRate             | Int   | Alternative Speed (%)                |
| FrameRate2            | Int   | Alternative Speed 2 (%)              |
| RenderDuplicateFrames | Bool  | Render Duplicate Frames to 60 Hz     |

### Graphics → Performance

Two visual groups inside one tab via the `group` field on `SettingDef`.

**Group "Performance":**

| Key                  | Type  | Label                          |
|----------------------|-------|--------------------------------|
| InflightFrames       | Combo | Buffer Graphics Commands       |
| HardwareTransform    | Bool  | Hardware Transform             |
| SoftwareSkinning     | Bool  | Software Skinning              |
| HardwareTessellation | Bool  | Hardware Tessellation          |

**Group "Speed Hacks":**

| Key                  | Type  | Label                                | Notes |
|----------------------|-------|--------------------------------------|-------|
| SkipBufferEffects    | Bool  | Skip Buffer Effects                  |       |
| DisableRangeCulling  | Bool  | Disable Culling                      |       |
| SkipGPUReadbackMode  | Combo | Skip GPU Readbacks                   |       |
| TextureBackoffCache  | Bool  | Lazy Texture Caching                 |       |
| SplineBezierQuality  | Combo | Spline/Bézier Curves Quality         |       |
| BloomHack            | Combo | Lower Resolution for Effects         |       |
| **DepthRasterMode**  | Combo | **Lens Flare Occlusion** *(new)*     | Auto=0, Low=1, Off=2, Always on=3 |

### Graphics → Textures

| Key                | Type  | Label                          |
|--------------------|-------|--------------------------------|
| TexHardwareScaling | Bool  | GPU Texture Upscaler (fast)    |
| TexScalingType     | Combo | Upscale Type                   |
| TexScalingLevel    | Combo | Upscale Level                  |
| TexDeposterize     | Bool  | Deposterize                    |
| AnisotropyLevel    | Combo | Anisotropic Filtering          |
| TextureFiltering   | Combo | Texture Filtering              |
| Smart2DTexFiltering| Bool  | Smart 2D Texture Filtering     |

### Graphics → Post-Processing *(new)*

Single combo. Section `[PostShaderList]`, key `PostShader1`. Replaces the
unimplemented "Display layout & effects" entry from native PPSSPP.

Options derived from `references/ppsspp-master/assets/shaders/defaultshaders.ini`
(use the `Name=` field for display, the section header for the value):

| Display              | INI value          |
|----------------------|--------------------|
| Off                  | `Off`              |
| FXAA Antialiasing    | `FXAA`             |
| CRT Scanlines        | `CRT`              |
| Natural Colors       | `Natural`          |
| Natural (No Blur)    | `NaturalA`         |
| Vignette             | `Vignette`         |
| Fake Reflections     | `FakeReflections`  |
| Bloom                | `Bloom`            |
| Bloom (no blur)      | `BloomNoBlur`      |
| Sharpen              | `Sharpen`          |
| Scanlines (CRT)      | `Scanlines`        |
| Cartoon              | `Cartoon`          |
| 4xHqGLSL Upscaler    | `4xHqGLSL`         |
| AA-Color             | `AAColor`          |
| Bicubic Upscaler     | `UpscaleBicubic`   |
| Spline36 Upscaler    | `UpscaleSpline36`  |
| 5xBR Upscaler        | `5xBR`             |
| 5xBR lv2 Upscaler    | `5xBR-lv2`         |
| Color Correction     | `ColorCorrection`  |
| PSP Color            | `PSPColor`         |
| LCD Persistence      | `LCDPersistence`   |
| Sharp Bilinear       | `UpscaleSharpBilinear` |
| FSR-EASU             | `FSR-EASU`         |

Default: `Off`. A chain of multiple shaders is **not** supported in this
iteration; we expose only `PostShader1`.

### Overlay *(new sidebar entry)*

| Key              | Section  | Type  | Label             | Notes |
|------------------|----------|-------|-------------------|-------|
| iShowStatusFlags | Graphics | Bool  | Show FPS Counter  | bitmask=2 |
| iShowStatusFlags | Graphics | Bool  | Show Speed        | bitmask=4 |
| iShowStatusFlags | Graphics | Bool  | Show Battery %    | bitmask=8 |
| iDebugOverlay    | Graphics | Combo | Debug Overlay     | Off=0, Debug Stats=1, Frame Graph=2, Frame Timing=3, Control=5, Audio=6, GPU Profile=7, GPU Allocator=8, Framebuffer List=9 |

**Note for Debug Overlay tooltip:** PPSSPP marks `iDebugOverlay` as
`CfgFlag::DONT_SAVE`, so PPSSPP itself does not persist this value across
runs. We can still write it from our UI, but the user should know it will not
be preserved if they edit the file from PPSSPP's own UI.

The four overlay rows live under category `Overlay`, no subcategory, no group.
Bit values come from `references/ppsspp-master/Core/ConfigValues.h` enum
`ShowStatusFlags { FPS_COUNTER = 1<<1, SPEED_COUNTER = 1<<2, BATTERY_PERCENT = 1<<3 }`.

## Framework change: `SettingDef::bitmask`

`cpp/src/core/setting_def.h` gains one optional field at the end of the struct
so existing call sites in PCSX2 and DuckStation adapters keep compiling
unchanged:

```cpp
// If non-zero, this Bool setting reads/writes a single bit of an
// int-valued INI key. The widget displays as a checkbox; on save the
// bit is set/cleared in the existing int and the full int is written
// back. Used by PPSSPP for iShowStatusFlags. Default 0 = normal Bool.
int bitmask = 0;
```

`cpp/src/ui/settings/emulator_settings_page.cpp` checkbox handling needs two
small additions:

**On load (around line 452, the Bool branch):**
- If `def.bitmask != 0`, parse the stored value as int (default to
  `def.defaultValue.toInt()` if empty), set the checkbox to
  `(intVal & def.bitmask) != 0`.

**On save (the same widget's `toggled` connection):**
- If `def.bitmask != 0`, re-read the current int value from
  `m_appController->settingValue(...)`, set or clear the bit, then
  `setSettingValue(...)` with the new int as a string.
- Re-reading on every toggle (rather than caching) means three checkboxes
  bound to the same key all merge correctly even if the user toggles them in
  rapid succession.

PCSX2 and DuckStation are not touched. Their `settingsSchema()` calls leave
`bitmask` at its default of 0 and behave exactly as before.

## What stays the same

- Audio settings, Audio sub-tabs.
- Controller mapping, hotkeys, RA settings, BIOS browser.
- `ensureConfig`, `createDefaultConfig`, `patchExistingConfig`.
- All controller / binding code.
- PCSX2 and DuckStation adapters and schemas.

## Files touched

- `cpp/src/core/setting_def.h` — add `bitmask` field
- `cpp/src/ui/settings/emulator_settings_page.cpp` — bitmask read/write paths
  inside the Bool widget branch
- `cpp/src/adapters/ppsspp_adapter.cpp` — full rewrite of `settingsSchema()`
  reflecting the new categorisation, new settings, and Overlay entries

No new files, no header changes beyond `setting_def.h`.

## Out of scope

- Multi-shader chain (PostShader2..N) — single-shader picker only.
- Exposing the Fullscreen toggle.
- Touching PCSX2 or DuckStation in any way.
- Changing `ensureConfig` / `createDefaultConfig` — none of the new settings
  are required for embedding, so they're UI-only additions.
- Changing the existing default values for any of the moved settings — the
  user has not asked for behaviour changes, only reorganisation.

## Risks and mitigations

- **Bitmask race / merge bug.** If the in-memory cache isn't refreshed before
  toggling each checkbox, the second toggle could clobber the first. Mitigation:
  re-read from `settingValue(...)` on every toggle, not from cached widget state.
- **Stale `iDebugOverlay` value.** PPSSPP itself does not persist this. Mitigation:
  document in tooltip; do not treat its absence on next read as a bug.
- **Shader value not round-tripping.** PPSSPP writes shader names as bare strings
  in `[PostShaderList]`, so our combo values match exactly — no `ConfigTranslator`
  format weirdness. Verify after first save.
