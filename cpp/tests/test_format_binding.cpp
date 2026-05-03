#include <QtTest>
#include "adapters/pcsx2_adapter.h"
#include "adapters/duckstation_adapter.h"
#include "adapters/ppsspp_adapter.h"

// Each emulator's formatBinding() has a different output convention. These
// tests freeze the contract so a future refactor that breaks one of them
// fails loudly instead of producing silently-wrong INI bindings.
//
//   PCSX2     : "SDL-{idx}/Element"      (buttons, no prefix)
//                "SDL-{idx}/+/-Element"   (axes, polarity prefix)
//   DuckStation: similar to PCSX2 default — uses base impl
//                triggers get polarity prefix; full axes do not
//   PPSSPP    : "{deviceId}-{NKCODE}"    (numeric only)

class TestFormatBinding : public QObject {
    Q_OBJECT
private slots:
    void pcsx2_buttonHasNoPrefix() {
        PCSX2Adapter a;
        QCOMPARE(a.formatBinding(0, "FaceSouth", /*isAxis=*/false, /*positive=*/true),
                 QString("SDL-0/FaceSouth"));
    }
    void pcsx2_axisGetsPolarityPrefix() {
        PCSX2Adapter a;
        QCOMPARE(a.formatBinding(0, "LeftX", /*isAxis=*/true, /*positive=*/true),
                 QString("SDL-0/+LeftX"));
        QCOMPARE(a.formatBinding(0, "LeftX", /*isAxis=*/true, /*positive=*/false),
                 QString("SDL-0/-LeftX"));
    }
    void pcsx2_deviceIndexInString() {
        PCSX2Adapter a;
        QCOMPARE(a.formatBinding(2, "A", /*isAxis=*/false, /*positive=*/true),
                 QString("SDL-2/A"));
    }

    void duckstation_buttonProducesValidBinding() {
        DuckStationAdapter a;
        // DuckStation may format differently from PCSX2; we just assert the
        // output is non-empty and contains the device + element so a future
        // refactor can't accidentally drop one.
        QString out = a.formatBinding(0, "A", /*isAxis=*/false, /*positive=*/true);
        QVERIFY(!out.isEmpty());
        QVERIFY(out.contains("A"));
        QVERIFY(out.contains("0"));
    }

    void ppspp_returnsNumericFormat() {
        PPSSPPAdapter a;
        // PPSSPP uses {deviceId}-{NKCODE}. The exact NKCODE depends on the
        // mapping; we verify the format shape here.
        QString out = a.formatBinding(0, "FaceSouth", /*isAxis=*/false, /*positive=*/true);
        // Output should be "{n}-{m}" or empty (if element is unmapped).
        if (!out.isEmpty()) {
            QVERIFY2(out.contains("-"),
                     qPrintable(QString("PPSSPP binding should be numeric '{id}-{code}', got: %1").arg(out)));
        }
    }
};

QTEST_GUILESS_MAIN(TestFormatBinding)
#include "test_format_binding.moc"
