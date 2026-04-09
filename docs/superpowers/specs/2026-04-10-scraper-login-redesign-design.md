# Scraper Login Redesign — Match RetroAchievements Card

**Date:** 2026-04-10
**Status:** Approved

## Goal

Redesign the ScreenScraper login page so its visual layout mirrors the
RetroAchievements login page exactly — a centered 360px card with logo, title,
description, fields, and full-width primary button. Replace the drawn "RA"
square on the RetroAchievements login page with the real PNG logo, and add a
matching PNG logo to the new Scraper login card.

Visual parity across the two login pages is the primary driver. The scraper's
existing functional behavior (virtual keyboard integration, field-focus
reclaim, validation flow) must be preserved unchanged.

## Asset work

### Move and rename the PNGs

The user added two logo PNGs under `assets/` at the repo root. The project's
convention puts all QML-referenced images under `cpp/qml/AppUI/images/` and
registers them in `cpp/CMakeLists.txt`. Move and rename to match the existing
`pcsx2_logo.png` / `duckstation_logo.png` / `ppsspp_logo.png` naming:

- `assets/RetroAchievments Logo.png` → `cpp/qml/AppUI/images/retroachievements_logo.png`
  (also fixes the `Achievments` misspelling and removes the space)
- `assets/ScreenScraper Logo.png` → `cpp/qml/AppUI/images/screenscraper_logo.png`

Delete the now-empty `assets/` directory, including its `.DS_Store`.

### Register the PNGs in CMake

Add two entries to the `RESOURCES` block of the `qt_add_qml_module(appui_backing ...)`
call in `cpp/CMakeLists.txt` (alongside the existing emulator logos):

```cmake
        qml/AppUI/images/retroachievements_logo.png
        qml/AppUI/images/screenscraper_logo.png
```

QML references use the project's relative path convention
(`"images/retroachievements_logo.png"`), matching how `empty-state-bg.webp`
and the aspect-ratio/resolution images are loaded.

## `RetroAchievementsSettings.qml` — logo swap only

Lines 112–129 currently render the placeholder logo as a 64×64 gradient
`Rectangle` with centered "RA" text:

```qml
// RA Logo
Rectangle {
    width: 64
    height: 64
    radius: 16
    anchors.horizontalCenter: parent.horizontalCenter
    gradient: Gradient {
        GradientStop { position: 0.0; color: SettingsTheme.accent }
        GradientStop { position: 1.0; color: Qt.darker(SettingsTheme.accent, 1.3) }
    }

    Text {
        anchors.centerIn: parent
        text: "RA"
        color: SettingsTheme.text
        font.pixelSize: 24
        font.weight: Font.Bold
    }
}
```

Replace with a 64×64 `Image` using the new PNG:

```qml
// RA Logo
Image {
    width: 64
    height: 64
    anchors.horizontalCenter: parent.horizontalCenter
    source: "images/retroachievements_logo.png"
    fillMode: Image.PreserveAspectFit
    smooth: true
}
```

No other changes to `RetroAchievementsSettings.qml`. The title, description,
fields, button, error text, focus logic, and the entire dashboard state are
untouched.

## `ScraperSettings.qml` — redesign the login state

Replace the entire "State 0: LOGIN" block (currently a full-width
left-aligned `Flickable` + `ColumnLayout` spanning lines 527–741) with a
centered card that mirrors the RetroAchievements login card exactly.

### Target layout

```
┌──────────────────── card (360×auto, radius 12) ─────────────┐
│                                                             │
│         [Image 64×64: screenscraper_logo.png]               │
│                                                             │
│                    ScreenScraper                            │
│                                                             │
│   Enter your ScreenScraper.fr credentials to download       │
│    media and metadata for your games.                       │
│                                                             │
│   Username                                                  │
│   [      TextField 40px, accent border on focus       ]     │
│                                                             │
│   Password                                                  │
│   [      TextField 40px, echoMode Password            ]     │
│                                                             │
│   [            Connect  (42px, accent)               ]      │
│                                                             │
│   error text (red, only when loginError is set)             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

Card structure matches RetroAchievements line-for-line: outer `Rectangle`
with `width: 360`, `radius: 12`, `color: SettingsTheme.card`, `border.width: 1`,
`border.color: SettingsTheme.border`, containing an anchored `Column` with
`spacing: 16` and `width: parent.width - 48`.

### Preserved behavior (must NOT be removed or broken)

- The existing `loginFocusIndex` 0/1/2 state machine and all its `Keys.onPressed`
  handlers at the root level of `ScraperSettings.qml`.
- **Virtual keyboard integration.** The scraper's controller-activate path
  invokes `virtualKeyboard.open(text, isPassword, label)` for each field.
  RetroAchievements has no virtual keyboard integration, but the scraper
  needs it for controller navigation on a TV/console context. Keep the
  existing activate logic untouched.
- Qt Quick Controls `TextField` (not plain `TextInput` like RetroAchievements uses) —
  required for the virtual keyboard integration to keep working.
- All existing signals and slots: `onScraperCredentialsValidated`,
  `app.validateScraperCredentials`, `onScraperSignedOut`.
- `signInBtn.enabled` disable-while-validating behavior.
- `_maybeReclaimLoginFocus` helper and the `Connections` on `loginUserField`
  / `loginPassField` that call it.
- The `loginError` Text node — reuse its id so existing signal handlers that
  set `loginError.visible` / `loginError.text` keep working.

### What changes

- **Layout:** full-width left-aligned → centered 360px card with background
  and border.
- **Page-body heading removed.** The 18px "Scraper" Text at the top of the
  current login column is deleted. The top nav bar already displays
  "Scraper", matching how RetroAchievements has no in-body title (the nav bar
  shows "Achievements").
- **Field widths:** 300px fixed → fill card inner width (~312px inside 24px
  horizontal padding).
- **Field heights:** 36 → 40 to match RetroAchievements.
- **Field label size:** 13 → 12 to match RetroAchievements.
- **Button:** 120px left-aligned "Sign In" → full-card-width 42px "Connect"
  with accent background. The text switches to "Validating..." during the
  disabled validation window, matching RetroAchievements.
- **Focus glow dropped.** The nested `anchors.margins: -4` glow rectangles on
  fields and the button are removed. Border-color-on-focus is the new focus
  indicator, matching RetroAchievements's simpler approach.
- **Field border treatment:** matches RetroAchievements —
  `border.width: 1`, `border.color: loginFocusIndex === N ? SettingsTheme.accent : SettingsTheme.border`.

### New button label text

- Idle: `"Connect"`
- Disabled / validating: `"Validating..."`

(Replaces the current `"Sign In"` which never changes text during validation.)

## `cpp/CMakeLists.txt`

Add two lines to the `RESOURCES` block inside `qt_add_qml_module(appui_backing …)`
at the existing image section (around line 289):

```cmake
    RESOURCES
        qml/AppUI/images/pcsx2_logo.png
        qml/AppUI/images/duckstation_logo.png
        qml/AppUI/images/ppsspp_logo.png
        qml/AppUI/images/retroachievements_logo.png       # ← new
        qml/AppUI/images/screenscraper_logo.png           # ← new
        qml/AppUI/images/empty-state-bg.webp
        ...
```

Alphabetical-ish ordering alongside the other `_logo.png` files is fine; no
strict convention is enforced elsewhere in the list.

## Out of scope

- Scraper **dashboard** state (lines 743+ of `ScraperSettings.qml`) — unchanged.
- Scraper **progress** state — unchanged.
- All scraper functionality: scraping jobs, account settings, sign-out,
  credential storage — unchanged.
- RetroAchievements dashboard, stats, game progress, achievements pages —
  unchanged.
- `SettingsOverlay.qml`, the top navigation bar, and page routing — unchanged.
- No changes to any C++ code. The virtual keyboard backend, credential
  validation slot, and signals are all already in place.

## Testing

Manual verification on a built desktop macOS bundle:

1. Build the app.
2. **RetroAchievements logo swap:** Open Settings → RetroAchievements. If
   signed in, sign out first. Confirm the real PNG logo renders in place of
   the drawn "RA" square at the top of the login card.
3. **Scraper card visual parity:** Open Settings → Scraper. If signed in,
   sign out first. Confirm:
   - The login content is a centered 360px card, not a full-width
     left-aligned column.
   - The card contents in order: ScreenScraper PNG logo (64×64), title
     "ScreenScraper", description text, Username field, Password field,
     "Connect" button, hidden error text.
   - Visually matches the RetroAchievements login card (card size, corner
     radius, border, spacing, field heights, button height).
4. **Focus navigation:** Press Down / Up on the controller or keyboard.
   Confirm focus cycles Username → Password → Connect → Username with
   accent border highlights (no focus glow).
5. **Virtual keyboard regression check:** With the virtual keyboard
   available, activate the Username field via controller. Confirm the
   virtual keyboard opens with `USERNAME` label and the text routes back
   to the field on close.
6. **Validation path:** Enter deliberately wrong credentials and press
   Connect. Confirm the button text switches to "Validating..." then back,
   and the error text appears below the button in the card. Then enter
   correct credentials and confirm the scraper dashboard state loads.
7. **No regressions to the dashboard state** — open the dashboard path
   after a successful login and confirm it renders as before.
