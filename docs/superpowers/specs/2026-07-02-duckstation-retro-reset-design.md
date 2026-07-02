# Implement `retro_reset` (DuckStation libretro)

**Date:** 2026-07-02
**Status:** Design approved, pending spec review
**Scope:** DuckStation libretro **core only** (`duckstation-libretro/src/duckstation-libretro/libretro.cpp`). No host changes.

## Background

`retro_reset` in the DuckStation libretro core is an empty stub (`libretro.cpp:281`: `RETRO_API void retro_reset(void) {}`). The host's Reset hotkey routes `HotkeyDispatcher` → `CoreRuntime::reset()` → `retro_reset`, so **the Reset hotkey silently does nothing** for DuckStation (confirmed by the 2026-06-29 code sweep).

DuckStation's reset entry point is `System::ResetSystem()` (`system.h:277`). It self-guards (`if (!IsValid()) return;`) and performs an undo-state save, an internal reset, and a BIOS reload as needed. In DuckStation's own code it is **always marshalled to the core thread** via `Host::RunOnCoreThread(System::ResetSystem)` (e.g. `core/hotkeys.cpp:171`, `core/fullscreenui.cpp:858`).

`CoreRuntime::reset()` invokes `retro_reset` **from the Qt thread** ("best-effort" cross-thread per its own doc comment). Calling `System::ResetSystem()` directly inside `retro_reset` would therefore race the worker thread mid-frame.

## Goal

Make the Reset hotkey reset the emulated PS1 console, with the reset running safely on the core (worker) thread.

## Design

Deferred-latch pattern, identical in shape to the shipped Player-2 hot-plug (`g_pad2_request`) drain:

1. Add a file-scope atomic in `libretro.cpp`: `static std::atomic<bool> g_reset_requested{false};`.
2. `retro_reset()` latches only: `g_reset_requested.store(true, std::memory_order_release);` — thread-safe, so the host's cross-thread call is fine.
3. At the **top of `retro_run`**, before `System::RunFrame()` (beside the existing pad2 drain): `if (g_reset_requested.exchange(false, std::memory_order_acquire)) System::ResetSystem();`.

This runs `ResetSystem` on the worker thread, between frames — never during `RunFrame`, never cross-thread — matching how DuckStation guarantees `ResetSystem` runs on the core thread. `ResetSystem`'s `IsValid()` guard makes a reset requested before boot a safe no-op. Self-contained in the core; the existing `CoreRuntime::reset()` → `retro_reset` path is unchanged.

## Rejected alternative

Calling `System::ResetSystem()` directly in `retro_reset`: simpler, but races the worker thread (the frontend calls `retro_reset` cross-thread). DuckStation's reset does too much (BIOS reload, state) to risk an unsynchronized call.

## Testing

No meaningful pure logic to unit-test (an atomic latch plus a guarded call). Verified by:
- **Build:** universal core via `package.sh` builds clean.
- **Manual (user):** boot a PS1 game, press the Reset hotkey → the game restarts (returns to BIOS/boot). Confirm no crash and that reset works repeatedly.

## Risk

Low. One new atomic and a two-line drain in `retro_run`, mirroring an already-shipped, reviewed pattern. Worst case is a one-frame latency between the hotkey press and the reset (imperceptible).

## Build / run

x86_64 under Rosetta; universal core via `package.sh` (no `--arm64-only`); after any host-app rebuild re-run `macdeployqt` (see `CLAUDE.md`). This change is core-only, so only the core dylib needs rebuilding/redeploying.
