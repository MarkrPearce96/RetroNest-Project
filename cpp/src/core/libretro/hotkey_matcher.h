#pragma once
#include <QObject>
#include <QHash>
#include <QString>
#include <Qt>

/**
 * Host-side hotkey matcher.
 *
 * Holds per-action keyboard bindings, detects press-edges on incoming
 * Qt key events, and emits actionPressed / actionReleased signals.
 *
 * Gamepad support is added in subsequent tasks. This iteration is
 * keyboard-only.
 *
 * Threading: setBinding/onKeyEvent must be called from the same thread
 * (typically the Qt main thread). No internal locks.
 */
class HotkeyMatcher : public QObject {
    Q_OBJECT
public:
    explicit HotkeyMatcher(QObject* parent = nullptr) : QObject(parent) {}

    // Replace the binding for one action. Empty bindingString clears it.
    // Currently parses only "Keyboard/<KeySequenceText>" (e.g. "Keyboard/F1",
    // "Keyboard/Shift+F2"). Unparseable strings are silently dropped.
    void setBinding(const QString& actionKey, const QString& bindingString);

    // Drop every binding and any held state.
    void clearAllBindings();

    // Qt keyboard event entry point. qtKey is the combined value
    // QKeyEvent::key() | int(QKeyEvent::modifiers()).
    void onKeyEvent(int qtKey, bool pressed);

signals:
    // Emitted on the press-edge (false→true) for every bound action.
    void actionPressed(const QString& actionKey);

    // Emitted on the release-edge (true→false) ONLY for hold-style
    // actions (currently just "FastForwardHold").
    void actionReleased(const QString& actionKey);

private:
    struct KeyBinding { int qtKey; };
    QHash<QString, KeyBinding> m_keyBindings;     // action → key
    QHash<int, QString>        m_keyToAction;     // key → action (reverse lookup)
    QHash<QString, bool>       m_actionPressed;   // current "is held" state per action

    static bool isHoldAction(const QString& actionKey);
};
