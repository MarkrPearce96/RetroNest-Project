# Auto HW→Software renderer fallback (DuckStation libretro)

**Date:** 2026-06-29
**Status:** Design approved, pending spec review
**Scope:** DuckStation libretro **core only** (`duckstation-libretro/src/duckstation-libretro/`). No host changes, no persisted state.

## Background

The DuckStation libretro core picks its GPU renderer at settings-apply time (`libretro_settings.cpp` writes `"GPU"/"Renderer"` — the user option, or hardcoded `"Metal"` in the enhancement profile). If the hardware (Metal) renderer fails to initialize at boot, `System::BootSystem` returns `false` and `retro_load_game` returns `false` (`libretro.cpp:391–395`) — the game simply fails to launch, with **no fallback** to the software renderer.

DuckStation already has the mechanism for a software boot: `SystemBootParameters::force_software_renderer` (`system.h:60`). `System::Initialize` honors it (`system.cpp:1818`: `CreateGPUBackend(force_software_renderer ? GPURenderer::Software : g_settings.gpu_renderer, …)`). On any `Initialize` failure, `BootSystem` fully tears down (`system.cpp:1672–1676`: `Host::OnSystemStopping(); DestroySystem(); return false;`), so a second boot attempt is safe.

## Goals

1. If the configured hardware renderer fails to init at boot, automatically retry once with the software renderer instead of failing to launch.
2. The fallback is **never silent** — it announces itself via an on-screen OSD message and a log line.
3. One-shot: the fallback applies to the current launch only; the next launch retries the configured hardware renderer (transient Metal failures self-heal).

## Non-goals

- Mid-game renderer loss/recovery (boot-time only).
- Persisting the software choice across launches.
- Any host-side UI/plumbing (native OSD only).
- Falling back when the user explicitly selected the Software renderer (nothing to fall back from).

## Design

### Boot-with-fallback (in `retro_load_game`, `libretro.cpp`)

Replace the single `BootSystem` call with:

1. Compute `configuredIsSoftware = (g_settings.gpu_renderer == GPURenderer::Software)`.
2. Attempt the normal boot (configured renderer). Capture success/failure.
   - **Test hook:** if the env var `DUCKSTATION_FORCE_GPU_FAIL` is set (non-empty), skip this first attempt entirely and treat it as failed — this lets the fallback path be exercised on hardware where Metal never actually fails. Off unless the env var is set; never affects normal use.
3. If the first attempt failed **and** `!configuredIsSoftware` (i.e. `ShouldFallBackToSoftware(...)` is true):
   - Log `RETRO_LOG_WARN`: hardware renderer failed, falling back to software (include the captured `Error` description from the first attempt when available).
   - Build a **fresh** `SystemBootParameters` (the first one was moved) with `force_software_renderer = true`, and retry `BootSystem` once.
   - On retry **success**: post a native OSD message (see Observability) and proceed.
   - On retry **failure**: log error and `return false` (the failure was not GPU-related; software can't fix it).
4. If the first attempt failed and `configuredIsSoftware`: log error and `return false` exactly as today.

### Pure decision helper (testable seam)

New dependency-free header `libretro_renderer_fallback.h` (mirrors the `libretro_pad2.h` / `libretro_analog.h` standalone pattern), with:

```cpp
// True when a failed boot should be retried with the software renderer:
// only when the boot failed AND the configured renderer was not already Software.
inline bool ShouldFallBackToSoftware(bool configuredIsSoftware, bool bootSucceeded) {
  return !bootSucceeded && !configuredIsSoftware;
}
```

The helper takes a `bool` (not the `GPURenderer` enum) so it stays free of core headers and compiles standalone in its test. `libretro.cpp` computes the bool from `g_settings.gpu_renderer`.

### Observability

- **OSD toast** on a successful fallback: *"Hardware renderer unavailable — using Software renderer (reduced quality)."* via the engine's `Host::AddIconOSDMessage` / `Host::AddOSDMessage` path (already rendered by the libretro OSD overlay), ~8s duration. (Exact signature confirmed against `core/host.h` during planning.)
- **Log line** in `/tmp/rn.log` (`RETRO_LOG_WARN`) naming the failure + that it fell back, plus the first attempt's `Error` description.
- Inherent visual tell: the software renderer is native 1× (internal upscaling/PGXP enhancements do not apply).

## Testing

- **Unit (standalone, clang++ + assert, like `libretro_pad2_test.cpp`):** `ShouldFallBackToSoftware` truth table — `(softwareConfigured=false, booted=false) → true`; `(false, true) → false`; `(true, false) → false`; `(true, true) → false`.
- **Build:** universal core via `package.sh` builds clean; the core compiles + links.
- **Manual (user):** launch a game with `DUCKSTATION_FORCE_GPU_FAIL=1` set → confirm (a) the game still boots, (b) the OSD toast appears, (c) the picture is native-res (no upscaling), (d) `/tmp/rn.log` shows the WARN fallback line. Then launch normally (no env var) → confirm hardware path is unaffected (upscaling works, no toast).

## Risk

Low. The only change on the normal path: a previously-fatal hardware-renderer boot failure now triggers one software retry. `BootSystem` fully tears down between attempts (verified). Worst case — a non-GPU boot failure with a hardware renderer configured — is one extra fast-failing boot before the same `false` return as today. The test hook is inert unless the env var is set.

## Build / run

x86_64 under Rosetta; universal core via `package.sh` (no `--arm64-only`). See `RetroNest-Project/CLAUDE.md`.
