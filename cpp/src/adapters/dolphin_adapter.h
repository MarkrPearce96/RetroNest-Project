#pragma once

#include "emulator_adapter.h"

/**
 * DolphinAdapter — adapter for Dolphin (GameCube + Wii).
 *
 * Dolphin spreads its config across multiple INI files under User/Config/:
 *   - Dolphin.ini       (Interface, Display, Core, General — the "main" file)
 *   - GFX.ini           (graphics — resolution + aspect live here)
 *   - GCPadNew.ini      (GameCube controller bindings)
 *   - WiimoteNew.ini    (Wii Remote bindings)
 *   - Hotkeys.ini       (native hotkeys; we clear conflicting ones)
 *   - RetroAchievements.ini (RA settings)
 *
 * configFilePath() returns the path to Dolphin.ini. The settings UI exposes
 * only Dolphin.ini settings in v1 (graphics page deferred to native UI).
 * Resolution and aspect ratio are routed to GFX.ini via the framework's
 * ResolutionOptions::iniFilePath / IniPatch::iniFilePath overrides.
 *
 * Controllers: default profiles are baked into GCPadNew.ini and WiimoteNew.ini
 * at install time (create-only — never overwritten on subsequent launches).
 * controllerTypes() returns empty so Dolphin does not appear in the in-app
 * controller mapping page; users remap through Dolphin's native UI.
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
    QString configFilePath() const override;

    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
    ResolutionOptions resolutionOptions() const override;
    AspectRatioOptions aspectRatioOptions() const override;

    // Controllers: empty (no in-app remap UI for v1) — defaults baked into INIs.
    QVector<ControllerTypeDef> controllerTypes() const override { return {}; }
    QVector<BindingDef> controllerBindingDefs() const override { return {}; }

    QVector<HotkeyDef> hotkeyBindingDefs() const override { return {}; }

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

private:
    /** On macOS: path inside .app bundle (Contents/MacOS/). Otherwise: emulators dir. */
    static QString portableDir();

    /** Absolute path to {portableDir}/User/Config/. */
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
