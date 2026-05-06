#include "emulator_settings_dialog_base.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_description_bar.h"
#include "ui/app_controller.h"
#include "core/sdl_input_manager.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QShowEvent>
#include <QTimer>

EmulatorSettingsDialogBase::EmulatorSettingsDialogBase(AppController* app,
                                                       const QString& emuId,
                                                       QWidget* parent)
    : QDialog(parent), m_app(app), m_emuId(emuId) {
    if (m_app)
        m_app->beginSettingsSession(m_emuId);
}

EmulatorSettingsDialogBase::~EmulatorSettingsDialogBase() {
    if (m_app)
        m_app->endSettingsSession(m_emuId);
}

void EmulatorSettingsDialogBase::setupChrome(const QString& title,
                                             const QSize& minSize,
                                             const QColor& windowBg) {
    setWindowTitle(title);
    setMinimumSize(minSize);
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(windowBg.name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_descBar = new SettingsDescriptionBar(this);
    if (m_app)
        m_descBar->setInputManager(m_app->sdlInputManager());

    root->addWidget(m_stack, 1);
    root->addWidget(m_descBar, 0);
}

void EmulatorSettingsDialogBase::setHub(QWidget* hub) {
    m_hub = hub;
    m_stack->addWidget(m_hub);

    // On the hub: hints only (no description text). On sub-pages: both.
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int index) {
        bool onHub = (m_stack->widget(index) == m_hub);
        m_descBar->setDescriptionVisible(!onHub);
        if (onHub) m_descBar->clear();
        applyHintsForCurrentPage();
    });

    m_descBar->setDescriptionVisible(false);
    applyHintsForCurrentPage();
}

void EmulatorSettingsDialogBase::pushPage(QWidget* page, bool hasSubTabs) {
    m_currentPageHasSubTabs = hasSubTabs;
    int idx = m_stack->addWidget(page);
    m_history.push(m_stack->currentIndex());
    m_stack->setCurrentIndex(idx);
    clearFocusedSetting();

    // Auto-focus the first focusable SettingsCard so arrow keys work
    // immediately. Skip NoFocus cards (compound containers).
    for (auto* card : page->findChildren<SettingsCard*>()) {
        if (card->focusPolicy() != Qt::NoFocus) {
            card->setFocus(Qt::OtherFocusReason);
            break;
        }
    }
}

void EmulatorSettingsDialogBase::popPage() {
    if (m_history.isEmpty()) { accept(); return; }
    QWidget* current = m_stack->currentWidget();
    int prev = m_history.pop();
    m_stack->setCurrentIndex(prev);
    m_currentPageHasSubTabs = false;
    if (current && current != m_hub) {
        m_stack->removeWidget(current);
        current->deleteLater();
    }
    clearFocusedSetting();
}

void EmulatorSettingsDialogBase::setFocusedSetting(const SettingDef& def) {
    m_descBar->setSetting(def);
}

void EmulatorSettingsDialogBase::clearFocusedSetting() {
    m_descBar->clear();
}

void EmulatorSettingsDialogBase::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    // Auto-focus the first focusable card on the hub when the dialog opens
    // so keyboard navigation works without a mouse-click priming. Mirrors
    // pushPage's per-page auto-focus. Deferred via singleShot(0, ...) so
    // the focus lands AFTER Qt's own showEvent focus shuffling — without
    // the defer the dialog itself steals focus back on the same tick.
    if (m_stack && m_stack->currentWidget() == m_hub && m_hub) {
        QWidget* hub = m_hub;
        QTimer::singleShot(0, this, [hub]{
            for (auto* card : hub->findChildren<SettingsCard*>()) {
                if (card->focusPolicy() != Qt::NoFocus) {
                    card->setFocus(Qt::OtherFocusReason);
                    break;
                }
            }
        });
    }
}

void EmulatorSettingsDialogBase::keyPressEvent(QKeyEvent* e) {
    // Escape and B-button (Key_Back) both act as hierarchical back.
    // On the hub, popPage() calls accept() which closes the dialog.
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Back) {
        popPage();
        return;
    }
    // Suppress Tab/Backtab on pages without sub-tabs so L1/R1 don't
    // accidentally move widget focus.
    if ((e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) &&
        !m_currentPageHasSubTabs && m_stack->currentWidget() != m_hub) {
        e->accept();
        return;
    }
    QDialog::keyPressEvent(e);
}

void EmulatorSettingsDialogBase::applyHintsForCurrentPage() {
    using BH = SettingsDescriptionBar::ButtonHint;

    if (m_stack->currentWidget() == m_hub) {
        m_descBar->setHints({
            BH{"navigate_ud", "Navigate"},
            BH{"confirm",     "Select"},
            BH{"back",        "Close"},
        });
    } else if (m_currentPageHasSubTabs) {
        m_descBar->setHints({
            BH{"navigate",    "Navigate"},
            BH{"confirm",     "Select"},
            BH{"switch_tab",  "Switch Tab"},
            BH{"back",        "Back"},
        });
    } else {
        m_descBar->setHints({
            BH{"navigate",    "Navigate"},
            BH{"confirm",     "Select"},
            BH{"back",        "Back"},
        });
    }
}
