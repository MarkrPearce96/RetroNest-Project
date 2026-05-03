#include "dolphin_adapter.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>

#include "core/paths.h"

namespace {
constexpr const char* DOLPHIN_INSTALL_FOLDER = "dolphin";
}

// ============================================================================
// Path helpers
// ============================================================================

QString DolphinAdapter::portableDir() {
    const QString installPath = Paths::emulatorsDir(DOLPHIN_INSTALL_FOLDER);
#if defined(Q_OS_MACOS)
    QDir dir(installPath);
    const auto entries = dir.entryList({"*.app"}, QDir::Dirs);
    for (const auto& entry : entries) {
        QString candidate = installPath + "/" + entry + "/Contents/MacOS";
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return installPath;
#else
    return installPath;
#endif
}

QString DolphinAdapter::userConfigDir() {
    return portableDir() + "/User/Config";
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
    return resolveExecutableInDir(manifest, installPath, "DolphinQt");
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
// Settings schema — Dolphin.ini only for v1 (graphics page deferred)
// ============================================================================

QVector<SettingDef> DolphinAdapter::settingsSchema() const {
    // Filled in by Task 12.
    return {};
}

// ============================================================================
// ensureConfig — multi-file
// ============================================================================

bool DolphinAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                   const QString& /*biosPath*/,
                                   const QString& /*savesPath*/) {
    // 1) Portable marker so Dolphin reads from User/ next to the binary.
    if (!ensurePortableMarker(portableDir(), "Dolphin"))
        return false;

    // 2) Ensure User/Config exists.
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
    if (!content.contains("AspectRatio") || !content.contains("InternalResolution")) {
        QVector<IniKeyPatch> seedPatches;
        if (!content.contains("AspectRatio "))
            seedPatches.append({"Settings", "AspectRatio", "0"});
        if (!content.contains("InternalResolution"))
            seedPatches.append({"Settings", "InternalResolution", "1"});
        if (!seedPatches.isEmpty() && patchIniKeys(content, seedPatches))
            return writeConfigFile(path, content, "Dolphin");
    }

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

void DolphinAdapter::patchRetroAchievements(const QString&, const QString&,
                                             bool, bool, bool, bool) {
    // Filled in by Task 13.
}
