# RetroNest libretro 16:9 letterbox parity — diagnostic + fix

**Date:** 2026-05-19
**Sub-project:** Close the visible-letterbox gap between RetroNest's PCSX2 display and standalone PCSX2 at `pcsx2_aspect_ratio = 16:9` fullscreen.
**Status:** Design approved, ready for implementation plan
**Repo scope:** RetroNest-Project only (libretro shim already correct per `[[session-handoff-stretch-flow-through-shipped]]`).

## Problem

At identical fullscreen resolution with `pcsx2_aspect_ratio = 16:9` in PCSX2's core options, the visible letterbox in RetroNest is materially larger than in standalone PCSX2:

- **Standalone fullscreen 16:9** (image #9): ~5% letterbox per side — matches the mathematical expectation for 16:9 content fitted into a 16:10 screen (3-5% per side).
- **RetroNest fullscreen 16:9** (image #10): ~13% letterbox per side — about 2.5× the expected amount.

User's stated goal: "make our app PCSX2 video an exact match to PCSX2 standalone in all aspect ratios. Currently all are perfect other than 16:9." So this is a targeted bug, not a UX redesign — the math is wrong somewhere on the 16:9 path only.

## Goal

Visible letterbox at `pcsx2_aspect_ratio = 16:9` fullscreen matches standalone PCSX2 within ~1% per side.

## Non-goals

- **Other aspect modes** — 4:3, Auto, 10:7, Stretch all already match per user statement. We do not touch their code paths.
- **Windowed mode parity** — borderless-fullscreen is the deployed user mode; that's what we fix.
- **PCSX2-libretro changes** — the libretro shim correctly reports `aspect_ratio = 16/9` (= 1.7778) and the prior sub-project verified that with a log line. The bug is downstream in RetroNest.

## Hypothesis space

PCSX2 standalone (16:9 fullscreen, ~1976×1230 screen):
- Surface = full screen.
- `CalculateDrawDstRect`: `targetAr = 16/9 ≈ 1.778`, `clientAr = 1976/1230 ≈ 1.606`.
- Since `targetAr > clientAr`, content fills width → `tw=1976, th=1976/1.778≈1112`.
- Letterbox per side = `(1230 - 1112) / 2 ≈ 59 px ≈ 5% of screen height`. Matches image #9.

RetroNest (16:9 fullscreen, same screen):
- `LibretroMetalItem` with `aspectMode="native"`, `m_nativeAspect=1.778`.
- `updateInnerGeometry`: same math, should produce same 59 px letterbox.
- Observed: ~160 px letterbox per side ≈ 13%. Off by ~100 px per side.

Where can ~100 px per side disappear? Three candidate causes:

1. **`bounds` from `mapRectToScene(boundingRect())` is shorter than `window()->height()`.** Something in the QML tree between `AppWindow` and `LibretroMetalItem` reserves vertical space (a wrapping `Item` with explicit height/margins, or a `Loader` that doesn't fill).
2. **Coordinate-system / DPR mismatch.** `bounds` reports scene-coordinate (logical-pixel) values; `m_window->setGeometry` interprets them correctly per Qt docs. But if PCSX2's `CalculateDrawDstRect` then re-letterboxes inside the (already-fitted) m_window surface, we get nested letterbox. PCSX2 reads `clientAr` from the layer's drawable size — if that's reported wrong, the internal math doubles the bars.
3. **CAMetalLayer drawableSize / bounds mismatch.** `m_window->setGeometry` sets one rect; the inner CAMetalLayer's bounds (set by Qt or PCSX2's `GSDeviceMTL`) may not match. PCSX2 would render to a smaller-than-expected layer and the result gets drawn smaller.

Without measurements we can't tell which dominates. The fix is informed by the data.

## Approach: instrument → measure → diagnose → fix → revert instrument

Five-step plan, each step short and well-bounded.

### Step 1 — Instrument

Add a single one-shot log line in `LibretroMetalItem::updateInnerGeometry()`, gated by a file-static `bool g_logged = false;` so it fires exactly once per process. The log prints all four candidate metrics:

```cpp
static bool g_logged = false;
if (!g_logged && bw >= 1.0 && bh >= 1.0) {
    g_logged = true;
    qInfo().nospace()
        << "[LibretroMetalItem.diag] bounds=("
        << bounds.x() << "," << bounds.y() << "," << bw << "x" << bh
        << ") window=" << (window() ? window()->width() : -1)
        << "x" << (window() ? window()->height() : -1)
        << " aspectMode=" << m_aspectMode
        << " nativeAspect=" << m_nativeAspect;
    // Also log the computed m_window geometry just before setGeometry.
    // Done inline at each return site.
}
```

Additionally, at each `m_window->setGeometry(...)` call site, prepend:

```cpp
if (!g_logged_geom_<branch>) { qInfo() << "[LibretroMetalItem.diag] setGeometry=" << <rect>; g_logged_geom_<branch> = true; }
```

(One static bool per branch keeps the log to one line per code path, no per-frame spam.)

### Step 2 — Measure

User launches RetroNest fullscreen with `pcsx2_aspect_ratio = 16:9`. Log captures the diagnostic line once on first frame. User pastes log line to controller.

### Step 3 — Diagnose

Numbers tell which hypothesis is right:

- **bounds.height() < window()->height()** → cause #1 (QML layout). Walk the QML tree from `LibretroMetalItem` upward (`EmulationView` → `Loader` → `Item` chain → `StackView` → `AppWindow`). Find the link that doesn't pass full height. Fix: change anchors/layout on that link.
- **bounds.height() ≈ window()->height() AND setGeometry rect's height = bw/1.778** → cause #2. PCSX2 is doing its own letterbox inside the already-fitted m_window. Fix: either skip RetroNest's outer fit when `aspectMode == "native"` (let PCSX2 do all the math, matches standalone exactly), or audit where PCSX2 reads its `clientAr` from and verify it sees the m_window dimensions.
- **bounds matches but setGeometry rect's height is wrong (smaller than bw/1.778)** → cause #3 (DPR / layer-size). Audit `syncContentsScale` for off-by-DPR.

### Step 4 — Fix

Targeted to the diagnosed cause. The most likely fix (and the one I'd predict from the symptoms) is **cause #2 with the "skip RetroNest's outer fit in native mode" remedy**:

```cpp
// At the top of updateInnerGeometry, after the stretch and sentinel branches:
//
// In "native" mode with a positive nativeAspect, PCSX2 (via libretro) is
// already going to letterbox its rendered content according to its own
// pcsx2_aspect_ratio setting. We must not letterbox a second time on top
// of that — give PCSX2 the full bounds and let it own the aspect math.
// This matches standalone PCSX2's behavior exactly: PCSX2 owns the window
// (here: m_window), PCSX2 computes the letterbox.
if (m_aspectMode == QStringLiteral("native") && m_nativeAspect > 0.0) {
    m_window->setGeometry(bounds.toRect());
    return;
}
```

This collapses "native + positive aspect" into the same path as "stretch" and "native + sentinel" — all three give PCSX2 the full surface and let PCSX2 letterbox. RetroNest only takes over when the user explicitly picks `4_3` or `16_9` aspectMode (which forces a specific ratio regardless of what the core wants).

But this is the **predicted** fix. The actual fix is informed by step 3 measurements.

### Step 5 — Revert instrumentation

Once the fix lands and smoke confirms parity, remove the diagnostic log lines. They served their purpose; permanent leaving-in is just noise.

## Components

| File | Role | Steps |
| --- | --- | --- |
| `cpp/src/ui/libretro/libretro_metal_item.mm` | Diagnostic log + fix | Step 1 adds log; Step 4 fix; Step 5 removes log |

That's it. Single file, all changes in `updateInnerGeometry`.

## Smoke test (after fix)

Three scenarios on R&C 2 (NTSC), Rosetta x86_64, fullscreen:

1. **16:9 parity (the fix)** — `pcsx2_aspect_ratio = 16:9`, RetroNest aspectMode `native`. Visible letterbox per side should match standalone within ~1%. Compare against image #9.
2. **4:3 no regression** — `pcsx2_aspect_ratio = 4:3`, RetroNest aspectMode `native`. Should look identical to before (user reported 4:3 already matched standalone).
3. **Stretch no regression** — `pcsx2_aspect_ratio = Stretch`, RetroNest aspectMode `native`. Should still fill edge-to-edge (the prior sub-project's headline behavior).
4. **Explicit RetroNest override no regression** — RetroNest aspectMode `4_3` while PCSX2 is in 16:9. Should still be 4:3 letterbox (RetroNest wins).

## Constraints

- Single-repo change (RetroNest-Project only).
- One commit for the fix; the diagnostic step is its own throwaway commit that gets reverted, or kept temporary and never landed.
- Don't restructure `updateInnerGeometry` beyond the targeted fix.
- Don't touch any other aspect-mode branch (`"4_3"`, `"16_9"`, `"stretch"`).

## Time estimate

Half-day:
- Step 1 (instrument): 15 min.
- Step 2 (user measures): 10 min round-trip.
- Step 3 (diagnose): 15 min — the numbers will be conclusive.
- Step 4 (fix): 30 min — small and targeted, plus a build.
- Step 5 (revert + smoke + commit + push): 30 min.

## Related memories

- `[[session-handoff-stretch-flow-through-shipped]]` — preceding sub-project. The 16:9 letterbox-thickness gap was explicitly called out there as the next follow-up.
- `[[session-handoff-aspect-ratio-shipped]]` — earlier sub-project that established the libretro shim's correct reporting of aspect_ratio.
- `[[project-pcsx2-libretro-port]]` — port tracker.
