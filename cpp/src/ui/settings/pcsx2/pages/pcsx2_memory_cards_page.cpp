#include "pcsx2_memory_cards_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_section_header.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../pcsx2_theme.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollArea>
#include <QVariantMap>

Pcsx2MemoryCardsPage::Pcsx2MemoryCardsPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Memory Cards") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* Pcsx2MemoryCardsPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2MemoryCardsPage::buildUi() {
    // Outer layout — just hosts the scroll area
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
    connect(back, &QPushButton::clicked, m_dialog, &Pcsx2SettingsDialog::popPage);
    root->addWidget(back);

    auto makeToggleRow = [this](const QString& key, const QString& labelOverride = QString()) -> Pcsx2ToggleRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ToggleRow(this);
        row->setLabel(labelOverride.isEmpty() ? d->label : labelOverride);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2MemoryCardsPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
        });
        return row;
    };

    auto makeToggleCard = [this, &makeToggleRow](const QString& key, const QString& labelOverride = QString()) -> Pcsx2Card* {
        auto* tr = makeToggleRow(key, labelOverride);
        if (!tr) return nullptr;
        auto* card = new Pcsx2Card(this);
        if (const SettingDef* d = findDef(key)) card->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &Pcsx2MemoryCardsPage::settingFocused);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        tr->setParent(card);
        v->addWidget(tr);
        return card;
    };

    auto makeSlotCard = [this, &makeToggleRow](const QString& enableKey, const QString& filenameKey,
                                               const QString& label, QLabel** outFilenameLabel) -> Pcsx2Card* {
        const SettingDef* fd = findDef(filenameKey);
        if (!fd) return nullptr;
        auto* card = new Pcsx2Card(this);
        if (const SettingDef* ed = findDef(enableKey)) card->setSettingDef(*ed);
        connect(card, &Pcsx2Card::focused, this, &Pcsx2MemoryCardsPage::settingFocused);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        v->setSpacing(8);

        auto* tr = makeToggleRow(enableKey, label);
        if (tr) { tr->setParent(card); v->addWidget(tr); }

        auto* fileRow = new QHBoxLayout();
        auto* nameLabel = new QLabel(card);
        nameLabel->setStyleSheet("color:#c4c0b8;font-size:13px;");
        *outFilenameLabel = nameLabel;
        fileRow->addWidget(nameLabel, 1);

        auto* browse = new QPushButton("Browse\u2026", card);
        browse->setStyleSheet(
            "QPushButton { background:#4a4642; color:#f2efe8; border:1px solid #706c66;"
            " border-radius:4px; padding:4px 12px; }"
            "QPushButton:focus { border-color:#f59e0b; }");
        connect(browse, &QPushButton::clicked, this, [this, filenameKey, nameLabel]{
            const SettingDef* d = findDef(filenameKey);
            if (!d) return;
            QString path = QFileDialog::getOpenFileName(this, "Select Memory Card",
                                                         QString(), "PS2 Memory Cards (*.ps2)");
            if (path.isEmpty()) return;
            QString base = QFileInfo(path).fileName();
            saveValue(d->section, d->key, base);
            nameLabel->setText(base);
        });
        fileRow->addWidget(browse, 0);
        v->addLayout(fileRow);
        return card;
    };

    if (auto* c = makeSlotCard("Slot1_Enable", "Slot1_Filename", "Slot 1 Enabled", &m_slot1FilenameLabel))
        root->addWidget(c);
    if (auto* c = makeSlotCard("Slot2_Enable", "Slot2_Filename", "Slot 2 Enabled", &m_slot2FilenameLabel))
        root->addWidget(c);

    auto* mtGrid = new QGridLayout();
    mtGrid->setSpacing(10);
    int col = 0;
    auto addMt = [&](const QString& key){
        auto* c = makeToggleCard(key);
        if (!c) return;
        mtGrid->addWidget(c, 0, col++);
    };
    addMt("Multitap1_Slot2_Enable");
    addMt("Multitap1_Slot3_Enable");
    addMt("Multitap1_Slot4_Enable");
    root->addLayout(mtGrid);

    root->addStretch();
}

void Pcsx2MemoryCardsPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }

    if (m_slot1FilenameLabel) {
        const SettingDef* d = findDef("Slot1_Filename");
        if (d) {
            QString cur = app->settingValue(emuId, d->section, d->key);
            m_slot1FilenameLabel->setText(cur.isEmpty() ? d->defaultValue : cur);
        }
    }
    if (m_slot2FilenameLabel) {
        const SettingDef* d = findDef("Slot2_Filename");
        if (d) {
            QString cur = app->settingValue(emuId, d->section, d->key);
            m_slot2FilenameLabel->setText(cur.isEmpty() ? d->defaultValue : cur);
        }
    }
}

void Pcsx2MemoryCardsPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
