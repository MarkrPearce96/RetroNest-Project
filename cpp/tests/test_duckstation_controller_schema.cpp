// cpp/tests/test_duckstation_controller_schema.cpp
#include <QtTest>
#include <QSet>
#include "adapters/duckstation_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"

// Pins the contract for DuckStation's slimmed controller schema:
//  - exactly one controller type (AnalogController)
//  - controllerSettingDefs / controllerSettingDefsForType deliberately empty
//  - every AnalogController BindingDef carries a non-empty cardSlot
//  - every AnalogController binding that *should* map to a physical button
//    has a non-zero spotlightR
//
// If a PR drifts any of these, the test trips loud rather than producing
// a silently-wrong UI.

class TestDuckStationControllerSchema : public QObject {
    Q_OBJECT

private:
    DuckStationAdapter adapter_;

private slots:
    void testSingleControllerType() {
        const auto types = adapter_.controllerTypes();
        QCOMPARE(types.size(), 1);
        QCOMPARE(types.front().id, QString("AnalogController"));
        QCOMPARE(types.front().displayName, QString("Analog Controller"));
        QVERIFY(types.front().svgResource.endsWith("analog_controller.svg"));
    }

    void testNoControllerSettings() {
        QVERIFY(adapter_.controllerSettingDefs().isEmpty());
        QVERIFY(adapter_.controllerSettingDefsForType("AnalogController").isEmpty());
    }

    void testBindingsHaveCardSlot() {
        const auto bindings = adapter_.controllerBindingDefsForType("AnalogController");
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
        const auto bindings = adapter_.controllerBindingDefsForType("AnalogController");
        // Physical-button labels that must light up on the controller artwork.
        // (LargeMotor / SmallMotor have no visible button, so they're allowed
        //  to leave spotlightR == 0.)
        const QSet<QString> mustHaveSpotlight{
            "Up", "Down", "Left", "Right",
            "Triangle", "Circle", "Cross", "Square",
            "L1", "L2", "R1", "R2",
            "Left Stick Up", "Left Stick Down", "Left Stick Left", "Left Stick Right",
            "Right Stick Up", "Right Stick Down", "Right Stick Left", "Right Stick Right",
            "L3", "R3",
            "Select", "Start", "Analog",
        };
        for (const auto& b : bindings) {
            if (!mustHaveSpotlight.contains(b.label)) continue;
            QVERIFY2(b.spotlightR > 0,
                qPrintable(QString("binding '%1' must have non-zero spotlightR").arg(b.label)));
            QVERIFY2(b.spotlightX > 0 && b.spotlightY > 0,
                qPrintable(QString("binding '%1' must have positive spotlight (x,y)").arg(b.label)));
        }
    }

    void testNoAlternateControllerTypes() {
        // The DigitalController / AnalogJoystick / NeGcon / NeGconRumble /
        // JogCon / PopnController / None branches have been removed — the
        // adapter should return an empty list for any non-AnalogController
        // type.
        for (const QString& dropped : {
            "None", "DigitalController", "AnalogJoystick",
            "NeGcon", "NeGconRumble", "JogCon", "PopnController",
        }) {
            QVERIFY2(adapter_.controllerBindingDefsForType(dropped).isEmpty(),
                qPrintable(QString("type '%1' should have no bindings").arg(dropped)));
            QVERIFY2(adapter_.controllerSettingDefsForType(dropped).isEmpty(),
                qPrintable(QString("type '%1' should have no settings").arg(dropped)));
        }
    }
};

QTEST_MAIN(TestDuckStationControllerSchema)
#include "test_duckstation_controller_schema.moc"
