#include "mgba_settings_dialog.h"
#include "mgba_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/adapter_registry.h"
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
    // CRITICAL: use the long-lived registered adapter, NOT a stack-local one.
    // The page holds the adapter pointer and dereferences it at write time
    // (libretroOptionsStore() / frontendSettingsStore()). A stack-local
    // adapter would die at end of this function leaving a dangling pointer.
    auto* adapter = AdapterRegistry::instance().adapterFor("mgba");
    if (!adapter) return;

    QVector<SettingDef> slice;
    for (const auto& d : adapter->settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &MgbaSettingsDialog::setFocusedSetting);

    pushPage(page, /*hasSubTabs=*/false);
}
