# Bundle-Transition Code Review Report

**Date:** 2026-04-09
**Commit reviewed:** `278efde` (`.app bundle`)
**State reviewed against:** post-Phase-1 HEAD of `chore/bundle-cleanup-and-review`
**Spec:** `docs/superpowers/specs/2026-04-09-bundle-cleanup-and-review-design.md`
**Plan:** `docs/superpowers/plans/2026-04-09-bundle-cleanup-and-review.md`

## Phase 1 outcome (for context)

Five sequential simplification commits landed on `chore/bundle-cleanup-and-review`:

| SHA | Scope | Δ |
|---|---|---|
| `d619f9a` | `cpp/src/core/` | -9 / +2 |
| `34f2050` | `cpp/src/adapters/` | -145 / +78 |
| `caed4cd` | `cpp/src/services/` | -55 / +49 |
| `d6436f2` | `cpp/src/ui/` + `cpp/src/ui/settings/` | -237 / +210 |
| `109abdb` | `cpp/qml/AppUI/` + `cpp/qml/SetupWizard/` | -247 / +167 |

At every chunk the verification gate (build + 6/6 tests + CLI smoke + warning count = 11) passed. Two spec reviews and two code-quality reviews ran per chunk; all five chunks were approved.

Two minor comment bugs surfaced by the bundle review were fixed inline as commit `32a6925` (below).

## Static review (Phase 2)

This section is the full output of `superpowers:code-reviewer` run against commit `278efde`, against the post-Phase-1 state of the tree.

---

# Bundle Transition Code Review — commit 278efde

## Summary

The commit correctly converts EmuFront to a macOS `.app` bundle and addresses the two main launch-from-Finder regressions (QML module path resolution and resource directory resolution). The core mechanics are sound: PPSSPP's `defaults write` still targets `org.ppsspp.ppsspp`, emulator launch remains a direct `QProcess::start`/`startDetached` call, and the Carbon hotkey registration has no bundle-specific dependencies. One comment in `main.cpp` contains a factually wrong `..` count that contradicts the correct path in the adjacent line of code, and the `CMakeLists.txt` comment contains a misleading claim about why the bundle ID matters. Neither error affects runtime behavior, but both will mislead future readers. The missing `LSUIElement` key is a design choice with a known tradeoff (brief Dock icon flash at startup) that must be verified manually.

## Static checklist

### 1. resolveResourceDir candidate paths

**Verdict:** NEEDS FIX (comment only — the path itself is correct) — **FIXED in commit `32a6925`**

**File:** `cpp/src/main.cpp` lines 55–61

The block comment at lines 55–56 stated the bundle layout requires `"../../../../../../"` and labeled it "up 6". The inline comment at line 61 labels the same candidate "up 5: MacOS,Contents,EmuFront.app,build,cpp". The actual path string on line 61 is `"/../../../../../"`, which contains five `..` segments. Five levels up from `cpp/build/EmuFront.app/Contents/MacOS` resolves to the project root (`EmuFront-Project/`), which is where `manifests/` and `themes/` live. The path is therefore arithmetically correct. The block comment was wrong on two counts: it showed six `..` pairs in the example string, and labeled the count "6" while enumerating only five components. Six levels up would land at `/Users/mark/Documents`, not the project root, which would cause `QDir::exists()` to return false and fall through to the fallback. The block comment was corrected in follow-up commit `32a6925`.

The fallback at line 67 returns the bare-exe path unconditionally. If neither the bare-exe candidate nor the bundle candidate resolves (e.g., in a CI artifact where the binary is somewhere else), the fallback will still return the bare-exe path, and the caller will get a `qCritical` from `ManifestLoader::loadAll`. This is acceptable as a developer-facing failure mode.

The installed-bundle candidate `exeDir + "/../Resources/" + name` at line 62 is correct for the standard macOS bundle layout (`Contents/MacOS/../Resources/`), but there is no CMake `install()` target or `macdeployqt` step that copies `manifests/` or `themes/` into the bundle's `Resources/` directory. This means the installed-bundle candidate will never match in the current build system. This is a forward-looking gap rather than a regression — the bare-exe path (candidate 1) continues to work during development, and the installed-bundle path is there as scaffolding for a future packaging step.

### 2. CFBundleIdentifier

**Verdict:** OK — **comment fixed in commit `32a6925`**

**File:** `cpp/CMakeLists.txt` lines 150–156, and the generated `cpp/build/EmuFront.app/Contents/Info.plist` line 14.

`com.markpearce.emufront` is a well-formed reverse-DNS identifier in a domain the author controls. It does not collide with `org.ppsspp.ppsspp`, PCSX2 (`net.pcsx2.pcsx2`), DuckStation (`org.duckstation.DuckStation`), or any Qt framework or SDL2 bundle identifier. Apple's reserved prefixes are `com.apple.*` — no concern here.

The comment at `CMakeLists.txt` line 147 contained a misleading parenthetical: `"(e.g. PPSSPP's NSUserDefaults portable path) have a stable key"`. EmuFront's bundle ID has no effect on PPSSPP's `NSUserDefaults` domain; PPSSPP's preferences are stored under `org.ppsspp.ppsspp` regardless of what bundle ID EmuFront has (confirmed below in item 3). EmuFront itself does not use `NSUserDefaults` for any of its own preferences. The real motivation for the bundle — exercising bundle-only macOS behaviors (`NSApplicationActivationPolicyRegular`, Launch Services, Gatekeeper) during development — is stated correctly in the second half of that comment. The parenthetical was replaced with the accurate rationale in follow-up commit `32a6925`.

### 3. PPSSPP `defaults write` targets

**Verdict:** OK

**File:** `cpp/src/adapters/ppsspp_adapter.cpp` line 363

```cpp
QProcess::execute("defaults", {"write", "org.ppsspp.ppsspp",
                               "UserPreferredMemoryStickDirectoryPath", configDir()});
```

The target domain is hard-coded as `org.ppsspp.ppsspp`. EmuFront's new bundle ID (`com.markpearce.emufront`) has no effect on this call; `defaults write` writes to the named domain unconditionally. There is no other `defaults write`, `NSUserDefaults`, or `UserPreferredMemoryStickDirectoryPath` reference anywhere in the codebase (confirmed by grep across `cpp/src/`). This is the only call site, and it is correct.

### 4. Carbon global hotkey registration

**Verdict:** OK (with one clarification)

**File:** `cpp/src/core/macos_fullscreen.mm` lines 58–78

`RegisterEventHotKey` uses `GetApplicationEventTarget()` and the Carbon event dispatcher. Neither of these depends on the process having a bundle identifier. The hotkey ID `'EMUF'` is an arbitrary four-byte creator code and does not conflict with any Apple-reserved value. The registration happens inside `AppController`'s constructor (confirmed at `app_controller.cpp` line 43), which is called after `QApplication app(argc, argv)` at `main.cpp` line 40. `QApplication`'s constructor initializes `NSApp` on macOS before returning, so `InstallApplicationEventHandler` and `GetApplicationEventTarget` are safe at the point of call in both bare-exe and bundled forms.

One clarification: Carbon's `RegisterEventHotKey` requires the calling process to be a foreground (UI) application for the hotkey to fire when another app is in front. A bundled app with `NSApplicationActivationPolicyRegular` (the default when there is no `LSUIElement` key) satisfies this. A bare executable where Qt happened to force `Prohibited` or `Accessory` policy would not. **This means the bundle change may actually improve hotkey reliability, not reduce it.**

### 5. `MacFullscreen::hideMenuBarAndDock()`

**Verdict:** NEEDS RUNTIME VERIFICATION

**File:** `cpp/src/core/macos_fullscreen.mm` lines 25–31, and `cpp/build/EmuFront.app/Contents/Info.plist`

`hideMenuBarAndDock()` calls `[NSApp setPresentationOptions: NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar]`. This API requires the calling process to have `NSApplicationActivationPolicyRegular`. A bundled app without `LSUIElement` defaults to Regular policy, which is correct. The call should work.

However, `hideMenuBarAndDock()` is called at `main.cpp` line 215, which is after `engine.loadFromModule("AppUI", "AppWindow")` and after the window is retrieved from `engine.rootObjects()`. This means the QML window is visible and the event loop has ticked before the Dock and menu bar are hidden. For the previous bare-exe form, the same timing applied. For the bundled form launched via Finder or `open`, macOS will render the Dock icon (bouncing) during the gap between app launch and the `setPresentationOptions` call. The Dock icon will then disappear once `hideMenuBarAndDock()` executes.

The generated `Info.plist` contains no `LSUIElement` key. Adding `LSUIElement = 1` to a custom plist template would prevent the Dock icon from appearing at any point, but it also requires explicitly calling `[NSApp activateIgnoringOtherApps:YES]` or the window will never receive focus. The current approach (Regular policy + `setPresentationOptions`) is functional but produces a brief Dock icon flash. Whether that flash is acceptable is a product decision, not a bug.

There is no `MACOSX_BUNDLE_INFO_PLIST` property set in `CMakeLists.txt` and no `*.plist.in` template in the project, so Qt generates the Info.plist from its defaults. If `LSUIElement` is desired, a custom plist template must be created and referenced via `MACOSX_BUNDLE_INFO_PLIST`.

### 6. Direct-exec rule for emulator launch

**Verdict:** OK

**File:** `cpp/src/core/game_session.cpp` line 99, and `cpp/src/ui/app_controller.cpp` line 886

`GameSession::start()` calls `m_process->start(execPath, args)` at line 99 — a direct POSIX `fork`/`exec`, not Launch Services. `AppController::openNativeEmulatorSettings()` calls `QProcess::startDetached(exec, {})` at line 886 — also a direct exec (Qt's `startDetached` is a detached `fork`/`exec` on POSIX systems, not a Launch Services call). The comment at lines 882–885 explicitly documents this rationale. No use of the `open` shell command or `NSWorkspace::openURL` was found anywhere in the source tree. The CLI launch path in `runCli()` at `main.cpp` line 323 also uses `proc.start(execPath, args)` directly. All three launch sites comply with the rule.

### 7. PPSSPP portable mode key (`UserPreferredMemoryStickDirectoryPath`)

**Verdict:** OK

**File:** `cpp/src/adapters/ppsspp_adapter.cpp` lines 360–365

This is a duplicate of item 3 for thoroughness. The call is:

```cpp
QProcess::execute("defaults", {"write", "org.ppsspp.ppsspp",
                               "UserPreferredMemoryStickDirectoryPath", configDir()});
```

`configDir()` (line 21) returns `Paths::emulatorsDir("ppsspp")`, which resolves to `{root}/emulators/ppsspp`. This is the memstick root; PPSSPP will look for `PSP/SYSTEM/ppsspp.ini` inside it. This call is inside `PPSSPPAdapter::ensureConfig()`, which is called by both `GameSession::start()` (via adapter) and `AppController::openNativeEmulatorSettings()` before any emulator launch. The bundle change does not affect this path: `defaults write org.ppsspp.ppsspp` writes to PPSSPP's own plist in `~/Library/Preferences/`, entirely independent of EmuFront's bundle ID.

## Runtime issues — manual smoke test required

The following behaviors cannot be verified by static analysis. Please work through this checklist before merging `chore/bundle-cleanup-and-review` into `main`.

- [ ] **Gatekeeper / quarantine check.** After building, double-click `cpp/build/EmuFront.app` from Finder and confirm it launches without a Gatekeeper prompt or a "damaged app" error. (Locally built bundles should not have a quarantine xattr. If the app is ever distributed as a download, the `xattr -rd com.apple.quarantine` step that `emulator_installer.cpp` line 275 applies to downloaded emulator bundles would also need to be applied to EmuFront's own bundle.)

- [ ] **Dock icon and menu bar.** Launch `EmuFront.app` from Finder (not from the terminal). Confirm that the Dock icon disappears and the macOS menu bar becomes invisible during the session. A brief flash of the Dock icon at startup is expected (see review item 5 above). Confirm the Dock icon does not reappear when switching away to another app and back, and that the menu bar does not reappear on mouse hover.

- [ ] **Cmd+Escape in-game menu.** Launch a game through EmuFront. While the emulator is running, press `Cmd+Escape` and confirm the in-game overlay appears. The Carbon global hotkey must fire even when the emulator process owns the foreground, which requires EmuFront to hold a registered `EventHotKeyRef` with the system. This is the primary test that `RegisterEventHotKey` is still functional in the bundled process.

- [ ] **PPSSPP memory stick path.** Install and launch a PPSSPP game. After quitting, confirm that save data and configuration landed in `{root}/emulators/ppsspp/PSP/` rather than in PPSSPP's default location at `~/Library/Application Support/PPSSPP/`. Optionally run `defaults read org.ppsspp.ppsspp UserPreferredMemoryStickDirectoryPath` in the terminal after launching a PPSSPP game and confirm the value matches `{root}/emulators/ppsspp`.

## Recommendations from the review

In priority order:

1. ✅ **Fix the block comment in `main.cpp` lines 55–56.** Completed in commit `32a6925`. The comment said `"../../../../../../"` (6 levels) but the code used `"/../../../../../"` (5 levels), and the 5-level path is correct. The comment was updated to match the code.

2. ✅ **Fix the misleading CMakeLists comment** at `cpp/CMakeLists.txt` line 147. Completed in commit `32a6925`. The parenthetical `"(e.g. PPSSPP's NSUserDefaults portable path) have a stable key"` was incorrect: EmuFront's bundle ID has no effect on PPSSPP's `NSUserDefaults` domain. The parenthetical was replaced with the accurate rationale (stable `CFBundleIdentifier` exposes `NSApplicationActivationPolicyRegular` behavior during development).

3. ⏸ **Decide on `LSUIElement` and document the decision.** If the brief Dock icon flash at startup is unacceptable for the product's UX (users must never see native OS chrome that breaks the console-appliance illusion), add `LSUIElement = 1` to a custom Info.plist template and add a compensating `[NSApp activateIgnoringOtherApps:YES]` call. If the flash is acceptable (it disappears before the QML window draws its first frame in practice), document this explicitly in `CLAUDE.md` so future contributors do not attempt to "fix" it by switching to `NSApplicationActivationPolicyAccessory`, which would break `setPresentationOptions`. **Decision required from user.**

4. ⏸ **Add a CMake install target or document the installed-bundle gap.** The `"/../Resources/" + name` candidate in `resolveResourceDir` (line 62) will never match because there is no step that copies `manifests/` or `themes/` into `EmuFront.app/Contents/Resources/`. Either add a CMake `install(DIRECTORY ...)` rule that populates `Resources/`, or add a comment noting that the installed-bundle candidate is reserved for a future packaging step. **Can be deferred until you're ready to ship a distributable bundle.**
