#include "settings_toggle_row.h"
#include "settings_toggle.h"
#include "ui/settings/settings_dialog_theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QMouseEvent>

SettingsToggleRow::SettingsToggleRow(QWidget* parent) : QWidget(parent) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color:#d0ccc4;font-size:13px;");
    m_label->setMinimumHeight(24);
    m_toggle = new SettingsToggle(this);
    lay->addWidget(m_label, 1);
    lay->addWidget(m_toggle, 0, Qt::AlignRight);
    connect(m_toggle, &QAbstractButton::toggled, this, &SettingsToggleRow::toggled);
    m_toggle->installEventFilter(this);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(34);
}

void SettingsToggleRow::setLabel(const QString& text) { m_label->setText(text); }
void SettingsToggleRow::setChecked(bool on) { m_toggle->setChecked(on); }
bool SettingsToggleRow::isChecked() const { return m_toggle->isChecked(); }

bool SettingsToggleRow::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_toggle && e->type() == QEvent::FocusIn) {
        emit focused(m_def);
    }
    return QWidget::eventFilter(obj, e);
}

void SettingsToggleRow::enterEvent(QEnterEvent* e) {
    QWidget::enterEvent(e);
    emit focused(m_def);
}

void SettingsToggleRow::mousePressEvent(QMouseEvent* e) {
    Q_UNUSED(e);
    // Mirror the inner toggle's enabled state — when a dependent toggle has
    // been disabled because its master is off, clicking anywhere on the row
    // (including the label) must not flip the value.
    if (m_toggle->isEnabled()) m_toggle->toggle();
}
