# PCSX2 Settings Page Redesign — Design Spec

**Date:** 2026-04-11
**Target:** PCSX2 emulator settings (`EmulatorSettingsPage` rendered from `PCSX2Adapter::settingsSchema()`)
**Status:** Design approved, ready for implementation plan

## Goals

Replace the current two-panel sidebar-based PCSX2 settings dialog with a card-grid UI driven by keyboard navigation. Provide:

1. A category hub as the landing page.
2. Per-category pages with distinctive sub-category tabs (for Graphics).
3. Mixed-width cards that group related settings instead of one-setting-per-card.
4. A persistent description bar showing help text and recommended values sourced from the upstream PCSX2 codebase.
5. Live visual previews for aspect ratio and OSD settings.

**This redesign applies to PCSX2 only** for the first pass. Other adapters (DuckStation, PPSSPP) continue to use the existing schema-driven widget page until a follow-up spec extends the new layout to them.

## Non-goals

- Changing what settings are exposed or their underlying INI storage.
- Changing the `SettingDef` struct or schema-driven loading/saving logic.
- Redesigning controller mapping, hotkey settings, or any other settings dialog.
- Supporting native window resizing of the dialog — layout is fixed-width, no responsive breakpoints.
- Touch or pointer-first affordances. Keyboard and controller are the primary targets.

## Visual identity

| Role | Colour |
| --- | --- |
| Window background | `#585450` (warm mid-grey) |
| Title bar / description bar / focus preview | `#4a4642` |
| Card background | `#646058` |
| Card border (resting) | `#706c66` |
| Card border (focused) | `#f59e0b` (amber accent) |
| Input field background | `#585450` |
| Primary text | `#f2efe8` |
| Secondary / label text | `#d0ccc4` |
| Muted text (preview labels, disabled) | `#9a9690` |
| Accent (active tab, toggle on, slider fill, recommended pill) | `#f59e0b` |

Font is the system sans (San Francisco / Segoe UI). Base label size is 13 px, section headers 12 px uppercase, page title 20 px. Toggle labels and slider values match card labels at 13 px.

## Window chrome and size

- Fixed minimum size: **950 × 550** (current PCSX2 dialog minimum — unchanged). Individual pages may request a slightly larger minimum when needed for uniform preview proportions.
- Single-window dialog, modal to the main app.
- Native macOS traffic-light buttons remain; they are not a custom element, they're the Qt dialog's default chrome.

## Navigation model

```
Category Hub (landing)
├── Emulation            ─┐
├── Graphics  ──────────  ├── settings page
│   ├── Display           │    (single-category pages)
│   ├── Rendering         │
│   ├── Post-Processing   │
│   └── OSD              ─┘
├── Audio
└── Memory Cards
```

- The landing page shows four large category cards (2×2 grid).
- Clicking / Enter on a card drills into that category.
- A back arrow (`←`) in the top-left of each category page returns to the hub.
- Graphics is the only category with sub-categories. Its page has an icon-tab row below the back button; the other three categories have no tab row.
- Keyboard: arrow keys move focus between cards in a grid, Enter opens a combo / toggles a bool / activates a slider edit mode. Back button (`Key_Back`) returns up one level. Tab navigation is not used — arrow-driven only, matching the existing unified input system.

## Layout primitives

### Card

Rounded 8 px rectangle, 12–14 px padding, 1.5 px border. Focused cards get an amber border plus a 2 px 30%-opacity amber halo via `box-shadow`. Cards can:

- Contain a single label + control (simple case).
- Stack multiple label + control rows vertically (combined card).
- Span the full grid width (`full` variant).
- Be a preview card — same shape but darker `#504c48` background with a `preview-screen` inside.

### Inline row

Inside a card, a row is `display: flex; align-items: center; justify-content: space-between`. Label on the left, control on the right. Used heavily for Speed Control, System Settings cards, and the OSD toggle card.

### Grid row

When multiple cards should sit side-by-side with different widths (e.g. slider card + small toggle card), they wrap in a full-width flex/grid container with `grid-template-columns: 1fr auto` so the secondary card shrinks to its content. This is how Output Latency + Minimal Output Latency and Fast Forward Volume + Mute Output are laid out.

### Section header

Full-width 12 px uppercase amber label with a 1 px bottom border in 40%-opacity `#706c66`. Acts as a group divider inside the card grid. Also used at the top of compound cards (`PERFORMANCE STATS`, `SETTINGS & INPUTS`) to title internal sub-groups.

### Description bar (persistent)

Pinned to the bottom of every settings page. Flex row with:

- `desc-text` on the left — 13 px body copy of the focused setting's description.
- `desc-rec` pill on the top right — 12 px amber text in a 5 px radius chip with `#f59e0b18` background and `#f59e0b40` border, e.g. `Recommended: 100%`.

Min-height 100 px to accommodate longer descriptions without shifting page content. 3 px amber left border. Content sources from upstream PCSX2 (`pcsx2-qt` hover help + INI comments); see `pcsx2-master` in the GitHub reference.

## Pages

### Category Hub (landing)

Four cards in a 2×2 grid, 14 px gap. Each card:

- 48 px square icon tile with a `#585450` background.
- 18 px bold category name.
- 13 px descriptor line ("Speed control, system settings, frame pacing").
- 12 px amber count ("13 settings →").

Below the grid, aligned bottom-right, the legacy **Open Native Settings** button is preserved as an escape hatch.

### Emulation

Three section groups, each with a section header above the relevant cards.

1. **Speed Control** — single full-width card with three inline rows: Normal Speed, Fast-Forward Speed, Slow-Motion Speed. Each row is a label (180 px min) + combo.
2. **System Settings** — full-width card with EE Cycle Rate + EE Cycle Skip as inline combo rows. Below it, a 2-column grid of toggle cards: Enable MTVU, Thread Pinning, CDVD Precache, Enable Cheats, Enable Fast Boot, Host Filesystem.
3. **Frame Pacing** — full-width card with VSync Queue Size as an inline combo row. Below it, 2-column toggle cards: Sync to Host Refresh, VSync, Skip Duplicate Frames, Optimal Frame Pacing.

### Graphics / Display

Top row: two 50%-width cards side by side.

- **Left card** (focused by default) — single column stack of combos: Renderer, Aspect Ratio, FMV Aspect Ratio Override, Deinterlace Mode, Bilinear Filter, and a Widescreen Patches toggle at the bottom. No min-height; the card sizes to its content.
- **Right card** — `preview-card`. Top portion is a 16:9 `preview-screen` containing a proportioned preview box representing the current Aspect Ratio selection (pillarboxes, 4:3 box, etc.). Below the preview, two stacked controls: Vertical Stretch slider and Crop (Left/Top/Right/Bottom compact inputs). The preview screen uses `aspect-ratio: 16/9` so the ratio box scales cleanly at any rendered size.

Below both cards: a full-width 3×2 grid of toggle cards — Anti-Blur, Integer Scaling, No-Interlacing Patches, Screen Offsets, Disable Interlace Offset, Show Overscan.

### Graphics / Rendering

- **Internal Resolution** — full-width focused card at the top with its combo.
- Six cards in a 2-column grid for: Texture Filtering, Trilinear Filtering, Anisotropic Filtering, Dithering, Blending Accuracy, Hardware Mipmapping (toggle).

### Graphics / Post-Processing

Two section groups.

1. **Sharpening / Anti-Aliasing** — one full-width card. Row 1: Contrast Adaptive Sharpening (label + combo). Row 2: Sharpness (label + slider) with FXAA toggle appended on the right of the same row using a secondary label + toggle group.
2. **Filters** — one full-width card. Top row: TV Shader (label + combo) with Shade Boost (label + toggle) appended on the right. Below: a 2×2 slider grid for Brightness, Contrast, Saturation, Gamma. The four sliders are disabled-looking (greyed label/track) when Shade Boost is off.

### Graphics / OSD

Top row: two 50%-width cards side by side, mirroring the Display page layout.

- **Left card** (focused by default) — a compound card with two internal titles.
  - `PERFORMANCE STATS` title at top, then inline toggle rows (label left, toggle right): Show FPS, Show Speed Percentage, Show GPU Usage, Show CPU Usage, Show Resolution, Show VPS.
  - `SETTINGS & INPUTS` title, then inline toggle rows: Show Patches, Show Settings, Show Inputs.
  - Internal titles use the same amber uppercase style as section headers.
- **Right card** — `preview-card`. Top: 16:9 `preview-screen` showing a simulated game scene with live OSD overlays. The overlays render at the positions documented under **Preview behavior → OSD preview** below — perf-column stats follow the `OsdPerformancePos` combo; Indicators / Settings / Patches / Inputs render in their hardcoded corners. Below the preview: OSD Scale slider, then a 2-column row of Messages Position + Performance Position combos.

Below both cards: a 3-column grid of remaining toggles — Show Frame Times, Show Status Indicators, Show GS Statistics, Show Hardware Info, Show Version, Show Video Capture Status, Show Input Recording Status, Show Texture Replacement Status, Warn About Unsafe Settings.

### Audio

Two section groups.

1. **Configuration** — 2-column grid of combo cards: Audio Backend, Expansion Mode, Sync Mode, Driver. Then a full-width Buffer Size slider card. Then a full-width grid row with `1fr auto` columns containing Output Latency (slider) and Minimal Output Latency (toggle) — the toggle card shrinks to its content so the slider takes the remaining width.
2. **Volume Controls** — full-width Standard Volume slider card. Then a full-width grid row with Fast Forward Volume slider and Mute Output toggle using the same `1fr auto` pattern.

### Memory Cards

Unchanged from the mockup pass — two full-width slot compound cards, then a 3-column toggle grid for multitap slots. The slot cards show slot icon, slot title, description, enable toggle, and a file path row with browse affordance.

## Preview behavior

Previews are **live**: every setting change in the UI updates the preview instantaneously with no animation or debouncing. They are not a GS capture — they are QML/Qt-drawn illustrations that reproduce PCSX2's actual display and OSD layout math on top of a fixed fake game scene (`game-scene` gradient backdrop). The game scene never changes, but the aspect/stretch/crop box drawn over it and the OSD overlays drawn on top of it both react live to every relevant setting.

### Aspect ratio preview — live

Reproduces the pipeline in `pcsx2/GS/Renderers/Common/GSRenderer.cpp::CalculateDrawDstRect` + `CalculateDrawSrcRect` (master branch):

1. **Source rect** — start with the native PS2 source rect (the preview assumes 640×448 NTSC interlaced). The source is the fake "game" image rendered inside the preview screen.
2. **Crop** — `GSConfig.Crop[4] = {L, T, R, B}` in PS2 native pixels is subtracted from the source rect. The preview shows the cropped region by masking the game image — the cropped-out areas render as the preview screen's dark letterbox colour (`#3a3632`), not the game texture. This matches PCSX2 cropping the source texture before it reaches the shader (`GSRenderer.cpp:445-455`).
3. **Effective aspect** — the cropped source rect has a new effective aspect (`crop_adjust = src_rect_ar / src_size_ar`). This effective aspect is what gets fitted to the target ratio.
4. **Target aspect** — chosen from `AspectRatioType`:
   - `Stretch` — fill the preview screen edge-to-edge, no letterbox.
   - `Auto 4:3/3:2` — 4:3 for interlaced content, 3:2 for progressive. The preview assumes interlaced (so 4:3) unless FMV override is set.
   - `4:3`, `16:9`, `10:7` — literal ratios.
5. **Letterbox / pillarbox** — the preview screen itself is a fixed 16:9 box. The target ratio is fitted into it using the same `arr = (targetAr * crop_adjust) / clientAr` rule: if `arr < 1` shrink width (pillarbox); if `arr > 1` shrink height (letterbox). Letterboxed bars render in `#3a3632`.
6. **Vertical stretch** — after letterboxing, the target height is multiplied by `StretchY / 100`. Values > 100% make the content taller than its letterbox and bleed vertically; values < 100% leave extra vertical space. PCSX2's config stores `StretchY` as a float; the UI exposes 10–300% (matching the current schema).
7. **Integer scaling** — if the `Integer Scaling` toggle is on, snap the content box to the nearest integer multiple of the source pixel size before drawing, same as `GSRenderer.cpp:357-389`.
8. **Center** — the content is centered inside the preview screen.

A small text label inside the content box shows the resulting effective ratio (`4:3`, `16:9`, etc.) so the user can sanity-check at a glance.

**FMV aspect ratio override** is presentational — when set, the preview ratio label shows `"FMV: <ratio>"` in amber beneath the main label to indicate it only applies during full-motion video.

### OSD preview — live

Reproduces the layout of `pcsx2/ImGui/ImGuiOverlays.cpp`. Every `OsdShow*` toggle in the settings card maps to a specific preview region; turning a toggle on adds the corresponding line/icon to the preview exactly where PCSX2 would render it in game.

**Positioning follows PCSX2 exactly:**

- **Performance column** — honours `OsdPerformancePos` (`None` / `TopLeft` / `TopCenter` / `TopRight` / `CenterLeft` / `Center` / `CenterRight` / `BottomLeft` / `BottomCenter` / `BottomRight`). All of the following stats flow into this column, stacked vertically in this order:
  - `OsdShowFPS` → `"FPS: 59.94"` (or `"FPS: N/A"`)
  - `OsdShowVPS` → `"VPS: 59.94"`
  - `OsdShowSpeed` → `"Speed: 100%"` (red `<95%`, green `>105%`, else white)
  - `OsdShowVersion` → `"PCSX2 v2.x.x"`
  - FPS/VPS/Speed/Version on a single joined line separated by `" | "` when multiple are enabled
  - `OsdShowGSStats` → three lines of GS / memory / queued-frame stats
  - `OsdShowResolution` → `"640x448 NTSC Interlaced"`
  - `OsdShowHardwareInfo` → two lines: `"CPU: <name> (<cores>C/<threads>T)"` and `"GPU: <device>"`
  - `OsdShowCPU` → one line with `EE:`, `GS:`, `VU:` (if MTVU) percentages, e.g. `"EE: 32.5% (5.42ms)  GS: 14.2% (2.36ms)"`
  - `OsdShowGPU` → `"GPU: 42.3% (4.21ms)"`
  - `OsdShowFrameTimes` → a sparkline plot below the column
- **Top-right (hardcoded)** — these always render top-right regardless of `OsdPerformancePos`, stacked in this order:
  - `OsdShowIndicators` → speed-mod icon (slow-motion / fast-forward / turbo / pause). Only visible when the indicated state is active; in the preview we always show the icon when the toggle is on.
  - `OsdShowVideoCapture` → red dot + `"Recording"` label.
  - `OsdShowInputRec` → input recording indicator.
  - `OsdShowTextureReplacements` → texture replacement load counter.
- **Bottom-right (hardcoded)** — `OsdShowSettings` renders a right-aligned line of non-default setting tokens (e.g. `"CR=1 FCDVD VSYNC EER=0 EEC=1"`). When `OsdshowPatches` is also on, the same line gets a prefix like `"DB=2 P=5 C=0 | "`.
- **Bottom-left (hardcoded)** — `OsdShowInputs` shows the first configured controller's button / axis readout, e.g. `"[PAD] 1 • DualShock  | A  X  ↑  LT: 0.42"`. Preview shows a static representative snapshot when the toggle is on.
- **Toasts / warnings** — `WarnAboutUnsafeSettings` is not an OSD overlay but a startup modal; no preview representation.
- **OsdMessagesPos** in the settings UI is for *async toast messages* (achievements, save states, hotkey notifications). These are not tied to any `OsdShow*` toggle, so the preview does not show a sample toast — the OsdMessagesPos combo still saves its value but has no visible effect in the preview.

**Rendering fidelity:** the preview uses a monospace font for all perf-column text (mimicking ImGui default) with a 1 px dark shadow behind each glyph. Margin between the edge of the preview screen and the text matches the `OsdMargin` config (8 px default). Line spacing is tight (`~1.3em`) like the real overlay.

**Reference files in `pcsx2-master`:**
- `pcsx2/Config.h:224-242` — `AspectRatioType` / `FMVAspectRatioSwitchType`
- `pcsx2/Config.h:341-353` — `OsdOverlayPos`
- `pcsx2/Config.h:843-855` — `AspectRatio`, `StretchY`, `Crop[4]`, `OsdMessagesPos`, `OsdPerformancePos`
- `pcsx2/GS/Renderers/Common/GSRenderer.cpp:291-455` — full draw-rect pipeline (`GetCurrentAspectRatioFloat`, `CalculateDrawDstRect`, `CalculateDrawSrcRect`)
- `pcsx2/ImGui/ImGuiOverlays.cpp:180-1253` — all OSD drawing functions (`DrawPerformanceOverlay`, `DrawSettingsOverlay`, `DrawInputsOverlay`, `DrawIndicatorsOverlay`)
- `pcsx2/ImGui/ImGuiOverlays.cpp:1682-1699` — `RenderOverlays` dispatcher

## Focus and description coupling

The description bar shows text for *whichever card currently has focus*. Each `SettingDef` already carries a `tooltip` field in the schema — the new UI treats this field as the description body. Setting-level `recommended` values are a new addition: a separate pass will extend `SettingDef` with an optional `recommendedValue` string (rendered in the amber pill), populated from PCSX2's upstream defaults and tooltip copy. Until that metadata is backfilled, the pill shows the schema's `defaultValue` as the recommendation.

When focus is on a card that groups multiple settings (Speed Control, Shade Boost, etc.), the description shown is for whichever individual control is currently focused *within* the card — navigation within a compound card moves focus through each internal row.

## Mapping to existing code

- The new UI replaces `EmulatorSettingsPage` when the adapter is `pcsx2`. A runtime branch in whatever currently constructs the dialog picks the new page class for `pcsx2` and keeps the legacy page for other adapters.
- Schema consumption stays identical — the new UI reads `PCSX2Adapter::settingsSchema()` and the existing INI read/write helpers. The new page just lays widgets out differently.
- The existing setting category / subcategory / group strings on each `SettingDef` drive the new page's navigation and grouping. The design above assumes the current schema's `category` and `subcategory` strings unchanged. Any section headers that don't match existing `group` fields (e.g. "PERFORMANCE STATS" inside the OSD compound card) are added via static labels in the page builder, not via schema changes.
- `dependsOn` stays in place for the Shade Boost sliders and any other dependent fields — the new UI renders dependent rows as visually disabled but still present (no hiding).
- Controller / keyboard input already uses the unified input system (arrow keys, Return, Key_Back). No new input handling is needed beyond the card-grid focus model.

## Out of scope / future work

- Applying the same layout language to DuckStation and PPSSPP. Their schemas are compatible in shape but the section groupings are different; each needs its own pass.
- A real captured-frame preview for aspect ratio (requires a GS screenshot hook).
- Per-setting "reset to default" buttons — not part of this pass; the existing global reset path still works.
- Search or filter across settings — deferred.

## Reference

- Upstream PCSX2 settings code: `pcsx2-master` (see `pcsx2-qt/Settings/`) for description text and recommended values.
- Current adapter: `cpp/src/adapters/pcsx2_adapter.cpp` — `settingsSchema()` around lines 19–300.
- Current dialog: `cpp/src/ui/settings/emulator_settings_page.cpp` — `setMinimumSize(950, 550)` at line 160; reused by the new PCSX2 page.
- Visual mockups: `.superpowers/brainstorm/36122-1775897705/content/mockup-all.html` (session-local, not committed).
