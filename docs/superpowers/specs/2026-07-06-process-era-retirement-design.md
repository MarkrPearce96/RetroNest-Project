# Process-Era Retirement — Design

**Date:** 2026-07-06 · **Status:** approved (user, this date)
**Origin:** suite-review follow-up; deliberately deferred out of Packet 7
(spec 2026-07-04, decision 3: "registry+QML only — EmulatorAdapter/
process-era retirement deferred").

## Problem

Before the libretro migration RetroNest launched emulators as external
QProcess binaries: INI patching, setup-wizard suppression, keystroke-
synthesized pause, native-UI launching, window management from outside.
All five shipped emulators are now in-process libretro cores, so that
machinery is dead code that misleads readers and future sessions. The
standalone adapters and per-emulator dialogs were already deleted during
the migration; what remains is the dual-backend core and the base-class
surface.

## Decision (approved)

**Delete dead paths; keep the class split.** `Backend::Process` and
everything only it consumed is removed. The `EmulatorAdapter →
LibretroAdapter` inheritance split stays (a full merge is a much larger,
mostly-mechanical diff for cosmetic gain — can be a later pass if ever
wanted). A rejected alternative, stubbing the Process path to error out,
leaves the dead code in place and was not worth doing.

## Scope

1. **Loader**: `backend` must equal `"libretro"` or the manifest is
   rejected at load with a clear error. The silent `"process"` default
   dies. New loader test pins the rejection.
2. **GameSession**: delete the `Backend` enum, QProcess member, process
   spawn/monitor/terminate handlers and SIGTERM-save logic. The
   QML-facing API (properties/signals) stays byte-identical.
3. **EmulatorAdapter slim-down by consumer audit**: each of the 46
   virtuals/helpers is grep-audited. Consumed by LibretroAdapter, a
   service, or UI → stays. Consumed by nothing (INI patching helpers,
   `resolveExecutable`, launch-arg machinery, `pauseHotkeyVirtualKeyCode`,
   native-settings-UI hooks, `formatBinding`, RA INI patching, …) →
   deleted. The compiler is the safety net: a misjudged deletion fails
   the build loudly, never silently.
4. **Pause plumbing**: AppController's `CGEventPostToPid` synthesis and
   SIGSTOP/SIGCONT fallback deleted — pause is `CoreRuntime::pause()`
   only. `openNativeEmulatorSettings` deleted as a feature (no libretro
   core has a native UI). `macos_fullscreen.mm` gets the same per-symbol
   audit because RetroNest's own window management also lives there.
5. **Installer**: core-zip install path stays; `.dmg`/`.AppImage`/
   process-app asset matching goes only where nothing reaches it.
6. **Docs**: trim the two CLAUDE.md paragraphs that still describe
   process-era mechanisms as present-tense legacy (macOS launch rule,
   portable-mode note).

## Invariants

- No behavior change for any shipped flow: session launch, in-game menu
  pause/resume, Save & Quit → Resume, settings, installs/updates.
- x86_64 daily driver never left broken; full suite green before the
  smoke handoff; nothing pushed until the user confirms in-app.

## Verification

- Full QtTest suite + new loader-rejection test.
- **USER SMOKE GATE (hard stop):** boot one game per core (all five),
  menu pause/resume, Save & Quit → Resume, one settings change, one
  in-app core reinstall — the blast radius of GameSession surgery.

## Risks

| Risk | Mitigation |
|---|---|
| Deleting a symbol something still consumes | Compile-time failure; per-symbol grep audit before each deletion |
| GameSession surgery breaks a live flow | QML API frozen; five-core smoke gate |
| Hypothetical future standalone emulator | Git history preserves the machinery; re-adding is a deliberate feature, not a fallback |
