# DuckStation Settings UI Design

**Date:** 2026-04-15
**Status:** Draft — pending user review

## Goal

Replace the generic schema-driven `EmulatorSettingsPage` dialog used for DuckStation with a bespoke Qt Widgets settings UI that mirrors the PCSX2 settings UI in layout, structure, and visual design. Only the content (categories, subcategories, individual settings) is swapped to match DuckStation's schema. Per-page visual tweaks may follow in later iterations; this design establishes visual parity as the baseline.

## Non-goals

- Refactoring PCSX2 widgets into a shared/common tree. Widgets are reused in place by `#include`-ing the existing `pcsx2_*` headers.
- Controller bindings UI (remains in `controller_settings_widget` and `hotkey_settings_page`).
- Per-game settings overrides.
- Any change to PCSX2's settings UI.
- Any change to the DuckStation adapter's schema or config patching logic.

## Current state

- PCSX2 has a bespoke Qt Widgets settings UI under `cpp/src/ui/settings/pcsx2/`:
  - `pcsx2_settings_dialog`, `pcsx2_category_hub`, `pcsx2_theme.h`
  - Pages: `pcsx2_emulation_page`, `pcsx2_graphics_page` (sub-tab container) + four sub-tab pages (Display, OSD, Rendering, Post-Processing), `pcsx2_audio_page`, `pcsx2_memory_cards_page`
  - Widgets: `pcsx2_toggle`, `pcsx2_toggle_row`, `pcsx2_combo_row`, `pcsx2_slider_row`, `pcsx2_section_header`, `pcsx2_description_bar`, `pcsx2_card`, `pcsx2_graphics_sub_tab_bar`, `pcsx2_aspect_ratio_preview`, `pcsx2_osd_preview`
- DuckStation currently falls through to the generic `EmulatorSettingsPage` (`cpp/src/ui/settings/emulator_settings_page.{h,cpp}`) — a schema-driven sidebar dialog.
- DuckStation's `settingsSchema()` declares six top-level categories: `Console`, `Emulation`, `Graphics` (with `Rendering` and `Advanced` subcategories), `On-Screen Display`, `Audio`, `Memory Cards`.

## Design

### File layout

A new parallel tree under `cpp/src/ui/settings/duckstation/`:

```
cpp/src/ui/settings/duckstation/
  duckstation_settings_dialog.{h,cpp}         — top-level dialog, mirrors pcsx2_settings_dialog
  duckstation_category_hub.{h,cpp}            — 5-card landing page
  duckstation_theme.h                         — colour/size constants (can alias pcsx2_theme values initially)
  pages/
    duckstation_console_page.{h,cpp}
    duckstation_emulation_page.{h,cpp}
    duckstation_graphics_page.{h,cpp}         — graphics sub-tab container
    duckstation_graphics_rendering_page.{h,cpp}
    duckstation_graphics_advanced_page.{h,cpp}
    duckstation_graphics_osd_page.{h,cpp}
    duckstation_audio_page.{h,cpp}
    duckstation_memory_cards_page.{h,cpp}
  widgets/
    duckstation_aspect_ratio_preview.{h,cpp}  — DuckStation-specific AR set
    duckstation_osd_preview.{h,cpp}           — DuckStation-specific OSD schema keys
```

### Widget reuse strategy

The generic PCSX2 widgets are visually identical for DuckStation and are reused in place by `#include`-ing the existing headers:

- `pcsx2/widgets/pcsx2_toggle.h`
- `pcsx2/widgets/pcsx2_toggle_row.h`
- `pcsx2/widgets/pcsx2_combo_row.h`
- `pcsx2/widgets/pcsx2_slider_row.h`
- `pcsx2/widgets/pcsx2_section_header.h`
- `pcsx2/widgets/pcsx2_description_bar.h`
- `pcsx2/widgets/pcsx2_card.h`
- `pcsx2/widgets/pcsx2_graphics_sub_tab_bar.h`

No extraction, renaming, or duplication. If a DuckStation page later needs a visual divergence, the relevant widget can be forked at that time.

Only the two preview widgets are duplicated, because their content genuinely differs:

- `duckstation_aspect_ratio_preview` — DuckStation's aspect ratio list (`Auto (Game Native)`, `Stretch To Fill`, `4:3`, `16:9`, `19:9`, `20:9`, `21:9`, `16:10`, `PAR 1:1`) differs from PCSX2's. Uses existing `qml/AppUI/images/ar/duckstation-*.webp` assets.
- `duckstation_osd_preview` — bound to DuckStation's On-Screen Display schema keys (`OSDScale`, `OSDMargin`, plus boolean toggles), not PCSX2's.

### Category hub (5 cards)

Layout: two 2-card rows plus a full-width stretched card on the bottom row.

| Row | Left column | Right column |
|---|---|---|
| 1 | 🎛️ **Console** — Region, BIOS, fast boot | 🎮 **Emulation** — Speed control, CPU, timing |
| 2 | 🖼️ **Graphics** — Renderer, rendering, advanced, OSD | 🔊 **Audio** — Backend, latency, volume |
| 3 | 💾 **Memory Cards** — Slots and card types (spans both columns) | — |

"Open Native Settings" button in the bottom-right, unchanged from PCSX2's hub.

Settings counts are computed at build time from the DuckStation adapter's `settingsSchema()` rather than hardcoded, so counts stay accurate if the schema changes.

Keyboard / controller navigation mirrors the PCSX2 hub: D-pad/arrows to move between cards, Return/A to activate, Down from the bottom card focuses the "Open Native Settings" button, Up from that button returns to the bottom card.

### Graphics sub-tabs (3 tabs)

`DuckStationGraphicsPage` uses the reused `pcsx2_graphics_sub_tab_bar` with three tabs:

1. **Rendering** — styled like PCSX2's Display tab. Contains, in order:
   - GPU/Renderer and GPU/Adapter combo rows (Graphics top-level, no subcategory).
   - `duckstation_aspect_ratio_preview` widget.
   - AspectRatio combo + CropMode combo + Scaling / FMV Scaling (from the Rendering subcategory, Display group in the schema).
   - Remainder of the Rendering subcategory grouped by the schema's `group` field: resolution scale, texture/sprite filtering, dithering, deinterlacing, PGXP toggles, widescreen rendering, force-round texture coordinates, FMV chroma smoothing, Force 4:3 for FMVs.
2. **Advanced** — Graphics/Advanced subcategory. Groups rendered from the schema's `group` field:
   - Display Options (alignment, rotation, fine crop mode + dependent int fields, disable mailbox presentation).
   - Rendering Options (multi-sampling, line detection, threaded rendering + max queued frames, texture modulation cropping, scaled interlacing, software renderer readbacks).
3. **OSD** — the entire `On-Screen Display` category, folded in here. Contains `duckstation_osd_preview` and the category's settings (OSDScale, OSDMargin, visibility toggles).

### Page construction pattern

Each page follows the PCSX2 page pattern exactly:

1. Obtain the adapter's `settingsSchema()` via `AppController`.
2. Filter by `(category, subcategory)` and group by the `group` field (empty group = ungrouped rows at the top of the page).
3. For each `SettingDef`, instantiate the matching row widget:
   - `SettingDef::Bool` → `pcsx2_toggle_row`
   - `SettingDef::Combo` → `pcsx2_combo_row`
   - `SettingDef::Int` with `min`/`max`/`step` → `pcsx2_slider_row`
   - Any other type encountered in DuckStation's schema (e.g. `String`, path): if PCSX2 already handles it, reuse that widget; if not, skip the row and emit a `qWarning` (matches current generic-dialog behaviour). DuckStation's current schema uses only `Bool`, `Combo`, and `Int`, so in practice only those three row types are needed.
4. Section titles rendered with `pcsx2_section_header`.
5. Focus/hover events wire into `pcsx2_description_bar` at the bottom of the dialog (same signal pattern as PCSX2).
6. `dependsOn` and `hintType = "paired"` / `"inline"` honoured identically to PCSX2.
7. Keyboard/controller navigation: Return/A to edit, Key_Back for Back (not Escape — matches the input-system rule in CLAUDE.md), hierarchical Escape behaviour mirrors `pcsx2_settings_dialog`.

### Wiring into the app

`AppController::openEmulatorSettings(emuId)` (or the equivalent dispatcher that currently builds `EmulatorSettingsPage`) is branched:

```cpp
if (emuId == "pcsx2")      return new Pcsx2SettingsDialog(...);
if (emuId == "duckstation") return new DuckStationSettingsDialog(...);
return new EmulatorSettingsPage(...);   // fallback for PPSSPP and future emulators
```

The generic `EmulatorSettingsPage` stays in the codebase untouched as the fallback path for emulators that have not yet received a bespoke dialog.

### Build integration

- All new `.cpp` files added to the `RetroNest` target in `cpp/CMakeLists.txt`, grouped under a new `# DuckStation settings` block next to the existing `# PCSX2 settings` block.
- No new external dependencies.
- `duckstation_aspect_ratio_preview` asset paths (`qml/AppUI/images/ar/duckstation-4x3.webp`, `-16x9.webp`) are already bundled; no resource changes required.

## Testing

Follows the PCSX2 pattern. Each widget or preview with non-trivial logic gets a focused Qt Test:

- `test_duckstation_aspect_ratio_preview` — mirrors `test_pcsx2_aspect_ratio_preview`, covering each AR option and the preview's rendered output.
- `test_duckstation_osd_preview` — basic smoke test for each visibility toggle and scale/margin changes.

Manual verification checklist:

- Hub loads, keyboard and controller navigation feel identical to PCSX2's.
- Each card opens its category page; Back returns to the hub.
- Each combo/toggle/slider round-trips to `settings.ini` on disk and re-reads correctly after dialog close+reopen.
- `dependsOn` chains (e.g. FineCrop* depends on FineCropMode; MaxQueuedFrames depends on UseThread) disable correctly.
- Description bar populates on focus for every row.
- "Open Native Settings" still launches the bundled DuckStation executable with the portable marker in place.

## Out of scope / explicitly deferred

- Extracting shared widgets into `ui/settings/common/` — deferred until a third emulator (e.g. PPSSPP) needs the same UI pattern.
- Graphics "Post-Processing" sub-tab — DuckStation's post-processing model is different enough that it warrants its own design pass, not a straight port of PCSX2's page.
- Visual theming per emulator — `duckstation_theme.h` is created but initially exposes the same values as `pcsx2_theme.h`. Divergence happens on demand.
- Any schema reshuffling in `duckstation_adapter.cpp` — page layout honours the existing schema.

## Risks

- **AR preview placement inconsistency**: the AR preview sits on the Rendering sub-tab even though PCSX2's equivalent widget sits on its Display sub-tab. Mitigation: clearly documented here; visually this is the intended parity (same "first tab you land on shows the AR preview"), structurally it matches DuckStation's schema.
- **Reusing `pcsx2_*` widgets across emulators creates a naming inconsistency**: a DuckStation page `#include`s a file named `pcsx2_toggle.h`. Accepted for now; documented in this spec as a deliberate deferred refactor.
- **Settings counts drift**: hardcoded counts in the PCSX2 hub can go stale. The DuckStation hub computes counts from the schema; if we want consistency we can retrofit PCSX2 later (out of scope here).
