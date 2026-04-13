# PCSX2 Settings — Button Hints & Key Behavior

## Overview

Add a button/key instruction row to the bottom of the PCSX2 settings description bar, matching the visual style of the QML `ButtonHints.qml` component used throughout the rest of the app. Also fix key behavior so Escape acts as hierarchical back (like B/Circle everywhere else) and L1/R1 map to Tab/Shift+Tab for sub-tab cycling.

## 1. Hints Row Inside Description Bar

Extend `Pcsx2DescriptionBar` to include a horizontal row of hint pills below the description text and recommended value.

### Hint data model

Each hint is a struct: `{ action, label }` where action determines the glyph/color and label is the display text (e.g. "Navigate", "Select", "Back").

### Visual style (matches QML ButtonHints.qml exactly)

- **Pill shape:** Rounded rectangle, radius 5px, 28px tall, width = glyph text width + 16px padding
- **Glyph text:** Bold, 14px (18px for PlayStation symbols and arrow glyphs)
- **Label text:** `#dddddd`, 14px medium weight, to the right of the pill
- **Spacing:** 20px between hint groups
- **Layout:** Centered horizontally in the description bar
- **Separation:** Margin above the hints row to visually separate from description text

### Colors per input type

**Xbox (controllerType == 1):**
| Action | Glyph | BG | FG | Border |
|--------|-------|-----|-----|--------|
| confirm | A | #2a5c2a | #6ddc6d | #3a7a3a |
| back | B | #5c2a2a | #dc6d6d | #7a3a3a |
| navigate | D-Pad arrows | #333333 | #cccccc | #555555 |
| switch_tab | LB / RB | #333333 | #cccccc | #555555 |

**PlayStation (controllerType == 2):**
| Action | Glyph | BG | FG | Border |
|--------|-------|-----|-----|--------|
| confirm | ✕ | #2a3a6a | #6d9ddc | #3a5a8a |
| back | ○ | #5c2a3a | #dc6d8d | #7a3a5a |
| navigate | D-Pad arrows | #333333 | #cccccc | #555555 |
| switch_tab | L1 / R1 | #333333 | #cccccc | #555555 |

**Keyboard (controllerType == 0):**
All pills use grey: `bg:#333333 / fg:#cccccc / border:#555555`. Glyphs: Enter, Esc, arrow symbols, Tab.

### Input type reactivity

The description bar listens to `SdlInputManager::controllerTypeChanged()` and repaints the hints row when the user switches between keyboard and controller input.

## 2. Key Behavior Changes

### Escape — hierarchical back

- In `Pcsx2SettingsDialog::keyPressEvent`, intercept `Key_Escape` and call `popPage()` instead of letting QDialog close the dialog.
- On the hub page, `popPage()` already calls `accept()` when history is empty, so Escape from the hub closes the dialog naturally.
- Also handle `Key_Back` (B button injection) to trigger `popPage()` — this makes B/Circle work as back throughout settings.

### Remove Backspace as back

- Remove the existing `Key_Backspace` handler in `Pcsx2SettingsDialog::keyPressEvent`. Escape/B now handles back exclusively.

### L1/R1 — map to Tab/Shift+Tab

- In `SdlInputManager::mapButtonToKey()`, add:
  - `SDL_CONTROLLER_BUTTON_LEFTSHOULDER` -> `Qt::Key_Backtab`
  - `SDL_CONTROLLER_BUTTON_RIGHTSHOULDER` -> `Qt::Key_Tab`
- The Graphics page already handles `Key_Tab`/`Key_Backtab` in its event filter to cycle sub-tabs, so this works immediately.
- On pages without sub-tabs, suppress Tab/Backtab in the dialog's key handler so they don't accidentally move widget focus.

## 3. Per-Page Hint Definitions

Each page type gets its own hint set. The dialog updates the description bar's hints when pushing/popping pages.

### Hub page
| Hint | Action |
|------|--------|
| D-Pad up/down | Navigate |
| A / ✕ / Enter | Select |
| B / ○ / Esc | Close |

### Emulation, Audio, Memory Cards pages (no sub-tabs)
| Hint | Action |
|------|--------|
| D-Pad all directions | Navigate |
| A / ✕ / Enter | Select |
| B / ○ / Esc | Back |

### Graphics page (has sub-tabs)
| Hint | Action |
|------|--------|
| D-Pad all directions | Navigate |
| A / ✕ / Enter | Select |
| L1 / R1 / Tab | Switch Tab |
| B / ○ / Esc | Back |

### Mechanism

Pages expose a `buttonHints()` method returning a `QVector<ButtonHint>` struct list. The dialog calls this when pushing a page and passes the result to the description bar. The hub has its own hints set directly by the dialog.

## 4. Files Changed

| File | Change |
|------|--------|
| `widgets/pcsx2_description_bar.h/cpp` | Add hints row, input-type-aware pill rendering, `setHints()` method |
| `pcsx2_settings_dialog.h/cpp` | Change Escape to hierarchical back, remove Backspace handler, add Key_Back handler, pass hints on page push/pop, suppress Tab on non-tabbed pages |
| `core/sdl_input_manager.cpp` | Map L1->Backtab, R1->Tab in `mapButtonToKey()` |
| `pcsx2_category_hub.cpp` | No changes needed (hub hints set by dialog) |
| Pages (emulation, audio, memory cards, graphics) | Add `buttonHints()` method returning appropriate hints |
