#include "ppsspp_settings_dialog.h"
#include "ppsspp_category_hub.h"
#include "../pcsx2/widgets/pcsx2_card.h"
#include "../pcsx2/widgets/pcsx2_description_bar.h"
#include "ppsspp_theme.h"
#include "pages/ppsspp_emulation_page.h"
#include "pages/ppsspp_audio_page.h"
#include "pages/ppsspp_overlay_page.h"
#include "ui/app_controller.h"
#include "core/sdl_input_manager.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QKeyEvent>

PpssppSettingsDialog::PpssppSettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : QDialog(parent), m_app(app), m_emuId(emuId) {
    setWindowTitle("PPSSPP Settings");
    setMinimumSize(950, 550);
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(PpssppTheme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_descBar = new Pcsx2DescriptionBar(this);
    m_descBar->setInputManager(app->sdlInputManager());

    m_hub = new PpssppCategoryHub(this);
    connect(m_hub, &PpssppCategoryHub::categoryActivated,
            this, &PpssppSettingsDialog::onCategoryActivated);
    connect(m_hub, &PpssppCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    m_stack->addWidget(m_hub);

    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int index) {
        bool onHub = (m_stack->widget(index) == m_hub);
        m_descBar->setDescriptionVisible(!onHub);
        if (onHub) m_descBar->clear();
        applyHintsForCurrentPage();
    });

    root->addWidget(m_stack, 1);
    root->addWidget(m_descBar, 0);

    m_descBar->setDescriptionVisible(false);
    applyHintsForCurrentPage();
}

void PpssppSettingsDialog::pushPage(QWidget* page, bool hasSubTabs) {
    m_currentPageHasSubTabs = hasSubTabs;
    int idx = m_stack->addWidget(page);
    m_history.push(m_stack->currentIndex());
    m_stack->setCurrentIndex(idx);
    if (size().width() < 1000 || size().height() < 700) resize(1000, 700);
    clearFocusedSetting();

    for (auto* card : page->findChildren<Pcsx2Card*>()) {
        if (card->focusPolicy() != Qt::NoFocus) {
            card->setFocus(Qt::OtherFocusReason);
            break;
        }
    }
}

void PpssppSettingsDialog::popPage() {
    if (m_history.isEmpty()) { accept(); return; }
    QWidget* current = m_stack->currentWidget();
    int prev = m_history.pop();
    m_stack->setCurrentIndex(prev);
    m_currentPageHasSubTabs = false;
    if (m_stack->currentWidget() == m_hub) resize(950, 550);
    if (current && current != m_hub) { m_stack->removeWidget(current); current->deleteLater(); }
    clearFocusedSetting();
}

void PpssppSettingsDialog::setFocusedSetting(const SettingDef& def) { m_descBar->setSetting(def); }
void PpssppSettingsDialog::clearFocusedSetting() { m_descBar->clear(); }

void PpssppSettingsDialog::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Back) { popPage(); return; }
    if ((e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) &&
        !m_currentPageHasSubTabs && m_stack->currentWidget() != m_hub) {
        e->accept();
        return;
    }
    QDialog::keyPressEvent(e);
}

void PpssppSettingsDialog::applyHintsForCurrentPage() {
    using BH = Pcsx2DescriptionBar::ButtonHint;
    if (m_stack->currentWidget() == m_hub) {
        m_descBar->setHints({
            BH{"navigate_ud", "Navigate"}, BH{"confirm", "Select"}, BH{"back", "Close"},
        });
    } else if (m_currentPageHasSubTabs) {
        m_descBar->setHints({
            BH{"navigate", "Navigate"}, BH{"confirm", "Select"},
            BH{"switch_tab", "Switch Tab"}, BH{"back", "Back"},
        });
    } else {
        m_descBar->setHints({
            BH{"navigate", "Navigate"}, BH{"confirm", "Select"}, BH{"back", "Back"},
        });
    }
}

void PpssppSettingsDialog::onCategoryActivated(const QString& category) {
    if (category == "Emulation") {
        auto* page = new PpssppEmulationPage(this);
        connect(page, &PpssppEmulationPage::settingFocused, this, &PpssppSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Audio") {
        auto* page = new PpssppAudioPage(this);
        connect(page, &PpssppAudioPage::settingFocused, this, &PpssppSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Overlay") {
        auto* page = new PpssppOverlayPage(this);
        connect(page, &PpssppOverlayPage::settingFocused, this, &PpssppSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    // Graphics branch wired in Task 13.
}
