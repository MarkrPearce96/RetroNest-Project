# Schema-Driven Settings (with Previews) — Design

**Date:** 2026-05-04
**Author:** Mark Pearce (with Claude)
**Status:** Approved for planning

## Goal

Replace the per-emulator "one `.cpp` page file per category" pattern with a single `GenericSettingsPage` driven by `adapter->settingsSchema()`. Apply first to **Dolphin** (which today has no in-app settings dialog and falls back to the native UI). Generalize PCSX2's existing aspect-ratio and OSD preview widgets so any emulator can opt in to a live preview via a category-level binding map. Existing PCSX2 / DuckStation / PPSSPP dialogs keep their bespoke pages unchanged in v1 — their migration to the generic page is a per-emulator follow-up.

The visual design is **identical** to today's dialogs (same `SettingsDialogTheme` palette, same `SettingsCard` / `SettingsToggleRow` / `SettingsComboRow` / `SettingsSliderRow` / `SettingsSectionHeader` / `SettingsDescriptionBar`). This spec changes how those widgets are assembled, not how they look.

## Acceptance criteria (from the user)

- Clean build: `cd cpp && cmake --build build`.
- All existing tests pass (including PCSX2 schema tests and any DolphinSchema test).
- Dolphin appears in the app's Settings menu and clicking it opens the new in-app dialog (no longer falls back to native).
- All four Dolphin category cards (Interface / Audio / Core / Graphics) open their pages.
- Tweaking each setting type (combo / toggle / slider) persists across app restart.
- Dolphin Graphics → Display sub-tab shows the aspect-ratio preview, which updates live when the aspect combo changes.
- PCSX2 Settings still work end-to-end with no visual or behavioral change (Display preview, OSD preview, all pages).
- Description bar updates on focus across all generic-rendered pages.
- Manual smoke (user runs): launch app → Dolphin Settings → tweak Interface, Audio, Core, Graphics settings → close → relaunch → confirm persistence; PCSX2 Settings → Display & OSD pages render previews and update live.

## Architecture

### Component overview

```
EmulatorSettingsDialogBase            (existing, unchanged)
   └─ DolphinSettingsDialog            (new, ~50 lines)
         ├─ DolphinCategoryHub          (new, ~40 lines — declares 4 cards)
         └─ GenericSettingsPage         (new — the renderer; reused per category)
                ├─ Iterates SettingDef vector for one category
                ├─ Renders cards via SettingsPageBuilder (existing)
                ├─ When PreviewSpec.previewType != "" → splits top row,
                │    mounts preview widget, live-binds settings → preview properties
                └─ Handles dependsOn, bitmask, saveTransform, spatial nav
```

### File creation / modification map

**Created:**
- `cpp/src/ui/settings/generic_settings_page.{h,cpp}`
- `cpp/src/ui/settings/widgets/preview/aspect_ratio_preview.{h,cpp}` (moved + renamed from `pcsx2/widgets/`)
- `cpp/src/ui/settings/widgets/preview/osd_preview.{h,cpp}` (moved + renamed from `pcsx2/widgets/`)
- `cpp/src/ui/settings/dolphin/dolphin_settings_dialog.{h,cpp}`
- `cpp/src/ui/settings/dolphin/dolphin_category_hub.{h,cpp}`

**Modified:**
- `cpp/src/core/setting_def.h` — add optional `saveTransform` callback **and** optional `iniFilePath` field for per-key file routing (mirrors the existing `IniPatch::iniFilePath`).
- `cpp/src/services/config_service.{h,cpp}` — `settingValue()` and `saveSettings()` look up the matching `SettingDef` in the adapter schema; if `SettingDef::iniFilePath` is non-empty, route reads/writes to that file. `saveSettings()` groups by file so a single call can update multiple files atomically.
- `cpp/src/adapters/emulator_adapter.h` — add new virtual `previewSpec(category, subcategory)` returning `{previewType, key→property map}`. Default returns empty (no preview).
- `cpp/src/adapters/dolphin_adapter.{h,cpp}` — extend schema to cover Interface (3, unchanged) + Audio (3, unchanged) + Core (5, unchanged) + Graphics (NEW, two sub-tabs: Display + Rendering). Implement `previewSpec()` for Graphics/Display.
- `cpp/src/adapters/pcsx2_adapter.{h,cpp}` — implement `previewSpec()` for Graphics/Display and Graphics/OSD. Behavior unchanged — this just declares via the new API what the bespoke pages already do.
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp` — update `#include` to the new shared widget path.
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.cpp` — same.
- `cpp/src/ui/app_controller.cpp:387-394` — replace the Dolphin native-fallback branch with `new DolphinSettingsDialog(this, emuId)`.
- `cpp/CMakeLists.txt` — register new sources; remove deleted ones.

**Deleted:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.{h,cpp}` (moved to shared).
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.{h,cpp}` (moved to shared).

### `GenericSettingsPage`

Constructor inputs:
- `EmulatorSettingsDialogBase* dlg` — parent dialog (back-button target, save callback owner).
- `QVector<SettingDef> categorySchema` — already filtered to one category; may span multiple subcategories (sub-tabs) or a single one.
- `EmulatorAdapter* adapter` — for `previewSpec()` lookups per active sub-tab.

Behavior:
1. **Group** entries by `subcategory`, then by `group` within subcategory.
2. If multiple subcategories present → render `SettingsGraphicsSubTabBar` at the top; sub-tab switching swaps the inner content area.
3. For each subcategory:
   - Query `adapter->previewSpec(category, subcategory)`. If non-empty, render top row as horizontal split (settings cards left in a vertical column, preview card right). Otherwise, render standard top-to-bottom card stack.
   - Iterate the subcategory's settings; for each, dispatch on `SettingDef::type`:
     - `Bool` → `SettingsPageBuilder::makeToggleCard`
     - `Combo` → `makeComboCard`
     - `Int`/`Float` with `layout == "slider"` → `makeSliderCard`
     - other types → fallback (toggle for Bool with bitmask, etc.)
   - Group entries that share the same non-empty `group` under a single `SettingsSectionHeader`.
4. **Load** initial values from `AppController::settingValue(emuId, section, key)` for each rendered widget.
5. **Save** via `AppController::saveSettings(emuId, {section/key: value})` on widget change — UNLESS `SettingDef::saveTransform` is set, in which case call that instead.
6. **Dependency resolution**: walk `findChildren<SettingsToggleRow*>()`; for each `SettingDef::dependsOn`, dim + disable dependents when master is off (existing logic from `duckstation_console_page.cpp:373-398`, lifted into the generic page).
7. **Preview live binding**: for each setting whose key appears in the active sub-tab's `PreviewSpec.keyToProperty`, install a `valueChanged`/`toggled`/`activated` connection that updates the preview widget via Qt's metaobject `setProperty(propertyName, value)`. This avoids the generic page knowing about specific preview widget classes.
8. **Spatial navigation**: lift the existing arrow-key spatial-nav `eventFilter` (currently duplicated in every per-emulator page) into the generic page. Single source of truth.
9. **Description bar**: emit `settingFocused(SettingDef)` whenever a card or row gains focus (cards already emit `focused(SettingDef)`).

### Preview widgets (relocated + generalized)

**`AspectRatioPreview`** (was `Pcsx2AspectRatioPreview`):
- Q_PROPERTY-exposed inputs: `aspectMode` (string), `stretchY` (int 0–100), `cropL/T/R/B` (int 0–100), `integerScaling` (bool), `fmvAspectMode` (string).
- All inputs default to a "feature-absent" value (-1 for ints, empty string for enums) — the corresponding overlay simply isn't drawn. Dolphin sets only `aspectMode` and `integerScaling`; PCSX2 sets all of them (unchanged from today).
- Visual rendering identical to current `Pcsx2AspectRatioPreview` — the change is the input surface, not the paint code.

**`OsdPreview`** (was `Pcsx2OsdPreview`):
- Q_PROPERTY-exposed inputs for every show* toggle PCSX2 currently exposes (showFps, showSpeed, showVps, showResolution, showCpu, showGpu, showSettings, showPatches, showInputs, showFrameTimes, showIndicators, showGsStats, showHardwareInfo, showVersion), plus `performancePos` / `messagesPos` / `osdScale`.
- All default to off / hidden / sensible neutral.
- PCSX2 explicitly sets every input as today; other emulators set only what they expose.

Both widgets stay in C++ Qt with the same `paintEvent` rendering. Q_PROPERTY exposure is the only structural change — it lets the generic page bind via `setProperty()` without knowing the concrete class.

### Preview binding API

```cpp
struct PreviewSpec {
    QString previewType;                  // "aspect" | "osd" | "" (no preview)
    QHash<QString, QString> keyToProperty;  // SettingDef::key → preview Q_PROPERTY name
};

class EmulatorAdapter {
    // ... existing members ...
    virtual PreviewSpec previewSpec(const QString& category,
                                    const QString& subcategory) const { return {}; }
};
```

Default returns empty → no preview. Adapters that want a preview override.

Example (Dolphin):
```cpp
PreviewSpec DolphinAdapter::previewSpec(const QString& cat, const QString& sub) const {
    if (cat == "Graphics" && sub == "Display") {
        return {"aspect", {
            {"AspectRatio",     "aspectMode"},
            {"IntegerScaling",  "integerScaling"},
        }};
    }
    return {};
}
```

Example (PCSX2 Display, captures everything PCSX2 binds today):
```cpp
PreviewSpec Pcsx2Adapter::previewSpec(const QString& cat, const QString& sub) const {
    if (cat == "Graphics" && sub == "Display") {
        return {"aspect", {
            {"AspectRatio",         "aspectMode"},
            {"FMVAspectRatioSwitch","fmvAspectMode"},
            {"StretchY",            "stretchY"},
            {"CropLeft",            "cropL"},
            {"CropTop",             "cropT"},
            {"CropRight",           "cropR"},
            {"CropBottom",          "cropB"},
            {"IntegerScaling",      "integerScaling"},
        }};
    }
    if (cat == "Graphics" && sub == "OSD") {
        return {"osd", { /* … OSD bindings … */ }};
    }
    return {};
}
```

### `saveTransform` escape hatch

Add to `SettingDef`:

```cpp
// Optional. If set, generic page invokes this instead of the default
// AppController::saveSettings() call when the widget's value changes.
// Receives a defaultSave(section, key, value) callback the transform
// can invoke 0..N times. Avoids depending on AppController in this
// header.
using SaveCallback = std::function<void(const QString& section,
                                        const QString& key,
                                        const QString& value)>;
std::function<void(const QString& widgetValue,
                   const SaveCallback& defaultSave)> saveTransform;
```

Default is unset → standard save path. Used only for the rare bespoke save logic; DuckStation overclock is the known case (one slider value writes both `OverclockNumerator` and `OverclockDenominator` — transform calls `defaultSave` twice).

### Dolphin schema scope

Current schema (3 categories, 11 settings — see `dolphin_adapter.cpp:197-280`). v1 extends to:

| Category | Sub-tab | Settings (v1) | Notes |
|---|---|---|---|
| Interface | — | PauseOnFocusLost, ConfirmStop, HideCursor | Unchanged from today |
| Audio | — | Backend, Volume, EnableJIT | Unchanged from today |
| Core | — | CPUCore, SkipIPL, EnableCheats, OverclockEnable, Overclock | Unchanged from today |
| Graphics | Display | AspectRatio, InternalResolution, IntegerScaling, VSync, Fullscreen | NEW — preview enabled |
| Graphics | Rendering | AntiAliasing, AnisotropicFiltering, ShaderCompilationMode, MaxAnisotropy, ShaderCache | NEW — no preview |

Concrete keys + defaults + tooltip text are sourced from `references/dolphin-master/Source/Core/DolphinQt/Settings/` panes (used as a catalog reference, not copied code). Final list resolved during implementation when the schema is written against the native source.

Graphics settings route writes to `GFX.ini` via the existing `IniPatch::iniFilePath` field on each `SettingDef` (already used for resolution + aspect ratio in `DolphinAdapter::resolutionOptions()`/`aspectRatioOptions()` — extended here per-key).

## Decisions baked in

1. **Visual design unchanged.** Same `SettingsDialogTheme`, same widget kit. Generic page is a structural refactor of how widgets are assembled, not a visual redesign.
2. **Existing PCSX2 / DuckStation / PPSSPP dialog/page code is behaviorally untouched.** Their pages keep rendering exactly as today. The only changes that touch their files are mechanical: PCSX2's two affected pages get an `#include` path update (preview widgets moved to shared), and PCSX2 / DuckStation / PPSSPP adapters add a `previewSpec()` override (PCSX2 declares its existing bindings; the others return empty until they migrate). No layout or logic change.
3. **PCSX2 preview widgets get promoted to shared, not duplicated.** Single source of truth from day one. Refactor is a file move + Q_PROPERTY exposure + graceful "feature absent" handling. No behavior change in PCSX2.
4. **Dolphin Display sub-tab gets the aspect preview from day one.** OSD preview infrastructure exists but Dolphin doesn't wire it in v1 (its OSD options are minimal).
5. **`saveTransform` lives on `SettingDef`** (per-key) for proximity to the data. Used only by the rare bespoke setting; default save path covers everything else.
6. **Hotkey + controller settings are out of scope.** Those have their own pages and stay as-is.
7. **Memory Cards and BIOS for Dolphin are out of scope for v1.** Users access them via Dolphin's native UI ("Open Native Settings" button on the hub remains).

## Out of scope

- Migrating PCSX2 / DuckStation / PPSSPP dialogs to `GenericSettingsPage` (per-emulator follow-up sessions).
- Dolphin Memory Card / BIOS / Wii Remote / GameCube controller in-app configuration.
- Per-game settings overrides for any emulator.
- Hotkey rebinding for Dolphin.
- Controller mapping for Dolphin (already deferred per `2026-05-03-dolphin-adapter-design.md`).
- New preview types beyond `aspect` / `osd`.

## Risks & mitigations

| Risk | Mitigation |
|---|---|
| PCSX2 regression after preview-widget move | Build PCSX2 + manual smoke of Display + OSD pages before declaring done. Existing PCSX2 schema tests must pass. |
| Generic page misses an edge case from an existing emulator | PCSX2/DuckStation/PPSSPP keep bespoke pages — they're not affected. Only Dolphin uses generic page in v1; edge cases discovered via Dolphin smoke. |
| Preview-widget input generalization changes PCSX2 visual behavior | Q_PROPERTY defaults are "feature absent" → no overlay drawn. PCSX2 explicitly sets every input as today. Visual diff = none. |
| Live binding via `setProperty()` is fragile (typos in property names) | Adapter unit test verifies every `keyToProperty` value names a real Q_PROPERTY on the named preview widget. |
| Dolphin Graphics keys span multiple INI files (`Dolphin.ini` vs `GFX.ini`) | `SettingDef` does not support per-key `iniFilePath` routing today (only `IniPatch` does). The plan adds the field to `SettingDef` and updates `ConfigService::settingValue()`/`saveSettings()` to honor it (look up the matching `SettingDef` in the adapter's schema; if `iniFilePath` is non-empty, route reads/writes to that file; group writes by file). Existing single-file callers see no change. |

## Open questions

To be resolved during the implementation plan or during build-out:

1. **Exact Dolphin Graphics setting list.** v1 lists ~10 settings across Display + Rendering sub-tabs; final list comes from a catalog pass against `references/dolphin-master/Source/Core/DolphinQt/Settings/EnhancementsWidget.cpp` and friends.
2. **Bare key vs fully-qualified key in `PreviewSpec.keyToProperty`.** Lean: bare key (e.g. `"AspectRatio"`), since the generic page already knows the section from the matched `SettingDef`. Implementer confirms during build.
3. **Preview-widget Q_PROPERTY naming convention.** Match Q_PROPERTY name to the property's semantic role (`aspectMode`, `integerScaling`), not the INI key. Adapter binding map handles the translation.
4. **Sub-tab ordering on Graphics.** Display first or Rendering first? Lean: Display first (matches PCSX2 today).

## Migration path (post-v1)

Each later session migrates one existing emulator's dialog to the generic page:
1. **PCSX2 migration session** — remove all `pcsx2/pages/*.cpp` files, replace with the generic page reading PCSX2's existing schema. Keep PCSX2 preview bindings (already declared in `Pcsx2Adapter::previewSpec()` from this v1 spec). Net: thousands of lines deleted, one preview API to test.
2. **DuckStation migration session** — same shape. The DuckStation overclock GCD math is the known `saveTransform` user.
3. **PPSSPP migration session** — same shape. PPSSPP's bitmask checkboxes are already supported by the generic page (existing `SettingDef::bitmask` field).

After all three migrations, the bespoke `pcsx2/pages/`, `duckstation/pages/`, `ppsspp/pages/` directories are deleted entirely. Future emulators (Cemu, RPCS3, …) need only an adapter + schema + a ~80-line dialog/hub stub — zero new page files.
