#include <QtTest>
#include "ui/settings/pcsx2/widgets/pcsx2_toggle.h"

class TestPcsx2Toggle : public QObject {
    Q_OBJECT
private slots:
    void defaultStateIsUnchecked() {
        Pcsx2Toggle t;
        QVERIFY(!t.isChecked());
        QVERIFY(t.isCheckable());
    }
    void setCheckedEmitsToggled() {
        Pcsx2Toggle t;
        QSignalSpy spy(&t, &QAbstractButton::toggled);
        t.setChecked(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }
    void sizeHintMatchesSpec() {
        Pcsx2Toggle t;
        QCOMPARE(t.sizeHint(), QSize(34, 18));
    }
};
QTEST_MAIN(TestPcsx2Toggle)
#include "test_pcsx2_toggle.moc"
