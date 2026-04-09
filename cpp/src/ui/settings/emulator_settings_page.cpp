#include "ui/settings/emulator_settings_page.h"
#include "ui/app_controller.h"
#include "adapters/adapter_registry.h"
#include "adapters/emulator_adapter.h"
#include "core/bitmask_helpers.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSlider>
#include <QPushButton>
#include <QTabBar>
#include <QScrollArea>
#include <QFrame>
#include <QAbstractItemView>
#include <QListView>
#include <QSet>
#include <QPainter>
#include <QDir>
#include <algorithm>

// ── Theme constants ──────────────────────────────────────────────────
static const char* kBg          = "#242440";
static const char* kSidebarBg   = "#282848";
static const char* kDivider     = "#353558";
static const char* kText        = "#e8e8ff";
static const char* kTextMuted   = "#9898bb";
static const char* kAccent      = "#e8922a";
static const char* kFieldBg     = "#282846";
static const char* kFieldBorder = "#44446a";
static const char* kActiveBg    = "#3a3a6a";

// INI key format is "section/key" — section may contain slashes, so split on LAST '/'.
static QString makeValueKey(const QString& section, const QString& key) {
    return section + "/" + key;
}

// Checkbox-style "true"/"false" round-trip with the mixed-case variants
// emulators write (PCSX2/DuckStation lowercase, PPSSPP capitalized).
static bool boolFromIni(const QString& raw, const QString& defaultValue) {
    const QString& source = raw.isEmpty() ? defaultValue : raw;
    return source.compare("true", Qt::CaseInsensitive) == 0;
}

// ── Stylesheet helpers ──────────────────────────────────────────────
static QString labelStyle() {
    return QString("QLabel { color: %1; font-size: 13px; font-weight: 600; } QLabel:disabled { color: %2; }")
        .arg(kText, kTextMuted);
}

// Generate arrow PNGs once and return {upPath, downPath}
static QPair<QString,QString> spinArrowPaths() {
    static QString up, dn;
    if (!up.isEmpty()) return {up, dn};

    QString dir = QDir::tempPath() + "/retronest_arrows";
    QDir().mkpath(dir);
    up = dir + "/up.png";
    dn = dir + "/down.png";

    auto paint = [](const QString& path, bool isUp) {
        int w = 16, h = 10;
        QPixmap px(w * 2, h * 2);  // 2x for retina
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(kText));
        QPolygonF tri;
        if (isUp)
            tri << QPointF(4, h*2-4) << QPointF(w, 4) << QPointF(w*2-4, h*2-4);
        else
            tri << QPointF(4, 4) << QPointF(w, h*2-4) << QPointF(w*2-4, 4);
        p.drawPolygon(tri);
        p.end();
        px.save(path, "PNG");
    };
    paint(up, true);
    paint(dn, false);
    return {up, dn};
}

static QString comboStyle() {
    auto [upPath, downPath] = spinArrowPaths();
    return QString::asprintf(
        "QComboBox { background: %s; border: 1px solid %s; border-radius: 6px; "
        "  color: %s; padding: 3px 24px 3px 10px; font-size: 12px; }"
        "QComboBox:hover { border-color: %s; }"
        "QComboBox::drop-down { border: none; width: 22px; }"
        "QComboBox::down-arrow { image: url(%s); width: 8px; height: 5px; }",
        kFieldBg, kFieldBorder, kText, kAccent, downPath.toUtf8().constData());
}

static QString spinStyle() {
    auto [upPath, downPath] = spinArrowPaths();
    return QString(
        "QSpinBox, QDoubleSpinBox { background: %1; border: 1px solid %2; border-radius: 4px; "
        "  color: %3; padding: 3px 6px; font-size: 12px; }"
        "QSpinBox:disabled, QDoubleSpinBox:disabled { color: %4; background: %5; }"
        "QSpinBox::up-button, QDoubleSpinBox::up-button { "
        "  subcontrol-origin: border; subcontrol-position: top right; width: 20px; "
        "  border-left: 1px solid %2; border-top-right-radius: 4px; background: %1; }"
        "QSpinBox::down-button, QDoubleSpinBox::down-button { "
        "  subcontrol-origin: border; subcontrol-position: bottom right; width: 20px; "
        "  border-left: 1px solid %2; border-bottom-right-radius: 4px; background: %1; }"
        "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover, "
        "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background: %6; }"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(%7); width: 8px; height: 5px; }"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(%8); width: 8px; height: 5px; }"
        "QSpinBox::up-arrow:disabled, QDoubleSpinBox::up-arrow:disabled, "
        "QSpinBox::down-arrow:disabled, QDoubleSpinBox::down-arrow:disabled { image: none; }"
    ).arg(kFieldBg, kFieldBorder, kText, kTextMuted, kBg, kActiveBg, upPath, downPath);
}

static QString lineEditStyle() {
    return QString(
        "QLineEdit { background: %1; border: 1px solid %2; border-radius: 4px; "
        "  color: %3; padding: 3px 6px; font-size: 12px; }"
    ).arg(kFieldBg, kFieldBorder, kText);
}

static QString checkBoxStyle() {
    return QString(
        "QCheckBox { color: %1; font-size: 12px; spacing: 6px; padding: 2px 0; }"
        "QCheckBox:disabled { color: %4; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border-radius: 3px; "
        "  border: 1.5px solid %2; background: transparent; }"
        "QCheckBox::indicator:checked { background: %3; border-color: %3; }"
        "QCheckBox::indicator:disabled { border-color: %4; background: transparent; }"
    ).arg(kText, kFieldBorder, kAccent, kTextMuted);
}

static QString sliderStyle() {
    return QString(
        "QSlider::groove:horizontal { background: %1; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: white; width: 14px; height: 14px; "
        "  margin: -5px 0; border-radius: 7px; }"
        "QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }"
    ).arg(kFieldBorder, kAccent);
}

// ═════════════════════════════════════════════════════════════════════
// Constructor
// ═════════════════════════════════════════════════════════════════════

EmulatorSettingsPage::EmulatorSettingsPage(AppController* appController,
                                           const QString& emuId,
                                           QWidget* parent)
    : QDialog(parent)
    , m_appController(appController)
    , m_emuId(emuId)
{
    setWindowTitle("Emulator Settings");
    setMinimumSize(950, 550);
    resize(950, 650);
    setStyleSheet(QString("QDialog { background: %1; }").arg(kBg));

    // Load schema from adapter
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (adapter)
        m_schema = adapter->settingsSchema();

    // Extract unique categories preserving order
    QSet<QString> seen;
    for (const auto& def : m_schema) {
        QString cat = def.category.isEmpty() ? QStringLiteral("General") : def.category;
        if (!seen.contains(cat)) {
            seen.insert(cat);
            m_categories.append(cat);
        }
    }

    buildUI();
}

// ═════════════════════════════════════════════════════════════════════
// Build the main layout: sidebar | content
// ═════════════════════════════════════════════════════════════════════

void EmulatorSettingsPage::buildUI() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Left sidebar ─────────────────────────────────────────────────
    auto* sidebar = new QWidget;
    sidebar->setFixedWidth(180);
    sidebar->setStyleSheet(QString("background: %1;").arg(kSidebarBg));

    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(8, 16, 8, 8);
    sidebarLayout->setSpacing(0);

    m_categoryList = new QListWidget;
    m_categoryList->setFrameShape(QFrame::NoFrame);
    m_categoryList->setStyleSheet(QString(
        "QListWidget { background: transparent; border: none; outline: none; }"
        "QListWidget::item { color: %1; font-size: 14px; padding: 8px 12px; "
        "  border-radius: 8px; margin: 1px 0; }"
        "QListWidget::item:selected { background: %2; color: %3; font-weight: bold; }"
        "QListWidget::item:hover:!selected { background: %4; }"
    ).arg(kTextMuted, kActiveBg, kText, kDivider));

    for (const auto& cat : m_categories)
        m_categoryList->addItem(cat);

    connect(m_categoryList, &QListWidget::currentRowChanged,
            this, &EmulatorSettingsPage::onCategoryChanged);

    sidebarLayout->addWidget(m_categoryList, 1);

    // Button to open native emulator settings
    auto* openSettingsBtn = new QPushButton("Open Native Settings");
    openSettingsBtn->setFixedHeight(28);
    openSettingsBtn->setCursor(Qt::PointingHandCursor);
    openSettingsBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; "
        "  border-radius: 6px; font-size: 12px; padding: 6px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(kTextMuted, kDivider));
    connect(openSettingsBtn, &QPushButton::clicked, this, [this]() {
        m_appController->openNativeEmulatorSettings(m_emuId);
    });
    sidebarLayout->addWidget(openSettingsBtn);

    // Divider line between sidebar and content
    auto* divider = new QFrame;
    divider->setFrameShape(QFrame::VLine);
    divider->setFixedWidth(1);
    divider->setStyleSheet(QString("background: %1; border: none;").arg(kDivider));

    // ── Right content area ───────────────────────────────────────────
    m_contentStack = new QStackedWidget;
    m_contentStack->setStyleSheet(QString("background: %1;").arg(kBg));

    for (const auto& cat : m_categories)
        m_contentStack->addWidget(buildCategoryPage(cat));

    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(divider);
    mainLayout->addWidget(m_contentStack, 1);

    // Select first category
    if (m_categoryList->count() > 0)
        m_categoryList->setCurrentRow(0);
}

// ═════════════════════════════════════════════════════════════════════
// Build a page for one category (may have sub-tabs)
// ═════════════════════════════════════════════════════════════════════

QWidget* EmulatorSettingsPage::buildCategoryPage(const QString& category) {
    // Find subcategories for this category
    QStringList subcats;
    QSet<QString> seen;
    for (const auto& def : m_schema) {
        QString cat = def.category.isEmpty() ? QStringLiteral("General") : def.category;
        if (cat != category) continue;
        if (!def.subcategory.isEmpty() && !seen.contains(def.subcategory)) {
            seen.insert(def.subcategory);
            subcats.append(def.subcategory);
        }
    }

    auto* page = new QWidget;
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    if (subcats.isEmpty()) {
        // No subcategories — show content directly
        pageLayout->addWidget(buildSubcategoryContent(category, QString()));
    } else {
        // Sub-tabs
        auto* tabBar = new QTabBar;
        tabBar->setExpanding(false);
        tabBar->setStyleSheet(QString(
            "QTabBar { background: transparent; border: none; }"
            "QTabBar::tab { color: %1; font-size: 13px; padding: 10px 16px; "
            "  border: none; border-bottom: 2px solid transparent; }"
            "QTabBar::tab:selected { color: %2; border-bottom-color: %3; font-weight: bold; }"
            "QTabBar::tab:hover:!selected { color: %2; }"
        ).arg(kTextMuted, kText, kAccent));

        for (const auto& sub : subcats)
            tabBar->addTab(sub);

        // Divider below tabs
        auto* tabDivider = new QFrame;
        tabDivider->setFrameShape(QFrame::HLine);
        tabDivider->setFixedHeight(1);
        tabDivider->setStyleSheet(QString("background: %1; border: none;").arg(kDivider));

        auto* subStack = new QStackedWidget;
        for (const auto& sub : subcats)
            subStack->addWidget(buildSubcategoryContent(category, sub));

        connect(tabBar, &QTabBar::currentChanged, subStack, &QStackedWidget::setCurrentIndex);

        pageLayout->addWidget(tabBar);
        pageLayout->addWidget(tabDivider);
        pageLayout->addWidget(subStack, 1);
    }

    return page;
}

// ═════════════════════════════════════════════════════════════════════
// Build scrollable content for one category+subcategory combo
// ═════════════════════════════════════════════════════════════════════

QWidget* EmulatorSettingsPage::buildSubcategoryContent(const QString& category,
                                                        const QString& subcategory) {
    // Filter settings for this category+subcategory
    QVector<SettingDef> filtered;
    for (const auto& def : m_schema) {
        const QString cat = def.category.isEmpty() ? QStringLiteral("General") : def.category;
        if (cat != category) continue;
        if (def.subcategory == subcategory)
            filtered.append(def);
    }

    // Group settings by group name
    QStringList groupOrder;
    QMap<QString, QVector<SettingDef>> groups;
    for (const auto& def : filtered) {
        if (!groups.contains(def.group))
            groupOrder.append(def.group);
        groups[def.group].append(def);
    }

    // Build scrollable content
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; }"
        "QScrollBar::handle:vertical { background: #444466; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    );

    auto* content = new QWidget;
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 12, 16, 12);
    contentLayout->setSpacing(6);

    // Separate form groups from all-bool groups for multi-column layout
    int firstAllBool = -1;
    for (int g = 0; g < groupOrder.size(); ++g) {
        const auto& defs = groups[groupOrder[g]];
        bool allBool = !defs.isEmpty() && std::all_of(defs.begin(), defs.end(),
            [](const SettingDef& d) { return d.type == SettingDef::Bool; });
        if (allBool && firstAllBool < 0) firstAllBool = g;
        if (!allBool && firstAllBool >= 0) { firstAllBool = -1; break; } // mixed, don't use columns
    }

    // If 3+ consecutive all-bool groups at the end, use multi-column layout
    bool useColumns = (firstAllBool >= 0 && (groupOrder.size() - firstAllBool) >= 3);

    for (int g = 0; g < groupOrder.size(); ++g) {
        if (useColumns && g == firstAllBool) {
            // Render remaining all-bool groups in a 3-column grid
            auto* columnsWidget = new QWidget;
            auto* columnsLayout = new QHBoxLayout(columnsWidget);
            columnsLayout->setContentsMargins(0, 0, 0, 0);
            columnsLayout->setSpacing(0);

            // Determine column count (max 3)
            int remaining = groupOrder.size() - firstAllBool;
            int numCols = qMin(remaining, 3);

            // Create column layouts
            QVector<QVBoxLayout*> cols;
            for (int c = 0; c < numCols; ++c) {
                auto* col = new QVBoxLayout;
                col->setContentsMargins(0, 0, 0, 0);
                col->setSpacing(4);
                col->setAlignment(Qt::AlignTop);
                columnsLayout->addLayout(col, 1);
                cols.append(col);
            }

            // Distribute groups: first 3 get one column each, extras go into last column
            for (int gi = firstAllBool; gi < groupOrder.size(); ++gi) {
                int colIdx = qMin(gi - firstAllBool, numCols - 1);
                cols[colIdx]->addWidget(buildGroupBox(groupOrder[gi], groups[groupOrder[gi]], true));
            }

            for (auto* col : cols)
                col->addStretch(1);

            contentLayout->addWidget(columnsWidget, 1);
            break; // all remaining groups handled
        }

        contentLayout->addWidget(buildGroupBox(groupOrder[g], groups[groupOrder[g]]));
    }

    contentLayout->addStretch(1);
    scrollArea->setWidget(content);
    return scrollArea;
}

// ═════════════════════════════════════════════════════════════════════
// Build a group box with all its settings
// ═════════════════════════════════════════════════════════════════════

QWidget* EmulatorSettingsPage::buildGroupBox(const QString& groupName,
                                              const QVector<SettingDef>& settings,
                                              bool singleColumnBools) {
    auto* container = new QWidget;
    auto* outerLayout = new QVBoxLayout(container);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(2);

    // Group header (small text above the box)
    if (!groupName.isEmpty() && !groupName.startsWith('_')) {
        auto* header = new QLabel(groupName);
        header->setStyleSheet(QString::asprintf(
            "color: %s; font-size: 11px; font-weight: 500; padding: 0 0 2px 2px;", kTextMuted));
        outerLayout->addWidget(header);
    }

    // Group box (transparent)
    auto* box = new QFrame;
    box->setStyleSheet("QFrame#groupBox { background: transparent; border: none; }");
    box->setObjectName("groupBox");

    auto* boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(12, 8, 12, 8);
    boxLayout->setSpacing(4);

    // Collect which keys are depended on
    QSet<QString> dependedOnKeys;
    for (const auto& def : settings) {
        if (!def.dependsOn.isEmpty())
            dependedOnKeys.insert(def.dependsOn);
    }

    // Track widgets that depend on a master key (bool or combo)
    QMap<QString, QVector<QWidget*>> dependentWidgets;
    QMap<QString, QCheckBox*> masterCheckboxes;
    QMap<QString, QComboBox*> masterCombos;

    // Helper lambda to create a checkbox and wire it up
    auto makeCheckbox = [this, &dependedOnKeys, &masterCheckboxes, &dependentWidgets](const SettingDef& def) -> QCheckBox* {
        auto* check = new QCheckBox(def.label);
        check->setStyleSheet(checkBoxStyle());
        if (!def.tooltip.isEmpty()) check->setToolTip(def.tooltip);

        const QString val = m_appController->settingValue(m_emuId, def.section, def.key);

        if (def.bitmask != 0) {
            // Bitmask checkbox: int-valued key, this widget owns one bit.
            const int intVal = val.isEmpty() ? def.defaultValue.toInt() : val.toInt();
            check->setChecked(BitmaskHelpers::getBit(intVal, def.bitmask));
        } else {
            check->setChecked(boolFromIni(val, def.defaultValue));
        }

        const QString section      = def.section;
        const QString key          = def.key;
        const int bitmask          = def.bitmask;
        const QString defaultValue = def.defaultValue;

        connect(check, &QCheckBox::toggled, this,
            [this, section, key, bitmask, defaultValue](bool checked) {
                QVariantMap values;
                if (bitmask != 0) {
                    // Re-read current int from disk so multiple bitmask
                    // checkboxes sharing the same key merge correctly.
                    const QString cur = m_appController->settingValue(m_emuId, section, key);
                    const int curInt = cur.isEmpty() ? defaultValue.toInt() : cur.toInt();
                    const int newInt = BitmaskHelpers::setBit(curInt, bitmask, checked);
                    values[makeValueKey(section, key)] = QString::number(newInt);
                } else {
                    values[makeValueKey(section, key)] = checked ? "true" : "false";
                }
                m_appController->saveSettings(m_emuId, values);
            });

        if (dependedOnKeys.contains(def.key))
            masterCheckboxes[def.key] = check;
        if (!def.dependsOn.isEmpty())
            dependentWidgets[def.dependsOn].append(check);

        return check;
    };

    // Writes a single section/key pair back to the emulator config file.
    auto saveScalar = [this](const QString& section, const QString& key, const QString& value) {
        QVariantMap values;
        values[makeValueKey(section, key)] = value;
        m_appController->saveSettings(m_emuId, values);
    };

    // Helper lambda to create a control widget from a SettingDef
    auto makeControl = [this, &dependedOnKeys, &masterCombos, saveScalar](const SettingDef& def) -> QWidget* {
        const QString section = def.section;
        const QString key     = def.key;
        const QString val     = m_appController->settingValue(m_emuId, section, key);

        if (def.type == SettingDef::Combo) {
            auto* combo = new QComboBox;
            auto* listView = new QListView(combo);
            listView->setStyleSheet(QString::asprintf(
                "QListView { background: %s; color: %s; border: none; outline: none; "
                "  border-radius: 6px; padding: 4px 0; }"
                "QListView::item { padding: 4px 12px; margin: 0; }"
                "QListView::item:hover { background: %s; color: #ffffff; }"
                "QListView::item:selected { background: %s; color: #ffffff; }",
                kFieldBg, kText, kAccent, kAccent));
            combo->setView(listView);
            combo->setStyleSheet(comboStyle());
            combo->setFixedHeight(28);
            combo->view()->parentWidget()->setStyleSheet(QString::asprintf(
                "background: %s; border: 1px solid %s; border-radius: 6px;",
                kFieldBg, kFieldBorder));

            const QString currentVal = val.isEmpty() ? def.defaultValue : val;
            int selectIdx = 0;
            for (int j = 0; j < def.options.size(); ++j) {
                combo->addItem(def.options[j].first, def.options[j].second);
                if (def.options[j].second == currentVal || def.options[j].first == currentVal)
                    selectIdx = j;
            }
            combo->setCurrentIndex(selectIdx);

            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [combo, section, key, saveScalar](int idx) {
                    QString iniVal = combo->itemData(idx).toString();
                    if (iniVal.isEmpty()) iniVal = combo->itemText(idx);
                    saveScalar(section, key, iniVal);
                });

            if (dependedOnKeys.contains(def.key))
                masterCombos[def.key] = combo;
            return combo;
        }

        if (def.type == SettingDef::Int) {
            auto* spin = new QSpinBox;
            spin->setStyleSheet(spinStyle());
            spin->setFixedHeight(28);
            spin->setRange(static_cast<int>(def.minVal), static_cast<int>(def.maxVal));
            spin->setSingleStep(static_cast<int>(def.step));
            if (!def.suffix.isEmpty()) spin->setSuffix(" " + def.suffix);
            spin->setValue(val.isEmpty() ? def.defaultValue.toInt() : val.toInt());

            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [section, key, saveScalar](int v) {
                    saveScalar(section, key, QString::number(v));
                });
            return spin;
        }

        if (def.type == SettingDef::Float) {
            auto* spin = new QDoubleSpinBox;
            spin->setStyleSheet(spinStyle());
            spin->setFixedHeight(28);
            spin->setRange(def.minVal, def.maxVal);
            spin->setSingleStep(def.step);
            if (!def.suffix.isEmpty()) spin->setSuffix(" " + def.suffix);
            spin->setValue(val.isEmpty() ? def.defaultValue.toDouble() : val.toDouble());

            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [section, key, saveScalar](double v) {
                    saveScalar(section, key, QString::number(v));
                });
            return spin;
        }

        // Default: string-valued line edit
        auto* edit = new QLineEdit;
        edit->setStyleSheet(lineEditStyle());
        edit->setFixedHeight(28);
        edit->setText(val.isEmpty() ? def.defaultValue : val);

        connect(edit, &QLineEdit::editingFinished, this,
            [edit, section, key, saveScalar]() {
                saveScalar(section, key, edit->text());
            });
        return edit;
    };

    // ── Render all settings in definition order (DuckStation style) ──
    for (int i = 0; i < settings.size(); ) {
        const auto& def = settings[i];

        if (def.layout == "paired") {
            // Collect consecutive paired items
            QVector<SettingDef> batch;
            while (i < settings.size() && settings[i].layout == "paired") {
                batch.append(settings[i]);
                ++i;
            }

            // Optional prefix label from first item's tooltip (e.g. "Crop")
            QString prefixText;
            if (!batch.isEmpty() && !batch[0].tooltip.isEmpty())
                prefixText = batch[0].tooltip;

            auto* rowLayout = new QHBoxLayout;
            rowLayout->setContentsMargins(0, 2, 0, 2);
            rowLayout->setSpacing(8);

            if (!prefixText.isEmpty()) {
                auto* prefixLabel = new QLabel(prefixText + ":");
                prefixLabel->setStyleSheet(labelStyle());
                prefixLabel->setFixedWidth(120);
                prefixLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                rowLayout->addWidget(prefixLabel);
                if (!batch[0].dependsOn.isEmpty())
                    dependentWidgets[batch[0].dependsOn].append(prefixLabel);
            }

            auto* grid = new QGridLayout;
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setHorizontalSpacing(8);
            grid->setVerticalSpacing(4);

            for (int j = 0; j < batch.size(); ++j) {
                const auto& pd = batch[j];
                int row = j / 2;
                int col = (j % 2) * 2;

                if (pd.type == SettingDef::Bool) {
                    auto* check = makeCheckbox(pd);
                    grid->addWidget(check, row, col, 1, 2);
                } else {
                    if (!pd.label.isEmpty()) {
                        auto* label = new QLabel(pd.label + ":");
                        label->setStyleSheet(labelStyle());
                        label->setMinimumWidth(40);
                        grid->addWidget(label, row, col);
                        if (!pd.dependsOn.isEmpty())
                            dependentWidgets[pd.dependsOn].append(label);
                    }

                    QWidget* control = makeControl(pd);
                    grid->addWidget(control, row, pd.label.isEmpty() ? col : col + 1);
                    if (!pd.dependsOn.isEmpty())
                        dependentWidgets[pd.dependsOn].append(control);
                }
            }

            // Make value columns stretch to fill
            grid->setColumnStretch(1, 1);
            if (grid->columnCount() > 3)
                grid->setColumnStretch(3, 1);

            rowLayout->addLayout(grid, 1);
            boxLayout->addLayout(rowLayout);

        } else if (def.type == SettingDef::Bool && def.layout != "slider") {
            // Collect consecutive bools into a 2-column grid
            QVector<SettingDef> boolBatch;
            while (i < settings.size() && settings[i].type == SettingDef::Bool
                   && settings[i].layout != "paired" && settings[i].layout != "slider") {
                boolBatch.append(settings[i]);
                ++i;
            }

            auto* grid = new QGridLayout;
            grid->setContentsMargins(0, 2, 0, 2);
            grid->setHorizontalSpacing(20);
            grid->setVerticalSpacing(4);

            int boolCols = singleColumnBools ? 1 : 2;
            for (int j = 0; j < boolBatch.size(); ++j) {
                auto* check = makeCheckbox(boolBatch[j]);
                grid->addWidget(check, j / boolCols, j % boolCols);
            }

            boxLayout->addLayout(grid);

        } else if (def.layout == "inline") {
            // Collect consecutive inline items onto one row (e.g. Left/Top/Right/Bottom)
            QVector<SettingDef> batch;
            while (i < settings.size() && settings[i].layout == "inline") {
                batch.append(settings[i]);
                ++i;
            }

            auto* row = new QHBoxLayout;
            row->setContentsMargins(0, 2, 0, 2);
            row->setSpacing(8);

            // Prefix label from first item's tooltip (used as row title)
            if (!batch.isEmpty() && !batch[0].tooltip.isEmpty()) {
                auto* prefixLabel = new QLabel(batch[0].tooltip + ":");
                prefixLabel->setStyleSheet(labelStyle());
                prefixLabel->setFixedWidth(120);
                row->addWidget(prefixLabel);
                if (!batch[0].dependsOn.isEmpty())
                    dependentWidgets[batch[0].dependsOn].append(prefixLabel);
            }

            for (const auto& item : batch) {
                auto* label = new QLabel(item.label + ":");
                label->setStyleSheet(labelStyle());
                row->addWidget(label);

                QWidget* control = makeControl(item);
                control->setFixedWidth(70);
                row->addWidget(control);

                if (!item.dependsOn.isEmpty()) {
                    dependentWidgets[item.dependsOn].append(label);
                    dependentWidgets[item.dependsOn].append(control);
                }
            }

            row->addStretch(1);
            boxLayout->addLayout(row);

        } else if (def.layout == "slider") {
            auto* row = new QHBoxLayout;
            row->setContentsMargins(0, 2, 0, 2);
            row->setSpacing(12);

            auto* label = new QLabel(def.label + ":");
            label->setStyleSheet(labelStyle());
            label->setFixedWidth(120);
            row->addWidget(label);

            auto* slider = new QSlider(Qt::Horizontal);
            slider->setStyleSheet(sliderStyle());
            slider->setRange(static_cast<int>(def.minVal), static_cast<int>(def.maxVal));
            slider->setSingleStep(static_cast<int>(def.step));
            const QString val = m_appController->settingValue(m_emuId, def.section, def.key);
            slider->setValue(val.isEmpty() ? def.defaultValue.toInt() : val.toInt());

            const QString suffixText = def.suffix.isEmpty() ? QString() : " " + def.suffix;

            auto* valueLabel = new QLabel(QString::number(slider->value()) + suffixText);
            valueLabel->setStyleSheet(labelStyle());
            valueLabel->setFixedWidth(60);
            valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            const QString section = def.section;
            const QString key     = def.key;
            connect(slider, &QSlider::valueChanged, this,
                [valueLabel, section, key, suffixText, saveScalar](int v) {
                    valueLabel->setText(QString::number(v) + suffixText);
                    saveScalar(section, key, QString::number(v));
                });

            row->addWidget(slider, 1);
            row->addWidget(valueLabel);
            boxLayout->addLayout(row);

            if (!def.dependsOn.isEmpty()) {
                dependentWidgets[def.dependsOn].append(label);
                dependentWidgets[def.dependsOn].append(slider);
                dependentWidgets[def.dependsOn].append(valueLabel);
            }
            ++i;

        } else if (def.layout == "resolution") {
            if (i + 2 < settings.size()) {
                const auto& resW = settings[i];
                const auto& resH = settings[i+1];
                const auto& resAuto = settings[i+2];

                auto* row = new QHBoxLayout;
                row->setContentsMargins(0, 4, 0, 4);
                row->setSpacing(8);

                auto* label = new QLabel("Resolution:");
                label->setStyleSheet(labelStyle());
                label->setFixedWidth(80);
                row->addWidget(label);

                auto* wSpin = new QSpinBox;
                wSpin->setStyleSheet(spinStyle());
                wSpin->setFixedHeight(28);
                wSpin->setFixedWidth(90);
                wSpin->setRange(1, 7680);
                QString wVal = m_appController->settingValue(m_emuId, resW.section, resW.key);
                wSpin->setValue(wVal.isEmpty() ? resW.defaultValue.toInt() : wVal.toInt());

                auto* xLabel = new QLabel("x");
                xLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(kTextMuted));

                auto* hSpin = new QSpinBox;
                hSpin->setStyleSheet(spinStyle());
                hSpin->setFixedHeight(28);
                hSpin->setFixedWidth(90);
                hSpin->setRange(1, 4320);
                QString hVal = m_appController->settingValue(m_emuId, resH.section, resH.key);
                hSpin->setValue(hVal.isEmpty() ? resH.defaultValue.toInt() : hVal.toInt());

                auto* autoCheck = new QCheckBox("Auto");
                autoCheck->setStyleSheet(checkBoxStyle());
                const QString autoVal = m_appController->settingValue(m_emuId, resAuto.section, resAuto.key);
                const bool isAuto = boolFromIni(autoVal, resAuto.defaultValue);
                autoCheck->setChecked(isAuto);
                wSpin->setEnabled(!isAuto);
                hSpin->setEnabled(!isAuto);

                const QString autoSection = resAuto.section;
                const QString autoKey     = resAuto.key;
                connect(autoCheck, &QCheckBox::toggled, this,
                    [wSpin, hSpin, autoSection, autoKey, saveScalar](bool checked) {
                        wSpin->setEnabled(!checked);
                        hSpin->setEnabled(!checked);
                        saveScalar(autoSection, autoKey, checked ? "true" : "false");
                    });

                const QString wSection = resW.section, wKey = resW.key;
                connect(wSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [wSection, wKey, saveScalar](int v) {
                        saveScalar(wSection, wKey, QString::number(v));
                    });

                const QString hSection = resH.section, hKey = resH.key;
                connect(hSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [hSection, hKey, saveScalar](int v) {
                        saveScalar(hSection, hKey, QString::number(v));
                    });

                row->addWidget(wSpin);
                row->addWidget(xLabel);
                row->addWidget(hSpin);
                row->addWidget(autoCheck);
                row->addStretch(1);
                boxLayout->addLayout(row);
                i += 3;
            } else {
                ++i;
            }

        } else {
            // Normal full-width setting (combo, int, float, string)
            auto* row = new QHBoxLayout;
            row->setContentsMargins(0, 2, 0, 2);
            row->setSpacing(16);

            auto* label = new QLabel(def.label + ":");
            label->setStyleSheet(labelStyle());
            label->setFixedWidth(180);
            row->addWidget(label);

            QWidget* control = makeControl(def);
            if (def.layout == "readonly") {
                control->setEnabled(false);
                label->setEnabled(false);
            }
            row->addWidget(control, 1);
            boxLayout->addLayout(row);

            if (!def.dependsOn.isEmpty()) {
                dependentWidgets[def.dependsOn].append(label);
                dependentWidgets[def.dependsOn].append(control);
            }
            ++i;
        }
    }

    // ── Wire up master bool → dependent widget enable/disable ────────
    for (auto it = masterCheckboxes.constBegin(); it != masterCheckboxes.constEnd(); ++it) {
        QCheckBox* master = it.value();
        const QVector<QWidget*>& deps = dependentWidgets[it.key()];

        // Set initial state
        bool enabled = master->isChecked();
        for (QWidget* w : deps)
            w->setEnabled(enabled);

        // Connect toggled signal
        connect(master, &QCheckBox::toggled, this,
            [deps](bool checked) {
                for (QWidget* w : deps)
                    w->setEnabled(checked);
            });
    }

    // ── Wire up master combo → dependent widget enable/disable ───────
    for (auto it = masterCombos.constBegin(); it != masterCombos.constEnd(); ++it) {
        QComboBox* master = it.value();
        const QVector<QWidget*>& deps = dependentWidgets[it.key()];

        // Enabled when not the first option (index > 0)
        bool enabled = master->currentIndex() > 0;
        for (QWidget* w : deps)
            w->setEnabled(enabled);

        connect(master, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [deps](int idx) {
                bool en = idx > 0;
                for (QWidget* w : deps)
                    w->setEnabled(en);
            });
    }

    outerLayout->addWidget(box);
    return container;
}

// ═════════════════════════════════════════════════════════════════════
// Slots
// ═════════════════════════════════════════════════════════════════════

void EmulatorSettingsPage::onCategoryChanged(int row) {
    if (row >= 0 && row < m_contentStack->count())
        m_contentStack->setCurrentIndex(row);
}

