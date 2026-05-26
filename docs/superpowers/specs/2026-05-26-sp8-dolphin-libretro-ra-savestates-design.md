# SP8 — Dolphin libretro RetroAchievements + savestates + packaging

Last of the 9 sub-projects (SP0–SP8) in the master conversion design
(`docs/superpowers/specs/2026-05-23-dolphin-libretro-conversion-design.md`, the **SP8 row**).
SP0–3, 5, 6, 7 shipped; SP4 (Vulkan) is deferred. After SP8 the conversion reaches
**v1 feature parity** on macOS/Metal.

## Goal

Reach v1 feature parity for the Dolphin libretro core:

- **RetroAchievements** working on GameCube *and* Wii, through RetroNest's shared
  `RcheevosRuntime` (not Dolphin's native `RetroAchievements.ini`).
- **Savestates** working: Save & Quit + Resume, and the in-game save/load-state
  hotkeys (all host-wired in SP3 but currently no-ops because the core stubs them).
- **Persistent saves**: GC memcards / Wii NAND / SD images survive an app restart.
- **One-command local deploy** of the universal core + Dolphin `Sys` data.

Metal-only. Vulkan (SP4) stays deferred; the master design's "both backends"
acceptance is read as Metal-only until SP4 (Windows) happens.

## Scope

**In scope — four core (`dolphin-libretro`, `libretro` branch) changes + host verification:**

1. Memory exposure for RA via `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` (+ `retro_get_memory_data(SYSTEM_RAM)`).
2. Savestates via Dolphin's internal buffer save/load, exposed through `State.h`.
3. Stable, host-provided user directory (replaces the hardcoded `/tmp` path).
4. Local deploy automation (universal `lipo` + `Sys` install).
5. Host-side verification matrix (no new host code expected).

**Out of scope:**

- **Full GitHub fork + CI release publishing** — gets its own follow-on spec, mirroring
  `2026-05-21-pcsx2-libretro-github-publishing-design.md`. SP8 does *local* deploy only.
  (The core repo currently has no fork remote — its only remote is upstream
  `dolphin-emu/dolphin`, no write access. Resolving the push target belongs to that spec.)
- **An Achievements settings card** — the shared `RcheevosRuntime` owns the RA toggles;
  `DolphinLibretroAdapter::raConsoleId` already returns 16/19 (`dolphin_libretro_adapter.cpp:8`),
  so no new host schema is needed.
- **Wii motion/pointer achievements** — SP5 shipped Wii Classic-controller only (no IR/motion),
  so motion-gated games remain unplayable/unachievable. Carried forward.
- **Vulkan** (SP4).

## Current state (what's stubbed)

Verified in `Source/Core/DolphinLibretro/LibretroFrontend.cpp` (2026-05-26):

- `retro_serialize_size` → `0`, `retro_serialize` → `false`, `retro_unserialize` → `false` (`:186-188`).
- `retro_get_memory_data` → `nullptr`, `retro_get_memory_size` → `0`; no `SET_MEMORY_MAPS` (`:256-257`).
- `UICommon::SetUserDirectory("/tmp/dolphin-libretro-user")` (`:123`).

Host is further along than the SP8 prep notes assumed: `raConsoleId` (16/19) is done, and the
shared `RcheevosRuntime` already supports **both** a `SET_MEMORY_MAPS` path *and* a per-console
`retro_get_memory_data(SYSTEM_RAM/SAVE_RAM/...)` fallback
(`cpp/src/core/libretro/rcheevos_runtime.cpp:118-129, 373-388`). The core only needs to expose
memory in one of those shapes.

## Core change 1 — Memory exposure for RA

### Why `SET_MEMORY_MAPS` (not SYSTEM_RAM-only)

rcheevos' own region tables decide this (`rcheevos-src/src/rcheevos/consoleinfo.c`):

- **GameCube** (`RC_CONSOLE_GAMECUBE` = 16) — **1 region**: `0x000000–0x17FFFFF` (24 MB),
  real address `0x80000000`, `RC_MEMORY_TYPE_SYSTEM_RAM` (`:519-522`).
- **Wii** (`RC_CONSOLE_WII` = 19) — **3 regions** (`:972-977`):
  - MEM1 `0x000000–0x17FFFFF` (24 MB) @ real `0x80000000`, SYSTEM_RAM
  - `0x1800000–0xFFFFFFF` @ `0x81800000`, **UNUSED** (a gap — not backed)
  - MEM2 `0x10000000–0x13FFFFFF` (64 MB) @ real `0x90000000`, SYSTEM_RAM

Dolphin allocates MEM1 (`MemoryManager::GetRAM()`) and MEM2 (`GetEXRAM()`) as **separate,
non-contiguous** buffers (`Source/Core/Core/HW/Memmap.h:91-92`). The shared runtime's contiguous
`retro_get_memory_data(SYSTEM_RAM)` fallback lays SYSTEM_RAM regions out back-to-back from one
pointer — fine for GameCube's single region, but it **cannot** represent Wii's two separated RAM
blocks. So a memory map matched by *real address* is required for Wii achievements to work.

### Implementation

In the core, after `BootCore` (RAM is allocated at boot, and the GC-vs-Wii layout differs per game):

- Build a `retro_memory_descriptor[]` and emit `RETRO_ENVIRONMENT_SET_MEMORY_MAPS`:
  - **Always**: MEM1 — `ptr = GetRAM()`, `len = GetRamSizeReal()` (24 MB), `start = 0x80000000`.
  - **Wii only**: MEM2 — `ptr = GetEXRAM()`, `len = GetExRamSizeReal()` (64 MB), `start = 0x90000000`.
  - Leave the `0x81800000` gap unmapped; rcheevos treats it as UNUSED.
- Also implement `retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)` → `GetRAM()` and
  `retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM)` → `GetRamSizeReal()` as the standard
  libretro courtesy/compat accessor. The shared runtime prefers the map when present.
- Detect GC vs Wii from the booted title (the running `Core::System` / boot parameters already
  know which; pick the check the surrounding code uses).

## Core change 2 — Savestates

### Approach: expose Dolphin's internal buffer API

`State.h`'s public surface is **slot/file-only** (`Save/Load/SaveAs/LoadAs`, `:95-99`) and `SaveAs`
runs LZ4 compression **asynchronously** on a work-queue thread. But `State.cpp` already has
synchronous in-memory primitives used by the undo-load feature:

- `static std::size_t SaveToBuffer(Core::System&, Common::UniqueBuffer<u8>&)` (`State.cpp:231`) —
  grows the buffer to the measured size and returns the actual byte count.
- `static bool LoadFromBuffer(Core::System&, std::span<u8>)` (`State.cpp:222`).

Add thin **public** wrappers in `State.h` around these (mirrors what PCSX2-libretro does). This
avoids temp-file I/O, the async compress-and-dump flush race, and gives an exact size.

### Threading

Dolphin savestates require the CPU thread quiesced. Run both serialize and unserialize through
`Core::RunOnCPUThread(system, fn)` (`Source/Core/Core/Core.h:161`) so the buffer op executes in a
safe state. This touches EmuThread lifecycle — the quit-crash paths
(`memory/metal-teardown-quit-crash`) must be re-smoked.

### `retro_serialize_size`

Dolphin states are variable-size and version/build-tagged, but libretro expects a stable
per-session upper bound (the frontend allocates exactly that buffer). Strategy:

- On first call, do a measuring `SaveToBuffer` to learn the actual size, then return it **padded
  with headroom**; cache and never shrink it (clamp to the max seen). A later real `retro_serialize`
  must never need more than the reported size or the frontend buffer overflows.
- Mirrors how PCSX2-libretro stabilizes its serialize size.

## Core change 3 — Stable user directory

Replace `UICommon::SetUserDirectory("/tmp/dolphin-libretro-user")` (`LibretroFrontend.cpp:123`):

- In `retro_init` (env_cb is available — `retro_set_environment` runs before `retro_init`), query
  `RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY` and root the Dolphin user dir there. The host maps that to
  `Paths::emulatorDataDir(emulator, system)` — a stable, per-emulator/per-system directory
  (`cpp/src/core/game_session.cpp:211`, `environment_callbacks.cpp:151-152`).
- Fall back to the existing `/tmp` path if the host returns nothing/empty.
- This persists the GC memcards / Wii NAND / SD images that SP7's GC/Wii settings reference, plus
  savestate files. Per-system save dirs mean GC and Wii get independent Dolphin user dirs, which is
  fine — Dolphin already separates `GC/` and `Wii/` internally.

## Core change 4 — Local deploy automation

Replace the manual lipo + `Sys` copy (today's steps in `memory/dolphin-libretro-build-setup`) with a
committed script (`Source/Core/DolphinLibretro/tools/deploy.sh`, or a CMake `deploy` target):

- `lipo -create` the arm64 + x86_64 `dolphin_libretro.dylib` →
  `~/Documents/RetroNest/emulators/libretro/cores/dolphin_libretro.dylib`.
- `cp -R Data/Sys` → the RetroNest `.app` `Contents/Resources/Sys/`.
- Parameterize both destination paths (defaults from the build memory) so it is reproducible, not
  pinned to one machine layout.
- Scope is *only* the local build→deploy step. Fork push + CI release stay in the follow-on spec.

## Host side — verification, not new code

`raConsoleId` (16/19) and the shared `RcheevosRuntime` memory plumbing are in place, so once the
core lands this is a verification matrix:

- **RA end-to-end** — load a GC title and a Wii title; confirm the achievement set loads
  (`rc_libretro_memory_init` succeeds with non-zero regions) and an achievement triggers. Wii is the
  real test — it exercises the MEM2 descriptor.
- **Savestates** — Save & Quit → Resume restores; in-game save/load-state hotkeys work; re-smoke the
  quit-crash paths.
- **Persistence** — a GC memcard / Wii save survives an app restart (proves the user-dir change).
- **FF HUD pill + hotkey suppression** — behave with Dolphin (shared infra).
- **RA hardcore** — load-state is blocked in hardcore mode (shared runtime enforces; verify it holds).

## Testing

- **Core round-trip / map check** — a standalone check in the existing `tools/` test style: the
  memory-map descriptors have the right base/size/real-address per console (GC = MEM1 only;
  Wii = MEM1 @ `0x80000000` + MEM2 @ `0x90000000`), and `serialize → unserialize` round-trips to an
  identical buffer.
- **Existing verify loop unchanged** — `check_schema_fidelity` (still 90/90), `test_core_options`,
  `DolphinLibretroSchema`. SP8 adds no core options, so these just keep passing.
- **Manual acceptance (SP8 gate)** — GC + Wii on Metal: RA triggers, save/resume works, saves
  persist, FF + hotkeys behave.

## Error handling

- Memory map only emitted when RAM pointers are valid (post-boot); if `GetRAM()` is null, skip the
  map and log — RA simply won't initialize rather than crash.
- `retro_serialize` returns `false` if the buffer is smaller than the live state needs (should not
  happen given the padded size, but guard anyway). `retro_unserialize` returns `false` on a failed
  `LoadFromBuffer` (e.g. version mismatch) without tearing down emulation.
- User-dir: empty `GET_SAVE_DIRECTORY` falls back to the `/tmp` path so boot never depends on it.

## Risks

1. **Serialize threading** — must run under `RunOnCPUThread`; getting it wrong is a hang or a
   teardown crash. Primary risk.
2. **`serialize_size` stability** — a reported size smaller than a later actual state overflows the
   frontend buffer. Pad generously; never shrink.
3. **`State.h` patch surface** — adds to the fork's upstream-merge burden, but it is two small
   wrappers and the fork is already heavily patched.

## Sequencing (informs the plan)

1. **User dir (change 3) first** — unblocks meaningful save/RA testing.
2. **Memory exposure (change 1)** — lets RA be validated independently of savestates.
3. **Savestates (change 2)** — most threading risk; do after RA is confirmed.
4. **Deploy automation (change 4)** — last; orthogonal.
5. **Host verification** — after each core piece lands.
