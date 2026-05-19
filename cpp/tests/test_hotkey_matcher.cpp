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

    void comboFiresAndSuppresses() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/4+8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);

        // Modifier down alone: no fire, no suppression.
        m.onGamepadButton(0, 4, true);
        QCOMPARE(spy.count(), 0);
        QVERIFY(!m.isSuppressed(0, 4));

        // Action button down while modifier held: fires, suppresses modifier.
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(m.isSuppressed(0, 4));

        // Action button release: suppression remains while modifier held.
        m.onGamepadButton(0, 8, false);
        QVERIFY(m.isSuppressed(0, 4));

        // Modifier release: suppression cleared.
        m.onGamepadButton(0, 4, false);
        QVERIFY(!m.isSuppressed(0, 4));
    }

    void modifierAloneDoesNotSuppress() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/4+8"));
        m.onGamepadButton(0, 4, true);
        QVERIFY(!m.isSuppressed(0, 4));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 4, false);
        QCOMPARE(spy.count(), 0);
    }

    void actionAloneDoesNotFireCombo() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/4+8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 8, true);   // no modifier held
        QCOMPARE(spy.count(), 0);
        QVERIFY(!m.isSuppressed(0, 4));
    }

    void singleButtonGamepadBindingDoesNotSuppress() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!m.isSuppressed(0, 8));
    }

    void comboPortIsolated() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/4+8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        // Modifier held on port 1; action on port 0 — must not match.
        m.onGamepadButton(1, 4, true);
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 0);
        QVERIFY(!m.isSuppressed(1, 4));
    }

    void comboMultiplePresses() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/4+8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 4, true);
        m.onGamepadButton(0, 8, true);     // press 1
        m.onGamepadButton(0, 8, false);
        m.onGamepadButton(0, 8, true);     // press 2
        m.onGamepadButton(0, 8, false);
        QCOMPARE(spy.count(), 2);
        QVERIFY(m.isSuppressed(0, 4));
    }

    void clearAllAlsoClearsSuppression() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/4+8"));
        m.onGamepadButton(0, 4, true);
        m.onGamepadButton(0, 8, true);
        QVERIFY(m.isSuppressed(0, 4));
        m.clearAllBindings();
        QVERIFY(!m.isSuppressed(0, 4));
    }

    void staticActiveSlotReflectsInstance() {
        HotkeyMatcher m;
        // Don't actually register globally — just sanity-check the field exists
        // and accepts assignment. (Real wiring is in AppController, not testable
        // here.)
        HotkeyMatcher::s_active.store(&m);
        QCOMPARE(HotkeyMatcher::s_active.load(), &m);
        HotkeyMatcher::s_active.store(nullptr);
        QVERIFY(HotkeyMatcher::s_active.load() == nullptr);
    }
};

QTEST_APPLESS_MAIN(TestHotkeyMatcher)
#include "test_hotkey_matcher.moc"
