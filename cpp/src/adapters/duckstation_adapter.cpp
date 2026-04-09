#include "duckstation_adapter.h"

static const char* DUCKSTATION_INSTALL_FOLDER = "duckstation";

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>

// ============================================================================
// Settings schema
// ============================================================================

QString DuckStationAdapter::configFilePath() const {
    return portableDir() + "/settings.ini";
}

QVector<SettingDef> DuckStationAdapter::settingsSchema() const {
    // Field order: category, subcategory, group, section, key, label, tooltip,
    //              type, defaultValue, options, minVal, maxVal, step, layout, suffix

    // Shared option lists
    const QVector<QPair<QString,QString>> memCardTypes = {
        {"No Memory Card",                          "None"},
        {"Shared Between All Games",                "Shared"},
        {"Separate Card Per Game (Serial)",          "PerGame"},
        {"Separate Card Per Game (Title)",           "PerGameTitle"},
        {"Separate Card Per Game (File Title)",      "PerGameFileTitle"},
        {"Non-Persistent Card (Do Not Save)",        "NonPersistent"},
    };

    // INI values must use shortest float representation (e.g. "1", "0.5", "2") —
    // DuckStation writes floats via StringUtil::ToChars / std::to_chars which
    // strips trailing zeros, so padded values fail to round-trip. See audit
    // 2026-04-06.
    const QVector<QPair<QString,QString>> speedOptions = {
        {"10% [6 FPS]",   "0.1"},
        {"20% [12 FPS]",  "0.2"},
        {"30% [18 FPS]",  "0.3"},
        {"40% [24 FPS]",  "0.4"},
        {"50% [30 FPS]",  "0.5"},
        {"60% [36 FPS]",  "0.6"},
        {"70% [42 FPS]",  "0.7"},
        {"75% [45 FPS]",  "0.75"},
        {"80% [48 FPS]",  "0.8"},
        {"90% [54 FPS]",  "0.9"},
        {"100% [60 FPS]", "1"},
        {"120% [72 FPS]", "1.2"},
        {"150% [90 FPS]", "1.5"},
        {"175% [105 FPS]","1.75"},
        {"200% [120 FPS]","2"},
        {"250% [150 FPS]","2.5"},
        {"300% [180 FPS]","3"},
        {"350% [210 FPS]","3.5"},
        {"400% [240 FPS]","4"},
        {"450% [270 FPS]","4.5"},
        {"500% [300 FPS]","5"},
    };

    const QVector<QPair<QString,QString>> turbospeedOptions = {
        {"Unlimited",        "0"},
        {"100% [60 FPS]",   "1"},
        {"150% [90 FPS]",   "1.5"},
        {"200% [120 FPS]",  "2"},
        {"300% [180 FPS]",  "3"},
        {"400% [240 FPS]",  "4"},
        {"500% [300 FPS]",  "5"},
        {"600% [360 FPS]",  "6"},
        {"700% [420 FPS]",  "7"},
        {"800% [480 FPS]",  "8"},
        {"900% [540 FPS]",  "9"},
        {"1000% [600 FPS]", "10"},
    };

    const QVector<QPair<QString,QString>> cdromSpeedupOptions = {
        {"None (Double Speed)", "1"},
        {"2x (Quad Speed)",     "2"},
        {"3x (6x Speed)",      "3"},
        {"4x (8x Speed)",      "4"},
        {"5x (10x Speed)",     "5"},
        {"6x (12x Speed)",     "6"},
        {"Maximum (Safer)",    "0"},
    };

    // Native DuckStation CDROM SeekSpeedup uses 1 for normal speed and 0 for the
    // maximum-cycles override (cdrom.cpp:1616, consolesettingswidget.cpp:19,74).
    // Earlier versions of this list had the endpoints swapped, so picking
    // "Maximum (Safer)" actually selected normal speed. See audit 2026-04-06.
    const QVector<QPair<QString,QString>> cdromSeekOptions = {
        {"None (Normal Speed)", "1"},
        {"2x",  "2"},
        {"3x",  "3"},
        {"4x",  "4"},
        {"5x",  "5"},
        {"6x",  "6"},
        {"Maximum (Safer)", "0"},
    };

    const QVector<QPair<QString,QString>> textureFilterOptions = {
        {"Nearest-Neighbor",                      "Nearest"},
        {"Bilinear",                              "Bilinear"},
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

    // INI values must match s_display_scaling_names from
    // references/duckstation-master/src/core/settings.cpp:2188-2190.
    // Three names were wrong before: NearestNeighbor → Nearest,
    // NearestNeighborInteger → NearestInteger, Bilinear → BilinearSmooth.
    // See audit 2026-04-06.
    const QVector<QPair<QString,QString>> scalingOptions = {
        {"Nearest-Neighbor",            "Nearest"},
        {"Nearest-Neighbor (Integer)",  "NearestInteger"},
        {"Bilinear (Smooth)",           "BilinearSmooth"},
        {"Bilinear (Hybrid)",           "BilinearHybrid"},
        {"Bilinear (Sharp)",            "BilinearSharp"},
        {"Bilinear (Integer)",          "BilinearInteger"},
        {"Lanczos (Sharp)",             "Lanczos"},
    };

    QVector<SettingDef> s;

    // =========================================================================
    // Console category
    // =========================================================================

    // Console group
    s.append({"Console", "", "Console", "Console", "Region", "Region", "",
              SettingDef::Combo, "Auto",
              {{"Auto-Detect", "Auto"}, {"NTSC-U", "NTSC-U"}, {"NTSC-J", "NTSC-J"}, {"PAL", "PAL"}},
              0, 0, 0, "", ""});
    s.append({"Console", "", "Console", "GPU", "ForceVideoTiming", "Force Video Timing", "",
              SettingDef::Combo, "Disabled",
              {{"Disabled (Auto)", "Disabled"}, {"NTSC/60hz", "NTSC"}, {"PAL/50hz", "PAL"}},
              0, 0, 0, "", ""});
    s.append({"Console", "", "Console", "BIOS", "PatchFastBoot", "Fast Boot", "", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Console", "", "Console", "MemoryCards", "FastForwardAccess", "Fast Forward Memory Card Access", "", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Console", "", "Console", "BIOS", "FastForwardBoot", "Fast Forward Boot", "", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Console", "", "Console", "Console", "Enable8MBRAM", "Enable 8MB RAM (Dev Console)", "Expands RAM to 8MB, as found in dev units.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // CPU Emulation group
    s.append({"Console", "", "CPU Emulation", "CPU", "ExecutionMode", "Execution Mode", "",
              SettingDef::Combo, "Recompiler",
              {{"Interpreter (Slowest)",        "Interpreter"},
               {"Cached Interpreter (Faster)",  "CachedInterpreter"},
               {"Recompiler (Fastest)",         "Recompiler"}},
              0, 0, 0, "", ""});
    s.append({"Console", "", "CPU Emulation", "CPU", "OverclockEnable", "Enable Clock Speed Control (Overclocking/Underclocking)", "", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Console", "", "CPU Emulation", "CPU", "OverclockNumerator", "Clock Speed Multiplier", "Sets the CPU clock multiplier (denominator stays at 1).",
              SettingDef::Combo, "1",
              {{"1x (100%)", "1"}, {"2x (200%)", "2"}, {"3x (300%)", "3"}, {"4x (400%)", "4"},
               {"5x (500%)", "5"}, {"6x (600%)", "6"}, {"7x (700%)", "7"}, {"8x (800%)", "8"},
               {"9x (900%)", "9"}, {"10x (1000%)", "10"}},
              0, 0, 0, "", ""});
    s.append({"Console", "", "CPU Emulation", "CPU", "RecompilerICache", "Enable Recompiler ICache", "Simulates the instruction cache in the recompiler. Slower but more accurate.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // CD-ROM Emulation group
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "ReadSpeedup", "Read Speedup", "Speeds up CD-ROM reads beyond hardware limits.",
              SettingDef::Combo, "1", cdromSpeedupOptions, 0, 0, 0, "", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "SeekSpeedup", "Seek Speedup", "Reduces seek time beyond hardware limits.",
              SettingDef::Combo, "1", cdromSeekOptions, 0, 0, 0, "", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "LoadImageToRAM", "Preload Image To RAM", "Loads the disc image into RAM to reduce I/O stutter.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "AutoDiscChange", "Switch to Next Disc on Stop", "Automatically switches to the next queued disc when the current one stops.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "LoadImagePatches", "Apply Image Patches", "Applies PPF patches found alongside the disc image.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "IgnoreHostSubcode", "Ignore Drive Subcode", "Ignores subcode errors on physical drives.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // =========================================================================
    // Emulation category
    // =========================================================================

    // Speed Control group
    s.append({"Emulation", "", "Speed Control", "Main", "EmulationSpeed", "Emulation Speed", "Sets the target emulation speed.",
              SettingDef::Combo, "1", speedOptions, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Speed Control", "Main", "FastForwardSpeed", "Fast Forward Speed", "Speed used when the fast forward hotkey is held.",
              SettingDef::Combo, "0", turbospeedOptions, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Speed Control", "Main", "TurboSpeed", "Turbo Speed", "Speed used when the turbo hotkey is held.",
              SettingDef::Combo, "0", turbospeedOptions, 0, 0, 0, "", ""});

    // Latency Control group
    s.append({"Emulation", "", "Latency Control", "Display", "VSync", "Vertical Sync (VSync)", "Synchronises frame presentation with the display refresh rate.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Latency Control", "Main", "SyncToHostRefreshRate", "Sync To Host Refresh Rate", "Adjusts emulation speed to match the host display refresh rate.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Latency Control", "Display", "OptimalFramePacing", "Optimal Frame Pacing", "Enables an optimal frame pacing mode that reduces jitter.", SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Latency Control", "Display", "PreFrameSleep", "Reduce Input Latency", "Reduces input lag by delaying frame presentation slightly.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Latency Control", "Display", "SkipPresentingDuplicateFrames", "Skip Duplicate Frame Display", "Skips presenting duplicate frames to reduce GPU load.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // Rewind group
    s.append({"Emulation", "", "Rewind", "Main", "RewindEnable", "Enable Rewinding", "Enables the rewind feature (uses extra RAM).", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    { SettingDef d = {"Emulation", "", "Rewind", "GPU", "UseSoftwareRendererForMemoryStates", "Use Software Renderer (Low VRAM Mode)", "Uses the software renderer for rewind snapshots to save VRAM.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""}; d.dependsOn = "RewindEnable"; s.append(d); }
    { SettingDef d = {"Emulation", "", "Rewind", "Main", "RewindFrequency", "Rewind Save Frequency", "How often (in seconds) to save a rewind snapshot.",
              SettingDef::Float, "10", {}, 0.0, 60.0, 0.1, "", "Seconds"}; d.dependsOn = "RewindEnable"; s.append(d); }
    { SettingDef d = {"Emulation", "", "Rewind", "Main", "RewindSaveSlots", "Rewind Buffer Size", "Number of rewind frames to keep in memory.",
              SettingDef::Int, "10", {}, 1, 1000, 1, "", "Frames"}; d.dependsOn = "RewindEnable"; s.append(d); }

    // Runahead group
    s.append({"Emulation", "", "Runahead", "Main", "RunaheadFrameCount", "Runahead", "Simulates N frames ahead to hide input latency.",
              SettingDef::Combo, "0",
              {{"Disabled", "0"}, {"1 Frame", "1"}, {"2 Frames", "2"}, {"3 Frames", "3"},
               {"4 Frames", "4"}, {"5 Frames", "5"}, {"6 Frames", "6"}, {"7 Frames", "7"},
               {"8 Frames", "8"}},
              0, 0, 0, "", ""});
    { SettingDef d = {"Emulation", "", "Runahead", "Main", "RunaheadForAnalogInput", "Enable for Analog Input", "Enables runahead even when an analog controller is connected.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""}; d.dependsOn = "RunaheadFrameCount"; s.append(d); }

    // =========================================================================
    // Graphics category — top-level (no subcategory)
    // =========================================================================

    s.append({"Graphics", "", "", "GPU", "Renderer", "Renderer", "GPU backend used for rendering.",
              SettingDef::Combo, "Automatic",
              {{"Automatic", "Automatic"}, {"Vulkan", "Vulkan"}, {"Metal", "Metal"}, {"OpenGL", "OpenGL"}, {"Software", "Software"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "", "", "GPU", "Adapter", "Adapter", "GPU adapter to use for rendering.",
              SettingDef::Combo, "",
              {{"Default", ""}},
              0, 0, 0, "", ""});

    // ── Graphics / Rendering subcategory ─────────────────────────────────

    s.append({"Graphics", "Rendering", "", "GPU", "ResolutionScale", "Internal Resolution", "Renders the PS1 GPU at a higher resolution.",
              SettingDef::Combo, "1",
              {{"1x Native",  "1"},  {"2x", "2"},  {"3x", "3"},  {"4x Native (1440p)", "4"},
               {"5x", "5"},  {"6x", "6"},  {"7x", "7"},  {"8x Native (4K)", "8"},
               {"9x", "9"},  {"10x","10"}, {"11x","11"}, {"12x","12"},
               {"13x","13"}, {"14x","14"}, {"15x","15"}, {"16x","16"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "DownsampleMode", "Down-Sampling", "Downsamples the rendered image back to native resolution.",
              SettingDef::Combo, "Disabled",
              {{"Disabled", "Disabled"}, {"Box", "Box"}, {"Adaptive", "Adaptive"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "TextureFilter", "Texture Filtering", "",
              SettingDef::Combo, "Nearest", textureFilterOptions, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "SpriteTextureFilter", "Sprite Texture Filtering", "Texture filtering applied only to 2D sprites.",
              SettingDef::Combo, "Nearest", textureFilterOptions, 0, 0, 0, "", ""});
    // INI values must match s_gpu_dithering_mode_names from
    // references/duckstation-master/src/core/settings.cpp:1708-1711.
    // Native default is TrueColor (settings.h:226). See audit 2026-04-06.
    s.append({"Graphics", "Rendering", "", "GPU", "DitheringMode", "Dithering", "",
              SettingDef::Combo, "TrueColor",
              {{"Unscaled", "Unscaled"},
               {"Unscaled (Shader Blending)", "UnscaledShaderBlend"},
               {"Scaled", "Scaled"},
               {"Scaled (Shader Blending)", "ScaledShaderBlend"},
               {"True Color", "TrueColor"},
               {"True Color (Full)", "TrueColorFull"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "DeinterlacingMode", "Deinterlacing", "",
              SettingDef::Combo, "Progressive",
              {{"Progressive (Optimal)", "Progressive"}, {"Disabled", "Disabled"},
               {"Weave", "Weave"}, {"Blend", "Blend"}, {"Adaptive", "Adaptive"}},
              0, 0, 0, "", ""});
    // INI values must match what Settings::ParseDisplayAspectRatio /
    // GetDisplayAspectRatioName accept; the special strings "Auto (Game Native)",
    // "Stretch To Fill" and "PAR 1:1" are required verbatim (with the space).
    // The "Custom" option had no native equivalent and is dropped — DuckStation
    // exposes custom ratios via numerator/denominator widgets that are out of
    // scope for this audit. See audit 2026-04-06.
    s.append({"Graphics", "Rendering", "", "Display", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "Auto (Game Native)",
              {{"Auto (Game Native)", "Auto (Game Native)"}, {"Stretch To Fill", "Stretch To Fill"},
               {"4:3", "4:3"}, {"16:9", "16:9"}, {"19:9", "19:9"}, {"20:9", "20:9"},
               {"21:9", "21:9"}, {"16:10", "16:10"}, {"PAR 1:1", "PAR 1:1"}},
              0, 0, 0, "", ""});
    // INI values must match s_display_crop_mode_names from
    // references/duckstation-master/src/core/settings.cpp:1923-1925
    // (Borders, not AllBorders). See audit 2026-04-06.
    s.append({"Graphics", "Rendering", "", "Display", "CropMode", "Crop", "",
              SettingDef::Combo, "Overscan",
              {{"None", "None"},
               {"Only Overscan Area", "Overscan"},
               {"Only Overscan Area (Aspect Uncorrected)", "OverscanUncorrected"},
               {"All Borders", "Borders"},
               {"All Borders (Aspect Uncorrected)", "BordersUncorrected"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "Display", "Scaling", "Scaling", "Scaling filter applied to the final output.",
              SettingDef::Combo, "BilinearSmooth", scalingOptions, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "Display", "Scaling24Bit", "FMV Scaling", "Scaling filter applied during FMV playback.",
              SettingDef::Combo, "BilinearSmooth", scalingOptions, 0, 0, 0, "", ""});
    // Rendering bools
    s.append({"Graphics", "Rendering", "", "GPU", "PGXPEnable", "PGXP Geometry Correction", "Fixes polygon wobble by using a sub-pixel geometry buffer.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "PGXPDepthBuffer", "PGXP Depth Buffer (Low Compatibility)", "Uses a depth buffer for PGXP; may break some games.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "Display", "Force4_3For24Bit", "Force 4:3 For FMVs", "Switches to 4:3 aspect ratio during FMV sequences.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "ChromaSmoothing24Bit", "FMV Chroma Smoothing", "Applies chroma smoothing to FMV playback.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "WidescreenHack", "Widescreen Rendering", "Stretches 3D geometry to fill a widescreen display.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "ForceRoundTextureCoordinates", "Round Upscaled Texture Coordinates", "Reduces texture seams at high resolutions.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // ── Graphics / Advanced subcategory ──────────────────────────────────

    // Display Options group
    s.append({"Graphics", "Advanced", "Display Options", "Display", "Alignment", "Screen Alignment", "",
              SettingDef::Combo, "Center",
              {{"Left/Top", "LeftOrTop"}, {"Center", "Center"}, {"Right/Bottom", "RightOrBottom"}},
              0, 0, 0, "paired", ""});
    s.append({"Graphics", "Advanced", "Display Options", "Display", "Rotation", "", "",
              SettingDef::Combo, "Normal",
              {{"No Rotation", "Normal"}, {"90 Degrees", "Rotate90"},
               {"180 Degrees","Rotate180"}, {"270 Degrees","Rotate270"}},
              0, 0, 0, "paired", ""});
    s.append({"Graphics", "Advanced", "Display Options", "Display", "FineCropMode", "Fine Crop Mode", "",
              SettingDef::Combo, "None",
              {{"None", "None"}, {"Video Resolution", "VideoResolution"},
               {"Internal Resolution", "InternalResolution"}, {"Window Resolution", "WindowResolution"}},
              0, 0, 0, "", ""});
    { SettingDef d = {"Graphics", "Advanced", "Display Options", "Display", "FineCropLeft",   "Left",   "Fine Crop Size", SettingDef::Int, "0", {}, 0, 100, 1, "inline", "px"}; d.dependsOn = "FineCropMode"; s.append(d); }
    { SettingDef d = {"Graphics", "Advanced", "Display Options", "Display", "FineCropTop",    "Top",    "", SettingDef::Int, "0", {}, 0, 100, 1, "inline", "px"}; d.dependsOn = "FineCropMode"; s.append(d); }
    { SettingDef d = {"Graphics", "Advanced", "Display Options", "Display", "FineCropRight",  "Right",  "", SettingDef::Int, "0", {}, 0, 100, 1, "inline", "px"}; d.dependsOn = "FineCropMode"; s.append(d); }
    { SettingDef d = {"Graphics", "Advanced", "Display Options", "Display", "FineCropBottom", "Bottom", "", SettingDef::Int, "0", {}, 0, 100, 1, "inline", "px"}; d.dependsOn = "FineCropMode"; s.append(d); }
    s.append({"Graphics", "Advanced", "Display Options", "Display", "DisableMailboxPresentation", "Disable Mailbox Presentation", "Forces FIFO presentation instead of mailbox.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // Rendering Options group
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "Multisamples", "Multi-Sampling", "",
              SettingDef::Combo, "1",
              {{"Disabled", "1"}, {"2x MSAA", "2"}, {"4x MSAA", "4"}, {"8x MSAA", "8"}, {"16x MSAA", "16"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "LineDetectMode", "Line Detection", "",
              SettingDef::Combo, "Disabled",
              {{"Disabled",  "Disabled"}, {"Quads",     "Quads"},
               {"Basic Triangles", "BasicTriangles"}, {"Aggressive Triangles", "AggressiveTriangles"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "UseThread", "Threaded Rendering", "Renders frames on a separate thread.", SettingDef::Bool, "true", {}, 0, 0, 0, "paired", ""});
    { SettingDef d = {"Graphics", "Advanced", "Rendering Options", "GPU", "MaxQueuedFrames", "Max Queued Frames", "Maximum number of frames to queue for rendering.",
              SettingDef::Int, "2", {}, 1, 4, 1, "paired", ""}; d.dependsOn = "UseThread"; s.append(d); }
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "EnableModulationCrop", "Texture Modulation Cropping (\"Old/v0 GPU\")", "Uses legacy texture modulation cropping behaviour.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "ScaledInterlacing", "Scaled Interlacing", "Scales interlaced content to full resolution.", SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "UseSoftwareRendererForReadbacks", "Software Renderer Readbacks", "Uses the software renderer for GPU readbacks.", SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});


    // =========================================================================
    // On-Screen Display category
    // =========================================================================

    // Display group
    s.append({"On-Screen Display", "", "Display", "Display", "OSDScale",  "Display Scale",   "Scale factor for OSD text.",
              SettingDef::Int, "100", {}, 50, 500, 10, "", "%"});
    s.append({"On-Screen Display", "", "Display", "Display", "OSDMargin", "Display Margin", "Margin around the OSD in pixels.",
              SettingDef::Int, "10", {}, 0, 100, 1, "", "px"});

    // Messages group — checkboxes paired, durations paired, Display Location full-width
    s.append({"On-Screen Display", "", "Messages", "Display", "ShowOSDMessages",          "Show Messages",           "", SettingDef::Bool, "true",  {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Messages", "Display", "ShowStatusIndicators",     "Show Status Indicators",  "", SettingDef::Bool, "true",  {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Messages", "Display", "AnimateOSDMessages",       "Animate Messages",        "", SettingDef::Bool, "true",  {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Messages", "Display", "BlurOSDMessageBackgrounds","Blur Message Backgrounds","", SettingDef::Bool, "true",  {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDErrorDuration",         "Error Duration",         "How long error messages remain on screen.",
              SettingDef::Float, "15", {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDWarningDuration",       "Warning Duration",       "",
              SettingDef::Float, "10", {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDInfoDuration",          "Information Duration",   "",
              SettingDef::Float, "5",  {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDQuickDuration",         "Action Duration",        "",
              SettingDef::Float, "2.5",  {}, 0.0, 60.0, 0.5, "paired", "seconds"});
    s.append({"On-Screen Display", "", "Messages", "Display", "OSDMessageLocation", "Display Location", "",
              SettingDef::Combo, "TopLeft",
              {{"Top Left",      "TopLeft"},    {"Top Center",    "TopCenter"},    {"Top Right",      "TopRight"},
               {"Bottom Left",   "BottomLeft"}, {"Bottom Center",  "BottomCenter"}, {"Bottom Right",   "BottomRight"}},
              0, 0, 0, "", ""});

    // Overlays group — all paired 2-column layout
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowFPS",              "Show FPS",              "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowSpeed",            "Show Emulation Speed",  "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowCPU",              "Show CPU Usage",        "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowGPU",              "Show GPU Usage",        "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowResolution",       "Show Resolution",       "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowGPUStatistics",    "Show GPU Statistics",   "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowFrameTimes",       "Show Frame Times",      "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowLatencyStatistics","Show Latency Statistics","", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowInputs",           "Show Controller Input",  "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"On-Screen Display", "", "Overlays", "Display", "ShowEnhancements",     "Show Enhancements",      "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // =========================================================================
    // Audio category
    // =========================================================================

    // Controls group — sliders then paired mute checkboxes
    s.append({"Audio", "", "Controls", "Audio", "OutputVolume",      "Output Volume",       "Master output volume.",
              SettingDef::Int, "100", {}, 0, 100, 1, "slider", "%"});
    s.append({"Audio", "", "Controls", "Audio", "FastForwardVolume",  "Fast Forward Volume", "Volume during fast forward.",
              SettingDef::Int, "100", {}, 0, 100, 1, "slider", "%"});
    s.append({"Audio", "", "Controls", "Audio", "OutputMuted",  "Mute All Sound",  "Silences all audio output.", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Audio", "", "Controls", "CDROM", "MuteCDAudio",  "Mute CD Audio",   "Silences CD audio tracks only.", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // Configuration group — Backend + Driver paired, then full-width combos, then sliders
    s.append({"Audio", "", "Configuration", "Audio", "Backend", "Backend", "",
              SettingDef::Combo, "Cubeb",
              {{"Cubeb", "Cubeb"}, {"SDL", "SDL"}, {"Null", "Null"}},
              0, 0, 0, "paired", ""});
    // TODO(audit-tier-4): Driver/Output Device should be enumerated at runtime
    // from Cubeb's GetCubebDriverNames() and AudioStream::GetOutputDevices().
    // Hard-coded options are functionally inert. Deferred until a shared
    // mechanism is designed across all three adapters.
    // The "Default" INI value must be empty string ("") — any non-empty value
    // is passed through to Cubeb as a driver-name lookup and fails. Audit 2026-04-06.
    s.append({"Audio", "", "Configuration", "Audio", "Driver", "Driver", "",
              SettingDef::Combo, "",
              {{"Default", ""}},
              0, 0, 0, "paired", ""});
    s.append({"Audio", "", "Configuration", "Audio", "OutputDevice", "Output Device", "",
              SettingDef::Combo, "",
              {{"Default", ""}},
              0, 0, 0, "", ""});
    s.append({"Audio", "", "Configuration", "Audio", "StretchMode", "Stretch Mode", "",
              SettingDef::Combo, "TimeStretch",
              {{"Time Stretch (Tempo Change, Best Sound)", "TimeStretch"},
               {"Resample (Pitch Change, Fastest)", "Resample"},
               {"None (Audio Glitches Are Fun)", "None"}},
              0, 0, 0, "", ""});
    s.append({"Audio", "", "Configuration", "Audio", "BufferMS", "Buffer Size", "Audio buffer size; lower values reduce latency.",
              SettingDef::Int, "50", {}, 10, 500, 5, "slider", "ms"});
    s.append({"Audio", "", "Configuration", "Audio", "OutputLatencyMS", "Output Latency", "Additional output latency.",
              SettingDef::Int, "20", {}, 1, 500, 1, "slider", "ms"});

    // Time Stretching group — sliders then paired checkboxes
    s.append({"Audio", "", "Time Stretching", "Audio", "StretchSequenceLengthMS", "Sequence Length", "SoundTouch sequence length.",
              SettingDef::Int, "30", {}, 10, 500, 1, "slider", "ms"});
    s.append({"Audio", "", "Time Stretching", "Audio", "StretchSeekWindowMS", "Seek Window", "SoundTouch seek window size.",
              SettingDef::Int, "20", {}, 10, 500, 1, "slider", "ms"});
    s.append({"Audio", "", "Time Stretching", "Audio", "StretchOverlapMS", "Overlap", "SoundTouch overlap duration.",
              SettingDef::Int, "10", {}, 1, 100, 1, "slider", "ms"});
    s.append({"Audio", "", "Time Stretching", "Audio", "StretchUseQuickSeek",    "Use Quick Seek",            "Enables SoundTouch quick seek mode (lower quality, less CPU).", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Audio", "", "Time Stretching", "Audio", "StretchUseAAFilter",     "Use Anti-Aliasing Filter",  "Enables SoundTouch anti-aliasing filter.", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // =========================================================================
    // Memory Cards category
    // =========================================================================

    // Memory Card 1 group
    s.append({"Memory Cards", "", "Memory Card 1", "MemoryCards", "Card1Type", "Memory Card Type", "",
              SettingDef::Combo, "PerGameTitle", memCardTypes, 0, 0, 0, "", ""});
    s.append({"Memory Cards", "", "Memory Card 1", "MemoryCards", "Card1Path", "Shared Memory Card Path", "",
              SettingDef::String, "", {}, 0, 0, 0, "readonly", ""});

    // Memory Card 2 group
    s.append({"Memory Cards", "", "Memory Card 2", "MemoryCards", "Card2Type", "Memory Card Type", "",
              SettingDef::Combo, "None", memCardTypes, 0, 0, 0, "", ""});
    s.append({"Memory Cards", "", "Memory Card 2", "MemoryCards", "Card2Path", "Shared Memory Card Path", "",
              SettingDef::String, "", {}, 0, 0, 0, "readonly", ""});

    // Game-Specific Card Settings group
    s.append({"Memory Cards", "", "Game-Specific Card Settings", "MemoryCards", "UsePlaylistTitle", "Use Single Card For Multi-Disc Games", "",
              SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});

    return s;
}

// ============================================================================
// Portable directory — inside .app bundle on macOS, emulators dir otherwise
// ============================================================================

QString DuckStationAdapter::portableDir() {
#if defined(Q_OS_MACOS)
    // DuckStation checks for portable.txt next to its binary inside the bundle.
    // This works as long as we launch DuckStation via direct exec rather than
    // Launch Services / `open` — the latter applies app translocation that
    // blocks rename() inside the bundle.
    QDir installDir(Paths::emulatorsDir(DUCKSTATION_INSTALL_FOLDER));
    const auto entries = installDir.entryList({"*.app"}, QDir::Dirs);
    for (const auto& entry : entries) {
        QString macosDir = installDir.absoluteFilePath(entry) + "/Contents/MacOS";
        if (QDir(macosDir).exists())
            return macosDir;
    }
    return Paths::emulatorsDir(DUCKSTATION_INSTALL_FOLDER);
#else
    return Paths::emulatorsDir(DUCKSTATION_INSTALL_FOLDER);
#endif
}

// ============================================================================
// ensureConfig — create or patch settings.ini before launch
// ============================================================================

bool DuckStationAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                      const QString& biosPath,
                                      const QString& savesPath) {
    const QString pDir = portableDir();

    // Enable portable mode — DuckStation checks for portable.txt next to
    // the binary (inside Contents/MacOS/ on macOS).
    if (!ensurePortableMarker(pDir, "DuckStation"))
        return false;

#if defined(Q_OS_MACOS)
    // Clean up the short-lived attempt to put portable.txt next to the bundle
    // and the previous attempt to use ~/Library/Application Support.
    QFile::remove(Paths::emulatorsDir(DUCKSTATION_INSTALL_FOLDER) + "/portable.txt");
    QFile::remove(Paths::emulatorsDir(DUCKSTATION_INSTALL_FOLDER) + "/settings.ini");
#endif

    const QString path = pDir + "/settings.ini";

    if (QFileInfo::exists(path))
        return patchExistingConfig(path, biosPath, savesPath);
    return createDefaultConfig(path, biosPath, savesPath);
}

// ============================================================================
// resolveExecutable — platform-aware executable resolution
// ============================================================================

QString DuckStationAdapter::resolveExecutable(const EmulatorManifest& manifest,
                                              const QString& installPath) {
    return resolveExecutableInDir(manifest, installPath, "DuckStation");
}

// ============================================================================
// createDefaultConfig — write only embedding-critical keys
// The emulator will fill in its own defaults for everything else on first
// launch.  This prevents our config from going stale when the emulator
// renames or removes INI keys in a future update.
// ============================================================================

bool DuckStationAdapter::createDefaultConfig(const QString& path,
                                              const QString& biosPath,
                                              const QString& savesPath) {

    // savesPath is this emulator's unified data root for its system,
    // i.e. {root}/emulators/duckstation/psx/. Every managed subfolder
    // lives directly under it — see EmulatorService::ensureConfig().
    const QString& dataRoot = savesPath;
    const QString memcardsPath    = dataRoot + "/memcards";
    const QString savestatesPath  = dataRoot + "/savestates";
    const QString screenshotsPath = dataRoot + "/screenshots";
    const QString cachePath       = dataRoot + "/cache";
    const QString cheatsPath      = dataRoot + "/cheats";
    const QString texturesPath    = dataRoot + "/textures";

    // Only write keys required for embedding (wizard suppression, fullscreen,
    // managed paths, controller type).  All other settings are left to the
    // emulator's own defaults so they stay in sync across updates.
    QStringList lines = {
        "[Main]",
        "SetupWizardComplete = true",
        "SetupWizardIncomplete = false",
        "ConfirmPowerOff = false",
        "StartFullscreen = true",
        "PauseOnFocusLoss = true",
        "SaveStateOnExit = true",
        "",
        "[BIOS]",
        "SearchDirectory = " + biosPath,
        "",
        "[Display]",
        "Fullscreen = true",
        "",
        "[MemoryCards]",
        "Directory = " + memcardsPath,
        "Card1Path = " + memcardsPath + "/shared_card_1.mcd",
        "Card2Path = " + memcardsPath + "/shared_card_2.mcd",
        "",
        "[Folders]",
        "SaveStates = " + savestatesPath,
        "Screenshots = " + screenshotsPath,
        "Cache = " + cachePath,
        "Cheats = " + cheatsPath,
        "Textures = " + texturesPath,
        "",
        "[Hotkeys]",
        "OpenPauseMenu =",
        "TogglePause =",
        "ToggleFullscreen =",
        "",
        "[Controller1]",
        "",
    };

    return writeConfigFile(path, lines.join("\n"), "DuckStation");
}

// ============================================================================
// patchExistingConfig — ensure required settings in an existing config
// ============================================================================

bool DuckStationAdapter::patchExistingConfig(const QString& path,
                                              const QString& biosPath,
                                              const QString& savesPath) {
    QString content;
    if (!readConfigFile(path, content, "DuckStation"))
        return false;

    bool changed = suppressSetupWizard(content, "Main");

    // Ensure folder paths, in-game menu behaviour, and neutered hotkeys
    // in a single patch pass. patchIniKeys injects missing keys/sections.
    //
    // savesPath is this emulator's unified data root
    // ({root}/emulators/duckstation/psx/) — every subfolder lives directly under it.
    const QString& dataRoot = savesPath;
    QVector<IniKeyPatch> patches = {
        {"BIOS",        "SearchDirectory", biosPath},
        {"MemoryCards", "Directory",       dataRoot + "/memcards"},
        {"Folders",     "SaveStates",      dataRoot + "/savestates"},
        {"Folders",     "Screenshots",     dataRoot + "/screenshots"},
        {"Folders",     "Cache",           dataRoot + "/cache"},
        {"Folders",     "Cheats",          dataRoot + "/cheats"},
        {"Folders",     "Textures",        dataRoot + "/textures"},

        {"Main", "PauseOnFocusLoss", "true"},
        {"Main", "SaveStateOnExit",  "true"},

        {"Hotkeys", "OpenPauseMenu",    ""},
        {"Hotkeys", "TogglePause",     ""},
        {"Hotkeys", "ToggleFullscreen", ""},
    };
    if (patchIniKeys(content, patches))
        changed = true;

    // Ensure [Controller1] section exists for controller bindings
    if (!content.contains("[Controller1]")) {
        content.append("\n[Controller1]\n");
        changed = true;
    }

    if (changed && !writeConfigFile(path, content, "DuckStation"))
        return false;
    return true;
}

// ============================================================================
// BIOS files
// ============================================================================

QVector<PathDef> DuckStationAdapter::pathsDefs() const {
    return {
        {"BIOS",         "BIOS",        "SearchDirectory", "",            PathBase::Bios},
        {"Memory Cards", "MemoryCards", "Directory",       "memcards",    PathBase::EmulatorData},
        {"Save States",  "Folders",     "SaveStates",      "savestates",  PathBase::EmulatorData},
        {"Screenshots",  "Folders",     "Screenshots",     "screenshots", PathBase::EmulatorData},
        {"Cache",        "Folders",     "Cache",           "cache",       PathBase::EmulatorData},
        {"Cheats",       "Folders",     "Cheats",          "cheats",      PathBase::EmulatorData},
        {"Textures",     "Folders",     "Textures",        "textures",    PathBase::EmulatorData},
    };
}

AspectRatioOptions DuckStationAdapter::aspectRatioOptions() const {
    return {{
        {"4:3", {
            {"Display", "AspectRatio", "4:3"},
            {"GPU", "WidescreenHack", "false"},
        }},
        {"16:9", {
            {"Display", "AspectRatio", "16:9"},
            {"GPU", "WidescreenHack", "true"},
        }},
    }, "4:3"};
}

ResolutionOptions DuckStationAdapter::resolutionOptions() const {
    return {"GPU", "ResolutionScale",
            {{"720P", "2"}, {"1080P", "3"}, {"1440P", "4"}, {"4K", "6"}}, "2"};
}

QVector<BiosDef> DuckStationAdapter::biosFiles() const {
    return {
        {"scph5501.bin", "PS1 BIOS (North America)", true, ""},
        {"scph5500.bin", "PS1 BIOS (Japan)", false, ""},
        {"scph5502.bin", "PS1 BIOS (Europe)", false, ""},
        {"scph1001.bin", "PS1 BIOS (North America, v2.2)", false, ""},
        {"scph7001.bin", "PS1 BIOS (North America, v4.1)", false, ""},
        {"scph101.bin",  "PSone BIOS (North America, v4.5)", false, ""},
    };
}

// ============================================================================
// Controller bindings
// ============================================================================

QString DuckStationAdapter::formatBinding(int deviceIndex, const QString& element,
                                           bool isAxis, bool positive) const {
    // DuckStation: buttons use "SDL-0/A" (no prefix), full axes use
    // "SDL-0/LeftX" (no polarity), triggers use "SDL-0/+LeftTrigger".
    // Only trigger axes get the polarity prefix.
    Q_UNUSED(positive);
    if (isAxis && element.contains("Trigger"))
        return QString("SDL-%1/+%2").arg(deviceIndex).arg(element);
    return QString("SDL-%1/%2").arg(deviceIndex).arg(element);
}

QStringList DuckStationAdapter::resumeLaunchArgs(const QString& /*stateFilePath*/) const {
    // DuckStation uses -resume to auto-load the game's resume state (no explicit path needed)
    return {"-resume"};
}

// ============================================================================
// Controller types
// ============================================================================

QVector<ControllerTypeDef> DuckStationAdapter::controllerTypes() const {
    return {
        {"None",              "Not Connected",    ""},
        {"DigitalController", "Digital Controller",":/AppUI/qml/AppUI/images/controllers/digital_controller.svg"},
        {"AnalogController",  "Analog Controller", ":/AppUI/qml/AppUI/images/controllers/analog_controller.svg"},
        {"AnalogJoystick",    "Analog Joystick",   ":/AppUI/qml/AppUI/images/controllers/analog_joystick.svg"},
        {"NeGcon",            "NeGcon",            ":/AppUI/qml/AppUI/images/controllers/ds_negcon.svg"},
        {"NeGconRumble",      "NeGcon (Rumble)",   ":/AppUI/qml/AppUI/images/controllers/ds_negcon.svg"},
        {"JogCon",            "JogCon",            ":/AppUI/qml/AppUI/images/controllers/Jogcon.svg"},
        {"PopnController",    "Pop'n Controller",  ":/AppUI/qml/AppUI/images/controllers/Popn.svg"},
    };
}

QVector<BindingDef> DuckStationAdapter::controllerBindingDefs() const {
    return {
        // D-Pad
        {BindingDef::Button, "Up",       "D-Pad",        "Controller1", "Up",    "SDL-0/DPadUp"},
        {BindingDef::Button, "Down",     "D-Pad",        "Controller1", "Down",  "SDL-0/DPadDown"},
        {BindingDef::Button, "Left",     "D-Pad",        "Controller1", "Left",  "SDL-0/DPadLeft"},
        {BindingDef::Button, "Right",    "D-Pad",        "Controller1", "Right", "SDL-0/DPadRight"},
        // Face Buttons
        {BindingDef::Button, "Cross",    "Face Buttons",  "Controller1", "Cross",    "SDL-0/A"},
        {BindingDef::Button, "Circle",   "Face Buttons",  "Controller1", "Circle",   "SDL-0/B"},
        {BindingDef::Button, "Square",   "Face Buttons",  "Controller1", "Square",   "SDL-0/X"},
        {BindingDef::Button, "Triangle", "Face Buttons",  "Controller1", "Triangle", "SDL-0/Y"},
        // Shoulders
        {BindingDef::Button, "L1", "Shoulders", "Controller1", "L1", "SDL-0/LeftShoulder"},
        {BindingDef::Button, "R1", "Shoulders", "Controller1", "R1", "SDL-0/RightShoulder"},
        // Triggers
        {BindingDef::Axis,   "L2", "Triggers", "Controller1", "L2", "SDL-0/+LeftTrigger"},
        {BindingDef::Axis,   "R2", "Triggers", "Controller1", "R2", "SDL-0/+RightTrigger"},
        // Stick Buttons
        {BindingDef::Button, "L3", "Stick Buttons", "Controller1", "L3", "SDL-0/LeftStick"},
        {BindingDef::Button, "R3", "Stick Buttons", "Controller1", "R3", "SDL-0/RightStick"},
        // Left Stick (half-axes)
        {BindingDef::Axis, "Left Stick Left",  "Left Stick",  "Controller1", "LLeft",  "SDL-0/LeftX"},
        {BindingDef::Axis, "Left Stick Right", "Left Stick",  "Controller1", "LRight", "SDL-0/LeftX"},
        {BindingDef::Axis, "Left Stick Up",    "Left Stick",  "Controller1", "LUp",    "SDL-0/LeftY"},
        {BindingDef::Axis, "Left Stick Down",  "Left Stick",  "Controller1", "LDown",  "SDL-0/LeftY"},
        // Right Stick (half-axes)
        {BindingDef::Axis, "Right Stick Left",  "Right Stick", "Controller1", "RLeft",  "SDL-0/RightX"},
        {BindingDef::Axis, "Right Stick Right", "Right Stick", "Controller1", "RRight", "SDL-0/RightX"},
        {BindingDef::Axis, "Right Stick Up",    "Right Stick", "Controller1", "RUp",    "SDL-0/RightY"},
        {BindingDef::Axis, "Right Stick Down",  "Right Stick", "Controller1", "RDown",  "SDL-0/RightY"},
        // System
        {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
        {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
        // Toggle
        {BindingDef::Button, "Analog", "System", "Controller1", "Analog", ""},
        // Motors
        {BindingDef::Axis,   "LargeMotor", "Motors", "Controller1", "LargeMotor", ""},
        {BindingDef::Axis,   "SmallMotor", "Motors", "Controller1", "SmallMotor", ""},
    };
}

QVector<BindingDef> DuckStationAdapter::controllerBindingDefsForType(const QString& type) const {
    if (type == "AnalogController")
        return controllerBindingDefs(); // default layout is AnalogController

    if (type == "DigitalController") {
        return {
            {BindingDef::Button, "Up",       "D-Pad",        "Controller1", "Up",       "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",     "D-Pad",        "Controller1", "Down",     "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",     "D-Pad",        "Controller1", "Left",     "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right",    "D-Pad",        "Controller1", "Right",    "SDL-0/DPadRight"},
            {BindingDef::Button, "Cross",    "Face Buttons",  "Controller1", "Cross",    "SDL-0/A"},
            {BindingDef::Button, "Circle",   "Face Buttons",  "Controller1", "Circle",   "SDL-0/B"},
            {BindingDef::Button, "Square",   "Face Buttons",  "Controller1", "Square",   "SDL-0/X"},
            {BindingDef::Button, "Triangle", "Face Buttons",  "Controller1", "Triangle", "SDL-0/Y"},
            {BindingDef::Button, "L1", "Shoulders", "Controller1", "L1", "SDL-0/LeftShoulder"},
            {BindingDef::Button, "R1", "Shoulders", "Controller1", "R1", "SDL-0/RightShoulder"},
            {BindingDef::Button, "L2", "Triggers",  "Controller1", "L2", "SDL-0/+LeftTrigger"},
            {BindingDef::Button, "R2", "Triggers",  "Controller1", "R2", "SDL-0/+RightTrigger"},
            {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
            {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
        };
    }

    if (type == "AnalogJoystick") {
        return {
            {BindingDef::Button, "Up",       "D-Pad",        "Controller1", "Up",       "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",     "D-Pad",        "Controller1", "Down",     "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",     "D-Pad",        "Controller1", "Left",     "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right",    "D-Pad",        "Controller1", "Right",    "SDL-0/DPadRight"},
            {BindingDef::Button, "Cross",    "Face Buttons",  "Controller1", "Cross",    "SDL-0/A"},
            {BindingDef::Button, "Circle",   "Face Buttons",  "Controller1", "Circle",   "SDL-0/B"},
            {BindingDef::Button, "Square",   "Face Buttons",  "Controller1", "Square",   "SDL-0/X"},
            {BindingDef::Button, "Triangle", "Face Buttons",  "Controller1", "Triangle", "SDL-0/Y"},
            {BindingDef::Button, "L1", "Shoulders", "Controller1", "L1", "SDL-0/LeftShoulder"},
            {BindingDef::Button, "R1", "Shoulders", "Controller1", "R1", "SDL-0/RightShoulder"},
            {BindingDef::Axis,   "L2", "Triggers",  "Controller1", "L2", "SDL-0/+LeftTrigger"},
            {BindingDef::Axis,   "R2", "Triggers",  "Controller1", "R2", "SDL-0/+RightTrigger"},
            {BindingDef::Button, "L3", "Stick Buttons", "Controller1", "L3", "SDL-0/LeftStick"},
            {BindingDef::Button, "R3", "Stick Buttons", "Controller1", "R3", "SDL-0/RightStick"},
            {BindingDef::Axis, "Left Stick Left",  "Left Stick",  "Controller1", "LLeft",  "SDL-0/LeftX"},
            {BindingDef::Axis, "Left Stick Right", "Left Stick",  "Controller1", "LRight", "SDL-0/LeftX"},
            {BindingDef::Axis, "Left Stick Up",    "Left Stick",  "Controller1", "LUp",    "SDL-0/LeftY"},
            {BindingDef::Axis, "Left Stick Down",  "Left Stick",  "Controller1", "LDown",  "SDL-0/LeftY"},
            {BindingDef::Axis, "Right Stick Left",  "Right Stick", "Controller1", "RLeft",  "SDL-0/RightX"},
            {BindingDef::Axis, "Right Stick Right", "Right Stick", "Controller1", "RRight", "SDL-0/RightX"},
            {BindingDef::Axis, "Right Stick Up",    "Right Stick", "Controller1", "RUp",    "SDL-0/RightY"},
            {BindingDef::Axis, "Right Stick Down",  "Right Stick", "Controller1", "RDown",  "SDL-0/RightY"},
            {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
            {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
            {BindingDef::Button, "Mode",   "System", "Controller1", "Mode",   ""},
        };
    }

    if (type == "NeGcon") {
        return {
            {BindingDef::Button, "Up",    "D-Pad",   "Controller1", "Up",    "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",  "D-Pad",   "Controller1", "Down",  "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",  "D-Pad",   "Controller1", "Left",  "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right", "D-Pad",   "Controller1", "Right", "SDL-0/DPadRight"},
            {BindingDef::Button, "A",     "Buttons",  "Controller1", "A",     "SDL-0/A"},
            {BindingDef::Button, "B",     "Buttons",  "Controller1", "B",     "SDL-0/B"},
            {BindingDef::Button, "R",     "Buttons",  "Controller1", "R",     "SDL-0/RightShoulder"},
            {BindingDef::Button, "Start", "System",   "Controller1", "Start", "SDL-0/Start"},
            {BindingDef::Axis,   "Steering Left",  "Steering", "Controller1", "SteeringLeft",  "SDL-0/LeftX"},
            {BindingDef::Axis,   "Steering Right", "Steering", "Controller1", "SteeringRight", "SDL-0/LeftX"},
            {BindingDef::Axis,   "I",  "Analog Buttons", "Controller1", "I",  "SDL-0/+RightTrigger"},
            {BindingDef::Axis,   "II", "Analog Buttons", "Controller1", "II", "SDL-0/+LeftTrigger"},
            {BindingDef::Axis,   "L",  "Analog Buttons", "Controller1", "L",  "SDL-0/LeftShoulder"},
        };
    }

    if (type == "NeGconRumble") {
        return {
            {BindingDef::Button, "Up",    "D-Pad",   "Controller1", "Up",    "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",  "D-Pad",   "Controller1", "Down",  "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",  "D-Pad",   "Controller1", "Left",  "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right", "D-Pad",   "Controller1", "Right", "SDL-0/DPadRight"},
            {BindingDef::Button, "A",     "Buttons",  "Controller1", "A",     "SDL-0/A"},
            {BindingDef::Button, "B",     "Buttons",  "Controller1", "B",     "SDL-0/B"},
            {BindingDef::Button, "R",     "Buttons",  "Controller1", "R",     "SDL-0/RightShoulder"},
            {BindingDef::Button, "Start", "System",   "Controller1", "Start", "SDL-0/Start"},
            {BindingDef::Button, "Analog","System",   "Controller1", "Analog",""},
            {BindingDef::Axis,   "Steering Left",  "Steering", "Controller1", "SteeringLeft",  "SDL-0/LeftX"},
            {BindingDef::Axis,   "Steering Right", "Steering", "Controller1", "SteeringRight", "SDL-0/LeftX"},
            {BindingDef::Axis,   "I",  "Analog Buttons", "Controller1", "I",  "SDL-0/+RightTrigger"},
            {BindingDef::Axis,   "II", "Analog Buttons", "Controller1", "II", "SDL-0/+LeftTrigger"},
            {BindingDef::Axis,   "L",  "Analog Buttons", "Controller1", "L",  "SDL-0/LeftShoulder"},
            {BindingDef::Axis,   "LargeMotor", "Motors", "Controller1", "LargeMotor", ""},
            {BindingDef::Axis,   "SmallMotor", "Motors", "Controller1", "SmallMotor", ""},
        };
    }

    if (type == "JogCon") {
        return {
            {BindingDef::Button, "Up",       "D-Pad",        "Controller1", "Up",       "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",     "D-Pad",        "Controller1", "Down",     "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",     "D-Pad",        "Controller1", "Left",     "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right",    "D-Pad",        "Controller1", "Right",    "SDL-0/DPadRight"},
            {BindingDef::Button, "Cross",    "Face Buttons",  "Controller1", "Cross",    "SDL-0/A"},
            {BindingDef::Button, "Circle",   "Face Buttons",  "Controller1", "Circle",   "SDL-0/B"},
            {BindingDef::Button, "Square",   "Face Buttons",  "Controller1", "Square",   "SDL-0/X"},
            {BindingDef::Button, "Triangle", "Face Buttons",  "Controller1", "Triangle", "SDL-0/Y"},
            {BindingDef::Button, "L1", "Shoulders", "Controller1", "L1", "SDL-0/LeftShoulder"},
            {BindingDef::Button, "R1", "Shoulders", "Controller1", "R1", "SDL-0/RightShoulder"},
            {BindingDef::Axis,   "L2", "Triggers",  "Controller1", "L2", "SDL-0/+LeftTrigger"},
            {BindingDef::Axis,   "R2", "Triggers",  "Controller1", "R2", "SDL-0/+RightTrigger"},
            {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
            {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
            {BindingDef::Button, "Mode",   "System", "Controller1", "Mode",   ""},
            {BindingDef::Axis,   "Steering Left",  "Steering", "Controller1", "SteeringLeft",  "SDL-0/LeftX"},
            {BindingDef::Axis,   "Steering Right", "Steering", "Controller1", "SteeringRight", "SDL-0/LeftX"},
            {BindingDef::Axis,   "Motor",          "Motors",   "Controller1", "Motor",          ""},
        };
    }

    if (type == "PopnController") {
        return {
            {BindingDef::Button, "Left White",   "Buttons", "Controller1", "LeftWhite",    ""},
            {BindingDef::Button, "Left Yellow",  "Buttons", "Controller1", "LeftYellow",   ""},
            {BindingDef::Button, "Left Green",   "Buttons", "Controller1", "LeftGreen",    ""},
            {BindingDef::Button, "Left Blue",    "Buttons", "Controller1", "LeftBlue",     ""},
            {BindingDef::Button, "Middle Red",   "Buttons", "Controller1", "MiddleRed",    ""},
            {BindingDef::Button, "Right Blue",   "Buttons", "Controller1", "RightBlue",    ""},
            {BindingDef::Button, "Right Green",  "Buttons", "Controller1", "RightGreen",   ""},
            {BindingDef::Button, "Right Yellow", "Buttons", "Controller1", "RightYellow",  ""},
            {BindingDef::Button, "Right White",  "Buttons", "Controller1", "RightWhite",   ""},
            {BindingDef::Button, "Select", "System", "Controller1", "Select", "SDL-0/Back"},
            {BindingDef::Button, "Start",  "System", "Controller1", "Start",  "SDL-0/Start"},
        };
    }

    // None or unknown
    return {};
}

QVector<SettingDef> DuckStationAdapter::controllerSettingDefsForType(const QString& type) const {
    const QVector<QPair<QString,QString>> invertOpts = {
        {"None", "0"}, {"Horizontal Flip", "1"}, {"Vertical Flip", "2"}, {"Both", "3"},
    };

    if (type == "AnalogController") {
        return {
            {"", "", "", "Controller1", "ForceAnalogOnReset",
             "Force Analog On Reset", "Automatically enables analog mode on reset.",
             SettingDef::Bool, "true", {}, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "AnalogDPadInDigitalMode",
             "Use Analog Sticks for D-Pad in Digital Mode", "",
             SettingDef::Bool, "true", {}, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "AnalogDeadzone",
             "Analog Deadzone", "",
             SettingDef::Float, "0", {}, 0.0, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "AnalogSensitivity",
             "Analog Sensitivity", "",
             SettingDef::Float, "1.33", {}, 0.01, 2.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "ButtonDeadzone",
             "Button/Trigger Deadzone", "",
             SettingDef::Float, "0.25", {}, 0.01, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "InvertLeftStick",
             "Invert Left Stick", "",
             SettingDef::Combo, "0", invertOpts, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "InvertRightStick",
             "Invert Right Stick", "",
             SettingDef::Combo, "0", invertOpts, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "LargeMotorVibrationBias",
             "Large Motor Vibration Bias", "",
             SettingDef::Int, "8", {}, -255, 255, 1, "slider", ""},
            {"", "", "", "Controller1", "SmallMotorVibrationBias",
             "Small Motor Vibration Bias", "",
             SettingDef::Int, "8", {}, -255, 255, 1, "slider", ""},
        };
    }

    if (type == "AnalogJoystick") {
        return {
            {"", "", "", "Controller1", "AnalogDeadzone",
             "Analog Deadzone", "",
             SettingDef::Float, "0", {}, 0.0, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "AnalogSensitivity",
             "Analog Sensitivity", "",
             SettingDef::Float, "1.33", {}, 0.01, 2.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "InvertLeftStick",
             "Invert Left Stick", "",
             SettingDef::Combo, "0", invertOpts, 0, 0, 0, "", ""},
            {"", "", "", "Controller1", "InvertRightStick",
             "Invert Right Stick", "",
             SettingDef::Combo, "0", invertOpts, 0, 0, 0, "", ""},
        };
    }

    if (type == "NeGcon") {
        return {
            {"", "", "Steering", "Controller1", "SteeringDeadzone",    "Deadzone",    "", SettingDef::Float, "0",   {}, 0.0,   0.99, 0.01, "slider", ""},
            {"", "", "Steering", "Controller1", "SteeringSaturation",  "Saturation",  "", SettingDef::Float, "1",   {}, 0.01,  1.0,  0.01, "slider", ""},
            {"", "", "Steering", "Controller1", "SteeringLinearity",   "Linearity",   "", SettingDef::Float, "0",   {}, -2.0,  2.0,  0.01, "slider", ""},
            {"", "", "Steering", "Controller1", "SteeringScaling",     "Scaling",     "", SettingDef::Float, "1",   {}, 0.01, 10.0,  0.01, "slider", ""},
            {"", "", "I Button", "Controller1", "IDeadzone",     "Deadzone",    "", SettingDef::Float, "0",   {}, 0.0,   0.99, 0.01, "slider", ""},
            {"", "", "I Button", "Controller1", "ISaturation",   "Saturation",  "", SettingDef::Float, "1",   {}, 0.01,  1.0,  0.01, "slider", ""},
            {"", "", "I Button", "Controller1", "ILinearity",    "Linearity",   "", SettingDef::Float, "0",   {}, -2.0,  2.0,  0.01, "slider", ""},
            {"", "", "I Button", "Controller1", "IScaling",      "Scaling",     "", SettingDef::Float, "1",   {}, 0.01, 10.0,  0.01, "slider", ""},
            {"", "", "II Button","Controller1", "IIDeadzone",    "Deadzone",    "", SettingDef::Float, "0",   {}, 0.0,   0.99, 0.01, "slider", ""},
            {"", "", "II Button","Controller1", "IISaturation",  "Saturation",  "", SettingDef::Float, "1",   {}, 0.01,  1.0,  0.01, "slider", ""},
            {"", "", "II Button","Controller1", "IILinearity",   "Linearity",   "", SettingDef::Float, "0",   {}, -2.0,  2.0,  0.01, "slider", ""},
            {"", "", "II Button","Controller1", "IIScaling",     "Scaling",     "", SettingDef::Float, "1",   {}, 0.01, 10.0,  0.01, "slider", ""},
            {"", "", "L Trigger","Controller1", "LDeadzone",     "Deadzone",    "", SettingDef::Float, "0",   {}, 0.0,   0.99, 0.01, "slider", ""},
            {"", "", "L Trigger","Controller1", "LSaturation",   "Saturation",  "", SettingDef::Float, "1",   {}, 0.01,  1.0,  0.01, "slider", ""},
            {"", "", "L Trigger","Controller1", "LLinearity",    "Linearity",   "", SettingDef::Float, "0",   {}, -2.0,  2.0,  0.01, "slider", ""},
            {"", "", "L Trigger","Controller1", "LScaling",      "Scaling",     "", SettingDef::Float, "1",   {}, 0.01, 10.0,  0.01, "slider", ""},
        };
    }

    if (type == "NeGconRumble") {
        return {
            {"", "", "", "Controller1", "SteeringDeadzone",
             "Steering Deadzone", "",
             SettingDef::Float, "0", {}, 0.0, 0.99, 0.01, "slider", ""},
            {"", "", "", "Controller1", "SteeringSensitivity",
             "Steering Sensitivity", "",
             SettingDef::Float, "1", {}, 0.01, 2.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "LargeMotorVibrationBias",
             "Large Motor Vibration Bias", "",
             SettingDef::Int, "8", {}, -255, 255, 1, "slider", ""},
            {"", "", "", "Controller1", "SmallMotorVibrationBias",
             "Small Motor Vibration Bias", "",
             SettingDef::Int, "8", {}, -255, 255, 1, "slider", ""},
        };
    }

    if (type == "JogCon") {
        return {
            {"", "", "", "Controller1", "AnalogDeadzone",
             "Analog Deadzone", "",
             SettingDef::Float, "0", {}, 0.0, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "AnalogSensitivity",
             "Analog Sensitivity", "",
             SettingDef::Float, "1.33", {}, 0.01, 2.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "ButtonDeadzone",
             "Button Deadzone", "",
             SettingDef::Float, "0.25", {}, 0.01, 1.0, 0.01, "slider", ""},
            {"", "", "", "Controller1", "SteeringHoldDeadzone",
             "Steering Hold Deadzone", "",
             SettingDef::Float, "0.03", {}, 0.01, 1.0, 0.01, "slider", ""},
        };
    }

    // DigitalController, PopnController, None — no settings
    return {};
}

QVector<HotkeyDef> DuckStationAdapter::hotkeyBindingDefs() const {
    return {
        // Interface
        {"Screenshot",              "Interface",   "Hotkeys", "Screenshot",              "Keyboard/F10"},

        // System
        {"Fast Forward (Hold)",     "System",      "Hotkeys", "FastForward",             "Keyboard/Tab"},
        {"Fast Forward (Toggle)",   "System",      "Hotkeys", "ToggleFastForward",       ""},
        {"Turbo (Hold)",            "System",      "Hotkeys", "Turbo",                   ""},
        {"Turbo (Toggle)",          "System",      "Hotkeys", "ToggleTurbo",             ""},
        {"Frame Step",              "System",      "Hotkeys", "FrameStep",               ""},
        {"Rewind",                  "System",      "Hotkeys", "Rewind",                  ""},
        {"Increase Emulation Speed","System",      "Hotkeys", "IncreaseEmulationSpeed",  ""},
        {"Decrease Emulation Speed","System",      "Hotkeys", "DecreaseEmulationSpeed",  ""},
        {"Reset Emulation Speed",   "System",      "Hotkeys", "ResetEmulationSpeed",     ""},
        {"Power Off",               "System",      "Hotkeys", "PowerOff",                ""},
        {"Reset",                   "System",      "Hotkeys", "Reset",                   ""},
        {"Change Disc",             "System",      "Hotkeys", "ChangeDisc",              ""},
        {"Previous Disc",           "System",      "Hotkeys", "SwitchToPreviousDisc",    ""},
        {"Next Disc",               "System",      "Hotkeys", "SwitchToNextDisc",        ""},
        {"Swap Memory Cards",       "System",      "Hotkeys", "SwapMemoryCards",         ""},

        // Audio
        {"Mute Audio",              "Audio",       "Hotkeys", "AudioMute",               ""},
        {"Mute CD Audio",           "Audio",       "Hotkeys", "AudioCDAudioMute",        ""},
        {"Volume Up",               "Audio",       "Hotkeys", "AudioVolumeUp",           ""},
        {"Volume Down",             "Audio",       "Hotkeys", "AudioVolumeDown",         ""},

        // Graphics
        {"Toggle Software Rendering","Graphics",   "Hotkeys", "ToggleSoftwareRendering", ""},
        {"Toggle Widescreen",       "Graphics",    "Hotkeys", "ToggleWidescreen",        ""},
        {"Toggle Post-Processing",  "Graphics",    "Hotkeys", "TogglePostProcessing",    ""},
        {"Increase Resolution",     "Graphics",    "Hotkeys", "IncreaseResolutionScale", ""},
        {"Decrease Resolution",     "Graphics",    "Hotkeys", "DecreaseResolutionScale", ""},

        // Save States
        {"Load Selected State",     "Save States", "Hotkeys", "LoadSelectedSaveState",          "Keyboard/F1"},
        {"Save Selected State",     "Save States", "Hotkeys", "SaveSelectedSaveState",          "Keyboard/F2"},
        {"Previous Save Slot",      "Save States", "Hotkeys", "SelectPreviousSaveStateSlot",    "Keyboard/F3"},
        {"Next Save Slot",          "Save States", "Hotkeys", "SelectNextSaveStateSlot",        "Keyboard/F4"},
        {"Save & Next Slot",        "Save States", "Hotkeys", "SaveStateAndSelectNextSlot",     ""},
        {"Undo Load State",         "Save States", "Hotkeys", "UndoLoadState",                  ""},
        {"Load Game State 1",       "Save States", "Hotkeys", "LoadGameState1",  ""},
        {"Save Game State 1",       "Save States", "Hotkeys", "SaveGameState1",  ""},
        {"Load Game State 2",       "Save States", "Hotkeys", "LoadGameState2",  ""},
        {"Save Game State 2",       "Save States", "Hotkeys", "SaveGameState2",  ""},
        {"Load Game State 3",       "Save States", "Hotkeys", "LoadGameState3",  ""},
        {"Save Game State 3",       "Save States", "Hotkeys", "SaveGameState3",  ""},
        {"Load Game State 4",       "Save States", "Hotkeys", "LoadGameState4",  ""},
        {"Save Game State 4",       "Save States", "Hotkeys", "SaveGameState4",  ""},
        {"Load Game State 5",       "Save States", "Hotkeys", "LoadGameState5",  ""},
        {"Save Game State 5",       "Save States", "Hotkeys", "SaveGameState5",  ""},
        {"Load Game State 6",       "Save States", "Hotkeys", "LoadGameState6",  ""},
        {"Save Game State 6",       "Save States", "Hotkeys", "SaveGameState6",  ""},
        {"Load Game State 7",       "Save States", "Hotkeys", "LoadGameState7",  ""},
        {"Save Game State 7",       "Save States", "Hotkeys", "SaveGameState7",  ""},
        {"Load Game State 8",       "Save States", "Hotkeys", "LoadGameState8",  ""},
        {"Save Game State 8",       "Save States", "Hotkeys", "SaveGameState8",  ""},
        {"Load Game State 9",       "Save States", "Hotkeys", "LoadGameState9",  ""},
        {"Save Game State 9",       "Save States", "Hotkeys", "SaveGameState9",  ""},
        {"Load Game State 10",      "Save States", "Hotkeys", "LoadGameState10", ""},
        {"Save Game State 10",      "Save States", "Hotkeys", "SaveGameState10", ""},
    };
}

// ============================================================================
// patchRetroAchievements — enable/disable RA in [Cheevos] section
// ============================================================================

void DuckStationAdapter::patchRetroAchievements(const QString& username,
                                                  const QString& token,
                                                  bool enabled,
                                                  bool hardcore,
                                                  bool notifications,
                                                  bool sounds) {
    const QString path = portableDir() + "/settings.ini";

    QString content;
    if (!readConfigFile(path, content, "DuckStation"))
        return;

    Q_UNUSED(username);
    Q_UNUSED(token);
    // DuckStation encrypts its token with a machine-specific key.
    // We can't pre-patch credentials — only enable RA and set preferences.
    // DuckStation will prompt the user to log in on first launch.
    QVector<IniKeyPatch> patches = {
        {"Cheevos", "Enabled", enabled ? "true" : "false"},
        {"Cheevos", "ChallengeMode", hardcore ? "true" : "false"},
        {"Cheevos", "Notifications", notifications ? "true" : "false"},
        {"Cheevos", "SoundEffects", sounds ? "true" : "false"},
    };

    if (patchIniKeys(content, patches))
        writeConfigFile(path, content, "DuckStation");
}

// ============================================================================
// Asset matching — select the right GitHub release asset for this platform
// ============================================================================

QString DuckStationAdapter::matchAsset(const QStringList& assetNames) const {
    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
#if defined(Q_OS_MACOS)
        if (lower.contains("mac") && name.endsWith(".zip"))
            return name;
#elif defined(Q_OS_WIN)
        if (lower.contains("windows") && lower.contains("x64") && name.endsWith(".zip"))
            return name;
#else
        if (lower.contains("linux") && lower.contains("x64") && name.endsWith(".AppImage"))
            return name;
#endif
    }
    return EmulatorAdapter::matchAsset(assetNames);
}

// ============================================================================
// Resume file lookup
// ============================================================================

QString DuckStationAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty()) return {};

    const QString dsSerial = serialToFilenameFormat(serial);
    const QString statesDir = Paths::emulatorDataDir("duckstation", "psx") + "/savestates";
    QDir dir(statesDir);
    if (!dir.exists()) return {};

    const QString expected = dsSerial + "_resume.sav";
    if (dir.exists(expected)) {
        return statesDir + "/" + expected;
    }
    return {};
}
