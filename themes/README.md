# RetroNest themes

Themes are runtime-loaded QML directories. Each theme provides a `theme.json`
manifest plus the QML pages it owns (typically `systemBrowser` and `gameList`).
Themes never own the entire app frame — AppWindow renders the chrome (button
hints, modals, overlays) and dispatches universal input.

> **Adding a new modal?** Skip past the theme contract to
> [Adding a new modal / overlay](#adding-a-new-modal--overlay) at the bottom.

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

Universal input gating goes through a single `modalRegistry` QtObject at the
top of `cpp/qml/AppUI/AppWindow.qml`. It exposes three derived booleans:

- `anyLibretroInhibited` — drives `app.libretroHotkeysSuppressed`. Suppresses
  the application-level `HotkeyMatcher` event filter so its
  `Esc = ToggleMenu` (and any other bound libretro hotkey) doesn't preempt
  the focused modal's `Keys.onPressed`.
- `anyShortcutInhibited` — disables AppWindow's universal `Esc` Shortcut.
  Modals that own `Esc` internally must be in this set. `SettingsOverlay`
  is intentionally NOT in this set so the Shortcut keeps driving
  `goBack` / `close` through its sub-page history.
- `anyVisible` — derived as `anyLibretroInhibited || anyShortcutInhibited`,
  i.e. "any modal-class surface visible." Gates the universal `Backspace` /
  `Back` Shortcut, the `M` Shortcut, and `mainHints.visible`.

To add a new modal:

1. Give your modal a stable `id:` and a `visible` (or `panelOpen`-style
   user-intent) property.
2. Open `cpp/qml/AppUI/AppWindow.qml`, find `QtObject { id: modalRegistry`,
   and **append your modal to the relevant OR expression(s)** plus add
   one row to the policy table in the comment above the registry.
   Default for new modals: both flags `true` (it's a normal modal that
   owns its own Esc/Back).
3. Inside your modal, handle `Esc`, `Backspace`, and `Qt.Key_Back` in
   `Keys.onPressed`. **Or** derive from `BaseModalCard.qml` and
   **do not** override `Keys.onPressed` — the base handles all three
   for you (QML attached-property semantics mean a derived
   `Keys.onPressed` fully replaces the base's; re-add the three keys
   yourself if you must override).
4. If you want a button-hint pill strip rendered with your modal's card,
   add a local `ButtonHints { hints: [...] }` inside the card. The main
   `ButtonHints` strip in AppWindow auto-hides while any registry modal
   is visible (`mainHints.visible` reads `!modalRegistry.anyVisible`).

### Modals that need different roles

The policy isn't always "both flags true." See the table in the
`modalRegistry` comment for the current exceptions. The two recurring
patterns:

- **`SettingsOverlay` style** (`inhibitShortcuts: false`) — you want the
  universal `Esc` to keep firing so it can walk back through the
  overlay's own sub-page history. The overlay's outer keypress consumes
  Backspace/M itself; the universal Esc Shortcut feeds the back action.
- **Libretro in-game menu style** (`inhibitLibretro: false`) — the libretro
  `HotkeyMatcher`'s `Esc = ToggleMenu` IS the close mechanism. If you
  suppress libretro hotkeys, you lock yourself in.

If neither pattern applies, take the default (both true).
