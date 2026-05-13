# SP7b — Settings push: core-options migration

**Date:** 2026-05-13
**Status:** Approved — ready for plan
**Sub-project:** SP7b (remaining scope under SP7 "Settings push" after SP7a closed Resources path + region/fps)
**Touches:** `pcsx2-libretro/` (new module + 2 file edits) AND `RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.{h,cpp}` (settingsSchema override).

## Goal

Make three of the values currently hardcoded in `pcsx2-libretro/Settings.cpp`'s
`InitializeDefaults` user-tweakable from RetroNest's per-emulator settings dialog,
by emitting them as libretro core options via `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2`:

1. **GS renderer** — Auto / Metal / Software / Null (today: Auto, hardcoded at `Settings.cpp:216`).
2. **MTVU (multi-threaded VU1)** — on / off (today: on, hardcoded at `Settings.cpp:248`).
3. **Fast boot** — on / off (today: on, hardcoded at `Settings.cpp:238` **and** at `LibretroFrontend.cpp:496` — see "FastBoot two-site coupling" below).

The three knobs above are the smallest valuable cut from the kickoff's
candidate list — the ones users actually tune per-game for compat/perf
trade-offs. Other candidates (Achievements toggle, Slot 2 memcard, EnableVerbose
logging) are deferred — see "Out of scope" below.

## Vocabulary clarification

The original SP7 plan called this "retire INI patching" — that phrasing was
wrong. `pcsx2-libretro/Settings.cpp` already uses **programmatic**
`MemorySettingsInterface::Set{String,Int,Bool}Value` calls. No INI string is
emitted; nothing to retire. SP7b's actual job is to make a curated subset of
those programmatic writes **user-configurable** via libretro's core options API,
and to mirror that schema host-side in `Pcsx2LibretroAdapter::settingsSchema()`
so the existing RetroNest settings dialog renders the rows.

## Out of scope (locked in)

- **Live runtime updates** of the exposed knobs. PCSX2 reads `Renderer` once
  during `VMManager::Initialize`; flipping it mid-run requires explicit
  GS-device teardown/re-open ceremony and is fragile even in PCSX2-Qt. `vuThread`
  is consulted once at VM init. `EnableFastBoot` is meaningful only at boot.
  All three knobs are naturally "restart to apply", matching PCSX2-Qt's UX.
- **Per-game overrides** (RetroNest-side data-model feature; distinct sub-project).
- **The three deferred candidate knobs:** Achievements (RetroNest already runs
  rcheevos in a parallel flow — exposing this would confuse users into thinking
  it controls in-game cheevos when it doesn't), Slot 2 memcard (niche),
  EnableVerbose (10–20% perf regression if accidentally enabled — risky toggle).
- **Cleanup carry-overs from SP7a's final review** — three items were listed
  for possible SP7b fold-in:
  - `Settings.cpp:148 SetResourcesDirectory()` fallback. **Already explicit**
    in the SP7a final-polish commit (`eece07804`) with WARN + inline comment
    documenting the fallback warm-state. Drop from SP7b.
  - `IsKnownSerialPrefix` + `DetectRegionFromSerialPrefix` duplicate prefix list
    in `CoreResources.cpp`. SP7b does not touch that file; defer to whichever
    later sub-project naturally edits there.
  - `g_detected_fps = 59.94` hardcoded in remaining 2 sites in `LibretroFrontend.cpp`
    (lines 121, 555). SP7b touches `LibretroFrontend.cpp` only at
    `retro_set_environment` and `retro_load_game`; these two sites stay out of
    the touched code path. Defer.

## Background

### Host-side support is already wired

RetroNest already handles the env calls SP7b's core code needs to make. SP7b is
not adding host infrastructure; it's filling out a schema.

- **`SET_CORE_OPTIONS_V2`** — `environment_callbacks.cpp:31-44` parses the
  emitted definitions into `EnvironmentContext::declaredOptions` (a
  `QVector<CoreOption>`).
- **Reconciliation** — `core_runtime.cpp:299-301` calls
  `m_options.load(optionsJsonPath, declaredOptions)` after `retro_init`,
  before `retro_load_game`. `OptionsStore::load` (`options_store.cpp:9-39`)
  reads any existing `options.json`, keeps user values that match the declared
  `values` whitelist, fills defaults for the rest, and persists.
- **`GET_VARIABLE` lookup** — `environment_callbacks.cpp:16-22` returns the
  current value for a given key as a `const char*` valid until the next
  env_cb call.
- **Settings dialog rendering** — `generic_settings_page.cpp:46-71` renders
  `SettingDef::Storage::LibretroOption` rows by reading/writing
  `LibretroAdapter::libretroOptionsStore()` (which shares the same JSON file
  with the runtime when a game is live, or lazy-loads from disk otherwise —
  see `libretro_adapter.cpp:138-164`).
- **Reference implementation** — `mgba_libretro_adapter.cpp:86` sets
  `d.storage = SettingDef::Storage::LibretroOption` on every adapter-provided
  SettingDef. SP7b's PCSX2 schema mirrors this pattern row-for-row.

### FastBoot two-site coupling

`VMBootParameters.fast_boot` is set hardcoded to `true` at
`pcsx2-libretro/LibretroFrontend.cpp:496` inside `retro_load_game`. PCSX2's
`VMManager::BootSystem` reads `params.fast_boot` directly, overriding the INI
`EmuCore/EnableFastBoot` value. **The user-facing FastBoot core option must be
wired at both sites:** the `EnableFastBoot` Settings.cpp value AND the
`params.fast_boot` field. Flipping only one will silently fail to take effect.

### Renderer enum values

Verified in `pcsx2/Config.h:271-281`:
```
enum class GSRendererType : s8 {
    Auto = -1,
    DX11 = 3,
    Null = 11,
    OGL = 12,
    SW = 13,
    VK = 14,
    Metal = 17,
    DX12 = 15,
};
```

SP7b exposes **only** the four values relevant on macOS — Auto, Metal, SW, Null.
DX11/DX12/OGL/VK are not viable build paths in our toolchain; hiding them keeps
the UI honest.

## Architecture

### New module: `pcsx2-libretro/CoreOptions.{h,cpp}`

Single responsibility — declare the option schema, emit it via env_cb, and
resolve user values into a typed struct.

```
// CoreOptions.h
namespace Pcsx2Libretro::CoreOptions {

struct Resolved {
    int  renderer  = -1;    // GSRendererType integer (-1=Auto)
    bool mtvu      = true;
    bool fast_boot = true;
};

bool EmitCoreOptionsV2(retro_environment_t cb);
Resolved ReadResolved(retro_environment_t cb);

} // namespace
```

`kCoreOptions[]` lives in the `.cpp` as a file-scope
`retro_core_option_v2_definition[]` array, terminated with a zero-initialised
entry. Labels and descriptions are human-readable (mirror the SP7a-era
`Settings.cpp` comments — those comments are the existing reasoning for each
default).

`EmitCoreOptionsV2`:
- Calls `cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &kCoreOptions_v2_struct)`.
- Returns the env_cb's bool result; on false, logs WARN once (host doesn't
  support the env — falls through to compile-time defaults; not a regression
  vs SP7a behavior).

`ReadResolved`:
- Issues three `GET_VARIABLE` queries with keys `pcsx2_renderer`, `pcsx2_mtvu`,
  `pcsx2_fast_boot`.
- Parses the string values returned by the host into typed fields. Unknown
  enum strings (e.g. renderer that doesn't match `auto|metal|software|null`)
  log a WARN with the offending value and fall back to the field's compile-time
  default.
- Returns `Resolved` by value (small POD, no allocation).
- Logs the resolved triple once: `[CoreOptions] renderer=auto mtvu=on fast_boot=on`.

### Settings.cpp integration

`InitializeDefaults` signature gains an optional resolved pointer:

```
void InitializeDefaults(const std::string& system_dir,
                        const std::string& save_dir,
                        const CoreOptions::Resolved* options = nullptr);
```

If `options` is null (defensive — should not happen in normal flow), the
existing hardcoded defaults are written (`Renderer=-1`, `vuThread=true`,
`EnableFastBoot=true`). If non-null, the three corresponding `g_si.Set*Value`
calls use `options->renderer`, `options->mtvu`, `options->fast_boot` instead.

The three call sites:
- `Settings.cpp:216` `g_si.SetIntValue("EmuCore/GS", "Renderer", ...)`
- `Settings.cpp:248` `g_si.SetBoolValue("EmuCore/Speedhacks", "vuThread", ...)`
- `Settings.cpp:238` `g_si.SetBoolValue("EmuCore", "EnableFastBoot", ...)`

### LibretroFrontend.cpp integration

Two edits:

1. `retro_set_environment` — after the env_cb pointer is stashed in
   `g_frontend.environ_cb`, call `CoreOptions::EmitCoreOptionsV2(g_frontend.environ_cb)`.
   This is the only safe time to emit per the libretro spec (it's the first
   env call the core can make).

2. `retro_load_game` — early (after BIOS resolution, before
   `InitializeDefaults`), call `CoreOptions::ReadResolved(g_frontend.environ_cb)`
   into a local `const auto resolved = …`. Pass `&resolved` to
   `Settings::InitializeDefaults`. Replace the hardcoded `params.fast_boot = true`
   at line 496 with `params.fast_boot = resolved.fast_boot`.

### Host-side: Pcsx2LibretroAdapter::settingsSchema

New override that returns three `SettingDef` rows with
`storage = SettingDef::Storage::LibretroOption`. Pattern mirrors
`mgba_libretro_adapter.cpp` row-for-row:

```
QVector<SettingDef> Pcsx2LibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;
    auto def = [](const char* key, const char* label, const char* tooltip,
                  SettingDef::Type type, const char* defaultValue,
                  QVector<QPair<QString,QString>> options) -> SettingDef {
        SettingDef d{};
        d.category = "Emulation";  // or "Recommended" — see UX note below
        d.key = key;
        d.label = label;
        d.tooltip = tooltip;
        d.type = type;
        d.defaultValue = defaultValue;
        d.options = std::move(options);
        d.storage = SettingDef::Storage::LibretroOption;
        return d;
    };
    s.append(def("pcsx2_renderer", "GS Renderer", "...", SettingDef::Combo, "auto",
                 {{"Auto", "auto"}, {"Metal", "metal"},
                  {"Software", "software"}, {"Null", "null"}}));
    s.append(def("pcsx2_mtvu", "Multi-Threaded VU1", "...", SettingDef::Bool, "enabled",
                 {{"Enabled", "enabled"}, {"Disabled", "disabled"}}));
    s.append(def("pcsx2_fast_boot", "Fast Boot", "...", SettingDef::Bool, "enabled",
                 {{"Enabled", "enabled"}, {"Disabled", "disabled"}}));
    return s;
}
```

UX note: category placement (`"Emulation"` vs `"Recommended"` vs a new
`"PS2 (libretro)"` tab) is a small UI decision deferred to plan-time review of
how PPSSPP / Dolphin / MgbaLibretro lay out their `settingsSchema()`. Plan
task 4 will pick the category by inspecting those three for the closest
analog. Default if uncertain: `"Recommended"` matching PPSSPP's lead pattern.

### Boot-time data flow

```
retro_set_environment(cb)
  ├─ g_frontend.environ_cb = cb
  └─ CoreOptions::EmitCoreOptionsV2(cb)
       └─ host: handleCoreOptionsV2 → declaredOptions
retro_init
  └─ host: m_options.load(optionsJsonPath, declaredOptions)
           ├─ reads existing options.json
           ├─ keeps user values that match declared "values" whitelist
           ├─ fills defaults for missing keys
           └─ saves back to disk
retro_load_game
  ├─ system_dir / BIOS resolution (unchanged)
  ├─ const auto opts = CoreOptions::ReadResolved(environ_cb);
  │     └─ host: handleGetVariable × 3
  ├─ Settings::InitializeDefaults(system_dir, save_dir, &opts);
  │     └─ writes Renderer / vuThread / EnableFastBoot via opts
  ├─ VMBootParameters params{};
  ├─ params.filename = game->path;
  ├─ params.fast_boot = opts.fast_boot;     // was: true (hardcoded)
  ├─ (cold-resume env query — unchanged from SP6.5 Task 4.5)
  └─ EmuThread::Start(params)
```

## Error handling

| Failure mode | Detection | Behavior |
|---|---|---|
| Host returns false from `SET_CORE_OPTIONS_V2` | `EmitCoreOptionsV2` return value | WARN once; continue. No regression vs SP7a — defaults still ship. |
| Host returns NULL from `GET_VARIABLE` for a known key | `retro_variable.value == nullptr` | WARN once per key per session; fall back to that field's compile-time default. |
| Unknown renderer string from host (e.g. legacy `vulkan`) | `ReadResolved` string→int switch hits default | WARN once with offending value; fall back to `auto`. |
| User stored a key value from a previous schema version no longer in the declared `values` whitelist | Caught by `OptionsStore::load` (host-side, existing) | Stale value silently replaced with default and persisted. Existing behavior — covered. |

## Testing

### TDD unit test

`pcsx2-libretro/tools/test_core_options.cpp` — standalone-compile pattern from
SP7a's `test_region_prefix.cpp`. Built via single `clang++` with
`-DSP7B_TEST_CORE_OPTIONS_ONLY` gate. No PCSX2 link required.

Cases:
1. Happy path: env_cb returns `metal` / `disabled` / `disabled` → `Resolved`
   has `renderer=17, mtvu=false, fast_boot=false`.
2. Missing key: env_cb returns NULL for `pcsx2_mtvu` → defaults to `true`.
3. Unknown enum: env_cb returns `vulkan` for renderer → defaults to `-1`
   (Auto) with WARN logged.
4. All defaults: env_cb returns each default string → `Resolved` matches
   compile-time defaults exactly.
5. Emit: a fake env_cb captures the `SET_CORE_OPTIONS_V2` payload and
   verifies key/value/default counts match `kCoreOptions`.

### Live smoke (kickoff requires ≥ 2 PS2 games)

Run inside `arch -x86_64 .../RetroNest`:

**R&C 2 (NTSC, default-routed, id=11):**
- Open settings dialog → verify three rows visible: GS Renderer, MTVU, Fast Boot.
- Toggle MTVU off → exit → re-launch → stderr should log
  `[CoreOptions] renderer=auto mtvu=off fast_boot=on`. Observe EE-thread saturation
  rises (single-threaded VU1 — known ~5–10% perf regression).
- Toggle MTVU back on, toggle Fast Boot off → re-launch → PS2 BIOS Sony intro
  + region check screen visible (sanity for `params.fast_boot` wiring).
- Toggle Fast Boot back on → re-launch → game boots straight to title.

**DBZ Budokai Tenkaichi 2 (PAL, SLES-54164, requires DB flip to libretro):**
- Same MTVU on→off→on cycle.
- Toggle renderer to `software` → re-launch → expect framerate drop to ~10–15 fps
  during 3D combat (CPU rasterization on PS2-scale geometry). Verify the
  renderer change is what caused the change. Toggle back to `auto`.
- Renderer `null` → re-launch → black screen, audio still works (sanity for the
  Null path). Toggle back.

### Regression coverage (must still work)

- SP7a region/fps: `[Region] region=NTSC fps=59.94 ...` line present for R&C 2;
  `region=PAL fps=50.00` for DBZ.
- SP7a Resources path: `[CoreResources] Resources dir = ...` line present.
- SP6 SET_MEMORY_MAPS: `SET_MEMORY_MAPS captured 2 descriptors` (RAM + scratchpad).
- SP6 memcard slot 1 persistence: save → exit → re-launch → load → save data present.
- SP6.5 save state: in-session save / load operates normally.
- SP6.5 Task 4.6 Save & Exit: writes `.resume`, clean shutdown, no force-kill.
- SP6.5 Task 4.8 EmuThread reuse: multiple Resume → Save & Exit → Resume cycles
  in one RetroNest process, each rcheevos session activating cleanly.

### Host build verification

`cmake --build build-arm64 --target RetroNest` and run unit tests under
`build-arm64`: `ctest -R 'test_options_store|test_environment_callbacks'`
must remain green (no host-side behavior change expected — schema additions
are pure-additive at the SettingDef list level).

## Files changed (estimate)

```
pcsx2-libretro/CoreOptions.h                                   NEW (~30 LOC)
pcsx2-libretro/CoreOptions.cpp                                 NEW (~150 LOC)
pcsx2-libretro/tools/test_core_options.cpp                     NEW (~100 LOC, standalone)
pcsx2-libretro/CMakeLists.txt                                  +1 line
pcsx2-libretro/Settings.cpp                                    +12/-3 (signature + 3 writes)
pcsx2-libretro/LibretroFrontend.cpp                            +4/-1 (emit + read + params)

RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.h    +1 line
RetroNest-Project/cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp  +35/-0
```

No upstream PCSX2 file edits. No new RetroNest-side host infrastructure (all
load-bearing host code already lives in `options_store.{h,cpp}` +
`environment_callbacks.cpp` + `libretro_adapter.cpp` + `generic_settings_page.cpp`).

## Plan-time decisions (deferred from this spec)

1. **UI category placement** of the three rows — `"Recommended"` vs
   `"Emulation"` vs a new pcsx2-libretro tab. Plan task that adds the schema
   should match the layout of whichever existing libretro adapter (MgbaLibretroAdapter
   is the obvious peer) the user already finds the cleanest.
2. **Tooltip wording** — the inline comments in `Settings.cpp` already explain
   each knob's purpose (`Settings.cpp:212-216` for Renderer, `:242-248` for
   MTVU, `:237` for FastBoot). The plan task that adds the schema lifts those
   comments into the `tooltip` field.
3. **Whether to wire `pcsx2_fast_boot` as `Bool` (Enabled/Disabled labels)
   vs renaming to an explicit Combo like other libretro cores do.** Bool is
   correct per `SettingDef::Type`; mGBA uses `Bool` for similar boot-time
   toggles. Stick with `Bool`.
