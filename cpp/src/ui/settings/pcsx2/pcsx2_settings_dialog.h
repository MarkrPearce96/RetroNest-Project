#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

class Pcsx2CategoryHub;

class Pcsx2SettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private slots:
    void onCategoryActivated(const QString& category);
};
