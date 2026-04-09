# Move Empty State from Theme to App Layer

**Date:** 2026-04-02
**Status:** Design approved

## Problem

The "No games found" empty state (background image, action buttons, focus navigation) currently lives in the theme layer (`themes/modern/SystemPage.qml`). This means every new theme must re-implement it. The empty state is an app concern — it should exist once in the app layer.

Additionally, `GameListPage.qml` contains a "No games found" text placeholder that is dead code — if a system has no games, it never appears in the system list.

## Design

### Approach

App-level `EmptyStatePage.qml` in the AppWindow StackView. AppWindow conditionally shows the empty state or the theme's systemBrowser based on whether any systems with games exist. Themes only load when there are games to display.

### New file: `qml/AppUI/EmptyStatePage.qml`

A self-contained QML page containing:

- **Background:** The retro arcade image (`_default.webp`), moved from `themes/modern/assets/artwork/` to `qml/AppUI/images/`. Uses `Image.PreserveAspectCrop` with a dark scrim overlay (`#1a1a2e` at 0.85 opacity).
- **Content:** Centered column with:
  - "No games found" heading (26px, bold, white)
  - Instruction text explaining how to add ROMs
  - "Open ROM Folder" primary button (purple: `#6b6be6`)
  - "Scan for Games" and "Import ROMs" secondary buttons (side by side)
- **Actions:** Buttons call `app.openRomFolder()`, `app.scanRomFolders()`, `app.importRoms()` directly — no themeContext dependency.
- **Focus navigation:** `emptyFocusIndex` property (0 = Open ROM Folder, 1 = Scan, 2 = Import). Arrow keys move between buttons, Return/Enter activates. Same pattern as the current theme implementation.
- **Controller hints:** Navigation hint text at the bottom ("↑ ↓ Navigate    Enter Select").

### AppWindow routing changes (`qml/AppUI/AppWindow.qml`)

- **Startup:** Check `themeContext.systems.length`. If 0, push `EmptyStatePage.qml`. Otherwise, push the theme's systemBrowser as before.
- **Transition to theme:** Listen for `themeContext.onSystemsChanged` / `themeContext.onGamesChanged`. When `systems.length` goes from 0 to >0, replace the StackView content with the theme's systemBrowser page.
- **Transition to empty:** If systems drop back to 0 (all games removed), replace with the empty state page.
- **Theme changes:** Only swap theme pages if currently showing a theme page (not the empty state).

### AppController changes (`cpp/src/ui/app_controller.h/.cpp`)

- Add `Q_INVOKABLE void openRomFolder()` — same logic as `ThemeContext::openRomFolder()`: resolve `Paths::romsDir()`, ensure it exists, open with `QDesktopServices::openUrl()`.

### Theme cleanup

**`themes/modern/SystemPage.qml`:**
- Remove the entire empty state block (~lines 348–523): the `emptyState` Rectangle, its buttons, scrim, and all related code.
- Remove `emptyFocusIndex` property and `activateEmptyButton()` function.
- Remove empty-state branches from keyboard handlers (the `systemList.length === 0` checks in `Keys.onLeftPressed`, `onRightPressed`, `onUpPressed`, `onDownPressed`, `onReturnPressed`, `onEnterPressed`).
- The page can now assume `systemList.length > 0` always.

**`themes/modern/GameListPage.qml`:**
- Remove the "No games found" placeholder text (~line 576). This is dead code — systems with zero games don't appear in the system list.

### Asset changes

- Copy `themes/modern/assets/artwork/_default.webp` → `qml/AppUI/images/empty-state-bg.webp`
- Add to CMakeLists.txt AppUI resources section
- Keep the original in the theme — it is still used as a fallback background when a system's artwork image fails to load (`SystemPage.qml` lines 95, 115)

## Files changed

| File | Change |
|------|--------|
| `qml/AppUI/EmptyStatePage.qml` | **New** — app-level empty state page |
| `qml/AppUI/images/empty-state-bg.webp` | **New** — moved from theme assets |
| `qml/AppUI/AppWindow.qml` | Conditional routing: empty state vs theme |
| `cpp/src/ui/app_controller.h` | Add `openRomFolder()` |
| `cpp/src/ui/app_controller.cpp` | Implement `openRomFolder()` |
| `cpp/CMakeLists.txt` | Add EmptyStatePage.qml + image to resources |
| `themes/modern/SystemPage.qml` | Remove empty state block (~170 lines) |
| `themes/modern/GameListPage.qml` | Remove dead "No games found" text |
