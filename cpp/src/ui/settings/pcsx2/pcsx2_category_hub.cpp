#include "pcsx2_category_hub.h"
#include "ui/settings/widgets/settings_card.h"  // makeCard's QWidget* return needs the complete type
#include "adapters/pcsx2_adapter.h"
#include <QGridLayout>

// Top-level hub for the schema-driven PCSX2 settings dialog. The card grid
// matches PCSX2 standalone's pane list (SettingsWindow.cpp), minus categories
// RetroNest manages or that are per-game-only upstream — see
// PCSX2Adapter::settingsSchema for the full skip list.
Pcsx2CategoryHub::Pcsx2CategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("PCSX2 Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // Row 0: Recommended — full-width headline. Curated short list of the
    // settings users most commonly tweak, sourced from PCSX2's official
    // wiki + community consensus. Same INI keys as the full panes below;
    // the Recommended card is a fast-access view.
    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "Most-tweaked settings — performance, visuals, audio",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);

    // Row 1: Emulation · Graphics · Audio
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed control, system, frame pacing",
                             countSettings("Emulation"), "Emulation"),       1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Graphics",
                             "Renderer, hacks, post-processing, OSD",
                             countSettings("Graphics"), "Graphics"),         1, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, latency, volume",
                             countSettings("Audio"), "Audio"),               1, 2);

    // Row 2: Memory Cards · Network & HDD · Achievements
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards",
                             "Slot enables, multitap",
                             countSettings("Memory Cards"), "Memory Cards"), 2, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F310"), "Network & HDD",
                             "Ethernet, DHCP, DNS, internal HDD",
                             countSettings("Network & HDD"),
                             "Network & HDD"),                               2, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3C6"), "Achievements",
                             "Hardcore, notifications, overlays",
                             countSettings("Achievements"), "Achievements"), 2, 2);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

int Pcsx2CategoryHub::countSettings(const QString& category) const {
    PCSX2Adapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
