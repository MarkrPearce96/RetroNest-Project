# Packet 7 Stage 2 — Core as Settings-Schema Truth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The core's declared options (SET_CORE_OPTIONS_V2/_INTL) become the single source of settings-schema truth — persisted to a per-core sidecar, seeded offline by a dlopen prober, rendered through a small hand-authored curation overlay — and the hand-mirrored option rows in all five adapters are deleted.

**Architecture:** Capture full v2 metadata in `environment_callbacks` → `CoreRuntime` writes `declared_options.json` beside `options.json` → `CoreProber` seeds the sidecar when absent (no session needed) → `LibretroAdapter::settingsSchema()` gains a base implementation merging sidecar × per-adapter `optionOverlays()` into `SettingDef` rows → the no-runtime fallback OptionsStore synthesizes from the sidecar. mGBA pilots the whole chain; the four big adapters follow one at a time, each parity-diffed before its mirror rows die.

**Tech Stack:** C++17/Qt6, QtTest, dlopen/dlsym, JSON via QJsonDocument.

**Spec:** `docs/superpowers/specs/2026-07-04-packet7-shared-contract-design.md` §Stage 2.

## SPIKE RESULT (2026-07-06 — plan foundation)

Standalone x86_64 probe (`retro_set_environment` only, no init, no game) against every deployed core:

| Core | Mechanism | Defs | Categories |
|---|---|---|---|
| mgba | SET_CORE_OPTIONS_V2_INTL | 13 | 5 |
| duckstation | SET_CORE_OPTIONS_V2 | 64 | none |
| ppsspp | SET_CORE_OPTIONS_V2_INTL | 76 | 5 |
| dolphin | SET_CORE_OPTIONS_V2 | 90 | none |
| pcsx2 | SET_CORE_OPTIONS_V2 | 89 | none |

**All five emit during `retro_set_environment` — the CoreProber design works with no fallbacks.** Consequences baked into this plan: capture must handle **V2_INTL** (use `intl->us`); sidecar must persist **categories** (mgba/ppsspp ship 5 each); pcsx2/dolphin/duckstation emit `categories=nullptr` (host overlay owns grouping, as designed).

## Global Constraints

- Same as Stage 1: x86_64 daily driver never left broken; **no pushes** until the user confirms; USER SMOKE GATES are hard stops with **standalone result reports**.
- `options.json` values are never migrated or rewritten beyond OptionsStore's existing behavior.
- Wording policy (spec decision 4): core-declared labels/info are adopted; overlay pins wording only where clearly better. Parity diffs compare **keys / value-sets / defaults / category routing** — NOT label/tooltip text.
- **CoreProber never calls `retro_init` and never `dlclose`s** — dlopen runs static initializers only; dlclose would run static *destructors* (the exact pcsx2 crash class from the quit-wedge saga). The handle is retained deliberately; dlopen refcounting makes a later real session safe.
- RetroNest builds/tests: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 6`, `arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 --output-on-failure`. Never pipe build output.

---

### Task 1: Rich declared-option capture + JSON round-trip

**Files:**
- Create: `cpp/src/core/libretro/declared_options.h`
- Create: `cpp/src/core/libretro/declared_options.cpp`
- Modify: `cpp/src/core/libretro/environment_callbacks.h` (context member), `environment_callbacks.cpp:32-46` (capture) + the V2_INTL dispatch arm
- Modify: `cpp/CMakeLists.txt` (add sources to `retronest_core`, new test target)
- Test: `cpp/tests/test_declared_options.cpp`

**Interfaces (produces):**

```cpp
// declared_options.h
struct DeclaredOptionValue { QString value; QString label; };          // label may be empty
struct DeclaredOption {
    QString key, label, categoryKey, info, defaultValue;
    QVector<DeclaredOptionValue> values;
};
struct DeclaredCategory { QString key, label, info; };
struct DeclaredOptionsDoc {
    int format = 1;
    QString coreLibraryVersion;                                        // filled by writer
    QVector<DeclaredCategory> categories;
    QVector<DeclaredOption> options;

    QJsonDocument toJson() const;
    static std::optional<DeclaredOptionsDoc> fromJson(const QByteArray&);
    bool save(const QString& path) const;                             // QSaveFile, indented
    static std::optional<DeclaredOptionsDoc> load(const QString& path);
    QVector<CoreOption> toCoreOptions() const;                        // thin view for OptionsStore
};
// Fill options/categories from a retro_core_options_v2* (nullptr-safe).
void populateFromV2(DeclaredOptionsDoc& doc, const struct retro_core_options_v2* v2);
```

`EnvironmentContext` gains `DeclaredOptionsDoc declaredDoc;` **alongside** the existing `declaredOptions` (`QVector<CoreOption>`) — the thin vector stays so `OptionsStore::load` and `test_environment_callbacks` compile unchanged; `handleCoreOptionsV2` fills **both** (thin derived via `doc.toCoreOptions()`).

- [ ] **Step 1: Write the failing test** — `cpp/tests/test_declared_options.cpp` with QtTest slots: `v2Populate()` (build a static 2-option `retro_core_option_v2_definition` array — one with a category key + value labels, one bare — plus a 1-entry categories array; assert every field lands), `jsonRoundTrip()` (populate → save to QTemporaryDir → load → compare all fields), `thinView()` (`toCoreOptions()` yields key/label/default/values with labels dropped), `loadMissingFile()` (returns nullopt, no crash). CMake target `test_declared_options` linking `retronest_core Qt6::Test`, `add_test(NAME DeclaredOptions …)` next to the schema-guard tests.
- [ ] **Step 2: Build the test target — expect FAIL** (`declared_options.h` not found).
- [ ] **Step 3: Implement** `declared_options.{h,cpp}` per the interface above. JSON shape: `{"format":1,"core_library_version":"","categories":[{"key","label","info"}...],"options":[{"key","label","category","info","default","values":[{"value","label"}...]}...]}`. Add both files to the `retronest_core` source list.
- [ ] **Step 4: Rewire capture.** In `environment_callbacks.cpp`, `handleCoreOptionsV2` becomes: `populateFromV2(ctx->declaredDoc, opts); ctx->declaredOptions = ctx->declaredDoc.toCoreOptions(); return true;`. Verify the dispatch arm for `SET_CORE_OPTIONS_V2_INTL` passes `((const retro_core_options_v2_intl*)data)->us` (read the arm first; the spike says mgba+ppsspp arrive via INTL, so this path is load-bearing).
- [ ] **Step 5: Build + run** `DeclaredOptions` + `EnvironmentCallbacks` tests → PASS; full suite green.
- [ ] **Step 6: Commit** `packet7-2: rich declared-option capture (full v2 metadata + JSON round-trip)`.

### Task 2: Sidecar write from live sessions + fake-core coverage

**Files:**
- Modify: `cpp/src/core/libretro/core_runtime.cpp` (~line 408, after `retro_init`) + `core_runtime.h`/StartConfig if a sidecar path member is cleaner than deriving
- Modify: `cpp/tests/fixtures/fake_libretro_core.c` (emit SET_CORE_OPTIONS_V2)
- Test: extend `cpp/tests/test_core_runtime.cpp`

**Interfaces:** sidecar path = `QFileInfo(m_cfg.optionsJsonPath).dir().filePath("declared_options.json")`; written only when `declaredDoc.options` non-empty; `coreLibraryVersion` from `retro_get_system_info().library_version`.

- [ ] **Step 1:** Extend `fake_libretro_core.c`: in `retro_set_environment`, emit a static 2-option `SET_CORE_OPTIONS_V2` (e.g. `fake_speed` = {"1","2"} default "1" with value labels; `fake_bool` = {"enabled","disabled"} default "disabled"), guarded so it keeps working for every existing test (env_cb returning false is fine — the fake core ignores the result).
- [ ] **Step 2:** Write the failing test in `test_core_runtime.cpp`: start a session against the fake core with `optionsJsonPath` in a QTemporaryDir, wait for it to reach running, stop; assert `declared_options.json` exists beside it, loads via `DeclaredOptionsDoc::load`, has the 2 options with labels and a non-empty `core_library_version`.
- [ ] **Step 3:** Implement the write in `runLoop()` directly after the `m_options.load(...)` reconcile block. Build; test PASS; full suite green.
- [ ] **Step 4: Commit** `packet7-2: persist declared options to per-core sidecar on every session start`.

### Task 3: CoreProber (offline seeding)

**Files:**
- Create: `cpp/src/core/libretro/core_prober.{h,cpp}`
- Modify: `cpp/CMakeLists.txt` (sources + test target)
- Test: `cpp/tests/test_core_prober.cpp` (probes the built `fake_libretro_core.dylib`)

**Interfaces (produces):** `namespace CoreProber { std::optional<DeclaredOptionsDoc> probe(const QString& coreDylibPath); }` — dlopen `RTLD_LAZY|RTLD_LOCAL`, resolve `retro_set_environment` + `retro_get_system_info`, install a static env callback that answers `GET_CORE_OPTIONS_VERSION=2` and captures V2/V2_INTL into a thread-guarded doc (mutex — probes are rare), call `retro_set_environment`, fill `coreLibraryVersion`, return. **No `retro_init`, no `dlclose`** (comment the wedge rationale; keep a `static QHash<QString,void*>` of retained handles so re-probes reuse).

- [ ] **Step 1:** Failing test: probe the fake core dylib (`CMAKE_BINARY_DIR` path, same mechanism `test_core_loader` uses to find it); assert 2 options + version string; probe a nonsense path → nullopt.
- [ ] **Step 2:** Implement; build; PASS; full suite green.
- [ ] **Step 3:** Manual sanity vs the real cores (matches the spike): temporarily run the probe against `~/Documents/RetroNest/emulators/libretro/cores/mgba_libretro.dylib` via a 5-line test slot or debug main — expect 13 options. Remove the temporary code.
- [ ] **Step 4: Commit** `packet7-2: CoreProber — offline declared-options seeding (dlopen, no init, no dlclose)`.

### Task 4: Overlay model + generic settingsSchema merge

**Files:**
- Create: `cpp/src/core/option_overlay.h` (struct only)
- Modify: `cpp/src/adapters/libretro/libretro_adapter.{h,cpp}` — new virtuals + base merge + sidecar/prober plumbing
- Test: `cpp/tests/test_libretro_schema_merge.cpp`

**Interfaces (produces):**

```cpp
// option_overlay.h
struct OptionOverlay {
    QString key;                     // core option key this curates
    QStringList categories;         // UI categories to list under (may include "Recommended")
    QString subcategory, group;     // optional routing
    QString labelOverride;           // empty = adopt core wording (decision 4)
    QString tooltipOverride;         // empty = core's info text
    QString defaultOverride;         // empty = core default (used where RetroNest deliberately differs)
    QString dependsOn, layout, suffix;
    SettingDef::Type typeOverride = SettingDef::Combo;  // Combo = "no override" sentinel is fine: merge default IS Combo
    bool hasTypeOverride = false;
    double minVal = 0, maxVal = 0, step = 0;
};
```

`LibretroAdapter` gains:
- `virtual QVector<OptionOverlay> optionOverlays() const { return {}; }`
- `virtual QVector<SettingDef> extraSettings() const { return {}; }` (hand-authored non-option rows: FrontendSetting/Ini)
- `const DeclaredOptionsDoc* declaredOptions()` — cached: runtime's live doc if running, else sidecar `DeclaredOptionsDoc::load`, else `CoreProber::probe(coreDylibInstallPath())` + `save()` (seeds the sidecar), else nullptr.
- **Base** `settingsSchema()` override (adapters converted in later tasks stop overriding it): for each overlay entry ×each of its `categories`, find the declared option by key and build a `SettingDef` — `storage=LibretroOption`, `key`, `label` = labelOverride | declared.label, `tooltip` = tooltipOverride | declared.info, `defaultValue` = defaultOverride | declared.default, `type` = typeOverride if `hasTypeOverride` else Combo, `options` = {value-label-or-value, value} pairs, plus routing/dependsOn/layout/min/max/step passthrough. Overlay order = row order. Declared options with no overlay entry are **not rendered** (logged once at debug level) but remain valid in OptionsStore. `extraSettings()` rows are appended as-is. Overlay keys missing from the declared doc are logged (warning) and skipped — the schema self-heals when a core drops an option.

- [ ] **Step 1:** Failing test `test_libretro_schema_merge.cpp`: a tiny `TestAdapter : LibretroAdapter` with a fixture `DeclaredOptionsDoc` injected (add a test-only setter or protected member), overlays exercising: adopt-wording, labelOverride, defaultOverride, Recommended cross-listing (2 categories → 2 rows, same key), unlisted-declared-key hidden, missing-declared-key skipped, extraSettings appended, value labels flowing into `options` pairs.
- [ ] **Step 2:** Implement `option_overlay.h` + the base merge; build; PASS; full suite green (existing adapters still override `settingsSchema()`, so nothing else changes yet).
- [ ] **Step 3: Commit** `packet7-2: overlay model + generic declared-options→SettingDef merge in LibretroAdapter`.

### Task 5: Fallback OptionsStore rewired to the declared doc

**Files:**
- Modify: `cpp/src/adapters/libretro/libretro_adapter.cpp:169-195` (`libretroOptionsStore`)
- Test: extend `cpp/tests/test_options_store.cpp` or the merge test

- [ ] **Step 1:** Failing test: adapter with declared doc available and NO schema override → `libretroOptionsStore()` contains ALL declared keys (superset of overlay), values validated against declared value-sets.
- [ ] **Step 2:** Implement: when `declaredOptions()` is non-null, synthesize from `doc->toCoreOptions()` (every declared key — supersedes the schema-derived subset); else fall back to the existing schema-derived path (still needed until all adapters convert). Build; PASS; suite green.
- [ ] **Step 3: Commit** `packet7-2: no-runtime OptionsStore synthesizes from declared options (schema-derived path kept for unconverted adapters)`.

### Task 6: mGBA pilot conversion + parity + USER SMOKE GATE 5

**Files:**
- Create: `cpp/tests/fixtures/schema_snapshots/mgba.json` (pre-conversion snapshot)
- Modify: `cpp/src/adapters/libretro/mgba_libretro_adapter.{h,cpp}` (settingsSchema → overlays + extras)
- Test: `cpp/tests/test_schema_parity.cpp` (transitional, per-core data rows)

- [ ] **Step 1: Snapshot BEFORE converting.** Add `test_schema_parity.cpp` with a dual mode: run with `SCHEMA_SNAPSHOT_WRITE=1` env → serialize `MgbaLibretroAdapter().settingsSchema()` (key, storage, category, type, defaultValue, ordered value list — NO labels/tooltips) to the fixture path; without the env → compare current schema against the fixture with rules: every snapshot row must exist with same key+category+storage; defaults equal unless the overlay's `defaultOverride` story says otherwise; value-set may GAIN values (report via qInfo) but must not LOSE any. Run snapshot mode; commit the fixture.
- [ ] **Step 2: Convert mgba.** Delete the `opt()` lambda + all 13-key mirrored rows (lines 82-313 region); implement `optionOverlays()` — one entry per key currently shown, with `categories` reproducing today's routing including the Recommended cross-listings (`mgba_skip_bios`, `mgba_color_correction`, `mgba_interframe_blending`, `mgba_idle_optimization`, `mgba_frameskip` → {"Recommended", native}), tooltips left empty except where the hand text is clearly better than upstream's, labels empty (adopt core). Implement `extraSettings()` returning the four `frontend(...)` rows (aspect_mode/integer_scale × Recommended/Video) verbatim. `settingsSchema()` override deleted; hub cards/previewSpec untouched.
- [ ] **Step 3: Generate the sidecar on this machine** (run the probe path once — e.g. `test_core_prober`'s manual slot or launch the settings page) and run `test_schema_parity` compare mode. Expect reported value-set gains (e.g. `mgba_gb_colors` palettes beyond "Grayscale" — the known stale mirror). Investigate anything LOST before proceeding; losses = overlay bug, not acceptance.
- [ ] **Step 4:** Update `cpp/CMakeLists.txt` if mgba had a schema-guard test affected; full suite + full x86_64 rebuild; app launches.
- [ ] **Step 5: Commit** `packet7-2: mGBA pilot — hand schema deleted, rendered from core-declared options + overlay`.
- [ ] **Step 6: USER SMOKE GATE 5 (standalone report first):** mGBA settings page renders with all categories/cards; options show core wording (fuller palette list on Default Game Boy Palette is EXPECTED); change a value without ever launching a game (prober-seeded sidecar), launch a GBA game, verify the value applied and persists; Recommended card mirrors edits.

### Tasks 7–10: duckstation → ppsspp → dolphin → pcsx2 conversions (one gate each)

Same recipe as Task 6 per core, with per-core notes. For EACH core, in this order (duckstation `786`cpp, ppsspp `592`, dolphin `682`, pcsx2 `1503` lines):

- [ ] **Step 1:** Snapshot fixture (`schema_snapshots/<core>.json`) from the CURRENT hand schema; commit.
- [ ] **Step 2:** Convert: `settingsSchema()` → `optionOverlays()` + `extraSettings()`. Carry over per-row `type` (Bool/Int/Float/slider layouts) via `typeOverride`/`hasTypeOverride` + min/max/step where the hand schema had them; carry `dependsOn` expressions verbatim; carry every deliberate `defaultOverride` (KNOWN CASE: duckstation renderer default "Automatic" vs core base "Metal" — review LOW item; verify against the parity diff rather than assuming). Keep hub cards, controller schemas, hotkeys, preview specs, transforms untouched. ppsspp: core options arrive via V2_INTL with upstream categories — overlay routing still wins (ignore core categories, keep today's tabs). pcsx2: expect the parity diff to surface hand-schema rows whose keys DON'T exist in the core's 89 (cross-listed/duplicated rows are fine — same key twice; genuinely orphaned keys must be investigated, they'd be dead UI today).
- [ ] **Step 3:** Parity compare; investigate losses; record gains in the commit message.
- [ ] **Step 4:** Update that core's QtTest schema guard (`test_<core>_libretro_schema`) to assert against the MERGED schema — keep structural rules (defaults ∈ values, no dup keys per category, prefixed keys), drop rules that hardcoded row counts of the hand mirror.
- [ ] **Step 5:** Full suite + rebuild + commit `packet7-2: <core> rendered from core-declared options`.
- [ ] **Step 6: USER SMOKE GATE (6/7/8/9, standalone report first):** core's settings pages browse correctly pre-launch; one meaningful option changed + verified in-game (duckstation: renderer default still Automatic + a video option; ppsspp: a graphics option; dolphin: an EXI/graphics option — raw ints now carry core value-labels; pcsx2: an Emulation + a Graphics option, and the Patches page unaffected); values survive app restart.

### Task 11: Delete the dead machinery

**Files:**
- Delete: `duckstation-libretro/src/duckstation-libretro/tools/check_schema_fidelity.py`, `pcsx2-libretro/pcsx2-libretro/tools/check_schema_fidelity.py` + its `add_custom_target check_schema_fidelity` (CMakeLists.txt:64-85) + `RETRONEST_PCSX2_LIBRETRO_ADAPTER` cache var, `dolphin-libretro/Source/Core/DolphinLibretro/tools/check_schema_fidelity.py` + its target + cache var
- Modify: purge every phantom `check_schema_fidelity` comment from RetroNest adapters (`dolphin_libretro_adapter.h:64`, `duckstation_libretro_adapter.h/.cpp`, `pcsx2_libretro_adapter.cpp` — grep `check_schema_fidelity` across cpp/src)
- Delete: `cpp/tests/test_schema_parity.cpp` + snapshots (transitional tooling; the updated per-core QtTest guards are the permanent net)

- [ ] **Step 1:** Deletions + comment purge; each fork rebuilds (drift check unaffected — the checker was never in the package); RetroNest full suite green.
- [ ] **Step 2:** Commits per repo: `packet7-2: retire schema fidelity checkers — the core is the schema source now`.
- [ ] **Step 3:** Update memory (stage 2 record: per-repo commits, sidecar format, overlay conventions, parity findings worth remembering); final standalone report with push list.

## Risks

| Risk | Mitigation |
|---|---|
| Prober dlopen of heavyweight cores in-app | Spike-proven for all 5; no init, no dlclose (static-dtor crash class); handle retained |
| Hand-schema rows for keys the core no longer declares | Parity diff flags; merge logs + skips (dead UI removed knowingly) |
| Value-set losses (hand mirror had values core lacks) | Parity rule: gains reported, losses block until investigated |
| Deliberate RetroNest defaults lost (duckstation renderer) | `defaultOverride` in overlay; parity compares defaults |
| Widget-type regressions (Bool/slider rows) | `typeOverride` carries the hand schema's type; per-core gate eyeballs the pages |
| Sidecar absent on fresh install | Prober seeds at first settings access; live session rewrites on every start |

## Effort

Tasks 1–5 ≈ one session · Task 6 (pilot + gate) ≈ half · Tasks 7–10 ≈ one to two sessions (pcsx2 is half of that alone) · Task 11 ≈ trivial.
