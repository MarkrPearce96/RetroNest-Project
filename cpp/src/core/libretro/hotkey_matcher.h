#pragma once
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <Qt>
#include <atomic>

/**
 * Host-side hotkey matcher.
 *
 * Each action key (e.g. "ToggleMenu") can be bound to ZERO OR MORE inputs
 * — any mix of keyboard keys and gamepad buttons / combos. A binding
 * string is one or more tokens joined by " & ":
 *
 *   "Keyboard/F1"                       single keyboard binding
 *   "Keyboard/Shift+F2"                 keyboard with modifier
 *   "Gamepad0/8"                        single gamepad button
 *   "Gamepad0/4+8"                      gamepad combo (modifier+button)
 *   "Keyboard/F2 & Gamepad0/8"          either input fires the action
 *
 * Gamepad combos populate a thread-safe suppression set so the input
 * router can mask the modifier button from the libretro core. Press-edge
 * detection is per-action: the action fires once per false→true transition
 * regardless of which bound input drove it. Hold-style actions (currently
 * just "FastForwardHold") also emit actionReleased on true→false.
 *
 * Threading: setBinding/onKeyEvent/onGamepadButton run on the Qt main
 * thread. isSuppressed() runs on the libretro worker thread and reads
 * m_suppressed under m_suppressedMutex.
 */
class HotkeyMatcher : public QObject {
    Q_OBJECT
public:
    explicit HotkeyMatcher(QObject* parent = nullptr) : QObject(parent) {}

    // Replace ALL bindings for one action. Empty bindingString clears them.
    // Tokens separated by " & " are each parsed as either a keyboard or a
    // gamepad binding; unparseable tokens are silently dropped.
    void setBinding(const QString& actionKey, const QString& bindingString);

    // Drop every binding and any held state.
    void clearAllBindings();

    // Clear gamepad held / pressed / suppressed state without dropping
    // bindings. Called when the in-game menu opens or closes — SDL stops
    // delivering button events to us while a menu owns input, so our
    // cached state goes stale and would block the next press-edge.
    void resetGamepadState();

    // Qt keyboard event entry point. qtKey is the combined value
    // QKeyEvent::key() | int(QKeyEvent::modifiers()). Returns true when
    // the event matched a bound action and fired (caller should consume
    // the event so widgets / shortcuts don't see it).
    bool onKeyEvent(int qtKey, bool pressed);

    // Gamepad input entry point. port is the libretro port (0..3 typically),
    // button is the raw libretro RetroPad button index.
    void onGamepadButton(int port, int button, bool pressed);

    // True if (port, button) is currently acting as the modifier of a
    // matched combo. The input router consults this to mask the modifier
    // button from the libretro core's view of the gamepad state.
    bool isSuppressed(int port, int button) const;

signals:
    // Emitted on the press-edge (false → true) for every bound action.
    void actionPressed(const QString& actionKey);

    // Emitted on the release-edge (true → false) ONLY for hold-style
    // actions (currently just "FastForwardHold").
    void actionReleased(const QString& actionKey);

private:
    struct KeyBinding { int qtKey; };
    // modifier == -1 for single-button bindings; >= 0 for combo bindings.
    struct GamepadBinding { int port; int modifier; int button; };

    struct ActionBindings {
        QList<KeyBinding>     keys;
        QList<GamepadBinding> pads;
    };

    QHash<QString, ActionBindings> m_bindings;       // action → all bindings
    QMultiHash<int, QString>       m_keyToActions;   // qtKey → actions
    QHash<QString, bool>           m_actionPressed;  // per-action "is held"
    QHash<int, QSet<int>>          m_padHeld;        // port → held buttons
    QSet<qint64>                   m_suppressed;     // padKey(port, modifier)
    mutable QMutex                 m_suppressedMutex;

    void firePressEdge(const QString& action, bool pressed);
    static bool isHoldAction(const QString& actionKey);
    static qint64 padKey(int port, int button) {
        return (qint64(port) << 32) | qint64(uint32_t(button));
    }
};
