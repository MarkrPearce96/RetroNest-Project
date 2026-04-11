#include "pcsx2_combo_row.h"
#include "../pcsx2_theme.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>

Pcsx2ComboRow::Pcsx2ComboRow(QWidget* parent) : QWidget(parent) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color:#d0ccc4;font-size:13px;");
    m_label->setMinimumWidth(180);
    m_label->setMinimumHeight(24);
    m_combo = new QComboBox(this);
    m_combo->setStyleSheet(Pcsx2Theme::comboQss());
    m_combo->setMinimumWidth(200);
    lay->addWidget(m_label, 0);
    lay->addWidget(m_combo, 1);
    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ emit valueChanged(value()); });
    m_combo->installEventFilter(this);
    setMinimumHeight(42);
}

void Pcsx2ComboRow::setLabel(const QString& text) { m_label->setText(text); }

void Pcsx2ComboRow::setOptions(const QVector<QPair<QString, QString>>& opts) {
    m_combo->blockSignals(true);
    m_combo->clear();
    for (const auto& o : opts) m_combo->addItem(o.first, o.second);
    m_combo->blockSignals(false);
}

void Pcsx2ComboRow::setValue(const QString& iniValue) {
    for (int i = 0; i < m_combo->count(); ++i) {
        if (m_combo->itemData(i).toString() == iniValue) {
            m_combo->setCurrentIndex(i);
            return;
        }
    }
}

QString Pcsx2ComboRow::value() const {
    return m_combo->currentData().toString();
}

bool Pcsx2ComboRow::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_combo && e->type() == QEvent::FocusIn) emit focused(m_def);
    return QWidget::eventFilter(obj, e);
}
