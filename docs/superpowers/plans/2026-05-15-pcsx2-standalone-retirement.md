# SP8 — PCSX2 Standalone Retirement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete the PCSX2 standalone adapter path; rename the libretro adapter from id `pcsx2-libretro` to `pcsx2` with one-shot user-data and DB migration on first launch.

**Architecture:** Two adapter classes exist today (`PCSX2Adapter` standalone + `Pcsx2LibretroAdapter`). After SP8 only the libretro adapter survives, registered under id `"pcsx2"`. User data under `emulators/pcsx2-libretro/` is moved to `emulators/pcsx2/` (with the existing standalone install archived under `emulators/.archive/`). A new schema-v7 DB migration rewrites every `emulator_id = 'pcsx2-libretro'` row to `'pcsx2'`.

**Tech Stack:** Qt 6 / C++17 / CMake. Existing `Database::runMigrations` framework for DB version bumps. New thin filesystem migration module called from `main.cpp` after `Paths::setRoot`.

**Spec:** `docs/superpowers/specs/2026-05-15-pcsx2-standalone-retirement-design.md`

---

## Pre-flight (manual, do before Task 1)

Take backups (the migration step in Task 7 archives one but a separate copy is cheap insurance):

```bash
TS=$(date +%Y%m%d-%H%M%S)
cp -R ~/Documents/RetroNest/emulators/pcsx2          ~/Desktop/sp8-backup-$TS-pcsx2-standalone
cp -R ~/Documents/RetroNest/emulators/pcsx2-libretro ~/Desktop/sp8-backup-$TS-pcsx2-libretro
cp    ~/Documents/RetroNest/config/retronest.db      ~/Desktop/sp8-backup-$TS-retronest.db 2>/dev/null || true
```

Repo state required: working tree clean on `main` branch; `origin/main` pushed. From `cd /Users/mark/Documents/Projects/RetroNest-Project`:

```bash
git status      # expect: nothing to commit, working tree clean
git rev-parse --abbrev-ref HEAD   # expect: main
git fetch && git status -sb        # expect: ## main...origin/main
```

---

## Task 1: Manifest collapse + adapter registry id swap

**Files:**
- Delete: `manifests/pcsx2.json`
- Rename + edit: `manifests/pcsx2-libretro.json` → `manifests/pcsx2.json`
- Modify: `cpp/src/adapters/adapter_registry.cpp` (lines 2, 17, 22)

- [ ] **Step 1: Delete the standalone manifest**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git rm manifests/pcsx2.json
```

- [ ] **Step 2: Move the libretro manifest into the standalone's slot**

```bash
git mv manifests/pcsx2-libretro.json manifests/pcsx2.json
```

- [ ] **Step 3: Edit the new `manifests/pcsx2.json`**

Open `manifests/pcsx2.json`. Replace the file's full contents with:

```json
{
  "id": "pcsx2",
  "name": "PCSX2",
  "description": "PlayStation 2 emulator (libretro core)",
  "systems": ["ps2"],
  "github_repo": "markpearce/pcsx2-retronest",
  "executable": "pcsx2_libretro.dylib",
  "install_folder": "libretro",
  "rom_extensions": ["iso", "chd", "cso", "bin", "cue", "m3u", "gz"],
  "launch_args": [],
  "backend": "libretro",
  "core_dylib": "pcsx2_libretro.dylib",
  "core_buildbot_path": "pcsx2_libretro.dylib.zip"
}
```

- [ ] **Step 4: Edit `cpp/src/adapters/adapter_registry.cpp`**

Remove line 2 (`#include "pcsx2_adapter.h"`).
Remove the line that reads `registerAdapter("pcsx2", std::make_unique<PCSX2Adapter>());` (currently line 17).
Change `registerAdapter("pcsx2-libretro", std::make_unique<Pcsx2LibretroAdapter>());` to `registerAdapter("pcsx2", std::make_unique<Pcsx2LibretroAdapter>());`.

After edit, the include block should look like:

```cpp
#include "adapter_registry.h"
#include "duckstation_adapter.h"
#include "ppsspp_adapter.h"
#include "dolphin_adapter.h"
#include "libretro/mgba_libretro_adapter.h"
#include "libretro/pcsx2_libretro_adapter.h"

#include <QDebug>
```

And the body of `registerBuiltinAdapters()` should be:

```cpp
void AdapterRegistry::registerBuiltinAdapters() {
    registerAdapter("duckstation", std::make_unique<DuckStationAdapter>());
    registerAdapter("ppsspp", std::make_unique<PPSSPPAdapter>());
    registerAdapter("dolphin", std::make_unique<DolphinAdapter>());
    registerAdapter("mgba", std::make_unique<MgbaLibretroAdapter>());
    registerAdapter("pcsx2", std::make_unique<Pcsx2LibretroAdapter>());
}
```

- [ ] **Step 5: Build will fail — that's expected**

`adapter_registry.cpp` no longer references `PCSX2Adapter`, but `CMakeLists.txt` still tries to compile `pcsx2_adapter.cpp` (and other call sites in `app_controller.cpp` still include `pcsx2/pcsx2_settings_dialog.h` which depends on the standalone adapter). The build break is intentional — Tasks 3 and 4 finish the chain.

Do NOT attempt to build at this step. Just verify the edits visually.

- [ ] **Step 6: Commit**

```bash
git add manifests/pcsx2.json cpp/src/adapters/adapter_registry.cpp
git commit -m "$(cat <<'EOF'
SP8 task 1: collapse pcsx2 manifest + swap adapter registry id

Deletes the standalone manifest, renames the libretro manifest to
pcsx2.json with id "pcsx2" and name "PCSX2" (drops the dev suffix).
adapter_registry now registers Pcsx2LibretroAdapter under "pcsx2".

Build is intentionally broken until tasks 3-4 land.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Unify settings-dialog dispatch in app_controller

**Files:**
- Modify: `cpp/src/ui/app_controller.cpp` (lines 17, 22, 458–488)

- [ ] **Step 1: Remove the libretro-flavor settings dialog include**

In `cpp/src/ui/app_controller.cpp`, delete line 22:

```cpp
#include "settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.h"
```

(The standalone include at line 17 — `#include "settings/pcsx2/pcsx2_settings_dialog.h"` — stays for now. Task 4 will repoint it after the file rename in that task.)

Actually, **leave the line-17 include AS-IS**. The standalone dialog header gets deleted in Task 4 (along with its `.cpp`), but the libretro-flavor dialog gets renamed into the same `settings/pcsx2/pcsx2_settings_dialog.h` slot. By the end of Task 4, the include path is correct.

So in Task 2 you only delete line 22 (the `_libretro` include).

- [ ] **Step 2: Collapse the two pcsx2 dispatch branches**

In `cpp/src/ui/app_controller.cpp`, in `showEmulatorSettings`, find the block:

```cpp
    if (emuId == QLatin1String("pcsx2")) {
        auto* dialog = new Pcsx2SettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
```

Leave it as-is — `Pcsx2SettingsDialog` will be the class name of the renamed libretro-flavor dialog after Task 4.

Then find this block lower in the function:

```cpp
    if (emuId == QLatin1String("pcsx2-libretro")) {
        auto* dialog = new Pcsx2LibretroSettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
```

**Delete this entire block** (5 lines including the trailing blank line). Post-rename there is no `pcsx2-libretro` id — the `"pcsx2"` branch above handles everything.

- [ ] **Step 3: Build will still fail — Task 4 finishes the chain**

The standalone dialog class `Pcsx2SettingsDialog` is currently the one being referenced here, and `pcsx2_settings_dialog.h` still resolves to the standalone header. The build error from Task 1 (`PCSX2Adapter` missing) plus this state are all resolved together at the end of Task 4.

Visual verification only — no build.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/app_controller.cpp
git commit -m "$(cat <<'EOF'
SP8 task 2: collapse pcsx2 settings-dialog dispatch

Removes the dedicated pcsx2-libretro dispatch branch and its include.
The "pcsx2" branch above now handles the single remaining PCSX2 path
once tasks 3-4 land. Build still broken intentionally.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Delete standalone source files

**Files:**
- Delete:
  - `cpp/src/adapters/pcsx2_adapter.h`
  - `cpp/src/adapters/pcsx2_adapter.cpp`
  - `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.h`
  - `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp`
  - `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.h`
  - `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.cpp`
- Modify: `cpp/CMakeLists.txt` (remove standalone refs)

- [ ] **Step 1: Delete the 6 standalone source files**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git rm cpp/src/adapters/pcsx2_adapter.h cpp/src/adapters/pcsx2_adapter.cpp
git rm cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.h \
       cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp \
       cpp/src/ui/settings/pcsx2/pcsx2_category_hub.h \
       cpp/src/ui/settings/pcsx2/pcsx2_category_hub.cpp
```

Verify the `cpp/src/ui/settings/pcsx2/` directory is now empty:

```bash
ls cpp/src/ui/settings/pcsx2/ 2>/dev/null
# Expected: nothing (empty dir, or directory gone if git rm cleaned it up)
```

If non-empty, investigate before continuing.

- [ ] **Step 2: Remove standalone `pcsx2_adapter.cpp` and the four standalone-dialog source/header lines from `cpp/CMakeLists.txt`**

Open `cpp/CMakeLists.txt`. Find and remove these lines (line numbers approximate — match by content):

Around line 63:
```
    src/adapters/pcsx2_adapter.cpp
```
Delete this single line.

Around lines 123–124:
```
    src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
    src/ui/settings/pcsx2/pcsx2_category_hub.cpp
```
Delete both lines.

Around line 146:
```
    src/adapters/pcsx2_adapter.h
```
Delete this line.

Around lines 223–224:
```
    src/ui/settings/pcsx2/pcsx2_settings_dialog.h
    src/ui/settings/pcsx2/pcsx2_category_hub.h
```
Delete both lines.

Then find and remove every other `src/adapters/pcsx2_adapter.cpp` line in test targets and binary targets (use a search; expect occurrences around lines 467, 490, 707, 759, 798). Delete each one — total ~5 additional occurrences in test/binary block source lists.

- [ ] **Step 3: Update the three pcsx2 test targets to compile against the libretro adapter**

In `cpp/CMakeLists.txt`, find each of the three `add_executable(test_pcsx2_*)` blocks (around lines 658, 671, 684). In **each** block, replace:

```
    src/adapters/pcsx2_adapter.cpp
```

with:

```
    src/adapters/libretro/pcsx2_libretro_adapter.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/frontend_settings_store.cpp
    src/core/libretro/retro_log.cpp
    src/core/libretro/rcheevos_runtime.cpp
```

(Pattern matches the binary targets that link `pcsx2_libretro_adapter.cpp` — visible at line 713 and 804 in the original CMakeLists.)

Verify by inspection of the diff that each of the three test blocks now compiles with the libretro adapter and its full dependency chain.

- [ ] **Step 4: Build the standalone-free tree**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
./scripts/build-universal.sh 2>&1 | tail -50
```

Expected: build succeeds. If it fails with `PCSX2Adapter` undefined or `Pcsx2LibretroSettingsDialog` undefined, double-check that Tasks 1 and 2 are committed and the libretro dialog files (rename happens in Task 4) are still present — they should be, since Task 4 hasn't run yet, so the file paths `cpp/src/ui/settings/pcsx2_libretro/...` still exist and the includes in `app_controller.cpp` line 22 was deleted in Task 2. The `app_controller.cpp` "pcsx2" branch references `Pcsx2SettingsDialog` class — that doesn't exist yet (it's the renamed-in-Task-4 libretro dialog).

**Expected failure mode at this point:** `Pcsx2SettingsDialog: undeclared identifier` from app_controller.cpp. That's correct — Task 4's directory rename creates the class.

Document the actual failure output, then proceed to Task 4.

- [ ] **Step 5: Commit**

```bash
git add cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP8 task 3: delete pcsx2 standalone source files

Removes pcsx2_adapter.{h,cpp}, pcsx2_settings_dialog.{h,cpp},
pcsx2_category_hub.{h,cpp} (the standalone variants) and drops every
reference from CMakeLists.txt. The three pcsx2 test targets are
retargeted to compile against pcsx2_libretro_adapter.cpp + its
libretro dependency chain.

Build still broken until Task 4 renames the libretro-flavor dialog
into the now-empty settings/pcsx2/ slot.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Rename libretro-flavor settings dialog into the `pcsx2/` slot

**Files:**
- Rename:
  - `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.h` → `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.h`
  - `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp` → `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp`
  - `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.h` → `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.h`
  - `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` → `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.cpp`
- Modify (after rename): class names + internal includes + `adapterFor("pcsx2-libretro")` → `adapterFor("pcsx2")`
- Modify: `cpp/CMakeLists.txt` (the 4 path lines edited in Task 3 reference the OLD location — re-fix)

- [ ] **Step 1: git mv the four files**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git mv cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.h \
       cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.h
git mv cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp \
       cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
git mv cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.h \
       cpp/src/ui/settings/pcsx2/pcsx2_category_hub.h
git mv cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp \
       cpp/src/ui/settings/pcsx2/pcsx2_category_hub.cpp
```

Verify the old directory is now empty / gone:

```bash
ls cpp/src/ui/settings/pcsx2_libretro/ 2>/dev/null
# Expected: nothing
rmdir cpp/src/ui/settings/pcsx2_libretro 2>/dev/null
```

- [ ] **Step 2: Rename classes in the renamed header `pcsx2_settings_dialog.h`**

Open `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.h`. Apply edits:

- `Pcsx2LibretroSettingsDialog` → `Pcsx2SettingsDialog` (both class declaration and constructor).
- Update the file's doc comment to say "SP8: per-emulator settings dialog for PCSX2 (libretro-backed)."

Final file content:

```cpp
#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

// SP8: per-emulator settings dialog for PCSX2 (libretro-backed).
// Mirrors MgbaSettingsDialog — sets up chrome, attaches the
// category hub, and routes category clicks to a GenericSettingsPage
// rendering the matching SettingDef rows from the long-lived adapter.
class Pcsx2SettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private:
    void onCategoryActivated(const QString& category);
};
```

- [ ] **Step 3: Rename classes in the renamed header `pcsx2_category_hub.h`**

Open `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.h`. Apply edits:

- `Pcsx2LibretroCategoryHub` → `Pcsx2CategoryHub`.
- Update doc comment.

Final content:

```cpp
#pragma once
#include "ui/settings/emulator_category_hub_base.h"

// SP8: category hub for the PCSX2 per-emulator settings dialog.
// Only the libretro option rows declared in
// Pcsx2LibretroAdapter::settingsSchema() are reachable.
class Pcsx2CategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    explicit Pcsx2CategoryHub(QWidget* parent = nullptr);

private:
    int countSettings(const QString& category) const;
};
```

- [ ] **Step 4: Rename classes + paths in `pcsx2_settings_dialog.cpp`**

Open `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp`. Apply (use search-and-replace where unambiguous):

- First-line include: change `#include "pcsx2_libretro_settings_dialog.h"` → `#include "pcsx2_settings_dialog.h"`.
- Second include: `#include "pcsx2_libretro_category_hub.h"` → `#include "pcsx2_category_hub.h"`.
- All occurrences of `Pcsx2LibretroSettingsDialog::` and `Pcsx2LibretroSettingsDialog(` and `new Pcsx2LibretroSettingsDialog` → `Pcsx2SettingsDialog` equivalents.
- All occurrences of `Pcsx2LibretroCategoryHub` → `Pcsx2CategoryHub`.
- Line 34: `auto* adapter = AdapterRegistry::instance().adapterFor("pcsx2-libretro");` → `auto* adapter = AdapterRegistry::instance().adapterFor("pcsx2");`.

Verify there are no remaining `Pcsx2Libretro` (case-sensitive) tokens in the file:

```bash
grep -nE 'Pcsx2Libretro' cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
# Expected: no output
```

- [ ] **Step 5: Rename classes in `pcsx2_category_hub.cpp`**

Open `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.cpp`. Apply:

- First-line include: `#include "pcsx2_libretro_category_hub.h"` → `#include "pcsx2_category_hub.h"`.
- All `Pcsx2LibretroCategoryHub` → `Pcsx2CategoryHub`.

The line referring to the adapter:
```cpp
#include "adapters/libretro/pcsx2_libretro_adapter.h"
```
STAYS — that's the adapter class which is keeping its name. Do not rename `Pcsx2LibretroAdapter` anywhere; only the dialog/hub UI classes are being renamed.

Verify:

```bash
grep -nE 'Pcsx2LibretroCategoryHub|Pcsx2LibretroSettingsDialog' cpp/src/ui/settings/pcsx2/pcsx2_category_hub.cpp
# Expected: no output
```

- [ ] **Step 6: Fix `cpp/CMakeLists.txt` paths**

In `cpp/CMakeLists.txt`, find lines (post-Task-3 edits) that still reference the OLD libretro paths:

```
    src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp
    src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp
```

Replace with:

```
    src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
    src/ui/settings/pcsx2/pcsx2_category_hub.cpp
```

And the header pair:

```
    # pcsx2-libretro settings
    src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.h
    src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.h
```

Replace with:

```
    # PCSX2 settings
    src/ui/settings/pcsx2/pcsx2_settings_dialog.h
    src/ui/settings/pcsx2/pcsx2_category_hub.h
```

Verify no `pcsx2_libretro` path-name (the file/dir name, not the adapter symbol) remains in CMakeLists.txt:

```bash
grep -n 'pcsx2_libretro_settings_dialog\|pcsx2_libretro_category_hub\|settings/pcsx2_libretro' cpp/CMakeLists.txt
# Expected: no output (the adapter file pcsx2_libretro_adapter is at a different path and stays)
```

- [ ] **Step 7: Repoint hardcoded data dir in `pcsx2_libretro_adapter.cpp`**

Open `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`. Find line 57:

```cpp
    const QString dir = Paths::emulatorDataDir("pcsx2-libretro", "ps2") + "/savestates";
```

Change to:

```cpp
    const QString dir = Paths::emulatorDataDir("pcsx2", "ps2") + "/savestates";
```

Also update the explainer comments at lines ~51–52 (before the function):

```cpp
// emulators/pcsx2/ps2/savestates/. Look there. Base id is
// "pcsx2" (the manifest id used by Paths::emulatorDataDir on
```

(Original referenced `pcsx2-libretro` — repoint to `pcsx2`.)

Other explainer comments at lines ~499, 504, 575, 868 that mention "pcsx2-libretro" as a path are still describing the libretro core's source tree (the upstream repo), not RetroNest's data dir. **Leave those comments alone** — they're correctly referring to the libretro fork's source.

To double-check before moving on, run:

```bash
grep -nE 'emulatorDataDir.*"pcsx2-libretro"|emulatorsDir.*"pcsx2-libretro"' cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
# Expected: no output (every data-dir lookup uses "pcsx2" now)
```

- [ ] **Step 8: Build**

```bash
./scripts/build-universal.sh 2>&1 | tail -30
```

Expected: build succeeds. The full `app_controller → Pcsx2SettingsDialog → Pcsx2CategoryHub → Pcsx2LibretroAdapter` chain now resolves with the renamed classes and updated paths.

If the build fails, the most likely cause is a missed `Pcsx2LibretroSettingsDialog` or `Pcsx2LibretroCategoryHub` reference somewhere outside the four renamed files. Search:

```bash
grep -rnE 'Pcsx2LibretroSettingsDialog|Pcsx2LibretroCategoryHub' cpp/src cpp/tests
# Expected: no output
```

Fix any results found, then rebuild.

- [ ] **Step 9: Smoke check — app launches**

```bash
open /Users/mark/Documents/Projects/RetroNest-Project/build/RetroNest.app
```

Expected: app window appears. **Do not migrate user data yet** — that's Task 7. The library will currently show no PCSX2 games working under id `"pcsx2"` from `emulators/pcsx2-libretro/` (data dir mismatch), but the app itself should run.

Close the app.

- [ ] **Step 10: Commit**

```bash
git add cpp/CMakeLists.txt cpp/src/ui/settings/pcsx2/ cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP8 task 4: rename libretro-flavor settings dialog into pcsx2/ slot

Renames cpp/src/ui/settings/pcsx2_libretro/* into the now-empty
cpp/src/ui/settings/pcsx2/ slot, dropping the _libretro suffix on
files and on the Pcsx2LibretroSettingsDialog / Pcsx2LibretroCategoryHub
class names. Internal includes, CMakeLists paths, and the
adapterFor("pcsx2-libretro") lookup are updated to match. The adapter
class Pcsx2LibretroAdapter keeps its name (it's an implementation
detail).

Also repoints pcsx2_libretro_adapter.cpp's savestate path from
emulators/pcsx2-libretro/ to emulators/pcsx2/ in line with the new id.

Build passes; full user-data migration is task 7.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Retarget the three pcsx2 test files to the libretro adapter

**Files:**
- Modify: `cpp/tests/test_pcsx2_preview_spec.cpp`
- Modify: `cpp/tests/test_pcsx2_schema.cpp`
- Modify: `cpp/tests/test_pcsx2_controller_schema.cpp`

CMakeLists already retargeted in Task 3.

- [ ] **Step 1: Update `test_pcsx2_preview_spec.cpp`**

In `cpp/tests/test_pcsx2_preview_spec.cpp`:

- Replace `#include "adapters/pcsx2_adapter.h"` with `#include "adapters/libretro/pcsx2_libretro_adapter.h"`.
- Replace every `PCSX2Adapter` with `Pcsx2LibretroAdapter` (case-sensitive search-and-replace).

Verify:

```bash
grep -nE 'PCSX2Adapter|pcsx2_adapter\.h' cpp/tests/test_pcsx2_preview_spec.cpp
# Expected: no output
```

- [ ] **Step 2: Update `test_pcsx2_schema.cpp`**

In `cpp/tests/test_pcsx2_schema.cpp`:

- Replace `#include "adapters/pcsx2_adapter.h"` with `#include "adapters/libretro/pcsx2_libretro_adapter.h"`.
- Replace every `PCSX2Adapter` with `Pcsx2LibretroAdapter`.

Verify:

```bash
grep -nE 'PCSX2Adapter|pcsx2_adapter\.h' cpp/tests/test_pcsx2_schema.cpp
# Expected: no output
```

- [ ] **Step 3: Update `test_pcsx2_controller_schema.cpp`**

Same pattern:

- Replace `#include "adapters/pcsx2_adapter.h"` with `#include "adapters/libretro/pcsx2_libretro_adapter.h"`.
- Replace every `PCSX2Adapter` with `Pcsx2LibretroAdapter`.

Verify:

```bash
grep -nE 'PCSX2Adapter|pcsx2_adapter\.h' cpp/tests/test_pcsx2_controller_schema.cpp
# Expected: no output
```

- [ ] **Step 4: Build the test binaries**

```bash
cd build
cmake --build . --target test_pcsx2_preview_spec test_pcsx2_schema test_pcsx2_controller_schema 2>&1 | tail -30
cd ..
```

Expected: all three targets build successfully.

- [ ] **Step 5: Run the three tests**

```bash
cd build
ctest -R 'Pcsx2(PreviewSpec|Schema|ControllerSchema)' --output-on-failure
cd ..
```

Expected: all three pass. If any fail, the failure reveals a real parity gap between the deleted `PCSX2Adapter` and the surviving `Pcsx2LibretroAdapter` — investigate the assertion, fix the parity bug in the libretro adapter (do not weaken the assertion), then re-run.

- [ ] **Step 6: Run the full test suite to confirm no other test breaks**

```bash
cd build
ctest --output-on-failure 2>&1 | tail -40
cd ..
```

Expected: 100% passing. Pre-existing flaky/skipped tests are acceptable; new failures are not.

- [ ] **Step 7: Commit**

```bash
git add cpp/tests/test_pcsx2_preview_spec.cpp cpp/tests/test_pcsx2_schema.cpp cpp/tests/test_pcsx2_controller_schema.cpp
git commit -m "$(cat <<'EOF'
SP8 task 5: retarget 3 pcsx2 tests to libretro adapter

test_pcsx2_preview_spec, test_pcsx2_schema, test_pcsx2_controller_schema
now exercise Pcsx2LibretroAdapter instead of the deleted PCSX2Adapter.
Same assertions still hold post-SP7c phase 5 parity work.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Comment / docstring cleanup

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.h` (line 35 comment)
- Modify: `cpp/src/ui/settings/widgets/preview/osd_preview.h` (lines 21, 27 comments)
- Final pass: scan for any lingering `PCSX2Adapter` or stale `pcsx2-libretro` references in comments

- [ ] **Step 1: Fix `duckstation_adapter.h:35`**

Open `cpp/src/adapters/duckstation_adapter.h`. Find the comment that mentions `PCSX2Adapter`. Replace `PCSX2Adapter` with `Pcsx2LibretroAdapter`.

- [ ] **Step 2: Fix `osd_preview.h` references**

Open `cpp/src/ui/settings/widgets/preview/osd_preview.h`. Lines 21 and 27 reference `Pcsx2Adapter::previewSpec()`. Replace `Pcsx2Adapter::previewSpec()` with `Pcsx2LibretroAdapter::previewSpec()`.

- [ ] **Step 3: Final sweep for stragglers**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
grep -rnE 'PCSX2Adapter|Pcsx2Adapter[^:]' cpp/src cpp/tests
# Expected: no output (the deleted class should leave no trace)

grep -rnE '"pcsx2-libretro"' cpp/src cpp/tests
# Expected: no output (the id is gone everywhere)
```

Fix any results in-place — they're either dead comments or missed renames.

- [ ] **Step 4: Verify build still passes**

```bash
./scripts/build-universal.sh 2>&1 | tail -10
```

Expected: success.

- [ ] **Step 5: Commit**

```bash
git add -u cpp/src cpp/tests
git commit -m "$(cat <<'EOF'
SP8 task 6: comment / docstring cleanup

Repoints stale PCSX2Adapter / Pcsx2Adapter references in duckstation
and osd_preview headers to Pcsx2LibretroAdapter. Final sweep confirms
no remaining references to the deleted class or to the retired
"pcsx2-libretro" id string anywhere in cpp/src or cpp/tests.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: User-data and DB migration

**Files:**
- Create: `cpp/src/core/migration_pcsx2.h`
- Create: `cpp/src/core/migration_pcsx2.cpp`
- Modify: `cpp/src/main.cpp` (call after `Paths::ensureDirectories()`)
- Modify: `cpp/src/core/database.cpp` (bump `CURRENT_SCHEMA_VERSION` to 7, add v6→v7 migration step)
- Modify: `cpp/CMakeLists.txt` (add `migration_pcsx2.cpp` to source lists in 4 places — see below)
- Test: `cpp/tests/test_migration_pcsx2.cpp` (TDD'd)

### Step group A — filesystem migration module (TDD)

- [ ] **Step 1: Write the failing test for the filesystem migration**

Create `cpp/tests/test_migration_pcsx2.cpp` with the following content:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "core/migration_pcsx2.h"
#include "core/paths.h"

class TestMigrationPcsx2 : public QObject {
    Q_OBJECT
private slots:

    void noop_whenNothingPresent() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators");

        QVERIFY(MigrationPcsx2::runIfNeeded());
        QVERIFY(QFile::exists(tmp.path() + "/emulators/.sp8-migrated"));
    }

    void idempotent_skipsWhenSentinelPresent() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators");
        QFile::open(QString(tmp.path() + "/emulators/.sp8-migrated").toLocal8Bit().data(), "w").close();
        // (Simplification: just touch via QFile.)
        QFile sentinel(tmp.path() + "/emulators/.sp8-migrated");
        QVERIFY(sentinel.open(QIODevice::WriteOnly));
        sentinel.close();

        // Create a fake standalone install — migration should NOT touch it
        QDir(tmp.path()).mkpath("emulators/pcsx2");
        QFile portable(tmp.path() + "/emulators/pcsx2/portable.txt");
        QVERIFY(portable.open(QIODevice::WriteOnly)); portable.close();

        QVERIFY(MigrationPcsx2::runIfNeeded());
        QVERIFY(QFile::exists(tmp.path() + "/emulators/pcsx2/portable.txt"));
        QVERIFY(!QDir(tmp.path() + "/emulators/.archive").exists());
    }

    void archivesStandalone_andPromotesLibretro() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());

        // Standalone install layout
        QDir(tmp.path()).mkpath("emulators/pcsx2/ps2/memcards");
        QFile portable(tmp.path() + "/emulators/pcsx2/portable.txt");
        QVERIFY(portable.open(QIODevice::WriteOnly)); portable.close();
        QFile mcStandalone(tmp.path() + "/emulators/pcsx2/ps2/memcards/Mcd001.ps2");
        QVERIFY(mcStandalone.open(QIODevice::WriteOnly)); mcStandalone.write("standalone"); mcStandalone.close();

        // Libretro data dir
        QDir(tmp.path()).mkpath("emulators/pcsx2-libretro/ps2/memcards");
        QFile mcLibretro(tmp.path() + "/emulators/pcsx2-libretro/ps2/memcards/Mcd001.ps2");
        QVERIFY(mcLibretro.open(QIODevice::WriteOnly)); mcLibretro.write("libretro"); mcLibretro.close();

        QVERIFY(MigrationPcsx2::runIfNeeded());

        // Archive directory exists with standalone marker inside
        QDir archives(tmp.path() + "/emulators/.archive");
        QVERIFY(archives.exists());
        const auto entries = archives.entryList(QStringList() << "pcsx2-standalone-*", QDir::Dirs);
        QCOMPARE(entries.size(), 1);
        QVERIFY(QFile::exists(archives.absolutePath() + "/" + entries.first() + "/portable.txt"));

        // emulators/pcsx2/ now contains libretro data
        QFile promoted(tmp.path() + "/emulators/pcsx2/ps2/memcards/Mcd001.ps2");
        QVERIFY(promoted.open(QIODevice::ReadOnly));
        QCOMPARE(promoted.readAll(), QByteArray("libretro"));
        promoted.close();

        // libretro dir is gone
        QVERIFY(!QDir(tmp.path() + "/emulators/pcsx2-libretro").exists());

        // Sentinel touched
        QVERIFY(QFile::exists(tmp.path() + "/emulators/.sp8-migrated"));
    }

    void promotesLibretroOnly_whenNoStandalone() {
        QTemporaryDir tmp;
        Paths::setRoot(tmp.path());
        QDir(tmp.path()).mkpath("emulators/pcsx2-libretro/ps2");
        QFile f(tmp.path() + "/emulators/pcsx2-libretro/ps2/marker");
        QVERIFY(f.open(QIODevice::WriteOnly)); f.close();

        QVERIFY(MigrationPcsx2::runIfNeeded());

        QVERIFY(QFile::exists(tmp.path() + "/emulators/pcsx2/ps2/marker"));
        QVERIFY(!QDir(tmp.path() + "/emulators/pcsx2-libretro").exists());
        QVERIFY(!QDir(tmp.path() + "/emulators/.archive").exists());
    }
};

QTEST_MAIN(TestMigrationPcsx2)
#include "test_migration_pcsx2.moc"
```

- [ ] **Step 2: Run the test to verify it fails (header missing)**

```bash
cd build
cmake --build . --target test_migration_pcsx2 2>&1 | tail -20
```

Expected: build fails with `migration_pcsx2.h: No such file or directory` and `MigrationPcsx2: undeclared identifier`.

(Add the test target to CMakeLists.txt in Step 4 after the header exists.)

- [ ] **Step 3: Implement `cpp/src/core/migration_pcsx2.h`**

Create `cpp/src/core/migration_pcsx2.h`:

```cpp
#pragma once

#include <QString>

// SP8: one-shot retirement of the PCSX2 standalone path.
//
// Sentinel-gated migration that:
//   1. If emulators/pcsx2/ looks like a standalone install (any of
//      portable.txt, .version.json, inis/, resources/, PCSX2-v*.app),
//      moves the whole dir to emulators/.archive/pcsx2-standalone-<ts>/.
//   2. If emulators/pcsx2-libretro/ exists, moves its contents into
//      emulators/pcsx2/ (which is empty after step 1, or already
//      empty if standalone was never installed).
//   3. Touches emulators/.sp8-migrated as the sentinel.
//
// Idempotent: if the sentinel exists, returns true immediately.
// Reads paths via Paths::emulatorsDir() — Paths::setRoot() must have
// been called first.
namespace MigrationPcsx2 {
    /** Returns true on success (including no-op). Returns false on
     *  hard failure (a move failed); caller should log and bail. The
     *  sentinel is NOT touched on failure so the next launch retries. */
    bool runIfNeeded();
}
```

- [ ] **Step 4: Implement `cpp/src/core/migration_pcsx2.cpp`**

Create `cpp/src/core/migration_pcsx2.cpp`:

```cpp
#include "migration_pcsx2.h"

#include "paths.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

bool looksLikeStandaloneInstall(const QString& dir) {
    const QDir d(dir);
    if (!d.exists()) return false;
    if (d.exists("portable.txt")) return true;
    if (d.exists(".version.json")) return true;
    if (d.exists("inis")) return true;
    if (d.exists("resources")) return true;
    const auto apps = d.entryList(QStringList() << "PCSX2-v*.app", QDir::Dirs);
    if (!apps.isEmpty()) return true;
    return false;
}

bool moveDir(const QString& from, const QString& to) {
    if (!QDir().rename(from, to)) {
        qCritical() << "[MigrationPcsx2] failed to move" << from << "->" << to;
        return false;
    }
    return true;
}

bool touchFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qCritical() << "[MigrationPcsx2] failed to touch sentinel" << path;
        return false;
    }
    f.close();
    return true;
}

}

bool MigrationPcsx2::runIfNeeded() {
    const QString emusDir = Paths::emulatorsDir();
    const QString sentinel = emusDir + "/.sp8-migrated";

    if (QFile::exists(sentinel)) return true;

    QDir().mkpath(emusDir);

    const QString standaloneDir = emusDir + "/pcsx2";
    const QString libretroDir   = emusDir + "/pcsx2-libretro";

    // Step 1: archive standalone install if present
    if (looksLikeStandaloneInstall(standaloneDir)) {
        const QString archiveRoot = emusDir + "/.archive";
        QDir().mkpath(archiveRoot);
        const QString ts = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss");
        const QString archiveTarget = archiveRoot + "/pcsx2-standalone-" + ts;
        if (!moveDir(standaloneDir, archiveTarget)) return false;
        qInfo() << "[MigrationPcsx2] archived standalone install to" << archiveTarget;
    } else if (QDir(standaloneDir).exists()) {
        // Not a standalone install but the dir exists — could be a
        // pre-migrated state (libretro already there). Leave it
        // alone and let the libretro promote step below skip if
        // there's nothing to promote.
        qInfo() << "[MigrationPcsx2] emulators/pcsx2/ exists but doesn't look standalone — leaving untouched";
    }

    // Step 2: promote libretro data
    if (QDir(libretroDir).exists()) {
        if (QDir(standaloneDir).exists()) {
            qCritical() << "[MigrationPcsx2] cannot promote libretro: emulators/pcsx2/ still exists after archive step";
            return false;
        }
        if (!moveDir(libretroDir, standaloneDir)) return false;
        qInfo() << "[MigrationPcsx2] promoted libretro data to" << standaloneDir;
    }

    // Step 3: sentinel
    return touchFile(sentinel);
}
```

- [ ] **Step 5: Add the new sources to `cpp/CMakeLists.txt`**

Two source lists need the new file. Find each `add_library` / `add_executable` block that contains `src/core/paths.cpp` (the migration depends on Paths) — typically:

- The main app target (`RetroNest`)
- The CLI target (if separate)
- Test targets that link migration_pcsx2 directly (only `test_migration_pcsx2` — see Step 6)

Add this line next to `src/core/paths.cpp` in each block:

```
    src/core/migration_pcsx2.cpp
```

Concretely: search `cpp/CMakeLists.txt` for `src/core/paths.cpp` and add `src/core/migration_pcsx2.cpp` adjacent in each occurrence inside `add_library`/`add_executable` source lists. **Do not** add it to test targets other than `test_migration_pcsx2` — those don't need the migration code.

- [ ] **Step 6: Add the test target to `cpp/CMakeLists.txt`**

At the end of the existing test-target section (after `test_pcsx2_controller_schema`), add:

```
add_executable(test_migration_pcsx2
    tests/test_migration_pcsx2.cpp
    src/core/migration_pcsx2.cpp
    src/core/paths.cpp
)
set_target_properties(test_migration_pcsx2 PROPERTIES AUTOMOC ON)
target_include_directories(test_migration_pcsx2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_migration_pcsx2 PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MigrationPcsx2 COMMAND test_migration_pcsx2)
```

- [ ] **Step 7: Build and run the migration tests**

```bash
cd build
cmake --build . --target test_migration_pcsx2 2>&1 | tail -20
ctest -R MigrationPcsx2 --output-on-failure
cd ..
```

Expected: all four sub-tests pass.

If the `idempotent_skipsWhenSentinelPresent` test fails because of an oddity in `QFile::open` static usage — open the test file and replace the awkward static-style touch with a regular instance call (the test source above already handles this with a second QFile instance).

### Step group B — wire into main.cpp

- [ ] **Step 8: Invoke the migration from `main.cpp`**

Open `cpp/src/main.cpp`. Find the block (around lines 140–145):

```cpp
    // Set up paths
    if (!Paths::setRoot(rootPath)) {
        qCritical() << "Invalid root path:" << rootPath;
        return 1;
    }
    Paths::ensureDirectories();
```

Immediately after `Paths::ensureDirectories();` and before `// Open database`, add:

```cpp

    // SP8: one-shot retirement migration. Fails-safe: on error we log
    // and continue without touching the sentinel — next launch retries.
    if (!MigrationPcsx2::runIfNeeded()) {
        qCritical() << "[main] SP8 PCSX2 migration failed; data may be inconsistent. Restart to retry.";
        // Continue running — partial migrations are recoverable from .archive/.
    }
```

Add the include at the top of `main.cpp` (alphabetically with other `"core/..."` includes):

```cpp
#include "core/migration_pcsx2.h"
```

### Step group C — DB migration

- [ ] **Step 9: Bump `CURRENT_SCHEMA_VERSION` to 7 and add v6→v7 step**

Open `cpp/src/core/database.cpp`. Change line 57:

```cpp
static const int CURRENT_SCHEMA_VERSION = 6;
```

to:

```cpp
static const int CURRENT_SCHEMA_VERSION = 7;
```

Then find the existing v5→v6 migration block (around lines 248–267) and add immediately after it, before the trailing `if (current < CURRENT_SCHEMA_VERSION)` guard:

```cpp
    if (current < 7) {
        auto db = QSqlDatabase::database(DB_CONNECTION);
        if (!db.transaction()) {
            qCritical() << "[Database] Failed to begin transaction for v6→v7 migration";
            return false;
        }
        QSqlQuery q(db);
        if (!q.exec("UPDATE games SET emulator_id = 'pcsx2' WHERE emulator_id = 'pcsx2-libretro'")) {
            qCritical() << "[Database] Migration v6→v7 failed:" << q.lastError().text();
            db.rollback();
            return false;
        }
        if (!db.commit()) {
            qCritical() << "[Database] Failed to commit v6→v7 migration";
            db.rollback();
            return false;
        }
        qInfo() << "[Database] Migrated schema v6 → v7 (renamed pcsx2-libretro → pcsx2)";
    }
```

The v6→v7 migration is purely UPDATE — no schema change — but the version bump ensures it only runs once.

- [ ] **Step 10: Build**

```bash
./scripts/build-universal.sh 2>&1 | tail -15
```

Expected: success.

- [ ] **Step 11: Run the full test suite**

```bash
cd build
ctest --output-on-failure 2>&1 | tail -30
cd ..
```

Expected: all green including the new `MigrationPcsx2` test.

- [ ] **Step 12: Commit**

```bash
git add cpp/src/core/migration_pcsx2.h cpp/src/core/migration_pcsx2.cpp \
        cpp/src/core/database.cpp cpp/src/main.cpp \
        cpp/tests/test_migration_pcsx2.cpp cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP8 task 7: one-shot user-data + DB migration on first launch

Adds MigrationPcsx2::runIfNeeded(): sentinel-gated, idempotent,
archives the standalone install under emulators/.archive/, promotes
libretro data into emulators/pcsx2/. Invoked from main.cpp right
after Paths::ensureDirectories(). Failure leaves the sentinel
untouched so the next launch retries.

Database schema bumped v6→v7 with a single UPDATE that rewrites every
emulator_id='pcsx2-libretro' row to 'pcsx2', so existing scanned games
keep working under the renamed adapter.

TDD: test_migration_pcsx2 covers four scenarios (no-op, sentinel-skip,
full archive+promote, libretro-only promote).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Live smoke gate

No code changes. Run the integration smoke against the user's real `~/Documents/RetroNest/` (the migration runs at first launch — be sure pre-flight backups are taken).

- [ ] **Step 1: Verify pre-flight backups exist**

```bash
ls -la ~/Desktop/sp8-backup-*
```

Expected: at least one backup directory each of `pcsx2-standalone` and `pcsx2-libretro`. If absent, take them now per the pre-flight section at the top of this document.

- [ ] **Step 2: Launch the freshly-built app**

```bash
open /Users/mark/Documents/Projects/RetroNest-Project/build/RetroNest.app
```

While running, in a separate terminal verify the migration happened:

```bash
ls -la ~/Documents/RetroNest/emulators/
# Expected: pcsx2/ exists, pcsx2-libretro/ is GONE, .archive/ exists, .sp8-migrated file exists
ls ~/Documents/RetroNest/emulators/.archive/
# Expected: pcsx2-standalone-<timestamp>/ dir, containing the old PCSX2-v2.6.3.app + inis + ps2 etc.
ls ~/Documents/RetroNest/emulators/pcsx2/
# Expected: only the libretro layout (ps2/ with memcards + savestates, core options, etc.)
```

- [ ] **Step 3: In the app, open the PCSX2 settings dialog**

Navigate to the PCSX2 emulator settings. Expected:

- Dialog title says "PCSX2" (not "PCSX2 (libretro core, dev)")
- All 89 core-option knobs render across the expected cards/sub-tabs (Recommended, Graphics with sub-tabs, Emulation, Audio, etc.)
- Aspect-ratio preview on Recommended works
- OSD preview on Graphics > On-Screen Display works

- [ ] **Step 4: Launch a previously-scanned PCSX2 game (R&C 2 NTSC recommended)**

In the library, locate Ratchet & Clank 2 (or any previously-scanned PS2 game). Click Play.

Expected:

- Game boots through to title screen using the libretro core.
- D-pad, analog stick, and rumble all work via the configured controller.
- Save state save (default hotkey or menu) and load both succeed in-session.

Quit the game. Then re-launch the same game. Expected:

- Cold-resume (auto-load of pending save state) works.
- Memcard data from prior play persists.

- [ ] **Step 5: Restart RetroNest to confirm sentinel short-circuit**

Close the app. Reopen:

```bash
open /Users/mark/Documents/Projects/RetroNest-Project/build/RetroNest.app
```

In another terminal:

```bash
ls -la ~/Documents/RetroNest/emulators/.sp8-migrated
```

Expected: sentinel file timestamp from first launch (i.e., NOT updated by the second launch). No archive dir change. Console / log output should NOT contain "[MigrationPcsx2] archived" or "[MigrationPcsx2] promoted" messages on this launch.

- [ ] **Step 6: Push to origin/main**

If all 5 smoke checks pass:

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git log --oneline origin/main..HEAD
# Expected: 7 commits, one per task (Tasks 1-7)
git push origin main
```

If any check fails, do NOT push. Investigate, file a followup task, and consider whether the failure is a SP8 blocker (revert) or a deferred fix.

- [ ] **Step 7: Update memory**

Mark SP8 as shipped in `MEMORY.md` and write a `session_handoff_sp8_shipped.md` per the project's handoff-memory convention. Remove the `sp8_kickoff.md` pointer from `MEMORY.md` once `session_handoff_sp8_shipped.md` supersedes it.

- [ ] **Step 8: No commit needed for Task 8 itself** — the work was verification. Final state: 7 commits on `main` past Phase 5, all green, pushed.

---

## Spec coverage self-review

- **D-SP8-1 ID rename** → Task 1 (manifest + adapter_registry); paths repointed in Task 4 step 7.
- **D-SP8-2 user data migration** → Task 7 step group A (filesystem) + DB v7 (step group C).
- **D-SP8-3 manifest collapse** → Task 1 steps 1–3.
- **D-SP8-4 controls.ini** → carried wholesale by Task 7's libretro-promote `mv`.
- **D-SP8-5 test retarget** → Task 5 (sources) + Task 3 (CMakeLists test targets).
- **D-SP8-bonus dir rename** → Task 4.
- **6 standalone source deletes** → Task 3.
- **app_controller dispatch unification** → Task 2.
- **Comment cleanup** → Task 6.
- **Smoke gate** → Task 8.

All decisions in the spec are covered. No placeholders remain.
