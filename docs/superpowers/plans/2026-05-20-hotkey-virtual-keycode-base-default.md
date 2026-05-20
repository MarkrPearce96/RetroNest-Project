# `hotkeyVirtualKeyCode()` Base Default Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the standard `Space / F5 / F7 / F8` `kVK_*` mapping into `EmulatorAdapter` base, so external-process adapters only override to disable or remap.

**Architecture:** Four files, four commits. Land the base change first (additive — overrides still win, no behavior change). Then strip each override one at a time, each commit independently testable.

**Tech Stack:** C++17, Qt 6, CMake. No new dependencies. No new tests — existing C++ tests don't cover `hotkeyVirtualKeyCode()`, and verification per the spec is build + smoke test of the in-game menu (DuckStation is the highest-risk smoke target since its override is removed entirely).

**Spec:** `docs/superpowers/specs/2026-05-20-hotkey-virtual-keycode-base-default-design.md`

---

## File Structure

**Modify (4):**
- `cpp/src/adapters/emulator_adapter.h` — base class default returns `Space / F5 / F7 / F8`
- `cpp/src/adapters/duckstation_adapter.h` — override deleted entirely (matched default)
- `cpp/src/adapters/ppsspp_adapter.h` — override shrinks to single opt-out for `ToggleFastForward`
- `cpp/src/adapters/dolphin_adapter.h` — override keeps disabling save/load/FF; `TogglePause` defers to base

**No new files. No CMakeLists.txt change. No new tests.**

---

## Task 1: Move default into `EmulatorAdapter` base

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h:386-396` — the `hotkeyVirtualKeyCode` doc-comment and method body

After this commit: nothing changes behaviorally. DuckStation/PPSSPP/Dolphin all still explicitly override and their overrides take precedence. Build must still succeed.

- [ ] **Step 1: Replace the existing default**

In `cpp/src/adapters/emulator_adapter.h`, find this block (around lines 386–396):

```cpp
    /**
     * Carbon kVK_* virtual keycode for the given action, or 0 if the
     * adapter doesn't expose a synth target for it. The corresponding
     * emulator hotkey must be bound to this key in createDefaultConfig
     * / patchExistingConfig — otherwise the synthesized keystroke
     * reaches the emulator but does nothing.
     */
    virtual int hotkeyVirtualKeyCode(HotkeyAction action) const {
        Q_UNUSED(action);
        return 0;
    }
```

Replace it with:

```cpp
    /**
     * Carbon kVK_* virtual keycode for the given action. Default
     * returns the standard external-emulator mapping (Space / F5 /
     * F7 / F8). The corresponding emulator hotkey must be bound to
     * this key in createDefaultConfig / patchExistingConfig —
     * otherwise the synthesized keystroke reaches the emulator but
     * does nothing.
     *
     * Adapters override only to disable an action (return 0) or
     * remap one. New external-process adapters get the standard
     * synthesis behavior for free. Libretro adapters inherit these
     * values but never reach synthesizeHotkey() — every call site
     * is gated by an isLibretro() check in AppController.
     */
    virtual int hotkeyVirtualKeyCode(HotkeyAction action) const {
        switch (action) {
        case HotkeyAction::TogglePause:       return 0x31; // kVK_Space
        case HotkeyAction::SaveState:         return 0x60; // kVK_F5
        case HotkeyAction::LoadState:         return 0x62; // kVK_F7
        case HotkeyAction::ToggleFastForward: return 0x64; // kVK_F8
        }
        return 0;
    }
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. The override-vs-virtual dispatch means DuckStation/PPSSPP/Dolphin still emit identical assembly at every call site — pure header change that touches every dependent translation unit but flips no behavior.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "$(cat <<'EOF'
refactor(adapters): set hotkeyVirtualKeyCode default to Space/F5/F7/F8

Move the standard kVK_* mapping into EmulatorAdapter base. All current
adapters (DuckStation, PPSSPP, Dolphin) still override explicitly with
identical or stricter values, so this commit changes no behavior. The
override-shrinking commits follow.

Libretro adapters inherit these values but never reach synthesizeHotkey()
(every call site in app_controller.cpp is gated by isLibretro()), so the
new default is safe for them.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Delete DuckStation's override

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.h:37-48` — delete the override + its preceding comment block

DuckStation's old override returned `Space / F5 / F7 / F8` — exactly what the base now returns. Deleting the override removes 12 lines and makes DuckStation pick up the base's implementation directly. Same runtime values; one fewer call site to update if `kVK_*` ever changes.

- [ ] **Step 1: Delete the override**

In `cpp/src/adapters/duckstation_adapter.h`, delete lines 37–48 (the comment block plus the entire `int hotkeyVirtualKeyCode(...) const override { ... }` body). For reference, the block currently looks like:

```cpp
    // SaveSelectedSaveState, LoadSelectedSaveState and ToggleFastForward
    // are force-bound to F5/F7/F8 and removed from hotkeyBindingDefs() so
    // the user can't rebind them and break in-game menu synthesis.
    int hotkeyVirtualKeyCode(HotkeyAction action) const override {
        switch (action) {
        case HotkeyAction::TogglePause:       return 0x31; // kVK_Space
        case HotkeyAction::SaveState:         return 0x60; // kVK_F5
        case HotkeyAction::LoadState:         return 0x62; // kVK_F7
        case HotkeyAction::ToggleFastForward: return 0x64; // kVK_F8
        }
        return 0;
    }
```

After the deletion, the surrounding context (lines around 36 and 49 currently) reads:

```cpp
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
```

(i.e. `hotkeyBindingDefs()` then `controllerTypes()` are now adjacent.)

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. DuckStation now inherits the base default, which returns identical values to what its override previously returned.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.h
git commit -m "$(cat <<'EOF'
refactor(duckstation): drop hotkeyVirtualKeyCode override

DuckStation returned exactly Space/F5/F7/F8 — identical to the new
EmulatorAdapter base default. Removing the override picks up the base
implementation directly. No behavior change.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Shrink PPSSPP's override

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.h:37-49` — shrink override to single opt-out

PPSSPP's only divergence from the standard is `ToggleFastForward = 0` (hold-style, no clean toggle hotkey). Shrink the override to just that.

- [ ] **Step 1: Replace the override**

In `cpp/src/adapters/ppsspp_adapter.h`, find this block (around lines 37–49):

```cpp
    // Save/Load State are bound to keyboard F5/F7 (1-135 / 1-137) in
    // controls.ini and removed from hotkeyBindingDefs(). Fast-forward
    // is hold-style on PPSSPP with no clean toggle hotkey, so we
    // return 0 — the in-game menu hides the FF action.
    int hotkeyVirtualKeyCode(HotkeyAction action) const override {
        switch (action) {
        case HotkeyAction::TogglePause:       return 0x31; // kVK_Space
        case HotkeyAction::SaveState:         return 0x60; // kVK_F5
        case HotkeyAction::LoadState:         return 0x62; // kVK_F7
        case HotkeyAction::ToggleFastForward: return 0;    // hold-style only; hidden in HUD
        }
        return 0;
    }
```

Replace it with:

```cpp
    // PPSSPP fast-forward is hold-style with no clean toggle hotkey,
    // so we return 0 for ToggleFastForward — the in-game menu hides
    // the FF action. Save/Load/Pause use the standard base defaults
    // (Space / F5 / F7), bound in controls.ini (1-135 / 1-137).
    int hotkeyVirtualKeyCode(HotkeyAction action) const override {
        if (action == HotkeyAction::ToggleFastForward) return 0;
        return EmulatorAdapter::hotkeyVirtualKeyCode(action);
    }
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. PPSSPP's `TogglePause/SaveState/LoadState` now resolve via the base; `ToggleFastForward` continues to return `0`.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/ppsspp_adapter.h
git commit -m "$(cat <<'EOF'
refactor(ppsspp): shrink hotkeyVirtualKeyCode override to FF opt-out

PPSSPP matched the standard Space/F5/F7 mapping for Pause/Save/Load —
those cases now defer to EmulatorAdapter::hotkeyVirtualKeyCode(). The
only divergence is ToggleFastForward which stays 0 (PPSSPP's FF is
hold-style with no clean toggle hotkey).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Defer Dolphin's `TogglePause` to base

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.h:78-93` — switch the `default:` case to fall through to base

Dolphin still disables Save/Load/FastForward. Only TogglePause matches the standard `Space`, so route just that through the base.

- [ ] **Step 1: Replace the override**

In `cpp/src/adapters/dolphin_adapter.h`, find this block (around lines 78–93):

```cpp
    // SaveSelectedSaveState, LoadSelectedSaveState and ToggleFastForward
    // are not exposed by Dolphin (it has its own state-slot UI), so we
    // disable them in our in-game menu.
    // Returning 0 hides the icons from the in-game menu via
    // currentGameInfo's supportsSaveState/supportsLoadState flags.
    // Pause still works because PauseOnFocusLost handles it on focus
    // change — the synthesized Space is a no-op for Dolphin but
    // harmless.
    int hotkeyVirtualKeyCode(HotkeyAction action) const override {
        switch (action) {
        case HotkeyAction::TogglePause:       return 0x31; // kVK_Space
        case HotkeyAction::SaveState:         return 0;
        case HotkeyAction::LoadState:         return 0;
        case HotkeyAction::ToggleFastForward: return 0;
        }
        return 0;
    }
```

Replace it with:

```cpp
    // Dolphin save/load state are disabled (returning 0 hides the
    // icons from the in-game menu via currentGameInfo's
    // supportsSaveState/supportsLoadState flags). Fast-forward isn't
    // wired. Pause uses the base default (Space) — works because
    // PauseOnFocusLost handles focus changes too; the synthesized
    // Space is a no-op for Dolphin but harmless.
    int hotkeyVirtualKeyCode(HotkeyAction action) const override {
        switch (action) {
        case HotkeyAction::SaveState:         return 0;
        case HotkeyAction::LoadState:         return 0;
        case HotkeyAction::ToggleFastForward: return 0;
        default:                              return EmulatorAdapter::hotkeyVirtualKeyCode(action);
        }
    }
```

- [ ] **Step 2: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. Dolphin's Save/Load/FF stay disabled; TogglePause now resolves to `0x31` via the base.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.h
git commit -m "$(cat <<'EOF'
refactor(dolphin): defer hotkeyVirtualKeyCode TogglePause to base

Save/Load/ToggleFastForward stay disabled (return 0). TogglePause
now falls through to EmulatorAdapter::hotkeyVirtualKeyCode() via
the switch's default: arm — same Space value, less duplication, and
any future HotkeyAction added to the enum will use the base default
for Dolphin until/unless Dolphin opts out.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Deploy, sign, smoke test, memory update

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md` — mark #9 shipped
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md` — update Tier 1/2 progress note

- [ ] **Step 1: Kill any running RetroNest**

```bash
pkill -f "build-x86_64/RetroNest.app" 2>/dev/null
```

- [ ] **Step 2: Deploy + resign**

Per the `build-cmake-needs-macdeployqt` memory, after every `cmake --build` the binary must be re-deployed and re-signed before launching, or the bundle dual-loads Qt and aborts.

```bash
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
```

Verify the binary's Qt refs are `@executable_path/...`:

```bash
otool -L cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest | grep -c "@executable_path/.*Qt"
```

Expected: ≥ 8.

- [ ] **Step 3: Launch and confirm running**

```bash
open cpp/build-x86_64/RetroNest.app
sleep 5
pgrep -fl "build-x86_64/RetroNest.app/Contents/MacOS/RetroNest"
```

Expected: a process line. If empty (crashed), check `~/Library/Logs/DiagnosticReports/RetroNest-*.ips` for the most recent crash and investigate.

- [ ] **Step 4: Hand off smoke test to the user**

The controller running this plan should ask the user to verify the following — DuckStation is the highest-risk case because its override was removed entirely:

**DuckStation** (must verify):
1. Launch a PS1 game via DuckStation.
2. Cmd+Shift+Escape opens the in-game menu and pauses the game.
3. Resume from the menu → game resumes (Space synthesized via base default).
4. Save State button → DuckStation saves (F5).
5. Load State button → DuckStation loads (F7).
6. Fast Forward toggle → game speeds up (F8).

**PPSSPP** (spot-check):
1. Launch a PSP game.
2. Pause/Save/Load via the in-game menu still work.
3. Fast Forward action is hidden from the in-game menu (because PPSSPP returns 0 for FF).

**Dolphin** (spot-check):
1. Launch a GameCube or Wii game.
2. Pause via the in-game menu works.
3. Save State / Load State / Fast Forward icons are hidden in the in-game menu.

If any regress, STOP and report which adapter + which action + observed behavior.

- [ ] **Step 5: Update refactor-roadmap memory**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`.

Find the line:
```
9. **`hotkeyVirtualKeyCode()` → base class default** — identical switch in `duckstation_adapter.h:38–46` and `ppsspp_adapter.h:39–47`; dolphin overrides to disable F5/F7/F8. Move default to `EmulatorAdapter` base; adapters override only to disable specific keys. ~15 LOC; removes step from new-adapter checklist.
```

Replace with:
```
9. ✅ **`hotkeyVirtualKeyCode()` → base class default** — shipped 2026-05-20. Base now returns Space/F5/F7/F8. DuckStation override deleted (matched default exactly); PPSSPP shrunk to one-line FF opt-out + defer-to-base; Dolphin defers TogglePause via `default:` arm of its switch. Adding a new external-process adapter no longer requires writing the boilerplate. Spec: `docs/superpowers/specs/2026-05-20-hotkey-virtual-keycode-base-default-design.md`. Plan: `docs/superpowers/plans/2026-05-20-hotkey-virtual-keycode-base-default.md`. Net ≈ −13 LOC.
```

Also update the front-matter description:
```
description: Ongoing generalization/cleanup roadmap for RetroNest. Tier 1 items 1-5 and Tier 2 #9 shipped 2026-05-20; remaining Tier 2 items pending. Resume here when starting a new session on this work.
```

- [ ] **Step 6: Update MEMORY.md index**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md`. Find:
```
- [Refactor roadmap](refactor-roadmap.md) — Multi-session generalization program. Tier 1 #1-5 shipped 2026-05-20; Tier 2 items (#6 HotkeyService, #7 BaseNotification, #8 AppController, #9 hotkeyVirtualKeyCode, #10 NavigableGrid mixin) pending. Open here when resuming refactor work.
```

Replace with:
```
- [Refactor roadmap](refactor-roadmap.md) — Multi-session generalization program. Tier 1 #1-5 + Tier 2 #9 shipped 2026-05-20; remaining Tier 2 items (#6 HotkeyService, #7 BaseNotification, #8 AppController, #10 NavigableGrid mixin) pending. Open here when resuming refactor work.
```

Memory files live outside the repo — no git commit needed.

---

## Self-review

**Spec coverage:**
- Base class default (`emulator_adapter.h`) → Task 1 ✓
- DuckStation override removal → Task 2 ✓
- PPSSPP override shrink → Task 3 ✓
- Dolphin override → defer TogglePause to base → Task 4 ✓
- Build verification at each step ✓
- Smoke test (DuckStation primary, PPSSPP + Dolphin spot-check) → Task 5 Step 4 ✓
- `build-cmake-needs-macdeployqt` reminder honored → Task 5 Step 2 ✓
- No new tests required — spec acknowledges existing test infrastructure doesn't cover this method, end-to-end smoke test is sufficient.

**Placeholder scan:** No TBDs. Every step shows the exact before/after code.

**Type / name consistency:**
- `HotkeyAction::TogglePause`, `SaveState`, `LoadState`, `ToggleFastForward` used identically across all four tasks.
- `EmulatorAdapter::hotkeyVirtualKeyCode(action)` invocation pattern matches in Tasks 3 and 4.
- `0x31 / 0x60 / 0x62 / 0x64` are the same constants in the base (Task 1) as in the old DuckStation override (verified by reading the source).
- Dolphin's `default:` arm covers `TogglePause` plus any future enum value — explicitly different from PPSSPP's `if` shape because Dolphin has multiple disables; both are equivalent in spirit.

**Commit ordering:** Task 1 (base) is independently safe because all adapters still explicitly override. Tasks 2/3/4 each become correct only after Task 1 lands; running them out of order would change DuckStation/PPSSPP/Dolphin's behavior. Subagent-driven execution serializes them, so this is fine.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-20-hotkey-virtual-keycode-base-default.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task, review between. Five small tasks (~10 lines each); the reviews catch any header-edit fumbles immediately.

**2. Inline Execution** — same session, batch with checkpoints.

Which approach?
