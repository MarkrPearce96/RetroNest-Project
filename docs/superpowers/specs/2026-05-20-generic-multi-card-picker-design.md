# GenericMultiCardPicker — collapse the two emulator card-picker settings pages

**Date:** 2026-05-20
**Status:** Approved (brainstorming)
**Roadmap item:** Tier 1 #5 (`refactor-roadmap.md`)

## Problem

`cpp/qml/AppUI/ResolutionSettings.qml` (299 LOC) and `cpp/qml/AppUI/AspectRatioSettings.qml` (299 LOC) are near line-for-line duplicates. Both render a `Flow` of 385-px cards (one per installed emulator that exposes options), each card showing a 14:9 preview image and a row of pill buttons; both wire a 2D card×pill keyboard focus model (Up/Down across card rows, Left/Right within pills with wrap-across-cards) plus a focusable Save button at the bottom-right.

The actual differences between the two files are confined to:

1. **API method names** — `app.quickResolutionOptions` vs `app.quickAspectRatioOptions`; `app.currentResolution` vs `app.currentAspectRatio`; `app.applyQuickResolution` vs `app.applyQuickAspectRatio`.
2. **Option key field** — resolution stores the chosen option by its numeric `value`; aspect ratio stores it by its string `label`. The card-side comparison (`isSelected: selectedKey === modelData.<field>`) and the pending-choice map key are the only places this matters.
3. **Preview image data** — different `previewImages` JSON dictionary (resolution maps int values to `images/res/*.webp`; aspect ratio maps label strings to `images/ar/*.webp`).

Everything else — focus model, layout, theme, save semantics, animations — is identical.

## Goal

Land one `GenericMultiCardPicker.qml` component and migrate both settings pages onto it. The wizard pages (`cpp/qml/SetupWizard/ResolutionPage.qml` + `AspectRatioPage.qml`) are explicitly out of scope: they're not actually twins of the settings pages — they use a different theme namespace (`WizardTheme`), a different backend object (`emulators` not `app`), different save semantics (write-immediately, no Save button), and visually different layouts (ResolutionPage is a row-list, AspectRatioPage is a 2-col grid of cards with hand-drawn 4:3/16:9 mini-previews). Forcing them onto a unified component would either gut their visual distinctness or require so many configuration props that the component stops being one component.

## Non-goals

- Wizard pages (`SetupWizard/ResolutionPage.qml`, `SetupWizard/AspectRatioPage.qml`) — left untouched. Logged as a separate follow-up entry on `refactor-roadmap.md` for a future session.
- Changes to the per-emulator option definitions in C++ (`app.quickResolutionOptions` and friends) — the component consumes them as-is.
- Visual redesign — the new component reproduces the current settings-pages visual exactly.

## Component API

Location: `cpp/qml/AppUI/GenericMultiCardPicker.qml`

```qml
FocusScope {
    id: root
    focus: true

    // --- Required backend hooks ---
    required property var optionsLoader     // (emuId: string) => [{label: string, value: int|string}, ...]
    required property var currentLoader     // (emuId: string) => chosenKey: string
    required property var applyChoices      // (choices: { [emuId: string]: chosenKey }) => void
    required property string optionKeyField // "value" (resolution) or "label" (aspect ratio)

    // --- Optional content ---
    property var previewImages: ({})        // { [emuId]: { [chosenKey]: imagePath } }

    // --- Layout knobs ---
    property int cardWidth: 385
    property int colCount: 2
    property real previewAspect: 14/9       // height = width / previewAspect
}
```

### Internal behavior

- **Card loading.** On `Component.onCompleted` and `StackView.onActivated`, calls `app.allEmulatorStatus()`, filters to `installed && optionsLoader(emuId).length > 0`, and builds `emuCards` array with `{emuId, name, systems, options, current}` per card.
- **Pending choices.** Initialized to each card's `current` value. Mutated only through `selectPill(cardIndex, pillIndex)`, which sets `pendingChoices[card.emuId] = card.options[pillIndex][optionKeyField]`. The component never writes to the backend until Save.
- **2D focus model.** Three integer/bool fields:
  - `focusCard: int` — which card has keyboard focus.
  - `focusPill: int` — which pill within that card.
  - `focusSave: bool` — true when focus is on the Save button (cards become non-focused while this is true).
- **Keyboard nav.**
  - Up: if `focusSave`, return to last row; else move up by `colCount` if possible (no wrap-around top).
  - Down: if last row, jump to Save; else move down by `colCount`.
  - Left: previous pill; if `focusPill === 0` and not on first card, move to last pill of previous card.
  - Right: next pill; if last pill of card and not on last card, move to first pill of next card.
  - Enter / Return: invokes `selectPill(focusCard, focusPill)` on pills, or `save()` on the Save button. `save()` calls `applyChoices(pendingChoices)`.
- **Pill activation visual.** Selected pill = accent background, non-selected = border-color background. Focused or hovered pill = 3-px outline + 1.05× scale (preserved from the current files).
- **Preview lookup.** `previewSource(emuId, chosenKey) = previewImages?.[emuId]?.[chosenKey] ?? ""`. Empty result renders a "Preview" placeholder text inside the 14:9 area.
- **Save button.** Right-aligned, hover-lightens, focus shows a 2-px accent border. Always rendered (even with zero cards; behavior with zero cards is "Save does nothing" because `pendingChoices` is empty — matches current code).
- **Scrolling.** Component is a `Flickable` with a thin custom `ScrollBar`. Identical to the existing pages.

### Why function-property hooks instead of method-name strings

The component never has to know the names of the backend methods it calls. Renaming `app.applyQuickResolution` becomes a one-line edit in the caller (the arrow function). String-based dispatch (`app[methodName](...)`) would silently fail at runtime if the method were renamed, with no compiler help.

## Per-page conversion

### `cpp/qml/AppUI/ResolutionSettings.qml` (299 → ~25 LOC)

```qml
import QtQuick

GenericMultiCardPicker {
    optionsLoader:  (emuId) => app.quickResolutionOptions(emuId)
    currentLoader:  (emuId) => app.currentResolution(emuId)
    applyChoices:   (choices) => app.applyQuickResolution(choices)
    optionKeyField: "value"

    previewImages: ({
        "duckstation": {
            "2": "images/res/duckstation-720p.webp",
            "3": "images/res/duckstation-1080p.webp",
            "4": "images/res/duckstation-1440p.webp",
            "6": "images/res/duckstation-4k.webp"
        }
    })
}
```

### `cpp/qml/AppUI/AspectRatioSettings.qml` (299 → ~20 LOC)

```qml
import QtQuick

GenericMultiCardPicker {
    optionsLoader:  (emuId) => app.quickAspectRatioOptions(emuId)
    currentLoader:  (emuId) => app.currentAspectRatio(emuId)
    applyChoices:   (choices) => app.applyQuickAspectRatio(choices)
    optionKeyField: "label"

    previewImages: ({
        "pcsx2":       { "4:3": "images/ar/pcsx2-4x3.webp",       "16:9": "images/ar/pcsx2-16x9.webp" },
        "duckstation": { "4:3": "images/ar/duckstation-4x3.webp", "16:9": "images/ar/duckstation-16x9.webp" }
    })
}
```

## CMakeLists.txt change

Add `qml/AppUI/GenericMultiCardPicker.qml` to the AppUI module's `QML_FILES` list (around line 343 of `cpp/CMakeLists.txt`). No other CMake change.

## Estimated LOC impact

| Change | Lines |
|------|------|
| Add `GenericMultiCardPicker.qml` | +~250 |
| `ResolutionSettings.qml` | 299 → ~25 (−274) |
| `AspectRatioSettings.qml` | 299 → ~20 (−279) |
| **Net** | **≈ −303** |

## Smoke-test checklist

1. **Settings → Resolution**
   - Cards appear for installed emulators that expose `quickResolutionOptions`. Cards are absent for emulators that don't.
   - Up/Down moves focus by full row (2 cards). Down past the last row lands on Save.
   - Left/Right cycles pills within the focused card; from the last pill of card N to the first pill of card N+1; from the first pill of card N to the last pill of card N−1.
   - Selecting a pill updates the preview image where one is defined (DuckStation 720p/1080p/1440p/4K).
   - Save applies all pending choices via `app.applyQuickResolution`.
   - Mouse click on a pill still selects it and updates focus to that card+pill.
2. **Settings → Aspect Ratio**
   - Same behaviors with AR options + AR preview images (PCSX2 + DuckStation 4:3/16:9).
   - Selecting 4:3 vs 16:9 swaps the preview image.
3. **Edge cases**
   - With zero installed emulators that expose options, the card area is empty and Save is the only focusable item. Down on Save → Up returns to Save (no-op). Save does nothing visible.
   - Hover lightens the Save button and pills; clicking with mouse works in all states.

## Build-and-launch reminder

Per the `build-cmake-needs-macdeployqt` memory entry, after `cmake --build cpp/build-x86_64` run `macdeployqt` + `codesign --force --deep --sign -` before launching. Skipping this produces a duplicate-Qt-load abort, not a working app.

## Follow-ups (not part of this work)

- **Wizard parity.** `SetupWizard/ResolutionPage.qml` and `SetupWizard/AspectRatioPage.qml` are not unified here. They use `WizardTheme`, `emulators.*`, immediate-write semantics, and visually different layouts (list-of-rows vs grid-of-mini-previews). Possible future session: extract a thin wizard-flavored picker shared between just those two files, or extend `GenericMultiCardPicker` with a `themeOverride`/`writeMode` flag — design needed.
- **Adding a third quick-setting in the future** (e.g., post-processing preset) becomes a ~20-LOC settings file.
