# RPCS3 → libretro core — Milestone 1: "It's a core" (x86_64 macOS)

A for-fun / learning sub-project: fork **RPCS3** (PS3 emulator) and turn it into a **libretro core** so it can eventually run inside RetroNest like the Dolphin/PPSSPP/PCSX2 cores. This is acknowledged up front to be a *much* larger undertaking than those (RPCS3 is monolithic and not designed as a frontend-driven library), so it is **decomposed into milestones** and this spec covers **only Milestone 1**.

> **Status:** design for Milestone 1, approved 2026-05-29. Later milestones are sketched at the end but NOT designed yet — each gets its own spec → plan cycle.

## Why this is hard (context the executor must internalize)

Unlike Dolphin/DuckStation (which had a core↔frontend separation and a frame-steppable run loop), RPCS3 is a monolithic standalone Qt app:
- Its run loop is many free-running threads — PPU + up to 6 SPU threads + an async RSX (GPU command) thread + audio — synchronized by **emulated hardware timing**, not host frames. libretro's `retro_run()` ("advance exactly one frame, then return") has no natural hook here.
- Its renderer (Vulkan, via MoltenVK on macOS) owns its own swapchain/window.
- There is **no official headless mode**; the emulator core interacts with the GUI through an `EmuCallbacks`-style indirection (GS frame creation, message dialogs, etc.).

Milestone 1 deliberately stops **before** any of those hard problems. It proves only the *build + libretro API plumbing*: a loadable core that initialises and runs an empty (black) frame loop. The hardest pieces (frame-step barrier, renderer retarget, input) are explicitly deferred.

## Platform & toolchain (decided)

**Build target: macOS x86_64 under Rosetta**, to match RetroNest's existing deploy so the eventual core drops into the same frontend. RPCS3 officially supports this: per the RPCS3 build wiki, on Apple Silicon you build/run the Intel build via Rosetta 2 + the x64 Homebrew at `/usr/local` + MoltenVK — the **same toolchain already used for the Dolphin libretro core and RetroNest** (`arch -x86_64`, `/usr/local` Homebrew, `-DCMAKE_OSX_ARCHITECTURES=x86_64`, scrub `/opt/homebrew`). Performance under Rosetta is poor, which is irrelevant for Milestone 1 (a black screen is the goal). arm64-native is much faster but would break RetroNest's x86_64 deploy, so it is out of scope. Requires macOS 14.4+/15.0+ with a supported GPU (the dev machine qualifies).

References:
- RPCS3 build guide: https://wiki.rpcs3.net/index.php?title=Help:Building_RPCS3
- RPCS3 arm64 background (why we deliberately stay x86_64): https://blog.rpcs3.net/2024/12/09/introducing-rpcs3-for-arm64/
- Precedent to mirror: the in-tree `DolphinLibretro` target in the `dolphin-libretro` fork (`Source/Core/DolphinLibretro/`, CMake `SUFFIX .dylib` override, x64/Rosetta build) and its build notes in RetroNest memory.

## Goal / success criteria

**Committed (Milestone 1 = done when all true):**
1. A fork of `RPCS3/rpcs3` exists with a `libretro` branch and an in-tree libretro target.
2. The target builds, x86_64 under Rosetta, into `rpcs3_libretro.dylib` — a Mach-O **x86_64** MODULE (verify with `file` / `lipo -info`).
3. A libretro frontend (**RetroArch x86_64** is the reference loader) loads the core without crashing: the core appears, `retro_get_system_info` returns a name + ROM extensions, `retro_api_version`/`retro_init`/`retro_get_system_av_info` are called, `retro_load_game(path)` returns `true`, and `retro_run()` is called repeatedly, each call presenting a **black framebuffer** via `video_refresh`, with no crash over a sustained loop.

**Stretch (Phase 1b — attempted inside M1, NOT required for success):**
4. `retro_load_game` invokes RPCS3's emulator boot of the supplied path **headlessly** (no Qt, no real GS render target) and logs how far it gets before failing. The *value* here is the diagnostic: it probes the core↔GUI coupling that governs Milestone 2+. Reaching "fails at GS-frame creation" is a successful probe.

## Scope

**In:** the fork + branch; one in-tree CMake MODULE target; a `libretro.cpp` implementing the minimum libretro API to load + run an empty loop; vendored `libretro.h`; building it x86_64/Rosetta; loading it in RetroArch x86_64; (stretch) a headless `Emu` boot attempt with logging.

**Out (all are later milestones or other sub-projects):** any rendering / first frame; the frame-step run loop across PPU/SPU/RSX; input; audio; savestates; RetroAchievements / `SET_GAME_IDENTITY`; RetroNest manifest + install-from-fork integration; performance work; arm64 or universal builds; a CI release workflow; PS3 firmware/per-game-config handling.

## Architecture

- **Repo:** fork `RPCS3/rpcs3` → user's GitHub (`fork` remote), upstream stays `origin`. Work on branch `libretro`. Clone `--recursive` (RPCS3 has many `3rdparty/` submodules). This mirrors the `dolphin-libretro` fork layout.
- **In-tree target:** new directory `rpcs3/rpcs3_libretro/` containing:
  - `libretro.h` — vendored libretro API header (from libretro-common; pin a known revision).
  - `libretro.cpp` — the `retro_*` entry points.
  - `CMakeLists.txt` — defines target `rpcs3_libretro` as a `MODULE` library; override `SUFFIX` to `.dylib` (RPCS3/CMake won't name a MODULE correctly for libretro otherwise — same fix as DolphinLibretro). Wired into the top-level build behind an `option(BUILD_LIBRETRO ...)` defaulting OFF so it never disturbs the normal `rpcs3` app build.
- **C-first layering:**
  - *Phase 1* compiles `libretro.cpp` linking **little or none** of RPCS3's emulator code — just enough to satisfy the API and prove build→load. `retro_run()` clears an internal framebuffer to black and calls `video_refresh`; `retro_get_system_av_info` reports a fixed geometry (e.g. 1280×720) and ~60 fps.
  - *Phase 1b* links RPCS3's emulator core (the `Emu` / `Emulator` translation units under `rpcs3/Emu/`) and, in `retro_load_game`, calls the boot entry point headlessly. Exact symbols (the global emulator instance, the boot method, the `EmuCallbacks` it needs stubbed) are **confirmed during Phase 0 recon** — do not assume names; read the source.
- **Frontend for testing:** RetroArch x86_64 (install via the x64 Homebrew cask or a downloaded Intel build; run under Rosetta). RetroArch's `--verbose` log is the primary evidence channel. RetroNest is intentionally not wired up in M1.

## Phased plan (de-risk order)

- **Phase 0 — toolchain + recon (no libretro code yet).** Clone the fork `--recursive`. Build **stock** `rpcs3` x86_64/Rosetta following the RPCS3 wiki adapted to the project's `/usr/local` x64-Homebrew + `arch -x86_64` + `-DCMAKE_OSX_ARCHITECTURES=x86_64` + `-DCMAKE_IGNORE_PATH=/opt/homebrew` dance. Outcomes: confirm the toolchain works here, measure build time/deps, and **read the source to map the core boundary** — locate the emulator core (`rpcs3/Emu/`), the boot entry point, and where/how it calls back into the GUI (`EmuCallbacks` and the GS-frame factory). Record exact symbol names + the coupling findings; these define Phase 1b and Milestone 2.
- **Phase 1 — stub core that builds + loads.** Add `rpcs3/rpcs3_libretro/` (header, `libretro.cpp` stub, CMake MODULE target behind `BUILD_LIBRETRO`). Implement the minimal API (`retro_api_version`, `retro_get_system_info`, `retro_get_system_av_info`, `retro_set_environment`, `retro_set_*` callbacks, `retro_init`/`retro_deinit`, `retro_load_game` returns true, `retro_unload_game`, `retro_run` → black frame, `retro_get_region`, `retro_serialize_size`→0, etc.). Build → `rpcs3_libretro.dylib`. Load in RetroArch x86_64; confirm init + a sustained no-crash run loop.
- **Phase 1b — headless boot probe (stretch).** Link the `Emu` core; in `retro_load_game`, set up minimal/stub `EmuCallbacks` and invoke the boot of the path with no real render target; log progress and the first failure. Capture the failure point as the input to Milestone 2.

## Key risks / unknowns

1. **Core↔Qt coupling (the big one).** `Emu` may not link without GUI symbols, and boot likely demands a GS-frame/callbacks object. Phase 1b exists precisely to measure this. If even linking `Emu` drags in Qt, that finding reshapes Milestone 2 (and is itself a valuable result).
2. **Heavy build.** RPCS3 + LLVM + submodules under Rosetta is large and slow; Phase 0 may take significant wall-clock and dep fiddling. Budget for 1–2 iterations on deps (mirrors the Dolphin CI dep-tuning experience).
3. **Global static init / threads at load.** RPCS3 uses extensive global state and its own thread pools; merely `dlopen`-ing the core or calling `retro_init` could spin things up unexpectedly. Watch init ordering and keep Phase 1 from triggering emulator startup.
4. **MODULE naming / not disturbing the app build.** Keep the libretro target isolated behind an OFF-by-default option so the normal `rpcs3` build is unaffected.

## Testing / acceptance

- **Build:** `file rpcs3_libretro.dylib` → `Mach-O 64-bit … x86_64`; `lipo -info` confirms x86_64.
- **Load (committed criteria):** RetroArch x86_64 `--verbose` shows `retro_get_system_info` + `retro_init` + `retro_get_system_av_info`; loading a dummy/any file logs `retro_load_game` returning true; the run loop calls `retro_run` repeatedly with a black `video_refresh` and **no crash** for a sustained period.
- **Phase 1b (stretch):** log shows the boot attempt and the exact point it stops.
- No automated unit tests — this is a build/integration spike; evidence is the build artifact + frontend logs (consistent with how the Dolphin core was first brought up).

## Workflow / auto-mode

The assistant prepares all code, CMake, and local commits. The **fork creation (`gh repo fork` / repo create), `git push`, and any tag/release steps are USER-RUN** — the same auto-mode constraint that applied to the SP9 work. The assistant hands over exact commands.

## Milestone roadmap (NOT designed here — future specs)

- **M2 — First frame:** retarget RPCS3's RSX/Vulkan output through libretro's HW-render (Vulkan) path; produce at least one real rendered frame (boot animation / homebrew). Depends on the Phase 0/1b coupling findings.
- **M3 — Frame-step run loop:** add a barrier that pauses/resumes PPU/SPU/RSX threads at frame boundaries so `retro_run()` advances exactly one frame.
- **M4 — Input + audio.**
- **M5 — RetroNest integration:** manifest, install-from-fork, `SET_GAME_IDENTITY`, pause/fast-forward, memory maps — reusing the patterns built for Dolphin.
- (Performance, arm64/universal, CI release: later still.)
