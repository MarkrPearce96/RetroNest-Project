#include "settings_toggle.h"
#include "ui/settings/settings_dialog_theme.h"
#include <QPainter>
#include <QKeyEvent>

SettingsToggle::SettingsToggle(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    connect(this, &QAbstractButton::toggled, this, [this]{ update(); });
}

void SettingsToggle::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool on = isChecked();
    QColor track = on ? SettingsDialogTheme::accent() : SettingsDialogTheme::cardBorder();
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);
    const int knobD = height() - 4;
    const int x = on ? (width() - knobD - 2) : 2;
    p.setBrush(SettingsDialogTheme::textPrimary());
    p.drawEllipse(x, 2, knobD, knobD);
    if (hasFocus()) {
        // Use white when checked (amber track would hide an amber ring).
        QColor ring = on ? SettingsDialogTheme::textPrimary() : SettingsDialogTheme::accent();
        QPen pen(ring, 2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1,1,-2,-2), height()/2.0, height()/2.0);
    }
}

void SettingsToggle::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        toggle();
        return;
    }
    QAbstractButton::keyPressEvent(e);
}
