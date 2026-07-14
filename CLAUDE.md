# Desktop Emulation Platform — Claude Instructions

## Non-Negotiable UX Rule
Users must NEVER see or interact with native emulator UIs.
There is only ONE application window, ONE unified settings system,
and ONE in-game overlay controlled by this app.

If an emulator cannot be embedded as a clean library,
use offscreen rendering, window parenting, or surface capture
to preserve the illusion. Do not weaken this rule.

## Technology Stack
- **Language:** C++17
- **UI Framework:** Qt6 (QML + Widgets for settings sub-pages)
- **Media/Input:** SDL2
- **Build:** CMake 3.16+
- **Emulators:** five libretro cores loaded in-process — PCSX2, DuckStation, PPSSPP, Dolphin (private forks) + mGBA (stock upstream core)

## Build & Run
```sh
cd cpp
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
cmake --build build
# Default: launch via Launch Services (real activation policy, dock, hotkeys):
open ./build/RetroNest.app
# When you want Qt log output in the terminal, use direct exec instead:
./build/RetroNest.app/Contents/MacOS/RetroNest            # GUI mode
./build/RetroNest.app/Contents/MacOS/RetroNest --cli      # CLI mode
```

The CMake build has a POST_BUILD step that runs `lsregister -f` against the
freshly-linked bundle, so `open ./build/RetroNest.app` always launches the
current build — Launch Services can't cache a stale path for
`com.markpearce.retronest` across rebuilds, moves, or renames.

**Note:** launching RetroNest itself via Launch Services is fine (and the
default above). Emulators are never launched as separate binaries at all —
they are in-process libretro cores loaded with dlopen (the process-era
QProcess launch path was retired 2026-07).

### Universal (Rosetta-capable) builds

For PS2 perf parity with standalone PCSX2 (which only ships x86_64), the
build can produce a universal `arm64;x86_64` RetroNest.app whose x86_64
slice runs PCSX2's recompilers under Rosetta:

```sh
./scripts/setup-x86_64-toolchain.sh   # one-time x86_64 Homebrew + deps
./scripts/build-universal.sh           # full universal build
./scripts/verify-universal.sh          # artifact-verification gate
```

The merged `.app` lands at `cpp/build-universal/RetroNest.app`. Libretro
cores are installed in-place at `~/Documents/RetroNest/emulators/libretro/cores/`.

The user switches between native arm64 and Rosetta x86_64 via Finder →
Get Info → "Open using Rosetta" on the .app. RetroNest does no auto-
relaunching; dyld picks the matching dylib slice automatically.

**Arch policy (real, per core — declared as `core_arch` in each manifest):**
universal everywhere is the goal, but today's truth is per-source:
- **duckstation** (local package.sh) and **ppsspp** (CI release): universal.
- **pcsx2** and **dolphin** GitHub releases: **x86_64-only** (pcsx2's CI
  builds under Rosetta; dolphin mirrors it). Local dolphin deploys via
  tools/deploy.sh ARE universal — only the downloadable zips aren't.
- **mgba**: universal, via a PUBLIC CI release (2026-07-10) — like ppsspp.
  `manifests/mgba.json` carries `github_repo: MarkrPearce96/mgba-libretro`
  (public, no `"private"` flag: mGBA is MPL 2.0, redistribution allowed). That
  repo is a STOCK mirror of upstream mgba-emu/mgba — **zero source patches**;
  it exists only to run `.github/workflows/libretro_release.yml`, which builds
  the universal core (one CMake invocation, `-DBUILD_LIBRETRO=ON`
  `-DBUILD_QT/SDL/GL=OFF -DUSE_LUA=OFF`) and publishes `mgba_libretro.dylib.zip`
  (just the dylib — mGBA ships no runtime assets). Because our CI is universal,
  this doesn't reintroduce the single-arch-clobber problem that got the
  libretro-buildbot download path removed (2026-07 packet 6). `scripts/build-universal.sh`
  still builds it locally for dev; sync the fork with `git merge upstream/master`
  (upstream = mgba-emu) + tag a `v*` release to rebuild.
- **duckstation ships via a PRIVATE, authenticated CI release** (user
  decision 2026-07-09): its manifest carries `github_repo` +
  `"private": true`, and RetroNest downloads the core with a build-time
  embedded fine-grained PAT (in gitignored `cpp/dev_credentials.cmake`) via
  the release-assets API. The release lives ONLY on the private origin repo
  and is fetchable only by the token holder — never public, never
  distributed (CC BY-NC-ND preserved). A tag push builds it x86_64-only via
  the fork's `.github/workflows/libretro_release.yml`; local dev still uses
  its `package.sh` (universal). This replaces the former "no github_repo /
  core never leaves this machine" stance.
Consequence: while the daily driver is the x86_64 app this all works; a
return to native arm64 requires pcsx2/dolphin release pipelines to go
universal first (tracked in the suite review, packet 6/7).
`scripts/verify-universal.sh` checks every core named in manifests/
against its declared core_arch.

### Current run mode: x86_64 (Rosetta) for everything (as of 2026-06-04)

The daily-driver build is the **x86_64** RetroNest app run under Rosetta —
**not** the native arm64 `build/`. Reason: PCSX2 only runs well on x86, and
RetroNest loads libretro cores **in-process**, so one running app is a single
architecture — every core in a session must match the host arch (you can't run
an arm64 DuckStation session and an x86_64 PCSX2 session in the same launch).
Running everything x86 avoids quitting/relaunching to switch emulators.
DuckStation/PPSSPP run fine under Rosetta (Metal/GPU is native; only CPU is
translated).

Build + run the x86 app directly (skip the full `build-universal.sh`, which
also rebuilds PCSX2/mGBA cores):
```sh
arch -x86_64 /usr/local/bin/cmake -S cpp -B cpp/build-x86_64 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2"
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 6
# FIRST deploy of a fresh build dir only — after that a POST_BUILD hook
# re-runs macdeployqt automatically whenever the app binary relinks:
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app \
    -qmldir=cpp/qml -no-codesign -always-overwrite
# pure-x86 binary → auto-runs under Rosetta, no Finder toggle:
cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1
```
- Building the default `all` is fine (shared sources live in the
  `retronest_core`/`retronest_ui` static libraries — tests no longer carry
  hand-copied source lists). Run the suite after building:
  `arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 --output-on-failure`.
  CI (`.github/workflows/tests.yml`) runs the same suite natively on arm64
  for every push/PR to main.
- **macdeployqt is self-maintaining**: once a bundle has been deployed once,
  every relink re-runs it via a POST_BUILD hook (takes a few minutes under
  Rosetta — that's the hook working, not a hang). The old failure mode
  (incremental rebuild → Qt double-load abort at launch) is gone.
- The DuckStation libretro core must be **universal**: build via its
  `package.sh` with **no** `--arm64-only` flag.
- **Dolphin save states are arch-sensitive** — states made under arm64 won't
  load under x86_64 (PCSX2/PPSSPP/DuckStation states do carry over). Make fresh
  Dolphin states after the switch.
- **Future:** when PCSX2 is arm64-native, switch back to the native arm64
  `build/` and this Rosetta detour goes away.
- **x86_64 builds of the core forks** (pcsx2/dolphin build dirs) must use
  `arch -x86_64 /usr/local/bin/cmake` — bare `arch -x86_64 cmake` resolves to
  the arm64 Homebrew cmake and dies with "Bad CPU type" (and if you pipe the
  build output, the failure is silently masked by the pipe's exit status).

## Architecture
- **retronest-libretro contract package** (`vendor/retronest-libretro/`) =
  the SINGLE source of truth for everything shared with the core forks: the
  pinned `libretro.h`, the private `RETRONEST_ENVIRONMENT_*` command registry
  (0x20001–0x20005, next free slot documented in the header), the
  `retronest_game_identity` struct, the optional `retronest_*` export
  prototypes, the shared NSView/emit helpers, and the option style guide.
  NEVER declare a private env command or vendor a libretro.h locally — edit
  the canonical package and run its `sync.sh` (copies into duckstation/pcsx2/
  dolphin; each fork's build checksums its copy via `check-drift.sh`, so
  editing a vendored copy fails that fork's build). Numeric values are frozen
  ABI, pinned by `test_retronest_contract`.
- **Registries** = the two JSON surfaces UI/services read instead of branching on ids:
  - `manifests/*.json` (one per emulator, `manifest_version` 1): identity,
    systems, `backend`/`core_dylib`/`core_arch`, `logo` (qrc path), and the
    `detail_page` capability block (controller pages, has_patches) that
    drives `EmulatorDetailPage`'s row model via `detailActionRows()`. The
    loader warns on unknown keys — extend `kKnownKeys` when adding one.
  - `manifests/systems.json` → `SystemRegistry`: per-system display name,
    ScreenScraper platform ID, RA console ID. The ONLY place system facts
    live; `test_system_registry` pins every value.
  - This applies to UI too (main app AND the setup wizard): pages bind to
    model-surfaced registry fields (e.g. `EmulatorListModel` exposes each
    manifest's `id`/`name`/`systems`/`logo`) — never hardcode a per-emulator
    or per-system `id→value` map in a page. If a page needs a manifest or
    system fact, surface it through the model; don't re-encode it.
- **Settings schema pipeline (libretro)** = the core is the single source of
  truth. `SET_CORE_OPTIONS_V2(_INTL)` is captured at session start and
  persisted to `{root}/emulators/libretro/<core>/declared_options.json`
  (seeded offline by `CoreProber` — dlopen, never `retro_init`, never
  `dlclose`). `LibretroAdapter::settingsSchema()` merges that declared table
  with the adapter's `optionOverlays()` (UI routing + dependsOn gates + rare
  deliberate `defaultOverride`) and `extraSettings()` (frontend-owned rows).
  Adapters carry NO option keys/values/labels of their own; per-core QtTest
  guards assert the merged shape against committed fixtures in
  `cpp/tests/fixtures/declared/`.
- **Adapters** (`adapters/libretro/`) own per-core curation + identity:
  overlays, hub cards, controller types/bindings, serial extraction, resume
  lookup, paths. Base `LibretroAdapter` owns everything generic.
- **Services** (`services/`) = thin orchestration between UI and core
- **Core** (`core/`) = shared utilities: Database, RomScanner, GameSession,
  SdlInputManager, CoreRuntime (the in-process libretro host), OptionsStore,
  SystemRegistry, etc.
- **UI** (`ui/`) = QML pages (`qml/AppUI/`, `qml/SetupWizard/`) + Widgets settings sub-pages (`ui/settings/`)
- **Themes** (`themes/`) = runtime-loaded QML directories with `theme.json` manifests

## Folder Structure (Runtime)
All directories derive from a single user-chosen root:
```
{root}/
  bios/                              — shared BIOS (all emulators)
  emulators/libretro/cores/          — core dylibs + _libs/ + _resources/ (+ .version sidecars)
  emulators/libretro/{coreId}/       — per-core options.json, declared_options.json, frontend.json, controls.ini
  emulators/{emuId}/{systemId}/      — ALL runtime data for that emulator
      savestates/
      memcards/
      screenshots/
      cheats/
      textures/
      cache/
      videos/                        — PCSX2 only
      logs/, patches/, gamesettings/, inputprofiles/  — PCSX2 only
  emulators/ppsspp/psp/              — PPSSPP libretro core's data tree
      SAVEDATA/, PPSSPP_STATE/, SCREENSHOT/, TEXTURES/, Cheats/, GAME/, PLUGINS/
  roms/{systemId}/                   — user ROM files
  config/                            — app-level config
  themes/                            — runtime themes
  downloaded_media/                  — scraped artwork
```

**Key rules:**
- Every emulator runs in **portable mode** — config/INI lives next to its binary under `{root}/emulators/{emuId}/`, not system defaults
- Every emulator keeps **all of its runtime data self-contained** under `{root}/emulators/{emuId}/{systemId}/`. This mirrors PPSSPP's forced `{memstick}/PSP/<subdir>` layout so every emulator follows the same pattern. Deleting an emulator is a single `rm -rf emulators/{emuId}/`.
- BIOS stays shared at `{root}/bios/` because a single ROM file can legitimately back multiple emulators for the same system
- PathDef/PathBase system: `Bios` → `{root}/bios/`, `EmulatorData` → `{root}/emulators/{emuId}/{systemId}/{suffix}`
- Helper: `Paths::emulatorDataDir(emuId, systemId)` returns `{root}/emulators/{emuId}/{systemId}/` — every adapter resolves its managed folders from there

**No emulator binaries are launched.** Every emulator is an in-process libretro core (dlopen via `CoreRuntime`); the process-era launch machinery — direct-exec rules, `openNativeEmulatorSettings`, keystroke-synthesized pause — was retired 2026-07. If a standalone-process emulator is ever reintroduced, recover that machinery deliberately from git history rather than rebuilding it from scratch.

**Portable mode is inherent for libretro cores:** every shipped emulator is a libretro core that receives its data directories from RetroNest via `retro_environment` callbacks (`GET_SAVE_DIRECTORY`, `GET_SYSTEM_DIRECTORY`, private `GET_*_DIR` overrides) — no on-disk portable-mode marker exists or is needed. The `portable.txt` / `NSUserDefaults` mechanisms described in older notes were for standalone process-backend emulators, none of which ship today. The private `GET_MEMCARDS_DIR` / `GET_TEXTURES_DIR` overrides (the Paths settings page → `PathOverridesStore`) are keyed **per-core by the calling core's emu id** (`EnvironmentContext::emuId`, set from `CoreRuntime`) — not a hardcoded id — so each emulator's Paths override applies; the pcsx2, duckstation, and dolphin forks call `GET_MEMCARDS_DIR` (dolphin routes it to `File::SetUserPath(D_GCUSER_IDX)`). Guard: `ConfigService::savePaths` rejects a pathological override dir (home / bare Documents-Desktop / system root) so a huge shared folder can't crash a core on boot.

## App Shell (startup, windows, navigation)

**Startup flow (`main.cpp`):** manifests + `SystemRegistry` load → adapters
registered → first-run check. On first run (no saved root) a **separate QML
engine** runs the `SetupWizard` module in a nested event loop; when the
wizard accepts, that engine is torn down and startup continues. Then
`Paths::setRoot` + directory creation → patches auto-fetch → Database →
services/`AppController`/`GameSession`/`ThemeContext` → the main AppUI
engine loads `AppWindow`. Fatal/wedge exit paths route through a single
`guardedHardExit()` (`main.cpp`); the first ROM scan is deferred off the
pre-first-paint path (app-shell review P8 — the fuller `main()`→AppBootstrap
class extraction was deliberately left for later, so `main()` is still one
long function).

**Window inventory:**
- **Main window** (`AppWindow.qml`) — borderless frameless
  `ApplicationWindow`, manually sized to the screen. Deliberately NOT
  native macOS fullscreen (keeps the menu bar / traffic lights away).
- **`LibretroOverlayPanel`** — transparent QQuickWindow above the main
  window while a HW-render core runs (in-game menu + RA toasts +
  indicator bar + FF chips); its native window is panel-configured
  (isa-swizzled NSPanel) so it can take key input over the game view.
- **Qt Widgets top-levels**: `GenericEmulatorSettingsDialog` (settings
  hub + pages), `HotkeySettingsDialog`, `ControllerMappingPage` (QDialog).
- **SetupWizard window** (first run only, own engine).

**Navigation** — one `StackView` (`mainStack`) in AppWindow:
- `EmptyStatePage` (no games) ↔ theme `systemBrowser` (root) → theme
  `gameList` (via `ThemeContext.navigateToSystemRequested`) →
  `EmulationView`.
- `EmulationView` is pushed on `gameStartingLibretro` **before**
  `retro_load_game` runs — load-bearing ordering: `LibretroMetalItem`
  must realize its NSView so `registerHardwareView()` answers the core's
  `GET_MACOS_NSVIEW` spin-wait. Popped on `gameFinished`.
- Every stack clear+repush goes through `AppWindow.resetStackToRoot()`
  (theme switch, empty-state ↔ theme transitions, initial load). It
  DEFERS while a game runs — clearing would destroy the live
  EmulationView and yank the NSView from the running core — and falls
  back to `EmptyStatePage` if the theme fails to resolve. Never call
  `mainStack.clear()` directly.
- **SettingsOverlay** is a slide-over inside the main window (NOT on the
  stack) with its own sub-page history (`canGoBack`/`goBack`).
- **Back/Esc contract** = `AppWindow.handleBack()` priority order:
  settings overlay visible → cancel-busy / goBack / close; game running
  → `toggleInGameMenu()` (routes HW-render through InGameMenuController,
  in-scene menu for SW-render); `mainStack.depth > 1` →
  `themeContext.navigateBack()`; else open the settings overlay.

**Modal & input gating:** single-sourced in `AppWindow.qml` — three
derived window properties (`anyModalVisible`, `anyEscOwningModalVisible`,
`cursorNeeded`) own the policy; every gate (matcher Binding, Esc/Back/M
Shortcuts, ButtonHints, Start-button Connections) and the one cursor
toggler read them. The canonical policy table lives as the comment block
above those properties. Adding a modal = add its flag to the matching
properties + document its table row; never write a private OR list or
call `setCursorVisible` from a modal. Current modals: GameActionPopup,
ResumeStateDialog, RA login prompt, update confirm/progress, virtual
keyboard.

**QML ↔ Widgets split:** shell, themes, and game surfaces are QML (AppUI
module); dense settings forms are Qt Widgets (the schema-driven
`GenericSettingsPage` pipeline). `AppController` is the single QML-facing
hub (invokables + signals); `ThemeContext` is the only API themes see.
`AppController` is the app's central hub; the app-shell review (2026-07)
extracted the libretro hotkey engine out of it into
`LibretroHotkeyController` (core/libretro/) and moved session identity +
the `libretroRuntime()` guard-chain helper into `GameService`.
`GameSession` now lives in `retronest_core` (and is unit-testable): its
former `LibretroGLItem` dependency was replaced by the abstract
`LibretroRenderSurface` interface (core/), which `LibretroGLItem`
implements — so the pre-stop GL render fence runs through the interface
and `game_session.cpp` carries no Qt Quick. The one remaining core→ui
seam noted in packet 5 is closed. Install/uninstall status reaches QML via
a reactive `AppController::emulatorStatus` `Q_PROPERTY` (backed by
`allEmulatorStatus()`, `emulatorStatusChanged` NOTIFY) — QML binds it
instead of manually re-pulling snapshots (app-shell review P9).

## Theme System
- Fullscreen UI — themes own the entire window, settings via Escape/Start modal overlay
- **ThemeContext** is the only API themes use — never access `app` directly.
  This boundary is **test-enforced**: `test_theme_contract.cpp` runs a
  forbidden-globals lint so theme QML can't bind root context globals
  (`gameModel`, `app`, `inputManager`, …) directly (app-shell review P5/R2).
- Themes provide `systemBrowser` and `gameList` pages in `theme.json`
- **Theme choice persists across launches** (`Paths::saveTheme`/`loadSavedTheme`,
  wired from `theme_manager.cpp`) — no longer auto-selects the first scanned
  theme on restart (app-shell review D1).
- **`theme.json` is validated at scan** — `minAppVersion` enforced, unknown
  keys warned, page files existence-checked; a broken/missing theme fails
  loudly instead of silently black-screening (app-shell review R4/R9).
- Controller support is automatic via unified input system (Keys.on* handlers only)

## Input System
- **Unified architecture:** controller buttons injected as Qt key events — QML keyboard handlers work for both
- D-pad/stick → Arrow keys, A/Cross → Return, B/Circle → **Key_Back** (not Escape), X/Square → Backspace, Y/Triangle → Key_M
- **Key_Back not Key_Escape for B-button** — prevents Qt Shortcut interception
- Start and R2 are **signals** (`navigateStart()`, `navigateShift()`), not key injections
- Binding capture mode: `startCapture()` emits raw `bindingCaptured()` signals instead of key injection

### Settings-page focus navigation (QML)
Controllers inject **arrow keys, not Tab**, so Qt's built-in focus chain
(`KeyNavigation` / `activeFocusOnTab`) doesn't apply — every settings page must
handle arrows itself. To stop each page hand-rolling a private `focusIndex`
(which repeatedly left controls mouse-only / unreachable), reuse the shared QML
components in `qml/AppUI/`, chosen by shape:
- **`GenericListPage`** — a flat vertical list (Themes, Emulator grid, All / Recently-played games).
- **`GenericMultiCardPicker`** — a card + pill 2-D grid with a trailing action button (Resolution, Aspect Ratio).
- **`ConfirmDialog`** — a modal Cancel/Confirm prompt; **Cancel focused by default** (so a controller can't confirm by mashing Enter), `destructive: true` for a red Confirm. Used by uninstall / reset-config / exit. Owns its own key nav; caller sets copy + `onConfirmed`.
- **`FocusRing`** — a registration-based, **row-aware** ring for heterogeneous scrollable pages that fit none of the above (the RetroAchievements dashboard). Controls call `register(this)` / `unregister(this)` (Component.onCompleted/onDestruction), expose an `activate()` method, and bind their highlight to `<ring>.currentItem === this`. The ring groups controls into on-screen rows by top edge (**Up/Down** move between rows, **Left/Right** within a row, preserving column by nearest centre) and owns Enter/Space + scroll-into-view. An unregistered or hidden control simply isn't in the ring — you can't silently orphan one, which was the recurring bug with hand-rolled `focusIndex` pages.

New settings surfaces should adopt one of these rather than hand-roll. Pages
that predate this and have **no** reachability gap are deliberately left as-is
(the Scraper 2-D grid + edit sub-mode, the category list, the RA login screen,
Paths) — don't refactor working nav for its own sake.

## In-Game Menu & Emulator Control
- **Trigger:** Cmd+Shift+Escape (Carbon global hotkey, system-wide), Select+Start (SDL combo, all controllers), or Touchpad press (DualShock 4 / DualSense single-button)
- **Overlay mechanism:** for HW-render cores the menu lives on `LibretroOverlayPanel`, a transparent `QQuickWindow` above the main window whose native window is panel-configured (isa-swizzled `NSPanel`: frameless, status-window-level, non-activating) so it can take key input over the borderless-fullscreen game view; software-render cores use an in-scene QML menu. (The process-era `InGameMenuPanel` that floated over external emulator processes was retired 2026-07.)
- **Pause is a direct call, centralized on the menu-visibility edge.**
  Libretro cores run in-process, so pausing is
  `CoreRuntime::pause()`/`resume()`, driven by the "paused exactly while
  the menu is open, resume before every close or the core's EmuThread
  watchdog kills the session" invariant. It rides two edges:
  `InGameMenuController`'s pause hook (HW overlay path) and the in-scene
  menu's `onVisibleChanged` edge in AppWindow.qml (SW path). Every menu
  trigger routes through the single `toggleInGameMenu()` — the hotkey-fired
  `onLibretroMenuToggleRequested` handler just delegates to it and never
  touches pause/resume itself (the former redundant third site was removed
  2026-07-10). Menu action handlers never call pause/resume themselves.
- **Save & Quit** serializes the core state (`retro_serialize`) to
  `<serial-or-basename>.resume` under the emulator's savestates dir;
  `findResumeFile()` on the adapter locates it at next launch and
  `CoreRuntime` restores it post-`retro_load_game`.
- Resume detection: serial extracted from ROM at import → scan savestates dir for resume file by serial

## RetroAchievements
- **Runtime tracking is RetroNest's own** — `RcheevosRuntime`
  (`core/libretro/rcheevos_runtime.cpp`, rc_client + libretro memory
  descriptors) runs in-process per session: game identification by hash,
  achievement triggers, unlock submission, in-game toasts/indicator.
  `GameSession` feeds it the RA console id from `SystemRegistry`.
- The **Web API** (`RAClient`) additionally powers the display surfaces:
  achievement catalogs, game lists, dashboards.
- Login happens once in RetroNest's Settings → RetroAchievements; the
  token is passed to each session's rc_client. No per-emulator credential
  patching.
- Title matching: 7-tier scoring (exact → deep substring) with normalization + brand prefix stripping
- Console IDs come from `manifests/systems.json` (`ra_console_id`) via `SystemRegistry` — single source of truth

## Controller Mapping (Qt Widgets)
- Per-emulator controller types declared via `controllerTypes()`; the
  detail-page buttons that open each type come from the manifest's
  `detail_page.controller_pages` (dolphin: GCPad1 + Wiimote1).
- **One binding format for every core** — bindings live in each core's
  `controls.ini` as `SDL-0/...` element names (`SDL-0/FaceSouth`,
  `SDL-0/+LeftTrigger`), seeded by `LibretroAdapter::ensureConfig` from the
  adapter's `controllerBindingDefsForType()` defaults. `GameSession` parses
  them through `retroPadSlotFromKey()` into the InputRouter — the core never
  sees host bindings, it sees RetroPad. (The per-emulator native formats and
  `formatBinding()` overrides were standalone-era; libretro retired them.)
- Analog sticks/L3/R3 are fixed-routed by `SdlInputManager` at the RetroPad
  layer, not controls.ini-remappable.
- Profile management: INI files under `{root}/config/controller_profiles/`

## Emulator Config Strategy (libretro)

### Three per-core stores under `{root}/emulators/libretro/<core>/`
- **`options.json`** — the core's libretro option values (`OptionsStore`).
  Written by the settings UI, handed to the core via `RETRO_ENVIRONMENT_GET_VARIABLE`.
  On session start `OptionsStore::load` reconciles against the core's
  declared value lists and silently drops values the core no longer accepts.
- **`declared_options.json`** — the core's full declared option table
  (keys, value sets, labels, defaults), rewritten on every session start and
  seeded offline by `CoreProber` when absent. Never hand-edited; it is the
  schema's source of truth (see Architecture).
- **`frontend.json` / `controls.ini`** — RetroNest-owned frontend settings
  (aspect mode, integer scale) and controller bindings.

### Schema = declared table × overlay — adapters carry no option data
Do NOT add option rows to an adapter by hand. To expose a new core option:
curate it in `optionOverlays()` (routing + optional dependsOn/defaultOverride);
to change values/defaults/wording, change the FORK's CoreOptions source —
it flows through automatically. Uncurated declared options stay valid in
`options.json` but hidden from the UI. Defaults come from the core unless
the overlay deliberately overrides (pinned in each core's guard test).

### Reset Configuration
`LibretroAdapter::resetSettingsToDefaults()` deletes `options.json` +
`frontend.json` and drops caches — wired through `config_service.cpp`.
Bindings/hotkeys reset separately.

### Ini-storage rows (legacy mechanism, kept for future non-option needs)
`SettingDef::Storage::Ini` plus bitmask checkboxes, load/save transforms and
round-trip rules still exist for rows that must write a real INI file; the
full playbook (round-trip bug classes, bitmask semantics) lives in
`docs/new-adapter-checklist.md`. No shipped adapter uses Ini storage today.

## Multi-Disc M3U Support
- Auto-detects disc patterns, generates M3U, suppresses individual discs from game list
- `disc_count` in DB (schema v4), exposed via GameListModel

## Adding a New Emulator
See `docs/new-adapter-checklist.md` for the complete step-by-step guide.

## Repository Rules
Assume version control. Respect folder boundaries.
Do not overwrite unrelated files. Prefer minimal diffs.
Do not add abstractions until they are clearly needed.
