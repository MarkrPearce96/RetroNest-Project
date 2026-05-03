#include "duckstation_settings_dialog.h"
#include "pages/duckstation_audio_page.h"
#include "pages/duckstation_memory_cards_page.h"
#include "pages/duckstation_console_page.h"
#include "pages/duckstation_emulation_page.h"
#include "pages/duckstation_graphics_page.h"
#include "duckstation_category_hub.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"

DuckStationSettingsDialog::DuckStationSettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("DuckStation Settings", QSize(1000, 720), SettingsDialogTheme::windowBg());

    auto* hub = new DuckStationCategoryHub(this);
    connect(hub, &DuckStationCategoryHub::categoryActivated,
            this, &DuckStationSettingsDialog::onCategoryActivated);
    connect(hub, &DuckStationCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    setHub(hub);
}

void DuckStationSettingsDialog::onCategoryActivated(const QString& category) {
    if (category == "Audio") {
        auto* page = new DuckStationAudioPage(this);
        connect(page, &DuckStationAudioPage::settingFocused,
                this, &DuckStationSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Console") {
        auto* page = new DuckStationConsolePage(this);
        connect(page, &DuckStationConsolePage::settingFocused,
                this, &DuckStationSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Emulation") {
        auto* page = new DuckStationEmulationPage(this);
        connect(page, &DuckStationEmulationPage::settingFocused,
                this, &DuckStationSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Graphics") {
        auto* page = new DuckStationGraphicsPage(this);
        connect(page, &DuckStationGraphicsPage::settingFocused,
                this, &DuckStationSettingsDialog::setFocusedSetting);
        pushPage(page, /*hasSubTabs=*/true);
        return;
    }
    if (category == "Memory Cards") {
        auto* page = new DuckStationMemoryCardsPage(this);
        connect(page, &DuckStationMemoryCardsPage::settingFocused,
                this, &DuckStationSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
}
