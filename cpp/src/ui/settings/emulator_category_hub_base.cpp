#include "emulator_category_hub_base.h"
#include "ui/settings/widgets/settings_card.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>

EmulatorCategoryHubBase::EmulatorCategoryHubBase(QWidget* parent) : QWidget(parent) {}

void EmulatorCategoryHubBase::setupChrome(const QString& title) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(14);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet("color:#f2efe8;font-size:20px;font-weight:600;");
    root->addWidget(titleLabel);

    // Scroll area wraps the content layout so the hub copes with hubs that
    // exceed the dialog viewport (e.g. PCSX2's 8-card grid). Transparent
    // chrome to keep the existing flat look — the scrollbar is the only
    // visible chrome and only when needed.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: 0; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical {"
        "  background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical {"
        "  background: #585450; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #706c66; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent; }");

    auto* scrollContent = new QWidget(scroll);
    m_contentLayout = new QVBoxLayout(scrollContent);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(14);
    scroll->setWidget(scrollContent);
    root->addWidget(scroll, 1);

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

    connect(m_nativeBtn, &QPushButton::clicked, this,
            &EmulatorCategoryHubBase::openNativeRequested);
    m_nativeBtn->installEventFilter(this);
}

SettingsCard* EmulatorCategoryHubBase::makeCard(const QString& icon, const QString& title,
                                              const QString& descriptor, int settingCount,
                                              const QString& categoryKey) {
    auto* card = new SettingsCard(this);
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

    v->addWidget(iconTile, 0, Qt::AlignHCenter);
    v->addSpacing(4);
    v->addWidget(t);
    v->addWidget(d);
    v->addStretch();

    auto* c = new QLabel(QString("%1 settings  →").arg(settingCount), card);
    c->setAlignment(Qt::AlignCenter);
    c->setStyleSheet("color:#f59e0b;font-size:12px;font-weight:500;");
    v->addWidget(c);

    QObject::connect(card, &SettingsCard::activated, this,
                     [this, categoryKey]{ emit categoryActivated(categoryKey); });
    return card;
}

bool EmulatorCategoryHubBase::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, e);

    auto* ke = static_cast<QKeyEvent*>(e);

    if (obj == m_nativeBtn) {
        if (ke->key() == Qt::Key_Up) {
            const auto cards = findChildren<SettingsCard*>(QString(), Qt::FindDirectChildrenOnly);
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

    if (qobject_cast<SettingsCard*>(obj)) {
        if (ke->key() == Qt::Key_Down) {
            m_nativeBtn->setFocus(Qt::TabFocusReason);
            return true;
        }
    }

    return QWidget::eventFilter(obj, e);
}
