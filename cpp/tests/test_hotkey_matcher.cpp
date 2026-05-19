#include <QtTest>
#include <QSignalSpy>
#include <Qt>
#include "core/libretro/hotkey_matcher.h"

class TestHotkeyMatcher : public QObject {
    Q_OBJECT
private slots:
    void firesOnKeyPressEdge() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Keyboard/F1"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);

        m.onKeyEvent(Qt::Key_F1, /*pressed=*/true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("Foo"));

        // Held: no refire while still pressed
        m.onKeyEvent(Qt::Key_F1, /*pressed=*/true);
        QCOMPARE(spy.count(), 0);

        // Release: non-hold action gets no extra actionPressed
        m.onKeyEvent(Qt::Key_F1, /*pressed=*/false);
        QCOMPARE(spy.count(), 0);

        // Press again: fires
        m.onKeyEvent(Qt::Key_F1, /*pressed=*/true);
        QCOMPARE(spy.count(), 1);
    }

    void unboundKeyDoesNothing() {
        HotkeyMatcher m;
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onKeyEvent(Qt::Key_F1, true);
        QCOMPARE(spy.count(), 0);
    }

    void emptyBindingClearsAction() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Keyboard/F1"));
        m.setBinding(QStringLiteral("Foo"), QString());
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onKeyEvent(Qt::Key_F1, true);
        QCOMPARE(spy.count(), 0);
    }

    void holdActionEmitsReleased() {
        HotkeyMatcher m;
        // FastForwardHold is the (currently only) hold-style action.
        m.setBinding(QStringLiteral("FastForwardHold"), QStringLiteral("Keyboard/F1"));
        QSignalSpy pressedSpy(&m, &HotkeyMatcher::actionPressed);
        QSignalSpy releasedSpy(&m, &HotkeyMatcher::actionReleased);

        m.onKeyEvent(Qt::Key_F1, true);
        QCOMPARE(pressedSpy.count(), 1);
        QCOMPARE(releasedSpy.count(), 0);

        m.onKeyEvent(Qt::Key_F1, false);
        QCOMPARE(pressedSpy.count(), 1);
        QCOMPARE(releasedSpy.count(), 1);
    }

    void modifiedKeyBindingMatches() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Keyboard/Shift+F2"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        // The host event filter sends `key() | int(modifiers())`. Shift+F2 keycode:
        const int combined = int(Qt::ShiftModifier) | int(Qt::Key_F2);
        m.onKeyEvent(combined, true);
        QCOMPARE(spy.count(), 1);
    }

    void clearAllBindingsRemovesAll() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Keyboard/F1"));
        m.setBinding(QStringLiteral("Bar"), QStringLiteral("Keyboard/F2"));
        m.clearAllBindings();
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onKeyEvent(Qt::Key_F1, true);
        m.onKeyEvent(Qt::Key_F2, true);
        QCOMPARE(spy.count(), 0);
    }

    void firesOnGamepadButtonPressEdge() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);

        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("Foo"));

        m.onGamepadButton(0, 8, true);    // held, no refire
        QCOMPARE(spy.count(), 0);

        m.onGamepadButton(0, 8, false);   // release; non-hold, no emit
        QCOMPARE(spy.count(), 0);

        m.onGamepadButton(0, 8, true);    // re-press, fires
        QCOMPARE(spy.count(), 1);
    }

    void gamepadPortIsolated() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad1/8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 8, true);    // wrong port
        QCOMPARE(spy.count(), 0);
        m.onGamepadButton(1, 8, true);    // right port
        QCOMPARE(spy.count(), 1);
    }

    void holdActionEmitsReleasedFromGamepad() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("FastForwardHold"), QStringLiteral("Gamepad0/3"));
        QSignalSpy pressedSpy(&m, &HotkeyMatcher::actionPressed);
        QSignalSpy releasedSpy(&m, &HotkeyMatcher::actionReleased);
        m.onGamepadButton(0, 3, true);
        QCOMPARE(pressedSpy.count(), 1);
        QCOMPARE(releasedSpy.count(), 0);
        m.onGamepadButton(0, 3, false);
        QCOMPARE(pressedSpy.count(), 1);
        QCOMPARE(releasedSpy.count(), 1);
    }

    void rebindKeyboardToGamepadDropsOldKey() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Keyboard/F1"));
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        // The old keyboard binding should no longer fire.
        m.onKeyEvent(Qt::Key_F1, true);
        QCOMPARE(spy.count(), 0);
        // The new gamepad binding should fire.
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 1);
    }

    void rebindGamepadToKeyboardDropsOldButton() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/8"));
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Keyboard/F1"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 0);
        m.onKeyEvent(Qt::Key_F1, true);
        QCOMPARE(spy.count(), 1);
    }

    void clearAllRemovesGamepadAlso() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/8"));
        m.clearAllBindings();
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_APPLESS_MAIN(TestHotkeyMatcher)
#include "test_hotkey_matcher.moc"
