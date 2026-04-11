#include "pcsx2_combo_row.h"
#include "../pcsx2_theme.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QStyleFactory>
#include <QListView>

Pcsx2ComboRow::Pcsx2ComboRow(QWidget* parent) : QWidget(parent) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color:#d0ccc4;font-size:13px;");
    m_label->setMinimumWidth(180);
    m_label->setMinimumHeight(24);
    m_combo = new QComboBox(this);
    // Force Fusion style on the combo so QSS rules like ::item:selected
    // actually apply — macOS's native style path ignores most pseudo-states.
    if (auto* fusion = QStyleFactory::create("Fusion")) {
        m_combo->setStyle(fusion);
    }
    // Replace the native view with a plain QListView we control directly,
    // ensuring our item stylesheet isn't filtered by the native style.
    auto* listView = new QListView(m_combo);
    if (auto* fusion = QStyleFactory::create("Fusion")) {
        listView->setStyle(fusion);
    }
    m_combo->setView(listView);
    m_combo->setStyleSheet(Pcsx2Theme::comboQss());
    m_combo->setMinimumWidth(200);
    // Style the popup container (QComboBoxPrivateContainer) directly so
    // Qt's default frame background doesn't show as black bars top/bottom.
    // Accessing view() forces lazy creation of the view + its parent container.
    if (QWidget* container = m_combo->view()->parentWidget()) {
        // Target the container class only — do NOT use a generic QFrame
        // selector because the inner QListView is also a QFrame and
        // would inherit these rules, clobbering its own selection colors.
        container->setStyleSheet(
            "QComboBoxPrivateContainer {"
            "  background-color: #585450;"
            "  border: 1px solid #706c66;"
            "  border-radius: 8px;"
            "}");
    }

    // Apply item styling directly on the view so it's not overridden by
    // any parent cascade. This gives us a visible amber highlight on the
    // current/selected item during keyboard navigation.
    if (auto* view = m_combo->view()) {
        view->setStyleSheet(
            "QAbstractItemView {"
            "  background-color: #585450;"
            "  color: #f2efe8;"
            "  border: none;"
            "  outline: none;"
            "  padding: 4px;"
            "  selection-background-color: #f59e0b;"
            "  selection-color: #1a1816;"
            "}"
            "QAbstractItemView::item {"
            "  padding: 6px 12px;"
            "  min-height: 28px;"
            "  border-radius: 4px;"
            "  color: #f2efe8;"
            "}"
            "QAbstractItemView::item:selected {"
            "  background-color: #f59e0b;"
            "  color: #1a1816;"
            "}"
            "QAbstractItemView::item:hover {"
            "  background-color: #6c6860;"
            "  color: #f2efe8;"
            "}");
    }
    // Strip the QListView frame + macOS focus rect so no thin lines
    // appear at the top and bottom of the popup.
    if (auto* view = m_combo->view()) {
        if (auto* frame = qobject_cast<QFrame*>(view)) {
            frame->setFrameShape(QFrame::NoFrame);
        }
        view->setAttribute(Qt::WA_MacShowFocusRect, false);
        view->setContentsMargins(0, 0, 0, 0);
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
