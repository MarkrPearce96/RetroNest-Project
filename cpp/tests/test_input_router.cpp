#include <QtTest>
#include "core/libretro/input_router.h"

class TestInputRouter : public QObject {
    Q_OBJECT
private slots:
    void testInitialStateIsZero() {
        InputRouter r;
        QVERIFY(!r.buttonPressed(0, RetroPadSlot::A));
        QVERIFY(!r.buttonPressed(0, RetroPadSlot::Up));
    }
    void testSetAndReadButton() {
        InputRouter r;
        r.setButtonPressed(0, RetroPadSlot::A, true);
        QVERIFY(r.buttonPressed(0, RetroPadSlot::A));
        QVERIFY(!r.buttonPressed(0, RetroPadSlot::B));
        r.setButtonPressed(0, RetroPadSlot::A, false);
        QVERIFY(!r.buttonPressed(0, RetroPadSlot::A));
    }
    void testBindingLookup() {
        InputRouter r;
        r.bind(0, "A", RetroPadSlot::A);
        r.bind(0, "DPadUp", RetroPadSlot::Up);
        QCOMPARE(r.lookup(0, "A"), RetroPadSlot::A);
        QCOMPARE(r.lookup(0, "DPadUp"), RetroPadSlot::Up);
        QCOMPARE(r.lookup(0, "Unknown"), RetroPadSlot::None);
    }
    void testPortsAreIndependent() {
        InputRouter r;
        r.setButtonPressed(0, RetroPadSlot::A, true);
        QVERIFY(r.buttonPressed(0, RetroPadSlot::A));
        QVERIFY(!r.buttonPressed(1, RetroPadSlot::A));
    }
};
QTEST_MAIN(TestInputRouter)
#include "test_input_router.moc"
