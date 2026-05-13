#include "pcsx2_libretro_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/libretro/pcsx2_libretro_adapter.h"
#include <QGridLayout>

Pcsx2LibretroCategoryHub::Pcsx2LibretroCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("PCSX2 (libretro) Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // SP7b currently exposes three knobs (renderer / MTVU / FastBoot)
    // all under the "Recommended" category. The full-width card mirrors
    // the layout convention of the other emulators' hubs.
    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "GS renderer, multi-threaded VU1, fast boot",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

int Pcsx2LibretroCategoryHub::countSettings(const QString& category) const {
    Pcsx2LibretroAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
