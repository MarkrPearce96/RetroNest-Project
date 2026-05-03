#include "ppsspp_graphics_textures_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QVariantMap>

PpssppGraphicsTexturesPage::PpssppGraphicsTexturesPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Graphics" && d.subcategory == "Textures") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppGraphicsTexturesPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppGraphicsTexturesPage::buildUi() {
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
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsTexturesPage::settingFocused);
        connect(row, &Pcsx2ComboRow::focused, this, &PpssppGraphicsTexturesPage::settingFocused);
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
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsTexturesPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppGraphicsTexturesPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new Pcsx2SectionHeader("Texture Upscaling", this));
    if (auto* c = makeToggleCard("TexHardwareScaling")) root->addWidget(c);
    if (auto* c = makeComboCard ("TexScalingType"))     root->addWidget(c);
    if (auto* c = makeComboCard ("TexScalingLevel"))    root->addWidget(c);
    if (auto* c = makeToggleCard("TexDeposterize"))     root->addWidget(c);

    root->addWidget(new Pcsx2SectionHeader("Filtering", this));
    if (auto* c = makeComboCard ("AnisotropyLevel"))     root->addWidget(c);
    if (auto* c = makeComboCard ("TextureFiltering"))    root->addWidget(c);
    if (auto* c = makeToggleCard("Smart2DTexFiltering")) root->addWidget(c);

    root->addStretch();
}

void PpssppGraphicsTexturesPage::loadValues() {
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

void PpssppGraphicsTexturesPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
