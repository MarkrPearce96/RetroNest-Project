#include "duckstation_libretro_adapter.h"

#include "core/binding_def.h"
#include "core/controller_type_def.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"

#include <QDir>

// PS1 Digital Controller — the standard (non-analog) PlayStation pad the
// DuckStation libretro core boots with. No PS1-specific controller SVG ships
// under :/AppUI/qml/AppUI/images/controllers/ yet, so svgResource is empty —
// ControllerBindingsView renders without artwork (spotlight coords are all
// 0/0/0 below, i.e. "no spotlight"). A future SP can add a PS1 pad SVG.
QVector<ControllerTypeDef> DuckStationLibretroAdapter::controllerTypes() const {
    return {
        {"AnalogController", "Analog Controller", ""},
        {"DigitalController", "Digital Controller", ""},
    };
}

// PS1 digital pad binding defs.
//
// Action keys (the .key field) match retroPadSlotFromKey() (input_router.h)
// — Up/Down/Left/Right/B/A/Y/X/L/R/L2/R2/Start/Select — so GameSession's
// controls.ini parser resolves each line to a RetroPadSlot and binds it into
// the InputRouter. Default values follow the SDL-0/... convention
// LibretroAdapter::ensureConfig seeds the file with on first launch. Both
// slots and defaults are identical to Pcsx2LibretroAdapter (same RetroPad
// layout); only the labels carry PS1 button names.
//
// PS1 face button positions vs libretro RetroPad:
//   RetroPad B  (south) = PS1 Cross
//   RetroPad A  (east)  = PS1 Circle
//   RetroPad Y  (west)  = PS1 Square
//   RetroPad X  (north) = PS1 Triangle
//
// Stick axes and L3/R3 are fed at runtime by the core (RETRO_DEVICE_ANALOG / RetroPad);
// they are not listed here as user-rebindable binding rows. Spotlight coords
// are 0/0/0 (no spotlight) because there is no PS1 controller SVG yet.
QVector<BindingDef> DuckStationLibretroAdapter::controllerBindingDefsForType(const QString&) const {
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
        { BindingDef::Button, "L1", "Shoulders", "Pad1", "L",  "SDL-0/LeftShoulder",  "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "R1", "Shoulders", "Pad1", "R",  "SDL-0/RightShoulder", "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "L2", "Shoulders", "Pad1", "L2", "SDL-0/+LeftTrigger",  "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "R2", "Shoulders", "Pad1", "R2", "SDL-0/+RightTrigger", "Shoulders", 0, 0, 0 },
        // System
        { BindingDef::Button, "Start",  "System", "Pad1", "Start",  "SDL-0/Start", "System", 0, 0, 0 },
        { BindingDef::Button, "Select", "System", "Pad1", "Select", "SDL-0/Back",  "System", 0, 0, 0 },
    };
}

QVector<PathDef> DuckStationLibretroAdapter::pathsDefs() const {
    return {
        { "Memory Cards", "libretro", "MemoryCards", "memcards",   PathBase::EmulatorData },
        { "Save States",  "libretro", "SaveStates",  "savestates", PathBase::EmulatorData },
    };
}

// Resume-on-launch: GameSession::terminate writes "<serial>.resume" under the
// DuckStation SaveStates dir; locate it here so GameSession feeds it to
// cfg.resumeStatePath (loaded post-retro_load_game via retro_unserialize).
// Base id "duckstation", systemId "psx". Mirrors Pcsx2LibretroAdapter::findResumeFile.
QString DuckStationLibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    QString dir = PathOverridesStore::instance().read("duckstation", "SaveStates");
    if (dir.isEmpty())
        dir = Paths::emulatorDataDir("duckstation", "psx") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    if (!entries.isEmpty())
        return d.absoluteFilePath(entries.first());
    return {};
}

// Phase 3: libretro-option-backed settings schema.
//
// Packet 7 Stage 2: the schema is rendered from the core's declared option
// table (declared_options.json sidecar / CoreProber) merged with this
// curation overlay — keys, value sets, value labels, defaults, and wording
// all come from the core (which this fork authors anyway, so the two sides
// can no longer drift). The overlay carries only UI routing: category /
// Graphics sub-tab / group box, Recommended cross-listings, and dependency
// gates. Entry order = per-category row order. Generated mechanically from
// the retired hand-written rows (parity-checked at conversion time by
// the since-retired test_schema_parity net).
QVector<OptionOverlay> DuckStationLibretroAdapter::optionOverlays() const {
    QVector<OptionOverlay> list;
    auto add = [&list](const QString& key, QVector<OverlayPlacement> places,
                       const QString& dependsOn = QString()) {
        OptionOverlay o;
        o.key = key;
        o.placements = std::move(places);
        o.dependsOn = dependsOn;
        list.append(o);
    };

    add("duckstation_gpu_renderer",
        {{ "Recommended", "", "Performance" },
         { "Graphics", "Rendering", "Rendering" }});
    add("duckstation_cpu_execution_mode",
        {{ "Recommended", "", "Performance" },
         { "Console", "", "CPU Emulation" }});
    add("duckstation_cdrom_read_speedup",
        {{ "Recommended", "", "Performance" },
         { "Console", "", "CD-ROM Emulation" }});
    add("duckstation_gpu_resolution_scale",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Rendering", "Rendering" }});
    add("duckstation_display_aspect_ratio",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_widescreen_hack",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_pgxp_enable",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_multisamples",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Advanced", "Rendering Options" }});
    add("duckstation_gpu_texture_filter",
        {{ "Recommended", "", "Visual Quality" },
         { "Graphics", "Rendering", "Rendering" }});
    add("duckstation_console_region",
        {{ "Recommended", "", "Convenience" },
         { "Console", "", "Console" }});
    add("duckstation_bios_fast_boot",
        {{ "Recommended", "", "Convenience" },
         { "Console", "", "Console" }});
    add("duckstation_gpu_force_video_timing",
        {{ "Console", "", "Console" }});
    add("duckstation_console_8mb_ram",
        {{ "Console", "", "Console" }});
    add("duckstation_bios_fast_forward_boot",
        {{ "Console", "", "Console" }},
        "duckstation_bios_fast_boot");
    add("duckstation_cpu_overclock_enable",
        {{ "Console", "", "CPU Emulation" }});
    add("duckstation_cpu_overclock_percent",
        {{ "Console", "", "CPU Emulation" }},
        "duckstation_cpu_overclock_enable");
    add("duckstation_cpu_recompiler_icache",
        {{ "Console", "", "CPU Emulation" }},
        "duckstation_cpu_execution_mode=Recompiler");
    add("duckstation_cdrom_seek_speedup",
        {{ "Console", "", "CD-ROM Emulation" }});
    add("duckstation_cdrom_preload",
        {{ "Console", "", "CD-ROM Emulation" }});
    add("duckstation_cdrom_image_patches",
        {{ "Console", "", "CD-ROM Emulation" }});
    add("duckstation_cdrom_auto_disc_change",
        {{ "Console", "", "CD-ROM Emulation" }});
    add("duckstation_cdrom_ignore_subcode",
        {{ "Console", "", "CD-ROM Emulation" }});
    add("duckstation_pad1_type",
        {{ "Console", "", "Controllers" }});
    add("duckstation_pad2_type",
        {{ "Console", "", "Controllers" }});
    add("duckstation_memcard_1_type",
        {{ "Memory Cards", "", "Memory Card 1" }});
    add("duckstation_memcard_2_type",
        {{ "Memory Cards", "", "Memory Card 2" }});
    add("duckstation_gpu_downsample_mode",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_sprite_texture_filter",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_dithering",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_deinterlacing",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_display_crop",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_display_scaling",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_display_scaling_24bit",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_pgxp_culling",
        {{ "Graphics", "Rendering", "Rendering" }},
        "duckstation_gpu_pgxp_enable");
    add("duckstation_gpu_pgxp_texture_correction",
        {{ "Graphics", "Rendering", "Rendering" }},
        "duckstation_gpu_pgxp_enable");
    add("duckstation_display_force_4_3_for_24bit",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_chroma_smoothing_24bit",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_gpu_force_round_texcoords",
        {{ "Graphics", "Rendering", "Rendering" }});
    add("duckstation_display_alignment",
        {{ "Graphics", "Advanced", "Display Options" }});
    add("duckstation_display_rotation",
        {{ "Graphics", "Advanced", "Display Options" }});
    add("duckstation_display_fine_crop",
        {{ "Graphics", "Advanced", "Display Options" }});
    add("duckstation_display_disable_mailbox",
        {{ "Graphics", "Advanced", "Display Options" }});
    add("duckstation_gpu_line_detect",
        {{ "Graphics", "Advanced", "Rendering Options" }});
    add("duckstation_gpu_modulation_crop",
        {{ "Graphics", "Advanced", "Rendering Options" }});
    add("duckstation_gpu_scaled_interlacing",
        {{ "Graphics", "Advanced", "Rendering Options" }});
    add("duckstation_gpu_sw_readbacks",
        {{ "Graphics", "Advanced", "Rendering Options" }});
    add("duckstation_gpu_texture_cache",
        {{ "Graphics", "Texture Replacement", "General Settings" }});
    add("duckstation_texrepl_enable_replacements",
        {{ "Graphics", "Texture Replacement", "Texture Replacement" }},
        "duckstation_gpu_texture_cache");
    add("duckstation_texrepl_dump_textures",
        {{ "Graphics", "Texture Replacement", "Texture Replacement" }},
        "duckstation_gpu_texture_cache");
    add("duckstation_texrepl_always_track_uploads",
        {{ "Graphics", "Texture Replacement", "Texture Replacement" }},
        "duckstation_gpu_texture_cache");
    add("duckstation_texrepl_dump_replaced_textures",
        {{ "Graphics", "Texture Replacement", "Texture Replacement" }},
        "duckstation_texrepl_dump_textures");
    add("duckstation_texrepl_enable_vram_write_replacements",
        {{ "Graphics", "Texture Replacement", "VRAM Write Replacement" }});
    add("duckstation_texrepl_dump_vram_writes",
        {{ "Graphics", "Texture Replacement", "VRAM Write Replacement" }});
    add("duckstation_texrepl_use_old_mdec_routines",
        {{ "Graphics", "Texture Replacement", "VRAM Write Replacement" }});
    add("duckstation_osd_show_fps",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_speed",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_cpu",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_gpu",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_resolution",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_gpu_stats",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_frame_times",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_latency_stats",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_inputs",
        {{ "Graphics", "On-Screen Display", "Overlays" }});
    add("duckstation_osd_show_enhancements",
        {{ "Graphics", "On-Screen Display", "Overlays" }});

    return list;
}

QVector<SettingsHubCard> DuckStationLibretroAdapter::settingsHubCards() const {
    return {
        // Row 0: Recommended — full-width across all 3 columns (mirrors Dolphin/PCSX2).
        {QStringLiteral("\U00002B50"), "Recommended",
         "Renderer, PGXP, resolution, region, fast boot",
         "Recommended", 0, 0, 1, 3},
        // Row 1: Console · Graphics · Memory Cards
        {QStringLiteral("\U0001F3AE"), "Console",
         "Region, CPU, CD-ROM, fast boot",
         "Console", 1, 0},
        {QStringLiteral("\U0001F3A8"), "Graphics",
         "Renderer, PGXP, upscaling, textures, advanced",
         "Graphics", 1, 1},
        {QStringLiteral("\U0001F4BE"), "Memory Cards",
         "Slot 1/2 card types",
         "Memory Cards", 1, 2},
    };
}

PreviewSpec DuckStationLibretroAdapter::previewSpec(const QString& category,
                                                    const QString& subcategory) const {
    if (category == "Recommended" && subcategory.isEmpty()) {
        // Aspect-ratio preview on the Recommended page (mirrors Dolphin/PCSX2).
        // Bound to the same duckstation_display_aspect_ratio key the Recommended
        // card lists. The shared AspectRatioPreview's fromSchemaValue may not
        // differentiate all DuckStation-specific strings ("Auto (Game Native)",
        // "PAR 1:1", etc.) until Phase 5 alignment work; it falls back to 4:3
        // gracefully.
        return {"aspect", {
            {"duckstation_display_aspect_ratio", "aspectMode"},
        }};
    }
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        // Overlay preview, mirrors dolphin_libretro_adapter's "osd" spec.
        return {"osd", {
            {"duckstation_osd_show_fps",         "showFps"},
            {"duckstation_osd_show_speed",       "showSpeed"},
            {"duckstation_osd_show_cpu",         "showCpu"},
            {"duckstation_osd_show_gpu",         "showGpu"},       // was "showVps" — fixed
            {"duckstation_osd_show_resolution",  "showResolution"},
            {"duckstation_osd_show_frame_times", "showFrameTimes"},
            {"duckstation_osd_show_inputs",      "showInputs"},
            // show_gpu_stats  → no showGpuStats in OsdPreview (omitted)
            // show_latency_stats → no showLatencyStats in OsdPreview (omitted)
            // show_enhancements  → no showEnhancements in OsdPreview (omitted)
        }};
    }
    return {};
}
