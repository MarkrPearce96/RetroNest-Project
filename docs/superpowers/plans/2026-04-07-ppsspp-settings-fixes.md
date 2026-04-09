# PPSSPP Settings Schema Fixes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply Tier 1 fixes from the 2026-04-07 PPSSPP settings audit so that every non-controller setting in `PPSSPPAdapter::settingsSchema()` round-trips correctly with native PPSSPP.

**Architecture:** Pure data edits to `cpp/src/adapters/ppsspp_adapter.cpp` inside `PPSSPPAdapter::settingsSchema()`. No new files, no API changes, no behavioural code touched. Single commit because the audit was clean (only 4 ERRORs across 51 settings, all in adjacent areas of the schema).

**Tech Stack:** C++17, Qt6 (`SettingDef` struct from `cpp/src/core/setting_def.h`).

**Source documents:**
- Audit: `docs/superpowers/audits/2026-04-07-ppsspp-settings-audit.md`
- Spec: `docs/superpowers/specs/2026-04-07-ppsspp-settings-audit-design.md`

**Out of scope (deliberate decisions from triage):**
- **`Rendering Resolution` default mismatch (WARN):** Cosmetic only — our `"2"` matches PPSSPP's runtime-computed default for ≥1000px displays. Leave as-is.
- **`Achievement Sound Volume` default mismatch (WARN):** Cosmetic only — our `"75"` is close to PPSSPP's logarithmic-mapped default. Leave as-is.

**Note on testing:** No unit tests for adapter schemas. Verification is (a) clean build, and (b) a final manual smoke-test pass.

---

## File Map

**Modify only:** `cpp/src/adapters/ppsspp_adapter.cpp`

All changes localised to `PPSSPPAdapter::settingsSchema()`. Four edits in two adjacent areas of the function (Frame Pacing block and Audio backend block) plus the Overlay block at the end.

---

## Task 1: Tier 1 — Round-trip ERRORs

All four fixes in one commit. Pure data edits.

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp:127-132` (FrameRate / FrameRate2 conversion to Combo)
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp:280-282` (AudioBufferSize lower bound)
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp:308` (Debug Overlay section + key)

### Step 1.1: Fix `Debug Overlay` section and key

PPSSPP registers this setting in `generalSettings[]` (which maps to INI section `[General]`) with the literal key `DebugOverlay` — no Hungarian `i` prefix (`references/ppsspp-master/Core/Config.cpp:255`). Our schema currently writes `[Graphics] iDebugOverlay`, which PPSSPP never reads.

Note: PPSSPP marks this setting `CfgFlag::DONT_SAVE`, so PPSSPP itself never writes it back to the INI on shutdown. But our app patches the INI before launch via `ensureConfig`, so the value persists across PPSSPP runs as long as it's changed through our settings UI. The existing tooltip already explains the "outside this app" caveat correctly.

- [ ] **Edit line 308.** Change the section from `"Graphics"` to `"General"` and the key from `"iDebugOverlay"` to `"DebugOverlay"`. Everything else (label, tooltip, default, options) is unchanged.

Replace:

```cpp
    s.append({"Overlay", "", "", "Graphics", "iDebugOverlay", "Debug Overlay",
```

With:

```cpp
    s.append({"Overlay", "", "", "General", "DebugOverlay", "Debug Overlay",
```

(Only the 4th and 5th `SettingDef` fields change. Lines 309–321 — the rest of the `s.append` — stay byte-identical.)

### Step 1.2: Bump `AudioBufferSize` lower bound to 128

PPSSPP's SDL backend silently clamps anything below 128 to 128 (`references/ppsspp-master/SDL/SDLMain.cpp:150`). Our schema currently allows users to pick 64, which is meaningless.

- [ ] **Edit line 282.** Change `minVal` from `64` to `128`. Default `"256"`, max `2048`, step `64` are unchanged.

Replace:

```cpp
    s.append({"Audio", "Audio backend", "", "Sound", "AudioBufferSize", "Buffer Size",
              "Audio buffer size in samples. Smaller = less latency but more crackling risk.",
              SettingDef::Int, "256", {}, 64, 2048, 64, "slider", ""});
```

With:

```cpp
    s.append({"Audio", "Audio backend", "", "Sound", "AudioBufferSize", "Buffer Size",
              "Audio buffer size in samples. Smaller = less latency but more crackling risk.",
              SettingDef::Int, "256", {}, 128, 2048, 64, "slider", ""});
```

### Step 1.3: Convert `FrameRate` (Alternative Speed) from Int slider to Combo with pre-computed FPS values

PPSSPP's `iFpsLimit1` is stored as a raw FPS cap, NOT a percentage. Native PPSSPP UI does the percent ↔ FPS conversion via `iFpsLimit1 = (percent * 60) / 100` (`references/ppsspp-master/UI/GameSettingsScreen.cpp:1677`). Our adapter currently writes the percent value directly to the FPS field, so a user picking "200" gets a 200 FPS cap (≈333% of native), not 120 FPS.

The fix is to convert the setting from an Int slider with `%` suffix to a Combo where each option's display label shows the percentage but the underlying INI value is the matching FPS. This is a pure data fix — no UI code changes needed.

- [ ] **Edit lines 127–129.** Replace the Int slider declaration with a Combo whose option list bakes in the percent ↔ FPS conversion.

Replace:

```cpp
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameRate", "Alternative Speed (%)",
              "Target FPS when using fast-forward. 0 = unlimited.",
              SettingDef::Int, "0", {}, 0, 300, 10, "slider", "%"});
```

With:

```cpp
    // PPSSPP stores iFpsLimit1 as raw FPS, not percent — native UI converts
    // user-entered percent into FPS via (percent * 60) / 100. We pre-bake the
    // FPS values into combo INI values so the user still sees percentages but
    // the INI gets the right cap. See audit 2026-04-07.
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameRate", "Alternative Speed",
              "Speed used when the alternative-speed hotkey is held.",
              SettingDef::Combo, "0",
              {{"Unlimited (No Cap)", "0"},
               {"25% (15 FPS)",  "15"},
               {"50% (30 FPS)",  "30"},
               {"75% (45 FPS)",  "45"},
               {"100% (60 FPS)", "60"},
               {"125% (75 FPS)", "75"},
               {"150% (90 FPS)", "90"},
               {"200% (120 FPS)","120"},
               {"300% (180 FPS)","180"},
               {"500% (300 FPS)","300"}},
              0, 0, 0});
```

(Note: the label loses the `(%)` suffix because the combo entries already include the percentage. The tooltip is also slightly updated to drop the now-stale "0 = unlimited" mention, which is captured in the first option's display label.)

### Step 1.4: Convert `FrameRate2` (Alternative Speed 2) from Int slider to Combo

Same fix as Step 1.3, but with an additional `Disabled` option mapping to `-1` (the native default for `iFpsLimit2`, which means "no second alternative speed").

- [ ] **Edit lines 130–132.** Replace the Int slider declaration with a Combo.

Replace:

```cpp
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameRate2", "Alternative Speed 2 (%)",
              "Second FPS target for toggling. -1 = disabled, 0 = unlimited.",
              SettingDef::Int, "-1", {}, -1, 300, 10, "slider", "%"});
```

With:

```cpp
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameRate2", "Alternative Speed 2",
              "Second alternative speed for toggling. Same FPS-vs-percent caveat as above.",
              SettingDef::Combo, "-1",
              {{"Disabled",         "-1"},
               {"Unlimited (No Cap)", "0"},
               {"25% (15 FPS)",  "15"},
               {"50% (30 FPS)",  "30"},
               {"75% (45 FPS)",  "45"},
               {"100% (60 FPS)", "60"},
               {"125% (75 FPS)", "75"},
               {"150% (90 FPS)", "90"},
               {"200% (120 FPS)","120"},
               {"300% (180 FPS)","180"},
               {"500% (300 FPS)","300"}},
              0, 0, 0});
```

### Step 1.5: Build

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project/cpp && cmake --build build`
  Expected: clean build. Pre-existing warnings in unrelated files are fine.

### Step 1.6: Commit

- [ ] Run:

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "$(cat <<'EOF'
fix(ppsspp): correct settings that fail to round-trip with native INI

Debug Overlay was registered against the wrong section and key
([Graphics] iDebugOverlay) — PPSSPP actually reads it from
[General] DebugOverlay (no Hungarian prefix). AudioBufferSize allowed
selecting values below 128 that the SDL backend silently clamps.
Alternative Speed and Alternative Speed 2 wrote percent values directly
into iFpsLimit1/2, which PPSSPP stores as raw FPS — picking "200"
capped at 200 FPS instead of 120. Converted both to combos with
pre-computed FPS values so the UI still shows percentages but the INI
gets the correct cap. Fixes from audit 2026-04-07.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Final manual smoke test (USER)

After the commit lands, do an end-to-end check that the changed settings actually persist through PPSSPP. This task is handed back to the user — a subagent cannot run the GUI.

The PPSSPP settings.ini path on macOS is inside the .app bundle: `/Users/mark/Documents/EmuFront/emulators/ppsspp/...` (check the actual path under your EmuFront root — same pattern as DuckStation).

### Step 2.1: Launch the app

- [ ] Run: `cd /Users/mark/Documents/EmuFront-Project && ./cpp/build/EmulatorFrontend`

### Step 2.2: Round-trip checks

For each item below: open Settings → PPSSPP, change to a non-default value, close Settings, reopen, confirm the value you set is shown. For settings with INI checks, also inspect the PPSSPP `.ini` file on disk.

- [ ] **Debug Overlay** (Overlay tab) — set `Debug Stats`, reopen, confirm. INI should now show the line `DebugOverlay = 1` under the `[General]` section (NOT `iDebugOverlay = 1` under `[Graphics]`). Then check that the old `iDebugOverlay` line is either absent or harmless (PPSSPP will ignore it).

- [ ] **Audio Buffer Size** (Audio → Audio backend) — confirm the slider's lower bound is now 128 (not 64). Set to 128, then 256, then 512. INI should read `iSDLAudioBufferSize = 128` / `256` / `512` (or `AudioBufferSize` — verify the actual key after a save).

- [ ] **Alternative Speed** (Graphics → Frame Pacing) — set to `200% (120 FPS)`. Reopen settings, confirm the dropdown shows `200% (120 FPS)`. INI should read `iFpsLimit1 = 120` (or `FrameRate = 120` — verify the actual on-disk key spelling). Then set to `100% (60 FPS)` and confirm INI reads `60`. Then set to `Unlimited (No Cap)` and confirm INI reads `0`.

- [ ] **Alternative Speed 2** — set to `Disabled`, reopen, confirm. INI should read `-1`. Then set to `300% (180 FPS)` and confirm INI reads `180`.

### Step 2.3: Verify the audit-clean settings still work

These were OK in the audit and shouldn't be affected, but a quick spot-check confirms the schema-wide commit didn't break anything adjacent:

- [ ] **Backend** (Graphics → Rendering) — confirm `Vulkan` is still selected and changing to/from OpenGL still round-trips with the `"3 (VULKAN)"` annotation suffix.
- [ ] **Show FPS Counter / Show Speed / Show Battery %** (Overlay tab) — toggle each. INI should read `iShowStatusFlags` as a bitmask (e.g. `2`, `6`, `14` depending on which combinations are on).

### Step 2.4: Real game launch

- [ ] Launch any PPSSPP game from the app and confirm it boots normally. If you set Alternative Speed to `200% (120 FPS)` and use the alt-speed hotkey, you should see the game cap at ~120 FPS (not 200 FPS). Quit the game.

### Step 2.5: Inspect the post-launch INI

- [ ] After PPSSPP has run once, re-inspect the PPSSPP ini file and confirm:
  - `DebugOverlay` is in the `[General]` section (or absent — PPSSPP doesn't write it back due to `CfgFlag::DONT_SAVE`, but our app patches it before launch, so it should appear there if you set a non-Off value).
  - `iFpsLimit1` / `iFpsLimit2` are stored as the raw FPS numbers from the combo's INI value (60, 120, etc.), NOT as percentages.
  - No keys reverted to a different format that would break the next read.

If everything passes, the audit's Tier 1 fixes are verified.

---

## Self-review notes

- **Spec coverage:** All 4 Tier 1 ERRORs from the audit have corresponding steps. 2 WARNs (Rendering Resolution default, Achievement Volume default) are explicitly out of scope as cosmetic.
- **Placeholder check:** No TBDs; every code-changing step shows the exact before/after blocks.
- **Type consistency:** No new types or fields introduced. The Combo conversion uses existing `SettingDef::Combo` machinery.
- **Migration concern:** Existing user `ppsspp.ini` files may contain old broken values:
  - `[Graphics] iDebugOverlay = N` — PPSSPP will simply ignore the orphan key. Once the user opens our settings page after the upgrade, our save path will write to the correct `[General] DebugOverlay` location. No active migration needed.
  - `[Graphics] FrameRate = N` (where N is a percentage) — the key itself is correct, only the value's interpretation changes. After the fix, on first read our combo won't be able to match `N` exactly to one of its 10 entries, so the UI will display the default "Unlimited" entry. The user will then re-pick their preferred speed and from then on the value is correct. This is a one-time visible reset for affected users; the previous behaviour was already wrong (they were getting the wrong cap), so this is a corrective reset.
- **Tooltip on Debug Overlay:** Left unchanged. The existing tooltip ("PPSSPP doesn't remember this setting between runs … it will reset to Off whenever the emulator is launched outside this app") is still accurate after the fix — PPSSPP itself doesn't persist the value, but inside our app we patch it via `ensureConfig` before launch.
