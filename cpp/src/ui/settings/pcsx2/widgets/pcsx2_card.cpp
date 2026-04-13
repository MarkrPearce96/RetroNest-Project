#include "pcsx2_card.h"
#include "../pcsx2_theme.h"
#include "pcsx2_toggle.h"
#include "pcsx2_slider_row.h"
#include <QScrollArea>
#include <QSlider>
#include <QPainter>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QWidget>
#include <QComboBox>
#include <QTimer>
#include <limits>

Pcsx2Card::Pcsx2Card(QWidget* parent) : QFrame(parent) {
    setObjectName("Pcsx2Card");
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);   // make QSS background paint
    setFrameStyle(QFrame::NoFrame);                // disable QFrame's default frame
    setStyleSheet(Pcsx2Theme::cardQss());
    setProperty("focused", false);
}

void Pcsx2Card::setPreviewStyle(bool preview) {
    if (preview) {
        setStyleSheet(QStringLiteral(
            "QFrame#Pcsx2Card {"
            "  background-color: #504c48;"
            "  border: 1px solid #706c66;"
            "  border-radius: 8px;"
            "}"
            "QFrame#Pcsx2Card[focused=\"true\"] {"
            "  border: 1px solid #f59e0b;"
            "}"));
    } else {
        setStyleSheet(Pcsx2Theme::cardQss());
    }
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void Pcsx2Card::focusInEvent(QFocusEvent* e) {
    QFrame::focusInEvent(e);
    setProperty("focused", true);
    style()->unpolish(this); style()->polish(this);
    emit focused(m_settingDef);
    emit focusedChanged();
    update();
}

void Pcsx2Card::mousePressEvent(QMouseEvent* e) {
    QFrame::mousePressEvent(e);
    setFocus(Qt::MouseFocusReason);
    emit activated();
}

void Pcsx2Card::enterEvent(QEnterEvent* e) {
    QFrame::enterEvent(e);
    emit focused(m_settingDef);
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

    // Only handle Enter/arrow if the card itself has focus — not when a
    // child control propagated the event up the parent chain.
    if (!hasFocus()) {
        QFrame::keyPressEvent(e);
        return;
    }

    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        // Try to activate the inner interactive widget first (toggle, combo, or slider).
        if (auto* t = findChild<Pcsx2Toggle*>()) {
            t->toggle();
            return;
        }
        if (auto* c = findChild<QComboBox*>()) {
            QTimer::singleShot(0, c, [c]() {
                c->setFocus(Qt::PopupFocusReason);
                c->showPopup();
            });
            return;
        }
        if (auto* sliderRow = findChild<Pcsx2SliderRow*>()) {
            if (auto* slider = sliderRow->findChild<QSlider*>()) {
                slider->setFocus(Qt::TabFocusReason);
                sliderRow->setEditing(true);
            }
            return;
        }
        // No inner interactive widget — fall back to activated() (hub cards).
        emit activated();
        return;
    }

    if (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down) {
        QWidget* parent = parentWidget();
        if (parent) {
            const auto siblings = parent->findChildren<Pcsx2Card*>(QString(), Qt::FindDirectChildrenOnly);
            if (siblings.size() >= 2) {
                const QRect mine = geometry();
                const QPoint myCenter = mine.center();

                Pcsx2Card* bestOverlap = nullptr;
                long long bestOverlapScore = std::numeric_limits<long long>::max();

                auto rangesOverlap = [](int a0, int a1, int b0, int b1) {
                    return a0 < b1 && b0 < a1;
                };

                const bool vertical = (k == Qt::Key_Up || k == Qt::Key_Down);

                for (Pcsx2Card* s : siblings) {
                    if (s == this || !s->isVisible()) continue;
                    const QRect r = s->geometry();
                    const QPoint c = r.center();
                    const int dx = c.x() - myCenter.x();
                    const int dy = c.y() - myCenter.y();

                    bool inDir = false;
                    bool perpOverlap = false;

                    switch (k) {
                        case Qt::Key_Left:
                            inDir = dx < 0;
                            perpOverlap = rangesOverlap(mine.top(), mine.bottom(), r.top(), r.bottom());
                            break;
                        case Qt::Key_Right:
                            inDir = dx > 0;
                            perpOverlap = rangesOverlap(mine.top(), mine.bottom(), r.top(), r.bottom());
                            break;
                        case Qt::Key_Up:
                            inDir = dy < 0;
                            perpOverlap = rangesOverlap(mine.left(), mine.right(), r.left(), r.right());
                            break;
                        case Qt::Key_Down:
                            inDir = dy > 0;
                            perpOverlap = rangesOverlap(mine.left(), mine.right(), r.left(), r.right());
                            break;
                    }
                    if (!inDir) continue;

                    // Use overlap-center distance instead of widget-center
                    // distance so full-width cards don't get penalised.
                    long long adx = qAbs(dx);
                    long long ady = qAbs(dy);
                    if (perpOverlap) {
                        if (vertical) {
                            int oL = qMax(mine.left(), r.left());
                            int oR = qMin(mine.right(), r.right());
                            adx = qAbs((oL + oR) / 2 - myCenter.x());
                        } else {
                            int oT = qMax(mine.top(), r.top());
                            int oB = qMin(mine.bottom(), r.bottom());
                            ady = qAbs((oT + oB) / 2 - myCenter.y());
                        }
                    }
                    const long long score = vertical
                        ? (ady * 2LL + adx)
                        : (adx * 2LL + ady);

                    if (perpOverlap && score < bestOverlapScore) {
                        bestOverlapScore = score;
                        bestOverlap = s;
                    }
                }

                Pcsx2Card* best = bestOverlap;
                if (best) {
                    best->setFocus(Qt::TabFocusReason);
                    // If we're inside a scroll area, scroll the new focus into view.
                    QWidget* p = best->parentWidget();
                    while (p) {
                        if (auto* sa = qobject_cast<QScrollArea*>(p)) {
                            sa->ensureWidgetVisible(best, 20, 40);
                            break;
                        }
                        p = p->parentWidget();
                    }
                    return;
                }
            }
        }
    }

    QFrame::keyPressEvent(e);
}
