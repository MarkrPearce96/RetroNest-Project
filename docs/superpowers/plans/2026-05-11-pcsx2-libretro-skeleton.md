# PCSX2 Libretro Core — Skeleton Phase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce `pcsx2_libretro.dylib` from pcsx2-master that RetroNest's existing `LibretroAdapter` can load, identify as PCSX2, and gracefully refuse to run a game from — proving build + link + dylib-load works on Apple Silicon before any deeper investment.

**Architecture:** Add a new sibling frontend directory `pcsx2-libretro/` inside pcsx2-master, alongside the existing `pcsx2-qt/` and `pcsx2-gsrunner/` frontends. Our shim links against PCSX2's existing `PCSX2` static library target, exports the libretro C ABI from `LibretroFrontend.cpp`, and satisfies PCSX2's `Host::*` link surface from `HostStubs.cpp` (modeled directly on `pcsx2-gsrunner/Main.cpp`'s Host implementations). One additive block in the top-level `CMakeLists.txt` opt-in toggles the new target via `ENABLE_LIBRETRO`. Skeleton-phase behavior: `retro_init` and `retro_get_system_info` work; `retro_load_game` logs and returns `false`; `retro_run` is a no-op.

**Tech Stack:** C++17, CMake 3.16+, libretro C ABI (vendored header pinned to commit `2253a95fbba02c898c9e1f11bb9fe2d3c06e287e` from `RetroNest-Project/vendor/libretro-api/libretro.h`), upstream PCSX2 master.

**Spec:** [2026-05-11-pcsx2-libretro-skeleton-design.md](../specs/2026-05-11-pcsx2-libretro-skeleton-design.md)

**Conventions used in this plan:**
- `PCSX2_ROOT` = `/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master`
- `RETRONEST_ROOT` = `/Users/mark/Documents/Projects/RetroNest-Project`
- `RETRONEST_DATA_ROOT` = the user-chosen RetroNest data root, e.g. `~/RetroNest/` — substitute as appropriate when running commands.
- All `cd` commands quote the path because `PCSX2_ROOT` is inside a folder with a space in its name (`Pcsx2 Experiment /`).

**File structure (this entire phase):**

| File | Created or modified | Purpose |
|---|---|---|
| `${PCSX2_ROOT}/.git/` | created by `git init` | Make pcsx2-master a real git clone of upstream PCSX2. |
| `${PCSX2_ROOT}/CMakeLists.txt` | modified (4 lines added) | Wire `ENABLE_LIBRETRO` option + `add_subdirectory(pcsx2-libretro)`. |
| `${PCSX2_ROOT}/pcsx2-libretro/CMakeLists.txt` | created | Build `pcsx2_libretro` MODULE target. |
| `${PCSX2_ROOT}/pcsx2-libretro/libretro.h` | created | Vendored copy of libretro API header, pinned to RetroNest's version. |
| `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h` | created | `FrontendState` struct + `FrontendLog` helper declaration. |
| `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.cpp` | created | All `retro_*` C exports. |
| `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp` | created | Every `Host::*` function as a stub, copied from gsrunner's pattern. |
| `${RETRONEST_ROOT}/manifests/pcsx2-libretro.json` | created | RetroNest manifest entry for the new core. |

**Existing files referenced (not modified):**
- `${PCSX2_ROOT}/pcsx2-gsrunner/Main.cpp` — canonical reference for Host stubs; we copy and adapt.
- `${PCSX2_ROOT}/pcsx2-gsrunner/CMakeLists.txt` — reference for our CMakeLists structure (the 21-line minimal pattern).
- `${PCSX2_ROOT}/pcsx2/Host.h` and the rest of the `Host::*` declarations scattered through `pcsx2/` — the link surface we have to satisfy.
- `${RETRONEST_ROOT}/manifests/mgba.json` — reference manifest schema.
- `${RETRONEST_ROOT}/vendor/libretro-api/libretro.h` — source of our pinned libretro header.

**On TDD format in this plan:** Pure unit tests don't fit the skeleton's nature (it has no behavior to assert about — it's a fully-linkable empty shell). Instead each task ends with one or more **verification steps** that build/run/inspect the result, then a **commit step**. The three full integration tests from the spec map to Tasks 8, 9, and 11. Steps remain bite-sized.

---

## Task 1: Bootstrap pcsx2-master as a git fork

**Files:**
- Modify (init): `${PCSX2_ROOT}/.git/`

- [ ] **Step 1: Inspect what's currently in pcsx2-master before initializing git**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
ls -la | head -20
test -d .git && echo "ALREADY A GIT REPO" || echo "not a git repo yet"
```

Expected: `not a git repo yet`. If it says `ALREADY A GIT REPO`, stop and ask the user — the assumption from the spec was that this is a loose download.

- [ ] **Step 2: Initialize a git repo and add upstream**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git init
git remote add upstream https://github.com/PCSX2/pcsx2.git
git fetch upstream
```

Expected: `git fetch` downloads upstream PCSX2's history (will take a minute — repository is large). Final output should look like `* [new branch] master -> upstream/master` plus a list of tags.

- [ ] **Step 3: Record the upstream commit we're pinning to, then hard-reset to it**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
UPSTREAM_PIN=$(git rev-parse upstream/master)
echo "Pinning to upstream/master @ ${UPSTREAM_PIN}"
git reset --hard upstream/master
git status
```

Expected: `nothing to commit, working tree clean`. The `${UPSTREAM_PIN}` value will be embedded in the spec at the end of this task — write it down.

If `git status` shows untracked files or modifications, those are leftovers from the loose-folder state. Inspect them with `git status -s`. If they're files the user accidentally added (e.g. a build directory), delete them with `rm -rf` after the user confirms. Do not commit anything that isn't on upstream/master.

- [ ] **Step 4: Create the long-lived working branch**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git checkout -b retronest-libretro
git log --oneline -1
```

Expected: `git log` shows the most recent upstream commit on `retronest-libretro`. The branch name matches what's described in the spec.

- [ ] **Step 5: Record the pin in the spec**

Open `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-skeleton-design.md`.

Find the "Fork bootstrap (step zero)" section. Replace the placeholder text:
```
`pin: upstream/master @ <40-char-sha>`
```
with the actual commit hash recorded in Step 3:
```
`pin: upstream/master @ <the-actual-sha-here>`
```

- [ ] **Step 6: Commit the spec update**

Run from RetroNest's repo (the spec lives there, not in pcsx2-master):
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add docs/superpowers/specs/2026-05-11-pcsx2-libretro-skeleton-design.md
git status
```

Expected: only the spec file is staged. If anything else is staged, unstage it (`git restore --staged <file>`) — this commit is the spec pin update only.

Then commit:
```sh
git commit -m "$(cat <<'EOF'
docs(specs): pin pcsx2 libretro skeleton spec to upstream commit

Records the upstream PCSX2 master commit our retronest-libretro
branch is rebased onto as the work-start pin. Future rebases will
move forward from this baseline.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

(The pcsx2-master repo itself has no new commits yet — the bootstrap is just clone + branch. The first commit on `retronest-libretro` will be Task 4's CMakeLists changes.)

---

## Task 2: Vendor the libretro API header

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/libretro.h`

- [ ] **Step 1: Create the pcsx2-libretro directory**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
mkdir pcsx2-libretro
```

Expected: directory created. No output.

- [ ] **Step 2: Copy the vendored libretro.h from RetroNest into pcsx2-libretro/**

Run:
```sh
cp "/Users/mark/Documents/Projects/RetroNest-Project/vendor/libretro-api/libretro.h" \
   "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/libretro.h"
```

This ensures both ends of the ABI use the *exact same* libretro.h. Drift between RetroNest's libretro.h and the core's libretro.h is a class of bug we never want to debug.

- [ ] **Step 3: Add a leading comment recording the vendor pin**

Edit `${PCSX2_ROOT}/pcsx2-libretro/libretro.h`. At the very top of the file (above the existing `/*!` block), insert:

```c
/* Vendored from RetroNest's vendor/libretro-api/libretro.h.
 * RetroNest pin: commit 2253a95fbba02c898c9e1f11bb9fe2d3c06e287e (2026-05-06).
 * Keep in sync with RetroNest's copy. The two files MUST stay byte-identical
 * after stripping this header — otherwise the ABI can drift between host
 * (RetroNest) and core (pcsx2_libretro.dylib) without a build error.
 */

```

(Note the trailing blank line.)

- [ ] **Step 4: Verify the file is byte-identical to RetroNest's after stripping our header**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro"
tail -n +7 libretro.h | diff - "/Users/mark/Documents/Projects/RetroNest-Project/vendor/libretro-api/libretro.h"
```

Expected: no output (no diff). If there's a diff, the header insertion in Step 3 was wrong size — count the lines of the prefix block including the trailing blank line and adjust the `tail -n +N` accordingly.

---

## Task 3: Create pcsx2-libretro/CMakeLists.txt

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/CMakeLists.txt`

- [ ] **Step 1: Write the CMakeLists for the MODULE target**

Create `${PCSX2_ROOT}/pcsx2-libretro/CMakeLists.txt` with this exact content:

```cmake
# pcsx2-libretro — libretro core frontend for PCSX2.
#
# Built only when -DENABLE_LIBRETRO=ON is passed. Off by default so this
# directory is invisible to anyone building upstream PCSX2 normally.
#
# Modeled on pcsx2-gsrunner/CMakeLists.txt — same minimal shape, but
# produces a MODULE library (loadable plugin) rather than an executable.

add_library(pcsx2_libretro MODULE)

target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
)

target_include_directories(pcsx2_libretro PRIVATE
    "${CMAKE_BINARY_DIR}/common/include"
    "${CMAKE_SOURCE_DIR}/pcsx2"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(pcsx2_libretro PRIVATE
    PCSX2_FLAGS
    PCSX2
)

# Libretro naming convention: pcsx2_libretro.dylib (no "lib" prefix).
set_target_properties(pcsx2_libretro PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pcsx2_libretro"
)

# Hide symbols by default — libretro cores only export the retro_* C ABI.
# The retro_* functions get explicit visibility from RETRO_API in libretro.h.
set_target_properties(pcsx2_libretro PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(PACKAGE_MODE)
    install(TARGETS pcsx2_libretro LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
    install(TARGETS pcsx2_libretro LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/bin)
endif()
```

- [ ] **Step 2: Sanity-check it parses (will still fail to build — sources don't exist yet)**

Skip the configure check here; Task 4 wires the option in and we'll configure at the end of Task 4. We can't usefully configure this directory in isolation.

---

## Task 4: Wire ENABLE_LIBRETRO into pcsx2-master's top-level CMakeLists.txt

**Files:**
- Modify: `${PCSX2_ROOT}/CMakeLists.txt` (insert ~4 lines)

- [ ] **Step 1: Read the existing frontend wiring section**

Run:
```sh
sed -n '40,75p' "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/CMakeLists.txt"
```

Expected: shows lines 40–75 ending with the `pcsx2-gsrunner` block. The exact line numbers may shift after upstream rebases, but the structure is stable: `add_subdirectory(common)` → `add_subdirectory(pcsx2)` → `ENABLE_QT_UI` block → updater block → tests block → `ENABLE_GSRUNNER` block.

- [ ] **Step 2: Add the ENABLE_LIBRETRO block immediately after the ENABLE_GSRUNNER block**

Open `${PCSX2_ROOT}/CMakeLists.txt`. Locate the existing block (around line 67–70):

```cmake
# gsrunner
if(ENABLE_GSRUNNER)
    add_subdirectory(pcsx2-gsrunner)
else()
    add_subdirectory(pcsx2-gsrunner EXCLUDE_FROM_ALL)
endif()
```

Insert these lines **directly after** that block, separated by a blank line:

```cmake

# libretro frontend (RetroNest)
option(ENABLE_LIBRETRO "Build the libretro frontend (pcsx2_libretro.dylib)" OFF)
if(ENABLE_LIBRETRO)
    add_subdirectory(pcsx2-libretro)
endif()
```

- [ ] **Step 3: Verify CMake configures with ENABLE_LIBRETRO=OFF (default — pcsx2-libretro is invisible)**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
cmake -B build-test-off -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl3)" 2>&1 | tail -20
```

Expected: configure succeeds. No mention of `pcsx2_libretro` or `pcsx2-libretro` in output (because the option defaulted to OFF).

If configure fails because of unrelated missing deps (e.g. SDL3 not installed), that's a pre-existing pcsx2-master build issue. Resolve those before continuing — `brew install sdl3 qt@6 libpng zstd cubeb` etc. as needed. The first time PCSX2 is configured on a machine it always needs a deps install pass; that's not our skeleton's bug.

- [ ] **Step 4: Verify CMake configures with ENABLE_LIBRETRO=ON (will succeed — sources will fail later, but configuration must succeed now)**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
rm -rf build-test-on
cmake -B build-test-on -DENABLE_LIBRETRO=ON \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl3)" 2>&1 | tail -20
```

Expected: configure succeeds. Output mentions `pcsx2-libretro` being processed. **It does not yet build** — sources `LibretroFrontend.cpp` and `HostStubs.cpp` don't exist. We're only checking that the CMake graph is well-formed.

If configure fails with `Cannot find source file: LibretroFrontend.cpp` — that's not a configure error, that'll surface during `cmake --build`. Configure should still succeed because CMake only checks the existence of source files at build time, not configure time. If configure fails for a different reason, fix it before continuing.

- [ ] **Step 5: Clean up the test build directories**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
rm -rf build-test-off build-test-on
```

- [ ] **Step 6: Commit the CMakeLists changes and the empty CMakeLists for the new directory**

Run in pcsx2-master (not RetroNest — this is the first commit on the fork branch):
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git status
```

Expected: shows `modified: CMakeLists.txt`, `new file: pcsx2-libretro/CMakeLists.txt`, `new file: pcsx2-libretro/libretro.h`. If anything else appears (e.g. a stray `build/` directory), exclude it.

Run:
```sh
git add CMakeLists.txt pcsx2-libretro/CMakeLists.txt pcsx2-libretro/libretro.h
git commit -m "$(cat <<'EOF'
libretro: scaffold pcsx2-libretro frontend directory

Adds the new pcsx2-libretro/ sibling alongside pcsx2-qt and
pcsx2-gsrunner, with:

- A MODULE library target named pcsx2_libretro that links PCSX2
  + PCSX2_FLAGS (same pattern as pcsx2-gsrunner).
- A vendored copy of libretro.h pinned to RetroNest's version
  to guarantee ABI alignment between host and core.
- An ENABLE_LIBRETRO option in the top-level CMakeLists.txt,
  default OFF — invisible to upstream PCSX2 builds.

No source files yet — Tasks 5-7 add LibretroFrontend.cpp and
HostStubs.cpp. Configure with -DENABLE_LIBRETRO=ON works;
build will fail until those sources land.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Create LibretroFrontend.h (shared state)

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h`

- [ ] **Step 1: Write the header**

Create `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h` with this exact content:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro frontend — shared state and logging helper.
// Declares the singleton FrontendState that holds libretro callbacks
// captured during retro_set_* calls. Both LibretroFrontend.cpp and
// HostStubs.cpp read from this state.

#pragma once

#include "libretro.h"

namespace Pcsx2Libretro
{

struct FrontendState
{
    retro_environment_t        environ_cb       = nullptr;
    retro_video_refresh_t      video_cb         = nullptr;
    retro_audio_sample_t       audio_cb         = nullptr;
    retro_audio_sample_batch_t audio_batch_cb   = nullptr;
    retro_input_poll_t         input_poll_cb    = nullptr;
    retro_input_state_t        input_state_cb   = nullptr;
    retro_log_printf_t         log_cb           = nullptr;
};

extern FrontendState g_frontend;

// Logging entry point used by HostStubs.cpp and LibretroFrontend.cpp.
// Routes through g_frontend.log_cb if available, else fprintf(stderr).
// Use it instead of printf/fprintf so log handling is consistent and
// libretro hosts can capture our log output.
void FrontendLog(retro_log_level level, const char* fmt, ...);

} // namespace Pcsx2Libretro
```

- [ ] **Step 2: Verify the file compiles in isolation (syntax check only)**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro"
clang++ -std=c++17 -fsyntax-only -I. LibretroFrontend.h
```

Expected: no output (clean syntax). If there's an error, it's a typo — fix and rerun.

---

## Task 6: Implement HostStubs.cpp (every Host:: function as a stub)

This is the largest task. The approach is to **copy gsrunner's Host implementations as the canonical base**, then adapt only what needs adapting (e.g. logging routes through `FrontendLog` instead of gsrunner's `std::fprintf`). Gsrunner is already a proven, linking, fully-stubbed reference for the same `PCSX2` static library we link against.

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp`

- [ ] **Step 1: Inventory the full Host:: function surface**

The `Host::` namespace is declared across multiple PCSX2 headers, not just `pcsx2/Host.h`. The complete set of `Host::*` functions a frontend must define is whatever gsrunner defines. Use gsrunner as the source of truth.

Run:
```sh
grep -nE "^\w[\w:<>*&\s,]+Host::[A-Za-z_]+\(" \
    "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-gsrunner/Main.cpp" \
    | head -80
```

Expected: a list of every `Host::FunctionName` defined in gsrunner. Approximately 50–70 functions. Save this list mentally — these are exactly the functions our HostStubs.cpp must define.

- [ ] **Step 2: Copy gsrunner's Host:: implementations into a working scratch buffer**

Open `${PCSX2_ROOT}/pcsx2-gsrunner/Main.cpp`. Identify every function whose name starts with `Host::`. Most are between lines ~150 and ~700, interspersed with the `GSRunner::` frontend logic.

Extract **only the `Host::*` function definitions** (skip everything else in Main.cpp). The transferred list will include functions like:
- `Host::CommitBaseSettingChanges`, `Host::LoadSettings`, `Host::CheckForSettingsChanges`, `Host::RequestResetSettings`, `Host::SetDefaultUISettings`, `Host::LocaleCircleConfirm`
- `Host::CreateHostProgressCallback`, `Host::ReportInfoAsync`, `Host::ReportErrorAsync`, `Host::ReportFormattedInfoAsync`, `Host::ReportFormattedErrorAsync`
- `Host::OpenURL`, `Host::CopyTextToClipboard`, `Host::BeginTextInput`, `Host::EndTextInput`, `Host::GetTopLevelWindowInfo`
- `Host::OnInputDeviceConnected`, `Host::OnInputDeviceDisconnected`, `Host::SetMouseMode`, `Host::SetMouseLock`
- `Host::AcquireRenderWindow`, `Host::ReleaseRenderWindow`, `Host::BeginPresentFrame`, `Host::RequestResizeHostDisplay`
- `Host::OnVMStarting`, `Host::OnVMStarted`, `Host::OnVMDestroyed`, `Host::OnVMPaused`, `Host::OnVMResumed`, `Host::OnGameChanged`, `Host::OnPerformanceMetricsUpdated`, `Host::OnSaveStateLoading`, `Host::OnSaveStateLoaded`, `Host::OnSaveStateSaved`
- `Host::AddOSDMessage`, `Host::AddKeyedOSDMessage`, `Host::AddIconOSDMessage`, `Host::RemoveKeyedOSDMessage`, `Host::ClearOSDMessages`
- `Host::TranslateToCString`, `Host::TranslateToStringView`, `Host::TranslateToString`, `Host::TranslatePluralToString`, `Host::ClearTranslationCache`
- All `Host::GetBase*SettingValue`, `Host::Get*SettingValue`, `Host::Set*SettingValue`, `Host::GetSettingsLock`, `Host::GetSecretsSettingsLock`, `Host::GetSettingsInterface`, `Host::*Internal::*SettingsLayer`
- `Host::InBatchMode`, `Host::InNoGUIMode`, `Host::GetHTTPUserAgent`, `Host::LocaleSensitiveCompare`
- `Host::RunOnCPUThread`, `Host::RunOnGSThread`
- `Host::RefreshGameListAsync`, `Host::CancelGameListRefresh`, `Host::RequestVMShutdown`
- `Host::Internal::GetTranslatedStringImpl`, `Host::Internal::GetBaseSettingsLayer`, `Host::Internal::GetSecretsSettingsLayer`, `Host::Internal::GetGameSettingsLayer`, `Host::Internal::GetInputSettingsLayer`, `Host::Internal::SetBaseSettingsLayer`, `Host::Internal::SetSecretsSettingsLayer`, `Host::Internal::SetGameSettingsLayer`, `Host::Internal::SetInputSettingsLayer`

(The exact list depends on upstream pcsx2-master at the pinned commit. Trust gsrunner's set as the authoritative inventory.)

- [ ] **Step 3: Create HostStubs.cpp with the standard preamble**

Create `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp` starting with:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro Host:: stubs.
//
// Implementations of every function in PCSX2's Host:: namespace, kept
// behavior-compatible with pcsx2-gsrunner's Main.cpp implementations
// where possible. The skeleton phase doesn't run any PCSX2 code that
// actually calls into these stubs, but every Host:: symbol must
// resolve at link time or pcsx2_libretro.dylib won't build.
//
// Maintenance note: when rebasing onto a newer upstream, if pcsx2/
// declares a new Host::* function, the build will fail with an
// undefined-symbol error pointing at the new name. Add a stub here
// modeled on the equivalent stub in pcsx2-gsrunner/Main.cpp.

#include "PrecompiledHeader.h"

#include "Host.h"
#include "LibretroFrontend.h"

#include "common/Console.h"
#include "common/Path.h"
#include "common/ProgressCallback.h"
#include "common/SettingsInterface.h"

#include "fmt/format.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <optional>

using namespace Pcsx2Libretro;
```

(The `#include "PrecompiledHeader.h"` line follows PCSX2's convention — every .cpp file in pcsx2/ and pcsx2-gsrunner/ uses this. The CMake setup pulls it in via `PCSX2_FLAGS`.)

- [ ] **Step 4: Append the `g_frontend` definition and `FrontendLog` implementation**

In the same file, append:

```cpp

// ----------------------------------------------------------------------------
// Frontend state singleton.
// ----------------------------------------------------------------------------

namespace Pcsx2Libretro
{
FrontendState g_frontend{};

void FrontendLog(retro_log_level level, const char* fmt, ...)
{
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (g_frontend.log_cb)
    {
        g_frontend.log_cb(level, "[pcsx2_libretro] %s\n", buffer);
    }
    else
    {
        std::fprintf(stderr, "[pcsx2_libretro] %s\n", buffer);
    }
}
} // namespace Pcsx2Libretro
```

- [ ] **Step 5: Append each Host:: stub, adapted from gsrunner**

For each Host:: function identified in Step 1, copy its gsrunner implementation into HostStubs.cpp, applying these mechanical edits:

1. **Replace `std::fprintf(stderr, …)` with `Pcsx2Libretro::FrontendLog(RETRO_LOG_INFO, …)`** for informational logs. Use `RETRO_LOG_ERROR` for error reports and `RETRO_LOG_WARN` for warnings.
2. **Remove any Qt-specific includes or types.** Gsrunner is already Qt-free, so this should be a no-op — but verify.
3. **Settings layer functions** (`Host::Internal::GetBaseSettingsLayer` and friends) — copy gsrunner's version verbatim if it returns `nullptr` / no-op. If gsrunner has a real implementation (e.g. it loads a settings file), replace with `nullptr` / no-op for the skeleton.
4. **`Host::CreateHostProgressCallback`** — copy verbatim; gsrunner returns a minimal `ProgressCallback` that logs to stderr, we want the same (logs go through `FrontendLog`).
5. **`Host::AcquireRenderWindow` / `Host::ReleaseRenderWindow` / `Host::BeginPresentFrame`** — these would matter for real rendering, but in the skeleton no VM runs and these are never called. Copy gsrunner's versions verbatim; if gsrunner has bespoke render-window logic, replace with `std::nullopt` return / no-op.
6. **`Host::RunOnCPUThread` / `Host::RunOnGSThread`** — for the skeleton, execute the function inline immediately on the caller's thread:
   ```cpp
   void Host::RunOnCPUThread(std::function<void()> function, bool block)
   {
       FrontendLog(RETRO_LOG_WARN, "RunOnCPUThread called in skeleton — running inline");
       if (function) function();
   }
   void Host::RunOnGSThread(std::function<void()> function)
   {
       FrontendLog(RETRO_LOG_WARN, "RunOnGSThread called in skeleton — running inline");
       if (function) function();
   }
   ```

For each function, the *body* should be one of:
- Return the supplied default (settings getters).
- Log + no-op (setters, notifications, VM lifecycle hooks).
- Return `false` / `nullptr` / `std::nullopt` / 0 (predicates and resource-returners).
- Verbatim copy from gsrunner (translation passthrough, progress callback, locale compare).

After Step 5 the file should be roughly 300–500 lines. If it's much shorter you missed Host:: functions; cross-check with the Step 1 inventory.

- [ ] **Step 6: Verify HostStubs.cpp compiles in isolation (syntax check)**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
clang++ -std=c++17 -fsyntax-only \
    -I pcsx2 -I pcsx2-libretro -I common -I 3rdparty/fmt/include \
    -I build-test-on/common/include \
    pcsx2-libretro/HostStubs.cpp 2>&1 | head -40
```

Expected: a small number of `cannot find header` warnings (the include path approximation here is incomplete — we don't have the full include list outside of CMake). What we're looking for is **no syntax errors in our actual code**. If clang complains about `unknown type name 'std::function'` or `expected ';' after function definition` — that's in our code, fix it. If clang complains about `cannot open source file "Pcsx2Defs.h"` — that's an include path issue we don't need to solve here; CMake will handle it.

If this step is too noisy to be useful, skip it — Task 8's `cmake --build` is the definitive check.

---

## Task 7: Implement LibretroFrontend.cpp (all retro_* C exports)

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.cpp`

- [ ] **Step 1: Write the preamble**

Create `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.cpp` starting with:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — exported retro_* C functions.
//
// Skeleton phase: enough to load, identify as PCSX2, and gracefully
// refuse retro_load_game. No VM is initialized; no PCSX2 code runs.

#include "LibretroFrontend.h"
#include "libretro.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace Pcsx2Libretro;

extern "C" {
```

(All retro_* functions go inside the `extern "C" { … }` block. Closing brace at end of file.)

- [ ] **Step 2: Implement the setters**

Append:

```cpp

RETRO_API void retro_set_environment(retro_environment_t cb)        { g_frontend.environ_cb     = cb; }
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)    { g_frontend.video_cb       = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)      { g_frontend.audio_cb       = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { g_frontend.audio_batch_cb = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb)          { g_frontend.input_poll_cb  = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb)        { g_frontend.input_state_cb = cb; }
```

- [ ] **Step 3: Implement retro_init / retro_deinit**

Append:

```cpp

RETRO_API void retro_init(void)
{
    // Try to obtain the libretro log interface for better log routing.
    retro_log_callback log_iface{};
    if (g_frontend.environ_cb &&
        g_frontend.environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_iface))
    {
        g_frontend.log_cb = log_iface.log;
    }
    FrontendLog(RETRO_LOG_INFO, "retro_init — PCSX2 libretro skeleton initialised");
}

RETRO_API void retro_deinit(void)
{
    FrontendLog(RETRO_LOG_INFO, "retro_deinit");
    g_frontend = FrontendState{};
}

RETRO_API unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}
```

- [ ] **Step 4: Implement retro_get_system_info and retro_get_system_av_info**

Append:

```cpp

RETRO_API void retro_get_system_info(struct retro_system_info* info)
{
    if (!info) return;
    std::memset(info, 0, sizeof(*info));
    info->library_name     = "PCSX2";
    info->library_version  = "skeleton-0.1";  // bumped manually until phase 2 hooks up BuildVersion.cpp
    info->valid_extensions = "iso|chd|cso|bin|cue|m3u|gz";
    info->need_fullpath    = true;
    info->block_extract    = true;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info)
{
    if (!info) return;
    std::memset(info, 0, sizeof(*info));
    info->geometry.base_width   = 640;
    info->geometry.base_height  = 448;
    info->geometry.max_width    = 1280;
    info->geometry.max_height   = 1024;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    info->timing.fps            = 60.0;       // placeholder — phase 3 will derive from GS region
    info->timing.sample_rate    = 48000.0;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
    FrontendLog(RETRO_LOG_INFO, "retro_set_controller_port_device(port=%u, device=%u) — ignored in skeleton",
                port, device);
}

RETRO_API void retro_reset(void)
{
    FrontendLog(RETRO_LOG_INFO, "retro_reset — no-op in skeleton");
}

RETRO_API void retro_run(void)
{
    // No-op. Skeleton produces no frames and no audio.
}

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool   retro_serialize(void*, size_t)         { return false; }
RETRO_API bool   retro_unserialize(const void*, size_t) { return false; }

RETRO_API void   retro_cheat_reset(void) {}
RETRO_API void   retro_cheat_set(unsigned, bool, const char*) {}
```

- [ ] **Step 5: Implement retro_load_game (the meaningful skeleton path)**

Append:

```cpp

RETRO_API bool retro_load_game(const struct retro_game_info* game)
{
    if (game && game->path)
        FrontendLog(RETRO_LOG_INFO, "retro_load_game called with path: %s", game->path);
    else
        FrontendLog(RETRO_LOG_INFO, "retro_load_game called with null game info");

    static const char* const refusal =
        "PCSX2 libretro core skeleton \xE2\x80\x94 game loading not implemented yet (phase 1)";

    // Surface the message in libretro hosts via SET_MESSAGE so it reaches
    // any frontend, not just RetroNest.
    if (g_frontend.environ_cb)
    {
        struct retro_message msg{};
        msg.msg    = refusal;
        msg.frames = 180; // ~3 seconds at 60fps
        g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
    }

    FrontendLog(RETRO_LOG_WARN, "%s", refusal);
    return false;
}

RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t)
{
    return false;
}

RETRO_API void retro_unload_game(void)
{
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game");
}

RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

RETRO_API void* retro_get_memory_data(unsigned) { return nullptr; }
RETRO_API size_t retro_get_memory_size(unsigned) { return 0; }
```

- [ ] **Step 6: Close the extern "C" block**

Append:

```cpp

} // extern "C"
```

That's the complete file.

- [ ] **Step 7: Verify it compiles in isolation (syntax check)**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro"
clang++ -std=c++17 -fsyntax-only -I. LibretroFrontend.cpp 2>&1 | head -20
```

Expected: no errors (it only depends on `libretro.h` and `LibretroFrontend.h`, both local). If anything fails, it's a typo in our code — fix it.

- [ ] **Step 8: Commit the libretro frontend sources**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git add pcsx2-libretro/LibretroFrontend.h pcsx2-libretro/LibretroFrontend.cpp pcsx2-libretro/HostStubs.cpp
git status
```

Expected: only the three new files are staged.

```sh
git commit -m "$(cat <<'EOF'
libretro: implement skeleton retro_* exports and Host:: stubs

Adds the libretro frontend's source files inside pcsx2-libretro/:

- LibretroFrontend.h declares the FrontendState singleton holding
  retro_* callbacks and the FrontendLog helper used by both .cpp
  files.

- LibretroFrontend.cpp implements every retro_* C ABI export.
  retro_init / retro_get_system_info / retro_get_system_av_info
  return sane PCSX2 values. retro_load_game logs and returns
  false with an OSD message. retro_run is a no-op. Save state,
  cheat, and memory APIs are explicit no-ops returning 0/false.

- HostStubs.cpp implements PCSX2's full Host:: namespace, modeled
  on pcsx2-gsrunner/Main.cpp's Host implementations. Logging
  routes through FrontendLog (libretro RETRO_LOG_* interface
  when available, fprintf(stderr) otherwise). RunOnCPUThread /
  RunOnGSThread execute inline because no VM/GS threads exist
  in the skeleton.

No VM is initialised in the skeleton — this commit is the
"fully-linkable empty shell" deliverable described in the
spec's success criteria.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Verification Test 1 — the dylib builds

This is the first of the three tests from the spec.

**Files:** none (verification only).

- [ ] **Step 1: Clean configure with ENABLE_LIBRETRO=ON**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
rm -rf build
cmake -B build -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl3)" 2>&1 | tee configure.log | tail -30
```

Expected: `-- Generating done` and `-- Build files have been written to: ...build`. Configure usually takes 30–90 seconds the first time.

If configure fails, inspect `configure.log` for the first error. Most common: missing system deps (`brew install <name>`). Fix and re-run.

- [ ] **Step 2: Build the pcsx2_libretro target**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
cmake --build build --target pcsx2_libretro -j 2>&1 | tee build.log | tail -40
```

Expected: green build, finishing with a line like:
```
[100%] Linking CXX shared module pcsx2-libretro/pcsx2_libretro.dylib
[100%] Built target pcsx2_libretro
```

The build will take **a long time** the first time (5–15 minutes, since it has to compile most of the PCSX2 core library — a few hundred MB of object files). Subsequent incremental builds touching only `pcsx2-libretro/` files take seconds.

- [ ] **Step 3: Verify the dylib actually exists and is loadable**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
ls -la build/pcsx2-libretro/pcsx2_libretro.dylib
file build/pcsx2-libretro/pcsx2_libretro.dylib
nm -gU build/pcsx2-libretro/pcsx2_libretro.dylib | grep -E "_retro_(init|deinit|run|get_system_info|load_game|api_version)$"
```

Expected:
- `ls`: a file is present, probably 50–200 MB.
- `file`: reports `Mach-O 64-bit dynamically linked shared library arm64` (or `x86_64` if you're on Intel).
- `nm`: lists at least these symbols as exported (`U` would be undefined, `T` defined-text):
  ```
  ...T _retro_api_version
  ...T _retro_deinit
  ...T _retro_get_system_info
  ...T _retro_init
  ...T _retro_load_game
  ...T _retro_run
  ```
  If any of these are missing or marked `U`, the export visibility is wrong — re-check the `RETRO_API` macro evaluates to the right `__attribute__((visibility("default")))` on macOS, and that `CXX_VISIBILITY_PRESET hidden` isn't hiding the retro_* symbols (it shouldn't, because RETRO_API explicitly sets default visibility).

- [ ] **Step 4: Common failure recovery — undefined Host:: symbol**

If link failed with errors like:
```
Undefined symbols for architecture arm64:
  "Host::SomeFunctionWeMissed(...)", referenced from:
      ...
```

Then we missed a Host:: stub. The fix:
1. Open `${PCSX2_ROOT}/pcsx2-gsrunner/Main.cpp`.
2. Search for `Host::SomeFunctionWeMissed`.
3. Copy that function's implementation into `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp` (adapting logging per Task 6 Step 5).
4. Re-run Step 2.

This may iterate 2–5 times even with a careful Task 6 — it's the dominant failure mode for the skeleton, and totally expected. Each missing symbol is a 30-second add. Commit the cumulative fixes once the link succeeds, separately from the original Task 7 commit if it makes the history cleaner.

- [ ] **Step 5: Clean up build logs and commit any HostStubs.cpp additions**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
echo "configure.log" >> .git/info/exclude
echo "build.log" >> .git/info/exclude
echo "build/" >> .git/info/exclude
```

If you added stubs during Step 4:
```sh
git add pcsx2-libretro/HostStubs.cpp
git commit -m "$(cat <<'EOF'
libretro: add Host:: stubs missed in initial scaffold

Linker surface check exposed N additional Host:: functions that
weren't in the initial gsrunner inventory pass. Each is stubbed
to match gsrunner's implementation pattern (log + default).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

(Replace `N` with the actual number you added.)

---

## Task 9: Verification Test 2 — loads in a neutral libretro host (retroarch)

**Files:** none (verification only).

- [ ] **Step 1: Verify retroarch is installed (or set up the fallback)**

Run:
```sh
which retroarch || echo "retroarch not installed"
```

If `retroarch` is installed, skip to Step 2.

If not, the fallback is a 30-line C program that does `dlopen` + a sequence of `retro_*` calls. Create `${PCSX2_ROOT}/pcsx2-libretro/tools/test_loader.c` (note the new `tools/` subdirectory):

```c
// Standalone libretro core loader for skeleton verification.
// Not built as part of pcsx2_libretro target — manual compile.
//
//   clang test_loader.c -o test_loader
//   ./test_loader path/to/pcsx2_libretro.dylib path/to/some.iso

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef void (*retro_init_fn)(void);
typedef void (*retro_deinit_fn)(void);
typedef unsigned (*retro_api_version_fn)(void);
typedef void (*retro_get_system_info_fn)(void*);
typedef void (*retro_set_environment_fn)(void* cb);
typedef int  (*retro_load_game_fn)(const void*);
typedef void (*retro_unload_game_fn)(void);

struct retro_system_info {
    const char* library_name;
    const char* library_version;
    const char* valid_extensions;
    int need_fullpath;
    int block_extract;
};

struct retro_game_info {
    const char* path;
    const void* data;
    size_t size;
    const char* meta;
};

static int env_cb(unsigned cmd, void* data) {
    (void)cmd; (void)data;
    return 0; // refuse everything — keeps the core in defaults
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <core.dylib> [<game.iso>]\n", argv[0]);
        return 1;
    }
    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen failed: %s\n", dlerror()); return 1; }

    #define LOAD(sym, type) type sym = (type)dlsym(h, #sym); \
        if (!sym) { fprintf(stderr, "missing symbol: %s\n", #sym); return 1; }
    LOAD(retro_api_version,       retro_api_version_fn);
    LOAD(retro_set_environment,   retro_set_environment_fn);
    LOAD(retro_init,              retro_init_fn);
    LOAD(retro_deinit,            retro_deinit_fn);
    LOAD(retro_get_system_info,   retro_get_system_info_fn);
    LOAD(retro_load_game,         retro_load_game_fn);
    LOAD(retro_unload_game,       retro_unload_game_fn);
    #undef LOAD

    printf("retro_api_version() = %u\n", retro_api_version());

    retro_set_environment(env_cb);
    retro_init();

    struct retro_system_info info = {0};
    retro_get_system_info(&info);
    printf("library_name     = %s\n", info.library_name);
    printf("library_version  = %s\n", info.library_version);
    printf("valid_extensions = %s\n", info.valid_extensions);

    if (argc >= 3) {
        struct retro_game_info game = {0};
        game.path = argv[2];
        int loaded = retro_load_game(&game);
        printf("retro_load_game returned: %s\n", loaded ? "TRUE" : "FALSE");
        if (loaded) retro_unload_game();
    }

    retro_deinit();
    dlclose(h);
    return 0;
}
```

Then build it:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/tools"
clang test_loader.c -o test_loader
```

- [ ] **Step 2: Run the verification**

If retroarch is installed:
```sh
retroarch -L "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib" /tmp/dummy.iso 2>&1 | head -60
```

Expected: log lines from `retro_init`, then `library_name = PCSX2` reported by retroarch, then a clean refusal (retroarch will show our OSD message in its UI and log the `retro_load_game → false` result).

If using the fallback loader:
```sh
"/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/tools/test_loader" \
    "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib" \
    /tmp/dummy.iso
```

Expected output (something close to):
```
retro_api_version() = 1
[pcsx2_libretro] retro_init — PCSX2 libretro skeleton initialised
library_name     = PCSX2
library_version  = skeleton-0.1
valid_extensions = iso|chd|cso|bin|cue|m3u|gz
[pcsx2_libretro] retro_load_game called with path: /tmp/dummy.iso
[pcsx2_libretro] PCSX2 libretro core skeleton — game loading not implemented yet (phase 1)
retro_load_game returned: FALSE
[pcsx2_libretro] retro_deinit
```

That's the spec's Test 2 passing.

- [ ] **Step 3: If you created the fallback loader, gitignore the binary but commit the source**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
echo "pcsx2-libretro/tools/test_loader" >> .gitignore
git add .gitignore pcsx2-libretro/tools/test_loader.c
git commit -m "$(cat <<'EOF'
libretro: add minimal standalone test loader for skeleton verification

A 30-line dlopen-based C program that exercises the libretro API
exports without requiring retroarch. Used to verify the dylib is a
well-formed libretro core independent of any specific host.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Create the RetroNest manifest

**Files:**
- Create: `${RETRONEST_ROOT}/manifests/pcsx2-libretro.json`

- [ ] **Step 1: Write the manifest**

Create `${RETRONEST_ROOT}/manifests/pcsx2-libretro.json` with exactly:

```json
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

- [ ] **Step 2: Validate the JSON parses**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
python3 -m json.tool manifests/pcsx2-libretro.json > /dev/null && echo "VALID JSON"
```

Expected: `VALID JSON`. If anything else, fix syntax.

- [ ] **Step 3: Verify RetroNest reads it (build + smoke-load)**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build 2>&1 | tail -10
```

Expected: build succeeds. (If RetroNest tracks manifests in CMake, the new file should be picked up. If it tracks them at runtime by reading the manifests directory, no rebuild is needed — but a build/run cycle catches any manifest validator regressions.)

- [ ] **Step 4: Commit the manifest**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add manifests/pcsx2-libretro.json
git commit -m "$(cat <<'EOF'
manifests: add pcsx2-libretro entry for the in-progress libretro core

Sibling of the existing pcsx2 (launched-binary) manifest. Both
entries appear in RetroNest's emulator list during the multi-phase
PCSX2-to-libretro port; the launched-binary entry stays canonical
until the core port is feature-complete.

For now the dylib is copied manually into
{data-root}/emulators/libretro/cores/. The github_repo +
core_buildbot_path fields describe the future GitHub-Release
fetch path that RetroNest's installer will use once we publish
release artifacts.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: Verification Test 3 — loads in RetroNest end-to-end

**Files:** none (verification only).

- [ ] **Step 1: Copy the built dylib into RetroNest's cores directory**

First, identify the RetroNest data root. The user previously configured one — by default it's a folder named `RetroNest` under the user's home or Documents directory. Confirm with:

```sh
ls -d ~/RetroNest/emulators/libretro/cores 2>/dev/null \
    || ls -d ~/Documents/RetroNest/emulators/libretro/cores 2>/dev/null \
    || echo "Cores directory not found — check RetroNest data root"
```

If neither location exists, RetroNest may not have been run with a chosen data root yet. Launch RetroNest once, walk through the setup wizard to choose a data root, then exit and re-run the check.

Once the path is known (call it `${CORES_DIR}`):
```sh
cp "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib" \
   "${CORES_DIR}/"
ls -la "${CORES_DIR}/pcsx2_libretro.dylib"
```

Expected: the file exists in the cores directory with sensible size.

- [ ] **Step 2: Launch RetroNest**

Run:
```sh
open "/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app"
```

Expected: RetroNest launches into the game library (its normal UI).

- [ ] **Step 3: Verify the new emulator entry appears**

In RetroNest's UI: navigate to wherever the emulator list is presented (Settings → Emulators, or the system browser, depending on theme).

Expected: see both `pcsx2` (the existing launched-binary entry) and `pcsx2-libretro` ("PCSX2 (libretro core, dev)") listed.

If `pcsx2-libretro` doesn't appear:
- Check RetroNest logs (run from terminal: `./cpp/build/RetroNest.app/Contents/MacOS/RetroNest 2>&1 | grep -i manifest`).
- Check that the manifest was bundled into the build (some build setups copy `manifests/*.json` into the app bundle's resources at build time — if so, the manifest add in Task 10 needs a build refresh).
- Check that the manifest JSON parses (Task 10 Step 2 should have already caught this).

- [ ] **Step 4: Attempt to launch a PS2 game on the new entry**

Have a PS2 ISO in the configured ROMs directory. In RetroNest's UI, set its preferred emulator to `pcsx2-libretro`, then try to launch it.

Expected:
- No crash.
- No hang (RetroNest stays responsive, does not freeze).
- An error / OSD message appears with text like "PCSX2 libretro core skeleton — game loading not implemented yet (phase 1)" (or however RetroNest's `LibretroAdapter` surfaces a `retro_load_game → false` outcome — exact wording depends on the OSD pipeline).
- RetroNest returns to the game list, ready for another action.

- [ ] **Step 5: Capture log evidence**

For the spec's "Test 3 passes" record, run RetroNest from terminal so we can see the libretro logs:
```sh
"/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest" 2>&1 \
    | tee retronest-test3.log
```

(Then in the GUI, attempt the same launch as Step 4.)

Expected `retronest-test3.log` to contain:
- `retro_init` log line from our shim
- `retro_get_system_info` being called by LibretroAdapter (may not log explicitly, depends on RetroNest's verbosity)
- `retro_load_game called with path: …`
- `PCSX2 libretro core skeleton — game loading not implemented yet`

Save this file as proof of Test 3 passing. **Do not commit it** — it's evidence, not source.

- [ ] **Step 6: Verify the existing pcsx2 (launched-binary) entry STILL works**

Crucial sanity check: the coexistence promise from the spec.

Switch the same PS2 game's preferred emulator from `pcsx2-libretro` back to `pcsx2`, launch it.

Expected: the game launches via the existing launched-binary path exactly as it did before this entire sub-project. If anything has regressed in the launched-binary path, that's a bug we introduced — investigate before declaring the skeleton done.

---

## Task 12: Wrap-up — record completion and prepare for sub-project 2

**Files:**
- Modify: `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-skeleton-design.md` (mark complete)

- [ ] **Step 1: Update the spec status from "Approved (brainstorming)" to "Complete"**

Edit `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-skeleton-design.md`. Change the header line:
```
**Status:** Approved (brainstorming)
```
to:
```
**Status:** Complete (skeleton verified — see verification logs)
```

Append a "Verification log" section at the very bottom:
```markdown

## Verification log

- **Test 1 (build):** PASSED — `pcsx2_libretro.dylib` produced at `pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib`, mach-o ARM64 (or x86_64 as appropriate), exports retro_* symbols.
- **Test 2 (neutral host):** PASSED — verified via retroarch / standalone test_loader. `library_name=PCSX2`, `retro_load_game` returns `false` cleanly with the documented OSD message.
- **Test 3 (RetroNest end-to-end):** PASSED — RetroNest discovered the `pcsx2-libretro` manifest, dylib loaded, attempted launch produced clean refusal OSD message, no crash/hang. Existing pcsx2 launched-binary path unaffected.

Pcsx2-master fork pin: `<actual-upstream-sha-from-Task-1>` (`upstream/master` at work start).
Branch: `retronest-libretro` in `${PCSX2_ROOT}`.
```

- [ ] **Step 2: Commit the spec update**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add docs/superpowers/specs/2026-05-11-pcsx2-libretro-skeleton-design.md
git commit -m "$(cat <<'EOF'
docs(specs): mark pcsx2 libretro skeleton complete

All three verification tests pass:
1. dylib builds
2. neutral libretro host loads and identifies it
3. RetroNest loads it end-to-end with clean refusal OSD

Next sub-project: VM lifecycle + game boot.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 3: Push the pcsx2-master fork branch (optional — only if you've set up an origin remote)**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git remote -v
```

If `origin` is configured (pointing at your private GitHub fork):
```sh
git push -u origin retronest-libretro
```

If `origin` isn't configured, defer this — sub-project 2 can set it up. Local-only is fine for now per the spec.

- [ ] **Step 4: Announce the next sub-project**

The skeleton is done. The next sub-project is **VM lifecycle + game boot** — wiring `retro_load_game` through to `VMManager::Initialize`, BIOS path resolution, and CDVD disc loading, so a game actually starts running inside the core (even with no output yet). That's the next brainstorm → spec → plan → implementation cycle.

---

## Plan self-review (post-write)

Run against the spec:

**Spec coverage:**

| Spec requirement | Implemented in |
|---|---|
| Fork bootstrap, pin recorded | Task 1 |
| `pcsx2-libretro/` directory created | Tasks 2–3 |
| Vendored libretro.h, byte-identical to RetroNest's | Task 2 |
| `pcsx2-libretro/CMakeLists.txt` modeled on gsrunner | Task 3 |
| Top-level `CMakeLists.txt` +4-line opt-in block | Task 4 |
| `LibretroFrontend.h` with `FrontendState` and `FrontendLog` | Task 5 |
| `HostStubs.cpp` covering every Host:: function | Task 6 |
| `LibretroFrontend.cpp` with all retro_* exports | Task 7 |
| Test 1 (build) | Task 8 |
| Test 2 (neutral libretro host) | Task 9 |
| RetroNest manifest, schema matches mgba.json | Task 10 |
| Test 3 (RetroNest end-to-end) | Task 11 |
| Existing launched-binary `pcsx2` keeps working | Task 11 Step 6 |
| Maintenance / upstream-update workflow | Documented in spec; not a runtime task, validated by the next monthly rebase, not by this plan. |
| Risks (Apple Silicon W^X, dep linkage) | Task 8 Step 4 covers the dominant failure mode (missing Host symbols). Other risks are spec-documented and surface during build naturally. |
| Spec status updated on completion | Task 12 |

All spec requirements are covered.

**Placeholder scan:** Search performed for "TBD", "TODO", "FIXME", "implement later". One legitimate occurrence in Task 1 Step 5 (the spec pin placeholder we're filling in) — this is the substitution itself, not a leftover. No other placeholders.

**Type / name consistency:** `FrontendState`, `g_frontend`, `FrontendLog`, `Pcsx2Libretro::` namespace are used consistently across Tasks 5, 6, and 7. Filenames are spelled the same way in CMakeLists (Task 3), file creation (Tasks 5–7), and commit messages.

**Bite-sized check:** Every step is one concrete action (write this file, run this command, check this output, commit). The longest step bodies are in Task 6 (HostStubs.cpp creation) — that task is intrinsically bigger because it's the bulk of the mechanical work, and is broken into 6 sub-steps already.
