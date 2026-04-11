#include "pcsx2_combo_row.h"
#include "../pcsx2_theme.h"
#include <QAbstractItemView>
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
    // Style the popup container (QComboBoxPrivateContainer) directly so
    // Qt's default frame background doesn't show as black bars top/bottom.
    // Accessing view() forces lazy creation of the view + its parent container.
    if (QWidget* container = m_combo->view()->parentWidget()) {
        container->setStyleSheet(
            "QComboBoxPrivateContainer, QFrame {"
            "  background-color: #585450;"
            "  border: 1px solid #706c66;"
            "  border-radius: 8px;"
            "}");
    }
    // Watch for the popup hiding so we can return focus to the parent
    // Pcsx2Card (otherwise arrow keys get trapped on the combo itself).
    m_combo->view()->installEventFilter(this);
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
    if (obj == m_combo->view() && e->type() == QEvent::Hide) {
        // Popup dismissed — walk up the parent chain to find the Pcsx2Card
        // ancestor and return focus to it so arrow keys resume card nav.
        QWidget* w = parentWidget();
        while (w) {
            // Use the className check (not the type) to avoid importing
            // the Pcsx2Card header into this row's implementation.
            if (QString::fromLatin1(w->metaObject()->className()) == QLatin1String("Pcsx2Card")) {
                w->setFocus(Qt::OtherFocusReason);
                break;
            }
            w = w->parentWidget();
        }
    }
    return QWidget::eventFilter(obj, e);
}

void Pcsx2ComboRow::enterEvent(QEnterEvent* e) {
    QWidget::enterEvent(e);
    emit focused(m_def);
}
