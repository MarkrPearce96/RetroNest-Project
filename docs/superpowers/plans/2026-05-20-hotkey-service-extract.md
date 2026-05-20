# HotkeyService Extract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the 4 hotkey methods + their helper from `ConfigService` into a new sibling `HotkeyService`. ConfigService loses 110 LOC of unrelated concern; HotkeyService becomes the focused future-extension point.

**Architecture:** New `HotkeyService` class (no dependencies) holds the four methods + the static `libretroHotkeysIniPath()` helper. `ConfigService` keeps its `resetConfiguration` cross-tie via a constructor-injected `HotkeyService*` pointer. `AppController` owns both as direct members; the 4 Q_INVOKABLE hotkey shims re-target to the new service. QML-facing API is unchanged.

**Tech Stack:** C++17, Qt 6, CMake. Existing `test_config_service_libretro_hotkeys.cpp` gets renamed to `test_hotkey_service_libretro.cpp` and re-pointed at HotkeyService — no new test logic needed; bodies are unchanged.

**Spec:** `docs/superpowers/specs/2026-05-20-hotkey-service-extract-design.md`

---

## File Structure

**Create (2):**
- `cpp/src/services/hotkey_service.h` — class declaration (~28 LOC)
- `cpp/src/services/hotkey_service.cpp` — file-static `libretroHotkeysIniPath()` + 4 method bodies copied verbatim from `config_service.cpp:583-688` (~115 LOC)

**Modify (5):**
- `cpp/src/services/config_service.h` — drop 4 hotkey declarations; add `HotkeyService* hotkeyService` ctor param + private member
- `cpp/src/services/config_service.cpp` — delete the 4 method bodies + `libretroHotkeysIniPath()` helper + `// ── Hotkeys ──` header; store the new pointer; update `resetConfiguration` call site
- `cpp/src/ui/app_controller.h` — add `#include "services/hotkey_service.h"`; declare `HotkeyService m_hotkeyService;` **before** `m_configService`
- `cpp/src/ui/app_controller.cpp` — construct `m_hotkeyService` first in member initializer list; pass `&m_hotkeyService` into `m_configService` ctor; add `statusMessage` connect; re-target 4 Q_INVOKABLE shim bodies
- `cpp/CMakeLists.txt` — add new files to main `qt_add_executable`; rename + add hotkey_service.cpp to the test target

**Rename (1):**
- `cpp/tests/test_config_service_libretro_hotkeys.cpp` → `cpp/tests/test_hotkey_service_libretro.cpp` (via `git mv`); update fixture class name; update includes; update `QTEST_MAIN` macro arg and the embedded `.moc` include line

---

## Task 1: Create `HotkeyService` with full bodies

After this commit, `HotkeyService` exists and compiles but has zero callers — `ConfigService` still owns its hotkey methods (temporarily duplicate code). Build passes; all tests pass; behavior unchanged.

**Files:**
- Create: `cpp/src/services/hotkey_service.h`
- Create: `cpp/src/services/hotkey_service.cpp`
- Modify: `cpp/CMakeLists.txt` (main executable target only; tests are migrated in Task 3)

- [ ] **Step 1: Create `cpp/src/services/hotkey_service.h`**

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class HotkeyService : public QObject {
    Q_OBJECT
public:
    explicit HotkeyService(QObject* parent = nullptr);

    QVariantList hotkeyBindings(const QString& emuId) const;
    void saveHotkey(const QString& emuId, const QString& section,
                    const QString& key, const QString& value);
    void clearHotkey(const QString& emuId, const QString& section,
                     const QString& key);
    void resetHotkeys(const QString& emuId);

signals:
    void statusMessage(const QString& msg);
};
```

- [ ] **Step 2: Create `cpp/src/services/hotkey_service.cpp`**

```cpp
#include "hotkey_service.h"

#include "adapters/adapter_registry.h"
#include "core/ini_file.h"
#include "core/libretro/libretro_hotkey_defs.h"

#include <QDir>
#include <QStandardPaths>

// Fixed storage path for the global libretro hotkey INI. The parent directory
// is created on demand so first-run saves don't fail with a missing directory.
static QString libretroHotkeysIniPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/libretro_hotkeys.ini");
}

HotkeyService::HotkeyService(QObject* parent) : QObject(parent) {}

QVariantList HotkeyService::hotkeyBindings(const QString& emuId) const {
    if (emuId == libretro_hotkeys::kSentinelEmuId) {
        IniFile ini;
        ini.load(libretroHotkeysIniPath());
        QVariantList list;
        for (const auto& def : libretro_hotkeys::kLibretroHotkeys) {
            QVariantMap item;
            item["label"] = def.label;
            item["group"] = def.group;
            item["section"] = def.section;
            item["key"] = def.key;
            item["defaultValue"] = def.defaultValue;
            QString current = ini.value(def.section, def.key);
            item["currentValue"] = current.isEmpty() ? def.defaultValue : current;
            list.append(item);
        }
        return list;
    }

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto defs = adapter->hotkeyBindingDefs();
    if (defs.isEmpty()) return {};

    QString configPath = adapter->controllerBindingsConfigFilePath();
    IniFile ini;
    if (!configPath.isEmpty())
        ini.load(configPath);

    QVariantList list;
    for (const auto& def : defs) {
        QVariantMap item;
        item["label"] = def.label;
        item["group"] = def.group;
        item["section"] = def.section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;

        QString current = ini.value(def.section, def.key);
        item["currentValue"] = current.isEmpty() ? def.defaultValue : current;
        list.append(item);
    }
    return list;
}

void HotkeyService::saveHotkey(const QString& emuId, const QString& section,
                                const QString& key, const QString& value) {
    if (emuId == libretro_hotkeys::kSentinelEmuId) {
        const QString configPath = libretroHotkeysIniPath();
        IniFile ini;
        ini.load(configPath);
        ini.setValue(section, key, value);
        if (!ini.save(configPath))
            emit statusMessage("Failed to save hotkey.");
        return;
    }

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    ini.setValue(section, key, value);

    if (!ini.save(configPath))
        emit statusMessage("Failed to save hotkey.");
}

void HotkeyService::clearHotkey(const QString& emuId, const QString& section, const QString& key) {
    saveHotkey(emuId, section, key, "");
}

void HotkeyService::resetHotkeys(const QString& emuId) {
    if (emuId == libretro_hotkeys::kSentinelEmuId) {
        const QString configPath = libretroHotkeysIniPath();
        IniFile ini;
        ini.load(configPath);
        for (const auto& def : libretro_hotkeys::kLibretroHotkeys)
            ini.setValue(def.section, def.key, def.defaultValue);
        if (ini.save(configPath))
            emit statusMessage("Hotkeys reset to defaults.");
        else
            emit statusMessage("Failed to reset hotkeys.");
        return;
    }

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    for (const auto& def : adapter->hotkeyBindingDefs())
        ini.setValue(def.section, def.key, def.defaultValue);

    if (ini.save(configPath))
        emit statusMessage("Hotkeys reset to defaults.");
    else
        emit statusMessage("Failed to reset hotkeys.");
}
```

This is a direct line-for-line copy of `config_service.cpp:33-37` (the helper) + `config_service.cpp:583-688` (the four methods). No semantic changes.

- [ ] **Step 3: Add the new files to `cpp/CMakeLists.txt`'s main executable target**

Find the `qt_add_executable(RetroNest ...)` block (or wherever `config_service.cpp` is listed) and add the new pair on adjacent lines. Search for `services/config_service.cpp` in `cpp/CMakeLists.txt` — both files (`.h` and `.cpp`) of ConfigService should be listed; add the same pair for `hotkey_service`:

```cmake
        src/services/config_service.cpp
        src/services/config_service.h
        src/services/hotkey_service.cpp
        src/services/hotkey_service.h
```

(Exact existing surrounding context may vary; the key is that both new files go into the same sources list that already contains `config_service.cpp`.)

- [ ] **Step 4: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds. `HotkeyService` is compiled but has no callers yet, so the linker may emit no warnings about unused symbols (the methods are still reachable as Qt-meta exposed).

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/hotkey_service.h cpp/src/services/hotkey_service.cpp cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(services): add HotkeyService skeleton

New cpp/src/services/hotkey_service.{h,cpp} containing the four
hotkey methods (hotkeyBindings/saveHotkey/clearHotkey/resetHotkeys)
plus the libretroHotkeysIniPath helper. Bodies are a line-for-line
copy of the same methods currently in ConfigService — Task 2 deletes
them from ConfigService and rewires consumers; until then, code is
intentionally duplicate so build + tests stay green at every step.

Not wired into any caller yet.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Delete from ConfigService + rewire callers

After this commit, `ConfigService` no longer owns the hotkey methods. `AppController` constructs both services and re-targets its Q_INVOKABLE shims. Build passes; the renamed test file (Task 3) is the only remaining cleanup before everything is green.

**Files:**
- Modify: `cpp/src/services/config_service.h` — drop 4 declarations, add HotkeyService ctor param + member
- Modify: `cpp/src/services/config_service.cpp` — delete 4 method bodies + section header + helper, update ctor, update resetConfiguration call
- Modify: `cpp/src/ui/app_controller.h` — include HotkeyService, add member before m_configService
- Modify: `cpp/src/ui/app_controller.cpp` — construct in initializer list, connect signal, re-target 4 Q_INVOKABLE shims

- [ ] **Step 1: Edit `cpp/src/services/config_service.h`**

The file currently declares the hotkey methods (lines 53-58 inclusive of the section comment). Delete those six lines:

```cpp
    // Hotkeys
    QVariantList hotkeyBindings(const QString& emuId) const;
    void saveHotkey(const QString& emuId, const QString& section,
                    const QString& key, const QString& value);
    void clearHotkey(const QString& emuId, const QString& section, const QString& key);
    void resetHotkeys(const QString& emuId);
```

Then, in the constructor declaration (line 24), add the new parameter:

```cpp
// before
explicit ConfigService(ManifestLoader* loader, QObject* parent = nullptr);

// after
explicit ConfigService(ManifestLoader* loader,
                       HotkeyService* hotkeyService,
                       QObject* parent = nullptr);
```

In the private members block (around line 93), add the pointer after `m_inputManager`:

```cpp
private:
    ManifestLoader* m_loader;
    SdlInputManager* m_inputManager = nullptr;
    HotkeyService* m_hotkeyService;   // non-owning; constructed by AppController, lifetime guaranteed by it
```

Add the forward declaration near the existing `class SdlInputManager;` (around line 11):

```cpp
class SdlInputManager;
class HotkeyService;
```

- [ ] **Step 2: Edit `cpp/src/services/config_service.cpp`**

Three deletions and two surgical edits:

**Deletion A — the static helper (lines 33-37):**

```cpp
// Fixed storage path for the global libretro hotkey INI. ...
static QString libretroHotkeysIniPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/libretro_hotkeys.ini");
}
```

Delete entirely.

**Deletion B — the four method bodies (lines 581-688):**

Delete from the `// ── Hotkeys ──────────────────────────────────────────────` section header through the closing `}` of `resetHotkeys()`. That's 108 lines including the section header and blank lines.

**Edit A — constructor (line 39-40):**

```cpp
// before
ConfigService::ConfigService(ManifestLoader* loader, QObject* parent)
    : QObject(parent), m_loader(loader) {}

// after
ConfigService::ConfigService(ManifestLoader* loader,
                              HotkeyService* hotkeyService,
                              QObject* parent)
    : QObject(parent), m_loader(loader), m_hotkeyService(hotkeyService) {}
```

**Edit B — resetConfiguration (line 242):**

```cpp
// before
    resetHotkeys(emuId);

// after
    m_hotkeyService->resetHotkeys(emuId);
```

Finally, add the include near the top of the file (where `config_service.h` is included):

```cpp
#include "config_service.h"
#include "hotkey_service.h"
```

- [ ] **Step 3: Edit `cpp/src/ui/app_controller.h`**

Add the include near the existing service includes (look for `#include "services/config_service.h"`):

```cpp
#include "services/config_service.h"
#include "services/hotkey_service.h"
```

In the private members block (around line 344, where `ConfigService m_configService;` is declared), insert `m_hotkeyService` **before** `m_configService` so it's constructed first:

```cpp
// before
    ConfigService m_configService;

// after
    HotkeyService m_hotkeyService;
    ConfigService m_configService;
```

(Order matters: C++ initializes class members in declaration order; we need `m_hotkeyService` to exist before `m_configService`'s constructor receives `&m_hotkeyService`.)

- [ ] **Step 4: Edit `cpp/src/ui/app_controller.cpp` — constructor**

The constructor initializer list (around lines 40-44) currently reads:

```cpp
    , m_gameService(loader, db)
    , m_scraperService(db)
    , m_emuService(loader)
    , m_raService(db)
    , m_configService(loader)
```

Change to (matching the new declaration order — `m_hotkeyService` must come before `m_configService`):

```cpp
    , m_gameService(loader, db)
    , m_scraperService(db)
    , m_emuService(loader)
    , m_raService(db)
    , m_hotkeyService(this)
    , m_configService(loader, &m_hotkeyService)
```

In the constructor body, the existing `connect(&m_configService, &ConfigService::statusMessage, ...)` line (around line 46) gets a sibling for the new service. Add immediately after the two existing `m_configService` connects:

```cpp
    connect(&m_configService, &ConfigService::statusMessage, this, &AppController::setStatus);
    connect(&m_configService, &ConfigService::configurationReset,
            this, &AppController::configurationReset);
    connect(&m_hotkeyService, &HotkeyService::statusMessage, this, &AppController::setStatus);
```

- [ ] **Step 5: Edit `cpp/src/ui/app_controller.cpp` — Q_INVOKABLE shims**

Four lines re-target from `m_configService.` to `m_hotkeyService.`. Around lines 709-734:

```cpp
// before
QVariantList AppController::hotkeyBindings(const QString& emuId) const { return m_configService.hotkeyBindings(emuId); }
// ...
    return !m_configService.hotkeyBindings(emuId).isEmpty();
// ...
void AppController::saveHotkey(const QString& emuId, const QString& section, const QString& key, const QString& value) {
    m_configService.saveHotkey(emuId, section, key, value);
}

void AppController::clearHotkey(const QString& emuId, const QString& section, const QString& key) {
    m_configService.clearHotkey(emuId, section, key);
}

void AppController::resetHotkeys(const QString& emuId) {
    m_configService.resetHotkeys(emuId);
}
```

Change every `m_configService.` in those five spots to `m_hotkeyService.`. The function signatures and bodies otherwise stay identical:

```cpp
// after
QVariantList AppController::hotkeyBindings(const QString& emuId) const { return m_hotkeyService.hotkeyBindings(emuId); }
// ...
    return !m_hotkeyService.hotkeyBindings(emuId).isEmpty();
// ...
void AppController::saveHotkey(const QString& emuId, const QString& section, const QString& key, const QString& value) {
    m_hotkeyService.saveHotkey(emuId, section, key, value);
}

void AppController::clearHotkey(const QString& emuId, const QString& section, const QString& key) {
    m_hotkeyService.clearHotkey(emuId, section, key);
}

void AppController::resetHotkeys(const QString& emuId) {
    m_hotkeyService.resetHotkeys(emuId);
}
```

(The single non-shim spot — around line 715, inside `currentGameInfo` — uses the same accessor pattern; re-target that too.)

- [ ] **Step 6: Build**

```bash
cmake --build cpp/build-x86_64
```

Expected: build succeeds.

If the build fails, the most likely cause is a missing include or member-declaration-order issue. Read the error and fix; do not proceed to commit until the build is clean.

Expected test failure at this point: `test_config_service_libretro_hotkeys` (the still-old-name test) will fail to compile or link because it instantiates `ConfigService cs(/*loader=*/nullptr)` (now requires a `HotkeyService*` parameter) and calls methods that no longer exist on ConfigService. That's expected — it gets migrated in Task 3. The main executable target should still build.

If the **main `RetroNest` target** fails to build, stop and fix. If only `test_config_service_libretro_hotkeys` fails, that's expected.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/services/config_service.h cpp/src/services/config_service.cpp \
        cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "$(cat <<'EOF'
refactor(services): move hotkey methods from ConfigService to HotkeyService

ConfigService loses 4 hotkey methods + libretroHotkeysIniPath helper
(~110 LOC). Keeps the resetConfiguration cross-tie via a constructor-
injected HotkeyService* pointer. AppController owns both services as
direct members, with m_hotkeyService declared before m_configService
so the latter's ctor can take its address.

Four AppController Q_INVOKABLE shims re-target from m_configService
to m_hotkeyService. The QML-facing API surface is unchanged.

config_service.cpp shrinks from 886 LOC to ~776 LOC of unified
settings/paths/controllers/quick-settings concerns. HotkeyService is
the focused future-extension point for any hotkey work.

The renamed test (test_hotkey_service_libretro.cpp) follows in the
next commit; the old test file will not compile against this commit
because ConfigService no longer exposes the hotkey methods.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Rename and migrate the test file

After this commit, all tests pass.

**Files:**
- Rename: `cpp/tests/test_config_service_libretro_hotkeys.cpp` → `cpp/tests/test_hotkey_service_libretro.cpp`
- Modify: `cpp/CMakeLists.txt` — rename the test target + swap which service source it links against

- [ ] **Step 1: Rename via git**

```bash
git mv cpp/tests/test_config_service_libretro_hotkeys.cpp cpp/tests/test_hotkey_service_libretro.cpp
```

- [ ] **Step 2: Edit the renamed file**

The new file `cpp/tests/test_hotkey_service_libretro.cpp` becomes:

```cpp
#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDir>
#include "services/hotkey_service.h"
#include "core/libretro/libretro_hotkey_defs.h"

class TestHotkeyServiceLibretro : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tmp;

private slots:
    void initTestCase() {
        QVERIFY(m_tmp.isValid());
        QStandardPaths::setTestModeEnabled(true);
    }

    void sentinelReturnsLibretroSchema() {
        HotkeyService hs;
        QVariantList rows = hs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        QCOMPARE(rows.size(), libretro_hotkeys::kLibretroHotkeys.size());

        // Every returned row exposes the expected fields.
        QVariantMap first = rows.first().toMap();
        QVERIFY(first.contains(QStringLiteral("label")));
        QVERIFY(first.contains(QStringLiteral("group")));
        QVERIFY(first.contains(QStringLiteral("section")));
        QVERIFY(first.contains(QStringLiteral("key")));
        QVERIFY(first.contains(QStringLiteral("defaultValue")));
        QVERIFY(first.contains(QStringLiteral("currentValue")));
        // No saved value yet → currentValue == defaultValue
        QCOMPARE(first.value(QStringLiteral("currentValue")).toString(),
                 first.value(QStringLiteral("defaultValue")).toString());
    }

    void saveThenReadBackRoundTrips() {
        HotkeyService hs;
        hs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Pause,
                      QStringLiteral("Keyboard/Z"));

        QVariantList rows = hs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        bool found = false;
        for (const QVariant& v : rows) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("key")).toString() == libretro_hotkeys::ids::Pause) {
                QCOMPARE(m.value(QStringLiteral("currentValue")).toString(),
                         QStringLiteral("Keyboard/Z"));
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void clearReturnsToDefault() {
        HotkeyService hs;
        hs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Mute,
                      QStringLiteral("Keyboard/Y"));
        hs.clearHotkey(libretro_hotkeys::kSentinelEmuId,
                       QStringLiteral("Hotkeys"),
                       libretro_hotkeys::ids::Mute);
        QVariantList rows = hs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        for (const QVariant& v : rows) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("key")).toString() == libretro_hotkeys::ids::Mute) {
                // After clear, currentValue should equal defaultValue.
                QCOMPARE(m.value(QStringLiteral("currentValue")).toString(),
                         m.value(QStringLiteral("defaultValue")).toString());
                return;
            }
        }
        QFAIL("Mute row not found");
    }

    void resetReturnsAllToDefaults() {
        HotkeyService hs;
        hs.saveHotkey(libretro_hotkeys::kSentinelEmuId,
                      QStringLiteral("Hotkeys"),
                      libretro_hotkeys::ids::Pause,
                      QStringLiteral("Keyboard/Q"));
        hs.resetHotkeys(libretro_hotkeys::kSentinelEmuId);
        QVariantList rows = hs.hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        for (const QVariant& v : rows) {
            QVariantMap m = v.toMap();
            QCOMPARE(m.value(QStringLiteral("currentValue")).toString(),
                     m.value(QStringLiteral("defaultValue")).toString());
        }
    }
};

QTEST_MAIN(TestHotkeyServiceLibretro)
#include "test_hotkey_service_libretro.moc"
```

The diff vs the old file:
- `#include "services/config_service.h"` → `#include "services/hotkey_service.h"`
- Class name `TestConfigServiceLibretroHotkeys` → `TestHotkeyServiceLibretro`
- Every `ConfigService cs(/*loader=*/nullptr);` → `HotkeyService hs;`
- Every `cs.` → `hs.`
- `QTEST_MAIN(TestConfigServiceLibretroHotkeys)` → `QTEST_MAIN(TestHotkeyServiceLibretro)`
- `#include "test_config_service_libretro_hotkeys.moc"` → `#include "test_hotkey_service_libretro.moc"`

- [ ] **Step 3: Edit `cpp/CMakeLists.txt` — rename the test target**

Find the block starting at line 480:

```cmake
add_executable(test_config_service_libretro_hotkeys
    tests/test_config_service_libretro_hotkeys.cpp
    src/services/config_service.cpp
    ...
```

Replace it with the new name + swap the service source from `config_service.cpp` to `hotkey_service.cpp`. The renamed test only exercises `HotkeyService` (the libretro sentinel path), so it doesn't need `config_service.cpp`. But the test executable does need the `path_overrides_store.cpp`, etc., if HotkeyService's includes pull them in transitively — they don't (HotkeyService only includes `adapter_registry.h`, `ini_file.h`, `libretro_hotkey_defs.h`, plus `QDir`/`QStandardPaths`). So the test sources list can be much smaller.

The full replacement (around lines 480-519):

```cmake
add_executable(test_hotkey_service_libretro
    tests/test_hotkey_service_libretro.cpp
    src/services/hotkey_service.cpp
    src/core/libretro/libretro_hotkey_defs.cpp
    src/adapters/emulator_adapter.cpp
    src/adapters/adapter_registry.cpp
    src/adapters/duckstation_adapter.cpp
    src/adapters/ppsspp_adapter.cpp
    src/adapters/dolphin_adapter.cpp
    src/adapters/libretro/libretro_adapter.cpp
    src/adapters/libretro/mgba_libretro_adapter.cpp
    src/adapters/libretro/pcsx2_libretro_adapter.cpp
    src/core/libretro/core_loader.cpp
    src/core/libretro/core_runtime.cpp
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
    src/core/path_overrides_store.cpp
    src/core/database.cpp
    src/core/manifest_loader.cpp
    src/core/sdl_input_manager.cpp
)
set_target_properties(test_hotkey_service_libretro PROPERTIES AUTOMOC ON)
target_include_directories(test_hotkey_service_libretro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../vendor/libretro-api
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(test_hotkey_service_libretro PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Test chdr-static rcheevos_static ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(NAME HotkeyServiceLibretro COMMAND test_hotkey_service_libretro)
```

The only changes from the original block: target name `test_config_service_libretro_hotkeys` → `test_hotkey_service_libretro`, source file `tests/test_config_service_libretro_hotkeys.cpp` → `tests/test_hotkey_service_libretro.cpp`, source library `src/services/config_service.cpp` → `src/services/hotkey_service.cpp`, and `add_test` NAME `ConfigServiceLibretroHotkeys` → `HotkeyServiceLibretro`. Everything else (the long adapter/core support source list, the include paths, the link libraries) is identical and needs to be there because the AdapterRegistry pulls in every adapter, even though the sentinel path doesn't touch them at test time.

- [ ] **Step 4: Build and run the renamed test**

```bash
cmake --build cpp/build-x86_64 --target test_hotkey_service_libretro
./cpp/build-x86_64/test_hotkey_service_libretro
```

Expected: build succeeds, test runs, all 4 test cases pass.

If the test fails to compile (typo, missing include), fix and re-run.
If a test case fails, the most likely cause is the `QStandardPaths::setTestModeEnabled(true)` not isolating the test from a real user libretro_hotkeys.ini — re-run with a fresh `QTemporaryDir` if so.

- [ ] **Step 5: Run the full test suite**

```bash
cmake --build cpp/build-x86_64
ctest --test-dir cpp/build-x86_64
```

Expected: all tests pass, including the renamed `HotkeyServiceLibretro`.

- [ ] **Step 6: Commit**

```bash
git add cpp/tests/test_hotkey_service_libretro.cpp cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test(hotkey): rename test_config_service_libretro_hotkeys → test_hotkey_service_libretro

git mv preserves history. Fixture class renamed
TestConfigServiceLibretroHotkeys → TestHotkeyServiceLibretro and
internals re-pointed at HotkeyService (which now owns the methods
the test was exercising all along). The libretro sentinel test path
is unchanged — same QVERIFY/QCOMPARE bodies, same expected results.
CMakeLists target + add_test name renamed in lockstep.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Build, deploy, sign, smoke test, memory update

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md`

- [ ] **Step 1: Kill any running RetroNest**

```bash
pkill -f "build-x86_64/RetroNest.app" 2>/dev/null
```

- [ ] **Step 2: Deploy + resign (per `build-cmake-needs-macdeployqt` memory)**

```bash
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=cpp/qml -no-codesign -always-overwrite
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
```

Verify the Qt refs are `@executable_path/...`:

```bash
otool -L cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest | grep -c "@executable_path/.*Qt"
```

Expected: ≥ 8.

- [ ] **Step 3: Launch and confirm running**

```bash
open cpp/build-x86_64/RetroNest.app
sleep 5
pgrep -fl "build-x86_64/RetroNest.app/Contents/MacOS/RetroNest"
```

Expected: a process line. If crashed, check `~/Library/Logs/DiagnosticReports/RetroNest-*.ips`.

- [ ] **Step 4: Hand off smoke test to the user**

The controller running this plan should ask the user to verify:

**External-process hotkey path** (DuckStation OR PPSSPP — pick whichever is installed):
1. Open **Settings → Hotkeys** for the emulator.
2. Bindings list populates with current INI values (no blank rows).
3. Capture a new binding on one row; click Save. Verify the emulator's INI file at `~/Documents/RetroNest/emulators/<emu>/.../<config>.ini` reflects the new binding.
4. Clear a binding. Verify the INI key is empty/removed.
5. Reset all hotkeys. Verify defaults restored.

**Libretro hotkey path** (sentinel `emuId`):
6. Open the global **Libretro Hotkeys** dialog (Settings → Manage Emulators → Libretro Hotkeys, or wherever the entry point is).
7. Repeat save/clear/reset against `~/Library/Application Support/<bundle>/libretro_hotkeys.ini`.

**Reset cross-tie:**
8. Settings → Manage Emulators → Reset to Defaults on DuckStation. Verify hotkeys also get reset (proves the `m_hotkeyService->resetHotkeys()` call from inside `resetConfiguration` still wires).

If any step regresses, STOP and report which page + action + observed behavior.

- [ ] **Step 5: Update refactor-roadmap memory**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/refactor-roadmap.md`.

Find the line:
```
6. **Extract `HotkeyService` from `ConfigService`** — `cpp/src/services/config_service.cpp` is 886 LOC handling INI settings + controller bindings + hotkey bindings + libretro hotkey sentinel. Hotkey slice has the cleanest seam (it talks to `HotkeyMatcher`/`HotkeyDispatcher`, not the settings caches). Split first; leave settings + controller-bindings sharing INI caching for now.
```

Replace with:
```
6. ✅ **Extract `HotkeyService` from `ConfigService`** — shipped 2026-05-20. New `cpp/src/services/hotkey_service.{h,cpp}` owns the 4 hotkey methods + libretroHotkeysIniPath helper that were previously inside ConfigService. ConfigService keeps its resetConfiguration cross-tie via a constructor-injected `HotkeyService*` pointer; AppController owns both services as direct members (declared in m_hotkeyService-before-m_configService order so the ctor can pass `&m_hotkeyService` in). The 4 Q_INVOKABLE shims in AppController re-target. config_service.cpp drops from 886 LOC → ~776 LOC. Test file renamed test_config_service_libretro_hotkeys.cpp → test_hotkey_service_libretro.cpp. Net ≈ +35 LOC (overhead of new file boilerplate; the win is separation of concerns, not LOC). Spec: `docs/superpowers/specs/2026-05-20-hotkey-service-extract-design.md`. Plan: `docs/superpowers/plans/2026-05-20-hotkey-service-extract.md`.
```

Also update the front-matter description:
```
description: Ongoing generalization/cleanup roadmap for RetroNest. Tier 1 items 1-5, Tier 2 #6 and #9 shipped 2026-05-20; #10 retired (overscoped); remaining Tier 2 items pending. Resume here when starting a new session on this work.
```

Update the "Suggested next step" section to remove #6 from the recommendation:
```
## Suggested next step (after #6, #9 done; #10 retired)

Tier 1 done; Tier 2 #6 and #9 done; #10 retired. Two remaining Tier 2 items:

- **#7 `BaseNotification` / `PopupBase.qml`** — 6 toast/popup files duplicate scrim + card + slide/fade + timer/Escape dismissal. Mid-size QML extraction.
- **#8 AppController signal-forwarding facade** — incremental, only when adding new features (don't do as a one-shot refactor).

Plus logged follow-ups:
- **`RetroAchievementsSettings.qml` keyboard-nav redesign** — pain point flagged during #4 smoke test. Own brainstorm cycle.
- **`AchievementsPage.qml` migration** — needs `GenericListPage` to grow `headerComponent: Component` first.
```

- [ ] **Step 6: Update MEMORY.md index**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md`. Find:
```
- [Refactor roadmap](refactor-roadmap.md) — Multi-session generalization program. Tier 1 #1-5 + Tier 2 #9 shipped 2026-05-20; #10 retired (overscoped); remaining Tier 2 (#6 HotkeyService, #7 BaseNotification, #8 AppController) pending. Open here when resuming refactor work.
```

Replace with:
```
- [Refactor roadmap](refactor-roadmap.md) — Multi-session generalization program. Tier 1 #1-5 + Tier 2 #6 and #9 shipped 2026-05-20; #10 retired (overscoped); remaining Tier 2 (#7 BaseNotification, #8 AppController) pending. Open here when resuming refactor work.
```

Memory files live outside the repo — no git commit needed.

---

## Self-review

**Spec coverage:**
- New `HotkeyService.{h,cpp}` files → Task 1 ✓
- `ConfigService` hotkey deletions + constructor injection of `HotkeyService*` → Task 2 ✓
- `resetConfiguration` cross-tie call update → Task 2 Step 2 ✓
- `AppController` member-declaration order + connect + 4 shim re-targets → Task 2 Steps 3-5 ✓
- Test file rename + fixture class rename + CMakeLists target rename → Task 3 ✓
- CMakeLists.txt entries for main exe + renamed test → Tasks 1 and 3 ✓
- Smoke test (external hotkey path, libretro sentinel path, reset cross-tie) → Task 4 Step 4 ✓
- Memory updates → Task 4 Steps 5-6 ✓

**Placeholder scan:** No TBDs. Every code block contains exact content; every file path is fully qualified.

**Type / name consistency:**
- `HotkeyService` named identically in Task 1 (declaration), Task 2 (constructor param + member + #include), Task 3 (test fixture), Task 4 (memory).
- The 4 method signatures match between Task 1's declaration and Task 1's `.cpp` definitions and Task 3's test calls (`hs.hotkeyBindings(...)` etc.).
- `m_hotkeyService` named consistently as both the AppController member (Task 2 Step 3) and the ConfigService pointer member (Task 2 Step 1). They are different objects (one is a value, one is a non-owning pointer), but the name match is intentional for readability.

**One real risk:** the `m_hotkeyService` declaration order in `app_controller.h` is load-bearing — must come before `m_configService` so it constructs first and exists when ConfigService's ctor takes `&m_hotkeyService`. Documented explicitly in both the spec and Task 2 Step 3.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-20-hotkey-service-extract.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task with two-stage review. Three real implementation tasks (#1, #2, #3) plus deploy (#4); the rewire in Task 2 is the biggest single diff and benefits most from independent review.

**2. Inline Execution** — same session, batch with checkpoints.

Which approach?
