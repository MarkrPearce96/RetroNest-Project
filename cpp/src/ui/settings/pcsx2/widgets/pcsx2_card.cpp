#include "pcsx2_card.h"
#include "../pcsx2_theme.h"
#include <QPainter>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QStyle>

Pcsx2Card::Pcsx2Card(QWidget* parent) : QFrame(parent) {
    setObjectName("Pcsx2Card");
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(Pcsx2Theme::cardQss());
    setProperty("focused", false);
}

void Pcsx2Card::focusInEvent(QFocusEvent* e) {
    QFrame::focusInEvent(e);
    setProperty("focused", true);
    style()->unpolish(this); style()->polish(this);
    emit focused();
    emit focusedChanged();
    update();
}

void Pcsx2Card::focusOutEvent(QFocusEvent* e) {
    QFrame::focusOutEvent(e);
    setProperty("focused", false);
    style()->unpolish(this); style()->polish(this);
    emit focusedChanged();
    update();
}

void Pcsx2Card::paintEvent(QPaintEvent* e) {
    QFrame::paintEvent(e);
    if (!hasFocus()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor halo = Pcsx2Theme::accent();
    halo.setAlphaF(0.30);
    QPen pen(halo, 2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QRectF r = rect().adjusted(1, 1, -1, -1);
    p.drawRoundedRect(r, 8, 8);
}

void Pcsx2Card::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        emit activated();
        return;
    }
    QFrame::keyPressEvent(e);
}
