# `BaseToast` + `BaseModalCard` — collapse toast and modal-card duplication

**Date:** 2026-05-20
**Status:** Approved (brainstorming)
**Roadmap item:** Tier 2 #7 (`refactor-roadmap.md`)

## Problem

The roadmap originally framed item #7 as one `BaseNotification` / `PopupBase.qml` covering 6 files: `ActionToast`, `AchievementToast`, `UpdateNotification`, `ProgressPopup`, `ResumeStateDialog`, `GameActionPopup`. The audit showed those 6 files actually live in **three distinct categories**, not one:

| File | Category | Scrim | Animation | Timer | Input |
|------|----------|-------|-----------|-------|-------|
| `ActionToast.qml` | Corner pill toast | — | slide+fade (180 ms) | auto-dismiss | none |
| `AchievementToast.qml` | Corner pill toast | — | slide+fade (180 ms) | auto-dismiss | none |
| `UpdateNotification.qml` | Inline pill (queued) | — | fade only | auto-dismiss | click only |
| `ProgressPopup.qml` | Qt `Popup` built-in | Qt-implicit | none | none | NoAutoClose |
| `ResumeStateDialog.qml` | Modal scrim + card | yes | fade only | none | Up/Down/Enter/Esc |
| `GameActionPopup.qml` | Modal scrim + card | yes | none | none | Up/Down/Enter/Esc + sub-state |

Real duplication is narrower than "6 files":

- **`ActionToast` ↔ `AchievementToast`** share the slide+fade transform + `Behavior on opacity` + `Behavior on y` + auto-dismiss `Timer` + `visibleState` lifecycle (~20 LOC duplicated).
- **`ResumeStateDialog` ↔ `GameActionPopup`** share the scrim `Rectangle` + centered card `Rectangle` (radius 12, `rgba(0.12, 0.12, 0.14, 0.95)` fill, `rgba(1, 1, 1, 0.1)` border) + `Escape`/`Back` handling (~40 LOC duplicated).
- `UpdateNotification` is unique (inline + queue + buttons + fade-only).
- `ProgressPopup` uses Qt's built-in `Popup` type — different mechanism, no clean sibling.

## Goal

Two narrow base components — not one unified type:

- **`BaseToast.qml`** owns the slide+fade chrome + dismiss timer + show/hide lifecycle. `ActionToast` and `AchievementToast` migrate onto it.
- **`BaseModalCard.qml`** owns the scrim + centered card + Escape/Back handling + close-on-scrim-click. `ResumeStateDialog` and `GameActionPopup` migrate onto it.

Each caller declares its bespoke content using QML's default-property pattern. The caller writes zero animation or scrim plumbing; the focus/navigation state stays in the caller because it differs per dialog (e.g. ResumeStateDialog's no-wrap two-button list vs. GameActionPopup's wrap-around five-action list with a confirmation sub-state).

## Non-goals

- No new pattern for `UpdateNotification` (inline pill with queue and clickable buttons — its own shape; no clean shared kernel with the other five).
- No new pattern for `ProgressPopup` (uses Qt's `Popup` type with modal closure semantics; rewriting it would be a redesign, not a refactor).
- No behavior changes to focus navigation, action handlers, or signal contracts.
- No animation or visual changes to the four migrated files beyond the two deliberate diffs documented under "Two intentional behavior changes" below.

## `BaseToast.qml` API

Location: `cpp/qml/AppUI/BaseToast.qml`

```qml
import QtQuick

Item {
    id: root
    default property alias contentItem: contentHolder.data

    property int duration: 1500
    property bool sticky: false
    property bool visibleState: false

    width: contentHolder.childrenRect.width
    height: contentHolder.childrenRect.height
    visible: visibleState || contentHolder.opacity > 0.0

    function show() {
        hideTimer.stop()
        visibleState = true
        if (!sticky) hideTimer.restart()
    }

    function hide() {
        hideTimer.stop()
        visibleState = false
    }

    Timer {
        id: hideTimer
        interval: root.duration
        repeat: false
        onTriggered: root.visibleState = false
    }

    Item {
        id: contentHolder
        width: childrenRect.width
        height: childrenRect.height
        opacity: root.visibleState ? 1.0 : 0.0
        transform: Translate {
            y: root.visibleState ? 0 : -10
            Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }
}
```

**Properties:**
- `duration` — auto-dismiss timeout in ms. `ActionToast` uses 1500; `AchievementToast` uses 6000.
- `sticky` — when `true`, `show()` does not start the dismiss timer (used by `ActionToast`'s Fast-Forward indicator path).

**Functions:**
- `show()` — sets `visibleState = true` and (if not sticky) restarts the dismiss timer.
- `hide()` — sets `visibleState = false` and cancels the timer.

**Default-property semantics:** children declared by the caller become children of `contentHolder`. The wrapper applies the slide-from-(-10 px)+fade animation; caller writes none of it.

## `BaseModalCard.qml` API

Location: `cpp/qml/AppUI/BaseModalCard.qml`

```qml
import QtQuick

Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 150

    default property alias cardContent: card.data
    property int cardWidth: 400
    property real cardHeight: 0      // > 0: explicit; 0: auto (children's bounding box + 48 padding)
    property bool closeOnScrimClick: true

    signal closeRequested()

    function open() {
        visible = true
        forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.7)
        MouseArea {
            anchors.fill: parent
            onClicked: { if (root.closeOnScrimClick) root.closeRequested() }
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: root.cardWidth
        height: root.cardHeight > 0 ? root.cardHeight : childrenRect.height + 48
        radius: 12
        color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
        Behavior on height { NumberAnimation { duration: 150 } }
        // Caller-declared children land here (default-property alias to card.data).
        // Their `parent` is the card Rectangle, so existing anchors like
        // `parent.bottom; bottomMargin: -36` resolve to the card's bottom —
        // matching what GameActionPopup.ButtonHints already does today.
    }

    Keys.onPressed: function(event) {
        if (!visible) return
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            event.accepted = true
            root.closeRequested()
        }
    }
}
```

**Properties:**
- `cardWidth` — card width in px (`ResumeStateDialog` uses 420; `GameActionPopup` uses 400).
- `cardHeight` — when > 0, the card uses this explicit height (used by `GameActionPopup` which has its own dynamic `popupState`-dependent height formula); when 0 (default), the card auto-tracks `childrenRect.height + 48` for callers that lay out with a single anchored `Column` (used by `ResumeStateDialog`).
- `closeOnScrimClick` — clicking the scrim emits `closeRequested()`. Defaults to `true`.

**Signal:**
- `closeRequested()` — emitted on Escape, Back, and (if `closeOnScrimClick` is true) scrim click. The caller decides what to do — hide, run cleanup, return focus elsewhere — by handling `onCloseRequested`.

**Function:**
- `open()` — sets `visible = true` and grabs active focus.

**Default-property semantics:** children declared by the caller become children of the card `Rectangle` directly (not a wrapping `Column`). The caller chooses its own layout — `Column`, explicit anchored items, whatever fits. This preserves `GameActionPopup`'s `ButtonHints` block, which uses `anchors.bottom: parent.bottom; bottomMargin: -36` to float 36 px BELOW the card — `parent` resolves to the card Rectangle (the direct visual parent), and the card doesn't `clip: true` so the negative-margin overflow renders correctly.

**Focus navigation stays in the caller:**
- `ResumeStateDialog` has a 2-button list with no-wrap Up/Down nav.
- `GameActionPopup` has 5 actions with wrap-around Up/Down nav and a confirmation sub-state with Left/Right nav.
- Centralizing these would force one navigation API to satisfy both, which would either over-generalize or fail to express GameActionPopup's sub-state. Each caller keeps its `Keys.onUpPressed` / `Keys.onDownPressed` / `Keys.onReturnPressed` handlers; the base provides only Escape/Back.

## Per-file conversion

### `ActionToast.qml` (93 → ~50 LOC)

`Item` root replaced by `BaseToast` root. The `Rectangle` pill becomes a direct child of `BaseToast` (placed automatically into `contentHolder`). All animation, timer, show/hide, and `visibleState` plumbing is removed (provided by the base). Properties `iconSource`, `label`, `duration`, `sticky` remain on the caller. `Image` + `Text` row stays as-is.

### `AchievementToast.qml` (146 → ~95 LOC)

Same shape: `BaseToast` root, `duration: 6000`. Caller keeps `property string header / title / description / imageUrl` plus `function show(t, d, url)` and `function showWithHeader(h, t, d, url, durationMs)` — these wrap parameter handling around `BaseToast.show()`. The 380-px Rectangle card with gold border + badge + text column remains the inline child.

### `ResumeStateDialog.qml` (131 → ~85 LOC)

`Item` root replaced by `BaseModalCard` root with `cardWidth: 420`. `cardHeight` is left at default `0` (the base auto-tracks `childrenRect.height + 48`). Scrim and card Rectangles deleted (provided by the base). Caller's `Text` title, `Text` body, and `Repeater` of two button rows are declared inside a `Column` that anchors `top: parent.top; left/right: parent.left/right; margins: 24` — `parent` here is the card Rectangle, the direct visual parent of the default-property children. The `Up`/`Down`/`Return` handlers stay in the caller. `Escape`/`Back` handling is removed (provided by the base via `closeRequested`); caller wires `onCloseRequested` to its existing `close()` logic.

### `GameActionPopup.qml` (308 → ~230 LOC)

`Item` root replaced by `BaseModalCard` root with `cardWidth: 400`. Caller sets `cardHeight` to its existing dynamic formula (the same `titleText.anchors.topMargin + titleText.height + 8 + (popupState === "actions" ? actionColumn.height : confirmColumn.height) + 16` expression). Scrim and card Rectangles deleted. Caller's title `Text`, `actionColumn`, `confirmColumn`, and `ButtonHints` footer are declared as direct children of the BaseModalCard — their `parent` is the card Rectangle. Existing anchors continue to work because the base's default-property alias drops children straight into the card, not into a wrapping `Column`. The 2D focus state machine (`popupState === "actions"` vs `"confirm"` with their distinct Up/Down/Left/Right handlers) stays in the caller. `Escape`/`Back` handling is removed; caller wires `onCloseRequested`.

**`ButtonHints` footer anchor — preserved without changes:** the existing `anchors.bottom: parent.bottom; anchors.bottomMargin: -36` resolves to the card's bottom edge with 36 px outside the card (negative-margin overflow renders correctly because the card doesn't `clip: true`). This works in the migrated form because the default-property alias points at `card.data`, so the `ButtonHints` block's `parent` is the card Rectangle — same as before.

**Card height smooth transition:** the existing `GameActionPopup` has `Behavior on height { NumberAnimation { duration: 150 } }` on the card, so it smoothly resizes when toggling between `actions` and `confirm` sub-states. Since `cardHeight` is now a `real` property on `BaseModalCard` driving the inner card's `height`, the `Behavior` needs to live on `cardHeight` — `BaseModalCard` provides `Behavior on cardHeight { NumberAnimation { duration: 150 } }` by default. Harmless for `ResumeStateDialog` (which leaves `cardHeight` at 0 and lets the auto-binding drive height; the auto path doesn't go through `cardHeight` so the Behavior is never triggered for it).

## CMakeLists.txt change

Add `qml/AppUI/BaseToast.qml` and `qml/AppUI/BaseModalCard.qml` to the AppUI module's `QML_FILES` list, adjacent to `GenericListPage.qml` and `GenericMultiCardPicker.qml` (the previous-refactor pattern). No other CMake change.

## Two intentional behavior changes

1. **`ResumeStateDialog` gains close-on-scrim-click.** Currently its scrim is a `MouseArea` that catches but does not close. After the migration, `closeOnScrimClick` defaults to `true`, so clicking the scrim closes the dialog. Modern UX expectation; matches `GameActionPopup`'s existing behavior. If the strict-no-close behavior is desired, set `closeOnScrimClick: false` on the caller.
2. **`ResumeStateDialog` z-index changes from 200 to 150.** `BaseModalCard` uses `z: 150`. Currently `ResumeStateDialog` is `z: 200`. The dialog is shown in the same context as `GameActionPopup` (over the game grid), so 150 is sufficient and matches its sibling. If a future overlay must layer above modal cards, both dialogs move together.

## Estimated LOC impact

| Change | Lines |
|------|------|
| New `BaseToast.qml` | +50 |
| New `BaseModalCard.qml` | +70 |
| `ActionToast.qml` | 93 → 50 (−43) |
| `AchievementToast.qml` | 146 → 95 (−51) |
| `ResumeStateDialog.qml` | 131 → 85 (−46) |
| `GameActionPopup.qml` | 308 → 230 (−78) |
| `CMakeLists.txt` | +2 |
| **Net** | **≈ −96 LOC** |

## Smoke test

Build, deploy, sign, launch x86_64 per the `build-cmake-needs-macdeployqt` memory.

1. **`ActionToast`** — open a libretro game; trigger Save State (F5) and Load State (F7) via the in-game menu. The top-right pill slides in, holds ~1.5 s, slides+fades out. Then toggle Fast Forward on; the sticky FF pill stays until FF turns off.
2. **`AchievementToast`** — open a game with RetroAchievements enabled; unlock an achievement (or trigger game-mastered). The larger gold-bordered card slides in from the top-right with the badge image, holds ~6 s, slides+fades out.
3. **`ResumeStateDialog`** — launch a game that has an existing save state. Scrim+card appears. Up/Down toggles between Resume / Start Fresh; Enter triggers the right path; Escape closes. Click the scrim — verify it closes (new behavior; flip `closeOnScrimClick` if undesired).
4. **`GameActionPopup`** — long-press a game card (or whatever opens it). Scrim+card appears with the 5 actions. Up/Down wraps. Enter activates each (Scrape, Achievements, Favorite, Open Folder, Remove). For Remove, the confirmation sub-state appears with Yes/No; Left/Right navigates; Enter on Yes deletes; Escape returns to actions. The card height smoothly resizes between the two sub-states (or jumps — flag if it does, see "Open risks").
5. **No regressions** — `UpdateNotification` and `ProgressPopup` were not touched; they should still work as before. Trigger an update notification (or check that one appears at launch if an emulator has an update available); trigger a scrape (Scrape action → ProgressPopup appears).

## Out of scope

- `UpdateNotification.qml` — inline pill with action buttons + queue. Its own shape; no clean shared kernel with the other five files. Could be revisited if a similar pattern emerges.
- `ProgressPopup.qml` — uses Qt's built-in `Popup` type with `modal: true` and `closePolicy: Popup.NoAutoClose`. The Qt `Popup` API differs enough from the four files in scope that lifting it onto a custom modal-card base would be a redesign, not a refactor.

## Open risks

- **None blocking.** The two design surprises surfaced during spec self-review (card-height smooth transition and `ButtonHints` parent-anchor resolution) are both handled in the API:
  - `BaseModalCard`'s card Rectangle has `Behavior on height { NumberAnimation { duration: 150 } }` baked in, so the `cardHeight`-driven transition in `GameActionPopup` animates smoothly without per-caller work.
  - Default-property children become direct children of the card Rectangle (not a wrapping `Column`), so `ButtonHints`'s existing `anchors.bottom: parent.bottom; bottomMargin: -36` continues to resolve to the card's bottom edge.

  Both will still be verified in the smoke test (steps 3 and 4), but neither needs a fallback escape hatch in the API.

## Follow-ups (none new from this work)

No surfaced follow-ups during brainstorming.
