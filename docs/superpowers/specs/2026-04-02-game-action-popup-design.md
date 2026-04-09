# Game Action Popup — Design Spec

## Overview

Replace the theme-local right-click context menu in `GameListPage.qml` with a shared, controller-friendly game action popup that lives in `qml/AppUI/` and is available to all themes automatically.

## Trigger

- **Controller:** Triangle (PS) / Y (Xbox) — injected as `Key_M` via `SdlInputManager`
- **Keyboard:** M key
- The popup opens when a game is highlighted/selected in a theme's game list

### SdlInputManager Mapping Addition

| Controller | Qt Key | Purpose |
|---|---|---|
| Triangle (PS) / Y (Xbox) | `Key_M` | Game action popup |

## GameActionPopup.qml

A new shared component in `qml/AppUI/GameActionPopup.qml`, hosted in `AppWindow.qml` alongside the other overlays.

### Visual Design

- Full-screen overlay with dark semi-transparent scrim
- Centered card with a vertical list of actions
- `z: 150` — above SettingsOverlay (100) but below VirtualKeyboard (200)

### Actions

1. **Scrape** — calls `themeContext.scrapeGame(gameId)`
2. **Toggle Favorite** — calls `themeContext.toggleFavorite(gameId)`. Label dynamically shows "Add to Favorites" or "Remove from Favorites" based on current state
3. **Open ROM Folder** — calls `themeContext.openGameRomFolder(gameId)`. Opens the containing folder in Finder/Explorer
4. **Remove from Library** — visually distinct (red text). Triggers confirmation sub-state before executing

### Navigation

- D-pad up/down (arrow keys) cycles through items
- A/Return selects the highlighted action
- B/Back or M dismisses the popup

### Remove Confirmation

Selecting "Remove from Library" swaps the action list for a confirmation view within the same popup:
- Text: "Delete ROM and remove from library?"
- Two buttons: Yes / No
- B/Back returns to the action list without removing
- Confirming calls `themeContext.removeGame(gameId)` which deletes the ROM file from disk and removes the database entry

### API

- `open(gameId)` — opens the popup for the given game
- `close()` — dismisses the popup
- Calls ThemeContext methods directly (no signals emitted back to themes)

## Favorite Behavior

### Sort Order (GameListModel)

- Favorited games sort to the top of the game list
- Within favorites: alphabetical order
- Within non-favorites: alphabetical order
- Sorting happens at the model level so all themes benefit automatically
- Re-sort triggered when favorite status changes

### Visual Indicator (Theme-Side)

- Themes render a star/indicator using the existing `favorite` role from GameListModel
- The modern theme adds a small star icon on the game cover/entry when `favorite === true`
- Other themes can use the same role however they choose

## ThemeContext & Backend Changes

### ThemeContext Additions

- `openGameActions(int gameId)` — Q_INVOKABLE method that emits a signal the popup listens to, passing the gameId
- `openRomFolder(int gameId)` — Q_INVOKABLE that resolves the ROM path from the database and opens the containing folder via `QDesktopServices::openUrl()`

### ThemeContext Modifications

- `removeGame(int gameId)` — updated to delete the ROM file from disk before removing the database entry

### GameListModel Changes

- Sort logic: favorites first (alphabetical), then non-favorites (alphabetical)
- Re-sort triggered when favorite status changes via `gamesChanged()` signal

## Theme Migration (Modern)

- Remove the right-click `Menu` from `GameListPage.qml`
- Add `Key_M` handler on the game list that calls `themeContext.openGameActions(gameId)` for the currently highlighted game
- Add star icon overlay on favorite games

## Files Affected

### New Files
- `qml/AppUI/GameActionPopup.qml` — the shared popup component

### Modified Files
- `cpp/src/core/sdl_input_manager.cpp` — add Triangle/Y → Key_M mapping
- `cpp/src/ui/theme_context.h` / `.cpp` — add `openGameActions()`, `openRomFolder()`, update `removeGame()`
- `cpp/src/ui/game_list_model.cpp` — favorite-first sort logic
- `qml/AppUI/AppWindow.qml` — host GameActionPopup
- `themes/modern/GameListPage.qml` — remove right-click menu, add Key_M handler, add favorite star
- `cpp/CMakeLists.txt` — add GameActionPopup.qml to resources
