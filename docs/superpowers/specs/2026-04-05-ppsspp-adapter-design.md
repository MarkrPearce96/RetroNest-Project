# PPSSPP Adapter Design Spec

## Overview

Add PPSSPP (PSP emulator) support to the desktop emulation platform. Follows the existing adapter pattern established by PCSX2 and DuckStation. Single controller type, ~25 settings, native RetroAchievements support, and resume state via save states.

## Manifest

```json
{
  "id": "ppsspp",
  "name": "PPSSPP",
  "description": "PPSSPP is a PSP emulator. Play PSP games in HD with save states, controller support, and custom configurations.",
  "systems": ["psp"],
  "github_repo": "hrydgard/ppsspp",
  "executable": "PPSSPPQt",
  "install_folder": "ppsspp",
  "rom_extensions": ["iso", "cso", "chd", "pbp", "elf", "prx"],
  "launch_args": ["--fullscreen", "--escape-exit", "{rom_path}"]
}
```

- `--escape-exit` ensures ESC quits cleanly for SIGTERM flow
- `PPSSPPQt` is the Qt frontend (consistent with PCSX2's Qt build)
- ROM extensions: ISO, CSO (compressed ISO), CHD, PBP (PSN digital), ELF/PRX (homebrew)

## System Mappings

| Location | Key | Value |
|----------|-----|-------|
| `theme_context.cpp` | `"psp"` | `"PlayStation Portable"` |
| `scraper.cpp` | `"psp"` | `61` (ScreenScraper ID) |
| `ra_client.cpp` | `"psp"` | `41` (RetroAchievements console ID) |

Theme assets (artwork, logos, gamepage-logos) for PSP already exist.

## Portable Mode & Folder Structure

Hybrid approach: config alongside executable, folder paths redirected via INI keys.

- Config: `{root}/emulators/ppsspp/ppsspp.ini`
- Controller config: `{root}/emulators/ppsspp/controls.ini`
- No `memstick_dir.txt` — individual paths redirected via INI keys

### Path Definitions

| Label | INI Section/Key | Base | Suffix |
|-------|----------------|------|--------|
| BIOS | `General/FlashFirmwarePath` | Bios | *(none)* |
| Save States | `General/SaveStatePath` | Saves | `psp/savestates` |
| Memory Stick Save Data | `General/MemStickSavePath` | Saves | `psp/memcards` |
| Screenshots | `General/ScreenshotPath` | Data | `ppsspp/screenshots` |
| Cheats | `General/CheatsPath` | Data | `ppsspp/cheats` |
| Textures | `General/TextureReplacementPath` | Data | `ppsspp/textures` |

### BIOS

PSP emulation is fully HLE — no required BIOS files. Optional:
- `ppge_atlas.zim` — built-in UI font atlas (optional)

## Config Patching

### createDefaultConfig (ppsspp.ini)

Minimal embedding-critical keys only:

```ini
[General]
FirstRun = False
AutoLoadSaveState = 0
EnableStateUndo = True
FlashFirmwarePath = {root}/bios/
SaveStatePath = {root}/saves/psp/savestates/
MemStickSavePath = {root}/saves/psp/memcards/
ScreenshotPath = {root}/data/ppsspp/screenshots/
CheatsPath = {root}/data/ppsspp/cheats/
TextureReplacementPath = {root}/data/ppsspp/textures/

[Graphics]
FullScreen = True
InternalResolution = 2

[Sound]
Enable = True
```

### createDefaultControlsConfig (controls.ini)

```ini
[ControlMapping]
Up = d:0/DPAD_UP
Down = d:0/DPAD_DOWN
Left = d:0/DPAD_LEFT
Right = d:0/DPAD_RIGHT
Cross = d:0/BUTTON_A
Circle = d:0/BUTTON_B
Square = d:0/BUTTON_X
Triangle = d:0/BUTTON_Y
Start = d:0/BUTTON_START
Select = d:0/BUTTON_BACK
L = d:0/LEFT_SHOULDER
R = d:0/RIGHT_SHOULDER
An.Up = d:0/AXIS_Y-
An.Down = d:0/AXIS_Y+
An.Left = d:0/AXIS_X-
An.Right = d:0/AXIS_X+
Fast-forward = d:0/RIGHT_TRIGGER
Save State =
Load State =
Pause =
```

- `Pause`, `Save State`, `Load State` cleared to prevent conflict with our overlay
- Fast-forward mapped to R2 by default

### patchExistingConfig

Same keys as create, defensive patching:
- Force `FirstRun = False`, `FullScreen = True`
- Force folder paths
- Clear conflicting hotkeys (`Pause`, `Save State`, `Load State`)

## Controller Bindings

### Controller Types

Single type: `{"standard", "PSP Controller", ""}`

### Binding Definitions (16 bindings)

| Group | Label | Key | Type | Default |
|-------|-------|-----|------|---------|
| D-Pad | Up | Up | Button | `d:0/DPAD_UP` |
| D-Pad | Down | Down | Button | `d:0/DPAD_DOWN` |
| D-Pad | Left | Left | Button | `d:0/DPAD_LEFT` |
| D-Pad | Right | Right | Button | `d:0/DPAD_RIGHT` |
| Face Buttons | Cross | Cross | Button | `d:0/BUTTON_A` |
| Face Buttons | Circle | Circle | Button | `d:0/BUTTON_B` |
| Face Buttons | Square | Square | Button | `d:0/BUTTON_X` |
| Face Buttons | Triangle | Triangle | Button | `d:0/BUTTON_Y` |
| Triggers | L | L | Button | `d:0/LEFT_SHOULDER` |
| Triggers | R | R | Button | `d:0/RIGHT_SHOULDER` |
| System | Start | Start | Button | `d:0/BUTTON_START` |
| System | Select | Select | Button | `d:0/BUTTON_BACK` |
| Analog Stick | Up | An.Up | Axis | `d:0/AXIS_Y-` |
| Analog Stick | Down | An.Down | Axis | `d:0/AXIS_Y+` |
| Analog Stick | Left | An.Left | Axis | `d:0/AXIS_X-` |
| Analog Stick | Right | An.Right | Axis | `d:0/AXIS_X+` |

### formatBinding() Override

Produces PPSSPP's native format: `d:{deviceIndex}/BUTTON_A` for buttons, `d:{deviceIndex}/AXIS_X+` for axes. Bindings written to `controls.ini` `[ControlMapping]` section (separate file from `ppsspp.ini`).

### Hotkey Definitions (13 hotkeys)

| Group | Label | Key | Default |
|-------|-------|-----|---------|
| Speed | Fast-forward | Fast-forward | `d:0/RIGHT_TRIGGER` |
| Speed | Speed Toggle | SpeedToggle | *(empty)* |
| Speed | Alt Speed 1 | Alt speed 1 | *(empty)* |
| Speed | Alt Speed 2 | Alt speed 2 | *(empty)* |
| Speed | Frame Advance | Frame Advance | *(empty)* |
| System | Rewind | Rewind | *(empty)* |
| System | Screenshot | Screenshot | *(empty)* |
| System | Mute Toggle | Mute toggle | *(empty)* |
| System | Reset | Reset | *(empty)* |
| Save States | Save State | Save State | *(empty)* |
| Save States | Load State | Load State | *(empty)* |
| Save States | Previous Slot | Previous Slot | *(empty)* |
| Save States | Next Slot | Next Slot | *(empty)* |

## Settings Schema (~25 settings)

### Emulation Tab

| Label | Section/Key | Type | Default | Options/Range |
|-------|------------|------|---------|---------------|
| Frame Skip | `Graphics/FrameSkip` | Combo | Off | Off, 1, 2, 3 |
| Frame Skip Type | `Graphics/FrameSkipType` | Combo | Number of frames | Number of frames, Percent of FPS |
| Auto Frameskip | `Graphics/AutoFrameSkip` | Bool | false | |
| Fast Memory | `CPU/FastMemory` | Bool | true | |
| I/O Threading | `CPU/IOTimingMethod` | Combo | Fast | Fast, Host, Simulate UMD delays |
| Multithreaded | `CPU/CPUThread` | Bool | true | |

### Graphics Tab

| Label | Section/Key | Type | Default | Options/Range |
|-------|------------|------|---------|---------------|
| Rendering Backend | `Graphics/GraphicsBackend` | Combo | Vulkan | OpenGL, Vulkan, Metal, Software |
| Internal Resolution | `Graphics/InternalResolution` | Combo | 2 | 1x through 10x |
| Texture Filtering | `Graphics/TextureFiltering` | Combo | Auto | Auto, Nearest, Linear, Auto Max Quality |
| Anisotropic Filtering | `Graphics/AnisotropyLevel` | Combo | Off | Off, 2x, 4x, 8x, 16x |
| VSync | `Graphics/VSyncInterval` | Bool | false | |
| Texture Scaling Level | `Graphics/TexScalingLevel` | Combo | 1 | Off, 2x, 3x, 4x, 5x |
| Texture Scaling Type | `Graphics/TexScalingType` | Combo | xBRZ | xBRZ, Hybrid, Bicubic, Hybrid+Bicubic |
| Hardware Transform | `Graphics/HardwareTransform` | Bool | true | |
| Software Skinning | `Graphics/SoftwareSkinning` | Bool | true | |
| Deposterize | `Graphics/TexDeposterize` | Bool | false | |

### Audio Tab

| Label | Section/Key | Type | Default | Options/Range |
|-------|------------|------|---------|---------------|
| Global Volume | `Sound/GlobalVolume` | Int | 6 | 0-10 |
| Alt Speed Volume | `Sound/AltSpeedVolume` | Int | -1 | -1 to 10 |
| Audio Backend | `Sound/AudioBackend` | Combo | Auto | Auto, CoreAudio, SDL |

### System Tab

| Label | Section/Key | Type | Default | Options/Range |
|-------|------------|------|---------|---------------|
| PSP Language | `SystemParam/Language` | Combo | English | Japanese, English, French, Spanish, German, Italian, Dutch, Portuguese, Russian, Korean, Chinese |
| PSP Nickname | `SystemParam/NickName` | String | PPSSPP | |
| Confirm Button | `SystemParam/ButtonPreference` | Combo | Cross | Cross, Circle |

## Resolution Options

Section: `Graphics`, Key: `InternalResolution`

| Label | Value |
|-------|-------|
| 1x PSP (480x272) | `1` |
| 2x (960x544) | `2` (default) |
| 3x (1440x816) | `3` |
| 4x (1920x1088) | `4` |
| 5x (2400x1360) | `5` |
| 10x (4800x2720) | `10` |

## Aspect Ratio Options

| Label | Patches |
|-------|---------|
| Stretch to Display | `Graphics/DisplayAspectRatio = "1.000000"` |
| PSP 16:9 (widescreen) | `Graphics/DisplayAspectRatio = "1.777778"` (default) |
| PSP Native | `Graphics/DisplayAspectRatio = "1.764706"` |

## Resume State

### Serial Extraction

- PSP ISOs: Read `PSP_GAME/PARAM.SFO` from ISO9660 filesystem, parse SFO binary format to extract `DISC_ID` field
- New SFO parser needed (`sfo_parser.h/.cpp`) — SFO is a binary format with header, key table, value table
- CSO/PBP serial extraction deferred to future (ISO only initially)
- Fallback: generate ID from filename if extraction fails

### Resume File Convention

- Reserved slot 0 for resume: `{gameId}-0.ppst`
- `findResumeFile()`: Scan `{root}/saves/psp/savestates/` for `{serial}-0.ppst`
- `resumeLaunchArgs()`: Returns `["--state={stateFilePath}"]`
- Save trigger before SIGTERM: To be determined during implementation (keystroke injection for save-to-slot-0, or PPSSPP auto-save settings)

## RetroAchievements

- `supportsRetroAchievements()` → `true`
- Native RA support via `rc_client` API
- `patchRetroAchievements()` patches `[Achievements]` section:
  - `AchievementsEnable` → `True`/`False`
  - `AchievementsHardcoreMode` → `True`/`False`
  - `AchievementsSoundEffects` → `True`/`False`
- No credential patching (PPSSPP stores its own token)

## Asset Matching

`matchAsset()` override:
- **macOS:** `.zip` containing `macOS`
- **Windows:** `.zip` containing `windows` and `x64`
- **Linux:** `.AppImage` or `.tar.gz` containing `linux`

## Files to Create

1. `manifests/ppsspp.json`
2. `cpp/src/adapters/ppsspp_adapter.h`
3. `cpp/src/adapters/ppsspp_adapter.cpp`
4. `cpp/src/core/sfo_parser.h`
5. `cpp/src/core/sfo_parser.cpp`

## Files to Modify

6. `cpp/src/adapters/adapter_registry.cpp` — register PPSSPPAdapter
7. `cpp/CMakeLists.txt` — add sources, headers, logo resources
8. `cpp/src/ui/theme_context.cpp` — add `{"psp", "PlayStation Portable"}`
9. `cpp/src/services/scraper.cpp` — add `{"psp", 61}`
10. `cpp/src/services/ra_client.cpp` — add `{"psp", 41}`
11. `qml/AppUI/images/` — add PPSSPP logo PNG
12. `cpp/src/ui/EmulatorLogos.js` — add logo path mapping
13. `qml/SetupWizard/EmulatorCard.qml` — add to `logoForEmu()`
