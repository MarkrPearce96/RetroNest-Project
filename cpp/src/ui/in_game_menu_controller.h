#pragma once

#include <QObject>
#include <functional>

class QQmlEngine;
class LibretroOverlayPanel;

/**
 * InGameMenuController — owner + façade for the HW-render in-game menu
 * surface, `LibretroOverlayPanel`: a transparent QQuickWindow above
 * RetroNest's main window hosting the menu + RA toasts + FF / save /
 * load chips while a HW-render libretro core is running.
 *
 * (mGBA + future software libretro cores use an in-scene QML overlay
 * inside AppWindow — that path stays on the QML side and doesn't go
 * through this controller.)
 *
 * Process-era retirement (2026-07): this used to route between two
 * backends — the overlay above vs `InGameMenuPanel`, a frameless NSPanel
 * floated over an external emulator process's fullscreen window. Every
 * emulator is an in-process libretro core now, so the NSPanel backend
 * and its pid-based screen positioning were deleted with the process
 * era (recover from git history if a standalone emulator ever returns).
 */
class InGameMenuController : public QObject {
    Q_OBJECT
public:
    explicit InGameMenuController(QQmlEngine* engine, QObject* parent = nullptr);
    ~InGameMenuController() override;

    /**
     * Pause-policy hook, called with `true` on the menu-open edge and
     * `false` on the close edge — the "core paused exactly while the
     * menu is open, resume before every close or the libretro core's
     * EmuThread watchdog kills the session" invariant lives HERE, on the
     * state transition, so every close path (action buttons, toggle
     * hotkey, programmatic closeMenu) resumes for free instead of each
     * caller remembering to. AppController installs a hook that maps
     * true/false onto GameSession pause/resumeEmulation; a redundant
     * resume is a safe no-op in the cores (ResumeVm guards prev_state).
     */
    using PauseHook = std::function<void(bool paused)>;
    void setPauseHook(PauseHook hook) { m_pauseHook = std::move(hook); }

    /** Open the in-game menu on the overlay window. */
    void openMenu();

    /** Close the menu. */
    void closeMenu();

    /** True iff the menu is currently presented. */
    bool isMenuOpen() const;

    /**
     * Libretro overlay lifecycle — always-on while a HW-render libretro
     * game is alive, independent of menu open/close (it hosts RA toasts
     * + indicator bar above the Metal NSView).
     */
    void showLibretroOverlayForCurrentGame();
    void hideLibretroOverlay();

signals:
    /** Overlay visibility changed. Edge-triggered. */
    void menuOpenChanged();

    /**
     * Forwarded action signals from the overlay panel. AppController owns
     * the policy of what each action does; the controller just routes the
     * user's click upward.
     */
    void resumeRequested();
    void exitWithSaveRequested();
    void exitWithoutSaveRequested();
    void saveStateRequested();
    void loadStateRequested();
    void toggleFastForwardRequested();

private:
    void ensureLibretroPanel();

    QQmlEngine* m_engine = nullptr;
    PauseHook m_pauseHook;
    bool m_lastMenuOpen = false;   // edge detection for the hook

    // Lazy-instantiated. The C++ instance lives for AppController's
    // lifetime once created; the inner QQuickWindow is created/destroyed
    // per game session by the panel internally.
    LibretroOverlayPanel* m_libretroPanel = nullptr;
};
