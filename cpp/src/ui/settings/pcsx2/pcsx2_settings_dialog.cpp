#include "pcsx2_settings_dialog.h"
#include "pcsx2_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"

Pcsx2SettingsDialog::Pcsx2SettingsDialog(AppController* app,
                                          const QString& emuId,
                                          QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("PCSX2 Settings", QSize(1000, 720), SettingsDialogTheme::windowBg());

    auto* hub = new Pcsx2CategoryHub(this);
    connect(hub, &Pcsx2CategoryHub::categoryActivated,
            this, &Pcsx2SettingsDialog::onCategoryActivated);
    connect(hub, &Pcsx2CategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    setHub(hub);
}

void Pcsx2SettingsDialog::onCategoryActivated(const QString& category) {
    PCSX2Adapter adapter;  // stateless; matches Pcsx2CategoryHub::countSettings.

    QVector<SettingDef> slice;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), &adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &Pcsx2SettingsDialog::setFocusedSetting);

    // Graphics is the only category with sub-tabs; show the L1/R1 hint on the
    // dialog chrome there.
    const bool hasSubTabs = (category == "Graphics");
    pushPage(page, hasSubTabs);
}
