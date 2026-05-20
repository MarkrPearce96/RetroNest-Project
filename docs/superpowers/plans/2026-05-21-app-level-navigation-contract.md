# App-level navigation contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move universal Back/Action keys + ButtonHints hint-set from per-theme reimplementation into AppUI/AppWindow, with themes declaring focus + hints via `ThemeContext` and a `hints` page property.

**Architecture:** AppWindow owns app-wide `Shortcut`s (`Escape`/`Backspace` → Back, `M` → Action) and a single `ButtonHints` strip whose `hints` array reads from the active StackView page's declared `hints` property. `ThemeContext` gains a `currentFocusedGameId` property so the AppWindow M-Shortcut knows what game (if any) is focused without traversing theme internals. Themes drop the universal handlers and only own their layout-specific arrow nav + Enter/Return activation.

**Tech Stack:** Qt6 (QML + C++17), CMake. No unit tests touched — verification is end-to-end manual smoke per the spec's testing matrix.

**Spec:** `docs/superpowers/specs/2026-05-21-app-level-navigation-contract-design.md`

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `cpp/src/ui/theme_context.h` | Modify | Add `currentFocusedGameId` Q_PROPERTY + signal + setter declaration |
| `cpp/src/ui/theme_context.cpp` | Modify | Implement getter/setter; setter emits change-on-change-only |
| `cpp/qml/AppUI/AppWindow.qml` | Modify | Extend Escape Shortcut to also fire on Backspace; add M Shortcut; change `mainHints.hints` to read from active page |
| `cpp/qml/AppUI/EmptyStatePage.qml` | Modify | Declare `hints` property |
| `themes/modern/GameListPage.qml` | Modify | Drop universal `Keys.onPressed`; add `hints` property; write `currentFocusedGameId` on selection change and reset on destruction |
| `themes/modern/SystemPage.qml` | Modify | Add `hints` property; reset `currentFocusedGameId` in `Component.onCompleted` |
| `themes/README.md` | Create | Theme-author contract doc (hints property, universal-key responsibilities, focused-id publishing) |

---

## Task 1: Add `currentFocusedGameId` property to ThemeContext

**Files:**
- Modify: `cpp/src/ui/theme_context.h`
- Modify: `cpp/src/ui/theme_context.cpp`

- [ ] **Step 1: Add the Q_PROPERTY + signal + member to the header**

Edit `cpp/src/ui/theme_context.h`. Add the Q_PROPERTY line after the existing `gameRunning` Q_PROPERTY (the last one before `public:`):

```cpp
    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)
    Q_PROPERTY(int currentFocusedGameId READ currentFocusedGameId
               WRITE setCurrentFocusedGameId
               NOTIFY currentFocusedGameIdChanged)
```

Add the accessor declarations alongside the other simple getters (e.g., right after `bool isGameRunning() const;`):

```cpp
    bool isGameRunning() const;
    int  currentFocusedGameId() const;
    void setCurrentFocusedGameId(int id);
```

Add the signal alongside the other signals (e.g., right after `void gameRunningChanged();`):

```cpp
    void gameRunningChanged();
    void currentFocusedGameIdChanged();
```

Add the member alongside the other private members:

```cpp
    QString m_currentSystem;
    int m_currentFocusedGameId = -1;
```

- [ ] **Step 2: Implement getter and setter**

Edit `cpp/src/ui/theme_context.cpp`. Append the implementation at the end of the file (after the last existing function, before the file ends):

```cpp
int ThemeContext::currentFocusedGameId() const {
    return m_currentFocusedGameId;
}

void ThemeContext::setCurrentFocusedGameId(int id) {
    if (m_currentFocusedGameId == id) return;
    m_currentFocusedGameId = id;
    emit currentFocusedGameIdChanged();
}
```

- [ ] **Step 3: Build and verify compilation**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-x86_64 -j8
```
Expected: build succeeds.

If you don't yet have `cpp/build-x86_64`, run the configure step first:
```bash
cmake -B cpp/build-x86_64 -S cpp -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)"
```
Per [[build-prefer-x86-only]], iterate against `cpp/build-x86_64/` — the universal build only matters at ship time.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/theme_context.h cpp/src/ui/theme_context.cpp
git commit -m "$(cat <<'EOF'
feat(theme): expose currentFocusedGameId on ThemeContext

Adds a theme→app channel so AppWindow's upcoming app-level M Shortcut
can target the focused game without reaching into theme internals.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: AppWindow — extend Escape Shortcut, add M Shortcut, source hints from active page

**Files:**
- Modify: `cpp/qml/AppUI/AppWindow.qml:723-742` (mainHints hint-set)
- Modify: `cpp/qml/AppUI/AppWindow.qml:837-870` (Escape Shortcut)

- [ ] **Step 1: Change `mainHints.hints` to read from the active page**

Edit `cpp/qml/AppUI/AppWindow.qml`. Locate the `mainHints` ButtonHints block (around line 723–742). Replace the entire `hints:` block expression with this:

```qml
        hints: {
            var page = mainStack.currentItem;
            if (page && page.hints !== undefined) return page.hints;
            return [];
        }
```

Leave the surrounding `ButtonHints { id: mainHints; ...; visible: ...; z: 50 }` and its anchors unchanged.

- [ ] **Step 2: Extend the Escape Shortcut to also fire on Backspace**

In the same file, locate the `Shortcut { sequence: "Escape" ... }` block (around line 837). Change the single `sequence: "Escape"` line to:

```qml
        sequences: ["Escape", "Backspace"]
```

Leave the `enabled:` expression and the full `onActivated:` body unchanged. This makes Backspace fire the exact same handler.

- [ ] **Step 3: Add a new M Shortcut for the Action key**

Immediately after the closing `}` of the Escape `Shortcut` (still inside `ApplicationWindow {}`), insert:

```qml
    // M key — universal Action. Opens GameActionPopup for the currently
    // focused game, when a theme page publishes one. Gated against modals
    // and against running games (libretro path already consumes M? No —
    // M isn't an emulator hotkey; we still gate on !app.gameRunning so
    // it can't fire while a game is the foreground).
    Shortcut {
        sequence: "M"
        enabled: !app.gameRunning
                 && !gameActionPopup.visible
                 && !resumeStateDialog.visible
                 && !settingsOverlay.visible
                 && !app.inGameMenuOpen
                 && !inputManager.virtualKeyboardOpen
        onActivated: {
            var id = themeContext.currentFocusedGameId
            if (id >= 0) themeContext.openGameActions(id)
        }
    }
```

- [ ] **Step 4: Build and launch — verify hint strip is empty (themes don't declare hints yet)**

Build:
```bash
cmake --build cpp/build-x86_64 -j8
```

Then per [[build-cmake-needs-macdeployqt]] redeploy + sign:
```bash
"$(brew --prefix qt@6)/bin/macdeployqt" cpp/build-x86_64/RetroNest.app
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
open cpp/build-x86_64/RetroNest.app
```

Expected: app launches. The bottom button hint strip is **empty** on the system page and game list page (because themes haven't declared `hints` yet — this is fine, expected mid-refactor). The strip still shows correctly when modals/overlays have their own hints. Quit the app.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/AppUI/AppWindow.qml
git commit -m "$(cat <<'EOF'
feat(ui): lift universal Back/Action keys + hint sourcing into AppWindow

- Escape Shortcut now also fires on Backspace (single handler).
- New M Shortcut opens GameActionPopup for themeContext.currentFocusedGameId.
- ButtonHints.hints now reads from mainStack.currentItem.hints so the
  active page (theme or EmptyStatePage) declares its own hint set.

Themes still drive their layout-specific arrow nav and Enter/Return.
Modals catch their own keys before bubbling to the Shortcuts as before.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: EmptyStatePage declares its hint set

**Files:**
- Modify: `cpp/qml/AppUI/EmptyStatePage.qml:4-11`

- [ ] **Step 1: Add the `hints` property to the root**

Edit `cpp/qml/AppUI/EmptyStatePage.qml`. Locate the root `Item { id: root; focus: true; ... }`. After the existing `property int focusIndex: 0` line, add:

```qml
    property var hints: [{action: "start", label: "Settings"}]
```

- [ ] **Step 2: Build, deploy, sign, launch — verify empty-state hint strip**

```bash
cmake --build cpp/build-x86_64 -j8
"$(brew --prefix qt@6)/bin/macdeployqt" cpp/build-x86_64/RetroNest.app
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
open cpp/build-x86_64/RetroNest.app
```

To exercise the empty-state path you can temporarily rename your RetroNest root (or any ROMs directory) so no systems show up — or accept that if you already have games imported you'll see the system page (which still shows an empty strip from this task; that's verified in the next task).

If you can reach the empty state: confirm the bottom strip shows only **"Start: Settings"** — same as before the refactor. Quit.

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/EmptyStatePage.qml
git commit -m "$(cat <<'EOF'
feat(ui): EmptyStatePage declares its hint set

Matches the new theme contract: each top-of-stack page declares its
own hints; AppWindow's single ButtonHints strip renders them.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Migrate `themes/modern/GameListPage.qml`

**Files:**
- Modify: `themes/modern/GameListPage.qml:20-22` (state additions)
- Modify: `themes/modern/GameListPage.qml:58-69` (selectCurrentGame)
- Modify: `themes/modern/GameListPage.qml:94-106` (delete universal Keys.onPressed)

- [ ] **Step 1: Add `hints` property and reset-on-destruction**

Edit `themes/modern/GameListPage.qml`. Right after the existing root state block (after the `property bool hasDetails: ...` line, before the `// Video delay timer` block — around line 25), add:

```qml
    property var hints: [
        {action: "navigate_ud", label: "Browse"},
        {action: "confirm",     label: "Launch"},
        {action: "action",      label: "Actions"},
        {action: "back",        label: "Back", keyboardKey: "Backspace"},
        {action: "start",       label: "Settings"}
    ]

    Component.onDestruction: themeContext.currentFocusedGameId = -1
```

- [ ] **Step 2: Publish the focused game id on every selection change**

Still in `GameListPage.qml`. Locate the `selectCurrentGame()` function (around line 58). After `root.selectedDetails = d` inside the `if (d && d.id !== undefined) { ... }` block, add the focused-id write. The full revised function reads:

```qml
    function selectCurrentGame() {
        root.showVideo = false
        videoPlayer.stop()
        videoPlayer.source = ""
        videoDelayTimer.stop()
        var d = themeContext.gameDetailsByIndex(root.listIndex)
        if (d && d.id !== undefined) {
            root.selectedDetails = d
            gameList.positionViewAtIndex(root.listIndex, ListView.Visible)
            videoDelayTimer.restart()
            themeContext.currentFocusedGameId = d.id
        } else {
            themeContext.currentFocusedGameId = -1
        }
    }
```

- [ ] **Step 3: Delete the now-redundant universal `Keys.onPressed` block**

In the same file, delete lines 94–106 (the entire `Keys.onPressed: function(event) { ... }` block that handles `Key_Backspace`/`Key_Escape`/`Key_Back` and `Key_M`). Leave the four single-key handlers above it (`Keys.onUpPressed`, `Keys.onDownPressed`, `Keys.onReturnPressed`, `Keys.onEnterPressed`) untouched — those are the layout-specific list nav + Enter-to-launch the theme still owns.

After the edit, the keyboard-navigation section should end with `Keys.onEnterPressed: { ... }` and flow directly into the `// ── Refresh on data change ──` comment.

- [ ] **Step 4: Build, deploy, sign, launch — smoke the GameListPage path**

```bash
cmake --build cpp/build-x86_64 -j8
"$(brew --prefix qt@6)/bin/macdeployqt" cpp/build-x86_64/RetroNest.app
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
open cpp/build-x86_64/RetroNest.app
```

From the system page, navigate into any system to reach the game list. Verify:
- Bottom hint strip shows: **D-Pad ↑↓ Browse · Enter Launch · M Actions · Backspace Back · Esc Settings** (or controller equivalents).
- Up/Down moves the highlight, scrolls.
- Enter launches the focused game.
- **Backspace** returns to the system page. ← previously theme-handled, now AppWindow Shortcut.
- **Escape** also returns to the system page. ← previously theme-handled via the same block, now AppWindow Shortcut.
- **M** opens the GameActionPopup for the focused game. ← previously theme-handled, now AppWindow Shortcut routing through `currentFocusedGameId`.
- Inside GameActionPopup: Backspace closes the popup (popup's own handler); M does nothing.
- Quit the running game (if any); return to game list; M still works.

Quit the app.

- [ ] **Step 5: Commit**

```bash
git add themes/modern/GameListPage.qml
git commit -m "$(cat <<'EOF'
refactor(theme/modern): drop universal Back/Action keys; declare hints

GameListPage no longer reimplements Backspace/Escape→navigateBack or
M→openGameActions — those are AppWindow Shortcuts now. The page
publishes the focused game via themeContext.currentFocusedGameId
and declares its hint set on the root for the central strip to render.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Migrate `themes/modern/SystemPage.qml`

**Files:**
- Modify: `themes/modern/SystemPage.qml:9-15` (Component.onCompleted)
- Modify: `themes/modern/SystemPage.qml:17-21` (add hints property)

- [ ] **Step 1: Add `hints` property to the root**

Edit `themes/modern/SystemPage.qml`. Locate the root state block (around line 17, after `Component.onCompleted` and `StackView.onActivated`). Right before `property var systemList: themeContext.systems`, add:

```qml
    property var hints: [
        {action: "navigate_lr", label: "Browse"},
        {action: "confirm",     label: "Select"},
        {action: "start",       label: "Settings"}
    ]
```

- [ ] **Step 2: Reset focused-id when SystemPage activates**

Still in `SystemPage.qml`. Locate `Component.onCompleted` (around line 9). The current body is:

```qml
    Component.onCompleted: {
        root.forceActiveFocus()
        if (systemList.length > 0)
            root.currentArtwork = "assets/artwork/" + systemList[0] + ".webp"
        Qt.callLater(rebuildCarouselModel)
    }
```

Add the focused-id reset as the first line of the body:

```qml
    Component.onCompleted: {
        themeContext.currentFocusedGameId = -1
        root.forceActiveFocus()
        if (systemList.length > 0)
            root.currentArtwork = "assets/artwork/" + systemList[0] + ".webp"
        Qt.callLater(rebuildCarouselModel)
    }
```

Also update `StackView.onActivated` (around line 15) to reset on re-entry:

```qml
    StackView.onActivated: {
        themeContext.currentFocusedGameId = -1
        root.forceActiveFocus()
    }
```

- [ ] **Step 3: Build, deploy, sign, launch — smoke the SystemPage path**

```bash
cmake --build cpp/build-x86_64 -j8
"$(brew --prefix qt@6)/bin/macdeployqt" cpp/build-x86_64/RetroNest.app
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
open cpp/build-x86_64/RetroNest.app
```

On the system page, verify:
- Bottom hint strip shows: **D-Pad ←→ Browse · Enter Select · Esc Settings** (or controller equivalents).
- Left/Right cycles the carousel.
- Enter navigates to the selected system's game list.
- **Backspace** opens settings (no theme stack to pop; AppWindow Shortcut falls through to `settingsOverlay.open()`).
- **Escape** opens settings (same handler).
- **M** does nothing (no focused game).
- Controller Start opens settings.
- Drill in to a game list, press Backspace to return → back on system page, M still no-ops, hint strip shows the carousel hints.

Quit the app.

- [ ] **Step 4: Commit**

```bash
git add themes/modern/SystemPage.qml
git commit -m "$(cat <<'EOF'
refactor(theme/modern): SystemPage declares hints; clears focused id

Aligns SystemPage with the new theme contract — no universal-key
handlers (none existed), hint set declared on the root, and the
focused-game-id is cleared on activation so the AppWindow M Shortcut
no-ops while the user browses systems.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Add `themes/README.md` — theme-author contract

**Files:**
- Create: `themes/README.md`

- [ ] **Step 1: Write the README**

Create `themes/README.md` with:

```markdown
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
```

- [ ] **Step 2: Commit**

```bash
git add themes/README.md
git commit -m "$(cat <<'EOF'
docs(themes): theme-author contract for the new AppWindow input contract

Defines the universal-input contract (Back, Action, Settings), the required
hints property, the currentFocusedGameId publishing convention, and the
top-level Backspace caveat for future themes that need text inputs.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Full integration smoke

**Files:** none (verification only)

- [ ] **Step 1: Clean build + redeploy**

```bash
cmake --build cpp/build-x86_64 -j8
"$(brew --prefix qt@6)/bin/macdeployqt" cpp/build-x86_64/RetroNest.app
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
open cpp/build-x86_64/RetroNest.app
```

- [ ] **Step 2: Walk the full smoke matrix from the spec**

Run through every row of the spec's testing matrix:

- **System page**: Left/Right cycles carousel; Enter navigates; Esc opens settings; Backspace opens settings; M no-op; Start opens settings.
- **Game list page**: Up/Down navigates; Enter launches; Backspace returns to system; Esc returns; M opens GameActionPopup.
- **GameActionPopup open**: Backspace closes; Esc closes; M no-op.
- **SettingsOverlay open**: Esc closes / goes back through sub-pages; M no-op.
- **During a libretro game**: Esc toggles InGameMenu; M and Backspace are no-ops (Shortcut gated by `app.gameRunning`).
- **During an external-process game**: Cmd+Shift+Esc still opens the InGameMenuPanel via the Carbon hotkey; M / Backspace no-op in our window (not focused).
- **Empty state page** (if reachable): Start opens settings; arrow keys still drive internal nav.
- **GameActionPopup M-key bug from #7 smoke (2026-05-21)**: confirm the previously flaky case now works because the Shortcut dispatches at app level, not via focus traversal.

- [ ] **Step 3: Report back any regressions**

Anything that misbehaves: stop, capture the symptom, and surface it before claiming completion. Per CLAUDE.md, "If you can't test the UI, say so explicitly rather than claiming success."

- [ ] **Step 4: Update the refactor roadmap memory after sign-off**

Once the user confirms the smoke matrix passes, append the shipped marker to
`/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`
("Logged follow-ups" section's existing "App-level keyboard shortcuts + ButtonHints" entry → mark shipped with the date, commits, and spec/plan paths).

---

## Self-review

**Spec coverage:**
- ThemeContext property → Task 1.
- AppWindow Shortcut/hints changes → Task 2.
- EmptyStatePage hints → Task 3.
- GameListPage migration (drop handlers, hints, focused-id) → Task 4.
- SystemPage hints + focused-id reset → Task 5.
- Theme README → Task 6.
- Spec's testing matrix → Task 7.
- Risk #1 (text inputs and Backspace) — covered by README's text-input caveat (Task 6) and the GameActionPopup modal smoke in Task 7.

**Placeholder scan:** no TBD/TODO/"similar to" lines; every code change is shown in full.

**Type consistency:** `currentFocusedGameId` / `setCurrentFocusedGameId` / `currentFocusedGameIdChanged` used identically across Tasks 1, 2, 4, 5. `hints` property shape (array of `{action, label, [keyboardKey]}`) consistent across Tasks 2, 3, 4, 5, 6.

**Build steps:** every code task ends with `cmake --build` + `macdeployqt` + `codesign --force --deep --sign -` per [[build-cmake-needs-macdeployqt]].
