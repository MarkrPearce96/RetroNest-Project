#include "pcsx2_graphics_rendering_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QVariantMap>

Pcsx2GraphicsRenderingPage::Pcsx2GraphicsRenderingPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "Rendering")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
}

const SettingDef* Pcsx2GraphicsRenderingPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2GraphicsRenderingPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        if (!d) return card;
        card->setSettingDef(*d);
        auto* row = new Pcsx2ComboRow(card, /*stacked=*/true);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };

    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        if (!d) return card;
        card->setSettingDef(*d);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    // Full-width Internal Resolution card
    root->addWidget(makeComboCard("upscale_multiplier"));

    // 2-column grid of 6 cards
    auto* grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeComboCard("filter"),                 0, 0);
    grid->addWidget(makeComboCard("TriFilter"),              0, 1);
    grid->addWidget(makeComboCard("MaxAnisotropy"),          1, 0);
    grid->addWidget(makeComboCard("dithering_ps2"),          1, 1);
    grid->addWidget(makeComboCard("accurate_blending_unit"), 2, 0);
    grid->addWidget(makeToggleCard("hw_mipmap"),             2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    root->addLayout(grid);
    root->addStretch();
}

void Pcsx2GraphicsRenderingPage::loadValues() {
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
}

void Pcsx2GraphicsRenderingPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
