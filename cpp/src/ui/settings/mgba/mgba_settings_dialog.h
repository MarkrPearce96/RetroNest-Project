#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

class MgbaSettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    MgbaSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private:
    void onCategoryActivated(const QString& category);
};
