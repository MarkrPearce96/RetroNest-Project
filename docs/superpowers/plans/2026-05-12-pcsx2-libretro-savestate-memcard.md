# PCSX2 Libretro Memory Cards, Memory Map & Reset (SP6) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Scope change (2026-05-12):** Task 4 (save state) was implemented at commit `2eddc63de` and reverted at `f164c4f5f` after code review revealed the spec's "FreezeBios() + FreezeInternals()" assumption was incomplete — the canonical PCSX2 save path also iterates `SavestateEntries[]` (EE main RAM, IOP RAM, VU mem, SPU2, PAD, GS, Achievements) via `FreezeOut`, and the symmetric load via `FreezeIn` requires a `zip_file_t*` rather than a flat buffer. Proper save state needs either in-memory libzip plumbing or a custom container format — both more complex than the spec anticipated. **Save state deferred to SP6.5.** SP6 now ships three items: memcards, memory map, reset.

**Goal:** Wire three VMManager-state-lifecycle items — memory-card persistence, `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` for PS2 RetroAchievements, and `retro_reset → VMManager::Reset()`. End state: in-game memcard saves persist between sessions; RA cheevos trigger on R&C 2; frontend reset (and RA hardcore mode enable) returns the VM to BIOS without restarting the core.

**Architecture:** All changes inside `pcsx2-libretro/`. **Zero RetroNest source changes** — the RetroNest side (CoreRuntime save/load plumbing, SET_MEMORY_MAPS env handler, retro_reset symbol loader) is already shipped. Save states use PCSX2's `memSavingState`/`memLoadingState` directly (raw uncompressed `VmStateBuffer`), with a probe-once `retro_serialize_size` cached as a constant. Memcards reuse PCSX2's existing `MemoryCardFile` plumbing — Settings.cpp configures slot 1 (file image, `Mcd001.ps2`) under a libretro `save_dir`-derived directory. Memory map issues a single descriptor for EE main RAM (32 MB at PS2-physical 0) from inside `retro_run` on the first frame reporting `VMState::Running`. `retro_reset` delegates to `VMManager::Reset()` which is thread-safe for the "outside-VM-thread while running" case.

**Tech Stack:** C++20, CMake, PCSX2 internal APIs (`VMManager`, `memSavingState`/`memLoadingState`, `EmuFolders`, `MemoryCardFile`), libretro C ABI. macOS arm64 dev builds, universal for ship.

**Spec:** [`docs/superpowers/specs/2026-05-12-pcsx2-libretro-savestate-memcard-design.md`](../specs/2026-05-12-pcsx2-libretro-savestate-memcard-design.md) (commit `8cb579c`)

**Predecessor plan reference:** [`docs/superpowers/plans/2026-05-12-pcsx2-libretro-analog-input.md`](2026-05-12-pcsx2-libretro-analog-input.md) (SP5.5).

**Testing model:** No unit tests — the pcsx2-libretro repo has no test target, mirroring SP4/SP5/SP5.5 precedent. Every code task ends with a build-verify (`cmake --build`). Final Task 6 is the manual R&C 2 smoke test suite from the spec.

**Working directory:** `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `retronest-libretro`, local-only — no fork remote, rebase workflow against `upstream/master`).

**Build command (every code task ends with):**

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && /opt/homebrew/bin/cmake --build build-arm64 --target pcsx2_libretro -j 4 2>&1 | tail -20
```

Expected: build succeeds with `[100%] Built target pcsx2_libretro`.

**Install for in-game smoke test:**

```sh
cp /Users/mark/Documents/Projects/pcsx2-libretro/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

(This is the arm64-only dev path. Final ship rebuild via `RetroNest-Project/scripts/build-universal.sh` is part of Task 6.)

---

## File Structure

**Modified files (2):**

- `pcsx2-libretro/LibretroFrontend.cpp` — `retro_reset` body; `retro_serialize_size`/`retro_serialize`/`retro_unserialize` bodies; new `GetSaveDirectory()` helper; `SET_MEMORY_MAPS` issue inside `retro_run`; new pause-stable handshake helper; `RETRONEST_STATE_TRACE` env-gated logging.
- `pcsx2-libretro/Settings.cpp` + `pcsx2-libretro/Settings.h` — `InitializeDefaults` signature extended with `save_dir` parameter; replace the memcard-disabled block with real configuration.

**No new files.** No upstream PCSX2 files touched. No RetroNest-side changes.

**Includes to add to LibretroFrontend.cpp:**

- `#include "SaveState.h"` (for `memSavingState`, `memLoadingState`, `VmStateBuffer`)
- `#include "MemoryTypes.h"` (for `eeMem`, `Ps2MemSize::MainRam`)
- `#include "common/Error.h"` (for `Error` struct used by `FreezeInternals`)

The file already includes `VMManager.h`. `libretro.h` is included via `LibretroFrontend.h`.

---

## Task 1 — `retro_reset` delegates to `VMManager::Reset()`

The quickest win. Six-line stub becomes a one-line delegate. No threading risk — `VMManager::Reset()` is internally safe to call from a non-VM thread while running (flips state to `Resetting` and returns; EE thread does the actual reset at next event-test boundary, see `pcsx2/VMManager.cpp:1762-1814`).

**Files:**
- Modify: `pcsx2-libretro/LibretroFrontend.cpp:188-191`

### Step 1.1: Replace the `retro_reset` body

In `pcsx2-libretro/LibretroFrontend.cpp`, find:

```cpp
RETRO_API void retro_reset(void)
{
    FrontendLog(RETRO_LOG_INFO, "retro_reset — no-op in skeleton");
}
```

Replace with:

```cpp
RETRO_API void retro_reset(void)
{
    if (!VMManager::HasValidVM())
    {
        FrontendLog(RETRO_LOG_INFO, "retro_reset called with no valid VM — ignoring");
        return;
    }
    FrontendLog(RETRO_LOG_INFO, "retro_reset → VMManager::Reset()");
    VMManager::Reset();
}
```

### Step 1.2: Build

Run:

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && /opt/homebrew/bin/cmake --build build-arm64 --target pcsx2_libretro -j 4 2>&1 | tail -20
```

Expected: `[100%] Built target pcsx2_libretro`.

### Step 1.3: Commit

Run:

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && git add pcsx2-libretro/LibretroFrontend.cpp && git commit -m "$(cat <<'EOF'
SP6 task 1: retro_reset → VMManager::Reset()

VMManager::Reset() is internally safe to call from outside the VM
thread while running — it flips state to Resetting and returns
immediately; the EE thread does the actual reset at its next
event-test boundary. If the VM is Paused at call time, Reset() does
the work inline (also safe from any thread).

Replaces the SP1 logging stub. Now wired for both the frontend's
hardware-reset UI path AND RetroAchievements hardcore-mode-enable
(event 14) which RcheevosRuntime already invokes via
g_syms->retro_reset() in RetroNest's rcheevos_runtime.cpp:189-190.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2 — `SET_MEMORY_MAPS` for EE main RAM (unlocks PS2 RetroAchievements)

Issues one memory descriptor — 32 MB EE main RAM at PS2-physical 0x00000000 — exactly once, on the first `retro_run` iteration where the VM reports `Running`. We can't issue from `retro_load_game` end-of-success because `eeMem->Main` is lazily allocated during boot. The gate co-locates with the existing one-shot `g_logged_running` block at `LibretroFrontend.cpp:199-209`.

**Files:**
- Modify: `pcsx2-libretro/LibretroFrontend.cpp` — add include for `MemoryTypes.h`; add `g_memory_map_issued` atomic; extend the first-Running block in `retro_run`; reset the flag on `retro_unload_game`.

### Step 2.1: Add the includes and atomic flag

In `pcsx2-libretro/LibretroFrontend.cpp`, add to the existing include block near the top of the file (alongside `VMManager.h`):

```cpp
#include "MemoryTypes.h"   // eeMem, Ps2MemSize::MainRam
```

Find the existing `g_logged_running` declaration (around line 92-93):

```cpp
// Atomic, used by retro_run to log VM state transitions only once.
std::atomic<bool> g_logged_running{false};
```

Replace with:

```cpp
// Atomic, used by retro_run to log VM state transitions only once.
std::atomic<bool> g_logged_running{false};

// Atomic, used by retro_run to issue SET_MEMORY_MAPS exactly once per
// loaded game. Reset to false in retro_unload_game so the next loaded
// game re-issues with fresh pointers (eeMem may be reallocated across
// VM init/shutdown cycles).
std::atomic<bool> g_memory_map_issued{false};
```

### Step 2.2: Extend the first-Running block in `retro_run`

In `pcsx2-libretro/LibretroFrontend.cpp`, find the existing block in `retro_run`:

```cpp
    // One-shot log when VM first reports Running with a non-zero CRC.
    if (!g_logged_running.load() && VMManager::GetState() == VMState::Running)
    {
        const u32 crc = VMManager::GetCurrentCRC();
        if (crc != 0)
        {
            FrontendLog(RETRO_LOG_INFO, "VM RUNNING — title=%s serial=%s crc=0x%08X",
                        VMManager::GetTitle(true).c_str(),
                        VMManager::GetDiscSerial().c_str(),
                        crc);
            g_logged_running.store(true);
        }
    }
```

Add a second one-shot block immediately after it:

```cpp
    // One-shot SET_MEMORY_MAPS issue when VM first reports Running.
    // EE main RAM is the 32 MB region at PS2-physical 0x00000000.
    // RetroAchievements needs this descriptor to read PS2 cheevo
    // memory addresses (without it, every PS2 cheevo silently fails
    // to trigger). RetroNest's env handler at
    // environment_callbacks.cpp:133-163 captures + copies the
    // descriptors and addrspace strings before this call returns, so
    // stack-allocated structs are safe.
    if (!g_memory_map_issued.load() &&
        VMManager::GetState() == VMState::Running &&
        eeMem != nullptr &&
        g_frontend.environ_cb)
    {
        retro_memory_descriptor desc{};
        desc.ptr       = eeMem->Main;
        desc.start     = 0x00000000;          // PS2-physical
        desc.len       = Ps2MemSize::MainRam; // 32 MB
        desc.select    = 0;                   // RA infers from start+len
        desc.flags     = RETRO_MEMDESC_SYSTEM_RAM;
        desc.addrspace = "";                  // unnamed default

        retro_memory_map mm{};
        mm.descriptors     = &desc;
        mm.num_descriptors = 1;

        const bool ok = g_frontend.environ_cb(
            RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mm);
        FrontendLog(ok ? RETRO_LOG_INFO : RETRO_LOG_WARN,
            "SET_MEMORY_MAPS issued: ee_ram_ptr=%p len=%u %s",
            desc.ptr, static_cast<unsigned>(desc.len),
            ok ? "(accepted)" : "(frontend returned false)");
        g_memory_map_issued.store(true);
    }
```

### Step 2.3: Reset the flag on `retro_unload_game`

In `pcsx2-libretro/LibretroFrontend.cpp`, find the existing `retro_unload_game` function. Add a single line resetting the flag at the start of the body, e.g.:

```cpp
RETRO_API void retro_unload_game(void)
{
    g_memory_map_issued.store(false);     // re-issue on next game load
    g_logged_running.store(false);        // re-log on next Running
    // ... existing body ...
}
```

(If `g_logged_running.store(false)` is already there, leave it; only add the `g_memory_map_issued` line. Verify by reading the function before editing.)

### Step 2.4: Build

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && /opt/homebrew/bin/cmake --build build-arm64 --target pcsx2_libretro -j 4 2>&1 | tail -20
```

Expected: build succeeds.

### Step 2.5: Commit

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && git add pcsx2-libretro/LibretroFrontend.cpp && git commit -m "$(cat <<'EOF'
SP6 task 2: SET_MEMORY_MAPS for EE main RAM (PS2 RetroAchievements)

Issues a single retro_memory_descriptor — 32 MB EE main RAM at
PS2-physical 0x00000000 — exactly once on the first retro_run frame
that reports VMState::Running. Reset to re-issue in retro_unload_game.

Can't issue from retro_load_game end-of-success because eeMem->Main
is lazily allocated during VM boot; the first-Running frame in
retro_run is the natural one-shot gate (co-located with the existing
g_logged_running block).

RetroNest's env handler at environment_callbacks.cpp:133-163 already
captures + copies the descriptors and addrspace strings before
returning, so stack-allocated structs are safe.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3 — Memory cards: enable slot 1, configure save directory

Replaces the SP6-deferred `Slot{1,2}_Enable=false` block in `Settings.cpp:266-274` with real wiring. Slot 1 enabled with the standard PCSX2 default filename `Mcd001.ps2`; slot 2 explicitly disabled (single-slot is the sensible libretro default; UI exposure deferred to SP7). `Folders/MemoryCards` set under the libretro `save_dir`, giving per-game isolation because RetroNest's adapter scopes each game's `save_dir` under `{game_id}/`.

PCSX2 auto-detects `MemoryCardType::File` from the `.ps2` extension at load time (`MemoryCardFile.cpp:595-599`), so we don't need to write a `Slot1_Type` key.

**Files:**
- Modify: `pcsx2-libretro/LibretroFrontend.cpp` — add `GetSaveDirectory()` helper; pass save dir to `Settings::InitializeDefaults`.
- Modify: `pcsx2-libretro/Settings.h:30` — extend `InitializeDefaults` signature.
- Modify: `pcsx2-libretro/Settings.cpp:130, 266-274` — accept `save_dir` parameter; replace disabled-memcard block.

### Step 3.1: Add `GetSaveDirectory()` helper to LibretroFrontend.cpp

In `pcsx2-libretro/LibretroFrontend.cpp`, find the existing `GetSystemDirectory()` function (around line 73-90). Add immediately after it:

```cpp
// Returns the libretro save directory, or empty string if the host
// does not provide one. Cached after first call.
std::string GetSaveDirectory()
{
    static std::string s_cached;
    static bool s_resolved = false;
    if (s_resolved) return s_cached;

    const char* dir = nullptr;
    if (g_frontend.environ_cb &&
        g_frontend.environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) &&
        dir != nullptr)
    {
        s_cached = dir;
    }
    s_resolved = true;
    return s_cached;
}
```

### Step 3.2: Extend `InitializeDefaults` signature

In `pcsx2-libretro/Settings.h`, find line 30:

```cpp
void InitializeDefaults(const std::string& system_dir);
```

Replace with:

```cpp
void InitializeDefaults(const std::string& system_dir,
                        const std::string& save_dir);
```

In `pcsx2-libretro/Settings.cpp:130`, find:

```cpp
void InitializeDefaults(const std::string& system_dir)
{
```

Replace with:

```cpp
void InitializeDefaults(const std::string& system_dir,
                        const std::string& save_dir)
{
```

### Step 3.3: Wire memcard configuration in Settings.cpp

In `pcsx2-libretro/Settings.cpp`, find the existing block at line 266-274:

```cpp
    // Disable memory cards (SP6 will wire these properly). Avoids file
    // sharing violations and unnecessary I/O during VM boot.
    for (int i = 0; i < 2; ++i)
    {
        const std::string enable_key = "Slot" + std::to_string(i + 1) + "_Enable";
        const std::string file_key   = "Slot" + std::to_string(i + 1) + "_Filename";
        g_si.SetBoolValue("MemoryCards", enable_key.c_str(), false);
        g_si.SetStringValue("MemoryCards", file_key.c_str(), "");
    }
```

Replace with:

```cpp
    // SP6: configure memory cards.
    //
    // Slot 1: enabled, file-image type, "Mcd001.ps2" (PCSX2 standard).
    //   PCSX2 auto-detects MemoryCardType::File from the .ps2 extension
    //   at MemoryCardFile.cpp load time, and auto-creates the file on
    //   first save. No Slot1_Type key needed.
    //
    // Slot 2: disabled. Single-slot is the libretro convention; SP7
    //   settings UI may expose a toggle later.
    //
    // Folders/MemoryCards: rooted at libretro save_dir so each
    //   {game_id}/ scope gets its own memcard image (RetroNest's
    //   adapter is responsible for the per-game scoping in save_dir).
    //   Empty save_dir falls back to PCSX2's default ("memcards"
    //   under DataRoot) — usable but not per-game-isolated.
    if (!save_dir.empty())
    {
        const std::string memcards_dir = save_dir + "/memcards";
        g_si.SetStringValue("Folders", "MemoryCards", memcards_dir.c_str());
    }
    else
    {
        FrontendLog(RETRO_LOG_WARN,
            "Host did not provide save_dir — memcards will use PCSX2 default location");
    }

    g_si.SetBoolValue  ("MemoryCards", "Slot1_Enable",   true);
    g_si.SetStringValue("MemoryCards", "Slot1_Filename", "Mcd001.ps2");
    g_si.SetBoolValue  ("MemoryCards", "Slot2_Enable",   false);
    g_si.SetStringValue("MemoryCards", "Slot2_Filename", "Mcd002.ps2");
```

### Step 3.4: Update the call site to pass save_dir

In `pcsx2-libretro/LibretroFrontend.cpp:305`, find:

```cpp
    Pcsx2Libretro::Settings::InitializeDefaults(system_dir);
```

Replace with:

```cpp
    const std::string save_dir = GetSaveDirectory();
    Pcsx2Libretro::Settings::InitializeDefaults(system_dir, save_dir);
```

### Step 3.5: Build

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && /opt/homebrew/bin/cmake --build build-arm64 --target pcsx2_libretro -j 4 2>&1 | tail -20
```

Expected: build succeeds.

### Step 3.6: Commit

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && git add pcsx2-libretro/LibretroFrontend.cpp pcsx2-libretro/Settings.cpp pcsx2-libretro/Settings.h && git commit -m "$(cat <<'EOF'
SP6 task 3: memory card slot 1 enabled, rooted at libretro save_dir

Replaces the SP6-deferred Slot{1,2}_Enable=false block in
Settings.cpp:266-274 with real configuration:

- Slot 1: enabled, "Mcd001.ps2" (PCSX2 standard 8 MB file image;
  MemoryCardType auto-detected from extension at MemoryCardFile.cpp
  load time, auto-created on first save).
- Slot 2: explicitly disabled (single-slot is the libretro
  convention; SP7 may expose a toggle).
- Folders/MemoryCards: rooted under libretro save_dir, giving
  per-game isolation via RetroNest's {game_id}/save_dir scoping.

Empty save_dir falls back to PCSX2's default ("memcards" under
DataRoot) — usable but not per-game-isolated; warning logged.

Adds GetSaveDirectory() helper mirroring GetSystemDirectory(), and
extends Settings::InitializeDefaults signature with a save_dir
parameter.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4 — Save state: probe-once + `memSavingState`/`memLoadingState` (DEFERRED TO SP6.5)

> **2026-05-12 update:** This task was implemented at commit `2eddc63de` and reverted at `f164c4f5f`. Code review found the spec's "FreezeBios() + FreezeInternals()" assumption is incomplete — the canonical PCSX2 save path also iterates `SavestateEntries[]` (EE main RAM, IOP RAM, VU mem, SPU2, PAD, GS, Achievements) via `entry->FreezeOut(saveme)` (`SaveState.cpp:739-753`). The symmetric load via `FreezeIn` requires a `zip_file_t*` not a flat buffer — see `BaseSavestateEntry::FreezeIn(zip_file_t*)` at `SaveState.cpp:469`. The implementation as specified would have produced valid-looking saves that didn't actually round-trip game state.
>
> The Task 4 content below remains as a reference for SP6.5, which will choose between:
> - **In-memory zip** via libzip's `zip_source_function` backing the zip with a `VmStateBuffer`, forcing `ZIP_CM_STORE` everywhere for deterministic size — ~150 lines of libzip plumbing.
> - **Disk-temp-file** via `SaveState_ZipToDisk` / `SaveState_UnzipFromDisk` with `RETRO_ENVIRONMENT_SET_SAVE_STATE_INFO_OVERRIDE` declaring a 64 MB worst-case bound and pad-to-size — ~30 lines, but variable compression and disk I/O per save.
>
> SP6 ships without save state. SP6.5 will re-brainstorm and re-spec the container choice properly.

The meat of SP6. Three pieces: a pause-stable handshake helper (shared by all three serialize entry points), the probe-once strategy for `retro_serialize_size`, and the actual `retro_serialize`/`retro_unserialize` bodies.

**Architectural risk:** calling `FreezeInternals` from the libretro callback thread (= the host frontend's main thread) while PCSX2's EE/IOP/MTGS/SPU2/VU threads are mid-instruction is undefined behavior. PCSX2-Qt's path calls it from the EE thread at an event-test boundary. From outside, the spec proposes a `VMManager::SetPaused(true)` + wait-for-`Paused` handshake, mirroring PCSX2-Qt's "save state while running" UI. This task prototypes it and falls back to a deferred-job approach if unstable.

**Files:**
- Modify: `pcsx2-libretro/LibretroFrontend.cpp` — add `SaveState.h` + `common/Error.h` includes; add a static `g_serialize_size` atomic; add helpers `WaitForVmPaused`/`ResumeVm`; implement `retro_serialize_size`, `retro_serialize`, `retro_unserialize`.

### Step 4.1: Add the includes

In `pcsx2-libretro/LibretroFrontend.cpp`, add to the existing include block (alongside `VMManager.h` / `MemoryTypes.h`):

```cpp
#include "SaveState.h"      // memSavingState, memLoadingState
#include "common/Error.h"   // Error
```

### Step 4.2: Add the probe-size atomic and pause-stable helpers

In `pcsx2-libretro/LibretroFrontend.cpp`, near the existing `g_logged_running` / `g_memory_map_issued` atomics, add:

```cpp
// Atomic, used by retro_serialize_size to return a constant after
// the first successful probe. 0 means "VM not yet ready, try again".
// Reset to 0 in retro_unload_game (different games = different sizes).
std::atomic<size_t> g_serialize_size{0};
```

Then in an anonymous namespace alongside `GetSystemDirectory` / `GetSaveDirectory`, add the pause-stable handshake helpers:

```cpp
// Pauses the VM and waits for it to reach VMState::Paused. Returns
// the state observed BEFORE the pause was requested, so the caller
// can restore it (we don't want to leave a Paused VM Running, or
// vice-versa).
//
// Polls in 1 ms increments up to a 200 ms ceiling. PCSX2-Qt uses the
// same handshake for its "save state while running" UI; the EE
// thread reaches the next event-test typically within a single
// frame (~16 ms). 200 ms is generous — if we hit the ceiling
// something is wrong (deep MTGS stall, infinite loop in interpreter)
// and we abort the save rather than block the host indefinitely.
//
// Caller must call ResumeVm(prev_state) regardless of whether the
// serialize succeeded.
//
// Returns VMState::Shutdown sentinel if pause failed (VM exited
// during the wait, etc.); caller should bail.
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
            // (Task 5 adds a trace line here.)
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

// Restores the VM to prev_state (the value returned by
// WaitForVmPaused). If prev was Running, un-pause. Otherwise leave
// as-is.
void ResumeVm(VMState prev_state)
{
    if (prev_state == VMState::Running &&
        VMManager::GetState() == VMState::Paused)
    {
        VMManager::SetPaused(false);
    }
}
```

If `<chrono>` and `<thread>` aren't already included at the top of the file (they should be — `retro_run` uses chrono literals at line 219), add them now.

### Step 4.3: Implement `retro_serialize_size` (probe-once)

In `pcsx2-libretro/LibretroFrontend.cpp:260`, find:

```cpp
RETRO_API size_t retro_serialize_size(void) { return 0; }
```

Replace with:

```cpp
RETRO_API size_t retro_serialize_size(void)
{
    // Probe-once: serialize state into a scratch buffer, cache its
    // size, return that constant forever. Returns 0 pre-Running so
    // the frontend retries later (spec-legal).
    //
    // PCSX2 raw save-state size is deterministic per (build, ELF).
    // Variable-size sources (compression, screenshots, zip headers)
    // aren't in memSavingState's output — it's pure raw bytes.
    const size_t cached = g_serialize_size.load();
    if (cached != 0) return cached;
    if (!VMManager::HasValidVM()) return 0;
    if (VMManager::GetState() != VMState::Running) return 0;

    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown)
    {
        FrontendLog(RETRO_LOG_WARN, "retro_serialize_size: pause handshake failed");
        return 0;
    }

    size_t probed = 0;
    {
        SaveStateBase::VmStateBuffer probe;
        memSavingState s(probe);
        Error err;
        if (s.FreezeBios() && s.FreezeInternals(&err) && !s.HasError())
        {
            probed = probe.size();
        }
        else
        {
            FrontendLog(RETRO_LOG_WARN,
                "retro_serialize_size: probe FreezeInternals failed (%s)",
                err.GetDescription().c_str());
        }
    }

    ResumeVm(prev);

    if (probed == 0) return 0;
    g_serialize_size.store(probed);
    FrontendLog(RETRO_LOG_INFO,
        "retro_serialize_size: probed=%zu bytes (cached)", probed);
    return probed;
}
```

### Step 4.4: Implement `retro_serialize`

Find the next stub:

```cpp
RETRO_API bool   retro_serialize(void*, size_t)         { return false; }
```

Replace with:

```cpp
RETRO_API bool retro_serialize(void* dst, size_t len)
{
    if (!dst) return false;
    const size_t expected = g_serialize_size.load();
    if (expected == 0) return false;       // probe hasn't run yet
    if (len < expected) return false;      // frontend allocation bug
    if (!VMManager::HasValidVM()) return false;

    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown) return false;

    bool ok = false;
    {
        SaveStateBase::VmStateBuffer buf;
        memSavingState s(buf);
        Error err;
        if (s.FreezeBios() && s.FreezeInternals(&err) && !s.HasError())
        {
            if (buf.size() > len)
            {
                FrontendLog(RETRO_LOG_ERROR,
                    "retro_serialize: produced %zu bytes but caller buffer is %zu — "
                    "probe-once assumption violated; not writing", buf.size(), len);
            }
            else
            {
                std::memcpy(dst, buf.data(), buf.size());
                if (buf.size() < len)
                {
                    std::memset(static_cast<u8*>(dst) + buf.size(), 0,
                                len - buf.size());
                }
                ok = true;
            }
        }
        else
        {
            FrontendLog(RETRO_LOG_WARN,
                "retro_serialize: FreezeInternals failed (%s)",
                err.GetDescription().c_str());
        }
    }

    ResumeVm(prev);
    return ok;
}
```

### Step 4.5: Implement `retro_unserialize`

Find:

```cpp
RETRO_API bool   retro_unserialize(const void*, size_t) { return false; }
```

Replace with:

```cpp
RETRO_API bool retro_unserialize(const void* src, size_t len)
{
    if (!src || len == 0) return false;
    if (!VMManager::HasValidVM()) return false;

    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown) return false;

    bool ok = false;
    {
        // Copy src bytes into a VmStateBuffer (memLoadingState reads
        // from the vector). The trailing zero-padding from
        // retro_serialize is harmless — memLoadingState reads only
        // what FreezeInternals asks for, ignoring the tail.
        SaveStateBase::VmStateBuffer buf(
            static_cast<const u8*>(src),
            static_cast<const u8*>(src) + len);
        memLoadingState s(buf);
        Error err;
        ok = s.FreezeBios() && s.FreezeInternals(&err) && !s.HasError();
        if (!ok)
        {
            FrontendLog(RETRO_LOG_WARN,
                "retro_unserialize: FreezeInternals failed (%s) — VM state may be corrupt",
                err.GetDescription().c_str());
        }
    }

    ResumeVm(prev);
    return ok;
}
```

### Step 4.6: Reset `g_serialize_size` on `retro_unload_game`

In `pcsx2-libretro/LibretroFrontend.cpp`, find the `retro_unload_game` body where Task 2 added `g_memory_map_issued.store(false)`. Add immediately after it:

```cpp
    g_serialize_size.store(0);            // re-probe on next game load
```

### Step 4.7: Build

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && /opt/homebrew/bin/cmake --build build-arm64 --target pcsx2_libretro -j 4 2>&1 | tail -20
```

Expected: build succeeds. Common errors to watch for:
- `'Error' was not declared` → `common/Error.h` include missing.
- `'memSavingState' was not declared` → `SaveState.h` include missing.
- `no member named 'VmStateBuffer' in 'SaveStateBase'` → wrong qualified name; it's defined inside the class as `using VmStateBuffer = std::vector<u8>;` (see `SaveState.h:72`).

### Step 4.8: Install for live-run smoke

```sh
cp /Users/mark/Documents/Projects/pcsx2-libretro/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

Launch R&C 2 in RetroNest. Verify in RetroNest's stderr log:

- `retro_serialize_size: probed=… bytes (cached)` line appears once the VM reaches Running.
- No `WARN` lines about pause handshake or `FreezeInternals failed`.
- Game continues running normally (handshake doesn't visibly hitch).

If you see hitches: the 200 ms pause deadline is being approached. Run with `RETRONEST_STATE_TRACE=1` (added in Task 5) for boundary timing.

If pause handshake fails repeatedly: **prototype fallback** — instead of `SetPaused(true)`, post a job onto the EE thread via PCSX2's `VMManager::Internal::ExecuteJobInsideEE` (or equivalent — confirm during prototype) and wait on a condition variable. This is the (b) fallback from spec Section "Pause-stable handshake". Document the switch in the task commit message.

### Step 4.9: Commit

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && git add pcsx2-libretro/LibretroFrontend.cpp && git commit -m "$(cat <<'EOF'
SP6 task 4: save state via memSavingState/memLoadingState

Wires retro_serialize_size / retro_serialize / retro_unserialize to
PCSX2's in-memory raw state primitives (SaveState.h:338,350).

retro_serialize_size: probe-once strategy. On first call while
VMState::Running, pauses VM (200 ms handshake), runs a scratch
memSavingState, caches its buffer.size() in g_serialize_size atomic,
returns. Subsequent calls return the cached value. Returns 0
pre-Running (spec-legal — frontend retries). Reset to 0 in
retro_unload_game so different games re-probe.

retro_serialize: pauses VM, runs memSavingState into a temp
VmStateBuffer, memcpy's into caller's dst, zero-pads tail if
buf.size() < len, returns true on success. Returns false if buf
exceeds len (probe-once assumption violation — should not happen).

retro_unserialize: pauses VM, copies src into a VmStateBuffer, runs
memLoadingState. Trailing zero-padding from retro_serialize is
ignored (memLoadingState reads only what FreezeInternals requests).

The pause-stable handshake (WaitForVmPaused/ResumeVm helpers) uses
VMManager::SetPaused + 1 ms polling up to a 200 ms ceiling, mirroring
PCSX2-Qt's "save state while running" UI handshake. Aborts with a
warning if the deadline is exceeded rather than blocking the host.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5 — `RETRONEST_STATE_TRACE` env-gated diagnostics (reduced scope)

**SP6.5 reduction:** save-state and pause-handshake boundaries (2, 3, 6) are deferred along with the save-state implementation. SP6's Task 5 now only adds boundary 4 (`retro_reset` invocation). Boundary 5 (`SET_MEMORY_MAPS` issue) is already unconditionally logged by Task 2 — no new code there. Net effect: ~5 lines of new code in `retro_reset`.

Mirrors the `RETRONEST_AUDIO_TRACE` / `RETRONEST_INPUT_TRACE` pattern from SP4/SP5: zero overhead when unset.

**Files:**
- Modify: `pcsx2-libretro/LibretroFrontend.cpp` — add a `IsStateTraceEnabled()` cached helper; add one trace line in `retro_reset`.

### Step 5.1: Add the cached env flag

In `pcsx2-libretro/LibretroFrontend.cpp`, in the same anonymous namespace as `GetSystemDirectory` / `GetSaveDirectory`, add:

```cpp
// RETRONEST_STATE_TRACE: env-gated trace at save-state, reset, and
// memory-map boundaries. Zero overhead when unset (single getenv at
// first call, cached bool thereafter). Mirrors RETRONEST_AUDIO_TRACE
// (SP4) and RETRONEST_INPUT_TRACE (SP5).
bool IsStateTraceEnabled()
{
    static const bool s_enabled = (std::getenv("RETRONEST_STATE_TRACE") != nullptr);
    return s_enabled;
}
```

### Step 5.2: Add the `retro_reset` trace line

Inside `retro_reset` (the body from Task 1), insert ONE trace line immediately after the `HasValidVM` early-return guard but before the existing `"retro_reset → VMManager::Reset()"` log line. The current Task 1 body is:

```cpp
RETRO_API void retro_reset(void)
{
    if (!VMManager::HasValidVM())
    {
        FrontendLog(RETRO_LOG_INFO, "retro_reset called with no valid VM — ignoring");
        return;
    }
    FrontendLog(RETRO_LOG_INFO, "retro_reset → VMManager::Reset()");
    VMManager::Reset();
}
```

Replace with:

```cpp
RETRO_API void retro_reset(void)
{
    if (!VMManager::HasValidVM())
    {
        FrontendLog(RETRO_LOG_INFO, "retro_reset called with no valid VM — ignoring");
        return;
    }
    if (IsStateTraceEnabled())
        FrontendLog(RETRO_LOG_INFO,
            "[STATE_TRACE] retro_reset state=%d",
            static_cast<int>(VMManager::GetState()));
    FrontendLog(RETRO_LOG_INFO, "retro_reset → VMManager::Reset()");
    VMManager::Reset();
}
```

**Other boundaries deferred to SP6.5:**
- Boundaries 1, 2, 3 (serialize/unserialize entry/exit timing): land with the SP6.5 save-state implementation
- Boundary 5 (SET_MEMORY_MAPS issue): already unconditionally logged from Task 2 — no new code
- Boundary 6 (pause-stable handshake transitions): the `WaitForVmPaused`/`ResumeVm` helpers were reverted along with Task 4; will return in SP6.5

### Step 5.3: Build

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && /opt/homebrew/bin/cmake --build build-arm64 --target pcsx2_libretro -j 4 2>&1 | tail -20
```

Expected: build succeeds.

### Step 5.4: Verify trace by triggering a reset in R&C 2

The user will run this manually as part of Task 6's smoke tests. Expected behavior: with `RETRONEST_STATE_TRACE=1` set, triggering `retro_reset` (frontend reset UI or RA hardcore mode toggle) produces a `[STATE_TRACE] retro_reset state=N` line; without the env set, no `[STATE_TRACE]` lines appear.

### Step 5.5: Commit

```sh
cd /Users/mark/Documents/Projects/pcsx2-libretro && git add pcsx2-libretro/LibretroFrontend.cpp && git commit -m "$(cat <<'EOF'
SP6 task 5: RETRONEST_STATE_TRACE env-gated diagnostics (reset only)

Adds the IsStateTraceEnabled() cached-bool helper and one trace line
in retro_reset. Mirrors RETRONEST_AUDIO_TRACE (SP4) /
RETRONEST_INPUT_TRACE (SP5): zero overhead when unset (single getenv
cached as static bool).

Save-state and pause-handshake trace boundaries are deferred to SP6.5
along with the save-state implementation. SET_MEMORY_MAPS issue is
already unconditionally logged from Task 2 — no new code there.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6 — End-to-end smoke tests + universal rebuild for ship

Validates the three SP6 smoke tests (reset, memcard persistence, PS2 RA), then rebuilds universal so the change ships in production. Mirrors SP5.5 Task 6 cadence. Save-state smoke tests 1-2 are deferred to SP6.5.

**Files:** none modified in this task (verification + rebuild only).

### Step 6.1: Universal rebuild

Build both arm64 and x86_64 slices, then lipo-merge:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project && ./scripts/build-universal.sh 2>&1 | tail -30
```

Expected: `pcsx2_libretro.dylib` ends up in `~/Documents/RetroNest/emulators/libretro/cores/` as a universal binary.

Verify:

```sh
file ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

Expected: `Mach-O universal binary with 2 architectures: [x86_64:...] [arm64:...]`.

### Step 6.2-6.3: Save-state smoke tests — DEFERRED to SP6.5

Smoke tests 1 (boot persistence / quit-resume) and 2 (mid-session save/load round-trip) require working save state, which was reverted from SP6. They are deferred to SP6.5's verification cycle. The `requestSaveState` plumbing on the RetroNest side remains in place and will resume working once SP6.5 implements `retro_serialize`/`retro_unserialize` properly.

### Step 6.4: Smoke test 3 — `retro_reset`

1. In R&C 2 running.
2. Trigger reset — either via frontend reset UI, or via the RetroAchievements path: enable RA hardcore mode in mid-session (`RcheevosRuntime` invokes `retro_reset` on event 14).
3. **Expected:** VM returns to BIOS / fast-boot startup. No libretro-core restart. RetroNest's window stays open. Game can be played again normally after reset.

### Step 6.5: Smoke test 4 — memory card persistence

1. In R&C 2, progress to a save point (typically end of first mission).
2. Save to memory card slot 1 from the in-game menu.
3. Confirm the save succeeds (no "memcard not detected" error).
4. Verify on disk:

```sh
ls -la ~/Documents/RetroNest/saves/{game_id}/memcards/Mcd001.ps2
```

(Substitute `{game_id}` for R&C 2's actual RetroNest game id; check `~/Documents/RetroNest/saves/` for the directory layout.)

Expected: file exists, ~8 MB.

5. Close RetroNest. Reopen. Launch R&C 2.
6. From the in-game start menu, navigate to "Load Game".
7. **Expected:** the save from step 2 appears in the list. Loading it resumes the game from that point.

### Step 6.6: Smoke test 5 — PS2 RetroAchievements

1. Enable RA in RetroNest settings (if not already), log in.
2. Launch R&C 2.
3. **Expected:** RA loads the cheevo set for R&C 2 (look for `[rcheevos]` log lines and the "RA: 0 of N achievements unlocked" toast). Proves the memory map reached RA via `SET_MEMORY_MAPS`.
4. Trigger a known early cheevo (e.g. first enemy kill, first crate broken).
5. **Expected:** unlock toast appears.

If no toast and the cheevo set loads but never triggers: check RetroNest log for `[rcheevos]` warnings about address translation failures — would indicate the memory descriptor's `start` / `select` mask is wrong. The PS2 EE memory mirror mask convention varies — start with `select=0` and re-evaluate if needed.

### Step 6.7: Update project memory file

Edit `~/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/project_pcsx2_libretro_port.md`:

- Move sub-project 7 (`⏳ Save states + memory cards (SP6)`) from `⏳` to `✅ (PARTIAL — save state deferred to SP6.5)`. Include spec/plan paths, smoke-test status, the SP6.5 deferral note.
- Add a new sub-project line for SP6.5: `⏳ Save state implementation (SP6.5)` with the two architectural options on record (in-memory libzip vs. disk-temp-file).
- Remove the two bullet points from the "Known PCSX2 libretro compat gaps" section that SP6 closes:
  - `retro_reset is a no-op`
  - `RETRO_ENVIRONMENT_SET_MEMORY_MAPS not issued`
- Leave the two SP7-bound entries (`retro_get_region` hardcodes NTSC, `av.timing.fps = 60.0` placeholder).

Also write a new session handoff memory file dated 2026-05-12 for SP6 (the prior `session_handoff_sp55_shipped.md` can be marked superseded or deleted) documenting:
- SP6 commits shipped (1, 2, 3, 5)
- Task 4 revert and SP6.5 deferral
- Memcard verification status from the smoke tests
- Pointer to start SP6.5 next session with brainstorming the save-state container-format choice

### Step 6.8: Final commit (plan-file checkmark)

If you want to mark the plan-file checkboxes as completed, do so and commit on RetroNest:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project && git add docs/superpowers/plans/2026-05-12-pcsx2-libretro-savestate-memcard.md && git commit -m "$(cat <<'EOF'
docs: SP6 plan — mark in-scope tasks complete

Four tasks shipped in pcsx2-libretro@retronest-libretro:
- retro_reset → VMManager::Reset()
- SET_MEMORY_MAPS for EE main RAM (PS2 RA unlocked)
- Memory cards slot 1 enabled under save_dir/memcards/
- RETRONEST_STATE_TRACE diagnostics (reset boundary only)

Save state (Task 4) reverted; deferred to SP6.5 after the
FreezeBios()+FreezeInternals() approach was found incomplete. SP6.5
will spec the container-format choice (in-memory libzip vs.
disk-temp-file).

Three SP6 smoke tests pass on R&C 2 in RetroNest (reset, memcard
persistence, PS2 RetroAchievements). Universal rebuild shipped.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-review: spec coverage (post-Task-4-revert)

| Spec requirement | Task | Notes |
|---|---|---|
| `retro_reset → VMManager::Reset()` | 1 | Direct |
| `SET_MEMORY_MAPS` for EE main RAM (32 MB at PS2-physical 0) | 2 | Issued from retro_run first-Running frame |
| Memcard slot 1 enabled with file image + `Mcd001.ps2` | 3 | Per-game via `save_dir/memcards/` |
| Slot 2 disabled | 3 | Explicit `false` |
| `Folders/MemoryCards` rooted at libretro `save_dir` | 3 | With empty-`save_dir` fallback + warning |
| `retro_serialize_size` probe-once → cached constant | **SP6.5** | Deferred — FreezeBios+FreezeInternals incomplete |
| `retro_serialize` via `memSavingState` | **SP6.5** | Deferred — needs SavestateEntries loop |
| `retro_unserialize` via `memLoadingState` | **SP6.5** | Deferred — needs libzip-from-memory plumbing |
| Pause-stable handshake | **SP6.5** | Deferred along with save state |
| `RETRONEST_STATE_TRACE` env-gated diagnostics (retro_reset only) | 5 | Reduced from six boundaries to one; rest deferred to SP6.5 |
| Manual smoke tests (reset, memcard, RA) | 6 | Three SP6 smoke tests; save-state tests deferred to SP6.5 |
| Universal rebuild | 6.1 | `scripts/build-universal.sh` |
| Out-of-scope items (folder memcards, slot 2 UI, IOP/scratchpad/VU descriptors, thumbnails, `.p2s` interchange) | — | Explicitly not implemented; no task needed |
