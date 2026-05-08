#include "hotkey_settings_dialog.h"
#include "ui/settings/generic_hotkey_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/settings_description_bar.h"
#include "ui/app_controller.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
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

    // Restore Defaults button — left-aligned in the description bar
    // footer, opposite the painted face hints.
    auto* restore = new QPushButton(QStringLiteral("Restore Defaults"), this);
    restore->setCursor(Qt::PointingHandCursor);
    restore->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:4px; padding:6px 14px; }"
        "QPushButton:focus { border-color:%4; }")
        .arg(SettingsDialogTheme::titleBarBg().name(),
             SettingsDialogTheme::textPrimary().name(),
             SettingsDialogTheme::cardBorder().name(),
             SettingsDialogTheme::accent().name()));
    connect(restore, &QPushButton::clicked, this,
            &HotkeySettingsDialog::onRestoreDefaultsClicked);

    if (m_descBar) {
        if (auto* descLayout = qobject_cast<QHBoxLayout*>(m_descBar->layout())) {
            descLayout->insertWidget(0, restore, 0, Qt::AlignBottom);
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
        default:
            break;
    }
    EmulatorSettingsDialogBase::keyPressEvent(e);
}
