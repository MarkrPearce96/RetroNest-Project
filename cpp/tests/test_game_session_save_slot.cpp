#include <QtTest>
#include <QSignalSpy>
#include "core/game_session.h"

class TestGameSessionSaveSlot : public QObject {
    Q_OBJECT
private slots:
    void defaultSlotIsOne() {
        GameSession s;
        QCOMPARE(s.currentSaveSlot(), 1);
    }
    void setterClampsToOneFive() {
        GameSession s;
        s.setCurrentSaveSlot(3);  QCOMPARE(s.currentSaveSlot(), 3);
        s.setCurrentSaveSlot(0);  QCOMPARE(s.currentSaveSlot(), 1);
        s.setCurrentSaveSlot(99); QCOMPARE(s.currentSaveSlot(), 5);
        s.setCurrentSaveSlot(-5); QCOMPARE(s.currentSaveSlot(), 1);
    }
    void emitsChangeSignal() {
        GameSession s;
        QSignalSpy spy(&s, &GameSession::currentSaveSlotChanged);
        s.setCurrentSaveSlot(3);
        QCOMPARE(spy.count(), 1);
        s.setCurrentSaveSlot(3);  // same value, no emit
        QCOMPARE(spy.count(), 1);
    }
    void clampDoesNotEmitWhenStillAtBoundary() {
        GameSession s;
        QSignalSpy spy(&s, &GameSession::currentSaveSlotChanged);
        // Default is 1; setter clamps -5 → 1, but value unchanged → no emit.
        s.setCurrentSaveSlot(-5);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_APPLESS_MAIN(TestGameSessionSaveSlot)
#include "test_game_session_save_slot.moc"
