#include "ppsspp_audio_page.h"
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

PpssppAudioPage::PpssppAudioPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Audio") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppAudioPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppAudioPage::buildUi() {
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

    // Audio playback
    root->addWidget(new SettingsSectionHeader("Audio Playback", this));
    if (auto* c = makeComboCard ("AudioSyncMode")) root->addWidget(c);
    if (auto* c = makeToggleCard("FillAudioGaps")) root->addWidget(c);

    // Game volume
    root->addWidget(new SettingsSectionHeader("Game Volume", this));
    if (auto* c = makeToggleCard("Enable"))                  root->addWidget(c);
    if (auto* c = makeSliderCard("GameVolume"))              root->addWidget(c);
    if (auto* c = makeSliderCard("ReverbRelativeVolume"))    root->addWidget(c);
    if (auto* c = makeSliderCard("AltSpeedRelativeVolume"))  root->addWidget(c);
    if (auto* c = makeSliderCard("AchievementVolume"))       root->addWidget(c);

    // UI sound
    root->addWidget(new SettingsSectionHeader("UI Sound", this));
    if (auto* c = makeToggleCard("UISound"))           root->addWidget(c);
    if (auto* c = makeSliderCard("UIVolume"))          root->addWidget(c);
    if (auto* c = makeSliderCard("GamePreviewVolume")) root->addWidget(c);

    // Audio backend
    root->addWidget(new SettingsSectionHeader("Audio Backend", this));
    if (auto* c = makeSliderCard("AudioBufferSize")) root->addWidget(c);
    if (auto* c = makeToggleCard("AutoAudioDevice")) root->addWidget(c);

    root->addStretch();
}

void PpssppAudioPage::loadValues() {
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

void PpssppAudioPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
