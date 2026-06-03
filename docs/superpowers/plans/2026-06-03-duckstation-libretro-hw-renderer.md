# DuckStation libretro #1 — Hardware Metal Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Switch the DuckStation libretro core from the Software renderer to the hardware Metal renderer at 4× internal resolution with PGXP and true color, via hardcoded defaults in `ApplySettings`.

**Architecture:** A single-file change to `ApplySettings` in `src/duckstation-libretro/libretro_settings.cpp` — the base-settings-layer block that already seeds `GPU/Renderer`, `GPU/UseThread`, region, memcard, and pad type. The display/present path and the inline run loop are unchanged; this only changes which renderer DuckStation draws with and at what internal resolution. There is no automated test harness for visual output, so validation is a build gate (compiles) plus a manual run gate (user launches RetroNest, inspects output + `/tmp/rn.log` against a checklist).

**Tech Stack:** C++ (DuckStation core), CMake + Ninja, Metal (macOS), libretro, RetroNest host.

**Spec:** `docs/superpowers/specs/2026-06-03-duckstation-libretro-hw-renderer-design.md`

**Repo:** `/Users/mark/Documents/Projects/duckstation-libretro` (fork `master`, local-only — **never push**).

---

## Important context for the implementer

- **All settings keys/values in this plan are already verified** against `src/core/settings.cpp` (line refs in the spec §3 table). Do not re-guess them; use them exactly as written.
- **Why some lines set values that already match engine defaults** (`PGXPCulling=true`, `PGXPTextureCorrection=true`, `DitheringMode="TrueColor"`): `ApplySettings` is the self-documenting single source of truth for our enhancement profile, and explicit writes are robust to upstream default changes. Keep them.
- **Why PGXP Vertex Cache and PGXP CPU are NOT set:** both stay at engine default `false` deliberately (vertex cache is a situational compatibility fallback that can cause visual errors; CPU mode is heavy). Do not add them.
- **The agent cannot launch the RetroNest GUI** (TCC blocks `~/Documents` + the app DB). Task 4 is executed by the **user**; the agent prepares the exact commands and the checklist, then waits.
- **Setter methods** on `SettingsInterface`: `SetStringValue`, `SetBoolValue`, `SetUIntValue` (all confirmed present and already used in this file / `settings.cpp:702`).

---

## Task 1: Enable HW Metal renderer + enhancement profile in `ApplySettings`

**Files:**
- Modify: `src/duckstation-libretro/libretro_settings.cpp` (the base-settings block inside `ApplySettings`, currently lines ~117–125)

- [ ] **Step 1: Read the current block to anchor the edit**

Run: `sed -n '117,126p' src/duckstation-libretro/libretro_settings.cpp`

Expected current content (the exact anchor for Step 2):

```cpp
    // (section, key) strings verified against core/settings.cpp Settings::Load:
    //   "Console"/"Region"          (line 261)   value "Auto"  (region idx 0)
    //   "GPU"/"Renderer"            (line 301)   value "Software"
    //   "GPU"/"UseThread"           (line 320, default true) -> false (inline run loop)
    //   "MemoryCards"/"Card1Type"   (line 526)   value "PerGameTitle"
    si->SetStringValue("Console", "Region", "Auto");
    si->SetStringValue("GPU", "Renderer", "Software");
    si->SetBoolValue("GPU", "UseThread", false);
    si->SetStringValue("MemoryCards", "Card1Type", "PerGameTitle");
```

If the lines differ (file drifted), adjust the `old_string` in Step 2 to match what's actually there — the change is conceptually: flip `Renderer` to `"Metal"` and append the enhancement block after the `UseThread` line.

- [ ] **Step 2: Apply the edit**

Replace the block above with the following. This (a) updates the verification comment, (b) changes `Renderer` from `"Software"` to `"Metal"`, and (c) adds the HW enhancement profile after the `UseThread` line:

```cpp
    // (section, key) strings verified against core/settings.cpp Settings::Load:
    //   "Console"/"Region"          (line 261)   value "Auto"  (region idx 0)
    //   "GPU"/"Renderer"            (line 301)   value "Metal" (HW Metal backend)
    //   "GPU"/"UseThread"           (line 320, default true) -> false (inline run loop)
    //   "MemoryCards"/"Card1Type"   (line 526)   value "PerGameTitle"
    si->SetStringValue("Console", "Region", "Auto");

    // Feature #1: hardware Metal renderer + enhancement profile (hardcoded defaults;
    // user-facing config is deferred to feature #3). All keys/values verified against
    // core/settings.cpp Settings::Load / LoadPGXPSettings. UseThread stays false: the
    // run loop (RunFrame interrupted at FrameDone) is built for inline execution.
    //
    //   "GPU"/"Renderer"="Metal"   force the HW Metal backend (deterministic vs
    //                              "Automatic"; both resolve to Metal on macOS).
    //                              Parsed by ParseRendererName, names at settings.cpp:1557.
    //   "GPU"/"ResolutionScale"=4  4x internal res (1280x960); GetUIntValue settings.cpp:304.
    //   "GPU"/"PGXPEnable"=true     PGXP master switch; settings.cpp:357. With culling on,
    //                              this gives geometry-stable (jitter-free) 3D.
    //   "GPU"/"PGXPCulling"=true    settings.cpp:646 (engine default true; explicit here).
    //   "GPU"/"PGXPTextureCorrection"=true  perspective-correct textures; settings.cpp:647
    //                              (engine default true; explicit here).
    //   "GPU"/"DitheringMode"="TrueColor"  24-bit true color, no dither banding. Parsed by
    //                              ParseGPUDitheringModeName (names settings.cpp:1725); the
    //                              GPUDitheringMode::TrueColor enum value is also the engine
    //                              default (settings.h:226) — set explicitly to self-document.
    //
    // Deliberately NOT set (stay at engine default false): "GPU"/"PGXPVertexCache"
    // (settings.cpp:649 — situational compatibility fallback, can cause visual errors;
    // the anti-jitter win comes from PGXPEnable+Culling) and "GPU"/"PGXPCPU"
    // (settings.cpp:650 — heavy accuracy mode, perf risk). Texture filtering / widescreen /
    // downsampling also left at defaults (deferred to #3).
    si->SetStringValue("GPU", "Renderer", "Metal");
    si->SetBoolValue("GPU", "UseThread", false);
    si->SetUIntValue("GPU", "ResolutionScale", 4u);
    si->SetBoolValue("GPU", "PGXPEnable", true);
    si->SetBoolValue("GPU", "PGXPCulling", true);
    si->SetBoolValue("GPU", "PGXPTextureCorrection", true);
    si->SetStringValue("GPU", "DitheringMode", "TrueColor");

    si->SetStringValue("MemoryCards", "Card1Type", "PerGameTitle");
```

- [ ] **Step 3: Sanity-check the edit**

Run: `grep -n 'GPU", "Renderer"\|ResolutionScale\|PGXPEnable\|PGXPCulling\|PGXPTextureCorrection\|DitheringMode' src/duckstation-libretro/libretro_settings.cpp`

Expected: one match each, with `Renderer` now `"Metal"` (no remaining `"Software"` for `GPU/Renderer`), `ResolutionScale` `4u`, `PGXPEnable`/`PGXPCulling`/`PGXPTextureCorrection` `true`, `DitheringMode` `"TrueColor"`.

Also confirm no stray second renderer write:
Run: `grep -n '"Renderer", "Software"' src/duckstation-libretro/libretro_settings.cpp`
Expected: no output.

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/Projects/duckstation-libretro
git add src/duckstation-libretro/libretro_settings.cpp
git commit -m "feat(libretro): enable HW Metal renderer at 4x + PGXP + true color"
```

---

## Task 2: Build the core (compile gate)

**Files:** none (build only)

- [ ] **Step 1: Configure (if `build-arm64` not already configured)**

```bash
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS"
cmake -B build-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DENABLE_OPENGL=OFF -DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON -DENABLE_LIBRETRO=ON
```

Expected: CMake configures without error (ends with "Build files have been written to: .../build-arm64"). If `build-arm64` already exists and was configured this way, this is a fast no-op and can be skipped.

- [ ] **Step 2: Build the libretro target**

```bash
cd /Users/mark/Documents/Projects/duckstation-libretro
cmake --build build-arm64 --target duckstation_libretro
```

Expected: `[N/N] Linking ... duckstation_libretro...` with exit code 0, no compile errors. The edit is settings-only (string/bool/uint setters already used in this file), so a clean compile is expected. If it fails, fix the edit (likely a typo in a key string or setter name) before proceeding — do not continue to deploy.

- [ ] **Step 3: Confirm the dylib was produced/updated**

Run: `ls -la build-arm64/src/duckstation-libretro/*.dylib`
Expected: a `duckstation_libretro*.dylib` with a fresh modification time.

---

## Task 3: Deploy to the RetroNest bundle

**Files:** none (deploy script)

- [ ] **Step 1: Run the package/deploy script**

```bash
cd /Users/mark/Documents/Projects/duckstation-libretro
src/duckstation-libretro/package.sh
```

Expected: the script reports building/copying the universal dylib + resources (metallib) + Frameworks libs into the RetroNest emulator location, exit code 0. (Per the handoff, `package.sh` ships the universal dylib, `metal_shaders.metallib`, resources, and the shader-compiler libs — everything the HW path needs at runtime is already covered by this script.)

- [ ] **Step 2: Confirm the deployed core was updated**

Run: `package.sh` prints the destination path; confirm the deployed dylib's mtime is fresh. If the script does not print it, locate the deployed core under the RetroNest emulators dir (`.../emulators/duckstation/...`) and check its timestamp.
Expected: deployed core timestamp matches this deploy run.

---

## Task 4: Manual validation (USER-RUN — agent waits)

**Files:** none (runtime validation)

> The agent CANNOT launch the RetroNest GUI (TCC blocks `~/Documents` + the app DB). The agent presents the commands + checklist below, then **waits for the user** to run them and report results. Do not mark this task complete until the user confirms.

- [ ] **Step 1: User launches RetroNest with logging**

Ask the user to run, then play a PS1 title for ~2 minutes (pick one with visible 3D so PGXP/upscaling are obvious — e.g. a racing or 3D-action game):

```bash
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1
```

- [ ] **Step 2: Inspect the log for renderer init + shader compilation (top risk)**

After the session, the agent reads the log:
Run: `grep -iE "renderer|metal|shader|pgxp|error|fail|warn" /tmp/rn.log | head -80`

Expected: evidence the **hardware Metal** renderer initialized (not Software), and **no** shader-generation/compilation errors. The HW renderer generates PS1 draw shaders at runtime (`gpu_hw_shadergen` → Metal device), so this is the most likely first break — investigate any shader/compile error here before anything else.

- [ ] **Step 3: Run the pass/fail checklist (agent + user together)**

- [ ] Core built clean (Task 2) and deployed (Task 3).
- [ ] `/tmp/rn.log`: Metal **hardware** renderer init succeeded; no shader-gen/compile errors.
- [ ] PS1 game boots and runs at full speed — one frame per `retro_run`, no stall/slowdown (inline + HW path holds).
- [ ] Output is visibly **sharper/upscaled** vs. the prior software render (compare the same scene).
- [ ] 3D geometry is **stable** — no PS1 polygon jitter/wobble (PGXP + culling working).
- [ ] No regression: audio plays, input maps correctly, save state save/load works, memcard persists.
- [ ] No new errors/warnings in `/tmp/rn.log` over a few minutes of play (texture cache / VRAM at 4× is clean).

- [ ] **Step 4: If validation FAILS, debug before claiming done**

Use the `superpowers:systematic-debugging` skill. Likely first suspects, in order (from spec §6): (1) HW shader generation/compilation in the deployed bundle; (2) inline+HW frame pump (`VideoThread::IsUsingThread()==false`, one frame per `retro_run`); (3) present-at-scale compositing to the NSView Metal layer; (4) texture cache / VRAM at 4×. Do not proceed to Task 5 until every checklist item passes.

---

## Task 5: Record the implementation outcome

**Files:**
- Modify: `docs/superpowers/specs/2026-06-03-duckstation-libretro-hw-renderer-design.md` (in the RetroNest-Project repo) — append an "Implementation Outcome" section.

- [ ] **Step 1: Append the outcome to the spec**

Append a section matching the convention of the skeleton/save-states specs, e.g.:

```markdown
## Implementation Outcome (YYYY-MM-DD)

**Status:** Complete / shipped.
- Settings applied in `ApplySettings`: Renderer=Metal, ResolutionScale=4, PGXPEnable+Culling+TextureCorrection on, DitheringMode=TrueColor.
- Validation: HW Metal renderer confirmed in `/tmp/rn.log`; output visibly upscaled; PGXP geometry stable; no audio/input/save-state/memcard regression.
- Notes / surprises: <anything found during validation — shader-pipeline behavior, VRAM at 4×, any per-game caveats>.
- Deferred to #3: user-facing core options + RetroNest settingsSchema() for these knobs.
```

Fill in the real date and the actual validation notes (replace the `<...>` placeholder with what was observed).

- [ ] **Step 2: Commit the outcome (RetroNest-Project repo)**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add docs/superpowers/specs/2026-06-03-duckstation-libretro-hw-renderer-design.md
git commit -m "docs: HW Metal renderer implementation outcome (complete)"
```

---

## Done when
- `ApplySettings` selects the HW Metal renderer with the verified 4×/PGXP/true-color profile, committed on `duckstation-libretro` `master`.
- The core builds clean and deploys via `package.sh`.
- The user-run validation checklist (Task 4, Step 3) fully passes.
- The implementation outcome is recorded in the spec and committed.
