#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDir>
#include "services/config_service.h"
#include "core/libretro/libretro_hotkey_defs.h"

class TestConfigServiceLibretroHotkeys : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tmp;

private slots:
    void initTestCase() {
        QVERIFY(m_tmp.isValid());
        QStandardPaths::setTestModeEnabled(true);
    }

    void sentinelReturnsLibretroSchema() {
        ConfigService cs(/*loader=*/nullptr);
        QVariantList rows = cs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
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
        ConfigService cs(/*loader=*/nullptr);
        cs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Pause,
                      QStringLiteral("Keyboard/Z"));

        QVariantList rows = cs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
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
        ConfigService cs(/*loader=*/nullptr);
        cs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Mute,
                      QStringLiteral("Keyboard/Y"));
        cs.clearHotkey(libretro_hotkeys::kSentinelEmuId,
                       QStringLiteral("Hotkeys"),
                       libretro_hotkeys::ids::Mute);
        QVariantList rows = cs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
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
        ConfigService cs(/*loader=*/nullptr);
        cs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Pause,
                      QStringLiteral("Keyboard/Q"));
        cs.resetHotkeys(libretro_hotkeys::kSentinelEmuId);
        QVariantList rows = cs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        for (const QVariant& v : rows) {
            QVariantMap m = v.toMap();
            QCOMPARE(m.value(QStringLiteral("currentValue")).toString(),
                     m.value(QStringLiteral("defaultValue")).toString());
        }
    }
};

QTEST_MAIN(TestConfigServiceLibretroHotkeys)
#include "test_config_service_libretro_hotkeys.moc"
