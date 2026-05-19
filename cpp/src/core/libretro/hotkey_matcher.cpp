#include "core/libretro/hotkey_matcher.h"
#include <QKeySequence>

namespace {

// Parse "Keyboard/<KeySequenceText>" → combined Qt key+modifier int.
// Returns 0 on parse failure (unbound).
int parseKeyboardSpec(const QString& spec) {
    static const QString prefix = QStringLiteral("Keyboard/");
    if (!spec.startsWith(prefix)) return 0;
    const QString rest = spec.mid(prefix.size());
    const QKeySequence seq(rest, QKeySequence::PortableText);
    if (seq.count() == 0) return 0;
    return seq[0];  // includes modifier bits
}

// Parse "Gamepad<P>/<B>" or "Gamepad<P>/<M>+<B>" → (port, modifier, button).
// Sets *mod to -1 for the single-button form. Returns false on parse failure.
bool parseGamepadSpec(const QString& spec, int* port, int* mod, int* btn) {
    static const QString prefix = QStringLiteral("Gamepad");
    if (!spec.startsWith(prefix)) return false;
    const int slash = spec.indexOf(QLatin1Char('/'));
    if (slash <= int(prefix.size())) return false;  // need at least one digit before slash
    bool okP = false;
    *port = spec.mid(prefix.size(), slash - prefix.size()).toInt(&okP);
    if (!okP || *port < 0) return false;

    const QString rest = spec.mid(slash + 1);
    const int plus = rest.indexOf(QLatin1Char('+'));
    if (plus < 0) {
        *mod = -1;
        bool okB = false;
        *btn = rest.toInt(&okB);
        return okB && *btn >= 0;
    }
    bool okM = false, okB = false;
    *mod = rest.left(plus).toInt(&okM);
    *btn = rest.mid(plus + 1).toInt(&okB);
    return okM && okB && *mod >= 0 && *btn >= 0;
}

}  // namespace

bool HotkeyMatcher::isHoldAction(const QString& actionKey) {
    return actionKey == QStringLiteral("FastForwardHold");
}

void HotkeyMatcher::setBinding(const QString& actionKey, const QString& bindingString) {
    // Drop any previous keyboard binding for this action.
    if (auto it = m_keyBindings.find(actionKey); it != m_keyBindings.end()) {
        m_keyToAction.remove(it->qtKey);
        m_keyBindings.erase(it);
    }
    // Drop any previous gamepad binding for this action.
    if (auto it = m_padBindings.find(actionKey); it != m_padBindings.end()) {
        m_padBindings.erase(it);
    }
    m_actionPressed.remove(actionKey);

    if (bindingString.isEmpty()) return;

    int port, mod, btn;
    if (parseGamepadSpec(bindingString, &port, &mod, &btn)) {
        m_padBindings.insert(actionKey, GamepadBinding{port, mod, btn});
        return;
    }
    const int key = parseKeyboardSpec(bindingString);
    if (key == 0) return;
    m_keyBindings.insert(actionKey, KeyBinding{key});
    m_keyToAction.insert(key, actionKey);
}

void HotkeyMatcher::clearAllBindings() {
    m_keyBindings.clear();
    m_keyToAction.clear();
    m_padBindings.clear();
    m_actionPressed.clear();
    m_padHeld.clear();
    m_suppressed.clear();
}

void HotkeyMatcher::onKeyEvent(int qtKey, bool pressed) {
    auto it = m_keyToAction.constFind(qtKey);
    if (it == m_keyToAction.constEnd()) return;
    const QString action = it.value();
    const bool wasPressed = m_actionPressed.value(action, false);
    if (pressed && !wasPressed) {
        m_actionPressed[action] = true;
        emit actionPressed(action);
    } else if (!pressed && wasPressed) {
        m_actionPressed[action] = false;
        if (isHoldAction(action)) emit actionReleased(action);
    }
}

void HotkeyMatcher::onGamepadButton(int port, int button, bool pressed) {
    // Maintain per-port held-button set.
    QSet<int>& held = m_padHeld[port];
    if (pressed) held.insert(button); else held.remove(button);

    // Process bindings whose ACTION-button matches this event.
    // Linear scan over all bindings (max ~22 entries).
    for (auto it = m_padBindings.constBegin(); it != m_padBindings.constEnd(); ++it) {
        const GamepadBinding& gb = it.value();
        if (gb.port != port || gb.button != button) continue;

        // Single-button binding always matches; combo requires modifier held.
        const bool comboMatches =
            gb.modifier < 0          // single-button binding
            || held.contains(gb.modifier);  // combo with modifier currently held

        if (!comboMatches) continue;

        const QString& action = it.key();
        const bool wasPressed = m_actionPressed.value(action, false);
        if (pressed && !wasPressed) {
            m_actionPressed[action] = true;
            emit actionPressed(action);
            if (gb.modifier >= 0)
                m_suppressed.insert(padKey(port, gb.modifier));
        } else if (!pressed && wasPressed) {
            m_actionPressed[action] = false;
            if (isHoldAction(action)) emit actionReleased(action);
        }
    }

    // On modifier release: clear suppression for any combo that uses this
    // button as the modifier on this port, and reset the action's held state
    // (the chord is broken).
    if (!pressed) {
        for (auto it = m_padBindings.constBegin(); it != m_padBindings.constEnd(); ++it) {
            const GamepadBinding& gb = it.value();
            if (gb.port == port && gb.modifier == button) {
                m_suppressed.remove(padKey(port, button));
                m_actionPressed.remove(it.key());
            }
        }
    }
}

bool HotkeyMatcher::isSuppressed(int port, int button) const {
    return m_suppressed.contains(padKey(port, button));
}
