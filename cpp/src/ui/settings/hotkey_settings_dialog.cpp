#include "hotkey_settings_dialog.h"
#include "ui/settings/generic_hotkey_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/settings_description_bar.h"
#include "ui/app_controller.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

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

    // Face-button hints in the description bar.
    if (m_descBar) {
        m_descBar->setInputManager(inputManager);
        m_descBar->setHints({
            { QStringLiteral("confirm"),     QStringLiteral("Rebind") },
            { QStringLiteral("back"),        QStringLiteral("Clear")  },
            { QStringLiteral("navigate_ud"), QStringLiteral("Navigate") },
            { QStringLiteral("switch_tab"),  QStringLiteral("Add") },
        });
    }

    // Restore Defaults button — top-left of the description bar, above
    // the painted face-button hints row.
    auto* restore = new QPushButton(QStringLiteral("Restore Defaults"), this);
    restore->setCursor(Qt::PointingHandCursor);
    restore->setFocusPolicy(Qt::NoFocus);  // don't steal focus from hotkey rows
    restore->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:4px; padding:6px 14px; font-weight:600; }"
        "QPushButton:hover { background:%4; }"
        "QPushButton:focus { border-color:%4; }")
        .arg(SettingsDialogTheme::titleBarBg().name(),
             SettingsDialogTheme::textPrimary().name(),
             SettingsDialogTheme::cardBorder().name(),
             SettingsDialogTheme::accent().name()));
    connect(restore, &QPushButton::clicked, this,
            &HotkeySettingsDialog::onRestoreDefaultsClicked);

    if (m_descBar) {
        if (auto* descLayout = qobject_cast<QHBoxLayout*>(m_descBar->layout())) {
            descLayout->insertWidget(0, restore, 0, Qt::AlignTop | Qt::AlignLeft);
        }
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
    // Face-button shortcuts. SdlInputManager translates A/B/X/Y to these
    // Qt keys via the unified-input pipeline (see CLAUDE.md "Input System").
    // Square (X) closes the dialog — matches controller_mapping_page where
    // Backspace is the page-level close shortcut.
    switch (e->key()) {
        case Qt::Key_Return:                            // A — Rebind
            m_page->rebindFocused();
            return;
        case Qt::Key_Back:                              // B — Clear
            m_page->clearFocused();
            return;
        case Qt::Key_M:                                 // Y — Add (alternate binding)
            m_page->appendRebindFocused();
            return;
        case Qt::Key_Backspace:                         // X — Close
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
    // works immediately. The base class searches for SettingsCard children
    // (which the hotkey page doesn't use) — defer to the page so focus
    // lands on a HotkeyBindingRow. Deferred via singleShot(0, ...) for the
    // same reason as the base class's own auto-focus: Qt's showEvent does
    // its own focus shuffling on the same tick.
    GenericHotkeyPage* page = m_page;
    if (page) QTimer::singleShot(0, page, [page]{ page->focusFirstRow(); });
}
