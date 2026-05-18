# RetroNest libretro Stretch flow-through — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When the user picks `pcsx2_aspect_ratio = Stretch` in PCSX2's core options, the displayed image fills the RetroNest display item edge-to-edge instead of falling back to 4:3.

**Architecture:** Two coordinated edits across two repos. (1) pcsx2-libretro: `AspectRatio::ComputeFromInputs` returns `0.0f` for `kStretch` instead of `4.0f/3.0f`. (2) RetroNest-Project: `GameSession::setLibretroAspectRatio` stops remapping `ratio <= 0` to 4:3; `LibretroMetalItem::updateInnerGeometry` treats `m_nativeAspect <= 0` (when `aspectMode == "native"`) as "fill the bounds" — same as the existing explicit-stretch branch. Both edits are independently regression-safe so either commit can land first without breaking the deployed app.

**Tech Stack:** C++20 (pcsx2-libretro), C++/Objective-C++ Qt6/QML (RetroNest-Project). No new infrastructure.

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-19-retronest-libretro-stretch-flow-through-design.md` (commit `481fbef`).

**Working repositories:**
- `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch: any feature branch off `main`)
- `/Users/mark/Documents/Projects/RetroNest-Project/` (branch: any feature branch off `main`)

---

## File Structure

| File | Role | Action |
| --- | --- | --- |
| `pcsx2-libretro/pcsx2-libretro/AspectRatio.cpp` | Stretch case in switch | **Modify** line 43 area |
| `pcsx2-libretro/pcsx2-libretro/AspectRatio.h` | File-level doc comment | **Modify** lines 10-13 area |
| `pcsx2-libretro/pcsx2-libretro/tools/test_aspect_ratio.cpp` | 3 Stretch unit-test cases | **Modify** three `check(...)` lines |
| `RetroNest-Project/cpp/src/core/game_session.cpp` | Sentinel passthrough | **Modify** `setLibretroAspectRatio` |
| `RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm` | Sentinel → fill branch | **Modify** `updateInnerGeometry` |

Two commits planned:
- **Commit A** (pcsx2-libretro): Stretch returns 0.0 + updated tests + comment. Self-contained, regression-safe (RetroNest still remaps 0.0 → 4/3 until Commit B).
- **Commit B** (RetroNest-Project): Sentinel passthrough + fill branch. Activates the Stretch flow-through end-to-end.

---

## Task 1: Update Stretch unit-test assertions

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/tools/test_aspect_ratio.cpp`

TDD red: change the three Stretch test cases to expect `0.0f` before changing the implementation.

- [ ] **Step 1: Find the three existing Stretch cases**

Run:
```bash
grep -n "Stretch\|AR_STRETCH" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_aspect_ratio.cpp
```
Expected output (line numbers approximate):
```
20:constexpr int AR_STRETCH    = 0;
51:    // Stretch: 4:3 fallback per spec (v1).
52:    check("Stretch → 4:3",  ComputeFromInputs(AR_STRETCH, 0.0f, VM_NTSC),       4.0f / 3.0f);
53:    check("Stretch ignores custom", ComputeFromInputs(AR_STRETCH, 1.777f, VM_NTSC), 4.0f / 3.0f);
...
69:    // Stretch deliberately diverges from upstream here — GSRenderer's
70:    // GetCurrentAspectRatioFloat would fall through to the Auto branch
71:    // and return 3:2 for progressive. Our v1 collapses Stretch to a
72:    // fixed 4:3 (spec: RetroNest's per-emulator stretch mode handles fill).
73:    check("Stretch + SDTV_480P stays 4:3",
74:                                     ComputeFromInputs(AR_STRETCH, 0.0f, VM_SDTV_480P),    4.0f / 3.0f);
```

- [ ] **Step 2: Replace the first Stretch block (the `4:3 fallback per spec` comment + 2 cases)**

Use Edit with `old_string`:
```
    // Stretch: 4:3 fallback per spec (v1).
    check("Stretch → 4:3",  ComputeFromInputs(AR_STRETCH, 0.0f, VM_NTSC),       4.0f / 3.0f);
    check("Stretch ignores custom", ComputeFromInputs(AR_STRETCH, 1.777f, VM_NTSC), 4.0f / 3.0f);
```

And `new_string`:
```
    // Stretch: 0.0 sentinel = "no aspect specified" (RetroNest fills the
    // display item edge-to-edge — see Stretch flow-through spec).
    check("Stretch → 0.0",  ComputeFromInputs(AR_STRETCH, 0.0f, VM_NTSC),       0.0f);
    check("Stretch ignores custom", ComputeFromInputs(AR_STRETCH, 1.777f, VM_NTSC), 0.0f);
```

- [ ] **Step 3: Replace the third Stretch case (the `SDTV_480P stays 4:3` block)**

Use Edit with `old_string`:
```
    // Stretch deliberately diverges from upstream here — GSRenderer's
    // GetCurrentAspectRatioFloat would fall through to the Auto branch
    // and return 3:2 for progressive. Our v1 collapses Stretch to a
    // fixed 4:3 (spec: RetroNest's per-emulator stretch mode handles fill).
    check("Stretch + SDTV_480P stays 4:3",
                                     ComputeFromInputs(AR_STRETCH, 0.0f, VM_SDTV_480P),    4.0f / 3.0f);
```

And `new_string`:
```
    // Stretch deliberately diverges from upstream here — GSRenderer's
    // GetCurrentAspectRatioFloat would fall through to the Auto branch
    // and return 3:2 for progressive. We emit the 0.0 sentinel regardless
    // of video mode so the frontend's fill semantics kick in.
    check("Stretch + SDTV_480P stays 0.0",
                                     ComputeFromInputs(AR_STRETCH, 0.0f, VM_SDTV_480P),    0.0f);
```

- [ ] **Step 4: Verify the file**

Run:
```bash
grep -c "AR_STRETCH" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_aspect_ratio.cpp
```
Expected: `4` (1 constant declaration + 3 `check` calls).

---

## Task 2: Verify the tests now fail against the current implementation

**Files:**
- No file changes — verification only.

- [ ] **Step 1: Rebuild and run the standalone test**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
  clang++ -std=c++20 -I../ test_aspect_ratio.cpp ../AspectRatio.cpp \
    -o test_aspect_ratio -DSP_ASPECT_TEST_ONLY && \
  ./test_aspect_ratio
```

Expected: 3 FAIL lines for the Stretch cases (current code returns `1.3333`, tests now expect `0.0000`); 11 PASS lines for the other cases; final `3 failure(s)`; exit code 1.

If you see anything different, stop and investigate — the implementation might already be returning 0.0 or one of the cases didn't update.

---

## Task 3: Change `AspectRatio.cpp` Stretch case to return `0.0f`

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/AspectRatio.cpp` (the `case kStretch:` block around line 37-44)

- [ ] **Step 1: Replace the Stretch case body**

Use Edit with `old_string`:
```cpp
        case kStretch:
            // See spec: RetroNest's aspect_ratio <= 0 fallback is 4:3, not
            // fill. Stretch-as-fill is delivered via RetroNest's per-emulator
            // aspect mode in v1; the libretro Stretch option is a no-op.
            return 4.0f / 3.0f;
```

And `new_string`:
```cpp
        case kStretch:
            // Stretch signals "no aspect constraint" to the frontend.
            // RetroNest interprets aspect_ratio == 0.0 as fill-the-display-
            // item-edge-to-edge (same path as its explicit aspectMode ==
            // "stretch"). Other libretro frontends fall back to their own
            // default (typically 4:3) per libretro.h convention. See spec
            // 2026-05-19-retronest-libretro-stretch-flow-through-design.md.
            return 0.0f;
```

- [ ] **Step 2: Verify**

Run:
```bash
grep -n "case kStretch" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/AspectRatio.cpp
```
Expected: one match. Following the line, the body should contain `return 0.0f;`.

---

## Task 4: Update `AspectRatio.h` file-level comment

**Files:**
- Modify: `pcsx2-libretro/pcsx2-libretro/AspectRatio.h` (lines 10-13)

- [ ] **Step 1: Replace the Stretch comment block**

Use Edit with `old_string`:
```cpp
// Stretch returns 4.0f/3.0f in v1 — RetroNest's display item treats
// aspect_ratio <= 0 as a fallback to 4:3, not as "fill". Stretch-as-fill
// is delivered via RetroNest's per-emulator aspect mode, not this option.
// See spec 2026-05-18-pcsx2-libretro-aspect-ratio-design.md for rationale.
```

And `new_string`:
```cpp
// Stretch returns 0.0f — libretro's "no aspect specified" sentinel.
// Frontends that honor it (RetroNest as of 2026-05-19) fill the display
// item edge-to-edge, matching standalone PCSX2's Stretch semantics.
// Frontends that don't honor it fall back to their own default (usually
// 4:3). See spec 2026-05-19-retronest-libretro-stretch-flow-through-design.md.
```

- [ ] **Step 2: Verify**

Run:
```bash
grep -n "0.0f" /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/AspectRatio.h
```
Expected: at least one match in the file-level comment region (around line 10).

---

## Task 5: Run the unit test again — should pass now

**Files:**
- No file changes — verification only.

- [ ] **Step 1: Rebuild and run**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
  clang++ -std=c++20 -I../ test_aspect_ratio.cpp ../AspectRatio.cpp \
    -o test_aspect_ratio -DSP_ASPECT_TEST_ONLY && \
  ./test_aspect_ratio | tail -3
```

Expected:
```
[PASS] Unknown enum → Auto default: got=1.3333 want=1.3333

0 failure(s)
```

If any case still fails, fix the implementation and re-run before proceeding to Task 6.

---

## Task 6: Commit pcsx2-libretro change (Commit A)

**Files:**
- No new changes — committing Tasks 1-5.

- [ ] **Step 1: Stage and commit**

Run from `/Users/mark/Documents/Projects/pcsx2-libretro/`:
```bash
git add pcsx2-libretro/AspectRatio.cpp pcsx2-libretro/AspectRatio.h \
        pcsx2-libretro/tools/test_aspect_ratio.cpp && \
git commit -m "$(cat <<'EOF'
feat(libretro): emit 0.0 for Stretch — sentinel for "fill" semantics

AspectRatio::ComputeFromInputs returns 0.0f when AspectRatioType::Stretch
is selected, matching libretro.h's "no aspect specified" convention.
Frontends that honor it fill the display surface edge-to-edge; frontends
that don't fall back to their own default (typically 4:3). This is a
no-op against the deployed RetroNest until its sibling commit lands —
RetroNest currently remaps ratio <= 0 to 4/3, so behavior is unchanged.

Closes the v1 deferred follow-up from session-handoff-aspect-ratio-shipped.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: clean commit, three files staged, hook passes.

- [ ] **Step 2: Verify**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro && git log -1 --stat
```
Expected: 3 files changed, all expected paths, ~15 insertions / ~15 deletions.

---

## Task 7: Rebuild + lipo-merge + deploy the dylib

**Files:**
- No source changes — build/deploy only. Replicates the recipe from `[[session-handoff-aspect-ratio-shipped]]`.

- [ ] **Step 1: Build x86_64 slice**

Run:
```bash
arch -x86_64 /usr/local/bin/cmake --build \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64 \
  --target pcsx2_libretro -j 4 2>&1 | tail -3
```

Expected: `[100%] Built target pcsx2_libretro`. If you see `unknown target CPU 'apple-m4'` you forgot the `arch -x86_64` prefix — that prefix is load-bearing per the build-cadence note in `[[session-handoff-aspect-ratio-shipped]]`.

- [ ] **Step 2: Verify new code is in the freshly built dylib**

Run:
```bash
strings /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib | grep "AspectRatio"
```

Expected: `[AspectRatio] re-emitted aspect=%.4f (was %.4f)` (from the SP wiring). The exact `0.0f` Stretch return doesn't show in strings — confirmed via Task 5's unit test.

- [ ] **Step 3: Lipo-merge with existing arm64 slice and deploy**

Run:
```bash
/Users/mark/Documents/Projects/RetroNest-Project/scripts/lipo-merge-dylib.sh \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
  /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib \
  /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib 2>&1 | tail -3
```

Expected: `✓ wrote universal /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib`. The arm64 slice will be stale (this is fine — the user's RetroNest runs x86_64 under Rosetta).

- [ ] **Step 4: Confirm deployed dylib is fresh**

Run:
```bash
stat -f "%Sm  %z bytes" /Users/mark/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

Expected: mtime within the last few minutes.

---

## Task 8: RetroNest — strip the ratio<=0 → 4/3 remap

**Files:**
- Modify: `RetroNest-Project/cpp/src/core/game_session.cpp:503-512`

- [ ] **Step 1: Replace `setLibretroAspectRatio`**

Use Edit with `old_string`:
```cpp
void GameSession::setLibretroAspectRatio(qreal ratio) {
    // CoreRuntime calls this once per session after retro_get_system_av_info.
    // Sentinels: 0 means "core didn't fill the field" — fall back to the
    // sensible PS2 default (4/3). qFuzzyCompare avoids a useless emit when
    // the core re-reports the same value.
    if (ratio <= 0.0) ratio = 4.0 / 3.0;
    if (qFuzzyCompare(m_libretroAspectRatio, ratio)) return;
    m_libretroAspectRatio = ratio;
    emit libretroAspectRatioChanged();
}
```

And `new_string`:
```cpp
void GameSession::setLibretroAspectRatio(qreal ratio) {
    // CoreRuntime calls this once per session after retro_get_system_av_info,
    // and again whenever the core re-emits SET_SYSTEM_AV_INFO.
    //
    // ratio > 0  → explicit aspect (e.g. 4/3, 16/9, custom-from-patch).
    // ratio == 0 → libretro convention: "no aspect specified."
    //              LibretroMetalItem treats this as fill-the-bounds
    //              (Stretch semantics). The pcsx2-libretro core emits 0.0
    //              when its pcsx2_aspect_ratio option is set to Stretch.
    //
    // qFuzzyCompare avoids a useless emit when the core re-reports the
    // same value (e.g. SP7a's region-refinement re-emit pass).
    if (qFuzzyCompare(m_libretroAspectRatio, ratio)) return;
    m_libretroAspectRatio = ratio;
    emit libretroAspectRatioChanged();
}
```

- [ ] **Step 2: Verify**

Run:
```bash
grep -n "if (ratio <= 0.0)" /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/game_session.cpp
```
Expected: no matches (the line is gone).

---

## Task 9: RetroNest — add fill branch for sentinel in `LibretroMetalItem`

**Files:**
- Modify: `RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm:123-127`

- [ ] **Step 1: Insert the sentinel-fill branch after the existing stretch branch**

Use Edit with `old_string`:
```cpp
    // Stretch mode = fill the whole item rect, no letterbox.
    if (m_aspectMode == QStringLiteral("stretch")) {
        m_window->setGeometry(bounds.toRect());
        return;
    }

    // Target aspect: explicit override modes win; "native" falls back to
    // m_nativeAspect (libretro av_info.geometry.aspect_ratio — 4/3 for PS2).
```

And `new_string`:
```cpp
    // Stretch mode = fill the whole item rect, no letterbox.
    if (m_aspectMode == QStringLiteral("stretch")) {
        m_window->setGeometry(bounds.toRect());
        return;
    }

    // Sentinel: core signaled "no aspect specified" (e.g. PCSX2 with
    // pcsx2_aspect_ratio = Stretch — emits 0.0 per libretro.h convention).
    // In "native" aspect mode the core's signal is what we follow, so fill
    // the bounds the same way explicit stretch does. Explicit aspect-mode
    // overrides ("4_3", "16_9") below are unaffected — they win regardless.
    if (m_aspectMode == QStringLiteral("native") && m_nativeAspect <= 0.0) {
        m_window->setGeometry(bounds.toRect());
        return;
    }

    // Target aspect: explicit override modes win; "native" falls back to
    // m_nativeAspect (libretro av_info.geometry.aspect_ratio — 4/3 for PS2).
```

- [ ] **Step 2: Verify the insertion point**

Run:
```bash
grep -n "Sentinel: core signaled" /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/libretro/libretro_metal_item.mm
```
Expected: one match, around line 128-130.

---

## Task 10: Build RetroNest

**Files:**
- No source changes — build verification only.

- [ ] **Step 1: Build**

Run:
```bash
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 \
  --target RetroNest -j 4 2>&1 | grep -E "Building|error|Built target RetroNest" | tail -10
```

Expected: `[100%] Built target RetroNest`. The build should incrementally rebuild only `game_session.cpp.o` and `libretro_metal_item.mm.o` plus the link step.

- [ ] **Step 2: Confirm the binary mtime**

Run:
```bash
stat -f "%Sm  %N" /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest
```
Expected: mtime within the last minute.

---

## Task 11: Manual smoke test (3 scenarios)

**Files:**
- No file changes — runtime verification.

Launch RetroNest with log capture:

```bash
rm -f /tmp/retronest-stretch-smoke.log && \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
    >/tmp/retronest-stretch-smoke.log 2>&1 &
```

Then exercise each scenario in the UI:

- [ ] **Step 1: Stretch flow-through (the headline fix)**

PCSX2 core option `pcsx2_aspect_ratio = Stretch`, RetroNest aspect mode = `native`. Load R&C 2.

Expected log line:
```
[AspectRatio] re-emitted aspect=0.0000 (was -1.0000)
```

Expected display: image fills the RetroNest display item edge-to-edge (no top/bottom or left/right bars from RetroNest's aspect fit; only bars come from RetroNest's display item not covering the full OS window — which is the separate non-goal issue).

Compare against image #6 (the prior stretch screenshot which showed obvious bars from RetroNest's 4:3 fallback). Bars should be visibly smaller / gone.

- [ ] **Step 2: 16:9 unchanged (no regression)**

Change PCSX2 core option to `pcsx2_aspect_ratio = 16:9`, RetroNest aspect mode `native`. Reload R&C 2.

Expected log line:
```
[AspectRatio] re-emitted aspect=1.7778 (was -1.0000)
```

Expected display: visually identical to image #4 from the prior session. Top/bottom letterbox only, no left/right padding. (The "thicker than standalone" property persists — it's the non-goal.)

- [ ] **Step 3: Explicit RetroNest override wins**

Change RetroNest aspect mode to `4_3` (via the RetroNest UI's per-emulator settings). PCSX2 core option stays `pcsx2_aspect_ratio = Stretch`. Reload.

Expected log line:
```
[AspectRatio] re-emitted aspect=0.0000 (was -1.0000)
```

Expected display: 4:3 letterbox (RetroNest's aspectMode overrides the libretro sentinel — by design).

- [ ] **Step 4: Verify log via grep**

Close the game in RetroNest, then:
```bash
grep "AspectRatio" /tmp/retronest-stretch-smoke.log
```
Expected: 3 log lines, one per scenario, with aspect values `0.0000`, `1.7778`, `0.0000`.

If scenario 1 fails (still shows 4:3 letterbox), the most likely cause is the lipo-merge step skipped or the new dylib wasn't picked up — check `stat` on the deployed dylib at the path used by the running RetroNest process.

---

## Task 12: Commit RetroNest change (Commit B)

**Files:**
- No new changes — committing Tasks 8-9.

- [ ] **Step 1: Stage and commit**

Run from `/Users/mark/Documents/Projects/RetroNest-Project/`:
```bash
git add cpp/src/core/game_session.cpp \
        cpp/src/ui/libretro/libretro_metal_item.mm && \
git commit -m "$(cat <<'EOF'
feat(libretro): honor aspect_ratio = 0.0 as "fill the display item"

GameSession::setLibretroAspectRatio no longer remaps ratio <= 0 to 4/3
— it stores the sentinel and propagates the change. LibretroMetalItem
adds a fill branch for the "native + nativeAspect <= 0" case, mirroring
the existing explicit-stretch branch.

Activates the pcsx2-libretro side's 0.0-for-Stretch emit (sibling
commit). When the user picks pcsx2_aspect_ratio = Stretch, the
display item now fills edge-to-edge instead of falling back to 4:3 —
matches standalone PCSX2's Stretch behavior. Explicit RetroNest
aspectMode values ("4_3", "16_9", "stretch") still override.

Smoke-verified on R&C 2 (Rosetta x86_64) across three scenarios:
Stretch → fill, 16:9 → unchanged, explicit 4:3 override → still wins.

Closes the v1 deferred follow-up from
session-handoff-aspect-ratio-shipped.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 2: Verify**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && git log -1 --stat
```
Expected: 2 files changed, ~25 insertions / ~6 deletions.

---

## Task 13: Push RetroNest + memory closeout

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/MEMORY.md`
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_aspect_ratio_shipped.md` (cross-reference closeout)
- Create: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_stretch_flow_through_shipped.md`

- [ ] **Step 1: Push RetroNest origin**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && \
  git push origin main 2>&1 | tail -5
```
Expected: `<prior_sha>..<new_sha>  main -> main`. pcsx2-libretro is a local-only fork — no push for that repo.

- [ ] **Step 2: Write the session-handoff memory**

Create `session_handoff_stretch_flow_through_shipped.md` with frontmatter (`type: project`), one-paragraph summary listing both commit SHAs, smoke results, and a note that the "thicker letterbox at 16:9 vs standalone" non-goal is still open as a potential follow-up.

- [ ] **Step 3: Update `session_handoff_aspect_ratio_shipped.md`**

Add a closeout line near the top: `**Stretch follow-up CLOSED 2026-05-19 — see [[session-handoff-stretch-flow-through-shipped]].**` Mark the "Known v1 limitation — Stretch is a no-op on libretro side" section as resolved.

- [ ] **Step 4: Update `MEMORY.md` index**

Replace the existing aspect-ratio entry's status with a pointer to the new session-handoff (one-line entry), or add a fresh top entry.

---

## Self-Review Checklist (performed during plan authoring)

**Spec coverage:**
- ✅ pcsx2-libretro Stretch → 0.0f → Task 3
- ✅ AspectRatio.h doc update → Task 4
- ✅ Three unit-test cases updated → Task 1
- ✅ RetroNest setLibretroAspectRatio sentinel passthrough → Task 8
- ✅ LibretroMetalItem fill branch → Task 9
- ✅ Three smoke scenarios (Stretch fills, 16:9 unchanged, explicit override wins) → Task 11
- ✅ Coordinated ship called out (both commits regression-safe in isolation) → Plan header + Task 6 / 12 commit messages
- ✅ Non-goals preserved (image-#4 letterbox-thickness, FMV) → noted in plan header

**Placeholder scan:** no TBD / TODO / "implement later". Every step has actual code or actual commands with expected output.

**Type/identifier consistency:**
- `setLibretroAspectRatio` (Task 8) — same identifier across all references
- `m_nativeAspect` (Task 9) — matches existing field name in `libretro_metal_item.{h,mm}`
- `m_aspectMode == QStringLiteral("native")` and `"stretch"` (Task 9) — exact-string comparisons match the rest of `updateInnerGeometry`
- `AR_STRETCH`, `VM_NTSC`, `VM_SDTV_480P`, `ComputeFromInputs` (Tasks 1-5) — all match the existing test file constants
- `0.0f` consistent across helper return + test assertions

---

## Execution Notes

- Total tasks: 13. Estimated 1-2 hours implementation + 30min smoke + 15min memory.
- Commit A (Tasks 1-6) is regression-safe — pcsx2-libretro alone produces `aspect_ratio = 0.0` that RetroNest still remaps to 4/3, so Stretch behavior is unchanged from today (4:3 letterbox) until Commit B lands.
- Commit B (Tasks 8-12) is also regression-safe in isolation — without Commit A, no `aspect_ratio = 0.0` is ever emitted, so the new fill branch never triggers.
- The order in this plan (A before B) is intentional: emit the sentinel first, then teach the consumer to interpret it. Reverse order would also work but reads less naturally as a story.
- Build-cadence: pcsx2-libretro changes require `arch -x86_64 /usr/local/bin/cmake --build .../pcsx2-libretro/build-x86_64 --target pcsx2_libretro` per `[[session-handoff-aspect-ratio-shipped]]`'s build-cadence note. The RetroNest cmake target alone does NOT trigger pcsx2-libretro rebuild. Task 7 covers the explicit per-arch build + lipo-merge.
- Smoke test must use the deployed dylib at `~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib`, not the per-arch builds. Task 7 step 3 deploys.
