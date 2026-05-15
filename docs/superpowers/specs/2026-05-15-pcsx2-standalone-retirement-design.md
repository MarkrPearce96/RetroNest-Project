# SP8 — Retire PCSX2 standalone path (design spec)

**Date:** 2026-05-15
**Status:** Approved (kickoff brainstorm)
**Supersedes:** N/A — closes the original "unify the rendering pipeline" goal of the libretro port arc.
**Predecessor:** SP7c Phase 5 (settings UI parity).

## Goal

Remove every trace of the launched-binary PCSX2 standalone path from RetroNest. After SP8, "PCSX2" in RetroNest means **only** the libretro core (`Pcsx2LibretroAdapter`).

## Why now

The libretro path is at feature parity with what standalone offered:

- SP3 / SP3.5 / SP3.6 / Phase 5 polish — HW render bridge, image fidelity matches standalone.
- SP4 / SP4.x — audio output with full sync correctness.
- SP5 / SP5.5 — input (digital + host-side analog + rumble).
- SP6 / SP6.5 — memcards, memory maps, save state in-session + cold-resume, scratchpad descriptor, atomic reset.
- SP7a / SP7b / SP7c — Resources path, region detection, core options, full schema + UI parity (89/89 knobs).
- EE cross-cycle freeze workaround (`9ee456d30`).

The standalone path is now strictly inferior in every dimension and adds maintenance cost (two adapters, two settings dialogs, two manifest entries, three duplicate test files).

## Non-goals

- Removing QProcess plumbing from `game_session.cpp` — still used by Dolphin / DuckStation / PPSSPP launched-binary adapters.
- Root-causing the EE freeze. The workaround is stable; SP8 ships independent of any further investigation.
- Touching the user's personal PCSX2 install outside `~/Documents/RetroNest/emulators/pcsx2/`. Their system-level PCSX2 (if any) is unaffected.
- Touching SP5.5 core-side rumble (can land in parallel).

## Decisions

### D-SP8-1 — ID strategy: rename

Delete the "pcsx2" id (standalone). Rename "pcsx2-libretro" → "pcsx2". User-facing name becomes plain "PCSX2" — the `(libretro core, dev)` suffix is dropped.

**Rationale:** "pcsx2" is the right user-facing name. The libretro path is production-grade. Existing scanned games with `emulator_id="pcsx2"` keep working unchanged. Existing games with `emulator_id="pcsx2-libretro"` get migrated.

### D-SP8-2 — User data migration: wholesale archive + move

Idempotent one-shot migration on app startup, gated by sentinel file `~/Documents/RetroNest/emulators/.sp8-migrated`. Algorithm:

1. **Detect standalone install** in `emulators/pcsx2/`. Signal markers: presence of any of `PCSX2-v*.app`, `portable.txt`, `.version.json`, `inis/`, `resources/`. If any present, treat the dir as a standalone install.
2. **Archive wholesale:** `mv emulators/pcsx2/ emulators/.archive/pcsx2-standalone-<unix-timestamp>/`. The user can recover anything from there; RetroNest no longer reads it.
3. **Promote libretro data:** `mv emulators/pcsx2-libretro/ emulators/pcsx2/`. After this, the libretro data dir is now at the canonical `pcsx2/` path.
4. **Touch sentinel** `emulators/.sp8-migrated`. Subsequent launches see the sentinel and short-circuit.

Failure modes:

- **`emulators/pcsx2/` doesn't exist:** standalone never installed. Just rename `pcsx2-libretro` → `pcsx2` if present and touch sentinel.
- **`emulators/pcsx2-libretro/` doesn't exist:** libretro never run. Just archive standalone dir (if standalone) and touch sentinel.
- **Both absent:** fresh user. Touch sentinel and proceed.
- **Atomic-move failure mid-step:** log error, **do not** touch sentinel. Next launch retries from the current state. The archive step is fully reversible (it's an `mv`, the data lives somewhere on disk either way).
- **Sentinel already present:** no-op.

**Rejected alternative:** per-file merge of standalone memcards/savestates into the libretro dir. Standalone and libretro use different memcard files at different sub-paths; "merging" them would silently overwrite or confuse. The user's recent play (libretro) wins as the canonical state; standalone data is recoverable from `.archive/`.

### D-SP8-3 — Manifest collapse

- Delete `manifests/pcsx2.json` (standalone).
- Move `manifests/pcsx2-libretro.json` → `manifests/pcsx2.json`.
- Edit the moved file: `"id": "pcsx2"`, `"name": "PCSX2"`, `"description": "PlayStation 2 emulator (libretro core)"`. All other libretro fields (`backend`, `core_dylib`, `core_buildbot_path`, `install_folder`, etc.) preserved verbatim.

### D-SP8-4 — `controls.ini`

Lives under the per-emulator data dir, gets carried in the wholesale `mv` of D-SP8-2 step 3. No separate handling needed.

### D-SP8-5 — Test files: retarget

All three tests:

- `cpp/tests/test_pcsx2_preview_spec.cpp` — asserts `PCSX2Adapter::previewSpec()` returns aspect preview on "Recommended" and no preview on "Graphics > Display".
- `cpp/tests/test_pcsx2_schema.cpp` — asserts the exact 89-knob schema across categories.
- `cpp/tests/test_pcsx2_controller_schema.cpp` — asserts DualShock 2 is the only controller type, controllerSettingDefs empty, all DS2 bindings have non-empty `cardSlot`, etc.

All three retarget cleanly to `Pcsx2LibretroAdapter` — the SP7c Phase 5 parity work guarantees identical previewSpec, schema, and controller schema between the two adapters. Mechanical edit: swap `#include "adapters/pcsx2_adapter.h"` → `#include "adapters/libretro/pcsx2_libretro_adapter.h"`, swap `PCSX2Adapter` → `Pcsx2LibretroAdapter`. Filenames keep the `test_pcsx2_*` prefix (they test the canonical "pcsx2" adapter).

### D-SP8-bonus — Directory rename in UI source

`cpp/src/ui/settings/pcsx2_libretro/` → `cpp/src/ui/settings/pcsx2/` (replacing the deleted standalone dir at the same path). Files renamed to drop the `_libretro` suffix:

- `pcsx2_libretro_settings_dialog.{h,cpp}` → `pcsx2_settings_dialog.{h,cpp}` (taking the names freed by the standalone delete)
- `pcsx2_libretro_category_hub.{h,cpp}` → `pcsx2_category_hub.{h,cpp}`

Class names follow: `Pcsx2LibretroSettingsDialog` → `Pcsx2SettingsDialog`, `Pcsx2LibretroCategoryHub` → `Pcsx2CategoryHub`. The `Pcsx2LibretroAdapter` class itself **stays named** — that's an internal implementation choice and renaming it is unnecessary churn.

## Inventory — what changes

### Deleted (source)

- `cpp/src/adapters/pcsx2_adapter.h`
- `cpp/src/adapters/pcsx2_adapter.cpp` (~1800 LOC)
- `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.{h,cpp}` (the standalone version)
- `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.{h,cpp}` (the standalone version)

Total: 6 files. The directory `cpp/src/ui/settings/pcsx2/` survives — repopulated by the renamed libretro-flavor files.

### Renamed (source)

`cpp/src/ui/settings/pcsx2_libretro/` (4 files) → `cpp/src/ui/settings/pcsx2/` per D-SP8-bonus.

### Deleted (manifests)

- `manifests/pcsx2.json` (standalone)

### Renamed (manifests)

- `manifests/pcsx2-libretro.json` → `manifests/pcsx2.json` (with field edits per D-SP8-3)

### Modified (source)

- `cpp/CMakeLists.txt` — drop the 6 standalone source-file references (lines 63, 123, 124, 146, 223, 224, 467, 490, 707, 759, 798); fix the 4 libretro UI references after the rename (lines 129, 130, 207, 208); fix the 3 test targets to compile against `pcsx2_libretro_adapter.cpp` instead of `pcsx2_adapter.cpp` (lines 658–695); remove any other lingering pcsx2_adapter.cpp refs.
- `cpp/src/adapters/adapter_registry.cpp` — delete line 17 (`registerAdapter("pcsx2", std::make_unique<PCSX2Adapter>())`); change line 22 from `"pcsx2-libretro"` to `"pcsx2"`. Remove the standalone `#include "pcsx2_adapter.h"` on line 2.
- `cpp/src/ui/app_controller.cpp` — line 17/22: remove the standalone-dialog include, fix the libretro-dialog include path; lines 458–488: collapse the two `if (emuId == ...)` branches into one (`"pcsx2"` only, using the renamed dialog).
- `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` line 57: change `Paths::emulatorDataDir("pcsx2-libretro", "ps2")` → `Paths::emulatorDataDir("pcsx2", "ps2")`. Update the explainer comments at lines 51–52, 499, 504, 575, 868 to refer to the new path.
- `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_settings_dialog.cpp` line 34: `AdapterRegistry::instance().adapterFor("pcsx2-libretro")` → `adapterFor("pcsx2")`. (Will be edited in its renamed location.)
- `cpp/src/adapters/duckstation_adapter.h:35` — comment "See PCSX2Adapter for rationale" — repoint to `Pcsx2LibretroAdapter` or strip.
- `cpp/src/ui/settings/widgets/preview/osd_preview.h:21,27` — two comments reference `Pcsx2Adapter::previewSpec()`. Repoint to `Pcsx2LibretroAdapter::previewSpec()`.

### Not modified (verified)

- `cpp/src/core/game_session.cpp:141` — Rosetta slow-mode warning checks `coreId() == "pcsx2"`. `Pcsx2LibretroAdapter::coreId()` already returns `"pcsx2"` (header line 14), so this fires correctly without change.

### Migration (new code)

- `cpp/src/core/migration_pcsx2.{h,cpp}` — single function `runSp8MigrationIfNeeded()`, invoked once from `app_controller.cpp` startup before any UI is constructed. Idempotent, sentinel-gated.

## Task plan

8 tasks, each a single commit (mirrors Phase 4/5 cadence):

| # | Task | Surface | Est |
|---|---|---|---|
| 1 | Spec + plan docs (this file + 2026-05-15-pcsx2-standalone-retirement.md) | docs/ | 1 hr |
| 2 | Manifest collapse + adapter_registry id swap + include cleanup | manifests/, adapter_registry.{cpp,h} | 30 min |
| 3 | `app_controller.cpp` dispatch unification | app_controller.cpp + dialog include | 30 min |
| 4 | Delete 6 standalone source files + drop standalone CMakeLists refs | 6 files + CMakeLists.txt | 30 min |
| 5 | Rename `settings/pcsx2_libretro/` → `settings/pcsx2/`, drop `_libretro` suffix on files + classes, fix CMakeLists, fix includes, fix `adapterFor("pcsx2-libretro")` → `adapterFor("pcsx2")`, fix `emulatorDataDir("pcsx2-libretro", ...)` → `emulatorDataDir("pcsx2", ...)` | 4 renamed files + ~3 site fixes | 45 min |
| 6 | Retarget 3 test files to `Pcsx2LibretroAdapter`; rebuild + run | 3 test files + CMakeLists | 30 min |
| 7 | User-data migration (`migration_pcsx2.{h,cpp}` + app_controller startup hook) | 2 new + 1 modified | 1–2 hr |
| 8 | Comment/docstring cleanup (`osd_preview.h`, `duckstation_adapter.h`, libretro adapter explainer comments) + live smoke gate | ~5 files + manual smoke | 30 min |

**Total estimate:** 5–7 hr.

## Smoke gate

After Task 8:

1. Backup `~/Documents/RetroNest/emulators/pcsx2/` and `~/Documents/RetroNest/emulators/pcsx2-libretro/` manually before launching.
2. Build via `./scripts/build-universal.sh`.
3. Launch RetroNest. Migration runs once on first start.
4. Verify:
   - `emulators/pcsx2/` now contains the libretro data layout (memcards, savestates, core options).
   - `emulators/.archive/pcsx2-standalone-<timestamp>/` contains the previous standalone install.
   - `emulators/.sp8-migrated` exists.
   - `emulators/pcsx2-libretro/` no longer exists.
5. Open a previously-scanned PS2 game in the library (R&C 2 NTSC + DBZ TT2 PAL recommended — both are SP7b smoke regulars). For each:
   - Game boots through to title screen.
   - Controller input works (D-pad + analog + rumble).
   - Memcard load + save persists across launches.
   - Save state save + load works in-session and cold-resume.
   - Settings dialog opens, all 89 knobs render correctly, sub-tabs intact, preview widgets functional.
6. Re-launch RetroNest. Verify migration does NOT run again (sentinel short-circuit).

## Risk callouts

- **Migration runs before UI:** If `mv` fails mid-step, the user starts in a half-migrated state. Mitigation: log error to console + crash log, do not touch sentinel, bail. Next launch retries.
- **Wholesale archive loses standalone memcards as primary:** Deliberate. The user's recent play is libretro; standalone data is recoverable from `.archive/`. If they object, they can manually restore.
- **No-fallback risk:** Post-SP8, any regression in the libretro path has no standalone path to fall back to. Mitigation: the EE freeze workaround is stable; smoke gate must pass before merging.
- **Test re-targeting:** If `Pcsx2LibretroAdapter` has any subtle schema drift vs. `PCSX2Adapter` that the 89/89 parity check missed, the retargeted tests will fail. Mitigation: tests are run as part of Task 6; if they fail, that's a real parity bug to fix, not a SP8 blocker.

## Related work

- [SP7c Phase 5 spec](2026-05-15-pcsx2-libretro-sp7c-phase5-ui-parity-design.md) — settings UI parity that this SP depends on.
- Memory: `[[project-pcsx2-libretro-port]]` — overall port arc.
- Memory: `[[ee-recompiler-freeze-pickup]]` — workaround stability context.
- Memory: `[[cold-resume-freeze-and-cocoa-class-chain]]` — Bug C (NSKVO observer on shutdown) is independent of SP8 but worth noting as still-open.
