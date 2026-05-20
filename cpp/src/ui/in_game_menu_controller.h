#pragma once

#include <QObject>
#include <QPointer>
#include <functional>

class QQmlEngine;
class InGameMenuPanel;
class LibretroOverlayPanel;

/**
 * InGameMenuController — single owner + façade for the two backing in-game
 * menu surfaces.
 *
 * The app has two physically different windows that host the in-game HUD:
 *
 *   - InGameMenuPanel       — frameless NSPanel floating over an external
 *                             emulator's fullscreen window
 *                             (PCSX2 / DuckStation / PPSSPP / Dolphin).
 *   - LibretroOverlayPanel  — transparent QQuickWindow above RetroNest's
 *                             main window, hosts the menu + RA toasts +
 *                             FF / save / load chips when a HW-render
 *                             libretro core (PCSX2 libretro) is running.
 *
 * (mGBA + future software libretro cores use an in-scene QML overlay
 * inside AppWindow — that path stays on the QML side and doesn't go
 * through this controller.)
 *
 * Callers (AppController + QML) used to talk to two parallel APIs with
 * separate visibility properties, lazy-create hooks, and action-signal
 * wirings. This controller collapses both behind one open/close pair, one
 * visibility property, and one set of action signals. Routing between the
 * two backends is decided by an `isHardwareRender` predicate supplied at
 * construction — the same one QML uses to pick its menu path, so menu
 * opens land on whichever backend matches the running game.
 */
class InGameMenuController : public QObject {
    Q_OBJECT
public:
    using IsHardwareRenderFn = std::function<bool()>;

    InGameMenuController(QQmlEngine* engine,
                         IsHardwareRenderFn isHardwareRender,
                         QObject* parent = nullptr);
    ~InGameMenuController() override;

    /**
     * Open the in-game menu using whichever backend matches the running
     * game. For external emulators, `emulatorPid` positions the floating
     * panel over that process's screen; for libretro it's ignored
     * (the overlay window is already positioned over the main window).
     */
    void openMenu(int64_t emulatorPid = 0);

    /** Close the menu (whichever backend is currently presenting it). */
    void closeMenu();

    /** True iff one of the backends is currently presenting the menu. */
    bool isMenuOpen() const;

    /**
     * Libretro overlay lifecycle — always-on while a HW-render libretro
     * game is alive, independent of menu open/close (it hosts RA toasts
     * + indicator bar above the Metal NSView). No-op for external games.
     */
    void showLibretroOverlayForCurrentGame();
    void hideLibretroOverlay();

    /**
     * True if the currently-active backend is the HW-render libretro
     * overlay. Action handlers check this to decide between
     * GameSession-direct calls (libretro) and process keystroke
     * synthesis (external). Backend choice is locked in at the moment
     * openMenu() runs.
     */
    bool currentBackendIsLibretro() const;

signals:
    /** Either backend's visibility changed. Edge-triggered. */
    void menuOpenChanged();

    /**
     * Forwarded action signals — identical set on both inner panels.
     * AppController owns the policy of what each action does (since it
     * differs between libretro vs external); the controller just routes
     * the user's click upward.
     */
    void resumeRequested();
    void exitWithSaveRequested();
    void exitWithoutSaveRequested();
    void saveStateRequested();
    void loadStateRequested();
    void toggleFastForwardRequested();

private:
    void ensureExternalPanel();
    void ensureLibretroPanel();

    QQmlEngine* m_engine = nullptr;
    IsHardwareRenderFn m_isHardwareRender;

    // Lazy-instantiated. The C++ instances live for AppController's
    // lifetime once created; the inner QQuickWindow is created/destroyed
    // per game session by each panel internally.
    InGameMenuPanel* m_externalPanel = nullptr;
    LibretroOverlayPanel* m_libretroPanel = nullptr;

    // Which backend the most recent openMenu() routed to. Sticky until
    // the next openMenu() — closeMenu() reads this to know which inner
    // panel to dismiss without re-querying isHardwareRender (which may
    // have changed if a libretro game just ended).
    enum class Backend { None, External, LibretroHW };
    Backend m_activeBackend = Backend::None;
};
