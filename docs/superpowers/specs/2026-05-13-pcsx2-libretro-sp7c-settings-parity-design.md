# SP7c — full PCSX2 libretro settings parity

**Branch (pcsx2-libretro):** `retronest-libretro`
**Branch (RetroNest-Project):** `main`
**Predecessors:** SP7a (Resources path + region/fps), SP7b (3-knob core-options migration)
**Spec author:** Claude (2026-05-13)

## Summary

Bring `Pcsx2LibretroAdapter`'s settings dialog to full parity with the standalone PCSX2 settings dialog (`PCSX2Adapter` + `Pcsx2CategoryHub`), routing every row through `Storage::LibretroOption` instead of `Storage::Ini`. SP7b proved the end-to-end plumbing at the 3-knob scale; SP7c scales it to roughly **~90 core-option keys / ~104 host-adapter rows** across 5 of the standalone's 7 category cards (Recommended rows re-display existing keys, so adapter rows > core keys).

The user's directive: *"the next thing i want to do is add all the settings that are available for pcsx2-libretro onto emulator settings, i want the layout of the settings and category hubs to be identical to the current pcsx2 standalone"*.

This spec interprets "identical" as "every meaningful tweak available, with the same hub-grid metaphor". Rows where libretro storage is inert (host-managed paths, FFmpeg recording) are dropped rather than shown as disabled stubs. Two entire cards are dropped because their domains are architecturally incompatible with the libretro variant — see Scope decisions below.

## Goals

1. Every user-visible PCSX2 setting that *can* take effect inside the libretro variant is reachable from the per-emulator settings dialog.
2. Hub grid layout matches standalone (Recommended full-width headline + grid of detailed cards).
3. Schema fidelity between core (`kDefinitions[]`) and host (`settingsSchema()`) is mechanically verifiable — no eyeball-only review can scale to ~100 rows.
4. File organization keeps each per-category source file within ~300 LOC for human + LLM context comfort.
5. TDD-driven phasing — each card lands as its own logical commit set with live smoke verification.

## Non-goals (explicit out-of-scope)

- **Per-game settings overrides.** Distinct sub-project — libretro core options are global-per-core; per-game overrides need RetroNest-side data-model work.
- **Hotkeys.** Live in `HotkeySettingsDialog`, not the per-emulator dialog. Same for both standalone and libretro PCSX2.
- **Controller bindings.** Live in `controls.ini` / `controllerBindingDefsForType`. Already wired by SP5.
- **Audio backend selection.** libretro audio is hardwired through `audioBatchTrampoline` — `EmuCore/SPU2 / Backend` cannot redirect audio output.
- **BIOS path picker.** Host sets via `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY`. A user-tweakable path would be silently ignored by `Settings::InitializeDefaults`.
- **Memory card filename text fields.** Host owns paths via `save_dir + "/memcards"`. Slot enables ARE in scope; per-slot filename strings are not.
- **FFmpeg media capture.** 18 rows in the standalone Graphics → Media Capture sub-tab. pcsx2-libretro has no `Host::` hooks for FFmpeg negotiation; libretro's own capture API would be the path if ever desired (out of scope).
- **Network & HDD card** (14 rows). PCSX2 needs direct OS network access via pcap/tap interfaces to emulate the PS2's Ethernet adapter and HDD. A libretro core running inside RetroNest's process can't acquire those interfaces. Even the "HDD enabled" toggle would be inert.
- **Achievements card** (14 rows). RetroNest's host-side rcheevos owns the achievements flow in libretro mode (see SP6.5 Task 4.7 — Scratchpad SET_MEMORY_MAPS + `Pcsx2LibretroAdapter::raConsoleId`). A core-side toggle would be confusing at best, conflicting at worst. SP7b's spec explicitly excluded this for the same reason.
- **Per-game widescreen / no-interlacing patch database UI.** These rely on `gamedb.yaml` + `patches.zip`; toggling exposure is in scope, but the patch-management UI itself is not.

## Scope decisions

### Decision 1: rows that don't apply to libretro variant — DROP

Three options were considered:
- **(A) Drop entirely.** Cleanest UX. Risks "incomplete parity" pushback.
- **(B) Mirror everything, disable inert rows with tooltip.** Most literal "identical" reading.
- **(C) Mirror everything as editable, document inertness in tooltip.** User-friendly but technically misleading (tweaks would be silently ignored).

**Decision: (A) Drop.** Reasoning: the standalone hub's own header comment explicitly says *"minus categories RetroNest manages or that are per-game-only upstream — see PCSX2Adapter::settingsSchema for the full skip list."* — so the standalone itself already drops rows. "Identical" doesn't mean "every standalone row regardless of effect"; it means "the same UX shape". A row whose value is silently overwritten by `Settings::InitializeDefaults` is worse than no row at all.

**Risk flag:** This means the hub renders 5 cards (Recommended + Emulation + Graphics + Audio + Memory Cards) instead of the standalone's 7. The user should redirect if they explicitly want the 7-card grid preserved.

### Decision 2: sub-tabs — mirror standalone

The Graphics card uses `subcategory` to render sub-tabs in standalone (Display, Rendering, Texture Replacement, Post-Processing, OSD — Media Capture is dropped per Decision 1). The `GenericSettingsPage` infrastructure auto-detects sub-tabs from distinct `subcategory` values (`generic_settings_page.cpp:124-126`); the `hasSubTabs` flag only controls the L1/R1 hint chrome on the dialog.

**Decision: Mirror standalone exactly.** `Pcsx2LibretroSettingsDialog::onCategoryActivated` flips `hasSubTabs = (category == "Graphics")` — one-line change from the current `false`-unconditional. All other categories remain single-page.

### Decision 3: schema-file organization — split by category

Three options were considered:
- **(A) Keep monolithic.** Hand-written `kDefinitions[]` for ~90 entries ≈ 3500+ LOC in one file. Untenable.
- **(B) Split by category.** Per-card source pair, aggregator concatenates at startup.
- **(C) Codegen from shared YAML/JSON.** Most robust against drift, but ~2 days of up-front tooling work.

**Decision: (B) Split.** Each category sub-file owns its slice of definitions, its parse helper, and its apply-defaults helper. Avoids codegen tooling cost. Each file stays around 200–400 LOC — comfortable for both humans and LLM context. Aggregator pattern works because the libretro `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2` callback takes a single pointer-to-array; we build a `std::vector<retro_core_option_v2_definition>` at startup from per-category sub-vectors, then pass `.data()` to the callback. The vector's lifetime spans the core's lifetime (static storage).

### Decision 4: schema-fidelity verification — Python diff script

Three options:
- **(A) Trust two-stage review per task.** Worked for SP7b at 3 knobs; doesn't scale to ~100 rows.
- **(B) Runtime / build-time diff script.** Compare core's declared strings to host adapter's stored-value strings; assert byte-for-byte match.
- **(C) Codegen from shared source.** Same up-front cost as Decision 3(C).

**Decision: (B) Python diff script** at `pcsx2-libretro/tools/check_schema_fidelity.py`. ~50 LOC. Reads CoreOptions*.cpp string literals from the core repo and `pcsx2_libretro_adapter.cpp` string literals from the RetroNest repo (path passed as argv); asserts every `(key, value)` pair declared by the host adapter appears in the core's definitions list, and every key in the host appears in the core. Exit non-zero on drift. Runs as `make check` in CMake and as a pre-commit hook.

Rationale: `OptionsStore::load` silently drops unrecognized values from `options.json` when they don't match the core's declared list. Drift = silent user-setting wipe. The fidelity check catches this before any user can hit it.

### Decision 5: hotkeys — skip (confirmed)

PCSX2 standalone has hotkey settings (key combos for save state / fast forward / etc.), but they live in a separate `HotkeySettingsDialog`, not in the 7-card hub. SP7c is bounded to the hub. Hotkeys are out of scope for both the standalone and libretro variants of *this* dialog.

## Architecture

### Final hub layout (5 cards)

```
┌──────────────────────────────────────────────────────────┐
│ Recommended (full-width headline)                        │
│ 14 most-tweaked settings, cross-referencing detailed cards│
└──────────────────────────────────────────────────────────┘
┌──────────────┬──────────────────┬──────────────────────┐
│ Emulation    │ Graphics         │ Audio                 │
│ ~17 knobs    │ ~62 knobs, 5 sub-│ ~6 knobs              │
│              │ tabs              │                       │
├──────────────┼──────────────────┼──────────────────────┤
│ Memory Cards │                  │                       │
│ ~5 knobs     │                  │                       │
└──────────────┴──────────────────┴──────────────────────┘
```

Grid follows `Pcsx2CategoryHub`'s template: Recommended is row-0 full-width, detailed cards are row-1+ in 3-column grid. Memory Cards alone on row 2 column 0.

### Total exposed knob count

| Card | Rows |
|---|---|
| Recommended (re-display, host-only) | 14 |
| Emulation | 17 |
| Graphics | 62 (5 sub-tabs) |
| Audio | 6 |
| Memory Cards | 5 |
| **Total host adapter rows** | **~104** |
| **Total core option keys** | **~90** (Recommended re-displays existing keys; some rows like FXAA are checkboxes that map to one key shared between Recommended and a detailed card) |

### Core side — pcsx2-libretro

New / changed files:

```
pcsx2-libretro/
├─ CoreOptions.h                      — public interface (unchanged shape):
│                                       struct Resolved { struct Emulation{...} emulation;
│                                                          struct Graphics{...}   graphics;
│                                                          struct Audio{...}      audio;
│                                                          struct MemoryCards{...} memory_cards; };
│                                       bool EmitCoreOptionsV2(retro_environment_t);
│                                       Resolved ReadResolved(retro_environment_t);
│
├─ CoreOptions.cpp                    — thin aggregator (≤120 LOC):
│                                       BuildDefinitions() concatenates sub-vectors at first call,
│                                       caches the result, returns a stable pointer.
│                                       EmitCoreOptionsV2 dispatches BuildDefinitions().data() to
│                                       RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2.
│                                       ReadResolved calls ParseEmulation, ParseGraphics,
│                                       ParseAudio, ParseMemoryCards in turn.
│
├─ CoreOptionsEmulation.{h,cpp}       — kEmulationDefinitions[] (~17 entries),
│                                       ParseEmulation(cb, Resolved::Emulation&),
│                                       ApplyEmulationDefaults(SettingsInterface&, const Resolved::Emulation&).
│
├─ CoreOptionsGraphics.{h,cpp}        — same shape, ~62 entries.
│
├─ CoreOptionsAudio.{h,cpp}           — same shape, ~6 entries.
│
├─ CoreOptionsMemoryCards.{h,cpp}     — same shape, ~5 entries.
│
├─ Settings.cpp                       — InitializeDefaults grows: after existing setup,
│                                       calls each Apply{Emulation,Graphics,Audio,MemoryCards}Defaults
│                                       in turn. Each apply helper is a flat sequence of
│                                       g_si.Set{Bool,Int,Float,String}Value calls, one per knob.
│
├─ tools/check_schema_fidelity.py     — ~50 LOC; argv: --core <path-to-CoreOptions*.cpp glob>
│                                                    --host <path-to-pcsx2_libretro_adapter.cpp>
│
└─ tools/test_core_options.cpp        — data-driven rewrite. Tests loop over
                                        BuildDefinitions() asserting structure (every key has
                                        ≥1 value, default value is in values list, etc.) plus
                                        a small number of hand-written round-trip tests
                                        (set env-var → ReadResolved → assert field value).
```

`Resolved`'s nested structure mirrors the category cards. Example shape:

```cpp
struct Resolved {
    struct Emulation {
        int  ee_cycle_rate;          // -3..+3
        int  ee_cycle_skip;          // 0..3
        bool vu_thread;              // MTVU
        bool thread_pinning;
        bool cheats;
        bool host_fs;
        bool cdvd_precache;
        bool fast_boot;
        bool fast_boot_fast_forward;
        // ... 17 total
    } emulation;

    struct Graphics {
        struct Display { /* ~17 fields */ } display;
        struct Rendering { /* ~7 fields */ } rendering;
        struct TextureReplacement { /* ~6 fields */ } texture_replacement;
        struct PostProcessing { /* ~9 fields */ } post_processing;
        struct OSD { /* ~23 fields */ } osd;
    } graphics;

    struct Audio { /* ~6 fields */ } audio;

    struct MemoryCards {
        bool slot1_enable;
        bool slot2_enable;
        bool multitap1_slot2_enable;
        bool multitap1_slot3_enable;
        bool multitap1_slot4_enable;
    } memory_cards;
};
```

### Host side — RetroNest-Project

Changed files:

```
cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
   — settingsSchema() grows from 3 rows → ~104 rows.
   — Organized by source comments matching the standalone PCSX2 adapter's
     section structure for review-friendliness. Helper lambdas (similar to
     the existing `opt(...)` in the current SP7b version) factor out
     boilerplate per widget type.
   — Recommended rows are 14 SettingDef entries that reference the same
     `key` strings as detailed-card rows. Multiple SettingDef rows pointing
     at one core-option key is supported (writes go to the same options.json
     entry; reads pull from the same source). No new core option keys.

cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp
   — 1 card → 5 cards. Grid pattern lifted from Pcsx2CategoryHub:
     row 0: Recommended (full-width headline)
     row 1: Emulation, Graphics, Audio
     row 2: Memory Cards (alone)
   — Card icons & blurbs match standalone exactly.

cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp
   — One line: `const bool hasSubTabs = (category == "Graphics");`
   — pushPage(page, hasSubTabs); (same pattern as duckstation / dolphin /
     pcsx2 / ppsspp dialogs).
```

The category hub's `countSettings(category)` helper continues to work as-is — counts SettingDef rows whose `category` field matches.

## Phase plan

Each phase = one self-contained landing with live smoke gate. Phases run sequentially.

### Phase 0 — Foundation refactor (no new user-visible knobs)

**Goal:** prove the split + aggregator + fidelity-check pattern works at the 3-knob scale before scaling to ~90 entries.

Tasks:
1. Create `CoreOptionsEmulation.{h,cpp}` and migrate SP7b's 3 knobs (renderer, mtvu, fast_boot) into it. Note: renderer/mtvu/fast_boot become `kEmulationDefinitions[]` entries; the SP7b "all under Recommended/Emulation" grouping is preserved by the adapter's category/group fields.
2. Refactor `CoreOptions.cpp` into the thin aggregator pattern. `BuildDefinitions()` returns a stable pointer to a heap-allocated concatenated vector. `EmitCoreOptionsV2` and `ReadResolved` dispatch into it.
3. Update `Resolved` to the nested-struct shape (currently flat with 3 fields). Migrate `Settings.cpp` apply sites to read `options->emulation.{vu_thread,fast_boot}` etc.
4. Write `tools/check_schema_fidelity.py`. At this stage it asserts on the 3-row baseline.
5. Rewrite `tools/test_core_options.cpp` as data-driven: one test loops over `BuildDefinitions()` asserting structure; another loops over the host adapter's 3 rows asserting key match. Keep the hand-written round-trip tests for the 3 knobs.

Smoke gate: SP7b's live-smoke flow on R&C 2 reproduces exactly. Three knobs still toggle as before.

Estimated commits: 4–6 (one per task).

### Phase 1 — Emulation card — CODE-SHIPPED 2026-05-13 (awaiting user smoke gate)

**Goal:** expose the remaining ~14 Emulation knobs (3 already done in Phase 0).

Tasks per knob group (3 sub-groups in standalone Emulation card):
- **Speed Control (3 knobs):** NominalScalar, TurboScalar, SlomoScalar.
- **System Settings (7 new knobs):** EECycleRate, EECycleSkip, EnableThreadPinning, EnableCheats, HostFs, CdvdPrecache, EnableFastBootFastForward. (vuThread and EnableFastBoot already shipped in Phase 0.)
- **Frame Pacing (5 knobs):** VsyncQueueSize, SyncToHostRefreshRate, VsyncEnable, UseVSyncForTiming, SkipDuplicateFrames.

Workflow per knob: add to `kEmulationDefinitions[]`, add field to `Resolved::Emulation`, add parse line to `ParseEmulation`, add apply line to `ApplyEmulationDefaults`, add SettingDef row to host adapter, run schema fidelity check, run unit test.

Smoke gate: tweak EECycleRate to -1 on R&C 2 + observe behavior. Toggle EnableCheats and verify no regression. Note: some Emulation knobs (e.g. SyncToHostRefreshRate) may interact with libretro's frame pacing; document any quirks in code comments.

**Delivery (2026-05-13):** pcsx2-libretro `retronest-libretro` HEAD `6474dd0c6` (6 commits `31e07fedf` → `6474dd0c6`); RetroNest-Project `main` HEAD `0be1407` (3 host commits). 15 knobs across 3 sub-groups; schema-fidelity 18 core keys / 18 host keys byte-for-byte. Plan at `docs/superpowers/plans/2026-05-13-pcsx2-libretro-sp7c-phase1-emulation.md`. **Pending: user runs Task 7 manual UI smoke before Phase 1 flips to ✅ shipped.**

### Phase 2 — Audio card

**Goal:** expose the 6 Audio knobs that apply to libretro.

Knobs: ExpansionMode, SyncMode, BufferMS, OutputLatencyMS, OutputLatencyMinimal, StandardVolume, FastForwardVolume, OutputMuted. (Note: that's 8 candidates from the inventory; final count may be lower after verifying each value actually flows through `LibretroAudioStream`. BufferMS and OutputLatencyMS in particular may not have effect since libretro audio bypasses Cubeb.)

**Verify-during-implementation:** for each Audio knob, confirm the INI key is read by something other than the Cubeb backend. Knobs whose only consumer is `SPU2::Initialize`'s Cubeb path get dropped (silently inert).

Smoke gate: StandardVolume slider visible + value persists in options.json.

Estimated commits: 2–3.

### Phase 3 — Memory Cards

**Goal:** expose 5 Memory Cards knobs.

Knobs: Slot1_Enable, Slot2_Enable, Multitap1_Slot2_Enable, Multitap1_Slot3_Enable, Multitap1_Slot4_Enable.

Slot1 already enabled by SP6's existing `Settings.cpp` setup; this phase makes the user-toggle explicit and adds slot 2 / multitap. The path side (`save_dir + "/memcards/Mcd00N.ps2"`) stays hardcoded — the toggle just gates `g_si.SetBoolValue("MemoryCards", "Slot1_Enable", ...)`.

Smoke gate: toggle Slot 2 to enabled → re-launch → verify `Mcd002.ps2` appears under `~/Documents/RetroNest/emulators/pcsx2-libretro/ps2/memcards/`.

Estimated commits: 2.

### Phase 4 — Graphics card (biggest)

**Goal:** expose ~62 Graphics knobs across 5 sub-tabs.

Sub-tab phases (each its own commit batch):
1. **Display (17 knobs)** — Renderer, AspectRatio, deinterlace_mode, StretchY, Crop{Left,Top,Right,Bottom}, pcrtc_antiblur, IntegerScaling, pcrtc_offsets, disable_interlace_offset, pcrtc_overscan, linear_present_mode, EnableWideScreenPatches, EnableNoInterlacingPatches, FMVAspectRatioSwitch. Renderer already in Phase 0.
2. **Rendering (7 knobs)** — upscale_multiplier, filter, TriFilter, MaxAnisotropy, dithering_ps2, accurate_blending_unit, hw_mipmap.
3. **Texture Replacement (6 knobs)** — LoadTextureReplacements, DumpReplaceableTextures, LoadTextureReplacementsAsync, DumpReplaceableMipmaps, PrecacheTextureReplacements, DumpTexturesWithFMVActive.
4. **Post-Processing (9 knobs)** — CASMode, CASSharpness, fxaa, TVShader, ShadeBoost, ShadeBoost_{Brightness,Contrast,Saturation,Gamma}.
5. **OSD (23 knobs)** — OsdScale, OsdMargin, OsdMessagesPos, OsdPerformancePos, OsdBoldText, OsdShow{Speed,FPS,VPS,Resolution,GSStats,CPU,GPU,Indicators,FrameTimes,HardwareInfo,Version,Settings,Inputs,VideoCapture,InputRec,TextureReplacements}, OsdshowPatches, WarnAboutUnsafeSettings.

Sub-tab navigation: passing `subcategory` strings ("Display", "Rendering", "Texture Replacement", "Post-Processing", "OSD") on each SettingDef row + `hasSubTabs=true` flip in the dialog completes the wiring.

Smoke gate per sub-tab: tweak one knob, observe effect:
- Display: AspectRatio 4:3 vs Stretch → visible widescreen change on R&C 2.
- Rendering: upscale_multiplier 1x → 4x → visible resolution change.
- Texture Replacement: enable LoadTextureReplacements → look for log line.
- Post-Processing: FXAA on → visible smoothing.
- OSD: OsdPerformancePos non-zero → FPS counter visible.

Estimated commits: 8–12 (one per sub-tab + cross-side adapter / dialog wiring).

### Phase 5 — Recommended card

**Goal:** add 14 host-adapter rows that cross-reference existing core-option keys.

Recommended is purely a host-side curated view. No new core-option keys. The 14 rows are the standalone's Recommended-card row labels mapped onto the existing core-option keys declared by Phases 1–4. Some renaming may be appropriate where Recommended has a different user-facing label than the detailed-card row.

Smoke gate: Recommended card renders with 14 rows; tweaking a row updates options.json under the same key the detailed-card row would update.

Estimated commits: 1–2.

### Phase 6 — Finalization + memory close-out

**Goal:** comprehensive live smoke + session memory.

Tasks:
1. Live-smoke session covering tweaks in each of the 5 cards on R&C 2 (NTSC).
2. DB-flip DBZ TT2 → libretro, repeat key tweaks (PAL coverage).
3. Schema-fidelity Python script wired into CI (or a CMake `check_schema_fidelity` target).
4. Write `session_handoff_sp7c_shipped.md` memory; supersede `sp7c_kickoff.md`.
5. Final-review polish pass (look for placeholder tooltips, typos, missing dependsOn fields).

Estimated commits: 2–4.

## Testing strategy

### Unit tests (core side)

`pcsx2-libretro/tools/test_core_options.cpp` becomes data-driven:

1. **Structural assertions over `BuildDefinitions()`:**
   - Every key has at least one entry in `values[]`.
   - `default_value` appears in `values[]`.
   - No two definitions share the same key.
   - Total count matches the sum of per-category sub-vector sizes.

2. **Round-trip assertions** (a handful, not one per knob):
   - Hand-pick 4–6 representative knobs (one per category, one sub-tab). For each:
     - Set env var → call `ReadResolved` → assert the matching `Resolved::*::field` reflects it.
     - Set unknown enum string → assert fallback to default + WARN log.

3. **Per-category parse helpers:**
   - Each `ParseEmulation`, `ParseGraphics`, etc. is independently testable.

Test compile gate: `clang++ -std=c++20 -DSP7C_TEST_CORE_OPTIONS_ONLY ...` (same standalone-compile pattern as SP7a/SP7b — no PCSX2 link).

### Schema-fidelity script (Python)

`pcsx2-libretro/tools/check_schema_fidelity.py`:

```
Usage: check_schema_fidelity.py
  --core <path-to-pcsx2-libretro-CoreOptions-glob>
  --host <path-to-RetroNest-pcsx2_libretro_adapter.cpp>

Behavior:
  1. Regex-extract { key, [values...] } tuples from core's CoreOptions*.cpp.
  2. Regex-extract { key, options=[{label,value},...] } from host adapter's
     settingsSchema().
  3. For each host row: assert key ∈ core keys AND every host value ∈ core's
     values for that key.
  4. For each core key: assert ∃ host row referencing it (allowed: multiple
     host rows per core key, e.g. Recommended re-display).
  5. Exit 0 on success, 1 with diff report on failure.
```

Wired into CMake as `make check_schema_fidelity`. Run as pre-commit hook.

### Live smoke

Per-phase smoke gate as listed in the phase plan above. Per-phase smoke is more important than per-knob smoke; "every knob persists in options.json" is asserted by the schema-fidelity + unit test combo. Per-phase smoke verifies the knob actually flows into PCSX2's behavior.

Two-disc coverage: R&C 2 (NTSC, default-routed) + DBZ Budokai Tenkaichi 2 EU (PAL, DB-flipped to libretro for the session).

## Risks and open questions

1. **Audio knob applicability uncertainty.** Some Audio settings (BufferMS, OutputLatencyMS) read from the SPU2 module's Cubeb backend code. libretro variant bypasses Cubeb (audio routed through `audioBatchTrampoline`). Phase 2 has explicit verify-during-implementation step to drop knobs whose values are unread. May reduce 6 → 3-4 final knobs.

2. **OSD knob count is high (23).** All probably honor the same INI consumers that the standalone uses (PCSX2's `ImGuiManager::DrawOSD`). Should mirror cleanly but Phase 4 OSD sub-tab will be the longest single batch.

3. **Hub deviation from "identical".** 5 cards instead of 7. **User redirect possible at spec-review gate.** If the user wants Network & HDD + Achievements preserved as disabled cards, swap to Decision 1 Option (B) — add `SettingDef::Storage::Disabled` semantics or a new "non-editable" flag.

4. **Aggregator pointer stability.** `BuildDefinitions()` returns a pointer that must remain valid for the core's lifetime. Use a function-local `static std::vector<retro_core_option_v2_definition>` — initialized once on first call, lives forever. Make sure the value-arrays inside each entry are also `static`-storage (they're string literals, so this works naturally — but the `values[]` aggregate-init arrays need careful lifetime management; see implementation plan).

5. **Apply-defaults ordering.** Some PCSX2 settings have inter-dependencies (e.g. `TriFilter` gates which `filter` values make sense). The apply order matters if a later write resets an earlier one. We mirror `Settings.cpp`'s existing per-call ordering: defaults first, options second.

6. **Recommended-card label drift.** Standalone's Recommended labels are sometimes slightly different from the detailed-card labels (e.g. "Multithreaded VU1 (MTVU)" vs "Enable Multithreaded VU1"). Phase 5 should match standalone's Recommended labels exactly even when they differ from the detailed-card label.

## Files touched (estimated)

### pcsx2-libretro
- New: `CoreOptionsEmulation.{h,cpp}`, `CoreOptionsGraphics.{h,cpp}`, `CoreOptionsAudio.{h,cpp}`, `CoreOptionsMemoryCards.{h,cpp}` — 8 files, ~1500 LOC total.
- Rewritten: `CoreOptions.{h,cpp}` (thin aggregator) — ~150 LOC.
- New: `tools/check_schema_fidelity.py` — ~50 LOC.
- Rewritten: `tools/test_core_options.cpp` — ~300 LOC (data-driven + hand-picked round-trip).
- Modified: `Settings.cpp` — ~+200 LOC across 4 new ApplyXxxDefaults helpers.
- Modified: `CMakeLists.txt` — +4 lines (register new .cpp files).

### RetroNest-Project
- Modified: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — 3 rows → ~104 rows. ~+1500 LOC.
- Modified: `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` — 1 card → 5 cards. ~+40 LOC.
- Modified: `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp` — 1-line hasSubTabs flip.
- New: `docs/superpowers/specs/2026-05-13-pcsx2-libretro-sp7c-settings-parity-design.md` (this file).
- New: `docs/superpowers/plans/2026-05-13-pcsx2-libretro-sp7c-settings-parity.md` (next step).

No upstream PCSX2 file edits.

## Success criteria

1. Every knob declared in the spec is reachable from `~/Documents/RetroNest/` → emulator settings → PCSX2 (libretro) → respective card.
2. Every knob tweak persists in `~/Documents/RetroNest/emulators/libretro/pcsx2/options.json`.
3. Schema-fidelity script passes (exit 0) at every commit.
4. Live smoke verifies one tweak per card propagates to PCSX2 behavior.
5. SP7b smoke flow (3 SP7b knobs) reproduces unchanged after Phase 0.
6. No regression in SP4 (audio), SP5 (input), SP6 (memcard/RA), SP6.5 (save state / cold resume) flows.
7. Qt teardown SIGABRT pre-existing behavior unchanged (tracked separately, see `[[session_handoff_sp7a_shipped]]`).

## Dependencies / interactions

- **SP5.5** (analog + rumble) — independent. Doesn't touch settings code.
- **SP8** (RetroNest adapter rewrite) — likely arrives after SP7c so libretro variant is feature-complete first.
- **No interaction with rcheevos / SP6.5 Task 4.7** — Achievements card is dropped.
- **No interaction with audio pacing (SP4.x)** — Audio knobs in scope don't include backend / sync mode that would change drain rates.

## What this spec does NOT cover

- Implementation plan — that's `writing-plans`' job after spec approval.
- Per-knob INI keys and PCSX2-side consumer code paths — those are derived during implementation by reading `Settings.cpp` and PCSX2's `EmuCore/` modules. The inventory in the spec author's working notes lists them but the spec keeps to logical-knob granularity.
- Widget choice per knob (Combo / Checkbox / Slider) — `pcsx2_adapter.cpp` already encodes these; the libretro adapter inherits the same choices verbatim per row.
