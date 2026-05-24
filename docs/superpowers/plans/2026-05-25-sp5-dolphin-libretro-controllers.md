# SP5: Dolphin Libretro Controller Mapping — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose GameCube + Wii Classic controllers in RetroNest's controller-mapping UI for the Dolphin libretro adapter — both controllers appear on the mapping page, every button/stick has a labelled spotlight on the controller SVG, and remapping persists across launches. Verify a physical gamepad actually drives gameplay (digital + analog).

**Architecture:** `DolphinLibretroAdapter` (host side) gains `controllerTypes()` + `controllerBindingDefsForType()`, mirroring `Pcsx2LibretroAdapter`'s pattern: `BindingDef` entries whose `.key` field is a RetroPad slot/axis the shared `InputRouter` resolves, `.defaultValue` is the `SDL-0/...` physical-input convention, and spotlight coordinates light up the controller SVG. The core-side `LibretroInputSource` (built in SP2) already reads RetroPad state each frame; SP5 is host-UI work plus confirming the core's GC-pad↔RetroPad default binding is correct.

**Tech Stack:** Qt 6, C++20, RetroNest's `BindingDef` / `ControllerTypeDef` / `InputRouter` (`RetroPadSlot` + `RetroPadAxis`), the existing controller-mapping QML UI.

**Parent spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-23-dolphin-libretro-conversion-design.md`

**Predecessors:** SP0–SP3 done. GameCube games boot, render (Metal), and play audio inside RetroNest. SP2 built `LibretroInputSource` (core-side RetroPad→Dolphin-pad bridge) but input has NOT been verified to actually drive gameplay yet.

**Working directory:** `/Users/mark/Documents/Projects/RetroNest-Project/` on `main` (host-side work). The `dolphin-libretro` core tree (`libretro` branch) is only touched if Task 1 reveals the core's default GC-pad binding needs fixing.

**Build/run:** see memory `dolphin-libretro-build-setup.md` — universal dylib via lipo, Sys-dir copy, macdeployqt+codesign after RetroNest rebuild, run x86_64 under Rosetta.

---

## Key facts established during prep

- **`BindingDef` field order** (`cpp/src/core/binding_def.h`): `{ Kind kind, QString label, QString group, QString section, QString key, QString defaultValue, QString cardSlot, int spotlightX, int spotlightY, int spotlightR }`. `kind` ∈ `{Button, Axis}`.
- **`ControllerTypeDef`** (`cpp/src/core/controller_type_def.h`): `{ QString id, QString displayName, QString svgResource, QHash<QString,QString> slotTitleOverrides = {} }`.
- **RetroPad slot keys** (`cpp/src/core/libretro/input_router.h`, `retroPadSlotFromKey`): `B Y Select Start Up Down Left Right A X L R L2 R2 L3 R3` (the only valid `.key` values for digital `BindingDef::Button` in the libretro path).
- **RetroPad axes** (`RetroPadAxis`): `LeftX LeftY RightX RightY L2 R2`. Analog routing IS wired end-to-end — `core_runtime.cpp:147` handles `RETRO_DEVICE_ANALOG`, the router applies radial deadzone. So GC main-stick / C-stick / analog-triggers can route.
- **SVGs already exist**: `:/AppUI/qml/AppUI/images/controllers/GameCube.svg` (viewBox 0 0 1799 1368) and `:/AppUI/qml/AppUI/images/controllers/Wii_classiccontroller.svg` (viewBox 0 0 2340 1182). Spotlight coordinates were calibrated by the deleted standalone adapter — reused below.
- **Critical translation**: the deleted standalone `DolphinAdapter` used Dolphin's NATIVE keys (`"Buttons/A"`, backtick `` `Button S` ``) + sections (`GCPad1`/`Wiimote1`). The libretro path is DIFFERENT — `.key` must be a RetroPad slot/axis, `.defaultValue` is `SDL-0/...`, `.section` is `Pad1`. We reuse the standalone's labels + groups + spotlight coords, but NOT its keys/defaults.
- **PCSX2 reference** (`cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`) — the canonical libretro binding-def shape. PCSX2 ships digital-only; SP5's analog `BindingDef::Axis` entries are new ground (Task 4 investigates the controls.ini analog path).

---

## GC / Wii → RetroPad mapping (the convention SP5 uses)

This must match the core's fixed GC-pad↔RetroPad binding (confirmed in Task 1). Standard RetroArch-Dolphin mapping:

| GameCube | RetroPad slot/axis | `.key` | `.defaultValue` |
|---|---|---|---|
| A | B (south) | `B` | `SDL-0/FaceSouth` |
| B | A (east) | `A` | `SDL-0/FaceEast` |
| X | Y (west) | `Y` | `SDL-0/FaceWest` |
| Y | X (north) | `X` | `SDL-0/FaceNorth` |
| Start | Start | `Start` | `SDL-0/Start` |
| Z | R (shoulder) | `R` | `SDL-0/RightShoulder` |
| L (digital) | L (shoulder) | `L` | `SDL-0/LeftShoulder` |
| R (digital) | L2 | `L2` | `SDL-0/+LeftTrigger` |
| D-Pad U/D/L/R | Up/Down/Left/Right | `Up`… | `SDL-0/DPad*` |
| Main Stick | Left analog | axis `LeftX/LeftY` | `SDL-0/LeftStick*` |
| C-Stick | Right analog | axis `RightX/RightY` | `SDL-0/RightStick*` |

(Z→R and GC-R→L2 is the pragmatic assignment that keeps both shoulders + Z usable; Task 1 confirms it against the core's actual binding and the table is adjusted if needed.)

---

## File structure

| File | Status | Responsibility |
|---|---|---|
| `cpp/src/adapters/libretro/dolphin_libretro_adapter.h` | Modify | Declare `controllerTypes()` + `controllerBindingDefsForType()`. |
| `cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp` | Modify | Implement both; two private free-function helpers `gcPadLibretroBindings()` + `wiiClassicLibretroBindings()`. |
| `cpp/tests/test_dolphin_libretro_controller_schema.cpp` | Create | Verify every `BindingDef.key` resolves to a valid RetroPad slot/axis; both types present; spotlights non-degenerate. |
| `cpp/CMakeLists.txt` | Modify | Add the new test target (mirror `test_pcsx2_libretro_controller_schema` if it exists, else the nearest controller-schema test). |
| `dolphin-libretro` core (only if Task 1 fails) | Maybe | Fix the GC-pad default profile so Dolphin's pad reads the `Libretro/0` virtual device. |

---

### Task 1: Verify input drives gameplay (digital + analog) — foundational

**Files:** Investigation; possibly modify the core's default-pad-profile code in `dolphin-libretro` if input is dead.

This is make-or-break. SP2 wired `LibretroInputSource` (a virtual `Libretro/0` device exposing RetroPad buttons + axes) but never confirmed Dolphin's GameCube pad actually READS from it. Dolphin binds its GC pad via `GCPadNew.ini`; if no profile binds `GCPad1` to `Libretro/0/...`, the pad sees nothing and the game ignores all input.

- [ ] **Step 1: Launch a GC game and test a physical gamepad**

Build/deploy per memory `dolphin-libretro-build-setup.md`, launch RetroNest x86_64, load a GC game (Twilight Princess), get past the Health & Safety screen.

With a connected gamepad, test:
- Does any face button advance past "Press any button to continue"?
- In-game: do the d-pad / face buttons do anything?
- Does the left analog stick move the character?

Record which of {digital buttons, analog stick} work or don't.

- [ ] **Step 2: If input is dead, inspect the core's pad binding**

Run with `RETRONEST_DOLPHIN_LOG=1` and check `/tmp/retronest.log` for the controller-interface device list — confirm `Added device: Libretro/0/0` appears (it did in SP3 logs). Then check whether Dolphin's `GCPadNew.ini` (under `/tmp/dolphin-libretro-user/Config/`) binds `[GCPad1]` to `Libretro/0`:
```bash
cat /tmp/dolphin-libretro-user/Config/GCPadNew.ini 2>/dev/null
```
If the file is missing or doesn't reference `Libretro/0`, the core needs to write a default GC-pad profile at boot that binds GCPad1 buttons to the `Libretro/0` device's RetroPad inputs (Buttons/A→`Libretro/0/B`, Main Stick→`Libretro/0/Left X±`, etc.). This is core-side work in `dolphin-libretro` (likely in `LibretroInputSource` or `EmuThread::StartGame`). Implement the minimal default-profile writer if needed, rebuild the dylib (both arches + lipo), and re-test.

**STOP and report after Step 2** with: what works, what doesn't, and whether a core-side fix was needed. The GC↔RetroPad mapping you confirm here drives the exact `.key`/label pairing in Tasks 3-5 (adjust the mapping table above if reality differs).

- [ ] **Step 3: Confirm analog routing specifically**

Move the left stick slowly. If the character walks (not just runs/snaps), the radial-deadzone analog path works. If digital-only (snap to full tilt or nothing), note it — analog `BindingDef::Axis` entries in Task 4 depend on this path being live.

- [ ] **Step 4: No commit unless a core fix was made**

If Step 2 required a core-side default-profile fix, commit it on the `libretro` branch of `dolphin-libretro` with a clear message. Otherwise nothing to commit — proceed to Task 2.

---

### Task 2: Add controllerTypes() to DolphinLibretroAdapter

**Files:**
- Modify: `cpp/src/adapters/libretro/dolphin_libretro_adapter.h`
- Modify: `cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp`

- [ ] **Step 1: Add the include + declarations to the header**

In `dolphin_libretro_adapter.h`, add the include near the top (after `#include "libretro_adapter.h"`):
```cpp
#include "core/binding_def.h"
#include "core/controller_type_def.h"
```
Add to the class public section (after `raConsoleId`):
```cpp
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
```

- [ ] **Step 2: Implement controllerTypes() in the .cpp**

Append to `dolphin_libretro_adapter.cpp`:
```cpp
QVector<ControllerTypeDef> DolphinLibretroAdapter::controllerTypes() const {
    return {
        { "GCPad1", "GameCube Controller",
          ":/AppUI/qml/AppUI/images/controllers/GameCube.svg", {} },
        { "WiiClassic", "Wii Classic Controller",
          ":/AppUI/qml/AppUI/images/controllers/Wii_classiccontroller.svg", {} },
    };
}
```
Note: type id `"WiiClassic"` (not the standalone's `"Wiimote1"`) — libretro controls.ini sections are adapter-private; using a clean id avoids confusion with Dolphin's native section naming. Confirm `controllerBindingsSection()` (base `LibretroAdapter`) maps it sanely; if the base needs the id echoed, that's fine.

- [ ] **Step 3: Build (controllerBindingDefsForType still undefined — link will fail)**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -10
```
Expected: compile error/undefined `controllerBindingDefsForType`. That's fine — Task 3 defines it. (Or stub it returning `{}` to get a clean build between tasks.)

- [ ] **Step 4: Stage only**

```bash
git add cpp/src/adapters/libretro/dolphin_libretro_adapter.{h,cpp}
```
Commit message (paired with Task 3):
```
SP5: DolphinLibretroAdapter controllerTypes() — GCPad + Wii Classic

Two controller types surfaced to the mapping UI, reusing the existing
GameCube.svg + Wii_classiccontroller.svg assets.
```

---

### Task 3: GameCube binding defs (digital + analog)

**Files:**
- Modify: `cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp`

Translate the deleted standalone adapter's GC layout to the libretro convention: keep labels/groups/cardSlots/spotlight-coords, swap keys → RetroPad slots/axes and defaults → `SDL-0/...`.

- [ ] **Step 1: Add the GC bindings helper (anonymous namespace at top of .cpp)**

```cpp
namespace {
QVector<BindingDef> gcPadLibretroBindings() {
    // GameCube.svg viewBox 0 0 1799 1368. Spotlight coords reused from the
    // (deleted) standalone DolphinAdapter's calibration. section "Pad1",
    // keys are RetroPad slots/axes, defaults are SDL-0/... physical inputs.
    return {
        // D-Pad
        {BindingDef::Button, "Up",    "D-Pad", "Pad1", "Up",    "SDL-0/DPadUp",    "DPad", 632, 740, 50},
        {BindingDef::Button, "Down",  "D-Pad", "Pad1", "Down",  "SDL-0/DPadDown",  "DPad", 632, 902, 50},
        {BindingDef::Button, "Left",  "D-Pad", "Pad1", "Left",  "SDL-0/DPadLeft",  "DPad", 557, 820, 50},
        {BindingDef::Button, "Right", "D-Pad", "Pad1", "Right", "SDL-0/DPadRight", "DPad", 707, 821, 50},
        // Face buttons (GC label / RetroPad slot)
        {BindingDef::Button, "A", "Face Buttons", "Pad1", "B", "SDL-0/FaceSouth", "FaceButtons", 1430, 438, 90},
        {BindingDef::Button, "B", "Face Buttons", "Pad1", "A", "SDL-0/FaceEast",  "FaceButtons", 1233, 543, 60},
        {BindingDef::Button, "X", "Face Buttons", "Pad1", "Y", "SDL-0/FaceWest",  "FaceButtons", 1626, 403, 65},
        {BindingDef::Button, "Y", "Face Buttons", "Pad1", "X", "SDL-0/FaceNorth", "FaceButtons", 1390, 250, 65},
        // Main Stick → RetroPad left analog (offset 80 from centre 367,438)
        {BindingDef::Axis, "Main Stick Up",    "Main Stick", "Pad1", "LeftY-", "SDL-0/-LeftStickY", "LeftAnalog", 367, 358, 70},
        {BindingDef::Axis, "Main Stick Down",  "Main Stick", "Pad1", "LeftY+", "SDL-0/+LeftStickY", "LeftAnalog", 367, 518, 70},
        {BindingDef::Axis, "Main Stick Left",  "Main Stick", "Pad1", "LeftX-", "SDL-0/-LeftStickX", "LeftAnalog", 287, 438, 70},
        {BindingDef::Axis, "Main Stick Right", "Main Stick", "Pad1", "LeftX+", "SDL-0/+LeftStickX", "LeftAnalog", 447, 438, 70},
        // C-Stick → RetroPad right analog (offset 80 from centre 1162,822)
        {BindingDef::Axis, "C-Stick Up",    "C-Stick", "Pad1", "RightY-", "SDL-0/-RightStickY", "RightAnalog", 1162, 742, 70},
        {BindingDef::Axis, "C-Stick Down",  "C-Stick", "Pad1", "RightY+", "SDL-0/+RightStickY", "RightAnalog", 1162, 902, 70},
        {BindingDef::Axis, "C-Stick Left",  "C-Stick", "Pad1", "RightX-", "SDL-0/-RightStickX", "RightAnalog", 1082, 822, 70},
        {BindingDef::Axis, "C-Stick Right", "C-Stick", "Pad1", "RightX+", "SDL-0/+RightStickX", "RightAnalog", 1242, 822, 70},
        // Triggers + Z
        {BindingDef::Button, "L (digital)", "Triggers", "Pad1", "L",  "SDL-0/LeftShoulder",  "Shoulders", 290, 100, 80},
        {BindingDef::Axis,   "L-Analog",    "Triggers", "Pad1", "L2", "SDL-0/+LeftTrigger",  "Shoulders", 290, 100, 80},
        {BindingDef::Button, "R (digital)", "Triggers", "Pad1", "R",  "SDL-0/RightShoulder", "Shoulders", 1517, 78, 80},
        {BindingDef::Axis,   "R-Analog",    "Triggers", "Pad1", "R2", "SDL-0/+RightTrigger", "Shoulders", 1517, 78, 80},
        {BindingDef::Button, "Z",           "Triggers", "Pad1", "R3", "SDL-0/RightStick",    "Shoulders", 1430, 100, 50},
        // System
        {BindingDef::Button, "Start", "System", "Pad1", "Start",  "SDL-0/Start", "System", 920, 420, 35},
    };
}
}  // namespace
```

**Note:** the `LeftY-`/`LeftX+` axis-key convention + `SDL-0/±LeftStickX` defaults are PROVISIONAL — Task 4 confirms how the controls.ini parser + `InputRouter` expect axis bindings to be keyed. Adjust this whole analog block to match Task 4's findings before relying on it.

- [ ] **Step 2: Build to confirm it compiles**

```bash
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -8
```
Expected: compiles (controllerBindingDefsForType still references the helper, defined in Task 5's dispatch — temporarily call `return gcPadLibretroBindings();` unconditionally to compile, or wait for Task 5).

- [ ] **Step 3: Stage only**

```bash
git add cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp
```
Commit message:
```
SP5: GameCube controller binding defs (digital + analog)

24 BindingDefs translated from the deleted standalone adapter's calibrated
GameCube.svg layout to the libretro convention (RetroPad slot/axis keys,
SDL-0/... defaults, Pad1 section). Digital buttons + main-stick/C-stick
analog + analog triggers.
```

---

### Task 4: Confirm the analog-axis binding mechanism

**Files:** Investigation; adjust Task 3's analog block + Task 5's Wii analog block accordingly.

PCSX2 ships digital-only bindings, so `BindingDef::Axis` in the libretro controls.ini path is unverified. Confirm how axis bindings are keyed + parsed before trusting Task 3's analog entries.

- [ ] **Step 1: Trace the controls.ini → InputRouter axis path**

```bash
grep -rn "RetroPadAxis\|retroPadAxisFromKey\|setAxis\|BindingDef::Axis\|\.kind == .*Axis\|isAxis" cpp/src/core/ cpp/src/services/ | head -25
```
Find: does a `retroPadAxisFromKey()` exist mapping `"LeftX"`/`"LeftY"`/`"RightX"`/`"RightY"`/`"L2"`/`"R2"` strings to `RetroPadAxis`? How does the controls.ini writer/reader handle `BindingDef::Axis` entries vs `Button`? How is polarity (`-`/`+`) represented?

- [ ] **Step 2: Reconcile Task 3's keys with reality**

If the parser expects axis keys WITHOUT polarity suffixes (e.g. `"LeftX"` with polarity in the value) rather than `"LeftX-"`, rewrite Task 3's 8 analog BindingDefs (and Wii's in Task 5) to match. If analog binding through controls.ini isn't supported at all, fall back to: keep analog sticks as fixed default routing (works in-game per Task 1, but not user-remappable), and either drop the analog `BindingDef::Axis` entries or mark them display-only. Document the decision in a comment.

- [ ] **Step 3: Report findings + any rewrite**

Report whether analog bindings are remappable through the UI or fixed-routing-only, and what convention the axis `.key`/`.defaultValue` use. Stage any Task 3 corrections.

```bash
git add cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp
```
Commit message:
```
SP5: reconcile analog axis binding keys with InputRouter/controls.ini

<describe: confirmed axis key convention is X; rewrote GC analog defs to
match / analog is fixed-routing so marked display-only>
```

---

### Task 5: Wii Classic binding defs + dispatch

**Files:**
- Modify: `cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp`

- [ ] **Step 1: Add the Wii Classic bindings helper (anon namespace)**

```cpp
namespace {
QVector<BindingDef> wiiClassicLibretroBindings() {
    // Wii_classiccontroller.svg viewBox 0 0 2340 1182. Spotlight coords reused
    // from the deleted standalone adapter. Same libretro key convention as GC.
    // Apply the SAME analog-axis key convention Task 4 settled on.
    return {
        // D-Pad (cluster centre 461,459)
        {BindingDef::Button, "Up",    "D-Pad", "Pad1", "Up",    "SDL-0/DPadUp",    "DPad", 461, 344, 50},
        {BindingDef::Button, "Down",  "D-Pad", "Pad1", "Down",  "SDL-0/DPadDown",  "DPad", 461, 574, 50},
        {BindingDef::Button, "Left",  "D-Pad", "Pad1", "Left",  "SDL-0/DPadLeft",  "DPad", 345, 459, 50},
        {BindingDef::Button, "Right", "D-Pad", "Pad1", "Right", "SDL-0/DPadRight", "DPad", 577, 459, 50},
        // Face buttons
        {BindingDef::Button, "A", "Face Buttons", "Pad1", "A", "SDL-0/FaceEast",  "FaceButtons", 2108, 460, 80},
        {BindingDef::Button, "B", "Face Buttons", "Pad1", "B", "SDL-0/FaceSouth", "FaceButtons", 1883, 633, 80},
        {BindingDef::Button, "X", "Face Buttons", "Pad1", "X", "SDL-0/FaceNorth", "FaceButtons", 1883, 289, 80},
        {BindingDef::Button, "Y", "Face Buttons", "Pad1", "Y", "SDL-0/FaceWest",  "FaceButtons", 1659, 461, 80},
        // Left stick (centre 857,838, offset 100)
        {BindingDef::Axis, "Left Stick Up",    "Left Stick", "Pad1", "LeftY-", "SDL-0/-LeftStickY", "LeftAnalog", 857, 738, 80},
        {BindingDef::Axis, "Left Stick Down",  "Left Stick", "Pad1", "LeftY+", "SDL-0/+LeftStickY", "LeftAnalog", 857, 938, 80},
        {BindingDef::Axis, "Left Stick Left",  "Left Stick", "Pad1", "LeftX-", "SDL-0/-LeftStickX", "LeftAnalog", 757, 838, 80},
        {BindingDef::Axis, "Left Stick Right", "Left Stick", "Pad1", "LeftX+", "SDL-0/+LeftStickX", "LeftAnalog", 957, 838, 80},
        // Right stick (centre 1481,837, offset 100)
        {BindingDef::Axis, "Right Stick Up",    "Right Stick", "Pad1", "RightY-", "SDL-0/-RightStickY", "RightAnalog", 1481, 737, 80},
        {BindingDef::Axis, "Right Stick Down",  "Right Stick", "Pad1", "RightY+", "SDL-0/+RightStickY", "RightAnalog", 1481, 937, 80},
        {BindingDef::Axis, "Right Stick Left",  "Right Stick", "Pad1", "RightX-", "SDL-0/-RightStickX", "RightAnalog", 1381, 837, 80},
        {BindingDef::Axis, "Right Stick Right", "Right Stick", "Pad1", "RightX+", "SDL-0/+RightStickX", "RightAnalog", 1581, 837, 80},
        // Triggers (L/R full + ZL/ZR)
        {BindingDef::Button, "L (digital)", "Triggers", "Pad1", "L",  "SDL-0/LeftShoulder",  "Shoulders", 370, 80, 70},
        {BindingDef::Axis,   "L-Analog",    "Triggers", "Pad1", "L2", "SDL-0/+LeftTrigger",  "Shoulders", 370, 80, 70},
        {BindingDef::Button, "ZL",          "Triggers", "Pad1", "L3", "SDL-0/LeftStick",     "Shoulders", 570, 80, 60},
        {BindingDef::Button, "R (digital)", "Triggers", "Pad1", "R",  "SDL-0/RightShoulder", "Shoulders", 1970, 80, 70},
        {BindingDef::Axis,   "R-Analog",    "Triggers", "Pad1", "R2", "SDL-0/+RightTrigger", "Shoulders", 1970, 80, 70},
        {BindingDef::Button, "ZR",          "Triggers", "Pad1", "R3", "SDL-0/RightStick",    "Shoulders", 1770, 80, 60},
        // System
        {BindingDef::Button, "Minus", "System", "Pad1", "Select", "SDL-0/Back",  "System", 996,  459, 50},
        {BindingDef::Button, "Plus",  "System", "Pad1", "Start",  "SDL-0/Start", "System", 1343, 459, 50},
    };
}
}  // namespace
```
(Wii "Home" has no clean RetroPad slot left — omitted; can map to a hotkey later. ZL/ZR reuse L3/R3 slots since the GC mapping used R3 for Z — note both controllers share the same `Pad1` controls.ini, so a user remapping one stick-click affects both; acceptable for v1, document it.)

- [ ] **Step 2: Implement the dispatch + remove any temporary unconditional return**

```cpp
QVector<BindingDef> DolphinLibretroAdapter::controllerBindingDefsForType(const QString& type) const {
    if (type == "WiiClassic")
        return wiiClassicLibretroBindings();
    return gcPadLibretroBindings();  // "GCPad1" or empty default
}
```

- [ ] **Step 3: Build**

```bash
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -8
```
Expected: clean build.

- [ ] **Step 4: Stage only**

```bash
git add cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp
```
Commit message:
```
SP5: Wii Classic controller binding defs + type dispatch

controllerBindingDefsForType routes WiiClassic→wiiClassicLibretroBindings,
else GCPad. Wii Classic layout calibrated against Wii_classiccontroller.svg.
```

---

### Task 6: Controller-schema test

**Files:**
- Create: `cpp/tests/test_dolphin_libretro_controller_schema.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Check for a sibling test to mirror**

```bash
ls cpp/tests/test_pcsx2_libretro_controller_schema.cpp cpp/tests/test_*controller_schema.cpp 2>/dev/null
grep -n "controller_schema" cpp/CMakeLists.txt | head
```
Mirror the closest existing controller-schema test's structure + its CMake `add_executable`/`add_test` block.

- [ ] **Step 2: Write the test**

```cpp
#include <QtTest>
#include "adapters/libretro/dolphin_libretro_adapter.h"
#include "core/libretro/input_router.h"

class TestDolphinLibretroControllerSchema : public QObject {
    Q_OBJECT
private slots:
    void bothControllerTypesPresent() {
        DolphinLibretroAdapter a;
        const auto types = a.controllerTypes();
        QCOMPARE(types.size(), 2);
        QVERIFY(types[0].id == "GCPad1");
        QVERIFY(types[1].id == "WiiClassic");
        for (const auto& t : types)
            QVERIFY(!t.svgResource.isEmpty());
    }
    void everyDigitalKeyResolvesToRetroPadSlot() {
        DolphinLibretroAdapter a;
        for (const QString type : {QStringLiteral("GCPad1"), QStringLiteral("WiiClassic")}) {
            for (const auto& b : a.controllerBindingDefsForType(type)) {
                if (b.kind != BindingDef::Button) continue;
                // retroPadSlotFromKey returns a valid slot (>=0) for known keys.
                QVERIFY2(retroPadSlotFromKey(b.key) >= RetroPadSlot::B,
                         qPrintable("unresolved digital key: " + b.key + " in " + type));
            }
        }
    }
    void spotlightsNonDegenerate() {
        DolphinLibretroAdapter a;
        for (const QString type : {QStringLiteral("GCPad1"), QStringLiteral("WiiClassic")}) {
            for (const auto& b : a.controllerBindingDefsForType(type)) {
                if (b.label == "Rumble/Motor") continue;  // 0,0,0 sentinel allowed
                QVERIFY2(b.spotlightR > 0,
                         qPrintable("degenerate spotlight: " + b.label + " in " + type));
            }
        }
    }
};
QTEST_MAIN(TestDolphinLibretroControllerSchema)
#include "test_dolphin_libretro_controller_schema.moc"
```
Adjust `retroPadSlotFromKey`'s return-type comparison to match its actual signature (found in Task 4 / input_router.h). If axis keys also need validation, add a `retroPadAxisFromKey` check for `BindingDef::Axis` entries per Task 4's findings.

- [ ] **Step 3: Add the test target to CMakeLists**

Mirror the sibling controller-schema test's `add_executable(...)` + source list + `target_link_libraries(... Qt6::Test ...)` + `add_test(...)`. The source list needs `dolphin_libretro_adapter.cpp` + `libretro_adapter.cpp` + `input_router.cpp` + whatever the sibling links.

- [ ] **Step 4: Build + run the test**

```bash
cmake -B cpp/build-arm64 -S cpp 2>&1 | tail -3
cmake --build cpp/build-arm64 --target test_dolphin_libretro_controller_schema 2>&1 | tail -8
ctest --test-dir cpp/build-arm64 -R DolphinLibretroControllerSchema --output-on-failure
```
Expected: 3 tests pass.

- [ ] **Step 5: Stage only**

```bash
git add cpp/tests/test_dolphin_libretro_controller_schema.cpp cpp/CMakeLists.txt
```
Commit message:
```
SP5: controller-schema test for DolphinLibretroAdapter

Verifies both controller types present with SVGs, every digital BindingDef
key resolves to a RetroPad slot, and spotlights are non-degenerate.
```

---

### Task 7: Build, deploy, UI + in-game verification

**Files:** None modified. Verification.

- [ ] **Step 1: Full RetroNest build + deploy**

```bash
cmake --build cpp/build-arm64 2>&1 | tail -5
# x86_64 too (per memory build-setup) + macdeployqt + codesign before launch
```
Re-run macdeployqt + `codesign --force --deep --sign -` on the x86_64 app (memory `dolphin-libretro-build-setup.md` pitfall).

- [ ] **Step 2: Controller-mapping UI check**

Launch RetroNest, open the controller-mapping page for Dolphin. Confirm:
- Both "GameCube Controller" and "Wii Classic Controller" appear (selectable).
- Each shows its SVG with spotlights on every button/stick when you focus a binding row.
- The labels read as GC/Wii names (A/B/X/Y/Z/Start, Main Stick, C-Stick, etc.).

- [ ] **Step 3: Remap + persist check**

Rebind one button (e.g. GC A → a different physical button). Confirm it writes to the controls.ini (`emulators/libretro/dolphin/controls.ini`). Restart RetroNest, reopen mapping — the rebind persists.

- [ ] **Step 4: In-game check**

Launch a GC game, confirm the remapped control works in-game and the analog stick still moves the character (Task 1's analog path intact).

- [ ] **Step 5: Report**

Report: both types shown ✓, spotlights ✓, remap persists ✓, in-game input ✓. Note anything deferred (e.g. analog remap if Task 4 found it fixed-routing-only, Wii Home button).

---

### Task 8: Close out

- [ ] **Step 1: Verify deliverables**
```bash
git -C /Users/mark/Documents/Projects/RetroNest-Project log --oneline -7
git -C /Users/mark/Documents/Projects/RetroNest-Project status --short | grep -v '^??'
```
Expected: SP5 commits present, tracked tree clean.

- [ ] **Step 2: Report + stage SP6.** SP6 (settings schema — Graphics) is next. Note any SP5 deferrals for later.

---

## Notes for the implementer

- **Task 1 is the gate.** If a physical gamepad doesn't drive gameplay, the UI work is cosmetic. Confirm digital + analog input first; fix the core's default GC-pad profile if needed (that's `dolphin-libretro` `libretro` branch, not RetroNest).
- **Translate, don't copy.** The deleted standalone adapter's bindings used Dolphin native keys + backtick expressions. SP5 uses RetroPad slot/axis keys + `SDL-0/...` defaults + `Pad1` section. Reuse only labels/groups/cardSlots/spotlight-coords.
- **Analog binding is new ground** (Task 4). PCSX2 never did `BindingDef::Axis`. Confirm the controls.ini axis convention before trusting the analog blocks; fall back to fixed-routing display-only if unsupported.
- **Single controls.ini shared by both controller types** — both use section `Pad1`. A user editing one stick-click slot affects both GC and Wii mappings. Acceptable for v1; document it.
- Commits: subagent stages, controller commits (auto-mode blocks subagent commits — see prior SPs).
- Build/run specifics all in memory `dolphin-libretro-build-setup.md`.
