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
    Q_UNUSED(topRow);
}

void Pcsx2GraphicsDisplayPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    Q_UNUSED(topRow);
}

void Pcsx2GraphicsDisplayPage::buildBottomToggleGrid(QVBoxLayout* root) {
    Q_UNUSED(root);
}

void Pcsx2GraphicsDisplayPage::loadValues() {}
void Pcsx2GraphicsDisplayPage::syncPreview() {}
