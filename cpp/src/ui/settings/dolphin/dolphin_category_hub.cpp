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

    // Row 1: General · Audio · Graphics
    // Interface is omitted — Dolphin's Interface settings only affect
    // Dolphin's own UI (window chrome, language, library covers), which
    // RetroNest hides entirely. The two embedding-critical Interface
    // keys (PauseOnFocusLost, ConfirmStop) are force-patched by the
    // adapter at config time.
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "General",
                             "Boot, cheats, speed, region, Discord",
                             countSettings("General"), "General"),      1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "DSP, backend, latency, volume",
                             countSettings("Audio"), "Audio"),          1, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",
                             "Renderer, enhancements, hacks, OSD",
                             countSettings("Graphics"), "Graphics"),    1, 2);

    // Row 2: GameCube · Wii · Advanced
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "GameCube",
                             "Memcards, slot devices, system",
                             countSettings("GameCube"), "GameCube"),    2, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Wii",
                             "Wiimote source, BT, NAND, SD",
                             countSettings("Wii"), "Wii"),              2, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F527"), "Advanced",
                             "CPU, timing, overclock, memory, RTC",
                             countSettings("Advanced"), "Advanced"),    2, 2);

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
