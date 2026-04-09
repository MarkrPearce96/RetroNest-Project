# PPSSPP Settings Audit

**Date:** 2026-04-07
**Scope:** All non-controller `SettingDef`s returned by
`PPSSPPAdapter::settingsSchema()` in
`cpp/src/adapters/ppsspp_adapter.cpp` (lines 57-324).
**Reference:** `references/ppsspp-master/` (PPSSPP master)

Controller binding settings are **out of scope** (separate concern, matches
the PCSX2 / DuckStation audit exclusions). Settings PPSSPP supports but we
don't expose are also out of scope.

---

## Summary

| | Count |
|---|---|
| Total settings audited | 51 |
| OK | 44 |
| WARN | 3 |
| ERROR | 4 |

### Top issues

1. **`Debug Overlay` is in the wrong section AND has the wrong INI key
   spelling.** Our schema writes `[Graphics] iDebugOverlay`, but the native
   `ConfigSetting` lives in `[General]` and uses the literal key
   `DebugOverlay` (no Hungarian `i` prefix) — see
   `references/ppsspp-master/Core/Config.cpp:255`. PPSSPP will never read
   our value. (PPSSPP also flags this setting `CfgFlag::DONT_SAVE`, so it
   won't persist anyway, but the schema is at least writing to a key that
   doesn't exist as far as PPSSPP is concerned.)

2. **`Alternative Speed (%)` and `Alternative Speed 2 (%)` write the wrong
   units.** PPSSPP's `iFpsLimit1` / `iFpsLimit2` are stored as **frames per
   second**, not as a percentage. Native UI converts the user-entered
   percent into FPS via `g_Config.iFpsLimit1 = (percent * 60) / 100` (see
   `references/ppsspp-master/UI/GameSettingsScreen.cpp:1677`). Our adapter
   writes the percent value directly (`200 → FrameRate=200`), which PPSSPP
   reads as a 200 FPS cap rather than 200% of native. The label, suffix
   and stored value are all wrong relative to native semantics.

3. **`Sound > AudioBufferSize` lower bound is too high.** PPSSPP's native
   slider is 0..2048 (`UI/GameSettingsScreen.cpp:748`); SDL clamps to a
   minimum of 128 internally. Our schema enforces 64..2048 with step 64,
   so users can pick 64 (which SDL silently bumps to 128). Functionally
   harmless but the lower bound is misleading.

---

## Findings (grouped by category)

Only settings with WARN / ERROR / INFO findings are listed. All OK
settings are enumerated in the appendix.

### Graphics — Emulation

#### Debug Overlay — `[Graphics] iDebugOverlay`
- **Severity:** ERROR
- (Listed under "Overlay" in the schema, but discussed here because the
  bug is the section/key, not the combo values.)
- **Our schema:** Combo, section `Graphics`, key `iDebugOverlay`,
  default `"0"`, options `0..3, 5..9`.
- **Native source:** Declared as
  `ConfigSetting("DebugOverlay", SETTING(g_Config, iDebugOverlay), 0, CfgFlag::DONT_SAVE)`
  inside `generalSettings[]` (`references/ppsspp-master/Core/Config.cpp:255`).
  The ConfigSection metadata at `Core/Config.cpp:1147` maps
  `generalSettings` → `[General]`. Native field is `int`, no translator,
  default `0`. The integer values for the combo (0=Off,
  1=Debug Stats, 2=Frame Graph, 3=Frame Timing, 5=Control, 6=Audio,
  7=GPU Profile, 8=GPU Allocator, 9=Framebuffer List) match
  `enum class DebugOverlay` in `Core/ConfigValues.h`. Note that
  `CfgFlag::DONT_SAVE` means PPSSPP never writes this key back to the
  INI on shutdown — our tooltip already calls this out — but the load
  path will read it.
- **Recommended fix:** Move the SettingDef into section `General` and
  rename the key from `iDebugOverlay` to `DebugOverlay` (drop the
  Hungarian `i` prefix to match what `ConfigSetting` registers).

---

### Graphics — Frame Pacing

#### Alternative Speed (%) — `[Graphics] FrameRate`
- **Severity:** ERROR
- **Our schema:** Int, default `"0"`, range `0..300` step `10`,
  suffix `%`, slider.
- **Native source:** `iFpsLimit1` is an integer **FPS cap**, not a
  percentage. Declared at `Core/Config.cpp:726`
  (`ConfigSetting("FrameRate", SETTING(g_Config, iFpsLimit1), 0, CfgFlag::PER_GAME)`).
  The native settings UI presents it as a percentage but converts in/out
  using PSP's 60 Hz base rate:
  - Read: `iAlternateSpeedPercent1_ = (g_Config.iFpsLimit1 * 100) / 60`
    (`UI/GameSettingsScreen.cpp:125`).
  - Write: `g_Config.iFpsLimit1 = (iAlternateSpeedPercent1_ * 60) / 100`
    (`UI/GameSettingsScreen.cpp:1677`).
  Our adapter writes the percentage value directly. A user picking
  "200" gets a 200 FPS cap, not 200% of 60 (which would be 120). A user
  picking "100" gets a 100 FPS cap rather than the intended 60.
- **Recommended fix:** Either (a) change the label/suffix to "FPS Cap"
  and adjust the range to native FPS values (0..960 or so), or (b) keep
  the percent label but convert on write/read by multiplying by 60/100
  before writing and back when displaying. Default `0` (unlimited) is
  fine in either case.

#### Alternative Speed 2 (%) — `[Graphics] FrameRate2`
- **Severity:** ERROR
- **Our schema:** Int, default `"-1"`, range `-1..300` step `10`,
  suffix `%`, slider.
- **Native source:** Same as `FrameRate`. `iFpsLimit2` declared at
  `Core/Config.cpp:727`, default `-1`. Stored as raw FPS, native UI
  translates to/from percent via the same 60-base formula
  (`UI/GameSettingsScreen.cpp:130`).
- **Recommended fix:** Same fix as `FrameRate`. Default `-1`
  (disabled) is correct.

---

### Audio — Audio backend

#### Buffer Size — `[Sound] AudioBufferSize`
- **Severity:** WARN
- **Our schema:** Int, default `"256"`, range `64..2048`, step `64`.
- **Native source:** Declared at `Core/Config.cpp:822` with default
  `256`. Native UI slider exposes the full range `0..2048`
  (`UI/GameSettingsScreen.cpp:748`). The SDL backend then enforces a
  minimum of `128` via
  `fmt.samples = std::max(g_Config.iSDLAudioBufferSize, 128)`
  (`SDL/SDLMain.cpp:150`, also `Qt/QtMain.cpp:91`).
- **Recommended fix:** Change `minVal` to `128` (matches the SDL clamp;
  selecting `64` is silently ignored). Step `64` is fine.

---

### Graphics — Rendering

#### Rendering Resolution — `[Graphics] InternalResolution`
- **Severity:** WARN
- **Our schema:** Combo, default `"2"`, options `Auto(0)..10x(10)`.
- **Native source:** `Core/Config.cpp:719`, default
  `&DefaultInternalResolution`. On the macOS SDL build (not
  `USING_WIN_UI` or `USING_QT_UI`), the default is computed at runtime
  from the host display: `2` if longest display side ≥ 1000 px,
  else `1` (`Core/Config.cpp:413-415`). Our hard-coded `2` is right for
  large desktop monitors but slightly wrong for sub-HD displays.
- **Recommended fix:** Acceptable as-is for the intended desktop use
  case. Optionally drop to `"1"` to match the smallest-display fallback,
  or use `"0"` ("Auto (1:1)") for safest neutral behaviour.

---

### Audio — Game volume

#### Achievement Sound Volume — `[Sound] AchievementVolume`
- **Severity:** WARN
- **Our schema:** Int, default `"75"`, range `0..100`, step `5`.
- **Native source:** `Core/Config.cpp:838` with default
  `&DefaultAchievementVolume`. The default is computed at runtime as
  `MultiplierToVolume100((float)iLegacyAchievementVolume / 10.0f)`
  with legacy default `6` (`Core/Config.cpp:807, 832`). The newer
  RetroAchievements UI uses `MultiplierToVolume100(0.6f)` directly
  (`UI/RetroAchievementScreens.cpp:394`). Range `VOLUME_OFF..VOLUMEHI_FULL`
  = `0..100` matches our schema. The static `"75"` we ship is close to
  the dynamically computed default but not exact.
- **Recommended fix:** Cosmetic only — leave as `"75"` or document
  that the upstream default depends on a logarithmic mapping.

---

## Bitmask verification

### `[Graphics] iShowStatusFlags`

Three bitmask checkboxes share this int key. Verified against
`enum class ShowStatusFlags` in
`references/ppsspp-master/Core/ConfigValues.h:211-215`:

| Schema label | Schema bitmask | Native enum value | Status |
|---|---|---|---|
| Show FPS Counter | `2` | `FPS_COUNTER = 1 << 1 = 2` | OK |
| Show Speed | `4` | `SPEED_COUNTER = 1 << 2 = 4` | OK |
| Show Battery % | `8` | `BATTERY_PERCENT = 1 << 3 = 8` | OK |

All three bits are read in
`UI/EmuScreen.cpp:1979`, `UI/DebugOverlay.cpp:477-489`, and
`SDL/CocoaBarItems.mm:196-203` via `iShowStatusFlags & (int)ShowStatusFlags::*`.
The schema covers exactly the three defined bits with no gaps or overlap.
The default `"0"` matches `Core/Config.cpp:695` which declares the field
default to `0`. The INI key `iShowStatusFlags` (with the Hungarian `i`
prefix) is exactly what `ConfigSetting` registers — this is one of
PPSSPP's rare cases where the INI key includes the prefix.

---

## ConfigTranslator verification

Only one combo in our schema maps to a `ConfigTranslator`-wrapped enum:

### `[Graphics] GraphicsBackend` (combo "Backend")
- Native: `ConfigSetting("GraphicsBackend", SETTING(g_Config, iGPUBackend), &DefaultGPUBackend, &GPUBackendTranslator::To, &GPUBackendTranslator::From, ...)`
  at `Core/Config.cpp:696`. Translator at `Core/Config.cpp:606-620`.
- The `To` function writes
  `StringFromInt(v) + " (" + GPUBackendToString(v) + ")"`, e.g.
  `"3 (VULKAN)"`, `"0 (OPENGL)"`, `"2 (DIRECT3D11)"`
  (`Core/Config.cpp:75-86`).
- Our schema option values use the **exact** same format
  (`"0 (OPENGL)"`, `"2 (DIRECT3D11)"`, `"3 (VULKAN)"`) and default
  `"3 (VULKAN)"`. **Round-trip OK.**

No other audited combo passes through `ConfigTranslator`. All other
int-typed combos (`IOTimingMethod`, `MultiSampleLevel`, `FrameSkip`,
`InflightFrames`, `SkipGPUReadbackMode`, `SplineBezierQuality`,
`BloomHack`, `DepthRasterMode`, `TexScalingType`, `TexScalingLevel`,
`AnisotropyLevel`, `TextureFiltering`, `AudioSyncMode`,
`InternalResolution`, `iDebugOverlay`) are written as bare integers
and our schema uses bare integer string values that match — no
parenthesised suffix expected.

---

## Appendix: Every audited setting

Legend: `OK` = round-trip verified; `WARN` / `ERROR` / `INFO` = see
Findings.

### Graphics — Emulation
| Label | Section / Key | Status |
|---|---|---|
| Fast Memory (Unstable) | `[CPU] FastMemoryAccess` | OK |
| Ignore Bad Memory Accesses | `[General] IgnoreBadMemAccess` | OK |
| I/O Timing Method | `[CPU] IOTimingMethod` | OK |
| Force Real Clock Sync | `[General] ForceLagSync2` | OK |
| CPU Clock (MHz) | `[CPU] CPUSpeed` | OK |

### Graphics — Rendering
| Label | Section / Key | Status |
|---|---|---|
| Backend | `[Graphics] GraphicsBackend` | OK (translator round-trip verified) |
| Rendering Resolution | `[Graphics] InternalResolution` | WARN (default depends on display) |
| Software Rendering | `[Graphics] SoftwareRenderer` | OK |
| Antialiasing (MSAA) | `[Graphics] MultiSampleLevel` | OK |
| Replace Textures | `[Graphics] ReplaceTextures` | OK |

### Graphics — Frame Pacing
| Label | Section / Key | Status |
|---|---|---|
| VSync | `[Graphics] VerticalSync` | OK |
| Frame Skipping | `[Graphics] FrameSkip` | OK |
| Auto Frameskip | `[Graphics] AutoFrameSkip` | OK |
| Alternative Speed (%) | `[Graphics] FrameRate` | **ERROR** (wrong units — FPS, not %) |
| Alternative Speed 2 (%) | `[Graphics] FrameRate2` | **ERROR** (wrong units — FPS, not %) |
| Render Duplicate Frames to 60 Hz | `[Graphics] RenderDuplicateFrames` | OK |

### Graphics — Performance
| Label | Section / Key | Status |
|---|---|---|
| Buffer Graphics Commands | `[Graphics] InflightFrames` | OK |
| Hardware Transform | `[Graphics] HardwareTransform` | OK |
| Software Skinning | `[Graphics] SoftwareSkinning` | OK |
| Hardware Tessellation | `[Graphics] HardwareTessellation` | OK |
| Skip Buffer Effects | `[Graphics] SkipBufferEffects` | OK |
| Disable Culling | `[Graphics] DisableRangeCulling` | OK |
| Skip GPU Readbacks | `[Graphics] SkipGPUReadbackMode` | OK |
| Lazy Texture Caching | `[Graphics] TextureBackoffCache` | OK |
| Spline/Bezier Curves Quality | `[Graphics] SplineBezierQuality` | OK |
| Lower Resolution for Effects | `[Graphics] BloomHack` | OK |
| Lens Flare Occlusion | `[Graphics] DepthRasterMode` | OK |

### Graphics — Textures
| Label | Section / Key | Status |
|---|---|---|
| GPU Texture Upscaler (fast) | `[Graphics] TexHardwareScaling` | OK |
| Upscale Type | `[Graphics] TexScalingType` | OK |
| Upscale Level | `[Graphics] TexScalingLevel` | OK |
| Deposterize | `[Graphics] TexDeposterize` | OK |
| Anisotropic Filtering | `[Graphics] AnisotropyLevel` | OK |
| Texture Filtering | `[Graphics] TextureFiltering` | OK |
| Smart 2D Texture Filtering | `[Graphics] Smart2DTexFiltering` | OK |

### Graphics — Post-Processing
| Label | Section / Key | Status |
|---|---|---|
| Post-Processing Shader | `[PostShaderList] PostShader1` | OK (Off + 22 shader names verified against `assets/shaders/defaultshaders.ini`) |

### Audio — Audio playback
| Label | Section / Key | Status |
|---|---|---|
| Playback Mode | `[Sound] AudioSyncMode` | OK |
| Fill Audio Gaps | `[Sound] FillAudioGaps` | OK |

### Audio — Game volume
| Label | Section / Key | Status |
|---|---|---|
| Enable Sound | `[Sound] Enable` | OK |
| Game Volume | `[Sound] GameVolume` | OK |
| Reverb Volume | `[Sound] ReverbRelativeVolume` | OK |
| Alternate Speed Volume | `[Sound] AltSpeedRelativeVolume` | OK |
| Achievement Sound Volume | `[Sound] AchievementVolume` | WARN (default is computed from legacy field) |

### Audio — UI sound
| Label | Section / Key | Status |
|---|---|---|
| UI Sound | `[General] UISound` | OK |
| UI Volume | `[Sound] UIVolume` | OK |
| Game Preview Volume | `[Sound] GamePreviewVolume` | OK |

### Audio — Audio backend
| Label | Section / Key | Status |
|---|---|---|
| Buffer Size | `[Sound] AudioBufferSize` | WARN (lower bound below SDL minimum) |
| Use New Audio Devices Automatically | `[Sound] AutoAudioDevice` | OK |

### Overlay (status flag bitmasks + debug overlay)
| Label | Section / Key | Status |
|---|---|---|
| Show FPS Counter | `[Graphics] iShowStatusFlags` (bit 2) | OK |
| Show Speed | `[Graphics] iShowStatusFlags` (bit 4) | OK |
| Show Battery % | `[Graphics] iShowStatusFlags` (bit 8) | OK |
| Debug Overlay | `[Graphics] iDebugOverlay` | **ERROR** (wrong section + wrong key — should be `[General] DebugOverlay`) |
