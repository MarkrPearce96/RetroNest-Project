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
        // ═══ Recommended ═══════════════════════════════════════════
        // Curated short list of the settings users most commonly change,
        // sourced from Dolphin's official performance guide
        // (https://dolphin-emu.org/docs/guides/performance-guide/) plus
        // community consensus (Reddit / XDA / Quora 2026 guides).
        //
        // These entries DUPLICATE keys that also appear under their
        // primary category (Graphics, General, Audio, etc.) — both write
        // to the same INI section/key, so editing here and editing in
        // the full pane produce the same result. The Recommended card
        // is a curated VIEW for users who don't want to hunt through
        // every sub-tab to find the dozen settings that actually
        // matter for most games.

        // Performance — biggest impact for getting games playable
        {"Recommended", "", "Performance", "Core", "GFXBackend",
         "Graphics Backend",
         "Renderer used to draw frames. Switching backends often produces "
         "the single biggest performance change. Metal is the macOS default; "
         "Vulkan via MoltenVK is also fast.",
         SettingDef::Combo, "Metal",
         { {"Metal","Metal"}, {"Vulkan","Vulkan"}, {"OpenGL","OGL"},
           {"Software Renderer","Software Renderer"}, {"Null","Null"} }},

        {"Recommended", "", "Performance", "Core", "CPUThread",
         "Dual Core Mode",
         "Splits CPU and GPU emulation across two threads. Significant "
         "speed gain for most games; a few may glitch with it on.",
         SettingDef::Bool, "False"},

        gfx({"Recommended", "", "Performance", "Settings", "ShaderCompilationMode",
         "Shader Compilation",
         "Asynchronous Ubershaders (Hybrid) avoids long pauses for shader "
         "compilation at the cost of brief texture pop-in.",
         SettingDef::Combo, "0",
         { {"Specialized (Default)","0"}, {"Exclusive Ubershaders","1"},
           {"Hybrid Ubershaders","2"}, {"Skip Drawing","3"} }}),

        gfx({"Recommended", "", "Performance", "Settings", "WaitForShadersBeforeStarting",
         "Compile Shaders Before Starting",
         "Pre-compile the shader pipeline at boot. Slower start, smoother "
         "first few minutes of gameplay.",
         SettingDef::Bool, "False"}),

        // Performance hacks — big speed-ups, occasional visual regressions
        gfx({"Recommended", "", "Performance Hacks", "Hacks", "EFBToTextureEnable",
         "Skip EFB Copy to RAM",
         "Skip the slow EFB→RAM copy and use a GPU texture instead. "
         "Big speed boost; can break a few games that read EFB on the CPU.",
         SettingDef::Bool, "True"}),

        gfx({"Recommended", "", "Performance Hacks", "Hacks", "XFBToTextureEnable",
         "Skip XFB Copy to RAM",
         "Skip the slow XFB→RAM copy. Big speed boost; required disabled "
         "for games that decode the XFB on the CPU.",
         SettingDef::Bool, "True"}),

        gfx({"Recommended", "", "Performance Hacks", "Hacks", "EFBAccessEnable",
         "Allow EFB CPU Access",
         "Lets games read back the EFB on the CPU. Required for accurate "
         "behavior in some games; disable for the speed boost when possible.",
         SettingDef::Bool, "False"}),

        gfx({"Recommended", "", "Performance Hacks", "Settings", "SafeTextureCacheColorSamples",
         "Texture Cache Accuracy",
         "Fast = best performance with risk of visual glitches; Safe = full "
         "accuracy. Default 128 is a balanced middle ground.",
         SettingDef::Combo, "128",
         { {"Safe (0)","0"}, {"Default (128)","128"}, {"Fast (512)","512"} }}),

        // Visual quality — the most-tweaked image settings
        gfx({"Recommended", "", "Visual Quality", "Settings", "InternalResolution",
         "Internal Resolution",
         "Render scale relative to native. Higher = sharper but slower. "
         "Single biggest knob for visual fidelity.",
         SettingDef::Combo, "1",
         { {"Native (1x)","1"}, {"2x (~720p)","2"}, {"3x (~1080p)","3"},
           {"4x (~1440p)","4"}, {"5x (~1800p)","5"}, {"6x (~4K)","6"} }}),

        gfx({"Recommended", "", "Visual Quality", "Settings", "AspectRatio",
         "Aspect Ratio",
         "Display aspect ratio. Auto matches the game; force 16:9 / 4:3 "
         "for stretching choices.",
         SettingDef::Combo, "0",
         { {"Auto","0"}, {"Force 16:9","1"}, {"Force 4:3","2"},
           {"Stretch","3"}, {"Custom","4"} }}),

        gfx({"Recommended", "", "Visual Quality", "Settings", "wideScreenHack",
         "Widescreen Hack",
         "Force 4:3 games to render in widescreen by altering the projection "
         "matrix. May produce artifacts; useful for 4:3-only titles.",
         SettingDef::Bool, "False"}),

        gfx({"Recommended", "", "Visual Quality", "Settings", "MSAA",
         "Anti-Aliasing (MSAA)",
         "Multi-sample anti-aliasing. Smoother edges, slower.",
         SettingDef::Combo, "1",
         { {"None","1"}, {"2x","2"}, {"4x","4"}, {"8x","8"} }}),

        // MaxAnisotropy + ForceTextureFiltering — paired side-by-side in
        // the Recommended page. layout == "paired" tells GenericSettingsPage
        // to wrap consecutive paired cards in a 2-column horizontal row.
        // Trailing positional fields after `options` go: minVal, maxVal,
        // step, layout, suffix.
        gfx({"Recommended", "", "Visual Quality", "Enhancements", "MaxAnisotropy",
         "Anisotropic Filtering",
         "Sharpens textures viewed at oblique angles.",
         SettingDef::Combo, "-1",
         { {"Default","-1"}, {"Off (1x)","0"}, {"2x","1"}, {"4x","2"}, {"8x","3"}, {"16x","4"} },
         0, 0, 0, "paired"}),

        gfx({"Recommended", "", "Visual Quality", "Enhancements", "ForceTextureFiltering",
         "Force Texture Filtering",
         "Override the game's texture-filtering choice. Default respects "
         "the game; Linear smooths low-res textures.",
         SettingDef::Combo, "0",
         { {"Default","0"}, {"Nearest","1"}, {"Linear","2"} },
         0, 0, 0, "paired"}),

        // Audio — most-toggled audio knobs
        {"Recommended", "", "Audio", "Core", "DSPHLE",
         "DSP HLE (Audio)",
         "High-Level Emulation of the audio DSP — fast and compatible. "
         "Disable to use LLE only when a specific game needs it.",
         SettingDef::Bool, "True"},

        {"Recommended", "", "Audio", "DSP", "Volume",
         "Volume",
         "Master output volume.",
         SettingDef::Int, "100", {}, 0, 100, 1, "slider", "%"},

        // Convenience — common quality-of-life toggles
        {"Recommended", "", "Convenience", "Core", "SkipIPL",
         "Skip GameCube Boot Animation",
         "Skip the GC IPL boot sequence and start the game directly.",
         SettingDef::Bool, "True"},

        {"Recommended", "", "Convenience", "Core", "EnableCheats",
         "Enable Cheats",
         "Process AR/Gecko cheat codes. Off by default for safety.",
         SettingDef::Bool, "False"},

        // ─── Interface settings intentionally NOT exposed ────
        // Dolphin's Interface pane controls *Dolphin's own UI* (window
        // title, language, library covers, etc.). RetroNest takes over
        // the UI entirely — users never see Dolphin's chrome — so these
        // settings would be inert. The two embedding-critical Interface
        // keys (PauseOnFocusLost=True for the overlay pause contract,
        // ConfirmStop=False so our exit flow is uninterrupted) are
        // force-set in patchDolphinIni() below, not exposed in the schema.

        // ─── Audio / DSP Options ─────────────────────────────
        // Mirrors Dolphin's AudioPane.cpp:55-69 — a single "DSP Emulation
        // Engine" combo that internally toggles two INI keys
        // (Core/DSPHLE and DSP/EnableJIT) to express three states:
        //   HLE                      → DSPHLE=True  (EnableJIT ignored)
        //   LLE Recompiler (slow)    → DSPHLE=False, EnableJIT=True
        //   LLE Interpreter (slowest)→ DSPHLE=False, EnableJIT=False
        // saveTransform writes both keys; loadTransform reads both and
        // synthesizes the three-way combo value.
        []() {
            SettingDef d;
            d.category    = "Audio";
            d.subcategory = "";
            d.group       = "DSP Options";
            d.section     = "Core";       // primary (DSPHLE) for the
            d.key         = "DSPHLE";     // dependsOn / focus-bar lookup
            d.label       = "DSP Emulation Engine";
            d.tooltip     = "How the audio DSP is emulated. HLE is fast and "
                            "compatible with most games. LLE is slower but "
                            "more accurate, and required by a handful of "
                            "titles.";
            d.type          = SettingDef::Combo;
            d.defaultValue  = "HLE";
            d.options       = {
                {"HLE",                       "HLE"},
                {"LLE Recompiler (slow)",     "LLE Recompiler"},
                {"LLE Interpreter (very slow)", "LLE Interpreter"},
            };
            d.saveTransform = [](const QString &v,
                                 const SettingDef::SaveCallback &save) {
                if (v == "HLE") {
                    save("Core", "DSPHLE", "True");
                } else if (v == "LLE Recompiler") {
                    save("Core", "DSPHLE", "False");
                    save("DSP",  "EnableJIT", "True");
                } else { // LLE Interpreter
                    save("Core", "DSPHLE", "False");
                    save("DSP",  "EnableJIT", "False");
                }
            };
            d.loadTransform = [](const SettingDef::LoadCallback &read) {
                const bool hle = read("Core", "DSPHLE")
                                     .compare("true", Qt::CaseInsensitive) == 0;
                if (hle) return QString("HLE");
                const QString jit = read("DSP", "EnableJIT");
                // Default-unset EnableJIT means JIT enabled (Dolphin's
                // upstream default), so unspecified → LLE Recompiler.
                const bool useJit = jit.isEmpty() ||
                    jit.compare("true", Qt::CaseInsensitive) == 0;
                return QString(useJit ? "LLE Recompiler" : "LLE Interpreter");
            };
            return d;
        }(),

        // ─── Audio / Backend Settings ────────────────────────
        // Mirrors Dolphin's Audio pane "Backend Settings" group.
        {"Audio", "", "Backend Settings", "DSP", "Backend",
         "Audio Backend",
         "Sound output backend. Cubeb is recommended on macOS.",
         SettingDef::Combo, "Cubeb", audioBackends},

        {"Audio", "", "Backend Settings", "Core", "AudioLatency",
         "Audio Latency",
         "Output latency in milliseconds. Lower = tighter sync, more risk of "
         "audio dropouts under load. 20 ms is a sensible default.",
         SettingDef::Int, "20", {}, 0, 200, 1, "slider", "ms"},

        {"Audio", "", "Backend Settings", "Core", "DPL2Decoder",
         "Dolby Pro Logic II Decoder",
         "Decode the emulated stereo mix into 5.1 surround output. Requires a "
         "backend that supports it — Cubeb on macOS does not, so this is "
         "effectively a no-op there. Active only when DSP HLE is off.",
         SettingDef::Bool, "False"},

        {"Audio", "", "Backend Settings", "Core", "DPL2Quality",
         "DPL2 Decoder Quality",
         "Trade-off between CPU cost and surround-decode accuracy.",
         SettingDef::Combo, "2",
         { {"Lowest",  "0"}, {"Low",   "1"},
           {"High",    "2"}, {"Highest","3"} },
         0, 0, 0, "", "", "DPL2Decoder"},  // gated on DPL2Decoder

        // ─── Audio / Audio Playback Settings ─────────────────
        // Mirrors Dolphin's Audio pane "Audio Playback Settings" group.
        {"Audio", "", "Audio Playback Settings", "DSP", "MuteOnDisabledSpeedLimit",
         "Mute When Disabling Speed Limit",
         "Silence audio while emulation is running unthrottled (e.g. fast-forward). "
         "Avoids unpleasant pitch/playback artifacts.",
         SettingDef::Bool, "False"},

        {"Audio", "", "Audio Playback Settings", "Core", "AudioBufferSize",
         "Audio Buffer Size",
         "Internal mixer buffer in milliseconds. Higher = smoother but more "
         "delay between picture and sound.",
         SettingDef::Int, "80", {}, 16, 512, 1, "slider", "ms"},

        {"Audio", "", "Audio Playback Settings", "Core", "AudioFillGaps",
         "Fill Audio Gaps",
         "Synthesize silence to fill gaps when emulation can't keep up. "
         "Disable for a more accurate native-hardware behavior; enable for smoothness.",
         SettingDef::Bool, "True"},

        {"Audio", "", "Audio Playback Settings", "Core", "AudioPreservePitch",
         "Preserve Audio Pitch",
         "Time-stretch audio to keep pitch constant when emulation runs slower or "
         "faster than 100%. Useful with fast-forward; otherwise leaves pitch unchanged.",
         SettingDef::Bool, "False"},

        // ─── Audio / Volume ──────────────────────────────────
        // Mirrors Dolphin's Audio pane "Volume" group.
        {"Audio", "", "Volume", "DSP", "Volume",
         "Volume",
         "Master output volume (0-100).",
         SettingDef::Int, "100", {}, 0, 100, 1, "slider", "%"},

        // ─── General / Basic Settings ────────────────────────
        // Mirrors Dolphin's General pane "Basic Settings" group.
        {"General", "", "Basic Settings", "Core", "CPUThread",
         "Enable Dual Core (speedhack)",
         "Splits CPU and GPU emulation across two threads. Significant speed "
         "gain; some games rely on tight CPU/GPU sync and may glitch with it on.",
         SettingDef::Bool, "False"},

        // SkipIPL lives in GameCube > IPL Settings to match upstream
        // (Source/Core/DolphinQt/Settings/GameCubePane.cpp:75-80).

        {"General", "", "Basic Settings", "Core", "EnableCheats",
         "Enable Cheats",
         "Enables AR/Gecko cheat code processing. Off by default for safety.",
         SettingDef::Bool, "False"},

        {"General", "", "Basic Settings", "Core", "OverrideRegionSettings",
         "Allow Mismatched Region Settings",
         "Force a region's settings (language, video mode) regardless of disc region.",
         SettingDef::Bool, "False"},

        {"General", "", "Basic Settings", "Core", "AutoDiscChange",
         "Change Discs Automatically",
         "Switch discs automatically for multi-disc games (M3U). RetroNest's "
         "M3U handling is independent and runs at launch time.",
         SettingDef::Bool, "False"},

        {"General", "", "Basic Settings", "Core", "EmulationSpeed",
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

        {"General", "", "Basic Settings", "Core", "LoadGameIntoMemory",
         "Load Whole Game Into Memory",
         "Pre-loads the entire game image into RAM at boot. Eliminates disc "
         "I/O stutter; uses more host memory.",
         SettingDef::Bool, "False"},

        {"General", "", "Basic Settings", "General", "UseDiscordPresence",
         "Show Current Game on Discord",
         "Publishes currently-playing game to Discord status when Dolphin runs. "
         "Requires a Dolphin build with Discord-RPC support compiled in.",
         SettingDef::Bool, "True"},

        // ─── General / Fallback Region ───────────────────────
        // Mirrors Dolphin's General pane "Fallback Region" group.
        {"General", "", "Fallback Region", "Core", "FallbackRegion",
         "Fallback Region",
         "Region used for games whose region cannot be auto-detected. "
         "Affects boot timing and the system menu locale.",
         SettingDef::Combo, "1", regions},

        // ═══ Advanced ═══════════════════════════════════════
        // Mirrors DolphinQt's AdvancedPane (Source/Core/DolphinQt/Settings/
        // AdvancedPane.cpp). Power-user knobs split out from the General
        // tab to match upstream's organization.

        // ─── Advanced / CPU Options ──────────────────────────
        {"Advanced", "", "CPU Options", "Core", "CPUCore",
         "CPU Core",
         "The CPU emulation backend. JIT is required for full-speed gameplay.",
         SettingDef::Combo, "1", cpuCores},

        {"Advanced", "", "CPU Options", "Core", "MMU",
         "Enable MMU",
         "Emulates the memory management unit. Slower but required for a small "
         "set of games (typically Virtual Console / homebrew).",
         SettingDef::Bool, "False"},

        {"Advanced", "", "CPU Options", "Core", "AccurateCPUCache",
         "Enable Write-Back Cache (slow)",
         "Emulate the CPU's L1 instruction cache. Slower but more accurate; "
         "needed for a handful of games that self-modify code.",
         SettingDef::Bool, "False"},

        {"Advanced", "", "CPU Options", "Core", "PauseOnPanic",
         "Pause On Panic",
         "Pause emulation when Dolphin reports a non-fatal error.",
         SettingDef::Bool, "False"},

        // ─── Advanced / Clock Override ───────────────────────
        {"Advanced", "", "Clock Override", "Core", "OverclockEnable",
         "Enable Emulated CPU Clock Override",
         "Allow the slider below to scale the emulated CPU's clock rate. "
         "Some games run smoother with overclocking; others crash.",
         SettingDef::Bool, "False"},

        // Overclock multiplier — slider widget is integer-only (cast via
        // int(minVal)/int(maxVal)), so whole-number bounds. minVal=1
        // keeps the emulated CPU at native or above; below that Dolphin
        // reads the float as 0.0 and stalls. dependsOn gates on enable.
        {"Advanced", "", "Clock Override", "Core", "Overclock",
         "CPU Overclock Multiplier",
         "Multiplier applied to the emulated CPU's clock when overclocking is enabled. "
         "1 = native, 2 = +100%, 4 = +300%.",
         SettingDef::Int, "1", {}, 1, 4, 1, "slider", "x", "OverclockEnable"},

        // ─── Advanced / VBI Frequency Override ───────────────
        {"Advanced", "", "VBI Frequency Override", "Core", "VIOverclockEnable",
         "Enable VBI Frequency Override",
         "Allow scaling the video-interface clock independently of the CPU. "
         "Affects refresh-rate timing for some games.",
         SettingDef::Bool, "False"},

        {"Advanced", "", "VBI Frequency Override", "Core", "VIOverclock",
         "VI Overclock Multiplier",
         "Multiplier applied to the VI clock when VI overclocking is enabled.",
         SettingDef::Int, "1", {}, 1, 4, 1, "slider", "x", "VIOverclockEnable"},

        // ─── Advanced / Memory Override ──────────────────────
        {"Advanced", "", "Memory Override", "Core", "RAMOverrideEnable",
         "Override RAM Sizes",
         "Allow using non-retail RAM sizes (MEM1/MEM2). Required for some "
         "homebrew. Most games will not boot with non-retail sizes.",
         SettingDef::Bool, "False"},

        // ─── Advanced / Timing ───────────────────────────────
        {"Advanced", "", "Timing", "Core", "CorrectTimeDrift",
         "Correct Time Drift",
         "Compensate for accumulated frame-pacing drift over long sessions. "
         "Smooths long-term sync at the cost of micro-stutter.",
         SettingDef::Bool, "False"},

        {"Advanced", "", "Timing", "Core", "RushFramePresentation",
         "Rush Frame Presentation",
         "Aggressively present frames as soon as they're ready. Lower latency, "
         "more tearing without VSync.",
         SettingDef::Bool, "False"},

        {"Advanced", "", "Timing", "Core", "SmoothEarlyPresentation",
         "Smooth Early Presentation",
         "Smooth pacing for frames that finish ahead of schedule.",
         SettingDef::Bool, "False"},

        // ─── Advanced / Custom RTC Options ───────────────────
        {"Advanced", "", "Custom RTC Options", "Core", "EnableCustomRTC",
         "Enable Custom RTC",
         "Override the emulated system clock with a fixed date/time. "
         "Set the value via Dolphin's native UI — RetroNest doesn't yet expose "
         "a date picker.",
         SettingDef::Bool, "False"},

        // ═══ Graphics ═══════════════════════════════════════
        // Five sub-tabs mirror DolphinQt's GraphicsPane structure:
        // General / Enhancements / Hacks / Advanced / On-Screen Display.

        // ─── Graphics / General ──────────────────────────────
        // GFXBackend lives in Dolphin.ini's [Core] section, NOT GFX.ini.
        // Backend names from Source/Core/VideoBackends/*/VideoBackend.h
        // CONFIG_NAME constants. macOS default is Metal.
        {"Graphics", "General", "Backend", "Core", "GFXBackend",
         "Graphics Backend",
         "Renderer used to draw frames. Metal is the macOS default; Vulkan "
         "uses MoltenVK; OGL is OpenGL; Software/Null are reference/silent.",
         SettingDef::Combo, "Metal",
         { {"Metal","Metal"}, {"Vulkan","Vulkan"}, {"OpenGL","OGL"},
           {"Software Renderer","Software Renderer"}, {"Null","Null"} }},

        gfx({"Graphics", "General", "Backend", "Settings", "BackendMultithreading",
         "Backend Multithreading",
         "Distribute video-backend work across multiple threads. "
         "Default-on is the recommended setting upstream.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "General", "Aspect & Window", "Settings", "AspectRatio",
         "Aspect Ratio",
         "Display aspect ratio. Auto matches the game's native aspect; "
         "Stretch fills the screen.",
         SettingDef::Combo, "0",
         { {"Auto","0"}, {"Force 16:9","1"}, {"Force 4:3","2"},
           {"Stretch","3"}, {"Custom","4"} }}),

        gfx({"Graphics", "General", "Aspect & Window", "Settings", "wideScreenHack",
         "Widescreen Hack",
         "Force 4:3 games to render in widescreen by hacking the projection "
         "matrix. Can produce visual artifacts; useful for 4:3-only titles.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "General", "Aspect & Window", "Settings", "InternalResolution",
         "Internal Resolution",
         "Render scale relative to native (1x = original, 6x ≈ 4K).",
         SettingDef::Combo, "1",
         { {"Native (1x)","1"}, {"2x (~720p)","2"}, {"3x (~1080p)","3"},
           {"4x (~1440p)","4"}, {"5x (~1800p)","5"}, {"6x (~4K)","6"} }}),

        gfx({"Graphics", "General", "Aspect & Window", "Hardware", "VSync",
         "V-Sync",
         "Synchronizes output to the display refresh rate. Reduces tearing.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "General", "Aspect & Window", "Settings", "BorderlessFullscreen",
         "Borderless Fullscreen",
         "Use a borderless window instead of exclusive fullscreen. Smoother "
         "task-switching, slight latency cost on some setups.",
         SettingDef::Bool, "False"}),

        // No gfx() wrapper — Fullscreen is in Dolphin.ini's [Display] section.
        {"Graphics", "General", "Aspect & Window", "Display", "Fullscreen",
         "Start in Fullscreen",
         "Render in fullscreen mode. RetroNest already runs Dolphin "
         "embedded in our window, so this is True by default.",
         SettingDef::Bool, "True"},

        gfx({"Graphics", "General", "Shader Compilation", "Settings", "ShaderCompilationMode",
         "Shader Compilation",
         "How shaders are compiled. Asynchronous reduces stutter at the "
         "cost of brief texture/lighting pop-in on first encounter.",
         SettingDef::Combo, "0",
         { {"Specialized (Default)","0"}, {"Exclusive Ubershaders","1"},
           {"Hybrid Ubershaders","2"}, {"Skip Drawing","3"} }}),

        gfx({"Graphics", "General", "Shader Compilation", "Settings", "WaitForShadersBeforeStarting",
         "Compile Shaders Before Starting",
         "Pre-compiles the shader pipeline before launching a game. "
         "Slower start, smoother gameplay.",
         SettingDef::Bool, "False"}),

        // ─── Graphics / Enhancements ─────────────────────────
        gfx({"Graphics", "Enhancements", "Anti-Aliasing", "Settings", "MSAA",
         "Multi-Sample Anti-Aliasing (MSAA)",
         "Higher = smoother edges, slower. Effectiveness depends on backend.",
         SettingDef::Combo, "1",
         { {"None","1"}, {"2x","2"}, {"4x","4"}, {"8x","8"} }}),

        gfx({"Graphics", "Enhancements", "Anti-Aliasing", "Settings", "SSAA",
         "Super-Sample Anti-Aliasing (SSAA)",
         "Renders at MSAA's sample count and downsamples. Highest quality, "
         "highest cost. Only takes effect with MSAA enabled.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Texture Filtering", "Enhancements", "MaxAnisotropy",
         "Anisotropic Filtering",
         "Sharpens textures viewed at oblique angles.",
         SettingDef::Combo, "-1",
         { {"Default","-1"}, {"Off (1x)","0"}, {"2x","1"}, {"4x","2"}, {"8x","3"}, {"16x","4"} }}),

        gfx({"Graphics", "Enhancements", "Texture Filtering", "Enhancements", "ForceTextureFiltering",
         "Force Texture Filtering",
         "Override the game's texture-filtering choice. Default respects the "
         "game; Nearest = pixelated, Linear = smooth.",
         SettingDef::Combo, "0",
         { {"Default","0"}, {"Nearest","1"}, {"Linear","2"} }}),

        gfx({"Graphics", "Enhancements", "Texture Filtering", "Enhancements", "OutputResampling",
         "Output Resampling",
         "Algorithm used to resample the rendered image to the window size.",
         SettingDef::Combo, "0",
         { {"Default","0"}, {"Bilinear","1"}, {"B-Spline","2"},
           {"Mitchell-Netravali","3"}, {"Catmull-Rom","4"} }}),

        gfx({"Graphics", "Enhancements", "Color & Lighting", "Enhancements", "ForceTrueColor",
         "Force 24-Bit Color",
         "Force higher-precision color output. Reduces banding on gradients.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Enhancements", "Color & Lighting", "Settings", "EnablePixelLighting",
         "Per-Pixel Lighting",
         "Higher-quality lighting. Slight performance cost; some games "
         "look noticeably better with it on.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Color & Lighting", "Enhancements", "DisableCopyFilter",
         "Disable Copy Filter",
         "Disable the post-process copy-filter pass. Reduces blur some games "
         "apply but may break intended visual effects.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Color & Lighting", "Settings", "DisableFog",
         "Disable Fog",
         "Skip rendering fog effects. Improves visibility in foggy games at "
         "the cost of intended atmosphere.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Color & Lighting", "Enhancements", "ArbitraryMipmapDetection",
         "Detect Arbitrary Mipmaps",
         "Detect when a game uses mipmaps as separate images rather than "
         "true LODs, and treat them appropriately.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Enhancements", "Color & Lighting", "Enhancements", "HDROutput",
         "HDR Post-Processing",
         "Output in HDR (high-dynamic-range) when the display supports it.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Stereoscopy", "Stereoscopy", "StereoMode",
         "Stereo Mode",
         "3D-stereoscopic rendering mode. Off disables stereo entirely.",
         SettingDef::Combo, "0",
         { {"Off","0"}, {"Side-by-Side","1"}, {"Top-and-Bottom","2"},
           {"Anaglyph","3"}, {"Quad Buffer","4"} }}),

        gfx({"Graphics", "Enhancements", "Stereoscopy", "Stereoscopy", "StereoSwapEyes",
         "Swap Eyes",
         "Swap the left and right eye images.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Stereoscopy", "Stereoscopy", "StereoPerEyeResolutionFull",
         "Use Full Resolution Per Eye",
         "Render each eye at the full internal resolution instead of half. "
         "Doubles GPU cost.",
         SettingDef::Bool, "False"}),

        // ─── Graphics / Hacks ────────────────────────────────
        gfx({"Graphics", "Hacks", "EFB", "Hacks", "EFBAccessEnable",
         "Allow EFB CPU Access",
         "Lets games read back the EFB on the CPU. Required for accurate "
         "behavior in some games; slower when enabled.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "EFB", "Hacks", "EFBAccessDeferInvalidation",
         "Defer EFB Cache Invalidation",
         "Reduces overhead by deferring EFB-cache invalidations. Speed win "
         "for many games; can cause visual glitches in a few.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "EFB", "Hacks", "EFBEmulateFormatChanges",
         "Emulate EFB Format Changes",
         "Accurately emulates EFB pixel-format changes. Slower but more correct.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "EFB", "Hacks", "EFBToTextureEnable",
         "Skip EFB Copy to RAM",
         "Skip the slow EFB→RAM copy and use a GPU texture instead. "
         "Big speed boost; can break a few games that read the EFB.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "EFB", "Hacks", "DeferEFBCopies",
         "Defer EFB Copies",
         "Delay flushing EFB copies until needed. Speed win for most games.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "EFB", "Hacks", "EFBScaledCopy",
         "Scaled EFB Copy",
         "Resize EFB copies to match the rendering scale. Required for "
         "high internal resolutions to look right.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "XFB", "Hacks", "XFBToTextureEnable",
         "Skip XFB Copy to RAM",
         "Skip the slow XFB→RAM copy. Big speed boost; required disabled "
         "for games that decode the XFB on the CPU.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "XFB", "Hacks", "ImmediateXFBEnable",
         "Immediately Present XFB",
         "Display the XFB as soon as it's drawn instead of on next vblank. "
         "Lower latency, slight tearing risk.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "XFB", "Hacks", "SkipDuplicateXFBs",
         "Skip Duplicate XFBs",
         "Detect and skip identical consecutive XFBs to save GPU work.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "XFB", "Hacks", "DisableCopyToVRAM",
         "Disable Copy To VRAM",
         "Disable the GPU-side path for EFB/XFB copies. Forces a slower CPU "
         "path; debug-only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "Texture Cache", "Settings", "SafeTextureCacheColorSamples",
         "Texture Cache Accuracy",
         "How many color samples Dolphin checks to decide if a cached "
         "texture is still valid. Lower = faster but may miss updates.",
         SettingDef::Combo, "128",
         { {"Safe (0)","0"}, {"Default (128)","128"}, {"Fast (512)","512"} }}),

        gfx({"Graphics", "Hacks", "Texture Cache", "Hacks", "FastTextureSampling",
         "Manual Texture Sampling",
         "Trade some accuracy for speed in the texture sampler.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "Texture Cache", "Settings", "SaveTextureCacheToState",
         "Save Texture Cache to State",
         "Save the texture cache contents in save states. Larger states, "
         "smoother resume.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "Other", "Hacks", "BBoxEnable",
         "Bounding Box Emulation",
         "Emulate the GameCube/Wii bounding-box hardware. Slower but required "
         "for a small set of games (e.g. Paper Mario).",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "Other", "Hacks", "VertexRounding",
         "Vertex Rounding",
         "Round vertex coordinates to integers. Fixes seams in some games.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "Other", "Hacks", "VISkip",
         "VI Skip",
         "Skip Video-Interface processing on duplicate frames.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "Other", "Settings", "FastDepthCalc",
         "Fast Depth Calculation",
         "Use a faster GPU-friendly depth-calculation path. Default-on.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "Other", "Settings", "CPUCull",
         "Cull Vertices on the CPU",
         "Cull invisible geometry on the CPU before sending to the GPU. "
         "Speeds up some games.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "Other", "Settings", "EnableGPUTextureDecoding",
         "GPU Texture Decoding",
         "Decode textures on the GPU instead of the CPU. Speed win on most setups.",
         SettingDef::Bool, "False"}),

        // ─── Graphics / Advanced ─────────────────────────────
        gfx({"Graphics", "Advanced", "Custom Textures", "Settings", "HiresTextures",
         "Load Custom Textures",
         "Load high-resolution texture replacements from "
         "User/Load/Textures/<game-id>/.",
         SettingDef::Bool, "False"}),

        // Prefetch is meaningless without custom textures loaded — gate
        // on HiresTextures (matches AdvancedWidget.cpp).
        gfx({"Graphics", "Advanced", "Custom Textures", "Settings", "CacheHiresTextures",
         "Prefetch Custom Textures",
         "Pre-load all custom textures into VRAM at boot. Eliminates load "
         "stutter; uses more memory.",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "", "HiresTextures"}),

        gfx({"Graphics", "Advanced", "Custom Textures", "Settings", "DumpTextures",
         "Dump Textures",
         "Save every texture the game uses to disk. Used to create custom "
         "texture packs.",
         SettingDef::Bool, "False"}),

        // Base/mip dump options only matter when texture dumping is on
        // — gate on DumpTextures (matches AdvancedWidget.cpp).
        gfx({"Graphics", "Advanced", "Custom Textures", "Settings", "DumpBaseTextures",
         "Dump Base Textures",
         "Include base mipmaps in the texture dump.",
         SettingDef::Bool, "True", {}, 0, 0, 0, "", "", "DumpTextures"}),

        gfx({"Graphics", "Advanced", "Custom Textures", "Settings", "DumpMipTextures",
         "Dump Mip Maps",
         "Include all mipmap levels in the texture dump.",
         SettingDef::Bool, "True", {}, 0, 0, 0, "", "", "DumpTextures"}),

        gfx({"Graphics", "Advanced", "Frame Dumping", "Settings", "DumpEFBTarget",
         "Dump EFB Target",
         "Dump the EFB contents to disk each frame. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Frame Dumping", "Settings", "DumpXFBTarget",
         "Dump XFB Target",
         "Dump the XFB contents to disk each frame. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Frame Dumping", "Settings", "BitrateKbps",
         "Frame Dump Bitrate",
         "Encoded video bitrate (kilobits per second) when dumping frames "
         "to a video file.",
         SettingDef::Int, "25000", {}, 1000, 100000, 1000, "slider", "kbps"}),

        gfx({"Graphics", "Advanced", "Frame Dumping", "Settings", "FrameDumpsResolutionType",
         "Frame Dump Resolution",
         "Source for the dumped frame's resolution.",
         SettingDef::Combo, "0",
         { {"Window","0"}, {"XFB Aspect-Corrected","1"}, {"Raw XFB","2"} }}),

        gfx({"Graphics", "Advanced", "Frame Dumping", "Settings", "PNGCompressionLevel",
         "PNG Compression Level",
         "Compression level for PNG screenshots and frame dumps. 0 = none, "
         "9 = maximum.",
         SettingDef::Int, "6", {}, 0, 9, 1, "slider", ""}),

        gfx({"Graphics", "Advanced", "Frame Dumping", "Settings", "UseLossless",
         "Use Lossless Codec (Ut Video)",
         "Encode frame dumps with a lossless codec. Larger files, no quality loss.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debug", "Settings", "WireFrame",
         "Enable Wireframe",
         "Render geometry as wireframe lines. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debug", "Settings", "OverlayStats",
         "Show Statistics Overlay",
         "Render a per-frame statistics overlay. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debug", "Settings", "OverlayProjStats",
         "Show Projection Statistics",
         "Render a projection-matrix statistics overlay. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debug", "Settings", "TexFmtOverlayEnable",
         "Texture Format Overlay",
         "Tag each texture with its format. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debug", "Settings", "EnableValidationLayer",
         "Enable Validation Layer",
         "Enable the graphics-API validation layer (Vulkan/D3D). Massive "
         "slowdown; debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debug", "Settings", "LogRenderTimeToFile",
         "Log Render Time To File",
         "Append per-frame GPU timing data to a log file. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debug", "Settings", "PerfSampWindowMS",
         "Performance Sample Window",
         "Sliding window (in milliseconds) for perf-stat averaging.",
         SettingDef::Int, "1000", {}, 100, 10000, 100, "slider", "ms"}),

        gfx({"Graphics", "Advanced", "Misc", "Settings", "PreferVSForLinePointExpansion",
         "Prefer VS for Point/Line Expansion",
         "Expand line/point primitives in the vertex shader instead of "
         "geometry. Workaround for some drivers.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Misc", "Settings", "EnableMods",
         "Enable Graphics Mods",
         "Load graphics mods from User/Load/GraphicMods/<game-id>/.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Misc", "Settings", "Crop",
         "Crop",
         "Crop overscan/black borders from the rendered image.",
         SettingDef::Bool, "False"}),

        // ─── Graphics / On-Screen Display ────────────────────
        // Group names mirror Dolphin's OnScreenDisplayPane (Source/Core/
        // DolphinQt/Settings/OnScreenDisplayPane.cpp): General,
        // Performance Statistics, Movie Window, Netplay.

        // OSD messages + font size live in Dolphin.ini, not GFX.ini.
        {"Graphics", "On-Screen Display", "General", "Interface",
         "OnScreenDisplayMessages",
         "Show Messages",
         "Display Dolphin's own status messages (savestates, achievements, etc.)",
         SettingDef::Bool, "True"},

        // Font size only matters when on-screen messages are on.
        {"Graphics", "On-Screen Display", "General", "Settings", "OSDFontSize",
         "OSD Font Size",
         "Point size for on-screen messages.",
         SettingDef::Int, "13", {}, 8, 32, 1, "slider", "pt",
         "OnScreenDisplayMessages"},

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowFPS",
         "Show FPS",
         "Frames per second the GPU is drawing.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowVPS",
         "Show VPS",
         "VBlanks per second — the rate the emulated game thinks it's running at.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowSpeed",
         "Show % Speed",
         "Emulation speed as a percentage of native.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowSpeedColors",
         "Show Speed Colors",
         "Tint the Show Speed indicator green/yellow/red based on how close "
         "to native it is.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowFTimes",
         "Show Frame Times",
         "Per-frame GPU time graph.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowVTimes",
         "Show VBlank Times",
         "Per-vblank time graph.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowGraphs",
         "Show Performance Graphs",
         "Render the FPS/VPS history as a graph instead of a single number.",
         SettingDef::Bool, "False"}),

        // Frame count + lag are part of the upstream "Movie Window" group
        // (debugging / TAS-style overlays). Keys live in Dolphin.ini
        // [General].
        {"Graphics", "On-Screen Display", "Movie Window", "General",
         "ShowFrameCount",
         "Show Frame Counter",
         "Display the running frame number.",
         SettingDef::Bool, "False"},

        {"Graphics", "On-Screen Display", "Movie Window", "General", "ShowLag",
         "Show Lag Counter",
         "Display the per-frame input-lag counter (movie/replay tooling).",
         SettingDef::Bool, "False"},

        gfx({"Graphics", "On-Screen Display", "Netplay", "Settings",
         "ShowNetPlayPing",
         "Show NetPlay Ping",
         "Display NetPlay session ping.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Netplay", "Settings",
         "ShowNetPlayMessages",
         "Show NetPlay Chat",
         "Display NetPlay chat/event messages on screen.",
         SettingDef::Bool, "False"}),

        // ═══ GameCube ═══════════════════════════════════════
        // Mirrors DolphinQt GameCubePane (Source/Core/DolphinQt/Settings/
        // GameCubePane.cpp). Two groups: IPL Settings + Device Settings.
        // Keys live in Dolphin.ini's [Core] section.

        // ─── GameCube / IPL Settings ─────────────────────────
        {"GameCube", "", "IPL Settings", "Core", "SkipIPL",
         "Skip Main Menu",
         "Skips the GameCube IPL boot sequence and starts the game directly. "
         "When disabled, requires IPL.bin in the BIOS folder.",
         SettingDef::Bool, "True"},

        {"GameCube", "", "IPL Settings", "Core", "SelectedLanguage",
         "System Language",
         "System language used by GameCube games that respect it. "
         "Indices 0..5 mirror upstream's combo (English/German/French/"
         "Spanish/Italian/Dutch).",
         SettingDef::Combo, "0",
         { {"English","0"}, {"German","1"}, {"French","2"},
           {"Spanish","3"}, {"Italian","4"}, {"Dutch","5"} }},

        // ─── GameCube / Device Settings ──────────────────────
        // Slot A, Slot B, and SP1 (Serial Port 1) all live in one
        // "Device Settings" group upstream (GameCubePane.cpp:94-200).
        //
        // EXIDeviceType enum — Source/Core/Core/HW/EXI/EXI_Device.h:25-45.
        // Default for Slot A is MemoryCardFolder (8); Slot B None (0xFF=255).
        // Subset of the enum exposed (the full set has debug/dev devices
        // and ethernet variants — included here for the sake of parity
        // with native UI).
        {"GameCube", "", "Device Settings", "Core", "SlotA",
         "Slot A Device",
         "Device plugged into the GameCube's left memory-card / EXI slot. "
         "MemoryCardFolder = per-game folders under the GC data dir; "
         "MemoryCard = single .raw image; AGP = Game Boy Player.",
         SettingDef::Combo, "8",
         { {"None","255"}, {"Memory Card","1"}, {"Memory Card Folder","8"},
           {"AGP (Game Boy Player)","9"}, {"Microphone","4"},
           {"Gecko Debugger","7"}, {"AD16","3"} }},

        {"GameCube", "", "Device Settings", "Core", "SlotB",
         "Slot B Device",
         "Device plugged into the GameCube's right memory-card / EXI slot. "
         "Microphone is most common here for games like Mario Party.",
         SettingDef::Combo, "255",
         { {"None","255"}, {"Memory Card","1"}, {"Memory Card Folder","8"},
           {"AGP (Game Boy Player)","9"}, {"Microphone","4"},
           {"Gecko Debugger","7"}, {"AD16","3"} }},

        {"GameCube", "", "Device Settings", "Core", "SerialPort1",
         "SP1 Device",
         "Device plugged into the GameCube's serial port — typically used "
         "for network adapters in compatible games.",
         SettingDef::Combo, "255",
         { {"None","255"}, {"Broadband Adapter (TAP)","11"},
           {"Broadband Adapter (Built-In)","12"},
           {"Broadband Adapter (XLink Kai)","10"},
           {"Modem Adapter (TAP)","13"} }},

        // ═══ Wii ════════════════════════════════════════════
        // Mirrors DolphinQt WiiPane (Source/Core/DolphinQt/Settings/
        // WiiPane.cpp) — but limited to the MAIN_* keys that live in
        // Dolphin.ini's [Core] section.
        //
        // SKIPPED: SYSCONF_* keys (Language, Widescreen, PAL60,
        // SoundMode, SensorBarPosition/Sensitivity, SpeakerVolume,
        // WiimoteMotor, Screensaver) — those persist in the Wii's
        // emulated SYSCONF binary file rather than an INI, and our
        // schema-driven page only routes to text INI files. The user
        // reaches these via the "Open Native Settings" button.
        {"Wii", "", "SD Card", "Core", "WiiSDCard",
         "Insert SD Card",
         "Make a virtual SD card visible to Wii software. Required for "
         "save imports, channel installs, and homebrew that uses the slot.",
         SettingDef::Bool, "True"},

        {"Wii", "", "SD Card", "Core", "WiiSDCardAllowWrites",
         "Allow Writes to SD Card",
         "When off, the SD card is read-only — useful for protecting a "
         "shared image from accidental modification.",
         SettingDef::Bool, "True"},

        {"Wii", "", "SD Card", "Core", "WiiSDCardEnableFolderSync",
         "Enable SD Card Folder Sync",
         "Mirror the SD card image from a host folder. Lets you drop "
         "files in/out of the SD without booting the Wii system menu.",
         SettingDef::Bool, "False"},

        {"Wii", "", "SD Card", "Core", "WiiSDCardFilesize",
         "SD Card Capacity (MiB)",
         "Capacity of the virtual SD card. 0 = use the image file as-is "
         "(don't resize). Slider range matches DolphinQt's combo.",
         SettingDef::Int, "0", {}, 0, 8192, 128, "slider", "MiB"},

        {"Wii", "", "Input & Misc", "Core", "WiiKeyboard",
         "Connect USB Keyboard",
         "Make a USB keyboard visible to Wii software (e.g. for chat-aware "
         "homebrew).",
         SettingDef::Bool, "False"},

        {"Wii", "", "Online", "Core", "EnableWiiLink",
         "Enable WiiConnect24 via WiiLink",
         "Patch the Wii Shop / Wii Channels to use community-run "
         "replacement servers (WiiLink). Off by default to avoid surprising "
         "users with third-party network calls.",
         SettingDef::Bool, "False"},
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
    // Aspect-ratio preview lives on the Recommended category (single-
    // subcategory page → subcategory == ""). Recommended is the primary
    // entry point and already has the AspectRatio combo near the top, so
    // the live preview is most useful there. Graphics/General has the
    // same combo without the preview — it's now a plain settings list,
    // matching the rest of the Graphics sub-tabs.
    //
    // Dolphin only exposes aspect ratio in a way that maps to the shared
    // AspectRatioPreview. Stretch / crop / integer-scaling are not GFX.ini
    // keys in Dolphin, so the preview shows just the aspect rectangle and
    // leaves the other Q_PROPERTYs at their feature-absent defaults.
    if (category == "Recommended" && subcategory.isEmpty()) {
        return {"aspect", {
            {"AspectRatio", "aspectMode"},
        }};
    }
    // Graphics / On-Screen Display: same preview layout as Recommended's
    // Visual Quality. Only the show-toggles whose semantics map to
    // OsdPreview's Q_PROPERTYs are listed — Dolphin's other OSD keys
    // (NetPlay ping, OSD font size, frame count, etc.) appear as
    // settings cards but don't drive the preview.
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        return {"osd", {
            {"ShowFPS",     "showFps"},
            {"ShowVPS",     "showVps"},
            {"ShowSpeed",   "showSpeed"},
            {"ShowFTimes",  "showFrameTimes"},
        }};
    }
    return {};
}
