# RetroNest libretro 16:9 letterbox parity — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the visible-letterbox gap between RetroNest's PCSX2 display and standalone PCSX2 at `pcsx2_aspect_ratio = 16:9` fullscreen. Other aspect modes already match; only 16:9 is wrong.

**Architecture:** Diagnostic-first single-file sub-project in `RetroNest-Project`. Instrument `LibretroMetalItem::updateInnerGeometry` with a one-shot log line; user measures live; diagnose from the numbers; apply targeted fix (predicted fix: collapse the `aspectMode=="native" && nativeAspect>0` branch into the same fill-bounds path as `stretch`, letting PCSX2 own the aspect math like it does in standalone). Strip the instrumentation before committing.

**Tech Stack:** Objective-C++ / Qt 6 / QML. No new infrastructure.

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-19-retronest-libretro-16-9-letterbox-parity-design.md` (commit `c6559f5`).

**Working repository:** `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`).

---

## File Structure

| File | Role | Action |
| --- | --- | --- |
| `cpp/src/ui/libretro/libretro_metal_item.mm` | All edits live here. `updateInnerGeometry` gets the instrument, then the fix, then the instrument is reverted. | **Modify** in 3 phases |

One commit total: the fix. Instrumentation is a working-tree-only throwaway — never committed.

---

## Task 1: Instrument `updateInnerGeometry` with one-shot diagnostic

**Files:**
- Modify: `cpp/src/ui/libretro/libretro_metal_item.mm:111` (head of `updateInnerGeometry`)

We add one combined log line that fires the first time `updateInnerGeometry` runs with non-degenerate bounds, capturing every input the math depends on plus the chosen branch's resulting `m_window` geometry. Static `bool` guard keeps it to exactly one line per process.

- [ ] **Step 1: Read the current function header to anchor the insertion**

```bash
sed -n '111,128p' /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm
```

Expected: lines 111-128 show:
```cpp
void LibretroMetalItem::updateInnerGeometry()
{
    if (!m_window || !window()) return;
    const QRectF bounds = mapRectToScene(boundingRect());

    const double bw = bounds.width();
    const double bh = bounds.height();
    if (bw < 1.0 || bh < 1.0) {
        m_window->setGeometry(bounds.toRect());
        return;
    }

    // Stretch mode = fill the whole item rect, no letterbox.
    if (m_aspectMode == QStringLiteral("stretch")) {
        m_window->setGeometry(bounds.toRect());
        return;
    }
```

- [ ] **Step 2: Add a `#include <QtDebug>` if it's not already present**

Check:
```bash
grep -n "QtDebug\|qInfo\|qDebug" /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm | head -5
```

If `qWarning` already appears (likely — it's used elsewhere in this file), no include needed; `<QtGlobal>` brings in the logging macros transitively. Skip to step 3.

If neither `qInfo` nor `qDebug` is reachable, add `#include <QtDebug>` near the top of the file's includes.

- [ ] **Step 3: Insert the one-shot diagnostic block at the top of `updateInnerGeometry`**

Use Edit with `old_string`:
```cpp
void LibretroMetalItem::updateInnerGeometry()
{
    if (!m_window || !window()) return;
    const QRectF bounds = mapRectToScene(boundingRect());

    const double bw = bounds.width();
    const double bh = bounds.height();
    if (bw < 1.0 || bh < 1.0) {
        m_window->setGeometry(bounds.toRect());
        return;
    }
```

And `new_string`:
```cpp
void LibretroMetalItem::updateInnerGeometry()
{
    if (!m_window || !window()) return;
    const QRectF bounds = mapRectToScene(boundingRect());

    const double bw = bounds.width();
    const double bh = bounds.height();

    // [DIAG] One-shot log capturing every input the aspect math depends on.
    // Removed before the fix lands. Spec:
    // 2026-05-19-retronest-libretro-16-9-letterbox-parity-design.md
    static bool s_diag_logged = false;
    if (!s_diag_logged && bw >= 1.0 && bh >= 1.0) {
        s_diag_logged = true;
        qInfo("[LibretroMetalItem.diag] bounds=(%.1f,%.1f %0.fx%0.f) window=%dx%d aspectMode=%s nativeAspect=%.4f DPR=%.2f",
              bounds.x(), bounds.y(), bw, bh,
              window() ? window()->width() : -1,
              window() ? window()->height() : -1,
              m_aspectMode.toUtf8().constData(),
              m_nativeAspect,
              window() ? window()->devicePixelRatio() : 1.0);
    }

    if (bw < 1.0 || bh < 1.0) {
        m_window->setGeometry(bounds.toRect());
        return;
    }
```

- [ ] **Step 4: Add a one-shot log at the final `setGeometry` call (the aspect-fit branch — the one that's suspected wrong)**

Find the `m_window->setGeometry(QRect(x, y, w, h));` line at the end of the function. Use Edit with `old_string`:

```cpp
    const int x = static_cast<int>(std::floor(bounds.x() + (bw - tw) / 2.0));
    const int y = static_cast<int>(std::floor(bounds.y() + (bh - th) / 2.0));
    const int w = static_cast<int>(std::floor(tw));
    const int h = static_cast<int>(std::floor(th));
    m_window->setGeometry(QRect(x, y, w, h));
}
```

And `new_string`:
```cpp
    const int x = static_cast<int>(std::floor(bounds.x() + (bw - tw) / 2.0));
    const int y = static_cast<int>(std::floor(bounds.y() + (bh - th) / 2.0));
    const int w = static_cast<int>(std::floor(tw));
    const int h = static_cast<int>(std::floor(th));

    // [DIAG] One-shot log on the aspect-fit branch (suspected wrong path).
    static bool s_diag_geom_logged = false;
    if (!s_diag_geom_logged) {
        s_diag_geom_logged = true;
        qInfo("[LibretroMetalItem.diag] aspect-fit setGeometry: targetAR=%.4f boundsAR=%.4f setGeom=(%d,%d %dx%d)",
              targetAR, bw / bh, x, y, w, h);
    }

    m_window->setGeometry(QRect(x, y, w, h));
}
```

- [ ] **Step 5: Verify the file has the diagnostic but no compile errors yet (visual check only)**

```bash
grep -n "\\[DIAG\\]\\|s_diag" /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm
```

Expected: 4 matches (2 comments + 2 statics).

DO NOT commit. The diagnostic is throwaway — it will be reverted before the fix commit lands.

---

## Task 2: Build RetroNest with the diagnostic

**Files:**
- No source changes — build only.

- [ ] **Step 1: Build**

```bash
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target RetroNest -j 4 2>&1 | grep -E "Building|error|Built target RetroNest" | tail -8
```

Expected: a `Building OBJCXX object .../libretro_metal_item.mm.o` line followed by `[100%] Built target RetroNest`. If errors mention `qInfo` not found, go back to Task 1 step 2 and add `#include <QtDebug>`.

- [ ] **Step 2: Confirm binary mtime**

```bash
stat -f "%Sm  %N" /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest
```

Expected: mtime within the last minute.

---

## Task 3: User measures (STOP for human-in-the-loop)

**Files:**
- No source changes — runtime data collection.

- [ ] **Step 1: Launch RetroNest with log capture**

```bash
rm -f /tmp/retronest-16-9-diag.log && \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
    >/tmp/retronest-16-9-diag.log 2>&1 &
```

- [ ] **Step 2: User runs the 16:9 fullscreen smoke**

Tell the user:
> Launch a PCSX2 game with `pcsx2_aspect_ratio = 16:9` and RetroNest aspect mode `native`. Make sure RetroNest is at fullscreen. Report when you've reached the in-game scene where the letterbox is visible.

Wait for user confirmation. (This is the user-smoke checkpoint — no implementer subagent should "drive" this.)

- [ ] **Step 3: Pull the diagnostic lines**

```bash
grep "\\[LibretroMetalItem.diag\\]" /tmp/retronest-16-9-diag.log
```

Expected: 2 lines. Example shape:
```
[LibretroMetalItem.diag] bounds=(0.0,0.0 1976x1230) window=1976x1230 aspectMode=native nativeAspect=1.7778 DPR=2.00
[LibretroMetalItem.diag] aspect-fit setGeometry: targetAR=1.7778 boundsAR=1.6065 setGeom=(0,59 1976x1112)
```

The actual numbers determine which hypothesis matches:

- **If `bounds.h` ≈ `window.h` AND `setGeom` height ≈ `bw / 1.7778`** → hypothesis #2 (PCSX2 internally re-letterboxes). Proceed to Task 4 with the **predicted fix**.
- **If `bounds.h` < `window.h`** → hypothesis #1 (QML layout). Proceed to Task 4 but the fix lives elsewhere (see "alternative diagnosis" callout below).
- **If `bounds.h` ≈ `window.h` AND `setGeom` height is much smaller than `bw / 1.7778`** → hypothesis #3 (DPR / drawableSize). Proceed to Task 4 with alternative.

Controller decides; implementer proceeds with the right fix.

---

## Task 4: Apply the predicted fix — collapse `native + positive aspect` into fill-bounds

**Files:**
- Modify: `cpp/src/ui/libretro/libretro_metal_item.mm:130` area (the existing sentinel-fill branch)

**This task is written assuming hypothesis #2 is confirmed.** Read the "alternative diagnosis" callout at the end of Task 3 if numbers point elsewhere — the fix surface differs.

The fix expands the existing fill-branch trigger condition so that `aspectMode=="native" && nativeAspect>0` also fills (today it falls through to aspect-fit). The result: in "native" mode, PCSX2 always gets the full bounds and computes its own letterbox — exactly mirroring standalone.

- [ ] **Step 1: Read the current sentinel-fill branch to anchor the edit**

```bash
sed -n '129,141p' /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm
```

Expected: lines 129-141 show the existing sentinel branch:
```cpp
    // Sentinel: core signaled "no aspect specified" (e.g. PCSX2 with
    // pcsx2_aspect_ratio = Stretch — emits 0.0 per libretro.h convention).
    // In "native" aspect mode the core's signal is what we follow, so fill
    // the bounds the same way explicit stretch does. Explicit aspect-mode
    // overrides ("4_3", "16_9") below are unaffected — they win regardless.
    if (m_aspectMode == QStringLiteral("native") && m_nativeAspect <= 0.0) {
        m_window->setGeometry(bounds.toRect());
        return;
    }
```

- [ ] **Step 2: Replace the conditional and update the comment**

Use Edit with `old_string`:
```cpp
    // Sentinel: core signaled "no aspect specified" (e.g. PCSX2 with
    // pcsx2_aspect_ratio = Stretch — emits 0.0 per libretro.h convention).
    // In "native" aspect mode the core's signal is what we follow, so fill
    // the bounds the same way explicit stretch does. Explicit aspect-mode
    // overrides ("4_3", "16_9") below are unaffected — they win regardless.
    if (m_aspectMode == QStringLiteral("native") && m_nativeAspect <= 0.0) {
        m_window->setGeometry(bounds.toRect());
        return;
    }
```

And `new_string`:
```cpp
    // Native aspect mode: give PCSX2 the full bounds and let it own the
    // letterbox math. PCSX2 (via libretro) already letterboxes its rendered
    // content according to pcsx2_aspect_ratio — when RetroNest letterboxes
    // a second time on top, the visible bars double (e.g. 16:9 on a 16:10
    // screen showed ~13% bars in RetroNest vs ~5% in standalone, because
    // both layers fitted 16:9 inside their respective surfaces).
    //
    // Filling unconditionally in native mode (rather than only on the 0.0
    // sentinel) matches standalone PCSX2's behavior: standalone owns its
    // window directly and computes the letterbox once. Here, "owning the
    // window" means m_window covers the whole item rect, and PCSX2's
    // GSRenderer::CalculateDrawDstRect picks the letterbox.
    //
    // Explicit aspect-mode overrides ("4_3", "16_9", "stretch") below are
    // unaffected — they still force their specific ratio regardless.
    if (m_aspectMode == QStringLiteral("native")) {
        m_window->setGeometry(bounds.toRect());
        return;
    }
```

- [ ] **Step 3: Verify the conditional is correctly broadened**

```bash
grep -n 'm_aspectMode == QStringLiteral("native")' /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm
```

Expected: one match (the broadened branch above). The previous version had an `&& m_nativeAspect <= 0.0` clause; it should be gone.

### Alternative diagnosis callout (only if Task 3's data points elsewhere)

If `bounds.h < window.h`: the fix is NOT in this file. Walk the QML tree:
1. `cpp/qml/AppUI/EmulationView.qml` — check the `LibretroMetalItem` and its `Loader` parent.
2. `cpp/qml/AppUI/AppWindow.qml` — check the `StackView`.

Find the link with explicit non-100% height/anchor, fix the anchor (`anchors.fill: parent`) or remove the explicit height. STOP and report back; don't proceed with the predicted fix.

If `setGeom` height is much smaller than `bw / 1.7778`: a DPR conversion is happening somewhere. Audit `syncContentsScale` and the CAMetalLayer `drawableSize` math in `libretro_metal_item.mm:158-205`. STOP and report back.

---

## Task 5: Remove the Task 1 diagnostic

**Files:**
- Modify: `cpp/src/ui/libretro/libretro_metal_item.mm` (revert the inserts from Task 1 steps 3-4)

- [ ] **Step 1: Remove the first diagnostic block (at function head)**

Use Edit with `old_string`:
```cpp
    const double bw = bounds.width();
    const double bh = bounds.height();

    // [DIAG] One-shot log capturing every input the aspect math depends on.
    // Removed before the fix lands. Spec:
    // 2026-05-19-retronest-libretro-16-9-letterbox-parity-design.md
    static bool s_diag_logged = false;
    if (!s_diag_logged && bw >= 1.0 && bh >= 1.0) {
        s_diag_logged = true;
        qInfo("[LibretroMetalItem.diag] bounds=(%.1f,%.1f %0.fx%0.f) window=%dx%d aspectMode=%s nativeAspect=%.4f DPR=%.2f",
              bounds.x(), bounds.y(), bw, bh,
              window() ? window()->width() : -1,
              window() ? window()->height() : -1,
              m_aspectMode.toUtf8().constData(),
              m_nativeAspect,
              window() ? window()->devicePixelRatio() : 1.0);
    }

    if (bw < 1.0 || bh < 1.0) {
```

And `new_string`:
```cpp
    const double bw = bounds.width();
    const double bh = bounds.height();
    if (bw < 1.0 || bh < 1.0) {
```

- [ ] **Step 2: Remove the second diagnostic block (at the aspect-fit setGeometry call)**

Use Edit with `old_string`:
```cpp
    const int x = static_cast<int>(std::floor(bounds.x() + (bw - tw) / 2.0));
    const int y = static_cast<int>(std::floor(bounds.y() + (bh - th) / 2.0));
    const int w = static_cast<int>(std::floor(tw));
    const int h = static_cast<int>(std::floor(th));

    // [DIAG] One-shot log on the aspect-fit branch (suspected wrong path).
    static bool s_diag_geom_logged = false;
    if (!s_diag_geom_logged) {
        s_diag_geom_logged = true;
        qInfo("[LibretroMetalItem.diag] aspect-fit setGeometry: targetAR=%.4f boundsAR=%.4f setGeom=(%d,%d %dx%d)",
              targetAR, bw / bh, x, y, w, h);
    }

    m_window->setGeometry(QRect(x, y, w, h));
}
```

And `new_string`:
```cpp
    const int x = static_cast<int>(std::floor(bounds.x() + (bw - tw) / 2.0));
    const int y = static_cast<int>(std::floor(bounds.y() + (bh - th) / 2.0));
    const int w = static_cast<int>(std::floor(tw));
    const int h = static_cast<int>(std::floor(th));
    m_window->setGeometry(QRect(x, y, w, h));
}
```

- [ ] **Step 3: Verify no `[DIAG]` markers remain**

```bash
grep -n "\\[DIAG\\]\\|s_diag" /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm
```

Expected: zero matches.

---

## Task 6: Build RetroNest with the fix

**Files:**
- No source changes — build only.

- [ ] **Step 1: Build**

```bash
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target RetroNest -j 4 2>&1 | grep -E "Building|error|Built target RetroNest" | tail -8
```

Expected: `[100%] Built target RetroNest`. Only `libretro_metal_item.mm.o` should be rebuilt (plus link step).

- [ ] **Step 2: Confirm binary mtime**

```bash
stat -f "%Sm  %N" /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest
```

Expected: mtime within the last minute.

---

## Task 7: User smoke test (STOP)

**Files:**
- No source changes — runtime verification.

Launch RetroNest fresh:

```bash
rm -f /tmp/retronest-16-9-smoke.log && \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
    >/tmp/retronest-16-9-smoke.log 2>&1 &
```

Exercise four scenarios in the UI. Confirm each visually.

- [ ] **Step 1: 16:9 parity (the headline fix)**

`pcsx2_aspect_ratio = 16:9`, RetroNest aspectMode `native`. Load a PCSX2 title at fullscreen.

Expected: letterbox per side matches standalone (~5% on a 16:10 screen) within ~1%. Compare against the standalone screenshot (image #9 from the brainstorm).

- [ ] **Step 2: 4:3 no regression**

`pcsx2_aspect_ratio = 4:3`, RetroNest aspectMode `native`. Reload.

Expected: 4:3 letterbox in fullscreen, identical to before this sub-project. (User reported 4:3 already matched standalone.)

- [ ] **Step 3: Stretch no regression**

`pcsx2_aspect_ratio = Stretch`, RetroNest aspectMode `native`. Reload.

Expected: image fills the screen edge-to-edge (the prior Stretch sub-project's headline behavior, still working).

- [ ] **Step 4: Explicit RetroNest override no regression**

RetroNest aspectMode `4_3` (in RetroNest's per-emulator UI), PCSX2 core option `pcsx2_aspect_ratio = 16:9`. Reload.

Expected: 4:3 letterbox — RetroNest's explicit mode overrides what the core reports.

If any scenario regresses, STOP and report back. Do not proceed to commit.

---

## Task 8: Commit the fix

**Files:**
- No new changes — committing Tasks 4 + 5.

- [ ] **Step 1: Confirm working tree contains only `libretro_metal_item.mm` with the fix (no diagnostic remnants)**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && git status --short
```

Expected: exactly `M cpp/src/ui/libretro/libretro_metal_item.mm`.

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && git diff --stat
```

Expected: roughly +14 / −5 lines (the comment block expansion + conditional broadening).

- [ ] **Step 2: Stage and commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && \
git add cpp/src/ui/libretro/libretro_metal_item.mm && \
git commit -m "$(cat <<'EOF'
fix(libretro): let PCSX2 own letterbox math in native aspect mode

LibretroMetalItem::updateInnerGeometry was double-letterboxing in
"native" mode with a positive nativeAspect: RetroNest fitted 16:9
inside the screen-shaped bounds, then PCSX2 (via libretro) fitted 16:9
again inside the already-letterboxed m_window surface. Visible bars
were ~2.5× standalone's (~13% per side in RetroNest vs ~5% in
standalone, on a 16:10 screen).

Fix: in "native" mode, give PCSX2 the full bounds unconditionally and
let it own the aspect math — mirroring how standalone PCSX2 owns its
window directly. The sentinel branch (nativeAspect <= 0.0) collapses
into the same path. Explicit aspect-mode overrides ("4_3", "16_9",
"stretch") below are unaffected.

Smoke-verified on R&C 2 (Rosetta x86_64) across four scenarios:
- 16:9 parity → letterbox now matches standalone within ~1%
- 4:3 → unchanged (PCSX2's internal letterbox still does the right thing)
- Stretch → still fills edge-to-edge
- Explicit RetroNest aspectMode = 4_3 override → still wins

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 3: Verify the commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && git log -1 --stat
```

Expected: 1 file changed (`libretro_metal_item.mm`), insertions and deletions roughly matching the +14/-5 from Step 1.

---

## Task 9: Push origin + memory closeout

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/MEMORY.md` (top entry)
- Create: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_16_9_letterbox_parity_shipped.md`
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_stretch_flow_through_shipped.md` (mark the 16:9 follow-up closed)

- [ ] **Step 1: Push to origin**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && \
  git push origin main 2>&1 | tail -5
```

Expected: `<prior_sha>..<new_sha>  main -> main`.

- [ ] **Step 2: Write the session-handoff memory**

Create `session_handoff_16_9_letterbox_parity_shipped.md` with frontmatter (`type: project`). Body content:
- One-paragraph summary: what shipped, commit SHA, smoke result.
- The diagnosis: what the log line revealed (capture the actual numbers from Task 3's measurement so future investigators have the data).
- Note that 4:3 / 10:7 / Auto / Stretch all already matched standalone — only the native-with-positive-aspect path had the double-letterbox.
- Reference to the spec and plan.

- [ ] **Step 3: Update `session_handoff_stretch_flow_through_shipped.md`**

Add a closeout line near the top: `**16:9 follow-up CLOSED 2026-05-19 — see [[session-handoff-16-9-letterbox-parity-shipped]].**`

- [ ] **Step 4: Update `MEMORY.md` index**

Prepend a new top entry pointing at the new session-handoff memory (one-line summary in the established format).

---

## Self-Review Checklist (performed during plan authoring)

**Spec coverage:**
- ✅ Instrument step (spec Step 1) → Task 1
- ✅ Build with instrument → Task 2
- ✅ User measurement (spec Step 2) → Task 3
- ✅ Diagnose (spec Step 3) → controller decision at end of Task 3 + alternative-diagnosis callout in Task 4
- ✅ Apply fix (spec Step 4, predicted) → Task 4
- ✅ Revert instrumentation (spec Step 5) → Task 5
- ✅ Smoke test scenarios → Task 7 (4 scenarios as spec requires)
- ✅ Commit + push + memory → Tasks 8-9

**Placeholder scan:** no TBD / TODO / "implement later." Every step has actual code or actual commands with expected output.

**Type/identifier consistency:**
- `m_aspectMode == QStringLiteral("native")` (Task 4) — matches the existing pattern in this file (`QStringLiteral("stretch")`, etc.).
- `s_diag_logged` / `s_diag_geom_logged` (Task 1, removed in Task 5) — consistent across both insertion and removal.
- `m_nativeAspect` — existing field, used identically across all tasks.
- `[LibretroMetalItem.diag]` log prefix — searched-for in Task 3 step 3 matches what's emitted in Task 1.

---

## Execution Notes

- 9 tasks total. Three pause points: Task 3 (user measurement), Task 4 (controller diagnosis decision — may need to redirect to alternative-diagnosis callout), Task 7 (user smoke).
- The diagnostic in Tasks 1-2 is throwaway — it's added to the working tree but Task 5 reverts it before Task 8 commits. The fix commit contains only the actual fix.
- This is a single-repo, single-file sub-project. No `arch -x86_64 cmake .../pcsx2-libretro/build-x86_64 --target pcsx2_libretro` build cadence needed — the libretro shim is correct; the fix is purely RetroNest-side.
- If Task 3's measurement shows hypothesis #1 or #3 (not the predicted #2), the implementer should STOP and report rather than proceed with Task 4's predicted fix. The controller decides whether to dispatch a different fix.
