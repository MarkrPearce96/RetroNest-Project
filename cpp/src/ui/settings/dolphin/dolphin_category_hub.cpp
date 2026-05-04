#include "dolphin_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/dolphin_adapter.h"
#include <QGridLayout>

DolphinCategoryHub::DolphinCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("Dolphin Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    // Row 0: Interface · General · Audio
    grid->addWidget(makeCard(QStringLiteral("\U0001F39B"),  "Interface",
                             "Window, cursor, language, focus",
                             countSettings("Interface"), "Interface"),  0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "General",
                             "CPU, JIT, fastmem, MMU, clock",
                             countSettings("General"), "General"),      0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "DSP, backend, latency, volume",
                             countSettings("Audio"), "Audio"),          0, 2);
    // Row 1: Graphics · GameCube · Wii
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",
                             "Renderer, enhancements, hacks, OSD",
                             countSettings("Graphics"), "Graphics"),    1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "GameCube",
                             "Memcards, slot devices, system",
                             countSettings("GameCube"), "GameCube"),    1, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Wii",
                             "Wiimote source, BT, NAND, SD",
                             countSettings("Wii"), "Wii"),              1, 2);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

int DolphinCategoryHub::countSettings(const QString& category) const {
    DolphinAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
