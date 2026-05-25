# SP6 — Dolphin libretro Graphics settings schema

**Date:** 2026-05-25
**Status:** Design approved, ready for plan
**Repos:** `dolphin-libretro` (`libretro` branch) — core options infra; `RetroNest-Project` (`main`) — host adapter schema
**Predecessor:** SP5 (controllers, done). **Successor:** SP7 (Audio + Core/Advanced categories).
**Parent spec:** `2026-05-23-dolphin-libretro-conversion-design.md` (§ "Settings schema mapping").

## Goal

Expose Dolphin's **Graphics** category (General / Enhancements / Hacks / Advanced /
On-Screen Display) as libretro core options (`dolphin_` prefix) plus host `SettingDef`s,
so they appear in RetroNest's settings UI and take effect on the next launch. The
deleted standalone adapter exposed ~79 Graphics rows; SP6 ships a **curated ~50** — see
the keep/drop curation under "Graphics option set" below.

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

## Graphics option set (curated ~50)

Five sub-tabs, matching DolphinQt's Graphics window and the standalone adapter
(recover the exact rows + GFX.ini keys + value lists via
`git -C RetroNest-Project show 03f48ae^:cpp/src/adapters/dolphin_adapter.cpp`). The
standalone exposed ~79 Graphics rows; SP6 curates to ~50 by dropping rows that are
meaningless in RetroNest's embedded console UI or are dev-only.

**Keep (~53):**

- **General (5)** — Aspect Ratio, V-Sync, Precision Frame Timing
  (`MAIN_PRECISION_FRAME_TIMING`, not a GFX key), Shader Compilation mode,
  Wait-for-shaders-before-starting.
- **Enhancements (15)** — Internal Resolution (`GFX_EFB_SCALE`), Anti-Aliasing
  (→ `GFX_MSAA`+`GFX_SSAA`), Texture Filtering (→ `GFX_ENHANCE_MAX_ANISOTROPY`
  +`GFX_ENHANCE_FORCE_TEXTURE_FILTERING`), Output Resampling, Scaled EFB Copy,
  Per-Pixel Lighting, Widescreen Hack, Force 24-Bit Color, Disable Fog, Arbitrary
  Mipmap Detection, Disable Copy Filter, HDR Post-Processing, Stereoscopic 3D Mode,
  Swap Eyes, Full Resolution Per Eye.
- **Hacks (14)** — Skip EFB Access from CPU (inv), Ignore Format Changes (inv), Store
  EFB Copies to Texture Only, Defer EFB Copies to RAM, Texture-cache Accuracy, GPU
  Texture Decoding, Store XFB Copies to Texture Only, Immediately Present XFB, Skip
  Duplicate Frames, Fast Depth Calculation, Disable Bounding Box (inv), Vertex
  Rounding, Save Texture Cache to State, VBI Skip.
- **Advanced (9)** — Load Custom Textures, Prefetch Custom Textures, Enable Graphics
  Mods, Crop, Backend Multithreading, Prefer VS for Point/Line Expansion, Cull on CPU,
  Defer EFB Cache Invalidation, Manual Texture Sampling (inv).
- **On-Screen Display (10)** — Show Messages, Font Size, Show FPS, Show Frame Times,
  Show VPS, Show VBlank Times, Show % Speed, Show Performance Graphs, Show Speed
  Colors, Performance Sample Window.

**Drop (~26):** GFXBackend (renderer → SP4); Start in Fullscreen / Render to Main /
Auto-Adjust Window Size (meaningless — RetroNest embeds the render output); Wireframe,
Texture Format Overlay, API Validation Layers, Log Render Time, Disable EFB VRAM
Copies (dev/debug); Dump Textures ×3, Dump EFB/XFB Target ×2, Frame-Dump Resolution
Type, PNG Compression Level (dump tooling); the Movie Window group (Show Movie Window,
Rerecord/Lag/Frame counters, Input Display, System Clock — 6); NetPlay Ping + Chat;
Show Statistics + Projection Statistics (render-debug overlays).

**Conventions carried over from the standalone:**

- **Int sliders → enumerated combos.** libretro core options v2 are Combo-only, so the
  one surviving slider (Texture-cache Accuracy / `SafeTextureCacheColorSamples`) keeps
  the standalone's three enumerated stops (Safe=0 / Default=128 / Fast=512); OSD Font
  Size and Perf-Sample-Window likewise become enumerated combos.
- **Complex-combo fan-out moves to the core** (see Architecture): Anti-Aliasing
  (one combo → `GFX_MSAA`+`GFX_SSAA`) and Texture Filtering (one combo →
  `GFX_ENHANCE_MAX_ANISOTROPY`+`GFX_ENHANCE_FORCE_TEXTURE_FILTERING`) are single host
  combos whose multi-key split happens in the core's `Apply`.
- **Intentional skips** (same as the standalone — they need infra core options can't
  express): Adapter combo (empty on macOS), Custom-Aspect width/height (dynamic
  visibility), Post-Processing effect picker (dynamic shader list), Stereoscopy
  Depth/Convergence (float sliders; widget is integer-only), Color Correction (modal),
  Borderless Fullscreen (upstream Windows-only).

## Host side (`RetroNest-Project`, `main`)

Extend the **existing** `cpp/src/adapters/libretro/dolphin_libretro_adapter.{h,cpp}`
(created in SP3/SP5; no new class) with the settings overrides, mirroring
`pcsx2_libretro_adapter`:

- `settingsSchema()` — ~50 `SettingDef`s with `storage = SettingDef::Storage::LibretroOption`,
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
  `cpp/tests/test_ppsspp_libretro_schema.cpp` — which does **not** load a dylib; it
  links the adapter source in-process (`QTEST_GUILESS_MAIN`) and asserts shape contracts
  against a hand-maintained fixture: every `LibretroOption` key carries the `dolphin_`
  prefix, every `defaultValue` is present in its own `options` list, each `settingsHubCard`
  `categoryKey` resolves to ≥1 row, the five Graphics sub-tabs are all present, and storage
  types are valid. Catches host-side drift/typos; the cross-repo value/default match is the
  Python script's job (above).
- **Core unit test (`tools/test_core_options.cpp`).** Standalone `clang++` compile with
  `-DCORE_OPTIONS_TEST_ONLY` (no Dolphin link — mirrors `pcsx2-libretro`'s
  `tools/test_core_options.cpp`). Asserts `Graphics::Parse` maps each option string to the
  correct `Values` field — **including the AA fan-out** (`"4x MSAA"` → `msaa=4, ssaa=false`)
  and **texture-filtering fan-out** (`"Force Linear and 4x Anisotropic"` →
  `aniso=Force4x, force=Linear`) — plus `BuildDefinitions` structure (key count, single
  terminator, no duplicate keys). `Graphics::Apply` (the 1:1 `Config::SetCurrent` writes) is
  `#ifndef CORE_OPTIONS_TEST_ONLY` so it compiles out of this test; its correctness is
  covered by the manual smoke.

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
