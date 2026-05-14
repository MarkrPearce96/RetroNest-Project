# PCSX2-libretro SP7c Phase 4 Task 5 — Post-Processing sub-tab Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the 9 GS post-pass shader knobs (CAS, FXAA, TV Shader, ShadeBoost master + 4 sliders) into the libretro core option pipeline and the RetroNest host adapter, taking schema fidelity from 57/57 → 66/66 and `test_core_options` from 38 → 40 cases.

**Architecture:** Two commits, same shape as SP7c Phase 4 Tasks 2/3/4. Commit 1 (`pcsx2-libretro/retronest-libretro` branch) extends `CoreOptionsGraphics.{h,cpp}` with the 9 knob declarations + parse + apply, extends the per-launch `[CoreOptions] graphics.postproc:` echo line in `CoreOptions.cpp`, and adds Cases 16/16b to `test_core_options.cpp` — schema-fidelity intentionally RED at this point. Commit 2 (`RetroNest-Project/main` branch) appends 9 `gopt(...)` rows to `pcsx2_libretro_adapter.cpp::settingsSchema()` under `subcategory="Post-Processing"` — schema-fidelity returns to GREEN at 66/66.

**Tech Stack:** C++20, libretro core options v2 API (`retro_core_option_v2_definition`), Qt 6 (host), Python 3 (`tools/check_schema_fidelity.py`), CMake.

**Spec:** `RetroNest-Project/docs/superpowers/specs/2026-05-14-pcsx2-libretro-sp7c-phase4-task5-design.md` (commit `e287bb1`).

**Repo HEADs at start:**
- `pcsx2-libretro` (working dir) `retronest-libretro` HEAD `b53948ab4`.
- `RetroNest-Project` `main` HEAD `f8d1bdf` (8 ahead of `origin/main` — do not push without explicit user request).

---

## File Structure

### Files modified — pcsx2-libretro repo (Commit 1: core, RED)

- `pcsx2-libretro/CoreOptionsGraphics.h` — fill the empty `Values::PostProcessing` struct (currently lines 102-104) with 9 fields. Header-only edit.
- `pcsx2-libretro/CoreOptionsGraphics.cpp`:
  - `AppendDefinitions`: 9 new `out.push_back({...})` blocks appended after the Texture Replacement block ends at line 639.
  - `Parse`: 9 new branches appended after the Texture Replacement block ends at line 730.
  - `ApplyDefaults`: 9 new `si.SetXValue` calls appended after the Texture Replacement block ends at line 779.
- `pcsx2-libretro/CoreOptions.cpp` — extend `ReadResolved` with one new `FrontendLog` call for `[CoreOptions] graphics.postproc: ...` line, appended after the existing `graphics.texrep` block (currently ends line 175).
- `pcsx2-libretro/tools/test_core_options.cpp` — add Case 16 + Case 16b after Case 15b (currently ends line 521).

### Files modified — RetroNest-Project repo (Commit 2: host, GREEN)

- `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — append 9 `s.append(gopt(...))` rows to `settingsSchema()` after the Texture Replacement block ends at line 833. Insertion point is just before `return s;` at line 835.

### No files created. No directories changed.

---

## Reference: Authoritative defaults (verified against PCSX2 master)

| Knob | Source-of-truth file:line | Default |
|---|---|---|
| `pcsx2_cas_mode` | `pcsx2/Config.h:723` `DEFAULT_CAS_MODE = GSCASMode::Disabled` | `0` |
| `pcsx2_cas_sharpness` | `pcsx2/Config.h:894` `u8 CAS_Sharpness = 50` | `50` |
| `pcsx2_fxaa` | `pcsx2/Config.h:809` `FXAA : 1` (bitfield zero-init) | `false` |
| `pcsx2_tv_shader` | `pcsx2/Config.h:870` `u8 TVShader = 0` | `0` |
| `pcsx2_shade_boost` | `pcsx2/Config.h:810` `ShadeBoost : 1` (bitfield zero-init) | `false` |
| `pcsx2_shade_boost_brightness` | `pcsx2/Config.h:741` `DEFAULT_SHADEBOOST_BRIGHTNESS = 50` | `50` |
| `pcsx2_shade_boost_contrast` | `pcsx2/Config.h:742` `DEFAULT_SHADEBOOST_CONTRAST = 50` | `50` |
| `pcsx2_shade_boost_saturation` | `pcsx2/Config.h:744` `DEFAULT_SHADEBOOST_SATURATION = 50` | `50` |
| `pcsx2_shade_boost_gamma` | `pcsx2/Config.h:743` `DEFAULT_SHADEBOOST_GAMMA = 50` | `50` |

All 4 ShadeBoost sliders default to **50** (the neutral-midpoint of the shader formula `value/50`). This corrects an earlier prep-memory note that claimed Gamma defaulted to 100; the PCSX2 source and the host `pcsx2_adapter.cpp:643` both say 50.

---

## Reference: Working-directory conventions

- The pcsx2-libretro repo working tree is at `/Users/mark/Documents/Projects/pcsx2-libretro/`. The core code lives in a sub-folder `pcsx2-libretro/` (so files are at `pcsx2-libretro/pcsx2-libretro/CoreOptionsGraphics.cpp` from the repo root). Use absolute paths in commands to avoid confusion.
- The RetroNest repo is at `/Users/mark/Documents/Projects/RetroNest-Project/`.
- `cd` is required to switch repos for git operations. Each commit step `cd`s into the right repo first.
- The schema-fidelity script's `--core` arg is a *glob string* — it must be wrapped in quotes so the shell does not expand it before passing it to Python.

---

## Task 1: Extend `Values::PostProcessing` struct in `CoreOptionsGraphics.h`

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptionsGraphics.h:102-104`

- [ ] **Step 1: Replace the placeholder struct body**

In `CoreOptionsGraphics.h`, replace the existing 3-line stub:

```cpp
    struct PostProcessing {
        // Phase 4 Task 5 fills these.
    } post_processing;
```

with the populated struct:

```cpp
    struct PostProcessing {
        // 9 knobs mirroring standalone PCSX2 Graphics/Post-Processing
        // sub-tab. All stored as INI under [EmuCore/GS]. Defaults match
        // pcsx2/Config.h:723,741-744,870,894 verbatim — all 5 slider
        // knobs default to 50 (neutral midpoint of the shader formula
        // value/50). Two groups: Sharpening/Anti-Aliasing (CAS×2 + FXAA)
        // and Filters (TVShader, ShadeBoost master + 4 sliders).
        int  cas_mode               = 0;     // GSCASMode::Disabled
        int  cas_sharpness          = 50;
        bool fxaa                   = false;
        int  tv_shader              = 0;
        bool shade_boost            = false;
        int  shade_boost_brightness = 50;
        int  shade_boost_contrast   = 50;
        int  shade_boost_saturation = 50;
        int  shade_boost_gamma      = 50;
    } post_processing;
```

- [ ] **Step 2: Confirm header still parses**

The header has no test of its own — Task 2's compile of `CoreOptionsGraphics.cpp` will catch any typo.

---

## Task 2: Append `AppendDefinitions` blocks for the 9 knobs

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptionsGraphics.cpp:639` (insert after the Texture Replacement block's closing `});` on line 639, before the closing brace of `AppendDefinitions` on line 640)

- [ ] **Step 1: Add the Post-Processing section header comment + 9 push_back blocks**

After the closing `});` of the last Texture Replacement block at line 639 and before the `}` that closes `AppendDefinitions` at line 640, paste this:

```cpp
    // ── Post-Processing sub-tab (9 knobs) — Phase 4 Task 5 ─────────────
    //
    // Two groups: Sharpening/Anti-Aliasing (CAS Mode + CAS Sharpness +
    // FXAA) and Filters (TV Shader + ShadeBoost master + 4 ShadeBoost
    // sliders). All stored under [EmuCore/GS].
    //
    // 5 standalone-side int sliders (CAS Sharpness + 4 ShadeBoost rows)
    // become Combo with stops 0/25/50/75/100 because libretro core
    // options v2 is Combo-only. The shared 5-stop value list is inlined
    // at each call site (matches Task 2's Crop{Left,Top,Right,Bottom}
    // pattern — keeps grep-ability, low duplication cost). Default 50
    // for all 5 hits a stop and is the neutral midpoint of PCSX2's
    // shader formula value/50.
    //
    // dependsOn for Sharpness + ShadeBoost slaves is expressed on the
    // host side (Combo dependsOn strings). Within-Graphics gates resolve
    // correctly via GenericSettingsPage::refreshDependencies (cross-
    // category limitation does not apply — see SP7c memory
    // cross_category_dependson_limitation).

    out.push_back({
        "pcsx2_cas_mode",
        "Contrast Adaptive Sharpening (CAS)",
        nullptr,
        "AMD's Contrast Adaptive Sharpening pass on the final image. "
        "Sharpen Only sharpens at the internal render resolution; "
        "Sharpen and Resize sharpens at the display resolution.",
        nullptr,
        nullptr,
        {
            { "0", "Disabled (Default)" },
            { "1", "Sharpen Only (Internal Resolution)" },
            { "2", "Sharpen and Resize (Display Resolution)" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_cas_sharpness",
        "CAS Sharpness",
        nullptr,
        "Strength of the CAS sharpening pass. Higher values produce a "
        "sharper image with more visible noise. Standalone PCSX2 exposes "
        "a 1–100% slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_fxaa",
        "FXAA",
        nullptr,
        "Fast Approximate Anti-Aliasing. A single-pass shader that "
        "softens jagged edges with low GPU cost.",
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
        "pcsx2_tv_shader",
        "TV Shader",
        nullptr,
        "Applies a CRT-style filter to the final output for an authentic "
        "retro look. None disables the filter.",
        nullptr,
        nullptr,
        {
            { "0", "None (Default)" },
            { "1", "Scanline Filter" },
            { "2", "Diagonal Filter" },
            { "3", "Triangular Filter" },
            { "4", "Wave Filter" },
            { "5", "Lottes CRT" },
            { "6", "4xRGSS Downsampling" },
            { "7", "NxAGSS Downsampling" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_shade_boost",
        "Shade Boost",
        nullptr,
        "Master toggle for manual brightness, contrast, saturation, and "
        "gamma adjustment via the Shade Boost shader.",
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
        "pcsx2_shade_boost_brightness",
        "Shade Boost — Brightness",
        nullptr,
        "Brightness multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); 0% blacks out the image; 100% is double "
        "brightness. Standalone PCSX2 exposes a 1–100% slider; libretro "
        "offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default — Neutral)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_shade_boost_contrast",
        "Shade Boost — Contrast",
        nullptr,
        "Contrast multiplier when Shade Boost is enabled. 50% is neutral "
        "(no change). Standalone PCSX2 exposes a 1–100% slider; libretro "
        "offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default — Neutral)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_shade_boost_saturation",
        "Shade Boost — Saturation",
        nullptr,
        "Color-saturation multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); 0% produces grayscale. Standalone PCSX2 "
        "exposes a 1–100% slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default — Neutral)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_shade_boost_gamma",
        "Shade Boost — Gamma",
        nullptr,
        "Gamma-correction multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); lower values darken midtones, higher "
        "values brighten midtones. Standalone PCSX2 exposes a 1–100% "
        "slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default — Neutral)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });
```

- [ ] **Step 2: Compile-check the file in isolation**

Run from the working dir:

```
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
clang++ -std=c++20 -I.. -DCORE_OPTIONS_TEST_ONLY \
    -c ../CoreOptionsGraphics.cpp -o /tmp/cog_check.o
```

Expected: returns 0, no warnings, no errors. (`clangd` may show false-positives in editors — those are noise; trust the actual compile.)

---

## Task 3: Append `Parse` branches for the 9 knobs

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptionsGraphics.cpp:730` (after Texture Replacement Parse block, before the closing `}` of `Parse` on line 731)

- [ ] **Step 1: Add 9 Parse branches**

After `out.texture_replacement.dump_textures_with_fmv_active = parse_bool(v);` on line 730 and before the closing `}` of `Parse`, paste:

```cpp

    // ── Post-Processing sub-tab ──
    if (const char* v = query("pcsx2_cas_mode"))
        out.post_processing.cas_mode = parse_int(v, 0);
    if (const char* v = query("pcsx2_cas_sharpness"))
        out.post_processing.cas_sharpness = parse_int(v, 50);
    if (const char* v = query("pcsx2_fxaa"))
        out.post_processing.fxaa = parse_bool(v);
    if (const char* v = query("pcsx2_tv_shader"))
        out.post_processing.tv_shader = parse_int(v, 0);
    if (const char* v = query("pcsx2_shade_boost"))
        out.post_processing.shade_boost = parse_bool(v);
    if (const char* v = query("pcsx2_shade_boost_brightness"))
        out.post_processing.shade_boost_brightness = parse_int(v, 50);
    if (const char* v = query("pcsx2_shade_boost_contrast"))
        out.post_processing.shade_boost_contrast = parse_int(v, 50);
    if (const char* v = query("pcsx2_shade_boost_saturation"))
        out.post_processing.shade_boost_saturation = parse_int(v, 50);
    if (const char* v = query("pcsx2_shade_boost_gamma"))
        out.post_processing.shade_boost_gamma = parse_int(v, 50);
```

- [ ] **Step 2: Re-run compile-check**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
clang++ -std=c++20 -I.. -DCORE_OPTIONS_TEST_ONLY \
    -c ../CoreOptionsGraphics.cpp -o /tmp/cog_check.o
```

Expected: returns 0, no warnings, no errors.

---

## Task 4: Append `ApplyDefaults` writes for the 9 knobs

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptionsGraphics.cpp:779` (after Texture Replacement ApplyDefaults block, before the closing `}` of `ApplyDefaults` on line 780, but before `#endif` on line 781)

- [ ] **Step 1: Add 9 SetXValue writes**

After `si.SetBoolValue("EmuCore/GS", "DumpTexturesWithFMVActive", v.texture_replacement.dump_textures_with_fmv_active);` on line 779 and before the closing `}` on line 780, paste:

```cpp

    // ── Post-Processing sub-tab ──
    si.SetIntValue ("EmuCore/GS", "CASMode",               v.post_processing.cas_mode);
    si.SetIntValue ("EmuCore/GS", "CASSharpness",          v.post_processing.cas_sharpness);
    si.SetBoolValue("EmuCore/GS", "fxaa",                  v.post_processing.fxaa);
    si.SetIntValue ("EmuCore/GS", "TVShader",              v.post_processing.tv_shader);
    si.SetBoolValue("EmuCore/GS", "ShadeBoost",            v.post_processing.shade_boost);
    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Brightness", v.post_processing.shade_boost_brightness);
    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Contrast",   v.post_processing.shade_boost_contrast);
    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Saturation", v.post_processing.shade_boost_saturation);
    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Gamma",      v.post_processing.shade_boost_gamma);
```

INI key names match `pcsx2_adapter.cpp:587-643` (the canonical PCSX2 INI keys).

- [ ] **Step 2: Compile-check the full module against pcsx2 (NON-test build)**

The test-only flag stubs out `MemorySettingsInterface` — to verify the real `ApplyDefaults` block compiles, do a real-mode syntax check:

```
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
clang++ -std=c++20 -I.. -DCORE_OPTIONS_TEST_ONLY \
    -c ../CoreOptionsGraphics.cpp -o /tmp/cog_check.o
```

Note: in `CORE_OPTIONS_TEST_ONLY` mode the `ApplyDefaults` body is `#ifdef`-guarded out (`CoreOptionsGraphics.cpp:733-781`), so the test-only compile only validates `AppendDefinitions` and `Parse`. The `ApplyDefaults` block is verified later in Task 8's full `build-universal.sh`. There is no faster intermediate check available.

---

## Task 5: Extend the per-launch `[CoreOptions] graphics.postproc:` echo

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions.cpp:175` (insert after the `graphics.texrep` `FrontendLog` block ends at line 175, before `return r;` on line 177)

- [ ] **Step 1: Add the postproc echo block**

After the closing `);` of the texrep `FrontendLog` call on line 175 and before `return r;` on line 177, paste:

```cpp

    const auto& gp = r.graphics.post_processing;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] graphics.postproc: cas=%d cas_sharp=%d fxaa=%s "
        "tv=%d shade=%s sb_br=%d sb_co=%d sb_sa=%d sb_ga=%d",
        gp.cas_mode, gp.cas_sharpness,
        gp.fxaa ? "on" : "off",
        gp.tv_shader,
        gp.shade_boost ? "on" : "off",
        gp.shade_boost_brightness, gp.shade_boost_contrast,
        gp.shade_boost_saturation, gp.shade_boost_gamma);
```

This matches the format of the existing `graphics.texrep` echo block (line 167-175): bools as `on`/`off`, ints raw.

- [ ] **Step 2: Compile-check**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
clang++ -std=c++20 -I.. -DCORE_OPTIONS_TEST_ONLY \
    -c ../CoreOptions.cpp -o /tmp/co_check.o
```

Expected: returns 0, no warnings, no errors.

---

## Task 6: Add `test_core_options.cpp` Cases 16 + 16b

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_core_options.cpp:521` (insert after Case 15b's last `check_bool` on line 521, before the `std::printf("\n%d failure(s)\n", failures);` on line 523)

- [ ] **Step 1: Add Cases 16 and 16b**

After Case 15b's last `check_bool` line on line 521 and before `std::printf("\n%d failure(s)\n", failures);` on line 523, paste:

```cpp

    // -------- Case 16: Graphics/Post-Processing round-trip --------
    //
    // SP7c Phase 4 Task 5 representative test for the Post-Processing
    // sub-tab. Picks one knob per value-encoding flavor + one per group:
    //   - 3-stop Combo, non-default (cas_mode = 1)
    //   - 5-stop Combo, non-default, depends on cas_mode flip
    //     (cas_sharpness = 75)
    //   - bool toggled to true (fxaa = enabled)
    //   - 8-stop Combo (tv_shader = 3)
    //   - bool master toggled to true (shade_boost = enabled)
    //   - 5-stop Combo (shade_boost_brightness = 25)
    //   - 5-stop Combo at edge stop (shade_boost_gamma = 0)
    // Leaves shade_boost_contrast and shade_boost_saturation at default
    // 50 to demonstrate selective-flip preserves untouched fields.
    fake::reset();
    fake::variables["pcsx2_cas_mode"]               = "1";
    fake::variables["pcsx2_cas_sharpness"]          = "75";
    fake::variables["pcsx2_fxaa"]                   = "enabled";
    fake::variables["pcsx2_tv_shader"]              = "3";
    fake::variables["pcsx2_shade_boost"]            = "enabled";
    fake::variables["pcsx2_shade_boost_brightness"] = "25";
    fake::variables["pcsx2_shade_boost_gamma"]      = "0";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 16 cas_mode=1",
                r.graphics.post_processing.cas_mode, 1);
    check_int ("Case 16 cas_sharpness=75",
                r.graphics.post_processing.cas_sharpness, 75);
    check_bool("Case 16 fxaa=on",
                r.graphics.post_processing.fxaa, true);
    check_int ("Case 16 tv_shader=3",
                r.graphics.post_processing.tv_shader, 3);
    check_bool("Case 16 shade_boost=on",
                r.graphics.post_processing.shade_boost, true);
    check_int ("Case 16 shade_boost_brightness=25",
                r.graphics.post_processing.shade_boost_brightness, 25);
    check_int ("Case 16 shade_boost_contrast preserved=50",
                r.graphics.post_processing.shade_boost_contrast, 50);
    check_int ("Case 16 shade_boost_saturation preserved=50",
                r.graphics.post_processing.shade_boost_saturation, 50);
    check_int ("Case 16 shade_boost_gamma=0 (edge)",
                r.graphics.post_processing.shade_boost_gamma, 0);

    // -------- Case 16b: Graphics/Post-Processing default-when-unset --------
    //
    // Confirms the full default vector when no env-var is set. There is
    // no single "non-default-default" anchor in this sub-tab (all 5
    // sliders default to 50, all 2 bools to false, all 2 combos to 0)
    // — so we assert all 9 fields explicitly to catch drift in any
    // individual initializer.
    fake::reset();

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 16b cas_mode default=0",
                r.graphics.post_processing.cas_mode, 0);
    check_int ("Case 16b cas_sharpness default=50",
                r.graphics.post_processing.cas_sharpness, 50);
    check_bool("Case 16b fxaa default=off",
                r.graphics.post_processing.fxaa, false);
    check_int ("Case 16b tv_shader default=0",
                r.graphics.post_processing.tv_shader, 0);
    check_bool("Case 16b shade_boost default=off",
                r.graphics.post_processing.shade_boost, false);
    check_int ("Case 16b shade_boost_brightness default=50",
                r.graphics.post_processing.shade_boost_brightness, 50);
    check_int ("Case 16b shade_boost_contrast default=50",
                r.graphics.post_processing.shade_boost_contrast, 50);
    check_int ("Case 16b shade_boost_saturation default=50",
                r.graphics.post_processing.shade_boost_saturation, 50);
    check_int ("Case 16b shade_boost_gamma default=50",
                r.graphics.post_processing.shade_boost_gamma, 50);
```

- [ ] **Step 2: Build the test binary**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
clang++ -std=c++20 -I.. test_core_options.cpp \
    ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
    ../CoreOptionsMemoryCards.cpp ../CoreOptionsGraphics.cpp \
    -DCORE_OPTIONS_TEST_ONLY -o test_core_options
```

Expected: builds cleanly with no warnings.

- [ ] **Step 3: Run the test, expect 40 cases all PASS**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
./test_core_options 2>&1 | tail -25
```

Expected: last line is `0 failure(s)`. Cases 16 and 16b appear in the output with `[PASS]` markers for all 18 assertions (9 in Case 16, 9 in Case 16b).

If any assertion fails, do not move on — the failure points to a typo in either the struct (Task 1), the Parse branches (Task 3), or the test cases themselves. Fix and re-run.

---

## Task 7: Confirm schema-fidelity is intentionally RED

**Files:**
- No edits. This task is a verification gate before committing.

- [ ] **Step 1: Run the schema-fidelity check**

```
/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py \
    --core "/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions*.cpp" \
    --host /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp 2>&1 | tail -15
```

Expected output: report shows the **9 new core keys are present but unmatched on the host side**, ending with something like `9 drift entries; check both sides match exactly.` exit code 1. The 9 keys named in the drift list must be exactly:

- `pcsx2_cas_mode`
- `pcsx2_cas_sharpness`
- `pcsx2_fxaa`
- `pcsx2_tv_shader`
- `pcsx2_shade_boost`
- `pcsx2_shade_boost_brightness`
- `pcsx2_shade_boost_contrast`
- `pcsx2_shade_boost_saturation`
- `pcsx2_shade_boost_gamma`

If the count is anything other than 9, or any other key is in the drift list, stop — there is a typo in the core-side keys. Fix and re-run before committing.

This RED state is intentional: the matching host-side commit comes in Task 9.

---

## Task 8: Commit the core (RED) commit

**Files:**
- Stage: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptionsGraphics.h`
- Stage: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptionsGraphics.cpp`
- Stage: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions.cpp`
- Stage: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_core_options.cpp`

- [ ] **Step 1: Verify nothing else is staged**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro && git status --short
```

Expected: only the 4 modified files above + the pre-existing untracked `pcsx2-libretro/tools/__pycache__/` etc. (do NOT stage the untracked artifacts).

- [ ] **Step 2: Stage the 4 files explicitly**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro && \
git add pcsx2-libretro/CoreOptionsGraphics.h \
        pcsx2-libretro/CoreOptionsGraphics.cpp \
        pcsx2-libretro/CoreOptions.cpp \
        pcsx2-libretro/tools/test_core_options.cpp
```

- [ ] **Step 3: Commit with the documented message**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro && git commit -m "$(cat <<'EOF'
SP7c Phase 4 Task 5 (core): Post-Processing sub-tab knobs (9)

Adds Values::PostProcessing (9 fields), AppendDefinitions blocks,
Parse branches, ApplyDefaults writes, the per-launch graphics.postproc
echo line, and test_core_options Cases 16+16b.

Breakdown: 9 knobs across Sharpening/AA (CAS Mode + CAS Sharpness +
FXAA) and Filters (TV Shader, ShadeBoost master + 4 ShadeBoost
sliders). All 5 slider defaults = 50 (verified against
pcsx2/Config.h:741-744,894); all 2 bool defaults = false; all 2 combo
defaults = 0.

Schema fidelity intentionally RED at this commit -- 9 core keys
declared with no host row yet. Matching host commit restores green.

test_core_options 40/40 0 failures.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 4: Verify commit landed cleanly**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro && git log -1 --stat
```

Expected: commit at HEAD, 4 files changed, no additional files.

---

## Task 9: Append 9 `gopt(...)` rows to the host adapter

**Files:**
- Modify: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp:833` (insert after the last Texture Replacement `gopt` block on line 833, before `return s;` on line 835)

- [ ] **Step 1: Insert the 9 Post-Processing rows**

After the closing `));` of the last Texture Replacement gopt block on line 833 (the `pcsx2_dump_textures_with_fmv_active` row) and before the blank line + `return s;` on line 835, paste:

```cpp

    // ── Graphics > Post-Processing (Phase 4 Task 5) ──────────────────────
    //
    // 9 knobs mirroring standalone's GraphicsPostProcessingSettingsTab.
    // Two groups: "Sharpening/Anti-Aliasing" (CAS×2 + FXAA) and
    // "Filters" (TV Shader, ShadeBoost master + 4 ShadeBoost sliders).
    // INI section is [EmuCore/GS] for all 9 (handled core-side in
    // CoreOptionsGraphics::ApplyDefaults).
    //
    // 5 standalone-side int sliders (CAS Sharpness + 4 ShadeBoost
    // rows) become Combo with stops 0/25/50/75/100 because libretro
    // core options v2 is Combo-only. Default 50 hits a stop and is the
    // neutral midpoint of PCSX2's shader formula value/50 (verified
    // against pcsx2/Config.h:741-744,894).
    //
    // dependsOn: pcsx2_cas_sharpness gates on pcsx2_cas_mode!=0
    // (value-equality form, Task 3 precedent: pcsx2_tri_filter!=2).
    // The 4 ShadeBoost sliders gate on pcsx2_shade_boost (master-bool
    // bare-key form, Task 4 precedent). All 5 dependsOn keys live
    // within the Graphics card -- findChildren resolution works
    // correctly. Cross-category limitation does not apply here.

    s.append(gopt(
        "Post-Processing", "Sharpening/Anti-Aliasing",
        "pcsx2_cas_mode", "Contrast Adaptive Sharpening (CAS)", "0",
        {{"Disabled (Default)", "0"},
         {"Sharpen Only (Internal Resolution)", "1"},
         {"Sharpen and Resize (Display Resolution)", "2"}},
        "AMD's Contrast Adaptive Sharpening pass on the final image. "
        "Sharpen Only sharpens at the internal render resolution; "
        "Sharpen and Resize sharpens at the display resolution."));

    s.append(gopt(
        "Post-Processing", "Sharpening/Anti-Aliasing",
        "pcsx2_cas_sharpness", "CAS Sharpness", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Strength of the CAS sharpening pass. Higher values produce a "
        "sharper image with more visible noise. Standalone PCSX2 exposes "
        "a 1-100% slider; libretro offers enumerated stops.",
        "pcsx2_cas_mode!=0"));

    s.append(gopt(
        "Post-Processing", "Sharpening/Anti-Aliasing",
        "pcsx2_fxaa", "FXAA", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Fast Approximate Anti-Aliasing. A single-pass shader that "
        "softens jagged edges with low GPU cost."));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_tv_shader", "TV Shader", "0",
        {{"None (Default)", "0"},
         {"Scanline Filter", "1"},
         {"Diagonal Filter", "2"},
         {"Triangular Filter", "3"},
         {"Wave Filter", "4"},
         {"Lottes CRT", "5"},
         {"4xRGSS Downsampling", "6"},
         {"NxAGSS Downsampling", "7"}},
        "Applies a CRT-style filter to the final output for an authentic "
        "retro look. None disables the filter."));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost", "Shade Boost", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Master toggle for manual brightness, contrast, saturation, and "
        "gamma adjustment via the Shade Boost shader."));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost_brightness", "Shade Boost — Brightness", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default — Neutral)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Brightness multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); 0% blacks out the image; 100% is double "
        "brightness. Standalone PCSX2 exposes a 1-100% slider; libretro "
        "offers enumerated stops.",
        "pcsx2_shade_boost"));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost_contrast", "Shade Boost — Contrast", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default — Neutral)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Contrast multiplier when Shade Boost is enabled. 50% is neutral "
        "(no change). Standalone PCSX2 exposes a 1-100% slider; libretro "
        "offers enumerated stops.",
        "pcsx2_shade_boost"));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost_saturation", "Shade Boost — Saturation", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default — Neutral)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Color-saturation multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); 0% produces grayscale. Standalone PCSX2 "
        "exposes a 1-100% slider; libretro offers enumerated stops.",
        "pcsx2_shade_boost"));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost_gamma", "Shade Boost — Gamma", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default — Neutral)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Gamma-correction multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); lower values darken midtones, higher values "
        "brighten midtones. Standalone PCSX2 exposes a 1-100% slider; "
        "libretro offers enumerated stops.",
        "pcsx2_shade_boost"));
```

---

## Task 10: Verify schema-fidelity returns to GREEN

**Files:**
- No edits. Verification gate before committing.

- [ ] **Step 1: Run the schema-fidelity check**

```
/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py \
    --core "/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions*.cpp" \
    --host /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp 2>&1 | tail -3
```

Expected exact output:

```
Schema fidelity OK: 66 core keys, 66 host keys, byte-for-byte match.
```

If counts are not exactly 66/66, stop. The most likely failures:
- key typo on either side (e.g. `pcsx2_shadeboost_gamma` vs `pcsx2_shade_boost_gamma`)
- value-list mismatch (a stop missing on one side)
- default-string mismatch (e.g. `"50"` core vs `"50%"` host)

Fix the offending side. The drift report names which keys mismatch.

---

## Task 11: Commit the host (GREEN) commit

**Files:**
- Stage: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`

- [ ] **Step 1: Verify only the host adapter file is staged**

```
cd /Users/mark/Documents/Projects/RetroNest-Project && git status --short
```

Expected: a single modified file `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`. No other changes.

- [ ] **Step 2: Stage and commit**

```
cd /Users/mark/Documents/Projects/RetroNest-Project && \
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp && \
git commit -m "$(cat <<'EOF'
SP7c Phase 4 Task 5 (host): Post-Processing sub-tab rows (9)

Appends 9 gopt() rows under subcategory="Post-Processing" --
3 rows in Sharpening/Anti-Aliasing group + 6 rows in Filters group.

CASSharpness gated on cas_mode!=0 (value-equality dependsOn,
Task 3 precedent). 4 ShadeBoost sliders gated on shade_boost
(master-bool dependsOn, Task 4 precedent). All 5 dependsOn
chains within the Graphics page -- findChildren resolves
cleanly. Cross-category limitation does not apply.

schema-fidelity OK: 66 core keys, 66 host keys, byte-for-byte match.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 3: Verify commit landed cleanly**

```
cd /Users/mark/Documents/Projects/RetroNest-Project && git log -1 --stat
```

Expected: 1 file changed, `pcsx2_libretro_adapter.cpp`.

---

## Task 12: Universal build (full integration)

**Files:**
- No edits. Full build to confirm everything links.

- [ ] **Step 1: Run the universal build**

```
cd /Users/mark/Documents/Projects/RetroNest-Project && ./scripts/build-universal.sh 2>&1 | tail -30
```

Expected: build succeeds. Cadence is ~3-5 min incremental; the 9-knob delta is small. If a build fails:
- INI-key typo in `ApplyDefaults` (Task 4) is the most likely cause — the test-only compile didn't exercise this path.
- `MemorySettingsInterface::SetIntValue` / `SetBoolValue` signature drift would also surface here.
- Linker errors involving `Pcsx2Libretro::CoreOptions::Graphics::*` mean the new fields aren't all referenced consistently.

If clangd showed "false-positive" warnings during edits (e.g. `'common/MemorySettingsInterface.h' not found`), ignore them — the cmake build is the truth.

- [ ] **Step 2: Re-run schema-fidelity sanity (post-build)**

The build produces no schema artifact, but re-running the check after the rebuild proves the source files are still consistent:

```
/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py \
    --core "/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/CoreOptions*.cpp" \
    --host /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp 2>&1 | tail -3
```

Expected: `Schema fidelity OK: 66 core keys, 66 host keys, byte-for-byte match.`

- [ ] **Step 3: Re-run test_core_options sanity**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools && \
./test_core_options 2>&1 | tail -3
```

Expected: `0 failure(s)`.

---

## Task 13: User-driven live-smoke gate

**Files:**
- No edits. The user runs RetroNest and verifies the 9 rows wire end-to-end.

- [ ] **Step 1: Tell the user the build is ready and walk through the smoke checklist**

Hand the user this exact checklist (copy verbatim into the user-facing message):

> Phase 4 Task 5 build is ready. Live-smoke checklist:
>
> 1. Launch RetroNest under Rosetta with `[CoreOptions]` filter:
>    ```
>    DYLD_FRAMEWORK_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/Frameworks \
>    QT_PLUGIN_PATH=/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/PlugIns \
>    arch -x86_64 \
>    /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest 2>&1 \
>      | grep --line-buffered -E "\[CoreOptions\]|graphics\.postproc|FATAL|ERROR"
>    ```
>    (Do not compose `APP=...; $APP/...` on one line — the second `$APP` expands before the assignment.)
>
> 2. Open Graphics card → Post-Processing sub-tab. Verify all 9 rows visible across 2 groups (3 in Sharpening/Anti-Aliasing, 6 in Filters). Group order: Sharpening/Anti-Aliasing first.
>
> 3. With **CAS = Disabled**: confirm the **CAS Sharpness** row is greyed.
>
> 4. Set **CAS = Sharpen Only**: confirm CAS Sharpness un-greys. Launch a game; confirm the next-launch echo line includes `cas=1`.
>
> 5. With **Shade Boost = Disabled**: confirm all 4 Shade Boost slider rows greyed.
>
> 6. Set **Shade Boost = Enabled**: confirm all 4 slider rows un-grey.
>
> 7. Visual-effect check 1 — Shade Boost = Enabled + Brightness = 100%: launch, picture noticeably brighter.
>
> 8. Visual-effect check 2 — Shade Boost = Enabled + Gamma = 0%: launch, picture noticeably darker.
>
> 9. Optional visual-effect check 3 — TV Shader = Scanline Filter: launch, scanline overlay visible.
>
> Note: when a master toggles off, dependent rows grey but their **stored values are preserved** (`shade_boost=off shade_boost_gamma=50` is normal). PCSX2 ignores the gated values at the core level when the master is off, so the rows are functionally inert. Same as standalone behavior.
>
> Note: the Qt-on-Rosetta SIGABRT will fire again at app exit. Ignore it (separate tracked issue).

- [ ] **Step 2: Wait for user feedback**

Do not proceed past this point until the user confirms smoke passed. If smoke fails, capture the failing observation and the relevant `[CoreOptions]` echo line; root-cause before patching.

- [ ] **Step 3: On smoke-pass, update the SP7c kickoff memory**

Update `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/sp7c_kickoff.md` to mark Task 5 shipped + smoke-verified, advance schema-fidelity to 66/66, and re-point the "next focus" line to Task 6 (On-Screen Display, ~23 knobs). The Phase 4 Task 6 prep memory does not yet exist — leave a hook in sp7c_kickoff.md saying so.

Mark the `phase4_task5_prep` memory as superseded (parallel to how `phase4_task4_prep` is marked).

---

## Self-Review

**Spec coverage check:**
- Spec inventory of 9 knobs → Tasks 1, 2, 3, 4 (struct + AppendDefinitions + Parse + ApplyDefaults). ✔
- Spec architecture (two-commit RED→GREEN) → Tasks 8 (RED) + 11 (GREEN). ✔
- Spec testing (Case 16 + 16b in test_core_options, schema-fidelity 66/66, live-smoke 10 steps) → Tasks 6, 7, 10, 13. ✔
- Spec error handling (none specific, parse fallbacks already in place) → no task needed; reusing existing `parse_int` / `parse_bool` lambdas. ✔
- Spec out-of-scope (patches.zip, Qt teardown, masterValues promotion, EmuFolders::Textures) → not in any task. ✔
- Spec commit-message templates → reproduced verbatim in Tasks 8 and 11. ✔
- Spec risks (clangd noise, INI key drift, dependsOn syntax) → mitigations called out in Tasks 4 step 2, 10 failure modes, 12 step 1. ✔

**Placeholder scan:** No "TBD"/"TODO"/"appropriate"/"similar to". Every code block is the literal text to paste. Every command has an exact path. Every expected output is named.

**Type / name consistency:** Spot-checked field names across struct (Task 1), Parse (Task 3), ApplyDefaults (Task 4), echo (Task 5), test (Task 6), and host gopt (Task 9): `cas_mode`, `cas_sharpness`, `fxaa`, `tv_shader`, `shade_boost`, `shade_boost_brightness`, `shade_boost_contrast`, `shade_boost_saturation`, `shade_boost_gamma` — all 9 used identically in all 6 sites. INI keys `CASMode`, `CASSharpness`, `fxaa`, `TVShader`, `ShadeBoost`, `ShadeBoost_Brightness/Contrast/Saturation/Gamma` match `pcsx2_adapter.cpp:587-643` (canonical PCSX2 INI key forms). Default values `0/50/false/0/false/50/50/50/50` match across struct (Task 1), AppendDefinitions (Task 2), Parse fallbacks (Task 3), echo (no defaults — uses parsed values), test Case 16b (Task 6), and host (Task 9).
