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

QVector<SettingDef> PpssppLibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    // Helper for upstream libretro options where the display label equals
    // the INI value. Most options are like this.
    auto opt = [](const QString& key, const QString& label,
                  const QString& def, const QStringList& vals,
                  const QString& category, const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
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

    // Helper for options whose display label differs from the stored value
    // (e.g. ppsspp_internal_resolution shows "1x (480x272)" but stores "480x272").
    auto optLabeled = [](const QString& key, const QString& label,
                         const QString& def,
                         const QVector<QPair<QString, QString>>& vals,
                         const QString& category, const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.type = SettingDef::Combo;
        d.options = vals;
        d.category = category;
        d.tooltip = tooltip;
        return d;
    };

    // Helper for FrontendSetting entries — backed by frontend.json, not the
    // core's options.json. PPSSPP libretro exposes no aspect/integer-scale
    // options of its own, so the AspectRatioPreview rows on Recommended are
    // wired through here. Mirrors mgba_libretro_adapter.cpp:105.
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

    // === System (11) ===

    s << opt("ppsspp_cpu_core", "CPU Core",
             "JIT",
             { "JIT", "IR JIT", "Interpreter" },
             "System",
             "Specifies CPU emulator. JIT is fastest. IR JIT is more stable. Interpreter is the most accurate but slowest.");

    s << opt("ppsspp_fast_memory", "Fast Memory",
             "enabled",
             { "enabled", "disabled" },
             "System",
             "Faster memory access. Some games might require disabling.");

    s << opt("ppsspp_ignore_bad_memory_access", "Ignore Bad Memory Accesses",
             "enabled",
             { "enabled", "disabled" },
             "System",
             "Continue running after detecting an invalid memory access. May help some buggy games.");

    s << opt("ppsspp_io_timing_method", "I/O Timing Method",
             "Fast",
             { "Fast", "Host", "Simulate UMD delays" },
             "System",
             "Affects how PSP storage I/O latency is emulated. 'Fast' is fastest. 'Simulate UMD delays' is most accurate.");

    s << opt("ppsspp_force_lag_sync", "Force Real Clock Sync",
             "disabled",
             { "enabled", "disabled" },
             "System",
             "Slows down emulation to keep timing closer to a real PSP. May reduce stutter in some games at a performance cost.");

    s << opt("ppsspp_locked_cpu_speed", "Locked CPU Speed",
             "disabled",
             { "disabled",
               "222MHz", "232MHz", "244MHz", "266MHz", "288MHz", "300MHz",
               "333MHz", "366MHz", "388MHz", "400MHz", "433MHz", "466MHz",
               "488MHz", "500MHz", "533MHz", "555MHz", "576MHz", "600MHz",
               "633MHz", "666MHz", "688MHz", "700MHz", "733MHz", "750MHz",
               "766MHz", "788MHz", "800MHz", "833MHz", "866MHz", "888MHz",
               "900MHz", "933MHz", "966MHz", "999MHz" },
             "System",
             "Locks the emulated PSP CPU frequency. 'disabled' = dynamic per-game default (usually 222 or 333 MHz).");

    s << opt("ppsspp_memstick_inserted", "Memory Stick Inserted",
             "enabled",
             { "enabled", "disabled" },
             "System",
             "Simulates a Memory Stick being inserted into the PSP. Disable to test no-MS code paths in homebrew.");

    s << opt("ppsspp_cache_iso", "Cache Full ISO in RAM",
             "disabled",
             { "enabled", "disabled" },
             "System",
             "Reads the whole ISO into RAM at boot. Eliminates disc-read stutter, but uses memory proportional to ISO size.");

    s << opt("ppsspp_cheats", "Internal Cheats Support",
             "disabled",
             { "enabled", "disabled" },
             "System",
             "Enable PPSSPP's built-in cheat engine (looks for a per-game .ini in the cheats folder).");

    s << opt("ppsspp_language", "Game Language",
             "Automatic",
             { "Automatic", "English", "Japanese", "French", "Spanish",
               "German", "Italian", "Dutch", "Portuguese", "Russian",
               "Korean", "Chinese Traditional", "Chinese Simplified" },
             "System",
             "Forces the PSP system language. 'Automatic' uses the host locale. Affects games that read PSP system language.");

    s << opt("ppsspp_psp_model", "PSP Model",
             "psp_2000_3000",
             { "psp_1000", "psp_2000_3000" },
             "System",
             "Emulates a PSP-1000 (32MB RAM) or PSP-2000/3000 (64MB RAM). A few games behave differently.");

    // === Video (22) ===

    s << opt("ppsspp_backend", "Backend",
             "auto",
             { "auto", "opengl", "vulkan", "none" },
             "Video",
             "GPU rendering backend. 'auto' picks the best one for the platform. 'none' disables rendering entirely.");

    s << opt("ppsspp_software_rendering", "Software Rendering",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Use the slow but maximum-accuracy software renderer instead of the GPU backend. Diagnostic / accuracy testing.");

    s << optLabeled("ppsspp_internal_resolution", "Rendering Resolution",
                    "960x544",
                    { { "1x (480x272)",     "480x272" },
                      { "2x (960x544)",     "960x544" },
                      { "3x (1440x816)",    "1440x816" },
                      { "4x (1920x1088)",   "1920x1088" },
                      { "5x (2400x1360)",   "2400x1360" },
                      { "6x (2880x1632)",   "2880x1632" },
                      { "7x (3360x1904)",   "3360x1904" },
                      { "8x (3840x2176)",   "3840x2176" },
                      { "9x (4320x2448)",   "4320x2448" },
                      { "10x (4800x2720)",  "4800x2720" } },
                    "Video",
                    "Internal render resolution. Higher = sharper but costs GPU. 1x is native PSP (480x272).");

    s << opt("ppsspp_mulitsample_level", "MSAA Antialiasing",
             "Disabled",
             { "Disabled", "x2", "x4", "x8" },
             "Video",
             "Multisample antialiasing. Vulkan backend only — ignored on OpenGL. Higher = smoother edges, more GPU.");

    s << opt("ppsspp_cropto16x9", "Crop to 16x9",
             "enabled",
             { "enabled", "disabled" },
             "Video",
             "Crops 1 pixel from the top and bottom of the 480x272 frame, yielding exactly 16:9. Eliminates ~1px black bar.");

    s << opt("ppsspp_frameskip", "Frameskip",
             "disabled",
             { "disabled", "1", "2", "3", "4", "5", "6", "7", "8" },
             "Video",
             "Skip rendering of N intermediate frames to maintain audio. 'disabled' = render every frame.");

    s << opt("ppsspp_frameskiptype", "Frameskip Type",
             "Number of frames",
             { "Number of frames", "Percent of FPS" },
             "Video",
             "Whether the Frameskip value is interpreted as a count of frames to skip, or as a percentage of the target FPS.");

    s << opt("ppsspp_auto_frameskip", "Auto Frameskip",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Dynamically frameskip when emulation can't keep up. Overrides manual Frameskip when enabled.");

    s << opt("ppsspp_frame_duplication", "Render Duplicate Frames to 60 Hz",
             "enabled",
             { "enabled", "disabled" },
             "Video",
             "Many PSP games target 30 FPS internally. With this enabled, each frame is rendered twice to match 60 Hz displays smoothly.");

    s << opt("ppsspp_detect_vsync_swap_interval", "Detect Frame Rate Changes",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Detect when a game changes its target frame rate and adjust accordingly. Most users want this off.");

    s << opt("ppsspp_inflight_frames", "Buffer Graphics Commands",
             "Up to 2",
             { "No buffer", "Up to 1", "Up to 2" },
             "Video",
             "How many frames of GPU work may be in-flight. Higher = smoother but more latency. 'No buffer' = lowest latency, may stutter.");

    s << opt("ppsspp_gpu_hardware_transform", "Hardware Transform",
             "enabled",
             { "enabled", "disabled" },
             "Video",
             "Performs vertex transforms on the GPU instead of the CPU. Faster on most hardware. Disable for diagnostics.");

    s << opt("ppsspp_software_skinning", "Software Skinning",
             "enabled",
             { "enabled", "disabled" },
             "Video",
             "Use CPU for character-model bone skinning. Often faster than GPU skinning on weaker GPUs.");

    s << opt("ppsspp_hardware_tesselation", "Hardware Tesselation",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Use GPU tessellation for spline/bezier curves where available. Faster but not universally supported.");

    s << opt("ppsspp_texture_scaling_type", "Texture Upscale Type",
             "xbrz",
             { "xbrz", "hybrid", "bicubic", "hybrid_bicubic" },
             "Video",
             "Algorithm used for texture upscaling when Texture Upscaling Level > disabled.");

    s << opt("ppsspp_texture_scaling_level", "Texture Upscaling Level",
             "disabled",
             { "disabled", "2x", "3x", "4x", "5x" },
             "Video",
             "Upscale textures via xBRZ-family filters. Improves 2D art at significant GPU cost. 'disabled' = original PSP textures.");

    s << opt("ppsspp_texture_deposterize", "Texture Deposterize",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Smooth-out the banding that can appear in upscaled textures. Only relevant when Texture Upscaling Level > disabled.");

    s << opt("ppsspp_texture_shader", "Texture Shader",
             "disabled",
             { "disabled", "2xBRZ", "4xBRZ", "MMPX" },
             "Video",
             "GPU-side texture upscaling shader. Lighter than CPU upscaling but with fewer algorithms.");

    s << opt("ppsspp_texture_anisotropic_filtering", "Anisotropic Filtering",
             "16x",
             { "disabled", "2x", "4x", "8x", "16x" },
             "Video",
             "Improves the sharpness of textures viewed at oblique angles. Cheap on modern GPUs.");

    s << opt("ppsspp_texture_filtering", "Texture Filtering",
             "Auto",
             { "Auto", "Nearest", "Linear", "Auto max quality" },
             "Video",
             "How textures are sampled when scaled. 'Linear' = smooth. 'Nearest' = pixelated (good for retro art).");

    s << opt("ppsspp_smart_2d_texture_filtering", "Smart 2D Texture Filtering",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Adapt the texture filter per-texture to preserve sharp 2D art while smoothing 3D models.");

    s << opt("ppsspp_texture_replacement", "Texture Replacement",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Load community texture packs from the texture replacement folder, if present for the running game.");

    // === Input (4) ===

    s << opt("ppsspp_button_preference", "Confirmation Button",
             "Cross",
             { "Cross", "Circle" },
             "Input",
             "Which face button is 'confirm' in PSP system menus. 'Cross' (US/EU) or 'Circle' (Japan).");

    s << opt("ppsspp_analog_is_circular", "Analog Circle vs Square Gate Compensation",
             "disabled",
             { "enabled", "disabled" },
             "Input",
             "Compensate when your gamepad's analog stick has a square gate (most do) versus the PSP's circular nub.");

    s << opt("ppsspp_analog_deadzone", "Analog Deadzone",
             "0.0",
             { "0.0", "0.05", "0.1", "0.15", "0.2", "0.25", "0.3",
               "0.35", "0.4", "0.45", "0.5" },
             "Input",
             "Ignore stick deflection below this magnitude (0.0–0.5 = 0%–50%). Higher = less drift, less precision.");

    s << opt("ppsspp_analog_sensitivity", "Analog Axis Scale",
             "1.00",
             { "1.00", "1.01", "1.02", "1.03", "1.04", "1.05", "1.06", "1.07", "1.08", "1.09", "1.10",
               "1.11", "1.12", "1.13", "1.14", "1.15", "1.16", "1.17", "1.18", "1.19", "1.20",
               "1.21", "1.22", "1.23", "1.24", "1.25", "1.26", "1.27", "1.28", "1.29", "1.30",
               "1.31", "1.32", "1.33", "1.34", "1.35", "1.36", "1.37", "1.38", "1.39", "1.40",
               "1.41", "1.42", "1.43", "1.44", "1.45", "1.46", "1.47", "1.48", "1.49", "1.50" },
             "Input",
             "Scale analog stick output (1.00 = 100%, 1.50 = 150%). Useful when gamepad sticks underrun the PSP's max range.");

    // === Hacks (6) ===

    s << opt("ppsspp_skip_buffer_effects", "Skip Buffer Effects",
             "disabled",
             { "enabled", "disabled" },
             "Hacks",
             "Skip render-to-texture effects (post-processing, advanced lighting). Big speedup but breaks some games visually.");

    s << opt("ppsspp_disable_range_culling", "Disable Culling",
             "disabled",
             { "enabled", "disabled" },
             "Hacks",
             "Disable distance/range culling. Fixes visual glitches in some games at a small performance cost.");

    s << opt("ppsspp_skip_gpu_readbacks", "Skip GPU Readbacks",
             "disabled",
             { "enabled", "disabled" },
             "Hacks",
             "Skip reading the GPU's framebuffer back to CPU. Big speedup; breaks games that rely on framebuffer feedback.");

    s << opt("ppsspp_lazy_texture_caching", "Lazy Texture Caching (Speedup)",
             "disabled",
             { "enabled", "disabled" },
             "Hacks",
             "Cache textures more aggressively. Faster, but some games that mutate textures rapidly may show stale data.");

    s << opt("ppsspp_spline_quality", "Spline/Bezier Curves Quality",
             "High",
             { "Low", "Medium", "High" },
             "Hacks",
             "Quality of spline/bezier curve tessellation. 'High' = smoothest curves, more GPU work.");

    s << opt("ppsspp_lower_resolution_for_effects", "Lower Resolution for Effects",
             "disabled",
             { "disabled", "Safe", "Balanced", "Aggressive" },
             "Hacks",
             "Render certain framebuffer effects at lower internal resolution to save GPU. 'Safe' is the most conservative.");

    // === Recommended (2 frontend-managed + 10 libretro duplicates) ===
    //
    // The 10 libretro entries are exact duplicates of their natural-category
    // rows above — same keys -> same backing OptionsStore values. The 2
    // FrontendSetting rows (aspect_mode, integer_scale) live in
    // frontend.json because PPSSPP libretro exposes no aspect option of its
    // own; the AspectRatioPreview pane is bound to them via previewSpec().

    s << frontend("aspect_mode", "Aspect Ratio",
                  "native",
                  { "native", "square", "4_3", "16_9", "stretch" },
                  "Recommended",
                  "How the emulated frame is fitted into the window. "
                  "'Native' preserves the PSP's natural aspect ratio. "
                  "'Square Pixel' shows pixel-perfect 1:1. "
                  "'4:3' / '16:9' force a TV-style aspect. "
                  "'Stretch' fills the window ignoring aspect.");

    s << frontend("integer_scale", "Integer Scale",
                  "OFF",
                  { "OFF", "ON" },
                  "Recommended",
                  "Snap the displayed frame to the largest integer multiple "
                  "of native resolution that fits. Eliminates pixel shimmer "
                  "at the cost of some unused screen area.");

    s << opt("ppsspp_cpu_core", "CPU Core",
             "JIT",
             { "JIT", "IR JIT", "Interpreter" },
             "Recommended",
             "Specifies CPU emulator. JIT is fastest. IR JIT is more stable. Interpreter is the most accurate but slowest.");

    s << opt("ppsspp_psp_model", "PSP Model",
             "psp_2000_3000",
             { "psp_1000", "psp_2000_3000" },
             "Recommended",
             "Emulates a PSP-1000 (32MB RAM) or PSP-2000/3000 (64MB RAM). A few games behave differently.");

    s << optLabeled("ppsspp_internal_resolution", "Rendering Resolution",
                    "960x544",
                    { { "1x (480x272)",     "480x272" },
                      { "2x (960x544)",     "960x544" },
                      { "3x (1440x816)",    "1440x816" },
                      { "4x (1920x1088)",   "1920x1088" },
                      { "5x (2400x1360)",   "2400x1360" },
                      { "6x (2880x1632)",   "2880x1632" },
                      { "7x (3360x1904)",   "3360x1904" },
                      { "8x (3840x2176)",   "3840x2176" },
                      { "9x (4320x2448)",   "4320x2448" },
                      { "10x (4800x2720)",  "4800x2720" } },
                    "Recommended",
                    "Internal render resolution. Higher = sharper but costs GPU. 1x is native PSP (480x272).");

    s << opt("ppsspp_mulitsample_level", "MSAA Antialiasing",
             "Disabled",
             { "Disabled", "x2", "x4", "x8" },
             "Recommended",
             "Multisample antialiasing. Vulkan backend only — ignored on OpenGL. Higher = smoother edges, more GPU.");

    s << opt("ppsspp_cropto16x9", "Crop to 16x9",
             "enabled",
             { "enabled", "disabled" },
             "Recommended",
             "Crops 1 pixel from the top and bottom of the 480x272 frame, yielding exactly 16:9. Eliminates ~1px black bar.");

    s << opt("ppsspp_frameskip", "Frameskip",
             "disabled",
             { "disabled", "1", "2", "3", "4", "5", "6", "7", "8" },
             "Recommended",
             "Skip rendering of N intermediate frames to maintain audio. 'disabled' = render every frame.");

    s << opt("ppsspp_auto_frameskip", "Auto Frameskip",
             "disabled",
             { "enabled", "disabled" },
             "Recommended",
             "Dynamically frameskip when emulation can't keep up. Overrides manual Frameskip when enabled.");

    s << opt("ppsspp_texture_scaling_level", "Texture Upscaling Level",
             "disabled",
             { "disabled", "2x", "3x", "4x", "5x" },
             "Recommended",
             "Upscale textures via xBRZ-family filters. Improves 2D art at significant GPU cost. 'disabled' = original PSP textures.");

    s << opt("ppsspp_texture_anisotropic_filtering", "Anisotropic Filtering",
             "16x",
             { "disabled", "2x", "4x", "8x", "16x" },
             "Recommended",
             "Improves the sharpness of textures viewed at oblique angles. Cheap on modern GPUs.");

    s << opt("ppsspp_skip_buffer_effects", "Skip Buffer Effects",
             "disabled",
             { "enabled", "disabled" },
             "Recommended",
             "Skip render-to-texture effects (post-processing, advanced lighting). Big speedup but breaks some games visually.");

    return s;
}

QString PpssppLibretroAdapter::extractSerial(const QString& romPath) const {
    QByteArray sfoData = Iso9660::readFile(romPath, "PSP_GAME/PARAM.SFO");
    if (sfoData.isEmpty()) {
        qWarning() << "[PPSSPP-libretro] Failed to read PSP_GAME/PARAM.SFO from:" << romPath;
        return {};
    }
    return SfoParser::extractDiscId(sfoData);
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
