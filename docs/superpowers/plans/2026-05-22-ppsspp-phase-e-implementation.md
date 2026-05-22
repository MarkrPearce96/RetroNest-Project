# PPSSPP Phase E Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bump PPSSPP's default internal rendering resolution to 4x (`1920x1088`) on first launch, via a reusable mechanism where the adapter schema's `SettingDef.defaultValue` overrides the libretro core's declared default.

**Architecture:** `OptionsStore::load` gains an optional `schemaDefaults` parameter, applied between "existing value in options.json" and "upstream-declared default" in the resolution priority chain. `GameSession` builds the map from the adapter's `settingsSchema()` (filtering `Storage::LibretroOption` rows) before invoking `CoreRuntime::start`. PPSSPP's `SettingDef.defaultValue` for `ppsspp_internal_resolution` flips from `"480x272"` to `"1920x1088"` — and a new drift-guardrail test asserts every other PPSSPP `LibretroOption` row matches upstream, with an explicit allowlist for intentional overrides.

**Tech Stack:** C++17, Qt6 (QtTest for unit tests), CMake 3.16+, ctest.

**Spec:** `docs/superpowers/specs/2026-05-22-ppsspp-phase-e-design.md`

---

## File Structure

Seven files change. Listed bottom-up so each task lands a self-contained, compilable, testable slice.

- `cpp/src/core/libretro/options_store.h` — extend `load()` signature (optional `schemaDefaults` arg, default `{}`).
- `cpp/src/core/libretro/options_store.cpp` — implement schema-default-aware resolution rule via a `pickInitialValue` static helper used by both the disk-path and `:memory:` branches.
- `cpp/tests/test_options_store.cpp` — add unit tests covering the new priority rule.
- `cpp/src/core/libretro/core_runtime.h` — add `QHash<QString, QString> schemaOptionDefaults` to `StartConfig`.
- `cpp/src/core/libretro/core_runtime.cpp` — pass `m_cfg.schemaOptionDefaults` to `m_options.load(...)`.
- `cpp/src/core/game_session.cpp` — build the schema-defaults map from `lr->settingsSchema()` before populating cfg.
- `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp` — change `ppsspp_internal_resolution` `SettingDef.defaultValue` at both rows (lines ~246 and ~482) from `"480x272"` to `"1920x1088"`.
- `cpp/tests/test_ppsspp_libretro_schema.cpp` — add upstream-defaults fixture (43 entries), drift guardrail test slot, dupe-consistency test slot.

---

## Task 1: OptionsStore — schema defaults override upstream

**Files:**
- Modify: `cpp/src/core/libretro/options_store.h`
- Modify: `cpp/src/core/libretro/options_store.cpp`
- Modify: `cpp/tests/test_options_store.cpp`

- [ ] **Step 1: Write the failing test**

Add these two new slots inside the `TestOptionsStore` class in `cpp/tests/test_options_store.cpp` (place them right after `testReconcileSeedsDefaults()` so all "load priority" tests live together):

```cpp
    void testSchemaDefaultOverridesUpstream() {
        QTemporaryDir d;
        OptionsStore s;
        QString path = d.path() + "/options.json";
        QVector<CoreOption> coreOpts = {
            {"mgba_skip_bios", "Skip BIOS", "OFF", {"OFF","ON"}},
        };
        QHash<QString, QString> schemaDefaults = { {"mgba_skip_bios", "ON"} };
        QVERIFY(s.load(path, coreOpts, schemaDefaults));
        // Schema default overrides upstream's "OFF" on first load.
        QCOMPARE(s.get("mgba_skip_bios"), QString("ON"));
    }
    void testExistingValueBeatsSchemaDefault() {
        QTemporaryDir d;
        QString path = d.path() + "/options.json";
        QVector<CoreOption> coreOpts = {
            {"mgba_skip_bios", "Skip BIOS", "OFF", {"OFF","ON"}},
        };
        // Seed an existing on-disk value.
        {
            OptionsStore s;
            s.load(path, coreOpts);
            s.set("mgba_skip_bios", "OFF");
            s.save();
        }
        // Schema says "ON", but the on-disk "OFF" must win.
        OptionsStore s2;
        QHash<QString, QString> schemaDefaults = { {"mgba_skip_bios", "ON"} };
        QVERIFY(s2.load(path, coreOpts, schemaDefaults));
        QCOMPARE(s2.get("mgba_skip_bios"), QString("OFF"));
    }
    void testInvalidSchemaDefaultFallsBackToUpstream() {
        QTemporaryDir d;
        OptionsStore s;
        QString path = d.path() + "/options.json";
        QVector<CoreOption> coreOpts = {
            {"mgba_skip_bios", "Skip BIOS", "OFF", {"OFF","ON"}},
        };
        // Schema names a value upstream doesn't list.
        QHash<QString, QString> schemaDefaults = { {"mgba_skip_bios", "MAYBE"} };
        QVERIFY(s.load(path, coreOpts, schemaDefaults));
        // Invalid schema default → silent fall-through to upstream default.
        QCOMPARE(s.get("mgba_skip_bios"), QString("OFF"));
    }
```

- [ ] **Step 2: Run test to verify it fails (compile error)**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" >/dev/null
cmake --build build --target test_options_store 2>&1 | tail -20
```

Expected: compile error referencing `load(...)` taking 2 args, not 3. (`QHash<QString,QString>` as third arg is unknown.)

- [ ] **Step 3: Update `options_store.h`**

Replace the `load` declaration:

```cpp
    bool load(const QString& jsonPath, const QVector<CoreOption>& coreOptions);
```

with:

```cpp
    bool load(const QString& jsonPath,
              const QVector<CoreOption>& coreOptions,
              const QHash<QString, QString>& schemaDefaults = {});
```

The default-argument value keeps every existing callsite source-compatible.

- [ ] **Step 4: Update `options_store.cpp`**

Replace the entire `OptionsStore::load(...)` function (lines 9–39 in the current file) with the version below. The change introduces a private static `pickInitialValue` helper that both the `:memory:` branch and the disk branch consult so the priority rule lives in one place.

```cpp
namespace {
QString pickInitialValue(const CoreOption& opt,
                         const QHash<QString, QString>& existing,
                         const QHash<QString, QString>& schemaDefaults) {
    auto existIt = existing.constFind(opt.key);
    if (existIt != existing.constEnd() && opt.values.contains(existIt.value()))
        return existIt.value();
    auto schemaIt = schemaDefaults.constFind(opt.key);
    if (schemaIt != schemaDefaults.constEnd() && opt.values.contains(schemaIt.value()))
        return schemaIt.value();
    return opt.defaultValue;
}
}  // namespace

bool OptionsStore::load(const QString& jsonPath,
                        const QVector<CoreOption>& coreOptions,
                        const QHash<QString, QString>& schemaDefaults) {
    QWriteLocker lk(&m_lock);
    m_path = jsonPath;
    m_values.clear();

    if (jsonPath == ":memory:") {
        m_path.clear();          // sentinel: never write to disk
        QHash<QString, QString> emptyExisting;
        for (const auto& opt : coreOptions)
            m_values.insert(opt.key, pickInitialValue(opt, emptyExisting, schemaDefaults));
        return true;
    }

    QHash<QString, QString> existing;
    QFile f(jsonPath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(f.readAll());
        const auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            existing.insert(it.key(), it.value().toString());
        f.close();
    }

    for (const auto& opt : coreOptions)
        m_values.insert(opt.key, pickInitialValue(opt, existing, schemaDefaults));

    lk.unlock();
    return save();
}
```

- [ ] **Step 5: Run tests to verify pass**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build --target test_options_store 2>&1 | tail -5
./build/test_options_store
```

Expected: all four pre-existing slots plus the three new slots pass. Look for `Totals: 7 passed, 0 failed`.

- [ ] **Step 6: Commit**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/options_store.h cpp/src/core/libretro/options_store.cpp cpp/tests/test_options_store.cpp
git commit -m "$(cat <<'EOF'
feat(libretro): OptionsStore consults schema defaults

Adds an optional schemaDefaults arg to OptionsStore::load. Priority
order for first-launch values is now: existing → schema → upstream,
with the schema entry validated against opt.values so a stale or
mistyped value silently degrades to upstream rather than corrupting
options.json.

Phase E step 1/5. See docs/superpowers/specs/2026-05-22-ppsspp-phase-e-design.md.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Plumb the override map through CoreRuntime

**Files:**
- Modify: `cpp/src/core/libretro/core_runtime.h`
- Modify: `cpp/src/core/libretro/core_runtime.cpp`

No new behavior tests — this task is pure plumbing covered end-to-end by manual verification in Task 6.

- [ ] **Step 1: Add the StartConfig field**

In `cpp/src/core/libretro/core_runtime.h`, inside the `struct StartConfig { ... }` (around line 25), add a new field after `raEncore`:

```cpp
    struct StartConfig {
        QString emuId;
        QString corePath;
        QString romPath;
        QString systemDir;
        QString saveDir;
        QString optionsJsonPath;
        QString resumeStatePath;   // optional; if non-empty, retro_unserialize after load
        int raConsoleId = 0;
        QString raUsername;
        QString raToken;
        bool raHardcore = false;
        bool raEncore = false;   // see LibretroRaConfig::encore
        QHash<QString, QString> schemaOptionDefaults;  // {libretro_key → adapter schema default}
    };
```

Add `#include <QHash>` near the top of `core_runtime.h` if it isn't already pulled in transitively.

- [ ] **Step 2: Pass the map to OptionsStore::load**

In `cpp/src/core/libretro/core_runtime.cpp`, update the existing `m_options.load(...)` call (currently line 386):

```cpp
    if (!m_envCtx.declaredOptions.isEmpty())
        m_options.load(m_cfg.optionsJsonPath, m_envCtx.declaredOptions, m_cfg.schemaOptionDefaults);
```

- [ ] **Step 3: Build to verify**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build 2>&1 | tail -10
```

Expected: clean build, no errors.

- [ ] **Step 4: Commit**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/core_runtime.h cpp/src/core/libretro/core_runtime.cpp
git commit -m "$(cat <<'EOF'
feat(libretro): plumb schemaOptionDefaults through CoreRuntime::StartConfig

Adds schemaOptionDefaults to StartConfig and forwards it to
OptionsStore::load. Empty by default — existing non-libretro flows
and any callers not building the map see no behavior change.

Phase E step 2/5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: GameSession builds the schema-defaults map

**Files:**
- Modify: `cpp/src/core/game_session.cpp`

- [ ] **Step 1: Extract the map from the adapter before populating cfg**

Open `cpp/src/core/game_session.cpp`. Find the block that builds the `CoreRuntime::StartConfig cfg;` (currently starts at line 203). Immediately AFTER the line that sets `cfg.optionsJsonPath` (currently line 209), insert this block:

```cpp
    // Phase E: build the schema-defaults override map from the libretro
    // adapter's settingsSchema(). Filters for Storage::LibretroOption rows
    // only — FrontendSetting rows (e.g. aspect_mode) live in a separate
    // sidecar and aren't libretro core options. Duplicate keys in the
    // schema (Recommended card duplicates rows from other cards) are
    // expected to carry matching defaultValue; the dupe-consistency test
    // enforces that invariant.
    {
        QHash<QString, QString> schemaDefaults;
        for (const auto& s : lr->settingsSchema()) {
            if (s.storage == SettingDef::Storage::LibretroOption && !s.key.isEmpty())
                schemaDefaults.insert(s.key, s.defaultValue);
        }
        cfg.schemaOptionDefaults = std::move(schemaDefaults);
    }
```

If `core/setting_def.h` isn't already included in this translation unit, add `#include "core/setting_def.h"` near the top.

- [ ] **Step 2: Build to verify**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build 2>&1 | tail -10
```

Expected: clean build, no errors.

- [ ] **Step 3: Commit**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/game_session.cpp
git commit -m "$(cat <<'EOF'
feat(libretro): GameSession extracts schema defaults from adapter

Walks the libretro adapter's settingsSchema() and forwards every
Storage::LibretroOption row's {key → defaultValue} as the schema
override map for OptionsStore. With this in place, any libretro
adapter can override an upstream-declared default just by editing
its SettingDef.defaultValue — no further plumbing needed.

Phase E step 3/5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Bump PPSSPP's internal resolution default to 4x

**Files:**
- Modify: `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`

- [ ] **Step 1: Update both ppsspp_internal_resolution rows**

Open `cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp`. The `ppsspp_internal_resolution` `SettingDef` appears at two locations — once in the main Video card (currently around line 245) and once in the Recommended card (currently around line 481). Both pass `"480x272"` as the third (defaultValue) argument to `optLabeled`. Change both to `"1920x1088"`:

```cpp
    s << optLabeled("ppsspp_internal_resolution", "Rendering Resolution",
                    "1920x1088",
                    { { "1x (480x272)",     "480x272" },
                      // ... rest of enum list unchanged ...
                    },
                    "Internal render resolution. Higher = sharper but costs GPU. 1x is native PSP (480x272).");
```

Apply the change to BOTH rows. The dupe-consistency test added in Task 5 enforces that both copies carry identical `defaultValue`; missing one row will fail that test.

- [ ] **Step 2: Build to verify the existing schema test still passes**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build --target test_ppsspp_libretro_schema 2>&1 | tail -5
./build/test_ppsspp_libretro_schema
```

Expected: all pre-existing slots pass (the count assertions are unaffected — we changed a `defaultValue` string, not the row count).

- [ ] **Step 3: Commit**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/ppsspp_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
feat(ppsspp): default internal resolution to 4x (1920x1088)

PPSSPP upstream defaults to native PSP resolution (480x272) for
low-end hardware compatibility. On RetroNest's target (modern Macs
running x86_64 under Rosetta or arm64 natively), 4x is crisp without
GPU cost concerns. Existing users with an options.json keep their
prior selection — this only affects fresh installs.

Phase E step 4/5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Drift guardrail + dupe-consistency tests

**Files:**
- Modify: `cpp/tests/test_ppsspp_libretro_schema.cpp`

- [ ] **Step 1: Add the upstream-defaults fixture**

Open `cpp/tests/test_ppsspp_libretro_schema.cpp`. Below the existing `knownFrontendKeys()` static method inside the `TestPpssppLibretroSchema` class (around line 54), add this new fixture. Every entry was extracted by reading `/Users/mark/Documents/Projects/ppsspp-libretro/libretro/libretro_core_options.h` lines 124–722 on 2026-05-22; the comment at the top of the existing `knownUpstreamKeys()` already binds this file to that header.

```cpp
    // Upstream-declared default for every libretro option key the
    // PpssppLibretroAdapter schema covers. Mirrors the `default_value`
    // field (last positional arg) of each `option_defs_us[]` entry in
    // libretro/libretro_core_options.h.
    //
    // When PPSSPP upstream changes a default, the drift-guardrail test
    // below fails. Resolution: either update this fixture (accept the
    // change) or add the key to `intentionalOverrides` and update the
    // schema's SettingDef.defaultValue (reject the change).
    static QHash<QString, QString> upstreamDefaults() {
        return {
            // System (11)
            {"ppsspp_cpu_core", "JIT"},
            {"ppsspp_fast_memory", "enabled"},
            {"ppsspp_ignore_bad_memory_access", "enabled"},
            {"ppsspp_io_timing_method", "Fast"},
            {"ppsspp_force_lag_sync", "disabled"},
            {"ppsspp_locked_cpu_speed", "disabled"},
            {"ppsspp_memstick_inserted", "enabled"},
            {"ppsspp_cache_iso", "disabled"},
            {"ppsspp_cheats", "disabled"},
            {"ppsspp_language", "Automatic"},
            {"ppsspp_psp_model", "psp_2000_3000"},
            // Video (22)
            {"ppsspp_backend", "auto"},
            {"ppsspp_software_rendering", "disabled"},
            {"ppsspp_internal_resolution", "480x272"},
            {"ppsspp_mulitsample_level", "Disabled"},
            {"ppsspp_cropto16x9", "enabled"},
            {"ppsspp_frameskip", "disabled"},
            {"ppsspp_frameskiptype", "Number of frames"},
            {"ppsspp_auto_frameskip", "disabled"},
            {"ppsspp_frame_duplication", "enabled"},
            {"ppsspp_detect_vsync_swap_interval", "disabled"},
            {"ppsspp_inflight_frames", "Up to 2"},
            {"ppsspp_gpu_hardware_transform", "enabled"},
            {"ppsspp_software_skinning", "enabled"},
            {"ppsspp_hardware_tesselation", "disabled"},
            {"ppsspp_texture_scaling_type", "xbrz"},
            {"ppsspp_texture_scaling_level", "disabled"},
            {"ppsspp_texture_deposterize", "disabled"},
            {"ppsspp_texture_shader", "disabled"},
            {"ppsspp_texture_anisotropic_filtering", "16x"},
            {"ppsspp_texture_filtering", "Auto"},
            {"ppsspp_smart_2d_texture_filtering", "disabled"},
            {"ppsspp_texture_replacement", "disabled"},
            // Input (4)
            {"ppsspp_button_preference", "Cross"},
            {"ppsspp_analog_is_circular", "disabled"},
            {"ppsspp_analog_deadzone", "0.0"},
            {"ppsspp_analog_sensitivity", "1.00"},
            // Hacks (6)
            {"ppsspp_skip_buffer_effects", "disabled"},
            {"ppsspp_disable_range_culling", "disabled"},
            {"ppsspp_skip_gpu_readbacks", "disabled"},
            {"ppsspp_lazy_texture_caching", "disabled"},
            {"ppsspp_spline_quality", "High"},
            {"ppsspp_lower_resolution_for_effects", "disabled"},
        };
    }

    // Keys whose schema SettingDef.defaultValue is deliberately different
    // from upstream. Each entry is a frontend-side product decision; the
    // drift test skips these so a future upstream-default change in one
    // of them won't auto-revert RetroNest's preference.
    static QSet<QString> intentionalOverrides() {
        return { "ppsspp_internal_resolution" };
    }
```

- [ ] **Step 2: Add the two test slots**

Below the existing test slots in the same file (after the last `void testSomething() {...}` slot but still inside the class, before the closing `};`), add:

```cpp
    void libretroOptionDefaults_matchUpstreamUnlessAllowlisted() {
        PpssppLibretroAdapter a;
        const auto fixture = upstreamDefaults();
        const auto allowlist = intentionalOverrides();
        for (const auto& s : a.settingsSchema()) {
            if (s.storage != SettingDef::Storage::LibretroOption)
                continue;
            if (allowlist.contains(s.key))
                continue;
            QVERIFY2(fixture.contains(s.key),
                qPrintable(QString("schema row for libretro option '%1' not present in "
                                   "upstreamDefaults fixture. Either add it to the fixture "
                                   "or mark it as an intentional override.").arg(s.key)));
            QVERIFY2(s.defaultValue == fixture.value(s.key),
                qPrintable(QString("default drift for '%1': schema='%2' vs upstream='%3'. "
                                   "Either update the schema to match upstream or add '%1' "
                                   "to intentionalOverrides().")
                        .arg(s.key).arg(s.defaultValue).arg(fixture.value(s.key))));
        }
    }
    void duplicatedRows_haveConsistentDefaults() {
        PpssppLibretroAdapter a;
        QHash<QString, QString> firstSeenDefault;
        QStringList violations;
        for (const auto& s : a.settingsSchema()) {
            if (s.storage != SettingDef::Storage::LibretroOption)
                continue;
            auto it = firstSeenDefault.constFind(s.key);
            if (it == firstSeenDefault.constEnd()) {
                firstSeenDefault.insert(s.key, s.defaultValue);
            } else if (it.value() != s.defaultValue) {
                violations << QString("'%1': first=%2 later=%3")
                                  .arg(s.key).arg(it.value()).arg(s.defaultValue);
            }
        }
        if (!violations.isEmpty()) {
            QFAIL(qPrintable(QString("schema rows with the same key carry different "
                                     "defaultValue. Recommended card duplicates must "
                                     "match their canonical row. Violations: %1")
                                .arg(violations.join("; "))));
        }
    }
```

`QVERIFY2` carries a runtime message string in its failure output (which `QCOMPARE` doesn't), so a drift failure tells the contributor the exact key and both values without needing to re-read the test.

- [ ] **Step 3: Build and run the test**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build --target test_ppsspp_libretro_schema 2>&1 | tail -5
./build/test_ppsspp_libretro_schema
```

Expected: all pre-existing slots pass, plus the two new slots pass. The `libretroOptionDefaults_matchUpstreamUnlessAllowlisted` slot skips `ppsspp_internal_resolution` (it's in the allowlist) and asserts the other 42 keys match the fixture. The `duplicatedRows_haveConsistentDefaults` slot passes because both copies of every duplicated key carry matching `defaultValue` (including both copies of resolution at `"1920x1088"` after Task 4).

If `libretroOptionDefaults_matchUpstreamUnlessAllowlisted` fails for any non-allowlisted key, the schema has accidental drift. Read the failure message: it names the key and shows the schema vs. upstream values. Fix by editing the schema's `SettingDef.defaultValue` to match upstream (the common case — accidental drift), or by adding the key to `intentionalOverrides()` if it's a deliberate frontend choice. Recompile and re-run.

- [ ] **Step 4: Commit**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/tests/test_ppsspp_libretro_schema.cpp
git commit -m "$(cat <<'EOF'
test(ppsspp): drift guardrail + dupe-consistency for libretro defaults

Adds two slots to TestPpssppLibretroSchema:

  libretroOptionDefaults_matchUpstreamUnlessAllowlisted — asserts every
  LibretroOption-typed SettingDef.defaultValue matches the upstream
  libretro_core_options.h declaration, with explicit allowlist for
  intentional overrides (currently just ppsspp_internal_resolution).

  duplicatedRows_haveConsistentDefaults — asserts the Recommended card
  duplicates of any libretro-option row carry the same defaultValue,
  protecting the QHash-merge invariant in GameSession.

Phase E step 5/5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Manual verification

No commits. This task is the end-to-end smoke test that proves the new behavior reaches a running game.

- [ ] **Step 1: Confirm full build + all tests green**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build 2>&1 | tail -5
cd build && ctest --output-on-failure 2>&1 | tail -30
```

Expected: full build clean, all tests pass.

- [ ] **Step 2: Reset PPSSPP options.json**

```sh
rm -f ~/Documents/RetroNest/emulators/libretro/ppsspp/options.json
ls ~/Documents/RetroNest/emulators/libretro/ppsspp/ 2>&1
```

Expected: no `options.json` listed.

- [ ] **Step 3: Launch RetroNest and inspect the Rendering Resolution dropdown**

```sh
open /Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app
```

In the running app: navigate Settings → PSP → Video → Rendering Resolution. **Expected:** the selected dropdown value is `"4x (1920x1088)"`. If it shows `"1x (480x272)"`, the schema change in Task 4 didn't reach the runtime — most likely GameSession isn't being called for the Settings UI path. In that case, file a follow-up: the Settings UI may use `OptionsStore::load` via a different entry point (e.g. `AppController`), and that entry point also needs to thread the schema-defaults map through.

- [ ] **Step 4: Inspect options.json**

After step 3, options.json should now exist (the act of opening the Settings page or starting a game triggers a load+save cycle):

```sh
cat ~/Documents/RetroNest/emulators/libretro/ppsspp/options.json | grep internal_resolution
```

Expected: `"ppsspp_internal_resolution": "1920x1088"`.

- [ ] **Step 5: Launch a PSP game**

Pick any PSP ROM from the library. Confirm rendering is crisp and not pixelated 1x.

- [ ] **Step 6: Regression check — existing user value wins**

```sh
# Quit RetroNest first.
# Manually edit options.json to set resolution back to 480x272.
sed -i '' 's/"ppsspp_internal_resolution": "1920x1088"/"ppsspp_internal_resolution": "480x272"/' \
    ~/Documents/RetroNest/emulators/libretro/ppsspp/options.json
cat ~/Documents/RetroNest/emulators/libretro/ppsspp/options.json | grep internal_resolution
```

Expected: shows `"480x272"`.

Now re-launch RetroNest and re-check Settings → PSP → Video → Rendering Resolution. **Expected:** the selected value is `"1x (480x272)"` — proving that an existing on-disk user value wins over the new schema default (the existing-value-wins priority in `pickInitialValue` is intact).

- [ ] **Step 7: Restore the new default**

```sh
rm ~/Documents/RetroNest/emulators/libretro/ppsspp/options.json
```

Next launch will write the new 4x default fresh. Done.

---

## Self-review notes

- All seven file changes from the spec are covered by Tasks 1–5; Task 6 is the verification gate.
- The `pickInitialValue` helper signature is consistent across the test (Task 1, Step 1) and the implementation (Task 1, Step 4) — both `(opt, existing, schemaDefaults)`.
- The `intentionalOverrides()` allowlist (Task 5) names exactly the one key Task 4 changes (`ppsspp_internal_resolution`).
- The dupe-consistency test (Task 5) asserts the invariant that Task 4 relies on (both resolution rows must carry the same `defaultValue`).
- The upstream-defaults fixture in Task 5 is fully populated with all 43 entries — no placeholders, all values match the upstream header as of 2026-05-22.
- The `QHash` include in `core_runtime.h` is flagged as conditional in Task 2 ("if not already pulled in transitively") — the executing engineer should add the include only if the build fails without it.
- Manual verification (Task 6, Step 3) anticipates a possible additional code path: if the Settings UI loads options.json via a non-`GameSession` route, that route also needs threading. Flagged as a follow-up rather than expanding scope mid-plan.
