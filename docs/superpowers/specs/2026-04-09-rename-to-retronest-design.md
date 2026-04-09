# Rename EmuFront → RetroNest

**Date:** 2026-04-09
**Status:** Design approved, ready for implementation plan
**Branch:** `chore/bundle-cleanup-and-review` (continues on same branch; merges together with the bundle cleanup + icon work)

## Context

EmuFront is being renamed to RetroNest. The user already dropped a
`RetroNest` logo into `assets/` and we wired it up as the macOS app icon in
commit `b8ea97b`. This spec covers the rest of the rename — build system,
source, QML, project doc — so that nothing still says "EmuFront" in places
that matter.

## Guiding decisions (already made with the user)

1. **No data migration code.** The user will manually delete the existing
   `~/Documents/EmuFront/` root directory and re-run the setup wizard after
   the rename lands. No need for first-launch config.json migration, DB
   filename migration, or on-disk directory renaming logic.
2. **Historical design/plan docs under `docs/superpowers/` are left alone.**
   They are historical records of past work; rewriting them would be
   revisionist and would pollute the diff with ~130 line changes in files
   that don't affect runtime behavior.
3. **Single atomic commit.** Splitting the rename into build/source/QML
   sub-commits would produce intermediate states that don't compile
   (e.g. QML titles referencing old name while source has new name).
4. **Continue on the current branch** (`chore/bundle-cleanup-and-review`)
   rather than cutting a fresh branch, so all the related work (bundle
   cleanup → icon → rename) merges together.

## Scope — changes

### Build system (`cpp/CMakeLists.txt`, 6 lines)

- `project(EmuFront LANGUAGES CXX OBJCXX)` → `project(RetroNest …)`
  - Changes `${PROJECT_NAME}`, so the CMake target, the executable, and
    the `.app` bundle all become `RetroNest`.
  - Build output path changes from `cpp/build/EmuFront.app/Contents/MacOS/EmuFront`
    to `cpp/build/RetroNest.app/Contents/MacOS/RetroNest`.
- `SCREENSCRAPER_SOFTNAME "EmuFront"` → `"RetroNest"`
  - Sent to ScreenScraper's API as the software-name identifier.
    Low-risk — ScreenScraper treats this as a new client string; no
    re-registration required.
- `MACOSX_BUNDLE_BUNDLE_NAME "EmuFront"` → `"RetroNest"`
  - CFBundleName — affects Dock tooltip, About menu, Finder display.
- `MACOSX_BUNDLE_GUI_IDENTIFIER "com.markpearce.emufront"` → `"com.markpearce.retronest"`
  - CFBundleIdentifier. Changing this means any per-app NSUserDefaults
    EmuFront might have set under its old bundle ID are orphaned. The
    codebase does not currently use NSUserDefaults for its own
    preferences (only for PPSSPP's domain via `defaults write
    org.ppsspp.ppsspp …`, which is unaffected), so this is safe.
- Two comment references containing "EmuFront" — updated for consistency.

### C++ / Obj-C++ source

- **`cpp/src/main.cpp`:**
  - `app.setApplicationName("EmuFront")` → `"RetroNest"`. This changes
    `QStandardPaths::AppDataLocation` from
    `~/Library/Application Support/EmuFront/` to
    `.../RetroNest/`, which is where `Paths::loadSavedRoot()` reads
    `config.json` from. Per decision #1, the old directory is abandoned;
    the user will re-run the wizard and a new `config.json` will be
    written under the new name.
  - `"/emulator-frontend.db"` → `"/retronest.db"`. Database filename.
  - Three `resolveResourceDir` path comments that mention `EmuFront.app`.
- **`cpp/src/ui/install_controller.cpp` line 126** — a second hardcoded
  `"/emulator-frontend.db"` that must be renamed in lockstep with the
  copy in `main.cpp`, or the app would end up writing to two different
  database files depending on code path.
- **`cpp/src/core/database.h` line 48** — docstring referencing the DB
  filename. Updated for accuracy.
- **User-Agent headers (6 sites, `"EmuFront/1.0"` → `"RetroNest/1.0"`):**
  - `cpp/src/core/emulator_installer.cpp` — 4 sites (lines 30, 64, 346, 412)
  - `cpp/src/core/github_client.h` — 1 site (line 27)
  - `cpp/src/core/ra_client.cpp` — 1 site (line 30)

  The User-Agent identifies the app to GitHub (for the release API
  during emulator installs), to RetroAchievements (web API), and to the
  download path for emulator binaries. Changing the string is cosmetic
  for the API side; no re-registration needed.

### QML (user-facing)

- `cpp/qml/AppUI/AppWindow.qml` line 8 — `title: "EmuFront"` →
  `title: "RetroNest"`. The borderless window title (not visible to users
  but read by accessibility tooling and the window server).
- `cpp/qml/SetupWizard/Main.qml` line 10 — `title: "EmuFront Setup"` →
  `"RetroNest Setup"`. Wizard window title.
- `cpp/qml/SetupWizard/WelcomePage.qml` line 16 — splash text `"EmuFront"`
  → `"RetroNest"`. The big welcome label shown on first launch.
- `cpp/qml/SetupWizard/FolderPage.qml` line 15 — `wizard.rootPath = dir + "/EmuFront"`
  → `dir + "/RetroNest"`. Default root directory name suggested to the
  user by the wizard after they pick a parent directory.

### Project doc

- **`CLAUDE.md`** — build/run commands (`./build/EmuFront.app/Contents/MacOS/EmuFront`
  → `./build/RetroNest.app/Contents/MacOS/RetroNest`), title ("EmuFront —
  Claude Instructions" → "RetroNest — Claude Instructions"), and the
  non-negotiable UX rule section where "EmuFront" is referenced.

## Out of scope

- **`docs/superpowers/specs/` and `docs/superpowers/plans/`** (14 files,
  historical design and plan documents). Left alone as historical records.
- **Git commit history** — commits say "EmuFront"; rewriting history is
  unsafe and destroys accurate record.
- **Git branch name** (`chore/bundle-cleanup-and-review`) — user can rename
  manually later if desired.
- **Repo directory** at `/Users/mark/Documents/EmuFront-Project` — this is
  on the user's filesystem and renaming it would break editor windows,
  git worktrees, and the Claude Code session itself. User renames manually
  with `mv` after merge if they want.
- **No data migration code** of any kind. Per decision #1.
- **RetroAchievements console IDs, ROM serials, BIOS filenames, controller
  binding format strings, INI keys and sections, stored value formats** —
  these are third-party identifiers that happen to live in the same
  codebase; they are unrelated to the app name.

## Verification gate

Same pattern used for the five simplification chunks in this branch:

1. `cmake -B build -DCMAKE_PREFIX_PATH=...` — reconfigure so CMake picks
   up the new `project()` name and generates the target at the new path.
2. `cmake --build build` — must succeed, no new warnings beyond the
   baseline of 11 captured at Task 0.
3. `ctest --test-dir build --output-on-failure` — all 6 tests must pass
   (IniFile, RomScanner, Iso9660Reader, SfoParser, BitmaskHelpers,
   PPSSPPSchema).
4. CLI smoke test at the **new** bundle path:
   `./build/RetroNest.app/Contents/MacOS/RetroNest --cli` — must print the
   `=== Emulator Status ===` block.
5. Manual cleanup: delete the stale `cpp/build/EmuFront.app` bundle from
   the previous configuration if CMake didn't remove it automatically.

## Success criteria

- Single atomic commit titled `refactor: rename EmuFront to RetroNest` (or
  similar) lands on `chore/bundle-cleanup-and-review`.
- `git grep EmuFront cpp/ CLAUDE.md` returns no matches after the commit.
- `git grep emulator-frontend cpp/ CLAUDE.md` returns no matches.
- Build and tests green.
- CLI smoke launch at the new bundle path works.
- User manually verifies GUI launch via `open cpp/build/RetroNest.app`
  from Finder: new bundle name, RetroNest icon, wizard shows "RetroNest
  Setup" and offers `~/Documents/RetroNest/` as the default root.

## Manual follow-ups for the user (outside this work)

These are noted here so they don't get lost, but they're not implementation
tasks for the coding agent:

- Delete `~/Documents/EmuFront/` to free disk space (emulator binaries,
  ROMs, saves). User runs this by hand when ready.
- Optionally rename the repo directory `/Users/mark/Documents/EmuFront-Project`
  → `/Users/mark/Documents/RetroNest-Project` after closing Claude Code
  and any open editor windows.
- Optionally rename the git branch
  (`git branch -m chore/bundle-cleanup-and-review chore/rename-to-retronest`).
- Optionally rename the GitHub repository via the GitHub UI.
- Re-run the setup wizard on first launch after the rename. Install
  emulators, import ROMs, re-scrape metadata, re-sign in to
  RetroAchievements and ScreenScraper.
