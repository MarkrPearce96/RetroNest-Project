#include "duckstation_settings_dialog.h"
#include "duckstation_category_hub.h"
#include "duckstation_theme.h"
#include "../pcsx2/widgets/pcsx2_card.h"
#include "../pcsx2/widgets/pcsx2_description_bar.h"
#include "ui/app_controller.h"
#include "core/sdl_input_manager.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>

DuckStationSettingsDialog::DuckStationSettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : QDialog(parent), m_app(app), m_emuId(emuId) {
    setWindowTitle("DuckStation Settings");
    setMinimumSize(950, 550);
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(DuckStationTheme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_descBar = new Pcsx2DescriptionBar(this);
    m_descBar->setInputManager(app->sdlInputManager());

    m_hub = new DuckStationCategoryHub(this);
    connect(m_hub, &DuckStationCategoryHub::categoryActivated,
            this, &DuckStationSettingsDialog::onCategoryActivated);
    connect(m_hub, &DuckStationCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    m_stack->addWidget(m_hub);

    // On the hub: show hints only (no description text). On sub-pages: show both.
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int index) {
        bool onHub = (m_stack->widget(index) == m_hub);
        m_descBar->setDescriptionVisible(!onHub);
        if (onHub) {
            m_descBar->clear();
        }
        applyHintsForCurrentPage();
    });

    root->addWidget(m_stack, 1);
    root->addWidget(m_descBar, 0);

    // Initial state: hub is active, hints only
    m_descBar->setDescriptionVisible(false);
    applyHintsForCurrentPage();
}

void DuckStationSettingsDialog::pushPage(QWidget* page, bool hasSubTabs) {
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

void DuckStationSettingsDialog::popPage() {
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

void DuckStationSettingsDialog::setFocusedSetting(const SettingDef& def) { m_descBar->setSetting(def); }
void DuckStationSettingsDialog::clearFocusedSetting() { m_descBar->clear(); }

void DuckStationSettingsDialog::keyPressEvent(QKeyEvent* e) {
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

void DuckStationSettingsDialog::applyHintsForCurrentPage() {
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

void DuckStationSettingsDialog::onCategoryActivated(const QString& category) {
    (void)category;
}
