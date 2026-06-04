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
// A digital pad has no analog sticks, so L3/R3 are omitted. Spotlight coords
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
// Pattern mirrors Pcsx2LibretroAdapter::settingsSchema and
// DolphinLibretroAdapter::settingsSchema (sibling adapters in this directory).
// Keys + values + defaults match duckstation-libretro's libretro_core_options.cpp
// byte-for-byte (enforced by tools/check_schema_fidelity.py in Phase 4).
// Core pairs are {value, label}; host pairs are {label, value} (flipped).
// Boolean core options use {"true","Enabled"}/{"false","Disabled"} in the core;
// the host mirrors them as {{"Enabled","true"},{"Disabled","false"}}.
QVector<SettingDef> DuckStationLibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    auto opt = [](const QString& category,
                  const QString& group,
                  const QString& key,
                  const QString& label,
                  const QString& def,
                  const QVector<QPair<QString,QString>>& valuesAndLabels,
                  const QString& tooltip,
                  const QString& dependsOn = {}) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.category = category;
        d.subcategory = "";
        d.group = group;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.tooltip = tooltip;
        d.type = SettingDef::Combo;
        d.options = valuesAndLabels;
        d.dependsOn = dependsOn;
        return d;
    };

    // gopt(): like opt() but with category hardcoded to "Graphics" and
    // subcategory set to the first argument. Used for all Graphics sub-tabs
    // (Rendering / Advanced / Texture Replacement). The schema-fidelity tool
    // (tools/check_schema_fidelity.py) accepts both opt() and gopt() call
    // sites and pulls (key, default, values) from the same positional layout.
    auto gopt = [](const QString& subcategory,
                   const QString& group,
                   const QString& key,
                   const QString& label,
                   const QString& def,
                   const QVector<QPair<QString,QString>>& valuesAndLabels,
                   const QString& tooltip,
                   const QString& dependsOn = {}) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.category = "Graphics";
        d.subcategory = subcategory;
        d.group = group;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.tooltip = tooltip;
        d.type = SettingDef::Combo;
        d.options = valuesAndLabels;
        d.dependsOn = dependsOn;
        return d;
    };

    // Shared value lists — hoisted so both the primary category rows and the
    // Recommended-card duplicate rows can reference them.
    const QVector<QPair<QString,QString>> memCardTypes = {
        {"No Memory Card",                         "None"},
        {"Shared Between All Games",               "Shared"},
        {"Separate Card Per Game (Serial)",        "PerGame"},
        {"Separate Card Per Game (Title)",         "PerGameTitle"},
        {"Separate Card Per Game (File Title)",    "PerGameFileTitle"},
        {"Non-Persistent Card (Do Not Save)",      "NonPersistent"},
    };
    const QVector<QPair<QString,QString>> textureFilterOptions = {
        {"Nearest-Neighbor",                       "Nearest"},
        {"Bilinear",                               "Bilinear"},
        {"Bilinear (No Edge Blending)",            "BilinearBinAlpha"},
        {"JINC2 (Slow)",                           "JINC2"},
        {"JINC2 (Slow, No Edge Blending)",         "JINC2BinAlpha"},
        {"xBR (Very Slow)",                        "xBR"},
        {"xBR (Very Slow, No Edge Blending)",      "xBRBinAlpha"},
        {"Scale2x (EPX)",                          "Scale2x"},
        {"Scale3x (Slow)",                         "Scale3x"},
        {"MMPX (Slow)",                            "MMPX"},
        {"MMPX Enhanced (Slow)",                   "MMPXEnhanced"},
    };
    const QVector<QPair<QString,QString>> scalingOptions = {
        {"Nearest-Neighbor",           "Nearest"},
        {"Nearest-Neighbor (Integer)", "NearestInteger"},
        {"Bilinear (Smooth)",          "BilinearSmooth"},
        {"Bilinear (Hybrid)",          "BilinearHybrid"},
        {"Bilinear (Sharp)",           "BilinearSharp"},
        {"Bilinear (Integer)",         "BilinearInteger"},
        {"Lanczos (Sharp)",            "Lanczos"},
    };
    const QVector<QPair<QString,QString>> cdromReadSpeedupOptions = {
        {"None (Double Speed)", "1"},
        {"2x (Quad Speed)",     "2"},
        {"3x (6x Speed)",       "3"},
        {"4x (8x Speed)",       "4"},
        {"5x (10x Speed)",      "5"},
        {"6x (12x Speed)",      "6"},
        {"Maximum (Safer)",     "0"},
    };

    // ── Recommended (curated cross-category view) ─────────────────────────
    //
    // Duplicate-key view: rows point at the same backing core options as
    // their primary category rows. The fidelity tool deduplicates by key so
    // duplicates are fine. Mirrors dolphin_libretro_adapter / pcsx2_libretro_adapter.

    // Performance
    s.append(opt(
        "Recommended", "Performance",
        "duckstation_gpu_renderer", "Renderer", "Automatic",
        {{"Auto", "Automatic"}, {"Metal", "Metal"}, {"Software", "Software"}},
        "GPU backend used for rendering. Auto picks Metal on macOS. Software "
        "emulates the GPU on the CPU for perfect accuracy at a significant "
        "performance cost."));

    s.append(opt(
        "Recommended", "Performance",
        "duckstation_cpu_execution_mode", "Execution Mode", "Recompiler",
        {{"Interpreter (Slowest)",       "Interpreter"},
         {"Cached Interpreter (Faster)", "CachedInterpreter"},
         {"Recompiler (Fastest)",        "Recompiler"}},
        "How the PSX CPU is emulated. Recompiler is fastest; Interpreter is "
        "used only for debugging."));

    s.append(opt(
        "Recommended", "Performance",
        "duckstation_cdrom_read_speedup", "CD-ROM Read Speedup", "1",
        cdromReadSpeedupOptions,
        "Speeds up CD-ROM reads beyond hardware limits. Cuts loading times in "
        "most games with no risk for the majority of titles."));

    // Visual Quality
    s.append(opt(
        "Recommended", "Visual Quality",
        "duckstation_gpu_resolution_scale", "Internal Resolution", "4",
        {{"1x Native", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x Native (1440p)", "4"},
         {"5x", "5"}, {"6x", "6"}, {"7x", "7"}, {"8x Native (4K)", "8"}},
        "Render scale relative to native PS1 resolution. The single biggest "
        "knob for visual fidelity. Higher values produce sharper visuals at "
        "the cost of GPU performance."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "duckstation_display_aspect_ratio", "Aspect Ratio", "Auto (Game Native)",
        {{"Auto (Game Native)", "Auto (Game Native)"}, {"Stretch To Fill", "Stretch To Fill"},
         {"4:3", "4:3"}, {"16:9", "16:9"}, {"19:9", "19:9"}, {"20:9", "20:9"},
         {"21:9", "21:9"}, {"16:10", "16:10"}, {"PAR 1:1", "PAR 1:1"}},
        "Display aspect ratio. Auto matches the game; force 16:9 for a "
        "widescreen TV; Stretch fills the whole window."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "duckstation_gpu_widescreen_hack", "Widescreen Rendering", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Stretches 3D geometry to fill a widescreen display. Pair with "
        "Aspect Ratio = 16:9 for the best result on a widescreen TV."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "duckstation_gpu_pgxp_enable", "PGXP Geometry Correction", "true",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Fixes polygon wobble by using a sub-pixel geometry buffer. Highly "
        "recommended — almost every PS1 game looks better with it on."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "duckstation_gpu_multisamples", "Multi-Sampling", "1",
        {{"Disabled", "1"}, {"2x MSAA", "2"}, {"4x MSAA", "4"},
         {"8x MSAA", "8"}, {"16x MSAA", "16"}},
        "Multi-sample anti-aliasing. Smoother edges at modest GPU cost."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "duckstation_gpu_texture_filter", "Texture Filtering", "Nearest",
        textureFilterOptions,
        "How textures are sampled. Nearest matches original hardware; "
        "Bilinear smooths low-res textures."));

    // Convenience
    s.append(opt(
        "Recommended", "Convenience",
        "duckstation_console_region", "Region", "Auto",
        {{"Auto-Detect",                "Auto"},
         {"NTSC-J (Japan)",             "NTSC-J"},
         {"NTSC-U/C (US, Canada)",      "NTSC-U"},
         {"PAL (Europe, Australia)",    "PAL"}},
        "PS1 region. Auto-Detect picks the right region from the disc; force "
        "a value only if a particular game refuses to boot."));

    s.append(opt(
        "Recommended", "Convenience",
        "duckstation_bios_fast_boot", "Fast Boot", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Skips the BIOS boot animation and Sony intro."));

    // ── Console ───────────────────────────────────────────────────────────
    //
    // Console group — mirrors standalone DuckStation consolesettingswidget.cpp.
    s.append(opt(
        "Console", "Console",
        "duckstation_console_region", "Region", "Auto",
        {{"Auto-Detect",                "Auto"},
         {"NTSC-J (Japan)",             "NTSC-J"},
         {"NTSC-U/C (US, Canada)",      "NTSC-U"},
         {"PAL (Europe, Australia)",    "PAL"}},
        "PS1 region. Auto-Detect picks the right region from the disc."));

    s.append(opt(
        "Console", "Console",
        "duckstation_gpu_force_video_timing", "Frame Rate", "Disabled",
        {{"Auto-Detect", "Disabled"}, {"NTSC (60hz)", "NTSC"}, {"PAL (50hz)", "PAL"}},
        "Forces a video timing mode, overriding auto-detection."));

    s.append(opt(
        "Console", "Console",
        "duckstation_console_8mb_ram", "Enable 8MB RAM (Dev Console)", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Expands RAM to 8MB, as found in dev units."));

    s.append(opt(
        "Console", "Console",
        "duckstation_bios_fast_boot", "Fast Boot", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Skips the BIOS boot animation and Sony intro."));

    s.append(opt(
        "Console", "Console",
        "duckstation_bios_fast_forward_boot", "Fast Forward Boot", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Runs at fast forward speed during boot until the game starts.",
        "duckstation_bios_fast_boot"));

    // CPU Emulation group
    s.append(opt(
        "Console", "CPU Emulation",
        "duckstation_cpu_execution_mode", "Execution Mode", "Recompiler",
        {{"Interpreter (Slowest)",       "Interpreter"},
         {"Cached Interpreter (Faster)", "CachedInterpreter"},
         {"Recompiler (Fastest)",        "Recompiler"}},
        "How the PSX CPU is emulated. Recompiler is fastest; Cached "
        "Interpreter is more accurate; Interpreter is the slowest, used "
        "only for debugging."));

    s.append(opt(
        "Console", "CPU Emulation",
        "duckstation_cpu_overclock_enable",
        "Enable Clock Speed Control (Overclocking/Underclocking)", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Enables CPU overclocking or underclocking."));

    s.append(opt(
        "Console", "CPU Emulation",
        "duckstation_cpu_overclock_percent", "Overclocking Percentage", "100",
        {{"50%",   "50"},  {"75%",   "75"},  {"100%",  "100"}, {"150%", "150"},
         {"200%",  "200"}, {"300%",  "300"},  {"400%",  "400"}, {"500%", "500"},
         {"750%",  "750"}, {"1000%", "1000"}},
        "Sets the CPU clock speed. 100% matches the original PSX CPU "
        "(33.87 MHz).",
        "duckstation_cpu_overclock_enable"));

    s.append(opt(
        "Console", "CPU Emulation",
        "duckstation_cpu_recompiler_icache", "Enable Recompiler ICache", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Simulates the instruction cache in the recompiler. Slower but more "
        "accurate.",
        "duckstation_cpu_execution_mode=Recompiler"));

    // CD-ROM Emulation group
    s.append(opt(
        "Console", "CD-ROM Emulation",
        "duckstation_cdrom_read_speedup", "Read Speedup", "1",
        cdromReadSpeedupOptions,
        "Speeds up CD-ROM reads beyond hardware limits."));

    s.append(opt(
        "Console", "CD-ROM Emulation",
        "duckstation_cdrom_seek_speedup", "Seek Speedup", "1",
        {{"None (Normal Speed)", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x", "4"},
         {"5x", "5"}, {"6x", "6"}, {"Maximum (Safer)", "0"}},
        "Reduces seek time beyond hardware limits."));

    s.append(opt(
        "Console", "CD-ROM Emulation",
        "duckstation_cdrom_preload", "Preload Image To RAM", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Loads the disc image into RAM to reduce I/O stutter."));

    s.append(opt(
        "Console", "CD-ROM Emulation",
        "duckstation_cdrom_image_patches", "Apply Image Patches", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Applies PPF patches found alongside the disc image."));

    s.append(opt(
        "Console", "CD-ROM Emulation",
        "duckstation_cdrom_auto_disc_change", "Switch to Next Disc on Stop", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Automatically switches to the next queued disc when the current "
        "one stops."));

    s.append(opt(
        "Console", "CD-ROM Emulation",
        "duckstation_cdrom_ignore_subcode", "Ignore Drive Subcode", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Ignores subcode errors on physical drives."));

    // ── Memory Cards ──────────────────────────────────────────────────────
    s.append(opt(
        "Memory Cards", "Memory Card 1",
        "duckstation_memcard_1_type", "Memory Card Type", "PerGameTitle",
        memCardTypes,
        "Memory card type for slot 1."));

    s.append(opt(
        "Memory Cards", "Memory Card 2",
        "duckstation_memcard_2_type", "Memory Card Type", "None",
        memCardTypes,
        "Memory card type for slot 2."));

    // ── Graphics / Rendering ──────────────────────────────────────────────
    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_renderer", "Renderer", "Automatic",
        {{"Auto", "Automatic"}, {"Metal", "Metal"}, {"Software", "Software"}},
        "GPU backend used for rendering. Auto picks Metal on macOS. Software "
        "emulates the GPU on the CPU for perfect accuracy at a significant "
        "performance cost."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_resolution_scale", "Internal Resolution", "4",
        {{"1x Native", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x Native (1440p)", "4"},
         {"5x", "5"}, {"6x", "6"}, {"7x", "7"}, {"8x Native (4K)", "8"}},
        "Renders the PS1 GPU at a higher resolution. Higher values produce "
        "sharper visuals at the cost of GPU performance."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_downsample_mode", "Down-Sampling", "Disabled",
        {{"Disabled", "Disabled"},
         {"Box (Downsample 3D/Smooth All)", "Box"},
         {"Adaptive (Preserve 3D/Smooth 2D)", "Adaptive"}},
        "Downsamples the rendered image back to native resolution."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_texture_filter", "Texture Filtering", "Nearest",
        textureFilterOptions,
        "Texture filter applied to 3D geometry."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_sprite_texture_filter", "Sprite Texture Filtering", "Nearest",
        textureFilterOptions,
        "Texture filtering applied only to 2D sprites."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_dithering", "Dithering", "TrueColor",
        {{"Unscaled",                       "Unscaled"},
         {"Unscaled (Shader Blending)",     "UnscaledShaderBlend"},
         {"Scaled",                         "Scaled"},
         {"Scaled (Shader Blending)",       "ScaledShaderBlend"},
         {"True Color",                     "TrueColor"},
         {"True Color (Full)",              "TrueColorFull"}},
        "Controls dithering applied to the rendered image."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_deinterlacing", "Deinterlacing", "Progressive",
        {{"Progressive (Optimal)", "Progressive"}, {"Disabled", "Disabled"},
         {"Weave", "Weave"}, {"Blend", "Blend"}, {"Adaptive", "Adaptive"}},
        "Controls deinterlacing mode for interlaced content."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_display_aspect_ratio", "Aspect Ratio", "Auto (Game Native)",
        {{"Auto (Game Native)", "Auto (Game Native)"}, {"Stretch To Fill", "Stretch To Fill"},
         {"4:3", "4:3"}, {"16:9", "16:9"}, {"19:9", "19:9"}, {"20:9", "20:9"},
         {"21:9", "21:9"}, {"16:10", "16:10"}, {"PAR 1:1", "PAR 1:1"}},
        "Aspect ratio used when displaying the image."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_display_crop", "Crop", "Overscan",
        {{"None", "None"},
         {"Only Overscan Area", "Overscan"},
         {"Only Overscan Area (Aspect Uncorrected)", "OverscanUncorrected"},
         {"All Borders", "Borders"},
         {"All Borders (Aspect Uncorrected)", "BordersUncorrected"}},
        "Controls how much of the image border is cropped."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_display_scaling", "Scaling", "BilinearSmooth",
        scalingOptions,
        "Scaling filter applied to the final output."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_display_scaling_24bit", "FMV Scaling", "BilinearSmooth",
        scalingOptions,
        "Scaling filter applied during FMV playback."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_pgxp_enable", "PGXP Geometry Correction", "true",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Fixes polygon wobble by using a sub-pixel geometry buffer."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_pgxp_culling", "PGXP Culling Correction", "true",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Corrects polygon culling when PGXP is enabled.",
        "duckstation_gpu_pgxp_enable"));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_pgxp_texture_correction", "PGXP Texture Correction", "true",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Applies perspective-correct texturing when PGXP is enabled.",
        "duckstation_gpu_pgxp_enable"));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_widescreen_hack", "Widescreen Rendering", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Stretches 3D geometry to fill a widescreen display."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_display_force_4_3_for_24bit", "Force 4:3 For FMVs", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Switches to 4:3 aspect ratio during FMV sequences."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_chroma_smoothing_24bit", "FMV Chroma Smoothing", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Applies chroma smoothing to FMV playback."));

    s.append(gopt(
        "Rendering", "Rendering",
        "duckstation_gpu_force_round_texcoords", "Round Upscaled Texture Coordinates", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Reduces texture seams at high resolutions."));

    // ── Graphics / Advanced ────────────────────────────────────────────────
    s.append(gopt(
        "Advanced", "Display Options",
        "duckstation_display_alignment", "Position", "Center",
        {{"Left/Top", "LeftOrTop"}, {"Center", "Center"}, {"Right/Bottom", "RightOrBottom"}},
        "Controls horizontal/vertical alignment of the display within the window."));

    s.append(gopt(
        "Advanced", "Display Options",
        "duckstation_display_rotation", "Rotation", "Normal",
        {{"No Rotation", "Normal"}, {"90 Degrees", "Rotate90"},
         {"180 Degrees", "Rotate180"}, {"270 Degrees", "Rotate270"}},
        "Rotates the display output."));

    s.append(gopt(
        "Advanced", "Display Options",
        "duckstation_display_fine_crop", "Fine Crop Mode", "None",
        {{"None", "None"}, {"Video Resolution", "VideoResolution"},
         {"Internal Resolution", "InternalResolution"}, {"Window Resolution", "WindowResolution"}},
        "Controls the fine crop mode for precise border trimming."));

    s.append(gopt(
        "Advanced", "Display Options",
        "duckstation_display_disable_mailbox", "Disable Mailbox Presentation", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Forces FIFO presentation instead of mailbox."));

    s.append(gopt(
        "Advanced", "Rendering Options",
        "duckstation_gpu_multisamples", "Multi-Sampling", "1",
        {{"Disabled", "1"}, {"2x MSAA", "2"}, {"4x MSAA", "4"},
         {"8x MSAA", "8"}, {"16x MSAA", "16"}},
        "Anti-aliasing via multi-sampling."));

    s.append(gopt(
        "Advanced", "Rendering Options",
        "duckstation_gpu_line_detect", "Line Detection", "Disabled",
        {{"Disabled", "Disabled"}, {"Quads", "Quads"},
         {"Basic Triangles", "BasicTriangles"}, {"Aggressive Triangles", "AggressiveTriangles"}},
        "Detects and renders single-pixel lines correctly at higher resolutions."));

    s.append(gopt(
        "Advanced", "Rendering Options",
        "duckstation_gpu_modulation_crop",
        "Texture Modulation Cropping (\"Old/v0 GPU\")", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Uses legacy texture modulation cropping behaviour."));

    s.append(gopt(
        "Advanced", "Rendering Options",
        "duckstation_gpu_scaled_interlacing", "Scaled Interlacing", "true",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Scales interlaced content to full resolution."));

    s.append(gopt(
        "Advanced", "Rendering Options",
        "duckstation_gpu_sw_readbacks", "Software Renderer Readbacks", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Uses the software renderer for GPU readbacks."));

    // ── Graphics / Texture Replacement ────────────────────────────────────
    s.append(gopt(
        "Texture Replacement", "General Settings",
        "duckstation_gpu_texture_cache", "Enable Texture Cache", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Enables the texture cache required for texture replacement."));

    s.append(gopt(
        "Texture Replacement", "Texture Replacement",
        "duckstation_texrepl_enable_replacements", "Enable Texture Replacement", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Enables loading of replacement textures.",
        "duckstation_gpu_texture_cache"));

    s.append(gopt(
        "Texture Replacement", "Texture Replacement",
        "duckstation_texrepl_dump_textures", "Enable Texture Dumping", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Dumps textures to disk for use as replacement templates.",
        "duckstation_gpu_texture_cache"));

    s.append(gopt(
        "Texture Replacement", "Texture Replacement",
        "duckstation_texrepl_always_track_uploads", "Always Track Uploads", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Always tracks texture uploads even when dumping is disabled.",
        "duckstation_gpu_texture_cache"));

    s.append(gopt(
        "Texture Replacement", "Texture Replacement",
        "duckstation_texrepl_dump_replaced_textures", "Dump Replaced Textures", "true",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Dumps textures that have been replaced.",
        "duckstation_texrepl_dump_textures"));

    s.append(gopt(
        "Texture Replacement", "VRAM Write Replacement",
        "duckstation_texrepl_enable_vram_write_replacements",
        "Enable VRAM Write Replacement", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Enables replacement of VRAM writes (background/FMV textures)."));

    s.append(gopt(
        "Texture Replacement", "VRAM Write Replacement",
        "duckstation_texrepl_dump_vram_writes", "Enable VRAM Write Dumping", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Dumps VRAM writes to disk for use as replacement templates."));

    s.append(gopt(
        "Texture Replacement", "VRAM Write Replacement",
        "duckstation_texrepl_use_old_mdec_routines", "Use Old MDEC Routines", "false",
        {{"Enabled", "true"}, {"Disabled", "false"}},
        "Uses older MDEC decode routines for VRAM write compatibility."));

    return s;
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
    // No OSD sub-tab preview yet — Phase 6.
    return {};
}
