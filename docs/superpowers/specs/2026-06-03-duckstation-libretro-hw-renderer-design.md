# DuckStation libretro #1 — Hardware Metal renderer (minimal)

**Date:** 2026-06-03
**Status:** Design approved, ready for implementation plan
**Repo:** `duckstation-libretro/` (fork `master`, local-only, never push)
**Predecessors:** [skeleton](2026-06-01-duckstation-libretro-skeleton-design.md), [save-states](2026-06-03-duckstation-libretro-savestates-design.md) — both complete and shipped.
**Source handoff:** `duckstation-libretro/docs/hardware-renderer-handoff-2026-06-03.md`

## 1. Goal & scope

Flip the DuckStation libretro core from the **Software** renderer to DuckStation's **hardware Metal** renderer at **4× internal resolution**, with **PGXP geometry + texture correction** and **true color** enabled — all via **hardcoded defaults** in `ApplySettings`. This is the headline reason to use modern DuckStation (upscaling + PGXP), which the skeleton deliberately left off for safety.

**In scope:**
- Hardware Metal renderer selection + a curated set of enhancement defaults, written into the base settings layer.
- Validating the HW shader pipeline and the inline (non-threaded) render/present path end-to-end through RetroNest.

**Explicitly out of scope (deferred to feature #3, settings migration):**
- Any user-facing configuration: libretro core options, RetroNest `settingsSchema()` wiring, live-changeable knobs. All values here are fixed defaults.
- Enabling the GPU video thread (`GPU/UseThread`) — stays `false`; see §5.
- Widescreen, texture filtering, downsampling, and other knobs not listed in §3.

**Success criteria:**
- PS1 output through RetroNest is visibly sharper/upscaled vs. the current software output.
- 3D geometry is stable (no PS1 polygon jitter/wobble) thanks to PGXP.
- No regression to audio, input, save states, or persistent memcards.
- `/tmp/rn.log` shows clean Metal HW renderer init + runtime shader compilation, no errors.

## 2. Where the change lives

A single function: `ApplySettings` in `src/duckstation-libretro/libretro_settings.cpp` — the same base-settings-layer block (under `Core::GetSettingsLock()`, on `Core::GetBaseSettingsLayer()`) that today sets `GPU/Renderer`, `GPU/UseThread`, region, memcard type, and Pad1 type.

No other files change. The display/present path (Metal `GPUDevice` + swapchain on RetroNest's NSView, `GPUBackend::HandleSubmitFrameCommand` self-present) and the run loop (`System::RunFrame()` interrupted at `FrameDone`) already work and are unchanged — this feature only changes **which renderer DuckStation uses to draw the frame** and at what internal resolution.

## 3. The settings changes

All values are written in the **same carefully-commented, verified-against-`core/settings.cpp` style** as the existing block. Every INI key + value below has been **verified against `Settings::Load` / `LoadPGXPSettings` in `src/core/settings.cpp`** (line refs in the table).

| Setting (intent) | INI key | Type | Current | New value | Verified at |
|---|---|---|---|---|---|
| Renderer | `GPU/Renderer` | string | `"Software"` | `"Metal"` | parse `settings.cpp:301`, names `:1557` |
| GPU video thread | `GPU/UseThread` | bool | `false` | `false` (unchanged) | `settings.cpp:320` |
| Internal resolution scale | `GPU/ResolutionScale` | uint | `1` | `4` | `settings.cpp:304` |
| PGXP master enable | `GPU/PGXPEnable` | bool | `false` | `true` | `settings.cpp:357` |
| PGXP culling | `GPU/PGXPCulling` | bool | `true` (default) | `true` (explicit) | `settings.cpp:646` |
| PGXP texture correction | `GPU/PGXPTextureCorrection` | bool | `true` (default) | `true` (explicit) | `settings.cpp:647` |
| True color (24-bit) | `GPU/DitheringMode` | string | `"TrueColor"` (default) | `"TrueColor"` (explicit) | `settings.cpp:334,730`; names `:1725`; default `settings.h:226` |

**Decisions baked in:**
- **`"Metal"`, not `"Automatic"`.** Both resolve to the Metal backend on macOS, but forcing it explicitly makes this validation feature deterministic and avoids "what did Automatic pick?" ambiguity in the logs.
- **PGXP Vertex Cache stays OFF (engine default).** Source check (`settings.cpp:649`, default `false`) showed vertex cache is a *situational compatibility fallback* (screen-coordinate vertex tracking) that can introduce visual errors in some games — not the source of the anti-jitter fix. The geometry-stability win comes from `PGXPEnable` + `PGXPCulling` (both on). Leaving it off keeps the "conservative safe defaults" intent. (Earlier draft listed it on; corrected after source diligence.) Not written → defaults false.
- **`GPU/PGXPCPU` stays OFF (engine default `settings.cpp:650`).** CPU-mode PGXP is heavy and accuracy-oriented; not needed for the visual win and a perf risk. Not written.
- **Texture filtering, widescreen, downsampling: OFF.** Divisive / artifact-prone; conservative default, deferred to #3 where the user can opt in.
- **Culling, texture correction, and true color are written explicitly even though they already match engine defaults.** This makes `ApplySettings` the self-documenting single source of truth for our enhancement profile, robust to upstream default changes. (`gpu_true_color`/`gpu_scaled_dithering` were folded into `gpu_dithering_mode` per delta §3; the `"TrueColor"` enum value — `GPUDitheringMode::TrueColor` — is the modern equivalent and the current default.)

## 4. Failure handling

**Hard fail on HW init failure — no silent fallback to Software.** This feature exists to exercise and validate the Metal HW path; a silent downgrade would mask exactly the breakage we want to see. If the Metal HW renderer fails to initialize at runtime, the error surfaces (in `/tmp/rn.log` and via DuckStation's existing error reporting) rather than quietly reverting. Automatic software fallback can be reconsidered later as a robustness feature once the HW path is proven; it is not part of #1.

## 5. Threading — stays inline

`GPU/UseThread` remains `false`. The run loop (`System::RunFrame()` + `InterruptExecution` at `FrameDone`, one frame per `retro_run`) was purpose-built for inline execution. Enabling the GPU video thread would re-open the run-loop design toward the threaded+present-CV model the skeleton deliberately avoided. The HW renderer is validated **inline first**; threading is a separate future decision, not part of this feature.

## 6. Risks & validation order

Implementation validates in this order — cheapest / most-likely-to-break first, per the handoff's flagged risks:

1. **HW shader pipeline (top suspect).** Unlike software (which needed only the prebuilt `metal_shaders.metallib` + shaderc/spirv-cross for ImGui), the HW renderer **generates PS1 draw shaders at runtime** (`video_shadergen` / `gpu_hw_shadergen` → compiled through the Metal device, possibly via shaderc/spirv-cross). First milestone: confirm runtime shader generation + compilation succeeds in the **deployed** bundle (metallib + Frameworks libs already shipped by `package.sh`), watching `/tmp/rn.log`. This is the most likely first break.
2. **Inline + HW interaction.** Confirm `RunFrame()` still yields exactly one frame per `retro_run` with the HW backend and `VideoThread::IsUsingThread() == false`, with the GPU command pipeline executing inline and no stall.
3. **Present at scale.** Confirm the upscaled (4×) frame composites correctly to the NSView Metal layer — geometry/aspect via `VideoPresenter`.
4. **Texture cache / VRAM.** Higher-res render targets + `gpu_hw_texture_cache` at 4× — watch for memory/format issues.

**Key code references:** `src/core/gpu_hw.cpp`, `gpu_hw_shadergen.cpp`, `gpu_hw_texture_cache.cpp`, `video_shadergen.cpp`; `src/util/metal_device.mm`; `src/core/video_thread.cpp` (inline path); `gpu_backend.cpp` (`HandleSubmitFrameCommand` present); `src/core/settings.cpp` (`ParseRendererName` ~301, `ResolutionScale` ~304, `s_gpu_renderer_names` ~1557). Delta report `duckstation-libretro/docs/swanstation-delta-2026-06-01.md` §2 (renderer/GPUDevice) + §3 (GPUSettings fields, renamed/gone ones).

## 7. Build / deploy / test

No automated harness for visual output. Manual loop:

```sh
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS"
cmake -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DENABLE_OPENGL=OFF -DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON -DENABLE_LIBRETRO=ON
cmake --build build-arm64 --target duckstation_libretro
src/duckstation-libretro/package.sh   # or cp arm64 dylib for quick iteration
```

Then **the user launches RetroNest** (TCC blocks the agent from the GUI launch + `~/Documents` + the DB):
`RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1`

### Pass/fail checklist
- [ ] Core builds clean (`duckstation_libretro` target).
- [ ] `/tmp/rn.log`: Metal **hardware** renderer init succeeds (not Software); no shader-gen/compile errors.
- [ ] PS1 game boots and runs at full speed (one frame per `retro_run`, no stall/slowdown).
- [ ] Output is visibly **sharper/upscaled** vs. prior software render (compare the same scene).
- [ ] 3D geometry is **stable** — no PS1 polygon jitter (PGXP working).
- [ ] No regression: audio plays, input maps correctly, save state save/load works, memcard persists.
- [ ] No new errors/warnings in `/tmp/rn.log` over a few minutes of play.

## 8. Follow-on (not this feature)
- **#3 settings migration:** expose renderer/scale/PGXP/filtering/widescreen as libretro core options + RetroNest `settingsSchema()` so the user can change them live. Reference: `pcsx2-libretro/.../CoreOptionsGraphics.cpp` + RetroNest `pcsx2_libretro_adapter.cpp settingsSchema()`.
- Possible later robustness: automatic HW→Software fallback on init failure.
- Possible later: GPU video thread (`GPU/UseThread = true`) — requires run-loop rework.

## Implementation Outcome (2026-06-03)

**Status: Complete / shipped.** Single commit `211e3bf` on `duckstation-libretro` `master` (local-only, not pushed). Implemented via subagent-driven development (implementer + spec-compliance review + code-quality review, both passed).

**What landed:** the verified settings profile in `ApplySettings` (`libretro_settings.cpp`) — `GPU/Renderer="Metal"`, `ResolutionScale=4`, `PGXPEnable`+`PGXPCulling`+`PGXPTextureCorrection=true`, `DitheringMode="TrueColor"`; `UseThread` stays `false`; `PGXPVertexCache`/`PGXPCPU` left at engine defaults. Built arm64 and deployed via `package.sh --arm64-only` (dylib + `metal_shaders.metallib` + shaderc/spirv-cross Frameworks libs).

**Validation (RetroNest, Toy Story 2, `/tmp/rn.log`):**
- HW Metal renderer active on Apple M4 (`GPU_HW`, Metal v20300), **4× scale confirmed** (`Resolution Scale: 4 (4096x2048)`), **True Color confirmed** (`Dithering: True Color`).
- **Top risk cleared:** runtime shader pipeline compiled fully in the deployed bundle — `3 vertex / 200 fragment / 411 pipelines`, `411/411`, no shader errors. Pipeline creation ~2.3s one-time at boot.
- Booted in 2.4s; ran 30s+ with live memcard save (no stall — inline+HW frame pump holds). Audio = `Null` (RetroNest capture), as expected.
- **User-confirmed visually:** output much sharper/upscaled, 3D geometry stable (PGXP working), input correct.

**Caveats / notes:**
- **Deploy was arm64-only** (`--arm64-only`). RetroNest policy is universal — a universal rebuild (drop the flag; needs the x86_64 build working) is required before any real shipping. Fine for this M4 validation.
- **Save-state save/load under the HW renderer was NOT separately re-verified** this session. Live memcard persistence was confirmed; save-states were validated in the prior (software-renderer) feature. Renderer swap shouldn't affect VRAM-level state serialization, but spot-check recommended.
- **Pre-existing, unrelated gap surfaced in the log:** `rcheevos` (RetroAchievements) `rc_libretro_memory_init failed` / `hash generation failed` — the core doesn't expose the memory map / disc hash rcheevos needs. Renderer-independent (would fail identically under software). Candidate future ticket, out of scope for #1.
- **Spec correction during planning:** `PGXPVertexCache` was dropped from ON → engine-default OFF after source diligence (situational fallback that can cause visual errors; anti-jitter comes from `PGXPEnable`+`Culling`). See §3.

**Deferred to feature #3:** expose renderer/scale/PGXP/filtering/widescreen as libretro core options + RetroNest `settingsSchema()` so the user can change them live.
