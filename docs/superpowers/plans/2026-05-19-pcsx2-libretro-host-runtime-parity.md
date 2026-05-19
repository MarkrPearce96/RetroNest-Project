# pcsx2-libretro Host runtime parity — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace three remaining hardcodes/stubs in `pcsx2-libretro/HostStubs.cpp` with real runtime data — refresh rate from `NSScreen`, `RequestResizeHostDisplay` that forwards re-queried NSView metrics to `GSResizeDisplayWindow`, and `IsFullscreen` returning the honest `true`.

**Architecture:** Extend the existing `MacNSViewMetrics` helper (from the prior 16:9-letterbox-parity ship) with a refresh-rate field. Cache the acquired NSView pointer in `HostStubs.cpp` so `RequestResizeHostDisplay` can re-query it. Two helper files, one stubs file. Single commit, pcsx2-libretro only (local-only fork — no push).

**Tech Stack:** C++20 + Objective-C++ (Apple SDK: `NSScreen`, `NSWindow`, `NSView`). No new infrastructure or dependencies.

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-19-pcsx2-libretro-host-runtime-parity-design.md` (commit `9f74b99`).

**Working repository:** `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `main`).

---

## File Structure

| File | Role | Action |
| --- | --- | --- |
| `pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.h` | `NSViewMetrics` struct definition | **Modify** — add `refresh_rate` field |
| `pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.mm` | Helper impl reading NSView/NSWindow/NSScreen | **Modify** — read `maximumFramesPerSecond` |
| `pcsx2-libretro/pcsx2-libretro/HostStubs.cpp` | Host:: hooks the libretro shim provides | **Modify** in 4 places: refresh rate wiring (1), `g_acquired_ns_view` declaration (2), `AcquireRenderWindow`/`ReleaseRenderWindow` set+clear (3), `RequestResizeHostDisplay` body (4), `IsFullscreen` body (5) |

One commit at the end.

---

## Task 1: Add `refresh_rate` field to `NSViewMetrics`

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.h:21-27` (struct body)

- [ ] **Step 1: Read the current struct to anchor the edit**

```bash
sed -n '18,32p' /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.h
```

Expected: shows the namespace + struct with the three fields (`surface_width`, `surface_height`, `surface_scale`).

- [ ] **Step 2: Add the `refresh_rate` field**

Use Edit with `old_string`:
```cpp
    struct NSViewMetrics
    {
        // surface_width/height in physical pixels (point size × backing scale),
        // matching what GSDeviceMTL stores in CAMetalLayer.drawableSize.
        uint32_t surface_width  = 0;
        uint32_t surface_height = 0;
        // Backing scale factor (1.0 on non-Retina, 2.0 on standard Retina).
        float    surface_scale  = 1.0f;
    };
```

And `new_string`:
```cpp
    struct NSViewMetrics
    {
        // surface_width/height in physical pixels (point size × backing scale),
        // matching what GSDeviceMTL stores in CAMetalLayer.drawableSize.
        uint32_t surface_width  = 0;
        uint32_t surface_height = 0;
        // Backing scale factor (1.0 on non-Retina, 2.0 on standard Retina).
        float    surface_scale  = 1.0f;
        // Screen refresh rate in Hz (NSScreen.maximumFramesPerSecond).
        // Falls back to 60.0f when the screen / API is unavailable.
        float    refresh_rate   = 60.0f;
    };
```

- [ ] **Step 3: Verify**

```bash
grep -n "refresh_rate" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.h
```

Expected: one match in the struct body.

---

## Task 2: Query `maximumFramesPerSecond` in `MacNSViewMetrics.mm`

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.mm` (the `Query` body)

The current implementation already reads `NSWindow.backingScaleFactor`. We extend it to also read the hosting `NSScreen`'s refresh rate. The NSScreen comes from `[host_window screen]` (preferred) or `[NSScreen mainScreen]` (fallback).

- [ ] **Step 1: Read the current `Query` body**

```bash
sed -n '10,35p' /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.mm
```

Expected: shows the full `Query` function with `bounds`, `host_window`, `scale` reads.

- [ ] **Step 2: Replace the function body to add screen-refresh-rate query**

Use Edit with `old_string`:
```cpp
NSViewMetrics Query(void* ns_view)
{
    NSViewMetrics out{};
    if (!ns_view) return out;

    NSView* view = (__bridge NSView*)ns_view;
    NSRect bounds = [view bounds];
    NSWindow* host_window = [view window];

    // backingScaleFactor is 1.0 on non-Retina; 2.0 on standard Retina.
    // Fall back to the main screen when the view isn't yet hosted in a
    // window (host_window can be nil during early Acquire callbacks).
    CGFloat scale = 1.0;
    if (host_window != nil)
        scale = [host_window backingScaleFactor];
    else if (NSScreen* screen = [NSScreen mainScreen])
        scale = [screen backingScaleFactor];

    out.surface_width  = static_cast<uint32_t>(bounds.size.width  * scale);
    out.surface_height = static_cast<uint32_t>(bounds.size.height * scale);
    out.surface_scale  = static_cast<float>(scale);
    return out;
}
```

And `new_string`:
```cpp
NSViewMetrics Query(void* ns_view)
{
    NSViewMetrics out{};
    if (!ns_view) return out;

    NSView* view = (__bridge NSView*)ns_view;
    NSRect bounds = [view bounds];
    NSWindow* host_window = [view window];

    // Pick the screen the view is currently displayed on, falling back to
    // the main screen when the view isn't yet hosted in a window
    // (host_window can be nil during early Acquire callbacks).
    NSScreen* screen = (host_window != nil) ? [host_window screen] : nil;
    if (screen == nil) screen = [NSScreen mainScreen];

    // backingScaleFactor is 1.0 on non-Retina; 2.0 on standard Retina.
    // Prefer the hosting window's value (matches the layer's actual
    // rendering target); only consult the screen if the window is unbacked.
    CGFloat scale = 1.0;
    if (host_window != nil)
        scale = [host_window backingScaleFactor];
    else if (screen != nil)
        scale = [screen backingScaleFactor];

    // NSScreen.maximumFramesPerSecond is macOS 12+. Guard with
    // respondsToSelector to remain safe on older systems (degenerate
    // fallback to the struct's 60.0f default).
    float refresh = 60.0f;
    if (screen != nil && [screen respondsToSelector:@selector(maximumFramesPerSecond)])
    {
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

- [ ] **Step 3: Verify**

```bash
grep -n "maximumFramesPerSecond\|refresh_rate" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/MacNSViewMetrics.mm
```

Expected: 2-3 matches (selector probe, the integer-to-float read, the final assignment).

---

## Task 3: Use the real refresh rate in `Host::AcquireRenderWindow`

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/HostStubs.cpp:283-297` (the WindowInfo assembly inside `AcquireRenderWindow`)

The existing code reads `surface_width/height/scale` from `Mac::Query(ns_view)` then hardcodes `wi.surface_refresh_rate = 60.0f;`. Replace that with the new field, with the same `> 0` fallback shape used for the other reads.

- [ ] **Step 1: Replace the metrics-read + wi-assembly block**

Use Edit with `old_string`:
```cpp
    const auto metrics = Pcsx2Libretro::Mac::Query(ns_view);
    const u32 sw = (metrics.surface_width  > 0) ? metrics.surface_width  : 640;
    const u32 sh = (metrics.surface_height > 0) ? metrics.surface_height : 448;
    const float ss = (metrics.surface_scale > 0.0f) ? metrics.surface_scale : 1.0f;
    Pcsx2Libretro::FrontendLog(RETRO_LOG_INFO,
        "AcquireRenderWindow: surface=%ux%u scale=%.2f", sw, sh, ss);

    WindowInfo wi{};
    wi.type = WindowInfo::Type::MacOS;
    wi.window_handle = ns_view;
    wi.surface_width = sw;
    wi.surface_height = sh;
    wi.surface_scale = ss;
    wi.surface_refresh_rate = 60.0f;
    return wi;
}
```

And `new_string`:
```cpp
    const auto metrics = Pcsx2Libretro::Mac::Query(ns_view);
    const u32 sw = (metrics.surface_width  > 0) ? metrics.surface_width  : 640;
    const u32 sh = (metrics.surface_height > 0) ? metrics.surface_height : 448;
    const float ss = (metrics.surface_scale > 0.0f) ? metrics.surface_scale : 1.0f;
    const float rr = (metrics.refresh_rate  > 0.0f) ? metrics.refresh_rate  : 60.0f;
    Pcsx2Libretro::FrontendLog(RETRO_LOG_INFO,
        "AcquireRenderWindow: surface=%ux%u scale=%.2f refresh=%.1fHz", sw, sh, ss, rr);

    // Cache for RequestResizeHostDisplay (defined below). Cleared in
    // ReleaseRenderWindow so the stale pointer never outlives the view.
    g_acquired_ns_view = ns_view;

    WindowInfo wi{};
    wi.type = WindowInfo::Type::MacOS;
    wi.window_handle = ns_view;
    wi.surface_width = sw;
    wi.surface_height = sh;
    wi.surface_scale = ss;
    wi.surface_refresh_rate = rr;
    return wi;
}
```

- [ ] **Step 2: Verify**

```bash
grep -n "wi.surface_refresh_rate\|g_acquired_ns_view" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/HostStubs.cpp
```

Expected: one `wi.surface_refresh_rate = rr;` match plus one `g_acquired_ns_view = ns_view;` in AcquireRenderWindow. The `g_acquired_ns_view` declaration comes in Task 4.

---

## Task 4: Add the `g_acquired_ns_view` file-static and `pcsx2/GS.h` include

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/HostStubs.cpp` near the top (after the existing includes; near the existing `Pcsx2Libretro::` globals)

The static lives in an anonymous namespace at file scope so it's translation-unit-local. We also need `#include "pcsx2/GS.h"` for the `GSResizeDisplayWindow` declaration (used in Task 5).

- [ ] **Step 1: Confirm `pcsx2/GS.h` isn't already included**

```bash
grep -n '#include "pcsx2/GS.h"' /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/HostStubs.cpp
```

If it returns a match, skip adding the include (use the existing one). If it returns nothing, proceed to Step 2.

- [ ] **Step 2: Add the `pcsx2/GS.h` include after the existing `pcsx2/Host.h`**

Use Edit with `old_string`:
```cpp
#include "pcsx2/Host.h"
```

And `new_string`:
```cpp
#include "pcsx2/GS.h"   // GSResizeDisplayWindow for RequestResizeHostDisplay
#include "pcsx2/Host.h"
```

(If Step 1 found `pcsx2/GS.h` is already included, do not perform Step 2.)

- [ ] **Step 3: Add the anonymous-namespace static near the existing libretro-shim globals**

Find the spot using:
```bash
grep -n "^std::mutex g_present_mutex\|^namespace {" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/HostStubs.cpp | head -3
```

Expected: shows `std::mutex g_present_mutex;` at line ~58. Insert the new static block immediately before that line.

Use Edit with `old_string`:
```cpp
std::mutex g_present_mutex;
```

And `new_string`:
```cpp
// Tracks the NSView Host::AcquireRenderWindow returned in WindowInfo so
// Host::RequestResizeHostDisplay can re-query it. Set in AcquireRenderWindow,
// cleared in ReleaseRenderWindow — never read outside those two callbacks
// and RequestResizeHostDisplay.
namespace { void* g_acquired_ns_view = nullptr; }

std::mutex g_present_mutex;
```

- [ ] **Step 4: Verify**

```bash
grep -n "g_acquired_ns_view" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/HostStubs.cpp
```

Expected: 2 matches — the declaration (Task 4) and the assignment in AcquireRenderWindow (Task 3).

---

## Task 5: Clear `g_acquired_ns_view` in `ReleaseRenderWindow` + implement `RequestResizeHostDisplay`

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/HostStubs.cpp:300-317` (the empty `ReleaseRenderWindow` and `RequestResizeHostDisplay`)

- [ ] **Step 1: Fill in `ReleaseRenderWindow`**

Use Edit with `old_string`:
```cpp
void Host::ReleaseRenderWindow()
{
}
```

And `new_string`:
```cpp
void Host::ReleaseRenderWindow()
{
    // Drop the cached pointer so RequestResizeHostDisplay can't dereference
    // a NSView that's been torn down.
    g_acquired_ns_view = nullptr;
}
```

- [ ] **Step 2: Replace the empty `RequestResizeHostDisplay` body**

Use Edit with `old_string`:
```cpp
void Host::RequestResizeHostDisplay(s32 width, s32 height)
{
}
```

And `new_string`:
```cpp
void Host::RequestResizeHostDisplay(s32 /*width*/, s32 /*height*/)
{
    // PCSX2 calls this when its internal render resolution changes (e.g. an
    // NTSC/PAL switch mid-game; see VMManager.cpp:957). Standalone PCSX2
    // resizes its NSWindow to the requested dims and then calls
    // GSResizeDisplayWindow with the *actual* new window size. We can't
    // resize a libretro frontend's window, so the most faithful response is
    // to ignore the requested dims and re-read whatever the NSView is right
    // now — refreshing PCSX2's m_window_info if anything has actually
    // changed (dock/undock, screen change, DPR change). Steady-state this
    // is a no-op; event-driven it eliminates the stale-state window where
    // aspect math would still see pre-event dimensions.
    if (!g_acquired_ns_view) return;
    const auto m = Pcsx2Libretro::Mac::Query(g_acquired_ns_view);
    if (m.surface_width == 0 || m.surface_height == 0) return;
    GSResizeDisplayWindow(m.surface_width, m.surface_height, m.surface_scale);
}
```

- [ ] **Step 3: Verify**

```bash
grep -n "ReleaseRenderWindow\|RequestResizeHostDisplay\|GSResizeDisplayWindow" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/HostStubs.cpp
```

Expected: matches for `ReleaseRenderWindow` definition (with `g_acquired_ns_view = nullptr`), `RequestResizeHostDisplay` definition (with the `Mac::Query` body), and one `GSResizeDisplayWindow` call.

---

## Task 6: `Host::IsFullscreen` returns `true`

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/HostStubs.cpp:395-398`

- [ ] **Step 1: Replace the body**

Use Edit with `old_string`:
```cpp
bool Host::IsFullscreen()
{
    return false;
}
```

And `new_string`:
```cpp
bool Host::IsFullscreen()
{
    // RetroNest is borderless-fullscreen always (AppWindow.qml:84-92 sizes
    // the QQuickWindow to screen.width/height every launch). Reporting
    // false would cause PCSX2's hotkey/UI code paths to treat us as
    // windowed; true matches what the user actually sees.
    return true;
}
```

- [ ] **Step 2: Verify**

```bash
grep -n "Host::IsFullscreen" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/HostStubs.cpp
```

Expected: one match (the definition).

---

## Task 7: Build the libretro core

**Files:**
- No source changes — build verification only.

The build cadence is the same as the prior two pcsx2-libretro sub-projects. `arch -x86_64` is load-bearing.

- [ ] **Step 1: Build x86_64 slice**

```bash
arch -x86_64 /usr/local/bin/cmake --build \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64 \
  --target pcsx2_libretro -j 4 2>&1 | tail -5
```

Expected final line: `[100%] Built target pcsx2_libretro`.

If you see `unknown target CPU 'apple-m4'`, the `arch -x86_64` prefix is missing (or you ran without `arch`). Re-run with the exact command above.

If you see `Objective-C was disabled in precompiled file`, somebody removed the `SKIP_PRECOMPILE_HEADERS` for `MacNSViewMetrics.mm`. Restore it in `pcsx2-libretro/CMakeLists.txt`.

- [ ] **Step 2: Verify fresh dylib**

```bash
stat -f "%Sm  %z bytes" \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib
```

Expected: mtime within the last minute.

---

## Task 8: Lipo-merge + deploy

**Files:**
- No source changes — deploy only.

- [ ] **Step 1: Lipo-merge with the existing arm64 slice and deploy to RetroNest's cores dir**

```bash
/Users/mark/Documents/Projects/RetroNest-Project/scripts/lipo-merge-dylib.sh \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib \
  /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib 2>&1 | tail -3
```

Expected: `✓ wrote universal /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib`.

The arm64 slice will be stale (older code). The user's RetroNest runs x86_64 under Rosetta so the loader picks the fresh x86_64 slice. arm64 rebuild only matters for a ship build.

- [ ] **Step 2: Confirm deployed mtime**

```bash
stat -f "%Sm" /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

Expected: within the last minute.

---

## Task 9: User smoke test (STOP)

**Files:**
- No source changes — runtime verification.

Launch RetroNest with log capture:

```bash
rm -f /tmp/retronest-host-parity-smoke.log && \
  pkill -f RetroNest 2>/dev/null; sleep 1; \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
    >/tmp/retronest-host-parity-smoke.log 2>&1 &
```

- [ ] **Step 1: Load any PCSX2 title; confirm refresh-rate log line**

User loads a game. Then:
```bash
grep "AcquireRenderWindow:" /tmp/retronest-host-parity-smoke.log
```
Expected: a line like `AcquireRenderWindow: surface=3420x2214 scale=2.00 refresh=60.0Hz` (or 120Hz / 144Hz depending on the user's display). Specifically, the `refresh=...Hz` portion is the new addition. If it reads `60.0Hz` on a user with a 60Hz display, that's correct; on a 120Hz display, expect `120.0Hz`.

- [ ] **Step 2: Quick aspect regression — 16:9 still pixel-equivalent to standalone**

User sets `pcsx2_aspect_ratio = 16:9`, RetroNest aspectMode `native`, fullscreen. Visible result should match the `[[session-handoff-16-9-letterbox-parity-shipped]]` perfect-parity baseline (same as image #15 from that session). If anything regressed, STOP and report.

- [ ] **Step 3: Optional — RequestResizeHostDisplay sanity**

Hard to trigger in normal use (would require an NTSC/PAL switch mid-game, dock/undock, or screen change). Skip unless one of those scenarios is part of the user's normal workflow.

---

## Task 10: Commit the fix + memory closeout

**Files:**
- Commit Tasks 1-6 (no new file changes).
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/MEMORY.md`
- Create: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_host_runtime_parity_shipped.md`

- [ ] **Step 1: Confirm working tree shape**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro && git status --short
```

Expected: exactly three modified files:
```
 M pcsx2-libretro/HostStubs.cpp
 M pcsx2-libretro/MacNSViewMetrics.h
 M pcsx2-libretro/MacNSViewMetrics.mm
```
(Untracked test binaries / `__pycache__` / `tools/resources` may also show — those are intentional artifacts, do not stage.)

- [ ] **Step 2: Stage and commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro && \
  git add pcsx2-libretro/MacNSViewMetrics.h \
          pcsx2-libretro/MacNSViewMetrics.mm \
          pcsx2-libretro/HostStubs.cpp && \
  git commit -m "$(cat <<'EOF'
fix(libretro): real refresh rate + RequestResize + IsFullscreen=true

Three small follow-ups to the 16:9 letterbox parity ship — replace
remaining hardcodes/stubs in HostStubs.cpp with real runtime data.

1. WindowInfo::surface_refresh_rate now reads NSScreen.maximumFramesPerSecond
   via the existing MacNSViewMetrics helper (extended with refresh_rate).
   PCSX2's internal frame-throttle math now references the user's actual
   display rate instead of an assumed 60Hz.

2. Host::RequestResizeHostDisplay was empty. Standalone responds by
   resizing the NSWindow then forwarding the new dims to
   GSResizeDisplayWindow. We can't resize a libretro frontend's window,
   so we cache the acquired NSView pointer and re-query its metrics on
   each RequestResize call, forwarding to GSResizeDisplayWindow.
   Steady state: no-op. Event-driven (dock/undock, screen change): PCSX2's
   m_window_info refreshes to match reality instead of going stale.

3. Host::IsFullscreen now returns true. RetroNest is always
   borderless-fullscreen (AppWindow.qml:84-92 sizes the window to the
   screen rect at launch). Mostly a correctness nit; eliminates a few
   minor PCSX2 UI code paths that treated us as windowed.

Smoke-verified: AcquireRenderWindow log now reads
"surface=WxH scale=2.00 refresh=NHz"; 16:9 letterbox parity from the
prior sub-project is preserved.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 3: Verify**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro && git log -1 --stat
```

Expected: 3 files changed, roughly +50 / -10 lines.

- [ ] **Step 4: Write the session-handoff memory**

Create `session_handoff_host_runtime_parity_shipped.md` with frontmatter (`type: project`). Body content:
- One-paragraph summary: what shipped, commit SHA, smoke result (real refresh rate observed; 16:9 parity preserved).
- Note the three concrete changes and their PCSX2-side consumers.
- Document that pcsx2-libretro stays local (no push).
- Reference spec + plan paths.

- [ ] **Step 5: Update `MEMORY.md` index**

Prepend a one-line entry pointing at the new session-handoff memory. Format matches the previous entries (one-line bullet, key facts, commit SHA, cross-reference to related ships).

---

## Self-Review Checklist (performed during plan authoring)

**Spec coverage:**
- ✅ Extend NSViewMetrics with refresh_rate → Task 1
- ✅ NSScreen.maximumFramesPerSecond query → Task 2
- ✅ Wire into AcquireRenderWindow → Task 3
- ✅ Cache NSView pointer + clear in Release → Tasks 3 (set), 4 (declare), 5 (clear)
- ✅ Implement RequestResizeHostDisplay → Task 5
- ✅ Include pcsx2/GS.h for GSResizeDisplayWindow → Task 4
- ✅ IsFullscreen returns true → Task 6
- ✅ Build via arch -x86_64 + lipo-merge + deploy → Tasks 7-8
- ✅ Smoke verifies new log + 16:9 regression check → Task 9
- ✅ Commit + memory closeout → Task 10

**Placeholder scan:** no TBD / TODO / "implement later." Every step has actual code or actual commands with expected output.

**Type/identifier consistency:**
- `NSViewMetrics::refresh_rate` (Task 1) — referenced consistently in Tasks 2, 3
- `g_acquired_ns_view` (Tasks 3, 4, 5) — same identifier, same anonymous-namespace location
- `Pcsx2Libretro::Mac::Query` — already-existing function, used identically in Tasks 3, 5
- `GSResizeDisplayWindow(width, height, scale)` — matches the upstream signature at `pcsx2/GS/GS.h:86`
- `wi.surface_refresh_rate` — matches the WindowInfo field name in `common/WindowInfo.h`
- `g_acquired_ns_view` set order: Task 4 declares (before any reference), Task 3 assigns (in AcquireRenderWindow), Task 5 clears (in ReleaseRenderWindow) and reads (in RequestResizeHostDisplay). All consistent.

---

## Execution Notes

- 10 tasks. Estimated ~1.5h implementation + 30min smoke + 15min commit/memory.
- One commit at the end (Task 10). No intermediate commits — all changes are tightly coupled (the static, its set/clear, its read).
- Build cadence is the same as the prior two ships: `arch -x86_64 /usr/local/bin/cmake --build .../pcsx2-libretro/build-x86_64 --target pcsx2_libretro` then `scripts/lipo-merge-dylib.sh`. RetroNest's `--target RetroNest` cmake does NOT trigger pcsx2_libretro rebuild — established in `[[session-handoff-aspect-ratio-shipped]]`.
- One pause point: Task 9 (user smoke). Tasks 1-8 can run as a single implementer dispatch + controller-driven build/deploy.
- No RetroNest-Project changes. No push to origin (pcsx2-libretro is local-only).
