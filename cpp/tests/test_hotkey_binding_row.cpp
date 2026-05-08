// cpp/tests/test_hotkey_binding_row.cpp
//
// Smoke + signal tests for HotkeyBindingRow. The row is a label + a
// BindBtn; the test verifies signal emission for left-click (rebind),
// shift+left-click (append rebind), and right-click (clear), and that
// focusing the row emits `focused`.

#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QPushButton>
#include <QLabel>

#include "core/binding_def.h"
#include "ui/settings/widgets/hotkey_binding_row.h"

class TestHotkeyBindingRow : public QObject {
    Q_OBJECT

private:
    HotkeyDef makeDef() const {
        HotkeyDef d;
        d.label = "Toggle Turbo";
        d.group = "Speed Control";
        d.section = "Hotkeys";
        d.key = "ToggleTurbo";
        d.defaultValue = "Keyboard/Period";
        return d;
    }

private slots:
    void constructsWithLabelAndButton() {
        HotkeyBindingRow row(makeDef());
        QVERIFY(row.findChild<QLabel*>() != nullptr);
        QVERIFY(row.findChild<QPushButton*>() != nullptr);
    }

    void setBindingDisplay_updatesButtonText() {
        HotkeyBindingRow row(makeDef());
        row.setBindingDisplay("Period");
        QCOMPARE(row.findChild<QPushButton*>()->text(), QStringLiteral("Period"));

        row.setBindingDisplay(QString());
        QCOMPARE(row.findChild<QPushButton*>()->text(), QStringLiteral("Not bound"));
    }

    void leftClick_emitsRebindRequested() {
        HotkeyBindingRow row(makeDef());
        QSignalSpy spy(&row, &HotkeyBindingRow::rebindRequested);

        auto* btn = row.findChild<QPushButton*>();
        QTest::mouseClick(btn, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().first().value<HotkeyDef>().key,
                 QStringLiteral("ToggleTurbo"));
    }

    void shiftLeftClick_emitsAppendRebindRequested() {
        HotkeyBindingRow row(makeDef());
        QSignalSpy spy(&row, &HotkeyBindingRow::appendRebindRequested);

        auto* btn = row.findChild<QPushButton*>();
        QTest::mouseClick(btn, Qt::LeftButton, Qt::ShiftModifier);

        QCOMPARE(spy.count(), 1);
    }

    void rightClick_emitsClearRequested() {
        HotkeyBindingRow row(makeDef());
        QSignalSpy spy(&row, &HotkeyBindingRow::clearRequested);

        auto* btn = row.findChild<QPushButton*>();
        QTest::mouseClick(btn, Qt::RightButton);

        QCOMPARE(spy.count(), 1);
    }

    void focusIn_emitsFocused() {
        HotkeyBindingRow row(makeDef());
        QSignalSpy spy(&row, &HotkeyBindingRow::focused);

        // Widget must be shown for focusInEvent to fire.
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));
        row.setFocus(Qt::TabFocusReason);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().first().value<HotkeyDef>().key,
                 QStringLiteral("ToggleTurbo"));
    }
};

QTEST_MAIN(TestHotkeyBindingRow)
#include "test_hotkey_binding_row.moc"
