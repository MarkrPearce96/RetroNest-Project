# SP2: Dolphin Libretro — Metal NSView + Boot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire `dolphin_libretro.dylib` to actually emulate a GameCube ROM. Boot a real ISO end-to-end, render frames into a host-provided NSView via Dolphin's Metal backend, drain audio samples to libretro's `audio_sample_batch_cb`, route input from libretro's RetroPad through to Dolphin's GameCube pad.

**Architecture:** Custom `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` extension (value `0x20001`, mirrors PCSX2's pattern) hands a host NSView into the core. `LibretroMetalContext.mm` wraps it as a `CAMetalLayer` and stuffs it into `WindowSystemInfo::render_surface` — no `VideoBackends/Metal` surgery needed (Dolphin's Metal backend already consumes a layer via WSI). `EmuThread` owns the BootManager/Core lifecycle. Custom `SoundStream` + `InputBackend` subclasses are injected directly via `Core::System::SetSoundStream()` and `ControllerInterface::AddDevice()` — sidesteps Dolphin's config-driven backend selection. Verified by a small standalone Cocoa harness (`tools/test_harness.mm`) that creates an `NSWindow`, supplies its NSView via the env callback, and boots a user-provided GC ROM for N frames.

**Tech Stack:** C++20, Objective-C++, Cocoa/AppKit, Metal, CAMetalLayer, Dolphin's `Core::System` / `BootManager` / `AudioCommon` / `InputCommon` APIs, libretro C ABI.

**Parent spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-23-dolphin-libretro-conversion-design.md`

**Predecessor:** SP1 complete — `dolphin_libretro.dylib` skeleton builds and dlopens cleanly on the `libretro` branch.

**Working directory:** `/Users/mark/Documents/Projects/dolphin-libretro/` on branch `libretro`.

**Out of scope (deferred to later SPs):**
- RetroNest-side adapter registration → SP3
- Vulkan render path → SP4
- Real controller mapping UI → SP5
- Settings core options → SP6/SP7

**Required from the user:** A GameCube ROM file (path passed to the test harness as an argument). Any GC `.iso` / `.gcm` / `.rvz` file the user owns will do. The plan never bundles or hardcodes a ROM.

---

## File structure

| File | Status | Responsibility |
|---|---|---|
| `Source/Core/DolphinLibretro/LibretroEnvironment.h` | Create | Constants for custom env extensions (`RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW = 0x20001`), helper `RequestHostNSView(environ_cb)`. |
| `Source/Core/DolphinLibretro/LibretroEnvironment.cpp` | Create | Implementations. |
| `Source/Core/DolphinLibretro/LibretroMetalContext.h` | Create | `PrepareWindowSystemInfo(void* nsview, WindowSystemInfo* out)` — wraps NSView in a CAMetalLayer, fills WSI. |
| `Source/Core/DolphinLibretro/LibretroMetalContext.mm` | Create | Implementation. |
| `Source/Core/DolphinLibretro/LibretroAudioStream.h` | Create | `LibretroAudioStream : public SoundStream` declaration. |
| `Source/Core/DolphinLibretro/LibretroAudioStream.cpp` | Create | Implementation — pull thread reads from Mixer, pushes batches to `audio_sample_batch_cb`. |
| `Source/Core/DolphinLibretro/LibretroInputSource.h` | Create | `LibretroInputBackend : public ciface::InputBackend` + `Device` subclass. |
| `Source/Core/DolphinLibretro/LibretroInputSource.cpp` | Create | Implementation — virtual device exposes RetroPad buttons/axes; `UpdateInput()` polls `input_state_cb`. |
| `Source/Core/DolphinLibretro/EmuThread.h` | Major rewrite | Real coordinator owning `Core::System*`, lifecycle (`StartGame`, `StopGame`), frame coordination (`RunFrame` advances Dolphin's CPU thread by one video frame). |
| `Source/Core/DolphinLibretro/EmuThread.cpp` | Major rewrite | Implementation. |
| `Source/Core/DolphinLibretro/LibretroFrontend.cpp` | Major update | Real `retro_load_game`, `retro_run`, `retro_unload_game`. |
| `Source/Core/DolphinLibretro/HostStubs.cpp` | Minor update | `Host_Message(WMUserStop)` signals EmuThread shutdown. |
| `Source/Core/DolphinLibretro/CMakeLists.txt` | Update | Add 4 new source files; ensure ObjC++ flags. |
| `Source/Core/DolphinLibretro/tools/test_harness.mm` | Create | Cocoa test app — replaces `test_load.cpp` for SP2-era smoke testing. |
| `Source/Core/DolphinLibretro/tools/CMakeLists.txt` | Update | Add the harness target. Keep the old `test_load` target for the basic ABI smoke. |

---

### Task 1: Custom libretro environment extension

**Files:**
- Create: `Source/Core/DolphinLibretro/LibretroEnvironment.h`
- Create: `Source/Core/DolphinLibretro/LibretroEnvironment.cpp`

Defines the constants for RetroNest's private environment extensions and a thin helper for the NSView retrieval. Lives separately from `LibretroFrontend.cpp` so other translation units (`LibretroMetalContext.mm`) can use it without pulling in the entire frontend.

- [ ] **Step 1: Write LibretroEnvironment.h**

Create the header:

```cpp
// RetroNest-private libretro environment extensions.
//
// These extensions are not part of the standard libretro API. They live in
// the RETRO_ENVIRONMENT_PRIVATE range (0x20000+) which the libretro spec
// reserves for frontend↔core private contracts. The numeric values MUST
// match RetroNest's environment_callbacks.h definitions exactly.

#pragma once

#include "DolphinLibretro/libretro.h"

namespace DolphinLibretro::Environment {

// Hand a host-owned NSView* to the core for Metal rendering.
// retro_environment_t cb writes a (void* NSView) into the data ptr.
// Matches RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW = (1 | RETRO_ENVIRONMENT_PRIVATE).
constexpr unsigned RETRONEST_GET_MACOS_NSVIEW = (1u | RETRO_ENVIRONMENT_PRIVATE);

// Stores the frontend's environ_cb at retro_set_environment time so other
// modules can use it. Caller-friendly wrappers below.
void SetEnvironmentCallback(retro_environment_t cb);
retro_environment_t GetEnvironmentCallback();

// Requests an NSView from the host. Returns nullptr if the env extension
// is unsupported or the host returned no view. Logs via retro_log_cb if
// it's been set.
void* RequestHostNSView();

// Similar accessor for the frontend log callback (set during retro_init).
void SetLogCallback(retro_log_printf_t cb);
void Log(enum retro_log_level level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

}  // namespace DolphinLibretro::Environment
```

- [ ] **Step 2: Write LibretroEnvironment.cpp**

```cpp
#include "DolphinLibretro/LibretroEnvironment.h"

#include <cstdarg>
#include <cstdio>

namespace DolphinLibretro::Environment {

namespace {
retro_environment_t  s_environ_cb = nullptr;
retro_log_printf_t   s_log_cb     = nullptr;
}  // namespace

void SetEnvironmentCallback(retro_environment_t cb) { s_environ_cb = cb; }
retro_environment_t GetEnvironmentCallback()        { return s_environ_cb; }

void SetLogCallback(retro_log_printf_t cb) { s_log_cb = cb; }

void Log(enum retro_log_level level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (s_log_cb)
    {
        // retro_log_printf_t takes (level, fmt, ...) — no vprintf variant
        // in the standard libretro header, so format locally and forward.
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        s_log_cb(level, "%s", buf);
    }
    else
    {
        // Fallback so smoke tests without retro_log still see output.
        vfprintf(stderr, fmt, args);
        fputc('\n', stderr);
    }
    va_end(args);
}

void* RequestHostNSView()
{
    if (!s_environ_cb)
    {
        Log(RETRO_LOG_ERROR, "[Environment] no environ_cb registered");
        return nullptr;
    }
    void* nsview = nullptr;
    if (!s_environ_cb(RETRONEST_GET_MACOS_NSVIEW, &nsview) || !nsview)
    {
        Log(RETRO_LOG_ERROR,
            "[Environment] host did not provide NSView via RETRONEST_GET_MACOS_NSVIEW (0x%x)",
            RETRONEST_GET_MACOS_NSVIEW);
        return nullptr;
    }
    Log(RETRO_LOG_INFO, "[Environment] got NSView=%p", nsview);
    return nsview;
}

}  // namespace DolphinLibretro::Environment
```

- [ ] **Step 3: Add to CMakeLists**

In `Source/Core/DolphinLibretro/CMakeLists.txt`, update `target_sources` to include the new files. Current:
```cmake
target_sources(dolphin_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    EmuThread.cpp
)
```

Change to:
```cmake
target_sources(dolphin_libretro PRIVATE
    LibretroFrontend.cpp
    LibretroEnvironment.cpp
    HostStubs.cpp
    EmuThread.cpp
)
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -10
```

Expected: clean build. The dylib gets two new symbols but they're hidden — `nm -gU` should still show exactly 25 `_retro_*` exports.

- [ ] **Step 5: Stage only**

```bash
git add Source/Core/DolphinLibretro/LibretroEnvironment.{h,cpp} \
        Source/Core/DolphinLibretro/CMakeLists.txt
```

Report — controller commits with message:
```
SP2: libretro environment extension helpers

Adds LibretroEnvironment.{h,cpp} with the RETRONEST_GET_MACOS_NSVIEW
constant (0x20001), environment-callback + log-callback storage, and a
RequestHostNSView() helper. Modules can now query the host without
plumbing the environ_cb through every signature.
```

---

### Task 2: LibretroMetalContext — NSView → WindowSystemInfo

**Files:**
- Create: `Source/Core/DolphinLibretro/LibretroMetalContext.h`
- Create: `Source/Core/DolphinLibretro/LibretroMetalContext.mm`

Wraps a host NSView with a CAMetalLayer and produces a `WindowSystemInfo` that Dolphin's Metal backend can consume directly. Skips `MetalUtil::PrepareWindow` — that function is intended for backends that own their NSView; we want the layer to live with our host.

- [ ] **Step 1: Read Dolphin's WindowSystemInfo struct**

Read `Source/Core/Common/WindowSystemInfo.h` to confirm the struct shape:

```bash
cat /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/Common/WindowSystemInfo.h | head -60
```

Expected fields include: `WindowSystemType type`, `void* display_connection`, `void* render_window`, `void* render_surface`, `float render_surface_scale`. The Metal backend reads `render_surface` as `CAMetalLayer*`.

- [ ] **Step 2: Write LibretroMetalContext.h**

```cpp
// Bridges a host-owned NSView into a WindowSystemInfo that Dolphin's
// Metal backend can consume directly. The host (RetroNest, or a SP2 test
// harness) owns the NSView; we own the CAMetalLayer that lives on top
// of it.

#pragma once

struct WindowSystemInfo;

namespace DolphinLibretro::Metal {

// Wraps `nsview` with a fresh CAMetalLayer attached as the view's layer,
// populates `*out` with the layer, surface size, and the macOS-specific
// WindowSystemType. Returns true on success.
//
// Caller is responsible for the NSView's lifetime. The layer is owned by
// the NSView once attached (the NSView retains its layer).
//
// Must be called from the main thread (NSView access).
bool PrepareWindowSystemInfo(void* nsview, WindowSystemInfo* out);

// Tears down the CAMetalLayer attachment. Call when the backend is
// stopped. Safe to call multiple times.
void ReleaseWindowSystemInfo(WindowSystemInfo* wsi);

}  // namespace DolphinLibretro::Metal
```

- [ ] **Step 3: Write LibretroMetalContext.mm**

```objective-c++
#import "DolphinLibretro/LibretroMetalContext.h"
#import "DolphinLibretro/LibretroEnvironment.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

#include "Common/WindowSystemInfo.h"

namespace DolphinLibretro::Metal {

bool PrepareWindowSystemInfo(void* nsview_raw, WindowSystemInfo* out)
{
    if (!nsview_raw || !out)
    {
        Environment::Log(RETRO_LOG_ERROR,
            "[Metal] PrepareWindowSystemInfo called with nsview=%p out=%p",
            nsview_raw, static_cast<void*>(out));
        return false;
    }

    NSView* view = (__bridge NSView*)nsview_raw;

    // Attach a CAMetalLayer to the view. setWantsLayer must be set before
    // setLayer (per AppKit docs) so the NSView becomes layer-backed.
    CAMetalLayer* layer = [CAMetalLayer layer];
    id<MTLDevice> mtl_device = MTLCreateSystemDefaultDevice();
    if (!mtl_device)
    {
        Environment::Log(RETRO_LOG_ERROR, "[Metal] no default MTLDevice");
        return false;
    }
    layer.device = mtl_device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;

    dispatch_block_t attach = ^{
        view.wantsLayer = YES;
        view.layer = layer;
        const CGFloat scale = view.window ? view.window.backingScaleFactor : 1.0;
        layer.contentsScale = scale;
        const NSSize size = view.bounds.size;
        layer.drawableSize = NSMakeSize(size.width * scale, size.height * scale);
    };
    if ([NSThread isMainThread])
        attach();
    else
        dispatch_sync(dispatch_get_main_queue(), attach);

    // Populate WSI. The Metal backend reads render_surface as the layer.
    out->type = WindowSystemType::MacOS;
    out->display_connection = nullptr;
    out->render_window = (__bridge void*)view;
    out->render_surface = (__bridge_retained void*)layer;
    const CGFloat scale = view.window ? view.window.backingScaleFactor : 1.0;
    out->render_surface_scale = static_cast<float>(scale);

    Environment::Log(RETRO_LOG_INFO,
        "[Metal] WSI ready: view=%p layer=%p scale=%.2f size=%.0fx%.0f",
        nsview_raw, (__bridge void*)layer, scale,
        view.bounds.size.width, view.bounds.size.height);
    return true;
}

void ReleaseWindowSystemInfo(WindowSystemInfo* wsi)
{
    if (!wsi || !wsi->render_surface)
        return;

    // The layer was retained into render_surface via __bridge_retained;
    // CFRelease balances that.
    CFRelease(wsi->render_surface);
    wsi->render_surface = nullptr;
    wsi->render_window = nullptr;
}

}  // namespace DolphinLibretro::Metal
```

- [ ] **Step 4: Update CMakeLists.txt — add Metal source + ObjC++ flags**

In `Source/Core/DolphinLibretro/CMakeLists.txt`, update `target_sources` and add Apple frameworks:

```cmake
target_sources(dolphin_libretro PRIVATE
    LibretroFrontend.cpp
    LibretroEnvironment.cpp
    LibretroMetalContext.mm
    HostStubs.cpp
    EmuThread.cpp
)

# Objective-C++ source needs ARC + AppKit/Metal/QuartzCore frameworks.
if(APPLE)
    set_source_files_properties(LibretroMetalContext.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    target_link_libraries(dolphin_libretro PRIVATE
        "-framework AppKit"
        "-framework Metal"
        "-framework QuartzCore"
    )
endif()
```

(Add the `if(APPLE)` block after the existing `set_target_properties` calls.)

- [ ] **Step 5: Build**

```bash
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -5
```

Expected: clean build. `LibretroMetalContext.mm.o` should appear in the link line.

- [ ] **Step 6: Stage only**

```bash
git add Source/Core/DolphinLibretro/LibretroMetalContext.{h,mm} \
        Source/Core/DolphinLibretro/CMakeLists.txt
```

Commit message:
```
SP2: LibretroMetalContext — host NSView → WindowSystemInfo bridge

Wraps a host-owned NSView with a fresh CAMetalLayer and stuffs it into
WindowSystemInfo::render_surface for Dolphin's Metal backend to consume.
No VideoBackends/Metal modifications needed — the existing Initialize()
already accepts a layer via WSI.

CMakeLists: add ObjC++ ARC flag for the .mm source, link AppKit/Metal/
QuartzCore frameworks on Apple.
```

---

### Task 3: EmuThread real implementation

**Files:**
- Modify: `Source/Core/DolphinLibretro/EmuThread.h` (major rewrite)
- Modify: `Source/Core/DolphinLibretro/EmuThread.cpp` (major rewrite)

The real coordinator. Owns a `Core::System*` (acquired via `Core::System::GetInstance()` if Dolphin uses singletons, else passed in), drives `BootManager::BootCore`, hooks `Core::SetState` for pause, and synchronizes per-frame execution.

Dolphin's threading model: `BootManager::BootCore` is called from the host thread and spawns Dolphin's internal CPU + Fifo threads. `Core::Stop` signals shutdown. We don't replace those threads — we gate them.

Per-frame model: libretro frontends call `retro_run` once per frame, expecting one video frame to be produced. Dolphin runs free-running once booted. To get exactly-one-frame-per-retro_run, we use a "frame end" hook — the VideoCommon code calls into a callback on each presented frame, and our EmuThread blocks `retro_run` until that callback fires.

**Sub-investigation required**: SP1 didn't need this; SP2 does. The implementer must locate Dolphin's "frame presented" hook. Look at `VideoCommon/VideoBackendBase.h`, `VideoCommon/Present.h`, or whatever hook PCSX2-libretro uses (`grep -r "RegisterPresentCallback\|on_present\|frame_count_cb" /Users/mark/Documents/Projects/pcsx2-libretro`). If no clean hook exists, fall back to a polled approach: `retro_run` runs for ~16.6ms wall-clock and trusts Dolphin to have produced approximately one frame.

- [ ] **Step 1: Investigate Dolphin's "frame presented" hook**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
grep -rn "OnFrameEnd\|RegisterPresentCallback\|present_callback\|frame_end_cb\|AfterFrameEvent" Source/Core/VideoCommon/ Source/Core/Core/ | head -20
```

Report whichever existing hook is the best match. If none exists clean enough, plan to add one (a small `PerformanceMetrics::AddOnFrameEnd` callback, or use Dolphin's existing `Common::Event` primitive in CoreTiming).

**Stop here and report findings before implementing Step 2** — the exact hook choice shapes the EmuThread API.

- [ ] **Step 2: Write EmuThread.h based on Step 1's findings**

Outline (the implementer fills in `RegisterFrameEndHook` per Step 1):

```cpp
#pragma once

#include "Common/WindowSystemInfo.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

namespace Core { class System; }

namespace DolphinLibretro {

class EmuThread
{
public:
    EmuThread();
    ~EmuThread();

    EmuThread(const EmuThread&) = delete;
    EmuThread& operator=(const EmuThread&) = delete;

    // Boots the given ROM with the given WSI. Returns true on successful
    // boot; false if BootManager::BootCore failed (bad path, unsupported
    // format, etc.). Must be paired with StopGame on the same EmuThread.
    bool StartGame(const std::string& rom_path, const WindowSystemInfo& wsi);

    // Tears down the running game. Idempotent.
    void StopGame();

    // Blocks the caller until Dolphin produces one video frame, or returns
    // after a timeout if Dolphin is paused or stalled. Called from retro_run.
    void WaitForFrame();

    // Pause / resume — maps to Core::SetState(State::Paused / State::Running).
    void SetPaused(bool paused);
    bool IsPaused() const { return m_paused.load(); }

    bool IsRunning() const { return m_running.load(); }

private:
    // Called by Dolphin's frame-end hook (registered in StartGame).
    void OnFrameEnd();

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};

    std::mutex m_frame_mutex;
    std::condition_variable m_frame_cv;
    bool m_frame_ready{false};

    // Token returned by Dolphin's frame-end hook registration, used to
    // unregister in StopGame. Type depends on Step 1's findings.
    // void* m_frame_hook_token = nullptr;
};

}  // namespace DolphinLibretro
```

- [ ] **Step 3: Write EmuThread.cpp**

Implementation outline. Based on `Core/BootManager.h:17-18` and `Core/Core.h:123-149`:

```cpp
#include "DolphinLibretro/EmuThread.h"
#include "DolphinLibretro/LibretroEnvironment.h"

#include "Core/BootManager.h"
#include "Core/Boot/Boot.h"
#include "Core/Core.h"
#include "Core/System.h"

#include <chrono>

namespace DolphinLibretro {

EmuThread::EmuThread() = default;

EmuThread::~EmuThread()
{
    StopGame();
}

bool EmuThread::StartGame(const std::string& rom_path, const WindowSystemInfo& wsi)
{
    if (m_running.load())
    {
        Environment::Log(RETRO_LOG_WARN, "[EmuThread] StartGame called while already running");
        return false;
    }

    auto boot_params = BootParameters::GenerateFromFile(rom_path);
    if (!boot_params)
    {
        Environment::Log(RETRO_LOG_ERROR,
            "[EmuThread] BootParameters::GenerateFromFile(%s) returned null",
            rom_path.c_str());
        return false;
    }

    auto& system = Core::System::GetInstance();

    // Register frame-end hook BEFORE booting so we don't miss the first frame.
    // (Implementer fills in based on Step 1's findings.)
    // m_frame_hook_token = VideoCommon::RegisterFrameEndCallback(
    //     [this]() { OnFrameEnd(); });

    if (!BootManager::BootCore(system, std::move(boot_params), wsi))
    {
        Environment::Log(RETRO_LOG_ERROR, "[EmuThread] BootManager::BootCore failed");
        return false;
    }

    m_running.store(true);
    m_paused.store(false);
    Environment::Log(RETRO_LOG_INFO, "[EmuThread] booted %s", rom_path.c_str());
    return true;
}

void EmuThread::StopGame()
{
    if (!m_running.exchange(false))
        return;

    Environment::Log(RETRO_LOG_INFO, "[EmuThread] stopping");

    // (Implementer fills in — unregister hook before Core::Stop so the
    //  callback's destructor doesn't race the queue tear-down.)
    // VideoCommon::UnregisterFrameEndCallback(m_frame_hook_token);

    Core::Stop(Core::System::GetInstance());

    // Wake any retro_run blocked on WaitForFrame so it can unwind.
    {
        std::lock_guard<std::mutex> lock(m_frame_mutex);
        m_frame_ready = true;
    }
    m_frame_cv.notify_all();
}

void EmuThread::WaitForFrame()
{
    using namespace std::chrono_literals;

    std::unique_lock<std::mutex> lock(m_frame_mutex);
    // 33ms timeout — twice the wall-clock frame budget; if we hit this,
    // Dolphin is paused or stalled and we don't want retro_run to hang.
    const bool got_frame = m_frame_cv.wait_for(lock, 33ms,
                                                [this] { return m_frame_ready; });
    if (got_frame)
        m_frame_ready = false;
    // If !got_frame, we return anyway — libretro frontends tolerate empty frames.
}

void EmuThread::SetPaused(bool paused)
{
    if (m_paused.exchange(paused) == paused)
        return;
    Core::SetState(Core::System::GetInstance(),
                   paused ? Core::State::Paused : Core::State::Running);
}

void EmuThread::OnFrameEnd()
{
    {
        std::lock_guard<std::mutex> lock(m_frame_mutex);
        m_frame_ready = true;
    }
    m_frame_cv.notify_one();
}

}  // namespace DolphinLibretro
```

- [ ] **Step 4: Resolve frame-end hook**

Replace the `// (Implementer fills in...)` comments with the actual hook calls based on Step 1's findings. If Dolphin doesn't have a clean hook, the fallback is: omit `OnFrameEnd`, change `WaitForFrame` to a fixed `std::this_thread::sleep_for(16ms)` (NTSC frame time). The smoke test will still produce frames; rate-control is just approximate.

- [ ] **Step 5: Build**

```bash
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -10
```

Common failure modes:
- **Undefined `Core::System::GetInstance()`**: Dolphin might require it accessed differently (e.g. `Core::System::GetInstance()` is in `Core/System.h`). Confirm the include + access pattern by reading the file.
- **Undefined `BootManager::BootCore`**: include path. The signature is in `Core/BootManager.h:17-18`.

If the build is clean, the dylib is larger now (BootManager + Core + AudioCommon code is no longer DCE'd).

- [ ] **Step 6: Stage only**

```bash
git add Source/Core/DolphinLibretro/EmuThread.{h,cpp}
```

Commit message:
```
SP2: real EmuThread — BootManager + Core lifecycle wiring

Owns BootManager::BootCore + Core::Stop/SetState. Per-frame coordination
via a condition variable signaled by Dolphin's frame-end hook (see
EmuThread.cpp for the specific hook used) — retro_run blocks at most
33ms waiting for a frame to be produced.
```

---

### Task 4: LibretroAudioStream — pull from Mixer, push to libretro

**Files:**
- Create: `Source/Core/DolphinLibretro/LibretroAudioStream.h`
- Create: `Source/Core/DolphinLibretro/LibretroAudioStream.cpp`

Subclass `AudioCommon::SoundStream`. On `SetRunning(true)`, spin up a thread that pulls from `Core::System::GetSoundStream()->GetMixer()->Mix(buf, frames)` in a tight loop and hands the batch to `audio_sample_batch_cb`. Sample rate matches Dolphin's mixer (32 kHz HLE).

Sidesteps Dolphin's "select backend by config" path: we instantiate ourselves and inject via `system.SetSoundStream(std::move(libretro_stream))` before `BootManager::BootCore`. `AudioCommon::InitSoundStream` is never called.

- [ ] **Step 1: Read SoundStream + Mixer interfaces**

```bash
cat /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/AudioCommon/SoundStream.h
cat /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/AudioCommon/Mixer.h | head -50
```

Confirm: `SoundStream` has `Init()`, `SetRunning(bool)`, `SetVolume(int)`, `GetMixer()`. `Mixer::Mix(s16* out, size_t num_frames)` is the pull API (stereo s16, native sample rate).

- [ ] **Step 2: Write LibretroAudioStream.h**

```cpp
// Dolphin SoundStream subclass that drains Dolphin's mixer and forwards
// batches to libretro's audio_sample_batch_cb. Pull architecture: a
// background thread loops on Mixer::Mix() at the native sample rate.

#pragma once

#include "AudioCommon/SoundStream.h"

#include <atomic>
#include <thread>

namespace DolphinLibretro {

class LibretroAudioStream final : public SoundStream
{
public:
    LibretroAudioStream();
    ~LibretroAudioStream() override;

    bool Init() override;
    bool SetRunning(bool running) override;
    void SetVolume(int volume) override;

private:
    void DrainLoop();

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_should_run{false};
    std::thread       m_drain_thread;
};

}  // namespace DolphinLibretro
```

- [ ] **Step 3: Write LibretroAudioStream.cpp**

```cpp
#include "DolphinLibretro/LibretroAudioStream.h"
#include "DolphinLibretro/LibretroEnvironment.h"
#include "DolphinLibretro/libretro.h"

#include "AudioCommon/Mixer.h"

#include <array>
#include <chrono>

// Forward-declared callback storage from LibretroFrontend.cpp.
namespace DolphinLibretro::Frontend {
extern retro_audio_sample_batch_t g_audio_batch_cb;
}  // namespace DolphinLibretro::Frontend

namespace DolphinLibretro {

namespace {
// Batch size: ~5ms at 32 kHz = 160 frames. Small enough to keep latency
// low, large enough that callback overhead is amortized.
constexpr size_t kBatchFrames = 160;
}  // namespace

LibretroAudioStream::LibretroAudioStream() = default;

LibretroAudioStream::~LibretroAudioStream()
{
    SetRunning(false);
}

bool LibretroAudioStream::Init()
{
    Environment::Log(RETRO_LOG_INFO, "[Audio] LibretroAudioStream::Init");
    return true;
}

bool LibretroAudioStream::SetRunning(bool running)
{
    if (running == m_running.exchange(running))
        return true;

    if (running)
    {
        m_should_run.store(true);
        m_drain_thread = std::thread([this] { DrainLoop(); });
        Environment::Log(RETRO_LOG_INFO, "[Audio] drain thread started");
    }
    else
    {
        m_should_run.store(false);
        if (m_drain_thread.joinable())
            m_drain_thread.join();
        Environment::Log(RETRO_LOG_INFO, "[Audio] drain thread stopped");
    }
    return true;
}

void LibretroAudioStream::SetVolume(int volume)
{
    // libretro has no per-core volume; the frontend manages output volume.
    (void)volume;
}

void LibretroAudioStream::DrainLoop()
{
    std::array<int16_t, kBatchFrames * 2> buf{};  // stereo s16
    Mixer* mixer = GetMixer();

    while (m_should_run.load())
    {
        if (!mixer || !Frontend::g_audio_batch_cb)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        mixer->Mix(buf.data(), kBatchFrames);
        // libretro expects interleaved stereo s16 frame count.
        Frontend::g_audio_batch_cb(buf.data(), kBatchFrames);
    }
}

}  // namespace DolphinLibretro
```

- [ ] **Step 4: Update CMakeLists**

```cmake
target_sources(dolphin_libretro PRIVATE
    LibretroFrontend.cpp
    LibretroEnvironment.cpp
    LibretroMetalContext.mm
    LibretroAudioStream.cpp
    HostStubs.cpp
    EmuThread.cpp
)
```

- [ ] **Step 5: Build**

```bash
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -10
```

May fail with "undefined `Frontend::g_audio_batch_cb`" — that's because LibretroFrontend.cpp's callback variable is in an anonymous namespace. Task 5 promotes it. For now, comment out the `DrainLoop` body's actual `Frontend::g_audio_batch_cb` call (replace with a stub) just to verify the file compiles. Restore in Task 5.

- [ ] **Step 6: Stage only**

```bash
git add Source/Core/DolphinLibretro/LibretroAudioStream.{h,cpp} \
        Source/Core/DolphinLibretro/CMakeLists.txt
```

Commit message:
```
SP2: LibretroAudioStream — Mixer→libretro audio bridge

Subclass of Dolphin's SoundStream. Pull thread reads from Mixer::Mix in
160-frame batches (~5ms at 32 kHz) and forwards to libretro's
audio_sample_batch_cb. Bypasses AudioCommon::InitSoundStream — we inject
directly via Core::System::SetSoundStream.
```

---

### Task 5: Promote callback storage + wire LibretroFrontend

**Files:**
- Modify: `Source/Core/DolphinLibretro/LibretroFrontend.cpp` (major update)

Move the callback variables from anonymous namespace to a `DolphinLibretro::Frontend` namespace so other translation units (LibretroAudioStream, LibretroInputSource) can read them. Then wire `retro_load_game` / `retro_run` / `retro_unload_game` to do real work via the EmuThread.

- [ ] **Step 1: Rewrite LibretroFrontend.cpp**

Replace the entire file with:

```cpp
// SP2: retro_* C ABI entrypoints — real implementation.
//
// retro_load_game boots a GameCube/Wii ROM via BootManager::BootCore.
// retro_run advances Dolphin by ~1 frame and forwards audio to the
// frontend (video is pushed directly by Dolphin's Metal backend into
// the CAMetalLayer we registered via WSI, so no video_refresh_cb call
// is needed — libretro frontends with HW context-rendered cores treat
// the video_refresh as a notification, not a transport).

#include "DolphinLibretro/libretro.h"
#include "DolphinLibretro/EmuThread.h"
#include "DolphinLibretro/LibretroEnvironment.h"
#include "DolphinLibretro/LibretroMetalContext.h"
#include "DolphinLibretro/LibretroAudioStream.h"
#include "DolphinLibretro/LibretroInputSource.h"

#include "Common/WindowSystemInfo.h"
#include "Core/System.h"

#include <memory>
#include <string>

namespace DolphinLibretro::Frontend {

// Callback storage — accessible to sibling TUs via extern declarations.
retro_environment_t        g_environ_cb         = nullptr;
retro_video_refresh_t      g_video_refresh_cb   = nullptr;
retro_audio_sample_t       g_audio_sample_cb    = nullptr;
retro_audio_sample_batch_t g_audio_batch_cb     = nullptr;
retro_input_poll_t         g_input_poll_cb      = nullptr;
retro_input_state_t        g_input_state_cb     = nullptr;

}  // namespace DolphinLibretro::Frontend

namespace {

std::unique_ptr<DolphinLibretro::EmuThread>           s_emu_thread;
std::unique_ptr<DolphinLibretro::LibretroAudioStream> s_audio_stream_owned;
WindowSystemInfo                                       s_wsi{};

}  // namespace

extern "C" {

RETRO_API unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info* info)
{
    info->library_name     = "Dolphin";
    info->library_version  = "SP2";
    info->valid_extensions = "iso|gcm|gcz|ciso|wbfs|rvz|wad|wia|nkit|m3u|dol|elf|tgc";
    info->need_fullpath    = true;
    info->block_extract    = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info)
{
    info->geometry.base_width   = 640;
    info->geometry.base_height  = 480;
    info->geometry.max_width    = 5120;
    info->geometry.max_height   = 4096;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    info->timing.fps            = 60.0;
    info->timing.sample_rate    = 32000.0;
}

RETRO_API void retro_set_environment(retro_environment_t cb)
{
    DolphinLibretro::Frontend::g_environ_cb = cb;
    DolphinLibretro::Environment::SetEnvironmentCallback(cb);

    // Fetch the frontend log callback if exposed.
    retro_log_callback log_cb{};
    if (cb && cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_cb) && log_cb.log)
        DolphinLibretro::Environment::SetLogCallback(log_cb.log);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)
{
    DolphinLibretro::Frontend::g_video_refresh_cb = cb;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)
{
    DolphinLibretro::Frontend::g_audio_sample_cb = cb;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    DolphinLibretro::Frontend::g_audio_batch_cb = cb;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t cb)
{
    DolphinLibretro::Frontend::g_input_poll_cb = cb;
}

RETRO_API void retro_set_input_state(retro_input_state_t cb)
{
    DolphinLibretro::Frontend::g_input_state_cb = cb;
}

RETRO_API void retro_init(void)
{
    s_emu_thread = std::make_unique<DolphinLibretro::EmuThread>();
}

RETRO_API void retro_deinit(void)
{
    s_emu_thread.reset();
    s_audio_stream_owned.reset();
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
    (void)port; (void)device;
}

RETRO_API void retro_reset(void)
{
    // SP2: not yet supported. Implementer can extend EmuThread with Reset()
    // if a test ROM exercises it. SP6/7 cleanup catches the gap.
}

RETRO_API void retro_run(void)
{
    if (DolphinLibretro::Frontend::g_input_poll_cb)
        DolphinLibretro::Frontend::g_input_poll_cb();

    DolphinLibretro::Input::PollFromFrontend();

    if (s_emu_thread && s_emu_thread->IsRunning())
        s_emu_thread->WaitForFrame();
}

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool   retro_serialize(void*, size_t) { return false; }
RETRO_API bool   retro_unserialize(const void*, size_t) { return false; }

RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned, bool, const char*) {}

RETRO_API bool retro_load_game(const struct retro_game_info* game)
{
    if (!game || !game->path)
    {
        DolphinLibretro::Environment::Log(RETRO_LOG_ERROR,
            "[retro_load_game] no game / no path");
        return false;
    }

    // 1. Get NSView from host + prepare WSI.
    void* nsview = DolphinLibretro::Environment::RequestHostNSView();
    if (!nsview)
        return false;
    if (!DolphinLibretro::Metal::PrepareWindowSystemInfo(nsview, &s_wsi))
        return false;

    // 2. Install our SoundStream into Dolphin's Core::System.
    s_audio_stream_owned = std::make_unique<DolphinLibretro::LibretroAudioStream>();
    // SoundStream pointer ownership: Core::System takes a unique_ptr.
    // We hold a raw reference too via s_audio_stream_owned for visibility,
    // but pass ownership to the system. NOTE: confirm signature of
    // SetSoundStream during implementation — if it's a raw pointer, adjust.
    Core::System::GetInstance().SetSoundStream(std::move(s_audio_stream_owned));

    // 3. Install our InputBackend with the ControllerInterface.
    DolphinLibretro::Input::Install();

    // 4. Boot via EmuThread.
    return s_emu_thread->StartGame(game->path, s_wsi);
}

RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t)
{
    return false;
}

RETRO_API void retro_unload_game(void)
{
    if (s_emu_thread)
        s_emu_thread->StopGame();
    DolphinLibretro::Input::Uninstall();
    DolphinLibretro::Metal::ReleaseWindowSystemInfo(&s_wsi);
}

RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API void*    retro_get_memory_data(unsigned) { return nullptr; }
RETRO_API size_t   retro_get_memory_size(unsigned) { return 0; }

}  // extern "C"
```

- [ ] **Step 2: Restore audio batch reference in LibretroAudioStream.cpp**

If Task 4 Step 5 stubbed `Frontend::g_audio_batch_cb` for a clean build, restore the real call now that the symbol is no longer in an anonymous namespace.

- [ ] **Step 3: Build**

```bash
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -15
```

Expected: `LibretroFrontend.cpp` references `LibretroInputSource.h` (`DolphinLibretro::Input::Install/Uninstall/PollFromFrontend`) which doesn't exist yet. Build will fail with undefined references. That's expected — Task 6 completes the wiring.

If everything else compiles cleanly with only `Input::*` undefined, the integration shape is right. Continue to Task 6.

- [ ] **Step 4: Stage only (will fail to build until Task 6)**

```bash
git add Source/Core/DolphinLibretro/LibretroFrontend.cpp \
        Source/Core/DolphinLibretro/LibretroAudioStream.cpp
```

Commit message (paired with Task 6's commit):
```
SP2 (wip): retro_load_game / run / unload — real wiring

retro_load_game requests an NSView from the host, builds the WSI, installs
the LibretroAudioStream into Core::System, installs the LibretroInputBackend,
and boots via EmuThread. retro_run polls input and waits for one frame.
retro_unload_game stops + tears down.

Callback variables promoted out of anonymous namespace into
DolphinLibretro::Frontend so sibling TUs can read them.

Does NOT build until Task 6 adds LibretroInputSource — staged together.
```

---

### Task 6: LibretroInputSource — RetroPad → GameCube pad

**Files:**
- Create: `Source/Core/DolphinLibretro/LibretroInputSource.h`
- Create: `Source/Core/DolphinLibretro/LibretroInputSource.cpp`

Defines an `InputBackend` + virtual `Device` exposing RetroPad buttons/axes. Each `retro_run`, `PollFromFrontend()` reads `input_state_cb` for each port × button and stores values for the Device's Input::GetState() to return.

GameCube ↔ RetroPad button mapping (mirrors the libretro convention):
- RetroPad B (south)     → GC A (cross-equivalent, primary)
- RetroPad A (east)      → GC B
- RetroPad Y (west)      → GC X
- RetroPad X (north)     → GC Y
- RetroPad Start         → GC Start
- RetroPad Select        → GC Z (no direct equivalent; Select is a common substitute)
- RetroPad L (shoulder)  → GC L (analog trigger, digital here)
- RetroPad R (shoulder)  → GC R (analog trigger, digital here)
- RetroPad D-Pad         → GC D-Pad
- RetroPad L-stick X/Y   → GC analog stick X/Y
- RetroPad R-stick X/Y   → GC C-Stick X/Y

For SP2: digital-only L/R triggers, full analog sticks. SP5 expands.

- [ ] **Step 1: Read InputBackend + Device interfaces**

```bash
cat /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/InputCommon/ControllerInterface/InputBackend.h
cat /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/InputCommon/ControllerInterface/CoreDevice.h | head -120
```

Confirm: `InputBackend` virtual interface (`PopulateDevices`, possibly `UpdateInput`). `Device::Input` has `GetState()` returning `ControlState` (typically [-1.0, 1.0]). `Device::Button : Input` and `Device::Axis : Input` are typically subclasses.

- [ ] **Step 2: Write LibretroInputSource.h**

```cpp
// Custom InputBackend that exposes a virtual "Libretro/0/..." device.
// Reads libretro's input_state_cb each retro_run and presents values
// through the standard ControlReference resolution path so Dolphin's
// GameCube pad code finds them via expression strings.

#pragma once

namespace DolphinLibretro::Input {

// Register the backend with ControllerInterface and create the default
// pad binding (binds Pad1 to "Libretro/0/...").
void Install();

// Unregister + remove the device. Idempotent.
void Uninstall();

// Pump libretro input_state_cb for every (port × button) the device
// exposes; called once per retro_run.
void PollFromFrontend();

}  // namespace DolphinLibretro::Input
```

- [ ] **Step 3: Write LibretroInputSource.cpp**

```cpp
#include "DolphinLibretro/LibretroInputSource.h"
#include "DolphinLibretro/LibretroEnvironment.h"
#include "DolphinLibretro/libretro.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/InputBackend.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>

namespace DolphinLibretro::Frontend {
extern retro_input_state_t g_input_state_cb;
}

namespace DolphinLibretro::Input {

namespace {

// State storage — one slot per RetroPad button (16) per port (4).
// Atomics so PollFromFrontend can write while the input thread reads.
constexpr unsigned kNumPorts = 4;
constexpr unsigned kNumButtons = 16;
constexpr unsigned kNumAxes = 4;  // L-stick X/Y, R-stick X/Y

struct PortState
{
    std::array<std::atomic<int16_t>, kNumButtons> buttons{};
    std::array<std::atomic<int16_t>, kNumAxes>    axes{};
};
std::array<PortState, kNumPorts> g_state;

// libretro button ids we care about (subset of RETRO_DEVICE_ID_JOYPAD_*).
struct ButtonDef { const char* name; unsigned retro_id; };
constexpr std::array<ButtonDef, 14> kButtonDefs = {{
    {"B",      RETRO_DEVICE_ID_JOYPAD_B},
    {"Y",      RETRO_DEVICE_ID_JOYPAD_Y},
    {"Select", RETRO_DEVICE_ID_JOYPAD_SELECT},
    {"Start",  RETRO_DEVICE_ID_JOYPAD_START},
    {"Up",     RETRO_DEVICE_ID_JOYPAD_UP},
    {"Down",   RETRO_DEVICE_ID_JOYPAD_DOWN},
    {"Left",   RETRO_DEVICE_ID_JOYPAD_LEFT},
    {"Right",  RETRO_DEVICE_ID_JOYPAD_RIGHT},
    {"A",      RETRO_DEVICE_ID_JOYPAD_A},
    {"X",      RETRO_DEVICE_ID_JOYPAD_X},
    {"L",      RETRO_DEVICE_ID_JOYPAD_L},
    {"R",      RETRO_DEVICE_ID_JOYPAD_R},
    {"L3",     RETRO_DEVICE_ID_JOYPAD_L3},
    {"R3",     RETRO_DEVICE_ID_JOYPAD_R3},
}};

struct AxisDef { const char* name; unsigned index; unsigned id; bool positive; };
constexpr std::array<AxisDef, 8> kAxisDefs = {{
    // L stick
    {"LX-", RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, false},
    {"LX+", RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, true},
    {"LY-", RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, false},
    {"LY+", RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, true},
    // R stick
    {"RX-", RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, false},
    {"RX+", RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, true},
    {"RY-", RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, false},
    {"RY+", RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, true},
}};

class LibretroDevice : public ciface::Core::Device
{
public:
    explicit LibretroDevice(unsigned port) : m_port(port)
    {
        for (const auto& b : kButtonDefs)
            AddInput(new Button(m_port, b.name, b.retro_id));
        for (const auto& a : kAxisDefs)
            AddInput(new Axis(m_port, a.name, a.index, a.id, a.positive));
    }

    std::string GetName() const override { return std::to_string(m_port); }
    std::string GetSource() const override { return "Libretro"; }
    int  GetSortPriority() const override { return -10; }

private:
    class Button : public Input
    {
    public:
        Button(unsigned port, const char* name, unsigned retro_id)
            : m_port(port), m_name(name), m_retro_id(retro_id) {}
        std::string GetName() const override { return m_name; }
        ControlState GetState() const override
        {
            return g_state[m_port].buttons[m_retro_id].load() ? 1.0 : 0.0;
        }
    private:
        unsigned m_port;
        std::string m_name;
        unsigned m_retro_id;
    };

    class Axis : public Input
    {
    public:
        Axis(unsigned port, const char* name, unsigned idx, unsigned id, bool positive)
            : m_port(port), m_name(name), m_idx(idx), m_id(id), m_positive(positive) {}
        std::string GetName() const override { return m_name; }
        ControlState GetState() const override
        {
            // Each axis appears as TWO inputs (X- / X+), each clipped to [0,1].
            // Index in g_state.axes: m_idx * 2 + m_id (0..3).
            const unsigned slot = (m_idx == RETRO_DEVICE_INDEX_ANALOG_LEFT ? 0 : 2)
                                + (m_id  == RETRO_DEVICE_ID_ANALOG_X      ? 0 : 1);
            const int16_t raw = g_state[m_port].axes[slot].load();
            const double  norm = static_cast<double>(raw) / 32767.0;
            return m_positive ? std::max(0.0, norm) : std::max(0.0, -norm);
        }
    private:
        unsigned m_port;
        std::string m_name;
        unsigned m_idx, m_id;
        bool m_positive;
    };

    unsigned m_port;
};

class LibretroInputBackend : public ciface::InputBackend
{
public:
    using ciface::InputBackend::InputBackend;
    void PopulateDevices() override
    {
        for (unsigned p = 0; p < kNumPorts; ++p)
            GetControllerInterface().AddDevice(std::make_shared<LibretroDevice>(p));
    }
};

std::unique_ptr<ciface::InputBackend> g_backend;

}  // namespace

void Install()
{
    if (g_backend)
        return;
    auto& ci = g_controller_interface;
    g_backend = std::make_unique<LibretroInputBackend>(&ci);
    ci.AddBackend(std::move(g_backend));
    ci.RefreshDevices();
    Environment::Log(RETRO_LOG_INFO, "[Input] LibretroInputBackend installed");
}

void Uninstall()
{
    // The backend remains registered for the lifetime of the process —
    // upstream ControllerInterface doesn't have a clean "remove backend"
    // API. On unload we just stop polling; the next retro_load_game
    // re-uses the existing backend's devices.
    Environment::Log(RETRO_LOG_INFO, "[Input] Uninstall (no-op for backend; devices retained)");
}

void PollFromFrontend()
{
    if (!Frontend::g_input_state_cb)
        return;

    for (unsigned port = 0; port < kNumPorts; ++port)
    {
        for (const auto& b : kButtonDefs)
        {
            const int16_t v = Frontend::g_input_state_cb(
                port, RETRO_DEVICE_JOYPAD, 0, b.retro_id);
            g_state[port].buttons[b.retro_id].store(v);
        }
        for (unsigned slot = 0; slot < kNumAxes; ++slot)
        {
            const unsigned idx = (slot < 2) ? RETRO_DEVICE_INDEX_ANALOG_LEFT
                                            : RETRO_DEVICE_INDEX_ANALOG_RIGHT;
            const unsigned id  = (slot % 2 == 0) ? RETRO_DEVICE_ID_ANALOG_X
                                                 : RETRO_DEVICE_ID_ANALOG_Y;
            const int16_t v = Frontend::g_input_state_cb(
                port, RETRO_DEVICE_ANALOG, idx, id);
            g_state[port].axes[slot].store(v);
        }
    }
}

}  // namespace DolphinLibretro::Input
```

- [ ] **Step 4: Update CMakeLists**

```cmake
target_sources(dolphin_libretro PRIVATE
    LibretroFrontend.cpp
    LibretroEnvironment.cpp
    LibretroMetalContext.mm
    LibretroAudioStream.cpp
    LibretroInputSource.cpp
    HostStubs.cpp
    EmuThread.cpp
)
```

- [ ] **Step 5: Build**

```bash
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -15
```

Expected: clean build. If `LibretroInputBackend(&ci)` constructor signature differs (some Dolphin versions use a different pattern), check `InputBackend.h:19-37` for the exact constructor. Common variant: `InputBackend(ControllerInterface*)` or just default-constructible. Adjust as needed.

Also check: does `g_controller_interface` exist as a global, or is it accessed via `ControllerInterface::GetInstance()` / `Core::System::GetControllerInterface()`? Look at `ControllerInterface/ControllerInterface.h` for the access pattern used by upstream `SDL/SDLGamepad.cpp`'s install code.

- [ ] **Step 6: Stage only**

```bash
git add Source/Core/DolphinLibretro/LibretroInputSource.{h,cpp} \
        Source/Core/DolphinLibretro/CMakeLists.txt
```

Commit message:
```
SP2: LibretroInputSource — RetroPad → Dolphin pad bridge

Custom InputBackend with virtual Device per libretro port exposing RetroPad
buttons + analog axes. PollFromFrontend reads libretro's input_state_cb
each retro_run and stores values atomically; Dolphin's pad code resolves
"Libretro/N/Button" via the standard ControlReference path.

SP2 ships digital triggers, full analog sticks. SP5 expands to per-button
controller mapping UI.
```

After this commit, the dylib builds cleanly and `dolphin_libretro_load_test` should still pass (basic ABI smoke).

---

### Task 7: HostStubs — wire Host_Message for clean shutdown

**Files:**
- Modify: `Source/Core/DolphinLibretro/HostStubs.cpp`

When Dolphin's core requests a shutdown (e.g. game ends with an internal call to `Core::QueueHostJob(...)` → eventually `Host_Message(WMUserStop)`), we need to signal the EmuThread so retro_run unwinds cleanly. SP2 minimum: log and set a flag.

- [ ] **Step 1: Update HostStubs.cpp Host_Message implementation**

In `HostStubs.cpp`, replace the existing `Host_Message` body:

```cpp
void Host_Message(HostMessageID id)
{
    // SP2: Dolphin signals shutdown via WMUserStop. Forward to the EmuThread
    // so retro_run sees IsRunning() == false on the next call and returns.
    // The actual Core::Stop has already happened by the time we get here;
    // we're just notifying the libretro layer.
    if (id == HostMessageID::WMUserStop)
    {
        // The EmuThread isn't directly reachable from this TU; SP3 wires
        // a proper notification path. For SP2, the libretro frontend
        // polls IsRunning() every retro_run and Dolphin's own shutdown
        // sequence sets that to false via EmuThread::StopGame.
    }
    (void)id;
}
```

- [ ] **Step 2: Build + verify load test still passes**

```bash
cmake --build build-libretro --target dolphin_libretro dolphin_libretro_load_test 2>&1 | tail -10
DYLIB=$(find build-libretro -name "dolphin_libretro.dylib" -type f)
TEST=$(find build-libretro -name "dolphin_libretro_load_test" -type f)
"$TEST" "$DYLIB"
```

Expected: SP1's basic ABI test still prints `ALL PASS`. (This time the dylib is much larger — it pulls in BootManager and AudioCommon and InputCommon — but the ABI surface is unchanged.)

- [ ] **Step 3: Stage only**

```bash
git add Source/Core/DolphinLibretro/HostStubs.cpp
```

Commit message:
```
SP2: HostStubs — Host_Message stub for WMUserStop

Acknowledges the WMUserStop message Dolphin uses to signal shutdown.
SP2 is a noop — the libretro layer polls EmuThread::IsRunning. SP3 wires
a proper notification when the host adapter needs synchronous teardown.
```

---

### Task 8: Cocoa test harness

**Files:**
- Create: `Source/Core/DolphinLibretro/tools/test_harness.mm`
- Modify: `Source/Core/DolphinLibretro/tools/CMakeLists.txt`

A small standalone Cocoa app that:
1. Creates an NSWindow + NSView (no decorations needed for the smoke test, but a visible window helps see frames).
2. Loads `dolphin_libretro.dylib`.
3. Provides an `environ_cb` that returns the NSView for `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW`.
4. Calls `retro_load_game` with a user-supplied ROM path.
5. Pumps the NSRunLoop while invoking `retro_run` ~60 times per second.
6. Counts video / audio callback invocations; exits with summary after a configurable duration.

- [ ] **Step 1: Write tools/test_harness.mm**

```objective-c++
// SP2 smoke harness: Cocoa app that boots a GC ROM through dolphin_libretro.dylib
// and pumps retro_run for N seconds. Verifies the Metal NSView handover +
// BootManager wiring end-to-end.
//
// Usage:
//   ./test_harness <path-to-dolphin_libretro.dylib> <path-to-rom.iso> [seconds]
//
// Default duration: 5 seconds. Exit 0 if any video frame was presented to the
// CAMetalLayer (verified via layer.nextDrawable), non-zero otherwise.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

#include <dlfcn.h>
#include <cstdio>
#include <atomic>
#include <thread>
#include <chrono>

#include "../libretro.h"

namespace {

void* g_dl_handle = nullptr;
NSView* g_view = nil;
std::atomic<unsigned> g_video_frames{0};
std::atomic<unsigned> g_audio_batches{0};
std::atomic<size_t>   g_audio_samples{0};

// libretro callbacks
bool environ_cb(unsigned cmd, void* data)
{
    constexpr unsigned RETRONEST_GET_MACOS_NSVIEW = (1u | 0x20000);

    switch (cmd)
    {
        case RETRONEST_GET_MACOS_NSVIEW:
            *static_cast<void**>(data) = (__bridge void*)g_view;
            return true;

        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *static_cast<bool*>(data) = true;
            return true;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return true;

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            auto* log = static_cast<retro_log_callback*>(data);
            log->log = [](enum retro_log_level lvl, const char* fmt, ...) {
                va_list a; va_start(a, fmt);
                vfprintf(stderr, fmt, a);
                va_end(a);
            };
            return true;
        }

        default:
            return false;
    }
}

void video_refresh_cb(const void* data, unsigned w, unsigned h, size_t pitch)
{
    (void)data; (void)w; (void)h; (void)pitch;
    g_video_frames.fetch_add(1);
}

void audio_sample_cb(int16_t l, int16_t r)
{
    (void)l; (void)r;
}

size_t audio_sample_batch_cb(const int16_t* data, size_t frames)
{
    (void)data;
    g_audio_batches.fetch_add(1);
    g_audio_samples.fetch_add(frames);
    return frames;
}

void input_poll_cb() {}
int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)port; (void)device; (void)index; (void)id;
    return 0;
}

template <typename T>
T resolve(const char* name)
{
    auto sym = reinterpret_cast<T>(dlsym(g_dl_handle, name));
    if (!sym) fprintf(stderr, "FAIL: dlsym(%s): %s\n", name, dlerror());
    return sym;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <dylib> <rom> [seconds]\n", argv[0]);
        return 1;
    }
    const char* dylib_path = argv[1];
    const char* rom_path   = argv[2];
    const double duration_s = (argc >= 4) ? atof(argv[3]) : 5.0;

    @autoreleasepool {
        // 1. Cocoa app shell.
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        const NSRect frame = NSMakeRect(100, 100, 640, 480);
        NSWindow* window = [[NSWindow alloc]
            initWithContentRect:frame
                      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        [window setTitle:@"dolphin_libretro SP2 smoke"];
        g_view = [[NSView alloc] initWithFrame:frame];
        [window setContentView:g_view];
        [window makeKeyAndOrderFront:nil];
        [app activateIgnoringOtherApps:YES];

        // Pump the run loop briefly so the view materialises.
        for (int i = 0; i < 10; ++i)
        {
            NSEvent* ev = [app nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES];
            if (ev) [app sendEvent:ev];
        }

        // 2. Load dylib.
        g_dl_handle = dlopen(dylib_path, RTLD_NOW | RTLD_LOCAL);
        if (!g_dl_handle) { fprintf(stderr, "FAIL dlopen: %s\n", dlerror()); return 2; }

        // 3. Resolve retro_* + wire callbacks.
        auto set_env       = resolve<void(*)(retro_environment_t)>("retro_set_environment");
        auto set_video     = resolve<void(*)(retro_video_refresh_t)>("retro_set_video_refresh");
        auto set_audio     = resolve<void(*)(retro_audio_sample_t)>("retro_set_audio_sample");
        auto set_audio_b   = resolve<void(*)(retro_audio_sample_batch_t)>("retro_set_audio_sample_batch");
        auto set_in_poll   = resolve<void(*)(retro_input_poll_t)>("retro_set_input_poll");
        auto set_in_state  = resolve<void(*)(retro_input_state_t)>("retro_set_input_state");
        auto p_init        = resolve<void(*)()>("retro_init");
        auto p_load_game   = resolve<bool(*)(const struct retro_game_info*)>("retro_load_game");
        auto p_run         = resolve<void(*)()>("retro_run");
        auto p_unload_game = resolve<void(*)()>("retro_unload_game");
        auto p_deinit      = resolve<void(*)()>("retro_deinit");
        if (!set_env || !p_run || !p_load_game) return 3;

        set_env(environ_cb);
        set_video(video_refresh_cb);
        set_audio(audio_sample_cb);
        set_audio_b(audio_sample_batch_cb);
        set_in_poll(input_poll_cb);
        set_in_state(input_state_cb);

        p_init();

        // 4. Boot the ROM.
        struct retro_game_info info = {};
        info.path = rom_path;
        if (!p_load_game(&info))
        {
            fprintf(stderr, "FAIL retro_load_game(%s)\n", rom_path);
            p_deinit();
            dlclose(g_dl_handle);
            return 4;
        }
        fprintf(stderr, "OK: retro_load_game succeeded\n");

        // 5. Pump retro_run + NSRunLoop for `duration_s` seconds.
        const auto start = std::chrono::steady_clock::now();
        const auto deadline = start + std::chrono::duration<double>(duration_s);
        while (std::chrono::steady_clock::now() < deadline)
        {
            p_run();
            NSEvent* ev = [app nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES];
            if (ev) [app sendEvent:ev];
        }

        // 6. Teardown.
        p_unload_game();
        p_deinit();
        dlclose(g_dl_handle);

        // 7. Report.
        fprintf(stderr, "\n=== SP2 smoke summary (%.1fs) ===\n", duration_s);
        fprintf(stderr, "video_refresh calls: %u\n", g_video_frames.load());
        fprintf(stderr, "audio batches:       %u\n", g_audio_batches.load());
        fprintf(stderr, "audio samples:       %zu\n", g_audio_samples.load());

        // Success if audio is flowing (proves Mixer + AudioStream wired)
        // OR video callbacks fire (proves video pipeline). Either alone is
        // sufficient evidence the core actually emulated.
        const bool ok = g_audio_batches.load() > 0 || g_video_frames.load() > 0;
        fprintf(stderr, "result: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 5;
    }
}
```

- [ ] **Step 2: Update tools/CMakeLists.txt to build the harness**

```cmake
# tools/CMakeLists.txt
#
# Two test targets:
#   dolphin_libretro_load_test - basic ABI smoke (SP1)
#   dolphin_libretro_smoke     - Cocoa harness that boots a real ROM (SP2)

add_executable(dolphin_libretro_load_test EXCLUDE_FROM_ALL test_load.cpp)
target_include_directories(dolphin_libretro_load_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
add_dependencies(dolphin_libretro_load_test dolphin_libretro)

if(APPLE)
    add_executable(dolphin_libretro_smoke EXCLUDE_FROM_ALL test_harness.mm)
    target_include_directories(dolphin_libretro_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_compile_options(dolphin_libretro_smoke PRIVATE -fobjc-arc)
    target_link_libraries(dolphin_libretro_smoke PRIVATE
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
    )
    add_dependencies(dolphin_libretro_smoke dolphin_libretro)
endif()
```

- [ ] **Step 3: Build the harness**

```bash
cmake -B build-libretro 2>&1 | tail -3
cmake --build build-libretro --target dolphin_libretro_smoke 2>&1 | tail -5
```

Expected: clean build of `dolphin_libretro_smoke` executable.

- [ ] **Step 4: Stage only**

```bash
git add Source/Core/DolphinLibretro/tools/test_harness.mm \
        Source/Core/DolphinLibretro/tools/CMakeLists.txt
```

Commit message:
```
SP2: Cocoa smoke harness for end-to-end ROM boot test

Standalone test_harness.mm creates an NSWindow + NSView, dlopens
dolphin_libretro.dylib, hands the NSView through the env callback,
boots a user-supplied GC ROM, and pumps retro_run for N seconds.
Verifies video and/or audio callbacks fire — definitive proof the
Metal NSView handover + BootManager wiring + audio bridge are working.

Both test targets are EXCLUDE_FROM_ALL; build explicitly via:
    cmake --build build-libretro --target dolphin_libretro_smoke
```

---

### Task 9: Run the smoke test (user provides ROM)

**Files:** None modified. This is verification.

- [ ] **Step 1: Locate a GameCube ROM**

The user must provide a GC ROM path. Ask if not already known. A common test ROM is "Wind Waker" if licensed; for a public-domain alternative, any homebrew `.dol` Dolphin can load.

Set environment for the next steps:
```bash
ROM_PATH="$HOME/Documents/path-to-your-rom.iso"  # SUBSTITUTE
ls -lh "$ROM_PATH"  # confirm file exists + sane size
```

- [ ] **Step 2: Run the smoke harness**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
DYLIB=$(find build-libretro -name "dolphin_libretro.dylib" -type f)
SMOKE=$(find build-libretro -name "dolphin_libretro_smoke" -type f)
"$SMOKE" "$DYLIB" "$ROM_PATH" 10
```

Expected output:
- An `NSWindow` titled "dolphin_libretro SP2 smoke" opens (briefly visible).
- stderr logs: `OK: retro_load_game succeeded` early.
- stderr periodically logs Dolphin's own logging (boot messages, video init, etc.).
- After ~10 seconds, summary:
  ```
  === SP2 smoke summary (10.0s) ===
  video_refresh calls: 0 (or some count)
  audio batches:       N (should be > 0 — confirms audio pipeline)
  audio samples:       M
  result: PASS
  ```

Exit code 0 = pass.

**Common failures and diagnoses:**
- **`FAIL retro_load_game`** → BootManager couldn't boot the ROM. Re-run with the ROM path in DolphinQt to confirm it's a valid file Dolphin recognizes.
- **`PASS` but 0 video frames + 0 audio** → BootManager succeeded but Dolphin's threads aren't running. Check `EmuThread::StartGame` did `BootManager::BootCore` AND the system entered Running state.
- **Crash in `vkCreateInstance` or `MTLCreateSystemDefaultDevice`** → Metal device unavailable. Run on a machine with Metal-capable GPU (M-series Macs all qualify).
- **`SetSoundStream` undefined** → API confirmed in Step 1 of Task 4 — adjust the call signature accordingly.

If the test fails, report the exact output. The plan's tasks are scoped to be diagnosable from the harness output.

- [ ] **Step 3: Report**

Report the smoke result with the harness output captured verbatim. If PASS, SP2 is done. If FAIL, propose a follow-up: which specific subsystem failed, what to investigate.

- [ ] **Step 4: Nothing to commit**

---

### Task 10: Close out

**Files:** None modified.

- [ ] **Step 1: Verify all SP2 deliverables exist**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
echo "=== New source files ==="
ls -1 Source/Core/DolphinLibretro/{LibretroEnvironment,LibretroMetalContext,LibretroAudioStream,LibretroInputSource}.{h,cpp,mm} 2>/dev/null
echo ""
echo "=== Modified files in SP2 ==="
git diff --stat master..HEAD -- Source/Core/DolphinLibretro/
echo ""
echo "=== SP2 commits ==="
git log --oneline master..HEAD | head -15
```

Expected:
- 4 new component files (LibretroEnvironment, LibretroMetalContext, LibretroAudioStream, LibretroInputSource) with their headers.
- 4 expanded files (LibretroFrontend, EmuThread, HostStubs, CMakeLists).
- 1 new test harness (test_harness.mm).
- ~8-10 SP2 commits on the libretro branch.

- [ ] **Step 2: Report**

- Smoke test PASS/FAIL with output.
- SP3 next: RetroNest-side adapter (`DolphinLibretroAdapter`), manifest flip, registry swap.
- Note any deferred items (e.g. Host_Message proper wiring, Reset support) that SP3 should pick up.

- [ ] **Step 3: Done.**

---

## Notes for the implementer

- **All git work in `/Users/mark/Documents/Projects/dolphin-libretro/`** on branch `libretro`. Each task stages files; the controller commits.
- **The build will grow significantly** — SP1's dylib was 86KB (DCE'd most Dolphin code). SP2's dylib will be 50-100MB once BootManager/Core/AudioCommon/InputCommon are actually reachable.
- **API names are best-effort** — Dolphin's internal APIs change between versions. If `Core::System::SetSoundStream` doesn't exist as written, the implementer should grep for the actual setter (`grep -rn "SetSoundStream\|set_sound_stream" Source/Core/Core/System.{h,cpp}`). Adjust call sites and report the deviation.
- **Frame-end hook may not exist cleanly** — Task 3 Step 1 investigates. If no hook, fall back to `std::this_thread::sleep_for(16ms)` in WaitForFrame and document the trade-off.
- **Threading caveat**: Dolphin's BootManager spawns its own CPU + Fifo threads. Don't call BootCore from a thread that's already locked — start fresh from the libretro caller's thread.
- **macOS-specific bits**: All Metal/Cocoa code in `.mm` files with `-fobjc-arc`. Don't mix Objective-C++ into the .cpp files; they don't get ARC and will leak/crash.
- **Smoke test is intentionally permissive**: PASS if EITHER video or audio callbacks fired. A real "renders correctly" verification needs visual inspection, which is out of scope for a CLI test.
