#include "generic_settings_page.h"
#include "adapters/emulator_adapter.h"
#include "core/setting_dependency.h"
#include "emulator_settings_dialog_base.h"
#include "settings_page_builder.h"
#include "ui/app_controller.h"
#include "widgets/preview/aspect_ratio_preview.h"
#include "widgets/preview/osd_preview.h"
#include "widgets/settings_card.h"
#include "widgets/settings_combo_row.h"
#include "widgets/settings_graphics_sub_tab_bar.h"
#include "widgets/settings_section_header.h"
#include "widgets/settings_slider_row.h"
#include "widgets/settings_toggle.h"
#include "widgets/settings_toggle_row.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <limits>

namespace {
// ─── Layout chrome (shared across buildUi and buildSubcategory) ───
// Centralized so a designer tweaking spacing in one place doesn't
// silently desync the math elsewhere.
constexpr int kRootContentMargin = 24;     // root layout left/right
constexpr int kRootContentMarginV = 16;    // root layout top/bottom
constexpr int kRootSpacing = 10;           // root layout inter-item spacing
constexpr int kSubLayoutSpacing = 8;       // sub-stack page inter-item spacing
constexpr int kColumnSpacing = 14;         // between left/right columns in topRow
constexpr int kStackSpacing = 12;          // between cards in left/right column
constexpr int kPreviewCardChromeH = 14;    // preview card contentsMargins L/R
constexpr int kPreviewCardChromeV = 12;    // preview card contentsMargins T/B
constexpr int kCardBorderPx = 1;           // QSS border on SettingsCard, each side
// Outer chrome (vertical) added on top of the inner preview widget to
// arrive at the preview card's outer height: 12 + 12 (margins) + 1 + 1
// (1px QSS borders both sides). Used by the runtime measurement in
// buildSubcategory.
constexpr int kPreviewCardOuterChromeV =
    kPreviewCardChromeV * 2 + kCardBorderPx * 2;

// Number of preview-bound cards beside the preview before alternation
// kicks in. This is a *design choice*, not a per-emulator detail:
// 4 cards gives a comfortable visual rhythm beside a 16:9 preview at
// the typical column width. Bumping to 3 or 5 would rebalance the
// preview-vs-cards ratio across all emulators.
constexpr int kCardsBesidePreview = 4;
} // namespace

GenericSettingsPage::GenericSettingsPage(EmulatorSettingsDialogBase *dlg,
                                         QVector<SettingDef> categorySchema,
                                         EmulatorAdapter *adapter,
                                         QWidget *parent)
    : QWidget(parent), m_dlg(dlg), m_adapter(adapter),
      m_schema(std::move(categorySchema)) {
  if (!m_schema.isEmpty())
    m_category = m_schema.front().category;

  // Discover ordered, unique subcategories (preserving first-seen order).
  QSet<QString> seen;
  for (const auto &d : m_schema) {
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

GenericSettingsPage::~GenericSettingsPage() { qApp->removeEventFilter(this); }

void GenericSettingsPage::buildUi() {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setStyleSheet(SettingsPageBuilder::kScrollAreaQss);
  outer->addWidget(scroll);

  auto *content = new QWidget(scroll);
  // Cap the scroll content widget at its layout sizeHint. Without this,
  // QScrollArea::widgetResizable(true) stretches the widget to viewport
  // height, and the trailing addStretch absorbs the gap — visible as an
  // empty area below the last setting on shorter pages.
  content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  scroll->setWidget(content);

  auto *root = new QVBoxLayout(content);
  root->setContentsMargins(kRootContentMargin, kRootContentMarginV,
                           kRootContentMargin, kRootContentMarginV);
  root->setSpacing(kRootSpacing);

  // Back button — NoFocus so Tab/arrows can't sink focus onto it
  // (popPage is reachable via Esc/B-button at the dialog level).
  auto *back = new QPushButton("← Back", content);
  back->setFocusPolicy(Qt::NoFocus);
  back->setStyleSheet(
      "QPushButton { background:transparent; color:#f2efe8; border:none;"
      " font-size:14px; padding:4px 0; text-align:left; }"
      "QPushButton:focus { color:#f59e0b; }");
  connect(back, &QPushButton::clicked, m_dlg,
          &EmulatorSettingsDialogBase::popPage);
  root->addWidget(back);

  // Sub-tab handling: if there's more than one subcategory, render a
  // SettingsGraphicsSubTabBar at the top + a QStackedWidget that swaps
  // on tab change.
  if (m_subcategories.size() > 1) {
    m_subTabBar = new SettingsGraphicsSubTabBar(content);
    // NoFocus — the bar is driven by Tab/Shift-Tab in eventFilter, not
    // by being a Tab stop itself. Otherwise it sinks focus and breaks
    // the spatial-nav contract that focus always lives on a settings
    // card while the page is interactive.
    m_subTabBar->setFocusPolicy(Qt::NoFocus);
    for (const auto &s : m_subcategories) {
      const QString icon = m_adapter
                               ? m_adapter->subcategoryIcon(m_category, s)
                               : QString();
      m_subTabBar->addTab(icon, s);
    }
    root->addWidget(m_subTabBar);

    m_subStack = new QStackedWidget(content);
    // One QStackedWidget page per subcategory — index in the stack
    // matches the index in m_subcategories. Subcategory name itself
    // is not needed here (buildSubcategory looks it up by index).
    for (int i = 0; i < m_subcategories.size(); ++i) {
      auto *sub = new QWidget(m_subStack);
      auto *subLayout = new QVBoxLayout(sub); // populated by buildSubcategory
      subLayout->setContentsMargins(0, 0, 0, 0);
      subLayout->setSpacing(kSubLayoutSpacing);
      m_subStack->addWidget(sub);
    }
    root->addWidget(m_subStack);

    // QStackedWidget sizeHint is the max of all pages by default — that
    // means a short sub-tab (Advanced, OSD) gets allocated the height
    // of the tallest sub-tab (General), opening a big empty area below
    // its last setting. Setting non-current pages to vertical Ignored
    // policy makes m_subStack adopt the *current* page's sizeHint, so
    // each sub-tab is sized to its own content.
    auto syncSubPagePolicies = [this]() {
      for (int i = 0; i < m_subStack->count(); ++i) {
        QWidget *page = m_subStack->widget(i);
        QSizePolicy policy = page->sizePolicy();
        policy.setVerticalPolicy(i == m_subStack->currentIndex()
                                     ? QSizePolicy::Preferred
                                     : QSizePolicy::Ignored);
        page->setSizePolicy(policy);
      }
      m_subStack->updateGeometry();
    };
    connect(m_subStack, &QStackedWidget::currentChanged, this,
            [syncSubPagePolicies](int) { syncSubPagePolicies(); });

    // Sub-tab activation: swap the stack page, clear the description
    // bar (the previously-focused setting is no longer visible), and
    // shift focus to the first card on the new tab so spatial nav
    // continues working without the user having to click.
    connect(m_subTabBar, &SettingsGraphicsSubTabBar::tabActivated, this,
            [this](int index) {
              m_subStack->setCurrentIndex(index);
              m_dlg->clearFocusedSetting();
              focusFirstSettingOnCurrentSubTab();
            });

    // Apply initial policies so the first-shown sub-tab adopts its own
    // sizeHint instead of the max-of-all default.
    syncSubPagePolicies();
  }

  // Per-subcategory build happens here — populated in Task 10.
  for (const QString &s : m_subcategories)
    buildSubcategory(s);

  root->addStretch();
}

void GenericSettingsPage::buildSubcategory(const QString &subcategory) {
  // Identify the parent layout for this subcategory's content.
  QVBoxLayout *layout = nullptr;
  if (m_subStack) {
    const int idx = m_subcategories.indexOf(subcategory);
    layout = qobject_cast<QVBoxLayout *>(m_subStack->widget(idx)->layout());
  } else {
    auto *scroll = findChild<QScrollArea *>();
    Q_ASSERT(scroll && scroll->widget());
    layout = qobject_cast<QVBoxLayout *>(scroll->widget()->layout());
  }
  Q_ASSERT(layout);

  const PreviewSpec spec = m_adapter
                               ? m_adapter->previewSpec(m_category, subcategory)
                               : PreviewSpec{};

  // Two-column layout when a preview is active:
  //   topRow     = leftStack (N preview-bound cards beside preview)
  //                | rightStack (header + preview)
  //   bottomGrid = QGridLayout (2 cols), preview-bound overflow cards
  //                pair-fill into row 0 col 0, row 0 col 1, row 1 col 0…
  //                — siblings in the same grid row align by construction.
  //   Orphan: a lone overflow card on the last row (odd count) re-spans
  //           both columns so it stretches full-width.
  // The preview is sized so its card height matches the natural N-card
  // leftStack height — keeps both topRow column heights equal (no gap
  // before bottomGrid). Non-preview-bound groups flow full-width below
  // bottomGrid.
  QVBoxLayout *leftStack = nullptr;
  QWidget *leftHost = nullptr;
  QGridLayout *bottomGrid = nullptr;
  QWidget *preview = nullptr;
  QWidget *previewBox = nullptr;
  // Only the FIRST encountered preview-bound group is "primary" — its cards
  // share the topRow with the preview and overflow into bottomGrid below.
  // Later preview-bound groups render full-width below the preview block,
  // same shape as non-preview-bound groups; their show* widgets are still
  // live-bound to the preview by wirePreviewBinding (which walks the whole
  // page tree, not just the leftStack/bottomGrid). Without this rule a
  // second preview-bound group's HEADER would land in leftStack while its
  // CARDS landed in bottomGrid — visually disconnected.
  QString primaryPreviewGroup;

  // kColumnSpacing, kStackSpacing, kCardsBesidePreview defined in the
  // anonymous namespace at the top of this file.
  if (!spec.previewType.isEmpty()) {
    QSet<QString> previewKeys;
    for (auto it = spec.keyToProperty.constBegin();
         it != spec.keyToProperty.constEnd(); ++it) {
      previewKeys.insert(it.key());
    }
    // First group (in schema order) that owns at least one spec-mapped key.
    QString firstPreviewGroup;
    for (const auto &d : m_schema) {
      if (d.subcategory == subcategory && previewKeys.contains(d.key)) {
        firstPreviewGroup = d.group;
        break;
      }
    }
    primaryPreviewGroup = firstPreviewGroup;

    // Header row above topRow — both section headers ("VISUAL QUALITY" /
    // "ASPECT RATIO PREVIEW", or whatever) sit at the same y. With the
    // headers OUTSIDE the columns, the first widget inside each column
    // (Internal Resolution on the left, preview card on the right) lines
    // up at exactly the same y — guaranteed by the QHBoxLayout, no
    // dependence on header height matching exactly.
    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(kColumnSpacing);
    auto *leftHeader = new SettingsSectionHeader(firstPreviewGroup, this);
    auto *rightHeader = new SettingsSectionHeader(
        spec.previewType == "osd" ? "OSD Preview" : "Aspect Ratio Preview",
        this);
    // Ignored width so headers split 50/50 — same as topRow / bottomGrid.
    leftHeader->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rightHeader->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    headerRow->addWidget(leftHeader, /*stretch=*/1);
    headerRow->addWidget(rightHeader, /*stretch=*/1);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(kColumnSpacing);

    leftHost = new QWidget(this);
    // Ignore sizeHint width so QHBoxLayout splits the row purely by
    // stretch — without this, the preview card's larger natural width
    // gives the right column more base width than the left, shifting
    // the column boundary and making the bottomGrid's pair-row gap
    // (Aniso↔FTF) sit at a different x than the preview's center.
    leftHost->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    leftStack = new QVBoxLayout(leftHost);
    leftStack->setContentsMargins(0, 0, 0, 0);
    leftStack->setSpacing(kStackSpacing);
    topRow->addWidget(leftHost, /*stretch=*/1);

    auto *rightHost = new QWidget(this);
    rightHost->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *rightStack = new QVBoxLayout(rightHost);
    rightStack->setContentsMargins(0, 0, 0, 0);
    rightStack->setSpacing(kStackSpacing);
    topRow->addWidget(rightHost, /*stretch=*/1);

    auto *card = new SettingsCard(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setPreviewStyle(true);
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(kPreviewCardChromeH, kPreviewCardChromeV,
                          kPreviewCardChromeH, kPreviewCardChromeV);
    v->setSpacing(10);
    preview = mountPreviewWidget(spec.previewType, card);
    if (preview) {
      // Wrap in a plain QWidget container so the outer layout doesn't see
      // the preview's hasHeightForWidth flag (the previews set
      // sp.setHeightForWidth(true) in their constructors). With the wrapper,
      // the outer layout treats the wrapper as a normal fixed-size widget.
      // Size is set later, after the cards loop populates leftStack and we
      // can measure its actual rendered height — that way the preview
      // matches whatever leftStack ends up at, regardless of QSS / border /
      // font tweaks that might shift card heights by a pixel or two.
      previewBox = new QWidget(card);
      auto *boxLayout = new QVBoxLayout(previewBox);
      boxLayout->setContentsMargins(0, 0, 0, 0);
      boxLayout->setSpacing(0);
      boxLayout->addWidget(preview);
      v->addWidget(previewBox, /*stretch=*/0, Qt::AlignHCenter);
    }
    rightStack->addWidget(card, /*stretch=*/0, Qt::AlignTop);
    rightStack->addStretch();

    // Group headerRow + topRow + bottomGrid into a QWidget container
    // with Maximum vertical sizePolicy. The wrapper's sizeHint is the
    // sum of children — and Maximum policy prevents the parent layout
    // from growing it beyond that. Extra page height stays in
    // subLayout's trailing addStretch where it belongs.
    auto *previewBlockHost = new QWidget(this);
    previewBlockHost->setSizePolicy(QSizePolicy::Preferred,
                                     QSizePolicy::Maximum);
    auto *previewBlock = new QVBoxLayout(previewBlockHost);
    previewBlock->setContentsMargins(0, 0, 0, 0);
    previewBlock->setSpacing(kStackSpacing);
    previewBlock->addLayout(headerRow);
    previewBlock->addLayout(topRow);

    bottomGrid = new QGridLayout();
    bottomGrid->setContentsMargins(0, 0, 0, 0);
    bottomGrid->setHorizontalSpacing(kColumnSpacing);
    bottomGrid->setVerticalSpacing(kStackSpacing);
    bottomGrid->setColumnStretch(0, 1);
    bottomGrid->setColumnStretch(1, 1);
    previewBlock->addLayout(bottomGrid);

    layout->addWidget(previewBlockHost);
    m_currentPreview = preview;
  }

  SettingsPageBuilder builder(
      this, m_schema,
      [this](const QString &sec, const QString &k, const QString &v) {
        saveValue(sec, k, v);
      },
      [this](const SettingDef &d) { emit settingFocused(d); });

  // Group entries by SettingDef::group (preserving first-seen order).
  QStringList groupOrder;
  QSet<QString> seenGroups;
  for (const auto &d : m_schema) {
    if (d.subcategory != subcategory)
      continue;
    if (!seenGroups.contains(d.group)) {
      seenGroups.insert(d.group);
      groupOrder.append(d.group);
    }
  }

  auto makeCardFor = [&](const SettingDef &d) -> QWidget * {
    switch (d.type) {
    case SettingDef::Combo:
      return builder.makeComboCard(d.key);
    case SettingDef::Bool:
      return builder.makeToggleCard(d.key);
    case SettingDef::Int:
    case SettingDef::Float:
      if (d.layout == "slider" || d.layout == "paired")
        return builder.makeSliderCard(d.key);
      return nullptr;
    default:
      return nullptr;
    }
  };

  // First kCardsBesidePreview preview-bound cards fill leftStack (beside
  // the preview). Cards past N pair-fill into bottomGrid: row 0 col 0,
  // row 0 col 1, row 1 col 0, … — siblings in the same grid row are
  // aligned by construction (no dependence on exact card-height math).
  int previewBoundCardIndex = 0;
  int overflowIndex = 0;

  for (const QString &group : groupOrder) {
    const bool isPreviewBound = leftStack && group == primaryPreviewGroup;

    // Section header for the primary preview-bound group is already in
    // the headerRow above topRow (so its top aligns with the right
    // column's preview header). All other groups get a full-width header
    // in the bottom layout.
    if (!group.isEmpty()) {
      const bool primaryHeaderAlreadyInHeaderRow =
          isPreviewBound && previewBoundCardIndex == 0;
      if (!primaryHeaderAlreadyInHeaderRow)
        layout->addWidget(new SettingsSectionHeader(group, this));
    }

    QVector<const SettingDef *> groupDefs;
    for (const auto &d : m_schema)
      if (d.subcategory == subcategory && d.group == group)
        groupDefs.append(&d);

    for (int i = 0; i < groupDefs.size(); ++i) {
      const SettingDef &d = *groupDefs[i];
      QWidget *card = makeCardFor(d);
      if (!card)
        continue;

      if (isPreviewBound) {
        if (previewBoundCardIndex < kCardsBesidePreview) {
          leftStack->addWidget(card, /*stretch=*/0, Qt::AlignTop);
        } else {
          const int row = overflowIndex / 2;
          const int col = overflowIndex % 2;
          bottomGrid->addWidget(card, row, col, Qt::AlignTop);
          ++overflowIndex;
        }
        ++previewBoundCardIndex;
      } else {
        // Non-preview-bound groups: full-width below the topRow,
        // with explicit pair handling so layout == "paired" pairs
        // render as side-by-side rows there too.
        if (d.layout == "paired" && i + 1 < groupDefs.size() &&
            groupDefs[i + 1]->layout == "paired") {
          QWidget *card2 = makeCardFor(*groupDefs[i + 1]);
          if (card2) {
            auto *pair = new QHBoxLayout();
            pair->setContentsMargins(0, 0, 0, 0);
            pair->setSpacing(8);
            pair->addWidget(card, /*stretch=*/1);
            pair->addWidget(card2, /*stretch=*/1);
            layout->addLayout(pair);
            ++i; // consume sibling
            continue;
          }
        }
        layout->addWidget(card);
      }
    }
  }

  // Now that leftStack is populated, measure its actual rendered height
  // and size the preview card to match. With both section headers now
  // sitting in a headerRow ABOVE the topRow, leftStack and rightStack
  // each only contain their first card / preview directly — no header
  // inside the column to subtract. previewCardH = leftStackH gives
  // identical column heights, with the first card top aligned with the
  // preview card top by virtue of the topRow's QHBoxLayout.
  if (preview && previewBox && leftHost) {
    leftHost->ensurePolished();
    if (auto *l = leftHost->layout())
      l->activate();
    const int leftStackH = leftHost->minimumSizeHint().height();
    const int previewCardH = qMax(80, leftStackH);
    const int previewHeight = qMax(60, previewCardH - kPreviewCardOuterChromeV);
    // Derive the preview's width from its own heightForWidth function so
    // future preview types (4:3, 21:9, etc.) work without editing this
    // file. AspectRatioPreview/OsdPreview are 16:9 today; sampling at a
    // known width and inverting gives the right answer for any linear
    // aspect-bound widget.
    constexpr int kAspectSampleWidth = 160;
    const int sampledHeight = preview->heightForWidth(kAspectSampleWidth);
    const int previewWidth = (sampledHeight > 0)
        ? (previewHeight * kAspectSampleWidth / sampledHeight)
        : (previewHeight * 16 / 9);
    preview->setFixedSize(previewWidth, previewHeight);
    previewBox->setFixedSize(previewWidth, previewHeight);
    // Cap both columns at exactly leftStackH so topRow can't stretch
    // past the natural leftStack bottom — this guarantees bottomGrid
    // sits 12px below leftStack's last card on every page, regardless
    // of how Qt distributes any vertical space coming from above.
    leftHost->setMaximumHeight(leftStackH);
    if (auto *box = previewBox->parentWidget()) {
      if (auto *outerCard = qobject_cast<SettingsCard *>(box)) {
        if (auto *rightHost = outerCard->parentWidget()) {
          rightHost->setMaximumHeight(leftStackH);
        }
      }
    }
  }

  // Lone overflow card on the last row: re-add with columnSpan=2 so it
  // stretches to full row width instead of sitting alone in column 0.
  if (bottomGrid && overflowIndex % 2 == 1) {
    const int lastRow = overflowIndex / 2;
    if (auto *item = bottomGrid->itemAtPosition(lastRow, 0)) {
      if (auto *w = item->widget()) {
        bottomGrid->removeWidget(w);
        bottomGrid->addWidget(w, lastRow, 0, /*rowSpan=*/1, /*colSpan=*/2,
                              Qt::AlignTop);
      }
    }
  }

  if (preview && !spec.keyToProperty.isEmpty())
    wirePreviewBinding(spec, preview);

  // Top-align leftStack so its cards shrink to their natural height
  // instead of inflating to match the rightStack. Preview is sized so
  // both columns balance, but the rightStack still owns the addStretch
  // (added when the preview card was placed) to absorb any 1-2px drift.
  if (leftStack)
    leftStack->addStretch();
  layout->addStretch();
}

void GenericSettingsPage::loadValues() {
  auto *app = m_dlg->appController();
  const QString emuId = m_dlg->emuId();

  // Block each row's signals during the programmatic seed — otherwise
  // setValue/setChecked emit valueChanged/toggled which the builder's
  // save lambda is connected to, which calls saveValue(), which writes
  // every non-Qt-default value back to disk on every page open. The
  // round-trip is harmless for the default save path but can fire
  // unwanted side effects for SettingDef::saveTransform consumers.
  // Read helper used by SettingDef::loadTransform consumers — they pass
  // (section, key) for each underlying INI key they need to inspect.
  auto readKey = [app, emuId](const QString &sec, const QString &k) {
    return app->settingValue(emuId, sec, k);
  };
  for (auto *combo : findChildren<SettingsComboRow *>()) {
    const SettingDef &d = combo->settingDef();
    const QString cur = d.loadTransform
        ? d.loadTransform(readKey)
        : app->settingValue(emuId, d.section, d.key);
    const QSignalBlocker blocker(combo);
    combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
  }
  for (auto *toggle : findChildren<SettingsToggleRow *>()) {
    const SettingDef &d = toggle->settingDef();
    const QString cur = app->settingValue(emuId, d.section, d.key);
    const QString v = cur.isEmpty() ? d.defaultValue : cur;
    const bool stored = v.compare("true", Qt::CaseInsensitive) == 0;
    const QSignalBlocker blocker(toggle);
    toggle->setChecked(d.inverted ? !stored : stored);
  }
  for (auto *slider : findChildren<SettingsSliderRow *>()) {
    const SettingDef &d = slider->settingDef();
    // Sliders may declare a loadTransform when the displayed unit
    // differs from the stored unit (e.g. Dolphin's MEM1/MEM2 sliders
    // display MB but Dolphin stores bytes via a 1-MiB multiplier).
    const QString cur = d.loadTransform
        ? d.loadTransform(readKey)
        : app->settingValue(emuId, d.section, d.key);
    const QString v = cur.isEmpty() ? d.defaultValue : cur;
    const QSignalBlocker blocker(slider);
    slider->setValue(v.toInt());
  }
}

void GenericSettingsPage::saveValue(const QString &section, const QString &key,
                                    const QString &value) {
  auto *app = m_dlg->appController();
  const QString emuId = m_dlg->emuId();

  auto defaultSave = [app, emuId](const QString &sec, const QString &k,
                                  const QString &v) {
    QVariantMap m;
    m[sec + "/" + k] = v;
    app->saveSettings(emuId, m);
  };

  bool transformed = false;
  for (const auto &d : m_schema) {
    if (d.section == section && d.key == key && d.saveTransform) {
      d.saveTransform(value, defaultSave);
      transformed = true;
      break;
    }
  }
  if (!transformed)
    defaultSave(section, key, value);

  // A toggle change may flip a dependent slider's active state.
  // Cheap to re-evaluate every save — refreshDependencies is O(rows).
  refreshDependencies();
}

void GenericSettingsPage::refreshDependencies() {
  // Master state maps populated from every Toggle / Combo on the page:
  //   masterStates[key] = is the master "active"? (truthy / non-disabled)
  //   masterValues[key] = the master's current raw value (combo equality
  //                       atoms in the dependsOn DSL compare against this).
  QHash<QString, bool> masterStates;
  QHash<QString, QString> masterValues;
  // For inverted toggles, the displayed `isChecked()` is the OPPOSITE of
  // the stored INI value. Dependency expressions reason about the stored
  // value (so e.g. `dependsOn = "BBoxEnable"` means "true when BBox is
  // enabled in the INI"), so we un-invert here before populating the
  // master maps.
  for (auto *tog : findChildren<SettingsToggleRow *>()) {
    const bool stored = tog->settingDef().inverted ? !tog->isChecked()
                                                   : tog->isChecked();
    masterStates.insert(tog->settingDef().key, stored);
    masterValues.insert(tog->settingDef().key,
                        stored ? "true" : "false");
  }

  // Combos can also act as masters (per setting_def.h:39-42): a combo is
  // 'active' when its value is not in {"", "0", "false", "Disabled", "None"}.
  for (auto *combo : findChildren<SettingsComboRow *>()) {
    const QString v = combo->value();
    const bool active = !v.isEmpty() && v != "0" &&
                        v.compare("false", Qt::CaseInsensitive) != 0 &&
                        v.compare("Disabled", Qt::CaseInsensitive) != 0 &&
                        v.compare("None", Qt::CaseInsensitive) != 0;
    masterStates.insert(combo->settingDef().key, active);
    masterValues.insert(combo->settingDef().key, v);
  }

  // dependsOn is parsed as a small boolean expression DSL: a bare key
  // ("Foo") behaves like the legacy truthy check; richer atoms ("!Foo",
  // "Foo=Bar", "Foo!=Bar") and chains joined by '&&' or '||' express
  // multi-condition gates verbatim. See evaluateDependencyExpression in
  // setting_dependency.h.
  auto isDependencyActive = [&](const SettingDef &d) -> bool {
    if (d.dependsOn.isEmpty())
      return true;
    return evaluateDependencyExpression(d.dependsOn, masterStates,
                                         masterValues);
  };

  // Apply opacity + inner-control disable to a row, regardless of widget
  // type. Row stays focusable so arrow-key spatial nav passes through.
  auto applyActive = [](QWidget *row, bool active, QWidget *innerControl) {
    row->setProperty("dependencyActive", active);
    if (innerControl)
      innerControl->setEnabled(active);
    if (!active) {
      if (!row->graphicsEffect()) {
        auto *eff = new QGraphicsOpacityEffect(row);
        eff->setOpacity(0.4);
        row->setGraphicsEffect(eff);
      }
    } else {
      row->setGraphicsEffect(nullptr);
    }
  };

  for (auto *slider : findChildren<SettingsSliderRow *>())
    applyActive(slider, isDependencyActive(slider->settingDef()),
                slider->findChild<QSlider *>());
  for (auto *toggle : findChildren<SettingsToggleRow *>())
    applyActive(toggle, isDependencyActive(toggle->settingDef()),
                toggle->findChild<SettingsToggle *>());
  for (auto *combo : findChildren<SettingsComboRow *>())
    applyActive(combo, isDependencyActive(combo->settingDef()),
                combo->findChild<QComboBox *>());
}

bool GenericSettingsPage::eventFilter(QObject *obj, QEvent *e) {
  if (e->type() != QEvent::KeyPress)
    return QWidget::eventFilter(obj, e);
  auto *ke = static_cast<QKeyEvent *>(e);
  const int k = ke->key();

  QWidget *current = QApplication::focusWidget();
  if (!current || !isAncestorOf(current))
    return QWidget::eventFilter(obj, e);

  // Tab / Shift+Tab cycles through sub-tabs when present (L1/R1 on a
  // controller is mapped to Tab/Backtab by the global input layer).
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

  if (k != Qt::Key_Left && k != Qt::Key_Right && k != Qt::Key_Up &&
      k != Qt::Key_Down)
    return QWidget::eventFilter(obj, e);

  // Combo dropdown open — let the popup handle arrows.
  if (auto *combo = qobject_cast<QComboBox *>(current))
    if (combo->view() && combo->view()->isVisible())
      return QWidget::eventFilter(obj, e);

  // Slider in edit mode handles its own arrows (see
  // SettingsSliderRow::setEditing).
  if (current->property("editing").toBool())
    return QWidget::eventFilter(obj, e);

  SettingsCard *sourceCard = nullptr;
  for (QWidget *w = current; w; w = w->parentWidget()) {
    if (auto *card = qobject_cast<SettingsCard *>(w)) {
      sourceCard = card;
      break;
    }
  }
  if (!sourceCard)
    return QWidget::eventFilter(obj, e);

  if (SettingsCard *next = findNextCardSpatial(sourceCard, k)) {
    next->setFocus(Qt::TabFocusReason);
    for (QWidget *p = next->parentWidget(); p; p = p->parentWidget()) {
      if (auto *sa = qobject_cast<QScrollArea *>(p)) {
        sa->ensureWidgetVisible(next, 20, 40);
        break;
      }
    }
  }
  // Always consume so combos/sliders/toggles don't see the arrow themselves.
  return true;
}

SettingsCard *GenericSettingsPage::findNextCardSpatial(SettingsCard *current,
                                                       int key) const {
  auto pagePoint = [this](QWidget *w) -> QPoint {
    return w->mapTo(const_cast<GenericSettingsPage *>(this), QPoint(0, 0));
  };
  const QRect mine(pagePoint(current), current->size());
  const QPoint myCenter = mine.center();
  const bool vertical = (key == Qt::Key_Up || key == Qt::Key_Down);

  auto rangesOverlap = [](int a0, int a1, int b0, int b1) {
    return a0 < b1 && b0 < a1;
  };

  // First pass: collect every in-direction card whose perpendicular range
  // overlaps the source. Track the closest "next row" (or column) by the
  // primary-axis gap so a strictly closer card always wins over a farther
  // but better-aligned one — without this, a full-width row two rows down
  // can outscore a half-width card immediately below.
  struct Cand {
    SettingsCard *card;
    long long primary;
    long long secondary;
  };
  QList<Cand> cands;
  long long minPrimary = std::numeric_limits<long long>::max();

  for (SettingsCard *card : findChildren<SettingsCard *>()) {
    if (card == current || !card->isVisible() ||
        card->focusPolicy() == Qt::NoFocus)
      continue;
    const QRect r(pagePoint(card), card->size());
    const QPoint c = r.center();
    const int dx = c.x() - myCenter.x();
    const int dy = c.y() - myCenter.y();
    bool inDir = false, perpOverlap = false;
    switch (key) {
    case Qt::Key_Left:
      inDir = dx < 0;
      perpOverlap =
          rangesOverlap(mine.top(), mine.bottom(), r.top(), r.bottom());
      break;
    case Qt::Key_Right:
      inDir = dx > 0;
      perpOverlap =
          rangesOverlap(mine.top(), mine.bottom(), r.top(), r.bottom());
      break;
    case Qt::Key_Up:
      inDir = dy < 0;
      perpOverlap =
          rangesOverlap(mine.left(), mine.right(), r.left(), r.right());
      break;
    case Qt::Key_Down:
      inDir = dy > 0;
      perpOverlap =
          rangesOverlap(mine.left(), mine.right(), r.left(), r.right());
      break;
    }
    if (!inDir || !perpOverlap)
      continue;

    // primary = signed distance along the nav axis (between rects, not centers,
    // so half-width cards on the same row tie). secondary = how far off-axis
    // the candidate's overlap-center is from the source's center.
    long long primary = 0, secondary = 0;
    if (vertical) {
      primary = (key == Qt::Key_Down) ? qMax<int>(0, r.top() - mine.bottom())
                                      : qMax<int>(0, mine.top() - r.bottom());
      const int oL = qMax(mine.left(), r.left());
      const int oR = qMin(mine.right(), r.right());
      secondary = qAbs((oL + oR) / 2 - myCenter.x());
    } else {
      primary = (key == Qt::Key_Right) ? qMax<int>(0, r.left() - mine.right())
                                       : qMax<int>(0, mine.left() - r.right());
      const int oT = qMax(mine.top(), r.top());
      const int oB = qMin(mine.bottom(), r.bottom());
      secondary = qAbs((oT + oB) / 2 - myCenter.y());
    }
    cands.push_back({card, primary, secondary});
    if (primary < minPrimary)
      minPrimary = primary;
  }
  if (cands.isEmpty())
    return nullptr;

  // Within the closest row/column (primary ≈ minPrimary, with a small
  // tolerance for grid spacing differences), pick the most-aligned card.
  constexpr long long kRowTolerance = 8;
  SettingsCard *best = nullptr;
  long long bestSecondary = std::numeric_limits<long long>::max();
  for (const Cand &c : cands) {
    if (c.primary > minPrimary + kRowTolerance)
      continue;
    if (c.secondary < bestSecondary) {
      bestSecondary = c.secondary;
      best = c.card;
    }
  }
  return best;
}

void GenericSettingsPage::focusFirstSettingOnCurrentSubTab() {
  // Search inside the visible sub-stack page when present, otherwise the
  // whole page. First focusable SettingsCard wins so spatial-nav has a
  // starting point.
  QWidget *searchRoot = m_subStack ? m_subStack->currentWidget() : this;
  if (!searchRoot)
    return;
  for (auto *card : searchRoot->findChildren<SettingsCard *>()) {
    if (!card->isVisible())
      continue;
    if (card->focusPolicy() == Qt::NoFocus)
      continue;
    card->setFocus(Qt::TabFocusReason);
    return;
  }
}

QWidget *GenericSettingsPage::mountPreviewWidget(const QString &previewType,
                                                 QWidget *parent) {
  if (previewType == "aspect")
    return new AspectRatioPreview(parent);
  if (previewType == "osd")
    return new OsdPreview(parent);
  return nullptr;
}

void GenericSettingsPage::wirePreviewBinding(const PreviewSpec &spec,
                                             QWidget *preview) {
  auto *app = m_dlg->appController();
  const QString emuId = m_dlg->emuId();

  // Initial sync: read each bound setting's current value and seed the
  // matching preview Q_PROPERTY.
  auto seed = [&](const QString &propName, const QString &val,
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
    const QString &key = it.key();
    const QString &propName = it.value();
    for (const auto &d : m_schema) {
      if (d.key != key)
        continue;
      const QString cur = app->settingValue(emuId, d.section, d.key);
      const QString val = cur.isEmpty() ? d.defaultValue : cur;
      seed(propName, val, d.type);
      break;
    }
  }

  // Live updates: connect change signals from each bound widget to a
  // setProperty() call on the preview.
  for (auto *combo : findChildren<SettingsComboRow *>()) {
    const QString key = combo->settingDef().key;
    if (!spec.keyToProperty.contains(key))
      continue;
    const QString prop = spec.keyToProperty.value(key);
    connect(combo, &SettingsComboRow::valueChanged, preview,
            [preview, prop](const QString &v) {
              preview->setProperty(prop.toUtf8().constData(), v);
            });
  }
  for (auto *tog : findChildren<SettingsToggleRow *>()) {
    const QString key = tog->settingDef().key;
    if (!spec.keyToProperty.contains(key))
      continue;
    const QString prop = spec.keyToProperty.value(key);
    connect(tog, &SettingsToggleRow::toggled, preview,
            [preview, prop](bool on) {
              preview->setProperty(prop.toUtf8().constData(), on);
            });
  }
  for (auto *slider : findChildren<SettingsSliderRow *>()) {
    const QString key = slider->settingDef().key;
    if (!spec.keyToProperty.contains(key))
      continue;
    const QString prop = spec.keyToProperty.value(key);
    connect(slider, &SettingsSliderRow::valueChanged, preview,
            [preview, prop](int v) {
              preview->setProperty(prop.toUtf8().constData(), v);
            });
  }
}
