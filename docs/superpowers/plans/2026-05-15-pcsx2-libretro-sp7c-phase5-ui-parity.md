# SP7c Phase 5 — UI Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the visible-layout gap between the libretro PCSX2 settings dialog and standalone PCSX2's dialog by wiring preview widgets and structurally aligning sub-tab row layouts.

**Architecture:** Two source files touched in RetroNest-Project (`cpp/src/adapters/libretro/pcsx2_libretro_adapter.{h,cpp}`). One method override (`previewSpec`) plugs into the existing adapter-driven preview framework — the preview widgets (`AspectRatioPreview`, `OsdPreview`) and the dispatching `GenericSettingsPage` already exist and need zero modification. The remaining work is reordering/adding/skip-commenting schema rows in the adapter's `.cpp` file. Mirrors the Phase 4 Task-1-through-6 cadence: each task is one commit, each smoke-testable independently.

**Tech Stack:** C++ (Qt 6), CMake, RetroNest-Project's `SettingDef`/`PreviewSpec` types, `GenericSettingsPage` rendering.

**Spec:** `/Users/mark/Documents/Projects/RetroNest-Project/docs/superpowers/specs/2026-05-15-pcsx2-libretro-sp7c-phase5-ui-parity-design.md` (commit `0bbee95`).

**Reference adapter:** `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp` (standalone), specifically `PCSX2Adapter::previewSpec` (line ~1767) and `PCSX2Adapter::settingsSchema`.

**Build command (used across all tasks):**
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && ./scripts/build-universal.sh 2>&1 | tail -10
```
Expected on success: `✓ Universal build complete.`

**Launch command for smoke (used across all tasks):**
```bash
LOG=/tmp/retronest-phase5-$(date +%Y%m%d-%H%M%S).log
DYLD_FRAMEWORK_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/Frameworks \
QT_PLUGIN_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/PlugIns \
arch -x86_64 \
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > "$LOG" 2>&1 &
echo "LOG=$LOG, PID=$!"
```

---

## File Structure

**Modified files (only two):**

- `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h` — add `previewSpec` override declaration (Task 1 only).
- `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — add `previewSpec` body (Task 1); reorder/add/skip-comment schema rows (Tasks 2 + 3).

**No new files.** Preview widgets, dispatch logic, and `PreviewSpec` type all exist; only the adapter is touched.

---

## Task 1: Preview wiring

**Files:**
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`

- [ ] **Step 1.1: Verify reference signature in standalone adapter header**

Read `cpp/src/adapters/pcsx2_adapter.h` and find the `previewSpec` declaration. Confirm it overrides a virtual method from the base `EmulatorAdapter` (or whatever the shared base is).

Run: `grep -nE "previewSpec" cpp/src/adapters/pcsx2_adapter.h`

Expected: a line like
```cpp
PreviewSpec previewSpec(const QString& category, const QString& subcategory) const override;
```

If the signature differs, mirror it exactly in Task 1's library version.

- [ ] **Step 1.2: Add `previewSpec` declaration to libretro adapter header**

Open `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`. Find the `settingsSchema` declaration. Immediately after it, add:

```cpp
    // SP7c Phase 5 — preview widgets for Recommended (aspect) + Graphics
    // On-Screen Display (osd). Returns empty PreviewSpec for every other
    // (category, subcategory) — GenericSettingsPage falls back to no-pane.
    PreviewSpec previewSpec(const QString& category,
                             const QString& subcategory) const override;
```

If the `#include` for `PreviewSpec` isn't already present (it should be — `settingsSchema` already returns `SettingDef` which lives in the same header), search up:

```bash
grep -n "PreviewSpec\|#include" cpp/src/adapters/libretro/pcsx2_libretro_adapter.h | head -10
```

Add an include for the same header standalone uses if missing.

- [ ] **Step 1.3: Add `previewSpec` body to libretro adapter source**

Open `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`. Find the end of `Pcsx2LibretroAdapter::settingsSchema()` (the closing `return s;` then `}`). Immediately after that closing brace, add:

```cpp

// SP7c Phase 5 — Preview wiring.
// Mirrors PCSX2Adapter::previewSpec (adapters/pcsx2_adapter.cpp ~1767). The
// preview widgets (cpp/src/ui/settings/widgets/preview/{aspect_ratio_preview,
// osd_preview}.{h,cpp}) are adapter-agnostic; they expose Qt properties
// named exactly as the values in keyToProperty below, and
// GenericSettingsPage::wirePreviewBinding routes schema-row changes into
// those properties via Qt's meta-object system.
//
// Empty PreviewSpec from every other (category, subcategory) — no preview
// pane rendered. See memory/sp7c_followup_ui_parity.md for the broader
// Phase 5 motivation.
PreviewSpec Pcsx2LibretroAdapter::previewSpec(const QString& category,
                                              const QString& subcategory) const {
    if (category == "Recommended" && subcategory.isEmpty()) {
        return {"aspect", {
            {"pcsx2_aspect_ratio", "aspectMode"},
        }};
    }
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        return {"osd", {
            {"pcsx2_osd_show_fps",                  "showFps"},
            {"pcsx2_osd_show_speed",                "showSpeed"},
            {"pcsx2_osd_show_vps",                  "showVps"},
            {"pcsx2_osd_show_resolution",           "showResolution"},
            {"pcsx2_osd_show_cpu",                  "showCpu"},
            {"pcsx2_osd_show_gpu",                  "showGpu"},
            {"pcsx2_osd_show_settings",             "showSettings"},
            {"pcsx2_osd_show_patches",              "showPatches"},
            {"pcsx2_osd_show_inputs",               "showInputs"},
            {"pcsx2_osd_show_frame_times",          "showFrameTimes"},
            {"pcsx2_osd_show_indicators",           "showIndicators"},
            {"pcsx2_osd_show_gs_stats",             "showGsStats"},
            {"pcsx2_osd_show_hardware_info",        "showHardwareInfo"},
            {"pcsx2_osd_show_version",              "showVersion"},
            {"pcsx2_osd_show_video_capture",        "showVideoCapture"},
            {"pcsx2_osd_show_input_rec",            "showInputRec"},
            {"pcsx2_osd_show_texture_replacements", "showTextureReplacements"},
            {"pcsx2_osd_messages_pos",              "messagesPos"},
            {"pcsx2_osd_performance_pos",           "performancePos"},
            {"pcsx2_osd_scale",                     "osdScale"},
        }};
    }
    return {};
}
```

- [ ] **Step 1.4: Verify all referenced libretro keys exist in current schema**

Sanity-check that the 21 keys above are all defined by `settingsSchema()`. They were verified during brainstorm but verify once more (the build will compile even if a key is missing — `wirePreviewBinding` just silently no-ops).

Run:
```bash
for k in pcsx2_aspect_ratio \
         pcsx2_osd_show_fps pcsx2_osd_show_speed pcsx2_osd_show_vps \
         pcsx2_osd_show_resolution pcsx2_osd_show_cpu pcsx2_osd_show_gpu \
         pcsx2_osd_show_settings pcsx2_osd_show_patches pcsx2_osd_show_inputs \
         pcsx2_osd_show_frame_times pcsx2_osd_show_indicators \
         pcsx2_osd_show_gs_stats pcsx2_osd_show_hardware_info \
         pcsx2_osd_show_version pcsx2_osd_show_video_capture \
         pcsx2_osd_show_input_rec pcsx2_osd_show_texture_replacements \
         pcsx2_osd_messages_pos pcsx2_osd_performance_pos \
         pcsx2_osd_scale; do
  grep -q "\"$k\"" cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp \
    && echo "✓ $k" || echo "✗ MISSING: $k"
done
```

Expected: 21 lines, each prefixed `✓`.

If any line says `✗ MISSING`, stop — either the key got renamed (find the new name and update Task 1's PreviewSpec) or it never got added (a Phase 4 gap; raise as a question before continuing).

- [ ] **Step 1.5: Build**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && ./scripts/build-universal.sh 2>&1 | tail -10
```

Expected: ends with `✓ Universal build complete.`

If the build fails on a missing include for `PreviewSpec`, search the standalone header for which header brings the type in:
```bash
grep -nE "#include.*[Pp]review" cpp/src/adapters/pcsx2_adapter.h cpp/src/adapters/emulator_adapter.h 2>/dev/null
```
and add the same include to the libretro adapter header.

- [ ] **Step 1.6: Smoke — Recommended aspect-ratio preview**

Kill any running RetroNest, then launch:
```bash
pkill -x RetroNest 2>/dev/null
LOG=/tmp/retronest-phase5t1-$(date +%H%M%S).log
DYLD_FRAMEWORK_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/Frameworks \
QT_PLUGIN_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/PlugIns \
arch -x86_64 \
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > "$LOG" 2>&1 &
echo "$LOG"
```

In the running app: open any PS2 game's settings → **Recommended** tab.

Expected: an aspect-ratio preview pane appears on the right side of the page (similar to standalone PCSX2 — a rectangle representing the emulated display proportioned per the current `Aspect Ratio` setting). Changing the `Aspect Ratio` combo causes the preview to reflow.

If no pane appears: check `$LOG` for warnings about "previewType" or "wirePreviewBinding"; verify Step 1.4's grep passed; check standalone's Recommended page renders a preview correctly (sanity that the framework is functional).

- [ ] **Step 1.7: Smoke — Graphics > On-Screen Display preview**

Still in the same app session: navigate to **Graphics** → **On-Screen Display** sub-tab.

Expected: an OSD preview pane appears showing what the OSD will look like in-game. Toggling `Show FPS` makes a row appear/disappear; changing `OSD Scale` changes the size; changing `OSD Messages Position` or `OSD Performance Position` moves the corresponding row to a different corner.

If no pane appears: same diagnostic as Step 1.6.

- [ ] **Step 1.8: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.h cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 5 Task 1: preview wiring for libretro PCSX2 dialog

Override Pcsx2LibretroAdapter::previewSpec to surface the existing
adapter-agnostic AspectRatioPreview (Recommended page) and OsdPreview
(Graphics > On-Screen Display) widgets — mirroring how PCSX2Adapter
already integrates with cpp/src/ui/settings/widgets/preview/*. 21 key
mappings (1 aspect + 20 OSD) all reference libretro core-option keys
defined by SP7c Phases 1-4 (verified pre-commit).

No framework changes — GenericSettingsPage already dispatches
PreviewSpec to widget instantiation; this just connects libretro into
that dispatch.

Smoke verified: Recommended aspect-ratio preview reflows on Aspect
Ratio combo change; Graphics > On-Screen Display preview updates live
on toggle of Show FPS / OSD Scale / position changes.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Top-level cards layout sweep

**Files:**
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` (schema rows only)

**Pre-computed audit summary** (gathered during plan writing; methodology lives in the spec):

| Card | Standalone count | Libretro count | Action |
|---|---|---|---|
| Recommended | 16 (15 + Audio Backend) | 15 | ✅ done in `9dc34f9`; verify-only step. |
| Emulation | 17 (3+9+5) | 15 (3+7+5) | Add 2 rows under System Settings: `pcsx2_mtvu`, `pcsx2_fast_boot`. |
| Audio | 11 (8+3) | 5 (2+3) | All 6 missing are architecturally inert; add skip comments. |
| Memory Cards | 7 (4+3) | 5 (2+3) | 2 missing are hardcoded filenames; add skip comments. |

- [ ] **Step 2.1: Verify Recommended card is unchanged from commit `9dc34f9`**

The Recommended card was overhauled in the prior commit. Verify presence + group structure:

Run:
```bash
grep -cE '"Recommended", "(Performance|Visual Quality|Frame Pacing|Audio|Convenience)"' \
  cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```

Expected: `15`

Run:
```bash
grep -E '"Recommended", "[^"]+"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp \
  | awk -F'"' '{print $4}' | sort | uniq -c
```

Expected:
```
   1 Audio
   3 Convenience
   2 Frame Pacing
   3 Performance
   6 Visual Quality
```

If counts differ, stop and investigate.

- [ ] **Step 2.2: Add `pcsx2_mtvu` row under Emulation → System Settings (position 3)**

Standalone Emulation > System Settings order:
```
EECycleRate, EECycleSkip, vuThread, EnableThreadPinning, EnableCheats,
HostFs, CdvdPrecache, EnableFastBoot, EnableFastBootFastForward
```

Current libretro System Settings order:
```
pcsx2_ee_cycle_rate, pcsx2_ee_cycle_skip, pcsx2_thread_pinning,
pcsx2_cheats, pcsx2_host_fs, pcsx2_cdvd_precache, pcsx2_fast_boot_ff
```

Need to insert `pcsx2_mtvu` between `pcsx2_ee_cycle_skip` and `pcsx2_thread_pinning`.

Find the `s.append(opt(...))` call for `pcsx2_thread_pinning`:
```bash
grep -nE '"pcsx2_thread_pinning"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```

Expected output: line number where that opt() call begins (around line 231).

Use Edit to insert the following BEFORE that `s.append(opt(` line (i.e., between the close of the cycle_skip opt and the open of the thread_pinning opt):

```cpp
    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_mtvu", "Multi-Threaded VU1", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Run the VU1 microprogram on its own thread instead of the EE "
        "thread. Compatible with the vast majority of games and "
        "significantly reduces EE-thread saturation on Apple Silicon. "
        "Disable only if a specific game shows MTVU-related glitches. "
        "Takes effect on next launch."));
```

**Note:** the same key (`pcsx2_mtvu`) is also in the Recommended card from yesterday's commit. Both rows reference the same backing core option; edits in either view update the same value. Phase 4 / SP7b precedent.

- [ ] **Step 2.3: Add `pcsx2_fast_boot` row under Emulation → System Settings (between cdvd_precache and fast_boot_ff)**

Find the `s.append(opt(...))` call for `pcsx2_fast_boot_ff`:
```bash
grep -nE '"pcsx2_fast_boot_ff"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```

Use Edit to insert the following BEFORE that `s.append(opt(` line:

```cpp
    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_fast_boot", "Fast Boot", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Skip the PS2 BIOS Sony intro and region-check screen on launch. "
        "Disable if you want to see the BIOS screen (e.g. to verify your "
        "BIOS region or to use the BIOS browser). Takes effect on next launch."));
```

**Note:** same key already in Recommended from yesterday's commit; both views share backing storage.

- [ ] **Step 2.4: Add skip-comment block in the Audio section explaining the 6 inert standalone rows**

Standalone Audio > Configuration rows that libretro deliberately doesn't expose:
- `Backend` — forced to "Libretro" per SP4 architectural decision (libretro core's audio path bypasses Cubeb/SDL/etc.).
- `DriverName`, `DeviceName`, `OutputLatencyMS`, `OutputLatencyMinimal` — RetroNest's host SDL audio sink owns these (they're upstream of the libretro shim).
- `ExpansionMode` — forced to "Disabled" per SP4 (libretro audio_batch_cb is stereo-only).

Find the Audio block start in the libretro adapter:
```bash
grep -nE '"Audio", "Configuration"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp | head -2
```

Use Edit to add the following comment block IMMEDIATELY BEFORE the first `s.append(opt("Audio", "Configuration", ...))` call:

```cpp
    // SP7c Phase 5 — Audio Configuration parity note.
    // Standalone Pcsx2Adapter (pcsx2_adapter.cpp Audio > Configuration)
    // exposes 8 rows; the libretro variant deliberately surfaces only the
    // 2 that are user-tweakable in this architecture:
    //   • Backend — FORCED to "Libretro" per SP4 (libretro audio_batch_cb
    //     is the only path; Cubeb/SDL/etc. are bypassed). Skipped.
    //   • DriverName / DeviceName / OutputLatencyMS / OutputLatencyMinimal
    //     — owned by RetroNest's host SDL audio sink, which sits upstream
    //     of the libretro core. Skipped (host-side concern).
    //   • ExpansionMode — FORCED to "Disabled" per SP4 (audio_batch_cb is
    //     stereo-only). Skipped.
    // The two libretro-applicable rows (SyncMode, BufferMS) follow.
```

- [ ] **Step 2.5: Add skip-comment block in Memory Cards section for hardcoded filenames**

Standalone Memory Cards > Memory Card Slots includes `Slot1_Filename` + `Slot2_Filename` (text inputs for the memcard image filenames). Libretro hardcodes these to `Mcd001.ps2` / `Mcd002.ps2` because libretro core options are Combo-only — there's no free-form string control type.

Find the Memory Cards block start:
```bash
grep -nE '"Memory Cards", "Memory Card Slots"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp | head -2
```

Use Edit to add the following comment block IMMEDIATELY BEFORE the first `s.append(opt("Memory Cards", "Memory Card Slots", ...))` call:

```cpp
    // SP7c Phase 5 — Memory Card Slots parity note.
    // Standalone Pcsx2Adapter exposes Slot1_Filename + Slot2_Filename as
    // free-form text inputs ("Mcd001.ps2" / "Mcd002.ps2" by default).
    // libretro core options are Combo-only (no free-form string control
    // type), so the filenames are hardcoded in pcsx2-libretro/Settings.cpp
    // to the same defaults. User-tweakable filename support would require
    // either a new control type in the core-options ABI or an out-of-band
    // RetroNest-side override — deferred. Slot enables + Multitap slots
    // follow as user-tweakable rows.
```

- [ ] **Step 2.6: Build**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && ./scripts/build-universal.sh 2>&1 | tail -10
```

Expected: ends with `✓ Universal build complete.`

- [ ] **Step 2.7: Smoke — open all four top-level cards**

Kill any running RetroNest, launch:
```bash
pkill -x RetroNest 2>/dev/null
LOG=/tmp/retronest-phase5t2-$(date +%H%M%S).log
DYLD_FRAMEWORK_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/Frameworks \
QT_PLUGIN_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/PlugIns \
arch -x86_64 \
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > "$LOG" 2>&1 &
echo "$LOG"
```

In the app, open a PS2 game's settings. For each of the 4 top-level cards (Recommended, Emulation, Audio, Memory Cards), verify:

| Card | Expected outcome |
|---|---|
| Recommended | 15 rows, 5 groups, aspect-ratio preview still visible (Task 1 regression check). |
| Emulation | 17 rows: 3 Speed Control + 9 System Settings + 5 Frame Pacing/Latency. `Multi-Threaded VU1` appears between `EE Cycle Skipping` and `Thread Pinning`. `Fast Boot` appears between `CDVD Precache` and `Fast-Forward Through BIOS`. |
| Audio | 5 rows (2 Configuration + 3 Controls). No regression from prior. |
| Memory Cards | 5 rows (2 Slots + 3 Multitap). No regression. |

If any row vanished, group is empty, or row count is wrong: revert latest Edit, re-inspect.

- [ ] **Step 2.8: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 5 Task 2: top-level cards layout-parity sweep

Add 2 rows under Emulation > System Settings (pcsx2_mtvu, pcsx2_fast_boot)
mirroring the standalone Pcsx2Adapter row presence + ordering. Both keys
already exist in the schema (also surfaced via Recommended from commit
9dc34f9); two-view sharing matches the SP7b precedent for renderer/mtvu/
fast_boot. Edits to either view route to the same backing core option.

Add skip-comment blocks documenting why Audio and Memory Cards omit some
standalone rows:
  • Audio: Backend/Driver/Device/OutputLatency/ExpansionMode are
    architecturally inert in the libretro variant (SP4 + RetroNest host
    SDL sink concerns).
  • Memory Cards: Slot1_Filename/Slot2_Filename are hardcoded because
    libretro core options are Combo-only (no free-form strings).

Recommended card already matches standalone structure from commit 9dc34f9;
verify-only step.

Smoke verified on R&C 2: all four top-level cards render expected row
counts and groups; preview pane on Recommended unchanged.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Graphics sub-tabs layout sweep

**Files:**
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` (schema rows only)

**Pre-computed audit summary:**

| Sub-tab | Standalone rows | Libretro rows | Action |
|---|---|---|---|
| Display | 17 | 16 | Add `pcsx2_renderer` row at position 0 (currently only in Recommended). |
| Rendering | 7 (6 + 1 in Hardware Rendering Options) | 7 (all in flat Rendering group) | Move `pcsx2_hw_mipmap` into a new "Hardware Rendering Options" subgroup. |
| Texture Replacement | 6 | 6 | ✅ aligned. |
| Post-Processing | 9 (3 + 6) | 9 (3 + 6) | ✅ aligned (Sharpening/AA before Filters in both). |
| On-Screen Display | 23 (5 + 9 + 6 + 2 + 1) | 23 (5 + 9 + 2 + 6 + 1) | Reorder: standalone places `Settings & Inputs` BEFORE `System Information`; libretro currently reversed. |
| Media Capture | 18 | 0 | Skip with comment (RetroNest handles screenshot/recording host-side). |

- [ ] **Step 3.1: Add `pcsx2_renderer` row at top of Graphics → Display**

Find the first `gopt(...)` call under Display (`pcsx2_aspect_ratio`):
```bash
grep -nE 'gopt\(\s*"Display"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp | head -1
```

Use Edit to insert the following BEFORE the first `s.append(gopt("Display", "Display", ...))` call (the aspect_ratio row):

```cpp
    s.append(gopt(
        "Display", "Display",
        "pcsx2_renderer", "Renderer", "auto",
        {{"Auto", "auto"},
         {"Metal", "metal"},
         {"Software", "software"},
         {"Null", "null"}},
        "PCSX2 graphics backend. Auto picks Metal on macOS. Software is "
        "CPU-only and much slower; useful for debugging rendering bugs "
        "or working around hardware-renderer regressions in specific games. "
        "Takes effect on next launch."));
```

**Note:** same `pcsx2_renderer` key is already in Recommended (Performance group); both views share backing storage per the SP7b two-view-sharing precedent.

- [ ] **Step 3.2: Move `pcsx2_hw_mipmap` into a "Hardware Rendering Options" subgroup**

Find the current `pcsx2_hw_mipmap` row:
```bash
grep -nE '"pcsx2_hw_mipmap"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```

Open the file and find that row. It currently uses `gopt("Rendering", "Rendering", ...)`. Change the second argument to `"Hardware Rendering Options"`:

Before (locate this exact pattern):
```cpp
    s.append(gopt(
        "Rendering", "Rendering",
        "pcsx2_hw_mipmap", "Hardware Mipmapping", "enabled",
```

After:
```cpp
    s.append(gopt(
        "Rendering", "Hardware Rendering Options",
        "pcsx2_hw_mipmap", "Hardware Mipmapping", "enabled",
```

That's the only edit needed — the gopt's second argument is the group name. Moving the row into its own group puts it under a separate group header in the dialog, matching standalone.

- [ ] **Step 3.3: Reorder OSD sub-tab — swap `System Information` and `Settings & Inputs` groups**

Standalone On-Screen Display group order:
```
On-Screen Display (5) → Performance Stats (9) → Settings & Inputs (6) → System Information (2) → Messages (1)
```

Current libretro order:
```
On-Screen Display (5) → Performance Stats (9) → System Information (2) → Settings & Inputs (6) → Messages (1)
```

The 2 `System Information` rows (pcsx2_osd_show_hardware_info, pcsx2_osd_show_version) currently sit between the last Performance Stats row and the first Settings & Inputs row. Need to move that 2-row block to AFTER the Settings & Inputs block.

Find the System Information block:
```bash
grep -nE '"On-Screen Display", "System Information"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```

This will identify the 2 `s.append(gopt(...))` calls for `pcsx2_osd_show_hardware_info` and `pcsx2_osd_show_version`.

Find the boundary between Settings & Inputs and Messages (the last S&I row is `pcsx2_osd_show_texture_replacements`; the Messages row is `pcsx2_warn_about_unsafe_settings`):
```bash
grep -nE '"pcsx2_osd_show_texture_replacements"|"pcsx2_warn_about_unsafe_settings"' \
  cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```

Edit plan:
1. Cut the 2 entire `s.append(gopt("On-Screen Display", "System Information", ...))` calls for hardware_info + version (including their multi-line `})` closings).
2. Paste them after the `s.append(gopt(..., "pcsx2_osd_show_texture_replacements", ...))` call's closing `}));` and before the `s.append(gopt(..., "pcsx2_warn_about_unsafe_settings", ...))` call.

If using two Edit calls: first Edit removes the System Information block; second Edit inserts it at the new location.

**Critical:** do NOT rename any `pcsx2_osd_*` key during this move — Task 1's `previewSpec` references them by exact name. Verify after edit:
```bash
grep -cE '"pcsx2_osd_show_hardware_info"|"pcsx2_osd_show_version"' \
  cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```
Expected: `2` (one occurrence of each, still present in the schema).

- [ ] **Step 3.4: Add Media Capture skip-comment block at end of Graphics section**

Find the last `gopt(...)` call (the Messages-group row `pcsx2_warn_about_unsafe_settings` is the final Graphics row in current code):
```bash
grep -nE '"pcsx2_warn_about_unsafe_settings"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```

After that row's closing `}));`, add the following comment block:

```cpp
    // SP7c Phase 5 — Media Capture sub-tab parity note.
    // Standalone Pcsx2Adapter (pcsx2_adapter.cpp Graphics > Media Capture)
    // exposes 18 rows across "Screenshot Capture Setup" (3) and
    // "Video Recording Setup" (15) — codec selection, container, bitrate,
    // resolution-mode, etc.
    //
    // The libretro variant deliberately skips Media Capture entirely:
    // RetroNest's host application owns the screenshot + video-recording
    // pipeline (libretro frontends are responsible for capture; PCSX2's
    // own capture code path is inert when running inside a libretro
    // shell). Duplicating a feature RetroNest already provides would be
    // confusing — same architectural reasoning as the Achievements card
    // skip (RetroNest owns rcheevos host-side).
    //
    // If a future RetroNest UI surfaces capture configuration, those
    // settings live in RetroNest-Project's own settings hub, not here.
```

- [ ] **Step 3.5: Build**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && ./scripts/build-universal.sh 2>&1 | tail -10
```

Expected: ends with `✓ Universal build complete.`

- [ ] **Step 3.6: Smoke — Graphics sub-tabs row counts**

Kill any running RetroNest, launch:
```bash
pkill -x RetroNest 2>/dev/null
LOG=/tmp/retronest-phase5t3-$(date +%H%M%S).log
DYLD_FRAMEWORK_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/Frameworks \
QT_PLUGIN_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/PlugIns \
arch -x86_64 \
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > "$LOG" 2>&1 &
echo "$LOG"
```

In the app, open Graphics card → each sub-tab:

| Sub-tab | Expected |
|---|---|
| Display | 17 rows. First row is `Renderer`. |
| Rendering | 7 rows. Last row `Hardware Mipmapping` appears under its own `Hardware Rendering Options` subgroup header. |
| Texture Replacement | 6 rows. No change from prior. |
| Post-Processing | 9 rows. Sharpening/Anti-Aliasing (3) before Filters (6). No change from prior. |
| On-Screen Display | 23 rows. Group order: On-Screen Display → Performance Stats → **Settings & Inputs** → **System Information** → Messages. Preview pane on right (Task 1 regression check). Toggling Show FPS / OSD Scale still drives preview live. |

If counts or group order are wrong: revert the offending Edit and retry.

- [ ] **Step 3.7: Smoke — Task 1 OSD preview regression check**

Still in the same app session, in Graphics → On-Screen Display:
- Toggle `Show FPS` — corresponding row in preview pane appears/disappears.
- Change `OSD Scale` from 100 to 150 — preview rows visibly enlarge.
- Change `OSD Messages Position` to a different corner — preview row moves.

Expected: all three reactions work, identical to Task 1's smoke 1.7.

If preview no longer reacts: an OSD key got renamed during Step 3.3's reorder. Diff against pre-reorder state:
```bash
git diff HEAD -- cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp \
  | grep -E '^\+.*"pcsx2_osd_|^-.*"pcsx2_osd_' \
  | head -40
```
Look for any deleted key without a matching added key — that's the rename.

- [ ] **Step 3.8: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 5 Task 3: Graphics sub-tabs layout-parity sweep

Bring the 5 Graphics sub-tabs into structural parity with standalone
Pcsx2Adapter:
  • Display: add pcsx2_renderer at position 0 (already in Recommended
    via SP7b; two-view sharing).
  • Rendering: move pcsx2_hw_mipmap into its own "Hardware Rendering
    Options" subgroup, matching standalone.
  • Texture Replacement / Post-Processing: already aligned, no edits.
  • On-Screen Display: swap System Information (2 rows) with Settings
    & Inputs (6 rows) so group order matches standalone (S&I before
    SysInfo). Preview-widget key bindings unchanged (verified post-
    edit grep).
  • Media Capture: skip with explanatory comment — RetroNest owns
    screenshot/recording host-side, same reasoning as the Achievements
    card skip.

Smoke verified on R&C 2: all five Graphics sub-tabs render expected
row counts and group orders; Task 1's OSD preview still reacts live
to Show FPS / OSD Scale / position changes.

Phase 5 complete — closes the SP7c sub-project.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

**Spec coverage check** (each spec requirement → which task implements it):

| Spec section | Task |
|---|---|
| Preview wiring (Recommended aspect) | Task 1 Step 1.3 (the `Recommended`/`aspect` branch) |
| Preview wiring (Graphics OSD) | Task 1 Step 1.3 (the `Graphics`/`On-Screen Display` branch) |
| Top-level Recommended verify | Task 2 Step 2.1 |
| Top-level Emulation sweep | Task 2 Steps 2.2 + 2.3 |
| Top-level Audio sweep | Task 2 Step 2.4 (skip comments) |
| Top-level Memory Cards sweep | Task 2 Step 2.5 (skip comments) |
| Graphics Display sweep | Task 3 Step 3.1 |
| Graphics Rendering sweep | Task 3 Step 3.2 |
| Graphics Texture Replacement / Post-Processing | (No edits required — audit confirms already aligned. Smoke covers verification.) |
| Graphics On-Screen Display sweep | Task 3 Step 3.3 |
| Media Capture skip | Task 3 Step 3.4 |
| OSD preview unchanged after Task 3 | Task 3 Step 3.7 (explicit regression check) |
| Per-task smoke before commit | Each task has a dedicated smoke step before its commit step |

All spec requirements have a concrete task.

**Placeholder scan:** searched for `TBD`, `TODO`, `audit`, `implement later` — none present in the plan. Every step has either an explicit Edit operation with exact code, or an explicit grep/build/launch command with expected output.

**Type consistency check:** `PreviewSpec` referenced in Task 1 matches the type in standalone `pcsx2_adapter.h`. `previewSpec(category, subcategory)` signature is consistent between header (Step 1.2) and body (Step 1.3). All `pcsx2_osd_*` keys in Task 1's bindings spelled identically to their `s.append(gopt(...))` definitions in the existing schema (verified by Step 1.4 grep gate). The hw_mipmap group rename in Step 3.2 doesn't change its key.
