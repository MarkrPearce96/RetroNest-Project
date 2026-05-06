#include "duckstation_settings_dialog.h"
#include "duckstation_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/duckstation_adapter.h"

DuckStationSettingsDialog::DuckStationSettingsDialog(AppController* app,
                                                     const QString& emuId,
                                                     QWidget* parent)
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
    DuckStationAdapter adapter;  // stateless; matches DolphinSettingsDialog::onCategoryActivated

    QVector<SettingDef> slice;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), &adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &DuckStationSettingsDialog::setFocusedSetting);

    // Graphics has 4 sub-tabs (Rendering / Advanced / PGXP / Texture
    // Replacement) — pass hasSubTabs=true so L1/R1 hints show on chrome.
    const bool hasSubTabs = (category == "Graphics");
    pushPage(page, hasSubTabs);
}
