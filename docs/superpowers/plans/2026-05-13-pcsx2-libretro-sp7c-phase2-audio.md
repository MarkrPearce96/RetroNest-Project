# SP7c Phase 2 Implementation Plan — Audio Card (5 new knobs)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the 5 Audio knobs that actually flow through `LibretroAudioStream` (audio routes through the libretro batch callback, not Cubeb — so Cubeb-only knobs are silently inert and are dropped). Add an Audio card to the libretro hub so the new rows are reachable from the UI.

**Architecture:** Each knob touches five places, mirroring Phase 1's per-knob workflow:
1. New module `pcsx2-libretro/CoreOptionsAudio.{h,cpp}` — owns `kAudioDefinitions[]`, `Audio::Parse`, `Audio::ApplyDefaults`.
2. `pcsx2-libretro/CoreOptions.h` — nest `Audio::Values audio{}` in `Resolved`.
3. `pcsx2-libretro/CoreOptions.cpp` — aggregate `Audio::AppendDefinitions` + `Audio::Parse`; extend `[CoreOptions]` echo log.
4. `pcsx2-libretro/Settings.cpp` — call `Audio::ApplyDefaults` in the per-call user-options block.
5. `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — 5 new `s.append(opt(...))` rows under `category="Audio"`.
6. `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` — add an Audio card next to the Emulation card.

Schema fidelity (byte-for-byte parity between core values and host options) is verified mechanically by `tools/check_schema_fidelity.py`. Round-trip parsing is verified by `tools/test_core_options.cpp` Case 11 (new).

**Tech Stack:**
- pcsx2-libretro: C++20, clang, libretro.h v2 core-options API.
- RetroNest-Project: C++20, Qt 6 (read/append-only on `settingsSchema`).
- Schema check: Python 3 (`tools/check_schema_fidelity.py` exists from Phase 0).
- Build: CMake (`build-arm64` is the dev build; universal lipo + RetroNest copy at close-out).
- Standalone unit test: `clang++` with `-DCORE_OPTIONS_TEST_ONLY`.

**Repo locations:**
- pcsx2-libretro: `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `retronest-libretro`, currently at HEAD `1c4b31d71`).
- RetroNest-Project: `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`, currently at HEAD `6569956`).

**Scope guard:**
- 5 net new core option keys, 5 net new host SettingDef rows. No other categories touched.
- Phase 2 does NOT add Graphics (Phase 4), Memory Cards (Phase 3), nor the Recommended-card reorg (Phase 5). The hub gains exactly one card (Audio).
- pcsx2-libretro fork has NO `origin` remote — close-out ends at `commit + lipo + copy to RetroNest cores dir`. **Do not include `git push origin` anywhere in this plan.**

---

## Knob inventory (5 total — pre-investigated 2026-05-13)

Section `SPU2/Output`. Each row is one core option key + one host SettingDef row.

| Core option key | INI key | Type | Default | Consumer in libretro mode |
|---|---|---|---|---|
| `pcsx2_audio_sync_mode` | `SyncMode`          | Combo (2: `Disabled`, `TimeStretch`) | `"TimeStretch"` | Drives SoundTouch via base `AudioStream::AllocateBuffer` (line 611-622 of `pcsx2/Host/AudioStream.cpp`) — `LibretroAudioStream` inherits |
| `pcsx2_audio_buffer_ms` | `BufferMS`          | Combo (8 stops, 10–200 ms)           | `"50"`          | Sets `m_buffer_size` and `m_target_buffer_size` via `BaseInitialize` (line 410-411) |
| `pcsx2_audio_volume`    | `StandardVolume`    | Combo (9 stops, 0–200 %)             | `"100"`         | `spu2.cpp:108` reads as sample multiplier — applies regardless of backend |
| `pcsx2_audio_ff_volume` | `FastForwardVolume` | Combo (9 stops, 0–200 %)             | `"100"`         | `spu2.cpp:109` — applies during fast-forward |
| `pcsx2_audio_muted`     | `OutputMuted`       | Bool (`enabled`/`disabled`)          | `"disabled"`    | `spu2.cpp:110/167` — mutes libretro batch output |

### Value strings (must match PCSX2's parsers byte-for-byte)

**`SyncMode`** — from `pcsx2/Pcsx2Config.cpp:1180-1183` (`s_spu2_sync_mode_names`):
```
{"Disabled (Noisy)",         "Disabled"}
{"TimeStretch (Recommended)","TimeStretch"}
```
Default `"TimeStretch"` (per `Config.h:969` `DEFAULT_SYNC_MODE`).

**`BufferMS`** — `u16`, default 50 (per `pcsx2/Host/AudioStreamTypes.h:55` `DEFAULT_BUFFER_MS`). Standalone uses a slider (range 10–500, step 10); libretro core options are Combo-only so we expose 8 stops:
```
{"10 ms (lowest latency)",  "10"}
{"20 ms",                   "20"}
{"30 ms",                   "30"}
{"50 ms (default)",         "50"}
{"75 ms",                   "75"}
{"100 ms",                  "100"}
{"150 ms",                  "150"}
{"200 ms",                  "200"}
```

**`StandardVolume` / `FastForwardVolume`** — `u32`, default 100, range 0–200 (per `Config.h:967` `MAX_VOLUME = 200`). 9 stops:
```
{"0% (Muted)", "0"}
{"25%",        "25"}
{"50%",        "50"}
{"75%",        "75"}
{"100% (default)", "100"}
{"125%",       "125"}
{"150%",       "150"}
{"175%",       "175"}
{"200% (max)", "200"}
```

**`OutputMuted`** — bool, standard `enabled`/`disabled` pair (matching Phase 1's bool pattern):
```
{"enabled",  "Enabled"}
{"disabled", "Disabled"}
```

### Knobs explicitly DROPPED (silently inert in libretro mode)

| Standalone candidate | INI key | Why dropped |
|---|---|---|
| `ExpansionMode` | `SPU2/Output/ExpansionMode` | Forced `Disabled` by `Settings.cpp:232` because libretro `audio_batch_cb` is stereo-only; `LibretroAudioStream` ctor asserts `expansion_mode == Disabled`. User toggle would be silently overridden. |
| `OutputLatencyMS` | StreamParameters `output_latency_ms` | Logged at `AudioStream.cpp:117` but otherwise consumed only by Cubeb/SDL paths — not by `BaseInitialize`, not by `LibretroAudioStream`. Frontend (RetroNest) owns presentation timing. |
| `OutputLatencyMinimal` | StreamParameters `minimal_output_latency` | Same as above — Cubeb-only. |
| `Backend` | `SPU2/Output/Backend` | Hardcoded `Libretro` by `Settings.cpp:226`; SP4 baseline. |
| `DriverName` / `DeviceName` | StreamParameters | Cubeb-only backend selector. |
| `StretchSequenceLengthMS` (memory's 6th candidate) | StreamParameters `stretch_sequence_length_ms` | Not exposed in standalone `pcsx2_adapter.cpp` Audio rows — for parity, drop. Phase 4+ may revisit if standalone adds it. |

These drops are documented in the spec's Phase 2 Decision 1; do not re-add without a spec amendment.

---

## File structure

### Created in this phase

| File | Purpose |
|---|---|
| `pcsx2-libretro/CoreOptionsAudio.h` | Mirrors `CoreOptionsEmulation.h`'s shape — `struct Values`, `AppendDefinitions`, `Parse`, `ApplyDefaults`. |
| `pcsx2-libretro/CoreOptionsAudio.cpp` | Mirrors `CoreOptionsEmulation.cpp`'s shape — 5 literal `out.push_back({...})` blocks, `Parse` with string/int/bool branches, `ApplyDefaults` gated by `#ifndef CORE_OPTIONS_TEST_ONLY`. |

### Modified in this phase

| File | Action |
|---|---|
| `pcsx2-libretro/CoreOptions.h` | Add `Audio::Values audio{}` field to `struct Resolved`; `#include "CoreOptionsAudio.h"`. |
| `pcsx2-libretro/CoreOptions.cpp` | Add `#include "CoreOptionsAudio.h"`; call `Audio::AppendDefinitions(v)` and `Audio::Parse(cb, r.audio)`; extend the 4-line `[CoreOptions]` echo log to 5 lines (add audio line). |
| `pcsx2-libretro/Settings.cpp` | In the per-call user-options block (the one after `g_initialized`), call `Audio::ApplyDefaults(g_si, options ? options->audio : Audio::Values{})`. |
| `pcsx2-libretro/CMakeLists.txt` | Append `CoreOptionsAudio.cpp` to the pcsx2_libretro target sources. |
| `pcsx2-libretro/tools/test_core_options.cpp` | Add Case 11 (Audio round-trip); update header comment with `../CoreOptionsAudio.cpp` in the compile example. |
| `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` | Append 5 `s.append(opt(...))` rows under `category="Audio"`. |
| `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` | Add an Audio `makeCard(...)` next to the Emulation card. |

### Untouched in this phase

- `pcsx2-libretro/CoreOptionsEmulation.{h,cpp}` — no changes; Phase 1's 15 knobs stay intact.
- `pcsx2-libretro/LibretroAudioStream.{h,cpp}` — no changes; audio routing was SP4 and is stable.
- `pcsx2-libretro/tools/check_schema_fidelity.py` — Phase 0's regexes already accept multiple `CoreOptions*.cpp` source files via glob; no script changes.
- All other RetroNest-Project files (dialog, generic settings page, etc.).

---

## Task 0: Baseline current state

**Files:** none.

- [ ] **Step 1: Confirm clean working tree on pcsx2-libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git status --short
git log --oneline -3
```
Expected: working tree clean (or only `?? pcsx2-libretro/tools/__pycache__/` + test binaries untracked). HEAD at `1c4b31d71` ("SP7c Phase 1 followup: echo all 18 resolved values in ReadResolved log") on `retronest-libretro`.

If anything is dirty, stop and resolve before proceeding.

- [ ] **Step 2: Confirm clean working tree on RetroNest-Project**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git status --short
git log --oneline -3
```
Expected: HEAD at `6569956` ("docs: mark SP7c Phase 1 ✅ shipped + record smoke-time fixes") on `main`. Only `?? cpp/build-*` untracked is fine.

- [ ] **Step 3: Baseline-pass the existing test (Phase 1 state)**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected output ends with:
```
0 failure(s)
```

- [ ] **Step 4: Baseline-pass the schema fidelity check**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```

Expected output ends with:
```
Schema fidelity OK: 18 core keys, 18 host keys, byte-for-byte match.
```

- [ ] **Step 5: Confirm pcsx2_libretro target builds**

Run:
```bash
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`.

- [ ] **Step 6: No commit. Baseline only.**

---

## Task 1: Scaffold the CoreOptionsAudio module (empty hooks)

This task creates the module skeleton with empty `AppendDefinitions` / `Parse` / `ApplyDefaults` functions, wires it into the aggregator and CMake, and confirms the existing test still passes. No new knobs yet — verifies the plumbing.

**Files:**
- Create: `pcsx2-libretro/CoreOptionsAudio.h`
- Create: `pcsx2-libretro/CoreOptionsAudio.cpp`
- Modify: `pcsx2-libretro/CoreOptions.h`
- Modify: `pcsx2-libretro/CoreOptions.cpp`
- Modify: `pcsx2-libretro/Settings.cpp`
- Modify: `pcsx2-libretro/CMakeLists.txt`
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (header-comment line only)

### 1.1 — Create the header

- [ ] **Step 1: Create `pcsx2-libretro/CoreOptionsAudio.h`**

Write the following to `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptionsAudio.h`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 2: Audio-category core options.
//
// Owns the kAudioDefinitions[] slice of the master core-options table.
// CoreOptions.cpp aggregates this module's slice (plus Emulation's slice
// from Phase 0/1) into the single table dispatched via SET_CORE_OPTIONS_V2.

#pragma once

#include "libretro.h"

#include <string>
#include <vector>

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }
class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions::Audio
{

// Per-category resolved values. Aggregated by struct Resolved.
//
// Defaults preserve the upstream PCSX2 defaults (SPU2Options ctor +
// AudioStreamParameters defaults) so a missing/empty options.json
// produces identical behavior to today's SP4 baseline.
struct Values
{
    // SPU2/Output/SyncMode — enum string (Pcsx2Config.cpp:1180 s_spu2_sync_mode_names).
    // PCSX2 reads this via SettingsWrapParsedEnum which calls ParseSyncMode;
    // exact-case names "Disabled" / "TimeStretch" are required.
    std::string sync_mode = "TimeStretch";

    // SPU2/Output/BufferMS — ring-buffer size in milliseconds.
    // Default 50 per AudioStreamTypes.h:55 DEFAULT_BUFFER_MS.
    // Consumed by base AudioStream::BaseInitialize → m_buffer_size /
    // m_target_buffer_size (AudioStream.cpp:410-411).
    int buffer_ms = 50;

    // SPU2/Output/StandardVolume — normal-play volume 0..200.
    // spu2.cpp:108 reads as a sample multiplier each retro_run.
    int volume = 100;

    // SPU2/Output/FastForwardVolume — volume during fast-forward 0..200.
    // spu2.cpp:109.
    int ff_volume = 100;

    // SPU2/Output/OutputMuted — global mute. spu2.cpp:110/167.
    bool muted = false;
};

// Append this category's option definitions to the master vector.
// Called once from CoreOptions::BuildDefinitions on first emit.
// Does NOT append the libretro terminator — the master aggregator does that.
void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out);

// Read this category's resolved values from the host. Called from
// CoreOptions::ReadResolved.
void Parse(retro_environment_t cb, Values& out);

// Apply this category's resolved values to the settings interface.
// Called from Pcsx2Libretro::Settings::InitializeDefaults's per-call
// user-options block.
void ApplyDefaults(MemorySettingsInterface& si, const Values& v);

} // namespace Pcsx2Libretro::CoreOptions::Audio
```

### 1.2 — Create the empty cpp

- [ ] **Step 2: Create `pcsx2-libretro/CoreOptionsAudio.cpp` with empty function bodies**

Write the following to `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptionsAudio.cpp`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsAudio.h"

#ifdef CORE_OPTIONS_TEST_ONLY
#include <cstdarg>
#include <cstdio>
static void FrontendLog(int /*level*/, const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
}
#else
#include "LibretroFrontend.h"                  // FrontendLog
#include "common/MemorySettingsInterface.h"    // MemorySettingsInterface
#endif

#include <cstdlib>
#include <cstring>

namespace Pcsx2Libretro::CoreOptions::Audio
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& /*out*/)
{
    // SP7c Phase 2 Task 2 fills this in with 5 literal out.push_back({...})
    // blocks. Empty in Task 1 to verify the aggregator plumbing first.
}

void Parse(retro_environment_t /*cb*/, Values& /*out*/)
{
    // SP7c Phase 2 Task 2 fills this in with 5 parse branches.
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& /*si*/, const Values& /*v*/)
{
    // SP7c Phase 2 Task 2 fills this in with 5 si.SetXValue(...) lines.
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Audio
```

### 1.3 — Wire into the aggregator header

- [ ] **Step 3: Update `pcsx2-libretro/CoreOptions.h` — add Audio include and field**

Open `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions.h`. After line 20 (`#include "CoreOptionsEmulation.h"`), add:

```cpp
#include "CoreOptionsAudio.h"
```

In `struct Resolved` (currently lines 27-34), insert a new line after the `emulation{};` field:

```cpp
struct Resolved
{
    Pcsx2Libretro::CoreOptions::Emulation::Values emulation{};
    Pcsx2Libretro::CoreOptions::Audio::Values     audio{};
    // Future phases append:
    //   Pcsx2Libretro::CoreOptions::Graphics::Values    graphics{};
    //   Pcsx2Libretro::CoreOptions::MemoryCards::Values memory_cards{};
};
```

### 1.4 — Wire into the aggregator cpp

- [ ] **Step 4: Update `pcsx2-libretro/CoreOptions.cpp` — include + AppendDefinitions + Parse call**

Open `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions.cpp`. After line 5 (`#include "CoreOptionsEmulation.h"`), add:

```cpp
#include "CoreOptionsAudio.h"
```

In `BuildDefinitions()` (currently lines 27-47), after the existing `Emulation::AppendDefinitions(v);` call (currently line 37), add:

```cpp
        Audio::AppendDefinitions(v);
```

And bump the pre-reserve from `8` to `16` (currently line 36) to accommodate Phase 2's growth without re-allocation:

```cpp
        v.reserve(16);  // 18 Phase 1 + 5 Phase 2 + terminator ≈ 24
```

In `ReadResolved()` (currently lines 74-114), after `Emulation::Parse(cb, r.emulation);` (currently line 79), add:

```cpp
    Audio::Parse(cb, r.audio);
```

Update the trailing comment about future phases to remove "Audio::Parse" (since we just added it):

```cpp
    // Future phases append Graphics::Parse, MemoryCards::Parse here.
```

The echo log expansion happens in Task 2 (once Audio actually has values to echo). Leave the 4-line log block as-is for now.

### 1.5 — Wire into Settings.cpp

- [ ] **Step 5: Update `pcsx2-libretro/Settings.cpp` — include + per-call ApplyDefaults**

Open `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/Settings.cpp`. After line 9 (`#include "CoreOptionsEmulation.h"`), add:

```cpp
#include "CoreOptionsAudio.h"
```

In `InitializeDefaults()`, in the per-call user-options block (currently lines 305-309), expand to include Audio:

```cpp
    {
        const CoreOptions::Emulation::Values em_defaults{};
        CoreOptions::Emulation::ApplyDefaults(
            g_si, options ? options->emulation : em_defaults);

        const CoreOptions::Audio::Values audio_defaults{};
        CoreOptions::Audio::ApplyDefaults(
            g_si, options ? options->audio : audio_defaults);
    }
```

This goes BEFORE `VMManager::Internal::LoadStartupSettings();` (currently line 314) — so the user's audio tweaks land in `g_si` before PCSX2 propagates the layered settings into the live `Pcsx2Config`.

### 1.6 — Add to CMake

- [ ] **Step 6: Update `pcsx2-libretro/CMakeLists.txt`**

Open `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CMakeLists.txt`. Find the line containing `CoreOptionsEmulation.cpp` (around line 20). Append `CoreOptionsAudio.cpp` on the next line — keep alphabetical-ish grouping with the Emulation entry:

```cmake
    CoreOptions.cpp
    CoreOptionsAudio.cpp
    CoreOptionsEmulation.cpp
```

(Order doesn't affect link semantics — just keep the per-phase modules visually grouped.)

### 1.7 — Update test compile-command comment

- [ ] **Step 7: Update `test_core_options.cpp` header comment**

Open `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_core_options.cpp`. Replace the compile-example block in the header (lines 7-11) with:

```cpp
//   cd pcsx2-libretro/tools
//   clang++ -std=c++20 -I.. test_core_options.cpp \
//       ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
//       -DCORE_OPTIONS_TEST_ONLY -o test_core_options
//   ./test_core_options
```

### 1.8 — Verify build + test still passes

- [ ] **Step 8: Rebuild standalone test with the new module**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: `0 failure(s)`. All 10+1 existing cases still pass — Audio module is wired but empty, so Case 7's structural sweep doesn't see any new entries.

- [ ] **Step 9: Rebuild pcsx2_libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -10
```

Expected: `[100%] Built target pcsx2_libretro`. CMake should pick up the new `CoreOptionsAudio.cpp` automatically (CMakeLists changed → regenerate).

If CMake reports "file not found" for the new module, check that the path matches the existing `CoreOptionsEmulation.cpp` entry exactly (no leading slash, no `pcsx2-libretro/` prefix — the entry is relative to the directory containing this `CMakeLists.txt`).

- [ ] **Step 10: Schema fidelity check still passes (no new keys yet)**

Run:
```bash
cmake --build build-arm64 --target check_schema_fidelity
```

Expected:
```
Schema fidelity OK: 18 core keys, 18 host keys, byte-for-byte match.
```

The empty `Audio::AppendDefinitions` contributes zero new core keys, so the count is unchanged.

### 1.9 — Commit the scaffold

- [ ] **Step 11: Commit the Audio module scaffold (core-side only)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsAudio.h \
        pcsx2-libretro/CoreOptionsAudio.cpp \
        pcsx2-libretro/CoreOptions.h \
        pcsx2-libretro/CoreOptions.cpp \
        pcsx2-libretro/Settings.cpp \
        pcsx2-libretro/CMakeLists.txt \
        pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 2 Task 1: scaffold CoreOptionsAudio module

Adds an empty CoreOptionsAudio.{h,cpp} module mirroring
CoreOptionsEmulation's shape (Values struct, AppendDefinitions, Parse,
ApplyDefaults). Wires it into the CoreOptions aggregator, the
Settings::InitializeDefaults per-call block, and CMakeLists.

No new user-visible knobs yet — Task 2 fills in the 5 Audio
definitions. This commit verifies the plumbing (build + test + schema
fidelity all still green at 18 keys) before the data-entry pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Implement the 5 Audio knobs (core side)

Fills in `AppendDefinitions`, `Parse`, and `ApplyDefaults` with the 5 knobs, extends the `[CoreOptions]` echo log, and adds Case 11 to the standalone test.

**Files:**
- Modify: `pcsx2-libretro/CoreOptionsAudio.cpp`
- Modify: `pcsx2-libretro/CoreOptions.cpp` (echo log line)
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (Case 11)

### 2.1 — Populate AppendDefinitions

- [ ] **Step 1: Replace the empty `AppendDefinitions` body in `CoreOptionsAudio.cpp`**

Replace the entire `AppendDefinitions` function body (Task 1's empty stub) with 5 literal `out.push_back({...})` blocks. Each block uses the same shape as Phase 1's Emulation entries — `CORE_BLOCK_RE` in `check_schema_fidelity.py` only parses literal brace-init lists (NOT lambda emissions), so no helper lambdas:

```cpp
void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // SP7c Phase 2 — Audio card (5 knobs under SPU2/Output).
    //
    // Why these 5 and not the standalone's 11: audio routes through
    // LibretroAudioStream (SP4), so Cubeb-only knobs (Backend, DriverName,
    // DeviceName, ExpansionMode, OutputLatencyMS, OutputLatencyMinimal) are
    // silently inert and dropped. The 5 retained knobs each have a verified
    // consumer in libretro mode — see plan section "Knob inventory" for the
    // line-level cross-reference.
    //
    // Each entry is a literal out.push_back({...}) block (no lambda helper)
    // so tools/check_schema_fidelity.py's CORE_BLOCK_RE recognizes them.
    out.push_back({
        "pcsx2_audio_sync_mode",
        "Audio Sync Mode",
        nullptr,
        "How emulated audio is paced against host audio. TimeStretch "
        "resamples audio (via SoundTouch) so pitch stays correct when "
        "emulation speed differs from 100%. Disabled passes raw samples "
        "through — fastest but produces audible artefacts during any "
        "speed deviation.",
        nullptr,
        nullptr,
        {
            { "Disabled",    "Disabled (Noisy)" },
            { "TimeStretch", "TimeStretch (Recommended)" },
            { nullptr,       nullptr },
        },
        "TimeStretch",
    });

    out.push_back({
        "pcsx2_audio_buffer_ms",
        "Audio Buffer Size",
        nullptr,
        "Ring-buffer size for emulated audio in milliseconds. Smaller "
        "values reduce audio latency at the cost of higher CPU pressure "
        "and a greater chance of underruns (crackling). 50 ms is the "
        "PCSX2 default; 30 ms is a reasonable low-latency target on a "
        "machine that can sustain it.",
        nullptr,
        nullptr,
        {
            { "10",  "10 ms (lowest latency)" },
            { "20",  "20 ms" },
            { "30",  "30 ms" },
            { "50",  "50 ms (default)" },
            { "75",  "75 ms" },
            { "100", "100 ms" },
            { "150", "150 ms" },
            { "200", "200 ms" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_audio_volume",
        "Volume",
        nullptr,
        "Normal-play audio volume as a percentage. 100% is the PS2's "
        "native output level. Values above 100% boost the signal "
        "digitally (may clip on loud passages).",
        nullptr,
        nullptr,
        {
            { "0",   "0% (Muted)" },
            { "25",  "25%" },
            { "50",  "50%" },
            { "75",  "75%" },
            { "100", "100% (default)" },
            { "125", "125%" },
            { "150", "150%" },
            { "175", "175%" },
            { "200", "200% (max)" },
            { nullptr, nullptr },
        },
        "100",
    });

    out.push_back({
        "pcsx2_audio_ff_volume",
        "Fast-Forward Volume",
        nullptr,
        "Volume during fast-forward as a percentage. Independent from "
        "the normal-play volume — useful for muting audio entirely "
        "during fast-forward without affecting regular playback.",
        nullptr,
        nullptr,
        {
            { "0",   "0% (Muted)" },
            { "25",  "25%" },
            { "50",  "50%" },
            { "75",  "75%" },
            { "100", "100% (default)" },
            { "125", "125%" },
            { "150", "150%" },
            { "175", "175%" },
            { "200", "200% (max)" },
            { nullptr, nullptr },
        },
        "100",
    });

    out.push_back({
        "pcsx2_audio_muted",
        "Mute Audio",
        nullptr,
        "Mute all PS2 audio output. RetroNest's UI sounds and any other "
        "non-PS2 audio sources are unaffected.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });
}
```

### 2.2 — Populate Parse

- [ ] **Step 2: Replace the empty `Parse` body in `CoreOptionsAudio.cpp`**

Replace the empty `Parse` stub with:

```cpp
void Parse(retro_environment_t cb, Values& out)
{
    if (!cb) return;

    auto query = [&cb](const char* key) -> const char* {
        retro_variable var{};
        var.key = key;
        if (cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
            return var.value;
        return nullptr;
    };

    // SyncMode: validate against the two PCSX2-accepted strings.
    // Anything else (e.g. an older "ASync" value left in a stale
    // options.json) falls back to the struct default and WARNs —
    // PCSX2's ParseSyncMode would otherwise reject the value and the
    // INI write would be a no-op.
    if (const char* v = query("pcsx2_audio_sync_mode")) {
        if (std::strcmp(v, "Disabled") == 0 || std::strcmp(v, "TimeStretch") == 0) {
            out.sync_mode = v;
        } else {
            FrontendLog(RETRO_LOG_WARN,
                "[CoreOptions] Unknown audio sync_mode '%s'; defaulting to TimeStretch", v);
            out.sync_mode = "TimeStretch";
        }
    }

    // Numeric knobs use the same parse_int helper pattern as Phase 1's
    // ee_cycle_rate / vsync_queue_size — strtol with fallback to the
    // struct default on unparseable input.
    auto parse_int = [&query](const char* key, int& out_field, int fallback) {
        if (const char* v = query(key)) {
            char* end = nullptr;
            const long parsed = std::strtol(v, &end, 10);
            if (end == v) {
                FrontendLog(RETRO_LOG_WARN,
                    "[CoreOptions] Unparseable %s='%s'; keeping default %d",
                    key, v, fallback);
                out_field = fallback;
            } else {
                out_field = static_cast<int>(parsed);
            }
        }
    };

    parse_int("pcsx2_audio_buffer_ms", out.buffer_ms, 50);
    parse_int("pcsx2_audio_volume",    out.volume,    100);
    parse_int("pcsx2_audio_ff_volume", out.ff_volume, 100);

    if (const char* v = query("pcsx2_audio_muted"))
        out.muted = (std::strcmp(v, "enabled") == 0);
}
```

### 2.3 — Populate ApplyDefaults

- [ ] **Step 3: Replace the empty `ApplyDefaults` body in `CoreOptionsAudio.cpp`**

Replace the empty `ApplyDefaults` stub (still inside the `#ifndef CORE_OPTIONS_TEST_ONLY` block) with:

```cpp
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // All 5 keys live under PCSX2's SPU2/Output INI section.
    // SetStringValue routes through MemorySettingsInterface, which is
    // PCSX2's base settings layer (set up by Settings::InitializeDefaults).
    // Pcsx2Config::SPU2Options::LoadSave (Pcsx2Config.cpp:1258-1267) reads
    // these via SettingsWrapEntry / SettingsWrapParsedEnum at VMManager
    // refresh time — LoadStartupSettings() runs after this in
    // InitializeDefaults so the new values take effect on this launch.
    si.SetStringValue("SPU2/Output", "SyncMode", v.sync_mode.c_str());
    si.SetIntValue   ("SPU2/Output", "BufferMS",          v.buffer_ms);
    si.SetIntValue   ("SPU2/Output", "StandardVolume",    v.volume);
    si.SetIntValue   ("SPU2/Output", "FastForwardVolume", v.ff_volume);
    si.SetBoolValue  ("SPU2/Output", "OutputMuted",       v.muted);
}
```

### 2.4 — Extend the echo log

- [ ] **Step 4: Add a 5th echo line in `CoreOptions.cpp::ReadResolved`**

Open `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions.cpp`. After the existing 4th `FrontendLog(...)` call in `ReadResolved` (the "pacing:" line currently lines 104-111), before the `return r;` (currently line 113), add:

```cpp
    const auto& a = r.audio;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] audio: sync_mode=%s buffer_ms=%d volume=%d "
        "ff_volume=%d muted=%s",
        a.sync_mode.c_str(), a.buffer_ms, a.volume, a.ff_volume,
        a.muted ? "on" : "off");
```

### 2.5 — Add Case 11 to the test

- [ ] **Step 5: Add Case 11 — Audio round-trip — to `test_core_options.cpp`**

Open `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_core_options.cpp`. After Case 10b's closing block (currently ends at line 327, the `check_int("Case 10b garbled vsync_queue_size → default 2", ...)` line), before `std::printf("\n%d failure(s)\n", failures);` (currently line 329), insert:

```cpp
    // -------- Case 11: Audio round-trip --------
    //
    // SP7c Phase 2 representative test for the Audio card. Sets all 5
    // Audio knobs to non-defaults, parses, asserts each field reflects
    // the env-var value. Mirrors Case 9 / Case 10's pattern.
    fake::reset();
    fake::variables["pcsx2_audio_sync_mode"] = "Disabled";
    fake::variables["pcsx2_audio_buffer_ms"] = "30";
    fake::variables["pcsx2_audio_volume"]    = "75";
    fake::variables["pcsx2_audio_ff_volume"] = "0";
    fake::variables["pcsx2_audio_muted"]     = "enabled";

    r = ReadResolved(&fake_env_cb);
    {
        const bool sm_ok = r.audio.sync_mode == "Disabled";
        check_bool("Case 11 sync_mode=Disabled", sm_ok, true);
    }
    check_int ("Case 11 buffer_ms=30",   r.audio.buffer_ms, 30);
    check_int ("Case 11 volume=75",      r.audio.volume,    75);
    check_int ("Case 11 ff_volume=0",    r.audio.ff_volume, 0);
    check_bool("Case 11 muted=on",       r.audio.muted,     true);

    // Unknown sync_mode string falls back to default "TimeStretch" + WARN.
    // Captures a regression scenario: a stale options.json from an
    // older core build with a value that PCSX2's ParseSyncMode would
    // reject. Without the explicit validation in Audio::Parse, the
    // string would flow into g_si unchanged and PCSX2's SettingsWrap
    // would silently reject it at SPU2 load time (hard to debug from
    // an empty log).
    fake::reset();
    fake::variables["pcsx2_audio_sync_mode"] = "ASync";
    r = ReadResolved(&fake_env_cb);
    {
        const bool sm_default = r.audio.sync_mode == "TimeStretch";
        check_bool("Case 11 unknown sync_mode → TimeStretch", sm_default, true);
    }
```

### 2.6 — Test, build, schema check

- [ ] **Step 6: Rebuild and run the standalone test**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: `0 failure(s)`. Cases 1–10b unchanged; Case 11 adds 7 sub-assertions (5 knob assertions + 2 fallback assertions).

If Case 7's structural sweep flags new entries, the Audio module's 5 entries pass automatically (they all have non-null key/desc/default + non-empty values[] + default appears in values[]).

- [ ] **Step 7: Rebuild pcsx2_libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`. Clangd diagnostics about PCSX2-internal headers can be ignored (recurring false positives from Phase 0/1 lessons).

- [ ] **Step 8: Run schema-fidelity check — EXPECT FAILURE**

Run:
```bash
cmake --build build-arm64 --target check_schema_fidelity
```

Expected: exit code 1 with output mentioning the 5 new core-only keys:
```
SCHEMA DRIFT DETECTED:
  - core declares key 'pcsx2_audio_sync_mode' with no host row
  - core declares key 'pcsx2_audio_buffer_ms' with no host row
  - core declares key 'pcsx2_audio_volume' with no host row
  - core declares key 'pcsx2_audio_ff_volume' with no host row
  - core declares key 'pcsx2_audio_muted' with no host row

5 drift entries; check both sides match exactly.
```

This is the expected intermediate state — host adapter hasn't been updated yet. We commit core-side and then update host (Task 3).

### 2.7 — Commit

- [ ] **Step 9: Commit the core-side Audio knobs**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsAudio.cpp \
        pcsx2-libretro/CoreOptions.cpp \
        pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 2 Task 2 (core): Audio knobs

Adds 5 knobs to the new Audio card under SPU2/Output:
SyncMode / BufferMS / StandardVolume / FastForwardVolume / OutputMuted.
SyncMode is a 2-entry combo matching PCSX2's s_spu2_sync_mode_names
exactly (Disabled / TimeStretch); volumes are 9-stop combos (0..200%);
BufferMS is an 8-stop combo (10..200 ms); OutputMuted is the standard
enabled/disabled bool.

Cubeb-only candidates (Backend, ExpansionMode, OutputLatencyMS,
OutputLatencyMinimal, DriverName, DeviceName) are dropped — they are
silently inert in libretro mode because audio routes through
LibretroAudioStream, not Cubeb. ExpansionMode is additionally forced
to Disabled by Settings.cpp:232 because the libretro batch callback
is stereo-only.

Parse validates SyncMode against the two PCSX2-accepted strings and
WARNs + falls back to TimeStretch on unknown values — captures the
"stale options.json with an older spelling" scenario before it reaches
PCSX2's SettingsWrap.

ReadResolved's echo log gains a 5th "[CoreOptions] audio: ..." line
so smoke testing can verify each knob's value reached the core.

test_core_options.cpp Case 11 covers Audio round-trip plus the
unknown-sync-mode fallback.

Schema-fidelity check will report drift against the host adapter until
the matching host PR lands; that's expected.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Host-side adapter — append 5 Audio rows

Mirrors the 5 core options into `Pcsx2LibretroAdapter::settingsSchema()` as `Storage::LibretroOption` rows under `category="Audio"`.

**Files:**
- Modify: `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`

- [ ] **Step 1: Open `pcsx2_libretro_adapter.cpp` and append Audio rows**

After the Frame Pacing block added in Phase 1 (currently ends at line 277 — the `pcsx2_skip_duplicate_frames` `s.append(opt(...))` call), before `return s;` (currently line 279), insert:

```cpp
    // SP7c Phase 2 — Audio card.
    //
    // 5 rows under category="Audio". Audio routes through
    // LibretroAudioStream (SP4), so Cubeb-only knobs from the standalone
    // dialog (Backend / DriverName / DeviceName / ExpansionMode /
    // OutputLatencyMS / OutputLatencyMinimal) are dropped — they are
    // silently inert in libretro mode. ExpansionMode is additionally
    // forced to Disabled by the core's Settings.cpp because the libretro
    // audio_batch_cb is stereo-only.
    //
    // Value strings MUST match the core's CoreOptionsAudio.cpp byte-for-byte
    // (and the SyncMode strings must match PCSX2's ParseSyncMode). The
    // check_schema_fidelity.py target verifies this mechanically.
    s.append(opt(
        "Audio", "Configuration",
        "pcsx2_audio_sync_mode", "Audio Sync Mode", "TimeStretch",
        {{"Disabled (Noisy)",         "Disabled"},
         {"TimeStretch (Recommended)","TimeStretch"}},
        "How emulated audio is paced against host audio. TimeStretch "
        "resamples audio (via SoundTouch) so pitch stays correct when "
        "emulation speed differs from 100%. Disabled passes raw samples "
        "through — fastest but produces audible artefacts during any "
        "speed deviation. Takes effect on next launch."));

    s.append(opt(
        "Audio", "Configuration",
        "pcsx2_audio_buffer_ms", "Audio Buffer Size", "50",
        {{"10 ms (lowest latency)", "10"},
         {"20 ms",                  "20"},
         {"30 ms",                  "30"},
         {"50 ms (default)",        "50"},
         {"75 ms",                  "75"},
         {"100 ms",                 "100"},
         {"150 ms",                 "150"},
         {"200 ms",                 "200"}},
        "Ring-buffer size for emulated audio in milliseconds. Smaller "
        "values reduce audio latency at the cost of higher CPU pressure "
        "and a greater chance of underruns. Takes effect on next launch."));

    s.append(opt(
        "Audio", "Controls",
        "pcsx2_audio_volume", "Volume", "100",
        {{"0% (Muted)",       "0"},
         {"25%",              "25"},
         {"50%",              "50"},
         {"75%",              "75"},
         {"100% (default)",   "100"},
         {"125%",             "125"},
         {"150%",             "150"},
         {"175%",             "175"},
         {"200% (max)",       "200"}},
        "Normal-play audio volume. 100% is the PS2's native output level. "
        "Values above 100% boost the signal digitally (may clip on loud "
        "passages). Takes effect on next launch."));

    s.append(opt(
        "Audio", "Controls",
        "pcsx2_audio_ff_volume", "Fast-Forward Volume", "100",
        {{"0% (Muted)",       "0"},
         {"25%",              "25"},
         {"50%",              "50"},
         {"75%",              "75"},
         {"100% (default)",   "100"},
         {"125%",             "125"},
         {"150%",             "150"},
         {"175%",             "175"},
         {"200% (max)",       "200"}},
        "Volume during fast-forward. Independent from normal-play volume — "
        "useful for muting audio entirely during fast-forward. Takes "
        "effect on next launch."));

    s.append(opt(
        "Audio", "Controls",
        "pcsx2_audio_muted", "Mute Audio", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Mute all PS2 audio output. RetroNest's UI sounds and other "
        "non-PS2 audio sources are unaffected. Takes effect on next launch."));
```

- [ ] **Step 2: Schema fidelity check — expect PASS**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```

Expected:
```
Schema fidelity OK: 23 core keys, 23 host keys, byte-for-byte match.
```

If drift is reported, fix the offending side until the match is exact. Common failure modes:
- Volume label typo — "200% (max)" vs "200% (Max)". Match the core string EXACTLY.
- SyncMode display label drift — must be "Disabled (Noisy)" not "Disabled (noisy)".

- [ ] **Step 3: Build RetroNest-Project**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 4: Commit host-side Audio rows**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 2 Task 3 (host): Audio card rows

Adds 5 SettingDef rows under category="Audio" via LibretroOption
storage: SyncMode / BufferMS / StandardVolume / FastForwardVolume /
OutputMuted. Groups: SyncMode + BufferMS under "Configuration";
volumes + muted under "Controls" (mirrors the standalone PCSX2 Audio
dialog's sub-section layout).

Cubeb-only standalone rows (Backend, DriverName, DeviceName,
ExpansionMode, OutputLatencyMS, OutputLatencyMinimal) are not mirrored
— they would be silently inert against pcsx2-libretro's
LibretroAudioStream-routed audio. See the Phase 2 plan for the
case-by-case rationale.

Keys + values match pcsx2-libretro/CoreOptionsAudio.cpp byte-for-byte
(verified by check_schema_fidelity).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Host-side hub — add the Audio card

Adds the Audio `makeCard(...)` to `Pcsx2LibretroCategoryHub` so the new rows are reachable from the UI. **In-phase rule from Phase 1 smoke**: every phase that adds rows under a new category MUST add the matching hub card in the same phase. Don't defer.

**Files:**
- Modify: `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp`

- [ ] **Step 1: Add the Audio card**

Open `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp`. After the Emulation card (currently lines 22-25), before `contentLayout()->addLayout(grid);` (currently line 27), insert:

```cpp
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Volume, mute, buffer, sync mode",
                             countSettings("Audio"), "Audio"),
                    1, 1);
```

The "🔊" speaker emoji (`U+1F50A`) matches the standalone PCSX2 hub's Audio-card iconography. Grid position `(1, 1)` places it to the right of the Emulation card (`(1, 0)`).

Also update the comment at the top of the function to reflect the new card count. Replace lines 13-16:

```cpp
    // SP7b's three knobs (renderer / MTVU / FastBoot) sit under
    // category="Recommended"; SP7c Phase 1 adds 15 rows under
    // category="Emulation". Phase 2 (Audio) + Phase 3 (Memory Cards) +
    // Phase 5 (full hub reorg per the spec) will add the remaining cards.
```

with:

```cpp
    // SP7b's three knobs (renderer / MTVU / FastBoot) sit under
    // category="Recommended"; SP7c Phase 1 added 15 rows under
    // category="Emulation"; SP7c Phase 2 adds 5 rows under
    // category="Audio". Phase 3 (Memory Cards) + Phase 5 (full hub
    // reorg per the spec) will add the remaining cards.
```

- [ ] **Step 2: Build RetroNest-Project**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 3: Commit host-side hub card**

```bash
git add cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 2 Task 4 (host): add Audio card to libretro hub

Pcsx2LibretroCategoryHub now renders 3 cards: Recommended (3 knobs),
Emulation (15 knobs), Audio (5 knobs). Card placed at grid (1, 1) —
right of Emulation — with the speaker emoji icon, mirroring the
standalone PCSX2 hub's iconography.

This follows the in-phase-hub-card rule established after Phase 1's
smoke caught the original deferred-hub-expansion mistake: every phase
that adds rows under a new category MUST add the matching hub card
in the same phase.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Universal build + RetroNest copy

The arm64 build is sufficient for development; the user's RetroNest install is universal (arm64 + x86_64), so the deployable dylib must be lipo'd.

**Files:** none (build/copy only).

- [ ] **Step 1: Build x86_64 slice**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
arch -x86_64 /usr/local/bin/cmake --build build-x86_64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`. If `build-x86_64` does not exist, regenerate it per the universal-build script before proceeding.

- [ ] **Step 2: lipo arm64 + x86_64 into universal dylib**

Run:
```bash
lipo -create \
    -output ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib \
    build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
    build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib
file ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

Expected: `file` reports the dylib as `Mach-O 64-bit dynamically linked shared library` with both `arm64` and `x86_64` slices.

- [ ] **Step 3: Sync resources sibling-dir**

Run:
```bash
rsync -a --delete pcsx2-libretro/bin/resources/ \
    ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/
```

(Defensive: no resources changed in this phase, but the sync is the standard close-out step and keeps the install layout consistent.)

- [ ] **Step 4: No commit. Build artifacts only.**

---

## Task 6: Live smoke gate

Verify the 5 Audio knobs end-to-end on a real PS2 game via RetroNest's UI. Audio effects are unambiguous (you can hear them), so smoke is fast.

**Files:** none (manual UI testing).

- [ ] **Step 1: Launch RetroNest in arm64 mode**

Open RetroNest from the Finder (NOT through `arch -x86_64`). Confirm a PS2 game is in the library — R&C 2 (NTSC) is the SP7a/SP7b/SP7c-Phase-1 reference and is the recommended smoke target.

- [ ] **Step 2: Open the per-emulator settings dialog for pcsx2-libretro**

Right-click the PS2 game → Settings. The hub should now render 3 cards: Recommended (3 settings), Emulation (15 settings), Audio (5 settings).

Acceptance: clicking the Audio card opens a page with 5 rows (SyncMode, BufferMS in "Configuration"; Volume, FastForwardVolume, Muted in "Controls"). Tooltips render. Combos open and show the correct value lists.

- [ ] **Step 3: Mute smoke — toggle OutputMuted to Enabled**

Set `Mute Audio` → `Enabled`. Save. Launch the game.

Expected: PS2 audio is silent (no music, no SFX). RetroNest UI sounds (if any) continue normally.

Console log should include the new echo line, e.g.:
```
[CoreOptions] audio: sync_mode=TimeStretch buffer_ms=50 volume=100 ff_volume=100 muted=on
```

Set `Mute Audio` → `Disabled`. Save. Launch. Confirm audio returns.

- [ ] **Step 4: Volume smoke — set Volume to 25%**

Set `Volume` → `25%`. Save. Launch.

Expected: PS2 audio is noticeably quieter (about a quarter the previous loudness). Music and SFX both attenuated equally.

Reset `Volume` → `100% (default)`. Save.

- [ ] **Step 5: Fast-forward volume smoke — set Fast-Forward Volume to 0%**

Set `Fast-Forward Volume` → `0% (Muted)`. Save. Launch. Engage RetroNest's fast-forward toggle (if mapped) — or run at normal speed first to confirm normal audio, then fast-forward.

Expected: normal-speed audio is full volume; fast-forward audio is silent.

Reset to `100% (default)`. Save.

- [ ] **Step 6: SyncMode smoke — switch to Disabled**

Set `Audio Sync Mode` → `Disabled (Noisy)`. Save. Launch.

Expected: at 100% emulation speed (the default), audio sounds nearly identical (no SoundTouch resampling needed when tempo == 1.0). The difference becomes audible only when emulation speed deviates from 100% — but smoke at 100% is sufficient to confirm the toggle reached PCSX2 without breaking audio entirely.

The console log echo should now show `sync_mode=Disabled`. Reset to `TimeStretch`. Save.

- [ ] **Step 7: BufferMS smoke — set to 10 ms**

Set `Audio Buffer Size` → `10 ms (lowest latency)`. Save. Launch.

Expected: audio still works (no silence, no crash). May have crackling/underruns on a CPU-pressured machine — that's the documented latency-vs-stability tradeoff. The log line should show `buffer_ms=10`.

Reset to `50 ms (default)`. Save.

- [ ] **Step 8: Schema-drift no-regression check**

In the settings dialog, edit any one of the 5 Audio knobs to a non-default value, save, and re-open the dialog. The saved value should persist (round-trip through `options.json` → core's `GET_VARIABLE` → `Audio::ApplyDefaults` → user-visible change on next launch). This catches the `OptionsStore::load` silent-drop bug if any value/key in the new rows doesn't match between core and host.

- [ ] **Step 9: No commit. Verification only.**

If anything fails, debug per `superpowers:systematic-debugging` before declaring Phase 2 complete. Common failure modes:
- SyncMode string mismatch — PCSX2's `ParseSyncMode` is case-sensitive. "Disabled" not "disabled"; "TimeStretch" not "Timestretch".
- Volume not applying — `spu2.cpp:108-109` reads `s_standard_volume` once at SPU2 init and on per-config-change events. If the value writes to `g_si` but doesn't propagate to `EmuConfig.SPU2.StandardVolume`, suspect `LoadStartupSettings()` ordering — should run AFTER `Audio::ApplyDefaults`.
- Mute silently ignored — `SPU2::SetOutputMuted` (`spu2.cpp:167-184`) requires the stream to exist; if launch sequence somehow bypasses SPU2 init, mute won't apply. Check log for the LibretroAudioStream construction line.

---

## Task 7: Close-out

- [ ] **Step 1: Verify all commits land**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git log --oneline -5
```

Expected: 2 new commits on `retronest-libretro` after `1c4b31d71`:
1. SP7c Phase 2 Task 1: scaffold CoreOptionsAudio module
2. SP7c Phase 2 Task 2 (core): Audio knobs

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git log --oneline -5
```

Expected: 2 new commits on `main` after `6569956`:
1. SP7c Phase 2 Task 3 (host): Audio card rows
2. SP7c Phase 2 Task 4 (host): add Audio card to libretro hub

- [ ] **Step 2: Do NOT push pcsx2-libretro to origin**

The pcsx2-libretro fork has no `origin` remote — only `upstream`. The `retronest-libretro` branch is intentionally kept local. Confirm:

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git remote -v
```

Expected: only `upstream` listed. If `origin` appears unexpectedly, stop and ask the user before pushing.

- [ ] **Step 3: RetroNest-Project main can be pushed if the user requests**

`origin` exists on RetroNest-Project. Wait for user direction before `git push origin main` — the user will decide.

- [ ] **Step 4: Update memory**

After live-smoke verification passes, update `~/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/sp7c_kickoff.md` to reflect Phase 2 shipped status. Add a Phase 2 handoff section summarizing:
- Commit range (2 core, 2 host).
- Total Audio-card rows: 5 (target was ~6, dropped to 5 after StretchSequenceLengthMS not in standalone — documented in the spec).
- Total knobs across SP7b + Phase 1 + Phase 2: 3 + 15 + 5 = 23.
- Hub now renders 3 cards (Recommended, Emulation, Audio).
- Lessons surfaced during Phase 2 execution (whatever shows up).
- Next focus: Phase 3 (Memory Cards, 5 knobs).

- [ ] **Step 5: Update spec status**

Update `RetroNest-Project/docs/superpowers/specs/2026-05-13-pcsx2-libretro-sp7c-settings-parity-design.md` Phase 2 section status line to ✅ shipped on the actual date. Document the final knob count (5, not 6) and the SyncMode-enum correction (2 values, not 3 as originally drafted).

Commit the docs change:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add docs/superpowers/specs/2026-05-13-pcsx2-libretro-sp7c-settings-parity-design.md \
        docs/superpowers/plans/2026-05-13-pcsx2-libretro-sp7c-phase2-audio.md
git commit -m "$(cat <<'EOF'
docs: mark SP7c Phase 2 ✅ shipped

Audio card now at libretro-applicable parity with the standalone PCSX2
dialog (5 knobs: SyncMode / BufferMS / StandardVolume / FastForwardVolume
/ OutputMuted). Cubeb-only knobs intentionally dropped (silently inert
in libretro mode — audio routes through LibretroAudioStream, not Cubeb).
Live-smoke verified.

Hub now renders 3 cards (Recommended + Emulation + Audio).

Next focus: Phase 3 (Memory Cards, 5 knobs).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-review checklist (the plan author runs this before handoff)

- [x] **Spec coverage:** every knob in the spec's Phase 2 section (Audio card) has a corresponding task — 5 net new knobs across Task 2. The 6 dropped candidates (ExpansionMode, OutputLatencyMS, OutputLatencyMinimal, Backend, DriverName, DeviceName) are explicitly documented as dropped with INI-key-level rationale. StretchSequenceLengthMS (memory's 6th candidate) is also dropped with rationale (not in standalone).
- [x] **SyncMode value set:** corrected from kickoff memory's "3 values (TimeStretch / ASync / None)" to the actual upstream enum (2 values: `Disabled` / `TimeStretch`) per `pcsx2/Config.h:960-965` and `pcsx2/Pcsx2Config.cpp:1180-1183`. Standalone adapter at `pcsx2_adapter.cpp:941-944` confirms the 2-value set. Plan documents this correction.
- [x] **No placeholders:** every step has the actual code or command. No "TBD", "similar to above without repetition", or "handle edge cases".
- [x] **Type consistency:** `pcsx2_audio_sync_mode` is `std::string` in `Values`; `SetStringValue` is called in `ApplyDefaults`; Case 11 reads `r.audio.sync_mode` (matches field name). Field naming matches across all 4 sites for each knob.
- [x] **Fork has no `origin` remote** — Task 7 Step 2 explicitly checks and refuses to push.
- [x] **`CORE_OPTIONS_TEST_ONLY`** macro gate used consistently in `CoreOptionsAudio.cpp` (same as Emulation module).
- [x] **`Resolved` is nested** — `r.audio.sync_mode`, not `r.sync_mode`. All test assertions match.
- [x] **`ApplyDefaults` body stays gated** by `#ifndef CORE_OPTIONS_TEST_ONLY` so the standalone test compile chain (which doesn't link `MemorySettingsInterface`) still works.
- [x] **Standalone test compile command** includes `../CoreOptionsAudio.cpp` (Task 1.7 updates the header-comment; Tasks 1.8 / 2.6 use the new command).
- [x] **Phase 2 host rows go under `category="Audio"`** with group `"Configuration"` for SyncMode + BufferMS, group `"Controls"` for volumes + muted — mirrors the standalone PCSX2 dialog's sub-section layout (`pcsx2_adapter.cpp:909-977`).
- [x] **Hub card added in-phase** — Task 4 satisfies the "MANDATORY in-phase hub expansion" rule from the Phase 1 smoke-caught gap. Not deferred.
- [x] **`Settings.cpp` per-call block wiring** — Task 1.5 adds the `Audio::ApplyDefaults` call alongside `Emulation::ApplyDefaults`, BEFORE `LoadStartupSettings()`, so user tweaks take effect on the next `retro_load_game` without restarting RetroNest. (Phase 1 followup `a5b432c2f` established this pattern.)
- [x] **Literal `out.push_back({...})` form everywhere** in `CoreOptionsAudio.cpp` — no lambda helpers. `check_schema_fidelity.py`'s `CORE_BLOCK_RE` only parses literal brace-init blocks (Phase 1 Task 2 caught this the hard way).
- [x] **No `/*dependsOn=*/` inline comments** in host `s.append(opt(...))` calls. None of the 5 Audio rows declare dependencies; `dependsOn` defaults to empty.
- [x] **CMakeLists update** — Task 1.6 adds `CoreOptionsAudio.cpp` to the target sources.
- [x] **Echo log extension** — Task 2.4 adds the 5th `[CoreOptions] audio: ...` line so smoke testing can verify each knob reached the core.
