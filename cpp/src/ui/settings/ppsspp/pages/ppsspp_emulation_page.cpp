#include "ppsspp_emulation_page.h"
#include "../ppsspp_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_section_header.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/widgets/settings_slider_row.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>

PpssppEmulationPage::PpssppEmulationPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Emulation") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppEmulationPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppEmulationPage::buildUi() {
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
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    auto* back = new QPushButton(QString::fromUtf8("\xE2\x86\x90 Back"), content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &PpssppSettingsDialog::popPage);
    root->addWidget(back);

    SettingsPageBuilder builder(this, m_schema,
        [this](const QString& sec, const QString& k, const QString& v){ saveValue(sec, k, v); },
        [this](const SettingDef& d){ emit settingFocused(d); });
    auto makeComboCard  = [&builder](const QString& key){ return builder.makeComboCard(key); };
    auto makeSliderCard = [&builder](const QString& key){ return builder.makeSliderCard(key); };
    auto makeToggleCard = [&builder](const QString& key){ return builder.makeToggleCard(key); };

    root->addWidget(new SettingsSectionHeader("CPU & Memory", this));
    if (auto* c = makeToggleCard("FastMemoryAccess"))   root->addWidget(c);
    if (auto* c = makeToggleCard("IgnoreBadMemAccess")) root->addWidget(c);
    if (auto* c = makeComboCard ("IOTimingMethod"))     root->addWidget(c);
    if (auto* c = makeToggleCard("ForceLagSync2"))      root->addWidget(c);
    if (auto* c = makeSliderCard("CPUSpeed"))           root->addWidget(c);

    root->addStretch();
}

void PpssppEmulationPage::loadValues() {
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
    for (auto* row : findChildren<SettingsSliderRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setValue(v.toInt());
    }
}

void PpssppEmulationPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
