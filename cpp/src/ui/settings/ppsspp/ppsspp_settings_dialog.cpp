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
    setupChrome("PPSSPP Settings", QSize(950, 550),
                SettingsDialogTheme::windowBg(), QSize(1000, 700));

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

    // No PPSSPP category renders sub-tabs (upstream uses a single scrolling
    // ItemHeader page per tab), so hasSubTabs is always false.
    pushPage(page, /*hasSubTabs=*/false);
}
