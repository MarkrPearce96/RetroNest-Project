#include "duckstation_console_page.h"
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

DuckStationConsolePage::DuckStationConsolePage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Console") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* DuckStationConsolePage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationConsolePage::buildUi() {
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationConsolePage::settingFocused);
        connect(row,  &Pcsx2ComboRow::focused, this, &DuckStationConsolePage::settingFocused);
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationConsolePage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &DuckStationConsolePage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };
    auto addIfPresent = [](QBoxLayout* layout, QWidget* w){ if (w) layout->addWidget(w); };
    (void)addIfPresent;

    // Console
    root->addWidget(new Pcsx2SectionHeader("Console", this));
    if (auto* c = makeComboCard("Region"))          root->addWidget(c);
    if (auto* c = makeComboCard("ForceVideoTiming")) root->addWidget(c);

    auto* consoleGrid = new QGridLayout();
    consoleGrid->setSpacing(10);
    int cgr = 0, cgc = 0;
    auto addConsoleToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        consoleGrid->addWidget(w, cgr, cgc);
        if (++cgc == 2) { cgc = 0; ++cgr; }
    };
    addConsoleToggle("PatchFastBoot");
    addConsoleToggle("FastForwardBoot");
    addConsoleToggle("FastForwardAccess");
    addConsoleToggle("Enable8MBRAM");
    root->addLayout(consoleGrid);

    // CPU Emulation
    root->addWidget(new Pcsx2SectionHeader("CPU Emulation", this));
    if (auto* c = makeComboCard("ExecutionMode"))      root->addWidget(c);
    if (auto* c = makeComboCard("OverclockNumerator")) root->addWidget(c);

    auto* cpuGrid = new QGridLayout();
    cpuGrid->setSpacing(10);
    int cpur = 0, cpuc = 0;
    auto addCpuToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        cpuGrid->addWidget(w, cpur, cpuc);
        if (++cpuc == 2) { cpuc = 0; ++cpur; }
    };
    addCpuToggle("OverclockEnable");
    addCpuToggle("RecompilerICache");
    root->addLayout(cpuGrid);

    // CD-ROM Emulation
    root->addWidget(new Pcsx2SectionHeader("CD-ROM Emulation", this));
    if (auto* c = makeComboCard("ReadSpeedup")) root->addWidget(c);
    if (auto* c = makeComboCard("SeekSpeedup")) root->addWidget(c);

    auto* cdromGrid = new QGridLayout();
    cdromGrid->setSpacing(10);
    int cdr = 0, cdc = 0;
    auto addCdromToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        cdromGrid->addWidget(w, cdr, cdc);
        if (++cdc == 2) { cdc = 0; ++cdr; }
    };
    addCdromToggle("LoadImageToRAM");
    addCdromToggle("AutoDiscChange");
    addCdromToggle("LoadImagePatches");
    addCdromToggle("IgnoreHostSubcode");
    root->addLayout(cdromGrid);

    root->addStretch();
}

void DuckStationConsolePage::loadValues() {
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
}

void DuckStationConsolePage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
