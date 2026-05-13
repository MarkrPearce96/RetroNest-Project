#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

// SP7b: per-emulator settings dialog for pcsx2-libretro.
// Mirrors MgbaSettingsDialog — sets up chrome, attaches the
// category hub, and routes category clicks to a GenericSettingsPage
// rendering the matching SettingDef rows from the long-lived adapter.
class Pcsx2LibretroSettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    Pcsx2LibretroSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private:
    void onCategoryActivated(const QString& category);
};
