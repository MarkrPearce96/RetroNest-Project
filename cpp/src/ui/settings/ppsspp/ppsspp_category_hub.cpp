#include "ppsspp_category_hub.h"
#include "../pcsx2/widgets/pcsx2_card.h"
#include "ppsspp_theme.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>

PpssppCategoryHub::PpssppCategoryHub(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(14);

    auto* title = new QLabel("PPSSPP Settings", this);
    title->setStyleSheet("color:#f2efe8;font-size:20px;font-weight:600;");
    root->addWidget(title);

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
    connect(m_nativeBtn, &QPushButton::clicked, this, &PpssppCategoryHub::openNativeRequested);
    m_nativeBtn->installEventFilter(this);

    // Install filter on bottom-row cards so Down → native button works.
    const auto cards = findChildren<Pcsx2Card*>(QString(), Qt::FindDirectChildrenOnly);
    if (cards.size() >= 2) {
        cards[cards.size() - 2]->installEventFilter(this);
        cards[cards.size() - 1]->installEventFilter(this);
    }
}

bool PpssppCategoryHub::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, e);

    auto* ke = static_cast<QKeyEvent*>(e);

    if (obj == m_nativeBtn) {
        if (ke->key() == Qt::Key_Up) {
            const auto cards = findChildren<Pcsx2Card*>(QString(), Qt::FindDirectChildrenOnly);
            if (!cards.isEmpty()) {
                cards.last()->setFocus(Qt::TabFocusReason);
                return true;
            }
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            emit openNativeRequested();
            return true;
        }
    }

    if (auto* card = qobject_cast<Pcsx2Card*>(obj)) {
        if (ke->key() == Qt::Key_Down) {
            m_nativeBtn->setFocus(Qt::TabFocusReason);
            return true;
        }
    }

    return QWidget::eventFilter(obj, e);
}

Pcsx2Card* PpssppCategoryHub::makeCard(const QString& icon, const QString& title,
                                       const QString& descriptor, int settingCount,
                                       const QString& categoryKey) {
    auto* card = new Pcsx2Card(this);
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(180);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(20, 22, 20, 18);
    v->setSpacing(8);
    v->setAlignment(Qt::AlignHCenter);

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

    auto* t = new QLabel(title, card);
    t->setAlignment(Qt::AlignCenter);
    t->setStyleSheet("color:#f2efe8;font-size:18px;font-weight:600;");

    auto* d = new QLabel(descriptor, card);
    d->setAlignment(Qt::AlignCenter);
    d->setWordWrap(true);
    d->setStyleSheet("color:#b0aca4;font-size:13px;");

    auto* c = new QLabel(QString("%1 settings  →").arg(settingCount), card);
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
