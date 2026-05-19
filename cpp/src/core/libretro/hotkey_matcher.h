#pragma once
#include <QObject>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>
#include <Qt>
#include <atomic>

/**
 * Host-side hotkey matcher.
 *
 * Holds per-action keyboard and gamepad bindings, detects press-edges on
 * incoming Qt key events and gamepad button events, and emits actionPressed /
 * actionReleased signals.
 *
 * Gamepad support covers single-button bindings and combo (modifier+button)
 * chords. Matched combos populate a suppression set so the input router can
 * mask the modifier button from the libretro core.
 *
 * Threading: setBinding/onKeyEvent/onGamepadButton must be called from the
 * same thread (typically the Qt main thread). No internal locks.
 */
class HotkeyMatcher : public QObject {
    Q_OBJECT
public:
    explicit HotkeyMatcher(QObject* parent = nullptr) : QObject(parent) {}

    // Static accessor used by CoreRuntime's input trampoline (worker thread)
    // to consult the currently-active matcher's suppression set. AppController
    // sets this in its constructor and clears it in its destructor.
    static std::atomic<HotkeyMatcher*> s_active;

    // Replace the binding for one action. Empty bindingString clears it.
    // Currently parses only "Keyboard/<KeySequenceText>" (e.g. "Keyboard/F1",
    // "Keyboard/Shift+F2"). Unparseable strings are silently dropped.
    void setBinding(const QString& actionKey, const QString& bindingString);

    // Drop every binding and any held state.
    void clearAllBindings();

    // Qt keyboard event entry point. qtKey is the combined value
    // QKeyEvent::key() | int(QKeyEvent::modifiers()).
    void onKeyEvent(int qtKey, bool pressed);

    // Gamepad input entry point. port is the libretro port (0..3 typically),
    // button is the raw libretro RetroPad button index.
    void onGamepadButton(int port, int button, bool pressed);

    // Returns true if (port, button) is currently acting as a combo modifier
    // in a matched combo. Used by the input router (Task 10) to mask the
    // modifier button from the libretro core's view of the gamepad state.
    bool isSuppressed(int port, int button) const;

signals:
    // Emitted on the press-edge (false→true) for every bound action.
    void actionPressed(const QString& actionKey);

    // Emitted on the release-edge (true→false) ONLY for hold-style
    // actions (currently just "FastForwardHold").
    void actionReleased(const QString& actionKey);

private:
    struct KeyBinding { int qtKey; };
    // modifier == -1 for single-button bindings; >= 0 for combo bindings.
    struct GamepadBinding { int port; int modifier; int button; };

    QHash<QString, KeyBinding>     m_keyBindings;   // action → key
    QHash<int, QString>            m_keyToAction;   // key → action (reverse lookup)
    QHash<QString, GamepadBinding> m_padBindings;   // action → (port, mod, btn)
    // m_padToAction is intentionally removed: onGamepadButton now does a linear
    // scan over m_padBindings (max ~22 entries) to handle both single-button and
    // combo bindings uniformly.
    QHash<QString, bool>           m_actionPressed; // current "is held" state per action
    QHash<int, QSet<int>>          m_padHeld;       // port → currently-held buttons
    QSet<qint64>                   m_suppressed;    // padKey(port, modifier) currently suppressed
    mutable QMutex                 m_suppressedMutex; // guards m_suppressed (worker-thread reads)

    static bool isHoldAction(const QString& actionKey);
    static qint64 padKey(int port, int button) {
        return (qint64(port) << 32) | qint64(uint32_t(button));
    }
};
