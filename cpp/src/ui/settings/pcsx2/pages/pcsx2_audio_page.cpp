#include "pcsx2_audio_page.h"
#include "../pcsx2_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_section_header.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/widgets/settings_slider_row.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
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
    // Outer layout — just hosts the scroll area
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

    auto* back = new QPushButton("\u2190 Back", content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &Pcsx2SettingsDialog::popPage);
    root->addWidget(back);

    SettingsPageBuilder builder(this, m_schema,
        [this](const QString& sec, const QString& k, const QString& v){ saveValue(sec, k, v); },
        [this](const SettingDef& d){ emit settingFocused(d); });
    auto makeComboCard  = [&builder](const QString& key){ return builder.makeComboCard(key); };
    auto makeSliderCard = [&builder](const QString& key){ return builder.makeSliderCard(key); };
    auto makeToggleCard = [&builder](const QString& key){ return builder.makeToggleCard(key); };

    // Configuration
    root->addWidget(new SettingsSectionHeader("Configuration", this));
    if (auto* c = makeComboCard("Backend"))       root->addWidget(c);
    if (auto* c = makeComboCard("ExpansionMode")) root->addWidget(c);
    if (auto* c = makeComboCard("SyncMode"))      root->addWidget(c);
    if (auto* c = makeComboCard("DriverName"))    root->addWidget(c);

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
    root->addWidget(new SettingsSectionHeader("Volume Controls", this));

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
    for (auto* slider : findChildren<SettingsSliderRow*>()) {
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
