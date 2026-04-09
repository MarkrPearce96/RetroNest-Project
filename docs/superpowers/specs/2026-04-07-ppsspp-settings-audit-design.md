# PPSSPP Settings Audit — Design

**Date:** 2026-04-07
**Type:** Read-only audit (no code changes)
**Scope:** Non-controller `SettingDef`s in `cpp/src/adapters/ppsspp_adapter.cpp`

## Goal

Verify every non-controller setting our app exposes for PPSSPP round-trips correctly with the native emulator: that changing it in our UI actually changes the right key, in the right section, with a value PPSSPP will read back the same way.

## Why

Per `CLAUDE.md`, our app and PPSSPP share the same INI file as source of truth. PPSSPP is the riskiest of the three emulators audited so far for round-trip bugs because it routes many settings through a `ConfigTranslator` that rewrites enum values in custom formats (e.g. `GraphicsBackend = 3 (VULKAN)` rather than just `3`). Combo INI values that don't exactly match what PPSSPP writes back will fail to round-trip and the UI will silently fall back to defaults.

This audit follows the same methodology as the prior PCSX2 (2026-04-06) and DuckStation (2026-04-06) audits — see those reports under `docs/superpowers/audits/` for the exact format and level of detail expected.

## Methodology

For each `SettingDef` in `PPSSPPAdapter::settingsSchema()`, in source order:

1. **Existence** — locate `section` + `key` in `references/ppsspp-master/`. PPSSPP settings are typically declared in `Core/Config.cpp` via `ConfigSetting` entries inside per-section arrays (`generalSettings`, `graphicsSettings`, `soundSettings`, etc.). The Hungarian-style key prefixes (`b` = bool, `i` = int, `f` = float, `s` = string) must match between our schema and `Core/Config.cpp`. Missing key → ERROR.

2. **Type** — confirm our `SettingDef::Type` matches the native field type (`Bool`/`Int`/`Float`/`String`). Mismatches → ERROR.

3. **Default** — confirm our `defaultValue` matches the native default constant or third arg of the `ConfigSetting` entry. Mismatches → WARN unless behavior-breaking.

4. **Combo round-trip — HIGH RISK FOR PPSSPP.** PPSSPP has multiple round-trip pathways depending on the setting:
   - **Plain int settings** — read/written as the bare integer (e.g. `iAnisotropyLevel = 4`).
   - **`ConfigTranslator`-wrapped enum settings** — written with a human-readable annotation suffix (e.g. `GraphicsBackend = 3 (VULKAN)`). Our combo INI value MUST include the suffix exactly as PPSSPP writes it. This is the canonical PPSSPP round-trip bug class and is explicitly called out in `CLAUDE.md`.
   - **String enum settings** — written as a name string. Our values must match the canonical name PPSSPP writes.
   - For every Combo, locate the matching `ConfigSetting` in `Core/Config.cpp` and any associated translator/lookup table to determine the exact write format.

5. **Bitmask settings** — PPSSPP packs multiple booleans into single int keys (e.g. `iShowStatusFlags`). Our adapter uses `SettingDef::bitmask` for these. For each bitmask setting, verify the bit value matches what native PPSSPP code uses (look for `g_Config.iShowStatusFlags & (1 << N)` patterns or similar).

6. **Range** — for `Int`/`Float`, sanity-check `minVal`/`maxVal`/`step` against any clamps in native code. Mismatches → WARN.

## Out of Scope

- Controller binding settings (separate concern; matches the PCSX2/DuckStation audit exclusions).
- Settings PPSSPP supports but we don't expose (this is an audit, not an expansion).
- Code fixes (audit is read-only; fixes happen in a follow-up plan).
- PCSX2 and DuckStation (already audited).

## Output

A single markdown report at `docs/superpowers/audits/2026-04-07-ppsspp-settings-audit.md`:

- **Summary table** — total settings, # OK / # WARN / # ERROR.
- **Findings, grouped by setting category** — only entries with issues are listed individually; OK entries are counted in the summary.
- Each finding includes: our schema entry → what native expects → severity → recommended fix (text only).
- Severity tags: `ERROR` (broken — wrong key/section, won't round-trip), `WARN` (suspicious — default mismatch, unclear range), `INFO` (minor).
- A "Top issues" section highlighting the 2-3 most serious bugs.
- An appendix listing every audited setting (label + section/key + status) so coverage can be confirmed at a glance.

## Process

Audit work runs in a research subagent to keep the main conversation context clean. The subagent does focused greps in `references/ppsspp-master/` per setting rather than loading large files into the main context. The final report is written to disk and summarized back here for review.

## Success Criteria

- Every non-controller `SettingDef` in `ppsspp_adapter.cpp` has been checked against native source.
- The report clearly identifies which settings are broken vs. suspicious vs. OK.
- Each ERROR/WARN names the exact native location (file + line or symbol) backing the finding.
- Bitmask settings are individually verified against native bit-test sites.
- ConfigTranslator-wrapped enum combos get extra scrutiny since they're the highest-risk bug class for PPSSPP specifically.
