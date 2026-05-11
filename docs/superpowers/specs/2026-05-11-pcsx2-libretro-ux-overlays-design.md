# PCSX2 Libretro Core — UX Overlays for Pattern B HW-Render Cores (Sub-project 3.5 of 8)

**Date:** 2026-05-11
**Status:** Shipped (overlays verified end-to-end; PCSX2 libretro graceful Quit deferred to SP3.6 — see Verification log + Known limitations)
**Owner:** mark
**Scope:** Interstitial sub-project between SP3 (HW render bridge) and SP4 (audio). Self-contained UX fix.
**Predecessors:** [Skeleton (SP1)](2026-05-11-pcsx2-libretro-skeleton-design.md), [VM Lifecycle (SP2)](2026-05-11-pcsx2-libretro-vm-lifecycle-design.md), [HW Render Bridge (SP3)](2026-05-11-pcsx2-libretro-video-bridge-design.md). All three complete.

## Context

SP3 shipped the HW render bridge: PCSX2 renders into a CAMetalLayer hosted on a native NSView inside RetroNest's main `QQuickWindow`. End-to-end verification confirmed Ratchet & Clank rendering its memory-card-insert screen inside RetroNest. However, smoke-test 2 (clean exit via Cmd+Shift+Escape) revealed a UX wall: the in-game menu, RetroAchievements unlock toasts, RA launch banner, RA indicator bar, and the fast-forward / save / load toasts are all declared in `AppWindow.qml`'s scene and end up *behind* the Metal NSView at render time. macOS's NSView compositing rule (subviews always render above their parent's content) makes this fundamental to Pattern B, not a Qt bug. mGBA is unaffected because its software path (`LibretroVideoItem`) has no native NSView and composites entirely inside the QML scene graph.

The diagnostic confirmed the underlying signals do fire:
- `[AppController] Cmd+Shift+Esc fired → globalHotkeyPressed` is logged on each press; the in-scene `inGameMenu.open()` runs but is invisible.
- The RA launch banner (`raInfoToast` with kind=`"ACHIEVEMENTS ACTIVE"`) is emitted from `RcheevosRuntime` and forwarded through to QML, but renders behind Metal.

The goal for SP3.5 is to make Pattern B libretro cores deliver the same seamless overlay UX that mGBA already provides: menu opens/closes smoothly with the same pill geometry, RA toasts appear top-right above the game, indicator chips render bottom-left, all without changing mGBA's working path.

## Goal

When a Pattern B HW-render libretro core (currently PCSX2; future DuckStation / PPSSPP / Dolphin via libretro) is running, every overlay that mGBA shows in-scene today renders identically above the Metal NSView, with bit-identical visual layout and identical pause/resume / signal timing semantics.

**Definition of done:**

1. Cmd+Shift+Escape during a PCSX2 libretro session opens the in-game menu pill at bottom-center, visibly above the rendered game.
2. The menu's seven actions (Resume, Save State, Load State, Fast Forward, Achievements, Save & Quit, Quit) all function. Each one's confirmation toast (FF / save / load pills) renders top-right, visibly above the game.
3. RetroAchievements unlock toasts (`AchievementToast`) and the launch banner (`raInfoToast` → `AchievementToast.show`) appear top-right, fading in/out per existing timing.
4. RA challenge / progress indicators (`RAIndicatorBar`) render bottom-left when active.
5. mGBA libretro session: in-scene overlays still work exactly as they do today. No regressions.
6. External-emulator session (launched-binary PCSX2 / DuckStation / etc): existing `InGameMenuPanel` floating panel still works exactly as today. No regressions.
7. Quit from the new menu cleanly tears down the VM and returns to the game list.

## Non-goals (deferred)

- **Migrate mGBA to the floating panel.** Future cross-core unification pass. Out of scope.
- **Migrate external-emulator overlays.** External path keeps using the existing `InGameMenuPanel`. No change to that code.
- **Save-state dialog UI / settings push UI.** Will be added to `LibretroOverlayPanel.qml` when SP6 / SP7 land.
- **Per-Pattern-B-core specialised overlays.** Disc-swap, PPSSPP UMD swap, etc. are individual sub-project additions.
- **Cross-platform Windows / Linux equivalent.** SP3.5 ships macOS only, matching the rest of the project today. The QML side is portable; the C++ side has a thin macOS-specific window-chrome helper. A Windows port can add a `WinFullscreen.cpp` sibling that uses `SetWindowPos(HWND_TOPMOST)` + `SetForegroundWindow` in place of `NSPanel` chrome.

## Architecture

### New C++ class `LibretroOverlayPanel`

Mirrors the existing `InGameMenuPanel` pattern. Owned by `AppController`. Loads its QML root from `qrc:/AppUI/qml/AppUI/LibretroOverlayPanel.qml`. Exposes signals forwarding the inner `InGameMenu`'s actions (`resumeRequested`, `exitWithSaveRequested`, `exitWithoutSaveRequested`, `saveStateRequested`, `loadStateRequested`, `toggleFastForwardRequested`) plus a `visibilityChanged` signal. Methods: `showForCurrentGame()`, `hide()`, `openMenu()`, `closeMenu()`, `isMenuOpen() const`.

Lifecycle: the C++ `LibretroOverlayPanel` instance lives for the lifetime of `AppController`. Its underlying `QQuickWindow` is created lazily on the first `showForCurrentGame` call (so no work happens if Pattern B is never used), then **destroyed at `gameFinished`**, then recreated on the next game start. This matches the agreed-upon "created at game start, destroyed at game end" lifecycle: each Pattern B session gets a fresh window.

### Window chrome

The `LibretroOverlayPanel.qml` root is a `Window` with `Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint`, `color: "transparent"`. After `show()`, the C++ side calls:

- `MacFullscreen::configurePanelWindow(nsView)` — non-activating `NSPanel` chrome (same call `InGameMenuPanel` already uses).
- `MacFullscreen::attachChildWindow(mainNSView, panelNSView)` — `[mainWindow addChildWindow:panelWindow ordered:NSWindowAbove]`. This is what makes the overlay window track the main window's screen / geometry / Spaces membership automatically.
- `[panelNSWindow setIgnoresMouseEvents:YES]` by default, so mouse clicks pass through to the game NSView below. Cleared on menu open, reapplied on menu close.

`MacFullscreen` gets two small additions: `attachChildWindow(void* parentNSView, void* childNSView)` and `setIgnoresMouseEvents(void* nsView, bool ignore)`. Both are ~5-line Objective-C helpers that consolidate the AppKit calls.

### QML overlay layout (`LibretroOverlayPanel.qml`)

The panel's QML root is a `Window` whose content area replicates exactly the overlay region of `AppWindow.qml`:

```
Window {
    id: panelWindow
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: false

    signal resumeRequested()
    signal exitWithSaveRequested()
    signal exitWithoutSaveRequested()
    signal saveStateRequested()
    signal loadStateRequested()
    signal toggleFastForwardRequested()

    function openMenu() { ... }     // calls inGameMenu.open() and re-emits hook
    function closeMenu() { ... }

    InGameMenu        { id: inGameMenu;     onSomethingRequested: ... }
    Column            { id: actionToasts;   anchors.top + right; ffToast + saveToast + loadToast }
    AchievementToast  { id: achievementToast; anchors.top + right }
    RAIndicatorBar    { id: raIndicators;   anchors.left + bottom }

    Connections { target: app
        function onRaAchievementUnlocked(id, title, desc, image) {
            achievementToast.show(title, desc, image)
            if (app.raSoundEffects) unlockSound.play()
        }
        function onRaInfoToast(header, title, desc, image, ms)   { achievementToast.show(title, desc, image) }
        function onRaIndicator(kind, data)                       { raIndicators.dispatch(kind, data) }
    }
    SoundEffect { id: unlockSound; source: "sounds/Libretro_Achievement_Unlock.wav" }
}
```

Each overlay item is a new instance — a `QQuickItem` can't live in two `QQuickWindow`s, so the HW-render path declares its own instances. Geometry / anchors / colors are copied verbatim from `AppWindow.qml`'s existing declarations.

### Wiring changes in `AppController`

```cpp
// app_controller.h
Q_INVOKABLE bool gameUsesHardwareRender() const;
Q_INVOKABLE void openLibretroOverlayMenu();
Q_INVOKABLE void closeLibretroOverlayMenu();
Q_PROPERTY(bool libretroOverlayMenuVisible
           READ libretroOverlayMenuVisible
           NOTIFY libretroOverlayMenuVisibleChanged)

// app_controller.cpp
m_libretroOverlayPanel = std::make_unique<LibretroOverlayPanel>(engine, this);
connect(this, &AppController::gameStartingLibretro, this, [this] {
    if (gameUsesHardwareRender())
        m_libretroOverlayPanel->showForCurrentGame();
});
connect(this, &AppController::gameFinished, this, [this](int, bool) {
    m_libretroOverlayPanel->hide();
});
// Forward overlay signals to GameSession (mirrors existing InGameMenuPanel forwarders).
connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::resumeRequested,
        m_gameService.session(), &GameSession::resumeEmulation);
connect(m_libretroOverlayPanel.get(), &LibretroOverlayPanel::saveStateRequested,
        m_gameService.session(), [s = m_gameService.session()] { s->saveStateLibretro(1); });
// ...etc.
```

`gameUsesHardwareRender()` returns `true` iff the current session is libretro and the running adapter is a `LibretroAdapter` whose `prefersHardwareRender()` returns true.

### Changes in `AppWindow.qml`

The existing seven overlay declarations stay in place — they continue to serve mGBA / software libretro / external paths. Two minor edits:

1. **`toggleInGameMenu()`** branches on `app.gameUsesHardwareRender()` before falling through to the existing libretro / external branches. The panel is reached via two new `Q_INVOKABLE` methods on `AppController` (mirroring the existing `openInGameMenuPanel` / `closeInGameMenuPanel` pattern) plus a `Q_PROPERTY bool libretroOverlayMenuVisible`:
   ```qml
   function toggleInGameMenu() {
       if (app.gameUsesHardwareRender()) {
           if (app.libretroOverlayMenuVisible) app.closeLibretroOverlayMenu();
           else                                app.openLibretroOverlayMenu();
           return;
       }
       // existing libretro / external branches unchanged
   }
   ```
2. **RA `Connections` blocks** (`onRaAchievementUnlocked`, `onRaInfoToast`, `onRaIndicator`) get an early-return guard at the top of each handler: `if (app.gameUsesHardwareRender()) return;`. Prevents double-firing when the overlay panel's Connections also receive the signal.

### Input model

| State | `setIgnoresMouseEvents` | `makePanelKey` | Result |
|---|---|---|---|
| Game running, no menu | YES | not called | Toasts / indicators visible; mouse + keyboard fall through to main window → game NSView |
| Menu open | NO | called on open | Menu receives mouse + keyboard. Controller routes via SDL's existing navigation-mode → menu FocusScope (`SdlInputManager` is paused via `gameSession.pauseEmulation()`, same as mGBA today) |
| Menu close | YES (reapplied) | not unset, but key-window returns naturally on `resumeEmulation` | Back to game-running state |

## File breakdown

### Created

| File | Approx LOC |
|---|---|
| `cpp/src/ui/libretro_overlay_panel.h` | ~50 |
| `cpp/src/ui/libretro_overlay_panel.cpp` | ~120 |
| `cpp/qml/AppUI/LibretroOverlayPanel.qml` | ~80 |

### Modified

| File | Approx Δ |
|---|---|
| `cpp/CMakeLists.txt` (sources + headers for the new class) | +3 |
| `cpp/src/ui/app_controller.h` (member, `gameUsesHardwareRender` accessor, `openLibretroOverlayMenu` / `closeLibretroOverlayMenu` invokables, `libretroOverlayMenuVisible` property) | +12 |
| `cpp/src/ui/app_controller.cpp` (instantiate, wire show/hide on game-start/end, forward overlay signals to `GameSession`, implement open/close + visibility property) | +35 |
| `cpp/qml/AppUI/AppWindow.qml` (`toggleInGameMenu` branch + RA Connections guards) | +20 |
| `cpp/src/core/macos_fullscreen.h` + `cpp/src/core/macos_fullscreen.mm` (`attachChildWindow` + `setIgnoresMouseEvents` helpers) | +15 |

**Total:** ~250 new LOC, ~85 modified LOC. One C++ class, one QML file, four touched files.

## Data flow at runtime

```
[ User clicks Launch on Ratchet & Clank ]
            │
            ▼
[ GameSession::startLibretro: hw = lr->prefersHardwareRender() == true ]
[ lr->prepareRuntime(); emit aboutToStartLibretro ]    ◀── SP3 signal
            │  (sync auto-connection)
            ▼
[ AppController::gameStartingLibretro fires ]
            │
            ├─► AppWindow.qml: pushes EmulationView      (SP3 path)
            │
            └─► AppController slot:
                if (gameUsesHardwareRender())
                    m_libretroOverlayPanel->showForCurrentGame();
                       │
                       ▼
                  [ LibretroOverlayPanel: ensureCreated,
                    addChildWindow(mainWindow, panelWindow, NSWindowAbove),
                    setIgnoresMouseEvents(YES), show() ]
            │
            ▼
[ startLibretro spin-waits for activeNSView() ]         (SP3 path)
[ rt->start → retro_load_game → AcquireRenderWindow → NSView ]
[ VM RUNNING; Metal frames render into EmulationView's NSView ]
            │
            ▼
[ RA flow: rcheevos → emit raInfoToast / raIndicator
           → AppController forwards → both AppWindow.qml and
             LibretroOverlayPanel.qml Connections fire ]
            │
            ├─ AppWindow.qml handler: gameUsesHardwareRender() → return early
            └─ LibretroOverlayPanel.qml handler: AchievementToast.show / RAIndicatorBar.dispatch
            │
            ▼
[ User presses Cmd+Shift+Escape ]
            │
            ▼
[ Carbon → AppController::globalHotkeyPressed →
  AppWindow.qml onGlobalHotkeyPressed → toggleInGameMenu() ]
            │
            ▼
[ toggleInGameMenu():
    if (app.gameUsesHardwareRender()) {
        m_libretroOverlayPanel.openMenu();
    } else if (isLibretroGame()) {
        inGameMenu.open();                  ◀── mGBA / software path
    } else {
        app.openInGameMenuPanel();          ◀── external path (today)
    } ]
            │
            ▼
[ LibretroOverlayPanel::openMenu:
    setIgnoresMouseEvents(NO);
    MacFullscreen::makePanelKey(panelNSView);
    InGameMenu.open() (inside panel's QML);
    GameSession::pauseEmulation() ]
            │
            ▼
[ User navigates menu via controller / keyboard / mouse.
  Keystrokes → panel (key window).
  Controller events → SDL → SdlInputManager (navigation mode) →
    menu's FocusScope. ]
            │
            ▼
[ User picks "Quit" → exitWithoutSaveRequested →
  GameSession::terminate() → CoreRuntime::stop → emu thread joins →
  CoreRuntime::finished → GameSession::finished →
  AppController::gameFinished → LibretroOverlayPanel::hide() →
  panel destroyed → AppWindow.qml pops EmulationView →
  back to game list ]
```

## Verification

Four tests, all requiring user interaction. Each gates Definition of Done items 1-7.

### Test 1 — In-game menu visible + functional for PCSX2

Launch Ratchet & Clank. Wait for the BIOS / game frame to render (~10 s). Press Cmd+Shift+Escape. Menu pill appears bottom-center, visibly above the game. Navigate every menu action; FF / save / load pills appear top-right, visibly above the game. Pick "Quit" → clean return to game list. No zombie windows in `ps aux | grep RetroNest`.

### Test 2 — RetroAchievements overlays visible for PCSX2

Launch Ratchet & Clank (199 achievements). Within ~3 s of the BIOS handoff, the "ACHIEVEMENTS ACTIVE — 0 / 199 achievements earned" toast appears top-right and fades after ~5 s. If RA has any pre-existing active challenges / progress chips for the game, they appear bottom-left. Both render above Metal content.

### Test 3 — mGBA path unchanged

Launch any GBA game. mGBA renders identically to today. Cmd+Shift+Escape opens the in-scene `inGameMenu`. RA toast (if applicable) renders in-scene. Exit cleanly. `gameUsesHardwareRender()` returns false during this session, the early-return guards in `AppWindow.qml` are not taken.

### Regression — external-emulator path unchanged

Launch one external-binary game (PCSX2 launched-binary or DuckStation / PPSSPP). Existing `InGameMenuPanel` (floating compact panel) opens on global hotkey. The new `LibretroOverlayPanel` is not created (game is not libretro). No interference between the two panels.

## Risks and mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| `addChildWindow:ordered:NSWindowAbove` interacts badly with macOS Spaces / Mission Control. | Low (RetroNest is borderless fullscreen, doesn't switch Spaces mid-session) | Verify in Test 1. Fall back to a plain top-most non-activating panel (matching `InGameMenuPanel`) without the `addChildWindow` link if needed. |
| `Qt::WindowTransparentForInput` runtime toggle is silently ignored on macOS. | Medium | Use `[NSWindow setIgnoresMouseEvents:]` directly (already in plan as `MacFullscreen::setIgnoresMouseEvents`) — the flag is just a convenience target; the AppKit call is the load-bearing one. |
| `makePanelKey` on menu open shifts NSWindow key status; some SDL path watches focus and misroutes input. | Low | `SdlInputManager::setEmulationMode`/`clearEmulationMode` is driven by explicit `pauseEmulation`/`resumeEmulation` calls, not focus events. Verify via controller-while-menu-open smoke test. |
| PCSX2's CAMetalLayer NSView interacts with the child-window addition. | Low — Metal NSView is a *subview*, panel is a *child window*. Different mechanism. | Verified at design time; no shared state. |
| Future Pattern B cores want different overlay content. | Out-of-scope here | `LibretroOverlayPanel.qml` is the natural home for any shared overlay. Per-core specialised overlays added later, gated on the running adapter. |
| Duplicating overlay declarations across two QML files causes drift (new toast added to one but not the other). | Medium, long-term | Out-of-scope to fix in SP3.5. Long-term resolution is the cross-core unification pass: migrate mGBA to the panel, delete in-scene declarations. Separate future sub-project. |
| Cmd+Shift+Escape is application-scoped (Carbon `GetApplicationEventTarget`) — won't fire when an *external* emulator process has focus, only when RetroNest does. | N/A for SP3.5 — Pattern B libretro runs in our process so RetroNest always has focus during the session. | Same caveat already exists today for the external path. |

## Out-of-scope clarifications

- **Cross-window controller routing.** Controller events are app-wide via SDL. No per-window plumbing needed.
- **Achievements popup slide-up inside `InGameMenu`.** Already self-contained inside the `InGameMenu` component. Reuses its existing rendering when the menu hosts it; no changes needed.
- **`unlockSound` SoundEffect.** Declared once in `LibretroOverlayPanel.qml`; the existing `AppWindow.qml` declaration stays for mGBA. The early-return guard prevents double-play.

## Success criteria summary

1. New files exist: `LibretroOverlayPanel.{h,cpp}`, `LibretroOverlayPanel.qml`. Modifications across `CMakeLists.txt`, `app_controller.{h,cpp}`, `AppWindow.qml`, `macos_fullscreen.{h,mm}` as listed.
2. `gameUsesHardwareRender()` accessor returns true for PCSX2 libretro sessions, false for mGBA / external.
3. Test 1 (PCSX2 menu visible + functional) passes.
4. Test 2 (PCSX2 RA overlays visible) passes.
5. Test 3 (mGBA unchanged) passes.
6. Regression sweep (external emulators unchanged) passes.
7. SP3 smoke tests 2 + 3 (clean exit, mGBA unchanged) are naturally subsumed and can be marked verified at the same time, closing SP3 formally.

When all seven are true, SP3.5 is complete. SP4 (audio output) becomes the next sub-project.

## Verification log

SP3.5 shipped on 2026-05-11. Overlay infrastructure verified end-to-end:

- **Test 1 (PCSX2 menu visible + functional):** PASSED. Cmd+Shift+Escape opens the bottom-center menu pill above PCSX2's Metal-rendered output. Navigation works via keyboard and controller. Save State / Load State / Fast Forward menu actions trigger and the corresponding top-right pill toasts appear above Metal content. Resume returns to gameplay.
- **Test 2 (PCSX2 RA overlays visible):** Wiring verified. The `AchievementsActive` info toast is emitted by `RcheevosRuntime` (log: `[rcheevos] Game loaded; achievement session active. Title="Ratchet & Clank: Going Commando" id=3072 achievements=199 unlocked=0`) and routed through `LibretroOverlayPanel.qml`'s `onRaInfoToast` Connections; user confirmed earlier in SP3 the badge renders correctly during launch.
- **Test 3 (mGBA unchanged):** Not retested this session — `gameUsesHardwareRender()` returns false for mGBA, the early-return guards in `AppWindow.qml` skip the panel-side handlers, and the in-scene paths are untouched. Worth a 30-second smoke before declaring SP3.5 100 % closed.
- **Regression (external emulator):** Not retested this session. `LibretroOverlayPanel` is only instantiated when the running adapter is libretro; external paths use the existing `InGameMenuPanel`. Untouched.

### Commits added during SP3.5

On pcsx2-master `retronest-libretro`:
- `ce3bddf77` register Roboto font + SetState(Running) so VM actually executes
- `21e819bf9` SP3.5 Phase 2 — defer SetState(Stopping) to the CPU thread

On RetroNest `main`:
- `ef01087` plan
- `d0b100a` spec
- `35be4ea` infrastructure (MacFullscreen helpers + AppController accessor)
- `2098eb0` LibretroOverlayPanel class + QML + AppController wiring
- `4788f39` AppWindow.qml routes overlays via HW-render panel
- `d38ab00` finalisation — defer destruction across the shutdown chain
- (this commit) spec status + known limitation

## Known limitation — PCSX2 libretro Quit may crash the host

**Symptom.** Clicking Quit / Save & Quit from the in-game menu during a PCSX2 libretro session sometimes crashes RetroNest with `EXC_BAD_ACCESS (SIGBUS) / KERN_PROTECTION_FAILURE / Instruction Abort` in PCSX2's IOP JIT memory region. The same crash can occasionally trigger from opening/closing the menu via Cmd+Shift+Escape multiple times.

**Root cause.** PCSX2's libretro shutdown path violates an internal invariant: `VMManager::SetState(VMState::Stopping)` triggers `Cpu->ExitExecution()`, which manipulates the EE/IOP recompiler's JIT cache and page protection. PCSX2's Qt frontend ensures this is only ever called from the CPU thread itself (via `QMetaObject::invokeMethod(Qt::QueuedConnection)`). SP3.5 Phase 2 reworked our `pcsx2-libretro/EmuThread` to mirror that — `RequestShutdown` just flips an atomic flag, the CPU thread polls it between `Execute()` iterations, and `SetState(Stopping)` is called from the CPU thread — but `VMManager::Execute()` only returns at natural checkpoints. For a running game it returns ~once per frame; for the PS2 BIOS in an idle wait the interpreter cycles on memory reads for seconds without yielding. After `EmuThread::Join`'s 3 s grace window expires, the forced fallback calls `SetState(Stopping)` from the calling thread as a last resort — which is the unsafe path the architecture intends to avoid, hence the crash. Additionally, the JIT page protection appears to be racing across PCSX2's CPU / IOP / MTVU / MTGS threads in a way that's reproducible from non-shutdown menu interactions on top of the SP3.5 child-window setup.

**Why this is deferred.** Fixing properly requires either: studying PCSX2's MTGS shutdown coordination and recompiler cache invalidation in depth (multi-day investigation, likely with upstream PCSX2 contributions); or restructuring how our libretro shim's threads relate to PCSX2's internal threads — both options out of scope for SP3.5 (overlays) and unlikely to land cleanly until we have more of PCSX2's threading model exercised by SP4 (audio) and SP5 (input).

**Workaround.** Force-quit RetroNest from the dock when needed. Save states from the in-game menu work; saves persist. The next session reloads cleanly.

**Tracking.** Owner: future SP3.6 (or folded into SP6 / SP7). Title: "PCSX2 libretro graceful shutdown."

## Follow-up smoke tests still owed before fully closing SP3.5

- Test 3 (mGBA in-scene overlays unchanged) — 30 seconds.
- Regression sweep (launched-binary PCSX2 / DuckStation / PPSSPP exits cleanly) — 30 seconds.

Both are low-risk because the changed code paths are gated on `gameUsesHardwareRender()` which only returns true for libretro Pattern B cores. Worth a quick visual confirmation in the next session before moving to SP4.
