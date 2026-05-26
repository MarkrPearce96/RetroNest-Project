# SP8 — Dolphin libretro RetroAchievements + savestates + packaging — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the Dolphin libretro core to v1 feature parity by implementing RetroAchievements memory exposure, savestates, a persistent user directory, and a one-command local deploy.

**Architecture:** Four focused changes in the core repo (`/Users/mark/Documents/Projects/dolphin-libretro`, `libretro` branch): (1) a pure descriptor-builder + `SET_MEMORY_MAPS` emit so rcheevos can read GC/Wii RAM; (2) public `State.h` buffer wrappers + `retro_serialize*` using them under `Core::RunOnCPUThread`; (3) route Dolphin's user dir through the host's save directory; (4) a deploy script. Then a host-side manual verification matrix in `RetroNest-Project`.

**Tech Stack:** C++20, Dolphin core libs, libretro ABI (`libretro.h`), CMake + Ninja, macOS/Metal. Standalone `clang++` test for pure logic; manual hardware acceptance for emulator-dependent paths (the established SP2–SP7 pattern).

---

## Context the executor needs

- **Build (incremental, arm64):** `cd /Users/mark/Documents/Projects/dolphin-libretro && cmake --build build-libretro --target dolphin_libretro`. The `build-libretro/` and `build-libretro-x86_64/` dirs are already configured (see `memory/dolphin-libretro-build-setup`). Only re-run the full `cmake -B …` configure if you add a new source file to `CMakeLists.txt` (Task 5 step), in which case reconfigure that one build dir.
- **x86_64 build (needed only for deploy in Task 7):** `PATH=/usr/local/bin:$PATH arch -x86_64 /usr/local/bin/cmake --build build-libretro-x86_64 --target dolphin_libretro`.
- **Diagnostics:** launching RetroNest with `RETRONEST_DOLPHIN_LOG=1` routes the core's `Environment::Log` + Dolphin LogManager to stderr; RetroNest captures the core's stderr to `/tmp/retronest.log`. Use this to read the `[MemoryMap]` / `[Savestate]` lines added below.
- **rcheevos facts that drive Task 1–2** (`RetroNest-Project/cpp/build-arm64/_deps/rcheevos-src/src/`):
  - GameCube (console **16**): 1 region — 24 MB @ real `0x80000000` (`consoleinfo.c:519`).
  - Wii (console **19**): MEM1 24 MB @ `0x80000000` + MEM2 64 MB @ `0x90000000` (`consoleinfo.c:972`).
  - `rc_libretro.c:513-516`: when `descriptor.select == 0`, matching uses `addr >= start && addr < start+len` — **no power-of-two requirement**, so 24 MB MEM1 with `select=0` is valid.
- **Host RA is already wired:** `DolphinLibretroAdapter::raConsoleId` returns 16/19; `RcheevosRuntime` consumes either the memory map or `retro_get_memory_data(SYSTEM_RAM)`. No host code changes expected — Task 7 is verification.

## File structure

**Core repo (`dolphin-libretro`):**
- Create `Source/Core/DolphinLibretro/MemoryMap.h` / `.cpp` — pure builder: RAM layout → `std::vector<retro_memory_descriptor>`. No Dolphin engine includes (so it unit-tests standalone).
- Create `Source/Core/DolphinLibretro/tools/test_memory_map.cpp` — standalone test for the builder.
- Modify `Source/Core/DolphinLibretro/LibretroFrontend.cpp` — user dir (Task 1), emit map + `retro_get_memory_data` (Task 3), `retro_serialize*` (Task 5).
- Modify `Source/Core/Core/State.h` / `State.cpp` — expose `SaveToBuffer`/`LoadFromBuffer` (Task 4).
- Modify `Source/Core/DolphinLibretro/CMakeLists.txt` — add `MemoryMap.cpp`.
- Create `Source/Core/DolphinLibretro/tools/deploy.sh` — universal lipo + Sys install (Task 6).

**Host repo (`RetroNest-Project`):**
- No source changes expected. Task 7 records the acceptance run.

---

## Task 1: Persistent user directory (do first — unblocks save/RA testing)

**Files:**
- Modify: `Source/Core/DolphinLibretro/LibretroFrontend.cpp:118-124` (`retro_init`)

- [ ] **Step 1: Replace the hardcoded `/tmp` user dir with the host save directory**

In `retro_init`, replace this block:

```cpp
    // Set Dolphin's User directory. SP2 uses a fixed /tmp path; SP3 should
    // route this through RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY so the host
    // can put it under its own data root.
    UICommon::SetUserDirectory("/tmp/dolphin-libretro-user");
    UICommon::Init();
```

with:

```cpp
    // SP8: root Dolphin's User dir under the host-provided save directory so GC
    // memcards / Wii NAND / SD images / savestates persist across runs. The host
    // maps RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY to a stable per-emulator/per-system
    // dir. env_cb is available here (retro_set_environment runs before retro_init).
    // Fall back to the old /tmp path so boot never depends on the host answering.
    std::string user_dir = "/tmp/dolphin-libretro-user";
    if (auto cb = DolphinLibretro::Environment::GetEnvironmentCallback())
    {
        const char* save_dir = nullptr;
        if (cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir && *save_dir)
            user_dir = std::string(save_dir) + "/dolphin-libretro-user";
    }
    DolphinLibretro::Environment::Log(RETRO_LOG_INFO, "[Frontend] user dir: %s", user_dir.c_str());
    UICommon::SetUserDirectory(user_dir);
    UICommon::Init();
```

- [ ] **Step 2: Build**

Run: `cd /Users/mark/Documents/Projects/dolphin-libretro && cmake --build build-libretro --target dolphin_libretro`
Expected: builds cleanly, `dolphin_libretro.dylib` relinked.

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/LibretroFrontend.cpp
git commit -m "SP8: root Dolphin user dir under host GET_SAVE_DIRECTORY"
```

> Functional verification (memcard persists across restart) happens in Task 7 once the core is deployed.

---

## Task 2: Memory-descriptor builder (pure logic, TDD)

**Files:**
- Create: `Source/Core/DolphinLibretro/MemoryMap.h`
- Create: `Source/Core/DolphinLibretro/MemoryMap.cpp`
- Test: `Source/Core/DolphinLibretro/tools/test_memory_map.cpp`

- [ ] **Step 1: Write the header**

Create `Source/Core/DolphinLibretro/MemoryMap.h`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP8: pure builder that turns a booted system's RAM layout into the libretro
// memory descriptors rcheevos expects for GameCube (console 16) and Wii (19).
// No Dolphin engine dependencies — unit-testable standalone (see tools/test_memory_map.cpp).

#pragma once

#include <cstdint>
#include <vector>

#include "libretro.h"

namespace DolphinLibretro::MemoryMap
{
// Host pointers + sizes for the booted system's RAM. mem2 is Wii-only.
struct RamLayout
{
    bool        is_wii    = false;
    void*       mem1      = nullptr;  // MEM1 / main RAM (MemoryManager::GetRAM())
    std::uint32_t mem1_size = 0;      // GetRamSizeReal() — 24 MiB
    void*       mem2      = nullptr;  // MEM2 (Wii only, GetEXRAM())
    std::uint32_t mem2_size = 0;      // GetExRamSizeReal() — 64 MiB
};

// MEM1 @ emulated address 0x80000000 always; MEM2 @ 0x90000000 when is_wii.
// select=0 (range-matched by rcheevos). Returns empty if mem1 is null/zero-size.
std::vector<retro_memory_descriptor> BuildDescriptors(const RamLayout& layout);

}  // namespace DolphinLibretro::MemoryMap
```

- [ ] **Step 2: Write the failing test**

Create `Source/Core/DolphinLibretro/tools/test_memory_map.cpp`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for DolphinLibretro::MemoryMap::BuildDescriptors.
// Manual compile (no Dolphin libs needed):
//
//   cd Source/Core/DolphinLibretro/tools
//   clang++ -std=c++20 -I.. test_memory_map.cpp ../MemoryMap.cpp \
//       -o test_memory_map && ./test_memory_map

#include "../MemoryMap.h"

#include <cstdio>

using namespace DolphinLibretro::MemoryMap;

static int failures = 0;
static void ck(const char* l, bool ok)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", l);
    if (!ok) ++failures;
}

int main()
{
    // Fake host pointers — never dereferenced, only carried into descriptors.
    auto* mem1 = reinterpret_cast<void*>(0x1000);
    auto* mem2 = reinterpret_cast<void*>(0x2000);

    // ── GameCube: one MEM1 descriptor ──
    {
        RamLayout gc{};
        gc.is_wii = false; gc.mem1 = mem1; gc.mem1_size = 24u * 1024 * 1024;
        const auto d = BuildDescriptors(gc);
        ck("gc: 1 descriptor", d.size() == 1);
        ck("gc: ptr == mem1", d[0].ptr == mem1);
        ck("gc: start 0x80000000", d[0].start == 0x80000000u);
        ck("gc: len 24MiB", d[0].len == 24u * 1024 * 1024);
        ck("gc: select 0", d[0].select == 0);
        ck("gc: SYSTEM_RAM flag", (d[0].flags & RETRO_MEMDESC_SYSTEM_RAM) != 0);
    }

    // ── Wii: MEM1 + MEM2 ──
    {
        RamLayout wii{};
        wii.is_wii = true;
        wii.mem1 = mem1; wii.mem1_size = 24u * 1024 * 1024;
        wii.mem2 = mem2; wii.mem2_size = 64u * 1024 * 1024;
        const auto d = BuildDescriptors(wii);
        ck("wii: 2 descriptors", d.size() == 2);
        ck("wii: mem1 @ 0x80000000", d[0].start == 0x80000000u && d[0].ptr == mem1);
        ck("wii: mem2 @ 0x90000000", d[1].start == 0x90000000u && d[1].ptr == mem2);
        ck("wii: mem2 len 64MiB", d[1].len == 64u * 1024 * 1024);
    }

    // ── Guard: no RAM → no descriptors ──
    {
        RamLayout empty{};
        ck("empty: 0 descriptors", BuildDescriptors(empty).empty());
    }

    std::printf("\n%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
```

- [ ] **Step 3: Run the test to verify it fails (no implementation yet)**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools
clang++ -std=c++20 -I.. test_memory_map.cpp ../MemoryMap.cpp -o test_memory_map
```
Expected: **link/compile error** — `BuildDescriptors` is declared but not defined (no `MemoryMap.cpp` yet).

- [ ] **Step 4: Write the implementation**

Create `Source/Core/DolphinLibretro/MemoryMap.cpp`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryMap.h"

namespace DolphinLibretro::MemoryMap
{
static retro_memory_descriptor MakeRam(void* ptr, std::uint32_t size, std::size_t start)
{
    retro_memory_descriptor d{};
    d.flags      = RETRO_MEMDESC_SYSTEM_RAM;
    d.ptr        = ptr;
    d.offset     = 0;
    d.start      = start;
    d.select     = 0;  // rcheevos range-matches when select==0 (rc_libretro.c:513)
    d.disconnect = 0;
    d.len        = size;
    d.addrspace  = nullptr;
    return d;
}

std::vector<retro_memory_descriptor> BuildDescriptors(const RamLayout& layout)
{
    std::vector<retro_memory_descriptor> out;
    if (!layout.mem1 || layout.mem1_size == 0)
        return out;  // RAM not allocated yet — caller retries later

    // MEM1 — GameCube + Wii main RAM at emulated 0x80000000.
    out.push_back(MakeRam(layout.mem1, layout.mem1_size, 0x80000000u));

    // MEM2 — Wii extended RAM at emulated 0x90000000.
    if (layout.is_wii && layout.mem2 && layout.mem2_size != 0)
        out.push_back(MakeRam(layout.mem2, layout.mem2_size, 0x90000000u));

    return out;
}

}  // namespace DolphinLibretro::MemoryMap
```

- [ ] **Step 5: Run the test to verify it passes**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools
clang++ -std=c++20 -I.. test_memory_map.cpp ../MemoryMap.cpp -o test_memory_map && ./test_memory_map
```
Expected: all `[PASS]` lines, final `OK (0 failures)`, exit 0.

- [ ] **Step 6: Add MemoryMap.cpp to the build**

In `Source/Core/DolphinLibretro/CMakeLists.txt`, the `target_sources(dolphin_libretro PRIVATE …)` block (lines 12-24) ends with `CoreOptionsCore.cpp`. Add `MemoryMap.cpp` as the last entry:

```cmake
    CoreOptionsCore.cpp
    MemoryMap.cpp
)
```

(No new test wiring needed — `test_memory_map.cpp` is compiled manually like `test_core_options.cpp`, not via CMake. The existing `target_include_directories` already lists `${CMAKE_SOURCE_DIR}/Source/Core`, so the new file's `#include "Core/HW/Memmap.h"` etc. resolve.)

- [ ] **Step 7: Build the core (CMake auto-reconfigures on the CMakeLists.txt change)**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro
```
Expected: CMake re-runs configure (it detects the edited `CMakeLists.txt`), then `MemoryMap.cpp` compiles into the dylib. If configure does not trigger, re-run the arm64 configure from `memory/dolphin-libretro-build-setup`, then rebuild.

- [ ] **Step 8: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/MemoryMap.h Source/Core/DolphinLibretro/MemoryMap.cpp \
        Source/Core/DolphinLibretro/tools/test_memory_map.cpp \
        Source/Core/DolphinLibretro/CMakeLists.txt
git commit -m "SP8: add memory-descriptor builder for GC/Wii RA (TDD)"
```

---

## Task 3: Emit SET_MEMORY_MAPS + wire retro_get_memory_data

**Files:**
- Modify: `Source/Core/DolphinLibretro/LibretroFrontend.cpp` (includes, `retro_run`, `retro_unload_game`, `retro_get_memory_data/_size`)

- [ ] **Step 1: Add includes + the one-shot emit state**

Near the existing includes in `LibretroFrontend.cpp`, add:

```cpp
#include "DolphinLibretro/MemoryMap.h"
#include "Core/HW/Memmap.h"
```

In the anonymous namespace (where `s_emu_thread` / `s_wsi` live), add:

```cpp
bool s_memory_map_emitted = false;
```

- [ ] **Step 2: Add a one-shot emit helper + call it from `retro_run`**

Add this helper just above `retro_run`:

```cpp
namespace {

// Emit the RA memory map once RAM is allocated. BootCore is async, so RAM isn't
// ready when retro_load_game returns — but it is by the first rendered frame.
// The host's rcheevos init is gated behind a network achievement-set fetch that
// lands many frames later, so first-frame emit wins the race. GameCube also works
// via the retro_get_memory_data(SYSTEM_RAM) fallback; Wii REQUIRES this map
// (MEM1 + MEM2 are separate allocations).
void MaybeEmitMemoryMap()
{
    if (s_memory_map_emitted)
        return;

    auto& system = Core::System::GetInstance();
    auto& memory = system.GetMemory();
    if (!memory.GetRAM())
        return;  // RAM not allocated yet — try again next frame

    DolphinLibretro::MemoryMap::RamLayout layout{};
    layout.is_wii    = system.IsWii();
    layout.mem1      = memory.GetRAM();
    layout.mem1_size = memory.GetRamSizeReal();
    layout.mem2      = memory.GetEXRAM();
    layout.mem2_size = memory.GetExRamSizeReal();

    const auto descriptors = DolphinLibretro::MemoryMap::BuildDescriptors(layout);
    if (descriptors.empty())
        return;

    retro_memory_map mmap{};
    mmap.descriptors     = descriptors.data();
    mmap.num_descriptors = static_cast<unsigned>(descriptors.size());

    auto cb = DolphinLibretro::Environment::GetEnvironmentCallback();
    if (cb && cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mmap))
    {
        s_memory_map_emitted = true;
        DolphinLibretro::Environment::Log(RETRO_LOG_INFO,
            "[MemoryMap] emitted %u descriptor(s), is_wii=%d, mem1=%u bytes, mem2=%u bytes",
            mmap.num_descriptors, layout.is_wii, layout.mem1_size,
            layout.is_wii ? layout.mem2_size : 0u);
    }
}

}  // namespace
```

In `retro_run`, after the `Core::HostDispatchJobs(...)` line and before/after the `WaitForFrame()` block, add:

```cpp
    if (s_emu_thread && s_emu_thread->IsRunning())
        MaybeEmitMemoryMap();
```

- [ ] **Step 3: Implement `retro_get_memory_data` / `retro_get_memory_size`**

Replace:

```cpp
RETRO_API void*    retro_get_memory_data(unsigned) { return nullptr; }
RETRO_API size_t   retro_get_memory_size(unsigned) { return 0; }
```

with:

```cpp
RETRO_API void* retro_get_memory_data(unsigned id)
{
    if (id != RETRO_MEMORY_SYSTEM_RAM)
        return nullptr;
    if (!s_emu_thread || !s_emu_thread->IsRunning())
        return nullptr;
    return Core::System::GetInstance().GetMemory().GetRAM();  // MEM1
}

RETRO_API size_t retro_get_memory_size(unsigned id)
{
    if (id != RETRO_MEMORY_SYSTEM_RAM)
        return 0;
    if (!s_emu_thread || !s_emu_thread->IsRunning())
        return 0;
    return Core::System::GetInstance().GetMemory().GetRamSizeReal();
}
```

- [ ] **Step 4: Reset the one-shot flag on unload**

In `retro_unload_game`, add (e.g. right after `s_emu_thread->StopGame();`):

```cpp
    s_memory_map_emitted = false;
```

- [ ] **Step 5: Build**

Run: `cd /Users/mark/Documents/Projects/dolphin-libretro && cmake --build build-libretro --target dolphin_libretro`
Expected: builds cleanly.

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/LibretroFrontend.cpp
git commit -m "SP8: emit SET_MEMORY_MAPS (GC/Wii) + wire SYSTEM_RAM accessor"
```

> End-to-end RA verification (achievement set loads + triggers, Wii especially) is in Task 7.

---

## Task 4: Expose Dolphin's internal buffer save/load

**Files:**
- Modify: `Source/Core/Core/State.h` (add declarations + includes)
- Modify: `Source/Core/Core/State.cpp:222,231` (drop `static`)

- [ ] **Step 1: Declare the public wrappers in `State.h`**

Add these includes near the top of `State.h` (after the existing `#include "Common/CommonTypes.h"`):

```cpp
#include <span>

#include "Common/Buffer.h"
```

Inside `namespace State`, add (e.g. next to the `SaveAs`/`LoadAs` declarations):

```cpp
// SP8 (libretro): synchronous, in-memory state save/load used by retro_serialize.
// Must be called with the CPU thread quiesced (e.g. via Core::RunOnCPUThread).
// SaveToBuffer grows `buffer` to the measured size and returns bytes written
// (0 on failure). LoadFromBuffer returns false on failure.
std::size_t SaveToBuffer(Core::System& system, Common::UniqueBuffer<u8>& buffer);
bool LoadFromBuffer(Core::System& system, std::span<u8> buffer);
```

- [ ] **Step 2: Make the existing definitions non-static**

In `State.cpp`, change line ~222:

```cpp
static bool LoadFromBuffer(Core::System& system, std::span<u8> buffer)
```
to
```cpp
bool LoadFromBuffer(Core::System& system, std::span<u8> buffer)
```

and line ~231:

```cpp
static std::size_t SaveToBuffer(Core::System& system, Common::UniqueBuffer<u8>& buffer)
```
to
```cpp
std::size_t SaveToBuffer(Core::System& system, Common::UniqueBuffer<u8>& buffer)
```

(Both are already inside `namespace State` and keep their bodies; internal callers like `SaveToBuffer(system, s_undo_load_buffer)` still resolve.)

- [ ] **Step 3: Build**

Run: `cd /Users/mark/Documents/Projects/dolphin-libretro && cmake --build build-libretro --target dolphin_libretro`
Expected: builds cleanly (the `core` lib recompiles `State.cpp` and dependents).

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/Core/State.h Source/Core/Core/State.cpp
git commit -m "SP8: expose State::SaveToBuffer/LoadFromBuffer for libretro serialize"
```

---

## Task 5: Implement retro_serialize_size / serialize / unserialize

**Files:**
- Modify: `Source/Core/DolphinLibretro/LibretroFrontend.cpp` (includes, anon namespace, the three stubbed entrypoints at `:186-188`)

- [ ] **Step 1: Add includes + serialize-size cache**

Add includes near the top of `LibretroFrontend.cpp`:

```cpp
#include "Core/State.h"
#include "Common/Buffer.h"
#include "Common/Event.h"

#include <cstring>
#include <span>
```

In the anonymous namespace, add:

```cpp
// Cached upper bound for retro_serialize_size. Dolphin states are variable-size;
// libretro wants a stable per-session bound. Grows, never shrinks.
size_t s_serialize_size_cache = 0;
```

- [ ] **Step 2: Add a helper that runs a buffer op on the CPU thread and blocks**

Add just above `retro_serialize_size` (replacing the three stub lines):

```cpp
namespace {

// Run `fn` on the CPU thread (safe state for save/load) and block until it
// completes. Core::RunOnCPUThread queues onto the CPU thread when called from
// another thread (retro_run's thread); the Event makes completion explicit so
// we can safely read by-reference captures after this returns.
template <typename Fn>
void RunStateOpAndWait(Fn&& fn)
{
    Common::Event done;
    Core::RunOnCPUThread(Core::System::GetInstance(), [&] {
        fn();
        done.Set();
    });
    done.Wait();
}

}  // namespace

RETRO_API size_t retro_serialize_size(void)
{
    if (!s_emu_thread || !s_emu_thread->IsRunning())
        return 0;

    Common::UniqueBuffer<u8> scratch;
    size_t measured = 0;
    RunStateOpAndWait([&] {
        measured = State::SaveToBuffer(Core::System::GetInstance(), scratch);
    });

    if (measured == 0)
        return s_serialize_size_cache;  // measure failed; keep any prior bound

    // Pad so a later, larger state still fits the frontend-allocated buffer.
    const size_t padded = measured + measured / 4 + (1u << 20);  // +25% +1 MiB
    if (padded > s_serialize_size_cache)
        s_serialize_size_cache = padded;

    DolphinLibretro::Environment::Log(RETRO_LOG_INFO,
        "[Savestate] size measured=%zu reported=%zu", measured, s_serialize_size_cache);
    return s_serialize_size_cache;
}

RETRO_API bool retro_serialize(void* data, size_t size)
{
    if (!data || !s_emu_thread || !s_emu_thread->IsRunning())
        return false;

    bool ok = false;
    RunStateOpAndWait([&] {
        Common::UniqueBuffer<u8> buffer(size);
        const size_t written = State::SaveToBuffer(Core::System::GetInstance(), buffer);
        if (written != 0 && written <= size)
        {
            std::memcpy(data, buffer.data(), written);
            ok = true;
            if (written + (1u << 20) > s_serialize_size_cache)
                s_serialize_size_cache = written + (1u << 20);  // keep bound honest
        }
        else
        {
            DolphinLibretro::Environment::Log(RETRO_LOG_ERROR,
                "[Savestate] serialize: state %zu bytes > buffer %zu", written, size);
        }
    });
    return ok;
}

RETRO_API bool retro_unserialize(const void* data, size_t size)
{
    if (!data || !s_emu_thread || !s_emu_thread->IsRunning())
        return false;

    bool ok = false;
    RunStateOpAndWait([&] {
        // LoadFromBuffer takes a mutable span (PointerWrap in Read mode advances a
        // copied pointer; it does not write to the buffer). Cast away const for the API.
        std::span<u8> span(const_cast<u8*>(static_cast<const u8*>(data)), size);
        ok = State::LoadFromBuffer(Core::System::GetInstance(), span);
    });
    if (!ok)
        DolphinLibretro::Environment::Log(RETRO_LOG_ERROR, "[Savestate] unserialize failed");
    return ok;
}
```

- [ ] **Step 3: Reset the size cache on unload**

In `retro_unload_game` (where Task 3 added `s_memory_map_emitted = false;`), add:

```cpp
    s_serialize_size_cache = 0;
```

- [ ] **Step 4: Build**

Run: `cd /Users/mark/Documents/Projects/dolphin-libretro && cmake --build build-libretro --target dolphin_libretro`
Expected: builds cleanly.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/LibretroFrontend.cpp
git commit -m "SP8: implement retro_serialize/unserialize via State buffer API"
```

> Round-trip + Save&Quit/Resume verification is in Task 7 (needs a booted game).

---

## Task 6: Local deploy script

**Files:**
- Create: `Source/Core/DolphinLibretro/tools/deploy.sh`

- [ ] **Step 1: Write the script**

Create `Source/Core/DolphinLibretro/tools/deploy.sh`:

```bash
#!/usr/bin/env bash
# SP8: deploy the universal Dolphin libretro core + Sys data into RetroNest.
# Replaces the manual lipo + cp -R dance. Run from anywhere.
#
#   tools/deploy.sh [CORES_DIR] [APP_RESOURCES_DIR]
#
# Defaults match the dev machine layout (see memory/dolphin-libretro-build-setup).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"  # -> dolphin-libretro/
ARM64_DYLIB="$REPO_ROOT/build-libretro/Source/Core/DolphinLibretro/dolphin_libretro.dylib"
X86_DYLIB="$REPO_ROOT/build-libretro-x86_64/Source/Core/DolphinLibretro/dolphin_libretro.dylib"

CORES_DIR="${1:-$HOME/Documents/RetroNest/emulators/libretro/cores}"
APP_RESOURCES="${2:-$HOME/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/Resources}"

for f in "$ARM64_DYLIB" "$X86_DYLIB"; do
    [ -f "$f" ] || { echo "ERROR: missing $f — build both arches first." >&2; exit 1; }
done

mkdir -p "$CORES_DIR"
echo "lipo -> $CORES_DIR/dolphin_libretro.dylib"
lipo -create "$ARM64_DYLIB" "$X86_DYLIB" -output "$CORES_DIR/dolphin_libretro.dylib"

echo "Sys -> $APP_RESOURCES/Sys"
mkdir -p "$APP_RESOURCES"
cp -R "$REPO_ROOT/Data/Sys" "$APP_RESOURCES/Sys"

echo "Deployed:"
lipo -info "$CORES_DIR/dolphin_libretro.dylib"
```

- [ ] **Step 2: Make it executable and run it (needs both arch builds present)**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
chmod +x Source/Core/DolphinLibretro/tools/deploy.sh
# Build the x86_64 arch too (arm64 already built in earlier tasks):
PATH=/usr/local/bin:$PATH arch -x86_64 /usr/local/bin/cmake --build build-libretro-x86_64 --target dolphin_libretro
Source/Core/DolphinLibretro/tools/deploy.sh
```
Expected: prints `lipo -> …`, `Sys -> …`, and `lipo -info` shows `x86_64 arm64`.

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/tools/deploy.sh
git commit -m "SP8: add local universal-core + Sys deploy script"
```

---

## Task 7: Host-side acceptance verification (RetroNest)

No host source changes are expected — this task runs the SP8 acceptance matrix against the deployed core and records the result. If any check fails, capture `/tmp/retronest.log` and route the fix back to the relevant core task.

**Files:**
- (verification only; optionally append results to `RetroNest-Project/docs/superpowers/specs/2026-05-26-sp8-dolphin-libretro-ra-savestates-design.md` under a "Verified" note)

- [ ] **Step 1: Launch RetroNest with core logging**

Run:
```bash
pkill -x RetroNest; sleep 1
RETRONEST_DOLPHIN_LOG=1 arch -x86_64 \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
  > /tmp/retronest.log 2>&1 &
```
(If RetroNest itself was rebuilt, re-run macdeployqt + `codesign --force --deep --sign -` first — see `memory/dolphin-libretro-build-setup`.)

- [ ] **Step 2: Verify RA on GameCube**

Load a GameCube title with a known achievement set, logged into RetroAchievements. In `/tmp/retronest.log` confirm:
- `[MemoryMap] emitted 1 descriptor(s), is_wii=0` from the core, and
- the host `[rcheevos] rc_libretro_memory_init` succeeds (non-zero regions; no "failed for console 16").
Then trigger an early achievement and confirm the unlock toast.

- [ ] **Step 3: Verify RA on Wii (the real test — exercises MEM2)**

Load a Wii title with achievements. Confirm `[MemoryMap] emitted 2 descriptor(s), is_wii=1` and `rc_libretro_memory_init` success for console 19, then trigger an achievement.

- [ ] **Step 4: Verify savestates**

- In-game: save-state hotkey, change game state, load-state hotkey → state restores. Confirm `[Savestate] size measured=…` appears.
- Save & Quit → relaunch → Resume → emulation resumes from the saved point.
- Re-smoke the quit paths (menu-quit and Cmd+Q-in-game) — no crash (regression check on `memory/metal-teardown-quit-crash`).

- [ ] **Step 5: Verify persistence + FF/hotkeys**

- Create a GC memcard save (or Wii save), quit RetroNest fully, relaunch, reload the game → the save is present (proves Task 1's user-dir change; the dir lives under the host save directory, not `/tmp`).
- Toggle Fast-Forward → HUD pill shows and input hotkeys are suppressed as on other cores.
- In RA hardcore mode, confirm load-state is blocked.

- [ ] **Step 6: Record the result**

Note pass/fail per system in the spec's record (or a commit message). The SP8 acceptance gate is: GC + Wii on Metal — RA triggers, save/resume works, saves persist, FF + hotkeys behave.

---

## Self-review notes

- **Spec coverage:** Task 1 ↔ user dir; Tasks 2–3 ↔ memory exposure (`SET_MEMORY_MAPS` + SYSTEM_RAM); Tasks 4–5 ↔ savestates (buffer API + `RunOnCPUThread` + size bound); Task 6 ↔ local deploy; Task 7 ↔ host verification matrix (RA GC/Wii, savestates, persistence, FF/hotkeys, hardcore). All spec sections map to a task.
- **Out of scope (per spec):** GitHub fork/CI publishing, Achievements settings card, Wii motion achievements, Vulkan — none appear as tasks, intentionally.
- **Type consistency:** `RamLayout`/`BuildDescriptors` signatures match between `MemoryMap.h`, the test, and the `MaybeEmitMemoryMap` caller. `State::SaveToBuffer`/`LoadFromBuffer` signatures match `State.h` ↔ `State.cpp` ↔ the `retro_serialize*` callers. `s_memory_map_emitted` / `s_serialize_size_cache` are both reset in `retro_unload_game`.
- **Risk watch (from spec):** serialize threading is handled via `RunStateOpAndWait` (explicit `Common::Event`); `serialize_size` only grows and pads, and `retro_serialize` re-checks `written <= size`.
