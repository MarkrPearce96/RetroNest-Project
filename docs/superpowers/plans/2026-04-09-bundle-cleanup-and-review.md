# Bundle-Transition Cleanup and Review Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run `code-simplifier:code-simplifier` across all of `cpp/` (excluding `cpp/tests/` and `cpp/CMakeLists.txt`) in dependency order with build+test+smoke gates between chunks, then run `code-review:code-review` against the bundle-transition commit `278efde` and produce a written report with a manual smoke-test checklist.

**Architecture:** Two phases. Phase 1 is five sequential code-simplifier passes — `core` → `adapters` → `services` → `ui` → `qml` — each gated by a clean build, passing tests, a CLI smoke launch, and a manual diff review for constraint violations, then committed as a discrete commit. Phase 2 runs the dedicated code-review skill against commit `278efde` with a 7-item static checklist and writes the report to a sibling file under `docs/superpowers/specs/`.

**Tech Stack:** Qt6, CMake, ctest, code-simplifier agent, code-review skill, git, EmuFront `--cli` mode for smoke launch.

**Spec:** `docs/superpowers/specs/2026-04-09-bundle-cleanup-and-review-design.md`

---

## Materials

### A. Simplifier agent constraint block

Every Phase 1 task hands this exact constraint list to its `code-simplifier:code-simplifier` agent invocation. Repeated in full inside each task because the engineer may read tasks out of order.

```
You may READ any file in the repository to understand callers and dependencies,
but you may EDIT only files inside the scope listed below. Do not edit any
file outside that scope, even to "fix" something.

Do not commit. The orchestrator commits after verification.

Hard constraints (these are non-negotiable for this codebase):

1. Do not rename public methods of EmulatorAdapter. Three concrete adapters
   (pcsx2_adapter, duckstation_adapter, ppsspp_adapter) override them.
   Renames cause silent v-table breakage.

2. Do not rename QML context properties. The names "app", "gameModel",
   "themeContext", "themeManager", "inputManager", "Theme", "SettingsTheme",
   "WizardTheme", "wizard", "emulators", and "installer" are bound by string
   in QML files (and by user themes outside this repo).

3. Do not change INI key names, INI section headers, or stored value
   formats. The application's settings UI reads and writes the same INI
   file the emulator reads. Round-tripping is critical and stored value
   formats (e.g. PPSSPP's "GraphicsBackend = 3 (VULKAN)") must match what
   the emulator writes back.

4. Do not change Qt signal/slot signatures. Qt's meta-object system
   resolves them by string at runtime. Renames break silently.

5. Do not introduce new abstractions. Per the project's CLAUDE.md: "do not
   add abstractions until they are clearly needed". Only collapse
   duplication that already exists or clarify existing code.

6. Do not touch cpp/CMakeLists.txt. Excluded from this pass entirely.

7. Do not touch theme QML files outside cpp/qml/.

8. Do not modify test files under cpp/tests/.

9. Preserve every public Q_OBJECT class name and its Q_INVOKABLE / Q_PROPERTY
   surface — these are looked up by string from QML.

When you're done, print:
- A one-line summary suitable for a commit subject
- A short bullet list of the key changes you made
- Any constraint you came close to violating and chose not to
```

### B. Verification gate

Every Phase 1 task ends with this gate. A chunk passes only if all four pass.

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp

# 1. Build must succeed
cmake --build build

# 2. All existing tests must pass
ctest --test-dir build --output-on-failure

# 3. CLI smoke launch must print emulator status without crashing
./build/EmuFront.app/Contents/MacOS/EmuFront --cli

# 4. Warning count must not have increased beyond the baseline captured
#    in Task 0 (see baseline.txt)
cmake --build build --clean-first 2>&1 | grep -c "warning:" > /tmp/emufront-warnings-after.txt
diff /tmp/emufront-warnings-baseline.txt /tmp/emufront-warnings-after.txt
```

If any of the four fails: STOP the pipeline. Do not move to the next chunk. Report the failure to the user and wait for their direction (revert, fix forward, or skip).

---

## Task 0: Pre-flight baseline

**Files:**
- Create: `/tmp/emufront-warnings-baseline.txt` (ephemeral)
- Read: `cpp/CMakeLists.txt`, `cpp/build/` state

- [ ] **Step 1: Verify clean working tree on main at the bundle commit**

```bash
cd /Users/mark/Documents/EmuFront-Project
git status
git log --oneline -3
```

Expected: working tree clean. `HEAD` is `ec4bac7 docs(spec): bundle-transition cleanup and review plan`. Two commits back is `278efde .app bundle`.

If the working tree is dirty, stop and ask the user how to handle it.

- [ ] **Step 2: Create the working branch**

```bash
cd /Users/mark/Documents/EmuFront-Project
git checkout -b chore/bundle-cleanup-and-review
```

Expected: switched to a new branch. Subsequent simplifier commits land here, not on main.

- [ ] **Step 3: Capture baseline warning count from a clean build**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build --clean-first 2>&1 | grep -c "warning:" > /tmp/emufront-warnings-baseline.txt
cat /tmp/emufront-warnings-baseline.txt
```

Expected: a single integer (the number of compiler warnings at the bundle commit). Record this number — every subsequent chunk must not exceed it.

- [ ] **Step 4: Confirm the build, tests, and CLI smoke are green at baseline**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build
ctest --test-dir build --output-on-failure
./build/EmuFront.app/Contents/MacOS/EmuFront --cli
```

Expected:
- Build: `[100%] Built target chdr-benchmark` (or similar success line), no errors.
- Tests: every test in the suite reports `Passed`. The current suite includes `IniFile`, `RomScanner`, `Iso9660Reader`, `SfoParser`, `BitmaskHelpers`, `PPSSPPSchema`.
- CLI smoke: prints `Loaded 3 emulator manifest(s)`, opens the database, prints the `=== Emulator Status ===` block listing DuckStation, PCSX2, PPSSPP, then exits.

If any of the three fails at baseline, stop and tell the user — the bundle commit itself is broken and Phase 1 cannot start until that's resolved.

---

## Task 1: Simplify `cpp/src/core/`

**Files:**
- May modify: any `.cpp` / `.h` / `.mm` file inside `cpp/src/core/` (~22 files)
- Read access: entire repository

- [ ] **Step 1: Spawn the code-simplifier agent for `cpp/src/core/`**

Use the `Agent` tool with `subagent_type: "code-simplifier:code-simplifier"`. Pass this exact prompt:

```
Run a code simplification pass over the files inside cpp/src/core/ in the
EmuFront repository at /Users/mark/Documents/EmuFront-Project.

Scope (you may EDIT only these):
  cpp/src/core/

You may READ any file in the repository to understand callers and
dependencies, but you may EDIT only files inside cpp/src/core/. Do not edit
any file outside that scope, even to "fix" something.

Do not commit. The orchestrator commits after verification.

Hard constraints (these are non-negotiable for this codebase):

1. Do not rename public methods of EmulatorAdapter. Three concrete adapters
   (pcsx2_adapter, duckstation_adapter, ppsspp_adapter) override them.
   Renames cause silent v-table breakage.

2. Do not rename QML context properties. The names "app", "gameModel",
   "themeContext", "themeManager", "inputManager", "Theme", "SettingsTheme",
   "WizardTheme", "wizard", "emulators", and "installer" are bound by string
   in QML files (and by user themes outside this repo).

3. Do not change INI key names, INI section headers, or stored value
   formats. The application's settings UI reads and writes the same INI
   file the emulator reads. Round-tripping is critical and stored value
   formats (e.g. PPSSPP's "GraphicsBackend = 3 (VULKAN)") must match what
   the emulator writes back.

4. Do not change Qt signal/slot signatures. Qt's meta-object system
   resolves them by string at runtime. Renames break silently.

5. Do not introduce new abstractions. Per the project's CLAUDE.md: "do not
   add abstractions until they are clearly needed". Only collapse
   duplication that already exists or clarify existing code.

6. Do not touch cpp/CMakeLists.txt. Excluded from this pass entirely.

7. Do not touch theme QML files outside cpp/qml/.

8. Do not modify test files under cpp/tests/.

9. Preserve every public Q_OBJECT class name and its Q_INVOKABLE / Q_PROPERTY
   surface — these are looked up by string from QML.

When you're done, print:
- A one-line summary suitable for a commit subject
- A short bullet list of the key changes you made
- Any constraint you came close to violating and chose not to
```

Expected: agent returns a summary, a bullet list of changes, and (optionally) any near-violation notes. The diff is left uncommitted in the working tree.

- [ ] **Step 2: Review the diff for constraint violations**

```bash
cd /Users/mark/Documents/EmuFront-Project
git diff --stat cpp/src/core/
git diff cpp/src/core/
```

Walk through the diff and check each constraint:
- No public `EmulatorAdapter` virtual renamed (grep the diff for `EmulatorAdapter::`)
- No QML context property name appears as a removed identifier (grep for the names listed in the constraint block)
- No INI key string literal changed (grep for changes to `setValue(`, `value(`, `[Section]` headers in IniFile usage)
- No `Q_OBJECT` class renamed
- No new helper class file created (compare against the pre-task file list)
- No file outside `cpp/src/core/` modified (`git diff --stat | grep -v "cpp/src/core/"` should be empty)

If any check fails, ask the user how to proceed. Do not auto-revert.

- [ ] **Step 3: Run the verification gate**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build
ctest --test-dir build --output-on-failure
./build/EmuFront.app/Contents/MacOS/EmuFront --cli
cmake --build build --clean-first 2>&1 | grep -c "warning:" > /tmp/emufront-warnings-after.txt
diff /tmp/emufront-warnings-baseline.txt /tmp/emufront-warnings-after.txt
```

Expected:
- Build: success.
- Tests: all six tests pass.
- CLI smoke: prints `=== Emulator Status ===` block.
- Warning diff: no output (counts identical).

If any check fails: STOP. Do not commit. Report the failure to the user.

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/core/
git commit -m "$(cat <<'EOF'
simplify(core): <one-line summary returned by the agent>

<bullet list of key changes from the agent>

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: one new commit on `chore/bundle-cleanup-and-review`.

---

## Task 2: Simplify `cpp/src/adapters/`

**Files:**
- May modify: any `.cpp` / `.h` file inside `cpp/src/adapters/` (~8 files)
- Read access: entire repository

- [ ] **Step 1: Spawn the code-simplifier agent for `cpp/src/adapters/`**

Use the `Agent` tool with `subagent_type: "code-simplifier:code-simplifier"`. Pass this exact prompt:

```
Run a code simplification pass over the files inside cpp/src/adapters/ in
the EmuFront repository at /Users/mark/Documents/EmuFront-Project.

Scope (you may EDIT only these):
  cpp/src/adapters/

You may READ any file in the repository to understand callers and
dependencies, but you may EDIT only files inside cpp/src/adapters/. Do not
edit any file outside that scope, even to "fix" something.

Do not commit. The orchestrator commits after verification.

Hard constraints (these are non-negotiable for this codebase):

1. Do not rename public methods of EmulatorAdapter. Three concrete adapters
   (pcsx2_adapter, duckstation_adapter, ppsspp_adapter) override them.
   Renames cause silent v-table breakage.

2. Do not rename QML context properties. The names "app", "gameModel",
   "themeContext", "themeManager", "inputManager", "Theme", "SettingsTheme",
   "WizardTheme", "wizard", "emulators", and "installer" are bound by string
   in QML files (and by user themes outside this repo).

3. Do not change INI key names, INI section headers, or stored value
   formats. The application's settings UI reads and writes the same INI
   file the emulator reads. Round-tripping is critical and stored value
   formats (e.g. PPSSPP's "GraphicsBackend = 3 (VULKAN)") must match what
   the emulator writes back.

4. Do not change Qt signal/slot signatures. Qt's meta-object system
   resolves them by string at runtime. Renames break silently.

5. Do not introduce new abstractions. Per the project's CLAUDE.md: "do not
   add abstractions until they are clearly needed". Only collapse
   duplication that already exists or clarify existing code.

6. Do not touch cpp/CMakeLists.txt. Excluded from this pass entirely.

7. Do not touch theme QML files outside cpp/qml/.

8. Do not modify test files under cpp/tests/.

9. Preserve every public Q_OBJECT class name and its Q_INVOKABLE / Q_PROPERTY
   surface — these are looked up by string from QML.

When you're done, print:
- A one-line summary suitable for a commit subject
- A short bullet list of the key changes you made
- Any constraint you came close to violating and chose not to
```

Expected: agent returns a summary, a bullet list of changes, and (optionally) any near-violation notes.

- [ ] **Step 2: Review the diff for constraint violations**

```bash
cd /Users/mark/Documents/EmuFront-Project
git diff --stat cpp/src/adapters/
git diff cpp/src/adapters/
```

Walk through the diff and check:
- The `EmulatorAdapter` virtual surface is unchanged. Compare the public method list before and after:
  ```bash
  git show HEAD:cpp/src/adapters/emulator_adapter.h | grep -E "virtual|Q_INVOKABLE"
  cat cpp/src/adapters/emulator_adapter.h | grep -E "virtual|Q_INVOKABLE"
  ```
  These two outputs must be identical.
- No INI key string literal changed in any of the three concrete adapters.
- No file outside `cpp/src/adapters/` modified.
- The PPSSPP `defaults write org.ppsspp.ppsspp …` call (look for `defaults write` in the diff context) is unchanged.

If any check fails, ask the user how to proceed.

- [ ] **Step 3: Run the verification gate**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build
ctest --test-dir build --output-on-failure
./build/EmuFront.app/Contents/MacOS/EmuFront --cli
cmake --build build --clean-first 2>&1 | grep -c "warning:" > /tmp/emufront-warnings-after.txt
diff /tmp/emufront-warnings-baseline.txt /tmp/emufront-warnings-after.txt
```

Expected: same as Task 1, Step 3. The `PPSSPPSchema` test in particular exercises the PPSSPP adapter — pay attention if it fails.

If any check fails: STOP. Do not commit. Report.

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/adapters/
git commit -m "$(cat <<'EOF'
simplify(adapters): <one-line summary returned by the agent>

<bullet list of key changes from the agent>

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Simplify `cpp/src/services/`

**Files:**
- May modify: any `.cpp` / `.h` file inside `cpp/src/services/` (~10 files)
- Read access: entire repository

- [ ] **Step 1: Spawn the code-simplifier agent for `cpp/src/services/`**

Use the `Agent` tool with `subagent_type: "code-simplifier:code-simplifier"`. Pass this exact prompt:

```
Run a code simplification pass over the files inside cpp/src/services/ in
the EmuFront repository at /Users/mark/Documents/EmuFront-Project.

Scope (you may EDIT only these):
  cpp/src/services/

You may READ any file in the repository to understand callers and
dependencies, but you may EDIT only files inside cpp/src/services/. Do not
edit any file outside that scope, even to "fix" something.

Do not commit. The orchestrator commits after verification.

Hard constraints (these are non-negotiable for this codebase):

1. Do not rename public methods of EmulatorAdapter. Three concrete adapters
   (pcsx2_adapter, duckstation_adapter, ppsspp_adapter) override them.
   Renames cause silent v-table breakage.

2. Do not rename QML context properties. The names "app", "gameModel",
   "themeContext", "themeManager", "inputManager", "Theme", "SettingsTheme",
   "WizardTheme", "wizard", "emulators", and "installer" are bound by string
   in QML files (and by user themes outside this repo).

3. Do not change INI key names, INI section headers, or stored value
   formats. The application's settings UI reads and writes the same INI
   file the emulator reads. Round-tripping is critical and stored value
   formats (e.g. PPSSPP's "GraphicsBackend = 3 (VULKAN)") must match what
   the emulator writes back.

4. Do not change Qt signal/slot signatures. Qt's meta-object system
   resolves them by string at runtime. Renames break silently.

5. Do not introduce new abstractions. Per the project's CLAUDE.md: "do not
   add abstractions until they are clearly needed". Only collapse
   duplication that already exists or clarify existing code.

6. Do not touch cpp/CMakeLists.txt. Excluded from this pass entirely.

7. Do not touch theme QML files outside cpp/qml/.

8. Do not modify test files under cpp/tests/.

9. Preserve every public Q_OBJECT class name and its Q_INVOKABLE / Q_PROPERTY
   surface — these are looked up by string from QML.

When you're done, print:
- A one-line summary suitable for a commit subject
- A short bullet list of the key changes you made
- Any constraint you came close to violating and chose not to
```

Expected: agent returns a summary, a bullet list of changes, and (optionally) any near-violation notes.

- [ ] **Step 2: Review the diff for constraint violations**

```bash
cd /Users/mark/Documents/EmuFront-Project
git diff --stat cpp/src/services/
git diff cpp/src/services/
```

Walk through the diff and check:
- No public Q_INVOKABLE method on `GameService`, `ScraperService`, `EmulatorService`, or `RaService` was renamed. Compare:
  ```bash
  git show HEAD:cpp/src/services/game_service.h | grep -E "Q_INVOKABLE|signals:|Q_PROPERTY"
  cat cpp/src/services/game_service.h | grep -E "Q_INVOKABLE|signals:|Q_PROPERTY"
  ```
  Repeat for each `*_service.h`. The two outputs must be identical for each file.
- No file outside `cpp/src/services/` modified.
- No HTTP request lost its timeout (the project's `feedback_code_review_patterns` rule: "timeouts on HTTP" is non-negotiable).

If any check fails, ask the user how to proceed.

- [ ] **Step 3: Run the verification gate**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build
ctest --test-dir build --output-on-failure
./build/EmuFront.app/Contents/MacOS/EmuFront --cli
cmake --build build --clean-first 2>&1 | grep -c "warning:" > /tmp/emufront-warnings-after.txt
diff /tmp/emufront-warnings-baseline.txt /tmp/emufront-warnings-after.txt
```

Expected: same as Task 1, Step 3.

If any check fails: STOP. Do not commit. Report.

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/services/
git commit -m "$(cat <<'EOF'
simplify(services): <one-line summary returned by the agent>

<bullet list of key changes from the agent>

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Simplify `cpp/src/ui/` and `cpp/src/ui/settings/`

**Files:**
- May modify: any `.cpp` / `.h` file inside `cpp/src/ui/` and `cpp/src/ui/settings/` (~40 files)
- Read access: entire repository

This is the largest chunk. The `ui/settings/` subtree contains the per-controller-type binding widgets (digital, analog, jogcon, negcon, popn, guitar, ds_negcon, ds_jogcon, ds_popn, analog_joystick, ds2, psp). Treat them carefully — the binding widgets are themselves an existing pattern; do not refactor the pattern, only clean within each widget.

- [ ] **Step 1: Spawn the code-simplifier agent for `cpp/src/ui/` (both directories)**

Use the `Agent` tool with `subagent_type: "code-simplifier:code-simplifier"`. Pass this exact prompt:

```
Run a code simplification pass over the files inside cpp/src/ui/ AND
cpp/src/ui/settings/ in the EmuFront repository at
/Users/mark/Documents/EmuFront-Project.

Scope (you may EDIT only these two directories):
  cpp/src/ui/
  cpp/src/ui/settings/

You may READ any file in the repository to understand callers and
dependencies, but you may EDIT only files inside those two directories. Do
not edit any file outside that scope, even to "fix" something.

Do not commit. The orchestrator commits after verification.

Hard constraints (these are non-negotiable for this codebase):

1. Do not rename public methods of EmulatorAdapter. Three concrete adapters
   (pcsx2_adapter, duckstation_adapter, ppsspp_adapter) override them.
   Renames cause silent v-table breakage.

2. Do not rename QML context properties. The names "app", "gameModel",
   "themeContext", "themeManager", "inputManager", "Theme", "SettingsTheme",
   "WizardTheme", "wizard", "emulators", and "installer" are bound by string
   in QML files (and by user themes outside this repo).

3. Do not change INI key names, INI section headers, or stored value
   formats. The application's settings UI reads and writes the same INI
   file the emulator reads. Round-tripping is critical and stored value
   formats (e.g. PPSSPP's "GraphicsBackend = 3 (VULKAN)") must match what
   the emulator writes back.

4. Do not change Qt signal/slot signatures. Qt's meta-object system
   resolves them by string at runtime. Renames break silently.

5. Do not introduce new abstractions. Per the project's CLAUDE.md: "do not
   add abstractions until they are clearly needed". Only collapse
   duplication that already exists or clarify existing code.

6. Do not touch cpp/CMakeLists.txt. Excluded from this pass entirely.

7. Do not touch theme QML files outside cpp/qml/.

8. Do not modify test files under cpp/tests/.

9. Preserve every public Q_OBJECT class name and its Q_INVOKABLE / Q_PROPERTY
   surface — these are looked up by string from QML.

Additional constraints specific to this chunk:

10. The per-controller-type binding widgets in cpp/src/ui/settings/
    (digital_, analog_, jogcon_, negcon_, popn_, guitar_, ds_negcon_,
    ds_jogcon_, ds_popn_, analog_joystick_, ds2_, psp_, ds_negcon_rumble_)
    follow an existing pattern. Do not refactor the pattern itself — only
    clean code within each widget.

11. The AppController class is exposed to QML as the "app" context property
    and has many Q_INVOKABLE methods. Do not rename any of them.

When you're done, print:
- A one-line summary suitable for a commit subject
- A short bullet list of the key changes you made
- Any constraint you came close to violating and chose not to
```

Expected: agent returns a summary, a bullet list of changes, and (optionally) any near-violation notes.

- [ ] **Step 2: Review the diff for constraint violations**

```bash
cd /Users/mark/Documents/EmuFront-Project
git diff --stat cpp/src/ui/
git diff cpp/src/ui/
```

Walk through the diff and check:
- `AppController`'s public Q_INVOKABLE surface is unchanged:
  ```bash
  git show HEAD:cpp/src/ui/app_controller.h | grep -E "Q_INVOKABLE|Q_PROPERTY|signals:"
  cat cpp/src/ui/app_controller.h | grep -E "Q_INVOKABLE|Q_PROPERTY|signals:"
  ```
  These must be identical.
- The same check for `ThemeContext`, `ThemeManager`, `WizardState`, `EmulatorListModel`, `InstallController`, `GameListModel`:
  ```bash
  for f in theme_context theme_manager wizard_state emulator_list_model install_controller game_list_model; do
    echo "--- $f ---"
    diff <(git show HEAD:cpp/src/ui/$f.h | grep -E "Q_INVOKABLE|Q_PROPERTY|signals:") \
         <(cat cpp/src/ui/$f.h | grep -E "Q_INVOKABLE|Q_PROPERTY|signals:")
  done
  ```
  Each `diff` must produce no output.
- No file outside `cpp/src/ui/` and `cpp/src/ui/settings/` modified.
- The binding widget pattern (one widget class per controller type) is preserved — no widget classes deleted, none merged together.

If any check fails, ask the user how to proceed.

- [ ] **Step 3: Run the verification gate**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build
ctest --test-dir build --output-on-failure
./build/EmuFront.app/Contents/MacOS/EmuFront --cli
cmake --build build --clean-first 2>&1 | grep -c "warning:" > /tmp/emufront-warnings-after.txt
diff /tmp/emufront-warnings-baseline.txt /tmp/emufront-warnings-after.txt
```

Expected: same as Task 1, Step 3.

If any check fails: STOP. Do not commit. Report.

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/src/ui/
git commit -m "$(cat <<'EOF'
simplify(ui): <one-line summary returned by the agent>

<bullet list of key changes from the agent>

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Simplify `cpp/qml/AppUI/` and `cpp/qml/SetupWizard/`

**Files:**
- May modify: any `.qml` / `.js` file inside `cpp/qml/AppUI/` and `cpp/qml/SetupWizard/` (~35 files)
- Read access: entire repository

QML is its own language with its own conventions. The simplifier agent should handle property bindings, signal handlers, and component composition idiomatically — not try to refactor QML as if it were JavaScript.

- [ ] **Step 1: Spawn the code-simplifier agent for `cpp/qml/`**

Use the `Agent` tool with `subagent_type: "code-simplifier:code-simplifier"`. Pass this exact prompt:

```
Run a code simplification pass over the files inside cpp/qml/AppUI/ AND
cpp/qml/SetupWizard/ in the EmuFront repository at
/Users/mark/Documents/EmuFront-Project.

Scope (you may EDIT only these two directories):
  cpp/qml/AppUI/
  cpp/qml/SetupWizard/

You may READ any file in the repository to understand callers and
dependencies, but you may EDIT only files inside those two directories. Do
not edit any file outside that scope, even to "fix" something.

Do not commit. The orchestrator commits after verification.

Hard constraints (these are non-negotiable for this codebase):

1. Do not rename QML context properties. The names "app", "gameModel",
   "themeContext", "themeManager", "inputManager", "Theme", "SettingsTheme",
   "WizardTheme", "wizard", "emulators", and "installer" are bound by string
   in QML files (and by user themes outside this repo).

2. Do not rename top-level QML component types (the .qml file basenames).
   They are imported by name from other QML files and from C++
   loadFromModule calls.

3. Do not introduce new abstractions. Per the project's CLAUDE.md: "do not
   add abstractions until they are clearly needed". Only collapse
   duplication that already exists or clarify existing code.

4. Do not touch theme QML files outside cpp/qml/. Themes live at the
   project root under themes/ and are user-overrideable at runtime.

5. Do not change Keys.on* handler key codes. The unified input system maps
   controller buttons to specific Qt key events (Key_Return, Key_Back,
   Key_Backspace, Key_M, arrow keys). Renaming these breaks controller
   navigation.

6. Use Key_Back, NOT Key_Escape, for B-button handlers. The CLAUDE.md
   notes: "Key_Back not Key_Escape for B-button — prevents Qt Shortcut
   interception". This is a deliberate choice; do not "correct" it.

7. Do not change signal names emitted by inputManager (navigateStart,
   navigateShift, bindingCaptured, etc.).

8. Preserve forceActiveFocus() calls. They look redundant but exist for a
   reason — see the focus_management feedback memory.

9. Do not modify any file outside cpp/qml/AppUI/ and cpp/qml/SetupWizard/.

When you're done, print:
- A one-line summary suitable for a commit subject
- A short bullet list of the key changes you made
- Any constraint you came close to violating and chose not to
```

Expected: agent returns a summary, a bullet list of changes, and (optionally) any near-violation notes.

- [ ] **Step 2: Review the diff for constraint violations**

```bash
cd /Users/mark/Documents/EmuFront-Project
git diff --stat cpp/qml/
git diff cpp/qml/
```

Walk through the diff and check:
- No QML context property name (`app.`, `gameModel.`, `themeContext.`, `themeManager.`, `inputManager.`, `Theme.`, `SettingsTheme.`, `WizardTheme.`, `wizard.`, `emulators.`, `installer.`) appears as a removed reference without an equivalent replacement.
- No `.qml` file was renamed (only contents may change).
- No `Keys.onPressed` / `Keys.on<Name>Pressed` handler had its key code changed. Grep the diff for `Key_`:
  ```bash
  git diff cpp/qml/ | grep -E "^[-+].*Key_"
  ```
  Inspect each change. Removals/additions of `Key_Escape` in a B-button context are a red flag — they should be `Key_Back`.
- No `forceActiveFocus()` call was deleted.
- No file outside `cpp/qml/AppUI/` and `cpp/qml/SetupWizard/` modified.

If any check fails, ask the user how to proceed.

- [ ] **Step 3: Run the verification gate**

```bash
cd /Users/mark/Documents/EmuFront-Project/cpp
cmake --build build
ctest --test-dir build --output-on-failure
./build/EmuFront.app/Contents/MacOS/EmuFront --cli
cmake --build build --clean-first 2>&1 | grep -c "warning:" > /tmp/emufront-warnings-after.txt
diff /tmp/emufront-warnings-baseline.txt /tmp/emufront-warnings-after.txt
```

Expected: same as Task 1, Step 3. Note: the CLI smoke launch does NOT load any QML, so it will not catch a broken QML file. The build step does — Qt's qmlcachegen runs at build time and fails on invalid QML. Watch the build output for `qmlcachegen` errors.

If any check fails: STOP. Do not commit. Report.

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/EmuFront-Project
git add cpp/qml/
git commit -m "$(cat <<'EOF'
simplify(qml): <one-line summary returned by the agent>

<bullet list of key changes from the agent>

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Run code-review on the bundle commit

**Files:**
- Read: commit `278efde` (the bundle change), the post-Phase-1 state of `cpp/src/main.cpp`, `cpp/CMakeLists.txt`, `cpp/src/adapters/ppsspp_adapter.cpp`, `cpp/src/core/macos_fullscreen.mm`, `cpp/src/core/game_session.cpp`, `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Confirm we're at the end of Phase 1**

```bash
cd /Users/mark/Documents/EmuFront-Project
git log --oneline -10
```

Expected: most recent commits (top down) are
`simplify(qml): …`, `simplify(ui): …`, `simplify(services): …`,
`simplify(adapters): …`, `simplify(core): …`,
`docs(spec): bundle-transition cleanup and review plan`,
`.app bundle`. If any of the simplify commits are missing or out of order, stop and resolve before running the review.

- [ ] **Step 2: Invoke the code-review skill**

Use the `Skill` tool with `skill: "code-review:code-review"`. Pass this exact instruction:

```
Review commit 278efde in this repository against the post-current-HEAD state
of the codebase. The commit converts EmuFront from a bare Unix executable
to a macOS .app bundle.

Work through this 7-item static checklist explicitly. For each item, state
the file/line you checked, what you found, and a verdict (OK / NEEDS FIX /
NEEDS RUNTIME VERIFICATION).

1. resolveResourceDir candidate paths in cpp/src/main.cpp.
   Are the ".." counts correct for every layout it claims to handle (dev
   bare exe, dev .app bundle, installed .app bundle)? Is the fallback path
   sensible? What happens if the binary is nested differently in CI or in
   a packaged distribution?

2. CFBundleIdentifier in cpp/CMakeLists.txt.
   The value is "com.markpearce.emufront". Is this sensible and
   non-colliding? Does it conflict with any other bundle ID the app talks
   to (org.ppsspp.ppsspp, PCSX2's bundle, DuckStation's bundle)?

3. PPSSPP `defaults write` calls in cpp/src/adapters/ppsspp_adapter.cpp.
   Verify they still target PPSSPP's bundle ID (org.ppsspp.ppsspp), NOT
   EmuFront's (com.markpearce.emufront). This is the most likely place to
   silently regress now that EmuFront has its own bundle identity.

4. Carbon global hotkey registration.
   Find the RegisterEventHotKey call (likely in cpp/src/ui/app_controller.cpp
   or a sibling file). Anything bundle-specific in the registration flow
   that would behave differently for a bundled process?

5. MacFullscreen::hideMenuBarAndDock() in cpp/src/core/macos_fullscreen.mm.
   Does the activation policy and dock-hide trick still work for a bundled
   app, or does the bundle change require explicit LSUIElement in
   Info.plist or NSApp.setActivationPolicy(.accessory) to work? CMake
   currently does not generate an Info.plist with LSUIElement.

6. Direct-exec rule. Per CLAUDE.md, GameSession::startEmulator() and
   AppController::openNativeEmulatorSettings() must call
   QProcess::start(execPath, args) directly and never go through `open` or
   Launch Services. Confirm both call sites still do this.

7. PPSSPP portable mode key (UserPreferredMemoryStickDirectoryPath).
   Find the call that sets this NSUserDefaults value (likely in
   cpp/src/adapters/ppsspp_adapter.cpp). Verify it is still being written
   against PPSSPP's bundle ID before launch and that the bundle change did
   not accidentally redirect it to EmuFront's bundle.

After the 7 items, write a separate "Runtime issues — manual smoke test
required" section listing these four checks the user must perform manually:
- Double-click EmuFront.app from Finder once and confirm it launches
  (Gatekeeper / quarantine xattr check)
- Confirm the Dock icon does not appear and the macOS menu bar stays hidden
- Confirm Cmd+Escape still triggers the in-game menu when an emulator is
  running
- Confirm a PPSSPP launch still resolves the portable memory stick path to
  the EmuFront-managed location

Return the full review as markdown text. Do not write any files yourself.
```

Expected: the skill returns a markdown review report covering all 7 static items plus the manual checklist section.

---

## Task 7: Write the review report file

**Files:**
- Create: `docs/superpowers/specs/2026-04-09-bundle-cleanup-review-report.md`

- [ ] **Step 1: Write the review report to disk**

Use the `Write` tool with `file_path: "/Users/mark/Documents/EmuFront-Project/docs/superpowers/specs/2026-04-09-bundle-cleanup-review-report.md"` and content:

```markdown
# Bundle-Transition Code Review Report

**Date:** 2026-04-09
**Commit reviewed:** `278efde` (bundle transition)
**State reviewed against:** post-Phase-1 HEAD of `chore/bundle-cleanup-and-review`
**Spec:** `docs/superpowers/specs/2026-04-09-bundle-cleanup-and-review-design.md`
**Plan:** `docs/superpowers/plans/2026-04-09-bundle-cleanup-and-review.md`

## Static review

<paste the full markdown returned by the code-review skill in Task 6 here>

## Runtime issues — manual smoke test required by user

The static review above cannot verify these. Please run through them
manually before merging the cleanup branch:

- [ ] Double-click `EmuFront.app` from Finder once and confirm it launches
      (catches Gatekeeper / quarantine xattr issues on first launch)
- [ ] Confirm the Dock icon does not appear and the macOS menu bar stays
      hidden during the session
- [ ] Confirm `Cmd+Escape` still triggers the in-game menu when an emulator
      is running
- [ ] Launch a PPSSPP game and confirm it still uses the EmuFront-managed
      memory stick path (e.g. saves land in
      `{root}/saves/psp/` rather than PPSSPP's default location)
```

Expected: file created at the path above.

- [ ] **Step 2: Commit the report**

```bash
cd /Users/mark/Documents/EmuFront-Project
git add docs/superpowers/specs/2026-04-09-bundle-cleanup-review-report.md
git commit -m "$(cat <<'EOF'
docs(review): bundle-transition code review report

Static review of commit 278efde (bundle transition) against post-cleanup
HEAD, plus a manual smoke-test checklist for the four runtime issues
review can't verify.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: one new commit on `chore/bundle-cleanup-and-review`.

---

## Task 8: Hand off the manual smoke test to the user

- [ ] **Step 1: Summarize the work and present the smoke-test checklist**

Tell the user:

> Phase 1 (code-simplifier sweep) is complete: 5 commits on
> `chore/bundle-cleanup-and-review`, build green, tests green at every
> step. Phase 2 (bundle review) is complete: report at
> `docs/superpowers/specs/2026-04-09-bundle-cleanup-review-report.md`.
>
> The review found `<N>` items that need fixing (or zero items — all clear)
> — see the report for details.
>
> Before this branch can land on `main`, please run the four manual
> smoke-test checks listed at the bottom of the report:
>
> 1. Double-click `EmuFront.app` in Finder
> 2. Confirm dock/menu bar stay hidden
> 3. Confirm `Cmd+Escape` works in-game
> 4. Launch a PPSSPP game and confirm portable saves location
>
> Let me know how the smoke tests go.

- [ ] **Step 2: Wait for user feedback**

Do not merge the branch automatically. Wait for the user to confirm the smoke tests pass (or report failures) before proposing next steps.

---

## Self-review notes (for the plan author, not the executor)

After writing this plan, I checked it against the spec:

**Spec coverage:**
- Phase 1, five chunks in dependency order → Tasks 1–5 ✓
- Per-chunk gate (build, tests, smoke) → Step 3 of each task ✓
- Constraint list handed to every agent → Materials §A, repeated in each task ✓
- "Stop on failure, do not roll forward" → explicit in each task's Step 3 ✓
- Phase 2 code-review against 278efde with 7-item checklist → Task 6 ✓
- Review report at `docs/superpowers/specs/2026-04-09-bundle-cleanup-review-report.md` → Task 7 ✓
- Manual smoke-test checklist for 4 runtime items → Task 7 (in report) + Task 8 (handoff) ✓
- Out-of-scope items (CMakeLists, tests, themes, manifests) → reflected in constraint list and by scope of edit-allowed dirs ✓

**Placeholders:** None. Every step has the actual command, prompt text, or content the engineer needs.

**Type consistency:** No new types or methods are defined by this plan — it only orchestrates existing tooling.
