# PPSSPP Libretro — Phase B+C: Settings Schema + Hub Cards Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Settings → PSP page in RetroNest render 5 populated category cards (Recommended / System / Video / Input / Hacks) covering 43 upstream PPSSPP libretro core options.

**Architecture:** Override `settingsSchema()` and `settingsHubCards()` on `PpssppLibretroAdapter`, mirroring the `mgba_libretro_adapter.cpp:82-412` pattern. All 43 unique options use `Storage::LibretroOption`. 10 of them are duplicated into a `"Recommended"` category for a curated shortcut card (also mgba's pattern — verified at `mgba_libretro_adapter.cpp:145-150` vs the same key under `"System"` later). Total `SettingDef` count = 53.

**Tech Stack:** C++17, Qt 6.11, RetroNest libretro adapter layer.

**Spec doc:** `docs/superpowers/specs/2026-05-22-ppsspp-phase-bc-settings-design.md` (commit `47aa1c8` on main).

**Branch state:** main is at `47aa1c8` (post Phase A + spec doc). Working tree clean. Build dir at `cpp/build/`.

---

## Background facts (cached so the executor doesn't have to re-research)

### Reference adapter

The closest precedent is `MgbaLibretroAdapter::settingsSchema` at `cpp/src/adapters/libretro/mgba_libretro_adapter.cpp:82-313` plus `settingsHubCards` at `:385-412`. Copy the `opt()` lambda pattern verbatim. PPSSPP has no `FrontendSetting` entries in this phase (aspect ratio is intentionally deferred to Phase E), so the `frontend()` helper from mgba isn't needed.

PCSX2's schema at `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp:96-1087` is much larger because PS2 has more knobs; same code structure.

### SettingDef shape

From `cpp/src/core/setting_def.h:11-126`. The fields you'll set in this phase:

- `storage = SettingDef::Storage::LibretroOption` (always, for every entry)
- `key` — libretro option key (e.g. `"ppsspp_internal_resolution"`)
- `label` — display label shown in the row
- `defaultValue` — upstream `default_value` from `libretro/libretro_core_options.h`
- `type = SettingDef::Combo` (always — every PPSSPP libretro option is a discrete-value list)
- `options` — `QVector<QPair<QString,QString>>` of `(displayLabel, iniValue)` pairs. For options where labels match values (most), label == value.
- `category` — one of `"Recommended"`, `"System"`, `"Video"`, `"Input"`, `"Hacks"`.
- `tooltip` — one-line description; verbatim from upstream `info` strings where possible

You do NOT need to touch: `subcategory`, `group`, `section`, `minVal`, `maxVal`, `step`, `layout`, `suffix`, `dependsOn`, `bitmask`, `inverted`, `recommendedValue`, `saveTransform`, `loadTransform`, `iniFilePath`.

### SettingsHubCard shape

From `cpp/src/adapters/emulator_adapter.h:116-125`:

```cpp
struct SettingsHubCard {
    QString icon;         // Emoji glyph
    QString title;
    QString descriptor;
    QString categoryKey;  // Matches SettingDef::category for routing
    int row = 0;
    int col = 0;
    int rowSpan = 1;
    int colSpan = 1;
};
```

### Upstream PPSSPP libretro core options

Reference file: `/Users/mark/Documents/Projects/ppsspp-libretro/libretro/libretro_core_options.h`. Full list of ~66 options with their `default_value` and value-list `desc` strings is at lines 95-1167. This plan enumerates the 43 we expose; values come verbatim from that header.

### Categories NOT covered

- **Network** (23 options — WLAN, adhoc, UPnP, MAC/IP parts). Deferred. Future work.
- **Backend** (`ppsspp_backend`). Actually included in Video — see Task 4. Just calling out: yes the GPU backend selector is in this schema.

### Why `resolutionOptions()` and `aspectRatioOptions()` are NOT overridden

Per the spec: mgba and PCSX2 libretro adapters both return `{}` for these. The data shape (section/key/IniPatch) is designed for standalone adapters that write INI files; libretro core options use `Storage::LibretroOption` via `SettingDef`. PPSSPP follows the same pattern. `ppsspp_internal_resolution` lives in the Video card as a `SettingDef` — verified to land there in Task 4.

---

## File map

| Path | Action | Responsibility |
|---|---|---|
| `cpp/src/adapters/libretro/ppsspp_libretro_adapter.h` | MODIFY (+2 lines) | Declare `settingsSchema()` + `settingsHubCards()` overrides. |
| `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp` | MODIFY (+~270 LOC) | Implement the two methods with `opt()` / `optLabeled()` helper lambdas. |
| `cpp/tests/test_ppsspp_libretro_schema.cpp` | CREATE | 7-slot regression guard (count, keys, defaults, categories, storage). |
| `cpp/CMakeLists.txt` | MODIFY (+~30 LOC) | Register `test_ppsspp_libretro_schema` add_executable + add_test. Same source-list pattern as `test_ppsspp_libretro_bindings` from Phase A. |

---

## Tasks

### Task 1: Write the failing regression test

**Files:**
- Create: `cpp/tests/test_ppsspp_libretro_schema.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create the test file**

Write to `cpp/tests/test_ppsspp_libretro_schema.cpp`:

```cpp
// cpp/tests/test_ppsspp_libretro_schema.cpp
//
// Phase B+C regression guard for PpssppLibretroAdapter::settingsSchema()
// and settingsHubCards(). Asserts data-shape contracts that prevent
// silent breakage if upstream renames an option or the schema drifts.

#include <QtTest>
#include <QSet>
#include "adapters/libretro/ppsspp_libretro_adapter.h"
#include "core/setting_def.h"

class TestPpssppLibretroSchema : public QObject {
    Q_OBJECT
private:
    // The 43 unique upstream option keys this schema exposes.
    // Maintained in sync with libretro/libretro_core_options.h.
    // Changes here must be matched by the .cpp implementation.
    static QSet<QString> knownUpstreamKeys() {
        return {
            // System (11)
            "ppsspp_cpu_core", "ppsspp_fast_memory",
            "ppsspp_ignore_bad_memory_access", "ppsspp_io_timing_method",
            "ppsspp_force_lag_sync", "ppsspp_locked_cpu_speed",
            "ppsspp_memstick_inserted", "ppsspp_cache_iso",
            "ppsspp_cheats", "ppsspp_language", "ppsspp_psp_model",
            // Video (22)
            "ppsspp_backend", "ppsspp_software_rendering",
            "ppsspp_internal_resolution", "ppsspp_mulitsample_level",
            "ppsspp_cropto16x9", "ppsspp_frameskip", "ppsspp_frameskiptype",
            "ppsspp_auto_frameskip", "ppsspp_frame_duplication",
            "ppsspp_detect_vsync_swap_interval", "ppsspp_inflight_frames",
            "ppsspp_gpu_hardware_transform", "ppsspp_software_skinning",
            "ppsspp_hardware_tesselation", "ppsspp_texture_scaling_type",
            "ppsspp_texture_scaling_level", "ppsspp_texture_deposterize",
            "ppsspp_texture_shader", "ppsspp_texture_anisotropic_filtering",
            "ppsspp_texture_filtering", "ppsspp_smart_2d_texture_filtering",
            "ppsspp_texture_replacement",
            // Input (4)
            "ppsspp_button_preference", "ppsspp_analog_is_circular",
            "ppsspp_analog_deadzone", "ppsspp_analog_sensitivity",
            // Hacks (6)
            "ppsspp_skip_buffer_effects", "ppsspp_disable_range_culling",
            "ppsspp_skip_gpu_readbacks", "ppsspp_lazy_texture_caching",
            "ppsspp_spline_quality", "ppsspp_lower_resolution_for_effects",
        };
    }

private slots:
    void totalCount_matchesSpec() {
        PpssppLibretroAdapter a;
        // 10 Recommended duplicates + 43 per-category originals = 53.
        QCOMPARE(a.settingsSchema().size(), 53);
    }

    void everyKey_hasPpssppPrefix() {
        PpssppLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            QVERIFY2(d.key.startsWith("ppsspp_"),
                     qPrintable(QString("SettingDef key '%1' missing ppsspp_ prefix").arg(d.key)));
        }
    }

    void everyKey_isKnownUpstream() {
        PpssppLibretroAdapter a;
        const auto allowed = knownUpstreamKeys();
        for (const auto& d : a.settingsSchema()) {
            QVERIFY2(allowed.contains(d.key),
                     qPrintable(QString("SettingDef key '%1' is not in the known upstream allow-list "
                                        "(this catches stale or renamed options)").arg(d.key)));
        }
    }

    void everyDefault_isInOptions() {
        PpssppLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            QVERIFY2(d.type == SettingDef::Combo,
                     qPrintable(QString("SettingDef '%1' is not Combo type (this phase only ships Combos)").arg(d.key)));
            bool found = false;
            for (const auto& opt : d.options) {
                if (opt.second == d.defaultValue) { found = true; break; }
            }
            QVERIFY2(found,
                     qPrintable(QString("SettingDef '%1' defaultValue '%2' not present in its options list")
                                    .arg(d.key).arg(d.defaultValue)));
        }
    }

    void recommendedHasNaturalDupe() {
        PpssppLibretroAdapter a;
        const auto schema = a.settingsSchema();
        const QSet<QString> naturalCats{"System", "Video", "Input", "Hacks"};
        for (const auto& rec : schema) {
            if (rec.category != "Recommended") continue;
            bool foundDupe = false;
            for (const auto& nat : schema) {
                if (nat.key == rec.key && nat.category != "Recommended"
                    && naturalCats.contains(nat.category)) {
                    foundDupe = true;
                    break;
                }
            }
            QVERIFY2(foundDupe,
                     qPrintable(QString("Recommended setting '%1' has no matching entry "
                                        "under System/Video/Input/Hacks").arg(rec.key)));
        }
    }

    void hubCards_referencedByEntries() {
        PpssppLibretroAdapter a;
        const auto cards = a.settingsHubCards();
        const auto schema = a.settingsSchema();
        for (const auto& card : cards) {
            bool found = false;
            for (const auto& d : schema) {
                if (d.category == card.categoryKey) { found = true; break; }
            }
            QVERIFY2(found,
                     qPrintable(QString("Hub card '%1' (categoryKey='%2') has no matching SettingDef entries")
                                    .arg(card.title).arg(card.categoryKey)));
        }
    }

    void allEntries_useLibretroOption() {
        PpssppLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            QVERIFY2(d.storage == SettingDef::Storage::LibretroOption,
                     qPrintable(QString("SettingDef '%1' is not Storage::LibretroOption "
                                        "(Phase B/C must use libretro storage)").arg(d.key)));
        }
    }
};

QTEST_GUILESS_MAIN(TestPpssppLibretroSchema)
#include "test_ppsspp_libretro_schema.moc"
```

- [ ] **Step 2: Register the test target in CMakeLists.txt**

Locate the block from Phase A registering `test_ppsspp_libretro_bindings` (search for `add_test(NAME PpssppLibretroBindings COMMAND test_ppsspp_libretro_bindings)`). Insert the new block directly below it.

Same source-list pattern as `test_ppsspp_libretro_bindings` (libretro adapter sources + their transitive deps). Don't reduce the source list — it was carefully constructed in Phase A.

```cmake
add_executable(test_ppsspp_libretro_schema
    tests/test_ppsspp_libretro_schema.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/adapters/libretro/ppsspp_libretro_adapter.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
    src/core/libretro/video_hardware_gl.mm
    src/core/libretro/hotkey_matcher.cpp
    src/core/libretro/environment_callbacks.cpp
    src/core/libretro/video_software.cpp
    src/core/libretro/audio_sink.cpp
    src/core/libretro/input_router.cpp
    src/core/libretro/options_store.cpp
    src/core/libretro/frontend_settings_store.cpp
    src/core/libretro/rcheevos_runtime.cpp
    src/core/libretro/retro_log.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
    src/core/sdl_input_manager.cpp
    src/core/path_overrides_store.cpp
)
set_target_properties(test_ppsspp_libretro_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_ppsspp_libretro_schema PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(test_ppsspp_libretro_schema PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Test chdr-static rcheevos_static
    ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS} "-framework OpenGL" "-framework IOSurface"
)
add_test(NAME PpssppLibretroSchema COMMAND test_ppsspp_libretro_schema)
```

- [ ] **Step 3: Configure CMake**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake .
```

Expected: `Configuring done` / `Generating done` / `Build files have been written to`. Several Qt-related developer warnings are expected and harmless.

- [ ] **Step 4: Build the new test target**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake --build . --target test_ppsspp_libretro_schema -j8
```

Expected: build succeeds. Even though `settingsSchema()` / `settingsHubCards()` aren't overridden yet, the test compiles and links — the base class returns empty defaults and the assertions just fail at runtime.

- [ ] **Step 5: Run the test and confirm it fails as expected**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && ctest -R "^PpssppLibretroSchema$" --output-on-failure
```

Expected output contains:

```
FAIL!  : TestPpssppLibretroSchema::totalCount_matchesSpec() Compared values are not the same
   Actual   (a.settingsSchema().size()): 0
   Expected (53)                       : 53
```

(Other slots might also fail trivially because the schema is empty, e.g. `recommendedHasNaturalDupe` and `hubCards_referencedByEntries` may pass vacuously when there are no entries to check. That's fine — Tasks 2-4 make all of them pass meaningfully.) Don't commit yet.

---

### Task 2: Declare the overrides in the header

**Files:**
- Modify: `cpp/src/adapters/libretro/ppsspp_libretro_adapter.h`

- [ ] **Step 1: Add the two override declarations**

In `cpp/src/adapters/libretro/ppsspp_libretro_adapter.h`, find the Phase A `controllerBindingDefsForType` declaration. Insert the two new overrides directly below it. Final header block:

```cpp
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<PathDef> pathsDefs() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<SettingDef> settingsSchema() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;

    QString extractSerial(const QString& romPath) const override;
```

The header already includes `libretro_adapter.h` → `emulator_adapter.h` which declares both `SettingDef` and `SettingsHubCard`, so no extra `#include` is needed.

---

### Task 3: Implement `settingsHubCards()`

**Files:**
- Modify: `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`

- [ ] **Step 1: Add the hub cards function**

In `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`, append below `controllerBindingDefsForType` (and above `extractSerial`):

```cpp
QVector<SettingsHubCard> PpssppLibretroAdapter::settingsHubCards() const {
    // Layout: 3-column grid.
    //   Row 0: Recommended (full-width)
    //   Row 1: System | Video | Input
    //   Row 2: Hacks
    // categoryKey routes clicks to the matching SettingDef::category.
    return {
        { QStringLiteral("\U0001F4A1"), "Recommended",
          "Most-tweaked settings — resolution, performance, compat",
          "Recommended", 0, 0, 1, 3 },
        { QStringLiteral("\U0001F4BE"), "System",
          "CPU core, memory, PSP model, language",
          "System", 1, 0 },
        { QStringLiteral("\U0001F5BC"), "Video",
          "Resolution, MSAA, frameskip, texture scaling",
          "Video", 1, 1 },
        { QStringLiteral("\U0001F3AE"), "Input",
          "Confirm button, analog deadzone & sensitivity",
          "Input", 1, 2 },
        { QStringLiteral("\U000026A1"), "Hacks",
          "Speed hacks — skip buffer effects, disable culling, lazy textures",
          "Hacks", 2, 0 },
    };
}
```

Emoji glyphs (verify these match other adapters):
- `\U0001F4A1` = 💡 light bulb
- `\U0001F4BE` = 💾 floppy disk
- `\U0001F5BC` = 🖼 framed picture
- `\U0001F3AE` = 🎮 video game
- `\U000026A1` = ⚡ high voltage

- [ ] **Step 2: Rebuild and test**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake --build . --target test_ppsspp_libretro_schema -j8 && ctest -R "^PpssppLibretroSchema$" --output-on-failure
```

Expected: builds successfully. Test still fails on `totalCount_matchesSpec` (schema is still empty), but `hubCards_referencedByEntries` may now START failing because the cards exist but no SettingDef references them. That's the next task.

---

### Task 4: Implement `settingsSchema()`

**Files:**
- Modify: `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`

The biggest task — 53 `SettingDef` entries via two helper lambdas.

- [ ] **Step 1: Add the helper lambdas at the top of `settingsSchema()`**

Append below the new `settingsHubCards()` and above `extractSerial()`. Start with the function signature and the helpers:

```cpp
QVector<SettingDef> PpssppLibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    // Helper for upstream libretro options where the display label equals
    // the INI value. Most options are like this.
    auto opt = [](const QString& key, const QString& label,
                  const QString& def, const QStringList& vals,
                  const QString& category, const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.type = SettingDef::Combo;
        for (const auto& v : vals)
            d.options.append({ v, v });
        d.category = category;
        d.tooltip = tooltip;
        return d;
    };

    // Helper for options whose display label differs from the stored value
    // (e.g. ppsspp_internal_resolution shows "1x (480x272)" but stores "480x272").
    auto optLabeled = [](const QString& key, const QString& label,
                         const QString& def,
                         const QVector<QPair<QString, QString>>& vals,
                         const QString& category, const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.type = SettingDef::Combo;
        d.options = vals;
        d.category = category;
        d.tooltip = tooltip;
        return d;
    };

    // Rest of body added in subsequent steps...

    return s;
}
```

- [ ] **Step 2: Add the System category (11 entries)**

Insert immediately after the `optLabeled` lambda definition:

```cpp
    // === System (11) ===

    s << opt("ppsspp_cpu_core", "CPU Core",
             "JIT",
             { "JIT", "IR JIT", "Interpreter" },
             "System",
             "Specifies CPU emulator. JIT is fastest. IR JIT is more stable. Interpreter is the most accurate but slowest.");

    s << opt("ppsspp_fast_memory", "Fast Memory",
             "enabled",
             { "enabled", "disabled" },
             "System",
             "Faster memory access. Some games might require disabling.");

    s << opt("ppsspp_ignore_bad_memory_access", "Ignore Bad Memory Accesses",
             "enabled",
             { "enabled", "disabled" },
             "System",
             "Continue running after detecting an invalid memory access. May help some buggy games.");

    s << opt("ppsspp_io_timing_method", "I/O Timing Method",
             "Fast",
             { "Fast", "Host", "Simulate UMD delays" },
             "System",
             "Affects how PSP storage I/O latency is emulated. 'Fast' is fastest. 'Simulate UMD delays' is most accurate.");

    s << opt("ppsspp_force_lag_sync", "Force Real Clock Sync",
             "disabled",
             { "enabled", "disabled" },
             "System",
             "Slows down emulation to keep timing closer to a real PSP. May reduce stutter in some games at a performance cost.");

    s << opt("ppsspp_locked_cpu_speed", "Locked CPU Speed",
             "disabled",
             { "disabled",
               "222MHz", "232MHz", "244MHz", "266MHz", "288MHz", "300MHz",
               "333MHz", "366MHz", "388MHz", "400MHz", "433MHz", "466MHz",
               "488MHz", "500MHz", "533MHz", "555MHz", "576MHz", "600MHz",
               "633MHz", "666MHz", "688MHz", "700MHz", "733MHz", "750MHz",
               "766MHz", "788MHz", "800MHz", "833MHz", "866MHz", "888MHz",
               "900MHz", "933MHz", "966MHz", "999MHz" },
             "System",
             "Locks the emulated PSP CPU frequency. 'disabled' = dynamic per-game default (usually 222 or 333 MHz).");

    s << opt("ppsspp_memstick_inserted", "Memory Stick Inserted",
             "enabled",
             { "enabled", "disabled" },
             "System",
             "Simulates a Memory Stick being inserted into the PSP. Disable to test no-MS code paths in homebrew.");

    s << opt("ppsspp_cache_iso", "Cache Full ISO in RAM",
             "disabled",
             { "enabled", "disabled" },
             "System",
             "Reads the whole ISO into RAM at boot. Eliminates disc-read stutter, but uses memory proportional to ISO size.");

    s << opt("ppsspp_cheats", "Internal Cheats Support",
             "disabled",
             { "enabled", "disabled" },
             "System",
             "Enable PPSSPP's built-in cheat engine (looks for a per-game .ini in the cheats folder).");

    s << opt("ppsspp_language", "Game Language",
             "Automatic",
             { "Automatic", "English", "Japanese", "French", "Spanish",
               "German", "Italian", "Dutch", "Portuguese", "Russian",
               "Korean", "Chinese Traditional", "Chinese Simplified" },
             "System",
             "Forces the PSP system language. 'Automatic' uses the host locale. Affects games that read PSP system language.");

    s << opt("ppsspp_psp_model", "PSP Model",
             "psp_2000_3000",
             { "psp_1000", "psp_2000_3000" },
             "System",
             "Emulates a PSP-1000 (32MB RAM) or PSP-2000/3000 (64MB RAM). A few games behave differently.");
```

- [ ] **Step 3: Add the Video category (22 entries)**

Append immediately after the System block:

```cpp
    // === Video (22) ===

    s << opt("ppsspp_backend", "Backend",
             "auto",
             { "auto", "opengl", "vulkan", "none" },
             "Video",
             "GPU rendering backend. 'auto' picks the best one for the platform. 'none' disables rendering entirely.");

    s << opt("ppsspp_software_rendering", "Software Rendering",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Use the slow but maximum-accuracy software renderer instead of the GPU backend. Diagnostic / accuracy testing.");

    s << optLabeled("ppsspp_internal_resolution", "Rendering Resolution",
                    "480x272",
                    { { "1x (480x272)",     "480x272" },
                      { "2x (960x544)",     "960x544" },
                      { "3x (1440x816)",    "1440x816" },
                      { "4x (1920x1088)",   "1920x1088" },
                      { "5x (2400x1360)",   "2400x1360" },
                      { "6x (2880x1632)",   "2880x1632" },
                      { "7x (3360x1904)",   "3360x1904" },
                      { "8x (3840x2176)",   "3840x2176" },
                      { "9x (4320x2448)",   "4320x2448" },
                      { "10x (4800x2720)",  "4800x2720" } },
                    "Video",
                    "Internal render resolution. Higher = sharper but costs GPU. 1x is native PSP (480x272).");

    s << opt("ppsspp_mulitsample_level", "MSAA Antialiasing",
             "Disabled",
             { "Disabled", "x2", "x4", "x8" },
             "Video",
             "Multisample antialiasing. Vulkan backend only — ignored on OpenGL. Higher = smoother edges, more GPU.");

    s << opt("ppsspp_cropto16x9", "Crop to 16x9",
             "enabled",
             { "enabled", "disabled" },
             "Video",
             "Crops 1 pixel from the top and bottom of the 480x272 frame, yielding exactly 16:9. Eliminates ~1px black bar.");

    s << opt("ppsspp_frameskip", "Frameskip",
             "disabled",
             { "disabled", "1", "2", "3", "4", "5", "6", "7", "8" },
             "Video",
             "Skip rendering of N intermediate frames to maintain audio. 'disabled' = render every frame.");

    s << opt("ppsspp_frameskiptype", "Frameskip Type",
             "Number of frames",
             { "Number of frames", "Percent of FPS" },
             "Video",
             "Whether the Frameskip value is interpreted as a count of frames to skip, or as a percentage of the target FPS.");

    s << opt("ppsspp_auto_frameskip", "Auto Frameskip",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Dynamically frameskip when emulation can't keep up. Overrides manual Frameskip when enabled.");

    s << opt("ppsspp_frame_duplication", "Render Duplicate Frames to 60 Hz",
             "enabled",
             { "enabled", "disabled" },
             "Video",
             "Many PSP games target 30 FPS internally. With this enabled, each frame is rendered twice to match 60 Hz displays smoothly.");

    s << opt("ppsspp_detect_vsync_swap_interval", "Detect Frame Rate Changes",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Detect when a game changes its target frame rate and adjust accordingly. Most users want this off.");

    s << opt("ppsspp_inflight_frames", "Buffer Graphics Commands",
             "Up to 2",
             { "No buffer", "Up to 1", "Up to 2" },
             "Video",
             "How many frames of GPU work may be in-flight. Higher = smoother but more latency. 'No buffer' = lowest latency, may stutter.");

    s << opt("ppsspp_gpu_hardware_transform", "Hardware Transform",
             "enabled",
             { "enabled", "disabled" },
             "Video",
             "Performs vertex transforms on the GPU instead of the CPU. Faster on most hardware. Disable for diagnostics.");

    s << opt("ppsspp_software_skinning", "Software Skinning",
             "enabled",
             { "enabled", "disabled" },
             "Video",
             "Use CPU for character-model bone skinning. Often faster than GPU skinning on weaker GPUs.");

    s << opt("ppsspp_hardware_tesselation", "Hardware Tesselation",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Use GPU tessellation for spline/bezier curves where available. Faster but not universally supported.");

    s << opt("ppsspp_texture_scaling_type", "Texture Upscale Type",
             "xbrz",
             { "xbrz", "hybrid", "bicubic", "hybrid_bicubic" },
             "Video",
             "Algorithm used for texture upscaling when Texture Upscaling Level > disabled.");

    s << opt("ppsspp_texture_scaling_level", "Texture Upscaling Level",
             "disabled",
             { "disabled", "2x", "3x", "4x", "5x" },
             "Video",
             "Upscale textures via xBRZ-family filters. Improves 2D art at significant GPU cost. 'disabled' = original PSP textures.");

    s << opt("ppsspp_texture_deposterize", "Texture Deposterize",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Smooth-out the banding that can appear in upscaled textures. Only relevant when Texture Upscaling Level > disabled.");

    s << opt("ppsspp_texture_shader", "Texture Shader",
             "disabled",
             { "disabled", "2xBRZ", "4xBRZ", "MMPX" },
             "Video",
             "GPU-side texture upscaling shader. Lighter than CPU upscaling but with fewer algorithms.");

    s << opt("ppsspp_texture_anisotropic_filtering", "Anisotropic Filtering",
             "16x",
             { "disabled", "2x", "4x", "8x", "16x" },
             "Video",
             "Improves the sharpness of textures viewed at oblique angles. Cheap on modern GPUs.");

    s << opt("ppsspp_texture_filtering", "Texture Filtering",
             "Auto",
             { "Auto", "Nearest", "Linear", "Auto max quality" },
             "Video",
             "How textures are sampled when scaled. 'Linear' = smooth. 'Nearest' = pixelated (good for retro art).");

    s << opt("ppsspp_smart_2d_texture_filtering", "Smart 2D Texture Filtering",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Adapt the texture filter per-texture to preserve sharp 2D art while smoothing 3D models.");

    s << opt("ppsspp_texture_replacement", "Texture Replacement",
             "disabled",
             { "enabled", "disabled" },
             "Video",
             "Load community texture packs from the texture replacement folder, if present for the running game.");
```

- [ ] **Step 4: Add the Input category (4 entries)**

Append:

```cpp
    // === Input (4) ===

    s << opt("ppsspp_button_preference", "Confirmation Button",
             "Cross",
             { "Cross", "Circle" },
             "Input",
             "Which face button is 'confirm' in PSP system menus. 'Cross' (US/EU) or 'Circle' (Japan).");

    s << opt("ppsspp_analog_is_circular", "Analog Circle vs Square Gate Compensation",
             "disabled",
             { "enabled", "disabled" },
             "Input",
             "Compensate when your gamepad's analog stick has a square gate (most do) versus the PSP's circular nub.");

    s << opt("ppsspp_analog_deadzone", "Analog Deadzone",
             "0.0",
             { "0.0", "0.05", "0.1", "0.15", "0.2", "0.25", "0.3",
               "0.35", "0.4", "0.45", "0.5" },
             "Input",
             "Ignore stick deflection below this magnitude (0.0–0.5 = 0%–50%). Higher = less drift, less precision.");

    s << opt("ppsspp_analog_sensitivity", "Analog Axis Scale",
             "1.00",
             { "1.00", "1.01", "1.02", "1.03", "1.04", "1.05", "1.06", "1.07", "1.08", "1.09", "1.10",
               "1.11", "1.12", "1.13", "1.14", "1.15", "1.16", "1.17", "1.18", "1.19", "1.20",
               "1.21", "1.22", "1.23", "1.24", "1.25", "1.26", "1.27", "1.28", "1.29", "1.30",
               "1.31", "1.32", "1.33", "1.34", "1.35", "1.36", "1.37", "1.38", "1.39", "1.40",
               "1.41", "1.42", "1.43", "1.44", "1.45", "1.46", "1.47", "1.48", "1.49", "1.50" },
             "Input",
             "Scale analog stick output (1.00 = 100%, 1.50 = 150%). Useful when gamepad sticks underrun the PSP's max range.");
```

- [ ] **Step 5: Add the Hacks category (6 entries)**

Append:

```cpp
    // === Hacks (6) ===

    s << opt("ppsspp_skip_buffer_effects", "Skip Buffer Effects",
             "disabled",
             { "enabled", "disabled" },
             "Hacks",
             "Skip render-to-texture effects (post-processing, advanced lighting). Big speedup but breaks some games visually.");

    s << opt("ppsspp_disable_range_culling", "Disable Culling",
             "disabled",
             { "enabled", "disabled" },
             "Hacks",
             "Disable distance/range culling. Fixes visual glitches in some games at a small performance cost.");

    s << opt("ppsspp_skip_gpu_readbacks", "Skip GPU Readbacks",
             "disabled",
             { "enabled", "disabled" },
             "Hacks",
             "Skip reading the GPU's framebuffer back to CPU. Big speedup; breaks games that rely on framebuffer feedback.");

    s << opt("ppsspp_lazy_texture_caching", "Lazy Texture Caching (Speedup)",
             "disabled",
             { "enabled", "disabled" },
             "Hacks",
             "Cache textures more aggressively. Faster, but some games that mutate textures rapidly may show stale data.");

    s << opt("ppsspp_spline_quality", "Spline/Bezier Curves Quality",
             "High",
             { "Low", "Medium", "High" },
             "Hacks",
             "Quality of spline/bezier curve tessellation. 'High' = smoothest curves, more GPU work.");

    s << opt("ppsspp_lower_resolution_for_effects", "Lower Resolution for Effects",
             "disabled",
             { "disabled", "Safe", "Balanced", "Aggressive" },
             "Hacks",
             "Render certain framebuffer effects at lower internal resolution to save GPU. 'Safe' is the most conservative.");
```

- [ ] **Step 6: Add the Recommended duplicates (10 entries)**

Append immediately above the closing `return s;` of `settingsSchema()`. These are exact duplicates of 10 entries from the categories above, but with `category = "Recommended"`. Both rows write to the same libretro option because they share the same `key`.

```cpp
    // === Recommended (10 — duplicates of the most-tweaked entries above,
    //                       same keys → same backing OptionsStore values) ===

    s << opt("ppsspp_cpu_core", "CPU Core",
             "JIT",
             { "JIT", "IR JIT", "Interpreter" },
             "Recommended",
             "Specifies CPU emulator. JIT is fastest. IR JIT is more stable. Interpreter is the most accurate but slowest.");

    s << opt("ppsspp_psp_model", "PSP Model",
             "psp_2000_3000",
             { "psp_1000", "psp_2000_3000" },
             "Recommended",
             "Emulates a PSP-1000 (32MB RAM) or PSP-2000/3000 (64MB RAM). A few games behave differently.");

    s << optLabeled("ppsspp_internal_resolution", "Rendering Resolution",
                    "480x272",
                    { { "1x (480x272)",     "480x272" },
                      { "2x (960x544)",     "960x544" },
                      { "3x (1440x816)",    "1440x816" },
                      { "4x (1920x1088)",   "1920x1088" },
                      { "5x (2400x1360)",   "2400x1360" },
                      { "6x (2880x1632)",   "2880x1632" },
                      { "7x (3360x1904)",   "3360x1904" },
                      { "8x (3840x2176)",   "3840x2176" },
                      { "9x (4320x2448)",   "4320x2448" },
                      { "10x (4800x2720)",  "4800x2720" } },
                    "Recommended",
                    "Internal render resolution. Higher = sharper but costs GPU. 1x is native PSP (480x272).");

    s << opt("ppsspp_mulitsample_level", "MSAA Antialiasing",
             "Disabled",
             { "Disabled", "x2", "x4", "x8" },
             "Recommended",
             "Multisample antialiasing. Vulkan backend only — ignored on OpenGL. Higher = smoother edges, more GPU.");

    s << opt("ppsspp_cropto16x9", "Crop to 16x9",
             "enabled",
             { "enabled", "disabled" },
             "Recommended",
             "Crops 1 pixel from the top and bottom of the 480x272 frame, yielding exactly 16:9. Eliminates ~1px black bar.");

    s << opt("ppsspp_frameskip", "Frameskip",
             "disabled",
             { "disabled", "1", "2", "3", "4", "5", "6", "7", "8" },
             "Recommended",
             "Skip rendering of N intermediate frames to maintain audio. 'disabled' = render every frame.");

    s << opt("ppsspp_auto_frameskip", "Auto Frameskip",
             "disabled",
             { "enabled", "disabled" },
             "Recommended",
             "Dynamically frameskip when emulation can't keep up. Overrides manual Frameskip when enabled.");

    s << opt("ppsspp_texture_scaling_level", "Texture Upscaling Level",
             "disabled",
             { "disabled", "2x", "3x", "4x", "5x" },
             "Recommended",
             "Upscale textures via xBRZ-family filters. Improves 2D art at significant GPU cost. 'disabled' = original PSP textures.");

    s << opt("ppsspp_texture_anisotropic_filtering", "Anisotropic Filtering",
             "16x",
             { "disabled", "2x", "4x", "8x", "16x" },
             "Recommended",
             "Improves the sharpness of textures viewed at oblique angles. Cheap on modern GPUs.");

    s << opt("ppsspp_skip_buffer_effects", "Skip Buffer Effects",
             "disabled",
             { "enabled", "disabled" },
             "Recommended",
             "Skip render-to-texture effects (post-processing, advanced lighting). Big speedup but breaks some games visually.");
```

- [ ] **Step 7: Rebuild the test target**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake --build . --target test_ppsspp_libretro_schema -j8
```

Expected: builds successfully.

- [ ] **Step 8: Run the schema test and confirm all 7 slots pass**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && ctest -R "^PpssppLibretroSchema$" --output-on-failure
```

Expected:

```
1/1 Test #N: PpssppLibretroSchema ........ Passed
100% tests passed, 0 tests failed out of 1
```

If any slot fails:
- `totalCount`: count the `s << ...` lines you added. Must be exactly 53.
- `everyDefault_isInOptions`: a `defaultValue` doesn't appear in the matching `options` list — usually a typo against upstream's `default_value`.
- `everyKey_isKnownUpstream`: a key you wrote doesn't match the allow-list in the test — typo in the key.
- `recommendedHasNaturalDupe`: a Recommended entry's key doesn't appear under any of System/Video/Input/Hacks — you forgot to add the natural-category twin.
- `hubCards_referencedByEntries`: probably the hub card's `categoryKey` is misspelled vs the matching `SettingDef.category`.

---

### Task 5: Run the full test suite to confirm no regression

- [ ] **Step 1: Full build**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && cmake --build . -j8
```

Expected: build succeeds. RetroNest.app gets relinked.

- [ ] **Step 2: Run all tests**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project/cpp/build && ctest -j4
```

Expected: 43 of 44 tests pass. The single failure is the pre-existing `HotkeyDefs::duckstation_completeness` (actual 99 vs expected 102) — drift from commit `54964c4`, untouched by Phase B. The new `PpssppLibretroSchema` and the Phase A `PpssppLibretroBindings` are both in the pass list.

---

### Task 6: Manual runtime smoke test

This verifies the Settings → PSP page actually renders the 5 cards and reads/writes options correctly. This step requires a UI session — if you're a fully autonomous executor, hand off to the human here.

- [ ] **Step 1: Launch RetroNest**

```bash
~/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest
```

- [ ] **Step 2: Navigate to Settings → PSP**

From the home screen: Settings → PPSSPP (or PSP). Previously this triggered `[AppController] showEmulatorSettings: adapter for "ppsspp" exposes no settings hub cards — skipping`. Now it should open a hub page rendering 5 cards in the expected layout (Recommended on row 0 spanning the width, System/Video/Input on row 1, Hacks on row 2).

- [ ] **Step 3: Open each card and verify rows**

Click each card and verify the expected row counts:
- Recommended: 10 rows
- System: 11 rows
- Video: 22 rows
- Input: 4 rows
- Hacks: 6 rows

Each row shows a label + dropdown with the option values. Tooltips appear on hover (where the UI exposes them).

- [ ] **Step 4: Test a value change persists**

In Settings → PSP → Video, change `Rendering Resolution` from `1x (480x272)` to `2x (960x544)`. Close the settings dialog. Re-open Settings → PSP → Video. Confirm the dropdown still shows `2x (960x544)` (i.e. the value persisted via OptionsStore).

- [ ] **Step 5: Test the Recommended-card shortcut works for the same key**

In Settings → PSP → Recommended, the same `Rendering Resolution` row should show `2x (960x544)` (the value set in step 4 via the Video card). Change it here to `3x (1440x816)`. Close and re-open. Both the Recommended card AND the Video card should now show `3x (1440x816)`. Confirms the duplication pattern routes both rows to the same OptionsStore key.

- [ ] **Step 6: Launch a PSP game with the new resolution**

Launch any PSP ROM (DBZ - Shin Budokai is known-working from Phase A). Confirm the rendered output is sharper than at 1x (you should see ~3x sharper textures and edges).

Exit the game cleanly via the in-game menu.

---

### Task 7: Commit

- [ ] **Step 1: Stage and commit**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project && git add \
    cpp/src/adapters/libretro/ppsspp_libretro_adapter.h \
    cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp \
    cpp/tests/test_ppsspp_libretro_schema.cpp \
    cpp/CMakeLists.txt && \
git commit -m "$(cat <<'EOF'
feat(ppsspp): settings schema + hub cards (Phase B+C)

Override settingsSchema() and settingsHubCards() on PpssppLibretroAdapter.
The Settings -> PSP page now renders 5 categorised cards covering 43
upstream PPSSPP libretro core options — the AppController early-return
("exposes no settings hub cards — skipping") path is no longer hit.

Layout (mirrors mgba_libretro_adapter.cpp:385-412):
  Row 0: Recommended (full-width — 10 most-tweaked options)
  Row 1: System (11) | Video (22) | Input (4)
  Row 2: Hacks (6)

Total SettingDef count = 53 (10 Recommended duplicates + 43 natural
entries — both rows mutate the same libretro option via shared key).

All entries use Storage::LibretroOption. Phase C absorbed: per the
mgba/PCSX2 pattern, resolutionOptions()/aspectRatioOptions() stay at
the base-class {} return — ppsspp_internal_resolution lives in the
Video card as a SettingDef.

Out of scope (deferred): Network options (23), BIOS files (Phase D),
frontendSettingDefaults (Phase E), settings audit (Phase F),
PPSSPP-specific hotkeys (Phase G).

Regression guard test_ppsspp_libretro_schema asserts:
- total count = 53
- every key has ppsspp_ prefix
- every key is in the known-upstream allow-list of 43 keys
- every defaultValue is in its options list
- every Recommended entry has a matching System/Video/Input/Hacks dupe
- every hub card categoryKey is referenced by >=1 SettingDef
- every entry uses Storage::LibretroOption

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 2: Verify commit**

Run:

```bash
cd ~/Documents/Projects/RetroNest-Project && git log --oneline -3
```

Expected: top commit is `feat(ppsspp): settings schema + hub cards (Phase B+C)`.

---

## Done criteria

All five checkboxes:

- [ ] `test_ppsspp_libretro_schema` passes (all 7 slots)
- [ ] Full ctest stays at 43/44 pass (the 1 pre-existing `HotkeyDefs` failure is untouched)
- [ ] Settings → PSP page renders 5 cards in the expected grid layout
- [ ] Value changes persist across dialog re-opens, and Recommended ↔ natural-category dupes mutate the same backing store
- [ ] Launching a PSP game at 2x or higher resolution visibly increases image sharpness

One commit lands on `main`: `feat(ppsspp): settings schema + hub cards (Phase B+C)`.

---

## Out of scope (post-B+C roadmap)

- **Phase D** — `biosFiles()` for compat.ini, flash0 fonts, ppge_atlas.zim, lang/*.ini, adhoc-servers.json.
- **Phase E** — `frontendSettingDefaults()` — RetroNest-side overrides like aspect_mode, integer_scale; may also override `ppsspp_internal_resolution` default from 1x to 2x for better OOB experience.
- **Phase F** — Settings audit pass — write a doc mirroring `docs/superpowers/audits/2026-04-07-ppsspp-settings-audit.md` against this new schema.
- **Phase G** — PPSSPP-specific hotkey overrides on top of the shared 22 in `libretro_hotkey_defs.cpp` (probably skip — shared set should be sufficient).
- **Network options** — 23 deferred. If shipped, would likely be a single Network card with the 3 main toggles (WLAN, adhoc server, UPnP), with the MAC / IP entry surfaces handled by a different widget (text field with split-on-display) rather than a 12-row combobox stack.
- **Slider rendering for analog deadzone / sensitivity** — currently both render as Combos. The 51-value sensitivity dropdown is awkward. If we revisit, add `layout = "slider"` and verify the renderer supports slider-from-Combo or migrate them to `Type::Float` + a value formatter that emits the upstream-compatible "1.23" string.
- **MSAA gating** — `ppsspp_mulitsample_level` is Vulkan-only; OpenGL silently ignores it. A future polish pass could add `dependsOn = "ppsspp_backend != opengl"` to hide it when the backend is OpenGL.
