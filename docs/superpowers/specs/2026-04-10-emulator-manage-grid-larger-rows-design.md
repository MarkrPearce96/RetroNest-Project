# Emulator Manage Grid — Larger Rows

**Date:** 2026-04-10
**Status:** Approved

## Goal

Scale the rows on the Emulators management page (`EmulatorManageGrid.qml`) up
by approximately 1.5× so each emulator entry is easier to read. The page
currently shows three installed emulators plus a "Coming Soon" placeholder;
the user reports the rows are hard to see at the current size.

Pure visual tuning — no structural changes, no behavioral changes.

## Scope

Single file touched: `cpp/qml/AppUI/EmulatorManageGrid.qml`.

Two row templates inside that file need the same set of changes:

1. The `Repeater` delegate for real emulators (roughly lines 49–156).
2. The "More Emulators / Coming Soon" placeholder row (roughly lines 160–213).

Both rows must stay visually consistent, so every measurement change is
applied to both.

## Sizing changes

| Element | Before | After |
|---|---|---|
| Row `Layout.preferredHeight` | 72 | 108 |
| Logo Rectangle size | 48×48 | 72×72 |
| Logo Rectangle `radius` | 8 | 10 |
| Logo Image inner margin (`width: parent.width - N`) | `-10` (so 38×38) | `-12` (so 60×60) |
| Logo fallback emoji `font.pixelSize` (installed row) | 22 | 32 |
| Logo fallback emoji `font.pixelSize` (placeholder row) | 20 | 32 |
| Title Text `font.pixelSize` | 15 | 22 |
| System Text `font.pixelSize` | 12 | 16 |
| Description Text `font.pixelSize` | 11 | 15 |
| Badge Rectangle `height` | 26 | 36 |
| Badge Rectangle horizontal padding (`badgeLabel.width + N`) | `+20` | `+28` |
| Badge Text `font.pixelSize` | 11 | 15 |
| Chevron `font.pixelSize` | 20 | 26 |
| Inner `RowLayout.anchors.leftMargin` / `rightMargin` | 14 | 18 |
| Inner `RowLayout.spacing` | 14 | 18 |

All other properties on the affected elements (colors, weights, binding
expressions, layout attached properties) stay exactly as they are.

## Out of scope

- The page header ("Emulators" at the top), the back arrow, and the hint bar
  at the bottom — these are rendered by the parent and not touched.
- Focus ring and hover styling (`FocusableItem`) — unchanged. It already
  adapts to whatever `Layout.preferredHeight` the row has.
- `EmulatorLogos.js` and the PNG assets themselves — unchanged. The existing
  `fillMode: Image.PreserveAspectFit` + `mipmap: true` + `smooth: true` on
  the Image node already handle the larger display size cleanly.
- No changes to any other QML file, no C++, no CMake.
- No changes to `Keys.onUpPressed` / `Keys.onDownPressed` navigation or
  the `MouseArea` click behavior — these remain structural and are not
  touched.

## Testing

Manual verification (pure visual tweak, so GUI-only):

1. Build the app.
2. Navigate to Settings → Emulators.
3. Confirm each row is visibly larger:
   - Row height approximately 108px.
   - Logo tile is 72×72 with the logo image filling 60×60 inside.
   - Title text is clearly larger (22px vs 15px).
   - System and description lines are legible (16px and 15px).
   - "Installed" / "Not Installed" badge text is readable (15px) inside a
     36px-tall pill with extra horizontal padding.
4. Move focus up/down with keyboard and a controller. Confirm the focus ring
   still wraps each row cleanly with no clipping or overshoot.
5. Confirm the chevron still renders on the right edge of each row without
   clipping or overlapping the badge.
6. Confirm the "Coming Soon" placeholder row visually matches the three
   installed rows in height and text scale.
