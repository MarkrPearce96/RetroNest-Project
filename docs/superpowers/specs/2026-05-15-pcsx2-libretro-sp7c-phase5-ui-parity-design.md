# SP7c Phase 5 — UI parity (preview widgets + layout sweep)

**Date:** 2026-05-15
**Sub-project:** SP7c (full settings parity between libretro PCSX2 and standalone PCSX2 dialogs)
**Phase:** 5 (close-out)
**Status:** Design — pending implementation plan

## Summary

Bring the libretro PCSX2 per-game settings dialog to **dialog-layout parity** with standalone PCSX2's dialog. Phase 4 (commits up to `7898eaebc` / `2556210`) closed core-options parity at 89/89 schema-fidelity; Phase 5 closes the *visible-layout* gap.

Two surfaces:
1. **Preview widgets** — wire the existing `AspectRatioPreview` (Recommended page) and `OsdPreview` (Graphics → On-Screen Display) into the libretro dialog, mirroring how `PCSX2Adapter` already does it.
2. **Structural layout sweep** — walk every sub-tab and reconcile group names, row ordering, and presence-of-row against standalone Pcsx2Adapter. Structural match, not byte-for-byte tooltip rewrite.

## Motivation

Phase 4 made every core option exist on the libretro side, but the dialog *layout* still drifts from standalone:
- Recommended page lacks the aspect-ratio preview pane that standalone has.
- Graphics → On-Screen Display lacks the live OSD preview pane.
- Per-sub-tab row order, group splits, and (in a few cases) presence-of-row diverge.

A user who knows the standalone PCSX2 dialog should find the libretro variant immediately familiar — same shape, same shelves, same navigation muscle-memory. Phase 5 closes that loop.

## Scope decisions (locked during brainstorm)

### Layout-parity tightness: **structural match**
- ✅ Same groups, same row order within each group, same sub-tab divisions.
- ✅ Same group-header text.
- ❌ Tooltip wording NOT required to match — keep libretro's existing text where already correct and clear (e.g., "Takes effect on next launch" qualifiers, macOS-specific Renderer notes).

### Hub card grid: **keep current 5 cards**
- ✅ Recommended · Emulation · Graphics · Audio · Memory Cards.
- ⏭ **Network & HDD** — DEV9/Ethernet/HDD knobs. Defer to future Phase 6 / new sub-project; requires new core options (substantial work comparable to Phase 2/3 sizing).
- ⏭ **Achievements** — RetroNest's rcheevos handles this host-side; duplicating in libretro would be redundant and confusing. Permanent skip.

### Media Capture sub-tab: **skip**
- Standalone Graphics has 6 sub-tabs; libretro Phase 4 covered 5. The 6th (Media Capture) is screenshot/recording config — RetroNest already handles host-side screenshot/recording. Skip with explanatory comment; no Phase 5 work.

## Architecture

**Two source surfaces touched, no infrastructure changes:**

1. `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h` — adds `previewSpec` override declaration.
2. `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — adds `previewSpec` implementation; schema rows reordered/regrouped.

**Data flow (already wired in the framework, currently dormant for libretro):**

```
GenericSettingsPage (per sub-tab render)
  ↓ calls
adapter->previewSpec(category, subcategory)   ← Phase 5 adds the libretro override
  ↓ returns
PreviewSpec { previewType: "aspect" | "osd", keyToProperty: {…} }
  ↓ GenericSettingsPage instantiates AspectRatioPreview / OsdPreview
  ↓ for each (schemaKey, widgetProperty) pair
  wirePreviewBinding(schemaKey ↔ widgetProperty)   ← live two-way binding via Qt meta-object
```

The preview-widget framework was built for standalone PCSX2 and is adapter-agnostic. Phase 5 just connects libretro to it.

**Risk surface:**
- Framework side: zero — preview widgets and binding logic already used by standalone.
- Schema side: a typo in a group name silently hides rows (the dialog only renders rows whose group it knows about). Mitigation: per-task smoke test.

## Task breakdown (3 tasks, mirrors Phase 4 cadence)

### Task 1 — Preview wiring

**Surface:** add `previewSpec` override in adapter `.h` (declaration) + `.cpp` (implementation, ~30 LOC).

**Concrete implementation:**
```cpp
PreviewSpec Pcsx2LibretroAdapter::previewSpec(const QString& category,
                                              const QString& subcategory) const {
    if (category == "Recommended" && subcategory.isEmpty()) {
        return {"aspect", {
            {"pcsx2_aspect_ratio", "aspectMode"},
        }};
    }
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        return {"osd", {
            {"pcsx2_osd_show_fps",                  "showFps"},
            {"pcsx2_osd_show_speed",                "showSpeed"},
            {"pcsx2_osd_show_vps",                  "showVps"},
            {"pcsx2_osd_show_resolution",           "showResolution"},
            {"pcsx2_osd_show_cpu",                  "showCpu"},
            {"pcsx2_osd_show_gpu",                  "showGpu"},
            {"pcsx2_osd_show_settings",             "showSettings"},
            {"pcsx2_osd_show_patches",              "showPatches"},
            {"pcsx2_osd_show_inputs",               "showInputs"},
            {"pcsx2_osd_show_frame_times",          "showFrameTimes"},
            {"pcsx2_osd_show_indicators",           "showIndicators"},
            {"pcsx2_osd_show_gs_stats",             "showGsStats"},
            {"pcsx2_osd_show_hardware_info",        "showHardwareInfo"},
            {"pcsx2_osd_show_version",              "showVersion"},
            {"pcsx2_osd_show_video_capture",        "showVideoCapture"},
            {"pcsx2_osd_show_input_rec",            "showInputRec"},
            {"pcsx2_osd_show_texture_replacements", "showTextureReplacements"},
            {"pcsx2_osd_messages_pos",              "messagesPos"},
            {"pcsx2_osd_performance_pos",           "performancePos"},
            {"pcsx2_osd_scale",                     "osdScale"},
        }};
    }
    return {};
}
```

**Why this just works:**
- All 21 mapped libretro core-option keys verified present in current schema (`pcsx2_aspect_ratio` from Phase 4 Task 2; `pcsx2_osd_*` from Phase 4 Task 6).
- `AspectRatioPreview` and `OsdPreview` widgets expose Qt properties (`aspectMode`, `showFps`, `osdScale`, …) named identically across adapters. `GenericSettingsPage::wirePreviewBinding` uses Qt's meta-object system to drive property updates.
- The value strings libretro writes (`"4:3"`, `"16:9"`, `"Auto 4:3/3:2"`, …) match what `AspectRatioPreview::aspectMode` expects, because both adapters use the same value list (verified during yesterday's Recommended-mirror commit at `9dc34f9`).

**Smoke test:**
1. Open Recommended → aspect preview pane appears on right; flip `Aspect Ratio` combo → preview reflows accordingly.
2. Open Graphics → On-Screen Display → OSD preview pane visible; toggle `Show FPS`, change `OSD Scale`, change `OSD Messages Position` → preview updates live.
3. Open every other sub-tab (Emulation, Audio, Memory Cards, Graphics → Display, etc.) → no preview pane appears (`previewSpec` returns `{}` for those).

**Acceptance:** smoke 1–3 all pass; no regression to existing sub-tab rendering.

### Task 2 — Top-level cards layout sweep

**Surface:** `pcsx2_libretro_adapter.cpp` schema rows for categories Recommended, Emulation, Audio, Memory Cards (no new code).

**Methodology** (per card):
1. Open `cpp/src/adapters/pcsx2_adapter.cpp::settingsSchema()` rows under the same `category`.
2. For each `(category, group)` pair, walk standalone rows top-to-bottom.
3. For each standalone row:
   - If libretro has it → confirm same position within group; if not, move it (reorder `s.append(opt(...))` calls).
   - If libretro lacks it AND the core-option key exists → add the row.
   - If libretro lacks it AND the core-option key doesn't exist → log as "needs new key", defer (out of Phase 5 scope; tracked as followup).
   - If standalone has a row that's architecturally inert in libretro (e.g., Audio Backend, Driver, Output Device) → skip with one-line comment.
4. Libretro rows that standalone doesn't have → keep them (Phase 1–4 extensions are fine).
5. Group-header text already matches across all four cards (verified during brainstorm).

**Per-card audit notes (preliminary, exact diffs land in plan stage):**
- **Recommended** — completed by commit `9dc34f9`. Task 2 **verifies** no regressions; no edits expected.
- **Emulation** — standalone has 17 rows across 3 groups; libretro Phase 1 has 15. Expected delta: 1–2 rows in System Settings; minor ordering tweaks.
- **Audio** — standalone has 11 rows (8 Configuration + 3 Controls); libretro Phase 2 has ~6. Several standalone rows are architecturally inert (Backend forced to Libretro per SP4; Driver/Device/OutputLatency handled host-side by RetroNest's SDL stack). Decision documented per-row.
- **Memory Cards** — standalone ~10 rows; libretro Phase 3 has 6. Likely add Slot 1/2 Filename rows if covered by existing core options; otherwise defer per "needs new key" rule.

**Smoke test:** open each top-level card → eyeball-compare against the standalone PCSX2 settings dialog for the same game → confirm same shape (groups, row order within groups, presence-of-rows for libretro-applicable knobs).

**Acceptance:** every group has the same set of rows that apply to the libretro variant, in the same order as standalone; per-row deviations from standalone have a one-line skip/keep comment.

### Task 3 — Graphics sub-tabs layout sweep

**Surface:** `pcsx2_libretro_adapter.cpp` `gopt(...)` schema rows for the Graphics card's sub-tabs.

**Per-sub-tab plan:**

| Sub-tab | Standalone rows | Libretro rows | Action |
|---|---|---|---|
| Display | ~32 | 32 | Audit row order + groupings within sub-tab; small reorder expected. |
| Rendering | ~14 | 14 | Audit. |
| Texture Replacement | 6 | 6 | Likely already aligned. |
| Post-Processing | 9 | 9 | Likely already aligned. |
| On-Screen Display | ~28 | 28 | Audit + verify Task 1's OSD preview unchanged after any reorder. |
| Media Capture | small | 0 | **Skip Phase 5.** RetroNest handles screenshot/recording host-side; duplicating Media Capture would be redundant. Marked as architecturally inert with one-line comment. |

**Special consideration for OSD sub-tab:** the preview-widget bindings declared in Task 1 reference 20 specific `pcsx2_osd_*` keys. Those keys must NOT be renamed during Task 3. Row positions within the sub-tab may shift; key identity is invariant. After Task 3 ships, re-run Task 1's OSD smoke (preview still updates live).

**Smoke test:** open Graphics → each sub-tab → eyeball-compare against standalone for the same game.

**Acceptance:** each existing sub-tab's row order matches standalone's structurally; Media Capture skip is commented; OSD preview unchanged in behavior.

## Out of scope (Phase 5)

- New core options (any "needs new key" finding during Tasks 2/3 is logged + deferred).
- Network & HDD card / sub-tab — separate future sub-project if needed.
- Achievements card — architecturally inert in libretro variant (RetroNest owns rcheevos).
- Media Capture sub-tab — architecturally inert (RetroNest owns screenshot/recording).
- Tooltip wording rewrites where libretro's text is already clear and correct.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Typo in a group name silently hides rows | Per-task smoke test before commit; eyeball every group renders the expected count. |
| Task 1 binding references a key that doesn't exist | Pre-checked all 21 keys before this spec; plan stage will re-grep as a TDD-style gate. |
| Reordering rows in OSD sub-tab breaks Task 1 preview | Task 3 acceptance includes "re-run Task 1 smoke" after edits. |
| AspectRatioPreview value-string mismatch (libretro key writes `"Auto 4:3/3:2"`, widget expects something else) | Verified during brainstorm: libretro `pcsx2_aspect_ratio` uses identical value strings to standalone `AspectRatio`. Smoke 1 confirms in practice. |

## Cadence

Each task ships as its own commit on RetroNest-Project `main`, with a smoke note in the commit body. Matches the Phase 4 Task 1–6 cadence the codebase already knows.

Estimated total: ~3 focused commits, ~200–400 LOC of schema-row edits, single file touched.

## Open questions

None — all scope decisions locked during brainstorm.

## Related artifacts

- `memory/sp7c_kickoff.md` — Phase 4 close memo (89/89 fidelity); five deferred Phase 5 followups tracked there.
- `memory/sp7c_followup_ui_parity.md` — original "UI parity" deferred-followup note (now superseded by this spec).
- `cpp/src/adapters/pcsx2_adapter.{h,cpp}` — reference for layout structure and preview-widget integration.
- `cpp/src/ui/settings/widgets/preview/{aspect_ratio_preview,osd_preview}.{h,cpp}` — adapter-agnostic preview widgets, no changes needed.
- `cpp/src/ui/settings/generic_settings_page.{h,cpp}` — renders the schema + dispatches PreviewSpec to widgets; no changes needed.
