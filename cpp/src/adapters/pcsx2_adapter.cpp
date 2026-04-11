#include "pcsx2_adapter.h"

static const char* PCSX2_INSTALL_FOLDER = "pcsx2";

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>

// ============================================================================
// Settings schema
// ============================================================================

QString PCSX2Adapter::configFilePath() const {
    return iniPath();
}

QVector<SettingDef> PCSX2Adapter::settingsSchema() const {
    QVector<SettingDef> s;
    // Field order: category, subcategory, group, section, key, label, tooltip, type, defaultValue, options, min, max, step

    // Interface tab removed — UI settings are controlled by the frontend, not exposed to users.

    // ═══════════════════════════════════════════════════════════════════════
    // Emulation (single page with grouped sections — no sub-tabs)
    // ═══════════════════════════════════════════════════════════════════════

    // ── Speed Control ───────────────────────────────────────────────────
    // INI values must use shortest float representation (e.g. "1", "0.5", "2") —
    // PCSX2 writes floats via StringUtil::ToChars which never produces zero-padded
    // forms, so padded values fail to round-trip. See audit 2026-04-06.
    const QVector<QPair<QString,QString>> speedOptions = {
        {"2% [1 FPS (NTSC) / 1 FPS (PAL)]",       "0.02"},
        {"10% [6 FPS (NTSC) / 5 FPS (PAL)]",      "0.1"},
        {"25% [15 FPS (NTSC) / 12 FPS (PAL)]",    "0.25"},
        {"50% [30 FPS (NTSC) / 25 FPS (PAL)]",    "0.5"},
        {"75% [45 FPS (NTSC) / 37 FPS (PAL)]",    "0.75"},
        {"90% [54 FPS (NTSC) / 45 FPS (PAL)]",    "0.9"},
        {"100% [60 FPS (NTSC) / 50 FPS (PAL)]",   "1"},
        {"110% [66 FPS (NTSC) / 55 FPS (PAL)]",   "1.1"},
        {"120% [72 FPS (NTSC) / 60 FPS (PAL)]",   "1.2"},
        {"150% [90 FPS (NTSC) / 75 FPS (PAL)]",   "1.5"},
        {"175% [105 FPS (NTSC) / 87 FPS (PAL)]",  "1.75"},
        {"200% [120 FPS (NTSC) / 100 FPS (PAL)]", "2"},
        {"300% [180 FPS (NTSC) / 150 FPS (PAL)]", "3"},
        {"400% [240 FPS (NTSC) / 200 FPS (PAL)]", "4"},
        {"500% [300 FPS (NTSC) / 250 FPS (PAL)]", "5"},
        {"1000% [600 FPS (NTSC) / 500 FPS (PAL)]","10"},
        {"Unlimited", "0"},
    };
    {
        SettingDef d{"Emulation", "", "Speed Control", "Framerate", "NominalScalar", "Normal Speed",
                  "Sets the target speed for normal gameplay.", SettingDef::Combo, "1", speedOptions, 0, 0, 0};
        d.recommendedValue = "1";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Speed Control", "Framerate", "TurboScalar", "Fast-Forward Speed",
                  "Sets the target speed when turbo mode is activated.", SettingDef::Combo, "2", speedOptions, 0, 0, 0};
        d.recommendedValue = "2";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Speed Control", "Framerate", "SlomoScalar", "Slow-Motion Speed",
                  "Sets the target speed when slow motion mode is activated.", SettingDef::Combo, "0.5", speedOptions, 0, 0, 0};
        d.recommendedValue = "0.5";
        s.append(d);
    }

    // ── System Settings ─────────────────────────────────────────────────
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "EECycleRate", "EE Cycle Rate",
                  "Underclocks or overclocks the emulated Emotion Engine CPU.",
                  SettingDef::Combo, "0", {
                      {"50% (Underclock)", "-3"}, {"60% (Underclock)", "-2"}, {"75% (Underclock)", "-1"},
                      {"100% (Normal Speed)", "0"},
                      {"130% (Overclock)", "1"}, {"180% (Overclock)", "2"}, {"300% (Overclock)", "3"}
                  }, 0, 0, 0};
        d.recommendedValue = "0";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "EECycleSkip", "EE Cycle Skipping",
                  "Makes the emulated Emotion Engine skip cycles.",
                  SettingDef::Combo, "0", {
                      {"Disabled", "0"}, {"Mild Underclock", "1"}, {"Moderate Underclock", "2"}, {"Maximum Underclock", "3"}
                  }, 0, 0, 0};
        d.recommendedValue = "0";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "vuThread", "Enable Multithreaded VU1 (MTVU)",
                  "Runs VU1 on a second thread. Substantial speed improvement in most games.", SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "EnableThreadPinning", "Enable Thread Pinning",
                  "Pins emulation threads to specific CPU cores for improved performance.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "CdvdPrecache", "Enable CDVD Precaching",
                  "Loads the disc image into RAM before starting. Can reduce stutter but uses more memory.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "HostFs", "Enable Host Filesystem",
                  "Enables access to the host filesystem from the emulated PS2.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "EnableCheats", "Enable Cheats",
                  "Enables loading cheats from pnach files.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "EnableFastBoot", "Fast Boot",
                  "Skips the PS2 BIOS splash screen when booting a game.", SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        s.append(d);
    }

    // ── Frame Pacing / Latency Control ─────────────────────────────────
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "VsyncQueueSize", "Maximum Frame Latency",
                  "Sets the number of frames that can be queued up before the CPU waits. Set to 0 for optimal frame pacing.",
                  SettingDef::Combo, "2", {
                      {"Optimal (Frame Pacing)", "0"}, {"1 frame", "1"}, {"2 frames", "2"}, {"3 frames", "3"}
                  }, 0, 0, 0};
        d.recommendedValue = "2";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "SyncToHostRefreshRate", "Sync to Host Refresh Rate",
                  "Adjusts emulation speed slightly to match your monitor's refresh rate.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "VsyncEnable", "Vertical Sync (VSync)",
                  "Synchronizes frame output with the monitor to prevent screen tearing.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "UseVSyncForTiming", "Use Host VSync Timing",
                  "Uses the host's VSync timing instead of the emulated console's timing.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "SkipDuplicateFrames", "Skip Presenting Duplicate Frames",
                  "Skips presenting frames that are identical to the previous frame.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Display
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Display", "", "EmuCore/GS", "Renderer", "Renderer", "",
              SettingDef::Combo, "-1",
              {{"Auto", "-1"}, {"OpenGL", "12"}, {"Vulkan", "14"},
#if defined(Q_OS_MACOS)
               {"Metal", "17"},
#endif
               {"Software", "13"}}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "4:3",
              {{"Auto 4:3/3:2", "Auto 4:3/3:2"}, {"4:3", "4:3"}, {"16:9", "16:9"},
               {"10:7", "10:7"}, {"Stretch", "Stretch"}}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "FMVAspectRatioSwitch", "FMV Aspect Ratio Override", "",
              SettingDef::Combo, "Off",
              {{"Off (Default)", "Off"}, {"Auto Standard (4:3 Interlaced / 3:2 Progressive)", "Auto 4:3/3:2"},
               {"Standard (4:3)", "4:3"}, {"Widescreen (16:9)", "16:9"}, {"Native/Full (10:7)", "10:7"}}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "deinterlace_mode", "Deinterlacing", "",
              SettingDef::Combo, "0",
              {{"Automatic", "0"}, {"Off", "1"}, {"Weave (Top)", "2"}, {"Weave (Bottom)", "3"},
               {"Bob (Top)", "4"}, {"Bob (Bottom)", "5"}, {"Blend (Top)", "6"}, {"Blend (Bottom)", "7"},
               {"Adaptive (Top)", "8"}, {"Adaptive (Bottom)", "9"}}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "linear_present_mode", "Bilinear Filtering", "",
              SettingDef::Combo, "1",
              {{"None", "0"}, {"Bilinear (Smooth)", "1"}, {"Bilinear (Sharp)", "2"}}, 0, 0, 0});

    s.append({"Graphics", "Display", "", "EmuCore/GS", "StretchY", "Vertical Stretch", "",
              SettingDef::Int, "100", {}, 10, 300, 1, "", "%"});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "CropLeft", "Left", "Crop", SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "CropTop", "Top", "", SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "CropRight", "Right", "", SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "CropBottom", "Bottom", "", SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"});
    // Display checkboxes
    s.append({"Graphics", "Display", "", "EmuCore", "EnableWideScreenPatches", "Apply Widescreen Patches",
              "Automatically applies widescreen patches to supported games.", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore", "EnableNoInterlacingPatches", "Apply No-Interlacing Patches",
              "Automatically applies no-interlacing patches to supported games.", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "pcrtc_antiblur", "Anti-Blur",
              "Enables internal anti-blur hacks.", SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "IntegerScaling", "Integer Scaling",
              "Adds padding to ensure the image is only scaled by whole numbers.", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "pcrtc_offsets", "Screen Offsets",
              "Enables PCRTc offsets which position the screen as the game requests.", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "disable_interlace_offset", "Disable Interlace Offset",
              "Disables interlacing offset which may reduce jitter in some games.", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Display", "", "EmuCore/GS", "pcrtc_overscan", "Show Overscan",
              "Shows the overscan area of the display.", SettingDef::Bool, "false", {}, 0, 0, 0});


    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Rendering
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "upscale_multiplier", "Internal Resolution",
                     "Sets the internal rendering resolution. Higher values produce sharper visuals at the cost of GPU performance.",
                     SettingDef::Combo, "1",
                     {{"Native (PS2) (Default)", "1"}, {"2x Native (~720px/HD)", "2"}, {"3x Native (~1080px/FHD)", "3"},
                      {"4x Native (~1440px/QHD)", "4"}, {"5x Native (~1800px/QHD+)", "5"}, {"6x Native (~2160px/4K UHD)", "6"},
                      {"7x Native (~2520px)", "7"}, {"8x Native (~2880px/5K UHD)", "8"}, {"9x Native (~3240px)", "9"},
                      {"10x Native (~3600px/6K UHD)", "10"}, {"11x Native (~3960px)", "11"}, {"12x Native (~4320px/8K UHD)", "12"}}, 0, 0, 0};
        d.recommendedValue = "1";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "filter", "Texture Filtering",
                     "Controls how textures are sampled when rendered. Bilinear (PS2) matches the original hardware behavior; Forced options ignore the game's preference.",
                     SettingDef::Combo, "2",
                     {{"Nearest", "0"}, {"Bilinear (Forced)", "1"}, {"Bilinear (PS2)", "2"}, {"Bilinear (Forced excluding sprite)", "3"}}, 0, 0, 0};
        d.recommendedValue = "2";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "TriFilter", "Trilinear Filtering",
                     "Enables trilinear filtering for smoother transitions between mipmap levels. Auto leaves this decision to each game.",
                     SettingDef::Combo, "-1",
                     {{"Auto (Default)", "-1"}, {"Off", "0"}, {"Trilinear (PS2)", "1"}, {"Trilinear (Forced)", "2"}}, 0, 0, 0};
        d.recommendedValue = "-1";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "MaxAnisotropy", "Anisotropic Filtering",
                     "Improves texture clarity at oblique viewing angles. Low cost on modern GPUs and generally safe to raise.",
                     SettingDef::Combo, "0",
                     {{"Off", "0"}, {"2x", "2"}, {"4x", "4"}, {"8x", "8"}, {"16x", "16"}}, 0, 0, 0};
        d.recommendedValue = "0";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "dithering_ps2", "Dithering",
                     "Controls how PS2 dithering patterns are applied to upscaled rendering. Unscaled matches the original appearance.",
                     SettingDef::Combo, "2",
                     {{"Off", "0"}, {"Scaled", "1"}, {"Unscaled (Default)", "2"}, {"Force 32bit", "3"}}, 0, 0, 0};
        d.recommendedValue = "2";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "accurate_blending_unit", "Blending Accuracy",
                     "Controls how accurately PS2 blending operations are emulated. Higher levels improve compatibility with heavy effects at a performance cost.",
                     SettingDef::Combo, "1",
                     {{"Minimum", "0"}, {"Basic (Default)", "1"}, {"Medium", "2"}, {"High", "3"}, {"Full", "4"}, {"Maximum", "5"}}, 0, 0, 0};
        d.recommendedValue = "1";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "hw_mipmap", "Mipmapping",
                     "Enables mipmapping which improves texture quality at the cost of performance.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        s.append(d);
    }



    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Post-Processing
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Post-Processing", "Sharpening/Anti-Aliasing", "EmuCore/GS", "CASMode", "Contrast Adaptive Sharpening",
                     "Contrast Adaptive Sharpening uses AMD's CAS algorithm to sharpen the final image. Sharpen Only applies at internal resolution; Sharpen and Resize applies at display resolution.",
                     SettingDef::Combo, "0",
                     {{"None (Default)", "0"}, {"Sharpen Only (Internal Resolution)", "1"},
                      {"Sharpen and Resize (Display Resolution)", "2"}}, 0, 0, 0};
        d.recommendedValue = "0";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Sharpening/Anti-Aliasing", "EmuCore/GS", "CASSharpness", "Sharpness",
                     "Strength of the CAS sharpening effect. Higher values produce sharper but potentially noisier images.",
                     SettingDef::Int, "50", {}, 0, 100, 1, "", "%"};
        d.recommendedValue = "50";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Sharpening/Anti-Aliasing", "EmuCore/GS", "fxaa", "FXAA",
                     "Enables Fast Approximate Anti-Aliasing.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }

    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "TVShader", "TV Shader",
                     "Applies a CRT-style filter to the final output for an authentic retro look. None disables the filter.",
                     SettingDef::Combo, "0",
                     {{"None (Default)", "0"}, {"Scanline Filter", "1"}, {"Diagonal Filter", "2"}, {"Triangular Filter", "3"},
                      {"Wave Filter", "4"}, {"Lottes CRT", "5"},
                      {"4xRGSS downsampling (4x Rotated Grid SuperSampling)", "6"},
                      {"NxAGSS downsampling (Nx Automatic Grid SuperSampling)", "7"}}, 0, 0, 0};
        d.recommendedValue = "0";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost", "Shade Boost",
                     "Enables manual adjustment of display brightness, contrast, and saturation.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Brightness", "Brightness",
                     "Adjusts the overall brightness of the display when Shade Boost is enabled.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        d.recommendedValue = "50";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Contrast", "Contrast",
                     "Adjusts the contrast between dark and light areas when Shade Boost is enabled.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        d.recommendedValue = "50";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Saturation", "Saturation",
                     "Adjusts color saturation when Shade Boost is enabled.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        d.recommendedValue = "50";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Gamma", "Gamma",
                     "Adjusts gamma correction when Shade Boost is enabled.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        d.recommendedValue = "50";
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > OSD
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "OSD", "On-Screen Display", "EmuCore/GS", "OsdScale", "OSD Scale", "",
              SettingDef::Int, "100", {}, 25, 500, 25, "", "%"});
    s.append({"Graphics", "OSD", "On-Screen Display", "EmuCore/GS", "OsdMessagesPos", "OSD Messages Position", "",
              SettingDef::Combo, "1",
              {{"None", "0"}, {"Top Left (Default)", "1"}, {"Top Center", "2"}, {"Top Right", "3"},
               {"Center Left", "4"}, {"Center", "5"}, {"Center Right", "6"},
               {"Bottom Left", "7"}, {"Bottom Center", "8"}, {"Bottom Right", "9"}}, 0, 0, 0});
    s.append({"Graphics", "OSD", "On-Screen Display", "EmuCore/GS", "OsdPerformancePos", "OSD Performance Position", "",
              SettingDef::Combo, "3",
              {{"None", "0"}, {"Top Left", "1"}, {"Top Center", "2"}, {"Top Right (Default)", "3"},
               {"Center Left", "4"}, {"Center", "5"}, {"Center Right", "6"},
               {"Bottom Left", "7"}, {"Bottom Center", "8"}, {"Bottom Right", "9"}}, 0, 0, 0});
    // ── Performance Stats ─────────────────────────────────────────────
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowSpeed", "Show Speed Percentages", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowFPS", "Show FPS", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowVPS", "Show VPS", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowResolution", "Show Resolution", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowGSStats", "Show GS Statistics", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowCPU", "Show CPU Usage", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowGPU", "Show GPU Usage", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowIndicators", "Show Status Indicators", "", SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Performance Stats", "EmuCore/GS", "OsdShowFrameTimes", "Show Frame Times", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    // ── System Information ───────────────────────────────────────────
    s.append({"Graphics", "OSD", "System Information", "EmuCore/GS", "OsdShowHardwareInfo", "Show Hardware Info", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "System Information", "EmuCore/GS", "OsdShowVersion", "Show PCSX2 Version", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    // ── Settings & Inputs ────────────────────────────────────────────
    s.append({"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS", "OsdShowSettings", "Show Settings", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS", "OsdshowPatches", "Show Patches", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS", "OsdShowInputs", "Show Inputs", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS", "OsdShowVideoCapture", "Show Video Capture Status", "", SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS", "OsdShowInputRec", "Show Input Recording Status", "", SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS", "OsdShowTextureReplacements", "Show Texture Replacement Status", "", SettingDef::Bool, "false", {}, 0, 0, 0});
    // ── Messages ─────────────────────────────────────────────────────
    s.append({"Graphics", "OSD", "Messages", "EmuCore", "WarnAboutUnsafeSettings", "Warn About Unsafe Settings", "", SettingDef::Bool, "true", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Audio
    // ═══════════════════════════════════════════════════════════════════════

    // ── Configuration ─────────────────────────────────────────────────
    // Audio enum combos use exact-case enum name strings, not integers — see
    // AudioStream::GetBackendName / GetExpansionModeName / GetSyncModeName in
    // references/pcsx2-master/pcsx2/Host/AudioStream.cpp:148-221. Audit 2026-04-06.
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "Backend", "Backend", "",
                     SettingDef::Combo, "Cubeb",
                     {{"Cubeb", "Cubeb"}, {"SDL", "SDL"}, {"Null (No Sound)", "Null"}}, 0, 0, 0};
        d.recommendedValue = "Cubeb";
        s.append(d);
    }
    // TODO(audit-tier-4): DriverName/DeviceName should be enumerated at runtime
    // from the selected backend (Cubeb driver list, host audio device list).
    // Hard-coded options here are macOS-specific and exclude most real devices.
    // Deferred until a shared mechanism is designed across all three adapters.
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "DriverName", "Driver", "",
                     SettingDef::Combo, "",
                     {{"Default", ""}, {"audiounit", "audiounit"}}, 0, 0, 0};
        d.recommendedValue = "Default";
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "DeviceName", "Output Device", "",
                     SettingDef::Combo, "",
                     {{"Default", ""}}, 0, 0, 0};
        d.recommendedValue = "Default";
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "ExpansionMode", "Expansion", "",
                     SettingDef::Combo, "Disabled",
                     {{"Disabled (Stereo)", "Disabled"}, {"Stereo with LFE", "StereoLFE"},
                      {"Quadraphonic", "Quadraphonic"}, {"Quadraphonic with LFE", "QuadraphonicLFE"},
                      {"5.1 Surround", "Surround51"}, {"7.1 Surround", "Surround71"}}, 0, 0, 0};
        d.recommendedValue = "Disabled";
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "SyncMode", "Synchronization", "",
                     SettingDef::Combo, "TimeStretch",
                     {{"Disabled (Noisy)", "Disabled"}, {"TimeStretch (Recommended)", "TimeStretch"}}, 0, 0, 0};
        d.recommendedValue = "TimeStretch";
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "BufferMS", "Buffer Size", "",
                     SettingDef::Int, "50", {}, 10, 500, 10, "slider", "ms"};
        d.recommendedValue = "50";
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "OutputLatencyMS", "Output Latency", "",
                     SettingDef::Int, "20", {}, 0, 500, 5, "slider", "ms"};
        d.recommendedValue = "20";
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "OutputLatencyMinimal", "Minimal Output Latency",
                     "Uses the smallest possible latency value. May cause crackling.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }

    // ── Controls ──────────────────────────────────────────────────────
    {
        SettingDef d{"Audio", "", "Controls", "SPU2/Output", "StandardVolume", "Standard Volume", "",
                     SettingDef::Int, "100", {}, 0, 200, 5, "slider", "%"};
        d.recommendedValue = "100";
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Controls", "SPU2/Output", "FastForwardVolume", "Fast Forward Volume", "",
                     SettingDef::Int, "100", {}, 0, 200, 5, "slider", "%"};
        d.recommendedValue = "100";
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Controls", "SPU2/Output", "OutputMuted", "Mute All Sound",
                     "Mutes all audio output.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Memory Cards
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Slot1_Enable", "Slot 1", "", SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Slot1_Filename", "Slot 1 Filename", "", SettingDef::String, "Mcd001.ps2", {}, 0, 0, 0};
        d.recommendedValue = "Mcd001.ps2";
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Slot2_Enable", "Slot 2", "", SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Slot2_Filename", "Slot 2 Filename", "", SettingDef::String, "Mcd002.ps2", {}, 0, 0, 0};
        d.recommendedValue = "Mcd002.ps2";
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Multitap1_Slot2_Enable", "Multitap 1 - Slot 2", "", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Multitap1_Slot3_Enable", "Multitap 1 - Slot 3", "", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Multitap1_Slot4_Enable", "Multitap 1 - Slot 4", "", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        s.append(d);
    }

    return s;
}

// ============================================================================
// Platform-specific config directory
// ============================================================================

QString PCSX2Adapter::configDir() {
    // Portable mode: config lives next to the emulator in our root
    return Paths::emulatorsDir(PCSX2_INSTALL_FOLDER);
}

QString PCSX2Adapter::iniPath() {
    return configDir() + "/inis/PCSX2.ini";
}

// ============================================================================
// ensureConfig — create or patch PCSX2.ini before launch
// ============================================================================

bool PCSX2Adapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                const QString& biosPath,
                                const QString& savesPath) {
    // Enable portable mode — PCSX2 stores data next to executable
    // portable.txt content is used as the data directory name; empty = current dir
    if (!ensurePortableMarker(configDir(), "PCSX2"))
        return false;

    const QString path = iniPath();

    if (!QFileInfo::exists(path))
        return createDefaultConfig(path, biosPath, savesPath);

    // Check if the file is incomplete (no SettingsVersion = created by
    // external setting writer before first PCSX2 launch)
    QString content;
    if (readConfigFile(path, content, "PCSX2") && !content.contains("SettingsVersion")) {
        qInfo() << "[PCSX2] Config incomplete, recreating with defaults";
        QFile::remove(path);
        return createDefaultConfig(path, biosPath, savesPath);
    }
    return patchExistingConfig(path, biosPath, savesPath);
}

// ============================================================================
// resolveExecutable — platform-aware executable resolution
// ============================================================================

QString PCSX2Adapter::resolveExecutable(const EmulatorManifest& manifest,
                                        const QString& installPath) {
    return resolveExecutableInDir(manifest, installPath, "PCSX2");
}

// ============================================================================
// createDefaultConfig — write only embedding-critical keys
// The emulator will fill in its own defaults for everything else on first
// launch.  This prevents our config from going stale when the emulator
// renames or removes INI keys in a future update.
// ============================================================================

bool PCSX2Adapter::createDefaultConfig(const QString& path,
                                       const QString& biosPath,
                                       const QString& savesPath) {

    // savesPath is this emulator's unified data root for its system,
    // i.e. {root}/emulators/pcsx2/ps2/. Every managed subfolder lives
    // directly under it — see EmulatorService::ensureConfig().
    const QString& dataRoot = savesPath;
    const QString savestatesPath    = dataRoot + "/savestates";
    const QString memcardsPath      = dataRoot + "/memcards";
    const QString screenshotsPath   = dataRoot + "/screenshots";
    const QString cachePath         = dataRoot + "/cache";
    const QString cheatsPath        = dataRoot + "/cheats";
    const QString videosPath        = dataRoot + "/videos";
    const QString texturesPath      = dataRoot + "/textures";
    const QString logsPath          = dataRoot + "/logs";
    const QString inputProfilesPath = dataRoot + "/inputprofiles";
    const QString patchesPath       = dataRoot + "/patches";
    const QString gamSettingsPath   = dataRoot + "/gamesettings";

    // Only write keys required for embedding (wizard suppression, fullscreen,
    // managed paths, input sources).  All other settings (graphics, audio,
    // speed hacks, etc.) are left to the emulator's own defaults so they
    // stay in sync across emulator updates.
    QStringList lines = {
        "[UI]",
        "SetupWizardComplete = true",
        "SettingsVersion = 1",
        "StartFullscreen = true",
        "HideMouseCursor = true",
        "RenderToSeparateWindow = false",
        "HideMainWindowWhenRunning = true",
        "PauseOnFocusLoss = true",
        "SetupWizardIncomplete = false",
        "",
        "[Folders]",
        "Bios = " + biosPath,
        "Savestates = " + savestatesPath,
        "Screenshots = " + screenshotsPath,
        "Cache = " + cachePath,
        "Cheats = " + cheatsPath,
        "MemoryCards = " + memcardsPath,
        "Snapshots = " + screenshotsPath,
        "Videos = " + videosPath,
        "Textures = " + texturesPath,
        "Logs = " + logsPath,
        "InputProfiles = " + inputProfilesPath,
        "Patches = " + patchesPath,
        "GameSettings = " + gamSettingsPath,
        "",
        "[InputSources]",
        "SDL = true",
        "SDLControllerEnhancedMode = true",
#if defined(Q_OS_MACOS)
        "SDLIOKitDriver = true",
        "SDLMFIDriver = true",
#endif
        "",
        "[EmuCore]",
        "SaveStateOnShutdown = true",
        "",
        "[EmuCore/Speedhacks]",
        "vuThread = true",
        "",
        "[Pad1]",
        "",
        "[Hotkeys]",
        "OpenPauseMenu =",
        "TogglePause =",
        "",
    };

    return writeConfigFile(path, lines.join("\n"), "PCSX2");
}

// ============================================================================
// patchExistingConfig — fix up an existing config for headless operation
// ============================================================================

bool PCSX2Adapter::patchExistingConfig(const QString& path,
                                       const QString& biosPath,
                                       const QString& savesPath) {
    QString content;
    if (!readConfigFile(path, content, "PCSX2"))
        return false;

    bool changed = suppressSetupWizard(content, "UI");

    // Remove saved window geometry (prevent windowed restore)
    static const QRegularExpression geoRe("MainWindowGeometry = .*\\n?");
    static const QRegularExpression stateRe("MainWindowState = .*\\n?");
    if (content.contains("MainWindowGeometry")) {
        content.replace(geoRe, "");
        changed = true;
    }
    if (content.contains("MainWindowState")) {
        content.replace(stateRe, "");
        changed = true;
    }

    // Ensure embedding-critical keys, folder paths, save-on-shutdown, and
    // neutered hotkeys in one pass. patchIniKeys injects missing keys and
    // sections as needed, so StartFullscreen / PauseOnFocusLoss /
    // RenderToSeparateWindow are force-corrected whether or not they exist.
    //
    // savesPath is this emulator's unified data root
    // ({root}/emulators/pcsx2/ps2/) — every subfolder lives directly under it.
    const QString& dataRoot = savesPath;
    QVector<IniKeyPatch> patches = {
        {"UI", "StartFullscreen",        "true"},
        {"UI", "PauseOnFocusLoss",       "true"},
        {"UI", "RenderToSeparateWindow", "false"},

        {"Folders", "Bios",          biosPath},
        {"Folders", "Savestates",    dataRoot + "/savestates"},
        {"Folders", "MemoryCards",   dataRoot + "/memcards"},
        {"Folders", "Screenshots",   dataRoot + "/screenshots"},
        {"Folders", "Cache",         dataRoot + "/cache"},
        {"Folders", "Cheats",        dataRoot + "/cheats"},
        {"Folders", "Snapshots",     dataRoot + "/screenshots"},
        {"Folders", "Videos",        dataRoot + "/videos"},
        {"Folders", "Textures",      dataRoot + "/textures"},
        {"Folders", "Logs",          dataRoot + "/logs"},
        {"Folders", "InputProfiles", dataRoot + "/inputprofiles"},
        {"Folders", "Patches",       dataRoot + "/patches"},
        {"Folders", "GameSettings",  dataRoot + "/gamesettings"},

        {"EmuCore", "SaveStateOnShutdown", "true"},

        // Suppress native pause menu and TogglePause — we use PauseOnFocusLoss instead
        {"Hotkeys", "OpenPauseMenu", ""},
        {"Hotkeys", "TogglePause",   ""},
    };
    if (patchIniKeys(content, patches))
        changed = true;

    // Ensure SDL input sources (whole section block — patchIniKeys is for
    // individual keys, and we need to emit the platform-specific lines below)
    if (!content.contains("[InputSources]")) {
        QStringList inputLines = {
            "",
            "[InputSources]",
            "SDL = true",
            "SDLControllerEnhancedMode = true",
#if defined(Q_OS_MACOS)
            "SDLIOKitDriver = true",
            "SDLMFIDriver = true",
#endif
            "",
        };
        content.append(inputLines.join("\n"));
        changed = true;
    }

    // Ensure [Pad1] section exists for controller bindings
    if (!content.contains("[Pad1]")) {
        content.append("\n[Pad1]\n");
        changed = true;
    }

    if (changed && !writeConfigFile(path, content, "PCSX2"))
        return false;
    return true;
}

// ============================================================================
// BIOS files
// ============================================================================

QVector<PathDef> PCSX2Adapter::pathsDefs() const {
    return {
        {"BIOS",         "Folders", "Bios",        "",            PathBase::Bios},
        {"Save States",  "Folders", "Savestates",  "savestates",  PathBase::EmulatorData},
        {"Memory Cards", "Folders", "MemoryCards", "memcards",    PathBase::EmulatorData},
        {"Screenshots",  "Folders", "Screenshots", "screenshots", PathBase::EmulatorData},
        {"Cache",        "Folders", "Cache",       "cache",       PathBase::EmulatorData},
        {"Cheats",       "Folders", "Cheats",      "cheats",      PathBase::EmulatorData},
        {"Textures",     "Folders", "Textures",    "textures",    PathBase::EmulatorData},
        {"Videos",       "Folders", "Videos",      "videos",      PathBase::EmulatorData},
    };
}

AspectRatioOptions PCSX2Adapter::aspectRatioOptions() const {
    return {{
        {"4:3", {
            {"EmuCore/GS", "AspectRatio", "4:3"},
            {"EmuCore", "EnableWideScreenPatches", "false"},
        }},
        {"16:9", {
            {"EmuCore/GS", "AspectRatio", "16:9"},
            {"EmuCore", "EnableWideScreenPatches", "true"},
        }},
    }, "4:3"};
}

ResolutionOptions PCSX2Adapter::resolutionOptions() const {
    return {"EmuCore/GS", "upscale_multiplier",
            {{"720P", "2"}, {"1080P", "3"}, {"1440P", "4"}, {"4K", "6"}}, "2"};
}

QVector<BiosDef> PCSX2Adapter::biosFiles() const {
    return {
        {"SCPH-70012.bin", "PS2 BIOS v12 (North America)", true, ""},
        {"SCPH-77001.bin", "PS2 BIOS v15 (North America)", false, ""},
        {"SCPH-70004.bin", "PS2 BIOS v12 (Europe)", false, ""},
        {"SCPH-70002.bin", "PS2 BIOS v12 (Europe)", false, ""},
        {"SCPH-70006.bin", "PS2 BIOS v12 (Japan)", false, ""},
        {"SCPH-39001.bin", "PS2 BIOS v7 (North America)", false, ""},
        {"SCPH-30004R.bin", "PS2 BIOS v6 (Europe)", false, ""},
    };
}

// ============================================================================
// Controller settings (Settings sub-tab)
// ============================================================================

QVector<SettingDef> PCSX2Adapter::controllerSettingDefs() const {
    return {
        {"", "", "", "Pad1", "InvertL",
         "Invert Left Stick", "Inverts the direction of the left analog stick.",
         SettingDef::Combo, "0",
         {{"Not Inverted", "0"}, {"Invert X Axis", "1"}, {"Invert Y Axis", "2"}, {"Invert Both Axes", "3"}},
         0, 3, 1, "", ""},

        {"", "", "", "Pad1", "InvertR",
         "Invert Right Stick", "Inverts the direction of the right analog stick.",
         SettingDef::Combo, "0",
         {{"Not Inverted", "0"}, {"Invert X Axis", "1"}, {"Invert Y Axis", "2"}, {"Invert Both Axes", "3"}},
         0, 3, 1, "", ""},

        {"", "", "", "Pad1", "Deadzone",
         "Analog Deadzone", "Sets the analog stick deadzone, i.e. the fraction of the stick movement which will be ignored.",
         SettingDef::Int, "0",
         {}, 0, 100, 1, "", "%"},

        {"", "", "", "Pad1", "AxisScale",
         "Analog Sensitivity", "Sets the analog stick axis scaling factor. A value between 130% and 140% is recommended when using recent controllers, e.g. DualShock 4, Xbox One Controller.",
         SettingDef::Int, "133",
         {}, 0, 200, 1, "", "%"},

        {"", "", "", "Pad1", "LargeMotorScale",
         "Large Motor Vibration Scale", "Increases or decreases the intensity of low frequency vibration sent by the game.",
         SettingDef::Int, "100",
         {}, 0, 200, 1, "", "%"},

        {"", "", "", "Pad1", "SmallMotorScale",
         "Small Motor Vibration Scale", "Increases or decreases the intensity of high frequency vibration sent by the game.",
         SettingDef::Int, "100",
         {}, 0, 200, 1, "", "%"},

        {"", "", "", "Pad1", "ButtonDeadzone",
         "Button/Trigger Deadzone", "Sets the deadzone for activating buttons/triggers, i.e. the fraction of the trigger which will be ignored.",
         SettingDef::Int, "0",
         {}, 0, 100, 1, "", "%"},

        {"", "", "", "Pad1", "PressureModifier",
         "Pressure Modifier Amount", "Sets the pressure when the modifier button is held.",
         SettingDef::Int, "50",
         {}, 0, 100, 1, "", "%"},
    };
}

// ============================================================================
// Controller bindings
// ============================================================================

QVector<BindingDef> PCSX2Adapter::controllerBindingDefs() const {
    return {
        // D-Pad
        {BindingDef::Button, "Up",       "D-Pad",        "Pad1", "Up",    "SDL-0/DPadUp"},
        {BindingDef::Button, "Down",     "D-Pad",        "Pad1", "Down",  "SDL-0/DPadDown"},
        {BindingDef::Button, "Left",     "D-Pad",        "Pad1", "Left",  "SDL-0/DPadLeft"},
        {BindingDef::Button, "Right",    "D-Pad",        "Pad1", "Right", "SDL-0/DPadRight"},
        // Face Buttons
        {BindingDef::Button, "Cross",    "Face Buttons",  "Pad1", "Cross",    "SDL-0/FaceSouth"},
        {BindingDef::Button, "Circle",   "Face Buttons",  "Pad1", "Circle",   "SDL-0/FaceEast"},
        {BindingDef::Button, "Square",   "Face Buttons",  "Pad1", "Square",   "SDL-0/FaceWest"},
        {BindingDef::Button, "Triangle", "Face Buttons",  "Pad1", "Triangle", "SDL-0/FaceNorth"},
        // Shoulders
        {BindingDef::Button, "L1", "Shoulders", "Pad1", "L1", "SDL-0/LeftShoulder"},
        {BindingDef::Button, "R1", "Shoulders", "Pad1", "R1", "SDL-0/RightShoulder"},
        // Triggers
        {BindingDef::Axis,   "L2", "Triggers", "Pad1", "L2", "SDL-0/+LeftTrigger"},
        {BindingDef::Axis,   "R2", "Triggers", "Pad1", "R2", "SDL-0/+RightTrigger"},
        // Stick Buttons
        {BindingDef::Button, "L3", "Stick Buttons", "Pad1", "L3", "SDL-0/LeftStick"},
        {BindingDef::Button, "R3", "Stick Buttons", "Pad1", "R3", "SDL-0/RightStick"},
        // Left Stick
        {BindingDef::Axis, "Left Stick Up",    "Left Stick",  "Pad1", "LUp",    "SDL-0/-LeftY"},
        {BindingDef::Axis, "Left Stick Down",  "Left Stick",  "Pad1", "LDown",  "SDL-0/+LeftY"},
        {BindingDef::Axis, "Left Stick Left",  "Left Stick",  "Pad1", "LLeft",  "SDL-0/-LeftX"},
        {BindingDef::Axis, "Left Stick Right", "Left Stick",  "Pad1", "LRight", "SDL-0/+LeftX"},
        // Right Stick
        {BindingDef::Axis, "Right Stick Up",    "Right Stick", "Pad1", "RUp",    "SDL-0/-RightY"},
        {BindingDef::Axis, "Right Stick Down",  "Right Stick", "Pad1", "RDown",  "SDL-0/+RightY"},
        {BindingDef::Axis, "Right Stick Left",  "Right Stick", "Pad1", "RLeft",  "SDL-0/-RightX"},
        {BindingDef::Axis, "Right Stick Right", "Right Stick", "Pad1", "RRight", "SDL-0/+RightX"},
        // Start/Select
        {BindingDef::Button, "Start",  "System", "Pad1", "Start",  "SDL-0/Start"},
        {BindingDef::Button, "Select", "System", "Pad1", "Select", "SDL-0/Back"},
        // Pressure/Analog
        {BindingDef::Button, "Pressure Modifier", "System", "Pad1", "PressureModifier", ""},
        {BindingDef::Button, "Analog",            "System", "Pad1", "Analog",           ""},
    };
}

QVector<HotkeyDef> PCSX2Adapter::hotkeyBindingDefs() const {
    return {
        // ── Speed Control ──
        {"Frame Advance",               "Speed Control", "Hotkeys", "FrameAdvance",       ""},
        {"Toggle Frame Limit",          "Speed Control", "Hotkeys", "ToggleFrameLimit",   ""},
        {"Toggle Turbo / Fast Forward", "Speed Control", "Hotkeys", "ToggleTurbo",        "Keyboard/Period"},
        {"Turbo / Fast Forward (Hold)", "Speed Control", "Hotkeys", "HoldTurbo",          ""},
        {"Toggle Slow Motion",          "Speed Control", "Hotkeys", "ToggleSlowMotion",   "Keyboard/Shift & Keyboard/Backspace"},
        {"Increase Target Speed",       "Speed Control", "Hotkeys", "IncreaseSpeed",      ""},
        {"Decrease Target Speed",       "Speed Control", "Hotkeys", "DecreaseSpeed",      ""},

        // ── System ──
        {"Reset Virtual Machine",       "System",        "Hotkeys", "ResetVM",            ""},
        {"Reload Patches",              "System",        "Hotkeys", "ReloadPatches",      ""},
        {"Swap Memory Cards",           "System",        "Hotkeys", "SwapMemCards",       ""},

        // ── Save States ──
        {"Select Previous Save Slot",   "Save States",   "Hotkeys", "PreviousSaveStateSlot",    "Keyboard/Shift & Keyboard/F2"},
        {"Select Next Save Slot",       "Save States",   "Hotkeys", "NextSaveStateSlot",         "Keyboard/F2"},
        {"Save State To Selected Slot", "Save States",   "Hotkeys", "SaveStateToSlot",           "Keyboard/F1"},
        {"Load State From Selected Slot","Save States",  "Hotkeys", "LoadStateFromSlot",         "Keyboard/F3"},
        {"Load Backup State",           "Save States",   "Hotkeys", "LoadBackupStateFromSlot",   ""},
        {"Save State and Select Next Slot","Save States", "Hotkeys","SaveStateAndSelectNextSlot", ""},
        {"Select Next Slot and Save State","Save States", "Hotkeys","SelectNextSlotAndSaveState", ""},
        {"Save State To Slot 1",        "Save States",   "Hotkeys", "SaveStateToSlot1",          ""},
        {"Load State From Slot 1",      "Save States",   "Hotkeys", "LoadStateFromSlot1",        ""},
        {"Save State To Slot 2",        "Save States",   "Hotkeys", "SaveStateToSlot2",          ""},
        {"Load State From Slot 2",      "Save States",   "Hotkeys", "LoadStateFromSlot2",        ""},
        {"Save State To Slot 3",        "Save States",   "Hotkeys", "SaveStateToSlot3",          ""},
        {"Load State From Slot 3",      "Save States",   "Hotkeys", "LoadStateFromSlot3",        ""},
        {"Save State To Slot 4",        "Save States",   "Hotkeys", "SaveStateToSlot4",          ""},
        {"Load State From Slot 4",      "Save States",   "Hotkeys", "LoadStateFromSlot4",        ""},
        {"Save State To Slot 5",        "Save States",   "Hotkeys", "SaveStateToSlot5",          ""},
        {"Load State From Slot 5",      "Save States",   "Hotkeys", "LoadStateFromSlot5",        ""},
        {"Save State To Slot 6",        "Save States",   "Hotkeys", "SaveStateToSlot6",          ""},
        {"Load State From Slot 6",      "Save States",   "Hotkeys", "LoadStateFromSlot6",        ""},
        {"Save State To Slot 7",        "Save States",   "Hotkeys", "SaveStateToSlot7",          ""},
        {"Load State From Slot 7",      "Save States",   "Hotkeys", "LoadStateFromSlot7",        ""},
        {"Save State To Slot 8",        "Save States",   "Hotkeys", "SaveStateToSlot8",          ""},
        {"Load State From Slot 8",      "Save States",   "Hotkeys", "LoadStateFromSlot8",        ""},
        {"Save State To Slot 9",        "Save States",   "Hotkeys", "SaveStateToSlot9",          ""},
        {"Load State From Slot 9",      "Save States",   "Hotkeys", "LoadStateFromSlot9",        ""},
        {"Save State To Slot 10",       "Save States",   "Hotkeys", "SaveStateToSlot10",         ""},
        {"Load State From Slot 10",     "Save States",   "Hotkeys", "LoadStateFromSlot10",       ""},

        // ── Audio ──
        {"Toggle Mute",                 "Audio",         "Hotkeys", "Mute",              ""},
        {"Increase Volume",             "Audio",         "Hotkeys", "IncreaseVolume",    ""},
        {"Decrease Volume",             "Audio",         "Hotkeys", "DecreaseVolume",    ""},
    };
}

// ============================================================================
// Controller types and per-type bindings/settings
// ============================================================================

QVector<ControllerTypeDef> PCSX2Adapter::controllerTypes() const {
    return {
        {"NotConnected", "Not Connected", ""},
        {"DualShock2",   "DualShock 2",   ":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg"},
        {"Guitar",       "Guitar",        ":/AppUI/qml/AppUI/images/controllers/Guitar.svg"},
        {"Jogcon",       "Jogcon",        ":/AppUI/qml/AppUI/images/controllers/Jogcon.svg"},
        {"Negcon",       "NeGcon",        ":/AppUI/qml/AppUI/images/controllers/Negcon.svg"},
        {"Popn",         "Pop'n Music",   ":/AppUI/qml/AppUI/images/controllers/Popn.svg"},
    };
}

QVector<BindingDef> PCSX2Adapter::controllerBindingDefsForType(const QString& type) const {
    if (type == "DualShock2")
        return controllerBindingDefs(); // existing DS2 bindings

    if (type == "Guitar") {
        return {
            {BindingDef::Button, "Strum Up",    "Strum",   "Pad", "Up",     "SDL-0/DPadUp"},
            {BindingDef::Button, "Strum Down",  "Strum",   "Pad", "Down",   "SDL-0/DPadDown"},
            {BindingDef::Button, "Select",      "System",  "Pad", "Select", "SDL-0/Back"},
            {BindingDef::Button, "Start",       "System",  "Pad", "Start",  "SDL-0/Start"},
            {BindingDef::Button, "Green",       "Frets",   "Pad", "Green",  "SDL-0/FaceSouth"},
            {BindingDef::Button, "Red",         "Frets",   "Pad", "Red",    "SDL-0/FaceEast"},
            {BindingDef::Button, "Yellow",      "Frets",   "Pad", "Yellow", "SDL-0/FaceNorth"},
            {BindingDef::Button, "Blue",        "Frets",   "Pad", "Blue",   "SDL-0/FaceWest"},
            {BindingDef::Button, "Orange",      "Frets",   "Pad", "Orange", "SDL-0/LeftShoulder"},
            {BindingDef::Axis,   "Whammy",      "Analog",  "Pad", "Whammy", "SDL-0/+LeftY"},
            {BindingDef::Button, "Tilt",        "Analog",  "Pad", "Tilt",   "SDL-0/LeftTrigger"},
        };
    }

    if (type == "Jogcon") {
        return {
            {BindingDef::Button, "Up",       "D-Pad",        "Pad", "Up",        "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",     "D-Pad",        "Pad", "Down",      "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",     "D-Pad",        "Pad", "Left",      "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right",    "D-Pad",        "Pad", "Right",     "SDL-0/DPadRight"},
            {BindingDef::Button, "Triangle", "Face Buttons", "Pad", "Triangle",  "SDL-0/FaceNorth"},
            {BindingDef::Button, "Circle",   "Face Buttons", "Pad", "Circle",    "SDL-0/FaceEast"},
            {BindingDef::Button, "Cross",    "Face Buttons", "Pad", "Cross",     "SDL-0/FaceSouth"},
            {BindingDef::Button, "Square",   "Face Buttons", "Pad", "Square",    "SDL-0/FaceWest"},
            {BindingDef::Button, "Select",   "System",       "Pad", "Select",    "SDL-0/Back"},
            {BindingDef::Button, "Start",    "System",       "Pad", "Start",     "SDL-0/Start"},
            {BindingDef::Button, "L1",       "Shoulders",    "Pad", "L1",        "SDL-0/LeftShoulder"},
            {BindingDef::Button, "L2",       "Shoulders",    "Pad", "L2",        "SDL-0/LeftTrigger"},
            {BindingDef::Button, "R1",       "Shoulders",    "Pad", "R1",        "SDL-0/RightShoulder"},
            {BindingDef::Button, "R2",       "Shoulders",    "Pad", "R2",        "SDL-0/RightTrigger"},
            {BindingDef::Axis,   "Dial Left",  "Dial",       "Pad", "DialLeft",  "SDL-0/-LeftX"},
            {BindingDef::Axis,   "Dial Right", "Dial",       "Pad", "DialRight", "SDL-0/+LeftX"},
            {BindingDef::Axis,   "LargeMotor",  "Motors", "Pad", "LargeMotor", ""},
            {BindingDef::Axis,   "SmallMotor",  "Motors", "Pad", "SmallMotor", ""},
        };
    }

    if (type == "Negcon") {
        return {
            {BindingDef::Button, "Up",    "D-Pad",        "Pad", "Up",         "SDL-0/DPadUp"},
            {BindingDef::Button, "Down",  "D-Pad",        "Pad", "Down",       "SDL-0/DPadDown"},
            {BindingDef::Button, "Left",  "D-Pad",        "Pad", "Left",       "SDL-0/DPadLeft"},
            {BindingDef::Button, "Right", "D-Pad",        "Pad", "Right",      "SDL-0/DPadRight"},
            {BindingDef::Button, "A",     "Face Buttons",  "Pad", "A",          "SDL-0/FaceSouth"},
            {BindingDef::Button, "B",     "Face Buttons",  "Pad", "B",          "SDL-0/FaceEast"},
            {BindingDef::Button, "I",     "Face Buttons",  "Pad", "I",          "SDL-0/FaceWest"},
            {BindingDef::Button, "II",    "Face Buttons",  "Pad", "II",         "SDL-0/FaceNorth"},
            {BindingDef::Button, "Start", "System",        "Pad", "Start",      "SDL-0/Start"},
            {BindingDef::Button, "L",     "Shoulders",     "Pad", "L",          "SDL-0/LeftShoulder"},
            {BindingDef::Button, "R",     "Shoulders",     "Pad", "R",          "SDL-0/RightShoulder"},
            {BindingDef::Axis,   "Twist Left",  "Twist",   "Pad", "TwistLeft",  "SDL-0/-LeftX"},
            {BindingDef::Axis,   "Twist Right", "Twist",   "Pad", "TwistRight", "SDL-0/+LeftX"},
            {BindingDef::Axis,   "LargeMotor",  "Motors",  "Pad", "LargeMotor", ""},
            {BindingDef::Axis,   "SmallMotor",  "Motors",  "Pad", "SmallMotor", ""},
        };
    }

    if (type == "Popn") {
        return {
            {BindingDef::Button, "Yellow Left",  "Buttons", "Pad", "YellowL",  ""},
            {BindingDef::Button, "Yellow Right", "Buttons", "Pad", "YellowR", ""},
            {BindingDef::Button, "Blue Left",    "Buttons", "Pad", "BlueL",    ""},
            {BindingDef::Button, "Blue Right",   "Buttons", "Pad", "BlueR",   ""},
            {BindingDef::Button, "White Left",   "Buttons", "Pad", "WhiteL",   ""},
            {BindingDef::Button, "White Right",  "Buttons", "Pad", "WhiteR",  ""},
            {BindingDef::Button, "Green Left",   "Buttons", "Pad", "GreenL",   ""},
            {BindingDef::Button, "Green Right",  "Buttons", "Pad", "GreenR",  ""},
            {BindingDef::Button, "Red",          "Buttons", "Pad", "Red",         ""},
            {BindingDef::Button, "Start",        "System",  "Pad", "Start",       "SDL-0/Start"},
            {BindingDef::Button, "Select",       "System",  "Pad", "Select",      "SDL-0/Back"},
        };
    }

    // NotConnected or unknown
    return {};
}

QVector<SettingDef> PCSX2Adapter::controllerSettingDefsForType(const QString& type) const {
    if (type == "DualShock2")
        return controllerSettingDefs(); // existing 8 settings

    if (type == "Guitar") {
        return {
            {"", "", "", "Pad", "Deadzone",
             "Whammy Bar Deadzone", "Sets the whammy bar deadzone.",
             SettingDef::Int, "0", {}, 0, 100, 1, "", "%"},
            {"", "", "", "Pad", "AxisScale",
             "Whammy Bar Sensitivity", "Sets the whammy bar axis scaling factor.",
             SettingDef::Int, "100", {}, 0, 200, 1, "", "%"},
        };
    }

    if (type == "Jogcon") {
        return {
            {"", "", "", "Pad", "Deadzone",
             "Dial Deadzone", "Sets the dial deadzone.",
             SettingDef::Int, "0", {}, 0, 100, 1, "", "%"},
            {"", "", "", "Pad", "AxisScale",
             "Dial Sensitivity", "Sets the dial axis scaling factor.",
             SettingDef::Int, "100", {}, 0, 200, 1, "", "%"},
        };
    }

    if (type == "Negcon") {
        return {
            {"", "", "", "Pad", "Deadzone",
             "Twist Deadzone", "Sets the twist axis deadzone.",
             SettingDef::Int, "0", {}, 0, 100, 1, "", "%"},
            {"", "", "", "Pad", "AxisScale",
             "Twist Sensitivity", "Sets the twist axis scaling factor.",
             SettingDef::Int, "100", {}, 0, 200, 1, "", "%"},
        };
    }

    // Popn, NotConnected — no settings
    return {};
}

// ============================================================================
// patchRetroAchievements — enable/disable RA in PCSX2.ini
// ============================================================================

void PCSX2Adapter::patchRetroAchievements(const QString& username,
                                            const QString& token,
                                            bool enabled,
                                            bool hardcore,
                                            bool notifications,
                                            bool sounds) {
    Q_UNUSED(username);
    Q_UNUSED(token);
    // No credential patching — emulators handle their own RA login on first launch.
    // Only enable/disable RA and set preferences.
    const QString mainPath = configFilePath();
    QString mainContent;
    if (readConfigFile(mainPath, mainContent, "PCSX2")) {
        QVector<IniKeyPatch> mainPatches = {
            {"Achievements", "Enabled", enabled ? "true" : "false"},
            {"Achievements", "HardcoreMode", hardcore ? "true" : "false"},
            {"Achievements", "Notifications", notifications ? "true" : "false"},
            {"Achievements", "SoundEffects", sounds ? "true" : "false"},
        };
        if (patchIniKeys(mainContent, mainPatches))
            writeConfigFile(mainPath, mainContent, "PCSX2");
    }
}

// ============================================================================
// Asset matching — select the right GitHub release asset for this platform
// ============================================================================

QString PCSX2Adapter::matchAsset(const QStringList& assetNames) const {
    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
#if defined(Q_OS_MACOS)
        if (lower.contains("mac") && (name.endsWith(".tar.xz") || name.endsWith(".dmg")))
            return name;
#elif defined(Q_OS_WIN)
        if (lower.contains("windows") && lower.contains("x64") && name.endsWith(".zip"))
            return name;
#else
        if (name.endsWith(".AppImage"))
            return name;
#endif
    }
    return EmulatorAdapter::matchAsset(assetNames);
}

// ============================================================================
// Resume file lookup
// ============================================================================

QString PCSX2Adapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty()) return {};

    const QString pcsx2Serial = serialToFilenameFormat(serial);
    const QString statesDir = Paths::emulatorDataDir("pcsx2", "ps2") + "/savestates";
    QDir dir(statesDir);
    if (!dir.exists()) return {};

    for (const auto& entry : dir.entryList(QDir::Files)) {
        if (entry.startsWith(pcsx2Serial) && entry.endsWith(".resume.p2s")) {
            return statesDir + "/" + entry;
        }
    }
    return {};
}
