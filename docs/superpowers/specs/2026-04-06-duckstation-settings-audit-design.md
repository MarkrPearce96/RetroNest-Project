# DuckStation Settings Audit — Design

**Date:** 2026-04-06
**Type:** Read-only audit (no code changes)
**Scope:** Non-controller `SettingDef`s in `cpp/src/adapters/duckstation_adapter.cpp`

## Goal

Verify every non-controller setting our app exposes for DuckStation round-trips correctly with the native emulator: that changing it in our UI actually changes the right key, in the right section, with a value DuckStation will read back the same way.

## Why

Per `CLAUDE.md`, our app and DuckStation share the same INI file as source of truth. If our `SettingDef` references a wrong section/key, or our `Combo` option strings don't match the format DuckStation writes back to disk, the UI silently falls back to defaults and user changes appear to "not stick." This is a class of bug that's invisible until a user notices, so a systematic audit is the only way to catch it.

This audit follows the same methodology as the 2026-04-06 PCSX2 audit — see `docs/superpowers/specs/2026-04-06-pcsx2-settings-audit-design.md` and the report at `docs/superpowers/audits/2026-04-06-pcsx2-settings-audit.md` for the pattern.

## Methodology

For each `SettingDef` in `DuckStationAdapter::settingsSchema()`, in source order:

1. **Existence** — locate `section` + `key` in `references/duckstation-master/`. DuckStation typically reads settings via `Settings::Load` in `src/core/settings.cpp` and via `SettingsInterface` accessor calls in feature code. Enums round-trip via `Settings::Parse*FromString` / `Settings::Get*Name` helpers (e.g. `ParseRendererName` / `GetRendererName`). If the key isn't found, ERROR.
2. **Type** — confirm our `SettingDef::Type` matches the native accessor (`GetBoolValue` / `GetIntValue` / `GetFloatValue` / `GetStringValue`). Mismatches → ERROR.
3. **Default** — confirm our `defaultValue` matches the native default. Mismatches → WARN unless behavior-breaking.
4. **Combo round-trip** — for `Combo` settings, identify what string DuckStation writes for each enum value. Our option INI values must match exactly. This is the highest-risk class of bug. Mismatches → ERROR.
5. **Range** — for `Int`/`Float`, sanity-check `minVal`/`maxVal`/`step` against any clamps in native code. Mismatches → WARN.

## Out of Scope

- Controller binding settings (separate concern; matches the PCSX2 audit's exclusion).
- Settings DuckStation supports but we don't expose (this is an audit, not an expansion).
- Code fixes (audit is read-only; fixes happen in a follow-up plan).
- PCSX2 (already done) and PPSSPP (separate audit).

## Output

A single markdown report at `docs/superpowers/audits/2026-04-06-duckstation-settings-audit.md`:

- **Summary table** — total settings, # OK / # WARN / # ERROR.
- **Findings, grouped by setting category** — only entries with issues are listed individually; OK entries are counted in the summary.
- Each finding includes: our schema entry → what native expects → severity → recommended fix (text only).
- Severity tags: `ERROR` (broken — wrong key/section, won't round-trip), `WARN` (suspicious — default mismatch, unclear range), `INFO` (minor).
- An appendix listing every audited setting (label + section/key + status) so the reviewer can confirm coverage.

## Process

Audit work runs in a research subagent to keep the main conversation context clean. The subagent does focused greps in `references/duckstation-master/` per setting rather than loading large files into the main context. The final report is written to disk and summarized back here for review.

## Success Criteria

- Every non-controller `SettingDef` in `duckstation_adapter.cpp` has been checked against native source.
- The report clearly identifies which settings are broken vs. suspicious vs. OK.
- Each ERROR/WARN names the exact native location (file + line or symbol) backing the finding.
