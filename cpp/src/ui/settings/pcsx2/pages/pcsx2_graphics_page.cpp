#include "pcsx2_graphics_page.h"
#include "pcsx2_graphics_stub_sub_page.h"
#include "pcsx2_graphics_rendering_page.h"
#include "pcsx2_graphics_post_processing_page.h"
#include "pcsx2_graphics_display_page.h"
#include "pcsx2_graphics_osd_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_graphics_sub_tab_bar.h"
#include "ui/app_controller.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>

Pcsx2GraphicsPage::Pcsx2GraphicsPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 14, 24, 8);
    root->setSpacing(12);

    auto* back = new QPushButton("\u2190 Back", this);
    back->setCursor(Qt::PointingHandCursor);
    back->setStyleSheet(
        "QPushButton { background:transparent; color:#f2efe8; border:none;"
        " font-size:14px; padding:4px 0; }"
        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &Pcsx2SettingsDialog::popPage);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(back, 0, Qt::AlignLeft);
    topRow->addStretch();
    root->addLayout(topRow);

    m_tabBar = new Pcsx2GraphicsSubTabBar(this);
    m_tabBar->addTab(QStringLiteral("\U0001F5A5"), "Display");
    m_tabBar->addTab(QStringLiteral("\U0001F3A8"), "Rendering");
    m_tabBar->addTab(QStringLiteral("\u2728"),     "Post-Proc");
    m_tabBar->addTab(QStringLiteral("\U0001F4CA"), "OSD");
    connect(m_tabBar, &Pcsx2GraphicsSubTabBar::tabActivated,
            this, &Pcsx2GraphicsPage::onSubTabActivated);
    root->addWidget(m_tabBar, 0, Qt::AlignLeft);

    m_stack = new QStackedWidget(this);

    AppController* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    // 0: Display (real — Plan 3)
    auto* display = new Pcsx2GraphicsDisplayPage(m_dialog);
    connect(display, &Pcsx2GraphicsDisplayPage::settingFocused,
            this, &Pcsx2GraphicsPage::settingFocused);
    m_stack->addWidget(display);

    // 1: Rendering (real — Plan 2)
    auto* rendering = new Pcsx2GraphicsRenderingPage(m_dialog);
    connect(rendering, &Pcsx2GraphicsRenderingPage::settingFocused,
            this, &Pcsx2GraphicsPage::settingFocused);
    m_stack->addWidget(rendering);

    // 2: Post-Processing (real — Plan 2)
    auto* postProc = new Pcsx2GraphicsPostProcessingPage(m_dialog);
    connect(postProc, &Pcsx2GraphicsPostProcessingPage::settingFocused,
            this, &Pcsx2GraphicsPage::settingFocused);
    m_stack->addWidget(postProc);

    // 3: OSD (real — Plan 4)
    auto* osd = new Pcsx2GraphicsOsdPage(m_dialog);
    connect(osd, &Pcsx2GraphicsOsdPage::settingFocused,
            this, &Pcsx2GraphicsPage::settingFocused);
    m_stack->addWidget(osd);

    root->addWidget(m_stack, 1);

    // Plan 3: Display is now a real page, so land on it by default.
    m_tabBar->setCurrentIndex(0);
    m_stack->setCurrentIndex(0);
}

void Pcsx2GraphicsPage::onSubTabActivated(int index) {
    m_stack->setCurrentIndex(index);
    m_dialog->clearFocusedSetting();
}
