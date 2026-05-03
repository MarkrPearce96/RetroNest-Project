#include "ppsspp_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include <QGridLayout>

PpssppCategoryHub::PpssppCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("PPSSPP Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "CPU clock, fast memory, I/O timing, real clock sync", 5, "Emulation"),
                    0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Graphics",
                             "Backend, resolution, frame pacing, textures, post-FX", 30, "Graphics"),
                    0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, latency, volume mix, UI sounds", 12, "Audio"),
                    1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F4CA"), "Overlay",
                             "FPS counter, speed, battery, debug overlay", 4, "Overlay"),
                    1, 1);
    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);

    // Install filter on bottom-row cards so Down → native button works.
    const auto cards = findChildren<SettingsCard*>(QString(), Qt::FindDirectChildrenOnly);
    if (cards.size() >= 2) {
        cards[cards.size() - 2]->installEventFilter(this);
        cards[cards.size() - 1]->installEventFilter(this);
    }
}
