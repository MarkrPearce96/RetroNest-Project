# Sync Emulator Default Settings to Native — Design

**Date:** 2026-04-07
**Type:** Data-only fix (no new code, no new mechanism)
**Scope:** `SettingDef::defaultValue` strings in `cpp/src/adapters/{pcsx2,duckstation,ppsspp}_adapter.cpp`

## Goal

Every `SettingDef::defaultValue` in our three adapters matches the corresponding emulator's standalone-install default value, so users get the same out-of-box experience whether they install through our app or natively.

## Why

The app currently has two reset paths plus the install path that all interact with default values, and they don't all converge to the same user-visible state because our `SettingDef::defaultValue` strings drift from what the emulator itself would use:

- **Install (`ensureConfig` → `createDefaultConfig`)** writes only embedding-critical keys to the INI. Every other setting is absent, so the emulator uses its native default at runtime — but our settings UI displays our `SettingDef::defaultValue` for those missing keys, which may not match. The user opens the settings page after install and sees a value that's different from what the emulator is actually doing.

- **`resetSettings(emuId)`** (in `app_controller.cpp`) writes `def.defaultValue` for every setting in the schema into the INI. So this writes our hard-coded defaults explicitly, again potentially diverging from native.

- **`resetConfiguration(emuId)`** deletes the INI entirely and re-runs `ensureConfig`. After reset, the emulator uses its native defaults at runtime. The settings UI shows our `defaultValue`, which (today) may not match.

After this fix, all three paths converge: the value the user sees in our UI immediately after install, the value written by "Reset Settings", the value implied by "Reset Configuration", and the value the emulator itself would use are all the same.

## Method (per adapter)

For each `SettingDef` in `settingsSchema()`, in source order:

1. Find the matching field in the native source:
   - PCSX2 → `references/pcsx2-master/pcsx2/Pcsx2Config.cpp` (`SettingsWrapEntry` declarations) and the corresponding `Config.h` field defaults
   - DuckStation → `references/duckstation-master/src/core/settings.cpp` (`Settings::Load` accessor calls and `DEFAULT_*` constants in `settings.h`)
   - PPSSPP → `references/ppsspp-master/Core/Config.cpp` (`ConfigSetting` constructor third argument)
2. If our `defaultValue` does not match the native default, update it.
3. Add a brief inline comment if the change is non-obvious or if the previous value was a deliberate UX override now being reverted.

## Explicit overrides (do NOT use native default)

Two settings per adapter are explicitly overridden to "native console res / aspect" rather than synced to the emulator's native default. The user accepts this divergence because the setup wizard overrides both at install time, so the schema default only matters as a fallback.

- **Internal resolution multiplier** — set to `1` (1x = native console pixel resolution) for:
  - PCSX2 `EmuCore/GS upscale_multiplier`
  - DuckStation `GPU ResolutionScale`
  - PPSSPP `Graphics InternalResolution`
- **Aspect ratio** — set to `4:3` for:
  - PCSX2 `EmuCore/GS AspectRatio`
  - DuckStation `Display AspectRatio`
  - PPSSPP — N/A (no `AspectRatio` entry in `settingsSchema()`; PPSSPP uses the wizard-only `aspectRatioOptions()` quick-settings which writes `Graphics DisplayAspectRatio` as a float)

## Deliberate UX overrides previously left in place — now reverted

The 2026-04-06 / 2026-04-07 audit fix passes deliberately left some default mismatches in place because we'd previously decided our value was a better UX choice than native. Per the user's instruction in this session, those overrides are now reverted in favour of strict native sync. The most prominent known case:

- **DuckStation `DeinterlacingMode`** — currently `"Adaptive"`, native default is `"Progressive"` (`DEFAULT_DISPLAY_DEINTERLACING_MODE` in `settings.h`). Will become `"Progressive"`.

Other similar cases (if any) will be identified during the per-adapter pass and updated without further consultation, since the directive is unambiguous: match native unless explicitly listed as an override above.

## Runtime-computed defaults (use static heuristic)

A small number of native defaults are computed at runtime and cannot be perfectly replicated in a static `defaultValue` string:

- **PPSSPP `AchievementVolume`** — native is `MultiplierToVolume100((float)iLegacyAchievementVolume / 10.0f)` with legacy default `6`. Working through the formula: `MultiplierToVolume100(0.6) = (0.6^(1/1.75)) * 100 + 0.5 ≈ 75`. The current schema value `"75"` is the correct runtime computation and needs no change. (An earlier draft of this spec said "60" — that was my arithmetic error during brainstorming, caught by the inventory subagent.)
- **DuckStation `Adapter`** — free-form string, default depends on host display detection. Already `""` after the round-trip audit fix.
- Any other display-dependent defaults found during the per-adapter pass: pick the most-common-case static value (typically the ≥1000px branch for desktop displays).

These cases get a brief inline comment explaining the heuristic so the next maintainer knows it's deliberate, not laziness.

## Out of scope

- **Code changes** beyond `defaultValue` string edits in the three adapter files. No new mechanism for runtime-queried defaults, no new reset path, no schema changes.
- **The two existing reset paths** (`resetSettings`, `resetConfiguration`). Both already work correctly. They just behave better after this fix because the values they reset to are now the right ones.
- **Controller bindings, hotkeys, and BIOS settings**. Same exclusion as the round-trip audits.
- **Settings the emulator supports but we don't expose**. Same exclusion as the round-trip audits.

## Per-adapter execution

- **Three commits**, one per adapter, each titled `fix({emuId}): sync default values to match native`. Independent and revertable.
- Each commit is the complete default-sync for that adapter — no tier splits because the work is small.
- All three adapters fixed in one branch (`fix/default-settings-sync`), merged together since they share the same goal and the smoke test covers all three at once.

## Verification

- Build clean after each commit (`cd cpp && cmake --build build`).
- No unit tests for adapter schemas (same situation as the audits).
- **Manual smoke test handed back to the user at the end**: for each emulator, open the settings page on a fresh install, confirm displayed defaults match a standalone install of that emulator's stable build. Hit "Reset Settings" and confirm same. Hit "Reset Configuration" and confirm same.

## Success criteria

- Every `SettingDef::defaultValue` in all three adapters either matches the emulator's native default exactly OR is documented as an explicit override (internal resolution, aspect ratio) OR is documented as a runtime-heuristic (handful of computed defaults).
- The build is clean and existing behaviour is unaffected for any setting the user has actively changed (since changed values are stored in the INI, not derived from `defaultValue`).
- Users see the same defaults in our UI that they would see in a standalone install of each emulator.
