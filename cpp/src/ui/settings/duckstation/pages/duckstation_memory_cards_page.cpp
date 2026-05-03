#include "duckstation_memory_cards_page.h"
#include "../duckstation_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/widgets/settings_section_header.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
#include "adapters/duckstation_adapter.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QVariantMap>

DuckStationMemoryCardsPage::DuckStationMemoryCardsPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Memory Cards") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* DuckStationMemoryCardsPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationMemoryCardsPage::buildUi() {
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
    connect(back, &QPushButton::clicked, m_dialog, &DuckStationSettingsDialog::popPage);
    root->addWidget(back);

    // Helper: make a slot card with a combo row for card type and a read-only path label
    auto makeSlotCard = [this](const QString& typeKey, const QString& pathKey,
                               const QString& title, QLabel** outPathLabel) -> SettingsCard* {
        const SettingDef* td = findDef(typeKey);
        const SettingDef* pd = findDef(pathKey);
        if (!td || !pd) return nullptr;

        auto* card = new SettingsCard(this);
        card->setSettingDef(*td);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        v->setSpacing(8);

        auto* header = new QLabel(title, card);
        header->setStyleSheet("color:#f2efe8;font-size:14px;font-weight:bold;");
        v->addWidget(header);

        auto* row = new SettingsComboRow(card);
        row->setLabel(td->label);
        row->setOptions(td->options);
        row->setSettingDef(*td);
        connect(card, &SettingsCard::focused, this, &DuckStationMemoryCardsPage::settingFocused);
        connect(row,  &SettingsComboRow::focused, this, &DuckStationMemoryCardsPage::settingFocused);
        connect(row,  &SettingsComboRow::valueChanged, this, [this, typeKey](const QString& val) {
            if (const SettingDef* d2 = findDef(typeKey)) saveValue(d2->section, d2->key, val);
        });
        v->addWidget(row);

        auto* pathLabel = new QLabel(card);
        pathLabel->setStyleSheet("color:#c4c0b8;font-size:13px;");
        *outPathLabel = pathLabel;
        v->addWidget(pathLabel);

        return card;
    };

    // Slot 1
    root->addWidget(new SettingsSectionHeader("Memory Card Slots", this));
    if (auto* c = makeSlotCard("Card1Type", "Card1Path", "Slot 1", &m_slot1PathLabel))
        root->addWidget(c);

    // Slot 2
    if (auto* c = makeSlotCard("Card2Type", "Card2Path", "Slot 2", &m_slot2PathLabel))
        root->addWidget(c);

    // Additional toggle options
    SettingsPageBuilder builder(this, m_schema,
        [this](const QString& sec, const QString& k, const QString& v){ saveValue(sec, k, v); },
        [this](const SettingDef& d){ emit settingFocused(d); });
    auto makeToggleCard = [&builder](const QString& key){ return builder.makeToggleCard(key); };

    root->addWidget(new SettingsSectionHeader("Game-Specific Settings", this));
    if (auto* c = makeToggleCard("UsePlaylistTitle")) root->addWidget(c);

    root->addStretch();
}

void DuckStationMemoryCardsPage::loadValues() {
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

    if (m_slot1PathLabel) {
        const SettingDef* d = findDef("Card1Path");
        if (d) {
            QString cur = app->settingValue(emuId, d->section, d->key);
            m_slot1PathLabel->setText(cur.isEmpty() ? d->defaultValue : cur);
        }
    }
    if (m_slot2PathLabel) {
        const SettingDef* d = findDef("Card2Path");
        if (d) {
            QString cur = app->settingValue(emuId, d->section, d->key);
            m_slot2PathLabel->setText(cur.isEmpty() ? d->defaultValue : cur);
        }
    }
}

void DuckStationMemoryCardsPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
