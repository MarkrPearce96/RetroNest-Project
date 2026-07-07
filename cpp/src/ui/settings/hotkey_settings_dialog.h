#pragma once

#include "ui/settings/emulator_settings_dialog_base.h"
#include "core/binding_def.h"

#include <QPointer>

class GenericHotkeyPage;
class LibretroHotkeyController;
class SdlInputManager;
class AppController;

// Hotkey settings dialog. Extends EmulatorSettingsDialogBase to inherit
// the warm-grey + amber palette, history-stack chrome, and bottom
// SettingsDescriptionBar. The body is a single GenericHotkeyPage; the
// stack is single-entry (no hub) — hotkey data fits on one scrollable
// page, like the controller-mapping view.
class HotkeySettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    HotkeySettingsDialog(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          QWidget* parent = nullptr);
    ~HotkeySettingsDialog() override;

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;

private slots:
    void onBindingFocused(HotkeyDef def, QString currentDisplay);
    void onRestoreDefaultsClicked();

private:
    GenericHotkeyPage* m_page = nullptr;
    SdlInputManager* m_inputManager;
    // Suppression hold released in the destructor (WA_DeleteOnClose).
    // QPointer: the dialog can outlive AppController at app teardown.
    QPointer<LibretroHotkeyController> m_suppressedHotkeys;
};
