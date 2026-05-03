#include <QtTest>
#include "ui/settings/widgets/settings_toggle.h"

class TestSettingsToggle : public QObject {
    Q_OBJECT
private slots:
    void defaultStateIsUnchecked() {
        SettingsToggle t;
        QVERIFY(!t.isChecked());
        QVERIFY(t.isCheckable());
    }
    void setCheckedEmitsToggled() {
        SettingsToggle t;
        QSignalSpy spy(&t, &QAbstractButton::toggled);
        t.setChecked(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }
    void sizeHintMatchesSpec() {
        SettingsToggle t;
        QCOMPARE(t.sizeHint(), QSize(34, 18));
    }
};
QTEST_MAIN(TestSettingsToggle)
#include "test_pcsx2_toggle.moc"
