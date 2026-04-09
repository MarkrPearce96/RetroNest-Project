# RetroAchievements Integration — Design Spec

## Overview

Integrate RetroAchievements (RA) into EmuFront so users can log into their RA account,
have achievements enabled in supported emulators, and browse achievement data (per-game
and across their library) entirely within the app's UI.

**Approach:** The emulators (DuckStation, PCSX2) handle all runtime achievement logic
natively. EmuFront stores RA credentials, patches them into emulator INI files, and uses
the RA Web API to fetch and display achievement data. No rcheevos library or emulator
memory inspection needed.

## Credentials & Authentication

### Storage

New class `RetroAchievementsCredentials` (mirrors `ScraperCredentials` pattern).

- File: `{config}/retroachievements.json`
- Fields: `username` (string), `token` (string — connect token), `apiKey` (string — web API key)
- Methods: `load()`, `save()`, `clearUser()`, `hasUserCredentials()`
- The user's password is used only for the initial login call and is never stored.

### Login Flow

1. User enters username + password in the RA settings page.
2. App calls RA standalone API: `GET https://retroachievements.org/dorequest.php?r=login2&u={username}&p={password}`
3. On success, response contains `Token` (connect token) and `Score`/`SoftcoreScore`.
4. App stores username + token. For web API calls, the connect token can be used as the
   API key (`y` parameter) — the RA web API accepts connect tokens in place of the
   separate web API key. This means the user only needs to enter username + password.
5. Credentials are persisted to `retroachievements.json`.

### Emulator INI Patching

Credentials are injected into emulator INI files during `ensureConfig()`, alongside
existing embedding-critical keys.

**DuckStation** — `settings.ini`, `[Cheevos]` section:

| Key | Value | Purpose |
|-----|-------|---------|
| `Enabled` | `true` | Master RA toggle |
| `Username` | `{username}` | RA account |
| `Token` | `{token}` | Connect token |
| `ChallengeMode` | `true`/`false` | Hardcore mode |
| `Notifications` | `true`/`false` | Unlock popups |
| `SoundEffects` | `true`/`false` | Unlock sounds |

**PCSX2** — `PCSX2.ini`, `[Achievements]` section + `secrets.ini`, `[Achievements]` section:

| File | Key | Value | Purpose |
|------|-----|-------|---------|
| `PCSX2.ini` | `Enabled` | `true` | Master RA toggle |
| `PCSX2.ini` | `HardcoreMode` | `true`/`false` | Hardcore mode |
| `PCSX2.ini` | `Notifications` | `true`/`false` | Unlock popups |
| `PCSX2.ini` | `SoundEffects` | `true`/`false` | Unlock sounds |
| `secrets.ini` | `Username` | `{username}` | RA account |
| `secrets.ini` | `Token` | `{token}` | Connect token |

**When no RA credentials are stored**, achievements remain disabled (`Enabled = false`).
Patching only occurs when the user has logged in.

### Adapter Changes

Each adapter gains new methods/logic:

- `patchRetroAchievements(username, token, hardcore, notifications, sounds)` — writes
  the RA keys into the appropriate INI file(s). Called from `ensureConfig()`.
- PCSX2 adapter must handle the separate `secrets.ini` file for credentials.
- The base `EmulatorAdapter` can declare a virtual `supportsRetroAchievements()` method
  (returns `false` by default). DuckStation and PCSX2 adapters override to return `true`.

## RA Web API Client

### New Class: `RetroAchievementsClient`

Located in `core/`. Uses `QNetworkAccessManager` (same as `GitHubClient` and `Scraper`).

**Authentication:** All web API calls include `y={apiKey}&z={username}` as query parameters.

### Endpoints Used

**Game ID Resolution:**
```
GET https://retroachievements.org/API/API_GetGameList.php?i={consoleId}&h=1&y={apiKey}
```
Returns all games for a console with their ROM hash arrays. Used to build the
local ROM hash → RA game ID mapping.

**Console IDs:** RA assigns numeric IDs to each console. Relevant mappings:
- PlayStation 1 = 12
- PlayStation 2 = 21

A static lookup in the client maps EmuFront system IDs (`psx`, `ps2`) to RA console IDs.

**Per-Game Achievements + User Progress:**
```
GET https://retroachievements.org/API/API_GetGameInfoAndUserProgress.php?g={gameId}&u={username}&y={apiKey}&a=1
```
Returns:
- Game metadata (title, icon, images)
- `NumAwardedToUser`, `NumAwardedToUserHardcore`, `UserCompletion`
- `Achievements` object keyed by ID, each with: `ID`, `Title`, `Description`, `Points`,
  `TrueRatio`, `Type`, `BadgeName`, `DateEarned`, `DateEarnedHardcore`

**User Profile/Summary:**
```
GET https://retroachievements.org/API/API_GetUserSummary.php?u={username}&y={apiKey}&g=0&a=5
```
Returns: `TotalPoints`, `TotalSoftcorePoints`, `TotalTruePoints`, `Rank`, `MemberSince`,
`RecentAchievements`.

### ROM Hashing

RA identifies games by MD5 hash of the ROM file. The client provides a `hashRom(filePath)`
method that computes the MD5.

For M3U multi-disc games, hash the first disc file listed in the M3U (RA typically
identifies games by disc 1).

Hash results are cached in the `ra_games` DB table and only recomputed if the ROM file
path changes.

### Rate Limiting

No documented rate limits from RA. The client batches requests with small delays
(~100ms between calls) during bulk sync to be a good API citizen.

## Data Model

### Database Schema Extension

Two new tables added to the SQLite database:

**`ra_games`** — Maps local games to RA game IDs with summary progress:

| Column | Type | Description |
|--------|------|-------------|
| `game_id` | INTEGER FK | References `games` table |
| `ra_game_id` | INTEGER | RA's numeric game ID |
| `rom_hash` | TEXT | MD5 hash used for matching |
| `num_achievements` | INTEGER | Total achievement count |
| `num_earned` | INTEGER | Softcore unlocks |
| `num_earned_hardcore` | INTEGER | Hardcore unlocks |
| `completion_pct` | TEXT | User completion percentage |
| `last_synced` | TEXT | ISO timestamp of last sync |

Primary key: `game_id`. Index on `ra_game_id`.

**`ra_achievements`** — Individual achievements per RA game:

| Column | Type | Description |
|--------|------|-------------|
| `ra_achievement_id` | INTEGER PK | RA's achievement ID |
| `ra_game_id` | INTEGER | FK to RA game |
| `title` | TEXT | Achievement name |
| `description` | TEXT | What to do |
| `points` | INTEGER | Point value |
| `true_ratio` | INTEGER | Rarity-weighted points (higher = rarer) |
| `badge_name` | TEXT | Badge image filename on RA |
| `type` | TEXT | Achievement type (core, unofficial) |
| `earned` | INTEGER | 0/1 softcore earned |
| `earned_hardcore` | INTEGER | 0/1 hardcore earned |
| `earned_date` | TEXT | Date earned (ISO string, empty if locked) |

Index on `ra_game_id`.

### Badge Images

Downloaded to `{root}/downloaded_media/ra_badges/{badge_name}.png`.

Badge URL format: `https://media.retroachievements.org/Badge/{badge_name}.png`

Downloaded during sync, same pattern as scraper artwork downloads. Images are served
from local cache so the UI stays responsive.

## Background Sync Service

### New Class: `RetroAchievementsService`

Located in `services/`. Mirrors `ScraperService` pattern.

**Owns:**
- `RetroAchievementsCredentials` instance
- `RetroAchievementsClient` instance
- Sync state (`syncing` bool property, exposed to QML)

### Sync Triggers

| Trigger | What syncs | Purpose |
|---------|-----------|---------|
| On login | Full: hash all ROMs, resolve game IDs, fetch all progress, download badges | Initial setup |
| On app launch (if logged in) | Incremental: re-fetch progress for all matched games, download new badges | Pick up unlocks from other sessions |
| After game exit | Targeted: re-fetch progress for the just-played game only | Show newly unlocked achievements immediately |

### Full Sync Flow

1. Fetch console game lists for all relevant systems (`psx`, `ps2`) — one API call per console.
   Build a hash → RA game ID lookup table.
2. Hash all ROMs in the local library (MD5). Skip ROMs already hashed in `ra_games`.
3. Match ROM hashes to RA game IDs. Write matches to `ra_games` table.
4. For each matched game, fetch `GetGameInfoAndUserProgress`. Write achievements to
   `ra_achievements`, update progress in `ra_games`.
5. Download any missing badge images in parallel.
6. Fetch user summary for profile stats.
7. Emit `syncCompleted()` signal.

### Incremental Sync Flow

Skip steps 1-3 (game ID resolution already cached). Re-run steps 4-7.

### Targeted Sync Flow

Run step 4 for one game only, then step 5 for new badges. Emit `gameSyncCompleted(gameId)`.

### Threading

Sync runs on a background thread via `QtConcurrent` or a worker `QThread`. Signals
update the DB and notify QML when data is ready. The `syncing` Q_PROPERTY lets the UI
show loading indicators.

## UI: Settings Overlay — RetroAchievements Page

### New File: `RetroAchievementsSettings.qml`

Added to `SettingsOverlay.qml` as a new category (alongside Emulators, Controllers,
Paths, Scraper, etc.).

### Logged-Out State

- RA logo + "RetroAchievements" title + subtitle
- Username text field (with VirtualKeyboard support for controller input)
- Password text field (masked, with VirtualKeyboard support)
- "Sign In" button
- Error message area for failed login attempts

### Logged-In State

**Profile Header:**
- Avatar (first letter of username), username, key stats inline (points, rank, achievements)
- "Sign Out" button

**Overall Stats (two rows of cards):**
- Row 1: Total Points, Achievements, Games Mastered, Games Started
- Row 2: Hardcore Points, Softcore Points, Average Completion

**Recent Unlocks:**
- Last 5 achievements earned across all games
- Each row: badge icon, achievement title, game name, date earned

**Game Progress Grid (3 columns):**
- One card per RA-matched game in the library
- Each card: game title, system name, progress bar, "X / Y" count, percentage
- Mastered games (100%) highlighted with green border and medal icon
- Clicking a card navigates to that game's full achievement list (`AchievementsPage`)

**Options:**
- Hardcore Mode toggle (with description)
- Notifications toggle
- Sound Effects toggle

**Sync Bar (bottom):**
- "Last synced: X ago · Y games matched"
- "Sync Now" button

### Controller Navigation

Follows existing settings page patterns:
- All focusable items accessible via D-pad
- VirtualKeyboard opens automatically for text fields when controller is active
- Toggles activated with Return/A button
- Back/B returns to settings category grid

## UI: Game Achievements Page

### New File: `AchievementsPage.qml`

A dedicated page showing all achievements for a single game.

### Entry Points

1. **Game action popup** → "Achievements" option → pushes `AchievementsPage` onto main StackView
2. **RA settings page** → click a game in the progress grid → pushes `AchievementsPage` onto settings StackView

### Layout

**Header:**
- Game title
- Progress bar with "X / Y — Z%" label
- Earned points out of total

**Achievement List (scrollable):**

Each achievement row:
- Badge icon (from local cache, `{root}/downloaded_media/ra_badges/`)
- Title + description
- Point value
- Rarity percentage (derived from `TrueRatio` — higher ratio = rarer)
- Earned date (if unlocked) or locked/greyed state

**Sort Order:** Unlocked achievements first (most recently earned at top), then locked.

**Empty States:**
- "No achievements available for this game" — if RA has no achievement set
- "Log in to RetroAchievements to track progress" — if not logged in

### Controller Navigation

- D-pad Up/Down scrolls the achievement list
- Back/B returns to previous view
- Standard focus glow on selected achievement row

## UI: Game Action Popup Change

### Modified File: `GameActionPopup.qml`

Add "Achievements" option to the action popup. Visible only when the game has a matched
RA game ID (checked against `ra_games` table via a new property on `GameListModel` or
`ThemeContext`).

Position in popup: after "Scrape", before "Favorite".

When selected, pushes `AchievementsPage` with the game's RA game ID.

## New Files Summary

| File | Location | Type | Purpose |
|------|----------|------|---------|
| `ra_credentials.h/cpp` | `core/` | C++ | Credential JSON I/O |
| `ra_client.h/cpp` | `core/` | C++ | RA Web API client |
| `ra_service.h/cpp` | `services/` | C++ | Sync orchestration + QML bridge |
| `RetroAchievementsSettings.qml` | `qml/AppUI/` | QML | Settings page (login + dashboard) |
| `AchievementsPage.qml` | `qml/AppUI/` | QML | Per-game achievement list |

## Modified Files Summary

| File | Change |
|------|--------|
| `database.h/cpp` | Add `ra_games` and `ra_achievements` tables, schema migration |
| `emulator_adapter.h` | Add virtual `supportsRetroAchievements()` (default false) |
| `duckstation_adapter.h/cpp` | Override `supportsRetroAchievements()`, add RA INI patching in `ensureConfig()` |
| `pcsx2_adapter.h/cpp` | Override `supportsRetroAchievements()`, add RA INI patching (main + secrets.ini) |
| `app_controller.h/cpp` | Expose `RetroAchievementsService` to QML |
| `game_service.h/cpp` | Trigger targeted sync after game exit |
| `game_list_model.h/cpp` | Add `hasAchievements` role (bool, from `ra_games`) |
| `theme_context.h/cpp` | Expose RA-related methods for themes |
| `SettingsOverlay.qml` | Add RetroAchievements category |
| `GameActionPopup.qml` | Add "Achievements" option (conditional on `hasAchievements`) |
| `CMakeLists.txt` | Add new source files and QML resources |

## What This Design Does NOT Include

- No rcheevos library integration — emulators handle runtime achievement logic
- No in-game achievement overlay from EmuFront — emulators show their own unlock notifications
- No leaderboard display — could be added later
- No friend/social features — could be added later
- No rich presence display — could be added later
- No achievement unlock notifications within EmuFront UI while a game is running
