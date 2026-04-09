# PCSX2 Settings Schema Fixes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply Tier 1–3 fixes from the 2026-04-06 PCSX2 settings audit so that every non-controller setting in `PCSX2Adapter::settingsSchema()` round-trips correctly with native PCSX2.

**Architecture:** All changes are pure data edits to `cpp/src/adapters/pcsx2_adapter.cpp` inside the `PCSX2Adapter::settingsSchema()` function. No new files, no API changes, no behavioral code touched. Each tier becomes one commit so they can be reviewed and reverted independently.

**Tech Stack:** C++17, Qt6 (`SettingDef` struct from `cpp/src/core/setting_def.h`).

**Source documents:**
- Audit: `docs/superpowers/audits/2026-04-06-pcsx2-settings-audit.md`
- Spec: `docs/superpowers/specs/2026-04-06-pcsx2-settings-audit-design.md`

**Out of scope:** Tier 4 (audio Driver/Device runtime enumeration) is deferred until after the DuckStation and PPSSPP audits, when a single shared mechanism can be designed for all three adapters.

**Note on testing:** There are no unit tests for adapter schemas in this repo (`cpp/tests/` only contains tests for IniFile, IsoReader, RomScanner, SfoParser). Verification is therefore: (a) the project builds clean after each task, and (b) a final manual smoke-test pass at the end of all tasks.

---

## File Map

**Modify only:** `cpp/src/adapters/pcsx2_adapter.cpp`

All changes are localised to `PCSX2Adapter::settingsSchema()` between lines 19 and 291. No other file is touched.

For reference, the `SettingDef` initializer field order (from `setting_def.h`) is:
`{ category, subcategory, group, section, key, label, tooltip, type, defaultValue, options, minVal, maxVal, step, layout, suffix, dependsOn }`

---

## Task 1: Tier 1 — Round-trip ERRORs

These are the seven settings whose stored value or key does not match what PCSX2 reads/writes. After this task, the highest-impact bugs from the audit are gone.

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:30-54` (speed combos)
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:235` (`OsdShowPatches` key)
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:248-263` (audio enum combos)

### Step 1.1: Rewrite the speed control combo

- [ ] **Replace lines 30–54** (the `speedOptions` constant and the three `s.append` calls that use it). PCSX2 writes floats via `StringUtil::ToChars` → shortest representation, so `1.0f` → `"1"`, `0.5f` → `"0.5"`, `0.0f` → `"0"`. Our padded `"1.00"` etc. never match on re-read.

Replace:

```cpp
    // ── Speed Control ───────────────────────────────────────────────────
    const QVector<QPair<QString,QString>> speedOptions = {
        {"2% [1 FPS (NTSC) / 1 FPS (PAL)]",      "0.02"},
        {"10% [6 FPS (NTSC) / 5 FPS (PAL)]",     "0.10"},
        {"25% [15 FPS (NTSC) / 12 FPS (PAL)]",   "0.25"},
        {"50% [30 FPS (NTSC) / 25 FPS (PAL)]",   "0.50"},
        {"75% [45 FPS (NTSC) / 37 FPS (PAL)]",   "0.75"},
        {"90% [54 FPS (NTSC) / 45 FPS (PAL)]",   "0.90"},
        {"100% [60 FPS (NTSC) / 50 FPS (PAL)]",  "1.00"},
        {"110% [66 FPS (NTSC) / 55 FPS (PAL)]",  "1.10"},
        {"120% [72 FPS (NTSC) / 60 FPS (PAL)]",  "1.20"},
        {"150% [90 FPS (NTSC) / 75 FPS (PAL)]",  "1.50"},
        {"175% [105 FPS (NTSC) / 87 FPS (PAL)]", "1.75"},
        {"200% [120 FPS (NTSC) / 100 FPS (PAL)]","2.00"},
        {"300% [180 FPS (NTSC) / 150 FPS (PAL)]","3.00"},
        {"400% [240 FPS (NTSC) / 200 FPS (PAL)]","4.00"},
        {"500% [300 FPS (NTSC) / 250 FPS (PAL)]","5.00"},
        {"1000% [600 FPS (NTSC) / 500 FPS (PAL)]","10.00"},
        {"Unlimited", "0.00"},
    };
    s.append({"Emulation", "", "Speed Control", "Framerate", "NominalScalar", "Normal Speed",
              "Sets the target speed for normal gameplay.", SettingDef::Combo, "1.00", speedOptions, 0, 0, 0});
    s.append({"Emulation", "", "Speed Control", "Framerate", "TurboScalar", "Fast-Forward Speed",
              "Sets the target speed when turbo mode is activated.", SettingDef::Combo, "2.00", speedOptions, 0, 0, 0});
    s.append({"Emulation", "", "Speed Control", "Framerate", "SlomoScalar", "Slow-Motion Speed",
              "Sets the target speed when slow motion mode is activated.", SettingDef::Combo, "0.50", speedOptions, 0, 0, 0});
```

With:

```cpp
    // ── Speed Control ───────────────────────────────────────────────────
    // INI values must use shortest float representation (e.g. "1", "0.5", "2") —
    // PCSX2 writes floats via StringUtil::ToChars which never produces zero-padded
    // forms, so padded values fail to round-trip. See audit 2026-04-06.
    const QVector<QPair<QString,QString>> speedOptions = {
        {"2% [1 FPS (NTSC) / 1 FPS (PAL)]",       "0.02"},
        {"10% [6 FPS (NTSC) / 5 FPS (PAL)]",      "0.1"},
        {"25% [15 FPS (NTSC) / 12 FPS (PAL)]",    "0.25"},
        {"50% [30 FPS (NTSC) / 25 FPS (PAL)]",    "0.5"},
        {"75% [45 FPS (NTSC) / 37 FPS (PAL)]",    "0.75"},
        {"90% [54 FPS (NTSC) / 45 FPS (PAL)]",    "0.9"},
        {"100% [60 FPS (NTSC) / 50 FPS (PAL)]",   "1"},
        {"110% [66 FPS (NTSC) / 55 FPS (PAL)]",   "1.1"},
        {"120% [72 FPS (NTSC) / 60 FPS (PAL)]",   "1.2"},
        {"150% [90 FPS (NTSC) / 75 FPS (PAL)]",   "1.5"},
        {"175% [105 FPS (NTSC) / 87 FPS (PAL)]",  "1.75"},
        {"200% [120 FPS (NTSC) / 100 FPS (PAL)]", "2"},
        {"300% [180 FPS (NTSC) / 150 FPS (PAL)]", "3"},
        {"400% [240 FPS (NTSC) / 200 FPS (PAL)]", "4"},
        {"500% [300 FPS (NTSC) / 250 FPS (PAL)]", "5"},
        {"1000% [600 FPS (NTSC) / 500 FPS (PAL)]","10"},
        {"Unlimited", "0"},
    };
    s.append({"Emulation", "", "Speed Control", "Framerate", "NominalScalar", "Normal Speed",
              "Sets the target speed for normal gameplay.", SettingDef::Combo, "1", speedOptions, 0, 0, 0});
    s.append({"Emulation", "", "Speed Control", "Framerate", "TurboScalar", "Fast-Forward Speed",
              "Sets the target speed when turbo mode is activated.", SettingDef::Combo, "2", speedOptions, 0, 0, 0});
    s.append({"Emulation", "", "Speed Control", "Framerate", "SlomoScalar", "Slow-Motion Speed",
              "Sets the target speed when slow motion mode is activated.", SettingDef::Combo, "0.5", speedOptions, 0, 0, 0});
```

### Step 1.2: Fix the `OsdShowPatches` key spelling

- [ ] **Edit line 235.** PCSX2's field is declared `OsdshowPatches` (lowercase `s` in `show`) and `SettingsWrapBitBool` stringifies the variable name as the INI key (`Config.h:781`, `Pcsx2Config.cpp:741/967`). Our current key is never read or written by PCSX2.

Replace:

```cpp
    s.append({"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS", "OsdShowPatches", "Show Patches", "", SettingDef::Bool, "false", {}, 0, 0, 0});
```

With:

```cpp
    s.append({"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS", "OsdshowPatches", "Show Patches", "", SettingDef::Bool, "false", {}, 0, 0, 0});
```

(Only the key string changes — `OsdShowPatches` → `OsdshowPatches`. Label, tooltip, type, default unchanged.)

### Step 1.3: Rewrite the audio enum combos

- [ ] **Replace lines 248–263** (`Backend`, `ExpansionMode`, `SyncMode`). PCSX2 round-trips these via `SettingsWrapParsedEnum` using exact-case enum name strings (`AudioStream::GetBackendName`, `GetExpansionModeName`, `GetSyncModeName` in `references/pcsx2-master/pcsx2/Host/AudioStream.cpp:148-221`). Our integer/lowercase strings never match on re-read.

Replace:

```cpp
    s.append({"Audio", "", "Configuration", "SPU2/Output", "Backend", "Backend", "",
              SettingDef::Combo, "cubeb",
              {{"Cubeb", "cubeb"}, {"SDL", "sdl"}, {"Null (No Sound)", "null"}}, 0, 0, 0});
    s.append({"Audio", "", "Configuration", "SPU2/Output", "DriverName", "Driver", "",
              SettingDef::Combo, "",
              {{"Default", ""}, {"audiounit", "audiounit"}}, 0, 0, 0});
    s.append({"Audio", "", "Configuration", "SPU2/Output", "DeviceName", "Output Device", "",
              SettingDef::Combo, "",
              {{"Default", ""}}, 0, 0, 0});
    s.append({"Audio", "", "Configuration", "SPU2/Output", "ExpansionMode", "Expansion", "",
              SettingDef::Combo, "0",
              {{"Disabled (Stereo)", "0"}, {"Stereo with LFE", "1"}, {"Quadraphonic", "2"},
               {"Quadraphonic with LFE", "3"}, {"5.1 Surround", "4"}, {"7.1 Surround", "5"}}, 0, 0, 0});
    s.append({"Audio", "", "Configuration", "SPU2/Output", "SyncMode", "Synchronization", "",
              SettingDef::Combo, "1",
              {{"Disabled (Noisy)", "0"}, {"TimeStretch (Recommended)", "1"}}, 0, 0, 0});
```

With:

```cpp
    // Audio enum combos use exact-case enum name strings, not integers — see
    // AudioStream::GetBackendName / GetExpansionModeName / GetSyncModeName in
    // references/pcsx2-master/pcsx2/Host/AudioStream.cpp:148-221. Audit 2026-04-06.
    s.append({"Audio", "", "Configuration", "SPU2/Output", "Backend", "Backend", "",
              SettingDef::Combo, "Cubeb",
              {{"Cubeb", "Cubeb"}, {"SDL", "SDL"}, {"Null (No Sound)", "Null"}}, 0, 0, 0});
    // TODO(audit-tier-4): DriverName/DeviceName should be enumerated at runtime
    // from the selected backend (Cubeb driver list, host audio device list).
    // Hard-coded options here are macOS-specific and exclude most real devices.
    // Deferred until a shared mechanism is designed across all three adapters.
    s.append({"Audio", "", "Configuration", "SPU2/Output", "DriverName", "Driver", "",
              SettingDef::Combo, "",
              {{"Default", ""}, {"audiounit", "audiounit"}}, 0, 0, 0});
    s.append({"Audio", "", "Configuration", "SPU2/Output", "DeviceName", "Output Device", "",
              SettingDef::Combo, "",
              {{"Default", ""}}, 0, 0, 0});
    s.append({"Audio", "", "Configuration", "SPU2/Output", "ExpansionMode", "Expansion", "",
              SettingDef::Combo, "Disabled",
              {{"Disabled (Stereo)", "Disabled"}, {"Stereo with LFE", "StereoLFE"},
               {"Quadraphonic", "Quadraphonic"}, {"Quadraphonic with LFE", "QuadraphonicLFE"},
               {"5.1 Surround", "Surround51"}, {"7.1 Surround", "Surround71"}}, 0, 0, 0});
    s.append({"Audio", "", "Configuration", "SPU2/Output", "SyncMode", "Synchronization", "",
              SettingDef::Combo, "TimeStretch",
              {{"Disabled (Noisy)", "Disabled"}, {"TimeStretch (Recommended)", "TimeStretch"}}, 0, 0, 0});
```

### Step 1.4: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`

  Expected: clean build, no warnings or errors related to `pcsx2_adapter.cpp`.

### Step 1.5: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(pcsx2): correct settings that fail to round-trip with native INI

Speed control combos (Nominal/Turbo/SlomoScalar), audio enum combos
(Backend/ExpansionMode/SyncMode), and the misspelled OsdShowPatches key
all wrote values PCSX2 could not match on re-read, causing the UI to
silently revert to defaults. Fixes from audit 2026-04-06.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Tier 2 — Misleading combo + range mismatch

### Step 2.1: Drop the broken `FullscreenMode` combo

- [ ] **Remove line 107–108.** PCSX2's `FullscreenMode` is a free-form display-mode string (`"WxH@refresh"`) produced at runtime; the literal `"native"` we currently write is meaningless to PCSX2 and the option silently does nothing. Per the chosen Option (a) from the audit discussion, drop the setting entirely — borderless fullscreen (the empty-string default) is what the app already uses in practice.

Delete these two lines:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "FullscreenMode", "Fullscreen Mode", "",
              SettingDef::Combo, "", {{"Borderless Fullscreen", ""}, {"Native Desktop", "native"}}, 0, 0, 0});
```

(No replacement — the row simply disappears from the Graphics → Display page. PCSX2 will continue to use borderless fullscreen because that's its default when the key is absent.)

### Step 2.2: Widen `OutputLatencyMS` upper bound to 500

- [ ] **Edit line 266–267.** PCSX2's native FullscreenUI exposes a 1..500 range (`FullscreenUI_Settings.cpp:3464`); the stream engine clamps to 0..65535 (`AudioStream.cpp:784`). Our 0..200 cap is unnecessarily restrictive.

Replace:

```cpp
    s.append({"Audio", "", "Configuration", "SPU2/Output", "OutputLatencyMS", "Output Latency", "",
              SettingDef::Int, "20", {}, 0, 200, 5, "slider", "ms"});
```

With:

```cpp
    s.append({"Audio", "", "Configuration", "SPU2/Output", "OutputLatencyMS", "Output Latency", "",
              SettingDef::Int, "20", {}, 0, 500, 5, "slider", "ms"});
```

### Step 2.3: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`

  Expected: clean build.

### Step 2.4: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(pcsx2): drop non-functional FullscreenMode combo, widen OutputLatencyMS

FullscreenMode's "native" option wrote a value PCSX2 doesn't recognise;
borderless fullscreen (empty default) is what the app uses anyway, so
the setting is removed. OutputLatencyMS upper bound widened from 200 to
500 to match PCSX2's native FullscreenUI range. Audit 2026-04-06.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Tier 3 — Type purity & missing combo entries

### Step 3.1: Declare `StretchY` and `OsdScale` as `Float`

- [ ] **Edit lines 125–126 and 208–209.** Native PCSX2 stores both as `float` (`Config.h:848`, `Config.h:851`). Our `Int` type works for whole numbers because `100.0f` round-trips as `"100"`, but declaring `Float` is more accurate and lets users enter fractional values.

Replace line 125–126:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "StretchY", "Vertical Stretch", "",
              SettingDef::Int, "100", {}, 10, 300, 1, "", "%"});
```

With:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "StretchY", "Vertical Stretch", "",
              SettingDef::Float, "100", {}, 10, 300, 1, "", "%"});
```

Replace line 208–209:

```cpp
    s.append({"Graphics", "OSD", "On-Screen Display", "EmuCore/GS", "OsdScale", "OSD Scale", "",
              SettingDef::Int, "100", {}, 25, 500, 25, "", "%"});
```

With:

```cpp
    s.append({"Graphics", "OSD", "On-Screen Display", "EmuCore/GS", "OsdScale", "OSD Scale", "",
              SettingDef::Float, "100", {}, 25, 500, 25, "", "%"});
```

### Step 3.2: Add the missing `10:7` aspect ratio option

- [ ] **Edit line 109–111.** PCSX2's `AspectRatio` enum supports `Stretch | Auto 4:3/3:2 | 4:3 | 16:9 | 10:7` (`Pcsx2Config.cpp:639-645`). We currently expose four; add `10:7`.

Replace:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "Auto 4:3/3:2",
              {{"Auto 4:3/3:2", "Auto 4:3/3:2"}, {"4:3", "4:3"}, {"16:9", "16:9"}, {"Stretch", "Stretch"}}, 0, 0, 0});
```

With:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "Auto 4:3/3:2",
              {{"Auto 4:3/3:2", "Auto 4:3/3:2"}, {"4:3", "4:3"}, {"16:9", "16:9"},
               {"10:7 (Native/Full)", "10:7"}, {"Stretch", "Stretch"}}, 0, 0, 0});
```

### Step 3.3: Add the missing `AdaptiveBFF` deinterlace option

- [ ] **Edit lines 116–120.** PCSX2's `GSInterlaceMode` has 10 values (`Config.h:291-303`); we expose 9. Add `Adaptive (Bottom)` mapping to `"9"`, and rename our existing `Adaptive` to `Adaptive (Top)` for symmetry with the other Top/Bottom pairs.

Replace:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "deinterlace_mode", "Deinterlacing", "",
              SettingDef::Combo, "0",
              {{"Automatic", "0"}, {"Off", "1"}, {"Weave (Top)", "2"}, {"Weave (Bottom)", "3"},
               {"Bob (Top)", "4"}, {"Bob (Bottom)", "5"}, {"Blend (Top)", "6"}, {"Blend (Bottom)", "7"},
               {"Adaptive", "8"}}, 0, 0, 0});
```

With:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "deinterlace_mode", "Deinterlacing", "",
              SettingDef::Combo, "0",
              {{"Automatic", "0"}, {"Off", "1"}, {"Weave (Top)", "2"}, {"Weave (Bottom)", "3"},
               {"Bob (Top)", "4"}, {"Bob (Bottom)", "5"}, {"Blend (Top)", "6"}, {"Blend (Bottom)", "7"},
               {"Adaptive (Top)", "8"}, {"Adaptive (Bottom)", "9"}}, 0, 0, 0});
```

### Step 3.4: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`

  Expected: clean build. The `Float` type is already defined in `setting_def.h:11` and supported by `emulator_settings_page.cpp`, so no other code changes are needed.

### Step 3.5: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(pcsx2): correct setting types and add missing combo entries

Declare StretchY/OsdScale as Float to match native (round-trip-safe but
allows fractional values). Add 10:7 aspect ratio and Adaptive (Bottom)
deinterlace mode that PCSX2 supports natively. Audit 2026-04-06.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Final manual smoke test

After all three commits land, do a quick end-to-end check that the changed settings actually persist through PCSX2.

### Step 4.1: Launch the app

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project && ./cpp/build/EmulatorFrontend`

### Step 4.2: For each fixed setting, verify round-trip

For each item below: open Settings → PCSX2, change the value to something non-default, close Settings, reopen Settings, confirm the value you set is shown (not the default).

- [ ] **Normal Speed** — set to `200%`, reopen, confirm it still shows `200%`.
- [ ] **Fast-Forward Speed** — set to `300%`, reopen, confirm.
- [ ] **Slow-Motion Speed** — set to `25%`, reopen, confirm.
- [ ] **Show Patches** (Graphics → OSD) — toggle on, reopen, confirm still on. Then inspect `{root}/emulators/PCSX2/inis/PCSX2.ini` and confirm the line reads `OsdshowPatches = true` (lowercase `s`).
- [ ] **Audio Backend** — switch to `SDL`, reopen, confirm. Inspect INI, confirm `Backend = SDL`.
- [ ] **Audio Expansion** — switch to `5.1 Surround`, reopen, confirm. Inspect INI, confirm `ExpansionMode = Surround51`.
- [ ] **Audio Synchronization** — switch to `Disabled (Noisy)`, reopen, confirm. Inspect INI, confirm `SyncMode = Disabled`.
- [ ] **Aspect Ratio** — confirm `10:7 (Native/Full)` appears in the dropdown and persists if selected.
- [ ] **Deinterlacing** — confirm `Adaptive (Bottom)` appears and persists.
- [ ] **OSD Scale** — set to a non-100 value (e.g. 150), reopen, confirm. Should still accept whole numbers.
- [ ] **Output Latency** — confirm slider now goes up to 500.
- [ ] **Fullscreen Mode** — confirm the setting no longer appears on the Graphics → Display page.

### Step 4.3: One real launch

- [ ] Launch any PCSX2 game from the app and confirm it boots normally (no crash from a malformed setting). Quit the game.

### Step 4.4: Inspect the post-launch INI

- [ ] Open `{root}/emulators/PCSX2/inis/PCSX2.ini` after PCSX2 has run once and confirm none of the changed keys have reverted to a different format. Specifically check `[Framerate] NominalScalar`, `[SPU2/Output] Backend`, `[SPU2/Output] ExpansionMode`, `[SPU2/Output] SyncMode`, `[EmuCore/GS] OsdshowPatches`.

If everything above passes, the audit's Tier 1–3 fixes are verified.

---

## Self-review notes

- **Spec coverage:** Every Tier 1, Tier 2, and Tier 3 finding from the audit has a corresponding step. Tier 4 is explicitly deferred and left with a TODO comment in code (Step 1.3).
- **Placeholder check:** No TBDs or "implement later"; every code-changing step shows the exact before/after blocks.
- **Type consistency:** The `Float` type is referenced in Step 3.1; verified it exists in `setting_def.h:11` and is already handled by the existing settings UI code.
- **OsdShowPatches migration:** Existing user INIs may contain a stale `OsdShowPatches = ...` line written by older versions of our app. PCSX2 will simply ignore it (it's reading `OsdshowPatches`), so there is no migration to write — the stale key is harmless and will be overwritten naturally if the user opens settings and saves.
