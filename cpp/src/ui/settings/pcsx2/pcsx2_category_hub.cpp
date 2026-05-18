#include "pcsx2_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/libretro/pcsx2_libretro_adapter.h"
#include <QGridLayout>

Pcsx2CategoryHub::Pcsx2CategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("PCSX2 Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);

    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "GS renderer, multi-threaded VU1, fast boot",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);

    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed control, system, frame pacing",
                             countSettings("Emulation"), "Emulation"),
                    1, 0);

    // 🎨 palette glyph chosen because the Graphics card covers all five
    // sub-tabs (Display / Rendering / Texture Replacement / Post-Processing
    // / On-Screen Display), not just Display — clearer than 🖥️ at card level.
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

    grid->addWidget(makeCard(QStringLiteral("\U0001F9E9"), "Patches",
                             "Widescreen / no-interlacing / game-fixes",
                             /*count*/ -1, "Patches"),
                    2, 1);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

int Pcsx2CategoryHub::countSettings(const QString& category) const {
    Pcsx2LibretroAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
