# PCSX2 Settings Redesign — Plan 1: Foundations

**Spec:** `docs/superpowers/specs/2026-04-11-pcsx2-settings-redesign-design.md`
**Plan series:** 1 of 4 (Foundations → Graphics Display → Graphics Rendering/Post-Processing → Graphics OSD & Polish)
**Date:** 2026-04-11

## For agentic workers

Execute tasks in order. Each task ends in a commit. Do not skip ahead. After each task run `ctest --test-dir cpp/build --output-on-failure` to catch regressions. The Plan 1 scope is intentionally narrow: schema recommendedValue plumbing + new dialog shell + three simple pages (Emulation, Audio, Memory Cards). Graphics is stubbed to the legacy `EmulatorSettingsPage` in Task 17 and replaced by real pages in Plans 2–4.

**Do NOT implement in this plan:** Graphics sub-pages, preview widgets, `dependsOn` disable behavior, DuckStation/PPSSPP changes.

## Goal

Ship the new card-grid PCSX2 settings dialog with fully functional Emulation, Audio, and Memory Cards pages driven by the existing schema, plus a persistent description bar that shows per-setting help + recommended values. Route `AppController::showEmulatorSettings` through the new dialog for `emuId == "pcsx2"` only.

## Architecture

```
AppController::showEmulatorSettings(emuId)
  └── if emuId == "pcsx2" → Pcsx2SettingsDialog (new)
  │     └── QStackedWidget
  │           ├── Pcsx2CategoryHub (4 cards)
  │           ├── Pcsx2EmulationPage
  │           ├── Pcsx2AudioPage
  │           └── Pcsx2MemoryCardsPage
  │     └── Pcsx2DescriptionBar (bottom, persistent)
  │
  │     Graphics card → opens legacy EmulatorSettingsPage as modal (Plan 1 stub)
  │
  └── else → EmulatorSettingsPage (legacy, unchanged)
```

Widget primitives live under `cpp/src/ui/settings/pcsx2/widgets/`. Pages live under `cpp/src/ui/settings/pcsx2/pages/`. Theme constants live in `cpp/src/ui/settings/pcsx2/pcsx2_theme.h`. All new files are added explicitly to the `SOURCES` / `HEADERS` lists in `cpp/CMakeLists.txt`.

Every interactive widget emits a `focused(SettingDef)` signal on focus-in. Pages aggregate and re-emit as `settingFocused(SettingDef)`. `Pcsx2SettingsDialog` connects that signal to `Pcsx2DescriptionBar::setSetting`, which fills the body copy from `def.tooltip` and the amber pill from `def.recommendedValue` (or `def.defaultValue` if recommended is empty).

## Tech Stack

- **Language:** C++17
- **UI framework:** Qt6 (Widgets module, `QDialog` / `QFrame` / `QWidget` / `QStackedWidget`), `AUTOMOC ON`
- **Styling:** Qt stylesheet strings; `paintEvent` only for the focus halo on `Pcsx2Card`
- **Build system:** CMake 3.16 (explicit source lists in `cpp/CMakeLists.txt`)
- **Tests:** QtTest (`QTEST_GUILESS_MAIN`), one executable per test file
- **Theme colours (spec §Visual identity):**
  - Window background `#585450`
  - Title/desc bar `#4a4642`
  - Card bg `#646058`
  - Card border resting `#706c66`
  - Card border focused `#f59e0b`
  - Input bg `#585450`
  - Text primary `#f2efe8` / secondary `#d0ccc4` / muted `#9a9690`
  - Accent `#f59e0b`

---

## Task 1: Add `recommendedValue` field to `SettingDef`

**Files:**
- `cpp/src/core/setting_def.h` (modify)

- [ ] Edit `cpp/src/core/setting_def.h`. After the existing `QString defaultValue;` line (line 21), insert a new field:

```cpp
    QString defaultValue;
    // Optional recommended value shown in the new PCSX2 dialog description bar.
    // When empty, UI falls back to displaying defaultValue.
    QString recommendedValue;
```

- [ ] Build to confirm zero regressions:

```
cd cpp && cmake --build build 2>&1 | tail -20
```

Expected: build succeeds. No tests need updating — `SettingDef{...}` aggregate literals still compile because `recommendedValue` has a default value and remaining trailing fields are already defaulted.

- [ ] Commit:

```
git add cpp/src/core/setting_def.h
git commit -m "Add optional recommendedValue field to SettingDef

First step of the PCSX2 settings redesign. The new dialog's
description bar shows a per-setting recommendation pill sourced
from upstream PCSX2 defaults; when unset, the UI falls back to
defaultValue."
```

---

## Task 2: Populate `recommendedValue` for Emulation settings (TDD)

**Files:**
- `cpp/tests/test_pcsx2_recommended_values.cpp` (create)
- `cpp/src/adapters/pcsx2_adapter.cpp` (modify)
- `cpp/CMakeLists.txt` (modify)

- [ ] Create `cpp/tests/test_pcsx2_recommended_values.cpp`:

```cpp
#include <QtTest>
#include "adapters/pcsx2_adapter.h"
#include "core/setting_def.h"

class TestPcsx2RecommendedValues : public QObject {
    Q_OBJECT
private:
    QVector<SettingDef> schema_;
private slots:
    void initTestCase() {
        PCSX2Adapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }
    void testEmulationSettingsAllHaveRecommended() {
        int emuCount = 0;
        for (const auto& d : schema_) {
            if (d.category != "Emulation") continue;
            ++emuCount;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Emulation/%1").arg(d.key)));
        }
        QVERIFY(emuCount >= 13); // spec: 3 speed + 8 system + 5 frame pacing = 16
    }
};
QTEST_GUILESS_MAIN(TestPcsx2RecommendedValues)
#include "test_pcsx2_recommended_values.moc"
```

- [ ] Register the test in `cpp/CMakeLists.txt` immediately after the `test_ppsspp_schema` block (line 384):

```cmake
add_executable(test_pcsx2_recommended_values
    tests/test_pcsx2_recommended_values.cpp
    src/adapters/pcsx2_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_pcsx2_recommended_values PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_recommended_values PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_recommended_values PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME Pcsx2RecommendedValues COMMAND test_pcsx2_recommended_values)
```

- [ ] Run test — expect failure:

```
cd cpp && cmake --build build --target test_pcsx2_recommended_values && ctest --test-dir build -R Pcsx2RecommendedValues --output-on-failure
```

Expected: FAIL with "missing recommendedValue for Emulation/NominalScalar" or similar.

- [ ] Edit `cpp/src/adapters/pcsx2_adapter.cpp` and add `recommendedValue` via trailing struct-member assignment. The `SettingDef` aggregate literal currently uses positional args; we'll convert the Emulation entries to `SettingDef` objects that set `recommendedValue` after construction. Replace lines 52–98 (all 16 Emulation entries) with:

```cpp
    // ── Speed Control ───────────────────────────────────────────────────
    {
        SettingDef d{"Emulation", "", "Speed Control", "Framerate", "NominalScalar", "Normal Speed",
                     "Sets the target speed for normal gameplay.", SettingDef::Combo, "1", speedOptions, 0, 0, 0};
        d.recommendedValue = "1"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Speed Control", "Framerate", "TurboScalar", "Fast-Forward Speed",
                     "Sets the target speed when turbo mode is activated.", SettingDef::Combo, "2", speedOptions, 0, 0, 0};
        d.recommendedValue = "2"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Speed Control", "Framerate", "SlomoScalar", "Slow-Motion Speed",
                     "Sets the target speed when slow motion mode is activated.", SettingDef::Combo, "0.5", speedOptions, 0, 0, 0};
        d.recommendedValue = "0.5"; s.append(d);
    }

    // ── System Settings ─────────────────────────────────────────────────
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "EECycleRate", "EE Cycle Rate",
                     "Underclocks or overclocks the emulated Emotion Engine CPU.",
                     SettingDef::Combo, "0", {
                         {"50% (Underclock)", "-3"}, {"60% (Underclock)", "-2"}, {"75% (Underclock)", "-1"},
                         {"100% (Normal Speed)", "0"},
                         {"130% (Overclock)", "1"}, {"180% (Overclock)", "2"}, {"300% (Overclock)", "3"}
                     }, 0, 0, 0};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "EECycleSkip", "EE Cycle Skipping",
                     "Makes the emulated Emotion Engine skip cycles.",
                     SettingDef::Combo, "0", {
                         {"Disabled", "0"}, {"Mild Underclock", "1"}, {"Moderate Underclock", "2"}, {"Maximum Underclock", "3"}
                     }, 0, 0, 0};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore/Speedhacks", "vuThread", "Enable Multithreaded VU1 (MTVU)",
                     "Runs VU1 on a second thread. Substantial speed improvement in most games.", SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "EnableThreadPinning", "Enable Thread Pinning",
                     "Pins emulation threads to specific CPU cores for improved performance.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "CdvdPrecache", "Enable CDVD Precaching",
                     "Loads the disc image into RAM before starting. Can reduce stutter but uses more memory.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "HostFs", "Enable Host Filesystem",
                     "Enables access to the host filesystem from the emulated PS2.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "EnableCheats", "Enable Cheats",
                     "Enables loading cheats from pnach files.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "System Settings", "EmuCore", "EnableFastBoot", "Fast Boot",
                     "Skips the PS2 BIOS splash screen when booting a game.", SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true"; s.append(d);
    }

    // ── Frame Pacing / Latency Control ─────────────────────────────────
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "VsyncQueueSize", "Maximum Frame Latency",
                     "Sets the number of frames that can be queued up before the CPU waits. Set to 0 for optimal frame pacing.",
                     SettingDef::Combo, "2", {
                         {"Optimal (Frame Pacing)", "0"}, {"1 frame", "1"}, {"2 frames", "2"}, {"3 frames", "3"}
                     }, 0, 0, 0};
        d.recommendedValue = "2"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "SyncToHostRefreshRate", "Sync to Host Refresh Rate",
                     "Adjusts emulation speed slightly to match your monitor's refresh rate.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "VsyncEnable", "Vertical Sync (VSync)",
                     "Synchronizes frame output with the monitor to prevent screen tearing.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "UseVSyncForTiming", "Use Host VSync Timing",
                     "Uses the host's VSync timing instead of the emulated console's timing.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Emulation", "", "Frame Pacing / Latency Control", "EmuCore/GS", "SkipDuplicateFrames", "Skip Presenting Duplicate Frames",
                     "Skips presenting frames that are identical to the previous frame.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
```

Recommendation source: mirrors PCSX2 master `pcsx2/Config.h` defaults (Pcsx2Config, Pcsx2Config::SpeedhackOptions, Pcsx2Config::GSOptions), with every value matching `defaultValue` since upstream has no distinct "recommendation" metadata.

- [ ] Re-run:

```
cd cpp && cmake --build build --target test_pcsx2_recommended_values && ctest --test-dir build -R Pcsx2RecommendedValues --output-on-failure
```

Expected: PASS.

- [ ] Commit:

```
git add cpp/src/adapters/pcsx2_adapter.cpp cpp/tests/test_pcsx2_recommended_values.cpp cpp/CMakeLists.txt
git commit -m "Populate PCSX2 Emulation recommendedValue

Upstream PCSX2 defaults match the schema defaults for every Emulation
setting, so recommendedValue mirrors defaultValue. Unit test guards
against regressions by asserting every Emulation entry is non-empty."
```

---

## Task 3: Populate `recommendedValue` for Audio settings

**Files:**
- `cpp/tests/test_pcsx2_recommended_values.cpp` (modify)
- `cpp/src/adapters/pcsx2_adapter.cpp` (modify)

- [ ] Extend the test with a new slot after `testEmulationSettingsAllHaveRecommended`:

```cpp
    void testAudioSettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Audio") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Audio/%1").arg(d.key)));
        }
        QCOMPARE(count, 11);
    }
```

- [ ] Run — expect FAIL.

- [ ] Replace the Audio block in `pcsx2_adapter.cpp` (lines 253–287) with wrapped-scope equivalents that set `recommendedValue`. Mapping (from PCSX2 `Pcsx2Config::SPU2Options` defaults, `AudioStream` defaults in `pcsx2/Host/AudioStream.cpp`):

| key | defaultValue | recommendedValue |
|---|---|---|
| Backend | `Cubeb` | `Cubeb` |
| DriverName | `` | `` |
| DeviceName | `` | `` |
| ExpansionMode | `Disabled` | `Disabled` |
| SyncMode | `TimeStretch` | `TimeStretch` |
| BufferMS | `50` | `50` |
| OutputLatencyMS | `20` | `20` |
| OutputLatencyMinimal | `false` | `false` |
| StandardVolume | `100` | `100` |
| FastForwardVolume | `100` | `100` |
| OutputMuted | `false` | `false` |

For each of the 11 entries, wrap the literal in a block `{ SettingDef d{...}; d.recommendedValue = "X"; s.append(d); }` following the Task 2 pattern. Example for the first two:

```cpp
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "Backend", "Backend", "",
                     SettingDef::Combo, "Cubeb",
                     {{"Cubeb", "Cubeb"}, {"SDL", "SDL"}, {"Null (No Sound)", "Null"}}, 0, 0, 0};
        d.recommendedValue = "Cubeb"; s.append(d);
    }
    {
        SettingDef d{"Audio", "", "Configuration", "SPU2/Output", "DriverName", "Driver", "",
                     SettingDef::Combo, "",
                     {{"Default", ""}, {"audiounit", "audiounit"}}, 0, 0, 0};
        d.recommendedValue = ""; s.append(d); // intentionally empty — "Default"
    }
```

Note: the empty-string `Default` drivers/devices need a non-empty recommendedValue for the test — set the human-readable tag `"Default"`. Update the entries where defaultValue is empty to `d.recommendedValue = "Default";` so the pill shows something meaningful:

```cpp
        d.recommendedValue = "Default";   // DriverName
        d.recommendedValue = "Default";   // DeviceName
```

All other audio recommendations equal defaultValue per table above.

- [ ] Rebuild + re-run test. Expected: PASS.

- [ ] Commit:

```
git add cpp/src/adapters/pcsx2_adapter.cpp cpp/tests/test_pcsx2_recommended_values.cpp
git commit -m "Populate PCSX2 Audio recommendedValue

Recommendations mirror SPU2/AudioStream defaults; empty-default
Driver/Device entries surface 'Default' in the pill for clarity."
```

---

## Task 4: Populate `recommendedValue` for Memory Cards settings

**Files:**
- `cpp/tests/test_pcsx2_recommended_values.cpp` (modify)
- `cpp/src/adapters/pcsx2_adapter.cpp` (modify)

- [ ] Add slot:

```cpp
    void testMemoryCardsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Memory Cards") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Memory Cards/%1").arg(d.key)));
        }
        QCOMPARE(count, 7);
    }
```

- [ ] Rebuild + run — expect FAIL.

- [ ] Replace the Memory Cards block (lines 292–298) with wrapped scopes. Mapping:

| key | rec |
|---|---|
| Slot1_Enable | `true` |
| Slot1_Filename | `Mcd001.ps2` |
| Slot2_Enable | `true` |
| Slot2_Filename | `Mcd002.ps2` |
| Multitap1_Slot2_Enable | `false` |
| Multitap1_Slot3_Enable | `false` |
| Multitap1_Slot4_Enable | `false` |

Example:

```cpp
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Slot1_Enable", "Slot 1", "", SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true"; s.append(d);
    }
    {
        SettingDef d{"Memory Cards", "", "", "MemoryCards", "Slot1_Filename", "Slot 1 Filename", "", SettingDef::String, "Mcd001.ps2", {}, 0, 0, 0};
        d.recommendedValue = "Mcd001.ps2"; s.append(d);
    }
```

(continue for all 7 entries)

- [ ] Rebuild + test — expect PASS.

- [ ] Commit:

```
git add cpp/src/adapters/pcsx2_adapter.cpp cpp/tests/test_pcsx2_recommended_values.cpp
git commit -m "Populate PCSX2 Memory Cards recommendedValue"
```

---

## Task 5: Create `pcsx2_theme.h` colour constants

**Files:**
- `cpp/src/ui/settings/pcsx2/pcsx2_theme.h` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] Create the directory structure first by creating the theme header. Contents:

```cpp
#pragma once
#include <QColor>
#include <QString>

// PCSX2 Settings dialog palette — spec 2026-04-11. Do NOT reuse
// settings_theme.h constants; the spec mandates a warm mid-grey +
// amber palette that differs from the global settings theme.
namespace Pcsx2Theme {

inline QColor windowBg()       { return QColor("#585450"); }
inline QColor titleBarBg()     { return QColor("#4a4642"); }
inline QColor cardBg()         { return QColor("#646058"); }
inline QColor cardBorder()     { return QColor("#706c66"); }
inline QColor cardBorderFocus(){ return QColor("#f59e0b"); }
inline QColor inputBg()        { return QColor("#585450"); }
inline QColor textPrimary()    { return QColor("#f2efe8"); }
inline QColor textSecondary()  { return QColor("#d0ccc4"); }
inline QColor textMuted()      { return QColor("#9a9690"); }
inline QColor accent()         { return QColor("#f59e0b"); }
inline QColor letterbox()      { return QColor("#3a3632"); }

// Ready-to-use stylesheet fragments
inline QString cardQss() {
    return QStringLiteral(
        "QFrame#Pcsx2Card {"
        "  background-color: #646058;"
        "  border: 1px solid #706c66;"
        "  border-radius: 8px;"
        "}"
        "QFrame#Pcsx2Card[focused=\"true\"] {"
        "  border: 1px solid #f59e0b;"
        "}");
}

inline QString sectionHeaderQss() {
    return QStringLiteral(
        "QLabel#Pcsx2SectionHeader {"
        "  color: #f59e0b;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  padding-bottom: 4px;"
        "}");
}

inline QString comboQss() {
    return QStringLiteral(
        "QComboBox {"
        "  background-color: #585450;"
        "  color: #f2efe8;"
        "  border: 1px solid #706c66;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  min-height: 22px;"
        "}"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #585450;"
        "  color: #f2efe8;"
        "  selection-background-color: #f59e0b;"
        "  selection-color: #1a1816;"
        "}");
}

inline QString sliderQss() {
    return QStringLiteral(
        "QSlider::groove:horizontal {"
        "  height: 4px; background: #4a4642; border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal { background: #f59e0b; border-radius: 2px; }"
        "QSlider::handle:horizontal {"
        "  background: #f59e0b; width: 14px; height: 14px;"
        "  margin: -5px 0; border-radius: 7px;"
        "}");
}

inline QString descriptionBarQss() {
    return QStringLiteral(
        "QFrame#Pcsx2DescriptionBar {"
        "  background-color: #4a4642;"
        "  border-left: 3px solid #f59e0b;"
        "  padding: 12px 16px;"
        "}"
        "QLabel#Pcsx2DescText { color: #f2efe8; font-size: 13px; }"
        "QLabel#Pcsx2DescRecommended {"
        "  color: #f59e0b; font-size: 12px;"
        "  padding: 3px 8px;"
        "  border: 1px solid rgba(245,158,11,0.25);"
        "  border-radius: 5px;"
        "  background-color: rgba(245,158,11,0.09);"
        "}");
}

} // namespace Pcsx2Theme
```

- [ ] Add to the `HEADERS` list in `cpp/CMakeLists.txt` (after line 142, inside the closing `)` of the HEADERS set):

```cmake
    src/ui/settings/pcsx2/pcsx2_theme.h
```

- [ ] Build:

```
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -5
```

Expected: builds cleanly.

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pcsx2_theme.h cpp/CMakeLists.txt
git commit -m "Add Pcsx2Theme header with spec palette"
```

---

## Task 6: Create `Pcsx2Card` base widget

**Files:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.h` (create)
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] `pcsx2_card.h`:

```cpp
#pragma once
#include <QFrame>

// Base card for the PCSX2 settings grid. Keyboard-focusable; repaints
// a 2 px amber halo when focused (QSS cannot draw outer glow).
class Pcsx2Card : public QFrame {
    Q_OBJECT
    Q_PROPERTY(bool focused READ hasFocus NOTIFY focusedChanged)
public:
    explicit Pcsx2Card(QWidget* parent = nullptr);

signals:
    void focused();
    void focusedChanged();
    void activated(); // Enter / Return

protected:
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
};
```

- [ ] `pcsx2_card.cpp`:

```cpp
#include "pcsx2_card.h"
#include "../pcsx2_theme.h"
#include <QPainter>
#include <QFocusEvent>
#include <QKeyEvent>

Pcsx2Card::Pcsx2Card(QWidget* parent) : QFrame(parent) {
    setObjectName("Pcsx2Card");
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(Pcsx2Theme::cardQss());
    setProperty("focused", false);
}

void Pcsx2Card::focusInEvent(QFocusEvent* e) {
    QFrame::focusInEvent(e);
    setProperty("focused", true);
    style()->unpolish(this); style()->polish(this);
    emit focused();
    emit focusedChanged();
    update();
}

void Pcsx2Card::focusOutEvent(QFocusEvent* e) {
    QFrame::focusOutEvent(e);
    setProperty("focused", false);
    style()->unpolish(this); style()->polish(this);
    emit focusedChanged();
    update();
}

void Pcsx2Card::paintEvent(QPaintEvent* e) {
    QFrame::paintEvent(e);
    if (!hasFocus()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor halo = Pcsx2Theme::accent();
    halo.setAlphaF(0.30);
    QPen pen(halo, 2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QRectF r = rect().adjusted(1, 1, -1, -1);
    p.drawRoundedRect(r, 8, 8);
}

void Pcsx2Card::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        emit activated();
        return;
    }
    QFrame::keyPressEvent(e);
}
```

- [ ] Add to CMake SOURCES + HEADERS:

```cmake
    # In SOURCES
    src/ui/settings/pcsx2/widgets/pcsx2_card.cpp
    # In HEADERS
    src/ui/settings/pcsx2/widgets/pcsx2_card.h
```

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -5
```

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.h cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.cpp cpp/CMakeLists.txt
git commit -m "Add Pcsx2Card base widget with focus halo"
```

---

## Task 7: Create `Pcsx2SectionHeader`

**Files:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_section_header.h` (create)
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_section_header.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] `.h`:

```cpp
#pragma once
#include <QLabel>

class Pcsx2SectionHeader : public QLabel {
    Q_OBJECT
public:
    explicit Pcsx2SectionHeader(const QString& text, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* e) override;
};
```

- [ ] `.cpp`:

```cpp
#include "pcsx2_section_header.h"
#include "../pcsx2_theme.h"
#include <QPainter>

Pcsx2SectionHeader::Pcsx2SectionHeader(const QString& text, QWidget* parent)
    : QLabel(text.toUpper(), parent) {
    setObjectName("Pcsx2SectionHeader");
    setStyleSheet(Pcsx2Theme::sectionHeaderQss());
    setContentsMargins(0, 8, 0, 4);
}

void Pcsx2SectionHeader::paintEvent(QPaintEvent* e) {
    QLabel::paintEvent(e);
    QPainter p(this);
    QColor line = Pcsx2Theme::cardBorder();
    line.setAlphaF(0.40);
    p.setPen(QPen(line, 1));
    int y = height() - 1;
    p.drawLine(0, y, width(), y);
}
```

- [ ] CMake: add `.cpp` to SOURCES, `.h` to HEADERS.

- [ ] Build + commit:

```
git add cpp/src/ui/settings/pcsx2/widgets/pcsx2_section_header.{h,cpp} cpp/CMakeLists.txt
git commit -m "Add Pcsx2SectionHeader"
```

---

## Task 8: Create `Pcsx2Toggle` + `Pcsx2ToggleRow` (TDD)

**Files:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_toggle.{h,cpp}` (create)
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_toggle_row.{h,cpp}` (create)
- `cpp/tests/test_pcsx2_toggle.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] `pcsx2_toggle.h`:

```cpp
#pragma once
#include <QAbstractButton>

class Pcsx2Toggle : public QAbstractButton {
    Q_OBJECT
public:
    explicit Pcsx2Toggle(QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(34, 18); }
protected:
    void paintEvent(QPaintEvent*) override;
};
```

- [ ] `pcsx2_toggle.cpp`:

```cpp
#include "pcsx2_toggle.h"
#include "../pcsx2_theme.h"
#include <QPainter>

Pcsx2Toggle::Pcsx2Toggle(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    connect(this, &QAbstractButton::toggled, this, [this]{ update(); });
}

void Pcsx2Toggle::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool on = isChecked();
    QColor track = on ? Pcsx2Theme::accent() : Pcsx2Theme::cardBorder();
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);
    const int knobD = height() - 4;
    const int x = on ? (width() - knobD - 2) : 2;
    p.setBrush(Pcsx2Theme::textPrimary());
    p.drawEllipse(x, 2, knobD, knobD);
    if (hasFocus()) {
        QPen pen(Pcsx2Theme::accent(), 1);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(0,0,-1,-1), height()/2.0, height()/2.0);
    }
}
```

- [ ] `pcsx2_toggle_row.h`:

```cpp
#pragma once
#include <QWidget>
#include "core/setting_def.h"

class QLabel;
class Pcsx2Toggle;

class Pcsx2ToggleRow : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2ToggleRow(QWidget* parent = nullptr);
    void setLabel(const QString& text);
    void setChecked(bool on);
    bool isChecked() const;
    void setSettingDef(const SettingDef& def) { m_def = def; }
    const SettingDef& settingDef() const { return m_def; }

signals:
    void toggled(bool on);
    void focused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    QLabel* m_label = nullptr;
    Pcsx2Toggle* m_toggle = nullptr;
    SettingDef m_def;
};
```

- [ ] `pcsx2_toggle_row.cpp`:

```cpp
#include "pcsx2_toggle_row.h"
#include "pcsx2_toggle.h"
#include "../pcsx2_theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>

Pcsx2ToggleRow::Pcsx2ToggleRow(QWidget* parent) : QWidget(parent) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color:#d0ccc4;font-size:13px;");
    m_toggle = new Pcsx2Toggle(this);
    lay->addWidget(m_label, 1);
    lay->addWidget(m_toggle, 0, Qt::AlignRight);
    connect(m_toggle, &QAbstractButton::toggled, this, &Pcsx2ToggleRow::toggled);
    m_toggle->installEventFilter(this);
}

void Pcsx2ToggleRow::setLabel(const QString& text) { m_label->setText(text); }
void Pcsx2ToggleRow::setChecked(bool on) { m_toggle->setChecked(on); }
bool Pcsx2ToggleRow::isChecked() const { return m_toggle->isChecked(); }

bool Pcsx2ToggleRow::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_toggle && e->type() == QEvent::FocusIn) {
        emit focused(m_def);
    }
    return QWidget::eventFilter(obj, e);
}
```

- [ ] `cpp/tests/test_pcsx2_toggle.cpp`:

```cpp
#include <QtTest>
#include "ui/settings/pcsx2/widgets/pcsx2_toggle.h"

class TestPcsx2Toggle : public QObject {
    Q_OBJECT
private slots:
    void defaultStateIsUnchecked() {
        Pcsx2Toggle t;
        QVERIFY(!t.isChecked());
        QVERIFY(t.isCheckable());
    }
    void setCheckedEmitsToggled() {
        Pcsx2Toggle t;
        QSignalSpy spy(&t, &QAbstractButton::toggled);
        t.setChecked(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }
    void sizeHintMatchesSpec() {
        Pcsx2Toggle t;
        QCOMPARE(t.sizeHint(), QSize(34, 18));
    }
};
QTEST_MAIN(TestPcsx2Toggle)
#include "test_pcsx2_toggle.moc"
```

- [ ] CMake: add 4 source files to SOURCES/HEADERS. Add test executable:

```cmake
add_executable(test_pcsx2_toggle
    tests/test_pcsx2_toggle.cpp
    src/ui/settings/pcsx2/widgets/pcsx2_toggle.cpp
)
set_target_properties(test_pcsx2_toggle PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_toggle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_toggle PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME Pcsx2Toggle COMMAND test_pcsx2_toggle)
```

- [ ] Build + test + commit:

```
cd cpp && cmake --build build && ctest --test-dir build -R Pcsx2Toggle --output-on-failure
git add cpp/src/ui/settings/pcsx2/widgets/pcsx2_toggle*.{h,cpp} cpp/tests/test_pcsx2_toggle.cpp cpp/CMakeLists.txt
git commit -m "Add Pcsx2Toggle + Pcsx2ToggleRow widgets"
```

---

## Task 9: Create `Pcsx2ComboRow` (TDD)

**Files:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_combo_row.{h,cpp}` (create)
- `cpp/tests/test_pcsx2_combo_row.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] `.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include <QPair>
#include "core/setting_def.h"

class QLabel;
class QComboBox;

class Pcsx2ComboRow : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2ComboRow(QWidget* parent = nullptr);
    void setLabel(const QString& text);
    void setOptions(const QVector<QPair<QString, QString>>& opts);
    void setValue(const QString& iniValue);
    QString value() const;
    void setSettingDef(const SettingDef& def) { m_def = def; }
    const SettingDef& settingDef() const { return m_def; }

signals:
    void valueChanged(QString iniValue);
    void focused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    QLabel* m_label = nullptr;
    QComboBox* m_combo = nullptr;
    SettingDef m_def;
};
```

- [ ] `.cpp`:

```cpp
#include "pcsx2_combo_row.h"
#include "../pcsx2_theme.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>

Pcsx2ComboRow::Pcsx2ComboRow(QWidget* parent) : QWidget(parent) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color:#d0ccc4;font-size:13px;");
    m_label->setMinimumWidth(180);
    m_combo = new QComboBox(this);
    m_combo->setStyleSheet(Pcsx2Theme::comboQss());
    m_combo->setMinimumWidth(200);
    lay->addWidget(m_label, 0);
    lay->addWidget(m_combo, 1);
    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ emit valueChanged(value()); });
    m_combo->installEventFilter(this);
}

void Pcsx2ComboRow::setLabel(const QString& text) { m_label->setText(text); }

void Pcsx2ComboRow::setOptions(const QVector<QPair<QString, QString>>& opts) {
    m_combo->blockSignals(true);
    m_combo->clear();
    for (const auto& o : opts) m_combo->addItem(o.first, o.second);
    m_combo->blockSignals(false);
}

void Pcsx2ComboRow::setValue(const QString& iniValue) {
    for (int i = 0; i < m_combo->count(); ++i) {
        if (m_combo->itemData(i).toString() == iniValue) {
            m_combo->setCurrentIndex(i);
            return;
        }
    }
}

QString Pcsx2ComboRow::value() const {
    return m_combo->currentData().toString();
}

bool Pcsx2ComboRow::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_combo && e->type() == QEvent::FocusIn) emit focused(m_def);
    return QWidget::eventFilter(obj, e);
}
```

- [ ] Test `cpp/tests/test_pcsx2_combo_row.cpp`:

```cpp
#include <QtTest>
#include "ui/settings/pcsx2/widgets/pcsx2_combo_row.h"

class TestPcsx2ComboRow : public QObject {
    Q_OBJECT
private slots:
    void setOptionsAndValueRoundTrips() {
        Pcsx2ComboRow row;
        row.setOptions({{"Auto", "-1"}, {"Off", "0"}, {"On", "1"}});
        row.setValue("0");
        QCOMPARE(row.value(), QString("0"));
        row.setValue("1");
        QCOMPARE(row.value(), QString("1"));
    }
    void valueChangedSignalFires() {
        Pcsx2ComboRow row;
        row.setOptions({{"A", "a"}, {"B", "b"}});
        QSignalSpy spy(&row, &Pcsx2ComboRow::valueChanged);
        row.setValue("b");
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().first().toString(), QString("b"));
    }
};
QTEST_MAIN(TestPcsx2ComboRow)
#include "test_pcsx2_combo_row.moc"
```

- [ ] CMake registration + test target (mirror Task 8's block).
- [ ] Build + test + commit:

```
git commit -m "Add Pcsx2ComboRow widget with value round-trip"
```

---

## Task 10: Create `Pcsx2SliderRow` (TDD)

**Files:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_slider_row.{h,cpp}` (create)
- `cpp/tests/test_pcsx2_slider_row.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] `.h`:

```cpp
#pragma once
#include <QWidget>
#include "core/setting_def.h"

class QLabel;
class QSlider;

class Pcsx2SliderRow : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2SliderRow(QWidget* parent = nullptr);
    void setLabel(const QString& text);
    void setRange(int lo, int hi);
    void setSuffix(const QString& s);
    void setValue(int v);
    int value() const;
    void setSettingDef(const SettingDef& def) { m_def = def; }
    const SettingDef& settingDef() const { return m_def; }

signals:
    void valueChanged(int v);
    void focused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void refreshValueLabel();
    QLabel* m_label = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_value = nullptr;
    QString m_suffix;
    SettingDef m_def;
};
```

- [ ] `.cpp`:

```cpp
#include "pcsx2_slider_row.h"
#include "../pcsx2_theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QEvent>

Pcsx2SliderRow::Pcsx2SliderRow(QWidget* parent) : QWidget(parent) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color:#d0ccc4;font-size:13px;");
    m_label->setMinimumWidth(180);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setStyleSheet(Pcsx2Theme::sliderQss());
    m_value = new QLabel(this);
    m_value->setStyleSheet("color:#f2efe8;font-size:13px;");
    m_value->setMinimumWidth(60);
    m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_label, 0);
    lay->addWidget(m_slider, 1);
    lay->addWidget(m_value, 0);
    connect(m_slider, &QSlider::valueChanged, this, [this](int v){
        refreshValueLabel();
        emit valueChanged(v);
    });
    m_slider->installEventFilter(this);
}

void Pcsx2SliderRow::setLabel(const QString& t) { m_label->setText(t); }
void Pcsx2SliderRow::setRange(int lo, int hi) { m_slider->setRange(lo, hi); }
void Pcsx2SliderRow::setSuffix(const QString& s) { m_suffix = s; refreshValueLabel(); }
void Pcsx2SliderRow::setValue(int v) { m_slider->setValue(v); }
int Pcsx2SliderRow::value() const { return m_slider->value(); }

void Pcsx2SliderRow::refreshValueLabel() {
    m_value->setText(QString::number(m_slider->value()) + m_suffix);
}

bool Pcsx2SliderRow::eventFilter(QObject* o, QEvent* e) {
    if (o == m_slider && e->type() == QEvent::FocusIn) emit focused(m_def);
    return QWidget::eventFilter(o, e);
}
```

- [ ] Test covering: setRange clamps setValue; setSuffix formats value label; valueChanged fires.
- [ ] CMake + test target + build + commit.

---

## Task 11: Create `Pcsx2DescriptionBar` (TDD)

**Files:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_description_bar.{h,cpp}` (create)
- `cpp/tests/test_pcsx2_description_bar.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] `.h`:

```cpp
#pragma once
#include <QFrame>
#include "core/setting_def.h"

class QLabel;

class Pcsx2DescriptionBar : public QFrame {
    Q_OBJECT
public:
    explicit Pcsx2DescriptionBar(QWidget* parent = nullptr);
    void setSetting(const SettingDef& def);
    void clear();
    // test hooks
    QString descText() const;
    QString recommendedText() const;
private:
    QLabel* m_text = nullptr;
    QLabel* m_rec = nullptr;
};
```

- [ ] `.cpp`:

```cpp
#include "pcsx2_description_bar.h"
#include "../pcsx2_theme.h"
#include <QHBoxLayout>
#include <QLabel>

Pcsx2DescriptionBar::Pcsx2DescriptionBar(QWidget* parent) : QFrame(parent) {
    setObjectName("Pcsx2DescriptionBar");
    setStyleSheet(Pcsx2Theme::descriptionBarQss());
    setMinimumHeight(100);
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    m_text = new QLabel(this);
    m_text->setObjectName("Pcsx2DescText");
    m_text->setWordWrap(true);
    m_rec = new QLabel(this);
    m_rec->setObjectName("Pcsx2DescRecommended");
    m_rec->setAlignment(Qt::AlignTop | Qt::AlignRight);
    lay->addWidget(m_text, 1);
    lay->addWidget(m_rec, 0, Qt::AlignTop);
    clear();
}

void Pcsx2DescriptionBar::setSetting(const SettingDef& def) {
    m_text->setText(def.tooltip.isEmpty()
                    ? QStringLiteral("No description available.")
                    : def.tooltip);
    const QString rec = def.recommendedValue.isEmpty() ? def.defaultValue : def.recommendedValue;
    m_rec->setText(QStringLiteral("Recommended: %1").arg(rec));
    m_rec->setVisible(!rec.isEmpty());
}

void Pcsx2DescriptionBar::clear() {
    m_text->setText(QStringLiteral("Focus a setting to see its description."));
    m_rec->setVisible(false);
}

QString Pcsx2DescriptionBar::descText() const { return m_text->text(); }
QString Pcsx2DescriptionBar::recommendedText() const { return m_rec->text(); }
```

- [ ] Test `test_pcsx2_description_bar.cpp`:

```cpp
#include <QtTest>
#include "ui/settings/pcsx2/widgets/pcsx2_description_bar.h"
#include "core/setting_def.h"

class TestPcsx2DescriptionBar : public QObject {
    Q_OBJECT
private slots:
    void setSettingFillsTextAndPill() {
        Pcsx2DescriptionBar bar;
        SettingDef d;
        d.tooltip = "Runs VU1 on a second thread.";
        d.defaultValue = "true";
        d.recommendedValue = "true";
        bar.setSetting(d);
        QCOMPARE(bar.descText(), QString("Runs VU1 on a second thread."));
        QCOMPARE(bar.recommendedText(), QString("Recommended: true"));
    }
    void emptyRecommendedFallsBackToDefault() {
        Pcsx2DescriptionBar bar;
        SettingDef d;
        d.tooltip = "t";
        d.defaultValue = "42";
        bar.setSetting(d);
        QCOMPARE(bar.recommendedText(), QString("Recommended: 42"));
    }
    void clearShowsPlaceholder() {
        Pcsx2DescriptionBar bar;
        bar.clear();
        QVERIFY(bar.descText().contains("Focus"));
    }
};
QTEST_MAIN(TestPcsx2DescriptionBar)
#include "test_pcsx2_description_bar.moc"
```

- [ ] CMake + build + test + commit.

---

## Task 12: Create `Pcsx2SettingsDialog` shell

**Files:**
- `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.{h,cpp}` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] `.h`:

```cpp
#pragma once
#include <QDialog>
#include <QStack>
#include "core/setting_def.h"

class AppController;
class QStackedWidget;
class Pcsx2DescriptionBar;
class Pcsx2CategoryHub;

class Pcsx2SettingsDialog : public QDialog {
    Q_OBJECT
public:
    Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

    // Navigation API used by child pages
    void pushPage(QWidget* page);
    void popPage();
    AppController* appController() const { return m_app; }
    QString emuId() const { return m_emuId; }

public slots:
    void setFocusedSetting(const SettingDef& def);
    void clearFocusedSetting();

private slots:
    void onCategoryActivated(const QString& category);

private:
    AppController* m_app;
    QString m_emuId;
    QStackedWidget* m_stack = nullptr;
    Pcsx2DescriptionBar* m_descBar = nullptr;
    Pcsx2CategoryHub* m_hub = nullptr;
    QStack<int> m_history;
};
```

- [ ] `.cpp`:

```cpp
#include "pcsx2_settings_dialog.h"
#include "pcsx2_category_hub.h"
#include "widgets/pcsx2_description_bar.h"
#include "pages/pcsx2_emulation_page.h"
#include "pages/pcsx2_audio_page.h"
#include "pages/pcsx2_memory_cards_page.h"
#include "pcsx2_theme.h"
#include "ui/settings/emulator_settings_page.h"
#include <QStackedWidget>
#include <QVBoxLayout>

Pcsx2SettingsDialog::Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : QDialog(parent), m_app(app), m_emuId(emuId) {
    setWindowTitle("PCSX2 Settings");
    setMinimumSize(950, 550);
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(Pcsx2Theme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_descBar = new Pcsx2DescriptionBar(this);

    m_hub = new Pcsx2CategoryHub(this);
    connect(m_hub, &Pcsx2CategoryHub::categoryActivated,
            this, &Pcsx2SettingsDialog::onCategoryActivated);
    m_stack->addWidget(m_hub);

    root->addWidget(m_stack, 1);
    root->addWidget(m_descBar, 0);
}

void Pcsx2SettingsDialog::pushPage(QWidget* page) {
    int idx = m_stack->addWidget(page);
    m_history.push(m_stack->currentIndex());
    m_stack->setCurrentIndex(idx);
    clearFocusedSetting();
}

void Pcsx2SettingsDialog::popPage() {
    if (m_history.isEmpty()) { accept(); return; }
    QWidget* current = m_stack->currentWidget();
    int prev = m_history.pop();
    m_stack->setCurrentIndex(prev);
    if (current && current != m_hub) { m_stack->removeWidget(current); current->deleteLater(); }
    clearFocusedSetting();
}

void Pcsx2SettingsDialog::setFocusedSetting(const SettingDef& def) { m_descBar->setSetting(def); }
void Pcsx2SettingsDialog::clearFocusedSetting() { m_descBar->clear(); }

void Pcsx2SettingsDialog::onCategoryActivated(const QString& category) {
    if (category == "Emulation") {
        auto* page = new Pcsx2EmulationPage(this);
        connect(page, &Pcsx2EmulationPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
    } else if (category == "Audio") {
        auto* page = new Pcsx2AudioPage(this);
        connect(page, &Pcsx2AudioPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
    } else if (category == "Memory Cards") {
        auto* page = new Pcsx2MemoryCardsPage(this);
        connect(page, &Pcsx2MemoryCardsPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
    } else if (category == "Graphics") {
        // Plan 1 fallback; replaced by Pcsx2GraphicsPage in Plan 2.
        auto* legacy = new EmulatorSettingsPage(m_app, m_emuId);
        legacy->setAttribute(Qt::WA_DeleteOnClose);
        legacy->setWindowModality(Qt::ApplicationModal);
        legacy->show();
    }
}
```

- [ ] Register `.cpp` + `.h` in CMake SOURCES/HEADERS.

Note: this file `#include`s pages that do not yet exist — subsequent tasks create them. The build will fail until Task 14. Mark the task's build verification as "deferred to Task 14" in the commit message.

Actually — to keep each task green, temporarily comment out the page `#include`s and the `onCategoryActivated` body until the pages land. Replace with:

```cpp
void Pcsx2SettingsDialog::onCategoryActivated(const QString& category) {
    Q_UNUSED(category); // pages land in Tasks 14-17
}
```

- [ ] Build (should compile). Commit:

```
git commit -m "Add Pcsx2SettingsDialog shell"
```

---

## Task 13: Create `Pcsx2CategoryHub`

**Files:**
- `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.{h,cpp}` (create)
- `cpp/CMakeLists.txt` (modify)

- [ ] `.h`:

```cpp
#pragma once
#include <QWidget>

class AppController;
class Pcsx2Card;

class Pcsx2CategoryHub : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2CategoryHub(QWidget* parent = nullptr);
signals:
    void categoryActivated(QString category);
    void openNativeRequested();
private:
    Pcsx2Card* makeCard(const QString& title, const QString& descriptor,
                       int settingCount, const QString& categoryKey);
};
```

- [ ] `.cpp`:

```cpp
#include "pcsx2_category_hub.h"
#include "widgets/pcsx2_card.h"
#include "widgets/pcsx2_section_header.h"
#include "pcsx2_theme.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

Pcsx2CategoryHub::Pcsx2CategoryHub(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(14);

    auto* title = new QLabel("PCSX2 Settings", this);
    title->setStyleSheet("color:#f2efe8;font-size:20px;font-weight:600;");
    root->addWidget(title);

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    grid->addWidget(makeCard("Emulation", "Speed control, system settings, frame pacing", 16, "Emulation"), 0, 0);
    grid->addWidget(makeCard("Graphics", "Renderer, display, post-processing, OSD", 35, "Graphics"), 0, 1);
    grid->addWidget(makeCard("Audio", "Backend, latency, volume", 11, "Audio"), 1, 0);
    grid->addWidget(makeCard("Memory Cards", "Memory cards and multitap slots", 7, "Memory Cards"), 1, 1);
    root->addLayout(grid, 1);

    auto* nativeBtn = new QPushButton("Open Native Settings", this);
    nativeBtn->setStyleSheet(
        "QPushButton { background:#4a4642; color:#f2efe8; border:1px solid #706c66;"
        " border-radius:4px; padding:6px 14px; }"
        "QPushButton:focus { border-color:#f59e0b; }");
    connect(nativeBtn, &QPushButton::clicked, this, &Pcsx2CategoryHub::openNativeRequested);
    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    bottom->addWidget(nativeBtn);
    root->addLayout(bottom);
}

Pcsx2Card* Pcsx2CategoryHub::makeCard(const QString& title, const QString& descriptor,
                                      int settingCount, const QString& categoryKey) {
    auto* card = new Pcsx2Card(this);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(6);
    auto* t = new QLabel(title, card);
    t->setStyleSheet("color:#f2efe8;font-size:18px;font-weight:600;");
    auto* d = new QLabel(descriptor, card);
    d->setStyleSheet("color:#d0ccc4;font-size:13px;");
    d->setWordWrap(true);
    auto* c = new QLabel(QString("%1 settings  →").arg(settingCount), card);
    c->setStyleSheet("color:#f59e0b;font-size:12px;");
    v->addWidget(t);
    v->addWidget(d);
    v->addStretch();
    v->addWidget(c);
    card->setMinimumHeight(140);
    QObject::connect(card, &Pcsx2Card::activated, this,
                     [this, categoryKey]{ emit categoryActivated(categoryKey); });
    return card;
}
```

- [ ] CMake registration. Uncomment the `Pcsx2SettingsDialog::onCategoryActivated` body entries for "Graphics" — but still leave Emulation/Audio/Memory Cards branches commented until their pages exist. Actually simpler: leave the body empty through Task 13 and wire it incrementally in each later task.

- [ ] Build + commit:

```
git commit -m "Add Pcsx2CategoryHub landing page"
```

---

## Task 14: Create `Pcsx2EmulationPage`

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_emulation_page.{h,cpp}` (create)
- `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp` (modify — wire up)
- `cpp/CMakeLists.txt` (modify)

- [ ] `.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class Pcsx2ComboRow;
class Pcsx2ToggleRow;

class Pcsx2EmulationPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2EmulationPage(Pcsx2SettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef> m_schema; // filtered to category=="Emulation"
};
```

- [ ] `.cpp` — skeleton showing the key structure; the engineer fills in all 16 widgets following the same pattern:

```cpp
#include "pcsx2_emulation_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_section_header.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../pcsx2_theme.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>

Pcsx2EmulationPage::Pcsx2EmulationPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Emulation") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* Pcsx2EmulationPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2EmulationPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    // Back button
    auto* back = new QPushButton("← Back", this);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; } QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &Pcsx2SettingsDialog::popPage);
    root->addWidget(back);

    auto makeComboRow = [this](const QString& key) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        auto* row = new Pcsx2ComboRow(this);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2EmulationPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& v){
            const SettingDef* d2 = findDef(key);
            saveValue(d2->section, d2->key, v);
        });
        return row;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2EmulationPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            const SettingDef* d2 = findDef(key);
            saveValue(d2->section, d2->key, on ? "true" : "false");
        });
        v->addWidget(row);
        card->setProperty("toggleKey", key);
        return card;
    };

    // ── Speed Control ──
    root->addWidget(new Pcsx2SectionHeader("Speed Control", this));
    auto* speedCard = new Pcsx2Card(this);
    auto* speedV = new QVBoxLayout(speedCard);
    speedV->setContentsMargins(14, 12, 14, 12);
    speedV->addWidget(makeComboRow("NominalScalar"));
    speedV->addWidget(makeComboRow("TurboScalar"));
    speedV->addWidget(makeComboRow("SlomoScalar"));
    root->addWidget(speedCard);

    // ── System Settings ──
    root->addWidget(new Pcsx2SectionHeader("System Settings", this));
    auto* sysCard = new Pcsx2Card(this);
    auto* sysV = new QVBoxLayout(sysCard);
    sysV->setContentsMargins(14, 12, 14, 12);
    sysV->addWidget(makeComboRow("EECycleRate"));
    sysV->addWidget(makeComboRow("EECycleSkip"));
    root->addWidget(sysCard);

    auto* sysGrid = new QGridLayout();
    sysGrid->setSpacing(10);
    sysGrid->addWidget(makeToggleCard("vuThread"),             0, 0);
    sysGrid->addWidget(makeToggleCard("EnableThreadPinning"),  0, 1);
    sysGrid->addWidget(makeToggleCard("CdvdPrecache"),         1, 0);
    sysGrid->addWidget(makeToggleCard("EnableCheats"),         1, 1);
    sysGrid->addWidget(makeToggleCard("EnableFastBoot"),       2, 0);
    sysGrid->addWidget(makeToggleCard("HostFs"),               2, 1);
    root->addLayout(sysGrid);

    // ── Frame Pacing ──
    root->addWidget(new Pcsx2SectionHeader("Frame Pacing", this));
    auto* fpCard = new Pcsx2Card(this);
    auto* fpV = new QVBoxLayout(fpCard);
    fpV->setContentsMargins(14, 12, 14, 12);
    fpV->addWidget(makeComboRow("VsyncQueueSize"));
    root->addWidget(fpCard);

    auto* fpGrid = new QGridLayout();
    fpGrid->setSpacing(10);
    fpGrid->addWidget(makeToggleCard("SyncToHostRefreshRate"), 0, 0);
    fpGrid->addWidget(makeToggleCard("VsyncEnable"),           0, 1);
    fpGrid->addWidget(makeToggleCard("SkipDuplicateFrames"),   1, 0);
    fpGrid->addWidget(makeToggleCard("UseVSyncForTiming"),     1, 1);
    root->addLayout(fpGrid);

    root->addStretch();
}

void Pcsx2EmulationPage::loadValues() {
    // Walk children and set value from AppController::settingValue
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
}

void Pcsx2EmulationPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

Note: verify the exact `saveSettings` payload shape against the existing `EmulatorSettingsPage` implementation — use whatever key format (`"section/key"` or a nested map) it already uses. Adjust `saveValue` accordingly.

- [ ] In `pcsx2_settings_dialog.cpp`, wire up the Emulation branch of `onCategoryActivated` (and re-enable the `#include`).

- [ ] CMake registration (page .cpp/.h). Build.

- [ ] Commit:

```
git commit -m "Add Pcsx2EmulationPage wired into dialog"
```

---

## Task 15: Create `Pcsx2AudioPage`

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_audio_page.{h,cpp}` (create)
- `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp` (modify)
- `cpp/CMakeLists.txt` (modify)

Mirror `Pcsx2EmulationPage`. Layout per spec §Audio:

1. Section header "Configuration".
2. 2-column grid of 4 combo cards wrapping `Pcsx2ComboRow` each: Backend, ExpansionMode, SyncMode, DriverName.
3. Full-width slider card: BufferMS.
4. `1fr auto` row (`QHBoxLayout` with expanding slider card + fixed-width toggle card): OutputLatencyMS (slider) + OutputLatencyMinimal (toggle).
5. Section header "Volume Controls".
6. Full-width slider card: StandardVolume.
7. `1fr auto` row: FastForwardVolume (slider) + OutputMuted (toggle).

Slider cards use `Pcsx2SliderRow` with `setRange(d.minVal, d.maxVal)` and `setSuffix(d.suffix)`.

`loadValues()` and `saveValue()` identical in shape to the Emulation page, extended to walk `Pcsx2SliderRow` children.

- [ ] Wire Audio branch in `onCategoryActivated`.
- [ ] CMake + build + commit:

```
git commit -m "Add Pcsx2AudioPage"
```

---

## Task 16: Create `Pcsx2MemoryCardsPage`

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_memory_cards_page.{h,cpp}` (create)
- `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp` (modify)
- `cpp/CMakeLists.txt` (modify)

Layout per spec §Memory Cards:

1. Back button.
2. Full-width compound card "Slot 1": `Pcsx2ToggleRow` ("Slot 1") + path row (`QLabel` "Mcd001.ps2" + `QPushButton` "Browse..." opening a `QFileDialog`, writing result through `saveValue` for `Slot1_Filename`).
3. Full-width compound card "Slot 2" — same pattern for `Slot2_*`.
4. 3-column grid of 3 toggle cards: `Multitap1_Slot2_Enable`, `Multitap1_Slot3_Enable`, `Multitap1_Slot4_Enable`.

All interactive widgets emit `focused()` → `settingFocused` re-emission.

- [ ] Wire Memory Cards branch in `onCategoryActivated`.
- [ ] CMake + build + commit:

```
git commit -m "Add Pcsx2MemoryCardsPage"
```

---

## Task 17: Wire Graphics card to legacy `EmulatorSettingsPage` fallback

**Files:**
- `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp` (modify)

- [ ] In `onCategoryActivated`, implement the Graphics branch:

```cpp
    } else if (category == "Graphics") {
        // Plan 1 fallback; replaced by Pcsx2GraphicsPage in Plan 2.
        // We open the legacy schema-driven page as a separate modal so Plan 1
        // leaves Graphics fully functional without requiring the new per-subpage
        // layouts to exist yet.
        auto* legacy = new EmulatorSettingsPage(m_app, m_emuId);
        legacy->setAttribute(Qt::WA_DeleteOnClose);
        legacy->setWindowModality(Qt::ApplicationModal);
        legacy->show();
    }
```

- [ ] Also connect the hub's `openNativeRequested` signal to `AppController::openNativeEmulatorSettings`:

```cpp
    connect(m_hub, &Pcsx2CategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
```

- [ ] Build + commit:

```
git commit -m "Wire PCSX2 Graphics card to legacy EmulatorSettingsPage fallback

Plan 1 stub. Plans 2-4 replace this with real Pcsx2GraphicsPage +
Display/Rendering/Post-Processing/OSD sub-pages."
```

---

## Task 18: Route PCSX2 through `AppController::showEmulatorSettings` + smoke test

**Files:**
- `cpp/src/ui/app_controller.cpp` (modify)

- [ ] Edit `cpp/src/ui/app_controller.cpp` line 860:

```cpp
#include "ui/settings/pcsx2/pcsx2_settings_dialog.h"
// ... (existing includes)

void AppController::showEmulatorSettings(const QString& emuId) {
    if (emuId == QLatin1String("pcsx2")) {
        auto* dialog = new Pcsx2SettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
    auto* dialog = new EmulatorSettingsPage(this, emuId);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
```

- [ ] Build + launch smoke test:

```
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build && open ./build/RetroNest.app
```

- [ ] Manual verification checklist (perform all):
  - Open RetroNest, navigate to Emulators → PCSX2 → settings button.
  - The new category hub appears with 4 cards (Emulation, Graphics, Audio, Memory Cards) and an "Open Native Settings" button bottom-right.
  - Arrow-key focus moves between the 4 cards; focused card shows amber border + halo.
  - Enter on Emulation → Emulation page renders Speed Control full-width card + System Settings card + 2×3 toggle grid + Frame Pacing card + 2×2 toggle grid.
  - Focusing any widget updates the bottom description bar with tooltip text and "Recommended: …" pill.
  - Change a value (e.g. toggle MTVU off), back out to hub, re-enter Emulation — value persists. Verify `PCSX2.ini` on disk updated.
  - Enter on Audio → 2×2 combo grid + sliders render, value round-trip works.
  - Enter on Memory Cards → slot cards + multitap grid render.
  - Enter on Graphics → legacy `EmulatorSettingsPage` opens as a separate window (Plan 1 stub). Close it returns focus to the hub.
  - Click "Open Native Settings" → PCSX2 launches externally.
  - Launch DuckStation and PPSSPP settings → still use the legacy `EmulatorSettingsPage` (no regressions).

- [ ] Run full test suite:

```
ctest --test-dir cpp/build --output-on-failure
```

Expected: all tests pass.

- [ ] Commit:

```
git add cpp/src/ui/app_controller.cpp
git commit -m "Route PCSX2 settings through Pcsx2SettingsDialog

AppController::showEmulatorSettings branches on emuId: pcsx2 gets
the new card-grid dialog; DuckStation and PPSSPP continue to use
EmulatorSettingsPage unchanged. Plan 1 complete."
```

---

## Plan 1 completion criteria

- [ ] `SettingDef` has a `recommendedValue` field.
- [ ] Every Emulation, Audio, Memory Cards PCSX2 schema entry has a non-empty `recommendedValue`, verified by `test_pcsx2_recommended_values`.
- [ ] `Pcsx2SettingsDialog` opens for `emuId == "pcsx2"` and shows a working category hub.
- [ ] Emulation, Audio, Memory Cards pages render from the schema, load + save values through `AppController`, update the description bar on focus.
- [ ] Graphics card opens the legacy `EmulatorSettingsPage` (Plan 1 stub, documented in code).
- [ ] DuckStation and PPSSPP still use `EmulatorSettingsPage` — no regression.
- [ ] `ctest` passes.

## Not in Plan 1 (handled in later plans)

- `Pcsx2GraphicsPage` + Display / Rendering / Post-Processing / OSD sub-pages
- Aspect-ratio and OSD live preview widgets
- `dependsOn` visual-disable logic for Shade Boost sliders
- DuckStation / PPSSPP redesigns
- Any schema changes beyond `recommendedValue`

### Critical Files for Implementation

- /Users/mark/Documents/RetroNest-Project/cpp/src/core/setting_def.h
- /Users/mark/Documents/RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/src/ui/app_controller.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/CMakeLists.txt
- /Users/mark/Documents/RetroNest-Project/cpp/tests/test_ppsspp_schema.cpp (reference pattern)

---

**Summary of what the plan covers:** 18 tasks implementing Plan 1 of 4. Tasks 1–4 add `SettingDef::recommendedValue` and backfill it for all 16 Emulation, 11 Audio, and 7 Memory Cards settings in `PCSX2Adapter::settingsSchema()`, guarded by a new `test_pcsx2_recommended_values` QtTest executable. Tasks 5–11 build the widget primitives (`pcsx2_theme.h`, `Pcsx2Card` with amber focus halo, `Pcsx2SectionHeader`, `Pcsx2Toggle`/`Pcsx2ToggleRow`, `Pcsx2ComboRow`, `Pcsx2SliderRow`, `Pcsx2DescriptionBar`) with QtTest unit coverage for the pure-logic widgets. Tasks 12–16 assemble the `Pcsx2SettingsDialog` shell, `Pcsx2CategoryHub` landing, and the three simple pages (Emulation, Audio, Memory Cards), each wired through `AppController::settingValue` / `saveSettings`. Task 17 stubs the Graphics card to open the legacy `EmulatorSettingsPage` as a modal fallback (with an in-code comment marking it as a Plan 1 stub). Task 18 flips `AppController::showEmulatorSettings` to route `pcsx2` through the new dialog and defines the manual smoke-test checklist.

**Important:** I could not write this to disk (read-only planning mode prohibits file creation). Please save the content above to `/Users/mark/Documents/RetroNest-Project/docs/superpowers/plans/2026-04-11-pcsx2-settings-redesign-plan-1-foundations.md` yourself, or invoke an agent with write access.