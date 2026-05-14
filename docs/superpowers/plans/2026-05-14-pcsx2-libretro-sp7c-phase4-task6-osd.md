# SP7c Phase 4 Task 6 — On-Screen Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the 23 OSD knobs from PCSX2's `Graphics → On-Screen Display` sub-tab as libretro core options + matching host rows in RetroNest's PCSX2 per-emulator settings dialog. Closes Phase 4 at schema-fidelity **89/89**.

**Architecture:** Two-commit pair, mirror of every prior Phase 4 task:
- **Bundle A (core)** in `pcsx2-libretro` — `Values::Osd` struct (23 fields), `AppendDefinitions`/`Parse`/`ApplyDefaults` blocks, multi-line per-launch echo, two new `test_core_options` cases. Schema fidelity goes RED (89 core, 66 host) at this commit by design.
- **Bundle B (host)** in `RetroNest-Project` — 23 `gopt(...)` rows under `subcategory="On-Screen Display"` in the libretro PCSX2 adapter. Schema fidelity returns GREEN at 89/89 once this commit lands.

**Tech Stack:** C++17 (`pcsx2-libretro` libretro core; `RetroNest-Project` Qt host adapter). No new dependencies.

**Spec:** `docs/superpowers/specs/2026-05-14-pcsx2-libretro-sp7c-phase4-task6-osd-design.md` (committed at `ddeb239` on `RetroNest-Project` `main`).

**Source verification anchors (verified at pickup 2026-05-14):**
- `pcsx2/Config.h:341-353` — `OsdOverlayPos` enum (`None=0, TopLeft=1, ..., BottomRight=9`).
- `pcsx2/Config.h:730-733` — `DEFAULT_OSD_SCALE=100`, `DEFAULT_OSD_MARGIN=10`, `DEFAULT_OSD_MESSAGE_POS=TopLeft (1)`, `DEFAULT_OSD_PERFORMANCE_POS=TopRight (3)`.
- `pcsx2/Config.h:768-786` — OSD bitfields. Note `OsdshowPatches` lowercase 's' at `:781`.
- `pcsx2/Pcsx2Config.cpp:720-745` — `Defaults()` body for GSOptions. Note: NO line for `OsdBoldText` → bitfield zero-init → `false`.
- `pcsx2/Pcsx2Config.cpp:1943` — `WarnAboutUnsafeSettings = true` in EmuCoreOptions `Defaults()`.
- Standalone adapter (reference shape only): `RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp:797-898`.

---

## File Structure

**Core repo (`/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/`):**

| File | Change | Purpose |
|---|---|---|
| `CoreOptionsGraphics.h` | Modify lines 120-122 (Osd stub → 23-field struct) | Typed value bag for the OSD sub-tab |
| `CoreOptionsGraphics.cpp` | Append after line 833 (AppendDefinitions block); append after line 944 (Parse); append after line 1004 (ApplyDefaults — INSIDE `#ifndef CORE_OPTIONS_TEST_ONLY`, before the closing `}` at line 1005) | 23 push_back blocks + 23 Parse if/queries + 23 SetXValue writes |
| `CoreOptions.cpp` | Append after line 186 (before the `return r;` at line 188) | 5-line per-launch `[CoreOptions] graphics.osd*:` echo |
| `tools/test_core_options.cpp` | Append after line 595 (before `std::printf("\n%d failure(s)\n", failures);` at line 597) | Cases 17 + 17b |

**Host repo (`/Users/mark/Documents/Projects/RetroNest-Project/`):**

| File | Change | Purpose |
|---|---|---|
| `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` | Append after line ~957 (last `s.append(gopt(...))` of the Post-Processing block) | 23 `s.append(gopt(...))` rows under `"On-Screen Display"`, 5 groups, dependsOn fan-out per spec |

**Test/verification:**

| File | Change | Purpose |
|---|---|---|
| `pcsx2-libretro/tools/check_schema_fidelity.py` | None | Should pass 89/89 after Bundle B |
| `pcsx2-libretro/tools/test_core_options` (binary) | Rebuild during Bundle A | Should pass 42/42 0 failures |

---

## Task 1: Bundle A — Core (pcsx2-libretro)

**Files:**
- Modify: `pcsx2-libretro/CoreOptionsGraphics.h:120-122`
- Modify: `pcsx2-libretro/CoreOptionsGraphics.cpp` (3 insertion sites)
- Modify: `pcsx2-libretro/CoreOptions.cpp` (1 insertion site)
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (1 insertion site)

Working directory: `/Users/mark/Documents/Projects/pcsx2-libretro/`

- [ ] **Step 1: Replace the Osd stub with the 23-field struct**

Edit `pcsx2-libretro/CoreOptionsGraphics.h:120-122`. Old text (3 lines):
```cpp
    struct Osd {
        // Phase 4 Task 6 fills these.
    } osd;
```

Replace with:
```cpp
    struct Osd {
        // 23 knobs mirroring standalone PCSX2 Graphics/On-Screen Display
        // sub-tab. 22 fields stored under [EmuCore/GS];
        // warn_about_unsafe_settings stored under [EmuCore]. Defaults
        // match PCSX2 source verbatim (Config.h:730-733,768-786 +
        // Pcsx2Config.cpp:720-745,1943). OsdBoldText defaults to false
        // because the bitfield zero-inits and there is no explicit
        // assignment in Defaults() — the standalone adapter's
        // default="true" is incorrect (separate SP7c followup).
        int  osd_scale            = 100;   // px-scale, neutral=100
        int  osd_margin           = 10;    // px from screen edge
        int  osd_messages_pos     = 1;     // OsdOverlayPos::TopLeft
        int  osd_performance_pos  = 3;     // OsdOverlayPos::TopRight
        bool osd_bold_text        = false;
        // Performance Stats group (9)
        bool osd_show_speed         = false;
        bool osd_show_fps           = false;
        bool osd_show_vps           = false;
        bool osd_show_resolution    = false;
        bool osd_show_gs_stats      = false;
        bool osd_show_cpu           = false;
        bool osd_show_gpu           = false;
        bool osd_show_indicators    = true;   // ← only non-false in PerfStats
        bool osd_show_frame_times   = false;
        // System Information group (2)
        bool osd_show_hardware_info = false;
        bool osd_show_version       = false;
        // Settings & Inputs group (6)
        bool osd_show_settings              = false;
        bool osd_show_patches               = false;
        bool osd_show_inputs                = false;
        bool osd_show_video_capture         = true;   // libretro-inert
        bool osd_show_input_rec             = true;   // libretro-inert
        bool osd_show_texture_replacements  = false;
        // Messages group (1) — stored under [EmuCore]
        bool warn_about_unsafe_settings     = true;
    } osd;
```

- [ ] **Step 2: Append 23 AppendDefinitions blocks after the post-processing block**

Insert into `pcsx2-libretro/CoreOptionsGraphics.cpp` immediately before the closing `}` of `AppendDefinitions` (currently at line 834). The 9 ShadeBoost_Gamma push_back ends at line 833 (`        "50",\n    });`). Insert the following block of 23 push_backs after line 833 and before line 834's `}`:

```cpp

    // ── On-Screen Display sub-tab (Phase 4 Task 6) ──
    //
    // 23 knobs. 22 stored under [EmuCore/GS]; warn_about_unsafe_settings
    // under [EmuCore] (split-section like Task 2 patches rows). Defaults
    // verified against pcsx2/Config.h:730-733,768-786 + Pcsx2Config.cpp:
    // 720-745,1943.
    //
    // OsdScale + OsdMargin are int sliders standalone-side; libretro v2
    // is Combo-only, so they use enumerated stops (Task 2 Crop + Task 5
    // CAS Sharpness precedent). 8 stops each, default sits on a stop.
    //
    // OsdMessagesPos + OsdPerformancePos use the 10-stop OsdOverlayPos
    // enum (None=0, TopLeft=1, TopCenter=2, TopRight=3, CenterLeft=4,
    // Center=5, CenterRight=6, BottomLeft=7, BottomCenter=8,
    // BottomRight=9). Per Config.h:341-353.

    // -- "On-Screen Display" group (5 rows) --

    out.push_back({
        "pcsx2_osd_scale",
        "OSD Scale",
        nullptr,
        "Global multiplier applied to every OSD overlay. 100% matches "
        "standalone PCSX2's default size. Standalone exposes a "
        "25-500% slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "50",  "50%" },
            { "75",  "75%" },
            { "100", "100% (Default)" },
            { "125", "125%" },
            { "150", "150%" },
            { "200", "200%" },
            { "300", "300%" },
            { "500", "500%" },
            { nullptr, nullptr },
        },
        "100",
    });

    out.push_back({
        "pcsx2_osd_margin",
        "OSD Margin",
        nullptr,
        "Pixel offset between the OSD elements and the screen edge. "
        "Standalone exposes a 0-100px slider; libretro offers "
        "enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0px" },
            { "5",   "5px" },
            { "10",  "10px (Default)" },
            { "15",  "15px" },
            { "20",  "20px" },
            { "30",  "30px" },
            { "50",  "50px" },
            { "100", "100px" },
            { nullptr, nullptr },
        },
        "10",
    });

    out.push_back({
        "pcsx2_osd_messages_pos",
        "OSD Messages Position",
        nullptr,
        "Corner where transient messages (save-state loaded, shader "
        "reload, etc.) are drawn. Set to None to hide them entirely.",
        nullptr,
        nullptr,
        {
            { "0", "None" },
            { "1", "Top Left (Default)" },
            { "2", "Top Center" },
            { "3", "Top Right" },
            { "4", "Center Left" },
            { "5", "Center" },
            { "6", "Center Right" },
            { "7", "Bottom Left" },
            { "8", "Bottom Center" },
            { "9", "Bottom Right" },
            { nullptr, nullptr },
        },
        "1",
    });

    out.push_back({
        "pcsx2_osd_performance_pos",
        "OSD Performance Position",
        nullptr,
        "Corner where the performance stats column (FPS/Speed/CPU/GPU/"
        "etc.) is drawn. Set to None to hide the column and grey out "
        "every Performance Stats / System Information toggle.",
        nullptr,
        nullptr,
        {
            { "0", "None" },
            { "1", "Top Left" },
            { "2", "Top Center" },
            { "3", "Top Right (Default)" },
            { "4", "Center Left" },
            { "5", "Center" },
            { "6", "Center Right" },
            { "7", "Bottom Left" },
            { "8", "Bottom Center" },
            { "9", "Bottom Right" },
            { nullptr, nullptr },
        },
        "3",
    });

    out.push_back({
        "pcsx2_osd_bold_text",
        "OSD Text Style (Bold)",
        nullptr,
        "Renders OSD text in bold. Easier to read on bright scenes.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // -- "Performance Stats" group (9 rows; host-side dependsOn pcsx2_osd_performance_pos!=0) --

    out.push_back({
        "pcsx2_osd_show_speed",
        "Show Speed Percentages",
        nullptr,
        "Displays the emulation speed as a percentage. Red below 95%, "
        "green above 105%.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_fps",
        "Show FPS",
        nullptr,
        "Displays the current frame rate reported by the GS. Useful "
        "for spotting performance issues.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_vps",
        "Show VPS",
        nullptr,
        "Displays vertical syncs per second — the PS2 display refresh "
        "reported by the GS.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_resolution",
        "Show Resolution",
        nullptr,
        "Displays the PS2 internal render resolution and interlacing "
        "mode.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_gs_stats",
        "Show GS Statistics",
        nullptr,
        "Displays per-frame GS statistics: draw-call count, VRAM use, "
        "and a frame-time summary.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_cpu",
        "Show CPU Usage",
        nullptr,
        "Displays per-component CPU usage (EE, GS, VU).",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_gpu",
        "Show GPU Usage",
        nullptr,
        "Displays GPU usage percentage and frame time in milliseconds.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_indicators",
        "Show Status Indicators",
        nullptr,
        "Displays icons for pause, fast-forward, slow-motion, and "
        "turbo modes in the top-right corner.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled (Default)" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_osd_show_frame_times",
        "Show Frame Times",
        nullptr,
        "Displays a rolling graph of recent frame times to visualise "
        "stutter.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // -- "System Information" group (2 rows; host-side dependsOn pcsx2_osd_performance_pos!=0) --

    out.push_back({
        "pcsx2_osd_show_hardware_info",
        "Show Hardware Info",
        nullptr,
        "Displays the CPU and GPU model names as two lines in the "
        "performance column.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_version",
        "Show PCSX2 Version",
        nullptr,
        "Displays the PCSX2 version string in the performance column.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // -- "Settings & Inputs" group (6 rows; mixed dependsOn host-side) --

    out.push_back({
        "pcsx2_osd_show_settings",
        "Show Settings",
        nullptr,
        "Displays a compact summary of active emulation settings in "
        "the bottom-right corner.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_patches",
        "Show Patches",
        nullptr,
        "Appends active patches (widescreen, no-interlacing, etc.) to "
        "the settings line. Requires Show Settings to be enabled.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_inputs",
        "Show Inputs",
        nullptr,
        "Displays the current controller input state at the "
        "bottom-left corner.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_video_capture",
        "Show Video Capture Status",
        nullptr,
        "Displays a recording indicator while video capture is "
        "active. (Inert in the libretro variant — no FFmpeg capture "
        "is driven from this build.)",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled (Default)" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_osd_show_input_rec",
        "Show Input Recording Status",
        nullptr,
        "Displays an indicator while input recording is active. "
        "(Inert in the libretro variant — input recording UI is not "
        "driven from this build.)",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled (Default)" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_osd_show_texture_replacements",
        "Show Texture Replacement Status",
        nullptr,
        "Displays an indicator when replacement textures are loaded "
        "for the current game. Requires Texture Replacement → Load "
        "Textures to be enabled.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // -- "Messages" group (1 row; host-side dependsOn pcsx2_osd_messages_pos!=0) --
    // NOTE: stored under [EmuCore] (not [EmuCore/GS]). Same split as
    // Task 2's widescreen/no-interlacing patches rows.

    out.push_back({
        "pcsx2_warn_about_unsafe_settings",
        "Warn About Unsafe Settings",
        nullptr,
        "Shows a startup warning if any unsafe settings are enabled.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled (Default)" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });
```

- [ ] **Step 3: Append 23 Parse if/query blocks after the post-processing parse block**

Insert into `pcsx2-libretro/CoreOptionsGraphics.cpp` immediately before the closing `}` of `Parse` (currently at line 945). The post-processing Parse block ends at line 944 (`        out.post_processing.shade_boost_gamma = parse_int(v, 50);`). Insert the following block after line 944 and before line 945's `}`:

```cpp

    // ── On-Screen Display sub-tab ──
    if (const char* v = query("pcsx2_osd_scale"))
        out.osd.osd_scale = parse_int(v, 100);
    if (const char* v = query("pcsx2_osd_margin"))
        out.osd.osd_margin = parse_int(v, 10);
    if (const char* v = query("pcsx2_osd_messages_pos"))
        out.osd.osd_messages_pos = parse_int(v, 1);
    if (const char* v = query("pcsx2_osd_performance_pos"))
        out.osd.osd_performance_pos = parse_int(v, 3);
    if (const char* v = query("pcsx2_osd_bold_text"))
        out.osd.osd_bold_text = parse_bool(v);

    if (const char* v = query("pcsx2_osd_show_speed"))
        out.osd.osd_show_speed = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_fps"))
        out.osd.osd_show_fps = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_vps"))
        out.osd.osd_show_vps = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_resolution"))
        out.osd.osd_show_resolution = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_gs_stats"))
        out.osd.osd_show_gs_stats = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_cpu"))
        out.osd.osd_show_cpu = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_gpu"))
        out.osd.osd_show_gpu = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_indicators"))
        out.osd.osd_show_indicators = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_frame_times"))
        out.osd.osd_show_frame_times = parse_bool(v);

    if (const char* v = query("pcsx2_osd_show_hardware_info"))
        out.osd.osd_show_hardware_info = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_version"))
        out.osd.osd_show_version = parse_bool(v);

    if (const char* v = query("pcsx2_osd_show_settings"))
        out.osd.osd_show_settings = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_patches"))
        out.osd.osd_show_patches = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_inputs"))
        out.osd.osd_show_inputs = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_video_capture"))
        out.osd.osd_show_video_capture = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_input_rec"))
        out.osd.osd_show_input_rec = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_texture_replacements"))
        out.osd.osd_show_texture_replacements = parse_bool(v);

    if (const char* v = query("pcsx2_warn_about_unsafe_settings"))
        out.osd.warn_about_unsafe_settings = parse_bool(v);
```

- [ ] **Step 4: Append 23 ApplyDefaults writes after the post-processing ApplyDefaults block**

Insert into `pcsx2-libretro/CoreOptionsGraphics.cpp` immediately before the closing `}` of `ApplyDefaults` (currently at line 1005, INSIDE the `#ifndef CORE_OPTIONS_TEST_ONLY` guard). The post-processing block ends at line 1004 (`    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Gamma",      v.post_processing.shade_boost_gamma);`). Insert the following block after line 1004 and before line 1005's `}`:

```cpp

    // ── On-Screen Display sub-tab ──
    // 22 fields under [EmuCore/GS]; warn_about_unsafe_settings under
    // [EmuCore] (split-section, see spec). OsdshowPatches INI key
    // preserves PCSX2's lowercase-'s' typo verbatim (Config.h:781) —
    // mis-spelling here would silently no-op (PCSX2 ignores the
    // correctly-cased key on read).
    si.SetIntValue ("EmuCore/GS", "OsdScale",                   v.osd.osd_scale);
    si.SetIntValue ("EmuCore/GS", "OsdMargin",                  v.osd.osd_margin);
    si.SetIntValue ("EmuCore/GS", "OsdMessagesPos",             v.osd.osd_messages_pos);
    si.SetIntValue ("EmuCore/GS", "OsdPerformancePos",          v.osd.osd_performance_pos);
    si.SetBoolValue("EmuCore/GS", "OsdBoldText",                v.osd.osd_bold_text);

    si.SetBoolValue("EmuCore/GS", "OsdShowSpeed",               v.osd.osd_show_speed);
    si.SetBoolValue("EmuCore/GS", "OsdShowFPS",                 v.osd.osd_show_fps);
    si.SetBoolValue("EmuCore/GS", "OsdShowVPS",                 v.osd.osd_show_vps);
    si.SetBoolValue("EmuCore/GS", "OsdShowResolution",          v.osd.osd_show_resolution);
    si.SetBoolValue("EmuCore/GS", "OsdShowGSStats",             v.osd.osd_show_gs_stats);
    si.SetBoolValue("EmuCore/GS", "OsdShowCPU",                 v.osd.osd_show_cpu);
    si.SetBoolValue("EmuCore/GS", "OsdShowGPU",                 v.osd.osd_show_gpu);
    si.SetBoolValue("EmuCore/GS", "OsdShowIndicators",          v.osd.osd_show_indicators);
    si.SetBoolValue("EmuCore/GS", "OsdShowFrameTimes",          v.osd.osd_show_frame_times);

    si.SetBoolValue("EmuCore/GS", "OsdShowHardwareInfo",        v.osd.osd_show_hardware_info);
    si.SetBoolValue("EmuCore/GS", "OsdShowVersion",             v.osd.osd_show_version);

    si.SetBoolValue("EmuCore/GS", "OsdShowSettings",            v.osd.osd_show_settings);
    si.SetBoolValue("EmuCore/GS", "OsdshowPatches",             v.osd.osd_show_patches); // lowercase-s, see Config.h:781
    si.SetBoolValue("EmuCore/GS", "OsdShowInputs",              v.osd.osd_show_inputs);
    si.SetBoolValue("EmuCore/GS", "OsdShowVideoCapture",        v.osd.osd_show_video_capture);
    si.SetBoolValue("EmuCore/GS", "OsdShowInputRec",            v.osd.osd_show_input_rec);
    si.SetBoolValue("EmuCore/GS", "OsdShowTextureReplacements", v.osd.osd_show_texture_replacements);

    // Split section: WarnAboutUnsafeSettings lives in [EmuCore], not
    // [EmuCore/GS]. Matches Pcsx2Config.cpp:1943 placement.
    si.SetBoolValue("EmuCore",    "WarnAboutUnsafeSettings",    v.osd.warn_about_unsafe_settings);
```

- [ ] **Step 5: Append the 5-line per-launch echo**

Edit `pcsx2-libretro/CoreOptions.cpp`. The post-processing echo ends at line 186 (the closing `);` of the `FrontendLog` call) and is followed by `\n    return r;` at line 188. Insert after line 186 and before line 188's `return r;`:

```cpp

    const auto& go = r.graphics.osd;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] graphics.osd: scale=%d margin=%d msg_pos=%d "
        "perf_pos=%d bold=%s",
        go.osd_scale, go.osd_margin, go.osd_messages_pos,
        go.osd_performance_pos,
        go.osd_bold_text ? "on" : "off");
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] graphics.osd.perf:  spd=%s fps=%s vps=%s res=%s "
        "gs=%s cpu=%s gpu=%s ind=%s ft=%s",
        go.osd_show_speed       ? "on" : "off",
        go.osd_show_fps         ? "on" : "off",
        go.osd_show_vps         ? "on" : "off",
        go.osd_show_resolution  ? "on" : "off",
        go.osd_show_gs_stats    ? "on" : "off",
        go.osd_show_cpu         ? "on" : "off",
        go.osd_show_gpu         ? "on" : "off",
        go.osd_show_indicators  ? "on" : "off",
        go.osd_show_frame_times ? "on" : "off");
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] graphics.osd.sys:   hw=%s ver=%s",
        go.osd_show_hardware_info ? "on" : "off",
        go.osd_show_version       ? "on" : "off");
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] graphics.osd.input: set=%s pat=%s inp=%s vc=%s "
        "ir=%s tr=%s",
        go.osd_show_settings              ? "on" : "off",
        go.osd_show_patches               ? "on" : "off",
        go.osd_show_inputs                ? "on" : "off",
        go.osd_show_video_capture         ? "on" : "off",
        go.osd_show_input_rec             ? "on" : "off",
        go.osd_show_texture_replacements  ? "on" : "off");
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] graphics.osd.warn:  unsafe=%s",
        go.warn_about_unsafe_settings ? "on" : "off");
```

- [ ] **Step 6: Append Cases 17 and 17b to test_core_options.cpp**

Insert into `pcsx2-libretro/tools/test_core_options.cpp` immediately after the Case 16b block (which ends at line 595, the last `check_int(...)` for `shade_boost_gamma`). The line immediately after (line 597) is `std::printf("\n%d failure(s)\n", failures);`. Insert after line 595 and before line 597:

```cpp

    // -------- Case 17: Graphics/OSD round-trip --------
    //
    // 10 representative non-default flips covering each value flavor in
    // the OSD sub-tab. Tests Parse only — dependsOn resolution is
    // host-side and not exercised here.
    fake::reset();
    fake::variables["pcsx2_osd_scale"]                  = "200";
    fake::variables["pcsx2_osd_margin"]                 = "30";
    fake::variables["pcsx2_osd_messages_pos"]           = "5";   // Center
    fake::variables["pcsx2_osd_performance_pos"]        = "9";   // BottomRight
    fake::variables["pcsx2_osd_bold_text"]              = "enabled";
    fake::variables["pcsx2_osd_show_fps"]               = "enabled";
    fake::variables["pcsx2_osd_show_indicators"]        = "disabled"; // flips true→false
    fake::variables["pcsx2_osd_show_settings"]          = "enabled";
    fake::variables["pcsx2_osd_show_patches"]           = "enabled";
    fake::variables["pcsx2_warn_about_unsafe_settings"] = "disabled"; // flips true→false

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 17 osd_scale=200",
                r.graphics.osd.osd_scale, 200);
    check_int ("Case 17 osd_margin=30",
                r.graphics.osd.osd_margin, 30);
    check_int ("Case 17 osd_messages_pos=5 (Center)",
                r.graphics.osd.osd_messages_pos, 5);
    check_int ("Case 17 osd_performance_pos=9 (BottomRight)",
                r.graphics.osd.osd_performance_pos, 9);
    check_bool("Case 17 osd_bold_text=on",
                r.graphics.osd.osd_bold_text, true);
    check_bool("Case 17 osd_show_fps=on",
                r.graphics.osd.osd_show_fps, true);
    check_bool("Case 17 osd_show_indicators=off (flip true→false)",
                r.graphics.osd.osd_show_indicators, false);
    check_bool("Case 17 osd_show_settings=on",
                r.graphics.osd.osd_show_settings, true);
    check_bool("Case 17 osd_show_patches=on",
                r.graphics.osd.osd_show_patches, true);
    check_bool("Case 17 warn_about_unsafe_settings=off (flip true→false)",
                r.graphics.osd.warn_about_unsafe_settings, false);

    // -------- Case 17b: Graphics/OSD default-when-unset --------
    //
    // Asserts all 23 OSD field defaults explicitly. Anchored on the
    // 4 non-false bool defaults (osd_show_indicators=true,
    // osd_show_video_capture=true, osd_show_input_rec=true,
    // warn_about_unsafe_settings=true) plus osd_bold_text=false
    // (catches anyone who follows the standalone adapter's incorrect
    // default="true" — actual PCSX2 runtime default is false because
    // the bitfield zero-inits and Pcsx2Config.cpp:720-745 has no
    // OsdBoldText assignment).
    fake::reset();

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 17b osd_scale default=100",
                r.graphics.osd.osd_scale, 100);
    check_int ("Case 17b osd_margin default=10",
                r.graphics.osd.osd_margin, 10);
    check_int ("Case 17b osd_messages_pos default=1 (TopLeft)",
                r.graphics.osd.osd_messages_pos, 1);
    check_int ("Case 17b osd_performance_pos default=3 (TopRight)",
                r.graphics.osd.osd_performance_pos, 3);
    check_bool("Case 17b osd_bold_text default=off (bitfield zero-init, adapter says true)",
                r.graphics.osd.osd_bold_text, false);
    check_bool("Case 17b osd_show_speed default=off",
                r.graphics.osd.osd_show_speed, false);
    check_bool("Case 17b osd_show_fps default=off",
                r.graphics.osd.osd_show_fps, false);
    check_bool("Case 17b osd_show_vps default=off",
                r.graphics.osd.osd_show_vps, false);
    check_bool("Case 17b osd_show_resolution default=off",
                r.graphics.osd.osd_show_resolution, false);
    check_bool("Case 17b osd_show_gs_stats default=off",
                r.graphics.osd.osd_show_gs_stats, false);
    check_bool("Case 17b osd_show_cpu default=off",
                r.graphics.osd.osd_show_cpu, false);
    check_bool("Case 17b osd_show_gpu default=off",
                r.graphics.osd.osd_show_gpu, false);
    check_bool("Case 17b osd_show_indicators default=on (non-false anchor)",
                r.graphics.osd.osd_show_indicators, true);
    check_bool("Case 17b osd_show_frame_times default=off",
                r.graphics.osd.osd_show_frame_times, false);
    check_bool("Case 17b osd_show_hardware_info default=off",
                r.graphics.osd.osd_show_hardware_info, false);
    check_bool("Case 17b osd_show_version default=off",
                r.graphics.osd.osd_show_version, false);
    check_bool("Case 17b osd_show_settings default=off",
                r.graphics.osd.osd_show_settings, false);
    check_bool("Case 17b osd_show_patches default=off",
                r.graphics.osd.osd_show_patches, false);
    check_bool("Case 17b osd_show_inputs default=off",
                r.graphics.osd.osd_show_inputs, false);
    check_bool("Case 17b osd_show_video_capture default=on (non-false anchor, libretro-inert)",
                r.graphics.osd.osd_show_video_capture, true);
    check_bool("Case 17b osd_show_input_rec default=on (non-false anchor, libretro-inert)",
                r.graphics.osd.osd_show_input_rec, true);
    check_bool("Case 17b osd_show_texture_replacements default=off",
                r.graphics.osd.osd_show_texture_replacements, false);
    check_bool("Case 17b warn_about_unsafe_settings default=on (non-false anchor, [EmuCore] section)",
                r.graphics.osd.warn_about_unsafe_settings, true);
```

- [ ] **Step 7: Rebuild test_core_options and run it**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && make -f Makefile.test_core_options clean && make -f Makefile.test_core_options && ./test_core_options 2>&1 | tail -10
```

(If `Makefile.test_core_options` does not exist, use whatever invocation built the existing `test_core_options` binary — check `ls` for `Makefile*` or `build*.sh` in `tools/`. The binary at `tools/test_core_options` already exists, so the build pattern is established.)

Expected: last line reads `0 failure(s)`. The last printed test should be `[PASS] Case 17b warn_about_unsafe_settings default=on (non-false anchor, [EmuCore] section): got=true want=true`. Total cases: 42 (40 prior + 17 + 17b).

If the test fails, the most likely cause is a typo in a field name (the `Values::Osd` struct uses snake_case; mismatches will not compile). Fix and re-run before proceeding.

- [ ] **Step 8: Run schema fidelity (expect RED)**

Run:
```bash
/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py \
    --core "/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions*.cpp" \
    --host /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp 2>&1 | tail -5
```

Expected output: `Schema fidelity FAILED: 89 core keys, 66 host keys` (or similar — RED at this commit by design; the host bundle restores GREEN). The 23 extra core keys without host counterparts are exactly the OSD knobs.

If the count is not 89 / 66, something is off — verify that the new 23 push_backs each have a `"pcsx2_..."` key string and that no key is duplicated.

- [ ] **Step 9: Build the libretro core**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && ./scripts/build-universal.sh 2>&1 | tail -20
```

Expected: completes without error in ~3-5 min (incremental build). Look for `[INFO] Universal build complete` or equivalent terminal message. Compilation errors should be addressed inline — most likely culprits are typos in INI key strings or missing semicolons in the new code blocks.

(If `build-universal.sh` is run from the RetroNest-Project root and accepts the libretro target by default, this will rebuild the PCSX2 libretro core dylib that the universal app loads. If it doesn't, check `./scripts/` for the libretro-specific build script.)

- [ ] **Step 10: Commit Bundle A**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro && \
git add pcsx2-libretro/CoreOptionsGraphics.h pcsx2-libretro/CoreOptionsGraphics.cpp pcsx2-libretro/CoreOptions.cpp pcsx2-libretro/tools/test_core_options.cpp && \
git commit -m "$(cat <<'EOF'
SP7c Phase 4 Task 6 (core): On-Screen Display sub-tab knobs (23)

[breakdown: 23 knobs across 5 groups (On-Screen Display 5 +
 Performance Stats 9 + System Information 2 + Settings & Inputs 6 +
 Messages 1). 22 under [EmuCore/GS], WarnAboutUnsafeSettings under
 [EmuCore] (split-section like Task 2 patches rows). Defaults
 verified against pcsx2/Config.h:730-733,768-786 + Pcsx2Config.cpp:
 720-745,1943 — 4 non-false bool defaults (OsdShowIndicators,
 OsdShowVideoCapture, OsdShowInputRec, WarnAboutUnsafeSettings).
 OsdBoldText defaults to false (bitfield zero-init), correcting
 standalone adapter's incorrect default="true". OsdshowPatches
 lowercase-s INI typo preserved verbatim.]

Schema fidelity intentionally RED at this commit -- 23 core keys
declared with no host row yet. Matching host commit restores green
at 89/89.

test_core_options 42/42 0 failures.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit lands cleanly; `git log -1 --oneline` shows the new SHA.

---

## Task 2: Bundle B — Host (RetroNest-Project)

**Files:**
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` (1 insertion site, ~line 958)

Working directory: `/Users/mark/Documents/Projects/RetroNest-Project/`

- [ ] **Step 1: Locate insertion point**

The Post-Processing block ends at the last `s.append(gopt(...))` call for `pcsx2_shade_boost_gamma`. Run:
```bash
grep -n 'pcsx2_shade_boost_gamma' /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
```
The `s.append(gopt(...))` for `pcsx2_shade_boost_gamma` starts at line ~947. The `));` closing it lands around line ~957. Insert the 23-row block immediately after that closing `));` and before the next section comment / next `s.append(...)` call.

- [ ] **Step 2: Insert 23 gopt rows**

Insert the following block at the location identified in Step 1. The block follows the Post-Processing block's `gopt(...)` argument shape (category, subcategory, key, label, default, choices, description, optional dependsOn).

```cpp

    // ── Graphics > On-Screen Display (Phase 4 Task 6) ───────────────────
    //
    // 23 knobs mirroring standalone PCSX2's Graphics/On-Screen Display
    // sub-tab. 22 stored under [EmuCore/GS]; WarnAboutUnsafeSettings
    // under [EmuCore] (split-section, handled core-side in
    // CoreOptionsGraphics::ApplyDefaults).
    //
    // Five groups: "On-Screen Display" (5 rows, no dependsOn);
    // "Performance Stats" (9 rows, all dependsOn pcsx2_osd_performance_pos!=0);
    // "System Information" (2 rows, both dependsOn pcsx2_osd_performance_pos!=0);
    // "Settings & Inputs" (6 rows, mixed dependsOn — see per-row notes);
    // "Messages" (1 row, dependsOn pcsx2_osd_messages_pos!=0).
    //
    // OsdScale + OsdMargin are int sliders standalone-side; libretro v2
    // is Combo-only, so they use enumerated stops (Task 2 Crop + Task 5
    // CAS Sharpness precedent).
    //
    // dependsOn audit: every key referenced (pcsx2_osd_performance_pos,
    // pcsx2_osd_messages_pos, pcsx2_osd_show_settings,
    // pcsx2_load_texture_replacements) lives within the Graphics card.
    // Cross-category limitation (refreshDependencies findChildren scope)
    // does NOT apply — see memory cross_category_dependson_limitation.

    // -- "On-Screen Display" group (5 rows, no dependsOn) --

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_scale", "OSD Scale", "100",
        {{"50%", "50"},
         {"75%", "75"},
         {"100% (Default)", "100"},
         {"125%", "125"},
         {"150%", "150"},
         {"200%", "200"},
         {"300%", "300"},
         {"500%", "500"}},
        "Global multiplier applied to every OSD overlay. 100% matches "
        "standalone PCSX2's default size. Standalone exposes a "
        "25-500% slider; libretro offers enumerated stops."));

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_margin", "OSD Margin", "10",
        {{"0px", "0"},
         {"5px", "5"},
         {"10px (Default)", "10"},
         {"15px", "15"},
         {"20px", "20"},
         {"30px", "30"},
         {"50px", "50"},
         {"100px", "100"}},
        "Pixel offset between the OSD elements and the screen edge. "
        "Standalone exposes a 0-100px slider; libretro offers "
        "enumerated stops."));

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_messages_pos", "OSD Messages Position", "1",
        {{"None", "0"},
         {"Top Left (Default)", "1"},
         {"Top Center", "2"},
         {"Top Right", "3"},
         {"Center Left", "4"},
         {"Center", "5"},
         {"Center Right", "6"},
         {"Bottom Left", "7"},
         {"Bottom Center", "8"},
         {"Bottom Right", "9"}},
        "Corner where transient messages (save-state loaded, shader "
        "reload, etc.) are drawn. Set to None to hide them entirely."));

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_performance_pos", "OSD Performance Position", "3",
        {{"None", "0"},
         {"Top Left", "1"},
         {"Top Center", "2"},
         {"Top Right (Default)", "3"},
         {"Center Left", "4"},
         {"Center", "5"},
         {"Center Right", "6"},
         {"Bottom Left", "7"},
         {"Bottom Center", "8"},
         {"Bottom Right", "9"}},
        "Corner where the performance stats column (FPS/Speed/CPU/GPU/"
        "etc.) is drawn. Set to None to hide the column and grey out "
        "every Performance Stats / System Information toggle."));

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_bold_text", "OSD Text Style (Bold)", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Renders OSD text in bold. Easier to read on bright scenes."));

    // -- "Performance Stats" group (9 rows, all dependsOn pcsx2_osd_performance_pos!=0) --

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_speed", "Show Speed Percentages", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the emulation speed as a percentage. Red below 95%, "
        "green above 105%.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_fps", "Show FPS", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the current frame rate reported by the GS. Useful "
        "for spotting performance issues.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_vps", "Show VPS", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays vertical syncs per second — the PS2 display refresh "
        "reported by the GS.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_resolution", "Show Resolution", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the PS2 internal render resolution and interlacing "
        "mode.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_gs_stats", "Show GS Statistics", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays per-frame GS statistics: draw-call count, VRAM use, "
        "and a frame-time summary.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_cpu", "Show CPU Usage", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays per-component CPU usage (EE, GS, VU).",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_gpu", "Show GPU Usage", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays GPU usage percentage and frame time in milliseconds.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_indicators", "Show Status Indicators", "enabled",
        {{"Enabled (Default)", "enabled"}, {"Disabled", "disabled"}},
        "Displays icons for pause, fast-forward, slow-motion, and "
        "turbo modes in the top-right corner.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_frame_times", "Show Frame Times", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays a rolling graph of recent frame times to visualise "
        "stutter.",
        "pcsx2_osd_performance_pos!=0"));

    // -- "System Information" group (2 rows, both dependsOn pcsx2_osd_performance_pos!=0) --

    s.append(gopt(
        "On-Screen Display", "System Information",
        "pcsx2_osd_show_hardware_info", "Show Hardware Info", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the CPU and GPU model names as two lines in the "
        "performance column.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "System Information",
        "pcsx2_osd_show_version", "Show PCSX2 Version", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the PCSX2 version string in the performance column.",
        "pcsx2_osd_performance_pos!=0"));

    // -- "Settings & Inputs" group (6 rows, mixed dependsOn) --

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_settings", "Show Settings", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays a compact summary of active emulation settings in "
        "the bottom-right corner.",
        "pcsx2_osd_messages_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_patches", "Show Patches", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Appends active patches (widescreen, no-interlacing, etc.) to "
        "the settings line. Requires Show Settings to be enabled.",
        "pcsx2_osd_show_settings"));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_inputs", "Show Inputs", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the current controller input state at the "
        "bottom-left corner."));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_video_capture", "Show Video Capture Status", "enabled",
        {{"Enabled (Default)", "enabled"}, {"Disabled", "disabled"}},
        "Displays a recording indicator while video capture is "
        "active. (Inert in the libretro variant — no FFmpeg capture "
        "is driven from this build.)"));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_input_rec", "Show Input Recording Status", "enabled",
        {{"Enabled (Default)", "enabled"}, {"Disabled", "disabled"}},
        "Displays an indicator while input recording is active. "
        "(Inert in the libretro variant — input recording UI is not "
        "driven from this build.)"));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_texture_replacements", "Show Texture Replacement Status", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays an indicator when replacement textures are loaded "
        "for the current game. Requires Texture Replacement → Load "
        "Textures to be enabled.",
        "pcsx2_load_texture_replacements"));

    // -- "Messages" group (1 row, dependsOn pcsx2_osd_messages_pos!=0) --

    s.append(gopt(
        "On-Screen Display", "Messages",
        "pcsx2_warn_about_unsafe_settings", "Warn About Unsafe Settings", "enabled",
        {{"Enabled (Default)", "enabled"}, {"Disabled", "disabled"}},
        "Shows a startup warning if any unsafe settings are enabled.",
        "pcsx2_osd_messages_pos!=0"));
```

- [ ] **Step 3: Run schema fidelity (expect GREEN at 89/89)**

Run:
```bash
/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py \
    --core "/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions*.cpp" \
    --host /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp 2>&1 | tail -3
```

Expected: `Schema fidelity OK: 89 core keys, 89 host keys, byte-for-byte match.`

If RED with a count mismatch, the most likely culprits are: missing `pcsx2_` prefix on a key, typo in a key name, or duplicate key. Run the script with `-v` if available, or diff the core-key list against the host-key list manually.

- [ ] **Step 4: Build universal**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && ./scripts/build-universal.sh 2>&1 | tail -10
```

Expected: completes in ~3-5 min. The new `gopt(...)` block should compile cleanly — `gopt` accepts the (cat, subcat, key, label, default, choices, desc, [dependsOn]) signature already proven by the Post-Processing block.

If compilation fails on a `gopt` call, the most common cause is mismatched brace/paren in a `{{"Label", "value"}, ...}` choices array. The compiler error line will identify the row.

- [ ] **Step 5: Commit Bundle B**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && \
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp && \
git commit -m "$(cat <<'EOF'
SP7c Phase 4 Task 6 (host): On-Screen Display sub-tab rows (23)

[breakdown: 23 rows under subcategory="On-Screen Display" across 5
 groups. 11 rows gated on pcsx2_osd_performance_pos!=0 (PerfStats 9
 + SysInfo 2). 1 row gated on pcsx2_osd_messages_pos!=0
 (OsdShowSettings). 1 row chained on pcsx2_osd_show_settings
 (OsdshowPatches — INI typo preserved). 1 row chained on
 pcsx2_load_texture_replacements (OsdShowTextureReplacements,
 master-bool from Task 4, also within-Graphics). 1 row
 (warn_about_unsafe_settings) gated on pcsx2_osd_messages_pos!=0.
 All dependsOn within-Graphics — cross-category limitation does
 not apply.]

schema-fidelity OK: 89 core keys, 89 host keys, byte-for-byte match.

PHASE 4 COMPLETE.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit lands cleanly; `git log -1 --oneline` shows the new SHA.

---

## Task 3: Live-smoke gate (user-driven, 12 steps)

This is a hand-off, not an automated task. After Bundles A + B commit, the user runs the universal app via Rosetta and walks the 12-step gate documented in the spec (`§Testing`) and in `[[phase4-task6-prep]]`. The plan executor's job ends at Bundle B commit; the executor should:

- [ ] **Step 1: Confirm both commits landed**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro && git log -1 --oneline && \
cd /Users/mark/Documents/Projects/RetroNest-Project && git log -1 --oneline
```

Expected: both repos show the new commit SHAs from Tasks 1+2.

- [ ] **Step 2: Print the smoke-gate launch recipe for the user**

Output to the user:

```
Bundle A + B committed. Run the 12-step smoke gate from the design spec.

Launch recipe:
    DYLD_FRAMEWORK_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/Frameworks \
    QT_PLUGIN_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/PlugIns \
    arch -x86_64 \
    /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest 2>&1 \
      | grep --line-buffered -E "\[CoreOptions\]|graphics\.osd|FATAL|ERROR"

12-step gate is in:
  docs/superpowers/specs/2026-05-14-pcsx2-libretro-sp7c-phase4-task6-osd-design.md §Testing
```

- [ ] **Step 3: After user reports gate pass, update memories and close Phase 4**

The user will report `gate passed` (or specific failures). On pass:
1. Update `[[sp7c-kickoff]]` memory: add Task 6 SHIPPED+LIVE-SMOKE section, mark Phase 4 complete, point next focus at Phase 5.
2. Update `[[phase4-task6-prep]]` memory: mark superseded.
3. Optionally file SP7c followup tickets — (a) standalone adapter `OsdBoldText default="true"` → `"false"`, (b) `EmuFolders::Textures` save-dir rooting from Task 4, (c) dialog-level `masterStates`/`masterValues` promotion from `[[cross-category-dependson-limitation]]`.

---

## Self-Review

**Spec coverage:**
- ✅ 23 OSD knobs across 5 groups — Tasks 1 Step 2 (AppendDefinitions), Task 2 Step 2 (gopt rows).
- ✅ `Values::Osd` struct with 23 fields and source-verified defaults — Task 1 Step 1.
- ✅ Split section [EmuCore/GS] (22) + [EmuCore] (1) for `WarnAboutUnsafeSettings` — Task 1 Step 4 (last line).
- ✅ `OsdshowPatches` lowercase-s INI typo preserved — Task 1 Step 4.
- ✅ Multi-line per-launch echo — Task 1 Step 5.
- ✅ Cases 17 + 17b — Task 1 Step 6.
- ✅ dependsOn fan-out (11 perf+sys, 1 messages-master, 1 settings-chain, 1 texture-repl-chain, 1 messages-master for warn) — Task 2 Step 2.
- ✅ All dependsOn keys within Graphics — verified in Task 2 Step 2 comment block.
- ✅ Schema fidelity verification at both RED (after A) and GREEN (after B) — Task 1 Step 8, Task 2 Step 3.
- ✅ Build verification — Task 1 Step 9, Task 2 Step 4.
- ✅ Commit message templates verbatim from spec — Task 1 Step 10, Task 2 Step 5.
- ✅ Live-smoke gate handoff — Task 3.

**Placeholder scan:** No TBD/TODO; every code block is complete; every command shows expected output.

**Type consistency:**
- Struct field names use snake_case (`osd_scale`, `osd_show_fps`, ...). Used consistently in:
  - Struct definition (Task 1 Step 1)
  - Parse branches (Task 1 Step 3): `out.osd.osd_scale`, etc.
  - ApplyDefaults (Task 1 Step 4): `v.osd.osd_scale`, etc.
  - Echo (Task 1 Step 5): `go.osd_scale`, etc.
  - Test cases (Task 1 Step 6): `r.graphics.osd.osd_scale`, etc.
- Libretro core option keys use the `pcsx2_osd_*` / `pcsx2_warn_*` prefix consistently in:
  - AppendDefinitions (Task 1 Step 2)
  - Parse (Task 1 Step 3)
  - Test cases (Task 1 Step 6)
  - Host gopt rows (Task 2 Step 2)
  - dependsOn strings (Task 2 Step 2)
- INI key strings use CamelCase verbatim from PCSX2 source — `OsdScale`, `OsdMargin`, `OsdMessagesPos`, `OsdPerformancePos`, `OsdBoldText`, `OsdShowSpeed`, ..., `OsdshowPatches` (typo preserved), `WarnAboutUnsafeSettings`. Used only in `ApplyDefaults` (Task 1 Step 4).
- 5-stop and 8-stop combo defaults match: `pcsx2_osd_scale` default `"100"` is in the 8-stop list; `pcsx2_osd_margin` default `"10"` is in the 8-stop list.

No type or naming inconsistencies surfaced.

---

## Plan complete

Save this plan, then offer execution choice between subagent-driven-development (recommended) and executing-plans.
