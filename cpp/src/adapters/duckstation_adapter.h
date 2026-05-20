#pragma once

#include "emulator_adapter.h"

/**
 * DuckStationAdapter — handles DuckStation-specific config patching
 * and executable resolution.
 *
 * Ported from src/main/duckstation-config.ts and src/main/standalone-launcher.ts.
 * - Suppresses setup wizard (SetupWizardComplete = true)
 * - Sets folder paths (BIOS, memcards, savestates, etc.)
 * - Enables fullscreen, disables confirm-on-close
 * - Handles macOS .app bundle resolution
 */
class DuckStationAdapter : public EmulatorAdapter {
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
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> controllerSettingDefsForType(const QString& type) const override;
    QStringList resumeLaunchArgs(const QString& stateFilePath) const override;
    QString formatBinding(int deviceIndex, const QString& element,
                          bool isAxis, bool positive) const override;
    bool supportsRetroAchievements() const override { return true; }
    bool supportsSaveOnExit() const override { return true; }
    QVector<AssetMatchRule> assetMatchRules() const override;
    RetroAchievementsKeyMap retroAchievementsKeyMap() const override;
    QString findResumeFile(const QString& serial) const override;

private:
    /** On macOS: path inside .app bundle (Contents/MacOS/). Otherwise: emulators dir. */
    static QString portableDir();
};
