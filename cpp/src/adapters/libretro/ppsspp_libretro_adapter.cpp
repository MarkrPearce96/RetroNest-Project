#include "ppsspp_libretro_adapter.h"
#include "core/iso9660_reader.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"
#include "core/sfo_parser.h"
#include <QDebug>
#include <QDir>

QVector<ControllerTypeDef> PpssppLibretroAdapter::controllerTypes() const {
    return {
        {"Standard", "PSP Controller",
         ":/AppUI/qml/AppUI/images/controllers/PSP.svg"},
    };
}

QVector<PathDef> PpssppLibretroAdapter::pathsDefs() const {
    return {
        { "Saves",       "libretro", "Saves",      "saves",      PathBase::EmulatorData },
        { "Save States", "libretro", "SaveStates", "savestates", PathBase::EmulatorData },
    };
}

QVector<BindingDef> PpssppLibretroAdapter::controllerBindingDefsForType(const QString&) const {
    // PSP-1000 horizontal layout. Spotlight coords target PSP.svg's
    // viewBox (2367 x 1014). PlayStation face-button conventions:
    //   Cross  (south) -> RetroPad B
    //   Circle (east)  -> RetroPad A
    //   Square (west)  -> RetroPad Y
    //   Triangle (north) -> RetroPad X
    // The analog nub feeds RetroPad axes at the runtime layer and is
    // not part of this digital-binding surface.
    return {
        // D-Pad — cross at left-middle. Up/Down share x=285, Left/Right share y=475.
        { BindingDef::Button, "D-Pad Up",    "D-Pad",   "Pad1", "Up",    "SDL-0/DPadUp",        "DPad",         272, 335, 50 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad",   "Pad1", "Down",  "SDL-0/DPadDown",      "DPad",         272, 595, 50 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad",   "Pad1", "Left",  "SDL-0/DPadLeft",      "DPad",         145, 465, 50 },
        { BindingDef::Button, "D-Pad Right", "D-Pad",   "Pad1", "Right", "SDL-0/DPadRight",     "DPad",         405, 465, 50 },
        // Face buttons — cluster center ~(2050, 620). Shifted down +60 vs.
        // first cut because the diamond sits below the LCD vertical centre.
        { BindingDef::Button, "Cross",       "Buttons", "Pad1", "B",     "SDL-0/FaceSouth",     "FaceButtons", 2110, 600, 45 },
        { BindingDef::Button, "Circle",      "Buttons", "Pad1", "A",     "SDL-0/FaceEast",      "FaceButtons", 2247, 462, 45 },
        { BindingDef::Button, "Square",      "Buttons", "Pad1", "Y",     "SDL-0/FaceWest",      "FaceButtons", 1973, 462, 45 },
        { BindingDef::Button, "Triangle",    "Buttons", "Pad1", "X",     "SDL-0/FaceNorth",     "FaceButtons", 2110, 320, 45 },
        // Shoulders — the white shapes at the top-left and top-right
        // corners of the body ARE the L and R buttons on a PSP-1000.
        { BindingDef::Button, "L",           "Shoulders", "Pad1", "L",   "SDL-0/LeftShoulder",  "Shoulders",    250, 75, 50 },
        { BindingDef::Button, "R",           "Shoulders", "Pad1", "R",   "SDL-0/RightShoulder", "Shoulders",   2120, 75, 50 },
        // System — Start sits at far right of the bottom icon row, Select
        // is one slot to the left. First-cut x was on the Display icon.
        { BindingDef::Button, "Start",       "System",    "Pad1", "Start",  "SDL-0/Start",      "System",      1850, 950, 30 },
        { BindingDef::Button, "Select",      "System",    "Pad1", "Select", "SDL-0/Back",       "System",      1700, 950, 30 },
    };
}

QVector<SettingsHubCard> PpssppLibretroAdapter::settingsHubCards() const {
    // Layout: 3-column grid.
    //   Row 0: Recommended (full-width)
    //   Row 1: System | Video | Input
    //   Row 2: Hacks
    // categoryKey routes clicks to the matching SettingDef::category.
    return {
        { QStringLiteral("\U0001F4A1"), "Recommended",
          "Most-tweaked settings — resolution, performance, compat",
          "Recommended", 0, 0, 1, 3 },
        { QStringLiteral("\U0001F4BE"), "System",
          "CPU core, memory, PSP model, language",
          "System", 1, 0 },
        { QStringLiteral("\U0001F5BC"), "Video",
          "Resolution, MSAA, frameskip, texture scaling",
          "Video", 1, 1 },
        { QStringLiteral("\U0001F3AE"), "Input",
          "Confirm button, analog deadzone & sensitivity",
          "Input", 1, 2 },
        { QStringLiteral("\U000026A1"), "Hacks",
          "Speed hacks — skip buffer effects, disable culling, lazy textures",
          "Hacks", 2, 0 },
    };
}

QVector<QPair<QString, QString>> PpssppLibretroAdapter::frontendSettingDefaults() const {
    // PPSSPP's libretro core exposes no aspect/integer-scale options of its
    // own — these are frontend concerns. Mirrors mgba_libretro_adapter.cpp:75.
    return {
        { "aspect_mode",   "native" },
        { "integer_scale", "OFF"    },
    };
}

PreviewSpec PpssppLibretroAdapter::previewSpec(const QString& category,
                                               const QString& subcategory) const {
    // Recommended hosts a live AspectRatioPreview bound to aspect_mode for
    // visual parity with PCSX2 / mgba. Other (category, subcategory) pairs
    // get no preview (returns empty PreviewSpec).
    if (category == "Recommended" && subcategory.isEmpty())
        return { "aspect", { { "aspect_mode", "aspectMode" } } };
    return {};
}

// Packet 7 Stage 2: the schema is rendered from the core's declared option
// table (declared_options.json sidecar / CoreProber) merged with this
// curation overlay — keys, value sets, value labels, defaults, and wording
// all come from the core. The overlay carries only UI routing: the
// System/Video/Input/Hacks tab each option lives on plus the Recommended
// cross-listings, and the one deliberate RetroNest default
// (ppsspp_internal_resolution 2x vs the core's native 1x). The core's own
// five option categories are deliberately ignored — RetroNest's tab
// grouping wins (spec decision: overlay owns routing). The 33 declared
// ad-hoc/networking options (ppsspp_enable_wlan, MAC digits, pro ad-hoc
// server address digits, UPnP, ...) stay uncurated: valid in OptionsStore,
// hidden from the UI. Entry order = per-category row order. Generated
// mechanically from the retired hand-written rows (parity-checked by
// test_schema_parity).
QVector<OptionOverlay> PpssppLibretroAdapter::optionOverlays() const {
    QVector<OptionOverlay> list;
    auto add = [&list](const QString& key, QVector<OverlayPlacement> places,
                       const QString& defaultOverride = QString()) {
        OptionOverlay o;
        o.key = key;
        o.placements = std::move(places);
        o.defaultOverride = defaultOverride;
        list.append(o);
    };

    // === System ===
    add("ppsspp_cpu_core",                 {{ "Recommended", "", "" }, { "System", "", "" }});
    add("ppsspp_fast_memory",              {{ "System", "", "" }});
    add("ppsspp_ignore_bad_memory_access", {{ "System", "", "" }});
    add("ppsspp_io_timing_method",         {{ "System", "", "" }});
    add("ppsspp_force_lag_sync",           {{ "System", "", "" }});
    add("ppsspp_locked_cpu_speed",         {{ "System", "", "" }});
    add("ppsspp_memstick_inserted",        {{ "System", "", "" }});
    add("ppsspp_cache_iso",                {{ "System", "", "" }});
    add("ppsspp_cheats",                   {{ "System", "", "" }});
    add("ppsspp_language",                 {{ "System", "", "" }});
    add("ppsspp_psp_model",                {{ "Recommended", "", "" }, { "System", "", "" }});

    // === Video ===
    add("ppsspp_backend",                  {{ "Video", "", "" }});
    add("ppsspp_software_rendering",       {{ "Video", "", "" }});
    // Deliberate RetroNest default: 2x (960x544) vs the core's 1x native.
    add("ppsspp_internal_resolution",      {{ "Recommended", "", "" }, { "Video", "", "" }},
        "960x544");
    // Key misspelling ("mulitsample") is upstream's — must match the core's
    // declared key verbatim; fixing it is a schema-breaking fork change.
    add("ppsspp_mulitsample_level",        {{ "Recommended", "", "" }, { "Video", "", "" }});
    add("ppsspp_cropto16x9",               {{ "Recommended", "", "" }, { "Video", "", "" }});
    add("ppsspp_frameskip",                {{ "Recommended", "", "" }, { "Video", "", "" }});
    add("ppsspp_frameskiptype",            {{ "Video", "", "" }});
    add("ppsspp_auto_frameskip",           {{ "Recommended", "", "" }, { "Video", "", "" }});
    add("ppsspp_frame_duplication",        {{ "Video", "", "" }});
    add("ppsspp_detect_vsync_swap_interval", {{ "Video", "", "" }});
    add("ppsspp_inflight_frames",          {{ "Video", "", "" }});
    add("ppsspp_gpu_hardware_transform",   {{ "Video", "", "" }});
    add("ppsspp_software_skinning",        {{ "Video", "", "" }});
    // "tesselation" misspelling is upstream's too — see above.
    add("ppsspp_hardware_tesselation",     {{ "Video", "", "" }});
    add("ppsspp_texture_scaling_type",     {{ "Video", "", "" }});
    add("ppsspp_texture_scaling_level",    {{ "Recommended", "", "" }, { "Video", "", "" }});
    add("ppsspp_texture_deposterize",      {{ "Video", "", "" }});
    add("ppsspp_texture_shader",           {{ "Video", "", "" }});
    add("ppsspp_texture_anisotropic_filtering", {{ "Recommended", "", "" }, { "Video", "", "" }});
    add("ppsspp_texture_filtering",        {{ "Video", "", "" }});
    add("ppsspp_smart_2d_texture_filtering", {{ "Video", "", "" }});
    add("ppsspp_texture_replacement",      {{ "Video", "", "" }});

    // === Input ===
    add("ppsspp_button_preference",        {{ "Input", "", "" }});
    add("ppsspp_analog_is_circular",       {{ "Input", "", "" }});
    add("ppsspp_analog_deadzone",          {{ "Input", "", "" }});
    add("ppsspp_analog_sensitivity",       {{ "Input", "", "" }});

    // === Hacks ===
    add("ppsspp_skip_buffer_effects",      {{ "Recommended", "", "" }, { "Hacks", "", "" }});
    add("ppsspp_disable_range_culling",    {{ "Hacks", "", "" }});
    add("ppsspp_skip_gpu_readbacks",       {{ "Hacks", "", "" }});
    add("ppsspp_lazy_texture_caching",     {{ "Hacks", "", "" }});
    add("ppsspp_spline_quality",           {{ "Hacks", "", "" }});
    add("ppsspp_lower_resolution_for_effects", {{ "Hacks", "", "" }});

    return list;
}

QVector<SettingDef> PpssppLibretroAdapter::extraSettings() const {
    // FrontendSetting rows — backed by frontend.json, not the core's
    // options.json. PPSSPP libretro exposes no aspect/integer-scale options
    // of its own, so the AspectRatioPreview rows on Recommended are wired
    // through here. Mirrors mgba_libretro_adapter.cpp. extraSettings rows
    // lead the merged schema, keeping these at the top of the Recommended
    // card as before.
    auto frontend = [](const QString& key, const QString& label,
                       const QString& def, const QStringList& vals,
                       const QString& category, const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::FrontendSetting;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.type = SettingDef::Combo;
        for (const auto& v : vals)
            d.options.append({ v, v });
        d.category = category;
        d.tooltip = tooltip;
        return d;
    };

    return {
        frontend("aspect_mode", "Aspect Ratio",
                 "native",
                 { "native", "square", "4_3", "16_9", "stretch" },
                 "Recommended",
                 "How the emulated frame is fitted into the window. "
                 "'Native' preserves the PSP's natural aspect ratio. "
                 "'Square Pixel' shows pixel-perfect 1:1. "
                 "'4:3' / '16:9' force a TV-style aspect. "
                 "'Stretch' fills the window ignoring aspect."),
        frontend("integer_scale", "Integer Scale",
                 "OFF",
                 { "OFF", "ON" },
                 "Recommended",
                 "Snap the displayed frame to the largest integer multiple "
                 "of native resolution that fits. Eliminates pixel shimmer "
                 "at the cost of some unused screen area."),
    };
}

QString PpssppLibretroAdapter::extractSerial(const QString& romPath) const {
    QByteArray sfoData = Iso9660::readFile(romPath, "PSP_GAME/PARAM.SFO");
    if (sfoData.isEmpty()) {
        qWarning() << "[PPSSPP-libretro] Failed to read PSP_GAME/PARAM.SFO from:" << romPath;
        return {};
    }
    return SfoParser::extractDiscId(sfoData);
}

QString PpssppLibretroAdapter::systemDirOverride() const {
    // The core looks for its bundled assets at <system_dir>/PPSSPP/
    // (fonts, flash0, compat.ini, lang, …). Missing assets don't fail the
    // boot — they cause font corruption and silently disable per-game
    // compat hacks. Two on-disk layouts are supported:
    //
    //   Fresh install: the CI release zip ships
    //   cores/ppsspp_libretro_resources/PPSSPP/<assets> next to the dylib
    //   and the installer unzips the whole archive into cores/, so
    //   system_dir must point at ppsspp_libretro_resources.
    //
    //   Legacy: assets hand-copied to {root}/bios/PPSSPP/ before this
    //   override existed. When the resources dir is absent we return
    //   empty so GameSession keeps its Paths::biosDir() fallback and
    //   that layout continues to work.
    //
    // Only system_dir is redirected here — the memstick data tree
    // (SAVEDATA, PPSSPP_STATE, …) comes from GET_SAVE_DIRECTORY and
    // stays at emulators/ppsspp/psp/.
    const QString dir =
        Paths::emulatorsDir("libretro") + "/cores/ppsspp_libretro_resources";
    return QDir(dir).exists() ? dir : QString();
}

QString PpssppLibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    // Mirror GameSession::terminate's write path: prefer the user's
    // SaveStates path override if set, otherwise default to
    // emulators/ppsspp/psp/savestates/. Without the override mirror,
    // a user who relocates their save dir would silently lose resume.
    QString dir = PathOverridesStore::instance().read("ppsspp", "SaveStates");
    if (dir.isEmpty())
        dir = Paths::emulatorDataDir("ppsspp", "psp") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    if (!entries.isEmpty())
        return d.absoluteFilePath(entries.first());
    return {};
}
