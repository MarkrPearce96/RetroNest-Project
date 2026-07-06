# Process-Era Retirement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete `Backend::Process` and everything only it consumed — QProcess launch, INI-patching helpers, keystroke/SIGSTOP pause synthesis, the native-settings-UI feature — leaving GameSession, EmulatorAdapter, and AppController honestly libretro-only.

**Architecture:** Pure deletion refactor guided by a consumer audit (already run at planning time — results baked in below). The compiler is the safety net: every cut is followed by a build; a misjudged deletion fails loudly. The `EmulatorAdapter → LibretroAdapter` split stays (approved decision).

**Tech Stack:** C++17/Qt6, QtTest.

**Spec:** `docs/superpowers/specs/2026-07-06-process-era-retirement-design.md`.

## Global Constraints

- x86_64 daily driver never left broken; **no pushes** until the user passes the smoke gate; gate report is a **standalone message**.
- QML-facing GameSession/AppController API stays byte-identical for live flows.
- Builds: `arch -x86_64 /usr/local/bin/cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 -j 6` (absolute paths, background for full builds, `&& echo OK || echo FAILED` — never `; echo $?`). Reconfigure after CMakeLists edits.
- **Audit rule for any symbol not explicitly listed here:** grep `cpp/src cpp/qml cpp/tests` for consumers outside `adapters/emulator_adapter.*`; a consumer that is itself being deleted in this plan does not count. Zero live consumers → delete; any doubt → keep and note.

## Planning-time audit results (bake-in)

**Definitely dead (zero consumers):** `ensurePortableMarker`, `patchOrCreateConfigFile`, `writeConfigFile`, `pauseHotkeyVirtualKeyCode`, `qtKeyToPcsx2Name`, `retroAchievementsKeyMap`, `serialToFilenameFormat`, `suppressSetupWizard`; `patchIniKeys` (sole consumer is its own unit test).

**Dead once their call sites die in this plan:** `additionalLaunchArgs` (only app_controller's `openNativeEmulatorSettings`), `buildLaunchArgs` (only GameSession process launch + main.cpp CLI launch branch), `hotkeyVirtualKeyCode` + `HotkeyAction` enum (only AppController keystroke synthesis / process-era menu capability gating).

**LIVE — do NOT delete (spec assumptions the audit overturned):** `resolveExecutable` (LibretroAdapter resolves the core dylib path with it), `matchAsset`/`assetMatchRules` (core-zip release matching), `formatBinding`/`formatMouseBinding`/`formatWheelBinding`/`writeBindingDeviceHeader` (config_service writes controls.ini through them), `patchRetroAchievements`/`supportsRetroAchievements`, `supportsSaveOnExit`, `ensureConfig`, `configFilePath`, `biosFiles`, `pathsDefs`, `hotkeyBindingDefs`, everything settings/controller/preview related.

---

### Task 1: Loader goes libretro-only

**Files:**
- Modify: `cpp/src/core/manifest_loader.cpp` (backend parse + validate), `cpp/src/core/manifest.h` (comment)
- Test: extend `cpp/tests/test_manifest_libretro_fields.cpp`

- [ ] **Step 1: Failing test** — add to `test_manifest_libretro_fields.cpp`:

```cpp
    void testProcessBackendRejected() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "p.json", R"({
            "manifest_version":1,"id":"p","name":"P","systems":["s"],"github_repo":"o/r",
            "executable":"P","rom_extensions":["bin"],"launch_args":[],
            "backend":"process"
        })");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("unsupported backend.*process")));
        ManifestLoader loader;
        loader.loadAll(dir.path());
        QVERIFY(loader.emulatorById("p") == nullptr);   // rejected, not defaulted
    }
    void testMissingBackendRejected() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "m.json", R"({
            "manifest_version":1,"id":"m","name":"M","systems":["s"],"github_repo":"o/r",
            "executable":"M","rom_extensions":["bin"],"launch_args":[]
        })");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("unsupported backend")));
        ManifestLoader loader;
        loader.loadAll(dir.path());
        QVERIFY(loader.emulatorById("m") == nullptr);
    }
```

NOTE: existing tests that write manifests without `"backend":"libretro"` (`testBackendDefaultsToProcess`, the detail_page/unknown-key/version tests) will need `"backend":"libretro","core_dylib":"x_libretro.dylib"` added — do that in Step 3, and DELETE `testBackendDefaultsToProcess` outright (the behavior it pins is the one being removed).

- [ ] **Step 2: Run — expect FAIL** (backend still defaults to "process" and loads).
- [ ] **Step 3: Implement.** In `manifest_loader.cpp`: `m.backend = obj.value("backend").toString();` (no default), and in `validateManifest` add:

```cpp
    if (m.backend != QLatin1String("libretro")) {
        qWarning() << "[Manifest] Rejecting" << m.id << "— unsupported backend"
                   << (m.backend.isEmpty() ? QStringLiteral("<missing>") : m.backend)
                   << "(process-backend emulators were retired; only libretro cores load)";
        return false;
    }
```

Drop the now-dead `m.backend != "libretro"` github_repo special-case wording if it reads oddly after this (github_repo stays optional for local-only cores — keep that behavior, duckstation relies on it). Update `manifest.h`'s `backend` comment: `// must be "libretro" (process backend retired 2026-07)`. Fix up the existing tests per the Step-1 note.
- [ ] **Step 4: Build + run the manifest test binary → all slots PASS.**
- [ ] **Step 5: Commit** `retire-process: manifests must declare backend=libretro (silent process default removed)`.

### Task 2: GameSession sheds the Process backend

**Files:**
- Modify: `cpp/src/core/game_session.h` (Backend enum at :209, QProcess include/member, `onProcessFinished`/`onProcessError` decls :204-205), `cpp/src/core/game_session.cpp` (backend selection at :62-66, the entire process launch/terminate/monitor path), `cpp/src/main.cpp:~398` (CLI launch branch using `buildLaunchArgs`)

- [ ] **Step 1:** Read `game_session.cpp` end to end once. Delete: the `Backend` enum + `m_backend` (class is libretro-only; `isLibretro()` returns true — keep the method returning `true` if QML consumes it, audit `isLibretro` in qml first), the QProcess member + `onProcessFinished`/`onProcessError` + every `m_backend == Backend::Process` branch (launch, terminate/SIGTERM-save, resolveExecutable-for-binary call, window activation via `activateProcess`). The manifest-backend check at :62-66 becomes: reject (qWarning + fail launch) if `manifest.backend != "libretro"` — belt to Task 1's suspenders.
- [ ] **Step 2:** `main.cpp` CLI: the `buildLaunchArgs` branch at ~:398 is a process-era direct-launch flow. Delete the launch subcommand branch (or make it print "launching requires the GUI app — process-backend launching was retired"), whichever the surrounding runCli structure makes cleaner. Audit what runCli documents in its help text and keep it truthful.
- [ ] **Step 3:** Build (background) + full ctest → green. `grep -rn "QProcess" cpp/src/core/game_session.*` → zero hits.
- [ ] **Step 4: Commit** `retire-process: GameSession is libretro-only — QProcess launch path deleted`.

### Task 3: EmulatorAdapter slim-down

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h` (963 lines today), `cpp/src/adapters/emulator_adapter.cpp`
- Delete: `cpp/tests/test_patch_ini_keys.cpp` + its CMake target/add_test
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1:** Delete the "definitely dead" list from the audit bake-in above, plus (now orphaned by Task 2/4): `additionalLaunchArgs`, `buildLaunchArgs`, `hotkeyVirtualKeyCode` + the `HotkeyAction` enum. For each symbol: delete decl + impl + doc comment; run the audit rule from Global Constraints before cutting anything not on the list. Expect `IniKeyPatch` struct and related includes to fall out too if `patchIniKeys` was their last user.
- [ ] **Step 2:** Delete `cpp/tests/test_patch_ini_keys.cpp`, its `add_executable`/`target_link_libraries`/`add_test` block in CMakeLists, reconfigure.
- [ ] **Step 3:** Build (background) + full ctest → green (one fewer test). Note the header's new line count in the commit message.
- [ ] **Step 4: Commit** `retire-process: EmulatorAdapter slimmed — INI patching, launch args, keystroke hotkey surface deleted`.

### Task 4: Pause plumbing + native-settings feature deletion

**Files:**
- Modify: `cpp/src/ui/app_controller.h/.cpp` (`openNativeEmulatorSettings` ~:663-673, keystroke synthesis + SIGSTOP fallback ~:1049+, menu-capability gating ~:918-922), `cpp/src/ui/settings/generic_emulator_settings_dialog.cpp` (its `openNativeEmulatorSettings` button/call), `cpp/src/core/macos_fullscreen.h/.mm` (per-symbol audit)
- Possibly modify: QML consuming the deleted invokable (grep first)

- [ ] **Step 1:** `grep -rn "openNativeEmulatorSettings" cpp/qml cpp/src` — delete the invokable, its impl, the dialog button that calls it, and any QML reference. The dequarantine + `additionalLaunchArgs` + `QProcess::startDetached` body goes with it.
- [ ] **Step 2:** In-game-menu paths in app_controller: read ~:900-1100. The capability map entries gated on `hotkeyVirtualKeyCode(...) != 0` (:918-922) are process-era — libretro menu capabilities flow through their own branch; delete the process branch and the keystroke-synthesis function + SIGSTOP/SIGCONT fallback (:1049+). Verify the libretro pause path (`CoreRuntime::pause()`) is what remains.
- [ ] **Step 3:** `macos_fullscreen` per-symbol audit (343 lines): `registerGlobalHotkey`, `configurePanelWindow`/`restoreOriginalClass`/`makePanelKey`, `hideMenuBarAndDock`/`restoreMenuBarAndDock`, `activateOurApp` are LIVE (in-game menu overlay + RetroNest's own window). `activateProcess(pid)` and `screenIndexForProcess(pid)` are process-era — audit + delete if their consumers died in Step 2/Task 2.
- [ ] **Step 4:** Build (background) + full ctest → green. Greps: `grep -rn "SIGSTOP\|SIGCONT\|CGEventPostToPid" cpp/src` → zero; `grep -rn "activateProcess\|screenIndexForProcess" cpp/src` → zero (if deleted).
- [ ] **Step 5: Commit** `retire-process: pause is CoreRuntime-only; native-settings launcher and keystroke synthesis deleted`.

### Task 5: Installer audit

**Files:**
- Possibly modify: `cpp/src/services/emulator_installer.h/.cpp`

- [ ] **Step 1:** Read the installer's asset pipeline. The core-zip path (download → unzip into cores/ → dequarantine/sign/repair → version sidecar) is LIVE. Audit `.dmg`/`.tar.xz`/`.AppImage`/app-bundle handling: if reachable only through process-backend manifests (now rejected at load), delete those branches; if `matchAsset`'s generic platform-keyword fallback is shared plumbing, keep it. When in doubt, keep — this task is opportunistic, not load-bearing.
- [ ] **Step 2:** Build + ctest green. Commit `retire-process: installer sheds process-app asset handling` (skip the commit if the audit found nothing safely deletable — say so in the task report).

### Task 6: Docs, full verification, USER SMOKE GATE 11

- [ ] **Step 1:** CLAUDE.md: the "macOS launch rule" paragraph and the Build & Run "Note on the never use Launch Services rule" paragraph still describe launching emulator binaries as a live concern — trim both to a one-line historical note (the rule now only governs how RetroNest itself is launched vs how it was; keep whatever remains true). Grep CLAUDE.md for `QProcess`, `portable.txt`, `openNativeEmulatorSettings` and reconcile.
- [ ] **Step 2:** Full x86_64 rebuild (background) + full ctest → green. App launches (`--cli` smoke: manifests load, 5 emulators listed).
- [ ] **Step 3:** Commit `retire-process: CLAUDE.md matches the libretro-only reality`.
- [ ] **Step 4: USER SMOKE GATE 11 (standalone report, hard stop):**
  1. Boot one game per core — all five (GameSession surgery blast radius).
  2. In-game menu: open/close, pause/resume clean, no input bleed.
  3. Save & Quit on one core → Resume works.
  4. One settings change applies + persists (any core).
  5. In-app reinstall of one core (installer path).
- [ ] **Step 5:** After the gate: memory update, standalone report with push status (push only on explicit say-so).

## Risks

| Risk | Mitigation |
|---|---|
| Deleting a symbol with a hidden consumer | Audit rule + compiler; full suite after every task |
| GameSession surgery breaks launch/quit | Five-core gate item 1; QML API frozen |
| In-game-menu capability map regression | Gate items 2-3 exercise menu, pause, save-quit |
| CLI launch behavior change surprises scripts | runCli help text updated in the same commit |

## Effort

Tasks 1-3 ≈ half a session · Tasks 4-6 ≈ half a session · Gate ≈ user time.
