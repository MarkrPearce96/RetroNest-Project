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
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
    QString subcategoryIcon(const QString& category,
                            const QString& subcategory) const override;
    QString configFilePath() const override;
    QString controllerBindingsConfigFilePath() const override;
    QString controllerBindingsSection(int port) const override;
    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
    ResolutionOptions resolutionOptions() const override;
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    // Synthesize Space → PPSSPP's "Pause (no menu)" virtual hotkey
    // (bound to keyboard Space / "1-62" in scrubControlsIniHotkeys).
    int pauseHotkeyVirtualKeyCode() const override { return 0x31 /* kVK_Space */; }
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> controllerSettingDefsForType(const QString& type) const override;
    bool supportsRetroAchievements() const override { return true; }
    RetroAchievementsKeyMap retroAchievementsKeyMap() const override;
    QVector<AssetMatchRule> assetMatchRules() const override;
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
    /**
     * Scrub any hotkey entries in controls.ini that aren't valid PPSSPP format
     * (deviceId-keyCode, optionally comma-separated alternates). Prevents the
     * PPSSPP parser from crashing on garbage left over from earlier buggy
     * captures (e.g. "Keyboard/d").
     */
    void scrubControlsIniHotkeys();
};
