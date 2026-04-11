#include "pcsx2_toggle.h"
#include "../pcsx2_theme.h"
#include <QPainter>

Pcsx2Toggle::Pcsx2Toggle(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    connect(this, &QAbstractButton::toggled, this, [this]{ update(); });
}

void Pcsx2Toggle::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool on = isChecked();
    QColor track = on ? Pcsx2Theme::accent() : Pcsx2Theme::cardBorder();
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);
    const int knobD = height() - 4;
    const int x = on ? (width() - knobD - 2) : 2;
    p.setBrush(Pcsx2Theme::textPrimary());
    p.drawEllipse(x, 2, knobD, knobD);
    if (hasFocus()) {
        QPen pen(Pcsx2Theme::accent(), 1);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(0,0,-1,-1), height()/2.0, height()/2.0);
    }
}
