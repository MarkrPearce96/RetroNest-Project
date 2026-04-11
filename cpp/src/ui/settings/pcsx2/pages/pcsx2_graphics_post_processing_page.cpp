#include "pcsx2_graphics_post_processing_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_section_header.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../widgets/pcsx2_slider_row.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QVariantMap>

Pcsx2GraphicsPostProcessingPage::Pcsx2GraphicsPostProcessingPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "Post-Processing")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    refreshDependencies();
}

const SettingDef* Pcsx2GraphicsPostProcessingPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

bool Pcsx2GraphicsPostProcessingPage::masterToggleState(const QString& masterKey) const {
    auto it = m_masterToggles.find(masterKey);
    if (it == m_masterToggles.end()) return true;
    return it.value()->isChecked();
}

void Pcsx2GraphicsPostProcessingPage::refreshDependencies() {
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        if (d.dependsOn.isEmpty()) continue;
        const bool enabled = masterToggleState(d.dependsOn);
        // Disable the slider row widget itself — Qt's default disabled-state
        // rendering greys out its internal QLabel + QSlider + value label.
        slider->setEnabled(enabled);
    }
}

void Pcsx2GraphicsPostProcessingPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(10);

    auto makeComboRow = [this](const QString& key) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ComboRow(this);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2GraphicsPostProcessingPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        return row;
    };

    auto makeToggleRow = [this](const QString& key) -> Pcsx2ToggleRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ToggleRow(this);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsPostProcessingPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            refreshDependencies();
        });
        m_masterToggles.insert(key, row);
        return row;
    };

    auto makeSliderRow = [this](const QString& key) -> Pcsx2SliderRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2SliderRow(this);
        row->setLabel(d->label);
        row->setRange(int(d->minVal), int(d->maxVal));
        row->setSuffix(d->suffix);
        row->setSettingDef(*d);
        connect(row, &Pcsx2SliderRow::focused, this, &Pcsx2GraphicsPostProcessingPage::settingFocused);
        connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
        });
        return row;
    };

    // Section 1: Sharpening / Anti-Aliasing
    root->addWidget(new Pcsx2SectionHeader("Sharpening / Anti-Aliasing", this));

    auto* sharpCard = new Pcsx2Card(this);
    auto* sharpV = new QVBoxLayout(sharpCard);
    sharpV->setContentsMargins(14, 12, 14, 12);
    sharpV->setSpacing(8);

    if (auto* casCombo = makeComboRow("CASMode"))
        sharpV->addWidget(casCombo);

    auto* sharpRow = new QHBoxLayout();
    sharpRow->setSpacing(16);
    if (const SettingDef* ds = findDef("CASSharpness")) {
        auto* slider = new Pcsx2SliderRow(sharpCard);
        slider->setLabel(ds->label);
        slider->setRange(int(ds->minVal), int(ds->maxVal));
        slider->setSuffix(ds->suffix);
        slider->setSettingDef(*ds);
        connect(slider, &Pcsx2SliderRow::focused, this, &Pcsx2GraphicsPostProcessingPage::settingFocused);
        connect(slider, &Pcsx2SliderRow::valueChanged, this, [this](int val){
            const SettingDef* dd = findDef("CASSharpness");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
        });
        sharpRow->addWidget(slider, 1);
    }
    if (auto* fxaaToggle = makeToggleRow("fxaa"))
        sharpRow->addWidget(fxaaToggle, 0);

    sharpV->addLayout(sharpRow);
    root->addWidget(sharpCard);

    // Section 2: Filters
    root->addWidget(new Pcsx2SectionHeader("Filters", this));

    auto* filterCard = new Pcsx2Card(this);
    auto* filterV = new QVBoxLayout(filterCard);
    filterV->setContentsMargins(14, 12, 14, 12);
    filterV->setSpacing(10);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(16);
    if (auto* tv = makeComboRow("TVShader")) topRow->addWidget(tv, 1);
    if (auto* sb = makeToggleRow("ShadeBoost")) topRow->addWidget(sb, 0);
    filterV->addLayout(topRow);

    auto* sliderGrid = new QGridLayout();
    sliderGrid->setSpacing(10);
    sliderGrid->setContentsMargins(0, 6, 0, 0);
    if (auto* r = makeSliderRow("ShadeBoost_Brightness")) sliderGrid->addWidget(r, 0, 0);
    if (auto* r = makeSliderRow("ShadeBoost_Contrast"))   sliderGrid->addWidget(r, 0, 1);
    if (auto* r = makeSliderRow("ShadeBoost_Saturation")) sliderGrid->addWidget(r, 1, 0);
    if (auto* r = makeSliderRow("ShadeBoost_Gamma"))      sliderGrid->addWidget(r, 1, 1);
    sliderGrid->setColumnStretch(0, 1);
    sliderGrid->setColumnStretch(1, 1);
    filterV->addLayout(sliderGrid);

    root->addWidget(filterCard);
    root->addStretch();
}

void Pcsx2GraphicsPostProcessingPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* tog : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = tog->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        tog->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        bool ok = false;
        int v = cur.toInt(&ok);
        if (!ok) v = d.defaultValue.toInt();
        slider->setValue(v);
    }

    refreshDependencies();
}

void Pcsx2GraphicsPostProcessingPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
