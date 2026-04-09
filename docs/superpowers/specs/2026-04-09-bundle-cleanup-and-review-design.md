# Bundle-Transition Cleanup and Review

**Date:** 2026-04-09
**Status:** Design approved, ready for implementation plan

## Context

EmuFront was just converted from a bare Unix executable to a macOS `.app`
bundle (commit `278efde`). The bundle change touched three files:

- `cpp/CMakeLists.txt` — `MACOSX_BUNDLE TRUE`, bundle identifier
  `com.markpearce.emufront`, bundle version metadata
- `cpp/src/main.cpp` — `resolveResourceDir()` helper for finding `manifests/`
  and `themes/` across bare-exe / dev-bundle / installed-bundle layouts;
  `addImportPath("qrc:/")` on both QML engines so module lookup works when
  the app is launched via Launch Services rather than direct exec
- `CLAUDE.md` — updated run instructions

The bundle conversion introduces several risks that the user wants to flush
out before continuing feature work:

1. **Unrelated drift.** Now is a natural moment for an opportunistic
   simplification pass across the whole app, since the codebase is in a
   stable state after months of feature work.
2. **Bundle-specific regressions.** Several behaviours flagged in `CLAUDE.md`
   could silently break in the bundled form: `MacFullscreen::hideMenuBarAndDock()`,
   the Carbon `Cmd+Escape` global hotkey, the PPSSPP NSUserDefaults portable
   path, and the strict "never launch emulators via `open` / Launch Services"
   rule.

This work is therefore split into two sequential phases.

## Phase 1 — Sequential code-simplifier sweep

**Goal:** Run the `code-simplifier:code-simplifier` agent across all of
`cpp/` (excluding `cpp/tests/` and `cpp/CMakeLists.txt`) in dependency order,
gating each chunk on a clean build, passing tests, and a CLI smoke launch.

### Order

The five chunks are processed strictly in this order. Each must complete
fully (build + tests + smoke + commit) before the next begins.

1. `cpp/src/core/`
2. `cpp/src/adapters/`
3. `cpp/src/services/`
4. `cpp/src/ui/` and `cpp/src/ui/settings/`
5. `cpp/qml/AppUI/` and `cpp/qml/SetupWizard/`

The order is forced by dependency direction: `ui/` depends on
`services/` + `adapters/` + `core/`; `services/` depends on `adapters/` +
`core/`; `adapters/` depends on `core/`. Processing in this order means each
chunk's tests exercise the code below it that has already been simplified
and verified.

### Per-chunk loop

For each subdirectory:

1. Spawn a fresh `code-simplifier:code-simplifier` agent for the chunk. The
   agent is allowed to **read** any file in the project (it needs to
   understand callers and dependencies) but is allowed to **edit** only
   files inside the chunk's subdirectory. Briefed with the constraint list
   below.
2. Run `cmake --build build` from `cpp/` — must succeed. The build must not
   introduce any new compiler warnings beyond the baseline that exists at
   commit `278efde` (i.e. warning count must not increase).
3. Run `ctest --test-dir build --output-on-failure` — every existing test
   must pass
4. Smoke-launch: `./build/EmuFront.app/Contents/MacOS/EmuFront --cli` — must
   print the emulator status block without crashing
5. If all four checks pass, commit the chunk as
   `simplify(<area>): <one-line summary from the agent>`
6. If any check fails, **stop the pipeline**, report the failure, and wait
   for user input. Do not roll forward to the next chunk.

### Constraints handed to every simplifier agent

These are non-negotiable for this codebase. Each agent invocation includes
this list verbatim in its prompt.

- **Do not rename public methods of `EmulatorAdapter`.** Three concrete
  adapters (`pcsx2_adapter`, `duckstation_adapter`, `ppsspp_adapter`) override
  them. Renames cause silent v-table breakage that the build may or may not
  catch depending on `override` annotations.
- **Do not rename QML context properties.** The names `app`, `gameModel`,
  `themeContext`, `themeManager`, `inputManager`, `Theme`, `SettingsTheme`,
  `WizardTheme`, `wizard`, `emulators`, and `installer` are bound by string
  in QML files (and by user themes outside this repo).
- **Do not change INI key names, INI section headers, or stored value
  formats.** Per `CLAUDE.md`, the application's settings UI reads and writes
  the same INI file the emulator reads. Round-tripping is critical and stored
  value formats (e.g. PPSSPP's `GraphicsBackend = 3 (VULKAN)`) must match
  what the emulator writes back.
- **Do not change Qt signal/slot signatures.** Qt's meta-object system
  resolves them by string at runtime. Renames break silently.
- **Do not introduce new abstractions.** Per `CLAUDE.md`'s repository rules:
  do not add abstractions until they are clearly needed. Only collapse
  duplication that already exists or clarify existing code.
- **Do not touch `CMakeLists.txt`.** Excluded from this pass entirely. It
  was just modified for the bundle work and is risky to auto-edit. If a
  follow-up cleanup is wanted, it gets its own dedicated session.
- **Do not touch theme QML files.** The QML pass operates strictly inside
  `cpp/qml/` and never reaches into `themes/` (which lives at the project
  root and is user-overrideable at runtime).

### Verification gate

A chunk is considered "passing" only if:

- Build succeeds (`cmake --build build`)
- Every test passes (`ctest --test-dir build --output-on-failure`)
- CLI smoke launch prints emulator status without error
  (`./build/EmuFront.app/Contents/MacOS/EmuFront --cli`)

If a chunk fails any of these, the pipeline halts and the user is asked
how to proceed. Do not attempt automated rollback — the user will decide
whether to revert, fix forward, or skip.

## Phase 2 — Bundle-transition code review

**Goal:** Run `code-review:code-review` against commit `278efde` (the bundle
change), reviewed against the post-Phase-1 state of the codebase. Verify
that nothing introduced in the bundle transition violates the project's
existing rules or sets up runtime regressions.

### Static review checklist

The reviewer is given this explicit checklist to work through:

1. **`resolveResourceDir` candidate paths.** Are the `..` counts correct
   for every layout it claims to handle (dev bare exe, dev `.app`, installed
   `.app`)? Is the fallback path sensible? What happens if EmuFront is
   nested differently in CI or in a packaged distribution?
2. **`CFBundleIdentifier = com.markpearce.emufront`.** Is this a sensible,
   non-colliding value? Does it conflict with any other bundle ID the app
   talks to (`org.ppsspp.ppsspp`, PCSX2's bundle, DuckStation's bundle)?
3. **PPSSPP `defaults write` calls.** Verify they still target **PPSSPP's**
   bundle ID (`org.ppsspp.ppsspp`), not EmuFront's. This is the most likely
   place to silently regress now that EmuFront has its own bundle identity.
4. **Carbon global hotkey registration.** Anything bundle-specific in the
   `RegisterEventHotKey` flow that would behave differently for a bundled
   process?
5. **`MacFullscreen::hideMenuBarAndDock()`.** Does the activation policy
   and dock-hide trick still work for a bundled app, or does the bundle
   change require explicit `LSUIElement` in `Info.plist` or
   `NSApp.setActivationPolicy(.accessory)`?
6. **Direct-exec rule.** Confirm `GameSession::startEmulator()` and
   `AppController::openNativeEmulatorSettings()` still call
   `QProcess::start(execPath, args)` directly and never go through `open`
   or Launch Services. `CLAUDE.md` flags this as a hard rule.
7. **PPSSPP portable mode key.** Verify `UserPreferredMemoryStickDirectoryPath`
   is still being written against PPSSPP's bundle ID before launch and that
   the bundle change did not accidentally redirect it.

### Runtime issues called out, not covered

Code review cannot verify these. The review report ends with an explicit
manual checklist for the user:

- Double-click `EmuFront.app` from Finder once and confirm it launches
  (Gatekeeper / quarantine xattr check)
- Confirm the Dock icon does not appear and the macOS menu bar stays hidden
- Confirm `Cmd+Escape` still triggers the in-game menu when an emulator is
  running
- Confirm a PPSSPP launch still resolves the portable memory stick path to
  the EmuFront-managed location

## Out of scope

- Cleanup of `CMakeLists.txt` itself
- Cleanup of `cpp/tests/`
- Cleanup of theme QML files (live in `themes/`, not `cpp/`)
- Cleanup of `manifests/` JSON files
- Any new feature work
- Notarization, codesigning, or distribution concerns
- Adding `LSUIElement` or other `Info.plist` keys (only do this if Phase 2
  finds we need them)

## Success criteria

- All five Phase 1 chunks committed, build green, tests green at every step
- Phase 2 review report written to
  `docs/superpowers/specs/2026-04-09-bundle-cleanup-review-report.md` and
  committed
- The review report ends with a manual smoke-test checklist covering the
  four runtime issues this review cannot verify (Finder launch, dock/menu
  bar, Cmd+Escape, PPSSPP portable path)
