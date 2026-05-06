#include "ppsspp_settings_dialog.h"
#include "ppsspp_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"

PpssppSettingsDialog::PpssppSettingsDialog(AppController* app,
                                            const QString& emuId,
                                            QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("PPSSPP Settings", QSize(1000, 720),
                SettingsDialogTheme::windowBg());

    auto* hub = new PpssppCategoryHub(this);
    connect(hub, &PpssppCategoryHub::categoryActivated,
            this, &PpssppSettingsDialog::onCategoryActivated);
    connect(hub, &PpssppCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    setHub(hub);
}

void PpssppSettingsDialog::onCategoryActivated(const QString& category) {
    PPSSPPAdapter adapter;  // stateless; matches DolphinCategoryHub::countSettings's stack-local form

    QVector<SettingDef> slice;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), &adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &PpssppSettingsDialog::setFocusedSetting);

    // Graphics has two sub-tabs (General + On-Screen Display) — pass
    // hasSubTabs=true so the L1/R1 "switch tab" hint shows on the dialog
    // chrome (mirrors duckstation_settings_dialog.cpp). Every other
    // category is a single scrolling page upstream and stays that way.
    const bool hasSubTabs = (category == "Graphics");
    pushPage(page, hasSubTabs);
}
