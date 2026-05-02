#include "ppsspp_graphics_pacing_fx_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QVariantMap>

PpssppGraphicsPacingFxPage::PpssppGraphicsPacingFxPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Graphics" &&
            (d.subcategory == "Frame Pacing" || d.subcategory == "Post-Processing"))
            m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppGraphicsPacingFxPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppGraphicsPacingFxPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

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
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsPacingFxPage::settingFocused);
        connect(row, &Pcsx2ComboRow::focused, this, &PpssppGraphicsPacingFxPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, val);
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
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsPacingFxPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppGraphicsPacingFxPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new Pcsx2SectionHeader("Frame Pacing", this));
    if (auto* c = makeToggleCard("VerticalSync"))           root->addWidget(c);
    if (auto* c = makeComboCard ("FrameSkip"))              root->addWidget(c);
    if (auto* c = makeToggleCard("AutoFrameSkip"))          root->addWidget(c);
    if (auto* c = makeComboCard ("FrameRate"))              root->addWidget(c);
    if (auto* c = makeComboCard ("FrameRate2"))             root->addWidget(c);
    if (auto* c = makeToggleCard("RenderDuplicateFrames"))  root->addWidget(c);

    root->addWidget(new Pcsx2SectionHeader("Post-Processing", this));
    if (auto* c = makeComboCard("PostShader1")) root->addWidget(c);

    root->addStretch();
}

void PpssppGraphicsPacingFxPage::loadValues() {
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

void PpssppGraphicsPacingFxPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
