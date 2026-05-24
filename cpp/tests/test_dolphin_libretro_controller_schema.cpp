// cpp/tests/test_dolphin_libretro_controller_schema.cpp
#include <QtTest>
#include <QSet>
#include "adapters/libretro/dolphin_libretro_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"
#include "core/libretro/input_router.h"

// Pins the contract for DolphinLibretroAdapter's controller schema (SP5):
//  - exactly two controller types (GCPad1, Wiimote1), each with an SVG
//  - every digital BindingDef key resolves to a RetroPad slot, so the
//    InputRouter actually binds it on launch (game_session.cpp uses
//    retroPadSlotFromKey + skips RetroPadSlot::None)
//  - every binding has a known cardSlot and a non-degenerate spotlight
//  - the empty-type call (used by ensureConfig + game_session) returns the
//    GC layout
class TestDolphinLibretroControllerSchema : public QObject {
    Q_OBJECT

private:
    DolphinLibretroAdapter adapter_;

    static const QSet<QString>& validSlots() {
        static const QSet<QString> v{
            "DPad", "FaceButtons", "LeftAnalog", "RightAnalog", "Shoulders", "System",
        };
        return v;
    }

private slots:
    void testBothControllerTypes() {
        const auto types = adapter_.controllerTypes();
        QCOMPARE(types.size(), 2);
        QCOMPARE(types[0].id, QString("GCPad1"));
        QCOMPARE(types[1].id, QString("Wiimote1"));
        for (const auto& t : types)
            QVERIFY2(!t.svgResource.isEmpty(),
                qPrintable(QString("missing svgResource for %1").arg(t.id)));
    }

    void testEveryDigitalKeyResolves() {
        for (const QString& type : {QStringLiteral("GCPad1"), QStringLiteral("Wiimote1")}) {
            const auto bindings = adapter_.controllerBindingDefsForType(type);
            QVERIFY(!bindings.isEmpty());
            for (const auto& b : bindings) {
                if (b.kind != BindingDef::Button) continue;
                QVERIFY2(retroPadSlotFromKey(b.key) != RetroPadSlot::None,
                    qPrintable(QString("unresolved digital key '%1' (%2) in %3")
                               .arg(b.key, b.label, type)));
            }
        }
    }

    void testBindingsHaveValidCardSlot() {
        for (const QString& type : {QStringLiteral("GCPad1"), QStringLiteral("Wiimote1")}) {
            for (const auto& b : adapter_.controllerBindingDefsForType(type)) {
                QVERIFY2(validSlots().contains(b.cardSlot),
                    qPrintable(QString("binding '%1' has invalid cardSlot '%2' in %3")
                               .arg(b.label, b.cardSlot, type)));
            }
        }
    }

    void testSpotlightsNonDegenerate() {
        for (const QString& type : {QStringLiteral("GCPad1"), QStringLiteral("Wiimote1")}) {
            for (const auto& b : adapter_.controllerBindingDefsForType(type)) {
                QVERIFY2(b.spotlightR > 0 && b.spotlightX > 0 && b.spotlightY > 0,
                    qPrintable(QString("degenerate spotlight for '%1' in %2").arg(b.label, type)));
            }
        }
    }

    void testEmptyTypeFallsBackToGc() {
        // ensureConfig() + game_session call controllerBindingDefsForType({}); it
        // must return the active (GameCube) layout, not an empty list.
        QCOMPARE(adapter_.controllerBindingDefsForType({}).size(),
                 adapter_.controllerBindingDefsForType("GCPad1").size());
    }
};

QTEST_MAIN(TestDolphinLibretroControllerSchema)
#include "test_dolphin_libretro_controller_schema.moc"
