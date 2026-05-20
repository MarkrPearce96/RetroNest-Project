#pragma once

#include "emulator_adapter.h"

class IniFile;

/**
 * DolphinAdapter — adapter for Dolphin (GameCube + Wii).
 *
 * Dolphin spreads its config across multiple INI files under User/Config/:
 *   - Dolphin.ini       (Interface, Display, Core, General — the "main" file)
 *   - GFX.ini           (graphics — resolution + aspect live here)
 *   - GCPadNew.ini      (GameCube controller bindings)
 *   - WiimoteNew.ini    (Wii Remote / Classic Controller bindings)
 *   - Hotkeys.ini       (native hotkeys; we clear conflicting ones)
 *   - RetroAchievements.ini (RA settings)
 *
 * configFilePath() returns the path to Dolphin.ini. The settings UI exposes
 * only Dolphin.ini settings in v1 (graphics page deferred to native UI).
 * Resolution and aspect ratio are routed to GFX.ini via the framework's
 * ResolutionOptions::iniFilePath / IniPatch::iniFilePath overrides.
 *
 * Controllers — see the controllerTypes() doc comment below.
 *
 * In-game menu: pause-on-focus-loss only (no save-on-exit, no resume).
 */
class DolphinAdapter : public EmulatorAdapter {
public:
    bool ensureConfig(const EmulatorManifest& manifest,
                      const QString& biosPath,
                      const QString& savesPath) override;

    QString resolveExecutable(const EmulatorManifest& manifest,
                              const QString& installPath) override;

    QVector<SettingDef> settingsSchema() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    QStringList settingsCategoriesWithSubTabs() const override { return {"Graphics"}; }
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
    QString subcategoryIcon(const QString& category,
                            const QString& subcategory) const override;
    QString configFilePath() const override;

    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
    ResolutionOptions resolutionOptions() const override;
    AspectRatioOptions aspectRatioOptions() const override;

    /**
     * Controllers — Dolphin exposes two: GameCube + Wii Classic Controller.
     * Each routes bindings into its own INI file and section:
     *   GCPad1   → GCPadNew.ini   [GCPad1]    keys: D-Pad/, Buttons/, etc.
     *   Wiimote1 → WiimoteNew.ini [Wiimote1]  keys: Classic/D-Pad/, Classic/Buttons/, etc.
     * The Wii section is named "Wiimote1" because Dolphin reads Classic
     * Controller bindings from the Wiimote section when Extension=Classic.
     * The bare Wii Remote (motion + pointer) isn't exposed as a separate
     * type — most users play with a regular gamepad, so the Classic
     * extension is the practical default. Default profiles are baked at
     * install time (create-only, never overwritten); the in-app schema-
     * driven UI writes through to the same files from then on.
     */
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;

    QString controllerBindingsConfigFilePath(const QString& controllerTypeId) const override;
    QString controllerBindingsSection(int port, const QString& controllerTypeId) const override;

    QString formatBinding(int deviceIndex, const QString& element,
                          bool isAxis, bool positive) const override;

    void writeBindingDeviceHeader(IniFile& ini, const QString& section,
                                  int deviceIndex, QObject* input) const override;

    QVector<HotkeyDef> hotkeyBindingDefs() const override { return {}; }
    // Save/Load/FF are intentionally NOT exposed on Dolphin: Dolphin's
    // HotkeyScheduler polls CGEventSourceKeyState on a background-app
    // thread macOS aggressively throttles, so none of CGEventPostToPid
    // / CGEventPost(kCGHIDEventTap) / AppleScript reach it reliably.
    // Returning 0 hides the icons from the in-game menu via
    // currentGameInfo's supportsSaveState/supportsLoadState flags.
    // Pause defers to the EmulatorAdapter base default (Space) via the
    // switch's `default:` arm — works because PauseOnFocusLost handles
    // focus changes too; the synthesized Space is a no-op for Dolphin
    // but harmless.
    int hotkeyVirtualKeyCode(HotkeyAction action) const override {
        switch (action) {
        case HotkeyAction::SaveState:         return 0;
        case HotkeyAction::LoadState:         return 0;
        case HotkeyAction::ToggleFastForward: return 0;
        default:                              return EmulatorAdapter::hotkeyVirtualKeyCode(action);
        }
    }

    bool supportsRetroAchievements() const override { return true; }
    bool supportsSaveOnExit() const override { return false; }

    void patchRetroAchievements(const QString& username, const QString& token,
                                bool enabled, bool hardcore,
                                bool notifications, bool sounds) override;

    QVector<AssetMatchRule> assetMatchRules() const override;

    /**
     * Dolphin doesn't publish to GitHub Releases — they distribute from
     * dl.dolphin-emu.org. We resolve the latest stable tag (e.g. "2603a")
     * via the GitHub `/tags` API, then construct the .dmg URL directly.
     */
    DirectDownloadInfo resolveDirectDownload(const EmulatorManifest& manifest) const override;

    /**
     * Returns `["-u", userBaseDir()]` so Dolphin reads/writes its config from
     * our managed sibling-of-the-bundle directory. We can't use Dolphin's
     * portable.txt mechanism on macOS — placing portable.txt or a User/
     * directory inside the .app bundle invalidates the Developer ID code
     * signature ("a sealed resource is missing or invalid") and causes
     * Gatekeeper to refuse to launch the binary.
     */
    QStringList additionalLaunchArgs() const override;

private:
    /** Absolute path to {emulators-dir}/dolphin (the install root). */
    static QString installDir();

    /**
     * Absolute path to the user data root, located OUTSIDE the .app bundle
     * at {installDir}/User. Passed to Dolphin via `-u`.
     */
    static QString userBaseDir();

    /** Absolute path to {userBaseDir}/Config/. */
    static QString userConfigDir();

    /** Per-INI-file path helpers that resolve under userConfigDir(). */
    static QString dolphinIniPath();
    static QString gfxIniPath();
    static QString gcpadIniPath();
    static QString wiimoteIniPath();
    static QString hotkeysIniPath();
    static QString retroAchievementsIniPath();

    /** Patch Dolphin.ini with our embedding-critical keys. */
    bool patchDolphinIni(const QString& dataRootGc, const QString& dataRootWii);

    /** Patch GFX.ini with VSync and reasonable defaults. */
    bool patchGfxIni();

    /** Write GCPadNew.ini default profile if file does not yet exist. */
    bool writeGcPadDefaultsIfMissing();

    /** Write WiimoteNew.ini default profile if file does not yet exist. */
    bool writeWiimoteDefaultsIfMissing();

    /** Patch Hotkeys.ini to clear hotkeys that would conflict with our overlay. */
    bool patchHotkeysIni();
};
