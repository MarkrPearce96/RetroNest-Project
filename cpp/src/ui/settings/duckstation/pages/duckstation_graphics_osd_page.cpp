#include "duckstation_graphics_osd_page.h"
#include "../duckstation_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "../../pcsx2/widgets/pcsx2_slider_row.h"
#include "../widgets/duckstation_osd_preview.h"
#include "ui/app_controller.h"
#include "adapters/duckstation_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVariantMap>
#include <QFrame>

DuckStationGraphicsOsdPage::DuckStationGraphicsOsdPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "On-Screen Display") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* DuckStationGraphicsOsdPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationGraphicsOsdPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void DuckStationGraphicsOsdPage::buildUi() {
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
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    // ── Helper: slider card (int) ─────────────────────────────────────────
    auto makeSliderCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2SliderRow(card);
        row->setLabel(d->label);
        row->setRange(int(d->minVal), int(d->maxVal));
        row->setSuffix(d->suffix);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2SliderRow::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val) {
            const SettingDef* d2 = findDef(key);
            if (!d2) return;
            saveValue(d2->section, d2->key, QString::number(val));
            if (!m_preview) return;
            if (key == "OSDScale")  m_preview->setScale(val);
            if (key == "OSDMargin") m_preview->setMargin(val);
        });
        v->addWidget(row);
        return card;
    };

    // ── Helper: toggle card ───────────────────────────────────────────────
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* d2 = findDef(key);
            if (d2) saveValue(d2->section, d2->key, on ? "true" : "false");
            if (!m_preview) return;
            if      (key == "ShowFPS")              m_preview->setShowFPS(on);
            else if (key == "ShowSpeed")            m_preview->setShowSpeed(on);
            else if (key == "ShowCPU")              m_preview->setShowCPU(on);
            else if (key == "ShowGPU")              m_preview->setShowGPU(on);
            else if (key == "ShowResolution")       m_preview->setShowResolution(on);
            else if (key == "ShowGPUStatistics")    m_preview->setShowGPUStatistics(on);
            else if (key == "ShowFrameTimes")       m_preview->setShowFrameTimes(on);
            else if (key == "ShowLatencyStatistics")m_preview->setShowLatencyStatistics(on);
            else if (key == "ShowInputs")           m_preview->setShowInputs(on);
            else if (key == "ShowEnhancements")     m_preview->setShowEnhancements(on);
            else if (key == "ShowOSDMessages")      m_preview->setShowOSDMessages(on);
            else if (key == "ShowStatusIndicators") m_preview->setShowStatusIndicators(on);
        });
        v->addWidget(row);
        return card;
    };

    // ── Preview card (full width) ─────────────────────────────────────────
    {
        auto* card = new Pcsx2Card(this);
        card->setFocusPolicy(Qt::NoFocus);
        card->setPreviewStyle(true);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        v->setSpacing(8);

        auto* lbl = new QLabel(QStringLiteral("OSD PREVIEW"), card);
        lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;letter-spacing:0.8px;");
        v->addWidget(lbl);

        m_preview = new DuckStationOsdPreview(card);
        v->addWidget(m_preview);
        root->addWidget(card);
    }

    // ── Section: Display ─────────────────────────────────────────────────
    root->addWidget(new Pcsx2SectionHeader("Display", this));
    if (auto* c = makeSliderCard("OSDScale"))  root->addWidget(c);
    if (auto* c = makeSliderCard("OSDMargin")) root->addWidget(c);

    // ── Section: Visibility (toggle grid) ────────────────────────────────
    root->addWidget(new Pcsx2SectionHeader("Visibility", this));

    // Bool keys from the "On-Screen Display" → "Overlays" and "Messages" groups
    static const char* kToggleKeys[] = {
        "ShowFPS", "ShowSpeed", "ShowCPU", "ShowGPU",
        "ShowResolution", "ShowGPUStatistics", "ShowFrameTimes", "ShowLatencyStatistics",
        "ShowInputs", "ShowEnhancements", "ShowOSDMessages", "ShowStatusIndicators",
        nullptr
    };

    auto* grid = new QGridLayout();
    grid->setSpacing(10);
    int gr = 0, gc = 0;
    for (const char** kp = kToggleKeys; *kp; ++kp) {
        auto* w = makeToggleCard(QString::fromLatin1(*kp));
        if (!w) continue;
        grid->addWidget(w, gr, gc);
        if (++gc == 3) { gc = 0; ++gr; }
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    root->addLayout(grid);

    root->addStretch();
}

void DuckStationGraphicsOsdPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* row : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setValue(v.toInt());
    }

    // Sync preview with loaded values
    if (!m_preview) return;
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const QString& key = row->settingDef().key;
        const bool on = row->isChecked();
        if      (key == "ShowFPS")               m_preview->setShowFPS(on);
        else if (key == "ShowSpeed")             m_preview->setShowSpeed(on);
        else if (key == "ShowCPU")               m_preview->setShowCPU(on);
        else if (key == "ShowGPU")               m_preview->setShowGPU(on);
        else if (key == "ShowResolution")        m_preview->setShowResolution(on);
        else if (key == "ShowGPUStatistics")     m_preview->setShowGPUStatistics(on);
        else if (key == "ShowFrameTimes")        m_preview->setShowFrameTimes(on);
        else if (key == "ShowLatencyStatistics") m_preview->setShowLatencyStatistics(on);
        else if (key == "ShowInputs")            m_preview->setShowInputs(on);
        else if (key == "ShowEnhancements")      m_preview->setShowEnhancements(on);
        else if (key == "ShowOSDMessages")       m_preview->setShowOSDMessages(on);
        else if (key == "ShowStatusIndicators")  m_preview->setShowStatusIndicators(on);
    }
    for (auto* row : findChildren<Pcsx2SliderRow*>()) {
        const QString& key = row->settingDef().key;
        if (key == "OSDScale")  m_preview->setScale(row->value());
        if (key == "OSDMargin") m_preview->setMargin(row->value());
    }
}
