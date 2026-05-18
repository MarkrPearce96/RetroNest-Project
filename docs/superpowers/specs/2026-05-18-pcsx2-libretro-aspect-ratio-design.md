# PCSX2 libretro — aspect ratio plumbing

**Date:** 2026-05-18
**Sub-project:** pcsx2-libretro aspect_ratio plumbing (follow-on from patches.zip ship)
**Status:** Design approved, ready for implementation plan
**Repo scope:** `pcsx2-libretro/pcsx2-libretro/` only — no edits in `pcsx2/`, no edits in RetroNest-Project

## Problem

`pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp:310` reports a hardcoded `4.0f / 3.0f` to the libretro frontend regardless of the `pcsx2_aspect_ratio` core option, `pcsx2_enable_widescreen_patches` toggle, or runtime video mode:

```cpp
info->geometry.aspect_ratio = 4.0f / 3.0f;
```

RetroNest sizes its display surface from the libretro AV info, so:

- Setting `pcsx2_aspect_ratio = 16:9` has zero visible effect.
- Widescreen patches render a 16:9-shaped image internally that then gets squashed back into the 4:3 frame on display — the patches "work" but their visible payoff is zero.

Surfaced 2026-05-18 during the patches.zip sub-project smoke test, after widescreen patches loaded successfully but display stayed pillarboxed.

## Goal

Report the correct aspect ratio to the libretro frontend, derived from PCSX2's own resolved state, and re-emit `SET_SYSTEM_AV_INFO` when that state changes mid-session.

## Non-goals (v1)

- **FMV aspect override (`pcsx2_fmv_aspect_ratio`).** PCSX2 has no hook that signals FMV start/end to the libretro shim; wiring one is its own sub-project. Skip; revisit if requested.
- **Crop / stretch-Y / custom display offsets.** Those still flow through PCSX2's internal renderer; they don't change what we report to libretro.
- **Edits inside `pcsx2/` (upstream).** Fix is purely in the libretro shim.
- **RetroNest-side changes.** No edits to RetroNest-Project. Includes the related "make `aspect_ratio <= 0` mean fill" change in `libretro_metal_item.mm` — feasible follow-up, not in scope here. The libretro `Stretch` option is a no-op in v1 (see Stretch note in the mapping section).

## Anchors in current code

| Concern | Location |
| --- | --- |
| Hardcode to fix | `pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp:310` |
| Re-emit pattern to mirror (SP7a) | `pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp:381-403` |
| Canonical AR float logic in PCSX2 | `pcsx2/GS/Renderers/Common/GSRenderer.cpp:291-312` (file-static `GetCurrentAspectRatioFloat`) |
| `AspectRatioType` enum | `pcsx2/Config.h:224-232` |
| `CurrentCustomAspectRatio` set by widescreen patches | `pcsx2/Patch.cpp:825` (`ApplyPatchSettingOverrides`) |
| Resolved AR field | `EmuConfig.GS.AspectRatio` (`pcsx2/Config.h:845`) + `EmuConfig.CurrentCustomAspectRatio` (`pcsx2/Config.h:1409`) |
| Progressive detection input | `gsVideoMode` already used by SP7a refinement loop |

## Inputs feeding the displayed aspect

Three independent inputs can mutate the correct float at runtime; the design must handle all three with one mechanism:

1. **User core option** — `pcsx2_aspect_ratio` → `EmuConfig.GS.AspectRatio` enum (`Stretch`, `RAuto4_3_3_2`, `R4_3`, `R16_9`, `R10_7`). Can flip mid-session.
2. **Widescreen patches** — when patches are enabled and AR is `RAuto4_3_3_2`, `Patch::ApplyPatchSettingOverrides` writes a per-game float into `EmuConfig.CurrentCustomAspectRatio` (e.g. 16:9 for a 4:3 game with a widescreen patch). This happens *inside PCSX2*, not through the libretro options layer.
3. **Video mode (Auto branch only)** — `GetVideoMode() == GS_VideoMode::SDTV_480P` makes the Auto branch resolve to 3:2 instead of 4:3. The mode is the same `gsVideoMode` that SP7a's region refinement already reads.

This is why a pure `RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE` listener is insufficient — input 2 and input 3 don't go through libretro options.

## Approach: per-frame recompute, emit-on-change

In `retro_run`, recompute the aspect float from the three inputs above each frame and re-emit `SET_SYSTEM_AV_INFO` only when the float actually changes. Cost is ~5 enum compares per frame; the actual `SET_SYSTEM_AV_INFO` call is rare (only on real change).

### Mapping (mirrors `GSRenderer::GetCurrentAspectRatioFloat`)

| `EmuConfig.GS.AspectRatio` | Custom override? | Result |
| --- | --- | --- |
| `Stretch` | n/a | `4.0f / 3.0f` (see note below) |
| `RAuto4_3_3_2` | `CurrentCustomAspectRatio > 0.f` | `CurrentCustomAspectRatio` (patch path) |
| `RAuto4_3_3_2` | none, `gsVideoMode == SDTV_480P` | `3.0f / 2.0f` |
| `RAuto4_3_3_2` | none, other modes | `4.0f / 3.0f` |
| `R4_3` | n/a | `4.0f / 3.0f` |
| `R16_9` | n/a | `16.0f / 9.0f` |
| `R10_7` | n/a | `10.0f / 7.0f` |

**Stretch note.** Standalone PCSX2's Stretch fills the host window because PCSX2 owns the surface. In libretro builds, the frontend owns the surface. RetroNest's display item (`cpp/src/ui/libretro/libretro_metal_item.mm:111-156`) consults `av.geometry.aspect_ratio` only when its own `m_aspectMode == "native"`; it has a separate `m_aspectMode == "stretch"` path that fills the bounds independent of the core's reported aspect (and treats `aspect_ratio <= 0` as a fallback to 4:3, not as "fill"). The pickup brief constrains this sub-project to `pcsx2-libretro/` only, so we cannot extend RetroNest's `<=0` interpretation here. Users who want stretch should set RetroNest's per-emulator aspect mode to "stretch"; the libretro core's `Stretch` option is therefore redundant and resolves to `4.0f / 3.0f` in v1. This matches the pre-fix behavior for that one cell of the matrix while every other cell becomes correct. A follow-up that extends RetroNest's `<=0` handling to mean "fill" is feasible but out of scope here.

## Components

### 1. New TU: `pcsx2-libretro/AspectRatio.{h,cpp}`

Small dedicated translation unit so the helper is unit-testable and `LibretroFrontend.cpp` doesn't grow another responsibility.

```cpp
// AspectRatio.h
#pragma once

namespace Pcsx2Libretro::AspectRatio
{
    // Resolves PCSX2's current effective display aspect to a libretro float.
    // Mirrors GSRenderer's GetCurrentAspectRatioFloat (which is file-static
    // upstream). Reads EmuConfig.GS.AspectRatio, EmuConfig.CurrentCustomAspectRatio,
    // and gsVideoMode (for the Auto branch's progressive detection).
    //
    // Returns 4.0f/3.0f for AspectRatioType::Stretch — see spec for rationale.
    // (Short version: RetroNest's display item treats aspect_ratio <= 0 as a
    // fallback to 4:3, not as "fill". Stretch is handled by RetroNest's own
    // per-emulator aspect mode in v1; the libretro Stretch option is a no-op.)
    float Compute();
}
```

`AspectRatio.cpp` implements the switch and references `EmuConfig`, `GSConfig`, and `gsVideoMode` directly (same headers SP7a already includes).

### 2. `LibretroFrontend.cpp` — `retro_get_system_av_info`

Replace `info->geometry.aspect_ratio = 4.0f / 3.0f;` with `info->geometry.aspect_ratio = Pcsx2Libretro::AspectRatio::Compute();`. Single-line swap.

### 3. `LibretroFrontend.cpp` — `retro_run` change detection

Add a file-static `g_last_emitted_aspect = -1.0f` near the existing `g_region_refined` / `g_detected_fps` state. Inside `retro_run` (immediately after the SP7a refinement block at `:381-403`):

```cpp
{
    const float current_aspect = Pcsx2Libretro::AspectRatio::Compute();
    if (std::fabs(current_aspect - g_last_emitted_aspect) > 0.001f)
    {
        retro_system_av_info av{};
        retro_get_system_av_info(&av);
        if (g_frontend.environ_cb)
            g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
        FrontendLog(RETRO_LOG_INFO,
            "[AspectRatio] re-emitted aspect=%.4f (was %.4f)",
            current_aspect, g_last_emitted_aspect);
        g_last_emitted_aspect = current_aspect;
    }
}
```

Reset `g_last_emitted_aspect = -1.0f` in `retro_init` / `retro_deinit` alongside the existing `g_frontend = FrontendState{};` cycle. First-frame post-load will always emit (matches SP7a's FPS behavior).

### 4. Build wiring

Add `AspectRatio.cpp` to the libretro core's CMake target. Existing pattern in `pcsx2-libretro/CMakeLists.txt` (or wherever the libretro shim sources are listed) — append the new TU alongside `CoreOptionsGraphics.cpp`, `CoreResources.cpp`, etc.

## Data flow

```
[user toggles pcsx2_aspect_ratio]  → libretro options → EmuConfig.GS.AspectRatio
[widescreen patch activates]       → Patch::ApplyPatchSettingOverrides → EmuConfig.CurrentCustomAspectRatio
[video mode changes]               → GS sets gsVideoMode
                                                              │
                                                              ▼
                              retro_run (each frame)
                                       │
                                       ▼
                AspectRatio::Compute() — read EmuConfig.GS.AspectRatio,
                                          EmuConfig.CurrentCustomAspectRatio,
                                          gsVideoMode → float
                                       │
                                       ▼
                  changed by > 0.001? ─── no ──► nothing
                                       │
                                      yes
                                       ▼
            retro_get_system_av_info → SET_SYSTEM_AV_INFO env call → frontend
```

## Error handling

- `g_frontend.environ_cb` null-check (already standard in this file).
- Reads on `EmuConfig.CurrentCustomAspectRatio` and `gsVideoMode` are intentional unsynchronised reads from the host thread, consistent with SP7a's documented data-race policy at `LibretroFrontend.cpp:374-380`: aligned int/float-sized loads are atomic at the instruction level on x86_64/arm64; a torn value would resolve to a sentinel that re-emits next frame at worst, never corrupts.
- `Compute()` has no fallible operations — pure switch.

## Testing

### Unit (`tools/test_aspect_ratio.cpp`)

New standalone test along the lines of `tools/test_region_prefix.cpp`. Constructs minimal `EmuConfig`-shaped fixtures (the helper only touches three fields, so a tiny mock or direct global mutation will do) and asserts `Compute()` for every cell of the mapping table:

- All 5 enum values.
- For `RAuto4_3_3_2`: with/without `CurrentCustomAspectRatio` set; with `gsVideoMode = SDTV_480P` vs interlaced.

~10 cases. Mirrors the existing `tools/test_region_prefix` pattern (single CPP, builds standalone, prints PASS/FAIL).

### Manual smoke (Rosetta x86_64, build via `cmake --build cpp/build-x86_64 --target RetroNest -j 4`)

1. **4:3 baseline** — NTSC R&C 2 (or DBZ TT2 PAL), AR=4:3 → ratio 1.333, one `[AspectRatio] re-emitted` log on first frame, none after.
2. **Mid-session option toggle** — Same game, change `pcsx2_aspect_ratio` to `16:9` via the RetroNest emulator settings dialog → one re-emit logged (`1.7778`), image visibly widens to fill more of the window.
3. **Widescreen patch** — Auto + `pcsx2_enable_widescreen_patches=true` on a title with a patch in `patches.zip` → ratio jumps to the patch's chosen aspect; in-game geometry now matches the displayed aspect.
4. **Stretch** — AR=Stretch → emitted aspect=1.333 (4:3 fallback per spec); confirm no per-frame chatter. Stretch-as-fill is delivered via RetroNest's per-emulator aspect mode, not this option, in v1.
5. **Progressive detection** — A 480P title in Auto mode → ratio resolves to 1.5 (3:2), not 1.333. (Note: most PS2 titles are interlaced; this case is rare in practice but verifies the branch.)
6. **No-regression replay** — Load a game with all defaults, play 60 seconds — confirm only the first-frame re-emit appears in the log, no per-frame chatter, no stutter.

## Constraints from pickup memory (locked in)

- No edits inside `pcsx2/` — fix is entirely in `pcsx2-libretro/`.
- No string parsing of `pcsx2_aspect_ratio` in the shim — switch on the `AspectRatioType` enum.
- No `SET_SYSTEM_AV_INFO` re-emit every frame — gate on actual float change (>0.001 epsilon).

## Time estimate

Half-day end-to-end:

- Spec + plan: ~1h (this doc + the plan that follows)
- Implementation: 2-3h (new TU, two edits in `LibretroFrontend.cpp`, CMake wiring, unit test)
- Smoke under Rosetta x86_64: 30min
- Memory close-out + commit + push: 15min

## Related memories

- `[[aspect-ratio-pickup]]` — surfacing of the bug; this spec consumes that pickup.
- `[[session-handoff-patches-shipped]]` — preceding sub-project that revealed the bug.
- `[[project-pcsx2-libretro-port]]` — SP7a's region/fps refinement is the structural template for this fix.
- `[[sp7c-kickoff]]` — Phase 4 Task 2 defines the Display sub-tab knobs whose user-visible payoff this work delivers.
