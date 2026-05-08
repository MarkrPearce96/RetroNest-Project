#include "mgba_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/libretro/mgba_libretro_adapter.h"
#include <QGridLayout>

MgbaCategoryHub::MgbaCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("mGBA Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // Row 0: Recommended — full-width curated card, mirrors the
    // Dolphin / PCSX2 / DuckStation / PPSSPP layout convention.
    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "Most-tweaked settings — performance, visuals, BIOS",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);

    // Row 1: System · Video · Audio
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "System",
                             "BIOS, Game Boy model",
                             countSettings("System"), "System"),       1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Video",
                             "Color correction, blending, palettes",
                             countSettings("Video"), "Video"),         1, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Low-pass filter",
                             countSettings("Audio"), "Audio"),         1, 2);

    // Row 2: Input · Emulation
    // Controller mapping lives on the dedicated "Controller Mapping" entry
    // surfaced from EmulatorDetailPage — not duplicated here.
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Input",
                             "Solar sensor, GBP rumble, opposing input",
                             countSettings("Input"), "Input"),         2, 0);
    grid->addWidget(makeCard(QStringLiteral("\U000026A1"), "Emulation",
                             "Idle-loop removal, frameskip",
                             countSettings("Emulation"), "Emulation"), 2, 1);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

int MgbaCategoryHub::countSettings(const QString& category) const {
    MgbaLibretroAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
