# PPSSPP Phase E — schema-driven libretro option defaults

**Status:** Approved for implementation
**Date:** 2026-05-22
**Predecessors:** Phase A (controller bindings), Phase B+C (settings schema + hub cards), V2_INTL fix `7361efc`
**Successor:** Migrate mgba + PCSX2 adapters to the same mechanism (deferred follow-up)

## Goal

Ship smarter out-of-box defaults for the PPSSPP libretro core on first launch — initially just bumping internal rendering resolution from 1x to 4x — via a reusable mechanism that any current or future libretro adapter inherits for free.

## Background

Until commit `7361efc` (V2_INTL handshake fix), RetroNest's PPSSPP libretro core never received any of its option values from the frontend — the SET_CORE_OPTIONS_V2_INTL env-cmd was unhandled, so options.json writes were silently ignored core-side. With that fix in place, the libretro core now reads the values RetroNest writes, which means default choices in `OptionsStore::load` finally have visible effect.

PPSSPP's upstream defaults are aimed at low-end hardware — most notably `ppsspp_internal_resolution = "480x272"` (1x native). On a modern display that looks pixelated and soft. A new RetroNest user should see crisp rendering without having to dive into Settings → PSP → Video on first run.

## In scope

- New plumbing so that an adapter's `SettingDef.defaultValue` (in `settingsSchema()`) overrides the libretro core's declared default at first-launch persistence time.
- PPSSPP: change `ppsspp_internal_resolution` schema default from `"480x272"` to `"1920x1088"` (4x).
- Regression test ("drift guardrail"): assert every PPSSPP `LibretroOption`-typed `SettingDef.defaultValue` matches upstream's declared default, with an explicit allowlist for intentional overrides.
- Regression test (dupe consistency): assert any duplicated-key rows in the schema (the Recommended card duplicates rows from other cards by design) carry matching `defaultValue` fields.

## Out of scope

- Migrating mgba / PCSX2 adapters to the new mechanism. The infrastructure built here is generic; opt-in for those cores is a follow-up that consists of (a) verifying their schemas don't drift from upstream, then (b) deliberately changing any `SettingDef.defaultValue` we want to override.
- Network options (still deferred from Phase B+C; no defaults to override yet).
- Per-game default overrides — a separate, larger feature. This design composes cleanly with per-game overrides layered on top later (per-game value > global value > schema default > upstream default); per-game work touches a different layer.
- Re-applying defaults to users who already have an `options.json`. Existing-value-wins semantics in `OptionsStore::load` are preserved unchanged.

## Architecture

The change in one sentence: **`OptionsStore::load` prefers the adapter schema's `defaultValue` over the libretro core's declared default when picking what to write on first launch, and `GameSession` extracts that map from the adapter before calling `CoreRuntime::start`.**

Resolution rule for each libretro option key, in priority order:

1. **Existing value in `options.json`**, if present and listed in `opt.values`. Unchanged from today.
2. **Schema default** — the adapter `SettingDef` for this key (where `storage == LibretroOption`), if its `defaultValue` is valid against `opt.values`.
3. **Upstream-declared default** — `opt.defaultValue` from the libretro core's SET_CORE_OPTIONS_V2_INTL payload. Fallback for any option the schema doesn't cover (e.g., PPSSPP's deferred network options).

The validity check against `opt.values` is the safety net: a stale, mistyped, or upstream-renamed schema value silently degrades to upstream-default at runtime rather than corrupting `options.json`. The drift test catches the typo at CI time.

### Why schema-as-single-source-of-truth, not a separate override map

The setup-kit alternative was a parallel `libretroOptionDefaults() -> QHash<key,value>` virtual method on the adapter. That works, but introduces two independently editable sources of "what the default is": `SettingDef.defaultValue` (drives UI dropdown highlight) and the override map (drives `OptionsStore` writes). Nothing enforces them to stay in sync — exactly the gotcha the setup kit itself flagged.

The chosen design eliminates that split: there is one field per setting (`SettingDef.defaultValue`), and both the UI surface and the runtime persistence layer consult the same field. Any future adapter author setting a default touches one place.

### Per-game compatibility

Per-game settings, when built later, become a new top layer (`options.<gameSerial>.json` overlay) on the priority chain. They sit above today's "existing value" layer and don't interact with the defaults question this design solves. The schema-as-source-of-truth model actually helps per-game UI later because there's a single canonical "default if everything else is unset" field to render as the inherits-from-default fallback.

## Components & data flow

Seven files change. Listed in dependency order so implementation proceeds bottom-up.

### `cpp/src/core/libretro/options_store.h`

Extend `load()` signature with an optional schema-defaults parameter. Default-arg preserves existing callers (notably the `:memory:` test path):

```cpp
bool load(const QString& jsonPath,
          const QVector<CoreOption>& coreOptions,
          const QHash<QString, QString>& schemaDefaults = {});
```

### `cpp/src/core/libretro/options_store.cpp`

Both code paths in `load()` (disk and `:memory:`) share a helper that implements the resolution rule:

```cpp
static QString pickInitialValue(const CoreOption& opt,
                                const QHash<QString, QString>& existing,
                                const QHash<QString, QString>& schemaDefaults) {
    auto existIt = existing.constFind(opt.key);
    if (existIt != existing.constEnd() && opt.values.contains(existIt.value()))
        return existIt.value();
    auto schemaIt = schemaDefaults.constFind(opt.key);
    if (schemaIt != schemaDefaults.constEnd() && opt.values.contains(schemaIt.value()))
        return schemaIt.value();
    return opt.defaultValue;
}
```

The `:memory:` branch (no on-disk `existing` map) passes an empty `QHash` for `existing`.

### `cpp/src/core/libretro/core_runtime.h`

Add one field to `StartConfig`:

```cpp
struct StartConfig {
    // ... existing fields ...
    QHash<QString, QString> schemaOptionDefaults;  // keyed by libretro option key
};
```

### `cpp/src/core/libretro/core_runtime.cpp`

At the existing `m_options.load(...)` callsite (line 386), pass the new map through:

```cpp
m_options.load(m_cfg.optionsJsonPath, m_envCtx.declaredOptions, m_cfg.schemaOptionDefaults);
```

### `cpp/src/core/game_session.cpp`

Around line 209 (where `cfg.optionsJsonPath` is set), extract the schema defaults from the libretro adapter before populating cfg further. `FrontendSetting`-typed rows like `aspect_mode` are deliberately excluded — they're not libretro options:

```cpp
QHash<QString, QString> schemaDefaults;
for (const auto& s : lr->settingsSchema()) {
    if (s.storage == SettingDef::Storage::LibretroOption && !s.key.isEmpty())
        schemaDefaults.insert(s.key, s.defaultValue);
}
cfg.schemaOptionDefaults = std::move(schemaDefaults);
```

PPSSPP's schema contains duplicate keys by design — the Recommended card duplicates rows from other cards. Both copies of any dupe must carry matching `defaultValue` for the QHash overwrite-on-insert to be safe. This invariant is enforced by the dupe-consistency test below rather than by guarding at extraction time.

### `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`

Exactly one value change: find the `SettingDef` for `ppsspp_internal_resolution` and change `defaultValue` from `"480x272"` to `"1920x1088"`. Apply to both the Video card row and the Recommended-card duplicate (per the matching-defaults invariant).

### `cpp/tests/test_ppsspp_libretro_schema.cpp`

Add two new test slots:

- **`libretroOptionDefaults_matchUpstreamUnlessAllowlisted`** — for every `LibretroOption`-typed row in `PpssppLibretroAdapter().settingsSchema()`, look up the upstream default in a hardcoded fixture and assert equality, unless the row's `key` is in `intentionalOverrides = {"ppsspp_internal_resolution"}`. A schema key missing from the fixture is a test failure, forcing fixture maintenance when new options are added.
- **`duplicatedRows_haveConsistentDefaults`** — group schema rows by `key`, assert every group has a single distinct `defaultValue`.

The upstream-defaults fixture is a hardcoded `QHash<QString, QString>` mirroring `libretro/libretro_core_options.h` from the ppsspp-libretro repo. Same maintenance pattern as the existing `knownUpstreamKeys()` `QSet` in this file. ~43 entries.

When upstream PPSSPP changes a default, the first test fails with a clear diff; the contributor either accepts the upstream change (update fixture) or rejects it (add the key to `intentionalOverrides` and update the schema to the deliberate value).

## Error handling

The design is self-healing because of the validity check in `pickInitialValue`:

| Scenario | Behavior |
|---|---|
| Schema has a key the libretro core doesn't declare | Silently ignored — `OptionsStore::load` iterates `coreOptions`, so schema-only keys are never read. |
| Schema's `defaultValue` not in `opt.values` (stale, typo, upstream renamed an enum) | Falls back to upstream's `opt.defaultValue`. Drift test catches this at CI time. |
| Schema is empty (adapter hasn't filled it in yet) | Behavior identical to today — upstream defaults across the board. |
| Two schema rows for the same key with different `defaultValue` | QHash overwrite is last-write-wins; dupe-consistency test catches this at CI time. |
| `cfg.schemaOptionDefaults` empty (e.g. non-libretro adapters, or no overrides configured) | Behavior identical to today. Backwards-compatible. |

No new runtime error paths. Every failure mode degrades to "today's behavior" rather than introducing a new way to break.

## Migration impact

**Existing users:** none. `OptionsStore::load` only writes a default when the key is missing or invalid in `options.json`. Users with an existing PPSSPP `options.json` keep their current resolution.

**Fresh installs / users who delete `options.json`:** see 4x rendering on first launch.

**Other libretro cores in this codebase (mgba, PCSX2):** none today. The mechanism is in place but no defaults are overridden — they continue to use upstream defaults until a future PR opts them in by editing `SettingDef.defaultValue` in their respective adapter schemas.

## Manual verification

1. Delete `~/Documents/RetroNest/emulators/libretro/ppsspp/options.json`.
2. Launch RetroNest.
3. Navigate Settings → PSP → Video → Rendering Resolution. Confirm dropdown shows "4x PSP (1920x1088)" as the selected value.
4. Inspect new `options.json` on disk. Confirm `"ppsspp_internal_resolution": "1920x1088"`.
5. Launch a PSP game (e.g. Crisis Core). Confirm rendering looks crisp / not pixelated 1x.
6. Manually edit `options.json` to set resolution to `"480x272"`. Re-launch RetroNest. Confirm the user-set value is respected (regression check on the "existing value wins" priority).

## Test count maintenance

`test_ppsspp_libretro_schema.cpp` has slots asserting hardcoded schema counts ("43 unique upstream options + 10 Recommended dupes + 2 FrontendSetting rows = 55"). Phase E does not add or remove any rows — only changes one `defaultValue`. The existing count slots stay green untouched. Two new test slots are added; if the test target is wired via QTest auto-discovery, no build-system change is needed. If individual slot names are listed somewhere, both new slot names must be added.

## Estimated scope

- Plumbing (options_store.h/cpp, core_runtime.h/cpp, game_session.cpp): ~30 LOC.
- PPSSPP adapter value change: 2 lines (Video card + Recommended dupe).
- Two new test slots + upstream-defaults fixture: ~80 LOC, of which the ~43-entry fixture is the bulk of the mechanical work (order of an hour to populate carefully against the upstream header).

Total: ~120 LOC + 1 hour fixture grunt. ~1 day end to end including manual verification.
