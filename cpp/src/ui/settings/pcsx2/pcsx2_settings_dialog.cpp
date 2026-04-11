#include "pcsx2_settings_dialog.h"
#include "pcsx2_category_hub.h"
#include "pages/pcsx2_emulation_page.h"
#include "widgets/pcsx2_description_bar.h"
#include "pcsx2_theme.h"
#include "ui/settings/emulator_settings_page.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>

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

    m_hub = new Pcsx2CategoryHub(this);
    connect(m_hub, &Pcsx2CategoryHub::categoryActivated,
            this, &Pcsx2SettingsDialog::onCategoryActivated);
    m_stack->addWidget(m_hub);

    root->addWidget(m_stack, 1);
    root->addWidget(m_descBar, 0);
}

void Pcsx2SettingsDialog::pushPage(QWidget* page) {
    int idx = m_stack->addWidget(page);
    m_history.push(m_stack->currentIndex());
    m_stack->setCurrentIndex(idx);
    clearFocusedSetting();
}

void Pcsx2SettingsDialog::popPage() {
    if (m_history.isEmpty()) { accept(); return; }
    QWidget* current = m_stack->currentWidget();
    int prev = m_history.pop();
    m_stack->setCurrentIndex(prev);
    if (current && current != m_hub) { m_stack->removeWidget(current); current->deleteLater(); }
    clearFocusedSetting();
}

void Pcsx2SettingsDialog::setFocusedSetting(const SettingDef& def) { m_descBar->setSetting(def); }
void Pcsx2SettingsDialog::clearFocusedSetting() { m_descBar->clear(); }

void Pcsx2SettingsDialog::onCategoryActivated(const QString& category) {
    if (category == "Emulation") {
        auto* page = new Pcsx2EmulationPage(this);
        connect(page, &Pcsx2EmulationPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Graphics") {
        // Plan 1 fallback; replaced by Pcsx2GraphicsPage in a later plan.
        auto* legacy = new EmulatorSettingsPage(m_app, m_emuId);
        legacy->setAttribute(Qt::WA_DeleteOnClose);
        legacy->setWindowModality(Qt::ApplicationModal);
        legacy->show();
    }
    // Emulation / Audio / Memory Cards branches wired in Tasks 14-16.
}
