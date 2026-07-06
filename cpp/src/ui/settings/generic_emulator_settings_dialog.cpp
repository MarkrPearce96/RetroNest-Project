#include "generic_emulator_settings_dialog.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/app_controller.h"
#include "adapters/emulator_adapter.h"

#include <QGridLayout>

GenericCategoryHub::GenericCategoryHub(const QString& title,
                                       const EmulatorAdapter* adapter,
                                       QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome(title);

    // Build per-category setting counts in one pass — cheaper than calling
    // settingsSchema() once per card on a 200+ entry schema (PCSX2/DuckStation).
    QHash<QString, int> counts;
    if (adapter) {
        for (const auto& def : adapter->settingsSchema())
            counts[def.category] += 1;
    }

    const auto cards = adapter ? adapter->settingsHubCards() : QVector<SettingsHubCard>{};

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    for (const auto& spec : cards) {
        auto* card = makeCard(spec.icon, spec.title, spec.descriptor,
                              counts.value(spec.categoryKey, 0), spec.categoryKey);
        grid->addWidget(card, spec.row, spec.col, spec.rowSpan, spec.colSpan);
    }

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

GenericEmulatorSettingsDialog::GenericEmulatorSettingsDialog(
    AppController* app,
    const QString& emuId,
    const QString& displayName,
    EmulatorAdapter* adapter,
    QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent), m_adapter(adapter) {

    setupChrome(displayName + " Settings", QSize(1000, 720),
                SettingsDialogTheme::windowBg());

    const auto subTabList = adapter ? adapter->settingsCategoriesWithSubTabs()
                                    : QStringList{};
    for (const auto& cat : subTabList)
        m_subTabCategories.insert(cat);

    auto* hub = new GenericCategoryHub(displayName + " Settings", adapter, this);
    connect(hub, &GenericCategoryHub::categoryActivated,
            this, &GenericEmulatorSettingsDialog::onCategoryActivated);
    setHub(hub);
}

void GenericEmulatorSettingsDialog::onCategoryActivated(const QString& category) {
    if (!m_adapter) return;

    // CRITICAL: the registered adapter is long-lived. GenericSettingsPage
    // holds the pointer and dereferences it at write time
    // (libretroOptionsStore() / frontendSettingsStore()). The dialog
    // always receives the registry-owned adapter from AppController,
    // so the pointer stays valid for the page's lifetime.
    QVector<SettingDef> slice;
    for (const auto& d : m_adapter->settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), m_adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &GenericEmulatorSettingsDialog::setFocusedSetting);

    pushPage(page, m_subTabCategories.contains(category));
}
