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

// Parse "Gamepad<P>/<B>" → (port, button). Returns false on failure.
bool parseGamepadSpec(const QString& spec, int* port, int* btn) {
    static const QString prefix = QStringLiteral("Gamepad");
    if (!spec.startsWith(prefix)) return false;
    const int slash = spec.indexOf(QLatin1Char('/'));
    if (slash <= int(prefix.size())) return false;  // need at least one digit before slash
    bool okP = false, okB = false;
    *port = spec.mid(prefix.size(), slash - prefix.size()).toInt(&okP);
    *btn  = spec.mid(slash + 1).toInt(&okB);
    return okP && okB && *port >= 0 && *btn >= 0;
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
        m_padToAction.remove(padKey(it->port, it->button));
        m_padBindings.erase(it);
    }
    m_actionPressed.remove(actionKey);

    if (bindingString.isEmpty()) return;

    int port, btn;
    if (parseGamepadSpec(bindingString, &port, &btn)) {
        m_padBindings.insert(actionKey, GamepadBinding{port, btn});
        m_padToAction.insert(padKey(port, btn), actionKey);
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
    m_padToAction.clear();
    m_actionPressed.clear();
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
    auto it = m_padToAction.constFind(padKey(port, button));
    if (it == m_padToAction.constEnd()) return;
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
