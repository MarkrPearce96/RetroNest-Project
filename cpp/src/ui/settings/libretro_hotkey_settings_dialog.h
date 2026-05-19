#pragma once

#include "ui/settings/emulator_settings_dialog_base.h"
#include "core/binding_def.h"

class GenericHotkeyPage;
class SdlInputManager;
class AppController;

// Global Libretro Hotkeys settings dialog.  Extends EmulatorSettingsDialogBase
// to inherit the warm-grey + amber palette, history-stack chrome, and bottom
// SettingsDescriptionBar.  Identical to HotkeySettingsDialog but hardcodes
// libretro_hotkeys::kSentinelEmuId — no per-emulator emuId arg.
class LibretroHotkeySettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    LibretroHotkeySettingsDialog(SdlInputManager* inputManager,
                                  AppController* appController,
                                  QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;

private slots:
    void onBindingFocused(HotkeyDef def, QString currentDisplay);
    void onRestoreDefaultsClicked();

private:
    GenericHotkeyPage* m_page = nullptr;
    SdlInputManager* m_inputManager;
};
