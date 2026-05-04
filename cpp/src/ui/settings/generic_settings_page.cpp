#include "generic_settings_page.h"
#include "emulator_settings_dialog_base.h"
#include "settings_page_builder.h"
#include "widgets/settings_graphics_sub_tab_bar.h"
#include "widgets/settings_section_header.h"
#include "ui/app_controller.h"
#include "adapters/emulator_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>

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

void GenericSettingsPage::buildSubcategory(const QString& /*subcategory*/) {
    // Implemented in Task 10.
}

void GenericSettingsPage::loadValues() {
    // Implemented in Task 11.
}

void GenericSettingsPage::saveValue(const QString& /*section*/,
                                    const QString& /*key*/,
                                    const QString& /*value*/) {
    // Implemented in Task 11.
}

void GenericSettingsPage::refreshDependencies() {
    // Implemented in Task 13.
}

bool GenericSettingsPage::eventFilter(QObject* obj, QEvent* e) {
    return QWidget::eventFilter(obj, e);  // Spatial nav implemented in Task 14.
}
