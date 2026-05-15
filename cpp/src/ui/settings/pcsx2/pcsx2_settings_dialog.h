#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

// Per-emulator settings dialog for PCSX2 (libretro-backed).
// Mirrors MgbaSettingsDialog — sets up chrome, attaches the
// category hub, and routes category clicks to a GenericSettingsPage
// rendering the matching SettingDef rows from the long-lived adapter.
class Pcsx2SettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private:
    void onCategoryActivated(const QString& category);
};
