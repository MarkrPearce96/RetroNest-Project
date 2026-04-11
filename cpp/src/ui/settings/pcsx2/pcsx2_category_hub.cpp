#include "pcsx2_category_hub.h"
#include "widgets/pcsx2_card.h"
#include "pcsx2_theme.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

Pcsx2CategoryHub::Pcsx2CategoryHub(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(14);

    auto* title = new QLabel("PCSX2 Settings", this);
    title->setStyleSheet("color:#f2efe8;font-size:20px;font-weight:600;");
    root->addWidget(title);

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",    "Speed control, system settings, frame pacing", 16, "Emulation"),    0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",     "Renderer, display, post-processing, OSD",       35, "Graphics"),     0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",        "Backend, latency, volume",                      11, "Audio"),        1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards", "Memory cards and multitap slots",                7, "Memory Cards"), 1, 1);
    root->addLayout(grid, 1);

    auto* nativeBtn = new QPushButton("Open Native Settings", this);
    nativeBtn->setStyleSheet(
        "QPushButton { background:#4a4642; color:#f2efe8; border:1px solid #706c66;"
        " border-radius:4px; padding:6px 14px; }"
        "QPushButton:focus { border-color:#f59e0b; }");
    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    bottom->addWidget(nativeBtn);
    root->addLayout(bottom);
    connect(nativeBtn, &QPushButton::clicked, this, &Pcsx2CategoryHub::openNativeRequested);
}

Pcsx2Card* Pcsx2CategoryHub::makeCard(const QString& icon, const QString& title,
                                      const QString& descriptor, int settingCount,
                                      const QString& categoryKey) {
    auto* card = new Pcsx2Card(this);
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
