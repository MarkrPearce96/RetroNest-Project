#include "settings_graphics_sub_tab_bar.h"
#include "ui/settings/settings_dialog_theme.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFontMetrics>

namespace {
constexpr int kTabWidth  = 120;
constexpr int kTabHeight = 64;
constexpr int kGap       = 10;
constexpr int kIconSize  = 22;
}

SettingsGraphicsSubTabBar::SettingsGraphicsSubTabBar(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumHeight(kTabHeight + 6);
    setCursor(Qt::PointingHandCursor);
}

void SettingsGraphicsSubTabBar::addTab(const QString& icon, const QString& label) {
    m_tabs.append({icon, label});
    updateGeometry();
    update();
}

void SettingsGraphicsSubTabBar::setCurrentIndex(int idx) {
    if (idx < 0 || idx >= m_tabs.size()) return;
    if (idx == m_current) return;
    m_current = idx;
    update();
    emit tabActivated(m_current);
}

QSize SettingsGraphicsSubTabBar::sizeHint() const {
    const int count = m_tabs.size();
    if (count == 0) return QSize(kTabWidth, kTabHeight + 6);
    return QSize(count * kTabWidth + (count - 1) * kGap, kTabHeight + 6);
}

QRect SettingsGraphicsSubTabBar::tabRectAt(int idx) const {
    return QRect(idx * (kTabWidth + kGap), 0, kTabWidth, kTabHeight);
}

void SettingsGraphicsSubTabBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < m_tabs.size(); ++i) {
        const QRect r = tabRectAt(i);
        const bool active = (i == m_current);

        // Box background
        QColor bg = active ? SettingsDialogTheme::cardBg().lighter(110) : SettingsDialogTheme::cardBg();
        QColor border = active ? SettingsDialogTheme::accent() : SettingsDialogTheme::cardBorder();
        p.setPen(QPen(border, active ? 1.5 : 1.0));
        p.setBrush(bg);
        p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 8, 8);

        // Icon (centered, upper half)
        p.setPen(SettingsDialogTheme::textPrimary());
        QFont iconFont = p.font();
        iconFont.setPointSize(kIconSize);
        p.setFont(iconFont);
        QRect iconRect(r.x(), r.y() + 8, r.width(), kIconSize + 6);
        p.drawText(iconRect, Qt::AlignHCenter | Qt::AlignVCenter, m_tabs[i].icon);

        // Label (lower half)
        QFont labelFont = p.font();
        labelFont.setPointSize(10);
        labelFont.setBold(active);
        p.setFont(labelFont);
        p.setPen(active ? SettingsDialogTheme::textPrimary() : SettingsDialogTheme::textSecondary());
        QRect labelRect(r.x(), r.y() + kTabHeight - 22, r.width(), 18);
        p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, m_tabs[i].label);

        // Amber underline for active tab
        if (active) {
            p.setPen(Qt::NoPen);
            p.setBrush(SettingsDialogTheme::accent());
            QRect underline(r.x() + 12, r.bottom() + 2, r.width() - 24, 3);
            p.drawRoundedRect(underline, 1.5, 1.5);
        }
    }

    // Focus ring around the whole bar when the widget has focus
    if (hasFocus()) {
        QColor halo = SettingsDialogTheme::accent();
        halo.setAlphaF(0.35);
        p.setPen(QPen(halo, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 10, 10);
    }
}

void SettingsGraphicsSubTabBar::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_Left:
            if (m_current > 0) setCurrentIndex(m_current - 1);
            return;
        case Qt::Key_Right:
            if (m_current < m_tabs.size() - 1) setCurrentIndex(m_current + 1);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            emit tabActivated(m_current);
            return;
        default:
            QWidget::keyPressEvent(e);
    }
}

void SettingsGraphicsSubTabBar::mousePressEvent(QMouseEvent* e) {
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (tabRectAt(i).contains(e->pos())) {
            setFocus();
            setCurrentIndex(i);
            return;
        }
    }
    QWidget::mousePressEvent(e);
}

void SettingsGraphicsSubTabBar::focusInEvent(QFocusEvent* e) { QWidget::focusInEvent(e); update(); }
void SettingsGraphicsSubTabBar::focusOutEvent(QFocusEvent* e) { QWidget::focusOutEvent(e); update(); }
