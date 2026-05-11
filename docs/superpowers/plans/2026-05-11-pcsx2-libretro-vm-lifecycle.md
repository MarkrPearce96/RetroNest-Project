# PCSX2 Libretro Core — VM Lifecycle + Game Boot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the SP1 skeleton's clean refusal in `retro_load_game` with actually booting a real PS2 ISO on a dedicated emu thread, until `VMManager::GetState() == Running` and `VMManager::GetCurrentCRC()` returns the game's CRC. Clean shutdown via `retro_unload_game`. No video, no audio, no input.

**Architecture:** Adds two new files to `pcsx2-libretro/` — `Settings.{h,cpp}` (owns a `MemorySettingsInterface` populated with minimum-to-boot keys, registered as PCSX2's base settings layer) and `EmuThread.{h,cpp}` (a `std::thread` wrapper that drives the PCSX2 CPU thread lifecycle `CPUThreadInitialize → Initialize → Execute → Shutdown → CPUThreadShutdown`). `retro_load_game` resolves BIOS via libretro's `GET_SYSTEM_DIRECTORY`, populates settings, and starts the emu thread. `retro_unload_game` signals stop and joins. `retro_run` becomes a passive state-observer (logs state transitions once, otherwise no-op). RetroNest-side unchanged.

**Tech Stack:** C++17, `std::thread` + `std::atomic` + `std::condition_variable` for emu-thread synchronization, PCSX2's `MemorySettingsInterface` (from `common/`), PCSX2's `VMManager::*` and `VMManager::Internal::*` namespaces.

**Spec:** [2026-05-11-pcsx2-libretro-vm-lifecycle-design.md](../specs/2026-05-11-pcsx2-libretro-vm-lifecycle-design.md)
**Predecessor sub-project:** [Skeleton plan](2026-05-11-pcsx2-libretro-skeleton.md) (complete; outputs preserved in pcsx2-master `retronest-libretro` branch and RetroNest main).

**Conventions used in this plan:**
- `PCSX2_ROOT` = `/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master`
- `RETRONEST_ROOT` = `/Users/mark/Documents/Projects/RetroNest-Project`
- `RETRONEST_DATA_ROOT` = `/Users/mark/Documents/RetroNest`
- All work happens on pcsx2-master's `retronest-libretro` branch (already set up). Zero RetroNest-side changes in SP2.
- PCSX2's gsrunner is the canonical reference for "minimal frontend driving the full VM." Lines cited below refer to `pcsx2-master/pcsx2-gsrunner/Main.cpp` at the SP1 pin (`upstream/master @ dead00eb6`).

**File structure for SP2 (the entire delta):**

| File | Created or modified | Purpose |
|---|---|---|
| `${PCSX2_ROOT}/pcsx2-libretro/Settings.h` | created | `Pcsx2Libretro::Settings` namespace declarations. |
| `${PCSX2_ROOT}/pcsx2-libretro/Settings.cpp` | created | Owns the static `MemorySettingsInterface`; populates SP2-minimum keys. |
| `${PCSX2_ROOT}/pcsx2-libretro/EmuThread.h` | created | `Pcsx2Libretro::EmuThread` class declaration. |
| `${PCSX2_ROOT}/pcsx2-libretro/EmuThread.cpp` | created | Thread management + init handshake + shutdown signal. |
| `${PCSX2_ROOT}/pcsx2-libretro/CMakeLists.txt` | modified | Add `Settings.cpp` and `EmuThread.cpp` to `target_sources`. |
| `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h` | modified | Forward-declare singleton `EmuThread&` accessor. |
| `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.cpp` | modified | `retro_load_game` / `retro_unload_game` / `retro_run` real implementations. |
| `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp` | modified | Settings getters route through `Settings::GetActiveInterface()`. |
| `${PCSX2_ROOT}/pcsx2-libretro/tools/test_loader.c` | modified | Extend to drive a VM-boot lifecycle test with retro_run polling. |

**Critical PCSX2 lifecycle surfaces** (discovered during plan research, referenced in tasks below):

- `VMManager::Internal::CPUThreadInitialize()` — must be the **first** PCSX2 call on the emu thread, after `Host::Internal::SetBaseSettingsLayer` and `VMManager::Internal::LoadStartupSettings`. (gsrunner Main.cpp:940)
- `VMManager::SetDefaultSettings(si, true, true, true, true, true)` — populates a `MemorySettingsInterface` with full PCSX2 defaults; we call this before overriding our specific keys. (gsrunner Main.cpp:152)
- `VMManager::Internal::LoadStartupSettings()` — called after `SetBaseSettingsLayer`, before `CPUThreadInitialize`. (gsrunner Main.cpp:154)
- `VMManager::Internal::CPUThreadShutdown()` — must be the **last** PCSX2 call on the emu thread, after `Shutdown`. (gsrunner Main.cpp:964)
- gsrunner's settings-init pattern (Main.cpp:149-154) is the exact pattern our `Settings::InitializeDefaults` mirrors.

---

## Task 1: Create Settings.h skeleton

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/Settings.h`

- [ ] **Step 1: Write the header**

Create `${PCSX2_ROOT}/pcsx2-libretro/Settings.h` with this exact content:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro Settings layer.
//
// Owns a MemorySettingsInterface populated with the minimum keys
// PCSX2 needs to boot in skeleton/VM-lifecycle mode (sub-project 2):
// BIOS path, software renderer, nullout audio, achievements off,
// fast boot on. Registered as the active base settings layer via
// Host::Internal::SetBaseSettingsLayer before VMManager::Initialize.
//
// Sub-project 7 (Settings push) will replace this hardcoded layer
// with values driven from RetroNest's libretro options system.

#pragma once

#include <string>

class MemorySettingsInterface;

namespace Pcsx2Libretro::Settings
{

// Populate the underlying MemorySettingsInterface with PCSX2 defaults
// (via VMManager::SetDefaultSettings) and then override the SP2-required
// keys. Must be called exactly once before VMManager::Initialize.
//
// system_dir: the libretro system directory (where BIOS lives), obtained
// from RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY.
void InitializeDefaults(const std::string& system_dir);

// Access the populated settings interface. Returns nullptr before
// InitializeDefaults has been called. Pointer is stable for the
// lifetime of the dylib.
MemorySettingsInterface* GetActiveInterface();

} // namespace Pcsx2Libretro::Settings
```

- [ ] **Step 2: Verify file exists, no actions further (commit happens in Task 4)**

Run:
```sh
ls -la "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/Settings.h"
```

Expected: file present, ~30 lines.

---

## Task 2: Implement Settings.cpp

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/Settings.cpp`

- [ ] **Step 1: Write the implementation**

Create `${PCSX2_ROOT}/pcsx2-libretro/Settings.cpp` with this exact content:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "Settings.h"
#include "LibretroFrontend.h"

#include "common/MemorySettingsInterface.h"
#include "pcsx2/Host.h"
#include "pcsx2/VMManager.h"

#include "libretro.h"

namespace Pcsx2Libretro::Settings
{
namespace
{
    MemorySettingsInterface g_si;
    bool g_initialized = false;
}

void InitializeDefaults(const std::string& system_dir)
{
    if (g_initialized)
    {
        FrontendLog(RETRO_LOG_WARN, "Settings::InitializeDefaults called twice — ignoring");
        return;
    }

    // Register our MemorySettingsInterface as PCSX2's base settings layer
    // BEFORE asking VMManager to fill it with defaults — SetDefaultSettings
    // dispatches through the active layer.
    Host::Internal::SetBaseSettingsLayer(&g_si);

    // Populate with PCSX2's full defaults (folders, GS, SPU2, achievements,
    // EmuCore — every section). Same call gsrunner uses (gsrunner/Main.cpp:152).
    VMManager::SetDefaultSettings(g_si, true, true, true, true, true);

    // Now override the SP2-required minimums.
    g_si.SetStringValue("Folders", "Bios", system_dir.c_str());

    // Force software GS renderer (no native window required).
    // GSRendererType::SW == 13 in pcsx2/Config.h; we use the string the
    // SettingsWrapper will parse back to that enum value.
    g_si.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(13));

    // Disable hardware audio output — SPU2 still initializes but discards.
    g_si.SetStringValue("SPU2/Output", "OutputModule", "nullout");

    // Disable achievements (avoid network init during boot).
    g_si.SetBoolValue("Achievements", "Enabled", false);

    // Fast boot — skip BIOS region check screen.
    g_si.SetBoolValue("EmuCore", "EnableFastBoot", true);

    // Disable HostFS (we don't expose host filesystem to the VM).
    g_si.SetBoolValue("EmuCore", "HostFs", false);

    // Apply the layered settings to the live Pcsx2Config.
    VMManager::Internal::LoadStartupSettings();

    g_initialized = true;

    FrontendLog(RETRO_LOG_INFO, "Settings::InitializeDefaults complete (BIOS dir = %s)",
                system_dir.c_str());
}

MemorySettingsInterface* GetActiveInterface()
{
    return g_initialized ? &g_si : nullptr;
}

} // namespace Pcsx2Libretro::Settings
```

- [ ] **Step 2: Verify file exists**

Run:
```sh
wc -l "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/Settings.cpp"
```

Expected: ~80 lines.

---

## Task 3: Create EmuThread.h skeleton

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/EmuThread.h`

- [ ] **Step 1: Write the header**

Create `${PCSX2_ROOT}/pcsx2-libretro/EmuThread.h` with this exact content:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro emu thread.
//
// Owns a std::thread that drives PCSX2's full VM lifecycle:
//   VMManager::Internal::CPUThreadInitialize
//   VMManager::Initialize (returns StartupSuccess or failure)
//   VMManager::Execute (blocking — runs until SetState(Stopping))
//   VMManager::Shutdown
//   VMManager::Internal::CPUThreadShutdown
//
// retro_load_game starts the thread synchronously: it waits (with
// a generous timeout) until Initialize has reported success or
// failure, so it can return a meaningful true/false to libretro.

#pragma once

#include "pcsx2/VMManager.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Pcsx2Libretro
{

class EmuThread
{
public:
    EmuThread();
    ~EmuThread();

    EmuThread(const EmuThread&) = delete;
    EmuThread& operator=(const EmuThread&) = delete;

    // Spawns the thread and waits for VMManager::Initialize to complete
    // (success or failure). Returns true iff Initialize returned
    // StartupSuccess. After this returns true, the VM is in the Running
    // state on the emu thread.
    bool Start(const VMBootParameters& boot_params);

    // Asks the VM to stop. Returns immediately; the actual shutdown
    // happens on the emu thread. Call Join() to wait for it.
    void RequestShutdown();

    // Blocks until the emu thread has exited. Idempotent.
    void Join();

    // True between successful Start() and Join() (or thread exit).
    bool IsRunning() const;

private:
    void ThreadFunc(VMBootParameters params);

    std::thread m_thread;
    std::mutex m_init_mutex;
    std::condition_variable m_init_cv;
    std::atomic<bool> m_init_done{false};
    std::atomic<bool> m_init_success{false};
    std::atomic<bool> m_thread_started{false};
};

// Singleton accessor — declared here, defined in EmuThread.cpp.
EmuThread& GetEmuThread();

} // namespace Pcsx2Libretro
```

- [ ] **Step 2: Verify**

```sh
wc -l "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/EmuThread.h"
```

Expected: ~55 lines.

---

## Task 4: Implement EmuThread.cpp

**Files:**
- Create: `${PCSX2_ROOT}/pcsx2-libretro/EmuThread.cpp`

- [ ] **Step 1: Write the implementation**

Create `${PCSX2_ROOT}/pcsx2-libretro/EmuThread.cpp` with this exact content:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "EmuThread.h"
#include "LibretroFrontend.h"

#include "common/Error.h"
#include "pcsx2/Host.h"
#include "pcsx2/VMManager.h"

#include "libretro.h"

#include <chrono>

namespace Pcsx2Libretro
{

namespace
{
    constexpr auto INIT_TIMEOUT = std::chrono::seconds(30);

    EmuThread g_emu_thread;
}

EmuThread& GetEmuThread()
{
    return g_emu_thread;
}

EmuThread::EmuThread() = default;

EmuThread::~EmuThread()
{
    if (m_thread.joinable())
    {
        RequestShutdown();
        Join();
    }
}

bool EmuThread::Start(const VMBootParameters& boot_params)
{
    if (m_thread.joinable())
    {
        FrontendLog(RETRO_LOG_ERROR, "EmuThread::Start called while thread already running");
        return false;
    }

    m_init_done.store(false);
    m_init_success.store(false);
    m_thread_started.store(false);

    m_thread = std::thread(&EmuThread::ThreadFunc, this, boot_params);

    // Wait for init handshake.
    std::unique_lock<std::mutex> lk(m_init_mutex);
    if (!m_init_cv.wait_for(lk, INIT_TIMEOUT, [this] { return m_init_done.load(); }))
    {
        FrontendLog(RETRO_LOG_ERROR, "EmuThread: VM init timed out after %llds",
                    static_cast<long long>(INIT_TIMEOUT.count()));
        // Thread is still running but we couldn't confirm init — request stop
        // and join to clean up.
        VMManager::SetState(VMState::Stopping);
        m_thread.join();
        return false;
    }

    return m_init_success.load();
}

void EmuThread::RequestShutdown()
{
    if (m_thread.joinable())
    {
        // Setting state to Stopping causes Cpu->Execute() to return,
        // which causes VMManager::Execute() to return, which lets the
        // thread function fall through to Shutdown + CPUThreadShutdown.
        VMManager::SetState(VMState::Stopping);
    }
}

void EmuThread::Join()
{
    if (m_thread.joinable())
        m_thread.join();
}

bool EmuThread::IsRunning() const
{
    return m_thread.joinable() && m_init_success.load();
}

void EmuThread::ThreadFunc(VMBootParameters params)
{
    m_thread_started.store(true);

    // CPUThreadInitialize must precede any other VMManager call on the
    // CPU thread (gsrunner/Main.cpp:940).
    if (!VMManager::Internal::CPUThreadInitialize())
    {
        FrontendLog(RETRO_LOG_ERROR, "VMManager::Internal::CPUThreadInitialize failed");
        m_init_success.store(false);
        m_init_done.store(true);
        m_init_cv.notify_all();
        return;
    }

    Error err;
    const VMBootResult result = VMManager::Initialize(params, &err);
    if (result != VMBootResult::StartupSuccess)
    {
        FrontendLog(RETRO_LOG_ERROR, "VMManager::Initialize failed: %s",
                    err.GetDescription().c_str());
        VMManager::Internal::CPUThreadShutdown();
        m_init_success.store(false);
        m_init_done.store(true);
        m_init_cv.notify_all();
        return;
    }

    FrontendLog(RETRO_LOG_INFO, "VMManager::Initialize succeeded; entering Execute");
    m_init_success.store(true);
    m_init_done.store(true);
    m_init_cv.notify_all();

    // Blocks until VMManager::SetState(Stopping) is called from another
    // thread (typically from RequestShutdown above).
    VMManager::Execute();

    FrontendLog(RETRO_LOG_INFO, "VMManager::Execute returned; shutting down VM");
    VMManager::Shutdown(false);
    VMManager::Internal::CPUThreadShutdown();
    FrontendLog(RETRO_LOG_INFO, "EmuThread: clean exit");
}

} // namespace Pcsx2Libretro
```

- [ ] **Step 2: Verify**

```sh
wc -l "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/EmuThread.cpp"
```

Expected: ~110 lines.

---

## Task 5: Update CMakeLists.txt with new sources

**Files:**
- Modify: `${PCSX2_ROOT}/pcsx2-libretro/CMakeLists.txt`

- [ ] **Step 1: Read current target_sources block**

Run:
```sh
sed -n '1,40p' "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/CMakeLists.txt"
```

Expected: shows `add_library(pcsx2_libretro MODULE)` and `target_sources(pcsx2_libretro PRIVATE LibretroFrontend.cpp HostStubs.cpp)`.

- [ ] **Step 2: Add Settings.cpp and EmuThread.cpp to target_sources**

Edit `${PCSX2_ROOT}/pcsx2-libretro/CMakeLists.txt`. Find the block:

```cmake
target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
)
```

Replace with:

```cmake
target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    Settings.cpp
    EmuThread.cpp
)
```

- [ ] **Step 3: Verify CMake re-configures cleanly with the new sources**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
cmake -B build -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl3)" 2>&1 | tail -10
```

Expected: `-- Configuring done` and `-- Generating done`. (Re-config only; full build is in Task 9.)

Use timeout: 600000.

---

## Task 6: Wire HostStubs.cpp settings routing through Settings::GetActiveInterface

**Files:**
- Modify: `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp`

- [ ] **Step 1: Add the Settings.h include**

Open `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp`. Find the existing `#include "LibretroFrontend.h"` line and add immediately after it:

```cpp
#include "Settings.h"
```

- [ ] **Step 2: Add settings-layer state variables near the top of HostStubs.cpp**

Find the section that defines `g_frontend` and `FrontendLog` (it starts with the comment `// Frontend state singleton.`). Immediately AFTER the closing brace of that `namespace Pcsx2Libretro { ... }` block, add:

```cpp

// ----------------------------------------------------------------------------
// Active settings layer tracking.
// ----------------------------------------------------------------------------

namespace
{
    SettingsInterface* g_base_layer = nullptr;
    SettingsInterface* g_secrets_layer = nullptr;
    SettingsInterface* g_game_layer = nullptr;
    SettingsInterface* g_input_layer = nullptr;
    std::mutex g_settings_mutex;

    SettingsInterface* ResolveSettings()
    {
        // Game layer takes precedence over base. Skeleton SP2 only uses base.
        if (g_game_layer) return g_game_layer;
        if (g_base_layer) return g_base_layer;
        return nullptr;
    }
}
```

- [ ] **Step 3: Replace Host::Internal settings layer functions with real implementations**

Find the existing stub implementations of these functions (they may currently be no-ops or `return nullptr`):
- `Host::Internal::GetBaseSettingsLayer`
- `Host::Internal::GetSecretsSettingsLayer`
- `Host::Internal::GetGameSettingsLayer`
- `Host::Internal::GetInputSettingsLayer`
- `Host::Internal::SetBaseSettingsLayer`
- `Host::Internal::SetSecretsSettingsLayer`
- `Host::Internal::SetGameSettingsLayer`
- `Host::Internal::SetInputSettingsLayer`

Replace each with:

```cpp
SettingsInterface* Host::Internal::GetBaseSettingsLayer()
{
    return g_base_layer;
}

SettingsInterface* Host::Internal::GetSecretsSettingsLayer()
{
    return g_secrets_layer;
}

SettingsInterface* Host::Internal::GetGameSettingsLayer()
{
    return g_game_layer;
}

SettingsInterface* Host::Internal::GetInputSettingsLayer()
{
    return g_input_layer;
}

void Host::Internal::SetBaseSettingsLayer(SettingsInterface* sif)
{
    std::scoped_lock lock(g_settings_mutex);
    g_base_layer = sif;
}

void Host::Internal::SetSecretsSettingsLayer(SettingsInterface* sif)
{
    std::scoped_lock lock(g_settings_mutex);
    g_secrets_layer = sif;
}

void Host::Internal::SetGameSettingsLayer(SettingsInterface* sif, std::unique_lock<std::mutex>& /*settings_lock*/)
{
    g_game_layer = sif;
}

void Host::Internal::SetInputSettingsLayer(SettingsInterface* sif, std::unique_lock<std::mutex>& /*settings_lock*/)
{
    g_input_layer = sif;
}
```

- [ ] **Step 4: Replace Host settings-getter family with routing through ResolveSettings()**

Find the existing implementations of these (skeleton phase returned the supplied defaults):
- `Host::GetStringSettingValue`
- `Host::GetSmallStringSettingValue`
- `Host::GetTinyStringSettingValue`
- `Host::GetBoolSettingValue`
- `Host::GetIntSettingValue`
- `Host::GetUIntSettingValue`
- `Host::GetFloatSettingValue`
- `Host::GetDoubleSettingValue`
- `Host::GetStringListSetting`

Replace each with a version that consults `ResolveSettings()`. Example pattern (apply to all):

```cpp
std::string Host::GetStringSettingValue(const char* section, const char* key, const char* default_value)
{
    std::scoped_lock lock(g_settings_mutex);
    SettingsInterface* si = ResolveSettings();
    if (!si) return default_value ? std::string(default_value) : std::string();
    return si->GetStringValue(section, key, default_value);
}

bool Host::GetBoolSettingValue(const char* section, const char* key, bool default_value)
{
    std::scoped_lock lock(g_settings_mutex);
    SettingsInterface* si = ResolveSettings();
    return si ? si->GetBoolValue(section, key, default_value) : default_value;
}

int Host::GetIntSettingValue(const char* section, const char* key, int default_value)
{
    std::scoped_lock lock(g_settings_mutex);
    SettingsInterface* si = ResolveSettings();
    return si ? si->GetIntValue(section, key, default_value) : default_value;
}

// ...and the same shape for UInt, Float, Double, StringList:
//   - lock g_settings_mutex
//   - resolve si
//   - call corresponding GetXxxValue with default
//   - or return default if si is null
```

Apply the same pattern to all 9 getters listed. The `SettingsInterface` base class declares matching `GetXxxValue` methods that `MemorySettingsInterface` implements.

For `GetSmallStringSettingValue` and `GetTinyStringSettingValue`, the underlying SettingsInterface method may be `GetStringValue` returning `std::string`; wrap by constructing the small/tiny string from the returned `std::string`. If `SettingsInterface` has dedicated `GetSmallStringValue`/`GetTinyStringValue` methods, use those.

- [ ] **Step 5: Replace Host::GetSettingsLock and Host::GetSettingsInterface**

```cpp
std::unique_lock<std::mutex> Host::GetSettingsLock()
{
    return std::unique_lock<std::mutex>(g_settings_mutex);
}

std::unique_lock<std::mutex> Host::GetSecretsSettingsLock()
{
    // SP2: secrets layer is unused; share the same mutex.
    return std::unique_lock<std::mutex>(g_settings_mutex);
}

SettingsInterface* Host::GetSettingsInterface()
{
    return ResolveSettings();
}
```

- [ ] **Step 6: Verify syntax with a one-off compile**

Run (using the existing build dir's compile commands):
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
cmake --build build --target pcsx2_libretro -j 2>&1 | tail -30
```

Expected: either clean build or specific errors. **If the build fails with errors specifically about the changes in this task** (`'g_base_layer' not declared`, `no member 'GetStringValue' in SettingsInterface`, etc.), fix them. If the build fails with errors NOT about this task (e.g. errors in LibretroFrontend.cpp's retro_load_game), leave them — Task 8 owns those changes.

If unsure whether an error is "this task" vs "future task", commit Task 6's progress as a separate commit first, then iterate.

Use timeout: 1800000 (30 minutes).

---

## Task 7: Add EmuThread accessor to LibretroFrontend.h

**Files:**
- Modify: `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h`

- [ ] **Step 1: Add forward declaration for EmuThread access**

Open `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h`. At the end of the file, just before the closing `} // namespace Pcsx2Libretro`, add:

```cpp

// Forward declaration of the emu-thread accessor defined in EmuThread.cpp.
class EmuThread;
EmuThread& GetEmuThread();
```

- [ ] **Step 2: Verify file content**

```sh
grep -n "GetEmuThread\|class EmuThread" "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/LibretroFrontend.h"
```

Expected: shows both lines just added.

---

## Task 8: Rewrite retro_load_game / retro_unload_game / retro_run

**Files:**
- Modify: `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.cpp`

- [ ] **Step 1: Add new includes**

Open `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.cpp`. Find the existing includes block (`#include "LibretroFrontend.h"`, `#include "libretro.h"`, etc.) and add:

```cpp
#include "EmuThread.h"
#include "Settings.h"

#include "pcsx2/VMManager.h"

#include <atomic>
#include <filesystem>
```

- [ ] **Step 2: Bump library_version**

Find `retro_get_system_info`. Change:
```cpp
info->library_version  = "skeleton-0.1";
```
to:
```cpp
info->library_version  = "vm-0.1";
```

- [ ] **Step 3: Add a small BIOS-scan helper near the top of the extern "C" block**

Just inside `extern "C" {` (before any retro_* function), add this static helper:

```cpp
namespace {

// Returns a likely PS2 BIOS file in `dir`, or empty string if none.
// Filenames PCSX2 historically recognises: SCPH*.BIN, ps2-*.bin,
// and the simple "bios.bin" / "BIOS.bin".
std::string FindPS2BiosFile(const std::string& dir)
{
    namespace fs = std::filesystem;
    if (dir.empty() || !fs::exists(dir) || !fs::is_directory(dir))
        return {};

    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        const std::string lower = [&]{
            std::string s = name;
            for (auto& c : s) c = static_cast<char>(::tolower(c));
            return s;
        }();
        // Common PS2 BIOS naming patterns.
        if (lower.rfind("scph", 0) == 0 && (lower.ends_with(".bin") || lower.ends_with(".bin.ecm")))
            return entry.path().string();
        if (lower.rfind("ps2-", 0) == 0 && lower.ends_with(".bin"))
            return entry.path().string();
        if (lower == "bios.bin")
            return entry.path().string();
    }
    return {};
}

// Returns the libretro system directory, or empty string if the host
// does not provide one. Cached after first call.
std::string GetSystemDirectory()
{
    static std::string s_cached;
    static bool s_resolved = false;
    if (s_resolved) return s_cached;

    const char* dir = nullptr;
    if (g_frontend.environ_cb &&
        g_frontend.environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) &&
        dir != nullptr)
    {
        s_cached = dir;
    }
    s_resolved = true;
    return s_cached;
}

// Atomic, used by retro_run to log VM state transitions only once.
std::atomic<bool> g_logged_running{false};

} // namespace
```

- [ ] **Step 4: Replace retro_load_game body with the real implementation**

Find the existing `retro_load_game` (returns false with the SP1 refusal message). Replace its entire body with:

```cpp
RETRO_API bool retro_load_game(const struct retro_game_info* game)
{
    if (!game || !game->path)
    {
        FrontendLog(RETRO_LOG_ERROR, "retro_load_game called with null game info");
        return false;
    }

    FrontendLog(RETRO_LOG_INFO, "retro_load_game: %s", game->path);

    // 1. Resolve BIOS path from libretro system directory.
    const std::string system_dir = GetSystemDirectory();
    if (system_dir.empty())
    {
        FrontendLog(RETRO_LOG_ERROR, "Host did not provide a system directory");
        struct retro_message msg{};
        msg.msg = "PCSX2 libretro: host provides no system directory — cannot locate BIOS";
        msg.frames = 240;
        if (g_frontend.environ_cb)
            g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
        return false;
    }

    const std::string bios_path = FindPS2BiosFile(system_dir);
    if (bios_path.empty())
    {
        FrontendLog(RETRO_LOG_ERROR, "No PS2 BIOS file found in %s", system_dir.c_str());
        const std::string msg_text = "PS2 BIOS not found in " + system_dir;
        struct retro_message msg{};
        msg.msg = msg_text.c_str();
        msg.frames = 240;
        if (g_frontend.environ_cb)
            g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
        return false;
    }
    FrontendLog(RETRO_LOG_INFO, "Found PS2 BIOS: %s", bios_path.c_str());

    // 2. Populate the in-memory settings layer.
    Pcsx2Libretro::Settings::InitializeDefaults(system_dir);

    // 3. Build VMBootParameters and start the emu thread.
    VMBootParameters params{};
    params.filename = game->path;
    params.fast_boot = true;

    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    const bool ok = emu.Start(params);
    if (!ok)
    {
        FrontendLog(RETRO_LOG_ERROR, "retro_load_game: emu thread Start returned false");
        struct retro_message msg{};
        msg.msg = "PCSX2 libretro: VM init failed (see log)";
        msg.frames = 240;
        if (g_frontend.environ_cb)
            g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
        return false;
    }

    FrontendLog(RETRO_LOG_INFO, "retro_load_game: VM started successfully");
    g_logged_running.store(false);
    return true;
}
```

- [ ] **Step 5: Replace retro_unload_game body**

Find `retro_unload_game` (currently logs and no-ops). Replace with:

```cpp
RETRO_API void retro_unload_game(void)
{
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game: requesting VM shutdown");
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    emu.RequestShutdown();
    emu.Join();
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game: emu thread joined cleanly");
}
```

- [ ] **Step 6: Add state observation to retro_run**

Find `retro_run` (currently a complete no-op). Replace with:

```cpp
RETRO_API void retro_run(void)
{
    // SP2: no frame output. Observe state transitions and log once when
    // the VM reaches Running with a non-zero CRC (proves the game booted).
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    if (!emu.IsRunning()) return;

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
}
```

- [ ] **Step 7: Extend retro_deinit to belt-and-braces join the emu thread**

Find `retro_deinit`. Add at the very top of its body, before any other code:

```cpp
    // Defensive: ensure emu thread is stopped before tearing down g_frontend.
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    emu.RequestShutdown();
    emu.Join();
```

---

## Task 9: Build the dylib

**Files:** none (verification).

- [ ] **Step 1: Clean build (full link to catch all undefined symbols)**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
cmake --build build --target pcsx2_libretro -j 2>&1 | tee build.log | tail -40
```

Expected: green build, finishing with `[100%] Linking CXX shared module pcsx2-libretro/pcsx2_libretro.dylib` and `[100%] Built target pcsx2_libretro`.

Use timeout: 1800000 (30 minutes).

- [ ] **Step 2: If the link fails with "Undefined symbols: VMManager::Internal::*"**

This means one of `CPUThreadInitialize`, `CPUThreadShutdown`, `LoadStartupSettings` isn't in the public symbol list of the PCSX2 static library.

Verify by:
```sh
grep -rn "CPUThreadInitialize\|CPUThreadShutdown\|LoadStartupSettings" \
    "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2/VMManager.h" 2>/dev/null
```

If they're declared in `VMManager::Internal::` namespace there, the link should resolve. If not present at all, we likely have the wrong symbol name — search gsrunner for the actual call:
```sh
grep -n "VMManager::Internal::" "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-gsrunner/Main.cpp"
```

Use the names gsrunner uses verbatim.

- [ ] **Step 3: If the link fails with other "Undefined symbols: Host::SomeNewFunction"**

A new Host function we missed in HostStubs.cpp during SP1 might be called transitively during VM init that wasn't called during the skeleton phase. Same recovery as SP1 Task 8 Step 4:
1. Find the function in gsrunner/Main.cpp.
2. Copy its implementation into HostStubs.cpp (with `std::fprintf` → `FrontendLog` adaptation).
3. Rebuild.

- [ ] **Step 4: Verify dylib exports unchanged from SP1**

```sh
nm -gU build/pcsx2-libretro/pcsx2_libretro.dylib | grep -E "_retro_(init|deinit|run|get_system_info|load_game|api_version)$"
```

Expected: same 6 symbols, all `T` (defined).

- [ ] **Step 5: Commit SP2 source-code changes**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git status
```

Expected staged/unstaged files: `CMakeLists.txt` (in `pcsx2-libretro/`), `Settings.{h,cpp}` (new), `EmuThread.{h,cpp}` (new), `LibretroFrontend.{h,cpp}` (modified), `HostStubs.cpp` (modified).

```sh
git add pcsx2-libretro/CMakeLists.txt \
    pcsx2-libretro/Settings.h pcsx2-libretro/Settings.cpp \
    pcsx2-libretro/EmuThread.h pcsx2-libretro/EmuThread.cpp \
    pcsx2-libretro/LibretroFrontend.h pcsx2-libretro/LibretroFrontend.cpp \
    pcsx2-libretro/HostStubs.cpp

git commit -m "$(cat <<'EOF'
libretro: implement VM lifecycle — retro_load_game boots a real PS2 game

Replaces SP1's clean refusal in retro_load_game with actually
booting the supplied PS2 ISO on a dedicated emu thread:

- Settings.{h,cpp}: owns a MemorySettingsInterface populated with
  PCSX2's full defaults via VMManager::SetDefaultSettings, then
  overridden for SP2 minimums (Folders/Bios from libretro system
  dir, Software GS renderer, nullout audio, achievements off,
  fast-boot on). Registered as the active base settings layer
  before VMManager::Initialize.

- EmuThread.{h,cpp}: std::thread wrapper driving the full PCSX2
  CPU thread lifecycle (CPUThreadInitialize → Initialize →
  Execute → Shutdown → CPUThreadShutdown). Start() blocks until
  Initialize reports success/failure via a condition variable,
  so retro_load_game can return a synchronous true/false.

- HostStubs.cpp: settings-getter family now routes through the
  active SettingsInterface layer, with proper mutex protection.
  Host::Internal::Set/GetXxxSettingsLayer track the layers.

- LibretroFrontend.cpp: retro_load_game resolves BIOS via
  RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, populates settings,
  builds VMBootParameters, and starts the emu thread.
  retro_unload_game requests shutdown and joins. retro_run
  logs the title/serial/CRC once when the VM reaches Running.
  retro_deinit defensively joins the emu thread.

library_version bumped to "vm-0.1".

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Extend test_loader to drive a VM lifecycle test

**Files:**
- Modify: `${PCSX2_ROOT}/pcsx2-libretro/tools/test_loader.c`

- [ ] **Step 1: Read current test_loader.c**

```sh
wc -l "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/tools/test_loader.c"
cat "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/tools/test_loader.c"
```

Expected: a ~60-line C file with `main` that dlopens, calls retro_api_version, retro_set_environment, retro_init, retro_get_system_info, retro_load_game, retro_unload_game, retro_deinit.

- [ ] **Step 2: Replace the body of `main` with a version that drives retro_run**

Replace the entire `main` function in `test_loader.c` with:

```c
typedef void (*retro_run_fn)(void);
typedef void (*retro_set_video_refresh_fn)(void* cb);
typedef void (*retro_set_audio_sample_fn)(void* cb);
typedef void (*retro_set_audio_sample_batch_fn)(void* cb);
typedef void (*retro_set_input_poll_fn)(void* cb);
typedef void (*retro_set_input_state_fn)(void* cb);

// Stub libretro callbacks — required before retro_init so the core
// doesn't NULL-dereference. Must match the libretro.h prototypes
// closely enough for ABI compatibility; in C with void* we just
// pass through.
static void vr_cb(const void* data, unsigned w, unsigned h, size_t pitch) {
    (void)data; (void)w; (void)h; (void)pitch;
}
static void as_cb(int16_t left, int16_t right) { (void)left; (void)right; }
static size_t ab_cb(const int16_t* data, size_t frames) { (void)data; return frames; }
static void ip_cb(void) {}
static int16_t is_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)port; (void)device; (void)index; (void)id; return 0;
}

// Static system directory: test_loader cwd. Allows the core to find a
// BIOS file placed next to where we run from.
static char s_system_dir[4096] = {0};

#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9
static int env_cb(unsigned cmd, void* data) {
    if (cmd == RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY) {
        if (s_system_dir[0] && data) {
            *(const char**)data = s_system_dir;
            return 1;
        }
        return 0;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <core.dylib> <game.iso> [<system_dir>]\n"
                        "  <system_dir> defaults to current working directory.\n",
                argv[0]);
        return 1;
    }

    if (argc >= 4) {
        snprintf(s_system_dir, sizeof(s_system_dir), "%s", argv[3]);
    } else {
        if (!getcwd(s_system_dir, sizeof(s_system_dir))) {
            fprintf(stderr, "getcwd failed\n");
            return 1;
        }
    }
    fprintf(stderr, "test_loader: system_dir = %s\n", s_system_dir);

    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen failed: %s\n", dlerror()); return 1; }

    #define LOAD(sym, type) type sym = (type)dlsym(h, #sym); \
        if (!sym) { fprintf(stderr, "missing symbol: %s\n", #sym); dlclose(h); return 1; }

    LOAD(retro_api_version,             retro_api_version_fn);
    LOAD(retro_set_environment,         retro_set_environment_fn);
    LOAD(retro_set_video_refresh,       retro_set_video_refresh_fn);
    LOAD(retro_set_audio_sample,        retro_set_audio_sample_fn);
    LOAD(retro_set_audio_sample_batch,  retro_set_audio_sample_batch_fn);
    LOAD(retro_set_input_poll,          retro_set_input_poll_fn);
    LOAD(retro_set_input_state,         retro_set_input_state_fn);
    LOAD(retro_init,                    retro_init_fn);
    LOAD(retro_deinit,                  retro_deinit_fn);
    LOAD(retro_get_system_info,         retro_get_system_info_fn);
    LOAD(retro_load_game,               retro_load_game_fn);
    LOAD(retro_unload_game,             retro_unload_game_fn);
    LOAD(retro_run,                     retro_run_fn);
    #undef LOAD

    printf("retro_api_version() = %u\n", retro_api_version());

    retro_set_environment(env_cb);
    retro_set_video_refresh(vr_cb);
    retro_set_audio_sample(as_cb);
    retro_set_audio_sample_batch(ab_cb);
    retro_set_input_poll(ip_cb);
    retro_set_input_state(is_cb);

    retro_init();

    struct retro_system_info info = {0};
    retro_get_system_info(&info);
    printf("library_name     = %s\n", info.library_name);
    printf("library_version  = %s\n", info.library_version);

    struct retro_game_info game = {0};
    game.path = argv[2];

    fprintf(stderr, "test_loader: calling retro_load_game\n");
    int loaded = retro_load_game(&game);
    printf("retro_load_game returned: %s\n", loaded ? "TRUE" : "FALSE");

    if (loaded) {
        fprintf(stderr, "test_loader: VM started — running for 20 seconds\n");
        // ~60 fps × 20 seconds = 1200 retro_run iterations
        for (int i = 0; i < 1200; ++i) {
            retro_run();
            usleep(16667); // ~60 Hz
        }
        fprintf(stderr, "test_loader: unloading game\n");
        retro_unload_game();
        fprintf(stderr, "test_loader: game unloaded\n");
    }

    retro_deinit();
    dlclose(h);
    return loaded ? 0 : 2;
}
```

Also add to the top of the file (after the existing includes):

```c
#include <unistd.h>   // getcwd, usleep
#include <stdint.h>   // int16_t
```

- [ ] **Step 3: Rebuild test_loader**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/tools"
clang test_loader.c -o test_loader
test -x test_loader && echo "test_loader rebuilt"
```

Expected: clean compile, executable present.

---

## Task 11: Run test_loader against a real PS2 game

**Files:** none (verification).

- [ ] **Step 1: Identify a known-good PS2 ISO and BIOS location**

The PS2 BIOS in this user's setup lives in RetroNest's `bios/` directory:
```sh
ls "/Users/mark/Documents/RetroNest/bios/"
```

Expected: at least one PS2 BIOS file (`SCPH*.BIN`, `ps2-*.bin`, etc.). If not present, this test cannot run — STOP and ask the user where their PS2 BIOS lives.

PS2 ROM:
```sh
ls "/Users/mark/Documents/RetroNest/roms/ps2/" | head -3
```

Expected: at least one `.iso` file. Pick one (e.g. Ratchet & Clank).

- [ ] **Step 2: Run test_loader with VM boot**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
./pcsx2-libretro/tools/test_loader \
    build/pcsx2-libretro/pcsx2_libretro.dylib \
    "/Users/mark/Documents/RetroNest/roms/ps2/Ratchet & Clank - Going Commando (USA) (v2.00).iso" \
    "/Users/mark/Documents/RetroNest/bios" 2>&1 | tee /tmp/test_loader_vm.log | tail -60
```

(Adjust the ISO path to whatever PS2 ISO is actually available.)

Use timeout: 1800000 (30 minutes — VM init can be slow on first run because of shader cache, recompiler warm-up, etc.).

- [ ] **Step 3: Verify expected log lines**

Expected key lines in `/tmp/test_loader_vm.log`:

1. `library_name     = PCSX2` and `library_version  = vm-0.1` (proves SP2 dylib loaded).
2. `[pcsx2_libretro] Found PS2 BIOS: <path>` (proves BIOS discovery worked).
3. `[pcsx2_libretro] Settings::InitializeDefaults complete` (proves settings layer init worked).
4. `[pcsx2_libretro] VMManager::Initialize succeeded; entering Execute` (proves Initialize returned StartupSuccess).
5. Within ~10 seconds of (4): `[pcsx2_libretro] VM RUNNING — title=... serial=... crc=0x...` (proves VM reached Running and disc CRC is non-zero — the VM is actually executing the game).
6. After 20 seconds: `[pcsx2_libretro] retro_unload_game: requesting VM shutdown`.
7. `[pcsx2_libretro] VMManager::Execute returned; shutting down VM`.
8. `[pcsx2_libretro] EmuThread: clean exit`.
9. `retro_load_game returned: TRUE` printed to stdout.
10. Exit code 0.

Run:
```sh
grep -E "library_(name|version)|Found PS2 BIOS|InitializeDefaults complete|Initialize succeeded|VM RUNNING|requesting VM shutdown|EmuThread: clean exit" /tmp/test_loader_vm.log
echo "exit code: $?"
```

If any of (1)-(8) are missing, stop and report which one. The first missing line is the failure point.

- [ ] **Step 4: Common failures and fixes**

| Symptom | Likely cause | Fix |
|---|---|---|
| `retro_load_game returned: FALSE` and log says "No PS2 BIOS file found" | `FindPS2BiosFile` didn't match your BIOS filename | Run `ls /Users/mark/Documents/RetroNest/bios` and compare to the patterns in Task 8 Step 3's `FindPS2BiosFile`. Add a matching pattern if needed and rebuild. |
| `retro_load_game returned: FALSE` and log says "Host did not provide a system directory" | test_loader's env_cb isn't routing through | Verify Task 10 Step 2's env_cb returns 1 for cmd=9. Verify `s_system_dir` was populated from argv[3]. |
| Initialize log says "VMManager::Initialize failed: ..." with a message | Settings missing a key PCSX2 needs | Read the error description. Common: `gamedb.yaml` not found (need `Folders/Resources` set). Add the override to `Settings.cpp`. |
| Crashes during Execute on first JIT recompile | Apple Silicon MAP_JIT entitlement missing | Verify RetroArch / RetroNest is signed with `com.apple.security.cs.allow-jit`. For test_loader specifically, sign it: `codesign --entitlements <entitlement-file> --force --sign - test_loader` |
| Hangs forever in Start (init timeout fires after 30s) | Initialize is stuck on a Host:: function call | Check log for the last [pcsx2_libretro] line; that's likely the Host stub it's calling. Look at what gsrunner does for that function and adjust. |
| `VM RUNNING` log line never appears even after 20 seconds | VM stuck in BIOS boot or disc swap, CRC still 0 | This is a real issue and may indicate the ISO is not loaded as a disc. Verify VMBootParameters.filename is set correctly. May need `source_type = CDVD_SourceType::Iso`. |

- [ ] **Step 5: Cleanup gitignore — add the test log path**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
grep -q "tools/test_loader_vm.log" .git/info/exclude || echo "/tmp/test_loader_vm.log" >> .git/info/exclude
```

(Already in /tmp so not actually tracked, but include in exclude defensively if the log gets copied locally.)

- [ ] **Step 6: Commit test_loader changes**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git add pcsx2-libretro/tools/test_loader.c
git commit -m "$(cat <<'EOF'
libretro: extend test_loader to drive a VM lifecycle verification

Adds a 20-second retro_run loop between retro_load_game and
retro_unload_game so the libretro core has time to reach the
Running state, log the game's CRC, and demonstrate clean
shutdown. Also accepts an optional system_dir argument that
gets returned from the env callback for RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,
so the core can find the BIOS.

Verification target for SP2: log shows "VM RUNNING — title=... crc=0x..."
followed by clean unload.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 12: End-to-end test in RetroNest

**Files:** none (verification).

- [ ] **Step 1: Copy the new dylib into RetroNest's cores directory**

```sh
cp "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib" \
   "/Users/mark/Documents/RetroNest/emulators/libretro/cores/"
ls -la "/Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib"
```

Expected: file is the new build (compare mtime to the freshly built one in pcsx2-master/build/).

- [ ] **Step 2: Stop any running RetroNest, launch from terminal with log capture**

```sh
pkill -x RetroNest 2>/dev/null; sleep 1
rm -f /tmp/retronest-sp2-test.log
"/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest" > /tmp/retronest-sp2-test.log 2>&1 &
disown
sleep 3
pgrep -lf RetroNest
```

Expected: RetroNest process visible.

- [ ] **Step 3: Manual UI test (REQUIRES USER)**

Ask the user:
- Navigate to Ratchet & Clank - Going Commando (USA) (the game that was DB-patched to use `pcsx2-libretro` during SP1 Task 11).
- Launch it.
- Wait 15-20 seconds.
- Exit the game in RetroNest's UI.

- [ ] **Step 4: Capture log and verify**

```sh
sleep 2
tail -60 /tmp/retronest-sp2-test.log
```

Expected lines in the log (same as Task 11 Step 3, but observed via RetroNest's plumbing):
- `[core] [pcsx2_libretro] retro_load_game: <path>`
- `[core] [pcsx2_libretro] Found PS2 BIOS: <path>`
- `[core] [pcsx2_libretro] Settings::InitializeDefaults complete`
- `[core] [pcsx2_libretro] VMManager::Initialize succeeded; entering Execute`
- `[core] [pcsx2_libretro] VM RUNNING — title=... crc=0x...`
- `[core] [pcsx2_libretro] retro_unload_game: requesting VM shutdown`
- `[core] [pcsx2_libretro] EmuThread: clean exit`

Confirm via:
```sh
grep -E "library_(name|version)|Found PS2 BIOS|InitializeDefaults complete|Initialize succeeded|VM RUNNING|EmuThread: clean exit" /tmp/retronest-sp2-test.log
```

- [ ] **Step 5: Verify the standalone PCSX2 path still works**

In RetroNest: launch a different PS2 game (one whose emulator_id is still `pcsx2`, not `pcsx2-libretro`). It should still launch via the standalone PCSX2 binary path exactly as it did before SP2 — the launched-binary adapter is unaffected.

(If standalone PCSX2 is not installed, this step is N/A. The point is to verify we haven't regressed the launched-binary path.)

- [ ] **Step 6: Stop RetroNest cleanly**

```sh
pkill -x RetroNest 2>/dev/null
```

---

## Task 13: Mark spec complete

**Files:**
- Modify: `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-vm-lifecycle-design.md`

- [ ] **Step 1: Update spec status and append verification log**

Open `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-vm-lifecycle-design.md`. Change the header line:

```
**Status:** Approved (brainstorming)
```

to:

```
**Status:** Complete (VM lifecycle verified — see Verification log)
```

Then at the end of the file, append:

```markdown

## Verification log

SP2 phase completed (date). All seven success criteria from the summary pass:

- **Test 1 (test_loader VM lifecycle):** PASSED — `test_loader pcsx2_libretro.dylib <iso> <bios_dir>` shows VM reach Running state, GetCurrentCRC returns a non-zero CRC matching the loaded game's CRC, and clean unload completes within 5 seconds.
- **Test 2 (clean shutdown):** PASSED — emu thread joins within timeout; no leaked threads, no late log lines indicating Shutdown reentrancy.
- **Test 3 (RetroNest end-to-end):** PASSED — Ratchet & Clank launches via the libretro core in RetroNest, logs show `VM RUNNING — title=... crc=0x...`, exiting the game in the UI causes clean Shutdown. Existing pcsx2 launched-binary path continues to work for other PS2 games.

### Observations and follow-ups uncovered during verification

(To be filled in after Task 11 runs; describe anything surprising — e.g. specific BIOS filename patterns that needed adding, additional Host:: stubs the link surface required, any JIT entitlement issues hit.)

### Fork state at completion

Commits added to `retronest-libretro` branch during SP2:
- `<sha>` libretro: implement VM lifecycle — retro_load_game boots a real PS2 game
- `<sha>` libretro: extend test_loader to drive a VM lifecycle verification
- Any additional commits made during verification iteration.
```

(Replace `<sha>` with the actual commit SHAs and fill in date when running.)

- [ ] **Step 2: Commit the spec update**

```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add docs/superpowers/specs/2026-05-11-pcsx2-libretro-vm-lifecycle-design.md
git commit -m "$(cat <<'EOF'
docs(specs): mark pcsx2 libretro VM lifecycle (SP2) complete

All three verification tests pass:
1. test_loader drives a 20-second VM lifecycle and observes
   the VM reaching Running state with a non-zero CRC
2. retro_unload_game causes clean Shutdown within 5 seconds
3. RetroNest end-to-end shows VM RUNNING in logs and exits cleanly

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Plan self-review (post-write)

**Spec coverage:**

| Spec requirement | Implemented in |
|---|---|
| Settings layer (MemorySettingsInterface populated with SP2-minimum keys) | Tasks 1, 2, 6 |
| BIOS resolution via RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY | Task 8 Step 3 + Task 8 Step 4 |
| Emu thread driving CPUThreadInitialize / Initialize / Execute / Shutdown / CPUThreadShutdown | Tasks 3, 4 |
| retro_load_game real implementation | Task 8 Step 4 |
| retro_unload_game real implementation | Task 8 Step 5 |
| retro_run state observation | Task 8 Step 6 |
| retro_deinit defensive join | Task 8 Step 7 |
| HostStubs.cpp settings routing | Task 6 |
| Software GS, nullout audio, achievements off, fast-boot on | Task 2 (Settings::InitializeDefaults) |
| Test 1 (test_loader VM lifecycle) | Tasks 10, 11 |
| Test 2 (clean shutdown) | Implicit in Task 11 (covered by test_loader exit path) |
| Test 3 (RetroNest end-to-end) | Task 12 |
| Spec status update | Task 13 |
| Existing pcsx2 launched-binary path continues to work | Task 12 Step 5 |

All spec requirements covered.

**Placeholder scan:** Two intentional "(Replace `<sha>` with...)" in Task 13 are template directives executed at run time, not unfinished plan text. No other TBDs, TODOs, or FIXMEs.

**Type / name consistency:**
- `Pcsx2Libretro::Settings::InitializeDefaults` / `GetActiveInterface` — referenced consistently across Tasks 2, 6, 8.
- `Pcsx2Libretro::EmuThread::Start` / `RequestShutdown` / `Join` / `IsRunning` — referenced consistently across Tasks 3, 4, 8.
- `Pcsx2Libretro::GetEmuThread()` — accessor referenced in Tasks 4, 7, 8.
- `g_frontend.environ_cb` — referenced consistently with the existing SP1 type.
- Same `FrontendLog(RETRO_LOG_*, ...)` signature used throughout.

**Bite-size check:** Each task is one logical change with verification, broken into 2-7 steps. The biggest task is Task 6 (HostStubs settings routing) which has 6 explicit sub-steps because it touches many small functions; that's expected for a "wire up many getters in one file" task.

**Risk coverage:** All five risks from the spec are addressed: JIT entitlement (Task 11 Step 4 troubleshooting table), software GS WindowInfo (not a code change but noted in spec), missing data files (Task 11 Step 4), settings key drift (the implementation reads gsrunner as ground truth and Settings.cpp's keys can be adjusted), BIOS name patterns (FindPS2BiosFile is a heuristic; Task 11 Step 4 shows how to extend).
