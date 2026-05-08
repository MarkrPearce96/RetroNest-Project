#include "mgba_settings_dialog.h"
#include "mgba_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/libretro/mgba_libretro_adapter.h"

MgbaSettingsDialog::MgbaSettingsDialog(AppController* app,
                                       const QString& emuId,
                                       QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("mGBA Settings", QSize(1000, 720), SettingsDialogTheme::windowBg());

    auto* hub = new MgbaCategoryHub(this);
    connect(hub, &MgbaCategoryHub::categoryActivated,
            this, &MgbaSettingsDialog::onCategoryActivated);
    setHub(hub);
}

void MgbaSettingsDialog::onCategoryActivated(const QString& category) {
    MgbaLibretroAdapter adapter;

    QVector<SettingDef> slice;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), &adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &MgbaSettingsDialog::setFocusedSetting);

    pushPage(page, /*hasSubTabs=*/false);
}
