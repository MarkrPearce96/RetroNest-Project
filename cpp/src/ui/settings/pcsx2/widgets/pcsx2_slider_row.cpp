#include "pcsx2_slider_row.h"
#include "../pcsx2_theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QEvent>

Pcsx2SliderRow::Pcsx2SliderRow(QWidget* parent) : QWidget(parent) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color:#d0ccc4;font-size:13px;");
    m_label->setMinimumWidth(180);
    m_label->setMinimumHeight(24);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setStyleSheet(Pcsx2Theme::sliderQss());
    m_value = new QLabel(this);
    m_value->setStyleSheet("color:#f2efe8;font-size:13px;");
    m_value->setMinimumWidth(60);
    m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_label, 0);
    lay->addWidget(m_slider, 1);
    lay->addWidget(m_value, 0);
    connect(m_slider, &QSlider::valueChanged, this, [this](int v){
        refreshValueLabel();
        emit valueChanged(v);
    });
    m_slider->installEventFilter(this);
    setMinimumHeight(42);
}

void Pcsx2SliderRow::setLabel(const QString& t) { m_label->setText(t); }
void Pcsx2SliderRow::setRange(int lo, int hi) { m_slider->setRange(lo, hi); }
void Pcsx2SliderRow::setSuffix(const QString& s) { m_suffix = s; refreshValueLabel(); }
void Pcsx2SliderRow::setValue(int v) { m_slider->setValue(v); }
int Pcsx2SliderRow::value() const { return m_slider->value(); }

void Pcsx2SliderRow::refreshValueLabel() {
    m_value->setText(QString::number(m_slider->value()) + m_suffix);
}

bool Pcsx2SliderRow::eventFilter(QObject* o, QEvent* e) {
    if (o == m_slider && e->type() == QEvent::FocusIn) emit focused(m_def);
    return QWidget::eventFilter(o, e);
}
