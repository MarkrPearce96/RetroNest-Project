#include "duckstation_category_hub.h"
#include "ui/settings/widgets/settings_card.h"  // makeCard() returns SettingsCard*
#include "adapters/duckstation_adapter.h"
#include <QGridLayout>

// Hub layout mirrors the upstream SettingsWindow tab list
// (settingswindow.cpp:92-184) in left-to-right reading order, with three
// structural folds vs upstream:
//   - Recommended (curated cross-cut at the top — Dolphin/PCSX2 pattern).
//   - On-Screen Display lives under Graphics as a sub-tab (Dolphin
//     pattern), not as a top-level card.
//   - Capture also lives under Graphics as a sub-tab — capture is
//     graphics-adjacent (screenshots + video output) so the unified
//     Graphics dialog stays clean.
// Other omitted panes (Interface, Game List, BIOS, Post-Processing,
// Debugging) are documented in duckstation-schema-alignment.md.
DuckStationCategoryHub::DuckStationCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("DuckStation Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // Row 0: Recommended — full-width stretch card. Highlighted at the top
    // because it's the curated short list of settings users most commonly
    // tweak (sourced from DuckStation's setup docs + community consensus).
    // Same INI keys as the full panes below, just collected for fast access.
    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "Most-tweaked settings — performance, visuals, audio",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);  // spans all 3 columns

    // Row 1: Console · Emulation · Memory Cards
    grid->addWidget(makeCard(QStringLiteral("\U0001F39B"), "Console",
                             "Region, fast boot, CPU, CD-ROM",
                             countSettings("Console"), "Console"),         1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed, latency, rewind, runahead",
                             countSettings("Emulation"), "Emulation"),     1, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards",
                             "Slots and card types",
                             countSettings("Memory Cards"), "Memory Cards"), 1, 2);

    // Row 2: Graphics · Audio · Achievements
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Graphics",
                             "Renderer, advanced, textures, OSD, capture",
                             countSettings("Graphics"), "Graphics"),       2, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, latency, volume",
                             countSettings("Audio"), "Audio"),             2, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3C6"), "Achievements",
                             "RetroAchievements options",
                             countSettings("Achievements"), "Achievements"), 2, 2);

    // Row 3: Advanced — full-width stretch row.
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
