# Dolphin Adapter — Design

**Date:** 2026-05-03
**Author:** Mark Pearce (with Claude)
**Status:** Approved for planning

## Goal

Add Dolphin (GameCube + Wii) to RetroNest as a first-class emulator. The user installs it via the in-app emulator install grid, scans GameCube and Wii ROMs, launches games, and the emulator runs in our window with our overlay — no Dolphin native UI ever shown to the user during gameplay.

This spec is intentionally an **MVP**, scoped against a deliberate set of decisions (see "Decisions baked in" at the bottom). A separate session will run the settings audit pass against the native source.

## Acceptance criteria (from the user)

- Clean build: `cd cpp && cmake --build build`.
- All existing tests pass.
- Dolphin appears in the in-app emulator install grid and installs via the GitHub release flow.
- Manual smoke (user runs): install via in-app flow → launch a game → verify controller input works → change a setting → reopen settings → confirm persistence → restart the app → confirm persistence.

## Investigation summary (verified against native source)

Sources cited from `/Users/mark/Documents/Projects/RetroNest-Project/references/dolphin-master/`.

| Question | Answer |
|---|---|
| Portable mode | Honors `portable.txt` next to the executable → uses `User/` dir alongside the binary. Also accepts `-u <path>` CLI flag. (`UICommon.cpp:317-319, 359, 417-429`) |
| macOS bundle | `Dolphin.app/Contents/MacOS/DolphinQt` (`Source/Core/DolphinQt/Info.plist.in:33-34, 38`) |
| Config files | Six relevant files under `User/Config/`: `Dolphin.ini`, `GFX.ini`, `GCPadNew.ini`, `WiimoteNew.ini`, `Hotkeys.ini`, `RetroAchievements.ini` (`Common/CommonPaths.h:104-112`) |
| Hotkeys | Live in `Hotkeys.ini` `[Hotkeys]` section (`HotkeyManager.cpp:211`) |
| Pause-on-focus-loss | `[Interface] PauseOnFocusLost = True` (`Core/Config/MainSettings.cpp:447`) |
| Save-on-shutdown | None — no native equivalent. `supportsSaveOnExit()` will return `false`. |
| Binding format | Backtick-delimited expression language: `` `SDL/0/Wireless Controller:Button A` `` — supports `\|` (OR), `&` (AND), `!` (NOT). Empty expressions return zero, no crash. (`InputCommon/ControlReference/ExpressionParser.h:13-48, 66-73`) |
| SDL gamepad device names | `SDL/{device_id}/{display name}`. Buttons: `Button S/E/W/N`, `Pad N/S/E/W`, `Shoulder L/R`, `Thumb L/R`, `Start`, `Back`. Axes: `Left X/Y`, `Right X/Y`, `Trigger L/R`. Polarity suffixes `+`/`-` for axes. (`InputCommon/ControllerInterface/SDL/SDLGamepad.h:382-429`) |
| CLI flags | `-e <file>` execute, `-b` batch, `-s <state>` load state, `-u <path>` user dir override, `-C` config override (`Source/Core/UICommon/CommandLineParse.cpp:85-116`) |
| Native RA | Yes — `RetroAchievements.ini`, conditionally compiled via `USE_RETRO_ACHIEVEMENTS`. (`AchievementManager.cpp:4, 64-99`) |
| ROM extensions | `.iso, .gcm, .gcz, .ciso, .wbfs, .rvz, .wad, .wia, .nkit, .m3u, .dol, .elf, .tgc` (`Source/Core/Core/Boot/Boot.cpp:64, 212`) |
| BIOS | GameCube IPL (`IPL.bin` per region) — required for the boot animation only. Skipped via `[Core] SkipIPL = True`. Wii NAND emulated internally. |
| GameID extraction | `DiscIO::Volume::GetGameID()` — Dolphin-internal, not used by us in v1. |

## Decisions baked in

These were explicitly settled with the user during brainstorming:

1. **Single manifest covering both systems** (`"systems": ["gc", "wii"]`). Matches Dolphin's reality — one binary handles both. RetroNest's existing `Paths::emulatorDataDir(emuId, systemId)` already supports per-system data layout under one emulator install.
2. **One Dolphin settings page**, not separate GC and Wii pages. Dolphin's settings (graphics, audio, interface) are global across both systems in the emulator itself; presenting two pages would be misleading because changing one would silently change the other. Per-game divergence is available later via Dolphin's `User/GameSettings/{GAMEID}.ini` mechanism (out of scope for v1).
3. **Default-only controllers, no in-app remap UI for v1.** `controllerTypes()` returns empty so Dolphin doesn't appear in the controller mapping page. Sensible defaults baked into `GCPadNew.ini` and `WiimoteNew.ini` at install time. Users with custom needs use Dolphin's native UI. A proper in-app remap pass (with Dolphin's expression-language `formatBinding()`) is a deliberate follow-up.
4. **In-game menu = pause-on-focus-loss only.** `supportsSaveOnExit() = false` (like PPSSPP). No serial extraction, no resume detection. Cmd+Esc overlay still works for "exit", "settings", "RA".
5. **Settings audit is a separate session**, per the user's explicit instruction. This adapter is shipped as a working MVP with a settings schema we believe is correct from native-source inspection — but not yet line-by-line audited against the source.

## Architecture

### File creation/modification map

**Created:**
- `manifests/dolphin.json`
- `cpp/src/adapters/dolphin_adapter.h`
- `cpp/src/adapters/dolphin_adapter.cpp`
- `qml/AppUI/images/dolphin-logo.png` (user supplies or Claude sources placeholder)
- (Optional) `themes/modern/assets/artwork/gc.webp`, `wii.webp` and matching logos/gamepage-logos

**Modified:**
- `cpp/src/adapters/adapter_registry.cpp` — register `DolphinAdapter`
- `cpp/CMakeLists.txt` — add adapter to SOURCES + HEADERS
- `cpp/src/adapters/emulator_adapter.h` — add optional `QString file` field to `ResolutionOptions`, `AspectRatioOptions`, and `IniPatch`
- `cpp/src/services/config_service.cpp` — honor `file` field in `quickResolutionOptions/currentResolution/applyQuickResolution` and the equivalent aspect-ratio paths
- `cpp/src/services/scraper.cpp` — `systemToScreenScraperId()` add `gc` (13), `wii` (16)
- `cpp/src/ui/theme_context.cpp` — `systemDisplayNames` add `gc` ("GameCube"), `wii` ("Wii")
- `cpp/src/services/ra_client.cpp` — `consoleIdMapping()` add `{"gc", 16}`, `{"wii", 19}`
- `qml/AppUI/EmulatorLogos.js` — add Dolphin logo path
- `qml/SetupWizard/EmulatorCard.qml` — add Dolphin to local `logoForEmu()` function
- `qml/AppUI/CMakeLists.txt` and `qml/SetupWizard/CMakeLists.txt` — add Dolphin logo to RESOURCES

### Manifest

```json
{
  "id": "dolphin",
  "name": "Dolphin",
  "description": "Dolphin is a GameCube and Wii emulator. Play GC/Wii games in HD with save states, controller support, and per-game configurations.",
  "systems": ["gc", "wii"],
  "github_repo": "dolphin-emu/dolphin",
  "executable": "DolphinQt",
  "install_folder": "dolphin",
  "rom_extensions": ["iso", "gcm", "gcz", "ciso", "wbfs", "rvz", "wad", "wia", "nkit", "m3u"],
  "launch_args": ["-b", "-e", "{rom_path}"]
}
```

`-b` = batch (no GUI lifecycle quirks), `-e` = execute the disc. `--fullscreen` is implicit because we set `[Display] Fullscreen = True` in `Dolphin.ini`.

### Install layout

```
emulators/dolphin/
  Dolphin.app/
    Contents/MacOS/
      DolphinQt
      portable.txt                           ← marker, written by ensureConfig()
      User/
        Config/
          Dolphin.ini                        ← main config; what configFilePath() returns
          GFX.ini                            ← graphics; resolution + aspect live here
          GCPadNew.ini                       ← GC controller defaults baked in
          WiimoteNew.ini                     ← Wii Remote defaults baked in
          Hotkeys.ini                        ← cleared of conflicting hotkeys
          RetroAchievements.ini              ← RA settings, patched via patchRetroAchievements()
        GC/, Wii/, GameSettings/, ScreenShots/, ...
  gc/                                         ← per-system data per RetroNest convention
    savestates/, screenshots/, cheats/, textures/, cache/, ...
  wii/
    savestates/, screenshots/, cheats/, textures/, cache/, ...
```

`portable.txt` lives next to `DolphinQt` inside `Contents/MacOS/`, matching how PCSX2/DuckStation work today. Per-system data dirs (`emulators/dolphin/gc/`, `emulators/dolphin/wii/`) are wired via `pathsDefs()` and pointed at from `Dolphin.ini` `[General]` keys.

### `ensureConfig()` — multi-file strategy

Called both before launch and when the user opens native settings. Idempotent.

1. Ensure `portable.txt` exists next to `DolphinQt` (`ensurePortableMarker()` helper).
2. Ensure `User/Config/` exists.
3. Patch `Dolphin.ini`:
   - `[Interface] PauseOnFocusLost = True` (in-game menu pause)
   - `[Interface] ConfirmStop = False` (no exit confirmation dialog)
   - `[Interface] HideCursor = True`
   - `[Interface] LanguageCode = ` (empty → system default)
   - `[Display] Fullscreen = True`
   - `[Display] RenderToMain = True` (avoid spawning a separate render window)
   - `[Core] SkipIPL = True` (boot games without requiring IPL bootrom)
   - `[Core] EnableCheats = False`
   - `[General] ISOPath0 = {emulators/dolphin/gc}`, `ISOPath1 = {emulators/dolphin/wii}`, `ISOPaths = 2`, `RecursiveISOPaths = True`
4. Patch `GFX.ini`:
   - `[Hardware] VSync = True`
   - `[Settings] AspectRatio = 0` (Auto; wizard overrides)
   - `[Settings] InternalResolution = 1` (1×; wizard overrides)
5. Write defaults for `GCPadNew.ini` `[GCPad1]` only if the file does not exist (do not overwrite user customizations on subsequent launches).
6. Write defaults for `WiimoteNew.ini` `[Wiimote1]` under the same "create-only" rule.
7. Patch `Hotkeys.ini` to clear keys that would conflict with our overlay: `[Hotkeys] General/Toggle Pause = `, `General/Open = `, `Save State/Save State Slot 1 = ` (any that compete with Cmd+Esc / Save on Exit). The expression parser tolerates empty values, so blanking is safe.

All paths derive from a single `userConfigDir()` helper inside the adapter, computed from `portableDir()`.

### Framework extension (the only one)

Add an optional `QString file` to three structs in `cpp/src/adapters/emulator_adapter.h`:

```cpp
struct ResolutionOptions {
    QString section;
    QString key;
    QVector<ResolutionOption> options;
    QString defaultValue;
    QString file;   // optional; empty = use configFilePath()
};

struct AspectRatioOptions {
    QVector<AspectRatioOption> options;
    QString defaultLabel;
    QString file;   // optional; empty = use configFilePath()
};

struct IniPatch {
    QString section;
    QString key;
    QString value;
    QString file;   // optional; empty = use configFilePath()
};
```

Then update `ConfigService` (`cpp/src/services/config_service.cpp`):
- `currentResolution()`: if `opts.file` non-empty, read from there instead of `configFilePath()`.
- `applyQuickResolution()`: same on the write side.
- `currentAspectRatio()` / `applyQuickAspectRatio()`: per-patch `file` field; per-`IniPatch` if present, else fall back to opts-level (keeping it simple).

PCSX2/DuckStation/PPSSPP leave `file` empty → unchanged behavior. Dolphin sets it to the absolute path of `User/Config/GFX.ini`.

### `settingsSchema()` (in-app settings page) — v1 scope

Only settings that live in `Dolphin.ini` are exposed (single `configFilePath()` constraint). Categories:

- **Interface** — `PauseOnFocusLost`, `ConfirmStop`, `HideCursor`, `LanguageCode`, `ThemeName`
- **Audio** (`[DSP]`) — `Backend` (Cubeb / OpenAL / Null), `Volume`, `EnableJIT`
- **Core** — `SkipIPL`, `EnableCheats`, `CPUCore` (Interpreter / JIT64 / JITARM64), `OverclockEnable`, `Overclock`

All option strings will use the exact form Dolphin writes back (`True`/`False` capitalization, named enum values where applicable). The author will sanity-check each option during implementation, but the comprehensive line-by-line audit is the separate session.

`GFX.ini` settings (graphics page) are deliberately not exposed in v1. Users access them via "Open Native Settings". Resolution and aspect ratio remain tweakable through the Quick Settings overlay (which uses the framework `file` field above to write to `GFX.ini`).

### Controllers

- `controllerTypes()` returns an empty list → Dolphin does not appear in the in-app controller mapping page.
- `controllerBindingDefs()` likewise empty.
- `controllerBindingsConfigFilePath()` and `controllerBindingsSection()` not overridden (irrelevant since the UI doesn't expose them).
- Defaults baked into `GCPadNew.ini` and `WiimoteNew.ini`:
  - **GCPad1** (Standard Controller, SDL gamepad 0):
    - `Buttons/A = \`SDL/0/<gamepad>:Button S\``
    - `Buttons/B = \`SDL/0/<gamepad>:Button E\``
    - `Buttons/X = \`SDL/0/<gamepad>:Button W\``
    - `Buttons/Y = \`SDL/0/<gamepad>:Button N\``
    - `Buttons/Z = \`SDL/0/<gamepad>:Shoulder R\``
    - `Buttons/Start = \`SDL/0/<gamepad>:Start\``
    - `D-Pad/Up = \`SDL/0/<gamepad>:Pad N\`` (and S/E/W)
    - `Main Stick/Up = \`SDL/0/<gamepad>:Left Y-\`` (etc.)
    - `C-Stick/Up = \`SDL/0/<gamepad>:Right Y-\`` (etc.)
    - `Triggers/L = \`SDL/0/<gamepad>:Trigger L\``
    - `Triggers/R = \`SDL/0/<gamepad>:Trigger R\``
  - **Wiimote1** (sideways Wiimote, no extension): A/B/1/2/+/-/Home plus D-Pad mapped to a SDL gamepad. The exact mapping will be derived during implementation by inspecting Dolphin's auto-config output.

The `<gamepad>` placeholder will be the literal display name used by SDL (commonly `Wireless Controller` for PS-style pads). The defaults file we write will use a generic name that Dolphin's expression parser resolves via the device-name pattern; if the user has a non-standard device, they edit through Dolphin's native UI.

### In-game menu

- `[Interface] PauseOnFocusLost = True` set in `ensureConfig()` → automatic pause when our overlay takes focus.
- `supportsSaveOnExit()` → `false`. "Exit & Save State" menu item hidden for Dolphin games (matches PPSSPP behavior).
- No `extractSerial()` / `findResumeFile()` overrides → resume detection silently no-ops.
- Conflicting native pause/load-state hotkeys cleared in `Hotkeys.ini`.

### RetroAchievements

- `supportsRetroAchievements()` → `true`.
- Override `patchRetroAchievements()` to write to `User/Config/RetroAchievements.ini` (different from `configFilePath()`). Patches `[Achievements]` keys: `Enabled`, `HardcoreEnabled`, plus notification/sound keys to be confirmed during implementation. Verified against `Source/Core/Core/Config/AchievementSettings.cpp:12,16`. Dolphin's bool format is `True`/`False` (capitalized — `Common/StringUtil.cpp:289-292`).
- Console IDs: GameCube = 16, Wii = 19 (RetroAchievements canonical IDs).

### Asset matching

```cpp
QVector<AssetMatchRule> assetMatchRules() const override {
#if defined(Q_OS_MACOS)
    return {
        {{"macos", "universal"}, ".dmg"},
        {{"macos"}, ".dmg"},
        {{"mac"}, ".dmg"},
    };
#elif defined(Q_OS_WIN)
    return {{{"windows", "x64"}, ".zip"}};
#else
    return {{{"linux"}, ".AppImage"}, {{"linux"}, ".tar.xz"}};
#endif
}
```

`emulator_installer.cpp` already handles `.dmg` extraction.

### Resolution and aspect ratio options (wizard)

`resolutionOptions()`:
```
section = "Settings", key = "InternalResolution", file = "<absolute>/User/Config/GFX.ini"
options:
  "Native (1x)"     -> "1"
  "2x (~720p)"      -> "2"
  "3x (~1080p)"     -> "3"
  "4x (~1440p)"     -> "4"
  "5x (~1800p)"     -> "5"
  "6x (~4K)"        -> "6"
defaultValue = "1"
```

`aspectRatioOptions()`:
```
options:
  "Auto":      [{section=Settings, key=AspectRatio, value="0", file=GFX.ini}]
  "Force 16:9":[{section=Settings, key=AspectRatio, value="1", file=GFX.ini}]
  "Force 4:3": [{section=Settings, key=AspectRatio, value="2", file=GFX.ini}]
  "Stretch":   [{section=Settings, key=AspectRatio, value="3", file=GFX.ini}]
defaultLabel = "Auto"
```

Values verified against Dolphin's `AspectMode` enum during implementation.

### System mappings, logo, theme assets

- `theme_context.cpp` `systemDisplayNames`: `{"gc", "GameCube"}`, `{"wii", "Wii"}`.
- `scraper.cpp` `systemToScreenScraperId()`: `{"gc", 13}`, `{"wii", 16}`.
- Logo: `qml/AppUI/images/dolphin-logo.png`. User supplies or Claude sources a placeholder.
- Theme assets (optional): `themes/modern/assets/artwork/{gc,wii}.webp`, plus matching logos and gamepage-logos. Marked optional in the plan — can ship without if user prefers to drop them in later.
- Wizard previews (optional): aspect ratio + resolution preview images. Defer.

## Risks and explicit non-goals

**Non-goals for v1** (call-out list so they're not a surprise):
- No in-app controller remap UI for Dolphin.
- No Graphics settings page in our UI.
- No save-on-exit, no game-resume from save state.
- No serial extraction (no resume scanning).
- No per-game `GameSettings/{GAMEID}.ini` UI.
- No settings audit pass — that's a separate session.

**Risks:**
- The default `GCPadNew.ini` / `WiimoteNew.ini` we bake in must use device-name patterns that match what Dolphin's SDL backend reports. If a user has an unusual gamepad that doesn't match the `<gamepad>` literal we write, controllers won't work out of the box. Mitigation: implementation will verify against Dolphin's auto-config output for a real PS/Xbox gamepad before declaring done. A follow-up could call Dolphin's `--config` CLI to write per-device defaults at install time.
- `[Display] RenderToMain = True` may interact oddly with our window-embedding strategy on macOS. If embedding fails, we revert to the non-embedded fullscreen path used by other adapters and document the trade-off.
- The `file` field framework tweak is small but lives in code paths used by every adapter. Plan must include a sanity smoke of PCSX2/DuckStation/PPSSPP wizard/quick-settings after the change.

## Verification plan

- `cd cpp && cmake --build build` clean.
- `ctest` (or whatever the project's test runner is — TBD during implementation when tests are wired up).
- Manual smoke handed back to the user as listed in Acceptance.

## Open questions deferred to implementation

These are details the spec deliberately leaves to discovery during code-writing rather than locking now:

- Exact `[DSP]` audio backend name strings Dolphin writes (`Cubeb` / `OpenAL` / `Null` — case-sensitive).
- Exact `[Display]` keys for fullscreen monitor selection (deferred until user reports a multi-monitor issue).
- Wii Remote default mapping details (mapped at implementation against Dolphin's auto-config output).
- Whether `Hotkeys.ini` `General/Open Settings` needs to be cleared too (depends on whether it intercepts Cmd+Esc on macOS).
- Exact RetroAchievements key names beyond `Enabled` and `HardcoreEnabled` (notifications, sound effects — TBD by reading `Source/Core/Core/Config/AchievementSettings.cpp` for the full `Info<bool>` list).
