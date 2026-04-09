# PCSX2 Settings Audit — Design

**Date:** 2026-04-06
**Type:** Read-only audit (no code changes)
**Scope:** Non-controller `SettingDef`s in `cpp/src/adapters/pcsx2_adapter.cpp`

## Goal

Verify every non-controller setting our app exposes for PCSX2 round-trips correctly with the native emulator: that changing it in our UI actually changes the right key, in the right section, with a value PCSX2 will read back the same way.

## Why

Per `CLAUDE.md`, our app and PCSX2 share the same INI file as source of truth. If our `SettingDef` references a wrong section/key, or our `Combo` option strings don't match the format PCSX2 writes back to disk, the UI silently falls back to defaults and user changes appear to "not stick." This class of bug is invisible until a user notices, so a systematic audit is the only way to catch it.

## Methodology

For each `SettingDef` in `PCSX2Adapter::settingsSchema()`, in source order:

1. **Existence** — locate `section` + `key` in `references/pcsx2-master/` (typically `pcsx2/Config.h`, `pcsx2/Pcsx2Config.cpp`, or per-component config files). If not found, ERROR.
2. **Type** — confirm our `SettingDef::Type` matches the native accessor (`GetBoolValue` / `GetIntValue` / `GetFloatValue` / `GetStringValue` / enum-as-string).
3. **Default** — confirm our `defaultValue` matches the native default. Mismatches are WARN unless they break behavior.
4. **Combo round-trip** — for `Combo` settings, identify what string PCSX2 writes for each enum value. Our option INI values must match exactly. Mismatches are ERROR.
5. **Range** — for `Int`/`Float`, sanity-check `minVal`/`maxVal`/`step` against any clamps in native code. Mismatches are WARN.

## Out of Scope

- Controller binding settings (separate audit, separate format concerns).
- Settings PCSX2 supports but we don't expose (this is an audit, not an expansion).
- Code fixes (audit is read-only; fixes happen in a follow-up).
- DuckStation and PPSSPP (separate per-emulator audits).

## Output

A single markdown report at `docs/superpowers/audits/2026-04-06-pcsx2-settings-audit.md`:

- **Summary table** — total settings, # OK / # WARN / # ERROR.
- **Findings, grouped by setting category** — only entries with issues are listed individually; OK entries are counted in the summary.
- Each finding includes: our schema entry → what native expects → severity → recommended fix (text only).
- Severity tags: `ERROR` (broken — wrong key/section, won't round-trip), `WARN` (suspicious — default mismatch, unclear range), `INFO` (minor).

## Process

Audit work runs in a research subagent to keep the main conversation context clean — the subagent does focused greps in `references/pcsx2-master/` per setting rather than loading large files into the main context. The final report is written to disk and summarized back here for review.

## Success Criteria

- Every non-controller `SettingDef` in `pcsx2_adapter.cpp` has been checked against native source.
- The report clearly identifies which settings are broken vs. suspicious vs. OK.
- Each ERROR/WARN names the exact native location (file + line or symbol) backing the finding, so a follow-up fix can verify independently.
