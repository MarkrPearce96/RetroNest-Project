#pragma once

#include "emulator_adapter.h"

/**
 * PPSSPPAdapter — handles PPSSPP-specific config patching and executable resolution.
 *
 * Config lives in two files:
 * - ppsspp.ini — main settings (graphics, audio, system, paths)
 * - controls.ini — controller bindings (ControlMapping section)
 *
 * Binding format: d:{deviceIndex}/{button} (e.g., d:0/BUTTON_A, d:0/AXIS_X+)
 */
class PPSSPPAdapter : public EmulatorAdapter {
public:
    bool ensureConfig(const EmulatorManifest& manifest,
                      const QString& biosPath,
                      const QString& savesPath) override;

    QString resolveExecutable(const EmulatorManifest& manifest,
                              const QString& installPath) override;

    QVector<SettingDef> settingsSchema() const override;
    QString configFilePath() const override;
    QString controllerBindingsConfigFilePath() const override;
    QString controllerBindingsSection(int port) const override;
    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
    ResolutionOptions resolutionOptions() const override;
    QVector<BindingDef> controllerBindingDefs() const override;
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<SettingDef> controllerSettingDefs() const override;
    bool supportsRetroAchievements() const override { return true; }
    void patchRetroAchievements(const QString& username, const QString& token,
                                 bool enabled, bool hardcore,
                                 bool notifications, bool sounds) override;
    QString matchAsset(const QStringList& assetNames) const override;
    QString extractSerial(const QString& romPath) const override;
    // Note: PPSSPP does not support save-on-exit. It has no SaveStateOnShutdown
    // INI key, no SIGTERM handler that flushes state, and no auto-save slot.
    // We intentionally fall back to the base class's empty findResumeFile()
    // so the "Resume" feature is disabled for PPSSPP games.
    QString formatBinding(int deviceIndex, const QString& element,
                           bool isAxis, bool positive) const override;
    QString formatKeyboardBinding(int qtKey, int modifiers) const override;
    QString formatMouseBinding(int qtButton) const override;
    QString formatWheelBinding(int direction) const override;

private:
    static QString configDir();
    static QString nativeConfigDir();
    static QString iniPath();
    static QString controlsIniPath();

    bool createDefaultConfig(const QString& path,
                             const QString& biosPath,
                             const QString& savesPath);
    bool patchExistingConfig(const QString& path,
                             const QString& biosPath,
                             const QString& savesPath);
    bool syncToNativeConfig(const QString& mainIniPath);

    /**
     * Scrub any hotkey entries in controls.ini that aren't valid PPSSPP format
     * (deviceId-keyCode, optionally comma-separated alternates). Prevents the
     * PPSSPP parser from crashing on garbage left over from earlier buggy
     * captures (e.g. "Keyboard/d").
     */
    void scrubControlsIniHotkeys();
};
