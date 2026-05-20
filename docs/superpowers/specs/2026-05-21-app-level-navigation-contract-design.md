# App-level navigation contract

## Goal

Universal app-wide keys (`Back`, `Action`) and the bottom `ButtonHints` strip live in **AppUI/AppWindow** only. Themes declare what's focused and what hints they want, but never reimplement the contract. Future themes can't drift between key binding and hint label because they don't define either.

## Background

Per the refactor roadmap, the modern theme currently reimplements universal input behaviour inside `themes/modern/GameListPage.qml`:

- `Keys.onPressed` for `Backspace`/`Escape`/`Back` → `themeContext.navigateBack()`
- `Keys.onPressed` for `M` → `themeContext.openGameActions(...)`

Any future theme would have to duplicate this verbatim to honour the same UX. Worse, the `ButtonHints` strip in `cpp/qml/AppUI/AppWindow.qml` hardcodes the hint set based on `mainStack.depth > 1` — implicitly assuming "depth>1 = a game list page where Enter launches and M opens actions." Themes have no way to declare their own hints; the binding and the label can drift apart silently.

The AppWindow already centralises:

- The bottom `ButtonHints` strip (`mainHints`, line 723) — hides correctly when modals overlay.
- The `Escape` `Shortcut` (line 837) — handles settings/in-game-menu toggling.
- Controller `Start` and `Select+Start` via the `inputManager` signal handlers.

What's missing is (a) global `Backspace`/`M` handling and (b) a clean contract for the active page to declare hints to the central strip.

## Architecture

```
┌─ AppWindow (AppUI) ─────────────────────────────────────────┐
│  Shortcut "Back"     → ThemeContext.navigateBack()          │
│     keys: Esc, Backspace                                    │
│  Shortcut "Action"   → ThemeContext.openGameActions(...)    │
│     key: M           (controller Y/Triangle synthesises M)  │
│  Shortcut "Start"    → existing Settings open/close         │
│                                                             │
│  ButtonHints {                                              │
│     hints: mainStack.currentItem.hints ?? defaultHints      │
│  }                                                          │
└─────────────────────────────────────────────────────────────┘
              ▲                          ▲
              │ reads                    │ reads
              │                          │
┌─ ThemeContext (C++) ─────────────────────────────────────────┐
│  Q_PROPERTY currentFocusedGameId : int  (-1 = none)         │
│  signal currentFocusedGameIdChanged                          │
│  setter setCurrentFocusedGameId(int)                         │
│                                                              │
│  navigateBack(), openGameActions(int) — already exist        │
└──────────────────────────────────────────────────────────────┘
              ▲
              │ writes on selection change
              │
┌─ Theme page (runtime-loaded QML, e.g. modern/GameListPage) ─┐
│  Keys.onUpPressed / onDownPressed — layout's arrow nav       │
│  Keys.onReturnPressed — themeContext.launchGame(...)         │
│                                                              │
│  property var hints: [                                       │
│      {action: "navigate_ud", label: "Browse"},               │
│      {action: "confirm",     label: "Launch"},               │
│      {action: "action",      label: "Actions"},              │
│      {action: "back",        label: "Back"},                 │
│      {action: "start",       label: "Settings"}              │
│  ]                                                           │
│                                                              │
│  onListIndexChanged: themeContext.currentFocusedGameId =     │
│      selectedDetails.id || -1                                │
└──────────────────────────────────────────────────────────────┘
```

## Concrete changes

### `cpp/src/ui/theme_context.{h,cpp}`

Add:

```cpp
Q_PROPERTY(int currentFocusedGameId READ currentFocusedGameId
           WRITE setCurrentFocusedGameId
           NOTIFY currentFocusedGameIdChanged)

int  currentFocusedGameId() const;
void setCurrentFocusedGameId(int id);

signals:
void currentFocusedGameIdChanged();

private:
int m_currentFocusedGameId = -1;
```

Setter emits the change signal only when the value changes.

### `cpp/qml/AppUI/AppWindow.qml`

1. Extend the existing `Shortcut { sequence: "Escape" }` (line 837) to use `sequences: ["Escape", "Backspace"]` so both keys fire the same `onActivated` handler. The existing `enabled` gate (`!gameActionPopup.visible && !resumeStateDialog.visible && !app.inGameMenuOpen`) is unchanged. Modals catch the keys first via their own `Keys.onPressed` and never bubble to the Shortcut.
2. Add a new `Shortcut { sequence: "M" }` whose handler calls `themeContext.openGameActions(themeContext.currentFocusedGameId)` only when:
   - `themeContext.currentFocusedGameId >= 0`,
   - no modal is open (`!gameActionPopup.visible && !resumeStateDialog.visible && !app.inGameMenuOpen && !settingsOverlay.visible && !inputManager.virtualKeyboardOpen`),
   - `!app.gameRunning`.
3. Change `mainHints.hints` (line 735) from the hardcoded if/else block to:

   ```qml
   hints: {
       var page = mainStack.currentItem;
       if (page && page.hints !== undefined) return page.hints;
       return [];   // empty hint strip if active page declares nothing
   }
   ```

   The existing visibility gate (`!settingsOverlay.visible && !gameActionPopup.visible && !isEmulationView`) is unchanged.

### `cpp/qml/AppUI/EmptyStatePage.qml`

Add at the root:

```qml
property var hints: [{action: "start", label: "Settings"}]
```

This preserves the current strip contents on the empty state.

### `themes/modern/GameListPage.qml`

1. Delete the `Keys.onPressed` block (lines 94–106). Both the `Backspace`/`Escape`/`Back` branch and the `M` branch.
2. Add at the root:

   ```qml
   property var hints: [
       {action: "navigate_ud", label: "Browse"},
       {action: "confirm",     label: "Launch"},
       {action: "action",      label: "Actions"},
       {action: "back",        label: "Back", keyboardKey: "Backspace"},
       {action: "start",       label: "Settings"}
   ]
   ```

3. In `selectCurrentGame()` (after `root.selectedDetails = d`):

   ```qml
   themeContext.currentFocusedGameId = (d && d.id !== undefined) ? d.id : -1
   ```

4. Add a `Component.onDestruction` that resets `themeContext.currentFocusedGameId = -1` so M no-ops once the page is popped.

### `themes/modern/SystemPage.qml`

1. Add at the root:

   ```qml
   property var hints: [
       {action: "navigate_lr", label: "Browse"},
       {action: "confirm",     label: "Select"},
       {action: "start",       label: "Settings"}
   ]
   ```

2. In `Component.onCompleted`: `themeContext.currentFocusedGameId = -1` (no focused game while choosing systems). `Keys.onLeftPressed`/`Right`/`Return`/`Enter` stay as-is.

### Theme-author docs

Create `themes/README.md` with a short contract section:

> Each theme page must declare a `hints` array on its root. The AppWindow renders a single `ButtonHints` strip from this property — pages that omit it get an empty strip.
>
> Universal keys are handled by AppWindow:
> - `Back` — `Escape` or `Backspace` (controller B/Circle). Fires `themeContext.navigateBack()`.
> - `Action` — `M` (controller Y/Triangle). Fires `themeContext.openGameActions(currentFocusedGameId)` when a game is focused.
> - `Settings` — controller `Start`. Toggles the settings overlay.
>
> Themes own only what the layout dictates:
> - Arrow-key navigation (carousel left/right, list up/down, grid up/down/left/right).
> - `Enter`/`Return` as "activate focused" — pages call `themeContext.launchGame(...)` or `themeContext.navigateToSystem(...)` directly.
> - Publishing the currently focused game via `themeContext.currentFocusedGameId` when one exists, or `-1` otherwise so the Action key no-ops cleanly.

## Intentional behaviour changes

1. **Backspace is now globally `Back`**, not only inside the modern theme. Previously Backspace did nothing on the SystemPage or EmptyStatePage; now it closes settings / pops the theme stack / opens settings, matching `Escape`.
2. **M key works on any page that publishes `currentFocusedGameId`**, not only inside the modern GameListPage's focus tree. The known "M doesn't always open GameActionPopup" bug surfaced during the BaseToast/BaseModalCard smoke test (2026-05-21) is expected to fix for free — `Shortcut` dispatches at the application level and doesn't depend on focus traversal reaching the theme page root.
3. **No behaviour change to modals.** GameActionPopup, ResumeStateDialog, SettingsOverlay, VirtualKeyboard, RA login prompt, update-confirm all keep their own `Keys.onPressed`. The new `Shortcut`s are gated on `!modalVisible` the same way the existing Escape `Shortcut` already is.

## Risks

- **Text inputs and Backspace.** All current text fields live inside modals (Scraper login, RA login, Virtual Keyboard, search) whose own focus contexts consume Backspace before it bubbles to the Shortcut. Plus the Shortcut is gated on modal-visibility flags. If a future top-level page wants a text input, it must consume Backspace locally — this is the standard QML pattern (`event.accepted = true` in the field's `Keys.onPressed`) and worth a one-line note in the theme README.
- **Theme migration.** Only the bundled `modern` theme exists today, so no external theme breakage. The README + contract change land together.
- **Property-write hot path.** Writing `currentFocusedGameId` on every arrow press is a single int store + signal emit — negligible.
- **Hint set on EmptyStatePage.** Empty state previously got its hints from AppWindow's hardcoded branch. After the change, the page itself declares `[{action: "start", label: "Settings"}]`. Verify visually the strip looks identical.

## Testing

No automated UI tests for theme keys in this repo. Manual smoke covers all paths:

- **System page**: Left/Right cycles the carousel; Enter navigates to system; `Esc` opens settings; `Backspace` opens settings; `M` does nothing (no focused game); controller Start opens settings.
- **Game list page**: Up/Down navigates the list; Enter launches; `Backspace` goes back to system; `Esc` goes back; `M` opens GameActionPopup.
- **With GameActionPopup open**: `Backspace` closes the popup (popup's own handler), `Esc` closes it, `M` does nothing (Shortcut gated).
- **With SettingsOverlay open**: `Esc` closes / goes back through sub-pages; `M` does nothing.
- **During a libretro game**: `Esc` toggles InGameMenu (existing path); `M`/`Backspace` do nothing (Shortcut gated by `app.gameRunning`).
- **During an external-process game**: Cmd+Shift+Esc still opens InGameMenuPanel via the Carbon hotkey; `M`/`Backspace` are no-ops in our window since we're not focused.
- **Empty state page**: Start opens settings; arrow keys still drive the empty-state's existing internal nav.
- **Build verification**: `cmake --build cpp/build-x86_64 -j8` plus the `macdeployqt` + ad-hoc codesign step per [[build-cmake-needs-macdeployqt]].

## Net change estimate

- `theme_context.{h,cpp}`: +~15 LOC
- `AppWindow.qml`: ~0 net (replace hardcoded hints with property read; add M Shortcut; extend Esc Shortcut to also accept Backspace)
- `themes/modern/GameListPage.qml`: −12 LOC (drop universal `Keys.onPressed`), +~6 LOC (hints + focusedId write/clear)
- `themes/modern/SystemPage.qml`: +~6 LOC (hints + focusedId reset)
- `EmptyStatePage.qml`: +1 LOC
- `themes/README.md`: new, ~30 LOC

## Out of scope (logged follow-ups already in roadmap)

- SetupWizard pages — separate focus tree, own Dialog window, immediate-write semantics. Tracked separately in the roadmap.
- RetroAchievementsSettings.qml keyboard-nav redesign — own brainstorm cycle.
- AchievementsPage.qml migration onto `GenericListPage.headerComponent` — small follow-up.
- Hotkey dialog footer-shortcut keys broken on per-emulator instance — event-tracing fix.
