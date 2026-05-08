// cpp/tests/test_ppsspp_controller_schema.cpp
#include <QtTest>
#include <QSet>
#include "adapters/ppsspp_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"

// Pins the contract for PPSSPP's slimmed controller schema:
//  - exactly one controller type ("Standard" — PSP Controller)
//  - controllerSettingDefs / controllerSettingDefsForType deliberately empty
//  - every binding carries a non-empty cardSlot
//  - every binding that should map to a physical button has a non-zero
//    spotlightR (PSP has no abstract motors / mode toggle, so all
//    bindings fall in this set)
//
// If a PR drifts any of these, the test trips loud rather than producing
// a silently-wrong UI.

class TestPPSSPPControllerSchema : public QObject {
    Q_OBJECT

private:
    PPSSPPAdapter adapter_;

private slots:
    void testSingleControllerType() {
        const auto types = adapter_.controllerTypes();
        QCOMPARE(types.size(), 1);
        QCOMPARE(types.front().id, QString("Standard"));
        QCOMPARE(types.front().displayName, QString("PSP Controller"));
        QVERIFY(types.front().svgResource.endsWith("PSP.svg"));
    }

    void testNoControllerSettings() {
        QVERIFY(adapter_.controllerSettingDefsForType("Standard").isEmpty());
    }

    void testBindingsHaveCardSlot() {
        const auto bindings = adapter_.controllerBindingDefsForType("Standard");
        QVERIFY(!bindings.isEmpty());

        const QSet<QString> validSlots{
            "DPad", "FaceButtons", "LeftAnalog", "RightAnalog",
            "Shoulders", "LeftShoulders", "RightShoulders", "System",
        };
        for (const auto& b : bindings) {
            QVERIFY2(!b.cardSlot.isEmpty(),
                qPrintable(QString("binding '%1' has empty cardSlot").arg(b.label)));
            QVERIFY2(validSlots.contains(b.cardSlot),
                qPrintable(QString("binding '%1' has unknown cardSlot '%2'")
                           .arg(b.label, b.cardSlot)));
        }
    }

    void testPhysicalButtonsHaveSpotlight() {
        const auto bindings = adapter_.controllerBindingDefsForType("Standard");
        // PSP has no abstract bindings (no rumble motors, no analog-mode
        // button) — every binding must light up a physical button on the
        // SVG.
        const QSet<QString> mustHaveSpotlight{
            "Up", "Down", "Left", "Right",
            "Triangle", "Circle", "Cross", "Square",
            "L", "R",
            "Select", "Start",
            "An.Up", "An.Down", "An.Left", "An.Right",
        };
        QCOMPARE(bindings.size(), mustHaveSpotlight.size());
        for (const auto& b : bindings) {
            QVERIFY2(mustHaveSpotlight.contains(b.label),
                qPrintable(QString("unexpected binding '%1'").arg(b.label)));
            QVERIFY2(b.spotlightR > 0,
                qPrintable(QString("binding '%1' must have non-zero spotlightR").arg(b.label)));
            QVERIFY2(b.spotlightX > 0 && b.spotlightY > 0,
                qPrintable(QString("binding '%1' must have positive spotlight (x,y)").arg(b.label)));
        }
    }

    void testNoAlternateControllerTypes() {
        // The legacy "NotConnected" entry has been removed — the adapter
        // should return an empty list for any non-Standard type.
        for (const QString& dropped : {"NotConnected", "AnalogController", "DualShock2"}) {
            QVERIFY2(adapter_.controllerBindingDefsForType(dropped).isEmpty(),
                qPrintable(QString("type '%1' should have no bindings").arg(dropped)));
            QVERIFY2(adapter_.controllerSettingDefsForType(dropped).isEmpty(),
                qPrintable(QString("type '%1' should have no settings").arg(dropped)));
        }
    }
};

QTEST_MAIN(TestPPSSPPControllerSchema)
#include "test_ppsspp_controller_schema.moc"
