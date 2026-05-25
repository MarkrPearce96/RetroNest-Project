# SP6 — Dolphin libretro Graphics settings schema

**Date:** 2026-05-25
**Status:** Design approved, ready for plan
**Repos:** `dolphin-libretro` (`libretro` branch) — core options infra; `RetroNest-Project` (`main`) — host adapter schema
**Predecessor:** SP5 (controllers, done). **Successor:** SP7 (Audio + Core/Advanced categories).
**Parent spec:** `2026-05-23-dolphin-libretro-conversion-design.md` (§ "Settings schema mapping").

## Goal

Expose Dolphin's **Graphics** category (General / Enhancements / Hacks / Advanced /
On-Screen Display, ~45 options) as libretro core options (`dolphin_` prefix) plus host
`SettingDef`s, so they appear in RetroNest's settings UI and take effect on the next
launch.

The Dolphin core currently exposes **zero** libretro options — there is no
`RETRO_ENVIRONMENT_SET_VARIABLES` / `SET_CORE_OPTIONS` / `GET_VARIABLE` anywhere in
`Source/Core/DolphinLibretro/`. SP6 builds that infrastructure from scratch (mirroring
`pcsx2-libretro`) and populates it with the Graphics category as the proving ground.
SP7 then adds Audio and Core/Advanced as pure additive content on top of the same infra.

## Scope

**In scope:** core-options infrastructure (emit / read / apply pipeline) + the Graphics
category content + schema-fidelity tooling + host-adapter settings overrides.

**Out of scope (deferred):**

- **Audio** and **Core/Advanced** categories → SP7.
- **Recommended** rollup card → SP7+ (it is a cross-category curated *view*; partial with
  Graphics alone).
- **Renderer option** (`dolphin_renderer` / `GFXBackend`) → SP4. Metal is the only working
  backend today (Vulkan is SP4-deferred), so a single-value combo is noise.
  `hardwareRenderBackend()` stays hardcoded to `MetalNSView`.
- **Live in-game option updates** — options are read once at `retro_load_game`
  (next-launch semantics, matching the existing libretro adapters).
- **Per-game INIs** (`User/GameSettings/{ID}.ini`).

## Architecture

Two-sided mirror of the established PCSX2/PPSSPP/mGBA pattern. Data flow:

```
Core: retro_set_environment → CoreOptions::EmitCoreOptionsV2(cb)      [declares the schema]
        ↓
Host: RetroNest settings UI renders DolphinLibretroAdapter::settingsSchema()
      user edits → persisted to {root}/emulators/libretro/dolphin/options.json
        ↓
Core: retro_load_game → CoreOptions::ReadResolved(cb) → Resolved{ graphics }
        → CoreOptions::Graphics::Apply(resolved.graphics)
        → Config::SetCurrent(Config::GFX_*, …)                       [Dolphin's runtime source of truth]
        ↓
      BootManager::BootCore — subsystems read the already-set Config layer
```

This replaces the deleted standalone adapter's GFX.ini/Dolphin.ini patching. The core
writes directly to Dolphin's runtime `Config::` layer — the source of truth Dolphin's
subsystems already read from. Dolphin's `Config::` already exposes every `GFX_*`
`Info<T>` key we need (`Source/Core/Core/Config/GraphicsSettings.h`), so no new engine
plumbing is required — only the `dolphin_*` → `Config::SetCurrent` mapping.

### Complex-combo fan-out moves to the core

The standalone adapter expressed two-key controls host-side via
`saveTransform`/`loadTransform`:

- **Anti-Aliasing** — one UI combo → Dolphin's `MSAA` (sample count) + `SSAA` (bool).
- **Texture Filtering** — one UI combo → `MaxAnisotropy` + `ForceTextureFiltering`.

In the core-options world this fan-out **moves into the core's `Graphics::Apply`**. The
host declares one plain combo (`dolphin_antialiasing`, `dolphin_texture_filtering`); the
core parses the chosen string and issues multiple `Config::SetCurrent` calls. Host
`SettingDef`s therefore get simpler (no transforms).

## Core side (`dolphin-libretro`, `libretro` branch)

New files in `Source/Core/DolphinLibretro/`:

- **`CoreOptions.{h,cpp}`** — thin aggregator (mirrors `pcsx2-libretro/CoreOptions.{h,cpp}`):
  - `Resolved` struct — for SP6 holds just `Graphics::Values graphics`.
  - `BuildDefinitions()` — concatenates each category's `AppendDefinitions` output +
    the libretro terminator; returns a process-lifetime function-local static by
    reference. Exposed for the structural unit test.
  - `EmitCoreOptionsV2(retro_environment_t cb)` — emit the schema once from
    `retro_set_environment`. Logs once and is non-fatal if the host lacks
    `SET_CORE_OPTIONS_V2`.
  - `ReadResolved(retro_environment_t cb)` — query current user values once at the top
    of `retro_load_game`; NULL / unknown enum strings fall back to defaults with a WARN.
- **`CoreOptionsGraphics.{h,cpp}`** — the Graphics module:
  - `AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)` — one literal
    `out.push_back({...})` block per option (no lambda helper) so
    `tools/check_schema_fidelity.py`'s `CORE_BLOCK_RE` recognizes each entry.
  - `Values` struct — the resolved Graphics values.
  - `Parse*` helpers — string → typed value (enum parsing, AA/texture-filtering split).
  - `Apply(const Values&)` — issue `Config::SetCurrent(Config::GFX_*, …)` for each
    option, including the AA/texture-filtering fan-out.
- **`CORE_OPTIONS_TEST_ONLY`** compile guard (like PCSX2) — when defined, the module
  compiles standalone (a local `FrontendLog` shim, no rest-of-core deps) so the apply
  logic can be unit-tested without booting.

**Wiring:**

- `EmitCoreOptionsV2(cb)` → `LibretroEnvironment.cpp` `retro_set_environment` (after
  stashing the `env_cb`).
- `ReadResolved(cb)` + `Graphics::Apply(resolved.graphics)` → `LibretroFrontend.cpp`
  `retro_load_game`, **before** `BootManager::BootCore`.
- `Source/Core/DolphinLibretro/CMakeLists.txt` — add the new sources + the
  `check_schema_fidelity` custom target.

## Graphics option set (~45, curated parity with the deleted standalone)

Five sub-tabs, matching DolphinQt's Graphics window and the standalone adapter
(recover the exact rows + GFX.ini keys + value lists via
`git -C RetroNest-Project show 03f48ae^:cpp/src/adapters/dolphin_adapter.cpp`):

- **General** — Aspect Ratio, V-Sync, Fullscreen, Precision Frame Timing,
  Render-to-Main, Auto-Adjust Window Size, Shader Compilation mode, Compile-shaders-
  before-starting.
- **Enhancements** — Internal Resolution, Anti-Aliasing (→ `MSAA`+`SSAA`),
  Texture Filtering (→ `MaxAnisotropy`+`ForceTextureFiltering`), Scaled EFB Copy, etc.
- **Hacks** — Skip EFB→RAM copy (`EFBToTextureEnable`), XFB→Texture
  (`XFBToTextureEnable`), EFB CPU access (`EFBAccessEnable`), Texture Cache accuracy
  (`SafeTextureCacheColorSamples`), Store/Immediate XFB, etc.
- **Advanced** — Wireframe, Show Statistics, backend multithreading, etc.
- **On-Screen Display** — Show FPS / FTimes / VPS / Speed, etc.

**Conventions carried over from the standalone:**

- **Int sliders → enumerated combos.** libretro core options v2 are Combo-only, so
  slider settings (e.g. texture-cache samples) become combos with enumerated stops
  chosen so the default + useful extremes are reachable (same approach PCSX2 used for
  StretchY/Crop).
- **Intentional skips** (same as the standalone — they need infra core options can't
  express): Adapter combo (empty on macOS), Custom-Aspect width/height (dynamic
  visibility), Post-Processing effect picker (dynamic shader list), Stereoscopy
  Depth/Convergence (float sliders; widget is integer-only), Color Correction (modal),
  Borderless Fullscreen (upstream Windows-only).

## Host side (`RetroNest-Project`, `main`)

Extend the **existing** `cpp/src/adapters/libretro/dolphin_libretro_adapter.{h,cpp}`
(created in SP3/SP5; no new class) with the settings overrides, mirroring
`pcsx2_libretro_adapter`:

- `settingsSchema()` — ~45 `SettingDef`s with `storage = SettingDef::Storage::LibretroOption`,
  built via `opt()` / `gopt(subcategory, …)` helpers exactly like PCSX2. The `gopt`
  helper hardcodes `category = "Graphics"` and pushes the sub-tab name into
  `subcategory`. Plain combos — no `saveTransform`/`loadTransform` (fan-out is core-side now).
- `settingsCategoriesWithSubTabs()` → `{ "Graphics" }`.
- `settingsHubCards()` → one **"Graphics"** card (Recommended deferred).
- `previewSpec(category, subcategory)` — mirror PCSX2 (e.g. an On-Screen Display
  preview); empty `PreviewSpec` elsewhere.

**Persistence is free** from the base class — `LibretroAdapter::libretroOptionsStore()`
+ `optionsJsonPath()` resolve to `{root}/emulators/libretro/dolphin/options.json`. No new
persistence code.

## Schema fidelity & testing

The core's option table and the host's `settingsSchema()` MUST stay value-for-value
identical, or `OptionsStore::load`'s whitelist silently drops unrecognized values and
users lose settings. Three guards:

- **Build-time (`check_schema_fidelity` CMake target).** Port
  `pcsx2-libretro/tools/check_schema_fidelity.py` → `dolphin-libretro/tools/`, wired as
  a `add_custom_target(check_schema_fidelity …)`. It statically diffs the core's
  `CoreOptions*.cpp` `push_back` blocks against the host adapter's `settingsSchema()`.
  **Cross-repo wrinkle:** the core repo's target must locate RetroNest's adapter source
  — the plan pins that path via a CMake cache var / env var (the two trees live in
  separate repos).
- **Host test (`test_dolphin_libretro_schema.cpp`).** Mirror
  `cpp/tests/test_ppsspp_libretro_schema.cpp` — load the built `dolphin_libretro.dylib`,
  capture its emitted option table at runtime, diff against `settingsSchema()`.
- **Core unit test (`tools/test_settings_apply.cpp`).** Links Common + Core only (no
  ROM); compiled with `CORE_OPTIONS_TEST_ONLY`. Asserts each option string maps to the
  correct `Config::Info<T>` setter, including the AA / texture-filtering fan-out.

**Manual smoke (per the build/deploy dance in the build-setup notes):** build the
universal dylib, deploy to RetroNest, boot a GameCube game and a Wii game, change
Internal Resolution 1x→2x, relaunch, confirm the change takes effect and persists in
`options.json`.

## Error handling

- **Host lacks `SET_CORE_OPTIONS_V2`** — `EmitCoreOptionsV2` logs once and returns false;
  defaults still apply (non-fatal).
- **Unknown / NULL option value at read** — `ReadResolved` falls back to the `Values`
  default for that option and logs a WARN.
- **Schema drift** — caught at build time by `check_schema_fidelity` (build fails with a
  diff) and at test time by `test_dolphin_libretro_schema`.

## Sequencing (informs the plan)

1. Core infra skeleton: `CoreOptions.{h,cpp}` + an empty `CoreOptionsGraphics` +
   `EmitCoreOptionsV2`/`ReadResolved` wiring into the two entry points. Verify the host
   sees an (empty) option set.
2. Graphics `AppendDefinitions` + `Parse*` + `Apply` (the ~45 options), with
   `test_settings_apply.cpp` driving the mapping TDD-style.
3. Host `settingsSchema()` / `settingsHubCards()` / `settingsCategoriesWithSubTabs()` /
   `previewSpec()`.
4. Schema-fidelity: port the python script + CMake target + `test_dolphin_libretro_schema.cpp`.
5. Build universal dylib, deploy, manual smoke on GC + Wii.
