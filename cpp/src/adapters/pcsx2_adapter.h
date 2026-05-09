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
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    // Carbon kVK_* keys for in-game menu actions. The corresponding
    // PCSX2 hotkeys are force-bound to these keys in createDefaultConfig
    // + patchExistingConfig; "Save State To Selected Slot",
    // "Load State From Selected Slot" and "Toggle Turbo / Fast Forward"
    // are deliberately omitted from hotkeyBindingDefs() so the user
    // can't rebind the keys our synthesis depends on.
    int hotkeyVirtualKeyCode(HotkeyAction action) const override {
        switch (action) {
        case HotkeyAction::TogglePause:       return 0x31; // kVK_Space
        case HotkeyAction::SaveState:         return 0x60; // kVK_F5
        case HotkeyAction::LoadState:         return 0x62; // kVK_F7
        case HotkeyAction::ToggleFastForward: return 0x64; // kVK_F8
        }
        return 0;
    }
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> controllerSettingDefsForType(const QString& type) const override;
    bool supportsRetroAchievements() const override { return true; }
    bool supportsSaveOnExit() const override { return true; }
    RetroAchievementsKeyMap retroAchievementsKeyMap() const override;
    QVector<AssetMatchRule> assetMatchRules() const override;
    QString findResumeFile(const QString& serial) const override;
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
    QString subcategoryIcon(const QString& category,
                            const QString& subcategory) const override;

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
