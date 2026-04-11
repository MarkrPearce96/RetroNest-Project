#include "pcsx2_graphics_display_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../widgets/pcsx2_slider_row.h"
#include "../widgets/pcsx2_aspect_ratio_preview.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVariantMap>

Pcsx2GraphicsDisplayPage::Pcsx2GraphicsDisplayPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "Display")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    syncPreview();
}

const SettingDef* Pcsx2GraphicsDisplayPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2GraphicsDisplayPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void Pcsx2GraphicsDisplayPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);
    buildLeftCompoundCard(topRow);
    buildRightPreviewCard(topRow);
    root->addLayout(topRow);

    buildBottomToggleGrid(root);
    root->addStretch();
}

void Pcsx2GraphicsDisplayPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    auto* card = new Pcsx2Card(this);

    if (const SettingDef* arDef = findDef("AspectRatio"))
        card->setSettingDef(*arDef);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);

    auto addCombo = [&](const QString& key) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return row;
    };

    addCombo("Renderer");
    m_aspectCombo = addCombo("AspectRatio");
    addCombo("FMVAspectRatioSwitch");
    addCombo("deinterlace_mode");
    addCombo("linear_present_mode");

    if (const SettingDef* d = findDef("EnableWideScreenPatches")) {
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this](bool on) {
            const SettingDef* dd = findDef("EnableWideScreenPatches");
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
    }

    if (m_aspectCombo) {
        connect(m_aspectCombo, &Pcsx2ComboRow::valueChanged, this, [this](const QString& val) {
            if (m_preview) {
                m_preview->setAspectRatio(Pcsx2AspectRatioPreview::fromSchemaValue(val));
            }
        });
    }

    topRow->addWidget(card, 1);
}

void Pcsx2GraphicsDisplayPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    Q_UNUSED(topRow);
}

void Pcsx2GraphicsDisplayPage::buildBottomToggleGrid(QVBoxLayout* root) {
    Q_UNUSED(root);
}

void Pcsx2GraphicsDisplayPage::loadValues() {}
void Pcsx2GraphicsDisplayPage::syncPreview() {}
