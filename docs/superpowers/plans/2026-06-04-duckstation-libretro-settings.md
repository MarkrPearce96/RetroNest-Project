# DuckStation libretro user-facing settings — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the DuckStation libretro core's settings user-configurable from RetroNest's settings UI via libretro core-options, mirroring the standalone DuckStation settings for every knob the libretro core actually honors, plus wiring ImGui so OSD overlays work.

**Architecture:** Three coordinated parts that must agree on every (key, value, default): (A) the DuckStation core declares core-options and reads them back into `GPUSettings`/`Settings`; (B) the RetroNest `DuckStationLibretroAdapter` exposes them as a `settingsSchema()` of `Storage::LibretroOption` rows; (C) a `check_schema_fidelity.py` (ported from pcsx2-libretro) plus a C++ QtTest enforce that A and B never drift. OSD overlays require initializing `ImGuiManager` in the libretro display bringup (a self-contained, deferrable sub-feature).

**Tech Stack:** C++17, libretro core-options v2 API, Qt6 (QtTest), Python 3 (fidelity tool), CMake/ctest. Build is x86_64-under-Rosetta (see CLAUDE.md "Current run mode").

**Spec:** `docs/superpowers/specs/2026-06-04-duckstation-libretro-settings-design.md`

**Key conventions discovered (read before starting):**
- **Core table shape** must match `pcsx2-libretro/.../CoreOptionsGraphics.cpp`: a function `AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)` that does `out.push_back({key, desc, nullptr, info, nullptr, nullptr, {{value,label},...,{nullptr,nullptr}}, default});`. `category_key` (6th field) is **nullptr** — grouping is host-side. The fidelity tool's `CORE_BLOCK_RE` depends on this exact shape.
- **Host adapter shape** must match `pcsx2_libretro_adapter.cpp`: `s.append(opt(category, group, key, label, default, {{label,value},...}, tooltip[, dependsOn]))` and `s.append(gopt(subcategory, group, key, label, default, {...}, tooltip[, dependsOn]))` (gopt hardcodes category="Graphics"). Host pairs are `{label, value}` — **opposite order** from core's `{value, label}`. The fidelity tool's `HOST_BLOCK_RE` depends on this.
- **Spelling source:** `cpp/src/adapters/duckstation_adapter.cpp` (standalone) has audited, round-trip-correct value strings. Transcribe value-sets/defaults/labels from the cited line ranges; the fidelity tool catches any transcription error.
- **Two repos.** Core lives at `/Users/mark/Documents/Projects/duckstation-libretro` (git master, **local-only — never push**). Host at `/Users/mark/Documents/Projects/RetroNest-Project`.

---

## File structure

**Core (`duckstation-libretro/src/duckstation-libretro/`):**
- Modify → `libretro_core_options.h` / new `libretro_core_options.cpp`: full option table via `AppendDefinitions`.
- Modify → `libretro.cpp`: `SET_CORE_OPTIONS_V2` in `retro_set_environment` (:163); call the read/apply pass in `retro_load_game` (~:248).
- Modify → `libretro_settings.cpp`: add `ApplyCoreOptions(SettingsInterface&, retro_environment_t)` invoked after the base writes in `ApplySettings`/`retro_load_game`.
- Modify → `libretro_host.cpp`: ImGui bringup + OSD draw on the present path (Phase 6 only).
- Create → `tools/check_schema_fidelity.py`: ported from pcsx2.

**Host (`RetroNest-Project/cpp/`):**
- Modify → `src/adapters/libretro/duckstation_libretro_adapter.h`: declare the 4 overrides.
- Modify → `src/adapters/libretro/duckstation_libretro_adapter.cpp`: `settingsSchema()`, `settingsCategoriesWithSubTabs()`, `settingsHubCards()`, `previewSpec()`.
- Create → `tests/test_duckstation_libretro_schema.cpp`: structure + first-run-default QtTest.
- Modify → `CMakeLists.txt` (near :648): register the new test exe + ensure `duckstation_libretro_adapter.cpp` is in its source list (fixes the vtable link gap noted in CLAUDE.md).

---

## Phase 0 — Core option-table mechanics (worked end-to-end on the Console category)

This phase stands up the full mechanism with ONE category, so the pattern is proven before bulk transcription.

### Task 0.1: Convert the stub table to the `AppendDefinitions` push_back shape

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro_core_options.h`
- Create: `duckstation-libretro/src/duckstation-libretro/libretro_core_options.cpp`

- [ ] **Step 1: Replace the header's static-array function with declarations**

In `libretro_core_options.h`, replace the `DuckStationCoreOptionDefinitions()` static-array body with:

```cpp
#pragma once
#include "libretro.h"
#include <vector>

// Builds the full DuckStation core-option table (pcsx2/dolphin CoreOptions shape:
// out.push_back({...}) with category_key=nullptr; host groups them). The vector
// must outlive retro_deinit — retro_set_environment keeps a static copy.
void DuckStationAppendCoreOptions(std::vector<retro_core_option_v2_definition>& out);

// Returns a NULL-terminated definitions array (static storage) for SET_CORE_OPTIONS_V2.
const retro_core_option_v2_definition* DuckStationCoreOptionDefinitions();
```

- [ ] **Step 2: Implement in the new .cpp with the Console category only (worked example)**

Create `libretro_core_options.cpp`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
#include "libretro_core_options.h"

void DuckStationAppendCoreOptions(std::vector<retro_core_option_v2_definition>& out)
{
  // ── Console ────────────────────────────────────────────────────────────
  // Values transcribed from standalone duckstation_adapter.cpp:331-353
  // (host pairs are {label,value}; core pairs are {value,label}).
  out.push_back({
    "duckstation_console_region", "Region", nullptr,
    "Console region. Auto detects from the disc.", nullptr, nullptr,
    {{"Auto", "Auto-Detect"}, {"NTSC-J", "NTSC-J (Japan)"},
     {"NTSC-U", "NTSC-U/C (US, Canada)"}, {"PAL", "PAL (Europe, Australia)"},
     {nullptr, nullptr}},
    "Auto",
  });
  out.push_back({
    "duckstation_cpu_execution_mode", "CPU Execution Mode", nullptr,
    "How the PSX CPU is emulated. Recompiler is fastest.", nullptr, nullptr,
    {{"Interpreter", "Interpreter (Slowest)"},
     {"CachedInterpreter", "Cached Interpreter (Faster)"},
     {"Recompiler", "Recompiler (Fastest)"}, {nullptr, nullptr}},
    "Recompiler",
  });
  out.push_back({
    "duckstation_bios_fast_boot", "Fast Boot", nullptr,
    "Skips the BIOS boot animation and Sony intro.", nullptr, nullptr,
    {{"true", "Enabled"}, {"false", "Disabled"}, {nullptr, nullptr}},
    "false",
  });
}

const retro_core_option_v2_definition* DuckStationCoreOptionDefinitions()
{
  static std::vector<retro_core_option_v2_definition> s_defs = [] {
    std::vector<retro_core_option_v2_definition> v;
    DuckStationAppendCoreOptions(v);
    v.push_back({nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 {{nullptr, nullptr}}, nullptr});  // libretro terminator
    return v;
  }();
  return s_defs.data();
}
```

- [ ] **Step 3: Add the new .cpp to the core's build**

Add `libretro_core_options.cpp` to the DuckStation libretro source list (find where `libretro_settings.cpp` is listed — likely `src/duckstation-libretro/CMakeLists.txt`; add the file next to it).

Run (from `duckstation-libretro`): `grep -rn "libretro_settings.cpp" src/duckstation-libretro/CMakeLists.txt`
Expected: a source-list line; add `libretro_core_options.cpp` beside it.

- [ ] **Step 4: Build the core**

```bash
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS" && src/duckstation-libretro/package.sh
```
Expected: builds and deploys with no errors.

- [ ] **Step 5: Commit**

```bash
cd "$DS"
git add src/duckstation-libretro/libretro_core_options.h src/duckstation-libretro/libretro_core_options.cpp src/duckstation-libretro/CMakeLists.txt
git commit -m "feat(libretro): convert core-option table to AppendDefinitions shape (Console worked example)"
```

### Task 0.2: Wire SET_CORE_OPTIONS_V2 in retro_set_environment

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro.cpp:163`

- [ ] **Step 1: Include the header and register options**

At `retro_set_environment` (currently `g_environ = cb;` at :163), add after that line:

```cpp
  // Declare core-options so the frontend (RetroNest) can present them and
  // answer GET_VARIABLE. Categorized v2 table; category_key is nullptr —
  // host groups. Must be declared here (frontend reads it before load).
  unsigned version = 0;
  if (cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && version >= 2) {
    static retro_core_options_v2 opts = {DuckStationCoreOptionDefinitions(), nullptr};
    cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts);
  }
```

Add `#include "libretro_core_options.h"` to the includes if not present.

- [ ] **Step 2: Build the core**

Run: `cd "$DS" && src/duckstation-libretro/package.sh`
Expected: builds clean.

- [ ] **Step 3: Verify the frontend captures the options (host-side smoke check)**

The host captures declared options into `m_envCtx.declaredOptions` (`core_runtime.cpp`). Defer functional verification to Phase 3 integration. For now confirm no build break.

- [ ] **Step 4: Commit**

```bash
cd "$DS"
git add src/duckstation-libretro/libretro.cpp
git commit -m "feat(libretro): register core-options via SET_CORE_OPTIONS_V2"
```

### Task 0.3: Add the read/apply pass (Console keys), overriding ApplySettings defaults

**Files:**
- Modify: `duckstation-libretro/src/duckstation-libretro/libretro_settings.cpp`

- [ ] **Step 1: Add an ApplyCoreOptions function**

In `libretro_settings.cpp`, add (near `ApplySettings`):

```cpp
// Reads user core-option choices via GET_VARIABLE and writes them into the
// SettingsInterface, OVERRIDING the hardcoded #1-profile defaults written
// earlier in ApplySettings. Precedence: user option > #1 default.
// Pattern mirrors pcsx2/dolphin CoreOptions Parse(): a query() lambda.
static void ApplyCoreOptions(SettingsInterface* si, retro_environment_t environ_cb)
{
  const auto query = [&](const char* key) -> const char* {
    retro_variable var{};
    var.key = key;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && var.value[0])
      return var.value;
    return nullptr;
  };
  const auto queryBool = [&](const char* key, bool fallback) -> bool {
    if (const char* v = query(key)) return std::string_view(v) == "true";
    return fallback;
  };

  // Console (worked example — full set filled in Phase 1/2).
  if (const char* v = query("duckstation_console_region"))
    si->SetStringValue("Console", "Region", v);
  if (const char* v = query("duckstation_cpu_execution_mode"))
    si->SetStringValue("CPU", "ExecutionMode", v);
  si->SetBoolValue("BIOS", "PatchFastBoot",
                   queryBool("duckstation_bios_fast_boot", false));
  // NOTE: GPU/UseThread is intentionally NOT read — stays the forced false
  // from ApplySettings (the inline run loop requires it).
}
```

Ensure `<string_view>` is included.

- [ ] **Step 2: Call it after the base writes**

In `ApplySettings` (or right after it in `retro_load_game`), after the hardcoded `si->Set*Value(...)` block and before `EmuFolders::LoadConfig`, add:

```cpp
    ApplyCoreOptions(si, g_environ);
```

(Use whichever `retro_environment_t` handle is in scope — `g_environ` per `retro_set_environment`.)

- [ ] **Step 3: Build the core**

Run: `cd "$DS" && src/duckstation-libretro/package.sh`
Expected: builds clean.

- [ ] **Step 4: Commit**

```bash
cd "$DS"
git add src/duckstation-libretro/libretro_settings.cpp
git commit -m "feat(libretro): read core-options and apply over defaults (Console keys)"
```

---

## Phase 1 — Fill the core option table (all in-scope keys except OSD)

Bulk transcription. For each category, add `out.push_back({...})` entries to `DuckStationAppendCoreOptions`. **Value-sets, labels, and defaults are transcribed from the cited standalone-adapter line ranges**, converting host `{label,value}` order to core `{value,label}` order, and booleans to `{{"true","Enabled"},{"false","Disabled"}}`. Defaults for the enhancement keys come from the **feature-#1 profile**, not the standalone defaults (see callouts).

### Task 1.1: Console + CD-ROM remaining keys

**Files:** Modify `libretro_core_options.cpp`

- [ ] **Step 1: Add the remaining Console/CD-ROM entries**

Transcribe from `duckstation_adapter.cpp`:
- `ForceVideoTiming` (:338-340) → key `duckstation_console_video_timing`, values `{Disabled→"Auto-Detect", NTSC→"NTSC (60hz)", PAL→"PAL (50hz)"}`, default `Disabled`.
- `Enable8MBRAM` (:351) → `duckstation_console_8mb_ram`, bool, default `false`.
- `OverclockEnable` (:362) → `duckstation_cpu_overclock_enable`, bool, default `false`.
- **Overclock percent (combo-only conversion):** the standalone uses an Int slider 10–1000 step 5 (:370-392). Expose as a combo `duckstation_cpu_overclock_percent` with stops `{"50","75","100","150","200","300","400","500","750","1000"}` (label each `"N%"`), default `"100"`. The fraction math moves to the core apply step (Task 2.1).
- `RecompilerICache` (:394) → `duckstation_cpu_recompiler_icache`, bool, default `false`.
- CDROM: `ReadSpeedup` (:400, values from `cdromSpeedupOptions` :133-141) → `duckstation_cdrom_read_speedup`, default `"1"`; `SeekSpeedup` (:403, `cdromSeekOptions` :147-155) → `duckstation_cdrom_seek_speedup`, default `"1"`; `LoadImageToRAM` (:406) → `duckstation_cdrom_preload`, bool; `LoadImagePatches` (:409) → `duckstation_cdrom_image_patches`, bool; `AutoDiscChange` (:412) → `duckstation_cdrom_auto_disc_change`, bool; `IgnoreHostSubcode` (:415) → `duckstation_cdrom_ignore_subcode`, bool.

Each entry follows the Task 0.1 Step 2 pattern. Example for a combo:

```cpp
  out.push_back({
    "duckstation_cdrom_read_speedup", "CD-ROM Read Speedup", nullptr,
    "Speeds up CD-ROM reads beyond hardware limits.", nullptr, nullptr,
    {{"1", "None (Double Speed)"}, {"2", "2x (Quad Speed)"}, {"3", "3x (6x Speed)"},
     {"4", "4x (8x Speed)"}, {"5", "5x (10x Speed)"}, {"6", "6x (12x Speed)"},
     {"0", "Maximum (Safer)"}, {nullptr, nullptr}},
    "1",
  });
```

- [ ] **Step 2: Build the core**

Run: `cd "$DS" && src/duckstation-libretro/package.sh`
Expected: builds clean.

- [ ] **Step 3: Commit**

```bash
cd "$DS" && git add src/duckstation-libretro/libretro_core_options.cpp
git commit -m "feat(libretro): add Console + CD-ROM core-options"
```

### Task 1.2: Memory Cards + BIOS FastForwardBoot

**Files:** Modify `libretro_core_options.cpp`

- [ ] **Step 1: Add entries**

- `Card1Type` (:503, values `memCardTypes` :102-109) → `duckstation_memcard_1_type`, default `PerGameTitle`.
- `Card2Type` (:509, same values) → `duckstation_memcard_2_type`, default `PerGameTitle` (confirm standalone default at :509; if it differs, match it).
- `FastForwardBoot` (:345) → `duckstation_bios_fast_forward_boot`, bool, default `false`.

`memCardTypes` core pairs: `{{"None","No Memory Card"},{"Shared","Shared Between All Games"},{"PerGame","Separate Card Per Game (Serial)"},{"PerGameTitle","Separate Card Per Game (Title)"},{"PerGameFileTitle","Separate Card Per Game (File Title)"},{"NonPersistent","Non-Persistent Card (Do Not Save)"}}`.

- [ ] **Step 2: Build + commit**

```bash
cd "$DS" && src/duckstation-libretro/package.sh
git add src/duckstation-libretro/libretro_core_options.cpp
git commit -m "feat(libretro): add Memory Card type + Fast Forward Boot core-options"
```

### Task 1.3: Graphics › Rendering

**Files:** Modify `libretro_core_options.cpp`

- [ ] **Step 1: Add Rendering entries (#1-profile defaults where noted)**

Transcribe from `duckstation_adapter.cpp:538-631`:
- `Renderer` → `duckstation_gpu_renderer`. **Limited values** (NOT the standalone's 5): `{{"Auto","Auto"},{"Metal","Metal"},{"Software","Software"}}`. **Default `"Auto"`** (resolves to Metal). *Do not expose Vulkan/OpenGL.*
- `ResolutionScale` (:544) → `duckstation_gpu_resolution_scale`, values `1..8` (`{"1","1x Native"}…{"8","8x Native (4K)"}`). **Default `"4"`** (#1 profile).
- `DownsampleMode` (:552, names `Disabled/Box/Adaptive`) → `duckstation_gpu_downsample_mode`, default `Disabled`.
- `TextureFilter` (:565, `textureFilterOptions` :157-169) → `duckstation_gpu_texture_filter`, default `Nearest`.
- `SpriteTextureFilter` (:567, same values) → `duckstation_gpu_sprite_texture_filter`, default `Nearest`.
- `DitheringMode` (:572, names `Unscaled/UnscaledShaderBlend/Scaled/ScaledShaderBlend/TrueColor/TrueColorFull`) → `duckstation_gpu_dithering`, **default `TrueColor`** (#1 profile).
- `DeinterlacingMode` (:581) → `duckstation_gpu_deinterlacing` (transcribe values from that block).
- `AspectRatio` (:591) → `duckstation_display_aspect_ratio`, default `"Auto (Game Native)"`. *(verify-applies — spec §6.)*
- `CropMode` (:599) → `duckstation_display_crop`. *(verify-applies.)*
- `Scaling` (:607, `scalingOptions` :175-183) → `duckstation_display_scaling`. *(verify-applies.)*
- `Scaling24Bit` (:610) → `duckstation_display_scaling_24bit`. *(verify-applies.)*
- `PGXPEnable` (:615) → `duckstation_gpu_pgxp_enable`, bool, **default `true`** (#1).
- `WidescreenHack` (:628) → `duckstation_gpu_widescreen_hack`, bool, default `false`.
- `Force4_3For24Bit` (:622) → `duckstation_display_force_4_3_for_24bit`, bool.
- `ChromaSmoothing24Bit` (:625) → `duckstation_gpu_chroma_smoothing_24bit`, bool.
- `ForceRoundTextureCoordinates` (:631) → `duckstation_gpu_force_round_texcoords`, bool.

PGXP sub-knobs not in the standalone Rendering pane but in the #1 profile — add and default to the #1 values:
- `PGXPCulling` → `duckstation_gpu_pgxp_culling`, bool, **default `true`**.
- `PGXPTextureCorrection` → `duckstation_gpu_pgxp_texture_correction`, bool, **default `true`**.

- [ ] **Step 2: Build + commit**

```bash
cd "$DS" && src/duckstation-libretro/package.sh
git add src/duckstation-libretro/libretro_core_options.cpp
git commit -m "feat(libretro): add Graphics Rendering core-options (#1 defaults preserved)"
```

### Task 1.4: Graphics › Advanced + Texture Replacement

**Files:** Modify `libretro_core_options.cpp`

- [ ] **Step 1: Add entries**

Advanced (`:645-708`): `Alignment` (:645), `Rotation` (:649), `FineCropMode` (:654), `DisableMailboxPresentation` (:673, bool), `Multisamples` (:684, values `{"1","Disabled"},{"2","2x MSAA"},{"4","4x MSAA"},{"8","8x MSAA"},{"16","16x MSAA"}`), `LineDetectMode` (:688), `EnableModulationCrop` (:700, bool), `ScaledInterlacing` (:704, bool), `UseSoftwareRendererForReadbacks` (:708, bool). Keys: `duckstation_display_alignment`, `_display_rotation`, `_display_fine_crop`, `_display_disable_mailbox`, `duckstation_gpu_multisamples`, `_gpu_line_detect`, `_gpu_modulation_crop`, `_gpu_scaled_interlacing`, `_gpu_sw_readbacks`. *Display-pane ones are verify-applies.*

Texture Replacement (`:732-774`): `EnableTextureCache` (:732, bool) → `duckstation_gpu_texture_cache`; the VRAM-write-replacement toggles (:767-774) → `duckstation_texrepl_*` bools. Transcribe sections/keys exactly from those lines.

- [ ] **Step 2: Build + commit**

```bash
cd "$DS" && src/duckstation-libretro/package.sh
git add src/duckstation-libretro/libretro_core_options.cpp
git commit -m "feat(libretro): add Graphics Advanced + Texture Replacement core-options"
```

---

## Phase 2 — Core read/apply for all keys

Extend `ApplyCoreOptions` (Task 0.3) to read every key added in Phase 1 and write it to the correct INI section/key. Section/key per setting is the 4th/5th positional field in the standalone `s.append({category, sub, group, SECTION, KEY, ...})` rows — use those exact section/key strings.

### Task 2.1: Apply Console/CPU/CD-ROM (incl. overclock fraction)

**Files:** Modify `libretro_settings.cpp`

- [ ] **Step 1: Add the reads**

Append to `ApplyCoreOptions`:

```cpp
  if (const char* v = query("duckstation_gpu_force_video_timing"))
    si->SetStringValue("GPU", "ForceVideoTiming", v);
  si->SetBoolValue("Console", "Enable8MBRAM", queryBool("duckstation_console_8mb_ram", false));
  si->SetBoolValue("CPU", "OverclockEnable", queryBool("duckstation_cpu_overclock_enable", false));
  si->SetBoolValue("CPU", "RecompilerICache", queryBool("duckstation_cpu_recompiler_icache", false));
  if (const char* v = query("duckstation_cdrom_read_speedup")) si->SetStringValue("CDROM", "ReadSpeedup", v);
  if (const char* v = query("duckstation_cdrom_seek_speedup")) si->SetStringValue("CDROM", "SeekSpeedup", v);
  si->SetBoolValue("CDROM", "LoadImageToRAM",   queryBool("duckstation_cdrom_preload", false));
  si->SetBoolValue("CDROM", "LoadImagePatches", queryBool("duckstation_cdrom_image_patches", false));
  si->SetBoolValue("CDROM", "AutoDiscChange",   queryBool("duckstation_cdrom_auto_disc_change", false));
  si->SetBoolValue("CDROM", "IgnoreHostSubcode",queryBool("duckstation_cdrom_ignore_subcode", false));
  si->SetBoolValue("BIOS", "FastForwardBoot",   queryBool("duckstation_bios_fast_forward_boot", false));
  if (const char* v = query("duckstation_memcard_1_type")) si->SetStringValue("MemoryCards", "Card1Type", v);
  if (const char* v = query("duckstation_memcard_2_type")) si->SetStringValue("MemoryCards", "Card2Type", v);

  // Overclock percent -> numerator/denominator (same gcd reduction as
  // Settings::CPUOverclockPercentToFraction, core/settings.cpp:230).
  if (const char* v = query("duckstation_cpu_overclock_percent")) {
    int percent = std::atoi(v);
    if (percent <= 0) percent = 100;
    auto gcd = [](int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; };
    const int g = gcd(percent, 100);
    si->SetIntValue("CPU", "OverclockNumerator", percent / g);
    si->SetIntValue("CPU", "OverclockDenominator", 100 / g);
  }
```

Ensure `<cstdlib>` is included.

- [ ] **Step 2: Build + commit**

```bash
cd "$DS" && src/duckstation-libretro/package.sh
git add src/duckstation-libretro/libretro_settings.cpp
git commit -m "feat(libretro): apply Console/CPU/CD-ROM/MemoryCard options (overclock fraction)"
```

### Task 2.2: Apply Graphics (Rendering + Advanced + Texture Replacement)

**Files:** Modify `libretro_settings.cpp`

- [ ] **Step 1: Add the reads**

Append the Graphics reads, e.g.:

```cpp
  // Renderer option value is already the canonical name ("Automatic"/"Metal"/
  // "Software", per ParseRendererName, settings.cpp:1557) — write through.
  if (const char* v = query("duckstation_gpu_renderer")) si->SetStringValue("GPU", "Renderer", v);
  if (const char* v = query("duckstation_gpu_resolution_scale"))
    si->SetUIntValue("GPU", "ResolutionScale", static_cast<u32>(std::atoi(v)));
  if (const char* v = query("duckstation_gpu_downsample_mode"))      si->SetStringValue("GPU", "DownsampleMode", v);
  if (const char* v = query("duckstation_gpu_texture_filter"))       si->SetStringValue("GPU", "TextureFilter", v);
  if (const char* v = query("duckstation_gpu_sprite_texture_filter"))si->SetStringValue("GPU", "SpriteTextureFilter", v);
  if (const char* v = query("duckstation_gpu_dithering"))            si->SetStringValue("GPU", "DitheringMode", v);
  if (const char* v = query("duckstation_gpu_deinterlacing"))        si->SetStringValue("GPU", "DeinterlacingMode", v);
  if (const char* v = query("duckstation_gpu_multisamples"))         si->SetUIntValue("GPU", "Multisamples", static_cast<u32>(std::atoi(v)));
  if (const char* v = query("duckstation_gpu_line_detect"))          si->SetStringValue("GPU", "LineDetectMode", v);
  si->SetBoolValue("GPU", "PGXPEnable",            queryBool("duckstation_gpu_pgxp_enable", true));
  si->SetBoolValue("GPU", "PGXPCulling",           queryBool("duckstation_gpu_pgxp_culling", true));
  si->SetBoolValue("GPU", "PGXPTextureCorrection", queryBool("duckstation_gpu_pgxp_texture_correction", true));
  si->SetBoolValue("GPU", "WidescreenHack",        queryBool("duckstation_gpu_widescreen_hack", false));
  si->SetBoolValue("GPU", "ChromaSmoothing24Bit",  queryBool("duckstation_gpu_chroma_smoothing_24bit", false));
  si->SetBoolValue("GPU", "ForceRoundTextureCoordinates", queryBool("duckstation_gpu_force_round_texcoords", false));
  si->SetBoolValue("GPU", "EnableModulationCrop",  queryBool("duckstation_gpu_modulation_crop", false));
  si->SetBoolValue("GPU", "ScaledInterlacing",     queryBool("duckstation_gpu_scaled_interlacing", false));
  si->SetBoolValue("GPU", "UseSoftwareRendererForReadbacks", queryBool("duckstation_gpu_sw_readbacks", false));
  si->SetBoolValue("GPU", "EnableTextureCache",    queryBool("duckstation_gpu_texture_cache", false));
  // Display-pane (verify-applies): AspectRatio/Crop/Scaling/Alignment/Rotation/FineCrop/Mailbox/Force4_3/Scaling24Bit
  if (const char* v = query("duckstation_display_aspect_ratio")) si->SetStringValue("Display", "AspectRatio", v);
  if (const char* v = query("duckstation_display_crop"))         si->SetStringValue("Display", "CropMode", v);
  if (const char* v = query("duckstation_display_scaling"))      si->SetStringValue("Display", "Scaling", v);
  // ...remaining display + texrepl keys, each to its standalone section/key.
```

Use the exact section/key from the standalone rows for every remaining key (alignment→`Display/Alignment`, rotation→`Display/Rotation`, fine crop→`Display/FineCropMode`, mailbox→`Display/DisableMailboxPresentation`, force 4:3→`Display/Force4_3For24Bit`, scaling 24bit→`Display/Scaling24Bit`, downsample scale if exposed→`GPU/DownsampleScale`, texrepl→`TextureReplacements/*` or `Hacks/*` per :767-774).

- [ ] **Step 2: Build + commit**

```bash
cd "$DS" && src/duckstation-libretro/package.sh
git add src/duckstation-libretro/libretro_settings.cpp
git commit -m "feat(libretro): apply Graphics Rendering/Advanced/TextureReplacement options"
```

---

## Phase 3 — RetroNest host adapter schema

### Task 3.1: Declare the overrides

**Files:** Modify `RetroNest-Project/cpp/src/adapters/libretro/duckstation_libretro_adapter.h`

- [ ] **Step 1: Add declarations**

Inside the class (after `findResumeFile`):

```cpp
    QVector<SettingDef> settingsSchema() const override;
    QStringList settingsCategoriesWithSubTabs() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    PreviewSpec previewSpec(const QString& category, const QString& subcategory) const override;
```

Match exact return types/signatures to the base class in `libretro_adapter.h` / `emulator_adapter.h` (verify names — e.g. `SettingsHubCard`, `PreviewSpec`).

- [ ] **Step 2: Build the host (will fail to link until 3.2 — that's expected)**

Skip building until 3.2; commit together.

### Task 3.2: Implement settingsSchema with opt()/gopt() helpers

**Files:** Modify `RetroNest-Project/cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp`

- [ ] **Step 1: Add the helpers + a worked category, mirroring pcsx2_libretro_adapter.cpp**

```cpp
QVector<SettingDef> DuckStationLibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    // opt(): flat category. gopt(): Graphics sub-tab (category hardcoded "Graphics").
    auto opt = [](const QString& category, const QString& group, const QString& key,
                  const QString& label, const QString& def,
                  const QVector<QPair<QString,QString>>& values,
                  const QString& tooltip, const QString& dependsOn = {}) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.category = category; d.group = group; d.key = key; d.label = label;
        d.defaultValue = def; d.tooltip = tooltip;
        d.type = values.isEmpty() ? SettingDef::Bool : SettingDef::Combo;
        d.options = values; d.dependsOn = dependsOn;
        return d;
    };
    auto gopt = [&](const QString& subcategory, const QString& group, const QString& key,
                    const QString& label, const QString& def,
                    const QVector<QPair<QString,QString>>& values,
                    const QString& tooltip, const QString& dependsOn = {}) -> SettingDef {
        SettingDef d = opt("Graphics", group, key, label, def, values, tooltip, dependsOn);
        d.subcategory = subcategory;
        return d;
    };

    // Bool helper: 2-value combo matching the core's {"true"/"false"} options.
    const QVector<QPair<QString,QString>> boolVals = {{"Enabled","true"},{"Disabled","false"}};

    // ── Console ── (keys MUST match the core table from Phase 0/1)
    s.append(opt("Console", "Console", "duckstation_console_region", "Region", "Auto",
        {{"Auto-Detect","Auto"},{"NTSC-J (Japan)","NTSC-J"},
         {"NTSC-U/C (US, Canada)","NTSC-U"},{"PAL (Europe, Australia)","PAL"}},
        "Console region. Auto detects from the disc."));
    s.append(opt("Console", "CPU Emulation", "duckstation_cpu_execution_mode", "Execution Mode", "Recompiler",
        {{"Interpreter (Slowest)","Interpreter"},{"Cached Interpreter (Faster)","CachedInterpreter"},
         {"Recompiler (Fastest)","Recompiler"}},
        "How the PSX CPU is emulated. Recompiler is fastest."));
    s.append(opt("Console", "Console", "duckstation_bios_fast_boot", "Fast Boot", "false",
        boolVals, "Skips the BIOS boot animation and Sony intro."));

    // ... all remaining categories, one s.append(opt/gopt(...)) per core key,
    //     {label,value} pairs (note: opposite order from core's {value,label}).
    //     Graphics rows use gopt() with subcategory "Rendering"/"Advanced"/
    //     "Texture Replacement"/"On-Screen Display". Defaults MUST equal the
    //     core table defaults (incl. #1-profile values: scale "4", PGXP "true",
    //     dithering "TrueColor", renderer "Auto").

    return s;
}

QStringList DuckStationLibretroAdapter::settingsCategoriesWithSubTabs() const {
    return {"Graphics"};
}
```

- [ ] **Step 2: Fill all remaining rows**

For every core key from Phase 1, add a matching `opt()`/`gopt()` row. Source labels/tooltips/values from the same standalone-adapter line ranges (Tasks 1.1–1.4). Add `dependsOn` to gated rows: overclock percent → `"duckstation_cpu_overclock_enable"`; downsample scale (if exposed) → `"duckstation_gpu_downsample_mode!=Disabled"`; recompiler icache → `"duckstation_cpu_execution_mode=Recompiler"`. (Match the `dependsOn` DSL in `setting_def.h`.)

- [ ] **Step 3: Add the Recommended curated view (duplicate-key rows)**

Mirror the standalone `duckstation_adapter.cpp:212-315` Recommended catalog, restricted to in-scope keys. These are **duplicate rows** with `category = "Recommended"` pointing at the SAME core-option keys (e.g. another `opt("Recommended", "Performance", "duckstation_cpu_execution_mode", ...)`). Same key + value-set + default as the primary row — editing in either place writes the same option. The fidelity tool dedupes by key, so duplicates are fine; the C++ test must not assume keys are unique across categories. Curated subset (drop the host-owned ones the standalone lists — VSync/Backend/Volume/EmulationSpeed): Renderer, ExecutionMode, ReadSpeedup, ResolutionScale, AspectRatio, WidescreenHack, PGXPEnable, Multisamples, TextureFilter, Region, Fast Boot.

- [ ] **Step 4: Implement settingsHubCards + previewSpec**

Mirror `dolphin_libretro_adapter.cpp`'s implementations: a Recommended full-width card + one card per category (Console / Graphics / BIOS / Memory Cards); `previewSpec` returns `"aspect"` for Recommended and `"osd"` for `Graphics`/`On-Screen Display`.

- [ ] **Step 5: Build the host**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6
```
Expected: links clean.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/libretro/duckstation_libretro_adapter.h cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp
git commit -m "feat(host): DuckStation libretro settingsSchema + hub cards + preview"
```

---

## Phase 4 — Fidelity tool (the automated drift gate)

### Task 4.1: Port check_schema_fidelity.py to the DuckStation core

**Files:** Create `duckstation-libretro/src/duckstation-libretro/tools/check_schema_fidelity.py`

- [ ] **Step 1: Copy the pcsx2 tool and re-point its docstring**

```bash
mkdir -p /Users/mark/Documents/Projects/duckstation-libretro/src/duckstation-libretro/tools
cp /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py \
   /Users/mark/Documents/Projects/duckstation-libretro/src/duckstation-libretro/tools/check_schema_fidelity.py
```

Edit the docstring to name DuckStation. The `CORE_BLOCK_RE` / `HOST_BLOCK_RE` are reusable as-is **because** Phase 0–3 deliberately used the pcsx2 callsite shapes (`out.push_back({... nullptr category_key ...})` and `opt()/gopt()`). No regex change should be needed.

- [ ] **Step 2: Run it against the two new files**

```bash
python3 /Users/mark/Documents/Projects/duckstation-libretro/src/duckstation-libretro/tools/check_schema_fidelity.py \
  --core "/Users/mark/Documents/Projects/duckstation-libretro/src/duckstation-libretro/libretro_core_options.cpp" \
  --host "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp"
```
Expected: `exit 0` (full match). If it reports drift, **fix the mismatched key/value/default** in whichever file is wrong (this is the tool doing its job — catching transcription errors from Phases 1–3) and re-run until exit 0.

- [ ] **Step 3: Commit (core repo)**

```bash
cd /Users/mark/Documents/Projects/duckstation-libretro
git add src/duckstation-libretro/tools/check_schema_fidelity.py
git commit -m "test(libretro): schema-fidelity check for DuckStation core-options vs host schema"
```

---

## Phase 5 — C++ schema QtTest

### Task 5.1: Add test_duckstation_libretro_schema.cpp

**Files:**
- Create: `RetroNest-Project/cpp/tests/test_duckstation_libretro_schema.cpp`
- Modify: `RetroNest-Project/cpp/CMakeLists.txt` (near :648)

- [ ] **Step 1: Write the failing test (structure + first-run defaults)**

```cpp
#include <QtTest>
#include <QSet>
#include "adapters/libretro/duckstation_libretro_adapter.h"
#include "core/setting_def.h"

class TestDuckStationLibretroSchema : public QObject {
    Q_OBJECT
    QVector<SettingDef> schema_;
private slots:
    void initTestCase() {
        DuckStationLibretroAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }
    void testAllRowsAreLibretroOptions() {
        for (const auto& d : schema_)
            QCOMPARE(d.storage, SettingDef::Storage::LibretroOption);
    }
    void testGraphicsHasSubTabs() {
        DuckStationLibretroAdapter adapter;
        QVERIFY(adapter.settingsCategoriesWithSubTabs().contains("Graphics"));
        QSet<QString> subs;
        for (const auto& d : schema_) if (d.category == "Graphics") subs.insert(d.subcategory);
        QVERIFY(subs.contains("Rendering"));
        QVERIFY(subs.contains("On-Screen Display"));
    }
    void testFirstRunDefaultsMatchFeature1Profile() {
        // The good #1 profile must be the default with no user options set.
        auto def = [&](const QString& key) {
            for (const auto& d : schema_) if (d.key == key) return d.defaultValue;
            return QString("<missing>");
        };
        QCOMPARE(def("duckstation_gpu_resolution_scale"), QString("4"));
        QCOMPARE(def("duckstation_gpu_pgxp_enable"), QString("true"));
        QCOMPARE(def("duckstation_gpu_dithering"), QString("TrueColor"));
        QCOMPARE(def("duckstation_gpu_renderer"), QString("Automatic"));
    }
    void testRendererExcludesUnwiredBackends() {
        for (const auto& d : schema_) if (d.key == "duckstation_gpu_renderer") {
            QSet<QString> vals; for (const auto& p : d.options) vals.insert(p.second);
            QCOMPARE(vals, QSet<QString>({"Automatic","Metal","Software"}));
        }
    }
    void testNoUseThreadOption() {
        for (const auto& d : schema_)
            QVERIFY2(!d.key.contains("use_thread"), "UseThread must not be user-exposed");
    }
};
QTEST_MAIN(TestDuckStationLibretroSchema)
#include "test_duckstation_libretro_schema.moc"
```

- [ ] **Step 2: Register in CMake**

After the `test_duckstation_schema` block (`cpp/CMakeLists.txt:648-660`), add:

```cmake
add_executable(test_duckstation_libretro_schema
    tests/test_duckstation_libretro_schema.cpp
    src/adapters/libretro/duckstation_libretro_adapter.cpp)   # include the adapter .cpp so the vtable links
set_target_properties(test_duckstation_libretro_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_duckstation_libretro_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_duckstation_libretro_schema PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME DuckStationLibretroSchema COMMAND test_duckstation_libretro_schema)
```

If the adapter .cpp needs other libretro adapter sources to link, add them too (mirror how `test_duckstation_controller_schema` is wired). This addresses the CLAUDE.md note that libretro adapter test targets miss the adapter from their source lists.

- [ ] **Step 3: Build + run the test**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_duckstation_libretro_schema -j 6
arch -x86_64 ./cpp/build-x86_64/test_duckstation_libretro_schema
```
Expected: all PASS. (If `testFirstRunDefaults...` fails, the core/host defaults are wrong — fix to the #1 profile.)

- [ ] **Step 4: Commit**

```bash
git add cpp/tests/test_duckstation_libretro_schema.cpp cpp/CMakeLists.txt
git commit -m "test(host): DuckStation libretro schema structure + first-run defaults"
```

---

## Phase 6 — OSD overlays (core ImGui bringup) — splittable

> **Risk gate:** This is the first time the libretro core renders ImGui. If it destabilizes the NSView present path, stop and split it to a follow-on feature — Phases 0–5 ship the settings independently. The spec (§5) authorizes this split.

### Task 6.1: Initialize ImGuiManager in the libretro display bringup

**Files:** Modify `duckstation-libretro/src/duckstation-libretro/libretro_host.cpp`

- [ ] **Step 1: Initialize ImGui after the NSView swapchain is acquired**

In the display-bringup path (where `AcquireRenderWindow`/F1 attaches the NSView swapchain), call `ImGuiManager::Initialize(...)` with the display scale, matching how standalone DuckStation initializes it after GPU device creation. Guard so it only runs once and only when a render window exists. Add cleanup (`ImGuiManager::Shutdown()`) on unload, respecting the autorelease-pool-before-dlclose discipline from the resume fix.

(Exact call signature: read `src/util/imgui_manager.h` for the current `Initialize` signature and copy standalone's bringup from where DuckStation's Qt host calls it.)

- [ ] **Step 2: Build the core**

Run: `cd "$DS" && src/duckstation-libretro/package.sh`
Expected: builds clean.

- [ ] **Step 3: Commit**

```bash
cd "$DS" && git add src/duckstation-libretro/libretro_host.cpp
git commit -m "feat(libretro): initialize ImGuiManager for OSD rendering"
```

### Task 6.2: Draw OSD on the present path without double-presenting

**Files:** Modify `duckstation-libretro/src/duckstation-libretro/libretro_host.cpp`

- [ ] **Step 1: Ensure OSD draw-data composites into the existing present**

The engine presents via `VideoPresenter::PresentFrame()` from inside `HandleSubmitFrameCommand`/`HandleUpdateDisplayCommand` (see the `FrameDoneOnVideoThread` comment at :261-289). Ensure the ImGui/OSD render pass runs as part of that present (DuckStation's presenter draws ImGui draw-data when a context exists) — do **not** add a second `PresentFrame` call (warned against at :268-273). If the presenter needs `ImGuiManager::Render()` called explicitly before present in this path, add it there.

- [ ] **Step 2: Build + manual verify (user-run)**

Build core + host (Phase 7 recipe). User launches RetroNest, enables "Show FPS", relaunches a PS1 game, confirms the FPS overlay renders on the NSView and the game still presents normally (no flicker/desync).

- [ ] **Step 3: Commit**

```bash
cd "$DS" && git add src/duckstation-libretro/libretro_host.cpp
git commit -m "feat(libretro): composite OSD overlay onto NSView present path"
```

### Task 6.3: Add OSD overlay core-options + host rows

**Files:** Modify `libretro_core_options.cpp`, `libretro_settings.cpp`, `duckstation_libretro_adapter.cpp`

- [ ] **Step 1: Add the OSD overlay toggles (overlays only — no message styling)**

Core table + apply + host schema, for keys (standalone `:837-846`, all `Display/*` bools): `ShowFPS`, `ShowSpeed`, `ShowCPU`, `ShowGPU`, `ShowResolution`, `ShowGPUStatistics`, `ShowFrameTimes`, `ShowLatencyStatistics`, `ShowInputs`, `ShowEnhancements`. Keys `duckstation_osd_show_*`. Host rows use `gopt("On-Screen Display", "Overlays", ...)`. Apply writes `si->SetBoolValue("Display", "Show...", ...)`.

- [ ] **Step 2: Re-run fidelity + schema test + build**

```bash
python3 .../tools/check_schema_fidelity.py --core ".../libretro_core_options.cpp" --host ".../duckstation_libretro_adapter.cpp"
arch -x86_64 ./cpp/build-x86_64/test_duckstation_libretro_schema
cd "$DS" && src/duckstation-libretro/package.sh
```
Expected: fidelity exit 0; tests PASS; core builds.

- [ ] **Step 3: Commit (both repos)**

```bash
cd "$DS" && git add -A && git commit -m "feat(libretro): expose OSD overlay toggles as core-options"
cd /Users/mark/Documents/Projects/RetroNest-Project && git add -A && git commit -m "feat(host): OSD overlay rows in DuckStation libretro schema"
```

---

## Phase 7 — Integration build, deploy, and manual verification

### Task 7.1: Full build + deploy + verification recipe

**Files:** none (verification only)

- [ ] **Step 1: Build + deploy both sides**

```bash
export DS=/Users/mark/Documents/Projects/duckstation-libretro
export MACOSX_DEPLOYMENT_TARGET=13.3
cd "$DS" && src/duckstation-libretro/package.sh
cd /Users/mark/Documents/Projects/RetroNest-Project
arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
```
Expected: both build/deploy clean.

- [ ] **Step 2: Run automated gates**

```bash
python3 "$DS/src/duckstation-libretro/tools/check_schema_fidelity.py" \
  --core "$DS/src/duckstation-libretro/libretro_core_options.cpp" \
  --host "/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/duckstation_libretro_adapter.cpp"
cd /Users/mark/Documents/Projects/RetroNest-Project && arch -x86_64 ./cpp/build-x86_64/test_duckstation_libretro_schema
```
Expected: fidelity exit 0; all tests PASS.

- [ ] **Step 3: Manual GUI verification (USER runs — TCC blocks the agent)**

Ask the user to:
```bash
cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1
```
Then in the UI:
1. Open the DuckStation settings page — confirm Console / CD-ROM / Graphics (Rendering/Advanced/Texture Replacement/On-Screen Display) / BIOS / Memory Cards / Recommended all appear.
2. **First-run default check:** with a fresh `options.json`, launch a PS1 game and confirm `/tmp/rn.log` shows the #1 profile (`Resolution Scale: 4`, PGXP on, TrueColor).
3. Change Internal Resolution to 2×, relaunch the game, confirm `/tmp/rn.log` shows `Resolution Scale: 2`.
4. Toggle PGXP off / change Dithering / change CPU Execution Mode — relaunch, confirm each takes effect in the log.
5. **Display-pane verify (spec §6):** change Aspect Ratio / Crop / Scaling — confirm each *visibly* applies; if any does nothing (host overrides it), remove that row from both core table and host schema and re-run the fidelity gate.
6. **OSD verify:** enable Show FPS / Show GPU — confirm the overlay renders on the NSView and the game presents normally.

- [ ] **Step 4: Final commit (any cleanups from verification)**

```bash
cd "$DS" && git add -A && git commit -m "fix(libretro): drop host-overridden display settings found in verification" || true
cd /Users/mark/Documents/Projects/RetroNest-Project && git add -A && git commit -m "chore(host): DuckStation libretro settings verified end-to-end" || true
```

---

## Notes for the implementer

- **Never push** the `duckstation-libretro` repo (local-only master).
- **Build `--target RetroNest`**, not `all` (some x86 test targets have unrelated link issues per CLAUDE.md).
- The **fidelity tool is your friend**: run it after every table/schema edit; it catches transcription drift instantly (exit 1 + diff). Treat exit 0 as the definition of "core and host agree."
- **Defaults are load-bearing:** the option-table defaults ARE the first-run profile. Getting `duckstation_gpu_resolution_scale=4`, PGXP true, dithering TrueColor, renderer Auto wrong is a feature-#1 regression — `testFirstRunDefaultsMatchFeature1Profile` guards it.
- If Phase 6 (OSD) stalls, ship Phases 0–5 and spin OSD into its own plan; the settings plumbing is independent.
