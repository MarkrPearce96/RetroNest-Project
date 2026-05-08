#include "mgba_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/libretro/mgba_libretro_adapter.h"
#include <QGridLayout>

MgbaCategoryHub::MgbaCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("mGBA Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // Row 0: System · Video · Audio
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "System",
                             "BIOS, Game Boy model",
                             countSettings("System"), "System"),       0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Video",
                             "Color correction, blending, palettes",
                             countSettings("Video"), "Video"),         0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Low-pass filter",
                             countSettings("Audio"), "Audio"),         0, 2);

    // Row 1: Input · Emulation
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Input",
                             "Solar sensor, GBP rumble, opposing input",
                             countSettings("Input"), "Input"),         1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U000026A1"), "Emulation",
                             "Idle-loop removal, frameskip",
                             countSettings("Emulation"), "Emulation"), 1, 1);

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
