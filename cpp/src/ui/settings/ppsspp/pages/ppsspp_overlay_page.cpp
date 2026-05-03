#include "ppsspp_overlay_page.h"
#include "../ppsspp_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_section_header.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "core/bitmask_helpers.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>

PpssppOverlayPage::PpssppOverlayPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Overlay") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppOverlayPage::findByLabel(const QString& label) const {
    for (const auto& d : m_schema) if (d.label == label) return &d;
    return nullptr;
}

void PpssppOverlayPage::buildUi() {
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

    auto makeBitmaskCard = [this](const QString& label) -> SettingsCard* {
        const SettingDef* d = findByLabel(label);
        if (!d) return nullptr;
        auto* card = new SettingsCard(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new SettingsToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &PpssppOverlayPage::settingFocused);
        connect(row, &SettingsToggleRow::focused, this, &PpssppOverlayPage::settingFocused);
        SettingDef defCopy = *d;
        connect(row, &SettingsToggleRow::toggled, this, [this, defCopy](bool on){
            saveBitmaskBit(defCopy, on);
        });
        v->addWidget(row);
        return card;
    };
    auto makeComboCard = [this](const QString& label) -> SettingsCard* {
        const SettingDef* d = findByLabel(label);
        if (!d) return nullptr;
        auto* card = new SettingsCard(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new SettingsComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &PpssppOverlayPage::settingFocused);
        connect(row, &SettingsComboRow::focused, this, &PpssppOverlayPage::settingFocused);
        SettingDef defCopy = *d;
        connect(row, &SettingsComboRow::valueChanged, this, [this, defCopy](const QString& v){
            saveValue(defCopy.section, defCopy.key, v);
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new SettingsSectionHeader("Status Indicators", this));
    if (auto* c = makeBitmaskCard("Show FPS Counter")) root->addWidget(c);
    if (auto* c = makeBitmaskCard("Show Speed"))       root->addWidget(c);
    if (auto* c = makeBitmaskCard("Show Battery %"))   root->addWidget(c);

    root->addWidget(new SettingsSectionHeader("Debug", this));
    if (auto* c = makeComboCard("Debug Overlay")) root->addWidget(c);

    root->addStretch();
}

void PpssppOverlayPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* row : findChildren<SettingsToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const int curInt = cur.isEmpty() ? d.defaultValue.toInt() : cur.toInt();
        row->setChecked(BitmaskHelpers::getBit(curInt, d.bitmask));
    }
    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
}

void PpssppOverlayPage::saveBitmaskBit(const SettingDef& def, bool checked) {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();
    // Re-read current int from disk so other bits stay intact when multiple
    // bitmask toggles share the same key.
    QString cur = app->settingValue(emuId, def.section, def.key);
    const int curInt = cur.isEmpty() ? def.defaultValue.toInt() : cur.toInt();
    const int newInt = BitmaskHelpers::setBit(curInt, def.bitmask, checked);
    saveValue(def.section, def.key, QString::number(newInt));
}

void PpssppOverlayPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
