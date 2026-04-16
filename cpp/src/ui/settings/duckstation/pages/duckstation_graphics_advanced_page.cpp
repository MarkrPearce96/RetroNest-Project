#include "duckstation_graphics_advanced_page.h"
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
#include <QScrollArea>
#include <QVariantMap>

DuckStationGraphicsAdvancedPage::DuckStationGraphicsAdvancedPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Graphics" && d.subcategory == "Advanced") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* DuckStationGraphicsAdvancedPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationGraphicsAdvancedPage::buildUi() {
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

    // ── Lambda helpers ────────────────────────────────────────────────────
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, val);
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

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
        connect(card, &Pcsx2Card::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row, &Pcsx2SliderRow::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, QString::number(val));
        });
        v->addWidget(row);
        return card;
    };

    // ── Section: Display Options ──────────────────────────────────────────
    root->addWidget(new Pcsx2SectionHeader("Display Options", this));

    if (auto* c = makeComboCard("Alignment"))   root->addWidget(c);
    if (auto* c = makeComboCard("Rotation"))    root->addWidget(c);
    if (auto* c = makeComboCard("FineCropMode")) root->addWidget(c);
    if (auto* c = makeSliderCard("FineCropLeft"))   root->addWidget(c);
    if (auto* c = makeSliderCard("FineCropTop"))    root->addWidget(c);
    if (auto* c = makeSliderCard("FineCropRight"))  root->addWidget(c);
    if (auto* c = makeSliderCard("FineCropBottom")) root->addWidget(c);
    if (auto* c = makeToggleCard("DisableMailboxPresentation")) root->addWidget(c);

    // ── Section: Rendering Options ────────────────────────────────────────
    root->addWidget(new Pcsx2SectionHeader("Rendering Options", this));

    if (auto* c = makeComboCard("Multisamples"))   root->addWidget(c);
    if (auto* c = makeComboCard("LineDetectMode")) root->addWidget(c);

    auto* renderGrid = new QGridLayout();
    renderGrid->setSpacing(10);
    int rgr = 0, rgc = 0;
    auto addRenderCell = [&](QWidget* w){
        if (!w) return;
        renderGrid->addWidget(w, rgr, rgc);
        if (++rgc == 2) { rgc = 0; ++rgr; }
    };

    addRenderCell(makeToggleCard("UseThread"));
    addRenderCell(makeSliderCard("MaxQueuedFrames"));
    addRenderCell(makeToggleCard("EnableModulationCrop"));
    addRenderCell(makeToggleCard("ScaledInterlacing"));
    addRenderCell(makeToggleCard("UseSoftwareRendererForReadbacks"));

    root->addLayout(renderGrid);
    root->addStretch();
}

void DuckStationGraphicsAdvancedPage::loadValues() {
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
    for (auto* row : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setValue(v.toInt());
    }
}

void DuckStationGraphicsAdvancedPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
