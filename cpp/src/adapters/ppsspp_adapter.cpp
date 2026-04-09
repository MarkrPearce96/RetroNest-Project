#include "ppsspp_adapter.h"
#include "core/sfo_parser.h"
#include "core/iso9660_reader.h"
#include "core/ini_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

static const char* PPSSPP_INSTALL_FOLDER = "ppsspp";

// ============================================================================
// Platform-specific config directory
// ============================================================================

QString PPSSPPAdapter::configDir() {
    // Portable memstick root — PPSSPP reads config from {memstick}/PSP/SYSTEM/
    return Paths::emulatorsDir(PPSSPP_INSTALL_FOLDER);
}

QString PPSSPPAdapter::nativeConfigDir() {
    // PPSSPP's config lives at {memstick}/PSP/SYSTEM/
    return configDir() + "/PSP/SYSTEM";
}

QString PPSSPPAdapter::iniPath() {
    // Config file that PPSSPP reads. Our settings UI also reads/writes this
    // file directly, so changes are instantly reflected in both UIs.
    return nativeConfigDir() + "/ppsspp.ini";
}

QString PPSSPPAdapter::controlsIniPath() {
    return nativeConfigDir() + "/controls.ini";
}

QString PPSSPPAdapter::configFilePath() const {
    return iniPath();
}

QString PPSSPPAdapter::controllerBindingsConfigFilePath() const {
    // PPSSPP stores controller bindings in a separate controls.ini file
    return controlsIniPath();
}

QString PPSSPPAdapter::controllerBindingsSection(int /*port*/) const {
    // PPSSPP uses a single [ControlMapping] section regardless of port
    return "ControlMapping";
}

// ============================================================================
// Settings schema
// ============================================================================

QVector<SettingDef> PPSSPPAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Emulation  (moved from top-level Emulation category)
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Emulation", "", "CPU", "FastMemoryAccess", "Fast Memory (Unstable)",
              "Uses faster but less accurate memory access. May cause crashes in some games.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Emulation", "", "General", "IgnoreBadMemAccess", "Ignore Bad Memory Accesses",
              "Silently ignores invalid memory reads/writes instead of crashing.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Emulation", "", "CPU", "IOTimingMethod", "I/O Timing Method",
              "Controls how UMD (disc) I/O timing is handled.",
              SettingDef::Combo, "0",
              {{"Fast (lag on slow storage)", "0"}, {"Host", "1"},
               {"Simulate UMD Delays", "2"}, {"Simulate UMD Slow", "3"}}, 0, 0, 0});
    s.append({"Graphics", "Emulation", "", "General", "ForceLagSync2", "Force Real Clock Sync",
              "Slower but less lag. Forces the emulator to run at real clock speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Emulation", "", "CPU", "CPUSpeed", "CPU Clock (MHz)",
              "Overclock the emulated PSP's CPU. 0 = default (222 MHz). Unstable on high values.",
              SettingDef::Int, "0", {}, 0, 1000, 1, "slider", "MHz"});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Rendering
    // ═══════════════════════════════════════════════════════════════════════
    // PPSSPP stores backend as "{int} ({NAME})" via ConfigTranslator, so our
    // combo values must match exactly for round-trip to work.
    s.append({"Graphics", "Rendering", "", "Graphics", "GraphicsBackend", "Backend",
              "Graphics API used for rendering.",
              SettingDef::Combo, "3 (VULKAN)",
              {{"OpenGL", "0 (OPENGL)"},
#if defined(Q_OS_WIN)
               {"Direct3D 11", "2 (DIRECT3D11)"},
#endif
               {"Vulkan", "3 (VULKAN)"}}, 0, 0, 0});
    s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "3",
              {{"720P", "3"}, {"1080P", "4"}, {"1440P", "6"}, {"4K", "8"}}, 0, 0, 0});
    s.append({"Graphics", "Rendering", "", "Graphics", "SoftwareRenderer", "Software Rendering (slow, accurate)",
              "Uses CPU rendering for maximum accuracy. Very slow.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Rendering", "", "Graphics", "MultiSampleLevel", "Antialiasing (MSAA)",
              "Multisample anti-aliasing level.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"},
               {"16x", "4"}, {"32x", "5"}}, 0, 0, 0});
    s.append({"Graphics", "Rendering", "", "Graphics", "ReplaceTextures", "Replace Textures",
              "Allow custom texture replacement packs.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Frame Pacing
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "VerticalSync", "VSync",
              "Synchronize rendering to display refresh rate.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameSkip", "Frame Skipping",
              "Number of frames to skip to maintain speed.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"1", "1"}, {"2", "2"}, {"3", "3"},
               {"4", "4"}, {"5", "5"}, {"6", "6"}, {"7", "7"}, {"8", "8"}}, 0, 0, 0});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "AutoFrameSkip", "Auto Frameskip",
              "Automatically skip frames to maintain speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    // PPSSPP stores iFpsLimit1 as raw FPS, not percent — native UI converts
    // user-entered percent into FPS via (percent * 60) / 100. We pre-bake the
    // FPS values into combo INI values so the user still sees percentages but
    // the INI gets the right cap. See audit 2026-04-07.
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameRate", "Alternative Speed",
              "Speed used when the alternative-speed hotkey is held.",
              SettingDef::Combo, "0",
              {{"Unlimited (No Cap)", "0"},
               {"25% (15 FPS)",  "15"},
               {"50% (30 FPS)",  "30"},
               {"75% (45 FPS)",  "45"},
               {"100% (60 FPS)", "60"},
               {"125% (75 FPS)", "75"},
               {"150% (90 FPS)", "90"},
               {"200% (120 FPS)","120"},
               {"300% (180 FPS)","180"},
               {"500% (300 FPS)","300"}},
              0, 0, 0});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameRate2", "Alternative Speed 2",
              "Second alternative speed for toggling. Same FPS-vs-percent caveat as above.",
              SettingDef::Combo, "-1",
              {{"Disabled",         "-1"},
               {"Unlimited (No Cap)", "0"},
               {"25% (15 FPS)",  "15"},
               {"50% (30 FPS)",  "30"},
               {"75% (45 FPS)",  "45"},
               {"100% (60 FPS)", "60"},
               {"125% (75 FPS)", "75"},
               {"150% (90 FPS)", "90"},
               {"200% (120 FPS)","120"},
               {"300% (180 FPS)","180"},
               {"500% (300 FPS)","300"}},
              0, 0, 0});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "RenderDuplicateFrames", "Render Duplicate Frames to 60 Hz",
              "Can make framerate smoother in games that run at lower framerates.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Performance  (two visual groups in one tab)
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Performance", "Performance", "Graphics", "InflightFrames", "Buffer Graphics Commands",
              "Faster, but adds input lag.",
              SettingDef::Combo, "3",
              {{"No buffer", "0"}, {"Up to 1", "1"}, {"Up to 2", "2"}, {"Up to 3", "3"}}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Performance", "Graphics", "HardwareTransform", "Hardware Transform",
              "Uses hardware geometry transformation. Disable only for debugging.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Performance", "Graphics", "SoftwareSkinning", "Software Skinning",
              "Combine skinned model draws on the CPU, faster in most games.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Performance", "Graphics", "HardwareTessellation", "Hardware Tessellation",
              "Uses hardware to make curves.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "SkipBufferEffects", "Skip Buffer Effects",
              "Faster, but nothing may draw in some games.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "DisableRangeCulling", "Disable Culling",
              "Disables range culling.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "SkipGPUReadbackMode", "Skip GPU Readbacks",
              "Skipping GPU readbacks is faster but may break some games.",
              SettingDef::Combo, "0",
              {{"No (Default)", "0"}, {"Skip", "1"}, {"Copy to texture", "2"}}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "TextureBackoffCache", "Lazy Texture Caching",
              "Faster, but can cause text problems in a few games.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "SplineBezierQuality", "Spline/Bezier Curves Quality",
              "Only used by some games, controls smoothness of curves.",
              SettingDef::Combo, "2",
              {{"Low", "0"}, {"Medium", "1"}, {"High (Default)", "2"}}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "BloomHack", "Lower Resolution for Effects",
              "Reduces artifacts.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"Safe", "1"}, {"Balanced", "2"}, {"Aggressive", "3"}}, 0, 0, 0});
    // NEW: Lens Flare Occlusion
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "DepthRasterMode", "Lens Flare Occlusion",
              "Controls how the depth raster is used for lens flare occlusion.",
              SettingDef::Combo, "0",
              {{"Auto", "0"}, {"Low", "1"}, {"Off", "2"}, {"Always on", "3"}}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Textures
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Textures", "", "Graphics", "TexHardwareScaling", "GPU Texture Upscaler (fast)",
              "Faster texture upscaling on the GPU.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "TexScalingType", "Upscale Type",
              "Algorithm used for texture upscaling.",
              SettingDef::Combo, "0",
              {{"xBRZ", "0"}, {"Hybrid", "1"}, {"Bicubic", "2"}, {"Hybrid+Bicubic", "3"}}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "TexScalingLevel", "Upscale Level",
              "CPU heavy - some scaling may be delayed to avoid stutter.",
              SettingDef::Combo, "1",
              {{"Off", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x", "4"}, {"5x", "5"}}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "TexDeposterize", "Deposterize",
              "Fixes visual banding glitches in upscaled textures.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "AnisotropyLevel", "Anisotropic Filtering",
              "Improves texture quality at oblique angles.",
              SettingDef::Combo, "4",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"}, {"16x", "4"}}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "TextureFiltering", "Texture Filtering",
              "Filtering applied to textures.",
              SettingDef::Combo, "1",
              {{"Auto", "1"}, {"Nearest", "2"}, {"Linear", "3"}, {"Auto Max Quality", "4"}}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "Smart2DTexFiltering", "Smart 2D Texture Filtering",
              "Smarter filtering for 2D textures.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Post-Processing  (NEW — replaces "Display layout & effects")
    // ═══════════════════════════════════════════════════════════════════════
    // Single-shader picker. PPSSPP stores the chain in [PostShaderList] as
    // PostShader1, PostShader2, ... — we expose only PostShader1 for now.
    // Values come from references/ppsspp-master/assets/shaders/defaultshaders.ini
    s.append({"Graphics", "Post-Processing", "", "PostShaderList", "PostShader1", "Post-Processing Shader",
              "Apply a post-processing effect to the rendered image.",
              SettingDef::Combo, "Off",
              {{"Off", "Off"},
               {"FXAA Antialiasing", "FXAA"},
               {"CRT (curved scanlines)", "CRT"},
               {"Natural Colors", "Natural"},
               {"Natural (No Blur)", "NaturalA"},
               {"Vignette", "Vignette"},
               {"Fake Reflections", "FakeReflections"},
               {"Bloom", "Bloom"},
               {"Bloom (no blur)", "BloomNoBlur"},
               {"Sharpen", "Sharpen"},
               {"Scanlines (flat)", "Scanlines"},
               {"Cartoon", "Cartoon"},
               {"4xHqGLSL Upscaler", "4xHqGLSL"},
               {"AA-Color", "AAColor"},
               {"Bicubic Upscaler", "UpscaleBicubic"},
               {"Spline36 Upscaler", "UpscaleSpline36"},
               {"5xBR Upscaler", "5xBR"},
               {"5xBR lv2 Upscaler", "5xBR-lv2"},
               {"Color Correction", "ColorCorrection"},
               {"PSP Color", "PSPColor"},
               {"LCD Persistence", "LCDPersistence"},
               {"Sharp Bilinear", "UpscaleSharpBilinear"},
               {"FSR-EASU", "FSR-EASU"}}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Audio — unchanged from previous schema
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Audio", "Audio playback", "", "Sound", "AudioSyncMode", "Playback Mode",
              "Audio synchronization method.",
              SettingDef::Combo, "1",
              {{"Granular", "0"}, {"Classic (lowest latency)", "1"}}, 0, 0, 0});
    s.append({"Audio", "Audio playback", "", "Sound", "FillAudioGaps", "Fill Audio Gaps",
              "Fill gaps in audio output to prevent pops.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    s.append({"Audio", "Game volume", "", "Sound", "Enable", "Enable Sound",
              "Enable audio output.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Audio", "Game volume", "", "Sound", "GameVolume", "Game Volume",
              "Master audio volume.",
              SettingDef::Int, "100", {}, 0, 100, 5, "slider", "%"});
    s.append({"Audio", "Game volume", "", "Sound", "ReverbRelativeVolume", "Reverb Volume",
              "Volume of reverb effects.",
              SettingDef::Int, "100", {}, 0, 200, 5, "slider", "%"});
    s.append({"Audio", "Game volume", "", "Sound", "AltSpeedRelativeVolume", "Alternate Speed Volume",
              "Volume when using fast-forward.",
              SettingDef::Int, "100", {}, 0, 100, 5, "slider", "%"});
    s.append({"Audio", "Game volume", "", "Sound", "AchievementVolume", "Achievement Sound Volume",
              "Volume of achievement notification sounds.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%"});

    s.append({"Audio", "UI sound", "", "General", "UISound", "UI Sound",
              "Play sounds for UI interactions.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Audio", "UI sound", "", "Sound", "UIVolume", "UI Volume",
              "Volume of UI sounds.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%"});
    s.append({"Audio", "UI sound", "", "Sound", "GamePreviewVolume", "Game Preview Volume",
              "Volume of game previews in the UI.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%"});

    s.append({"Audio", "Audio backend", "", "Sound", "AudioBufferSize", "Buffer Size",
              "Audio buffer size in samples. Smaller = less latency but more crackling risk.",
              SettingDef::Int, "256", {}, 128, 2048, 64, "slider", ""});
    s.append({"Audio", "Audio backend", "", "Sound", "AutoAudioDevice", "Use New Audio Devices Automatically",
              "Automatically switch to newly connected audio devices.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Overlay  (NEW top-level sidebar entry — bitmask checkboxes + Debug overlay)
    // ═══════════════════════════════════════════════════════════════════════
    // iShowStatusFlags is an int bitfield. Bit values come from
    // references/ppsspp-master/Core/ConfigValues.h enum ShowStatusFlags:
    //   FPS_COUNTER     = 1 << 1 = 2
    //   SPEED_COUNTER   = 1 << 2 = 4
    //   BATTERY_PERCENT = 1 << 3 = 8
    // The trailing literal-int initializer uses the new SettingDef::bitmask field.
    // PPSSPP's own default for iShowStatusFlags is 0 (verified in
    // references/ppsspp-master/Core/Config.cpp), so initialising our schema
    // default to "0" matches upstream and never silently clobbers user bits.
    s.append({"Overlay", "", "", "Graphics", "iShowStatusFlags", "Show FPS Counter",
              "Display the framerate counter in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", /*bitmask=*/2});
    s.append({"Overlay", "", "", "Graphics", "iShowStatusFlags", "Show Speed",
              "Display the emulation speed percentage in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", /*bitmask=*/4});
    s.append({"Overlay", "", "", "Graphics", "iShowStatusFlags", "Show Battery %",
              "Display the host battery percentage in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", /*bitmask=*/8});
    s.append({"Overlay", "", "", "General", "DebugOverlay", "Debug Overlay",
              "PPSSPP debug overlay. Note: PPSSPP doesn't remember this setting "
              "between runs, so it will reset to Off whenever the emulator is "
              "launched outside this app.",
              SettingDef::Combo, "0",
              {{"Off", "0"},
               {"Debug Stats", "1"},
               {"Frame Graph", "2"},
               {"Frame Timing", "3"},
               {"Control", "5"},
               {"Audio", "6"},
               {"GPU Profile", "7"},
               {"GPU Allocator", "8"},
               {"Framebuffer List", "9"}}, 0, 0, 0});

    return s;
}

// ============================================================================
// ensureConfig — create or patch ppsspp.ini + controls.ini before launch
// ============================================================================

bool PPSSPPAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                 const QString& biosPath,
                                 const QString& savesPath) {
    // Set portable memstick directory so PPSSPP reads config from our managed location.
    // On macOS, PPSSPP checks NSUserDefaults "UserPreferredMemoryStickDirectoryPath" first.
#if defined(Q_OS_MACOS)
    QProcess::execute("defaults", {"write", "org.ppsspp.ppsspp",
                                   "UserPreferredMemoryStickDirectoryPath", configDir()});
#endif

    // Ensure PSP/SYSTEM directory exists (where PPSSPP reads ppsspp.ini + controls.ini)
    const QString nativeDir = nativeConfigDir();
    if (!QDir().mkpath(nativeDir)) {
        qWarning() << "[PPSSPP] Failed to create PSP/SYSTEM directory:" << nativeDir;
        return false;
    }

    // Our managed config (for UI system — controller type, binding storage)
    const QString mainPath = iniPath();
    const bool ok = QFileInfo::exists(mainPath)
        ? patchExistingConfig(mainPath, biosPath, savesPath)
        : createDefaultConfig(mainPath, biosPath, savesPath);
    if (!ok)
        return false;

    // Sync managed config → PPSSPP's native config in PSP/SYSTEM/
    if (!syncToNativeConfig(mainPath))
        return false;

    // Remove any malformed hotkey entries in controls.ini so a previously
    // corrupted binding (e.g. "Save State = Keyboard/d") doesn't crash PPSSPP
    // on next launch.
    scrubControlsIniHotkeys();

    return true;
}

// ============================================================================
// resolveExecutable — platform-aware executable resolution
// ============================================================================

QString PPSSPPAdapter::resolveExecutable(const EmulatorManifest& manifest,
                                          const QString& installPath) {
    return resolveExecutableInDir(manifest, installPath, "PPSSPPSDL");
}

// ============================================================================
// createDefaultConfig — write only embedding-critical keys to ppsspp.ini
// ============================================================================

bool PPSSPPAdapter::createDefaultConfig(const QString& path,
                                        const QString& /*biosPath*/,
                                        const QString& /*savesPath*/) {
    // PPSSPP hardcodes every PSP subdirectory (SAVEDATA, PPSSPP_STATE,
    // SCREENSHOT, TEXTURES, Cheats, GAME, PLUGINS, SYSTEM) as literal
    // children of {memstick}/PSP/ — see Core/Util/PathUtil.cpp upstream.
    // There is no INI key to relocate any of them individually, so we only
    // write wizard suppression, fullscreen, and controller type here.
    QStringList lines = {
        "[General]",
        "FirstRun = False",
        "AutoLoadSaveState = 0",
        "EnableStateUndo = True",
        "",
        "[Graphics]",
        "FullScreen = True",
        "",
        "[Sound]",
        "Enable = True",
        "",
        "[Pad1]",
        "Type = Standard",
        "",
    };

    if (!writeConfigFile(path, lines.join("\n"), "PPSSPP"))
        return false;

    // Create default controls.ini with default bindings
    QStringList ctrlLines = {
        "[ControlMapping]",
        "Up = 10-19",
        "Down = 10-20",
        "Left = 10-21",
        "Right = 10-22",
        "Cross = 10-96,10-189",
        "Circle = 10-97,10-190",
        "Square = 10-99,10-191",
        "Triangle = 10-100,10-188",
        "Start = 10-108,10-197",
        "Select = 10-109,10-196",
        "L = 10-102,10-194",
        "R = 10-103,10-195",
        "An.Up = 10-4003",
        "An.Down = 10-4002",
        "An.Left = 10-4001",
        "An.Right = 10-4000",
        "Fast-forward = 10-4036",
        "",
    };

    return writeConfigFile(controlsIniPath(), ctrlLines.join("\n"), "PPSSPP");
}

// ============================================================================
// patchExistingConfig — fix up an existing ppsspp.ini for headless operation
// ============================================================================

bool PPSSPPAdapter::patchExistingConfig(const QString& path,
                                        const QString& /*biosPath*/,
                                        const QString& /*savesPath*/) {
    QString content;
    if (!readConfigFile(path, content, "PPSSPP"))
        return false;

    // See createDefaultConfig() — PPSSPP hardcodes all PSP subdirs under
    // {memstick}/PSP/, so we only patch wizard suppression, fullscreen,
    // and controller type.
    QVector<IniKeyPatch> patches = {
        {"General",  "FirstRun",          "False"},
        {"General",  "AutoLoadSaveState", "0"},
        {"General",  "EnableStateUndo",   "True"},
        {"Graphics", "FullScreen",        "True"},
        {"Pad1",     "Type",              "Standard"},
    };

    if (patchIniKeys(content, patches) && !writeConfigFile(path, content, "PPSSPP"))
        return false;
    return true;
}

// ============================================================================
// syncToNativeConfig — sync controller settings [Pad1] → [Control]
// ============================================================================

bool PPSSPPAdapter::syncToNativeConfig(const QString& mainIniPath) {
    // configFilePath() returns the native ppsspp.ini directly, so our settings UI
    // writes there in real-time (like PCSX2/DuckStation).
    //
    // controllerBindingsConfigFilePath() returns controls.ini directly, so the
    // controller binding UI writes there in real-time too.
    //
    // The only state that still needs syncing on launch is controller settings
    // (deadzone, sensitivity) — our UI writes them under [Pad1] but PPSSPP reads
    // them from the [Control] section in the same ppsspp.ini.
    IniFile mainIni;
    if (!mainIni.load(mainIniPath)) {
        qWarning() << "[PPSSPP] Cannot read ppsspp.ini for sync:" << mainIniPath;
        return false;
    }

    // ── Sync controller settings from [Pad1] → [Control] in ppsspp.ini ──
    bool mainChanged = false;
    for (const auto& def : controllerSettingDefs()) {
        QString val = mainIni.value("Pad1", def.key);
        if (!val.isEmpty() && mainIni.value("Control", def.key) != val) {
            mainIni.setValue("Control", def.key, val);
            mainChanged = true;
        }
    }

    if (mainChanged) {
        if (!mainIni.save(mainIniPath))
            qWarning() << "[PPSSPP] Failed to sync controller settings to:" << mainIniPath;
    }

    return true;
}

void PPSSPPAdapter::scrubControlsIniHotkeys() {
    const QString path = controlsIniPath();
    if (!QFileInfo::exists(path)) return;

    IniFile ini;
    if (!ini.load(path)) {
        qWarning() << "[PPSSPP] Cannot load controls.ini for hotkey scrub:" << path;
        return;
    }

    // PPSSPP input mapping values must match: "<int>-<int>" optionally
    // repeated with ':' (chord) or ',' (alternatives) separators.
    static const QRegularExpression validMapping(
        QStringLiteral("^\\d+-\\d+([:,]\\d+-\\d+)*$"));

    bool changed = false;
    for (const auto& def : hotkeyBindingDefs()) {
        const QString value = ini.value(def.section, def.key);
        if (value.isEmpty()) continue;
        if (!validMapping.match(value).hasMatch()) {
            qWarning().noquote()
                << "[PPSSPP] Removing malformed hotkey" << def.key
                << "=" << value << "from controls.ini";
            ini.setValue(def.section, def.key, "");
            changed = true;
        }
    }

    if (changed && !ini.save(path))
        qWarning() << "[PPSSPP] Failed to save scrubbed controls.ini:" << path;
}

// ============================================================================
// Paths, BIOS, resolution, aspect ratio
// ============================================================================

QVector<PathDef> PPSSPPAdapter::pathsDefs() const {
    // PPSSPP enforces a fixed directory layout under {memstick}/PSP/ and has
    // no INI keys to relocate individual subdirs. Returning an empty list
    // hides PPSSPP from the Paths Settings screen.
    return {};
}

QVector<BiosDef> PPSSPPAdapter::biosFiles() const {
    // PSP emulation is fully HLE — no required BIOS files
    return {
        {"ppge_atlas.zim", "PSP UI font atlas (optional)", false, ""},
    };
}

ResolutionOptions PPSSPPAdapter::resolutionOptions() const {
    return {"Graphics", "InternalResolution",
            {{"720P", "3"}, {"1080P", "4"}, {"1440P", "6"}, {"4K", "8"}},
            "3"};
}

// ============================================================================
// Controller bindings, types, settings
// ============================================================================

QVector<ControllerTypeDef> PPSSPPAdapter::controllerTypes() const {
    return {
        {"NotConnected", "Not Connected", ""},
        {"Standard",     "PSP Controller", ""},
    };
}

QVector<BindingDef> PPSSPPAdapter::controllerBindingDefs() const {
    // PPSSPP controls.ini format: {deviceId}-{keyCode}
    // Device 10 = DEVICE_ID_PAD_0 (generic gamepad)
    // Button keycodes: Android NKCODEs (19=DpadUp, 96=ButtonA, etc.)
    // Axis keycodes: 4000 + (axisId*2) + (negative ? 1 : 0)
    return {
        // D-Pad
        {BindingDef::Button, "Up",       "D-Pad",        "ControlMapping", "Up",       "10-19"},
        {BindingDef::Button, "Down",     "D-Pad",        "ControlMapping", "Down",     "10-20"},
        {BindingDef::Button, "Left",     "D-Pad",        "ControlMapping", "Left",     "10-21"},
        {BindingDef::Button, "Right",    "D-Pad",        "ControlMapping", "Right",    "10-22"},
        // Face Buttons (GameController NKCODE + raw joystick button fallback)
        // Raw buttons: NKCODE_BUTTON_1(188)=Triangle, _2(189)=Cross, _3(190)=Circle, _4(191)=Square
        {BindingDef::Button, "Cross",    "Face Buttons",  "ControlMapping", "Cross",    "10-96,10-189"},
        {BindingDef::Button, "Circle",   "Face Buttons",  "ControlMapping", "Circle",   "10-97,10-190"},
        {BindingDef::Button, "Square",   "Face Buttons",  "ControlMapping", "Square",   "10-99,10-191"},
        {BindingDef::Button, "Triangle", "Face Buttons",  "ControlMapping", "Triangle", "10-100,10-188"},
        // Triggers (raw: _7(194)=L1, _8(195)=R1)
        {BindingDef::Button, "L", "Triggers", "ControlMapping", "L", "10-102,10-194"},
        {BindingDef::Button, "R", "Triggers", "ControlMapping", "R", "10-103,10-195"},
        // System (raw: _9(196)=Select, _10(197)=Start)
        {BindingDef::Button, "Start",  "System", "ControlMapping", "Start",  "10-108,10-197"},
        {BindingDef::Button, "Select", "System", "ControlMapping", "Select", "10-109,10-196"},
        // Analog Stick (axis keycodes: 4000 + axisId*2 + negative)
        // Y-: up (4003), Y+: down (4002), X-: left (4001), X+: right (4000)
        {BindingDef::Axis, "An.Up",    "Analog Stick", "ControlMapping", "An.Up",    "10-4003"},
        {BindingDef::Axis, "An.Down",  "Analog Stick", "ControlMapping", "An.Down",  "10-4002"},
        {BindingDef::Axis, "An.Left",  "Analog Stick", "ControlMapping", "An.Left",  "10-4001"},
        {BindingDef::Axis, "An.Right", "Analog Stick", "ControlMapping", "An.Right", "10-4000"},
    };
}

QVector<SettingDef> PPSSPPAdapter::controllerSettingDefs() const {
    return {
        {"", "", "", "Control", "AnalogDeadzone",
         "Analog Deadzone", "Sets the analog stick deadzone.",
         SettingDef::Int, "15", {}, 0, 100, 1, "", "%"},

        {"", "", "", "Control", "AnalogSensitivity",
         "Analog Sensitivity", "Sets the analog stick sensitivity.",
         SettingDef::Int, "110", {}, 0, 200, 1, "", "%"},
    };
}

// ============================================================================
// Hotkeys
// ============================================================================

QVector<HotkeyDef> PPSSPPAdapter::hotkeyBindingDefs() const {
    return {
        // Speed (10-4036 = right trigger axis positive)
        {"Fast-forward",       "Speed",       "ControlMapping", "Fast-forward",  "10-4036"},
        {"Speed Toggle",       "Speed",       "ControlMapping", "SpeedToggle",   ""},
        {"Alt Speed 1",        "Speed",       "ControlMapping", "Alt speed 1",   ""},
        {"Alt Speed 2",        "Speed",       "ControlMapping", "Alt speed 2",   ""},
        {"Frame Advance",      "Speed",       "ControlMapping", "Frame Advance", ""},
        // System
        {"Rewind",             "System",      "ControlMapping", "Rewind",        ""},
        {"Screenshot",         "System",      "ControlMapping", "Screenshot",    ""},
        {"Mute Toggle",        "System",      "ControlMapping", "Mute toggle",   ""},
        {"Reset",              "System",      "ControlMapping", "Reset",         ""},
        // Save States
        {"Save State",         "Save States", "ControlMapping", "Save State",    ""},
        {"Load State",         "Save States", "ControlMapping", "Load State",    ""},
        {"Previous Slot",      "Save States", "ControlMapping", "Previous Slot", ""},
        {"Next Slot",          "Save States", "ControlMapping", "Next Slot",     ""},
    };
}

// ============================================================================
// formatBinding — PPSSPP native format
// ============================================================================

QString PPSSPPAdapter::formatBinding(int deviceIndex, const QString& element,
                                      bool isAxis, bool positive) const {
    // PPSSPP controls.ini format: {deviceId}-{keyCode}
    // Device ID: 10 + deviceIndex (DEVICE_ID_PAD_0 = 10)
    // Keycodes: Android NKCODE values for buttons, 4000-based for axes
    const int ppssppDeviceId = 10 + deviceIndex;

    // SDL button names → {GameController NKCODE, raw joystick NKCODE_BUTTON_N fallback}
    // Raw fallback uses NKCODE_BUTTON_1(188)+ series for standard controller layout
    struct ButtonCodes { int gc; int raw; };
    static const QMap<QString, ButtonCodes> buttonToNkcode = {
        {"DPadUp",         {19, -1}},  {"DPadDown",       {20, -1}},
        {"DPadLeft",       {21, -1}},  {"DPadRight",      {22, -1}},
        {"FaceSouth",      {96, 189}}, {"A",              {96, 189}},  // raw b2=189
        {"FaceEast",       {97, 190}}, {"B",              {97, 190}},  // raw b3=190
        {"FaceWest",       {99, 191}}, {"X",              {99, 191}},  // raw b4=191
        {"FaceNorth",     {100, 188}}, {"Y",             {100, 188}},  // raw b1=188
        {"LeftShoulder",  {102, 194}}, {"RightShoulder", {103, 195}},  // raw b7,b8
        {"LeftTrigger",   {104, -1}},  {"RightTrigger",  {105, -1}},
        {"LeftStick",     {106, 199}}, {"RightStick",    {107, 200}},  // raw b12,b13
        {"Start",         {108, 197}}, {"Back",          {109, 196}},  // raw b10,b9
    };

    // SDL axis names → PPSSPP axis IDs (for 4000-based encoding)
    // Formula: 4000 + (axisId * 2) + (negative ? 1 : 0)
    static const QMap<QString, int> axisToId = {
        {"LeftX",   0},    // JOYSTICK_AXIS_X
        {"LeftY",   1},    // JOYSTICK_AXIS_Y
        {"RightX", 11},    // JOYSTICK_AXIS_Z
        {"RightY", 14},    // JOYSTICK_AXIS_RZ
        {"LeftTrigger",  17},  // JOYSTICK_AXIS_LTRIGGER
        {"RightTrigger", 18},  // JOYSTICK_AXIS_RTRIGGER
    };

    if (isAxis) {
        auto it = axisToId.find(element);
        if (it != axisToId.end()) {
            int keyCode = 4000 + (it.value() * 2) + (positive ? 0 : 1);
            return QString("%1-%2").arg(ppssppDeviceId).arg(keyCode);
        }
        // Fallback for unknown axes
        return QString("%1-%2").arg(ppssppDeviceId).arg(element);
    }

    auto it = buttonToNkcode.find(element);
    if (it != buttonToNkcode.end()) {
        QString result = QString("%1-%2").arg(ppssppDeviceId).arg(it->gc);
        // Add raw joystick button fallback (comma-separated alternative)
        if (it->raw >= 0)
            result += QString(",%1-%2").arg(ppssppDeviceId).arg(it->raw);
        return result;
    }
    // Fallback for unknown buttons
    return QString("%1-%2").arg(ppssppDeviceId).arg(element);
}

// ============================================================================
// Keyboard/mouse/wheel bindings — PPSSPP uses DEVICE_ID_KEYBOARD=1 with
// Android NKCODE values. Mouse and wheel captures are not supported for
// hotkeys in this path (PPSSPP mouse bindings use a separate device id and
// we don't wire that into the hotkey capture UI).
// ============================================================================

static int qtKeyToPpssppNkcode(int qtKey) {
    // Letters: NKCODE_A(29) .. NKCODE_Z(54)
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return 29 + (qtKey - Qt::Key_A);
    // Digits: NKCODE_0(7) .. NKCODE_9(16)
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return 7 + (qtKey - Qt::Key_0);
    // Function keys: NKCODE_F1(131) .. NKCODE_F12(142)
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12)
        return 131 + (qtKey - Qt::Key_F1);
    switch (qtKey) {
        case Qt::Key_Return:       return 66;  // NKCODE_ENTER
        case Qt::Key_Enter:        return 66;
        case Qt::Key_Space:        return 62;  // NKCODE_SPACE
        case Qt::Key_Escape:       return 111; // NKCODE_ESCAPE
        case Qt::Key_Tab:          return 61;  // NKCODE_TAB
        case Qt::Key_Backspace:    return 67;  // NKCODE_DEL
        case Qt::Key_Delete:       return 112; // NKCODE_FORWARD_DEL
        case Qt::Key_Up:           return 19;  // NKCODE_DPAD_UP
        case Qt::Key_Down:         return 20;  // NKCODE_DPAD_DOWN
        case Qt::Key_Left:         return 21;  // NKCODE_DPAD_LEFT
        case Qt::Key_Right:        return 22;  // NKCODE_DPAD_RIGHT
        case Qt::Key_Shift:        return 59;  // NKCODE_SHIFT_LEFT
        case Qt::Key_Control:      return 113; // NKCODE_CTRL_LEFT
        case Qt::Key_Alt:          return 57;  // NKCODE_ALT_LEFT
        case Qt::Key_Semicolon:    return 74;  // NKCODE_SEMICOLON
        case Qt::Key_Comma:        return 55;  // NKCODE_COMMA
        case Qt::Key_Period:       return 56;  // NKCODE_PERIOD
        case Qt::Key_Slash:        return 76;  // NKCODE_SLASH
        case Qt::Key_Minus:        return 69;  // NKCODE_MINUS
        case Qt::Key_Equal:        return 70;  // NKCODE_EQUALS
        case Qt::Key_BracketLeft:  return 71;  // NKCODE_LEFT_BRACKET
        case Qt::Key_BracketRight: return 72;  // NKCODE_RIGHT_BRACKET
        case Qt::Key_Backslash:    return 73;  // NKCODE_BACKSLASH
        default: return -1;
    }
}

QString PPSSPPAdapter::formatKeyboardBinding(int qtKey, int modifiers) const {
    // PPSSPP hotkey bindings are single-key; modifier chords aren't supported
    // here, so we ignore modifiers entirely and bind the main key.
    Q_UNUSED(modifiers);
    const int code = qtKeyToPpssppNkcode(qtKey);
    if (code < 0) return {};
    return QString("1-%1").arg(code);
}

QString PPSSPPAdapter::formatMouseBinding(int qtButton) const {
    Q_UNUSED(qtButton);
    return {};
}

QString PPSSPPAdapter::formatWheelBinding(int direction) const {
    Q_UNUSED(direction);
    return {};
}

// ============================================================================
// Serial extraction — PSP uses PARAM.SFO inside ISO
// ============================================================================

QString PPSSPPAdapter::extractSerial(const QString& romPath) const {
    QByteArray sfoData = Iso9660::readFile(romPath, "PSP_GAME/PARAM.SFO");
    if (sfoData.isEmpty()) {
        qWarning() << "[PPSSPP] Failed to read PSP_GAME/PARAM.SFO from:" << romPath;
        return {};
    }
    QString discId = SfoParser::extractDiscId(sfoData);
    if (discId.isEmpty()) {
        qWarning() << "[PPSSPP] No DISC_ID found in PARAM.SFO for:" << romPath;
    }
    return discId;
}

// ============================================================================
// RetroAchievements
// ============================================================================

void PPSSPPAdapter::patchRetroAchievements(const QString& username,
                                            const QString& token,
                                            bool enabled,
                                            bool hardcore,
                                            bool notifications,
                                            bool sounds) {
    Q_UNUSED(username);
    Q_UNUSED(token);
    Q_UNUSED(notifications);
    // No credential patching — PPSSPP stores its own RA login.
    const QString mainPath = configFilePath();
    QString content;
    if (readConfigFile(mainPath, content, "PPSSPP")) {
        QVector<IniKeyPatch> patches = {
            {"Achievements", "AchievementsEnable", enabled ? "True" : "False"},
            {"Achievements", "AchievementsHardcoreMode", hardcore ? "True" : "False"},
            {"Achievements", "AchievementsSoundEffects", sounds ? "True" : "False"},
        };
        if (patchIniKeys(content, patches))
            writeConfigFile(mainPath, content, "PPSSPP");
    }
}

// ============================================================================
// Asset matching — select the right GitHub release asset for this platform
// ============================================================================

QString PPSSPPAdapter::matchAsset(const QStringList& assetNames) const {
    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
#if defined(Q_OS_MACOS)
        if (lower.contains("macos") && name.endsWith(".zip"))
            return name;
#elif defined(Q_OS_WIN)
        if (lower.contains("windows") && lower.contains("x64") && name.endsWith(".zip"))
            return name;
#else
        if (name.endsWith(".AppImage"))
            return name;
        if (lower.contains("linux") && (name.endsWith(".tar.gz") || name.endsWith(".tar.xz")))
            return name;
#endif
    }
    return EmulatorAdapter::matchAsset(assetNames);
}
