# PPSSPP Libretro — OSD Upstream Patch + Adapter Integration Design

**Date:** 2026-05-22
**Status:** Approved, ready for plan
**Predecessor:** `2026-05-22-ppsspp-phase-bc-settings-design.md` (Phase B+C)

---

## Goal

Add a 6th "OSD" hub card to the Settings → PSP page in RetroNest, backed by 4 new `ppsspp_osd_*` libretro core options that wrap PPSSPP's existing on-screen overlay surface. Provides visual parity with the OSD card on PCSX2's Settings page using PPSSPP's existing rendering capabilities.

## Motivation

After Phase B+C landed (`940cfac`), the PPSSPP settings hub has 5 cards (Recommended / System / Video / Input / Hacks) but no OSD card. PCSX2's adapter ships a rich OSD card (16+ keys, position/scale, per-stat toggles) backed by upstream PCSX2's `pcsx2_osd_*` libretro options. PPSSPP's libretro core exposes **zero** OSD options today, so an equivalent card is impossible without patching upstream.

This spec defines the minimum patch to PPSSPP's libretro frontend that surfaces the OSD knobs PPSSPP *already* renders, and the matching RetroNest adapter changes.

## Scope

### In-scope

**ppsspp-libretro fork** (`/Users/mark/Documents/Projects/ppsspp-libretro`):
- Add `"osd"` category and 4 option definitions to `libretro/libretro_core_options.h`.
- Add 4 `check_variable()` blocks in `libretro/libretro.cpp` that read the new options and write to `g_Config.iShowStatusFlags` / `g_Config.iDebugOverlay`.
- Rebuild the universal libretro dylib via `cmake -DLIBRETRO=ON`.
- Install the rebuilt dylib into `/Users/mark/Documents/Projects/RetroNest-Project/emulators/libretro/cores/`.

**RetroNest-Project** (sibling repo):
- Extend `cpp/src/ui/settings/widgets/preview/osd_preview.h/.cpp` with a `showBattery` Q_PROPERTY and a small battery glyph painted alongside the existing FPS/Speed indicators.
- Extend `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`:
  - Add a 6th `SettingsHubCard` (📊 "OSD") at grid position (row 2, col 1).
  - Add 4 `SettingDef` rows under category `"OSD"`.
  - Extend `previewSpec()` to return an OsdPreview binding for `("OSD", "")`.
- Update `cpp/tests/test_ppsspp_libretro_schema.cpp`:
  - Bump expected count from 55 to 59.
  - Add the 4 new keys to `knownUpstreamKeys()`.
  - Add a new test slot asserting the OSD previewSpec wiring.

### Out-of-scope (explicit non-goals)

- **Position / scale / bold-text toggles** — PPSSPP's `DrawFPS` and `DrawDebugStats` render at fixed positions with no scale knob. Adding these requires modifying PPSSPP's rendering code (rejected during brainstorming).
- **Per-stat granular toggles** like PCSX2's separate Show GPU / Show CPU / Show VPS. PPSSPP's overlay system doesn't expose them — its `iDebugOverlay` is a mutually-exclusive enum.
- **Developer-only flags** (`bShowFrameProfiler`, `bShowDeveloperMenu`, `iShowImDebugger`). These aren't user-facing OSD; they belong on a hypothetical Developer card, not OSD.
- **Recommended-card duplicates** of the OSD entries. OSD options are advanced; the Recommended card already covers the high-impact knobs.

## Architecture

### Component diagram

```
┌──────────────────── ppsspp-libretro fork ────────────────────┐
│                                                               │
│  libretro/libretro_core_options.h                             │
│     ├── categories: + "osd"                                   │
│     └── option_defs_us: + 4 ppsspp_osd_* entries              │
│                          │                                     │
│  libretro/libretro.cpp   │                                     │
│     check_variables():   ▼                                     │
│     +4 var.key blocks ──> g_Config.iShowStatusFlags (bitmask) │
│                       └─> g_Config.iDebugOverlay   (int enum) │
│                                  │                             │
│  UI/DebugOverlay.cpp (unchanged) │                             │
│     DrawFPS()           ◄────────┤                             │
│     DrawDebugOverlay()  ◄────────┘                             │
│                                                               │
│  cmake -DLIBRETRO=ON → ppsspp_libretro.dylib (universal)      │
└──────────────────────────┬────────────────────────────────────┘
                           │ cp
                           ▼
┌──────────────────── RetroNest-Project ───────────────────────┐
│  /emulators/libretro/cores/ppsspp_libretro.dylib              │
│                           │ loaded by                          │
│                           ▼                                    │
│  cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp        │
│     settingsSchema()    +4 OSD SettingDefs                    │
│     settingsHubCards()  +1 OSD card                           │
│     previewSpec()       +1 ("OSD","") branch                  │
│                           │ binds                              │
│                           ▼                                    │
│  cpp/src/ui/settings/widgets/preview/osd_preview.{h,cpp}      │
│     +Q_PROPERTY showBattery + battery glyph paint code        │
└───────────────────────────────────────────────────────────────┘
```

### PPSSPP overlay surface (the ground truth being wrapped)

From `Core/ConfigValues.h:210-214`:
```cpp
enum class ShowStatusFlags {
    FPS_COUNTER     = 1 << 1,   // = 2
    SPEED_COUNTER   = 1 << 2,   // = 4
    BATTERY_PERCENT = 1 << 3,   // = 8
};
```

From `Core/Config.h:646` and `Core/ConfigValues.h:253-266`:
```cpp
enum class DebugOverlay : int {
    OFF,                  // 0
    DEBUG_STATS,          // 1
    FRAME_GRAPH,          // 2
    FRAME_TIMING,         // 3
#ifdef USE_PROFILER
    FRAME_PROFILE,
#endif
    CONTROL,              // 4 (when USE_PROFILER undefined)
    Audio,                // 5
    GPU_PROFILE,          // 6
    GPU_ALLOCATOR,        // 7
    FRAMEBUFFER_LIST,     // 8
};
```

**Verified:** `USE_PROFILER` is never defined anywhere in the source tree (grep found no `#define USE_PROFILER` outside the commented `// #define USE_PROFILER` in `Common/Profiler/Profiler.h:5`, and the libretro CMake / Makefile set no such flag). So the enum is **compactly numbered 0..8** in every build — there is no gap to skip, and the libretro option's 9 values map 1:1 to enum indices 0..8. No mapping table needed.

Plan still includes a defensive verification step that re-runs the grep before wiring, so the assumption is checked rather than trusted.

### The 4 new libretro options

| Option key                  | Type    | Default     | Backing                              |
|-----------------------------|---------|-------------|--------------------------------------|
| `ppsspp_osd_show_fps`       | toggle  | `disabled`  | `iShowStatusFlags` bit `1<<1`        |
| `ppsspp_osd_show_speed`     | toggle  | `disabled`  | `iShowStatusFlags` bit `1<<2`        |
| `ppsspp_osd_show_battery`   | toggle  | `disabled`  | `iShowStatusFlags` bit `1<<3`        |
| `ppsspp_osd_debug_overlay`  | combo×9 | `Off`       | `iDebugOverlay` (int, mapped enum)   |

Defaults all `disabled` / `Off` match PPSSPP's standalone defaults — no surprise OSD on first run.

### The check_variables() integration

In `libretro/libretro.cpp`, immediately after the existing `ppsspp_psp_model` block (`~line 615`), add:

```cpp
   // OSD status indicators — packed into iShowStatusFlags bitmask.
   {
      unsigned flags = 0;
      var.key = "ppsspp_osd_show_fps";
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
          && !strcmp(var.value, "enabled"))
         flags |= (1 << 1);
      var.key = "ppsspp_osd_show_speed";
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
          && !strcmp(var.value, "enabled"))
         flags |= (1 << 2);
      var.key = "ppsspp_osd_show_battery";
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
          && !strcmp(var.value, "enabled"))
         flags |= (1 << 3);
      g_Config.iShowStatusFlags = (int)flags;
   }

   // OSD debug overlay — single enum. With USE_PROFILER undefined (verified
   // for all our builds), enum DebugOverlay is compactly numbered 0..8, so
   // label index == enum value. No mapping table needed.
   var.key = "ppsspp_osd_debug_overlay";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      static const char* labels[] = {
         "Off", "Debug stats", "Frame timings graph", "Frame timing",
         "Control debug", "Audio debug", "GPU profile",
         "GPU allocator viewer", "Framebuffer list",
      };
      for (size_t i = 0; i < sizeof(labels)/sizeof(labels[0]); ++i) {
         if (!strcmp(var.value, labels[i])) {
            g_Config.iDebugOverlay = (int)i;
            break;
         }
      }
   }
```

This block is re-evaluated on every `check_variables()` call (each save-state load / option-page close), so toggling at runtime takes effect immediately.

### OsdPreview extension

Add to `osd_preview.h`:

```cpp
Q_PROPERTY(bool showBattery READ showBattery WRITE setShowBattery)
// ...
public:
    bool showBattery() const { return m_showBattery; }
    void setShowBattery(bool on);
private:
    bool m_showBattery = false;
```

Add to `osd_preview.cpp`: a `setShowBattery` that flips `m_showBattery` and `update()`s, plus paint code that draws a small "🔋 100%" glyph beside the existing FPS/Speed indicators when `m_showBattery` is true. Implementation tracks the existing paint logic for showFps/showSpeed verbatim.

### Adapter wiring

```cpp
// In settingsHubCards(), append:
{ QStringLiteral("\U0001F4CA"), "OSD",
  "On-screen FPS, speed, battery, and debug overlays",
  "OSD", 2, 1 },

// In settingsSchema(), append a new "=== OSD (4) ===" block with 4 opt(...) rows.

// In previewSpec(), add:
if (category == "OSD" && subcategory.isEmpty()) {
    return { "osd", {
        { "ppsspp_osd_show_fps",     "showFps" },
        { "ppsspp_osd_show_speed",   "showSpeed" },
        { "ppsspp_osd_show_battery", "showBattery" },
    }};
}
```

### Final hub layout

```
┌────────────────────── Recommended ──────────────────────┐
│  (aspect preview pane on right, 12 settings rows)        │
├──────────────┬──────────────┬──────────────┐
│   System     │    Video     │    Input     │
│   (11 rows)  │   (22 rows)  │   (4 rows)   │
├──────────────┼──────────────┴──────────────┘
│    Hacks     │     OSD      │
│   (6 rows)   │   (4 rows)   │  (osd preview pane)
└──────────────┴──────────────┘
```

Schema count: 55 → 59 (+4 new OSD rows). No changes to existing counts.

## Risk assessment

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| `iDebugOverlay` enum index drift between PPSSPP versions | low | high (wrong overlay shows) | Plan explicitly dumps & pins enum values; menu order documented in libretro_core_options.h via a comment |
| Battery percent always reports 100% in libretro (no host battery API) | high | low (cosmetic — toggle still does something) | Documented as known-cosmetic. Preview shows a battery glyph regardless of host battery state |
| Universal dylib build broken on current branch | medium | blocks the whole chain | Plan Task 1 is a clean-build verification before any new code lands |
| `g_Config.iShowStatusFlags` overwrite races a user toggle in PPSSPP's standalone settings menu | n/a | n/a | Standalone settings menu is unreachable from libretro builds — no race possible |
| Other adapters that bind `OsdPreview` regress when the showBattery Q_PROPERTY is added | low | medium | Q_PROPERTYs are additive; existing bindings ignore unknown properties. Verified via existing test suite (PCSX2's OSD test must keep passing) |

## Test plan

### Unit (in `test_ppsspp_libretro_schema.cpp`)
- `totalCount_matchesSpec`: bumped 55 → 59
- `everyKey_isKnown`: 4 new keys added to allow-list
- `recommendedHasNaturalDupe`: no change (OSD entries are not in Recommended)
- `hubCards_referencedByEntries`: must remain green after the 6th card lands
- **New slot** `previewSpec_osd_isOsd`:
  - asserts `previewSpec("OSD", "").previewType == "osd"`
  - asserts the 3 key→property pairs present
  - asserts `previewSpec("Hacks", "")` etc. still empty

### Build verification (ppsspp-libretro)
- `cmake -DLIBRETRO=ON .` produces a working `Makefile`/Ninja config
- Resulting `ppsspp_libretro.dylib` loads under macOS without missing symbols (`otool -L`, `nm -gU | grep retro_`)
- Universal binary: `lipo -info` reports `x86_64 arm64`

### Runtime smoke (manual)
- Launch RetroNest, navigate Settings → PSP → OSD card opens with 4 rows
- Toggle Show FPS → launch a PSP game → FPS counter visible top-right
- Toggle Show Speed → speed % appears next to / instead of FPS
- Set Debug Overlay = "Frame timings graph" → graph overlay renders mid-screen
- Reset Debug Overlay to "Off" → graph disappears within the next option re-poll
- Close + re-open settings → all 4 OSD toggles persist
- OSD preview pane on the OSD card live-updates as toggles change

### Regression
- `test_ppsspp_libretro_schema` all slots pass (now 10 instead of 9)
- `test_ppsspp_libretro_bindings` (Phase A) unchanged, still green
- Full `ctest -j4`: still 43/44 (HotkeyDefs::duckstation_completeness remains the pre-existing failure)
- PCSX2 OSD card still renders correctly (sanity: OsdPreview Q_PROPERTY addition is additive)

## Out of scope (explicit non-goals)

- Per-stat granular OSD toggles (Show CPU, Show GPU, Show VPS, etc.) — PPSSPP doesn't render them separately
- OSD position selector (corner) — PPSSPP has no position knob
- OSD scale slider — PPSSPP has no scale knob
- OSD bold-text toggle — PPSSPP has no font weight knob
- Frontend OSD overlay (RetroArch-style messages via `RETRO_MESSAGE_EXT`) — separate subsystem, would conflict
- New Recommended-card duplicates — OSD is advanced; not promoted
- "Developer" card exposing `bShowFrameProfiler` / `bShowDeveloperMenu` — separate future work
- Sub-tab restructuring of Video into a multi-tab card — rejected during brainstorming; flat card layout preferred
- Localization of the OSD option labels into PPSSPP's I18N system — libretro options are upstream-English-only by convention

## Files touched (final list)

### `ppsspp-libretro` fork
- `libretro/libretro_core_options.h` (+~80 LOC: 1 category entry + 4 option_def entries)
- `libretro/libretro.cpp` (+~40 LOC: 4 check_variable blocks)

### `RetroNest-Project` sibling
- `cpp/src/ui/settings/widgets/preview/osd_preview.h` (+5 LOC)
- `cpp/src/ui/settings/widgets/preview/osd_preview.cpp` (+~15 LOC)
- `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp` (+~50 LOC)
- `cpp/tests/test_ppsspp_libretro_schema.cpp` (+~25 LOC)

Total: 6 files modified, ~215 LOC added across two commits (one per repo).

## Deferred follow-ups

- After landing: revisit whether the `iDebugOverlay = "Control debug"` overlay is useful when libretro input goes through RetroPad (may render an empty controller diagram).
- If users request it: a `ppsspp_osd_show_status_indicators` super-toggle that mirrors PPSSPP's standalone "hide all status flags" shortcut.
- Localization pass for the OSD label/tooltip strings, coordinated with whatever Phase F audit produces.
