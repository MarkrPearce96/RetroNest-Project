# PPSSPP Resolution Options — 4-Option Set

**Date:** 2026-04-09
**Status:** Approved

## Goal

Replace PPSSPP's current resolution lists (11 options in the settings schema,
6 options in the setup wizard) with the same 4-option set used by PCSX2 and
DuckStation — labeled `720P / 1080P / 1440P / 4K`. This unifies the resolution
UX across all three emulators.

## Mapping

PSP's native resolution is 480×272, so PPSSPP uses integer multipliers of that
base. No multiplier lands exactly on 720p/1080p/1440p/4K, so each label picks
the closest sensible multiplier:

| Label | `InternalResolution` value | Actual pixels | Notes |
|-------|---------------------------|---------------|-------|
| 720P  | `3` | 1440×816  | Closest to 720p height |
| 1080P | `4` | 1920×1088 | Essentially 1080p |
| 1440P | `6` | 2880×1632 | Essentially 1440p |
| 4K    | `8` | 3840×2176 | Essentially 4K (3840×2160) |

**Default:** `3` (720P).

Rationale for choosing real-pixel mapping over mirroring PCSX2/DuckStation's
nominal `2/3/4/6` multipliers: PPSSPP's 2× is only 960×544, which is really
540p. Labeling it "720P" would make the label lie. With the mapping above the
named resolution is always the one the user actually gets.

## Changes

All edits are in `cpp/src/adapters/ppsspp_adapter.cpp`.

### 1. `settingsSchema()` (line 94–100)

Replace the 11-entry combo options list with the 4 new `{label, value}` pairs.
Change the default from `"1"` to `"3"`.

Before:

```cpp
s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
          "Rendering resolution multiplier.",
          SettingDef::Combo, "1",
          {{"Auto (1:1)", "0"}, {"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"},
           {"3x (1440x816)", "3"}, {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"},
           {"6x (2880x1632)", "6"}, {"7x (3360x1904)", "7"}, {"8x (3840x2176)", "8"},
           {"9x (4320x2448)", "9"}, {"10x (4800x2720)", "10"}}, 0, 0, 0});
```

After:

```cpp
s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
          "Rendering resolution multiplier.",
          SettingDef::Combo, "3",
          {{"720P", "3"}, {"1080P", "4"}, {"1440P", "6"}, {"4K", "8"}}, 0, 0, 0});
```

### 2. `resolutionOptions()` (line 576–581)

Replace the 6-entry wizard list with the same 4 entries. Change the
`defaultValue` from `"2"` to `"3"`.

Before:

```cpp
ResolutionOptions PPSSPPAdapter::resolutionOptions() const {
    return {"Graphics", "InternalResolution",
            {{"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"}, {"3x (1440x816)", "3"},
             {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"}, {"10x (4800x2720)", "10"}},
            "2"};
}
```

After:

```cpp
ResolutionOptions PPSSPPAdapter::resolutionOptions() const {
    return {"Graphics", "InternalResolution",
            {{"720P", "3"}, {"1080P", "4"}, {"1440P", "6"}, {"4K", "8"}},
            "3"};
}
```

## Stale-value behavior

Both call sites read/write the same INI key (`[Graphics] InternalResolution`).
If a user's existing `ppsspp.ini` contains a value that isn't in the new set
(e.g. `2`, `5`, `10`), the settings-page combo won't match any option and the
existing schema fallback behavior takes over — the user sees the default
selected in the UI, and the next save rewrites the INI to one of the four
values. No migration step is needed.

## Out of scope

- PCSX2 and DuckStation resolution options are unchanged.
- No changes to the `ResolutionOptions` struct, `ResolutionPage.qml`, or any
  QML/wizard flow.
- No migration of existing `ppsspp.ini` files.

## Testing

Manual verification:

1. Build the app.
2. Open Settings → Graphics → Rendering Resolution. Confirm the combo shows
   exactly 4 options: 720P, 1080P, 1440P, 4K.
3. Open the Setup Wizard resolution page. Confirm PPSSPP shows the same 4
   options, with 720P selected by default.
4. Select each option in turn and save. Confirm `ppsspp.ini` writes the
   expected integer (`3`, `4`, `6`, `8`) to `[Graphics] InternalResolution`.
