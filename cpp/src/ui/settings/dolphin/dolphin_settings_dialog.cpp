#include "dolphin_settings_dialog.h"
#include "dolphin_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/dolphin_adapter.h"

DolphinSettingsDialog::DolphinSettingsDialog(AppController* app,
                                              const QString& emuId,
                                              QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("Dolphin Settings", QSize(1000, 720), SettingsDialogTheme::windowBg());

    auto* hub = new DolphinCategoryHub(this);
    connect(hub, &DolphinCategoryHub::categoryActivated,
            this, &DolphinSettingsDialog::onCategoryActivated);
    connect(hub, &DolphinCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    setHub(hub);
}

void DolphinSettingsDialog::onCategoryActivated(const QString& category) {
    DolphinAdapter adapter;  // stateless; matches DolphinCategoryHub::countSettings's stack-local form

    QVector<SettingDef> slice;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), &adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &DolphinSettingsDialog::setFocusedSetting);

    // Graphics has sub-tabs (Display + Rendering) — pass hasSubTabs=true so
    // L1/R1 hint shows on the dialog chrome (mirrors duckstation_settings_dialog.cpp:49).
    const bool hasSubTabs = (category == "Graphics");
    pushPage(page, hasSubTabs);
}
