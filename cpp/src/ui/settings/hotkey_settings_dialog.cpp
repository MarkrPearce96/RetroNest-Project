#include "hotkey_settings_dialog.h"
#include "ui/settings/generic_hotkey_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/settings_description_bar.h"
#include "ui/app_controller.h"

#include <QKeyEvent>
#include <QShowEvent>
#include <QTimer>

HotkeySettingsDialog::HotkeySettingsDialog(SdlInputManager* inputManager,
                                            AppController* appController,
                                            const QString& emuId,
                                            QWidget* parent)
    : EmulatorSettingsDialogBase(appController, emuId, parent),
      m_inputManager(inputManager) {
    setupChrome(QStringLiteral("Hotkey Settings"),
                QSize(900, 720),
                SettingsDialogTheme::windowBg());

    m_page = new GenericHotkeyPage(inputManager, appController, emuId, this);
    connect(m_page, &GenericHotkeyPage::bindingFocused,
            this, &HotkeySettingsDialog::onBindingFocused);
    setHub(m_page);  // single-page dialog — page IS the hub

    // Footer hints match the controller mapping page's pill row:
    //   confirm → ↵ Rebind            (Enter / A button)
    //   clear   → ⌫ Restore Default   (Backspace / B button)
    //   close   → Esc Close            (Esc / X button)
    if (m_descBar) {
        m_descBar->setInputManager(inputManager);
        m_descBar->setHints({
            { QStringLiteral("confirm"), QStringLiteral("Rebind") },
            { QStringLiteral("clear"),   QStringLiteral("Restore Default") },
            { QStringLiteral("close"),   QStringLiteral("Close") },
        });
    }
}

void HotkeySettingsDialog::onBindingFocused(HotkeyDef def, QString currentDisplay) {
    if (m_descBar) m_descBar->setHotkey(def, currentDisplay);
}

void HotkeySettingsDialog::onRestoreDefaultsClicked() {
    if (m_page) m_page->restoreDefaults();
}

void HotkeySettingsDialog::keyPressEvent(QKeyEvent* e) {
    if (!m_page) {
        EmulatorSettingsDialogBase::keyPressEvent(e);
        return;
    }
    // Footer-action shortcuts. SdlInputManager translates A/B/X face buttons
    // to these Qt keys via the unified-input pipeline (see CLAUDE.md
    // "Input System"). Keyboard analogues mirror the controller mapping
    // page's footer pills.
    switch (e->key()) {
        case Qt::Key_Return:                            // ↵ / A — Rebind
            m_page->rebindFocused();
            return;
        case Qt::Key_Backspace:                         // ⌫ / B — Restore Default
        case Qt::Key_Back:
            m_page->restoreFocusedToDefault();
            return;
        case Qt::Key_Escape:                            // Esc / X — Close
            accept();
            return;
        default:
            break;
    }
    EmulatorSettingsDialogBase::keyPressEvent(e);
}

void HotkeySettingsDialog::showEvent(QShowEvent* e) {
    EmulatorSettingsDialogBase::showEvent(e);
    // Auto-focus the first hotkey row so keyboard / controller navigation
    // works immediately. Deferred via singleShot(0, ...) for the same
    // reason as the base class's own auto-focus: Qt's showEvent does its
    // own focus shuffling on the same tick.
    GenericHotkeyPage* page = m_page;
    if (page) QTimer::singleShot(0, page, [page]{ page->focusFirstRow(); });
}
