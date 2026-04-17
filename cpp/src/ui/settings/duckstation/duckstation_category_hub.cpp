#include "duckstation_category_hub.h"
#include "../pcsx2/widgets/pcsx2_card.h"
#include "duckstation_theme.h"
#include "adapters/duckstation_adapter.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>

DuckStationCategoryHub::DuckStationCategoryHub(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(14);

    auto* title = new QLabel("DuckStation Settings", this);
    title->setStyleSheet("color:#f2efe8;font-size:20px;font-weight:600;");
    root->addWidget(title);

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

    root->addLayout(grid, 1);

    m_nativeBtn = new QPushButton("Open Native Settings", this);
    m_nativeBtn->setCursor(Qt::PointingHandCursor);
    m_nativeBtn->setStyleSheet(
        "QPushButton { background:#4a4642; color:#f2efe8; border:1px solid #706c66;"
        " border-radius:4px; padding:6px 14px; }"
        "QPushButton:focus { border-color:#f59e0b; }");
    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    bottom->addWidget(m_nativeBtn);
    root->addLayout(bottom);
    connect(m_nativeBtn, &QPushButton::clicked, this, &DuckStationCategoryHub::openNativeRequested);
    m_nativeBtn->installEventFilter(this);
}

bool DuckStationCategoryHub::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, e);

    auto* ke = static_cast<QKeyEvent*>(e);

    // Up from the native button → focus the stretched Memory Cards card.
    if (obj == m_nativeBtn) {
        if (ke->key() == Qt::Key_Up) {
            if (m_stretchCard) { m_stretchCard->setFocus(Qt::TabFocusReason); return true; }
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            emit openNativeRequested();
            return true;
        }
    }

    // Down from the stretched card → focus the native button.
    if (obj == m_stretchCard) {
        if (ke->key() == Qt::Key_Down) {
            m_nativeBtn->setFocus(Qt::TabFocusReason);
            return true;
        }
    }

    return QWidget::eventFilter(obj, e);
}

int DuckStationCategoryHub::countSettings(const QString& category) const {
    DuckStationAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}

Pcsx2Card* DuckStationCategoryHub::makeCard(const QString& icon, const QString& title,
                                            const QString& descriptor, int settingCount,
                                            const QString& categoryKey) {
    auto* card = new Pcsx2Card(this);
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(180);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(20, 22, 20, 18);
    v->setSpacing(8);
    v->setAlignment(Qt::AlignHCenter);

    // Icon tile — 56x56 rounded square with dark background and big emoji
    auto* iconTile = new QLabel(icon, card);
    iconTile->setAlignment(Qt::AlignCenter);
    iconTile->setFixedSize(56, 56);
    iconTile->setStyleSheet(
        "QLabel {"
        "  background-color: #585450;"
        "  border-radius: 12px;"
        "  font-size: 28px;"
        "  color: #f2efe8;"
        "}");

    // Title — 18px bold, centered
    auto* t = new QLabel(title, card);
    t->setAlignment(Qt::AlignCenter);
    t->setStyleSheet("color:#f2efe8;font-size:18px;font-weight:600;");

    // Descriptor — 13px muted, centered, wrap
    auto* d = new QLabel(descriptor, card);
    d->setAlignment(Qt::AlignCenter);
    d->setWordWrap(true);
    d->setStyleSheet("color:#b0aca4;font-size:13px;");

    // Count — 12px amber, centered
    auto* c = new QLabel(QString("%1 settings  \u2192").arg(settingCount), card);
    c->setAlignment(Qt::AlignCenter);
    c->setStyleSheet("color:#f59e0b;font-size:12px;font-weight:500;");

    v->addWidget(iconTile, 0, Qt::AlignHCenter);
    v->addSpacing(4);
    v->addWidget(t);
    v->addWidget(d);
    v->addStretch();
    v->addWidget(c);

    QObject::connect(card, &Pcsx2Card::activated, this,
                     [this, categoryKey]{ emit categoryActivated(categoryKey); });
    return card;
}
