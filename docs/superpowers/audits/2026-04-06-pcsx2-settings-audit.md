# PCSX2 Settings Audit

**Date:** 2026-04-06
**Scope:** All non-controller `SettingDef`s returned by `PCSX2Adapter::settingsSchema()` in
`cpp/src/adapters/pcsx2_adapter.cpp` (lines 19–291).
**Reference:** `references/pcsx2-master/` (PCSX2 2.x master)

Controller binding settings, hotkeys, controller types, port configuration and
RetroAchievements credentials are **out of scope** (separate audit).

---

## Summary

| | Count |
|---|---|
| Total settings audited | 82 |
| OK | 69 |
| WARN | 4 |
| ERROR | 9 |

### Top issues

1. **Speed Control combo values are floats in the wrong stringification format.**
   PCSX2 writes floats via `std::to_chars` / `std::ostringstream` (shortest
   representation): `1.0f` → `"1"`, `0.5f` → `"0.5"`. Our options use padded
   forms `"1.00"`, `"0.50"`, `"2.00"` etc. which never match on re-read, so the
   combo silently reverts to default after PCSX2 saves once.
   Affects `NominalScalar`, `TurboScalar`, `SlomoScalar`.
2. **Audio `Backend`, `ExpansionMode`, `SyncMode` combos use integer strings**
   but PCSX2 round-trips them as **enum name strings** (`"Cubeb"`, `"Disabled"`,
   `"Surround51"`, `"TimeStretch"` …). Every one of these three combos will
   fail to round-trip.
3. **`OsdShowPatches` is misspelled** in native source. The real key is
   `OsdshowPatches` (lowercase `s` in `show`). Our widget reads/writes a key
   PCSX2 never touches.

---

## Findings (grouped by category)

Only settings with WARN / ERROR / INFO findings are listed. All OK settings
are enumerated in the appendix.

### Emulation

#### Normal Speed — `[Framerate] NominalScalar`
- **Severity:** ERROR
- **Our schema:** Combo, default `"1.00"`, options include `"0.02","0.10","0.25","0.50","0.75","0.90","1.00","1.10","1.20","1.50","1.75","2.00","3.00","4.00","5.00","10.00","0.00"`.
- **Native source:** Float. Read/written via `SetFloatValue` →
  `INISettingsInterface::SetFloatValue` →
  `StringUtil::ToChars(float)` which on non-MSVC (macOS builds) uses
  `std::ostringstream << value` with C locale and default precision (6)
  — i.e. shortest representation.
  - `references/pcsx2-master/pcsx2/INISettingsInterface.cpp:244-248`
  - `references/pcsx2-master/common/StringUtil.h:153-172`
  - `references/pcsx2-master/pcsx2/Pcsx2Config.cpp:1722-1724` (`SettingsWrapEntry` on float).
  - `references/pcsx2-master/pcsx2-qt/Settings/EmulationSettingsWidget.cpp:199-210` confirms native combo stores `static_cast<float>(speed)/100.0f` and a plain `0.0f` for "Unlimited".
  Native will write `1` / `0.5` / `2` / `10` / `0` / `1.1` / `1.2` / `1.5` / `3` / `4` / `5`, never the zero-padded forms we use.
- **Recommended fix:** Change option INI values to the shortest-representation
  forms: `"0.02","0.1","0.25","0.5","0.75","0.9","1","1.1","1.2","1.5","1.75","2","3","4","5","10","0"`. Update `defaultValue` to `"1"`.
  The "Unlimited" option at `0` is supported by PCSX2 at runtime (only
  RetroAchievements hardcore mode clamps it via `ClampSpeed` at
  `VMManager.cpp:3146`).

#### Fast-Forward Speed — `[Framerate] TurboScalar`
- **Severity:** ERROR
- **Our schema:** Combo, default `"2.00"`, same options as above.
- **Native source:** Same mechanism as `NominalScalar`. Native default = `2.0f` → `"2"`.
- **Recommended fix:** Same as `NominalScalar`. Default → `"2"`.

#### Slow-Motion Speed — `[Framerate] SlomoScalar`
- **Severity:** ERROR
- **Our schema:** Combo, default `"0.50"`, same options.
- **Native source:** Same mechanism. Default = `0.5f` → `"0.5"`.
- **Recommended fix:** Same as above. Default → `"0.5"`.

---

### Graphics › Display

#### Fullscreen Mode — `[EmuCore/GS] FullscreenMode`
- **Severity:** ERROR
- **Our schema:** Combo, default `""`, options `{"Borderless Fullscreen": "", "Native Desktop": "native"}`.
- **Native source:** Free-form display-mode string produced by
  `GSDevice::GetFullscreenModeString(width, height, refresh_rate)`, e.g.
  `"1920x1080@60.000000"`. Empty string means borderless fullscreen.
  - `references/pcsx2-master/pcsx2/GS/Renderers/Common/GSDevice.cpp:250-300`
  - `references/pcsx2-master/pcsx2-qt/Settings/GraphicsSettingsWidget.cpp:827-831, 1089-1098`
  PCSX2 has no concept of a `"native"` value — when loaded it won't match any
  enumerated mode and fullscreen will stay borderless (harmless) but the
  setting is effectively non-functional and the option is misleading.
- **Recommended fix:** Either (a) remove the combo entirely and always write
  `""` (borderless), or (b) enumerate available exclusive-fullscreen modes at
  runtime via the host window system and bind them as combo options. Do not
  ship the `"native"` placeholder.

#### Aspect Ratio — `[EmuCore/GS] AspectRatio`
- **Severity:** INFO
- **Our schema:** Combo, options `{Auto 4:3/3:2, 4:3, 16:9, Stretch}`.
- **Native source:** Enum written as name via `SettingsWrapEnumEx`:
  `Stretch | Auto 4:3/3:2 | 4:3 | 16:9 | 10:7`
  (`references/pcsx2-master/pcsx2/Pcsx2Config.cpp:639-645`,
   `Pcsx2Config.cpp:931`).
- **Recommended fix:** Not an error — we just omit the `"10:7"` native option.
  Add `{"10:7 (Native/Full)", "10:7"}` if users want parity.

#### Deinterlacing — `[EmuCore/GS] deinterlace_mode`
- **Severity:** INFO
- **Our schema:** Combo with values `"0"`..`"8"` (9 options; `Adaptive → 8`).
- **Native source:** `GSInterlaceMode` has 10 values
  `{Automatic, Off, WeaveTFF, WeaveBFF, BobTFF, BobBFF, BlendTFF, BlendBFF, AdaptiveTFF, AdaptiveBFF}`
  (`references/pcsx2-master/pcsx2/Config.h:291-303`). Our single "Adaptive"
  maps to `AdaptiveTFF`; `AdaptiveBFF` (`"9"`) isn't exposed.
- **Recommended fix:** Optional — add a ninth entry for `AdaptiveBFF`. Mislabel
  aside, round-trip is fine.

#### Vertical Stretch — `[EmuCore/GS] StretchY`
- **Severity:** WARN
- **Our schema:** `Int`, default `"100"`, range `10..300`.
- **Native source:** `float StretchY = 100.0f`
  (`references/pcsx2-master/pcsx2/Config.h:848`). Written via
  `SettingsWrapEntry` → `SetFloatValue`.
- **Recommended fix:** Integer write paths round-trip (`100.0f` → `"100"`) so
  existing behavior is safe, but declaring `Float` would allow fractional
  values and avoid subtle issues if a user edits the INI manually.

---

### Graphics › Post-Processing

_(no non-OK findings; ShadeBoost, CASMode, CASSharpness, TVShader, FXAA all
confirmed against `Pcsx2Config.cpp:1045-1072`.)_

---

### Graphics › OSD

#### Show Patches — `[EmuCore/GS] OsdShowPatches`
- **Severity:** ERROR
- **Our schema:** Bool, default `"false"`, key `OsdShowPatches`.
- **Native source:** The key name is **`OsdshowPatches`** (lowercase `s` in
  `show`). `SettingsWrapBitBool` uses `#varname` as the literal key, and the
  field in `GSOptions` is declared `OsdshowPatches : 1`.
  - `references/pcsx2-master/pcsx2/Config.h:781`
  - `references/pcsx2-master/pcsx2/Pcsx2Config.cpp:741, 967`
  - `references/pcsx2-master/pcsx2/ImGui/FullscreenUI_Settings.cpp:3401`
  - `references/pcsx2-master/pcsx2/GS/GS.cpp:1144` and `ImGuiOverlays.cpp:796`
  all use `OsdshowPatches`.
- **Recommended fix:** Rename the key in our schema from `OsdShowPatches` to
  `OsdshowPatches`. (Label/tooltip unchanged.)

#### OSD Scale — `[EmuCore/GS] OsdScale`
- **Severity:** WARN
- **Our schema:** `Int`, default `"100"`, range `25..500`, step `25`.
- **Native source:** `float OsdScale = 100.0f`
  (`references/pcsx2-master/pcsx2/Config.h:851`, default
  `DEFAULT_OSD_SCALE = 100.0f`). Written via `SetFloatValue`.
- **Recommended fix:** Same as `StretchY` — round-trip is safe for whole-number
  values (`100.0f` → `"100"`), but declaring `Float` is more accurate and lets
  users enter non-integer scales.

---

### Audio

#### Backend — `[SPU2/Output] Backend`
- **Severity:** ERROR
- **Our schema:** Combo, default `"cubeb"`, options
  `{Cubeb: "cubeb", SDL: "sdl", Null (No Sound): "null"}`.
- **Native source:** Written via `SettingsWrapParsedEnum(Backend, "Backend", &AudioStream::ParseBackendName, &AudioStream::GetBackendName)`
  (`Pcsx2Config.cpp:1257`). `GetBackendName`/`ParseBackendName` use
  case-sensitive `std::strcmp` against `s_backend_names = {"Null","Cubeb","SDL"}`
  (`references/pcsx2-master/pcsx2/Host/AudioStream.cpp:148-171`). Default is
  `DEFAULT_BACKEND` which resolves to `Cubeb` — i.e. written as `"Cubeb"`.
- **Recommended fix:** Change option INI values to exact-case enum names:
  `{"Cubeb":"Cubeb", "SDL":"SDL", "Null (No Sound)":"Null"}`. Update
  `defaultValue` to `"Cubeb"`.

#### Expansion — `[SPU2/Output] ExpansionMode`
- **Severity:** ERROR
- **Our schema:** Combo, default `"0"`, options
  `{"Disabled (Stereo)":"0","Stereo with LFE":"1","Quadraphonic":"2","Quadraphonic with LFE":"3","5.1 Surround":"4","7.1 Surround":"5"}`.
- **Native source:** Written as enum-name string via
  `AudioStream::GetExpansionModeName` and parsed back by
  `AudioStream::ParseExpansionMode` (`AudioStream.cpp:183-221`, called from
  `AudioStreamParameters::LoadSave` at `AudioStream.cpp:781`). Names:
  `"Disabled","StereoLFE","Quadraphonic","QuadraphonicLFE","Surround51","Surround71"`.
- **Recommended fix:** Change INI values to:
  `{"Disabled (Stereo)":"Disabled","Stereo with LFE":"StereoLFE","Quadraphonic":"Quadraphonic","Quadraphonic with LFE":"QuadraphonicLFE","5.1 Surround":"Surround51","7.1 Surround":"Surround71"}`. Default → `"Disabled"`.

#### Synchronization — `[SPU2/Output] SyncMode`
- **Severity:** ERROR
- **Our schema:** Combo, default `"1"`, options
  `{"Disabled (Noisy)":"0","TimeStretch (Recommended)":"1"}`.
- **Native source:** `SettingsWrapParsedEnum(SyncMode, "SyncMode", &ParseSyncMode, &GetSyncModeName)`
  (`Pcsx2Config.cpp:1258`) with
  `s_spu2_sync_mode_names = {"Disabled","TimeStretch"}`
  (`Pcsx2Config.cpp:1175-1205`). Parsed/written as the raw name string.
- **Recommended fix:** Change INI values to
  `{"Disabled (Noisy)":"Disabled","TimeStretch (Recommended)":"TimeStretch"}`.
  Default → `"TimeStretch"`.

#### Driver — `[SPU2/Output] DriverName`
- **Severity:** INFO
- **Our schema:** Combo, default `""`, options `{Default:"", audiounit:"audiounit"}`.
- **Native source:** Free-form string written via `SettingsWrapEntry(DriverName)`
  (`Pcsx2Config.cpp:1259`). Valid values depend on the chosen backend at
  runtime (Cubeb driver names come from `GetCubebDriverNames()`,
  `AudioStream.cpp:75-81`; SDL doesn't use this). Empty string means "auto".
- **Recommended fix:** Not incorrect — but the hard-coded `audiounit` option is
  macOS-specific, and other Cubeb drivers (WASAPI, PulseAudio, Core Audio
  etc.) are unreachable from the UI. Consider enumerating drivers at runtime
  based on the selected backend, or just leave a plain string field.

#### Output Device — `[SPU2/Output] DeviceName`
- **Severity:** INFO — needs manual verification
- **Our schema:** Combo, default `""`, options `{Default:""}`.
- **Native source:** Free-form string, enumerated at runtime from host audio
  devices (`AudioStream::GetOutputDevices(backend)`).
- **Recommended fix:** Enumerate at runtime rather than hard-coding a single
  "Default" entry.

#### Output Latency — `[SPU2/Output] OutputLatencyMS`
- **Severity:** WARN
- **Our schema:** `Int`, default `"20"`, range `0..200`, step `5`.
- **Native source:** Written as int; clamped `0..65535`
  (`AudioStream.cpp:784`). FullscreenUI exposes range `1..500`
  (`FullscreenUI_Settings.cpp:3464`), the Qt settings widget uses the same
  DEFAULT (20) and the stream engine tolerates anything up to u16 max.
- **Recommended fix:** Widening our upper bound to at least `500` would match
  native UI. Lower bound `0` is fine when `OutputLatencyMinimal=false`.

---

### Memory Cards

_(no non-OK findings — `Slot1_Enable`, `Slot1_Filename`, `Slot2_*`,
`Multitap1_Slot{2,3,4}_Enable` all confirmed against
`Pcsx2Config.cpp:2055-2068` and `MemoryCardFile.cpp:244-250`.)_

---

## Appendix: Every audited setting

Legend: `OK` = round-trip verified; `WARN` / `ERROR` / `INFO` = see Findings.

### Emulation — Speed Control
| Label | Section / Key | Status |
|---|---|---|
| Normal Speed | `[Framerate] NominalScalar` | ERROR |
| Fast-Forward Speed | `[Framerate] TurboScalar` | ERROR |
| Slow-Motion Speed | `[Framerate] SlomoScalar` | ERROR |

### Emulation — System Settings
| Label | Section / Key | Status |
|---|---|---|
| EE Cycle Rate | `[EmuCore/Speedhacks] EECycleRate` | OK |
| EE Cycle Skipping | `[EmuCore/Speedhacks] EECycleSkip` | OK |
| Enable Multithreaded VU1 (MTVU) | `[EmuCore/Speedhacks] vuThread` | OK |
| Enable Thread Pinning | `[EmuCore] EnableThreadPinning` | OK |
| Enable CDVD Precaching | `[EmuCore] CdvdPrecache` | OK |
| Enable Host Filesystem | `[EmuCore] HostFs` | OK |
| Enable Cheats | `[EmuCore] EnableCheats` | OK |
| Fast Boot | `[EmuCore] EnableFastBoot` | OK |

### Emulation — Frame Pacing / Latency Control
| Label | Section / Key | Status |
|---|---|---|
| Maximum Frame Latency | `[EmuCore/GS] VsyncQueueSize` | OK |
| Sync to Host Refresh Rate | `[EmuCore/GS] SyncToHostRefreshRate` | OK |
| Vertical Sync (VSync) | `[EmuCore/GS] VsyncEnable` | OK |
| Use Host VSync Timing | `[EmuCore/GS] UseVSyncForTiming` | OK |
| Skip Presenting Duplicate Frames | `[EmuCore/GS] SkipDuplicateFrames` | OK |

### Graphics › Display
| Label | Section / Key | Status |
|---|---|---|
| Renderer | `[EmuCore/GS] Renderer` | OK |
| Fullscreen Mode | `[EmuCore/GS] FullscreenMode` | ERROR |
| Aspect Ratio | `[EmuCore/GS] AspectRatio` | INFO (missing 10:7) |
| FMV Aspect Ratio Override | `[EmuCore/GS] FMVAspectRatioSwitch` | OK |
| Deinterlacing | `[EmuCore/GS] deinterlace_mode` | INFO (missing AdaptiveBFF) |
| Bilinear Filtering | `[EmuCore/GS] linear_present_mode` | OK |
| Vertical Stretch | `[EmuCore/GS] StretchY` | WARN (native is float) |
| Crop Left | `[EmuCore/GS] CropLeft` | OK |
| Crop Top | `[EmuCore/GS] CropTop` | OK |
| Crop Right | `[EmuCore/GS] CropRight` | OK |
| Crop Bottom | `[EmuCore/GS] CropBottom` | OK |
| Apply Widescreen Patches | `[EmuCore] EnableWideScreenPatches` | OK |
| Apply No-Interlacing Patches | `[EmuCore] EnableNoInterlacingPatches` | OK |
| Anti-Blur | `[EmuCore/GS] pcrtc_antiblur` | OK |
| Integer Scaling | `[EmuCore/GS] IntegerScaling` | OK |
| Screen Offsets | `[EmuCore/GS] pcrtc_offsets` | OK |
| Disable Interlace Offset | `[EmuCore/GS] disable_interlace_offset` | OK |
| Show Overscan | `[EmuCore/GS] pcrtc_overscan` | OK |

### Graphics › Rendering
| Label | Section / Key | Status |
|---|---|---|
| Internal Resolution | `[EmuCore/GS] upscale_multiplier` | OK |
| Texture Filtering | `[EmuCore/GS] filter` | OK |
| Trilinear Filtering | `[EmuCore/GS] TriFilter` | OK |
| Anisotropic Filtering | `[EmuCore/GS] MaxAnisotropy` | OK |
| Dithering | `[EmuCore/GS] dithering_ps2` | OK |
| Blending Accuracy | `[EmuCore/GS] accurate_blending_unit` | OK |
| Mipmapping | `[EmuCore/GS] hw_mipmap` | OK |

### Graphics › Post-Processing
| Label | Section / Key | Status |
|---|---|---|
| Contrast Adaptive Sharpening | `[EmuCore/GS] CASMode` | OK |
| Sharpness | `[EmuCore/GS] CASSharpness` | OK |
| FXAA | `[EmuCore/GS] fxaa` | OK |
| TV Shader | `[EmuCore/GS] TVShader` | OK |
| Shade Boost | `[EmuCore/GS] ShadeBoost` | OK |
| Brightness | `[EmuCore/GS] ShadeBoost_Brightness` | OK |
| Contrast | `[EmuCore/GS] ShadeBoost_Contrast` | OK |
| Saturation | `[EmuCore/GS] ShadeBoost_Saturation` | OK |

### Graphics › OSD
| Label | Section / Key | Status |
|---|---|---|
| OSD Scale | `[EmuCore/GS] OsdScale` | WARN (native is float) |
| OSD Messages Position | `[EmuCore/GS] OsdMessagesPos` | OK |
| OSD Performance Position | `[EmuCore/GS] OsdPerformancePos` | OK |
| Show Speed Percentages | `[EmuCore/GS] OsdShowSpeed` | OK |
| Show FPS | `[EmuCore/GS] OsdShowFPS` | OK |
| Show VPS | `[EmuCore/GS] OsdShowVPS` | OK |
| Show Resolution | `[EmuCore/GS] OsdShowResolution` | OK |
| Show GS Statistics | `[EmuCore/GS] OsdShowGSStats` | OK |
| Show CPU Usage | `[EmuCore/GS] OsdShowCPU` | OK |
| Show GPU Usage | `[EmuCore/GS] OsdShowGPU` | OK |
| Show Status Indicators | `[EmuCore/GS] OsdShowIndicators` | OK |
| Show Frame Times | `[EmuCore/GS] OsdShowFrameTimes` | OK |
| Show Hardware Info | `[EmuCore/GS] OsdShowHardwareInfo` | OK |
| Show PCSX2 Version | `[EmuCore/GS] OsdShowVersion` | OK |
| Show Settings | `[EmuCore/GS] OsdShowSettings` | OK |
| Show Patches | `[EmuCore/GS] OsdShowPatches` | **ERROR** (native: `OsdshowPatches`) |
| Show Inputs | `[EmuCore/GS] OsdShowInputs` | OK |
| Show Video Capture Status | `[EmuCore/GS] OsdShowVideoCapture` | OK |
| Show Input Recording Status | `[EmuCore/GS] OsdShowInputRec` | OK |
| Show Texture Replacement Status | `[EmuCore/GS] OsdShowTextureReplacements` | OK |
| Warn About Unsafe Settings | `[EmuCore] WarnAboutUnsafeSettings` | OK |

### Audio — Configuration
| Label | Section / Key | Status |
|---|---|---|
| Backend | `[SPU2/Output] Backend` | ERROR |
| Driver | `[SPU2/Output] DriverName` | INFO |
| Output Device | `[SPU2/Output] DeviceName` | INFO |
| Expansion | `[SPU2/Output] ExpansionMode` | ERROR |
| Synchronization | `[SPU2/Output] SyncMode` | ERROR |
| Buffer Size | `[SPU2/Output] BufferMS` | OK |
| Output Latency | `[SPU2/Output] OutputLatencyMS` | WARN |
| Minimal Output Latency | `[SPU2/Output] OutputLatencyMinimal` | OK |

### Audio — Controls
| Label | Section / Key | Status |
|---|---|---|
| Standard Volume | `[SPU2/Output] StandardVolume` | OK |
| Fast Forward Volume | `[SPU2/Output] FastForwardVolume` | OK |
| Mute All Sound | `[SPU2/Output] OutputMuted` | OK |

### Memory Cards
| Label | Section / Key | Status |
|---|---|---|
| Slot 1 | `[MemoryCards] Slot1_Enable` | OK |
| Slot 1 Filename | `[MemoryCards] Slot1_Filename` | OK |
| Slot 2 | `[MemoryCards] Slot2_Enable` | OK |
| Slot 2 Filename | `[MemoryCards] Slot2_Filename` | OK |
| Multitap 1 - Slot 2 | `[MemoryCards] Multitap1_Slot2_Enable` | OK |
| Multitap 1 - Slot 3 | `[MemoryCards] Multitap1_Slot3_Enable` | OK |
| Multitap 1 - Slot 4 | `[MemoryCards] Multitap1_Slot4_Enable` | OK |
