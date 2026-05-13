#include "pcsx2_libretro_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/libretro/pcsx2_libretro_adapter.h"
#include <QGridLayout>

Pcsx2LibretroCategoryHub::Pcsx2LibretroCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("PCSX2 (libretro) Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // SP7b's three knobs (renderer / MTVU / FastBoot) sit under
    // category="Recommended"; SP7c Phase 1 added 15 rows under
    // category="Emulation"; SP7c Phase 2 adds 5 rows under
    // category="Audio". Phase 3 (Memory Cards) + Phase 5 (full hub
    // reorg per the spec) will add the remaining cards.
    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "GS renderer, multi-threaded VU1, fast boot",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);

    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed control, system, frame pacing",
                             countSettings("Emulation"), "Emulation"),
                    1, 0);

    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Volume, mute, buffer, sync mode",
                             countSettings("Audio"), "Audio"),
                    1, 1);

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
