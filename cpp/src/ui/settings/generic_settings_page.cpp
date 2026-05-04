#include "generic_settings_page.h"
#include "emulator_settings_dialog_base.h"
#include "settings_page_builder.h"
#include "widgets/settings_graphics_sub_tab_bar.h"
#include "widgets/settings_section_header.h"
#include "widgets/settings_card.h"
#include "widgets/settings_combo_row.h"
#include "widgets/settings_toggle_row.h"
#include "widgets/settings_slider_row.h"
#include "ui/app_controller.h"
#include "adapters/emulator_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QSlider>

GenericSettingsPage::GenericSettingsPage(EmulatorSettingsDialogBase* dlg,
                                         QVector<SettingDef> categorySchema,
                                         EmulatorAdapter* adapter,
                                         QWidget* parent)
    : QWidget(parent)
    , m_dlg(dlg)
    , m_adapter(adapter)
    , m_schema(std::move(categorySchema)) {
    if (!m_schema.isEmpty()) m_category = m_schema.front().category;

    // Discover ordered, unique subcategories (preserving first-seen order).
    QSet<QString> seen;
    for (const auto& d : m_schema) {
        if (!seen.contains(d.subcategory)) {
            seen.insert(d.subcategory);
            m_subcategories.append(d.subcategory);
        }
    }

    buildUi();
    loadValues();
    refreshDependencies();
}

GenericSettingsPage::~GenericSettingsPage() = default;

void GenericSettingsPage::buildUi() {
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

    // Back button (matches existing pages — see duckstation_console_page.cpp:76-81)
    auto* back = new QPushButton("← Back", content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dlg, &EmulatorSettingsDialogBase::popPage);
    root->addWidget(back);

    // Sub-tab handling: if there's more than one subcategory, render a
    // SettingsGraphicsSubTabBar at the top + a QStackedWidget that swaps
    // on tab change.
    if (m_subcategories.size() > 1) {
        m_subTabBar = new SettingsGraphicsSubTabBar(content);
        for (const auto& s : m_subcategories) m_subTabBar->addTab("", s);
        root->addWidget(m_subTabBar);

        m_subStack = new QStackedWidget(content);
        // One QStackedWidget page per subcategory — index in the stack
        // matches the index in m_subcategories. Subcategory name itself
        // is not needed here (buildSubcategory looks it up by index).
        for (int i = 0; i < m_subcategories.size(); ++i) {
            auto* sub = new QWidget(m_subStack);
            auto* subLayout = new QVBoxLayout(sub);  // populated by buildSubcategory
            subLayout->setContentsMargins(0, 0, 0, 0);
            subLayout->setSpacing(10);
            m_subStack->addWidget(sub);
        }
        root->addWidget(m_subStack);

        connect(m_subTabBar, &SettingsGraphicsSubTabBar::tabActivated,
                m_subStack, &QStackedWidget::setCurrentIndex);
    }

    // Per-subcategory build happens here — populated in Task 10.
    for (const QString& s : m_subcategories) buildSubcategory(s);

    root->addStretch();
}

void GenericSettingsPage::buildSubcategory(const QString& subcategory) {
    // Identify the parent layout for this subcategory's content.
    QVBoxLayout* layout = nullptr;
    if (m_subStack) {
        const int idx = m_subcategories.indexOf(subcategory);
        layout = qobject_cast<QVBoxLayout*>(m_subStack->widget(idx)->layout());
    } else {
        // Single-subcategory case: append directly to the scroll content
        // (the QScrollArea's child widget's layout, set up in buildUi()).
        auto* scroll = findChild<QScrollArea*>();
        Q_ASSERT(scroll && scroll->widget());
        layout = qobject_cast<QVBoxLayout*>(scroll->widget()->layout());
    }
    Q_ASSERT(layout);

    SettingsPageBuilder builder(this, m_schema,
        [this](const QString& sec, const QString& k, const QString& v){ saveValue(sec, k, v); },
        [this](const SettingDef& d){ emit settingFocused(d); });

    // Group entries by SettingDef::group (preserving first-seen order).
    QStringList groupOrder;
    QSet<QString> seenGroups;
    for (const auto& d : m_schema) {
        if (d.subcategory != subcategory) continue;
        if (!seenGroups.contains(d.group)) {
            seenGroups.insert(d.group);
            groupOrder.append(d.group);
        }
    }

    for (const QString& group : groupOrder) {
        if (!group.isEmpty()) {
            layout->addWidget(new SettingsSectionHeader(group, this));
        }
        for (const auto& d : m_schema) {
            if (d.subcategory != subcategory || d.group != group) continue;
            QWidget* card = nullptr;
            switch (d.type) {
                case SettingDef::Combo:
                    card = builder.makeComboCard(d.key);
                    break;
                case SettingDef::Bool:
                    card = builder.makeToggleCard(d.key);
                    break;
                case SettingDef::Int:
                case SettingDef::Float:
                    if (d.layout == "slider") card = builder.makeSliderCard(d.key);
                    break;
                default:
                    break;
            }
            if (card) layout->addWidget(card);
        }
    }
    // Push content to the top so sparse subcategory pages don't end up
    // vertically centered inside the QStackedWidget. (Single-subcategory
    // pages also benefit — buildUi() adds its own stretch on the outer
    // root layout below this one.)
    layout->addStretch();
}

void GenericSettingsPage::loadValues() {
    auto* app = m_dlg->appController();
    const QString emuId = m_dlg->emuId();

    // Block each row's signals during the programmatic seed — otherwise
    // setValue/setChecked emit valueChanged/toggled which the builder's
    // save lambda is connected to, which calls saveValue(), which writes
    // every non-Qt-default value back to disk on every page open. The
    // round-trip is harmless for the default save path but can fire
    // unwanted side effects for SettingDef::saveTransform consumers.
    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        const QString cur = app->settingValue(emuId, d.section, d.key);
        const QSignalBlocker blocker(combo);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* toggle : findChildren<SettingsToggleRow*>()) {
        const SettingDef& d = toggle->settingDef();
        const QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        const QSignalBlocker blocker(toggle);
        toggle->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<SettingsSliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        const QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        const QSignalBlocker blocker(slider);
        slider->setValue(v.toInt());
    }
}

void GenericSettingsPage::saveValue(const QString& section,
                                    const QString& key,
                                    const QString& value) {
    auto* app = m_dlg->appController();
    const QString emuId = m_dlg->emuId();

    auto defaultSave = [app, emuId](const QString& sec, const QString& k,
                                    const QString& v) {
        QVariantMap m;
        m[sec + "/" + k] = v;
        app->saveSettings(emuId, m);
    };

    bool transformed = false;
    for (const auto& d : m_schema) {
        if (d.section == section && d.key == key && d.saveTransform) {
            d.saveTransform(value, defaultSave);
            transformed = true;
            break;
        }
    }
    if (!transformed) defaultSave(section, key, value);

    // A toggle change may flip a dependent slider's active state.
    // Cheap to re-evaluate every save — refreshDependencies is O(rows).
    refreshDependencies();
}

void GenericSettingsPage::refreshDependencies() {
    // Master state map: which dependsOn-target keys are currently 'on'.
    QHash<QString, bool> masterStates;
    for (auto* tog : findChildren<SettingsToggleRow*>())
        masterStates.insert(tog->settingDef().key, tog->isChecked());

    // Apply to dependent slider rows (same pattern as duckstation page).
    for (auto* slider : findChildren<SettingsSliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        if (d.dependsOn.isEmpty()) continue;
        const bool active = masterStates.value(d.dependsOn, true);
        slider->setProperty("dependencyActive", active);
        // Disable the inner QSlider so the user can't drag the track while
        // the master toggle is off — the row itself stays focusable so
        // arrow-key spatial nav still passes through it.
        if (auto* inner = slider->findChild<QSlider*>())
            inner->setEnabled(active);
        if (!active) {
            if (!slider->graphicsEffect()) {
                auto* eff = new QGraphicsOpacityEffect(slider);
                eff->setOpacity(0.4);
                slider->setGraphicsEffect(eff);
            }
        } else {
            slider->setGraphicsEffect(nullptr);
        }
    }
}

bool GenericSettingsPage::eventFilter(QObject* obj, QEvent* e) {
    return QWidget::eventFilter(obj, e);  // Spatial nav implemented in Task 14.
}
