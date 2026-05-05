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

    // Row 0: Emulation · Graphics · Audio
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed control, system, frame pacing",
                             countSettings("Emulation"), "Emulation"),       0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Graphics",
                             "Renderer, hacks, post-processing, OSD",
                             countSettings("Graphics"), "Graphics"),         0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, latency, volume",
                             countSettings("Audio"), "Audio"),               0, 2);

    // Row 1: Memory Cards · Network & HDD · Achievements
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards",
                             "Slot enables, multitap",
                             countSettings("Memory Cards"), "Memory Cards"), 1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F310"), "Network & HDD",
                             "Ethernet, DHCP, DNS, internal HDD",
                             countSettings("Network & HDD"),
                             "Network & HDD"),                               1, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3C6"), "Achievements",
                             "Hardcore, notifications, overlays",
                             countSettings("Achievements"), "Achievements"), 1, 2);

    // Row 2: Advanced — full-width
    grid->addWidget(makeCard(QStringLiteral("\U0001F527"), "Advanced",
                             "EE/VU/IOP, savestates, PINE",
                             countSettings("Advanced"), "Advanced"),
                    2, 0, 1, 3);

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
