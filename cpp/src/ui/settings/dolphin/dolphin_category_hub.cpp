#include "dolphin_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/dolphin_adapter.h"
#include <QGridLayout>

DolphinCategoryHub::DolphinCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("Dolphin Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    grid->addWidget(makeCard(QStringLiteral("\U0001F39B"),  "Interface",
                             "Pause, cursor, focus",
                             countSettings("Interface"), "Interface"),  0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Core",
                             "CPU, IPL, overclock",
                             countSettings("Core"), "Core"),            0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",
                             "Resolution, AA, VSync",
                             countSettings("Graphics"), "Graphics"),    1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, volume, JIT",
                             countSettings("Audio"), "Audio"),          1, 1);

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
