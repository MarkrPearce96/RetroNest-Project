#include "duckstation_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/duckstation_adapter.h"
#include <QGridLayout>
#include <QPushButton>
#include <QKeyEvent>

DuckStationCategoryHub::DuckStationCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("DuckStation Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    grid->addWidget(makeCard(QStringLiteral("\U0001F39B"),  "Console",
                             "Region, BIOS, fast boot",
                             countSettings("Console"), "Console"),   0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed control, CPU, timing",
                             countSettings("Emulation"), "Emulation"), 0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",
                             "Renderer, rendering, advanced, OSD",
                             countSettings("Graphics") + countSettings("On-Screen Display"),
                             "Graphics"), 1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, latency, volume",
                             countSettings("Audio"), "Audio"),        1, 1);

    m_stretchCard = makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards",
                             "Slots and card types",
                             countSettings("Memory Cards"), "Memory Cards");
    m_stretchCard->setMinimumHeight(120);  // shorter than the 2×2 cards — full-width doesn't need 180
    grid->addWidget(m_stretchCard, 2, 0, 1, 2);  // span 2 columns
    m_stretchCard->installEventFilter(this);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

bool DuckStationCategoryHub::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() != QEvent::KeyPress)
        return EmulatorCategoryHubBase::eventFilter(obj, e);

    auto* ke = static_cast<QKeyEvent*>(e);

    // Up from native button → focus the stretched Memory Cards card.
    if (obj == nativeBtn() && ke->key() == Qt::Key_Up && m_stretchCard) {
        m_stretchCard->setFocus(Qt::TabFocusReason);
        return true;
    }
    // Down from stretched card → focus native button.
    if (obj == m_stretchCard && ke->key() == Qt::Key_Down) {
        nativeBtn()->setFocus(Qt::TabFocusReason);
        return true;
    }

    return EmulatorCategoryHubBase::eventFilter(obj, e);
}

int DuckStationCategoryHub::countSettings(const QString& category) const {
    DuckStationAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
