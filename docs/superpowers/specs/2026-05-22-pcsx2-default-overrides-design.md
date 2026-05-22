# PCSX2 default overrides — first consumer of Phase E

**Status:** Approved for implementation
**Date:** 2026-05-22
**Predecessors:** PPSSPP Phase E (`docs/superpowers/specs/2026-05-22-ppsspp-phase-e-design.md`) — built the schema-as-source-of-truth mechanism this work uses.
**Scope:** Minimal — three default-value edits only. No drift safety net (deferred).

## Goal

Override three PCSX2 libretro defaults to better match RetroNest's target audience (modern Macs, widescreen displays, auto-downloaded widescreen patches):

| Key | Upstream default | New default | Rationale |
|---|---|---|---|
| `pcsx2_upscale_multiplier` | `"1"` (1x Native) | `"2"` (2x, ~720px HD) | Crisp on modern displays. Conservative enough that PS2-heavy games stay smooth on Apple Silicon and Intel-via-Rosetta. |
| `pcsx2_aspect_ratio` | `"4:3"` | `"16:9"` | Most PS2 games and modern displays are widescreen. Combined with widescreen patches (next row), gives true 16:9 rendering. |
| `pcsx2_enable_widescreen_patches` | `"disabled"` | `"enabled"` | Safe to enable because RetroNest auto-downloads patch files. Gives true 16:9 rendering of game viewports instead of stretched 4:3. |

## Background

PPSSPP Phase E (commits `6fd46ac` through `d3620de`) built a generic mechanism where each libretro adapter's `SettingDef.defaultValue` is the single source of truth for first-launch option values. The plumbing flows through:

- `OptionsStore::pickInitialValue` — three-tier priority chain (existing → schema → upstream)
- `CoreRuntime::StartConfig.schemaOptionDefaults`
- `GameSession::startLibretro` extracts the map from any adapter's `settingsSchema()`
- `LibretroAdapter::libretroOptionsStore` synthesizes `CoreOption.defaultValue` from `SettingDef.defaultValue` for the Settings-UI-open-without-running-game path

Phase E was designed to be **per-adapter opt-in by simply editing the schema**. This is the first consumer of that abstraction outside PPSSPP itself.

## In scope

Six line-edits in one file: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`. Each of the three keys appears twice — once in the canonical PCSX2 settings card (Video / Visual Quality), once in the Recommended-card duplicate. Both copies of each row must change to the same new value, mirroring the dupe-consistency invariant enforced for PPSSPP.

Approximate line numbers (verify at implementation time):

| Key | New default | Lines |
|---|---|---|
| `pcsx2_upscale_multiplier` | `"2"` | 240 (Recommended), 895 (canonical) |
| `pcsx2_aspect_ratio` | `"16:9"` | 258 (Recommended), 685 (canonical) |
| `pcsx2_enable_widescreen_patches` | `"enabled"` | 271 (Recommended), 828 (canonical) |

## Out of scope

- **Drift safety net.** No `test_pcsx2_libretro_schema.cpp` file, no upstream-defaults fixture, no `intentionalOverrides()` allowlist. PCSX2 has ~80 libretro options, making the full Phase E-style fixture roughly twice the manual effort of PPSSPP's. Deferred to a future migration if and when upstream-default drift becomes a concern.
- **Dupe-consistency test.** Even the smaller subset of the Phase E safety net (asserting the Recommended-card duplicate matches its canonical row) is not added here. Risk is low because the three changes are made in pairs and reviewed together.
- **Tooltip text changes.** The description strings on each row stay as-is. They describe the option, not the default, and remain accurate.
- **`recommendedValue` field changes.** Empty today and stays empty — the description bar auto-falls-back to `defaultValue` (verified in PPSSPP Phase E).
- **Setup-wizard integration.** PCSX2 doesn't implement `resolutionOptions()` or `aspectRatioOptions()`, so it never appears in the wizard's resolution or aspect-ratio pages. The wizard path (which writes to INI via `install_controller.cpp:117`) is a separate, unrelated mechanism. No conflict. Future work to surface PCSX2 in the wizard would be a separate, larger refactor.

## Architecture

No architecture changes. This consumes the Phase E mechanism unchanged.

After the six line-edits land:

1. Fresh install (no `~/Documents/RetroNest/emulators/libretro/pcsx2/options.json`):
   - User opens RetroNest Settings → PS2 → Visual Quality, OR launches a PS2 game.
   - `LibretroAdapter::libretroOptionsStore()` (Settings path) OR `GameSession::startLibretro` → `CoreRuntime` → `OptionsStore::load` (game-launch path) walks PCSX2's `settingsSchema()`.
   - For each `Storage::LibretroOption` row, the new `SettingDef.defaultValue` flows through the priority chain. With no `options.json` and no upstream-distinct value to fall back to, the schema value is what gets persisted.
   - `options.json` written with `"pcsx2_upscale_multiplier": "2"`, `"pcsx2_aspect_ratio": "16:9"`, `"pcsx2_enable_widescreen_patches": "enabled"`.
2. Existing user (with prior `options.json`):
   - On next launch, `OptionsStore::load` finds existing values and uses them. The schema default is ignored for keys already in `options.json`. No regression.
3. UI rendering:
   - Settings dropdown highlight tracks `SettingDef.defaultValue` automatically (already verified during PPSSPP Phase E).
   - "Recommended:" badge below the description bar auto-translates the new defaultValue to its display label via the same combo-value-to-label lookup used for PPSSPP's resolution.
   - Aspect-ratio preview pane watches `pcsx2_aspect_ratio` (via `previewSpec` mapping at `pcsx2_libretro_adapter.cpp:1449`) and will render 16:9 by default.

## Verification

### Build

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build
```

Expected: clean build, no errors. No tests added or modified, so existing test target results stay unchanged.

### Manual

1. Delete existing PCSX2 options.json:
   ```sh
   rm -f ~/Documents/RetroNest/emulators/libretro/pcsx2/options.json
   ```
2. Launch RetroNest, navigate to Settings → PS2 → Visual Quality.
3. Confirm Internal Resolution dropdown shows **"2x Native (~720px/HD)"**.
4. Confirm Aspect Ratio shows **"16:9 (Widescreen)"**.
5. Confirm Apply Widescreen Patches shows **"Enabled"**.
6. Inspect `~/Documents/RetroNest/emulators/libretro/pcsx2/options.json` and confirm all three keys carry the new values.
7. Launch a PS2 game (e.g. Final Fantasy X). Confirm:
   - Sharp rendering at 2x (not pixelated 1x).
   - 16:9 viewport (no horizontal black bars on 4:3-original games if widescreen patches downloaded; stretched 4:3 fill otherwise).
8. Regression check: quit, edit options.json to set `pcsx2_upscale_multiplier` back to `"1"`, relaunch, confirm dropdown shows "1x Native (PS2)" again — proves existing-value-wins priority survives.

## Estimated scope

- Six 1-line edits in one file.
- One commit.
- Time: ~10 minutes implementation + ~5 minutes manual verification.

## Migration story for future adapters

This spec establishes the lightweight precedent: **for any libretro adapter whose schema is already wired into Phase E (mgba is the only remaining one), changing defaults is now just editing the schema.** No design doc, no plan, no infrastructure — just edit the lines, commit, verify. The drift safety net is an independent decision per adapter.

Recommended sequence for the next adapter (mgba):
1. Skim its schema. Identify any defaults that are upstream-conservative-for-low-end-hardware.
2. Edit `SettingDef.defaultValue` for each. Commit.
3. (Optional, separate work) Add a `test_mgba_libretro_schema.cpp` mirroring PPSSPP's drift test if upstream-default churn becomes a concern.
