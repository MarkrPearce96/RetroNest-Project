// cpp/tests/test_dolphin_controller_schema.cpp
#include <QtTest>
#include <QSet>
#include "adapters/dolphin_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"

// Pins Dolphin's two-controller schema (GameCube + Wii Classic Controller).
// Every binding clusters into a known cardSlot; physical buttons have
// non-zero spotlights; bindings route to GCPadNew.ini vs WiimoteNew.ini by
// type id; formatBinding translates SDL element names into Dolphin's
// expression syntax. The Wii type maps to the Classic Controller extension
// — we ship Classic by default since most users play with a regular gamepad
// and the bare Wiimote needs hardware most people don't have.

class TestDolphinControllerSchema : public QObject {
    Q_OBJECT

private:
    DolphinAdapter adapter_;

private slots:
    void testTwoControllerTypes() {
        const auto types = adapter_.controllerTypes();
        QCOMPARE(types.size(), 2);

        QSet<QString> ids;
        for (const auto& t : types) ids.insert(t.id);
        QCOMPARE(ids, QSet<QString>({"GCPad1", "Wiimote1"}));

        for (const auto& t : types) {
            if (t.id == "GCPad1") {
                QCOMPARE(t.displayName, QString("GameCube Controller"));
                QVERIFY(t.svgResource.endsWith("GameCube.svg"));
                QVERIFY(t.slotTitleOverrides.isEmpty());
            } else {
                QCOMPARE(t.displayName, QString("Wii Classic Controller"));
                QVERIFY(t.svgResource.endsWith("Wii_classiccontroller.svg"));
                QVERIFY(t.slotTitleOverrides.isEmpty());
            }
        }
    }

    void testNoControllerSettings() {
        QVERIFY(adapter_.controllerSettingDefsForType("GCPad1").isEmpty());
        QVERIFY(adapter_.controllerSettingDefsForType("Wiimote1").isEmpty());
    }

    void testGcPadBindingsCount() {
        QCOMPARE(adapter_.controllerBindingDefsForType("GCPad1").size(), 23);
    }

    void testWiimoteBindingsCount() {
        // Classic Controller: 4 D-Pad + 4 face + 4 Lstick + 4 Rstick
        // + 6 shoulder/trigger (L, L-Analog, ZL, R, R-Analog, ZR)
        // + 3 system (- / Home / +) + 1 rumble = 26 bindings
        QCOMPARE(adapter_.controllerBindingDefsForType("Wiimote1").size(), 26);
    }

    void testBindingsHaveCardSlot() {
        const QSet<QString> validSlots{
            "DPad", "FaceButtons", "LeftAnalog", "RightAnalog",
            "LeftShoulders", "RightShoulders", "System",
        };
        for (const QString& type : {"GCPad1", "Wiimote1"}) {
            for (const auto& b : adapter_.controllerBindingDefsForType(type)) {
                QVERIFY2(!b.cardSlot.isEmpty(),
                    qPrintable(QString("[%1] '%2' has empty cardSlot").arg(type, b.label)));
                QVERIFY2(validSlots.contains(b.cardSlot),
                    qPrintable(QString("[%1] '%2' has unknown cardSlot '%3'")
                               .arg(type, b.label, b.cardSlot)));
            }
        }
    }

    void testGcPadPhysicalButtonsHaveSpotlight() {
        const QSet<QString> mustHaveSpotlight{
            "Up", "Down", "Left", "Right",
            "A", "B", "X", "Y",
            "Main Stick Up", "Main Stick Down", "Main Stick Left", "Main Stick Right",
            "C-Stick Up", "C-Stick Down", "C-Stick Left", "C-Stick Right",
            "L (digital)", "L-Analog", "R (digital)", "R-Analog", "Z",
            "Start",
        };
        for (const auto& b : adapter_.controllerBindingDefsForType("GCPad1")) {
            if (!mustHaveSpotlight.contains(b.label)) continue;
            QVERIFY2(b.spotlightR > 0,
                qPrintable(QString("GCPad '%1' must have spotlightR > 0").arg(b.label)));
        }
    }

    void testWiimotePhysicalButtonsHaveSpotlight() {
        // Wii Classic Controller. Rumble/Motor is abstract — exempt.
        const QSet<QString> mustHaveSpotlight{
            "Up", "Down", "Left", "Right",
            "A", "B", "X", "Y",
            "Left Stick Up", "Left Stick Down", "Left Stick Left", "Left Stick Right",
            "Right Stick Up", "Right Stick Down", "Right Stick Left", "Right Stick Right",
            "L (digital)", "L-Analog", "ZL",
            "R (digital)", "R-Analog", "ZR",
            "Minus", "Plus", "Home",
        };
        for (const auto& b : adapter_.controllerBindingDefsForType("Wiimote1")) {
            if (!mustHaveSpotlight.contains(b.label)) continue;
            QVERIFY2(b.spotlightR > 0,
                qPrintable(QString("Classic '%1' must have spotlightR > 0").arg(b.label)));
        }
    }

    void testGcPadRoutesToGcPadFile() {
        QVERIFY(adapter_.controllerBindingsConfigFilePath("GCPad1").endsWith("GCPadNew.ini"));
        QCOMPARE(adapter_.controllerBindingsSection(1, "GCPad1"), QString("GCPad1"));
    }

    void testWiimoteRoutesToWiimoteFile() {
        QVERIFY(adapter_.controllerBindingsConfigFilePath("Wiimote1").endsWith("WiimoteNew.ini"));
        QCOMPARE(adapter_.controllerBindingsSection(1, "Wiimote1"), QString("Wiimote1"));
    }

    void testFormatBindingTranslation() {
        struct Case {
            QString sdlElement; bool isAxis; bool positive; QString expected;
        };
        const QVector<Case> cases = {
            {"FaceSouth",     false, false, "`Button S`"},
            {"FaceEast",      false, false, "`Button E`"},
            {"FaceWest",      false, false, "`Button W`"},
            {"FaceNorth",     false, false, "`Button N`"},
            {"DPadUp",        false, false, "`Pad N`"},
            {"DPadDown",      false, false, "`Pad S`"},
            {"DPadLeft",      false, false, "`Pad W`"},
            {"DPadRight",     false, false, "`Pad E`"},
            {"LeftShoulder",  false, false, "`Shoulder L`"},
            {"RightShoulder", false, false, "`Shoulder R`"},
            {"LeftTrigger",   true,  true,  "`Trigger L`"},
            {"RightTrigger",  true,  true,  "`Trigger R`"},
            {"LeftStick",     false, false, "`Thumb L`"},
            {"RightStick",    false, false, "`Thumb R`"},
            {"Back",          false, false, "`Back`"},
            {"Start",         false, false, "`Start`"},
            {"Guide",         false, false, "`Guide`"},
            {"LeftX",         true,  true,  "`Left X+`"},
            {"LeftX",         true,  false, "`Left X-`"},
            {"LeftY",         true,  true,  "`Left Y+`"},
            {"LeftY",         true,  false, "`Left Y-`"},
            {"RightX",        true,  true,  "`Right X+`"},
            {"RightX",        true,  false, "`Right X-`"},
            {"RightY",        true,  true,  "`Right Y+`"},
            {"RightY",        true,  false, "`Right Y-`"},
        };
        for (const auto& c : cases) {
            QCOMPARE(adapter_.formatBinding(0, c.sdlElement, c.isAxis, c.positive),
                     c.expected);
        }
    }
};

QTEST_MAIN(TestDolphinControllerSchema)
#include "test_dolphin_controller_schema.moc"
