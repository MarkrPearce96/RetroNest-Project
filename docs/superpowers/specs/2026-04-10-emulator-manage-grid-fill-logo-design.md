# Emulator Manage Grid — Fill Logo Tile With Rounded Clipping

**Date:** 2026-04-10
**Status:** Approved

## Goal

Make each emulator logo on the Settings → Emulators management page fill its
72×72 tile completely, with the logo's corners clipped to match the tile's
rounded corners. Today the logo `Image` sits inside the tile with 12px
padding on each side, revealing the tile's `SettingsTheme.border`-coloured
background as a visible grey ring around the logo. The user wants that ring
gone — the logo should *be* the rounded tile.

## Scope

Single file: `cpp/qml/AppUI/EmulatorManageGrid.qml`. Only the real-emulator
`Repeater` delegate is affected. The "Coming Soon" placeholder row has no
`Image` (just a `\u2795` emoji `Text`) and is unchanged.

## Approach — reuse the existing project pattern

The project already clips images to rounded corners in
`cpp/qml/AppUI/EmulatorDetailPage.qml:215-236`, using the
`Qt5Compat.GraphicalEffects` `OpacityMask` pattern:

1. Invisible `Image` node — the logo (`visible: false`)
2. Invisible `Rectangle` with the desired `radius` — the mask shape
3. `OpacityMask` that paints the `Image` clipped to the `Rectangle`'s shape

We reuse that pattern in `EmulatorManageGrid.qml` for consistency.

## Changes

### 1. Add the import

At the top of `cpp/qml/AppUI/EmulatorManageGrid.qml`, alongside the existing
`import` lines, add:

```qml
import Qt5Compat.GraphicalEffects
```

### 2. Replace the delegate's logo Image

Inside the `Repeater` delegate's logo `Rectangle`, replace the current
padded `Image` node with the OpacityMask triplet.

Before (current):

```qml
Image {
    anchors.centerIn: parent
    width: parent.width - 12
    height: parent.height - 12
    source: EmulatorLogos.logoForEmu(modelData.id)
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
    visible: source !== ""
}
```

After:

```qml
Image {
    id: logoImg
    anchors.fill: parent
    source: EmulatorLogos.logoForEmu(modelData.id)
    fillMode: Image.PreserveAspectCrop
    smooth: true
    mipmap: true
    visible: false
}

Rectangle {
    id: logoMask
    anchors.fill: parent
    radius: 10
    visible: false
}

OpacityMask {
    anchors.fill: parent
    source: logoImg
    maskSource: logoMask
    visible: EmulatorLogos.logoForEmu(modelData.id) !== ""
}
```

Notes on the mode change:
- `PreserveAspectFit` → `PreserveAspectCrop`. With Crop, the logo always
  fills the full 72×72 tile; if a logo PNG isn't square, its longer edge
  gets cropped rather than leaving the tile's background peeking through.
  The current DuckStation / PCSX2 / PPSSPP PNGs are square-shaped so Crop
  and Fit produce identical results for them, but Crop is safer for any
  future non-square logo asset.
- `anchors.fill: parent` replaces the `parent.width - 12` padding, removing
  the visible grey ring.
- The `Image` becomes `visible: false` because `OpacityMask` takes over
  rendering. The `Rectangle` mask source is also `visible: false` — it
  exists only as a geometry/radius source for the mask shader.
- The `OpacityMask`'s `visible` binding on `EmulatorLogos.logoForEmu(...)`
  ensures nothing renders when a logo is missing; the existing fallback
  emoji `Text` shows through on top of the Rectangle background in that
  case.

### 3. Fallback emoji — unchanged

The `Text { text: "\uD83C\uDFAE" ... visible: EmulatorLogos.logoForEmu(modelData.id) === "" }`
node stays exactly as it is. In the fallback case the `OpacityMask` is
hidden and the tile background + emoji show through normally.

### 4. Logo tile Rectangle — unchanged

The outer `Rectangle { width: 72; height: 72; radius: 10; color: SettingsTheme.border }`
is not touched. When a logo is present the masked image fully covers it;
when a logo is missing the Rectangle serves as the background for the
fallback emoji.

## Out of scope

- `EmulatorLogos.js` and the PNG assets themselves.
- The "Coming Soon" placeholder row.
- The `Repeater` delegate's badge, chevron, title, focus ring, click
  handling, or any other element outside the logo tile.
- Any other QML file, C++, or CMake change.

## Testing

Manual verification (GUI-only, user-side):

1. Build the app.
2. Navigate to Settings → Emulators.
3. For each of the three installed emulators (DuckStation, PCSX2, PPSSPP),
   confirm the logo fills the entire 72×72 tile with no visible grey ring
   around it, and that the corners are rounded cleanly (no square overflow,
   no jagged clipping).
4. Confirm the "Coming Soon" placeholder row with the `+` emoji still
   renders normally — it should be visually unchanged from before this
   change.
5. Move focus Up/Down with keyboard and a controller — confirm the focus
   ring still wraps each row cleanly (regression check).
6. Temporarily simulate a missing logo (optional, only if easy) — e.g. by
   browsing with a new emulator that has no PNG yet — and confirm the
   fallback `🎮` emoji still renders inside the tile.
