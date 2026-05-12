# PCSX2 Libretro Save State (SP6.5) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire `retro_serialize_size` / `retro_serialize` / `retro_unserialize` for the PCSX2 libretro core so save states round-trip cleanly mid-session, and `GameSession::terminate → CoreRuntime::requestSaveState` writes a working resume file.

**Architecture:** Use PCSX2's canonical `SaveState_DownloadState` save path wrapped in an in-memory libzip container (forced `ZIP_CM_STORE` for deterministic probe-once buffer size). For the load path, add one sanctioned upstream function — `SaveState_UnzipFromMemory(buf, size, error)` — that delegates to a shared `SaveState_UnzipFromZip` helper extracted from the existing `SaveState_UnzipFromDisk`. Pause-stable VM handshake (`WaitForVmPaused` / `ResumeVm`, 200 ms ceiling, 1 ms poll) wraps every entry point. All new files live in `pcsx2-libretro/`; the upstream addition is ~26 lines comment-flagged `// pcsx2-libretro: SP6.5`.

**Tech Stack:** C++17, libzip (system; `<zip.h>`), PCSX2's `SaveStateBase` / `memSavingState` / `ArchiveEntryList` (`pcsx2/SaveState.h`), libretro C ABI (`pcsx2-libretro/libretro.h`). Build: CMake with `-DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF`. Smoke test on Ratchet & Clank 2 inside RetroNest under `arch -x86_64`. Working directories: `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `retronest-libretro`) and `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`, for build/install scripts).

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-12-pcsx2-libretro-savestate-sp65-design.md` (commit `81fbbe3`).

**Predecessor commits to reference:**
- `2eddc63de` (reverted) — source of `WaitForVmPaused`, `ResumeVm`, `g_serialize_size` atomic, and the probe-once / reset-on-unload pattern. Use `git show 2eddc63de -- pcsx2-libretro/LibretroFrontend.cpp` to see the verbatim code.
- `803262791` — SP6 Task 5, source of the `IsStateTraceEnabled()` cached-bool helper and `RETRONEST_STATE_TRACE` env-gating pattern. Already shipped in `LibretroFrontend.cpp:121-132`.

---

## Task 1: Skeleton refactor — introduce `LibretroSaveState.{h,cpp}`

**What this builds:** Move the pause-stable handshake helpers (`WaitForVmPaused`, `ResumeVm`) and the `g_serialize_size` atomic out of `LibretroFrontend.cpp` into new files `LibretroSaveState.{h,cpp}`. Three `retro_*` entry points become one-line delegates that still return `0`/`false` (no behavior change yet — same stubs, just refactored). Wire the new files into the CMake target.

**Why first:** Lets later tasks add complete implementations without bloating `LibretroFrontend.cpp` to 600+ lines. The reverted commit `2eddc63de` is the source of truth for `WaitForVmPaused` / `ResumeVm` — those land verbatim in this commit.

**Files:**
- Create: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroSaveState.h`
- Create: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroSaveState.cpp`
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp` (replace stubs lines 354-356 with delegate calls; remove no helpers yet because none are present in current HEAD — those are lifted from the reverted commit and land in this task's new files)
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CMakeLists.txt` (add `LibretroSaveState.cpp` to `target_sources`)
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp` `retro_unload_game` (call `ResetSerializeSizeCache()`)

- [ ] **Step 1.1: Create `LibretroSaveState.h`**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — save state entry points (SP6.5).
//
// Wraps PCSX2's canonical SaveState_DownloadState path in an in-memory
// libzip container so retro_serialize / retro_unserialize round-trip
// cleanly without disk I/O. Probe-once size strategy: cache the
// serialized buffer size on first call while VM is Running, return
// that constant thereafter (deterministic by construction because
// every zip entry is ZIP_CM_STORE-compressed).
//
// See:
//   - SP6.5 spec: docs/superpowers/specs/2026-05-12-pcsx2-libretro-savestate-sp65-design.md
//   - Reverted predecessor: git show 2eddc63de -- pcsx2-libretro/LibretroFrontend.cpp

#pragma once

#include "pcsx2/VMManager.h"   // VMState enum

#include <cstddef>

namespace Pcsx2Libretro
{

// Pauses the VM and waits for it to reach VMState::Paused. Returns
// the state observed BEFORE the pause was requested, so the caller
// can restore it (we don't want to leave a Paused VM Running, or
// vice-versa).
//
// Polls in 1 ms increments up to a 200 ms ceiling. PCSX2-Qt uses the
// same handshake for its "save state while running" UI; the EE
// thread reaches the next event-test typically within a single
// frame (~16 ms). 200 ms catches a deeply stalled MTGS or recompiler
// edge case without locking the host indefinitely — on timeout we
// log a WARN and return VMState::Shutdown so the caller can bail.
//
// Caller must call ResumeVm(prev_state) regardless of whether the
// serialize succeeded.
VMState WaitForVmPaused();

// Restores the VM to prev_state (the value returned by
// WaitForVmPaused). If prev was Running, un-pause. Otherwise leave
// as-is.
void ResumeVm(VMState prev_state);

// Reset the probe-once size cache. Called from retro_unload_game so
// the next game loaded re-probes (different game = different ELF =
// potentially different size).
void ResetSerializeSizeCache();

// Libretro entry-point implementations. LibretroFrontend.cpp's
// retro_serialize_size / retro_serialize / retro_unserialize call
// straight through to these.
size_t SerializeSize();
bool   Serialize(void* dst, size_t len);
bool   Unserialize(const void* src, size_t len);

} // namespace Pcsx2Libretro
```

- [ ] **Step 1.2: Create `LibretroSaveState.cpp` skeleton with `WaitForVmPaused` / `ResumeVm` lifted from `2eddc63de`**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — save state entry points (SP6.5).
//
// Task 1 lands the skeleton: pause-stable handshake helpers
// (lifted verbatim from reverted commit 2eddc63de) plus stub
// entry points that still return 0 / false. Tasks 2-4 fill in
// the libzip plumbing and the upstream load call.

#include "LibretroSaveState.h"
#include "LibretroFrontend.h"   // FrontendLog

#include "pcsx2/VMManager.h"
#include "common/Error.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace Pcsx2Libretro
{

namespace
{

// Probe-once: cache the serialized size on first call while VM is
// Running, return that constant forever. 0 means "VM not yet ready,
// frontend should retry on next call". Reset in
// ResetSerializeSizeCache() (called from retro_unload_game).
std::atomic<size_t> g_serialize_size{0};

// RETRONEST_STATE_TRACE: env-gated tracing. Zero overhead when unset.
// Same pattern as IsStateTraceEnabled() in LibretroFrontend.cpp
// (which already covers the retro_reset boundary from SP6 Task 5).
// SP6.5 adds five more boundaries inside SerializeSize / Serialize /
// Unserialize — defined here so this translation unit owns its
// cached-bool independently of the frontend file.
bool IsStateTraceEnabled()
{
    static const bool s_enabled = (std::getenv("RETRONEST_STATE_TRACE") != nullptr);
    return s_enabled;
}

} // namespace

VMState WaitForVmPaused()
{
    using namespace std::chrono_literals;
    const VMState prev = VMManager::GetState();
    if (prev != VMState::Running)
    {
        // Already paused / not running. No handshake needed.
        return prev;
    }
    VMManager::SetPaused(true);
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + 200ms;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const VMState s = VMManager::GetState();
        if (s == VMState::Paused)
        {
            if (IsStateTraceEnabled())
            {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                FrontendLog(RETRO_LOG_INFO,
                    "[STATE_TRACE] WaitForVmPaused: paused in %lldms", static_cast<long long>(elapsed));
            }
            return prev;
        }
        if (s == VMState::Shutdown) return VMState::Shutdown;
        std::this_thread::sleep_for(1ms);
    }
    FrontendLog(RETRO_LOG_WARN,
        "WaitForVmPaused: 200 ms deadline exceeded — VMState=%d",
        static_cast<int>(VMManager::GetState()));
    return VMState::Shutdown;  // bail
}

void ResumeVm(VMState prev_state)
{
    if (prev_state == VMState::Running &&
        VMManager::GetState() == VMState::Paused)
    {
        VMManager::SetPaused(false);
    }
}

void ResetSerializeSizeCache()
{
    g_serialize_size.store(0);
}

// Task 1 stubs. Tasks 2-4 fill in the real implementations.
size_t SerializeSize() { return 0; }
bool   Serialize(void* /*dst*/, size_t /*len*/)         { return false; }
bool   Unserialize(const void* /*src*/, size_t /*len*/) { return false; }

} // namespace Pcsx2Libretro
```

- [ ] **Step 1.3: Update `LibretroFrontend.cpp` to delegate the three stubs**

Replace lines 354-356 in `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`:

```cpp
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool   retro_serialize(void*, size_t)         { return false; }
RETRO_API bool   retro_unserialize(const void*, size_t) { return false; }
```

with:

```cpp
RETRO_API size_t retro_serialize_size(void)                  { return Pcsx2Libretro::SerializeSize(); }
RETRO_API bool   retro_serialize(void* dst, size_t len)      { return Pcsx2Libretro::Serialize(dst, len); }
RETRO_API bool   retro_unserialize(const void* src, size_t len) { return Pcsx2Libretro::Unserialize(src, len); }
```

Also add the new header include near the top, after the existing `#include "Settings.h"` line (currently `LibretroFrontend.cpp:14`):

```cpp
#include "LibretroSaveState.h"
```

- [ ] **Step 1.4: Call `ResetSerializeSizeCache()` from `retro_unload_game`**

In `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroFrontend.cpp`, inside `retro_unload_game` (currently around line 441-450), add one line near the top of the function next to the existing `g_memory_map_issued.store(false)`:

```cpp
RETRO_API void retro_unload_game(void)
{
    g_memory_map_issued.store(false);     // re-issue on next game load
    g_logged_running.store(false);        // re-log on next Running
    Pcsx2Libretro::ResetSerializeSizeCache();  // re-probe on next game load (SP6.5)
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game: requesting VM shutdown");
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    emu.RequestShutdown();
    emu.Join();
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game: emu thread joined cleanly");
}
```

- [ ] **Step 1.5: Add `LibretroSaveState.cpp` to `CMakeLists.txt`**

In `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CMakeLists.txt`, add the new source after `LibretroInputSource.cpp` (currently line 16):

```cmake
target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    Settings.cpp
    EmuThread.cpp
    LibretroAudioStream.cpp
    LibretroInputSource.cpp
    LibretroSaveState.cpp
)
```

- [ ] **Step 1.6: Build and verify**

Run:

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -30
```

Expected: `[100%] Built target pcsx2_libretro`. No new warnings. No errors. If clangd shows phantom "file not found" diagnostics on PCSX2 headers, ignore them — the cmake build is the source of truth (documented in `project_pcsx2_libretro_port.md`).

- [ ] **Step 1.7: Verify symbol export still works**

```bash
nm /Users/mark/Documents/Projects/pcsx2-libretro/build/pcsx2-libretro/pcsx2_libretro.dylib | grep -E "retro_serialize|retro_unserialize"
```

Expected: three lines showing `T _retro_serialize_size`, `T _retro_serialize`, `T _retro_unserialize`. They were exported before; they should still be exported.

- [ ] **Step 1.8: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/LibretroSaveState.h pcsx2-libretro/LibretroSaveState.cpp pcsx2-libretro/LibretroFrontend.cpp pcsx2-libretro/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP6.5 task 1: extract LibretroSaveState skeleton

Mechanical refactor ahead of the SP6.5 in-memory save state work.
Moves pause-stable handshake helpers (WaitForVmPaused, ResumeVm)
and the g_serialize_size probe-once atomic out of LibretroFrontend.cpp
into new pcsx2-libretro/LibretroSaveState.{h,cpp}. Helpers lifted
verbatim from reverted commit 2eddc63de which the SP6 reviewer
approved on their merits — only the home file changes.

The three retro_serialize* entry points become one-line delegates
into Pcsx2Libretro::SerializeSize / Serialize / Unserialize.
LibretroSaveState.cpp keeps them as stubs returning 0 / false in
this commit; tasks 2-4 fill in real implementations.

retro_unload_game gains a ResetSerializeSizeCache() call so the
next game loaded re-probes (different ELF = potentially different
size).

CMakeLists.txt picks up the new translation unit.

No upstream PCSX2 files touched. No RetroNest source changes.
No behavior change — three stubs still return 0 / false.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Add upstream `SaveState_UnzipFromMemory` (sanctioned exception)

**What this builds:** Add one new public function to `pcsx2/SaveState.{h,cpp}` so the libretro core can drive PCSX2's canonical load flow from an in-memory zip buffer without forking `PreLoadPrep`/`PostLoadPrep`. Extracts a shared body `SaveState_UnzipFromZip(zip_t*, Error*)` so `SaveState_UnzipFromDisk` and the new `SaveState_UnzipFromMemory` literally share their downstream logic — any future upstream load-flow change propagates automatically.

**Why second:** Lands before Task 4 needs it. Refactor + addition is mechanical and can be smoke-tested through the existing `SaveState_UnzipFromDisk` path (which now goes through the extracted helper) before any libretro code calls the new function.

**Why upstream:** `PreLoadPrep` (`SaveState.cpp:51-65`) and `PostLoadPrep` (`SaveState.cpp:67-88`) are file-static and depend on internal PCSX2 globals: `tlb[48]`, `vu1Thread`, `MTGS`, `mmap_ResetBlockTracking`, `VMManager::Internal::ClearCPUExecutionCaches`, `resetCache`, `UnmapTLB`/`MapTLB`, `EmuConfig.Gamefixes.GoemonTlbHack`, `CBreakPoints::SetSkipFirst`, `UpdateVSyncRate`, `R5900SymbolImporter`. Replicating them in `pcsx2-libretro/` would be ~150 lines of fragile internal-API forking. Adding one function in `pcsx2/` matches the SP4/SP5 sanctioned-exception pattern documented in `project_pcsx2_libretro_port.md` ("save-state-backend seam would fit").

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2/SaveState.h:61` (add one declaration after the existing `SaveState_UnzipFromDisk`)
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2/SaveState.cpp:1175-1246` (refactor existing function body into a shared helper) + append the new function

- [ ] **Step 2.1: Add `SaveState_UnzipFromMemory` declaration to `SaveState.h`**

In `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2/SaveState.h`, after the existing line 61:

```cpp
extern bool SaveState_UnzipFromDisk(const std::string& filename, Error* error);
```

add this line:

```cpp
// pcsx2-libretro: SP6.5 — in-memory unzip for the libretro core's retro_unserialize.
// Shares the same body as SaveState_UnzipFromDisk via a private SaveState_UnzipFromZip helper.
extern bool SaveState_UnzipFromMemory(const void* buf, size_t size, Error* error);
```

- [ ] **Step 2.2: Extract `SaveState_UnzipFromZip` from `SaveState_UnzipFromDisk`**

In `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2/SaveState.cpp`, replace the existing `SaveState_UnzipFromDisk` function (lines 1175-1246) with this refactored version. The body from line 1190 onward (`CheckVersion` through `PostLoadPrep` + `return true`) moves to a new private helper:

```cpp
// pcsx2-libretro: SP6.5 — extracted from SaveState_UnzipFromDisk so the new
// SaveState_UnzipFromMemory can share the same CheckVersion + per-entry
// existence check + PreLoadPrep + LoadInternalStructuresState +
// per-entry FreezeIn + PostLoadPrep flow without duplication.
static bool SaveState_UnzipFromZip(zip_t* zf, const std::string& filename_for_log, Error* error)
{
	// look for version and screenshot information in the zip stream:
	if (!CheckVersion(filename_for_log, zf, error))
		return false;

	// check that all parts are included
	const s64 internal_index = CheckFileExistsInState(zf, EntryFilename_InternalStructures, true);
	s64 entryIndices[std::size(SavestateEntries)];

	// Log any parts and pieces that are missing, and then generate an exception.
	bool allPresent = (internal_index >= 0);
	for (u32 i = 0; i < std::size(SavestateEntries); i++)
	{
		const bool required = SavestateEntries[i]->IsRequired();
		entryIndices[i] = CheckFileExistsInState(zf, SavestateEntries[i]->GetFilename(), required);
		if (entryIndices[i] < 0 && required)
		{
			allPresent = false;
			break;
		}
	}
	if (!allPresent)
	{
		Error::SetString(error, "Some required components were not found or are incomplete.");
		return false;
	}

	PreLoadPrep();

	if (!LoadInternalStructuresState(zf, internal_index, error))
	{
		if (!error->IsValid())
			Error::SetString(error, "Save state corruption in internal structures.");

		VMManager::Reset();
		return false;
	}

	for (u32 i = 0; i < std::size(SavestateEntries); ++i)
	{
		if (entryIndices[i] < 0)
		{
			SavestateEntries[i]->FreezeIn(nullptr);
			continue;
		}

		auto zff = zip_fopen_index_managed(zf, entryIndices[i], 0);
		if (!zff || !SavestateEntries[i]->FreezeIn(zff.get()))
		{
			Error::SetString(error, fmt::format("Save state corruption in {}.", SavestateEntries[i]->GetFilename()));
			VMManager::Reset();
			return false;
		}
	}

	PostLoadPrep();
	return true;
}

bool SaveState_UnzipFromDisk(const std::string& filename, Error* error)
{
	zip_error_t ze = {};
	auto zf = zip_open_managed(filename.c_str(), ZIP_RDONLY, &ze);
	if (!zf)
	{
		Console.Error("Failed to open zip file '%s' for save state load: %s", filename.c_str(), zip_error_strerror(&ze));
		if (zip_error_code_zip(&ze) == ZIP_ER_NOENT)
			Error::SetString(error, "Savestate file does not exist.");
		else
			Error::SetString(error, fmt::format("Savestate zip error: {}", zip_error_strerror(&ze)));

		return false;
	}

	// pcsx2-libretro: SP6.5 — delegate to the shared zip-body helper.
	return SaveState_UnzipFromZip(zf.get(), filename, error);
}
```

**Note on the signature change to `CheckVersion`:** `SaveState_UnzipFromZip` takes a `filename_for_log` parameter and passes it to `CheckVersion(filename, zf, error)`. The existing `CheckVersion` signature at `SaveState.cpp:1094` already takes `(const std::string& filename, zip_t* zf, Error* error)` — used only for the error message text. Pass the filename from `SaveState_UnzipFromDisk` and an in-memory marker like `"<memory>"` from `SaveState_UnzipFromMemory`. No `CheckVersion` modification needed.

- [ ] **Step 2.3: Add `SaveState_UnzipFromMemory` definition**

Append to `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2/SaveState.cpp` immediately after the refactored `SaveState_UnzipFromDisk` (so it sits next to its sibling):

```cpp
// pcsx2-libretro: SP6.5 — in-memory variant of SaveState_UnzipFromDisk.
// Used by the libretro core (pcsx2-libretro/LibretroSaveState.cpp) to drive
// the canonical load flow from a buffer the libretro frontend hands us
// during retro_unserialize. Shares the entire downstream load body with
// SaveState_UnzipFromDisk via SaveState_UnzipFromZip — see comment on
// that helper above.
//
// Buffer lifetime: caller owns `buf`. The zip_source_buffer takes a
// non-owning view (the 4th arg is freep=0). zip_close frees the source.
bool SaveState_UnzipFromMemory(const void* buf, size_t size, Error* error)
{
	if (!buf || size == 0)
	{
		Error::SetString(error, "Savestate memory buffer is empty.");
		return false;
	}

	zip_error_t ze = {};
	zip_source_t* zs = zip_source_buffer_create(buf, size, /*freep=*/0, &ze);
	if (!zs)
	{
		Error::SetString(error, fmt::format("Savestate zip_source_buffer_create failed: {}", zip_error_strerror(&ze)));
		return false;
	}

	zip_t* zf = zip_open_from_source(zs, ZIP_RDONLY, &ze);
	if (!zf)
	{
		zip_source_free(zs);
		Error::SetString(error, fmt::format("Savestate zip_open_from_source failed: {}", zip_error_strerror(&ze)));
		return false;
	}

	const bool ok = SaveState_UnzipFromZip(zf, "<memory>", error);
	zip_close(zf);  // also frees zs
	return ok;
}
```

- [ ] **Step 2.4: Build and verify nothing regressed in the existing path**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -20
```

Expected: `[100%] Built target pcsx2_libretro`. No new warnings. The refactor of `SaveState_UnzipFromDisk` is mechanical; the build should succeed cleanly.

- [ ] **Step 2.5: Verify the new symbols are exported (for the libretro core's later use)**

The libretro core dlopens PCSX2 via direct linking, not symbol lookup, so this is mostly a sanity check:

```bash
nm /Users/mark/Documents/Projects/pcsx2-libretro/build/pcsx2-libretro/pcsx2_libretro.dylib | grep -E "SaveState_UnzipFromMemory|SaveState_UnzipFromDisk" | head -4
```

Expected: both symbols appear. (They'll have C++-mangled names.)

- [ ] **Step 2.6: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2/SaveState.h pcsx2/SaveState.cpp
git commit -m "$(cat <<'EOF'
SP6.5 task 2: add SaveState_UnzipFromMemory upstream (sanctioned)

Adds one public function SaveState_UnzipFromMemory(buf, size, error)
to pcsx2/SaveState.{h,cpp} so the pcsx2-libretro core can drive the
canonical load flow from a memory buffer without forking the file-
static PreLoadPrep / PostLoadPrep helpers and their dependencies on
tlb[48], vu1Thread, MTGS, mmap_ResetBlockTracking, resetCache,
UnmapTLB / MapTLB, R5900SymbolImporter, etc.

Body shared with SaveState_UnzipFromDisk via a new file-static
SaveState_UnzipFromZip(zip_t*, filename_for_log, error). Same
CheckVersion + per-entry existence check + PreLoadPrep +
LoadInternalStructuresState + per-entry FreezeIn + PostLoadPrep
flow, no duplication, no behavior change to the disk path.

All three modified hunks comment-flagged // pcsx2-libretro: SP6.5
per the SP4 / SP5 sanctioned-exception convention documented in
project_pcsx2_libretro_port.md ("save-state-backend seam would fit").
Monthly rebase reviewer can grep for those markers.

Total upstream diff: ~26 lines added (one shared helper plus the new
function plus a 4-line wrapper around the existing
SaveState_UnzipFromDisk that now delegates to the shared helper).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Implement `Serialize` + `SerializeSize` via in-memory libzip

**What this builds:** The save half of SP6.5. `SerializeSize` runs the canonical `SaveState_DownloadState` path, wraps the output in an in-memory zip with `ZIP_CM_STORE` everywhere, caches the byte count, and returns it. `Serialize` does the same and `memcpy`s the bytes into the caller's buffer (zero-padding any tail). Load still returns `false` — that's Task 4.

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroSaveState.cpp` (fill in `SerializeSize` and `Serialize`; add libzip helper functions)

- [ ] **Step 3.1: Add includes**

In `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroSaveState.cpp`, at the top after the existing `#include "common/Error.h"`:

```cpp
#include "LibretroSaveState.h"
#include "LibretroFrontend.h"   // FrontendLog, g_frontend

#include "pcsx2/VMManager.h"
#include "pcsx2/SaveState.h"    // SaveState_DownloadState, ArchiveEntryList, SaveState_UnzipFromMemory
#include "common/Error.h"

#include <zip.h>                // libzip

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>
```

- [ ] **Step 3.2: Define `MemoryZipSink` write-mode helper**

Inside the anonymous namespace in `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroSaveState.cpp`, after the `IsStateTraceEnabled` helper, add:

```cpp
// A libzip source backed by a std::vector<u8>. Write-only — Task 4
// uses zip_source_buffer_create directly for the read path (the
// upstream SaveState_UnzipFromMemory call handles that), so we
// don't need full read/seek/write callback dispatch here.
//
// Lifetime: caller constructs a MemoryWriteSink, calls
// AcquireSource() once (which returns a zip_source_t* whose
// ownership transfers to the zip_t* on successful
// zip_open_from_source), then after zip_close the caller takes
// sink.bytes by reference.
//
// On zip_open_from_source FAILURE the source is not transferred;
// the caller must zip_source_free(...) the returned pointer to
// avoid a leak. Standard libzip ownership contract.
struct MemoryWriteSink
{
    std::vector<u8> bytes;
    size_t cursor = 0;   // libzip writes/seeks within bytes

    // libzip callback. cmd is one of the ZIP_SOURCE_* opcodes.
    static zip_int64_t Callback(void* userdata, void* data, zip_uint64_t length, zip_source_cmd_t cmd)
    {
        MemoryWriteSink* self = static_cast<MemoryWriteSink*>(userdata);
        switch (cmd)
        {
        case ZIP_SOURCE_OPEN:
            self->cursor = 0;
            return 0;
        case ZIP_SOURCE_READ:
        {
            if (self->cursor >= self->bytes.size()) return 0;
            const zip_uint64_t avail = self->bytes.size() - self->cursor;
            const zip_uint64_t n = (length < avail) ? length : avail;
            std::memcpy(data, self->bytes.data() + self->cursor, n);
            self->cursor += n;
            return static_cast<zip_int64_t>(n);
        }
        case ZIP_SOURCE_CLOSE:
            return 0;
        case ZIP_SOURCE_STAT:
        {
            zip_stat_t* st = static_cast<zip_stat_t*>(data);
            zip_stat_init(st);
            st->size = self->bytes.size();
            st->valid = ZIP_STAT_SIZE;
            return sizeof(*st);
        }
        case ZIP_SOURCE_ERROR:
        {
            int* errs = static_cast<int*>(data);
            errs[0] = errs[1] = 0;
            return 2 * sizeof(int);
        }
        case ZIP_SOURCE_FREE:
            // MemoryWriteSink is stack-owned by the caller; nothing to free.
            return 0;
        case ZIP_SOURCE_BEGIN_WRITE:
            self->bytes.clear();
            self->cursor = 0;
            return 0;
        case ZIP_SOURCE_WRITE:
        {
            if (self->cursor + length > self->bytes.size())
                self->bytes.resize(self->cursor + length);
            std::memcpy(self->bytes.data() + self->cursor, data, length);
            self->cursor += length;
            return static_cast<zip_int64_t>(length);
        }
        case ZIP_SOURCE_COMMIT_WRITE:
            // Truncate any tail past cursor (in case write-then-shrink happened).
            self->bytes.resize(self->cursor);
            return 0;
        case ZIP_SOURCE_ROLLBACK_WRITE:
            self->bytes.clear();
            self->cursor = 0;
            return 0;
        case ZIP_SOURCE_SEEK:
        {
            zip_int64_t off = zip_source_seek_compute_offset(self->cursor, self->bytes.size(), data, length, nullptr);
            if (off < 0) return -1;
            self->cursor = static_cast<size_t>(off);
            return 0;
        }
        case ZIP_SOURCE_SEEK_WRITE:
        {
            zip_int64_t off = zip_source_seek_compute_offset(self->cursor, self->bytes.size(), data, length, nullptr);
            if (off < 0) return -1;
            self->cursor = static_cast<size_t>(off);
            return 0;
        }
        case ZIP_SOURCE_TELL:
            return static_cast<zip_int64_t>(self->cursor);
        case ZIP_SOURCE_TELL_WRITE:
            return static_cast<zip_int64_t>(self->cursor);
        case ZIP_SOURCE_SUPPORTS:
            return zip_source_make_command_bitmap(
                ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT,
                ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_SEEK, ZIP_SOURCE_TELL,
                ZIP_SOURCE_BEGIN_WRITE, ZIP_SOURCE_WRITE, ZIP_SOURCE_COMMIT_WRITE,
                ZIP_SOURCE_ROLLBACK_WRITE, ZIP_SOURCE_SEEK_WRITE, ZIP_SOURCE_TELL_WRITE,
                ZIP_SOURCE_SUPPORTS, -1);
        default:
            return -1;
        }
    }

    zip_source_t* AcquireSource()
    {
        zip_error_t ze = {};
        zip_source_t* zs = zip_source_function_create(&MemoryWriteSink::Callback, this, &ze);
        if (!zs)
        {
            FrontendLog(RETRO_LOG_WARN, "MemoryWriteSink::AcquireSource: %s", zip_error_strerror(&ze));
        }
        return zs;
    }
};
```

**Note:** `u8` comes from PCSX2's `common/Pcsx2Types.h` (transitively included via `pcsx2/VMManager.h`). If the compiler complains, add `#include "common/Pcsx2Types.h"` explicitly.

- [ ] **Step 3.3: Define the save-into-buffer worker function**

Still inside the anonymous namespace in `LibretroSaveState.cpp`, after `MemoryWriteSink`:

```cpp
// Drives SaveState_DownloadState, then walks the returned
// ArchiveEntryList writing each entry into an in-memory zip with
// forced ZIP_CM_STORE compression. Returns true on success and
// leaves the finalized zip bytes in sink.bytes. Returns false and
// leaves sink.bytes in unspecified state on any failure (caller
// should bail).
//
// Pre-condition: caller has paused the VM (WaitForVmPaused).
bool BuildZipIntoSink(MemoryWriteSink& sink)
{
    Error err;
    std::unique_ptr<ArchiveEntryList> srclist = SaveState_DownloadState(&err);
    if (!srclist)
    {
        FrontendLog(RETRO_LOG_WARN,
            "BuildZipIntoSink: SaveState_DownloadState failed: %s",
            err.GetDescription().c_str());
        return false;
    }

    zip_source_t* src = sink.AcquireSource();
    if (!src) return false;

    zip_error_t ze = {};
    zip_t* zf = zip_open_from_source(src, ZIP_CREATE | ZIP_TRUNCATE, &ze);
    if (!zf)
    {
        FrontendLog(RETRO_LOG_WARN,
            "BuildZipIntoSink: zip_open_from_source: %s", zip_error_strerror(&ze));
        zip_source_free(src);   // not transferred when zip_open_from_source fails
        return false;
    }
    // src ownership now belongs to zf — do NOT zip_source_free below.

    // Write each ArchiveEntry as a ZIP_CM_STORE file entry. Buffer
    // slices come from srclist's contiguous VmStateBuffer; we hand
    // libzip a non-owning view (freep=0) since srclist outlives the
    // call.
    bool ok = true;
    const u8* base = srclist->GetBuffer().data();
    for (uint i = 0; i < srclist->GetLength(); ++i)
    {
        const ArchiveEntry& e = (*srclist)[i];

        zip_source_t* es = zip_source_buffer(zf, base + e.GetDataIndex(), e.GetDataSize(), /*freep=*/0);
        if (!es)
        {
            FrontendLog(RETRO_LOG_WARN,
                "BuildZipIntoSink: zip_source_buffer failed for %s",
                e.GetFilename().c_str());
            ok = false;
            break;
        }

        const s64 fi = zip_file_add(zf, e.GetFilename().c_str(), es, ZIP_FL_ENC_UTF_8);
        if (fi < 0)
        {
            FrontendLog(RETRO_LOG_WARN,
                "BuildZipIntoSink: zip_file_add failed for %s: %s",
                e.GetFilename().c_str(), zip_strerror(zf));
            zip_source_free(es);   // not transferred on failure
            ok = false;
            break;
        }
        // es now owned by zf.

        if (zip_set_file_compression(zf, fi, ZIP_CM_STORE, 0) != 0)
        {
            FrontendLog(RETRO_LOG_WARN,
                "BuildZipIntoSink: zip_set_file_compression failed for %s: %s",
                e.GetFilename().c_str(), zip_strerror(zf));
            ok = false;
            break;
        }
    }

    if (!ok)
    {
        zip_discard(zf);   // also frees src
        return false;
    }

    // The version-indicator entry that SaveState_UnzipFromZip
    // expects (CheckVersion reads "PCSX2 Savestate Version.id").
    // Build it inline; the format matches SaveState_AddToZip at
    // pcsx2/SaveState.cpp:976-1010.
    //
    // Note: the existing pcsx2/SaveState.cpp definitions of
    // STATE_PCSX2_VERSION_SIZE (32) and EntryFilename_StateVersion
    // and g_SaveVersion are file-static / private. We reproduce the
    // constants here. Cross-validate at code-review time:
    //   - STATE_PCSX2_VERSION_SIZE: pcsx2/SaveState.cpp:342
    //   - EntryFilename_StateVersion: pcsx2/SaveState.cpp:339
    //   - g_SaveVersion: pcsx2/SaveState.h:29 (public)
    {
        constexpr u32 kVersionSize = 32;
        struct VersionIndicator
        {
            u32 save_version;
            char version[kVersionSize];
        };

        // Stack-local is safe because zip_close runs before this function
        // returns, finalizing the zip before vi goes out of scope.
        // libzip holds a non-owning view (freep=0).
        VersionIndicator vi{};
        vi.save_version = g_SaveVersion;
        std::strncpy(vi.version, "libretro", kVersionSize - 1);
        vi.version[kVersionSize - 1] = 0;

        zip_source_t* vsrc = zip_source_buffer(zf, &vi, sizeof(vi), /*freep=*/0);
        if (!vsrc)
        {
            FrontendLog(RETRO_LOG_WARN, "BuildZipIntoSink: version source: %s", zip_strerror(zf));
            zip_discard(zf);
            return false;
        }
        const s64 fi = zip_file_add(zf, "PCSX2 Savestate Version.id", vsrc, ZIP_FL_ENC_UTF_8);
        if (fi < 0)
        {
            FrontendLog(RETRO_LOG_WARN, "BuildZipIntoSink: version zip_file_add: %s", zip_strerror(zf));
            zip_source_free(vsrc);
            zip_discard(zf);
            return false;
        }
        zip_set_file_compression(zf, fi, ZIP_CM_STORE, 0);
    }

    if (zip_close(zf) != 0)
    {
        FrontendLog(RETRO_LOG_WARN, "BuildZipIntoSink: zip_close failed: %s", zip_strerror(zf));
        zip_discard(zf);
        return false;
    }
    // zip_close on success has freed zf and finalized sink.bytes.

    return true;
}
```

**Important verification at code-review time:** the `kVersionSize = 32` and `"PCSX2 Savestate Version.id"` filename must match `STATE_PCSX2_VERSION_SIZE` and `EntryFilename_StateVersion` in `pcsx2/SaveState.cpp:339-342`. If they drift in a future upstream change, `CheckVersion` rejects our save. Comment-flag both constants in code review so a rebase reviewer notices.

- [ ] **Step 3.4: Replace `SerializeSize` stub with the real implementation**

Replace the stub `size_t SerializeSize() { return 0; }` in `LibretroSaveState.cpp` with:

```cpp
size_t SerializeSize()
{
    // Probe-once: build a scratch zip, cache its size, return that
    // constant forever for this game. Returns 0 pre-Running so the
    // frontend retries on the next call (spec-legal).
    //
    // Deterministic by construction: every entry is ZIP_CM_STORE, so
    // output size = sum(entry_bytes) + sum(local_file_headers) +
    // central_directory + EOCD — fixed per (build, game).
    const size_t cached = g_serialize_size.load();
    if (cached != 0) return cached;
    if (!VMManager::HasValidVM()) return 0;
    if (VMManager::GetState() != VMState::Running) return 0;

    if (IsStateTraceEnabled())
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] SerializeSize: probe start");

    const auto t0 = std::chrono::steady_clock::now();
    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown)
    {
        FrontendLog(RETRO_LOG_WARN, "SerializeSize: pause handshake failed");
        return 0;
    }

    size_t probed = 0;
    {
        MemoryWriteSink sink;
        if (BuildZipIntoSink(sink))
            probed = sink.bytes.size();
    }

    ResumeVm(prev);

    if (probed == 0) return 0;
    g_serialize_size.store(probed);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    FrontendLog(RETRO_LOG_INFO,
        "SerializeSize: probed=%zu bytes in %lldms (cached)",
        probed, static_cast<long long>(elapsed));
    return probed;
}
```

- [ ] **Step 3.5: Replace `Serialize` stub with the real implementation**

Replace the stub `bool Serialize(void* /*dst*/, size_t /*len*/) { return false; }` with:

```cpp
bool Serialize(void* dst, size_t len)
{
    if (!dst) return false;
    const size_t expected = g_serialize_size.load();
    if (expected == 0) return false;          // probe hasn't run yet
    if (len < expected) return false;         // frontend allocation bug
    if (!VMManager::HasValidVM()) return false;

    if (IsStateTraceEnabled())
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] Serialize: start len=%zu", len);

    const auto t0 = std::chrono::steady_clock::now();
    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown) return false;

    bool ok = false;
    {
        MemoryWriteSink sink;
        if (BuildZipIntoSink(sink))
        {
            if (sink.bytes.size() > len)
            {
                FrontendLog(RETRO_LOG_ERROR,
                    "Serialize: produced %zu bytes but caller buffer is %zu — "
                    "probe-once assumption violated; not writing",
                    sink.bytes.size(), len);
            }
            else
            {
                std::memcpy(dst, sink.bytes.data(), sink.bytes.size());
                if (sink.bytes.size() < len)
                {
                    std::memset(static_cast<u8*>(dst) + sink.bytes.size(), 0,
                                len - sink.bytes.size());
                }
                ok = true;
            }
        }
    }

    ResumeVm(prev);

    if (IsStateTraceEnabled())
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] Serialize: done ok=%d in %lldms",
            ok ? 1 : 0, static_cast<long long>(elapsed));
    }
    return ok;
}
```

- [ ] **Step 3.6: Build**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -30
```

Expected: `[100%] Built target pcsx2_libretro`. No new warnings.

If `<zip.h>` isn't found, libzip is bundled at `3rdparty/libzip` — the existing `pcsx2/SaveState.cpp` includes it directly. Verify the target_link_libraries pulls libzip through:

```bash
grep -rn "libzip\|3rdparty/libzip\|find_package(.*libzip\|target_link_libraries.*zip" /Users/mark/Documents/Projects/pcsx2-libretro/cmake /Users/mark/Documents/Projects/pcsx2-libretro/CMakeLists.txt 2>/dev/null | head -10
```

If libzip isn't already on the link path, add `find_package(libzip REQUIRED)` and `target_link_libraries(pcsx2_libretro PRIVATE libzip::zip)` to `pcsx2-libretro/CMakeLists.txt`. Most likely it's already transitively pulled via `PCSX2` — try the build first.

- [ ] **Step 3.7: Install the new dylib for smoke testing**

```bash
cp /Users/mark/Documents/Projects/pcsx2-libretro/build/pcsx2-libretro/pcsx2_libretro.dylib ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

This single-arch dylib (arm64 OR x86_64 depending on the cmake build) is sufficient for smoke testing. The universal-build path (`RetroNest-Project/scripts/build-universal.sh`) is only needed for cross-arch parity verification before final commit.

- [ ] **Step 3.8: Smoke test — probe-once stability**

```bash
arch -x86_64 env RETRONEST_STATE_TRACE=1 \
    /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest \
    > /tmp/retronest_sp65_t3.log 2>&1 &
```

In RetroNest: launch R&C 2. Wait for game to boot past the BIOS / Insomniac logo (VM reaches Running). Trigger a save state from RetroNest's UI (or from a libretro frontend Save State action; check `LibretroOverlayPanel` for the binding — SP3.5 wired the in-game menu).

Inspect the log:

```bash
grep -E "SerializeSize|Serialize:" /tmp/retronest_sp65_t3.log
```

Expected lines (probe-once):
- One `SerializeSize: probed=N bytes in Mms (cached)` line (M < 200; N should be 5–10 MB for R&C 2 at this point in the game).
- For every subsequent save: a `[STATE_TRACE] Serialize: start len=N` followed by `[STATE_TRACE] Serialize: done ok=1 in Yms`.

If the log instead shows multiple `probed=N` lines, probe-once is broken — investigate the `g_serialize_size` atomic.

Trigger 4 more saves. Verify:
- `N` is identical across all probes (only one probe line should exist).
- All `Serialize: done ok=1` lines fire.
- Load is still stubbed → frontend's load action returns false. RetroNest may show an error toast for load; that's expected this commit.

Quit RetroNest. Close cleanly via the close button.

- [ ] **Step 3.9: Commit (only if smoke test passed)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/LibretroSaveState.cpp
git commit -m "$(cat <<'EOF'
SP6.5 task 3: in-memory save via libzip ZIP_CM_STORE

Implements Pcsx2Libretro::SerializeSize and Serialize. The save flow:

  1. WaitForVmPaused (200 ms ceiling) so PCSX2 internal state is stable.
  2. SaveState_DownloadState writes the canonical ArchiveEntryList
     (BIOS + InternalStructures + 14 SavestateEntries — EE/IOP/VU/SPU2/
     USB/PAD/GS/Achievements).
  3. MemoryWriteSink wraps a std::vector<u8> behind a libzip
     zip_source_function callback that handles BEGIN_WRITE/WRITE/
     COMMIT_WRITE/SEEK_WRITE/TELL_WRITE plus standard read opcodes.
  4. zip_open_from_source(ZIP_CREATE|ZIP_TRUNCATE); for each
     ArchiveEntry, zip_source_buffer over the slice and zip_file_add
     with ZIP_CM_STORE (forced uncompressed for deterministic output).
  5. Inline-build the version-indicator entry that SaveState_UnzipFromZip
     CheckVersion expects (PCSX2 Savestate Version.id, 32-byte struct).
  6. zip_close finalizes sink.bytes. ResumeVm.

SerializeSize probes once, caches in g_serialize_size atomic, returns
that constant for the rest of the game session. Output is deterministic
by construction with ZIP_CM_STORE everywhere — sum of entry data sizes
plus fixed zip overhead — so probe-once is safe.

Serialize re-runs the same flow, memcpy's into the caller's buffer,
zero-pads any tail. Refuses if the probe-once assumption was somehow
violated (loud RETRO_LOG_ERROR).

Load still stubbed (returns false) — wired in task 4.

RETRONEST_STATE_TRACE adds three boundaries: handshake-paused,
SerializeSize-probe-done, Serialize-start/done.

Smoke-tested on R&C 2: probe-once size stable across 5 saves,
Serialize: done ok=1 every time.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Implement `Unserialize` via `SaveState_UnzipFromMemory`

**What this builds:** The load half of SP6.5. `Unserialize` pauses the VM, calls the upstream `SaveState_UnzipFromMemory` added in Task 2 with the caller's buffer, resumes. End-to-end round-trip works after this task.

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroSaveState.cpp` (replace `Unserialize` stub)

- [ ] **Step 4.1: Replace `Unserialize` stub with the real implementation**

In `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/LibretroSaveState.cpp`, replace `bool Unserialize(const void* /*src*/, size_t /*len*/) { return false; }` with:

```cpp
bool Unserialize(const void* src, size_t len)
{
    if (!src || len == 0) return false;
    if (!VMManager::HasValidVM()) return false;

    if (IsStateTraceEnabled())
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] Unserialize: start len=%zu", len);

    const auto t0 = std::chrono::steady_clock::now();
    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown) return false;

    Error err;
    const bool ok = SaveState_UnzipFromMemory(src, len, &err);
    if (!ok)
    {
        // SaveState_UnzipFromZip already calls VMManager::Reset() on
        // mid-load failure. Pre-PreLoadPrep failures (CheckVersion,
        // missing entries) leave the VM untouched.
        FrontendLog(RETRO_LOG_WARN, "Unserialize: load failed: %s",
            err.GetDescription().c_str());
    }

    ResumeVm(prev);

    if (IsStateTraceEnabled())
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] Unserialize: done ok=%d in %lldms",
            ok ? 1 : 0, static_cast<long long>(elapsed));
    }
    return ok;
}
```

- [ ] **Step 4.2: Build**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build --target pcsx2_libretro 2>&1 | tail -20
```

Expected: `[100%] Built target pcsx2_libretro`.

- [ ] **Step 4.3: Install**

```bash
cp /Users/mark/Documents/Projects/pcsx2-libretro/build/pcsx2-libretro/pcsx2_libretro.dylib ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

- [ ] **Step 4.4: Smoke test 1 — mid-session round-trip**

```bash
arch -x86_64 env RETRONEST_STATE_TRACE=1 \
    /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest \
    > /tmp/retronest_sp65_t4_rt.log 2>&1 &
```

In RetroNest:
1. Launch R&C 2. Boot to first playable point.
2. Note Ratchet's position visually + inventory state.
3. Save state (frontend slot 0 via RetroNest UI / LibretroOverlayPanel).
4. Move Ratchet noticeably (~30 s of gameplay; walk, jump, shoot if you have a weapon).
5. Load state from slot 0.

Verify:
- Ratchet visually snaps back to the save position.
- Inventory matches save point.
- Audio resyncs within 1 s (brief glitch is normal; persistent desync is not).
- No crash, no `[ERROR]` lines in the log, no GS device complaint.

Log inspection:

```bash
grep -E "SerializeSize|Serialize:|Unserialize:" /tmp/retronest_sp65_t4_rt.log
```

Expected: probe line once, then alternating Serialize / Unserialize done lines, all `ok=1`.

- [ ] **Step 4.5: Smoke test 2 — quit-resume**

With game running, quit RetroNest cleanly (close button). Wait for clean shutdown. Reopen RetroNest. Launch R&C 2 again.

Verify:
- The `findResumeFile` log line indicates resume file was found.
- Game returns to approximately where it was at quit, not to BIOS/title screen.

```bash
grep -E "requestSaveState|findResumeFile|Unserialize:|SerializeSize" /tmp/retronest_sp65_t4_rt.log
```

Expected: `requestSaveState` on the quit side, `findResumeFile` + `Unserialize: done ok=1` on the relaunch side.

- [ ] **Step 4.6: Smoke test 3 — bad input rejection**

This requires a libretro frontend hook to call `retro_unserialize` with a crafted buffer. Easiest: temporarily wire RetroNest's load-state path to feed 256 bytes of zeros (or skip — covered indirectly by step 4.4's exercise of all `Unserialize` error paths). If skipping, document in the commit message that bad-input behavior was inferred from the `SaveState_UnzipFromZip` path rather than directly exercised.

For a manual test path, the cleanest insertion point is a one-off addition inside RetroNest's `CoreRuntime::flushPendingLoadState` that injects garbage on a specific path flag. **Out of scope for SP6.5 unless step 4.4 surfaces a load-failure bug.**

- [ ] **Step 4.7: Smoke test 4 — probe-once size consistency across game session**

Repeat the size verification from Task 3 step 3.8, this time confirming probe-once stays consistent ALSO across save+load cycles (the load should not invalidate the cached size). Trigger: 5 saves, 5 loads, interleaved. The trace should still show only one `probed=N` line for the whole session.

- [ ] **Step 4.8: Verify universal build still works**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
./scripts/build-universal.sh 2>&1 | tail -20
```

Expected: build succeeds for both arm64 and x86_64 slices. Universal dylib at `~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib` is updated.

Quick repeat of smoke test 4.4 under universal-build dylib confirms parity.

- [ ] **Step 4.9: Commit (only if all smoke tests passed)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/LibretroSaveState.cpp
git commit -m "$(cat <<'EOF'
SP6.5 task 4: in-memory load via SaveState_UnzipFromMemory

Implements Pcsx2Libretro::Unserialize. The load flow:

  1. WaitForVmPaused (200 ms ceiling) so PCSX2 internal state is stable.
  2. Call upstream SaveState_UnzipFromMemory(src, len, &err) added
     in task 2. That function opens a zip_source_buffer over the
     caller's bytes, then delegates to the shared SaveState_UnzipFromZip
     which runs the canonical CheckVersion + per-entry existence check
     + PreLoadPrep + LoadInternalStructuresState + per-entry FreezeIn
     + PostLoadPrep flow.
  3. Mid-load failures inside SaveState_UnzipFromZip call
     VMManager::Reset() (mirrors SaveState.cpp:1239); pre-PreLoadPrep
     failures (CheckVersion mismatch, missing entries) leave the VM
     untouched.
  4. ResumeVm regardless of outcome.

RETRONEST_STATE_TRACE adds the final two boundaries:
Unserialize-start/done. SP6.5 trace surface complete.

Smoke-tested on R&C 2:
  - Mid-session round-trip: save, advance 30 s gameplay, load — Ratchet
    position + inventory snap back; audio resyncs within 1 s; no crash.
  - Quit-resume: GameSession::terminate → requestSaveState writes resume
    file; relaunch finds it via findResumeFile and Unserialize ok=1
    restores game state, not BIOS / title screen.
  - Probe-once size stable across 5 save+load cycles in one session.
  - Universal build (arm64 + x86_64) parity confirmed.

SP6.5 complete.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Post-implementation cleanup

After Task 4 commits cleanly, optional follow-ups:

- **Update memory:** edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_sp6_shipped.md` and `project_pcsx2_libretro_port.md` to mark SP6.5 ✅ DONE and capture any architectural facts that surfaced (e.g. zip_source_function callback gotchas, libzip ownership-on-failure rules).
- **Trace flag documentation:** if there's a `RETRONEST_*_TRACE` registry doc anywhere, add the new SP6.5 boundary list.
- **Pre-allocate the probe-cached size** as a `sink.bytes.reserve(g_serialize_size.load())` micro-optimization at the top of `BuildZipIntoSink` (skips vector growth for every non-probe call). Only do this if profiling shows growth cost matters — likely under 1 ms.
- **Push branches:** the user may not want this; ask before `git push`.

## Risks and mitigations recap

| Risk | Mitigation |
|---|---|
| `zip_source_function` opcode dispatch has subtle bugs | Two-stage subagent review (spec-compliance + code-quality), exercise both read paths (via Task 4 round-trip) and write paths (Task 3 probe + serialize) before commit |
| Version-indicator constants (32, "PCSX2 Savestate Version.id") drift from upstream | Code-review markers in Task 3 step 3.3; monthly rebase reviewer notices |
| `zip_close` cost > 30 ms on every save | Profile via `[STATE_TRACE] Serialize: done ... in Yms` log; if >30 ms consistently, add `sink.bytes.reserve` |
| Pause handshake hangs game | 200 ms ceiling + RETRO_LOG_WARN + Shutdown sentinel — host never blocks indefinitely |
| Upstream `SaveState.cpp` changes break our shared helper | Comment-flagged hunks visible to rebase reviewer; shared body means most upstream changes propagate automatically |
| libretro frontend calls `retro_serialize` from a wrong thread | Same handshake covers any caller; only enforces single-pauser invariant which is the spec contract |

## Definition of done

- [ ] All 4 task commits land cleanly on `pcsx2-libretro@retronest-libretro`.
- [ ] Task 2's upstream addition is comment-flagged at every hunk.
- [ ] Smoke test 4.4 (mid-session round-trip) passes on R&C 2.
- [ ] Smoke test 4.5 (quit-resume) passes on R&C 2.
- [ ] Universal build verified.
- [ ] Both repos clean. No untracked staged work.
