#include "pcsx2_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include <QGridLayout>

Pcsx2CategoryHub::Pcsx2CategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("PCSX2 Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",    "Speed control, system settings, frame pacing", 16, "Emulation"),    0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",     "Renderer, display, post-processing, OSD",       35, "Graphics"),     0, 1);
    auto* audioCard = makeCard(QStringLiteral("\U0001F50A"), "Audio",        "Backend, latency, volume",                      11, "Audio");
    auto* memCard   = makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards", "Memory cards and multitap slots",                7, "Memory Cards");
    audioCard->installEventFilter(this);
    memCard->installEventFilter(this);
    grid->addWidget(audioCard, 1, 0);
    grid->addWidget(memCard,   1, 1);
    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}
