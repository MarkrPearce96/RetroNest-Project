# Dolphin Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Dolphin (GameCube + Wii) as a first-class RetroNest emulator: installable from the in-app grid, ROMs scan into the library, games launch in our window with our overlay, settings persist across restarts.

**Architecture:** Single Dolphin manifest covering both `gc` and `wii` systems (one binary handles both). New `DolphinAdapter` inherits from `EmulatorAdapter`, writes a portable layout next to the binary, patches Dolphin's split config (`Dolphin.ini`, `GFX.ini`, `GCPadNew.ini`, `WiimoteNew.ini`, `Hotkeys.ini`, `RetroAchievements.ini`), exposes a Dolphin.ini-only settings page (Graphics deferred to native UI), bakes default controller profiles (no in-app remap UI for v1), and uses pause-on-focus-loss for our in-game overlay (no save-on-exit / no resume detection).

**Framework extension (the only one):** add an optional `file` field to `ResolutionOptions`, `AspectRatioOptions`, and `IniPatch` so the wizard's resolution/aspect quick-settings can write to `GFX.ini` instead of the adapter's main `configFilePath()`. PCSX2/DuckStation/PPSSPP leave `file` empty → unchanged behavior.

**Tech Stack:** C++17, Qt6 (Core/Test/Widgets), CMake 3.16+. Tests use QtTest (`QtTest`/`QObject` slots, `QVERIFY`/`QCOMPARE`, registered via `add_test`).

**Spec:** `docs/superpowers/specs/2026-05-03-dolphin-adapter-design.md` (read first).

---

## Reference: file structure overview

**Created:**
- `manifests/dolphin.json`
- `cpp/src/adapters/dolphin_adapter.h`
- `cpp/src/adapters/dolphin_adapter.cpp`
- `cpp/tests/test_dolphin_schema.cpp`
- `cpp/tests/test_quick_settings_file_field.cpp`
- `qml/AppUI/images/dolphin-logo.png` (placeholder; user supplies final asset later)

**Modified:**
- `cpp/src/adapters/emulator_adapter.h` (add `file` field to three structs)
- `cpp/src/services/config_service.cpp` (honor `file` field in 4 quick-settings methods)
- `cpp/src/adapters/adapter_registry.cpp` (register `DolphinAdapter`)
- `cpp/CMakeLists.txt` (add adapter sources + 2 new tests)
- `cpp/src/services/scraper.cpp` (add `gc`/`wii` ScreenScraper IDs)
- `cpp/src/ui/theme_context.cpp` (add `gc`/`wii` display names)
- `cpp/src/services/ra_client.cpp` (add `gc`/`wii` RA console IDs)
- `qml/AppUI/EmulatorLogos.js` (add Dolphin logo path)
- `qml/SetupWizard/EmulatorCard.qml` (add Dolphin to local `logoForEmu()`)
- `qml/AppUI/CMakeLists.txt` (add Dolphin logo to RESOURCES)
- `qml/SetupWizard/CMakeLists.txt` (add Dolphin logo to RESOURCES)

---

## Task 1: Add `file` field to ResolutionOptions, AspectRatioOptions, IniPatch

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h:19-58`

- [ ] **Step 1: Add `file` field to the three structs**

Open `cpp/src/adapters/emulator_adapter.h`. In the struct definitions near the top (lines 19-58), add an optional `file` field to each of `ResolutionOptions`, `IniPatch`, and `AspectRatioOptions`. The default empty string preserves existing behavior — adapters that don't set it continue to use `configFilePath()`.

Replace the three struct bodies as follows:

```cpp
/**
 * ResolutionOptions — describes how to set resolution for an emulator.
 *
 * If `file` is non-empty, it overrides configFilePath() as the INI file
 * read/written by the quick-settings UI for resolution. Adapters whose
 * resolution lives in a separate file (e.g. Dolphin's GFX.ini) set it
 * to the absolute path of that file; others leave it empty.
 */
struct ResolutionOptions {
    QString section;       // INI section
    QString key;           // INI key
    QVector<ResolutionOption> options;
    QString defaultValue;  // which value is default
    QString file;          // optional override; empty = use configFilePath()
};

/**
 * IniPatch — a single section/key/value to write to an INI file.
 *
 * If `file` is non-empty, it overrides configFilePath() as the destination
 * file. Used by adapters whose aspect-ratio patches target a non-main file
 * (e.g. Dolphin's GFX.ini).
 */
struct IniPatch {
    QString section;
    QString key;
    QString value;
    QString file;          // optional override; empty = use configFilePath()
};

/**
 * AspectRatioOption — a label + list of INI patches to apply when selected.
 * Supports emulators that need multiple keys changed (e.g. aspect + widescreen patch).
 */
struct AspectRatioOption {
    QString label;               // e.g. "4:3", "16:9"
    QVector<IniPatch> patches;   // all INI writes for this choice
};

/**
 * AspectRatioOptions — describes aspect ratio choices for an emulator.
 *
 * Per-patch `IniPatch::file` overrides the destination file on a per-patch
 * basis. The top-level `file` here is unused; per-patch granularity is what
 * Dolphin needs (a single aspect choice may touch GFX.ini only).
 */
struct AspectRatioOptions {
    QVector<AspectRatioOption> options;
    QString defaultLabel;  // which option label is default
};
```

- [ ] **Step 2: Verify the header still compiles cleanly**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -20`
Expected: build proceeds (the new fields are optional with defaults, so existing call sites are untouched). If it fails, fix any unrelated compile errors before continuing.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "Adapter framework: add optional file field to ResolutionOptions/IniPatch

Lets adapters route quick-settings reads/writes to a non-main config file
(e.g. Dolphin's GFX.ini). Default empty preserves PCSX2/DuckStation/PPSSPP
behavior unchanged."
```

---

## Task 2: ConfigService — honor `file` field in quick resolution

**Files:**
- Modify: `cpp/src/services/config_service.cpp:167-203` (currentResolution + applyQuickResolution)

- [ ] **Step 1: Add a helper at the top of the file**

Open `cpp/src/services/config_service.cpp`. After the existing `#include`s (around line 10), add this small helper that picks between `opts.file` and `adapter->configFilePath()`:

```cpp
// Returns options.file if non-empty, else adapter->configFilePath().
// Used by quick-settings paths so adapters can target a non-main INI file
// (e.g. Dolphin's GFX.ini for resolution/aspect).
template <typename Opts>
static QString resolveQuickSettingsPath(const Opts& opts, EmulatorAdapter* adapter) {
    return opts.file.isEmpty() ? adapter->configFilePath() : opts.file;
}
```

- [ ] **Step 2: Update `currentResolution()` to use the helper**

Find `QString ConfigService::currentResolution(const QString& emuId) const` (around line 167). Replace the body with:

```cpp
QString ConfigService::currentResolution(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto opts = adapter->resolutionOptions();
    if (opts.options.isEmpty()) return {};

    QString configPath = resolveQuickSettingsPath(opts, adapter);
    if (configPath.isEmpty()) return opts.defaultValue;

    IniFile ini;
    ini.load(configPath);
    QString val = ini.value(opts.section, opts.key);
    return val.isEmpty() ? opts.defaultValue : val;
}
```

- [ ] **Step 3: Update `applyQuickResolution()` to use the helper**

Find `void ConfigService::applyQuickResolution(const QVariantMap& choices)` (around line 183). Replace the body with:

```cpp
void ConfigService::applyQuickResolution(const QVariantMap& choices) {
    for (auto it = choices.constBegin(); it != choices.constEnd(); ++it) {
        const QString& emuId = it.key();
        const QString value = it.value().toString();

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        auto opts = adapter->resolutionOptions();
        if (opts.options.isEmpty()) continue;

        QString configPath = resolveQuickSettingsPath(opts, adapter);
        if (configPath.isEmpty()) continue;

        IniFile ini;
        ini.load(configPath);
        ini.setValue(opts.section, opts.key, value);
        ini.save(configPath);
    }
    emit statusMessage("Resolution settings saved.");
}
```

- [ ] **Step 4: Build to verify**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -10`
Expected: clean compile.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/config_service.cpp
git commit -m "ConfigService: honor ResolutionOptions::file in quick resolution path"
```

---

## Task 3: ConfigService — honor `file` field in quick aspect ratio

**Files:**
- Modify: `cpp/src/services/config_service.cpp:219-271` (currentAspectRatio + applyQuickAspectRatio)

Aspect ratio is per-`IniPatch` (different patches in the same option could in principle target different files; in practice Dolphin uses one file per option). We group patches by their resolved file path before reading/writing.

- [ ] **Step 1: Update `currentAspectRatio()` to use per-patch file**

Find `QString ConfigService::currentAspectRatio(const QString& emuId) const` (around line 219). Replace the body with:

```cpp
QString ConfigService::currentAspectRatio(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto opts = adapter->aspectRatioOptions();
    if (opts.options.isEmpty()) return {};

    const QString fallbackPath = adapter->configFilePath();

    // Match by comparing the first patch — whichever option's first patch
    // value matches what's on disk (in its own file) is the current selection.
    for (const auto& opt : opts.options) {
        if (opt.patches.isEmpty()) continue;
        const auto& firstPatch = opt.patches.first();
        const QString path = firstPatch.file.isEmpty() ? fallbackPath : firstPatch.file;
        if (path.isEmpty()) continue;

        IniFile ini;
        ini.load(path);
        QString val = ini.value(firstPatch.section, firstPatch.key);
        if (val == firstPatch.value)
            return opt.label;
    }
    return opts.defaultLabel;
}
```

- [ ] **Step 2: Update `applyQuickAspectRatio()` to group patches by file**

Find `void ConfigService::applyQuickAspectRatio(const QVariantMap& choices)` (around line 244). Replace the body with:

```cpp
void ConfigService::applyQuickAspectRatio(const QVariantMap& choices) {
    for (auto it = choices.constBegin(); it != choices.constEnd(); ++it) {
        const QString& emuId = it.key();
        const QString label = it.value().toString();

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        auto opts = adapter->aspectRatioOptions();
        const QString fallbackPath = adapter->configFilePath();

        for (const auto& opt : opts.options) {
            if (opt.label != label) continue;

            // Group patches by file so we load each file once.
            QMap<QString, QVector<IniPatch>> byFile;
            for (const auto& patch : opt.patches) {
                const QString path = patch.file.isEmpty() ? fallbackPath : patch.file;
                if (path.isEmpty()) continue;
                byFile[path].append(patch);
            }

            for (auto fit = byFile.constBegin(); fit != byFile.constEnd(); ++fit) {
                IniFile ini;
                ini.load(fit.key());
                for (const auto& patch : fit.value())
                    ini.setValue(patch.section, patch.key, patch.value);
                ini.save(fit.key());
            }
            break;
        }
    }
    emit statusMessage("Aspect ratio settings saved.");
}
```

- [ ] **Step 3: Make sure `<QMap>` is included**

At the top of `config_service.cpp`, ensure `#include <QMap>` is present. Add it under the existing `#include`s if missing.

- [ ] **Step 4: Build**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -10`
Expected: clean compile.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/config_service.cpp
git commit -m "ConfigService: honor IniPatch::file in quick aspect ratio path"
```

---

## Task 4: Test the framework extension

**Files:**
- Create: `cpp/tests/test_quick_settings_file_field.cpp`
- Modify: `cpp/CMakeLists.txt` (add new test target)

Verifies that the `file` field actually causes ConfigService to read/write the alternate file rather than `configFilePath()`. Uses a minimal test adapter that points the resolution at a separate temporary INI file.

- [ ] **Step 1: Write the test file**

Create `cpp/tests/test_quick_settings_file_field.cpp` with:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "adapters/emulator_adapter.h"
#include "adapters/adapter_registry.h"
#include "services/config_service.h"
#include "core/manifest.h"

namespace {
class FileFieldTestAdapter : public EmulatorAdapter {
public:
    QString mainFile;
    QString altFile;

    bool ensureConfig(const EmulatorManifest&, const QString&, const QString&) override { return true; }
    QString resolveExecutable(const EmulatorManifest&, const QString&) override { return {}; }
    QString configFilePath() const override { return mainFile; }

    ResolutionOptions resolutionOptions() const override {
        return {"Settings", "InternalResolution",
                {{"1x", "1"}, {"2x", "2"}}, "1", altFile};
    }

    AspectRatioOptions aspectRatioOptions() const override {
        return {{
            {"Auto", {{"Settings", "AspectRatio", "0", altFile}}},
            {"16:9", {{"Settings", "AspectRatio", "1", altFile}}},
        }, "Auto"};
    }
};
}

class TestQuickSettingsFileField : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmp_;
    FileFieldTestAdapter* adapter_ = nullptr;
    ConfigService* svc_ = nullptr;

private slots:
    void initTestCase() {
        QVERIFY(tmp_.isValid());

        auto adapter = std::make_unique<FileFieldTestAdapter>();
        adapter->mainFile = tmp_.path() + "/main.ini";
        adapter->altFile  = tmp_.path() + "/alt.ini";

        // Seed both files with section headers so IniFile loads them.
        for (const QString& path : {adapter->mainFile, adapter->altFile}) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[Settings]\n");
            f.close();
        }

        adapter_ = adapter.get();
        AdapterRegistry::instance().registerAdapter("filefieldtest", std::move(adapter));
        // Quick-settings paths only consult AdapterRegistry; nullptr loader is safe.
        svc_ = new ConfigService(/*loader=*/nullptr, /*parent=*/this);
    }

    void resolutionWritesToAltFile() {
        svc_->applyQuickResolution({{"filefieldtest", "2"}});

        // Main file should NOT contain the resolution key.
        QFile main(adapter_->mainFile);
        QVERIFY(main.open(QIODevice::ReadOnly));
        QString mainContent = QString::fromUtf8(main.readAll());
        QVERIFY2(!mainContent.contains("InternalResolution"),
                 "Resolution should not have been written to configFilePath()");

        // Alt file SHOULD contain it.
        QFile alt(adapter_->altFile);
        QVERIFY(alt.open(QIODevice::ReadOnly));
        QString altContent = QString::fromUtf8(alt.readAll());
        QVERIFY2(altContent.contains("InternalResolution = 2"),
                 "Resolution should have been written to ResolutionOptions::file");
    }

    void resolutionReadsFromAltFile() {
        QString val = svc_->currentResolution("filefieldtest");
        QCOMPARE(val, QString("2"));
    }

    void aspectRatioWritesToAltFile() {
        svc_->applyQuickAspectRatio({{"filefieldtest", "16:9"}});

        QFile alt(adapter_->altFile);
        QVERIFY(alt.open(QIODevice::ReadOnly));
        QString altContent = QString::fromUtf8(alt.readAll());
        QVERIFY2(altContent.contains("AspectRatio = 1"),
                 "Aspect should have been written to IniPatch::file");
    }

    void aspectRatioReadsFromAltFile() {
        QString label = svc_->currentAspectRatio("filefieldtest");
        QCOMPARE(label, QString("16:9"));
    }
};

QTEST_MAIN(TestQuickSettingsFileField)
#include "test_quick_settings_file_field.moc"
```

- [ ] **Step 2: Add the test to CMakeLists.txt**

Open `cpp/CMakeLists.txt`. Find the existing `test_ppsspp_schema` block (around line 492). Below it, add:

```cmake
add_executable(test_quick_settings_file_field
    tests/test_quick_settings_file_field.cpp
    src/services/config_service.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/pcsx2_adapter.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
    src/core/database.cpp
    src/core/manifest_loader.cpp
)
set_target_properties(test_quick_settings_file_field PROPERTIES AUTOMOC ON)
target_include_directories(test_quick_settings_file_field PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_quick_settings_file_field PRIVATE Qt6::Core Qt6::Sql Qt6::Test chdr-static)
add_test(NAME QuickSettingsFileField COMMAND test_quick_settings_file_field)
```

- [ ] **Step 3: Configure + build the new test target**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" 2>&1 | tail -10`
Then: `cd cpp && cmake --build build --target test_quick_settings_file_field 2>&1 | tail -10`
Expected: clean build.

If a link error names a missing symbol from a source not listed above (e.g. `EmulatorService::*`), add the missing source file to the test's SOURCES list. The set above mirrors `test_rom_scanner` which links the broadest set of helpers; if it's not enough, copy more from the `${PROJECT_NAME}` SOURCES list at the top of `CMakeLists.txt`.

- [ ] **Step 4: Run the test**

Run: `cd cpp/build && ctest --test-dir . -R QuickSettingsFileField --output-on-failure 2>&1 | tail -20`
Expected: PASS for all 4 cases.

- [ ] **Step 5: Run the full test suite to confirm no regression**

Run: `cd cpp/build && ctest --output-on-failure 2>&1 | tail -30`
Expected: all tests still pass. If anything red flags, the framework change leaked somewhere — investigate before committing.

- [ ] **Step 6: Commit**

```bash
git add cpp/tests/test_quick_settings_file_field.cpp cpp/CMakeLists.txt
git commit -m "Tests: verify ResolutionOptions::file / IniPatch::file route to alternate file"
```

---

## Task 5: Manifest

**Files:**
- Create: `manifests/dolphin.json`

- [ ] **Step 1: Write the manifest**

Create `manifests/dolphin.json`:

```json
{
  "id": "dolphin",
  "name": "Dolphin",
  "description": "Dolphin is a GameCube and Wii emulator. Play GC/Wii games in HD with save states, controller support, and per-game configurations.",
  "systems": ["gc", "wii"],
  "github_repo": "dolphin-emu/dolphin",
  "executable": "DolphinQt",
  "install_folder": "dolphin",
  "rom_extensions": ["iso", "gcm", "gcz", "ciso", "wbfs", "rvz", "wad", "wia", "nkit", "m3u"],
  "launch_args": ["-b", "-e", "{rom_path}"]
}
```

- [ ] **Step 2: Commit**

```bash
git add manifests/dolphin.json
git commit -m "Add Dolphin manifest (GameCube + Wii)"
```

---

## Task 6: DolphinAdapter header

**Files:**
- Create: `cpp/src/adapters/dolphin_adapter.h`

- [ ] **Step 1: Write the header**

Create `cpp/src/adapters/dolphin_adapter.h`:

```cpp
#pragma once

#include "emulator_adapter.h"

/**
 * DolphinAdapter — adapter for Dolphin (GameCube + Wii).
 *
 * Dolphin spreads its config across multiple INI files under User/Config/:
 *   - Dolphin.ini       (Interface, Display, Core, General — the "main" file)
 *   - GFX.ini           (graphics — resolution + aspect live here)
 *   - GCPadNew.ini      (GameCube controller bindings)
 *   - WiimoteNew.ini    (Wii Remote bindings)
 *   - Hotkeys.ini       (native hotkeys; we clear conflicting ones)
 *   - RetroAchievements.ini (RA settings)
 *
 * configFilePath() returns the path to Dolphin.ini. The settings UI exposes
 * only Dolphin.ini settings in v1 (graphics page deferred to native UI).
 * Resolution and aspect ratio are routed to GFX.ini via the framework's
 * ResolutionOptions::file / IniPatch::file overrides.
 *
 * Controllers: default profiles are baked into GCPadNew.ini and WiimoteNew.ini
 * at install time (create-only — never overwritten on subsequent launches).
 * controllerTypes() returns empty so Dolphin does not appear in the in-app
 * controller mapping page; users remap through Dolphin's native UI.
 *
 * In-game menu: pause-on-focus-loss only (no save-on-exit, no resume).
 */
class DolphinAdapter : public EmulatorAdapter {
public:
    bool ensureConfig(const EmulatorManifest& manifest,
                      const QString& biosPath,
                      const QString& savesPath) override;

    QString resolveExecutable(const EmulatorManifest& manifest,
                              const QString& installPath) override;

    QVector<SettingDef> settingsSchema() const override;
    QString configFilePath() const override;

    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
    ResolutionOptions resolutionOptions() const override;
    AspectRatioOptions aspectRatioOptions() const override;

    // Controllers: empty (no in-app remap UI for v1) — defaults baked into INIs.
    QVector<ControllerTypeDef> controllerTypes() const override { return {}; }
    QVector<BindingDef> controllerBindingDefs() const override { return {}; }

    QVector<HotkeyDef> hotkeyBindingDefs() const override { return {}; }

    bool supportsRetroAchievements() const override { return true; }
    bool supportsSaveOnExit() const override { return false; }

    void patchRetroAchievements(const QString& username, const QString& token,
                                bool enabled, bool hardcore,
                                bool notifications, bool sounds) override;

    QVector<AssetMatchRule> assetMatchRules() const override;

private:
    /** On macOS: path inside .app bundle (Contents/MacOS/). Otherwise: emulators dir. */
    static QString portableDir();

    /** Absolute path to {portableDir}/User/Config/. */
    static QString userConfigDir();

    /** Per-INI-file path helpers that resolve under userConfigDir(). */
    static QString dolphinIniPath();
    static QString gfxIniPath();
    static QString gcpadIniPath();
    static QString wiimoteIniPath();
    static QString hotkeysIniPath();
    static QString retroAchievementsIniPath();

    /** Patch Dolphin.ini with our embedding-critical keys. */
    bool patchDolphinIni(const QString& dataRootGc, const QString& dataRootWii);

    /** Patch GFX.ini with VSync and reasonable defaults. */
    bool patchGfxIni();

    /** Write GCPadNew.ini default profile if file does not yet exist. */
    bool writeGcPadDefaultsIfMissing();

    /** Write WiimoteNew.ini default profile if file does not yet exist. */
    bool writeWiimoteDefaultsIfMissing();

    /** Patch Hotkeys.ini to clear hotkeys that would conflict with our overlay. */
    bool patchHotkeysIni();
};
```

- [ ] **Step 2: Verify the header compiles**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -5`
Expected: still builds (the header is not yet referenced by anything, so this is just a syntax check).

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.h
git commit -m "Add DolphinAdapter header"
```

---

## Task 7: DolphinAdapter cpp scaffold + register + CMakeLists

**Files:**
- Create: `cpp/src/adapters/dolphin_adapter.cpp` (stubs only — fleshed out in later tasks)
- Modify: `cpp/src/adapters/adapter_registry.cpp` (register DolphinAdapter)
- Modify: `cpp/CMakeLists.txt:36-40, 133-137` (add adapter sources/headers)

- [ ] **Step 1: Create dolphin_adapter.cpp scaffold**

Create `cpp/src/adapters/dolphin_adapter.cpp`:

```cpp
#include "dolphin_adapter.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>

#include "core/paths.h"

namespace {
constexpr const char* DOLPHIN_INSTALL_FOLDER = "dolphin";
}

// ============================================================================
// Path helpers
// ============================================================================

QString DolphinAdapter::portableDir() {
    const QString installPath = Paths::emulatorsDir(DOLPHIN_INSTALL_FOLDER);
#if defined(Q_OS_MACOS)
    QDir dir(installPath);
    const auto entries = dir.entryList({"*.app"}, QDir::Dirs);
    for (const auto& entry : entries) {
        QString candidate = installPath + "/" + entry + "/Contents/MacOS";
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return installPath;
#else
    return installPath;
#endif
}

QString DolphinAdapter::userConfigDir() {
    return portableDir() + "/User/Config";
}

QString DolphinAdapter::dolphinIniPath()         { return userConfigDir() + "/Dolphin.ini"; }
QString DolphinAdapter::gfxIniPath()             { return userConfigDir() + "/GFX.ini"; }
QString DolphinAdapter::gcpadIniPath()           { return userConfigDir() + "/GCPadNew.ini"; }
QString DolphinAdapter::wiimoteIniPath()         { return userConfigDir() + "/WiimoteNew.ini"; }
QString DolphinAdapter::hotkeysIniPath()         { return userConfigDir() + "/Hotkeys.ini"; }
QString DolphinAdapter::retroAchievementsIniPath(){ return userConfigDir() + "/RetroAchievements.ini"; }

QString DolphinAdapter::configFilePath() const {
    return dolphinIniPath();
}

// ============================================================================
// Executable resolution
// ============================================================================

QString DolphinAdapter::resolveExecutable(const EmulatorManifest& manifest,
                                          const QString& installPath) {
    return resolveExecutableInDir(manifest, installPath, "DolphinQt");
}

// ============================================================================
// Asset matching
// ============================================================================

QVector<EmulatorAdapter::AssetMatchRule> DolphinAdapter::assetMatchRules() const {
#if defined(Q_OS_MACOS)
    return {
        {{"macos", "universal"}, ".dmg"},
        {{"macos"},              ".dmg"},
        {{"mac"},                ".dmg"},
    };
#elif defined(Q_OS_WIN)
    return {
        {{"windows", "x64"}, ".zip"},
        {{"windows"},        ".zip"},
    };
#else
    return {
        {{"linux"}, ".AppImage"},
        {{"linux"}, ".tar.xz"},
    };
#endif
}

// ============================================================================
// BIOS files (GameCube IPL — optional; SkipIPL is set in Dolphin.ini)
// ============================================================================

QVector<BiosDef> DolphinAdapter::biosFiles() const {
    return {
        {"GC/USA/IPL.bin", "GameCube IPL (NTSC-U)", false, ""},
        {"GC/EUR/IPL.bin", "GameCube IPL (PAL)",     false, ""},
        {"GC/JAP/IPL.bin", "GameCube IPL (NTSC-J)",  false, ""},
    };
}

// ============================================================================
// Path defs (per-system data dirs under emulators/dolphin/{gc,wii}/)
// ============================================================================

QVector<PathDef> DolphinAdapter::pathsDefs() const {
    // Dolphin doesn't expose individual folder-path keys for savestates etc.
    // (it derives them from User dir). We declare them so RetroNest creates
    // the per-system data directories on first launch — Dolphin reads them
    // because we point its ISOPath0/ISOPath1 keys at gc/ and wii/ in
    // patchDolphinIni(), and the standard subfolders are created by Dolphin
    // itself the first time it writes savestates/screenshots/etc.
    return {
        {"Save States",  "", "", "savestates",  PathBase::EmulatorData},
        {"Screenshots",  "", "", "screenshots", PathBase::EmulatorData},
        {"Cheats",       "", "", "cheats",      PathBase::EmulatorData},
        {"Textures",     "", "", "textures",    PathBase::EmulatorData},
        {"Cache",        "", "", "cache",       PathBase::EmulatorData},
    };
}

// ============================================================================
// Resolution / aspect ratio (route writes to GFX.ini via the file field)
// ============================================================================

ResolutionOptions DolphinAdapter::resolutionOptions() const {
    return {
        "Settings", "InternalResolution",
        {
            {"Native (1x)",   "1"},
            {"2x (~720p)",    "2"},
            {"3x (~1080p)",   "3"},
            {"4x (~1440p)",   "4"},
            {"5x (~1800p)",   "5"},
            {"6x (~4K)",      "6"},
        },
        "1",
        gfxIniPath(),
    };
}

AspectRatioOptions DolphinAdapter::aspectRatioOptions() const {
    const QString gfx = gfxIniPath();
    return {
        {
            {"Auto",       {{"Settings", "AspectRatio", "0", gfx}}},
            {"Force 16:9", {{"Settings", "AspectRatio", "1", gfx}}},
            {"Force 4:3",  {{"Settings", "AspectRatio", "2", gfx}}},
            {"Stretch",    {{"Settings", "AspectRatio", "3", gfx}}},
        },
        "Auto"
    };
}

// ============================================================================
// Settings schema — Dolphin.ini only for v1 (graphics page deferred)
// ============================================================================

QVector<SettingDef> DolphinAdapter::settingsSchema() const {
    // Filled in by Task 12.
    return {};
}

// ============================================================================
// ensureConfig — multi-file
// ============================================================================

bool DolphinAdapter::ensureConfig(const EmulatorManifest& manifest,
                                   const QString& biosPath,
                                   const QString& savesPath) {
    Q_UNUSED(manifest); Q_UNUSED(biosPath); Q_UNUSED(savesPath);
    // Filled in by Tasks 9-11.
    return true;
}

bool DolphinAdapter::patchDolphinIni(const QString&, const QString&) { return true; }
bool DolphinAdapter::patchGfxIni() { return true; }
bool DolphinAdapter::writeGcPadDefaultsIfMissing() { return true; }
bool DolphinAdapter::writeWiimoteDefaultsIfMissing() { return true; }
bool DolphinAdapter::patchHotkeysIni() { return true; }

// ============================================================================
// RetroAchievements
// ============================================================================

void DolphinAdapter::patchRetroAchievements(const QString&, const QString&,
                                             bool, bool, bool, bool) {
    // Filled in by Task 13.
}
```

- [ ] **Step 2: Register the adapter**

Open `cpp/src/adapters/adapter_registry.cpp`. After the existing `#include "ppsspp_adapter.h"` (line 4), add:

```cpp
#include "dolphin_adapter.h"
```

Inside `registerBuiltinAdapters()` (around line 13), after the `ppsspp` line, add:

```cpp
    registerAdapter("dolphin", std::make_unique<DolphinAdapter>());
```

- [ ] **Step 3: Add the adapter to CMakeLists.txt — main target SOURCES + HEADERS**

Open `cpp/CMakeLists.txt`. Find the SOURCES list around line 36-40 — add `src/adapters/dolphin_adapter.cpp` after `src/adapters/ppsspp_adapter.cpp`.

Find the HEADERS list around line 133-137 — add `src/adapters/dolphin_adapter.h` after `src/adapters/ppsspp_adapter.h`.

- [ ] **Step 4: Add the adapter to the test SOURCES that link adapters**

Several test executables list every adapter source. Update each to include `src/adapters/dolphin_adapter.cpp`:

- `test_format_binding` (around line 433-447) — add to SOURCES list.
- `test_rom_scanner` (around line 449-466) — add to SOURCES list.

(The other tests don't link adapters, so they don't need updating. The new `test_quick_settings_file_field` from Task 4 already lists every adapter — leave it as-is.)

- [ ] **Step 5: Reconfigure + clean build**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" 2>&1 | tail -5`
Then: `cd cpp && cmake --build build 2>&1 | tail -15`
Expected: full clean build, no errors. The Dolphin adapter is now wired into the binary as a no-op stub.

- [ ] **Step 6: Run all tests to confirm no regression**

Run: `cd cpp/build && ctest --output-on-failure 2>&1 | tail -30`
Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp cpp/src/adapters/adapter_registry.cpp cpp/CMakeLists.txt
git commit -m "Wire DolphinAdapter into build, registry, and adapter-linked tests"
```

---

## Task 8: System mappings (theme display names, ScreenScraper IDs, RA console IDs)

**Files:**
- Modify: `cpp/src/ui/theme_context.cpp` (`systemDisplayNames`)
- Modify: `cpp/src/services/scraper.cpp` (`systemToScreenScraperId`)
- Modify: `cpp/src/services/ra_client.cpp` (`consoleIdMapping`)

- [ ] **Step 1: Find and update systemDisplayNames**

Run: `grep -n 'systemDisplayNames' cpp/src/ui/theme_context.cpp`
Open the file and add `{"gc", "GameCube"}` and `{"wii", "Wii"}` entries to the map. The exact location depends on the file's current layout — match the style of the existing entries (e.g. `{"psx", "PlayStation"}`).

- [ ] **Step 2: Find and update systemToScreenScraperId**

Run: `grep -n 'systemToScreenScraperId' cpp/src/services/scraper.cpp`
Add: `{"gc", 13}` and `{"wii", 16}` (canonical ScreenScraper system IDs for GameCube and Wii).

- [ ] **Step 3: Find and update consoleIdMapping**

Run: `grep -n 'consoleIdMapping' cpp/src/services/ra_client.cpp`
Add: `{"gc", 16}` and `{"wii", 19}` (canonical RetroAchievements console IDs for GameCube and Wii).

- [ ] **Step 4: Build to verify**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -5`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/theme_context.cpp cpp/src/services/scraper.cpp cpp/src/services/ra_client.cpp
git commit -m "System mappings: register gc/wii in display names, ScreenScraper, RetroAchievements"
```

---

## Task 9: Implement `ensureConfig()` — Phase 1: portable.txt + Dolphin.ini

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp` (replace stub `ensureConfig` and `patchDolphinIni`)

- [ ] **Step 1: Replace the `ensureConfig` stub**

Find the stub `bool DolphinAdapter::ensureConfig(...)` body in `dolphin_adapter.cpp` and replace it with:

```cpp
bool DolphinAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                   const QString& /*biosPath*/,
                                   const QString& /*savesPath*/) {
    // 1) Portable marker so Dolphin reads from User/ next to the binary.
    if (!ensurePortableMarker(portableDir(), "Dolphin"))
        return false;

    // 2) Ensure User/Config exists.
    if (!QDir().mkpath(userConfigDir())) {
        qWarning() << "[Dolphin] Failed to create" << userConfigDir();
        return false;
    }

    // 3) Compute per-system data roots used in Dolphin.ini ISOPath* keys.
    const QString dataRootGc  = Paths::emulatorDataDir(DOLPHIN_INSTALL_FOLDER, "gc");
    const QString dataRootWii = Paths::emulatorDataDir(DOLPHIN_INSTALL_FOLDER, "wii");
    QDir().mkpath(dataRootGc);
    QDir().mkpath(dataRootWii);

    // 4) Patch each config file. Each helper is independently idempotent
    //    and logs its own failures — keep going so a single bad file
    //    doesn't block a launch entirely.
    bool ok = true;
    ok &= patchDolphinIni(dataRootGc, dataRootWii);
    ok &= patchGfxIni();
    ok &= writeGcPadDefaultsIfMissing();
    ok &= writeWiimoteDefaultsIfMissing();
    ok &= patchHotkeysIni();
    return ok;
}
```

- [ ] **Step 2: Implement `patchDolphinIni`**

Replace the `patchDolphinIni` stub with:

```cpp
bool DolphinAdapter::patchDolphinIni(const QString& dataRootGc, const QString& dataRootWii) {
    const QString path = dolphinIniPath();
    QString content;

    if (QFile::exists(path)) {
        if (!readConfigFile(path, content, "Dolphin"))
            return false;
    } else {
        content = "";  // patchIniKeys will append all sections + keys
    }

    const QVector<IniKeyPatch> patches = {
        // Interface — pause-on-focus, no exit confirmation, no system-cursor flicker.
        {"Interface", "PauseOnFocusLost",  "True"},
        {"Interface", "ConfirmStop",        "False"},
        {"Interface", "HideCursor",         "True"},

        // Display — fullscreen render, render to main window (avoid second window).
        {"Display", "Fullscreen",   "True"},
        {"Display", "RenderToMain", "True"},

        // Core — boot directly without IPL bootrom requirement.
        {"Core", "SkipIPL",      "True"},
        {"Core", "EnableCheats", "False"},

        // General — point ISO scanning at our per-system data dirs so any
        // saves/states Dolphin writes via its own UI go to the right place.
        // Note: these are write-targets, not ROM-scan paths (RetroNest scans
        // ROMs directly — we just give Dolphin a sensible default).
        {"General", "ISOPath0",          dataRootGc},
        {"General", "ISOPath1",          dataRootWii},
        {"General", "ISOPaths",          "2"},
        {"General", "RecursiveISOPaths", "True"},
    };

    if (patchIniKeys(content, patches))
        return writeConfigFile(path, content, "Dolphin");
    // Even if no patches changed, ensure the file exists on disk.
    if (!QFile::exists(path))
        return writeConfigFile(path, content, "Dolphin");
    return true;
}
```

- [ ] **Step 3: Build**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -5`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp
git commit -m "Dolphin ensureConfig: portable marker + Dolphin.ini patches

Sets PauseOnFocusLost, fullscreen, RenderToMain, SkipIPL, and points
ISOPath0/1 at the per-system data dirs."
```

---

## Task 10: Implement `ensureConfig()` — Phase 2: GFX.ini + Hotkeys.ini

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp` (replace `patchGfxIni` + `patchHotkeysIni` stubs)

- [ ] **Step 1: Implement `patchGfxIni`**

Replace the `patchGfxIni` stub with:

```cpp
bool DolphinAdapter::patchGfxIni() {
    const QString path = gfxIniPath();
    QString content;

    if (QFile::exists(path)) {
        if (!readConfigFile(path, content, "Dolphin"))
            return false;
    } else {
        content = "";
    }

    const QVector<IniKeyPatch> patches = {
        {"Hardware", "VSync", "True"},
        // AspectRatio + InternalResolution are user-tunable through the wizard;
        // we only seed them if the file is fresh.
    };

    bool wrote = false;
    if (patchIniKeys(content, patches)) {
        if (!writeConfigFile(path, content, "Dolphin"))
            return false;
        wrote = true;
    }

    // Seed defaults only if these keys are absent (avoid overwriting user choices).
    if (!content.contains("AspectRatio") || !content.contains("InternalResolution")) {
        QVector<IniKeyPatch> seedPatches;
        if (!content.contains("AspectRatio "))
            seedPatches.append({"Settings", "AspectRatio", "0"});
        if (!content.contains("InternalResolution"))
            seedPatches.append({"Settings", "InternalResolution", "1"});
        if (!seedPatches.isEmpty() && patchIniKeys(content, seedPatches))
            return writeConfigFile(path, content, "Dolphin");
    }

    if (!wrote && !QFile::exists(path))
        return writeConfigFile(path, content, "Dolphin");
    return true;
}
```

- [ ] **Step 2: Implement `patchHotkeysIni`**

Replace the `patchHotkeysIni` stub with:

```cpp
bool DolphinAdapter::patchHotkeysIni() {
    const QString path = hotkeysIniPath();
    QString content;

    if (QFile::exists(path)) {
        if (!readConfigFile(path, content, "Dolphin"))
            return false;
    } else {
        // Create an empty file with just the [Hotkeys] section so Dolphin
        // doesn't auto-populate defaults that conflict with our overlay.
        content = "[Hotkeys]\n";
    }

    // Clear native hotkeys that compete with our Cmd+Esc overlay or
    // automatic save-on-exit logic. Dolphin's expression parser tolerates
    // empty values (returns 0/false, no crash).
    const QVector<IniKeyPatch> patches = {
        {"Hotkeys", "General/Toggle Pause",       ""},
        {"Hotkeys", "General/Open",                ""},
        {"Hotkeys", "General/Exit",                ""},
        {"Hotkeys", "Save State/Save State Slot 1", ""},
        {"Hotkeys", "Load State/Load State Slot 1", ""},
    };

    if (patchIniKeys(content, patches))
        return writeConfigFile(path, content, "Dolphin");
    if (!QFile::exists(path))
        return writeConfigFile(path, content, "Dolphin");
    return true;
}
```

- [ ] **Step 3: Build**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -5`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp
git commit -m "Dolphin ensureConfig: GFX.ini VSync + Hotkeys.ini conflict clearing"
```

---

## Task 11: Implement `ensureConfig()` — Phase 3: GCPadNew.ini and WiimoteNew.ini defaults

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp` (replace `writeGcPadDefaultsIfMissing` + `writeWiimoteDefaultsIfMissing` stubs)

These are create-only — once the file exists, we never overwrite it (so user customizations made through Dolphin's native UI persist). Bindings use Dolphin's backtick expression-language format (`` `SDL/0/<device>:<element>` ``).

- [ ] **Step 1: Implement `writeGcPadDefaultsIfMissing`**

Replace the `writeGcPadDefaultsIfMissing` stub with:

```cpp
bool DolphinAdapter::writeGcPadDefaultsIfMissing() {
    const QString path = gcpadIniPath();
    if (QFile::exists(path))
        return true;  // never overwrite — user may have customized via Dolphin's UI

    // Default GCPad1 profile: Standard Controller mapped to SDL gamepad 0.
    // The literal `<device>` placeholder below is replaced at write time with
    // the SDL controller display name pattern. We use the GameController
    // catch-all since most users plug in PS/Xbox-style pads which Dolphin
    // exposes via its SDL backend with a "Wireless Controller" or similar name.
    //
    // If a user has an unusual gamepad that doesn't match this pattern,
    // they can re-map through Dolphin's native UI (which we leave untouched
    // on subsequent launches).
    const char* profile = R"INI([GCPad1]
Device = SDL/0/Wireless Controller
Buttons/A = `Button S`
Buttons/B = `Button E`
Buttons/X = `Button W`
Buttons/Y = `Button N`
Buttons/Z = `Shoulder R`
Buttons/Start = `Start`
D-Pad/Up = `Pad N`
D-Pad/Down = `Pad S`
D-Pad/Left = `Pad W`
D-Pad/Right = `Pad E`
Main Stick/Up = `Left Y-`
Main Stick/Down = `Left Y+`
Main Stick/Left = `Left X-`
Main Stick/Right = `Left X+`
C-Stick/Up = `Right Y-`
C-Stick/Down = `Right Y+`
C-Stick/Left = `Right X-`
C-Stick/Right = `Right X+`
Triggers/L = `Trigger L`
Triggers/R = `Trigger R`
Triggers/L-Analog = `Trigger L`
Triggers/R-Analog = `Trigger R`
Rumble/Motor = `Motor`
)INI";

    QString content = QString::fromUtf8(profile);
    return writeConfigFile(path, content, "Dolphin");
}
```

- [ ] **Step 2: Implement `writeWiimoteDefaultsIfMissing`**

Replace the `writeWiimoteDefaultsIfMissing` stub with:

```cpp
bool DolphinAdapter::writeWiimoteDefaultsIfMissing() {
    const QString path = wiimoteIniPath();
    if (QFile::exists(path))
        return true;  // never overwrite

    // Default Wiimote1 profile: emulated Wiimote (no real Wii Remote needed),
    // sideways orientation, no extension. Maps to SDL gamepad 0.
    // This is a minimal "playable" profile — full Wii Remote IR/motion
    // mapping is out of scope for v1; users who need it remap via Dolphin's
    // native UI.
    const char* profile = R"INI([Wiimote1]
Source = 1
Device = SDL/0/Wireless Controller
Options/Sideways Wiimote = True
Buttons/A = `Button S`
Buttons/B = `Button E`
Buttons/1 = `Button W`
Buttons/2 = `Button N`
Buttons/- = `Back`
Buttons/+ = `Start`
Buttons/Home = `Guide`
D-Pad/Up = `Pad N`
D-Pad/Down = `Pad S`
D-Pad/Left = `Pad W`
D-Pad/Right = `Pad E`
Shake/X = `Shoulder L`
Shake/Y = `Shoulder L`
Shake/Z = `Shoulder L`
IR/Up = `Right Y-`
IR/Down = `Right Y+`
IR/Left = `Right X-`
IR/Right = `Right X+`
Tilt/Forward = `Left Y-`
Tilt/Backward = `Left Y+`
Tilt/Left = `Left X-`
Tilt/Right = `Left X+`
Rumble/Motor = `Motor`
)INI";

    QString content = QString::fromUtf8(profile);
    return writeConfigFile(path, content, "Dolphin");
}
```

- [ ] **Step 3: Build**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -5`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp
git commit -m "Dolphin ensureConfig: bake default GCPad / Wiimote profiles (create-only)"
```

---

## Task 12: Implement `settingsSchema()` — Dolphin.ini settings

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp` (replace `settingsSchema` stub)

V1 scope: only settings that live in `Dolphin.ini` (which is what `configFilePath()` returns). Graphics settings live in `GFX.ini` and are accessed via "Open Native Settings".

- [ ] **Step 1: Implement `settingsSchema`**

Replace the `settingsSchema` stub with:

```cpp
QVector<SettingDef> DolphinAdapter::settingsSchema() const {
    // Field order: category, subcategory, group, section, key, label, tooltip,
    //              type, defaultValue, options, minVal, maxVal, step, layout, suffix
    // All values use Dolphin's True/False capitalization (Common/StringUtil.cpp:289-292).

    const QVector<QPair<QString,QString>> audioBackends = {
        {"Cubeb",   "Cubeb"},
        {"OpenAL",  "OpenAL"},
        {"Null",    "Null"},
    };

    const QVector<QPair<QString,QString>> cpuCores = {
        {"Interpreter (slow, accurate)",  "0"},
        {"Cached Interpreter",             "5"},
        {"JIT Recompiler (recommended)",   "1"},
        {"JITARM64 (Apple Silicon)",       "4"},
    };

    return {
        // ─── Interface ───────────────────────────────────────
        {"Interface", "", "", "Interface", "PauseOnFocusLost",
         "Pause When Window Loses Focus",
         "Pauses emulation automatically when the RetroNest window loses focus. "
         "Required for the in-game overlay to work cleanly.",
         SettingDef::Bool, "True"},

        {"Interface", "", "", "Interface", "ConfirmStop",
         "Confirm Before Stopping Emulation",
         "Show a confirmation dialog when stopping a game from Dolphin's UI. "
         "Disabled by default so RetroNest's own exit flow is uninterrupted.",
         SettingDef::Bool, "False"},

        {"Interface", "", "", "Interface", "HideCursor",
         "Hide Cursor During Gameplay",
         "Hides the mouse cursor while a game is running.",
         SettingDef::Bool, "True"},

        // ─── Audio (DSP) ─────────────────────────────────────
        {"Audio", "", "", "DSP", "Backend",
         "Audio Backend",
         "Sound output backend. Cubeb is recommended on macOS.",
         SettingDef::Combo, "Cubeb", audioBackends},

        {"Audio", "", "", "DSP", "Volume",
         "Volume",
         "Master output volume (0-100).",
         SettingDef::Int, "100", {}, 0, 100, 1, "slider", "%"},

        {"Audio", "", "", "DSP", "EnableJIT",
         "Enable DSP JIT",
         "Enables the DSP JIT recompiler. Significant performance improvement; "
         "leave on unless debugging.",
         SettingDef::Bool, "True"},

        // ─── Core ────────────────────────────────────────────
        {"Core", "", "", "Core", "CPUCore",
         "CPU Core",
         "The CPU emulation backend. JIT is required for full-speed gameplay.",
         SettingDef::Combo, "1", cpuCores},

        {"Core", "", "", "Core", "SkipIPL",
         "Skip GameCube Boot Animation",
         "Skips the GameCube IPL boot sequence and starts the game directly. "
         "When disabled, requires IPL.bin in the BIOS folder.",
         SettingDef::Bool, "True"},

        {"Core", "", "", "Core", "EnableCheats",
         "Enable Cheats",
         "Enables AR/Gecko cheat code processing. Off by default for safety.",
         SettingDef::Bool, "False"},

        {"Core", "", "", "Core", "OverclockEnable",
         "Enable CPU Overclocking",
         "Allows the slider below to scale the emulated CPU's clock rate. "
         "Some games run smoother with overclocking; others crash.",
         SettingDef::Bool, "False"},

        {"Core", "", "", "Core", "Overclock",
         "Overclock Multiplier",
         "Multiplier applied to the emulated CPU's clock when overclocking is enabled. "
         "1.0 = native, 1.5 = +50%.",
         SettingDef::Float, "1", {}, 0.5, 4.0, 0.05, "slider", "x"},
    };
}
```

- [ ] **Step 2: Build**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -5`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp
git commit -m "Dolphin settingsSchema: Interface / Audio / Core (Dolphin.ini scope)"
```

---

## Task 13: Implement `patchRetroAchievements`

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp` (replace `patchRetroAchievements` stub)

Dolphin's RA settings live in `RetroAchievements.ini` `[Achievements]`, distinct from `configFilePath()`. Verified against `Source/Core/Core/Config/AchievementSettings.cpp:12,16`. Bool format is `True`/`False`.

- [ ] **Step 1: Implement `patchRetroAchievements`**

Replace the `patchRetroAchievements` stub with:

```cpp
void DolphinAdapter::patchRetroAchievements(const QString& /*username*/, const QString& /*token*/,
                                              bool enabled, bool hardcore,
                                              bool notifications, bool sounds) {
    const QString path = retroAchievementsIniPath();
    QString content;

    if (QFile::exists(path)) {
        if (!readConfigFile(path, content, "Dolphin"))
            return;
    } else {
        content = "[Achievements]\n";
    }

    const QString trueVal = "True";
    const QString falseVal = "False";

    const QVector<IniKeyPatch> patches = {
        {"Achievements", "Enabled",          enabled       ? trueVal : falseVal},
        {"Achievements", "HardcoreEnabled",  hardcore      ? trueVal : falseVal},
        {"Achievements", "ProgressEnabled",   notifications ? trueVal : falseVal},
        {"Achievements", "SoundEnabled",      sounds        ? trueVal : falseVal},
    };

    if (patchIniKeys(content, patches))
        writeConfigFile(path, content, "Dolphin");
}
```

Note: the `ProgressEnabled` and `SoundEnabled` key names are inferred from Dolphin's RA section conventions. Verify these against `Source/Core/Core/Config/AchievementSettings.cpp` during implementation; if they differ, replace with the actual key names. If a key doesn't exist in Dolphin's source, omit it from the patches vector — patching a non-existent key just creates one Dolphin will ignore.

- [ ] **Step 2: Build**

Run: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -5`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp
git commit -m "Dolphin: patchRetroAchievements writes to RetroAchievements.ini [Achievements]"
```

---

## Task 14: Logo + UI registration

**Files:**
- Create: `qml/AppUI/images/dolphin-logo.png` (placeholder)
- Modify: `qml/AppUI/EmulatorLogos.js` (logo path map)
- Modify: `qml/SetupWizard/EmulatorCard.qml` (`logoForEmu()`)
- Modify: `qml/AppUI/CMakeLists.txt` (RESOURCES)
- Modify: `qml/SetupWizard/CMakeLists.txt` (RESOURCES)

- [ ] **Step 1: Create a placeholder logo**

Generate a simple 256×256 PNG placeholder for now:

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
# Use Python+Pillow if available, else copy an existing logo as a temporary placeholder.
if command -v python3 >/dev/null && python3 -c "from PIL import Image" 2>/dev/null; then
  python3 -c "
from PIL import Image, ImageDraw, ImageFont
img = Image.new('RGBA', (256,256), (40,40,40,255))
d = ImageDraw.Draw(img)
d.text((30,100), 'DOLPHIN', fill=(0,180,220,255))
img.save('qml/AppUI/images/dolphin-logo.png')
"
else
  cp qml/AppUI/images/ppsspp-logo.png qml/AppUI/images/dolphin-logo.png
fi
ls -la qml/AppUI/images/dolphin-logo.png
```

A real logo will be supplied later; the placeholder just unblocks the install grid render. If `ppsspp-logo.png` doesn't exist either, fall back to copying any existing `*-logo.png` you can find in `qml/AppUI/images/`.

- [ ] **Step 2: Add to `EmulatorLogos.js`**

Open `qml/AppUI/EmulatorLogos.js` — find the existing logo map and add `"dolphin": "qrc:/AppUI/qml/AppUI/images/dolphin-logo.png"` (match the resource path style of the existing entries).

- [ ] **Step 3: Add to `EmulatorCard.qml` `logoForEmu()`**

Open `qml/SetupWizard/EmulatorCard.qml` — find the local `logoForEmu()` function and add a case for `"dolphin"`. Match the path style of existing entries.

- [ ] **Step 4: Add the PNG to both CMakeLists RESOURCES**

In `qml/AppUI/CMakeLists.txt`, find the RESOURCES list that includes other logo PNGs and add `qml/AppUI/images/dolphin-logo.png` next to them.

In `qml/SetupWizard/CMakeLists.txt`, do the same — find the RESOURCES section that lists logo PNGs and add the Dolphin one.

If the SetupWizard logo path differs (some projects use a separate `qml/SetupWizard/images/` dir), check `EmulatorCard.qml` to see which path it expects and add the PNG to that location too.

- [ ] **Step 5: Reconfigure + build**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" 2>&1 | tail -5`
Then: `cd cpp && cmake --build build --target RetroNest 2>&1 | tail -5`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add qml/AppUI/images/dolphin-logo.png qml/AppUI/EmulatorLogos.js qml/SetupWizard/EmulatorCard.qml qml/AppUI/CMakeLists.txt qml/SetupWizard/CMakeLists.txt
git commit -m "Dolphin: register logo placeholder in install grid + setup wizard"
```

---

## Task 15: Schema test — validate Dolphin settings shape

**Files:**
- Create: `cpp/tests/test_dolphin_schema.cpp`
- Modify: `cpp/CMakeLists.txt` (add test target)

Mirrors the structure of `test_ppsspp_schema.cpp` — verifies categories, that key settings exist, and that combo/option counts are sane.

- [ ] **Step 1: Write the test file**

Create `cpp/tests/test_dolphin_schema.cpp`:

```cpp
#include <QtTest>
#include <QSet>
#include "adapters/dolphin_adapter.h"
#include "core/setting_def.h"

class TestDolphinSchema : public QObject {
    Q_OBJECT

private:
    QVector<SettingDef> schema_;

private slots:
    void initTestCase() {
        DolphinAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void testTopLevelCategories() {
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories, QSet<QString>({"Interface", "Audio", "Core"}));
    }

    void testPauseOnFocusLostExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.key == "PauseOnFocusLost") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Interface"));
        QCOMPARE(found->section, QString("Interface"));
        QCOMPARE(int(found->type), int(SettingDef::Bool));
        QCOMPARE(found->defaultValue, QString("True"));
    }

    void testAudioBackendIsCubebDefault() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "DSP" && d.key == "Backend") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QCOMPARE(found->defaultValue, QString("Cubeb"));
        QVERIFY(found->options.size() >= 2);
    }

    void testCpuCoreCombo() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "Core" && d.key == "CPUCore") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QVERIFY(found->options.size() >= 3);
    }

    void testBoolValuesAreCapitalized() {
        // Dolphin writes True/False (Common/StringUtil.cpp:289-292), not true/false.
        for (const auto& d : schema_) {
            if (d.type != SettingDef::Bool) continue;
            QVERIFY2(d.defaultValue == "True" || d.defaultValue == "False",
                     qPrintable(QString("Bool '%1' has non-capitalized default '%2'")
                                .arg(d.key, d.defaultValue)));
        }
    }

    void testResolutionOptionsTargetGfxIni() {
        DolphinAdapter adapter;
        ResolutionOptions opts = adapter.resolutionOptions();
        QVERIFY(!opts.options.isEmpty());
        QVERIFY2(!opts.file.isEmpty(),
                 "Resolution must target GFX.ini via the file field");
        QVERIFY(opts.file.endsWith("GFX.ini"));
        QCOMPARE(opts.section, QString("Settings"));
        QCOMPARE(opts.key, QString("InternalResolution"));
    }

    void testAspectRatioOptionsTargetGfxIni() {
        DolphinAdapter adapter;
        AspectRatioOptions opts = adapter.aspectRatioOptions();
        QVERIFY(!opts.options.isEmpty());
        for (const auto& opt : opts.options) {
            QVERIFY(!opt.patches.isEmpty());
            for (const auto& patch : opt.patches) {
                QVERIFY2(!patch.file.isEmpty(),
                         qPrintable(QString("Aspect '%1' patch missing file field").arg(opt.label)));
                QVERIFY(patch.file.endsWith("GFX.ini"));
            }
        }
    }
};

QTEST_MAIN(TestDolphinSchema)
#include "test_dolphin_schema.moc"
```

- [ ] **Step 2: Add to CMakeLists.txt**

In `cpp/CMakeLists.txt`, after the `test_ppsspp_schema` block, add:

```cmake
add_executable(test_dolphin_schema
    tests/test_dolphin_schema.cpp
    src/adapters/dolphin_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/iso9660_reader.cpp
    src/core/sfo_parser.cpp
    src/core/paths.cpp
)
set_target_properties(test_dolphin_schema PROPERTIES AUTOMOC ON)
target_include_directories(test_dolphin_schema PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_dolphin_schema PRIVATE Qt6::Core Qt6::Test chdr-static)
add_test(NAME DolphinSchema COMMAND test_dolphin_schema)
```

- [ ] **Step 3: Reconfigure + build the test**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" 2>&1 | tail -5`
Then: `cd cpp && cmake --build build --target test_dolphin_schema 2>&1 | tail -10`
Expected: clean build.

- [ ] **Step 4: Run the test**

Run: `cd cpp/build && ctest --test-dir . -R DolphinSchema --output-on-failure 2>&1 | tail -20`
Expected: all assertions PASS.

- [ ] **Step 5: Commit**

```bash
git add cpp/tests/test_dolphin_schema.cpp cpp/CMakeLists.txt
git commit -m "Tests: validate Dolphin schema shape, defaults, and GFX.ini routing"
```

---

## Task 16: Final clean build + full test suite

**Files:** none — verification step.

- [ ] **Step 1: Clean reconfigure + full build**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" 2>&1 | tail -10`
Then: `cd cpp && cmake --build build 2>&1 | tail -20`
Expected: full clean build, no warnings related to the new code.

- [ ] **Step 2: Run the full test suite**

Run: `cd cpp/build && ctest --output-on-failure 2>&1 | tail -50`
Expected: all tests PASS, including:
- `IniFile`, `PatchIniKeys`, `FormatBinding`, `RomScanner`, `Iso9660Reader`, `SfoParser`, `BitmaskHelpers` (existing — must not regress)
- `PPSSPPSchema`, `Pcsx2*`, `Duckstation*` (existing schema/UI tests — must not regress)
- `QuickSettingsFileField` (new — Task 4)
- `DolphinSchema` (new — Task 15)

- [ ] **Step 3: Sanity-check the launchable bundle**

Run: `ls -la cpp/build/RetroNest.app/Contents/MacOS/RetroNest`
Expected: binary exists. The Dolphin manifest will appear in the in-app install grid the next time the user launches the app.

- [ ] **Step 4: Hand off the smoke test**

The user will manually verify (per the acceptance criteria):
1. Launch RetroNest.
2. Open the emulator install grid → confirm Dolphin appears.
3. Install Dolphin via the GitHub release flow → confirm extraction succeeds.
4. Drop a GameCube ISO and a Wii ISO into `roms/gc/` and `roms/wii/` respectively, scan.
5. Launch the GC game → controller input works → press Cmd+Esc → overlay appears → exit.
6. Launch the Wii game → same checks.
7. Open Dolphin settings page in our UI → toggle `PauseOnFocusLost` off → save → reopen settings → confirm the toggle is still off.
8. Use the Quick Settings overlay to change Resolution to "2x" → save → reopen → confirm the value persists.
9. Quit the app, relaunch → re-open settings → confirm both changes are still persisted.

**Plan complete.** Awaiting smoke-test feedback before declaring the adapter shipped.

---

## Spec coverage check

Walked the spec section-by-section:

- ✅ Manifest — Task 5
- ✅ Install layout — Tasks 7, 9
- ✅ `ensureConfig()` portable.txt — Task 9
- ✅ `ensureConfig()` Dolphin.ini patches — Task 9
- ✅ `ensureConfig()` GFX.ini patches — Task 10
- ✅ `ensureConfig()` GCPad/Wiimote defaults — Task 11
- ✅ `ensureConfig()` Hotkeys.ini conflict-clearing — Task 10
- ✅ Framework `file` field extension — Tasks 1-3
- ✅ Framework test (no PCSX2/DS/PPSSPP regression) — Task 4 + Task 16
- ✅ `configFilePath()` — Task 7
- ✅ `settingsSchema()` (Dolphin.ini only) — Task 12
- ✅ `controllerTypes() = empty` (no in-app remap) — Task 6 (header)
- ✅ Pause-on-focus-loss — Task 9 (set in Dolphin.ini)
- ✅ `supportsSaveOnExit() = false` — Task 6 (header)
- ✅ `supportsRetroAchievements() = true` + `patchRetroAchievements()` — Tasks 6, 13
- ✅ Console IDs (RA), ScreenScraper IDs, display names — Task 8
- ✅ Logo + setup wizard registration — Task 14
- ✅ Asset matching — Task 7
- ✅ Resolution + aspect-ratio options targeting GFX.ini — Task 7 (already in scaffold)
- ✅ Build verification, full test suite — Task 16
- ✅ Schema test — Task 15

**Non-goals confirmed deferred:** in-app controller remap, Graphics settings page in our UI, save-on-exit, serial extraction / resume scanning, per-game `GameSettings/{GAMEID}.ini` UI, settings audit pass.

---

## Open items the implementer will resolve in code

These are implementation-time discoveries called out in the spec — not plan gaps:

- Exact RA key names in `RetroAchievements.ini` beyond `Enabled` and `HardcoreEnabled` (Task 13 step 1 calls this out — verify against `Source/Core/Core/Config/AchievementSettings.cpp` and adjust `ProgressEnabled`/`SoundEnabled` if those are not the real key names; omit any that don't exist).
- Default Wiimote profile may need real-device testing to refine the IR/motion mappings (Task 11 step 2 ships a reasonable starter; v1.1 work).
- Whether any additional `Hotkeys.ini` keys conflict with our Cmd+Esc overlay on macOS specifically (Task 10 step 2 covers the obvious ones; add to the patches list if smoke-testing reveals more).
