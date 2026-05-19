#include "hotkey_binding_row.h"
#include "ui/settings/binding_widget_common.h"
#include "ui/settings/settings_dialog_theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QStyle>

namespace {
constexpr const char* kColumnSelectedStyle =
    "QPushButton { background:#3a352e; color:#f2efe8; border:1px solid #f59e0b;"
    "  border-radius:4px; padding:6px 14px; font-weight:600; }";
constexpr const char* kColumnUnselectedStyle =
    "QPushButton { background:#2b2823; color:#9a9690; border:1px solid #3a3632;"
    "  border-radius:4px; padding:6px 14px; }";
}  // namespace

HotkeyBindingRow::HotkeyBindingRow(const HotkeyDef& def,
                                    bool dualColumn,
                                    QWidget* parent)
    : QWidget(parent), m_def(def), m_dualColumn(dualColumn) {
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
    m_button->setFocusPolicy(Qt::NoFocus);  // row owns focus
    m_button->setText(QStringLiteral("Not bound"));
    connect(m_button, &QPushButton::clicked, this, [this]{
        if (m_dualColumn) {
            m_currentColumn = ColKeyboard;
            applyColumnHighlight();
            emit columnChanged(m_def, m_currentColumn);
        }
        emit rebindRequested(m_def);
    });
    m_button->onShiftClick = [this]{ emit appendRebindRequested(m_def); };
    m_button->onRightClick = [this]{ emit clearRequested(m_def); };
    layout->addWidget(m_button, 1);

    if (m_dualColumn) {
        m_controllerButton = new BindBtn(this);
        m_controllerButton->setFixedHeight(kBtnH);
        m_controllerButton->setCursor(Qt::PointingHandCursor);
        m_controllerButton->setFocusPolicy(Qt::NoFocus);
        m_controllerButton->setText(QStringLiteral("Not bound"));
        connect(m_controllerButton, &QPushButton::clicked, this, [this]{
            m_currentColumn = ColController;
            applyColumnHighlight();
            emit columnChanged(m_def, m_currentColumn);
            emit rebindRequested(m_def);
        });
        m_controllerButton->onShiftClick = [this]{
            m_currentColumn = ColController;
            applyColumnHighlight();
            emit columnChanged(m_def, m_currentColumn);
            emit appendRebindRequested(m_def);
        };
        m_controllerButton->onRightClick = [this]{
            m_currentColumn = ColController;
            applyColumnHighlight();
            emit columnChanged(m_def, m_currentColumn);
            emit clearRequested(m_def);
        };
        layout->addWidget(m_controllerButton, 1);
        applyColumnHighlight();
    } else {
        m_button->setStyleSheet(kHotkeyRowDefaultStyle);
    }
}

void HotkeyBindingRow::setBindingDisplay(const QString& displayText) {
    if (m_dualColumn) return;  // use setDualBindingDisplay instead
    m_button->setText(displayText.isEmpty() ? QStringLiteral("Not bound")
                                             : displayText);
    const QString tip = (displayText.isEmpty() ? QStringLiteral("Not bound")
                                                : displayText)
        + QStringLiteral("\n\nLeft click to assign a new button\n"
                         "Shift + left click for additional bindings\n"
                         "Right click to clear binding");
    m_button->setToolTip(tip);
}

void HotkeyBindingRow::setDualBindingDisplay(const QString& keyboardText,
                                              const QString& controllerText) {
    if (!m_dualColumn) {
        setBindingDisplay(keyboardText);
        return;
    }
    m_button->setText(keyboardText.isEmpty() ? QStringLiteral("Not bound")
                                              : keyboardText);
    m_controllerButton->setText(controllerText.isEmpty()
                                    ? QStringLiteral("Not bound")
                                    : controllerText);
    const QString tipBase =
        QStringLiteral("\n\nLeft click to assign a new binding\n"
                       "Shift + left click for additional bindings\n"
                       "Right click to clear");
    m_button->setToolTip((keyboardText.isEmpty() ? QStringLiteral("Not bound")
                                                  : keyboardText) + tipBase);
    m_controllerButton->setToolTip((controllerText.isEmpty()
                                        ? QStringLiteral("Not bound")
                                        : controllerText) + tipBase);
}

void HotkeyBindingRow::setCapturing(bool capturing) {
    BindBtn* target = (m_dualColumn && m_currentColumn == ColController)
                          ? m_controllerButton
                          : m_button;
    if (capturing) {
        target->setStyleSheet(kCapturingStyle);
        target->setText(QStringLiteral("Press a button... [5]"));
    } else {
        if (m_dualColumn) applyColumnHighlight();
        else target->setStyleSheet(kHotkeyRowDefaultStyle);
    }
}

void HotkeyBindingRow::setCapturingText(const QString& text) {
    BindBtn* target = (m_dualColumn && m_currentColumn == ColController)
                          ? m_controllerButton
                          : m_button;
    target->setText(text);
}

void HotkeyBindingRow::applyColumnHighlight() {
    if (!m_dualColumn) return;
    m_button->setStyleSheet(m_currentColumn == ColKeyboard
                                ? kColumnSelectedStyle
                                : kColumnUnselectedStyle);
    m_controllerButton->setStyleSheet(m_currentColumn == ColController
                                          ? kColumnSelectedStyle
                                          : kColumnUnselectedStyle);
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
    // Dual-column rows: Left/Right toggles the active column.
    if (m_dualColumn) {
        if (e->key() == Qt::Key_Left && m_currentColumn != ColKeyboard) {
            m_currentColumn = ColKeyboard;
            applyColumnHighlight();
            emit columnChanged(m_def, m_currentColumn);
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_Right && m_currentColumn != ColController) {
            m_currentColumn = ColController;
            applyColumnHighlight();
            emit columnChanged(m_def, m_currentColumn);
            e->accept();
            return;
        }
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
