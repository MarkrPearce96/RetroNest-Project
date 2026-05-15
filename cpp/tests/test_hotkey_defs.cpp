// cpp/tests/test_hotkey_defs.cpp
//
// Regression guard for adapter hotkeyBindingDefs() lists. One slot per
// emulator. Asserts:
//  - total entry count matches the spec appendix
//  - every overlay-conflict / platform-irrelevance trim-list key is ABSENT
//  - a hand-picked sample of expected rows is PRESENT
//  - sample rows live in the upstream-correct group
//
// Spec: docs/superpowers/specs/2026-05-08-hotkey-defs-upstream-audit-design.md

#include <QtTest>

#include "core/binding_def.h"
#include "adapters/duckstation_adapter.h"
#include "adapters/ppsspp_adapter.h"

class TestHotkeyDefs : public QObject {
    Q_OBJECT

private:
    static QStringList keysOf(const QVector<HotkeyDef>& defs) {
        QStringList ks;
        ks.reserve(defs.size());
        for (const auto& d : defs) ks << d.key;
        return ks;
    }
    static const HotkeyDef* findKey(const QVector<HotkeyDef>& defs, const QString& key) {
        for (const auto& d : defs) if (d.key == key) return &d;
        return nullptr;
    }

private slots:
    void duckstation_completeness() {
        DuckStationAdapter adapter;
        const auto defs = adapter.hotkeyBindingDefs();

        QCOMPARE(defs.size(), 102);

        const QStringList keys = keysOf(defs);
        // Overlay-conflict trim list — these MUST NOT appear.
        QVERIFY(!keys.contains("OpenPauseMenu"));
        QVERIFY(!keys.contains("TogglePause"));
        QVERIFY(!keys.contains("ToggleFullscreen"));
        QVERIFY(!keys.contains("PowerOff"));  // newly removed by trim policy

        // Sample present rows.
        QVERIFY(keys.contains("OpenCheatsMenu"));        // Interface (newly added)
        QVERIFY(keys.contains("Screenshot"));            // Interface
        QVERIFY(keys.contains("FastForward"));           // System
        QVERIFY(keys.contains("ToggleMediaCapture"));    // System (newly added)
        QVERIFY(keys.contains("FreecamToggle"));         // Free Camera (newly added)
        QVERIFY(keys.contains("FreecamRollLeft"));       // Free Camera (newly added)
        QVERIFY(keys.contains("ToggleSoftwareRendering")); // Graphics
        QVERIFY(keys.contains("ToggleOSD"));               // Graphics (newly added)
        QVERIFY(keys.contains("AudioMute"));             // Audio
        QVERIFY(keys.contains("LoadGameState1"));        // Save States
        QVERIFY(keys.contains("LoadGlobalState1"));      // Save States (newly added)
        QVERIFY(keys.contains("ToggleVRAMView"));        // Debugging (newly added)

        // Sample group placements.
        const HotkeyDef* freecam = findKey(defs, "FreecamToggle");
        QVERIFY(freecam);
        QCOMPARE(freecam->group, QStringLiteral("Free Camera"));

        const HotkeyDef* loadGlobal = findKey(defs, "LoadGlobalState1");
        QVERIFY(loadGlobal);
        QCOMPARE(loadGlobal->group, QStringLiteral("Save States"));

        const HotkeyDef* toggleOSD = findKey(defs, "ToggleOSD");
        QVERIFY(toggleOSD);
        QCOMPARE(toggleOSD->group, QStringLiteral("Graphics"));

        const HotkeyDef* vramView = findKey(defs, "ToggleVRAMView");
        QVERIFY(vramView);
        QCOMPARE(vramView->group, QStringLiteral("Debugging"));

        // Default value preserved on FastForward.
        const HotkeyDef* fastForward = findKey(defs, "FastForward");
        QVERIFY(fastForward);
        QCOMPARE(fastForward->defaultValue, QStringLiteral("Keyboard/Tab"));
    }

    void ppsspp_completeness() {
        PPSSPPAdapter adapter;
        const auto defs = adapter.hotkeyBindingDefs();

        QCOMPARE(defs.size(), 25);

        const QStringList keys = keysOf(defs);
        // Overlay-conflict trim list — these MUST NOT appear.
        QVERIFY(!keys.contains("Pause"));
        QVERIFY(!keys.contains("Pause (no menu)"));
        QVERIFY(!keys.contains("Toggle Fullscreen"));
        QVERIFY(!keys.contains("Exit App"));
        // Platform-irrelevance trim list — these MUST NOT appear.
        QVERIFY(!keys.contains("VR camera adjust"));
        QVERIFY(!keys.contains("VR camera reset"));
        QVERIFY(!keys.contains("Toggle WLAN"));
        QVERIFY(!keys.contains("Toggle touch controls"));
        QVERIFY(!keys.contains("OpenChat"));
        QVERIFY(!keys.contains("Toggle mouse input"));
        QVERIFY(!keys.contains("DevMenu"));
        QVERIFY(!keys.contains("Toggle Debugger"));
        QVERIFY(!keys.contains("Texture Dumping"));
        QVERIFY(!keys.contains("Texture Replacement"));
        QVERIFY(!keys.contains("Audio/Video Recording"));

        // Sample present rows.
        QVERIFY(keys.contains("RapidFire"));
        QVERIFY(keys.contains("Analog limiter"));
        QVERIFY(keys.contains("Fast-forward"));
        QVERIFY(keys.contains("Save State"));
        QVERIFY(keys.contains("Display Portrait"));
        QVERIFY(keys.contains("Toggle tilt control"));

        // Group renames / placements.
        const HotkeyDef* saveState = findKey(defs, "Save State");
        QVERIFY(saveState);
        QCOMPARE(saveState->group, QStringLiteral("Emulator controls"));

        const HotkeyDef* rapidFire = findKey(defs, "RapidFire");
        QVERIFY(rapidFire);
        QCOMPARE(rapidFire->group, QStringLiteral("Control modifiers"));

        // Default value preserved on Fast-forward (right-trigger axis-positive).
        const HotkeyDef* fastForward = findKey(defs, "Fast-forward");
        QVERIFY(fastForward);
        QCOMPARE(fastForward->defaultValue, QStringLiteral("10-4036"));

        // controls.ini section is correct (PPSSPP uses ControlMapping, not Hotkeys).
        QCOMPARE(saveState->section, QStringLiteral("ControlMapping"));
    }
};

QTEST_GUILESS_MAIN(TestHotkeyDefs)
#include "test_hotkey_defs.moc"
