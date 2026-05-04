#include "dolphin_adapter.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>

#include "core/github_client.h"
#include "core/paths.h"

namespace {
constexpr const char* DOLPHIN_INSTALL_FOLDER = "dolphin";
}

// ============================================================================
// Path helpers
// ============================================================================

QString DolphinAdapter::installDir() {
    return Paths::emulatorsDir(DOLPHIN_INSTALL_FOLDER);
}

QString DolphinAdapter::userBaseDir() {
    // Sibling of Dolphin.app, NOT inside the bundle. Modifying anything
    // under Dolphin.app/Contents/ invalidates the code signature, which
    // causes Gatekeeper to flag the binary as "damaged" and refuse to
    // launch it. Dolphin's own --user/-u CLI flag is the documented way
    // to redirect its config directory away from the bundle.
    return installDir() + "/User";
}

QString DolphinAdapter::userConfigDir() {
    return userBaseDir() + "/Config";
}

QStringList DolphinAdapter::additionalLaunchArgs() const {
    return {"-u", userBaseDir()};
}

QString DolphinAdapter::dolphinIniPath()          { return userConfigDir() + "/Dolphin.ini"; }
QString DolphinAdapter::gfxIniPath()              { return userConfigDir() + "/GFX.ini"; }
QString DolphinAdapter::gcpadIniPath()            { return userConfigDir() + "/GCPadNew.ini"; }
QString DolphinAdapter::wiimoteIniPath()          { return userConfigDir() + "/WiimoteNew.ini"; }
QString DolphinAdapter::hotkeysIniPath()          { return userConfigDir() + "/Hotkeys.ini"; }
QString DolphinAdapter::retroAchievementsIniPath(){ return userConfigDir() + "/RetroAchievements.ini"; }

QString DolphinAdapter::configFilePath() const {
    return dolphinIniPath();
}

// ============================================================================
// Executable resolution
// ============================================================================

QString DolphinAdapter::resolveExecutable(const EmulatorManifest& manifest,
                                          const QString& installPath) {
    // The official dl.dolphin-emu.org .dmg ships the binary as `Dolphin`
    // inside `Dolphin.app/Contents/MacOS/`. (Earlier checklist notes said
    // `DolphinQt` based on the CMake target name — that's only the
    // build-time name; the shipped product is `Dolphin`.)
    return resolveExecutableInDir(manifest, installPath, "Dolphin");
}

// ============================================================================
// Asset matching
// ============================================================================

QVector<EmulatorAdapter::AssetMatchRule> DolphinAdapter::assetMatchRules() const {
#if defined(Q_OS_MACOS)
    return {
        {{"macos", "universal"}, ".dmg"},
        {{"macos"},              ".dmg"},
        {{"mac"},                ".dmg"},
    };
#elif defined(Q_OS_WIN)
    return {
        {{"windows", "x64"}, ".zip"},
        {{"windows"},        ".zip"},
    };
#else
    return {
        {{"linux"}, ".AppImage"},
        {{"linux"}, ".tar.xz"},
    };
#endif
}

// ============================================================================
// Direct download (Dolphin distributes via dl.dolphin-emu.org, not GitHub)
// ============================================================================

EmulatorAdapter::DirectDownloadInfo
DolphinAdapter::resolveDirectDownload(const EmulatorManifest& manifest) const {
    DirectDownloadInfo info;

    // Dolphin's stable tags are 4-digit version numbers with optional letter
    // suffix (e.g. 2603, 2603a). The repo also has legacy/unrelated tags
    // like "nJoy" — filter to the stable pattern.
    static const QRegularExpression stablePattern("^(\\d{4})([a-z]?)$");
    const QString tag = GitHubClient::fetchLatestStableTag(manifest.github_repo, stablePattern);
    if (tag.isEmpty()) {
        qWarning() << "[Dolphin] Could not resolve latest stable tag from"
                   << manifest.github_repo << "tags";
        return info;
    }

#if defined(Q_OS_MACOS)
    info.assetName = QString("dolphin-%1-universal.dmg").arg(tag);
    info.downloadUrl = QString("https://dl.dolphin-emu.org/releases/%1/%2")
                           .arg(tag, info.assetName);
#else
    // Other platforms: dl.dolphin-emu.org publishes a different naming scheme;
    // fall back to GitHub Releases for now (which will fail loudly, surfacing
    // that non-mac install is unimplemented). When you add Win/Linux support,
    // construct the right URL pattern here.
    qWarning() << "[Dolphin] Direct download URL pattern only implemented for macOS";
    return info;
#endif

    info.version = tag;
    info.publishedAt = tag;  // Dolphin tags are unique per release; doubles as the update key.
    qInfo() << "[Dolphin] Resolved direct download:" << info.downloadUrl;
    return info;
}

// ============================================================================
// BIOS files (GameCube IPL — optional; SkipIPL is set in Dolphin.ini)
// ============================================================================

QVector<BiosDef> DolphinAdapter::biosFiles() const {
    return {
        {"GC/USA/IPL.bin", "GameCube IPL (NTSC-U)", false, ""},
        {"GC/EUR/IPL.bin", "GameCube IPL (PAL)",     false, ""},
        {"GC/JAP/IPL.bin", "GameCube IPL (NTSC-J)",  false, ""},
    };
}

// ============================================================================
// Path defs (per-system data dirs under emulators/dolphin/{gc,wii}/)
// ============================================================================

QVector<PathDef> DolphinAdapter::pathsDefs() const {
    // Dolphin doesn't expose individual folder-path keys for savestates etc.
    // (it derives them from User dir). We declare them so RetroNest creates
    // the per-system data directories on first launch — Dolphin reads them
    // because we point its ISOPath0/ISOPath1 keys at gc/ and wii/ in
    // patchDolphinIni(), and the standard subfolders are created by Dolphin
    // itself the first time it writes savestates/screenshots/etc.
    return {
        {"Save States",  "", "", "savestates",  PathBase::EmulatorData},
        {"Screenshots",  "", "", "screenshots", PathBase::EmulatorData},
        {"Cheats",       "", "", "cheats",      PathBase::EmulatorData},
        {"Textures",     "", "", "textures",    PathBase::EmulatorData},
        {"Cache",        "", "", "cache",       PathBase::EmulatorData},
    };
}

// ============================================================================
// Resolution / aspect ratio (route writes to GFX.ini via the iniFilePath field)
// ============================================================================

ResolutionOptions DolphinAdapter::resolutionOptions() const {
    return {
        "Settings", "InternalResolution",
        {
            {"Native (1x)",   "1"},
            {"2x (~720p)",    "2"},
            {"3x (~1080p)",   "3"},
            {"4x (~1440p)",   "4"},
            {"5x (~1800p)",   "5"},
            {"6x (~4K)",      "6"},
        },
        "1",
        gfxIniPath(),
    };
}

AspectRatioOptions DolphinAdapter::aspectRatioOptions() const {
    const QString gfx = gfxIniPath();
    return {
        {
            {"Auto",       {{"Settings", "AspectRatio", "0", gfx}}},
            {"Force 16:9", {{"Settings", "AspectRatio", "1", gfx}}},
            {"Force 4:3",  {{"Settings", "AspectRatio", "2", gfx}}},
            {"Stretch",    {{"Settings", "AspectRatio", "3", gfx}}},
        },
        "Auto"
    };
}

// ============================================================================
// Settings schema — Dolphin.ini + GFX.ini (per SettingDef::iniFilePath)
// ============================================================================

QVector<SettingDef> DolphinAdapter::settingsSchema() const {
    // Field order: category, subcategory, group, section, key, label, tooltip,
    //              type, defaultValue, options, minVal, maxVal, step, layout, suffix
    // All values use Dolphin's True/False capitalization (Common/StringUtil.cpp:289-292).

    // Helper: stamp a SettingDef with GFX.ini as the target file.
    // gfxIniPath() is static, so no capture needed.
    auto gfx = [](SettingDef d) { d.iniFilePath = gfxIniPath(); return d; };

    const QVector<QPair<QString,QString>> audioBackends = {
        {"Cubeb",   "Cubeb"},
        {"OpenAL",  "OpenAL"},
        {"Null",    "Null"},
    };

    const QVector<QPair<QString,QString>> cpuCores = {
        {"Interpreter (slow, accurate)",  "0"},
        {"Cached Interpreter",             "5"},
        {"JIT Recompiler (recommended)",   "1"},
        {"JITARM64 (Apple Silicon)",       "4"},
    };

    // Cursor visibility — written as the int underlying ShowCursor enum
    // (MainSettings.h:255-261). DolphinQt renders this as 3 radio buttons.
    const QVector<QPair<QString,QString>> cursorModes = {
        {"Never",        "0"},
        {"Always",       "1"},
        {"On Movement",  "2"},
    };

    // Region fallback — DiscIO::Region enum (Source/Core/DiscIO/Enums.h).
    const QVector<QPair<QString,QString>> regions = {
        {"NTSC-J (Japan)",       "0"},
        {"NTSC-U (Americas)",    "1"},
        {"PAL (Europe)",         "2"},
        {"Unknown / region-free","3"},
        {"NTSC-K (Korea, Wii)",  "4"},
    };

    // Language list mirrors DolphinQt MakeLanguageComboBox()
    // (InterfacePane.cpp). Stored as ISO 639-1 code (or "" for system default).
    const QVector<QPair<QString,QString>> languages = {
        {"<System Language>",       ""},
        {"Bahasa Melayu",           "ms"},
        {"Català",                  "ca"},
        {"Čeština",                 "cs"},
        {"Dansk",                   "da"},
        {"Deutsch",                 "de"},
        {"English",                 "en"},
        {"Español",                 "es"},
        {"Français",                "fr"},
        {"Hrvatski",                "hr"},
        {"Italiano",                "it"},
        {"Magyar",                  "hu"},
        {"Nederlands",              "nl"},
        {"Norsk bokmål",            "nb"},
        {"Polski",                  "pl"},
        {"Português",               "pt"},
        {"Português (Brasil)",      "pt_BR"},
        {"Română",                  "ro"},
        {"Srpski",                  "sr"},
        {"Suomi",                   "fi"},
        {"Svenska",                 "sv"},
        {"Türkçe",                  "tr"},
        {"Ελληνικά",               "el"},
        {"Русский",                 "ru"},
        {"العربية",                 "ar"},
        {"فارسی",                   "fa"},
        {"한국어",                   "ko"},
        {"日本語",                   "ja"},
        {"简体中文",                  "zh_CN"},
        {"繁體中文",                  "zh_TW"},
    };

    return {
        // ─── Interface / Window ──────────────────────────────
        {"Interface", "", "Window", "Interface", "PauseOnFocusLost",
         "Pause When Window Loses Focus",
         "Pauses emulation automatically when the RetroNest window loses focus. "
         "Required for the in-game overlay to work cleanly.",
         SettingDef::Bool, "True"},

        {"Interface", "", "Window", "Interface", "ConfirmStop",
         "Confirm Before Stopping Emulation",
         "Show a confirmation dialog when stopping a game from Dolphin's UI. "
         "Disabled by default so RetroNest's own exit flow is uninterrupted.",
         SettingDef::Bool, "False"},

        {"Interface", "", "Window", "Display", "KeepWindowOnTop",
         "Keep Window On Top",
         "Forces the Dolphin window to stay above other windows during play.",
         SettingDef::Bool, "False"},

        {"Interface", "", "Window", "Display", "DisableScreenSaver",
         "Disable Screensaver During Play",
         "Prevents the system screensaver from kicking in while a game is running.",
         SettingDef::Bool, "True"},

        // ─── Interface / Cursor ──────────────────────────────
        {"Interface", "", "Cursor", "Interface", "CursorVisibility",
         "Mouse Cursor",
         "When the mouse cursor is shown over the render area. "
         "Never = always hidden; Always = always shown; On Movement = "
         "auto-hide after a short idle period.",
         SettingDef::Combo, "2", cursorModes},

        {"Interface", "", "Cursor", "Interface", "LockCursor",
         "Lock Cursor To Window",
         "Confines the mouse cursor to the render window. Useful for mouse-driven "
         "Wii-pointer games. Windows-only effect upstream.",
         SettingDef::Bool, "False"},

        // ─── Interface / Language ────────────────────────────
        {"Interface", "", "Language", "Interface", "LanguageCode",
         "Dolphin UI Language",
         "Language used by Dolphin's own UI (when you open native settings). "
         "Does not change RetroNest's language. \"System Language\" matches the OS.",
         SettingDef::Combo, "", languages},

        {"Interface", "", "Language", "Core", "FallbackRegion",
         "Fallback Region",
         "Region used for games whose region cannot be auto-detected. "
         "Affects boot timing and the system menu locale.",
         SettingDef::Combo, "1", regions},

        // ─── Interface / Library ─────────────────────────────
        {"Interface", "", "Library", "Interface", "ShowActiveTitle",
         "Show Active Title In Window Title",
         "Appends the running game's title to the window-title bar.",
         SettingDef::Bool, "True"},

        {"Interface", "", "Library", "Interface", "UseBuiltinTitleDatabase",
         "Use Built-In Title Database",
         "Use Dolphin's bundled game-name database to display friendly titles "
         "in the game list.",
         SettingDef::Bool, "True"},

        {"Interface", "", "Library", "General", "UseGameCovers",
         "Download Game Covers",
         "When enabled, Dolphin downloads cover art for known games to display "
         "in its native game list. RetroNest scrapes its own art separately.",
         SettingDef::Bool, "False"},

        // ─── Interface / Other ───────────────────────────────
        {"Interface", "", "Other", "Interface", "UsePanicHandlers",
         "Use Panic Handlers",
         "Show pop-up dialogs for non-fatal emulation errors. Disable for "
         "kiosk-style play; enable for debugging.",
         SettingDef::Bool, "True"},

        {"Interface", "", "Other", "General", "HotkeysRequireFocus",
         "Hotkeys Require Window Focus",
         "When on, Dolphin's native hotkeys only fire while its window has focus. "
         "RetroNest's overlay hotkeys are not affected.",
         SettingDef::Bool, "True"},

        {"Interface", "", "Other", "General", "EnablePlayTimeTracking",
         "Track Play Time",
         "Record per-game playtime in Dolphin's title database.",
         SettingDef::Bool, "True"},

        // ─── Audio / Output ──────────────────────────────────
        {"Audio", "", "Output", "DSP", "Backend",
         "Audio Backend",
         "Sound output backend. Cubeb is recommended on macOS.",
         SettingDef::Combo, "Cubeb", audioBackends},

        {"Audio", "", "Output", "DSP", "Volume",
         "Volume",
         "Master output volume (0-100).",
         SettingDef::Int, "100", {}, 0, 100, 1, "slider", "%"},

        {"Audio", "", "Output", "DSP", "MuteOnDisabledSpeedLimit",
         "Mute When Speed Limit Disabled",
         "Silence audio while emulation is running unthrottled (e.g. fast-forward). "
         "Avoids unpleasant pitch/playback artifacts.",
         SettingDef::Bool, "False"},

        // ─── Audio / DSP Emulation ───────────────────────────
        {"Audio", "", "DSP Emulation", "Core", "DSPHLE",
         "DSP HLE",
         "High-Level Emulation of the DSP — fast and compatible with most games. "
         "Disable to use LLE (slower, more accurate; required for a small set of titles).",
         SettingDef::Bool, "True"},

        {"Audio", "", "DSP Emulation", "DSP", "EnableJIT",
         "DSP LLE — Enable JIT",
         "When DSP HLE is off (LLE mode), use the JIT recompiler. "
         "Significant performance improvement; leave on unless debugging.",
         SettingDef::Bool, "True"},

        // ─── Audio / Latency & Quality ───────────────────────
        {"Audio", "", "Latency & Quality", "Core", "AudioLatency",
         "Audio Latency",
         "Output latency in milliseconds. Lower = tighter sync, more risk of "
         "audio dropouts under load. 20 ms is a sensible default.",
         SettingDef::Int, "20", {}, 0, 200, 1, "slider", "ms"},

        {"Audio", "", "Latency & Quality", "Core", "AudioBufferSize",
         "Audio Buffer Size",
         "Internal mixer buffer in milliseconds. Higher = smoother but more "
         "delay between picture and sound.",
         SettingDef::Int, "80", {}, 16, 512, 1, "slider", "ms"},

        {"Audio", "", "Latency & Quality", "Core", "AudioFillGaps",
         "Fill Audio Gaps",
         "Synthesize silence to fill gaps when emulation can't keep up. "
         "Disable for a more accurate native-hardware behavior; enable for smoothness.",
         SettingDef::Bool, "True"},

        {"Audio", "", "Latency & Quality", "Core", "AudioPreservePitch",
         "Preserve Pitch When Speed Changes",
         "Time-stretch audio to keep pitch constant when emulation runs slower or "
         "faster than 100%. Useful with fast-forward; otherwise leaves pitch unchanged.",
         SettingDef::Bool, "False"},

        // ─── Audio / Surround ────────────────────────────────
        {"Audio", "", "Surround", "Core", "DPL2Decoder",
         "Dolby Pro Logic II Decoder",
         "Decode the emulated stereo mix into 5.1 surround output. Requires a "
         "backend that supports it — Cubeb on macOS does not, so this is "
         "effectively a no-op there. Active only when DSP HLE is off.",
         SettingDef::Bool, "False"},

        {"Audio", "", "Surround", "Core", "DPL2Quality",
         "DPL2 Decoder Quality",
         "Trade-off between CPU cost and surround-decode accuracy.",
         SettingDef::Combo, "2",
         { {"Lowest",  "0"}, {"Low",   "1"},
           {"High",    "2"}, {"Highest","3"} },
         0, 0, 0, "", "", "DPL2Decoder"},  // gated on DPL2Decoder

        // ─── General / CPU ───────────────────────────────────
        {"General", "", "CPU", "Core", "CPUCore",
         "CPU Core",
         "The CPU emulation backend. JIT is required for full-speed gameplay.",
         SettingDef::Combo, "1", cpuCores},

        {"General", "", "CPU", "Core", "CPUThread",
         "Dual Core Mode",
         "Splits CPU and GPU emulation across two threads. Significant speed "
         "gain; some games rely on tight CPU/GPU sync and may glitch with it on.",
         SettingDef::Bool, "False"},

        {"General", "", "CPU", "Core", "MMU",
         "Enable MMU",
         "Emulates the memory management unit. Slower but required for a small "
         "set of games (typically Virtual Console / homebrew).",
         SettingDef::Bool, "False"},

        {"General", "", "CPU", "Core", "AccurateCPUCache",
         "Accurate CPU Instruction Cache",
         "Emulate the CPU's L1 instruction cache. Slower but more accurate; "
         "needed for a handful of games that self-modify code.",
         SettingDef::Bool, "False"},

        // ─── General / Boot & Cheats ─────────────────────────
        {"General", "", "Boot & Cheats", "Core", "SkipIPL",
         "Skip GameCube Boot Animation",
         "Skips the GameCube IPL boot sequence and starts the game directly. "
         "When disabled, requires IPL.bin in the BIOS folder.",
         SettingDef::Bool, "True"},

        {"General", "", "Boot & Cheats", "Core", "EnableCheats",
         "Enable Cheats",
         "Enables AR/Gecko cheat code processing. Off by default for safety.",
         SettingDef::Bool, "False"},

        {"General", "", "Boot & Cheats", "Core", "AutoDiscChange",
         "Auto Disc Change",
         "Switch discs automatically for multi-disc games (M3U). RetroNest's "
         "M3U handling is independent and runs at launch time.",
         SettingDef::Bool, "False"},

        {"General", "", "Boot & Cheats", "Core", "OverrideRegionSettings",
         "Override Region Settings",
         "Force a region's settings (language, video mode) regardless of disc region.",
         SettingDef::Bool, "False"},

        // ─── General / Speed ─────────────────────────────────
        {"General", "", "Speed", "Core", "EmulationSpeed",
         "Emulation Speed",
         "Cap on emulated speed relative to native. Unlimited = no throttle. "
         "Stored as a float multiplier; combo offers common breakpoints.",
         SettingDef::Combo, "1.000000",
         { {"Unlimited",  "0.000000"},
           {"25%",        "0.250000"},
           {"50%",        "0.500000"},
           {"75%",        "0.750000"},
           {"100%",       "1.000000"},
           {"125%",       "1.250000"},
           {"150%",       "1.500000"},
           {"200%",       "2.000000"},
           {"300%",       "3.000000"} }},

        {"General", "", "Speed", "Core", "CorrectTimeDrift",
         "Correct Time Drift",
         "Compensate for accumulated frame-pacing drift over long sessions. "
         "Smooths long-term sync at the cost of micro-stutter.",
         SettingDef::Bool, "False"},

        {"General", "", "Speed", "Core", "RushFramePresentation",
         "Rush Frame Presentation",
         "Aggressively present frames as soon as they're ready. Lower latency, "
         "more tearing without VSync.",
         SettingDef::Bool, "False"},

        {"General", "", "Speed", "Core", "SmoothEarlyPresentation",
         "Smooth Early Presentation",
         "Smooth pacing for frames that finish ahead of schedule.",
         SettingDef::Bool, "False"},

        // ─── General / Overclock ─────────────────────────────
        {"General", "", "Overclock", "Core", "OverclockEnable",
         "Enable CPU Overclocking",
         "Allow the slider below to scale the emulated CPU's clock rate. "
         "Some games run smoother with overclocking; others crash.",
         SettingDef::Bool, "False"},

        // Overclock multiplier — slider widget is integer-only (cast via
        // int(minVal)/int(maxVal)), so whole-number bounds. minVal=1
        // keeps the emulated CPU at native or above; below that Dolphin
        // reads the float as 0.0 and stalls. dependsOn gates on enable.
        {"General", "", "Overclock", "Core", "Overclock",
         "CPU Overclock Multiplier",
         "Multiplier applied to the emulated CPU's clock when overclocking is enabled. "
         "1 = native, 2 = +100%, 4 = +300%.",
         SettingDef::Int, "1", {}, 1, 4, 1, "slider", "x", "OverclockEnable"},

        {"General", "", "Overclock", "Core", "VIOverclockEnable",
         "Enable VI (Video Interface) Overclocking",
         "Allow scaling the video-interface clock independently of the CPU. "
         "Affects refresh-rate timing for some games.",
         SettingDef::Bool, "False"},

        {"General", "", "Overclock", "Core", "VIOverclock",
         "VI Overclock Multiplier",
         "Multiplier applied to the VI clock when VI overclocking is enabled.",
         SettingDef::Int, "1", {}, 1, 4, 1, "slider", "x", "VIOverclockEnable"},

        // ─── General / Memory (Advanced) ─────────────────────
        {"General", "", "Memory (Advanced)", "Core", "RAMOverrideEnable",
         "Override RAM Sizes",
         "Allow using non-retail RAM sizes (MEM1/MEM2). Required for some "
         "homebrew. Most games will not boot with non-retail sizes.",
         SettingDef::Bool, "False"},

        {"General", "", "Memory (Advanced)", "Core", "LoadGameIntoMemory",
         "Load Game Into Memory",
         "Pre-loads the entire game image into RAM at boot. Eliminates disc "
         "I/O stutter; uses more host memory.",
         SettingDef::Bool, "False"},

        // ─── General / Misc ──────────────────────────────────
        {"General", "", "Misc", "Core", "PauseOnPanic",
         "Pause On Panic",
         "Pause emulation when Dolphin reports a non-fatal error.",
         SettingDef::Bool, "False"},

        {"General", "", "Misc", "Core", "EnableCustomRTC",
         "Enable Custom Real-Time Clock",
         "Override the emulated system clock with a fixed date/time. "
         "Set the value via Dolphin's native UI — RetroNest doesn't yet expose "
         "a date picker.",
         SettingDef::Bool, "False"},

        {"General", "", "Misc", "General", "UseDiscordPresence",
         "Discord Rich Presence",
         "Publish currently-playing game to Discord status when Dolphin runs.",
         SettingDef::Bool, "True"},

        // ─── Graphics / Display ──────────────────────────────
        // AspectRatio, InternalResolution, VSync live in
        // GFX.ini → wrap in gfx(). Fullscreen lives in Dolphin.ini →
        // omit gfx() so it inherits configFilePath() = Dolphin.ini.
        gfx({"Graphics", "Display", "", "Settings", "AspectRatio",
         "Aspect Ratio",
         "Display aspect ratio. Auto matches the game's native aspect; "
         "Stretch fills the screen.",
         SettingDef::Combo, "0",
         { {"Auto","0"}, {"Force 16:9","1"}, {"Force 4:3","2"}, {"Stretch","3"} }}),

        gfx({"Graphics", "Display", "", "Settings", "InternalResolution",
         "Internal Resolution",
         "Render scale relative to native (1x = original, 6x ≈ 4K).",
         SettingDef::Combo, "1",
         { {"Native (1x)","1"}, {"2x (~720p)","2"}, {"3x (~1080p)","3"},
           {"4x (~1440p)","4"}, {"5x (~1800p)","5"}, {"6x (~4K)","6"} }}),

        gfx({"Graphics", "Display", "", "Hardware", "VSync",
         "VSync",
         "Synchronizes output to the display refresh rate. Reduces tearing.",
         SettingDef::Bool, "True"}),

        // No gfx() wrapper — Fullscreen is in Dolphin.ini's [Display] section.
        {"Graphics", "Display", "", "Display", "Fullscreen",
         "Fullscreen",
         "Render in fullscreen mode. RetroNest already runs Dolphin "
         "embedded in our window, so this is True by default.",
         SettingDef::Bool, "True"},

        // ─── Graphics / Rendering ────────────────────────────
        // All five Rendering keys live in GFX.ini → wrap each in gfx().
        gfx({"Graphics", "Rendering", "", "Settings", "MSAA",
         "Anti-Aliasing (MSAA)",
         "Multi-sample anti-aliasing. Higher = smoother edges, slower.",
         SettingDef::Combo, "1",
         { {"None","1"}, {"2x","2"}, {"4x","4"}, {"8x","8"} }}),

        gfx({"Graphics", "Rendering", "", "Enhancements", "MaxAnisotropy",
         "Anisotropic Filtering",
         "Sharpens textures viewed at oblique angles.",
         SettingDef::Combo, "-1",
         { {"Default","-1"}, {"Off (1x)","0"}, {"2x","1"}, {"4x","2"}, {"8x","3"}, {"16x","4"} }}),

        gfx({"Graphics", "Rendering", "", "Settings", "ShaderCompilationMode",
         "Shader Compilation",
         "How shaders are compiled. Asynchronous reduces stutter at the "
         "cost of brief texture/lighting pop-in on first encounter.",
         SettingDef::Combo, "0",
         { {"Specialized (Default)","0"}, {"Exclusive Ubershaders","1"},
           {"Hybrid Ubershaders","2"}, {"Skip Drawing","3"} }}),

        gfx({"Graphics", "Rendering", "", "Settings", "WaitForShadersBeforeStarting",
         "Compile Shaders Before Starting",
         "Pre-compiles the shader pipeline before launching a game. "
         "Slower start, smoother gameplay.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Rendering", "", "Settings", "EnablePixelLighting",
         "Per-Pixel Lighting",
         "Higher-quality lighting. Slight performance cost; some games "
         "look noticeably better with it on.",
         SettingDef::Bool, "False"}),
    };
}

// ============================================================================
// ensureConfig — multi-file
// ============================================================================

bool DolphinAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                   const QString& /*biosPath*/,
                                   const QString& /*savesPath*/) {
    // 1) Ensure User/Config exists, sibling to (NOT inside) the .app bundle.
    //    Dolphin is pointed at this directory via the `-u` CLI flag — see
    //    additionalLaunchArgs(). We deliberately do NOT write portable.txt
    //    inside the bundle: that would invalidate the macOS code signature
    //    and Gatekeeper would refuse to launch the binary ("a sealed
    //    resource is missing or invalid").
    if (!QDir().mkpath(userConfigDir())) {
        qWarning() << "[Dolphin] Failed to create" << userConfigDir();
        return false;
    }

    // 3) Compute per-system data roots used in Dolphin.ini ISOPath* keys.
    const QString dataRootGc  = Paths::emulatorDataDir(DOLPHIN_INSTALL_FOLDER, "gc");
    const QString dataRootWii = Paths::emulatorDataDir(DOLPHIN_INSTALL_FOLDER, "wii");
    QDir().mkpath(dataRootGc);
    QDir().mkpath(dataRootWii);

    // 4) Patch each config file. Each helper is independently idempotent
    //    and logs its own failures — keep going so a single bad file
    //    doesn't block a launch entirely.
    bool ok = true;
    ok &= patchDolphinIni(dataRootGc, dataRootWii);
    ok &= patchGfxIni();
    ok &= writeGcPadDefaultsIfMissing();
    ok &= writeWiimoteDefaultsIfMissing();
    ok &= patchHotkeysIni();
    return ok;
}

bool DolphinAdapter::patchDolphinIni(const QString& dataRootGc, const QString& dataRootWii) {
    const QString path = dolphinIniPath();
    QString content;

    if (QFile::exists(path)) {
        if (!readConfigFile(path, content, "Dolphin"))
            return false;
    } else {
        content = "";  // patchIniKeys will append all sections + keys
    }

    const QVector<IniKeyPatch> patches = {
        // Interface — pause-on-focus, no exit confirmation, no system-cursor flicker.
        {"Interface", "PauseOnFocusLost",  "True"},
        {"Interface", "ConfirmStop",        "False"},
        {"Interface", "HideCursor",         "True"},

        // Display — fullscreen render, render to main window (avoid second window).
        {"Display", "Fullscreen",   "True"},
        {"Display", "RenderToMain", "True"},

        // Core — boot directly without IPL bootrom requirement.
        {"Core", "SkipIPL",      "True"},
        {"Core", "EnableCheats", "False"},

        // Analytics — opt out of Dolphin's usage statistics and mark the
        // first-run consent prompt as already shown so the dialog doesn't
        // pop up every time we open the native UI.
        // Source: Source/Core/Core/Config/MainSettings.cpp:452-454.
        {"Analytics", "Enabled",         "False"},
        {"Analytics", "PermissionAsked", "True"},

        // General — point ISO scanning at our per-system data dirs so any
        // saves/states Dolphin writes via its own UI go to the right place.
        // Note: these are write-targets, not ROM-scan paths (RetroNest scans
        // ROMs directly — we just give Dolphin a sensible default).
        {"General", "ISOPath0",          dataRootGc},
        {"General", "ISOPath1",          dataRootWii},
        {"General", "ISOPaths",          "2"},
        {"General", "RecursiveISOPaths", "True"},
    };

    if (patchIniKeys(content, patches))
        return writeConfigFile(path, content, "Dolphin");
    // Even if no patches changed, ensure the file exists on disk.
    if (!QFile::exists(path))
        return writeConfigFile(path, content, "Dolphin");
    return true;
}
bool DolphinAdapter::patchGfxIni() {
    const QString path = gfxIniPath();
    QString content;

    if (QFile::exists(path)) {
        if (!readConfigFile(path, content, "Dolphin"))
            return false;
    } else {
        content = "";
    }

    const QVector<IniKeyPatch> patches = {
        {"Hardware", "VSync", "True"},
        // AspectRatio + InternalResolution are user-tunable through the wizard;
        // we only seed them if the file is fresh.
    };

    bool wrote = false;
    if (patchIniKeys(content, patches)) {
        if (!writeConfigFile(path, content, "Dolphin"))
            return false;
        wrote = true;
    }

    // Seed defaults only if these keys are absent (avoid overwriting user choices).
    // Use the trailing-space form ("AspectRatio ") consistently so a key like
    // "AspectRatioGreaterThan" can't accidentally satisfy the check.
    QVector<IniKeyPatch> seedPatches;
    if (!content.contains("AspectRatio "))
        seedPatches.append({"Settings", "AspectRatio", "0"});
    if (!content.contains("InternalResolution "))
        seedPatches.append({"Settings", "InternalResolution", "1"});
    if (!seedPatches.isEmpty() && patchIniKeys(content, seedPatches))
        return writeConfigFile(path, content, "Dolphin");

    if (!wrote && !QFile::exists(path))
        return writeConfigFile(path, content, "Dolphin");
    return true;
}
bool DolphinAdapter::writeGcPadDefaultsIfMissing() {
    const QString path = gcpadIniPath();
    if (QFile::exists(path))
        return true;  // never overwrite — user may have customized via Dolphin's UI

    // Default GCPad1 profile: Standard Controller mapped to SDL gamepad 0.
    // The literal `<device>` placeholder below is replaced at write time with
    // the SDL controller display name pattern. We use the GameController
    // catch-all since most users plug in PS/Xbox-style pads which Dolphin
    // exposes via its SDL backend with a "Wireless Controller" or similar name.
    //
    // If a user has an unusual gamepad that doesn't match this pattern,
    // they can re-map through Dolphin's native UI (which we leave untouched
    // on subsequent launches).
    const char* profile = R"INI([GCPad1]
Device = SDL/0/Wireless Controller
Buttons/A = `Button S`
Buttons/B = `Button E`
Buttons/X = `Button W`
Buttons/Y = `Button N`
Buttons/Z = `Shoulder R`
Buttons/Start = `Start`
D-Pad/Up = `Pad N`
D-Pad/Down = `Pad S`
D-Pad/Left = `Pad W`
D-Pad/Right = `Pad E`
Main Stick/Up = `Left Y-`
Main Stick/Down = `Left Y+`
Main Stick/Left = `Left X-`
Main Stick/Right = `Left X+`
C-Stick/Up = `Right Y-`
C-Stick/Down = `Right Y+`
C-Stick/Left = `Right X-`
C-Stick/Right = `Right X+`
Triggers/L = `Trigger L`
Triggers/R = `Trigger R`
Triggers/L-Analog = `Trigger L`
Triggers/R-Analog = `Trigger R`
Rumble/Motor = `Motor`
)INI";

    QString content = QString::fromUtf8(profile);
    return writeConfigFile(path, content, "Dolphin");
}
bool DolphinAdapter::writeWiimoteDefaultsIfMissing() {
    const QString path = wiimoteIniPath();
    if (QFile::exists(path))
        return true;  // never overwrite

    // Default Wiimote1 profile: emulated Wiimote (no real Wii Remote needed),
    // sideways orientation, no extension. Maps to SDL gamepad 0.
    // This is a minimal "playable" profile — full Wii Remote IR/motion
    // mapping is out of scope for v1; users who need it remap via Dolphin's
    // native UI.
    const char* profile = R"INI([Wiimote1]
Source = 1
Device = SDL/0/Wireless Controller
Options/Sideways Wiimote = True
Buttons/A = `Button S`
Buttons/B = `Button E`
Buttons/1 = `Button W`
Buttons/2 = `Button N`
Buttons/- = `Back`
Buttons/+ = `Start`
Buttons/Home = `Guide`
D-Pad/Up = `Pad N`
D-Pad/Down = `Pad S`
D-Pad/Left = `Pad W`
D-Pad/Right = `Pad E`
Shake/X = `Shoulder L`
Shake/Y = `Shoulder L`
Shake/Z = `Shoulder L`
IR/Up = `Right Y-`
IR/Down = `Right Y+`
IR/Left = `Right X-`
IR/Right = `Right X+`
Tilt/Forward = `Left Y-`
Tilt/Backward = `Left Y+`
Tilt/Left = `Left X-`
Tilt/Right = `Left X+`
Rumble/Motor = `Motor`
)INI";

    QString content = QString::fromUtf8(profile);
    return writeConfigFile(path, content, "Dolphin");
}
bool DolphinAdapter::patchHotkeysIni() {
    const QString path = hotkeysIniPath();
    QString content;

    if (QFile::exists(path)) {
        if (!readConfigFile(path, content, "Dolphin"))
            return false;
    } else {
        // Create an empty file with just the [Hotkeys] section so Dolphin
        // doesn't auto-populate defaults that conflict with our overlay.
        content = "[Hotkeys]\n";
    }

    // Clear native hotkeys that compete with our Cmd+Esc overlay or
    // automatic save-on-exit logic. Dolphin's expression parser tolerates
    // empty values (returns 0/false, no crash).
    const QVector<IniKeyPatch> patches = {
        {"Hotkeys", "General/Toggle Pause",       ""},
        {"Hotkeys", "General/Open",                ""},
        {"Hotkeys", "General/Exit",                ""},
        {"Hotkeys", "Save State/Save State Slot 1", ""},
        {"Hotkeys", "Load State/Load State Slot 1", ""},
    };

    if (patchIniKeys(content, patches))
        return writeConfigFile(path, content, "Dolphin");
    if (!QFile::exists(path))
        return writeConfigFile(path, content, "Dolphin");
    return true;
}

// ============================================================================
// RetroAchievements
// ============================================================================

void DolphinAdapter::patchRetroAchievements(const QString& /*username*/, const QString& /*token*/,
                                             bool enabled, bool hardcore,
                                             bool notifications, bool sounds) {
    const QString path = retroAchievementsIniPath();
    QString content;

    if (QFile::exists(path)) {
        if (!readConfigFile(path, content, "Dolphin"))
            return;
    } else {
        content = "[Achievements]\n";
    }

    const QString trueVal  = "True";
    const QString falseVal = "False";

    QVector<IniKeyPatch> patches = {
        {"Achievements", "Enabled",         enabled   ? trueVal : falseVal},
        {"Achievements", "HardcoreEnabled", hardcore  ? trueVal : falseVal},
        // ProgressEnabled maps to our "notifications" toggle (progress/challenge pop-ups).
        // Dolphin has no dedicated sound-effects key in AchievementSettings.cpp, so
        // the `sounds` parameter is left unused.
        {"Achievements", "ProgressEnabled", notifications ? trueVal : falseVal},
    };
    Q_UNUSED(sounds); // No SoundEnabled key exists in Dolphin's AchievementSettings.cpp.

    if (patchIniKeys(content, patches))
        writeConfigFile(path, content, "Dolphin");
}

// ============================================================================
// Preview spec
// ============================================================================

PreviewSpec DolphinAdapter::previewSpec(const QString& category,
                                        const QString& subcategory) const {
    if (category == "Graphics" && subcategory == "Display") {
        // Dolphin only exposes aspect ratio in a way that maps to the shared
        // AspectRatioPreview. Stretch / crop / integer-scaling are not
        // GFX.ini keys in Dolphin, so the preview shows just the aspect
        // rectangle and leaves the other Q_PROPERTYs at their feature-absent
        // defaults.
        return {"aspect", {
            {"AspectRatio", "aspectMode"},
        }};
    }
    return {};
}
