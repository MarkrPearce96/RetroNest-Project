# Hotkey Settings Page — Design Spec

## Overview

Rewrite the Qt Widgets `HotkeySettingsPage` dialog to match PCSX2's hotkey system: expand from 9 hotkeys to ~39 across 4 categories, add a left sidebar for category navigation, and reuse shared binding widget code from the controller mapping work.

## Scope

**In scope:**
- Left sidebar with 4 categories (Speed Control, System, Save States, Audio)
- Grid layout per category (label + binding button)
- Expand PCSX2 adapter from 9 to 39 hotkey definitions
- Click-to-bind, right-click-to-clear (existing pattern)
- Per-controller-type display names (reuse `binding_display.h`)
- Restore Defaults + Close bottom bar
- Reuse `binding_widget_common.h` shared code

**Out of scope:**
- Navigation category (Toggle Fullscreen removed — app is always fullscreen)
- Open Pause Menu, Achievements, Leaderboards (our UI controls these)
- Shut Down VM (future universal hotkey in our app)
- Toggle Input Recording, Toggle Mouse Lock (niche, we handle input)
- Profile management (hotkeys are shared across all uses)

## Layout

```
┌──────────────────────────────────────────────────────┐
│  Hotkey Settings                                      │
├────────────┬─────────────────────────────────────────┤
│            │                                          │
│  Speed     │  Category Title                          │
│  Control ◄ │                                          │
│            │  Label              [Binding Button]     │
│  System    │  Label              [Binding Button]     │
│            │  Label              [Binding Button]     │
│  Save      │  ...                                     │
│  States    │                                          │
│            │                                          │
│  Audio     │                                          │
│            │                                          │
├────────────┴─────────────────────────────────────────┤
│                    [Restore Defaults]  [Close]        │
└──────────────────────────────────────────────────────┘
```

## Left Sidebar

- 4 entries: Speed Control, System, Save States, Audio
- Selected entry highlighted with accent colour
- Clicking switches the right content area
- Styled to match controller mapping dialog sidebar

## Content Area

- Category title at top (bold, primary text colour)
- Subtitle: "Click a binding to remap. Right-click to clear."
- Grid layout: label (left, 220px) + binding button (right, fills remaining)
- Binding buttons use `BindBtn` from `binding_widget_common.h`
- Display names use `displayBinding()` from `binding_display.h` with controller type detection
- Scrollable if category has many items (Save States has 27 entries)

## Hotkey Definitions (39 total)

All stored in `[Hotkeys]` INI section.

### Speed Control (8 hotkeys)

| Label | INI Key | Default |
|-------|---------|---------|
| Toggle Pause | TogglePause | SDL-0/Guide |
| Frame Advance | FrameAdvance | |
| Toggle Frame Limit | ToggleFrameLimit | |
| Toggle Turbo / Fast Forward | ToggleTurbo | Keyboard/Period |
| Turbo / Fast Forward (Hold) | HoldTurbo | |
| Toggle Slow Motion | ToggleSlowMotion | Keyboard/Shift & Keyboard/Backspace |
| Increase Target Speed | IncreaseSpeed | |
| Decrease Target Speed | DecreaseSpeed | |

### System (3 hotkeys)

| Label | INI Key | Default |
|-------|---------|---------|
| Reset Virtual Machine | ResetVM | |
| Reload Patches | ReloadPatches | |
| Swap Memory Cards | SwapMemCards | |

### Save States (25 hotkeys)

| Label | INI Key | Default |
|-------|---------|---------|
| Select Previous Save Slot | PreviousSaveStateSlot | Keyboard/Shift & Keyboard/F2 |
| Select Next Save Slot | NextSaveStateSlot | Keyboard/F2 |
| Save State To Selected Slot | SaveStateToSlot | Keyboard/F1 |
| Load State From Selected Slot | LoadStateFromSlot | Keyboard/F3 |
| Load Backup State | LoadBackupStateFromSlot | |
| Save State and Select Next Slot | SaveStateAndSelectNextSlot | |
| Select Next Slot and Save State | SelectNextSlotAndSaveState | |
| Save State To Slot 1 | SaveStateToSlot1 | |
| Load State From Slot 1 | LoadStateFromSlot1 | |
| Save State To Slot 2 | SaveStateToSlot2 | |
| Load State From Slot 2 | LoadStateFromSlot2 | |
| Save State To Slot 3 | SaveStateToSlot3 | |
| Load State From Slot 3 | LoadStateFromSlot3 | |
| Save State To Slot 4 | SaveStateToSlot4 | |
| Load State From Slot 4 | LoadStateFromSlot4 | |
| Save State To Slot 5 | SaveStateToSlot5 | |
| Load State From Slot 5 | LoadStateFromSlot5 | |
| Save State To Slot 6 | SaveStateToSlot6 | |
| Load State From Slot 6 | LoadStateFromSlot6 | |
| Save State To Slot 7 | SaveStateToSlot7 | |
| Load State From Slot 7 | LoadStateFromSlot7 | |
| Save State To Slot 8 | SaveStateToSlot8 | |
| Load State From Slot 8 | LoadStateFromSlot8 | |
| Save State To Slot 9 | SaveStateToSlot9 | |
| Load State From Slot 9 | LoadStateFromSlot9 | |
| Save State To Slot 10 | SaveStateToSlot10 | |
| Load State From Slot 10 | LoadStateFromSlot10 | |

### Audio (3 hotkeys)

| Label | INI Key | Default |
|-------|---------|---------|
| Toggle Mute | Mute | |
| Increase Volume | IncreaseVolume | |
| Decrease Volume | DecreaseVolume | |

## Backend Changes

### PCSX2 Adapter

`hotkeyBindingDefs()` expanded from 9 to 39 entries. Same `HotkeyDef` struct:
```cpp
{label, group, "Hotkeys", iniKey, defaultValue}
```

Group names: "Speed Control", "System", "Save States", "Audio"

### HotkeySettingsPage (rewrite)

Replace current simple grouped list with sidebar + content layout:
- Constructor: builds sidebar + stacked/switched content per category
- Reuses `BindBtn` from `binding_widget_common.h`
- Reuses `displayBinding()` from `binding_display.h`
- Same capture flow as controller binding widgets (connect to SdlInputManager signals)
- `loadBindings()` reads from `m_appController->hotkeyBindings()`
- Save via `m_appController->saveHotkey(section, key, value)`
- Clear via right-click → `m_appController->clearHotkey(section, key)`
- Reset via `m_appController->resetHotkeys()`

### AppController

No API changes needed — existing `hotkeyBindings()`, `saveHotkey()`, `clearHotkey()`, `resetHotkeys()` already handle everything. The adapter expansion provides the new definitions automatically.

## Dialog Sizing

- Minimum size: 900 x 600
- Default size: 900 x 600
- Sidebar width: 160px fixed

## Styling

Matches controller mapping dialog:
- Same colour scheme from `binding_widget_common.h`
- Same sidebar styling (#1e1e3a background, accent highlight)
- Same bottom bar pattern (Restore Defaults + Close)
- Same binding button style (BindBtn with click-to-capture, right-click-to-clear)
