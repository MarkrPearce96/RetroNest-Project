# GenericListPage — collapse the four list/grid pages behind one component

**Date:** 2026-05-20
**Status:** Approved (brainstorming)
**Roadmap item:** Tier 1 #4 (`refactor-roadmap.md`)

## Problem

Four QML pages in `cpp/qml/AppUI/` reimplement the same scrolling-list shell —
each with its own outer `Item`, its own `Keys.on*` handlers (when present), its
own empty-state text, and its own arrow-key wrap-around math. Behavior drifts
across them: two have keyboard/controller focus navigation, two are mouse-only;
two have empty-state text, two don't; the header style differs between the
ones that have a header.

The four pages:

1. `AllGamesPage.qml` — Achievements → All Games. ListView, mouse-only, has
   title text, no empty state.
2. `RecentlyPlayedPage.qml` — Achievements → Recently Played. ListView,
   mouse-only, header inside ListView, has empty state.
3. `EmulatorManageGrid.qml` — Settings → Manage Emulators. Flickable +
   Repeater, focus nav with wrap-around, trailing non-data "Coming Soon" row.
4. `ThemesPage.qml` — Settings → Themes. ListView with `currentIndex`, focus
   nav with wrap-around, `positionViewAtIndex` on focus change, `ButtonHints`
   sibling below the list.

In addition, two files are orphaned dead code, kept alive only by build cache:

- `GamesPage.qml` (the pre-theme-system games shell — superseded by
  `themes/modern/GameListPage.qml`)
- `GameGridView.qml` (used only by `GamesPage.qml`)

## Goal

Land one `GenericListPage.qml` component that owns:

- The `ListView` and its arrow-key wrap-around navigation
- Enter/Return → activation signal
- Escape/Back → emit signal; default handler pops `panelStack`
- Optional header text, optional empty-state text, optional trailing list footer
- `positionViewAtIndex` on focus change

…and migrate all four pages to it. Per-page code shrinks to the page's
delegate + page-specific activation handler. Every list page gains uniform
arrow/controller focus navigation, including the two achievement pages that
were previously mouse-only.

Delete `GamesPage.qml` and `GameGridView.qml` in the same PR.

## Non-goals

- The theme-owned pages (`themes/modern/GameListPage.qml`,
  `themes/modern/SystemPage.qml`) are **out of scope**. Per the
  CLAUDE.md theme rules ("Themes own the entire window"), themes implement
  their own list/grid pages by design.
- Roadmap item #10 (`NavigableGrid` / keyboard-nav mixin across the other ~59
  QML files) stays a separate task. We are not refactoring any callers
  outside the four pages listed above.
- No changes to delegate content (game row layout, emulator row layout, theme
  row layout). Only their focused-state border is added or unified.

## Component API

Location: `cpp/qml/AppUI/GenericListPage.qml`

```qml
Item {
    id: root
    focus: true

    // --- Required ---
    property alias model: listView.model           // array or QAbstractItemModel
    required property Component delegate           // page-specific row

    // --- Optional content ---
    property string headerText: ""                 // styled title above list, hidden if empty
    property string emptyText: ""                  // shown centered when list is empty
    property Component listFooter: null            // appended at end of list (e.g. "Coming Soon")

    // --- Optional layout knobs ---
    property real listMargins: 20
    property real itemSpacing: 6

    // --- Signals ---
    signal activated(int index)                    // Enter/Return or row click
    signal backRequested()                         // Escape/Back; default impl pops panelStack
}
```

### Internal behavior

- `ListView` fills the area below the optional header, with
  `currentIndex` starting at 0.
- Arrow keys (Up/Down) move `currentIndex` with wrap-around top↔bottom.
- Enter/Return emits `activated(currentIndex)`.
- Escape/Back emits `backRequested()`. The default implementation runs:
  ```
  if (typeof panelStack !== 'undefined' && panelStack.depth > 1)
      panelStack.pop()
  ```
  A page that needs different back behavior overrides the signal handler.
- `positionViewAtIndex(currentIndex, ListView.Contain)` runs on every
  `currentIndex` change.
- Delegates read `ListView.isCurrentItem` (Qt-native) to style themselves.
  No `focused` property is injected.
- When the list is empty and `emptyText` is non-empty, a centered Text appears
  (style: 14px, `SettingsTheme.textDim`).
- When `headerText` is non-empty, a Text renders above the list (style:
  18px Bold, `SettingsTheme.text`, 20px top/left margin).
- When `listFooter` is set, the Component is bound to `ListView.footer` and
  rendered after the last data row inside the scroll area.

### Mouse interaction contract

`GenericListPage` exposes one public function:

```qml
function activate(index) {
    listView.currentIndex = index
    activated(index)
}
```

Each delegate keeps its own `MouseArea` and calls `listPage.activate(index)`
on click (where `listPage` is the caller-side id of the `GenericListPage`).
This routes mouse and keyboard onto the same activation path and keeps focus
in sync after a click.

This converges mouse and keyboard onto the same activation path, so a
clicked-then-Enter sequence behaves predictably.

## Per-page conversion

| Page | `headerText` | `emptyText` | `listFooter` | `activated` handler | Delegate change |
|------|------|------|------|------|------|
| `AllGamesPage.qml` | `"All Games (" + allGames.length + ")"` | `"No games"` | — | `panelStack.push(achievementsPageComponent, { raGameId: model.raGameId, gameTitle: model.title })` | Add `ListView.isCurrentItem` focused-border state |
| `RecentlyPlayedPage.qml` | `recentGames.length + " recently played games"` | `"No recently played games"` | — | `panelStack.push(achievementsPageComponent, { raGameId: model.gameId, gameTitle: model.title })` | Add focused-border state; existing in-list 12px header is replaced by the new 18px Bold header above |
| `EmulatorManageGrid.qml` | — | — | Component rendering the "Coming Soon" row (currently lines 174–227) | Emit existing `emulatorSelected(emuList[i].id)` | `FocusableItem` switches from `index === root.focusIndex` to `ListView.isCurrentItem` |
| `ThemesPage.qml` | — | — | — | `applyTheme(i)` (existing function) | Delegate switches from `index === root.focusIndex` to `ListView.isCurrentItem`. Page wraps `GenericListPage` + `ButtonHints` in a `ColumnLayout`. |

### Focused-state visual contract (new in AllGamesPage / RecentlyPlayedPage)

Both achievement-page delegates currently use a plain bordered Rectangle. They
gain (matching the pattern already in `ThemesPage.qml:49–60`):

- 2px border in `SettingsTheme.focusBorder` when `ListView.isCurrentItem` is
  true, else the existing 1px `SettingsTheme.border`
- A soft outer glow Rectangle at 0.3 opacity behind the focused row
- `Behavior on border.color` / `border.width` using `SettingsTheme.animFast`

`EmulatorManageGrid` and `ThemesPage` delegates already render this state from
their own `isFocused` property; the only change is the source — read
`ListView.isCurrentItem` instead of `index === root.focusIndex`.

## Dead-code removal

The following files are deleted in the same PR:

- `cpp/qml/AppUI/GamesPage.qml` (104 LOC) — no live references; superseded by
  the theme's `GameListPage.qml`.
- `cpp/qml/AppUI/GameGridView.qml` (32 LOC) — used only by `GamesPage.qml`.

Both must also be removed from the AppUI module's source list in
`cpp/CMakeLists.txt`.

## Estimated LOC impact

| Change | Lines |
|------|------|
| New `GenericListPage.qml` | +~80 |
| `AllGamesPage.qml` | 131 → ~55 (−76) |
| `RecentlyPlayedPage.qml` | 131 → ~55 (−76) |
| `EmulatorManageGrid.qml` | 234 → ~110 (−124) |
| `ThemesPage.qml` | 227 → ~100 (−127) |
| Delete `GamesPage.qml` | −104 |
| Delete `GameGridView.qml` | −32 |
| **Net** | **≈ −459** |

## Smoke-test checklist

Run the app and verify:

1. **Achievements → All Games**: arrow keys move focus with wrap-around;
   focused row shows accent border + glow; Enter/A drills into the game's
   achievement detail; Escape/B pops back.
2. **Achievements → Recently Played**: same as above; empty-state text
   appears when the list is empty.
3. **Settings → Manage Emulators**: arrow keys wrap around; "Coming Soon"
   row is visible at the bottom and is not focusable; Enter/A drills into
   the selected emulator; mouse click still works on every row.
4. **Settings → Themes**: arrow keys + Enter/A applies a theme;
   `ButtonHints` is visible below the list; mouse click still applies.

## Open risks / decisions deferred

- **`ListView.footer` and focus skipping**: Qt's `ListView.footer` is
  rendered after the last delegate but is not part of the keyboard
  navigation model — meaning the "Coming Soon" row will *not* be focusable,
  which is the desired behavior. If, during implementation, we discover this
  isn't true on Qt 6.x, we fall back to rendering the footer as a sibling
  inside the same ColumnLayout (the way `ButtonHints` lives in ThemesPage).
- **`positionViewAtIndex` cost**: Calling it on every `currentIndex` change
  is what `ThemesPage` already does today; no observed perf issue. If
  delegate creation cost grows, we revisit (out of scope here).

## Follow-ups (not part of this work)

- Roadmap item #10 (NavigableGrid mixin across the ~59 other `Keys.onPressed`
  sites) can later adopt `GenericListPage` where the call site is a list, or
  the focus-wrap helper inside it can be split into a JS module if reuse
  outside `GenericListPage` materializes.
- Roadmap item #5 (`GenericMultiCardPicker`) — the ResolutionSettings /
  AspectRatioSettings pages are 2D-focus card pickers, structurally
  different from a 1D list, and stay a separate component.
