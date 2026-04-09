# Rename EmuFront to RetroNest Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename every user-visible, code-level, and build-system reference to "EmuFront" / "emulator-frontend" to "RetroNest" / "retronest" in a single atomic commit, leaving historical design/plan docs under `docs/superpowers/` untouched and applying no data migration.

**Architecture:** Mechanical string replacement across 11 files in `cpp/` and `CLAUDE.md`. The rename is applied as a series of independent edits, then verified with a single build + test + CLI smoke gate, then committed atomically. Splitting the rename into sub-commits would produce intermediate states where only part of the codebase had been renamed — tolerable for compilation (most changes are independent) but incoherent as history.

**Tech Stack:** CMake, Qt6 (C++17 + QML), clang++. No new tests; rely on existing `ctest` suite (6 tests) plus a CLI smoke launch at the new bundle path.

**Spec:** `docs/superpowers/specs/2026-04-09-rename-to-retronest-design.md`

---

## Materials

### A. Verification gate

Run after all edits are applied, before committing. A rename passes only if **all** of these succeed:

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp

# 1. Reconfigure — CMake needs to pick up the new project() name
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"

# 2. Remove the stale EmuFront.app from the previous configuration
#    (CMake does not clean up old targets on rename; do it by hand)
rm -rf build/EmuFront.app

# 3. Clean build — ensures no stale objects from the old target name
cmake --build build --clean-first

# 4. All 6 tests pass
ctest --test-dir build --output-on-failure

# 5. CLI smoke at the NEW bundle path — must print the emulator status block
./build/RetroNest.app/Contents/MacOS/RetroNest --cli

# 6. No "EmuFront" or "emulator-frontend" left in cpp/ or CLAUDE.md
git grep EmuFront cpp/ CLAUDE.md
git grep emulator-frontend cpp/ CLAUDE.md
```

Both `git grep` commands must produce **no output** (exit code 1 from zsh is expected for zero matches).

Warning count should remain at the baseline of 11 captured in the bundle-cleanup Phase 1 pre-flight.

### B. Out of scope (do NOT edit)

- Any file under `docs/superpowers/specs/` or `docs/superpowers/plans/` — these are historical records, intentionally preserved.
- Any file under `cpp/tests/` — tests exercise specific strings that may include "EmuFront" as historical test data; none of the current 6 tests do, but if you find one, flag it and stop.
- Any file under `assets/` — contains `RetroNest icon.webp`; source asset name stays as-is.
- Git commit messages (can't rewrite safely).
- The user's filesystem directory `/Users/mark/Documents/EmuFront-Project` — user will rename manually with `mv` after merge.

---

## Task 0: Pre-flight

**Files:** none modified. Read-only verification that the working tree is ready.

- [ ] **Step 1: Verify clean working tree on the expected branch**

```bash
cd /Users/mark/Documents/EmuFront-Project
git status
git log --oneline -5
```

Expected: `On branch chore/bundle-cleanup-and-review`, working tree clean. Most recent commit is `f0c79b9 docs(spec): rename EmuFront to RetroNest`.

If the tree is dirty or you are on a different branch, STOP and ask the user how to proceed.

- [ ] **Step 2: Confirm the baseline build is green before touching anything**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build 2>&1 | tail -3
ctest --test-dir build --output-on-failure 2>&1 | tail -5
./build/EmuFront.app/Contents/MacOS/EmuFront --cli 2>&1 | head -8
```

Expected:
- Build succeeds.
- All 6 tests pass.
- CLI smoke prints `Loaded 3 emulator manifest(s)` and the `=== Emulator Status ===` block with DuckStation/PCSX2/PPSSPP.

If anything fails at baseline, STOP and tell the user — the rename cannot start with a broken build.

---

## Task 1: Rename in `cpp/CMakeLists.txt`

**Files:**
- Modify: `cpp/CMakeLists.txt` (6 edits)

- [ ] **Step 1: Change the project name**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/CMakeLists.txt`:

```
old_string: project(EmuFront LANGUAGES CXX OBJCXX)
new_string: project(RetroNest LANGUAGES CXX OBJCXX)
```

This single change makes `${PROJECT_NAME}` expand to `RetroNest`, so the build target, the executable, and the `.app` bundle all become `RetroNest` automatically.

- [ ] **Step 2: Change the ScreenScraper software name**

```
old_string: set(SCREENSCRAPER_SOFTNAME "EmuFront" CACHE STRING "ScreenScraper software name")
new_string: set(SCREENSCRAPER_SOFTNAME "RetroNest" CACHE STRING "ScreenScraper software name")
```

- [ ] **Step 3: Update the app icon comment**

```
old_string: # App icon — bundled into EmuFront.app/Contents/Resources/AppIcon.icns.
new_string: # App icon — bundled into RetroNest.app/Contents/Resources/AppIcon.icns.
```

- [ ] **Step 4: Update the bundle-metadata note comment**

```
old_string: # Note: EmuFront's bundle ID does NOT affect PPSSPP's NSUserDefaults domain
new_string: # Note: RetroNest's bundle ID does NOT affect PPSSPP's NSUserDefaults domain
```

- [ ] **Step 5: Change CFBundleName**

```
old_string:     MACOSX_BUNDLE_BUNDLE_NAME "EmuFront"
new_string:     MACOSX_BUNDLE_BUNDLE_NAME "RetroNest"
```

- [ ] **Step 6: Change CFBundleIdentifier**

```
old_string:     MACOSX_BUNDLE_GUI_IDENTIFIER "com.markpearce.emufront"
new_string:     MACOSX_BUNDLE_GUI_IDENTIFIER "com.markpearce.retronest"
```

- [ ] **Step 7: Spot-check the file**

```bash
cd /Users/mark/Documents/EmuFront-Project
grep -n "EmuFront\|emulator-frontend\|emufront" cpp/CMakeLists.txt
```

Expected: **no output**. If anything matches, fix it before moving on.

---

## Task 2: Rename in `cpp/src/main.cpp`

**Files:**
- Modify: `cpp/src/main.cpp` (4 edits)

- [ ] **Step 1: Change the Qt application name**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/src/main.cpp`:

```
old_string:     app.setApplicationName("EmuFront");
new_string:     app.setApplicationName("RetroNest");
```

Effect: `QStandardPaths::AppDataLocation` now resolves to `~/Library/Application Support/RetroNest/`, which is where `Paths::loadSavedRoot()` reads `config.json` from on next launch. Per the spec, the old `EmuFront` directory is abandoned.

- [ ] **Step 2: Update the multi-line resolveResourceDir comment block**

```
old_string:     // Resolve a resource directory (manifests, themes, …) relative to the
    // executable. Supports layouts for dev (bare + bundle) and installed bundle.
    // Manifests live at the project root, so from cpp/build/EmuFront that's
    // "../../" and from cpp/build/EmuFront.app/Contents/MacOS/EmuFront that's
    // "../../../../../" (up 5: MacOS, Contents, EmuFront.app, build, cpp).
new_string:     // Resolve a resource directory (manifests, themes, …) relative to the
    // executable. Supports layouts for dev (bare + bundle) and installed bundle.
    // Manifests live at the project root, so from cpp/build/RetroNest that's
    // "../../" and from cpp/build/RetroNest.app/Contents/MacOS/RetroNest that's
    // "../../../../../" (up 5: MacOS, Contents, RetroNest.app, build, cpp).
```

- [ ] **Step 3: Update the inline candidate-path comment**

```
old_string:             exeDir + "/../../../../../" + name,          // dev .app bundle (up 5: MacOS,Contents,EmuFront.app,build,cpp)
new_string:             exeDir + "/../../../../../" + name,          // dev .app bundle (up 5: MacOS,Contents,RetroNest.app,build,cpp)
```

- [ ] **Step 4: Change the database filename**

```
old_string:     const QString dbPath = Paths::configDir() + "/emulator-frontend.db";
new_string:     const QString dbPath = Paths::configDir() + "/retronest.db";
```

**Critical:** this is one of two hardcoded copies of the DB filename. The other is in `install_controller.cpp` and MUST be renamed in lockstep (Task 3 Step 1). If one is renamed and the other isn't, the app will write to two different DB files depending on code path.

- [ ] **Step 5: Spot-check the file**

```bash
cd /Users/mark/Documents/EmuFront-Project
grep -n "EmuFront\|emulator-frontend\|emufront" cpp/src/main.cpp
```

Expected: **no output**.

---

## Task 3: Rename in other C++/Obj-C++ source files

**Files:**
- Modify: `cpp/src/ui/install_controller.cpp` (1 edit — DB filename, in lockstep with Task 2)
- Modify: `cpp/src/core/database.h` (1 edit — docstring)
- Modify: `cpp/src/core/emulator_installer.cpp` (1 edit with `replace_all: true`, covers 4 sites)
- Modify: `cpp/src/core/github_client.h` (1 edit)
- Modify: `cpp/src/core/ra_client.cpp` (1 edit)

- [ ] **Step 1: Rename the second hardcoded DB filename (lockstep with Task 2 Step 4)**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/src/ui/install_controller.cpp`:

```
old_string:         QString dbPath = Paths::configDir() + "/emulator-frontend.db";
new_string:         QString dbPath = Paths::configDir() + "/retronest.db";
```

Note the leading 8 spaces of indentation — the old line is inside a function body.

- [ ] **Step 2: Update the database docstring**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/src/core/database.h`:

```
old_string:  * One file at {root}/config/emulator-frontend.db.
new_string:  * One file at {root}/config/retronest.db.
```

- [ ] **Step 3: Rename all 4 User-Agent headers in `emulator_installer.cpp`**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/src/core/emulator_installer.cpp` with `replace_all: true`:

```
old_string: "EmuFront/1.0"
new_string: "RetroNest/1.0"
replace_all: true
```

This single edit changes all 4 User-Agent header assignments (at lines 30, 64, 346, and 412 in the current file).

- [ ] **Step 4: Rename the User-Agent header in `github_client.h`**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/src/core/github_client.h`:

```
old_string:     req.setHeader(QNetworkRequest::UserAgentHeader, "EmuFront/1.0");
new_string:     req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
```

- [ ] **Step 5: Rename the User-Agent header in `ra_client.cpp`**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/src/core/ra_client.cpp`:

```
old_string:     req.setHeader(QNetworkRequest::UserAgentHeader, "EmuFront/1.0");
new_string:     req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
```

- [ ] **Step 6: Spot-check all touched files**

```bash
cd /Users/mark/Documents/EmuFront-Project
grep -n "EmuFront\|emulator-frontend\|emufront" \
    cpp/src/ui/install_controller.cpp \
    cpp/src/core/database.h \
    cpp/src/core/emulator_installer.cpp \
    cpp/src/core/github_client.h \
    cpp/src/core/ra_client.cpp
```

Expected: **no output**.

---

## Task 4: Rename in QML

**Files:**
- Modify: `cpp/qml/AppUI/AppWindow.qml` (1 edit)
- Modify: `cpp/qml/SetupWizard/Main.qml` (1 edit)
- Modify: `cpp/qml/SetupWizard/WelcomePage.qml` (1 edit)
- Modify: `cpp/qml/SetupWizard/FolderPage.qml` (1 edit)

- [ ] **Step 1: Change the main window title**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/qml/AppUI/AppWindow.qml`:

```
old_string:     title: "EmuFront"
new_string:     title: "RetroNest"
```

- [ ] **Step 2: Change the wizard window title**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/qml/SetupWizard/Main.qml`:

```
old_string:     title: "EmuFront Setup"
new_string:     title: "RetroNest Setup"
```

- [ ] **Step 3: Change the wizard welcome text**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/qml/SetupWizard/WelcomePage.qml`:

```
old_string:             text: "EmuFront"
new_string:             text: "RetroNest"
```

Note the 12 spaces of indentation — this line lives inside a nested QML element.

- [ ] **Step 4: Change the wizard default root directory suggestion**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/cpp/qml/SetupWizard/FolderPage.qml`:

```
old_string:             wizard.rootPath = dir + "/EmuFront"
new_string:             wizard.rootPath = dir + "/RetroNest"
```

- [ ] **Step 5: Spot-check all touched QML files**

```bash
cd /Users/mark/Documents/EmuFront-Project
grep -n "EmuFront" \
    cpp/qml/AppUI/AppWindow.qml \
    cpp/qml/SetupWizard/Main.qml \
    cpp/qml/SetupWizard/WelcomePage.qml \
    cpp/qml/SetupWizard/FolderPage.qml
```

Expected: **no output**.

---

## Task 5: Rename in `CLAUDE.md`

**Files:**
- Modify: `CLAUDE.md` (3 edits)

- [ ] **Step 1: Update the GUI mode run command**

Use the `Edit` tool on `/Users/mark/Documents/EmuFront-Project/CLAUDE.md`:

```
old_string: ./build/EmuFront.app/Contents/MacOS/EmuFront            # GUI mode
new_string: ./build/RetroNest.app/Contents/MacOS/RetroNest            # GUI mode
```

- [ ] **Step 2: Update the CLI mode run command**

```
old_string: ./build/EmuFront.app/Contents/MacOS/EmuFront --cli      # CLI mode
new_string: ./build/RetroNest.app/Contents/MacOS/RetroNest --cli      # CLI mode
```

- [ ] **Step 3: Update the `open` run command**

```
old_string: open ./build/EmuFront.app
new_string: open ./build/RetroNest.app
```

- [ ] **Step 4: Spot-check the file**

```bash
cd /Users/mark/Documents/EmuFront-Project
grep -n "EmuFront\|emulator-frontend\|emufront" CLAUDE.md
```

Expected: **no output**.

---

## Task 6: Verification gate

**Files:** none modified. Build, test, smoke, and exhaustive grep sweep against the new state.

- [ ] **Step 1: Reconfigure CMake (so it picks up the new `project()` name)**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" 2>&1 | tail -5
```

Expected: `-- Configuring done`, `-- Generating done`, `-- Build files have been written to: .../build`. No errors. Warnings about Qt policies that existed before the rename may still appear — those are not introduced by this change.

- [ ] **Step 2: Remove the stale `EmuFront.app` bundle from the previous configuration**

CMake does not automatically clean up old target artifacts when you rename a project. Remove the old bundle by hand so you don't end up with both `EmuFront.app` and `RetroNest.app` cluttering the build directory.

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
rm -rf build/EmuFront.app
```

Expected: command completes silently, exit 0.

- [ ] **Step 3: Clean rebuild**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build --clean-first 2>&1 | tail -5
```

Expected: last line contains `Built target` or similar success indicator. Full build succeeds. The new bundle appears at `build/RetroNest.app/`.

- [ ] **Step 4: Verify the new bundle exists and has the correct layout**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
ls -la build/RetroNest.app/Contents/MacOS/ build/RetroNest.app/Contents/Resources/ 2>&1 | head -20
grep -A 1 "CFBundleName\|CFBundleIdentifier\|CFBundleExecutable\|CFBundleIconFile" build/RetroNest.app/Contents/Info.plist
```

Expected:
- `build/RetroNest.app/Contents/MacOS/RetroNest` exists (the new executable name).
- `build/RetroNest.app/Contents/Resources/AppIcon.icns` exists (copied by CMake via `MACOSX_PACKAGE_LOCATION`).
- `Info.plist` contains `CFBundleName = RetroNest`, `CFBundleIdentifier = com.markpearce.retronest`, `CFBundleExecutable = RetroNest`, `CFBundleIconFile = AppIcon`.

- [ ] **Step 5: Run the full test suite**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
ctest --test-dir build --output-on-failure 2>&1 | tail -15
```

Expected: `100% tests passed, 0 tests failed out of 6`. Test names are `IniFile`, `RomScanner`, `Iso9660Reader`, `SfoParser`, `BitmaskHelpers`, `PPSSPPSchema`.

If any test fails, STOP and report. Do not commit.

- [ ] **Step 6: CLI smoke test at the NEW bundle path**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
./build/RetroNest.app/Contents/MacOS/RetroNest --cli 2>&1 | head -15
```

Expected output:
```
Loaded 3 emulator manifest(s)
[Paths] Root: "/Users/mark/Documents/EmuFront"
[Database] Opened: "/Users/mark/Documents/EmuFront/config/retronest.db"

=== Emulator Status ===
  DuckStation     [psx]: INSTALLED  -> ...
  PCSX2           [ps2]: INSTALLED  -> ...
  PPSSPP          [psp]: INSTALLED  -> ...
```

Specific things to verify in the output:
- The bundle path is `RetroNest.app/Contents/MacOS/RetroNest` (not EmuFront).
- The database filename is `retronest.db` (not emulator-frontend.db).
- The `[Paths] Root:` line still shows `/Users/mark/Documents/EmuFront` — this is expected because `Paths::loadSavedRoot()` read the old root from the OLD `~/Library/Application Support/EmuFront/config.json` that still exists from before the rename. **This is not a bug.** The user will manually delete both `~/Documents/EmuFront/` and `~/Library/Application Support/EmuFront/` after this branch lands, then re-run the setup wizard on next launch, at which point the new root will be `/Users/mark/Documents/RetroNest`.

If the CLI smoke fails or the database creation fails (e.g. because the old DB filename is still somewhere the code is looking), STOP and report.

- [ ] **Step 7: Exhaustive grep sweep — no EmuFront or emulator-frontend left anywhere in scope**

```bash
cd /Users/mark/Documents/EmuFront-Project
git grep EmuFront cpp/ CLAUDE.md
git grep emulator-frontend cpp/ CLAUDE.md
git grep -i emufront cpp/ CLAUDE.md
```

Expected: **all three commands produce no output**. Exit code 1 from zsh is expected and fine — that means grep found no matches.

If any command produces output, read the results carefully and add the missed references to the working tree (as additional Edits). Re-run the grep sweep until clean.

Note: `git grep` also searches the historical `docs/superpowers/` tree but you are explicitly limiting the grep to `cpp/` and `CLAUDE.md`, so historical docs are not checked — that's correct per the spec.

- [ ] **Step 8: Confirm working tree is coherent**

```bash
cd /Users/mark/Documents/EmuFront-Project
git status
git diff --stat
```

Expected: `git status` shows all the files touched by Tasks 1–5 as modified, no new or deleted files, no untracked files other than `cpp/build/`. `git diff --stat` shows something like:

```
 CLAUDE.md                                       |  6 +++---
 cpp/CMakeLists.txt                              | 12 ++++++------
 cpp/qml/AppUI/AppWindow.qml                     |  2 +-
 cpp/qml/SetupWizard/FolderPage.qml              |  2 +-
 cpp/qml/SetupWizard/Main.qml                    |  2 +-
 cpp/qml/SetupWizard/WelcomePage.qml             |  2 +-
 cpp/src/core/database.h                         |  2 +-
 cpp/src/core/emulator_installer.cpp             |  8 ++++----
 cpp/src/core/github_client.h                    |  2 +-
 cpp/src/core/ra_client.cpp                      |  2 +-
 cpp/src/main.cpp                                | 10 +++++-----
 cpp/src/ui/install_controller.cpp               |  2 +-
 12 files changed, ...
```

Roughly 12 files, modifications only. If anything else appears (new files, deletions, unexpected files), STOP and investigate.

---

## Task 7: Single atomic commit

**Files:** none modified. Stage and commit all the rename changes.

- [ ] **Step 1: Stage the exact files modified by Tasks 1–5**

```bash
cd /Users/mark/Documents/EmuFront-Project
git add \
    CLAUDE.md \
    cpp/CMakeLists.txt \
    cpp/qml/AppUI/AppWindow.qml \
    cpp/qml/SetupWizard/FolderPage.qml \
    cpp/qml/SetupWizard/Main.qml \
    cpp/qml/SetupWizard/WelcomePage.qml \
    cpp/src/core/database.h \
    cpp/src/core/emulator_installer.cpp \
    cpp/src/core/github_client.h \
    cpp/src/core/ra_client.cpp \
    cpp/src/main.cpp \
    cpp/src/ui/install_controller.cpp
git status
```

Expected: "Changes to be committed" lists all 12 files, no unstaged changes.

- [ ] **Step 2: Create the commit**

```bash
cd /Users/mark/Documents/EmuFront-Project
git commit -m "$(cat <<'EOF'
refactor: rename EmuFront to RetroNest

Single atomic rename across build system, source, QML, and CLAUDE.md.
Historical design/plan docs under docs/superpowers/ are intentionally
left alone as history. No data migration code — user will manually
delete the existing ~/Documents/EmuFront/ root and the old
~/Library/Application Support/EmuFront/ directory, then re-run the
setup wizard on next launch to create a fresh ~/Documents/RetroNest/
root with a new config.json and an empty retronest.db.

Build system (cpp/CMakeLists.txt):
- project(EmuFront) → project(RetroNest): changes \${PROJECT_NAME},
  so the CMake target, the executable, and the .app bundle all
  become RetroNest (was EmuFront).
- SCREENSCRAPER_SOFTNAME: "EmuFront" → "RetroNest". Low-risk API
  client string; no re-registration required with ScreenScraper.
- MACOSX_BUNDLE_BUNDLE_NAME: "EmuFront" → "RetroNest" (CFBundleName).
- MACOSX_BUNDLE_GUI_IDENTIFIER: "com.markpearce.emufront" →
  "com.markpearce.retronest" (CFBundleIdentifier). Does not affect
  PPSSPP's NSUserDefaults domain (unchanged at org.ppsspp.ppsspp).
- Two comment references updated for consistency.

C++/Obj-C++ source:
- main.cpp: app.setApplicationName → "RetroNest"; three
  resolveResourceDir path comments; DB filename
  "/emulator-frontend.db" → "/retronest.db".
- install_controller.cpp: second hardcoded DB filename
  "/emulator-frontend.db" → "/retronest.db" (must match main.cpp
  or the app would write to two different files).
- database.h: docstring referencing the DB filename.
- User-Agent headers (6 sites across 3 files): "EmuFront/1.0" →
  "RetroNest/1.0" (emulator_installer.cpp x4, github_client.h,
  ra_client.cpp).

QML (user-facing):
- AppWindow.qml: window title "EmuFront" → "RetroNest".
- SetupWizard/Main.qml: wizard title "EmuFront Setup" → "RetroNest Setup".
- SetupWizard/WelcomePage.qml: welcome splash text "EmuFront" → "RetroNest".
- SetupWizard/FolderPage.qml: default root directory suffix
  "/EmuFront" → "/RetroNest" appended to the parent dir the user picks.

Project doc:
- CLAUDE.md: build/run commands updated to reference the new
  RetroNest.app bundle path.

Out of scope:
- docs/superpowers/specs/ and docs/superpowers/plans/ — 14 historical
  design and plan documents, intentionally preserved as accurate
  record of when the project was named EmuFront.
- Git commit history (can't rewrite).
- The repo directory at /Users/mark/Documents/EmuFront-Project
  (user renames manually later).
- The current git branch name (chore/bundle-cleanup-and-review) —
  user can rename manually if desired.
- No data migration code.

Verified: build green, all 6 tests pass, CLI smoke launch at the new
bundle path (./build/RetroNest.app/Contents/MacOS/RetroNest --cli)
prints the emulator status block with the new database filename
(retronest.db), git grep EmuFront and emulator-frontend against cpp/
and CLAUDE.md both return zero matches.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
git log --oneline -5
```

Expected: one new commit on `chore/bundle-cleanup-and-review`. `git log --oneline -5` shows the commit as HEAD.

---

## Task 8: Manual GUI smoke test handoff

**Files:** none modified. Tell the user what to check manually.

- [ ] **Step 1: Report the completion and present the manual checklist**

Tell the user:

> Rename complete. One commit lands on `chore/bundle-cleanup-and-review`:
> `refactor: rename EmuFront to RetroNest`.
>
> Build green, 6/6 tests pass, CLI smoke works at the new path
> `./build/RetroNest.app/Contents/MacOS/RetroNest`, `git grep EmuFront cpp/ CLAUDE.md`
> and `git grep emulator-frontend cpp/ CLAUDE.md` both return zero matches.
>
> There are three manual follow-ups before the branch is truly clean, all of
> which are your call:
>
> 1. **GUI smoke test.** `open cpp/build/RetroNest.app` from the project
>    directory (or double-click `RetroNest.app` in Finder). Confirm:
>    - Dock tooltip says "RetroNest" (not "EmuFront")
>    - The RetroNest app icon appears in the Dock
>    - The app shows your existing data because the saved root at
>      `~/Library/Application Support/EmuFront/config.json` still points to
>      `~/Documents/EmuFront/`. This is expected and temporary — once you
>      delete the old directories (step 2), the app will launch the setup
>      wizard on next run and you'll see "RetroNest Setup" with a default
>      root of `~/Documents/RetroNest/`.
>
> 2. **Clean start.** When you're ready to do the fresh setup:
>    ```
>    rm -rf "$HOME/Documents/EmuFront"
>    rm -rf "$HOME/Library/Application Support/EmuFront"
>    ```
>    Then launch RetroNest again. You'll see the wizard with
>    "RetroNest Setup" in the title and `~/Documents/RetroNest` as the
>    default root. Re-install emulators, re-import ROMs, re-sign-in
>    to ScreenScraper and RetroAchievements.
>
> 3. **Optional further renames** — none of these affect runtime behavior:
>    - Rename the repo dir: close Claude Code and any editor windows on
>      this project, then from the shell run
>      `mv "$HOME/Documents/EmuFront-Project" "$HOME/Documents/RetroNest-Project"`.
>    - Rename the current git branch:
>      `git branch -m chore/bundle-cleanup-and-review chore/retronest-rename`
>      (or any name you prefer).
>    - Rename the GitHub repository via the GitHub web UI.

- [ ] **Step 2: Wait for the user to report smoke test results**

Do not merge the branch to `main` automatically. Wait for the user to confirm the manual GUI smoke test looks right (or to report any regression) before proposing next steps.

---

## Self-review notes (for the plan author, not the executor)

After writing this plan, I checked it against the spec at
`docs/superpowers/specs/2026-04-09-rename-to-retronest-design.md`:

**Spec coverage — every enumerated rename from the spec has a task:**
- CMakeLists.txt (6 edits) → Task 1 ✓
- main.cpp setApplicationName, resolveResourceDir comments, DB filename (4 edits) → Task 2 ✓
- install_controller.cpp second DB filename → Task 3 Step 1 ✓
- database.h docstring → Task 3 Step 2 ✓
- User-Agent headers across 3 files (6 sites) → Task 3 Steps 3–5 ✓
- QML 4 files → Task 4 ✓
- CLAUDE.md 3 lines → Task 5 ✓
- "Single atomic commit" decision → Task 7 ✓
- "Continue on current branch" decision → Task 0 Step 1 verifies branch ✓
- "No data migration" decision → explicit in Out of Scope block and Task 6 Step 6 note about `[Paths] Root:` line ✓
- "Historical docs left alone" decision → Materials section B and Task 6 Step 7 grep scope ✓
- Manual follow-ups (delete old dirs, optional repo/branch rename) → Task 8 Step 1 ✓

**Placeholder scan:** every Edit has an explicit old_string and new_string. No "TBD", no "similar to Task N", no "add appropriate error handling". The commit message at Task 7 Step 2 is complete and does not contain fill-in slots.

**Type consistency:** no new types, methods, or properties are introduced by this plan. All changes are string replacements in existing code.
