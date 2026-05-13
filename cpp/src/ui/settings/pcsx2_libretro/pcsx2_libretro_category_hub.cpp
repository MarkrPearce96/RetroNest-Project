#include "pcsx2_libretro_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/libretro/pcsx2_libretro_adapter.h"
#include <QGridLayout>

Pcsx2LibretroCategoryHub::Pcsx2LibretroCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("PCSX2 (libretro) Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    // SP7b's three knobs (renderer / MTVU / FastBoot) sit under
    // category="Recommended"; SP7c Phase 1 added 15 rows under
    // category="Emulation"; SP7c Phase 2 added 5 rows under
    // category="Audio"; SP7c Phase 3 added 5 rows under category="Memory
    // Cards". SP7c Phase 4 Task 2 added 16 rows under category="Graphics"
    // (subcategory="Display"); Tasks 3-6 will add the remaining 4
    // Graphics sub-tabs. Phase 5 (full hub reorg) will reconcile the
    // Recommended labels with the standalone PCSX2 dialog.
    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "GS renderer, multi-threaded VU1, fast boot",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);

    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed control, system, frame pacing",
                             countSettings("Emulation"), "Emulation"),
                    1, 0);

    // 🎨 palette glyph matches the standalone Pcsx2Adapter's Rendering
    // sub-tab icon (see Pcsx2Adapter::subcategoryIcon) — clearer than
    // 🖥️ (Display sub-tab) at the card level because the Graphics card
    // covers all five sub-tabs, not just Display.
    grid->addWidget(makeCard(QStringLiteral("\U0001F3A8"), "Graphics",
                             "Aspect ratio, upscaling, post-FX, OSD, textures",
                             countSettings("Graphics"), "Graphics"),
                    1, 1);

    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Volume, mute, buffer, sync mode",
                             countSettings("Audio"), "Audio"),
                    1, 2);

    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards",
                             "Slot 1/2 enables, Multitap slots",
                             countSettings("Memory Cards"), "Memory Cards"),
                    2, 0);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

int Pcsx2LibretroCategoryHub::countSettings(const QString& category) const {
    Pcsx2LibretroAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
