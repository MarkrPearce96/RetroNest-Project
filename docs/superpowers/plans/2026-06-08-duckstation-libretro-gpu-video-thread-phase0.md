# DuckStation libretro — GPU video thread, Phase 0 (measurement spike) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Instrument the DuckStation libretro core to measure, at internal resolution 1×/4×/8×, how much per-frame core-thread time is spent in inline GPU command handling and present — the time that would move off-thread when `GPU/UseThread=true` — then apply the spec's go/no-go gate to decide whether to build the rework.

**Architecture:** Add a header-only, env-gated profiler (`GpuProf`) to the core. Three RAII/scoped timers feed three accumulators: `runframe` (whole `System::RunFrame`), `gpu_cmd` (every inline `GPUBackend::HandleCommand`), and `present` (whole `VideoPresenter::PresentFrame`, a subset of `gpu_cmd`). Once per frame `retro_run` calls `GpuProf::EndFrame`, which every 60 frames averages and prints a `[GPU_PROF]` line to stderr (→ `/tmp/rn.log` under RetroNest) and resets. The pure line-formatting math is split out as `FormatProfLine` so it is unit-testable without a GUI. Off by default (no `GPU_PROF` env var ⇒ zero added work).

**Tech Stack:** C++20, DuckStation fork core (`duckstation-libretro/`, git `master`, **local-only — never push**), RetroNest host (Qt/Metal), x86_64/Rosetta build.

**Scope:** This plan is **Phase 0 only**. Phases 1–7 (the actual threading rework) are gated on the measurement and get their own plan written *after* the numbers come back (Task 5 decides). Spec: `RetroNest-Project/docs/superpowers/specs/2026-06-08-duckstation-libretro-gpu-video-thread-design.md`.

**Conventions:**
- All code lives in `duckstation-libretro/`. Commit to the **core repo** (`duckstation-libretro`, git `master`, never push). The results doc (Task 5) lives in `duckstation-libretro/docs/`.
- `$DS` = `/Users/mark/Documents/Projects/duckstation-libretro`. Core `.cpp` files include siblings as `"gpu_prof.h"`; `libretro.cpp` (in `src/duckstation-libretro/`) includes as `"core/gpu_prof.h"`.
- TCC blocks the agent from launching the GUI, so **Task 4 is user-run** (the agent cannot observe the rendered app). Everything else is agent-runnable.

---

### Task 1: `GpuProf` header + pure-formatter unit test (TDD)

**Files:**
- Create: `$DS/src/core/gpu_prof.h`
- Test: `$DS/src/core/gpu_prof_test.cpp` (standalone; compiled directly with `clang++`, not wired into CMake — it is a dev test for the pure math)

- [ ] **Step 1: Write the failing test**

Create `$DS/src/core/gpu_prof_test.cpp`:

```cpp
// Standalone unit test for GpuProf::FormatProfLine (Phase 0 measurement spike).
// Build & run:
//   clang++ -std=c++20 -I src src/core/gpu_prof_test.cpp -o /tmp/gpu_prof_test && /tmp/gpu_prof_test
#include "core/gpu_prof.h"

#include <cassert>
#include <cstdio>
#include <string>

int main()
{
  // 60 fps -> 16.67 ms budget; runframe 20 ms is over budget; gpu_frac = 12/20 = 0.60.
  const std::string over = GpuProf::FormatProfLine(8, 600, 60.0, 20.0, 12.0, 8.0);
  assert(over == "[GPU_PROF] scale=8x frame=600 budget_ms=16.67 runframe_ms=20.00 gpu_cmd_ms=12.00 "
                 "present_ms=8.00 gpu_frac=0.60 over_budget=1");

  // 60 fps; runframe 5 ms under budget -> over_budget=0; gpu_frac = 1/5 = 0.20.
  const std::string under = GpuProf::FormatProfLine(1, 60, 60.0, 5.0, 1.0, 0.50);
  assert(under == "[GPU_PROF] scale=1x frame=60 budget_ms=16.67 runframe_ms=5.00 gpu_cmd_ms=1.00 "
                  "present_ms=0.50 gpu_frac=0.20 over_budget=0");

  // fps=0 guarded -> budget 0; runframe 1 ms is "over" budget 0.
  const std::string zero = GpuProf::FormatProfLine(4, 120, 0.0, 1.0, 0.0, 0.0);
  assert(zero == "[GPU_PROF] scale=4x frame=120 budget_ms=0.00 runframe_ms=1.00 gpu_cmd_ms=0.00 "
                 "present_ms=0.00 gpu_frac=0.00 over_budget=1");

  std::printf("gpu_prof_test: OK\n");
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails (no header yet)**

Run:
```bash
cd "$DS" && clang++ -std=c++20 -I src src/core/gpu_prof_test.cpp -o /tmp/gpu_prof_test
```
Expected: FAIL — `fatal error: 'core/gpu_prof.h' file not found`.

- [ ] **Step 3: Write the header**

Create `$DS/src/core/gpu_prof.h`:

```cpp
// SPDX-License-Identifier: CC-BY-NC-ND-4.0
// Temporary GPU profiling for the libretro GPU-video-thread Phase 0 measurement spike.
// Gated by the GPU_PROF env var; off by default (zero added work when unset).
// Remove once Phase 0 concludes.
#pragma once

#include "common/timer.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace GpuProf {

// Per-frame accumulators in nanoseconds. Reset every 60 frames inside EndFrame().
// C++17 inline variables: single definition across all translation units.
inline std::atomic<double> s_runframe_ns{0.0};
inline std::atomic<double> s_gpu_cmd_ns{0.0};
inline std::atomic<double> s_present_ns{0.0};
inline std::atomic<std::uint64_t> s_frames{0};

inline bool Enabled()
{
  static const bool enabled = (std::getenv("GPU_PROF") != nullptr);
  return enabled;
}

inline void Add(std::atomic<double>& acc, double ns)
{
  double cur = acc.load(std::memory_order_relaxed);
  while (!acc.compare_exchange_weak(cur, cur + ns, std::memory_order_relaxed))
  {
  }
}

// Pure formatter — no I/O, no Timer dependency — so it is unit-testable in isolation.
// Inputs are per-frame averages in milliseconds (caller divides the 60-frame accumulator
// by the sample count).
inline std::string FormatProfLine(unsigned scale, std::uint64_t frame, double fps, double runframe_ms,
                                  double gpu_cmd_ms, double present_ms)
{
  const double budget_ms = (fps > 0.0) ? (1000.0 / fps) : 0.0;
  const double gpu_frac = (runframe_ms > 0.0) ? (gpu_cmd_ms / runframe_ms) : 0.0;
  const int over_budget = (runframe_ms > budget_ms) ? 1 : 0;
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "[GPU_PROF] scale=%ux frame=%llu budget_ms=%.2f runframe_ms=%.2f gpu_cmd_ms=%.2f "
                "present_ms=%.2f gpu_frac=%.2f over_budget=%d",
                scale, static_cast<unsigned long long>(frame), budget_ms, runframe_ms, gpu_cmd_ms,
                present_ms, gpu_frac, over_budget);
  return std::string(buf);
}

// Call once per frame from retro_run. Every 60th frame, averages the accumulators,
// prints a [GPU_PROF] line to stderr (lands in /tmp/rn.log under RetroNest), and resets.
inline void EndFrame(unsigned scale, double fps)
{
  const std::uint64_t n = s_frames.fetch_add(1, std::memory_order_relaxed) + 1;
  if ((n % 60) != 0)
    return;

  constexpr double kSamples = 60.0;
  const double runframe_ms = s_runframe_ns.exchange(0.0, std::memory_order_relaxed) / kSamples / 1.0e6;
  const double gpu_cmd_ms = s_gpu_cmd_ns.exchange(0.0, std::memory_order_relaxed) / kSamples / 1.0e6;
  const double present_ms = s_present_ns.exchange(0.0, std::memory_order_relaxed) / kSamples / 1.0e6;

  std::fprintf(stderr, "%s\n", FormatProfLine(scale, n, fps, runframe_ms, gpu_cmd_ms, present_ms).c_str());
  std::fflush(stderr);
}

// RAII timer. Self-gating: if GPU_PROF is unset it reads no clock and adds nothing.
// Safe to place at the top of a hot function; RAII covers every return path.
class ScopedTimer
{
public:
  explicit ScopedTimer(std::atomic<double>& acc)
    : m_acc(acc), m_active(Enabled()), m_start(m_active ? Timer::GetCurrentValue() : 0)
  {
  }
  ~ScopedTimer()
  {
    if (m_active)
      Add(m_acc, Timer::ConvertValueToNanoseconds(Timer::GetCurrentValue() - m_start));
  }
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
  std::atomic<double>& m_acc;
  bool m_active;
  Timer::Value m_start;
};

} // namespace GpuProf
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
cd "$DS" && clang++ -std=c++20 -I src src/core/gpu_prof_test.cpp -o /tmp/gpu_prof_test && /tmp/gpu_prof_test
```
Expected: compiles, links (no `Timer` symbols are odr-used by `FormatProfLine`, so no link of `timer.cpp` is needed), prints `gpu_prof_test: OK`, exit 0.

- [ ] **Step 5: Commit**

```bash
cd "$DS" && git add src/core/gpu_prof.h src/core/gpu_prof_test.cpp
git commit -m "Add env-gated GpuProf instrumentation header (Phase 0 spike)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Wire the three timers into the inline GPU path

**Files:**
- Modify: `$DS/src/core/gpu_backend.cpp` (include + `HandleCommand:383`)
- Modify: `$DS/src/core/video_presenter.cpp` (include + `PresentFrame:1485`)
- Modify: `$DS/src/duckstation-libretro/libretro.cpp` (includes + `retro_run:202`)

- [ ] **Step 1: Instrument `GPUBackend::HandleCommand` (captures all inline GPU command handling)**

In `$DS/src/core/gpu_backend.cpp`, add the include alongside the existing core includes near the top of the file:

```cpp
#include "gpu_prof.h"
```

Then add the scoped timer as the first line of the function body at line 383:

```cpp
void GPUBackend::HandleCommand(const VideoThreadCommand* cmd)
{
  GpuProf::ScopedTimer _prof(GpuProf::s_gpu_cmd_ns);
  switch (cmd->type)
  {
```

- [ ] **Step 2: Instrument `VideoPresenter::PresentFrame` (the present subset)**

In `$DS/src/core/video_presenter.cpp`, add the include near the top of the file:

```cpp
#include "gpu_prof.h"
```

Then add the scoped timer as the first line of the function body at line 1485:

```cpp
bool VideoPresenter::PresentFrame(GPUBackend* backend, u64 present_time)
{
  GpuProf::ScopedTimer _prof(GpuProf::s_present_ns);
  FullscreenUI::UploadAsyncTextures();
```

- [ ] **Step 3: Time `RunFrame` and emit per-frame in `retro_run`**

In `$DS/src/duckstation-libretro/libretro.cpp`, add these includes near the top (alongside the existing `core/...` includes). `core/system.h` is already present (used for `System::RunFrame`); add the others if not already included:

```cpp
#include "core/gpu_prof.h"
#include "core/settings.h"
#include "common/timer.h"
```

Then replace the single `System::RunFrame();` call (currently line 214, keep the existing comment block above it) so it becomes:

```cpp
  // Runs exactly one frame, returning when InterruptExecution() fires at the
  // frame boundary (set by Host::PumpMessagesOnCoreThread). RunFrame() is a
  // single-frame driver added to the fork core; unlike Execute() it does not
  // loop, so it yields back to retro_run every frame.
  const Timer::Value rf_start = GpuProf::Enabled() ? Timer::GetCurrentValue() : 0;
  System::RunFrame();
  if (GpuProf::Enabled())
  {
    GpuProf::Add(GpuProf::s_runframe_ns,
                 Timer::ConvertValueToNanoseconds(Timer::GetCurrentValue() - rf_start));
    GpuProf::EndFrame(static_cast<unsigned>(g_settings.gpu_resolution_scale), System::GetVideoFrameRate());
  }
```

- [ ] **Step 4: Build the core (verifies all three TUs compile + link)**

Run:
```bash
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS" && src/duckstation-libretro/package.sh
```
Expected: builds and deploys the core to the shared cores dir with no compile/link errors. (The `GpuProf` symbols are header-inline; `s_*_ns` are inline variables with a single definition across TUs.)

- [ ] **Step 5: Commit**

```bash
cd "$DS" && git add src/core/gpu_backend.cpp src/core/video_presenter.cpp src/duckstation-libretro/libretro.cpp
git commit -m "Wire GpuProf timers into inline GPU path (Phase 0 spike)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Build RetroNest (x86/Rosetta) against the instrumented core

**Files:** none (build only).

- [ ] **Step 1: Build + deploy RetroNest**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
```
Expected: builds successfully. The `macdeployqt … Cannot resolve rpath libspirv-cross-c-shared` line is a known cosmetic non-fatal warning (the lib is bundled + symlinked in `Contents/Frameworks/`).

- [ ] **Step 2: No commit** (build artifacts only).

---

### Task 4: Run the measurement (USER — TCC blocks the agent from the GUI)

**Goal:** Capture `[GPU_PROF]` lines at 1×, 4×, and 8× internal resolution on a self-running, GPU-active scene.

> **Hand this task to the user.** The agent cannot launch or observe the GUI. The user runs the steps and pastes back the `[GPU_PROF]` lines from `/tmp/rn.log`.

- [ ] **Step 1: Obtain a free self-running PS1 homebrew demo**

Get a no-input, GPU-active PS1 `.exe`/`.ps-exe` (DuckStation boots these with no disc). Recommended: PSn00bSDK `n00bdemo`, or a GPU-heavy demoscene demo from pouet.net.

- [ ] **Step 2: Run at 1× and capture**

In RetroNest's settings (the #3 settings UI), set internal resolution scale to **1×**. Then launch with profiling enabled:
```bash
GPU_PROF=1 /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1
```
Load the demo, let it run ~30 s, quit. Capture the lines:
```bash
grep '\[GPU_PROF\]' /tmp/rn.log | tail -20
```
Expected: lines like `[GPU_PROF] scale=1x frame=... budget_ms=16.67 runframe_ms=... gpu_cmd_ms=... present_ms=... gpu_frac=... over_budget=...`.

- [ ] **Step 3: Repeat at 4×**

Set internal resolution to **4×**, relaunch the same way, load the same demo scene, run ~30 s, capture `grep '\[GPU_PROF\]' /tmp/rn.log | tail -20`.

- [ ] **Step 4: Repeat at 8×**

Set internal resolution to **8×**, relaunch, same scene, ~30 s, capture the lines.

- [ ] **Step 5: Report**

Paste the captured `[GPU_PROF]` lines for all three scales back into the session for Task 5. Acceptance: lines appear at each scale with plausible values (`scale=` matches the chosen setting; `budget_ms` ≈ 16.67 for an NTSC demo; `present_ms` ≤ `gpu_cmd_ms` ≤ `runframe_ms`).

---

### Task 5: Apply the go/no-go gate and write the results doc

**Files:**
- Create: `$DS/docs/gpu-video-thread-phase0-results-2026-06-08.md`

- [ ] **Step 1: Compute the gate from the 8× numbers**

Using the spec's thresholds (§6) on the **8×** averages:
- **GO** if `runframe_ms` > `budget_ms` (i.e. `over_budget=1`) **and** `gpu_frac` ≥ 0.30.
- **MARGINAL** if `runframe_ms` is ~80–100% of `budget_ms` **and** `gpu_frac` ≥ 0.40 (user decides).
- **NO-GO** if `runframe_ms` < ~80% of `budget_ms` (comfortably full speed).

Apply the homebrew-floor caveat (spec §5.4): a GO from a modest demo is trustworthy; a NO-GO is a *floor* — phrase it "measured cheap on available content; worst-case games unverified," not a hard no.

- [ ] **Step 2: Write the results doc**

Create `$DS/docs/gpu-video-thread-phase0-results-2026-06-08.md` with this structure, filling the bracketed values from the captured data:

```markdown
# GPU video thread — Phase 0 measurement results (2026-06-08)

**Demo:** [name + source]   **Run mode:** x86_64 (Rosetta)   **Host fps/budget:** [fps] / [budget_ms] ms

## Captured [GPU_PROF] averages
| scale | runframe_ms | gpu_cmd_ms | present_ms | gpu_frac | over_budget |
|-------|-------------|------------|------------|----------|-------------|
| 1×    | [..]        | [..]       | [..]       | [..]     | [..]        |
| 4×    | [..]        | [..]       | [..]       | [..]     | [..]        |
| 8×    | [..]        | [..]       | [..]       | [..]     | [..]        |

## Cost slope (1×→8×)
[1–2 sentences: does gpu_cmd/present grow steeply with scale, or stay flat?]

## Gate decision
**[GO | MARGINAL | NO-GO]** — [reasoning vs the §6 thresholds, including the homebrew-floor caveat.]

## Next step
[If GO/MARGINAL: brainstorm/spec is done; write the Phases 1–7 implementation plan.
 If NO-GO: stop; record the floor; revisit if a GPU-heavy title is obtained.]
```

- [ ] **Step 3: Commit the results doc**

```bash
cd "$DS" && git add docs/gpu-video-thread-phase0-results-2026-06-08.md
git commit -m "Record GPU video thread Phase 0 measurement results + gate decision

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 4: Branch on the decision**
  - **GO / MARGINAL** → proceed to write the Phases 1–7 implementation plan (separate planning session; spec §7 is the outline). The instrumentation can stay in (env-gated) to measure the after-state.
  - **NO-GO** → stop here. Optionally revert the instrumentation, or leave it env-gated for future re-measurement. Update the handoff/memory with the outcome.

---

## Notes on the instrumentation (for the implementer)

- **Why `HandleCommand`, not the three `PushCommand` variants:** in the inline (`!use_thread`) path all of `PushCommand` / `PushCommandAndWakeThread` / `PushCommandAndSync` dispatch straight to `gpu_backend->HandleCommand(cmd)` (`video_thread.cpp:259–303`). Timing `HandleCommand` once captures every inline command (including the submit/update-display command that triggers present) with a single edit site.
- **`present_ms ⊂ gpu_cmd_ms`:** present runs inside `HandleSubmitFrameCommand`/`HandleUpdateDisplayCommand` → `PresentFrame`, which is reached *through* `HandleCommand`. So `present_ms` is a subset of `gpu_cmd_ms`, reported separately to distinguish present-blocking (drawable acquire / vsync) from command-building.
- **Threaded-mode note:** in a later phase with `use_thread=true`, `HandleCommand` and `PresentFrame` run on the worker thread, so these same timers would then measure worker-thread time — harmless, and useful for the after-measurement. `runframe_ms` always stays on the core thread.
- **No clock when disabled:** `ScopedTimer` and the `retro_run` timing both read the clock only when `GpuProf::Enabled()` (cached `getenv`), so an unset `GPU_PROF` adds nothing measurable.
```
