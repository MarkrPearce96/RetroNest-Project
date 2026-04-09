# PPSSPP Graphics Settings Reorganization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize the PPSSPP settings UI into 3 sidebar entries (Graphics, Audio, Overlay), add Lens Flare Occlusion / Post-Processing Shader / Debug Overlay settings, and add a `bitmask` field to `SettingDef` so the Show FPS / Speed / Battery checkboxes can target single bits of `iShowStatusFlags`.

**Architecture:** All schema changes happen inside `PPSSPPAdapter::settingsSchema()`. The bitmask support is a small, additive change to `SettingDef` and to one branch of `EmulatorSettingsPage::buildSubcategoryContent()` (the checkbox lambda). PCSX2 and DuckStation adapters are not touched.

**Tech Stack:** C++17, Qt6 (Widgets + Test), CMake.

**Spec:** `docs/superpowers/specs/2026-04-07-ppsspp-graphics-reorganization-design.md`

---

## File Structure

**Modify:**
- `cpp/src/core/setting_def.h` — add `bitmask` field
- `cpp/src/ui/settings/emulator_settings_page.cpp` — bitmask read/write inside the existing `makeCheckbox` lambda
- `cpp/src/adapters/ppsspp_adapter.cpp` — full rewrite of `settingsSchema()`

**Create:**
- `cpp/src/core/bitmask_helpers.h` — two pure helper functions (`setBit`, `getBit`) so the bitmask logic can be unit-tested without instantiating Qt widgets
- `cpp/tests/test_bitmask_helpers.cpp` — unit tests for the helpers
- `cpp/tests/test_ppsspp_schema.cpp` — schema smoke test (categories, sub-tabs, presence of new keys)

**CMake:**
- `cpp/CMakeLists.txt` — register the two new test executables

---

## Task 1: Add bitmask helper module (TDD)

**Files:**
- Create: `cpp/src/core/bitmask_helpers.h`
- Test: `cpp/tests/test_bitmask_helpers.cpp`

The bitmask logic is small but tricky enough to deserve its own pure functions. Extracting them into a header keeps the widget code thin and lets us TDD the bit math without needing a `QApplication`.

- [ ] **Step 1: Write the failing test**

Create `cpp/tests/test_bitmask_helpers.cpp`:

```cpp
#include <QtTest>
#include "core/bitmask_helpers.h"

class TestBitmaskHelpers : public QObject {
    Q_OBJECT
private slots:
    void testGetBitSet() {
        // FPS_COUNTER bit (1<<1 = 2) is set in 6 (0b110)
        QCOMPARE(BitmaskHelpers::getBit(6, 2), true);
    }
    void testGetBitClear() {
        // FPS_COUNTER bit (2) is NOT set in 4 (0b100)
        QCOMPARE(BitmaskHelpers::getBit(4, 2), false);
    }
    void testSetBitOn() {
        // Setting FPS bit (2) in 4 (0b100) → 6 (0b110)
        QCOMPARE(BitmaskHelpers::setBit(4, 2, true), 6);
    }
    void testSetBitOff() {
        // Clearing FPS bit (2) in 6 (0b110) → 4 (0b100)
        QCOMPARE(BitmaskHelpers::setBit(6, 2, false), 4);
    }
    void testSetBitIdempotent() {
        // Setting an already-set bit leaves the value unchanged
        QCOMPARE(BitmaskHelpers::setBit(6, 2, true), 6);
        // Clearing an already-clear bit leaves the value unchanged
        QCOMPARE(BitmaskHelpers::setBit(4, 2, false), 4);
    }
    void testZeroBitmaskIsNoOp() {
        // bitmask=0 means "not a bitmask widget"; getBit returns false
        QCOMPARE(BitmaskHelpers::getBit(123, 0), false);
    }
};

QTEST_GUILESS_MAIN(TestBitmaskHelpers)
#include "test_bitmask_helpers.moc"
```

- [ ] **Step 2: Register the test in CMake**

Edit `cpp/CMakeLists.txt`. Find the existing `add_executable(test_ini_file ...)` block (around line 283). Add directly below the `test_sfo_parser` block:

```cmake
add_executable(test_bitmask_helpers
    tests/test_bitmask_helpers.cpp
)
set_target_properties(test_bitmask_helpers PROPERTIES AUTOMOC ON)
target_include_directories(test_bitmask_helpers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_bitmask_helpers PRIVATE Qt6::Core Qt6::Test)
add_test(NAME BitmaskHelpers COMMAND test_bitmask_helpers)
```

- [ ] **Step 3: Run the test to confirm it fails**

```bash
cd cpp
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
cmake --build build --target test_bitmask_helpers
```

Expected: build fails with `'core/bitmask_helpers.h' file not found`.

- [ ] **Step 4: Create the helper header**

Create `cpp/src/core/bitmask_helpers.h`:

```cpp
#pragma once

namespace BitmaskHelpers {

// Returns true if the given bit is set in value. bitmask=0 returns false
// (used as a sentinel meaning "not a bitmask widget").
inline bool getBit(int value, int bitmask) {
    if (bitmask == 0) return false;
    return (value & bitmask) != 0;
}

// Returns a new int with the given bit set or cleared. Idempotent for
// bits that are already in the requested state.
inline int setBit(int value, int bitmask, bool on) {
    if (bitmask == 0) return value;
    return on ? (value | bitmask) : (value & ~bitmask);
}

} // namespace BitmaskHelpers
```

- [ ] **Step 5: Run the test and confirm it passes**

```bash
cd cpp
cmake --build build --target test_bitmask_helpers
ctest --test-dir build -R BitmaskHelpers --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/bitmask_helpers.h cpp/tests/test_bitmask_helpers.cpp cpp/CMakeLists.txt
git commit -m "feat(core): add BitmaskHelpers for SettingDef bitmask support"
```

---

## Task 2: Add `bitmask` field to `SettingDef`

**Files:**
- Modify: `cpp/src/core/setting_def.h`

`SettingDef` is a plain-data struct. Adding a trailing field with a default value is non-breaking — every existing braced-init in the PCSX2 / DuckStation / current PPSSPP schemas continues to compile because the new field defaults to 0.

- [ ] **Step 1: Add the field**

Edit `cpp/src/core/setting_def.h`. After the `dependsOn` field (around line 39), add:

```cpp
    // If non-zero, this Bool setting reads/writes a single bit of an
    // int-valued INI key. The widget displays as a checkbox; on save the
    // bit is set/cleared in the existing int and the full int is written
    // back. Used by PPSSPP for iShowStatusFlags. Default 0 = normal Bool.
    int bitmask = 0;
```

The full struct should now end like:

```cpp
    // If non-empty, this setting is enabled only when the named key's bool is true.
    // The key is matched within the same settings group/section context.
    QString dependsOn;

    // If non-zero, this Bool setting reads/writes a single bit of an
    // int-valued INI key. The widget displays as a checkbox; on save the
    // bit is set/cleared in the existing int and the full int is written
    // back. Used by PPSSPP for iShowStatusFlags. Default 0 = normal Bool.
    int bitmask = 0;
};
```

- [ ] **Step 2: Build to verify nothing else broke**

```bash
cd cpp
cmake --build build --target EmuFront 2>&1 | tail -20
```

Expected: build succeeds. Existing PCSX2 / DuckStation / PPSSPP schema code keeps compiling because `bitmask` defaults to 0.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/core/setting_def.h
git commit -m "feat(core): add optional bitmask field to SettingDef"
```

---

## Task 3: Wire bitmask into the checkbox widget

**Files:**
- Modify: `cpp/src/ui/settings/emulator_settings_page.cpp` (the `makeCheckbox` lambda, lines 448–470)

This task changes the existing checkbox creation lambda so that when a `SettingDef` has a non-zero `bitmask`, the checkbox reads/writes a single bit of an int-valued key instead of a `"true"`/`"false"` string. The bit math is delegated to `BitmaskHelpers`.

- [ ] **Step 1: Add the include**

Near the top of `cpp/src/ui/settings/emulator_settings_page.cpp`, after the existing `#include "adapters/emulator_adapter.h"` line:

```cpp
#include "core/bitmask_helpers.h"
```

- [ ] **Step 2: Replace the `makeCheckbox` lambda body**

Find the lambda starting at `auto makeCheckbox = [this, &dependedOnKeys, &masterCheckboxes, &dependentWidgets](const SettingDef& def) -> QCheckBox* {` (around line 448). Replace the whole lambda body with:

```cpp
    auto makeCheckbox = [this, &dependedOnKeys, &masterCheckboxes, &dependentWidgets](const SettingDef& def) -> QCheckBox* {
        auto* check = new QCheckBox(def.label);
        check->setStyleSheet(checkBoxStyle());
        if (!def.tooltip.isEmpty()) check->setToolTip(def.tooltip);

        const QString val = m_appController->settingValue(m_emuId, def.section, def.key);

        if (def.bitmask != 0) {
            // Bitmask checkbox: int-valued key, this widget owns one bit.
            const int intVal = val.isEmpty() ? def.defaultValue.toInt() : val.toInt();
            check->setChecked(BitmaskHelpers::getBit(intVal, def.bitmask));
        } else {
            check->setChecked(val.isEmpty() ? (def.defaultValue == "true") : (val == "true"));
        }

        const QString section = def.section;
        const QString key     = def.key;
        const int bitmask     = def.bitmask;
        const QString defaultValue = def.defaultValue;

        connect(check, &QCheckBox::toggled, this,
            [this, section, key, bitmask, defaultValue](bool checked) {
                QVariantMap values;
                if (bitmask != 0) {
                    // Re-read current int from disk so multiple bitmask
                    // checkboxes sharing the same key merge correctly.
                    const QString cur = m_appController->settingValue(m_emuId, section, key);
                    const int curInt = cur.isEmpty() ? defaultValue.toInt() : cur.toInt();
                    const int newInt = BitmaskHelpers::setBit(curInt, bitmask, checked);
                    values[section + "/" + key] = QString::number(newInt);
                } else {
                    values[section + "/" + key] = checked ? "true" : "false";
                }
                m_appController->saveSettings(m_emuId, values);
            });

        if (dependedOnKeys.contains(def.key))
            masterCheckboxes[def.key] = check;
        if (!def.dependsOn.isEmpty())
            dependentWidgets[def.dependsOn].append(check);

        return check;
    };
```

- [ ] **Step 3: Build the full app to verify the change compiles**

```bash
cd cpp
cmake --build build --target EmuFront 2>&1 | tail -30
```

Expected: clean build, no warnings introduced.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/settings/emulator_settings_page.cpp
git commit -m "feat(settings): wire bitmask field through QCheckBox load/save"
```

---

## Task 4: Schema smoke test for the new PPSSPP layout (TDD)

**Files:**
- Create: `cpp/tests/test_ppsspp_schema.cpp`
- Modify: `cpp/CMakeLists.txt`

A small schema test fixates the new layout: it instantiates `PPSSPPAdapter`, calls `settingsSchema()`, and asserts that (a) the three sidebar categories exist, (b) the six Graphics sub-tabs exist, (c) all the new keys are present with the right type and bitmask. This catches both the schema rewrite in Task 5 and any future regression.

- [ ] **Step 1: Write the failing test**

Create `cpp/tests/test_ppsspp_schema.cpp`:

```cpp
#include <QtTest>
#include <QSet>
#include "adapters/ppsspp_adapter.h"
#include "core/setting_def.h"

class TestPPSSPPSchema : public QObject {
    Q_OBJECT

private:
    QVector<SettingDef> schema_;

private slots:
    void initTestCase() {
        PPSSPPAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void testCategoriesAreGraphicsAudioOverlay() {
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories, QSet<QString>({"Graphics", "Audio", "Overlay"}));
    }

    void testGraphicsSubcategories() {
        QSet<QString> subs;
        for (const auto& d : schema_)
            if (d.category == "Graphics") subs.insert(d.subcategory);
        QCOMPARE(subs, QSet<QString>({
            "Emulation", "Rendering", "Frame Pacing",
            "Performance", "Textures", "Post-Processing"
        }));
    }

    void testEmulationSettingsLiveUnderGraphics() {
        // FastMemoryAccess used to live under category="Emulation".
        // It must now be under Graphics → Emulation sub-tab.
        bool found = false;
        for (const auto& d : schema_) {
            if (d.key == "FastMemoryAccess") {
                QCOMPARE(d.category, QString("Graphics"));
                QCOMPARE(d.subcategory, QString("Emulation"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    void testLensFlareOcclusionExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.key == "DepthRasterMode") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Graphics"));
        QCOMPARE(found->subcategory, QString("Performance"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QCOMPARE(found->options.size(), 4);  // Auto / Low / Off / Always on
    }

    void testPostProcessingShaderExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "PostShaderList" && d.key == "PostShader1") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Graphics"));
        QCOMPARE(found->subcategory, QString("Post-Processing"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QVERIFY(found->options.size() >= 10);  // expect a healthy shader list
        // First option must be the "Off" sentinel
        QCOMPARE(found->options.first().second, QString("Off"));
    }

    void testOverlayBitmaskCheckboxes() {
        struct Expected { QString label; int bit; };
        const QVector<Expected> expected = {
            {"Show FPS Counter", 2},
            {"Show Speed",       4},
            {"Show Battery %",   8},
        };
        for (const auto& exp : expected) {
            const SettingDef* found = nullptr;
            for (const auto& d : schema_) {
                if (d.label == exp.label) { found = &d; break; }
            }
            QVERIFY2(found != nullptr, qPrintable("missing: " + exp.label));
            QCOMPARE(found->category, QString("Overlay"));
            QCOMPARE(found->key, QString("iShowStatusFlags"));
            QCOMPARE(int(found->type), int(SettingDef::Bool));
            QCOMPARE(found->bitmask, exp.bit);
        }
    }

    void testDebugOverlayExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.key == "iDebugOverlay") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Overlay"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
    }
};

QTEST_GUILESS_MAIN(TestPPSSPPSchema)
#include "test_ppsspp_schema.moc"
```

- [ ] **Step 2: Register the test in CMake**

Edit `cpp/CMakeLists.txt`. Below the `test_bitmask_helpers` block from Task 1, add:

```cmake
add_executable(test_ppsspp_schema
    tests/test_ppsspp_schema.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
)
set_target_properties(test_ppsspp_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_ppsspp_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_ppsspp_schema PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PPSSPPSchema COMMAND test_ppsspp_schema)
```

If `ppsspp_adapter.cpp` pulls in additional sources (check the `add_executable(EmuFront ...)` block above for any `ppsspp_*` files), add them here too. If a link error appears for an unresolved symbol, add the `.cpp` that defines it to this test target's source list.

- [ ] **Step 3: Build the test and confirm it fails**

```bash
cd cpp
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
cmake --build build --target test_ppsspp_schema
ctest --test-dir build -R PPSSPPSchema --output-on-failure
```

Expected: test compiles but fails — at least `testCategoriesAreGraphicsAudioOverlay` (current schema has `Emulation` as a top-level category) and the new-key tests will fail.

- [ ] **Step 4: Commit the failing test**

```bash
git add cpp/tests/test_ppsspp_schema.cpp cpp/CMakeLists.txt
git commit -m "test(ppsspp): add schema smoke test for new layout (failing)"
```

---

## Task 5: Rewrite `PPSSPPAdapter::settingsSchema()`

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp` (lines 57–270, the `settingsSchema()` function)

This replaces the entire schema function to match the spec: Graphics with 6 sub-tabs (Emulation moved in as the first), Audio unchanged, new Overlay top-level category. The defaults and combo options for unchanged settings are preserved verbatim — only categories/subcategories change for those.

- [ ] **Step 1: Replace `settingsSchema()`**

In `cpp/src/adapters/ppsspp_adapter.cpp`, replace lines 57–270 (the entire body of `settingsSchema()` from `QVector<SettingDef> PPSSPPAdapter::settingsSchema() const {` through `return s;\n}`) with:

```cpp
QVector<SettingDef> PPSSPPAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Emulation  (moved from top-level Emulation category)
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Emulation", "", "CPU", "FastMemoryAccess", "Fast Memory (Unstable)",
              "Uses faster but less accurate memory access. May cause crashes in some games.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Emulation", "", "General", "IgnoreBadMemAccess", "Ignore Bad Memory Accesses",
              "Silently ignores invalid memory reads/writes instead of crashing.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Emulation", "", "CPU", "IOTimingMethod", "I/O Timing Method",
              "Controls how UMD (disc) I/O timing is handled.",
              SettingDef::Combo, "0",
              {{"Fast (lag on slow storage)", "0"}, {"Host", "1"},
               {"Simulate UMD Delays", "2"}, {"Simulate UMD Slow", "3"}}, 0, 0, 0});
    s.append({"Graphics", "Emulation", "", "General", "ForceLagSync2", "Force Real Clock Sync",
              "Slower but less lag. Forces the emulator to run at real clock speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Emulation", "", "CPU", "CPUSpeed", "CPU Clock (MHz)",
              "Overclock the emulated PSP's CPU. 0 = default (222 MHz). Unstable on high values.",
              SettingDef::Int, "0", {}, 0, 1000, 1, "slider", "MHz"});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Rendering
    // ═══════════════════════════════════════════════════════════════════════
    // PPSSPP stores backend as "{int} ({NAME})" via ConfigTranslator, so our
    // combo values must match exactly for round-trip to work.
    s.append({"Graphics", "Rendering", "", "Graphics", "GraphicsBackend", "Backend",
              "Graphics API used for rendering.",
              SettingDef::Combo, "3 (VULKAN)",
              {{"OpenGL", "0 (OPENGL)"},
#if defined(Q_OS_WIN)
               {"Direct3D 11", "2 (DIRECT3D11)"},
#endif
               {"Vulkan", "3 (VULKAN)"}}, 0, 0, 0});
    s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "2",
              {{"Auto (1:1)", "0"}, {"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"},
               {"3x (1440x816)", "3"}, {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"},
               {"6x (2880x1632)", "6"}, {"7x (3360x1904)", "7"}, {"8x (3840x2176)", "8"},
               {"9x (4320x2448)", "9"}, {"10x (4800x2720)", "10"}}, 0, 0, 0});
    s.append({"Graphics", "Rendering", "", "Graphics", "SoftwareRenderer", "Software Rendering (slow, accurate)",
              "Uses CPU rendering for maximum accuracy. Very slow.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Rendering", "", "Graphics", "MultiSampleLevel", "Antialiasing (MSAA)",
              "Multisample anti-aliasing level.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"},
               {"16x", "4"}, {"32x", "5"}}, 0, 0, 0});
    s.append({"Graphics", "Rendering", "", "Graphics", "ReplaceTextures", "Replace Textures",
              "Allow custom texture replacement packs.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Frame Pacing
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "VerticalSync", "VSync",
              "Synchronize rendering to display refresh rate.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameSkip", "Frame Skipping",
              "Number of frames to skip to maintain speed.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"1", "1"}, {"2", "2"}, {"3", "3"},
               {"4", "4"}, {"5", "5"}, {"6", "6"}, {"7", "7"}, {"8", "8"}}, 0, 0, 0});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "AutoFrameSkip", "Auto Frameskip",
              "Automatically skip frames to maintain speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameRate", "Alternative Speed (%)",
              "Target FPS when using fast-forward. 0 = unlimited.",
              SettingDef::Int, "0", {}, 0, 300, 10, "slider", "%"});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "FrameRate2", "Alternative Speed 2 (%)",
              "Second FPS target for toggling. -1 = disabled, 0 = unlimited.",
              SettingDef::Int, "-1", {}, -1, 300, 10, "slider", "%"});
    s.append({"Graphics", "Frame Pacing", "", "Graphics", "RenderDuplicateFrames", "Render Duplicate Frames to 60 Hz",
              "Can make framerate smoother in games that run at lower framerates.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Performance  (two visual groups in one tab)
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Performance", "Performance", "Graphics", "InflightFrames", "Buffer Graphics Commands",
              "Faster, but adds input lag.",
              SettingDef::Combo, "3",
              {{"No buffer", "0"}, {"Up to 1", "1"}, {"Up to 2", "2"}, {"Up to 3", "3"}}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Performance", "Graphics", "HardwareTransform", "Hardware Transform",
              "Uses hardware geometry transformation. Disable only for debugging.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Performance", "Graphics", "SoftwareSkinning", "Software Skinning",
              "Combine skinned model draws on the CPU, faster in most games.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Performance", "Graphics", "HardwareTessellation", "Hardware Tessellation",
              "Uses hardware to make curves.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "SkipBufferEffects", "Skip Buffer Effects",
              "Faster, but nothing may draw in some games.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "DisableRangeCulling", "Disable Culling",
              "Disables range culling.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "SkipGPUReadbackMode", "Skip GPU Readbacks",
              "Skipping GPU readbacks is faster but may break some games.",
              SettingDef::Combo, "0",
              {{"No (Default)", "0"}, {"Skip", "1"}, {"Copy to texture", "2"}}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "TextureBackoffCache", "Lazy Texture Caching",
              "Faster, but can cause text problems in a few games.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "SplineBezierQuality", "Spline/Bezier Curves Quality",
              "Only used by some games, controls smoothness of curves.",
              SettingDef::Combo, "2",
              {{"Low", "0"}, {"Medium", "1"}, {"High (Default)", "2"}}, 0, 0, 0});
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "BloomHack", "Lower Resolution for Effects",
              "Reduces artifacts.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"Safe", "1"}, {"Balanced", "2"}, {"Aggressive", "3"}}, 0, 0, 0});
    // NEW: Lens Flare Occlusion
    s.append({"Graphics", "Performance", "Speed Hacks", "Graphics", "DepthRasterMode", "Lens Flare Occlusion",
              "Controls how the depth raster is used for lens flare occlusion.",
              SettingDef::Combo, "0",
              {{"Auto", "0"}, {"Low", "1"}, {"Off", "2"}, {"Always on", "3"}}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Textures
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Graphics", "Textures", "", "Graphics", "TexHardwareScaling", "GPU Texture Upscaler (fast)",
              "Faster texture upscaling on the GPU.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "TexScalingType", "Upscale Type",
              "Algorithm used for texture upscaling.",
              SettingDef::Combo, "0",
              {{"xBRZ", "0"}, {"Hybrid", "1"}, {"Bicubic", "2"}, {"Hybrid+Bicubic", "3"}}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "TexScalingLevel", "Upscale Level",
              "CPU heavy - some scaling may be delayed to avoid stutter.",
              SettingDef::Combo, "1",
              {{"Off", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x", "4"}, {"5x", "5"}}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "TexDeposterize", "Deposterize",
              "Fixes visual banding glitches in upscaled textures.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "AnisotropyLevel", "Anisotropic Filtering",
              "Improves texture quality at oblique angles.",
              SettingDef::Combo, "4",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"}, {"16x", "4"}}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "TextureFiltering", "Texture Filtering",
              "Filtering applied to textures.",
              SettingDef::Combo, "1",
              {{"Auto", "1"}, {"Nearest", "2"}, {"Linear", "3"}, {"Auto Max Quality", "4"}}, 0, 0, 0});
    s.append({"Graphics", "Textures", "", "Graphics", "Smart2DTexFiltering", "Smart 2D Texture Filtering",
              "Smarter filtering for 2D textures.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Graphics → Post-Processing  (NEW — replaces "Display layout & effects")
    // ═══════════════════════════════════════════════════════════════════════
    // Single-shader picker. PPSSPP stores the chain in [PostShaderList] as
    // PostShader1, PostShader2, ... — we expose only PostShader1 for now.
    // Values come from references/ppsspp-master/assets/shaders/defaultshaders.ini
    s.append({"Graphics", "Post-Processing", "", "PostShaderList", "PostShader1", "Post-Processing Shader",
              "Apply a post-processing effect to the rendered image.",
              SettingDef::Combo, "Off",
              {{"Off", "Off"},
               {"FXAA Antialiasing", "FXAA"},
               {"CRT Scanlines", "CRT"},
               {"Natural Colors", "Natural"},
               {"Natural (No Blur)", "NaturalA"},
               {"Vignette", "Vignette"},
               {"Fake Reflections", "FakeReflections"},
               {"Bloom", "Bloom"},
               {"Bloom (no blur)", "BloomNoBlur"},
               {"Sharpen", "Sharpen"},
               {"Scanlines (CRT)", "Scanlines"},
               {"Cartoon", "Cartoon"},
               {"4xHqGLSL Upscaler", "4xHqGLSL"},
               {"AA-Color", "AAColor"},
               {"Bicubic Upscaler", "UpscaleBicubic"},
               {"Spline36 Upscaler", "UpscaleSpline36"},
               {"5xBR Upscaler", "5xBR"},
               {"5xBR lv2 Upscaler", "5xBR-lv2"},
               {"Color Correction", "ColorCorrection"},
               {"PSP Color", "PSPColor"},
               {"LCD Persistence", "LCDPersistence"},
               {"Sharp Bilinear", "UpscaleSharpBilinear"},
               {"FSR-EASU", "FSR-EASU"}}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Audio — unchanged from previous schema
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Audio", "Audio playback", "", "Sound", "AudioSyncMode", "Playback Mode",
              "Audio synchronization method.",
              SettingDef::Combo, "1",
              {{"Granular", "0"}, {"Classic (lowest latency)", "1"}}, 0, 0, 0});
    s.append({"Audio", "Audio playback", "", "Sound", "FillAudioGaps", "Fill Audio Gaps",
              "Fill gaps in audio output to prevent pops.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    s.append({"Audio", "Game volume", "", "Sound", "Enable", "Enable Sound",
              "Enable audio output.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Audio", "Game volume", "", "Sound", "GameVolume", "Game Volume",
              "Master audio volume.",
              SettingDef::Int, "100", {}, 0, 100, 5, "slider", "%"});
    s.append({"Audio", "Game volume", "", "Sound", "ReverbRelativeVolume", "Reverb Volume",
              "Volume of reverb effects.",
              SettingDef::Int, "100", {}, 0, 200, 5, "slider", "%"});
    s.append({"Audio", "Game volume", "", "Sound", "AltSpeedRelativeVolume", "Alternate Speed Volume",
              "Volume when using fast-forward.",
              SettingDef::Int, "100", {}, 0, 100, 5, "slider", "%"});
    s.append({"Audio", "Game volume", "", "Sound", "AchievementVolume", "Achievement Sound Volume",
              "Volume of achievement notification sounds.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%"});

    s.append({"Audio", "UI sound", "", "General", "UISound", "UI Sound",
              "Play sounds for UI interactions.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Audio", "UI sound", "", "Sound", "UIVolume", "UI Volume",
              "Volume of UI sounds.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%"});
    s.append({"Audio", "UI sound", "", "Sound", "GamePreviewVolume", "Game Preview Volume",
              "Volume of game previews in the UI.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%"});

    s.append({"Audio", "Audio backend", "", "Sound", "AudioBufferSize", "Buffer Size",
              "Audio buffer size in samples. Smaller = less latency but more crackling risk.",
              SettingDef::Int, "256", {}, 64, 2048, 64, "slider", ""});
    s.append({"Audio", "Audio backend", "", "Sound", "AutoAudioDevice", "Use New Audio Devices Automatically",
              "Automatically switch to newly connected audio devices.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ═══════════════════════════════════════════════════════════════════════
    // Overlay  (NEW top-level sidebar entry — bitmask checkboxes + Debug overlay)
    // ═══════════════════════════════════════════════════════════════════════
    // iShowStatusFlags is an int bitfield. Bit values come from
    // references/ppsspp-master/Core/ConfigValues.h enum ShowStatusFlags:
    //   FPS_COUNTER     = 1 << 1 = 2
    //   SPEED_COUNTER   = 1 << 2 = 4
    //   BATTERY_PERCENT = 1 << 3 = 8
    // The trailing literal-int initializer uses the new SettingDef::bitmask field.
    s.append({"Overlay", "", "", "Graphics", "iShowStatusFlags", "Show FPS Counter",
              "Display the framerate counter in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", 2});
    s.append({"Overlay", "", "", "Graphics", "iShowStatusFlags", "Show Speed",
              "Display the emulation speed percentage in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", 4});
    s.append({"Overlay", "", "", "Graphics", "iShowStatusFlags", "Show Battery %",
              "Display the host battery percentage in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", 8});
    s.append({"Overlay", "", "", "Graphics", "iDebugOverlay", "Debug Overlay",
              "PPSSPP debug overlay. Note: PPSSPP itself does not persist this "
              "value across runs (it is marked DONT_SAVE upstream), so it will "
              "reset whenever PPSSPP is launched outside this app.",
              SettingDef::Combo, "0",
              {{"Off", "0"},
               {"Debug Stats", "1"},
               {"Frame Graph", "2"},
               {"Frame Timing", "3"},
               {"Control", "5"},
               {"Audio", "6"},
               {"GPU Profile", "7"},
               {"GPU Allocator", "8"},
               {"Framebuffer List", "9"}}, 0, 0, 0});

    return s;
}
```

Note on the Overlay rows: the trailing positional arguments (`"", "", "", 2`) correspond to `layout`, `suffix`, `dependsOn`, and the new `bitmask` field, in that order. The Debug Overlay entry uses defaults for all of those — it's a normal Combo, not a bitmask widget.

- [ ] **Step 2: Build the schema test and the full app**

```bash
cd cpp
cmake --build build --target test_ppsspp_schema EmuFront 2>&1 | tail -30
```

Expected: clean build.

- [ ] **Step 3: Run the schema test and confirm it passes**

```bash
ctest --test-dir build -R PPSSPPSchema --output-on-failure
```

Expected: `100% tests passed`. If any subtest fails, fix the schema entry it points at and rerun before moving on.

- [ ] **Step 4: Run the full test suite to confirm no regressions**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests pass (BitmaskHelpers, PPSSPPSchema, IniFile, RomScanner, Iso9660Reader, SfoParser).

- [ ] **Step 5: Commit**

```bash
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "feat(ppsspp): reorganize Graphics tab and add Overlay/Post-Processing settings"
```

---

## Task 6: Manual verification in the running app

**Files:** none — runtime smoke test only.

- [ ] **Step 1: Launch the app and open the PPSSPP settings page**

```bash
cd cpp
./build/EmulatorFrontend
```

Open Settings → PPSSPP. Verify by eye:

1. The PPSSPP sidebar shows exactly **Graphics**, **Audio**, **Overlay** (and any non-PPSSPP categories like Controllers/Hotkeys that come from elsewhere).
2. Clicking **Graphics** shows 6 horizontal sub-tabs in this order: **Emulation, Rendering, Frame Pacing, Performance, Textures, Post-Processing**.
3. The **Performance** sub-tab shows two grouped sections labeled "Performance" and "Speed Hacks", with **Lens Flare Occlusion** at the bottom of "Speed Hacks".
4. The **Post-Processing** sub-tab shows a single combo "Post-Processing Shader" defaulting to "Off".
5. Clicking **Overlay** in the sidebar shows four widgets: three checkboxes (Show FPS Counter, Show Speed, Show Battery %) and one combo (Debug Overlay).

- [ ] **Step 2: Verify bitmask round-trip in the actual INI**

With PPSSPP closed:

1. Toggle **Show FPS Counter** ON in the Overlay tab. Open `{root}/emulators/ppsspp/PSP/SYSTEM/ppsspp.ini` (or wherever `configFilePath()` resolves) and confirm `[Graphics]` has `iShowStatusFlags = 2`.
2. Toggle **Show Speed** ON. Confirm the same key now reads `iShowStatusFlags = 6` (2 | 4).
3. Toggle **Show Battery %** ON. Confirm `iShowStatusFlags = 14` (2 | 4 | 8).
4. Toggle **Show FPS Counter** OFF. Confirm `iShowStatusFlags = 12` (4 | 8).

If any of those values are wrong, the bitmask read-modify-write logic in Task 3 is broken — re-read the lambda and check the order of `settingValue` / `setBit` / `saveSettings` calls.

- [ ] **Step 3: Verify Post-Processing shader writes to the right section**

Pick "FXAA Antialiasing" in the combo. Confirm `ppsspp.ini` now contains:

```ini
[PostShaderList]
PostShader1 = FXAA
```

- [ ] **Step 4: Launch a PPSSPP game and confirm the settings take effect**

Start any PSP game. Confirm the FPS counter is visible in the corner if you enabled it, and the FXAA shader visibly affects the image. PPSSPP itself should not reset the values when it launches (other than `iDebugOverlay`, which is `DONT_SAVE` upstream).

If something doesn't take effect, the most likely cause is `configFilePath()` pointing at a file PPSSPP doesn't actually read — verify that `ensureConfig` and the runtime PPSSPP both use the same path.

- [ ] **Step 5: Smoke-test PCSX2 and DuckStation settings pages**

Open Settings → PCSX2 and Settings → DuckStation. Confirm they look identical to before — same categories, same widgets, same values. The `bitmask` field defaulting to 0 in `SettingDef` should mean zero observable changes for those adapters.

- [ ] **Step 6: Final commit (only if you fixed anything during manual verification)**

If verification surfaced no issues, skip this step. Otherwise:

```bash
git add -p   # review every change before staging
git commit -m "fix(ppsspp): <describe what you fixed>"
```

---

## Self-review checklist (run before declaring done)

- [ ] All 6 Graphics sub-tabs render with their expected setting list.
- [ ] Overlay sidebar entry exists and contains exactly 4 widgets.
- [ ] `iShowStatusFlags` bitmask round-trip works for all 8 combinations of FPS/Speed/Battery.
- [ ] PCSX2 and DuckStation settings pages are byte-identical to before.
- [ ] `ctest --test-dir build` passes with zero failures.
- [ ] No new compiler warnings introduced.
