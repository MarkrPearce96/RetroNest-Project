#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDir>
#include "services/hotkey_service.h"
#include "core/libretro/libretro_hotkey_defs.h"

class TestHotkeyServiceLibretro : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tmp;

private slots:
    void initTestCase() {
        QVERIFY(m_tmp.isValid());
        QStandardPaths::setTestModeEnabled(true);
    }

    void sentinelReturnsLibretroSchema() {
        HotkeyService hs;
        QVariantList rows = hs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        QCOMPARE(rows.size(), libretro_hotkeys::kLibretroHotkeys.size());

        // Every returned row exposes the expected fields.
        QVariantMap first = rows.first().toMap();
        QVERIFY(first.contains(QStringLiteral("label")));
        QVERIFY(first.contains(QStringLiteral("group")));
        QVERIFY(first.contains(QStringLiteral("section")));
        QVERIFY(first.contains(QStringLiteral("key")));
        QVERIFY(first.contains(QStringLiteral("defaultValue")));
        QVERIFY(first.contains(QStringLiteral("currentValue")));
        // No saved value yet → currentValue == defaultValue
        QCOMPARE(first.value(QStringLiteral("currentValue")).toString(),
                 first.value(QStringLiteral("defaultValue")).toString());
    }

    void saveThenReadBackRoundTrips() {
        HotkeyService hs;
        hs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Pause,
                      QStringLiteral("Keyboard/Z"));

        QVariantList rows = hs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        bool found = false;
        for (const QVariant& v : rows) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("key")).toString() == libretro_hotkeys::ids::Pause) {
                QCOMPARE(m.value(QStringLiteral("currentValue")).toString(),
                         QStringLiteral("Keyboard/Z"));
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void clearReturnsToDefault() {
        HotkeyService hs;
        hs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Mute,
                      QStringLiteral("Keyboard/Y"));
        hs.clearHotkey(libretro_hotkeys::kSentinelEmuId,
                       QStringLiteral("Hotkeys"),
                       libretro_hotkeys::ids::Mute);
        QVariantList rows = hs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        for (const QVariant& v : rows) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("key")).toString() == libretro_hotkeys::ids::Mute) {
                // After clear, currentValue should equal defaultValue.
                QCOMPARE(m.value(QStringLiteral("currentValue")).toString(),
                         m.value(QStringLiteral("defaultValue")).toString());
                return;
            }
        }
        QFAIL("Mute row not found");
    }

    void resetReturnsAllToDefaults() {
        HotkeyService hs;
        hs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Pause,
                      QStringLiteral("Keyboard/Q"));
        hs.resetHotkeys(libretro_hotkeys::kSentinelEmuId);
        QVariantList rows = hs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        for (const QVariant& v : rows) {
            QVariantMap m = v.toMap();
            QCOMPARE(m.value(QStringLiteral("currentValue")).toString(),
                     m.value(QStringLiteral("defaultValue")).toString());
        }
    }
};

QTEST_MAIN(TestHotkeyServiceLibretro)
#include "test_hotkey_service_libretro.moc"
