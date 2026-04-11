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
    auto* card = new Pcsx2Card(this);
    card->setPreviewStyle(true);
    if (const SettingDef* d = findDef("StretchY"))
        card->setSettingDef(*d);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    auto* lbl = new QLabel(QStringLiteral("ASPECT RATIO PREVIEW"), card);
    lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;"
                       "letter-spacing:0.8px;");
    v->addWidget(lbl);

    m_preview = new Pcsx2AspectRatioPreview(card);
    m_preview->setMinimumHeight(180);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    v->addWidget(m_preview, 1);

    if (const SettingDef* d = findDef("StretchY")) {
        m_stretchSlider = new Pcsx2SliderRow(card);
        m_stretchSlider->setLabel(d->label);
        m_stretchSlider->setRange(int(d->minVal), int(d->maxVal));
        m_stretchSlider->setSuffix(d->suffix);
        m_stretchSlider->setSettingDef(*d);
        connect(m_stretchSlider, &Pcsx2SliderRow::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(m_stretchSlider, &Pcsx2SliderRow::valueChanged, this, [this](int val) {
            const SettingDef* dd = findDef("StretchY");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
            if (m_preview) m_preview->setStretchY(val);
        });
        v->addWidget(m_stretchSlider);
    }

    auto* cropLabel = new QLabel(QStringLiteral("Crop"), card);
    cropLabel->setStyleSheet("color:#d0ccc4;font-size:12px;font-weight:500;");
    v->addWidget(cropLabel);

    auto* cropRow = new QHBoxLayout();
    cropRow->setSpacing(8);

    auto makeCropSpin = [&](const QString& key, const QString& axis) -> QSpinBox* {
        auto* w = new QWidget(card);
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);

        auto* axisLbl = new QLabel(axis, w);
        axisLbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;");
        h->addWidget(axisLbl);

        auto* spin = new QSpinBox(w);
        spin->setRange(0, 100);
        spin->setSuffix(QStringLiteral(" px"));
        spin->setStyleSheet(
            "QSpinBox {"
            "  background:#585450; color:#f2efe8;"
            "  border:1px solid #706c66; border-radius:4px;"
            "  padding:2px 4px; min-width:58px;"
            "}"
            "QSpinBox:focus { border-color:#f59e0b; }");
        h->addWidget(spin, 1);

        cropRow->addWidget(w, 1);

        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, key](int val) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
            if (m_preview && m_cropL && m_cropT && m_cropR && m_cropB) {
                m_preview->setCrop(
                    m_cropL->value(), m_cropT->value(),
                    m_cropR->value(), m_cropB->value());
            }
        });
        return spin;
    };

    m_cropL = makeCropSpin("CropLeft",   QStringLiteral("L"));
    m_cropT = makeCropSpin("CropTop",    QStringLiteral("T"));
    m_cropR = makeCropSpin("CropRight",  QStringLiteral("R"));
    m_cropB = makeCropSpin("CropBottom", QStringLiteral("B"));

    v->addLayout(cropRow);

    topRow->addWidget(card, 1);
}

void Pcsx2GraphicsDisplayPage::buildBottomToggleGrid(QVBoxLayout* root) {
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        if (!d) return card;
        card->setSettingDef(*d);

        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            if (key == "IntegerScaling" && m_preview)
                m_preview->setIntegerScaling(on);
        });
        if (key == "IntegerScaling")
            m_integerScalingToggle = row;
        v->addWidget(row);
        return card;
    };

    auto* grid = new QGridLayout();
    grid->setSpacing(12);

    grid->addWidget(makeToggleCard("pcrtc_antiblur"),             0, 0);
    grid->addWidget(makeToggleCard("IntegerScaling"),             0, 1);
    grid->addWidget(makeToggleCard("EnableNoInterlacingPatches"), 0, 2);
    grid->addWidget(makeToggleCard("pcrtc_offsets"),              1, 0);
    grid->addWidget(makeToggleCard("disable_interlace_offset"),   1, 1);
    grid->addWidget(makeToggleCard("pcrtc_overscan"),             1, 2);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    root->addLayout(grid);
}

void Pcsx2GraphicsDisplayPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* tog : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = tog->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        tog->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        bool ok = false;
        int v = cur.toInt(&ok);
        if (!ok) v = d.defaultValue.toInt();
        slider->setValue(v);
    }

    auto loadCrop = [&](QSpinBox* spin, const QString& key) {
        if (!spin) return;
        const SettingDef* d = findDef(key);
        if (!d) return;
        QString cur = app->settingValue(emuId, d->section, d->key);
        bool ok = false;
        int v = cur.toInt(&ok);
        if (!ok) v = d->defaultValue.toInt();
        spin->setValue(v);
    };
    loadCrop(m_cropL, "CropLeft");
    loadCrop(m_cropT, "CropTop");
    loadCrop(m_cropR, "CropRight");
    loadCrop(m_cropB, "CropBottom");
}

void Pcsx2GraphicsDisplayPage::syncPreview() {
    if (!m_preview) return;

    if (m_aspectCombo) {
        m_preview->setAspectRatio(
            Pcsx2AspectRatioPreview::fromSchemaValue(m_aspectCombo->value()));
    }
    if (m_stretchSlider) {
        m_preview->setStretchY(m_stretchSlider->value());
    }
    if (m_cropL && m_cropT && m_cropR && m_cropB) {
        m_preview->setCrop(m_cropL->value(), m_cropT->value(),
                           m_cropR->value(), m_cropB->value());
    }
    if (m_integerScalingToggle) {
        m_preview->setIntegerScaling(m_integerScalingToggle->isChecked());
    }
}
