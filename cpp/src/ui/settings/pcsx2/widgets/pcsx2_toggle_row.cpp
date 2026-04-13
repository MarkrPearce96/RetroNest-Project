#include "pcsx2_toggle_row.h"
#include "pcsx2_toggle.h"
#include "../pcsx2_theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QMouseEvent>

Pcsx2ToggleRow::Pcsx2ToggleRow(QWidget* parent) : QWidget(parent) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color:#d0ccc4;font-size:13px;");
    m_label->setMinimumHeight(24);
    m_toggle = new Pcsx2Toggle(this);
    lay->addWidget(m_label, 1);
    lay->addWidget(m_toggle, 0, Qt::AlignRight);
    connect(m_toggle, &QAbstractButton::toggled, this, &Pcsx2ToggleRow::toggled);
    m_toggle->installEventFilter(this);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(34);
}

void Pcsx2ToggleRow::setLabel(const QString& text) { m_label->setText(text); }
void Pcsx2ToggleRow::setChecked(bool on) { m_toggle->setChecked(on); }
bool Pcsx2ToggleRow::isChecked() const { return m_toggle->isChecked(); }

bool Pcsx2ToggleRow::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_toggle && e->type() == QEvent::FocusIn) {
        emit focused(m_def);
    }
    return QWidget::eventFilter(obj, e);
}

void Pcsx2ToggleRow::enterEvent(QEnterEvent* e) {
    QWidget::enterEvent(e);
    emit focused(m_def);
}

void Pcsx2ToggleRow::mousePressEvent(QMouseEvent* e) {
    Q_UNUSED(e);
    m_toggle->toggle();
}
