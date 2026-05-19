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

}  // namespace

bool HotkeyMatcher::isHoldAction(const QString& actionKey) {
    return actionKey == QStringLiteral("FastForwardHold");
}

void HotkeyMatcher::setBinding(const QString& actionKey, const QString& bindingString) {
    // Drop any previous binding for this action.
    auto it = m_keyBindings.find(actionKey);
    if (it != m_keyBindings.end()) {
        m_keyToAction.remove(it->qtKey);
        m_keyBindings.erase(it);
    }
    m_actionPressed.remove(actionKey);

    if (bindingString.isEmpty()) return;

    const int key = parseKeyboardSpec(bindingString);
    if (key == 0) return;
    m_keyBindings.insert(actionKey, KeyBinding{key});
    m_keyToAction.insert(key, actionKey);
}

void HotkeyMatcher::clearAllBindings() {
    m_keyBindings.clear();
    m_keyToAction.clear();
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
