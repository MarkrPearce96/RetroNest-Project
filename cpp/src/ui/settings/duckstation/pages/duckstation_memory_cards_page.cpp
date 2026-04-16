#include "duckstation_memory_cards_page.h"
#include "../duckstation_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "ui/app_controller.h"
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
                               const QString& title, QLabel** outPathLabel) -> Pcsx2Card* {
        const SettingDef* td = findDef(typeKey);
        const SettingDef* pd = findDef(pathKey);
        if (!td || !pd) return nullptr;

        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*td);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        v->setSpacing(8);

        auto* header = new QLabel(title, card);
        header->setStyleSheet("color:#f2efe8;font-size:14px;font-weight:bold;");
        v->addWidget(header);

        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(td->label);
        row->setOptions(td->options);
        row->setSettingDef(*td);
        connect(card, &Pcsx2Card::focused, this, &DuckStationMemoryCardsPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::focused, this, &DuckStationMemoryCardsPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::valueChanged, this, [this, typeKey](const QString& val) {
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
    root->addWidget(new Pcsx2SectionHeader("Memory Card Slots", this));
    if (auto* c = makeSlotCard("Card1Type", "Card1Path", "Slot 1", &m_slot1PathLabel))
        root->addWidget(c);

    // Slot 2
    if (auto* c = makeSlotCard("Card2Type", "Card2Path", "Slot 2", &m_slot2PathLabel))
        root->addWidget(c);

    // Additional toggle options
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationMemoryCardsPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &DuckStationMemoryCardsPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new Pcsx2SectionHeader("Game-Specific Settings", this));
    if (auto* c = makeToggleCard("UsePlaylistTitle")) root->addWidget(c);

    root->addStretch();
}

void DuckStationMemoryCardsPage::loadValues() {
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
