# Empty State Redesign — Design Spec

## Problem

The "No games found" empty state on `SystemPage.qml` has several issues:
1. **Text is invisible** — white text over a busy retro room background image with no contrast treatment
2. **Instructions are unclear** — "Add ROMs to your system folders and scan to get started" doesn't guide the user through the actual steps
3. **Missing action** — no way to open the ROM folder directly from this screen
4. **No keyboard/controller navigation** — buttons are mouse-only (MouseArea with no focus handling)

## Solution

Redesign the empty state overlay in `themes/modern/SystemPage.qml` with improved readability, clearer options, and full keyboard navigation.

## Design Decisions

### Visual Treatment: Full Dark Scrim
- Semi-transparent dark overlay covers the entire background image
- Guarantees text readability regardless of background artwork
- Background image is retained (user requirement) but dimmed
- Subtle purple radial gradient at center for visual warmth

### Button Layout: Stacked Hierarchy
- **Primary action (top):** "Open ROM Folder" — large purple button, visually prominent
- **Secondary actions (below, side-by-side):** "Scan for Games" + "Import ROMs" — subdued dark purple style
- This layout guides the natural workflow: open folder → add ROMs → scan

### Navigation: Standard Focus Pattern
- Uses `FocusableItem.qml` for focus glow, consistent with rest of app
- Up/Down arrows move between primary button and secondary row
- Left/Right arrows move between the two secondary buttons
- Enter/Return activates the focused button
- Controller input works automatically via the unified input system (key injection)

## Changes Required

### 1. ThemeContext — Add `openRomFolder()` method

**File:** `cpp/src/ui/theme_context.h` and `cpp/src/ui/theme_context.cpp`

Add a new `Q_INVOKABLE void openRomFolder()` method that:
- Gets the ROM directory path from `Paths::romsDir()` (or equivalent)
- Opens it in the system file manager via `QDesktopServices::openUrl(QUrl::fromLocalFile(path))`
- Creates the directory first if it doesn't exist

This follows the same pattern as `WizardState::openFolder()`.

### 2. SystemPage.qml — Redesign Empty State

**File:** `themes/modern/SystemPage.qml` (lines ~319-403)

Replace the existing empty state `Column` with:

#### Scrim overlay
- `Rectangle` covering the full parent
- Color: dark purple/blue gradient, ~85% opacity
- Subtle radial accent gradient layered on top
- Only visible when `systemList.length === 0`

#### Content layout
- Centered `Column` containing:
  - **Title:** "No games found" — white, bold, 26px
  - **Instructions:** "Add ROMs to your system folders, then scan to discover them. Use Open ROM Folder to find the right directory." — white at 70% opacity, 14px, centered, 1.6 line height
  - **Primary button:** "Open ROM Folder"
    - Purple background (#5b5bd6), white text, 15px font, bold
    - Hover: lighter purple (#6b6be6)
    - Wrapped in `FocusableItem` for focus glow
    - `Keys.onReturnPressed` / `Keys.onEnterPressed` → `themeContext.openRomFolder()`
    - `Keys.onDownPressed` → move focus to "Scan for Games"
  - **Secondary button row:** `Row` containing:
    - "Scan for Games" — dark background (rgba 44,44,78,0.8), border, 14px
      - `Keys.onReturnPressed` → `themeContext.scanRomFolders()`
      - `Keys.onRightPressed` → move focus to "Import ROMs"
      - `Keys.onUpPressed` → move focus to "Open ROM Folder"
    - "Import ROMs" — same style
      - `Keys.onReturnPressed` → `themeContext.importRoms()`
      - `Keys.onLeftPressed` → move focus to "Scan for Games"
      - `Keys.onUpPressed` → move focus to "Open ROM Folder"
  - **Nav hint:** small text "↑ ↓ Navigate   Enter Select" at ~35% opacity

#### Focus management
- When empty state becomes visible, auto-focus the primary button ("Open ROM Folder")
- `focusIndex` pattern (0 = Open ROM Folder, 1 = Scan for Games, 2 = Import ROMs)
- Arrow key handlers update `focusIndex` and call `forceActiveFocus()` on the target button

### 3. Remove "Press Escape to open Settings"

The hint text at the bottom of the current empty state is removed — it's not relevant to the empty state workflow and adds visual clutter.

## Out of Scope

- Changes to the `GameListPage.qml` simple empty state (different context, different page)
- Changes to the background image itself
- Any settings or configuration changes
