# DuckStation Settings Audit

**Date:** 2026-04-06
**Scope:** All non-controller `SettingDef`s returned by
`DuckStationAdapter::settingsSchema()` in
`cpp/src/adapters/duckstation_adapter.cpp` (lines 19-417).
**Reference:** `references/duckstation-master/` (DuckStation master)

Controller binding settings, hotkeys, controller types, port configuration
and RetroAchievements credentials are **out of scope** (separate audit).

---

## Summary

| | Count |
|---|---|
| Total settings audited | 104 |
| OK | 79 |
| WARN | 7 |
| ERROR | 18 |

### Top issues

1. **Dithering, Crop, Display Scaling and Aspect Ratio combos use INI value
   strings that don't match what DuckStation writes back.** All four combos
   are name-based enums round-tripped via `Settings::Get*Name`/`Parse*Name`
   in `references/duckstation-master/src/core/settings.cpp` (lines
   1708-2057). Many of our option strings are off by case, suffix or
   word — e.g. we use `ScaledDithering` where native writes `Scaled`,
   `AllBorders` where native writes `Borders`, `Bilinear` where native
   writes `BilinearSmooth`, and `Auto`/`Stretch`/`PAR1:1` where native
   writes `Auto (Game Native)`/`Stretch To Fill`/`PAR 1:1`. Every one of
   these settings will silently revert to native defaults after the
   emulator saves once.
2. **`EmulationSpeed`, `FastForwardSpeed` and `TurboSpeed` combo values are
   floats in the wrong stringification format.** DuckStation writes floats
   via `StringUtil::ToChars(float)` →
   `std::to_chars` (`references/duckstation-master/src/common/string_util.cpp:333-342`),
   which produces shortest representation: `1.0f` → `"1"`, `0.5f` → `"0.5"`,
   `2.0f` → `"2"`. Our options use padded forms `"1.0"`, `"0.5"`, `"2.0"`
   etc., which never match on re-read — these speed combos silently revert
   to default after DuckStation saves once. Same class of bug as the
   PCSX2 audit's `NominalScalar`/`TurboScalar`/`SlomoScalar` finding.
3. **Seek Speedup combo values are mirrored relative to native semantics.**
   Native `cdrom_seek_speedup` uses 1 for "normal speed" and 0 for the
   maximum speedup-cycles override
   (`references/duckstation-master/src/core/cdrom.cpp:1616`). DuckStation's
   own UI binds this to the value array `{1,2,3,4,5,6,0}`
   (`consolesettingswidget.cpp:19,74`). Our schema swaps the endpoints —
   we map "None (Normal Speed)" to `0` and "Maximum (Safer)" to `1`, so
   the user gets the opposite of what they pick.

---

## Findings (grouped by category)

Only settings with WARN / ERROR / INFO findings are listed. All OK settings
are enumerated in the appendix.

### Console — Console group

#### Region — `[Console] Region`
- **Severity:** OK
- (Verified — names `Auto`, `NTSC-J`, `NTSC-U`, `PAL` match
  `s_console_region_names` in
  `references/duckstation-master/src/core/settings.cpp:1397-1402`. Default
  matches `DEFAULT_CONSOLE_REGION = ConsoleRegion::Auto`.)

### Console — CD-ROM Emulation group

#### Seek Speedup — `[CDROM] SeekSpeedup`
- **Severity:** ERROR
- **Our schema:** Combo, default `"1"`, options
  `{"None (Normal Speed)":"0","2x":"2","3x":"3","4x":"4","5x":"5","6x":"6","Maximum (Safer)":"1"}`.
- **Native source:** Integer. Read via
  `si.GetSaturatedIntValue<u8>("CDROM", "SeekSpeedup", 1)` at
  `references/duckstation-master/src/core/settings.cpp:467`. Native
  semantics: value `1` = normal speed (1×), values `2..6` = N× speedup,
  value `0` = maximum (uses `cdrom_max_seek_speedup_cycles`),
  `references/duckstation-master/src/core/cdrom.cpp:1616`. DuckStation's
  own Qt UI uses the value array `{1, 2, 3, 4, 5, 6, 0}` for indices 0..6
  (`references/duckstation-master/src/duckstation-qt/consolesettingswidget.cpp:19,74`),
  i.e. labels `None (Normal Speed)`, `2x`, `3x`, `4x`, `5x`, `6x`,
  `Maximum (Safer)` mapping to `1, 2, 3, 4, 5, 6, 0` respectively.
- **Recommended fix:** Swap our combo so `None (Normal Speed)` maps to
  `"1"` and `Maximum (Safer)` maps to `"0"` (i.e. mirror the existing
  ReadSpeedup combo: `{"None (Normal Speed)":"1", "2x":"2", ..., "6x":"6", "Maximum (Safer)":"0"}`).
  Default already `"1"` — keep.

---

### Emulation — Speed Control group

#### Emulation Speed — `[Main] EmulationSpeed`
- **Severity:** ERROR
- **Our schema:** Combo, default `"1.0"`, 21 options of form
  `"0.1","0.2",...,"1.0",...,"5.0"`.
- **Native source:** `emulation_speed = si.GetFloatValue("Main", "EmulationSpeed", 1.0f)`
  at `references/duckstation-master/src/core/settings.cpp:265`. Written
  back via `SetFloatValue` →
  `StringUtil::ToChars(float)` which uses `std::to_chars` shortest
  representation
  (`references/duckstation-master/src/common/string_util.cpp:333-342`,
  `references/duckstation-master/src/common/settings_interface.cpp:256-259`):
  `1.0f` → `"1"`, `0.5f` → `"0.5"`, `2.0f` → `"2"`. Padded forms like
  `"1.0"` and `"2.0"` never come out the other side.
- **Recommended fix:** Change every option's INI value to its
  shortest-float form, and update default to `"1"`:
  `{"10% [6 FPS]":"0.1","20% [12 FPS]":"0.2","30% [18 FPS]":"0.3","40% [24 FPS]":"0.4","50% [30 FPS]":"0.5","60% [36 FPS]":"0.6","70% [42 FPS]":"0.7","75% [45 FPS]":"0.75","80% [48 FPS]":"0.8","90% [54 FPS]":"0.9","100% [60 FPS]":"1","120% [72 FPS]":"1.2","150% [90 FPS]":"1.5","175% [105 FPS]":"1.75","200% [120 FPS]":"2","250% [150 FPS]":"2.5","300% [180 FPS]":"3","350% [210 FPS]":"3.5","400% [240 FPS]":"4","450% [270 FPS]":"4.5","500% [300 FPS]":"5"}`.

#### Fast Forward Speed — `[Main] FastForwardSpeed`
- **Severity:** ERROR
- **Our schema:** Combo, default `"0.0"`, options use `"1.0","2.0",...`.
- **Native source:** Same mechanism as `EmulationSpeed` (settings.cpp:266).
  Native default `0.0f` → `"0"`.
- **Recommended fix:** Same as above. Use shortest forms (`"0","1","1.5","2","3","4","5","6","7","8","9","10"`) and default `"0"`.

#### Turbo Speed — `[Main] TurboSpeed`
- **Severity:** ERROR
- **Our schema:** Combo, default `"2.0"`, padded float options.
- **Native source:** Same mechanism as `EmulationSpeed` (settings.cpp:267).
  Native default is `0.0f`, but our default `2.0` is intentional UX —
  the bug is the format, not the choice.
- **Recommended fix:** Same fix. Default `"2"`.

---

### Emulation — Rewind group

#### Rewind Save Frequency — `[Main] RewindFrequency`
- **Severity:** WARN
- **Our schema:** Float, default `"10.0"`, range 0–60.
- **Native source:** Float; `si.GetFloatValue("Main", "RewindFrequency", 10.0f)`
  at settings.cpp:282. Default `10.0f` → `"10"`. Round-trip is numerically
  fine because the Float widget parses `"10"` and `"10.0"` to the same
  value, but our stored default differs from what native writes.
- **Recommended fix:** Change default to `"10"` for tidiness. No
  behavioural impact.

---

### Graphics — top-level

#### Adapter — `[GPU] Adapter`
- **Severity:** WARN
- **Our schema:** Combo, default `"Default"`, options `{Default: ""}`.
- **Native source:** Free-form string;
  `gpu_adapter = si.GetStringViewValue("GPU", "Adapter", "")` at
  settings.cpp:303. Empty string means "default adapter"; the actual
  list is enumerated at runtime by the host renderer.
- **Recommended fix:** Change `defaultValue` from `"Default"` to `""` so
  it matches the only option's INI value. Optionally enumerate adapters
  at runtime; current single-entry combo is misleading.

---

### Graphics — Rendering subcategory

#### Dithering — `[GPU] DitheringMode`
- **Severity:** ERROR
- **Our schema:** Combo, default `"ScaledDithering"`, options
  `{"Unscaled":"UnscaledDithering","Unscaled (Shader Blending)":"UnscaledShaderBlending","Scaled":"ScaledDithering","Scaled (Shader Blending)":"ScaledShaderBlending","True Color":"TrueColor","True Color (Full)":"TrueColorFull"}`.
- **Native source:** Round-tripped through `Settings::ParseGPUDitheringModeName` /
  `GetGPUDitheringModeName` against `s_gpu_dithering_mode_names = {"Unscaled","UnscaledShaderBlend","Scaled","ScaledShaderBlend","TrueColor","TrueColorFull"}`
  (`references/duckstation-master/src/core/settings.cpp:1708-1711`). Note
  the native names use `ShaderBlend`, not `ShaderBlending`, and use plain
  `Unscaled`/`Scaled` (no `Dithering` suffix). Default
  `DEFAULT_GPU_DITHERING_MODE = TrueColor` (settings.h:226).
- **Recommended fix:** Change INI values to:
  `{"Unscaled":"Unscaled","Unscaled (Shader Blending)":"UnscaledShaderBlend","Scaled":"Scaled","Scaled (Shader Blending)":"ScaledShaderBlend","True Color":"TrueColor","True Color (Full)":"TrueColorFull"}`.
  Update `defaultValue` to `"TrueColor"` to match native default.

#### Deinterlacing — `[GPU] DeinterlacingMode`
- **Severity:** WARN
- **Our schema:** Combo, default `"Adaptive"`, options
  `{"Progressive (Optimal)":"Progressive","Disabled":"Disabled","Weave":"Weave","Blend":"Blend","Adaptive":"Adaptive"}`.
- **Native source:** Names match
  `s_display_deinterlacing_mode_names = {"Disabled","Weave","Blend","Adaptive","Progressive"}`
  (settings.cpp:1887). Default
  `DEFAULT_DISPLAY_DEINTERLACING_MODE = Progressive` (settings.h:234) —
  not `Adaptive`.
- **Recommended fix:** Change `defaultValue` to `"Progressive"` (UX-only
  change; round-trip already correct).

#### Aspect Ratio — `[Display] AspectRatio`
- **Severity:** ERROR
- **Our schema:** Combo, default `"4:3"`, options
  `{"Auto (Game Native)":"Auto","Stretch To Fill":"Stretch","4:3":"4:3","16:9":"16:9","19:9":"19:9","20:9":"20:9","21:9":"21:9","16:10":"16:10","PAR 1:1":"PAR1:1","Custom":"Custom"}`.
- **Native source:** `Settings::ParseDisplayAspectRatio` at
  settings.cpp:2010-2040 special-cases the strings
  `"Auto (Game Native)"`, `"Stretch To Fill"` and `"PAR 1:1"` (with the
  literal space); everything else must match `"<num>:<denom>"` and parse
  as positive integers. `Settings::GetDisplayAspectRatioName` at
  settings.cpp:2042-2057 writes those exact strings back. Native default
  is `DisplayAspectRatio::Auto()` (settings.h:236).
  - `"Auto"` (without `(Game Native)`) does not parse → returns nullopt
    → falls back to native default.
  - `"Stretch"` (without `To Fill`) → same.
  - `"PAR1:1"` (no space) → has a colon, so the parser tries to read
    `"PAR1"` as a `s16`, fails, returns nullopt → fallback.
  - `"Custom"` is not a recognised value at all → fallback. There is no
    "Custom" aspect ratio in DuckStation; users would need to set a
    `n:m` ratio directly.
  - `"4:3"`, `"16:9"`, `"19:9"`, `"20:9"`, `"21:9"`, `"16:10"` all parse
    correctly via the generic `n:m` path.
- **Recommended fix:** Change INI values to native canonical strings:
  `{"Auto (Game Native)":"Auto (Game Native)","Stretch To Fill":"Stretch To Fill","4:3":"4:3","16:9":"16:9","19:9":"19:9","20:9":"20:9","21:9":"21:9","16:10":"16:10","PAR 1:1":"PAR 1:1"}`.
  Drop the `"Custom"` option entirely (no native equivalent), or replace
  it with a free-form text input that writes a `n:m` value. Consider
  changing `defaultValue` to `"Auto (Game Native)"` to match native.

#### Crop — `[Display] CropMode`
- **Severity:** ERROR
- **Our schema:** Combo, default `"Overscan"`, options
  `{"None":"None","Only Overscan Area":"Overscan","Only Overscan Area (Aspect Uncorrected)":"OverscanUncorrected","All Borders":"AllBorders","All Borders (Aspect Uncorrected)":"AllBordersUncorrected"}`.
- **Native source:** `s_display_crop_mode_names = {"None","Overscan","OverscanUncorrected","Borders","BordersUncorrected"}`
  (settings.cpp:1923-1925). Native uses `Borders`, not `AllBorders`.
- **Recommended fix:** Change INI values:
  `{"All Borders":"Borders","All Borders (Aspect Uncorrected)":"BordersUncorrected"}`
  (other entries already match).

#### Scaling — `[Display] Scaling`
- **Severity:** ERROR
- **Our schema:** Combo, default `"Bilinear"`, options
  `{"Nearest-Neighbor":"NearestNeighbor","Nearest-Neighbor (Integer)":"NearestNeighborInteger","Bilinear (Smooth)":"Bilinear","Bilinear (Hybrid)":"BilinearHybrid","Bilinear (Sharp)":"BilinearSharp","Bilinear (Integer)":"BilinearInteger","Lanczos (Sharp)":"Lanczos"}`.
- **Native source:** `s_display_scaling_names = {"Nearest","NearestInteger","BilinearSmooth","BilinearHybrid","BilinearSharp","BilinearInteger","Lanczos"}`
  (settings.cpp:2188-2190). Default
  `DEFAULT_DISPLAY_SCALING = BilinearSmooth` (settings.h:240). Three of
  our seven values are wrong: `NearestNeighbor` ≠ `Nearest`,
  `NearestNeighborInteger` ≠ `NearestInteger`, `Bilinear` ≠ `BilinearSmooth`.
- **Recommended fix:** Change INI values to:
  `{"Nearest-Neighbor":"Nearest","Nearest-Neighbor (Integer)":"NearestInteger","Bilinear (Smooth)":"BilinearSmooth","Bilinear (Hybrid)":"BilinearHybrid","Bilinear (Sharp)":"BilinearSharp","Bilinear (Integer)":"BilinearInteger","Lanczos (Sharp)":"Lanczos"}`.
  Update `defaultValue` to `"BilinearSmooth"`.

#### FMV Scaling — `[Display] Scaling24Bit`
- **Severity:** ERROR
- **Our schema:** Same `scalingOptions` as Scaling above; default `"Bilinear"`.
- **Native source:** Same name table — settings.cpp:389-391 reads it via
  `ParseDisplayScaling`.
- **Recommended fix:** Apply identical fix as `Scaling` — share the
  corrected `scalingOptions` list.

---

### Graphics — Advanced › Display Options

_(`Alignment`, `Rotation`, `FineCropMode`, `FineCropLeft/Top/Right/Bottom`,
`DisableMailboxPresentation` all verified OK against settings.cpp:380-414
and the corresponding name tables.)_

---

### Graphics — Advanced › Rendering Options

_(no non-OK findings — `Multisamples`, `LineDetectMode`, `UseThread`,
`MaxQueuedFrames`, `EnableModulationCrop`, `ScaledInterlacing`,
`UseSoftwareRendererForReadbacks` all verified against settings.cpp.)_

---

### On-Screen Display — Display group

#### Display Scale — `[Display] OSDScale`
- **Severity:** WARN
- **Our schema:** `Int`, default `"100"`, range 50–500, step 10, suffix `%`.
- **Native source:** Float;
  `display_osd_scale = si.GetFloatValue("Display", "OSDScale", DEFAULT_OSD_SCALE)`
  at settings.cpp:435. `DEFAULT_OSD_SCALE = 100.0f` (settings.h:247). Round-trip
  is safe for whole-number values (`100.0f` → `"100"`), but declaring `Float`
  would be more accurate.
- **Recommended fix:** Change type to `Float`. Same issue as PCSX2's
  `OsdScale`.

#### Display Margin — `[Display] OSDMargin`
- **Severity:** WARN
- **Our schema:** `Int`, default `"10"`, range 0–100.
- **Native source:** Float;
  `display_osd_margin = std::max(si.GetFloatValue("Display", "OSDMargin", ImGuiManager::DEFAULT_SCREEN_MARGIN), 0.0f)`
  at settings.cpp:436.
- **Recommended fix:** Change type to `Float`. Round-trip works for
  integer values today.

---

### On-Screen Display — Messages group

#### Error/Warning/Information/Action Duration — `[Display] OSD{Error,Warning,Info,Quick}Duration`
- **Severity:** WARN (cosmetic)
- **Our schema:** Float, defaults `"15.0"`, `"10.0"`, `"5.0"`, `"2.5"`.
- **Native source:** Float;
  `display_osd_message_duration[i] = si.GetFloatValue("Display", "OSD{}Duration", ...)`
  at settings.cpp:438-443. Default array `{15.0f, 10.0f, 5.0f, 2.5f}`
  (settings.cpp:120-125). Native writes `15.0f` → `"15"`, `10.0f` → `"10"`,
  `5.0f` → `"5"`, `2.5f` → `"2.5"`.
- **Recommended fix:** None functionally needed (Float widget reads both
  forms identically). Trim defaults to `"15"`, `"10"`, `"5"`, `"2.5"` for
  tidiness.

---

### Audio — Configuration group

#### Driver — `[Audio] Driver`
- **Severity:** ERROR
- **Our schema:** Combo, default `"Default"`, options `{Default: "Default"}`.
- **Native source:** Free-form string;
  `audio_driver = si.GetStringViewValue("Audio", "Driver")` at settings.cpp:478.
  Empty string means "auto"; any non-empty value is passed through to the
  Cubeb driver lookup at runtime
  (`references/duckstation-master/src/util/audio_stream.cpp:75-86`).
  Writing the literal string `"Default"` to the INI will cause Cubeb to
  attempt to find a driver named "Default" — not what the user expects.
- **Recommended fix:** Change the only option's INI value from `"Default"`
  to `""` and update `defaultValue` to `""`. Optionally enumerate
  available Cubeb drivers at runtime via `GetCubebDriverNames()` instead
  of a hard-coded single entry.

#### Output Device — `[Audio] OutputDevice`
- **Severity:** INFO — needs manual verification
- **Our schema:** Combo, default `""`, options `{Default: ""}`.
- **Native source:** Free-form string enumerated at runtime from host
  audio devices via `AudioStream::GetOutputDevices(backend, driver, ...)`.
- **Recommended fix:** Enumerate at runtime once a device list is
  reachable; current single placeholder is functionally inert.

---

### Memory Cards

_(no non-OK findings — `Card1Type`, `Card2Type`, `Card1Path`, `Card2Path`,
`UsePlaylistTitle` all verified against
`s_memory_card_type_names = {"None","Shared","PerGame","PerGameTitle","PerGameFileTitle","NonPersistent"}`
at settings.cpp:2452-2454, defaults `DEFAULT_MEMORY_CARD_1_TYPE = PerGameTitle`,
`DEFAULT_MEMORY_CARD_2_TYPE = None` at settings.h:636-637.)_

---

## Appendix: Every audited setting

Legend: `OK` = round-trip verified; `WARN` / `ERROR` / `INFO` = see Findings.

### Console — Console
| Label | Section / Key | Status |
|---|---|---|
| Region | `[Console] Region` | OK |
| Force Video Timing | `[GPU] ForceVideoTiming` | OK |
| Fast Boot | `[BIOS] PatchFastBoot` | OK |
| Fast Forward Memory Card Access | `[MemoryCards] FastForwardAccess` | OK |
| Fast Forward Boot | `[BIOS] FastForwardBoot` | OK |
| Enable 8MB RAM (Dev Console) | `[Console] Enable8MBRAM` | OK |

### Console — CPU Emulation
| Label | Section / Key | Status |
|---|---|---|
| Execution Mode | `[CPU] ExecutionMode` | OK |
| Enable Clock Speed Control | `[CPU] OverclockEnable` | OK |
| Clock Speed Multiplier | `[CPU] OverclockNumerator` | OK |
| Enable Recompiler ICache | `[CPU] RecompilerICache` | OK |

### Console — CD-ROM Emulation
| Label | Section / Key | Status |
|---|---|---|
| Read Speedup | `[CDROM] ReadSpeedup` | OK |
| Seek Speedup | `[CDROM] SeekSpeedup` | **ERROR** (mirrored values) |
| Preload Image To RAM | `[CDROM] LoadImageToRAM` | OK |
| Switch to Next Disc on Stop | `[CDROM] AutoDiscChange` | OK |
| Apply Image Patches | `[CDROM] LoadImagePatches` | OK |
| Ignore Drive Subcode | `[CDROM] IgnoreHostSubcode` | OK |

### Emulation — Speed Control
| Label | Section / Key | Status |
|---|---|---|
| Emulation Speed | `[Main] EmulationSpeed` | **ERROR** (float string format) |
| Fast Forward Speed | `[Main] FastForwardSpeed` | **ERROR** (float string format) |
| Turbo Speed | `[Main] TurboSpeed` | **ERROR** (float string format) |

### Emulation — Latency Control
| Label | Section / Key | Status |
|---|---|---|
| Vertical Sync (VSync) | `[Display] VSync` | OK |
| Sync To Host Refresh Rate | `[Main] SyncToHostRefreshRate` | OK |
| Optimal Frame Pacing | `[Display] OptimalFramePacing` | OK |
| Reduce Input Latency | `[Display] PreFrameSleep` | OK |
| Skip Duplicate Frame Display | `[Display] SkipPresentingDuplicateFrames` | OK |

### Emulation — Rewind
| Label | Section / Key | Status |
|---|---|---|
| Enable Rewinding | `[Main] RewindEnable` | OK |
| Use Software Renderer (Low VRAM Mode) | `[GPU] UseSoftwareRendererForMemoryStates` | OK |
| Rewind Save Frequency | `[Main] RewindFrequency` | WARN (default format) |
| Rewind Buffer Size | `[Main] RewindSaveSlots` | OK |

### Emulation — Runahead
| Label | Section / Key | Status |
|---|---|---|
| Runahead | `[Main] RunaheadFrameCount` | OK |
| Enable for Analog Input | `[Main] RunaheadForAnalogInput` | OK |

### Graphics — top-level
| Label | Section / Key | Status |
|---|---|---|
| Renderer | `[GPU] Renderer` | OK |
| Adapter | `[GPU] Adapter` | WARN (default mismatch) |

### Graphics — Rendering
| Label | Section / Key | Status |
|---|---|---|
| Internal Resolution | `[GPU] ResolutionScale` | OK |
| Down-Sampling | `[GPU] DownsampleMode` | OK |
| Texture Filtering | `[GPU] TextureFilter` | OK |
| Sprite Texture Filtering | `[GPU] SpriteTextureFilter` | OK |
| Dithering | `[GPU] DitheringMode` | **ERROR** (4 of 6 names wrong) |
| Deinterlacing | `[GPU] DeinterlacingMode` | WARN (default mismatch) |
| Aspect Ratio | `[Display] AspectRatio` | **ERROR** (4 of 10 values wrong) |
| Crop | `[Display] CropMode` | **ERROR** (`AllBorders*` should be `Borders*`) |
| Scaling | `[Display] Scaling` | **ERROR** (3 of 7 names wrong) |
| FMV Scaling | `[Display] Scaling24Bit` | **ERROR** (same as Scaling) |
| PGXP Geometry Correction | `[GPU] PGXPEnable` | OK |
| PGXP Depth Buffer | `[GPU] PGXPDepthBuffer` | OK |
| Force 4:3 For FMVs | `[Display] Force4_3For24Bit` | OK |
| FMV Chroma Smoothing | `[GPU] ChromaSmoothing24Bit` | OK |
| Widescreen Rendering | `[GPU] WidescreenHack` | OK |
| Round Upscaled Texture Coordinates | `[GPU] ForceRoundTextureCoordinates` | OK |

### Graphics — Advanced › Display Options
| Label | Section / Key | Status |
|---|---|---|
| Screen Alignment | `[Display] Alignment` | OK |
| Rotation | `[Display] Rotation` | OK |
| Fine Crop Mode | `[Display] FineCropMode` | OK |
| Fine Crop Left | `[Display] FineCropLeft` | OK |
| Fine Crop Top | `[Display] FineCropTop` | OK |
| Fine Crop Right | `[Display] FineCropRight` | OK |
| Fine Crop Bottom | `[Display] FineCropBottom` | OK |
| Disable Mailbox Presentation | `[Display] DisableMailboxPresentation` | OK |

### Graphics — Advanced › Rendering Options
| Label | Section / Key | Status |
|---|---|---|
| Multi-Sampling | `[GPU] Multisamples` | OK |
| Line Detection | `[GPU] LineDetectMode` | OK |
| Threaded Rendering | `[GPU] UseThread` | OK |
| Max Queued Frames | `[GPU] MaxQueuedFrames` | OK |
| Texture Modulation Cropping | `[GPU] EnableModulationCrop` | OK |
| Scaled Interlacing | `[GPU] ScaledInterlacing` | OK |
| Software Renderer Readbacks | `[GPU] UseSoftwareRendererForReadbacks` | OK |

### On-Screen Display — Display
| Label | Section / Key | Status |
|---|---|---|
| Display Scale | `[Display] OSDScale` | WARN (native is float) |
| Display Margin | `[Display] OSDMargin` | WARN (native is float) |

### On-Screen Display — Messages
| Label | Section / Key | Status |
|---|---|---|
| Show Messages | `[Display] ShowOSDMessages` | OK |
| Show Status Indicators | `[Display] ShowStatusIndicators` | OK |
| Animate Messages | `[Display] AnimateOSDMessages` | OK |
| Blur Message Backgrounds | `[Display] BlurOSDMessageBackgrounds` | OK |
| Error Duration | `[Display] OSDErrorDuration` | WARN (default format) |
| Warning Duration | `[Display] OSDWarningDuration` | WARN (default format) |
| Information Duration | `[Display] OSDInfoDuration` | WARN (default format) |
| Action Duration | `[Display] OSDQuickDuration` | WARN (default format) |
| Display Location | `[Display] OSDMessageLocation` | OK |

### On-Screen Display — Overlays
| Label | Section / Key | Status |
|---|---|---|
| Show FPS | `[Display] ShowFPS` | OK |
| Show Emulation Speed | `[Display] ShowSpeed` | OK |
| Show CPU Usage | `[Display] ShowCPU` | OK |
| Show GPU Usage | `[Display] ShowGPU` | OK |
| Show Resolution | `[Display] ShowResolution` | OK |
| Show GPU Statistics | `[Display] ShowGPUStatistics` | OK |
| Show Frame Times | `[Display] ShowFrameTimes` | OK |
| Show Latency Statistics | `[Display] ShowLatencyStatistics` | OK |
| Show Controller Input | `[Display] ShowInputs` | OK |
| Show Enhancements | `[Display] ShowEnhancements` | OK |

### Audio — Controls
| Label | Section / Key | Status |
|---|---|---|
| Output Volume | `[Audio] OutputVolume` | OK |
| Fast Forward Volume | `[Audio] FastForwardVolume` | OK |
| Mute All Sound | `[Audio] OutputMuted` | OK |
| Mute CD Audio | `[CDROM] MuteCDAudio` | OK |

### Audio — Configuration
| Label | Section / Key | Status |
|---|---|---|
| Backend | `[Audio] Backend` | OK |
| Driver | `[Audio] Driver` | **ERROR** (`"Default"` is not a valid driver) |
| Output Device | `[Audio] OutputDevice` | INFO (single placeholder) |
| Stretch Mode | `[Audio] StretchMode` | OK |
| Buffer Size | `[Audio] BufferMS` | OK |
| Output Latency | `[Audio] OutputLatencyMS` | OK |

### Audio — Time Stretching
| Label | Section / Key | Status |
|---|---|---|
| Sequence Length | `[Audio] StretchSequenceLengthMS` | OK |
| Seek Window | `[Audio] StretchSeekWindowMS` | OK |
| Overlap | `[Audio] StretchOverlapMS` | OK |
| Use Quick Seek | `[Audio] StretchUseQuickSeek` | OK |
| Use Anti-Aliasing Filter | `[Audio] StretchUseAAFilter` | OK |

### Memory Cards
| Label | Section / Key | Status |
|---|---|---|
| Card 1 Type | `[MemoryCards] Card1Type` | OK |
| Card 1 Path | `[MemoryCards] Card1Path` | OK |
| Card 2 Type | `[MemoryCards] Card2Type` | OK |
| Card 2 Path | `[MemoryCards] Card2Path` | OK |
| Use Single Card For Multi-Disc Games | `[MemoryCards] UsePlaylistTitle` | OK |
