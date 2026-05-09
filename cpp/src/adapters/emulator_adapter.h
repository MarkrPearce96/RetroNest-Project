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
     * Return the in-process libretro options store for this adapter, or nullptr
     * if this adapter is not libretro-backed. Overridden by LibretroAdapter.
     * Used by GenericSettingsPage to read/write SettingDefs whose storage is
     * SettingDef::Storage::LibretroOption.
     */
    virtual class OptionsStore* libretroOptionsStore() { return nullptr; }

    /**
     * Return the frontend settings store for this adapter, or nullptr if not
     * a libretro-backed adapter. Overridden by LibretroAdapter.
     * Used by GenericSettingsPage to read/write SettingDefs whose storage is
     * SettingDef::Storage::FrontendSetting.
     */
    virtual class FrontendSettingsStore* frontendSettingsStore() { return nullptr; }

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
     * Carbon kVK_* virtual keycode that, when synthesized to the
     * running emulator's process, toggles its pause state. Used by
     * AppController to pause/resume the emulator natively when the
     * in-game menu opens/closes — clean audio (no SIGSTOP buffer-cut
     * click). Adapters that don't expose a "pause-only" hotkey return
     * 0; AppController falls back to SIGSTOP for those (audio
     * stutter, but still pauses correctly).
     *
     * The corresponding emulator config (its TogglePause / equivalent
     * hotkey) must be bound to this key in createDefaultConfig /
     * patchExistingConfig — otherwise the synthesized keystroke
     * reaches the emulator but does nothing.
     */
    virtual int pauseHotkeyVirtualKeyCode() const { return 0; }

    /**
     * Return the CLI arguments to resume from a save state.
     * Default: {"-statefile", stateFilePath} (PCSX2 convention).
     * Override for emulators that use a different flag (e.g. DuckStation uses "-resume").
     */
    virtual QStringList resumeLaunchArgs(const QString& stateFilePath) const {
        return {"-statefile", stateFilePath};
    }

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
     * Per-emulator INI key names used for RetroAchievements settings.
     * Each adapter declares its section + key names + bool formatting; the
     * base patchRetroAchievements() does the read/patch/write dance.
     *
     * An empty key (empty QString) means "this emulator doesn't expose this
     * setting" — the patch is skipped for that field.
     */
    struct RetroAchievementsKeyMap {
        QString section;             // e.g. "Achievements" or "Cheevos"
        QString enabledKey;          // e.g. "Enabled" or "AchievementsEnable"
        QString hardcoreKey;         // e.g. "HardcoreMode" or "ChallengeMode"
        QString notificationsKey;    // empty → skip
        QString soundEffectsKey;     // e.g. "SoundEffects"
        QString trueValue = "true";  // some emulators want "True" (title case)
        QString falseValue = "false";
        QString configTag;           // "PCSX2"/"DuckStation"/"PPSSPP" — for log messages
    };

    /**
     * Override to declare the RA key map. Default returns an empty map,
     * which causes patchRetroAchievements() to be a no-op.
     */
    virtual RetroAchievementsKeyMap retroAchievementsKeyMap() const { return {}; }

    /**
     * Patch RA credentials and settings into the emulator's config.
     * Default implementation uses retroAchievementsKeyMap() and configFilePath()
     * to do the read/patch/write. Override only for emulator-specific quirks
     * (e.g. credentials living in a separate file).
     */
    virtual void patchRetroAchievements(const QString& username, const QString& token,
                                         bool enabled, bool hardcore,
                                         bool notifications, bool sounds) {
        Q_UNUSED(username); Q_UNUSED(token);

        const auto map = retroAchievementsKeyMap();
        if (map.section.isEmpty()) return;

        const QString path = configFilePath();
        QString content;
        if (!readConfigFile(path, content, map.configTag.isEmpty() ? "Adapter" : map.configTag))
            return;

        const QString trueVal = map.trueValue;
        const QString falseVal = map.falseValue;
        QVector<IniKeyPatch> patches;
        if (!map.enabledKey.isEmpty())
            patches.append({map.section, map.enabledKey, enabled ? trueVal : falseVal});
        if (!map.hardcoreKey.isEmpty())
            patches.append({map.section, map.hardcoreKey, hardcore ? trueVal : falseVal});
        if (!map.notificationsKey.isEmpty())
            patches.append({map.section, map.notificationsKey, notifications ? trueVal : falseVal});
        if (!map.soundEffectsKey.isEmpty())
            patches.append({map.section, map.soundEffectsKey, sounds ? trueVal : falseVal});

        if (patchIniKeys(content, patches))
            writeConfigFile(path, content, map.configTag.isEmpty() ? "Adapter" : map.configTag);
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

    /**
     * Adapter-specific launch arguments to prepend to every spawn of the
     * emulator binary, regardless of whether a game is being launched or
     * the user is opening the native settings UI. Default empty.
     *
     * Used by emulators that need a CLI flag to point at a custom config
     * location — e.g. Dolphin on macOS uses `-u <user-dir>` because
     * portable.txt-inside-the-bundle would invalidate the code signature.
     */
    virtual QStringList additionalLaunchArgs() const { return {}; }

    /**
     * Build the final command-line arguments for launching a game.
     * Default implementation prepends additionalLaunchArgs(), then
     * substitutes {rom_path} in manifest launch_args.
     * Adapters can override for special logic.
     */
    virtual QStringList buildLaunchArgs(const EmulatorManifest& manifest,
                                        const QString& romPath) {
        QStringList args = additionalLaunchArgs();
        for (const auto& arg : manifest.launch_args) {
            QString resolved = arg;
            resolved.replace("{rom_path}", romPath);
            args.append(resolved);
        }
        return args;
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
     * Helper: convert SYSTEM.CNF serial format to emulator filename format.
     * "SLUS_200.62" → "SLUS-20062" (underscore → hyphen, remove dot).
     * Both PCSX2 and DuckStation use this format in save state filenames.
     */
    static QString serialToFilenameFormat(const QString& serial) {
        QString result = serial;
        result.replace('_', '-');
        result.remove('.');
        return result;
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

    /**
     * Helper: ensure a portable.txt marker exists in the given directory.
     * Creates the directory if needed. Returns true if marker exists or was created.
     */
    static bool ensurePortableMarker(const QString& dir, const QString& adapterName) {
        const QString portableMarker = dir + "/portable.txt";
        if (QFileInfo::exists(portableMarker))
            return true;
        if (!QDir().mkpath(dir)) {
            qWarning() << "[" + adapterName + "] Failed to create directory:" << dir;
            return false;
        }
        QFile marker(portableMarker);
        if (marker.open(QIODevice::WriteOnly)) {
            marker.close();
            qInfo() << "[" + adapterName + "] Created portable.txt at" << portableMarker;
            return true;
        }
        qWarning() << "[" + adapterName + "] Failed to create portable.txt at" << portableMarker;
        return false;
    }

    /**
     * Helper: read a config file into a QString.
     * Returns true on success, false on failure (logs warning).
     */
    static bool readConfigFile(const QString& path, QString& outContent, const QString& adapterName) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "[" + adapterName + "] Cannot read config:" << path;
            return false;
        }
        outContent = QString::fromUtf8(file.readAll());
        file.close();
        return true;
    }

    /**
     * Helper: write config content to a file.
     * Creates parent directories if needed. Returns true on success.
     */
    static bool writeConfigFile(const QString& path, const QString& content, const QString& adapterName) {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            qWarning() << "[" + adapterName + "] Failed to create directory for:" << path;
            return false;
        }
        QFile outFile(path);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[" + adapterName + "] Failed to write config:" << path;
            return false;
        }
        outFile.write(content.toUtf8());
        qInfo() << "[" + adapterName + "] Wrote config at" << path;
        return true;
    }

    /**
     * Helper: suppress the emulator's first-run wizard in INI content.
     * Handles SetupWizardComplete and SetupWizardIncomplete keys.
     * @param section The INI section containing wizard keys (e.g. "UI", "Main")
     */
    static bool suppressSetupWizard(QString& content, const QString& section) {
        bool changed = false;
        if (content.contains("SetupWizardComplete = false")) {
            content.replace("SetupWizardComplete = false", "SetupWizardComplete = true");
            changed = true;
        }
        if (content.contains("SetupWizardIncomplete = true")) {
            content.replace("SetupWizardIncomplete = true", "SetupWizardIncomplete = false");
            changed = true;
        }
        if (!content.contains("SetupWizardComplete")) {
            const QString header = "[" + section + "]";
            if (content.contains(header))
                content.replace(header, header + "\nSetupWizardComplete = true");
            else
                content.prepend(header + "\nSetupWizardComplete = true\n");
            changed = true;
        }
        return changed;
    }

    /**
     * Helper: patch INI keys within their respective sections.
     * If the key exists, updates its value. If missing, appends to the section.
     * If the section is missing, appends both section and key.
     */
    struct IniKeyPatch {
        QString section;
        QString key;
        QString value;
    };

    static bool patchIniKeys(QString& content, const QVector<IniKeyPatch>& patches) {
        bool changed = false;
        for (const auto& p : patches) {
            const QString sectionHeader = "[" + p.section + "]";
            QRegularExpression keyRe("^" + QRegularExpression::escape(p.key) + "\\s*=\\s*.*$",
                                     QRegularExpression::MultilineOption);
            const QString newLine = p.key + " = " + p.value;

            auto match = keyRe.match(content);
            if (match.hasMatch()) {
                if (match.captured() != newLine) {
                    content.replace(keyRe, newLine);
                    changed = true;
                }
            } else if (content.contains(sectionHeader)) {
                content.replace(sectionHeader, sectionHeader + "\n" + newLine);
                changed = true;
            } else {
                content.append("\n" + sectionHeader + "\n" + newLine + "\n");
                changed = true;
            }
        }
        return changed;
    }
};
