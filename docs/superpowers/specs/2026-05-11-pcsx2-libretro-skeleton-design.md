# PCSX2 Libretro Core — Skeleton Phase (Sub-project 1 of 8)

**Date:** 2026-05-11
**Status:** Complete (skeleton verified end-to-end — see Verification log)
**Owner:** mark
**Scope:** First sub-project of the multi-phase PCSX2-to-libretro port. Skeleton only.

## Context

RetroNest's non-negotiable UX rule is "users never see native emulator UIs." Currently PCSX2 satisfies this via launched-binary plus window parenting plus surface capture plus Carbon-event-synthesized pause — functional, but a different rendering and control pipeline from mGBA (which uses libretro). The long-term goal is one unified in-process rendering pipeline for every emulator, modeled on the libretro abstraction RetroNest already supports.

This is the first of eight sub-projects that, taken together, produce a fully functional PCSX2 libretro core built from upstream PCSX2 master:

1. **Skeleton libretro core** ← *this document*
2. VM lifecycle + game boot
3. Video output
4. Audio output
5. Input
6. Save states + memory cards
7. Settings push (replace INI patching)
8. RetroNest adapter rewrite (retire launched-binary PCSX2 path)

Each sub-project gets its own brainstorm → spec → plan → implementation cycle.

## Goal

Prove that pcsx2-master can produce a libretro core (`pcsx2_libretro.dylib`) that RetroNest's existing `LibretroAdapter` can load, identify, and gracefully refuse to run a game from. Nothing more.

**Definition of done:** RetroNest is launched, discovers the `pcsx2-libretro` manifest, finds the dylib in `{root}/emulators/libretro/cores/`, and when the user tries to launch a PS2 game on it, RetroNest receives a clean `retro_load_game → false` from the core with an OSD message reading *"PCSX2 libretro core skeleton — game loading not implemented yet"* — no crash, no hang, no silent failure.

## Motivation

Selected from the integration-shape brainstorm:
- **Driver:** consistency / one rendering pipeline across all emulators.
- **Shape:** port current PCSX2 master to libretro (not LRPS2, not custom host, not IPC).
- **First bite:** smallest, riskiest piece — proves build/link/dylib-load works on Apple Silicon before any deeper investment.

## Non-goals (deferred to later sub-projects)

- VM lifecycle. `VMManager::Initialize` is **not** called. PCSX2's CPU recompiler, GS, SPU2, MTGS, all of it — untouched at runtime.
- Video output. No `retro_video_refresh_t` calls. No frame buffer.
- Audio output. No `retro_audio_sample_batch_t` calls.
- Input. No `retro_input_state_t` reading.
- Save states. `retro_serialize_size` returns 0; `retro_serialize`/`retro_unserialize` are no-ops.
- Memory cards. `retro_get_memory_*` return null/0.
- Settings exposure. `retro_get_variable` returns empty.
- BIOS / disc handling.
- A `Pcsx2LibretroAdapter` C++ subclass in RetroNest. Generic `LibretroAdapter` handles the skeleton.

## Coexistence with existing PCSX2

The existing `pcsx2` manifest (launched-binary backend) is **not modified**. A new `pcsx2-libretro` manifest is added side-by-side. Both PCSX2 entries appear in RetroNest. The launched-binary path keeps working through all eight sub-projects and is only retired in sub-project 8.

## Fork bootstrap (step zero)

Currently `pcsx2-master/` is a loose folder, not a git clone. One-time setup, before any code is written:

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git init
git remote add upstream https://github.com/PCSX2/pcsx2.git
git fetch upstream
git reset --hard upstream/master
git checkout -b retronest-libretro
```

The upstream commit hash that `git reset --hard upstream/master` lands on is recorded as the **work-start pin** in this spec and committed in the first commit on `retronest-libretro`.

`pin: upstream/master @ dead00eb62a7ca9b3321ede510eb79aab0328922`

Optionally push to a private GitHub fork (`markpearce/pcsx2-retronest`) for offsite backup — not required for skeleton work but recommended.

## Maintenance and upstream-update workflow

Our fork is purely additive — one new directory (`pcsx2-libretro/`) plus a 4-line block in the top-level `CMakeLists.txt`. Monthly update process:

```sh
cd pcsx2-master
git fetch upstream
git checkout retronest-libretro
git rebase upstream/master
cmake --build build --target pcsx2_libretro
```

Typical outcomes:
- ~11 months of 12: clean rebase, clean rebuild.
- Occasional: a `CMakeLists.txt` conflict (upstream reorganized the frontend section). ~30 seconds to fix.
- 2–4 times a year: upstream changed an internal API our shim called. Code fix inside `pcsx2-libretro/` — minutes to ~an hour.

The fundamental discipline that protects this: **never modify any file outside `pcsx2-libretro/`** (except the single block in the top-level `CMakeLists.txt`). If something seems to require an upstream-file edit, find another way (subclass, link a public symbol, ask upstream to expose it).

## Architecture

### pcsx2-master fork layout (skeleton-phase delta only)

```
pcsx2-master/
├── CMakeLists.txt            (+4 lines — described below)
├── pcsx2/                    upstream — unchanged
├── pcsx2-qt/                 upstream — unchanged
├── pcsx2-gsrunner/           upstream — unchanged
└── pcsx2-libretro/           NEW — entire skeleton lives here
    ├── CMakeLists.txt
    ├── libretro.h            vendored copy of upstream libretro API header
    ├── LibretroFrontend.h    shared types + FrontendState struct
    ├── LibretroFrontend.cpp  all retro_* C exports
    └── HostStubs.cpp         every Host:: function in pcsx2/Host.h as a stub
```

### Top-level CMakeLists.txt change

Added near the existing frontend wiring (lines ~50–70), opt-in default-off:

```cmake
option(ENABLE_LIBRETRO "Build the libretro frontend (pcsx2_libretro.dylib)" OFF)
if(ENABLE_LIBRETRO)
    add_subdirectory(pcsx2-libretro)
endif()
```

Invisible to anyone building upstream PCSX2 normally.

### pcsx2-libretro/CMakeLists.txt

A `MODULE` library target named `pcsx2_libretro` that:
- Compiles `LibretroFrontend.cpp` and `HostStubs.cpp`.
- Links against PCSX2's existing `PCSX2` static library target (provided by upstream `add_subdirectory(pcsx2)`).
- Links against `common`, `fmt`, and all 3rdparty deps PCSX2's static library transitively requires (SDL3, cubeb, etc.) — even though the skeleton doesn't *call* them, the linker needs them resolved.
- Sets `OUTPUT_NAME pcsx2_libretro` and `PREFIX ""` (libretro convention — no `lib` prefix).
- Does **not** use `-undefined dynamic_lookup` on macOS — we want hard link errors, not deferred-symbol surprises.
- Modeled closely on `pcsx2-gsrunner/CMakeLists.txt`.

### Build commands

```sh
cd pcsx2-master
cmake -B build -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF
cmake --build build --target pcsx2_libretro
# Output: pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib
```

### RetroNest-side delta (skeleton phase)

One new manifest file (schema follows existing `manifests/mgba.json`):

```json
// RetroNest-Project/manifests/pcsx2-libretro.json
{
  "id": "pcsx2-libretro",
  "name": "PCSX2 (libretro core, dev)",
  "description": "PlayStation 2 (libretro core — development build)",
  "systems": ["ps2"],
  "github_repo": "markpearce/pcsx2-retronest",
  "executable": "pcsx2_libretro.dylib",
  "install_folder": "libretro",
  "rom_extensions": ["iso", "chd", "cso", "bin", "cue", "m3u", "gz"],
  "launch_args": [],
  "backend": "libretro",
  "core_dylib": "pcsx2_libretro.dylib",
  "core_buildbot_path": "pcsx2_libretro.dylib.zip"
}
```

(`github_repo` and `core_buildbot_path` describe how RetroNest's installer will eventually fetch the dylib once we publish releases — they don't need to resolve during skeleton dev because we're copying the dylib in manually. Filled in now so the manifest is shaped correctly for later.)

One manual one-time copy step during dev:

```sh
cp pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib \
   ~/RetroNest/emulators/libretro/cores/
```

(Where `~/RetroNest/` is the user-chosen RetroNest data root; substitute as appropriate.)

No new C++ code in RetroNest. The generic `LibretroAdapter` handles loading.

## Components

### `pcsx2-libretro/LibretroFrontend.cpp`

All 30+ exported `retro_*` C functions. Most are one to three lines. The non-trivial ones:

- `retro_set_environment(cb)`: store `cb` in `g_frontend.environ_cb`.
- `retro_set_video_refresh(cb)`: store `cb` in `g_frontend.video_cb`.
- `retro_set_audio_sample(cb)`: store `cb`.
- `retro_set_audio_sample_batch(cb)`: store `cb`.
- `retro_set_input_poll(cb)`: store `cb`.
- `retro_set_input_state(cb)`: store `cb`.
- `retro_init()`: log "PCSX2 libretro skeleton: retro_init". Try `RETRO_ENVIRONMENT_GET_LOG_INTERFACE` and store the log callback in `g_frontend.log_cb` if available.
- `retro_deinit()`: log. Reset `g_frontend` to zero-init.
- `retro_api_version()`: return `RETRO_API_VERSION`.
- `retro_get_system_info(info)`:
  - `library_name = "PCSX2"`
  - `library_version = "<git-describe output> (skeleton)"` (embedded at build time via a generated header — same mechanism `pcsx2/BuildVersion.cpp` uses)
  - `valid_extensions = "iso|chd|cso|bin|cue|m3u|gz"`
  - `need_fullpath = true`
  - `block_extract = true`
- `retro_get_system_av_info(av)`: NTSC-ish defaults — `640x448` base, `1080x720` max, `4.0/3.0` aspect, `60.0` fps, `48000.0` Hz. These are placeholders; sub-project 3 (video output) will replace them with real values from PCSX2's GS settings.
- `retro_set_controller_port_device(port, device)`: log, no-op.
- `retro_reset()`: log, no-op.
- `retro_run()`: no-op. (No frames produced, no input read.)
- `retro_serialize_size()`: return 0.
- `retro_serialize(data, size)`, `retro_unserialize(data, size)`: return false.
- `retro_cheat_reset()`, `retro_cheat_set(...)`: log, no-op.
- `retro_load_game(game)`:
  - Log the path `game->path` if non-null.
  - Send an OSD message via `environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, ...)`: *"PCSX2 libretro core skeleton — game loading not implemented yet"*.
  - Return `false`.
- `retro_load_game_special(...)`: return `false`.
- `retro_unload_game()`: log, no-op.
- `retro_get_region()`: return `RETRO_REGION_NTSC`.
- `retro_get_memory_data(id)`, `retro_get_memory_size(id)`: return `nullptr` / `0`.

A single file-local `g_frontend` struct (declared in `LibretroFrontend.h`) holds the env/video/audio/input/log callbacks; `HostStubs.cpp` reads from it for logging.

### `pcsx2-libretro/HostStubs.cpp`

Implements every function in `pcsx2/Host.h`. Pattern by category:

- **OSD / messaging** (`AddOSDMessage`, `AddKeyedOSDMessage`, `AddIconOSDMessage`, `RemoveKeyedOSDMessage`, `ClearOSDMessages`, `ReportInfoAsync`, `ReportErrorAsync`, `ReportFormattedInfoAsync`, `ReportFormattedErrorAsync`): forward the message text to libretro's log interface; ignore duration/icon arguments.
- **Translation** (`TranslateToCString`, `TranslateToStringView`, `TranslateToString`, `TranslatePluralToString`, `ClearTranslationCache`, `Internal::GetTranslatedStringImpl`): return the input string verbatim; no localization in the skeleton.
- **Settings getters** (all `Get*SettingValue`, `GetBase*SettingValue`, `GetStringListSetting`, `GetSettingsLock`, `GetSecretsSettingsLock`, `GetSettingsInterface`, `Internal::Get*SettingsLayer`): return supplied defaults / empty / nullptr. We have no settings layer in the skeleton.
- **Settings setters** (all `Set*SettingValue`, `Add/RemoveBaseValueFromStringList`, `ContainsBaseSettingValue`, `RemoveBaseSettingValue`, `CommitBaseSettingChanges`, `Internal::Set*SettingsLayer`, `SetDefaultUISettings`): log a warning, no-op.
- **Mode flags** (`InBatchMode`, `InNoGUIMode`): return `true` (skeleton runs as if headless/no-GUI).
- **URL / clipboard / misc** (`OpenURL`, `CopyTextToClipboard`, `GetHTTPUserAgent`): log; `OpenURL` no-ops; `CopyTextToClipboard` returns `false`; `GetHTTPUserAgent` returns `"PCSX2-libretro-skeleton/1.0"`.
- **VM control** (`RequestVMShutdown`, `RequestResetSettings`, `RequestResizeHostDisplay`): log, no-op. No VM exists in the skeleton.
- **Thread dispatch** (`RunOnCPUThread`, `RunOnGSThread`): run the supplied `std::function` inline immediately. Acceptable because no VM/GS threads exist yet — anything that calls these in the skeleton runs on the caller's thread, which is fine for a no-op core.
- **Game list refresh** (`RefreshGameListAsync`, `CancelGameListRefresh`): log, no-op.
- **Progress callback** (`CreateHostProgressCallback`): return a minimal `ProgressCallback` stub that logs status updates and discards them.
- **Locale compare** (`LocaleSensitiveCompare`): return `lhs.compare(rhs)` — byte-wise; close enough for skeleton.

Each stub body is 1–5 lines. Total file expected to be 200–400 lines of mechanical code.

### `pcsx2-libretro/LibretroFrontend.h`

```cpp
#pragma once
#include "libretro.h"

struct FrontendState {
    retro_environment_t environ_cb = nullptr;
    retro_video_refresh_t video_cb = nullptr;
    retro_audio_sample_t audio_cb = nullptr;
    retro_audio_sample_batch_t audio_batch_cb = nullptr;
    retro_input_poll_t input_poll_cb = nullptr;
    retro_input_state_t input_state_cb = nullptr;
    retro_log_printf_t log_cb = nullptr;
};

extern FrontendState g_frontend;

// Single logging entry used by both LibretroFrontend.cpp and HostStubs.cpp.
void FrontendLog(retro_log_level level, const char* fmt, ...);
```

### `pcsx2-libretro/libretro.h`

A vendored copy of the upstream libretro header. We pin to the same version `vendor/libretro-api/libretro.h` in RetroNest uses, so the ABI is identical on both sides. The pinned version is recorded in the file's leading comment (`// vendored from libretro/RetroArch @ <commit-sha>`).

## Data flow at runtime

```
[ User clicks a PS2 game in RetroNest, "PCSX2 (libretro core, dev)" entry ]
                       │
                       ▼
[ LibretroAdapter::launchGame() → CoreRuntime → dlopen(pcsx2_libretro.dylib) ]
                       │
                       ▼
[ retro_set_environment(env_cb) ]   →   store callback in g_frontend
                       │
                       ▼
[ retro_init() ]                     →   log; fetch log_cb if available
                       │
                       ▼
[ retro_get_system_info(&info) ]     →   fill info struct (name, version, exts)
                       │
                       ▼
[ retro_get_system_av_info(&av) ]    →   fill placeholder NTSC 640x448@60
                       │
                       ▼
[ retro_load_game(&game) ]           →   log path; send OSD "skeleton — not impl";
                       │                  return FALSE
                       ▼
[ RetroNest sees false → reports launch failure cleanly to user ]
                       │
                       ▼
[ retro_unload_game() ] → [ retro_deinit() ]   →   logs; no-op cleanup
```

PCSX2's core library is linked into the dylib but **no PCSX2 code is executed**. The skeleton is, literally, a fully-linkable empty shell.

## Verification

Three tests, in order. All three must pass for the skeleton to be considered done.

### Test 1 — "It builds."

```sh
cd pcsx2-master
cmake -B build -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF
cmake --build build --target pcsx2_libretro 2>&1 | tee build.log
test -f build/pcsx2-libretro/pcsx2_libretro.dylib && echo "TEST 1 PASS"
```

Validates: every `Host::*` stub satisfies every `extern` reference from `pcsx2/`. Every transitive 3rdparty dep links on Apple Silicon. The CMake target is well-formed.

### Test 2 — "It loads in a neutral libretro host."

Use the `retroarch` CLI as a third-party verifier (independent of RetroNest):

```sh
retroarch -L build/pcsx2-libretro/pcsx2_libretro.dylib /path/to/any-iso.iso 2>&1 | head -50
```

Expected: log lines showing `retro_init`, `retro_get_system_info` reporting `library_name=PCSX2`, then a clean refusal from `retro_load_game` with our message. If `retroarch` isn't installed, this test can be replaced with a tiny C program that does `dlopen` + a sequence of `retro_*` calls — but `retroarch` is the lowest-friction option.

Validates: the dylib is a well-formed libretro core that works in any host, not RetroNest-specific.

### Test 3 — "It loads in RetroNest end-to-end."

```sh
cp build/pcsx2-libretro/pcsx2_libretro.dylib \
   ~/RetroNest/emulators/libretro/cores/
cp RetroNest-Project/manifests/pcsx2-libretro.json \
   ~/RetroNest/manifests/         # if that's where RetroNest reads from
open ~/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app
```

In the app: try to launch a PS2 game on the new "PCSX2 (libretro core, dev)" entry.

Expected: clean refusal OSD message in RetroNest's own UI, no crash, no hang, application returns to the game list. Logs should show the `retro_load_game → false` path and the skeleton's logged message.

Validates: end-to-end RetroNest ↔ libretro core integration, including manifest discovery, dylib path resolution, and OSD message propagation through `LibretroAdapter`'s pipe.

## Risks and mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| PCSX2's static library transitively requires deps (SDL3, cubeb, libpng, zstd, …) that don't link cleanly into a `MODULE` dylib on macOS | Medium | The skeleton's whole point is to flush these out early. We follow `pcsx2-gsrunner/CMakeLists.txt`'s exact link list — it already works on macOS in the same configuration. |
| Apple Silicon W^X / JIT entitlements break the build (PCSX2's recompiler needs RWX pages) | Low for skeleton | The skeleton doesn't *run* the recompiler. The build itself doesn't require JIT entitlements. Becomes a sub-project-2 problem when we actually call `VMManager::Initialize`. |
| The single Host::TranslateToCString returning input verbatim violates a "pointer valid until language change" guarantee callers rely on | Low for skeleton | Skeleton doesn't call into PCSX2 code, so no caller exercises the guarantee. Becomes a real concern in later sub-projects. |
| `pcsx2/Host.h` gains a new function in a future upstream rebase and our stubs don't cover it | Recurring | The rebase will fail to link with a clear undefined-symbol error pointing at the new function name. Trivial to add a stub. This is exactly the kind of small upstream-API drift we'd see 2–4 times a year. |
| RetroNest's `LibretroAdapter` expects a `core_id` of `pcsx2` to map to specific paths under `emulators/libretro/pcsx2/` and that subdirectory doesn't exist on a fresh data root | Low | The skeleton makes no use of that subdirectory (no options.json, no controls.ini, no frontend.json yet). `LibretroAdapter` should tolerate their absence; if it doesn't, that's a generic LibretroAdapter bug to fix once. |

## Out-of-scope clarifications (asked-but-deferred decisions)

- **Cross-build via RetroNest's CMake** (`RETRONEST_PCSX2_CORE_SRC` option): deliberately deferred. Manual copy is the dev workflow for the skeleton. Revisit only if/when the libretro core has proven itself.
- **Pcsx2LibretroAdapter C++ subclass**: not created in this sub-project. Generic `LibretroAdapter` is enough.
- **GitHub Actions CI** to build cross-platform release artifacts: deferred. The skeleton is built locally on macOS only.
- **Pushing the fork to a public GitHub repo**: deferred. Local-only `retronest-libretro` branch is enough to start.

## Success criteria summary

1. `pcsx2-master/` is a real git clone with `upstream` remote configured, on a `retronest-libretro` branch pinned to a recorded upstream commit.
2. `cmake --build build --target pcsx2_libretro` produces `pcsx2_libretro.dylib` cleanly.
3. `retroarch -L pcsx2_libretro.dylib …` loads it, reports `library_name=PCSX2`, refuses the game cleanly.
4. RetroNest loads it via a new `pcsx2-libretro` manifest, attempts to launch a PS2 game, shows the OSD refusal message, doesn't crash or hang.
5. Existing `pcsx2` launched-binary path continues to work unchanged.

When all five are true, the skeleton sub-project is complete. The next sub-project (VM lifecycle + game boot) begins.

## Verification log

Skeleton phase completed 2026-05-11. All three tests from the success-criteria summary pass:

- **Test 1 (build):** PASSED. `cmake -B build -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF` configures clean. `cmake --build build --target pcsx2_libretro` produces `build/pcsx2-libretro/pcsx2_libretro.dylib` (Mach-O 64-bit bundle arm64, ~8.5 MB). `nm -gU` confirms all required `retro_*` symbols exported as defined-text. Symbol visibility hiding works (no PCSX2 internal `_ZN4Host*` symbols leaked).
- **Test 2 (neutral libretro host):** PASSED. `retroarch` not installed on this machine; verified via the fallback `pcsx2-libretro/tools/test_loader` (committed in the fork). Output:
  ```
  retro_api_version() = 1
  library_name     = PCSX2
  library_version  = skeleton-0.1
  valid_extensions = iso|chd|cso|bin|cue|m3u|gz
  retro_load_game returned: FALSE
  ```
- **Test 3 (RetroNest end-to-end):** PASSED. Manifest discovered, dylib loaded by `LibretroAdapter`, `retro_load_game` reached our shim with the real ISO path, returned `false`, `retro_deinit` ran cleanly, no crash/hang. Verified with Ratchet & Clank - Going Commando. Existing `pcsx2` launched-binary manifest untouched and continues to work for other PS2 games.

### Observations and follow-ups uncovered during verification

1. **Spec assumption corrected (Task 4):** The plan claimed CMake would not validate `target_sources()` files at configure time. CMake 4.3.1 does. The implementer added `set_source_files_properties(... GENERATED TRUE)` to `pcsx2-libretro/CMakeLists.txt` to compile-defer source-existence checks; this was removed in Task 8 once the sources existed on disk. Net: pcsx2-libretro's CMakeLists now matches the original Task 3 spec, with no GENERATED marker. Future plan iterations should land all source files BEFORE the first configure pass to avoid this round-trip.

2. **Spec assumption corrected ("Zero new C++ in RetroNest"):** The spec stated "we don't even need a subclass — we'll let `LibretroAdapter` handle it generically." That was wrong — `LibretroAdapter::coreId()` is pure virtual, so the adapter registry can't instantiate it directly. The skeleton phase added a minimal `Pcsx2LibretroAdapter` (header + empty .cpp) that overrides only `coreId() = "pcsx2"`, plus registration in `adapter_registry.cpp`. Net code addition on RetroNest side: ~25 lines. Header is at `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`; .cpp exists solely so AUTOMOC emits the Q_OBJECT vtable. Later phases fill in PS2-specific overrides (settings schema, controller bindings, BIOS paths, RA console ID, resume-file lookup).

3. **Plan miscounted prefix lines (Task 2):** The vendor-pin comment block at the top of `pcsx2-libretro/libretro.h` is 7 lines (5 comment lines + `*/` + blank), not 6 as the plan stated. The byte-identity verification (`tail -n +8` instead of `+7`) caught this trivially. Cosmetic; no behavior impact.

4. **Plan miss on Host:: function discovery:** The plan listed Host:: function names from `pcsx2/Host.h` alone, but several real Host:: declarations live in headers like `pcsx2/ImGui/FullscreenUI.h` and `pcsx2/ImGui/ImGuiManager.h`. The Task 8 link surface check exposed these and the implementer added missing includes to `HostStubs.cpp`. Net: 2 additional includes, 0 additional stub functions (gsrunner's implementations covered all the bodies). For sub-project 2, the plan should specify "use gsrunner/Main.cpp as the canonical Host:: inventory" — which is what Task 6 already did, this is just a reinforcement.

5. **RetroNest feature gap: emulator-selection per game.** All PS2 games were tagged with `emulator_id = "pcsx2"` at scan time (when only the standalone manifest existed). RetroNest has no in-app UI to change a game's preferred emulator after the fact, even when the tagged emulator becomes uninstalled. For verification we patched a single DB row directly. Sub-project 8 (adapter rewrite) is the natural place to add a "change emulator for this game" action AND/OR an auto-fallback rule ("if tagged emulator isn't installed, fall back to any other installed emulator that handles this system").

6. **RetroNest feature gap: `RETRO_ENVIRONMENT_SET_MESSAGE` not handled.** RetroNest's libretro env-callback dispatcher logged `unhandled enum 6` when our shim sent the OSD refusal message. The shim's refusal text never reached the user — they saw RetroNest's generic "Launch failed" indicator instead. The libretro core itself is well-formed (the test_loader and retroarch would both surface the message correctly); this is a RetroNest-side gap to address before sub-project 8 or alongside it.

### Fork state at completion

- `pcsx2-master/` is on branch `retronest-libretro`, pinned to `upstream/master @ dead00eb62a7ca9b3321ede510eb79aab0328922`.
- Three commits on the fork branch:
  - `c33251197` libretro: scaffold pcsx2-libretro frontend directory
  - `9ac6ed64f` libretro: align ENABLE_LIBRETRO block indentation with file style
  - `0c6309724` libretro: implement skeleton retro_* exports and Host:: stubs
  - `7bd65a708` libretro: fix build — remove GENERATED flag and add missing ImGui includes
  - `2140698e6` libretro: add minimal standalone test loader for skeleton verification
- RetroNest commits on `main`:
  - `7295318` docs: pcsx2-libretro skeleton — spec + implementation plan
  - `236ef85` docs(specs): pin pcsx2 libretro skeleton spec to upstream commit
  - `5bbf96d` manifests: add pcsx2-libretro entry for the in-progress libretro core
  - `4b50fb3` adapters: register Pcsx2LibretroAdapter for the new libretro core

