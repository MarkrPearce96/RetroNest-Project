# Settings UI & Setup Wizard Redesign — Design Spec

## Overview

Redesign all settings UI pages and the setup wizard to be controller/keyboard friendly while maintaining full mouse support. Replace the centered modal settings overlay with a right slide panel. Apply a new warm charcoal + amber color palette. Make interactions smoother with focus glow indicators, slide animations, and controller hint bars.

## Scope

### In Scope
- Settings overlay (layout + navigation model)
- Emulator manage page (list view)
- Emulator detail page (installed + not installed states)
- Paths settings page
- Scraper settings page (dashboard + progress display)
- Themes page
- Setup wizard (all 7 pages)
- Shared color palette
- Focus management system (controller/keyboard/mouse)

### Out of Scope
- Config page (ConfigSettings / ConfigForm) — per-emulator INI settings
- Controller mapping UI
- Hotkeys UI
- Theme engine / theme runtime behavior

## Color Palette

Warm charcoal base with amber/gold accent. Replaces the current cold blue-tinted dark palette.

| Token       | Hex       | Usage                                    |
|-------------|-----------|------------------------------------------|
| Background  | `#131210` | App background, dimmed areas             |
| Base        | `#1a1917` | Panel backgrounds behind content         |
| Surface     | `#201f1c` | Slide panel background, card containers  |
| Card        | `#282621` | Individual cards, list items, form fields |
| Border      | `#353330` | Default borders, dividers, unfocused edges |
| Accent      | `#e8a838` | Focus glow, active tabs, primary buttons, amber highlight |
| Success     | `#6a9b4a` | Installed badges, BIOS detected, scrape success |
| Error       | `#c85040` | Uninstall button, BIOS missing, scrape failed |
| Warning     | `#aa8844` | Reinstall/update buttons, optional status |
| Text        | `#e0ddd6` | Primary text                             |
| TextMuted   | `#8a8680` | Secondary text, labels                   |
| TextDim     | `#6a6660` | Tertiary text, descriptions              |
| TextFaint   | `#5a5650` | Hints, disabled text                     |
| TextGhost   | `#4a4640` | Controller hint text                     |

These palette tokens are for the settings UI and setup wizard only. They do not affect theme colors used by the main app (system browser, game list, etc.), which remain theme-controlled.

## Input Model

All redesigned pages support three input methods simultaneously:

### Controller
- **D-pad / Left Stick**: Moves focus between focusable items
- **A / Cross**: Confirm / select / toggle
- **B / Circle**: Go back / close
- **L1 / R1**: Switch tabs (emulator pill tabs in Paths), advance wizard pages
- **Start**: Select All (in scraper system/media selection)

### Keyboard
- **Arrow keys**: Move focus (same as D-pad)
- **Enter**: Confirm / select
- **Escape**: Go back / close settings
- **Tab**: Move between text input fields
- **Space**: Toggle checkboxes / pills
- Direct text input in focused text fields

### Mouse
- Fully functional with visible cursor in settings overlay
- Click, hover states, scroll wheel all work
- Hover highlights items (subtle background change)
- Mouse interaction does not break focus state — clicking an item also moves focus to it

## Focus System

### Focus Indicator
Amber glow border on the focused item:
```
border: 2px solid #e8a838
box-shadow: 0 0 15px rgba(232, 168, 56, 0.3)
```

### Focus Behavior
- Focus is always on exactly one item when settings/wizard is open
- D-pad up/down moves focus vertically through lists
- D-pad left/right moves focus horizontally (grids, pill tabs)
- Focus wraps at list boundaries (last item → first item)
- Entering a sub-page sets focus on the first focusable item
- Going back restores focus to the item that was selected
- When mouse clicks an item, focus moves to that item
- Focus movement is animated (smooth transition, ~150ms)

### Controller Hints Bar
Every page has a persistent hint bar at the bottom showing available actions:
- Format: `[icon] Action` pairs separated by spacing
- Examples: `↕ Navigate`, `🅰 Select`, `🅱 Back`, `L1/R1 Emulator`
- Hints update based on context (e.g., focused on a toggle shows "🅰 Toggle")
- Color: `#4a4640` (TextGhost)

## Settings Overlay

### Layout
- **Right slide panel** anchored to the right edge of the screen
- Width: ~50% of screen width
- Full height of screen
- Background: `Surface` (#201f1c)
- Left border: 1px `Border` (#2e2c28)
- Shadow: `-8px 0 40px rgba(0,0,0,0.5)` for depth
- The theme/game content remains visible (dimmed) on the left side behind a semi-transparent overlay

### Open/Close
- Opens when user presses Escape
- Slides in from the right edge with a smooth animation (~250ms ease-out)
- Closes on Escape again, or B/Circle, or clicking outside the panel
- Slides out to the right on close (~200ms ease-in)

### Category List (Home View)
Vertical list of category items, each showing:
- Icon (emoji)
- Title (14px, semibold)
- Subtitle/description (10px, muted)
- Background: `Card` (#282621), rounded corners (10px)
- Spacing: 8px between items

Categories:
1. Emulators — "Manage installations & BIOS"
2. Paths — "Configure folder locations"
3. Scraper — "Download metadata & artwork"
4. Themes — "Choose visual theme"

### Navigation Model
- **Replace-in-place**: Selecting a category slides the category list out to the left and slides the content page in from the right (~200ms)
- **B / Circle / Back button**: Slides content out to the right, slides category list back in from the left
- Header shows: back arrow button (left), page title (center/left)
- Back arrow button: 32x32px, `Card` background, rounded (6px), amber arrow icon

## Emulator Manage Page

### List View
Vertical list of emulators, each as a card:
- Logo area: 48x48px, `Border` background, rounded (8px)
- Name: 15px, medium weight
- System: 12px, `TextDim`
- Description: 11px, `TextFaint`
- Install status badge: pill shape (20px border-radius)
  - Installed: green text on dark green bg (`#6a9b4a` on `#1e2a1a`)
  - Not Installed: amber text on dark amber bg (`#e8a838` on `#2a2518`)
- Chevron: `›` in `TextGhost`, indicating drillable
- D-pad up/down to navigate, A to drill into detail

### Detail Page (Installed)
Slides in when selecting an emulator. Sections from top to bottom:

**Header**: Back button + emulator name + install status badge

**Logo**: 120x120px centered, `Card` background, rounded (12px)

**Info Section**:
- Section label: "INFO" (10px, uppercase, `TextMuted`)
- Card with key-value rows separated by subtle dividers:
  - System → e.g., "PlayStation 2"
  - Version → e.g., "v1.7.5432"
  - Description → e.g., "Open-source PlayStation 2 emulator"

**BIOS Section**:
- Section label: "BIOS" (10px, uppercase, `TextMuted`)
- Single status box (not individual file listing):
  - **Detected**: Green background tint, green dot (8px, glowing), "BIOS Detected" text in green
  - **Not Detected**: Red background tint, red dot (8px, glowing), "No BIOS Detected" text in red, plus "Open BIOS Folder" button inside the box

**Actions Section**:
- Section label: "ACTIONS" (10px, uppercase, `TextMuted`)
- Full-width buttons stacked vertically (6px gap):
  - **Emulator Settings** — accent button (`#e8a838` bg, dark text)
  - **Controller Mapping** — surface button (`Card` bg, light text)
  - **Hotkeys** — surface button
  - **Reinstall / Update** — warning button (amber text on dark amber bg)
  - **Reset Configuration** — surface button
  - **Uninstall** — danger button (red text on dark red bg)
- Each button: 12px padding, rounded (8px), 13px font
- Focus glow on the focused button

**Confirmation Dialogs**: Uninstall and Reset Configuration show a centered modal dialog with title, message, Cancel + Confirm buttons. Same warm palette styling.

### Detail Page (Not Installed)
Same header and logo. Reduced content:

**Info Section**: System and description only (no version)

**BIOS Section**: Neutral card with message: "BIOS files can be added after installation."

**Get Started Section**:
- Large accent button: "Install [Emulator Name]" (15px, 16px padding)
- Helper text below: "Downloads the latest release from GitHub" (11px, `TextFaint`)

## Paths Settings Page

### Layout
Inside the slide panel, replaces category list on selection.

**Emulator Pill Tabs** (top):
- Horizontal row of pill-shaped tabs, one per installed emulator
- Active tab: `Accent` background, dark text, rounded (20px)
- Inactive tab: `Card` background, `TextMuted` text, `Border` border
- Switch with L1/R1 shortcuts or D-pad left/right when tabs are focused
- Focus on tabs: amber glow around the active pill

**Path Cards** (scrollable area):
- One card per path (BIOS, Memory Cards, Save States, Screenshots, etc.)
- Each card contains:
  - Label: uppercase, 9px, `TextMuted` (e.g., "BIOS")
  - Value row: text field showing current path + "Browse" button
  - Text field: `Base` (#1a1917) background, `Border` border, path text in light color
  - Browse button: `Accent` text, `Card` background, rounded (4px)
- D-pad up/down between cards
- A on a focused card opens the file browser dialog
- Direct keyboard editing of path text when field is focused

**Bottom Bar**:
- Save button: accent style
- Reset button: surface style
- Controller hints on the right

## Scraper Settings Page

### Dashboard View (Single Page)
No multi-step flow. All configuration on one scrollable page:

**Account Section**:
- Section label: "ACCOUNT" (uppercase, `TextMuted`)
- Card showing username and connection status
  - Connected: green "Connected" text
  - Not connected: shows username + password text fields and "Sign In" button
- "Edit" link on the right when connected

**Systems Section**:
- Section label: "SYSTEMS" (uppercase, `TextMuted`)
- Toggle pills in a wrapping flow layout
- Selected: `Accent` background, dark text, rounded (12px)
- Unselected: `Border` background, `TextMuted` text
- D-pad navigates between pills, A toggles
- Start button: Select All / Deselect All

**Media Types Section**:
- Section label: "MEDIA" (uppercase, `TextMuted`)
- Same toggle pill pattern as systems

**Game Filter** (if applicable):
- Section label: "FILTER"
- Options: All Games, Missing Only, etc.

**Start Scraping Button**:
- Large accent button at bottom, full width
- Text: "Start Scraping" (centered, bold)
- 10px padding, rounded (6px)

### Progress View
Replaces dashboard content when scraping starts. Same slide panel.

**Header Area** (centered text):
- Title: "SCRAPING IN PROGRESS" (16px, bold, letter-spacing 1.5px)
  - Changes to "SCRAPING COMPLETE" in green when done
- System info: "PS2, GBA [2 SYSTEMS]" (12px, `TextMuted`)
- Game counter: "GAME 7 OF 23 — Game Name" (11px, `TextDim`)
- Progress bar: 4px height, `Border` track, `Accent` fill (animated width)
  - Turns green (`Success`) on completion

**Detail Card** (current game being scraped):
- Game title: 15px, bold, uppercase
- Two-column layout:
  - Left: Cover art (140x190px, rounded, showing downloaded image)
  - Right: Metadata rows (rating with stars, release date, developer, publisher, genre, players)
- Status indicator below metadata:
  - "Downloading media..." in muted
  - "✓ Scraped successfully" in green
  - "Failed: [reason]" in red
  - "Not available on ScreenScraper" in amber/warning
- Description: below the card content, separated by a subtle divider, scrollable

**Footer**:
- API quota: "API CALLS: 142 / 20000" (left, `TextFaint`)
- Progress counter: "7 / 23" (right)
- STOP button (red-styled) during scraping → becomes DONE button (amber-styled) when complete
- B / Circle also stops scraping (with confirmation)

## Themes Page

### Layout
Inside the slide panel, replaces category list on selection.

Vertical list of theme items:

**Each theme row**:
- Color preview swatch: 64x48px, rounded (6px), showing a hardcoded gradient per theme (derived from theme.json colors or manually assigned) with an accent-colored bar at the bottom
- Name: 14px, medium weight
- Description: 11px, `TextDim`
- Author: 10px, `TextFaint`, prefixed with "by"
- Right side:
  - **Active theme**: Green dot (8px, glowing) + "Active" text in green
  - **Inactive theme**: "Apply" button (surface style, 12px)
- Active theme row has a subtle amber-tinted background

**Interaction**:
- D-pad up/down to navigate
- A applies the focused theme (if not already active)
- Applying a theme shows the change immediately (live preview)

## Setup Wizard

### Layout
- **Wide centered card** on a dark radial gradient background (`#252320` center → `#131210` edge)
- Card: `Surface` background, rounded (14px), generous padding (24px 28px)
- Shadow: `0 8px 40px rgba(0,0,0,0.6)`
- Border: 1px `Border`
- Width: ~80% of screen, or sensible max-width for large screens

### Progress Indicator
- Thin progress bar at the top of the card (2px height)
- `Border` track, `Accent` fill
- Step counter in header: "3 / 7" (right-aligned, `TextDim`)

### Navigation
- **Hybrid model**:
  - L1/R1 (shoulder buttons) advance/go back between pages at any time
  - Focusable Back/Continue buttons at the bottom of each page
  - Continue button: accent style, labeled "Get Started" on page 1, "Continue" on middle pages, "Finish" on last page
  - Back button: surface style, hidden on first page
- Page transitions: content slides in from the right (forward) or left (backward), ~200ms

### Page 1: Welcome
- Title: large, centered, app name/logo
- Description: brief welcome text
- Single "Get Started" button

### Page 2: Choose Root Folder
- Title: "Choose Data Folder"
- Description: explains what the root folder is for
- Current path display in a card
- "Browse" button to open folder dialog
- A on the path card opens the browse dialog

### Page 3: Select Emulators
- Title: "Select Emulators"
- Description: "Choose which emulators to install"
- Grid of emulator cards (up to 3 columns)
- Each card: logo, name, system name, toggle selection with A
- Selected cards get amber border + check indicator
- D-pad navigates the grid

### Page 4: Resolution
- Title: "Display Resolution"
- Per-emulator sections (if multiple selected)
- Pill buttons for resolution options (e.g., Native, 2x, 3x, 4x)
- L1/R1 switch between emulators if shown as tabs
- D-pad left/right between pills, A to select

### Page 5: Aspect Ratio
- Title: "Aspect Ratio"
- Per-emulator sections
- Pill buttons for aspect ratio options (4:3, 16:9, etc.)
- Visual preview showing the selected ratio shape
- Same interaction pattern as Resolution page

### Page 6: Files (BIOS + ROMs Combined)
- Title: "Files"
- **BIOS sub-section**:
  - Per-emulator BIOS status (same simplified format: green detected / red missing)
  - "Open BIOS Folder" button
  - "Refresh" button
- **ROM Folders sub-section**:
  - List of system folders with paths
  - "Open ROM Folder" button
- All read-only/informational with action buttons

### Page 7: Install
- Title: "Installing" → "Setup Complete"
- Progress bar showing download/extraction progress
- Status text per emulator being installed
- On completion: "Finish" button to close wizard and enter the app

## Animations

### Slide Panel
- **Open**: Panel slides in from right edge, `250ms ease-out`. Background dims simultaneously.
- **Close**: Panel slides out to right, `200ms ease-in`. Background brightens.

### Page Transitions (within panel)
- **Forward** (entering a category/detail): Current content slides left + fades out, new content slides in from right + fades in. `200ms ease-out`.
- **Backward** (B/back): Reverse direction. Content slides right + fades out, previous content slides in from left. `200ms ease-out`.

### Focus Movement
- Focus glow transitions smoothly when moving between items (`150ms`).
- No abrupt border changes — the glow fades from one item and appears on the next.

### Wizard Page Transitions
- Same slide pattern as panel page transitions.
- Progress bar fill animates smoothly when advancing/going back.

### Button Press
- Subtle scale-down on press (0.97), spring back on release.
- Quick color change on press for tactile feedback.

## File Changes Required

### QML Files to Rewrite
- `cpp/qml/AppUI/SettingsOverlay.qml` — new slide panel layout + navigation
- `cpp/qml/AppUI/EmulatorsSettings.qml` — remove (replaced by EmulatorManagePage redesign)
- `cpp/qml/AppUI/EmulatorManagePage.qml` — new list view design
- `cpp/qml/AppUI/EmulatorManageGrid.qml` — replace grid with vertical list
- `cpp/qml/AppUI/EmulatorDetailPage.qml` — new detail layout with simplified BIOS
- `cpp/qml/AppUI/PathsSettings.qml` — new pill tabs + stacked cards layout
- `cpp/qml/AppUI/ScraperSettings.qml` — new single-page dashboard + progress redesign
- `cpp/qml/SetupWizard/Main.qml` — new wide card layout + progress bar
- `cpp/qml/SetupWizard/NavBar.qml` — updated styling + controller hints
- `cpp/qml/SetupWizard/StepIndicator.qml` — replace dots with progress bar
- `cpp/qml/SetupWizard/WelcomePage.qml` — restyle
- `cpp/qml/SetupWizard/FolderPage.qml` — restyle
- `cpp/qml/SetupWizard/EmulatorsPage.qml` — restyle
- `cpp/qml/SetupWizard/ResolutionPage.qml` — restyle
- `cpp/qml/SetupWizard/AspectRatioPage.qml` — restyle
- `cpp/qml/SetupWizard/BiosPage.qml` — merge with RomsPage into new FilesPage
- `cpp/qml/SetupWizard/RomsPage.qml` — merge into FilesPage (delete this file)
- `cpp/qml/SetupWizard/InstallPage.qml` — restyle
- `cpp/qml/SetupWizard/PillButton.qml` — add focus/keyboard support + restyle
- `cpp/qml/SetupWizard/EmulatorCard.qml` — add focus/keyboard support + restyle
- `cpp/qml/SetupWizard/WizardTheme.qml` — update with new palette colors

### New QML Files
- `cpp/qml/SetupWizard/FilesPage.qml` — combined BIOS + ROM folders page
- `cpp/qml/AppUI/FocusManager.qml` (or similar) — shared focus navigation logic if needed

### QML Files Unchanged
- `cpp/qml/AppUI/ConfigSettings.qml` — out of scope
- `cpp/qml/AppUI/ConfigForm.qml` — out of scope
- `cpp/qml/AppUI/AppWindow.qml` — may need minor edits for new overlay anchoring
- `cpp/qml/AppUI/BiosSection.qml` — may be simplified or removed (detail page handles BIOS differently now)

### C++ Files
- No C++ changes expected. All redesign work is QML-only.
- The existing C++ backend (app controller, theme manager, game service, scraper service) provides the same Q_INVOKABLE methods and signals.
- `BiosSection` component in QML is replaced by the simplified status indicator — no C++ change needed since `app.biosStatus(emuId)` still returns the data, we just display it differently.

## Visual Mockups

HTML mockups are saved in `.superpowers/brainstorm/` for reference:
- `settings-slide-panel.html` — slide panel layout options
- `color-palette.html` — accent color options
- `paths-page.html` — paths page approaches
- `scraper-page.html` — scraper dashboard approaches
- `wizard-design.html` — wizard card options
- `emulator-fullsize-v2.html` — emulator manage + detail at full size (final)
- `themes-fullsize.html` — themes page at full size
- `scraper-progress.html` — scraper progress display at full size
