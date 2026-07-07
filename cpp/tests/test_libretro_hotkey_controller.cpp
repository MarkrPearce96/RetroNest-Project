// cpp/tests/test_libretro_hotkey_controller.cpp
//
// First QtTest coverage of the app-global libretro hotkey engine
// (app-shell review P1): key routing through the matcher, the three
// suppression sources (QML flag, widget refcount, injected check), and
// binding replacement. The session provider returns nullptr throughout —
// every dispatched action must no-op safely without a running game.

#include <QtTest>
#include <QSignalSpy>
#include "core/libretro/libretro_hotkey_controller.h"
#include "core/libretro/libretro_hotkey_defs.h"

namespace ids = libretro_hotkeys::ids;

class TestLibretroHotkeyController : public QObject {
    Q_OBJECT

private slots:
    void boundKeyFiresActionAndConsumes() {
        LibretroHotkeyController ctl([]() -> GameSession* { return nullptr; });
        ctl.setBindings({ row(ids::ToggleMenu, "Keyboard/F1") });

        QSignalSpy spy(&ctl, &LibretroHotkeyController::menuToggleRequested);
        QVERIFY(ctl.handleKeyEvent(Qt::Key_F1, true));
        QCOMPARE(spy.count(), 1);
        ctl.handleKeyEvent(Qt::Key_F1, false);

        // Unbound key is not consumed.
        QVERIFY(!ctl.handleKeyEvent(Qt::Key_F2, true));
    }

    void uiSuppressionBlocks() {
        LibretroHotkeyController ctl([]() -> GameSession* { return nullptr; });
        ctl.setBindings({ row(ids::ToggleMenu, "Keyboard/F1") });
        QSignalSpy spy(&ctl, &LibretroHotkeyController::menuToggleRequested);

        ctl.setUiSuppressed(true);
        QVERIFY(!ctl.handleKeyEvent(Qt::Key_F1, true));
        QCOMPARE(spy.count(), 0);

        ctl.setUiSuppressed(false);
        QVERIFY(ctl.handleKeyEvent(Qt::Key_F1, true));
        QCOMPARE(spy.count(), 1);
        ctl.handleKeyEvent(Qt::Key_F1, false);
    }

    void suppressionRefcountBlocksUntilFullyReleased() {
        LibretroHotkeyController ctl([]() -> GameSession* { return nullptr; });
        ctl.setBindings({ row(ids::ToggleMenu, "Keyboard/F1") });
        QSignalSpy spy(&ctl, &LibretroHotkeyController::menuToggleRequested);

        ctl.acquireSuppression();
        ctl.acquireSuppression();
        ctl.releaseSuppression();
        QVERIFY(ctl.suppressed());                     // one hold remains
        QVERIFY(!ctl.handleKeyEvent(Qt::Key_F1, true));
        QCOMPARE(spy.count(), 0);

        ctl.releaseSuppression();
        QVERIFY(!ctl.suppressed());
        QVERIFY(ctl.handleKeyEvent(Qt::Key_F1, true));
        QCOMPARE(spy.count(), 1);
        ctl.handleKeyEvent(Qt::Key_F1, false);

        // Extra release must not underflow into a sticky "unsuppressed" debt.
        ctl.releaseSuppression();
        ctl.acquireSuppression();
        QVERIFY(ctl.suppressed());
    }

    void injectedSuppressionCheckConsulted() {
        bool vetoed = false;
        LibretroHotkeyController ctl([]() -> GameSession* { return nullptr; },
                                     [&vetoed]() { return vetoed; });
        ctl.setBindings({ row(ids::ToggleMenu, "Keyboard/F1") });
        QSignalSpy spy(&ctl, &LibretroHotkeyController::menuToggleRequested);

        vetoed = true;
        QVERIFY(!ctl.handleKeyEvent(Qt::Key_F1, true));
        vetoed = false;
        QVERIFY(ctl.handleKeyEvent(Qt::Key_F1, true));
        QCOMPARE(spy.count(), 1);
    }

    void actionsNoOpWithoutSession() {
        LibretroHotkeyController ctl([]() -> GameSession* { return nullptr; });
        ctl.setBindings({
            row(ids::SaveState, "Keyboard/F5"),
            row(ids::LoadState, "Keyboard/F6"),
            row(ids::NextSlot,  "Keyboard/F7"),
            row(ids::Mute,      "Keyboard/M"),
        });
        QSignalSpy toastSpy(&ctl, &LibretroHotkeyController::infoToastRequested);

        // Every action dispatches into a null session — must not crash,
        // and the slot-change toast must not fire without a session.
        for (int key : { int(Qt::Key_F5), int(Qt::Key_F6),
                         int(Qt::Key_F7), int(Qt::Key_M) }) {
            QVERIFY(ctl.handleKeyEvent(key, true));
            ctl.handleKeyEvent(key, false);
        }
        QCOMPARE(toastSpy.count(), 0);
    }

    void setBindingsReplacesEverything() {
        LibretroHotkeyController ctl([]() -> GameSession* { return nullptr; });
        ctl.setBindings({ row(ids::ToggleMenu, "Keyboard/F1") });
        ctl.setBindings({ row(ids::ToggleMenu, "Keyboard/F2") });

        QSignalSpy spy(&ctl, &LibretroHotkeyController::menuToggleRequested);
        QVERIFY(!ctl.handleKeyEvent(Qt::Key_F1, true));   // old binding gone
        QVERIFY(ctl.handleKeyEvent(Qt::Key_F2, true));
        QCOMPARE(spy.count(), 1);
    }

private:
    static QVariant row(const QString& actionKey, const QString& binding) {
        return QVariantMap{ { QStringLiteral("key"), actionKey },
                            { QStringLiteral("currentValue"), binding } };
    }
};

QTEST_GUILESS_MAIN(TestLibretroHotkeyController)
#include "test_libretro_hotkey_controller.moc"
