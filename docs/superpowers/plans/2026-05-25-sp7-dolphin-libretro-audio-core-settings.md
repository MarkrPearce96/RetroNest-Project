# SP7 — Dolphin libretro Audio + Core/system settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add 37 libretro core options (Audio 9, General 6, Advanced 11, GameCube 5, Wii 6) + a host-only Recommended rollup card to the Dolphin libretro core, purely additive on SP6's core-options infra.

**Architecture:** Two new core modules (`CoreOptionsAudio`, `CoreOptionsCore`) each mirroring `CoreOptionsGraphics` (literal `AppendDefinitions` push_back blocks + primitive `Values` + `Parse` + guarded `Apply`); they hang off `CoreOptions::Resolved`. The host adds an `opt(category,…)` helper for flat categories plus the Recommended view (extra rows re-referencing existing keys). All three SP6 verification guards extend.

**Tech Stack:** C++20, libretro core options v2, Dolphin `Config::SetCurrent`, Qt `SettingDef`, a standalone clang++ unit test, a Python fidelity script, QtTest.

**Repos & branches:** Core = `dolphin-libretro` on `libretro`. Host = `RetroNest-Project` on `main`. Both are the established long-lived sub-project branches (SP6 committed directly to them). No new branch.

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-25-sp7-dolphin-libretro-audio-core-settings-design.md`

---

## Conventions (read once, applies to every task)

- **push_back 8-field shape** (core `AppendDefinitions`): `{ key, label, nullptr, info, nullptr, nullptr, { {value,label}, …, {nullptr,nullptr} }, default }`. Value pairs are **`{stored_value, display_label}`**.
- **Host `opt()` value pairs are the OPPOSITE order: `{display_label, stored_value}`** (matches the fidelity script: core `(value,label)`, host `(label,value)`).
- **Defaults & value lists MUST match byte-for-byte** between the core push_back and the host row, or the fidelity check fails and `OptionsStore::load` silently drops values.
- **bools** are combos `{enabled,disabled}` parsed by `parse_bool` (`"enabled"` → true).
- **Resolved symbol/type/default reference** for every key is in **Appendix A** at the bottom — use it verbatim in `Apply`.
- Commit messages end with: `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>`
- **Core build (arm64 compile-check):** `cmake --build build-libretro --target dolphin_libretro` (full env in the build-setup memory; the build dir already exists).
- **Standalone core unit test compile/run** (from `Source/Core/DolphinLibretro/tools/`):
  ```bash
  clang++ -std=c++20 -I.. -I../../.. -DCORE_OPTIONS_TEST_ONLY \
      test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsGraphics.cpp \
      ../CoreOptionsAudio.cpp ../CoreOptionsCore.cpp \
      -o test_core_options && ./test_core_options
  ```
  (The two new `.cpp` files are added to this command starting in Task 3.)

---

## Task 1: `CoreOptionsAudio.h` — Values struct + declarations

**Files:**
- Create: `Source/Core/DolphinLibretro/CoreOptionsAudio.h`

- [ ] **Step 1: Create the header**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7: Audio-category libretro core options for dolphin-libretro. Same
// shape as CoreOptionsGraphics: primitive Values (no engine dep), Parse
// (string -> Values, incl. the DSP-engine fan-out), and Apply (guarded
// out of the CORE_OPTIONS_TEST_ONLY build).

#pragma once

#include "libretro.h"
#include <string>
#include <vector>

namespace DolphinLibretro::CoreOptions::Audio
{

// Resolved Audio values. Member-initializers are the defaults applied when
// an option is unset / the host returns NULL — they mirror Dolphin's own
// Config defaults (see Appendix A of the plan).
struct Values
{
    // dolphin_dsp_engine fan-out -> MAIN_DSP_HLE + MAIN_DSP_JIT.
    bool dsp_hle            = true;   // HLE default
    bool dsp_jit            = true;   // LLE-Recompiler vs Interpreter; ignored when HLE
    int  latency            = 20;     // MAIN_AUDIO_LATENCY (ms)
    bool dpl2_decoder       = false;  // MAIN_DPL2_DECODER
    int  dpl2_quality       = 2;      // MAIN_DPL2_QUALITY (AudioCommon::DPL2Quality, 0-3; High=2)
    int  buffer_size        = 80;     // MAIN_AUDIO_BUFFER_SIZE (ms)
    bool fill_gaps          = true;   // MAIN_AUDIO_FILL_GAPS
    bool preserve_pitch     = false;  // MAIN_AUDIO_PRESERVE_PITCH
    bool mute_on_unthrottle = false;  // MAIN_AUDIO_MUTE_ON_DISABLED_SPEED_LIMIT
    int  volume             = 100;    // MAIN_AUDIO_VOLUME (0-100)
};

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out);
void Parse(retro_environment_t cb, Values& out);
#ifndef CORE_OPTIONS_TEST_ONLY
void Apply(const Values& v);
#endif

} // namespace DolphinLibretro::CoreOptions::Audio
```

- [ ] **Step 2: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsAudio.h
git commit -m "SP7: CoreOptionsAudio.h — Audio Values + decls

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: `CoreOptionsAudio.cpp` — AppendDefinitions

**Files:**
- Create: `Source/Core/DolphinLibretro/CoreOptionsAudio.cpp`

- [ ] **Step 1: Create the .cpp with includes + AppendDefinitions**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsAudio.h"

#include <cstdlib>
#include <cstring>

#ifndef CORE_OPTIONS_TEST_ONLY
#include "AudioCommon/Enums.h"          // AudioCommon::DPL2Quality
#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"
#endif

namespace DolphinLibretro::CoreOptions::Audio
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    out.push_back({
        "dolphin_dsp_engine", "DSP Emulation Engine", nullptr,
        "How the audio DSP is emulated. HLE is fast and compatible; LLE is "
        "slower but accurate and required by a few games.",
        nullptr, nullptr,
        {
            { "HLE",             "HLE (Recommended)" },
            { "LLE Recompiler",  "LLE Recompiler (Slow)" },
            { "LLE Interpreter", "LLE Interpreter (Very Slow)" },
            { nullptr, nullptr },
        },
        "HLE",
    });
    out.push_back({
        "dolphin_audio_latency", "Audio Latency", nullptr,
        "Output latency in milliseconds. Lower is tighter but risks dropouts. "
        "Only active with backends that support latency control (OpenAL).",
        nullptr, nullptr,
        {
            { "0", "0 ms" }, { "10", "10 ms" }, { "20", "20 ms" }, { "40", "40 ms" },
            { "60", "60 ms" }, { "80", "80 ms" }, { "100", "100 ms" },
            { "150", "150 ms" }, { "200", "200 ms" },
            { nullptr, nullptr },
        },
        "20",
    });
    out.push_back({
        "dolphin_dpl2_decoder", "Dolby Pro Logic II Decoder", nullptr,
        "Decode the stereo mix into 5.1 surround. Requires a backend that "
        "supports DPL2 (OpenAL) and DSP in LLE mode; otherwise inert.",
        nullptr, nullptr,
        {
            { "enabled", "Enabled" }, { "disabled", "Disabled" },
            { nullptr, nullptr },
        },
        "disabled",
    });
    out.push_back({
        "dolphin_dpl2_quality", "DPL2 Decoding Quality", nullptr,
        "Trade-off between CPU cost and surround-decode accuracy.",
        nullptr, nullptr,
        {
            { "0", "Lowest (Latency ~10 ms)" }, { "1", "Low (Latency ~20 ms)" },
            { "2", "High (Latency ~40 ms)" },   { "3", "Highest (Latency ~80 ms)" },
            { nullptr, nullptr },
        },
        "2",
    });
    out.push_back({
        "dolphin_audio_buffer_size", "Audio Buffer Size", nullptr,
        "Internal mixer buffer in milliseconds. Higher is smoother but adds "
        "delay between picture and sound.",
        nullptr, nullptr,
        {
            { "32", "32 ms" }, { "48", "48 ms" }, { "64", "64 ms" }, { "80", "80 ms" },
            { "96", "96 ms" }, { "128", "128 ms" }, { "160", "160 ms" },
            { "256", "256 ms" }, { "512", "512 ms" },
            { nullptr, nullptr },
        },
        "80",
    });
    out.push_back({
        "dolphin_audio_fill_gaps", "Fill Audio Gaps", nullptr,
        "Synthesize silence when emulation can't keep up. Disable for more "
        "accurate native behaviour; enable for smoothness.",
        nullptr, nullptr,
        {
            { "enabled", "Enabled" }, { "disabled", "Disabled" },
            { nullptr, nullptr },
        },
        "enabled",
    });
    out.push_back({
        "dolphin_audio_preserve_pitch", "Preserve Audio Pitch", nullptr,
        "Time-stretch audio to keep pitch constant when emulation runs off "
        "100%. Useful with fast-forward.",
        nullptr, nullptr,
        {
            { "enabled", "Enabled" }, { "disabled", "Disabled" },
            { nullptr, nullptr },
        },
        "disabled",
    });
    out.push_back({
        "dolphin_audio_mute_on_unthrottle", "Mute When Unthrottled", nullptr,
        "Silence audio while running unthrottled (fast-forward). Avoids "
        "pitch/playback artifacts.",
        nullptr, nullptr,
        {
            { "enabled", "Enabled" }, { "disabled", "Disabled" },
            { nullptr, nullptr },
        },
        "disabled",
    });
    out.push_back({
        "dolphin_volume", "Volume", nullptr,
        "Master output volume.",
        nullptr, nullptr,
        {
            { "0", "0%" }, { "10", "10%" }, { "20", "20%" }, { "30", "30%" },
            { "40", "40%" }, { "50", "50%" }, { "60", "60%" }, { "70", "70%" },
            { "80", "80%" }, { "90", "90%" }, { "100", "100%" },
            { nullptr, nullptr },
        },
        "100",
    });
}

} // namespace DolphinLibretro::CoreOptions::Audio
```

- [ ] **Step 2: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsAudio.cpp
git commit -m "SP7: CoreOptionsAudio AppendDefinitions (9 options)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: `Audio::Parse` (TDD via the standalone test)

**Files:**
- Modify: `Source/Core/DolphinLibretro/CoreOptionsAudio.cpp` (add `Parse`)
- Modify: `Source/Core/DolphinLibretro/tools/test_core_options.cpp` (add Audio cases + include)

- [ ] **Step 1: Write the failing test additions**

In `test_core_options.cpp`, add the include after the existing CoreOptionsGraphics include (line 14):

```cpp
#include "../CoreOptionsAudio.h"
```

Then insert this block in `main()` immediately before the `// ── Full schema size` comment (~line 126):

```cpp
    // ── Audio: DSP-engine fan-out + a slider + default ──
    fake::reset();
    fake::vars["dolphin_dsp_engine"] = "LLE Recompiler";
    fake::vars["dolphin_volume"]     = "70";
    Audio::Values au{};
    Audio::Parse(&fake_cb, au);
    ck_bool("DSP LLE-Recompiler hle", au.dsp_hle, false);
    ck_bool("DSP LLE-Recompiler jit", au.dsp_jit, true);
    ck_int ("Audio volume 70",        au.volume, 70);
    ck_int ("Audio latency default",  au.latency, 20);

    fake::reset();
    fake::vars["dolphin_dsp_engine"] = "LLE Interpreter";
    Audio::Values au2{};
    Audio::Parse(&fake_cb, au2);
    ck_bool("DSP LLE-Interp hle", au2.dsp_hle, false);
    ck_bool("DSP LLE-Interp jit", au2.dsp_jit, false);

    fake::reset();
    fake::vars["dolphin_dsp_engine"] = "HLE";
    Audio::Values au3{};
    Audio::Parse(&fake_cb, au3);
    ck_bool("DSP HLE hle", au3.dsp_hle, true);
```

- [ ] **Step 2: Run the test to verify it fails (undefined `Audio::Parse`)**

Run (from `Source/Core/DolphinLibretro/tools/`):
```bash
clang++ -std=c++20 -I.. -I../../.. -DCORE_OPTIONS_TEST_ONLY \
    test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsGraphics.cpp \
    ../CoreOptionsAudio.cpp -o test_core_options && ./test_core_options
```
Expected: **link/compile error** — `Audio::Parse` undefined (also `BuildDefinitions` still references only Graphics, so `dolphin_dsp_engine` isn't emitted yet — that's fine, Parse is what we're testing).

- [ ] **Step 3: Implement `Audio::Parse`**

Append to `CoreOptionsAudio.cpp` inside the namespace, after `AppendDefinitions`:

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
    auto parse_bool = [](const char* s) { return s && std::strcmp(s, "enabled") == 0; };
    auto parse_int = [](const char* s, int fb) {
        if (!s) return fb; char* e = nullptr; long n = std::strtol(s, &e, 10);
        return e == s ? fb : static_cast<int>(n);
    };

    // DSP engine fan-out: one combo -> dsp_hle + dsp_jit.
    if (const char* v = query("dolphin_dsp_engine")) {
        if (std::strcmp(v, "HLE") == 0)              { out.dsp_hle = true;  out.dsp_jit = true; }
        else if (std::strcmp(v, "LLE Recompiler") == 0) { out.dsp_hle = false; out.dsp_jit = true; }
        else                                          { out.dsp_hle = false; out.dsp_jit = false; } // LLE Interpreter
    }
    if (const char* v = query("dolphin_audio_latency"))   out.latency = parse_int(v, 20);
    if (const char* v = query("dolphin_dpl2_decoder"))    out.dpl2_decoder = parse_bool(v);
    if (const char* v = query("dolphin_dpl2_quality"))    out.dpl2_quality = parse_int(v, 2);
    if (const char* v = query("dolphin_audio_buffer_size")) out.buffer_size = parse_int(v, 80);
    if (const char* v = query("dolphin_audio_fill_gaps")) out.fill_gaps = parse_bool(v);
    if (const char* v = query("dolphin_audio_preserve_pitch")) out.preserve_pitch = parse_bool(v);
    if (const char* v = query("dolphin_audio_mute_on_unthrottle")) out.mute_on_unthrottle = parse_bool(v);
    if (const char* v = query("dolphin_volume"))          out.volume = parse_int(v, 100);
}
```

- [ ] **Step 4: Run the test to verify the new Audio cases pass**

Run (same command as Step 2). Expected: all `[PASS]`, including the five Audio lines. (`BuildDefinitions size` assertion still expects 54 and will continue to pass until Task 5 wires Audio in — leave it for now.)

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsAudio.cpp Source/Core/DolphinLibretro/tools/test_core_options.cpp
git commit -m "SP7: Audio::Parse + DSP fan-out unit tests

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: `Audio::Apply`

**Files:**
- Modify: `Source/Core/DolphinLibretro/CoreOptionsAudio.cpp` (add guarded `Apply`)

- [ ] **Step 1: Implement `Apply`**

Append to `CoreOptionsAudio.cpp` inside the namespace, after `Parse`:

```cpp
#ifndef CORE_OPTIONS_TEST_ONLY
void Apply(const Values& v)
{
    // DSP engine fan-out (1:1 here; the string split happened in Parse).
    Config::SetCurrent(Config::MAIN_DSP_HLE, v.dsp_hle);
    Config::SetCurrent(Config::MAIN_DSP_JIT, v.dsp_jit);
    Config::SetCurrent(Config::MAIN_AUDIO_LATENCY, v.latency);
    Config::SetCurrent(Config::MAIN_DPL2_DECODER, v.dpl2_decoder);
    Config::SetCurrent(Config::MAIN_DPL2_QUALITY,
                       static_cast<AudioCommon::DPL2Quality>(v.dpl2_quality));
    Config::SetCurrent(Config::MAIN_AUDIO_BUFFER_SIZE, v.buffer_size);
    Config::SetCurrent(Config::MAIN_AUDIO_FILL_GAPS, v.fill_gaps);
    Config::SetCurrent(Config::MAIN_AUDIO_PRESERVE_PITCH, v.preserve_pitch);
    Config::SetCurrent(Config::MAIN_AUDIO_MUTE_ON_DISABLED_SPEED_LIMIT, v.mute_on_unthrottle);
    Config::SetCurrent(Config::MAIN_AUDIO_VOLUME, v.volume);
}
#endif
```

- [ ] **Step 2: Commit** (compile-checked in Task 11 once wired + CMake updated)

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsAudio.cpp
git commit -m "SP7: Audio::Apply -> Config::MAIN_* writes

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Wire Audio into `CoreOptions.{h,cpp}` + readback log

**Files:**
- Modify: `Source/Core/DolphinLibretro/CoreOptions.h`
- Modify: `Source/Core/DolphinLibretro/CoreOptions.cpp`

- [ ] **Step 1: Add the Audio Values member to `Resolved`**

In `CoreOptions.h`, add the include after the Graphics include (line 11) and the member in `Resolved`:

```cpp
#include "CoreOptionsGraphics.h"
#include "CoreOptionsAudio.h"
```

```cpp
struct Resolved
{
    Graphics::Values graphics{};
    Audio::Values    audio{};
};
```

- [ ] **Step 2: Append Audio to `BuildDefinitions` + `ReadResolved`**

In `CoreOptions.cpp` `BuildDefinitions()`, add after `Graphics::AppendDefinitions(v);` (line 27):

```cpp
        Audio::AppendDefinitions(v);
```

In `ReadResolved()`, add after `Graphics::Parse(cb, r.graphics);` (line 61):

```cpp
    Audio::Parse(cb, r.audio);
```

Then add an Audio readback log after the existing graphics log block (before `return r;`):

```cpp
    const auto& a = r.audio;
    CORE_OPTIONS_LOG(RETRO_LOG_INFO,
        "[CoreOptions] resolved audio: dsp(hle=%d,jit=%d) latency=%d dpl2(dec=%d,q=%d) "
        "buffer=%d fill_gaps=%d preserve_pitch=%d mute_unthrottle=%d volume=%d",
        a.dsp_hle ? 1 : 0, a.dsp_jit ? 1 : 0, a.latency, a.dpl2_decoder ? 1 : 0,
        a.dpl2_quality, a.buffer_size, a.fill_gaps ? 1 : 0, a.preserve_pitch ? 1 : 0,
        a.mute_on_unthrottle ? 1 : 0, a.volume);
```

- [ ] **Step 3: Update the standalone test size assertion to include Audio (9)**

In `test_core_options.cpp`, change the size assertion (lines 126-128) to:

```cpp
    // ── Full schema size: 53 Graphics + 9 Audio = 62 options + 1 terminator = 63 ──
    ck_int("BuildDefinitions size (62 opts + terminator)",
           static_cast<long>(BuildDefinitions().size()), 63);
```

- [ ] **Step 4: Run the test — all pass including the 63 size**

Run (from `tools/`):
```bash
clang++ -std=c++20 -I.. -I../../.. -DCORE_OPTIONS_TEST_ONLY \
    test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsGraphics.cpp \
    ../CoreOptionsAudio.cpp -o test_core_options && ./test_core_options
```
Expected: `0 failure(s)`, size line `got=63 want=63`.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptions.h Source/Core/DolphinLibretro/CoreOptions.cpp Source/Core/DolphinLibretro/tools/test_core_options.cpp
git commit -m "SP7: wire Audio into Resolved/BuildDefinitions/ReadResolved + readback log

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: `CoreOptionsCore.h` — Values (4 sub-structs) + declarations

**Files:**
- Create: `Source/Core/DolphinLibretro/CoreOptionsCore.h`

- [ ] **Step 1: Create the header**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7: Core/system libretro core options for dolphin-libretro (the
// standalone's General / Advanced / GameCube / Wii panes). Same shape as
// CoreOptionsGraphics: primitive Values, Parse, guarded Apply. Nearly all
// keys are Config::MAIN_* (Dolphin.ini [Core]/[DSP]). See Appendix A of
// the SP7 plan for the exact symbol/type/default of each field.

#pragma once

#include "libretro.h"
#include <string>
#include <vector>

namespace DolphinLibretro::CoreOptions::Core
{

struct Values
{
    struct General {
        bool        cpu_thread       = false;        // MAIN_CPU_THREAD (Dolphin's own default = false)
        bool        enable_cheats    = false;        // MAIN_ENABLE_CHEATS
        bool        load_into_memory = false;        // MAIN_LOAD_GAME_INTO_MEMORY
        bool        override_region  = false;        // MAIN_OVERRIDE_REGION_SETTINGS
        std::string emulation_speed  = "1.000000";   // MAIN_EMULATION_SPEED (float multiplier)
        int         fallback_region  = 1;            // MAIN_FALLBACK_REGION (DiscIO::Region; 1=NTSC-U)
    } general;

    struct Advanced {
        std::string cpu_core                  = "JIT";  // MAIN_CPU_CORE ("Interpreter"/"Cached Interpreter"/"JIT")
        bool        mmu                       = false;  // MAIN_MMU
        bool        pause_on_panic            = false;  // MAIN_PAUSE_ON_PANIC
        bool        accurate_cpu_cache        = false;  // MAIN_ACCURATE_CPU_CACHE
        bool        correct_time_drift        = false;  // MAIN_CORRECT_TIME_DRIFT
        bool        rush_frame_presentation   = false;  // MAIN_RUSH_FRAME_PRESENTATION
        bool        smooth_early_presentation = false;  // MAIN_SMOOTH_EARLY_PRESENTATION
        bool        overclock_enable          = false;  // MAIN_OVERCLOCK_ENABLE
        int         overclock                 = 1;      // MAIN_OVERCLOCK (multiplier 1..4 -> float)
        bool        vi_overclock_enable       = false;  // MAIN_VI_OVERCLOCK_ENABLE
        int         vi_overclock              = 1;      // MAIN_VI_OVERCLOCK (multiplier 1..4 -> float)
    } advanced;

    struct GameCube {
        bool skip_ipl      = true;   // MAIN_SKIP_IPL
        int  language      = 0;      // MAIN_GC_LANGUAGE (0=English..5=Dutch)
        int  slot_a        = 8;      // MAIN_SLOT_A (EXIDeviceType int; 8=GCI Folder)
        int  slot_b        = 255;    // MAIN_SLOT_B (255=None)
        int  serial_port_1 = 255;    // MAIN_SERIAL_PORT_1 (255=None)
    } gamecube;

    struct Wii {
        bool               keyboard         = false;  // MAIN_WII_KEYBOARD
        bool               wiilink          = false;  // MAIN_WII_WIILINK_ENABLE
        bool               sd_card          = true;   // MAIN_WII_SD_CARD
        bool               sd_card_writes   = true;   // MAIN_ALLOW_SD_WRITES
        bool               sd_card_folder_sync = false; // MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC
        unsigned long long sd_card_size     = 0;      // MAIN_WII_SD_CARD_FILESIZE (u64 bytes; 0=Auto)
    } wii;
};

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out);
void Parse(retro_environment_t cb, Values& out);
#ifndef CORE_OPTIONS_TEST_ONLY
void Apply(const Values& v);
#endif

} // namespace DolphinLibretro::CoreOptions::Core
```

- [ ] **Step 2: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsCore.h
git commit -m "SP7: CoreOptionsCore.h — General/Advanced/GameCube/Wii Values + decls

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: `CoreOptionsCore.cpp` — AppendDefinitions (28 options)

**Files:**
- Create: `Source/Core/DolphinLibretro/CoreOptionsCore.cpp`

- [ ] **Step 1: Create the .cpp with includes + AppendDefinitions**

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsCore.h"

#include <cstdlib>
#include <cstring>

#ifndef CORE_OPTIONS_TEST_ONLY
#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/EXI/EXI_Device.h"     // ExpansionInterface::EXIDeviceType
#include "Core/PowerPC/PowerPC.h"       // PowerPC::CPUCore, DefaultCPUCore
#include "DiscIO/Enums.h"               // DiscIO::Region
#endif

namespace DolphinLibretro::CoreOptions::Core
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // ── General ──
    out.push_back({
        "dolphin_cpu_thread", "Dual Core (Speed Hack)", nullptr,
        "Run CPU and GPU emulation on separate threads. Big speed gain; a few "
        "timing-sensitive games may glitch with it on.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_enable_cheats", "Enable Cheats", nullptr,
        "Process AR/Gecko cheat codes. Off by default for safety.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_load_game_into_memory", "Load Whole Game Into Memory", nullptr,
        "Pre-load the entire disc image into RAM at boot. Eliminates disc I/O "
        "stutter; uses more host memory.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_override_region_settings", "Allow Mismatched Region Settings", nullptr,
        "Force a region's settings (language, video mode) regardless of disc region.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_emulation_speed", "Speed Limit", nullptr,
        "Cap on emulated speed relative to native. Unlimited removes the throttle.",
        nullptr, nullptr,
        {
            { "0.000000", "Unlimited" },
            { "0.100000", "10%" }, { "0.200000", "20%" }, { "0.300000", "30%" },
            { "0.400000", "40%" }, { "0.500000", "50%" }, { "0.600000", "60%" },
            { "0.700000", "70%" }, { "0.800000", "80%" }, { "0.900000", "90%" },
            { "1.000000", "100% (Normal Speed)" },
            { "1.100000", "110%" }, { "1.200000", "120%" }, { "1.300000", "130%" },
            { "1.400000", "140%" }, { "1.500000", "150%" }, { "1.600000", "160%" },
            { "1.700000", "170%" }, { "1.800000", "180%" }, { "1.900000", "190%" },
            { "2.000000", "200%" },
            { nullptr, nullptr },
        },
        "1.000000",
    });
    out.push_back({
        "dolphin_fallback_region", "Fallback Region", nullptr,
        "Region used for games whose region can't be auto-detected. Affects "
        "boot timing and the system-menu locale.",
        nullptr, nullptr,
        {
            { "0", "NTSC-J (Japan)" }, { "1", "NTSC-U (Americas)" },
            { "2", "PAL (Europe)" }, { "3", "Region-Free / Unknown" },
            { "4", "NTSC-K (Korea)" },
            { nullptr, nullptr },
        },
        "1",
    });

    // ── Advanced ──
    out.push_back({
        "dolphin_cpu_core", "CPU Emulation Engine", nullptr,
        "The CPU backend. JIT is required for full-speed gameplay; the "
        "interpreters are debug/accuracy fallbacks.",
        nullptr, nullptr,
        {
            { "Interpreter",        "Interpreter (Slowest)" },
            { "Cached Interpreter", "Cached Interpreter (Slow)" },
            { "JIT",                "JIT Recompiler (Recommended)" },
            { nullptr, nullptr },
        },
        "JIT",
    });
    out.push_back({
        "dolphin_mmu", "Enable MMU", nullptr,
        "Emulate the memory management unit. Slower but required by a small "
        "set of games (typically Virtual Console / homebrew).",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_pause_on_panic", "Pause on Panic", nullptr,
        "Pause emulation when Dolphin reports a non-fatal error.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_accurate_cpu_cache", "Enable Write-Back Cache (Slow)", nullptr,
        "Emulate the CPU's L1 cache. Slower but more accurate; needed for a "
        "handful of self-modifying-code games.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_correct_time_drift", "Correct Time Drift", nullptr,
        "Compensate for accumulated frame-pacing drift over long sessions.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_rush_frame_presentation", "Rush Frame Presentation", nullptr,
        "Aggressively present frames as soon as they're ready. Lower latency, "
        "more tearing without V-Sync.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_smooth_early_presentation", "Smooth Early Presentation", nullptr,
        "Smooth pacing for frames that finish ahead of schedule.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_overclock_enable", "Enable CPU Clock Override", nullptr,
        "Allow the multiplier below to scale the emulated CPU clock. Some "
        "games run smoother overclocked; others crash.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_overclock", "CPU Overclock Multiplier", nullptr,
        "Multiplier on the emulated CPU clock when overclocking is enabled. "
        "1x = native.",
        nullptr, nullptr,
        {
            { "1", "1x (Native)" }, { "2", "2x (+100%)" },
            { "3", "3x (+200%)" }, { "4", "4x (+300%)" },
            { nullptr, nullptr },
        },
        "1",
    });
    out.push_back({
        "dolphin_vi_overclock_enable", "Enable VBI Frequency Override", nullptr,
        "Scale the video-interface clock independently of the CPU. Affects "
        "refresh-rate timing for some games.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_vi_overclock", "VI Overclock Multiplier", nullptr,
        "Multiplier on the VI clock when VI overclocking is enabled.",
        nullptr, nullptr,
        {
            { "1", "1x (Native)" }, { "2", "2x" }, { "3", "3x" }, { "4", "4x" },
            { nullptr, nullptr },
        },
        "1",
    });

    // ── GameCube ──
    out.push_back({
        "dolphin_skip_ipl", "Skip Main Menu (IPL)", nullptr,
        "Skip the GameCube boot animation and start the game directly. When "
        "off, requires IPL.bin in the BIOS folder.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "enabled",
    });
    out.push_back({
        "dolphin_gc_language", "System Language", nullptr,
        "System language used by GameCube games that respect it.",
        nullptr, nullptr,
        {
            { "0", "English" }, { "1", "German" }, { "2", "French" },
            { "3", "Spanish" }, { "4", "Italian" }, { "5", "Dutch" },
            { nullptr, nullptr },
        },
        "0",
    });
    out.push_back({
        "dolphin_slot_a", "Slot A", nullptr,
        "Device in the GameCube's left memory-card / EXI slot.",
        nullptr, nullptr,
        {
            { "255", "Nothing" }, { "0", "Dummy" }, { "1", "Memory Card" },
            { "8", "GCI Folder" }, { "7", "USB Gecko" },
            { "9", "Advance Game Port" }, { "4", "Microphone" },
            { nullptr, nullptr },
        },
        "8",
    });
    out.push_back({
        "dolphin_slot_b", "Slot B", nullptr,
        "Device in the GameCube's right memory-card / EXI slot.",
        nullptr, nullptr,
        {
            { "255", "Nothing" }, { "0", "Dummy" }, { "1", "Memory Card" },
            { "8", "GCI Folder" }, { "7", "USB Gecko" },
            { "9", "Advance Game Port" }, { "4", "Microphone" },
            { nullptr, nullptr },
        },
        "255",
    });
    out.push_back({
        "dolphin_serial_port_1", "Serial Port 1 (SP1)", nullptr,
        "Device on the GameCube's serial port — network adapters in "
        "compatible games.",
        nullptr, nullptr,
        {
            { "255", "Nothing" }, { "0", "Dummy" },
            { "5", "Broadband Adapter (TAP)" },
            { "10", "Broadband Adapter (XLink Kai)" },
            { "11", "Broadband Adapter (tapserver)" },
            { "12", "Broadband Adapter (HLE)" },
            { "13", "Modem Adapter (tapserver)" },
            { "6", "Triforce AM-Baseboard" },
            { nullptr, nullptr },
        },
        "255",
    });

    // ── Wii ──
    out.push_back({
        "dolphin_wii_keyboard", "Connect USB Keyboard", nullptr,
        "Make a USB keyboard visible to Wii software.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_enable_wiilink", "Enable WiiConnect24 (WiiLink)", nullptr,
        "Patch the Wii Shop / Channels to use community WiiLink servers. Off "
        "by default to avoid third-party network calls.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_wii_sd_card", "Insert SD Card", nullptr,
        "Make a virtual SD card visible to Wii software. Required for save "
        "imports, channel installs, and SD-using homebrew.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "enabled",
    });
    out.push_back({
        "dolphin_wii_sd_card_writes", "Allow Writes to SD Card", nullptr,
        "When off, the SD card is read-only — protects a shared image from "
        "accidental modification.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "enabled",
    });
    out.push_back({
        "dolphin_wii_sd_card_folder_sync", "Auto-Sync SD with Folder", nullptr,
        "Mirror the SD card image from a host folder.",
        nullptr, nullptr,
        { { "enabled", "Enabled" }, { "disabled", "Disabled" }, { nullptr, nullptr } },
        "disabled",
    });
    out.push_back({
        "dolphin_wii_sd_card_size", "SD Card Size", nullptr,
        "Capacity of the virtual SD card. Auto uses the image file as-is.",
        nullptr, nullptr,
        {
            { "0", "Auto" },
            { "67108864", "64 MiB" }, { "134217728", "128 MiB" },
            { "268435456", "256 MiB" }, { "536870912", "512 MiB" },
            { "1073741824", "1 GiB" }, { "2147483648", "2 GiB" },
            { "4294967296", "4 GiB (SDHC)" }, { "8589934592", "8 GiB (SDHC)" },
            { "17179869184", "16 GiB (SDHC)" }, { "34359738368", "32 GiB (SDHC)" },
            { nullptr, nullptr },
        },
        "0",
    });
}

} // namespace DolphinLibretro::CoreOptions::Core
```

- [ ] **Step 2: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsCore.cpp
git commit -m "SP7: CoreOptionsCore AppendDefinitions (28 options)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: `Core::Parse` (TDD via the standalone test)

**Files:**
- Modify: `Source/Core/DolphinLibretro/CoreOptionsCore.cpp` (add `Parse`)
- Modify: `Source/Core/DolphinLibretro/tools/test_core_options.cpp` (add Core cases + include)

- [ ] **Step 1: Write the failing test additions**

Add the include after the CoreOptionsAudio include:

```cpp
#include "../CoreOptionsCore.h"
```

Insert in `main()` before the `// ── Full schema size` block:

```cpp
    // ── Core/system Parse: enums, multipliers, u64 ──
    fake::reset();
    fake::vars["dolphin_cpu_core"]       = "Cached Interpreter";
    fake::vars["dolphin_fallback_region"] = "2";
    fake::vars["dolphin_slot_a"]         = "1";
    fake::vars["dolphin_wii_sd_card_size"] = "134217728";
    fake::vars["dolphin_overclock"]      = "3";
    fake::vars["dolphin_cpu_thread"]     = "enabled";
    Core::Values c{};
    Core::Parse(&fake_cb, c);
    ck_str ("CPU core cached",     c.advanced.cpu_core, "Cached Interpreter");
    ck_int ("Fallback region PAL", c.general.fallback_region, 2);
    ck_int ("Slot A memcard",      c.gamecube.slot_a, 1);
    ck_int ("Overclock 3x",        c.advanced.overclock, 3);
    ck_bool("Dual core enabled",   c.general.cpu_thread, true);
    ck_bool("SkipIPL default",     c.gamecube.skip_ipl, true);
    ck_bool("SD card default on",  c.wii.sd_card, true);
    // u64 size compared via long is safe here (134217728 < 2^31).
    ck_int ("SD size 128MiB",      static_cast<long>(c.wii.sd_card_size), 134217728);
```

- [ ] **Step 2: Run to verify failure (`Core::Parse` undefined)**

Run (from `tools/`):
```bash
clang++ -std=c++20 -I.. -I../../.. -DCORE_OPTIONS_TEST_ONLY \
    test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsGraphics.cpp \
    ../CoreOptionsAudio.cpp ../CoreOptionsCore.cpp -o test_core_options && ./test_core_options
```
Expected: compile/link error — `Core::Parse` undefined.

- [ ] **Step 3: Implement `Core::Parse`**

Append to `CoreOptionsCore.cpp` inside the namespace, after `AppendDefinitions`:

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
    auto parse_bool = [](const char* s) { return s && std::strcmp(s, "enabled") == 0; };
    auto parse_int = [](const char* s, int fb) {
        if (!s) return fb; char* e = nullptr; long n = std::strtol(s, &e, 10);
        return e == s ? fb : static_cast<int>(n);
    };

    // ── General ──
    if (const char* v = query("dolphin_cpu_thread"))            out.general.cpu_thread = parse_bool(v);
    if (const char* v = query("dolphin_enable_cheats"))         out.general.enable_cheats = parse_bool(v);
    if (const char* v = query("dolphin_load_game_into_memory")) out.general.load_into_memory = parse_bool(v);
    if (const char* v = query("dolphin_override_region_settings")) out.general.override_region = parse_bool(v);
    if (const char* v = query("dolphin_emulation_speed"))       out.general.emulation_speed = v;
    if (const char* v = query("dolphin_fallback_region"))       out.general.fallback_region = parse_int(v, 1);

    // ── Advanced ──
    if (const char* v = query("dolphin_cpu_core"))                 out.advanced.cpu_core = v;
    if (const char* v = query("dolphin_mmu"))                      out.advanced.mmu = parse_bool(v);
    if (const char* v = query("dolphin_pause_on_panic"))           out.advanced.pause_on_panic = parse_bool(v);
    if (const char* v = query("dolphin_accurate_cpu_cache"))       out.advanced.accurate_cpu_cache = parse_bool(v);
    if (const char* v = query("dolphin_correct_time_drift"))       out.advanced.correct_time_drift = parse_bool(v);
    if (const char* v = query("dolphin_rush_frame_presentation"))  out.advanced.rush_frame_presentation = parse_bool(v);
    if (const char* v = query("dolphin_smooth_early_presentation")) out.advanced.smooth_early_presentation = parse_bool(v);
    if (const char* v = query("dolphin_overclock_enable"))         out.advanced.overclock_enable = parse_bool(v);
    if (const char* v = query("dolphin_overclock"))                out.advanced.overclock = parse_int(v, 1);
    if (const char* v = query("dolphin_vi_overclock_enable"))      out.advanced.vi_overclock_enable = parse_bool(v);
    if (const char* v = query("dolphin_vi_overclock"))             out.advanced.vi_overclock = parse_int(v, 1);

    // ── GameCube ──
    if (const char* v = query("dolphin_skip_ipl"))        out.gamecube.skip_ipl = parse_bool(v);
    if (const char* v = query("dolphin_gc_language"))     out.gamecube.language = parse_int(v, 0);
    if (const char* v = query("dolphin_slot_a"))          out.gamecube.slot_a = parse_int(v, 8);
    if (const char* v = query("dolphin_slot_b"))          out.gamecube.slot_b = parse_int(v, 255);
    if (const char* v = query("dolphin_serial_port_1"))   out.gamecube.serial_port_1 = parse_int(v, 255);

    // ── Wii ──
    if (const char* v = query("dolphin_wii_keyboard"))           out.wii.keyboard = parse_bool(v);
    if (const char* v = query("dolphin_enable_wiilink"))         out.wii.wiilink = parse_bool(v);
    if (const char* v = query("dolphin_wii_sd_card"))            out.wii.sd_card = parse_bool(v);
    if (const char* v = query("dolphin_wii_sd_card_writes"))     out.wii.sd_card_writes = parse_bool(v);
    if (const char* v = query("dolphin_wii_sd_card_folder_sync")) out.wii.sd_card_folder_sync = parse_bool(v);
    if (const char* v = query("dolphin_wii_sd_card_size")) {
        char* e = nullptr;
        unsigned long long n = std::strtoull(v, &e, 10);
        if (e != v) out.wii.sd_card_size = n;
    }
}
```

- [ ] **Step 4: Run — new Core cases pass**

Run (same command as Step 2). Expected: all `[PASS]` for the Core lines. (Size assertion still 63 until Task 10.)

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsCore.cpp Source/Core/DolphinLibretro/tools/test_core_options.cpp
git commit -m "SP7: Core::Parse + unit tests (enums, multipliers, u64 size)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 9: `Core::Apply`

**Files:**
- Modify: `Source/Core/DolphinLibretro/CoreOptionsCore.cpp` (add guarded `Apply`)

- [ ] **Step 1: Implement `Apply`**

Append to `CoreOptionsCore.cpp` inside the namespace, after `Parse`:

```cpp
#ifndef CORE_OPTIONS_TEST_ONLY
void Apply(const Values& v)
{
    using ExpansionInterface::EXIDeviceType;

    // ── General ──
    Config::SetCurrent(Config::MAIN_CPU_THREAD, v.general.cpu_thread);
    Config::SetCurrent(Config::MAIN_ENABLE_CHEATS, v.general.enable_cheats);
    Config::SetCurrent(Config::MAIN_LOAD_GAME_INTO_MEMORY, v.general.load_into_memory);
    Config::SetCurrent(Config::MAIN_OVERRIDE_REGION_SETTINGS, v.general.override_region);
    Config::SetCurrent(Config::MAIN_EMULATION_SPEED,
                       static_cast<float>(std::strtod(v.general.emulation_speed.c_str(), nullptr)));
    Config::SetCurrent(Config::MAIN_FALLBACK_REGION,
                       static_cast<DiscIO::Region>(v.general.fallback_region));

    // ── Advanced ──
    {
        PowerPC::CPUCore core = PowerPC::DefaultCPUCore();  // arch-appropriate JIT
        if      (v.advanced.cpu_core == "Interpreter")        core = PowerPC::CPUCore::Interpreter;
        else if (v.advanced.cpu_core == "Cached Interpreter") core = PowerPC::CPUCore::CachedInterpreter;
        // "JIT" -> DefaultCPUCore() (JIT64 on x86_64, JITARM64 on arm64)
        Config::SetCurrent(Config::MAIN_CPU_CORE, core);
    }
    Config::SetCurrent(Config::MAIN_MMU, v.advanced.mmu);
    Config::SetCurrent(Config::MAIN_PAUSE_ON_PANIC, v.advanced.pause_on_panic);
    Config::SetCurrent(Config::MAIN_ACCURATE_CPU_CACHE, v.advanced.accurate_cpu_cache);
    Config::SetCurrent(Config::MAIN_CORRECT_TIME_DRIFT, v.advanced.correct_time_drift);
    Config::SetCurrent(Config::MAIN_RUSH_FRAME_PRESENTATION, v.advanced.rush_frame_presentation);
    Config::SetCurrent(Config::MAIN_SMOOTH_EARLY_PRESENTATION, v.advanced.smooth_early_presentation);
    Config::SetCurrent(Config::MAIN_OVERCLOCK_ENABLE, v.advanced.overclock_enable);
    Config::SetCurrent(Config::MAIN_OVERCLOCK, static_cast<float>(v.advanced.overclock));
    Config::SetCurrent(Config::MAIN_VI_OVERCLOCK_ENABLE, v.advanced.vi_overclock_enable);
    Config::SetCurrent(Config::MAIN_VI_OVERCLOCK, static_cast<float>(v.advanced.vi_overclock));

    // ── GameCube ──
    Config::SetCurrent(Config::MAIN_SKIP_IPL, v.gamecube.skip_ipl);
    Config::SetCurrent(Config::MAIN_GC_LANGUAGE, v.gamecube.language);
    Config::SetCurrent(Config::MAIN_SLOT_A, static_cast<EXIDeviceType>(v.gamecube.slot_a));
    Config::SetCurrent(Config::MAIN_SLOT_B, static_cast<EXIDeviceType>(v.gamecube.slot_b));
    Config::SetCurrent(Config::MAIN_SERIAL_PORT_1, static_cast<EXIDeviceType>(v.gamecube.serial_port_1));

    // ── Wii ──
    Config::SetCurrent(Config::MAIN_WII_KEYBOARD, v.wii.keyboard);
    Config::SetCurrent(Config::MAIN_WII_WIILINK_ENABLE, v.wii.wiilink);
    Config::SetCurrent(Config::MAIN_WII_SD_CARD, v.wii.sd_card);
    Config::SetCurrent(Config::MAIN_ALLOW_SD_WRITES, v.wii.sd_card_writes);
    Config::SetCurrent(Config::MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC, v.wii.sd_card_folder_sync);
    Config::SetCurrent(Config::MAIN_WII_SD_CARD_FILESIZE,
                       static_cast<u64>(v.wii.sd_card_size));
}
#endif
```

> **Note for the executor:** if any `Config::SetCurrent` call fails to compile due to a type mismatch (e.g. `MAIN_GC_LANGUAGE` wanting an enum, or `MAIN_WII_SD_CARD_FILESIZE` a different integer width), consult Appendix A and the actual `Info<T>` declaration in `Source/Core/Core/Config/MainSettings.h` — the symbol/type table was resolved against this exact checkout, but cast to the declared `T` if the compiler insists. Do **not** change the option's stored string format.

- [ ] **Step 2: Commit** (compiled in Task 11)

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsCore.cpp
git commit -m "SP7: Core::Apply -> Config::MAIN_* writes (CPU core via DefaultCPUCore)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 10: Wire Core into `CoreOptions.{h,cpp}` + readback log + size 91

**Files:**
- Modify: `Source/Core/DolphinLibretro/CoreOptions.h`
- Modify: `Source/Core/DolphinLibretro/CoreOptions.cpp`
- Modify: `Source/Core/DolphinLibretro/tools/test_core_options.cpp`

- [ ] **Step 1: Add Core to `Resolved`**

In `CoreOptions.h`, add the include and member:

```cpp
#include "CoreOptionsGraphics.h"
#include "CoreOptionsAudio.h"
#include "CoreOptionsCore.h"
```

```cpp
struct Resolved
{
    Graphics::Values graphics{};
    Audio::Values    audio{};
    Core::Values     core{};
};
```

- [ ] **Step 2: Append Core to `BuildDefinitions` + `ReadResolved`**

In `CoreOptions.cpp` `BuildDefinitions()`, after `Audio::AppendDefinitions(v);`:

```cpp
        Core::AppendDefinitions(v);
```

In `ReadResolved()`, after `Audio::Parse(cb, r.audio);`:

```cpp
    Core::Parse(cb, r.core);
```

Add a Core readback log after the audio log block (before `return r;`):

```cpp
    const auto& c = r.core;
    CORE_OPTIONS_LOG(RETRO_LOG_INFO,
        "[CoreOptions] resolved core: dual_core=%d cpu_core=%s mmu=%d region=%d speed=%s | "
        "overclock(en=%d,%dx) vi_oc(en=%d,%dx) | gc: skip_ipl=%d lang=%d slotA=%d slotB=%d sp1=%d | "
        "wii: kbd=%d wiilink=%d sd(on=%d,wr=%d,sync=%d,size=%llu)",
        c.general.cpu_thread ? 1 : 0, c.advanced.cpu_core.c_str(), c.advanced.mmu ? 1 : 0,
        c.general.fallback_region, c.general.emulation_speed.c_str(),
        c.advanced.overclock_enable ? 1 : 0, c.advanced.overclock,
        c.advanced.vi_overclock_enable ? 1 : 0, c.advanced.vi_overclock,
        c.gamecube.skip_ipl ? 1 : 0, c.gamecube.language, c.gamecube.slot_a,
        c.gamecube.slot_b, c.gamecube.serial_port_1,
        c.wii.keyboard ? 1 : 0, c.wii.wiilink ? 1 : 0, c.wii.sd_card ? 1 : 0,
        c.wii.sd_card_writes ? 1 : 0, c.wii.sd_card_folder_sync ? 1 : 0,
        static_cast<unsigned long long>(c.wii.sd_card_size));
```

- [ ] **Step 3: Bump the standalone test size assertion to 91**

In `test_core_options.cpp`, update:

```cpp
    // ── Full schema size: 53 Graphics + 9 Audio + 28 Core = 90 options + terminator = 91 ──
    ck_int("BuildDefinitions size (90 opts + terminator)",
           static_cast<long>(BuildDefinitions().size()), 91);
```

- [ ] **Step 4: Run the test — `0 failure(s)`, size 91**

Run (from `tools/`):
```bash
clang++ -std=c++20 -I.. -I../../.. -DCORE_OPTIONS_TEST_ONLY \
    test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsGraphics.cpp \
    ../CoreOptionsAudio.cpp ../CoreOptionsCore.cpp -o test_core_options && ./test_core_options
```
Expected: `got=91 want=91`, `0 failure(s)`.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptions.h Source/Core/DolphinLibretro/CoreOptions.cpp Source/Core/DolphinLibretro/tools/test_core_options.cpp
git commit -m "SP7: wire Core into Resolved/BuildDefinitions/ReadResolved + readback log; size->91

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 11: CMake — add the two sources + arm64 compile-check

**Files:**
- Modify: `Source/Core/DolphinLibretro/CMakeLists.txt`

- [ ] **Step 1: Add the new sources to the target source list**

In `CMakeLists.txt`, after the `CoreOptionsGraphics.cpp` line (line 21):

```cmake
    CoreOptionsAudio.cpp
    CoreOptionsCore.cpp
```

- [ ] **Step 2: Build the core (arm64) to verify Apply compiles against the real engine**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -30
```
Expected: builds to completion (`dolphin_libretro.dylib` linked). If a `Config::SetCurrent` type mismatch appears, fix per the Task 9 note (cast to the declared `Info<T>`), keeping stored strings unchanged.

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CMakeLists.txt
git commit -m "SP7: add CoreOptionsAudio/Core sources to CMake

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 12: Apply the new categories at boot (`LibretroFrontend.cpp`)

**Files:**
- Modify: `Source/Core/DolphinLibretro/LibretroFrontend.cpp:220`

- [ ] **Step 1: Add the two Apply calls beside `Graphics::Apply`**

After the existing line `DolphinLibretro::CoreOptions::Graphics::Apply(resolved.graphics);` (line 220), add:

```cpp
    DolphinLibretro::CoreOptions::Audio::Apply(resolved.audio);
    DolphinLibretro::CoreOptions::Core::Apply(resolved.core);
```

- [ ] **Step 2: Rebuild the core (arm64) to confirm it links**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -15
```
Expected: links cleanly.

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/LibretroFrontend.cpp
git commit -m "SP7: apply Audio + Core options at retro_load_game

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 13: Host — `opt()` helper + 37 rows (Audio/General/Advanced/GameCube/Wii)

**Files:**
- Modify: `RetroNest-Project/cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp`

- [ ] **Step 1: Add the `opt()` helper next to `gopt` (after line 141)**

```cpp
    // opt(): like gopt but with an explicit category (subcategory empty) for
    // the flat (no sub-tab) categories — Audio/General/Advanced/GameCube/Wii
    // and the Recommended view. Mirrors pcsx2_libretro_adapter's opt().
    auto opt = [](const QString& category, const QString& group,
                  const QString& key, const QString& label, const QString& def,
                  const QVector<QPair<QString,QString>>& valuesAndLabels,
                  const QString& tooltip, const QString& dependsOn = {}) -> SettingDef {
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

- [ ] **Step 2: Append the 37 rows just before `return s;` (line 333)**

> Value pairs here are **`{display_label, stored_value}`** (host order). Defaults match the core push_back exactly.

```cpp
    // ═══ Audio ═══
    s.append(opt("Audio", "DSP", "dolphin_dsp_engine", "DSP Emulation Engine", "HLE",
        {{"HLE (Recommended)","HLE"},{"LLE Recompiler (Slow)","LLE Recompiler"},{"LLE Interpreter (Very Slow)","LLE Interpreter"}},
        "How the audio DSP is emulated. HLE is fast and compatible; LLE is slower but accurate and required by a few games."));
    s.append(opt("Audio", "Backend", "dolphin_audio_latency", "Audio Latency", "20",
        {{"0 ms","0"},{"10 ms","10"},{"20 ms","20"},{"40 ms","40"},{"60 ms","60"},{"80 ms","80"},{"100 ms","100"},{"150 ms","150"},{"200 ms","200"}},
        "Output latency in milliseconds. Only active with backends that support latency control (OpenAL)."));
    s.append(opt("Audio", "Backend", "dolphin_dpl2_decoder", "Dolby Pro Logic II Decoder", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Decode the stereo mix into 5.1 surround. Requires a DPL2-capable backend and DSP in LLE mode."));
    s.append(opt("Audio", "Backend", "dolphin_dpl2_quality", "DPL2 Decoding Quality", "2",
        {{"Lowest (Latency ~10 ms)","0"},{"Low (Latency ~20 ms)","1"},{"High (Latency ~40 ms)","2"},{"Highest (Latency ~80 ms)","3"}},
        "Trade-off between CPU cost and surround-decode accuracy."));
    s.append(opt("Audio", "Playback", "dolphin_audio_buffer_size", "Audio Buffer Size", "80",
        {{"32 ms","32"},{"48 ms","48"},{"64 ms","64"},{"80 ms","80"},{"96 ms","96"},{"128 ms","128"},{"160 ms","160"},{"256 ms","256"},{"512 ms","512"}},
        "Internal mixer buffer in milliseconds. Higher is smoother but adds delay between picture and sound."));
    s.append(opt("Audio", "Playback", "dolphin_audio_fill_gaps", "Fill Audio Gaps", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Synthesize silence when emulation can't keep up. Disable for accuracy; enable for smoothness."));
    s.append(opt("Audio", "Playback", "dolphin_audio_preserve_pitch", "Preserve Audio Pitch", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Time-stretch audio to keep pitch constant when emulation runs off 100%. Useful with fast-forward."));
    s.append(opt("Audio", "Playback", "dolphin_audio_mute_on_unthrottle", "Mute When Unthrottled", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Silence audio while running unthrottled (fast-forward). Avoids pitch/playback artifacts."));
    s.append(opt("Audio", "Volume", "dolphin_volume", "Volume", "100",
        {{"0%","0"},{"10%","10"},{"20%","20"},{"30%","30"},{"40%","40"},{"50%","50"},{"60%","60"},{"70%","70"},{"80%","80"},{"90%","90"},{"100%","100"}},
        "Master output volume."));

    // ═══ General ═══
    s.append(opt("General", "Basic", "dolphin_cpu_thread", "Dual Core (Speed Hack)", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Run CPU and GPU emulation on separate threads. Big speed gain; a few timing-sensitive games may glitch with it on."));
    s.append(opt("General", "Basic", "dolphin_enable_cheats", "Enable Cheats", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Process AR/Gecko cheat codes. Off by default for safety."));
    s.append(opt("General", "Basic", "dolphin_load_game_into_memory", "Load Whole Game Into Memory", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Pre-load the entire disc image into RAM at boot. Eliminates disc I/O stutter; uses more host memory."));
    s.append(opt("General", "Basic", "dolphin_override_region_settings", "Allow Mismatched Region Settings", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Force a region's settings (language, video mode) regardless of disc region."));
    s.append(opt("General", "Basic", "dolphin_emulation_speed", "Speed Limit", "1.000000",
        {{"Unlimited","0.000000"},{"10%","0.100000"},{"20%","0.200000"},{"30%","0.300000"},{"40%","0.400000"},{"50%","0.500000"},{"60%","0.600000"},{"70%","0.700000"},{"80%","0.800000"},{"90%","0.900000"},{"100% (Normal Speed)","1.000000"},{"110%","1.100000"},{"120%","1.200000"},{"130%","1.300000"},{"140%","1.400000"},{"150%","1.500000"},{"160%","1.600000"},{"170%","1.700000"},{"180%","1.800000"},{"190%","1.900000"},{"200%","2.000000"}},
        "Cap on emulated speed relative to native. Unlimited removes the throttle."));
    s.append(opt("General", "Region", "dolphin_fallback_region", "Fallback Region", "1",
        {{"NTSC-J (Japan)","0"},{"NTSC-U (Americas)","1"},{"PAL (Europe)","2"},{"Region-Free / Unknown","3"},{"NTSC-K (Korea)","4"}},
        "Region used for games whose region can't be auto-detected. Affects boot timing and the system-menu locale."));

    // ═══ Advanced ═══
    s.append(opt("Advanced", "CPU", "dolphin_cpu_core", "CPU Emulation Engine", "JIT",
        {{"Interpreter (Slowest)","Interpreter"},{"Cached Interpreter (Slow)","Cached Interpreter"},{"JIT Recompiler (Recommended)","JIT"}},
        "The CPU backend. JIT is required for full-speed gameplay; the interpreters are debug/accuracy fallbacks."));
    s.append(opt("Advanced", "CPU", "dolphin_mmu", "Enable MMU", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Emulate the memory management unit. Slower but required by a small set of games."));
    s.append(opt("Advanced", "CPU", "dolphin_pause_on_panic", "Pause on Panic", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Pause emulation when Dolphin reports a non-fatal error."));
    s.append(opt("Advanced", "CPU", "dolphin_accurate_cpu_cache", "Enable Write-Back Cache (Slow)", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Emulate the CPU's L1 cache. Slower but more accurate; needed for a handful of self-modifying-code games."));
    s.append(opt("Advanced", "Timing", "dolphin_correct_time_drift", "Correct Time Drift", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Compensate for accumulated frame-pacing drift over long sessions."));
    s.append(opt("Advanced", "Timing", "dolphin_rush_frame_presentation", "Rush Frame Presentation", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Aggressively present frames as soon as they're ready. Lower latency, more tearing without V-Sync."));
    s.append(opt("Advanced", "Timing", "dolphin_smooth_early_presentation", "Smooth Early Presentation", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Smooth pacing for frames that finish ahead of schedule."));
    s.append(opt("Advanced", "Clock Override", "dolphin_overclock_enable", "Enable CPU Clock Override", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Allow the multiplier below to scale the emulated CPU clock. Some games run smoother overclocked; others crash."));
    s.append(opt("Advanced", "Clock Override", "dolphin_overclock", "CPU Overclock Multiplier", "1",
        {{"1x (Native)","1"},{"2x (+100%)","2"},{"3x (+200%)","3"},{"4x (+300%)","4"}},
        "Multiplier on the emulated CPU clock when overclocking is enabled. 1x = native.", "dolphin_overclock_enable"));
    s.append(opt("Advanced", "VBI Override", "dolphin_vi_overclock_enable", "Enable VBI Frequency Override", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Scale the video-interface clock independently of the CPU. Affects refresh-rate timing for some games."));
    s.append(opt("Advanced", "VBI Override", "dolphin_vi_overclock", "VI Overclock Multiplier", "1",
        {{"1x (Native)","1"},{"2x","2"},{"3x","3"},{"4x","4"}},
        "Multiplier on the VI clock when VI overclocking is enabled.", "dolphin_vi_overclock_enable"));

    // ═══ GameCube ═══
    s.append(opt("GameCube", "IPL", "dolphin_skip_ipl", "Skip Main Menu (IPL)", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip the GameCube boot animation and start the game directly. When off, requires IPL.bin in the BIOS folder."));
    s.append(opt("GameCube", "IPL", "dolphin_gc_language", "System Language", "0",
        {{"English","0"},{"German","1"},{"French","2"},{"Spanish","3"},{"Italian","4"},{"Dutch","5"}},
        "System language used by GameCube games that respect it."));
    s.append(opt("GameCube", "Devices", "dolphin_slot_a", "Slot A", "8",
        {{"Nothing","255"},{"Dummy","0"},{"Memory Card","1"},{"GCI Folder","8"},{"USB Gecko","7"},{"Advance Game Port","9"},{"Microphone","4"}},
        "Device in the GameCube's left memory-card / EXI slot."));
    s.append(opt("GameCube", "Devices", "dolphin_slot_b", "Slot B", "255",
        {{"Nothing","255"},{"Dummy","0"},{"Memory Card","1"},{"GCI Folder","8"},{"USB Gecko","7"},{"Advance Game Port","9"},{"Microphone","4"}},
        "Device in the GameCube's right memory-card / EXI slot."));
    s.append(opt("GameCube", "Devices", "dolphin_serial_port_1", "Serial Port 1 (SP1)", "255",
        {{"Nothing","255"},{"Dummy","0"},{"Broadband Adapter (TAP)","5"},{"Broadband Adapter (XLink Kai)","10"},{"Broadband Adapter (tapserver)","11"},{"Broadband Adapter (HLE)","12"},{"Modem Adapter (tapserver)","13"},{"Triforce AM-Baseboard","6"}},
        "Device on the GameCube's serial port — network adapters in compatible games."));

    // ═══ Wii ═══
    s.append(opt("Wii", "Misc", "dolphin_wii_keyboard", "Connect USB Keyboard", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Make a USB keyboard visible to Wii software."));
    s.append(opt("Wii", "Misc", "dolphin_enable_wiilink", "Enable WiiConnect24 (WiiLink)", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Patch the Wii Shop / Channels to use community WiiLink servers. Off by default to avoid third-party network calls."));
    s.append(opt("Wii", "SD Card", "dolphin_wii_sd_card", "Insert SD Card", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Make a virtual SD card visible to Wii software. Required for save imports, channel installs, and SD-using homebrew."));
    s.append(opt("Wii", "SD Card", "dolphin_wii_sd_card_writes", "Allow Writes to SD Card", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "When off, the SD card is read-only — protects a shared image from accidental modification."));
    s.append(opt("Wii", "SD Card", "dolphin_wii_sd_card_folder_sync", "Auto-Sync SD with Folder", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Mirror the SD card image from a host folder."));
    s.append(opt("Wii", "SD Card", "dolphin_wii_sd_card_size", "SD Card Size", "0",
        {{"Auto","0"},{"64 MiB","67108864"},{"128 MiB","134217728"},{"256 MiB","268435456"},{"512 MiB","536870912"},{"1 GiB","1073741824"},{"2 GiB","2147483648"},{"4 GiB (SDHC)","4294967296"},{"8 GiB (SDHC)","8589934592"},{"16 GiB (SDHC)","17179869184"},{"32 GiB (SDHC)","34359738368"}},
        "Capacity of the virtual SD card. Auto uses the image file as-is."));
```

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp
git commit -m "SP7: host opt() helper + 37 Audio/Core/system schema rows

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 14: Host — Recommended rollup card (16 rows) + hub cards

**Files:**
- Modify: `RetroNest-Project/cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp`

> Recommended rows re-reference existing keys with **identical default + value list** (the fidelity script merges duplicate host rows; only a default mismatch errors). Reproduce each home row's exact value list and default.

- [ ] **Step 1: Append the Recommended rows immediately after the Wii rows (still before `return s;`)**

```cpp
    // ═══ Recommended (curated cross-category VIEW — re-references existing keys) ═══
    // Performance
    s.append(opt("Recommended", "Performance", "dolphin_cpu_thread", "Dual Core (Speed Hack)", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Run CPU and GPU emulation on separate threads. Big speed gain for most games."));
    s.append(opt("Recommended", "Performance", "dolphin_shader_compilation", "Shader Compilation", "Specialized",
        {{"Specialized (Default)","Specialized"},{"Exclusive Ubershaders","Exclusive Ubershaders"},{"Hybrid Ubershaders","Hybrid Ubershaders"},{"Skip Drawing","Skip Drawing"}},
        "Ubershader modes avoid shader-compile stutter at a GPU cost."));
    s.append(opt("Recommended", "Performance", "dolphin_wait_for_shaders", "Compile Shaders Before Starting", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Pre-compile the shader pipeline before launching. Slower start, smoother first minutes."));
    // Performance Hacks
    s.append(opt("Recommended", "Performance Hacks", "dolphin_store_efb_to_texture", "Store EFB Copies to Texture Only", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip the slow EFB->RAM copy. Big speed boost; can break games that read EFB on the CPU."));
    s.append(opt("Recommended", "Performance Hacks", "dolphin_store_xfb_to_texture", "Store XFB Copies to Texture Only", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip the slow XFB->RAM copy. Big speed boost; required off for games that decode the XFB on the CPU."));
    s.append(opt("Recommended", "Performance Hacks", "dolphin_skip_efb_access", "Skip EFB Access from CPU", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip CPU read-back of the EFB. Faster; disable for games that need accurate EFB access."));
    s.append(opt("Recommended", "Performance Hacks", "dolphin_texcache_accuracy", "Texture Cache Accuracy", "Default",
        {{"Safe (Slowest)","Safe"},{"Default","Default"},{"Fast","Fast"}},
        "Fast = best performance with glitch risk; Safe = full accuracy. Default is balanced."));
    // Visual Quality
    s.append(opt("Recommended", "Visual Quality", "dolphin_internal_resolution", "Internal Resolution", "1x",
        {{"Auto (Window Size)","Auto"},{"Native (1x)","1x"},{"2x","2x"},{"3x","3x"},{"4x","4x"},{"5x","5x"},{"6x (4K)","6x"},{"7x","7x"},{"8x","8x"}},
        "Render scale relative to native. The single biggest knob for visual fidelity."));
    s.append(opt("Recommended", "Visual Quality", "dolphin_aspect_ratio", "Aspect Ratio", "Auto",
        {{"Auto","Auto"},{"Force 16:9","16:9"},{"Force 4:3","4:3"},{"Stretch to Window","Stretch"}},
        "Display aspect ratio. Auto matches the game."));
    s.append(opt("Recommended", "Visual Quality", "dolphin_antialiasing", "Anti-Aliasing", "None",
        {{"None","None"},{"2x MSAA","2x MSAA"},{"4x MSAA","4x MSAA"},{"8x MSAA","8x MSAA"},{"2x SSAA","2x SSAA"},{"4x SSAA","4x SSAA"},{"8x SSAA","8x SSAA"}},
        "Smooths edges. SSAA is far more demanding than MSAA."));
    s.append(opt("Recommended", "Visual Quality", "dolphin_texture_filtering", "Texture Filtering", "Default",
        {{"Default","Default"},{"1x Anisotropic","1x Anisotropic"},{"2x Anisotropic","2x Anisotropic"},{"4x Anisotropic","4x Anisotropic"},{"8x Anisotropic","8x Anisotropic"},{"16x Anisotropic","16x Anisotropic"},{"Force Nearest and 1x Anisotropic","Force Nearest and 1x Anisotropic"},{"Force Linear and 1x Anisotropic","Force Linear and 1x Anisotropic"},{"Force Linear and 2x Anisotropic","Force Linear and 2x Anisotropic"},{"Force Linear and 4x Anisotropic","Force Linear and 4x Anisotropic"},{"Force Linear and 8x Anisotropic","Force Linear and 8x Anisotropic"},{"Force Linear and 16x Anisotropic","Force Linear and 16x Anisotropic"}},
        "Sharpens distant textures and optionally forces a magnification filter."));
    s.append(opt("Recommended", "Visual Quality", "dolphin_widescreen_hack", "Widescreen Hack", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Force 4:3 games to render in widescreen. May produce artifacts."));
    // Audio
    s.append(opt("Recommended", "Audio", "dolphin_dsp_engine", "DSP Emulation Engine", "HLE",
        {{"HLE (Recommended)","HLE"},{"LLE Recompiler (Slow)","LLE Recompiler"},{"LLE Interpreter (Very Slow)","LLE Interpreter"}},
        "HLE is fast and compatible. Use LLE only when a game needs it."));
    s.append(opt("Recommended", "Audio", "dolphin_volume", "Volume", "100",
        {{"0%","0"},{"10%","10"},{"20%","20"},{"30%","30"},{"40%","40"},{"50%","50"},{"60%","60"},{"70%","70"},{"80%","80"},{"90%","90"},{"100%","100"}},
        "Master output volume."));
    // Convenience
    s.append(opt("Recommended", "Convenience", "dolphin_enable_cheats", "Enable Cheats", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Process AR/Gecko cheat codes."));
    s.append(opt("Recommended", "Convenience", "dolphin_skip_ipl", "Skip GameCube Boot Animation", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip the GC IPL boot sequence and start the game directly."));
```

> **Important** — the fidelity check compares **stored values + default only** (not display labels), and merges all host rows for a key. So each Recommended row must use the **same default** and a **subset of the same stored values** as its home row; labels are free. The two re-referenced graphics rows to double-check are `dolphin_texcache_accuracy` (home default `Default`, stored values `Safe`/`Default`/`Fast` — read line ~228 of this file) and `dolphin_texture_filtering` (home stored-value list — read line ~174). If a home row's stored values differ from what's written above, copy the home row's stored values verbatim. The fidelity check (Task 16) catches any mismatch.

- [ ] **Step 2: Add the six hub cards** (replace the `settingsHubCards()` body at line 336-342)

```cpp
QVector<SettingsHubCard> DolphinLibretroAdapter::settingsHubCards() const {
    return {
        {QStringLiteral("\U00002B50"), "Recommended",
         "The dozen settings that matter most",
         "Recommended", 0, 0},
        {QStringLiteral("\U0001F3A8"), "Graphics",
         "Resolution, AA, enhancements, hacks, OSD",
         "Graphics", 0, 1},
        {QStringLiteral("\U0001F50A"), "Audio",
         "DSP engine, latency, volume, surround",
         "Audio", 1, 0},
        {QStringLiteral("\U00002699\U0000FE0F"), "General",
         "Dual core, cheats, speed limit, region",
         "General", 1, 1},
        {QStringLiteral("\U0001F6E0\U0000FE0F"), "Advanced",
         "CPU core, MMU, overclock, timing",
         "Advanced", 2, 0},
        {QStringLiteral("\U0001F4BE"), "GameCube",
         "IPL, language, memory-card slots, SP1",
         "GameCube", 2, 1},
        {QStringLiteral("\U0001F3AE"), "Wii",
         "USB keyboard, WiiLink, SD card",
         "Wii", 3, 0},
    };
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp
git commit -m "SP7: Recommended rollup card (16 re-referenced rows) + 6 new hub cards

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 15: Host shape test — relax `noDuplicateKeys` to per-category + new assertions

**Files:**
- Modify: `RetroNest-Project/cpp/tests/test_dolphin_libretro_schema.cpp`

- [ ] **Step 1: Replace `noDuplicateKeys()` with a per-category version + add a Recommended-reference test**

Replace the `noDuplicateKeys()` slot (lines 54-61) with:

```cpp
    void noDuplicateKeysPerCategory() {
        // Recommended deliberately re-references keys that live in other
        // categories, so global uniqueness no longer holds — enforce
        // uniqueness within each category instead.
        DolphinLibretroAdapter a;
        QSet<QString> seen;  // "category/key"
        for (const auto& d : a.settingsSchema()) {
            const QString id = d.category + "/" + d.key;
            QVERIFY2(!seen.contains(id),
                qPrintable(QString("duplicate key '%1' in category '%2'").arg(d.key).arg(d.category)));
            seen.insert(id);
        }
    }

    void recommendedRows_haveAHomeElsewhere() {
        DolphinLibretroAdapter a;
        QSet<QString> nonRecKeys;
        for (const auto& d : a.settingsSchema())
            if (d.category != "Recommended") nonRecKeys.insert(d.key);
        for (const auto& d : a.settingsSchema())
            if (d.category == "Recommended")
                QVERIFY2(nonRecKeys.contains(d.key),
                    qPrintable(QString("Recommended row '%1' has no home row in another category").arg(d.key)));
    }
```

- [ ] **Step 2: Build + run the host schema test**

Run (use the project's existing ctest wiring for this target):
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64
cmake --build . --target test_dolphin_libretro_schema 2>&1 | tail -15
ctest -R DolphinLibretroSchema --output-on-failure
```
Expected: all slots pass — including `everyDefault_isInOptions` (proves each row's default ∈ its options), `noDuplicateKeysPerCategory`, `recommendedRows_haveAHomeElsewhere`, and `hubCards_referencedByEntries` (all 7 cards resolve).

> If `test_dolphin_libretro_schema` is not a standalone CMake target, build the test suite the project normally builds and filter with the `ctest -R DolphinLibretroSchema` name — the SP6 memory notes confirm `ctest -R DolphinLibretroSchema` is the invocation.

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/tests/test_dolphin_libretro_schema.cpp
git commit -m "SP7: relax host dup-key test to per-category; assert Recommended rows have a home

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 16: Schema fidelity → 90/90

**Files:**
- (Run only; modify `dolphin_libretro_adapter.cpp` or the new `.cpp` blocks only if drift is reported)

- [ ] **Step 1: Run the fidelity check**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target check_schema_fidelity 2>&1 | tail -40
```
Expected: `Schema fidelity OK: 90 core keys, 90 host keys, byte-for-byte match.`

- [ ] **Step 2: If drift is reported, fix it**

The report lists each mismatch as `key 'X': default differs …` or `… host has values not declared in core …`. For each: make the host row's `{label, value}` pairs / default match the core push_back's `{value, label}` / default **exactly** (remember the swapped pair order). Re-run Step 1 until clean. Then re-run the host test (Task 15 Step 2) and the core unit test (Task 10 Step 4) to confirm nothing regressed.

- [ ] **Step 3: Commit (only if drift fixes were needed)**

```bash
# In whichever repo changed:
git add -A && git commit -m "SP7: fix schema drift to reach 90/90 fidelity

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 17: Build universal dylib, deploy, heavier smoke

**Files:** (none — build/deploy/verify)

- [ ] **Step 1: Build both arches + lipo + deploy** (per the build-setup memory)

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro
PATH=/usr/local/bin:$PATH arch -x86_64 /usr/local/bin/cmake --build build-libretro-x86_64 --target dolphin_libretro
lipo -create build-libretro/Source/Core/DolphinLibretro/dolphin_libretro.dylib \
            build-libretro-x86_64/Source/Core/DolphinLibretro/dolphin_libretro.dylib \
  -output /Users/mark/Documents/RetroNest/emulators/libretro/cores/dolphin_libretro.dylib
```
(Rebuild RetroNest.app + macdeployqt + codesign only if the host adapter changed enough to need it; the adapter is compiled into RetroNest, so rebuild the host app and re-deploy per the build-setup memory.)

- [ ] **Step 2: Launch with the diagnostic log and smoke-test**

```bash
pkill -x RetroNest; sleep 1
RETRONEST_DOLPHIN_LOG=1 arch -x86_64 \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
  > /tmp/retronest.log 2>&1 &
```

Verify, across **NTSC + PAL** and **GameCube + Wii** titles:
- [ ] Each title still **boots** with default settings.
- [ ] `/tmp/retronest.log` shows `[CoreOptions] resolved audio: …` and `[CoreOptions] resolved core: …` matching the UI (default: `dual_core=0 cpu_core=JIT mmu=0 …`).
- [ ] In the settings UI: the Recommended card + Audio/General/Advanced/GameCube/Wii cards all open and render their rows.
- [ ] Change **DSP Emulation Engine** HLE→LLE Recompiler and **Volume** 100→50, relaunch, confirm the readback log + audible behaviour change and that `options.json` persisted them.
- [ ] Change **CPU Emulation Engine** away from JIT and back; confirm a game still boots with JIT (the default). Spot-check **MMU enabled** does not regress a known-good title (then turn it back off).

- [ ] **Step 3: Update the SP7 memory note to SHIPPED**

After smoke passes, update `~/.claude/.../memory/sp7-dolphin-audio-core-settings-prep.md` (and the SP6 note's "Next" pointer + `dolphin-libretro-build-setup.md` status) to record SP7 as shipped with the final commit ranges — mirroring how SP6 was recorded.

---

## Appendix A — Resolved Config symbol/type/default reference

All symbols are namespace `Config`, declared in `Source/Core/Core/Config/MainSettings.h`. Use these verbatim in `Apply`.

| host key | `Config::SYMBOL` | type | Dolphin default |
|---|---|---|---|
| dolphin_dsp_engine (hle) | `MAIN_DSP_HLE` | bool | true |
| dolphin_dsp_engine (jit) | `MAIN_DSP_JIT` | bool | true |
| dolphin_audio_latency | `MAIN_AUDIO_LATENCY` | int | 20 |
| dolphin_dpl2_decoder | `MAIN_DPL2_DECODER` | bool | false |
| dolphin_dpl2_quality | `MAIN_DPL2_QUALITY` | `AudioCommon::DPL2Quality` (int 0-3) | High=2 |
| dolphin_audio_buffer_size | `MAIN_AUDIO_BUFFER_SIZE` | int | 80 |
| dolphin_audio_fill_gaps | `MAIN_AUDIO_FILL_GAPS` | bool | true |
| dolphin_audio_preserve_pitch | `MAIN_AUDIO_PRESERVE_PITCH` | bool | false |
| dolphin_audio_mute_on_unthrottle | `MAIN_AUDIO_MUTE_ON_DISABLED_SPEED_LIMIT` | bool | false |
| dolphin_volume | `MAIN_AUDIO_VOLUME` | int | 100 |
| dolphin_cpu_thread | `MAIN_CPU_THREAD` | bool | **false** (non-Android) |
| dolphin_enable_cheats | `MAIN_ENABLE_CHEATS` | bool | false |
| dolphin_load_game_into_memory | `MAIN_LOAD_GAME_INTO_MEMORY` | bool | false |
| dolphin_override_region_settings | `MAIN_OVERRIDE_REGION_SETTINGS` | bool | false |
| dolphin_emulation_speed | `MAIN_EMULATION_SPEED` | float | 1.0 |
| dolphin_fallback_region | `MAIN_FALLBACK_REGION` | `DiscIO::Region` | runtime locale (plan defaults to NTSC-U=1) |
| dolphin_cpu_core | `MAIN_CPU_CORE` | `PowerPC::CPUCore` | `DefaultCPUCore()` = JITARM64(4)/JIT64(1) |
| dolphin_mmu | `MAIN_MMU` | bool | false |
| dolphin_pause_on_panic | `MAIN_PAUSE_ON_PANIC` | bool | false |
| dolphin_accurate_cpu_cache | `MAIN_ACCURATE_CPU_CACHE` | bool | false |
| dolphin_correct_time_drift | `MAIN_CORRECT_TIME_DRIFT` | bool | false |
| dolphin_rush_frame_presentation | `MAIN_RUSH_FRAME_PRESENTATION` | bool | false |
| dolphin_smooth_early_presentation | `MAIN_SMOOTH_EARLY_PRESENTATION` | bool | false |
| dolphin_overclock_enable | `MAIN_OVERCLOCK_ENABLE` | bool | false |
| dolphin_overclock | `MAIN_OVERCLOCK` | float | 1.0 |
| dolphin_vi_overclock_enable | `MAIN_VI_OVERCLOCK_ENABLE` | bool | false |
| dolphin_vi_overclock | `MAIN_VI_OVERCLOCK` | float | 1.0 |
| dolphin_skip_ipl | `MAIN_SKIP_IPL` | bool | true |
| dolphin_gc_language | `MAIN_GC_LANGUAGE` | int (0-5) | 0 |
| dolphin_slot_a | `MAIN_SLOT_A` | `ExpansionInterface::EXIDeviceType` | MemoryCardFolder=8 |
| dolphin_slot_b | `MAIN_SLOT_B` | `ExpansionInterface::EXIDeviceType` | None=255 |
| dolphin_serial_port_1 | `MAIN_SERIAL_PORT_1` | `ExpansionInterface::EXIDeviceType` | None=255 |
| dolphin_wii_keyboard | `MAIN_WII_KEYBOARD` | bool | false |
| dolphin_enable_wiilink | `MAIN_WII_WIILINK_ENABLE` | bool | false |
| dolphin_wii_sd_card | `MAIN_WII_SD_CARD` | bool | true |
| dolphin_wii_sd_card_writes | `MAIN_ALLOW_SD_WRITES` | bool | true |
| dolphin_wii_sd_card_folder_sync | `MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC` | bool | false |
| dolphin_wii_sd_card_size | `MAIN_WII_SD_CARD_FILESIZE` | u64 | 0 |

**Enum values:** `PowerPC::CPUCore`: Interpreter=0, JIT64=1, JITARM64=4, CachedInterpreter=5 (`PowerPC.h`). `EXIDeviceType`: Dummy=0, MemoryCard=1, Microphone=4, Ethernet/BBA-TAP=5, Baseboard=6, Gecko=7, MemoryCardFolder=8, AGP=9, EthernetXLink=10, EthernetTapServer=11, EthernetBuiltIn=12, ModemTapServer=13, None=255 (`Core/HW/EXI/EXI_Device.h`). `DiscIO::Region`: NTSC_J=0, NTSC_U=1, PAL=2, Unknown=3, NTSC_K=4 (`DiscIO/Enums.h`). GC language: 0=English…5=Dutch (combobox index, not `DiscIO::Language`).
