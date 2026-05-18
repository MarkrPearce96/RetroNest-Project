# RetroNest libretro Stretch flow-through

**Date:** 2026-05-19
**Sub-project:** Stretch semantics end-to-end (closes v1 deferred follow-up from `[[session-handoff-aspect-ratio-shipped]]`)
**Status:** Design approved, ready for implementation plan
**Repo scope:** `pcsx2-libretro` (1-2 lines + test updates) and `RetroNest-Project` (two small helpers)

## Problem

When the user selects `pcsx2_aspect_ratio = Stretch` in the PCSX2 core options, the displayed image should fill the RetroNest display item edge-to-edge — matching standalone PCSX2's Stretch behavior. Today it letterboxes to 4:3 instead.

Root cause is a two-stage v1 no-op:

1. **pcsx2-libretro** (`AspectRatio.cpp:43`) — `kStretch` returns `4.0f / 3.0f`, not `0.0f`. The v1 ship documented this as a known limitation in `[[session-handoff-aspect-ratio-shipped]]` because RetroNest would have remapped 0.0 to 4:3 anyway (next bullet).

2. **RetroNest-Project** (`game_session.cpp:503-512`) — `GameSession::setLibretroAspectRatio(0.0)` is treated as "core didn't fill the field" and remapped to `4.0/3.0`. There is no current path for "core asked for fill semantics."

So even the user toggling RetroNest's per-emulator aspect mode to `stretch` doesn't help when PCSX2 is set to Stretch: RetroNest's stretch path fills its display item, but PCSX2 then letterboxes inside its surface based on `EmuConfig.GS.AspectRatio == Stretch` falling through to `clientAr` (the surface aspect) — which itself is the screen aspect, so no internal bars; but the user expected the PCSX2 Stretch option to drive the visible result.

Surfaced 2026-05-19 immediately after the aspect-ratio-plumbing sub-project shipped, during the same smoke session. Memo `[[session-handoff-aspect-ratio-shipped]]` already records this as a known follow-up.

## Goal

Make `pcsx2_aspect_ratio = Stretch` flow through end-to-end so the display fills the RetroNest item edge-to-edge — equivalent to setting RetroNest's per-emulator aspect mode to `stretch`. Two equivalent user paths to fill behavior; both work.

## Non-goals (v1)

- **Image-#4 letterbox-thickness gap.** Distinct issue: with `pcsx2_aspect_ratio = 16:9` + RetroNest `aspectMode = native`, RetroNest's display item bounds in fullscreen appear to give a slightly thicker letterbox than standalone PCSX2 at the same screen size. Likely Qt scene-coords / DPR / borderless-window-vs-screen math. Out of scope here; investigate separately if still observable after this ships.
- **FMV aspect override** (`pcsx2_fmv_aspect_ratio`) — same as the prior sub-project: needs a separate PCSX2 → libretro signaling channel.
- **New RetroNest UI knobs or core options.** No UI surface changes.

## Anchors in current code

| Concern | Location |
| --- | --- |
| Stretch returns 4/3 today (v1 no-op) | `pcsx2-libretro/pcsx2-libretro/AspectRatio.cpp:43` |
| Test cases for Stretch | `pcsx2-libretro/pcsx2-libretro/tools/test_aspect_ratio.cpp` (3 cases) |
| Header file-level comment | `pcsx2-libretro/pcsx2-libretro/AspectRatio.h:1-13` |
| RetroNest's `ratio <= 0 → 4/3` remap | `RetroNest-Project/cpp/src/core/game_session.cpp:503-512` |
| RetroNest's aspect-mode "native" fit | `RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm:111-156` |
| Stretch path already exists (line 124) | `RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm:124-127` |
| QML host (no changes) | `RetroNest-Project/cpp/qml/AppUI/EmulationView.qml:58-83` |

## Approach: 0.0 sentinel means "fill"

Reinterpret `aspect_ratio = 0.0f` from the libretro core as "fill the display item" rather than "core didn't fill the field." Both repos updated to honor the new semantic.

### Mapping changes

| Layer | Before | After |
| --- | --- | --- |
| `AspectRatio::ComputeFromInputs(kStretch, ...)` | `4.0f / 3.0f` | `0.0f` |
| `GameSession::setLibretroAspectRatio(0.0)` | Remaps to `4.0/3.0`, stores; emits change | Stores `0.0`; emits change |
| `LibretroMetalItem::updateInnerGeometry()` when `aspectMode == "native"` AND `m_nativeAspect <= 0.0` | Falls back to fit 4:3 in bounds | Sets `m_window` to full `bounds.toRect()` (same as existing `aspectMode == "stretch"` branch) |

The existing explicit `aspectMode` paths (`"4_3"`, `"16_9"`, `"stretch"`) keep current semantics.

## Components

### 1. `pcsx2-libretro/pcsx2-libretro/AspectRatio.cpp`

Switch case at line 43 becomes:

```cpp
case kStretch:
    // Stretch signals "no aspect constraint" to the frontend. RetroNest
    // (and any libretro frontend that follows the 0.0 = "unspecified"
    // convention from libretro.h) fills the display surface edge-to-edge.
    // See spec 2026-05-19-retronest-libretro-stretch-flow-through-design.md.
    return 0.0f;
```

### 2. `pcsx2-libretro/pcsx2-libretro/AspectRatio.h`

File-level comment (`AspectRatio.h:6-13`) updated. The Stretch sentence inverts:

```cpp
// Stretch returns 0.0f — libretro's "no aspect specified" sentinel. Frontends
// that honor it (e.g. RetroNest as of 2026-05-19) fill the display surface
// edge-to-edge, matching standalone PCSX2's Stretch semantics. Frontends that
// don't honor it fall back to their own default (usually 4:3).
```

### 3. `pcsx2-libretro/pcsx2-libretro/tools/test_aspect_ratio.cpp`

Three Stretch test cases updated. Total case count stays at 14:

```cpp
// Stretch: 0.0 sentinel = "fill the display item" (no aspect constraint).
check("Stretch → 0.0",          ComputeFromInputs(AR_STRETCH, 0.0f, VM_NTSC),         0.0f);
check("Stretch ignores custom", ComputeFromInputs(AR_STRETCH, 1.777f, VM_NTSC),       0.0f);
check("Stretch + SDTV_480P stays 0.0",
                                 ComputeFromInputs(AR_STRETCH, 0.0f, VM_SDTV_480P),   0.0f);
```

### 4. `RetroNest-Project/cpp/src/core/game_session.cpp`

`setLibretroAspectRatio` (lines 503-512) stops remapping `ratio <= 0`:

```cpp
void GameSession::setLibretroAspectRatio(qreal ratio) {
    // CoreRuntime calls this once per session after retro_get_system_av_info,
    // and again whenever the core re-emits SET_SYSTEM_AV_INFO.
    //
    // ratio > 0  → explicit aspect (e.g. 4/3, 16/9, custom-from-patch).
    // ratio == 0 → libretro convention: "no aspect specified." LibretroMetalItem
    //              treats this as fill-the-bounds (Stretch semantics). The
    //              pcsx2-libretro core emits 0.0 when its pcsx2_aspect_ratio
    //              option is set to Stretch.
    if (qFuzzyCompare(m_libretroAspectRatio, ratio)) return;
    m_libretroAspectRatio = ratio;
    emit libretroAspectRatioChanged();
}
```

The `qFuzzyCompare` short-circuit moves to the top; `if (ratio <= 0.0) ratio = 4.0 / 3.0;` is deleted.

### 5. `RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm`

`updateInnerGeometry` adds a sentinel branch right after the existing stretch branch. The new branch:

```cpp
// Stretch mode = fill the whole item rect, no letterbox.
if (m_aspectMode == QStringLiteral("stretch")) {
    m_window->setGeometry(bounds.toRect());
    return;
}

// Core signaled "no aspect specified" (e.g. PCSX2 with pcsx2_aspect_ratio
// = Stretch). Treat as fill, equivalent to explicit stretch aspect mode.
if (m_nativeAspect <= 0.0 && m_aspectMode == QStringLiteral("native")) {
    m_window->setGeometry(bounds.toRect());
    return;
}
```

Placement: between the existing `aspectMode == "stretch"` branch and the `targetAR` computation. Explicit `"4_3"` / `"16_9"` paths below are unaffected — they keep overriding regardless of `m_nativeAspect`.

## Data flow

```
[user picks pcsx2_aspect_ratio = Stretch]
        │
        ▼
EmuConfig.GS.AspectRatio = Stretch
        │
        ▼
AspectRatio::Compute() → 0.0f           (was 4.0f/3.0f)
        │
        ▼
retro_get_system_av_info reports aspect_ratio = 0.0
        │
        ▼
CoreRuntime emits aspectRatioReported(0.0)
        │
        ▼
GameSession::setLibretroAspectRatio(0.0)
  → stores 0.0                          (was: remapped to 4/3)
  → emits libretroAspectRatioChanged()
        │
        ▼
LibretroMetalItem.nativeAspect = 0.0
        │
        ▼
updateInnerGeometry sees aspectMode == "native" AND m_nativeAspect <= 0.0
  → m_window->setGeometry(bounds.toRect())   (was: fit 4:3 in bounds)
        │
        ▼
PCSX2 receives a surface that fills the display item area
PCSX2 internal targetAr falls through to clientAr (surface aspect)
  → no internal letterbox
        │
        ▼
[Display: edge-to-edge fill — matches standalone Stretch]
```

## Error handling

- `qFuzzyCompare(m_libretroAspectRatio, ratio)` short-circuit prevents redundant signal emits when the core re-reports the same value.
- `m_window` and `window()` null checks at the top of `updateInnerGeometry` are preserved (existing pattern).
- The new `m_nativeAspect <= 0.0` check is gated on `aspectMode == "native"` so explicit user overrides keep working — a user who picks RetroNest aspect mode `4_3` while PCSX2 is set to Stretch sees 4:3 (RetroNest wins), not fill. This matches the explicit-wins precedent already in the file.

## Testing

### Unit (pcsx2-libretro)

Edit `tools/test_aspect_ratio.cpp` so the three Stretch cases assert `0.0f`. Rebuild standalone test:

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
  clang++ -std=c++20 -I../ test_aspect_ratio.cpp ../AspectRatio.cpp \
    -o test_aspect_ratio -DSP_ASPECT_TEST_ONLY && \
  ./test_aspect_ratio
```

Expected: 14 PASS lines, `0 failure(s)`.

### Manual smoke (Rosetta x86_64)

Build sequence:

```bash
arch -x86_64 /usr/local/bin/cmake --build \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64 \
  --target pcsx2_libretro -j 4

/Users/mark/Documents/Projects/RetroNest-Project/scripts/lipo-merge-dylib.sh \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib \
  /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib

cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 \
  --target RetroNest -j 4
```

Three smoke scenarios on R&C 2 (NTSC):

1. **Stretch flow-through (the headline fix)** — set `pcsx2_aspect_ratio = Stretch`, RetroNest aspect mode `native`. Expect:
   - Log: `[AspectRatio] re-emitted aspect=0.0000 (was -1.0000)` on first frame.
   - Display: fills the RetroNest item edge-to-edge. Compare against image #6 baseline — visible bars should disappear.

2. **16:9 unchanged** — set `pcsx2_aspect_ratio = 16:9`, RetroNest aspect mode `native`. Expect:
   - Log: `[AspectRatio] re-emitted aspect=1.7778 (was -1.0000)`.
   - Display: same as image #4 (no regression).

3. **Explicit RetroNest override still wins** — `pcsx2_aspect_ratio = Stretch`, RetroNest aspect mode `4_3`. Expect:
   - Log: `aspect=0.0000`.
   - Display: 4:3 letterboxed (RetroNest aspectMode overrides the libretro sentinel).

## Constraints

- Don't touch the `aspectMode != "native"` branches in `updateInnerGeometry`. Their behavior is correct as-is.
- Don't introduce a new public API surface in RetroNest. The sentinel flows through the existing `nativeAspect` qreal property.
- Coordinated ship: pcsx2-libretro change alone produces `aspect_ratio = 0.0` that RetroNest then remaps to 4:3 (regression). RetroNest change alone has nothing emitting 0.0. Land both together (one commit per repo, or coordinated PRs).

## Time estimate

Half-day end-to-end:

- Spec + plan: 1h (this doc + the plan that follows)
- Implementation: 1-2h (5 small edits across 2 repos, all known locations)
- Smoke under Rosetta x86_64: 30min
- Memory close-out + commits + push: 15min

## Related memories

- `[[session-handoff-aspect-ratio-shipped]]` — preceding sub-project. Stretch=4:3 was explicitly documented there as a deferred v1 follow-up; this sub-project closes that follow-up.
- `[[project-pcsx2-libretro-port]]` — overall port tracker.
- `[[aspect-ratio-pickup]]` — closed by the preceding sub-project; this sub-project further refines the same surface.
