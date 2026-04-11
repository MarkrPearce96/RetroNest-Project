#include "pcsx2_audio_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_section_header.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../widgets/pcsx2_slider_row.h"
#include "../pcsx2_theme.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QVariantMap>

Pcsx2AudioPage::Pcsx2AudioPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Audio") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* Pcsx2AudioPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2AudioPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    auto* back = new QPushButton("\u2190 Back", this);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &Pcsx2SettingsDialog::popPage);
    root->addWidget(back);

    auto makeComboRow = [this](const QString& key) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ComboRow(this);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2AudioPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& v){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, v);
        });
        return row;
    };
    auto makeSliderCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2SliderRow(card);
        row->setLabel(d->label);
        row->setRange(int(d->minVal), int(d->maxVal));
        row->setSuffix(d->suffix);
        row->setSettingDef(*d);
        connect(row, &Pcsx2SliderRow::focused, this, &Pcsx2AudioPage::settingFocused);
        connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, QString::number(val));
        });
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2AudioPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };
    auto addIfPresent = [](QBoxLayout* layout, QWidget* w){ if (w) layout->addWidget(w); };

    // Configuration
    root->addWidget(new Pcsx2SectionHeader("Configuration", this));

    auto* cfgCard = new Pcsx2Card(this);
    auto* cfgV = new QVBoxLayout(cfgCard);
    cfgV->setContentsMargins(14, 12, 14, 12);
    addIfPresent(cfgV, makeComboRow("Backend"));
    addIfPresent(cfgV, makeComboRow("ExpansionMode"));
    addIfPresent(cfgV, makeComboRow("SyncMode"));
    addIfPresent(cfgV, makeComboRow("DriverName"));
    root->addWidget(cfgCard);

    if (auto* bufCard = makeSliderCard("BufferMS")) root->addWidget(bufCard);

    auto* latRow = new QHBoxLayout();
    latRow->setSpacing(10);
    if (auto* latCard = makeSliderCard("OutputLatencyMS")) {
        latCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        latRow->addWidget(latCard, 1);
    }
    if (auto* minCard = makeToggleCard("OutputLatencyMinimal")) {
        minCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        latRow->addWidget(minCard, 0);
    }
    root->addLayout(latRow);

    // Volume Controls
    root->addWidget(new Pcsx2SectionHeader("Volume Controls", this));

    if (auto* stdCard = makeSliderCard("StandardVolume")) root->addWidget(stdCard);

    auto* volRow = new QHBoxLayout();
    volRow->setSpacing(10);
    if (auto* ffCard = makeSliderCard("FastForwardVolume")) {
        ffCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        volRow->addWidget(ffCard, 1);
    }
    if (auto* muteCard = makeToggleCard("OutputMuted")) {
        muteCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        volRow->addWidget(muteCard, 0);
    }
    root->addLayout(volRow);

    root->addStretch();
}

void Pcsx2AudioPage::loadValues() {
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
        slider->setValue(v.toInt());
    }
}

void Pcsx2AudioPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
