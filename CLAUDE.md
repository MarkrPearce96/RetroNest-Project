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
- **Emulators:** PCSX2, DuckStation, PPSSPP (more to follow)

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

**Note on the "never use Launch Services" rule:** that rule applies to how
**RetroNest launches emulator binaries** (`GameSession`,
`openNativeEmulatorSettings` — always direct `QProcess::start(execPath, args)`).
It does **not** apply to how you launch RetroNest itself. Emulator portable
mode is enforced by RetroNest at runtime (writing `portable.txt` / setting
the PPSSPP `NSUserDefaults` key) before spawning each emulator, regardless
of how RetroNest itself was started.

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

**Policy:** all libretro cores RetroNest ships are universal binaries.
This eliminates host/core arch-mismatch failure modes. New cores
(future DuckStation libretro, PPSSPP libretro) follow the same rule.

## Architecture
- **Manifests** (`manifests/*.json`) = metadata (id, name, systems, github_repo, executable, launch_args)
- **Adapters** (`adapters/`) own all emulator behavior (config patching, platform paths, launch logic)
- **Adapter base class** provides shared helpers: `suppressSetupWizard()`, `patchIniKeys()`, `matchAsset()`
- **Services** (`services/`) = thin orchestration between UI and core
- **Core** (`core/`) = shared utilities: Database, IniFile, RomScanner, GameSession, SdlInputManager, etc.
- **UI** (`ui/`) = QML pages (`qml/AppUI/`, `qml/SetupWizard/`) + Widgets settings sub-pages (`ui/settings/`)
- **Themes** (`themes/`) = runtime-loaded QML directories with `theme.json` manifests

## Folder Structure (Runtime)
All directories derive from a single user-chosen root:
```
{root}/
  bios/                              — shared BIOS (all emulators)
  emulators/{emuId}/                 — binaries + portable config/INI files
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

**macOS launch rule:** Always launch emulator binaries via direct exec (`QProcess::start(execPath, args)`), NEVER via `open` or Launch Services. Going through Launch Services applies app translocation and sandbox rules to downloaded `.app` bundles, which blocks `rename()` inside the bundle and breaks portable mode. Both `GameSession` and `openNativeEmulatorSettings` use direct exec for this reason.

**Per-emulator portable mechanisms:** Different standalone emulators use different mechanisms to enable portable mode. DuckStation looks for `portable.txt` next to the binary (inside `Contents/MacOS/` on macOS). Libretro cores (mgba, PCSX2, PPSSPP) receive their data directories from RetroNest via `retro_environment` callbacks, so they need no on-disk portable-mode marker.

## Theme System
- Fullscreen UI — themes own the entire window, settings via Escape/Start modal overlay
- **ThemeContext** is the only API themes use — never access `app` directly
- Themes provide `systemBrowser` and `gameList` pages in `theme.json`
- Controller support is automatic via unified input system (Keys.on* handlers only)

## Input System
- **Unified architecture:** controller buttons injected as Qt key events — QML keyboard handlers work for both
- D-pad/stick → Arrow keys, A/Cross → Return, B/Circle → **Key_Back** (not Escape), X/Square → Backspace, Y/Triangle → Key_M
- **Key_Back not Key_Escape for B-button** — prevents Qt Shortcut interception
- Start and R2 are **signals** (`navigateStart()`, `navigateShift()`), not key injections
- Binding capture mode: `startCapture()` emits raw `bindingCaptured()` signals instead of key injection

## In-Game Menu & Emulator Control
- **Trigger:** Cmd+Shift+Escape (Carbon global hotkey, system-wide), Select+Start (SDL combo, all controllers), or Touchpad press (DualShock 4 / DualSense single-button)
- **Overlay mechanism:** the menu is a separate `QQuickWindow` (`InGameMenuPanel`) backed by an isa-swizzled `NSPanel` (frameless, status-window-level, fullscreen-auxiliary, non-activating-panel) so it floats over a fullscreened external emulator without our app activating
- **Pause is per-adapter.** Each adapter overrides `pauseHotkeyVirtualKeyCode()` to return the Carbon `kVK_*` for its TogglePause hotkey; the corresponding INI key is bound to that hotkey in `createDefaultConfig` + `patchExistingConfig`. AppController synthesizes the keystroke via `CGEventPostToPid` for clean native pause (no SIGSTOP audio click). Adapters that don't expose a pause-only hotkey return 0 → AppController falls back to SIGSTOP/SIGCONT (audio click but works universally).
- **`CGEventPostToPid` requires Accessibility permission** on macOS Sonoma+ (System Settings → Privacy & Security → Accessibility → enable RetroNest.app). Without it, synthesized keystrokes are silently dropped and the emulator never pauses. macOS prompts the first time the menu opens.
- **Pause hotkey is hidden from user-facing settings** — none of the adapters list TogglePause / `General/Toggle Pause` in their `hotkeyBindingDefs()`, so the user can't accidentally rebind the key our synthesis depends on.
- **Close-side input bleed avoided** by polling SDL state until A/B/X/Y are released before sending the unpause keystroke (e.g. Cross-to-Resume → no jump in the resumed game).
- Save-on-exit via emulator's **SaveStateOnShutdown** config — SIGTERM triggers save, SIGKILL skips
- Resume detection: serial extracted from ROM at import → scan savestates dir for resume file by serial
- Serial format conversion per emulator (e.g. `SCUS_949.00` → `SCUS-94900`)

## RetroAchievements
- Web API only — emulators handle achievement tracking at runtime; we only display data
- No credential patching — emulators handle their own RA login (DuckStation encrypts, PCSX2 was unreliable)
- Title matching: 7-tier scoring (exact → deep substring) with normalization + brand prefix stripping
- Console IDs in `ra_client.cpp` `consoleIdMapping()` — single source of truth

## Controller Mapping (Qt Widgets)
- Per-emulator controller types declared via `controllerTypes()`
- **PCSX2 binding format:** `SDL-0/FaceSouth` (generic names, no `+` for buttons, `+/-` for axes only)
- **DuckStation binding format:** `SDL-0/A` (SDL names, bare axis names for full axes)
- **PPSSPP binding format:** `{deviceId}-{keyCode}` (numeric, e.g. `10-19` for d-pad up). Often needs dual bindings (`10-96,10-189`) — GameController NKCODE + raw joystick button fallback.
- Each emulator's `formatBinding()` override handles the differences
- Profile management: INI files under `{root}/config/controller_profiles/`
- **Bindings in a separate file/section?** Override `controllerBindingsConfigFilePath()` and `controllerBindingsSection(port)` so the UI writes directly to where the emulator reads (PPSSPP uses `controls.ini` `[ControlMapping]`, not the main config). This makes binding changes instant in both UIs.

## Emulator Config Strategy (INI Patching)

### Source of truth = our app, via the shared config file
Our settings UI reads/writes the same file the emulator reads, so changes are instant in both UIs and there's no merge logic. `configFilePath()` should point at the file the emulator actually reads (not a separate managed copy). `ensureConfig()` runs both before launching a game AND when the user opens the native settings UI, so the file always has wizard-suppressed and embedding-critical keys before the emulator touches it.

### createDefaultConfig — minimal keys only
Only write keys **required for embedding**: wizard suppression, fullscreen, managed folder paths,
input source headers. Do NOT write graphics, audio, speed, or controller type.

### patchExistingConfig — defensive
Same critical keys, check existence before overwrite, force-inject truly required keys, warn on missing.

### settingsSchema — validated at load
Missing INI keys → disabled/greyed UI widgets + qWarning. Stale keys skipped on save.

### Bitmask checkboxes — when an emulator packs flags into a single int key
Some emulators store multiple boolean flags as bits of a single int-valued INI key (e.g. PPSSPP's `iShowStatusFlags` packs FPS/speed/CPU/GPU/debug toggles into one int). For these, set `SettingDef::bitmask` to the bit value the checkbox owns (e.g. `1`, `2`, `4`, `8`, …). The widget renders as a normal `Bool` checkbox; on load it reads the int and tests the bit, on save it re-reads the current int from disk, sets/clears the bit, and writes the full int back. Multiple bitmask checkboxes that share the same `section`/`key` merge correctly without caching. Default `bitmask = 0` means "normal Bool" — every existing setting in PCSX2 and DuckStation uses this path because both emulators expose flags as individual bool keys, not packed ints. Only reach for `bitmask` when the native emulator's source actually packs the flags; do not introduce it as a refactor.

### Stored value format must round-trip
If the emulator rewrites a setting in a different format than ours (e.g. PPSSPP writes `GraphicsBackend = 3 (VULKAN)` via a `ConfigTranslator`, not just `3`), our combo values **must** match the format the emulator writes — otherwise the UI can't match the stored value on the next read and falls back to the default. Always check what the emulator writes after a save before defining combo options.

## Multi-Disc M3U Support
- Auto-detects disc patterns, generates M3U, suppresses individual discs from game list
- `disc_count` in DB (schema v4), exposed via GameListModel

## Adding a New Emulator
See `docs/new-adapter-checklist.md` for the complete step-by-step guide.

## Repository Rules
Assume version control. Respect folder boundaries.
Do not overwrite unrelated files. Prefer minimal diffs.
Do not add abstractions until they are clearly needed.
