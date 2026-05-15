#include "pcsx2_settings_dialog.h"
#include "pcsx2_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/adapter_registry.h"
#include "adapters/libretro/pcsx2_libretro_adapter.h"

Pcsx2SettingsDialog::Pcsx2SettingsDialog(AppController* app,
                                        const QString& emuId,
                                        QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("PCSX2 Settings", QSize(1000, 720), SettingsDialogTheme::windowBg());

    auto* hub = new Pcsx2CategoryHub(this);
    connect(hub, &Pcsx2CategoryHub::categoryActivated,
            this, &Pcsx2SettingsDialog::onCategoryActivated);
    setHub(hub);
}

void Pcsx2SettingsDialog::onCategoryActivated(const QString& category) {
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
    auto* adapter = AdapterRegistry::instance().adapterFor("pcsx2");
    if (!adapter) return;

    QVector<SettingDef> slice;
    for (const auto& d : adapter->settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &Pcsx2SettingsDialog::setFocusedSetting);

    // Graphics is the only category with multiple sub-tabs (Display /
    // Rendering / Texture Replacement / Post-Processing / On-Screen
    // Display). GenericSettingsPage auto-detects sub-tabs from the
    // distinct SettingDef::subcategory values on the page's rows;
    // hasSubTabs only controls the L1/R1 navigation hint chrome.
    const bool hasSubTabs = (category == "Graphics");
    pushPage(page, hasSubTabs);
}
