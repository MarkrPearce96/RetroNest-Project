#pragma once

#include <QObject>
#include <QPointer>

class QQmlEngine;
class QQmlComponent;
class QQuickWindow;

/**
 * LibretroOverlayPanel — owns the fullscreen transparent QQuickWindow
 * loaded from AppUI/LibretroOverlayPanel.qml. Used by Pattern B HW-render
 * libretro cores (PCSX2 today) to render the in-game menu, RA toasts, RA
 * indicator bar, and FF / save / load pills above the Metal NSView,
 * which would otherwise composite on top of any in-scene QML overlay.
 *
 * Lifecycle: the C++ instance lives for AppController's lifetime. The
 * underlying QQuickWindow is created lazily on the first
 * showForCurrentGame() call, hidden + destroyed on hide(), and recreated
 * on the next show. mGBA / software-libretro / external-emulator paths
 * never trigger this panel.
 *
 * Modelled on InGameMenuPanel; the two coexist (external emulators use
 * the existing one).
 */
class LibretroOverlayPanel : public QObject {
    Q_OBJECT
public:
    explicit LibretroOverlayPanel(QQmlEngine* engine, QObject* parent = nullptr);
    ~LibretroOverlayPanel() override;

    /**
     * Create the QQuickWindow if needed, position it over RetroNest's
     * main window, attach it as a macOS child window above the main
     * window, and show it. The window starts with mouse events ignored
     * so toasts / badges are visible but clicks fall through to the
     * game NSView below.
     */
    void showForCurrentGame();

    /** Hide and destroy the underlying QQuickWindow. */
    void hide();

    /** Show the in-game menu inside the panel. Clears
     *  setIgnoresMouseEvents and makes the panel the system key window. */
    void openMenu();

    /** Close the in-game menu and reapply setIgnoresMouseEvents. */
    void closeMenu();

    bool isMenuOpen() const;

signals:
    void resumeRequested();
    void exitWithSaveRequested();
    void exitWithoutSaveRequested();
    void saveStateRequested();
    void loadStateRequested();
    void toggleFastForwardRequested();
    void menuVisibleChanged();

private:
    void ensureCreated();
    void wireSignals();

    QQmlEngine* m_engine;
    QQmlComponent* m_component = nullptr;
    QPointer<QQuickWindow> m_window;
    bool m_menuOpen = false;
};
