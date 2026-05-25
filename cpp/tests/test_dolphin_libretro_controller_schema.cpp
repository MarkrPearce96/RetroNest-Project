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

    void testEmptyTypeIsFeedSuperset() {
        // ensureConfig() + game_session call controllerBindingDefsForType({}) to
        // seed controls.ini + feed the InputRouter. It must be the UNION of every
        // RetroPad slot either controller needs: every GameCube key plus the
        // Wii-Classic-only slots ZL/minus/Home (L2/R2/R3).
        const auto feed = adapter_.controllerBindingDefsForType({});
        const auto gc = adapter_.controllerBindingDefsForType("GCPad1");
        QVERIFY(feed.size() > gc.size());

        QSet<QString> feedKeys;
        for (const auto& b : feed) feedKeys.insert(b.key);
        for (const auto& b : gc)
            QVERIFY2(feedKeys.contains(b.key),
                qPrintable(QString("feed set missing GameCube key '%1'").arg(b.key)));
        for (const QString& wiiOnly : {QStringLiteral("L2"), QStringLiteral("R2"), QStringLiteral("R3")})
            QVERIFY2(feedKeys.contains(wiiOnly),
                qPrintable(QString("feed set missing Wii Classic slot '%1'").arg(wiiOnly)));

        // Every fed key must resolve to a real RetroPad slot (game_session skips
        // RetroPadSlot::None, so an unresolved key would silently not be fed).
        for (const auto& b : feed)
            QVERIFY2(retroPadSlotFromKey(b.key) != RetroPadSlot::None,
                qPrintable(QString("unfed feed key '%1'").arg(b.key)));
    }
};

QTEST_MAIN(TestDolphinLibretroControllerSchema)
#include "test_dolphin_libretro_controller_schema.moc"
