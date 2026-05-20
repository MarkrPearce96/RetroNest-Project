# RetroNest themes

Themes are runtime-loaded QML directories. Each theme provides a `theme.json`
manifest plus the QML pages it owns (typically `systemBrowser` and `gameList`).
Themes never own the entire app frame — AppWindow renders the chrome (button
hints, modals, overlays) and dispatches universal input.

## Universal input contract

The following keys are handled globally by AppWindow. Do not reimplement them
in a theme page:

| Key                  | Action  | Effect                                       |
|----------------------|---------|----------------------------------------------|
| `Esc` / `Backspace`  | Back    | `themeContext.navigateBack()` (pops the theme stack, or opens Settings if at root). Controller B/Circle synthesises Backspace. |
| `M`                  | Action  | `themeContext.openGameActions(currentFocusedGameId)`. No-op when no game is focused. Controller Y/Triangle synthesises M. |
| Controller `Start`   | Settings| Toggles the Settings overlay.                |

Modals (Settings overlay, GameActionPopup, ResumeStateDialog, etc.) catch their
own keys first, so the global Shortcuts never fire underneath them.

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

The global `Backspace` Shortcut means a top-level page **cannot** host a raw
text input without consuming `Backspace` locally. The standard QML pattern:

```qml
TextField {
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Backspace) event.accepted = true
    }
}
```

All current text inputs (Scraper login, RA login, Virtual Keyboard, search)
live inside modals whose focus contexts already swallow `Backspace`, so no
existing page is affected.
