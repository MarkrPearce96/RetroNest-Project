# Emulator Management Redesign — Design Spec

## Overview

Redesign the emulator management pages to support the full emulator lifecycle: discovery, installation with real progress, configuration, reset, update, and uninstall. Add startup update notifications.

## 1. Emulator Grid (EmulatorManageGrid.qml)

**Current:** Shows only installed emulators (filtered from `allEmulatorStatus()`).

**Change:** Show ALL emulators from manifests.

- Installed emulators: full opacity, clickable to detail page
- Uninstalled emulators: 45% opacity, "Not Installed" badge at bottom of card, clickable to detail page
- Remove the `.filter(function(e) { return e.installed })` — use the full list
- Badge: small pill with text "Not Installed", background `Theme.divider`, text `Theme.textDim`, positioned absolute at bottom center of card
- `onEmulatorInstalled` signal handler refreshes the full list (already does this)

## 2. Emulator Detail Page (EmulatorDetailPage.qml)

Two states based on `emuInfo.installed`:

### Uninstalled State

- **Left column:** logo, description, emulated systems, BIOS status — all available from manifest pre-install
- **Right column:** "Get Started" header, prominent "Install [Name]" button (accent color, full width, larger padding), helper text "Downloads the latest release from GitHub"
- Clicking Install opens the `ProgressPopup`, on completion the page transitions in-place to installed state

### Installed State (enhanced from current)

- **Left column:** same as current, plus **version info** displayed below the logo (e.g. "Version: v1.7.5"), read from `.version.json`
- **Right column — Actions:**
  - Emulator Settings — opens `EmulatorSettingsPage` dialog (existing)
  - Reset Configuration — now shows confirmation dialog first, then resets settings + controller mappings + hotkeys
  - Reinstall / Update — opens `ProgressPopup` with async install
  - Uninstall — existing confirmation dialog, then `ProgressPopup` during removal, navigates back to grid on completion
- **Right column — Controls:**
  - Controller Mapping — opens `ControllerMappingPage` dialog (existing)
  - Hotkeys — opens `HotkeySettingsPage` dialog (existing)

### Reset Confirmation Dialog

New `Popup` in `EmulatorDetailPage.qml`, styled like the existing uninstall confirmation:

- Title: "Reset [Name] Configuration?"
- Body: "This will reset all emulator settings, controller mappings, and hotkeys to their install defaults."
- Buttons: "Cancel" (surface color) / "Reset" (accent color)

## 3. Progress Popup (new: ProgressPopup.qml)

Reusable modal QML component for install, reinstall, and uninstall operations.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `title` | string | e.g. "Installing PCSX2" |
| `subtitle` | string | e.g. "Downloading latest release..." |
| `progress` | real | 0.0–1.0 for determinate, -1 for indeterminate |
| `progressText` | string | e.g. "62% — 48 MB / 78 MB" |
| `accentColor` | color | purple for install/reinstall, red for uninstall |
| `visible` | bool | controls show/hide |

### Behavior

- Modal overlay (blocks interaction with page behind)
- Centered popup with rounded corners, emulator logo, title, subtitle, progress bar, progress text
- Determinate mode: filled bar based on `progress` value
- Indeterminate mode (`progress === -1`): animated bar sliding back and forth
- Auto-closes when operation signals completion
- No cancel button

### Usage

- **Install/Reinstall download phase:** determinate bar, subtitle "Downloading latest release...", text shows percentage + MB
- **Install/Reinstall extract phase:** indeterminate bar, subtitle "Extracting files..."
- **Uninstall:** indeterminate bar, red accent, subtitle "Removing files..."

## 4. Threaded Installer Backend

### Current Problem

`EmulatorInstaller::install()` is synchronous — blocks UI thread, no progress reporting.

### Solution

#### New Signals on AppController

```cpp
signals:
    void installProgress(const QString& emuId, double progress,
                         const QString& phase, const QString& detail);
    void installFinished(const QString& emuId, bool success, const QString& message);
    void uninstallFinished(const QString& emuId, bool success, const QString& message);
```

#### EmulatorInstaller Changes

- Split `install()` into download + extract steps
- Download uses `QNetworkAccessManager` with `QNetworkReply::downloadProgress` for real byte-level progress
- Extract phase emits indeterminate progress (-1)
- Runs on a background thread via `QtConcurrent::run()` or a `QThread` worker

#### EmulatorService Changes

- `installEmulator()` becomes async — kicks off worker, returns immediately
- `uninstallEmulator()` moves from AppController to EmulatorService, also runs async
- Both emit progress/finished signals that AppController forwards to QML

#### AppController Changes

- `installEmulator()` becomes non-blocking — calls async service method
- `uninstallEmulator()` delegates to service
- Connects service progress/finished signals to own signals for QML consumption

## 5. Reset Configuration (enhanced)

### Current

`resetConfiguration()` deletes config file and re-runs `ensureConfig()`. Only resets emulator INI settings.

### Change

`resetConfiguration()` now performs a full reset:

1. Delete existing config file
2. Re-run `ensureConfig()` to regenerate embedding-critical defaults
3. Call `resetControllerBindings(emuId)` — reset all controller bindings to adapter defaults
4. Call `resetControllerSettings(emuId)` — reset controller settings to adapter defaults
5. Call `resetHotkeys()` — reset hotkeys to adapter defaults (applies to all emulators since hotkeys are unified)

All three sub-reset methods already exist and work correctly. The change is calling them together from `resetConfiguration()`.

## 6. Version Tracking

### On Install

After successful install, save version metadata to `emulators/[install_folder]/.version.json`:

```json
{
    "version": "v1.7.5",
    "installed_at": "2026-03-28T14:30:00Z"
}
```

The version tag comes from the GitHub release that was downloaded.

### Reading Version

- `allEmulatorStatus()` reads `.version.json` for each installed emulator and includes `version` in the returned QVariantMap
- Detail page displays version string below the logo in the left info column

### EmulatorInstaller Changes

- `install()` return type extended to include the release tag/version string
- After download, extract the tag name from the GitHub API response
- Write `.version.json` after successful extraction

## 7. Update Notification on Startup

### Backend

New method: `EmulatorService::checkForUpdates()`

- Iterates all installed emulators
- For each, reads `.version.json` for current version
- Hits GitHub releases API (`https://api.github.com/repos/{owner}/{repo}/releases/latest`) for latest version
- Compares tags; if different, emits signal
- Runs async on background thread at startup

**Rate limiting:**
- Cache check results in `update_check.json` in the app data directory:
  ```json
  {
      "last_check": "2026-03-28T14:30:00Z",
      "updates": {
          "pcsx2": { "current": "v1.7.5", "latest": "v1.8.0" }
      }
  }
  ```
- Only hit GitHub API if last check was >24 hours ago
- Unauthenticated GitHub API: 60 requests/hour limit (plenty for 2-3 emulators once/day)

### Signals

```cpp
signals:
    void updateAvailable(const QString& emuId, const QString& currentVersion,
                         const QString& latestVersion);
```

### QML: UpdateNotification.qml

- Toast/banner that slides in from the top
- Shows emulator name, available version, current version
- "View" button navigates to: Settings Overlay > Manage Emulators > that emulator's detail page
- Dismissable via X button, auto-hides after 10 seconds if not interacted with
- If multiple updates available, show one at a time (queue)

### Startup Flow

1. App starts → `AppController` constructor (or `Component.onCompleted` in QML) calls `checkForUpdates()`
2. Service checks cache → if stale, hits GitHub API async
3. For each update found, emits `updateAvailable` signal
4. QML `UpdateNotification` component listens for signal, shows toast

## 8. Settings Audit

Verify correctness of existing emulator settings for both PCSX2 and DuckStation:

- Both adapters return correct `settingsSchema()` with valid section/key pairs matching their INI files
- `settingValue()` correctly reads current values for each schema entry
- `saveSettings()` correctly writes back changed values
- Settings dialog populates widgets with current values on open
- Combo boxes map display labels to INI values correctly (the `optionValues` map)
- Stale keys handled per CLAUDE.md rules: disabled + greyed out with tooltip

No architectural changes — audit and fix bugs found.

## Files Modified

| File | Change |
|------|--------|
| `cpp/qml/AppUI/EmulatorManageGrid.qml` | Show all emulators, greyed out + badge for uninstalled |
| `cpp/qml/AppUI/EmulatorDetailPage.qml` | Two states, version info, reset confirmation dialog |
| `cpp/qml/AppUI/ProgressPopup.qml` | **New** — reusable progress popup component |
| `cpp/qml/AppUI/UpdateNotification.qml` | **New** — startup update toast |
| `cpp/qml/AppUI/AppWindow.qml` | Add UpdateNotification component |
| `cpp/src/core/emulator_installer.h/cpp` | Async install with progress signals, version tag extraction |
| `cpp/src/services/emulator_service.h/cpp` | Async install/uninstall, update checking, rate-limited GitHub API |
| `cpp/src/ui/app_controller.h/cpp` | New signals (installProgress, installFinished, uninstallFinished, updateAvailable), enhanced resetConfiguration |
| `cpp/src/adapters/pcsx2_adapter.*` | Audit settingsSchema correctness |
| `cpp/src/adapters/duckstation_adapter.*` | Audit settingsSchema correctness |

## Out of Scope

- Controller mapping UI changes
- Hotkey mapping UI changes
- Adding new emulator manifests/adapters
- Auto-install updates (user must manually click Reinstall/Update)
