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
    setupChrome(title, adapter && adapter->hasNativeSettingsUI());

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

    int maxRow = -1;
    QVector<SettingsCard*> bottomRowCards;
    for (const auto& spec : cards) {
        auto* card = makeCard(spec.icon, spec.title, spec.descriptor,
                              counts.value(spec.categoryKey, 0), spec.categoryKey);
        grid->addWidget(card, spec.row, spec.col, spec.rowSpan, spec.colSpan);

        const int bottom = spec.row + spec.rowSpan - 1;
        if (bottom > maxRow) {
            maxRow = bottom;
            bottomRowCards.clear();
            bottomRowCards.append(card);
        } else if (bottom == maxRow) {
            bottomRowCards.append(card);
        }
    }

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);

    // Bottom-row cards forward Down → native button (base eventFilter
    // handles the redirect). PPSSPP needed this previously because Qt's
    // default focus-next from a card lands on the next card, not on the
    // button below the grid; installing the filter on bottom-row cards
    // fixes that. Skip when there's no native button (libretro adapters).
    if (nativeBtn()) {
        for (auto* card : bottomRowCards)
            card->installEventFilter(this);
    }
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
    connect(hub, &GenericCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
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
