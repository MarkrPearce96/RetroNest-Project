#include "settings_page_builder.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_slider_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include <QVBoxLayout>

const char* SettingsPageBuilder::kScrollAreaQss =
    "QScrollArea { background: transparent; border: none; }"
    "QScrollArea > QWidget > QWidget { background: transparent; }"
    "QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }"
    "QScrollBar::handle:vertical { background: #706c66; border-radius: 4px; min-height: 30px; }"
    "QScrollBar::handle:vertical:hover { background: #7a7670; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }";

SettingsPageBuilder::SettingsPageBuilder(QWidget* parentPage,
                                         const QVector<SettingDef>& schema,
                                         SaveFn save,
                                         FocusFn focus)
    : m_parent(parentPage), m_schema(schema), m_save(std::move(save)), m_focus(std::move(focus)) {}

const SettingDef* SettingsPageBuilder::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

// Note on lifetime: lambdas connected here outlive the SettingsPageBuilder
// (the builder is typically a stack local in the page's buildUi(), while the
// cards/rows are children of the page). Capture the looked-up section/key by
// value at card-creation time and never reach back through `this`. Same goes
// for save / focus — copy the std::function into the lambda.

SettingsCard* SettingsPageBuilder::makeComboCard(const QString& key) {
    const SettingDef* d = findDef(key);
    if (!d) return nullptr;
    auto* card = new SettingsCard(m_parent);
    card->setSettingDef(*d);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    auto* row = new SettingsComboRow(card);
    row->setLabel(d->label);
    row->setOptions(d->options);
    row->setSettingDef(*d);
    auto focus = m_focus;
    QObject::connect(card, &SettingsCard::focused, m_parent, focus);
    QObject::connect(row,  &SettingsComboRow::focused, m_parent, focus);
    QString section = d->section;
    QString defKey = d->key;
    auto save = m_save;
    QObject::connect(row, &SettingsComboRow::valueChanged, m_parent,
                     [section, defKey, save](const QString& v){
                         save(section, defKey, v);
                     });
    v->addWidget(row);
    return card;
}

SettingsCard* SettingsPageBuilder::makeSliderCard(const QString& key) {
    const SettingDef* d = findDef(key);
    if (!d) return nullptr;
    auto* card = new SettingsCard(m_parent);
    card->setSettingDef(*d);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    auto* row = new SettingsSliderRow(card);
    row->setLabel(d->label);
    row->setRange(int(d->minVal), int(d->maxVal));
    row->setSuffix(d->suffix);
    row->setSettingDef(*d);
    auto focus = m_focus;
    QObject::connect(card, &SettingsCard::focused, m_parent, focus);
    QObject::connect(row,  &SettingsSliderRow::focused, m_parent, focus);
    QString section = d->section;
    QString defKey = d->key;
    auto save = m_save;
    QObject::connect(row, &SettingsSliderRow::valueChanged, m_parent,
                     [section, defKey, save](int val){
                         save(section, defKey, QString::number(val));
                     });
    v->addWidget(row);
    return card;
}

SettingsCard* SettingsPageBuilder::makeToggleCard(const QString& key) {
    const SettingDef* d = findDef(key);
    if (!d) return nullptr;
    auto* card = new SettingsCard(m_parent);
    card->setSettingDef(*d);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    auto* row = new SettingsToggleRow(card);
    row->setLabel(d->label);
    row->setSettingDef(*d);
    auto focus = m_focus;
    QObject::connect(card, &SettingsCard::focused, m_parent, focus);
    QObject::connect(row,  &SettingsToggleRow::focused, m_parent, focus);
    QString section = d->section;
    QString defKey = d->key;
    auto save = m_save;
    QObject::connect(row, &SettingsToggleRow::toggled, m_parent,
                     [section, defKey, save](bool on){
                         save(section, defKey, on ? "true" : "false");
                     });
    v->addWidget(row);
    return card;
}
