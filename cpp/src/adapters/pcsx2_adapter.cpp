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

    // The schema below mirrors PCSX2 standalone's Settings dialog category-by-
    // category, group-by-group, in the same order the panes appear in
    // pcsx2-qt/Settings/SettingsWindow.cpp (top-level categories) and the
    // matching *Widget.cpp / *.ui files for each pane. See
    // docs/new-adapter-checklist.md "Mirroring the upstream UI verbatim".
    //
    // Categories we deliberately omit from the visible schema:
    //   - Interface  : controls PCSX2's own UI which RetroNest hides; the
    //                  embedding-critical UI keys are force-patched in
    //                  patchExistingConfig.
    //   - Game List  : RetroNest manages ROM scanning.
    //   - Patches    : per-game-only upstream — out of scope.
    //   - Cheats     : per-game-only upstream — out of scope.
    //   - Game Fixes : per-game-only upstream — out of scope.
    //   - Folders    : RetroNest manages every emulator's data layout.
    //
    // Settings deferred (need infrastructure we don't have yet) are documented
    // in user memory file pcsx2-schema-alignment.md.

    // ═══════════════════════════════════════════════════════════════════════
    // BIOS  (mirrors BIOSSettingsWidget — only the boot-behaviour toggles;
    // the BIOS-file picker + folder are RetroNest-managed via the wizard.)
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"BIOS", "", "Options", "EmuCore", "EnableFastBoot", "Fast Boot",
                     "Skips the PS2 BIOS splash screen when booting a game.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"BIOS", "", "Options", "EmuCore", "EnableFastBootFastForward", "Fast Forward Boot",
                     "Force-fast-forwards through the BIOS boot sequence after Fast Boot.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "EnableFastBoot";
        s.append(d);
    }

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
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Speed Control", "Framerate", "TurboScalar", "Fast-Forward Speed",
                  "Sets the target speed when turbo mode is activated.", SettingDef::Combo, "2", speedOptions, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Speed Control", "Framerate", "SlomoScalar", "Slow-Motion Speed",
                  "Sets the target speed when slow motion mode is activated.", SettingDef::Combo, "0.5", speedOptions, 0, 0, 0};
        s.append(d);
    }

    // ── System Settings ─────────────────────────────────────────────────
    // Upstream order: EECycleRate, EECycleSkip, vuThread, EnableThreadPinning,
    // EnableCheats, HostFs, CdvdPrecache. Per-game-only fastCDVD is omitted.
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "EECycleRate", "EE Cycle Rate",
                  "Underclocks or overclocks the emulated Emotion Engine CPU.",
                  SettingDef::Combo, "0", {
                      {"50% (Underclock)", "-3"}, {"60% (Underclock)", "-2"}, {"75% (Underclock)", "-1"},
                      {"100% (Normal Speed)", "0"},
                      {"130% (Overclock)", "1"}, {"180% (Overclock)", "2"}, {"300% (Overclock)", "3"}
                  }, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "EECycleSkip", "EE Cycle Skipping",
                  "Makes the emulated Emotion Engine skip cycles.",
                  SettingDef::Combo, "0", {
                      {"Disabled", "0"}, {"Mild Underclock", "1"}, {"Moderate Underclock", "2"}, {"Maximum Underclock", "3"}
                  }, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "vuThread", "Enable Multithreaded VU1 (MTVU)",
                  "Runs VU1 on a second thread. Substantial speed improvement in most games.", SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "EnableThreadPinning", "Enable Thread Pinning",
                  "Pins emulation threads to specific CPU cores for improved performance.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "EnableCheats", "Enable Cheats",
                  "Enables loading cheats from pnach files.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "HostFs", "Enable Host Filesystem",
                  "Enables access to the host filesystem from the emulated PS2.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "CdvdPrecache", "Enable CDVD Precaching",
                  "Loads the disc image into RAM before starting. Can reduce stutter but uses more memory.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }

    // ── Frame Pacing / Latency Control ─────────────────────────────────
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "VsyncQueueSize", "Maximum Frame Latency",
                  "Sets the number of frames that can be queued up before the CPU waits. Set to 0 for optimal frame pacing.",
                  SettingDef::Combo, "2", {
                      {"Optimal (Frame Pacing)", "0"}, {"1 frame", "1"}, {"2 frames", "2"}, {"3 frames", "3"}
                  }, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "SyncToHostRefreshRate", "Sync to Host Refresh Rate",
                  "Adjusts emulation speed slightly to match your monitor's refresh rate.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "VsyncEnable", "Vertical Sync (VSync)",
                  "Synchronizes frame output with the monitor to prevent screen tearing.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "UseVSyncForTiming", "Use Host VSync Timing",
                  "Uses the host's VSync timing instead of the emulated console's timing.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "VsyncEnable && SyncToHostRefreshRate";
        s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "SkipDuplicateFrames", "Skip Presenting Duplicate Frames",
                  "Skips presenting frames that are identical to the previous frame.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Display
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "Renderer", "Renderer",
                     "Selects which backend PCSX2 uses to render frames. Auto picks the best option for your GPU; Vulkan and Metal are fastest on modern hardware, OpenGL is the most compatible, Software emulates the GS on CPU for perfect accuracy.",
                     SettingDef::Combo, "-1",
                     {{"Auto", "-1"}, {"OpenGL", "12"}, {"Vulkan", "14"},
#if defined(Q_OS_MACOS)
                      {"Metal", "17"},
#endif
                      {"Software", "13"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "AspectRatio", "Aspect Ratio",
                     "Controls the aspect ratio of the emulated display. Auto selects 4:3 for interlaced games and 3:2 for progressive games. 16:9 stretches the image for widescreen TVs; Stretch fills the whole window.",
                     SettingDef::Combo, "4:3",
                     {{"Auto 4:3/3:2", "Auto 4:3/3:2"}, {"4:3", "4:3"}, {"16:9", "16:9"},
                      {"10:7", "10:7"}, {"Stretch", "Stretch"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "FMVAspectRatioSwitch", "FMV Aspect Ratio Override",
                     "Overrides the aspect ratio only while full-motion video (FMV) is playing. Useful for games with widescreen cutscenes inside a 4:3 main game.",
                     SettingDef::Combo, "Off",
                     {{"Off (Default)", "Off"}, {"Auto Standard (4:3 Interlaced / 3:2 Progressive)", "Auto 4:3/3:2"},
                      {"Standard (4:3)", "4:3"}, {"Widescreen (16:9)", "16:9"}, {"Native/Full (10:7)", "10:7"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "deinterlace_mode", "Deinterlacing",
                     "Selects how interlaced frames are combined for progressive display. Automatic picks the best option per game; Weave preserves detail at the cost of combing; Bob and Blend smooth motion at the cost of vertical resolution.",
                     SettingDef::Combo, "0",
                     {{"Automatic", "0"}, {"Off", "1"}, {"Weave (Top)", "2"}, {"Weave (Bottom)", "3"},
                      {"Bob (Top)", "4"}, {"Bob (Bottom)", "5"}, {"Blend (Top)", "6"}, {"Blend (Bottom)", "7"},
                      {"Adaptive (Top)", "8"}, {"Adaptive (Bottom)", "9"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "linear_present_mode", "Bilinear Filtering",
                     "Applies a bilinear filter when scaling the final image to the window. Smooth is the standard option; Sharp uses a pixel-art-friendly variant that keeps edges crisp.",
                     SettingDef::Combo, "1",
                     {{"None", "0"}, {"Bilinear (Smooth)", "1"}, {"Bilinear (Sharp)", "2"}}, 0, 0, 0};
        s.append(d);
    }

    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "StretchY", "Vertical Stretch",
                     "Multiplies the display height after aspect-ratio fitting. Values above 100% make the image taller than its letterbox; values below leave extra vertical space. Default is 100%.",
                     SettingDef::Int, "100", {}, 10, 300, 1, "", "%"};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "CropLeft", "Left",
                     "Trims pixels from the left edge of the source image before it's fit to the display window. Useful for games with garbage pixels at the border.",
                     SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "CropTop", "Top",
                     "Trims pixels from the top edge of the source image before it's fit to the display window. Useful for games with garbage pixels at the border.",
                     SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "CropRight", "Right",
                     "Trims pixels from the right edge of the source image before it's fit to the display window. Useful for games with garbage pixels at the border.",
                     SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "CropBottom", "Bottom",
                     "Trims pixels from the bottom edge of the source image before it's fit to the display window. Useful for games with garbage pixels at the border.",
                     SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"};
        s.append(d);
    }
    // Display checkboxes
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore", "EnableWideScreenPatches", "Apply Widescreen Patches",
                     "Automatically applies community widescreen patches to supported games. Reshapes the rendering to true 16:9 instead of stretching the 4:3 picture.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore", "EnableNoInterlacingPatches", "Apply No-Interlacing Patches",
                     "Automatically applies community no-interlacing patches to supported games. Removes flicker in games that render in interlaced mode.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "pcrtc_antiblur", "Anti-Blur",
                     "Enables internal anti-blur hacks that remove the PS2's GS smear on commonly-affected games. Safe to leave on.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "IntegerScaling", "Integer Scaling",
                     "Snaps the rendered image to an integer multiple of the source pixel size. Produces crisp pixel-art scaling at the cost of leaving letterbox bars.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "pcrtc_offsets", "Screen Offsets",
                     "Enables PCRTC offsets so the screen is positioned exactly where the game requests. Fixes games that deliberately offset the viewport.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "disable_interlace_offset", "Disable Interlace Offset",
                     "Disables the half-pixel interlace offset which can reduce jitter on some games that render at half vertical resolution.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "pcrtc_overscan", "Show Overscan",
                     "Shows the overscan area of the display that would normally be hidden by a CRT bezel. Exposes any garbage the game draws outside the safe area.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }


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
        s.append(d);
    }
    // Internal Resolution / blending / dithering / filtering — used by both
    // HW and SW renderers; no per-renderer gate.
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "filter", "Texture Filtering",
                     "Controls how textures are sampled when rendered. Bilinear (PS2) matches the original hardware behavior; Forced options ignore the game's preference.",
                     SettingDef::Combo, "2",
                     {{"Nearest", "0"}, {"Bilinear (Forced)", "1"}, {"Bilinear (PS2)", "2"}, {"Bilinear (Forced excluding sprite)", "3"}}, 0, 0, 0};
        // Disabled when trilinear filtering forces a value (PS2 trilinear or
        // Forced trilinear) — mirrors GraphicsSettingsWidget::onTrilinearChanged.
        d.dependsOn = "TriFilter!=2 && TriFilter!=3";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "TriFilter", "Trilinear Filtering",
                     "Enables trilinear filtering for smoother transitions between mipmap levels. Auto leaves this decision to each game.",
                     SettingDef::Combo, "-1",
                     {{"Auto (Default)", "-1"}, {"Off", "0"}, {"Trilinear (PS2)", "1"}, {"Trilinear (Forced)", "2"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "MaxAnisotropy", "Anisotropic Filtering",
                     "Improves texture clarity at oblique viewing angles. Low cost on modern GPUs and generally safe to raise.",
                     SettingDef::Combo, "0",
                     {{"Off", "0"}, {"2x", "2"}, {"4x", "4"}, {"8x", "8"}, {"16x", "16"}}, 0, 0, 0};
        // Upstream disables anisotropic filtering when GPU palette conversion
        // is on (paltex), since palette-mode textures bypass HW filtering.
        d.dependsOn = "!paltex";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "dithering_ps2", "Dithering",
                     "Controls how PS2 dithering patterns are applied to upscaled rendering. Unscaled matches the original appearance.",
                     SettingDef::Combo, "2",
                     {{"Off", "0"}, {"Scaled", "1"}, {"Unscaled (Default)", "2"}, {"Force 32bit", "3"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "accurate_blending_unit", "Blending Accuracy",
                     "Controls how accurately PS2 blending operations are emulated. Higher levels improve compatibility with heavy effects at a performance cost.",
                     SettingDef::Combo, "1",
                     {{"Minimum", "0"}, {"Basic (Default)", "1"}, {"Medium", "2"}, {"High", "3"}, {"Full", "4"}, {"Maximum", "5"}}, 0, 0, 0};
        s.append(d);
    }

    // ── Hardware Rendering Options (Renderer != Software) ─────────────
    // Mirrors upstream's "Hardware Rendering Options" grid in
    // GraphicsHardwareRenderingSettingsTab.ui. Manual Hardware Renderer Fixes
    // (UserHacks) is upstream-gated to per-game/dev — omitted globally.
    {
        SettingDef d{"Graphics", "Rendering", "Hardware Rendering Options", "EmuCore/GS", "hw_mipmap", "Mipmapping",
                     "Enables mipmapping which improves texture quality at the cost of performance.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "Hardware Rendering Options", "EmuCore/GS", "HWAccurateAlphaTest", "Accurate Alpha Test",
                     "Emulates the PS2's alpha test more accurately. Fixes some games that have transparent geometry artifacts.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "Hardware Rendering Options", "EmuCore/GS", "HWAA1", "AA1",
                     "Enables PS2 Anti-Aliasing (AA1). Fixes anti-aliased lines and triangles in some games.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }

    // ── Software Renderer (Renderer == Software) ─────────────────────
    // Mirrors GraphicsSoftwareRenderingSettingsTab.ui. Texture Filtering is
    // shared with the HW combo above (same INI key) — no separate entry.
    {
        SettingDef d{"Graphics", "Rendering", "Software Renderer", "EmuCore/GS", "extrathreads", "Software Rendering Threads",
                     "Number of additional worker threads used by the software renderer. Higher counts can improve framerate on multi-core CPUs.",
                     SettingDef::Int, "2", {}, 0, 32, 1};
        d.dependsOn = "Renderer=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "Software Renderer", "EmuCore/GS", "autoflush_sw", "Auto Flush",
                     "Forces the software renderer to flush after every draw call. Slightly improves accuracy at the cost of performance.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "Renderer=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "Software Renderer", "EmuCore/GS", "mipmap", "Mipmapping",
                     "Enables mipmapping for the software renderer.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "Renderer=13";
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Hardware Fixes  (Renderer != Software)
    // Mirrors GraphicsHardwareFixesSettingsTab.ui — verbatim group + order.
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_CPUSpriteRenderBW", "CPU Sprite Render Size",
                     "Forces the CPU to render sprites whose width is below this threshold. Helps with depth/colour issues in some games.",
                     SettingDef::Combo, "0",
                     {{"0 (Disabled)", "0"}, {"1 (64 Max Width)", "1"}, {"2 (128 Max Width)", "2"},
                      {"3 (192 Max Width)", "3"}, {"4 (256 Max Width)", "4"}, {"5 (320 Max Width)", "5"},
                      {"6 (384 Max Width)", "6"}, {"7 (448 Max Width)", "7"}, {"8 (512 Max Width)", "8"},
                      {"9 (576 Max Width)", "9"}, {"10 (640 Max Width)", "10"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_CPUSpriteRenderLevel", "CPU Sprite Render Level",
                     "Selects which primitive types the CPU sprite render fix applies to.",
                     SettingDef::Combo, "0",
                     {{"Sprites Only", "0"}, {"Sprites/Triangles", "1"}, {"Blended Sprites/Triangles", "2"}}, 0, 0, 0};
        d.dependsOn = "UserHacks_CPUSpriteRenderBW!=0 && Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_CPUCLUTRender", "Software CLUT Render",
                     "Enables CPU rendering of CLUT (palette) effects. Fixes specific palette-based effects in some games.",
                     SettingDef::Combo, "0",
                     {{"0 (Disabled)", "0"}, {"1 (Normal)", "1"}, {"2 (Aggressive)", "2"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_GPUTargetCLUTMode", "GPU Target CLUT",
                     "Reads CLUT values from a render target on the GPU. Fixes palette-based effects in some games.",
                     SettingDef::Combo, "0",
                     {{"Disabled (Default)", "0"}, {"Enabled (Exact Match)", "1"}, {"Enabled (Check Inside Target)", "2"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_AutoFlushLevel", "Auto Flush",
                     "Forces a GS flush at the start of every primitive. Improves accuracy at a performance cost.",
                     SettingDef::Combo, "0",
                     {{"Disabled (Default)", "0"}, {"Enabled (Sprites Only)", "1"}, {"Enabled (All Primitives)", "2"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_TextureInsideRt", "Texture Inside RT",
                     "Allows textures to be sampled from inside an active render target. Fixes effects that rely on render-target sampling.",
                     SettingDef::Combo, "0",
                     {{"Disabled (Default)", "0"}, {"Inside Target", "1"}, {"Merge Targets", "2"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_Limit24BitDepth", "Limit Depth to 24 Bits",
                     "Limits 32-bit depth buffer reads to 24 bits. Fixes some games that misuse 32-bit depth values.",
                     SettingDef::Combo, "0",
                     {{"Disabled (Default)", "0"}, {"Prioritize Upper Bits", "1"}, {"Prioritize Lower Bits", "2"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_SkipDraw_Start", "Skip Draw Range Start",
                     "First draw call to skip when the Skip Draw Range hack is active.",
                     SettingDef::Int, "0", {}, 0, 10000, 1, "paired", ""};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "", "EmuCore/GS", "UserHacks_SkipDraw_End", "Skip Draw Range End",
                     "Last draw call to skip when the Skip Draw Range hack is active.",
                     SettingDef::Int, "0", {}, 0, 10000, 1, "paired", ""};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    // Hardware Fixes grid (10 boolean toggles per upstream's HW fixes pane).
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "UserHacks_DisableDepthSupport", "Disable Depth Conversion",
                     "Disables depth-buffer conversion. Improves performance on weak GPUs at the cost of accuracy.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "UserHacks_CPU_FB_Conversion", "Framebuffer Conversion",
                     "Converts frame buffers on the CPU. Fixes specific games that misuse frame-buffer formats.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "UserHacks_DisablePartialInvalidation", "Disable Partial Source Invalidation",
                     "Skips partial texture-cache invalidation. May fix specific texture corruption.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "paltex", "GPU Palette Conversion",
                     "Performs palette texture conversion on the GPU instead of the CPU.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "UserHacks_Disable_Safe_Features", "Disable Safe Features",
                     "Disables several safety features in the renderer. Faster but may cause graphical glitches.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "preload_frame_with_gs_data", "Preload Frame Data",
                     "Preloads frame data so games that read from previous frames render correctly.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "UserHacks_DisableRenderFixes", "Disable Render Fixes",
                     "Disables built-in render fixes that work around hardware quirks.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "UserHacks_ReadTCOnClose", "Read Targets When Closing",
                     "Reads back render targets to system memory when closed. Helps games that rely on previous frame contents.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "UserHacks_EstimateTextureRegion", "Estimate Texture Region",
                     "Estimates the active texture region from primitive bounds. Reduces over-fetch in some games.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Hardware Fixes", "Hardware Fixes", "EmuCore/GS", "UserHacks_DrawBuffering", "Draw Buffering",
                     "Buffers draw calls to reduce GPU state changes.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Upscaling Fixes  (Renderer != Software)
    // Mirrors GraphicsUpscalingFixesSettingsTab.ui.
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "", "EmuCore/GS", "UserHacks_HalfPixelOffset", "Half Pixel Offset",
                     "Adjusts texture sampling by half a pixel. Fixes blurry textures in upscaled rendering.",
                     SettingDef::Combo, "0",
                     {{"Off (Default)", "0"}, {"Normal (Vertex)", "1"}, {"Special (Texture)", "2"},
                      {"Special (Texture - Aggressive)", "3"}, {"Align to Native", "4"},
                      {"Align to Native - with Texture Offset", "5"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "", "EmuCore/GS", "UserHacks_native_scaling", "Native Scaling",
                     "Renders at native resolution where the game expects it.",
                     SettingDef::Combo, "0",
                     {{"Off", "0"}, {"Normal", "1"}, {"Aggressive", "2"},
                      {"Normal (Maintain Upscale)", "3"}, {"Aggressive (Maintain Upscale)", "4"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "", "EmuCore/GS", "UserHacks_round_sprite_offset", "Round Sprite",
                     "Rounds sprite vertex coordinates to fix sprite alignment when upscaling.",
                     SettingDef::Combo, "0",
                     {{"Off (Default)", "0"}, {"Half", "1"}, {"Full", "2"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "", "EmuCore/GS", "UserHacks_BilinearHack", "Bilinear Dirty Upscale",
                     "Forces bilinear or nearest filtering on upscaled dirty regions.",
                     SettingDef::Combo, "0",
                     {{"Automatic (Default)", "0"}, {"Force Bilinear", "1"}, {"Force Nearest", "2"}}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "", "EmuCore/GS", "UserHacks_TCOffsetX", "Texture Offsets X",
                     "Manually shifts the texture sample position horizontally.",
                     SettingDef::Int, "0", {}, 0, 1000, 1, "paired", ""};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "", "EmuCore/GS", "UserHacks_TCOffsetY", "Texture Offsets Y",
                     "Manually shifts the texture sample position vertically.",
                     SettingDef::Int, "0", {}, 0, 1000, 1, "paired", ""};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    // Upscaling Fixes grid (4 toggles).
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "Upscaling Fixes", "EmuCore/GS", "UserHacks_align_sprite_X", "Align Sprite",
                     "Aligns sprite X coordinates to fix vertical line artifacts in some games.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "Upscaling Fixes", "EmuCore/GS", "UserHacks_NativePaletteDraw", "Unscaled Palette Texture Draws",
                     "Renders palette-mode draws at native resolution. Fixes upscaling artefacts on palette textures.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "Upscaling Fixes", "EmuCore/GS", "UserHacks_merge_pp_sprite", "Merge Sprite",
                     "Merges co-located sprite passes into one upscaled draw.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Upscaling Fixes", "Upscaling Fixes", "EmuCore/GS", "UserHacks_forceEvenSpritePosition", "Force Even Sprite Position",
                     "Snaps sprite positions to even pixel boundaries.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Texture Replacement  (Renderer != Software)
    // Mirrors GraphicsTextureReplacementSettingsTab.ui Options group. The
    // texture-search-directory picker is RetroNest-managed.
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Texture Replacement", "Options", "EmuCore/GS", "LoadTextureReplacements", "Load Textures",
                     "Loads replacement textures from the textures folder when launching a game.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Texture Replacement", "Options", "EmuCore/GS", "DumpReplaceableTextures", "Dump Textures",
                     "Dumps the game's textures to disk so they can be edited and reused as replacements.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Texture Replacement", "Options", "EmuCore/GS", "LoadTextureReplacementsAsync", "Asynchronous Texture Loading",
                     "Loads replacement textures off the main render thread.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "LoadTextureReplacements && Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Texture Replacement", "Options", "EmuCore/GS", "DumpReplaceableMipmaps", "Dump Mipmaps",
                     "Also dumps each mip level along with the base texture.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "DumpReplaceableTextures && Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Texture Replacement", "Options", "EmuCore/GS", "PrecacheTextureReplacements", "Precache Textures",
                     "Loads every replacement texture into memory at game start.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "LoadTextureReplacements && Renderer!=13";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Texture Replacement", "Options", "EmuCore/GS", "DumpTexturesWithFMVActive", "Dump FMV Textures",
                     "Includes textures used by FMV playback in the texture dump output.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "DumpReplaceableTextures && Renderer!=13";
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
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Sharpening/Anti-Aliasing", "EmuCore/GS", "CASSharpness", "Sharpness",
                     "Strength of the CAS sharpening effect. Higher values produce sharper but potentially noisier images.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "", "%"};
        d.dependsOn = "CASMode!=0";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Sharpening/Anti-Aliasing", "EmuCore/GS", "fxaa", "FXAA",
                     "Enables Fast Approximate Anti-Aliasing.", SettingDef::Bool, "false", {}, 0, 0, 0};
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
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost", "Shade Boost",
                     "Enables manual adjustment of display brightness, contrast, and saturation.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Brightness", "Brightness",
                     "Adjusts the overall brightness of the display when Shade Boost is enabled.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Contrast", "Contrast",
                     "Adjusts the contrast between dark and light areas when Shade Boost is enabled.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Saturation", "Saturation",
                     "Adjusts color saturation when Shade Boost is enabled.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Gamma", "Gamma",
                     "Adjusts gamma correction when Shade Boost is enabled.",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Media Capture
    // Mirrors GraphicsMediaCaptureSettingsTab.ui Screenshot Capture Setup
    // group. Video Recording Setup is deferred — its codec/format combos are
    // populated dynamically from FFmpeg at runtime, which our schema can't
    // express today. See pcsx2-schema-alignment.md.
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Media Capture", "Screenshot Capture Setup", "EmuCore/GS", "ScreenshotSize", "Resolution",
                     "Resolution at which screenshots are saved.",
                     SettingDef::Combo, "0",
                     {{"Display Resolution (Aspect Corrected)", "0"},
                      {"Internal Resolution (Aspect Corrected)", "1"},
                      {"Internal Resolution (No Aspect Correction)", "2"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Media Capture", "Screenshot Capture Setup", "EmuCore/GS", "ScreenshotFormat", "Format",
                     "Image format used when saving screenshots.",
                     SettingDef::Combo, "0",
                     {{"PNG", "0"}, {"JPEG", "1"}, {"WebP", "2"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Media Capture", "Screenshot Capture Setup", "EmuCore/GS", "ScreenshotQuality", "Quality",
                     "Compression quality for lossy screenshot formats. Higher = larger files / better quality.",
                     SettingDef::Int, "90", {}, 1, 100, 1, "", "%"};
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Advanced
    // Mirrors GraphicsAdvancedSettingsTab.ui (gated upstream on
    // ShouldShowAdvancedSettings — RetroNest exposes them unconditionally).
    // Windows-only entries (UseBlitSwapChain, ExclusiveFullscreenControl) are
    // omitted; FrameRateNTSC/PAL float spinboxes are deferred (see
    // pcsx2-schema-alignment.md).
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Advanced", "Advanced Options", "EmuCore/GS", "HWDownloadMode", "Hardware Download Mode",
                     "Controls how render-target readbacks are handled. Disabling readbacks improves performance but breaks games that read the GS output.",
                     SettingDef::Combo, "0",
                     {{"Accurate (Recommended)", "0"},
                      {"Disable Readbacks (Synchronize GS Thread)", "1"},
                      {"Unsynchronized (Non-Deterministic)", "2"},
                      {"Disabled (Ignore Transfers)", "3"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Advanced Options", "EmuCore/GS", "GSDumpCompression", "GS Dump Compression",
                     "Compression algorithm for GS dump files.",
                     SettingDef::Combo, "2",
                     {{"Uncompressed", "0"}, {"LZMA (xz)", "1"}, {"Zstandard (zst)", "2"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Advanced Options", "EmuCore/GS", "texture_preloading", "Texture Preloading",
                     "Controls how aggressively textures are uploaded to the GPU.",
                     SettingDef::Combo, "0",
                     {{"None", "0"}, {"Partial", "1"}, {"Full (Hash Cache)", "2"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Advanced Options", "EmuCore/GS", "OverrideTextureBarriers", "Override Texture Barriers",
                     "Forces the renderer to use or skip texture barriers regardless of GPU capability detection.",
                     SettingDef::Combo, "-1",
                     {{"Automatic (Default)", "-1"}, {"Force Disabled", "0"}, {"Force Enabled", "1"}}, 0, 0, 0};
        // Upstream disables when SW (13) or Metal (17) — both renderers don't support barriers.
        d.dependsOn = "Renderer!=13 && Renderer!=17";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Advanced Options", "EmuCore/GS", "ExtendedUpscalingMultipliers", "Extended Upscaling Multipliers",
                     "Adds Internal Resolution multipliers above 12x. Requires a powerful GPU.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Advanced Options", "EmuCore/GS", "DisableMailboxPresentation", "Disable Mailbox Presentation",
                     "Forces FIFO present mode in Vulkan. Use only if you have presentation issues.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Advanced Options", "EmuCore/GS", "HWSpinCPUForReadbacks", "Spin CPU During Readbacks",
                     "Spins the CPU while waiting for GPU readbacks to complete. Reduces input latency at the cost of power use.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Advanced Options", "EmuCore/GS", "HWSpinGPUForReadbacks", "Spin GPU During Readbacks",
                     "Issues GPU spin draws while waiting for readbacks to complete.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Debugging Options", "EmuCore/GS", "UseDebugDevice", "Use Debug Device",
                     "Creates the graphics device with debug validation enabled. Useful for graphics-driver bug reports.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Debugging Options", "EmuCore/GS", "DisableShaderCache", "Disable Shader Cache",
                     "Skips reading the on-disk shader cache. Forces shaders to be recompiled at startup.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "Advanced", "Debugging Options", "EmuCore/GS", "DisableVertexShaderExpand", "Disable Vertex Shader Expand",
                     "Disables vertex shader sprite expansion. Forces sprite expansion on the CPU instead.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // On-Screen Display  (top-level category, mirrors OSDSettingsWidget.ui)
    // ═══════════════════════════════════════════════════════════════════════
    { SettingDef d{"On-Screen Display", "", "On-Screen Display", "EmuCore/GS", "OsdScale", "OSD Scale",
                   "Global multiplier applied to every OSD overlay. 100% matches PCSX2 upstream's default size.",
                   SettingDef::Int, "100", {}, 25, 500, 25, "", "%"};
      s.append(d); }
    { SettingDef d{"On-Screen Display", "", "On-Screen Display", "EmuCore/GS", "OsdMargin", "OSD Margin",
                   "Pixel offset between the OSD elements and the screen edge.",
                   SettingDef::Int, "10", {}, 0, 100, 1, "", "px"};
      s.append(d); }
    { SettingDef d{"On-Screen Display", "", "On-Screen Display", "EmuCore/GS", "OsdMessagesPos", "OSD Messages Position",
                   "Corner where transient messages (save-state loaded, shader reload, etc.) are drawn.",
                   SettingDef::Combo, "1",
                   {{"None", "0"}, {"Top Left (Default)", "1"}, {"Top Center", "2"}, {"Top Right", "3"},
                    {"Center Left", "4"}, {"Center", "5"}, {"Center Right", "6"},
                    {"Bottom Left", "7"}, {"Bottom Center", "8"}, {"Bottom Right", "9"}}, 0, 0, 0};
      s.append(d); }
    { SettingDef d{"On-Screen Display", "", "On-Screen Display", "EmuCore/GS", "OsdPerformancePos", "OSD Performance Position",
                   "Corner where the performance stats column (FPS/Speed/CPU/GPU/etc.) is drawn.",
                   SettingDef::Combo, "3",
                   {{"None", "0"}, {"Top Left", "1"}, {"Top Center", "2"}, {"Top Right (Default)", "3"},
                    {"Center Left", "4"}, {"Center", "5"}, {"Center Right", "6"},
                    {"Bottom Left", "7"}, {"Bottom Center", "8"}, {"Bottom Right", "9"}}, 0, 0, 0};
      s.append(d); }
    { SettingDef d{"On-Screen Display", "", "On-Screen Display", "EmuCore/GS", "OsdBoldText", "OSD Text Style (Bold)",
                   "Renders OSD text in bold. Easier to read on bright scenes.",
                   SettingDef::Bool, "true", {}, 0, 0, 0};
      s.append(d); }
    // ── Performance Stats (greyed out when OSD Performance Position = None) ──
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowSpeed", "Show Speed Percentages",
                   "Displays the emulation speed as a percentage. Red below 95%, green above 105%.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowFPS", "Show FPS",
                   "Displays the current frame rate reported by the GS. Useful for spotting performance issues.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowVPS", "Show VPS",
                   "Displays vertical syncs per second — the PS2 display refresh reported by the GS.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowResolution", "Show Resolution",
                   "Displays the PS2 internal render resolution and interlacing mode.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowGSStats", "Show GS Statistics",
                   "Displays per-frame GS statistics: draw-call count, VRAM use, and a frame-time summary.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowCPU", "Show CPU Usage",
                   "Displays per-component CPU usage (EE, GS, VU).",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowGPU", "Show GPU Usage",
                   "Displays GPU usage percentage and frame time in milliseconds.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowIndicators", "Show Status Indicators",
                   "Displays icons for pause, fast-forward, slow-motion, and turbo modes in the top-right corner.",
                   SettingDef::Bool, "true", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Performance Stats", "EmuCore/GS", "OsdShowFrameTimes", "Show Frame Times",
                   "Displays a rolling graph of recent frame times to visualise stutter.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    // ── System Information (greyed out when OSD Performance Position = None) ──
    { SettingDef d{"On-Screen Display", "", "System Information", "EmuCore/GS", "OsdShowHardwareInfo", "Show Hardware Info",
                   "Displays the CPU and GPU model names as two lines in the performance column.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "System Information", "EmuCore/GS", "OsdShowVersion", "Show PCSX2 Version",
                   "Displays the PCSX2 version string in the performance column.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdPerformancePos!=0"; s.append(d); }
    // ── Settings & Inputs ────────────────────────────────────────────
    { SettingDef d{"On-Screen Display", "", "Settings & Inputs", "EmuCore/GS", "OsdShowSettings", "Show Settings",
                   "Displays a compact summary of active emulation settings in the bottom-right corner.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdMessagesPos!=0"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Settings & Inputs", "EmuCore/GS", "OsdshowPatches", "Show Patches",
                   "Appends active patches (widescreen, no-interlacing, etc.) to the settings line.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      d.dependsOn = "OsdShowSettings"; s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Settings & Inputs", "EmuCore/GS", "OsdShowInputs", "Show Inputs",
                   "Displays the current controller input state at the bottom-left corner.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Settings & Inputs", "EmuCore/GS", "OsdShowVideoCapture", "Show Video Capture Status",
                   "Displays a recording indicator while video capture is active.",
                   SettingDef::Bool, "true", {}, 0, 0, 0};
      s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Settings & Inputs", "EmuCore/GS", "OsdShowInputRec", "Show Input Recording Status",
                   "Displays an indicator while input recording is active.",
                   SettingDef::Bool, "true", {}, 0, 0, 0};
      s.append(d); }
    { SettingDef d{"On-Screen Display", "", "Settings & Inputs", "EmuCore/GS", "OsdShowTextureReplacements", "Show Texture Replacement Status",
                   "Displays an indicator when replacement textures are loaded for the current game.",
                   SettingDef::Bool, "false", {}, 0, 0, 0};
      s.append(d); }
    // ── Messages (greyed out when OSD Messages Position = None) ───────
    { SettingDef d{"On-Screen Display", "", "Messages", "EmuCore", "WarnAboutUnsafeSettings", "Warn About Unsafe Settings",
                   "Shows a startup warning if any unsafe settings are enabled.",
                   SettingDef::Bool, "true", {}, 0, 0, 0};
      d.dependsOn = "OsdMessagesPos!=0"; s.append(d); }

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
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "SyncMode", "Synchronization", "",
                     SettingDef::Combo, "TimeStretch",
                     {{"Disabled (Noisy)", "Disabled"}, {"TimeStretch (Recommended)", "TimeStretch"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "BufferMS", "Buffer Size", "",
                     SettingDef::Int, "50", {}, 10, 500, 10, "slider", "ms"};
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "OutputLatencyMS", "Output Latency", "",
                     SettingDef::Int, "20", {}, 0, 500, 5, "slider", "ms"};
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "OutputLatencyMinimal", "Minimal Output Latency",
                     "Uses the smallest possible latency value. May cause crackling.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }

    // ── Controls ──────────────────────────────────────────────────────
    {
        SettingDef d{"Audio", "", "Controls", "SPU2/Output", "StandardVolume", "Standard Volume", "",
                     SettingDef::Int, "100", {}, 0, 200, 5, "slider", "%"};
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Controls", "SPU2/Output", "FastForwardVolume", "Fast Forward Volume", "",
                     SettingDef::Int, "100", {}, 0, 200, 5, "slider", "%"};
        s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Controls", "SPU2/Output", "OutputMuted", "Mute All Sound",
                     "Mutes all audio output.", SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Memory Cards
    // Mirrors MemoryCardSettingsWidget upstream — Slot 1/2 enable + filename
    // entries, plus the Multitap toggles. The card-creation/conversion/delete
    // dialogs and drag-drop card management are deferred (modal sub-dialogs
    // and a custom QListWidget). See pcsx2-schema-alignment.md.
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Memory Cards", "", "Memory Card Slots", "MemoryCards", "Slot1_Enable", "Slot 1 Enable",
                     "Inserts a virtual memory card into Slot 1.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "Memory Card Slots", "MemoryCards", "Slot1_Filename", "Slot 1 Filename",
                     "Filename of the memory card image used in Slot 1 (relative to the memcards folder).",
                     SettingDef::String, "Mcd001.ps2", {}, 0, 0, 0};
        d.dependsOn = "Slot1_Enable";
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "Memory Card Slots", "MemoryCards", "Slot2_Enable", "Slot 2 Enable",
                     "Inserts a virtual memory card into Slot 2.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "Memory Card Slots", "MemoryCards", "Slot2_Filename", "Slot 2 Filename",
                     "Filename of the memory card image used in Slot 2 (relative to the memcards folder).",
                     SettingDef::String, "Mcd002.ps2", {}, 0, 0, 0};
        d.dependsOn = "Slot2_Enable";
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "Multitap", "MemoryCards", "Multitap1_Slot2_Enable", "Multitap 1 - Slot 2",
                     "Enables the second memory-card slot of Multitap 1.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "Multitap", "MemoryCards", "Multitap1_Slot3_Enable", "Multitap 1 - Slot 3",
                     "Enables the third memory-card slot of Multitap 1.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "Multitap", "MemoryCards", "Multitap1_Slot4_Enable", "Multitap 1 - Slot 4",
                     "Enables the fourth memory-card slot of Multitap 1.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Network & HDD  (mirrors DEV9SettingsWidget — Ethernet + HDD basics)
    //
    // Deferred (see pcsx2-schema-alignment.md):
    //   - Ethernet Device Type / Device combos: dynamically populated from
    //     the OS network adapter list at runtime.
    //   - DNS host table (custom QTableView with row editor).
    //   - HDD "Create Image" button (modal HddCreateQt dialog).
    //   - HDD Size slider — paired with the Create button; surfaced as Int.
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Network & HDD", "", "Ethernet", "DEV9/Eth", "EthEnable", "Ethernet Enabled",
                     "Enables the emulated Network Adapter so the game can access network features.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "InterceptDHCP", "Intercept DHCP",
                     "Intercepts DHCP requests so PCSX2 can supply a static address to the emulated PS2.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "EthEnable";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "PS2IP", "PS2 Address",
                     "IPv4 address handed to the PS2 over DHCP.",
                     SettingDef::String, "0.0.0.0", {}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "AutoMask", "Subnet Mask Auto",
                     "Auto-derives the subnet mask from the host network configuration.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "Mask", "Subnet Mask",
                     "Manually configured subnet mask.",
                     SettingDef::String, "0.0.0.0", {}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP && !AutoMask";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "AutoGateway", "Gateway Auto",
                     "Auto-derives the gateway address from the host network configuration.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "Gateway", "Gateway Address",
                     "Manually configured gateway address.",
                     SettingDef::String, "0.0.0.0", {}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP && !AutoGateway";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "ModeDNS1", "DNS1 Mode",
                     "How DNS server 1 is resolved.",
                     SettingDef::Combo, "Auto",
                     {{"Manual", "Manual"}, {"Auto", "Auto"}, {"Internal", "Internal"}}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "DNS1", "DNS1 Address",
                     "Manually configured DNS server 1.",
                     SettingDef::String, "0.0.0.0", {}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP && ModeDNS1=Manual";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "ModeDNS2", "DNS2 Mode",
                     "How DNS server 2 is resolved.",
                     SettingDef::Combo, "Auto",
                     {{"Manual", "Manual"}, {"Auto", "Auto"}, {"Internal", "Internal"}}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Intercept DHCP", "DEV9/Eth", "DNS2", "DNS2 Address",
                     "Manually configured DNS server 2.",
                     SettingDef::String, "0.0.0.0", {}, 0, 0, 0};
        d.dependsOn = "InterceptDHCP && ModeDNS2=Manual";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Hard Disk Drive", "DEV9/Hdd", "HddEnable", "HDD Enabled",
                     "Enables the internal hard disk drive expansion bay.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Hard Disk Drive", "DEV9/Hdd", "HddLBA48", "Enable 48-Bit LBA",
                     "Allows HDD images larger than 137 GiB.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "HddEnable";
        s.append(d);
    }
    {
        SettingDef d{"Network & HDD", "", "Hard Disk Drive", "DEV9/Hdd", "HddFile", "HDD File",
                     "Filename of the HDD image (.raw) inside the DEV9 data folder.",
                     SettingDef::String, "DEV9hdd.raw", {}, 0, 0, 0};
        d.dependsOn = "HddEnable";
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Achievements  (mirrors AchievementSettingsWidget — preference toggles
    // only; user/token, login button, and audio-file pickers are RetroNest-
    // managed and deferred respectively. See pcsx2-schema-alignment.md.)
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Achievements", "", "Account", "Achievements", "Enabled", "Enable Achievements",
                     "Enables the RetroAchievements integration.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Settings", "Achievements", "ChallengeMode", "Enable Hardcore Mode",
                     "Disables save states, cheats, and slowdown so achievements are earned under stricter conditions.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Settings", "Achievements", "SpectatorMode", "Enable Spectator Mode",
                     "Allows other players to watch your session via the achievement servers.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Settings", "Achievements", "EncoreMode", "Enable Encore Mode",
                     "Allows previously-unlocked achievements to be earned again on subsequent playthroughs.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Settings", "Achievements", "UnofficialTestMode", "Test Unofficial Achievements",
                     "Includes unofficial / WIP achievements when checking for unlocks.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Notifications", "Achievements", "Notifications", "Show Achievement Notifications",
                     "Pops up an OSD notification when an achievement is earned.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Notifications", "Achievements", "NotificationsDuration", "Achievement Notifications Duration",
                     "How long achievement notifications stay on screen.",
                     SettingDef::Int, "5", {}, 3, 30, 1, "slider", "s"};
        d.dependsOn = "Notifications && Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Notifications", "Achievements", "LeaderboardNotifications", "Show Leaderboard Notifications",
                     "Pops up an OSD notification when a leaderboard score changes.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Notifications", "Achievements", "LeaderboardsDuration", "Leaderboard Notifications Duration",
                     "How long leaderboard notifications stay on screen.",
                     SettingDef::Int, "5", {}, 3, 30, 1, "slider", "s"};
        d.dependsOn = "LeaderboardNotifications && Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Notifications", "Achievements", "SoundEffects", "Enable Sound Effects",
                     "Plays a sound effect when an achievement unlocks.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Notifications", "Achievements", "NotificationPosition", "Notification Position",
                     "Corner where notifications appear.",
                     SettingDef::Combo, "1",
                     {{"None", "0"}, {"Top Left (Default)", "1"}, {"Top Center", "2"}, {"Top Right", "3"},
                      {"Center Left", "4"}, {"Center", "5"}, {"Center Right", "6"},
                      {"Bottom Left", "7"}, {"Bottom Center", "8"}, {"Bottom Right", "9"}}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Overlay Settings", "Achievements", "Overlays", "Enable In-Game Overlays",
                     "Renders an in-game overlay listing recent achievement progress.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Overlay Settings", "Achievements", "LBOverlays", "Enable In-Game Leaderboard Overlays",
                     "Renders an in-game overlay listing leaderboard standings.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }
    {
        SettingDef d{"Achievements", "", "Overlay Settings", "Achievements", "OverlayPosition", "Overlay Position",
                     "Corner where the in-game overlays appear.",
                     SettingDef::Combo, "8",
                     {{"Top Left", "0"}, {"Top Center", "1"}, {"Top Right", "2"},
                      {"Center Left", "3"}, {"Center", "4"}, {"Center Right", "5"},
                      {"Bottom Left", "6"}, {"Bottom Center", "7"}, {"Bottom Right (Default)", "8"}}, 0, 0, 0};
        d.dependsOn = "Enabled";
        s.append(d);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Advanced  (mirrors AdvancedSettingsWidget — CPU/VU/IOP recompiler &
    // rounding/clamping, Game Settings, Savestate, PINE.)
    //
    // Clamping Mode is a single combo writing three boolean keys via
    // saveTransform, mirroring upstream's setClampingMode helper.
    // ═══════════════════════════════════════════════════════════════════════
    const QVector<QPair<QString,QString>> roundingOptions = {
        {"Nearest", "0"}, {"Negative", "1"}, {"Positive", "2"}, {"Chop/Zero (Default)", "3"}
    };
    const QVector<QPair<QString,QString>> divRoundingOptions = {
        {"Nearest (Default)", "0"}, {"Negative", "1"}, {"Positive", "2"}, {"Chop/Zero", "3"}
    };
    // Build the Clamping Mode combo for {fpu, vu0, vu1}. Three INI keys per
    // unit (full/extra/overflow) collapse into a single 4-state combo. The
    // saveTransform writes all three; loadTransform synthesises the combo
    // value from whichever combination of bits is set on disk. Mirrors
    // AdvancedSettingsWidget::{getGlobalClampingModeIndex,setClampingMode}.
    auto makeClampingMode = [&](const QString& fullKey,
                                const QString& extraKey,
                                const QString& overflowKey) {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)",
                     "EmuCore/CPU/Recompiler",
                     fullKey,  // primary key — also used for dependency atoms
                     "Clamping Mode",
                     "Controls how out-of-range floating-point values are clamped. Higher levels are more accurate but slower.",
                     SettingDef::Combo, "Normal (Default)",
                     {{"None", "None"},
                      {"Normal (Default)", "Normal (Default)"},
                      {"Extra + Preserve Sign", "Extra + Preserve Sign"},
                      {"Full", "Full"}}, 0, 0, 0};
        d.saveTransform = [extraKey, fullKey, overflowKey](
            const QString& v, const SettingDef::SaveCallback& save) {
            const QString section = "EmuCore/CPU/Recompiler";
            // index: None=0, Normal=1, Extra=2, Full=3
            //   first  (overflow)  : index >= 1
            //   second (extra)     : index >= 2
            //   third  (full/sign) : index >= 3
            int idx = 1; // default to Normal
            if (v == "None") idx = 0;
            else if (v == "Extra + Preserve Sign") idx = 2;
            else if (v == "Full") idx = 3;
            save(section, fullKey,     idx >= 3 ? "true" : "false");
            save(section, extraKey,    idx >= 2 ? "true" : "false");
            save(section, overflowKey, idx >= 1 ? "true" : "false");
        };
        d.loadTransform = [extraKey, fullKey, overflowKey](
            const SettingDef::LoadCallback& read) -> QString {
            const QString section = "EmuCore/CPU/Recompiler";
            const auto truthy = [](const QString& s) {
                const QString l = s.toLower();
                return l == "true" || l == "1";
            };
            if (truthy(read(section, fullKey)))     return "Full";
            if (truthy(read(section, extraKey)))    return "Extra + Preserve Sign";
            if (truthy(read(section, overflowKey))) return "Normal (Default)";
            return "None";
        };
        return d;
    };

    // EmotionEngine (MIPS-IV)
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/CPU", "FPU.Roundmode", "Rounding Mode",
                     "Floating-point rounding mode used by the Emotion Engine FPU.",
                     SettingDef::Combo, "3", roundingOptions, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/CPU", "FPUDiv.Roundmode", "Division Rounding Mode",
                     "Floating-point rounding mode used for FPU division.",
                     SettingDef::Combo, "0", divRoundingOptions, 0, 0, 0};
        s.append(d);
    }
    s.append(makeClampingMode("fpuFullMode", "fpuExtraOverflow", "fpuOverflow"));
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/CPU/Recompiler", "EnableEE", "Enable Recompiler",
                     "Performs JIT translation of EE MIPS code. Required for full speed.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/Speedhacks", "WaitLoop", "Wait Loop Detection",
                     "Detects busy-wait loops and lets the CPU sleep until the next interrupt.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/CPU/Recompiler", "EnableFastmem", "Enable Fast Memory Access",
                     "Uses page-fault-driven fast memory access. Much faster but requires more memory.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/CPU/Recompiler", "EnableEECache", "Enable Cache (Slow)",
                     "Emulates the EE's instruction cache. Required by a few games but very slow.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/Speedhacks", "IntcStat", "INTC Spin Detection",
                     "Detects spin-loops on the INTC interrupt register.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/CPU/Recompiler", "PauseOnTLBMiss", "Pause On TLB Miss",
                     "Pauses emulation when a TLB miss occurs. Useful for debugging.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "EmotionEngine (MIPS-IV)", "EmuCore/CPU", "ExtraMemory", "Enable Extended RAM (Dev Console)",
                     "Doubles the EE memory to 64 MiB. Required by some homebrew that targets the dev console.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }

    // Vector Units (VU)
    {
        SettingDef d{"Advanced", "", "Vector Units (VU)", "EmuCore/CPU", "VU0.Roundmode", "VU0 Rounding Mode",
                     "Floating-point rounding mode used by VU0.",
                     SettingDef::Combo, "3", roundingOptions, 0, 0, 0};
        s.append(d);
    }
    s.append([&]{ auto d = makeClampingMode("vu0SignOverflow", "vu0ExtraOverflow", "vu0Overflow");
                  d.group = "Vector Units (VU)"; d.label = "VU0 Clamping Mode"; return d; }());
    {
        SettingDef d{"Advanced", "", "Vector Units (VU)", "EmuCore/CPU", "VU1.Roundmode", "VU1 Rounding Mode",
                     "Floating-point rounding mode used by VU1.",
                     SettingDef::Combo, "3", roundingOptions, 0, 0, 0};
        s.append(d);
    }
    s.append([&]{ auto d = makeClampingMode("vu1SignOverflow", "vu1ExtraOverflow", "vu1Overflow");
                  d.group = "Vector Units (VU)"; d.label = "VU1 Clamping Mode"; return d; }());
    {
        SettingDef d{"Advanced", "", "Vector Units (VU)", "EmuCore/CPU/Recompiler", "EnableVU0", "Enable VU0 Recompiler (Micro Mode)",
                     "Performs JIT translation of VU0 microcode.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "Vector Units (VU)", "EmuCore/CPU/Recompiler", "EnableVU1", "Enable VU1 Recompiler",
                     "Performs JIT translation of VU1 microcode.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "Vector Units (VU)", "EmuCore/Speedhacks", "vuFlagHack", "mVU Flag Hack",
                     "Skips redundant VU flag updates. Significant speedup, very high compatibility.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "Vector Units (VU)", "EmuCore/Speedhacks", "vu1Instant", "Enable Instant VU1",
                     "Runs VU1 micro-instructions instantly instead of emulating their cycle cost.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }

    // I/O Processor (IOP, MIPS-I)
    {
        SettingDef d{"Advanced", "", "I/O Processor (IOP, MIPS-I)", "EmuCore/CPU/Recompiler", "EnableIOP", "Enable Recompiler",
                     "Performs JIT translation of the IOP's MIPS-I machine code.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }

    // Game Settings
    {
        SettingDef d{"Advanced", "", "Game Settings", "EmuCore", "EnableGameFixes", "Enable Game Fixes",
                     "Automatically applies known fixes to games with compatibility quirks.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "Game Settings", "EmuCore", "EnablePatches", "Enable Compatibility Patches",
                     "Automatically applies community patches to known problematic games.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }

    // Savestate Settings
    {
        SettingDef d{"Advanced", "", "Savestate Settings", "EmuCore", "SavestateCompressionType", "Compression Method",
                     "Compression algorithm used when writing savestates.",
                     SettingDef::Combo, "2",
                     {{"Uncompressed", "0"}, {"Deflate64", "1"},
                      {"Zstandard", "2"}, {"LZMA2", "3"}}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "Savestate Settings", "EmuCore", "SavestateCompressionRatio", "Compression Level",
                     "Compression level used when writing savestates.",
                     SettingDef::Combo, "1",
                     {{"Low (Fast)", "0"}, {"Medium (Recommended)", "1"},
                      {"High", "2"}, {"Very High (Slow, Not Recommended)", "3"}}, 0, 0, 0};
        d.dependsOn = "SavestateCompressionType!=0";
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "Savestate Settings", "EmuCore", "BackupSavestate", "Create Save State Backups",
                     "Backs up the previous save state before overwriting (.backup suffix).",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "Savestate Settings", "EmuCore", "SaveStateOnShutdown", "Save State On Shutdown",
                     "Automatically saves a state when emulation is stopped, so you can resume next launch.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }

    // PINE Settings (PCSX2's IPC interface)
    {
        SettingDef d{"Advanced", "", "PINE Settings", "EmuCore", "EnablePINE", "Enable",
                     "Enables the PINE IPC interface so external tools can poke PCSX2's memory.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        s.append(d);
    }
    {
        SettingDef d{"Advanced", "", "PINE Settings", "EmuCore", "PINESlot", "Slot",
                     "TCP slot used by the PINE interface.",
                     SettingDef::Int, "28011", {}, 1, 65535, 1};
        d.dependsOn = "EnablePINE";
        s.append(d);
    }

    // Debug pane is intentionally not surfaced — see pcsx2-schema-alignment.md.

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

EmulatorAdapter::RetroAchievementsKeyMap PCSX2Adapter::retroAchievementsKeyMap() const {
    return {
        "Achievements",       // section
        "Enabled",            // enabledKey
        "HardcoreMode",       // hardcoreKey
        "Notifications",      // notificationsKey
        "SoundEffects",       // soundEffectsKey
        "true", "false",      // bool format
        "PCSX2",              // configTag
    };
}

// ============================================================================
// Asset matching — select the right GitHub release asset for this platform
// ============================================================================

QVector<EmulatorAdapter::AssetMatchRule> PCSX2Adapter::assetMatchRules() const {
#if defined(Q_OS_MACOS)
    return { {{"mac"}, ".tar.xz"}, {{"mac"}, ".dmg"} };
#elif defined(Q_OS_WIN)
    return { {{"windows", "x64"}, ".zip"} };
#else
    return { {{}, ".AppImage"} };
#endif
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

// ============================================================================
// Preview spec
// ============================================================================

PreviewSpec PCSX2Adapter::previewSpec(const QString& category,
                                       const QString& subcategory) const {
    if (category == "Graphics" && subcategory == "Display") {
        return {"aspect", {
            {"AspectRatio",          "aspectMode"},
            {"FMVAspectRatioSwitch", "fmvAspectMode"},
            {"StretchY",             "stretchY"},
            {"CropLeft",             "cropL"},
            {"CropTop",              "cropT"},
            {"CropRight",            "cropR"},
            {"CropBottom",           "cropB"},
            {"IntegerScaling",       "integerScaling"},
        }};
    }
    if (category == "On-Screen Display" && subcategory.isEmpty()) {
        return {"osd", {
            {"OsdShowFPS",                 "showFps"},
            {"OsdShowSpeed",               "showSpeed"},
            {"OsdShowVPS",                 "showVps"},
            {"OsdShowResolution",          "showResolution"},
            {"OsdShowCPU",                 "showCpu"},
            {"OsdShowGPU",                 "showGpu"},
            {"OsdShowSettings",            "showSettings"},
            {"OsdshowPatches",             "showPatches"},
            {"OsdShowInputs",              "showInputs"},
            {"OsdShowFrameTimes",          "showFrameTimes"},
            {"OsdShowIndicators",          "showIndicators"},
            {"OsdShowGSStats",             "showGsStats"},
            {"OsdShowHardwareInfo",        "showHardwareInfo"},
            {"OsdShowVersion",             "showVersion"},
            {"OsdShowVideoCapture",        "showVideoCapture"},
            {"OsdShowInputRec",            "showInputRec"},
            {"OsdShowTextureReplacements", "showTextureReplacements"},
            {"OsdMessagesPos",             "messagesPos"},
            {"OsdPerformancePos",          "performancePos"},
            {"OsdScale",                   "osdScale"},
        }};
    }
    return {};
}
