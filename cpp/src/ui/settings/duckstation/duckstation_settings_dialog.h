#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

class DuckStationCategoryHub;

class DuckStationSettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    DuckStationSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private:
    void onCategoryActivated(const QString& category);
};
