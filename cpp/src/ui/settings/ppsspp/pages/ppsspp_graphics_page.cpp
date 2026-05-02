#include "ppsspp_graphics_page.h"
#include "ppsspp_graphics_rendering_page.h"
#include "ppsspp_graphics_performance_page.h"
#include "ppsspp_graphics_textures_page.h"
#include "ppsspp_graphics_pacing_fx_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_graphics_sub_tab_bar.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_toggle.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QApplication>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QTimer>

PpssppGraphicsPage::PpssppGraphicsPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 14, 24, 8);
    root->setSpacing(12);

    auto* back = new QPushButton(QString::fromUtf8("\xE2\x86\x90 Back"), this);
    back->setCursor(Qt::PointingHandCursor);
    back->setFocusPolicy(Qt::NoFocus);
    back->setStyleSheet(
        "QPushButton { background:transparent; color:#f2efe8; border:none;"
        " font-size:14px; padding:4px 0; }"
        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &PpssppSettingsDialog::popPage);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(back, 0, Qt::AlignLeft);
    topRow->addStretch();
    root->addLayout(topRow);

    m_tabBar = new Pcsx2GraphicsSubTabBar(this);
    m_tabBar->setFocusPolicy(Qt::NoFocus);
    m_tabBar->addTab(QStringLiteral("\U0001F3A8"), "Rendering");
    m_tabBar->addTab(QStringLiteral("⚡"),     "Performance");
    m_tabBar->addTab(QStringLiteral("\U0001F9F1"), "Textures");
    m_tabBar->addTab(QStringLiteral("✨"),     "Pacing & FX");
    connect(m_tabBar, &Pcsx2GraphicsSubTabBar::tabActivated,
            this, &PpssppGraphicsPage::onSubTabActivated);
    root->addWidget(m_tabBar, 0, Qt::AlignLeft);

    m_stack = new QStackedWidget(this);

    auto* rendering = new PpssppGraphicsRenderingPage(m_dialog);
    connect(rendering, &PpssppGraphicsRenderingPage::settingFocused,
            this, &PpssppGraphicsPage::settingFocused);
    m_stack->addWidget(rendering);

    auto* performance = new PpssppGraphicsPerformancePage(m_dialog);
    connect(performance, &PpssppGraphicsPerformancePage::settingFocused,
            this, &PpssppGraphicsPage::settingFocused);
    m_stack->addWidget(performance);

    auto* textures = new PpssppGraphicsTexturesPage(m_dialog);
    connect(textures, &PpssppGraphicsTexturesPage::settingFocused,
            this, &PpssppGraphicsPage::settingFocused);
    m_stack->addWidget(textures);

    auto* pacing = new PpssppGraphicsPacingFxPage(m_dialog);
    connect(pacing, &PpssppGraphicsPacingFxPage::settingFocused,
            this, &PpssppGraphicsPage::settingFocused);
    m_stack->addWidget(pacing);

    root->addWidget(m_stack, 1);

    m_tabBar->setCurrentIndex(0);
    m_stack->setCurrentIndex(0);

    QTimer::singleShot(0, this, [this]{ focusFirstSettingOnCurrentTab(); });

    qApp->installEventFilter(this);
}

PpssppGraphicsPage::~PpssppGraphicsPage() {
    qApp->removeEventFilter(this);
}

void PpssppGraphicsPage::onSubTabActivated(int index) {
    m_stack->setCurrentIndex(index);
    m_dialog->clearFocusedSetting();
    focusFirstSettingOnCurrentTab();
}

bool PpssppGraphicsPage::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        QWidget* current = QApplication::focusWidget();
        if (!current || !isAncestorOf(current))
            return QWidget::eventFilter(obj, e);

        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            const int count = m_tabBar->tabCount();
            if (count < 2) return QWidget::eventFilter(obj, e);
            int next = m_tabBar->currentIndex();
            if (ke->key() == Qt::Key_Backtab || (ke->modifiers() & Qt::ShiftModifier))
                next = (next - 1 + count) % count;
            else
                next = (next + 1) % count;
            m_tabBar->setCurrentIndex(next);
            return true;
        }
    }
    return QWidget::eventFilter(obj, e);
}

void PpssppGraphicsPage::focusFirstSettingOnCurrentTab() {
    QWidget* page = m_stack->currentWidget();
    if (!page) return;
    for (QWidget* w : page->findChildren<QWidget*>()) {
        if (!w->isVisible()) continue;
        if (w->focusPolicy() == Qt::NoFocus) continue;
        if (qobject_cast<QComboBox*>(w)    ||
            qobject_cast<QSlider*>(w)      ||
            qobject_cast<Pcsx2Toggle*>(w)  ||
            (qobject_cast<Pcsx2Card*>(w) && w->focusPolicy() != Qt::NoFocus)) {
            w->setFocus(Qt::TabFocusReason);
            return;
        }
    }
}
