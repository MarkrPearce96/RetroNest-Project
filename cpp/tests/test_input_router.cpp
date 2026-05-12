#include <QtTest>
#include "core/libretro/input_router.h"

class TestInputRouter : public QObject {
    Q_OBJECT
private slots:
    // --- existing digital tests (unchanged) ---
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

    // --- new analog tests (SP5.5) ---

    void testInitialAxesAreZero() {
        InputRouter r;
        QCOMPARE(r.axis(0, RetroPadAxis::LeftX), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::LeftY), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::RightX), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::RightY), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::L2), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::R2), int16_t(0));
    }

    void testTriggerPerAxisDeadzoneAndPassthrough() {
        InputRouter r;
        // Default deadzone 0.15 → threshold ≈ 4915 of 32767.
        r.setAxis(0, RetroPadAxis::L2, 3000);   // below threshold
        QCOMPARE(r.axis(0, RetroPadAxis::L2), int16_t(0));
        r.setAxis(0, RetroPadAxis::L2, 16000);  // well above
        const int16_t v = r.axis(0, RetroPadAxis::L2);
        QVERIFY(v > 0);
        QVERIFY(v < 16000);   // rescaled by deadzone
    }

    void testStickRadialDeadzoneZeroesBothBelow() {
        InputRouter r;
        // X=3000, Y=3000 → magnitude ≈ 4243 < 4915 deadzone → both should be zero.
        r.setAxis(0, RetroPadAxis::LeftX, 3000);
        r.setAxis(0, RetroPadAxis::LeftY, 3000);
        QCOMPARE(r.axis(0, RetroPadAxis::LeftX), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::LeftY), int16_t(0));
    }

    void testStickRadialDeadzoneReleasesBothAbove() {
        InputRouter r;
        // X=4000, Y=4000 → magnitude ≈ 5657 > 4915 → both should be non-zero.
        r.setAxis(0, RetroPadAxis::LeftX, 4000);
        r.setAxis(0, RetroPadAxis::LeftY, 4000);
        QVERIFY(r.axis(0, RetroPadAxis::LeftX) > 0);
        QVERIFY(r.axis(0, RetroPadAxis::LeftY) > 0);
    }

    void testStickRadialPreservesSign() {
        InputRouter r;
        r.setAxis(0, RetroPadAxis::RightX, -20000);
        r.setAxis(0, RetroPadAxis::RightY,  20000);
        QVERIFY(r.axis(0, RetroPadAxis::RightX) < 0);
        QVERIFY(r.axis(0, RetroPadAxis::RightY) > 0);
    }

    void testDeadzoneZeroIsPassthrough() {
        InputRouter r;
        r.setInnerDeadzone(0.0f);
        r.setAxis(0, RetroPadAxis::L2, 1);
        QCOMPARE(r.axis(0, RetroPadAxis::L2), int16_t(1));
        r.setAxis(0, RetroPadAxis::LeftX, 12345);
        r.setAxis(0, RetroPadAxis::LeftY, 0);
        QCOMPARE(r.axis(0, RetroPadAxis::LeftX), int16_t(12345));
    }

    void testDeadzoneClampedToHalf() {
        InputRouter r;
        r.setInnerDeadzone(0.6f);   // should clamp to 0.5
        // At dz=0.5, threshold = 16383. A value of 16000 should still register 0.
        r.setAxis(0, RetroPadAxis::L2, 16000);
        QCOMPARE(r.axis(0, RetroPadAxis::L2), int16_t(0));
        // A value of 20000 (above 16383) should register non-zero.
        r.setAxis(0, RetroPadAxis::L2, 20000);
        QVERIFY(r.axis(0, RetroPadAxis::L2) > 0);
    }

    void testAxisPortIsolation() {
        InputRouter r;
        r.setAxis(0, RetroPadAxis::LeftX, 30000);
        QCOMPARE(r.axis(1, RetroPadAxis::LeftX), int16_t(0));
        QCOMPARE(r.axis(2, RetroPadAxis::LeftX), int16_t(0));
        QCOMPARE(r.axis(3, RetroPadAxis::LeftX), int16_t(0));
    }
};
QTEST_MAIN(TestInputRouter)
#include "test_input_router.moc"
