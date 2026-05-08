#include "hotkey_binding_row.h"
#include "ui/settings/binding_widget_common.h"
#include "ui/settings/settings_dialog_theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QStyle>

HotkeyBindingRow::HotkeyBindingRow(const HotkeyDef& def, QWidget* parent)
    : QWidget(parent), m_def(def) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("focused", false);
    setStyleSheet(QStringLiteral(
        "HotkeyBindingRow { border-radius: 4px; background: transparent; }"
        "HotkeyBindingRow[focused=\"true\"] { background: %1; }")
        .arg(SettingsDialogTheme::titleBarBg().name()));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(12);

    m_label = new QLabel(def.label, this);
    m_label->setFixedWidth(220);
    m_label->setStyleSheet(QStringLiteral(
        "color:%1; font-size:13px; background:transparent;")
        .arg(SettingsDialogTheme::textPrimary().name()));
    layout->addWidget(m_label);

    m_button = new BindBtn(this);
    m_button->setFixedHeight(kBtnH);
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->setStyleSheet(kHotkeyRowDefaultStyle);
    m_button->setText(QStringLiteral("Not bound"));

    connect(m_button, &QPushButton::clicked, this,
            [this]{ emit rebindRequested(m_def); });
    m_button->onShiftClick = [this]{ emit appendRebindRequested(m_def); };
    m_button->onRightClick = [this]{ emit clearRequested(m_def); };

    layout->addWidget(m_button, 1);
}

void HotkeyBindingRow::setBindingDisplay(const QString& displayText) {
    m_button->setText(displayText.isEmpty() ? QStringLiteral("Not bound")
                                            : displayText);
    // Tooltip preserves the three-line input guide users were trained on by
    // the legacy hotkey page (spec §6).
    const QString tip = (displayText.isEmpty() ? QStringLiteral("Not bound")
                                                : displayText)
        + QStringLiteral("\n\nLeft click to assign a new button\n"
                          "Shift + left click for additional bindings\n"
                          "Right click to clear binding");
    m_button->setToolTip(tip);
}

void HotkeyBindingRow::setCapturing(bool capturing) {
    if (capturing) {
        m_button->setStyleSheet(kCapturingStyle);
        m_button->setText(QStringLiteral("Press a button... [5]"));
    } else {
        m_button->setStyleSheet(kHotkeyRowDefaultStyle);
    }
}

void HotkeyBindingRow::setCapturingText(const QString& text) {
    m_button->setText(text);
}

void HotkeyBindingRow::focusInEvent(QFocusEvent* e) {
    QWidget::focusInEvent(e);
    setProperty("focused", true);
    style()->unpolish(this); style()->polish(this);
    update();
    emit focused(m_def);
}

void HotkeyBindingRow::focusOutEvent(QFocusEvent* e) {
    QWidget::focusOutEvent(e);
    setProperty("focused", false);
    style()->unpolish(this); style()->polish(this);
    update();
}

void HotkeyBindingRow::keyPressEvent(QKeyEvent* e) {
    // Catch Up/Down before they bubble up to the parent QScrollArea, which
    // would otherwise consume them as scroll commands. Emitting a signal
    // (rather than walking the parent chain to the page) keeps this widget
    // independent of where it lives in the dialog hierarchy.
    if (e->key() == Qt::Key_Up) {
        emit navigateRequested(-1);
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_Down) {
        emit navigateRequested(+1);
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

void HotkeyBindingRow::paintEvent(QPaintEvent* e) {
    QWidget::paintEvent(e);
    if (!hasFocus()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor halo = SettingsDialogTheme::accent();
    halo.setAlphaF(0.45);
    QPen pen(halo, 2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QRectF r = rect().adjusted(1, 1, -1, -1);
    p.drawRoundedRect(r, 4, 4);
}
