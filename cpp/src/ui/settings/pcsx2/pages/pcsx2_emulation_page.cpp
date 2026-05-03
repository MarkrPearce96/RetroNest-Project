#include "pcsx2_emulation_page.h"
#include "../pcsx2_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_section_header.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
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
    auto makeToggleCard = [&builder](const QString& key){ return builder.makeToggleCard(key); };
    auto addIfPresent = [](QBoxLayout* layout, QWidget* w){ if (w) layout->addWidget(w); };
    (void)addIfPresent;

    // Speed Control
    root->addWidget(new SettingsSectionHeader("Speed Control", this));
    if (auto* c = makeComboCard("NominalScalar")) root->addWidget(c);
    if (auto* c = makeComboCard("TurboScalar"))   root->addWidget(c);
    if (auto* c = makeComboCard("SlomoScalar"))   root->addWidget(c);

    // System Settings
    root->addWidget(new SettingsSectionHeader("System Settings", this));
    if (auto* c = makeComboCard("EECycleRate")) root->addWidget(c);
    if (auto* c = makeComboCard("EECycleSkip")) root->addWidget(c);

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
    root->addWidget(new SettingsSectionHeader("Frame Pacing", this));
    if (auto* c = makeComboCard("VsyncQueueSize")) root->addWidget(c);

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

void Pcsx2EmulationPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
