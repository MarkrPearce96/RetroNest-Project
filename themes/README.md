# RetroNest themes

Themes are runtime-loaded QML directories. Each theme provides a `theme.json`
manifest plus the QML pages it owns (typically `systemBrowser` and `gameList`).
Themes never own the entire app frame — AppWindow renders the chrome (button
hints, modals, overlays) and dispatches universal input.

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
