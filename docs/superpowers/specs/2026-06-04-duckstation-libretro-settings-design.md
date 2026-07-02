# DuckStation libretro — user-facing settings (feature #3)

**Date:** 2026-06-04
**Status:** Design — awaiting review
**Repos touched:** `duckstation-libretro/` (core) + `RetroNest-Project/` (host)
**Predecessor:** feature #1 — `docs/superpowers/specs/2026-06-03-duckstation-libretro-hw-renderer-design.md`
**Handoff:** `duckstation-libretro/docs/settings-schema-handoff-2026-06-04.md`

## 1. Problem

Feature #1 shipped the DuckStation libretro core with a hardcoded enhancement
profile written into the base settings layer by `ApplySettings`
(`src/duckstation-libretro/libretro_settings.cpp`): Metal renderer, 4×
resolution, PGXP on, TrueColor dithering. None of it is user-changeable.

Feature #3 makes the DuckStation settings user-configurable from RetroNest's
settings UI, the same way PCSX2/Dolphin/PPSSPP already are — driven through
libretro **core-options** rather than the hardcoded `ApplySettings` writes.

## 2. Scope

Mirror the **structure** of the standalone `DuckStationAdapter::settingsSchema()`
(`cpp/src/adapters/duckstation_adapter.cpp`) — same categories, groups, labels,
value-sets, tooltips, gating — for every setting the **libretro core actually
honors**, re-plumbed through libretro core-options. We mirror the *standalone
adapter*, not a fresh derivation, because that file already contains audited,
round-trip-correct value strings for every key (its comments cite the exact
`core/settings.cpp` name-table line + audit dates). It is our spelling source.

### 2.1 In scope — categories exposed

| Category | Sub-tab / groups | Settings (core-honored) |
|---|---|---|
| **Recommended** | curated view | duplicate-key view of the most-changed knobs (mirrors standalone), restricted to in-scope keys |
| **Console** | Console / CPU Emulation / CD-ROM Emulation | Region, ForceVideoTiming, Enable8MBRAM; ExecutionMode, OverclockEnable, Overclock %, RecompilerICache; CDROM ReadSpeedup, SeekSpeedup, LoadImageToRAM, LoadImagePatches, AutoDiscChange, IgnoreHostSubcode |
| **BIOS** | (folded into Console group, as standalone does) | PatchFastBoot, FastForwardBoot |
| **Memory Cards** | Memory Card 1 / 2 | Card1Type, Card2Type (type only — paths are host-managed) |
| **Graphics › Rendering** | — | Renderer\*, ResolutionScale, DownsampleMode (+ DownsampleScale), TextureFilter, SpriteTextureFilter, DitheringMode, DeinterlacingMode, AspectRatio†, CropMode†, Scaling†, Scaling24Bit†, PGXPEnable/Culling/TextureCorrection + the 24-bit/FMV PGXP knobs, WidescreenHack, Force4_3For24Bit, ChromaSmoothing24Bit, ForceRoundTextureCoordinates |
| **Graphics › Advanced** | Display Options / Rendering Options | Alignment†, Rotation†, FineCropMode†, DisableMailboxPresentation†; Multisamples, LineDetectMode, EnableModulationCrop, ScaledInterlacing, UseSoftwareRendererForReadbacks |
| **Graphics › Texture Replacement** | General / VRAM Write Replacement | EnableTextureCache + replacement toggles |
| **Graphics › On-Screen Display** | Overlays | **(requires core ImGui bringup — see §5)** ShowFPS, ShowSpeed, ShowCPU, ShowGPU, ShowResolution, ShowGPUStatistics, ShowFrameTimes, ShowLatencyStatistics, ShowInputs, ShowEnhancements |

\* **Renderer** is limited to **Auto / Metal / Software** — not Vulkan/OpenGL.
The HW path is `MetalNSView`; only Metal is wired. Matches the existing stub.

† **Display-pane settings** (AspectRatio, CropMode, Scaling, alignment,
rotation, mailbox) — see §6 risk: in `MetalNSView` mode the core's own
presenter composites the final image, so these *should* apply, but the libretro
shim reports a fixed 4:3 `av_info` (`libretro.cpp:178-184`). Include them, verify
each visibly applies, drop any the host overrides.

### 2.2 Out of scope — host-owned categories (would be dead controls)

These are owned by the RetroNest host in libretro mode and are deliberately
**not** exposed:

- **Audio** (Backend/Driver/Device/Volume/Stretch) — core runs `Audio/Backend=Null`
  by design; RetroNest owns audio output.
- **Achievements** — host-driven via the RA web API + rcheevos hash (feature #4).
- **Capture / Screenshots / Media Capture** — host owns capture.
- **Frame pacing & speed**: VSync, SyncToHostRefreshRate, OptimalFramePacing,
  PreFrameSleep, Emulation/FastForward/Turbo Speed, Runahead, Rewind — host
  drives timing (one `retro_run` per frame, `UseThread=false`); libretro has its
  own runahead/rewind/fast-forward.
- **OSD message-styling** (duration/location/animate/blur) — those govern the
  core's own notification *messages*, which overlap RetroNest's notification
  system. Only the OSD *overlay toggles* (§2.1) are exposed.

### 2.3 Deliberate exclusions with engine reasons

- **`GPU/UseThread` (Threaded Rendering)** — excluded entirely, forced `false`.
  The libretro run loop (`RunFrame` interrupted at `FrameDone`, inline /
  non-threaded) depends on it; exposing it would let the user break the core.
- **Renderer** — Auto/Metal/Software only (see §2.1 note).

## 3. Architecture — three parts

### A. DuckStation core (`duckstation-libretro/`)

1. **Expand `src/duckstation-libretro/libretro_core_options.h`** from the
   3-option stub to the full `retro_core_option_v2_definition` table.
   - Key convention: `duckstation_<area>_<setting>` (e.g.
     `duckstation_gpu_resolution_scale`), following the existing stub +
     `pcsx2_*`/`dolphin_*` precedent.
   - **Defaults = the feature-#1 profile** for the enhancement keys
     (ResolutionScale=`4`, PGXPEnable=`true`, PGXPCulling=`true`,
     PGXPTextureCorrection=`true`, DitheringMode=`TrueColor`, Renderer=`Auto`),
     so a fresh install with no user values still boots at the good profile via
     `GET_VARIABLE` defaults alone.
   - Use category-grouped definitions (`retro_core_options_v2` with
     `categories`) following the Dolphin/PCSX2 `CoreOptionsGraphics.cpp` shape;
     split into `libretro_core_options.{h,cpp}` if the table outgrows a header.

2. **Wire `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2`** in `retro_set_environment`
   (`libretro.cpp:163`) — pass `DuckStationCoreOptionDefinitions()`.

3. **Add a read/apply pass** (the Dolphin/PCSX2 `query()` lambda pattern: read
   each key via `RETRO_ENVIRONMENT_GET_VARIABLE`, parse, write into the
   `SettingsInterface`). It runs **after** `ApplySettings`'s base writes in
   `retro_load_game` (~`:248`), so **core-options override the hardcoded
   defaults** (precedence: user option > #1 default). `UseThread` is *not* read
   — it stays the forced `false` from `ApplySettings`.

4. **Combo-only re-expression.** libretro core-options v2 have no Int/Float
   sliders and no save/load transforms (the standalone adapter uses those for
   e.g. Overclock % → numerator/denominator, OSD scale/margins). Each such
   setting becomes an **enumerated combo** of discrete stops, and any derivation
   moves into the core's apply step — e.g. the core reads
   `duckstation_cpu_overclock_percent` (combo: 50/75/.../1000) and computes
   `OverclockNumerator`/`OverclockDenominator` via the same gcd reduction
   (`Settings::CPUOverclockPercentToFraction`, `settings.cpp:230`) before
   writing them.

### B. RetroNest host (`cpp/src/adapters/libretro/duckstation_libretro_adapter.{h,cpp}`)

Add (mirroring the **Dolphin libretro adapter** shape):

- `settingsSchema()` — `QVector<SettingDef>`, every row
  `Storage::LibretroOption`, `type = Combo` (or `Bool` rendered as a 2-value
  combo). `d.key` = the core-option key; `d.options` value strings match the
  core table **byte-for-byte**. Categories/groups/labels/tooltips mirror the
  standalone `DuckStationAdapter` layout (§2.1).
- `settingsCategoriesWithSubTabs()` → `{"Graphics"}` (Graphics has sub-tabs:
  Rendering / Advanced / Texture Replacement / On-Screen Display).
- `settingsHubCards()` — Recommended + the category cards.
- `previewSpec()` — `"aspect"` preview for Recommended, `"osd"` preview for
  Graphics › On-Screen Display (mirrors Dolphin).
- Use `dependsOn` for gated rows exactly as standalone does (e.g.
  `OverclockEnable`, `DownsampleMode!=Disabled`, `ExecutionMode=Recompiler`).

### C. Schema fidelity

Wire DuckStation into `tools/check_schema_fidelity.py` (the same `opt()`/`gopt()`
lambda callsite shape the tool already parses for Dolphin/PCSX2), so the host
schema and the core option table can never silently diverge on key / default /
value-set. The libretro adapter's schema-building code must use callsite shapes
the tool's `HOST_BLOCK_RE` / `HOST_VALUES_REF_RE` recognise.

## 4. Apply timing — boot-time

Values are read once in `retro_load_game` and written to the base settings
layer (matches PCSX2/Dolphin). Every in-scope setting (resolution scale, CPU
mode, overclock, region, PGXP, OSD toggles) is an init-time setting that needs
a fresh boot to take effect in the engine anyway, so a live mid-game apply path
would be wasted work for v1. Change a setting → relaunch the game → it takes
effect. (Live OSD-toggle is a possible later nicety, explicitly not in v1.)

## 5. OSD overlays — core ImGui bringup (new engine work)

OSD is **not** a free schema addition. DuckStation's OSD (FPS, speed, CPU/GPU
usage, resolution, stats, inputs) is drawn by its **ImGui layer**, which the
libretro core does **not** initialize today: `Host`'s ImGui hooks are no-ops,
`GetDefaultFullscreenUITheme()` returns `""`, auxiliary render windows are
unsupported, and `libretro_host.cpp` marks ImGui *"out of scope for skeleton."*
With no ImGui context the engine's present path skips OSD draw, so the toggles
would be dead.

To make them live, this feature adds, in the core:

1. **Initialize `ImGuiManager`** during the libretro display bringup (after the
   `MetalNSView` swapchain is acquired in `AcquireRenderWindow`).
2. **Draw OSD draw-data onto the NSView present path.** The engine presents via
   `VideoPresenter::PresentFrame()` from inside
   `HandleSubmitFrameCommand`/`HandleUpdateDisplayCommand`
   (`libretro_host.cpp:261-289`); the OSD must composite there, **without** a
   second present (the `FrameDoneOnVideoThread` comment warns explicitly about
   double-present / `RestoreDeviceContext` desync).
3. Shut `ImGuiManager` down cleanly on unload (and respect the
   autorelease-pool-before-dlclose discipline from the resume fix).

This is the first time the libretro core renders ImGui, so it carries the
highest implementation risk in this feature and needs hands-on visual
verification (see §6). A core-rendered performance overlay does not violate the
"one overlay" UX rule (that rule is about native *menu* UIs); the Dolphin
libretro adapter already exposes a working OSD sub-tab, so the pattern is
established.

**Sequencing:** the OSD bringup (core) is a self-contained sub-task. If it
proves unstable on the NSView present path, OSD can be split out to a follow-on
feature without blocking the rest of #3 — the settings plumbing (parts A/B/C)
stands on its own.

## 6. Risks / verify-during-implementation

- **First-run defaults.** Assert a fresh install with empty `options.json` still
  boots at the #1 profile (4× + PGXP + TrueColor), driven by the option-table
  defaults. This is the regression guard for feature #1.
- **Value round-trip.** Each option value string must match (a) the core table
  and (b) what the core's apply step / `Settings::Load` parses into the engine
  enum — exact `settings.cpp` name-table spellings (`Metal`, `TrueColor`,
  `Nearest`, `Adaptive`, `Auto (Game Native)`, …). The standalone adapter is the
  audited source; `check_schema_fidelity.py` is the enforcement.
- **Display-pane settings (AspectRatio/CropMode/Scaling/rotation/alignment).**
  Include them, but verify each *visibly* applies in `MetalNSView` mode; drop
  any the host overrides. The shim's fixed 4:3 `av_info` is a hint, not
  authoritative for the NSView present.
- **OSD compositing** — verify the overlay draws on the NSView without
  disturbing the existing present path (no double-present, no desync).
- **Overclock fraction** — verify the percent-combo → numerator/denominator
  derivation in the core matches what the standalone produces (gcd reduction).
- **Combo-only conversions** — settings the standalone exposes as Int/Float
  sliders must be enumerated sensibly (e.g. OSD scale stops) without losing
  useful range; cap ResolutionScale at a sane max (1×–8× exposed; core max 16×).

## 7. Build / deploy / test

Per the handoff (x86/Rosetta is the current run mode):

```sh
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS" && src/duckstation-libretro/package.sh            # universal + self-contained + no-SDL core

cd /Users/mark/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
```

**Test** (the *user* launches — TCC blocks the agent from the GUI):
`cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1`.
Open the DuckStation settings page, change resolution/PGXP/etc., relaunch a PS1
game, confirm the change took effect (`/tmp/rn.log` GPU_HW init line, e.g.
`Resolution Scale: N`). Toggle an OSD overlay and confirm it renders on the
NSView. Run `tools/check_schema_fidelity.py` and confirm DuckStation passes.

## 8. Out of scope / follow-ons

- Live (mid-game) settings apply.
- The host-owned categories in §2.2.
- Controller types / region beyond what's already wired (#1).
- OSD message-styling knobs.

## 9. Key references

- Core: `src/duckstation-libretro/libretro_core_options.h` (stub to expand),
  `libretro_settings.cpp` (`ApplySettings`), `libretro.cpp`
  (`retro_set_environment`:163, `retro_load_game`:~248, `av_info`:178),
  `libretro_host.cpp` (present path :261-289, ImGui stubs :470+),
  `src/core/settings.cpp` (enum name tables + INI keys).
- Host: `cpp/src/adapters/libretro/duckstation_libretro_adapter.{h,cpp}` (add
  schema here), `dolphin_libretro_adapter.cpp` (closest model),
  `pcsx2_libretro_adapter.cpp`, `cpp/src/adapters/duckstation_adapter.cpp`
  (standalone — the spelling source), `cpp/src/core/libretro/core_runtime.cpp`
  (`m_options`/`declaredOptions`), `environment_callbacks.cpp` (GET_VARIABLE /
  SET_CORE_OPTIONS_V2 handlers), `tools/check_schema_fidelity.py`.
- Reference option tables: `dolphin-libretro/.../CoreOptionsGraphics.cpp`,
  `pcsx2-libretro/.../CoreOptionsGraphics.cpp`.
- Rules: `RetroNest-Project/CLAUDE.md` (Emulator Config Strategy, settingsSchema,
  Bitmask checkboxes, Stored value format must round-trip).
- Delta report: `duckstation-libretro/docs/swanstation-delta-2026-06-01.md` §3.
