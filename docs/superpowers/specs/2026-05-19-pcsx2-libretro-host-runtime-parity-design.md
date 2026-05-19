# pcsx2-libretro Host runtime parity

**Date:** 2026-05-19
**Sub-project:** Replace remaining `Host::` hardcodes/stubs in `HostStubs.cpp` with real runtime data, mirroring what standalone PCSX2 sees.
**Status:** Design approved, ready for implementation plan
**Repo scope:** `pcsx2-libretro` only (local-only fork — no push). Single commit.

## Problem

After the 16:9-letterbox-parity sub-project ([[session-handoff-16-9-letterbox-parity-shipped]]) the libretro core's video output is pixel-equivalent to standalone PCSX2 because `Host::AcquireRenderWindow` now reports real NSView dimensions instead of hardcoded 640×448. Three smaller `Host::` stubs still feed PCSX2 fake or stale data; each is the same shape as the NSView fix (replace a hardcode with a real query) and each closes a distinct standalone-parity gap.

| Stub today | Real behavior in standalone | User-visible impact today |
| --- | --- | --- |
| `wi.surface_refresh_rate = 60.0f` (`HostStubs.cpp:296` area) | Reads `NSScreen.maximumFramesPerSecond` | PCSX2 throttles to 60Hz reference regardless of screen — slight pacing drift on 120Hz / 144Hz displays |
| `Host::RequestResizeHostDisplay` is empty (`HostStubs.cpp:315`) | Resizes the NSWindow then forwards new dims to `GSResizeDisplayWindow` | PCSX2's `m_window_info` goes stale on dock/undock or screen change; aspect math uses pre-event dims until the game is reloaded |
| `Host::IsFullscreen` returns `false` (`HostStubs.cpp:395`) | Returns actual NSWindow fullscreen state | Correctness nit only — RetroNest is borderless-fullscreen always (`AppWindow.qml:84-92`), so the lie costs no functionality but is wrong on its face |

## Goal

Each stub returns truth derived from the same NSView pointer the prior sub-project already passes through `AcquireRenderWindow`. Net effect: PCSX2's view of the host environment matches what standalone would see, in every case we currently fake.

## Non-goals

- **Frame pacing inside libretro** — the libretro contract owns frame cadence (frontend drives `retro_run` once per host frame). Reporting accurate `surface_refresh_rate` lets PCSX2's internal throttle math reference the right number, but the present loop itself remains libretro-driven. That architectural gap is not addressed here.
- **Other stubs** (`GetTopLevelWindowInfo`, `SetFullscreen`, `OnInputDeviceConnected/Disconnected`, exit-related stubs) — only consumed by PCSX2-Qt UI / hotkey paths we don't expose. Leaving as-is.
- **RetroNest-Project changes** — none. All three fixes are in the pcsx2-libretro shim.

## Anchors

| Concern | Location |
| --- | --- |
| Existing NSView query helper | `pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.{h,mm}` (added in `afd2179a2`) |
| WindowInfo hardcode site (refresh_rate) | `HostStubs.cpp:296` (the line `wi.surface_refresh_rate = 60.0f;` inside `AcquireRenderWindow`) |
| Resize-host stub | `HostStubs.cpp:315` (`void Host::RequestResizeHostDisplay(s32 width, s32 height) {}`) |
| Fullscreen stub | `HostStubs.cpp:395` (`bool Host::IsFullscreen() { return false; }`) |
| PCSX2 caller of RequestResizeHostDisplay | `pcsx2/VMManager.cpp:957` |
| GSResizeDisplayWindow signature | `pcsx2/GS/GS.h:86` — `void GSResizeDisplayWindow(u32 width, u32 height, float scale)` |
| RetroNest borderless-fullscreen sizing | `RetroNest-Project/cpp/qml/AppUI/AppWindow.qml:84-92` |

## Components

### 1. Extend `MacNSViewMetrics::NSViewMetrics` with `refresh_rate`

Add one field; keep the struct as the single source of truth for "what the host display looks like."

```cpp
// MacNSViewMetrics.h
namespace Pcsx2Libretro::Mac
{
    struct NSViewMetrics
    {
        uint32_t surface_width  = 0;
        uint32_t surface_height = 0;
        float    surface_scale  = 1.0f;
        // Reported screen refresh rate in Hz. Falls back to 60.0f when
        // NSScreen.maximumFramesPerSecond is unavailable (pre-macOS 12 or
        // headless / unbacked view).
        float    refresh_rate   = 60.0f;
    };

    NSViewMetrics Query(void* ns_view);
}
```

### 2. Implement refresh-rate query in `MacNSViewMetrics.mm`

After reading `bounds` and `backingScaleFactor`, also read the host screen's refresh rate:

```objc
// MacNSViewMetrics.mm
NSViewMetrics Query(void* ns_view)
{
    NSViewMetrics out{};
    if (!ns_view) return out;

    NSView* view = (__bridge NSView*)ns_view;
    NSRect bounds = [view bounds];
    NSWindow* host_window = [view window];

    // Backing scale (DPR).
    CGFloat scale = 1.0;
    NSScreen* screen = nil;
    if (host_window != nil) {
        scale = [host_window backingScaleFactor];
        screen = [host_window screen];
    }
    if (screen == nil) screen = [NSScreen mainScreen];
    if (screen != nil && scale == 1.0)
        scale = [screen backingScaleFactor];

    // Refresh rate. NSScreen.maximumFramesPerSecond returns the screen's
    // refresh rate as an integer (Hz); available since macOS 12. We're
    // already on a recent SDK so the property is always present; gate on
    // respondsToSelector just to be defensive against unusual environments.
    float refresh = 60.0f;
    if (screen != nil && [screen respondsToSelector:@selector(maximumFramesPerSecond)]) {
        NSInteger fps = [screen maximumFramesPerSecond];
        if (fps > 0) refresh = static_cast<float>(fps);
    }

    out.surface_width  = static_cast<uint32_t>(bounds.size.width  * scale);
    out.surface_height = static_cast<uint32_t>(bounds.size.height * scale);
    out.surface_scale  = static_cast<float>(scale);
    out.refresh_rate   = refresh;
    return out;
}
```

### 3. Use real refresh rate in `AcquireRenderWindow`

`HostStubs.cpp` already calls `Mac::Query(ns_view)` and reads `surface_width/height/scale`. Add the refresh-rate read; drop the hardcoded `60.0f`:

```cpp
// HostStubs.cpp inside Host::AcquireRenderWindow:
const auto metrics = Pcsx2Libretro::Mac::Query(ns_view);
const u32 sw = (metrics.surface_width  > 0) ? metrics.surface_width  : 640;
const u32 sh = (metrics.surface_height > 0) ? metrics.surface_height : 448;
const float ss = (metrics.surface_scale > 0.0f) ? metrics.surface_scale : 1.0f;
const float rr = (metrics.refresh_rate > 0.0f) ? metrics.refresh_rate : 60.0f;
Pcsx2Libretro::FrontendLog(RETRO_LOG_INFO,
    "AcquireRenderWindow: surface=%ux%u scale=%.2f refresh=%.1fHz", sw, sh, ss, rr);

WindowInfo wi{};
wi.type = WindowInfo::Type::MacOS;
wi.window_handle = ns_view;
wi.surface_width = sw;
wi.surface_height = sh;
wi.surface_scale = ss;
wi.surface_refresh_rate = rr;   // was hardcoded 60.0f
return wi;
```

### 4. Store the acquired NSView pointer + implement `RequestResizeHostDisplay`

Add a file-static pointer that tracks the NSView between `AcquireRenderWindow` and `ReleaseRenderWindow`. `RequestResizeHostDisplay` re-queries the metrics and forwards them to `GSResizeDisplayWindow`, refreshing PCSX2's `m_window_info`.

Why ignore the `width`/`height` params from PCSX2's call? Standalone uses them to resize the NSWindow; we can't resize a libretro frontend's window. The most faithful "we received your request" response is to re-read whatever the actual surface looks like *now* and push that to PCSX2, so its aspect math has fresh inputs. In a steady-state session (no screen change) this is a no-op; in dock/undock or screen-change scenarios it refreshes stale state.

```cpp
// HostStubs.cpp file-static near the other libretro-shim globals:
namespace { void* g_acquired_ns_view = nullptr; }

// Inside Host::AcquireRenderWindow, after the existing wi assembly:
g_acquired_ns_view = ns_view;

// Inside Host::ReleaseRenderWindow (currently empty):
void Host::ReleaseRenderWindow() { g_acquired_ns_view = nullptr; }

// Replace the empty RequestResizeHostDisplay:
void Host::RequestResizeHostDisplay(s32 /*width*/, s32 /*height*/)
{
    // PCSX2 calls this when its internal render resolution changes (e.g.
    // NTSC/PAL switch mid-game). Standalone would resize its NSWindow and
    // then push the new dims to GSResizeDisplayWindow; we can't resize the
    // libretro frontend's window, so instead we re-query the NSView's
    // *current* dimensions and forward those — refreshing PCSX2's
    // m_window_info if anything changed (dock/undock, screen change).
    // In steady state this is a no-op; in event-driven cases it eliminates
    // the stale-state window where aspect math would use pre-event values.
    if (!g_acquired_ns_view) return;
    const auto m = Pcsx2Libretro::Mac::Query(g_acquired_ns_view);
    if (m.surface_width == 0 || m.surface_height == 0) return;
    GSResizeDisplayWindow(m.surface_width, m.surface_height, m.surface_scale);
}
```

(Need `#include "pcsx2/GS.h"` for `GSResizeDisplayWindow`. Already included transitively per the existing GS.h reference; verify during implementation.)

### 5. `IsFullscreen` returns `true`

RetroNest is always borderless-fullscreen. One-line change:

```cpp
bool Host::IsFullscreen() { return true; }
```

## Data flow

```
[App start] → Host::AcquireRenderWindow
  ↓ Mac::Query(ns_view)
  surface_width/height/scale/refresh_rate populated from NSView + NSScreen
  ↓
  wi → PCSX2 (via WindowInfo return)
  ↓
  g_acquired_ns_view stashed

[Game-time, mid-session NTSC/PAL switch] → PCSX2 calls Host::RequestResizeHostDisplay(w, h)
  ↓ Mac::Query(g_acquired_ns_view) re-reads current NSView state
  ↓
  GSResizeDisplayWindow(real_w, real_h, real_scale)
  ↓
  PCSX2 GSDeviceMTL::ResizeWindow updates m_window_info → aspect math uses fresh dims

[App shutdown] → Host::ReleaseRenderWindow → g_acquired_ns_view = nullptr
```

## Error handling

- Null `ns_view` in `Mac::Query` → returns zero-width metrics → `AcquireRenderWindow` falls back to the existing 640×448/60Hz/1.0 defaults (preserves prior behavior in the degenerate case).
- Null `g_acquired_ns_view` in `RequestResizeHostDisplay` → early return (no GS update). Safe; the call before `AcquireRenderWindow` is meaningless anyway.
- `screen == nil` in the refresh-rate query → falls back to 60Hz. Safe default.
- Pre-macOS 12 (no `maximumFramesPerSecond`) → `respondsToSelector` guard falls back to 60Hz. Safe; we don't promise multi-rate parity on legacy systems.

## Testing

This is a small set of well-bounded changes; existing unit tests stay green and we lean on smoke verification.

### Smoke (Rosetta x86_64)

1. **Refresh rate visible in log** — Launch RetroNest, load any PCSX2 title. Confirm the `AcquireRenderWindow` log line now includes `refresh=...Hz` and the value matches the user's display.
2. **16:9 parity preserved** — Same scene as `[[session-handoff-16-9-letterbox-parity-shipped]]`. No regression in letterbox; pixel-measure if any visual change is suspected.
3. **All other aspect modes preserved** — Quick run through 4:3, 10:7, Stretch, Auto. No regressions.
4. **Optional: dock/undock test** — If the user can change display modes mid-session, confirm PCSX2's render geometry refreshes after `RequestResizeHostDisplay` fires. Hard to trigger reliably; OK to defer.

## Constraints

- Single-commit sub-project. All three fixes touch the same file (`HostStubs.cpp`) plus the helper (`MacNSViewMetrics.{h,mm}`).
- No `RetroNest-Project` changes. No push (pcsx2-libretro is local-only).
- Don't modify any other `Host::` stub. Don't expand scope to other Host:: realism (out of scope per design).

## Time estimate

Half-day:

- Implementation: 1h (small surface, all known sites)
- Build + deploy via the established `arch -x86_64 cmake --build .../pcsx2-libretro/build-x86_64 --target pcsx2_libretro` recipe, then `scripts/lipo-merge-dylib.sh`: 15min
- Smoke: 20min
- Memory + commit: 15min

## Related memories

- `[[session-handoff-16-9-letterbox-parity-shipped]]` — the prior sub-project. Established `MacNSViewMetrics` and the AcquireRenderWindow real-data pattern this work extends.
- `[[session-handoff-stretch-flow-through-shipped]]` — preceding work on the libretro aspect plumbing.
- `[[session-handoff-aspect-ratio-shipped]]` — original aspect-ratio sub-project; established the build-cadence note that pcsx2-libretro needs its own cmake target (`arch -x86_64 cmake --build .../pcsx2-libretro/build-x86_64 --target pcsx2_libretro -j 4`) followed by `scripts/lipo-merge-dylib.sh` to deploy.
- `[[project-pcsx2-libretro-port]]` — port tracker.
