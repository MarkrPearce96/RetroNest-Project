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
    QString configFilePath() const override;
    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
    ResolutionOptions resolutionOptions() const override;
    AspectRatioOptions aspectRatioOptions() const override;
    QVector<BindingDef> controllerBindingDefs() const override;
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> controllerSettingDefsForType(const QString& type) const override;
    QStringList resumeLaunchArgs(const QString& stateFilePath) const override;
    QString formatBinding(int deviceIndex, const QString& element,
                          bool isAxis, bool positive) const override;
    bool supportsRetroAchievements() const override { return true; }
    bool supportsSaveOnExit() const override { return true; }
    QString matchAsset(const QStringList& assetNames) const override;
    void patchRetroAchievements(const QString& username, const QString& token,
                                 bool enabled, bool hardcore,
                                 bool notifications, bool sounds) override;
    QString findResumeFile(const QString& serial, const QString& savesRoot) const override;

private:
    /** On macOS: path inside .app bundle (Contents/MacOS/). Otherwise: emulators dir. */
    static QString portableDir();

    /** Write a fresh config with our defaults. Returns true on success. */
    bool createDefaultConfig(const QString& path,
                             const QString& biosPath,
                             const QString& savesPath);

    /** Patch an existing config to ensure required settings. Returns true on success. */
    bool patchExistingConfig(const QString& path,
                             const QString& biosPath,
                             const QString& savesPath);
};
