# SP6 — Dolphin libretro Graphics settings schema — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the Dolphin libretro core's options infrastructure from scratch (mirroring `pcsx2-libretro`) and populate it with a curated ~50 of Dolphin's Graphics settings, so they appear in RetroNest's per-emulator settings UI and take effect on the next launch.

**Architecture:** Core side gains `CoreOptions.{h,cpp}` (a thin aggregator: `Resolved`, `BuildDefinitions`, `EmitCoreOptionsV2`, `ReadResolved`) + `CoreOptionsGraphics.{h,cpp}` (`AppendDefinitions` literal `push_back` blocks, a primitive-typed `Values` struct, `Parse` (string→Values, incl. AA/texture-filtering fan-out), `Apply` (Values→`Config::SetCurrent(Config::GFX_*, …)`)). `EmitCoreOptionsV2` wires into `retro_set_environment`; `ReadResolved` + `Graphics::Apply` wire into `retro_load_game` next to the existing Metal-backend line (the config layer set there lands in Dolphin's highest-priority `CurrentRun` layer and survives boot). Host side extends the existing `DolphinLibretroAdapter` with `settingsSchema()`/`settingsHubCards()`/`settingsCategoriesWithSubTabs()`/`previewSpec()`. A ported `check_schema_fidelity.py` + CMake target guards core↔host value parity at build time; a `test_dolphin_libretro_schema.cpp` QtTest guards host shape; a standalone `tools/test_core_options.cpp` unit-tests `Parse`.

**Tech Stack:** C++20, libretro core options v2 (`libretro.h` `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2`), Dolphin `Config::Info<T>` layer, CMake/Ninja, Qt 6 / RetroNest C++ host, Python 3 (fidelity script).

**Spec:** `docs/superpowers/specs/2026-05-25-sp6-dolphin-libretro-graphics-settings-design.md`

**Predecessors:** SP0–SP3 + SP5 done (GC + Wii boot, render on Metal, play audio, take input). SP4 (Vulkan) deferred. The Dolphin core exposes **zero** libretro options today — SP6 builds that.

**Working directories:**
- Core: `/Users/mark/Documents/Projects/dolphin-libretro/` on branch `libretro`.
- Host: `/Users/mark/Documents/Projects/RetroNest-Project/` on branch `main`.

**Build/run:** see memory `dolphin-libretro-build-setup.md` — arm64 + x86_64 cmake/ninja, universal dylib via `lipo`, `Data/Sys` copy into the RetroNest .app, macdeployqt + codesign after a RetroNest rebuild, run x86_64 under Rosetta with `RETRONEST_DOLPHIN_LOG=1` to capture the core's stderr.

---

## Key facts established during prep (verbatim references — do not re-derive)

### Config-load order (the correctness linchpin)
`UICommon::Init()` (called in `retro_init`) reads Dolphin.ini/GFX.ini into the `Base` config layer once (via `Config::AddLayer(GenerateBaseConfigLoader())`, whose `Layer` ctor calls `Load()`). `BootManager::BootCore` does **NOT** reload config from disk. The read-priority `SEARCH_ORDER` is `CurrentRun` > … > `Base`. Therefore **`Config::SetCurrent(Config::GFX_*, value)` called in `retro_load_game` before boot lands in `CurrentRun`, shadows the disk value, and is read by the video backend at boot.** The existing `Config::SetCurrent(Config::MAIN_GFX_BACKEND, std::string("Metal"))` at `LibretroFrontend.cpp:206` is exactly such a slot — SP6's `Graphics::Apply` goes right after it.

### Core wire points (`Source/Core/DolphinLibretro/LibretroFrontend.cpp`)
- `retro_set_environment` (lines 77–86): stashes `DolphinLibretro::Frontend::g_environ_cb = cb` and `Environment::SetEnvironmentCallback(cb)`. Add `EmitCoreOptionsV2(cb)` here.
- `retro_load_game` (lines 188–223): line 206 sets the Metal backend; line 222 returns `s_emu_thread->StartGame(...)` (→ `BootManager::BootCore`). Add `ReadResolved` + `Graphics::Apply` between 206 and 210.
- The env callback for runtime queries is `DolphinLibretro::Environment::GetEnvironmentCallback()` (returns the stashed `s_environ_cb`).
- Logging helper: `DolphinLibretro::Environment::Log(RETRO_LOG_*, fmt, …)` (the project's `FrontendLog` analog).

### `Config::SetCurrent` API (`Source/Core/Common/Config/Config.h:124`)
```cpp
template <typename InfoT, typename ValueT>
void SetCurrent(const Info<InfoT>& info, const ValueT& value);  // writes LayerType::CurrentRun (highest read priority, not persisted)
```

### Dolphin Config symbols + enums for every kept option
Enums (`Source/Core/VideoCommon/VideoConfig.h`, all `enum class … : int`, sequential from 0 unless noted):
- `AspectMode`: Auto=0, ForceWide=1, ForceStandard=2, Stretch=3, Custom=4, CustomStretch=5, Raw=6.
- `ShaderCompilationMode`: Synchronous=0, SynchronousUberShaders=1, AsynchronousUberShaders=2, AsynchronousSkipRendering=3.
- `TextureFilteringMode`: Default=0, Nearest=1, Linear=2.
- `AnisotropicFilteringMode`: Default=-1, Force1x=0, Force2x=1, Force4x=2, Force8x=3, Force16x=4 (value is `x` in `1<<x`).
- `OutputResamplingMode`: Default=0, Bilinear=1, BSpline=2, MitchellNetravali=3, CatmullRom=4, SharpBilinear=5, AreaSampling=6.
- `StereoMode`: Off=0, SideBySide=1, TopAndBottom=2, Anaglyph=3, QuadBuffer=4, Passive=5.

`GFX_EFB_SCALE` is a plain `Info<int>` ("InternalResolution"): 0=Auto, 1=Native, N=Nx. `GFX_MSAA` is `Info<u32>` (raw sample count, 1=off); `GFX_SSAA` is `Info<bool>`. The per-option symbol/type/default are in the option tables in Tasks 3–7.

### Schema-fidelity script mechanics (`pcsx2-libretro/tools/check_schema_fidelity.py`)
- `CORE_BLOCK_RE` matches `out.push_back({ "key", "desc", nullptr, "info"…, nullptr, nullptr, { …values… }, "default" })` — the exact 8-field layout with three `nullptr`s (desc_categorized, info_categorized, category_key). **Core `push_back` blocks must match this shape or they're invisible to the checker.** Core value pairs are `{"value","Display"}` (value first); `VALUE_PAIR_RE` takes the first string.
- `HOST_BLOCK_RE` matches `s.append(opt(…))` or `s.append(gopt(…))` positional calls. Host value pairs are `{"Display","value"}` (value second); `HOST_PAIR_RE` takes the second string.
- It diffs, per key: **default must match**, and **value-sets must be equal** (bidirectional); plus **key-sets must be equal** (every host key in core and vice-versa). Labels are NOT compared. Exit 1 + stderr report on drift; exit 0 + "Schema fidelity OK" otherwise. `--core <glob>` + `--host <path>` are required args.

### Host schema shape (mirror `Pcsx2LibretroAdapter`)
- `settingsSchema()` builds rows via two lambda helpers. `gopt(subcategory, group, key, label, def, valuesAndLabels, tooltip[, dependsOn])` hardcodes `category="Graphics"`, `storage=LibretroOption`, `type=Combo`, pushes `subcategory`. (`opt(...)` is the same minus the hardcoded category; SP6 uses `gopt` for all rows since every option is under Graphics.)
- `settingsCategoriesWithSubTabs() → {"Graphics"}` (declared inline in the .h) makes the sub-tab bar appear; distinct `subcategory` strings become the sub-tabs.
- `settingsHubCards()` returns `SettingsHubCard{ icon, title, descriptor, categoryKey, row, col[, rowSpan, colSpan] }`.
- `previewSpec(category, subcategory)` returns `PreviewSpec{ previewType, keyToProperty }` for specific pages, `{}` elsewhere.

### Struct field reference
- `SettingDef` (`cpp/src/core/setting_def.h`): `enum Type { Bool, Int, Float, String, Combo }`; `enum class Storage { Ini, LibretroOption, FrontendSetting }`. Relevant fields: `category, subcategory, group, key, label, tooltip, type, defaultValue, options (QVector<QPair<QString,QString>> = (label,value)), dependsOn, storage`.
- `SettingsHubCard` (`cpp/src/adapters/emulator_adapter.h:116`): `{ QString icon; QString title; QString descriptor; QString categoryKey; int row; int col; int rowSpan=1; int colSpan=1; }`.
- `PreviewSpec` (`cpp/src/core/preview_spec.h:18`): `{ QString previewType; QHash<QString,QString> keyToProperty; }`.

### Modular core pattern (mirror exactly)
- `CoreOptions::BuildDefinitions()` = function-local static `std::vector<retro_core_option_v2_definition>`, `reserve`, calls each `Category::AppendDefinitions(v)`, then pushes ONE all-`nullptr` terminator. Returns by const ref.
- `EmitCoreOptionsV2(cb)`: build `retro_core_options_v2{ .categories=nullptr, .definitions=BuildDefinitions().data() }`, call `cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts)`; a `false` return is informational (host lacks *categories*; options still register).
- `ReadResolved(cb)`: default-construct `Resolved r{}`, call each `Category::Parse(cb, r.<member>)`, return `r`.
- A category's `Parse` only overwrites a `Values` field when `GET_VARIABLE` returns a value (else the member-initializer default stands). `Apply` is `#ifndef CORE_OPTIONS_TEST_ONLY` so the standalone test compiles without engine deps.

---

## File map

### Core repo: `/Users/mark/Documents/Projects/dolphin-libretro/` (branch `libretro`)

| Path | Action | Responsibility |
|---|---|---|
| `Source/Core/DolphinLibretro/CoreOptions.h` | CREATE | `Resolved` (holds `Graphics::Values graphics`) + `BuildDefinitions`/`EmitCoreOptionsV2`/`ReadResolved` decls. libretro.h only. |
| `Source/Core/DolphinLibretro/CoreOptions.cpp` | CREATE | Aggregator impls + the `FrontendLog`/test-shim include split. |
| `Source/Core/DolphinLibretro/CoreOptionsGraphics.h` | CREATE | `Values` (primitive-typed, nested per sub-tab) + `AppendDefinitions`/`Parse`/`Apply` decls. |
| `Source/Core/DolphinLibretro/CoreOptionsGraphics.cpp` | CREATE | `AppendDefinitions` (literal blocks), `Parse` (string→Values + fan-out), `Apply` (Values→Config, guarded). |
| `Source/Core/DolphinLibretro/LibretroFrontend.cpp` | MODIFY | `EmitCoreOptionsV2` in `retro_set_environment`; `ReadResolved`+`Graphics::Apply` in `retro_load_game`. |
| `Source/Core/DolphinLibretro/CMakeLists.txt` | MODIFY | Register the two new `.cpp` in `target_sources`; add `check_schema_fidelity` custom target. |
| `Source/Core/DolphinLibretro/tools/check_schema_fidelity.py` | CREATE | Ported fidelity script (core↔host value/default diff). |
| `Source/Core/DolphinLibretro/tools/test_core_options.cpp` | CREATE | Standalone `Parse` + `BuildDefinitions` unit test (`-DCORE_OPTIONS_TEST_ONLY`). |

### Host repo: `/Users/mark/Documents/Projects/RetroNest-Project/` (branch `main`)

| Path | Action | Responsibility |
|---|---|---|
| `cpp/src/adapters/libretro/dolphin_libretro_adapter.h` | MODIFY | Declare `settingsSchema`/`settingsHubCards`/`settingsCategoriesWithSubTabs`/`previewSpec`. |
| `cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp` | MODIFY | Implement them (~50 `gopt` rows + hub card + OSD preview). |
| `cpp/tests/test_dolphin_libretro_schema.cpp` | CREATE | QtTest shape guard (mirror `test_ppsspp_libretro_schema.cpp`). |
| `cpp/CMakeLists.txt` | MODIFY | Register the `test_dolphin_libretro_schema` target (mirror the ppsspp block). |

---

## Value-string conventions (used by every option)

- **Bool options** → 2-value combos. Core declares `{ {"enabled","Enabled"}, {"disabled","Disabled"} }` (value,label); host declares `{ {"Enabled","enabled"}, {"Disabled","disabled"} }` (label,value). Value-set `{enabled,disabled}` matches. `Parse` maps `"enabled"→true`.
- **Enum/int combos** → readable value strings (e.g. `"Auto"`, `"4x"`, `"4x MSAA"`). The value string is what's stored in `options.json`; `Parse` maps it to the primitive; `Apply` casts to the Dolphin enum.
- **Inverted options** (3 of them — "Skip EFB Access", "Ignore Format Changes", "Disable Bounding Box", "Manual Texture Sampling") keep the standalone's user-facing label; the inversion lives in `Apply` (`Config::SetCurrent(sym, !v.flag)`), never in a host `inverted` flag.
- **Defaults follow the deleted standalone's presented defaults** (recover via `git -C /Users/mark/Documents/Projects/RetroNest-Project show 03f48ae^:cpp/src/adapters/dolphin_adapter.cpp`), converting `True→enabled` / `False→disabled`. These become the effective defaults (`Apply` always writes, overriding Dolphin's own config defaults — matching standalone behavior). The one place this matters: `DisableCopyFilter` (Dolphin default `true`, standalone presented `False` → option default `disabled`).

---

## Task overview

1. Scaffold `CoreOptions.{h,cpp}` + `CoreOptionsGraphics.{h,cpp}` (empty), register in CMake, build.
2. Wire `EmitCoreOptionsV2` + `ReadResolved`/`Apply` into the frontend (still no options → no-op), build.
3. **General** sub-tab (5) + create the `test_core_options.cpp` harness.
4. **Enhancements** sub-tab (15) — incl. AA + texture-filtering fan-out (the key Parse tests).
5. **Hacks** sub-tab (14) — incl. the 3 inverted bools.
6. **Advanced** sub-tab (9).
7. **On-Screen Display** sub-tab (10).
8. Host `DolphinLibretroAdapter` settings overrides (~50 `gopt` rows + hub card + preview).
9. Port `check_schema_fidelity.py` + CMake target; run it green.
10. Host `test_dolphin_libretro_schema.cpp` + CMake target; run it green.
11. Build universal dylib, deploy, rebuild RetroNest.app.
12. Manual smoke (GC + Wii: change a setting, relaunch, verify + `options.json`).
13. Update auto-memory.

---

## Task 1: Scaffold the CoreOptions modules

**Files:**
- Create: `Source/Core/DolphinLibretro/CoreOptions.h`, `CoreOptions.cpp`, `CoreOptionsGraphics.h`, `CoreOptionsGraphics.cpp`
- Modify: `Source/Core/DolphinLibretro/CMakeLists.txt` (add the two `.cpp` after `EmuThread.cpp` on line 19)

- [ ] **Step 1: Create `CoreOptionsGraphics.h`** (Values struct + 3 decls; primitives only so the test compiles without engine deps)

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP6: Graphics-category libretro core options for dolphin-libretro.
// Values are stored as PRIMITIVES (int/bool/std::string) so this header
// and Parse() carry no Dolphin engine dependency — the standalone unit
// test compiles CoreOptionsGraphics.cpp with -DCORE_OPTIONS_TEST_ONLY and
// links nothing else. Apply() (the only engine-touching function) is
// guarded out of that build.

#pragma once

#include "libretro.h"
#include <string>
#include <vector>

namespace DolphinLibretro::CoreOptions::Graphics
{

// Resolved Graphics values. Member-initializers are the defaults applied
// when an option is unset / the host returns NULL. They mirror the
// deleted standalone adapter's presented defaults.
struct Values
{
    struct General {
        std::string aspect_ratio        = "Auto";        // AspectMode
        bool        vsync                = true;
        bool        precision_frame_timing = true;       // MAIN_PRECISION_FRAME_TIMING
        std::string shader_compilation   = "Specialized"; // ShaderCompilationMode
        bool        wait_for_shaders     = false;
    } general;

    struct Enhancements {
        int  internal_resolution = 1;     // GFX_EFB_SCALE (0=Auto,1=Native,N=Nx)
        int  msaa                = 1;     // GFX_MSAA raw sample count (1=off)  ─┐ AA fan-out
        bool ssaa                = false; // GFX_SSAA                            ─┘
        int  aniso               = -1;    // AnisotropicFilteringMode int       ─┐ tex-filter fan-out
        int  force_filter        = 0;     // TextureFilteringMode int           ─┘
        int  output_resampling   = 0;     // OutputResamplingMode int
        bool scaled_efb_copy     = true;
        bool per_pixel_lighting  = false;
        bool widescreen_hack     = false;
        bool force_true_color    = true;
        bool disable_fog         = false;
        bool arbitrary_mipmap_detection = true;
        bool disable_copy_filter = false; // NB: Dolphin's own default is true; standalone presented false
        bool hdr_output          = false;
        int  stereo_mode         = 0;     // StereoMode int
        bool stereo_swap_eyes    = false;
        bool stereo_per_eye_full = false;
    } enhancements;

    struct Hacks {
        bool skip_efb_access      = true;  // → GFX_HACK_EFB_ACCESS_ENABLE = !v (inverted)
        bool ignore_format_changes = true; // → GFX_HACK_EFB_EMULATE_FORMAT_CHANGES = !v (inverted)
        bool store_efb_to_texture = true;  // → GFX_HACK_SKIP_EFB_COPY_TO_RAM
        bool defer_efb_copies     = true;  // → GFX_HACK_DEFER_EFB_COPIES
        int  texcache_accuracy    = 128;   // → GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES (0/128/512)
        bool gpu_texture_decoding = false; // → GFX_ENABLE_GPU_TEXTURE_DECODING
        bool store_xfb_to_texture = true;  // → GFX_HACK_SKIP_XFB_COPY_TO_RAM
        bool immediate_xfb        = false; // → GFX_HACK_IMMEDIATE_XFB
        bool skip_duplicate_xfbs  = true;  // → GFX_HACK_SKIP_DUPLICATE_XFBS
        bool fast_depth_calc      = true;  // → GFX_FAST_DEPTH_CALC
        bool disable_bounding_box = true;  // → GFX_HACK_BBOX_ENABLE = !v (inverted)
        bool vertex_rounding      = false; // → GFX_HACK_VERTEX_ROUNDING
        bool save_texcache_to_state = true; // → GFX_SAVE_TEXTURE_CACHE_TO_STATE
        bool vbi_skip             = false; // → GFX_HACK_VI_SKIP
    } hacks;

    struct Advanced {
        bool load_custom_textures   = false; // → GFX_HIRES_TEXTURES
        bool prefetch_custom_textures = false; // → GFX_CACHE_HIRES_TEXTURES
        bool enable_graphics_mods   = false; // → GFX_MODS_ENABLE
        bool crop                   = false; // → GFX_CROP
        bool backend_multithreading = true;  // → GFX_BACKEND_MULTITHREADING
        bool prefer_vs_expansion    = false; // → GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION
        bool cpu_cull               = false; // → GFX_CPU_CULL
        bool defer_efb_invalidation = false; // → GFX_HACK_EFB_DEFER_INVALIDATION
        bool manual_texture_sampling = false; // → GFX_HACK_FAST_TEXTURE_SAMPLING = !v (inverted)
    } advanced;

    struct Osd {
        bool show_messages   = true;  // → MAIN_OSD_MESSAGES
        int  font_size       = 13;    // → MAIN_OSD_FONT_SIZE (13/18/24/36)
        bool show_fps        = false; // → GFX_SHOW_FPS
        bool show_ftimes     = false; // → GFX_SHOW_FTIMES
        bool show_vps        = false; // → GFX_SHOW_VPS
        bool show_vtimes     = false; // → GFX_SHOW_VTIMES
        bool show_speed      = false; // → GFX_SHOW_SPEED
        bool show_graphs     = false; // → GFX_SHOW_GRAPHS
        bool show_speed_colors = true; // → GFX_SHOW_SPEED_COLORS
        int  perf_samp_window = 1000;  // → GFX_PERF_SAMP_WINDOW (250/500/1000/2000/5000)
    } osd;
};

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out);
void Parse(retro_environment_t cb, Values& out);
void Apply(const Values& v);   // #ifndef CORE_OPTIONS_TEST_ONLY in the .cpp

} // namespace DolphinLibretro::CoreOptions::Graphics
```

- [ ] **Step 2: Create `CoreOptions.h`** (aggregator)

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP6: libretro core-options aggregator for dolphin-libretro. Mirrors
// pcsx2-libretro/CoreOptions.{h,cpp}. Graphics is the only category in
// SP6; SP7 appends Audio + Core/Advanced modules here with no refactor.

#pragma once

#include "libretro.h"
#include "CoreOptionsGraphics.h"
#include <vector>

namespace DolphinLibretro::CoreOptions
{

struct Resolved
{
    Graphics::Values graphics{};
};

// Build (or return the cached) master option-definitions vector. First
// call concatenates each category's AppendDefinitions + the libretro
// terminator; the storage is a process-lifetime function-local static.
const std::vector<retro_core_option_v2_definition>& BuildDefinitions();

// Emit the schema to the host. Call once from retro_set_environment.
bool EmitCoreOptionsV2(retro_environment_t cb);

// Query the host for current user values. Call once at the top of
// retro_load_game (before BootCore). NULL/unknown values fall back to
// the Values member-initializer defaults.
Resolved ReadResolved(retro_environment_t cb);

} // namespace DolphinLibretro::CoreOptions
```

- [ ] **Step 3: Create `CoreOptions.cpp`** (full aggregator impl — does not change with later tasks)

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptions.h"

#ifdef CORE_OPTIONS_TEST_ONLY
#include <cstdarg>
#include <cstdio>
namespace { void TestLog(int, const char* fmt, ...) {
    std::va_list ap; va_start(ap, fmt); std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr); va_end(ap);
} }
#define CORE_OPTIONS_LOG(level, ...) TestLog(level, __VA_ARGS__)
#else
#include "LibretroEnvironment.h"   // DolphinLibretro::Environment::Log
#define CORE_OPTIONS_LOG(level, ...) DolphinLibretro::Environment::Log(level, __VA_ARGS__)
#endif

namespace DolphinLibretro::CoreOptions
{

const std::vector<retro_core_option_v2_definition>& BuildDefinitions()
{
    static const std::vector<retro_core_option_v2_definition> kAll = [] {
        std::vector<retro_core_option_v2_definition> v;
        v.reserve(64);  // ~50 Graphics + terminator + headroom for SP7
        Graphics::AppendDefinitions(v);
        // libretro terminator — must be the final entry.
        v.push_back({
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            {{nullptr, nullptr}},
            nullptr
        });
        return v;
    }();
    return kAll;
}

bool EmitCoreOptionsV2(retro_environment_t cb)
{
    if (!cb) return false;
    retro_core_options_v2 opts{};
    opts.categories  = nullptr;  // uncategorized; host adapter groups via SettingDef.category
    opts.definitions = const_cast<retro_core_option_v2_definition*>(
        BuildDefinitions().data());
    const bool ok = cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts);
    if (!ok) {
        CORE_OPTIONS_LOG(RETRO_LOG_WARN,
            "[CoreOptions] Host does not support core-option categories "
            "(options still registered; GET_VARIABLE will work)");
    }
    return ok;
}

Resolved ReadResolved(retro_environment_t cb)
{
    Resolved r{};
    if (!cb) return r;
    Graphics::Parse(cb, r.graphics);
    return r;
}

} // namespace DolphinLibretro::CoreOptions
```

- [ ] **Step 4: Create `CoreOptionsGraphics.cpp`** (skeleton — empty bodies; Tasks 3–7 fill them)

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsGraphics.h"

#include <cstdlib>
#include <cstring>

#ifndef CORE_OPTIONS_TEST_ONLY
#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "VideoCommon/VideoConfig.h"
#endif

namespace DolphinLibretro::CoreOptions::Graphics
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& /*out*/)
{
    // Tasks 3-7 push one literal block per option here.
}

void Parse(retro_environment_t cb, Values& out)
{
    if (!cb) return;
    // Tasks 3-7 add per-option query() reads here.
    (void)out;
}

#ifndef CORE_OPTIONS_TEST_ONLY
void Apply(const Values& /*v*/)
{
    // Tasks 3-7 add Config::SetCurrent(...) calls here.
}
#endif

} // namespace DolphinLibretro::CoreOptions::Graphics
```

- [ ] **Step 5: Register the new sources in `CMakeLists.txt`**

In `Source/Core/DolphinLibretro/CMakeLists.txt`, the `target_sources` block (lines 12–20) must become:

```cmake
target_sources(dolphin_libretro PRIVATE
    LibretroFrontend.cpp
    LibretroEnvironment.cpp
    LibretroMetalContext.mm
    LibretroAudioStream.cpp
    LibretroInputSource.cpp
    HostStubs.cpp
    EmuThread.cpp
    CoreOptions.cpp
    CoreOptionsGraphics.cpp
)
```

- [ ] **Step 6: Build the arm64 core to verify the scaffold compiles**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -15
```
Expected: `[100%] Built target dolphin_libretro`. Nothing calls CoreOptions yet, so runtime behavior is unchanged. (If `build-libretro` doesn't exist, configure it per the memory `dolphin-libretro-build-setup.md` arm64 block first.)

- [ ] **Step 7: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptions.h Source/Core/DolphinLibretro/CoreOptions.cpp \
        Source/Core/DolphinLibretro/CoreOptionsGraphics.h Source/Core/DolphinLibretro/CoreOptionsGraphics.cpp \
        Source/Core/DolphinLibretro/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP6 Task 1: scaffold CoreOptions + CoreOptionsGraphics modules

Aggregator (Resolved/BuildDefinitions/EmitCoreOptionsV2/ReadResolved)
plus an empty Graphics module (primitive-typed Values + AppendDefinitions
/Parse/Apply stubs). Apply is guarded by CORE_OPTIONS_TEST_ONLY so the
standalone Parse test links no engine deps. No call sites yet.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Wire emit + read/apply into the frontend

**Files:**
- Modify: `Source/Core/DolphinLibretro/LibretroFrontend.cpp` (3 edits)

- [ ] **Step 1: Include the aggregator**

Near the top of `LibretroFrontend.cpp` (with the other project includes), add:
```cpp
#include "CoreOptions.h"
```

- [ ] **Step 2: Emit options in `retro_set_environment`**

Replace the body of `retro_set_environment` (lines 77–86) so the emit call follows the callback stash:
```cpp
RETRO_API void retro_set_environment(retro_environment_t cb)
{
    DolphinLibretro::Frontend::g_environ_cb = cb;
    DolphinLibretro::Environment::SetEnvironmentCallback(cb);

    // SP6: declare core options as soon as the env_cb is available — the
    // only legal time per the libretro spec (before retro_init).
    DolphinLibretro::CoreOptions::EmitCoreOptionsV2(cb);

    // Fetch the frontend log callback if exposed.
    retro_log_callback log_cb{};
    if (cb && cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_cb) && log_cb.log)
        DolphinLibretro::Environment::SetLogCallback(log_cb.log);
}
```

- [ ] **Step 3: Read + apply resolved options in `retro_load_game`**

In `retro_load_game`, immediately AFTER the Metal-backend line (`LibretroFrontend.cpp:206`, `Config::SetCurrent(Config::MAIN_GFX_BACKEND, std::string("Metal"));`) and before `UICommon::InitControllers(s_wsi);`, insert:
```cpp
    // SP6: read user-tweaked Graphics options and write them into Dolphin's
    // CurrentRun config layer. UICommon::Init (in retro_init) already loaded
    // the Base layer from disk; CurrentRun has higher read priority and is
    // NOT reloaded by BootCore, so these win and are read by the video
    // backend at boot. Read once — options take effect on next launch only.
    const auto resolved = DolphinLibretro::CoreOptions::ReadResolved(
        DolphinLibretro::Environment::GetEnvironmentCallback());
    DolphinLibretro::CoreOptions::Graphics::Apply(resolved.graphics);
```

- [ ] **Step 4: Build to verify**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -15
```
Expected: clean build. `Apply` is still a no-op (Tasks 3–7 fill it), so behavior is unchanged, but the option-emit path is now live — a libretro frontend would see an empty (terminator-only) option set.

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/LibretroFrontend.cpp
git commit -m "$(cat <<'EOF'
SP6 Task 2: wire CoreOptions into retro_set_environment + retro_load_game

EmitCoreOptionsV2 declares the (currently empty) schema at environment
time; ReadResolved + Graphics::Apply run in retro_load_game right after
the Metal-backend SetCurrent, writing into the CurrentRun layer that
survives BootCore. Apply is still a no-op until Tasks 3-7 populate it.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: General sub-tab (5 options) + test harness

**Files:**
- Create: `Source/Core/DolphinLibretro/tools/test_core_options.cpp`
- Modify: `Source/Core/DolphinLibretro/CoreOptionsGraphics.cpp` (General slice of AppendDefinitions/Parse/Apply)

### Option table — General (subcategory `"General"`)

| `dolphin_` key | label | values (value→Display) | default | Config symbol (type) | Apply mapping |
|---|---|---|---|---|---|
| `dolphin_aspect_ratio` | Aspect Ratio | `Auto`→Auto, `16:9`→Force 16:9, `4:3`→Force 4:3, `Stretch`→Stretch to Window | `Auto` | `GFX_ASPECT_RATIO` (`AspectMode`) | Auto→Auto(0), 16:9→ForceWide(1), 4:3→ForceStandard(2), Stretch→Stretch(3) |
| `dolphin_vsync` | V-Sync | bool | `enabled` | `GFX_VSYNC` (`bool`) | direct |
| `dolphin_precision_frame_timing` | Precision Frame Timing | bool | `enabled` | `MAIN_PRECISION_FRAME_TIMING` (`bool`) | direct (Main, not GFX) |
| `dolphin_shader_compilation` | Shader Compilation | `Specialized`→Specialized (Default), `Exclusive Ubershaders`→…, `Hybrid Ubershaders`→…, `Skip Drawing`→… | `Specialized` | `GFX_SHADER_COMPILATION_MODE` (`ShaderCompilationMode`) | Specialized→Synchronous(0), Exclusive Ubershaders→SynchronousUberShaders(1), Hybrid Ubershaders→AsynchronousUberShaders(2), Skip Drawing→AsynchronousSkipRendering(3) |
| `dolphin_wait_for_shaders` | Compile Shaders Before Starting | bool | `disabled` | `GFX_WAIT_FOR_SHADERS_BEFORE_STARTING` (`bool`) | direct |

- [ ] **Step 1: Add the General `AppendDefinitions` blocks** (top of `AppendDefinitions`, replacing the comment). Each block is the exact 8-field shape `CORE_BLOCK_RE` requires. Two representative blocks shown in full; the remaining three follow the same shape per the table:

```cpp
    // ── General sub-tab ──
    out.push_back({
        "dolphin_aspect_ratio",
        "Aspect Ratio",
        nullptr,
        "Display aspect ratio. Auto matches the game's native aspect; "
        "Stretch fills the window.",
        nullptr,
        nullptr,
        {
            { "Auto",    "Auto" },
            { "16:9",    "Force 16:9" },
            { "4:3",     "Force 4:3" },
            { "Stretch", "Stretch to Window" },
            { nullptr,   nullptr },
        },
        "Auto",
    });
    out.push_back({
        "dolphin_vsync",
        "V-Sync",
        nullptr,
        "Synchronizes output to the display refresh rate. Reduces tearing.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });
    // dolphin_precision_frame_timing — bool block, default "enabled".
    // dolphin_shader_compilation — combo: Specialized / Exclusive Ubershaders /
    //   Hybrid Ubershaders / Skip Drawing, default "Specialized".
    // dolphin_wait_for_shaders — bool block, default "disabled".
    // (Same 8-field shape; values + defaults per the General table above.)
```

- [ ] **Step 2: Add the General `Parse` reads.** Add the three shared lambdas at the top of `Parse` (once, used by all sub-tabs) then the General reads:

```cpp
    auto query = [&cb](const char* key) -> const char* {
        retro_variable var{};
        var.key = key;
        if (cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
            return var.value;
        return nullptr;
    };
    auto parse_bool = [](const char* s) { return s && std::strcmp(s, "enabled") == 0; };

    // ── General ──
    if (const char* v = query("dolphin_aspect_ratio"))  out.general.aspect_ratio = v;
    if (const char* v = query("dolphin_vsync"))          out.general.vsync = parse_bool(v);
    if (const char* v = query("dolphin_precision_frame_timing"))
        out.general.precision_frame_timing = parse_bool(v);
    if (const char* v = query("dolphin_shader_compilation")) out.general.shader_compilation = v;
    if (const char* v = query("dolphin_wait_for_shaders"))   out.general.wait_for_shaders = parse_bool(v);
```

- [ ] **Step 3: Add the General `Apply` writes** (inside the `#ifndef CORE_OPTIONS_TEST_ONLY` `Apply`). Two string-combo helpers map the readable value to the enum:

```cpp
    // ── General ──
    {
        const std::string& ar = v.general.aspect_ratio;
        AspectMode mode = AspectMode::Auto;
        if      (ar == "16:9")    mode = AspectMode::ForceWide;
        else if (ar == "4:3")     mode = AspectMode::ForceStandard;
        else if (ar == "Stretch") mode = AspectMode::Stretch;
        Config::SetCurrent(Config::GFX_ASPECT_RATIO, mode);
    }
    Config::SetCurrent(Config::GFX_VSYNC, v.general.vsync);
    Config::SetCurrent(Config::MAIN_PRECISION_FRAME_TIMING, v.general.precision_frame_timing);
    {
        const std::string& sc = v.general.shader_compilation;
        ShaderCompilationMode mode = ShaderCompilationMode::Synchronous;
        if      (sc == "Exclusive Ubershaders") mode = ShaderCompilationMode::SynchronousUberShaders;
        else if (sc == "Hybrid Ubershaders")    mode = ShaderCompilationMode::AsynchronousUberShaders;
        else if (sc == "Skip Drawing")          mode = ShaderCompilationMode::AsynchronousSkipRendering;
        Config::SetCurrent(Config::GFX_SHADER_COMPILATION_MODE, mode);
    }
    Config::SetCurrent(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING, v.general.wait_for_shaders);
```

- [ ] **Step 4: Create the standalone test harness** `tools/test_core_options.cpp` (fake env_cb + first General cases):

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for DolphinLibretro::CoreOptions::Graphics::Parse +
// CoreOptions::BuildDefinitions. Not part of the dolphin_libretro target.
// Manual compile (CORE_OPTIONS_TEST_ONLY gates out Apply + engine deps):
//
//   cd Source/Core/DolphinLibretro/tools
//   clang++ -std=c++20 -I.. -I../../.. -DCORE_OPTIONS_TEST_ONLY \
//       test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsGraphics.cpp \
//       -o test_core_options && ./test_core_options

#include "../CoreOptions.h"
#include "../CoreOptionsGraphics.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

using namespace DolphinLibretro::CoreOptions;

namespace fake {
    std::map<std::string, std::string> vars;
    void reset() { vars.clear(); }
}
static bool fake_cb(unsigned cmd, void* data) {
    if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE) {
        auto* v = static_cast<retro_variable*>(data);
        if (!v || !v->key) return false;
        auto it = fake::vars.find(v->key);
        if (it == fake::vars.end()) { v->value = nullptr; return false; }
        v->value = it->second.c_str();
        return true;
    }
    return false;
}

static int failures = 0;
static void ck_int(const char* l, long got, long want) {
    bool ok = got == want;
    std::printf("[%s] %s: got=%ld want=%ld\n", ok ? "PASS" : "FAIL", l, got, want);
    if (!ok) ++failures;
}
static void ck_bool(const char* l, bool got, bool want) {
    bool ok = got == want;
    std::printf("[%s] %s: got=%d want=%d\n", ok ? "PASS" : "FAIL", l, got, want);
    if (!ok) ++failures;
}
static void ck_str(const char* l, const std::string& got, const char* want) {
    bool ok = got == want;
    std::printf("[%s] %s: got='%s' want='%s'\n", ok ? "PASS" : "FAIL", l, got.c_str(), want);
    if (!ok) ++failures;
}

int main() {
    // ── BuildDefinitions structure ──
    const auto& defs = BuildDefinitions();
    // Last entry must be the all-null terminator.
    ck_bool("terminator present", defs.back().key == nullptr, true);
    // No duplicate keys among the non-terminator entries.
    {
        std::map<std::string,int> seen; bool dup = false;
        for (const auto& d : defs) if (d.key) if (++seen[d.key] > 1) dup = true;
        ck_bool("no duplicate keys", dup, false);
    }

    // ── General Parse ──
    fake::reset();
    fake::vars["dolphin_aspect_ratio"]  = "16:9";
    fake::vars["dolphin_vsync"]         = "disabled";
    fake::vars["dolphin_shader_compilation"] = "Skip Drawing";
    Graphics::Values g{};
    Graphics::Parse(&fake_cb, g);
    ck_str ("General aspect",   g.general.aspect_ratio, "16:9");
    ck_bool("General vsync",     g.general.vsync, false);
    ck_str ("General shaderc",   g.general.shader_compilation, "Skip Drawing");
    ck_bool("General pft default (unset)", g.general.precision_frame_timing, true);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 5: Compile + run the test — expect PASS**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools
clang++ -std=c++20 -I.. -I../../.. -DCORE_OPTIONS_TEST_ONLY \
    test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsGraphics.cpp \
    -o test_core_options && ./test_core_options
```
Expected: all `[PASS]`, `0 failure(s)`. (The `-I../../..` puts `Source/Core` on the include path so `libretro.h` and the module headers resolve; `CORE_OPTIONS_TEST_ONLY` compiles out `Apply`, so no Dolphin engine link is needed.)

- [ ] **Step 6: Build the core target too** (confirms `Apply`'s real Config writes compile)

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -8
```
Expected: clean build.

- [ ] **Step 7: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsGraphics.cpp Source/Core/DolphinLibretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP6 Task 3: Graphics/General options + test harness

5 General options (aspect ratio, vsync, precision frame timing, shader
compilation, wait-for-shaders) wired through AppendDefinitions/Parse/
Apply. New standalone test_core_options.cpp asserts BuildDefinitions
structure + General Parse.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Enhancements sub-tab (15 options) — incl. AA + texture-filtering fan-out

**Files:**
- Modify: `CoreOptionsGraphics.cpp` (Enhancements slice), `tools/test_core_options.cpp` (fan-out cases)

### Option table — Enhancements (subcategory `"Enhancements"`)

| `dolphin_` key | label | values | default | Config symbol (type) | Apply mapping |
|---|---|---|---|---|---|
| `dolphin_internal_resolution` | Internal Resolution | `Auto`,`1x`,`2x`,`3x`,`4x`,`5x`,`6x`,`7x`,`8x` | `1x` | `GFX_EFB_SCALE` (`int`) | Auto→0, `Nx`→N |
| `dolphin_antialiasing` | Anti-Aliasing | `None`,`2x MSAA`,`4x MSAA`,`8x MSAA`,`2x SSAA`,`4x SSAA`,`8x SSAA` | `None` | `GFX_MSAA` (`u32`) + `GFX_SSAA` (`bool`) | **fan-out** → `msaa`,`ssaa` |
| `dolphin_texture_filtering` | Texture Filtering | `Default`,`1x`/`2x`/`4x`/`8x`/`16x Anisotropic`,`Force Nearest and 1x Anisotropic`,`Force Linear and {1,2,4,8,16}x Anisotropic` (12) | `Default` | `GFX_ENHANCE_MAX_ANISOTROPY` (`AnisotropicFilteringMode`) + `GFX_ENHANCE_FORCE_TEXTURE_FILTERING` (`TextureFilteringMode`) | **fan-out** → `aniso`,`force_filter` |
| `dolphin_output_resampling` | Output Resampling | `Default`,`Bilinear`,`Bicubic B-Spline`,`Bicubic Mitchell-Netravali`,`Bicubic Catmull-Rom`,`Sharp Bilinear`,`Area Sampling` | `Default` | `GFX_ENHANCE_OUTPUT_RESAMPLING` (`OutputResamplingMode`) | →0..6 in listed order |
| `dolphin_scaled_efb_copy` | Scaled EFB Copy | bool | `enabled` | `GFX_HACK_COPY_EFB_SCALED` (`bool`) | direct |
| `dolphin_per_pixel_lighting` | Per-Pixel Lighting | bool | `disabled` | `GFX_ENABLE_PIXEL_LIGHTING` (`bool`) | direct |
| `dolphin_widescreen_hack` | Widescreen Hack | bool | `disabled` | `GFX_WIDESCREEN_HACK` (`bool`) | direct |
| `dolphin_force_true_color` | Force 24-Bit Color | bool | `enabled` | `GFX_ENHANCE_FORCE_TRUE_COLOR` (`bool`) | direct |
| `dolphin_disable_fog` | Disable Fog | bool | `disabled` | `GFX_DISABLE_FOG` (`bool`) | direct |
| `dolphin_arbitrary_mipmap_detection` | Arbitrary Mipmap Detection | bool | `enabled` | `GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION` (`bool`) | direct |
| `dolphin_disable_copy_filter` | Disable Copy Filter | bool | `disabled` | `GFX_ENHANCE_DISABLE_COPY_FILTER` (`bool`) | direct (NB Dolphin default true; we present disabled) |
| `dolphin_hdr_output` | HDR Post-Processing | bool | `disabled` | `GFX_ENHANCE_HDR_OUTPUT` (`bool`) | direct |
| `dolphin_stereo_mode` | Stereoscopic 3D Mode | `Off`,`Side-by-Side`,`Top-and-Bottom`,`Anaglyph`,`HDMI 3D`,`Passive` | `Off` | `GFX_STEREO_MODE` (`StereoMode`) | Off→0,SBS→1,TAB→2,Anaglyph→3,HDMI 3D→QuadBuffer(4),Passive→5 |
| `dolphin_stereo_swap_eyes` | Swap Eyes | bool | `disabled` | `GFX_STEREO_SWAP_EYES` (`bool`) | direct |
| `dolphin_stereo_per_eye_full` | Full Resolution Per Eye | bool | `disabled` | `GFX_STEREO_PER_EYE_RESOLUTION_FULL` (`bool`) | direct |

### Fan-out value→field maps
- **Anti-Aliasing** (`dolphin_antialiasing` → `msaa`,`ssaa`): `None`→(1,false); `2x MSAA`→(2,false); `4x MSAA`→(4,false); `8x MSAA`→(8,false); `2x SSAA`→(2,true); `4x SSAA`→(4,true); `8x SSAA`→(8,true).
- **Texture Filtering** (`dolphin_texture_filtering` → `aniso`,`force_filter`), aniso = AnisotropicFilteringMode int (Default=-1,1x=0,2x=1,4x=2,8x=3,16x=4), force = TextureFilteringMode int (Default=0,Nearest=1,Linear=2): `Default`→(-1,0); `1x Anisotropic`→(0,0); `2x Anisotropic`→(1,0); `4x Anisotropic`→(2,0); `8x Anisotropic`→(3,0); `16x Anisotropic`→(4,0); `Force Nearest and 1x Anisotropic`→(0,1); `Force Linear and 1x Anisotropic`→(0,2); `Force Linear and 2x Anisotropic`→(1,2); `Force Linear and 4x Anisotropic`→(2,2); `Force Linear and 8x Anisotropic`→(3,2); `Force Linear and 16x Anisotropic`→(4,2).

- [ ] **Step 1: Add the Enhancements `AppendDefinitions` blocks** — one literal block per row per the table. Representative `Auto`/`Nx` resolution + the AA combo shown in full:

```cpp
    // ── Enhancements sub-tab ──
    out.push_back({
        "dolphin_internal_resolution",
        "Internal Resolution",
        nullptr,
        "Render scale relative to native (1x = 640x528; 6x is roughly 4K).",
        nullptr,
        nullptr,
        {
            { "Auto", "Auto (Window Size)" },
            { "1x", "Native (1x)" }, { "2x", "2x" }, { "3x", "3x" },
            { "4x", "4x" }, { "5x", "5x" }, { "6x", "6x (4K)" },
            { "7x", "7x" }, { "8x", "8x" },
            { nullptr, nullptr },
        },
        "1x",
    });
    out.push_back({
        "dolphin_antialiasing",
        "Anti-Aliasing",
        nullptr,
        "Reduces aliasing on edges. SSAA is far more demanding than MSAA "
        "but also anti-aliases shader effects.",
        nullptr,
        nullptr,
        {
            { "None", "None" },
            { "2x MSAA", "2x MSAA" }, { "4x MSAA", "4x MSAA" }, { "8x MSAA", "8x MSAA" },
            { "2x SSAA", "2x SSAA" }, { "4x SSAA", "4x SSAA" }, { "8x SSAA", "8x SSAA" },
            { nullptr, nullptr },
        },
        "None",
    });
    // ... remaining 13 Enhancements blocks per the table (texture filtering
    //     = the 12-value combo above; output resampling = 7 values;
    //     the rest are bool blocks). Defaults per the table.
```

- [ ] **Step 2: Add Enhancements `Parse` (incl. fan-out)**. Add a `parse_int` lambda next to the others, then:

```cpp
    auto parse_int = [](const char* s, int fb) {
        if (!s) return fb; char* e = nullptr; long n = std::strtol(s, &e, 10);
        return e == s ? fb : static_cast<int>(n);
    };
    // ── Enhancements ──
    if (const char* v = query("dolphin_internal_resolution"))
        out.enhancements.internal_resolution = (std::strcmp(v, "Auto") == 0) ? 0 : parse_int(v, 1);
    if (const char* v = query("dolphin_antialiasing")) {
        // fan-out → (msaa sample count, ssaa flag)
        struct AA { int msaa; bool ssaa; };
        const std::map<std::string, AA> t = {
            {"None",{1,false}}, {"2x MSAA",{2,false}}, {"4x MSAA",{4,false}}, {"8x MSAA",{8,false}},
            {"2x SSAA",{2,true}}, {"4x SSAA",{4,true}}, {"8x SSAA",{8,true}},
        };
        auto it = t.find(v); if (it == t.end()) it = t.find("None");
        out.enhancements.msaa = it->second.msaa;
        out.enhancements.ssaa = it->second.ssaa;
    }
    if (const char* v = query("dolphin_texture_filtering")) {
        // fan-out → (AnisotropicFilteringMode int, TextureFilteringMode int)
        struct TF { int aniso; int force; };
        const std::map<std::string, TF> t = {
            {"Default",{-1,0}},
            {"1x Anisotropic",{0,0}}, {"2x Anisotropic",{1,0}}, {"4x Anisotropic",{2,0}},
            {"8x Anisotropic",{3,0}}, {"16x Anisotropic",{4,0}},
            {"Force Nearest and 1x Anisotropic",{0,1}},
            {"Force Linear and 1x Anisotropic",{0,2}}, {"Force Linear and 2x Anisotropic",{1,2}},
            {"Force Linear and 4x Anisotropic",{2,2}}, {"Force Linear and 8x Anisotropic",{3,2}},
            {"Force Linear and 16x Anisotropic",{4,2}},
        };
        auto it = t.find(v); if (it == t.end()) it = t.find("Default");
        out.enhancements.aniso = it->second.aniso;
        out.enhancements.force_filter = it->second.force;
    }
    if (const char* v = query("dolphin_output_resampling")) {
        const std::map<std::string,int> t = {
            {"Default",0},{"Bilinear",1},{"Bicubic B-Spline",2},
            {"Bicubic Mitchell-Netravali",3},{"Bicubic Catmull-Rom",4},
            {"Sharp Bilinear",5},{"Area Sampling",6},
        };
        auto it = t.find(v); out.enhancements.output_resampling = (it == t.end()) ? 0 : it->second;
    }
    if (const char* v = query("dolphin_scaled_efb_copy"))   out.enhancements.scaled_efb_copy = parse_bool(v);
    if (const char* v = query("dolphin_per_pixel_lighting")) out.enhancements.per_pixel_lighting = parse_bool(v);
    if (const char* v = query("dolphin_widescreen_hack"))    out.enhancements.widescreen_hack = parse_bool(v);
    if (const char* v = query("dolphin_force_true_color"))   out.enhancements.force_true_color = parse_bool(v);
    if (const char* v = query("dolphin_disable_fog"))        out.enhancements.disable_fog = parse_bool(v);
    if (const char* v = query("dolphin_arbitrary_mipmap_detection")) out.enhancements.arbitrary_mipmap_detection = parse_bool(v);
    if (const char* v = query("dolphin_disable_copy_filter")) out.enhancements.disable_copy_filter = parse_bool(v);
    if (const char* v = query("dolphin_hdr_output"))         out.enhancements.hdr_output = parse_bool(v);
    if (const char* v = query("dolphin_stereo_mode")) {
        const std::map<std::string,int> t = {
            {"Off",0},{"Side-by-Side",1},{"Top-and-Bottom",2},{"Anaglyph",3},{"HDMI 3D",4},{"Passive",5},
        };
        auto it = t.find(v); out.enhancements.stereo_mode = (it == t.end()) ? 0 : it->second;
    }
    if (const char* v = query("dolphin_stereo_swap_eyes"))   out.enhancements.stereo_swap_eyes = parse_bool(v);
    if (const char* v = query("dolphin_stereo_per_eye_full")) out.enhancements.stereo_per_eye_full = parse_bool(v);
```

- [ ] **Step 3: Add Enhancements `Apply`** (casts primitives → Dolphin enums):

```cpp
    // ── Enhancements ──
    Config::SetCurrent(Config::GFX_EFB_SCALE, v.enhancements.internal_resolution);
    Config::SetCurrent(Config::GFX_MSAA, static_cast<u32>(v.enhancements.msaa));
    Config::SetCurrent(Config::GFX_SSAA, v.enhancements.ssaa);
    Config::SetCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY,
                       static_cast<AnisotropicFilteringMode>(v.enhancements.aniso));
    Config::SetCurrent(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                       static_cast<TextureFilteringMode>(v.enhancements.force_filter));
    Config::SetCurrent(Config::GFX_ENHANCE_OUTPUT_RESAMPLING,
                       static_cast<OutputResamplingMode>(v.enhancements.output_resampling));
    Config::SetCurrent(Config::GFX_HACK_COPY_EFB_SCALED, v.enhancements.scaled_efb_copy);
    Config::SetCurrent(Config::GFX_ENABLE_PIXEL_LIGHTING, v.enhancements.per_pixel_lighting);
    Config::SetCurrent(Config::GFX_WIDESCREEN_HACK, v.enhancements.widescreen_hack);
    Config::SetCurrent(Config::GFX_ENHANCE_FORCE_TRUE_COLOR, v.enhancements.force_true_color);
    Config::SetCurrent(Config::GFX_DISABLE_FOG, v.enhancements.disable_fog);
    Config::SetCurrent(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION, v.enhancements.arbitrary_mipmap_detection);
    Config::SetCurrent(Config::GFX_ENHANCE_DISABLE_COPY_FILTER, v.enhancements.disable_copy_filter);
    Config::SetCurrent(Config::GFX_ENHANCE_HDR_OUTPUT, v.enhancements.hdr_output);
    Config::SetCurrent(Config::GFX_STEREO_MODE, static_cast<StereoMode>(v.enhancements.stereo_mode));
    Config::SetCurrent(Config::GFX_STEREO_SWAP_EYES, v.enhancements.stereo_swap_eyes);
    Config::SetCurrent(Config::GFX_STEREO_PER_EYE_RESOLUTION_FULL, v.enhancements.stereo_per_eye_full);
```

- [ ] **Step 4: Add fan-out test cases** to `tools/test_core_options.cpp` `main()` (before the final printf):

```cpp
    // ── Enhancements fan-out ──
    fake::reset();
    fake::vars["dolphin_antialiasing"]     = "4x SSAA";
    fake::vars["dolphin_texture_filtering"] = "Force Linear and 4x Anisotropic";
    fake::vars["dolphin_internal_resolution"] = "Auto";
    Graphics::Values e{};
    Graphics::Parse(&fake_cb, e);
    ck_int ("AA msaa",            e.enhancements.msaa, 4);
    ck_bool("AA ssaa",            e.enhancements.ssaa, true);
    ck_int ("TexFilter aniso",    e.enhancements.aniso, 2);     // Force4x
    ck_int ("TexFilter force",    e.enhancements.force_filter, 2); // Linear
    ck_int ("InternalRes Auto→0", e.enhancements.internal_resolution, 0);

    fake::reset();
    fake::vars["dolphin_antialiasing"] = "None";
    Graphics::Values e2{};
    Graphics::Parse(&fake_cb, e2);
    ck_int ("AA None msaa", e2.enhancements.msaa, 1);
    ck_bool("AA None ssaa", e2.enhancements.ssaa, false);
```

- [ ] **Step 5: Compile + run the test — expect PASS**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools
clang++ -std=c++20 -I.. -I../../.. -DCORE_OPTIONS_TEST_ONLY \
    test_core_options.cpp ../CoreOptions.cpp ../CoreOptionsGraphics.cpp \
    -o test_core_options && ./test_core_options
```
Expected: all `[PASS]`, `0 failure(s)`.

- [ ] **Step 6: Build the core target** — `cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -8` → clean.

- [ ] **Step 7: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/CoreOptionsGraphics.cpp Source/Core/DolphinLibretro/tools/test_core_options.cpp
git commit -m "$(cat <<'EOF'
SP6 Task 4: Graphics/Enhancements options + fan-out tests

15 Enhancements options including the two complex combos whose multi-key
split lives in Parse: Anti-Aliasing -> (GFX_MSAA, GFX_SSAA) and Texture
Filtering -> (GFX_ENHANCE_MAX_ANISOTROPY, GFX_ENHANCE_FORCE_TEXTURE_
FILTERING). Test covers both fan-outs + Auto resolution.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Hacks sub-tab (14 options) — incl. the inverted bools

**Files:** Modify `CoreOptionsGraphics.cpp` (Hacks slice), `tools/test_core_options.cpp` (inversion cases)

### Option table — Hacks (subcategory `"Hacks"`). The four **inverted** rows store the user-facing meaning; `Apply` writes `!value` (or, for accuracy, the mapped value) to Config.

| `dolphin_` key | label | values | default | Config symbol (type) | Apply mapping |
|---|---|---|---|---|---|
| `dolphin_skip_efb_access` | Skip EFB Access from CPU | bool | `enabled` | `GFX_HACK_EFB_ACCESS_ENABLE` (`bool`) | **inverted**: set `!v` |
| `dolphin_ignore_format_changes` | Ignore Format Changes | bool | `enabled` | `GFX_HACK_EFB_EMULATE_FORMAT_CHANGES` (`bool`) | **inverted**: set `!v` |
| `dolphin_store_efb_to_texture` | Store EFB Copies to Texture Only | bool | `enabled` | `GFX_HACK_SKIP_EFB_COPY_TO_RAM` (`bool`) | direct |
| `dolphin_defer_efb_copies` | Defer EFB Copies to RAM | bool | `enabled` | `GFX_HACK_DEFER_EFB_COPIES` (`bool`) | direct |
| `dolphin_texcache_accuracy` | Texture Cache Accuracy | `Safe`→Safe, `Default`→Default, `Fast`→Fast | `Default` | `GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES` (`int`) | Safe→0, Default→128, Fast→512 |
| `dolphin_gpu_texture_decoding` | GPU Texture Decoding | bool | `disabled` | `GFX_ENABLE_GPU_TEXTURE_DECODING` (`bool`) | direct |
| `dolphin_store_xfb_to_texture` | Store XFB Copies to Texture Only | bool | `enabled` | `GFX_HACK_SKIP_XFB_COPY_TO_RAM` (`bool`) | direct |
| `dolphin_immediate_xfb` | Immediately Present XFB | bool | `disabled` | `GFX_HACK_IMMEDIATE_XFB` (`bool`) | direct |
| `dolphin_skip_duplicate_xfbs` | Skip Presenting Duplicate Frames | bool | `enabled` | `GFX_HACK_SKIP_DUPLICATE_XFBS` (`bool`) | direct |
| `dolphin_fast_depth_calc` | Fast Depth Calculation | bool | `enabled` | `GFX_FAST_DEPTH_CALC` (`bool`) | direct |
| `dolphin_disable_bounding_box` | Disable Bounding Box | bool | `enabled` | `GFX_HACK_BBOX_ENABLE` (`bool`) | **inverted**: set `!v` |
| `dolphin_vertex_rounding` | Vertex Rounding | bool | `disabled` | `GFX_HACK_VERTEX_ROUNDING` (`bool`) | direct |
| `dolphin_save_texcache_to_state` | Save Texture Cache to State | bool | `enabled` | `GFX_SAVE_TEXTURE_CACHE_TO_STATE` (`bool`) | direct |
| `dolphin_vbi_skip` | VBI Skip | bool | `disabled` | `GFX_HACK_VI_SKIP` (`bool`) | direct |

- [ ] **Step 1: Add Hacks `AppendDefinitions` blocks** — bool blocks + the `texcache_accuracy` combo (`{ {"Safe","Safe"},{"Default","Default"},{"Fast","Fast"} }`, default `Default`), per the table.

- [ ] **Step 2: Add Hacks `Parse`** (all `parse_bool` except accuracy):

```cpp
    // ── Hacks ──
    if (const char* v = query("dolphin_skip_efb_access"))       out.hacks.skip_efb_access = parse_bool(v);
    if (const char* v = query("dolphin_ignore_format_changes")) out.hacks.ignore_format_changes = parse_bool(v);
    if (const char* v = query("dolphin_store_efb_to_texture"))  out.hacks.store_efb_to_texture = parse_bool(v);
    if (const char* v = query("dolphin_defer_efb_copies"))      out.hacks.defer_efb_copies = parse_bool(v);
    if (const char* v = query("dolphin_texcache_accuracy")) {
        if      (std::strcmp(v, "Safe") == 0) out.hacks.texcache_accuracy = 0;
        else if (std::strcmp(v, "Fast") == 0) out.hacks.texcache_accuracy = 512;
        else                                  out.hacks.texcache_accuracy = 128;
    }
    if (const char* v = query("dolphin_gpu_texture_decoding"))  out.hacks.gpu_texture_decoding = parse_bool(v);
    if (const char* v = query("dolphin_store_xfb_to_texture"))  out.hacks.store_xfb_to_texture = parse_bool(v);
    if (const char* v = query("dolphin_immediate_xfb"))         out.hacks.immediate_xfb = parse_bool(v);
    if (const char* v = query("dolphin_skip_duplicate_xfbs"))   out.hacks.skip_duplicate_xfbs = parse_bool(v);
    if (const char* v = query("dolphin_fast_depth_calc"))       out.hacks.fast_depth_calc = parse_bool(v);
    if (const char* v = query("dolphin_disable_bounding_box"))  out.hacks.disable_bounding_box = parse_bool(v);
    if (const char* v = query("dolphin_vertex_rounding"))       out.hacks.vertex_rounding = parse_bool(v);
    if (const char* v = query("dolphin_save_texcache_to_state")) out.hacks.save_texcache_to_state = parse_bool(v);
    if (const char* v = query("dolphin_vbi_skip"))              out.hacks.vbi_skip = parse_bool(v);
```

- [ ] **Step 3: Add Hacks `Apply`** (note the `!` on the three inverted rows):

```cpp
    // ── Hacks ── (inverted: option is the user-facing "skip/ignore/disable")
    Config::SetCurrent(Config::GFX_HACK_EFB_ACCESS_ENABLE, !v.hacks.skip_efb_access);
    Config::SetCurrent(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, !v.hacks.ignore_format_changes);
    Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, v.hacks.store_efb_to_texture);
    Config::SetCurrent(Config::GFX_HACK_DEFER_EFB_COPIES, v.hacks.defer_efb_copies);
    Config::SetCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, v.hacks.texcache_accuracy);
    Config::SetCurrent(Config::GFX_ENABLE_GPU_TEXTURE_DECODING, v.hacks.gpu_texture_decoding);
    Config::SetCurrent(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, v.hacks.store_xfb_to_texture);
    Config::SetCurrent(Config::GFX_HACK_IMMEDIATE_XFB, v.hacks.immediate_xfb);
    Config::SetCurrent(Config::GFX_HACK_SKIP_DUPLICATE_XFBS, v.hacks.skip_duplicate_xfbs);
    Config::SetCurrent(Config::GFX_FAST_DEPTH_CALC, v.hacks.fast_depth_calc);
    Config::SetCurrent(Config::GFX_HACK_BBOX_ENABLE, !v.hacks.disable_bounding_box);
    Config::SetCurrent(Config::GFX_HACK_VERTEX_ROUNDING, v.hacks.vertex_rounding);
    Config::SetCurrent(Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE, v.hacks.save_texcache_to_state);
    Config::SetCurrent(Config::GFX_HACK_VI_SKIP, v.hacks.vbi_skip);
```

- [ ] **Step 4: Add Hacks Parse test cases** (accuracy mapping + an inverted default):

```cpp
    // ── Hacks ──
    fake::reset();
    fake::vars["dolphin_texcache_accuracy"] = "Fast";
    fake::vars["dolphin_skip_efb_access"]   = "disabled";
    Graphics::Values h{};
    Graphics::Parse(&fake_cb, h);
    ck_int ("Accuracy Fast→512",      h.hacks.texcache_accuracy, 512);
    ck_bool("skip_efb_access set",    h.hacks.skip_efb_access, false);
    ck_bool("disable_bbox default",   h.hacks.disable_bounding_box, true);  // unset → default
```

- [ ] **Step 5: Compile + run the test (PASS) and build the core (clean)** — same two commands as Task 4 Steps 5–6.

- [ ] **Step 6: Commit**

```bash
git -C /Users/mark/Documents/Projects/dolphin-libretro add Source/Core/DolphinLibretro/CoreOptionsGraphics.cpp Source/Core/DolphinLibretro/tools/test_core_options.cpp
git -C /Users/mark/Documents/Projects/dolphin-libretro commit -m "$(cat <<'EOF'
SP6 Task 5: Graphics/Hacks options

14 Hacks options. Three present the user-facing inverted meaning (Skip
EFB Access, Ignore Format Changes, Disable Bounding Box) and Apply writes
!value to the underlying GFX_HACK_* bool. Texture-cache Accuracy maps
Safe/Default/Fast -> 0/128/512.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Advanced sub-tab (9 options)

**Files:** Modify `CoreOptionsGraphics.cpp` (Advanced slice), `tools/test_core_options.cpp` (one case)

### Option table — Advanced (subcategory `"Advanced"`)

| `dolphin_` key | label | type | default | Config symbol (type) | Apply mapping |
|---|---|---|---|---|---|
| `dolphin_load_custom_textures` | Load Custom Textures | bool | `disabled` | `GFX_HIRES_TEXTURES` (`bool`) | direct |
| `dolphin_prefetch_custom_textures` | Prefetch Custom Textures | bool | `disabled` | `GFX_CACHE_HIRES_TEXTURES` (`bool`) | direct |
| `dolphin_enable_graphics_mods` | Enable Graphics Mods | bool | `disabled` | `GFX_MODS_ENABLE` (`bool`) | direct |
| `dolphin_crop` | Crop | bool | `disabled` | `GFX_CROP` (`bool`) | direct |
| `dolphin_backend_multithreading` | Backend Multithreading | bool | `enabled` | `GFX_BACKEND_MULTITHREADING` (`bool`) | direct |
| `dolphin_prefer_vs_expansion` | Prefer VS for Point/Line Expansion | bool | `disabled` | `GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION` (`bool`) | direct |
| `dolphin_cpu_cull` | Cull Vertices on the CPU | bool | `disabled` | `GFX_CPU_CULL` (`bool`) | direct |
| `dolphin_defer_efb_invalidation` | Defer EFB Cache Invalidation | bool | `disabled` | `GFX_HACK_EFB_DEFER_INVALIDATION` (`bool`) | direct |
| `dolphin_manual_texture_sampling` | Manual Texture Sampling | bool | `disabled` | `GFX_HACK_FAST_TEXTURE_SAMPLING` (`bool`) | **inverted**: set `!v` |

- [ ] **Step 1: Add Advanced `AppendDefinitions`** — 9 bool blocks per the table (subcategory `"Advanced"`, group e.g. `"Utility"`/`"Misc"`/`"Experimental"` — group is cosmetic; use `"Advanced"` if unsure since the host sub-tab is what matters).

- [ ] **Step 2: Add Advanced `Parse`**:

```cpp
    // ── Advanced ──
    if (const char* v = query("dolphin_load_custom_textures"))    out.advanced.load_custom_textures = parse_bool(v);
    if (const char* v = query("dolphin_prefetch_custom_textures")) out.advanced.prefetch_custom_textures = parse_bool(v);
    if (const char* v = query("dolphin_enable_graphics_mods"))    out.advanced.enable_graphics_mods = parse_bool(v);
    if (const char* v = query("dolphin_crop"))                    out.advanced.crop = parse_bool(v);
    if (const char* v = query("dolphin_backend_multithreading"))  out.advanced.backend_multithreading = parse_bool(v);
    if (const char* v = query("dolphin_prefer_vs_expansion"))     out.advanced.prefer_vs_expansion = parse_bool(v);
    if (const char* v = query("dolphin_cpu_cull"))                out.advanced.cpu_cull = parse_bool(v);
    if (const char* v = query("dolphin_defer_efb_invalidation"))  out.advanced.defer_efb_invalidation = parse_bool(v);
    if (const char* v = query("dolphin_manual_texture_sampling")) out.advanced.manual_texture_sampling = parse_bool(v);
```

- [ ] **Step 3: Add Advanced `Apply`** (note the inverted last row):

```cpp
    // ── Advanced ──
    Config::SetCurrent(Config::GFX_HIRES_TEXTURES, v.advanced.load_custom_textures);
    Config::SetCurrent(Config::GFX_CACHE_HIRES_TEXTURES, v.advanced.prefetch_custom_textures);
    Config::SetCurrent(Config::GFX_MODS_ENABLE, v.advanced.enable_graphics_mods);
    Config::SetCurrent(Config::GFX_CROP, v.advanced.crop);
    Config::SetCurrent(Config::GFX_BACKEND_MULTITHREADING, v.advanced.backend_multithreading);
    Config::SetCurrent(Config::GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION, v.advanced.prefer_vs_expansion);
    Config::SetCurrent(Config::GFX_CPU_CULL, v.advanced.cpu_cull);
    Config::SetCurrent(Config::GFX_HACK_EFB_DEFER_INVALIDATION, v.advanced.defer_efb_invalidation);
    // "Manual Texture Sampling" checked = FastTextureSampling OFF.
    Config::SetCurrent(Config::GFX_HACK_FAST_TEXTURE_SAMPLING, !v.advanced.manual_texture_sampling);
```

- [ ] **Step 4: Add one Advanced test case** (the inversion):

```cpp
    // ── Advanced ──
    fake::reset();
    fake::vars["dolphin_manual_texture_sampling"] = "enabled";
    Graphics::Values a{};
    Graphics::Parse(&fake_cb, a);
    ck_bool("manual_texture_sampling set", a.advanced.manual_texture_sampling, true);
    ck_bool("backend_mt default",          a.advanced.backend_multithreading, true);
```

- [ ] **Step 5: Compile + run the test (PASS); build the core (clean).**

- [ ] **Step 6: Commit** — `SP6 Task 6: Graphics/Advanced options` (9 options; Manual Texture Sampling inverts onto GFX_HACK_FAST_TEXTURE_SAMPLING).

---

## Task 7: On-Screen Display sub-tab (10 options)

**Files:** Modify `CoreOptionsGraphics.cpp` (OSD slice), `tools/test_core_options.cpp` (one case)

### Option table — On-Screen Display (subcategory `"On-Screen Display"`)

| `dolphin_` key | label | values | default | Config symbol (type) | Apply mapping |
|---|---|---|---|---|---|
| `dolphin_osd_messages` | Show Messages | bool | `enabled` | `MAIN_OSD_MESSAGES` (`bool`) | direct (Main) |
| `dolphin_osd_font_size` | Font Size | `13`→Small, `18`→Medium, `24`→Large, `36`→Extra Large | `13` | `MAIN_OSD_FONT_SIZE` (`int`) | parse int |
| `dolphin_show_fps` | Show FPS | bool | `disabled` | `GFX_SHOW_FPS` (`bool`) | direct |
| `dolphin_show_ftimes` | Show Frame Times | bool | `disabled` | `GFX_SHOW_FTIMES` (`bool`) | direct |
| `dolphin_show_vps` | Show VPS | bool | `disabled` | `GFX_SHOW_VPS` (`bool`) | direct |
| `dolphin_show_vtimes` | Show VBlank Times | bool | `disabled` | `GFX_SHOW_VTIMES` (`bool`) | direct |
| `dolphin_show_speed` | Show % Speed | bool | `disabled` | `GFX_SHOW_SPEED` (`bool`) | direct |
| `dolphin_show_graphs` | Show Performance Graphs | bool | `disabled` | `GFX_SHOW_GRAPHS` (`bool`) | direct |
| `dolphin_show_speed_colors` | Show Speed Colors | bool | `enabled` | `GFX_SHOW_SPEED_COLORS` (`bool`) | direct |
| `dolphin_perf_samp_window` | Performance Sample Window | `250`→250 ms, `500`→500 ms, `1000`→1000 ms, `2000`→2000 ms, `5000`→5000 ms | `1000` | `GFX_PERF_SAMP_WINDOW` (`int`) | parse int |

- [ ] **Step 1: Add OSD `AppendDefinitions`** — 8 bool blocks + the two int combos (`dolphin_osd_font_size` values `{ {"13","Small"},{"18","Medium"},{"24","Large"},{"36","Extra Large"} }` default `13`; `dolphin_perf_samp_window` values `{ {"250","250 ms"},…,{"5000","5000 ms"} }` default `1000`).

- [ ] **Step 2: Add OSD `Parse`**:

```cpp
    // ── On-Screen Display ──
    if (const char* v = query("dolphin_osd_messages"))   out.osd.show_messages = parse_bool(v);
    if (const char* v = query("dolphin_osd_font_size"))  out.osd.font_size = parse_int(v, 13);
    if (const char* v = query("dolphin_show_fps"))       out.osd.show_fps = parse_bool(v);
    if (const char* v = query("dolphin_show_ftimes"))    out.osd.show_ftimes = parse_bool(v);
    if (const char* v = query("dolphin_show_vps"))       out.osd.show_vps = parse_bool(v);
    if (const char* v = query("dolphin_show_vtimes"))    out.osd.show_vtimes = parse_bool(v);
    if (const char* v = query("dolphin_show_speed"))     out.osd.show_speed = parse_bool(v);
    if (const char* v = query("dolphin_show_graphs"))    out.osd.show_graphs = parse_bool(v);
    if (const char* v = query("dolphin_show_speed_colors")) out.osd.show_speed_colors = parse_bool(v);
    if (const char* v = query("dolphin_perf_samp_window")) out.osd.perf_samp_window = parse_int(v, 1000);
```

- [ ] **Step 3: Add OSD `Apply`**:

```cpp
    // ── On-Screen Display ──
    Config::SetCurrent(Config::MAIN_OSD_MESSAGES, v.osd.show_messages);
    Config::SetCurrent(Config::MAIN_OSD_FONT_SIZE, v.osd.font_size);
    Config::SetCurrent(Config::GFX_SHOW_FPS, v.osd.show_fps);
    Config::SetCurrent(Config::GFX_SHOW_FTIMES, v.osd.show_ftimes);
    Config::SetCurrent(Config::GFX_SHOW_VPS, v.osd.show_vps);
    Config::SetCurrent(Config::GFX_SHOW_VTIMES, v.osd.show_vtimes);
    Config::SetCurrent(Config::GFX_SHOW_SPEED, v.osd.show_speed);
    Config::SetCurrent(Config::GFX_SHOW_GRAPHS, v.osd.show_graphs);
    Config::SetCurrent(Config::GFX_SHOW_SPEED_COLORS, v.osd.show_speed_colors);
    Config::SetCurrent(Config::GFX_PERF_SAMP_WINDOW, v.osd.perf_samp_window);
```

- [ ] **Step 4: Add one OSD test case + a final key-count assertion** to `main()`:

```cpp
    // ── OSD ──
    fake::reset();
    fake::vars["dolphin_show_fps"]      = "enabled";
    fake::vars["dolphin_perf_samp_window"] = "5000";
    Graphics::Values o{};
    Graphics::Parse(&fake_cb, o);
    ck_bool("show_fps set",        o.osd.show_fps, true);
    ck_int ("perf_samp_window",    o.osd.perf_samp_window, 5000);
    ck_bool("show_speed_colors def", o.osd.show_speed_colors, true);

    // ── Full schema size: 53 options + 1 terminator = 54 ──
    ck_int("BuildDefinitions size (53 opts + terminator)",
           static_cast<long>(BuildDefinitions().size()), 54);
```

- [ ] **Step 5: Compile + run the test (PASS); build the core (clean).** The `54` count assertion confirms every option block is present.

- [ ] **Step 6: Commit** — `SP6 Task 7: Graphics/On-Screen Display options` (10 options; completes the 53-option Graphics set + the BuildDefinitions size guard).

---

## Task 8: Host `DolphinLibretroAdapter` settings overrides

**Files:**
- Modify: `cpp/src/adapters/libretro/dolphin_libretro_adapter.h`, `dolphin_libretro_adapter.cpp`

- [ ] **Step 1: Declare the overrides in the header.** In `dolphin_libretro_adapter.h`, before the closing `};`, add:

```cpp
    // SP6: Graphics core-options schema. Keys + values + defaults mirror
    // dolphin-libretro/Source/Core/DolphinLibretro/CoreOptionsGraphics.cpp's
    // push_back table EXACTLY — check_schema_fidelity.py enforces this, and
    // OptionsStore::load drops any host value the core didn't declare.
    QVector<SettingDef> settingsSchema() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    QStringList settingsCategoriesWithSubTabs() const override { return {"Graphics"}; }
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
```

Ensure the includes at the top of the `.h` cover `SettingsHubCard`/`PreviewSpec` (they come via `libretro_adapter.h` → `emulator_adapter.h`; add `#include "core/preview_spec.h"` and `#include "core/setting_def.h"` if not already transitively present — confirm by building).

- [ ] **Step 2: Implement `settingsSchema()` in the `.cpp`** with a `gopt` helper identical to PCSX2's, then ~50 rows. Representative head shown; the full row set mirrors the Task 3–7 tables (label/values/default identical, value-pairs in `(label,value)` order):

```cpp
QVector<SettingDef> DolphinLibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    auto gopt = [](const QString& subcategory, const QString& group,
                   const QString& key, const QString& label, const QString& def,
                   const QVector<QPair<QString,QString>>& valuesAndLabels,
                   const QString& tooltip, const QString& dependsOn = {}) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.category = "Graphics";
        d.subcategory = subcategory;
        d.group = group;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.tooltip = tooltip;
        d.type = SettingDef::Combo;
        d.options = valuesAndLabels;   // (label, value) pairs
        d.dependsOn = dependsOn;
        return d;
    };

    // ── General ──
    s.append(gopt("General", "Basic", "dolphin_aspect_ratio", "Aspect Ratio", "Auto",
        {{"Auto","Auto"}, {"Force 16:9","16:9"}, {"Force 4:3","4:3"}, {"Stretch to Window","Stretch"}},
        "Display aspect ratio. Auto matches the game's native aspect; Stretch fills the window."));
    s.append(gopt("General", "Basic", "dolphin_vsync", "V-Sync", "enabled",
        {{"Enabled","enabled"}, {"Disabled","disabled"}},
        "Synchronizes output to the display refresh rate. Reduces tearing. Takes effect on next launch."));
    // ... precision_frame_timing, shader_compilation, wait_for_shaders (General)
    // ── Enhancements ── internal_resolution, antialiasing, texture_filtering,
    //    output_resampling, scaled_efb_copy, per_pixel_lighting, widescreen_hack,
    //    force_true_color, disable_fog, arbitrary_mipmap_detection,
    //    disable_copy_filter, hdr_output, stereo_mode, stereo_swap_eyes,
    //    stereo_per_eye_full
    // ── Hacks ── skip_efb_access, ignore_format_changes, store_efb_to_texture,
    //    defer_efb_copies, texcache_accuracy, gpu_texture_decoding,
    //    store_xfb_to_texture, immediate_xfb, skip_duplicate_xfbs,
    //    fast_depth_calc, disable_bounding_box, vertex_rounding,
    //    save_texcache_to_state, vbi_skip
    // ── Advanced ── load_custom_textures, prefetch_custom_textures,
    //    enable_graphics_mods, crop, backend_multithreading,
    //    prefer_vs_expansion, cpu_cull, defer_efb_invalidation,
    //    manual_texture_sampling
    // ── On-Screen Display ── osd_messages, osd_font_size, show_fps, show_ftimes,
    //    show_vps, show_vtimes, show_speed, show_graphs, show_speed_colors,
    //    perf_samp_window
    //
    // For EVERY row: subcategory = the sub-tab name above; key/default match the
    // core; the value-set (second element of each pair) equals the core's value
    // list. Bools use {{"Enabled","enabled"},{"Disabled","disabled"}}.

    return s;
}
```

- [ ] **Step 3: Implement `settingsHubCards()`** (one Graphics card):

```cpp
QVector<SettingsHubCard> DolphinLibretroAdapter::settingsHubCards() const {
    return {
        {QStringLiteral("\U0001F3A8"), "Graphics",
         "Resolution, AA, enhancements, hacks, OSD",
         "Graphics", 0, 0},
    };
}
```

- [ ] **Step 4: Implement `previewSpec()`** (OSD preview; empty elsewhere). Use the same property names the PCSX2 OSD preview pane binds — only include keys this schema actually declares:

```cpp
PreviewSpec DolphinLibretroAdapter::previewSpec(const QString& category,
                                                const QString& subcategory) const {
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        return {"osd", {
            {"dolphin_show_fps",          "showFps"},
            {"dolphin_show_speed",        "showSpeed"},
            {"dolphin_show_vps",          "showVps"},
            {"dolphin_show_ftimes",       "showFrameTimes"},
            {"dolphin_osd_messages",      "showSettings"},
        }};
    }
    return {};
}
```

- [ ] **Step 5: Build the host (arm64)**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -20
```
Expected: `[100%] Built target RetroNest`. (Use whichever build dir the project uses — per memory the active host build is `cpp/build-x86_64`. If an arm64 host build dir exists, build that too.)

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/dolphin_libretro_adapter.h cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
SP6 Task 8: DolphinLibretroAdapter Graphics settings schema

settingsSchema() declares ~53 LibretroOption rows across the five Graphics
sub-tabs (General/Enhancements/Hacks/Advanced/On-Screen Display), keys and
value-sets mirroring CoreOptionsGraphics.cpp exactly. One Graphics hub card
+ an On-Screen Display preview. settingsCategoriesWithSubTabs()={Graphics}.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Port `check_schema_fidelity.py` + CMake target

**Files:**
- Create: `Source/Core/DolphinLibretro/tools/check_schema_fidelity.py`
- Modify: `Source/Core/DolphinLibretro/CMakeLists.txt` (add the custom target)

- [ ] **Step 1: Copy the PCSX2 script verbatim and adjust the docstring.**

```bash
cp /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py \
   /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools/check_schema_fidelity.py
chmod +x /Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools/check_schema_fidelity.py
```

The script is core-agnostic: `CORE_BLOCK_RE` matches `out.push_back({...})` and `HOST_BLOCK_RE` matches `s.append(opt|gopt(...))` — both formats SP6 uses unchanged. The only edit is the module docstring's "pcsx2" → "dolphin" wording (cosmetic). Verify the two regexes are present and unmodified after copy.

- [ ] **Step 2: Add the CMake custom target** to `Source/Core/DolphinLibretro/CMakeLists.txt`, after the `add_subdirectory(tools)` line (line 56):

```cmake
# SP6: schema-fidelity check between this core's CoreOptions*.cpp and
# RetroNest-Project's DolphinLibretroAdapter::settingsSchema(). The two
# trees live in separate repos, so the host path is a cache var defaulting
# to the user's known location; override with -DRETRONEST_DOLPHIN_LIBRETRO_ADAPTER=
# at configure time (e.g. for CI).
#
# Invoke: cmake --build build-libretro --target check_schema_fidelity
set(RETRONEST_DOLPHIN_LIBRETRO_ADAPTER
    "$ENV{HOME}/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp"
    CACHE PATH "Host-side libretro adapter for schema-fidelity check"
)
add_custom_target(check_schema_fidelity
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_schema_fidelity.py
            --core "${CMAKE_CURRENT_SOURCE_DIR}/CoreOptions*.cpp"
            --host "${RETRONEST_DOLPHIN_LIBRETRO_ADAPTER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking core/host SettingDef schema fidelity"
    VERBATIM
)
```

- [ ] **Step 3: Reconfigure + run the target — expect OK**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
cmake -B build-libretro -S . >/dev/null 2>&1   # pick up the new target
cmake --build build-libretro --target check_schema_fidelity 2>&1 | tail -20
```
Expected: `Schema fidelity OK: 53 core keys, 53 host keys, byte-for-byte match.` If it reports drift, the message names the exact key + whether it's a default mismatch, a value not in core, or a value not in host — fix the offending side (usually a typo'd value string or a default that differs) and re-run until OK. **Do not proceed with drift.**

- [ ] **Step 4: Quick sanity — run the script directly to confirm it parses both sides**

```bash
/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/tools/check_schema_fidelity.py \
  --core "/Users/mark/Documents/Projects/dolphin-libretro/Source/Core/DolphinLibretro/CoreOptions*.cpp" \
  --host "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/dolphin_libretro_adapter.cpp"
echo "exit=$?"
```
Expected: the OK line + `exit=0`. (Confirms the script found both files and parsed >0 keys each — a `parsed 0` error means the regex didn't match your block shape; re-check the `push_back`/`gopt` formatting against the Key-facts section.)

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/tools/check_schema_fidelity.py Source/Core/DolphinLibretro/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP6 Task 9: port check_schema_fidelity + CMake target

Ports pcsx2-libretro's schema-fidelity script (core-agnostic regexes work
as-is) and adds a check_schema_fidelity custom target. RETRONEST_DOLPHIN_
LIBRETRO_ADAPTER cache var pins the cross-repo host adapter path. Reports
OK: 53/53 keys match.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Host `test_dolphin_libretro_schema.cpp`

**Files:**
- Create: `cpp/tests/test_dolphin_libretro_schema.cpp`
- Modify: `cpp/CMakeLists.txt` (register the target — mirror the `test_ppsspp_libretro_schema` block)

- [ ] **Step 1: Create the test** (in-process QtTest shape guard — mirrors `test_ppsspp_libretro_schema.cpp`; does NOT load a dylib):

```cpp
// cpp/tests/test_dolphin_libretro_schema.cpp
//
// SP6 shape guard for DolphinLibretroAdapter::settingsSchema() +
// settingsHubCards() + previewSpec(). Asserts contracts that prevent
// silent breakage; cross-repo value/default parity is enforced separately
// by dolphin-libretro/tools/check_schema_fidelity.py.

#include <QtTest>
#include <QSet>
#include "adapters/libretro/dolphin_libretro_adapter.h"
#include "core/setting_def.h"

class TestDolphinLibretroSchema : public QObject {
    Q_OBJECT
private slots:
    void everyLibretroKey_hasDolphinPrefix() {
        DolphinLibretroAdapter a;
        for (const auto& d : a.settingsSchema())
            if (d.storage == SettingDef::Storage::LibretroOption)
                QVERIFY2(d.key.startsWith("dolphin_"),
                    qPrintable(QString("LibretroOption key '%1' missing dolphin_ prefix").arg(d.key)));
    }

    void everyRow_isLibretroCombo() {
        DolphinLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            QVERIFY2(d.storage == SettingDef::Storage::LibretroOption,
                qPrintable(QString("row '%1' is not LibretroOption storage").arg(d.key)));
            QVERIFY2(d.type == SettingDef::Combo,
                qPrintable(QString("row '%1' is not Combo type").arg(d.key)));
        }
    }

    void everyDefault_isInOptions() {
        DolphinLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            bool found = false;
            for (const auto& opt : d.options)
                if (opt.second == d.defaultValue) { found = true; break; }
            QVERIFY2(found, qPrintable(QString("row '%1' default '%2' not in its options")
                                           .arg(d.key).arg(d.defaultValue)));
        }
    }

    void allFiveSubTabs_present() {
        DolphinLibretroAdapter a;
        QSet<QString> subs;
        for (const auto& d : a.settingsSchema())
            if (d.category == "Graphics") subs.insert(d.subcategory);
        const QSet<QString> expect{"General","Enhancements","Hacks","Advanced","On-Screen Display"};
        QCOMPARE(subs, expect);
    }

    void noDuplicateKeys() {
        DolphinLibretroAdapter a;
        QSet<QString> seen;
        for (const auto& d : a.settingsSchema()) {
            QVERIFY2(!seen.contains(d.key), qPrintable(QString("duplicate key '%1'").arg(d.key)));
            seen.insert(d.key);
        }
    }

    void hubCards_referencedByEntries() {
        DolphinLibretroAdapter a;
        const auto schema = a.settingsSchema();
        for (const auto& card : a.settingsHubCards()) {
            bool found = false;
            for (const auto& d : schema) if (d.category == card.categoryKey) { found = true; break; }
            QVERIFY2(found, qPrintable(QString("hub card '%1' (categoryKey='%2') matches no row")
                                           .arg(card.title).arg(card.categoryKey)));
        }
    }

    void previewSpec_osd_isOsd_elsewhereEmpty() {
        DolphinLibretroAdapter a;
        const auto osd = a.previewSpec("Graphics", "On-Screen Display");
        QCOMPARE(osd.previewType, QStringLiteral("osd"));
        QVERIFY(a.previewSpec("Graphics", "General").previewType.isEmpty());
    }

    void previewKeys_existInSchema() {
        DolphinLibretroAdapter a;
        QSet<QString> keys;
        for (const auto& d : a.settingsSchema()) keys.insert(d.key);
        const auto osd = a.previewSpec("Graphics", "On-Screen Display");
        for (auto it = osd.keyToProperty.constBegin(); it != osd.keyToProperty.constEnd(); ++it)
            QVERIFY2(keys.contains(it.key()),
                qPrintable(QString("previewSpec key '%1' is not a declared schema row").arg(it.key())));
    }
};

QTEST_GUILESS_MAIN(TestDolphinLibretroSchema)
#include "test_dolphin_libretro_schema.moc"
```

- [ ] **Step 2: Register the target** in `cpp/CMakeLists.txt` — copy the `test_ppsspp_libretro_schema` `add_executable(... )` block (the ~30-line block around line 790), then mechanically substitute: target name `test_ppsspp_libretro_schema` → `test_dolphin_libretro_schema`; test source `tests/test_ppsspp_libretro_schema.cpp` → `tests/test_dolphin_libretro_schema.cpp`; adapter source `src/adapters/libretro/ppsspp_libretro_adapter.cpp` → `src/adapters/libretro/dolphin_libretro_adapter.cpp`; and the `add_test(NAME PpssppLibretroSchema ...)` → `add_test(NAME DolphinLibretroSchema COMMAND test_dolphin_libretro_schema)`. Keep all other sources/includes/link-libs identical (the Dolphin adapter pulls in the same `libretro_adapter`/`core_loader`/`environment_callbacks`/etc. dependency set as the PPSSPP one).

- [ ] **Step 3: Build + run the test — expect PASS**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake -B cpp/build-x86_64 -S cpp >/dev/null 2>&1     # pick up the new target
cmake --build cpp/build-x86_64 --target test_dolphin_libretro_schema 2>&1 | tail -15
cd cpp/build-x86_64 && ctest -R DolphinLibretroSchema --output-on-failure
```
Expected: `100% tests passed`. A failure on `everyDefault_isInOptions` or `allFiveSubTabs_present` points to a typo'd default or a missing/misspelled subcategory in Task 8 — fix and re-run.

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/tests/test_dolphin_libretro_schema.cpp cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
SP6 Task 10: host schema shape test

test_dolphin_libretro_schema (QtTest, in-process) asserts every row is a
dolphin_-prefixed LibretroOption Combo, every default is in its options,
all five Graphics sub-tabs are present, no duplicate keys, hub-card
categoryKeys resolve, and previewSpec keys exist in the schema.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: Build universal dylib + deploy

**Files:** None — build/deploy orchestration (per memory `dolphin-libretro-build-setup.md`).

- [ ] **Step 1: Build arm64 core** — `cmake --build build-libretro --target dolphin_libretro 2>&1 | tail -8` → built.

- [ ] **Step 2: Build x86_64 core**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
PATH=/usr/local/bin:$PATH arch -x86_64 /usr/local/bin/cmake --build build-libretro-x86_64 --target dolphin_libretro 2>&1 | tail -8
```
Expected: built. (If `build-libretro-x86_64` is absent, configure it per the memory's x86_64 block first.)

- [ ] **Step 3: lipo into the installed universal dylib + verify**

```bash
lipo -create \
  /Users/mark/Documents/Projects/dolphin-libretro/build-libretro/Source/Core/DolphinLibretro/dolphin_libretro.dylib \
  /Users/mark/Documents/Projects/dolphin-libretro/build-libretro-x86_64/Source/Core/DolphinLibretro/dolphin_libretro.dylib \
  -output /Users/mark/Documents/RetroNest/emulators/libretro/cores/dolphin_libretro.dylib
lipo -archs /Users/mark/Documents/RetroNest/emulators/libretro/cores/dolphin_libretro.dylib
```
Expected: `x86_64 arm64`. (No `Data/Sys` change this SP, so the Sys-dir copy can be skipped unless the .app was rebuilt fresh.)

- [ ] **Step 4: Rebuild host + macdeployqt + codesign** (only needed because Task 8/10 changed RetroNest)

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-x86_64 --target RetroNest 2>&1 | tail -8
/usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
```
Expected: clean build + signed app (prevents the double-Qt abort per the memory).

- [ ] **Step 5: No commit** (build artefacts only).

---

## Task 12: Manual smoke (GC + Wii)

**Files:** None — user-driven verification. Report PASS/FAIL per step; do not proceed to Task 13 on any FAIL.

- [ ] **Step 1: Launch RetroNest with the core's stderr captured**

```bash
pkill -x RetroNest; sleep 1
RETRONEST_DOLPHIN_LOG=1 arch -x86_64 \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
  > /tmp/sp6-smoke.log 2>&1 &
```

- [ ] **Step 2: Open Dolphin settings → verify the Graphics card + five sub-tabs render.** The settings hub shows a single **Graphics** card; opening it shows sub-tabs **General / Enhancements / Hacks / Advanced / On-Screen Display**, each populated. Tooltips render on hover. Internal Resolution defaults to "Native (1x)", Aspect Ratio to "Auto".

- [ ] **Step 3: GameCube smoke — change Internal Resolution and confirm it applies.** Launch a GC game (e.g. Wind Waker). With defaults, confirm it boots and renders. Quit. Set **Internal Resolution → 2x**, set **Show FPS → Enabled**. Relaunch the same game. Confirm: the FPS counter now shows on-screen, and the image is visibly sharper (2x). Check `/tmp/sp6-smoke.log` for no `[CoreOptions]` errors.

- [ ] **Step 4: Confirm persistence in `options.json`.**

```bash
cat /Users/mark/Documents/RetroNest/emulators/libretro/dolphin/options.json
```
Expected: contains `"dolphin_internal_resolution": "2x"` and `"dolphin_show_fps": "enabled"` (plus any other toggled keys). Confirms the host persisted the user's choices to the path the base `LibretroAdapter` provides.

- [ ] **Step 5: Wii smoke — confirm options apply on Wii too.** Launch a Wii game (e.g. Mario Galaxy). Set **Aspect Ratio → Force 16:9** and **Anti-Aliasing → 4x MSAA**. Relaunch. Confirm the aspect change is visible and the game still renders correctly (AA is subtle; absence of crashes/glitches suffices). Reset both back to defaults afterward.

- [ ] **Step 6: Regression — settings don't break unrelated behavior.** With all options at defaults, confirm input (DualSense), audio, and save/load-state still work on one GC and one Wii title (SP5 / SP3 regression). Report findings.

- [ ] **Step 7: No commit** (manual verification).

---

## Task 13: Update auto-memory

**Files:** Auto-memory in `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-dolphin-libretro/memory/`.

- [ ] **Step 1: Update `sp6-dolphin-graphics-settings-prep.md`** — change the title/status to SP6 SHIPPED; record: the new core module paths (`CoreOptions*.{h,cpp}`), the host overrides location, the 53-option curated Graphics set, the schema-fidelity target + how to run it, the two test commands, and the commit-hash ranges (`git -C dolphin-libretro log --oneline` for the core; `git -C RetroNest-Project log --oneline` for host). Note that SP7 appends Audio + Core/Advanced modules with no infra refactor.

- [ ] **Step 2: Update `dolphin-libretro-build-setup.md` "Status" section** — add SP6 done; note the `check_schema_fidelity` target and `test_dolphin_libretro_schema` as part of the build/verify loop now.

- [ ] **Step 3: No git commit** (auto-memory lives outside the repos).

---

## Self-review (run after writing)

**Spec coverage:**
- Core-options infra (emit/read/apply) → Tasks 1–2. ✓
- Graphics ~50 curated, 5 sub-tabs → Tasks 3–7 (53 options; keep/drop matches the spec's curation). ✓
- `Config::SetCurrent` apply into CurrentRun before boot → Task 2 Step 3 (placement proven in Key-facts). ✓
- AA + texture-filtering fan-out core-side → Task 4 (in `Parse`, tested). ✓
- Host `settingsSchema`/`settingsHubCards`/`settingsCategoriesWithSubTabs`/`previewSpec` → Task 8. ✓
- Persistence free from base class → noted Task 12 Step 4 (no code). ✓
- Renderer omitted / Recommended deferred → not implemented anywhere (correct). ✓
- Build-time schema fidelity (Python + CMake target, cross-repo path) → Task 9. ✓
- Host shape test (mirror PPSSPP, in-process) → Task 10. ✓
- Core unit test (standalone Parse + BuildDefinitions, `CORE_OPTIONS_TEST_ONLY`) → Tasks 3–7. ✓
- Manual smoke GC + Wii, change setting + relaunch + options.json → Task 12. ✓

**Placeholder scan:** Option content uses tables + one full worked example per sub-tab; every option's key/values/default/Config-symbol/mapping is fully specified in the tables (no "TBD"/"etc." standing in for unspecified behavior — the "..." in code blocks always points back to a complete table). Config symbols, enum integer values, and wire-point line numbers are all concrete (resolved in prep). ✓

**Type consistency:** `Values` field names are identical across the header (Task 1), `Parse` (Tasks 3–7), and `Apply` (Tasks 3–7). `Graphics::{AppendDefinitions,Parse,Apply}` signatures match the header. The `gopt` signature in Task 8 matches PCSX2's. Core value-pairs are `(value,label)`; host pairs are `(label,value)` — consistent with the fidelity script's `VALUE_PAIR_RE`/`HOST_PAIR_RE`. The 53-option count is asserted in Task 7 (core: `BuildDefinitions().size()==54`) and implied by Task 10 (host sub-tab coverage). ✓
