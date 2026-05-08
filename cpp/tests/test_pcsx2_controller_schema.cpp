// cpp/tests/test_pcsx2_controller_schema.cpp
#include <QtTest>
#include <QSet>
#include "adapters/pcsx2_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"

// Pins the contract for PCSX2's slimmed controller schema:
//  - exactly one controller type (DualShock 2)
//  - controllerSettingDefs / controllerSettingDefsForType deliberately empty
//  - every DS2 BindingDef carries a non-empty cardSlot
//  - every DS2 binding that *should* map to a physical button has a
//    non-zero spotlightR
//
// If a PR drifts any of these, the test trips loud rather than producing
// a silently-wrong UI.

class TestPcsx2ControllerSchema : public QObject {
    Q_OBJECT

private:
    PCSX2Adapter adapter_;

private slots:
    void testSingleControllerType() {
        const auto types = adapter_.controllerTypes();
        QCOMPARE(types.size(), 1);
        QCOMPARE(types.front().id, QString("DualShock2"));
        QCOMPARE(types.front().displayName, QString("DualShock 2"));
        QVERIFY(types.front().svgResource.endsWith("DualShock_2.svg"));
    }

    void testNoControllerSettings() {
        QVERIFY(adapter_.controllerSettingDefsForType("DualShock2").isEmpty());
    }

    void testDualShock2BindingsHaveCardSlot() {
        const auto bindings = adapter_.controllerBindingDefsForType("DualShock2");
        QVERIFY(!bindings.isEmpty());

        // Every binding must declare a slot; falling back to group is fine
        // for adapters mid-migration but PCSX2 is the pilot — we expect
        // the new field to be populated explicitly.
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
        const auto bindings = adapter_.controllerBindingDefsForType("DualShock2");
        // Physical-button labels that must light up on the controller artwork.
        // (LargeMotor / SmallMotor / Pressure Modifier / Analog have no
        //  visible button, so they're allowed to leave spotlightR == 0.)
        const QSet<QString> mustHaveSpotlight{
            "Up", "Down", "Left", "Right",
            "Triangle", "Circle", "Cross", "Square",
            "L1", "L2", "R1", "R2",
            "Left Stick Up", "Left Stick Down", "Left Stick Left", "Left Stick Right",
            "Right Stick Up", "Right Stick Down", "Right Stick Left", "Right Stick Right",
            "L3", "R3",
            "Select", "Start",
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
        // The Guitar / Jogcon / NeGcon / Pop'n branches have been removed
        // — the adapter should return an empty list for any non-DS2 type.
        for (const QString& dropped : {"Guitar", "Jogcon", "Negcon", "Popn", "NotConnected"}) {
            QVERIFY2(adapter_.controllerBindingDefsForType(dropped).isEmpty(),
                qPrintable(QString("type '%1' should have no bindings").arg(dropped)));
            QVERIFY2(adapter_.controllerSettingDefsForType(dropped).isEmpty(),
                qPrintable(QString("type '%1' should have no settings").arg(dropped)));
        }
    }
};

QTEST_MAIN(TestPcsx2ControllerSchema)
#include "test_pcsx2_controller_schema.moc"
