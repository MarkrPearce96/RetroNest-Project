# Dynamic Button Hints — Design Spec

## Overview

Add a dynamic button hint system that displays contextual button instructions across all pages. Hints switch automatically between Xbox, PlayStation, and keyboard labels based on the connected controller type and last input device. The hints live in the app layer (not themes) so they persist across theme changes.

## Goals

- Show contextual button hints on every navigable page
- Dynamically switch between Xbox / PlayStation / Keyboard labels based on input
- Unknown controllers default to Xbox labels
- Hints live in the app layer — themes never need to implement them
- No background bar — floating glyphs with text shadow for readability
- Styled as rounded key/button glyphs resembling physical keys/buttons

## C++ Changes: Controller Type Detection

### SdlInputManager additions

Add a `ControllerType` enum and `controllerType` Q_PROPERTY:

```cpp
enum ControllerType { Keyboard, Xbox, PlayStation };
Q_ENUM(ControllerType)
Q_PROPERTY(int controllerType READ controllerType NOTIFY controllerTypeChanged)
```

**Detection logic:** In `openController()`, parse `SDL_GameControllerName()` (case-insensitive) for keywords:
- `"ps3"`, `"ps4"`, `"ps5"`, `"dualshock"`, `"dualsense"`, `"playstation"` → `PlayStation`
- Everything else → `Xbox`

**Per-controller tracking:** Store the detected type per instance ID in a `QMap<SDL_JoystickID, ControllerType> m_controllerTypes`. When a button press arrives in `pollEvents()`, update the active controller type from that controller's entry.

**Combined with lastInputWasController:** The `controllerType` property returns `Keyboard` when `lastInputWasController` is `false`, otherwise returns the active controller's detected type. Emit `controllerTypeChanged` whenever the effective type changes (input mode switch or different controller used).

## QML Component: ButtonHints.qml

New file: `cpp/qml/AppUI/ButtonHints.qml`

### Interface

```qml
Item {
    property var hints: []  // [{action: "confirm", label: "Select"}, ...]
}
```

### Action Vocabulary

Fixed set of action IDs that map to glyphs per input mode:

| Action | Xbox | PlayStation | Keyboard |
|--------|------|-------------|----------|
| `confirm` | A (green) | ✕ (blue) | Enter (grey) |
| `back` | B (red) | ○ (pink) | Esc (grey) |
| `action` | Y (yellow) | △ (teal) | M (grey) |
| `delete` | X (blue) | ▢ (pink) | Backspace (grey) |
| `navigate_lr` | D-Pad ◂▸ (grey) | D-Pad ◂▸ (grey) | ←→ (grey) |
| `navigate_ud` | D-Pad ▴▾ (grey) | D-Pad ▴▾ (grey) | ↑↓ (grey) |
| `start` | Start (grey) | Start (grey) | Esc (grey) |

### Glyph Styling

- Rounded rectangles (~4px radius) with colored backgrounds per button type
- Xbox face buttons: green (A), red (B), yellow (Y), blue (X)
- PlayStation face buttons: blue (✕), pink (○), teal (△), pink (▢)
- Keyboard keys and D-Pad/Start: uniform dark grey background (#333) with light text (#ccc)
- Text shadow on labels for readability over any background content
- Font: small (11-12px), weight 600-700 for glyphs, 500 for labels

### Layout

- `Row` of glyph+label pairs, centered horizontally
- Spacing: ~20px between hint pairs, ~5px between glyph and label
- No background bar — transparent, floating over content

## Placement

### Theme Pages (AppWindow.qml)

A single `ButtonHints` instance anchored to bottom center of the window:

- **Z-order:** 50 (above themes, below settings overlay at 100 and game action popup at 150)
- **Bottom margin:** ~16-20px from screen edge
- **Visibility:** Hidden when `settingsOverlay.visible` or `gameActionPopup.visible`
- **Hint set** determined by page context:

| Context | Hints |
|---------|-------|
| Empty state | `start` Settings |
| SystemPage (`mainStack.depth === 1`, not empty state) | `navigate_lr` Browse, `confirm` Select, `start` Settings |
| GameListPage (`mainStack.depth > 1`) | `navigate_ud` Browse, `confirm` Launch, `action` Actions, `back` Back, `start` Settings |

### Settings Overlay (SettingsOverlay.qml)

Replace existing `ControllerHints` with `ButtonHints` at the same position (bottom of panel column):

| Context | Hints |
|---------|-------|
| Category list | `navigate_ud` Navigate, `confirm` Select, `back` Close |
| Sub-pages (default) | `navigate_ud` Navigate, `confirm` Select, `back` Back |
| Scrape progress (running) | `back` Stop |
| Scrape progress (complete) | `confirm` Done, `back` Back |

The scrape progress context is detected by checking if the current `panelStack.currentItem` has a `scrapeRunning` property (only ScraperSettings exposes this). When `scrapeRunning === true`, show the "running" hints. When `scrapeRunning === false` and the page is still the scraper (i.e. `progressTotal > 0`), show the "complete" hints. All other settings sub-pages use the default sub-page hints.

### Game Action Popup (GameActionPopup.qml)

A `ButtonHints` instance inside the popup, anchored to the bottom:

| Context | Hints |
|---------|-------|
| Game actions | `navigate_ud` Navigate, `confirm` Select, `back` Close |

## Existing Code Changes

- **ControllerHints.qml:** Remove or keep as deprecated. The new `ButtonHints.qml` replaces it entirely.
- **SettingsOverlay.qml:** Swap `ControllerHints` usage for `ButtonHints` with appropriate hint arrays.
- **AppWindow.qml:** Add the floating `ButtonHints` instance with visibility/hint-set logic.
- **GameActionPopup.qml:** Add `ButtonHints` instance.
- **sdl_input_manager.h/.cpp:** Add `ControllerType` enum, `controllerType` property, per-controller type tracking, detection logic.
- **CMakeLists.txt:** Add `ButtonHints.qml` to resources, remove `ControllerHints.qml` if deleted.

## Out of Scope

- Custom glyph images/icons (using styled text for now)
- Per-theme hint customization (hints are app-layer only)
- VirtualKeyboard hints (already has its own key guidance)
