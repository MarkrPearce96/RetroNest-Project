#include "duckstation_graphics_advanced_page.h"
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
#include <QScrollArea>
#include <QVariantMap>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QStyleFactory>
#include <QStyle>
#include <QEvent>
#include <QKeyEvent>

DuckStationGraphicsAdvancedPage::DuckStationGraphicsAdvancedPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Graphics" && d.subcategory == "Advanced") m_schema.append(d);
    buildUi();
    loadValues();
    refreshDependencies();
}

const SettingDef* DuckStationGraphicsAdvancedPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationGraphicsAdvancedPage::buildUi() {
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

    // ── Lambda helpers ────────────────────────────────────────────────────
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
        connect(card, &SettingsCard::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row,  &SettingsComboRow::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row,  &SettingsComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, val);
            const bool nowInactive = val.isEmpty() || val == "0"
                || val.compare("false",    Qt::CaseInsensitive) == 0
                || val.compare("Disabled", Qt::CaseInsensitive) == 0
                || val.compare("None",     Qt::CaseInsensitive) == 0;
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
        connect(card, &SettingsCard::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row, &SettingsToggleRow::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
            if (!on) resetDependentsOf(key);
            refreshDependencies();
        });
        v->addWidget(row);
        return card;
    };

    SettingsPageBuilder builder(this, m_schema,
        [this](const QString& sec, const QString& k, const QString& v){ saveValue(sec, k, v); },
        [this](const SettingDef& d){ emit settingFocused(d); });
    auto makeSliderCard = [&builder](const QString& key){ return builder.makeSliderCard(key); };

    // ── Section: Display Options ──────────────────────────────────────────
    root->addWidget(new SettingsSectionHeader("Display Options", this));

    // Helper to build a compact combo card whose row label is hidden — used
    // for the second card in a paired row (e.g. Rotation next to Alignment),
    // so it doesn't waste 180px on an empty label slot.
    auto makeCompactComboCard = [this](const QString& key) -> SettingsCard* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new SettingsCard(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new SettingsComboRow(card);
        row->setLabelVisible(false);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row,  &SettingsComboRow::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(row,  &SettingsComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, val);
        });
        v->addWidget(row);
        return card;
    };

    // Helper to build a small spin-box card with a label prefix — used for
    // the Fine Crop Size row (Left / Top / Right / Bottom). Each card is its
    // own focus target so card-level keyboard nav can reach all four.
    auto makeSpinCard = [this](const QString& key, const QString& prefix) -> SettingsCard* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new SettingsCard(this);
        card->setSettingDef(*d);
        auto* h = new QHBoxLayout(card);
        h->setContentsMargins(12, 10, 12, 10);
        h->setSpacing(8);
        auto* lbl = new QLabel(prefix, card);
        lbl->setStyleSheet("color:#d0ccc4;font-size:13px;");
        h->addWidget(lbl, 0);

        auto* spin = new QSpinBox(card);
        // Force Fusion so QSS pseudo-states (focus, disabled, up/down-button)
        // actually render — macOS's native style ignores most of them. Cache
        // the style instance: QStyleFactory::create allocates a fresh QStyle
        // per call, and Qt allows the same QStyle to be set on many widgets.
        static QStyle* s_fusion = QStyleFactory::create("Fusion");
        if (s_fusion) spin->setStyle(s_fusion);
        spin->setRange(int(d->minVal), int(d->maxVal));
        spin->setSingleStep(int(d->step));
        spin->setSuffix(d->suffix.isEmpty() ? QString() : QStringLiteral(" ") + d->suffix);
        spin->setProperty("settingKey", key);
        spin->setProperty("settingSection", d->section);
        spin->setStyleSheet(
            "QSpinBox { background:#3f3c39; color:#f2efe8; border:1px solid #585450;"
            "  border-radius:6px; padding:4px 22px 4px 6px; min-width:48px; }"
            "QSpinBox:focus { border:1px solid #f59e0b; }"
            "QSpinBox:disabled { color:#7a7670; }"
            "QSpinBox::up-button {"
            "  subcontrol-origin: border; subcontrol-position: top right;"
            "  width:18px; border-left:1px solid #585450;"
            "  border-top-right-radius:6px; background:#4a4744; }"
            "QSpinBox::up-button:hover  { background:#5a5650; }"
            "QSpinBox::up-button:pressed{ background:#3a3733; }"
            "QSpinBox::down-button {"
            "  subcontrol-origin: border; subcontrol-position: bottom right;"
            "  width:18px; border-left:1px solid #585450;"
            "  border-bottom-right-radius:6px; background:#4a4744; }"
            "QSpinBox::down-button:hover  { background:#5a5650; }"
            "QSpinBox::down-button:pressed{ background:#3a3733; }"
            "QSpinBox::up-arrow   { image:none; width:8px; height:8px;"
            "  border-left:4px solid transparent; border-right:4px solid transparent;"
            "  border-bottom:5px solid #f2efe8; }"
            "QSpinBox::down-arrow { image:none; width:8px; height:8px;"
            "  border-left:4px solid transparent; border-right:4px solid transparent;"
            "  border-top:5px solid #f2efe8; }");
        connect(card, &SettingsCard::focused, this, &DuckStationGraphicsAdvancedPage::settingFocused);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this, key](int val){
            if (const SettingDef* d2 = findDef(key))
                saveValue(d2->section, d2->key, QString::number(val));
        });
        // Install ourselves as event filter so Enter while editing returns
        // focus to the parent card instead of bubbling up to the dialog
        // (where Esc/Enter would pop the page back to the hub).
        spin->installEventFilter(this);
        h->addWidget(spin, 1);
        return card;
    };

    // Screen Position pair: two cards side-by-side. The first carries the
    // "Screen Position" label (from the schema), the second is compact with
    // its label hidden so visually they read as one labeled row.
    auto* posLayout = new QHBoxLayout();
    posLayout->setSpacing(10);
    if (auto* c = makeComboCard("Alignment")) posLayout->addWidget(c, 1);
    if (auto* c = makeCompactComboCard("Rotation")) posLayout->addWidget(c, 1);
    root->addLayout(posLayout);

    if (auto* c = makeComboCard("FineCropMode")) root->addWidget(c);

    // Fine Crop Size: four small spin-box cards in a row, each labeled with
    // its edge name. Splitting into individual cards lets card-level
    // keyboard nav land on each one independently.
    auto* cropLayout = new QHBoxLayout();
    cropLayout->setSpacing(10);
    if (auto* c = makeSpinCard("FineCropLeft",   "Left:"))   cropLayout->addWidget(c, 1);
    if (auto* c = makeSpinCard("FineCropTop",    "Top:"))    cropLayout->addWidget(c, 1);
    if (auto* c = makeSpinCard("FineCropRight",  "Right:"))  cropLayout->addWidget(c, 1);
    if (auto* c = makeSpinCard("FineCropBottom", "Bottom:")) cropLayout->addWidget(c, 1);
    root->addLayout(cropLayout);

    if (auto* c = makeToggleCard("DisableMailboxPresentation")) root->addWidget(c);

    // ── Section: Rendering Options ────────────────────────────────────────
    root->addWidget(new SettingsSectionHeader("Rendering Options", this));

    if (auto* c = makeComboCard("Multisamples"))   root->addWidget(c);
    if (auto* c = makeComboCard("LineDetectMode")) root->addWidget(c);

    auto* renderGrid = new QGridLayout();
    renderGrid->setSpacing(10);
    int rgr = 0, rgc = 0;
    auto addRenderCell = [&](QWidget* w){
        if (!w) return;
        renderGrid->addWidget(w, rgr, rgc);
        if (++rgc == 2) { rgc = 0; ++rgr; }
    };

    addRenderCell(makeToggleCard("UseThread"));
    addRenderCell(makeSliderCard("MaxQueuedFrames"));
    addRenderCell(makeToggleCard("EnableModulationCrop"));
    addRenderCell(makeToggleCard("ScaledInterlacing"));
    addRenderCell(makeToggleCard("UseSoftwareRendererForReadbacks"));

    root->addLayout(renderGrid);
    root->addStretch();
}

void DuckStationGraphicsAdvancedPage::loadValues() {
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
    for (auto* row : findChildren<SettingsSliderRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setValue(v.toInt());
    }
    // Inline Fine Crop spin boxes: tagged with `settingKey` / `settingSection`
    // since they're raw QSpinBox widgets rather than Pcsx2 row wrappers.
    for (auto* spin : findChildren<QSpinBox*>()) {
        const QString key = spin->property("settingKey").toString();
        if (key.isEmpty()) continue;
        const SettingDef* d = findDef(key);
        if (!d) continue;
        QString cur = app->settingValue(emuId, d->section, d->key);
        const QString v = cur.isEmpty() ? d->defaultValue : cur;
        QSignalBlocker sb(spin);
        spin->setValue(v.toInt());
    }
}

void DuckStationGraphicsAdvancedPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

bool DuckStationGraphicsAdvancedPage::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        if (auto* spin = qobject_cast<QSpinBox*>(obj)) {
            const int k = static_cast<QKeyEvent*>(e)->key();
            if (k == Qt::Key_Return || k == Qt::Key_Enter) {
                // QSpinBox already commits on edit, so we just need to return
                // focus to the parent SettingsCard. Mirrors SettingsSliderRow's
                // Enter-to-exit-edit behaviour.
                for (QWidget* w = spin->parentWidget(); w; w = w->parentWidget()) {
                    if (w->inherits("SettingsCard") && w->focusPolicy() != Qt::NoFocus) {
                        w->setFocus(Qt::OtherFocusReason);
                        break;
                    }
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, e);
}

void DuckStationGraphicsAdvancedPage::resetDependentsOf(const QString& masterKey) {
    // Walk schema for every dependent of `masterKey`, write its schema default
    // to disk, and update the matching widget. Signal-blocking the row prevents
    // re-entrancy through the page's own save handler.
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
        for (auto* spin : findChildren<QSpinBox*>()) {
            if (spin->property("settingKey").toString() != d.key) continue;
            QSignalBlocker sb(spin);
            spin->setValue(static_cast<int>(d.defaultValue.toDouble()));
        }
    }
}

void DuckStationGraphicsAdvancedPage::refreshDependencies() {
    // Snapshot active state of every potential master. Toggles are active when
    // checked. Combos are inactive on the sentinel values "0" / "false" /
    // "Disabled" / "None" / empty — matches DuckStation's combo conventions.
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
        } else if (auto* spin = card->findChild<QSpinBox*>()) {
            spin->setProperty("dependencyActive", active);
            spin->setEnabled(active);
            dimTarget = spin;
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
