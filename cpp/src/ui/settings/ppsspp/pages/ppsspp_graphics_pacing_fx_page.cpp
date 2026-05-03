#include "ppsspp_graphics_pacing_fx_page.h"
#include "../ppsspp_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_section_header.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
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
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(SettingsPageBuilder::kScrollAreaQss);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    SettingsPageBuilder builder(this, m_schema,
        [this](const QString& sec, const QString& k, const QString& v){ saveValue(sec, k, v); },
        [this](const SettingDef& d){ emit settingFocused(d); });
    auto makeComboCard  = [&builder](const QString& key){ return builder.makeComboCard(key); };
    auto makeToggleCard = [&builder](const QString& key){ return builder.makeToggleCard(key); };

    root->addWidget(new SettingsSectionHeader("Frame Pacing", this));
    if (auto* c = makeToggleCard("VerticalSync"))           root->addWidget(c);
    if (auto* c = makeComboCard ("FrameSkip"))              root->addWidget(c);
    if (auto* c = makeToggleCard("AutoFrameSkip"))          root->addWidget(c);
    if (auto* c = makeComboCard ("FrameRate"))              root->addWidget(c);
    if (auto* c = makeComboCard ("FrameRate2"))             root->addWidget(c);
    if (auto* c = makeToggleCard("RenderDuplicateFrames"))  root->addWidget(c);

    root->addWidget(new SettingsSectionHeader("Post-Processing", this));
    if (auto* c = makeComboCard("PostShader1")) root->addWidget(c);

    root->addStretch();
}

void PpssppGraphicsPacingFxPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<SettingsToggleRow*>()) {
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
