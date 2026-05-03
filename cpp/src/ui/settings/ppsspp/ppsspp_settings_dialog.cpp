#include "ppsspp_settings_dialog.h"
#include "ppsspp_category_hub.h"
#include "ui/settings/settings_dialog_theme.h"
#include "pages/ppsspp_emulation_page.h"
#include "pages/ppsspp_audio_page.h"
#include "pages/ppsspp_overlay_page.h"
#include "pages/ppsspp_graphics_page.h"
#include "ui/app_controller.h"

PpssppSettingsDialog::PpssppSettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("PPSSPP Settings", QSize(950, 550), SettingsDialogTheme::windowBg(), QSize(1000, 700));

    auto* hub = new PpssppCategoryHub(this);
    connect(hub, &PpssppCategoryHub::categoryActivated,
            this, &PpssppSettingsDialog::onCategoryActivated);
    connect(hub, &PpssppCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    setHub(hub);
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
    if (category == "Graphics") {
        auto* page = new PpssppGraphicsPage(this);
        connect(page, &PpssppGraphicsPage::settingFocused, this, &PpssppSettingsDialog::setFocusedSetting);
        pushPage(page, /*hasSubTabs=*/true);
        return;
    }
}
