#include "duckstation_emulation_page.h"
#include "../duckstation_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "../../pcsx2/widgets/pcsx2_slider_row.h"
#include "ui/app_controller.h"
#include "adapters/duckstation_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>

DuckStationEmulationPage::DuckStationEmulationPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Emulation") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* DuckStationEmulationPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationEmulationPage::buildUi() {
    // Outer layout — just hosts the scroll area
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

    auto* back = new QPushButton("\u2190 Back", content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &DuckStationSettingsDialog::popPage);
    root->addWidget(back);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& v){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, v);
        });
        v->addWidget(row);
        return card;
    };
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };
    auto makeSliderCard = [this](const QString& key, int lo, int hi, const QString& suffix) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2SliderRow(card);
        row->setLabel(d->label);
        row->setRange(lo, hi);
        row->setSuffix(suffix);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row,  &Pcsx2SliderRow::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row,  &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, QString::number(val));
        });
        v->addWidget(row);
        return card;
    };

    // Speed Control
    root->addWidget(new Pcsx2SectionHeader("Speed Control", this));
    if (auto* c = makeComboCard("EmulationSpeed"))   root->addWidget(c);
    if (auto* c = makeComboCard("FastForwardSpeed")) root->addWidget(c);
    if (auto* c = makeComboCard("TurboSpeed"))       root->addWidget(c);

    // Latency Control
    root->addWidget(new Pcsx2SectionHeader("Latency Control", this));
    auto* latencyGrid = new QGridLayout();
    latencyGrid->setSpacing(10);
    int lgr = 0, lgc = 0;
    auto addLatencyToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        latencyGrid->addWidget(w, lgr, lgc);
        if (++lgc == 2) { lgc = 0; ++lgr; }
    };
    addLatencyToggle("VSync");
    addLatencyToggle("SyncToHostRefreshRate");
    addLatencyToggle("OptimalFramePacing");
    addLatencyToggle("PreFrameSleep");
    addLatencyToggle("SkipPresentingDuplicateFrames");
    root->addLayout(latencyGrid);

    // Rewind
    root->addWidget(new Pcsx2SectionHeader("Rewind", this));
    if (auto* c = makeToggleCard("RewindEnable"))                       root->addWidget(c);
    if (auto* c = makeToggleCard("UseSoftwareRendererForMemoryStates")) root->addWidget(c);
    if (auto* c = makeSliderCard("RewindFrequency", 0, 60, " s"))       root->addWidget(c);
    if (auto* c = makeSliderCard("RewindSaveSlots",  1, 50, ""))        root->addWidget(c);

    // Runahead
    root->addWidget(new Pcsx2SectionHeader("Runahead", this));
    if (auto* c = makeComboCard("RunaheadFrameCount"))     root->addWidget(c);
    if (auto* c = makeToggleCard("RunaheadForAnalogInput")) root->addWidget(c);

    root->addStretch();
}

void DuckStationEmulationPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        slider->setValue(static_cast<int>(v.toDouble()));
    }
}

void DuckStationEmulationPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
