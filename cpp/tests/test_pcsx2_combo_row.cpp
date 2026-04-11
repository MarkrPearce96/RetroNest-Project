#include <QtTest>
#include "ui/settings/pcsx2/widgets/pcsx2_combo_row.h"

class TestPcsx2ComboRow : public QObject {
    Q_OBJECT
private slots:
    void setOptionsAndValueRoundTrips() {
        Pcsx2ComboRow row;
        row.setOptions({{"Auto", "-1"}, {"Off", "0"}, {"On", "1"}});
        row.setValue("0");
        QCOMPARE(row.value(), QString("0"));
        row.setValue("1");
        QCOMPARE(row.value(), QString("1"));
    }
    void valueChangedSignalFires() {
        Pcsx2ComboRow row;
        row.setOptions({{"A", "a"}, {"B", "b"}});
        QSignalSpy spy(&row, &Pcsx2ComboRow::valueChanged);
        row.setValue("b");
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().first().toString(), QString("b"));
    }
};
QTEST_MAIN(TestPcsx2ComboRow)
#include "test_pcsx2_combo_row.moc"
