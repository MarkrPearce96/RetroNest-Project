# PPSSPP Settings UI Redesign

**Date:** 2026-05-02
**Status:** Draft — pending user review

## Goal

Replace the generic schema-driven `EmulatorSettingsPage` dialog used for PPSSPP with a bespoke Qt Widgets settings UI that mirrors the PCSX2 and DuckStation settings dialogs in layout, structure, and visual design. Content (categories, subcategories, individual settings) is mapped to PPSSPP's schema. Visual parity with the existing PCSX2/DuckStation dialogs is the baseline; per-page polish lands later if needed.

## Non-goals

- Refactoring `pcsx2_*` widgets into a shared/neutral tree. Widgets are reused in place by `#include`-ing the existing headers (same pattern DuckStation uses).
- Controller bindings UI (remains in `controller_settings_widget`, `hotkey_settings_page`, `psp_bindings_widget`).
- Per-game settings overrides.
- Any change to PCSX2's or DuckStation's settings UIs.
- Schema rewrites of the PPSSPP adapter beyond a structural reorganization required by the new hub (see "Schema reorganization").

## Current state

- PCSX2 and DuckStation each have bespoke dialogs under `cpp/src/ui/settings/{pcsx2,duckstation}/` (dialog shell + category hub + per-category pages + widgets).
- PPSSPP falls through to the generic `EmulatorSettingsPage` at `cpp/src/ui/app_controller.cpp:862` — a sidebar `QListWidget` + stacked `QGroupBox` dialog with no custom theme.
- PPSSPP's `PPSSPPAdapter::settingsSchema()` (`cpp/src/adapters/ppsspp_adapter.cpp:57`) declares 51 settings in 3 top-level categories: **Graphics** (35 settings, 9 sub-categories — including an "Emulation" sub-section that holds CPU/memory settings), **Audio** (12 settings, 4 sub-sections), **Overlay** (4 settings, with bitmask toggles writing to `iShowStatusFlags`).
- The dialog selector at `app_controller.cpp:862` is hardcoded: `pcsx2 → Pcsx2SettingsDialog`, `duckstation → DuckStationSettingsDialog`, fall-through → `EmulatorSettingsPage`. PPSSPP currently lands in the fall-through.

## Design

### Visual mockup

The redesign was validated against a two-view mockup (category hub + Graphics sub-tab page) using the same `Pcsx2Theme` palette: warm grey window `#585450`, card `#646058`, dark bar `#4a4642`, amber accent `#f59e0b`, primary text `#f2efe8`. Card geometry, sub-tab bar geometry, section headers, toggle/combo rows, and description bar all match the existing PCSX2 dialog exactly.

### Schema reorganization

The current schema groups CPU/memory settings under a `Graphics → Emulation` sub-section, which is misleading (they are not graphics). PCSX2 and DuckStation both expose `Emulation` as a top-level category. To match that pattern (and to keep PPSSPP's Graphics page within a 4-tab sub-bar), this reorganization moves `Graphics → Emulation` to a new top-level `Emulation` category in `PPSSPPAdapter::settingsSchema()`:

- Settings being moved: `Fast Memory`, `Ignore Bad Memory Access`, `I/O Timing Method`, `Force Real Clock Sync`, `CPU Clock Override`.
- INI section/key names are unchanged — only the schema's `category` / `subcategory` strings change.
- No INI migration needed; existing user configs continue to read/write the same `[Graphics]`, `[CPU]`, etc. sections.

The new top-level category structure becomes:

| Category   | Settings | Sub-tabs |
|------------|----------|----------|
| Emulation  | 5        | flat page |
| Graphics   | ~30      | Rendering, Performance, Textures, Pacing & FX |
| Audio      | 12       | flat page (sections rendered as section headers) |
| Overlay    | 4        | flat page |

(Total preserved at 51; the exact Graphics row count is finalized during implementation against the live schema.)

### Graphics sub-tab consolidation

PPSSPP's Graphics has 9 schema sub-sections (after pulling out Emulation, 8 remain plus 3 ungrouped settings). These consolidate into 4 sub-tabs that fit the existing `Pcsx2GraphicsSubTabBar` (4 boxes, 120×64 each):

| Sub-tab        | Source sub-sections                                              |
|----------------|------------------------------------------------------------------|
| Rendering      | Rendering (~5)                                                   |
| Performance    | Performance (~9) + ungrouped settings whose nature is performance-leaning (e.g. command-buffer / hardware-transform style toggles) |
| Textures       | Textures (~7)                                                    |
| Pacing & FX    | Frame Pacing (~6) + Post-Processing (1)                          |

Final placement of currently-ungrouped settings (the schema has a small number not assigned to a sub-section) is decided during implementation by reading each setting's effect and routing it to the closest sub-tab. None will land on a different top-level category.

Section headers within each sub-tab page preserve the original sub-section grouping (e.g. the Pacing & FX page renders a "Frame Pacing" section header above the frame-pacing rows and a "Post-Processing" header above the shader picker).

### File layout

A new parallel tree under `cpp/src/ui/settings/ppsspp/`:

```
cpp/src/ui/settings/ppsspp/
  ppsspp_settings_dialog.{h,cpp}              — top-level dialog, mirrors pcsx2_settings_dialog
  ppsspp_category_hub.{h,cpp}                 — 4-card landing page
  ppsspp_theme.h                              — re-exports Pcsx2Theme tokens (alias header)
  pages/
    ppsspp_emulation_page.{h,cpp}             — flat page
    ppsspp_graphics_page.{h,cpp}              — graphics sub-tab container
    ppsspp_graphics_rendering_page.{h,cpp}
    ppsspp_graphics_performance_page.{h,cpp}
    ppsspp_graphics_textures_page.{h,cpp}
    ppsspp_graphics_pacing_fx_page.{h,cpp}
    ppsspp_audio_page.{h,cpp}                 — flat page with section headers per sub-section
    ppsspp_overlay_page.{h,cpp}               — flat page (bitmask toggles for iShowStatusFlags)
```

No PPSSPP-specific widgets — the existing PCSX2 widget set covers every row type the PPSSPP schema produces. There is no aspect-ratio preview or OSD preview in this redesign (PPSSPP's overlay is simpler than DuckStation's OSD; if a preview is wanted later, it lands as a follow-up).

### Widget reuse strategy

All PCSX2 widgets are visually identical for PPSSPP and are reused in place by `#include`-ing the existing headers:

- `pcsx2/widgets/pcsx2_toggle.h`
- `pcsx2/widgets/pcsx2_toggle_row.h`
- `pcsx2/widgets/pcsx2_combo_row.h`
- `pcsx2/widgets/pcsx2_slider_row.h`
- `pcsx2/widgets/pcsx2_section_header.h`
- `pcsx2/widgets/pcsx2_description_bar.h`
- `pcsx2/widgets/pcsx2_card.h`
- `pcsx2/widgets/pcsx2_graphics_sub_tab_bar.h`

This matches the established convention from the DuckStation redesign (spec `2026-04-15-duckstation-settings-ui-design.md`). Pcsx2 widgets are not renamed or moved.

### Theme

`ppsspp_theme.h` is a thin header that `#include`s `pcsx2_theme.h` and defines `namespace PpssppTheme { using namespace Pcsx2Theme; }`. PPSSPP code references `PpssppTheme::accent()` etc.; under the hood every symbol resolves to `Pcsx2Theme`. This matches `duckstation_theme.h`'s pattern and keeps the door open for per-emulator divergence later without forcing a refactor on day one. No new colour values or QSS strings are introduced.

### Bitmask checkboxes (Overlay page)

PPSSPP's Overlay page already uses `SettingDef::bitmask` for `iShowStatusFlags` (FPS / Speed / Battery / Debug). The new `ppsspp_overlay_page` continues to render these as `Pcsx2ToggleRow` widgets that read/modify individual bits — the bitmask logic lives in the existing `SettingDef` save path and does not change. No new widget type is needed.

### Stored value format (round-trip)

PPSSPP writes some combo values in a translated form (e.g. `GraphicsBackend = 3 (VULKAN)`). The current schema already declares combo options in that exact format so the dialog re-loads correctly after save. The redesign reuses these option lists verbatim — no format changes.

### Routing

`AppController::showEmulatorSettings()` (`cpp/src/ui/app_controller.cpp:862`) gains a third `if` branch:

```cpp
if (emuId == "ppsspp") { new PpssppSettingsDialog(this, emuId); return; }
```

The fall-through to `EmulatorSettingsPage` remains for any future emulator that has not yet been redesigned.

### Native settings button

The hub includes an "Open Native Settings" button identical in placement and styling to PCSX2/DuckStation. It calls the existing `openNativeEmulatorSettings()` flow (which already handles PPSSPP via direct `QProcess::start` per the macOS launch rule in `CLAUDE.md`).

### Keyboard / controller navigation

Behavior is inherited from the PCSX2/DuckStation pattern — no PPSSPP-specific navigation logic:

- Hub: arrow keys move focus between cards, Enter/A activates, Down from bottom-row card focuses the native button, Up from native button returns to bottom-row card.
- Pages: Tab/Shift+Tab cycle within rows (and across sub-tabs on the Graphics page).
- B-button / Escape pops the page (not Backspace, per the input system rules in `CLAUDE.md`).
- Each focusable row emits `focused()` to drive the description bar.

## Build system

Add the new `cpp/src/ui/settings/ppsspp/**/*.{h,cpp}` files to `cpp/CMakeLists.txt` under the `RetroNest` target. No new dependencies, no test target changes (existing PPSSPP tests like `test_ppsspp_schema` continue to verify schema correctness against the reorganized categories — see "Test impact" below).

## Test impact

- `test_ppsspp_schema` — needs assertions updated for the new `Emulation` top-level category and the renamed Graphics sub-categories (`Rendering`, `Performance`, `Textures`, `Pacing & FX`). The setting set is unchanged; only the category/subcategory strings differ.
- No new tests required for the dialog UI (matches PCSX2/DuckStation precedent — visual widgets are exercised by hand).
- Existing PCSX2/DuckStation tests are unaffected (no shared code is modified).

## Risks and trade-offs

- **Schema reorganization is user-visible.** Anyone reading the existing settings UI today will find "Emulation" no longer under Graphics. Acceptable: this matches PCSX2/DuckStation conventions and the old grouping was misleading.
- **Sub-tab consolidation is editorial.** Combining Frame Pacing + Post-Processing into "Pacing & FX" trades two separate small pages for one fuller page. If any individual sub-tab grows beyond comfortable scroll length, the page can split in a follow-up.
- **No aspect-ratio / OSD preview widgets** in this redesign. PPSSPP's overlay is text-only and aspect ratio is fixed by PSP geometry; previews are not adding value here. Easy to add later if needed.
- **Tight coupling to `pcsx2_*` widgets.** A future shared-widget refactor (Approach 2 from brainstorming) would touch PCSX2 + DuckStation + PPSSPP simultaneously. Acceptable: deferred per the same trade-off the DuckStation spec accepted.

## Open questions

None — palette, hub structure, sub-tab grouping, and widget reuse strategy are validated against the v2 mockup.
