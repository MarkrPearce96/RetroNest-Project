#include "generic_settings_page.h"
#include "emulator_settings_dialog_base.h"
#include "settings_page_builder.h"
#include "widgets/settings_graphics_sub_tab_bar.h"
#include "widgets/settings_section_header.h"
#include "widgets/settings_card.h"
#include "widgets/settings_combo_row.h"
#include "widgets/preview/aspect_ratio_preview.h"
#include "widgets/preview/osd_preview.h"
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
#include <QApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include <QKeyEvent>
#include <QLabel>
#include <limits>

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
    qApp->installEventFilter(this);
}

GenericSettingsPage::~GenericSettingsPage() {
    qApp->removeEventFilter(this);
}

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

    // Back button (matches existing pages — see duckstation_console_page.cpp:76-81).
    // NoFocus so Tab/arrows can't sink focus onto it — popPage is reachable
    // via Esc/B-button at the dialog level.
    auto* back = new QPushButton("← Back", content);
    back->setFocusPolicy(Qt::NoFocus);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dlg, &EmulatorSettingsDialogBase::popPage);
    root->addWidget(back);

    // Sub-tab handling: if there's more than one subcategory, render a
    // SettingsGraphicsSubTabBar at the top + a QStackedWidget that swaps
    // on tab change. Mirrors duckstation_graphics_page.cpp:40-47.
    if (m_subcategories.size() > 1) {
        m_subTabBar = new SettingsGraphicsSubTabBar(content);
        // NoFocus — the bar is driven by Tab/Shift-Tab in eventFilter, not
        // by being a Tab stop itself. Otherwise it sinks focus and breaks
        // the spatial-nav contract that focus always lives on a settings
        // card while the page is interactive.
        m_subTabBar->setFocusPolicy(Qt::NoFocus);
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
            subLayout->setSpacing(8);
            m_subStack->addWidget(sub);
        }
        root->addWidget(m_subStack);

        // Sub-tab activation: swap the stack page, clear the description
        // bar (the previously-focused setting is no longer visible), and
        // shift focus to the first card on the new tab so spatial nav
        // continues working without the user having to click.
        connect(m_subTabBar, &SettingsGraphicsSubTabBar::tabActivated,
                this, [this](int index) {
                    m_subStack->setCurrentIndex(index);
                    m_dlg->clearFocusedSetting();
                    focusFirstSettingOnCurrentSubTab();
                });
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
        auto* scroll = findChild<QScrollArea*>();
        Q_ASSERT(scroll && scroll->widget());
        layout = qobject_cast<QVBoxLayout*>(scroll->widget()->layout());
    }
    Q_ASSERT(layout);

    const PreviewSpec spec = m_adapter
        ? m_adapter->previewSpec(m_category, subcategory)
        : PreviewSpec{};

    // When a preview is active, the top row gets a split layout:
    //   left column  = SettingDef::group blocks containing preview-bound
    //                  keys (i.e. groups that "belong with" the preview)
    //   right column = preview card
    // Everything else (groups with no preview-bound keys) flows into the
    // full-width `layout` BELOW the top row. Mirrors PCSX2's display page
    // pattern (pcsx2_graphics_display_page.cpp:79-83 — buildLeftCompoundCard
    // + buildRightPreviewCard inside topRow, then buildBottomToggleGrid
    // below).
    QVBoxLayout* topLeftLayout = nullptr;     // only set when a preview exists
    QWidget* preview = nullptr;
    QSet<QString> previewBoundGroups;          // groups whose entries feed the preview

    if (!spec.previewType.isEmpty()) {
        // Identify which groups in this subcategory hold preview-bound keys.
        // Those groups go in the top-row left column next to the preview.
        for (auto it = spec.keyToProperty.constBegin();
             it != spec.keyToProperty.constEnd(); ++it) {
            for (const auto& d : m_schema) {
                if (d.subcategory == subcategory && d.key == it.key()) {
                    previewBoundGroups.insert(d.group);
                    break;
                }
            }
        }

        auto* topRow = new QHBoxLayout();
        topRow->setSpacing(14);

        // leftHost — its layout takes preview-bound group(s).
        auto* leftHost = new QWidget(this);
        topLeftLayout = new QVBoxLayout(leftHost);
        topLeftLayout->setContentsMargins(0, 0, 0, 0);
        topLeftLayout->setSpacing(8);
        topRow->addWidget(leftHost, /*stretch=*/1);

        // Right column: preview card. Sized to content (label + preview)
        // and centered vertically in the row, so the preview always sits
        // beside the middle of the left column regardless of how many
        // settings the preview-bound group contains. Future emulators
        // with shorter or longer preview-bound groups get the same
        // visual rhythm — preview floats at the row's midline.
        auto* card = new SettingsCard(this);
        card->setFocusPolicy(Qt::NoFocus);
        card->setPreviewStyle(true);
        card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        v->setSpacing(10);
        auto* lbl = new QLabel(spec.previewType == "osd"
                                   ? QStringLiteral("OSD PREVIEW")
                                   : QStringLiteral("ASPECT RATIO PREVIEW"), card);
        lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;"
                           "letter-spacing:0.8px;");
        v->addWidget(lbl);
        preview = mountPreviewWidget(spec.previewType, card);
        if (preview) v->addWidget(preview);
        topRow->addWidget(card, /*stretch=*/1, Qt::AlignTop);

        layout->addLayout(topRow);
        m_currentPreview = preview;
    }

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

    // Helper: build a card for one SettingDef. Returns nullptr for
    // unsupported types. Centralised so the pair-rendering branch below
    // can construct two cards at once.
    auto makeCardFor = [&](const SettingDef& d) -> QWidget* {
        switch (d.type) {
            case SettingDef::Combo: return builder.makeComboCard(d.key);
            case SettingDef::Bool:  return builder.makeToggleCard(d.key);
            case SettingDef::Int:
            case SettingDef::Float:
                if (d.layout == "slider"
                    || d.layout == "paired" /* paired numeric is rare */)
                    return builder.makeSliderCard(d.key);
                // fall through
                return nullptr;
            default:
                return nullptr;
        }
    };

    for (const QString& group : groupOrder) {
        // Preview-bound group → ALL its cards stack in the topLeft
        // column under one header, even if the column ends up taller
        // than the preview. Non-preview-bound groups → full-width below.
        QVBoxLayout* dest = (topLeftLayout && previewBoundGroups.contains(group))
            ? topLeftLayout
            : layout;

        if (!group.isEmpty()) {
            dest->addWidget(new SettingsSectionHeader(group, this));
        }

        // Collect this group's defs in declaration order so we can peek
        // ahead for the pairing logic.
        QVector<const SettingDef*> groupDefs;
        for (const auto& d : m_schema)
            if (d.subcategory == subcategory && d.group == group)
                groupDefs.append(&d);

        for (int i = 0; i < groupDefs.size(); ++i) {
            const SettingDef& d = *groupDefs[i];
            QWidget* card = makeCardFor(d);
            if (!card) continue;

            // Pair with the next def when BOTH have layout == "paired".
            // Renders the two cards side-by-side in a horizontal row,
            // each taking 50% of the destination width.
            if (d.layout == "paired"
                && i + 1 < groupDefs.size()
                && groupDefs[i + 1]->layout == "paired") {
                QWidget* card2 = makeCardFor(*groupDefs[i + 1]);
                if (card2) {
                    auto* pair = new QHBoxLayout();
                    pair->setContentsMargins(0, 0, 0, 0);
                    pair->setSpacing(8);
                    pair->addWidget(card,  /*stretch=*/1);
                    pair->addWidget(card2, /*stretch=*/1);
                    dest->addLayout(pair);
                    ++i;  // skip the paired sibling — already consumed
                    continue;
                }
            }
            dest->addWidget(card);
        }
    }

    if (preview && !spec.keyToProperty.isEmpty())
        wirePreviewBinding(spec, preview);

    // Top-align cards in both layouts. The left column especially needs it
    // — without a stretch its cards get vertically centered next to the
    // taller preview card.
    if (topLeftLayout) topLeftLayout->addStretch();
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

    // Combos can also act as masters (per setting_def.h:39-42): a combo is
    // 'active' when its value is not in {"", "0", "false", "Disabled", "None"}.
    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const QString v = combo->value();
        const bool active = !v.isEmpty()
            && v != "0"
            && v.compare("false",    Qt::CaseInsensitive) != 0
            && v.compare("Disabled", Qt::CaseInsensitive) != 0
            && v.compare("None",     Qt::CaseInsensitive) != 0;
        masterStates.insert(combo->settingDef().key, active);
    }

    // Apply to dependent slider rows (same pattern as duckstation page).
    // Toggle and combo dependents are not yet handled — no current schema
    // declares dependsOn on a non-slider row. See plan T13 follow-up.
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
    if (e->type() != QEvent::KeyPress) return QWidget::eventFilter(obj, e);
    auto* ke = static_cast<QKeyEvent*>(e);
    const int k = ke->key();

    QWidget* current = QApplication::focusWidget();
    if (!current || !isAncestorOf(current))
        return QWidget::eventFilter(obj, e);

    // Tab / Shift+Tab cycles through sub-tabs when present. Mirrors
    // DuckStationGraphicsPage::eventFilter:106-117 (L1/R1 on a controller
    // is mapped to Tab/Backtab by the global input layer).
    if ((k == Qt::Key_Tab || k == Qt::Key_Backtab) && m_subTabBar) {
        const int count = m_subTabBar->tabCount();
        if (count >= 2) {
            int next = m_subTabBar->currentIndex();
            if (k == Qt::Key_Backtab || (ke->modifiers() & Qt::ShiftModifier)) {
                next = (next - 1 + count) % count;
            } else {
                next = (next + 1) % count;
            }
            m_subTabBar->setCurrentIndex(next);
            return true;
        }
    }

    if (k != Qt::Key_Left && k != Qt::Key_Right && k != Qt::Key_Up && k != Qt::Key_Down)
        return QWidget::eventFilter(obj, e);

    // Combo dropdown open — let the popup handle arrows.
    if (auto* combo = qobject_cast<QComboBox*>(current))
        if (combo->view() && combo->view()->isVisible())
            return QWidget::eventFilter(obj, e);

    // Slider in edit mode handles its own arrows (see SettingsSliderRow::setEditing).
    if (current->property("editing").toBool())
        return QWidget::eventFilter(obj, e);

    SettingsCard* sourceCard = nullptr;
    for (QWidget* w = current; w; w = w->parentWidget()) {
        if (auto* card = qobject_cast<SettingsCard*>(w)) { sourceCard = card; break; }
    }
    if (!sourceCard) return QWidget::eventFilter(obj, e);

    if (SettingsCard* next = findNextCardSpatial(sourceCard, k)) {
        next->setFocus(Qt::TabFocusReason);
        for (QWidget* p = next->parentWidget(); p; p = p->parentWidget()) {
            if (auto* sa = qobject_cast<QScrollArea*>(p)) {
                sa->ensureWidgetVisible(next, 20, 40);
                break;
            }
        }
    }
    // Always consume so combos/sliders/toggles don't see the arrow themselves.
    return true;
}

SettingsCard* GenericSettingsPage::findNextCardSpatial(SettingsCard* current, int key) const {
    auto pagePoint = [this](QWidget* w) -> QPoint {
        return w->mapTo(const_cast<GenericSettingsPage*>(this), QPoint(0, 0));
    };
    const QRect mine(pagePoint(current), current->size());
    const QPoint myCenter = mine.center();
    const bool vertical = (key == Qt::Key_Up || key == Qt::Key_Down);

    auto rangesOverlap = [](int a0, int a1, int b0, int b1) { return a0 < b1 && b0 < a1; };

    // First pass: collect every in-direction card whose perpendicular range
    // overlaps the source. Track the closest "next row" (or column) by the
    // primary-axis gap so a strictly closer card always wins over a farther
    // but better-aligned one — without this, a full-width row two rows down
    // can outscore a half-width card immediately below.
    struct Cand { SettingsCard* card; long long primary; long long secondary; };
    QList<Cand> cands;
    long long minPrimary = std::numeric_limits<long long>::max();

    for (SettingsCard* card : findChildren<SettingsCard*>()) {
        if (card == current || !card->isVisible() || card->focusPolicy() == Qt::NoFocus) continue;
        const QRect r(pagePoint(card), card->size());
        const QPoint c = r.center();
        const int dx = c.x() - myCenter.x();
        const int dy = c.y() - myCenter.y();
        bool inDir = false, perpOverlap = false;
        switch (key) {
            case Qt::Key_Left:  inDir = dx < 0; perpOverlap = rangesOverlap(mine.top(),  mine.bottom(), r.top(),  r.bottom()); break;
            case Qt::Key_Right: inDir = dx > 0; perpOverlap = rangesOverlap(mine.top(),  mine.bottom(), r.top(),  r.bottom()); break;
            case Qt::Key_Up:    inDir = dy < 0; perpOverlap = rangesOverlap(mine.left(), mine.right(),  r.left(), r.right());  break;
            case Qt::Key_Down:  inDir = dy > 0; perpOverlap = rangesOverlap(mine.left(), mine.right(),  r.left(), r.right());  break;
        }
        if (!inDir || !perpOverlap) continue;

        // primary = signed distance along the nav axis (between rects, not centers,
        // so half-width cards on the same row tie). secondary = how far off-axis
        // the candidate's overlap-center is from the source's center.
        long long primary = 0, secondary = 0;
        if (vertical) {
            primary  = (key == Qt::Key_Down) ? qMax<int>(0, r.top() - mine.bottom())
                                             : qMax<int>(0, mine.top() - r.bottom());
            const int oL = qMax(mine.left(), r.left());
            const int oR = qMin(mine.right(), r.right());
            secondary = qAbs((oL + oR) / 2 - myCenter.x());
        } else {
            primary  = (key == Qt::Key_Right) ? qMax<int>(0, r.left() - mine.right())
                                              : qMax<int>(0, mine.left() - r.right());
            const int oT = qMax(mine.top(), r.top());
            const int oB = qMin(mine.bottom(), r.bottom());
            secondary = qAbs((oT + oB) / 2 - myCenter.y());
        }
        cands.push_back({card, primary, secondary});
        if (primary < minPrimary) minPrimary = primary;
    }
    if (cands.isEmpty()) return nullptr;

    // Within the closest row/column (primary ≈ minPrimary, with a small
    // tolerance for grid spacing differences), pick the most-aligned card.
    constexpr long long kRowTolerance = 8;
    SettingsCard* best = nullptr;
    long long bestSecondary = std::numeric_limits<long long>::max();
    for (const Cand& c : cands) {
        if (c.primary > minPrimary + kRowTolerance) continue;
        if (c.secondary < bestSecondary) { bestSecondary = c.secondary; best = c.card; }
    }
    return best;
}

void GenericSettingsPage::focusFirstSettingOnCurrentSubTab() {
    // Search inside the visible sub-stack page when present, otherwise the
    // whole page. First focusable SettingsCard wins so spatial-nav has a
    // starting point. Mirrors DuckStationGraphicsPage::focusFirstSettingOnCurrentTab.
    QWidget* searchRoot = m_subStack ? m_subStack->currentWidget() : this;
    if (!searchRoot) return;
    for (auto* card : searchRoot->findChildren<SettingsCard*>()) {
        if (!card->isVisible()) continue;
        if (card->focusPolicy() == Qt::NoFocus) continue;
        card->setFocus(Qt::TabFocusReason);
        return;
    }
}

QWidget* GenericSettingsPage::mountPreviewWidget(const QString& previewType,
                                                  QWidget* parent) {
    if (previewType == "aspect") return new AspectRatioPreview(parent);
    if (previewType == "osd")    return new OsdPreview(parent);
    return nullptr;
}

void GenericSettingsPage::wirePreviewBinding(const PreviewSpec& spec,
                                              QWidget* preview) {
    auto* app = m_dlg->appController();
    const QString emuId = m_dlg->emuId();

    // Initial sync: read each bound setting's current value and seed the
    // matching preview Q_PROPERTY.
    auto seed = [&](const QString& propName, const QString& val,
                    SettingDef::Type type) {
        bool ok = false;
        const int asInt = val.toInt(&ok);
        // For non-Combo numeric types the int form is the right shape;
        // Combo values may be either numeric (e.g. Dolphin AspectRatio "0".."3")
        // or string (e.g. PCSX2 AspectRatio "16:9"), so we pass them through
        // as-is and let the preview widget's WRITE accessor parse. Bools
        // are routed via the True/False branches below — explicit guard
        // here in case any future schema stores a bool as "0"/"1".
        if (ok && type != SettingDef::Combo && type != SettingDef::Bool) {
            preview->setProperty(propName.toUtf8().constData(), asInt);
        } else if (val.compare("true", Qt::CaseInsensitive) == 0) {
            preview->setProperty(propName.toUtf8().constData(), true);
        } else if (val.compare("false", Qt::CaseInsensitive) == 0) {
            preview->setProperty(propName.toUtf8().constData(), false);
        } else {
            preview->setProperty(propName.toUtf8().constData(), val);
        }
    };

    for (auto it = spec.keyToProperty.constBegin();
         it != spec.keyToProperty.constEnd(); ++it) {
        const QString& key = it.key();
        const QString& propName = it.value();
        for (const auto& d : m_schema) {
            if (d.key != key) continue;
            const QString cur = app->settingValue(emuId, d.section, d.key);
            const QString val = cur.isEmpty() ? d.defaultValue : cur;
            seed(propName, val, d.type);
            break;
        }
    }

    // Live updates: connect change signals from each bound widget to a
    // setProperty() call on the preview.
    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const QString key = combo->settingDef().key;
        if (!spec.keyToProperty.contains(key)) continue;
        const QString prop = spec.keyToProperty.value(key);
        connect(combo, &SettingsComboRow::valueChanged, preview,
                [preview, prop](const QString& v) {
                    preview->setProperty(prop.toUtf8().constData(), v);
                });
    }
    for (auto* tog : findChildren<SettingsToggleRow*>()) {
        const QString key = tog->settingDef().key;
        if (!spec.keyToProperty.contains(key)) continue;
        const QString prop = spec.keyToProperty.value(key);
        connect(tog, &SettingsToggleRow::toggled, preview,
                [preview, prop](bool on) {
                    preview->setProperty(prop.toUtf8().constData(), on);
                });
    }
    for (auto* slider : findChildren<SettingsSliderRow*>()) {
        const QString key = slider->settingDef().key;
        if (!spec.keyToProperty.contains(key)) continue;
        const QString prop = spec.keyToProperty.value(key);
        connect(slider, &SettingsSliderRow::valueChanged, preview,
                [preview, prop](int v) {
                    preview->setProperty(prop.toUtf8().constData(), v);
                });
    }
}
