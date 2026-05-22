# PPSSPP Libretro — OSD Upstream Patch + Adapter Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add 4 `ppsspp_osd_*` libretro core options to the ppsspp-libretro fork that wrap PPSSPP's existing `iShowStatusFlags` + `iDebugOverlay` surface, rebuild + install the universal dylib, then expose a matching 6th "OSD" hub card on the PPSSPP Settings page in RetroNest.

**Architecture:** Two-repo, two-commit chain. Commit A (ppsspp-libretro) extends `libretro_core_options.h` + `libretro.cpp` so the dylib reports OSD options to any libretro frontend, then rebuilds the universal dylib. Commit B (RetroNest-Project) extends the PPSSPP adapter with the 6th hub card + 4 SettingDef rows + a `previewSpec("OSD","")` binding, and adds a `showBattery` Q_PROPERTY to the shared `OsdPreview` widget so the preview pane reflects the battery toggle.

**Tech Stack:** C++17, CMake / Make (PPSSPP libretro build), Qt 6.11, libretro core API v2.

**Spec doc:** `docs/superpowers/specs/2026-05-22-ppsspp-osd-upstream-patch-design.md` (commits `c41e24a` + `6188240` on main).

**Repo state before starting:**
- `ppsspp-libretro`: branch `main` at `872e25617a`. Working tree clean.
- `RetroNest-Project`: branch `main` at `6188240` (the spec-correction commit). Working tree clean.

---

## Background facts (cached so the executor doesn't have to re-research)

### PPSSPP's overlay surface (the ground truth being wrapped)

`Core/ConfigValues.h:210-214`:
```cpp
enum class ShowStatusFlags {
    FPS_COUNTER     = 1 << 1,   // = 2
    SPEED_COUNTER   = 1 << 2,   // = 4
    BATTERY_PERCENT = 1 << 3,   // = 8
};
```
Packed into `g_Config.iShowStatusFlags` (int). Render gate is `UI/EmuScreen.cpp:1979` — `if (g_Config.iShowStatusFlags) DrawFPS(...)`.

`Core/ConfigValues.h:253-266`:
```cpp
enum class DebugOverlay : int {
    OFF,                  // 0
    DEBUG_STATS,          // 1
    FRAME_GRAPH,          // 2
    FRAME_TIMING,         // 3
#ifdef USE_PROFILER
    FRAME_PROFILE,
#endif
    CONTROL,              // 4 (USE_PROFILER undefined — verified Task 1)
    Audio,                // 5
    GPU_PROFILE,          // 6
    GPU_ALLOCATOR,        // 7
    FRAMEBUFFER_LIST,     // 8
};
```
`USE_PROFILER` is **never defined** in any build (commented in `Common/Profiler/Profiler.h:5`; no CMake / Makefile flag sets it). Label-index == enum-value, no mapping table needed.

### Where in `libretro.cpp` to insert the new check_variable blocks

After the existing `ppsspp_psp_model` block at `libretro/libretro.cpp:615-622`, just before the `ppsspp_button_preference` block at `:624`. (Line numbers will shift; the contextual anchor is "right after `ppsspp_psp_model` and right before `ppsspp_button_preference`.")

### Where in `libretro_core_options.h` to insert

- **Category list** (`option_cats_us[]`): append `{"osd", "OSD", "Configure on-screen overlays."}` immediately before the `network` entry at line 117 (the `network` entry is the final concrete entry before the `{NULL,NULL,NULL}` sentinel).
- **Option defs** (`option_defs_us[]`): append the 4 new entries near the end of the array, immediately before the `{NULL, ...}` sentinel that closes `option_defs_us`. The exact line number will be high (~1100s); locate by grepping for the closing sentinel `{ NULL, {{0}}, NULL }` or the last `ppsspp_` entry.

### `retro_core_option_v2_definition` shape (from existing entries)

```cpp
{
   "ppsspp_<key>",         // key
   "<Visible label>",      // desc
   NULL,                   // desc_categorized (NULL = use desc)
   "<Tooltip>",            // info
   NULL,                   // info_categorized (NULL = use info)
   "<category-key>",       // category_key (matches option_cats_us)
   {
      { "<value>", "<label-or-NULL>" },
      // ...
      { NULL, NULL },
   },
   "<default-value>"
}
```

### Installed dylib path (target of step 4)

`/Users/mark/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib`
(with `.universal-backup` sibling from a prior backup — leave that alone)

### OsdPreview existing state struct (extend, don't restructure)

The widget holds a single `struct State m_s;` (private) with bool fields like `fps`, `speed`, `videoCapture`, `inputRec`, etc. Q_PROPERTYs are paired with bare getters/setters. The battery field will be added as `m_s.battery` to fit the existing pattern.

`drawTopRightIndicators` (`osd_preview.cpp:281-301`) is the natural home for the battery glyph — it already pushes `m_s.indicators` / `videoCapture` / `inputRec` / `textureReplacements` strings into a vertical list anchored top-right.

### Adapter integration anchors

`ppsspp_libretro_adapter.cpp`:
- `settingsHubCards()` is at `:67` (added in commit `940cfac`) — append the new 6th card before the closing `}`.
- `settingsSchema()` is at `:97` — append a new `=== OSD (4) ===` block right before the `Recommended` block (so natural categories cluster together).
- `previewSpec()` is at `:79` — add an `if (category == "OSD" ...)` branch inside the existing method.

---

## File map

| Path | Repo | Action | Responsibility |
|---|---|---|---|
| `libretro/libretro_core_options.h` | ppsspp-libretro | MODIFY (+~80 LOC) | Add `"osd"` category + 4 `retro_core_option_v2_definition` entries. |
| `libretro/libretro.cpp` | ppsspp-libretro | MODIFY (+~35 LOC) | Add 4 `check_variable` blocks in `check_variables()`. |
| `cpp/src/ui/settings/widgets/preview/osd_preview.h` | RetroNest | MODIFY (+5 LOC) | Add `showBattery` Q_PROPERTY + getter + setter declaration. |
| `cpp/src/ui/settings/widgets/preview/osd_preview.cpp` | RetroNest | MODIFY (+~15 LOC) | `setShowBattery` impl + add battery line to `drawTopRightIndicators`. |
| `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp` | RetroNest | MODIFY (+~55 LOC) | 6th hub card, 4 OSD SettingDefs, `previewSpec("OSD","")` branch. |
| `cpp/tests/test_ppsspp_libretro_schema.cpp` | RetroNest | MODIFY (+~30 LOC) | Count 55→59, +4 allow-list keys, new `previewSpec_osd_isOsd` slot. |

No header changes in the PPSSPP adapter (`ppsspp_libretro_adapter.h`) — `settingsSchema`, `settingsHubCards`, `previewSpec` are already declared.

No CMake changes — both repos already build the modified files.

---

## Tasks

### Task 1: Verify USE_PROFILER + clean libretro build

**Files:** none modified.

This is a defensive verification before any code change. Catches the case where USE_PROFILER got defined somewhere new since the spec, or where the libretro build pipeline is broken on the current branch.

- [ ] **Step 1: Re-verify USE_PROFILER is never defined**

```bash
grep -rn "define USE_PROFILER\|-DUSE_PROFILER" /Users/mark/Documents/Projects/ppsspp-libretro \
  --include="*.cmake" --include="CMakeLists.txt" --include="Makefile*" --include="*.h" --include="*.cpp" \
  2>/dev/null | grep -v "^//\|// " | head
```

Expected: only one result — `Common/Profiler/Profiler.h:5: // #define USE_PROFILER` (commented). Any uncommented `#define USE_PROFILER` or `-DUSE_PROFILER` invalidates the index-mapping assumption — STOP and update the plan if found.

- [ ] **Step 2: Locate the libretro build directory and dump the previous build flags**

```bash
ls /Users/mark/Documents/Projects/ppsspp-libretro/build-libretro 2>/dev/null \
  || ls /Users/mark/Documents/Projects/ppsspp-libretro/build 2>/dev/null \
  || echo "no existing build dir"
```

If a build dir exists, note its path (will reuse in Task 4). If not, you'll create one fresh.

- [ ] **Step 3: Configure a fresh libretro build to verify the pipeline works**

```bash
cd /Users/mark/Documents/Projects/ppsspp-libretro && \
  rm -rf build-libretro && mkdir build-libretro && cd build-libretro && \
  cmake -DLIBRETRO=ON -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -G Ninja .. 2>&1 | tail -10
```

Expected: `Configuring done` / `Generating done`. If cmake fails with a missing dependency, FIX root cause — do NOT add a workaround.

- [ ] **Step 4: Build the dylib (baseline — no source changes yet)**

```bash
cd /Users/mark/Documents/Projects/ppsspp-libretro/build-libretro && \
  ninja ppsspp_libretro 2>&1 | tail -15
```

Expected: builds successfully, produces `lib/ppsspp_libretro.dylib` (or `ppsspp_libretro.dylib` at the build root, depending on CMake target). First clean build may take 10-30 minutes.

- [ ] **Step 5: Verify the dylib is universal**

```bash
DYLIB=$(find /Users/mark/Documents/Projects/ppsspp-libretro/build-libretro -name "ppsspp_libretro.dylib" -type f | head -1) && \
  echo "Path: $DYLIB" && \
  lipo -info "$DYLIB"
```

Expected: `Architectures in the fat file: $DYLIB are: x86_64 arm64`. Note the path — used in Task 4.

- [ ] **Step 6: Verify the dylib loads & exports the libretro entrypoints**

```bash
nm -gU "$DYLIB" | grep "_retro_" | head -10
```

Expected: at least `_retro_init`, `_retro_run`, `_retro_get_system_info`, `_retro_load_game`, `_retro_set_environment`. Empty output means the dylib is broken — STOP.

No commit at end of Task 1 — verification only.

---

### Task 2: Add the `osd` category and 4 option defs to `libretro_core_options.h`

**Files:**
- Modify: `/Users/mark/Documents/Projects/ppsspp-libretro/libretro/libretro_core_options.h`

- [ ] **Step 1: Add the `osd` category to `option_cats_us[]`**

Locate the existing `network` category block (search for `"Configure network options."`). Insert the new `osd` block immediately BEFORE it.

Edit replaces:
```c
   {
      "network",
      "Network",
      "Configure network options."
   },
   { NULL, NULL, NULL },
};
```

With:
```c
   {
      "osd",
      "OSD",
      "Configure on-screen overlays (FPS, speed, battery, debug)."
   },
   {
      "network",
      "Network",
      "Configure network options."
   },
   { NULL, NULL, NULL },
};
```

- [ ] **Step 2: Locate the closing sentinel of `option_defs_us[]`**

```bash
grep -n "^};\|^\s*{ NULL, {{0}}, NULL }" /Users/mark/Documents/Projects/ppsspp-libretro/libretro/libretro_core_options.h | head -10
```

Find the entry that's the `option_defs_us[]` sentinel — it's `{ NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL }` (8 NULLs to match the `retro_core_option_v2_definition` field count) followed by `};`. Note the line number.

- [ ] **Step 3: Insert the 4 OSD option_def entries immediately before the sentinel**

Insert (preserving the existing 3-space indentation used by this file):

```c
   {
      "ppsspp_osd_show_fps",
      "Show FPS Counter",
      NULL,
      "Render the current frames-per-second in the corner of the screen.",
      NULL,
      "osd",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "ppsspp_osd_show_speed",
      "Show Speed Percentage",
      NULL,
      "Render the current emulation speed (% of real PSP) in the corner of the screen.",
      NULL,
      "osd",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "ppsspp_osd_show_battery",
      "Show Battery Percentage",
      NULL,
      "Render the host battery percentage in the corner of the screen. Note: under libretro this typically reports 100% because the core has no host battery hook.",
      NULL,
      "osd",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "ppsspp_osd_debug_overlay",
      "Debug Overlay",
      NULL,
      "Render a full-screen debug overlay. Exactly one overlay may be active at a time.",
      NULL,
      "osd",
      {
         { "Off",                   NULL },
         { "Debug stats",           NULL },
         { "Frame timings graph",   NULL },
         { "Frame timing",          NULL },
         { "Control debug",         NULL },
         { "Audio debug",           NULL },
         { "GPU profile",           NULL },
         { "GPU allocator viewer",  NULL },
         { "Framebuffer list",      NULL },
         { NULL, NULL },
      },
      "Off"
   },
```

- [ ] **Step 4: Confirm the header still compiles standalone**

```bash
cd /Users/mark/Documents/Projects/ppsspp-libretro && \
  clang++ -fsyntax-only -I libretro -I . libretro/libretro_core_options.h 2>&1 | head -20
```

Expected: silence (no errors) — or, if clang complains about a missing C-only `extern` declaration, instead build the libretro target as a real link test (Step 5).

- [ ] **Step 5: Rebuild the dylib incrementally**

```bash
cd /Users/mark/Documents/Projects/ppsspp-libretro/build-libretro && \
  ninja ppsspp_libretro 2>&1 | tail -10
```

Expected: builds successfully. Only `libretro_core_options.h` consumers re-compile (a few translation units). Errors here are usually a mismatched brace count in your insertion — re-read your changes and recount.

No commit yet — Tasks 2-4 land as one ppsspp-libretro commit at the end of Task 4.

---

### Task 3: Wire the 4 options in `libretro.cpp` `check_variables()`

**Files:**
- Modify: `/Users/mark/Documents/Projects/ppsspp-libretro/libretro/libretro.cpp`

- [ ] **Step 1: Locate the insertion point**

Find the `ppsspp_psp_model` block:

```bash
grep -nA8 "var.key = \"ppsspp_psp_model\"" /Users/mark/Documents/Projects/ppsspp-libretro/libretro/libretro.cpp
```

Note the line number where the `}` closing the `if (var.value)` block lives. The new code goes immediately after that closing brace and before the next `var.key = "ppsspp_button_preference"` block.

- [ ] **Step 2: Insert the OSD status-flags block**

Insert after the `ppsspp_psp_model` block:

```cpp
   // OSD status indicators — packed into iShowStatusFlags bitmask.
   // Bit values from Core/ConfigValues.h:210-214 (ShowStatusFlags enum):
   //   FPS_COUNTER     = 1 << 1
   //   SPEED_COUNTER   = 1 << 2
   //   BATTERY_PERCENT = 1 << 3
   {
      unsigned flags = 0;
      var.key = "ppsspp_osd_show_fps";
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
          && !strcmp(var.value, "enabled"))
         flags |= (1 << 1);
      var.key = "ppsspp_osd_show_speed";
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
          && !strcmp(var.value, "enabled"))
         flags |= (1 << 2);
      var.key = "ppsspp_osd_show_battery";
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value
          && !strcmp(var.value, "enabled"))
         flags |= (1 << 3);
      g_Config.iShowStatusFlags = (int)flags;
   }
```

- [ ] **Step 3: Insert the OSD debug-overlay block**

Immediately after the status-flags block:

```cpp
   // OSD debug overlay — single enum. USE_PROFILER is never defined in any
   // ppsspp-libretro build (verified in plan Task 1), so enum DebugOverlay
   // is compactly numbered 0..8 and label-index == enum-value directly.
   var.key = "ppsspp_osd_debug_overlay";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
      static const char* const labels[] = {
         "Off", "Debug stats", "Frame timings graph", "Frame timing",
         "Control debug", "Audio debug", "GPU profile",
         "GPU allocator viewer", "Framebuffer list",
      };
      for (size_t i = 0; i < sizeof(labels)/sizeof(labels[0]); ++i) {
         if (!strcmp(var.value, labels[i])) {
            g_Config.iDebugOverlay = (int)i;
            break;
         }
      }
   }
```

- [ ] **Step 4: Rebuild and confirm it links**

```bash
cd /Users/mark/Documents/Projects/ppsspp-libretro/build-libretro && \
  ninja ppsspp_libretro 2>&1 | tail -10
```

Expected: `[X/X] Linking CXX shared library` (or similar) followed by `ninja: Entering directory ... [N/N]` success. If the linker complains about `g_Config` or `Core/Config.h` not found, the include for Core/Config.h is already present at the top of `libretro.cpp` (verify with `head -20 libretro.cpp`) — if missing, add `#include "Core/Config.h"` at the top.

- [ ] **Step 5: Sanity-check the new options are reported via the libretro API**

The libretro core's `retro_get_core_options_v2` / variant-update path is hard to exercise without a frontend. Defer functional verification to Task 9 (runtime smoke in RetroNest). For now, dump the symbol table to confirm new strings are present:

```bash
DYLIB=$(find /Users/mark/Documents/Projects/ppsspp-libretro/build-libretro -name "ppsspp_libretro.dylib" -type f | head -1) && \
  strings "$DYLIB" | grep -E "ppsspp_osd_" | sort -u
```

Expected: 4 lines — `ppsspp_osd_show_fps`, `ppsspp_osd_show_speed`, `ppsspp_osd_show_battery`, `ppsspp_osd_debug_overlay`. Zero lines means the option_defs_us insertion didn't actually compile in — re-check Task 2.

No commit yet.

---

### Task 4: Install dylib + commit ppsspp-libretro changes

**Files:**
- Modify (install target, not source): `/Users/mark/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib`
- Will be committed: the two ppsspp-libretro source files from Tasks 2 + 3.

- [ ] **Step 1: Backup the currently installed dylib**

```bash
cp /Users/mark/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib \
   /Users/mark/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib.pre-osd-backup
ls -la /Users/mark/Documents/RetroNest/emulators/libretro/cores/ | grep ppsspp
```

Expected: original dylib + a new `.pre-osd-backup` sibling. The existing `.universal-backup` is from a prior build — leave it alone.

- [ ] **Step 2: Install the freshly built dylib**

```bash
DYLIB=$(find /Users/mark/Documents/Projects/ppsspp-libretro/build-libretro -name "ppsspp_libretro.dylib" -type f | head -1) && \
  cp "$DYLIB" /Users/mark/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib && \
  lipo -info /Users/mark/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib
```

Expected: `Architectures in the fat file: ... are: x86_64 arm64`.

- [ ] **Step 3: Verify install works end-to-end with the symbol grep**

```bash
strings /Users/mark/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib | grep -c "ppsspp_osd_"
```

Expected: `4`. (Counts all 4 unique option keys.)

- [ ] **Step 4: Confirm the ppsspp-libretro git state shows exactly the 2 expected file changes**

```bash
cd /Users/mark/Documents/Projects/ppsspp-libretro && git status
```

Expected:
```
On branch main
Changes not staged for commit:
	modified:   libretro/libretro.cpp
	modified:   libretro/libretro_core_options.h
```

(Plus any untracked `build-libretro/` directory — that's fine, gitignored or to-be-ignored.)

- [ ] **Step 5: Commit in the ppsspp-libretro repo**

```bash
cd /Users/mark/Documents/Projects/ppsspp-libretro && \
  git add libretro/libretro_core_options.h libretro/libretro.cpp && \
  git commit -m "$(cat <<'EOF'
feat(libretro): expose OSD options (show fps/speed/battery + debug overlay)

Adds 4 ppsspp_osd_* libretro core options that wrap PPSSPP's existing
iShowStatusFlags bitmask + iDebugOverlay enum:

  ppsspp_osd_show_fps      -> iShowStatusFlags bit (1<<1)
  ppsspp_osd_show_speed    -> iShowStatusFlags bit (1<<2)
  ppsspp_osd_show_battery  -> iShowStatusFlags bit (1<<3)
  ppsspp_osd_debug_overlay -> iDebugOverlay int  (label-index = enum-value)

USE_PROFILER is never defined in any ppsspp-libretro build configuration,
so enum DebugOverlay (Core/ConfigValues.h:253-266) is compactly numbered
0..8 and the 9 combo labels map 1:1 to enum values — no mapping table.

No PPSSPP rendering changes; the existing DrawFPS / DrawDebugOverlay code
in UI/DebugOverlay.cpp consumes both fields unchanged.

The 4 options live in a new "osd" category alongside existing system /
video / input / hacks / network categories.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 6: Verify commit landed**

```bash
cd /Users/mark/Documents/Projects/ppsspp-libretro && git log --oneline -3
```

Expected top commit: `feat(libretro): expose OSD options (show fps/speed/battery + debug overlay)`.

---

### Task 5: Add `showBattery` Q_PROPERTY to OsdPreview

**Files:**
- Modify: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/settings/widgets/preview/osd_preview.h`
- Modify: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/ui/settings/widgets/preview/osd_preview.cpp`

This is additive — existing adapters (PCSX2, mgba) don't bind `showBattery` and are unaffected.

- [ ] **Step 1: Add the Q_PROPERTY declaration**

In `osd_preview.h`, locate the `Q_PROPERTY(bool showSpeed ...)` line (`:30`). Insert a new line immediately after:

```cpp
    Q_PROPERTY(bool showBattery READ showBattery WRITE setShowBattery)
```

- [ ] **Step 2: Add the getter declaration**

In `osd_preview.h`, locate `bool showSpeed() const            { return m_s.speed; }` (`:89`). Insert immediately after:

```cpp
    bool showBattery() const          { return m_s.battery; }
```

- [ ] **Step 3: Add the setter declaration**

In `osd_preview.h`, locate the `void setShowSpeed(bool on);` setter declaration in the public section (search for `setShowSpeed`). Insert immediately after:

```cpp
    void setShowBattery(bool on);
```

- [ ] **Step 4: Add the `battery` field to the State struct**

In `osd_preview.h`, locate the `struct State` declaration (search for `struct State` or follow the references from `m_s.fps`, `m_s.speed`). The struct holds bool members for each show* flag. Add a `battery` field immediately after `speed`:

```cpp
    struct State {
        // ... existing fields ...
        bool fps = false;
        bool vps = false;
        bool speed = false;
        bool battery = false;   // <-- NEW
        // ... rest ...
    };
```

(Exact location: search the file for `bool speed` to find the line; insert the new line directly below.)

- [ ] **Step 5: Add the setter implementation**

In `osd_preview.cpp`, locate the `void OsdPreview::setShowSpeed(bool on)` implementation. Copy its body shape and insert directly after:

```cpp
void OsdPreview::setShowBattery(bool on) {
    if (m_s.battery == on) return;
    m_s.battery = on;
    update();
}
```

- [ ] **Step 6: Render the battery indicator in `drawTopRightIndicators`**

In `osd_preview.cpp:281-301`, modify the items-collection at the top of the function to include a battery line:

Replace:
```cpp
    QStringList items;
    if (m_s.indicators)          items << QStringLiteral("⏩ FF");
    if (m_s.videoCapture)        items << QStringLiteral("⏺ REC");
    if (m_s.inputRec)            items << QStringLiteral("● INPUT");
    if (m_s.textureReplacements) items << QStringLiteral("\U0001F3A8 TEX");
    if (items.isEmpty()) return;
```

With:
```cpp
    QStringList items;
    if (m_s.battery)             items << QStringLiteral("\U0001F50B 100%");
    if (m_s.indicators)          items << QStringLiteral("⏩ FF");
    if (m_s.videoCapture)        items << QStringLiteral("⏺ REC");
    if (m_s.inputRec)            items << QStringLiteral("● INPUT");
    if (m_s.textureReplacements) items << QStringLiteral("\U0001F3A8 TEX");
    if (items.isEmpty()) return;
```

(`\U0001F50B` = 🔋. "100%" is a static placeholder because the preview has no live battery source — PPSSPP under libretro typically reports 100% anyway.)

- [ ] **Step 7: Build everything that depends on `osd_preview` and confirm no regressions**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && \
  cmake --build . -j8 2>&1 | tail -10
```

Expected: clean build. The OsdPreview widget is used by RetroNest.app + several tests; all should build.

- [ ] **Step 8: Run the full test suite (sanity that PCSX2 OSD path is unaffected)**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && ctest -j4 2>&1 | tail -8
```

Expected: 43/44 (HotkeyDefs pre-existing). Any new failure means the Q_PROPERTY change broke something — re-read your `osd_preview.h` edits for declaration order or visibility issues.

No commit yet — Tasks 5-7 land as one RetroNest-Project commit at the end of Task 10.

---

### Task 6: Update `test_ppsspp_libretro_schema.cpp` to expect 4 new entries

**Files:**
- Modify: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/tests/test_ppsspp_libretro_schema.cpp`

Write the failing test first so we know the adapter changes in Task 7 actually do what we expect.

- [ ] **Step 1: Bump expected total count from 55 to 59**

In `test_ppsspp_libretro_schema.cpp`, locate the `totalCount_matchesSpec` slot. Replace:

```cpp
    void totalCount_matchesSpec() {
        PpssppLibretroAdapter a;
        // 43 libretro originals + 10 Recommended libretro dupes
        //  + 2 FrontendSetting rows in Recommended (aspect_mode, integer_scale) = 55.
        QCOMPARE(a.settingsSchema().size(), 55);
    }
```

With:

```cpp
    void totalCount_matchesSpec() {
        PpssppLibretroAdapter a;
        // 43 libretro originals + 10 Recommended libretro dupes
        //  + 2 FrontendSetting rows in Recommended (aspect_mode, integer_scale)
        //  + 4 OSD libretro rows (Phase B+C-OSD) = 59.
        QCOMPARE(a.settingsSchema().size(), 59);
    }
```

- [ ] **Step 2: Add the 4 new OSD keys to `knownUpstreamKeys()`**

In the `knownUpstreamKeys()` static function, append a new line after the existing `Hacks (6)` block:

```cpp
            // Hacks (6)
            "ppsspp_skip_buffer_effects", "ppsspp_disable_range_culling",
            "ppsspp_skip_gpu_readbacks", "ppsspp_lazy_texture_caching",
            "ppsspp_spline_quality", "ppsspp_lower_resolution_for_effects",
            // OSD (4)
            "ppsspp_osd_show_fps", "ppsspp_osd_show_speed",
            "ppsspp_osd_show_battery", "ppsspp_osd_debug_overlay",
        };
    }
```

- [ ] **Step 3: Add a new test slot for the OSD previewSpec wiring**

Insert a new slot at the end of the `private slots:` section, immediately before the final `};`:

```cpp
    void previewSpec_osd_isOsd() {
        // The OSD card hosts an OsdPreview pane bound to the 3 toggle keys.
        // The debug_overlay combo is not bound to the preview (preview has
        // no representation for full-screen debug overlays).
        PpssppLibretroAdapter a;
        const auto spec = a.previewSpec("OSD", "");
        QCOMPARE(spec.previewType, QStringLiteral("osd"));
        QCOMPARE(spec.keyToProperty.value("ppsspp_osd_show_fps"),
                 QStringLiteral("showFps"));
        QCOMPARE(spec.keyToProperty.value("ppsspp_osd_show_speed"),
                 QStringLiteral("showSpeed"));
        QCOMPARE(spec.keyToProperty.value("ppsspp_osd_show_battery"),
                 QStringLiteral("showBattery"));
    }
```

- [ ] **Step 4: Rebuild the test**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && \
  cmake --build . --target test_ppsspp_libretro_schema -j8 2>&1 | tail -5
```

Expected: builds successfully.

- [ ] **Step 5: Run the test — expect failures**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && \
  ctest -R "^PpssppLibretroSchema$" --output-on-failure 2>&1 | tail -25
```

Expected failures:
- `totalCount_matchesSpec`: actual 55, expected 59
- `previewSpec_osd_isOsd`: `spec.previewType` is empty (was `osd`), comparisons fail

Other slots pass. This confirms the test correctly detects the missing adapter changes.

No commit yet.

---

### Task 7: Add 6th hub card + 4 OSD SettingDefs + previewSpec OSD branch

**Files:**
- Modify: `/Users/mark/Documents/Projects/RetroNest-Project/cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`

- [ ] **Step 1: Add the 6th hub card**

In `settingsHubCards()`, locate the closing `};` of the return list. Insert a new card entry immediately before the `};`:

Replace:
```cpp
        { QStringLiteral("\U000026A1"), "Hacks",
          "Speed hacks — skip buffer effects, disable culling, lazy textures",
          "Hacks", 2, 0 },
    };
}
```

With:
```cpp
        { QStringLiteral("\U000026A1"), "Hacks",
          "Speed hacks — skip buffer effects, disable culling, lazy textures",
          "Hacks", 2, 0 },
        { QStringLiteral("\U0001F4CA"), "OSD",
          "On-screen FPS, speed, battery, and debug overlays",
          "OSD", 2, 1 },
    };
}
```

(`\U0001F4CA` = 📊 bar chart — picked for visual distinction from the other 5 card glyphs.)

- [ ] **Step 2: Add the OSD branch to `previewSpec()`**

In `previewSpec()`, after the existing Recommended branch and before the trailing `return {};`, insert:

Replace:
```cpp
PreviewSpec PpssppLibretroAdapter::previewSpec(const QString& category,
                                               const QString& subcategory) const {
    // Recommended hosts a live AspectRatioPreview bound to aspect_mode for
    // visual parity with PCSX2 / mgba. Other (category, subcategory) pairs
    // get no preview (returns empty PreviewSpec).
    if (category == "Recommended" && subcategory.isEmpty())
        return { "aspect", { { "aspect_mode", "aspectMode" } } };
    return {};
}
```

With:
```cpp
PreviewSpec PpssppLibretroAdapter::previewSpec(const QString& category,
                                               const QString& subcategory) const {
    // Recommended hosts a live AspectRatioPreview bound to aspect_mode for
    // visual parity with PCSX2 / mgba.
    if (category == "Recommended" && subcategory.isEmpty())
        return { "aspect", { { "aspect_mode", "aspectMode" } } };

    // OSD hosts an OsdPreview pane bound to the 3 status-flag toggles. The
    // debug_overlay combo is intentionally not bound — the preview has no
    // representation for the full-screen debug overlay modes.
    if (category == "OSD" && subcategory.isEmpty())
        return { "osd", {
            { "ppsspp_osd_show_fps",     "showFps" },
            { "ppsspp_osd_show_speed",   "showSpeed" },
            { "ppsspp_osd_show_battery", "showBattery" },
        }};

    // Other (category, subcategory) pairs get no preview.
    return {};
}
```

- [ ] **Step 3: Add the 4 OSD SettingDef rows**

In `settingsSchema()`, locate the `// === Hacks (6) ===` block and find its last entry (`ppsspp_lower_resolution_for_effects`). Insert a new OSD block immediately after the closing semicolon of that entry, BEFORE the `=== Recommended ===` block.

Insert:

```cpp
    // === OSD (4) ===
    // Wraps PPSSPP's iShowStatusFlags (3 toggles) + iDebugOverlay (1 combo).
    // Backed by 4 ppsspp_osd_* libretro options added in the ppsspp-libretro
    // fork (see commits in that repo for the upstream patch).

    s << opt("ppsspp_osd_show_fps", "Show FPS Counter",
             "disabled",
             { "enabled", "disabled" },
             "OSD",
             "Render the current frames-per-second in the corner of the screen.");

    s << opt("ppsspp_osd_show_speed", "Show Speed Percentage",
             "disabled",
             { "enabled", "disabled" },
             "OSD",
             "Render the current emulation speed (% of real PSP) in the corner of the screen.");

    s << opt("ppsspp_osd_show_battery", "Show Battery Percentage",
             "disabled",
             { "enabled", "disabled" },
             "OSD",
             "Render the host battery percentage. Note: under libretro this typically reports 100% because the core has no host battery hook.");

    s << opt("ppsspp_osd_debug_overlay", "Debug Overlay",
             "Off",
             { "Off", "Debug stats", "Frame timings graph", "Frame timing",
               "Control debug", "Audio debug", "GPU profile",
               "GPU allocator viewer", "Framebuffer list" },
             "OSD",
             "Render a full-screen debug overlay. Exactly one overlay may be active at a time.");
```

- [ ] **Step 4: Rebuild the test target**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && \
  cmake --build . --target test_ppsspp_libretro_schema -j8 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 5: Run the schema test and confirm all slots pass**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && \
  ctest -R "^PpssppLibretroSchema$" --output-on-failure 2>&1 | tail -15
```

Expected:
```
1/1 Test #N: PpssppLibretroSchema ........ Passed
100% tests passed, 0 tests failed out of 1
```

If `totalCount_matchesSpec` still fails: count your `s << ...` lines — must be 59 total now.
If `previewSpec_osd_isOsd` fails on `previewType`: double-check the string is `"osd"`, lowercase.
If `previewSpec_osd_isOsd` fails on `keyToProperty.value(...)`: triple-check the Q_PROPERTY name strings exactly match those declared in `osd_preview.h` (camelCase, `showFps` not `show_fps` not `showFPS`).

No commit yet.

---

### Task 8: Full ctest regression check

- [ ] **Step 1: Full build**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && \
  cmake --build . -j8 2>&1 | tail -5
```

Expected: `[100%] Built target RetroNest`.

- [ ] **Step 2: Full test suite**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && ctest -j4 2>&1 | tail -8
```

Expected: `43 of 44 tests passed` (HotkeyDefs::duckstation_completeness remains the lone pre-existing failure from commit `54964c4`).

- [ ] **Step 3: Confirm PCSX2 OSD-related tests still pass**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build && \
  ctest -R "Pcsx2" --output-on-failure 2>&1 | tail -10
```

Expected: all PCSX2 tests pass — the OsdPreview Q_PROPERTY addition is additive and PCSX2 didn't bind `showBattery`, so it's unaffected.

If any unexpected failure: STOP and diagnose — the OsdPreview change may have broken existing bindings.

---

### Task 9: Manual runtime smoke test

Requires interactive UI. If you're a fully autonomous executor, run Step 1 to launch and hand off to the human for Steps 2-7.

- [ ] **Step 1: Launch RetroNest**

```bash
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app/Contents/MacOS/RetroNest 2>&1 | tee /tmp/retronest-osd-smoke.log &
```

The log file capture is useful — if anything goes wrong, look for `[Pcsx2] Failed to map option` or similar `[PPSSPP]` messages.

- [ ] **Step 2: Open Settings → PSP and confirm 6 cards render**

From the home screen: Settings → PSP. Expect 6 cards:
- Row 0: Recommended (full-width, with aspect preview pane)
- Row 1: System | Video | Input
- Row 2: Hacks | OSD

The new OSD card should have a 📊 glyph, title "OSD", and the descriptor "On-screen FPS, speed, battery, and debug overlays".

- [ ] **Step 3: Open the OSD card and verify the 4 rows + preview**

Click OSD. Expect:
- 4 setting rows: Show FPS Counter / Show Speed Percentage / Show Battery Percentage / Debug Overlay
- Live `OsdPreview` pane next to the rows showing the simulated game scene
- Toggling Show FPS / Speed / Battery causes the preview's top-right indicators to update live (FPS=top-left in the perf column, battery=top-right glyph)

- [ ] **Step 4: Test value persistence**

Toggle Show FPS Counter to "enabled". Close the settings dialog. Re-open Settings → PSP → OSD. Confirm Show FPS Counter still shows "enabled".

- [ ] **Step 5: Test runtime effect — Show FPS in-game**

With Show FPS Counter = enabled, launch any PSP ROM (DBZ - Shin Budokai or similar from the Phase A test set). Once the game is rendering, confirm an FPS counter appears in the top-right corner of the rendered frame.

Exit cleanly via the in-game menu.

- [ ] **Step 6: Test runtime effect — Debug Overlay**

Settings → PSP → OSD → set Debug Overlay = "Frame timings graph". Save & close. Launch the same PSP ROM. Confirm a frame-time graph overlay renders mid-screen (translucent bars showing per-frame ms).

Reset Debug Overlay = "Off" before exiting.

- [ ] **Step 7: Confirm libretro option allow-list parity**

Settings → PSP → OSD → click any toggle. Watch the RetroNest stderr capture. Confirm NO `[PPSSPP] unknown option` or `option key not registered` warnings appear — that would mean the dylib install (Task 4) didn't actually replace the running core.

---

### Task 10: Commit RetroNest-Project changes

- [ ] **Step 1: Verify the working tree shows exactly the 4 expected files**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && git status
```

Expected:
```
On branch main
Changes not staged for commit:
	modified:   cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp
	modified:   cpp/src/ui/settings/widgets/preview/osd_preview.cpp
	modified:   cpp/src/ui/settings/widgets/preview/osd_preview.h
	modified:   cpp/tests/test_ppsspp_libretro_schema.cpp
```

If `ppsspp_libretro_adapter.h` is also modified: revert it — this plan added no header changes.

- [ ] **Step 2: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && \
  git add cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp \
          cpp/src/ui/settings/widgets/preview/osd_preview.h \
          cpp/src/ui/settings/widgets/preview/osd_preview.cpp \
          cpp/tests/test_ppsspp_libretro_schema.cpp && \
  git commit -m "$(cat <<'EOF'
feat(ppsspp): OSD hub card + preview + 4 ppsspp_osd_* settings rows

Adds a 6th "OSD" hub card to the PPSSPP Settings page, wired to the 4
new ppsspp_osd_* libretro core options added in the ppsspp-libretro
fork's matching commit.

Layout (now 6 cards):
  Row 0: Recommended (full-width, aspect preview)
  Row 1: System (11) | Video (22) | Input (4)
  Row 2: Hacks (6)   | OSD (4)

Schema count: 55 -> 59 (+4 OSD libretro rows under category "OSD").

OsdPreview gains a showBattery bool Q_PROPERTY + a small battery glyph
painted in drawTopRightIndicators. The change is additive -- PCSX2 and
mgba adapters don't bind showBattery and are unaffected (verified by
running the full ctest, still 43/44 with HotkeyDefs unchanged).

previewSpec("OSD","") binds the 3 status-flag toggles to OsdPreview
Q_PROPERTYs (show_fps -> showFps, show_speed -> showSpeed,
show_battery -> showBattery). The debug_overlay combo is not bound to
the preview -- the preview has no representation for full-screen debug
overlays, and binding a combo string to a bool Q_PROPERTY would be wrong.

test_ppsspp_libretro_schema:
  - total count bumped 55 -> 59
  - 4 new keys added to knownUpstreamKeys allow-list
  - new slot previewSpec_osd_isOsd asserts the 3 key->property pairs

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 3: Verify commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project && git log --oneline -3
```

Expected top commit: `feat(ppsspp): OSD hub card + preview + 4 ppsspp_osd_* settings rows`.

---

## Done criteria

- [ ] `cmake -DLIBRETRO=ON` produces a universal `ppsspp_libretro.dylib` with 4 `ppsspp_osd_*` strings present (`strings | grep -c ppsspp_osd_` == 4)
- [ ] Universal dylib installed at `/Users/mark/Documents/RetroNest/emulators/libretro/cores/ppsspp_libretro.dylib`, old version preserved as `.pre-osd-backup`
- [ ] `test_ppsspp_libretro_schema` passes all 10 slots (was 9 before this work)
- [ ] Full ctest stays at 43/44 (HotkeyDefs unchanged)
- [ ] Settings → PSP page renders 6 cards including the new OSD card
- [ ] Toggling Show FPS in OSD card causes the FPS counter to appear in-game
- [ ] Setting Debug Overlay = "Frame timings graph" renders a graph overlay in-game

Two commits land:
1. ppsspp-libretro: `feat(libretro): expose OSD options (show fps/speed/battery + debug overlay)`
2. RetroNest-Project: `feat(ppsspp): OSD hub card + preview + 4 ppsspp_osd_* settings rows`

---

## Out of scope (revisit list)

- Position selector / scale slider / bold-text — PPSSPP doesn't render these (would need PPSSPP rendering changes)
- Per-stat granularity (Show CPU / Show GPU / Show VPS) — PPSSPP doesn't expose per-stat toggles
- Developer flags (`bShowFrameProfiler`, `bShowImDebugger`) — separate Developer card if ever wanted
- Recommended-card duplicates of the OSD entries — OSD is advanced, not promoted
- Localization of the option labels into PPSSPP's I18N system — libretro options are upstream-English-only
- Live battery percentage in OsdPreview (always shows "100%" placeholder) — preview has no host battery source

---

## Plan self-review notes

Verified against the spec:

1. **Spec coverage**:
   - "Add `osd` category + 4 options to libretro_core_options.h" → Task 2
   - "Add 4 check_variable blocks in libretro.cpp" → Task 3
   - "Rebuild universal dylib + install" → Tasks 1, 4
   - "Add showBattery Q_PROPERTY to OsdPreview" → Task 5
   - "Add 6th hub card + 4 SettingDef rows + previewSpec OSD branch" → Task 7
   - "Update test (count, allow-list, new slot)" → Task 6
   - All non-goals from spec explicitly preserved in the plan's "Out of scope"

2. **Placeholder scan**: No TBD / TODO / "add appropriate error handling" patterns. Every code block is complete.

3. **Type consistency**:
   - SettingDef field names match Phase B+C: `key`, `label`, `defaultValue`, `type`, `options`, `category`, `tooltip`
   - SettingsHubCard ctor args order matches existing usage in adapter: `icon, title, descriptor, categoryKey, row, col`
   - PreviewSpec field names match `core/preview_spec.h`: `previewType`, `keyToProperty`
   - Q_PROPERTY names (`showFps`, `showSpeed`, `showBattery`) consistent across Task 5 (declaration), Task 6 (test assertion), Task 7 (previewSpec binding)
