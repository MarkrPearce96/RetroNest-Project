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
    auto* card = new Pcsx2Card(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setMinimumHeight(460);

    if (const SettingDef* perfDef = findDef("OsdPerformancePos"))
        card->setSettingDef(*perfDef);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);

    auto addMiniHeader = [&](const QString& text) {
        auto* hdr = new QLabel(text, card);
        hdr->setStyleSheet(
            "color:#f59e0b; font-size:11px; font-weight:700;"
            "letter-spacing:1.0px; padding:6px 0 2px 0;");
        v->addWidget(hdr);
    };

    auto addToggle = [&](const QString& key) {
        const SettingDef* d = findDef(key);
        if (!d) return;
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            if (!m_preview) return;
            if      (key == "OsdShowFPS")        m_preview->setShowFps(on);
            else if (key == "OsdShowSpeed")      m_preview->setShowSpeed(on);
            else if (key == "OsdShowVPS")        m_preview->setShowVps(on);
            else if (key == "OsdShowResolution") m_preview->setShowResolution(on);
            else if (key == "OsdShowCPU")        m_preview->setShowCpu(on);
            else if (key == "OsdShowGPU")        m_preview->setShowGpu(on);
            else if (key == "OsdShowSettings")   m_preview->setShowSettings(on);
            else if (key == "OsdshowPatches")    m_preview->setShowPatches(on);
            else if (key == "OsdShowInputs")     m_preview->setShowInputs(on);
        });
        v->addWidget(row);
    };

    addMiniHeader(QStringLiteral("PERFORMANCE STATS"));
    addToggle("OsdShowFPS");
    addToggle("OsdShowSpeed");
    addToggle("OsdShowGPU");
    addToggle("OsdShowCPU");
    addToggle("OsdShowResolution");
    addToggle("OsdShowVPS");

    addMiniHeader(QStringLiteral("SETTINGS & INPUTS"));
    addToggle("OsdshowPatches");
    addToggle("OsdShowSettings");
    addToggle("OsdShowInputs");

    v->addStretch();
    topRow->addWidget(card, 1);
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
