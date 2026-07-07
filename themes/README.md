# RetroNest themes

Themes are runtime-loaded QML directories. Each theme provides a `theme.json`
manifest plus the QML pages it owns (typically `systemBrowser` and `gameList`).
Themes never own the entire app frame — AppWindow renders the chrome (button
hints, modals, overlays) and dispatches universal input.

> **Adding a new modal?** Skip past the theme contract to
> [Adding a new modal / overlay](#adding-a-new-modal--overlay) at the bottom.

## theme.json manifest

```json
{
  "name": "Modern",
  "version": "1.0",
  "author": "Built-in",
  "description": "Clean carousel and grid layout with detail panel",
  "preview": "preview.png",
  "minAppVersion": "0.1.0",
  "pages": {
    "systemBrowser": "SystemPage.qml",
    "gameList": "GameListPage.qml"
  }
}
```

Validated at scan (`ThemeManager::scanThemes`, pinned by
`test_theme_manager`):

- `pages.systemBrowser` and `pages.gameList` are **required**, and every
  declared page file must exist on disk — a theme failing either is
  rejected with a log line instead of dying at first navigation.
- `minAppVersion` is **enforced**: if it's newer than the running app
  version the theme is skipped (omit the key to accept any version).
- Unknown top-level keys log a warning (typo net) but don't reject.
- The theme's **id** is its directory name; the user themes dir
  (`{root}/themes/`) is scanned before the bundled dir, and the first
  directory with a given id wins.

The chosen theme id persists across launches (config.json, written when
the user picks a theme in Settings → Themes). If a saved theme
disappears, the first valid theme is used.

If a theme page fails to load at runtime anyway (broken QML), AppWindow
falls back to the built-in empty-state page for the root — a broken
theme can't produce a dead black window.

## API boundary — themeContext ONLY (enforced)

Theme pages load in the main QML engine, so the app's root context
objects (`app`, `gameModel`, `inputManager`, `themeManager`, ...) are
*technically* resolvable from theme QML. **Using them is forbidden** —
anything a theme binds silently becomes frozen public API for every
future theme. The contract:

- `themeContext` is the ONLY app object a theme may touch — navigation,
  game operations, system/game data, and `themeContext.gameModel` (the
  game list model for views).
- `test_theme_contract` lints every `.qml` under `themes/` for bare
  references to the forbidden globals and fails CI with file:line.

Need something that isn't on `themeContext`? Add it to ThemeContext
(and its docs here), never bind the global.

## Universal input contract

The following keys are handled globally by AppWindow. Do not reimplement them
in a theme page:

| Key                                     | Action  | Effect |
|-----------------------------------------|---------|--------|
| `Esc`                                   | Back    | Closes the current modal / settings sub-page; if none open and the theme stack has depth > 1, pops it; else opens Settings. |
| `Backspace` / controller B/Circle (`Back`) | Back    | Same as `Esc`, but disabled while a settings panel, virtual keyboard, or login/confirm modal is open so it doesn't clobber text-input or in-modal navigation. |
| `M` / controller Y/Triangle            | Action  | `themeContext.openGameActions(currentFocusedGameId)`. No-op when no game is focused. |
| Controller `Start`                      | Settings| Toggles the Settings overlay. |

Controller B/Circle (`Key_Back`) and X/Square (`Backspace`) both fire the same
Backspace/Back Shortcut.

The Esc Shortcut is intentionally always enabled (except inside a couple of
top-level modal popups) so it can close Settings sub-pages. The Backspace/Back
Shortcut is more conservatively gated — see the text-input caveat below.

## What themes own

- **Arrow-key navigation** — carousel left/right, list up/down, grid up/down/left/right.
  The layout dictates the semantics, so AppWindow can't generalise.
- **`Enter`/`Return`** as "activate focused" — themes call `themeContext.launchGame(...)`
  or `themeContext.navigateToSystem(...)` directly.

## Required page properties

Each theme page must declare:

```qml
property var hints: [
    {action: "navigate_lr", label: "Browse"},
    {action: "confirm",     label: "Select"},
    {action: "start",       label: "Settings"}
]
```

AppWindow's single `ButtonHints` strip reads from `mainStack.currentItem.hints`.
Pages that omit the property get an empty strip.

Valid `action` values (per `cpp/qml/AppUI/ButtonHints.qml`): `confirm`, `back`,
`action`, `delete`, `navigate_lr`, `navigate_ud`, `start`. Each action picks
a glyph appropriate to the active input device (keyboard / Xbox / PlayStation).
Pass `keyboardKey` to override the keyboard glyph text — useful when the global
key for an action is more specific than the action's default keyboard glyph
(e.g. `{action: "back", label: "Back", keyboardKey: "Backspace"}` shows
"Backspace" instead of the default "Esc").

## Publishing the focused game

If a page has a currently-focused game, write its database id to
`themeContext.currentFocusedGameId`. AppWindow's M Shortcut reads this to
target `openGameActions`. Set to `-1` when no game is focused (system page,
empty state, transitional moments). Clearing on `Component.onDestruction`
keeps things tidy if the page is popped.

## Text input caveat

The Esc Shortcut fires before focused TextFields see the key. The Backspace
Shortcut is therefore gated against `settingsOverlay.visible` and
`inputManager.virtualKeyboardOpen` so it can't clobber text editing in those
contexts. If a future top-level theme page (i.e. one rendered directly by the
StackView, not inside Settings or the virtual keyboard) hosts a text input,
the page must override the global Shortcut with `Keys.shortcutOverride`:

```qml
TextField {
    Keys.shortcutOverride: function(event) {
        if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Escape)
            event.accepted = true
    }
}
```

`Keys.onPressed` with `event.accepted = true` is **not** sufficient — Qt
`Shortcut`s preempt the focus tree's key handling. Only `shortcutOverride`
disables a Shortcut for a focused item.

## Adding a new modal / overlay

This section is for **app developers**, not theme authors. Themes don't
add modals.

Universal input gating is single-sourced in three derived window
properties near the top of `cpp/qml/AppUI/AppWindow.qml` (the policy
table lives in the comment block right above them):

- `anyModalVisible` — the matcher-inhibiting set. Feeds the
  `app.libretroHotkeysSuppressed` Binding (together with
  `settingsOverlay.panelOpen` and `!app.gameRunning`) so bound libretro
  hotkeys can't preempt a focused modal's `Keys.onPressed`. Also hides
  the main `ButtonHints` strip.
- `anyEscOwningModalVisible` — `anyModalVisible || app.inGameMenuOpen`.
  Disables the universal `Esc` Shortcut (modals that own Esc internally
  must be here; `SettingsOverlay` deliberately isn't, so Esc keeps
  driving its sub-page goBack), and gates the `Backspace`/`Back` and
  `M` Shortcuts and the controller Start button.
- `cursorNeeded` — mouse-driven overlays. Its change handler is the
  ONLY `setCursorVisible` caller; never toggle the cursor from a modal.

To add a new modal:

1. Give your modal a stable `id:` and a `visible` (or `panelOpen`-style
   user-intent) property.
2. Add its flag to the matching derived properties and one row to the
   policy table comment — nothing else to keep in sync.
3. Inside your modal, handle `Esc`, `Backspace`, and `Qt.Key_Back` in
   `Keys.onPressed`. **Or** derive from `BaseModalCard.qml` and
   **do not** override `Keys.onPressed` — the base handles all three
   for you (QML attached-property semantics mean a derived
   `Keys.onPressed` fully replaces the base's; re-add the three keys
   yourself if you must override).
4. Full-window scrims must swallow scroll as well as clicks
   (`onWheel: (wheel) => wheel.accepted = true` — see BaseModalCard),
   or two-finger swipes keep driving the page beneath the modal.
5. If you want a button-hint pill strip rendered with your modal's card,
   add a local `ButtonHints { hints: [...] }` inside the card. The main
   strip auto-hides while any modal is visible.

### Modals that need different roles

The policy isn't always "all flags." See the policy table for the
current exceptions. The two recurring patterns:

- **`SettingsOverlay` style** (not Esc-owning) — you want the universal
  `Esc` to keep firing so it can walk back through the overlay's own
  sub-page history.
- **Libretro in-game menu style** (not matcher-inhibiting) — the
  matcher's `Esc = ToggleMenu` IS the close mechanism. If you suppress
  libretro hotkeys, you lock yourself in.
