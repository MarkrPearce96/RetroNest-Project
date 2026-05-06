#include "duckstation_category_hub.h"
#include "ui/settings/widgets/settings_card.h"  // makeCard() returns SettingsCard*
#include "adapters/duckstation_adapter.h"
#include <QGridLayout>

// Hub layout mirrors the upstream SettingsWindow tab list
// (settingswindow.cpp:92-184) in left-to-right reading order, omitting
// Interface / Game List / Post-Processing / Debugging panes per
// duckstation-schema-alignment.md (in user memory).
DuckStationCategoryHub::DuckStationCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("DuckStation Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // Row 0: BIOS · Console · Emulation
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BB"), "BIOS",
                             "Region BIOS, parallel port",
                             countSettings("BIOS"), "BIOS"),               0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F39B"), "Console",
                             "Region, fast boot, CPU, CD-ROM",
                             countSettings("Console"), "Console"),         0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed, latency, rewind, runahead",
                             countSettings("Emulation"), "Emulation"),     0, 2);

    // Row 1: Memory Cards · Graphics · On-Screen Display
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards",
                             "Slots and card types",
                             countSettings("Memory Cards"), "Memory Cards"), 1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Graphics",
                             "Renderer, advanced, PGXP, textures",
                             countSettings("Graphics"), "Graphics"),       1, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F4CA"), "On-Screen Display",
                             "OSD, messages, overlays",
                             countSettings("On-Screen Display"),
                             "On-Screen Display"),                          1, 2);

    // Row 2: Audio · Achievements · Capture
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, latency, volume",
                             countSettings("Audio"), "Audio"),             2, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3C6"), "Achievements",
                             "RetroAchievements options",
                             countSettings("Achievements"), "Achievements"), 2, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3A5"), "Capture",
                             "Screenshots and media capture",
                             countSettings("Capture"), "Capture"),         2, 2);

    // Row 3: Advanced (full width — small category, gets the stretch row)
    grid->addWidget(makeCard(QStringLiteral("\U0001F527"), "Advanced",
                             "Logging",
                             countSettings("Advanced"), "Advanced"),       3, 0, 1, 3);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

int DuckStationCategoryHub::countSettings(const QString& category) const {
    DuckStationAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
