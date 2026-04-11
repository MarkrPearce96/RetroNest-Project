#include "pcsx2_emulation_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_section_header.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../pcsx2_theme.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>

Pcsx2EmulationPage::Pcsx2EmulationPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Emulation") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* Pcsx2EmulationPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2EmulationPage::buildUi() {
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

    auto makeComboRow = [this](const QString& key) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ComboRow(this);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2EmulationPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& v){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, v);
        });
        return row;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2EmulationPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };
    auto addIfPresent = [](QBoxLayout* layout, QWidget* w){ if (w) layout->addWidget(w); };

    // Speed Control
    root->addWidget(new Pcsx2SectionHeader("Speed Control", this));
    auto* speedCard = new Pcsx2Card(this);
    auto* speedV = new QVBoxLayout(speedCard);
    speedV->setContentsMargins(14, 12, 14, 12);
    addIfPresent(speedV, makeComboRow("NominalScalar"));
    addIfPresent(speedV, makeComboRow("TurboScalar"));
    addIfPresent(speedV, makeComboRow("SlomoScalar"));
    root->addWidget(speedCard);

    // System Settings
    root->addWidget(new Pcsx2SectionHeader("System Settings", this));
    auto* sysCard = new Pcsx2Card(this);
    auto* sysV = new QVBoxLayout(sysCard);
    sysV->setContentsMargins(14, 12, 14, 12);
    addIfPresent(sysV, makeComboRow("EECycleRate"));
    addIfPresent(sysV, makeComboRow("EECycleSkip"));
    root->addWidget(sysCard);

    auto* sysGrid = new QGridLayout();
    sysGrid->setSpacing(10);
    int gr = 0, gc = 0;
    auto addToggleGrid = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        sysGrid->addWidget(w, gr, gc);
        if (++gc == 2) { gc = 0; ++gr; }
    };
    addToggleGrid("vuThread");
    addToggleGrid("EnableThreadPinning");
    addToggleGrid("CdvdPrecache");
    addToggleGrid("EnableCheats");
    addToggleGrid("EnableFastBoot");
    addToggleGrid("HostFs");
    root->addLayout(sysGrid);

    // Frame Pacing
    root->addWidget(new Pcsx2SectionHeader("Frame Pacing", this));
    auto* fpCard = new Pcsx2Card(this);
    auto* fpV = new QVBoxLayout(fpCard);
    fpV->setContentsMargins(14, 12, 14, 12);
    addIfPresent(fpV, makeComboRow("VsyncQueueSize"));
    root->addWidget(fpCard);

    auto* fpGrid = new QGridLayout();
    fpGrid->setSpacing(10);
    int fgr = 0, fgc = 0;
    auto addFpToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        fpGrid->addWidget(w, fgr, fgc);
        if (++fgc == 2) { fgc = 0; ++fgr; }
    };
    addFpToggle("SyncToHostRefreshRate");
    addFpToggle("VsyncEnable");
    addFpToggle("SkipDuplicateFrames");
    addFpToggle("UseVSyncForTiming");
    root->addLayout(fpGrid);

    root->addStretch();
}

void Pcsx2EmulationPage::loadValues() {
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

void Pcsx2EmulationPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
