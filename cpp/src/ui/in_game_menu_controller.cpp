#include "in_game_menu_controller.h"
#include "libretro_overlay_panel.h"

InGameMenuController::InGameMenuController(QQmlEngine* engine, QObject* parent)
    : QObject(parent),
      m_engine(engine) {}

InGameMenuController::~InGameMenuController() = default;

void InGameMenuController::openMenu() {
    ensureLibretroPanel();
    m_libretroPanel->openMenu();
}

void InGameMenuController::closeMenu() {
    if (m_libretroPanel) m_libretroPanel->closeMenu();
}

bool InGameMenuController::isMenuOpen() const {
    return m_libretroPanel && m_libretroPanel->isMenuOpen();
}

void InGameMenuController::showLibretroOverlayForCurrentGame() {
    ensureLibretroPanel();
    m_libretroPanel->showForCurrentGame();
}

void InGameMenuController::hideLibretroOverlay() {
    if (m_libretroPanel) m_libretroPanel->hide();
}

void InGameMenuController::ensureLibretroPanel() {
    if (m_libretroPanel) return;
    m_libretroPanel = new LibretroOverlayPanel(m_engine, this);

    connect(m_libretroPanel, &LibretroOverlayPanel::resumeRequested,
            this, &InGameMenuController::resumeRequested);
    connect(m_libretroPanel, &LibretroOverlayPanel::exitWithSaveRequested,
            this, &InGameMenuController::exitWithSaveRequested);
    connect(m_libretroPanel, &LibretroOverlayPanel::exitWithoutSaveRequested,
            this, &InGameMenuController::exitWithoutSaveRequested);
    connect(m_libretroPanel, &LibretroOverlayPanel::saveStateRequested,
            this, &InGameMenuController::saveStateRequested);
    connect(m_libretroPanel, &LibretroOverlayPanel::loadStateRequested,
            this, &InGameMenuController::loadStateRequested);
    connect(m_libretroPanel, &LibretroOverlayPanel::toggleFastForwardRequested,
            this, &InGameMenuController::toggleFastForwardRequested);
    // The open-state EDGE drives the pause policy (see setPauseHook) —
    // pausing here rather than in each caller means no close path can
    // forget to resume.
    connect(m_libretroPanel, &LibretroOverlayPanel::menuVisibleChanged,
            this, [this] {
                const bool open = isMenuOpen();
                if (open == m_lastMenuOpen) return;
                m_lastMenuOpen = open;
                if (m_pauseHook) m_pauseHook(open);
                emit menuOpenChanged();
            });
}
