# SP7c Phase 1 Implementation Plan — Emulation Card (15 new knobs)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the 15 remaining Emulation-card knobs that the standalone PCSX2 dialog already shows but pcsx2-libretro does not. Phase 0 shipped 2 of the 17 standalone Emulation rows (renderer, vuThread, EnableFastBoot) plus the aggregator pattern; this phase fills the gap to 17 total. No new architecture — Phase 1 is data-entry through the seam Phase 0 already built.

**Architecture:** Each new knob touches five places per the SP7c per-knob workflow:
1. `pcsx2-libretro/CoreOptionsEmulation.h` — append field to `struct Values`.
2. `pcsx2-libretro/CoreOptionsEmulation.cpp::AppendDefinitions` — append `retro_core_option_v2_definition` to `out`.
3. `pcsx2-libretro/CoreOptionsEmulation.cpp::Parse` — append `query()`-and-match branch.
4. `pcsx2-libretro/CoreOptionsEmulation.cpp::ApplyDefaults` — append `g_si.Set{Int,Bool,Float}Value(...)` line.
5. `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp::settingsSchema` — append `s.append(opt(...))` row under `category="Emulation"`.

Then `tools/check_schema_fidelity.py` mechanically verifies the core/host pair is byte-identical, and `tools/test_core_options.cpp` Case 7 auto-asserts structural sanity for every entry without a hand-written test.

**Tech Stack:**
- pcsx2-libretro: C++20, clang, libretro.h v2 core-options API.
- RetroNest-Project: C++20, Qt 6 (read/append-only on `settingsSchema`).
- Schema check: Python 3 (`tools/check_schema_fidelity.py` exists from Phase 0).
- Build: CMake (`build-arm64` is the dev build; universal lipo + RetroNest copy at close-out).
- Standalone unit test: `clang++` with `-DCORE_OPTIONS_TEST_ONLY`.

**Repo locations:**
- pcsx2-libretro: `/Users/mark/Documents/Projects/pcsx2-libretro/` (branch `retronest-libretro`, currently at HEAD `065ae2024`).
- RetroNest-Project: `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`, currently at HEAD `c26b205`).

**Scope guard:**
- 15 net new core option keys, 15 net new host SettingDef rows. No other categories touched.
- Phase 1 does NOT add the Recommended card (Phase 5), Graphics card (Phase 4), Audio card (Phase 2), Memory Cards card (Phase 3), nor any sub-tab handling. The host hub stays at the single-card layout shipped by SP7b/Phase 0; this phase just thickens that card's count.
- pcsx2-libretro fork has NO `origin` remote — close-out ends at `commit + lipo + copy to RetroNest cores dir`. **Do not include `git push origin` anywhere in this plan.** (Phase 0 made this mistake and corrected at close-out.)

**Out-of-phase scope:**
- The 14 Recommended-card rows that re-display these new Emulation keys arrive in Phase 5 with the hub expansion. Don't anticipate them in this phase's adapter rows.
- `Pcsx2LibretroCategoryHub` stays 1-card.
- `Pcsx2LibretroSettingsDialog` keeps `hasSubTabs=false`.

---

## File structure

### Changed in this phase

| File | Action | Responsibility |
|---|---|---|
| `pcsx2-libretro/CoreOptionsEmulation.h` | Modify | Add 15 fields to `struct Values`. |
| `pcsx2-libretro/CoreOptionsEmulation.cpp` | Modify | Add 15 entries to `AppendDefinitions`; add 15 parse branches to `Parse`; add 15 `SetXValue` lines to `ApplyDefaults`. |
| `pcsx2-libretro/Settings.cpp` | Modify (1 line removal) | Remove the now-redundant `g_si.SetBoolValue("EmuCore", "HostFs", false);` line (line 234). HostFs becomes user-controlled via `ApplyDefaults`. |
| `pcsx2-libretro/tools/test_core_options.cpp` | Modify | Loosen Case 6's hard-coded count assertion (becomes regression sentinel for SP7b's first 3 keys); add 3 new hand-written round-trip cases (Case 8/9/10) — one per knob sub-group. |
| `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` | Modify | Append 15 rows to `settingsSchema()` under `category="Emulation"`. Extend `opt(...)` helper to take a `category` arg, OR add a parallel helper for the Emulation card (your call at Task 2 step 3). |

### Untouched in this phase

- `pcsx2-libretro/CoreOptions.h` and `CoreOptions.cpp` — Phase 0's aggregator is unchanged.
- `pcsx2-libretro/LibretroFrontend.cpp` — already wires `params.fast_boot = resolved.emulation.fast_boot` at line 512.
- `pcsx2-libretro/CMakeLists.txt` — no new files.
- `pcsx2-libretro/tools/check_schema_fidelity.py` — already handles any number of core/host rows; no script changes.
- All other RetroNest-Project files (dialog, hub, ui, etc.).

---

## Knob inventory (15 total)

Three sub-groups. Each row is one core option key + one host SettingDef row.

### Sub-group A — Speed Control (3 knobs)

INI section/key cross-referenced against `RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp:220-232`.

| Core option key | INI section | INI key | Type | Default | Values (core's `value` strings) |
|---|---|---|---|---|---|
| `pcsx2_normal_speed`      | `Framerate` | `NominalScalar` | Combo (17) | `"1"`   | `0.02, 0.1, 0.25, 0.5, 0.75, 0.9, 1, 1.1, 1.2, 1.5, 1.75, 2, 3, 4, 5, 10, 0` |
| `pcsx2_fast_forward_speed`| `Framerate` | `TurboScalar`   | Combo (17) | `"2"`   | same value list as above |
| `pcsx2_slow_motion_speed` | `Framerate` | `SlomoScalar`   | Combo (17) | `"0.5"` | same value list as above |

The 17 display labels match the standalone exactly (see `pcsx2_adapter.cpp:200-218` — `"100% [60 FPS (NTSC) / 50 FPS (PAL)]"` etc.). The trailing `"Unlimited"` maps to value `"0"`.

INI value-format note: PCSX2 writes floats via `StringUtil::ToChars` (shortest representation — `"1"` not `"1.000000"`, `"0.5"` not `"0.500"`). The core's option values MUST match this exact form or round-trip fails. The list above is the canonical form.

### Sub-group B — System Settings (7 knobs)

Cross-referenced against `pcsx2_adapter.cpp:238-293`. `vuThread` and `EnableFastBoot` already shipped in Phase 0 — both are skipped here.

| Core option key | INI section | INI key | Type | Default | Values |
|---|---|---|---|---|---|
| `pcsx2_ee_cycle_rate`     | `EmuCore/Speedhacks` | `EECycleRate`               | Combo (7) | `"0"`     | `-3, -2, -1, 0, 1, 2, 3` |
| `pcsx2_ee_cycle_skip`     | `EmuCore/Speedhacks` | `EECycleSkip`               | Combo (4) | `"0"`     | `0, 1, 2, 3` |
| `pcsx2_thread_pinning`    | `EmuCore`            | `EnableThreadPinning`       | Bool      | `disabled` | `enabled, disabled` |
| `pcsx2_cheats`            | `EmuCore`            | `EnableCheats`              | Bool      | `disabled` | `enabled, disabled` |
| `pcsx2_host_fs`           | `EmuCore`            | `HostFs`                    | Bool      | `disabled` | `enabled, disabled` |
| `pcsx2_cdvd_precache`     | `EmuCore`            | `CdvdPrecache`              | Bool      | `disabled` | `enabled, disabled` |
| `pcsx2_fast_boot_ff`      | `EmuCore`            | `EnableFastBootFastForward` | Bool      | `disabled` | `enabled, disabled` |

EE-rate labels (from standalone):
```
{"-3", "50% (Underclock)"}, {"-2", "60% (Underclock)"}, {"-1", "75% (Underclock)"},
{"0",  "100% (Normal Speed)"},
{"1",  "130% (Overclock)"}, {"2",  "180% (Overclock)"}, {"3",  "300% (Overclock)"}
```

EE-skip labels:
```
{"0", "Disabled"}, {"1", "Mild Underclock"},
{"2", "Moderate Underclock"}, {"3", "Maximum Underclock"}
```

### Sub-group C — Frame Pacing / Latency Control (5 knobs)

Cross-referenced against `pcsx2_adapter.cpp:297-324`.

| Core option key | INI section | INI key | Type | Default | Values |
|---|---|---|---|---|---|
| `pcsx2_vsync_queue_size`     | `EmuCore/GS` | `VsyncQueueSize`       | Combo (4) | `"2"`     | `0, 1, 2, 3` |
| `pcsx2_sync_to_host_rr`      | `EmuCore/GS` | `SyncToHostRefreshRate`| Bool      | `disabled` | `enabled, disabled` |
| `pcsx2_vsync`                | `EmuCore/GS` | `VsyncEnable`          | Bool      | `disabled` | `enabled, disabled` |
| `pcsx2_use_vsync_timing`     | `EmuCore/GS` | `UseVSyncForTiming`    | Bool      | `disabled` | `enabled, disabled` |
| `pcsx2_skip_duplicate_frames`| `EmuCore/GS` | `SkipDuplicateFrames`  | Bool      | `disabled` | `enabled, disabled` |

VsyncQueueSize labels:
```
{"0", "Optimal (Frame Pacing)"}, {"1", "1 frame"},
{"2", "2 frames"}, {"3", "3 frames"}
```

**Libretro-pacing caveat:** the frontend (RetroNest) owns presentation in libretro mode — `VsyncEnable` and `UseVSyncForTiming` may be cosmetic on this build. We expose them anyway (for parity with the standalone dialog) and document the caveat in the tooltip.

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
Expected: working tree clean (or only `?? pcsx2-libretro/tools/` untracked test binaries). HEAD at `065ae2024` ("SP7c Phase 0 Task 8: data-driven structural test pass for BuildDefinitions") or a descendant on `retronest-libretro`.

If anything is dirty, stop and resolve before proceeding.

- [ ] **Step 2: Confirm clean working tree on RetroNest-Project**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git status --short
git log --oneline -3
```
Expected: HEAD at `c26b205` ("docs: SP7c Phase 0 plan") or descendant on `main`. Only `?? cpp/build-*` untracked is fine. No staged or unstaged code changes.

- [ ] **Step 3: Baseline-pass the existing test (Phase 0 state)**

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

If non-zero, stop — Phase 0's baseline is broken and Phase 1 can't proceed.

- [ ] **Step 4: Baseline-pass the schema fidelity check**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```

Expected output ends with:
```
Schema fidelity OK: 3 core keys, 3 host keys, byte-for-byte match.
```

- [ ] **Step 5: Confirm pcsx2_libretro target builds**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`.

- [ ] **Step 6: No commit. Baseline only.**

---

## Task 1: Loosen the Case 6 count assertion so it doesn't break per-knob

The current Case 6 hard-codes `count == 4` (3 SP7b knobs + terminator). Once Phase 1 adds knobs, that count breaks. Rather than updating the literal four times across three sub-groups, loosen Case 6 to a regression sentinel: the SP7b 3 keys still appear at indices [0..2], terminator still at the end, total count strictly greater than 4. Case 7 (structural sweep) already covers per-knob sanity.

**Files:**
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (lines 170-184 — Case 6 block).

- [ ] **Step 1: Open `test_core_options.cpp` and replace the Case 6 body**

Replace lines 170-185 (the entire Case 6 block including the closing brace) with:

```cpp
    // -------- Case 6: BuildDefinitions retains SP7b's 3 keys at the head, terminator at the tail --------
    //
    // SP7c Phase 1+ adds more knobs; this case is a regression sentinel,
    // not a count fixture. Per-knob sanity is covered by Case 7's data-driven
    // sweep. Three invariants:
    //   1. The first 3 keys are still pcsx2_renderer / pcsx2_mtvu / pcsx2_fast_boot
    //      in that order (SP7b call-site ordering must not regress).
    //   2. Total entries strictly greater than 4 once Phase 1 lands (>= 19 after
    //      all 15 Phase-1 knobs); Phase 0 leaves it at 4. We assert >= 4 here
    //      and let Case 7 catch sub-entry shape.
    //   3. The final entry is the libretro terminator (key == nullptr).
    {
        const auto& defs = BuildDefinitions();
        check_bool("Case 6 size >= 4 (SP7b minimum)",
                   defs.size() >= 4, true);

        if (defs.size() >= 4) {
            check_bool("Case 6 [0].key = pcsx2_renderer",
                       defs[0].key && std::strcmp(defs[0].key, "pcsx2_renderer") == 0, true);
            check_bool("Case 6 [1].key = pcsx2_mtvu",
                       defs[1].key && std::strcmp(defs[1].key, "pcsx2_mtvu") == 0, true);
            check_bool("Case 6 [2].key = pcsx2_fast_boot",
                       defs[2].key && std::strcmp(defs[2].key, "pcsx2_fast_boot") == 0, true);
            const auto& last = defs.back();
            check_bool("Case 6 last entry is terminator",
                       last.key == nullptr, true);
        }
    }
```

- [ ] **Step 2: Rebuild + run the test**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: `0 failure(s)`. Same 3 SP7b knobs assertions still pass; the count assertion is now a `>= 4` floor.

- [ ] **Step 3: Commit (core-side only)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 1 Task 1: loosen Case 6 to regression sentinel

Phase 0 hardcoded the BuildDefinitions count at 4 (3 SP7b knobs +
terminator). Phase 1+ grows that count, so Case 6 becomes a sentinel
for the SP7b head ordering rather than a fixture for total count.
Case 7's data-driven sweep already enforces per-knob sanity.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Sub-group A — Speed Control (3 knobs)

This task adds NominalScalar / TurboScalar / SlomoScalar end-to-end. Float-typed; uses `g_si.SetFloatValue`.

**Files:**
- Modify: `pcsx2-libretro/CoreOptionsEmulation.h`
- Modify: `pcsx2-libretro/CoreOptionsEmulation.cpp`
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (Case 8 — round-trip)
- Modify: `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`

### 2.1 — Core side: extend `Values`

- [ ] **Step 1: Open `CoreOptionsEmulation.h` and extend `struct Values`**

Replace the existing `struct Values { ... };` block (lines 26-31) with:

```cpp
struct Values
{
    int  renderer  = -1;    // GSRendererType: -1=Auto, 17=Metal, 13=SW, 11=Null
    bool mtvu      = true;  // EmuCore/Speedhacks/vuThread
    bool fast_boot = true;  // EmuCore/EnableFastBoot AND VMBootParameters.fast_boot

    // --- SP7c Phase 1: Speed Control (3 knobs) ---
    // Framerate scalars. INI writes as float via SetFloatValue;
    // PCSX2's ToChars produces shortest-form strings ("1", "0.5", "2", etc.).
    // Value 0.0 maps to "Unlimited".
    float normal_speed       = 1.0f;  // Framerate/NominalScalar
    float fast_forward_speed = 2.0f;  // Framerate/TurboScalar
    float slow_motion_speed  = 0.5f;  // Framerate/SlomoScalar
};
```

### 2.2 — Core side: extend `AppendDefinitions`

- [ ] **Step 2: Open `CoreOptionsEmulation.cpp` and append three definitions inside `AppendDefinitions`**

After the existing `pcsx2_fast_boot` block (ends at line 88 with `});`), insert the three new blocks before the closing `}` of `AppendDefinitions` (line 89). Use a shared local lambda for the 17-entry speed list to keep duplication out:

```cpp
    // SP7c Phase 1 — Speed Control sub-group.
    //
    // All three scalars share the same 17-preset value list. Labels are
    // copied verbatim from the standalone PCSX2 dialog
    // (RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp:200-218) so
    // users see identical wording between the two PCSX2 variants.
    //
    // INI form: PCSX2 writes via StringUtil::ToChars (shortest-form). "1"
    // not "1.0", "0.5" not "0.50". Our option values must match this exact
    // form to round-trip cleanly.
    static constexpr retro_core_option_value kSpeedValues[] = {
        { "0.02", "2% [1 FPS (NTSC) / 1 FPS (PAL)]" },
        { "0.1",  "10% [6 FPS (NTSC) / 5 FPS (PAL)]" },
        { "0.25", "25% [15 FPS (NTSC) / 12 FPS (PAL)]" },
        { "0.5",  "50% [30 FPS (NTSC) / 25 FPS (PAL)]" },
        { "0.75", "75% [45 FPS (NTSC) / 37 FPS (PAL)]" },
        { "0.9",  "90% [54 FPS (NTSC) / 45 FPS (PAL)]" },
        { "1",    "100% [60 FPS (NTSC) / 50 FPS (PAL)]" },
        { "1.1",  "110% [66 FPS (NTSC) / 55 FPS (PAL)]" },
        { "1.2",  "120% [72 FPS (NTSC) / 60 FPS (PAL)]" },
        { "1.5",  "150% [90 FPS (NTSC) / 75 FPS (PAL)]" },
        { "1.75", "175% [105 FPS (NTSC) / 87 FPS (PAL)]" },
        { "2",    "200% [120 FPS (NTSC) / 100 FPS (PAL)]" },
        { "3",    "300% [180 FPS (NTSC) / 150 FPS (PAL)]" },
        { "4",    "400% [240 FPS (NTSC) / 200 FPS (PAL)]" },
        { "5",    "500% [300 FPS (NTSC) / 250 FPS (PAL)]" },
        { "10",   "1000% [600 FPS (NTSC) / 500 FPS (PAL)]" },
        { "0",    "Unlimited" },
        { nullptr, nullptr },
    };

    auto push_speed = [&out](const char* key,
                             const char* desc,
                             const char* info,
                             const char* default_value) {
        retro_core_option_v2_definition d{};
        d.key             = key;
        d.desc            = desc;
        d.desc_categorized = nullptr;
        d.info            = info;
        d.info_categorized = nullptr;
        d.category_key    = nullptr;
        std::memcpy(d.values, kSpeedValues, sizeof(kSpeedValues));
        d.default_value   = default_value;
        out.push_back(d);
    };

    push_speed("pcsx2_normal_speed",
        "Normal Speed",
        "Target emulation speed during normal gameplay (relative to PS2's "
        "native rate). 100% is real-time. Lower for handheld-style throttling, "
        "higher for fast-forwarding through cutscenes.",
        "1");

    push_speed("pcsx2_fast_forward_speed",
        "Fast-Forward Speed",
        "Target speed when fast-forward is engaged. The frontend's "
        "fast-forward toggle scales emulation by this factor.",
        "2");

    push_speed("pcsx2_slow_motion_speed",
        "Slow-Motion Speed",
        "Target speed when slow-motion is engaged. The frontend's "
        "slow-motion toggle scales emulation by this factor.",
        "0.5");
```

Also add `#include <cstring>` at the top of `CoreOptionsEmulation.cpp` if not already present (memcpy is used). It IS already present at line 22 — no change needed.

### 2.3 — Core side: extend `Parse`

- [ ] **Step 3: In `CoreOptionsEmulation.cpp`, append three parse branches to `Parse`**

After the existing `pcsx2_fast_boot` branch (ends at line 119), insert before the closing `}` of `Parse`:

```cpp
    // SP7c Phase 1 — Speed Control parsing.
    // Values are float-encoded strings. strtof handles the canonical
    // "1", "0.5", "2", "10", "0" (Unlimited) forms. On bad parse, leave
    // the field at its Values{} default and WARN.
    auto parse_speed = [&query](const char* key, float& out_field, float fallback) {
        if (const char* v = query(key)) {
            char* end = nullptr;
            const float parsed = std::strtof(v, &end);
            if (end == v) {
                FrontendLog(RETRO_LOG_WARN,
                    "[CoreOptions] Unparseable %s='%s'; keeping default %.3f",
                    key, v, fallback);
                out_field = fallback;
            } else {
                out_field = parsed;
            }
        }
    };

    parse_speed("pcsx2_normal_speed",       out.normal_speed,       1.0f);
    parse_speed("pcsx2_fast_forward_speed", out.fast_forward_speed, 2.0f);
    parse_speed("pcsx2_slow_motion_speed",  out.slow_motion_speed,  0.5f);
```

Add `#include <cstdlib>` at the top of `CoreOptionsEmulation.cpp` if not already present (for `std::strtof`). The file currently has `#include <cstring>` at line 22; add `#include <cstdlib>` next to it:

```cpp
#include <cstdlib>
#include <cstring>
```

### 2.4 — Core side: extend `ApplyDefaults`

- [ ] **Step 4: In `CoreOptionsEmulation.cpp`, append three SetFloatValue lines inside `ApplyDefaults`**

After the existing `vuThread` line (line 143), inside the `#ifndef CORE_OPTIONS_TEST_ONLY` block, before the closing `}`:

```cpp
    // SP7c Phase 1 — Speed Control. PCSX2's Framerate section reads these
    // floats directly into Pcsx2Config; SetFloatValue routes through
    // ToChars so the on-disk INI string matches our option-values list
    // ("1", "0.5", "2", "0" for Unlimited).
    si.SetFloatValue("Framerate", "NominalScalar", v.normal_speed);
    si.SetFloatValue("Framerate", "TurboScalar",   v.fast_forward_speed);
    si.SetFloatValue("Framerate", "SlomoScalar",   v.slow_motion_speed);
```

### 2.5 — Core side: build + test

- [ ] **Step 5: Rebuild standalone test and run**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: `0 failure(s)`. Case 6 still passes (head ordering unchanged), Case 7's structural sweep now covers 6 entries (3 SP7b + 3 new).

- [ ] **Step 6: Add Case 8 — round-trip Speed Control**

Open `pcsx2-libretro/tools/test_core_options.cpp`. After Case 7's closing `}` (line ~232), before `std::printf("\n%d failure(s)\n", failures);`, insert:

```cpp
    // -------- Case 8: Speed Control round-trip --------
    //
    // SP7c Phase 1 representative test for the Speed Control sub-group.
    // We pick one knob per sub-group (Case 8/9/10) rather than testing all
    // 15 individually — Case 7's structural sweep already proves every
    // entry has well-formed values/default; this case proves the parse
    // path's float conversion works end-to-end.
    fake::reset();
    fake::variables["pcsx2_renderer"]            = "auto";
    fake::variables["pcsx2_mtvu"]                = "enabled";
    fake::variables["pcsx2_fast_boot"]           = "enabled";
    fake::variables["pcsx2_normal_speed"]        = "1.5";
    fake::variables["pcsx2_fast_forward_speed"]  = "4";
    fake::variables["pcsx2_slow_motion_speed"]   = "0.25";

    r = ReadResolved(&fake_env_cb);
    {
        // Float comparisons: literal "1.5" → 1.5f exactly under IEEE 754.
        const bool ns_ok  = r.emulation.normal_speed       == 1.5f;
        const bool ffs_ok = r.emulation.fast_forward_speed == 4.0f;
        const bool sms_ok = r.emulation.slow_motion_speed  == 0.25f;
        check_bool("Case 8 normal_speed=1.5",        ns_ok,  true);
        check_bool("Case 8 fast_forward_speed=4",    ffs_ok, true);
        check_bool("Case 8 slow_motion_speed=0.25",  sms_ok, true);
    }

    // Unparseable string falls back to default 1.0
    fake::reset();
    fake::variables["pcsx2_normal_speed"] = "not-a-number";
    r = ReadResolved(&fake_env_cb);
    check_bool("Case 8 garbled normal_speed → default 1.0",
               r.emulation.normal_speed == 1.0f, true);
```

- [ ] **Step 7: Re-compile and run**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: `0 failure(s)`.

- [ ] **Step 8: Confirm pcsx2_libretro target still builds**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`. If clangd diagnostics complain about PCSX2 headers ("file not found" / "C++17 extension"), ignore them — Phase 0 lessons surfaced this as a recurring false positive when the actual CMake build is green.

- [ ] **Step 9: Run schema-fidelity check — EXPECT FAILURE**

Run:
```bash
cmake --build build-arm64 --target check_schema_fidelity
```

Expected: exit code 1 with output like:
```
SCHEMA DRIFT DETECTED:
  - core declares key 'pcsx2_normal_speed' with no host row
  - core declares key 'pcsx2_fast_forward_speed' with no host row
  - core declares key 'pcsx2_slow_motion_speed' with no host row

3 drift entries; check both sides match exactly.
```

This is the expected intermediate state — host adapter hasn't been updated yet. We commit core-side and then update host.

- [ ] **Step 10: Commit core-side Speed Control**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsEmulation.h \
        pcsx2-libretro/CoreOptionsEmulation.cpp \
        pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 1 Task 2 (core): Speed Control knobs

Adds NominalScalar / TurboScalar / SlomoScalar as
pcsx2_normal_speed / pcsx2_fast_forward_speed / pcsx2_slow_motion_speed
core options. All three share the standalone PCSX2 dialog's 17-preset
value list (0.02x–10x + Unlimited). Float-typed Parse via strtof;
unparseable values fall back to defaults with a WARN log. ApplyDefaults
writes via SetFloatValue so the INI form matches PCSX2's ToChars output
("1", "0.5", "2", "0").

test_core_options.cpp Case 8 covers the Speed Control round-trip plus
the strtof fallback behavior.

Schema-fidelity check will report drift against the host adapter until
the matching host PR lands; that's expected.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### 2.6 — Host side: extend `settingsSchema`

- [ ] **Step 11: Open `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` and extend the `opt` helper**

The existing `opt(...)` helper (lines 75-91) hard-codes `category = "Recommended"` and `group = "Emulation"`. We need rows under `category = "Emulation"` instead. Modify the lambda to take a category parameter, defaulting to "Recommended" for SP7b backward-compat — or add a parallel `optEmu` helper. We'll do the parameter approach since the SP7b rows will move out of "Recommended" in Phase 5 anyway.

Replace the `opt` lambda at lines 75-91 with:

```cpp
    auto opt = [](const QString& category,
                  const QString& group,
                  const QString& key,
                  const QString& label,
                  const QString& def,
                  const QVector<QPair<QString,QString>>& valuesAndLabels,
                  const QString& tooltip,
                  const QString& dependsOn = {}) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.category = category;
        d.subcategory = "";
        d.group = group;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.tooltip = tooltip;
        d.type = SettingDef::Combo;
        d.options = valuesAndLabels;
        d.dependsOn = dependsOn;
        return d;
    };
```

Update the existing 3 SP7b `s.append(opt(...))` calls (lines 93-120) to pass the new `category` + `group` args. They stay under "Recommended"/"Emulation" for now (Phase 5 will reorganize). Edit each in turn:

Existing SP7b row 1:
```cpp
    s.append(opt(
        "pcsx2_renderer", "GS Renderer", "auto",
        {{"Auto", "auto"}, ...
```

becomes:

```cpp
    s.append(opt(
        "Recommended", "Emulation",
        "pcsx2_renderer", "GS Renderer", "auto",
        {{"Auto", "auto"}, ...
```

Repeat for `pcsx2_mtvu` and `pcsx2_fast_boot`.

- [ ] **Step 12: Append the 3 Speed Control rows**

After the existing `pcsx2_fast_boot` block (around line 120), before `return s;`, insert:

```cpp
    // SP7c Phase 1 — Speed Control (sub-group A of the Emulation card).
    // Three Framerate scalars; values list mirrors the standalone PCSX2
    // dialog exactly (RetroNest's cpp/src/adapters/pcsx2_adapter.cpp:200-218).
    const QVector<QPair<QString,QString>> speedOptions = {
        {"2% [1 FPS (NTSC) / 1 FPS (PAL)]",       "0.02"},
        {"10% [6 FPS (NTSC) / 5 FPS (PAL)]",      "0.1"},
        {"25% [15 FPS (NTSC) / 12 FPS (PAL)]",    "0.25"},
        {"50% [30 FPS (NTSC) / 25 FPS (PAL)]",    "0.5"},
        {"75% [45 FPS (NTSC) / 37 FPS (PAL)]",    "0.75"},
        {"90% [54 FPS (NTSC) / 45 FPS (PAL)]",    "0.9"},
        {"100% [60 FPS (NTSC) / 50 FPS (PAL)]",   "1"},
        {"110% [66 FPS (NTSC) / 55 FPS (PAL)]",   "1.1"},
        {"120% [72 FPS (NTSC) / 60 FPS (PAL)]",   "1.2"},
        {"150% [90 FPS (NTSC) / 75 FPS (PAL)]",   "1.5"},
        {"175% [105 FPS (NTSC) / 87 FPS (PAL)]",  "1.75"},
        {"200% [120 FPS (NTSC) / 100 FPS (PAL)]", "2"},
        {"300% [180 FPS (NTSC) / 150 FPS (PAL)]", "3"},
        {"400% [240 FPS (NTSC) / 200 FPS (PAL)]", "4"},
        {"500% [300 FPS (NTSC) / 250 FPS (PAL)]", "5"},
        {"1000% [600 FPS (NTSC) / 500 FPS (PAL)]","10"},
        {"Unlimited", "0"},
    };

    s.append(opt(
        "Emulation", "Speed Control",
        "pcsx2_normal_speed", "Normal Speed", "1",
        speedOptions,
        "Target emulation speed during normal gameplay (relative to PS2's "
        "native rate). 100% is real-time. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "Speed Control",
        "pcsx2_fast_forward_speed", "Fast-Forward Speed", "2",
        speedOptions,
        "Target speed when fast-forward is engaged. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "Speed Control",
        "pcsx2_slow_motion_speed", "Slow-Motion Speed", "0.5",
        speedOptions,
        "Target speed when slow-motion is engaged. Takes effect on next launch."));
```

- [ ] **Step 13: Run schema-fidelity check — EXPECT PASS**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```

Expected:
```
Schema fidelity OK: 6 core keys, 6 host keys, byte-for-byte match.
```

If drift is reported, fix the offending side until exact match.

- [ ] **Step 14: Build RetroNest-Project**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -5
```

(Adjust build dir name if RetroNest's local convention differs — `cpp/build-arm64` is the standard arm64 dev build.)

Expected: clean build. The new arguments to the `opt` lambda are forward-compat with the existing SP7b call sites (now updated to pass category/group explicitly).

- [ ] **Step 15: Commit host-side Speed Control**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 1 Task 2 (host): Speed Control rows

Adds NominalScalar / TurboScalar / SlomoScalar SettingDef rows under
category="Emulation" group="Speed Control" via the LibretroOption
storage path. Keys + values match
pcsx2-libretro/CoreOptionsEmulation.cpp's kSpeedValues exactly so
OptionsStore::load does not silently drop user values.

Extends the local opt() helper with category + group + dependsOn
parameters; SP7b's existing 3 rows updated to pass category="Recommended"
group="Emulation" explicitly (Phase 5 will reorganize them when the
Recommended card lands).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Sub-group B — System Settings (7 knobs)

Same shape as Task 2. Two Combos (EECycleRate / EECycleSkip) plus five Bools (ThreadPinning / Cheats / HostFs / CdvdPrecache / FastBootFastForward).

**Files:**
- Modify: `pcsx2-libretro/CoreOptionsEmulation.h`
- Modify: `pcsx2-libretro/CoreOptionsEmulation.cpp`
- Modify: `pcsx2-libretro/tools/test_core_options.cpp` (Case 9 — round-trip)
- Modify: `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`

### 3.1 — Core side: extend `Values`

- [ ] **Step 1: In `CoreOptionsEmulation.h`, extend `struct Values`**

After the Speed Control fields added in Task 2, before the closing `};`:

```cpp
    // --- SP7c Phase 1: System Settings (7 new knobs) ---
    // vuThread (MTVU) and fast_boot already declared above; not duplicated.
    int  ee_cycle_rate  = 0;     // EmuCore/Speedhacks/EECycleRate (-3..+3)
    int  ee_cycle_skip  = 0;     // EmuCore/Speedhacks/EECycleSkip (0..3)
    bool thread_pinning = false; // EmuCore/EnableThreadPinning
    bool cheats         = false; // EmuCore/EnableCheats
    bool host_fs        = false; // EmuCore/HostFs
    bool cdvd_precache  = false; // EmuCore/CdvdPrecache
    bool fast_boot_ff   = false; // EmuCore/EnableFastBootFastForward
```

### 3.2 — Core side: extend `AppendDefinitions`

- [ ] **Step 2: In `CoreOptionsEmulation.cpp::AppendDefinitions`, append 7 definitions**

After the three Speed Control blocks (push_speed calls), before the closing `}`:

```cpp
    // SP7c Phase 1 — System Settings sub-group.
    out.push_back({
        "pcsx2_ee_cycle_rate",
        "EE Cycle Rate",
        nullptr,
        "Underclocks or overclocks the emulated Emotion Engine CPU. "
        "Negative values reduce work-per-cycle (compatibility/perf tradeoff); "
        "positive values speed CPU-bound games at the cost of timing accuracy. "
        "Most games should stay at 100%.",
        nullptr,
        nullptr,
        {
            { "-3", "50% (Underclock)" },
            { "-2", "60% (Underclock)" },
            { "-1", "75% (Underclock)" },
            { "0",  "100% (Normal Speed)" },
            { "1",  "130% (Overclock)" },
            { "2",  "180% (Overclock)" },
            { "3",  "300% (Overclock)" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_ee_cycle_skip",
        "EE Cycle Skipping",
        nullptr,
        "Makes the emulated Emotion Engine skip cycles. Stronger underclock "
        "than EE Cycle Rate; can recover frame-rate in slow scenes at the "
        "cost of visible glitches. Leave Disabled unless a specific game "
        "needs it.",
        nullptr,
        nullptr,
        {
            { "0", "Disabled" },
            { "1", "Mild Underclock" },
            { "2", "Moderate Underclock" },
            { "3", "Maximum Underclock" },
            { nullptr, nullptr },
        },
        "0",
    });

    // Bools reuse SP7b's enabled/disabled value pair pattern.
    static constexpr retro_core_option_value kBoolValues[] = {
        { "enabled",  "Enabled" },
        { "disabled", "Disabled" },
        { nullptr,    nullptr },
    };

    auto push_bool = [&out](const char* key,
                            const char* desc,
                            const char* info,
                            const char* default_value) {
        retro_core_option_v2_definition d{};
        d.key             = key;
        d.desc            = desc;
        d.desc_categorized = nullptr;
        d.info            = info;
        d.info_categorized = nullptr;
        d.category_key    = nullptr;
        std::memcpy(d.values, kBoolValues, sizeof(kBoolValues));
        d.default_value   = default_value;
        out.push_back(d);
    };

    push_bool("pcsx2_thread_pinning",
        "Thread Pinning",
        "Pin emulation threads to specific CPU cores. Can reduce stutter "
        "on systems with heterogeneous cores (e.g. Apple Silicon "
        "performance/efficiency split). Default off.",
        "disabled");

    push_bool("pcsx2_cheats",
        "Enable Cheats",
        "Load pnach cheat files from the cheats/ resource directory on "
        "game launch. Off by default; the in-game OSD logs each loaded "
        "patch line.",
        "disabled");

    push_bool("pcsx2_host_fs",
        "Enable Host Filesystem",
        "Allow the emulated PS2 to read files from the host filesystem "
        "(homebrew-only feature; retail games never use it). Off by "
        "default.",
        "disabled");

    push_bool("pcsx2_cdvd_precache",
        "CDVD Precache",
        "Load the entire disc image into RAM before booting. Eliminates "
        "in-game disc-read stutter at the cost of extra memory usage and "
        "a slower initial boot. Off by default.",
        "disabled");

    push_bool("pcsx2_fast_boot_ff",
        "Fast-Forward Through BIOS",
        "When Fast Boot is enabled, also auto-fast-forward the brief "
        "BIOS boot animation. Has no effect when Fast Boot is disabled.",
        "disabled");
```

### 3.3 — Core side: extend `Parse`

- [ ] **Step 3: In `CoreOptionsEmulation.cpp::Parse`, append parse branches**

After the Speed Control parse_speed calls, before the closing `}`:

```cpp
    // SP7c Phase 1 — System Settings parsing.
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

    auto parse_bool = [&query](const char* key, bool& out_field) {
        if (const char* v = query(key))
            out_field = (std::strcmp(v, "enabled") == 0);
    };

    parse_int("pcsx2_ee_cycle_rate", out.ee_cycle_rate, 0);
    parse_int("pcsx2_ee_cycle_skip", out.ee_cycle_skip, 0);
    parse_bool("pcsx2_thread_pinning", out.thread_pinning);
    parse_bool("pcsx2_cheats",         out.cheats);
    parse_bool("pcsx2_host_fs",        out.host_fs);
    parse_bool("pcsx2_cdvd_precache",  out.cdvd_precache);
    parse_bool("pcsx2_fast_boot_ff",   out.fast_boot_ff);
```

### 3.4 — Core side: extend `ApplyDefaults`

- [ ] **Step 4: In `CoreOptionsEmulation.cpp::ApplyDefaults`, append SetX lines**

After the three SetFloatValue lines added in Task 2, before the closing `}`:

```cpp
    // SP7c Phase 1 — System Settings.
    si.SetIntValue ("EmuCore/Speedhacks", "EECycleRate", v.ee_cycle_rate);
    si.SetIntValue ("EmuCore/Speedhacks", "EECycleSkip", v.ee_cycle_skip);
    si.SetBoolValue("EmuCore", "EnableThreadPinning",       v.thread_pinning);
    si.SetBoolValue("EmuCore", "EnableCheats",              v.cheats);
    si.SetBoolValue("EmuCore", "HostFs",                    v.host_fs);
    si.SetBoolValue("EmuCore", "CdvdPrecache",              v.cdvd_precache);
    si.SetBoolValue("EmuCore", "EnableFastBootFastForward", v.fast_boot_ff);
```

### 3.5 — Test, build, schema check, commit

- [ ] **Step 5: Add Case 9 — round-trip System Settings**

In `pcsx2-libretro/tools/test_core_options.cpp`, after Case 8's closing block, before `std::printf("\n%d failure(s)\n", failures);`, insert:

```cpp
    // -------- Case 9: System Settings round-trip --------
    fake::reset();
    fake::variables["pcsx2_renderer"]      = "auto";
    fake::variables["pcsx2_mtvu"]          = "enabled";
    fake::variables["pcsx2_fast_boot"]     = "enabled";
    fake::variables["pcsx2_ee_cycle_rate"] = "-1";
    fake::variables["pcsx2_ee_cycle_skip"] = "2";
    fake::variables["pcsx2_thread_pinning"]= "enabled";
    fake::variables["pcsx2_cheats"]        = "enabled";
    fake::variables["pcsx2_host_fs"]       = "disabled";
    fake::variables["pcsx2_cdvd_precache"] = "enabled";
    fake::variables["pcsx2_fast_boot_ff"]  = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 9 ee_cycle_rate=-1",  r.emulation.ee_cycle_rate,  -1);
    check_int ("Case 9 ee_cycle_skip=2",   r.emulation.ee_cycle_skip,   2);
    check_bool("Case 9 thread_pinning",    r.emulation.thread_pinning,  true);
    check_bool("Case 9 cheats",            r.emulation.cheats,          true);
    check_bool("Case 9 host_fs=off",       r.emulation.host_fs,         false);
    check_bool("Case 9 cdvd_precache",     r.emulation.cdvd_precache,   true);
    check_bool("Case 9 fast_boot_ff",      r.emulation.fast_boot_ff,    true);
```

- [ ] **Step 6: Rebuild and run the standalone test**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: `0 failure(s)`.

- [ ] **Step 7: Rebuild pcsx2_libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`.

- [ ] **Step 8: Commit core-side System Settings**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsEmulation.h \
        pcsx2-libretro/CoreOptionsEmulation.cpp \
        pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 1 Task 3 (core): System Settings knobs

Adds 7 knobs to the Emulation card's System Settings sub-group:
EECycleRate / EECycleSkip / EnableThreadPinning / EnableCheats /
HostFs / CdvdPrecache / EnableFastBootFastForward. Two Combos
(EECycleRate, EECycleSkip) and five Bools share the helper push_bool
lambda that mirrors push_speed's pattern.

Settings.cpp's hardcoded HostFs=false write becomes redundant once
this lands (the user-controlled ApplyDefaults call overwrites it
either way). Removal scheduled for Task 5's cleanup.

test_core_options.cpp Case 9 covers System Settings round-trip.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### 3.6 — Host side: append 7 SettingDef rows

- [ ] **Step 9: Open `pcsx2_libretro_adapter.cpp` and append System Settings rows**

After the Speed Control block added in Task 2 Step 12, before `return s;`, insert:

```cpp
    // SP7c Phase 1 — System Settings (sub-group B of the Emulation card).
    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_ee_cycle_rate", "EE Cycle Rate", "0",
        {{"50% (Underclock)",  "-3"},
         {"60% (Underclock)",  "-2"},
         {"75% (Underclock)",  "-1"},
         {"100% (Normal Speed)","0"},
         {"130% (Overclock)",  "1"},
         {"180% (Overclock)",  "2"},
         {"300% (Overclock)",  "3"}},
        "Underclocks or overclocks the emulated Emotion Engine CPU. "
        "Most games should stay at 100%. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_ee_cycle_skip", "EE Cycle Skipping", "0",
        {{"Disabled",            "0"},
         {"Mild Underclock",     "1"},
         {"Moderate Underclock", "2"},
         {"Maximum Underclock",  "3"}},
        "Makes the EE skip cycles. Stronger underclock than EE Cycle Rate; "
        "can recover frame-rate in slow scenes at the cost of visible "
        "glitches. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_thread_pinning", "Thread Pinning", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Pin emulation threads to specific CPU cores. Can reduce stutter "
        "on heterogeneous-core CPUs. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_cheats", "Enable Cheats", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Load pnach cheat files on game launch. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_host_fs", "Enable Host Filesystem", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Allow the emulated PS2 to read host files. Homebrew-only feature; "
        "retail games never use it. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_cdvd_precache", "CDVD Precache", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Load the entire disc image into RAM before booting. Eliminates "
        "disc-read stutter at the cost of memory and a slower initial "
        "boot. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_fast_boot_ff", "Fast-Forward Through BIOS", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "When Fast Boot is enabled, also fast-forward the brief BIOS boot "
        "animation. No effect when Fast Boot is disabled. Takes effect on "
        "next launch.",
        /*dependsOn=*/"pcsx2_fast_boot"));
```

- [ ] **Step 10: Schema fidelity check — expect PASS**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```

Expected:
```
Schema fidelity OK: 13 core keys, 13 host keys, byte-for-byte match.
```

- [ ] **Step 11: Build RetroNest-Project**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 12: Commit host-side System Settings**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 1 Task 3 (host): System Settings rows

Adds 7 SettingDef rows under category="Emulation"
group="System Settings" mirroring the standalone PCSX2 dialog's
sub-section: EECycleRate / EECycleSkip / EnableThreadPinning /
EnableCheats / HostFs / CdvdPrecache / EnableFastBootFastForward.
The Fast-Forward Through BIOS row uses dependsOn="pcsx2_fast_boot"
so the UI greys it out when Fast Boot is disabled.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Sub-group C — Frame Pacing / Latency Control (5 knobs)

VsyncQueueSize is a 4-entry Combo; the other 4 are Bools. Same shape as Task 3.

**Files:** same set as Task 3.

### 4.1 — Core side: extend `Values`

- [ ] **Step 1: In `CoreOptionsEmulation.h`, extend `struct Values`**

After the System Settings fields added in Task 3, before the closing `};`:

```cpp
    // --- SP7c Phase 1: Frame Pacing / Latency Control (5 knobs) ---
    int  vsync_queue_size      = 2;     // EmuCore/GS/VsyncQueueSize (0..3)
    bool sync_to_host_rr       = false; // EmuCore/GS/SyncToHostRefreshRate
    bool vsync                 = false; // EmuCore/GS/VsyncEnable
    bool use_vsync_timing      = false; // EmuCore/GS/UseVSyncForTiming
    bool skip_duplicate_frames = false; // EmuCore/GS/SkipDuplicateFrames
```

### 4.2 — Core side: extend `AppendDefinitions`

- [ ] **Step 2: In `CoreOptionsEmulation.cpp::AppendDefinitions`, append 5 definitions**

After the System Settings block (push_bool calls), before the closing `}`:

```cpp
    // SP7c Phase 1 — Frame Pacing / Latency Control sub-group.
    //
    // libretro pacing note: the frontend (RetroNest) owns presentation in
    // libretro mode, so VsyncEnable / UseVSyncForTiming may be cosmetic on
    // this build (the actual vsync decision happens in CoreRuntime's
    // present path). We expose them for parity with the standalone dialog
    // and document the caveat in the tooltip.
    out.push_back({
        "pcsx2_vsync_queue_size",
        "Maximum Frame Latency",
        nullptr,
        "Frames the GS can queue before the EE must wait. Lower values "
        "reduce input latency at the cost of frame-pacing smoothness. "
        "0 is the lowest-latency 'optimal' mode (re-paces every frame).",
        nullptr,
        nullptr,
        {
            { "0", "Optimal (Frame Pacing)" },
            { "1", "1 frame" },
            { "2", "2 frames" },
            { "3", "3 frames" },
            { nullptr, nullptr },
        },
        "2",
    });

    push_bool("pcsx2_sync_to_host_rr",
        "Sync to Host Refresh Rate",
        "Adjust emulation speed slightly to align with the host display's "
        "refresh rate. Reduces audio drift on non-60Hz monitors. May be "
        "cosmetic in libretro mode — the frontend owns presentation timing.",
        "disabled");

    push_bool("pcsx2_vsync",
        "Vertical Sync (VSync)",
        "Synchronize frame submission with the host display's vblank. "
        "May be cosmetic in libretro mode — the frontend owns presentation.",
        "disabled");

    push_bool("pcsx2_use_vsync_timing",
        "Use Host VSync Timing",
        "Drive emulation timing from host vsync instead of the emulated "
        "console's refresh. Only takes effect when both VSync and "
        "Sync to Host Refresh Rate are enabled.",
        "disabled");

    push_bool("pcsx2_skip_duplicate_frames",
        "Skip Presenting Duplicate Frames",
        "Don't re-present a frame if the GS hasn't produced new output. "
        "Saves a tiny amount of GPU time. Mostly cosmetic in libretro mode.",
        "disabled");
```

### 4.3 — Core side: extend `Parse`

- [ ] **Step 3: In `CoreOptionsEmulation.cpp::Parse`, append parse branches**

After the System Settings parse calls, before the closing `}`:

```cpp
    // SP7c Phase 1 — Frame Pacing parsing.
    parse_int("pcsx2_vsync_queue_size", out.vsync_queue_size, 2);
    parse_bool("pcsx2_sync_to_host_rr",       out.sync_to_host_rr);
    parse_bool("pcsx2_vsync",                 out.vsync);
    parse_bool("pcsx2_use_vsync_timing",      out.use_vsync_timing);
    parse_bool("pcsx2_skip_duplicate_frames", out.skip_duplicate_frames);
```

### 4.4 — Core side: extend `ApplyDefaults`

- [ ] **Step 4: In `CoreOptionsEmulation.cpp::ApplyDefaults`, append SetX lines**

After the System Settings SetX lines, before the closing `}`:

```cpp
    // SP7c Phase 1 — Frame Pacing.
    si.SetIntValue ("EmuCore/GS", "VsyncQueueSize",       v.vsync_queue_size);
    si.SetBoolValue("EmuCore/GS", "SyncToHostRefreshRate", v.sync_to_host_rr);
    si.SetBoolValue("EmuCore/GS", "VsyncEnable",          v.vsync);
    si.SetBoolValue("EmuCore/GS", "UseVSyncForTiming",    v.use_vsync_timing);
    si.SetBoolValue("EmuCore/GS", "SkipDuplicateFrames",  v.skip_duplicate_frames);
```

### 4.5 — Test, build, commit

- [ ] **Step 5: Add Case 10 — round-trip Frame Pacing**

In `pcsx2-libretro/tools/test_core_options.cpp`, after Case 9, before `std::printf("\n%d failure(s)\n", failures);`:

```cpp
    // -------- Case 10: Frame Pacing round-trip --------
    fake::reset();
    fake::variables["pcsx2_renderer"]               = "auto";
    fake::variables["pcsx2_mtvu"]                   = "enabled";
    fake::variables["pcsx2_fast_boot"]              = "enabled";
    fake::variables["pcsx2_vsync_queue_size"]       = "0";
    fake::variables["pcsx2_sync_to_host_rr"]        = "enabled";
    fake::variables["pcsx2_vsync"]                  = "enabled";
    fake::variables["pcsx2_use_vsync_timing"]       = "enabled";
    fake::variables["pcsx2_skip_duplicate_frames"]  = "disabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 10 vsync_queue_size=0",  r.emulation.vsync_queue_size,      0);
    check_bool("Case 10 sync_to_host_rr",     r.emulation.sync_to_host_rr,       true);
    check_bool("Case 10 vsync",               r.emulation.vsync,                 true);
    check_bool("Case 10 use_vsync_timing",    r.emulation.use_vsync_timing,      true);
    check_bool("Case 10 skip_duplicate=off",  r.emulation.skip_duplicate_frames, false);
```

- [ ] **Step 6: Rebuild and run the standalone test**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsEmulation.cpp -DCORE_OPTIONS_TEST_ONLY -o test_core_options
./test_core_options
```

Expected: `0 failure(s)`.

- [ ] **Step 7: Rebuild pcsx2_libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`.

- [ ] **Step 8: Commit core-side Frame Pacing**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/CoreOptionsEmulation.h \
        pcsx2-libretro/CoreOptionsEmulation.cpp \
        pcsx2-libretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 1 Task 4 (core): Frame Pacing knobs

Adds 5 knobs to the Emulation card's Frame Pacing / Latency Control
sub-group: VsyncQueueSize / SyncToHostRefreshRate / VsyncEnable /
UseVSyncForTiming / SkipDuplicateFrames. VsyncQueueSize is a 4-entry
Combo; the rest are Bools that reuse Task 3's push_bool helper.

Libretro-pacing caveat documented in each tooltip: the frontend owns
presentation in libretro mode, so several of these knobs may be
cosmetic on this build. We expose them for parity with the standalone
dialog rather than silently dropping them.

test_core_options.cpp Case 10 covers Frame Pacing round-trip.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### 4.6 — Host side: append 5 SettingDef rows

- [ ] **Step 9: Open `pcsx2_libretro_adapter.cpp` and append Frame Pacing rows**

After the System Settings block added in Task 3 Step 9, before `return s;`, insert:

```cpp
    // SP7c Phase 1 — Frame Pacing / Latency Control (sub-group C).
    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_vsync_queue_size", "Maximum Frame Latency", "2",
        {{"Optimal (Frame Pacing)", "0"},
         {"1 frame",                "1"},
         {"2 frames",               "2"},
         {"3 frames",               "3"}},
        "Frames the GS can queue before the EE must wait. Lower values "
        "reduce input latency at the cost of pacing smoothness. Takes "
        "effect on next launch."));

    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_sync_to_host_rr", "Sync to Host Refresh Rate", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Adjust emulation speed slightly to align with the host display's "
        "refresh rate. May be cosmetic in libretro mode. Takes effect on "
        "next launch."));

    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_vsync", "Vertical Sync (VSync)", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Synchronize frame submission with the host display's vblank. "
        "May be cosmetic in libretro mode. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_use_vsync_timing", "Use Host VSync Timing", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Drive emulation timing from host vsync instead of the emulated "
        "console's refresh. Only takes effect when both VSync and Sync "
        "to Host Refresh Rate are enabled. Takes effect on next launch.",
        /*dependsOn=*/"pcsx2_vsync && pcsx2_sync_to_host_rr"));

    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_skip_duplicate_frames", "Skip Presenting Duplicate Frames", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Don't re-present a frame if the GS hasn't produced new output. "
        "Mostly cosmetic in libretro mode. Takes effect on next launch."));
```

- [ ] **Step 10: Schema fidelity check — expect PASS**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target check_schema_fidelity
```

Expected:
```
Schema fidelity OK: 18 core keys, 18 host keys, byte-for-byte match.
```

- [ ] **Step 11: Build RetroNest-Project**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-arm64 --target RetroNest 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 12: Commit host-side Frame Pacing**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 1 Task 4 (host): Frame Pacing rows

Adds 5 SettingDef rows under category="Emulation"
group="Frame Pacing / Latency Control": VsyncQueueSize /
SyncToHostRefreshRate / VsyncEnable / UseVSyncForTiming /
SkipDuplicateFrames. UseVSyncForTiming uses the standalone dialog's
multi-master dependsOn expression
("pcsx2_vsync && pcsx2_sync_to_host_rr") so the UI greys it out when
either gate is disabled.

With this commit the Emulation card is at full standalone parity
(17 of 17 rows from pcsx2_adapter.cpp's "Emulation" category).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Remove redundant Settings.cpp HostFs hardcode

Phase 0 left `Settings.cpp:234` with a hardcoded `g_si.SetBoolValue("EmuCore", "HostFs", false);`. After Phase 1 Task 3, `Emulation::ApplyDefaults` writes the user's chosen value to the same key — so the hardcoded line is dead and confusing. Remove it.

**Files:**
- Modify: `pcsx2-libretro/Settings.cpp` (delete one line + the now-stale comment block above it).

- [ ] **Step 1: Open `pcsx2-libretro/Settings.cpp` and delete the HostFs hardcode**

Delete lines 233-234 (one comment line + one code line):

```cpp
    // Disable HostFS (we don't expose host filesystem to the VM).
    g_si.SetBoolValue("EmuCore", "HostFs", false);
```

The user-controlled `Emulation::ApplyDefaults` runs after this point and writes the user's selection (default `false`), so behavior is byte-identical when the user doesn't tweak the toggle.

- [ ] **Step 2: Rebuild pcsx2_libretro**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
cmake --build build-arm64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`.

- [ ] **Step 3: Commit Settings.cpp cleanup**

```bash
git add pcsx2-libretro/Settings.cpp
git commit -m "$(cat <<'EOF'
SP7c Phase 1 Task 5: drop redundant Settings.cpp HostFs hardcode

Phase 0's foundation left g_si.SetBoolValue("EmuCore", "HostFs", false)
at Settings.cpp:234 as a SP7b-era safety belt. After Phase 1 Task 3 the
user-controlled Emulation::ApplyDefaults writes the same key with the
user's selection (default false), running AFTER this hardcoded line —
so the hardcode is dead. Removing it eliminates the misleading
appearance that HostFs is host-managed.

Behavior is byte-identical when the user leaves Host Filesystem on
its default "disabled" setting.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Universal build + RetroNest copy

The arm64 build is sufficient for development; the user's RetroNest install is universal (arm64 + x86_64), so the deployable dylib must be lipo'd.

**Files:** none (build/copy only).

- [ ] **Step 1: Build x86_64 slice**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
arch -x86_64 /usr/local/bin/cmake --build build-x86_64 --target pcsx2_libretro 2>&1 | tail -5
```

Expected: `[100%] Built target pcsx2_libretro`. If `build-x86_64` does not exist, regenerate it per `scripts/build-universal.sh` first.

- [ ] **Step 2: lipo arm64 + x86_64 into universal dylib**

Run:
```bash
lipo -create \
    -output ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib \
    build-arm64/pcsx2-libretro/pcsx2_libretro.dylib \
    build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib
file ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

Expected: `file` reports the dylib as `Mach-O 64-bit dynamically linked shared library arm64` and `... x86_64` (two architectures).

- [ ] **Step 3: Sync resources sibling-dir**

Run:
```bash
rsync -a --delete pcsx2-libretro/bin/resources/ \
    ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/
```

(Defensive: no resources changed in this phase, but the sync is the standard close-out step and keeps the install layout consistent.)

- [ ] **Step 4: No commit. Build artifacts only.**

---

## Task 7: Live smoke gate

Verify three behaviors end-to-end on real PS2 games via RetroNest's UI. This is the gate for declaring Phase 1 complete.

**Files:** none (manual UI testing).

- [ ] **Step 1: Launch RetroNest in arm64 mode**

Open RetroNest from the Finder (NOT through `arch -x86_64`). Confirm a PS2 game is in the library — R&C 2 (NTSC) is the SP7a/SP7b/SP6.5 reference; DBZ Budokai Tenkaichi 2 (PAL) is the PAL reference. Either is fine.

- [ ] **Step 2: Open the per-emulator settings dialog for pcsx2-libretro**

Right-click the PS2 game → Settings → confirm the dialog opens with `Pcsx2LibretroSettingsDialog`. The settings list should now show 18 rows total (3 SP7b + 15 Phase 1).

Acceptance: every Phase 1 knob name visible. Tooltips render. Combos open and show the correct value lists.

- [ ] **Step 3: Speed Control smoke — tweak Normal Speed**

Change `Normal Speed` from `100%` to `50%`. Save. Launch the game.

Expected: emulation runs at half real-time. Audio + video stay in sync (SoundTouch resamples). RetroNest's overlay's "FPS" counter shows approximately 30 (NTSC) / 25 (PAL).

Set back to `100%`, launch, verify normal-rate. Save.

- [ ] **Step 4: System Settings smoke — tweak EE Cycle Rate**

Change `EE Cycle Rate` to `-1 (75% Underclock)`. Save. Launch.

Expected: emulation feels slow (visible game-logic slowdown — not the same as speed scaling; e.g. characters move at reduced pace, animations stutter under load).

Console log should show the change applied — look for the `[CoreOptions]` resolved line in stderr (e.g. via `Console.app` or stdout if RetroNest was launched from terminal).

Set back to `0 (100% Normal Speed)`. Save. Launch. Confirm normal pace.

- [ ] **Step 5: System Settings smoke — tweak Enable Cheats**

Change `Enable Cheats` from `Disabled` to `Enabled`. Save. Launch.

Expected: stderr shows PCSX2's cheat-loading log line for the current game (e.g. `(Cheats) Loaded X cheats from pnach file ...` if any pnach exists for the disc; otherwise `(Cheats) No cheats found` confirming the toggle ran the pnach lookup path). The key behavior under test is that the toggle reaches PCSX2's cheat subsystem — pnach availability is not our problem.

Set back to `Disabled`. Save.

- [ ] **Step 6: Frame Pacing smoke — tweak Maximum Frame Latency**

Change `Maximum Frame Latency` from `2 frames` to `Optimal (Frame Pacing)`. Save. Launch.

Expected: the game runs (no crash). Stderr should show the new VsyncQueueSize=0 reflected in PCSX2's GS init log if it logs the value; otherwise the absence of regressions is sufficient.

Set back to `2 frames`.

- [ ] **Step 7: Schema-drift no-regression check**

In RetroNest's settings dialog, manually edit any libretro option, save, and re-open the dialog. The saved value should persist (round-trip through `options.json` → core's `GET_VARIABLE` → core's `ApplyDefaults` → user-visible change). This catches the OptionsStore::load silent-drop bug if any value/key in the new rows doesn't match between core and host.

- [ ] **Step 8: No commit. Verification only.**

If anything fails, debug per `superpowers:systematic-debugging` before declaring Phase 1 complete. Common failure modes:
- Schema mismatch (handled by Task 2.13 / 3.10 / 4.10 fidelity checks but worth re-verifying).
- INI section/key mismatch (e.g. `EmuCore` vs `EmuCore/Speedhacks`).
- Float-form mismatch (PCSX2 reads `"1"` not `"1.0"` — `SetFloatValue` must produce shortest form).
- ApplyDefaults running before VMManager::SetDefaultSettings (it runs after, in Settings.cpp:251-258, but verify).

---

## Task 8: Close-out

- [ ] **Step 1: Verify all commits land**

Run:
```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git log --oneline -10
```

Expected: 5 new commits on `retronest-libretro`:
1. SP7c Phase 1 Task 1: loosen Case 6 to regression sentinel
2. SP7c Phase 1 Task 2 (core): Speed Control knobs
3. SP7c Phase 1 Task 3 (core): System Settings knobs
4. SP7c Phase 1 Task 4 (core): Frame Pacing knobs
5. SP7c Phase 1 Task 5: drop redundant Settings.cpp HostFs hardcode

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git log --oneline -5
```

Expected: 3 new commits on `main`:
1. SP7c Phase 1 Task 2 (host): Speed Control rows
2. SP7c Phase 1 Task 3 (host): System Settings rows
3. SP7c Phase 1 Task 4 (host): Frame Pacing rows

- [ ] **Step 2: Do NOT push pcsx2-libretro to origin**

The pcsx2-libretro fork has no `origin` remote — only `upstream`. The `retronest-libretro` branch is intentionally kept local. Confirm:

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git remote -v
```

Expected: only `upstream` listed (no `origin`). If `origin` appears unexpectedly, stop and ask the user before pushing.

- [ ] **Step 3: RetroNest-Project main can be pushed if the user requests**

`origin` exists on RetroNest-Project. Wait for user direction before `git push origin main` — the user will decide.

- [ ] **Step 4: Update memory**

After live-smoke verification passes, update `~/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/sp7c_kickoff.md` and `project_pcsx2_libretro_port.md` to reflect Phase 1 shipped status. Write a Phase 1 handoff section summarizing:
- Commit range (5 core, 3 host).
- Total Emulation-card rows: 18 (target was 17 — we shipped one extra because Speed Control's three rows count as separate knobs).
- Lessons surfaced during Phase 1 execution (whatever shows up).
- Next focus: Phase 2 (Audio card, ~6 knobs).

- [ ] **Step 5: Update spec status**

Update `RetroNest-Project/docs/superpowers/specs/2026-05-13-pcsx2-libretro-sp7c-settings-parity-design.md`'s Phase 1 status line to ✅ shipped on 2026-05-13 (or whatever the actual date is).

Commit the docs change:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add docs/superpowers/specs/2026-05-13-pcsx2-libretro-sp7c-settings-parity-design.md
git commit -m "$(cat <<'EOF'
docs: mark SP7c Phase 1 shipped

Emulation card now at full 17-row parity with standalone PCSX2 dialog
(3 SP7b knobs + 15 Phase 1 knobs across Speed Control / System
Settings / Frame Pacing). Live-smoke verified.

Next focus: Phase 2 (Audio card).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-review checklist (the plan author runs this before handoff)

- [x] **Spec coverage:** every knob in the spec's Phase 1 table (`docs/superpowers/specs/2026-05-13-pcsx2-libretro-sp7c-settings-parity-design.md` Phase 1 section) has a corresponding task. 3 + 7 + 5 = 15 net new knobs across Tasks 2/3/4.
- [x] **No placeholders:** every step has the actual code or command. No "TBD", "similar to above without repetition", or "handle edge cases".
- [x] **Type consistency:** `pcsx2_normal_speed` is `float` in `Values`; `SetFloatValue` is called in `ApplyDefaults`; Case 8 reads `r.emulation.normal_speed` (matches field name). Field naming matches across all 4 sites for each knob.
- [x] **Fork has no `origin` remote** — Task 8 Step 2 explicitly checks and refuses to push. Phase 0's plan-gap caught at close-out is closed here.
- [x] **`CORE_OPTIONS_TEST_ONLY`** (Phase 0's renamed macro), not the pre-rename `SP7B_*` form.
- [x] **`Resolved` is nested** — `r.emulation.normal_speed`, not `r.normal_speed`. All test assertions match.
- [x] **`ApplyDefaults` body stays gated** by the existing `#ifndef CORE_OPTIONS_TEST_ONLY` block. New SetX lines go inside the gate.
- [x] **Standalone test compile command** includes `../CoreOptionsEmulation.cpp` (verified against the current test_core_options.cpp header comment which already shows it).
- [x] **Phase 1 host rows go under `category="Emulation"`**, not `category="Recommended"`. (The 3 SP7b rows stay under "Recommended" for backward-compat — Phase 5 reorganizes them.)
- [x] **dependsOn** wiring matches standalone — `pcsx2_fast_boot_ff` depends on `pcsx2_fast_boot`; `pcsx2_use_vsync_timing` depends on `pcsx2_vsync && pcsx2_sync_to_host_rr`.
