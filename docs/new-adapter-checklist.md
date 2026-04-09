# Adding a New Emulator Adapter

## Files to create/modify (in order)

### 1. Manifest
Create `manifests/{emuId}.json`:
```json
{
  "id": "{emuId}",
  "name": "Display Name",
  "description": "One-line description",
  "systems": ["{systemId}"],
  "github_repo": "owner/repo",
  "executable": "{binary-name}",
  "install_folder": "{emuId}",
  "rom_extensions": ["iso", "bin", "chd"],
  "launch_args": ["-fullscreen", "{rom_path}"]
}
```

### 2. Adapter header
Create `cpp/src/adapters/{emuId}_adapter.h`:
- Inherit from `EmulatorAdapter`
- Override required methods (see list below)
- Declare `portableDir()`, `createDefaultConfig()`, `patchExistingConfig()` as private

### 3. Adapter implementation
Create `cpp/src/adapters/{emuId}_adapter.cpp`:
- **`ensureConfig()`** — check if config exists, call create or patch
- **`createDefaultConfig()`** — write **only** embedding-critical keys:
  - Use `suppressSetupWizard()` helper (inherited) with emulator's wizard section name
  - Use `patchIniKeys()` helper (inherited) with `QVector<IniKeyPatch>` for folder paths
  - Write fullscreen keys, input source section headers, controller section header (no Type=)
- **`patchExistingConfig()`** — same helpers, same keys, defensive patching
- **`resolveExecutable()`** — handle macOS .app bundle, Windows .exe, Linux paths
- **`configFilePath()`** — return path to the emulator's main INI/config file
- **`pathsDefs()`** — declare paths with `PathBase::Bios`/`Saves`/`Data`
- **`biosFiles()`** — list required/optional BIOS files with MD5s
- **`settingsSchema()`** — define INI keys the settings UI exposes
- **`resolutionOptions()`** / **`aspectRatioOptions()`** — wizard options
- **`controllerTypes()`** — available controller types (e.g. DualShock2, Guitar, Jogcon, NeGcon, Popn)
- **`controllerBindingDefs()`** — button/axis binding definitions (default/DualShock2)
- **`controllerBindingDefsForType(type)`** — per-type binding definitions
- **`controllerSettingDefsForType(type)`** — per-type controller settings (deadzone, sensitivity, etc.)
- **`hotkeyBindingDefs()`** — hotkey definitions
- **`formatBinding()`** — override if emulator uses non-standard binding format (no `+` prefix for buttons, only for axes)

### 4. Register adapter
In `cpp/src/adapters/adapter_registry.cpp`:
- `#include "{emuId}_adapter.h"`
- Add `registerAdapter<{EmuId}Adapter>("{emuId}")` in constructor

### 5. CMakeLists.txt
Add to SOURCES and HEADERS sections.

### 6. Installer asset matching
Override `matchAsset()` in the adapter class to select the correct GitHub release asset
per platform (macOS .dmg/.tar.xz, Windows .zip, Linux .AppImage, etc.). The base class
provides a generic platform-keyword fallback. No changes needed to `emulator_installer.cpp`.

### 7. System mappings
Add system ID in two places:
- `theme_context.cpp` — `systemDisplayNames` map (display name for UI)
- `scraper.cpp` — `systemToScreenScraperId()` (ScreenScraper API ID for scraping)

### 8. Logo
Add emulator logo PNG to `qml/AppUI/images/` and update:
- `EmulatorLogos.js` — add the logo path mapping
- `SetupWizard/EmulatorCard.qml` — add to its local `logoForEmu()` function
- CMakeLists.txt RESOURCES sections (both AppUI and SetupWizard modules)

### 9. Theme assets (optional)
Add system artwork to `themes/modern/assets/`:
- `artwork/{systemId}.webp` — system background
- `logos/{systemId}.webp` — system logo
- `gamepage-logos/{systemId}.webp` — game list logo

### 10. Quick settings previews (optional)
- `AspectRatioSettings.qml` — add to `previewImages` map + add image files to `images/ar/`
- `ResolutionSettings.qml` — add to `previewImages` map + add image files to `images/res/`

### 11. In-game menu support (if applicable)
- Add `PauseOnFocusLoss = true` (or equivalent) to `createDefaultConfig()` and `patchExistingConfig()`
- Add `SaveStateOnShutdown = true` (or equivalent) to enable save-on-SIGTERM
- Suppress native pause menu in INI patching (prevent conflict with our overlay)
- Clear any native pause/toggle hotkeys that would conflict with `PauseOnFocusLoss`
- Override `extractSerial(romPath)` to extract the game serial/ID from the ROM file.
  For disc-based systems (ISO/BIN/CHD), use the shared `Iso9660::readFile()` + `parseSystemCnfSerial()`.
  For cartridge-based systems (N64, GB, GBA), read the serial from the ROM header at the
  platform-specific offset. The serial is stored in the DB at import time (schema v6).
- Override `findResumeFile(serial, savesRoot)` to scan `{savesRoot}/{systemId}/savestates/`
  for resume files matching the serial. **Important:** emulators often reformat the serial
  in filenames (e.g. `SCUS_949.00` → `SCUS-94900`). Check what format the emulator uses
  and convert accordingly in this method.
- If the emulator's CLI flag for loading a state file differs from `-statefile`, override
  `resumeLaunchArgs()` (see DuckStation's `-resume` example)

### 12. RetroAchievements support (if the emulator has native RA support)
- Add console ID mapping in `ra_client.cpp` `consoleIdMapping()` (e.g. `{"n64", 2}`)
  — automatically updates `raConsoleId()`, `allConsoleIds()`, pre-caching, title matching
- Override `supportsRetroAchievements()` → `true` in adapter header
- Override `patchRetroAchievements(username, token, enabled, hardcore, notifications, sounds)`
  to patch the emulator's RA INI section (Enabled + preferences only, ignore username/token)
- Everything else is automatic: title matching, game popup, in-game menu, first-launch
  prompt, settings dashboard, console game list pre-caching

## Settings Sync Strategy

**`configFilePath()` must point at the file the emulator actually reads.** Our settings UI reads and writes that same file, so any change in either UI is instantly visible to the other — no merge logic, no waiting for a game launch. This is how all three current adapters work (PCSX2, DuckStation, PPSSPP).

`ensureConfig()` is called both before launching a game AND when the user clicks "Open Native Settings" in our app. Use it to write embedding-critical keys (wizard suppression, fullscreen, folder paths) so the emulator's native UI never sees a fresh-install state.

**Settings the user has touched in our app are managed; settings they haven't touched are left alone.** Don't write defaults the user never asked for — let the user freely tweak unmanaged settings in the native UI without our app overwriting them on next launch.

**Only keep a sync step for things that live in a different file or section from where our UI writes.** Example: PPSSPP's controller bindings live in `controls.ini` but the controller UI defaults to writing to the main config. Override `controllerBindingsConfigFilePath()` and `controllerBindingsSection(port)` so the UI writes directly to the right place — no sync required. The only thing PPSSPP's `ensureConfig()` still syncs is controller settings (deadzone/sensitivity) from `[Pad1]` to the `[Control]` section in the same `ppsspp.ini`.

## Investigating an Emulator Before Writing the Adapter

Before writing code, you must answer these questions by inspecting the emulator's source or testing it:

### 1. Where does it ACTUALLY read config from?
Don't assume `portable.txt` or `--root` flags work. Launch the emulator natively and check:
```sh
find ~/Library ~/.config ~/Documents -name "{emuId}*.ini" 2>/dev/null
```
Each emulator has its own portable mechanism:
- **PCSX2 / DuckStation:** check for `portable.txt` next to the binary (inside `Contents/MacOS/` on macOS)
- **PPSSPP on macOS:** ignores `memstick_dir.txt` entirely and reads from `~/.config/ppsspp/PSP/SYSTEM/` — it only uses an `NSUserDefaults` preference (`UserPreferredMemoryStickDirectoryPath`, app id `org.ppsspp.ppsspp`) which we set via `defaults write` before launch

### 2. macOS launch method
**Always launch the emulator binary via direct exec** (`QProcess::start(execPath, args)`), NOT via `open` or Launch Services. Going through Launch Services applies app translocation/sandbox rules to downloaded `.app` bundles, which blocks `rename()` inside the bundle and breaks portable mode (the emulator can't atomically save its config). Both `GameSession` and `openNativeEmulatorSettings` use direct exec for this reason.

### 3. What binding format does its controls config use?
Don't trust display names in the native UI — read the actual INI file after letting the emulator auto-configure. Common formats:
- **Named format** (PCSX2, DuckStation): `SDL-0/FaceSouth`, `SDL-0/+LeftTrigger`
- **Numeric format** (PPSSPP): `{deviceId}-{keyCode}` where device 10 = gamepad, keycodes are Android NKCODEs (19=DPadUp, 96=ButtonA, etc.), axes use `4000 + (axisId*2) + (negative?1:0)`
- **Device-specific formats**: Some emulators require per-controller raw button indices in addition to generic mappings

Find the parser function (often `FromConfigString`, `ParseBinding`, or similar) to see exactly what the emulator expects.

### 4. Bindings in a separate file or section?
If the emulator stores controller bindings somewhere other than the main config file or under a port-numbered `Pad{n}` section, override these `EmulatorAdapter` virtuals so our UI writes directly to where the emulator reads:
- `controllerBindingsConfigFilePath()` — defaults to `configFilePath()`
- `controllerBindingsSection(port)` — defaults to `"Pad{port}"`

PPSSPP overrides both: bindings live in `controls.ini` `[ControlMapping]`, not `ppsspp.ini` `[Pad1]`.

### 5. Does it crash on empty binding values?
Some emulators (PPSSPP) crash with SIGSEGV when parsing lines like `Pause =` (empty value after `=`). Only write keys with actual values — for unbound actions, omit the key entirely.

### 6. Does it need dual/multi-bindings for controllers?
On macOS, PPSSPP receives button events via both the SDL GameController API and the raw joystick API. Its auto-configure binds BOTH as comma-separated alternatives (e.g., `Cross = 10-96,10-189`). Without the raw fallback, buttons don't register in-game. Check the native auto-configure output — if it has comma-separated values, you need to replicate that in your defaults and `formatBinding()`.

### 7. Do stored setting values round-trip cleanly?
Our app and the emulator share one INI file, so any value we write must match exactly what the emulator writes back the next time it saves — otherwise the UI silently falls back to the default and the user thinks "my setting didn't stick." **Always check what the emulator writes after a save before defining combo options or defaults.**

The 2026-04-06 / 2026-04-07 audits across PCSX2, DuckStation and PPSSPP turned up the following bug classes. Check for each when adding a new adapter:

- **Float stringification format.** Native code typically writes floats via `StringUtil::ToChars` / `std::to_chars` (shortest representation), so `1.0f` becomes `"1"`, `0.5f` becomes `"0.5"`, `2.0f` becomes `"2"`. Padded forms like `"1.00"` or `"2.0"` in your combo options never match on re-read. Trim them to shortest form.
- **Enum name vs integer string.** Some int settings are written by name (e.g. PCSX2 audio `Backend = Cubeb`, `ExpansionMode = StereoLFE`) via paired `Parse*Name` / `Get*Name` functions in the native source. Find the name table and use the exact-case enum names as your combo INI values, not integer strings.
- **Case-sensitive enum names.** PCSX2 audio backend names are matched via case-sensitive `strcmp`: `"Cubeb"` works, `"cubeb"` does not.
- **Translator-wrapped enums (annotated form).** PPSSPP routes some int settings through a `ConfigTranslator` that writes the value as `<int> (<NAME>)`, e.g. `GraphicsBackend = 3 (VULKAN)`. Your combo INI value must include the parenthesised suffix exactly as the translator writes it. Look for `ConfigTranslator`, `Translate*`, or `(GPUBackendTranslator)`-style wrappers in the native source.
- **Wrong INI section.** Native code may register a setting in a section you don't expect. PPSSPP's `DebugOverlay` is in `[General]`, not `[Graphics]`, because the `ConfigSetting` lives in `generalSettings[]`. Always trace which settings array the field belongs to.
- **Hungarian-prefix mismatch.** PPSSPP C++ field names use prefixes like `iDebugOverlay`, `iFpsLimit1`, `iShowStatusFlags`, but the corresponding INI keys may or may not include the prefix — `iShowStatusFlags` keeps the `i`, `DebugOverlay` drops it. The prefix is set explicitly in the `ConfigSetting` constructor, so always read the actual key string from `Core/Config.cpp`.
- **Mirrored / swapped values.** DuckStation's `SeekSpeedup` uses `1` for "normal speed" and `0` for "maximum cycles override." The native UI binds the value array `{1,2,3,4,5,6,0}` so the labels and INI values aren't in monotonic order. If you assume `0` = off you'll get the opposite of what the user picked.
- **Pre-baked unit conversion.** PPSSPP's `iFpsLimit1` is stored as raw FPS, but the native UI presents it as a percentage and converts via `iFpsLimit1 = (percent * 60) / 100` outside the INI layer. If your UI shows percent, bake the conversion into the combo INI values (e.g. `{"200% (120 FPS)", "120"}`) — do not store the percent number directly.
- **Special-case enum strings.** DuckStation's `Settings::ParseDisplayAspectRatio` accepts the literal strings `"Auto (Game Native)"`, `"Stretch To Fill"`, and `"PAR 1:1"` (with the literal space) in addition to generic `n:m` ratios. Find the `Parse*` function and use the exact literals it expects — `"Auto"`, `"Stretch"`, `"PAR1:1"` will all silently fall back.
- **Hard-coded `"Default"` string for free-form audio fields.** Both PCSX2 and DuckStation expose the audio `Driver` setting as a free-form string with `""` meaning "auto / use Cubeb default". Writing the literal string `"Default"` to the INI causes Cubeb to look up a driver literally named "Default" and silently fail. Use `""` as the INI value for the "Default" entry.
- **Range clamps below an internal minimum.** PPSSPP exposes `AudioBufferSize` as `0..2048`, but the SDL backend silently clamps anything below `128` to `128`. Search the backend code for `std::max(setting, MIN)` patterns and set your `minVal` to the actual clamp.
- **`DONT_SAVE` flags.** PPSSPP's `DebugOverlay` is marked `CfgFlag::DONT_SAVE`, meaning PPSSPP itself never persists it on shutdown. Our app's `ensureConfig()` patches it before launch, so the value lives across game launches as long as the user changes it through our settings UI. Mention this in the tooltip so users aren't confused when launching the emulator outside our app resets it.
- **Packed-int / bitmask settings.** PPSSPP's `iShowStatusFlags` packs FPS / Speed / Battery toggles into one int. Use `SettingDef::bitmask` for these — see the "Bitmask checkboxes" section in `CLAUDE.md`. Verify each bit value against the native bit-test sites (`g_Config.iShowStatusFlags & (int)ShowStatusFlags::*`) before setting `bitmask`.

When in doubt, flip every setting in our UI to a non-default value, save, and `diff` the resulting INI against the unsaved version — the diff is the source of truth for what the emulator round-trips with.

### 8. Does config live in multiple files?
PPSSPP splits settings into `ppsspp.ini` (graphics/audio/system) and `controls.ini` (bindings). Your `ensureConfig` may need to touch multiple files, and `controllerBindingsConfigFilePath()` may need to point at a different file than `configFilePath()`.

### 9. Does the manifest `executable` field match the actual binary?
GitHub releases may ship a different binary than you expect (e.g., PPSSPP ships `PPSSPPSDL.app`, not `PPSSPPQt.app`). Check by extracting a release and running `find ./Contents/MacOS -type f`.

## Verifying the adapter — settings audit pass

Before considering a new adapter complete, run a structured settings audit comparing every `SettingDef` in your `settingsSchema()` against the native source. This catches the round-trip bugs described in section 7 above before users find them.

Prior audits live in `docs/superpowers/audits/` (PCSX2 / DuckStation / PPSSPP, all dated 2026-04-06 or 2026-04-07). Use them as the report-format template. The audit is read-only — write the report first, then plan and execute fixes in a separate branch. For each `SettingDef` check:

1. **Existence** — does the section + key exist in the native source?
2. **Type** — does `SettingDef::Type` match the native accessor (`GetBoolValue` / `GetIntValue` / `GetFloatValue` / `GetStringValue`)?
3. **Default** — does `defaultValue` match the native default?
4. **Combo round-trip** — for every Combo, find the matching write path in the native source and verify each option's INI value string matches exactly what the emulator writes back. This is the highest-risk class of bug.
5. **Bitmask values** — for each `SettingDef::bitmask` checkbox, verify the bit value matches the native bit-test site.
6. **Range** — for `Int`/`Float`, sanity-check `minVal`/`maxVal` against any clamp in the backend.

Section 7 above lists the specific bug classes to look for. The audit reports under `docs/superpowers/audits/` are the canonical examples of what a finished audit looks like.

## Automatic behaviors (no per-emulator code needed)
- **Setup Wizard** — new emulators appear automatically in the wizard's emulator
  selection, resolution, aspect ratio, and BIOS pages. Driven by `ManifestLoader`
  + adapter methods (`resolutionOptions()`, `aspectRatioOptions()`, `biosFiles()`)
- Directory creation via `EmulatorService::ensureConfig()` + `pathsDefs()`
- Settings UI via `ConfigPage` + `settingsSchema()`
- Path UI via `PathsSettings.qml` + `pathsDefs()`
- Controller UI via `ControllerSettings.qml` (app-level) + `controllerBindingDefs()`
- Controller mapping dialog via `ControllerMappingPage` (Widgets) + `controllerTypes()` + `controllerBindingDefsForType()`
- Hotkey UI via `HotkeySettings.qml` + `hotkeyBindingDefs()`
- Emulator management grid in settings overlay (install/uninstall/update/BIOS)
- Install/uninstall via `EmulatorInstaller` + manifest `github_repo`
- Update checking via `GitHubClient::fetchLatestTag()` + manifest `github_repo`
- ROM scanning via `RomScanner` + manifest `rom_extensions`
- Game scraping via `Scraper` + system ID → ScreenScraper mapping in `scraper.cpp`
- RetroAchievements display via `RAService` + console ID mapping in `ra_client.cpp`
- RA first-launch login prompt via `AppController::raEmulatorLoginPrompt` signal + QML dialog
