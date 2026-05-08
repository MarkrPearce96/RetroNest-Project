# Hotkey Defs Upstream Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring `hotkeyBindingDefs()` in PCSX2 / DuckStation / PPSSPP adapters into verbatim alignment with each emulator's upstream hotkey list — categories, group order, entry order, action keys, display labels, and defaults — pinned to upstream `master` as of 2026-05-08.

**Architecture:** Pure-data audit. No new components. The `HotkeyDef` struct, `ConfigService::hotkeyBindings()` lookup, `GenericHotkeyPage` rendering, `HotkeyBindingRow` widget, `HotkeySettingsDialog` chrome, and per-emulator `formatBinding()` are all unchanged. The work is exclusively replacing the body of three `hotkeyBindingDefs()` functions and adding a single new Qt::Test executable (`test_hotkey_defs`) with three independent slots that regression-guard the lists.

**Tech Stack:** C++17, Qt6 Widgets, Qt::Test, CMake.

**Spec:** `docs/superpowers/specs/2026-05-08-hotkey-defs-upstream-audit-design.md` (commit `c0f1631`).

---

## File Map

**New files:**
- `cpp/tests/test_hotkey_defs.cpp` — Qt::Test executable with three slots (`pcsx2_completeness`, `duckstation_completeness`, `ppsspp_completeness`) that assert entry counts, trim-list absences, sample present rows, and key→group assignments. Slots are added one-per-task as each adapter migrates.

**Files modified:**
- `cpp/src/adapters/pcsx2_adapter.cpp:1515-1565` — replace 38-entry `hotkeyBindingDefs()` body with the 60-entry list in Task 1.
- `cpp/src/adapters/duckstation_adapter.cpp:1508-1571` — replace 50-entry body with the 102-entry list in Task 2.
- `cpp/src/adapters/ppsspp_adapter.cpp:1045-1064` — replace 13-entry body with the 25-entry list in Task 3.
- `cpp/CMakeLists.txt` — add `add_executable(test_hotkey_defs ...)` block in Task 1 (alongside the other hotkey tests around line 692).
- `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md` — index entry in Task 5.

**Files unchanged (verify these are NOT touched):**
- `cpp/src/core/binding_def.h` (`HotkeyDef`).
- `cpp/src/services/config_service.{h,cpp}` (hotkey persistence).
- `cpp/src/ui/settings/generic_hotkey_page.{h,cpp}`, `cpp/src/ui/settings/hotkey_settings_dialog.{h,cpp}`, `cpp/src/ui/settings/widgets/hotkey_binding_row.{h,cpp}`.
- All adapter headers (`pcsx2_adapter.h`, `duckstation_adapter.h`, `ppsspp_adapter.h`).
- `dolphin_adapter.{h,cpp}` (intentionally returns `{}`).

**Files created (memory, outside repo):**
- `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/hotkey-defs-upstream-aligned.md` — project-type memory entry in Task 5.

---

## Cross-Task Conventions

- **Build:** `cmake --build cpp/build` (incremental). The CMake configure step only needs to re-run when `CMakeLists.txt` is edited (Task 1).
- **Run a single hotkey-defs test:** `cd cpp/build && ctest --output-on-failure -R HotkeyDefs` (this runs all three slots once the test exists).
- **Run a single slot directly:** `./cpp/build/test_hotkey_defs -v2 pcsx2_completeness`
- **Full sweep before commit:** `cd cpp/build && ctest --output-on-failure` — expect 100% pass.
- **Manual smoke** (Task 4): `open ./cpp/build/RetroNest.app`, launch a game on the relevant emulator, in-game menu → Hotkey Settings.
- **Commit style** (matches `git log`): lowercase scope prefix, em-dash separator, brief imperative summary. Examples: `pcsx2: align hotkey defs with upstream`, `duckstation: align hotkey defs with upstream`, `hotkey: regression test for adapter hotkey lists`.
- **Commit grouping rule:** test additions and adapter migrations land in the same commit per emulator (test-first within the commit, code change after). This keeps every commit green: the test is added at the same time as the implementation that satisfies it.

---

## Task 1: PCSX2 — migrate hotkey defs + add test_hotkey_defs scaffold

**Why first:** PCSX2 is the lowest-risk adapter (smallest list of net adds at +22; existing key names already match upstream). It's also where the new test executable is born — so wiring CMake happens once here and Tasks 2-3 only append slots to the existing file.

**Files:**
- Create: `cpp/tests/test_hotkey_defs.cpp`
- Modify: `cpp/CMakeLists.txt` (one new `add_executable` block + `add_test`)
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp:1515-1565`

- [ ] **Step 1.1: Read existing test patterns to confirm fixture style**

Run: `cat cpp/tests/test_hotkey_binding_row.cpp | head -30`
Note: `QtTest` + `QApplication` is *not* needed for a non-GUI test — schema tests use bare `QtTest` + `QObject`. We follow the schema-test pattern since this test only constructs adapters and reads a `QVector<HotkeyDef>`. `chdr-static` link is not required for the lighter shape we use here.

- [ ] **Step 1.2: Write the failing test file with the PCSX2 slot**

Create `cpp/tests/test_hotkey_defs.cpp`:

```cpp
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
#include "adapters/pcsx2_adapter.h"
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
    void pcsx2_completeness() {
        PCSX2Adapter adapter;
        const auto defs = adapter.hotkeyBindingDefs();

        QCOMPARE(defs.size(), 60);

        const QStringList keys = keysOf(defs);
        // Overlay-conflict trim list — these MUST NOT appear.
        QVERIFY(!keys.contains("ToggleFullscreen"));
        QVERIFY(!keys.contains("OpenPauseMenu"));
        QVERIFY(!keys.contains("TogglePause"));
        QVERIFY(!keys.contains("ShutdownVM"));

        // Sample present rows (covers Navigation / Speed / System / Save States / Audio / Graphics).
        QVERIFY(keys.contains("OpenAchievementsList"));    // Navigation (newly added)
        QVERIFY(keys.contains("FrameAdvance"));            // Speed
        QVERIFY(keys.contains("ToggleMouseLock"));         // System (newly added)
        QVERIFY(keys.contains("LoadStateFromSlot1"));      // Save States
        QVERIFY(keys.contains("Mute"));                    // Audio
        QVERIFY(keys.contains("Screenshot"));              // Graphics (newly added)
        QVERIFY(keys.contains("ToggleSoftwareRendering")); // Graphics (newly added)

        // Group renames / placements.
        const HotkeyDef* frameAdvance = findKey(defs, "FrameAdvance");
        QVERIFY(frameAdvance);
        QCOMPARE(frameAdvance->group, QStringLiteral("Speed"));  // was "Speed Control"

        const HotkeyDef* screenshot = findKey(defs, "Screenshot");
        QVERIFY(screenshot);
        QCOMPARE(screenshot->group, QStringLiteral("Graphics"));

        const HotkeyDef* openAchievements = findKey(defs, "OpenAchievementsList");
        QVERIFY(openAchievements);
        QCOMPARE(openAchievements->group, QStringLiteral("Navigation"));
    }

    // duckstation_completeness — added in Task 2.
    // ppsspp_completeness — added in Task 3.
};

QTEST_MAIN(TestHotkeyDefs)
#include "test_hotkey_defs.moc"
```

- [ ] **Step 1.3: Add the CMakeLists entry**

Edit `cpp/CMakeLists.txt`. Locate the existing `add_executable(test_hotkey_binding_row …)` block (around line 692) and add this new block immediately *after* the closing `add_test(NAME HotkeyBindingRow …)` line:

```cmake
add_executable(test_hotkey_defs
    tests/test_hotkey_defs.cpp
    src/adapters/pcsx2_adapter.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_hotkey_defs PROPERTIES AUTOMOC ON)
target_include_directories(test_hotkey_defs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_hotkey_defs PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME HotkeyDefs COMMAND test_hotkey_defs)
```

(Source-file list mirrors the schema tests — every adapter pulls in `emulator_adapter.cpp`, `ini_file.cpp`, `iso9660_reader.cpp`, `sfo_parser.cpp`, `paths.cpp`. `chdr-static` is needed because PCSX2/DuckStation adapters reference it transitively.)

- [ ] **Step 1.4: Reconfigure + build the test target**

Run:
```bash
cmake -B cpp/build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" cpp \
  && cmake --build cpp/build --target test_hotkey_defs 2>&1 | tail -20
```
Expected: clean build of `test_hotkey_defs`. If linking fails with missing symbols, double-check the source-file list above — typically `iso9660_reader.cpp` or `sfo_parser.cpp` is the culprit.

- [ ] **Step 1.5: Run the test and verify it FAILS**

Run:
```bash
./cpp/build/test_hotkey_defs -v2
```
Expected: `pcsx2_completeness` fails on the very first `QCOMPARE(defs.size(), 60)` — actual count is `38`. This is the failing-test signal that proves the test is wired correctly before we fix the adapter.

- [ ] **Step 1.6: Replace `PCSX2Adapter::hotkeyBindingDefs()` body**

Edit `cpp/src/adapters/pcsx2_adapter.cpp`. Replace the existing function body (lines 1515-1565, the `return { … };` block) with this 60-entry list (preserving the function signature line `QVector<HotkeyDef> PCSX2Adapter::hotkeyBindingDefs() const {` and closing `}`):

```cpp
QVector<HotkeyDef> PCSX2Adapter::hotkeyBindingDefs() const {
    return {
        // ── Navigation ──
        {"Open Achievements List",      "Navigation",  "Hotkeys", "OpenAchievementsList",       ""},
        {"Open Leaderboards List",      "Navigation",  "Hotkeys", "OpenLeaderboardsList",       ""},

        // ── Speed ──
        {"Frame Advance",               "Speed",       "Hotkeys", "FrameAdvance",               ""},
        {"Toggle Frame Limit",          "Speed",       "Hotkeys", "ToggleFrameLimit",           ""},
        {"Toggle Turbo / Fast Forward", "Speed",       "Hotkeys", "ToggleTurbo",                "Keyboard/Period"},
        {"Turbo / Fast Forward (Hold)", "Speed",       "Hotkeys", "HoldTurbo",                  ""},
        {"Toggle Slow Motion",          "Speed",       "Hotkeys", "ToggleSlowMotion",           "Keyboard/Shift & Keyboard/Backspace"},
        {"Increase Target Speed",       "Speed",       "Hotkeys", "IncreaseSpeed",              ""},
        {"Decrease Target Speed",       "Speed",       "Hotkeys", "DecreaseSpeed",              ""},

        // ── System ──
        {"Reset Virtual Machine",       "System",      "Hotkeys", "ResetVM",                    ""},
        {"Reload Patches",              "System",      "Hotkeys", "ReloadPatches",              ""},
        {"Swap Memory Cards",           "System",      "Hotkeys", "SwapMemCards",               ""},
        {"Toggle Input Recording Mode", "System",      "Hotkeys", "InputRecToggleMode",         ""},
        {"Toggle Mouse Lock",           "System",      "Hotkeys", "ToggleMouseLock",            ""},

        // ── Save States ──
        {"Select Previous Save Slot",   "Save States", "Hotkeys", "PreviousSaveStateSlot",      "Keyboard/Shift & Keyboard/F2"},
        {"Select Next Save Slot",       "Save States", "Hotkeys", "NextSaveStateSlot",          "Keyboard/F2"},
        {"Save State To Selected Slot", "Save States", "Hotkeys", "SaveStateToSlot",            "Keyboard/F1"},
        {"Load State From Selected Slot","Save States","Hotkeys", "LoadStateFromSlot",          "Keyboard/F3"},
        {"Load Backup State From Selected Slot","Save States","Hotkeys","LoadBackupStateFromSlot",""},
        {"Save State and Select Next Slot","Save States","Hotkeys","SaveStateAndSelectNextSlot",""},
        {"Select Next Slot and Save State","Save States","Hotkeys","SelectNextSlotAndSaveState",""},
        {"Save State To Slot 1",        "Save States", "Hotkeys", "SaveStateToSlot1",           ""},
        {"Load State From Slot 1",      "Save States", "Hotkeys", "LoadStateFromSlot1",         ""},
        {"Save State To Slot 2",        "Save States", "Hotkeys", "SaveStateToSlot2",           ""},
        {"Load State From Slot 2",      "Save States", "Hotkeys", "LoadStateFromSlot2",         ""},
        {"Save State To Slot 3",        "Save States", "Hotkeys", "SaveStateToSlot3",           ""},
        {"Load State From Slot 3",      "Save States", "Hotkeys", "LoadStateFromSlot3",         ""},
        {"Save State To Slot 4",        "Save States", "Hotkeys", "SaveStateToSlot4",           ""},
        {"Load State From Slot 4",      "Save States", "Hotkeys", "LoadStateFromSlot4",         ""},
        {"Save State To Slot 5",        "Save States", "Hotkeys", "SaveStateToSlot5",           ""},
        {"Load State From Slot 5",      "Save States", "Hotkeys", "LoadStateFromSlot5",         ""},
        {"Save State To Slot 6",        "Save States", "Hotkeys", "SaveStateToSlot6",           ""},
        {"Load State From Slot 6",      "Save States", "Hotkeys", "LoadStateFromSlot6",         ""},
        {"Save State To Slot 7",        "Save States", "Hotkeys", "SaveStateToSlot7",           ""},
        {"Load State From Slot 7",      "Save States", "Hotkeys", "LoadStateFromSlot7",         ""},
        {"Save State To Slot 8",        "Save States", "Hotkeys", "SaveStateToSlot8",           ""},
        {"Load State From Slot 8",      "Save States", "Hotkeys", "LoadStateFromSlot8",         ""},
        {"Save State To Slot 9",        "Save States", "Hotkeys", "SaveStateToSlot9",           ""},
        {"Load State From Slot 9",      "Save States", "Hotkeys", "LoadStateFromSlot9",         ""},
        {"Save State To Slot 10",       "Save States", "Hotkeys", "SaveStateToSlot10",          ""},
        {"Load State From Slot 10",     "Save States", "Hotkeys", "LoadStateFromSlot10",        ""},

        // ── Audio ──
        {"Toggle Mute",                 "Audio",       "Hotkeys", "Mute",                       ""},
        {"Increase Volume",             "Audio",       "Hotkeys", "IncreaseVolume",             ""},
        {"Decrease Volume",             "Audio",       "Hotkeys", "DecreaseVolume",             ""},

        // ── Graphics ──
        {"Save Screenshot",             "Graphics",    "Hotkeys", "Screenshot",                 ""},
        {"Toggle Video Capture",        "Graphics",    "Hotkeys", "ToggleVideoCapture",         ""},
        {"Save Single Frame GS Dump",   "Graphics",    "Hotkeys", "GSDumpSingleFrame",          ""},
        {"Save Multi Frame GS Dump",    "Graphics",    "Hotkeys", "GSDumpMultiFrame",           ""},
        {"Toggle Software Rendering",   "Graphics",    "Hotkeys", "ToggleSoftwareRendering",    ""},
        {"Increase Upscale Multiplier", "Graphics",    "Hotkeys", "IncreaseUpscaleMultiplier",  ""},
        {"Decrease Upscale Multiplier", "Graphics",    "Hotkeys", "DecreaseUpscaleMultiplier",  ""},
        {"Toggle On-Screen Display",    "Graphics",    "Hotkeys", "ToggleOSD",                  ""},
        {"Cycle Aspect Ratio",          "Graphics",    "Hotkeys", "CycleAspectRatio",           ""},
        {"Toggle Hardware Mipmapping",  "Graphics",    "Hotkeys", "ToggleMipmapMode",           ""},
        {"Cycle Deinterlace Mode",      "Graphics",    "Hotkeys", "CycleInterlaceMode",         ""},
        {"Cycle TV Shader",             "Graphics",    "Hotkeys", "CycleTVShader",              ""},
        {"Cycle Blending Accuracy",     "Graphics",    "Hotkeys", "CycleBlendingAccuracy",      ""},
        {"Toggle Texture Dumping",      "Graphics",    "Hotkeys", "ToggleTextureDumping",       ""},
        {"Toggle Texture Replacements", "Graphics",    "Hotkeys", "ToggleTextureReplacements",  ""},
        {"Reload Texture Replacements", "Graphics",    "Hotkeys", "ReloadTextureReplacements",  ""},
    };
}
```

Count check (do this mentally before proceeding): 2 Navigation + 7 Speed + 5 System + 27 Save States + 3 Audio + 16 Graphics = **60**. ✓

- [ ] **Step 1.7: Build + run the test, verify it PASSES**

Run:
```bash
cmake --build cpp/build --target test_hotkey_defs && ./cpp/build/test_hotkey_defs -v2
```
Expected: `pcsx2_completeness` PASSES. The other slot stubs don't exist yet, so this is the only assertion run.

- [ ] **Step 1.8: Run the full test sweep — verify no regressions**

Run:
```bash
cd cpp/build && ctest --output-on-failure 2>&1 | tail -10
```
Expected: 100% pass, including the new `HotkeyDefs` test and all 37 existing tests.

- [ ] **Step 1.9: Commit**

```bash
git add cpp/tests/test_hotkey_defs.cpp \
        cpp/CMakeLists.txt \
        cpp/src/adapters/pcsx2_adapter.cpp && \
git commit -m "$(cat <<'EOF'
pcsx2: align hotkey defs with upstream

38-entry list grows to 60. Adopt upstream PCSX2 source order,
group names verbatim ("Speed" not "Speed Control"), and category
membership for every entry from pcsx2/Hotkeys.cpp + pcsx2/GS/GS.cpp.
Drop the 4 trim-list entries (overlay-conflict): ToggleFullscreen,
OpenPauseMenu, TogglePause, ShutdownVM. Add the entire 16-entry
Graphics group (Screenshot, ToggleSoftwareRendering, etc.) plus
OpenAchievementsList / OpenLeaderboardsList / InputRecToggleMode /
ToggleMouseLock. Save state slot pairs interleave Save/Load.
Existing F-key defaults retained as documented divergence.

New test_hotkey_defs executable regression-guards the list.

Spec: docs/superpowers/specs/2026-05-08-hotkey-defs-upstream-audit-design.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: DuckStation — migrate hotkey defs

**Why this task:** DuckStation has the largest delta (+53 net adds, including the 14-entry Free Camera group, 20 Global save state slots, and 3 Debugging entries). Names match upstream exactly so no migration risk. The PowerOff trim is the only behavior-relevant removal — users who had it bound see it disappear from the UI.

**Files:**
- Modify: `cpp/tests/test_hotkey_defs.cpp` (append `duckstation_completeness` slot)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp:1508-1571`

- [ ] **Step 2.1: Append the failing DuckStation test slot**

Edit `cpp/tests/test_hotkey_defs.cpp`. Replace the placeholder comment `// duckstation_completeness — added in Task 2.` with this slot body:

```cpp
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

        // Default value preserved on FastForward.
        const HotkeyDef* fastForward = findKey(defs, "FastForward");
        QVERIFY(fastForward);
        QCOMPARE(fastForward->defaultValue, QStringLiteral("Keyboard/Tab"));
    }
```

- [ ] **Step 2.2: Build + run, verify it FAILS**

Run:
```bash
cmake --build cpp/build --target test_hotkey_defs && ./cpp/build/test_hotkey_defs -v2
```
Expected: `duckstation_completeness` fails on `QCOMPARE(defs.size(), 102)` — actual count is `50`. (`pcsx2_completeness` still passes.)

- [ ] **Step 2.3: Replace `DuckStationAdapter::hotkeyBindingDefs()` body**

Edit `cpp/src/adapters/duckstation_adapter.cpp`. Replace the existing function body (lines 1508-1571) with this 102-entry list:

```cpp
QVector<HotkeyDef> DuckStationAdapter::hotkeyBindingDefs() const {
    return {
        // ── Interface ──
        {"Open Cheat Settings",        "Interface",   "Hotkeys", "OpenCheatsMenu",            ""},
        {"Open Achievement List",      "Interface",   "Hotkeys", "OpenAchievements",          ""},
        {"Open Leaderboard List",      "Interface",   "Hotkeys", "OpenLeaderboards",          ""},
        {"Save Screenshot",            "Interface",   "Hotkeys", "Screenshot",                "Keyboard/F10"},

        // ── System ──
        {"Fast Forward (Hold)",        "System",      "Hotkeys", "FastForward",               "Keyboard/Tab"},
        {"Fast Forward (Toggle)",      "System",      "Hotkeys", "ToggleFastForward",         ""},
        {"Turbo (Hold)",               "System",      "Hotkeys", "Turbo",                     ""},
        {"Turbo (Toggle)",             "System",      "Hotkeys", "ToggleTurbo",               ""},
        {"Restart Game",               "System",      "Hotkeys", "Reset",                     ""},
        {"Change Disc",                "System",      "Hotkeys", "ChangeDisc",                ""},
        {"Switch to Previous Disc",    "System",      "Hotkeys", "SwitchToPreviousDisc",      ""},
        {"Switch to Next Disc",        "System",      "Hotkeys", "SwitchToNextDisc",          ""},
        {"Rewind",                     "System",      "Hotkeys", "Rewind",                    ""},
        {"Frame Step",                 "System",      "Hotkeys", "FrameStep",                 ""},
        {"Toggle Media Capture",       "System",      "Hotkeys", "ToggleMediaCapture",        ""},
        {"Swap Memory Card Slots",     "System",      "Hotkeys", "SwapMemoryCards",           ""},
        {"Toggle Clock Speed Control (Overclocking)","System","Hotkeys","ToggleOverclocking", ""},
        {"Increase Emulation Speed",   "System",      "Hotkeys", "IncreaseEmulationSpeed",    ""},
        {"Decrease Emulation Speed",   "System",      "Hotkeys", "DecreaseEmulationSpeed",    ""},
        {"Reset Emulation Speed",      "System",      "Hotkeys", "ResetEmulationSpeed",       ""},

        // ── Graphics ──
        {"Rotate Display Clockwise",   "Graphics",    "Hotkeys", "RotateClockwise",           ""},
        {"Rotate Display Counterclockwise","Graphics","Hotkeys", "RotateCounterclockwise",    ""},
        {"Toggle On-Screen Display",   "Graphics",    "Hotkeys", "ToggleOSD",                 ""},
        {"Toggle Software Rendering",  "Graphics",    "Hotkeys", "ToggleSoftwareRendering",   ""},
        {"Toggle PGXP",                "Graphics",    "Hotkeys", "TogglePGXP",                ""},
        {"Toggle PGXP Depth Buffer",   "Graphics",    "Hotkeys", "TogglePGXPDepth",           ""},
        {"Toggle Widescreen",          "Graphics",    "Hotkeys", "ToggleWidescreen",          ""},
        {"Toggle Texture Modulation Cropping","Graphics","Hotkeys","ToggleModulationCrop",    ""},
        {"Toggle Post-Processing",     "Graphics",    "Hotkeys", "TogglePostProcessing",      ""},
        {"Reload Post Processing Shaders","Graphics", "Hotkeys", "ReloadPostProcessingShaders",""},
        {"Reload Texture Replacements","Graphics",    "Hotkeys", "ReloadTextureReplacements", ""},
        {"Increase Resolution Scale",  "Graphics",    "Hotkeys", "IncreaseResolutionScale",   ""},
        {"Decrease Resolution Scale",  "Graphics",    "Hotkeys", "DecreaseResolutionScale",   ""},
        {"Record Single Frame GPU Trace","Graphics",  "Hotkeys", "RecordSingleFrameGPUDump",  ""},
        {"Record Multi-Frame GPU Trace","Graphics",   "Hotkeys", "RecordMultiFrameGPUDump",   ""},

        // ── Free Camera ──
        {"Freecam Toggle",             "Free Camera", "Hotkeys", "FreecamToggle",             ""},
        {"Freecam Reset",              "Free Camera", "Hotkeys", "FreecamReset",              ""},
        {"Freecam Move Left",          "Free Camera", "Hotkeys", "FreecamMoveLeft",           ""},
        {"Freecam Move Right",         "Free Camera", "Hotkeys", "FreecamMoveRight",          ""},
        {"Freecam Move Up",            "Free Camera", "Hotkeys", "FreecamMoveUp",             ""},
        {"Freecam Move Down",          "Free Camera", "Hotkeys", "FreecamMoveDown",           ""},
        {"Freecam Move Forward",       "Free Camera", "Hotkeys", "FreecamMoveForward",        ""},
        {"Freecam Move Backward",      "Free Camera", "Hotkeys", "FreecamMoveBackward",       ""},
        {"Freecam Rotate Left",        "Free Camera", "Hotkeys", "FreecamRotateLeft",         ""},
        {"Freecam Rotate Right",       "Free Camera", "Hotkeys", "FreecamRotateRight",        ""},
        {"Freecam Rotate Forward",     "Free Camera", "Hotkeys", "FreecamRotateForward",      ""},
        {"Freecam Rotate Backward",    "Free Camera", "Hotkeys", "FreecamRotateBackward",     ""},
        {"Freecam Roll Left",          "Free Camera", "Hotkeys", "FreecamRollLeft",           ""},
        {"Freecam Roll Right",         "Free Camera", "Hotkeys", "FreecamRollRight",          ""},

        // ── Audio ──
        {"Toggle Mute",                "Audio",       "Hotkeys", "AudioMute",                 ""},
        {"Toggle CD Audio Mute",       "Audio",       "Hotkeys", "AudioCDAudioMute",          ""},
        {"Volume Up",                  "Audio",       "Hotkeys", "AudioVolumeUp",             ""},
        {"Volume Down",                "Audio",       "Hotkeys", "AudioVolumeDown",           ""},

        // ── Save States (selected/undo) ──
        {"Load From Selected Slot",    "Save States", "Hotkeys", "LoadSelectedSaveState",     "Keyboard/F1"},
        {"Save To Selected Slot",      "Save States", "Hotkeys", "SaveSelectedSaveState",     "Keyboard/F2"},
        {"Select Previous Save Slot",  "Save States", "Hotkeys", "SelectPreviousSaveStateSlot","Keyboard/F3"},
        {"Select Next Save Slot",      "Save States", "Hotkeys", "SelectNextSaveStateSlot",   "Keyboard/F4"},
        {"Save State and Select Next Slot","Save States","Hotkeys","SaveStateAndSelectNextSlot",""},
        {"Undo Load State",            "Save States", "Hotkeys", "UndoLoadState",             ""},

        // ── Debugging ──
        {"Toggle PGXP CPU Mode",       "Debugging",   "Hotkeys", "TogglePGXPCPU",             ""},
        {"Toggle PGXP Preserve Projection Precision","Debugging","Hotkeys","TogglePGXPPreserveProjPrecision",""},
        {"Toggle VRAM View",           "Debugging",   "Hotkeys", "ToggleVRAMView",            ""},

        // ── Save States (per-slot Game) ──
        {"Load Game State 1",          "Save States", "Hotkeys", "LoadGameState1",            ""},
        {"Save Game State 1",          "Save States", "Hotkeys", "SaveGameState1",            ""},
        {"Load Game State 2",          "Save States", "Hotkeys", "LoadGameState2",            ""},
        {"Save Game State 2",          "Save States", "Hotkeys", "SaveGameState2",            ""},
        {"Load Game State 3",          "Save States", "Hotkeys", "LoadGameState3",            ""},
        {"Save Game State 3",          "Save States", "Hotkeys", "SaveGameState3",            ""},
        {"Load Game State 4",          "Save States", "Hotkeys", "LoadGameState4",            ""},
        {"Save Game State 4",          "Save States", "Hotkeys", "SaveGameState4",            ""},
        {"Load Game State 5",          "Save States", "Hotkeys", "LoadGameState5",            ""},
        {"Save Game State 5",          "Save States", "Hotkeys", "SaveGameState5",            ""},
        {"Load Game State 6",          "Save States", "Hotkeys", "LoadGameState6",            ""},
        {"Save Game State 6",          "Save States", "Hotkeys", "SaveGameState6",            ""},
        {"Load Game State 7",          "Save States", "Hotkeys", "LoadGameState7",            ""},
        {"Save Game State 7",          "Save States", "Hotkeys", "SaveGameState7",            ""},
        {"Load Game State 8",          "Save States", "Hotkeys", "LoadGameState8",            ""},
        {"Save Game State 8",          "Save States", "Hotkeys", "SaveGameState8",            ""},
        {"Load Game State 9",          "Save States", "Hotkeys", "LoadGameState9",            ""},
        {"Save Game State 9",          "Save States", "Hotkeys", "SaveGameState9",            ""},
        {"Load Game State 10",         "Save States", "Hotkeys", "LoadGameState10",           ""},
        {"Save Game State 10",         "Save States", "Hotkeys", "SaveGameState10",           ""},

        // ── Save States (per-slot Global) ──
        {"Load Global State 1",        "Save States", "Hotkeys", "LoadGlobalState1",          ""},
        {"Save Global State 1",        "Save States", "Hotkeys", "SaveGlobalState1",          ""},
        {"Load Global State 2",        "Save States", "Hotkeys", "LoadGlobalState2",          ""},
        {"Save Global State 2",        "Save States", "Hotkeys", "SaveGlobalState2",          ""},
        {"Load Global State 3",        "Save States", "Hotkeys", "LoadGlobalState3",          ""},
        {"Save Global State 3",        "Save States", "Hotkeys", "SaveGlobalState3",          ""},
        {"Load Global State 4",        "Save States", "Hotkeys", "LoadGlobalState4",          ""},
        {"Save Global State 4",        "Save States", "Hotkeys", "SaveGlobalState4",          ""},
        {"Load Global State 5",        "Save States", "Hotkeys", "LoadGlobalState5",          ""},
        {"Save Global State 5",        "Save States", "Hotkeys", "SaveGlobalState5",          ""},
        {"Load Global State 6",        "Save States", "Hotkeys", "LoadGlobalState6",          ""},
        {"Save Global State 6",        "Save States", "Hotkeys", "SaveGlobalState6",          ""},
        {"Load Global State 7",        "Save States", "Hotkeys", "LoadGlobalState7",          ""},
        {"Save Global State 7",        "Save States", "Hotkeys", "SaveGlobalState7",          ""},
        {"Load Global State 8",        "Save States", "Hotkeys", "LoadGlobalState8",          ""},
        {"Save Global State 8",        "Save States", "Hotkeys", "SaveGlobalState8",          ""},
        {"Load Global State 9",        "Save States", "Hotkeys", "LoadGlobalState9",          ""},
        {"Save Global State 9",        "Save States", "Hotkeys", "SaveGlobalState9",          ""},
        {"Load Global State 10",       "Save States", "Hotkeys", "LoadGlobalState10",         ""},
        {"Save Global State 10",       "Save States", "Hotkeys", "SaveGlobalState10",         ""},
    };
}
```

Count check: 4 Interface + 16 System + 15 Graphics + 14 Free Camera + 4 Audio + 6 Save States (selected/undo) + 3 Debugging + 20 Game State + 20 Global State = **102**. ✓

> If your walk of `s_hotkey_list[]` produces a count other than 102, prefer upstream's count and adjust the test's `QCOMPARE(defs.size(), …)` line accordingly. The rule is verbatim alignment, not the integer.

- [ ] **Step 2.4: Build + run the test, verify it PASSES**

Run:
```bash
cmake --build cpp/build --target test_hotkey_defs && ./cpp/build/test_hotkey_defs -v2
```
Expected: both `pcsx2_completeness` and `duckstation_completeness` PASS.

- [ ] **Step 2.5: Run the full sweep**

Run:
```bash
cd cpp/build && ctest --output-on-failure 2>&1 | tail -10
```
Expected: 100% pass.

- [ ] **Step 2.6: Commit**

```bash
git add cpp/tests/test_hotkey_defs.cpp \
        cpp/src/adapters/duckstation_adapter.cpp && \
git commit -m "$(cat <<'EOF'
duckstation: align hotkey defs with upstream

50-entry list grows to 102. Adopt upstream src/core/hotkeys.cpp
source order, group names verbatim, and the full 7-group structure:
Interface · System · Graphics · Free Camera · Audio · Save States ·
Debugging. Adds entire 14-entry Free Camera group, 20 Global save
state slots, 3 Debugging entries, and ~16 missing Graphics/System/
Interface entries (OpenCheatsMenu, OpenAchievements, FreecamToggle,
ToggleVRAMView, ToggleMediaCapture, ToggleOverclocking, etc.).
13 display labels refresh to upstream verbatim ("Mute Audio" →
"Toggle Mute", "Reset" → "Restart Game", "Increase Resolution" →
"Increase Resolution Scale"). Upstream defaults preserved
(F10 Screenshot, Tab FastForward, F1-F4 save state nav).

Drop PowerOff per overlay-conflict trim policy (overlay owns
shutdown via SIGTERM). Drop OpenPauseMenu, TogglePause,
ToggleFullscreen for the same reason.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: PPSSPP — migrate hotkey defs

**Why this task:** PPSSPP is the smallest delta in row count but introduces the most visible UX change — collapsing our 3-group view (Speed / System / Save States) into upstream's 2-group view (Control modifiers / Emulator controls). Save State and Load State move from a dedicated "Save States" header into "Emulator controls", matching upstream's actual UI. Six new modifier-toggle hotkeys (RapidFire, Analog limiter, Axis swap, etc.) become user-rebindable for the first time.

**Files:**
- Modify: `cpp/tests/test_hotkey_defs.cpp` (append `ppsspp_completeness` slot)
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp:1045-1064`

- [ ] **Step 3.1: Append the failing PPSSPP test slot**

Edit `cpp/tests/test_hotkey_defs.cpp`. Replace the placeholder comment `// ppsspp_completeness — added in Task 3.` with this slot body:

```cpp
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
        QVERIFY(keys.contains("RapidFire"));         // Control modifiers (newly added)
        QVERIFY(keys.contains("Analog limiter"));    // Control modifiers (newly added)
        QVERIFY(keys.contains("Fast-forward"));      // Emulator controls
        QVERIFY(keys.contains("Save State"));        // Emulator controls (regrouped from Save States)
        QVERIFY(keys.contains("Display Portrait"));  // Emulator controls (newly added)
        QVERIFY(keys.contains("Toggle tilt control"));// Emulator controls (newly added)

        // Group renames / placements.
        const HotkeyDef* saveState = findKey(defs, "Save State");
        QVERIFY(saveState);
        QCOMPARE(saveState->group, QStringLiteral("Emulator controls"));  // was "Save States"

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
```

- [ ] **Step 3.2: Build + run, verify it FAILS**

Run:
```bash
cmake --build cpp/build --target test_hotkey_defs && ./cpp/build/test_hotkey_defs -v2
```
Expected: `ppsspp_completeness` fails on `QCOMPARE(defs.size(), 25)` — actual count is `13`. Other slots still pass.

- [ ] **Step 3.3: Replace `PPSSPPAdapter::hotkeyBindingDefs()` body**

Edit `cpp/src/adapters/ppsspp_adapter.cpp`. Replace the existing function body (lines 1045-1064) with this 25-entry list:

```cpp
QVector<HotkeyDef> PPSSPPAdapter::hotkeyBindingDefs() const {
    return {
        // ── Control modifiers ──
        {"Rotate Analog (CW)",       "Control modifiers", "ControlMapping", "Rotate Analog (CW)",  ""},
        {"Rotate Analog (CCW)",      "Control modifiers", "ControlMapping", "Rotate Analog (CCW)", ""},
        {"Analog limiter",           "Control modifiers", "ControlMapping", "Analog limiter",      ""},
        {"RapidFire",                "Control modifiers", "ControlMapping", "RapidFire",           ""},
        {"Axis swap (hold)",         "Control modifiers", "ControlMapping", "Axis swap (hold)",    ""},
        {"Axis swap (toggle)",       "Control modifiers", "ControlMapping", "Axis swap (toggle)",  ""},

        // ── Emulator controls ──
        {"Fast-forward",             "Emulator controls", "ControlMapping", "Fast-forward",                "10-4036"},
        {"SpeedToggle",              "Emulator controls", "ControlMapping", "SpeedToggle",                 ""},
        {"Alt speed 1",              "Emulator controls", "ControlMapping", "Alt speed 1",                 ""},
        {"Alt speed 2",              "Emulator controls", "ControlMapping", "Alt speed 2",                 ""},
        {"Analog speed",             "Emulator controls", "ControlMapping", "Analog speed",                ""},
        {"Reset",                    "Emulator controls", "ControlMapping", "Reset",                       ""},
        {"Frame Advance",            "Emulator controls", "ControlMapping", "Frame Advance",               ""},
        {"Rewind",                   "Emulator controls", "ControlMapping", "Rewind",                      ""},
        {"Save State",               "Emulator controls", "ControlMapping", "Save State",                  ""},
        {"Load State",               "Emulator controls", "ControlMapping", "Load State",                  ""},
        {"Previous Slot",            "Emulator controls", "ControlMapping", "Previous Slot",               ""},
        {"Next Slot",                "Emulator controls", "ControlMapping", "Next Slot",                   ""},
        {"Toggle tilt control",      "Emulator controls", "ControlMapping", "Toggle tilt control",         ""},
        {"Display Portrait",         "Emulator controls", "ControlMapping", "Display Portrait",            ""},
        {"Display Portrait Reversed","Emulator controls", "ControlMapping", "Display Portrait Reversed",   ""},
        {"Display Landscape",        "Emulator controls", "ControlMapping", "Display Landscape",           ""},
        {"Display Landscape Reversed","Emulator controls","ControlMapping", "Display Landscape Reversed",  ""},
        {"Screenshot",               "Emulator controls", "ControlMapping", "Screenshot",                  ""},
        {"Mute toggle",              "Emulator controls", "ControlMapping", "Mute toggle",                 ""},
    };
}
```

Count check: 6 Control modifiers + 19 Emulator controls = **25**. ✓

> The order shown above derives from `KeyMap.h` enum order. If your walk of `psp_button_names[]` (in `Common/KeyMap.cpp`) produces a different order, prefer the array order — `cats[]` in `UI/ControlMappingScreen.cpp` matches against array order, not enum order. Total count is invariant at 25; only row positions may shift.

- [ ] **Step 3.4: Build + run the test, verify it PASSES**

Run:
```bash
cmake --build cpp/build --target test_hotkey_defs && ./cpp/build/test_hotkey_defs -v2
```
Expected: all three slots PASS.

- [ ] **Step 3.5: Run the full sweep**

Run:
```bash
cd cpp/build && ctest --output-on-failure 2>&1 | tail -10
```
Expected: 100% pass — 38 tests total now (the original 37 plus `HotkeyDefs`).

- [ ] **Step 3.6: Commit**

```bash
git add cpp/tests/test_hotkey_defs.cpp \
        cpp/src/adapters/ppsspp_adapter.cpp && \
git commit -m "$(cat <<'EOF'
ppsspp: align hotkey defs with upstream

13-entry list grows to 25. Collapse our 3-way split (Speed / System /
Save States) into upstream's 2-group structure (Control modifiers /
Emulator controls) per Common/KeyMap.cpp psp_button_names[] and
UI/ControlMappingScreen.cpp cats[]. Save/Load State and slot rotation
move into Emulator controls — matches PPSSPP's standalone UI.

Adds 6 Control modifiers (RapidFire, Analog limiter, Rotate Analog
CW/CCW, Axis swap hold/toggle) and 6 Emulator controls (Analog speed,
Toggle tilt control, Display Portrait/Reversed, Display Landscape/
Reversed). Existing Fast-forward default (10-4036, right-trigger
axis-positive) preserved.

Drops 4 overlay-conflict entries (Pause, Pause no menu, Toggle
Fullscreen, Exit App) and 11 platform-irrelevance entries (VR camera
adjust/reset, Toggle WLAN, Toggle touch controls, OpenChat, Toggle
mouse input, DevMenu, Toggle Debugger, Texture Dumping, Texture
Replacement, Audio/Video Recording).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Manual verification

**Why this task:** unit tests verify list shape, presence/absence of keys, and group assignments — but the dialog rendering (scroll position, group ordering across the page, label wrapping for long labels like "Toggle Clock Speed Control (Overclocking)") is a UX surface only a human eye can spot-check.

**Setup:**
```bash
open /Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app
```

- [ ] **Step 4.1: PCSX2 — full visual pass**

1. Launch a PS2 game → in-game menu (Cmd+Esc or Select+Circle) → Hotkey Settings.
2. Confirm the page renders **6 group headers** in this order: `Navigation` · `Speed` · `System` · `Save States` · `Audio` · `Graphics`.
3. Spot-check rows:
   - `Navigation` group has `Open Achievements List` and `Open Leaderboards List`.
   - `Speed` group has `Frame Advance` (was previously under "Speed Control"); `Toggle Turbo / Fast Forward` shows default `Period`.
   - `System` group has `Toggle Mouse Lock` (newly added, last row of the group).
   - Save state slot rows interleave: `Save State To Slot 1`, `Load State From Slot 1`, `Save State To Slot 2`, `Load State From Slot 2`, … (not all-saves-then-all-loads).
   - `Graphics` group is fully populated — verify `Save Screenshot`, `Toggle Software Rendering`, `Cycle Aspect Ratio`, `Reload Texture Replacements` all appear.
4. Try one binding: click `Save Screenshot`, capture `F8`, confirm the description bar updates and the binding persists across dialog close/reopen.

- [ ] **Step 4.2: DuckStation — visual pass**

1. Launch a PS1 game → in-game menu → Hotkey Settings.
2. Confirm **7 group headers** appear: `Interface` · `System` · `Graphics` · `Free Camera` · `Audio` · `Save States` · `Debugging` (with `Save States` appearing again after `Debugging` for the per-slot Game/Global rows — same group header repeating is intentional, mirroring upstream interleave).
3. Spot-check:
   - `Interface` has `Open Cheat Settings` (newly added) and `Save Screenshot` (label refresh from "Screenshot").
   - `System` has `Restart Game` (label refresh from "Reset"), `Switch to Previous Disc` / `Switch to Next Disc` (label refresh).
   - `Free Camera` has all 14 entries (Toggle, Reset, 12 movement/rotation/roll directions).
   - `Audio` shows `Toggle Mute` (label refresh from "Mute Audio").
   - `Save States` (first appearance) has `Load From Selected Slot` showing default `F1`.
   - `Debugging` has `Toggle VRAM View`.
   - Per-slot section has both `Load Game State 1`-`10` and `Load Global State 1`-`10` (40 rows total).
4. **Critical:** verify `Power Off` is NOT in the list anywhere. If a user previously bound it, the on-disk value is preserved but no longer reachable via UI — that's the intended trim behavior.

- [ ] **Step 4.3: PPSSPP — visual pass**

1. Launch a PSP game → in-game menu → Hotkey Settings.
2. Confirm **only 2 group headers** appear: `Control modifiers` · `Emulator controls`. (No "Save States" header — those rows are inside `Emulator controls`.)
3. Spot-check:
   - `Control modifiers` has `RapidFire`, `Analog limiter`, `Rotate Analog (CW)`, `Rotate Analog (CCW)`, `Axis swap (hold)`, `Axis swap (toggle)` — all 6 newly added.
   - `Emulator controls` has `Fast-forward` (with default `10-4036` showing as a controller binding), `Save State`, `Load State`, `Previous Slot`, `Next Slot`, the 4 Display rotation entries, `Toggle tilt control`, etc.
   - Total visible row count is 25.
4. Try one binding: click `RapidFire`, capture a controller button, verify the saved value uses PPSSPP's numeric format (`10-{nkcode}`) and lands in `controls.ini` `[ControlMapping]`. Run:
   ```bash
   cat <root>/emulators/ppsspp/PSP/SYSTEM/controls.ini | grep -A2 '\[ControlMapping\]'
   ```
   (substitute your runtime root) — `RapidFire = 10-{some-code}` should appear in the section.

- [ ] **Step 4.4: Dolphin — confirm still hidden**

1. Open the Dolphin emulator detail page in the app shell.
2. Confirm the `Hotkeys` button is NOT visible (Dolphin returns `{}` from `hotkeyBindingDefs()`; this audit doesn't change that). Already shipped in commit `6620b6d` via `AppController::hasHotkeys()` gate — this step verifies no regression.

- [ ] **Step 4.5: Spot-fix any issues surfaced**

If any of steps 4.1-4.4 reveal a problem:
- For data issues (wrong label, wrong group, missing entry): fix the relevant adapter's `hotkeyBindingDefs()` body and re-run `./cpp/build/test_hotkey_defs -v2` plus the full sweep. Commit as a follow-up like `pcsx2: fix <description>`.
- For rendering issues (label wraps weirdly, group spacing wrong): these belong to `generic_hotkey_page.cpp` / `settings_section_header.cpp`, not the adapter list. Fix at the rendering layer.

If no issues, skip this step.

---

## Task 5: Memory entry + finalize

**Why this task:** project-memory hygiene — future sessions need to know this audit happened and where its target state is pinned.

**Files:**
- Create: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/hotkey-defs-upstream-aligned.md`
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md`

> **Note:** memory lives outside the repo, no git commit.

- [ ] **Step 5.1: Write the memory entry**

Create `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/hotkey-defs-upstream-aligned.md`:

```markdown
---
name: Hotkey defs aligned with upstream — PCSX2 / DuckStation / PPSSPP
description: hotkeyBindingDefs() in all three adapters now mirrors each emulator's upstream source verbatim — categories, group order, entry order, action keys, display labels, defaults — pinned to upstream master as of 2026-05-08.
type: project
---

PCSX2 (60 entries, 6 groups: Navigation · Speed · System · Save States · Audio · Graphics), DuckStation (102 entries, 7 groups: Interface · System · Graphics · Free Camera · Audio · Save States · Debugging), and PPSSPP (25 entries, 2 groups: Control modifiers · Emulator controls) all walk their upstream hotkey registries row by row.

**Sources of truth (pinned 2026-05-08):**
- PCSX2: `pcsx2/Hotkeys.cpp` (`g_common_hotkeys`) + `pcsx2/GS/GS.cpp` (`g_gs_hotkeys`).
- DuckStation: `src/core/hotkeys.cpp` (`s_hotkey_list[]`); defaults in `src/core/settings.cpp` `Settings::SetDefaultHotkeyConfig()`.
- PPSSPP: `Common/KeyMap.cpp` `psp_button_names[]` (controls.ini key strings) + `UI/ControlMappingScreen.cpp` `cats[]` (group boundaries).

**Trim policies:**
- *Overlay conflict* (all three): Pause variants, Quit/Power-Off variants, Toggle Fullscreen — overlay owns these via `PauseOnFocusLoss`, SIGTERM, app window. Includes DuckStation `PowerOff` (removed; was previously exposed).
- *Platform irrelevance* (PPSSPP only): VR camera adjust/reset, Toggle WLAN, Toggle touch controls, OpenChat, Toggle mouse input, DevMenu, Toggle Debugger, Texture Dumping, Texture Replacement, Audio/Video Recording.

**Documented divergence (PCSX2 only):** upstream PCSX2 ships zero default keyboard bindings in its `DEFINE_HOTKEY` macros — defaults migrate via `Pads.ini` outside the registry. We retain our existing F-key set (`Keyboard/F1` Save, `Keyboard/F2` Slot+, `Keyboard/F3` Load, `Keyboard/Period` Turbo, `Keyboard/Shift & Keyboard/F2` Slot−, `Keyboard/Shift & Keyboard/Backspace` Slow-Mo) because the wiki + community docs assume them. Strict-mirror divergence flagged in spec §4.2.

**Why:** mirrors the existing `feedback-exhaustive-settings-audit.md` rule for settings panes, applied to hotkeys. Was driven by user feedback that hotkeys "aren't in the same order or category as the standalone emulator."

**How to apply:** when a new emulator adapter is added with hotkey support, walk its upstream registry / virtkey list / DEFINE_HOTKEY macros source-order and emit one `HotkeyDef` per entry, matching upstream's `key`, `label`, and `group` fields. Apply the overlay-conflict trim. Decide platform-irrelevance trim per emulator. Add a slot to `cpp/tests/test_hotkey_defs.cpp` mirroring the existing three slots' shape.

**Spec:** `docs/superpowers/specs/2026-05-08-hotkey-defs-upstream-audit-design.md` (commit `c0f1631`).
**Plan:** `docs/superpowers/plans/2026-05-08-hotkey-defs-upstream-audit.md`.
```

- [ ] **Step 5.2: Add the index entry**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md`. Append a new line immediately after the existing `Hotkey settings redesign` entry:

```markdown
- [Hotkey defs aligned with upstream](hotkey-defs-upstream-aligned.md) — PCSX2/DuckStation/PPSSPP hotkeyBindingDefs() walk upstream registries verbatim (categories, order, labels, defaults); pinned 2026-05-08.
```

- [ ] **Step 5.3: Final test sweep**

Run:
```bash
cd cpp/build && ctest --output-on-failure 2>&1 | tail -10
```
Expected: 100% pass — 38 tests, no regressions.

- [ ] **Step 5.4: Final git status**

Run:
```bash
git log --oneline main..HEAD
```
Expected (in commit order, after Task 5):
```
<sha> ppsspp: align hotkey defs with upstream
<sha> duckstation: align hotkey defs with upstream
<sha> pcsx2: align hotkey defs with upstream
```
The spec commit `c0f1631` is already on `main`. Optionally a Task 4.5 spot-fix commit.

```bash
git status
```
Expected: working tree clean (or only the existing pre-audit untracked items).

---

## Verification Checklist

After all tasks complete, this is the spec's success criteria walked end-to-end:

- [ ] PCSX2 list = 60 entries across 6 groups in upstream source order.
- [ ] DuckStation list = 102 entries across 7 groups in upstream source order; `PowerOff` absent.
- [ ] PPSSPP list = 25 entries across 2 groups (`Control modifiers`, `Emulator controls`); save-state rows live in `Emulator controls`, not a separate `Save States` header.
- [ ] All 4 overlay-conflict trims absent across the three adapters: `(Toggle)Pause`, `OpenPauseMenu`/`ShutdownVM`/`Exit App`/`PowerOff` (per-emulator naming), `ToggleFullscreen`.
- [ ] All 11 PPSSPP platform-irrelevance trims absent.
- [ ] PCSX2 F-key defaults preserved (`F1` save, `F2` slot+, `F3` load, `Period` turbo, `Shift+F2` slot−, `Shift+Backspace` slow-mo).
- [ ] DuckStation upstream defaults present (`F10` Screenshot, `Tab` FastForward, `F1`-`F4` save state nav).
- [ ] PPSSPP `Fast-forward` default `10-4036` preserved.
- [ ] `test_hotkey_defs` is in `ctest` and passes all three slots.
- [ ] `MEMORY.md` has the new index entry; `hotkey-defs-upstream-aligned.md` exists.
- [ ] Manual smoke pass for PCSX2 / DuckStation / PPSSPP / Dolphin (Task 4) all green.
