// cpp/tests/test_detail_actions.cpp
//
// Pins the manifest-driven ACTIONS row model that replaced
// EmulatorDetailPage.qml's per-emulator branching (packet 7 stage 3):
// row order, the controller-page expansion, the hotkeys/patches
// conditionals, and the verbatim button copy the QML used to hardcode.

#include <QtTest>
#include "core/detail_actions.h"

class TestDetailActions : public QObject {
    Q_OBJECT
    static EmulatorManifest base(const QString& id, const QString& name) {
        EmulatorManifest m; m.id = id; m.name = name; return m;
    }
    static QStringList actions(const QVariantList& rows) {
        QStringList out;
        for (const auto& r : rows) out << r.toMap().value("action").toString();
        return out;
    }
private slots:
    void notInstalled_isEmpty() {
        QVERIFY(detailActionRows(base("x", "X"), false, true).isEmpty());
    }
    void plainEmulator_defaultChain() {
        const auto rows = detailActionRows(base("mgba", "mGBA"), true, true);
        QCOMPARE(actions(rows), QStringList({"settings","controller","hotkeys",
                                             "reinstall","reset","uninstall"}));
        QCOMPARE(rows[0].toMap()["label"].toString(), QString("Emulator Settings"));
        QCOMPARE(rows[1].toMap()["label"].toString(), QString("Controller Mapping"));
        QCOMPARE(rows[1].toMap()["controllerType"].toString(), QString(""));
        QCOMPARE(rows[3].toMap()["label"].toString(), QString("Reinstall / Update"));
        QCOMPARE(rows[4].toMap()["label"].toString(), QString("Reset Configuration"));
        QCOMPARE(rows[5].toMap()["label"].toString(), QString("Uninstall"));
    }
    void noHotkeys_dropsRow() {
        const auto rows = detailActionRows(base("x", "X"), true, false);
        QVERIFY(!actions(rows).contains("hotkeys"));
        QCOMPARE(rows.size(), 5);
    }
    void dolphin_twoControllerPages() {
        auto m = base("dolphin", "Dolphin");
        m.controller_pages = { {"GameCube Controller", "GCPad1"},
                               {"Wii Classic Controller", "Wiimote1"} };
        const auto rows = detailActionRows(m, true, true);
        QCOMPARE(actions(rows), QStringList({"settings","controller","controller",
                                             "hotkeys","reinstall","reset","uninstall"}));
        QCOMPARE(rows[1].toMap()["label"].toString(), QString("GameCube Controller"));
        QCOMPARE(rows[1].toMap()["controllerType"].toString(), QString("GCPad1"));
        QCOMPARE(rows[2].toMap()["label"].toString(), QString("Wii Classic Controller"));
        QCOMPARE(rows[2].toMap()["controllerType"].toString(), QString("Wiimote1"));
    }
    void pcsx2_patchesRowAfterHotkeys() {
        auto m = base("pcsx2", "PCSX2");
        m.has_patches = true;
        const auto rows = detailActionRows(m, true, true);
        QCOMPARE(actions(rows), QStringList({"settings","controller","hotkeys","patches",
                                             "reinstall","reset","uninstall"}));
        // Copy pinned verbatim — this is the string the QML used to hardcode.
        QCOMPARE(rows[3].toMap()["label"].toString(), QString("Refresh PCSX2 Patches"));
    }
};
QTEST_APPLESS_MAIN(TestDetailActions)
#include "test_detail_actions.moc"
