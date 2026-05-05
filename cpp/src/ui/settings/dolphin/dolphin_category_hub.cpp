#include "dolphin_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/dolphin_adapter.h"
#include <QGridLayout>

DolphinCategoryHub::DolphinCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("Dolphin Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // Row 0: Recommended — full-width stretch card. Highlighted at the
    // top of the hub because it's the curated short list of settings
    // users most commonly tweak (sourced from Dolphin's official
    // performance guide + community consensus). Same INI keys as the
    // full panes below, just collected for fast access.
    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "Most-tweaked settings — performance, visuals, audio",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);  // spans all 3 columns

    // Row 1: Interface · General · Audio
    grid->addWidget(makeCard(QStringLiteral("\U0001F39B"),  "Interface",
                             "Window, cursor, language, focus",
                             countSettings("Interface"), "Interface"),  1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "General",
                             "CPU, JIT, fastmem, MMU, clock",
                             countSettings("General"), "General"),      1, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "DSP, backend, latency, volume",
                             countSettings("Audio"), "Audio"),          1, 2);
    // Row 2: Graphics · GameCube · Wii
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",
                             "Renderer, enhancements, hacks, OSD",
                             countSettings("Graphics"), "Graphics"),    2, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "GameCube",
                             "Memcards, slot devices, system",
                             countSettings("GameCube"), "GameCube"),    2, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Wii",
                             "Wiimote source, BT, NAND, SD",
                             countSettings("Wii"), "Wii"),              2, 2);

    // Row 3: Advanced — full-width stretch card. Mirrors Dolphin's
    // Settings dialog Advanced tab (CPU options, timing, overclock,
    // memory override, custom RTC). Power-user knobs.
    grid->addWidget(makeCard(QStringLiteral("\U0001F527"), "Advanced",
                             "CPU, timing, overclock, memory, RTC",
                             countSettings("Advanced"), "Advanced"),
                    3, 0, 1, 3);

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
