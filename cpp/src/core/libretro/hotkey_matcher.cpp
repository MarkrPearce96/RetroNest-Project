#include "core/libretro/hotkey_matcher.h"
#include <QKeySequence>
#include <QMutexLocker>
#include <QStringList>

std::atomic<HotkeyMatcher*> HotkeyMatcher::s_active{nullptr};

namespace {

// Parse "Keyboard/<KeySequenceText>" → combined Qt key+modifier int.
// Returns 0 on parse failure (i.e. token isn't a keyboard binding).
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
    if (slash <= int(prefix.size())) return false;
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
    // Drop the action's existing bindings from the forward and reverse maps.
    if (auto it = m_bindings.find(actionKey); it != m_bindings.end()) {
        for (const KeyBinding& kb : it->keys)
            m_keyToActions.remove(kb.qtKey, actionKey);
        m_bindings.erase(it);
    }
    m_actionPressed.remove(actionKey);

    if (bindingString.isEmpty()) return;

    // Split on " & " so a single row can bind both keyboard and gamepad.
    const QStringList tokens = bindingString.split(QStringLiteral(" & "),
                                                   Qt::SkipEmptyParts);
    ActionBindings ab;
    for (const QString& tokenRaw : tokens) {
        const QString token = tokenRaw.trimmed();
        if (token.isEmpty()) continue;

        int port, mod, btn;
        if (parseGamepadSpec(token, &port, &mod, &btn)) {
            ab.pads.append(GamepadBinding{port, mod, btn});
            continue;
        }
        const int key = parseKeyboardSpec(token);
        if (key != 0) {
            ab.keys.append(KeyBinding{key});
            m_keyToActions.insert(key, actionKey);
        }
    }

    if (!ab.keys.isEmpty() || !ab.pads.isEmpty())
        m_bindings.insert(actionKey, ab);
}

void HotkeyMatcher::clearAllBindings() {
    m_bindings.clear();
    m_keyToActions.clear();
    m_actionPressed.clear();
    m_padHeld.clear();
    {
        QMutexLocker locker(&m_suppressedMutex);
        m_suppressed.clear();
    }
}

void HotkeyMatcher::resetGamepadState() {
    m_padHeld.clear();
    m_actionPressed.clear();
    QMutexLocker locker(&m_suppressedMutex);
    m_suppressed.clear();
}

void HotkeyMatcher::firePressEdge(const QString& action, bool pressed) {
    const bool wasPressed = m_actionPressed.value(action, false);
    if (pressed && !wasPressed) {
        m_actionPressed[action] = true;
        emit actionPressed(action);
    } else if (!pressed && wasPressed) {
        m_actionPressed[action] = false;
        if (isHoldAction(action)) emit actionReleased(action);
    }
}

void HotkeyMatcher::onKeyEvent(int qtKey, bool pressed) {
    // QMultiHash returns ALL actions bound to this key (rare today but
    // cheap, and lets two actions share a binding if a user really wants).
    const auto actions = m_keyToActions.values(qtKey);
    for (const QString& action : actions) firePressEdge(action, pressed);
}

void HotkeyMatcher::onGamepadButton(int port, int button, bool pressed) {
    // Update the per-port held-button set first so combo detection has
    // the up-to-date state.
    QSet<int>& held = m_padHeld[port];
    if (pressed) held.insert(button); else held.remove(button);

    // For each action's pad bindings, check if this event matches the
    // ACTION-button half. Combos fire only when their modifier is held.
    for (auto it = m_bindings.constBegin(); it != m_bindings.constEnd(); ++it) {
        const QString& action = it.key();
        const ActionBindings& ab = it.value();
        for (const GamepadBinding& gb : ab.pads) {
            if (gb.port != port || gb.button != button) continue;
            const bool comboMatches = gb.modifier < 0
                                       || held.contains(gb.modifier);
            if (!comboMatches) continue;

            const bool wasPressed = m_actionPressed.value(action, false);
            if (pressed && !wasPressed) {
                m_actionPressed[action] = true;
                emit actionPressed(action);
                if (gb.modifier >= 0) {
                    QMutexLocker locker(&m_suppressedMutex);
                    m_suppressed.insert(padKey(port, gb.modifier));
                }
            } else if (!pressed && wasPressed) {
                m_actionPressed[action] = false;
                if (isHoldAction(action)) emit actionReleased(action);
            }
        }
    }

    // If THIS event released a button that's the modifier of any combo,
    // clear its suppression entry and drop any leftover held state for
    // the action (chord broken).
    if (!pressed) {
        for (auto it = m_bindings.constBegin(); it != m_bindings.constEnd(); ++it) {
            const QString& action = it.key();
            const ActionBindings& ab = it.value();
            for (const GamepadBinding& gb : ab.pads) {
                if (gb.port == port && gb.modifier == button) {
                    {
                        QMutexLocker locker(&m_suppressedMutex);
                        m_suppressed.remove(padKey(port, button));
                    }
                    m_actionPressed.remove(action);
                }
            }
        }
    }
}

bool HotkeyMatcher::isSuppressed(int port, int button) const {
    QMutexLocker locker(&m_suppressedMutex);
    return m_suppressed.contains(padKey(port, button));
}
