# Default Settings Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Update every `SettingDef::defaultValue` in the three emulator adapters so that fresh installs match each emulator's standalone-install defaults, with two explicit overrides (internal resolution → `1`, aspect ratio → `4:3`) per the design spec.

**Architecture:** Pure data edits to `cpp/src/adapters/{pcsx2,duckstation,ppsspp}_adapter.cpp` inside their respective `settingsSchema()` functions. No new files, no API changes, no behavioural code touched. Three small commits, one per adapter, easy to review and revert independently.

**Tech Stack:** C++17, Qt6 (`SettingDef` struct from `cpp/src/core/setting_def.h`).

**Source documents:**
- Spec: `docs/superpowers/specs/2026-04-07-default-settings-sync-design.md`
- Inventory: `docs/superpowers/audits/2026-04-07-default-mismatch-inventory.md` (the source of truth — every change in this plan comes from there)

**Note on testing:** No unit tests for adapter schemas (same situation as the round-trip audits). Verification is (a) clean build after each commit, and (b) a final manual smoke test handed back to the user.

**Total scope:** 8 single-line `defaultValue` edits across 3 files. PCSX2 = 2 edits, DuckStation = 5 edits, PPSSPP = 1 edit. The other "explicit override" entries from the spec (`upscale_multiplier`, `ResolutionScale`, DuckStation `AspectRatio`) are already at their target values and need no change.

---

## File Map

**Modify only:**
- `cpp/src/adapters/pcsx2_adapter.cpp`
- `cpp/src/adapters/duckstation_adapter.cpp`
- `cpp/src/adapters/ppsspp_adapter.cpp`

For reference, the `SettingDef` aggregate-initializer field order (from `setting_def.h`) is: `{ category, subcategory, group, section, key, label, tooltip, type, defaultValue, options, minVal, maxVal, step, layout, suffix, dependsOn, bitmask }`.

---

## Task 1: PCSX2 — sync defaults to native

Two edits: one native sync (`vuThread`), one explicit override (`AspectRatio`). The explicit override for `upscale_multiplier` is already correct (`"1"`) and needs no change.

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:72-73` (`vuThread`)
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:110-113` (`AspectRatio`)

### Step 1.1: Sync `vuThread` to native default `false`

PCSX2's `Pcsx2Config::SpeedhackOptions::SpeedhackOptions()` constructor calls `DisableAll()` then enables only `WaitLoop`, `IntcStat`, `vuFlagHack`, and `vu1Instant` — `vuThread` stays `false` (`references/pcsx2-master/pcsx2/Pcsx2Config.cpp:378-396`). Our schema currently defaults it to `"true"`.

- [ ] **Edit lines 72-73.** Change the 9th argument (`defaultValue`) from `"true"` to `"false"`.

Replace:

```cpp
    s.append({"Emulation", "", "System Settings", "EmuCore/Speedhacks", "vuThread", "Enable Multithreaded VU1 (MTVU)",
              "Runs VU1 on a second thread. Substantial speed improvement in most games.", SettingDef::Bool, "true", {}, 0, 0, 0});
```

With:

```cpp
    s.append({"Emulation", "", "System Settings", "EmuCore/Speedhacks", "vuThread", "Enable Multithreaded VU1 (MTVU)",
              "Runs VU1 on a second thread. Substantial speed improvement in most games.", SettingDef::Bool, "false", {}, 0, 0, 0});
```

### Step 1.2: Override `AspectRatio` default to `"4:3"`

Native PCSX2 default is `Auto 4:3/3:2` (`references/pcsx2-master/pcsx2/Pcsx2Config.cpp:931`), but per the design spec the schema default should be `4:3` regardless. The setup wizard overrides this at install time anyway.

- [ ] **Edit lines 110-113.** Change `defaultValue` from `"Auto 4:3/3:2"` to `"4:3"`. Options list stays unchanged.

Replace:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "Auto 4:3/3:2",
              {{"Auto 4:3/3:2", "Auto 4:3/3:2"}, {"4:3", "4:3"}, {"16:9", "16:9"},
               {"10:7", "10:7"}, {"Stretch", "Stretch"}}, 0, 0, 0});
```

With:

```cpp
    s.append({"Graphics", "Display", "", "EmuCore/GS", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "4:3",
              {{"Auto 4:3/3:2", "Auto 4:3/3:2"}, {"4:3", "4:3"}, {"16:9", "16:9"},
               {"10:7", "10:7"}, {"Stretch", "Stretch"}}, 0, 0, 0});
```

### Step 1.3: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`
  Expected: clean build. Pre-existing warnings in unrelated files are fine.

### Step 1.4: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(pcsx2): sync defaults to native standalone install

Sync vuThread default from "true" to "false" to match native PCSX2's
SpeedhackOptions constructor (Pcsx2Config.cpp:378-396), and override
AspectRatio default to "4:3" per the design spec (the setup wizard
overrides this at install time anyway). See spec
docs/superpowers/specs/2026-04-07-default-settings-sync-design.md.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: DuckStation — sync defaults to native

Five edits: four native syncs (`TurboSpeed`, `LoadImagePatches`, `FastForwardAccess`, `DeinterlacingMode`, `OptimalFramePacing`) and zero explicit overrides — both `ResolutionScale` and `AspectRatio` are already at their target values (`"1"` and `"4:3"`) and need no change.

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:145` (`FastForwardAccess`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:172` (`LoadImagePatches`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:184-185` (`TurboSpeed`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:190` (`OptimalFramePacing`)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:253-257` (`DeinterlacingMode`)

### Step 2.1: Sync `FastForwardAccess` to native default `false`

Native: `bool memory_card_fast_forward_access : 1 = false` (`references/duckstation-master/src/core/settings.h:311`).

- [ ] **Edit line 145.** Change `defaultValue` from `"true"` to `"false"`.

Replace:

```cpp
    s.append({"Console", "", "Console", "MemoryCards", "FastForwardAccess", "Fast Forward Memory Card Access", "", SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});
```

With:

```cpp
    s.append({"Console", "", "Console", "MemoryCards", "FastForwardAccess", "Fast Forward Memory Card Access", "", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
```

### Step 2.2: Sync `LoadImagePatches` to native default `false`

Native: `bool cdrom_load_image_patches : 1 = false` (`references/duckstation-master/src/core/settings.h:316`).

- [ ] **Edit line 172.** Change `defaultValue` from `"true"` to `"false"`.

Replace:

```cpp
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "LoadImagePatches", "Apply Image Patches", "Applies PPF patches found alongside the disc image.", SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});
```

With:

```cpp
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "LoadImagePatches", "Apply Image Patches", "Applies PPF patches found alongside the disc image.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
```

### Step 2.3: Sync `TurboSpeed` to native default `0` (Unlimited)

Native: `float turbo_speed = 0.0f` (`references/duckstation-master/src/core/settings.h:380`). The current `"2"` was a deliberate UX override from the round-trip audit; the spec instructs us to revert UX overrides in favour of strict native sync.

- [ ] **Edit lines 184-185.** Change `defaultValue` from `"2"` to `"0"`.

Replace:

```cpp
    s.append({"Emulation", "", "Speed Control", "Main", "TurboSpeed", "Turbo Speed", "Speed used when the turbo hotkey is held.",
              SettingDef::Combo, "2", turbospeedOptions, 0, 0, 0, "", ""});
```

With:

```cpp
    s.append({"Emulation", "", "Speed Control", "Main", "TurboSpeed", "Turbo Speed", "Speed used when the turbo hotkey is held.",
              SettingDef::Combo, "0", turbospeedOptions, 0, 0, 0, "", ""});
```

### Step 2.4: Sync `OptimalFramePacing` to native macOS default `true`

Native (macOS): `DEFAULT_OPTIMAL_FRAME_PACING` is `true` on every platform except Android and ARM64 Linux (`references/duckstation-master/src/core/settings.h:267-272`). The field is declared `bool display_optimal_frame_pacing : 1 = DEFAULT_OPTIMAL_FRAME_PACING` at `settings.h:101`. Our app targets desktop macOS, so the macOS value applies.

- [ ] **Edit line 190.** Change `defaultValue` from `"false"` to `"true"`.

Replace:

```cpp
    s.append({"Emulation", "", "Latency Control", "Display", "OptimalFramePacing", "Optimal Frame Pacing", "Enables an optimal frame pacing mode that reduces jitter.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
```

With:

```cpp
    s.append({"Emulation", "", "Latency Control", "Display", "OptimalFramePacing", "Optimal Frame Pacing", "Enables an optimal frame pacing mode that reduces jitter.", SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});
```

### Step 2.5: Sync `DeinterlacingMode` to native default `Progressive`

Native: `DEFAULT_DISPLAY_DEINTERLACING_MODE = DisplayDeinterlacingMode::Progressive` (`references/duckstation-master/src/core/settings.h:234`), which writes the string `"Progressive"` per the name table at `settings.cpp:1887`. The current `"Adaptive"` was a deliberate UX override left in place during the round-trip audit ("Adaptive looks better for retro PS1 content"); the spec instructs us to revert this.

- [ ] **Edit lines 253-257.** Change `defaultValue` from `"Adaptive"` to `"Progressive"`. Options list stays unchanged.

Replace:

```cpp
    s.append({"Graphics", "Rendering", "", "GPU", "DeinterlacingMode", "Deinterlacing", "",
              SettingDef::Combo, "Adaptive",
              {{"Progressive (Optimal)", "Progressive"}, {"Disabled", "Disabled"},
               {"Weave", "Weave"}, {"Blend", "Blend"}, {"Adaptive", "Adaptive"}},
              0, 0, 0, "", ""});
```

With:

```cpp
    s.append({"Graphics", "Rendering", "", "GPU", "DeinterlacingMode", "Deinterlacing", "",
              SettingDef::Combo, "Progressive",
              {{"Progressive (Optimal)", "Progressive"}, {"Disabled", "Disabled"},
               {"Weave", "Weave"}, {"Blend", "Blend"}, {"Adaptive", "Adaptive"}},
              0, 0, 0, "", ""});
```

### Step 2.6: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`
  Expected: clean build.

### Step 2.7: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/duckstation_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(duckstation): sync defaults to native standalone install

Five defaults updated to match native DuckStation defaults: TurboSpeed
"2" → "0" (native is 0/unlimited, settings.h:380), LoadImagePatches
"true" → "false" (settings.h:316), FastForwardAccess "true" → "false"
(settings.h:311), OptimalFramePacing "false" → "true" on macOS
(settings.h:267-272 + 101), DeinterlacingMode "Adaptive" → "Progressive"
(DEFAULT_DISPLAY_DEINTERLACING_MODE at settings.h:234, reverts the
deliberate UX override left in place by the round-trip audit).
ResolutionScale and AspectRatio explicit overrides per the design spec
are already at their target values ("1" and "4:3") and need no change.
See spec docs/superpowers/specs/2026-04-07-default-settings-sync-design.md.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: PPSSPP — sync defaults to native

One edit: the `InternalResolution` explicit override per the design spec. Every other static-default `SettingDef` in PPSSPP's schema already matches its native `ConfigSetting()` literal. `AchievementVolume` ("75") matches its runtime computation and needs no change despite being a runtime-computed default.

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp:94-100` (`InternalResolution`)

### Step 3.1: Override `InternalResolution` default to `"1"` (1× native PSP resolution)

Native PPSSPP `DefaultInternalResolution()` is runtime-computed and depends on display size and build flags (`references/ppsspp-master/Core/Config.cpp:403-418`) — on macOS SDL builds it returns `2` for displays ≥1000px and `1` otherwise. Per the design spec, the schema default is `"1"` regardless (1× native PSP resolution; the setup wizard overrides at install time).

- [ ] **Edit lines 94-100.** Change the 9th SettingDef field (`defaultValue`) from `"2"` to `"1"`. Options list stays unchanged.

Replace:

```cpp
    s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "2",
              {{"Auto (1:1)", "0"}, {"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"},
               {"3x (1440x816)", "3"}, {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"},
               {"6x (2880x1632)", "6"}, {"7x (3360x1904)", "7"}, {"8x (3840x2176)", "8"},
               {"9x (4320x2448)", "9"}, {"10x (4800x2720)", "10"}}, 0, 0, 0});
```

With:

```cpp
    s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "1",
              {{"Auto (1:1)", "0"}, {"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"},
               {"3x (1440x816)", "3"}, {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"},
               {"6x (2880x1632)", "6"}, {"7x (3360x1904)", "7"}, {"8x (3840x2176)", "8"},
               {"9x (4320x2448)", "9"}, {"10x (4800x2720)", "10"}}, 0, 0, 0});
```

### Step 3.2: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`
  Expected: clean build.

### Step 3.3: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(ppsspp): sync InternalResolution default to 1x native PSP resolution

Override the InternalResolution schema default from "2" to "1" per the
design spec — 1× native PSP resolution (480×272) is the most authentic
baseline, and the setup wizard overrides at install time anyway. Every
other static-default SettingDef in PPSSPP's settingsSchema() already
matches its native ConfigSetting() literal in Core/Config.cpp. See spec
docs/superpowers/specs/2026-04-07-default-settings-sync-design.md.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Final manual smoke test (USER)

After all three commits land, do an end-to-end check that the default values display correctly in the settings UI and that the two reset paths produce the same state. This task is handed back to the user — a subagent cannot run the GUI.

### Step 4.1: Launch the app

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project && ./cpp/build/EmulatorFrontend`

### Step 4.2: Verify changed PCSX2 defaults

- [ ] **Enable Multithreaded VU1 (MTVU)** (Emulation → System Settings) — open the PCSX2 settings page on a fresh install (or after Reset Configuration). The MTVU checkbox should be **OFF** (was previously ON).
- [ ] **Aspect Ratio** (Graphics → Display) — should default to **`4:3`** (was previously `Auto 4:3/3:2`).

### Step 4.3: Verify changed DuckStation defaults

- [ ] **Fast Forward Memory Card Access** (Console → Console) — should default to **OFF** (was ON).
- [ ] **Apply Image Patches** (Console → CD-ROM Emulation) — should default to **OFF** (was ON).
- [ ] **Turbo Speed** (Emulation → Speed Control) — should default to **`Unlimited`** / `0` (was `200%`).
- [ ] **Optimal Frame Pacing** (Emulation → Latency Control) — should default to **ON** (was OFF).
- [ ] **Deinterlacing** (Graphics → Rendering) — should default to **`Progressive (Optimal)`** (was `Adaptive`).

### Step 4.4: Verify changed PPSSPP default

- [ ] **Rendering Resolution** (Graphics → Rendering) — should default to **`1x PSP (480x272)`** (was `2x (960x544)`).

### Step 4.5: Verify both reset paths produce the same state

For one emulator (any of the three):

- [ ] Open settings, change a few values away from default, save.
- [ ] Click **Reset Settings** (this calls `resetSettings()` which writes our `defaultValue` to every key).
- [ ] Confirm the values you changed are back to the new defaults from this fix.
- [ ] Now change a few values again, save.
- [ ] Click **Reset Configuration** (this calls `resetConfiguration()` which deletes the INI entirely and re-runs `ensureConfig()`).
- [ ] Confirm the values are back to the new defaults from this fix.
- [ ] Both reset paths should produce visually identical settings pages.

### Step 4.6: Real game launch

- [ ] Launch a game on each of the three emulators with the new defaults. Confirm each boots normally and runs at the expected baseline (1× native resolution, 4:3 aspect ratio, etc.).

If everything passes, the default-settings-sync work is verified.

---

## Self-review notes

- **Spec coverage:** Every entry in the inventory file (1 PCSX2 native + 2 PCSX2 overrides, 5 DuckStation native + 2 DuckStation overrides, 1 PPSSPP override) is accounted for. The "explicit override" entries that already match their target value (PCSX2 `upscale_multiplier`, DuckStation `ResolutionScale`, DuckStation `AspectRatio`) are explicitly noted as no-change in the relevant task introduction. PPSSPP `AchievementVolume` is explicitly noted as already-correct in Task 3's introduction.
- **Placeholder check:** No TBDs; every code-changing step has the exact before/after blocks.
- **Type consistency:** No new types introduced. Every change is a `defaultValue` string edit; the surrounding `SettingDef` aggregate fields stay byte-identical.
- **Migration concern:** Existing user INIs are unaffected. Settings the user has actively changed are stored in the INI and read from disk; only the displayed default for unchanged keys, and the values written by `resetSettings()`, change after this fix.
- **The spec's `AchievementVolume = 60` instruction is incorrect** — corrected inline in the spec doc earlier. The correct value is `"75"` and the schema is already at `"75"`. Task 3 reflects the correction.
