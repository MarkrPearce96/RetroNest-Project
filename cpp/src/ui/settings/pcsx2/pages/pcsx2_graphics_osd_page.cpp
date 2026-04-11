#include "pcsx2_graphics_osd_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../widgets/pcsx2_slider_row.h"
#include "../widgets/pcsx2_osd_preview.h"
#include "../widgets/pcsx2_toggle.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QVariantMap>
#include <QScrollArea>
#include <QFrame>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QAbstractItemView>
#include <limits>

Pcsx2GraphicsOsdPage::Pcsx2GraphicsOsdPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "OSD")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    syncPreview();
    qApp->installEventFilter(this);
}

Pcsx2GraphicsOsdPage::~Pcsx2GraphicsOsdPage() {
    qApp->removeEventFilter(this);
}

const SettingDef* Pcsx2GraphicsOsdPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2GraphicsOsdPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void Pcsx2GraphicsOsdPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }"
        "QScrollBar::handle:vertical { background: #706c66; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #7a7670; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::sub-page:vertical, QScrollBar::add-page:vertical { background: transparent; }");
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);
    buildLeftCompoundCard(topRow);
    buildRightPreviewCard(topRow);
    root->addLayout(topRow);

    buildBottomToggleGrid(root);
    root->addStretch();
}

void Pcsx2GraphicsOsdPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    Q_UNUSED(topRow);
}

void Pcsx2GraphicsOsdPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    Q_UNUSED(topRow);
}

void Pcsx2GraphicsOsdPage::buildBottomToggleGrid(QVBoxLayout* root) {
    Q_UNUSED(root);
}

void Pcsx2GraphicsOsdPage::loadValues() {}
void Pcsx2GraphicsOsdPage::syncPreview() {}

bool Pcsx2GraphicsOsdPage::eventFilter(QObject* obj, QEvent* e) {
    return QWidget::eventFilter(obj, e);
}

QList<QWidget*> Pcsx2GraphicsOsdPage::collectFocusables() const {
    return {};
}

QWidget* Pcsx2GraphicsOsdPage::findNextFocusSpatial(QWidget* /*current*/, int /*key*/) const {
    return nullptr;
}
