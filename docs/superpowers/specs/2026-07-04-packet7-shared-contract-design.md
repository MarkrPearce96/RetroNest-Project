# Packet 7 — Shared Core Contract & Extensibility Refactor (Design)

Date: 2026-07-04
Status: approved by user (stages 1–4), pending implementation plan
Source: `RetroNest-Suite-Review-2026-07-02.md`, Packet 7 + findings #6, #7, and the Medium manifest/branching items. Final packet of the suite review (packets 0–6 shipped).

## Goal

Make adding/maintaining a libretro core cheap and drift-free:

1. **(a)** One vendored `retronest-libretro` contract package replacing hand-copied private-ABI constants, four diverged `libretro.h` vintages, and triplicated glue.
2. **(b)** The core becomes the single source of settings-schema truth; the ~2,500–3,000 lines of hand-mirrored option rows in the adapters are deleted.
3. **(c)** Manifests (plus a new systems registry) become the real registry; per-core `emuId === …` branching leaves QML/services.
4. **(d)** CLAUDE.md and the new-adapter checklist describe the libretro reality.

End-state test: a conforming stock core (mGBA today, core #6 tomorrow) needs only a manifest + registry entries + a small curation overlay. A fork with private-ABI needs additionally: run `sync.sh`, implement the env/export surface.

## Corrections to the review report (verified 2026-07-04 by code survey)

These four findings shaped the design and differ from the report's text:

1. **RetroNest has no `check_schema_fidelity.py`** — adapter comments referencing it are phantoms. The three real copies live in the forks (225 lines each, name-string diffs only; duckstation's is not even wired into CMake). Real enforcement is the C++ QtTest shape-guards (`test_{pcsx2,dolphin,duckstation,ppsspp}_libretro_schema`) added in packets 5–6.
2. **The hand-mirrored schemas are load-bearing offline.** `LibretroAdapter::libretroOptionsStore()` (libretro_adapter.cpp:169-195) synthesizes a fallback OptionsStore from `settingsSchema()` so settings are browsable/editable before any game has run. The core's `SET_CORE_OPTIONS_V2` metadata (environment_callbacks.cpp:32-46) exists only during a session and is dropped after value-validation. Piece (b) must supply an **offline** metadata source.
3. **The schema is ~half mirror, ~half genuine frontend authoring.** `SettingDef` (cpp/src/core/setting_def.h) carries category/subcategory/group routing, custom labels, tooltips, `dependsOn`, save/load transforms, recommendedValue, layout hints; hub cards (emoji + grid coords) are separate. The core supplies keys/value-sets/defaults/descriptions; UI curation stays host-side.
4. **The adoption surface is RetroNest + 3 forks, not 5 repos.** mGBA is a stock upstream clone (zero RetroNest code — must never carry the package). ppsspp's fork has only an empty `ppsspp-libretro/` scaffold; it adopts when that gains code.

Bonus finding: duckstation's NSView-metrics helper (libretro_window.mm:99-108) carries a `dispatch_sync`-to-main-thread fix that pcsx2's `MacNSViewMetrics.mm` lacks — the shared helper propagates it.

## Decisions (user-approved)

| # | Decision | Choice | Key rationale |
|---|---|---|---|
| 1 | Schema-truth mechanism | **Runtime capture + cached sidecar** (not GET_SETTINGS_SCHEMA export, not build-time generation) | Works for stock mGBA and any future conforming core; no new ABI; forks barely change |
| 2 | Package distribution | **Copy + sync script + checksum drift check** (not submodule/subtree) | Matches pcsx2's existing pin-banner precedent; forks stay standalone; no submodule friction with upstream rebases/CI |
| 3 | 7(c) depth | **Registry + QML de-branching only** | Process-era `EmulatorAdapter`/`Backend::Process` retirement deferred to a later packet |
| 4 | Settings wording | **Adopt core-declared labels/descriptions**; overlay pins wording only where hand-written text is clearly better | Keeps the overlay small — the point of the refactor. Pages will read slightly differently |
| 5 | Shared deploy/CMake fragment | **Document the deploy contract now; consolidate CI later** | The dylibbundler/sign/zip pipeline was rebuilt and end-to-end verified 2026-07-04 (v2026.07.04.1); churning it is highest-risk/lowest-urgency. Follow-up packet |

## Stage 1 — `retronest-libretro` contract package (7a)

**Canonical home:** `RetroNest-Project/vendor/retronest-libretro/`, absorbing `vendor/libretro-api/` (update the ~6 `target_include_directories` references in cpp/CMakeLists.txt).

Contents:

- `libretro.h` — pinned copy. RetroNest's current vendor copy (newest vintage, env cmd ≤ 80) becomes the pin; forks update to it (duckstation's is a 2023 generation older; dolphin/ppsspp 2024). Header changes are additive; verified by compiling each fork.
- `retronest_libretro.h` — the contract header:
  - `RETRO_ENVIRONMENT_PRIVATE` base (0x20000) + the five command macros under canonical names: `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` (0x20001), `GET_BOOT_STATE_PATH` (0x20002), `GET_MEMCARDS_DIR` (0x20003), `GET_TEXTURES_DIR` (0x20004), `SET_GAME_IDENTITY` (0x20005), each with direction/ownership doc comments and which cores use it.
  - `struct retronest_game_identity { const char* ra_hash; const char* serial; }` (duckstation's name wins; dolphin's `RetroNestGameIdentity` renamed).
  - Prototypes for the optional exports: `retronest_set_paused(bool)`, `retronest_set_fast_forward(bool)`, `retronest_shutdown_wedged() -> bool` — all optional per core (duckstation exports none; that is conforming).
  - A "next free slot: 0x20006" marker.
- `retronest_nsview.h/.mm` — shared NSView-metrics helper based on duckstation's (with the main-thread `dispatch_sync` fix). Adopted by pcsx2 (replaces `MacNSViewMetrics.mm` — deliberate behavior fix, gets its own smoke test). dolphin's `LibretroMetalContext.mm` is a Metal-context helper, out of scope.
- `emit_core_options_v2.h` — header-only unification of the pcsx2/dolphin `EmitCoreOptionsV2()` helpers. duckstation keeps its inline dispatch (adoption optional).
- `docs/option-style-guide.md` — grammar rules for **future** options (boolean spelling `enabled/disabled`, float format, key naming). Migrating existing option values is a breaking change, out of scope.
- `docs/deploy-contract.md` — the deploy/release contract written down once: destination `{root}/emulators/libretro/cores/`, `<core>_libretro.{dylib,dylib.zip}` layout, `<core>_libretro_resources/` + `<core>_libretro_libs/` conventions, dylibbundler + sibling-ref flatten + ad-hoc codesign steps, version sidecars, universal-vs-x86_64 policy. (Per decision 5, CI workflows are not touched in this packet.)
- `sync.sh` + `MANIFEST.sha256` — copies the package into each adopting repo's vendored location; each adopter gets a cheap check (CMake target or CI step) that its vendored files match the manifest (catches local edits; staleness vs canonical is caught at sync time).

**Per-fork vendored locations:** `duckstation-libretro/src/duckstation-libretro/retronest-libretro/`, `pcsx2-libretro/pcsx2-libretro/retronest-libretro/`, `dolphin-libretro/Source/Core/DolphinLibretro/retronest-libretro/` (inside each fork's isolation directory, preserving the upstream-confinement discipline).

**Adoption changes:**

- RetroNest: `environment_callbacks.h` includes `retronest_libretro.h` instead of declaring the macros/struct; `core_loader.h` typedefs from the package prototypes.
- duckstation: replaces the local constants in `libretro_window.mm:123` and `libretro.cpp:356-360`; replaces `libretro.h`.
- pcsx2: replaces constants in `HostStubs.cpp:271`, `LibretroFrontend.cpp:175-176`, `Settings.cpp:43-46`, `tools/test_settings_overrides.cpp:18-20`; replaces `libretro.h`; swaps `MacNSViewMetrics.mm` for the shared helper; deletes the committed `tools/__pycache__/`.
- dolphin: `LibretroEnvironment.h:17,24,26-30` re-exports from the package (or callers include it directly); `tools/test_harness.mm:40` updated; replaces `libretro.h`.
- Python fidelity checkers are **left in place** this stage — they still guard the hand schemas until stage 2 deletes both together.

**Order:** RetroNest → dolphin → pcsx2 → duckstation, each ending in rebuild + deploy + in-app smoke before the next.

**Safety:** constants byte-identical by construction ⇒ zero behavior change except pcsx2's NSView fix (deliberate, isolated, smoke-tested).

## Stage 2 — core as settings-schema truth (7b)

Data flow: core emits `SET_CORE_OPTIONS_V2` → RetroNest persists full metadata to a per-core sidecar → settings UI renders sidecar × curation overlay → hand-mirrored rows deleted.

Components:

1. **Capture.** `handleCoreOptionsV2` retains full v2 metadata (labels, info strings, category ids, value display names); `CoreRuntime` writes `{root}/emulators/libretro/<coreId>/declared_options.json` (sibling of `options.json`) on receipt. Sidecar carries a format version + the core's `library_version` for staleness detection. (`SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK` remains unhandled — unchanged from today.)
2. **Offline seeding — `CoreProber`.** dlopen the core, install the environment callback, call `retro_set_environment`, collect options, unload. Run at install time or on first settings visit when no sidecar exists. **SPIKE FIRST:** verify each of the five cores emits options during `retro_set_environment` (not `retro_init`/`retro_load_game`). Fallback for late emitters: capture-on-first-session + CI-generated seed JSON in the release zip.
3. **Curation overlay.** Per-core, stays C++ in the adapter (transforms/`dependsOn` are code; one authoring place). Entry keyed by option key: category/subcategory/group routing, optional label override (exceptional per decision 4), `dependsOn`, layout/suffix/slider hints, `recommendedValue`, hidden flag, ordering. Hub cards stay hand-authored.
4. **Generic merge.** `LibretroAdapter::settingsSchema()` base implementation: declared options × overlay → `SettingDef` rows. Includes type inference (`enabled/disabled` & `true/false` pairs → Bool; overlay can override) and generation of the "Recommended" category rows from `recommendedValue` marks (replacing today's duplicated rows). Per-core adapters shrink to overlay + hub cards + genuinely hand-authored non-option rows (`Storage::FrontendSetting`/`Ini`, e.g. mGBA/ppsspp aspect ratio). pcsx2's deliberate `categories = nullptr` is fine — categories live in the overlay.
5. **Fallback rewire.** `libretroOptionsStore()`'s no-runtime path synthesizes from the sidecar instead of the hand schema — browse-before-launch keeps working.
6. **Parity net + deletion.** Transition-only harness diffs new rendered schema vs. old hand schema on keys/value-sets/defaults (not wording) per core before its mirror rows are deleted. Pilot: **mGBA** (smallest schema; proves the stock-core path), then duckstation → ppsspp → dolphin → pcsx2, each parity-diffed + in-app smoked. After the last core: delete the three fork-side Python checkers + their CMake targets. QtTest shape-guards are kept, re-pointed at the merged schema.

Invariants: `options.json` values untouched (no user-settings migration); stale-key silent-reset behavior unchanged (out of scope).

## Stage 3 — registry + de-branching (7c)

- **`manifests/systems.json`** (new): systemId → display name, RA console ID, ScreenScraper platform ID. Replaces `ThemeContext::systemDisplayName()` (theme_context.cpp:252-283), `scraper.cpp:74-86`, `ra_client.cpp:334-346`, **and** the five per-adapter `raConsoleId()` overrides (two competing truth sources today; dolphin spans GC+Wii so the registry is system-keyed).
- **Emulator manifests fattened:** `logo` asset path (kills `EmulatorLogos.js`, fixes mGBA's missing logo) + a `detail_page` capabilities block (has-patches, controller-slot count, extra rows) consumed by `EmulatorDetailPage.qml` as a row model — replacing every `emuId === "dolphin"` branch and the focus-index offset arithmetic (lines 31–96, 390–454) with model-driven indices.
- **Loader hardening:** `manifest_version` field; warn on unknown keys (today silently ignored).
- Fields are added only where a consumer exists today; speculative registry fields (bios requirements, max players, …) wait for a consumer.
- Verification: extend the manifest QtTests; **hands-on gamepad smoke pass** for detail-page focus navigation.

## Stage 4 — documentation (7d)

- **CLAUDE.md:** rewrite architecture/config-strategy sections around the libretro reality (in-process cores, OptionsStore + declared-options sidecar + overlay, contract package, registries). Delete: INI-patching-era config strategy, "RetroAchievements is web-only" (contradicted by live rcheevos runtime), per-emulator native binding formats, phantom checker references, stale Technology Stack emulator list.
- **`docs/new-adapter-checklist.md`:** front half rewritten as the core-#6 recipe (fork upstream or use stock core → `sync.sh` → env/export surface as needed → manifest + systems entries + overlay → smoke checklist). Settings-audit back half kept.
- **Per-fork CLAUDE.md** (duckstation, pcsx2, dolphin, ppsspp — short): build command, arch policy, deploy route, upstream-rebase procedure, push policy.

## Process guarantees (all stages)

- The x86_64/Rosetta app is the daily driver: every stage ends with `cpp/build-x86_64` rebuild (macdeployqt fires via the packet-5 POST_BUILD hook), redeploy, and a written smoke-test checklist for the user.
- **Nothing is pushed until the user confirms that stage in-app.** Stages land as separate commits per repo, individually revertible.
- Full QtTest suite green before each smoke handoff.

## Out of scope (recorded follow-ups)

- CI/deploy consolidation (shared `package-core.sh`) — after this packet settles (decision 5).
- Process-era retirement: `Backend::Process`, launch-arg machinery, INI helpers, `EmulatorAdapter` slim-down (decision 3).
- Migrating existing option values to the style guide grammar (schema-breaking release).
- OptionsStore stale-key silent-reset UX; `SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK` support.
- ppsspp/mGBA package adoption (ppsspp when its scaffold gains code; mGBA never while stock).

## Risks

| Risk | Mitigation |
|---|---|
| Probe timing (cores emitting options after `retro_set_environment`) | Stage-2 spike is the first task; fallback = first-session capture + CI seed JSON |
| Settings wording changes user-visibly | Decision 4 accepts this; overlay pins exceptional labels; parity diff ignores wording but pins structure |
| pcsx2 NSView helper swap changes behavior | Deliberate fix; isolated commit; dedicated smoke item |
| Detail-page focus regressions | Gamepad-in-hand smoke checklist item |
| libretro.h bump breaks a fork build | Additive header; compile check per fork before anything else in that repo |
| Daily driver breakage mid-refactor | Per-stage smoke gate + no-push-until-confirmed + revertible per-repo commits |

## Effort estimate

Stage 1 ≈ one session · Stage 2 ≈ two–three sessions (spike + pilot + four cores) · Stage 3 ≈ one session · Stage 4 ≈ half a session.
