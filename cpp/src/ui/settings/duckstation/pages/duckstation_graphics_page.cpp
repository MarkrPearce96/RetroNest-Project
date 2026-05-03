#include "duckstation_graphics_page.h"
#include "duckstation_graphics_rendering_page.h"
#include "duckstation_graphics_advanced_page.h"
#include "duckstation_graphics_osd_page.h"
#include "../duckstation_settings_dialog.h"
#include "ui/settings/widgets/settings_graphics_sub_tab_bar.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_toggle.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QApplication>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QTimer>

DuckStationGraphicsPage::DuckStationGraphicsPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 14, 24, 8);
    root->setSpacing(12);

    auto* back = new QPushButton("\u2190 Back", this);
    back->setCursor(Qt::PointingHandCursor);
    back->setFocusPolicy(Qt::NoFocus);
    back->setStyleSheet(
        "QPushButton { background:transparent; color:#f2efe8; border:none;"
        " font-size:14px; padding:4px 0; }"
        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &DuckStationSettingsDialog::popPage);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(back, 0, Qt::AlignLeft);
    topRow->addStretch();
    root->addLayout(topRow);

    m_tabBar = new SettingsGraphicsSubTabBar(this);
    m_tabBar->setFocusPolicy(Qt::NoFocus);
    m_tabBar->addTab(QStringLiteral("\U0001F3A8"), "Rendering");
    m_tabBar->addTab(QStringLiteral("\u2699"),     "Advanced");
    m_tabBar->addTab(QStringLiteral("\U0001F4CA"), "OSD");
    connect(m_tabBar, &SettingsGraphicsSubTabBar::tabActivated,
            this, &DuckStationGraphicsPage::onSubTabActivated);
    root->addWidget(m_tabBar, 0, Qt::AlignLeft);

    m_stack = new QStackedWidget(this);

    // 0: Rendering
    auto* rendering = new DuckStationGraphicsRenderingPage(m_dialog);
    connect(rendering, &DuckStationGraphicsRenderingPage::settingFocused,
            this, &DuckStationGraphicsPage::settingFocused);
    m_stack->addWidget(rendering);
    // 1: Advanced
    auto* advanced = new DuckStationGraphicsAdvancedPage(m_dialog);
    connect(advanced, &DuckStationGraphicsAdvancedPage::settingFocused,
            this, &DuckStationGraphicsPage::settingFocused);
    m_stack->addWidget(advanced);
    // 2: OSD
    auto* osd = new DuckStationGraphicsOsdPage(m_dialog);
    connect(osd, &DuckStationGraphicsOsdPage::settingFocused,
            this, &DuckStationGraphicsPage::settingFocused);
    m_stack->addWidget(osd);

    root->addWidget(m_stack, 1);

    m_tabBar->setCurrentIndex(0);
    m_stack->setCurrentIndex(0);

    QTimer::singleShot(0, this, [this]{ focusFirstSettingOnCurrentTab(); });

    qApp->installEventFilter(this);
}

DuckStationGraphicsPage::~DuckStationGraphicsPage() {
    qApp->removeEventFilter(this);
}

void DuckStationGraphicsPage::onSubTabActivated(int index) {
    m_stack->setCurrentIndex(index);
    m_dialog->clearFocusedSetting();
    focusFirstSettingOnCurrentTab();
}

void DuckStationGraphicsPage::replaceSubPage(int index, QWidget* page) {
    QWidget* old = m_stack->widget(index);
    m_stack->removeWidget(old);
    old->deleteLater();
    m_stack->insertWidget(index, page);
    if (m_stack->currentIndex() == index) {
        m_stack->setCurrentIndex(index);
        focusFirstSettingOnCurrentTab();
    }
}

bool DuckStationGraphicsPage::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        QWidget* current = QApplication::focusWidget();
        if (!current || !isAncestorOf(current))
            return QWidget::eventFilter(obj, e);

        // Tab / Shift+Tab cycles through sub-tabs.
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            const int count = m_tabBar->tabCount();
            if (count < 2) return QWidget::eventFilter(obj, e);
            int next = m_tabBar->currentIndex();
            if (ke->key() == Qt::Key_Backtab || (ke->modifiers() & Qt::ShiftModifier)) {
                next = (next - 1 + count) % count;
            } else {
                next = (next + 1) % count;
            }
            m_tabBar->setCurrentIndex(next);
            return true;
        }
    }
    return QWidget::eventFilter(obj, e);
}

void DuckStationGraphicsPage::focusFirstSettingOnCurrentTab() {
    QWidget* page = m_stack->currentWidget();
    if (!page) return;

    // Try to find the first focusable setting control on the current tab.
    for (QWidget* w : page->findChildren<QWidget*>()) {
        if (!w->isVisible()) continue;
        if (w->focusPolicy() == Qt::NoFocus) continue;
        if (qobject_cast<QComboBox*>(w)    ||
            qobject_cast<QSlider*>(w)      ||
            qobject_cast<SettingsToggle*>(w)  ||
            (qobject_cast<SettingsCard*>(w) && w->focusPolicy() != Qt::NoFocus)) {
            w->setFocus(Qt::TabFocusReason);
            return;
        }
    }
}
