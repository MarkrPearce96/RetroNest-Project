#pragma once
#include <QObject>
#include <QVariantList>
#include <functional>
#include <memory>

class GameSession;
class HotkeyMatcher;
class HotkeyDispatcher;
class SdlInputManager;
class QEvent;

/**
 * LibretroHotkeyController — the app-global libretro hotkey engine
 * (matcher + dispatcher + qApp key-event filter + suppression +
 * binding-sync), extracted from AppController (app-shell review P1).
 *
 * Construction wires the whole engine from two injected functions:
 *  - SessionProvider: returns the active GameSession (or nullptr) —
 *    every dispatched action routes through it, so actions harmlessly
 *    no-op when no libretro game is running.
 *  - SuppressionCheck: optional extra host-side veto evaluated per key
 *    event (AppController injects "a Qt modal widget is active").
 *
 * Suppression is the union of three sources:
 *  - setUiSuppressed(): the QML modal-policy Binding
 *    (app.libretroHotkeysSuppressed) forwards here.
 *  - acquireSuppression()/releaseSuppression(): a REFCOUNT for widget
 *    surfaces that must inhibit hotkeys for their lifetime regardless of
 *    how they were opened — HotkeySettingsDialog holds one while it
 *    exists so binding capture can never fire the action being bound
 *    (review R3: this used to rely on "dialogs only open via the
 *    settings overlay" keeping the QML flag asserted).
 *  - the injected SuppressionCheck.
 *
 * The controller installs itself as a qApp event filter; tests drive
 * handleKeyEvent() directly. Gamepad input arrives via
 * attachInputManager() (SdlInputManager::gamepadButtonChanged).
 *
 * The matcher pointer (matcher()) is handed to GameSession →
 * CoreRuntime as an explicit injection for the worker-thread
 * combo-modifier suppression lookup — the old hidden
 * HotkeyMatcher::s_active static is gone.
 */
class LibretroHotkeyController : public QObject {
    Q_OBJECT
public:
    using SessionProvider  = std::function<GameSession*()>;
    using SuppressionCheck = std::function<bool()>;

    explicit LibretroHotkeyController(SessionProvider sessionProvider,
                                      SuppressionCheck extraSuppression = {},
                                      QObject* parent = nullptr);
    ~LibretroHotkeyController() override;

    /** Replace all bindings. Rows are {key, currentValue} maps (the
     *  HotkeyService row shape for the libretro sentinel emulator). */
    void setBindings(const QVariantList& rows);

    /** QML-driven suppression (the modal-policy Binding). */
    void setUiSuppressed(bool suppressed);

    /** Refcounted suppression for widget surfaces (see class docs). */
    void acquireSuppression();
    void releaseSuppression();

    /** Effective suppression across all three sources. */
    bool suppressed() const;

    /** Route SdlInputManager::gamepadButtonChanged into the matcher. */
    void attachInputManager(SdlInputManager* mgr);

    /** Clear cached gamepad state (menu open/close makes it stale). */
    void resetGamepadState();

    /** Keyboard entry point (combinedKey = key | modifiers). Returns true
     *  when a bound action fired and the event should be consumed. */
    bool handleKeyEvent(int combinedKey, bool pressed);

    bool eventFilter(QObject* watched, QEvent* event) override;

    /** The matcher instance — injected into GameSession/CoreRuntime for
     *  the worker-thread combo-suppression lookup. */
    HotkeyMatcher* matcher() const { return m_matcher.get(); }

signals:
    /** The user's ToggleMenu binding fired. QML selects the right menu
     *  surface (overlay panel vs in-scene HUD). */
    void menuToggleRequested();

    /** Generic toast request (currently the save-slot change notice). */
    void infoToastRequested(const QString& header, const QString& title,
                            const QString& description,
                            const QString& imageUrl, int durationMs);

private:
    SessionProvider  m_session;
    SuppressionCheck m_extraSuppression;
    bool m_uiSuppressed  = false;
    int  m_suppressCount = 0;

    std::unique_ptr<HotkeyMatcher>    m_matcher;
    std::unique_ptr<HotkeyDispatcher> m_dispatcher;
};
