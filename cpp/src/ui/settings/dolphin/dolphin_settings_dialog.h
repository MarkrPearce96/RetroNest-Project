#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

class DolphinSettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    DolphinSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private:
    void onCategoryActivated(const QString& category);
};
