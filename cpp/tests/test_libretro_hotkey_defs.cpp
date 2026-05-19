#include <QtTest>
#include <QSet>
#include "core/libretro/libretro_hotkey_defs.h"
#include "core/binding_def.h"

class TestLibretroHotkeyDefs : public QObject {
    Q_OBJECT
private slots:
    void allKeysUnique() {
        QSet<QString> seen;
        for (const auto& def : libretro_hotkeys::kLibretroHotkeys) {
            QVERIFY2(!seen.contains(def.key), qPrintable("duplicate key: " + def.key));
            seen.insert(def.key);
        }
    }
    void hasExpectedCount() {
        QCOMPARE(libretro_hotkeys::kLibretroHotkeys.size(), 22);
    }
    void everyDefHasSection() {
        for (const auto& def : libretro_hotkeys::kLibretroHotkeys)
            QCOMPARE(def.section, QStringLiteral("Hotkeys"));
    }
    void sentinelEmuIdIsStable() {
        QCOMPARE(libretro_hotkeys::kSentinelEmuId, QStringLiteral("_libretro_global"));
    }
};

QTEST_APPLESS_MAIN(TestLibretroHotkeyDefs)
#include "test_libretro_hotkey_defs.moc"
