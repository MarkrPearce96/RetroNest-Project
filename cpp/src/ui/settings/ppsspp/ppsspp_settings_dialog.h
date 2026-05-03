#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

class PpssppCategoryHub;

class PpssppSettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    PpssppSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private slots:
    void onCategoryActivated(const QString& category);
};
