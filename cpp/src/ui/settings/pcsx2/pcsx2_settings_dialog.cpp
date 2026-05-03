#include "pcsx2_settings_dialog.h"
#include "pcsx2_category_hub.h"
#include "pages/pcsx2_emulation_page.h"
#include "pages/pcsx2_audio_page.h"
#include "pages/pcsx2_memory_cards_page.h"
#include "pages/pcsx2_graphics_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"

Pcsx2SettingsDialog::Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("PCSX2 Settings", QSize(950, 550), SettingsDialogTheme::windowBg(), QSize(1000, 700));

    auto* hub = new Pcsx2CategoryHub(this);
    connect(hub, &Pcsx2CategoryHub::categoryActivated,
            this, &Pcsx2SettingsDialog::onCategoryActivated);
    connect(hub, &Pcsx2CategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    setHub(hub);
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
        connect(page, &Pcsx2GraphicsPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page, /*hasSubTabs=*/true);
        return;
    }
}
