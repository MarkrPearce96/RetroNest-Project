#include "pcsx2_libretro_adapter.h"

#include "core/binding_def.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"

#include <QDir>

// SP5: PS2 DualShock 2 binding defs.
//
// Action keys (the .key field) match retroPadSlotFromKey() (input_router.h)
// — B/Y/Select/Start/Up/Down/Left/Right/A/X/L/R/L2/R2/L3/R3 — so
// GameSession's controls.ini parser resolves each line to a RetroPadSlot
// and binds it into the InputRouter. Default values follow the SDL-0/...
// convention LibretroAdapter::ensureConfig seeds the file with on first launch.
//
// PS2 face button positions vs libretro RetroPad:
//   RetroPad B  (south) = PS2 Cross
//   RetroPad A  (east)  = PS2 Circle
//   RetroPad Y  (west)  = PS2 Square
//   RetroPad X  (north) = PS2 Triangle
//
// Spotlight coords are 0/0/0 (no spotlight) because there is no PS2
// controller SVG configured yet. Future SP can add one.
QVector<BindingDef> Pcsx2LibretroAdapter::controllerBindingDefsForType(const QString&) const {
    return {
        // D-Pad
        { BindingDef::Button, "D-Pad Up",    "D-Pad", "Pad1", "Up",    "SDL-0/DPadUp",    "DPad", 0, 0, 0 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad", "Pad1", "Down",  "SDL-0/DPadDown",  "DPad", 0, 0, 0 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad", "Pad1", "Left",  "SDL-0/DPadLeft",  "DPad", 0, 0, 0 },
        { BindingDef::Button, "D-Pad Right", "D-Pad", "Pad1", "Right", "SDL-0/DPadRight", "DPad", 0, 0, 0 },
        // Face buttons
        { BindingDef::Button, "Cross",    "Buttons", "Pad1", "B", "SDL-0/FaceSouth", "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "Circle",   "Buttons", "Pad1", "A", "SDL-0/FaceEast",  "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "Square",   "Buttons", "Pad1", "Y", "SDL-0/FaceWest",  "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "Triangle", "Buttons", "Pad1", "X", "SDL-0/FaceNorth", "FaceButtons", 0, 0, 0 },
        // Shoulders + triggers (triggers route as digital here; full analog is future work)
        { BindingDef::Button, "L1", "Shoulders", "Pad1", "L",  "SDL-0/LeftShoulder",   "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "R1", "Shoulders", "Pad1", "R",  "SDL-0/RightShoulder",  "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "L2", "Shoulders", "Pad1", "L2", "SDL-0/+LeftTrigger",   "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "R2", "Shoulders", "Pad1", "R2", "SDL-0/+RightTrigger",  "Shoulders", 0, 0, 0 },
        // Stick clicks
        { BindingDef::Button, "L3 (Left Stick Click)",  "Sticks", "Pad1", "L3", "SDL-0/LeftStick",  "LeftAnalog",  0, 0, 0 },
        { BindingDef::Button, "R3 (Right Stick Click)", "Sticks", "Pad1", "R3", "SDL-0/RightStick", "RightAnalog", 0, 0, 0 },
        // System
        { BindingDef::Button, "Start",  "System", "Pad1", "Start",  "SDL-0/Start", "System", 0, 0, 0 },
        { BindingDef::Button, "Select", "System", "Pad1", "Select", "SDL-0/Back",  "System", 0, 0, 0 },
    };
}

// SP6.5: GameSession::terminate writes "{serial}.resume" under
// emulators/pcsx2/ps2/savestates/. Look there. Base id is "pcsx2"
// (the manifest id used by Paths::emulatorDataDir on the save side
// at game_session.cpp:392); systemId is "ps2".
QString Pcsx2LibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    // Path overrides: search the override dir first if set, else fall
    // back to the default <emulator_data>/savestates. Must mirror the
    // write side in GameSession::terminate / libretroSlotPath, otherwise
    // a user with a SaveStates override would silently lose cold-resume.
    QString dir = PathOverridesStore::instance().read("pcsx2", "SaveStates");
    if (dir.isEmpty())
        dir = Paths::emulatorDataDir("pcsx2", "ps2") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    if (!entries.isEmpty())
        return d.absoluteFilePath(entries.first());
    return {};
}

// Path overrides: three user-overridable folders exposed in the Paths UI.
// "libretro" section is informational — ConfigService routes libretro adapters
// to PathOverridesStore, so only the key identifies the override within the
// "pcsx2" namespace. defaultSuffix values match what the runtime consumers
// expect:
//   memcards   — pcsx2-libretro/Settings.cpp uses save_dir + "/memcards"
//   savestates — per-emulator data dir + "/savestates"
//   textures   — pcsx2-libretro/Settings.cpp uses save_dir + "/textures"
// BIOS is intentionally not overridable per-emulator — see spec.
QVector<PathDef> Pcsx2LibretroAdapter::pathsDefs() const {
    return {
        { "Memory Cards", "libretro", "MemoryCards", "memcards",   PathBase::EmulatorData },
        { "Save States",  "libretro", "SaveStates",  "savestates", PathBase::EmulatorData },
        { "Textures",     "libretro", "Textures",    "textures",   PathBase::EmulatorData },
    };
}

// Packet 7 Stage 2: the schema is rendered from the core's declared option
// table (declared_options.json sidecar / CoreProber) merged with this
// curation overlay — keys, value sets, value labels, defaults, and wording
// all come from the core (which this fork authors via CoreOptions.cpp; the
// pre-conversion parity diff was 89/89 keys with zero value-set drift).
// The overlay carries UI routing (Graphics sub-tab + group box, the flat
// category/group pages, the 15 Recommended cross-listings), all 27
// dependsOn gates verbatim, and the three deliberate RetroNest defaults:
//   pcsx2_upscale_multiplier "2" (core: "1"),
//   pcsx2_aspect_ratio "16:9" (core: "4:3"),
//   pcsx2_enable_widescreen_patches "enabled" (core: "disabled").
// Entry order = per-category row order (Recommended block leads, exactly
// as the hand schema did). Generated mechanically from the retired
// hand-written rows (parity-checked at conversion time by the
// since-retired test_schema_parity net).
//
// pcsx2_filter appears as TWO entries: its Graphics row is gated on
// pcsx2_tri_filter, but the Recommended copy is deliberately ungated —
// tri_filter has no Recommended row, and a gate whose master has no row
// on the same page can never grey reliably (the hand schema made the
// same per-placement distinction).
//
// Media Capture sub-tab is deliberately not curated: RetroNest's host
// application owns the screenshot + video-recording pipeline. Same
// architectural reasoning as the silently-omitted Achievements card
// (RetroNest owns rcheevos host-side).
QVector<OptionOverlay> Pcsx2LibretroAdapter::optionOverlays() const {
    QVector<OptionOverlay> list;
    auto add = [&list](const QString& key, QVector<OverlayPlacement> places,
                       const QString& defaultOverride = QString(),
                       const QString& dependsOn = QString()) {
        OptionOverlay o;
        o.key = key;
        o.placements = std::move(places);
        o.defaultOverride = defaultOverride;
        o.dependsOn = dependsOn;
        list.append(o);
    };

    add("pcsx2_renderer",
        {{ "Recommended", "", "Performance" },
         { "Graphics", "Display", "Display" }});
    add("pcsx2_mtvu",
        {{ "Recommended", "", "Performance" },
         { "Emulation", "", "System Settings" }});
    add("pcsx2_ee_cycle_rate",
        {{ "Recommended", "", "Performance" },
         { "Emulation", "", "System Settings" }});
    add("pcsx2_upscale_multiplier",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Rendering", "Rendering" }},
        /*defaultOverride=*/"2");
    add("pcsx2_aspect_ratio",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Display", "Display" }},
        /*defaultOverride=*/"16:9");
    add("pcsx2_enable_widescreen_patches",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Display", "Display" }},
        /*defaultOverride=*/"enabled");
    add("pcsx2_accurate_blending_unit",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Rendering", "Rendering" }});
    add("pcsx2_max_anisotropy",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Rendering", "Rendering" }});
    add("pcsx2_filter",
        {{ "Recommended", "", "Visual Quality" }});
    add("pcsx2_vsync",
        {{ "Recommended", "", "Frame Pacing" },
         { "Emulation", "", "Frame Pacing / Latency Control" }});
    add("pcsx2_sync_to_host_rr",
        {{ "Recommended", "", "Frame Pacing" },
         { "Emulation", "", "Frame Pacing / Latency Control" }});
    add("pcsx2_audio_volume",
        {{ "Recommended", "", "Audio" },
         { "Audio", "", "Controls" }});
    add("pcsx2_normal_speed",
        {{ "Recommended", "", "Convenience" },
         { "Emulation", "", "Speed Control" }});
    add("pcsx2_fast_boot",
        {{ "Recommended", "", "Convenience" },
         { "Emulation", "", "System Settings" }});
    add("pcsx2_cheats",
        {{ "Recommended", "", "Convenience" },
         { "Emulation", "", "System Settings" }});
    add("pcsx2_fast_forward_speed",
        {{ "Emulation", "", "Speed Control" }});
    add("pcsx2_slow_motion_speed",
        {{ "Emulation", "", "Speed Control" }});
    add("pcsx2_ee_cycle_skip",
        {{ "Emulation", "", "System Settings" }});
    add("pcsx2_thread_pinning",
        {{ "Emulation", "", "System Settings" }});
    add("pcsx2_host_fs",
        {{ "Emulation", "", "System Settings" }});
    add("pcsx2_cdvd_precache",
        {{ "Emulation", "", "System Settings" }});
    add("pcsx2_fast_boot_ff",
        {{ "Emulation", "", "System Settings" }},
        QString(),
        /*dependsOn=*/"pcsx2_fast_boot");
    add("pcsx2_vsync_queue_size",
        {{ "Emulation", "", "Frame Pacing / Latency Control" }});
    add("pcsx2_use_vsync_timing",
        {{ "Emulation", "", "Frame Pacing / Latency Control" }},
        QString(),
        /*dependsOn=*/"pcsx2_vsync && pcsx2_sync_to_host_rr");
    add("pcsx2_skip_duplicate_frames",
        {{ "Emulation", "", "Frame Pacing / Latency Control" }});
    add("pcsx2_audio_sync_mode",
        {{ "Audio", "", "Configuration" }});
    add("pcsx2_audio_buffer_ms",
        {{ "Audio", "", "Configuration" }});
    add("pcsx2_audio_ff_volume",
        {{ "Audio", "", "Controls" }});
    add("pcsx2_audio_muted",
        {{ "Audio", "", "Controls" }});
    add("pcsx2_mc_slot1_enable",
        {{ "Memory Cards", "", "Memory Card Slots" }});
    add("pcsx2_mc_slot2_enable",
        {{ "Memory Cards", "", "Memory Card Slots" }});
    add("pcsx2_mc_multitap1_slot2",
        {{ "Memory Cards", "", "Multitap" }});
    add("pcsx2_mc_multitap1_slot3",
        {{ "Memory Cards", "", "Multitap" }});
    add("pcsx2_mc_multitap1_slot4",
        {{ "Memory Cards", "", "Multitap" }});
    add("pcsx2_fmv_aspect_ratio",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_deinterlace_mode",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_linear_present_mode",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_stretch_y",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_crop_left",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_crop_top",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_crop_right",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_crop_bottom",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_enable_no_interlacing_patches",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_pcrtc_antiblur",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_integer_scaling",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_pcrtc_offsets",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_disable_interlace_offset",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_pcrtc_overscan",
        {{ "Graphics", "Display", "Display" }});
    add("pcsx2_filter",
        {{ "Graphics", "Rendering", "Rendering" }},
        QString(),
        /*dependsOn=*/"pcsx2_tri_filter!=2 && pcsx2_tri_filter!=3");
    add("pcsx2_tri_filter",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("pcsx2_dithering_ps2",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("pcsx2_hw_mipmap",
        {{ "Graphics", "Rendering", "Hardware Rendering Options" }});
    add("pcsx2_load_texture_replacements",
        {{ "Graphics", "Texture Replacement", "Options" }});
    add("pcsx2_dump_replaceable_textures",
        {{ "Graphics", "Texture Replacement", "Options" }});
    add("pcsx2_load_texture_replacements_async",
        {{ "Graphics", "Texture Replacement", "Options" }},
        QString(),
        /*dependsOn=*/"pcsx2_load_texture_replacements");
    add("pcsx2_precache_texture_replacements",
        {{ "Graphics", "Texture Replacement", "Options" }},
        QString(),
        /*dependsOn=*/"pcsx2_load_texture_replacements");
    add("pcsx2_dump_replaceable_mipmaps",
        {{ "Graphics", "Texture Replacement", "Options" }},
        QString(),
        /*dependsOn=*/"pcsx2_dump_replaceable_textures");
    add("pcsx2_dump_textures_with_fmv_active",
        {{ "Graphics", "Texture Replacement", "Options" }},
        QString(),
        /*dependsOn=*/"pcsx2_dump_replaceable_textures");
    add("pcsx2_cas_mode",
        {{ "Graphics", "Post-Processing", "Sharpening/Anti-Aliasing" }});
    add("pcsx2_cas_sharpness",
        {{ "Graphics", "Post-Processing", "Sharpening/Anti-Aliasing" }},
        QString(),
        /*dependsOn=*/"pcsx2_cas_mode!=0");
    add("pcsx2_fxaa",
        {{ "Graphics", "Post-Processing", "Sharpening/Anti-Aliasing" }});
    add("pcsx2_tv_shader",
        {{ "Graphics", "Post-Processing", "Filters" }});
    add("pcsx2_shade_boost",
        {{ "Graphics", "Post-Processing", "Filters" }});
    add("pcsx2_shade_boost_brightness",
        {{ "Graphics", "Post-Processing", "Filters" }},
        QString(),
        /*dependsOn=*/"pcsx2_shade_boost");
    add("pcsx2_shade_boost_contrast",
        {{ "Graphics", "Post-Processing", "Filters" }},
        QString(),
        /*dependsOn=*/"pcsx2_shade_boost");
    add("pcsx2_shade_boost_saturation",
        {{ "Graphics", "Post-Processing", "Filters" }},
        QString(),
        /*dependsOn=*/"pcsx2_shade_boost");
    add("pcsx2_shade_boost_gamma",
        {{ "Graphics", "Post-Processing", "Filters" }},
        QString(),
        /*dependsOn=*/"pcsx2_shade_boost");
    add("pcsx2_osd_scale",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("pcsx2_osd_margin",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("pcsx2_osd_messages_pos",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("pcsx2_osd_performance_pos",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("pcsx2_osd_bold_text",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("pcsx2_osd_show_speed",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_fps",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_vps",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_resolution",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_gs_stats",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_cpu",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_gpu",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_indicators",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_frame_times",
        {{ "Graphics", "On-Screen Display", "Performance Stats" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_settings",
        {{ "Graphics", "On-Screen Display", "Settings & Inputs" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_messages_pos!=0");
    add("pcsx2_osd_show_patches",
        {{ "Graphics", "On-Screen Display", "Settings & Inputs" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_show_settings");
    add("pcsx2_osd_show_inputs",
        {{ "Graphics", "On-Screen Display", "Settings & Inputs" }});
    add("pcsx2_osd_show_video_capture",
        {{ "Graphics", "On-Screen Display", "Settings & Inputs" }});
    add("pcsx2_osd_show_input_rec",
        {{ "Graphics", "On-Screen Display", "Settings & Inputs" }});
    add("pcsx2_osd_show_texture_replacements",
        {{ "Graphics", "On-Screen Display", "Settings & Inputs" }},
        QString(),
        /*dependsOn=*/"pcsx2_load_texture_replacements");
    add("pcsx2_osd_show_hardware_info",
        {{ "Graphics", "On-Screen Display", "System Information" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_osd_show_version",
        {{ "Graphics", "On-Screen Display", "System Information" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_performance_pos!=0");
    add("pcsx2_warn_about_unsafe_settings",
        {{ "Graphics", "On-Screen Display", "Messages" }},
        QString(),
        /*dependsOn=*/"pcsx2_osd_messages_pos!=0");

    return list;
}

// Preview wiring. The preview widgets
// (cpp/src/ui/settings/widgets/preview/{aspect_ratio_preview,osd_preview}.{h,cpp})
// are adapter-agnostic; they expose Qt properties named exactly as the
// values in keyToProperty below, and
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

QVector<SettingsHubCard> Pcsx2LibretroAdapter::settingsHubCards() const {
    return {
        // Row 0: Recommended — full-width across 3 columns.
        {QStringLiteral("\U0001F4A1"), "Recommended",
         "GS renderer, multi-threaded VU1, fast boot",
         "Recommended", 0, 0, 1, 3},
        // Row 1: Emulation · Graphics · Audio
        // 🎨 palette glyph chosen because the Graphics card covers all five
        // sub-tabs (Display / Rendering / Texture Replacement / Post-Processing
        // / On-Screen Display), not just Display — clearer than 🖥️ at card level.
        {QStringLiteral("\U0001F3AE"), "Emulation",
         "Speed control, system, frame pacing",
         "Emulation", 1, 0},
        {QStringLiteral("\U0001F3A8"), "Graphics",
         "Aspect ratio, upscaling, post-FX, OSD, textures",
         "Graphics", 1, 1},
        {QStringLiteral("\U0001F50A"), "Audio",
         "Volume, mute, buffer, sync mode",
         "Audio", 1, 2},
        // Row 2: Memory Cards
        {QStringLiteral("\U0001F4BE"), "Memory Cards",
         "Slot 1/2 enables, Multitap slots",
         "Memory Cards", 2, 0},
    };
}
