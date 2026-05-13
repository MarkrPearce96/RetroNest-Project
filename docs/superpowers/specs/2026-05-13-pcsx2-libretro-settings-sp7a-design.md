# SP7a — Settings push: Resources path + region/fps

**Date:** 2026-05-13
**Status:** Approved — ready for plan
**Sub-project:** SP7a (subset of SP7 "Settings push")
**Touches:** `pcsx2-libretro/` only. No RetroNest-Project changes.

## Goal

Eliminate the two dev-machine-specific hardcodes that block installing the
pcsx2-libretro core on any other machine, and report the correct PS2 region
(NTSC vs PAL) and frame rate (59.94 vs 50) to the libretro host:

1. **Hardcoded `EmuFolders::Resources` path** → derive at runtime from the
   running dylib's own location via `dladdr`.
2. **`retro_get_region` hardcoded to NTSC** and **`av_info.timing.fps`
   hardcoded to 60.0** → detect from the disc serial at load time, refine
   from `gsVideoMode` once the game has executed `SetGsCrt`.

## Out of scope (deferred to SP7b)

- Migrating any of the existing hardcoded settings in `Settings.cpp`
  (renderer = Auto, MTVU = on, FastBoot = on, Achievements = off, memcard
  slot 2 = off, etc.) into libretro core options (`RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2`).
- Any new user-facing UI.
- Override of `params.fast_boot = true` in `retro_load_game` (LibretroFrontend.cpp:448) —
  that pairing belongs with SP7b's FastBoot core option since both touch
  the same effective knob.

The defaults shipped today remain unchanged after SP7a.

## Background

### Current state (relevant code)

- `pcsx2-libretro/Settings.cpp:158-159` and `:204-205` — both assign the
  literal string `/Users/mark/Documents/Projects/pcsx2-libretro/bin/resources`
  to `EmuFolders::Resources` and to the `Folders/Resources` MemorySettingsInterface
  key. The path embeds the developer's home directory and was re-broken
  once already during SP6 when the parent folder was renamed (recovered
  by commit `9f4fb2679`).
- `pcsx2-libretro/LibretroFrontend.cpp:532` — `retro_get_region()` returns
  `RETRO_REGION_NTSC` unconditionally.
- `pcsx2-libretro/LibretroFrontend.cpp` `retro_get_system_av_info` (around
  line 274) — `info->timing.fps = 60.0` placeholder, comment says "phase 3
  will derive from GS region".

### PCSX2 APIs we will lean on

| Need                              | API                                                         |
| --------------------------------- | ----------------------------------------------------------- |
| Disc serial after VMManager init  | `std::string VMManager::GetDiscSerial()` (VMManager.h:93)    |
| GameDB entry for a serial         | `GameDatabase::findGame(const std::string&)` returns        |
|                                   | `const GameDatabaseSchema::GameEntry*` with `std::string region` |
| Runtime video mode (post-SetGsCrt)| `extern GS_VideoMode gsVideoMode;` from `pcsx2/GS.h:231`     |

`s_disc_serial` is populated inside `VMManager::Initialize` via
`cdvdGetDiscInfo` (VMManager.cpp:1056). It is safe to read via the public
accessor after Initialize returns; gsVideoMode remains `Uninitialized`
until the EE thread reaches the SetGsCrt SYSCALL (typically a few frames
into the BIOS / game splash).

## Architecture

Two small additions under `pcsx2-libretro/`, no upstream files modified.

```
pcsx2-libretro/
  CoreResources.{h,cpp}   <-- NEW
  LibretroFrontend.cpp    <-- edited at 3 sites
  Settings.cpp            <-- edited at 2 sites
```

A single new `.{h,cpp}` pair (`CoreResources`) hosts both the dylib-relative
path resolver and the region-detection helper, since each is roughly 30
lines and they share no state — keeping them together avoids file-count
proliferation while still being trivially deletable later.

## Component A — Dylib-relative Resources discovery

### `CoreResources.h`

```cpp
namespace Pcsx2Libretro::CoreResources {
    // Resolves the resources directory by inspecting the running dylib's
    // path via dladdr and appending `pcsx2_libretro_resources/` to the
    // dylib's parent directory. Returns the resolved absolute path. The
    // returned path may not exist on disk; on missing-directory the
    // function logs RETRO_LOG_ERROR but still returns the resolved path so
    // downstream Metal init produces its existing clear failure mode.
    std::string ResolveResourcesDir();
}
```

### Implementation

```cpp
std::string Pcsx2Libretro::CoreResources::ResolveResourcesDir()
{
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&ResolveResourcesDir), &info) == 0
        || info.dli_fname == nullptr)
    {
        FrontendLog(RETRO_LOG_ERROR,
            "dladdr failed when resolving resources dir; "
            "Metal GS init will fail to find metallibs");
        return {};
    }

    std::string dylib_path = info.dli_fname;
    std::string dir = Path::GetDirectory(dylib_path);
    std::string resources = Path::Combine(dir, "pcsx2_libretro_resources");

    if (!FileSystem::DirectoryExists(resources.c_str()))
    {
        FrontendLog(RETRO_LOG_ERROR,
            "Resources directory not found at '%s' — "
            "RetroNest install layout missing pcsx2_libretro_resources/ "
            "next to the dylib. Metal init will fail.",
            resources.c_str());
    }
    return resources;
}
```

### Settings.cpp edits

Both hardcoded path strings (lines 158-159 and 204-205) replaced with a
single resolved value at the top of `InitializeDefaults`:

```cpp
const std::string resources_dir = CoreResources::ResolveResourcesDir();
// ...
EmuFolders::Resources = resources_dir;  // was: "/Users/mark/.../bin/resources"
// ...
g_si.SetStringValue("Folders", "Resources", resources_dir.c_str());
```

The comment blocks at those two sites are rewritten to point at SP7a and
explain the dladdr resolution + the install-layout dependency.

### Build / install impact

The dev workflow currently does:
```
cmake --build build --target pcsx2_libretro
cp build/pcsx2-libretro/pcsx2_libretro.dylib ~/Documents/RetroNest/emulators/libretro/cores/
```

Add one rsync step:
```
rsync -a --delete pcsx2-libretro/bin/resources/ \
    ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/
```

This is documented in the spec (and gets folded into the monthly-rebase
runbook in memory) — the plan will also add it to whatever build helper
already lives in the repo, if any. No new CMake install rules required
for SP7a; the copy-step stays an external dev workflow concern.

## Component B — Region detection + correct fps

### `CoreResources.h` (additions)

```cpp
namespace Pcsx2Libretro::CoreResources {
    struct DetectedRegion {
        unsigned libretro_region;  // RETRO_REGION_NTSC | RETRO_REGION_PAL
        double   fps;              // 59.94 | 50.0
    };

    // Looks up the serial in PCSX2's GameDatabase first, then falls back
    // to prefix heuristic, then defaults to NTSC. Logs a WARN on the final
    // fallback path so unknown discs are visible in real-world use.
    DetectedRegion DetectRegionFromSerial(const std::string& serial);

    // Maps a runtime GS_VideoMode (after SetGsCrt) to libretro region/fps.
    // Returns std::nullopt for Uninitialized.
    std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode);
}
```

### Serial canonical form (load-bearing)

PCSX2's `ExecutablePathToSerial` (CDVD.cpp:525) canonicalizes serials to
`PREFIX-NNNNN` form: `SCES_123.45` becomes `SCES-12345` (underscore →
hyphen, dot removed, uppercase). `VMManager::GetDiscSerial()` returns
this canonical form. Both detection paths below operate on it.

### Detection precedence

1. **GameDatabase lookup.** `GameDatabase::findGame(serial)` returns a
   `GameEntry*` whose `region` field is a free-form string. Match is
   case-insensitive starts-with on "PAL" → PAL/50, anything else (including
   "NTSC-U", "NTSC-J", "NTSC-K", "NTSC", etc.) → NTSC/59.94. If the entry
   is found but `region` is empty, fall through to step 2.
2. **Serial prefix heuristic** (4-letter prefix taken as `serial.substr(0, 4)`):
   - `SLES`, `SCES`, `SLED`, `SCED` → PAL/50
   - `SCUS`, `SLUS`, `SCAJ`, `SLPS`, `SLPM`, `SCKA`, `SLKA`, `SCKR`, `PSXC` → NTSC/59.94
3. **Default + warn.** NTSC/59.94, `FrontendLog(RETRO_LOG_WARN, "Unknown disc serial '%s' — defaulting to NTSC", ...)`.

### `gsVideoMode` mapping (refinement step)

| `GS_VideoMode`                                          | Libretro region | fps   |
| ------------------------------------------------------- | --------------- | ----- |
| `PAL`, `DVD_PAL`                                        | `PAL`           | 50.0  |
| `NTSC`, `DVD_NTSC`, `SDTV_480P`, `HDTV_720P`,           |                 |       |
| `HDTV_1080I`, `HDTV_1080P`, `VESA`                      | `NTSC`          | 59.94 |
| `SDTV_576P`                                             | `PAL`           | 50.0  |
| `Uninitialized`                                         | (none)          | —     |

(SDTV_576P is the EDTV/progressive PAL mode; classified as PAL.)

### Wiring in `LibretroFrontend.cpp`

Module-level statics (file-scope, default-NTSC):

```cpp
namespace {
    unsigned g_detected_region = RETRO_REGION_NTSC;
    double   g_detected_fps    = 59.94;
    bool     g_region_refined  = false;
}
```

1. **In `retro_load_game`,** after `EmuThread::Start()` has reported
   init success but before returning `true`:
   ```cpp
   const auto serial = VMManager::GetDiscSerial();
   const auto detected = CoreResources::DetectRegionFromSerial(serial);
   g_detected_region = detected.libretro_region;
   g_detected_fps    = detected.fps;
   FrontendLog(RETRO_LOG_INFO,
       "[SP7a] region=%s fps=%.2f from serial '%s'",
       g_detected_region == RETRO_REGION_PAL ? "PAL" : "NTSC",
       g_detected_fps, serial.c_str());
   ```

2. **`retro_get_region()`** returns `g_detected_region`.

3. **`retro_get_system_av_info`** sets `info->timing.fps = g_detected_fps`.
   All other fields (`timing.sample_rate`, `geometry.*`, `geometry.aspect_ratio`)
   stay as today — SP7a does not touch them.

4. **`retro_run` refinement** — at the top of `retro_run`, once per call
   while `!g_region_refined`:
   ```cpp
   if (!g_region_refined) {
       if (auto refined = CoreResources::RegionFromGsVideoMode(gsVideoMode)) {
           if (refined->libretro_region != g_detected_region
               || std::abs(refined->fps - g_detected_fps) > 0.05) {
               // Disagreement with our serial-based prediction — re-emit av_info.
               g_detected_region = refined->libretro_region;
               g_detected_fps    = refined->fps;
               retro_system_av_info av{};
               retro_get_system_av_info(&av);
               if (g_frontend.environ_cb)
                   g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
               FrontendLog(RETRO_LOG_INFO,
                   "[SP7a] region refined to %s fps=%.2f from gsVideoMode",
                   g_detected_region == RETRO_REGION_PAL ? "PAL" : "NTSC",
                   g_detected_fps);
           }
           g_region_refined = true;  // gate even if no disagreement
       }
       // else gsVideoMode still Uninitialized — try again next frame
   }
   ```

The flag `g_region_refined` ensures we never re-emit `SET_SYSTEM_AV_INFO`
twice. It is reset to `false` in `retro_load_game` (alongside the static
defaults) so a subsequent load via the singleton-EmuThread reuse path
(SP6.5 Task 4.8 territory) re-detects fresh.

## Data flow

```
retro_load_game
   |
   v
EmuThread::Start --> VMManager::Initialize
   (waits init_done)        |
   v                        v
   |                  s_disc_serial set
   |                  (gsVideoMode = Uninitialized)
   |
   v
VMManager::GetDiscSerial() ----> DetectRegionFromSerial
   |                                   |
   v                                   v
g_detected_region / g_detected_fps cached
   |
   v
retro_load_game returns true
   |
   v (frontend may now call:)
retro_get_region        --> returns g_detected_region
retro_get_system_av_info -> writes g_detected_fps
   |
   v
retro_run (called every host vsync)
   |
   v
if (!g_region_refined && gsVideoMode != Uninitialized)
   |                                       |
   |                              if disagreement, re-emit
   v                              SET_SYSTEM_AV_INFO,
EmuThread runs the EE                update statics
   |
   v
... game executes SetGsCrt, gsVideoMode flips to NTSC/PAL/etc ...
```

## Error handling

- **dladdr fails / dli_fname null:** RETRO_LOG_ERROR, return empty string.
  Settings.cpp then assigns empty to `EmuFolders::Resources`, GS init logs
  the same clear "Failed to create ImGui font texture" / Metal-lib load
  failure today's hardcoded path would produce if the path were wrong.
  Acceptable — failure is loud, not silent.
- **Resources dir missing on disk:** RETRO_LOG_ERROR with the resolved
  path so the install fix is one log line away. We still return the path
  (don't fall back to anything else) — there's no other plausible location.
- **GameDatabase::findGame returns null:** fall through to prefix
  heuristic. No log here (common for homebrew / non-retail dumps).
- **Prefix heuristic doesn't match:** WARN log + NTSC default. Refinement
  step will correct PAL homebrew once the game runs.
- **gsVideoMode stuck at Uninitialized for many frames:** harmless — the
  serial-based guess stands. `g_region_refined` stays false; we keep
  checking every frame. Cost is one branch per `retro_run`.

## Testing / smoke gates

Live-run criteria the plan's final step must verify:

1. **R&C 2 (NTSC, `SCUS_972.68`)**: boot end-to-end shows
   - `[SP7a] Resources dir = .../pcsx2_libretro_resources` log line
   - `[SP7a] region=NTSC fps=59.94 from serial 'SCUS-97268'` log line
   - Game renders, audio plays, achievements unlock — fully equivalent
     to today's behavior. SET_SYSTEM_AV_INFO refinement either does not
     fire (no disagreement) OR fires once with matching values.

2. **A PAL disc** (user picks one from the shelf — any PAL retail PS2
   game): boot end-to-end shows
   - `[SP7a] region=PAL fps=50.00 from serial '<SLES/SCES_...>'` log line
   - Game runs at 50 Hz pacing as judged by the libretro host. Audio
     doesn't drift over a 2-minute play test (the regression signal if
     libretro is still applying 60 Hz timing).

3. **Install-location independence**: rename `pcsx2-libretro/` directory
   (or build artifact dir) such that dladdr returns a different parent.
   Re-copy the dylib + the `pcsx2_libretro_resources/` directory to the
   new RetroNest cores location. Boot R&C 2 — core finds resources via
   dladdr, no hardcoded path leaks through. (Regression class that broke
   us at SP6 commit `9f4fb2679`.)

4. **Negative resources path**: temporarily move
   `~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/`
   aside. Boot fails with the RETRO_LOG_ERROR line clearly naming the
   missing path. Restore — boot succeeds. (Verifies the diagnostic is
   actionable.)

5. **mGBA cold-resume / launch unchanged** — SP7a doesn't touch RetroNest
   or anything mGBA reaches.

## Memory update on completion

After SP7a ships, `project_pcsx2_libretro_port.md` needs:
- SP7's "next focus" line narrowed to SP7b only (core-options migration).
- A new bullet under SP7a marked DONE with commit hashes.
- A correction note that the original SP7 description called this "INI
  patching" — clarify it's MemorySettingsInterface SetStringValue calls,
  not INI-string emission.

## Decisions deferred to plan execution

- **No new build script for the rsync.** No build helper exists under
  `pcsx2-libretro/` today, and SP7a's scope explicitly stays inside
  `pcsx2-libretro/`. The rsync line is documented in this spec and
  added to the monthly-rebase block in the project memory; the plan's
  final smoke step runs it manually. Adding a packaged dev script is
  SP7b/SP8 territory.
- **SDTV_576P classified as PAL** in `RegionFromGsVideoMode`, per the
  mapping table above. Rationale: retail PS2 discs that select 576P
  are always PAL-territory (Europe/Australia); homebrew that runs 576P
  at 60 Hz is rare enough that a single-cycle re-emit cost on
  misclassification is acceptable.
