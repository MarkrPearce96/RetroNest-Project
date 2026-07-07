#pragma once

#include "core/manifest.h"
#include "core/paths.h"
#include "core/preview_spec.h"
#include "core/setting_def.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

class IniFile;
class QObject;
class LibretroAdapter;  // typed downcast target; see asLibretro() below

/**
 * ResolutionOption — a friendly label + INI value pair for resolution selection.
 */
struct ResolutionOption {
    QString label;   // e.g. "720P"
    QString value;   // INI value, e.g. "2"
};

/**
 * ResolutionOptions — describes how to set resolution for an emulator.
 *
 * If `iniFilePath` is non-empty, it overrides configFilePath() as the
 * caller-supplied absolute path read/written by the quick-settings UI
 * for resolution. Adapters whose resolution lives in a separate file
 * (e.g. Dolphin's GFX.ini) set it to that absolute path; others leave
 * it empty.
 */
struct ResolutionOptions {
    QString section;       // INI section
    QString key;           // INI key
    QVector<ResolutionOption> options;
    QString defaultValue;  // which value is default
    QString iniFilePath;   // optional caller-supplied absolute path; empty = use configFilePath()
};

/**
 * IniPatch — a single section/key/value to write to an INI file.
 *
 * If `iniFilePath` is non-empty, it overrides configFilePath() as the
 * destination (caller-supplied absolute path). Used by adapters whose
 * aspect-ratio patches target a non-main file (e.g. Dolphin's GFX.ini).
 */
struct IniPatch {
    QString section;
    QString key;
    QString value;
    QString iniFilePath;   // optional caller-supplied absolute path; empty = use configFilePath()
};

/**
 * AspectRatioOption — a label + list of INI patches to apply when selected.
 * Supports emulators that need multiple keys changed (e.g. aspect + widescreen patch).
 */
struct AspectRatioOption {
    QString label;               // e.g. "4:3", "16:9"
    QVector<IniPatch> patches;   // all INI writes for this choice
};

/**
 * AspectRatioOptions — describes aspect ratio choices for an emulator.
 *
 * Routing to a non-main config file is per-patch (via IniPatch::iniFilePath),
 * which is what Dolphin needs since a single aspect choice may touch
 * GFX.ini only.
 */
struct AspectRatioOptions {
    QVector<AspectRatioOption> options;
    QString defaultLabel;  // which option label is default
};

/**
 * BiosDef — describes a BIOS file an emulator may need.
 */
struct BiosDef {
    QString filename;       // e.g. "scph5501.bin"
    QString description;    // e.g. "PS1 BIOS (North America)"
    bool required = false;  // true = emulator won't work without it
    QString md5;            // optional MD5 for verification
};

/**
 * PathBase — which root directory a PathDef resolves from.
 */
enum class PathBase {
    Bios,          // Paths::biosDir()
    EmulatorData,  // Paths::emulatorDataDir(emuId, systemId) + "/" + suffix
};

/**
 * PathDef — describes a configurable folder path for an emulator.
 */
struct PathDef {
    QString label;          // e.g. "BIOS"
    QString section;        // INI section, e.g. "Folders" or "BIOS"
    QString key;            // INI key, e.g. "Bios" or "SearchDirectory"
    QString defaultSuffix;  // appended to base dir, e.g. "savestates"
    PathBase base = PathBase::EmulatorData;
};

/**
 * SettingsHubCard — one card on the per-emulator settings dialog hub.
 * The generic settings dialog builds a QGridLayout from a vector of
 * these; each card routes a click to the GenericSettingsPage for the
 * matching SettingDef::category.
 */
struct SettingsHubCard {
    QString icon;         // emoji glyph (e.g. U+1F4A1 for Recommended)
    QString title;        // card title, e.g. "Recommended"
    QString descriptor;   // 1-line subtitle shown under the title
    QString categoryKey;  // matches SettingDef::category for routing
    int row = 0;          // QGridLayout row
    int col = 0;          // QGridLayout column
    int rowSpan = 1;
    int colSpan = 1;
};

/**
 * EmulatorAdapter — abstract base for emulator-specific behavior.
 *
 * Each emulator gets an adapter that handles:
 * - Config generation (first-launch setup, wizard suppression)
 * - Platform-specific executable resolution
 * - Launch argument building (with special-case overrides)
 */
class EmulatorAdapter {
public:
    virtual ~EmulatorAdapter() = default;

    /**
     * Check if the emulator is installed by resolving the executable
     * and checking if it exists on disk.
     */
    virtual bool isInstalled(const EmulatorManifest& manifest) {
        const QString installPath = Paths::emulatorsDir(manifest.install_folder);
        const QString exec = resolveExecutable(manifest, installPath);
        return QFileInfo::exists(exec);
    }

    /**
     * Ensure the emulator's config is set up for headless/fullscreen operation.
     * Called before each launch. Creates or patches INI files to suppress
     * first-run wizards, set BIOS paths, configure folder paths, etc.
     * Returns true on success, false if config creation/patching failed.
     */
    virtual bool ensureConfig(const EmulatorManifest& manifest,
                              const QString& biosPath,
                              const QString& savesPath) = 0;

    /**
     * Resolve the platform-specific executable path.
     * Handles macOS .app bundles, Windows .exe suffixes, etc.
     * Returns the full path to the executable.
     */
    virtual QString resolveExecutable(const EmulatorManifest& manifest,
                                      const QString& installPath) = 0;

    /**
     * Return the full settings schema for this emulator.
     * Each entry describes one setting with its INI location, type, and UI metadata.
     */
    virtual QVector<SettingDef> settingsSchema() const { return {}; }

    /**
     * Return the card grid for the per-emulator settings hub. Empty
     * means "no settings UI" — AppController::showEmulatorSettings
     * warns and skips. Each card's `categoryKey` must match a
     * SettingDef::category in `settingsSchema()`.
     */
    virtual QVector<SettingsHubCard> settingsHubCards() const { return {}; }

    /**
     * Category keys whose pages render with sub-tabs (drives the L1/R1
     * "switch tab" hint chrome on the dialog). Default empty — adapters
     * with a Graphics pane that bundles OSD/Capture/etc. override.
     */
    virtual QStringList settingsCategoriesWithSubTabs() const { return {}; }

    /**
     * Return the preview-widget spec for one (category, subcategory) pair.
     * Default returns empty (no preview). Adapters that want an
     * AspectRatioPreview or OsdPreview shown alongside the settings cards
     * override this to return a non-empty PreviewSpec.
     */
    virtual PreviewSpec previewSpec(const QString& category,
                                    const QString& subcategory) const {
        Q_UNUSED(category);
        Q_UNUSED(subcategory);
        return {};
    }

    /**
     * Return the icon glyph (typically an emoji) shown above each sub-tab
     * label in GenericSettingsPage's SettingsGraphicsSubTabBar. Default
     * returns empty — the sub-tab renders label-only. Override per
     * (category, subcategory) pair.
     */
    virtual QString subcategoryIcon(const QString& category,
                                    const QString& subcategory) const {
        Q_UNUSED(category);
        Q_UNUSED(subcategory);
        return {};
    }

    /**
     * Return the path to this emulator's main config file.
     */
    virtual QString configFilePath() const { return {}; }

    /**
     * Typed downcast to LibretroAdapter — the only concrete subtype that
     * needs special handling at the EmulatorAdapter* call site (e.g.
     * GenericSettingsPage routing LibretroOption / FrontendSetting writes).
     *
     * Returns nullptr for native-process adapters; LibretroAdapter overrides
     * to return `this`. Prefer this over `dynamic_cast` — it avoids RTTI
     * and makes the libretro subtype an explicit part of the interface.
     *
     * libretroOptionsStore() / frontendSettingsStore() used to live on this
     * base as nullptr-returning virtuals; native adapters never overrode
     * them, so they were just noise. They now live on LibretroAdapter only;
     * call sites that hold an EmulatorAdapter* go through asLibretro().
     */
    virtual LibretroAdapter* asLibretro() { return nullptr; }

    /**
     * Return the path to the config file where controller bindings are stored.
     * Most emulators store bindings in the main config file, so this defaults
     * to configFilePath(). Override for emulators that use a separate file
     * (e.g., PPSSPP stores bindings in controls.ini).
     */
    virtual QString controllerBindingsConfigFilePath() const { return configFilePath(); }

    /**
     * Return the INI section name where controller bindings for a given port
     * are stored. Most emulators use a port-numbered section like "Pad1".
     * Override for emulators that use a different scheme (e.g., PPSSPP uses
     * a single "ControlMapping" section regardless of port).
     */
    virtual QString controllerBindingsSection(int port) const {
        return QString("Pad%1").arg(port);
    }

    /**
     * Type-aware overload: where bindings for `controllerTypeId` are stored.
     * Default delegates to the no-arg form; override when an emulator routes
     * different controller types to different files (e.g. Dolphin: GameCube
     * → GCPadNew.ini, Wii Classic Controller → WiimoteNew.ini).
     */
    virtual QString controllerBindingsConfigFilePath(const QString& controllerTypeId) const {
        Q_UNUSED(controllerTypeId);
        return controllerBindingsConfigFilePath();
    }

    /**
     * Type-aware overload: section name for `controllerTypeId` at `port`.
     * Default delegates to the port-only form; override when the section
     * depends on the controller type (e.g. Dolphin: "GCPad1" vs "Wiimote1").
     */
    virtual QString controllerBindingsSection(int port, const QString& controllerTypeId) const {
        Q_UNUSED(controllerTypeId);
        return controllerBindingsSection(port);
    }

    /**
     * Optional hook called after `ConfigService` writes per-binding values
     * into `section` of the bindings file. Default is a no-op — most
     * emulators encode the device in each binding string (e.g. "SDL-0/A").
     *
     * Dolphin overrides this to write a section-wide
     * `Device = SDL/{deviceIndex}/{deviceName}` line, since Dolphin's
     * INI format keeps the device separate from the per-key element name.
     *
     * `deviceIndex < 0` means "no device captured" (e.g. clear-all flow,
     * or a keyboard-only capture). Adapters should treat `< 0` as a no-op.
     */
    virtual void writeBindingDeviceHeader(IniFile& ini,
                                          const QString& section,
                                          int deviceIndex,
                                          QObject* input) const {
        Q_UNUSED(ini); Q_UNUSED(section);
        Q_UNUSED(deviceIndex); Q_UNUSED(input);
    }

    /**
     * Return the INI section name where controller settings (deadzone,
     * sensitivity, vibration) for a given port are stored. Default mirrors
     * controllerBindingsSection. Override when the emulator reads settings
     * from a different section than its bindings — e.g., PPSSPP stores
     * bindings in [ControlMapping] but settings in [Control].
     */
    virtual QString controllerSettingsSection(int port) const {
        return QString("Pad%1").arg(port);
    }

    /**
     * Return the list of BIOS files this emulator uses.
     */
    virtual QVector<BiosDef> biosFiles() const { return {}; }

    /**
     * Return the configurable folder paths for this emulator.
     */
    virtual QVector<PathDef> pathsDefs() const { return {}; }

    /**
     * Return the resolution options for the setup wizard.
     * Maps friendly labels (720P, 1080P, etc.) to INI values.
     */
    virtual ResolutionOptions resolutionOptions() const { return {}; }

    /**
     * Return the aspect ratio options for the setup wizard.
     * Each option can patch multiple INI keys (e.g. aspect ratio + widescreen patches).
     */
    virtual AspectRatioOptions aspectRatioOptions() const { return {}; }

    /**
     * Return available controller types for this emulator.
     * Default empty — adapters that expose a controller mapping page
     * return one or more `ControllerTypeDef`s.
     */
    virtual QVector<ControllerTypeDef> controllerTypes() const { return {}; }

    /**
     * Return controller bindings for a specific controller type.
     * Default empty. Override per type to expose bindings on the
     * controller mapping page.
     */
    virtual QVector<BindingDef> controllerBindingDefsForType(const QString& type) const {
        Q_UNUSED(type);
        return {};
    }

    /**
     * Return controller-specific settings for a given type (deadzone,
     * sensitivity, vibration, etc.). Default empty. Used by
     * `ConfigService::restoreDefaultsForPort` when resetting an
     * emulator's stored controller-settings section.
     */
    virtual QVector<SettingDef> controllerSettingDefsForType(const QString& type) const {
        Q_UNUSED(type);
        return {};
    }

    /**
     * Return hotkey binding definitions for this emulator.
     */
    virtual QVector<HotkeyDef> hotkeyBindingDefs() const { return {}; }


    /**
     * Whether this emulator supports saving a resume state when the game
     * exits. PCSX2 and DuckStation provide this via a native
     * "save state on shutdown" config key. PPSSPP has no such mechanism,
     * so it should return false and the "Exit & Save State" menu item
     * will be hidden for its games.
     */
    virtual bool supportsSaveOnExit() const { return false; }

    /**
     * Extract game serial/ID from a ROM file.
     * Default reads SYSTEM.CNF from ISO9660 image (works for PS1 and PS2).
     * Returns the serial (e.g. "SLUS_200.62") or empty string on failure.
     */
    virtual QString extractSerial(const QString& romPath) const;

    /**
     * Find resume state file for a given serial. Each adapter knows the
     * emulator-specific savestate directory and file-naming convention, so
     * no caller-supplied root is needed.
     * Returns the full path to the resume file, or empty string if not found.
     */
    virtual QString findResumeFile(const QString& serial) const {
        Q_UNUSED(serial);
        return {};
    }

    /**
     * Resolved download for adapters whose binaries are NOT distributed
     * through GitHub Releases (e.g. Dolphin distributes via dl.dolphin-emu.org).
     * See `resolveDirectDownload()` below.
     */
    struct DirectDownloadInfo {
        QString version;       // e.g. "2603a" — used for update checks + display
        QString publishedAt;   // ISO 8601 — for update detection (may be empty if version alone is unique)
        QString assetName;     // filename for the downloaded file (e.g. "dolphin-2603a-universal.dmg")
        QString downloadUrl;   // direct URL to download
        QString sha256;        // optional — empty = skip verification; lower-case hex
    };

    /**
     * Optional override for adapters whose binaries are distributed outside
     * GitHub Releases. Default returns an empty struct → EmulatorInstaller
     * uses the normal GitHub Releases flow (`/releases/latest` + asset
     * matching).
     *
     * If you override this and return a non-empty `downloadUrl`, the
     * installer skips the GitHub Releases API entirely and downloads
     * directly from `downloadUrl`. You're responsible for resolving the
     * latest version (typically via the GitHub `/tags` endpoint —
     * `GitHubClient::fetchLatestStableTag` is provided for this).
     *
     * Called synchronously from the installer (both sync and async paths),
     * so any network work blocks for the duration. Keep it brief.
     */
    virtual DirectDownloadInfo resolveDirectDownload(const EmulatorManifest& manifest) const {
        Q_UNUSED(manifest);
        return {};
    }

    /**
     * Declarative rule for matching a GitHub release asset by name.
     * An asset name matches if every entry in `substrings` is contained in
     * the lower-cased asset name AND the asset name ends with `extension`.
     * Order in assetMatchRules() is preference order — first match wins.
     */
    struct AssetMatchRule {
        QStringList substrings;  // all must be in name.toLower(); empty = no substring requirement
        QString extension;       // name.endsWith(extension)
    };

    /**
     * Return the list of asset-match rules for the *current* platform.
     * Each adapter declares its own rules inside the appropriate Q_OS_* block.
     * Returning an empty list falls back to the generic platform-keyword
     * heuristic in matchAsset().
     */
    virtual QVector<AssetMatchRule> assetMatchRules() const { return {}; }

    /**
     * Select the correct GitHub release asset for this platform.
     * Default implementation walks assetMatchRules() first, then falls back
     * to a generic "name contains platform name + common archive extension"
     * heuristic. Override only for unusual logic — most adapters should
     * override assetMatchRules() instead.
     */
    virtual QString matchAsset(const QStringList& assetNames) const {
        const auto rules = assetMatchRules();
        for (const auto& name : assetNames) {
            const QString lower = name.toLower();
            for (const auto& rule : rules) {
                if (!name.endsWith(rule.extension)) continue;
                bool ok = true;
                for (const auto& s : rule.substrings) {
                    if (!lower.contains(s.toLower())) { ok = false; break; }
                }
                if (ok) return name;
            }
        }

        // Fallback: generic platform-keyword + common archive extension.
#if defined(Q_OS_MACOS)
        const QString platform = "mac";
#elif defined(Q_OS_WIN)
        const QString platform = "windows";
#else
        const QString platform = "linux";
#endif
        for (const auto& name : assetNames) {
            const QString lower = name.toLower();
            if (lower.contains(platform) &&
                (name.endsWith(".zip") || name.endsWith(".tar.xz") ||
                 name.endsWith(".dmg") || name.endsWith(".tar.gz"))) {
                return name;
            }
        }
        return {};
    }

    /**
     * Whether this emulator has built-in RetroAchievements support.
     */
    virtual bool supportsRetroAchievements() const { return false; }

    /**
     * Push RA login/preferences into the emulator. The base is a no-op —
     * the INI-patching default died with the process era (2026-07);
     * LibretroAdapter overrides this to persist RA prefs its own way.
     */
    virtual void patchRetroAchievements(const QString& username, const QString& token,
                                         bool enabled, bool hardcore,
                                         bool notifications, bool sounds) {
        Q_UNUSED(username); Q_UNUSED(token); Q_UNUSED(enabled);
        Q_UNUSED(hardcore); Q_UNUSED(notifications); Q_UNUSED(sounds);
    }

    /**
     * Format a captured SDL input event into the emulator's INI binding string.
     *
     * Per-emulator output format reference (DO NOT REMOVE — adding a new
     * emulator that silently inherits the wrong default will look correct in
     * the UI but produce non-functional bindings):
     *
     *   PCSX2     : "SDL-{idx}/FaceSouth"      buttons    (no + prefix)
     *               "SDL-{idx}/+LeftX"         axes       (always polarity prefixed)
     *               default below matches this convention
     *
     *   DuckStation: "SDL-{idx}/A"             buttons    (SDL names, bare)
     *                "SDL-{idx}/LeftX"          full axes (no polarity)
     *                "SDL-{idx}/+LeftTrigger"   trigger    (polarity for triggers only)
     *                overrides this method to differentiate trigger vs full axis
     *
     *   PPSSPP    : "{deviceId}-{NKCODE}"      buttons    (numeric only)
     *                e.g. "10-19" for d-pad up — uses NKCODE table not SDL names
     *                often needs dual bindings ("10-96,10-189") for fallback
     *                overrides this method completely; default is unused
     *
     * Default below produces the PCSX2 convention.
     */
    virtual QString formatBinding(int deviceIndex, const QString& element,
                                   bool isAxis, bool positive) const {
        if (isAxis) {
            QString prefix = positive ? "+" : "-";
            return QString("SDL-%1/%2%3").arg(deviceIndex).arg(prefix, element);
        }
        return QString("SDL-%1/%2").arg(deviceIndex).arg(element);
    }

    /**
     * Format a captured keyboard event into the emulator's INI binding string.
     * Default: PCSX2/DuckStation format "Keyboard/<Name>", with modifier prefixes
     * ("Keyboard/Shift & ", etc.) joined by " & ".
     * Override for emulators that use a different format (e.g. PPSSPP: "1-<nkcode>").
     * Return an empty string to indicate the captured key is unsupported and
     * should be discarded.
     */
    virtual QString formatKeyboardBinding(int qtKey, int modifiers) const {
        QString binding;
        if (modifiers & Qt::ShiftModifier)   binding  = "Keyboard/Shift & ";
        if (modifiers & Qt::ControlModifier) binding += "Keyboard/Control & ";
        if (modifiers & Qt::AltModifier)     binding += "Keyboard/Alt & ";
        binding += "Keyboard/" + qtKeyToPcsx2Name(qtKey);
        return binding;
    }

    /**
     * Format a captured mouse button into the emulator's INI binding string.
     * Default: PCSX2/DuckStation "Pointer-0/<ButtonName>".
     * Return empty to indicate the button is unsupported for this emulator.
     */
    virtual QString formatMouseBinding(int qtButton) const {
        switch (qtButton) {
            case Qt::RightButton:   return "Pointer-0/RightButton";
            case Qt::MiddleButton:  return "Pointer-0/MiddleButton";
            case Qt::BackButton:    return "Pointer-0/Button4";
            case Qt::ForwardButton: return "Pointer-0/Button5";
            default: return {};
        }
    }

    /**
     * Format a captured mouse wheel direction into the emulator's INI binding string.
     * @param direction 0=up, 1=down, 2=left, 3=right
     * Return empty to indicate the direction is unsupported for this emulator.
     */
    virtual QString formatWheelBinding(int direction) const {
        switch (direction) {
            case 0: return "Pointer-0/WheelUp";
            case 1: return "Pointer-0/WheelDown";
            case 2: return "Pointer-0/WheelLeft";
            case 3: return "Pointer-0/WheelRight";
            default: return {};
        }
    }


protected:
    /**
     * Helper: map a Qt::Key code to the PCSX2/DuckStation INI key name used
     * inside "Keyboard/<Name>" bindings. Letters/digits/function keys are
     * handled by range; named keys use the explicit map.
     */
    static QString qtKeyToPcsx2Name(int qtKey) {
        switch (qtKey) {
        case Qt::Key_Return:       return "Return";
        case Qt::Key_Enter:        return "Return";
        case Qt::Key_Escape:       return "Escape";
        case Qt::Key_Backspace:    return "Backspace";
        case Qt::Key_Tab:          return "Tab";
        case Qt::Key_Backtab:      return "Backtab";
        case Qt::Key_Space:        return "Space";
        case Qt::Key_Delete:       return "Delete";
        case Qt::Key_Period:       return "Period";
        case Qt::Key_Comma:        return "Comma";
        case Qt::Key_Slash:        return "Slash";
        case Qt::Key_Backslash:    return "Backslash";
        case Qt::Key_Semicolon:    return "Semicolon";
        case Qt::Key_Apostrophe:   return "Apostrophe";
        case Qt::Key_BracketLeft:  return "BracketLeft";
        case Qt::Key_BracketRight: return "BracketRight";
        case Qt::Key_Minus:        return "Minus";
        case Qt::Key_Equal:        return "Equal";
        case Qt::Key_QuoteLeft:    return "QuoteLeft";
        case Qt::Key_Up:           return "Up";
        case Qt::Key_Down:         return "Down";
        case Qt::Key_Left:         return "Left";
        case Qt::Key_Right:        return "Right";
        case Qt::Key_Insert:       return "Insert";
        case Qt::Key_Home:         return "Home";
        case Qt::Key_End:          return "End";
        case Qt::Key_PageUp:       return "PageUp";
        case Qt::Key_PageDown:     return "PageDown";
        case Qt::Key_CapsLock:     return "CapsLock";
        case Qt::Key_NumLock:      return "NumLock";
        case Qt::Key_ScrollLock:   return "ScrollLock";
        case Qt::Key_Pause:        return "Pause";
        default: break;
        }
        if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
            return QString("F%1").arg(qtKey - Qt::Key_F1 + 1);
        if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
            return QString::number(qtKey - Qt::Key_0);
        if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
            return QString(QChar(qtKey).toLower());
        return {};
    }

    /**
     * Helper: resolve a platform-specific executable inside an install directory.
     * Searches for .app bundles on macOS, appends .exe on Windows, and falls back
     * to the manifest executable name. @param fallbackName is the hardcoded binary
     * name to try inside bundles (e.g. "PCSX2", "DuckStation").
     */
    static QString resolveExecutableInDir(const EmulatorManifest& manifest,
                                          const QString& installPath,
                                          const QString& fallbackName) {
#if defined(Q_OS_MACOS)
        QDir dir(installPath);
        const auto entries = dir.entryList({"*.app"}, QDir::Dirs);
        for (const auto& entry : entries) {
            QString exec = installPath + "/" + entry + "/Contents/MacOS/" + fallbackName;
            if (QFileInfo::exists(exec)) return exec;

            QString bundleName = QFileInfo(entry).completeBaseName();
            exec = installPath + "/" + entry + "/Contents/MacOS/" + bundleName;
            if (QFileInfo::exists(exec)) return exec;
        }
        QString direct = installPath + "/" + manifest.executable;
        if (QFileInfo::exists(direct)) return direct;
        return installPath + "/" + fallbackName;
#elif defined(Q_OS_WIN)
        QString exeName = manifest.executable;
        if (!exeName.endsWith(".exe")) exeName += ".exe";
        return installPath + "/" + exeName;
#else
        return installPath + "/" + manifest.executable;
#endif
    }


};
