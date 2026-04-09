# DuckStation Settings Schema Fixes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply Tier 1 and Tier 2 fixes from the 2026-04-06 DuckStation settings audit so that every non-controller setting in `DuckStationAdapter::settingsSchema()` round-trips correctly with native DuckStation.

**Architecture:** Pure data edits to `cpp/src/adapters/duckstation_adapter.cpp` inside `DuckStationAdapter::settingsSchema()`. No new files, no API changes, no behavioural code touched. Each tier becomes one commit so they can be reviewed and reverted independently.

**Tech Stack:** C++17, Qt6 (`SettingDef` struct from `cpp/src/core/setting_def.h`).

**Source documents:**
- Audit: `docs/superpowers/audits/2026-04-06-duckstation-settings-audit.md`
- Spec: `docs/superpowers/specs/2026-04-06-duckstation-settings-audit-design.md`

**Out of scope (deliberate decisions from triage):**
- **Tier 3 (OSDScale/OSDMargin Int→Float):** Skipped. Same pattern that backfired on PCSX2 — Float forces `QDoubleSpinBox` 2-decimal display. Round-trip is already safe with `Int` since whole-number floats stringify identically.
- **Tier 4 (Audio Driver/Device runtime enumeration):** Deferred until after the PPSSPP audit, when one shared mechanism can be designed across all three adapters.
- **Deinterlacing default change** (`Adaptive` → `Progressive`): Left as `Adaptive`. The audit flagged it as a default mismatch (WARN), but the original choice is a deliberate UX decision and round-trip works fine.

**Note on testing:** No unit tests for adapter schemas in this repo. Verification is (a) clean build after each task, and (b) a final manual smoke-test pass.

---

## File Map

**Modify only:** `cpp/src/adapters/duckstation_adapter.cpp`

All changes localised to `DuckStationAdapter::settingsSchema()` (lines 19–417). The shared option lists `speedOptions`, `turbospeedOptions`, `cdromSeekOptions`, and `scalingOptions` are declared at the top of the function — fixing them at the source point fixes every `s.append` that references them.

`SettingDef` field order (from `setting_def.h`):
`{ category, subcategory, group, section, key, label, tooltip, type, defaultValue, options, minVal, maxVal, step, layout, suffix, dependsOn }`

---

## Task 1: Tier 1A — Round-trip ERRORs in shared option lists

The audit's most painful bugs cluster in four shared option lists declared at the top of `settingsSchema()`. Fixing the lists once propagates the fix to every `s.append` that uses them. This is the lowest-risk, highest-impact part of the fix pass.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:33-55` (`speedOptions`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:57-70` (`turbospeedOptions`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:82-90` (`cdromSeekOptions`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:106-114` (`scalingOptions`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:167-172` (defaults of `EmulationSpeed`/`FastForwardSpeed`/`TurboSpeed`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:256-259` (default of `Scaling`/`Scaling24Bit`)

### Step 1.1: Rewrite `speedOptions` with shortest-float values

DuckStation writes floats via `StringUtil::ToChars` → `std::to_chars` shortest representation (`src/common/string_util.cpp:333-342`). Padded forms like `"1.0"`, `"2.0"` never round-trip. Same bug class as PCSX2's `NominalScalar` fix.

- [ ] Replace lines 33–55:

```cpp
    const QVector<QPair<QString,QString>> speedOptions = {
        {"10% [6 FPS]",   "0.1"},
        {"20% [12 FPS]",  "0.2"},
        {"30% [18 FPS]",  "0.3"},
        {"40% [24 FPS]",  "0.4"},
        {"50% [30 FPS]",  "0.5"},
        {"60% [36 FPS]",  "0.6"},
        {"70% [42 FPS]",  "0.7"},
        {"75% [45 FPS]",  "0.75"},
        {"80% [48 FPS]",  "0.8"},
        {"90% [54 FPS]",  "0.9"},
        {"100% [60 FPS]", "1.0"},
        {"120% [72 FPS]", "1.2"},
        {"150% [90 FPS]", "1.5"},
        {"175% [105 FPS]","1.75"},
        {"200% [120 FPS]","2.0"},
        {"250% [150 FPS]","2.5"},
        {"300% [180 FPS]","3.0"},
        {"350% [210 FPS]","3.5"},
        {"400% [240 FPS]","4.0"},
        {"450% [270 FPS]","4.5"},
        {"500% [300 FPS]","5.0"},
    };
```

With:

```cpp
    // INI values must use shortest float representation (e.g. "1", "0.5", "2") —
    // DuckStation writes floats via StringUtil::ToChars / std::to_chars which
    // strips trailing zeros, so padded values fail to round-trip. See audit
    // 2026-04-06.
    const QVector<QPair<QString,QString>> speedOptions = {
        {"10% [6 FPS]",   "0.1"},
        {"20% [12 FPS]",  "0.2"},
        {"30% [18 FPS]",  "0.3"},
        {"40% [24 FPS]",  "0.4"},
        {"50% [30 FPS]",  "0.5"},
        {"60% [36 FPS]",  "0.6"},
        {"70% [42 FPS]",  "0.7"},
        {"75% [45 FPS]",  "0.75"},
        {"80% [48 FPS]",  "0.8"},
        {"90% [54 FPS]",  "0.9"},
        {"100% [60 FPS]", "1"},
        {"120% [72 FPS]", "1.2"},
        {"150% [90 FPS]", "1.5"},
        {"175% [105 FPS]","1.75"},
        {"200% [120 FPS]","2"},
        {"250% [150 FPS]","2.5"},
        {"300% [180 FPS]","3"},
        {"350% [210 FPS]","3.5"},
        {"400% [240 FPS]","4"},
        {"450% [270 FPS]","4.5"},
        {"500% [300 FPS]","5"},
    };
```

### Step 1.2: Rewrite `turbospeedOptions` with shortest-float values

- [ ] Replace lines 57–70:

```cpp
    const QVector<QPair<QString,QString>> turbospeedOptions = {
        {"Unlimited",        "0.0"},
        {"100% [60 FPS]",   "1.0"},
        {"150% [90 FPS]",   "1.5"},
        {"200% [120 FPS]",  "2.0"},
        {"300% [180 FPS]",  "3.0"},
        {"400% [240 FPS]",  "4.0"},
        {"500% [300 FPS]",  "5.0"},
        {"600% [360 FPS]",  "6.0"},
        {"700% [420 FPS]",  "7.0"},
        {"800% [480 FPS]",  "8.0"},
        {"900% [540 FPS]",  "9.0"},
        {"1000% [600 FPS]", "10.0"},
    };
```

With:

```cpp
    const QVector<QPair<QString,QString>> turbospeedOptions = {
        {"Unlimited",        "0"},
        {"100% [60 FPS]",   "1"},
        {"150% [90 FPS]",   "1.5"},
        {"200% [120 FPS]",  "2"},
        {"300% [180 FPS]",  "3"},
        {"400% [240 FPS]",  "4"},
        {"500% [300 FPS]",  "5"},
        {"600% [360 FPS]",  "6"},
        {"700% [420 FPS]",  "7"},
        {"800% [480 FPS]",  "8"},
        {"900% [540 FPS]",  "9"},
        {"1000% [600 FPS]", "10"},
    };
```

### Step 1.3: Fix `cdromSeekOptions` mirrored values

DuckStation's CDROM `SeekSpeedup` field uses `1` for normal speed and `0` for the maximum-cycles override (`src/core/cdrom.cpp:1616`). DuckStation's own UI binds to `{1,2,3,4,5,6,0}` (`consolesettingswidget.cpp:19,74`). Our schema currently has `0` and `1` swapped, so picking "Maximum" actually selects normal speed and vice versa. This is a pure value swap on the two endpoint entries.

- [ ] Replace lines 82–90:

```cpp
    const QVector<QPair<QString,QString>> cdromSeekOptions = {
        {"None (Normal Speed)", "0"},
        {"2x",  "2"},
        {"3x",  "3"},
        {"4x",  "4"},
        {"5x",  "5"},
        {"6x",  "6"},
        {"Maximum (Safer)", "1"},
    };
```

With:

```cpp
    // Native DuckStation CDROM SeekSpeedup uses 1 for normal speed and 0 for the
    // maximum-cycles override (cdrom.cpp:1616, consolesettingswidget.cpp:19,74).
    // Earlier versions of this list had the endpoints swapped, so picking
    // "Maximum (Safer)" actually selected normal speed. See audit 2026-04-06.
    const QVector<QPair<QString,QString>> cdromSeekOptions = {
        {"None (Normal Speed)", "1"},
        {"2x",  "2"},
        {"3x",  "3"},
        {"4x",  "4"},
        {"5x",  "5"},
        {"6x",  "6"},
        {"Maximum (Safer)", "0"},
    };
```

(The `SeekSpeedup` setting at line 155 has default `"1"`. After this swap, `"1"` correctly maps to `None (Normal Speed)`, so the default stays correct. No change needed at the `s.append` site.)

### Step 1.4: Rewrite `scalingOptions` with native canonical names

DuckStation's `s_display_scaling_names = {"Nearest","NearestInteger","BilinearSmooth","BilinearHybrid","BilinearSharp","BilinearInteger","Lanczos"}` (`src/core/settings.cpp:2188-2190`). Three of our seven values are wrong (`NearestNeighbor`, `NearestNeighborInteger`, `Bilinear`). Native default is `BilinearSmooth` (`settings.h:240`).

- [ ] Replace lines 106–114:

```cpp
    const QVector<QPair<QString,QString>> scalingOptions = {
        {"Nearest-Neighbor",        "NearestNeighbor"},
        {"Nearest-Neighbor (Integer)", "NearestNeighborInteger"},
        {"Bilinear (Smooth)",       "Bilinear"},
        {"Bilinear (Hybrid)",       "BilinearHybrid"},
        {"Bilinear (Sharp)",        "BilinearSharp"},
        {"Bilinear (Integer)",      "BilinearInteger"},
        {"Lanczos (Sharp)",         "Lanczos"},
    };
```

With:

```cpp
    // INI values must match s_display_scaling_names from
    // references/duckstation-master/src/core/settings.cpp:2188-2190.
    // Three names were wrong before: NearestNeighbor → Nearest,
    // NearestNeighborInteger → NearestInteger, Bilinear → BilinearSmooth.
    // See audit 2026-04-06.
    const QVector<QPair<QString,QString>> scalingOptions = {
        {"Nearest-Neighbor",            "Nearest"},
        {"Nearest-Neighbor (Integer)",  "NearestInteger"},
        {"Bilinear (Smooth)",           "BilinearSmooth"},
        {"Bilinear (Hybrid)",           "BilinearHybrid"},
        {"Bilinear (Sharp)",            "BilinearSharp"},
        {"Bilinear (Integer)",          "BilinearInteger"},
        {"Lanczos (Sharp)",             "Lanczos"},
    };
```

### Step 1.5: Update speed-control defaults to shortest-float forms

- [ ] Replace lines 167–172:

```cpp
    s.append({"Emulation", "", "Speed Control", "Main", "EmulationSpeed", "Emulation Speed", "Sets the target emulation speed.",
              SettingDef::Combo, "1.0", speedOptions, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Speed Control", "Main", "FastForwardSpeed", "Fast Forward Speed", "Speed used when the fast forward hotkey is held.",
              SettingDef::Combo, "0.0", turbospeedOptions, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Speed Control", "Main", "TurboSpeed", "Turbo Speed", "Speed used when the turbo hotkey is held.",
              SettingDef::Combo, "2.0", turbospeedOptions, 0, 0, 0, "", ""});
```

With:

```cpp
    s.append({"Emulation", "", "Speed Control", "Main", "EmulationSpeed", "Emulation Speed", "Sets the target emulation speed.",
              SettingDef::Combo, "1", speedOptions, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Speed Control", "Main", "FastForwardSpeed", "Fast Forward Speed", "Speed used when the fast forward hotkey is held.",
              SettingDef::Combo, "0", turbospeedOptions, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Speed Control", "Main", "TurboSpeed", "Turbo Speed", "Speed used when the turbo hotkey is held.",
              SettingDef::Combo, "2", turbospeedOptions, 0, 0, 0, "", ""});
```

### Step 1.6: Update Scaling/FMV Scaling defaults

- [ ] Replace lines 256–259:

```cpp
    s.append({"Graphics", "Rendering", "", "Display", "Scaling", "Scaling", "Scaling filter applied to the final output.",
              SettingDef::Combo, "Bilinear", scalingOptions, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "Display", "Scaling24Bit", "FMV Scaling", "Scaling filter applied during FMV playback.",
              SettingDef::Combo, "Bilinear", scalingOptions, 0, 0, 0, "", ""});
```

With:

```cpp
    s.append({"Graphics", "Rendering", "", "Display", "Scaling", "Scaling", "Scaling filter applied to the final output.",
              SettingDef::Combo, "BilinearSmooth", scalingOptions, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "Display", "Scaling24Bit", "FMV Scaling", "Scaling filter applied during FMV playback.",
              SettingDef::Combo, "BilinearSmooth", scalingOptions, 0, 0, 0, "", ""});
```

### Step 1.7: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`
  Expected: clean build. Pre-existing warnings in unrelated files are fine.

### Step 1.8: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(duckstation): correct shared option lists and defaults that fail to round-trip

Speed control combos used padded float strings ("1.0", "2.0") that
DuckStation's StringUtil::ToChars never produces; CDROM SeekSpeedup had
the "None"/"Maximum" endpoints mapped to swapped INI values, so picking
Maximum actually selected normal speed; Display Scaling combo had three
of seven names wrong (Nearest, NearestInteger, BilinearSmooth) and an
incorrect default that fell through to native fallback. Fixes from audit
2026-04-06.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Tier 1B — Round-trip ERRORs in inline combos

The remaining ERROR-class fixes are in combos defined inline at their `s.append` site, not in shared lists. Each is a self-contained data edit.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:228-236` (`DitheringMode`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:242-247` (`AspectRatio`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:248-255` (`CropMode`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:367-370` (audio `Driver`)

### Step 2.1: Fix the `DitheringMode` combo

Native `s_gpu_dithering_mode_names = {"Unscaled","UnscaledShaderBlend","Scaled","ScaledShaderBlend","TrueColor","TrueColorFull"}` (`src/core/settings.cpp:1708-1711`). Native default `DEFAULT_GPU_DITHERING_MODE = TrueColor` (`settings.h:226`).

- [ ] Replace lines 228–236:

```cpp
    s.append({"Graphics", "Rendering", "", "GPU", "DitheringMode", "Dithering", "",
              SettingDef::Combo, "ScaledDithering",
              {{"Unscaled", "UnscaledDithering"},
               {"Unscaled (Shader Blending)", "UnscaledShaderBlending"},
               {"Scaled", "ScaledDithering"},
               {"Scaled (Shader Blending)", "ScaledShaderBlending"},
               {"True Color", "TrueColor"},
               {"True Color (Full)", "TrueColorFull"}},
              0, 0, 0, "", ""});
```

With:

```cpp
    // INI values must match s_gpu_dithering_mode_names from
    // references/duckstation-master/src/core/settings.cpp:1708-1711.
    // Native default is TrueColor (settings.h:226). See audit 2026-04-06.
    s.append({"Graphics", "Rendering", "", "GPU", "DitheringMode", "Dithering", "",
              SettingDef::Combo, "TrueColor",
              {{"Unscaled", "Unscaled"},
               {"Unscaled (Shader Blending)", "UnscaledShaderBlend"},
               {"Scaled", "Scaled"},
               {"Scaled (Shader Blending)", "ScaledShaderBlend"},
               {"True Color", "TrueColor"},
               {"True Color (Full)", "TrueColorFull"}},
              0, 0, 0, "", ""});
```

### Step 2.2: Fix the `AspectRatio` combo and drop the broken `Custom` option

Native `Settings::ParseDisplayAspectRatio` (`src/core/settings.cpp:2010-2040`) special-cases the literal strings `"Auto (Game Native)"`, `"Stretch To Fill"`, `"PAR 1:1"` (with the literal space); everything else must parse as `"<num>:<denom>"`. The previous values `"Auto"`, `"Stretch"`, `"PAR1:1"` all fail to parse and silently fall back to native default. The `"Custom"` option is not a recognised value at all and is functionally inert; dropping it is safe (verified: no other references in `cpp/`).

- [ ] Replace lines 242–247:

```cpp
    s.append({"Graphics", "Rendering", "", "Display", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "4:3",
              {{"Auto (Game Native)", "Auto"}, {"Stretch To Fill", "Stretch"},
               {"4:3", "4:3"}, {"16:9", "16:9"}, {"19:9", "19:9"}, {"20:9", "20:9"},
               {"21:9", "21:9"}, {"16:10", "16:10"}, {"PAR 1:1", "PAR1:1"}, {"Custom", "Custom"}},
              0, 0, 0, "", ""});
```

With:

```cpp
    // INI values must match what Settings::ParseDisplayAspectRatio /
    // GetDisplayAspectRatioName accept; the special strings "Auto (Game Native)",
    // "Stretch To Fill" and "PAR 1:1" are required verbatim (with the space).
    // The "Custom" option had no native equivalent and is dropped — DuckStation
    // exposes custom ratios via numerator/denominator widgets that are out of
    // scope for this audit. See audit 2026-04-06.
    s.append({"Graphics", "Rendering", "", "Display", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "4:3",
              {{"Auto (Game Native)", "Auto (Game Native)"}, {"Stretch To Fill", "Stretch To Fill"},
               {"4:3", "4:3"}, {"16:9", "16:9"}, {"19:9", "19:9"}, {"20:9", "20:9"},
               {"21:9", "21:9"}, {"16:10", "16:10"}, {"PAR 1:1", "PAR 1:1"}},
              0, 0, 0, "", ""});
```

(Note: the default stays `"4:3"` rather than changing to `"Auto (Game Native)"`. Like the Deinterlacing default, this is a UX choice we're deliberately preserving — most users expect 4:3 as the PS1 default, and changing it would surprise existing users.)

### Step 2.3: Fix the `CropMode` combo

Native `s_display_crop_mode_names = {"None","Overscan","OverscanUncorrected","Borders","BordersUncorrected"}` (`src/core/settings.cpp:1923-1925`). Our `AllBorders` / `AllBordersUncorrected` should be `Borders` / `BordersUncorrected`.

- [ ] Replace lines 248–255:

```cpp
    s.append({"Graphics", "Rendering", "", "Display", "CropMode", "Crop", "",
              SettingDef::Combo, "Overscan",
              {{"None", "None"},
               {"Only Overscan Area", "Overscan"},
               {"Only Overscan Area (Aspect Uncorrected)", "OverscanUncorrected"},
               {"All Borders", "AllBorders"},
               {"All Borders (Aspect Uncorrected)", "AllBordersUncorrected"}},
              0, 0, 0, "", ""});
```

With:

```cpp
    // INI values must match s_display_crop_mode_names from
    // references/duckstation-master/src/core/settings.cpp:1923-1925
    // (Borders, not AllBorders). See audit 2026-04-06.
    s.append({"Graphics", "Rendering", "", "Display", "CropMode", "Crop", "",
              SettingDef::Combo, "Overscan",
              {{"None", "None"},
               {"Only Overscan Area", "Overscan"},
               {"Only Overscan Area (Aspect Uncorrected)", "OverscanUncorrected"},
               {"All Borders", "Borders"},
               {"All Borders (Aspect Uncorrected)", "BordersUncorrected"}},
              0, 0, 0, "", ""});
```

### Step 2.4: Fix the audio `Driver` combo

Native `audio_driver = si.GetStringViewValue("Audio", "Driver")` is free-form (`src/core/settings.cpp:478`); empty string means "auto", any non-empty value is passed through to Cubeb's driver lookup (`src/util/audio_stream.cpp:75-86`). Currently we write the literal string `"Default"` to the INI, which Cubeb tries to look up as a driver name and fails.

- [ ] Replace lines 367–370:

```cpp
    s.append({"Audio", "", "Configuration", "Audio", "Driver", "", "",
              SettingDef::Combo, "Default",
              {{"Default", "Default"}},
              0, 0, 0, "paired", ""});
```

With:

```cpp
    // TODO(audit-tier-4): Driver/Output Device should be enumerated at runtime
    // from Cubeb's GetCubebDriverNames() and AudioStream::GetOutputDevices().
    // Hard-coded options are functionally inert. Deferred until a shared
    // mechanism is designed across all three adapters.
    // The "Default" INI value must be empty string ("") — any non-empty value
    // is passed through to Cubeb as a driver-name lookup and fails. Audit 2026-04-06.
    s.append({"Audio", "", "Configuration", "Audio", "Driver", "Driver", "",
              SettingDef::Combo, "",
              {{"Default", ""}},
              0, 0, 0, "paired", ""});
```

(Also restores the missing `"Driver"` label — the previous declaration had an empty label string, which the previous spec audit didn't flag because it's cosmetic, but while we're here it's worth fixing.)

### Step 2.5: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`
  Expected: clean build.

### Step 2.6: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(duckstation): correct inline combos for Dithering, AspectRatio, Crop, audio Driver

DitheringMode used "ScaledDithering"/"ShaderBlending" suffixes that
DuckStation never writes; AspectRatio used "Auto"/"Stretch"/"PAR1:1"
that the parser doesn't accept (must be "Auto (Game Native)" /
"Stretch To Fill" / "PAR 1:1" with the literal space) and exposed a
"Custom" option with no native equivalent that silently fell back to
Auto; CropMode used "AllBorders" instead of native "Borders"; audio
Driver wrote the literal string "Default" which Cubeb attempted to look
up as a driver name. Audit 2026-04-06.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Tier 2 — Cosmetic default cleanups

Three default values that don't match what DuckStation writes back. Round-trip works fine in all three cases (the Float widget reads `"10"` and `"10.0"` identically), so this is purely cosmetic — it just means the value the user sees in the UI immediately after install matches what DuckStation will overwrite it with the first time it saves.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:184-185` (`RewindFrequency` default)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:206-209` (`Adapter` default)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:324-331` (OSD duration defaults)

### Step 3.1: `RewindFrequency` default `"10.0"` → `"10"`

- [ ] Replace lines 184–185:

```cpp
    { SettingDef d = {"Emulation", "", "Rewind", "Main", "RewindFrequency", "Rewind Save Frequency", "How often (in seconds) to save a rewind snapshot.",
              SettingDef::Float, "10.0", {}, 0.0, 60.0, 0.1, "", "Seconds"}; d.dependsOn = "RewindEnable"; s.append(d); }
```

With:

```cpp
    { SettingDef d = {"Emulation", "", "Rewind", "Main", "RewindFrequency", "Rewind Save Frequency", "How often (in seconds) to save a rewind snapshot.",
              SettingDef::Float, "10", {}, 0.0, 60.0, 0.1, "", "Seconds"}; d.dependsOn = "RewindEnable"; s.append(d); }
```

### Step 3.2: GPU `Adapter` default `"Default"` → `""`

The only option is `{"Default", ""}` — its INI value is the empty string, but our default was `"Default"`, so the combo can't match it and the UI shows the wrong selected entry.

- [ ] Replace lines 206–209:

```cpp
    s.append({"Graphics", "", "", "GPU", "Adapter", "Adapter", "GPU adapter to use for rendering.",
              SettingDef::Combo, "Default",
              {{"Default", ""}},
              0, 0, 0, "", ""});
```

With:

```cpp
    s.append({"Graphics", "", "", "GPU", "Adapter", "Adapter", "GPU adapter to use for rendering.",
              SettingDef::Combo, "",
              {{"Default", ""}},
              0, 0, 0, "", ""});
```

### Step 3.3: OSD duration defaults to shortest-float forms

- [ ] Replace lines 324–331:

```cpp
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDErrorDuration",         "Error Duration",         "How long error messages remain on screen.",
              SettingDef::Float, "15.0", {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDWarningDuration",       "Warning Duration",       "",
              SettingDef::Float, "10.0", {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDInfoDuration",          "Information Duration",   "",
              SettingDef::Float, "5.0",  {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDQuickDuration",         "Action Duration",        "",
              SettingDef::Float, "2.5",  {}, 0.0, 60.0, 0.5, "paired", "seconds"});
```

With:

```cpp
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDErrorDuration",         "Error Duration",         "How long error messages remain on screen.",
              SettingDef::Float, "15", {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDWarningDuration",       "Warning Duration",       "",
              SettingDef::Float, "10", {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDInfoDuration",          "Information Duration",   "",
              SettingDef::Float, "5",  {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDQuickDuration",         "Action Duration",        "",
              SettingDef::Float, "2.5",  {}, 0.0, 60.0, 0.5, "paired", "seconds"});
```

(Only the three whole-number defaults change. `"2.5"` is already in shortest form.)

### Step 3.4: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`
  Expected: clean build.

### Step 3.5: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(duckstation): tidy default value formatting to match what native writes

Trim padded-float defaults (RewindFrequency "10.0", OSD durations
"15.0"/"10.0"/"5.0") to shortest-float form so the UI's initial
selection matches what DuckStation writes the first time it saves.
Change GPU Adapter default from "Default" to empty string so the
combo correctly highlights its only option. Audit 2026-04-06.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Final manual smoke test (USER)

After all three commits land, do an end-to-end check that the changed settings actually persist through DuckStation. This task is handed back to the user — a subagent cannot run the GUI.

### Step 4.1: Launch the app

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project && ./cpp/build/EmulatorFrontend`

### Step 4.2: Round-trip checks

For each item below: open Settings → DuckStation, change to a non-default value, close Settings, reopen, confirm the value you set is shown (not the default).

- [ ] **Emulation Speed** — set 200%, reopen, confirm. INI should read `EmulationSpeed = 2`.
- [ ] **Fast Forward Speed** — set 300%, reopen, confirm. INI should read `FastForwardSpeed = 3`.
- [ ] **Turbo Speed** — set 500%, reopen, confirm. INI should read `TurboSpeed = 5`.
- [ ] **Seek Speedup (CD-ROM Emulation)** — set "Maximum (Safer)", reopen, confirm. INI should read `SeekSpeedup = 0`. Then set "None (Normal Speed)", reopen, confirm. INI should read `SeekSpeedup = 1`. (This is the swap fix — verify the labels and values now line up.)
- [ ] **Dithering** — set "Scaled (Shader Blending)", reopen, confirm. INI should read `DitheringMode = ScaledShaderBlend`.
- [ ] **Aspect Ratio** — set "Auto (Game Native)", reopen, confirm. INI should read `AspectRatio = Auto (Game Native)`. Then set "PAR 1:1", confirm INI reads `AspectRatio = PAR 1:1` (with the space). Confirm the dropdown no longer offers a "Custom" entry.
- [ ] **Crop** — set "All Borders", reopen, confirm. INI should read `CropMode = Borders`.
- [ ] **Scaling** — set "Nearest-Neighbor", reopen, confirm. INI should read `Scaling = Nearest`. Then set "Bilinear (Smooth)", confirm INI reads `Scaling = BilinearSmooth`.
- [ ] **FMV Scaling** — set "Bilinear (Sharp)", reopen, confirm. INI should read `Scaling24Bit = BilinearSharp`.
- [ ] **Audio Driver** — confirm "Default" is selected. INI should read `Driver = ` (empty value, not `Driver = Default`).

### Step 4.3: Default-cleanup checks

These are first-launch-only checks. To verify, you can either delete `{root}/emulators/DuckStation/settings.ini` (if you're willing to lose your DuckStation config) and relaunch, or just inspect the schema defaults visually after the merge:

- [ ] **Rewind Save Frequency** — confirm widget shows `10` (not `10.0`) before any user change.
- [ ] **OSD Error/Warning/Information Duration** — confirm widgets show `15`, `10`, `5` (not `15.0`, `10.0`, `5.0`).
- [ ] **GPU Adapter** — confirm dropdown highlights "Default" (combo can match empty-string value to `{"Default", ""}`).

### Step 4.4: Real game launch

- [ ] Launch any DuckStation game from the app and confirm it boots normally. Quit the game.

### Step 4.5: Inspect the post-launch INI

- [ ] Open `{root}/emulators/DuckStation/settings.ini` after DuckStation has run once and verify none of the changed keys reverted to a different format. Specifically check `[Main] EmulationSpeed`, `[Main] TurboSpeed`, `[CDROM] SeekSpeedup`, `[GPU] DitheringMode`, `[Display] AspectRatio`, `[Display] CropMode`, `[Display] Scaling`, `[Display] Scaling24Bit`, `[Audio] Driver`.

If everything passes, the audit's Tier 1+2 fixes are verified.

---

## Self-review notes

- **Spec coverage:** Every Tier 1 ERROR (8 finding groups) and Tier 2 WARN (3 cosmetic groups) from the audit has a corresponding step. Tier 3 (Float) and Tier 4 (runtime device enumeration) are explicitly out of scope, with the Tier 4 case getting an inline TODO comment in code (Step 2.4). The Deinterlacing default change is also explicitly out of scope.
- **Placeholder check:** No TBDs or "implement later"; every code-changing step shows the exact before/after blocks.
- **Type consistency:** No new types or callsites introduced; all changes are SettingDef data fields.
- **Migration concern:** Existing user settings.ini files may contain the old (broken) values. They were already not round-tripping correctly, so the next save by DuckStation will overwrite them with native canonical forms regardless. No migration code needed.
- **`SeekSpeedup` semantic note:** The swap means a user who previously had this set to "None (Normal Speed)" (storing `0`) and then upgrades will, on first re-read, see "Maximum (Safer)" selected — because their stored `0` now corresponds to Maximum. This is technically a behaviour change for that one user, but the previous behaviour was already broken (they were getting the opposite of what they picked), so this just makes the setting work correctly going forward.
