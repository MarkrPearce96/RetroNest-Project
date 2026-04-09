# In-Game Menu & Quit System Design

**Date:** 2026-04-04
**Status:** Approved

## Overview

Add an in-game menu that allows users to pause, resume, and quit emulator games without ever seeing native emulator UIs. This requires refactoring the currently blocking synchronous launcher into an async model, implementing emulator control via IPC (PINE protocol for PCSX2) with hotkey injection as a universal fallback, and building a new QML in-game menu component.

## Goals

- Users can quit a running game and return to the game list
- Users see a confirmation before quitting (with save state option)
- The emulator pauses while the menu is open
- Native emulator pause menus are suppressed
- Architecture supports adding more emulators in the future

## Non-Goals (Future Work)

- DuckStation support (no IPC — needs hotkey-only approach, separate effort)
- Additional menu options (screenshot, toggle fullscreen, etc.)
- Multiple save state slot selection in the menu

---

## Section 1: Async Launcher Refactor

### Problem

`EmulatorLauncher::launch()` is a static method with a blocking `waitForReadyRead()` loop. The entire Qt event loop is frozen while a game runs — no UI, no input processing, no signals.

### Solution

New class **`GameSession`** owned by `GameService`:

- Holds the `QProcess*` for the running emulator
- `start(manifest, adapter, romPath)` — launches the process and returns immediately
- Signals: `started()`, `finished(int exitCode, bool crashed)`, `errorOccurred(QString)`
- Properties: `isRunning()`, `kill()`, `pid()`
- Only one game session at a time (enforced by `GameService`)

### Call Chain Changes

**Before (synchronous):**
```
ThemeContext::launchGame()
  → AppController::launchGame()
    → GameService::launchGame()
      → EmulatorLauncher::launch()  // blocks until exit
    → returns result
```

**After (async):**
```
ThemeContext::launchGame()
  → AppController::launchGame()
    → GameService::startGame()
      → GameSession::start()  // returns immediately
    → emits gameStarted()

GameSession::finished signal
  → GameService::onGameFinished()
    → emits gameFinished()
      → AppController/ThemeContext react
```

### State Exposure

`GameService` exposes a `gameRunning` property (Q_PROPERTY) so QML can react:
- Hide/show game list
- Switch Escape key behavior (settings overlay vs in-game menu)
- Enable/disable the in-game menu

---

## Section 2: Emulator Control Abstraction

### Adapter Extensions

New virtual methods on `EmulatorAdapter`:

```cpp
// IPC support
virtual bool supportsIPC() const { return false; }
virtual bool connectIPC(qint64 pid) { return false; }
virtual void disconnectIPC() {}

// Emulator commands (IPC or hotkey injection)
virtual bool sendPause() { return false; }
virtual bool sendResume() { return false; }
virtual bool sendSaveState(int slot) { return false; }
virtual bool sendLoadState(int slot) { return false; }

// Fallback: hotkey to inject for pause/resume
virtual QString pauseHotkeyString() const { return QString(); }
```

Default implementations return false/empty. Each adapter overrides what it supports.

### PCSX2Adapter Implementation

- `supportsIPC() → true`
- `connectIPC()` — connects to PINE Unix socket at `/tmp/pcsx2.sock` (or `$TMPDIR/pcsx2.sock`)
- `sendSaveState(slot)` — PINE opcode `0x09` with slot number
- `sendLoadState(slot)` — PINE opcode `0x0A` with slot number
- `sendPause()` / `sendResume()` — hotkey injection (PINE lacks native pause support)
- `pauseHotkeyString()` — returns the configured Toggle Pause key

### PineClient Utility

New class `core/pine_client.h/.cpp`:

- Handles Unix socket (macOS/Linux) and TCP socket (Windows) connection
- Message framing: 4-byte length prefix + opcode + parameters
- Response parsing: 4-byte length + status byte + data
- Methods: `connect(slot)`, `disconnect()`, `sendCommand(opcode, params) → response`
- Reusable for any emulator that adopts PINE in the future

### Hotkey Injection Fallback

For commands not supported by IPC (e.g., pause/resume on PCSX2), or for emulators without IPC entirely:

- `GameSession` can inject synthetic key events into the emulator process window
- The adapter provides the key string, `GameSession` handles the platform-specific injection
- This is the universal fallback path that all future emulators can use

---

## Section 3: In-Game Menu

### Component: `InGameMenu.qml`

Centered card/panel with semi-transparent background dimming the game. Consistent with existing app visual style (similar to settings overlay).

### Triggers

**Controller:** Select + Circle (PS) / Select + B (Xbox)
- `SdlInputManager` detects the combo and emits a new `quitComboPressed()` signal
- Combo detection: Select must be held, then Circle/B pressed while held

**Keyboard:** Escape key
- When `gameService.gameRunning` is true, Escape opens the in-game menu instead of the settings overlay

### Main Menu

| # | Option | Action |
|---|--------|--------|
| 1 | Resume Game | Close menu, unpause emulator |
| 2 | Quit Game | Transition to quit submenu |

### Quit Submenu

| # | Option | Action |
|---|--------|--------|
| 1 | Back to Pause Menu | Return to main menu |
| 2 | Exit & Save State | Save to slot 1, wait for confirmation, kill process |
| 3 | Exit Without Saving | Kill process immediately |

### Behavior

- Opening the menu sends pause to the emulator (via adapter)
- "Resume Game" sends unpause and dismisses the menu
- Full keyboard + controller navigation (Keys.onPressed handlers)
- Only visible/accessible when `gameService.gameRunning` is true
- D-pad / arrow keys navigate, A/Enter selects, B/Escape dismisses (resume)

### Exit & Save State Flow

1. User selects "Exit & Save State"
2. Show brief "Saving..." indicator
3. Call `adapter->sendSaveState(1)` (PINE opcode for PCSX2)
4. Wait for success response
5. Kill the emulator process
6. Return to game list

### Exit Without Saving Flow

1. User selects "Exit Without Saving"
2. Kill the emulator process immediately
3. Return to game list

---

## Section 4: Resume Save State Prompt

### Detection

After the emulator starts and PINE connects:
1. Query game serial via PINE `MsgID (0x0C)` and CRC via `MsgUUID (0x0D)`
2. Derive save state file path for slot 1 (follows PCSX2's naming convention)
3. Check if file exists on disk

### Prompt Component: `ResumeStateDialog.qml`

Simple dialog over the game:
- Text: "A save state was found. Resume from where you left off?"
- Two options: **Resume** / **Start Fresh**
- Controller and keyboard navigable

### Flow

1. Game launches → emulator process starts
2. PINE connection established
3. Pause emulator immediately
4. Check for slot 1 save state file
5. If found → show `ResumeStateDialog` → user chooses
   - Resume → `adapter->sendLoadState(1)` → unpause
   - Start Fresh → unpause
6. If not found → unpause immediately

---

## Section 5: PCSX2 Native Menu Suppression

Disable PCSX2's built-in ImGui pause menu by patching the INI:

- In `[Hotkeys]` section, set `OpenPauseMenu` to empty/unbound
- Applied in both `createDefaultConfig()` and `patchExistingConfig()`
- Same pattern as existing INI patching (using `patchIniKeys()` with `IniKeyPatch`)
- All other PCSX2 hotkeys managed through our hotkey settings page remain functional

---

## New Files

| File | Purpose |
|------|---------|
| `cpp/src/core/game_session.h/.cpp` | Async QProcess wrapper, game lifecycle |
| `cpp/src/core/pine_client.h/.cpp` | PINE IPC protocol client |
| `cpp/qml/AppUI/InGameMenu.qml` | In-game pause/quit menu |
| `cpp/qml/AppUI/ResumeStateDialog.qml` | Save state resume prompt |

## Modified Files

| File | Changes |
|------|---------|
| `cpp/src/core/emulator_launcher.h/.cpp` | Remove — replaced by GameSession |
| `cpp/src/adapters/emulator_adapter.h` | Add IPC/control virtual methods |
| `cpp/src/adapters/pcsx2_adapter.h/.cpp` | Implement PINE IPC, suppress native menu |
| `cpp/src/services/game_service.h/.cpp` | Own GameSession, expose gameRunning state |
| `cpp/src/ui/app_controller.h/.cpp` | Async launch signals, expose to QML |
| `cpp/src/ui/theme_context.h/.cpp` | Async launch, expose gameRunning |
| `cpp/qml/AppUI/AppWindow.qml` | Context-switch Escape, integrate InGameMenu |
| `cpp/src/core/sdl_input_manager.h/.cpp` | Select+Circle/B combo detection, quitComboPressed signal |
| `cpp/CMakeLists.txt` | Add new source files |

## Documentation Updates

After implementation:
- Update `CLAUDE.md` with in-game menu architecture, emulator control abstraction, PINE protocol reference, and new-adapter control method checklist
- Add memory entry for emulator control patterns
