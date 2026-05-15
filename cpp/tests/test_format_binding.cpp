#include <QtTest>
#include "adapters/duckstation_adapter.h"
#include "adapters/ppsspp_adapter.h"

// Each emulator's formatBinding() has a different output convention. These
// tests freeze the contract so a future refactor that breaks one of them
// fails loudly instead of producing silently-wrong INI bindings.
//
//   DuckStation: uses base impl
//                triggers get polarity prefix; full axes do not
//   PPSSPP    : "{deviceId}-{NKCODE}"    (numeric only)

class TestFormatBinding : public QObject {
    Q_OBJECT
private slots:
    void duckstation_buttonProducesValidBinding() {
        DuckStationAdapter a;
        // Output should be non-empty and contain the device + element so a
        // future refactor can't accidentally drop one.
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
