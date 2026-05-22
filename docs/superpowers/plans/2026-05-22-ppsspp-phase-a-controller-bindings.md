# PPSSPP Libretro — Phase A: Controller Bindings UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the PSP controller-mapping page in RetroNest Settings render per-button rows (D-pad, face buttons, shoulders, Start/Select) with spotlight overlays on the PSP artwork, mirroring the working mgba and PCSX2 libretro adapters.

**Architecture:** Override `PpssppLibretroAdapter::controllerBindingDefsForType()` to return a 12-entry `QVector<BindingDef>` covering the PSP-1000's physical controls. Each entry maps a label to a RetroPad slot via the canonical `.key` strings that `retroPadSlotFromKey()` in `cpp/src/core/libretro/input_router.h` recognises, plus a `cardSlot` for layout and `spotlightX/Y/R` viewBox coordinates against `cpp/qml/AppUI/images/controllers/PSP.svg` (viewBox `0 0 2367 1014`). The same data drives both the visual mapper UI (`controller_bindings_view.cpp:493`) and launch-time SDL → RetroPad binding (`game_session.cpp:380`).

**Tech Stack:** C++17, Qt 6.11, RetroNest libretro adapter layer.

**No spec doc** — this plan IS the spec (Phase A is small and mechanical).

**Branch state:** main is at `ba238c2` (post task #6, post migration-helper removal). Working tree clean. Build dir at `cpp/build/`.

---

## Background facts (cached so the executor doesn't have to re-research)

### Reference adapters

The closest precedent is `Pcsx2LibretroAdapter::controllerBindingDefsForType` at `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp:25`. PSP's surface is **literally a subset** of PCSX2's DualShock 2 surface — drop L2/R2/L3/R3, rename L1→L and R1→R, and keep the same face-button mapping (PlayStation conventions: South=Cross/B, East=Circle/A, West=Square/Y, North=Triangle/X). PCSX2's spotlight coords are all `0,0,0` (suppressed) because its DS2 SVG hasn't been wired; we DO want PSP spotlights wired here. Use mgba's pattern (`cpp/src/adapters/libretro/mgba_libretro_adapter.cpp:67`) for the spotlight-coords shape — it has real positional data against `Gameboy.svg`.

### BindingDef shape

From `cpp/src/core/binding_def.h:21`:

```cpp
struct BindingDef {
    enum Kind { Button, Axis };
    Kind kind;
    QString label;          // "Cross", "L1"
    QString group;          // "Face Buttons", "Triggers"
    QString section;        // INI section — use "Pad1" for libretro port 1
    QString key;            // INI key — MUST match retroPadSlotFromKey()
    QString defaultValue;   // "SDL-0/+A"
    QString cardSlot = {};  // "DPad" / "FaceButtons" / "LeftAnalog" /
                            // "RightAnalog" / "Shoulders" / "System"
    int spotlightX = 0;     // viewBox X
    int spotlightY = 0;     // viewBox Y
    int spotlightR = 0;     // spotlight radius. {0,0,0} = no overlay.
};
```

### Canonical .key strings

From `cpp/src/core/libretro/input_router.h:49` — these are the ONLY strings `retroPadSlotFromKey()` recognises:

`Up`, `Down`, `Left`, `Right`, `B`, `A`, `Y`, `X`, `L`, `R`, `L2`, `R2`, `L3`, `R3`, `Start`, `Select`. Any other `.key` value resolves to `RetroPadSlot::None` and is silently skipped at launch wiring.

### Full PSP → RetroPad map (the 12 rows you'll be writing)

| Physical PSP button | `.label`     | `.group`    | `.key`   | `.defaultValue`        | `.cardSlot`   |
|---------------------|--------------|-------------|----------|------------------------|---------------|
| D-Pad Up            | `D-Pad Up`   | `D-Pad`     | `Up`     | `SDL-0/DPadUp`         | `DPad`        |
| D-Pad Down          | `D-Pad Down` | `D-Pad`     | `Down`   | `SDL-0/DPadDown`       | `DPad`        |
| D-Pad Left          | `D-Pad Left` | `D-Pad`     | `Left`   | `SDL-0/DPadLeft`       | `DPad`        |
| D-Pad Right         | `D-Pad Right`| `D-Pad`     | `Right`  | `SDL-0/DPadRight`      | `DPad`        |
| Cross (×)           | `Cross`      | `Buttons`   | `B`      | `SDL-0/FaceSouth`      | `FaceButtons` |
| Circle (○)          | `Circle`     | `Buttons`   | `A`      | `SDL-0/FaceEast`       | `FaceButtons` |
| Square (□)          | `Square`     | `Buttons`   | `Y`      | `SDL-0/FaceWest`       | `FaceButtons` |
| Triangle (△)        | `Triangle`   | `Buttons`   | `X`      | `SDL-0/FaceNorth`      | `FaceButtons` |
| L                   | `L`          | `Shoulders` | `L`      | `SDL-0/LeftShoulder`   | `Shoulders`   |
| R                   | `R`          | `Shoulders` | `R`      | `SDL-0/RightShoulder`  | `Shoulders`   |
| Start               | `Start`      | `System`    | `Start`  | `SDL-0/Start`          | `System`      |
| Select              | `Select`     | `System`    | `Select` | `SDL-0/Back`           | `System`      |

PSP also has an **analog nub** but it's not in this surface — the nub feeds RetroPad analog axes which RetroNest's `InputRouter` handles at the runtime layer (not via `BindingDef`). Don't add nub rows here.

PSP also has a **HOME** button but it's a system-level button RetroNest does not expose to games, so don't bind it.

### SVG facts

- File: `cpp/qml/AppUI/images/controllers/PSP.svg`
- viewBox: `0 0 2367 1014` (wider than tall — PSP-1000 horizontal layout)
- The widget that consumes spotlight coords is `controller_bindings_view.cpp` — it overlays an OpenEmu-style highlight on the SVG at `(spotlightX, spotlightY)` with radius `spotlightR`, in viewBox units.
- Approximate button positions (rough first cut — you WILL need to tune these visually in Task 3):
  - D-pad center: ~(325, 510). Up/Down ±100 in Y, Left/Right ±100 in X. Radius ~55.
  - Face cluster center: ~(1900, 510). Triangle/Cross ±100 in Y, Square/Circle ±100 in X. Radius ~50.
  - L shoulder: ~(130, 150), r ~60. R shoulder: ~(2240, 150), r ~60.
  - Start: ~(1450, 870), r ~35. Select: ~(1330, 870), r ~35.

### Why no upfront unit test for retroPadSlotFromKey routing

`retroPadSlotFromKey()` is a free inline function with no PPSSPP-specific behaviour. The standalone DuckStation controller-schema tests cover INI semantics for standalone adapters; libretro adapters route through SDL at runtime and the contract is tested implicitly via `test_controller_bindings_view` + a manual smoke-test (Task 4). We add a small data-shape regression test (Task 1) that catches accidental key drift (a `.key` like `"Cross"` would silently break runtime binding because it doesn't match `retroPadSlotFromKey`).

---

## File map

| Path | Action | Responsibility |
|---|---|---|
| `cpp/src/adapters/libretro/ppsspp_libretro_adapter.h` | MODIFY (+1 line) | Declare `controllerBindingDefsForType(const QString&) const override`. |
| `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp` | MODIFY (+~30 LOC) | Implement the 12-row table. |
| `cpp/tests/test_ppsspp_libretro_bindings.cpp` | CREATE | Regression guard: count = 12, every `.key` in the canonical set, every `.cardSlot` in the allowed set. |
| `cpp/CMakeLists.txt` | MODIFY (+9 LOC) | Register `test_ppsspp_libretro_bindings` add_executable + add_test. |
| `cpp/qml/AppUI/images/controllers/PSP.svg` | READ-ONLY | Visual reference for spotlight coordinate tuning in Task 3. Don't edit. |

---

## Tasks

### Task 1: Write the failing regression test

**Files:**
- Create: `cpp/tests/test_ppsspp_libretro_bindings.cpp`
- Modify: `cpp/CMakeLists.txt` (register the new test)

- [ ] **Step 1: Create the test file**

Write to `cpp/tests/test_ppsspp_libretro_bindings.cpp`:

```cpp
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
```

- [ ] **Step 2: Register the test target in CMakeLists.txt**

Add this block to `cpp/CMakeLists.txt` immediately after the existing `add_executable(test_hotkey_defs ...)` block (search for `add_test(NAME HotkeyDefs COMMAND test_hotkey_defs)` and insert below). Sources mirror the minimal set needed to link a libretro adapter test — `core_runtime.cpp` + its OpenGL deps are required because LibretroAdapter is its base class:

```cmake
add_executable(test_ppsspp_libretro_bindings
    tests/test_ppsspp_libretro_bindings.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/adapters/libretro/ppsspp_libretro_adapter.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/video_hardware_gl.mm
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/frontend_settings_store.cpp
    src/core/libretro/rcheevos_runtime.cpp
    src/core/libretro/retro_log.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
    src/core/path_overrides_store.cpp
)
set_target_properties(test_ppsspp_libretro_bindings PROPERTIES AUTOMOC ON)
target_include_directories(test_ppsspp_libretro_bindings PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(test_ppsspp_libretro_bindings PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Test chdr-static rcheevos_static
    ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS} "-framework OpenGL" "-framework IOSurface"
)
add_test(NAME PpssppLibretroBindings COMMAND test_ppsspp_libretro_bindings)
```

- [ ] **Step 3: Configure + build the new test target**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake . && cmake --build . --target test_ppsspp_libretro_bindings -j8
```

Expected: build succeeds (the test compiles even though `controllerBindingDefsForType` isn't overridden yet — the base class returns `{}` and we just get test failures, not compile errors).

- [ ] **Step 4: Run the test and confirm it fails as expected**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && ctest -R "^PpssppLibretroBindings$" --output-on-failure
```

Expected output contains:
```
FAIL!  : TestPpssppLibretroBindings::rowCount_matchesPspSurface() Compared values are not the same
   Actual   (defs.size()): 0
   Expected (12)         : 12
```

This confirms the test sees the empty default from the base class. Don't commit yet — Task 2 makes it pass.

---

### Task 2: Implement `controllerBindingDefsForType`

**Files:**
- Modify: `cpp/src/adapters/libretro/ppsspp_libretro_adapter.h`
- Modify: `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`

- [ ] **Step 1: Declare the override in the header**

In `cpp/src/adapters/libretro/ppsspp_libretro_adapter.h`, find the existing declarations (around line 24–27):

```cpp
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<PathDef> pathsDefs() const override;

    QString extractSerial(const QString& romPath) const override;
```

Insert one new declaration between `pathsDefs` and `extractSerial`:

```cpp
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<PathDef> pathsDefs() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;

    QString extractSerial(const QString& romPath) const override;
```

The header already includes `libretro_adapter.h` → `emulator_adapter.h` which declares `BindingDef`, so no extra `#include` is needed.

- [ ] **Step 2: Implement the table in the .cpp**

In `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`, append this function below the existing `pathsDefs()` and above `extractSerial()`. The spotlight coordinates here are FIRST-CUT estimates — Task 3 tunes them by visual inspection.

```cpp
QVector<BindingDef> PpssppLibretroAdapter::controllerBindingDefsForType(const QString&) const {
    // PSP-1000 horizontal layout. Spotlight coords target PSP.svg's
    // viewBox (2367 x 1014). PlayStation face-button conventions:
    //   Cross  (south) -> RetroPad B
    //   Circle (east)  -> RetroPad A
    //   Square (west)  -> RetroPad Y
    //   Triangle (north) -> RetroPad X
    // The analog nub feeds RetroPad axes at the runtime layer and is
    // not part of this digital-binding surface.
    return {
        // D-Pad — cross at left-middle
        { BindingDef::Button, "D-Pad Up",    "D-Pad",   "Pad1", "Up",    "SDL-0/DPadUp",        "DPad",         325, 410, 55 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad",   "Pad1", "Down",  "SDL-0/DPadDown",      "DPad",         325, 610, 55 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad",   "Pad1", "Left",  "SDL-0/DPadLeft",      "DPad",         225, 510, 55 },
        { BindingDef::Button, "D-Pad Right", "D-Pad",   "Pad1", "Right", "SDL-0/DPadRight",     "DPad",         425, 510, 55 },
        // Face buttons — cluster at right-middle
        { BindingDef::Button, "Cross",       "Buttons", "Pad1", "B",     "SDL-0/FaceSouth",     "FaceButtons", 1900, 610, 50 },
        { BindingDef::Button, "Circle",      "Buttons", "Pad1", "A",     "SDL-0/FaceEast",      "FaceButtons", 2000, 510, 50 },
        { BindingDef::Button, "Square",      "Buttons", "Pad1", "Y",     "SDL-0/FaceWest",      "FaceButtons", 1800, 510, 50 },
        { BindingDef::Button, "Triangle",    "Buttons", "Pad1", "X",     "SDL-0/FaceNorth",     "FaceButtons", 1900, 410, 50 },
        // Shoulders — top corners
        { BindingDef::Button, "L",           "Shoulders", "Pad1", "L",   "SDL-0/LeftShoulder",  "Shoulders",    130, 150, 60 },
        { BindingDef::Button, "R",           "Shoulders", "Pad1", "R",   "SDL-0/RightShoulder", "Shoulders",   2240, 150, 60 },
        // System — Start + Select centered low
        { BindingDef::Button, "Start",       "System",    "Pad1", "Start",  "SDL-0/Start",      "System",      1450, 870, 35 },
        { BindingDef::Button, "Select",      "System",    "Pad1", "Select", "SDL-0/Back",       "System",      1330, 870, 35 },
    };
}
```

- [ ] **Step 3: Rebuild the test target**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake --build . --target test_ppsspp_libretro_bindings -j8
```

Expected: build succeeds.

- [ ] **Step 4: Run the test and confirm all 5 slots pass**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && ctest -R "^PpssppLibretroBindings$" --output-on-failure
```

Expected:
```
1/1 Test #N: PpssppLibretroBindings ......... Passed
100% tests passed, 0 tests failed out of 1
```

- [ ] **Step 5: Run the full test suite to confirm no regression**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake --build . -j8 && ctest -j4
```

Expected: 41 of 42 tests pass; the one pre-existing failure is `HotkeyDefs::duckstation_completeness` (actual 99 vs expected 102 — drift from commit `54964c4`, not caused by Phase A). Confirm the new test `PpssppLibretroBindings` is in the pass list.

- [ ] **Step 6: Commit (Tasks 1 + 2 together — they're one logical change)**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project && git add \
    cpp/src/adapters/libretro/ppsspp_libretro_adapter.h \
    cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp \
    cpp/tests/test_ppsspp_libretro_bindings.cpp \
    cpp/CMakeLists.txt && \
git commit -m "$(cat <<'EOF'
feat(ppsspp): wire controllerBindingDefsForType for PSP

Phase A of the post-task-#6 PPSSPP libretro fill-in. The skeleton
PpssppLibretroAdapter previously returned the base-class default
({}) for controller bindings, so the Settings -> PSP -> Controllers
card existed but was empty. Add a 12-row table mirroring the
Pcsx2LibretroAdapter pattern minus L2/R2/L3/R3 (PSP has none) and
with L1/R1 renamed to L/R. PlayStation face conventions: Cross->B,
Circle->A, Square->Y, Triangle->X. Spotlight coords target PSP.svg
viewBox 0 0 2367 1014 — first-cut estimates to be tuned visually
in a follow-up commit.

Regression guard test_ppsspp_libretro_bindings asserts:
- row count = 12
- every .key resolves via retroPadSlotFromKey (no silent runtime drops)
- every .cardSlot is in the six-value allowed set
- every .section is "Pad1"
- face-button RetroPad mapping matches PlayStation convention

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Tune spotlight coordinates against the SVG

**Files:**
- Modify: `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp` (only the spotlight ints)

This is a **visual tuning loop**. The Task 2 coordinates were estimates. Now adjust them so each spotlight visually lines up on PSP.svg.

- [ ] **Step 1: Rebuild the full app**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake --build . --target RetroNest -j8
```

- [ ] **Step 2: Launch RetroNest**

Run:

```bash
~/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest
```

Navigate: Settings (Escape or Start) → PSP → Controllers → Standard / PSP Controller card. The PSP.svg renders with binding rows down one side. Focusing a binding row draws an overlay circle on the SVG at `(spotlightX, spotlightY)` with radius `spotlightR`.

- [ ] **Step 3: Tune each row's coords**

For each of the 12 rows, focus its card and observe whether the spotlight circle covers the physical button on the artwork. If not, edit `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`, rebuild (`cmake --build . --target RetroNest -j8`), relaunch, and re-check.

Tuning checklist (cross off as each one is visually correct):

- [ ] D-Pad Up — center on the up arrow
- [ ] D-Pad Down — center on the down arrow
- [ ] D-Pad Left — center on the left arrow
- [ ] D-Pad Right — center on the right arrow
- [ ] Cross — center on the × button
- [ ] Circle — center on the ○ button
- [ ] Square — center on the □ button
- [ ] Triangle — center on the △ button
- [ ] L — center on the left shoulder
- [ ] R — center on the right shoulder
- [ ] Start — center on the Start text
- [ ] Select — center on the Select text

Radius guidance: pick a radius that hugs the physical button edge — large enough to read as "this button", small enough not to bleed into neighbours. ~45–60 for face/d-pad buttons, ~30–40 for the small Start/Select tabs, ~55–70 for shoulders.

- [ ] **Step 4: Re-run the regression test (sanity-check)**

The coord tuning shouldn't change row count or `.key`/`.cardSlot` values, but re-run to be sure:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake --build . --target test_ppsspp_libretro_bindings -j8 && ctest -R "^PpssppLibretroBindings$" --output-on-failure
```

Expected: all 5 slots pass.

- [ ] **Step 5: Commit the tuned coordinates**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project && git add cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp && \
git commit -m "$(cat <<'EOF'
feat(ppsspp): tune controller-binding spotlight coords against PSP.svg

Tuned visually against the actual SVG in the Settings -> PSP ->
Controllers card. All 12 spotlight overlays now land on their
physical button on the PSP-1000 artwork.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

If no coords needed adjustment, skip this commit (Task 2's commit stands alone).

---

### Task 4: Runtime smoke-test the bindings

This verifies the launch-time SDL → RetroPad wiring still works after Phase A — same code path as before, but now also reading our 12 entries. Requires a connected gamepad.

- [ ] **Step 1: Launch RetroNest and start a PSP game**

```bash
~/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest
```

Pick any PSP ROM (DBZ - Shin Budokai is known-working from task #6 smoke test).

- [ ] **Step 2: Verify each button reaches the game**

In the game, exercise each binding:

- [ ] D-Pad — character/menu cursor moves in all 4 directions
- [ ] Cross — confirm / primary action
- [ ] Circle — cancel / back
- [ ] Square — secondary action
- [ ] Triangle — secondary action / menu
- [ ] L — shoulder-bound action
- [ ] R — shoulder-bound action
- [ ] Start — pause menu (the in-game PSP one, not RetroNest's overlay)
- [ ] Select — secondary menu

- [ ] **Step 3: Verify the in-game menu hotkey still works**

Press Cmd+Shift+Escape (or Select+Start combo). Confirm RetroNest's in-game menu opens. This proves the hotkey path is independent of the controller-binding changes.

- [ ] **Step 4: Exit cleanly and confirm no regression in launch logs**

Close the game from the in-game menu. The libretro core shuts down. Check stderr for `[GameSession]`, `[SdlInput]`, `[InputRouter]` log lines — none should report "unbound" or "unknown key" errors.

---

## Done criteria

All four checkboxes:

- [ ] `test_ppsspp_libretro_bindings` passes (all 5 slots)
- [ ] Full ctest suite stays at 41/42 pass (the 1 pre-existing `HotkeyDefs` failure is untouched)
- [ ] PSP.svg shows correct spotlight on each of the 12 bindings in the Controllers settings page
- [ ] All 12 buttons reach the game during a real PSP-game smoke test

Two commits land on `main` (or three if Task 3 needed a tune-up commit):
1. `feat(ppsspp): wire controllerBindingDefsForType for PSP`
2. (optional) `feat(ppsspp): tune controller-binding spotlight coords against PSP.svg`

---

## Out of scope (saved for later phases)

- Settings schema + hub cards (Phase B)
- Resolution + aspect ratio options (Phase C)
- BIOS / assets audit (Phase D)
- Frontend setting defaults (Phase E)
- Audit pass (Phase F)
- PSP-specific hotkeys (Phase G)
- Analog nub binding UI — RetroPad axes are runtime-routed, not part of `BindingDef`. If the user ever wants a sensitivity / deadzone UI for the nub, that lives in Phase B (settings schema), not here.
- A second controller type — PSP only has one physical layout. If we ever expose a USB-controller-as-PSP profile, add a new entry to `controllerTypes()` and branch in `controllerBindingDefsForType` on the `type` argument.
