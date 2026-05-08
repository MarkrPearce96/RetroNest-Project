# In-Game Menu Overlay — Design

**Date:** 2026-05-09
**Status:** Draft
**Scope:** Make the in-game menu visually appear *over* the running game for external (process-backed) emulators, not over the app's home page. Redesign the menu itself as an OpenEmu-style horizontal HUD.

## Problem

When a user presses the in-game menu hotkey (Cmd+Escape or Select+Circle) while playing a game in PCSX2, DuckStation, PPSSPP, or Dolphin, the menu opens but the user sees the *app's* page (system browser / game list) behind it instead of the game. For libretro cores, the menu correctly appears over the game.

The structural reason: libretro cores render *inside* our Qt window via `EmulationView`, so the in-game menu's `z: 200` overlay sits on top of the still-visible emulation frame for free. External emulators run as separate processes with their own native windows. `MacFullscreen::activateOurApp()` raises our entire fullscreen window in front of the emulator window, and whatever `mainStack` was last showing is what fills the background behind the menu.

## Goal

When an external emulator is running and the user opens the in-game menu, the menu appears as a small floating HUD over the (paused) game frame — visually equivalent to libretro today. The user is not pulled out to the app's home page.

## Non-Goals

- True embedding of native external emulators inside our Qt window. (Investigated: macOS does not allow cross-process `NSWindow` reparenting; alternatives — borderless-window-positioning, ScreenCaptureKit surface capture, library-mode forks — are multi-week-to-multi-month projects of their own. The libretro path already covers the embedding case for systems where a libretro core is acceptable.)
- Changes to libretro pause/resume or rendering. The libretro path keeps its current in-window menu placement.

## Approach summary

Two coordinated changes:

1. **Visual:** rewrite `InGameMenu.qml` as an OpenEmu-style horizontal HUD anchored to the bottom-center of its host. Same component used by both paths.
2. **Architecture:** for external emulators, host the HUD in a new floating `NSPanel`-backed `QQuickWindow` (`InGameMenuPanel`) that floats above the emulator window without activating our app. For the libretro path, the HUD continues to render inside the main window over `EmulationView`.

The whole approach hinges on the fact that the macOS Carbon global hotkey and SDL controller polling already work regardless of foreground state — the trigger is free. The hard part is getting a Qt-rendered widget on top of another process's window, which on macOS requires either (a) activating our app or (b) a non-activating panel at a high window level. We choose (b).

## Architecture

### Two delivery paths

`AppWindow.toggleInGameMenu()` becomes a router based on the current state:

```
isLibretro = mainStack.currentItem && mainStack.currentItem.isEmulationView

if isLibretro:
    open / close InGameMenu inside main window (today's path)
else:
    show / hide InGameMenuPanel (new path)
```

The libretro path keeps its current `gameSession.pauseEmulation()` / `resumeEmulation()` calls. The external path relies on the emulator's own `PauseOnFocusLoss` setting (already configured for each adapter today) — when the panel becomes the system key window, the emulator's window loses key status and pauses itself.

### `InGameMenuPanel` host

A new C++ class `InGameMenuPanel` (`cpp/src/ui/in_game_menu_panel.{h,cpp}`) owns:

- A `QQuickView` loading `InGameMenuPanel.qml`.
- Logic to position/show/hide the underlying `NSWindow`.

`AppController` owns one instance for the app's lifetime, exposes `openInGameMenuPanel()` / `closeInGameMenuPanel()` / `inGameMenuPanelVisible` to QML.

### `NSPanel` configuration

A new helper `MacFullscreen::configurePanelWindow(NSWindow*)` in `macos_fullscreen.mm` applies, at app startup after the panel's QQuickView is realized:

- **Style mask:** `NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel`. The non-activating panel can become *key* (receive keystrokes) without making our app *active*.
- **Window level:** `NSStatusWindowLevel` (25). Sits above normal application windows. Avoids `NSScreenSaverWindowLevel` so we don't fight system UI.
- **Collection behavior:** `NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary | NSWindowCollectionBehaviorTransient`. Appears on every Space (including a fullscreened emulator's), behaves as an auxiliary panel, doesn't show in window listings.
- **Background:** `setOpaque:NO`, `setBackgroundColor:[NSColor clearColor]`, `setHasShadow:NO`. The Qt content draws only the HUD pill; the surrounding panel area is transparent.
- **`hidesOnDeactivate`:** `NO`. Visibility is managed explicitly.

The QQuickView/QWindow uses `Qt.FramelessWindowHint | Qt.Tool` and `Qt::WA_TranslucentBackground`.

### Open / close flow

**Open** (called from the router when external):

1. Resolve target screen via new `MacFullscreen::screenForProcess(pid_t)` — walks `[NSWorkspace runningApplications]`, finds the emulator process's main window using `CGWindowListCopyWindowInfo`, returns its `NSScreen`. Falls back to `[NSScreen mainScreen]`.
2. Resize/position the panel's frame to be `~480 × 100 px` (matches HUD content), bottom-centered with ~32 px bottom margin on that screen.
3. `[panelWindow orderFrontRegardless]` — bring to its window level.
4. `[panelWindow makeKeyWindow]` — panel becomes key. Emulator's window loses key → emulator's `PauseOnFocusLoss` fires → emulator pauses.
5. QML calls existing `inGameMenu.open()` (state init, RA lookup, focus).

**Close** (resume, exit-with-save, exit-without-save, achievements navigation):

1. Run the existing close-action paths (`themeContext.saveAndStopGame(...)` / `stopGame()` / settings overlay navigation) first if applicable.
2. `[panelWindow orderOut:nil]` — panel disappears.
3. macOS automatically returns key status to the next-most-recent key window (the emulator's), which fires its own `windowDidBecomeKey` → emulator resumes via `PauseOnFocusLoss`. We do not — and cannot — `makeKey` on another process's window.

### Hotkey routing

Already global today; no changes:

- `Cmd+Escape` → Carbon `RegisterEventHotKey` → `AppController::onGlobalHotkeyPressed` → `app.globalHotkeyPressed` signal → `AppWindow.qml` calls `window.toggleInGameMenu()`.
- SDL `Select+Circle` → `SdlInputManager::inGameMenuRequested` → same `toggleInGameMenu()`.

`toggleInGameMenu()` itself becomes the libretro/external router described above.

## HUD visual redesign (`InGameMenu.qml`)

`InGameMenu.qml` is rewritten as a horizontal HUD anchored to the bottom-center of its parent. The same component is used by both hosts:

- **Libretro path:** parented inside the main window, anchored to the bottom of the emulation area.
- **External path:** parented inside the panel's QML scene, anchored to the bottom of the panel's small frame.

### Layout

```
┌───────────────────────────────────────────────────────────┐
│  [▶ Resume]  [🏆 Achievements]  [💾 Save & Quit]  [⏹ Quit]  │
└───────────────────────────────────────────────────────────┘
              ↑ ~32 px from bottom edge of host
```

- **Pill container:** rounded `Rectangle`, `radius: 14`, background `Qt.rgba(0.08, 0.08, 0.10, 0.88)`, 1 px subtle white border, soft drop shadow.
- **Row:** horizontal `Row`, ~12 px spacing, ~16 px internal padding.
- **Icon-button:** vertical stack — SVG icon (28×28 source, drawn at ~28 px) on top, label (12 px, secondary text color) below. Hover/focus highlight tile (`Qt.rgba(1,1,1,0.10)`); pressed/active accent tile.
- **No scrim.** The entire window/panel surrounding the HUD is transparent.

### Removed: submenu state

Today's `menuState: "main"` / `"quit"` two-step is removed. `Save & Quit` and `Quit Without Save` become two separate top-level icons. The existing `supportsSaveOnExit` flag toggles whether the `Save & Quit` icon is shown.

### Achievements icon visibility

Driven by the existing `app.hasRACredentials()` and the asynchronously-resolved `raGameId > 0` — same logic as today, just hides the icon (no gap) when not applicable.

### Controller / keyboard navigation

- Left / Right cycles `focusIndex` through visible icon-buttons.
- A / Return activates the focused button.
- B / Back / Esc closes the menu (= Resume).
- M / Triangle: no-op (gameActionPopup doesn't apply in-game).

### Icon assets

Four new SVGs in `cpp/qml/AppUI/images/hud/`:

- `resume.svg` — play / triangle
- `achievements.svg` — trophy
- `save_quit.svg` — floppy + exit, or door + save
- `quit.svg` — power / stop / door-out

Style: monochrome white, rounded line-style, ~24 px viewBox, matching the visual weight of the existing controller SVGs in `images/controllers/`. Initial commit uses simple placeholder shapes; replacing them with finished art is a no-code change.

## Input routing

`SdlInputManager` injects synthetic `QKeyEvent`s targeted at the focused window via Qt's input-routing (effectively `QGuiApplication::focusWindow()`). When the panel becomes key, Qt's `focusWindow` follows. The existing `Keys.on*` handlers inside `InGameMenu.qml` then receive controller events identically in both hosts.

`SdlInputManager` exposes signals (`navigateStart`, `navigateShift`, `inGameMenuRequested`) consumed by `Connections` blocks. Today the consumer lives in `AppWindow.qml`. The panel's QML scene gets its own `Connections` block (or the existing one is gated). To prevent double-handling:

- `AppController` exposes `inGameMenuPanelVisible` (bool) to QML.
- `AppWindow.qml`'s `inputManager` `Connections` block gates relevant handlers on `!app.inGameMenuPanelVisible`.
- `InGameMenuPanel.qml`'s `Connections` block consumes them when the panel is open.

## Pause behavior — the load-bearing assumption

The approach depends on each external emulator pausing when its window loses key status, regardless of whether our app activates. Each adapter already configures its emulator's pause-on-focus-loss option. Implementations differ slightly per emulator:

| Emulator | Setting | Risk |
|----------|---------|------|
| PCSX2 | `EmuCore/PauseOnFocusLoss = true` | Low — uses Qt window-focus events |
| DuckStation | `Main/PauseOnFocusLoss = true` | Low — uses Qt window-focus events |
| PPSSPP | `iPauseOnLostFocus = 1` | **Medium** — historically finicky on macOS |
| Dolphin | `[General] PauseOnFocusLost = True` | Low — Qt window focus |

PPSSPP is the only meaningful unknown. Verification happens during implementation; if PPSSPP fails to pause with a non-activating panel, fallback options include synthesizing the emulator's own pause hotkey (`CGEventPostToPid`) on panel open, or scoping PPSSPP back to the Option-A approach (transparent main window) while the others use the panel. The fallback decision is captured here so future-us doesn't re-litigate it.

## Edge cases

- **Game ends from the menu (Quit / Save & Quit):** `themeContext.stopGame()` / `saveAndStopGame()` triggers emulator process exit → `onGameFinished` → close panel + clear in-game state. Same flow as today, with `panel.hide()` swapped in for `inGameMenu.close()`.
- **Achievements navigation:** the settings overlay lives in the main window. When `onAchievementsRequested` fires from the panel, call `MacFullscreen::activateOurApp()`, hide the panel, then open `settingsOverlay.navigateToAchievements(...)`. The user is then in the app proper — a deliberate context switch, matching today's behavior.
- **Resume state dialog / RA login prompt / update notifications:** all confined to the main window. None can fire while a game is running and the panel is open (RA login fires only at game start).
- **Multi-monitor / emulator on a non-primary display:** the panel's screen is resolved at *each* open via `screenForProcess(pid)`. We do not track screen changes while the panel is open — paused state means the user can't move the emulator window between displays mid-menu.
- **App quit while panel is visible:** panel is owned by `AppController`; destroyed on app shutdown.
- **Libretro game running:** panel is never shown; in-window menu path used.

## Files

### New

- `cpp/src/ui/in_game_menu_panel.h` / `.cpp` — `InGameMenuPanel` host class.
- `cpp/qml/AppUI/InGameMenuPanel.qml` — `Window` wrapping `InGameMenu`.
- `cpp/qml/AppUI/images/hud/resume.svg`
- `cpp/qml/AppUI/images/hud/achievements.svg`
- `cpp/qml/AppUI/images/hud/save_quit.svg`
- `cpp/qml/AppUI/images/hud/quit.svg`

### Modified

- `cpp/qml/AppUI/InGameMenu.qml` — full visual rewrite (HUD layout, no scrim, no submenu).
- `cpp/qml/AppUI/AppWindow.qml` — `toggleInGameMenu()` becomes the libretro/external router; gate `inputManager` `Connections` on `!app.inGameMenuPanelVisible`.
- `cpp/src/ui/app_controller.h` / `.cpp` — own the `InGameMenuPanel`; expose open/close + visibility to QML.
- `cpp/src/core/macos_fullscreen.h` / `.mm` — add `configurePanelWindow(NSWindow*)` and `screenForProcess(pid_t)`.
- `cpp/CMakeLists.txt` — register new sources and the new QML file.

## Risks / things to verify during implementation

1. **PPSSPP pause behavior** with a non-activating panel as the key window — highest risk; historically finicky on macOS. Fallback: synthesize the emulator's own pause hotkey on open, or use Option A for PPSSPP only.
2. **`NSWindowCollectionBehaviorFullScreenAuxiliary`** behavior when the emulator is in macOS native fullscreen — needs to actually appear above the emulator's Space, not get pushed behind.
3. **SDL controller event routing** while the panel is key — assumes Qt's `focusWindow` follows. If not, `SdlInputManager` needs a `setActiveWindow(QWindow*)` setter that the panel calls on show/hide.
4. **`makeKeyWindow` on a non-activating panel while a different app is foreground** — should work per Apple docs; smoke-test before committing to full implementation. If it fails, the whole approach falls apart and we revert to Option A (transparent main window).

## Fallback plan (if the panel approach fails verification)

The previously-considered Option A is the documented fallback: make the main window's underlying `NSWindow` non-opaque (`setOpaque:NO`), bind `ApplicationWindow.color` and `mainStack.visible` to a derived `externalEmulatorMenuOpen` flag so that during the in-game menu over an external emulator the main window goes transparent and the emulator window (now stacked behind ours) shows through. Reuses today's `activateOurApp()` flow and existing menu/pause/resume plumbing. Smaller diff, lower risk, less elegant macOS UX.

The HUD redesign of `InGameMenu.qml` is independent of which host approach wins and would carry across either way.
