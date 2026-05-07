#include "dolphin_adapter.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVariantList>

#include "core/binding_def.h"
#include "core/controller_type_def.h"
#include "core/github_client.h"
#include "core/ini_file.h"
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

    // Helper: mark an inverted Bool. Mirrors upstream Dolphin's
    // `ConfigBool(label, key, layer, inverted=true)` — checkbox visual
    // state is the inverse of the stored INI value (e.g. label "Disable
    // Bounding Box" stored over BBoxEnable=False/True).
    auto inv = [](SettingDef d) { d.inverted = true; return d; };

    // ConfigSliderU32 multiplier from AdvancedPane.cpp:250,266 — the
    // slider position (in MB) is multiplied to byte count on write and
    // divided on read. Named so the two Memory-Override sliders below
    // don't carry a magic literal.
    constexpr qint64 kBytesPerMiB = 0x100000;

    // Helper: build a Memory-Override slider (MEM1/MEM2). Both sliders
    // display in MB but Dolphin stores the byte count, so they share a
    // uniform save/loadTransform pair. Captured by-value so each
    // SettingDef owns its own copy of the key string and default text.
    auto memSlider = [](const char* iniKey, const char* label,
                        const char* tooltip, int defaultMB,
                        int minMB, int maxMB, const char* suffix) {
        SettingDef d;
        d.category    = "Advanced";
        d.group       = "Memory Override";
        d.section     = "Core";
        d.key         = iniKey;
        d.label       = label;
        d.tooltip     = tooltip;
        d.type        = SettingDef::Int;
        d.defaultValue = QString::number(defaultMB);
        d.minVal      = minMB;
        d.maxVal      = maxMB;
        d.step        = 1;
        d.layout      = "slider";
        d.suffix      = suffix;
        d.dependsOn   = "RAMOverrideEnable";
        const QString section = d.section;
        const QString key     = d.key;
        const QString defStr  = d.defaultValue;
        d.saveTransform = [section, key](const QString &v,
                                          const SettingDef::SaveCallback &save) {
            const qint64 bytes = qint64(v.toInt()) * kBytesPerMiB;
            save(section, key, QString::number(bytes));
        };
        d.loadTransform = [section, key, defStr](const SettingDef::LoadCallback &read) {
            const QString cur = read(section, key);
            if (cur.isEmpty()) return defStr;
            return QString::number(cur.toLongLong() / kBytesPerMiB);
        };
        return d;
    };

    const QVector<QPair<QString,QString>> audioBackends = {
        {"Cubeb",   "Cubeb"},
        {"OpenAL",  "OpenAL"},
        {"Null",    "Null"},
    };

    // CPU cores — labels mirror CPU_CORE_NAMES in AdvancedPane.cpp:43-48.
    // The numeric values match Dolphin's CPUCore enum (PowerPC.h).
    const QVector<QPair<QString,QString>> cpuCores = {
        {"Interpreter (slowest)",                       "0"},
        {"Cached Interpreter (slower)",                 "5"},
        {"JIT Recompiler for x86-64 (recommended)",     "1"},
        {"JIT Recompiler for ARM64 (recommended)",      "4"},
    };

    // Cursor visibility — written as the int underlying ShowCursor enum
    // (MainSettings.h:255-261). DolphinQt renders this as 3 radio buttons.
    const QVector<QPair<QString,QString>> cursorModes = {
        {"Never",        "0"},
        {"Always",       "1"},
        {"On Movement",  "2"},
    };

    // Region fallback — labels + order mirror GeneralPane.cpp:231
    // (`{tr("NTSC-J"), tr("NTSC-U"), tr("PAL"), tr("NTSC-K")}`). Numeric
    // values match DiscIO::Region (Source/Core/DiscIO/Enums.h). The
    // "Unknown" enum value (=3) exists in the enum but isn't exposed in
    // the upstream combo, so we omit it here too.
    const QVector<QPair<QString,QString>> regions = {
        {"NTSC-J",  "0"},
        {"NTSC-U",  "1"},
        {"PAL",     "2"},
        {"NTSC-K",  "4"},
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
                {"HLE (recommended)",           "HLE"},
                {"LLE Recompiler (slow)",       "LLE Recompiler"},
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

        // Upstream gates this on AudioCommon::SupportsLatencyControl(backend),
        // which returns true only for OpenAL and WASAPI (AudioCommon.cpp:151).
        // We expose Cubeb / OpenAL / Null on macOS, so the slider is active
        // only with OpenAL. Cubeb (the default macOS backend) hides/greys
        // it upstream.
        {"Audio", "", "Backend Settings", "Core", "AudioLatency",
         "Latency",
         "Output latency in milliseconds. Lower = tighter sync, more risk of "
         "audio dropouts under load. Only available with backends that "
         "support latency control (OpenAL on macOS).",
         SettingDef::Int, "20", {}, 0, 200, 1, "slider", "ms",
         "Backend=OpenAL"},

        // Upstream gates the DPL2 controls on (backend supports DPL2)
        // AND (DSP is in LLE mode) — see AudioPane::OnDspChanged in
        // references/dolphin-master/Source/Core/DolphinQt/Settings/
        // AudioPane.cpp:233-241. On macOS only OpenAL of the three
        // backends we expose supports DPL2 (Cubeb / Null don't). The DSP
        // half compares against MAIN_DSP_HLE; our DSPHLE combo (whose
        // saveTransform writes both DSPHLE and EnableJIT) carries the
        // synthesized values "HLE" / "LLE Recompiler" / "LLE Interpreter",
        // so we express "DSP != HLE" with `DSPHLE!=HLE`.
        {"Audio", "", "Backend Settings", "Core", "DPL2Decoder",
         "Dolby Pro Logic II Decoder",
         "Decode the emulated stereo mix into 5.1 surround output. Requires "
         "an audio backend that supports DPL2 (only OpenAL among the "
         "available backends) AND DSP set to LLE mode. Otherwise inert.",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "",
         "Backend=OpenAL && DSPHLE!=HLE"},

        // Upstream additionally gates DPL2Quality on DPL2Decoder being
        // checked (AudioPane.cpp:240 — `enabled && m_dolby_pro_logic->
        // isChecked()`). Express the full chain so the combo greys
        // whenever any link breaks, mirroring upstream's behavior.
        {"Audio", "", "Backend Settings", "Core", "DPL2Quality",
         "Decoding Quality",
         "Trade-off between CPU cost and surround-decode accuracy.",
         SettingDef::Combo, "2",
         { {"Lowest (Latency ~10 ms)",  "0"},
           {"Low (Latency ~20 ms)",     "1"},
           {"High (Latency ~40 ms)",    "2"},
           {"Highest (Latency ~80 ms)", "3"} },
         0, 0, 0, "", "",
         "DPL2Decoder && Backend=OpenAL && DSPHLE!=HLE"},

        // ─── Audio / Audio Playback Settings ─────────────────
        // Order mirrors AudioPane.cpp:197-200 — buffer size, fill gaps,
        // preserve pitch, then mute-on-disabled-speed-limit at the bottom.
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

        {"Audio", "", "Audio Playback Settings", "DSP", "MuteOnDisabledSpeedLimit",
         "Mute When Disabling Speed Limit",
         "Silence audio while emulation is running unthrottled (e.g. fast-forward). "
         "Avoids unpleasant pitch/playback artifacts.",
         SettingDef::Bool, "False"},

        // ─── Audio / Volume ──────────────────────────────────
        // Mirrors Dolphin's Audio pane "Volume" group.
        {"Audio", "", "Volume", "DSP", "Volume",
         "Volume",
         "Master output volume (0-100).",
         SettingDef::Int, "100", {}, 0, 100, 1, "slider", "%"},

        // ─── General / Basic Settings ────────────────────────
        // Order mirrors GeneralPane::CreateBasic (GeneralPane.cpp:136-191):
        // dualcore, cheats, load-whole-game, override-region, auto-disc,
        // discord, then Speed Limit combo at the bottom.
        // SkipIPL lives in GameCube > IPL Settings to match upstream
        // (Source/Core/DolphinQt/Settings/GameCubePane.cpp:75-80).
        // Auto Update + Usage Statistics groups intentionally omitted —
        // RetroNest manages emulator binaries itself, and we don't surface
        // analytics opt-in through the unified shell.
        {"General", "", "Basic Settings", "Core", "CPUThread",
         "Enable Dual Core (speedhack)",
         "Splits CPU and GPU emulation across two threads. Significant speed "
         "gain; some games rely on tight CPU/GPU sync and may glitch with it on.",
         SettingDef::Bool, "False"},

        {"General", "", "Basic Settings", "Core", "EnableCheats",
         "Enable Cheats",
         "Enables AR/Gecko cheat code processing. Off by default for safety.",
         SettingDef::Bool, "False"},

        {"General", "", "Basic Settings", "Core", "LoadGameIntoMemory",
         "Load Whole Game Into Memory",
         "Pre-loads the entire game image into RAM at boot. Eliminates disc "
         "I/O stutter; uses more host memory.",
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

        {"General", "", "Basic Settings", "General", "UseDiscordPresence",
         "Show Current Game on Discord",
         "Publishes currently-playing game to Discord status when Dolphin runs. "
         "Requires a Dolphin build with Discord-RPC support compiled in.",
         SettingDef::Bool, "True"},

        // Upstream's Speed Limit combo: Unlimited + 10..200% in 10%
        // steps, with 100% suffixed "(Normal Speed)" — see
        // GeneralPane.cpp:178-188. Stored as float multiplier (index *
        // 0.1f).
        {"General", "", "Basic Settings", "Core", "EmulationSpeed",
         "Speed Limit",
         "Cap on emulated speed relative to native. Unlimited = no throttle. "
         "Stored as a float multiplier; combo offers common breakpoints.",
         SettingDef::Combo, "1.000000",
         { {"Unlimited",            "0.000000"},
           {"10%",                  "0.100000"},
           {"20%",                  "0.200000"},
           {"30%",                  "0.300000"},
           {"40%",                  "0.400000"},
           {"50%",                  "0.500000"},
           {"60%",                  "0.600000"},
           {"70%",                  "0.700000"},
           {"80%",                  "0.800000"},
           {"90%",                  "0.900000"},
           {"100% (Normal Speed)",  "1.000000"},
           {"110%",                 "1.100000"},
           {"120%",                 "1.200000"},
           {"130%",                 "1.300000"},
           {"140%",                 "1.400000"},
           {"150%",                 "1.500000"},
           {"160%",                 "1.600000"},
           {"170%",                 "1.700000"},
           {"180%",                 "1.800000"},
           {"190%",                 "1.900000"},
           {"200%",                 "2.000000"} }},

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
        // Order mirrors AdvancedPane::CreateLayout (AdvancedPane.cpp:65-101):
        // CPU Emulation Engine, Enable MMU, Pause on Panic, Enable Write-
        // Back Cache.
        {"Advanced", "", "CPU Options", "Core", "CPUCore",
         "CPU Emulation Engine",
         "The CPU emulation backend. JIT is required for full-speed gameplay.",
         SettingDef::Combo, "1", cpuCores},

        {"Advanced", "", "CPU Options", "Core", "MMU",
         "Enable MMU",
         "Emulates the memory management unit. Slower but required for a small "
         "set of games (typically Virtual Console / homebrew).",
         SettingDef::Bool, "False"},

        {"Advanced", "", "CPU Options", "Core", "PauseOnPanic",
         "Pause on Panic",
         "Pause emulation when Dolphin reports a non-fatal error.",
         SettingDef::Bool, "False"},

        {"Advanced", "", "CPU Options", "Core", "AccurateCPUCache",
         "Enable Write-Back Cache (slow)",
         "Emulate the CPU's L1 instruction cache. Slower but more accurate; "
         "needed for a handful of games that self-modify code.",
         SettingDef::Bool, "False"},

        // ─── Advanced / Timing ───────────────────────────────
        // Mirrors AdvancedPane.cpp:103-139 — Timing comes BEFORE Clock
        // Override / VBI / Memory / RTC blocks.
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

        // ─── Advanced / Clock Override ───────────────────────
        {"Advanced", "", "Clock Override", "Core", "OverclockEnable",
         "Enable Emulated CPU Clock Override",
         "Allow the slider below to scale the emulated CPU's clock rate. "
         "Some games run smoother with overclocking; others crash.",
         SettingDef::Bool, "False"},

        // Overclock multiplier — slider widget is integer-only (cast via
        // int(minVal)/int(maxVal)), so whole-number bounds. minVal=1
        // keeps the emulated CPU at native or above; below that Dolphin
        // reads the float as 0.0 and stalls. Upstream uses a float slider
        // 0.01-5.00 in 0.01 steps; our coarser int-multiplier mapping
        // covers the common 100/200/300/400% steps until we add a
        // dedicated float-slider widget. dependsOn gates on enable.
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
        // Mirrors AdvancedPane::CreateLayout. MEM1/MEM2 sliders display
        // in MB; Dolphin stores the byte count via the kBytesPerMiB
        // multiplier handled by memSlider's save/loadTransform.
        {"Advanced", "", "Memory Override", "Core", "RAMOverrideEnable",
         "Enable Emulated Memory Size Override",
         "Allow using non-retail RAM sizes (MEM1/MEM2). Required for some "
         "homebrew. Most games will not boot with non-retail sizes.",
         SettingDef::Bool, "False"},

        memSlider("MEM1Size", "MEM1",
                  "Size of the emulated console's main RAM. Default 24 MB.",
                  /*defaultMB=*/24, /*min=*/24, /*max=*/64, " MB (MEM1)"),

        memSlider("MEM2Size", "MEM2",
                  "Size of the emulated Wii's external RAM. Default 64 MB.",
                  /*defaultMB=*/64, /*min=*/64, /*max=*/128, " MB (MEM2)"),

        // ─── Advanced / Custom RTC Options ───────────────────
        // Upstream also exposes a `QDateTimeEdit` gated on
        // EnableCustomRTC. We don't have a datetime widget type yet —
        // users set the underlying CustomRTCValue via Dolphin's native
        // UI when they need a fixed clock.
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
        // Mirrors GeneralWidget (Source/Core/DolphinQt/Config/Graphics/
        // GeneralWidget.cpp). Three groups: Basic, Other, Shader Compilation.
        //
        // Skipped (intentionally):
        //  - Adapter combo: empty on macOS (g_backend_info.Adapters).
        //  - Custom Aspect Ratio width/height: shown only when AspectRatio
        //    is Custom or Custom (Stretch); needs dynamic visibility infra.
        //  - Borderless Fullscreen: upstream gates this Windows-only
        //    (AdvancedWidget.cpp:177-182) — skipped on macOS for parity.

        // GFXBackend lives in Dolphin.ini's [Core] section, not GFX.ini.
        {"Graphics", "General", "Basic", "Core", "GFXBackend",
         "Backend",
         "Renderer used to draw frames. Metal is the macOS default; Vulkan "
         "uses MoltenVK; OGL is OpenGL; Software/Null are reference/silent.",
         SettingDef::Combo, "Metal",
         { {"Metal","Metal"}, {"Vulkan","Vulkan"}, {"OpenGL","OGL"},
           {"Software Renderer","Software Renderer"}, {"Null","Null"} }},

        // AspectMode enum (Source/Core/VideoCommon/VideoConfig.h):
        // Auto=0, ForceWide=1, ForceStandard=2, Stretch=3, Custom=4,
        // CustomStretch=5. Order + labels mirror GeneralWidget.cpp:65-67.
        gfx({"Graphics", "General", "Basic", "Settings", "AspectRatio",
         "Aspect Ratio",
         "Display aspect ratio. Auto matches the game's native aspect; "
         "Stretch fills the screen; Custom uses the configured ratio.",
         SettingDef::Combo, "0",
         { {"Auto",                "0"},
           {"Force 16:9",           "1"},
           {"Force 4:3",            "2"},
           {"Stretch to Window",    "3"},
           {"Custom",               "4"},
           {"Custom (Stretch)",     "5"} }}),

        gfx({"Graphics", "General", "Basic", "Hardware", "VSync",
         "V-Sync",
         "Synchronizes output to the display refresh rate. Reduces tearing.",
         SettingDef::Bool, "True"}),

        // Fullscreen lives in Dolphin.ini's [Display] section, not GFX.ini.
        {"Graphics", "General", "Basic", "Display", "Fullscreen",
         "Start in Fullscreen",
         "Render in fullscreen mode. RetroNest already runs Dolphin "
         "embedded in our window, so this is True by default.",
         SettingDef::Bool, "True"},

        {"Graphics", "General", "Basic", "Core", "PrecisionFrameTiming",
         "Precision Frame Timing",
         "Uses high-resolution timers and busy-waiting for improved frame "
         "pacing. Marginally increases power usage.",
         SettingDef::Bool, "True"},

        // "Other" group — render-target settings. Both keys live in
        // Dolphin.ini's [Display] section (not GFX.ini).
        {"Graphics", "General", "Other", "Display", "RenderToMain",
         "Render to Main Window",
         "Embed the render output in the main window instead of opening a "
         "separate render window.",
         SettingDef::Bool, "False"},

        {"Graphics", "General", "Other", "Display", "RenderWindowAutoSize",
         "Auto-Adjust Window Size",
         "Resize the render window to match the internal resolution.",
         SettingDef::Bool, "False"},

        gfx({"Graphics", "General", "Shader Compilation", "Settings", "ShaderCompilationMode",
         "Shader Compilation",
         "How shaders are compiled. Asynchronous variants reduce stutter at "
         "the cost of brief pop-in on first encounter.",
         SettingDef::Combo, "0",
         { {"Specialized (Default)",    "0"},
           {"Exclusive Ubershaders",    "1"},
           {"Hybrid Ubershaders",       "2"},
           {"Skip Drawing",             "3"} }}),

        gfx({"Graphics", "General", "Shader Compilation", "Settings", "WaitForShadersBeforeStarting",
         "Compile Shaders Before Starting",
         "Pre-compile the shader pipeline before launching a game. "
         "Slower start, smoother gameplay.",
         SettingDef::Bool, "False"}),

        // ─── Graphics / Enhancements ─────────────────────────
        // Mirrors EnhancementsWidget (EnhancementsWidget.cpp:60-255). Two
        // groups: Enhancements + Stereoscopy.
        //
        // Skipped (intentionally):
        //  - Anti-Aliasing combo: upstream is a ConfigComplexChoice over
        //    MSAA + SSAA. Without a synthesized 2-key combo it's safer to
        //    keep MSAA + SSAA as separate settings.
        //  - Texture Filtering combo: same — upstream combines
        //    MaxAnisotropy + ForceTextureFiltering. Kept as two settings.
        //  - Color Correction modal + Post-Processing Effect combo —
        //    need dialog/dynamic-shader-list infra.
        //  - Stereoscopy Depth/Convergence sliders — upstream uses float
        //    sliders; our slider widget is integer-only.

        gfx({"Graphics", "Enhancements", "Enhancements", "Settings", "InternalResolution",
         "Internal Resolution",
         "Render scale relative to native (1x = original, 6x ≈ 4K).",
         SettingDef::Combo, "1",
         { {"Auto (Multiple of 640x528)", "0"},
           {"Native (640x528)",            "1"},
           {"2x Native (1280x1056) for 720p",  "2"},
           {"3x Native (1920x1584) for 1080p", "3"},
           {"4x Native (2560x2112) for 1440p", "4"},
           {"5x Native (3200x2640)",           "5"},
           {"6x Native (3840x3168) for 4K",    "6"},
           {"7x Native (4480x3696)",           "7"},
           {"8x Native (5120x4224) for 5K",    "8"} }}),

        // Anti-Aliasing — upstream synthesizes a single ConfigComplexChoice
        // over MSAA + SSAA (EnhancementsWidget.cpp:110-111 + Update
        // AntialiasingOptions:408-440). Entries are populated dynamically
        // from `g_backend_info.AAModes` (the backend's supported MSAA
        // sample counts) plus mirrored "{N}x SSAA" entries when SSAA is
        // supported. We hard-code the {1, 2, 4, 8} sample set, which
        // every macOS-supported backend (Metal, Vulkan via MoltenVK, OGL)
        // honors; if the user picks a value the active backend doesn't
        // support, Dolphin clamps internally.
        []() {
            SettingDef d;
            d.category    = "Graphics";
            d.subcategory = "Enhancements";
            d.group       = "Enhancements";
            d.section     = "Settings";   // primary (MSAA) for the
            d.key         = "MSAA";       // dependsOn / focus-bar lookup
            d.label       = "Anti-Aliasing";
            d.tooltip     = "Reduces aliasing on object edges. SSAA is "
                            "significantly more demanding than MSAA but "
                            "applies anti-aliasing to lighting and shader "
                            "effects too.";
            d.iniFilePath = gfxIniPath();
            d.type          = SettingDef::Combo;
            d.defaultValue  = "None";
            d.options       = {
                {"None",     "None"},
                {"2x MSAA",  "2x MSAA"},
                {"4x MSAA",  "4x MSAA"},
                {"8x MSAA",  "8x MSAA"},
                {"2x SSAA",  "2x SSAA"},
                {"4x SSAA",  "4x SSAA"},
                {"8x SSAA",  "8x SSAA"},
            };
            d.saveTransform = [](const QString &v,
                                 const SettingDef::SaveCallback &save) {
                // (msaaSamples, ssaaFlag). SSAA half is a bool; MSAA half
                // is the sample count (1 means none — upstream's
                // ConfigComplexChoice writes (u32)1, false for "None").
                struct Pair { const char* msaa; const char* ssaa; };
                static const std::map<QString, Pair> table = {
                    {"None",    {"1", "False"}},
                    {"2x MSAA", {"2", "False"}},
                    {"4x MSAA", {"4", "False"}},
                    {"8x MSAA", {"8", "False"}},
                    {"2x SSAA", {"2", "True"}},
                    {"4x SSAA", {"4", "True"}},
                    {"8x SSAA", {"8", "True"}},
                };
                auto it = table.find(v);
                if (it == table.end()) it = table.find("None");
                save("Settings", "MSAA", it->second.msaa);
                save("Settings", "SSAA", it->second.ssaa);
            };
            d.loadTransform = [](const SettingDef::LoadCallback &read) {
                const QString msaa  = read("Settings", "MSAA");
                const QString ssaaR = read("Settings", "SSAA");
                const bool    ssaa  = ssaaR.compare("true", Qt::CaseInsensitive) == 0;
                const int     n     = msaa.isEmpty() ? 1 : msaa.toInt();
                if (n <= 1) return QString("None");
                const QString suffix = ssaa ? " SSAA" : " MSAA";
                return QString::number(n) + "x" + suffix;
            };
            return d;
        }(),

        // Texture Filtering — upstream's ConfigComplexChoice over
        // MaxAnisotropy + ForceTextureFiltering (EnhancementsWidget.cpp:
        // 113-131). 12 entries cover the cross-product of:
        //   - 5 anisotropic levels (1x..16x) at default sampling
        //   - "Default" entry that uses the game's choice
        //   - "Force Nearest and 1x Anisotropic" (only one combination)
        //   - "Force Linear and {1,2,4,8,16}x Anisotropic"
        // Upstream gates the combo on FastTextureSampling being true
        // (manual sampling at EnhancementsWidget.cpp:132 disables this
        // combo since the texture sampler is then driven by shader code).
        // AnisotropicFilteringMode enum: Default=-1, Force1x=0, Force2x=1,
        // Force4x=2, Force8x=3, Force16x=4. TextureFilteringMode enum:
        // Default=0, Nearest=1, Linear=2.
        []() {
            SettingDef d;
            d.category    = "Graphics";
            d.subcategory = "Enhancements";
            d.group       = "Enhancements";
            d.section     = "Enhancements"; // primary (MaxAnisotropy)
            d.key         = "MaxAnisotropy";
            d.label       = "Texture Filtering";
            d.tooltip     = "Sharpens distant textures (anisotropic) and "
                            "optionally forces a fixed magnification filter "
                            "(nearest/linear) regardless of what the game "
                            "requests. Disabled when Manual Texture Sampling "
                            "is on.";
            d.iniFilePath = gfxIniPath();
            d.type          = SettingDef::Combo;
            d.defaultValue  = "Default";
            d.options       = {
                {"Default",                            "Default"},
                {"1x Anisotropic",                     "1x Anisotropic"},
                {"2x Anisotropic",                     "2x Anisotropic"},
                {"4x Anisotropic",                     "4x Anisotropic"},
                {"8x Anisotropic",                     "8x Anisotropic"},
                {"16x Anisotropic",                    "16x Anisotropic"},
                {"Force Nearest and 1x Anisotropic",   "Force Nearest and 1x Anisotropic"},
                {"Force Linear and 1x Anisotropic",    "Force Linear and 1x Anisotropic"},
                {"Force Linear and 2x Anisotropic",    "Force Linear and 2x Anisotropic"},
                {"Force Linear and 4x Anisotropic",    "Force Linear and 4x Anisotropic"},
                {"Force Linear and 8x Anisotropic",    "Force Linear and 8x Anisotropic"},
                {"Force Linear and 16x Anisotropic",   "Force Linear and 16x Anisotropic"},
            };
            d.dependsOn   = "FastTextureSampling";
            d.saveTransform = [](const QString &v,
                                 const SettingDef::SaveCallback &save) {
                // (anisoLevel, forceFilter)
                struct Pair { const char* aniso; const char* filter; };
                static const std::map<QString, Pair> table = {
                    {"Default",                          {"-1", "0"}},
                    {"1x Anisotropic",                   {"0",  "0"}},
                    {"2x Anisotropic",                   {"1",  "0"}},
                    {"4x Anisotropic",                   {"2",  "0"}},
                    {"8x Anisotropic",                   {"3",  "0"}},
                    {"16x Anisotropic",                  {"4",  "0"}},
                    {"Force Nearest and 1x Anisotropic", {"0",  "1"}},
                    {"Force Linear and 1x Anisotropic",  {"0",  "2"}},
                    {"Force Linear and 2x Anisotropic",  {"1",  "2"}},
                    {"Force Linear and 4x Anisotropic",  {"2",  "2"}},
                    {"Force Linear and 8x Anisotropic",  {"3",  "2"}},
                    {"Force Linear and 16x Anisotropic", {"4",  "2"}},
                };
                auto it = table.find(v);
                if (it == table.end()) it = table.find("Default");
                save("Enhancements", "MaxAnisotropy",         it->second.aniso);
                save("Enhancements", "ForceTextureFiltering", it->second.filter);
            };
            d.loadTransform = [](const SettingDef::LoadCallback &read) {
                const QString anisoR = read("Enhancements", "MaxAnisotropy");
                const QString filtR  = read("Enhancements", "ForceTextureFiltering");
                const int aniso = anisoR.isEmpty() ? -1 : anisoR.toInt();
                const int filt  = filtR.isEmpty()  ? 0  : filtR.toInt();
                // aniso==-1 → Default (regardless of filt; upstream's
                // "Default" entry pairs (-1, 0)).
                if (aniso == -1) return QString("Default");
                static const char* anisoLabel[] = {
                    "1x", "2x", "4x", "8x", "16x"
                };
                if (aniso < 0 || aniso > 4) return QString("Default");
                const QString lvl = QString::fromLatin1(anisoLabel[aniso]);
                if (filt == 1) return QString("Force Nearest and 1x Anisotropic");
                if (filt == 2) return QString("Force Linear and ") + lvl + " Anisotropic";
                return lvl + " Anisotropic";
            };
            return d;
        }(),

        // Output Resampling enum (Source/Core/VideoCommon/VideoConfig.h
        // OutputResamplingMode). Labels mirror EnhancementsWidget.cpp:
        // 134-137 verbatim — including the "Bicubic:" prefix.
        gfx({"Graphics", "Enhancements", "Enhancements", "Enhancements", "OutputResampling",
         "Output Resampling",
         "Algorithm used to resample the rendered image to the window size.",
         SettingDef::Combo, "0",
         { {"Default",                       "0"},
           {"Bilinear",                       "1"},
           {"Bicubic: B-Spline",              "2"},
           {"Bicubic: Mitchell-Netravali",    "3"},
           {"Bicubic: Catmull-Rom",           "4"},
           {"Sharp Bilinear",                 "5"},
           {"Area Sampling",                  "6"} }}),

        gfx({"Graphics", "Enhancements", "Enhancements", "Hacks", "EFBScaledCopy",
         "Scaled EFB Copy",
         "Resize EFB copies to match the rendering scale. Required for "
         "high internal resolutions to look right.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Enhancements", "Enhancements", "Settings", "EnablePixelLighting",
         "Per-Pixel Lighting",
         "Higher-quality lighting. Slight performance cost; some games "
         "look noticeably better with it on.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Enhancements", "Settings", "wideScreenHack",
         "Widescreen Hack",
         "Force 4:3 games to render in widescreen by hacking the projection "
         "matrix. Can produce visual artifacts; useful for 4:3-only titles.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Enhancements", "Enhancements", "ForceTrueColor",
         "Force 24-Bit Color",
         "Force higher-precision color output. Reduces banding on gradients.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Enhancements", "Enhancements", "Settings", "DisableFog",
         "Disable Fog",
         "Skip rendering fog effects. Improves visibility in foggy games at "
         "the cost of intended atmosphere.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Enhancements", "Enhancements", "ArbitraryMipmapDetection",
         "Arbitrary Mipmap Detection",
         "Detect when a game uses mipmaps as separate images rather than "
         "true LODs, and treat them appropriately.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Enhancements", "Enhancements", "Enhancements", "DisableCopyFilter",
         "Disable Copy Filter",
         "Disable the post-process copy-filter pass. Reduces blur some games "
         "apply but may break intended visual effects.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Enhancements", "Enhancements", "Enhancements", "HDROutput",
         "HDR Post-Processing",
         "Output in HDR (high-dynamic-range) when the display supports it.",
         SettingDef::Bool, "False"}),

        // StereoMode enum: Off=0, SBS=1, TAB=2, Anaglyph=3, QuadBuffer=4,
        // Passive=5. Upstream label "Quad Buffer" → "HDMI 3D" mismatch
        // is a recent rename in Dolphin's source — combo follows the
        // upstream display name.
        gfx({"Graphics", "Enhancements", "Stereoscopy", "Stereoscopy", "StereoMode",
         "Stereoscopic 3D Mode",
         "3D-stereoscopic rendering mode. Off disables stereo entirely.",
         SettingDef::Combo, "0",
         { {"Off",             "0"},
           {"Side-by-Side",    "1"},
           {"Top-and-Bottom",  "2"},
           {"Anaglyph",        "3"},
           {"HDMI 3D",         "4"},
           {"Passive",         "5"} }}),

        gfx({"Graphics", "Enhancements", "Stereoscopy", "Stereoscopy", "StereoSwapEyes",
         "Swap Eyes",
         "Swap the left and right eye images.",
         SettingDef::Bool, "False"}),

        // Upstream hides "Use Full Resolution Per Eye" except when the
        // stereo mode is Side-by-Side or Top-and-Bottom (EnhancementsWidget
        // .cpp:243-248 + 263-271). Express that with a value-equality OR.
        gfx({"Graphics", "Enhancements", "Stereoscopy", "Stereoscopy", "StereoPerEyeResolutionFull",
         "Use Full Resolution Per Eye",
         "Render each eye at the full internal resolution instead of half. "
         "Doubles GPU cost.",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "",
         "StereoMode=1 || StereoMode=2"}),

        // ─── Graphics / Hacks ────────────────────────────────
        // Mirrors HacksWidget (HacksWidget.cpp:34-124). Four groups in
        // order: Embedded Frame Buffer (EFB), Texture Cache, External
        // Frame Buffer (XFB), Other. Three settings are upstream-inverted
        // (`ConfigBool(..., inverted=true)`) — wrapped here with `inv()`.
        //
        // Settings that previously lived in this sub-tab moved to match
        // upstream: EFBAccessDeferInvalidation + FastTextureSampling →
        // Advanced/Experimental, EFBScaledCopy → Enhancements,
        // DisableCopyToVRAM + CPUCull + EnableGPUTextureDecoding move
        // (DCV → Advanced/Utility, CPUCull → Advanced/Misc, GPUTexDecoding
        // → Hacks/Texture Cache where upstream actually places it).

        gfx(inv({"Graphics", "Hacks", "Embedded Frame Buffer (EFB)", "Hacks", "EFBAccessEnable",
         "Skip EFB Access from CPU",
         "Ignores any requests from the CPU to read from or write to the "
         "EFB. Speed boost; disables EFB-based effects in some games.",
         SettingDef::Bool, "True"})),

        gfx(inv({"Graphics", "Hacks", "Embedded Frame Buffer (EFB)", "Hacks", "EFBEmulateFormatChanges",
         "Ignore Format Changes",
         "Ignores any changes to the EFB format. Speed win for many games; "
         "graphical defects in a small number of others.",
         SettingDef::Bool, "True"})),

        gfx({"Graphics", "Hacks", "Embedded Frame Buffer (EFB)", "Hacks", "EFBToTextureEnable",
         "Store EFB Copies to Texture Only",
         "Stores EFB copies exclusively on the GPU, bypassing system "
         "memory. Big speed boost; can cause graphical defects in a small "
         "number of games.",
         SettingDef::Bool, "True"}),

        // Upstream disables Defer EFB Copies when both Store EFB Copies
        // AND Store XFB Copies are enabled — see HacksWidget::
        // UpdateDeferEFBCopiesEnabled (HacksWidget.cpp:271-277). Equivalent
        // enabled-when expression: "either EFB or XFB texture-only copy
        // is OFF".
        gfx({"Graphics", "Hacks", "Embedded Frame Buffer (EFB)", "Hacks", "DeferEFBCopies",
         "Defer EFB Copies to RAM",
         "Waits until the game synchronizes with the GPU before writing "
         "EFB copies to RAM. Speed boost.",
         SettingDef::Bool, "True", {}, 0, 0, 0, "", "",
         "!EFBToTextureEnable || !XFBToTextureEnable"}),

        gfx({"Graphics", "Hacks", "Texture Cache", "Settings", "SafeTextureCacheColorSamples",
         "Accuracy",
         "How many color samples Dolphin checks to decide if a cached "
         "texture is still valid. Safe = fewest misses, Fast = highest perf.",
         SettingDef::Combo, "128",
         { {"Safe",     "0"},
           {"Default",  "128"},
           {"Fast",     "512"} }}),

        gfx({"Graphics", "Hacks", "Texture Cache", "Settings", "EnableGPUTextureDecoding",
         "GPU Texture Decoding",
         "Decode textures on the GPU instead of the CPU. Speed win on "
         "most setups; disabled when Arbitrary Mipmap Detection is on.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "External Frame Buffer (XFB)", "Hacks", "XFBToTextureEnable",
         "Store XFB Copies to Texture Only",
         "Stores XFB copies exclusively on the GPU. Big speed boost; "
         "graphical defects in a small number of games.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "External Frame Buffer (XFB)", "Hacks", "ImmediateXFBEnable",
         "Immediately Present XFB",
         "Display the XFB as soon as it's drawn instead of on next vblank. "
         "Lower latency, slight tearing risk.",
         SettingDef::Bool, "False"}),

        // Upstream disables Skip Presenting Duplicate Frames when
        // Immediate XFB or VBI Skip is on (HacksWidget.cpp:279-284).
        gfx({"Graphics", "Hacks", "External Frame Buffer (XFB)", "Hacks", "SkipDuplicateXFBs",
         "Skip Presenting Duplicate Frames",
         "Detect and skip identical consecutive XFBs to save GPU work.",
         SettingDef::Bool, "True", {}, 0, 0, 0, "", "",
         "!ImmediateXFBEnable && !VISkip"}),

        gfx({"Graphics", "Hacks", "Other", "Settings", "FastDepthCalc",
         "Fast Depth Calculation",
         "Use a faster GPU-friendly depth-calculation path. Default-on.",
         SettingDef::Bool, "True"}),

        gfx(inv({"Graphics", "Hacks", "Other", "Hacks", "BBoxEnable",
         "Disable Bounding Box",
         "Disables bounding-box emulation. Significant GPU speed-up; some "
         "games (e.g. Paper Mario) require BBox to render correctly.",
         SettingDef::Bool, "True"})),

        gfx({"Graphics", "Hacks", "Other", "Hacks", "VertexRounding",
         "Vertex Rounding",
         "Round vertex coordinates to integers. Fixes seams in some games "
         "at higher internal resolutions.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Hacks", "Other", "Settings", "SaveTextureCacheToState",
         "Save Texture Cache to State",
         "Save the texture cache contents in save states. Larger states, "
         "smoother resume.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Hacks", "Other", "Hacks", "VISkip",
         "VBI Skip",
         "Skips Vertical Blank Interrupts when lag is detected. Smoother "
         "audio at non-100%% emulation speed; can cause freezes.",
         SettingDef::Bool, "False"}),

        // ─── Graphics / Advanced ─────────────────────────────
        // Mirrors AdvancedWidget (AdvancedWidget.cpp:43-206). Group
        // order: Debugging, Utility, Texture Dumping, Frame Dumping,
        // Misc, Experimental.
        //
        // Skipped: ProgressiveScan (SYSCONF_PROGRESSIVE_SCAN — SYSCONF
        // file, not text INI) and BorderlessFullscreen (Windows-only).

        gfx({"Graphics", "Advanced", "Debugging", "Settings", "WireFrame",
         "Enable Wireframe",
         "Render geometry as wireframe lines. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debugging", "Settings", "TexFmtOverlayEnable",
         "Texture Format Overlay",
         "Tag each texture with its format. May require an emulation reset.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debugging", "Settings", "EnableValidationLayer",
         "Enable API Validation Layers",
         "Enable graphics-API validation (Vulkan/D3D). Massive slowdown; "
         "useful for debugging shaders or backend issues.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Debugging", "Settings", "LogRenderTimeToFile",
         "Log Render Time to File",
         "Append per-frame GPU timing data to User/Logs/render_time.txt.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Utility", "Settings", "HiresTextures",
         "Load Custom Textures",
         "Load high-resolution texture replacements from "
         "User/Load/Textures/<game-id>/.",
         SettingDef::Bool, "False"}),

        // Prefetch is meaningless without custom textures loaded — gate
        // on HiresTextures (matches AdvancedWidget.cpp:210-211).
        gfx({"Graphics", "Advanced", "Utility", "Settings", "CacheHiresTextures",
         "Prefetch Custom Textures",
         "Pre-load all custom textures into VRAM at boot. Eliminates load "
         "stutter; uses more memory.",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "", "HiresTextures"}),

        gfx({"Graphics", "Advanced", "Utility", "Hacks", "DisableCopyToVRAM",
         "Disable EFB VRAM Copies",
         "Disable the GPU-side path for EFB/XFB copies. Forces a slower "
         "CPU path; debug-only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Utility", "Settings", "EnableMods",
         "Enable Graphics Mods",
         "Load graphics mods from User/Load/GraphicMods/<game-id>/.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Utility", "Settings", "DumpEFBTarget",
         "Dump EFB Target",
         "Dump the EFB contents to disk each frame. Debug only.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Utility", "Settings", "DumpXFBTarget",
         "Dump XFB Target",
         "Dump the XFB contents to disk each frame. Debug only.",
         SettingDef::Bool, "False"}),

        // Texture Dumping group — DumpBaseTextures + DumpMipTextures gate
        // on DumpTextures (AdvancedWidget.cpp:212-215).
        gfx({"Graphics", "Advanced", "Texture Dumping", "Settings", "DumpTextures",
         "Enable",
         "Save every texture the game uses to disk. Used to create custom "
         "texture packs.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Texture Dumping", "Settings", "DumpBaseTextures",
         "Dump Base Textures",
         "Include base mipmaps in the texture dump.",
         SettingDef::Bool, "True", {}, 0, 0, 0, "", "", "DumpTextures"}),

        gfx({"Graphics", "Advanced", "Texture Dumping", "Settings", "DumpMipTextures",
         "Dump Mip Maps",
         "Include all mipmap levels in the texture dump.",
         SettingDef::Bool, "True", {}, 0, 0, 0, "", "", "DumpTextures"}),

        // Frame Dumping group — order mirrors AdvancedWidget.cpp:127-154.
        // UseLossless + BitrateKbps are skipped — they're wrapped in
        // `#if defined(HAVE_FFMPEG)` upstream and the macOS Dolphin build
        // ships without FFmpeg, so they're invisible in the standalone
        // UI. Dolphin still dumps frames as PNG without FFmpeg; only
        // video-codec settings are gated.
        gfx({"Graphics", "Advanced", "Frame Dumping", "Settings", "FrameDumpsResolutionType",
         "Resolution Type",
         "Source for the dumped frame's resolution.",
         SettingDef::Combo, "0",
         { {"Window Resolution",                          "0"},
           {"Aspect Ratio Corrected Internal Resolution", "1"},
           {"Raw Internal Resolution",                    "2"} }}),

        gfx({"Graphics", "Advanced", "Frame Dumping", "Settings", "PNGCompressionLevel",
         "PNG Compression Level",
         "Compression level for PNG screenshots and frame dumps. 0 = none, "
         "9 = maximum.",
         SettingDef::Int, "6", {}, 0, 9, 1, "slider", ""}),

        // Misc group — order mirrors AdvancedWidget.cpp:172-176. ProgressiveScan
        // and BorderlessFullscreen omitted (SYSCONF / Windows-only).
        gfx({"Graphics", "Advanced", "Misc", "Settings", "Crop",
         "Crop",
         "Crop overscan/black borders from the rendered image.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Misc", "Settings", "BackendMultithreading",
         "Backend Multithreading",
         "Distribute video-backend work across multiple threads. Default-on "
         "is recommended upstream.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "Advanced", "Misc", "Settings", "PreferVSForLinePointExpansion",
         "Prefer VS for Point/Line Expansion",
         "Expand line/point primitives in the vertex shader instead of the "
         "geometry shader. Workaround for some drivers.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Advanced", "Misc", "Settings", "CPUCull",
         "Cull Vertices on the CPU",
         "Cull invisible geometry on the CPU before sending to the GPU. "
         "Speeds up some games.",
         SettingDef::Bool, "False"}),

        // Experimental group — order mirrors AdvancedWidget.cpp:189-195.
        // FastTextureSampling is upstream-inverted: the upstream label is
        // "Manual Texture Sampling", so checked = stored False = manual.
        gfx({"Graphics", "Advanced", "Experimental", "Hacks", "EFBAccessDeferInvalidation",
         "Defer EFB Cache Invalidation",
         "Reduces overhead by deferring EFB-cache invalidations. Speed win "
         "for many games; can cause visual glitches in a few.",
         SettingDef::Bool, "False"}),

        gfx(inv({"Graphics", "Advanced", "Experimental", "Hacks", "FastTextureSampling",
         "Manual Texture Sampling",
         "Trade some accuracy for speed in the texture sampler.",
         SettingDef::Bool, "False"})),

        // ─── Graphics / On-Screen Display ────────────────────
        // Mirrors OnScreenDisplayPane (Source/Core/DolphinQt/Settings/
        // OnScreenDisplayPane.cpp). Five groups in upstream order:
        // General, Performance Statistics, Movie Window, Netplay, Debug.

        // OSD messages + font size live in Dolphin.ini, not GFX.ini.
        {"Graphics", "On-Screen Display", "General", "Interface",
         "OnScreenDisplayMessages",
         "Show Messages",
         "Display Dolphin's own status messages (savestates, achievements, etc.)",
         SettingDef::Bool, "True"},

        // Font size only matters when on-screen messages are on.
        {"Graphics", "On-Screen Display", "General", "Settings", "OSDFontSize",
         "Font Size",
         "Point size for on-screen messages.",
         SettingDef::Int, "13", {}, 12, 40, 1, "slider", "pt",
         "OnScreenDisplayMessages"},

        // Performance Statistics group — order mirrors
        // OnScreenDisplayPane.cpp:54-63.
        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowFPS",
         "Show FPS",
         "Frames per second the GPU is drawing.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowFTimes",
         "Show Frame Times",
         "Per-frame GPU time graph.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowVPS",
         "Show VPS",
         "VBlanks per second — the rate the emulated game thinks it's running at.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowVTimes",
         "Show VBlank Times",
         "Per-vblank time graph.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowSpeed",
         "Show % Speed",
         "Emulation speed as a percentage of native.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowGraphs",
         "Show Performance Graphs",
         "Render the FPS/VPS history as a graph instead of a single number.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "ShowSpeedColors",
         "Show Speed Colors",
         "Tint the speed indicator green/yellow/red based on how close "
         "to native it is.",
         SettingDef::Bool, "True"}),

        gfx({"Graphics", "On-Screen Display", "Performance Statistics", "Settings",
         "PerfSampWindowMS",
         "Performance Sample Window (ms)",
         "Sliding window (in milliseconds) for FPS/VPS averaging. Higher = "
         "more stable, slower to update.",
         SettingDef::Int, "1000", {}, 0, 10000, 100, "slider", "ms"}),

        // Movie Window group — order mirrors OnScreenDisplayPane.cpp:
        // 78-83. The five sub-options gate on the Movie Window toggle
        // itself (OnScreenDisplayPane.cpp:122-133).
        {"Graphics", "On-Screen Display", "Movie Window", "Movie",
         "ShowOSD",
         "Show Movie Window",
         "Show a window that can be filled with movie-recording info "
         "(rerecord/lag/frame counters, input display, system clock).",
         SettingDef::Bool, "False"},

        {"Graphics", "On-Screen Display", "Movie Window", "Movie",
         "ShowRerecord",
         "Show Rerecord Counter",
         "How many times the input recording has been overwritten by "
         "loading savestates.",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "", "ShowOSD"},

        {"Graphics", "On-Screen Display", "Movie Window", "General", "ShowLag",
         "Show Lag Counter",
         "Display the per-frame input-lag counter (movie/replay tooling).",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "", "ShowOSD"},

        {"Graphics", "On-Screen Display", "Movie Window", "General",
         "ShowFrameCount",
         "Show Frame Counter",
         "Display the running frame number.",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "", "ShowOSD"},

        {"Graphics", "On-Screen Display", "Movie Window", "Movie",
         "ShowInputDisplay",
         "Show Input Display",
         "Display the controls currently being input.",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "", "ShowOSD"},

        {"Graphics", "On-Screen Display", "Movie Window", "Movie",
         "ShowRTC",
         "Show System Clock",
         "Display the current system time.",
         SettingDef::Bool, "False", {}, 0, 0, 0, "", "", "ShowOSD"},

        // Netplay group.
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

        // Debug group — moved from Graphics/Advanced/Debug to match
        // upstream's OnScreenDisplayPane::CreateLayout (OnScreenDisplay
        // Pane.cpp:97-106).
        gfx({"Graphics", "On-Screen Display", "Debug", "Settings",
         "OverlayStats",
         "Show Statistics",
         "Render various rendering statistics.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "On-Screen Display", "Debug", "Settings",
         "OverlayProjStats",
         "Show Projection Statistics",
         "Render projection-matrix statistics.",
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
        // Slot A, Slot B, and SP1 (Serial Port 1) live in one "Device
        // Settings" group upstream (GameCubePane.cpp:94-198). Row labels
        // upstream are "Slot A:" / "Slot B:" / "SP1:" — our schema-driven
        // UI strips the trailing colon convention, so we use the bare
        // names. Combo entries + their order come from
        // GameCubePane.cpp:136-163; printable names come from the
        // EnumFormatter at EXI_Device.h:88-136. Numeric values match
        // EXIDeviceType (None = 0xFF/255).
        {"GameCube", "", "Device Settings", "Core", "SlotA",
         "Slot A",
         "Device plugged into the GameCube's left memory-card / EXI slot.",
         SettingDef::Combo, "8",
         { {"<Nothing>",                "255"},
           {"Dummy",                    "0"},
           {"Memory Card",              "1"},
           {"GCI Folder",               "8"},
           {"USB Gecko",                "7"},
           {"Advance Game Port",        "9"},
           {"Microphone",               "4"} }},

        {"GameCube", "", "Device Settings", "Core", "SlotB",
         "Slot B",
         "Device plugged into the GameCube's right memory-card / EXI slot. "
         "Microphone is most common here for games like Mario Party.",
         SettingDef::Combo, "255",
         { {"<Nothing>",                "255"},
           {"Dummy",                    "0"},
           {"Memory Card",              "1"},
           {"GCI Folder",               "8"},
           {"USB Gecko",                "7"},
           {"Advance Game Port",        "9"},
           {"Microphone",               "4"} }},

        {"GameCube", "", "Device Settings", "Core", "SerialPort1",
         "SP1",
         "Device plugged into the GameCube's serial port — typically used "
         "for network adapters in compatible games.",
         SettingDef::Combo, "255",
         { {"<Nothing>",                       "255"},
           {"Dummy",                           "0"},
           {"Broadband Adapter (TAP)",         "5"},
           {"Broadband Adapter (XLink Kai)",   "10"},
           {"Broadband Adapter (tapserver)",   "11"},
           {"Broadband Adapter (HLE)",         "12"},
           {"Modem Adapter (tapserver)",       "13"},
           {"Triforce Baseboard",              "6"} }},

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
        // ─── Wii / Misc Settings ─────────────────────────────
        // Order mirrors WiiPane::CreateMisc grid (WiiPane.cpp:153-162).
        // After dropping SYSCONF rows (PAL60, Screen Saver, Aspect Ratio,
        // System Language, Sound), the only INI-backed Misc settings are
        // Connect USB Keyboard (row 0 col 1) and Enable WiiConnect24 via
        // WiiLink (row 1 col 1).
        {"Wii", "", "Misc Settings", "Core", "WiiKeyboard",
         "Connect USB Keyboard",
         "Make a USB keyboard visible to Wii software (e.g. for chat-aware "
         "homebrew).",
         SettingDef::Bool, "False"},

        {"Wii", "", "Misc Settings", "Core", "EnableWiiLink",
         "Enable WiiConnect24 via WiiLink",
         "Patch the Wii Shop / Wii Channels to use community-run "
         "replacement servers (WiiLink). Off by default to avoid surprising "
         "users with third-party network calls.",
         SettingDef::Bool, "False"},

        // ─── Wii / SD Card Settings ──────────────────────────
        // Order mirrors WiiPane::CreateSDCard (WiiPane.cpp:165-277).
        // SD Card Path / SD Sync Folder text fields are skipped — we
        // don't have a path/file-picker widget type yet; users set those
        // via Dolphin's native UI when needed. Pack/Unpack buttons are
        // skipped (actions, not settings).
        {"Wii", "", "SD Card Settings", "Core", "WiiSDCard",
         "Insert SD Card",
         "Make a virtual SD card visible to Wii software. Required for "
         "save imports, channel installs, and homebrew that uses the slot.",
         SettingDef::Bool, "True"},

        {"Wii", "", "SD Card Settings", "Core", "WiiSDCardAllowWrites",
         "Allow Writes to SD Card",
         "When off, the SD card is read-only — useful for protecting a "
         "shared image from accidental modification.",
         SettingDef::Bool, "True"},

        {"Wii", "", "SD Card Settings", "Core", "WiiSDCardEnableFolderSync",
         "Automatically Sync with Folder",
         "Mirror the SD card image from a host folder. Lets you drop "
         "files in/out of the SD without booting the Wii system menu.",
         SettingDef::Bool, "False"},

        // Capacity stored in BYTES, matching the SDSizeComboEntry table at
        // WiiPane.cpp:58-70. Combo values are exact byte counts so the
        // INI round-trips with what Dolphin writes.
        {"Wii", "", "SD Card Settings", "Core", "WiiSDCardFilesize",
         "SD Card File Size",
         "Capacity of the virtual SD card. Auto = use the image file as-is "
         "(don't resize).",
         SettingDef::Combo, "0",
         { {"Auto",            "0"},
           {"64 MiB",           "67108864"},
           {"128 MiB",          "134217728"},
           {"256 MiB",          "268435456"},
           {"512 MiB",          "536870912"},
           {"1 GiB",            "1073741824"},
           {"2 GiB",            "2147483648"},
           {"4 GiB (SDHC)",     "4294967296"},
           {"8 GiB (SDHC)",     "8589934592"},
           {"16 GiB (SDHC)",    "17179869184"},
           {"32 GiB (SDHC)",    "34359738368"} }},
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

QString DolphinAdapter::subcategoryIcon(const QString& category,
                                         const QString& subcategory) const {
    if (category != "Graphics") return {};
    if (subcategory == "General")           return QStringLiteral("⚙");      // ⚙
    if (subcategory == "Enhancements")      return QStringLiteral("✨");      // ✨
    if (subcategory == "Hacks")             return QStringLiteral("⚡");      // ⚡
    if (subcategory == "Advanced")          return QStringLiteral("\U0001F527");  // 🔧
    if (subcategory == "On-Screen Display") return QStringLiteral("\U0001F4CA");  // 📊
    return {};
}

// ============================================================================
// Controller binding helpers (anonymous namespace)
// ============================================================================

namespace {

QVector<BindingDef> wiimoteBindings() {
    // Wii Remote SVG: viewBox 0 0 777 1614 (portrait, held vertically)
    // D-Pad cross centre anchored at (387, 390) from arm-path translates
    // A button: blue highlight cluster at y≈753-779, centre (388, 768)
    // 1/2 buttons: translate(410,1116) and translate(410,1243) → centres (420,1140) (420,1265)
    // −/+/Home row: translate(358,218)/(411,217) arms + translate(383,229) centre nub
    // IR sensor grill: translate(265,52) cluster, centre (388, 72)
    return {
        // D-Pad — cluster centre (387, 390); arm offset 65
        {BindingDef::Button, "Up",    "D-Pad", "Wiimote1", "D-Pad/Up",    "`Pad N`",
            "DPad", 387, 325, 55},
        {BindingDef::Button, "Down",  "D-Pad", "Wiimote1", "D-Pad/Down",  "`Pad S`",
            "DPad", 387, 455, 55},
        {BindingDef::Button, "Left",  "D-Pad", "Wiimote1", "D-Pad/Left",  "`Pad W`",
            "DPad", 322, 390, 55},
        {BindingDef::Button, "Right", "D-Pad", "Wiimote1", "D-Pad/Right", "`Pad E`",
            "DPad", 452, 390, 55},

        // Buttons
        {BindingDef::Button, "A", "Buttons", "Wiimote1", "Buttons/A", "`Button S`",
            "FaceButtons", 388, 768, 70},    // large round button, blue highlight cluster y≈753-779
        {BindingDef::Button, "B", "Buttons", "Wiimote1", "Buttons/B", "`Button E`",
            "FaceButtons", 490, 750, 50},    // underside trigger, eyeballed (not directly visible)
        {BindingDef::Button, "1", "Buttons", "Wiimote1", "Buttons/1", "`Button W`",
            "FaceButtons", 420, 1140, 40},   // translate(410,1116) arm
        {BindingDef::Button, "2", "Buttons", "Wiimote1", "Buttons/2", "`Button N`",
            "FaceButtons", 420, 1265, 40},   // translate(410,1243) arm

        // Tilt → LeftAnalog — no physical button; spotlight Wiimote body centre (388, 807)
        {BindingDef::Axis, "Tilt Forward",  "Tilt", "Wiimote1", "Tilt/Forward",  "`Left Y-`",
            "LeftAnalog", 388, 720, 60},
        {BindingDef::Axis, "Tilt Backward", "Tilt", "Wiimote1", "Tilt/Backward", "`Left Y+`",
            "LeftAnalog", 388, 895, 60},
        {BindingDef::Axis, "Tilt Left",     "Tilt", "Wiimote1", "Tilt/Left",     "`Left X-`",
            "LeftAnalog", 320, 807, 60},
        {BindingDef::Axis, "Tilt Right",    "Tilt", "Wiimote1", "Tilt/Right",    "`Left X+`",
            "LeftAnalog", 455, 807, 60},

        // IR → RightAnalog — spotlight IR sensor grill at top (388, 72)
        {BindingDef::Axis, "IR Up",    "IR", "Wiimote1", "IR/Up",    "`Right Y-`",
            "RightAnalog", 388, 30, 55},
        {BindingDef::Axis, "IR Down",  "IR", "Wiimote1", "IR/Down",  "`Right Y+`",
            "RightAnalog", 388, 120, 55},
        {BindingDef::Axis, "IR Left",  "IR", "Wiimote1", "IR/Left",  "`Right X-`",
            "RightAnalog", 330, 72, 55},
        {BindingDef::Axis, "IR Right", "IR", "Wiimote1", "IR/Right", "`Right X+`",
            "RightAnalog", 445, 72, 55},

        // Shake → LeftShoulders — abstract, no spotlight
        {BindingDef::Button, "Shake X", "Shake", "Wiimote1", "Shake/X", "`Shoulder L`",
            "LeftShoulders", 0, 0, 0},
        {BindingDef::Button, "Shake Y", "Shake", "Wiimote1", "Shake/Y", "`Shoulder L`",
            "LeftShoulders", 0, 0, 0},
        {BindingDef::Button, "Shake Z", "Shake", "Wiimote1", "Shake/Z", "`Shoulder L`",
            "LeftShoulders", 0, 0, 0},

        // System — −/+/Home row at y≈217-250
        {BindingDef::Button, "Minus", "System", "Wiimote1", "Buttons/-",    "`Back`",
            "System", 370, 230, 35},   // translate(358,218) arm, centre x=370
        {BindingDef::Button, "Plus",  "System", "Wiimote1", "Buttons/+",    "`Start`",
            "System", 423, 230, 35},   // translate(411,217) arm, centre x=423
        {BindingDef::Button, "Home",  "System", "Wiimote1", "Buttons/Home", "`Guide`",
            "System", 388, 244, 35},   // translate(383,229) centre nub
        {BindingDef::Axis,   "Rumble/Motor", "System", "Wiimote1", "Rumble/Motor", "`Motor`",
            "System", 0, 0, 0},
    };
}

QVector<BindingDef> gcPadBindings() {
    // GameCube SVG: viewBox 0 0 1802 1361 (landscape)
    // D-Pad: blue/purple cluster centre (885, 395), paths range x=[811,958] y=[353,443]
    // Main Stick: dense grey cluster centre (765, 635), range x=[675,830] y=[545,726]
    // C-Stick: yellow paths centre (1195, 868), range x=[1143,1242] y=[800,936]
    // A button: teal-green paths at (1429,431)/(1441,458), centre (1430, 460)
    // B button: red paths cluster centre (1245, 545)
    // X/Y: grey, flanking A; X right ≈(1510,460), Y above ≈(1430,375)
    // L trigger: left-side cluster centre (205, 185)
    // R trigger: right-side cluster centre (1660, 230)
    // Z: small top-right cluster centre (1340, 100)
    // Start: pale oval cluster centre (1050, 640)
    return {
        // D-Pad — cluster centre (885, 395); arm offset 70
        {BindingDef::Button, "Up",    "D-Pad", "GCPad1", "D-Pad/Up",    "`Pad N`",
            "DPad", 885, 325, 60},
        {BindingDef::Button, "Down",  "D-Pad", "GCPad1", "D-Pad/Down",  "`Pad S`",
            "DPad", 885, 465, 60},
        {BindingDef::Button, "Left",  "D-Pad", "GCPad1", "D-Pad/Left",  "`Pad W`",
            "DPad", 815, 395, 60},
        {BindingDef::Button, "Right", "D-Pad", "GCPad1", "D-Pad/Right", "`Pad E`",
            "DPad", 955, 395, 60},

        // Face Buttons
        {BindingDef::Button, "A", "Face Buttons", "GCPad1", "Buttons/A", "`Button S`",
            "FaceButtons", 1430, 460, 100},  // large teal-green button, centre of cluster
        {BindingDef::Button, "B", "Face Buttons", "GCPad1", "Buttons/B", "`Button E`",
            "FaceButtons", 1245, 545, 50},   // red cluster centre (1223-1288, 520-575)
        {BindingDef::Button, "X", "Face Buttons", "GCPad1", "Buttons/X", "`Button W`",
            "FaceButtons", 1515, 460, 65},   // grey, right of A
        {BindingDef::Button, "Y", "Face Buttons", "GCPad1", "Buttons/Y", "`Button N`",
            "FaceButtons", 1430, 375, 65},   // grey, above A

        // Main Stick — cluster centre (765, 635); direction offset 90
        {BindingDef::Axis, "Main Stick Up",    "Main Stick", "GCPad1", "Main Stick/Up",    "`Left Y-`",
            "LeftAnalog", 765, 545, 90},
        {BindingDef::Axis, "Main Stick Down",  "Main Stick", "GCPad1", "Main Stick/Down",  "`Left Y+`",
            "LeftAnalog", 765, 725, 90},
        {BindingDef::Axis, "Main Stick Left",  "Main Stick", "GCPad1", "Main Stick/Left",  "`Left X-`",
            "LeftAnalog", 675, 635, 90},
        {BindingDef::Axis, "Main Stick Right", "Main Stick", "GCPad1", "Main Stick/Right", "`Left X+`",
            "LeftAnalog", 855, 635, 90},

        // C-Stick — yellow cluster centre (1195, 868); direction offset 88
        {BindingDef::Axis, "C-Stick Up",    "C-Stick", "GCPad1", "C-Stick/Up",    "`Right Y-`",
            "RightAnalog", 1195, 780, 80},
        {BindingDef::Axis, "C-Stick Down",  "C-Stick", "GCPad1", "C-Stick/Down",  "`Right Y+`",
            "RightAnalog", 1195, 956, 80},
        {BindingDef::Axis, "C-Stick Left",  "C-Stick", "GCPad1", "C-Stick/Left",  "`Right X-`",
            "RightAnalog", 1107, 868, 80},
        {BindingDef::Axis, "C-Stick Right", "C-Stick", "GCPad1", "C-Stick/Right", "`Right X+`",
            "RightAnalog", 1283, 868, 80},

        // Triggers / shoulder — L and L-Analog share the same physical trigger
        {BindingDef::Button, "L (digital)", "Triggers", "GCPad1", "Triggers/L",        "`Trigger L`",
            "LeftShoulders", 205, 185, 120},
        {BindingDef::Axis,   "L-Analog",    "Triggers", "GCPad1", "Triggers/L-Analog", "`Trigger L`",
            "LeftShoulders", 205, 185, 120},
        {BindingDef::Button, "R (digital)", "Triggers", "GCPad1", "Triggers/R",        "`Trigger R`",
            "RightShoulders", 1660, 230, 130},
        {BindingDef::Axis,   "R-Analog",    "Triggers", "GCPad1", "Triggers/R-Analog", "`Trigger R`",
            "RightShoulders", 1660, 230, 130},
        {BindingDef::Button, "Z",           "Buttons",  "GCPad1", "Buttons/Z",         "`Shoulder R`",
            "RightShoulders", 1340, 100, 65},  // small top-right cluster at y≈48-152

        // System
        {BindingDef::Button, "Start", "System", "GCPad1", "Buttons/Start", "`Start`",
            "System", 1050, 640, 65},    // pale oval cluster centre (974-1104, 564-718)
        {BindingDef::Axis,   "Rumble/Motor", "System", "GCPad1", "Rumble/Motor", "`Motor`",
            "System", 0, 0, 0},
    };
}

}  // namespace

// ============================================================================
// Controller types
// ============================================================================

QVector<ControllerTypeDef> DolphinAdapter::controllerTypes() const {
    ControllerTypeDef gcpad{
        "GCPad1", "GameCube Controller",
        ":/AppUI/qml/AppUI/images/controllers/GameCube.svg",
        /*slotTitleOverrides=*/{}
    };
    ControllerTypeDef wii{
        "Wiimote1", "Wii Remote",
        ":/AppUI/qml/AppUI/images/controllers/Wii.svg",
        /*slotTitleOverrides=*/{
            {"LeftAnalog",    "TILT"},
            {"RightAnalog",   "IR POINTING"},
            {"LeftShoulders", "SHAKE"},
        }
    };
    return {gcpad, wii};
}

QVector<BindingDef> DolphinAdapter::controllerBindingDefs() const {
    return controllerBindingDefsForType("GCPad1");
}

// ============================================================================
// Type-aware bindings dispatch
// ============================================================================

QVector<BindingDef> DolphinAdapter::controllerBindingDefsForType(const QString& type) const {
    if (type == "GCPad1")   return gcPadBindings();
    if (type == "Wiimote1") return wiimoteBindings();
    return {};
}

// ============================================================================
// Type-aware bindings-storage routing
// ============================================================================

QString DolphinAdapter::controllerBindingsConfigFilePath(const QString& controllerTypeId) const {
    if (controllerTypeId == "Wiimote1") return wiimoteIniPath();
    return gcpadIniPath();  // default for "GCPad1" or empty
}

QString DolphinAdapter::controllerBindingsSection(int port, const QString& controllerTypeId) const {
    Q_UNUSED(port);  // v1: port always 1
    return controllerTypeId.isEmpty() ? "GCPad1" : controllerTypeId;
}

// ============================================================================
// formatBinding — Dolphin expression syntax: bare element name in backticks,
// device communicated separately via the section's Device = line (see
// writeBindingDeviceHeader). Axis polarity is encoded in the element name
// for sticks; triggers stay polarity-less ("Trigger L" not "+Trigger L").
// ============================================================================

QString DolphinAdapter::formatBinding(int /*deviceIndex*/, const QString& element,
                                       bool isAxis, bool positive) const {
    // Stick axes: "Left X-" / "Left X+" etc. Triggers / non-stick axes stay
    // polarity-less.
    static const QHash<QString, QString> stickAxisRoot{
        {"LeftX",  "Left X"},
        {"LeftY",  "Left Y"},
        {"RightX", "Right X"},
        {"RightY", "Right Y"},
    };
    if (isAxis) {
        if (auto it = stickAxisRoot.constFind(element); it != stickAxisRoot.constEnd()) {
            const QString polarity = positive ? "+" : "-";
            return QString("`%1%2`").arg(it.value(), polarity);
        }
        // Non-stick axis (Trigger L/R)
        if (element == "LeftTrigger")  return "`Trigger L`";
        if (element == "RightTrigger") return "`Trigger R`";
    }

    static const QHash<QString, QString> buttonNames{
        {"FaceSouth",     "Button S"},
        {"FaceEast",      "Button E"},
        {"FaceWest",      "Button W"},
        {"FaceNorth",     "Button N"},
        {"DPadUp",        "Pad N"},
        {"DPadDown",      "Pad S"},
        {"DPadLeft",      "Pad W"},
        {"DPadRight",     "Pad E"},
        {"LeftShoulder",  "Shoulder L"},
        {"RightShoulder", "Shoulder R"},
        {"LeftStick",     "Thumb L"},
        {"RightStick",    "Thumb R"},
        {"Back",          "Back"},
        {"Start",         "Start"},
        {"Guide",         "Guide"},
    };
    if (auto it = buttonNames.constFind(element); it != buttonNames.constEnd())
        return QString("`%1`").arg(it.value());

    qWarning() << "[DolphinAdapter] unknown SDL element for formatBinding:" << element;
    return {};
}

// ============================================================================
// writeBindingDeviceHeader — writes "Device = SDL/{idx}/{name}" into the
// active section so Dolphin knows which physical device to use.
// ============================================================================

void DolphinAdapter::writeBindingDeviceHeader(IniFile& ini, const QString& section,
                                               int deviceIndex, QObject* input) const {
    if (deviceIndex < 0 || !input) return;

    const QVariantList controllers = input->property("connectedControllers").toList();

    QString deviceName;
    for (const auto& v : controllers) {
        const auto m = v.toMap();
        if (m.value("deviceIndex").toInt() == deviceIndex) {
            deviceName = m.value("name").toString();
            break;
        }
    }
    if (deviceName.isEmpty()) {
        // Couldn't resolve — leave the existing Device line untouched rather
        // than writing "SDL/N/" with a blank name (would break Dolphin's
        // parser).
        return;
    }
    ini.setValue(section, "Device", QString("SDL/%1/%2").arg(deviceIndex).arg(deviceName));
}
