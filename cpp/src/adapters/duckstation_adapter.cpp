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

// ============================================================================
// Preview spec — wires the shared AspectRatio + OSD previews to schema-driven
// pages. Mirrors DolphinAdapter::previewSpec; the upstream Graphics > Rendering
// pane has the AspectRatio combo near the top, and the OSD pane has the
// overlay toggles.
// ============================================================================

PreviewSpec DuckStationAdapter::previewSpec(const QString& category,
                                             const QString& subcategory) const {
    // Aspect-ratio preview lives on the Recommended category — that's the
    // primary entry point and already has AspectRatio near the top
    // (mirrors dolphin_adapter / pcsx2_adapter). Graphics > Rendering has
    // the same combo without the preview, matching the rest of the
    // sub-tabs.
    //
    // The shared AspectRatioPreview's fromSchemaValue currently maps
    // PCSX2/Dolphin aspect strings; DuckStation's strings ("Auto (Game
    // Native)", "PAR 1:1" etc.) fall back to R4_3. The preview still
    // renders — it just doesn't differentiate DuckStation's extra ratios
    // (19:9, 20:9, 21:9, 16:10) yet. Tracked in
    // `duckstation-schema-alignment.md` as a follow-up.
    if (category == "Recommended" && subcategory.isEmpty()) {
        return {"aspect", {
            {"AspectRatio", "aspectMode"},
        }};
    }
    // OsdPreview Q_PROPERTYs that exist (osd_preview.h:22-44):
    //   showFps, showSpeed, showFrameTimes, showResolution, showCpu,
    //   showGpu, showGsStats, showInputs, showSettings, … etc.
    // DuckStation's ShowLatencyStatistics has no direct preview equivalent
    // — left out (preview still renders, just no toggle for it).
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        return {"osd", {
            {"ShowFPS",            "showFps"},
            {"ShowSpeed",          "showSpeed"},
            {"ShowFrameTimes",     "showFrameTimes"},
            {"ShowResolution",     "showResolution"},
            {"ShowCPU",            "showCpu"},
            {"ShowGPU",            "showGpu"},
            {"ShowGPUStatistics",  "showGsStats"},
            {"ShowInputs",         "showInputs"},
            {"ShowEnhancements",   "showSettings"},
        }};
    }
    return {};
}

QString DuckStationAdapter::subcategoryIcon(const QString& category,
                                             const QString& subcategory) const {
    if (category != "Graphics") return {};
    if (subcategory == "Rendering")           return QStringLiteral("\U0001F5BC");  // 🖼
    if (subcategory == "Advanced")            return QStringLiteral("\U0001F527");  // 🔧
    if (subcategory == "Texture Replacement") return QStringLiteral("\U0001F3A8");  // 🎨
    if (subcategory == "On-Screen Display")   return QStringLiteral("\U0001F4CA");  // 📊
    if (subcategory == "Capture")             return QStringLiteral("\U0001F3A5");  // 🎥
    return {};
}

// ============================================================================
// settingsSchema — mirrors standalone DuckStation's SettingsWindow panes
// verbatim: same top-level categories, same group order, same setting order
// inside each group, same labels, same gating chains. Driven entirely from
// the schema; rendered by GenericSettingsPage. See `dolphin-schema-alignment.md`
// in user memory for the reference shape and `docs/new-adapter-checklist.md`
// "Mirroring the upstream UI verbatim (THE rule)" for the audit recipe.
//
// Top-level categories in upstream display order (skipping panes that don't
// apply to RetroNest's embedded UI — see `duckstation-schema-alignment.md`
// in user memory for the full omission rationale):
//   BIOS · Console · Emulation · Memory Cards · Graphics · On-Screen Display
//   · Audio · Achievements · Capture · Advanced
//
// Helper lambdas:
//   dep(d, expr) — set d.dependsOn (boolean DSL — see setting_def.h).
// ============================================================================
QVector<SettingDef> DuckStationAdapter::settingsSchema() const {
    auto dep = [](SettingDef d, const QString& expr) -> SettingDef {
        d.dependsOn = expr;
        return d;
    };

    // ── Shared option lists ─────────────────────────────────────────────

    const QVector<QPair<QString,QString>> memCardTypes = {
        {"No Memory Card",                          "None"},
        {"Shared Between All Games",                "Shared"},
        {"Separate Card Per Game (Serial)",          "PerGame"},
        {"Separate Card Per Game (Title)",           "PerGameTitle"},
        {"Separate Card Per Game (File Title)",      "PerGameFileTitle"},
        {"Non-Persistent Card (Do Not Save)",        "NonPersistent"},
    };

    // Speed combo — used for EmulationSpeed / FastForwardSpeed / TurboSpeed.
    // Mirrors fillComboBoxWithEmulationSpeeds (emulationsettingswidget.cpp:171):
    // "Unlimited" + 25 percentages, label "%1% [%2 FPS (NTSC) / %3 FPS (PAL)]".
    // INI values use shortest float representation (DuckStation writes via
    // StringUtil::ToChars / std::to_chars — padded forms like "1.00" fail to
    // round-trip — audit 2026-04-06).
    const QVector<QPair<QString,QString>> speedOptions = [] {
        using P = QPair<QString,QString>;
        QVector<P> out;
        out.append(P{QStringLiteral("Unlimited"), QStringLiteral("0")});
        const QVector<int> speeds = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 125, 150, 175,
                                     200, 250, 300, 350, 400, 450, 500, 600, 700, 800, 900, 1000};
        for (int p : speeds) {
            const QString label = QStringLiteral("%1% [%2 FPS (NTSC) / %3 FPS (PAL)]")
                                      .arg(p).arg((60 * p) / 100).arg((50 * p) / 100);
            const double f = static_cast<double>(p) / 100.0;
            const QString val = QString::number(f, 'g', 6);  // shortest form (e.g. "1", "0.5", "1.25")
            out.append(P{label, val});
        }
        return out;
    }();

    const QVector<QPair<QString,QString>> cdromSpeedupOptions = {
        {"None (Double Speed)", "1"},
        {"2x (Quad Speed)",     "2"},
        {"3x (6x Speed)",      "3"},
        {"4x (8x Speed)",      "4"},
        {"5x (10x Speed)",     "5"},
        {"6x (12x Speed)",     "6"},
        {"Maximum (Safer)",    "0"},
    };

    // Native CDROM SeekSpeedup uses 1 for normal speed and 0 for the
    // maximum-cycles override (cdrom.cpp:1616, consolesettingswidget.cpp:19,74).
    // Earlier versions had endpoints swapped — picking "Maximum" actually
    // selected normal speed. See audit 2026-04-06.
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

    // INI values must match s_display_scaling_names (settings.cpp:2188-2190).
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

    // NotificationLocation — used for Achievement notification + indicator
    // location. INI values match s_notification_location_names
    // (settings.cpp:2421-2423).
    const QVector<QPair<QString,QString>> notificationLocationOptions = {
        {"Top Left",      "TopLeft"},
        {"Top Center",    "TopCenter"},
        {"Top Right",     "TopRight"},
        {"Bottom Left",   "BottomLeft"},
        {"Bottom Center", "BottomCenter"},
        {"Bottom Right",  "BottomRight"},
    };

    QVector<SettingDef> s;

    // =========================================================================
    // Recommended  (curated short list of the settings users most commonly
    // change — sourced from DuckStation's official setup/performance
    // documentation + community consensus. These entries DUPLICATE keys
    // that also appear under their primary category; both write the same
    // INI section/key, so editing here and editing in the full pane
    // produce the same result. The Recommended card is a curated VIEW for
    // users who don't want to hunt through every sub-tab to find the
    // dozen settings that actually matter for most games. Mirrors
    // dolphin_adapter.cpp / pcsx2_adapter.cpp.
    // =========================================================================

    // Performance — biggest impact for getting games playable.
    s.append({"Recommended", "", "Performance", "GPU", "Renderer", "Renderer",
              "GPU backend used for rendering. Switching backends often produces "
              "the single biggest performance change. Vulkan and Metal are fastest "
              "on modern macOS hardware; OpenGL is the most compatible; Software "
              "emulates the GPU on the CPU for perfect accuracy.",
              SettingDef::Combo, "Automatic",
              {{"Automatic", "Automatic"}, {"Vulkan", "Vulkan"}, {"Metal", "Metal"},
               {"OpenGL", "OpenGL"}, {"Software", "Software"}},
              0, 0, 0, "", ""});
    s.append({"Recommended", "", "Performance", "CPU", "ExecutionMode", "Execution Mode",
              "How the PSX CPU is emulated. Recompiler is fastest; Cached Interpreter "
              "is more accurate; Interpreter is the slowest, used only for debugging.",
              SettingDef::Combo, "Recompiler",
              {{"Interpreter (Slowest)",        "Interpreter"},
               {"Cached Interpreter (Faster)",  "CachedInterpreter"},
               {"Recompiler (Fastest)",         "Recompiler"}},
              0, 0, 0, "", ""});
    s.append({"Recommended", "", "Performance", "GPU", "UseThread", "Threaded Rendering",
              "Renders frames on a separate thread. Substantial speed improvement on "
              "modern multi-core CPUs.",
              SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});
    s.append({"Recommended", "", "Performance", "CDROM", "ReadSpeedup", "CD-ROM Read Speedup",
              "Speeds up CD-ROM reads beyond hardware limits. Cuts loading times in "
              "most games at no risk for the majority of titles.",
              SettingDef::Combo, "1", cdromSpeedupOptions, 0, 0, 0, "", ""});

    // Visual Quality — the most-tweaked image settings. Aspect Ratio drives
    // the live preview (see previewSpec).
    s.append({"Recommended", "", "Visual Quality", "GPU", "ResolutionScale", "Internal Resolution",
              "Render scale relative to native PS1 resolution. Higher = sharper but "
              "slower. Single biggest knob for visual fidelity.",
              SettingDef::Combo, "1",
              {{"1x Native",  "1"},  {"2x", "2"},  {"3x", "3"},  {"4x Native (1440p)", "4"},
               {"5x", "5"},  {"6x", "6"},  {"7x", "7"},  {"8x Native (4K)", "8"}},
              0, 0, 0, "", ""});
    s.append({"Recommended", "", "Visual Quality", "Display", "AspectRatio", "Aspect Ratio",
              "Display aspect ratio. Auto matches the game; force 16:9 for a "
              "widescreen TV; Stretch fills the whole window.",
              SettingDef::Combo, "Auto (Game Native)",
              {{"Auto (Game Native)", "Auto (Game Native)"}, {"Stretch To Fill", "Stretch To Fill"},
               {"4:3", "4:3"}, {"16:9", "16:9"}, {"19:9", "19:9"}, {"20:9", "20:9"},
               {"21:9", "21:9"}, {"16:10", "16:10"}, {"PAR 1:1", "PAR 1:1"}},
              0, 0, 0, "", ""});
    s.append({"Recommended", "", "Visual Quality", "GPU", "WidescreenHack", "Widescreen Rendering",
              "Stretches 3D geometry to fill a widescreen display. Pair with Aspect "
              "Ratio = 16:9 for the best result on a widescreen TV.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Recommended", "", "Visual Quality", "GPU", "PGXPEnable", "PGXP Geometry Correction",
              "Fixes polygon wobble by using a sub-pixel geometry buffer. Highly "
              "recommended — almost every PS1 game looks better with it on.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    // Multi-Sampling + Texture Filtering — paired side-by-side on the
    // Recommended page (matches dolphin_adapter's "paired" Anisotropic
    // Filtering / Force Texture Filtering pattern).
    s.append({"Recommended", "", "Visual Quality", "GPU", "Multisamples", "Multi-Sampling",
              "Multi-sample anti-aliasing. Smoother edges at modest GPU cost.",
              SettingDef::Combo, "1",
              {{"Disabled", "1"}, {"2x MSAA", "2"}, {"4x MSAA", "4"}, {"8x MSAA", "8"}, {"16x MSAA", "16"}},
              0, 0, 0, "paired", ""});
    s.append({"Recommended", "", "Visual Quality", "GPU", "TextureFilter", "Texture Filtering",
              "How textures are sampled. Nearest matches original hardware; Bilinear "
              "smooths low-res textures.",
              SettingDef::Combo, "Nearest", textureFilterOptions, 0, 0, 0, "paired", ""});

    // Frame Pacing — common stutter-fix toggles.
    s.append({"Recommended", "", "Frame Pacing", "Display", "VSync", "Vertical Sync (VSync)",
              "Synchronises frame output with the monitor to prevent screen tearing.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Recommended", "", "Frame Pacing", "Main", "SyncToHostRefreshRate",
              "Sync To Host Refresh Rate",
              "Adjusts emulation speed slightly so the console's refresh rate "
              "matches your monitor's. Smoother pacing at the cost of correct game speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Recommended", "", "Frame Pacing", "Display", "OptimalFramePacing",
              "Optimal Frame Pacing",
              "Reduces frame pacing jitter at a small CPU cost. Recommended on for "
              "most users.",
              SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});

    // Audio — most-toggled audio knobs.
    s.append({"Recommended", "", "Audio", "Audio", "Backend", "Audio Backend",
              "Output backend for the audio engine. Cubeb is the macOS default; SDL "
              "is the cross-platform fallback.",
              SettingDef::Combo, "Cubeb",
              {{"Null (No Output)", "Null"}, {"Cubeb", "Cubeb"}, {"SDL", "SDL"}},
              0, 0, 0, "", ""});
    s.append({"Recommended", "", "Audio", "Audio", "OutputVolume", "Output Volume",
              "Master output volume.",
              SettingDef::Int, "100", {}, 0, 200, 1, "slider", "%"});

    // Convenience — common quality-of-life toggles.
    s.append({"Recommended", "", "Convenience", "Main", "EmulationSpeed", "Emulation Speed",
              "Sets the target emulation speed for normal gameplay.",
              SettingDef::Combo, "1", speedOptions, 0, 0, 0, "", ""});
    s.append({"Recommended", "", "Convenience", "Console", "Region", "Region",
              "PS1 region. Auto-Detect picks the right region from the disc; force a "
              "value only if a particular game refuses to boot.",
              SettingDef::Combo, "Auto",
              {{"Auto-Detect",                 "Auto"},
               {"NTSC-J (Japan)",              "NTSC-J"},
               {"NTSC-U/C (US, Canada)",       "NTSC-U"},
               {"PAL (Europe, Australia)",     "PAL"}},
              0, 0, 0, "", ""});
    s.append({"Recommended", "", "Convenience", "BIOS", "PatchFastBoot", "Fast Boot",
              "Skips the BIOS boot animation and Sony intro.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // =========================================================================
    // Console category — mirrors consolesettingswidget.cpp
    //
    // Upstream BIOS pane (PIO Parallel Port group, BIOS/TTYLogging) is
    // intentionally omitted from RetroNest — Mark removed it 2026-05-06
    // (BIOS file pickers are filesystem-scanned + RetroNest-managed,
    // and the parallel-port emulation + TTY logging are debug-only edge
    // cases not relevant to the embedded UX).
    // =========================================================================
    // =========================================================================

    // Console group
    s.append({"Console", "", "Console", "Console", "Region", "Region", "",
              SettingDef::Combo, "Auto",
              {{"Auto-Detect",                 "Auto"},
               {"NTSC-J (Japan)",              "NTSC-J"},
               {"NTSC-U/C (US, Canada)",       "NTSC-U"},
               {"PAL (Europe, Australia)",     "PAL"}},
              0, 0, 0, "", ""});
    s.append({"Console", "", "Console", "GPU", "ForceVideoTiming", "Frame Rate", "",
              SettingDef::Combo, "Disabled",
              {{"Auto-Detect", "Disabled"}, {"NTSC (60hz)", "NTSC"}, {"PAL (50hz)", "PAL"}},
              0, 0, 0, "", ""});
    s.append({"Console", "", "Console", "BIOS", "PatchFastBoot", "Fast Boot",
              "Skips the BIOS boot animation and Sony intro.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append(dep({"Console", "", "Console", "BIOS", "FastForwardBoot", "Fast Forward Boot",
                  "Runs at fast forward speed during boot until the game starts.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "", ""}, "PatchFastBoot"));
    s.append({"Console", "", "Console", "MemoryCards", "FastForwardAccess", "Fast Forward Memory Card Access",
              "Speeds up memory card access animations.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Console", "", "Console", "Console", "Enable8MBRAM", "Enable 8MB RAM (Dev Console)",
              "Expands RAM to 8MB, as found in dev units.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // CPU Emulation group
    s.append({"Console", "", "CPU Emulation", "CPU", "ExecutionMode", "Execution Mode", "",
              SettingDef::Combo, "Recompiler",
              {{"Interpreter (Slowest)",        "Interpreter"},
               {"Cached Interpreter (Faster)",  "CachedInterpreter"},
               {"Recompiler (Fastest)",         "Recompiler"}},
              0, 0, 0, "", ""});
    s.append({"Console", "", "CPU Emulation", "CPU", "OverclockEnable",
              "Enable Clock Speed Control (Overclocking/Underclocking)", "",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    // Overclocking Percentage — slider stores percent; on save we synthesize
    // OverclockNumerator / OverclockDenominator via the same gcd reduction
    // upstream uses (Settings::CPUOverclockPercentToFraction at settings.cpp:230).
    // On load we recover the percent via num*100/denom (FractionToPercent).
    {
        SettingDef d = {"Console", "", "CPU Emulation", "CPU", "OverclockNumerator",
                        "Overclocking Percentage",
                        "Sets the CPU clock speed. 100% matches the original PSX CPU "
                        "(33.87 MHz). Stored as a numerator/denominator fraction.",
                        SettingDef::Int, "100", {}, 10, 1000, 5, "slider", "%"};
        d.dependsOn = "OverclockEnable";
        d.saveTransform = [](const QString& widgetValue,
                              const SettingDef::SaveCallback& save) {
            bool ok = false;
            int percent = widgetValue.toInt(&ok);
            if (!ok) percent = 100;
            const auto gcd = [](int a, int b) { while (b) { a %= b; std::swap(a, b); } return a; };
            const int g = gcd(percent, 100);
            save("CPU", "OverclockNumerator",   QString::number(percent / g));
            save("CPU", "OverclockDenominator", QString::number(100 / g));
        };
        d.loadTransform = [](const SettingDef::LoadCallback& read) -> QString {
            const int num = read("CPU", "OverclockNumerator").toInt();
            const int den = read("CPU", "OverclockDenominator").toInt();
            if (num <= 0 || den <= 0) return "100";
            return QString::number((num * 100) / den);
        };
        s.append(d);
    }
    s.append(dep({"Console", "", "CPU Emulation", "CPU", "RecompilerICache", "Enable Recompiler ICache",
                  "Simulates the instruction cache in the recompiler. Slower but more accurate.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "", ""},
                 "ExecutionMode=Recompiler"));

    // CD-ROM Emulation group
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "ReadSpeedup", "Read Speedup",
              "Speeds up CD-ROM reads beyond hardware limits.",
              SettingDef::Combo, "1", cdromSpeedupOptions, 0, 0, 0, "", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "SeekSpeedup", "Seek Speedup",
              "Reduces seek time beyond hardware limits.",
              SettingDef::Combo, "1", cdromSeekOptions, 0, 0, 0, "", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "LoadImageToRAM", "Preload Image To RAM",
              "Loads the disc image into RAM to reduce I/O stutter.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "LoadImagePatches", "Apply Image Patches",
              "Applies PPF patches found alongside the disc image.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "AutoDiscChange", "Switch to Next Disc on Stop",
              "Automatically switches to the next queued disc when the current one stops.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Console", "", "CD-ROM Emulation", "CDROM", "IgnoreHostSubcode", "Ignore Drive Subcode",
              "Ignores subcode errors on physical drives.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // =========================================================================
    // Emulation category — mirrors emulationsettingswidget.cpp
    // =========================================================================

    // Speed Control group
    s.append({"Emulation", "", "Speed Control", "Main", "EmulationSpeed", "Emulation Speed",
              "Sets the target emulation speed.",
              SettingDef::Combo, "1", speedOptions, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Speed Control", "Main", "FastForwardSpeed", "Fast Forward Speed",
              "Speed used when the fast forward hotkey is held.",
              SettingDef::Combo, "0", speedOptions, 0, 0, 0, "", ""});
    s.append({"Emulation", "", "Speed Control", "Main", "TurboSpeed", "Turbo Speed",
              "Speed used when the turbo hotkey is held.",
              SettingDef::Combo, "0", speedOptions, 0, 0, 0, "", ""});

    // Latency Control group
    s.append({"Emulation", "", "Latency Control", "Display", "VSync", "Vertical Sync (VSync)",
              "Synchronises frame presentation with the display refresh rate.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Emulation", "", "Latency Control", "Main", "SyncToHostRefreshRate", "Sync To Host Refresh Rate",
              "Adjusts emulation speed so the console's refresh rate matches the host's.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Emulation", "", "Latency Control", "Display", "OptimalFramePacing", "Optimal Frame Pacing",
              "Reduces frame pacing jitter at a small CPU cost.",
              SettingDef::Bool, "true", {}, 0, 0, 0, "paired", ""});
    s.append(dep({"Emulation", "", "Latency Control", "Display", "PreFrameSleep", "Reduce Input Latency",
                  "Sleeps before frame presentation to lower input lag.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "OptimalFramePacing"));
    // Skip Duplicate Frame Display — disabled when both VSync AND
    // SyncToHostRefreshRate force the duplicate-skip effect already.
    // Mirrors EmulationSettingsWidget::updateSkipDuplicateFramesEnabled.
    s.append(dep({"Emulation", "", "Latency Control", "Display", "SkipPresentingDuplicateFrames",
                  "Skip Duplicate Frame Display",
                  "Skips presenting duplicate frames to reduce GPU load.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""},
                 "!VSync || !SyncToHostRefreshRate"));
    s.append(dep({"Emulation", "", "Latency Control", "Display", "PreFrameSleepBuffer",
                  "Frame Time Buffer", "Pre-frame sleep buffer in milliseconds.",
                  SettingDef::Float, "2", {}, 0.5, 20.0, 0.5, "", "Milliseconds"},
                 "PreFrameSleep && OptimalFramePacing"));

    // Rewind group — RewindEnable is mutually exclusive with Runahead upstream;
    // Runahead also gates the rewind sub-controls.
    s.append(dep({"Emulation", "", "Rewind", "Main", "RewindEnable", "Enable Rewinding",
                  "Enables the rewind feature (uses extra RAM).",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "", ""},
                 "RunaheadFrameCount=0"));
    s.append(dep({"Emulation", "", "Rewind", "Main", "RewindFrequency", "Rewind Save Frequency",
                  "How often to save a rewind snapshot.",
                  SettingDef::Float, "10", {}, 0.0, 3600.0, 0.1, "", "Seconds"},
                 "RewindEnable && RunaheadFrameCount=0"));
    s.append(dep({"Emulation", "", "Rewind", "Main", "RewindSaveSlots", "Rewind Buffer Size",
                  "Number of rewind frames to keep in memory.",
                  SettingDef::Int, "10", {}, 1, 10000, 1, "", "Frames"},
                 "RewindEnable && RunaheadFrameCount=0"));
    s.append(dep({"Emulation", "", "Rewind", "GPU", "UseSoftwareRendererForMemoryStates",
                  "Use Software Renderer (Low VRAM Mode)",
                  "Uses the software renderer for rewind snapshots to save VRAM.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "", ""},
                 "RewindEnable && RunaheadFrameCount=0"));

    // Runahead group
    s.append({"Emulation", "", "Runahead", "Main", "RunaheadFrameCount", "Runahead",
              "Simulates N frames ahead to hide input latency.",
              SettingDef::Combo, "0",
              {{"Disabled", "0"},  {"1 Frame", "1"},  {"2 Frames", "2"}, {"3 Frames", "3"},
               {"4 Frames", "4"},  {"5 Frames", "5"}, {"6 Frames", "6"}, {"7 Frames", "7"},
               {"8 Frames", "8"},  {"9 Frames", "9"}, {"10 Frames", "10"}},
              0, 0, 0, "", ""});
    s.append(dep({"Emulation", "", "Runahead", "Main", "RunaheadForAnalogInput",
                  "Enable for Analog Input",
                  "Enables runahead even when an analog controller is connected.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "", ""},
                 "RunaheadFrameCount"));

    // =========================================================================
    // Memory Cards category — mirrors memorycardsettingswidget.cpp
    //
    // Save Locations group (Memory Cards / Save States dirs) is intentionally
    // omitted: RetroNest manages those directories under
    // {root}/emulators/duckstation/{systemId}/{memcards,savestates} and
    // the adapter writes them in createDefaultConfig / patchExistingConfig.
    // =========================================================================

    s.append({"Memory Cards", "", "Memory Card 1", "MemoryCards", "Card1Type", "Memory Card Type", "",
              SettingDef::Combo, "PerGameTitle", memCardTypes, 0, 0, 0, "", ""});
    s.append(dep({"Memory Cards", "", "Memory Card 1", "MemoryCards", "Card1Path",
                  "Shared Memory Card Path", "",
                  SettingDef::String, "", {}, 0, 0, 0, "readonly", ""}, "Card1Type=Shared"));

    s.append({"Memory Cards", "", "Memory Card 2", "MemoryCards", "Card2Type", "Memory Card Type", "",
              SettingDef::Combo, "None", memCardTypes, 0, 0, 0, "", ""});
    s.append(dep({"Memory Cards", "", "Memory Card 2", "MemoryCards", "Card2Path",
                  "Shared Memory Card Path", "",
                  SettingDef::String, "", {}, 0, 0, 0, "readonly", ""}, "Card2Type=Shared"));

    s.append({"Memory Cards", "", "Game-Specific Card Settings", "MemoryCards", "UsePlaylistTitle",
              "Use Single Card For Multi-Disc Games",
              "When playing a multi-disc game with per-game-title memory cards, use a "
              "single card for all discs.",
              SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});

    // =========================================================================
    // Graphics category — mirrors graphicssettingswidget.cpp's QTabWidget.
    //
    // Sub-tab order verbatim (Rendering / Advanced / PGXP / Texture Replacement).
    // The upstream 5th tab ("Debugging") is gated at runtime via
    // QtHost::ShouldShowDebugOptions() / setTabVisible(TAB_INDEX_DEBUGGING)
    // (graphicssettingswidget.cpp:601) and hidden by default — dropped here
    // matching the precedent in user memory `pcsx2-schema-alignment.md`.
    //
    // Renderer + Adapter live at the top of the upstream Graphics pane (above
    // the QTabWidget). We render them at the top of the first sub-tab
    // (Rendering) — Adapter is deferred (dynamically populated per renderer
    // at runtime).
    // =========================================================================

    // ── Graphics / Rendering ─────────────────────────────────────────────

    s.append({"Graphics", "Rendering", "", "GPU", "Renderer", "Renderer", "GPU backend used for rendering.",
              SettingDef::Combo, "Automatic",
              {{"Automatic", "Automatic"}, {"Vulkan", "Vulkan"}, {"Metal", "Metal"},
               {"OpenGL", "OpenGL"}, {"Software", "Software"}},
              0, 0, 0, "", ""});

    s.append({"Graphics", "Rendering", "", "GPU", "ResolutionScale", "Internal Resolution",
              "Renders the PS1 GPU at a higher resolution.",
              SettingDef::Combo, "1",
              {{"1x Native",  "1"},  {"2x", "2"},  {"3x", "3"},  {"4x Native (1440p)", "4"},
               {"5x", "5"},  {"6x", "6"},  {"7x", "7"},  {"8x Native (4K)", "8"},
               {"9x", "9"},  {"10x","10"}, {"11x","11"}, {"12x","12"},
               {"13x","13"}, {"14x","14"}, {"15x","15"}, {"16x","16"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "DownsampleMode", "Down-Sampling",
              "Downsamples the rendered image back to native resolution.",
              SettingDef::Combo, "Disabled",
              {{"Disabled", "Disabled"}, {"Box (Downsample 3D/Smooth All)", "Box"},
               {"Adaptive (Preserve 3D/Smooth 2D)", "Adaptive"}},
              0, 0, 0, "", ""});
    // Down-Sampling Display Scale (GPU/DownsampleScale) is intentionally
    // omitted — upstream sets it visible only when DownsampleMode == Box
    // (graphicssettingswidget.cpp:1052: setVisible(mode==GPUDownsampleMode::Box)).
    // Our dependsOn DSL only greys out, doesn't hide; rendering it as a
    // permanently-greyed slider when Adaptive is picked would diverge from
    // upstream. Dynamic-visibility blocker — restore once the schema gains
    // a "hide when inactive" mode.
    s.append({"Graphics", "Rendering", "", "GPU", "TextureFilter", "Texture Filtering", "",
              SettingDef::Combo, "Nearest", textureFilterOptions, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "SpriteTextureFilter", "Sprite Texture Filtering",
              "Texture filtering applied only to 2D sprites.",
              SettingDef::Combo, "Nearest", textureFilterOptions, 0, 0, 0, "", ""});
    // INI values match s_gpu_dithering_mode_names (settings.cpp:1708-1711).
    // Native default is TrueColor (settings.h:226). Audit 2026-04-06.
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
    // INI values must match Settings::ParseDisplayAspectRatio /
    // GetDisplayAspectRatioName: special strings "Auto (Game Native)",
    // "Stretch To Fill" and "PAR 1:1" required verbatim (with the spaces).
    // The "Custom" option is dropped — it requires a dynamic-visibility
    // numerator/denominator spinbox pair (deferral blocker). Audit 2026-04-06.
    s.append({"Graphics", "Rendering", "", "Display", "AspectRatio", "Aspect Ratio", "",
              SettingDef::Combo, "Auto (Game Native)",
              {{"Auto (Game Native)", "Auto (Game Native)"}, {"Stretch To Fill", "Stretch To Fill"},
               {"4:3", "4:3"}, {"16:9", "16:9"}, {"19:9", "19:9"}, {"20:9", "20:9"},
               {"21:9", "21:9"}, {"16:10", "16:10"}, {"PAR 1:1", "PAR 1:1"}},
              0, 0, 0, "", ""});
    // INI values match s_display_crop_mode_names (settings.cpp:1923-1925)
    // — Borders, not AllBorders. Audit 2026-04-06.
    s.append({"Graphics", "Rendering", "", "Display", "CropMode", "Crop", "",
              SettingDef::Combo, "Overscan",
              {{"None", "None"},
               {"Only Overscan Area", "Overscan"},
               {"Only Overscan Area (Aspect Uncorrected)", "OverscanUncorrected"},
               {"All Borders", "Borders"},
               {"All Borders (Aspect Uncorrected)", "BordersUncorrected"}},
              0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "Display", "Scaling", "Scaling",
              "Scaling filter applied to the final output.",
              SettingDef::Combo, "BilinearSmooth", scalingOptions, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "Display", "Scaling24Bit", "FMV Scaling",
              "Scaling filter applied during FMV playback.",
              SettingDef::Combo, "BilinearSmooth", scalingOptions, 0, 0, 0, "", ""});
    // PGXP toggles live in the Rendering tab upstream; the rest of the PGXP
    // settings are in the dedicated PGXP sub-tab below.
    s.append({"Graphics", "Rendering", "", "GPU", "PGXPEnable", "PGXP Geometry Correction",
              "Fixes polygon wobble by using a sub-pixel geometry buffer.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append(dep({"Graphics", "Rendering", "", "GPU", "PGXPDepthBuffer",
                  "PGXP Depth Buffer (Low Compatibility)",
                  "Uses a depth buffer for PGXP; may break some games.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "", ""}, "PGXPEnable"));
    s.append({"Graphics", "Rendering", "", "Display", "Force4_3For24Bit", "Force 4:3 For FMVs",
              "Switches to 4:3 aspect ratio during FMV sequences.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "ChromaSmoothing24Bit", "FMV Chroma Smoothing",
              "Applies chroma smoothing to FMV playback.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "WidescreenHack", "Widescreen Rendering",
              "Stretches 3D geometry to fill a widescreen display.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Rendering", "", "GPU", "ForceRoundTextureCoordinates",
              "Round Upscaled Texture Coordinates",
              "Reduces texture seams at high resolutions.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // ── Graphics / Advanced ──────────────────────────────────────────────

    // Display Options group
    //
    // Fullscreen Mode + Exclusive Fullscreen Control are deferred:
    // upstream populates the FullscreenMode combo from
    // adapter.fullscreen_modes at runtime (filesystem/runtime-scanned combo
    // blocker), and ExclusiveFullscreenControl is gated to Vulkan-only via
    // a backend-capability check we don't yet model.
    s.append({"Graphics", "Advanced", "Display Options", "Display", "Alignment", "Position", "",
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
    // Fine Crop bounds match upstream graphicssettingswidget.ui — short range
    // would silently clip user values written by the standalone UI.
    s.append(dep({"Graphics", "Advanced", "Display Options", "Display", "FineCropLeft",
                  "Left", "Fine Crop Size",
                  SettingDef::Int, "0", {}, -32768, 32767, 1, "inline", "px"}, "FineCropMode"));
    s.append(dep({"Graphics", "Advanced", "Display Options", "Display", "FineCropTop",
                  "Top", "",
                  SettingDef::Int, "0", {}, -32768, 32767, 1, "inline", "px"}, "FineCropMode"));
    s.append(dep({"Graphics", "Advanced", "Display Options", "Display", "FineCropRight",
                  "Right", "",
                  SettingDef::Int, "0", {}, -32768, 32767, 1, "inline", "px"}, "FineCropMode"));
    s.append(dep({"Graphics", "Advanced", "Display Options", "Display", "FineCropBottom",
                  "Bottom", "",
                  SettingDef::Int, "0", {}, -32768, 32767, 1, "inline", "px"}, "FineCropMode"));
    s.append({"Graphics", "Advanced", "Display Options", "Display", "DisableMailboxPresentation",
              "Disable Mailbox Presentation",
              "Forces FIFO presentation instead of mailbox.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    // Use Blit Swap Chain (Display/UseBlitSwapChain) is Windows + D3D11
    // only — `#ifdef _WIN32` around the binding (graphicssettingswidget.cpp:170)
    // and `#ifndef _WIN32 ... delete m_ui.blitSwapChain` removes the widget
    // entirely on non-Windows builds (graphicssettingswidget.cpp:578-582).
    // Dropped per the compile-gate exclusion rule.

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
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "UseThread", "Threaded Rendering",
              "Renders frames on a separate thread.",
              SettingDef::Bool, "true", {}, 0, 0, 0, "paired", ""});
    s.append(dep({"Graphics", "Advanced", "Rendering Options", "GPU", "MaxQueuedFrames",
                  "Max Queued Frames",
                  "Maximum number of frames to queue for rendering.",
                  SettingDef::Int, "2", {}, 0, 10, 1, "paired", ""}, "UseThread"));
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "EnableModulationCrop",
              "Texture Modulation Cropping (\"Old/v0 GPU\")",
              "Uses legacy texture modulation cropping behaviour.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "ScaledInterlacing",
              "Scaled Interlacing",
              "Scales interlaced content to full resolution.",
              SettingDef::Bool, "true", {}, 0, 0, 0, "", ""});
    s.append({"Graphics", "Advanced", "Rendering Options", "GPU", "UseSoftwareRendererForReadbacks",
              "Software Renderer Readbacks",
              "Uses the software renderer for GPU readbacks.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "", ""});

    // ── Graphics / PGXP — entire sub-tab dropped ─────────────────────────
    //
    // Mark removed the PGXP sub-tab 2026-05-06 — the per-knob toggles are
    // niche power-user options (CPU Mode, Vertex Cache, Disable on 2D
    // Polygons, Preserve Projection Precision …) that even DuckStation's
    // standalone UI gates behind PGXPEnable + various sub-conditions, so
    // the whole tab is effectively inert for normal use. The two PGXP
    // toggles users actually care about — PGXP Geometry Correction
    // (PGXPEnable) and PGXP Depth Buffer (PGXPDepthBuffer) — remain
    // reachable in Graphics > Rendering. The float spinboxes
    // (PGXPTolerance / PGXPDepthThreshold) were already deferred for the
    // float-step blocker.

    // ── Graphics / Texture Replacement ───────────────────────────────────
    //
    // Textures Directory (Folders/Textures) is omitted: RetroNest manages
    // textures under {root}/emulators/duckstation/{systemId}/textures/.

    // General Settings group
    s.append({"Graphics", "Texture Replacement", "General Settings", "GPU", "EnableTextureCache",
              "Enable Texture Cache", "",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    // PreloadTextures upstream gates `EnableVRAMWriteReplacements ||
    // (EnableTextureCache && EnableTextureReplacements)`. Our DSL can't
    // mix && / ||, but since EnableTextureReplacements is itself gated
    // on EnableTextureCache (below), the simplified
    // `EnableTextureReplacements || EnableVRAMWriteReplacements` matches
    // upstream's effective active states.
    s.append(dep({"Graphics", "Texture Replacement", "General Settings", "TextureReplacements",
                  "PreloadTextures", "Preload Texture Replacements", "",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""},
                 "EnableTextureReplacements || EnableVRAMWriteReplacements"));

    // Texture Replacement group — three top-level toggles all gated on
    // EnableTextureCache (graphicssettingswidget.cpp:1116-1118).
    s.append(dep({"Graphics", "Texture Replacement", "Texture Replacement", "TextureReplacements",
                  "EnableTextureReplacements", "Enable Texture Replacement", "",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "EnableTextureCache"));
    s.append(dep({"Graphics", "Texture Replacement", "Texture Replacement", "TextureReplacements",
                  "DumpTextures", "Enable Texture Dumping", "",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "EnableTextureCache"));
    s.append(dep({"Graphics", "Texture Replacement", "Texture Replacement", "TextureReplacements",
                  "AlwaysTrackUploads", "Always Track Uploads", "",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "EnableTextureCache"));
    // DumpReplacedTextures upstream gates `(EnableTextureCache &&
    // DumpTextures) || DumpVRAMWrites` — simplified the same way as
    // PreloadTextures since DumpTextures is itself gated on
    // EnableTextureCache.
    s.append(dep({"Graphics", "Texture Replacement", "Texture Replacement", "TextureReplacements",
                  "DumpReplacedTextures", "Dump Replaced Textures", "",
                  SettingDef::Bool, "true", {}, 0, 0, 0, "paired", ""},
                 "DumpTextures || DumpVRAMWrites"));

    // VRAM Write (Background) Replacement group
    s.append({"Graphics", "Texture Replacement", "VRAM Write Replacement", "TextureReplacements",
              "EnableVRAMWriteReplacements", "Enable VRAM Write Replacement", "",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "Texture Replacement", "VRAM Write Replacement", "TextureReplacements",
              "DumpVRAMWrites", "Enable VRAM Write Dumping", "",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    // Use Old MDEC Routines lives in [Hacks] upstream — same widget group, different INI section.
    s.append({"Graphics", "Texture Replacement", "VRAM Write Replacement", "Hacks",
              "UseOldMDECRoutines", "Use Old MDEC Routines", "",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // =========================================================================
    // Graphics > On-Screen Display sub-tab — mirrors osdsettingswidget.cpp.
    //
    // Upstream surfaces this as a top-level category, but RetroNest folds
    // it into Graphics as a sub-tab matching the Dolphin pattern (one
    // unified Graphics dialog covers display + OSD).
    //
    // Theme + Font + Overlay Font deferred: filesystem/runtime-scanned combos
    // (FullscreenUI::GetThemeNames(), ImGuiManager::GetTextFontNames(),
    // ImGuiManager::GetFixedFontNames() — populated at widget construction).
    // =========================================================================

    s.append({"Graphics", "On-Screen Display", "Display", "Display", "OSDScale",  "Display Scale",
              "Scale factor for OSD text.",
              SettingDef::Int, "100", {}, 25, 500, 1, "", "%"});
    s.append({"Graphics", "On-Screen Display", "Display", "Display", "OSDMargin", "Display Margins",
              "Margin around the OSD in pixels.",
              SettingDef::Int, "10", {}, 0, 200, 1, "", "px"});
    // Theme — display labels and INI values match s_theme_names +
    // s_theme_display_names (fullscreenui_widgets.cpp:121-130). Empty INI
    // value for "Automatic" theme matches upstream's default.
    //
    // Font (Main/ImGuiTextFont) and Overlay Font (Main/ImGuiFixedFont)
    // are intentionally omitted — Mark's standalone DuckStation build
    // doesn't surface them in the OSD pane (likely a build-flag /
    // version difference in the .ui file). Restore once the standalone
    // shows them again.
    s.append({"Graphics", "On-Screen Display", "Display", "UI", "FullscreenUITheme",
              "Theme", "Selects the OSD/big-picture theme.",
              SettingDef::Combo, "",
              {{"Automatic",   ""},
               {"Dark",        "Dark"},
               {"Light",       "Light"},
               {"AMOLED",      "AMOLED"},
               {"Cobalt Sky",  "CobaltSky"},
               {"Grey Matter", "GreyMatter"},
               {"Green Giant", "GreenGiant"},
               {"Pinky Pals",  "PinkyPals"},
               {"Dark Ocean",  "DarkOcean"},
               {"Dark Ruby",   "DarkRuby"},
               {"Purple Rain", "PurpleRain"}},
              0, 0, 0, "", ""});

    s.append({"Graphics", "On-Screen Display", "Messages", "Display", "ShowOSDMessages",          "Show Messages",           "", SettingDef::Bool, "true",  {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Messages", "Display", "ShowStatusIndicators",     "Show Status Indicators",  "", SettingDef::Bool, "true",  {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Messages", "Display", "AnimateOSDMessages",       "Animate Messages",        "", SettingDef::Bool, "true",  {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Messages", "Display", "BlurOSDMessageBackgrounds","Blur Message Backgrounds","", SettingDef::Bool, "true",  {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Messages", "Display", "OSDErrorDuration", "Error Duration",
              "How long error messages remain on screen.",
              SettingDef::Float, "15", {}, 0.5, 60.0, 0.5, "paired", "seconds"});
    s.append({"Graphics", "On-Screen Display", "Messages", "Display", "OSDWarningDuration", "Warning Duration", "",
              SettingDef::Float, "10", {}, 0.5, 60.0, 0.5, "paired", "seconds"});
    s.append(dep({"Graphics", "On-Screen Display", "Messages", "Display", "OSDInfoDuration", "Information Duration", "",
                  SettingDef::Float, "5",  {}, 0.5, 60.0, 0.5, "paired", "seconds"}, "ShowOSDMessages"));
    s.append(dep({"Graphics", "On-Screen Display", "Messages", "Display", "OSDQuickDuration", "Action Duration", "",
                  SettingDef::Float, "2.5",  {}, 0.5, 60.0, 0.5, "paired", "seconds"}, "ShowOSDMessages"));
    s.append({"Graphics", "On-Screen Display", "Messages", "Display", "OSDMessageLocation", "Display Location", "",
              SettingDef::Combo, "TopLeft", notificationLocationOptions, 0, 0, 0, "", ""});

    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowFPS",              "Show FPS",              "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowSpeed",            "Show Emulation Speed",  "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowCPU",              "Show CPU Usage",        "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowGPU",              "Show GPU Usage",        "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowResolution",       "Show Resolution",       "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowGPUStatistics",    "Show GPU Statistics",   "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowFrameTimes",       "Show Frame Times",      "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowLatencyStatistics","Show Latency Statistics","", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowInputs",           "Show Controller Input", "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "On-Screen Display", "Overlays", "Display", "ShowEnhancements",     "Show Enhancements",     "", SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // =========================================================================
    // Audio category — mirrors audiosettingswidget.cpp
    // =========================================================================

    // Controls group
    s.append({"Audio", "", "Controls", "Audio", "OutputVolume", "Output Volume",
              "Master output volume.",
              SettingDef::Int, "100", {}, 0, 200, 1, "slider", "%"});
    s.append({"Audio", "", "Controls", "Audio", "FastForwardVolume", "Fast Forward Volume",
              "Volume during fast forward.",
              SettingDef::Int, "100", {}, 0, 200, 1, "slider", "%"});
    s.append({"Audio", "", "Controls", "Audio", "OutputMuted", "Mute All Sound",
              "Silences all audio output.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Audio", "", "Controls", "CDROM", "MuteCDAudio", "Mute CD Audio",
              "Silences CD audio tracks only.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // Configuration group
    //
    // Audio backend INI values match s_backend_names (audio_stream.cpp:20).
    // On macOS the available backends are Null / Cubeb / SDL.
    s.append({"Audio", "", "Configuration", "Audio", "Backend", "Backend", "",
              SettingDef::Combo, "Cubeb",
              {{"Null (No Output)", "Null"}, {"Cubeb", "Cubeb"}, {"SDL", "SDL"}},
              0, 0, 0, "paired", ""});
    // Driver and Output Device are deferred — upstream populates both at
    // runtime (Cubeb's GetCubebDriverNames() / AudioStream::GetOutputDevices()
    // — async device enumeration, filesystem-scanned-style blocker). The
    // "Default" INI value must be the empty string ("") — any non-empty value
    // is passed to Cubeb as a driver-name lookup and fails. Audit 2026-04-06.
    s.append({"Audio", "", "Configuration", "Audio", "Driver", "Driver", "",
              SettingDef::Combo, "",
              {{"Default", ""}}, 0, 0, 0, "paired", ""});
    s.append({"Audio", "", "Configuration", "Audio", "OutputDevice", "Output Device", "",
              SettingDef::Combo, "",
              {{"Default", ""}}, 0, 0, 0, "", ""});
    // StretchMode display labels match s_stretch_mode_display_names
    // (core_audio_stream.cpp:230-234) verbatim. INI values match
    // s_stretch_mode_names (core_audio_stream.cpp:225-229).
    s.append({"Audio", "", "Configuration", "Audio", "StretchMode", "Stretch Mode", "",
              SettingDef::Combo, "TimeStretch",
              {{"Off (Noisy)", "None"},
               {"Resampling (Pitch Shift)", "Resample"},
               {"Time Stretch (Tempo Change, Best Sound)", "TimeStretch"}},
              0, 0, 0, "", ""});
    s.append({"Audio", "", "Configuration", "Audio", "BufferMS", "Buffer Size",
              "Audio buffer size; lower values reduce latency.",
              SettingDef::Int, "50", {}, 15, 500, 1, "slider", "ms"});
    s.append(dep({"Audio", "", "Configuration", "Audio", "OutputLatencyMS", "Output Latency",
                  "Additional output latency.",
                  SettingDef::Int, "20", {}, 1, 500, 1, "slider", "ms"},
                 "!OutputLatencyMinimal"));
    s.append({"Audio", "", "Configuration", "Audio", "OutputLatencyMinimal", "Minimal",
              "Use the smallest output latency the audio device supports.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // Time Stretching group — gated on StretchMode == TimeStretch.
    s.append(dep({"Audio", "", "Time Stretching", "Audio", "StretchSequenceLengthMS",
                  "Sequence Length", "SoundTouch sequence length.",
                  SettingDef::Int, "30", {}, 20, 100, 1, "slider", "ms"},
                 "StretchMode=TimeStretch"));
    s.append(dep({"Audio", "", "Time Stretching", "Audio", "StretchSeekWindowMS",
                  "Seek Window", "SoundTouch seek window size.",
                  SettingDef::Int, "20", {}, 10, 30, 1, "slider", "ms"},
                 "StretchMode=TimeStretch"));
    s.append(dep({"Audio", "", "Time Stretching", "Audio", "StretchOverlapMS",
                  "Overlap", "SoundTouch overlap duration.",
                  SettingDef::Int, "10", {}, 5, 15, 1, "slider", "ms"},
                 "StretchMode=TimeStretch"));
    s.append({"Audio", "", "Time Stretching", "Audio", "StretchUseQuickSeek", "Use Quick Seek",
              "Enables SoundTouch quick seek mode (lower quality, less CPU).",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Audio", "", "Time Stretching", "Audio", "StretchUseAAFilter", "Use Anti-Aliasing Filter",
              "Enables SoundTouch anti-aliasing filter.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});

    // =========================================================================
    // Achievements category — mirrors achievementsettingswidget.cpp
    //
    // Login box (username display + Login/Logout buttons) is intentionally
    // omitted — credentials live in RetroNest's RAService (see CLAUDE.md
    // RetroAchievements section). NotificationScale + IndicatorScale are
    // deferred: upstream uses a tri-mode size combo (Auto / OSD Scale /
    // Custom) coupled to a percent spinbox — needs combo+spinbox composite
    // widget infra we don't have yet.
    // =========================================================================

    // Settings group
    s.append({"Achievements", "", "Settings", "Cheevos", "Enabled", "Enable Achievements", "",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append(dep({"Achievements", "", "Settings", "Cheevos", "ChallengeMode",
                  "Enable Hardcore Mode",
                  "Disables save states, fast forward, cheats, and more.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Settings", "Cheevos", "SpectatorMode",
                  "Enable Spectator Mode",
                  "Tracks achievements without unlocking them on RetroAchievements.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Settings", "Cheevos", "EncoreMode", "Enable Encore Mode",
                  "Allows replaying already-earned achievements for additional notifications.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""},
                 "Enabled && !SpectatorMode"));
    s.append(dep({"Achievements", "", "Settings", "Cheevos", "UnofficialTestMode",
                  "Test Unofficial Achievements",
                  "Includes unofficial / in-development achievements.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Settings", "Cheevos", "PrefetchBadges", "Prefetch Badges",
                  "Pre-downloads achievement badge images.",
                  SettingDef::Bool, "true", {}, 0, 0, 0, "paired", ""}, "Enabled"));

    // Notifications group
    s.append(dep({"Achievements", "", "Notifications", "Cheevos", "Notifications",
                  "Show Achievement Notifications",
                  "Pops up a notification when an achievement is unlocked.",
                  SettingDef::Bool, "true", {}, 0, 0, 0, "", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Notifications", "Cheevos", "NotificationsDuration",
                  "Achievement Notifications Duration",
                  "How long the achievement notification stays on screen.",
                  SettingDef::Int, "5", {}, 1, 30, 1, "slider", "seconds"},
                 "Enabled && Notifications"));
    s.append(dep({"Achievements", "", "Notifications", "Cheevos", "LeaderboardNotifications",
                  "Show Leaderboard Notifications",
                  "Pops up a notification on leaderboard submissions.",
                  SettingDef::Bool, "true", {}, 0, 0, 0, "", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Notifications", "Cheevos", "LeaderboardsDuration",
                  "Leaderboard Notifications Duration",
                  "How long the leaderboard notification stays on screen.",
                  SettingDef::Int, "10", {}, 1, 30, 1, "slider", "seconds"},
                 "Enabled && LeaderboardNotifications"));
    s.append(dep({"Achievements", "", "Notifications", "Cheevos", "LeaderboardTrackers",
                  "Show Leaderboard Trackers",
                  "Shows live leaderboard trackers while playing.",
                  SettingDef::Bool, "true", {}, 0, 0, 0, "", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Notifications", "Cheevos", "SoundEffects",
                  "Enable Sound Effects",
                  "Plays a sound on achievement unlock and other events.",
                  SettingDef::Bool, "true", {}, 0, 0, 0, "", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Notifications", "Cheevos", "NotificationLocation",
                  "Notification Location", "",
                  SettingDef::Combo, "TopLeft", notificationLocationOptions, 0, 0, 0, "", ""},
                 "Enabled"));

    // Progress Tracking group
    s.append(dep({"Achievements", "", "Progress Tracking", "Cheevos", "ChallengeIndicatorMode",
                  "Challenge Indicators", "",
                  SettingDef::Combo, "PersistentIcon",
                  {{"Disabled", "Disabled"},
                   {"Show Persistent Icons", "PersistentIcon"},
                   {"Show Temporary Icons",  "TemporaryIcon"},
                   {"Show Notifications",    "Notification"}},
                  0, 0, 0, "", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Progress Tracking", "Cheevos", "ProgressIndicatorMode",
                  "Progress Indicators", "",
                  SettingDef::Combo, "Icon",
                  {{"Disabled", "Disabled"},
                   {"Show Icon", "Icon"},
                   {"Show Icon and Title", "IconAndTitle"}},
                  0, 0, 0, "", ""}, "Enabled"));
    s.append(dep({"Achievements", "", "Progress Tracking", "Cheevos", "IndicatorLocation",
                  "Indicator Location", "",
                  SettingDef::Combo, "BottomRight", notificationLocationOptions, 0, 0, 0, "", ""},
                 "Enabled"));

    // =========================================================================
    // Graphics > Capture sub-tab — mirrors capturesettingswidget.cpp.
    //
    // Upstream surfaces this as a top-level category, but RetroNest folds
    // it into Graphics as a sub-tab matching the OSD precedent — capture
    // is graphics-adjacent (screenshots + video output), so one unified
    // Graphics dialog stays clean.
    //
    // Save Locations (Folders/Screenshots, Folders/Videos) deferred —
    // managed by RetroNest under
    // emulators/duckstation/{systemId}/{screenshots,videos}.
    // Container / VideoCodec / AudioCodec combos are deferred — populated
    // dynamically from MediaCapture::GetContainerList(backend) /
    // GetVideoCodecList(backend, container) / GetAudioCodecList. Without
    // backend selection these inputs degrade to free-text strings, so we
    // surface the keys but leave the combo population to upstream.
    // =========================================================================

    // Screenshots group
    s.append({"Graphics", "Capture", "Screenshots", "Display", "ScreenshotMode", "Screenshot Size", "",
              SettingDef::Combo, "ScreenResolution",
              {{"Screen Resolution", "ScreenResolution"},
               {"Internal Resolution", "InternalResolution"},
               {"Internal Resolution (Aspect Uncorrected)", "UncorrectedInternalResolution"}},
              0, 0, 0, "paired", ""});
    s.append({"Graphics", "Capture", "Screenshots", "Display", "ScreenshotFormat", "Screenshot Format", "",
              SettingDef::Combo, "PNG",
              {{"PNG", "PNG"}, {"JPEG", "JPEG"}, {"WebP", "WebP"}},
              0, 0, 0, "paired", ""});
    s.append({"Graphics", "Capture", "Screenshots", "Display", "ScreenshotQuality", "Screenshot Quality", "",
              SettingDef::Int, "85", {}, 1, 100, 1, "slider", "%"});
    s.append({"Graphics", "Capture", "Screenshots", "Display", "ScreenshotFileNameFormat", "File Name Format", "",
              SettingDef::Combo, "TitleAndTimestamp",
              {{"Timestamp", "Timestamp"},
               {"Game and Timestamp", "TitleAndTimestamp"},
               {"Timestamp in Game Folder", "TimestampInFolder"},
               {"Game and Timestamp in Game Folder", "TitleAndTimestampInFolder"}},
              0, 0, 0, "", ""});

    // Media Capture group
    s.append({"Graphics", "Capture", "Media Capture", "MediaCapture", "Backend", "Backend", "",
              SettingDef::Combo, "FFmpeg",
              {{"FFmpeg", "FFmpeg"}},
              0, 0, 0, "paired", ""});
    // Container is dynamic per backend — surfaced as a free-form String so
    // round-trip with upstream's selection still works (default mp4).
    s.append({"Graphics", "Capture", "Media Capture", "MediaCapture", "Container", "Container", "",
              SettingDef::String, "mp4", {}, 0, 0, 0, "paired", ""});
    s.append({"Graphics", "Capture", "Media Capture", "MediaCapture", "FilenameFormat", "File Name Format", "",
              SettingDef::Combo, "TitleAndTimestamp",
              {{"Timestamp", "Timestamp"},
               {"Game and Timestamp", "TitleAndTimestamp"},
               {"Timestamp in Game Folder", "TimestampInFolder"},
               {"Game and Timestamp in Game Folder", "TitleAndTimestampInFolder"}},
              0, 0, 0, "", ""});

    // Video sub-area
    s.append({"Graphics", "Capture", "Media Capture", "MediaCapture", "VideoCapture", "Capture Video",
              "Captures video to the chosen file when media capture is started.",
              SettingDef::Bool, "true", {}, 0, 0, 0, "paired", ""});
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "VideoCodec", "Video Codec",
                  "Video codec name (FFmpeg) — leave blank for default.",
                  SettingDef::String, "", {}, 0, 0, 0, "paired", ""}, "VideoCapture"));
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "VideoBitrate", "Video Bitrate",
                  "Target video bitrate.",
                  SettingDef::Int, "6000", {}, 100, 100000, 100, "", "kbps"}, "VideoCapture"));
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "VideoAutoSize",
                  "Automatic Video Resolution",
                  "Captures at the running game's internal resolution.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "", ""}, "VideoCapture"));
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "VideoWidth", "Video Width", "",
                  SettingDef::Int, "640", {}, 320, 32768, 16, "paired", "px"},
                 "VideoCapture && !VideoAutoSize"));
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "VideoHeight", "Video Height", "",
                  SettingDef::Int, "480", {}, 240, 32768, 16, "paired", "px"},
                 "VideoCapture && !VideoAutoSize"));
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "VideoCodecUseArgs",
                  "Enable Extra Video Arguments", "",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "VideoCapture"));
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "VideoCodecArgs",
                  "Extra Video Arguments",
                  "Codec parameters as 'key = value : key = value' pairs.",
                  SettingDef::String, "", {}, 0, 0, 0, "paired", ""},
                 "VideoCapture && VideoCodecUseArgs"));

    // Audio sub-area
    s.append({"Graphics", "Capture", "Media Capture", "MediaCapture", "AudioCapture", "Capture Audio",
              "Captures audio to the chosen file when media capture is started.",
              SettingDef::Bool, "true", {}, 0, 0, 0, "paired", ""});
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "AudioCodec", "Audio Codec",
                  "Audio codec name (FFmpeg) — leave blank for default.",
                  SettingDef::String, "", {}, 0, 0, 0, "paired", ""}, "AudioCapture"));
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "AudioBitrate", "Audio Bitrate",
                  "Target audio bitrate.",
                  SettingDef::Int, "128", {}, 16, 2048, 1, "", "kbps"}, "AudioCapture"));
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "AudioCodecUseArgs",
                  "Enable Extra Audio Arguments", "",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "AudioCapture"));
    // NOTE — upstream binds both VideoCodecArgs and AudioCodecArgs to the
    // same INI key "MediaCapture/AudioCodecArgs" (capturesettingswidget.cpp
    // :64+:70). Our Audio extra args therefore mirrors the upstream bug; if
    // upstream fixes it, the audit will refresh the key here.
    s.append(dep({"Graphics", "Capture", "Media Capture", "MediaCapture", "AudioCodecArgs",
                  "Extra Audio Arguments",
                  "Codec parameters as 'key = value : key = value' pairs.",
                  SettingDef::String, "", {}, 0, 0, 0, "paired", ""},
                 "AudioCapture && AudioCodecUseArgs"));

    // =========================================================================
    // Advanced category — mirrors advancedsettingswidget.cpp (Logging only).
    //
    // Cache + Covers folder pickers omitted (RetroNest manages those paths).
    // ShowDebugMenu omitted — its only effect is unlocking the upstream
    // Debugging panes, which RetroNest already drops. Log Channels button
    // (popup menu over LogWindow::populateFilterMenu) and Web Cache action
    // buttons (refresh / clear) deferred — modal/action widget blockers.
    // RAIntegration is Windows-only.
    // =========================================================================

    s.append({"Advanced", "", "Logging", "Logging", "LogLevel", "Log Level",
              "Verbosity of host log messages.",
              SettingDef::Combo, "Info",
              {{"None", "None"}, {"Error", "Error"}, {"Warning", "Warning"},
               {"Information", "Info"}, {"Verbose", "Verbose"}, {"Developer", "Dev"},
               {"Debug", "Debug"}, {"Trace", "Trace"}},
              0, 0, 0, "", ""});
    s.append({"Advanced", "", "Logging", "Logging", "LogToConsole", "Log To System Console",
              "Logs messages to the console window.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Advanced", "", "Logging", "Logging", "LogToDebug", "Log To Debug Console",
              "Logs messages to the debug console where supported.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Advanced", "", "Logging", "Logging", "LogToWindow", "Log To Window",
              "Logs messages to the in-emulator log window.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append({"Advanced", "", "Logging", "Logging", "LogToFile", "Log To File",
              "Logs messages to duckstation.log in the user directory.",
              SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""});
    s.append(dep({"Advanced", "", "Logging", "Logging", "LogTimestamps", "Log Timestamps",
                  "Includes elapsed time since application start in window/console logs.",
                  SettingDef::Bool, "true", {}, 0, 0, 0, "paired", ""},
                 "LogToConsole || LogToWindow"));
    s.append(dep({"Advanced", "", "Logging", "Logging", "LogFileTimestamps", "Log File Timestamps",
                  "Includes elapsed time since application start in file logs.",
                  SettingDef::Bool, "false", {}, 0, 0, 0, "paired", ""}, "LogToFile"));

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

    // savesPath is this emulator's unified data root — every managed
    // subfolder lives directly under it (see EmulatorService::ensureConfig).
    const QString& dataRoot = savesPath;
    const QVector<IniKeyPatch> patches = {
        // Wizard suppression + embedding-critical Main keys.
        {"Main", "SetupWizardComplete",   "true"},
        {"Main", "SetupWizardIncomplete", "false"},
        {"Main", "ConfirmPowerOff",       "false"},
        {"Main", "StartFullscreen",       "true"},
        {"Main", "PauseOnFocusLoss",      "true"},
        {"Main", "SaveStateOnExit",       "true"},

        {"BIOS",        "SearchDirectory", biosPath},
        {"Display",     "Fullscreen",      "true"},

        {"MemoryCards", "Directory",       dataRoot + "/memcards"},
        {"MemoryCards", "Card1Path",       dataRoot + "/memcards/shared_card_1.mcd"},
        {"MemoryCards", "Card2Path",       dataRoot + "/memcards/shared_card_2.mcd"},

        {"Folders", "SaveStates",  dataRoot + "/savestates"},
        {"Folders", "Screenshots", dataRoot + "/screenshots"},
        {"Folders", "Cache",       dataRoot + "/cache"},
        {"Folders", "Cheats",      dataRoot + "/cheats"},
        {"Folders", "Textures",    dataRoot + "/textures"},

        // Force-bound on every launch so a user-rebound key is corrected
        // back to the synth target. Hidden from hotkeyBindingDefs() so the
        // user can't rebind via our UI either. AppController synthesizes
        // Space / F5 / F7 / F8 via kVK_* — see DuckStationAdapter::
        // hotkeyVirtualKeyCode.
        {"Hotkeys", "OpenPauseMenu",         ""},
        {"Hotkeys", "TogglePause",           "Keyboard/Space"},
        {"Hotkeys", "ToggleFullscreen",      ""},
        {"Hotkeys", "SaveSelectedSaveState", "Keyboard/F5"},
        {"Hotkeys", "LoadSelectedSaveState", "Keyboard/F7"},
        {"Hotkeys", "ToggleFastForward",     "Keyboard/F8"},

        // Without [Pad1].Type DuckStation registers zero bindings — see
        // Controller::GetSettingsSection / Settings::Load in upstream.
        {"Pad1", "Type", "AnalogController"},
    };

    return patchOrCreateConfigFile(pDir + "/settings.ini", patches, "DuckStation");
}

// ============================================================================
// resolveExecutable — platform-aware executable resolution
// ============================================================================

QString DuckStationAdapter::resolveExecutable(const EmulatorManifest& manifest,
                                              const QString& installPath) {
    return resolveExecutableInDir(manifest, installPath, "DuckStation");
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
    //
    // SdlInputManager emits canonical FaceSouth/East/West/North for the
    // face buttons, but DuckStation's SDL parser only recognises the
    // Xbox-style A/B/X/Y names. Translate before formatting; a non-matching
    // string would silently parse as no binding.
    Q_UNUSED(positive);
    QString name = element;
    if      (name == "FaceSouth") name = "A";
    else if (name == "FaceEast")  name = "B";
    else if (name == "FaceWest")  name = "X";
    else if (name == "FaceNorth") name = "Y";

    if (isAxis && name.contains("Trigger"))
        return QString("SDL-%1/+%2").arg(deviceIndex).arg(name);
    return QString("SDL-%1/%2").arg(deviceIndex).arg(name);
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
        {"AnalogController", "Analog Controller",
         ":/AppUI/qml/AppUI/images/controllers/analog_controller.svg"},
    };
}

QVector<BindingDef> DuckStationAdapter::controllerBindingDefsForType(const QString& type) const {
    if (type != "AnalogController") return {};

    // AnalogController — 27 bindings across 7 cardSlots.
    // Spotlight coordinates are in the analog_controller.svg intrinsic
    // viewBox (12174.6 × 8309.6). The SVG's content is wrapped in a
    // parent transform="translate(-934.33, -774.13)", so display coords =
    // raw - (934, 774). Centers calibrated against the labeled SVG
    // elements: D-Pad cluster around (2417, 4201), face buttons around
    // (9758, 4192), Botões2 face circles cx/cy (Square 9642/4966 raw,
    // Triangle 10692/3917, Cross 10692/6016, Circle 11741/4966), sticks
    // at (4429, 6233) and (7746, 6233), shoulder shapes at top.
    return {
        // D-Pad
        {BindingDef::Button, "Up",    "D-Pad", "Pad1", "Up",    "SDL-0/DPadUp",
            "DPad", 2417, 3560, 450},
        {BindingDef::Button, "Right", "D-Pad", "Pad1", "Right", "SDL-0/DPadRight",
            "DPad", 3387, 4201, 450},
        {BindingDef::Button, "Down",  "D-Pad", "Pad1", "Down",  "SDL-0/DPadDown",
            "DPad", 2417, 4842, 450},
        {BindingDef::Button, "Left",  "D-Pad", "Pad1", "Left",  "SDL-0/DPadLeft",
            "DPad", 1447, 4201, 450},

        // Left Analog
        {BindingDef::Axis, "Left Stick Up",    "Left Stick", "Pad1", "LUp",    "SDL-0/LeftY",
            "LeftAnalog", 4429, 5533, 350},
        {BindingDef::Axis, "Left Stick Right", "Left Stick", "Pad1", "LRight", "SDL-0/LeftX",
            "LeftAnalog", 5129, 6233, 350},
        {BindingDef::Axis, "Left Stick Down",  "Left Stick", "Pad1", "LDown",  "SDL-0/LeftY",
            "LeftAnalog", 4429, 6933, 350},
        {BindingDef::Axis, "Left Stick Left",  "Left Stick", "Pad1", "LLeft",  "SDL-0/LeftX",
            "LeftAnalog", 3729, 6233, 350},
        {BindingDef::Button, "L3", "Stick Buttons", "Pad1", "L3", "SDL-0/LeftStick",
            "LeftAnalog", 4429, 6233, 700},

        // Face Buttons
        {BindingDef::Button, "Triangle", "Face Buttons", "Pad1", "Triangle", "SDL-0/Y",
            "FaceButtons", 9758, 3143, 520},
        {BindingDef::Button, "Circle",   "Face Buttons", "Pad1", "Circle",   "SDL-0/B",
            "FaceButtons", 10807, 4192, 520},
        {BindingDef::Button, "Cross",    "Face Buttons", "Pad1", "Cross",    "SDL-0/A",
            "FaceButtons", 9758, 5242, 520},
        {BindingDef::Button, "Square",   "Face Buttons", "Pad1", "Square",   "SDL-0/X",
            "FaceButtons", 8708, 4192, 520},

        // Right Analog
        {BindingDef::Axis, "Right Stick Up",    "Right Stick", "Pad1", "RUp",    "SDL-0/RightY",
            "RightAnalog", 7746, 5533, 350},
        {BindingDef::Axis, "Right Stick Right", "Right Stick", "Pad1", "RRight", "SDL-0/RightX",
            "RightAnalog", 8446, 6233, 350},
        {BindingDef::Axis, "Right Stick Down",  "Right Stick", "Pad1", "RDown",  "SDL-0/RightY",
            "RightAnalog", 7746, 6933, 350},
        {BindingDef::Axis, "Right Stick Left",  "Right Stick", "Pad1", "RLeft",  "SDL-0/RightX",
            "RightAnalog", 7046, 6233, 350},
        {BindingDef::Button, "R3", "Stick Buttons", "Pad1", "R3", "SDL-0/RightStick",
            "RightAnalog", 7746, 6233, 700},

        // Shoulders
        {BindingDef::Axis,   "L2", "Triggers",  "Pad1", "L2", "SDL-0/+LeftTrigger",
            "LeftShoulders", 2697, 331, 550},
        {BindingDef::Button, "L1", "Shoulders", "Pad1", "L1", "SDL-0/LeftShoulder",
            "LeftShoulders", 2516, 1091, 550},
        {BindingDef::Button, "R1", "Shoulders", "Pad1", "R1", "SDL-0/RightShoulder",
            "RightShoulders", 9659, 1091, 550},
        {BindingDef::Axis,   "R2", "Triggers",  "Pad1", "R2", "SDL-0/+RightTrigger",
            "RightShoulders", 9479, 331, 550},

        // System
        {BindingDef::Button, "Select", "System", "Pad1", "Select", "SDL-0/Back",
            "System", 4910, 4130, 300},
        {BindingDef::Button, "Start",  "System", "Pad1", "Start",  "SDL-0/Start",
            "System", 7261, 4159, 270},
        {BindingDef::Button, "Analog", "System", "Pad1", "Analog", "",
            "System", 6145, 5395, 240},

        // Abstract bindings — no spotlight (no physical button on artwork).
        {BindingDef::Axis, "LargeMotor", "Motors", "Pad1", "LargeMotor", "",
            "System", 0, 0, 0},
        {BindingDef::Axis, "SmallMotor", "Motors", "Pad1", "SmallMotor", "",
            "System", 0, 0, 0},
    };
}

QVector<SettingDef> DuckStationAdapter::controllerSettingDefsForType(const QString& type) const {
    Q_UNUSED(type);
    return {};
}

QVector<HotkeyDef> DuckStationAdapter::hotkeyBindingDefs() const {
    return {
        // ── Interface ──
        {"Open Cheat Settings",        "Interface",   "Hotkeys", "OpenCheatsMenu",            ""},
        {"Open Achievement List",      "Interface",   "Hotkeys", "OpenAchievements",          ""},
        {"Open Leaderboard List",      "Interface",   "Hotkeys", "OpenLeaderboards",          ""},
        {"Save Screenshot",            "Interface",   "Hotkeys", "Screenshot",                "Keyboard/F10"},

        // ── System ──
        // ToggleFastForward is force-bound to Keyboard/F8 in
        // createDefaultConfig + patchExistingConfig and synthesized by
        // AppController for the in-game menu's Fast Forward action; it's
        // omitted here so users can't rebind the synth key.
        {"Fast Forward (Hold)",        "System",      "Hotkeys", "FastForward",               "Keyboard/Tab"},
        {"Turbo (Hold)",               "System",      "Hotkeys", "Turbo",                     ""},
        {"Turbo (Toggle)",             "System",      "Hotkeys", "ToggleTurbo",               ""},
        {"Restart Game",               "System",      "Hotkeys", "Reset",                     ""},
        {"Change Disc",                "System",      "Hotkeys", "ChangeDisc",                ""},
        {"Switch to Previous Disc",    "System",      "Hotkeys", "SwitchToPreviousDisc",      ""},
        {"Switch to Next Disc",        "System",      "Hotkeys", "SwitchToNextDisc",          ""},
        {"Rewind",                     "System",      "Hotkeys", "Rewind",                    ""},
        {"Frame Step",                 "System",      "Hotkeys", "FrameStep",                 ""},
        {"Toggle Media Capture",       "System",      "Hotkeys", "ToggleMediaCapture",        ""},
        {"Swap Memory Card Slots",     "System",      "Hotkeys", "SwapMemoryCards",           ""},
        {"Toggle Clock Speed Control (Overclocking)","System","Hotkeys","ToggleOverclocking", ""},
        {"Increase Emulation Speed",   "System",      "Hotkeys", "IncreaseEmulationSpeed",    ""},
        {"Decrease Emulation Speed",   "System",      "Hotkeys", "DecreaseEmulationSpeed",    ""},
        {"Reset Emulation Speed",      "System",      "Hotkeys", "ResetEmulationSpeed",       ""},

        // ── Graphics ──
        {"Rotate Display Clockwise",   "Graphics",    "Hotkeys", "RotateClockwise",           ""},
        {"Rotate Display Counterclockwise","Graphics","Hotkeys", "RotateCounterclockwise",    ""},
        {"Toggle On-Screen Display",   "Graphics",    "Hotkeys", "ToggleOSD",                 ""},
        {"Toggle Software Rendering",  "Graphics",    "Hotkeys", "ToggleSoftwareRendering",   ""},
        {"Toggle PGXP",                "Graphics",    "Hotkeys", "TogglePGXP",                ""},
        {"Toggle PGXP Depth Buffer",   "Graphics",    "Hotkeys", "TogglePGXPDepth",           ""},
        {"Toggle Widescreen",          "Graphics",    "Hotkeys", "ToggleWidescreen",          ""},
        {"Toggle Texture Modulation Cropping","Graphics","Hotkeys","ToggleModulationCrop",    ""},
        {"Toggle Post-Processing",     "Graphics",    "Hotkeys", "TogglePostProcessing",      ""},
        {"Reload Post Processing Shaders","Graphics", "Hotkeys", "ReloadPostProcessingShaders",""},
        {"Reload Texture Replacements","Graphics",    "Hotkeys", "ReloadTextureReplacements", ""},
        {"Increase Resolution Scale",  "Graphics",    "Hotkeys", "IncreaseResolutionScale",   ""},
        {"Decrease Resolution Scale",  "Graphics",    "Hotkeys", "DecreaseResolutionScale",   ""},
        {"Record Single Frame GPU Trace","Graphics",  "Hotkeys", "RecordSingleFrameGPUDump",  ""},
        {"Record Multi-Frame GPU Trace","Graphics",   "Hotkeys", "RecordMultiFrameGPUDump",   ""},

        // ── Free Camera ──
        {"Freecam Toggle",             "Free Camera", "Hotkeys", "FreecamToggle",             ""},
        {"Freecam Reset",              "Free Camera", "Hotkeys", "FreecamReset",              ""},
        {"Freecam Move Left",          "Free Camera", "Hotkeys", "FreecamMoveLeft",           ""},
        {"Freecam Move Right",         "Free Camera", "Hotkeys", "FreecamMoveRight",          ""},
        {"Freecam Move Up",            "Free Camera", "Hotkeys", "FreecamMoveUp",             ""},
        {"Freecam Move Down",          "Free Camera", "Hotkeys", "FreecamMoveDown",           ""},
        {"Freecam Move Forward",       "Free Camera", "Hotkeys", "FreecamMoveForward",        ""},
        {"Freecam Move Backward",      "Free Camera", "Hotkeys", "FreecamMoveBackward",       ""},
        {"Freecam Rotate Left",        "Free Camera", "Hotkeys", "FreecamRotateLeft",         ""},
        {"Freecam Rotate Right",       "Free Camera", "Hotkeys", "FreecamRotateRight",        ""},
        {"Freecam Rotate Forward",     "Free Camera", "Hotkeys", "FreecamRotateForward",      ""},
        {"Freecam Rotate Backward",    "Free Camera", "Hotkeys", "FreecamRotateBackward",     ""},
        {"Freecam Roll Left",          "Free Camera", "Hotkeys", "FreecamRollLeft",           ""},
        {"Freecam Roll Right",         "Free Camera", "Hotkeys", "FreecamRollRight",          ""},

        // ── Audio ──
        {"Toggle Mute",                "Audio",       "Hotkeys", "AudioMute",                 ""},
        {"Toggle CD Audio Mute",       "Audio",       "Hotkeys", "AudioCDAudioMute",          ""},
        {"Volume Up",                  "Audio",       "Hotkeys", "AudioVolumeUp",             ""},
        {"Volume Down",                "Audio",       "Hotkeys", "AudioVolumeDown",           ""},

        // ── Save States (selected/undo) ──
        // SaveSelectedSaveState / LoadSelectedSaveState are force-bound
        // to F5/F7 and synthesized by AppController for the in-game
        // menu's Save State / Load State actions; omitted here so
        // users can't rebind the synth keys.
        {"Select Previous Save Slot",  "Save States", "Hotkeys", "SelectPreviousSaveStateSlot","Keyboard/F3"},
        {"Select Next Save Slot",      "Save States", "Hotkeys", "SelectNextSaveStateSlot",   "Keyboard/F4"},
        {"Save State and Select Next Slot","Save States","Hotkeys","SaveStateAndSelectNextSlot",""},
        {"Undo Load State",            "Save States", "Hotkeys", "UndoLoadState",             ""},

        // ── Debugging ──
        {"Toggle PGXP CPU Mode",       "Debugging",   "Hotkeys", "TogglePGXPCPU",             ""},
        {"Toggle PGXP Preserve Projection Precision","Debugging","Hotkeys","TogglePGXPPreserveProjPrecision",""},
        {"Toggle VRAM View",           "Debugging",   "Hotkeys", "ToggleVRAMView",            ""},

        // ── Save States (per-slot Game) ──
        {"Load Game State 1",          "Save States", "Hotkeys", "LoadGameState1",            ""},
        {"Save Game State 1",          "Save States", "Hotkeys", "SaveGameState1",            ""},
        {"Load Game State 2",          "Save States", "Hotkeys", "LoadGameState2",            ""},
        {"Save Game State 2",          "Save States", "Hotkeys", "SaveGameState2",            ""},
        {"Load Game State 3",          "Save States", "Hotkeys", "LoadGameState3",            ""},
        {"Save Game State 3",          "Save States", "Hotkeys", "SaveGameState3",            ""},
        {"Load Game State 4",          "Save States", "Hotkeys", "LoadGameState4",            ""},
        {"Save Game State 4",          "Save States", "Hotkeys", "SaveGameState4",            ""},
        {"Load Game State 5",          "Save States", "Hotkeys", "LoadGameState5",            ""},
        {"Save Game State 5",          "Save States", "Hotkeys", "SaveGameState5",            ""},
        {"Load Game State 6",          "Save States", "Hotkeys", "LoadGameState6",            ""},
        {"Save Game State 6",          "Save States", "Hotkeys", "SaveGameState6",            ""},
        {"Load Game State 7",          "Save States", "Hotkeys", "LoadGameState7",            ""},
        {"Save Game State 7",          "Save States", "Hotkeys", "SaveGameState7",            ""},
        {"Load Game State 8",          "Save States", "Hotkeys", "LoadGameState8",            ""},
        {"Save Game State 8",          "Save States", "Hotkeys", "SaveGameState8",            ""},
        {"Load Game State 9",          "Save States", "Hotkeys", "LoadGameState9",            ""},
        {"Save Game State 9",          "Save States", "Hotkeys", "SaveGameState9",            ""},
        {"Load Game State 10",         "Save States", "Hotkeys", "LoadGameState10",           ""},
        {"Save Game State 10",         "Save States", "Hotkeys", "SaveGameState10",           ""},

        // ── Save States (per-slot Global) ──
        {"Load Global State 1",        "Save States", "Hotkeys", "LoadGlobalState1",          ""},
        {"Save Global State 1",        "Save States", "Hotkeys", "SaveGlobalState1",          ""},
        {"Load Global State 2",        "Save States", "Hotkeys", "LoadGlobalState2",          ""},
        {"Save Global State 2",        "Save States", "Hotkeys", "SaveGlobalState2",          ""},
        {"Load Global State 3",        "Save States", "Hotkeys", "LoadGlobalState3",          ""},
        {"Save Global State 3",        "Save States", "Hotkeys", "SaveGlobalState3",          ""},
        {"Load Global State 4",        "Save States", "Hotkeys", "LoadGlobalState4",          ""},
        {"Save Global State 4",        "Save States", "Hotkeys", "SaveGlobalState4",          ""},
        {"Load Global State 5",        "Save States", "Hotkeys", "LoadGlobalState5",          ""},
        {"Save Global State 5",        "Save States", "Hotkeys", "SaveGlobalState5",          ""},
        {"Load Global State 6",        "Save States", "Hotkeys", "LoadGlobalState6",          ""},
        {"Save Global State 6",        "Save States", "Hotkeys", "SaveGlobalState6",          ""},
        {"Load Global State 7",        "Save States", "Hotkeys", "LoadGlobalState7",          ""},
        {"Save Global State 7",        "Save States", "Hotkeys", "SaveGlobalState7",          ""},
        {"Load Global State 8",        "Save States", "Hotkeys", "LoadGlobalState8",          ""},
        {"Save Global State 8",        "Save States", "Hotkeys", "SaveGlobalState8",          ""},
        {"Load Global State 9",        "Save States", "Hotkeys", "LoadGlobalState9",          ""},
        {"Save Global State 9",        "Save States", "Hotkeys", "SaveGlobalState9",          ""},
        {"Load Global State 10",       "Save States", "Hotkeys", "LoadGlobalState10",         ""},
        {"Save Global State 10",       "Save States", "Hotkeys", "SaveGlobalState10",         ""},
    };
}

// ============================================================================
// patchRetroAchievements — enable/disable RA in [Cheevos] section
// ============================================================================

EmulatorAdapter::RetroAchievementsKeyMap DuckStationAdapter::retroAchievementsKeyMap() const {
    // DuckStation encrypts its token with a machine-specific key, so we can't
    // pre-patch credentials. The base implementation just toggles the RA prefs;
    // DuckStation prompts the user to log in on first launch.
    return {
        "Cheevos",            // section
        "Enabled",            // enabledKey
        "ChallengeMode",      // hardcoreKey
        "Notifications",      // notificationsKey
        "SoundEffects",       // soundEffectsKey
        "true", "false",      // bool format
        "DuckStation",        // configTag
    };
}

// ============================================================================
// Asset matching — select the right GitHub release asset for this platform
// ============================================================================

QVector<EmulatorAdapter::AssetMatchRule> DuckStationAdapter::assetMatchRules() const {
#if defined(Q_OS_MACOS)
    return { {{"mac"}, ".zip"} };
#elif defined(Q_OS_WIN)
    return { {{"windows", "x64"}, ".zip"} };
#else
    return { {{"linux", "x64"}, ".AppImage"} };
#endif
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

// Hub layout mirrors the upstream SettingsWindow tab list
// (settingswindow.cpp:92-184) in left-to-right reading order, with three
// structural folds vs upstream:
//   - Recommended (curated cross-cut at the top — Dolphin/PCSX2 pattern).
//   - On-Screen Display lives under Graphics as a sub-tab (Dolphin
//     pattern), not as a top-level card.
//   - Capture also lives under Graphics as a sub-tab — capture is
//     graphics-adjacent (screenshots + video output) so the unified
//     Graphics dialog stays clean.
// Other omitted panes (Interface, Game List, BIOS, Post-Processing,
// Debugging) are documented in duckstation-schema-alignment.md.
QVector<SettingsHubCard> DuckStationAdapter::settingsHubCards() const {
    return {
        // Row 0: Recommended — full-width stretch card. Highlighted at the top
        // because it's the curated short list of settings users most commonly
        // tweak (sourced from DuckStation's setup docs + community consensus).
        // Same INI keys as the full panes below, just collected for fast access.
        {QStringLiteral("\U0001F4A1"), "Recommended",
         "Most-tweaked settings — performance, visuals, audio",
         "Recommended", 0, 0, 1, 3},
        // Row 1: Console · Emulation · Memory Cards
        {QStringLiteral("\U0001F39B"), "Console",
         "Region, fast boot, CPU, CD-ROM",
         "Console", 1, 0},
        {QStringLiteral("\U0001F3AE"), "Emulation",
         "Speed, latency, rewind, runahead",
         "Emulation", 1, 1},
        {QStringLiteral("\U0001F4BE"), "Memory Cards",
         "Slots and card types",
         "Memory Cards", 1, 2},
        // Row 2: Graphics · Audio · Achievements
        {QStringLiteral("\U0001F5BC"), "Graphics",
         "Renderer, advanced, textures, OSD, capture",
         "Graphics", 2, 0},
        {QStringLiteral("\U0001F50A"), "Audio",
         "Backend, latency, volume",
         "Audio", 2, 1},
        {QStringLiteral("\U0001F3C6"), "Achievements",
         "RetroAchievements options",
         "Achievements", 2, 2},
        // Row 3: Advanced — full-width stretch row.
        {QStringLiteral("\U0001F527"), "Advanced",
         "Logging",
         "Advanced", 3, 0, 1, 3},
    };
}
