#include "duckstation_emulation_page.h"
#include "../duckstation_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_section_header.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/widgets/settings_toggle.h"
#include "ui/settings/widgets/settings_slider_row.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
#include "adapters/duckstation_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QSlider>
#include <QComboBox>
#include <QSignalBlocker>

DuckStationEmulationPage::DuckStationEmulationPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Emulation") m_schema.append(d);
    buildUi();
    loadValues();
    refreshDependencies();
}

const SettingDef* DuckStationEmulationPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationEmulationPage::buildUi() {
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
    connect(back, &QPushButton::clicked, m_dialog, &DuckStationSettingsDialog::popPage);
    root->addWidget(back);

    auto makeComboCard = [this](const QString& key) -> SettingsCard* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new SettingsCard(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new SettingsComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row,  &SettingsComboRow::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row,  &SettingsComboRow::valueChanged, this, [this, key](const QString& v){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, v);
            // If this combo just transitioned to an inactive sentinel value,
            // reset every dependent setting back to its schema default.
            const bool nowInactive = v.isEmpty() || v == "0"
                || v.compare("false",    Qt::CaseInsensitive) == 0
                || v.compare("Disabled", Qt::CaseInsensitive) == 0
                || v.compare("None",     Qt::CaseInsensitive) == 0;
            if (nowInactive) resetDependentsOf(key);
            refreshDependencies();
        });
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> SettingsCard* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new SettingsCard(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new SettingsToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row, &SettingsToggleRow::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
            // Master toggle just turned off — reset its dependents to defaults.
            if (!on) resetDependentsOf(key);
            refreshDependencies();
        });
        v->addWidget(row);
        return card;
    };
    auto makeSliderCard = [this](const QString& key, int lo, int hi, const QString& suffix) -> SettingsCard* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new SettingsCard(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new SettingsSliderRow(card);
        row->setLabel(d->label);
        row->setRange(lo, hi);
        row->setSuffix(suffix);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row,  &SettingsSliderRow::focused, this, &DuckStationEmulationPage::settingFocused);
        connect(row,  &SettingsSliderRow::valueChanged, this, [this, key](int val){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, QString::number(val));
        });
        v->addWidget(row);
        return card;
    };

    // Speed Control
    root->addWidget(new SettingsSectionHeader("Speed Control", this));
    if (auto* c = makeComboCard("EmulationSpeed"))   root->addWidget(c);
    if (auto* c = makeComboCard("FastForwardSpeed")) root->addWidget(c);
    if (auto* c = makeComboCard("TurboSpeed"))       root->addWidget(c);

    // Latency Control
    root->addWidget(new SettingsSectionHeader("Latency Control", this));
    auto* latencyGrid = new QGridLayout();
    latencyGrid->setSpacing(10);
    int lgr = 0, lgc = 0;
    auto addLatencyToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        latencyGrid->addWidget(w, lgr, lgc);
        if (++lgc == 2) { lgc = 0; ++lgr; }
    };
    addLatencyToggle("VSync");
    addLatencyToggle("SyncToHostRefreshRate");
    addLatencyToggle("OptimalFramePacing");
    addLatencyToggle("PreFrameSleep");
    addLatencyToggle("SkipPresentingDuplicateFrames");
    root->addLayout(latencyGrid);

    // Rewind
    root->addWidget(new SettingsSectionHeader("Rewind", this));
    if (auto* c = makeToggleCard("RewindEnable"))                       root->addWidget(c);
    if (auto* c = makeToggleCard("UseSoftwareRendererForMemoryStates")) root->addWidget(c);
    if (auto* c = makeSliderCard("RewindFrequency", 0, 60, " s"))       root->addWidget(c);
    if (auto* c = makeSliderCard("RewindSaveSlots",  1, 50, ""))        root->addWidget(c);

    // Runahead
    root->addWidget(new SettingsSectionHeader("Runahead", this));
    if (auto* c = makeComboCard("RunaheadFrameCount"))     root->addWidget(c);
    if (auto* c = makeToggleCard("RunaheadForAnalogInput")) root->addWidget(c);

    root->addStretch();
}

void DuckStationEmulationPage::loadValues() {
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
        slider->setValue(static_cast<int>(v.toDouble()));
    }
}

void DuckStationEmulationPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void DuckStationEmulationPage::resetDependentsOf(const QString& masterKey) {
    // Walk the schema for every setting that depends on `masterKey`. For each
    // one, write its schema default to disk and update the on-screen widget so
    // a "stale value while master is off" can never exist. Signal-blocking the
    // row prevents this from re-triggering the page's own save handler.
    for (const SettingDef& d : m_schema) {
        if (d.dependsOn != masterKey) continue;
        saveValue(d.section, d.key, d.defaultValue);
        for (auto* tog : findChildren<SettingsToggleRow*>()) {
            if (tog->settingDef().key != d.key) continue;
            QSignalBlocker sb(tog);
            tog->setChecked(d.defaultValue.compare("true", Qt::CaseInsensitive) == 0);
        }
        for (auto* slider : findChildren<SettingsSliderRow*>()) {
            if (slider->settingDef().key != d.key) continue;
            QSignalBlocker sb(slider);
            slider->setValue(static_cast<int>(d.defaultValue.toDouble()));
        }
        for (auto* combo : findChildren<SettingsComboRow*>()) {
            if (combo->settingDef().key != d.key) continue;
            QSignalBlocker sb(combo);
            combo->setValue(d.defaultValue);
        }
    }
}

void DuckStationEmulationPage::refreshDependencies() {
    // Snapshot the active state of every potential master. Toggles are active
    // when checked. Combos are treated as active when their value isn't the
    // sentinel "0" / "false" / empty — matching how DuckStation's Runahead
    // combo uses "0" for "Disabled".
    QHash<QString, bool> masterStates;
    for (auto* tog : findChildren<SettingsToggleRow*>())
        masterStates.insert(tog->settingDef().key, tog->isChecked());
    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const QString v = combo->value();
        const bool active = !v.isEmpty()
            && v != "0"
            && v.compare("false",    Qt::CaseInsensitive) != 0
            && v.compare("Disabled", Qt::CaseInsensitive) != 0
            && v.compare("None",     Qt::CaseInsensitive) != 0;
        masterStates.insert(combo->settingDef().key, active);
    }

    // Walk every card on the page; if the card's setting depends on a master
    // that's currently inactive, dim the row and disable its inner control so
    // the user can't click or drag it. The card itself stays focusable so
    // arrow-key spatial nav still passes through.
    for (auto* card : findChildren<SettingsCard*>()) {
        const QString& dep = card->settingDef().dependsOn;
        if (dep.isEmpty()) continue;
        const bool active = masterStates.value(dep, true);

        QWidget* dimTarget = nullptr;
        if (auto* sliderRow = card->findChild<SettingsSliderRow*>()) {
            sliderRow->setProperty("dependencyActive", active);
            if (auto* inner = sliderRow->findChild<QSlider*>()) inner->setEnabled(active);
            dimTarget = sliderRow;
        } else if (auto* toggleRow = card->findChild<SettingsToggleRow*>()) {
            toggleRow->setProperty("dependencyActive", active);
            if (auto* inner = toggleRow->findChild<SettingsToggle*>()) inner->setEnabled(active);
            dimTarget = toggleRow;
        } else if (auto* comboRow = card->findChild<SettingsComboRow*>()) {
            comboRow->setProperty("dependencyActive", active);
            if (auto* inner = comboRow->findChild<QComboBox*>()) inner->setEnabled(active);
            dimTarget = comboRow;
        }
        if (!dimTarget) continue;

        if (!active) {
            if (!dimTarget->graphicsEffect()) {
                auto* eff = new QGraphicsOpacityEffect(dimTarget);
                eff->setOpacity(0.4);
                dimTarget->setGraphicsEffect(eff);
            }
        } else {
            dimTarget->setGraphicsEffect(nullptr);
        }
    }
}
