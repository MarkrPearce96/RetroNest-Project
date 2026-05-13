#include "pcsx2_libretro_settings_dialog.h"
#include "pcsx2_libretro_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/adapter_registry.h"
#include "adapters/libretro/pcsx2_libretro_adapter.h"

Pcsx2LibretroSettingsDialog::Pcsx2LibretroSettingsDialog(AppController* app,
                                                          const QString& emuId,
                                                          QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("PCSX2 (libretro) Settings", QSize(1000, 720), SettingsDialogTheme::windowBg());

    auto* hub = new Pcsx2LibretroCategoryHub(this);
    connect(hub, &Pcsx2LibretroCategoryHub::categoryActivated,
            this, &Pcsx2LibretroSettingsDialog::onCategoryActivated);
    setHub(hub);
}

void Pcsx2LibretroSettingsDialog::onCategoryActivated(const QString& category) {
    // Controller card: delegate to AppController which opens the shared
    // ControllerMappingPage (works for any emulator with controllerTypes()).
    if (category == QStringLiteral("__controller__")) {
        m_app->showControllerMapping(m_emuId);
        return;
    }

    // CRITICAL: use the long-lived registered adapter, NOT a stack-local one.
    // GenericSettingsPage holds the adapter pointer and dereferences it at
    // write time (libretroOptionsStore()). A stack-local adapter would die
    // at end of this function leaving a dangling pointer — same risk mGBA's
    // dialog calls out at mgba_settings_dialog.cpp:29-32.
    auto* adapter = AdapterRegistry::instance().adapterFor("pcsx2-libretro");
    if (!adapter) return;

    QVector<SettingDef> slice;
    for (const auto& d : adapter->settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &Pcsx2LibretroSettingsDialog::setFocusedSetting);

    pushPage(page, /*hasSubTabs=*/false);
}
