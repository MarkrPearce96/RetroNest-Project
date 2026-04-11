#include "pcsx2_card.h"
#include "../pcsx2_theme.h"
#include <QPainter>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QStyle>
#include <QWidget>
#include <limits>

Pcsx2Card::Pcsx2Card(QWidget* parent) : QFrame(parent) {
    setObjectName("Pcsx2Card");
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);   // make QSS background paint
    setFrameStyle(QFrame::NoFrame);                // disable QFrame's default frame
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
    const int k = e->key();

    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        emit activated();
        return;
    }

    if (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down) {
        // Spatial arrow navigation: find the nearest sibling Pcsx2Card in the
        // requested direction, among the direct children of our parent widget.
        QWidget* parent = parentWidget();
        if (parent) {
            const auto siblings = parent->findChildren<Pcsx2Card*>(QString(), Qt::FindDirectChildrenOnly);
            if (siblings.size() >= 2) {
                const QPoint myCenter = geometry().center();
                Pcsx2Card* best = nullptr;
                int bestScore = std::numeric_limits<int>::max();

                for (Pcsx2Card* s : siblings) {
                    if (s == this || !s->isVisible()) continue;
                    const QPoint c = s->geometry().center();
                    const int dx = c.x() - myCenter.x();
                    const int dy = c.y() - myCenter.y();

                    bool inDir = false;
                    switch (k) {
                        case Qt::Key_Left:  inDir = dx <  0 && qAbs(dx) >= qAbs(dy); break;
                        case Qt::Key_Right: inDir = dx >  0 && qAbs(dx) >= qAbs(dy); break;
                        case Qt::Key_Up:    inDir = dy <  0 && qAbs(dy) >  qAbs(dx); break;
                        case Qt::Key_Down:  inDir = dy >  0 && qAbs(dy) >  qAbs(dx); break;
                    }
                    if (!inDir) continue;

                    const int score = dx * dx + dy * dy;
                    if (score < bestScore) { bestScore = score; best = s; }
                }

                if (best) {
                    best->setFocus(Qt::TabFocusReason);
                    return;
                }
            }
        }
    }

    QFrame::keyPressEvent(e);
}
