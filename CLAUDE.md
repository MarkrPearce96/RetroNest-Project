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
# Fast dev loop — direct exec, bypasses Launch Services (no translocation):
./build/RetroNest.app/Contents/MacOS/RetroNest            # GUI mode
./build/RetroNest.app/Contents/MacOS/RetroNest --cli      # CLI mode
# To exercise Launch Services / activation policy / dock behavior:
open ./build/RetroNest.app
```

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
  bios/               — shared BIOS (all emulators)
  saves/{systemId}/   — savestates/ + memcards/ per system
  data/{emuId}/       — cache, screenshots, cheats, textures (per emulator)
  emulators/{emuId}/  — binaries + portable config/INI files
  roms/{systemId}/    — user ROM files
  config/             — app-level config
  themes/             — runtime themes
  downloaded_media/   — scraped artwork
```

**Key rules:**
- Emulator binaries (`emulators/`) and generated data (`data/`) must stay in separate top-level dirs
- Every emulator runs in **portable mode** — config/INI lives in `{root}/emulators/{emuId}/`, not system defaults
- PathDef/PathBase system: `Bios` → `{root}/bios/`, `Saves` → `{root}/saves/{systemId}/{suffix}`, `Data` → `{root}/data/{emuId}/{suffix}`

**macOS launch rule:** Always launch emulator binaries via direct exec (`QProcess::start(execPath, args)`), NEVER via `open` or Launch Services. Going through Launch Services applies app translocation and sandbox rules to downloaded `.app` bundles, which blocks `rename()` inside the bundle and breaks portable mode. Both `GameSession` and `openNativeEmulatorSettings` use direct exec for this reason.

**Per-emulator portable mechanisms:** Different emulators use different mechanisms to enable portable mode. PCSX2 and DuckStation look for `portable.txt` next to the binary (inside `Contents/MacOS/` on macOS). PPSSPP doesn't read `portable.txt` on macOS — instead, it checks an `NSUserDefaults` preference (`UserPreferredMemoryStickDirectoryPath`, app id `org.ppsspp.ppsspp`) which we set via `defaults write` before launch.

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
- Trigger: Cmd+Escape (Carbon global hotkey) or Select+Circle (SDL combo)
- Pausing via **PauseOnFocusLoss** config — our app takes focus, emulator auto-pauses (no IPC)
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
