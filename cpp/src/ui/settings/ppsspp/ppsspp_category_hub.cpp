#include "ppsspp_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/ppsspp_adapter.h"
#include <QGridLayout>

PpssppCategoryHub::PpssppCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("PPSSPP Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // Mirrors PPSSPP's standalone GameSettingsScreen tabs (minus Controls,
    // Tools, VR — see ppsspp_adapter.cpp::settingsSchema for rationale).
    // Row 0: Graphics · Audio
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Graphics",
                             "Backend, resolution, frame pacing, textures, overlays",
                             countSettings("Graphics"), "Graphics"),
                    0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, latency, volume mix, UI sounds",
                             countSettings("Audio"), "Audio"),
                    0, 1);

    // Row 1: Networking · System
    grid->addWidget(makeCard(QStringLiteral("\U0001F4F6"), "Networking",
                             "Wlan, ad hoc, infrastructure, UPnP, chat",
                             countSettings("Networking"), "Networking"),
                    1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F527"), "System",
                             "Memory stick, save states, cheats, PSP settings",
                             countSettings("System"), "System"),
                    1, 1);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);

    // Bottom-row cards forward Down → native button (base class default).
    const auto cards = findChildren<SettingsCard*>(QString(), Qt::FindDirectChildrenOnly);
    if (cards.size() >= 2) {
        cards[cards.size() - 2]->installEventFilter(this);
        cards[cards.size() - 1]->installEventFilter(this);
    }
}

int PpssppCategoryHub::countSettings(const QString& category) const {
    PPSSPPAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
