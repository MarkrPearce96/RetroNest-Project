// cpp/tests/test_ppsspp_libretro_bindings.cpp
//
// Phase A regression guard for PpssppLibretroAdapter::controllerBindingDefsForType.
// Asserts the data shape contracts:
//   - exactly 12 rows (PSP physical surface)
//   - every .key matches a value retroPadSlotFromKey() recognises
//     (otherwise launch-time SDL -> RetroPad wiring silently skips it)
//   - every .cardSlot is one of the six the schema-driven view supports
//   - section is "Pad1" for all rows (libretro port 1 convention)

#include <QtTest>
#include <QSet>
#include "adapters/libretro/ppsspp_libretro_adapter.h"
#include "core/libretro/input_router.h"

class TestPpssppLibretroBindings : public QObject {
    Q_OBJECT
private slots:
    void rowCount_matchesPspSurface() {
        PpssppLibretroAdapter a;
        const auto defs = a.controllerBindingDefsForType("Standard");
        QCOMPARE(defs.size(), 12);
    }

    void everyKey_resolvesViaRetroPadSlot() {
        PpssppLibretroAdapter a;
        const auto defs = a.controllerBindingDefsForType("Standard");
        for (const auto& d : defs) {
            const RetroPadSlot slot = retroPadSlotFromKey(d.key);
            QVERIFY2(slot != RetroPadSlot::None,
                     qPrintable(QString("BindingDef '%1' has unrecognised .key '%2' "
                                        "(retroPadSlotFromKey returns None — runtime "
                                        "wiring will silently drop this binding)")
                                    .arg(d.label).arg(d.key)));
        }
    }

    void everyCardSlot_isInAllowedSet() {
        PpssppLibretroAdapter a;
        const auto defs = a.controllerBindingDefsForType("Standard");
        const QSet<QString> allowed{
            "DPad", "FaceButtons", "LeftAnalog", "RightAnalog", "Shoulders", "System"
        };
        for (const auto& d : defs) {
            QVERIFY2(allowed.contains(d.cardSlot),
                     qPrintable(QString("BindingDef '%1' has unexpected .cardSlot '%2'")
                                    .arg(d.label).arg(d.cardSlot)));
        }
    }

    void everySection_isPad1() {
        PpssppLibretroAdapter a;
        const auto defs = a.controllerBindingDefsForType("Standard");
        for (const auto& d : defs) {
            QCOMPARE(d.section, QStringLiteral("Pad1"));
        }
    }

    void facialMapping_matchesPlayStationConvention() {
        PpssppLibretroAdapter a;
        const auto defs = a.controllerBindingDefsForType("Standard");
        auto findByLabel = [&](const QString& label) -> const BindingDef* {
            for (const auto& d : defs) if (d.label == label) return &d;
            return nullptr;
        };
        // PSP face buttons follow PlayStation conventions:
        //   Cross  (bottom) -> RetroPad B (south)
        //   Circle (right)  -> RetroPad A (east)
        //   Square (left)   -> RetroPad Y (west)
        //   Triangle (top)  -> RetroPad X (north)
        QVERIFY(findByLabel("Cross"));    QCOMPARE(findByLabel("Cross")->key,    QStringLiteral("B"));
        QVERIFY(findByLabel("Circle"));   QCOMPARE(findByLabel("Circle")->key,   QStringLiteral("A"));
        QVERIFY(findByLabel("Square"));   QCOMPARE(findByLabel("Square")->key,   QStringLiteral("Y"));
        QVERIFY(findByLabel("Triangle")); QCOMPARE(findByLabel("Triangle")->key, QStringLiteral("X"));
    }
};

QTEST_GUILESS_MAIN(TestPpssppLibretroBindings)
#include "test_ppsspp_libretro_bindings.moc"
