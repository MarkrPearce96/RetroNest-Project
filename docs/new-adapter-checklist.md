# Adding a New Emulator (libretro core)

Every emulator RetroNest ships is a libretro core loaded in-process by
`CoreRuntime`. Core #6 follows this recipe. The old standalone-process
adapter recipe is gone — its surviving reference material (settings audits,
INI round-trip bug classes, SettingDef toolbox) is kept at the bottom of
this file under "Legacy reference".

## The recipe

### 1. Decide: stock core or fork
- **Stock upstream core** (the mGBA path): the core works unmodified against
  plain libretro. No repo to maintain; build or fetch a universal dylib and
  skip straight to step 2. RetroNest-specific niceties that need private ABI
  (game identity, fast-forward export, NSView handover) are simply absent.
- **Private fork** (the duckstation/pcsx2/dolphin/ppsspp path): fork the
  upstream emulator, add a libretro shell, and adopt the contract package:
  run `vendor/retronest-libretro/sync.sh` to vendor the pinned `libretro.h`
  + `retronest_libretro.h` into the fork (its build must checksum the copy
  via `check-drift.sh`), then implement whatever subset of the private env
  commands (0x20001-0x20005) and `retronest_*` exports the core needs.
  NEVER invent private ABI locally — extend the canonical package. Option
  definitions follow `vendor/retronest-libretro/docs/option-style-guide.md`.

### 2. Manifest + logo
Create `manifests/{emuId}.json`:
```json
{
  "manifest_version": 1,
  "id": "{emuId}",
  "name": "Display Name",
  "description": "One-line description",
  "systems": ["{systemId}"],
  "github_repo": "owner/repo",
  "executable": "{core}_libretro.dylib",
  "install_folder": "libretro",
  "rom_extensions": ["iso", "bin", "chd"],
  "launch_args": [],
  "backend": "libretro",
  "core_dylib": "{core}_libretro.dylib",
  "core_arch": "universal",
  "logo": "qrc:/AppUI/qml/AppUI/images/{emuId}_logo.png",
  "detail_page": {}
}
```
- `core_arch` must be honest (`universal` | `x86_64` | `arm64`) — it drives
  the arch-mismatch warning and `scripts/verify-universal.sh`.
- `detail_page` only when needed: `controller_pages` for multi-pad cores
  (see dolphin), `has_patches` for a patches pipeline (see pcsx2). The
  detail page renders entirely from this block via `detailActionRows()` —
  no QML edits.
- Omit `github_repo` for local-only cores (excluded from update checks —
  the duckstation licensing pattern).
- Add `qml/AppUI/images/{emuId}_logo.png` and list it in BOTH qrc resource
  blocks in `cpp/CMakeLists.txt` (grep `dolphin_logo.png` for the spots).
- The loader warns on unknown keys — if you add a manifest field, extend
  `kKnownKeys` + `EmulatorManifest` + the parse block in
  `cpp/src/core/manifest_loader.cpp`, and cover it in
  `tests/test_manifest_libretro_fields.cpp`.

### 3. systems.json entries
For each new `{systemId}`, add to `manifests/systems.json`: display name,
`screenscraper_id`, and `ra_console_id` when RetroAchievements supports the
system. That single entry lights up theme display names, scraping, and the
whole RA stack (fetch set, session runtime, dashboards). Pin the new values
in `tests/test_system_registry.cpp`.

### 4. Adapter subclass
Create `cpp/src/adapters/libretro/{emuId}_libretro_adapter.{h,cpp}`,
inheriting `LibretroAdapter`. Typical surface (see mgba for the minimal
stock-core shape, dolphin/pcsx2 for the full fork shape):
- `coreId()` — required; everything else has workable defaults.
- `hardwareRenderBackend()` — GL / MetalNSView per the core's video path.
- `controllerTypes()` + `controllerBindingDefsForType()` — RetroPad slot
  keys must resolve via `retroPadSlotFromKey()`; defaults are `SDL-0/...`
  element names seeded into the core's `controls.ini`.
- `pathsDefs()` — user-relocatable dirs (saves, savestates, ...).
- `extractSerial()` / `findResumeFile()` — Save & Quit -> Resume support.
- `biosFiles()` if the system needs BIOS.
- Settings surface: see step 6.
Register it in `cpp/src/adapters/adapter_registry.cpp` and add the .cpp/.h
to the `retronest_core` source list in `cpp/CMakeLists.txt`.

### 5. Build + first boot
Full x86_64 build (see CLAUDE.md build section), install the core dylib at
`{root}/emulators/libretro/cores/`, import a ROM, boot it. Fix the boring
stuff (paths, video backend, input) before touching settings.

### 6. Settings: the core declares, the adapter curates
There is no hand-written schema. The pipeline:
1. The core's `SET_CORE_OPTIONS_V2(_INTL)` is captured at session start and
   written to `{root}/emulators/libretro/{emuId}/declared_options.json`;
   opening the settings page before any session seeds it via `CoreProber`.
2. Implement `optionOverlays()` — one entry per option the UI should show:
   placements (category / Graphics sub-tab / group, incl. "Recommended"
   cross-listings), `dependsOn` gates, and the RARE deliberate
   `defaultOverride`. Uncurated options stay hidden but valid.
3. `extraSettings()` for frontend-owned rows (aspect mode / integer scale,
   `Storage::FrontendSetting`) and `settingsHubCards()` + `previewSpec()`
   for the hub layout.
4. Add a guard test (`tests/test_{emuId}_libretro_schema.cpp`) asserting the
   merged shape against a committed fixture in `tests/fixtures/declared/`
   (record it by probing the installed core — see the stage-2 guards for
   the pattern: count lock, defaults-in-values, dependsOn pins).
To change option values/defaults/wording: change the CORE (fork) source,
rebuild, re-probe. Never mirror option data into the adapter.

### 7. Theme assets (optional)
Add system artwork to `themes/modern/assets/`:
- `artwork/{systemId}.webp` — system background
- `logos/{systemId}.webp` — system logo
- `gamepage-logos/{systemId}.webp` — game list logo

### 8. Quick settings previews (optional)
- `AspectRatioSettings.qml` — add to `previewImages` map + add image files to `images/ar/`
- `ResolutionSettings.qml` — add to `previewImages` map + add image files to `images/res/`

### 9. Smoke checklist (hand to the user)
- Manage grid tile shows the logo; detail page rows navigate cleanly with a
  gamepad (settings / controller page(s) / reinstall / reset / uninstall).
- Settings pages render pre-launch (prober-seeded), one option change
  applies in-game and survives an app restart.
- Controller mapping page renders; a remap takes effect in-game.
- Save & Quit -> Resume round-trip.
- Scrape one game (systems.json SS id) and, if RA-supported, one
  achievement unlocks or at least the game identifies (RA console id).
- Arch-mismatch toast absent when core_arch matches the app slice.

## Automatic behaviors (no per-emulator code needed)
- Setup Wizard pages, directory creation, install/update flow, ROM
  scanning, game scraping, RA display + session runtime, settings hub +
  pages, controller mapping dialog, detail page rows — all driven by the
  manifest, `systems.json`, and adapter virtuals above.

## Legacy reference — standalone-era settings playbook

Everything below predates the libretro migration. It matters when a row
uses `Storage::Ini` (no shipped adapter today) or when auditing a fork's
CoreOptions source for round-trip quality; the SettingDef toolbox
(dependsOn DSL, transforms, bitmask) is still the live mechanism set used
by overlays and extraSettings. NOTE: the process-era retirement (2026-07)
DELETED much of the machinery referenced below (`openNativeEmulatorSettings`,
the QProcess launch path, `patchIniKeys`/INI-patching helpers,
`formatBinding` overrides beyond the base) — where this text says a helper
"exists" or "is used", read it as historical; recover implementations from
git history, don't expect them in the tree.

## Investigating an Emulator Before Writing the Adapter

Before writing code, you must answer these questions by inspecting the emulator's source or testing it:

### 1. Where does it ACTUALLY read config from?
Don't assume `portable.txt` or `--root` flags work. Launch the emulator natively and check:
```sh
find ~/Library ~/.config ~/Documents -name "{emuId}*.ini" 2>/dev/null
```
Each standalone emulator has its own portable mechanism. The remaining standalone adapters check for `portable.txt` next to the binary (inside `Contents/MacOS/` on macOS) — DuckStation follows this convention (Dolphin used to, but has since migrated to the libretro core, so it's no longer a standalone binary). Don't assume the next emulator does; some use `NSUserDefaults` keys, env vars, or no portable mode at all and need a different launch-time workaround.

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
GitHub releases may ship a binary whose name differs from the repo name or the marketing name. Check by extracting a release and running `find ./Contents/MacOS -type f`.

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

## Mirroring the upstream UI verbatim (THE rule)

Once round-trip correctness is confirmed, the schema must also **mirror the standalone emulator's settings dialog visually**: same top-level categories, same group order inside each category, same setting order inside each group, same labels, same gating chains. The reference implementation is the standalone DuckStation adapter (`cpp/src/adapters/duckstation_adapter.cpp`); the original Dolphin adapter that pioneered this pattern has since migrated to the libretro core (`cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp`). Future emulator migrations follow the same shape.

### Universal audit recipe

For every emulator pane / widget file in `references/<emu>-master/Source/.../Settings/`, walk the constructor and `addWidget` / `addRow` calls in order. For each widget produce a row:

- `(category, subcategory, group, INI section, INI key, label, default, gating)`

Then diff that against your `settingsSchema()`. Every difference is a fix:

- **Wrong group / sub-tab** → MOVE
- **Wrong position** → REORDER (the schema vector's order IS the display order)
- **Wrong label** → RELABEL (verbatim from upstream's `tr(...)` strings)
- **Wrong combo option label or stored value** → RELABEL combo
- **Missing setting** → ADD
- **Setting in our schema but not in upstream UI** → check whether it's compile-time gated upstream (`#if defined(...)`); if so, drop it from the visible schema and document why in a memory entry. If it's there but we hid it, restore it.
- **Wrong gating** → use the `dependsOn` DSL (see below) to mirror upstream's `setEnabled` / `OnXChanged` chains exactly.

### Use the schema-driven `GenericSettingsPage` for new emulators

`cpp/src/ui/settings/generic_settings_page.cpp` is the rendering target. Wire every visible category through it from your dialog. **Do not write bespoke per-emulator C++ pages for new emulators** — those exist for PCSX2/DuckStation only because they predate the schema-driven pipeline, and they're a migration debt, not a model. See `recommended-page-layout-contract.md` for the rendering contract; `previewSpec(category, subcategory)` is usually the only adapter override needed for previews.

### `SettingDef` mechanisms — full toolbox

Every shape upstream uses has a schema mechanism. **Reach for the existing tool before adding a new mechanism**, and only mark a setting deferred when no current mechanism fits.

- **Plain types**: `Bool` / `Int` / `Float` / `String` / `Combo` with `category`, `subcategory`, `group`, `section`, `key`, `label`, `tooltip`, `defaultValue`. Sliders via `layout = "slider"`. Paired side-by-side via `layout = "paired"`.

- **Multi-file routing**: `iniFilePath`. Use a small lambda helper to stamp it onto every entry in a sub-section (Dolphin uses `gfx()` to stamp `GFX.ini`).

- **Dependency gates** (`dependsOn`): a small boolean DSL. Atoms are `Foo` (truthy), `!Foo` (falsy), `Foo=Bar` (combo equality), `Foo!=Bar` (combo inequality). Combine with `&&` or `||` (single top-level operator, no parens, no mixing). Bare-key form (`dependsOn = "Foo"`) preserves the original truthy-only semantics, so PCSX2 / DuckStation / PPSSPP entries that pre-date the DSL keep working without edits. See `cpp/src/core/setting_dependency.h`. Examples:
  - `"Backend=OpenAL && DSPHLE!=HLE"` — Dolphin's DPL2 Decoder gate
  - `"!EFBToTextureEnable || !XFBToTextureEnable"` — Dolphin's Defer EFB Copies gate
  - `"!ImmediateXFBEnable && !VISkip"` — Dolphin's Skip Duplicate XFBs gate

- **Inverted booleans**: `SettingDef::inverted = true`. Mirrors upstream `ConfigBool(label, key, layer, /*inverted=*/true)` — checkbox visually flips relative to the stored INI value. Use a tiny `inv()` lambda helper alongside the file-routing helper to keep schema entries terse: `gfx(inv({...}))`. Dolphin uses this for "Disable Bounding Box" (`BBoxEnable` stored, displayed inverted), "Ignore Format Changes" (`EFBEmulateFormatChanges`), "Skip EFB Access from CPU" (`EFBAccessEnable`), "Manual Texture Sampling" (`FastTextureSampling`).

- **Multi-key combos** (synthesized): `saveTransform` + `loadTransform`. A single combo whose entries write to two-or-more INI keys at once. Pattern proven across audio, graphics combos, and sliders. Use this when upstream has a `ConfigComplexChoice` or similar combined widget. Examples:
  - Dolphin's "DSP Emulation Engine" — three combo states write `Core/DSPHLE` + `DSP/EnableJIT`
  - Dolphin's "Anti-Aliasing" — seven combo states write `MSAA` + `SSAA`
  - Dolphin's "Texture Filtering" — twelve combo states write `MaxAnisotropy` + `ForceTextureFiltering`

- **Sliders with unit conversion**: same `saveTransform` / `loadTransform`. A slider can display in MB while storing in bytes (Dolphin's MEM1/MEM2 multiply by `0x100000`), or display in % while storing as a float multiplier, etc. Both load and save paths honour the transforms.

- **Bitmask checkboxes**: `SettingDef::bitmask` for emulators that pack flag bits into one int (PPSSPP's `iShowStatusFlags`). Multiple bitmask checkboxes sharing the same `section`/`key` merge correctly without caching.

### Deferral policy — only defer when infra is genuinely missing

Defer a setting **only** when matching upstream needs infrastructure that doesn't exist yet. The currently-recognised blockers (as of the 2026-05-05 Dolphin pass):

- **Float sliders with sub-integer step** — our slider widget is integer-only. Workaround: `saveTransform` from int slider position to a float string. Direct float sliders are deferred.
- **Modal sub-dialogs** — upstream "Configure" buttons that open second-level dialogs (Dolphin's Color Correction, Post-Processing per-shader options). Need a button widget type + sub-modal infra.
- **Filesystem-scanned combos** — combos populated at runtime by reading a directory (Dolphin's Post-Processing Effect picker, scanning `User/Load/Shaders/`). Need dynamic combo population.
- **Dynamic-visibility widgets** — fields shown only when a sibling has a specific value (Dolphin's Custom Aspect Ratio width/height). Different from greying out (which the DSL handles).
- **Datetime widgets** — Dolphin's Custom RTC value picker. No `QDateTimeEdit`-equivalent widget type yet.

Every deferral needs a memory entry naming the upstream widget, the blocking infra, and the affected setting keys. **Do not** silently skip an upstream setting because it "looks debug-only" or "users won't touch it" — if it's in the standalone UI, it goes in our UI unless it hits one of these blockers.

### Dolphin's outstanding deferrals

See `dolphin-schema-alignment.md` (in user memory) for the live list. As of 2026-05-05 the deferred set is: Color Correction modal, Post-Processing Effect picker, Custom Aspect Ratio width/height, Stereo Depth/Convergence sliders, CPU Clock Override + VBI Frequency Override fine-grained sliders, Custom RTC datetime picker, backend-capability gates (HDR / GPU texture decoding etc.), and transitive dependency closure.

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
