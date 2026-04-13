#include "pcsx2_slider_row.h"
#include "../pcsx2_theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QEvent>
#include <QKeyEvent>

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

void Pcsx2SliderRow::setEditing(bool on) {
    m_editing = on;
    m_slider->setProperty("editing", on);
    // Visual feedback: amber border when editing.
    if (on) {
        m_slider->setStyleSheet(
            Pcsx2Theme::sliderQss() +
            QStringLiteral("QSlider { border: 1px solid #f59e0b; border-radius: 4px; }"));
    } else {
        m_slider->setStyleSheet(Pcsx2Theme::sliderQss());
    }
}

bool Pcsx2SliderRow::eventFilter(QObject* o, QEvent* e) {
    if (o == m_slider && e->type() == QEvent::FocusIn) {
        emit focused(m_def);
    }
    if (o == m_slider && e->type() == QEvent::FocusOut) {
        setEditing(false);
    }
    if (o == m_slider && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            // Don't allow editing if the slider's dependency is inactive.
            if (!m_editing && property("dependencyActive").isValid()
                && !property("dependencyActive").toBool()) {
                return true;
            }
            setEditing(!m_editing);
            return true;
        }
        // When not editing, block arrow keys so spatial nav handles them.
        if (!m_editing) {
            if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down ||
                ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right) {
                return false; // let page-level event filter handle spatial nav
            }
        }
    }
    return QWidget::eventFilter(o, e);
}

void Pcsx2SliderRow::enterEvent(QEnterEvent* e) {
    QWidget::enterEvent(e);
    emit focused(m_def);
}
