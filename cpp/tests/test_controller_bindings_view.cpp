// cpp/tests/test_controller_bindings_view.cpp
//
// Smoke tests for ControllerBindingsView. Construction depends on
// AdapterRegistry (no AppController required — we pass nullptr and the
// view skips the config-service reload path gracefully), so the tests
// are widget-level only: build the view, verify cards render, and
// check that the bindingFocused signal fires on focus.

#include <QtTest>
#include <QApplication>
#include <QSignalSpy>

#include "adapters/adapter_registry.h"
#include "ui/settings/controller_bindings_view.h"
#include "ui/settings/widgets/settings_card.h"

class TestControllerBindingsView : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Keep any stray config reads out of the home directory.
        qputenv("XDG_CONFIG_HOME", QByteArrayLiteral("/tmp/retronest-test-config"));
        // Register all built-in adapters so AdapterRegistry::adapterFor("pcsx2")
        // returns a valid adapter.
        AdapterRegistry::instance().registerBuiltinAdapters();
    }

    // ----------------------------------------------------------------
    // Construction: PCSX2 adapter has one controller type (DualShock 2)
    // with 28 bindings. All 28 have non-empty `key` fields, so all 28
    // BindingCards should be rendered.
    // ----------------------------------------------------------------
    void constructsForPcsx2() {
        // Pass nullptr for SdlInputManager and AppController — the view
        // doesn't dereference either at construction time; AppController
        // is only used in reloadBindings() which guards the nullptr case.
        ControllerBindingsView view(/*inputManager=*/nullptr,
                                    /*appController=*/nullptr,
                                    "pcsx2",
                                    /*controllerTypeId=*/"",
                                    /*port=*/1);
        view.resize(1280, 720);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        // All 28 PCSX2 DS2 bindings have non-empty INI keys.
        const auto cards = view.findChildren<SettingsCard*>();
        QVERIFY2(cards.size() >= 28,
            qPrintable(QString("expected ≥28 cards, got %1").arg(cards.size())));
    }

    // ----------------------------------------------------------------
    // reloadBindings() with nullptr AppController must not crash.
    // ----------------------------------------------------------------
    void reloadBindingsDoesNotCrash() {
        ControllerBindingsView view(nullptr, nullptr, "pcsx2", "", 1);
        view.reloadBindings();
        view.reloadBindings();
        view.reloadBindings();
        // If we get here without crashing, the test passes.
        QVERIFY(true);
    }

    // ----------------------------------------------------------------
    // Focusing a card should emit bindingFocused.
    // Trigger via QFocusEvent directly to avoid platform-specific
    // focus-delivery quirks in headless test environments.
    // ----------------------------------------------------------------
    void bindingFocusedSignalFires() {
        ControllerBindingsView view(nullptr, nullptr, "pcsx2", "", 1);
        view.resize(1280, 720);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        const auto cards = view.findChildren<SettingsCard*>();
        QVERIFY(!cards.isEmpty());

        QSignalSpy spy(&view, &ControllerBindingsView::bindingFocused);

        // Deliver a focus-in event directly to the first card so the
        // focusInEvent handler fires regardless of window-system focus state.
        QFocusEvent focusIn(QEvent::FocusIn, Qt::TabFocusReason);
        QApplication::sendEvent(cards.first(), &focusIn);
        QApplication::processEvents();

        // focusInEvent → emit focused() → lambda → onCardFocused() → bindingFocused()
        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(TestControllerBindingsView)
#include "test_controller_bindings_view.moc"
