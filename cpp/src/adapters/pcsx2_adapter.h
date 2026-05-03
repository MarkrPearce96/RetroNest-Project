#pragma once

#include "emulator_adapter.h"

/**
 * PCSX2Adapter — handles PCSX2-specific config patching and executable resolution.
 *
 * Ported from src/main/pcsx2-config.ts and src/main/standalone-launcher.ts.
 * - Suppresses first-run wizard (SetupWizardComplete = true)
 * - Sets folder paths (BIOS, saves, cache, etc.)
 * - Enables fullscreen, removes saved window geometry
 * - Enables SDL input sources
 * - Handles macOS .app bundle resolution
 * - Supports per-game config overrides
 */
class PCSX2Adapter : public EmulatorAdapter {
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
    QVector<SettingDef> controllerSettingDefs() const override;
    QVector<BindingDef> controllerBindingDefs() const override;
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> controllerSettingDefsForType(const QString& type) const override;
    bool supportsRetroAchievements() const override { return true; }
    bool supportsSaveOnExit() const override { return true; }
    RetroAchievementsKeyMap retroAchievementsKeyMap() const override;
    QVector<AssetMatchRule> assetMatchRules() const override;
    QString findResumeFile(const QString& serial) const override;

private:
    /** Get the platform-specific PCSX2 config directory. */
    static QString configDir();

    /** Get the full path to PCSX2.ini. */
    static QString iniPath();

    /** Write a fresh config with recommended defaults. Returns true on success. */
    bool createDefaultConfig(const QString& path,
                             const QString& biosPath,
                             const QString& savesPath);

    /** Patch an existing config to ensure required settings. Returns true on success. */
    bool patchExistingConfig(const QString& path,
                             const QString& biosPath,
                             const QString& savesPath);
};
