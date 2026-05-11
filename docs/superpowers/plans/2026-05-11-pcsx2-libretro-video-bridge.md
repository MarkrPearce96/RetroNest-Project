# PCSX2 Libretro Core — HW Render Bridge / Video Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the host-provided HW render bridge so PCSX2 renders directly to a CAMetalLayer hosted inside a native NSView inside RetroNest's main Qt window. End-to-end success = "user sees the PS2 BIOS boot logo / game title screen inside RetroNest within ~10 seconds of clicking Launch."

**Architecture:** Adds a parallel hardware-rendering path alongside the existing mGBA software path. RetroNest side: new `LibretroMetalItem` QQuickItem backed by a native QWindow whose `winId()` is an NSView. The NSView pointer is registered with `CoreRuntime` and served to the libretro core via a new private env command `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` (= `1 | RETRO_ENVIRONMENT_PRIVATE` = `0x20001`). `EmulationView.qml` switches between `LibretroVideoItem` (software, mGBA) and `LibretroMetalItem` (hardware, PCSX2) based on `Pcsx2LibretroAdapter::prefersHardwareRender() = true`. pcsx2-libretro fork side: `Host::AcquireRenderWindow` queries the env command and returns a `Type::MacOS` `WindowInfo`. `Host::BeginPresentFrame` signals a frame-ready condition variable; `retro_run` blocks on it (with timeout) to drive frame cadence.

**Tech Stack:** Qt 6 (QQuickItem, QWindow, native winId() = NSView on macOS), Objective-C++ (.mm files for the Metal-aware widget), `<condition_variable>` + `<mutex>` for cross-thread frame signaling, libretro env command in the `RETRO_ENVIRONMENT_PRIVATE` range, PCSX2's existing `GSDeviceMTL` consumes our supplied NSView via `WindowInfo.window_handle`.

**Spec:** [2026-05-11-pcsx2-libretro-video-bridge-design.md](../specs/2026-05-11-pcsx2-libretro-video-bridge-design.md)
**Predecessor sub-projects:** SP1 (skeleton) and SP2 (VM lifecycle), both complete.

**Conventions used in this plan:**
- `PCSX2_ROOT` = `/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master`
- `RETRONEST_ROOT` = `/Users/mark/Documents/Projects/RetroNest-Project`
- `RETRONEST_DATA_ROOT` = `/Users/mark/Documents/RetroNest`
- All pcsx2-libretro-fork work continues on the `retronest-libretro` branch. All RetroNest work on `main` (matching the established project pattern).

**File structure (this entire phase):**

| File | Created or modified | Side | Purpose |
|---|---|---|---|
| `${RETRONEST_ROOT}/cpp/src/ui/libretro/libretro_metal_item.h` | created | RetroNest | QQuickItem decl with nativeView() accessor. |
| `${RETRONEST_ROOT}/cpp/src/ui/libretro/libretro_metal_item.mm` | created | RetroNest | ObjC++ impl: layer-backed NSView wrapped in a QWindow. |
| `${RETRONEST_ROOT}/cpp/src/core/libretro/environment_callbacks.cpp` | modified | RetroNest | Adds `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` handler. |
| `${RETRONEST_ROOT}/cpp/src/core/libretro/environment_callbacks.h` | modified | RetroNest | Declares the env command constant. |
| `${RETRONEST_ROOT}/cpp/src/core/libretro/core_runtime.h` | modified | RetroNest | `setActiveNSView(void*) / clearActiveNSView()`. |
| `${RETRONEST_ROOT}/cpp/src/core/libretro/core_runtime.cpp` | modified | RetroNest | Stores NSView pointer; env callback reads from it. |
| `${RETRONEST_ROOT}/cpp/src/adapters/libretro/libretro_adapter.h` | modified | RetroNest | `virtual bool prefersHardwareRender() const { return false; }`. |
| `${RETRONEST_ROOT}/cpp/src/adapters/libretro/pcsx2_libretro_adapter.h` | modified | RetroNest | Overrides `prefersHardwareRender() = true`. |
| `${RETRONEST_ROOT}/cpp/src/core/game_session.h` | modified | RetroNest | `Q_PROPERTY libretroBackend`. |
| `${RETRONEST_ROOT}/cpp/src/core/game_session.cpp` | modified | RetroNest | Sets backend before launching; calls CoreRuntime::setActiveNSView. |
| `${RETRONEST_ROOT}/cpp/qml/AppUI/EmulationView.qml` | modified | RetroNest | `Loader` conditional between LibretroVideoItem and LibretroMetalItem. |
| `${RETRONEST_ROOT}/cpp/CMakeLists.txt` | modified | RetroNest | Adds the new .h/.mm to sources + headers lists. |
| `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp` | modified | fork | `AcquireRenderWindow` queries env command; `BeginPresentFrame` signals cv. |
| `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.cpp` | modified | fork | `retro_run` blocks on cv with timeout; library_version → `"video-0.1"`. |
| `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h` | modified | fork | Declares the present-frame cv extern. |

**Commit checkpoints:** RetroNest changes commit in 2 logical commits (one for the env infrastructure + adapter wiring, one for the QQuickItem + QML changes). pcsx2-libretro changes commit in 1 commit. Then verification work. ~3 substantive commits + final spec-complete commit.

**Order of work:** RetroNest first (so the env command exists and the QQuickItem builds), then pcsx2-libretro consumes them. End-to-end verification gates everything.

---

## Task 1: Add the private env command constant to environment_callbacks.h

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/src/core/libretro/environment_callbacks.h`

- [ ] **Step 1: Find the right insertion point**

Run:
```sh
head -30 "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/environment_callbacks.h"
```

Expected: header guards + includes + a class/namespace declaration.

- [ ] **Step 2: Add the private env command constant**

Open `${RETRONEST_ROOT}/cpp/src/core/libretro/environment_callbacks.h`. Find the line `#pragma once` (or the existing header guard). Immediately after the `#include "libretro.h"` (or vendored `libretro-api/libretro.h`) line, add:

```cpp

// RetroNest-private env command IDs (RETRO_ENVIRONMENT_PRIVATE = 0x20000).
//
// 0x20001 — RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW
//           Used by pcsx2_libretro (and future Metal-backed cores) to fetch
//           the NSView* that hosts the core's CAMetalLayer. Output pointer
//           is `void**` written to by the host. Returns false if no native
//           view is registered (mGBA / software cores hit this case).
#define RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW (1 | RETRO_ENVIRONMENT_PRIVATE)
```

- [ ] **Step 3: Verify it compiles**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build 2>&1 | tail -5
```

Expected: builds without error (the constant is unused so far). If build complains "RETRO_ENVIRONMENT_PRIVATE not defined", the libretro.h include path is wrong — check for `#include "libretro.h"` in the file or update.

Use timeout: 600000.

(No commit yet — Task 4 commits all env-related changes together.)

---

## Task 2: Add NSView pointer storage to CoreRuntime

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/src/core/libretro/core_runtime.h`
- Modify: `${RETRONEST_ROOT}/cpp/src/core/libretro/core_runtime.cpp`

- [ ] **Step 1: Add accessors to core_runtime.h**

Open `${RETRONEST_ROOT}/cpp/src/core/libretro/core_runtime.h`. Find the `class CoreRuntime` declaration. In its public section, add:

```cpp
    /**
     * Register a native NSView pointer for the libretro core to consume via
     * RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW. Call before retro_load_game when
     * the active adapter prefers hardware rendering. Pass nullptr to clear.
     *
     * Stored as void* so this header doesn't drag in Objective-C++. The actual
     * NSView* is provided by LibretroMetalItem on macOS.
     */
    void setActiveNSView(void* ns_view);
    void* activeNSView() const;
```

In the class's private member section, add:

```cpp
    std::atomic<void*> m_active_ns_view{nullptr};
```

If `<atomic>` isn't already included, add `#include <atomic>` near the top.

- [ ] **Step 2: Implement the accessors in core_runtime.cpp**

Open `${RETRONEST_ROOT}/cpp/src/core/libretro/core_runtime.cpp`. At the end of the file (before any closing namespace brace), add:

```cpp

void CoreRuntime::setActiveNSView(void* ns_view) {
    m_active_ns_view.store(ns_view, std::memory_order_release);
}

void* CoreRuntime::activeNSView() const {
    return m_active_ns_view.load(std::memory_order_acquire);
}
```

- [ ] **Step 3: Verify the build**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build 2>&1 | tail -5
```

Expected: builds clean.

Use timeout: 600000. No commit yet.

---

## Task 3: Wire the env command handler in environment_callbacks.cpp

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/src/core/libretro/environment_callbacks.cpp`

- [ ] **Step 1: Find the switch statement that dispatches env commands**

Run:
```sh
grep -n "switch\s*(cmd\|switch(cmd\|RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY" "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/environment_callbacks.cpp" | head -5
```

Expected: shows the switch location (around line 50 based on earlier exploration). The switch handles `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY`, `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT`, etc.

- [ ] **Step 2: Add the case for RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW**

In the switch statement, add a new case alongside the others (place it after `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` for readability):

```cpp
        case RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW: {
            if (!data) return false;
            void* ns_view = m_runtime->activeNSView();
            if (!ns_view) return false;
            *reinterpret_cast<void**>(data) = ns_view;
            return true;
        }
```

The handler returns false (env command "not supported in this state") if no NSView is registered — which is the right semantic for mGBA and other software cores.

- [ ] **Step 3: Verify the build**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build 2>&1 | tail -5
```

Expected: builds clean. The env handler now responds to our private command.

Use timeout: 600000. No commit yet — Task 4 commits.

---

## Task 4: Add prefersHardwareRender() virtual + commit env infrastructure

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/src/adapters/libretro/libretro_adapter.h`
- Modify: `${RETRONEST_ROOT}/cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`

- [ ] **Step 1: Add the virtual to the base adapter**

Open `${RETRONEST_ROOT}/cpp/src/adapters/libretro/libretro_adapter.h`. Find the `class LibretroAdapter : public ...` declaration. In its public section, add:

```cpp
    /**
     * True if this core requires a hardware-rendering host context (CAMetalLayer
     * on macOS). When true, EmulationView hosts LibretroMetalItem and registers
     * its NSView with CoreRuntime before retro_load_game. When false (default),
     * EmulationView hosts LibretroVideoItem and the core renders in software.
     */
    virtual bool prefersHardwareRender() const { return false; }
```

- [ ] **Step 2: Override in Pcsx2LibretroAdapter**

Open `${RETRONEST_ROOT}/cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`. Inside the `class Pcsx2LibretroAdapter : public LibretroAdapter` block (currently very minimal), add to the public section:

```cpp
    bool prefersHardwareRender() const override { return true; }
```

- [ ] **Step 3: Build and verify**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 4: Commit env infrastructure + adapter wiring**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add cpp/src/core/libretro/environment_callbacks.h \
        cpp/src/core/libretro/environment_callbacks.cpp \
        cpp/src/core/libretro/core_runtime.h \
        cpp/src/core/libretro/core_runtime.cpp \
        cpp/src/adapters/libretro/libretro_adapter.h \
        cpp/src/adapters/libretro/pcsx2_libretro_adapter.h
git status
```

Expected: only those 6 files staged.

```sh
git commit -m "$(cat <<'EOF'
libretro: scaffold HW render context env command + adapter flag

Wires the host side of pattern B (host-provided HW render context)
without yet creating the Metal-backed widget that consumes it:

- environment_callbacks.h: new RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW
  (= 1 | RETRO_ENVIRONMENT_PRIVATE = 0x20001). Used by Metal-backed
  libretro cores to fetch the NSView* that hosts their CAMetalLayer.

- core_runtime.{h,cpp}: setActiveNSView(void*) / activeNSView()
  store the pointer (atomic) for the env handler to serve.

- environment_callbacks.cpp: dispatch handler returns the active
  NSView when set; returns false (env command not supported) when
  no view is registered — the right semantic for software cores.

- libretro_adapter.h: virtual prefersHardwareRender() = false (default).

- pcsx2_libretro_adapter.h: overrides prefersHardwareRender() = true.
  EmulationView (Task 8) will read this to decide which widget to
  host.

mGBA and other software cores continue to work unchanged: their
adapters return prefersHardwareRender() = false, EmulationView
hosts LibretroVideoItem, and the env command returns false to the
core (which it ignores because it never queries it).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Create LibretroMetalItem.h

**Files:**
- Create: `${RETRONEST_ROOT}/cpp/src/ui/libretro/libretro_metal_item.h`

- [ ] **Step 1: Write the header**

Create `${RETRONEST_ROOT}/cpp/src/ui/libretro/libretro_metal_item.h` with this exact content:

```cpp
// SPDX-License-Identifier: GPL-3.0+
// pcsx2-libretro / RetroNest — Metal-backed video item.
//
// QQuickItem that exposes a layer-backed NSView for hardware-rendering
// libretro cores (PCSX2 on macOS uses Metal via GSDeviceMTL; the core
// installs its own CAMetalLayer on the NSView we provide).
//
// LibretroMetalItem is the hardware-render counterpart of
// LibretroVideoItem (software). EmulationView.qml hosts one or the
// other based on the active adapter's prefersHardwareRender().
//
// Implementation lives in libretro_metal_item.mm because it touches
// Objective-C++ (NSView, CAMetalLayer).

#pragma once

#include <QQuickItem>

class QWindow;

class LibretroMetalItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
public:
    explicit LibretroMetalItem(QQuickItem* parent = nullptr);
    ~LibretroMetalItem() override;

    LibretroMetalItem(const LibretroMetalItem&) = delete;
    LibretroMetalItem& operator=(const LibretroMetalItem&) = delete;

    /**
     * Returns the underlying NSView pointer suitable for
     * RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW. Returns nullptr if the
     * underlying window hasn't been realized yet. Pass through to
     * CoreRuntime::setActiveNSView before retro_load_game.
     */
    void* nativeView() const;

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    QWindow* m_window = nullptr;   // owns the underlying NSView via Qt's surface
};
```

- [ ] **Step 2: Verify it exists**

```sh
wc -l "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.h"
```

Expected: ~40 lines.

No commit. Task 9 commits widget + QML changes together.

---

## Task 6: Implement LibretroMetalItem.mm

**Files:**
- Create: `${RETRONEST_ROOT}/cpp/src/ui/libretro/libretro_metal_item.mm`

- [ ] **Step 1: Write the implementation**

Create `${RETRONEST_ROOT}/cpp/src/ui/libretro/libretro_metal_item.mm` with this exact content:

```objc++
// SPDX-License-Identifier: GPL-3.0+

#include "libretro_metal_item.h"

#include <QGuiApplication>
#include <QWindow>
#include <QDebug>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

LibretroMetalItem::LibretroMetalItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, false);

    // Create a child QWindow that will own a layer-backed NSView.
    // setSurfaceType(MetalSurface) makes Qt request a NSView suitable
    // for CAMetalLayer attachment.
    m_window = new QWindow();
    m_window->setSurfaceType(QSurface::MetalSurface);
    m_window->setFlags(Qt::Widget);
    m_window->create(); // realizes the NSView immediately

    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(m_window->winId());
    if (view) {
        [view setWantsLayer:YES];
        // We do NOT install a CAMetalLayer here. PCSX2's GSDeviceMTL
        // creates and installs its own CAMetalLayer on this NSView via
        // AttachSurfaceOnMainThread(). Leaving the layer empty lets that
        // installation succeed.
    } else {
        qWarning() << "[LibretroMetalItem] winId() returned null — NSView not realized";
    }
}

LibretroMetalItem::~LibretroMetalItem()
{
    // m_window is owned; Qt will tear it down. PCSX2's CAMetalLayer was
    // installed on the NSView; when the view is destroyed the layer
    // detaches with it. CoreRuntime::clearActiveNSView is called by
    // GameSession before this destructor runs (game-end path).
    delete m_window;
    m_window = nullptr;
}

void* LibretroMetalItem::nativeView() const
{
    if (!m_window) return nullptr;
    return reinterpret_cast<void*>(m_window->winId());
}

void LibretroMetalItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickItem::itemChange(change, value);

    // When the item is added to a window, parent our child QWindow to it
    // so geometry tracking works. Without this, the underlying NSView is
    // a free-floating window not visually inside RetroNest.
    if (change == QQuickItem::ItemSceneChange && value.window) {
        if (m_window) {
            m_window->setParent(value.window);
            const QRect r = mapRectToScene(boundingRect()).toRect();
            m_window->setGeometry(r);
            m_window->show();
        }
    }
}

void LibretroMetalItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (m_window && window()) {
        const QRect r = mapRectToScene(newGeometry).toRect();
        m_window->setGeometry(r);
    }
}
```

- [ ] **Step 2: Verify it exists**

```sh
wc -l "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm"
```

Expected: ~75 lines.

No commit. Task 9 commits.

---

## Task 7: Add LibretroMetalItem files to CMakeLists.txt

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/CMakeLists.txt`

- [ ] **Step 1: Find the sources/headers blocks**

Run:
```sh
grep -n "libretro_video_item" "/Users/mark/Documents/Projects/RetroNest-Project/cpp/CMakeLists.txt"
```

Expected: shows ~2 lines (one in sources, one in headers).

- [ ] **Step 2: Add the new .mm source alongside libretro_video_item.cpp**

Open `${RETRONEST_ROOT}/cpp/CMakeLists.txt`. Find the line containing `src/ui/libretro/libretro_video_item.cpp` in the main sources list. Add immediately after it:

```cmake
    src/ui/libretro/libretro_metal_item.mm
```

- [ ] **Step 3: Add the new .h header alongside libretro_video_item.h**

Find the line containing `src/ui/libretro/libretro_video_item.h` in the headers list. Add immediately after it:

```cmake
    src/ui/libretro/libretro_metal_item.h
```

- [ ] **Step 4: Set Objective-C++ flags for the .mm file (if needed)**

CMake usually auto-detects .mm files and applies the correct flags on Apple platforms. To be safe, find the `if(APPLE)` block (or add one near the bottom before `target_link_libraries`) and ensure it includes:

```cmake
if(APPLE)
    set_source_files_properties(
        src/ui/libretro/libretro_metal_item.mm
        PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    # Link AppKit + QuartzCore for the Metal-backed widget.
    target_link_libraries(RetroNest PRIVATE
        "-framework AppKit"
        "-framework QuartzCore"
        "-framework Metal"
    )
endif()
```

(If an `if(APPLE)` block already exists with similar content, merge into it rather than duplicating.)

- [ ] **Step 5: Re-configure CMake to pick up the new source**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake -B build 2>&1 | tail -5
```

Expected: configure completes successfully.

Use timeout: 600000. No commit. Task 9 commits.

---

## Task 8: Build the new widget; iterate on compile errors

**Files:** none (verification).

- [ ] **Step 1: Build the RetroNest target**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build -j 2>&1 | tee /tmp/sp3_build.log | tail -30
```

Use timeout: 1800000 (30 minutes).

- [ ] **Step 2: Common failure modes and fixes**

| Symptom | Likely cause | Fix |
|---|---|---|
| `error: use of undeclared identifier 'QML_ELEMENT'` | QML_ELEMENT requires the file to be registered as a QML module type. | Add `qt6_target_qml_sources` or `qt_add_qml_module` registration. Easiest: copy the same QML registration pattern used by `LibretroVideoItem` (look at CMakeLists for `QML_ELEMENT` lines). |
| `error: cannot import 'AppKit/AppKit.h'` | Objective-C headers not found. | The COMPILE_FLAGS in Task 7 Step 4 are insufficient; add `-x objective-c++` or use `set_source_files_properties(... LANGUAGE OBJCXX)`. |
| `error: undefined symbol _OBJC_CLASS_$_NSView` | AppKit framework not linked. | Verify Task 7 Step 4 added `-framework AppKit` to target_link_libraries. |
| `error: no member named 'create' in 'QWindow'` | Qt version mismatch. | QWindow::create() is in Qt 5+. Confirm Qt 6 is being used. |
| QML throws `LibretroMetalItem is not a type` at runtime | QML_ELEMENT registration didn't take effect. | Build target needs `qt_add_qml_module` or equivalent. Check how LibretroVideoItem is registered and mirror it. |
| Multiple Qt6 frameworks missing | Need `Qt6::Gui` private headers for native interfaces. | Add `target_link_libraries(RetroNest PRIVATE Qt6::GuiPrivate)` if available, or use the public `QWindow::winId()` which doesn't need private headers (the plan uses winId()). |

- [ ] **Step 3: Iterate**

Fix one issue at a time, rebuild, repeat. Don't iterate more than ~5 times — if compile errors aren't converging, STOP and report BLOCKED with the first non-trivial error and what attempted fixes haven't worked.

If the build succeeds, proceed to Task 9.

---

## Task 9: GameSession wires libretroBackend + register NSView; EmulationView.qml conditional

**Files:**
- Modify: `${RETRONEST_ROOT}/cpp/src/core/game_session.h` (or wherever the libretro session lives — check first)
- Modify: `${RETRONEST_ROOT}/cpp/src/core/game_session.cpp`
- Modify: `${RETRONEST_ROOT}/cpp/qml/AppUI/EmulationView.qml`

- [ ] **Step 1: Locate GameSession**

Run:
```sh
find "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src" -name "game_session.*" -o -name "GameSession*" 2>/dev/null | head -5
```

Expected: shows the .h/.cpp pair. If named differently, adapt the rest of this task.

- [ ] **Step 2: Add libretroBackend Q_PROPERTY**

In the GameSession class declaration (e.g. `game_session.h`), in the public section near other Q_PROPERTYs, add:

```cpp
    Q_PROPERTY(QString libretroBackend READ libretroBackend NOTIFY libretroBackendChanged)

    QString libretroBackend() const { return m_libretroBackend; }
```

In the signals section, add:

```cpp
    void libretroBackendChanged();
```

In the private section, add:

```cpp
    QString m_libretroBackend = QStringLiteral("software"); // "software" | "metal"
```

- [ ] **Step 3: Wire backend selection + NSView registration at launch**

In game_session.cpp, find the function that launches a libretro core (likely `launch()` or `startLibretroGame()` — search for `CoreRuntime` or `retro_load_game` references). Before it calls into CoreRuntime's load, add this logic:

```cpp
    // SP3: detect whether this adapter wants the HW render bridge.
    auto* libretro = adapter ? adapter->asLibretro() : nullptr;
    const bool hw = libretro && libretro->prefersHardwareRender();
    const QString new_backend = hw ? QStringLiteral("metal") : QStringLiteral("software");
    if (new_backend != m_libretroBackend) {
        m_libretroBackend = new_backend;
        emit libretroBackendChanged();
    }

    // NSView registration happens from QML once LibretroMetalItem is realized.
    // For now, ensure the runtime knows there's no view registered yet for
    // software backends (defensive — clears any stale value from a prior game).
    if (m_coreRuntime && !hw) {
        m_coreRuntime->setActiveNSView(nullptr);
    }
```

(If `adapter->asLibretro()` doesn't exist as a downcast helper on the EmulatorAdapter base, locate the existing way GameSession dynamic-casts to LibretroAdapter and mirror it. The skeleton-phase adapter registry's pattern should be evident.)

- [ ] **Step 4: Update EmulationView.qml with the conditional Loader**

Open `${RETRONEST_ROOT}/cpp/qml/AppUI/EmulationView.qml`. Find the existing block:

```qml
    LibretroVideoItem {
        id: video
        anchors.fill: parent
        ...
    }
```

Replace it with:

```qml
    // SP3: choose hardware (Metal) vs software (QImage) widget by the
    // active session's libretroBackend. Both items respond to the same
    // setFrame()/aspectMode/integerScale conventions, but LibretroMetalItem
    // is a passive NSView host (frames are pushed directly via CAMetalLayer
    // by the core), so its setFrame is a no-op and only LibretroVideoItem
    // listens for frameReady.
    Loader {
        id: videoLoader
        anchors.fill: parent
        sourceComponent: (root.session && root.session.libretroBackend === "metal")
            ? metalComponent
            : softwareComponent

        Component {
            id: softwareComponent
            LibretroVideoItem {
                id: video
                anchors.fill: parent
                aspectMode:   root.session ? root.session.libretroAspectMode   : "native"
                integerScale: root.session ? root.session.libretroIntegerScale  : false
            }
        }

        Component {
            id: metalComponent
            LibretroMetalItem {
                id: video
                anchors.fill: parent
                // Once realized, register our NSView with CoreRuntime via
                // the session's Q_INVOKABLE bridge. The session forwards to
                // CoreRuntime::setActiveNSView so the core's
                // RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW returns this view.
                Component.onCompleted: {
                    if (root.session)
                        root.session.registerHardwareView(video.nativeView)
                }
                Component.onDestruction: {
                    if (root.session)
                        root.session.registerHardwareView(0)
                }
            }
        }
    }
```

Also update the existing `Connections { target: root.session; function onFrameReady(frame) { video.setFrame(frame) } }` block to use the Loader's currently-instantiated item:

```qml
    Connections {
        target: root.session
        function onFrameReady(frame) {
            // Only software path uses frameReady; metal path renders directly
            // to the NSView's CAMetalLayer and never emits this signal.
            if (videoLoader.item && videoLoader.item.setFrame)
                videoLoader.item.setFrame(frame)
        }
    }
```

- [ ] **Step 5: Add the Q_INVOKABLE bridge in GameSession**

In game_session.h:
```cpp
    Q_INVOKABLE void registerHardwareView(qulonglong view_ptr);
```

In game_session.cpp:
```cpp
void GameSession::registerHardwareView(qulonglong view_ptr) {
    if (!m_coreRuntime) return;
    m_coreRuntime->setActiveNSView(reinterpret_cast<void*>(view_ptr));
}
```

(Using `qulonglong` because QML can't pass raw `void*`; `nativeView` returns `void*` which Qt will marshal as `qulonglong` via Q_INVOKABLE.)

Adjust `void* LibretroMetalItem::nativeView() const` to be a `Q_INVOKABLE qulonglong nativeView() const` so QML can read it directly:

```cpp
    Q_INVOKABLE qulonglong nativeView() const;
```

And in the .mm:
```cpp
qulonglong LibretroMetalItem::nativeView() const
{
    if (!m_window) return 0;
    return static_cast<qulonglong>(m_window->winId());
}
```

- [ ] **Step 6: Build and verify**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project/cpp"
cmake --build build -j 2>&1 | tail -15
```

Expected: builds clean. Same troubleshooting from Task 8 if compile errors appear.

Use timeout: 1800000.

- [ ] **Step 7: Commit RetroNest widget + QML changes**

Run:
```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add cpp/CMakeLists.txt \
        cpp/src/ui/libretro/libretro_metal_item.h \
        cpp/src/ui/libretro/libretro_metal_item.mm \
        cpp/src/core/game_session.h \
        cpp/src/core/game_session.cpp \
        cpp/qml/AppUI/EmulationView.qml
git status
```

Expected: only those 6 files staged.

```sh
git commit -m "$(cat <<'EOF'
libretro: LibretroMetalItem + EmulationView HW render switching

Adds the Metal-backed half of the HW render bridge:

- LibretroMetalItem (.h/.mm): QQuickItem hosting a child QWindow
  with QSurface::MetalSurface. The underlying NSView is layer-backed
  and ready to receive a CAMetalLayer; PCSX2's GSDeviceMTL installs
  its own layer via AttachSurfaceOnMainThread. nativeView() exposes
  the NSView pointer (as qulonglong for QML marshalling).

- GameSession: libretroBackend property ("software" | "metal") set
  from Pcsx2LibretroAdapter::prefersHardwareRender() at launch.
  registerHardwareView(qulonglong) bridges from QML to
  CoreRuntime::setActiveNSView so the env handler can serve the
  pointer to the libretro core.

- EmulationView.qml: Loader switches between LibretroVideoItem
  (software, mGBA) and LibretroMetalItem (HW, PCSX2) based on
  session.libretroBackend. LibretroMetalItem registers its NSView
  pointer on Component.onCompleted and clears it on destruction.

- CMakeLists.txt: new sources + headers + AppKit/QuartzCore/Metal
  framework links + Objective-C++ ARC flag for the .mm file.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Rewrite HostStubs.cpp AcquireRenderWindow + BeginPresentFrame

**Files:**
- Modify: `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp`

- [ ] **Step 1: Add the global frame-ready synchronization primitives**

Open `${PCSX2_ROOT}/pcsx2-libretro/HostStubs.cpp`. Near the top, in the `namespace Pcsx2Libretro` block (where `g_frontend` is declared), add:

```cpp

// Frame-ready synchronization: PCSX2's MTGS thread calls
// Host::BeginPresentFrame after each rendered frame is ready to display.
// We signal this condition variable, and retro_run waits on it to drive
// frame-paced execution.
std::mutex g_present_mutex;
std::condition_variable g_present_cv;
std::atomic<bool> g_present_ready{false};
```

- [ ] **Step 2: Rewrite Host::AcquireRenderWindow**

Find the existing `Host::AcquireRenderWindow` (currently returns a default Surfaceless WindowInfo). Replace its body with:

```cpp
std::optional<WindowInfo> Host::AcquireRenderWindow(bool /*recreate_window*/)
{
    if (!g_frontend.environ_cb)
    {
        FrontendLog(RETRO_LOG_ERROR, "AcquireRenderWindow: no environ_cb available");
        return std::nullopt;
    }

    void* ns_view = nullptr;
    static constexpr unsigned RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW = (1 | 0x20000);
    if (!g_frontend.environ_cb(RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW, &ns_view) || !ns_view)
    {
        FrontendLog(RETRO_LOG_ERROR,
            "AcquireRenderWindow: host did not provide an NSView (env command failed). "
            "RetroNest must instantiate LibretroMetalItem and register its NSView before retro_load_game.");
        return std::nullopt;
    }

    FrontendLog(RETRO_LOG_INFO, "AcquireRenderWindow: got NSView=%p", ns_view);

    WindowInfo wi{};
    wi.type = WindowInfo::Type::MacOS;
    wi.window_handle = ns_view;
    wi.surface_width = 640;
    wi.surface_height = 448;
    wi.surface_scale = 1.0f;
    wi.surface_refresh_rate = 60.0f;
    return wi;
}
```

(The `0x20000` literal is `RETRO_ENVIRONMENT_PRIVATE` from the libretro header; we hardcode it here to avoid a cross-repo header dependency. Documented in the comment so a future reader knows it's the matching constant.)

- [ ] **Step 3: Rewrite Host::BeginPresentFrame**

Find the existing `Host::BeginPresentFrame` (currently a no-op). Replace its body with:

```cpp
void Host::BeginPresentFrame()
{
    // PCSX2's MTGS thread calls this immediately before presenting a frame.
    // Signal retro_run so it can return one frame's worth of work.
    {
        std::scoped_lock lock(g_present_mutex);
        g_present_ready.store(true, std::memory_order_release);
    }
    g_present_cv.notify_one();
}
```

- [ ] **Step 4: Verify the file still parses**

Spot-check by re-reading the surrounding code; ensure the global mutex/cv/atomic are in scope where BeginPresentFrame references them. If they're declared inside `namespace Pcsx2Libretro` but BeginPresentFrame is in the global `Host::` namespace, you'll need to qualify them as `Pcsx2Libretro::g_present_*`.

No build yet — Task 12 builds.

---

## Task 11: Rewrite retro_run with frame-paced cv wait; bump library_version

**Files:**
- Modify: `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.cpp`
- Modify: `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h`

- [ ] **Step 1: Add extern declarations to LibretroFrontend.h**

Open `${PCSX2_ROOT}/pcsx2-libretro/LibretroFrontend.h`. Near the `FrontendState` declaration, add:

```cpp
// Frame-ready synchronization. Defined in HostStubs.cpp.
extern std::mutex g_present_mutex;
extern std::condition_variable g_present_cv;
extern std::atomic<bool> g_present_ready;
```

If `<mutex>`, `<condition_variable>`, `<atomic>` aren't already included at the top, add them.

- [ ] **Step 2: Bump library_version**

In LibretroFrontend.cpp, find `retro_get_system_info`. Change:
```cpp
info->library_version  = "vm-0.1";
```
to:
```cpp
info->library_version  = "video-0.1";
```

- [ ] **Step 3: Rewrite retro_run to block on the present cv**

Find the existing `retro_run` (currently observes state and otherwise no-ops). Replace its entire body with:

```cpp
RETRO_API void retro_run(void)
{
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    if (!emu.IsRunning()) return;

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

    // Frame-paced wait. PCSX2's MTGS thread signals g_present_cv from
    // Host::BeginPresentFrame after each rendered frame. retro_run returns
    // as soon as a frame is ready, so the host (RetroArch / RetroNest)
    // drives ~60Hz cadence by calling us once per host frame.
    //
    // 100 ms timeout protects against VM hangs / Initialize-failed paths
    // where Frame would never be signalled.
    using namespace std::chrono_literals;
    std::unique_lock<std::mutex> lock(g_present_mutex);
    g_present_cv.wait_for(lock, 100ms, [] { return g_present_ready.load(); });
    g_present_ready.store(false, std::memory_order_release);
}
```

- [ ] **Step 4: Verify the file still parses**

Spot-check the include block and namespace qualifications. `g_present_mutex` and family are in the `Pcsx2Libretro` namespace (declared in LibretroFrontend.h); since `LibretroFrontend.cpp` does `using namespace Pcsx2Libretro;` near the top (from SP1), bare names should resolve.

If they're in the global namespace instead, qualify them.

No build yet — Task 12 builds.

---

## Task 12: Build the pcsx2_libretro dylib; iterate on errors; commit

**Files:** none (verification + commit).

- [ ] **Step 1: Build the libretro core**

Run:
```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
cmake --build build --target pcsx2_libretro -j 2>&1 | tee /tmp/sp3_pcsx2_build.log | tail -40
```

Use timeout: 1800000.

- [ ] **Step 2: Common failures**

| Symptom | Likely cause | Fix |
|---|---|---|
| `error: 'g_present_mutex' undeclared` | Globals declared in Pcsx2Libretro namespace but referenced unqualified in HostStubs.cpp's Host:: function (Host:: is global namespace). | Either qualify as `Pcsx2Libretro::g_present_mutex` in HostStubs.cpp's BeginPresentFrame, or move the globals to a translation-unit-local anonymous namespace in HostStubs.cpp and re-declare extern in LibretroFrontend.h to match. |
| Linker error on Type::MacOS | WindowInfo enum value name differs. | Verify by grepping `common/WindowInfo.h` — names are `Surfaceless`, `Win32`, `X11`, `Wayland`, `MacOS`. Adjust if a rename happened. |
| Duplicate g_present_mutex definition | Header included from two TUs, both with definition. | Move the definition into HostStubs.cpp only; LibretroFrontend.h has only the extern declarations. |

- [ ] **Step 3: Verify dylib exports**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
nm -gU build/pcsx2-libretro/pcsx2_libretro.dylib | grep -E "_retro_(init|deinit|run|get_system_info|load_game|api_version)$"
```

Expected: same 6 symbols, all `T`. No regressions.

- [ ] **Step 4: Commit pcsx2-libretro changes**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
git status
```

Expected: HostStubs.cpp + LibretroFrontend.cpp + LibretroFrontend.h modified.

```sh
git add pcsx2-libretro/HostStubs.cpp \
        pcsx2-libretro/LibretroFrontend.cpp \
        pcsx2-libretro/LibretroFrontend.h
git commit -m "$(cat <<'EOF'
libretro: HW render — query host NSView, frame-paced retro_run

Consumes the host-side HW render bridge from RetroNest SP3:

- HostStubs.cpp: Host::AcquireRenderWindow queries
  RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW (0x20001) via the libretro
  env callback, returns a Type::MacOS WindowInfo populated with the
  NSView pointer. PCSX2's GSDeviceMTL installs its CAMetalLayer on
  that NSView via AttachSurfaceOnMainThread, completing the
  Pattern B bridge.

  If the host returns false (e.g. software-only host like the
  test_loader), AcquireRenderWindow returns nullopt — same clean
  init-failure path SP2 already handles.

- HostStubs.cpp: Host::BeginPresentFrame, previously a no-op,
  now signals g_present_cv every time the MTGS thread is about
  to present a frame. Provides per-frame heartbeat to retro_run.

- LibretroFrontend.cpp: retro_run blocks on g_present_cv with a
  100ms timeout. Returns one frame's worth of work per call; the
  host drives ~60Hz cadence. Timeout protects against VM-hung
  or post-shutdown paths. Also keeps the SP2 VM RUNNING one-shot
  log.

library_version bumped to "video-0.1".

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 13: End-to-end verification — boot screen visible

**Files:** none (verification + user interaction required).

- [ ] **Step 1: Copy the new dylib to RetroNest's cores directory**

```sh
cp "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib" \
   "/Users/mark/Documents/RetroNest/emulators/libretro/cores/"
ls -la "/Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib"
```

Expected: fresh dylib in place.

- [ ] **Step 2: Stop any running RetroNest, relaunch with log capture**

```sh
pkill -x RetroNest 2>/dev/null; sleep 1
rm -f /tmp/retronest-sp3-test.log
"/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest" > /tmp/retronest-sp3-test.log 2>&1 &
disown
sleep 3
pgrep -lf RetroNest | head -2
```

- [ ] **Step 3: REQUIRES USER — launch the game**

Ask the user to:
- Navigate to Ratchet & Clank - Going Commando (the game DB still has its `emulator_id = pcsx2-libretro` from prior testing).
- Launch it.
- Wait ~15 seconds.
- Observe whether the BIOS boot logo (or game title screen) appears in the main window.
- Tell us what they see.

- [ ] **Step 4: Tail the log + diagnose**

```sh
tail -80 /tmp/retronest-sp3-test.log
```

Expected log lines IF the bridge works:
```
[core] [pcsx2_libretro] retro_init — PCSX2 libretro skeleton initialised
[core] [pcsx2_libretro] retro_load_game: ...
[core] [pcsx2_libretro] Found PS2 BIOS: ...
[core] [pcsx2_libretro] Settings::InitializeDefaults complete
[core] [pcsx2_libretro] EmuThread: calling CPUThreadInitialize
[core] [pcsx2_libretro] EmuThread: CPUThreadInitialize succeeded
[core] [pcsx2_libretro] EmuThread: calling VMManager::Initialize
[core] [pcsx2_libretro] AcquireRenderWindow: got NSView=0x...
[core] [pcsx2_libretro] VMManager::Initialize succeeded; entering Execute
[core] [pcsx2_libretro] VM RUNNING — title=... crc=0x...
```

Common failures and fixes:

| Symptom | Likely cause | Fix |
|---|---|---|
| Log shows `AcquireRenderWindow: host did not provide an NSView` | LibretroMetalItem.onCompleted didn't fire OR registerHardwareView never received the pointer. | Check EmulationView.qml Loader is actually instantiating metalComponent (not softwareComponent) — confirm session.libretroBackend === "metal" at launch time. Add a `console.log` in onCompleted to verify. |
| Log shows AcquireRenderWindow got a non-null NSView, but Initialize still fails | The NSView isn't realized / has zero size / wrong type. | Verify Task 9 Step 4 (geometry change handler) is setting m_window->setGeometry properly. Verify the QWindow was created with MetalSurface. |
| RetroNest crashes on launch with EXC_BAD_ACCESS | The QWindow was destroyed before PCSX2 finished using its NSView. | Check that LibretroMetalItem outlives the EmuThread. Component.onDestruction in QML should call registerHardwareView(0) BEFORE the underlying widget is destroyed. |
| Game launches but the rendered output is invisible | CAMetalLayer attached but the QQuickItem is not laid out correctly. | Use macOS Inspector or `defaults write` debugging; verify the QWindow has non-zero size. |
| Game's audio crackles or runs at wrong speed | Frame pacing is off (no audio yet, but retro_run timing affects subtle behaviour). | This is SP4's problem (audio). Ignore for SP3. |

Iterate up to 5 times. STOP and report BLOCKED if not converging.

---

## Task 14: End-to-end verification — clean exit

**Files:** none (verification + user interaction).

- [ ] **Step 1: While the game is running**

Ask the user to press Cmd+Shift+Escape (the existing RetroNest global hotkey for in-game menu).

The in-game menu appears.

- [ ] **Step 2: User selects Exit Game**

The user navigates the in-game menu to find an "Exit Game" or "Quit" action (depends on the active theme — should mirror SP1 / SP2 behavior).

- [ ] **Step 3: Verify clean teardown in the log**

```sh
tail -30 /tmp/retronest-sp3-test.log
```

Expected:
```
[core] [pcsx2_libretro] retro_unload_game: requesting VM shutdown
[core] [pcsx2_libretro] VMManager::Execute returned; shutting down VM
[core] [pcsx2_libretro] EmuThread: clean exit
[core] [pcsx2_libretro] retro_unload_game: emu thread joined cleanly
```

And RetroNest's UI: returns to the game list view; no crashes, no zombie windows.

If retro_unload_game hangs (no "clean exit" log line within 10 seconds), the MTGS thread is stuck waiting for a present that never came. STOP and report — may need to add a wake-up in Host::BeginPresentFrame to unblock retro_run waiters during shutdown.

---

## Task 15: Sanity verification — mGBA still works

**Files:** none (verification + user interaction).

- [ ] **Step 1: Launch mGBA's existing game**

Ask the user to launch any GBA game from RetroNest's library (whichever mGBA-emulated game they normally test). mGBA uses the software path: LibretroVideoItem, frameReady, QImage.

- [ ] **Step 2: Verify behavior unchanged**

mGBA should run exactly as before SP3:
- Game starts within a couple seconds.
- Video displays normally inside the same EmulationView.
- Cmd+Shift+Escape brings up the in-game menu; Exit Game returns cleanly.

If mGBA broke, the conditional Loader in EmulationView.qml or the session.libretroBackend property is misbehaving. Verify:
- `session.libretroBackend` should be `"software"` when mGBA is the active adapter.
- The Loader should instantiate LibretroVideoItem (softwareComponent).
- frameReady connection should route to videoLoader.item.setFrame.

Fix and re-verify. mGBA regression is a hard blocker.

---

## Task 16: Mark spec complete + commit verification log

**Files:**
- Modify: `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-video-bridge-design.md`

- [ ] **Step 1: Update spec status**

Open `${RETRONEST_ROOT}/docs/superpowers/specs/2026-05-11-pcsx2-libretro-video-bridge-design.md`. Change:
```
**Status:** Approved (brainstorming)
```
to:
```
**Status:** Complete (verified end-to-end — see Verification log)
```

- [ ] **Step 2: Append verification log**

At the end of the file, append:

```markdown

## Verification log

SP3 phase completed (date). All success criteria verified:

- **Test 1 (boot screen visible):** PASSED — Ratchet & Clank launched, BIOS boot logo / title screen visible in RetroNest's main window within ~10s. Log confirms AcquireRenderWindow returned NSView pointer, VMManager::Initialize succeeded, VM RUNNING reported with correct CRC.
- **Test 2 (clean exit):** PASSED — Cmd+Shift+Escape → Exit Game → emu thread joins cleanly, returns to game list. No leaks, no zombie windows.
- **Test 3 (mGBA unchanged):** PASSED — mGBA software-path libretro game launches and plays exactly as before SP3.

### Observations / follow-ups uncovered during implementation

(To be filled in at execution time.)

### Fork state at completion

Commits added during SP3:
- (filled in at execution time)
```

- [ ] **Step 3: Commit**

```sh
cd "/Users/mark/Documents/Projects/RetroNest-Project"
git add docs/superpowers/specs/2026-05-11-pcsx2-libretro-video-bridge-design.md
git commit -m "$(cat <<'EOF'
docs(specs): mark SP3 complete — HW render bridge verified end-to-end

PCSX2 renders the BIOS boot logo and game title screen into a
Metal-backed widget inside RetroNest's main window. Clean exit
returns to the game list. mGBA software path unaffected.

Next: SP4 (audio output) wires SPU2 → retro_audio_sample_batch_t.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Plan self-review (post-write)

**Spec coverage:**

| Spec requirement | Implemented in |
|---|---|
| RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW constant + handler | Tasks 1, 3 |
| CoreRuntime::setActiveNSView / activeNSView | Task 2 |
| Pcsx2LibretroAdapter::prefersHardwareRender() = true | Task 4 |
| LibretroMetalItem.{h,mm} | Tasks 5, 6 |
| CMakeLists registration + framework links | Task 7 |
| GameSession libretroBackend property + registerHardwareView | Task 9 |
| EmulationView.qml conditional Loader | Task 9 |
| Host::AcquireRenderWindow consumes env command | Task 10 |
| Host::BeginPresentFrame signals cv | Task 10 |
| retro_run blocks on cv with timeout | Task 11 |
| library_version bumped | Task 11 |
| End-to-end Test 1 (boot screen visible) | Task 13 |
| End-to-end Test 2 (clean exit) | Task 14 |
| End-to-end Test 3 (mGBA unchanged) | Task 15 |
| Spec status update | Task 16 |

All spec requirements covered.

**Placeholder scan:** Tasks 14/15 verification rely on "the existing theme's Exit Game action" and "whichever mGBA-emulated game the user normally tests" — these are intentionally user-context-dependent, not unfilled blanks.

The Verification log section in Task 16 has `(filled in at execution time)` placeholders for SHAs and follow-up notes — those are template directives that get filled in when the task runs, not unfilled spec content.

No other TBDs / TODOs / FIXMEs.

**Type / name consistency:**
- `RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW` — used in Tasks 1, 3, 10 with same value `(1 | 0x20000)` = `0x20001`.
- `setActiveNSView` / `activeNSView` — consistently used in Tasks 2, 3, 9.
- `prefersHardwareRender` — consistently used in Tasks 4, 9.
- `libretroBackend` property values `"software"` / `"metal"` — consistently used in Tasks 9 (game_session + EmulationView).
- `g_present_mutex` / `g_present_cv` / `g_present_ready` — declared in Task 10 (Pcsx2Libretro namespace in HostStubs.cpp), referenced in Tasks 10, 11 (same names).
- `nativeView()` returns `qulonglong` (revised in Task 9 Step 5) — header signature change confirmed before the QML callsite.

**Bite-size check:** Largest task is 9 (game_session + QML + Q_INVOKABLE bridge) with 7 sub-steps. Each step is one logical change. Total 16 tasks is proportional to the cross-repo scope (both RetroNest and pcsx2-libretro fork modified).

**Cross-repo coordination:** Tasks 1-9 are RetroNest-side. Tasks 10-12 are pcsx2-libretro-side. The two commits land in independent repos, but Task 13 onwards requires both to be deployed (dylib copied, RetroNest rebuilt) — the verification stage naturally ties them together.
