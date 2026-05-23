# SP3: Dolphin Libretro Host Adapter Scaffold — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the standalone `DolphinAdapter` in RetroNest with `DolphinLibretroAdapter`, flip the manifest to `backend: "libretro"`, and install the SP2-built `dolphin_libretro.dylib` so RetroNest can launch the libretro core through its normal in-app game-launch flow.

**Architecture:** New `DolphinLibretroAdapter` mirrors `MgbaLibretroAdapter`'s minimal shape (smallest of the three existing libretro adapters). Only the required overrides + `MetalNSView` hardware render backend. Settings schema and full controller mapping are deliberately deferred to SP6/7 and SP5 — this is the integration scaffold, not feature parity. The standalone `dolphin_adapter.{h,cpp}` (~2490 LOC) is deleted in the same commit chain.

**Tech Stack:** Qt 6 (Q_OBJECT), C++20, RetroNest's `LibretroAdapter` base class + shared libretro runtime infrastructure.

**Parent spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-23-dolphin-libretro-conversion-design.md`

**Predecessors:**
- SP1: `dolphin_libretro.dylib` skeleton — builds + dlopens.
- SP2: BootManager / EmuThread / Audio / Input wired. `dolphin_libretro.dylib` accepts a GC ROM via `retro_load_game`; Core reaches `Running` state. Frame production + clean teardown are SP3 follow-up items per the SP2 close-out commit.

**Working directory:** `/Users/mark/Documents/Projects/RetroNest-Project/`. Branch matches whatever the user's existing libretro work uses (recent commits show `main`). All SP3 git work in this tree — `dolphin-libretro/` is untouched.

---

## Scope and non-scope

**In scope for SP3:**
- Create `DolphinLibretroAdapter` with minimum-viable overrides (`coreId`, `hardwareRenderBackend`, `raConsoleId`).
- Empty/placeholder for everything else (`settingsSchema`, `controllerTypes`, `pathsDefs`, etc. all return `{}` for SP3).
- Flip `manifests/dolphin.json` to libretro backend, point at `dolphin_libretro.dylib`.
- Delete `dolphin_adapter.{h,cpp}` and the two standalone-only test files.
- Install the SP2-built dylib to `{emulators-dir}/libretro/cores/dolphin_libretro.dylib`.
- Verify RetroNest builds clean and can launch a GC ROM through the normal game flow (clicking a tile, not the smoke harness).

**Explicitly deferred:**
- Full settings schema → SP6/7.
- Real controller mapping UI → SP5.
- Vulkan render backend → SP4.
- Frame production debug (carried from SP2's close-out) — SP3 may surface a fix as a side effect; if not, becomes a focused debug ticket.
- `extractSerial` for resume support — SP3 ships with `findResumeFile` returning `{}` (base default), so Save & Quit + Resume isn't wired yet.

---

## File structure

| File | Status | Responsibility |
|---|---|---|
| `cpp/src/adapters/libretro/dolphin_libretro_adapter.h` | Create | Class declaration. ~30 LOC, mirrors `MgbaLibretroAdapter.h`. |
| `cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp` | Create | Skeleton implementation. Empty/minimal returns for SP3. |
| `manifests/dolphin.json` | Modify | Flip `backend` → `"libretro"`, add `core_dylib` + `core_buildbot_path`, remove standalone-only fields. |
| `cpp/src/adapters/adapter_registry.cpp` | Modify | Swap registration to `DolphinLibretroAdapter`. |
| `cpp/src/adapters/dolphin_adapter.h` | **Delete** | Standalone replaced. |
| `cpp/src/adapters/dolphin_adapter.cpp` | **Delete** | Standalone replaced. |
| `cpp/tests/test_dolphin_schema.cpp` | **Delete** | Tests the standalone schema; new tests would be for SP6/7. |
| `cpp/tests/test_dolphin_controller_schema.cpp` | **Delete** | Tests the standalone controller schema; new tests would be for SP5. |
| `cpp/CMakeLists.txt` | Modify | Add new adapter source, remove standalone adapter + its test entries. |
| `{emulators-dir}/libretro/cores/dolphin_libretro.dylib` | Install | The SP2-built dylib copied into RetroNest's expected core path. |

---

### Task 1: Create the DolphinLibretroAdapter class

**Files:**
- Create: `cpp/src/adapters/libretro/dolphin_libretro_adapter.h`
- Create: `cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp`

Minimum-viable adapter. Mirrors the smallest existing libretro adapter (`MgbaLibretroAdapter` at 41 LOC header), with `hardwareRenderBackend()` switched to `MetalNSView` since SP2's dylib uses that path.

- [ ] **Step 1: Write the header**

Create `cpp/src/adapters/libretro/dolphin_libretro_adapter.h`:

```cpp
#pragma once
#include "libretro_adapter.h"

// Skeleton-phase DolphinLibretroAdapter.
//
// LibretroAdapter declares coreId() pure-virtual; the registry only
// instantiates concrete subclasses, so even though SP3 ships a minimal
// surface (most overrides return empty), we need a named class to
// register. SP4 (Vulkan path), SP5 (controllers), SP6/7 (settings), and
// SP8 (achievements + polish) fill in the remaining overrides
// incrementally.
//
// Replaces the standalone DolphinAdapter that previously launched
// Dolphin.app as an external process. The standalone adapter and its
// tests are deleted in the same commit chain.
class DolphinLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "dolphin"; }

    HardwareRenderBackend hardwareRenderBackend() const override {
        // SP2 built the Metal NSView handover path. SP4 will switch to
        // Vulkan when that work lands; until then the core only supports
        // the Metal path on macOS.
        return HardwareRenderBackend::MetalNSView;
    }

    // GameCube = RC_CONSOLE_GAMECUBE (16); Wii = RC_CONSOLE_WII (19).
    // RetroNest's RA console mapping in cpp/src/core/ra_client.cpp already
    // contains the gc/wii string→id entries.
    int raConsoleId(const QString& systemId) const override;

    // SP3 ships with this base default (empty). SP5 expands to GameCube
    // pad + Wii Classic Controller bindings. Until then, the controller
    // mapping page hides Dolphin entirely (controllerTypes() == {}).
    // No controllerTypes() override — base returns {}.

    // SP3: no save-on-exit / resume yet. SP6.5-equivalent (resume wiring)
    // becomes its own follow-up sub-task once extractSerial() is
    // implemented for GC/Wii disc headers. Base findResumeFile() returns {}.

    // SP6/7: settings schema. Base settingsSchema() returns {} — the
    // emulator's settings page renders empty for SP3.

    // SP3 doesn't expose any per-emulator path overrides yet. SP8 adds
    // Save States, Memory Cards (GC), NAND (Wii), Screenshots.
    // Base pathsDefs() returns {}.
};
```

- [ ] **Step 2: Write the implementation**

Create `cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp`:

```cpp
#include "dolphin_libretro_adapter.h"

int DolphinLibretroAdapter::raConsoleId(const QString& systemId) const {
    if (systemId == "gc")
        return 16;
    if (systemId == "wii")
        return 19;
    return 0;
}
```

That's it. Three files plus the comment header — under 60 LOC total. Everything else is base-default until later sub-projects.

- [ ] **Step 3: Stage only**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/dolphin_libretro_adapter.{h,cpp}
```

Report — controller commits with message:
```
SP3: DolphinLibretroAdapter skeleton

Minimum-viable adapter — coreId() = "dolphin", hardwareRenderBackend() =
MetalNSView, raConsoleId() maps gc→16 and wii→19. Everything else takes
the LibretroAdapter base defaults (empty settings, no controller types,
no path overrides). SP5/6/7/8 fill those in.
```

---

### Task 2: Update CMakeLists to include the new adapter source

**Files:**
- Modify: `cpp/CMakeLists.txt`

Add the new adapter to SOURCES + HEADERS. Don't touch the standalone references in this task — Task 5 deletes those wholesale.

- [ ] **Step 1: Locate the libretro adapter entries**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
grep -n "pcsx2_libretro_adapter\|ppsspp_libretro_adapter\|mgba_libretro_adapter" cpp/CMakeLists.txt | head -10
```

Expected: each adapter (`pcsx2`, `ppsspp`, `mgba`) appears in both the main SOURCES list and HEADERS list. There may also be entries in test target source lists.

- [ ] **Step 2: Add Dolphin libretro adapter to SOURCES + HEADERS**

In `cpp/CMakeLists.txt`, find the line listing `src/adapters/libretro/ppsspp_libretro_adapter.cpp` in the main SOURCES (will be near other adapter files). Add immediately after:
```cmake
src/adapters/libretro/dolphin_libretro_adapter.cpp
```

Find the matching HEADERS section listing `src/adapters/libretro/ppsspp_libretro_adapter.h`. Add immediately after:
```cmake
src/adapters/libretro/dolphin_libretro_adapter.h
```

Keep alphabetical order if other adapters are sorted (dolphin before mgba/pcsx2/ppsspp).

- [ ] **Step 3: Verify cmake still configures**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake -B cpp/build-arm64 -S cpp 2>&1 | tail -5
```

Expected: clean reconfigure. If there are pre-existing build issues from earlier session state, they're not ours.

- [ ] **Step 4: Stage only**

```bash
git add cpp/CMakeLists.txt
```

Commit message:
```
SP3: register dolphin_libretro_adapter in cpp/CMakeLists.txt

Adds the new adapter to SOURCES + HEADERS. Standalone DolphinAdapter
references are removed in a later task.
```

---

### Task 3: Flip the manifest to libretro backend

**Files:**
- Modify: `manifests/dolphin.json`

Switch from standalone-process fields to libretro-core fields. Match the shape of `ppsspp.json` / `pcsx2.json`.

- [ ] **Step 1: Read the existing manifests**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cat manifests/{dolphin,ppsspp,pcsx2}.json
```

Confirm the libretro shape:
- `id`, `name`, `description`, `systems` — unchanged
- `backend: "libretro"`
- `core_dylib: "<name>_libretro.dylib"`
- `core_buildbot_path: "<name>_libretro.dylib.zip"` (or similar — confirm by inspecting ppsspp/pcsx2)
- `rom_extensions` — unchanged

Standalone-only fields to remove from dolphin.json:
- `github_repo`
- `executable`
- `install_folder`
- `launch_args`

- [ ] **Step 2: Rewrite manifests/dolphin.json**

Replace the whole file with:

```json
{
  "id": "dolphin",
  "name": "Dolphin",
  "description": "Dolphin is a GameCube and Wii emulator. Play GC/Wii games in HD with save states, controller support, and per-game configurations.",
  "systems": ["gc", "wii"],
  "backend": "libretro",
  "core_dylib": "dolphin_libretro.dylib",
  "core_buildbot_path": "dolphin_libretro.dylib.zip",
  "rom_extensions": ["iso", "gcm", "gcz", "ciso", "wbfs", "rvz", "wad", "wia", "nkit", "m3u", "dol", "elf", "tgc"]
}
```

If `ppsspp.json`'s libretro field names differ (e.g. `core` instead of `core_dylib`), match those exactly. The investigation found `core_dylib` + `core_buildbot_path` — verify against the actual file.

- [ ] **Step 3: Stage only**

```bash
git add manifests/dolphin.json
```

Commit message:
```
SP3: flip dolphin manifest to libretro backend

Backend = "libretro", core_dylib = "dolphin_libretro.dylib". Removes
the standalone-only fields (github_repo, executable, install_folder,
launch_args). The id stays "dolphin" — the adapter registry swap in a
following commit keeps the same key.
```

---

### Task 4: Swap the adapter registration

**Files:**
- Modify: `cpp/src/adapters/adapter_registry.cpp`

Switch the `"dolphin"` registration to `DolphinLibretroAdapter`. The id stays the same so manifests and existing game data don't need to change.

- [ ] **Step 1: Inspect current registration**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
grep -n "DolphinAdapter\|dolphin_adapter\.h" cpp/src/adapters/adapter_registry.cpp
```

Expected: two lines — an `#include` and the `registerAdapter(...)` call. Note both line numbers.

- [ ] **Step 2: Update the include + registration**

In `cpp/src/adapters/adapter_registry.cpp`:

- Replace `#include "dolphin_adapter.h"` with:
  ```cpp
  #include "libretro/dolphin_libretro_adapter.h"
  ```

- Replace:
  ```cpp
  registerAdapter("dolphin", std::make_unique<DolphinAdapter>());
  ```
  with:
  ```cpp
  registerAdapter("dolphin", std::make_unique<DolphinLibretroAdapter>());
  ```

- [ ] **Step 3: Stage only**

```bash
git add cpp/src/adapters/adapter_registry.cpp
```

Commit message:
```
SP3: register DolphinLibretroAdapter under id="dolphin"

The standalone DolphinAdapter and its include go away in the following
delete-the-old commit. Id stays "dolphin" so manifests and existing
game data don't need to change.
```

---

### Task 5: Delete the standalone DolphinAdapter + its tests

**Files:**
- **Delete**: `cpp/src/adapters/dolphin_adapter.h`
- **Delete**: `cpp/src/adapters/dolphin_adapter.cpp`
- **Delete**: `cpp/tests/test_dolphin_schema.cpp`
- **Delete**: `cpp/tests/test_dolphin_controller_schema.cpp`
- Modify: `cpp/CMakeLists.txt` (remove old adapter + test entries)

Big delete. ~2490 LOC of standalone adapter + two test files. The replacement was already added in Task 1.

- [ ] **Step 1: Confirm no remaining references**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
grep -rn "DolphinAdapter\b\|dolphin_adapter\.h" cpp/src/ cpp/tests/ qml/ 2>/dev/null
```

Expected: only matches in the four files we're about to delete + any test runner manifests that list them. Anything else is a missed reference — report it instead of deleting.

If a reference appears in a file that shouldn't be deleted (e.g. `cpp/src/services/something.cpp`), report STATUS=BLOCKED with the location — that's an SP3 plan gap.

- [ ] **Step 2: Delete the four files**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git rm cpp/src/adapters/dolphin_adapter.h \
       cpp/src/adapters/dolphin_adapter.cpp \
       cpp/tests/test_dolphin_schema.cpp \
       cpp/tests/test_dolphin_controller_schema.cpp
```

- [ ] **Step 3: Remove CMakeLists entries**

Find every reference in `cpp/CMakeLists.txt`:
```bash
grep -n "dolphin_adapter\|test_dolphin_schema\|test_dolphin_controller_schema" cpp/CMakeLists.txt
```

Remove each matched line. There's likely:
- One in main SOURCES (`src/adapters/dolphin_adapter.cpp`)
- One in main HEADERS (`src/adapters/dolphin_adapter.h`)
- One `add_executable` block per test (`add_executable(test_dolphin_schema ...)` and `add_executable(test_dolphin_controller_schema ...)`) — these become orphaned with the test source files gone. Delete the entire `add_executable` block + its `target_link_libraries`/`add_test` calls for each.
- The standalone `dolphin_adapter.cpp` may also appear in other test target source lists (the investigation found ~7 such references). Remove from each.

Use multiple `sed -i` or precise edits — easier to spot-edit each occurrence.

- [ ] **Step 4: Verify cmake still configures**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake -B cpp/build-arm64 -S cpp 2>&1 | tail -10
```

Expected: clean configure. If it complains about a missing `dolphin_adapter.cpp`, we missed a reference — find + remove it.

- [ ] **Step 5: Verify build is clean**

```bash
cmake --build cpp/build-arm64 2>&1 | tail -15
```

Expected: clean build of all targets except the deleted tests (which no longer exist). If any build target fails compilation, the missing piece is likely a `#include "dolphin_adapter.h"` somewhere we missed.

- [ ] **Step 6: Stage the deletions + CMakeLists changes**

```bash
git add -A cpp/CMakeLists.txt cpp/src/adapters/ cpp/tests/
git status --short | head -20
```

Should show: 4 file deletions (`D`), 1 modification (`M`) on CMakeLists.

Commit message:
```
SP3: delete standalone DolphinAdapter + its tests

Removes:
- cpp/src/adapters/dolphin_adapter.{h,cpp}        (~2490 LOC)
- cpp/tests/test_dolphin_schema.cpp               (settings schema test)
- cpp/tests/test_dolphin_controller_schema.cpp   (controller bindings test)
- All corresponding CMakeLists entries

Equivalent test coverage for the libretro adapter ships with SP5 (controllers)
and SP6/7 (settings).
```

---

### Task 6: Install the SP2 dylib to RetroNest's core path

**Files:** None modified. This is a deployment step.

RetroNest looks for libretro cores at `{emulators-dir}/libretro/cores/{manifest.core_dylib}`. The SP2 build produced `dolphin_libretro.dylib` in the Dolphin build tree; copy it where RetroNest can find it.

- [ ] **Step 1: Locate RetroNest's emulators directory**

Inspect what `Paths::emulatorsDir("libretro")` resolves to at runtime. Usually one of:
- `~/Documents/RetroNest/emulators/libretro/cores/` (user-data root)
- `~/Library/Application Support/RetroNest/emulators/libretro/cores/` (macOS app-support)
- A path baked into the cpp config

Check by running RetroNest briefly OR by inspecting `cpp/src/core/paths.cpp`:
```bash
grep -n "emulatorsDir\b\|libretro/cores\|libretro\".*cores" cpp/src/core/paths.cpp 2>/dev/null | head -5
```

If it's not obvious, look at where existing libretro cores live (PPSSPP, PCSX2, mGBA):
```bash
find ~/Documents/RetroNest ~/Library/Application\ Support/RetroNest 2>/dev/null -name "*_libretro.dylib" | head -5
```

Record the path as `$CORES_DIR`.

- [ ] **Step 2: Copy the dylib**

```bash
SRC=/Users/mark/Documents/Projects/dolphin-libretro/build-libretro/Source/Core/DolphinLibretro/dolphin_libretro.dylib
DEST="$CORES_DIR/dolphin_libretro.dylib"
mkdir -p "$(dirname "$DEST")"
cp -v "$SRC" "$DEST"
ls -la "$DEST"
```

Expected: ~19MB dylib in place. Verify the file size + that it's executable.

- [ ] **Step 3: Nothing to stage**

This is a runtime install, not a tracked artifact. SP8 will revisit and figure out a proper packaging story for the bundled dylib (where it ships in the RetroNest installer / DMG).

Report the destination path so the next task can reference it.

---

### Task 7: Build + launch verification

**Files:** None modified. This is verification.

Build RetroNest with the changes, launch a GC ROM through the normal in-app flow, observe what happens.

- [ ] **Step 1: Build RetroNest**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-arm64 2>&1 | tail -10
```

Expected: clean build. The new `dolphin_libretro_adapter.cpp` compiles, no references to the deleted standalone files.

- [ ] **Step 2: Launch RetroNest**

The user can launch via the usual mechanism. If unsure how:
```bash
ls cpp/build-arm64/AppUI/*.app 2>/dev/null
open cpp/build-arm64/AppUI/RetroNest.app
```

Wait for the main UI to appear.

- [ ] **Step 3: Verify Dolphin appears in the emulator list**

- Navigate to wherever emulator status is shown (Settings → Emulators, or the SetupWizard).
- Confirm: Dolphin is shown.
- Confirm: it's marked as "installed" (the dylib path resolves) — NOT showing an install button or "missing" state. If it's showing as missing, the install-status check is reading the wrong path; cross-reference Task 6's destination against `LibretroAdapter::isInstalled` logic.

- [ ] **Step 4: Launch a GC ROM**

- Pick the same Twilight Princess RVZ used for SP2 smoke (or any other GC title).
- Click the tile to launch.
- Observe.

**Expected outcomes (any of these counts as SP3 done):**
- A. **Game boots and renders** — Twilight Princess shows the GC boot logo, then the game. Audio plays. Input works. SP3 + SP2's frame-production gap both resolved together.
- B. **Game boots but renders nothing** (matches SP2 smoke behavior) — Dolphin enters Running state, the libretro overlay appears, but the game area is black. Audio may or may not work. SP3's integration is verified end-to-end; frame-production gap from SP2 carries forward as a focused follow-up.
- C. **Launch fails with a specific error** — adapter registers, manifest resolves, but launching fails. Capture the error: log output, RetroNest's UI message. This is a real bug to triage.

- [ ] **Step 5: Capture the launch attempt**

Save whatever output is visible — RetroNest's log file, console output, any toast/error messages. Path is usually:
```bash
ls -lt ~/Library/Logs/RetroNest/ 2>/dev/null | head -3
tail -100 ~/Library/Logs/RetroNest/latest.log 2>/dev/null
```

Or whatever log path the existing app uses.

- [ ] **Step 6: Report**

Report:
- Outcome category (A/B/C above)
- The first 30 lines of relevant log output (Dolphin libretro Environment::Log entries, RetroNest's launch path)
- Any UI state worth noting (overlay rendered? black screen? error toast?)

---

### Task 8: Close out

**Files:** None modified.

- [ ] **Step 1: Verify all deliverables**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
echo "=== New adapter ==="
ls -1 cpp/src/adapters/libretro/dolphin_libretro_adapter.{h,cpp}
echo ""
echo "=== Manifest ==="
cat manifests/dolphin.json
echo ""
echo "=== Deleted standalone ==="
ls cpp/src/adapters/dolphin_adapter.{h,cpp} 2>&1 | head -2
ls cpp/tests/test_dolphin_schema.cpp 2>&1 | head -1
ls cpp/tests/test_dolphin_controller_schema.cpp 2>&1 | head -1
echo ""
echo "=== Installed dylib ==="
find ~/Documents/RetroNest ~/Library/Application\ Support/RetroNest 2>/dev/null -name "dolphin_libretro.dylib" -exec ls -la {} \;
echo ""
echo "=== SP3 commits ==="
git log --oneline -8
```

Expected:
- Adapter source + header present
- Manifest has `backend: "libretro"`
- The four deleted files are gone
- Dylib installed at the expected path
- 5-6 SP3 commits

- [ ] **Step 2: Report**

- SP3 launch result (A/B/C from Task 7).
- Whether frame production resolved on its own or needs SP3.5 / SP4 follow-up.
- Note what's deferred to SP4 (Vulkan), SP5 (controllers), SP6/7 (settings), SP8 (RA + polish).

- [ ] **Step 3: Done.**

---

## Notes for the implementer

- **All git work in `RetroNest-Project/`.** SP3 is host-side only.
- **The dylib install (Task 6) is a runtime artifact, not a tracked file.** SP8 will sort the proper packaging story.
- **If the SP2 frame-production gap remains visible in Task 7 (outcome B):** that's expected per the SP2 close-out commit. SP3 still ships — its gate is "RetroNest launches the core end-to-end via normal flow," which outcome B satisfies. Frame production becomes a focused debug task (likely SP3.5, or rolled into SP4 as part of the Vulkan path investigation).
- **Don't add settings, controllers, or path overrides** even if it feels easy — those are explicitly deferred to keep SP3 small. The plan's whole point is "integration scaffold with minimum surface, ship fast."
- **If a deletion in Task 5 surfaces a reference we missed**, STOP, report the location, don't paper over. The standalone adapter shouldn't be referenced from anywhere we haven't already deleted; if it is, that's a real coupling worth understanding before nuking.
- **Commit cadence**: 5 commits expected — adapter create, cmake add, manifest flip, registry swap, delete-standalone. Plus the install + verify steps which don't produce commits.
