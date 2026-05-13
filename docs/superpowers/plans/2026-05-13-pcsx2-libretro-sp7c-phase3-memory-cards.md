# SP7c Phase 3 Implementation Plan — Memory Cards Card (5 new knobs)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the 5 Memory Cards knobs that the standalone PCSX2 dialog shows under its "Memory Cards" card (2 Slot Enable toggles + 3 Multitap toggles). The 2 filename rows are dropped because libretro core options are combo-only — filenames stay hardcoded in `Settings.cpp`. Add a Memory Cards card to the libretro hub at grid `(1, 2)` so the new rows are reachable from the UI.

**Architecture:** Each knob touches the same five places as Phase 1/2's per-knob workflow:
1. New module `pcsx2-libretro/CoreOptionsMemoryCards.{h,cpp}` — owns `kMemoryCardsDefinitions[]`, `MemoryCards::Parse`, `MemoryCards::ApplyDefaults`.
2. `pcsx2-libretro/CoreOptions.h` — nest `MemoryCards::Values memory_cards{}` in `Resolved`.
3. `pcsx2-libretro/CoreOptions.cpp` — aggregate `MemoryCards::AppendDefinitions` + `MemoryCards::Parse`; extend the 5-line `[CoreOptions]` echo log to 6 lines.
4. `pcsx2-libretro/Settings.cpp` — REMOVE the two `Slot{1,2}_Enable` writes from the one-shot block; ADD `MemoryCards::ApplyDefaults` call to the per-call user-options block. Filenames stay in the one-shot block.
5. `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — 5 new `s.append(opt(...))` rows under `category="Memory Cards"` in 2 groups.
6. `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` — add a Memory Cards card at grid `(1, 2)` (right of Audio at `(1, 1)`).

Schema fidelity (byte-for-byte parity between core values and host options) is verified mechanically by `tools/check_schema_fidelity.py`. Round-trip parsing is verified by `tools/test_core_options.cpp` Case 12 (new).

**Tech Stack:**
- pcsx2-libretro: C++20, clang, libretro.h v2 core-options API.
- RetroNest-Project: C++20, Qt 6 (read/append-only on `settingsSchema`).
- Schema check: Python 3 (`tools/check_schema_fidelity.py` exists from Phase 0).
- Build: CMake (`build-arm64` is the dev build; universal lipo + RetroNest copy at close-out).
- Standalone unit test: `clang++` with `-DCORE_OPTIONS_TEST_ONLY`.

**Repo locations:**
- pcsx2-libretro: `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `retronest-libretro`, currently at HEAD `91bf9f4bf`).
- RetroNest-Project: `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`, currently at HEAD `87dc9fc6` — Phase 2 docs commit + 27 commits pushed to `origin/main` 2026-05-13).

**Scope guard:**
- 5 net new core option keys, 5 net new host SettingDef rows. No other categories touched.
- Phase 3 does NOT add Graphics (Phase 4) nor the Recommended-card reorg (Phase 5). The hub gains exactly one card (Memory Cards).
- pcsx2-libretro fork has NO `origin` remote — close-out ends at `commit + lipo + copy to RetroNest cores dir`. **Do not include `git push origin` anywhere in this plan.**

---

## Knob inventory (5 total — pre-investigated 2026-05-13)

Section `MemoryCards`. Each row is one core option key + one host SettingDef row. All five are `Bool` enabled/disabled toggles — no enums, no ints.

| Core option key | INI key | Type | Default | Group | Standalone row source |
|---|---|---|---|---|---|
| `pcsx2_mc_slot1_enable`    | `Slot1_Enable`           | Bool | `true`  | Memory Card Slots | `pcsx2_adapter.cpp:987-990` |
| `pcsx2_mc_slot2_enable`    | `Slot2_Enable`           | Bool | `true`  | Memory Card Slots | `pcsx2_adapter.cpp:1000-1003` |
| `pcsx2_mc_multitap1_slot2` | `Multitap1_Slot2_Enable` | Bool | `false` | Multitap          | `pcsx2_adapter.cpp:1013-1016` |
| `pcsx2_mc_multitap1_slot3` | `Multitap1_Slot3_Enable` | Bool | `false` | Multitap          | `pcsx2_adapter.cpp:1019-1022` |
| `pcsx2_mc_multitap1_slot4` | `Multitap1_Slot4_Enable` | Bool | `false` | Multitap          | `pcsx2_adapter.cpp:1025-1028` |

### Decision: Slot2_Enable default = `true` (matches standalone)

The standalone PCSX2 dialog at `cpp/src/adapters/pcsx2_adapter.cpp:1002` defaults `Slot2_Enable` to `true`. Pre-Phase-3 `Settings.cpp:287` defaults it to `false` (a SP6-era single-slot convention). Phase 3 follows the parity-driven precedent set in Phase 2 (drop `StretchSequenceLengthMS` because standalone doesn't show it) and adopts standalone's default of `true`.

**Behavioral change to flag at smoke time:** users that have never opened the Memory Cards dialog will now get Slot 2 enabled on the next core launch. PCSX2 only auto-creates `Mcd002.ps2` on the first WRITE to Slot 2 (not on enable-toggle), so the on-disk footprint change is invisible until a game actively uses Slot 2. The standalone behavior is identical — this is parity, not regression.

### Value strings (must match the host adapter byte-for-byte)

All 5 knobs use the standard bool pair (matching Phase 1/2's bool pattern):
```
{"enabled",  "Enabled"}
{"disabled", "Disabled"}
```

### Knobs explicitly DROPPED

| Standalone candidate | INI key | Why dropped |
|---|---|---|
| `Slot1_Filename` | `MemoryCards/Slot1_Filename` | libretro core options are Combo-only — no free-form string input. Filename stays hardcoded `"Mcd001.ps2"` in `Settings.cpp` one-shot block. |
| `Slot2_Filename` | `MemoryCards/Slot2_Filename` | Same as above — hardcoded `"Mcd002.ps2"`. |
| `Multitap1_Enable` master toggle | (does not exist in `MemoryCards` section) | Standalone has no master Multitap1 toggle in this card. The 3 `Multitap1_Slot{2,3,4}_Enable` rows are independent. The unrelated `Pcsx2/MultitapPort1_Enabled` setting lives in a different section and is exposed by standalone's controllers settings — out of scope for the Memory Cards card. |
| `MultitapPort1_Enabled` / `MultitapPort2_Enabled` | `Pcsx2` section | Same as above — different section (`Pcsx2`, not `MemoryCards`); not in standalone's Memory Cards rows; out of scope. |

If a future need for user-tweakable filenames arises (per-game memcards via path manipulation, etc.), revisit then.

### Settings.cpp cleanup required (mirrors Phase 1 Task 5's HostFs cleanup)

Currently `Settings.cpp:285-288` (one-shot init block) writes:
```cpp
g_si.SetBoolValue  ("MemoryCards", "Slot1_Enable",   true);
g_si.SetStringValue("MemoryCards", "Slot1_Filename", "Mcd001.ps2");
g_si.SetBoolValue  ("MemoryCards", "Slot2_Enable",   false);
g_si.SetStringValue("MemoryCards", "Slot2_Filename", "Mcd002.ps2");
```

Phase 3 must:
1. **Move** the two `Slot{1,2}_Enable` writes OUT of the one-shot block — they are now owned by `MemoryCards::ApplyDefaults` and re-applied on every `retro_load_game`.
2. **Keep** the two `Slot{1,2}_Filename` writes in the one-shot block — filenames are not user-configurable in Phase 3 (libretro Combo-only constraint).
3. **Add** the 3 multitap writes to `MemoryCards::ApplyDefaults` (none exist in `Settings.cpp` today — `VMManager::SetDefaultSettings` writes the upstream defaults of `false`, which Phase 3 preserves as the struct defaults).

The `Folders/MemoryCards = save_dir + "/memcards"` write at `Settings.cpp:277` is infrastructure (not a user knob) and stays in the one-shot block.

---

## File structure

### Created in this phase

| File | Purpose |
|---|---|
| `pcsx2-libretro/CoreOptionsMemoryCards.h` | Mirrors `CoreOptionsAudio.h`'s shape — `struct Values` (5 bool fields), `AppendDefinitions`, `Parse`, `ApplyDefaults`. |
| `pcsx2-libretro/CoreOptionsMemoryCards.cpp` | Mirrors `CoreOptionsAudio.cpp`'s shape — 5 literal `out.push_back({...})` blocks, `Parse` with 5 bool branches, `ApplyDefaults` gated by `#ifndef CORE_OPTIONS_TEST_ONLY`. |

### Modified in this phase

| File | Action |
|---|---|
| `pcsx2-libretro/CoreOptions.h` | Add `MemoryCards::Values memory_cards{}` field to `struct Resolved`; `#include "CoreOptionsMemoryCards.h"`. |
| `pcsx2-libretro/CoreOptions.cpp` | Add `#include "CoreOptionsMemoryCards.h"`; bump `v.reserve(24)` → `v.reserve(32)`; call `MemoryCards::AppendDefinitions(v)` and `MemoryCards::Parse(cb, r.memory_cards)`; extend the 5-line `[CoreOptions]` echo log to 6 lines (add memory_cards line). |
| `pcsx2-libretro/Settings.cpp` | Remove the 2 `Slot{1,2}_Enable` writes from the one-shot block (lines 285, 287); keep the 2 `Slot{1,2}_Filename` writes (lines 286, 288). In the per-call user-options block, call `MemoryCards::ApplyDefaults(g_si, options ? options->memory_cards : MemoryCards::Values{})`. |
| `pcsx2-libretro/CMakeLists.txt` | Append `CoreOptionsMemoryCards.cpp` to the pcsx2_libretro target sources. |
| `pcsx2-libretro/tools/test_core_options.cpp` | Add Case 12 (Memory Cards round-trip); update header comment with `../CoreOptionsMemoryCards.cpp` in the compile example. |
| `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` | Append 5 `s.append(opt(...))` rows under `category="Memory Cards"` (2 in group "Memory Card Slots", 3 in group "Multitap"). |
| `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` | Add a Memory Cards card at grid `(1, 2)` next to the Audio card. Update the leading comment to reflect Phase 3 having shipped. |

---

## Task 0: Confirm baseline is green

**Files:** none (verification only)

- [ ] **Step 1: Confirm pcsx2-libretro HEAD and clean tree**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git log -1 --oneline retronest-libretro
git status
```
Expected: HEAD is `91bf9f4bf SP7c Phase 2 Task 2 (core): Audio knobs`. Working tree clean except for the listed untracked tools/ artifacts (`__pycache__`, `resources`, `test_core_options`, `test_rcheevos_hash`, `test_region_prefix`).

- [ ] **Step 2: Confirm RetroNest-Project HEAD and clean tree**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git log -1 --oneline main
git status
```
Expected: HEAD includes the Phase 2 hub + adapter + docs commits. Working tree clean.

- [ ] **Step 3: Run schema-fidelity check on current baseline**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```
Expected: green — "23 core keys / 23 host keys, byte-for-byte match" (or equivalent success line).

- [ ] **Step 4: Run the standalone test on current baseline**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp \
  ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
  -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```
Expected: "0 failures across 11 cases" (or equivalent — 11 numbered cases, exit code 0).

- [ ] **Step 5: No commit** (baseline check only)

---

## Task 1: Scaffold CoreOptionsMemoryCards module (header + empty impl)

**Files:**
- Create: `pcsx2-libretro/CoreOptionsMemoryCards.h`
- Create: `pcsx2-libretro/CoreOptionsMemoryCards.cpp`
- Modify: `pcsx2-libretro/CoreOptions.h`
- Modify: `pcsx2-libretro/CoreOptions.cpp`
- Modify: `pcsx2-libretro/Settings.cpp`
- Modify: `pcsx2-libretro/CMakeLists.txt`

The aim of Task 1 is to land the empty module wired into the aggregator + Settings.cpp so the build stays green. Task 2 will fill in the 5 knobs.

- [ ] **Step 1: Create `pcsx2-libretro/CoreOptionsMemoryCards.h`**

Content (mirrors `CoreOptionsAudio.h`):

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 3: Memory Cards-category core options.
//
// Owns the kMemoryCardsDefinitions[] slice of the master core-options
// table. CoreOptions.cpp aggregates this module's slice (plus Emulation's
// from Phase 0/1 and Audio's from Phase 2) into the single table dispatched
// via SET_CORE_OPTIONS_V2.

#pragma once

#include "libretro.h"

#include <vector>

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }
class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions::MemoryCards
{

// Per-category resolved values. Aggregated by struct Resolved.
//
// Defaults match the upstream PCSX2 defaults shown by the standalone
// PCSX2 dialog (cpp/src/adapters/pcsx2_adapter.cpp:987-1028) so a
// missing/empty options.json reproduces standalone's out-of-the-box
// behavior. Slot2_Enable defaults to true here (matching standalone),
// not false — Phase 3 deliberately switches the pre-SP7c default for
// parity. See plan section "Decision: Slot2_Enable default".
struct Values
{
    // MemoryCards/Slot1_Enable — inserts a virtual memcard into Slot 1.
    // Pcsx2Config.cpp:2051 reads via SettingsWrapEntry as `Mcd[0].Enabled`.
    bool slot1_enable = true;

    // MemoryCards/Slot2_Enable — inserts a virtual memcard into Slot 2.
    // Pcsx2Config.cpp:2051 reads via SettingsWrapEntry as `Mcd[1].Enabled`.
    bool slot2_enable = true;

    // MemoryCards/Multitap1_Slot{2,3,4}_Enable — enables additional
    // memcard slots when Multitap 1 is connected. Pcsx2Config.cpp:2062
    // reads via SettingsWrapEntry. Independent of any Multitap1
    // master toggle (which lives in the unrelated Pcsx2 section).
    bool multitap1_slot2 = false;
    bool multitap1_slot3 = false;
    bool multitap1_slot4 = false;
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

} // namespace Pcsx2Libretro::CoreOptions::MemoryCards
```

- [ ] **Step 2: Create `pcsx2-libretro/CoreOptionsMemoryCards.cpp` (empty bodies)**

Content (the empty scaffold; Task 2 fills in the 5 knobs):

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsMemoryCards.h"

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

#include <cstring>

namespace Pcsx2Libretro::CoreOptions::MemoryCards
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& /*out*/)
{
    // Filled in by Task 2.
}

void Parse(retro_environment_t /*cb*/, Values& /*out*/)
{
    // Filled in by Task 2.
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& /*si*/, const Values& /*v*/)
{
    // Filled in by Task 2.
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::MemoryCards
```

- [ ] **Step 3: Aggregate the new module in `CoreOptions.h`**

Edit `pcsx2-libretro/CoreOptions.h`:

Replace this block:
```cpp
#include "libretro.h"
#include "CoreOptionsEmulation.h"
#include "CoreOptionsAudio.h"
```
with:
```cpp
#include "libretro.h"
#include "CoreOptionsEmulation.h"
#include "CoreOptionsAudio.h"
#include "CoreOptionsMemoryCards.h"
```

And replace this block in `struct Resolved`:
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
with:
```cpp
struct Resolved
{
    Pcsx2Libretro::CoreOptions::Emulation::Values    emulation{};
    Pcsx2Libretro::CoreOptions::Audio::Values        audio{};
    Pcsx2Libretro::CoreOptions::MemoryCards::Values  memory_cards{};
    // Future phases append:
    //   Pcsx2Libretro::CoreOptions::Graphics::Values graphics{};
};
```

- [ ] **Step 4: Aggregate the new module in `CoreOptions.cpp`**

Edit `pcsx2-libretro/CoreOptions.cpp`:

Replace this block:
```cpp
#include "CoreOptions.h"
#include "CoreOptionsEmulation.h"
#include "CoreOptionsAudio.h"
```
with:
```cpp
#include "CoreOptions.h"
#include "CoreOptionsEmulation.h"
#include "CoreOptionsAudio.h"
#include "CoreOptionsMemoryCards.h"
```

In `BuildDefinitions()`, replace this block:
```cpp
    static const std::vector<retro_core_option_v2_definition> kAll = [] {
        std::vector<retro_core_option_v2_definition> v;
        v.reserve(24);  // 18 Phase 1 + 5 Phase 2 + terminator
        Emulation::AppendDefinitions(v);
        Audio::AppendDefinitions(v);
        // libretro terminator — must be the final entry per libretro.h:6787.
        v.push_back({
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            {{nullptr, nullptr}},
            nullptr
        });
        return v;
    }();
```
with:
```cpp
    static const std::vector<retro_core_option_v2_definition> kAll = [] {
        std::vector<retro_core_option_v2_definition> v;
        v.reserve(32);  // 18 Phase 1 + 5 Phase 2 + 5 Phase 3 + terminator + headroom
        Emulation::AppendDefinitions(v);
        Audio::AppendDefinitions(v);
        MemoryCards::AppendDefinitions(v);
        // libretro terminator — must be the final entry per libretro.h:6787.
        v.push_back({
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            {{nullptr, nullptr}},
            nullptr
        });
        return v;
    }();
```

In `ReadResolved()`, replace this block:
```cpp
    Emulation::Parse(cb, r.emulation);
    Audio::Parse(cb, r.audio);

    // Future phases append Graphics::Parse, MemoryCards::Parse here.
```
with:
```cpp
    Emulation::Parse(cb, r.emulation);
    Audio::Parse(cb, r.audio);
    MemoryCards::Parse(cb, r.memory_cards);

    // Future phases append Graphics::Parse here.
```

(Echo-log line is added in Task 2 once the fields exist on the resolved struct — keeping Task 1 minimal.)

- [ ] **Step 5: Wire CMakeLists.txt**

Edit `pcsx2-libretro/CMakeLists.txt`. Find the existing block listing `CoreOptions.cpp` / `CoreOptionsAudio.cpp` / `CoreOptionsEmulation.cpp` (around line 19-21) and append `CoreOptionsMemoryCards.cpp` so it reads (alphabetical order preserved):

```cmake
    CoreOptions.cpp
    CoreOptionsAudio.cpp
    CoreOptionsEmulation.cpp
    CoreOptionsMemoryCards.cpp
```

- [ ] **Step 6: Wire Settings.cpp's per-call ApplyDefaults**

Edit `pcsx2-libretro/Settings.cpp`. In the per-call block (currently lines 306-314), replace:
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
with:
```cpp
    {
        const CoreOptions::Emulation::Values em_defaults{};
        CoreOptions::Emulation::ApplyDefaults(
            g_si, options ? options->emulation : em_defaults);

        const CoreOptions::Audio::Values audio_defaults{};
        CoreOptions::Audio::ApplyDefaults(
            g_si, options ? options->audio : audio_defaults);

        const CoreOptions::MemoryCards::Values mc_defaults{};
        CoreOptions::MemoryCards::ApplyDefaults(
            g_si, options ? options->memory_cards : mc_defaults);
    }
```

(`Settings.cpp`'s one-shot block edits — moving the 2 Slot Enable writes — happen in Task 2 once the new ApplyDefaults actually writes those keys; doing it in Task 1 would briefly leave both slots un-written between aggregator wiring and knob fill-in.)

- [ ] **Step 7: Build the core**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64
```
Expected: success. The new module compiles cleanly; `kAll.reserve(32)` does not yet have anything to push beyond Phase 1+2's 23 entries (MemoryCards::AppendDefinitions is empty); `MemoryCards::ApplyDefaults` is a no-op so Settings behavior is byte-identical to baseline.

- [ ] **Step 8: Recompile + run the standalone test**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp \
  ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
  ../CoreOptionsMemoryCards.cpp \
  -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```
Expected: still 0 failures across 11 cases. (Case 12 lands in Task 2.)

- [ ] **Step 9: Run schema-fidelity check**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```
Expected: still 23 core keys / 23 host keys, byte-for-byte match. The empty `MemoryCards::AppendDefinitions` adds no new keys.

- [ ] **Step 10: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsMemoryCards.h \
        pcsx2-libretro/CoreOptionsMemoryCards.cpp \
        pcsx2-libretro/CoreOptions.h \
        pcsx2-libretro/CoreOptions.cpp \
        pcsx2-libretro/Settings.cpp \
        pcsx2-libretro/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP7c Phase 3 Task 1: scaffold CoreOptionsMemoryCards module

Mirrors the Phase 2 CoreOptionsAudio scaffold: empty AppendDefinitions /
Parse / ApplyDefaults bodies; aggregated by CoreOptions.{h,cpp}; Resolved
gains a MemoryCards::Values memory_cards{} field; Settings.cpp's per-call
block calls MemoryCards::ApplyDefaults (currently a no-op); CMakeLists.txt
includes the new TU.

Bumps the BuildDefinitions reserve from 24 → 32 (target capacity = 18
Phase 1 + 5 Phase 2 + 5 Phase 3 + terminator + small headroom). Phase 2's
Task 1 followup landed the lesson that v.reserve(N) must size to FINAL
capacity to avoid the reallocation that would otherwise happen at the 17th
new entry.

Knob bodies + Settings.cpp's one-shot-block cleanup land in Task 2.
EOF
)"
```

---

## Task 2: Add the 5 Memory Cards knobs

**Files:**
- Modify: `pcsx2-libretro/CoreOptionsMemoryCards.cpp`
- Modify: `pcsx2-libretro/CoreOptions.cpp` (extend echo log to a 6th line)
- Modify: `pcsx2-libretro/Settings.cpp` (REMOVE 2 Slot{1,2}_Enable writes from one-shot block)
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (add Case 12)

- [ ] **Step 1: Fill in `MemoryCards::AppendDefinitions`**

Replace the `AppendDefinitions` body in `pcsx2-libretro/CoreOptionsMemoryCards.cpp` with:

```cpp
void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // SP7c Phase 3 — Memory Cards card (5 knobs under MemoryCards).
    //
    // All 5 are independent bool toggles. Slot{1,2}_Enable govern whether
    // PCSX2 mounts a virtual memory card in slots 1/2. Multitap1_Slot{2,3,4}
    // expose the additional memcard slots that become accessible when
    // Multitap 1 is connected — they are independent of the Multitap1
    // master enable (which lives in the unrelated Pcsx2 section and is
    // managed by the controllers settings, not the Memory Cards card).
    //
    // The 2 filename rows from the standalone dialog (Slot1_Filename,
    // Slot2_Filename) are dropped — libretro core options are Combo-only,
    // not free-form strings, so the filenames stay hardcoded in
    // Settings.cpp's one-shot init block.
    //
    // Each entry is a literal out.push_back({...}) block (no lambda
    // helper) so tools/check_schema_fidelity.py's CORE_BLOCK_RE
    // recognizes them.
    out.push_back({
        "pcsx2_mc_slot1_enable",
        "Memory Card Slot 1",
        nullptr,
        "Inserts a virtual memory card into Slot 1. The card image is "
        "stored as Mcd001.ps2 under the per-game memcards folder. "
        "Disabling this prevents games from saving/loading via Slot 1.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_mc_slot2_enable",
        "Memory Card Slot 2",
        nullptr,
        "Inserts a virtual memory card into Slot 2. The card image is "
        "stored as Mcd002.ps2 under the per-game memcards folder. "
        "PCSX2 only auto-creates the file the first time a game writes "
        "to Slot 2.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_mc_multitap1_slot2",
        "Multitap 1 - Slot 2",
        nullptr,
        "Enables the second memory-card slot of Multitap 1. Only useful "
        "when a game supports Multitap 1 and you need additional save "
        "slots for extra players.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_mc_multitap1_slot3",
        "Multitap 1 - Slot 3",
        nullptr,
        "Enables the third memory-card slot of Multitap 1.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_mc_multitap1_slot4",
        "Multitap 1 - Slot 4",
        nullptr,
        "Enables the fourth memory-card slot of Multitap 1.",
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

- [ ] **Step 2: Fill in `MemoryCards::Parse`**

Replace the `Parse` body in `pcsx2-libretro/CoreOptionsMemoryCards.cpp` with:

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

    // 5 bool knobs. "enabled" → true, anything else → false. Mirrors the
    // single-line bool branches used in CoreOptionsAudio.cpp::Parse for
    // pcsx2_audio_muted (no helper lambda — at 5 callsites, inline is
    // shorter and matches Phase 2's precedent).
    if (const char* v = query("pcsx2_mc_slot1_enable"))
        out.slot1_enable = (std::strcmp(v, "enabled") == 0);
    if (const char* v = query("pcsx2_mc_slot2_enable"))
        out.slot2_enable = (std::strcmp(v, "enabled") == 0);
    if (const char* v = query("pcsx2_mc_multitap1_slot2"))
        out.multitap1_slot2 = (std::strcmp(v, "enabled") == 0);
    if (const char* v = query("pcsx2_mc_multitap1_slot3"))
        out.multitap1_slot3 = (std::strcmp(v, "enabled") == 0);
    if (const char* v = query("pcsx2_mc_multitap1_slot4"))
        out.multitap1_slot4 = (std::strcmp(v, "enabled") == 0);
}
```

- [ ] **Step 3: Fill in `MemoryCards::ApplyDefaults`**

Replace the `ApplyDefaults` body (still inside the `#ifndef CORE_OPTIONS_TEST_ONLY` block) in `pcsx2-libretro/CoreOptionsMemoryCards.cpp` with:

```cpp
#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // All 5 keys live under PCSX2's MemoryCards INI section.
    // Pcsx2Config::McdOptions::LoadSave (Pcsx2Config.cpp:2051-2065) reads
    // these via SettingsWrapEntry at VMManager refresh time —
    // LoadStartupSettings() runs after this in InitializeDefaults so the
    // new values take effect on this launch.
    //
    // The Slot{1,2}_Filename writes stay in Settings.cpp's one-shot block
    // (libretro Combo-only constraint — filenames are not user-tweakable).
    si.SetBoolValue("MemoryCards", "Slot1_Enable",           v.slot1_enable);
    si.SetBoolValue("MemoryCards", "Slot2_Enable",           v.slot2_enable);
    si.SetBoolValue("MemoryCards", "Multitap1_Slot2_Enable", v.multitap1_slot2);
    si.SetBoolValue("MemoryCards", "Multitap1_Slot3_Enable", v.multitap1_slot3);
    si.SetBoolValue("MemoryCards", "Multitap1_Slot4_Enable", v.multitap1_slot4);
}
#endif
```

- [ ] **Step 4: Extend the `[CoreOptions]` echo log to a 6th line**

Edit `pcsx2-libretro/CoreOptions.cpp`. After the existing audio-line block (currently the last `FrontendLog` call before `return r;`), insert a memory_cards-line block. The full sequence in `ReadResolved` should now end like:

```cpp
    const auto& a = r.audio;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] audio: sync_mode=%s buffer_ms=%d volume=%d "
        "ff_volume=%d muted=%s",
        a.sync_mode.c_str(), a.buffer_ms, a.volume, a.ff_volume,
        a.muted ? "on" : "off");

    const auto& m = r.memory_cards;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] memory_cards: slot1=%s slot2=%s mt1_s2=%s "
        "mt1_s3=%s mt1_s4=%s",
        m.slot1_enable    ? "on" : "off",
        m.slot2_enable    ? "on" : "off",
        m.multitap1_slot2 ? "on" : "off",
        m.multitap1_slot3 ? "on" : "off",
        m.multitap1_slot4 ? "on" : "off");

    return r;
```

- [ ] **Step 5: Remove the 2 Slot{1,2}_Enable writes from Settings.cpp's one-shot block**

Edit `pcsx2-libretro/Settings.cpp`. The block at lines 285-288 currently reads:
```cpp
    g_si.SetBoolValue  ("MemoryCards", "Slot1_Enable",   true);
    g_si.SetStringValue("MemoryCards", "Slot1_Filename", "Mcd001.ps2");
    g_si.SetBoolValue  ("MemoryCards", "Slot2_Enable",   false);
    g_si.SetStringValue("MemoryCards", "Slot2_Filename", "Mcd002.ps2");
```

Replace with (delete the 2 Bool writes; keep the 2 String writes; add a comment explaining the move):
```cpp
    // Slot{1,2}_Enable are now owned by CoreOptions::MemoryCards::ApplyDefaults
    // (called from the per-call user-options block below) so dialog tweaks
    // take effect on the next retro_load_game. Filenames stay hardcoded
    // here — libretro core options are Combo-only.
    g_si.SetStringValue("MemoryCards", "Slot1_Filename", "Mcd001.ps2");
    g_si.SetStringValue("MemoryCards", "Slot2_Filename", "Mcd002.ps2");
```

Also, update the SP6 explanatory block immediately above (lines 259-273). The existing comment talks about "Slot 2: disabled. Single-slot is the libretro convention". After Phase 3 the convention is parity-with-standalone (Slot 2 enabled by default). Replace the lines 259-273 comment block:

```cpp
    // SP6: configure memory cards.
    //
    // Slot 1: enabled, file-image type, "Mcd001.ps2" (PCSX2 standard).
    //   PCSX2 auto-detects MemoryCardType::File from the .ps2 extension
    //   at MemoryCardFile.cpp load time, and auto-creates the file on
    //   first save. No Slot1_Type key needed.
    //
    // Slot 2: disabled. Single-slot is the libretro convention; SP7
    //   settings UI may expose a toggle later.
    //
    // Folders/MemoryCards: rooted at libretro save_dir so each
    //   {game_id}/ scope gets its own memcard image (RetroNest's
    //   adapter is responsible for the per-game scoping in save_dir).
    //   Empty save_dir falls back to PCSX2's default ("memcards"
    //   under DataRoot) — usable but not per-game-isolated.
```
with:
```cpp
    // SP6 / SP7c-Phase-3: configure memory cards.
    //
    // Filenames: hardcoded "Mcd001.ps2" / "Mcd002.ps2" (PCSX2 standard).
    //   PCSX2 auto-detects MemoryCardType::File from the .ps2 extension
    //   at MemoryCardFile.cpp load time and auto-creates the file on
    //   first write. Filenames are NOT user-tweakable — libretro core
    //   options are Combo-only, not free-form strings.
    //
    // Slot{1,2} enable + Multitap1 slot enables: now user-tweakable via
    //   CoreOptions::MemoryCards (per-call ApplyDefaults below). Defaults
    //   match the standalone PCSX2 dialog (both slots enabled, multitap
    //   slots disabled).
    //
    // Folders/MemoryCards: rooted at libretro save_dir so each
    //   {game_id}/ scope gets its own memcard image (RetroNest's
    //   adapter is responsible for the per-game scoping in save_dir).
    //   Empty save_dir falls back to PCSX2's default ("memcards"
    //   under DataRoot) — usable but not per-game-isolated.
```

- [ ] **Step 6: Add Case 12 (Memory Cards round-trip) to test_core_options.cpp**

Edit `pcsx2-libretro/tools/test_core_options.cpp`. First, update the header comment's compile example (currently around line 9) to include `CoreOptionsMemoryCards.cpp`:

```cpp
//       ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
//       ../CoreOptionsMemoryCards.cpp \
```

Then, find the end of Case 11 (around line 365 — the closing `}` of the unknown-sync-mode-fallback sub-block). Immediately before the closing `return failures != 0;` of `main()`, insert the new Case 12. (If the file uses a different end-of-main idiom, place Case 12 right after Case 11 and before whatever summary print + return-statement comes next — match the style of how Case 11 was added.)

Insert this block:

```cpp
    // -------- Case 12: Memory Cards round-trip --------
    //
    // SP7c Phase 3 representative test for the Memory Cards card. Sets
    // all 5 MemoryCards knobs to NON-default values (slot1=off,
    // slot2=off, all 3 multitap=on), parses, asserts each field reflects
    // the env-var value. The non-default choice for each knob means a
    // broken Parse branch (e.g. struct-init bleed-through) would be
    // caught — testing slot1=on against the default true would be a
    // tautology.
    {
        FakeEnv env;
        env.set("pcsx2_mc_slot1_enable",    "disabled");
        env.set("pcsx2_mc_slot2_enable",    "disabled");
        env.set("pcsx2_mc_multitap1_slot2", "enabled");
        env.set("pcsx2_mc_multitap1_slot3", "enabled");
        env.set("pcsx2_mc_multitap1_slot4", "enabled");

        Pcsx2Libretro::CoreOptions::Resolved r =
            Pcsx2Libretro::CoreOptions::ReadResolved(env.cb());

        check_bool("Case 12 slot1=disabled",    r.memory_cards.slot1_enable,    false);
        check_bool("Case 12 slot2=disabled",    r.memory_cards.slot2_enable,    false);
        check_bool("Case 12 mt1_slot2=enabled", r.memory_cards.multitap1_slot2, true);
        check_bool("Case 12 mt1_slot3=enabled", r.memory_cards.multitap1_slot3, true);
        check_bool("Case 12 mt1_slot4=enabled", r.memory_cards.multitap1_slot4, true);
    }

    // -------- Case 12b: Memory Cards default-when-unset --------
    //
    // Confirms that when the host returns NULL for every Memory Cards
    // key (the "user has never opened the dialog" case), Resolved's
    // struct defaults stand: both slots enabled, all 3 multitap slots
    // disabled. This is the parity-with-standalone behavior introduced
    // in Phase 3 — pre-Phase-3 had Slot2 default = false.
    {
        FakeEnv env;
        // Intentionally do not set any pcsx2_mc_* keys.

        Pcsx2Libretro::CoreOptions::Resolved r =
            Pcsx2Libretro::CoreOptions::ReadResolved(env.cb());

        check_bool("Case 12b slot1 default=on",  r.memory_cards.slot1_enable,    true);
        check_bool("Case 12b slot2 default=on",  r.memory_cards.slot2_enable,    true);
        check_bool("Case 12b mt1_s2 default=off", r.memory_cards.multitap1_slot2, false);
        check_bool("Case 12b mt1_s3 default=off", r.memory_cards.multitap1_slot3, false);
        check_bool("Case 12b mt1_s4 default=off", r.memory_cards.multitap1_slot4, false);
    }
```

If `FakeEnv` and the `check_bool` helper do not match the existing test infrastructure verbatim — read the file's current Case 11 (around lines 329-365) and copy whatever helper-naming it uses. The structure above is the intent; the exact symbol names must match what test_core_options.cpp already uses.

- [ ] **Step 7: Compile and run the standalone test**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp \
  ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
  ../CoreOptionsMemoryCards.cpp \
  -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```
Expected: 0 failures across 12 numbered cases (counting Case 12 + Case 12b separately if your runner does, or "13 cases" if it does — match the test runner's reporting style; the exit code is the load-bearing assertion).

- [ ] **Step 8: Build the core**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64
```
Expected: success. Trust this output, NOT clangd diagnostics (Phase 0/1/2 lesson — compile_commands.json staleness produces persistent false positives on PCSX2-internal headers).

- [ ] **Step 9: Run schema-fidelity check (will FAIL — host rows land in Task 3)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```
Expected: **FAIL** with "core has 5 keys not present in host: pcsx2_mc_slot1_enable, pcsx2_mc_slot2_enable, pcsx2_mc_multitap1_slot2, pcsx2_mc_multitap1_slot3, pcsx2_mc_multitap1_slot4" (or the script's equivalent). This is expected — Task 3 lands the matching host rows. **Do NOT block on this failure.**

- [ ] **Step 10: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsMemoryCards.cpp \
        pcsx2-libretro/CoreOptions.cpp \
        pcsx2-libretro/Settings.cpp \
        pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 3 Task 2 (core): Memory Cards knobs

5 literal out.push_back({...}) blocks in CoreOptionsMemoryCards.cpp's
AppendDefinitions (Slot{1,2}_Enable + Multitap1_Slot{2,3,4}_Enable);
single-line bool Parse branches (no helper lambda — at 5 callsites,
inline matches Phase 2's pcsx2_audio_muted precedent); ApplyDefaults
writes 5 keys under MemoryCards section.

Settings.cpp's one-shot init block: REMOVE the 2 Slot{1,2}_Enable
writes (now owned by MemoryCards::ApplyDefaults) and update the SP6
comment block to reflect parity-with-standalone (both slots default
enabled, was: Slot 2 default disabled). Filenames stay hardcoded —
libretro core options are Combo-only.

CoreOptions.cpp echo log gains a 6th [CoreOptions] line (memory_cards
sub-group). test_core_options.cpp gains Case 12 (round-trip on 5 NON-
default values) + Case 12b (defaults-when-unset, asserts the parity-
flipped Slot2 default).

Schema-fidelity check is intentionally RED after this commit — the
matching 5 host rows land in Task 3.
EOF
)"
```

---

## Task 3: Add 5 host rows under category="Memory Cards" + hub card

**Files:**
- Modify: `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`
- Modify: `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp`

This task is the host side — Phase 1/2's hub-card rule is mandatory in-phase: the new category must have a matching hub card by the end of this commit. Don't defer.

- [ ] **Step 1: Append 5 Memory Cards rows in pcsx2_libretro_adapter.cpp**

Edit `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`. Find the end of `settingsSchema()` (currently line 354 — the last `s.append(opt(...))` for `pcsx2_audio_muted`). Just before the trailing `return s;` (line 356), insert:

```cpp
    // SP7c Phase 3 — Memory Cards card.
    //
    // 5 rows under category="Memory Cards" mirroring the standalone PCSX2
    // dialog at cpp/src/adapters/pcsx2_adapter.cpp:986-1029. The 2 filename
    // rows (Slot1_Filename / Slot2_Filename) are dropped — libretro core
    // options are Combo-only, not free-form strings, so the filenames stay
    // hardcoded in the core's Settings.cpp.
    //
    // Slot2_Enable defaults to "enabled" matching standalone (was "disabled"
    // pre-Phase-3, a SP6 single-slot convention). Behavioral change is
    // mostly invisible — PCSX2 only auto-creates Mcd002.ps2 on first WRITE
    // to Slot 2, so users that don't actively use Slot 2 see no change.
    //
    // Value strings MUST match the core's CoreOptionsMemoryCards.cpp
    // byte-for-byte. The check_schema_fidelity.py target verifies this
    // mechanically.
    s.append(opt(
        "Memory Cards", "Memory Card Slots",
        "pcsx2_mc_slot1_enable", "Memory Card Slot 1", "enabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Inserts a virtual memory card into Slot 1. Stored as Mcd001.ps2 "
        "under the per-game memcards folder. Disabling prevents games "
        "from saving/loading via Slot 1. Takes effect on next launch."));

    s.append(opt(
        "Memory Cards", "Memory Card Slots",
        "pcsx2_mc_slot2_enable", "Memory Card Slot 2", "enabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Inserts a virtual memory card into Slot 2. Stored as Mcd002.ps2 "
        "under the per-game memcards folder. PCSX2 only auto-creates the "
        "file the first time a game writes to Slot 2. Takes effect on "
        "next launch."));

    s.append(opt(
        "Memory Cards", "Multitap",
        "pcsx2_mc_multitap1_slot2", "Multitap 1 - Slot 2", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Enables the second memory-card slot of Multitap 1. Only useful "
        "when a game supports Multitap 1 and you need additional save "
        "slots for extra players. Takes effect on next launch."));

    s.append(opt(
        "Memory Cards", "Multitap",
        "pcsx2_mc_multitap1_slot3", "Multitap 1 - Slot 3", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Enables the third memory-card slot of Multitap 1. Takes effect "
        "on next launch."));

    s.append(opt(
        "Memory Cards", "Multitap",
        "pcsx2_mc_multitap1_slot4", "Multitap 1 - Slot 4", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Enables the fourth memory-card slot of Multitap 1. Takes effect "
        "on next launch."));
```

- [ ] **Step 2: Add Memory Cards card to the hub**

Edit `RetroNest-Project/cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp`.

Replace the leading comment block (lines 13-17) and the existing makeCard sequence so the file reads:

```cpp
    // SP7b's three knobs (renderer / MTVU / FastBoot) sit under
    // category="Recommended"; SP7c Phase 1 added 15 rows under
    // category="Emulation"; SP7c Phase 2 added 5 rows under
    // category="Audio"; SP7c Phase 3 adds 5 rows under category="Memory
    // Cards". Phase 5 (full hub reorg per the spec) will add the
    // remaining cards.
    grid->addWidget(makeCard(QStringLiteral("\U0001F4A1"), "Recommended",
                             "GS renderer, multi-threaded VU1, fast boot",
                             countSettings("Recommended"), "Recommended"),
                    0, 0, 1, 3);

    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "Speed control, system, frame pacing",
                             countSettings("Emulation"), "Emulation"),
                    1, 0);

    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Volume, mute, buffer, sync mode",
                             countSettings("Audio"), "Audio"),
                    1, 1);

    grid->addWidget(makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards",
                             "Slot 1/2 enables, Multitap slots",
                             countSettings("Memory Cards"), "Memory Cards"),
                    1, 2);
```

(Emoji `\U0001F4BE` = 💾 floppy disk — visually distinct from Recommended's 💡, Emulation's 🎮, and Audio's 🔊. Both `category=` and the makeCard's category-id string MUST be the literal "Memory Cards" — that's what `countSettings` filters on and what the SettingDef rows use.)

- [ ] **Step 3: Build the host**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-universal
```
Expected: success.

- [ ] **Step 4: Run schema-fidelity check (now passes)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```
Expected: GREEN — "28 core keys / 28 host keys, byte-for-byte match" (or equivalent — 23 from Phase 1+2 plus 5 from Phase 3).

If the script reports any default-mismatch flags or extra-keys-on-either-side, STOP and reconcile — the core/host strings must be byte-identical. Common gotchas: trailing whitespace in tooltip strings, `Enabled` vs `enabled` casing in the value list (host uses `{label, value}` so the host's first-of-pair is the LABEL `"Enabled"` and the second is the VALUE `"enabled"` — the script compares the VALUE to the core's value-string).

- [ ] **Step 5: Commit (host side)**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp \
        cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 3 (host): Memory Cards card rows + hub card

5 s.append(opt(...)) rows under category="Memory Cards" mirroring the
core's CoreOptionsMemoryCards.cpp byte-for-byte. 2 rows under group
"Memory Card Slots" (Slot 1/2 enables, both default enabled — matches
standalone, was Slot 2 default disabled in pre-SP7c Settings.cpp); 3
rows under group "Multitap" (Multitap1_Slot{2,3,4}_Enable, all default
disabled).

Pcsx2LibretroCategoryHub gains the Memory Cards card at grid (1, 2),
completing row 1 alongside Emulation (1, 0) and Audio (1, 1). Phase 1/2
hub-card-mandatory-in-phase rule preserved.

The 2 standalone Slot{1,2}_Filename rows are dropped — libretro core
options are Combo-only (no free-form string input). Filenames stay
hardcoded in the core's Settings.cpp.

Schema fidelity 28 core / 28 host, byte-for-byte match.
EOF
)"
```

---

## Task 4: Universal lipo + copy to RetroNest cores dir

**Files:** none (build artifact handling)

- [ ] **Step 1: Build the x86_64 slice (Rosetta cmake)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
arch -x86_64 /usr/local/bin/cmake --build build-x86_64
```
Expected: success. The new TU (CoreOptionsMemoryCards.cpp) compiles cleanly via x86_64 toolchain too.

- [ ] **Step 2: lipo the universal dylib**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
# Locate the per-arch dylibs — exact name may be pcsx2_libretro.dylib or similar
ls build-arm64/pcsx2-libretro/*.dylib build-x86_64/pcsx2-libretro/*.dylib
# Then run the existing lipo command (mirror what Phase 2 used — there
# should be a script or documented one-liner from Phase 1/2 close-out).
```

If the project has a `tools/build_universal.sh` or equivalent, run it. Otherwise mirror the Phase 2 close-out exactly — same lipo invocation, same output path. Phase 1's lessons section noted: "Build dir + lipo paths confirmed for SP7c+SP10 universal builds: build-arm64 (default) + build-x86_64 (needs `arch -x86_64 /usr/local/bin/cmake`). Resources sync source is `pcsx2-libretro/bin/resources/` relative to repo root — NOT `pcsx2-libretro/pcsx2-libretro/bin/resources/`."

- [ ] **Step 3: Copy the universal dylib + (if changed) resources to RetroNest cores dir**

Copy the lipo'd universal dylib to wherever Phase 1/2's close-out put it (likely `RetroNest-Project/cpp/build-universal/Frameworks/` or the equivalent cores dir). Phase 3 added no new resources, so the resources sync from `pcsx2-libretro/bin/resources/` is unchanged from Phase 2 — only re-sync if the existing close-out script does so unconditionally.

- [ ] **Step 4: No commit** (build artifacts only)

---

## Task 5: Live smoke gate

**Files:** none (manual verification)

Memory cards have visible game-level effects but are slower to verify than audio (need to load a game, write a save, relaunch). The smoke gate is shorter than Phase 1 (which needed EE-rate OSD warnings) but longer than Phase 2 (which had audible mute/volume).

If launching RetroNest directly from Bash hits the codesign SIGKILL issue documented in Phase 2's lessons, use the Rosetta launch recipe:
```bash
APP=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app
arch -x86_64 env DYLD_FRAMEWORK_PATH="$APP/Contents/Frameworks" \
  QT_PLUGIN_PATH="$APP/Contents/PlugIns" "$APP/Contents/MacOS/RetroNest" 2>&1 | \
  tee /tmp/retronest-phase3-smoke.log
```

Otherwise, launch RetroNest from Finder and follow the system-log capture method used in Phase 1/2 close-out.

- [ ] **Step 1: Verify the dialog renders the new card**

Open RetroNest → emulator settings for `pcsx2-libretro` → confirm the hub now shows 4 cards: Recommended (top, full-width row 0), Emulation, Audio, Memory Cards (bottom row, 3 cards). Memory Cards card shows the floppy-disk icon (💾) and "5 settings" count.

- [ ] **Step 2: Verify the 5 rows are present and the defaults are correct**

Click into the Memory Cards card. Expected layout:
- Group "Memory Card Slots":
  - Memory Card Slot 1 — `Enabled`
  - Memory Card Slot 2 — `Enabled` ← was Disabled before Phase 3, parity-flipped intentionally
- Group "Multitap":
  - Multitap 1 - Slot 2 — `Disabled`
  - Multitap 1 - Slot 3 — `Disabled`
  - Multitap 1 - Slot 4 — `Disabled`

- [ ] **Step 3: Verify [CoreOptions] echo log includes a memory_cards line**

In the captured stdout/stderr, grep `[CoreOptions]`. Expected: 6 lines per launch — 4 Phase 1 lines + 1 Phase 2 audio line + 1 NEW Phase 3 memory_cards line of the form:
```
[CoreOptions] memory_cards: slot1=on slot2=on mt1_s2=off mt1_s3=off mt1_s4=off
```

- [ ] **Step 4: Slot 1 disable smoke**

Toggle `Memory Card Slot 1 → Disabled`. Save settings. Launch a save-supporting game (R&C 2 NTSC). Enter the in-game save menu. Expected: game reports "no memory card detected" or fails to write to Slot 1. Re-enable Slot 1 and relaunch — the game finds the card again. Confirm `[CoreOptions] memory_cards: slot1=off ...` echoes the off state during the disabled-toggle launch and `slot1=on ...` after re-enabling.

- [ ] **Step 5: Slot 2 toggle smoke**

Toggle `Memory Card Slot 2 → Disabled` then back to `Enabled`. Each launch's `[CoreOptions]` line should reflect the toggle. (Don't bother trying to force a Slot 2 write — most games default to Slot 1; verifying the dialog persists the toggle and the echo line tracks is sufficient.)

- [ ] **Step 6: Multitap toggle smoke (lightweight — schema persistence only)**

Toggle `Multitap 1 - Slot 2 → Enabled`. Relaunch. Expected: `[CoreOptions] memory_cards: ... mt1_s2=on ...` in the log. Reset to disabled. Full multitap-game smoke is overkill for Phase 3 parity — the regression risk is in the value reaching the INI, not in the gameplay behavior.

- [ ] **Step 7: Persistence across re-open smoke**

Close the dialog, reopen it. Expected: every toggle change made during this session is still selected. (Tests RetroNest's options.json round-trip + the host adapter's value-list match — schema-fidelity script catches this mechanically too, but eyes-on-dialog is the user-perceptible check.)

- [ ] **Step 8: Capture findings in the kickoff memory**

Update `~/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/sp7c_kickoff.md`:
- Move the Phase 3 kickoff section to a "Phase 3 ✅ SHIPPED 2026-05-13" header.
- Record the final commit hashes (3 expected: Task 1 scaffold, Task 2 core, Task 3 host).
- Note any lessons surfaced during execution (especially: did the Slot2_Enable default flip cause any user-visible surprise? did Settings.cpp's one-shot-block edit have any unexpected interaction with VMManager::SetDefaultSettings overwriting things? did the schema-fidelity script need any tweaks?).
- Add a "Phase 4 kickoff — start here in a fresh session" section with the same template (Phase 4 is Graphics, ~62 knobs — the biggest phase; flag the kBoolValues hardening question deferred from Phase 2).
- Update `MEMORY.md` index line to reflect Phase 3 shipped.

- [ ] **Step 9: No commit** (smoke + memory updates are not code commits)

---

## Phase 3 ground rules (carrying forward from Phase 2)

These are reminders, not new policy — the writing-plans process must keep them top-of-mind during subagent dispatch.

- **HUB CARD MANDATORY IN-PHASE.** Phase 1/2 rule. Task 3 covers it. NEVER defer hub expansion across a phase boundary.
- **`CORE_BLOCK_RE` only matches LITERAL `out.push_back({...})` form.** No lambda helpers in `AppendDefinitions`. With 5 bools sharing the same value list, the inline duplication is small (5 × ~5 lines = ~25 lines). Phase 4 (Graphics, 62 knobs) is the place to revisit hardening `CORE_BLOCK_RE` for `static constexpr kBoolValues[]` identifier refs.
- **No `/*dependsOn=*/` inline comments** in host `s.append(opt(...))` calls. None of the 5 Phase 3 rows declare dependsOn — standalone's `Slot1_Filename` depends on `Slot1_Enable`, but we drop the filename rows.
- **`opt(...)` lambda is 8-arg**: `(category, group, key, label, def, valuesAndLabels, tooltip, dependsOn={})`. None of the 5 Phase 3 rows pass `dependsOn`, so they all use the 7-arg form (relying on the default).
- **`v.reserve(N)` sizes to FINAL capacity.** Bumped 24 → 32 in Task 1 Step 4 (18 + 5 + 5 + terminator + headroom).
- **No `git push origin retronest-libretro`.** Fork has only `upstream` remote. Close-out is commit + lipo + copy.
- **Standalone test compile** needs `../CoreOptionsMemoryCards.cpp` added to the command (Task 1 Step 8 + Task 2 Step 7 already include it).
- **Trust `cmake --build` and `./test_core_options`, NOT clangd diagnostics.** compile_commands.json staleness false positives keep recurring across phases.
- **Rosetta launch recipe** (if launching RetroNest from Bash hits codesign SIGKILL): `arch -x86_64 env DYLD_FRAMEWORK_PATH="$APP/Contents/Frameworks" QT_PLUGIN_PATH="$APP/Contents/PlugIns" "$APP/Contents/MacOS/RetroNest"` against the universal RetroNest.app at `cpp/build-universal/`.

---

## Self-review notes (run after writing the plan)

- **Spec coverage:** the 5 knobs from the kickoff's "Concrete knob list" are all covered by Task 2 Steps 1-3 (definitions, parse, apply). The two filename rows are explicitly dropped with the libretro-Combo-only rationale documented in Task 3 Step 1's leading comment + the kickoff's "Knobs explicitly DROPPED" table. The Slot2_Enable default decision is resolved (choice A: parity-with-standalone, default `true`), justified inline in the plan + Task 2 Step 1's struct comment.
- **Hub card:** in Task 3 Step 2, in the same commit as the host rows — preserves the Phase 1/2 in-phase rule.
- **Settings.cpp split:** Task 2 Step 5 explicitly REMOVES the 2 Slot{1,2}_Enable writes from the one-shot block (the change Phase 1 Task 5 surfaced as the canonical pattern for "user knob now lives in ApplyDefaults") and KEEPS the 2 Slot{1,2}_Filename writes (combo-only constraint).
- **Reserve bump:** Task 1 Step 4 takes 24 → 32 with the Phase 2 followup-lesson rationale in the commit message.
- **Test compile command:** updated in test_core_options.cpp's header comment (Task 2 Step 6) AND in the ad-hoc Bash invocations (Tasks 1 Step 8, Task 2 Step 7).
- **Schema fidelity:** Task 2 Step 9 explicitly notes the expected RED state (host rows lag); Task 3 Step 4 verifies it goes GREEN at 28 / 28.
- **No placeholders:** every code block contains the actual content. The lipo + copy step (Task 4 Step 2) is the only one that defers to "mirror what Phase 2 did" — but the file-path hint and the build-x86_64 invocation are concrete; only the final cp/lipo command is left to mirror Phase 2's documented script.
- **Multitap1_Enable master toggle question (raised in kickoff):** investigated during plan writing — verified at `pcsx2/Pcsx2Config.cpp:1839-1840` that `MultitapPort1_Enabled` / `MultitapPort2_Enabled` live in the `Pcsx2` section (not `MemoryCards`) and are NOT exposed by standalone's Memory Cards card. They're managed by standalone's controllers settings instead. Phase 3 does not add them — out of scope for the Memory Cards card, parity-correct.
