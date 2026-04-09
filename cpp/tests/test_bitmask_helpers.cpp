#include <QtTest>
#include "core/bitmask_helpers.h"

class TestBitmaskHelpers : public QObject {
    Q_OBJECT
private slots:
    void testGetBitSet() {
        // FPS_COUNTER bit (1<<1 = 2) is set in 6 (0b110)
        QCOMPARE(BitmaskHelpers::getBit(6, 2), true);
    }
    void testGetBitClear() {
        // FPS_COUNTER bit (2) is NOT set in 4 (0b100)
        QCOMPARE(BitmaskHelpers::getBit(4, 2), false);
    }
    void testSetBitOn() {
        // Setting FPS bit (2) in 4 (0b100) → 6 (0b110)
        QCOMPARE(BitmaskHelpers::setBit(4, 2, true), 6);
    }
    void testSetBitOff() {
        // Clearing FPS bit (2) in 6 (0b110) → 4 (0b100)
        QCOMPARE(BitmaskHelpers::setBit(6, 2, false), 4);
    }
    void testSetBitIdempotent() {
        // Setting an already-set bit leaves the value unchanged
        QCOMPARE(BitmaskHelpers::setBit(6, 2, true), 6);
        // Clearing an already-clear bit leaves the value unchanged
        QCOMPARE(BitmaskHelpers::setBit(4, 2, false), 4);
    }
    void testSetBitPreservesOtherBits() {
        // Setting bit 2 (0b0010) in 0b1001 must preserve bits 0 and 3 → 0b1011
        QCOMPARE(BitmaskHelpers::setBit(0b1001, 0b0010, true),  0b1011);
        // Clearing the same bit must give back 0b1001
        QCOMPARE(BitmaskHelpers::setBit(0b1011, 0b0010, false), 0b1001);
    }
    void testZeroBitmaskIsNoOp() {
        // bitmask=0 means "not a bitmask widget"; getBit returns false
        QCOMPARE(BitmaskHelpers::getBit(123, 0), false);
        // setBit with bitmask=0 returns the value unchanged
        QCOMPARE(BitmaskHelpers::setBit(123, 0, true),  123);
        QCOMPARE(BitmaskHelpers::setBit(123, 0, false), 123);
    }
};

QTEST_GUILESS_MAIN(TestBitmaskHelpers)
#include "test_bitmask_helpers.moc"
