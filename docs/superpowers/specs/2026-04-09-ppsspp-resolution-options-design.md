# PPSSPP Resolution Options — 4-Option Set (Quick Resolution Page + Wizard)

**Date:** 2026-04-09
**Status:** Implemented (commits `0445ad4` + `800d204`)

## Goal

Change PPSSPP's **quick Resolution settings page** and **setup wizard
resolution page** to expose the same 4-option labeled set used by PCSX2 and
DuckStation — `720P / 1080P / 1440P / 4K` — so the quick resolution UX is
consistent across all three emulators.

The **full Graphics → Rendering Resolution combo** in the per-emulator Graphics
settings page is deliberately **unchanged** — it continues to expose the full
Auto + 1x–10x multiplier list for power users.

## Scope: two UI surfaces, one INI key

PPSSPP exposes `[Graphics] InternalResolution` through two separate UI
surfaces in RetroNest, driven by two independent code paths in the adapter:

| Surface | Driven by | Scope of this change |
|---|---|---|
| Quick "Resolution" settings page (4 cards, one per installed emulator) | `PPSSPPAdapter::resolutionOptions()` | **Changed to 4 options** |
| Setup Wizard → resolution page | `PPSSPPAdapter::resolutionOptions()` (same method) | **Changed to 4 options** |
| Per-emulator Settings → Graphics → Rendering Resolution combo | `PPSSPPAdapter::settingsSchema()` | **Unchanged (keeps 11 options)** |

Both surfaces read/write the same INI key, so values chosen in one place are
observable in the other. See "Stale-value behavior" below for how mismatches
between the two option sets are handled.

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

### `resolutionOptions()` — only this method changes

Replace the 6-entry wizard list with the 4-entry labeled set. Change the
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

### `settingsSchema()` — NOT changed

The `InternalResolution` entry in `settingsSchema()` keeps its full option
list (Auto + 1×–10×) and its original default of `"1"`. The full per-emulator
Graphics settings page is the place for power users to pick any supported
multiplier, including exotic values like 7× or 9× that don't fit a labeled
preset.

## Stale-value behavior

Because the quick Resolution page exposes only 4 of the 11 possible values,
users can land in a state where the underlying INI key holds a value that
isn't in the quick page's option set:

- **User picks `7x (3360x1904)` in Graphics → Rendering Resolution**, then
  opens the quick Resolution page. The quick page's combo won't match any
  option and falls through to its default (`720P`). If the user saves on the
  quick page, the INI is overwritten to `3`. If they don't save, the `7`
  value stays intact.

This is the accepted behavior: the quick page is a convenience surface for
common picks, and the Graphics page remains the source of truth for the full
range. No migration or cross-page synchronization is performed.

## Out of scope

- PCSX2 and DuckStation resolution options are unchanged.
- The full Graphics → Rendering Resolution combo in PPSSPP's settings schema
  is unchanged.
- No changes to the `ResolutionOptions` struct, `ResolutionPage.qml`, or any
  QML/wizard flow.
- No migration of existing `ppsspp.ini` files.

## Testing

Manual verification:

1. Build the app.
2. Open Settings → Resolution (the quick picker). Confirm PPSSPP's card shows
   exactly 4 options: 720P, 1080P, 1440P, 4K, with 720P as the default.
3. Open Settings → Graphics → Rendering Resolution. Confirm the combo still
   shows the **full** Auto + 1×–10× list (11 options), with 1× as the default.
4. Open the Setup Wizard resolution page. Confirm PPSSPP shows 720P / 1080P /
   1440P / 4K with 720P selected by default.
5. Select each of the 4 quick-page options in turn and save. Confirm
   `ppsspp.ini` writes the expected integer (`3`, `4`, `6`, `8`) to
   `[Graphics] InternalResolution`.
6. On the Graphics page, select `7x (3360x1904)`, save, reopen the quick
   Resolution page, and confirm it falls back to the default (720P) display
   without overwriting the INI until the user saves.

## Implementation history

- **`0445ad4`** — Initial implementation changed **both** `resolutionOptions()`
  and `settingsSchema()` to the 4-option set. This was too aggressive: it
  removed the full multiplier list from the per-emulator Graphics settings
  page, which power users rely on.
- **`800d204`** — Reverted the `settingsSchema()` change while keeping
  `resolutionOptions()` at the 4-option set. This is the final intended state
  and matches the scope documented in this spec.
