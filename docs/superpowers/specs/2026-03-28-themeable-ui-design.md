# Themeable UI & Rich Metadata — Design Spec

## Context

The emulator frontend currently has a functional but basic QML UI: a tab bar (Games/Settings), a sidebar system filter, and a game grid showing cover art and titles. Game metadata is limited to title and cover art. The goal is to transform this into an immersive, ES-DE-style experience where:

- The UI is fullscreen with no chrome — themes own the entire window
- Users browse systems via a carousel and drill into game lists with rich metadata
- Different themes can provide completely different page layouts
- Settings are accessed via an Escape-key overlay, not a tab

This requires changes across three layers: the data layer (database + scraper + models), the theme infrastructure (ThemeManager, ThemeContext, Loader), and the UI itself (new app shell + default theme).

## Architecture: Loader-Based Theme Swapping

### App Shell (Compiled QML — not themed)

The app shell is minimal:

- `AppWindow.qml` — fullscreen window containing a `StackView` that loads theme pages, plus the settings overlay
- `SettingsOverlay.qml` — modal popup triggered by Escape key, shows a grid of setting categories (Emulators, Config, Paths, Scraper, Controllers, Themes). Clicking a category opens the corresponding settings page within the overlay. Escape again closes it.
- `StatusBar.qml` — optional, can overlay the bottom edge for transient status messages

**Removed:** `TopBar.qml`, the tab-based Games/Settings navigation, and the `StackLayout` switching between GamesPage and SettingsPage.

### Theme Contract (New C++ Classes)

#### ThemeManager

Scans `themes/` directory at startup. Each subdirectory with a valid `theme.json` is a theme.

**Responsibilities:**
- Scan and parse `theme.json` manifests
- Track the current active theme (persisted in database or config)
- Resolve page names to file URLs: `resolve("systemBrowser")` → `file:///path/to/themes/modern/SystemPage.qml`
- Expose available themes list to QML (for the Themes settings page)

**Exposed to QML as context property `themeManager`:**
- `availableThemes` — QVariantList of {id, name, author, description, previewImage}
- `currentThemeId` — read/write QString
- `resolve(pageName)` — Q_INVOKABLE returning QUrl

#### ThemeContext

Thin facade over AppController and GameListModel. This is the **only** API themes interact with.

**Exposed to QML as context property `themeContext`:**

Properties:
- `systems` — QStringList of system IDs that have games
- `systemNames` — QVariantMap mapping system ID → display name (e.g., "psx" → "PlayStation")
- `systemGameCounts` — QVariantMap mapping system ID → game count
- `currentSystem` — read/write QString
- `gameModel` — GameListModel pointer (the expanded model with all metadata roles)

Methods (Q_INVOKABLE):
- `navigateToSystem(systemId)` — sets currentSystem + signals theme to transition
- `navigateBack()` — signals theme to go back to system browser
- `launchGame(gameId)` — delegates to AppController
- `scrapeGame(gameId)` — delegates to AppController
- `scrapeAll()` — delegates to AppController
- `removeGame(gameId)` — delegates to AppController
- `gameDetails(gameId)` — returns QVariantMap with all metadata fields for one game
- `importRoms()` — delegates to AppController (file dialog)
- `scanRomFolders()` — delegates to AppController

Signals:
- `systemChanged()` — emitted when currentSystem changes
- `gamesChanged()` — emitted when game list is modified (import, remove, scrape)

### theme.json Format

```json
{
  "name": "Modern",
  "version": "1.0",
  "author": "Built-in",
  "description": "Clean carousel and grid layout with detail panel",
  "minAppVersion": "1.0",
  "preview": "preview.png",
  "pages": {
    "systemBrowser": "SystemPage.qml",
    "gameList": "GameListPage.qml"
  }
}
```

Required pages: `systemBrowser`, `gameList`. The app shell loads these via `themeManager.resolve()`.

### Theme Directory Structure

```
themes/
  modern/
    theme.json
    SystemPage.qml        — full-screen system carousel
    GameListPage.qml      — game grid + right-side detail panel
    components/            — theme-private QML components
      GameCard.qml
      DetailPanel.qml
      CarouselItem.qml
    assets/                — images, fonts, backgrounds
      preview.png
```

### How Theme Loading Works

In `AppWindow.qml`:

```qml
ApplicationWindow {
    visibility: Window.FullScreen  // or Window.Maximized

    StackView {
        id: mainStack
        anchors.fill: parent
        initialItem: themeManager.resolve("systemBrowser")
    }

    SettingsOverlay {
        id: settingsOverlay
        visible: false
    }

    Shortcut {
        sequence: "Escape"
        onActivated: settingsOverlay.visible = !settingsOverlay.visible
    }
}
```

Theme pages navigate by calling `themeContext.navigateToSystem(systemId)` and `themeContext.navigateBack()`. The app shell listens for these signals and drives the StackView:

```qml
Connections {
    target: themeContext
    function onNavigateToSystemRequested(systemId) {
        mainStack.push(themeManager.resolve("gameList"))
    }
    function onNavigateBackRequested() {
        mainStack.pop()
    }
}
```

This keeps themes decoupled from the StackView — they call ThemeContext methods, the shell handles the actual page transitions.

## Data Layer Changes

### Database Migration v1 → v2

Add columns to the `games` table:

| Column | Type | Default | Source |
|--------|------|---------|--------|
| description | TEXT | '' | ScreenScraper `jeu.synopsis` |
| developer | TEXT | '' | ScreenScraper `jeu.developpeur` |
| publisher | TEXT | '' | ScreenScraper `jeu.editeur` |
| release_date | TEXT | '' | ScreenScraper `jeu.dates` |
| genres | TEXT | '' | ScreenScraper `jeu.genres` (comma-separated) |
| rating | REAL | 0.0 | ScreenScraper `jeu.note` |
| players | TEXT | '' | ScreenScraper `jeu.joueurs` |
| last_played | TEXT | '' | Set by app on game launch |
| play_count | INTEGER | 0 | Incremented by app on game launch |
| favorite | INTEGER | 0 | User toggle |

Migration uses the existing schema version system in `database.cpp`.

### GameRecord Struct Expansion

File: `cpp/src/core/database.h`

Add all new fields to match the database columns.

### Scraper Enhancement

File: `cpp/src/core/scraper.cpp`

The ScreenScraper API response (`jeuInfos`) already contains all metadata. Currently only `jeu.noms` (title) and `jeu.medias` (cover art) are extracted. Add extraction for:

- `jeu.synopsis` → description (prefer English, fall back to any language)
- `jeu.developpeur.text` → developer
- `jeu.editeur.text` → publisher
- `jeu.dates[0].text` → release_date
- `jeu.genres[].noms[].text` → genres (comma-joined)
- `jeu.note.text` → rating (normalize to 0.0–5.0 scale)
- `jeu.joueurs` → players

### GameListModel Role Expansion

File: `cpp/src/ui/game_list_model.h`

Add roles for all new metadata fields:

- DescriptionRole, DeveloperRole, PublisherRole, ReleaseDateRole
- GenresRole, RatingRole, PlayersRole
- LastPlayedRole, PlayCountRole, FavoriteRole

## Default "Modern" Theme

### SystemPage.qml — Carousel

- Full-screen horizontal carousel
- Center system: large card with system artwork/logo, system name, game count
- Adjacent systems: smaller, faded, partially visible
- Navigation: left/right arrow keys, mouse click on arrows, mouse wheel
- Enter or click center system: push GameListPage onto StackView with transition
- Dot indicators at bottom showing position
- Data source: `themeContext.systems`, `themeContext.systemNames`, `themeContext.systemGameCounts`

### GameListPage.qml — Grid + Detail Panel

- Back button (top-left) pops StackView back to carousel
- System name + game count in header
- Toolbar: Scan, Import, Scrape All buttons
- Main area: cover art grid (responsive columns based on window width)
- Single-click a game: right-side detail panel slides in, grid shrinks
- Detail panel shows: cover art, title, developer + year, star rating, genre tags, description, player count, launch button
- Double-click or Launch button: `themeContext.launchGame(gameId)`
- Right-click: context menu (Scrape, Remove, Toggle Favorite)
- Data source: `themeContext.gameModel` with all metadata roles

## Settings Overlay

Part of the app shell, not themed. Triggered by Escape key.

- Modal overlay with dimmed background
- Grid of category cards: Emulators, Configuration, Paths, Scraper, Controllers, Themes
- Clicking a category loads the corresponding settings page within the overlay
- Each settings page reuses existing QML settings components (EmulatorManagePage, PathsSettings, ScraperSettings, etc.) or launches Qt Widget dialogs
- "Themes" category: list of available themes with previews, click to switch
- Escape closes the overlay, returning to the theme page underneath

## Files to Create

| File | Purpose |
|------|---------|
| `cpp/src/ui/theme_manager.h/.cpp` | Theme discovery, parsing, resolution |
| `cpp/src/ui/theme_context.h/.cpp` | Facade API for themes |
| `cpp/qml/AppUI/SettingsOverlay.qml` | Escape-key settings modal |
| `themes/modern/theme.json` | Theme manifest |
| `themes/modern/SystemPage.qml` | Carousel system browser |
| `themes/modern/GameListPage.qml` | Grid + detail panel |
| `themes/modern/components/*.qml` | Theme-private components |

## Files to Modify

| File | Change |
|------|--------|
| `cpp/src/core/database.h/.cpp` | GameRecord expansion + schema migration v1→v2 |
| `cpp/src/core/scraper.cpp` | Extract full metadata from ScreenScraper response |
| `cpp/src/ui/game_list_model.h/.cpp` | New roles for all metadata fields |
| `cpp/src/main.cpp` | Wire ThemeManager + ThemeContext as context properties |
| `cpp/qml/AppUI/AppWindow.qml` | Remove TopBar, replace StackLayout with StackView + Loader, add SettingsOverlay + Escape shortcut |
| `cpp/CMakeLists.txt` | Add new C++ files, update QML module |

## Files to Remove/Deprecate

| File | Reason |
|------|--------|
| `cpp/qml/AppUI/TopBar.qml` | No longer needed — fullscreen UI |
| `cpp/qml/AppUI/GamesPage.qml` | Replaced by theme's SystemPage + GameListPage |
| `cpp/qml/AppUI/SystemSidebar.qml` | Replaced by theme's system browser |
| `cpp/qml/AppUI/GameGridView.qml` | Replaced by theme's game list |
| `cpp/qml/AppUI/GameCard.qml` | Replaced by theme's game card component |

These can be kept temporarily as reference or moved into `themes/classic/` as a legacy theme.

## Verification Plan

1. **Database migration:** Run app with existing database, verify v2 migration adds columns without data loss. Check that existing games still load.
2. **Scraper metadata:** Scrape a known game, verify all metadata fields populate in the database. Check the GameListModel exposes them via roles.
3. **Theme loading:** Place the modern theme in `themes/modern/`, verify ThemeManager discovers it and resolves pages to correct URLs.
4. **Carousel:** Launch app, verify system carousel displays with correct system names and game counts. Test left/right navigation.
5. **Game list:** Select a system, verify page transition. Click a game, verify detail panel with metadata. Double-click to launch.
6. **Settings overlay:** Press Escape, verify overlay appears. Navigate to each settings category. Press Escape to close.
7. **Theme switching:** Add a second minimal theme, switch via Settings > Themes, verify pages reload with new theme.
