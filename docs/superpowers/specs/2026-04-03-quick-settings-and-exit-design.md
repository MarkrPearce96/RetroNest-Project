# Quick Settings (Resolution, Aspect Ratio) & Exit — Design Spec

## Overview

Add three new entries to the settings overlay category list:

1. **Resolution** — quick resolution picker for installed emulators
2. **Aspect Ratio** — quick aspect ratio picker for installed emulators
3. **Exit** — close the application with confirmation

These provide fast access to the most common emulator display settings without navigating into per-emulator config pages.

## Settings Category List

Updated order (7 items total):

1. Emulators
2. Paths
3. Scraper
4. Themes
5. Resolution (new)
6. Aspect Ratio (new)
7. Exit (new)

## Resolution Page (`ResolutionSettings.qml`)

### Layout

A scrollable page containing one **card** per installed emulator that has resolution options. Cards flow horizontally and wrap (flex-wrap grid, ~420px per card, ~28px gap).

Each card contains:
- **Emulator label** — system name (e.g. "PlayStation 2") + emulator name (e.g. "PCSX2") in smaller muted text
- **Preview image** — 14:9 aspect ratio, rounded corners (8px radius), dark placeholder background. The `Image` source is driven by the currently selected option, making it easy to add per-option preview images later by mapping option values to asset paths
- **Pill buttons** — horizontal row below the image, one per resolution option (e.g. 720P, 1080P, 1440P, 4K). Selected pill is highlighted in accent color (#e8922a). Pills use flex: 1 for equal width

No card background — just the label, image, and pills directly on the page surface.

### Behavior

- Only installed emulators with non-empty `resolutionOptions()` are shown
- Selecting a pill updates **local QML state only** (pending choices map)
- The preview image source updates immediately when a pill is selected
- **Save button** at the bottom-right applies all pending changes at once via `app.applyQuickResolution(choices)`
- Current selection is read from the emulator's INI on page load via `app.currentResolution(emuId)`

### Controller/Keyboard Navigation

Uses a 2D focus index: `focusCard` (vertical) and `focusPill` (horizontal within card).
- Up/Down moves between cards
- Left/Right moves between pills within the focused card
- Return/Enter selects the focused pill
- Cards use `FocusableItem` for focus glow

## Aspect Ratio Page (`AspectRatioSettings.qml`)

Identical layout and behavior to the Resolution page, but:
- Uses `aspectRatioOptions()` from adapters (e.g. "4:3", "16:9")
- Save calls `app.applyQuickAspectRatio(choices)`
- Current selection read via `app.currentAspectRatio(emuId)`

### Multi-Key Patching

Aspect ratio options can patch multiple INI keys per selection. For example, PCSX2's 16:9 option sets both `AspectRatio=16:9` and `EnableWideScreenPatches=true`. The adapter's `AspectRatioOptions` struct already bundles these — `applyQuickAspectRatio()` iterates all key/value pairs in the selected option and writes them all.

## Exit Category Item

### Behavior

- Selecting "Exit" from the category list does **not** push a page
- Instead, it shows a **confirmation dialog** centered over the settings overlay
- Dialog text: "Exit Application?"
- Two buttons: "Cancel" (returns to category list) and "Exit" (calls `Qt.quit()`)
- Dialog follows the overlay's dark styling (surface background, accent-colored Exit button)
- Dialog is dismissable via Escape/B-button (same as Cancel)

## C++ Backend Changes (AppController)

New `Q_INVOKABLE` methods:

### Resolution

- `QVariantList quickResolutionOptions(const QString& emuId)` — wraps `adapter->resolutionOptions()`, returns list of `{label, value}` maps
- `QString currentResolution(const QString& emuId)` — reads the resolution INI key for the emulator, returns the current value
- `void applyQuickResolution(const QVariantMap& choices)` — takes `{emuId: value}` map, writes each value to the adapter's resolution INI section/key

### Aspect Ratio

- `QVariantList quickAspectRatioOptions(const QString& emuId)` — wraps `adapter->aspectRatioOptions()`, returns list of `{label, keys}` maps where `keys` is the list of INI patches for that option
- `QString currentAspectRatio(const QString& emuId)` — reads the first INI key for each option to determine which is currently active, returns the matching label
- `void applyQuickAspectRatio(const QVariantMap& choices)` — takes `{emuId: label}` map, looks up the matching option's key/value pairs and writes them all

## SettingsOverlay.qml Changes

- `categoryCount`: 4 → 7
- Add three `ListElement` entries to the category `ListModel`:
  - `{name: "Resolution", icon: "...", subtitle: "Quick resolution settings", catIndex: 4}`
  - `{name: "Aspect Ratio", icon: "...", subtitle: "Quick aspect ratio settings", catIndex: 5}`
  - `{name: "Exit", icon: "...", subtitle: "Close the application", catIndex: 6}`
- Update header text mapping for indices 4 ("Resolution"), 5 ("Aspect Ratio")
- Update `selectCategory()`:
  - Index 4 → push `resolutionPageComponent`
  - Index 5 → push `aspectRatioPageComponent`
  - Index 6 → show exit confirmation dialog (no page push)
- Add three new `Component` blocks:
  - `resolutionPageComponent` → `ResolutionSettings {}`
  - `aspectRatioPageComponent` → `AspectRatioSettings {}`
  - Exit confirmation dialog (inline or component)

## New QML Files

- `cpp/qml/AppUI/ResolutionSettings.qml`
- `cpp/qml/AppUI/AspectRatioSettings.qml`

Both added to CMakeLists.txt AppUI QML module resources.

## Preview Image System

Each card's image area uses a QML `Image` element with a source computed from the selected option. The image source mapping is a simple JS object in each page, e.g.:

```qml
property var resolutionImages: ({
    "pcsx2": { "2": "images/res/pcsx2-720p.png", "3": "images/res/pcsx2-1080p.png", ... },
    "duckstation": { ... }
})
```

If no image is mapped for an option, the placeholder (dark background) is shown. This makes it straightforward to add images later — just drop files into `qml/AppUI/images/res/` (or similar) and fill in the mapping object.
