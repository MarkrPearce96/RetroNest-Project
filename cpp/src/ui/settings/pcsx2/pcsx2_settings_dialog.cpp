#include "pcsx2_settings_dialog.h"
#include "pcsx2_category_hub.h"
#include "pages/pcsx2_emulation_page.h"
#include "pages/pcsx2_audio_page.h"
#include "pages/pcsx2_memory_cards_page.h"
#include "pages/pcsx2_graphics_page.h"
#include "widgets/pcsx2_card.h"
#include "widgets/pcsx2_description_bar.h"
#include "pcsx2_theme.h"
#include "ui/app_controller.h"
#include "core/sdl_input_manager.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>

Pcsx2SettingsDialog::Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : QDialog(parent), m_app(app), m_emuId(emuId) {
    setWindowTitle("PCSX2 Settings");
    setMinimumSize(950, 550);
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(Pcsx2Theme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_descBar = new Pcsx2DescriptionBar(this);
    m_descBar->setInputManager(app->sdlInputManager());

    m_hub = new Pcsx2CategoryHub(this);
    connect(m_hub, &Pcsx2CategoryHub::categoryActivated,
            this, &Pcsx2SettingsDialog::onCategoryActivated);
    connect(m_hub, &Pcsx2CategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    m_stack->addWidget(m_hub);

    // Always show description bar — it shows hints even on the hub.
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int index) {
        bool onHub = (m_stack->widget(index) == m_hub);
        m_descBar->setVisible(true);
        if (onHub) {
            m_descBar->clear();
        }
        applyHintsForCurrentPage();
    });

    root->addWidget(m_stack, 1);
    root->addWidget(m_descBar, 0);

    // Initial state: hub is active, show hub hints
    applyHintsForCurrentPage();
}

void Pcsx2SettingsDialog::pushPage(QWidget* page, bool hasSubTabs) {
    m_currentPageHasSubTabs = hasSubTabs;
    int idx = m_stack->addWidget(page);
    m_history.push(m_stack->currentIndex());
    m_stack->setCurrentIndex(idx);
    if (size().width() < 1000 || size().height() < 700) {
        resize(1000, 700);
    }
    clearFocusedSetting();

    // Auto-focus the first focusable Pcsx2Card so arrow keys work
    // immediately.  Skip NoFocus cards (compound containers).
    for (auto* card : page->findChildren<Pcsx2Card*>()) {
        if (card->focusPolicy() != Qt::NoFocus) {
            card->setFocus(Qt::OtherFocusReason);
            break;
        }
    }
}

void Pcsx2SettingsDialog::popPage() {
    if (m_history.isEmpty()) { accept(); return; }
    QWidget* current = m_stack->currentWidget();
    int prev = m_history.pop();
    m_stack->setCurrentIndex(prev);
    m_currentPageHasSubTabs = false;
    if (m_stack->currentWidget() == m_hub) {
        resize(950, 550);
    }
    if (current && current != m_hub) { m_stack->removeWidget(current); current->deleteLater(); }
    clearFocusedSetting();
}

void Pcsx2SettingsDialog::setFocusedSetting(const SettingDef& def) { m_descBar->setSetting(def); }
void Pcsx2SettingsDialog::clearFocusedSetting() { m_descBar->clear(); }

void Pcsx2SettingsDialog::keyPressEvent(QKeyEvent* e) {
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

void Pcsx2SettingsDialog::applyHintsForCurrentPage() {
    using BH = Pcsx2DescriptionBar::ButtonHint;

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

void Pcsx2SettingsDialog::onCategoryActivated(const QString& category) {
    if (category == "Emulation") {
        auto* page = new Pcsx2EmulationPage(this);
        connect(page, &Pcsx2EmulationPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Audio") {
        auto* page = new Pcsx2AudioPage(this);
        connect(page, &Pcsx2AudioPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Memory Cards") {
        auto* page = new Pcsx2MemoryCardsPage(this);
        connect(page, &Pcsx2MemoryCardsPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Graphics") {
        auto* page = new Pcsx2GraphicsPage(this);
        connect(page, &Pcsx2GraphicsPage::settingFocused,
                this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page, true);  // hasSubTabs = true
        return;
    }
    // Emulation / Audio / Memory Cards branches wired in Tasks 14-16.
}
