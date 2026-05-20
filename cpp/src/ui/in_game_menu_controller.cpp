#include "in_game_menu_controller.h"
#include "in_game_menu_panel.h"
#include "libretro_overlay_panel.h"

InGameMenuController::InGameMenuController(QQmlEngine* engine,
                                           IsHardwareRenderFn isHardwareRender,
                                           QObject* parent)
    : QObject(parent),
      m_engine(engine),
      m_isHardwareRender(std::move(isHardwareRender)) {}

InGameMenuController::~InGameMenuController() = default;

void InGameMenuController::openMenu(int64_t emulatorPid) {
    const bool hw = m_isHardwareRender && m_isHardwareRender();
    if (hw) {
        ensureLibretroPanel();
        m_activeBackend = Backend::LibretroHW;
        m_libretroPanel->openMenu();
    } else {
        ensureExternalPanel();
        m_activeBackend = Backend::External;
        m_externalPanel->showOverEmulator(emulatorPid);
    }
}

void InGameMenuController::closeMenu() {
    switch (m_activeBackend) {
    case Backend::External:
        if (m_externalPanel) m_externalPanel->hide();
        break;
    case Backend::LibretroHW:
        if (m_libretroPanel) m_libretroPanel->closeMenu();
        break;
    case Backend::None:
        break;
    }
}

bool InGameMenuController::isMenuOpen() const {
    return (m_externalPanel && m_externalPanel->isVisible())
        || (m_libretroPanel && m_libretroPanel->isMenuOpen());
}

void InGameMenuController::showLibretroOverlayForCurrentGame() {
    ensureLibretroPanel();
    m_libretroPanel->showForCurrentGame();
}

void InGameMenuController::hideLibretroOverlay() {
    if (m_libretroPanel) m_libretroPanel->hide();
}

bool InGameMenuController::currentBackendIsLibretro() const {
    return m_activeBackend == Backend::LibretroHW;
}

void InGameMenuController::ensureExternalPanel() {
    if (m_externalPanel) return;
    m_externalPanel = new InGameMenuPanel(m_engine, this);

    // Forward action signals 1:1. Policy (synth keystroke vs
    // GameSession call) lives on AppController.
    connect(m_externalPanel, &InGameMenuPanel::resumeRequested,
            this, &InGameMenuController::resumeRequested);
    connect(m_externalPanel, &InGameMenuPanel::exitWithSaveRequested,
            this, &InGameMenuController::exitWithSaveRequested);
    connect(m_externalPanel, &InGameMenuPanel::exitWithoutSaveRequested,
            this, &InGameMenuController::exitWithoutSaveRequested);
    connect(m_externalPanel, &InGameMenuPanel::saveStateRequested,
            this, &InGameMenuController::saveStateRequested);
    connect(m_externalPanel, &InGameMenuPanel::loadStateRequested,
            this, &InGameMenuController::loadStateRequested);
    connect(m_externalPanel, &InGameMenuPanel::toggleFastForwardRequested,
            this, &InGameMenuController::toggleFastForwardRequested);
    connect(m_externalPanel, &InGameMenuPanel::visibilityChanged,
            this, &InGameMenuController::menuOpenChanged);
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
    connect(m_libretroPanel, &LibretroOverlayPanel::menuVisibleChanged,
            this, &InGameMenuController::menuOpenChanged);
}
