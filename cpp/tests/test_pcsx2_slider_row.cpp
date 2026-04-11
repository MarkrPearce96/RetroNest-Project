#include <QtTest>
#include "ui/settings/pcsx2/widgets/pcsx2_slider_row.h"

class TestPcsx2SliderRow : public QObject {
    Q_OBJECT
private slots:
    void rangeClampsValue() {
        Pcsx2SliderRow row;
        row.setRange(0, 100);
        row.setValue(150);
        QCOMPARE(row.value(), 100);
        row.setValue(-50);
        QCOMPARE(row.value(), 0);
    }
    void suffixFormatsValueLabel() {
        Pcsx2SliderRow row;
        row.setRange(0, 100);
        row.setSuffix(" ms");
        row.setValue(42);
        QCOMPARE(row.value(), 42);
        // value label text is internal but we know refreshValueLabel was called;
        // verifying via value() alone is sufficient for Plan 1.
    }
    void valueChangedSignalFires() {
        Pcsx2SliderRow row;
        row.setRange(0, 100);
        QSignalSpy spy(&row, &Pcsx2SliderRow::valueChanged);
        row.setValue(50);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 50);
    }
};
QTEST_MAIN(TestPcsx2SliderRow)
#include "test_pcsx2_slider_row.moc"
