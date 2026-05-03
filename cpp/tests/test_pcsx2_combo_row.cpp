#include <QtTest>
#include "ui/settings/widgets/settings_combo_row.h"

class TestSettingsComboRow : public QObject {
    Q_OBJECT
private slots:
    void setOptionsAndValueRoundTrips() {
        SettingsComboRow row;
        row.setOptions({{"Auto", "-1"}, {"Off", "0"}, {"On", "1"}});
        row.setValue("0");
        QCOMPARE(row.value(), QString("0"));
        row.setValue("1");
        QCOMPARE(row.value(), QString("1"));
    }
    void valueChangedSignalFires() {
        SettingsComboRow row;
        row.setOptions({{"A", "a"}, {"B", "b"}});
        QSignalSpy spy(&row, &SettingsComboRow::valueChanged);
        row.setValue("b");
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().first().toString(), QString("b"));
    }
};
QTEST_MAIN(TestSettingsComboRow)
#include "test_pcsx2_combo_row.moc"
