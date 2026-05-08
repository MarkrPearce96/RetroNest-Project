// cpp/tests/test_generic_hotkey_page.cpp
//
// Smoke tests for GenericHotkeyPage. Uses nullptr for AppController —
// the page guards the nullptr and renders the empty-state branch, so
// these tests verify chrome/structure rather than data binding.

#include <QtTest>
#include <QApplication>
#include <QSignalSpy>

#include "ui/settings/generic_hotkey_page.h"
#include "ui/settings/widgets/hotkey_binding_row.h"

class TestGenericHotkeyPage : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        qputenv("XDG_CONFIG_HOME", QByteArrayLiteral("/tmp/retronest-test-config"));
    }

    void nullAppControllerProducesEmptyPage() {
        GenericHotkeyPage page(/*inputManager=*/nullptr,
                                /*appController=*/nullptr,
                                /*emuId=*/"pcsx2");
        page.resize(900, 720);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        QVERIFY(page.isEmpty());
        QCOMPARE(page.findChildren<HotkeyBindingRow*>().size(), 0);
    }

    void publicActionsNoopWithoutFocus() {
        GenericHotkeyPage page(nullptr, nullptr, "pcsx2");
        page.rebindFocused();
        page.appendRebindFocused();
        page.clearFocused();
        page.restoreDefaults();
        QVERIFY(true);
    }
};

QTEST_MAIN(TestGenericHotkeyPage)
#include "test_generic_hotkey_page.moc"
