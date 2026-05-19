# RetroNest libretro path overrides — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Paths UI persist and propagate path overrides for libretro adapters (PCSX2 + mGBA), covering 3 PCSX2 paths (Memory Cards, Save States, Textures) and 2 mGBA paths (Saves, Save States).

**Architecture:** New `PathOverridesStore` (RetroNest, JSON-backed) becomes the persistence backend when an adapter has no INI. Three propagation flavors: RetroNest-controlled paths consult the store at computation sites; mGBA Saves overrides `save_dir` via the libretro env; PCSX2 Memcards/Textures use two new private env enums (`0x20003`, `0x20004`) that `Settings::InitializeDefaults` queries on each `retro_load_game`.

**Tech Stack:** C++20, Qt 6 (QObject/QtTest/QJsonDocument), libretro env C ABI, PCSX2 fork at `/Users/mark/Documents/Projects/pcsx2-libretro/`.

**Spec:** `docs/superpowers/specs/2026-05-19-retronest-libretro-path-overrides-design.md` (commit `07c04e1`).

**Repos touched:**
- `/Users/mark/Documents/Projects/RetroNest-Project/` (origin/main, push when done)
- `/Users/mark/Documents/Projects/pcsx2-libretro/` (local-only fork, no push)

---

## Task 1: PathOverridesStore — JSON-backed persistence helper

**Files:**
- Create: `cpp/src/core/path_overrides_store.h`
- Create: `cpp/src/core/path_overrides_store.cpp`
- Create: `cpp/tests/test_path_overrides_store.cpp`
- Modify: `cpp/CMakeLists.txt` (add source + test target)

- [ ] **Step 1: Write the failing test**

Create `cpp/tests/test_path_overrides_store.cpp`:
```cpp
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/path_overrides_store.h"

class TestPathOverridesStore : public QObject {
    Q_OBJECT
private slots:
    void testReadMissingReturnsEmpty() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString());
    }
    void testWriteThenReadRoundTrips() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        store.write("pcsx2", "MemoryCards", "/Volumes/Ext/memcards");
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString("/Volumes/Ext/memcards"));
    }
    void testWritePersistsAcrossInstances() {
        QTemporaryDir dir;
        const QString path = dir.filePath("overrides.json");
        { PathOverridesStore a(path); a.write("mgba", "Saves", "/x/y"); }
        PathOverridesStore b(path);
        QCOMPARE(b.read("mgba", "Saves"), QString("/x/y"));
    }
    void testClearRemovesKey() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        store.write("pcsx2", "Textures", "/t");
        store.clear("pcsx2", "Textures");
        QCOMPARE(store.read("pcsx2", "Textures"), QString());
    }
    void testEmptyStringTreatedAsUnset() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        store.write("pcsx2", "MemoryCards", "");
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString());
    }
    void testMultipleEmulatorsCoexist() {
        QTemporaryDir dir;
        PathOverridesStore store(dir.filePath("overrides.json"));
        store.write("pcsx2", "MemoryCards", "/p");
        store.write("mgba",  "Saves",       "/m");
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString("/p"));
        QCOMPARE(store.read("mgba",  "Saves"),       QString("/m"));
    }
    void testCorruptFileTreatedAsEmpty() {
        QTemporaryDir dir;
        const QString p = dir.filePath("overrides.json");
        QFile f(p); f.open(QIODevice::WriteOnly); f.write("not json"); f.close();
        PathOverridesStore store(p);
        QCOMPARE(store.read("pcsx2", "MemoryCards"), QString());
    }
};
QTEST_MAIN(TestPathOverridesStore)
#include "test_path_overrides_store.moc"
```

- [ ] **Step 2: Write header**

Create `cpp/src/core/path_overrides_store.h`:
```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// PathOverridesStore — JSON-backed persistence for per-emulator
// directory overrides exposed in the Paths settings UI.
//
// Used as the backend when adapter->configFilePath().isEmpty()
// (i.e., libretro adapters). Native adapters keep their existing
// INI-based persistence via ConfigService.

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QString>

class PathOverridesStore {
public:
    // Constructor used in tests with an explicit file path.
    explicit PathOverridesStore(const QString& filePath);

    // Singleton access for production code. Resolves to
    // <writable-app-data>/path_overrides.json — typically
    // ~/Library/Application Support/RetroNest/path_overrides.json
    // on macOS.
    static PathOverridesStore& instance();

    // Returns the override for (emuId, key), or empty string if
    // unset / file missing / file malformed. Empty-string values
    // on disk are treated as unset (fall back to default).
    QString read(const QString& emuId, const QString& key) const;

    // Writes the override and persists to disk immediately.
    // Empty path removes the override (equivalent to clear()).
    void write(const QString& emuId, const QString& key, const QString& path);

    // Removes the override for (emuId, key). No-op if unset.
    void clear(const QString& emuId, const QString& key);

private:
    void load();
    bool save() const;

    QString          m_filePath;
    mutable QMutex   m_mutex;        // guards m_root + file I/O
    mutable QJsonObject m_root;      // {emuId: {key: path, ...}, ...}
};
```

- [ ] **Step 3: Write implementation**

Create `cpp/src/core/path_overrides_store.cpp`:
```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "path_overrides_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QStandardPaths>

PathOverridesStore::PathOverridesStore(const QString& filePath)
    : m_filePath(filePath) {
    load();
}

PathOverridesStore& PathOverridesStore::instance() {
    static const QString defaultPath = []() {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        return dir + "/path_overrides.json";
    }();
    static PathOverridesStore singleton(defaultPath);
    return singleton;
}

void PathOverridesStore::load() {
    QMutexLocker lock(&m_mutex);
    m_root = {};
    QFile f(m_filePath);
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return;  // corrupt → empty
    if (!doc.isObject()) return;
    m_root = doc.object();
}

bool PathOverridesStore::save() const {
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
    return true;
}

QString PathOverridesStore::read(const QString& emuId, const QString& key) const {
    QMutexLocker lock(&m_mutex);
    const auto emu = m_root.value(emuId).toObject();
    const auto v = emu.value(key).toString();
    return v.isEmpty() ? QString() : v;
}

void PathOverridesStore::write(const QString& emuId, const QString& key,
                               const QString& path) {
    QMutexLocker lock(&m_mutex);
    auto emu = m_root.value(emuId).toObject();
    if (path.isEmpty())
        emu.remove(key);
    else
        emu.insert(key, path);
    if (emu.isEmpty())
        m_root.remove(emuId);
    else
        m_root.insert(emuId, emu);
    save();
}

void PathOverridesStore::clear(const QString& emuId, const QString& key) {
    write(emuId, key, QString());
}
```

- [ ] **Step 4: Register source + test target in CMakeLists.txt**

Edit `cpp/CMakeLists.txt`. Add to the `SOURCES` list (alphabetically near other core/ entries, around line 58):
```
    src/core/path_overrides_store.cpp
```

Add a new test target near `test_ini_file` (around line 446):
```cmake
add_executable(test_path_overrides_store
    tests/test_path_overrides_store.cpp
    src/core/path_overrides_store.cpp
)
target_include_directories(test_path_overrides_store PRIVATE src)
target_link_libraries(test_path_overrides_store PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PathOverridesStore COMMAND test_path_overrides_store)
```

- [ ] **Step 5: Build and run tests**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target test_path_overrides_store -j4
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/test_path_overrides_store
```
Expected: 7 tests pass (`Totals: 7 passed, 0 failed, 0 skipped`).

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/path_overrides_store.h cpp/src/core/path_overrides_store.cpp \
        cpp/tests/test_path_overrides_store.cpp cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(core): PathOverridesStore — JSON-backed per-emulator path overrides

Persistence backend for the Paths settings UI for libretro adapters
(which have no INI of their own). Reads from / writes to
<AppDataLocation>/path_overrides.json with a {emuId: {key: path}}
shape. Empty-string overrides are treated as unset (fall back to
default). Thread-safe via internal QMutex.

7 unit tests cover: missing-file empty read, write+read round-trip,
cross-instance persistence, clear, empty-string-as-unset,
multi-emulator coexistence, corrupt-file recovery.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: ConfigService — route libretro adapters to PathOverridesStore

**Files:**
- Modify: `cpp/src/services/config_service.cpp` (`pathValue`, `savePaths` — wrap existing INI logic with empty-`configFilePath()` branch)
- Modify: `cpp/src/services/config_service.h` (add include if needed)

- [ ] **Step 1: Add include**

Top of `cpp/src/services/config_service.cpp`, add to existing includes:
```cpp
#include "core/path_overrides_store.h"
```

- [ ] **Step 2: Modify `pathValue` to branch on configFilePath emptiness**

Replace the existing `pathValue` body (currently at `cpp/src/services/config_service.cpp:393-403`) with:
```cpp
QString ConfigService::pathValue(const QString& emuId, const QString& section, const QString& key) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) {
        // Libretro adapter — no INI. Read from PathOverridesStore.
        // `section` is informational ("libretro") and ignored; the key
        // alone scopes the override within the emulator namespace.
        return PathOverridesStore::instance().read(emuId, key);
    }

    IniFile ini;
    ini.load(configPath);
    return ini.value(section, key);
}
```

- [ ] **Step 3: Modify `savePaths` to branch on configFilePath emptiness**

Replace the existing `savePaths` body (currently at `cpp/src/services/config_service.cpp:415-440`) with:
```cpp
void ConfigService::savePaths(const QString& emuId, const QVariantMap& values) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) {
        // Libretro adapter — route to PathOverridesStore.
        auto& store = PathOverridesStore::instance();
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            int lastSlash = it.key().lastIndexOf('/');
            if (lastSlash > 0) {
                QString key = it.key().mid(lastSlash + 1);
                store.write(emuId, key, it.value().toString());
            } else {
                qWarning() << "[Paths] Skipping malformed key (no section separator):" << it.key();
            }
        }
        emit statusMessage("Paths saved.");
        return;
    }

    IniFile ini;
    ini.load(configPath);
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        int lastSlash = it.key().lastIndexOf('/');
        if (lastSlash > 0) {
            QString section = it.key().left(lastSlash);
            QString key = it.key().mid(lastSlash + 1);
            ini.setValue(section, key, it.value().toString());
        } else {
            qWarning() << "[Paths] Skipping malformed key (no section separator):" << it.key();
        }
    }

    if (ini.save(configPath))
        emit statusMessage("Paths saved.");
    else
        emit statusMessage("Failed to save paths.");
}
```

- [ ] **Step 4: Verify build still passes**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target RetroNest -j4 2>&1 | grep -E "error|Built target RetroNest$" | tail -10
```
Expected: `[100%] Built target RetroNest` (with no `error:` lines for this file).

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/config_service.cpp
git commit -m "$(cat <<'EOF'
feat(config-service): route libretro path overrides to PathOverridesStore

When the adapter has no INI (configFilePath() empty — all libretro
adapters), pathValue and savePaths now go through PathOverridesStore
instead of the IniFile path. Native adapters (Dolphin, DuckStation,
PPSSPP) unchanged. The PathDef section field is informational for
libretro entries; only the key matters within the emulator namespace.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Adapter `pathsDefs()` — PCSX2 (new) and mGBA (revised)

**Files:**
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h` (declare `pathsDefs` override)
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` (implement `pathsDefs`)
- Modify: `cpp/src/adapters/libretro/mgba_libretro_adapter.cpp` (populate section/key; drop Screenshots row)

- [ ] **Step 1: Add `pathsDefs()` declaration to PCSX2 adapter header**

In `cpp/src/adapters/libretro/pcsx2_libretro_adapter.h`, add inside the public block (next to `extractSerial`, `findResumeFile`, etc.):
```cpp
QVector<PathDef> pathsDefs() const override;
```

- [ ] **Step 2: Add `pathsDefs()` implementation to PCSX2 adapter**

In `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`, add (near `findResumeFile`, around line 54):
```cpp
QVector<PathDef> Pcsx2LibretroAdapter::pathsDefs() const {
    // Three user-overridable folders. "libretro" section is informational
    // — ConfigService routes libretro adapters to PathOverridesStore, so
    // only the key identifies the override within the "pcsx2" namespace.
    // BIOS is intentionally not overridable per-emulator — see spec.
    return {
        { "Memory Cards", "libretro", "MemoryCards", "memcards",   PathBase::EmulatorData },
        { "Save States",  "libretro", "SaveStates",  "savestates", PathBase::EmulatorData },
        { "Textures",     "libretro", "Textures",    "textures",   PathBase::EmulatorData },
    };
}
```

- [ ] **Step 3: Update mGBA `pathsDefs()` — drop Screenshots, populate section/key**

In `cpp/src/adapters/libretro/mgba_libretro_adapter.cpp`, replace the existing `pathsDefs` body (around line 13–18):
```cpp
QVector<PathDef> MgbaLibretroAdapter::pathsDefs() const {
    // Screenshots row dropped — RetroNest has no gameplay-screenshot
    // capture, so an override would be UI for a feature that doesn't
    // exist. Re-add when/if screenshot capture lands.
    return {
        { "Saves",       "libretro", "Saves",      "saves",      PathBase::EmulatorData },
        { "Save states", "libretro", "SaveStates", "savestates", PathBase::EmulatorData },
    };
}
```

- [ ] **Step 4: Build to confirm overrides compile**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target RetroNest -j4 2>&1 | grep -E "error|Built target RetroNest$" | tail -5
```
Expected: `[100%] Built target RetroNest`.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.h \
        cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp \
        cpp/src/adapters/libretro/mgba_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
feat(adapters): PCSX2 pathsDefs (new); mGBA pathsDefs revised

PCSX2 gains 3 overridable paths (Memory Cards, Save States, Textures);
mGBA Screenshots row dropped (no consumer in RetroNest), Saves and
Save States now have non-empty section/key strings so the Save button
can persist them through ConfigService::savePaths.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: RetroNest env handlers — `GET_MEMCARDS_DIR` + `GET_TEXTURES_DIR`

**Files:**
- Modify: `cpp/src/core/libretro/environment_callbacks.h` (declare 2 new env constants)
- Modify: `cpp/src/core/libretro/environment_callbacks.cpp` (dispatch cases)
- Modify: `cpp/tests/test_environment_callbacks.cpp` (add 4 tests)

- [ ] **Step 1: Write failing tests**

In `cpp/tests/test_environment_callbacks.cpp`, add inside the `TestEnvironmentCallbacks` class (after the last `private slots:` test, before the closing `};`):
```cpp
    // PathOverridesStore-backed env enums (path-overrides feature).
    // The handlers consult PathOverridesStore directly, so tests need
    // to seed the store via its singleton. Test order assumes no
    // bleed-through across cases (each writes then clears).

    void testGetMemcardsDirReturnsOverrideWhenSet() {
        PathOverridesStore::instance().write("pcsx2", "MemoryCards", "/tmp/mc");
        EnvironmentContext ctx;
        const char* out = nullptr;
        QVERIFY(environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR, &out));
        QCOMPARE(QString(out), QString("/tmp/mc"));
        PathOverridesStore::instance().clear("pcsx2", "MemoryCards");
    }
    void testGetMemcardsDirReturnsFalseWhenUnset() {
        PathOverridesStore::instance().clear("pcsx2", "MemoryCards");
        EnvironmentContext ctx;
        const char* out = nullptr;
        QVERIFY(!environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR, &out));
    }
    void testGetTexturesDirReturnsOverrideWhenSet() {
        PathOverridesStore::instance().write("pcsx2", "Textures", "/tmp/tex");
        EnvironmentContext ctx;
        const char* out = nullptr;
        QVERIFY(environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR, &out));
        QCOMPARE(QString(out), QString("/tmp/tex"));
        PathOverridesStore::instance().clear("pcsx2", "Textures");
    }
    void testGetTexturesDirReturnsFalseWhenUnset() {
        PathOverridesStore::instance().clear("pcsx2", "Textures");
        EnvironmentContext ctx;
        const char* out = nullptr;
        QVERIFY(!environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR, &out));
    }
```

Add include at the top of the file (alongside existing includes):
```cpp
#include "core/path_overrides_store.h"
```

- [ ] **Step 2: Add env enum constants in header**

In `cpp/src/core/libretro/environment_callbacks.h`, add near the existing `RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH` constant (search for `0x20002`):
```cpp
// Path-override env queries. Each returns the user's override (set via
// the Paths settings UI; stored in PathOverridesStore) for one PCSX2-
// owned folder. Same shape as GET_BOOT_STATE_PATH (0x20002): data is
// const char**, returns true with *data set when override exists,
// false otherwise. The libretro core falls back to its save_dir-based
// default when false comes back, so non-RetroNest hosts and mGBA
// (which doesn't query these) keep working unchanged.
constexpr unsigned RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR =
    (3u | RETRO_ENVIRONMENT_PRIVATE);
constexpr unsigned RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR =
    (4u | RETRO_ENVIRONMENT_PRIVATE);
```

- [ ] **Step 3: Add dispatch cases in `environmentDispatch`**

In `cpp/src/core/libretro/environment_callbacks.cpp`, add include near the top:
```cpp
#include "core/path_overrides_store.h"
```

Add two new cases inside the `switch` in `environmentDispatch`, immediately after the `case RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH:` block:
```cpp
        case RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR:
        case RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR: {
            // PCSX2 path overrides. Looks up PathOverridesStore directly
            // (not ctx, so this also works for any future per-session
            // override propagation). Lifetime: thread_local cache keeps
            // the QByteArray storage alive for the duration of the env
            // call AND past it — matches the GET_BOOT_STATE_PATH pattern.
            if (!data) return false;
            const QString key =
                (cmd == RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR) ? QStringLiteral("MemoryCards")
                                                                : QStringLiteral("Textures");
            const QString path = PathOverridesStore::instance().read("pcsx2", key);
            if (path.isEmpty()) return false;
            thread_local QByteArray cached;
            cached = path.toUtf8();
            *static_cast<const char**>(data) = cached.constData();
            return true;
        }
```

- [ ] **Step 4: Build and run the updated test**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target test_environment_callbacks -j4
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/test_environment_callbacks
```
Expected: all existing tests pass + 4 new ones pass (total count = prior + 4).

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/libretro/environment_callbacks.h \
        cpp/src/core/libretro/environment_callbacks.cpp \
        cpp/tests/test_environment_callbacks.cpp
git commit -m "$(cat <<'EOF'
feat(libretro/env): GET_MEMCARDS_DIR + GET_TEXTURES_DIR private env

Two new private env enums (0x20003 / 0x20004) deliver user path
overrides from PathOverridesStore to the pcsx2-libretro core during
retro_load_game. Each returns the override as const char* when set;
returns false when unset so the core falls back to its save_dir-
based default. Shape mirrors GET_BOOT_STATE_PATH (0x20002), with the
same thread_local-cached QByteArray lifetime trick.

4 new dispatch tests pin the contract (override returned, no-override
returns false) for both enums.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Runtime consumption — Save States in GameSession

**Files:**
- Modify: `cpp/src/core/game_session.cpp` (`libretroSlotPath`, `terminate`)

- [ ] **Step 1: Add include**

Near the existing includes in `cpp/src/core/game_session.cpp`, add:
```cpp
#include "core/path_overrides_store.h"
```

- [ ] **Step 2: Wrap the savestates dir computation in `libretroSlotPath`**

In `GameSession::libretroSlotPath` (around `cpp/src/core/game_session.cpp:441-455`), find the line that builds the path (it uses `Paths::emulatorDataDir(...) + "/savestates"` or similar) and replace its directory construction with a helper-applied lookup. Replace the function body with:
```cpp
QString GameSession::libretroSlotPath(int slot) const {
    if (!m_libretroAdapter || !m_manifest || m_currentRomPath.isEmpty())
        return {};
    const QString systemId = Paths::systemIdFor(m_manifest->id, m_manifest->systems);
    const QString serial = m_libretroAdapter->extractSerial(m_currentRomPath);
    const QString baseName = serial.isEmpty()
        ? QFileInfo(m_currentRomPath).completeBaseName()
        : serial;
    // Path overrides: user-chosen dir trumps the default. The override
    // is stored per-emulator ("pcsx2" / "mgba") under the "SaveStates"
    // key; default is <emulator_data>/savestates.
    QString dir = PathOverridesStore::instance().read(m_manifest->id, "SaveStates");
    if (dir.isEmpty())
        dir = Paths::emulatorDataDir(m_manifest->id, systemId) + "/savestates";
    QDir().mkpath(dir);
    return dir + "/" + baseName + "_slot" + QString::number(slot) + ".state";
}
```

- [ ] **Step 3: Wrap the resume-file path in `terminate`**

In `GameSession::terminate` (around `cpp/src/core/game_session.cpp:394-415`), replace the existing `const QString resumePath = ...` line with override-aware construction. Replace:
```cpp
            const QString resumePath = Paths::emulatorDataDir(mf->id, systemId)
                + "/savestates/" + resumeName + ".resume";
            QDir().mkpath(QFileInfo(resumePath).absolutePath());
```
With:
```cpp
            QString dir = PathOverridesStore::instance().read(mf->id, "SaveStates");
            if (dir.isEmpty())
                dir = Paths::emulatorDataDir(mf->id, systemId) + "/savestates";
            const QString resumePath = dir + "/" + resumeName + ".resume";
            QDir().mkpath(dir);
```

- [ ] **Step 4: Build**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target RetroNest -j4 2>&1 | grep -E "error|Built target RetroNest$" | tail -5
```
Expected: `Built target RetroNest`.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/game_session.cpp
git commit -m "$(cat <<'EOF'
feat(game-session): consult PathOverridesStore for save-state dirs

Both libretroSlotPath (slot states) and terminate's resume-file
write now honor the SaveStates override in PathOverridesStore.
Affects both PCSX2 and mGBA — same code path, looked up per emuId.
Default <emulator_data>/savestates remains the fallback when no
override is set.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Runtime consumption — mGBA `save_dir` redirect

**Files:**
- Modify: `cpp/src/core/libretro/core_runtime.cpp` (compute `m_envCtx.saveDirectory` with override)

- [ ] **Step 1: Add include**

In `cpp/src/core/libretro/core_runtime.cpp`, add to existing includes:
```cpp
#include "core/path_overrides_store.h"
```

- [ ] **Step 2: Override `saveDirectory` for adapters that route Saves through it**

Find the existing assignment at `cpp/src/core/libretro/core_runtime.cpp:275`:
```cpp
m_envCtx.saveDirectory = m_cfg.saveDir.toUtf8();
```
Replace with:
```cpp
// Path overrides for libretro Saves (mGBA writes .srm directly to
// the libretro save_dir; redirecting save_dir is the propagation
// path). Only honor when the adapter exposes a "Saves" pathsDef —
// PCSX2 doesn't route through save_dir for memcards (those go via
// GET_MEMCARDS_DIR env), so a save_dir override here would break
// PCSX2's expected default location. Keyed by emulator id.
QString saveDir = m_cfg.saveDir;
{
    const QString override = PathOverridesStore::instance().read(m_cfg.emuId, "Saves");
    if (!override.isEmpty()) {
        QDir().mkpath(override);
        saveDir = override;
    }
}
m_envCtx.saveDirectory = saveDir.toUtf8();
```

- [ ] **Step 3: Confirm `StartConfig` has `emuId`**

Search `cpp/src/core/libretro/core_runtime.h` for `struct StartConfig`. If `QString emuId;` is not present, add it.

```
grep -A20 "struct StartConfig" /Users/mark/Documents/Projects/RetroNest-Project/cpp/src/core/libretro/core_runtime.h
```

If `emuId` is missing, add the field to `StartConfig` and populate it at the call site in `GameSession::startLibretro` (around `cpp/src/core/game_session.cpp:187`):
```cpp
cfg.emuId = manifest.id;
```

- [ ] **Step 4: Build**

```
cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 --target RetroNest -j4 2>&1 | grep -E "error|Built target RetroNest$" | tail -5
```
Expected: `Built target RetroNest`.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/libretro/core_runtime.cpp cpp/src/core/libretro/core_runtime.h cpp/src/core/game_session.cpp
git commit -m "$(cat <<'EOF'
feat(core-runtime): honor Saves override via libretro save_dir redirect

CoreRuntime::start now consults PathOverridesStore for the "Saves"
override per-emulator and redirects libretro save_dir to that path
when set. Used by mGBA (which writes .srm files directly to save_dir).
PCSX2 doesn't have a Saves override (memcards go through the
dedicated GET_MEMCARDS_DIR env enum), so this path is mGBA-only in
practice but stays generic for future libretro adapters with the
same shape.

Adds StartConfig::emuId field if not already present so the runtime
can scope the override lookup.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: pcsx2-libretro — env queries in `Settings::InitializeDefaults` + standalone test

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/Settings.cpp` (extract override helper + apply env queries)
- Create: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_settings_overrides.cpp`
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/.gitignore` (add new binary name)

This task lives in the `pcsx2-libretro` repo (not RetroNest-Project). All paths in this task are absolute or rooted there.

- [ ] **Step 1: Add env enum constants in pcsx2-libretro**

In `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/Settings.cpp`, near the top of the anonymous namespace (or just below existing includes), add:
```cpp
// RetroNest-private env enums for path overrides. Number must match
// RetroNest-Project's environment_callbacks.h exactly.
//   0x20003 = GET_MEMCARDS_DIR
//   0x20004 = GET_TEXTURES_DIR
// (0x20002 is GET_BOOT_STATE_PATH; see LibretroFrontend.cpp.)
// Each returns true with *data set to const char* when an override
// is configured; returns false otherwise. We fall through to the
// existing save_dir-based default when false comes back, so any
// host that doesn't implement these env enums keeps the prior
// behavior unchanged.
constexpr unsigned RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR =
    (3u | RETRO_ENVIRONMENT_PRIVATE);
constexpr unsigned RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR =
    (4u | RETRO_ENVIRONMENT_PRIVATE);

// Returns the override for `env_id` from the host, or empty string.
// Pulled out for standalone unit testing — see tools/test_settings_overrides.cpp.
std::string QueryPathOverride(retro_environment_t cb, unsigned env_id)
{
    if (!cb) return {};
    const char* out = nullptr;
    if (cb(env_id, &out) && out && out[0] != '\0')
        return out;
    return {};
}
```

- [ ] **Step 2: Apply the overrides in `InitializeDefaults`**

In the same file, locate the existing block that sets `Folders/MemoryCards` and `Folders/Textures` (around `pcsx2-libretro/Settings.cpp:284-305`). Currently it looks like:
```cpp
if (!save_dir.empty())
{
    const std::string memcards_dir = save_dir + "/memcards";
    g_si.SetStringValue("Folders", "MemoryCards", memcards_dir.c_str());

    const std::string textures_dir = save_dir + "/textures";
    g_si.SetStringValue("Folders", "Textures", textures_dir.c_str());
}
```

After that block, add:
```cpp
// Path-override layer: if the host advertises the new env enums
// (RetroNest does; RetroArch / other hosts don't), the user's chosen
// dir replaces the save_dir-derived default above. The env callback
// lives on the singleton FrontendState set in retro_set_environment;
// on hosts that don't implement these enums, QueryPathOverride
// returns empty and we keep the prior default.
{
    retro_environment_t cb = g_frontend.environ_cb;  // from LibretroFrontend.h
    const std::string mc = QueryPathOverride(cb, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR);
    if (!mc.empty())
        g_si.SetStringValue("Folders", "MemoryCards", mc.c_str());

    const std::string tex = QueryPathOverride(cb, RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR);
    if (!tex.empty())
        g_si.SetStringValue("Folders", "Textures", tex.c_str());
}
```

Settings.cpp lives in `namespace Pcsx2Libretro::Settings` and already includes `LibretroFrontend.h`, so `g_frontend` resolves to `Pcsx2Libretro::g_frontend` via the enclosing namespace.

- [ ] **Step 3: Create the standalone test**

Create `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/test_settings_overrides.cpp`:
```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for QueryPathOverride and the env-override
// fallback contract in Settings::InitializeDefaults. Doesn't link
// PCSX2 — tests the helper in isolation.
//
// Build:
//   clang++ -std=c++20 test_settings_overrides.cpp -o test_settings_overrides
//   ./test_settings_overrides

#include <cstdio>
#include <cstring>
#include <string>

// Stand-in libretro env signature.
using retro_environment_t = bool (*)(unsigned cmd, void* data);
static constexpr unsigned RETRO_ENVIRONMENT_PRIVATE = 0x20000;
static constexpr unsigned RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR = (3u | RETRO_ENVIRONMENT_PRIVATE);
static constexpr unsigned RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR = (4u | RETRO_ENVIRONMENT_PRIVATE);

// Helper under test (copy-paste-equivalent to the one in Settings.cpp).
// If the production helper changes, update this in lockstep.
static std::string QueryPathOverride(retro_environment_t cb, unsigned env_id)
{
    if (!cb) return {};
    const char* out = nullptr;
    if (cb(env_id, &out) && out && out[0] != '\0')
        return out;
    return {};
}

// Test scaffolding.
static int failures = 0;
static void expect(const std::string& got, const std::string& want, const char* label) {
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got=%s want=%s\n", label, got.c_str(), want.c_str());
        ++failures;
    }
}

// Mock env_cb factories.
static const char* g_mock_mc_value = nullptr;
static bool MockEnvWithMc(unsigned cmd, void* data) {
    if (cmd == RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR && g_mock_mc_value) {
        *static_cast<const char**>(data) = g_mock_mc_value;
        return true;
    }
    return false;
}
static bool MockEnvAlwaysFalse(unsigned, void*) { return false; }

int main() {
    expect(QueryPathOverride(nullptr, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR), "",
           "null cb returns empty");
    expect(QueryPathOverride(MockEnvAlwaysFalse, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR), "",
           "cb returns false → empty");

    g_mock_mc_value = "/Volumes/Ext/memcards";
    expect(QueryPathOverride(MockEnvWithMc, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR),
           "/Volumes/Ext/memcards",
           "cb returns true + path → that path");
    expect(QueryPathOverride(MockEnvWithMc, RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR), "",
           "cb returns false for wrong enum → empty");

    g_mock_mc_value = "";
    expect(QueryPathOverride(MockEnvWithMc, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR), "",
           "cb returns empty string → treated as unset");

    if (failures == 0)
        std::printf("test_settings_overrides: OK (5/5)\n");
    else
        std::printf("test_settings_overrides: %d FAILURES\n", failures);
    return failures;
}
```

- [ ] **Step 4: Add binary name to the tools gitignore**

Append to `/Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools/.gitignore` (insert in alphabetical position):
```
test_settings_overrides
```
Place it after `test_region_prefix` line, before `__pycache__/`.

- [ ] **Step 5: Build the standalone test and run it**

```
cd /Users/mark/Documents/Projects/pcsx2-libretro/pcsx2-libretro/tools
clang++ -std=c++20 test_settings_overrides.cpp -o test_settings_overrides
./test_settings_overrides
```
Expected: `test_settings_overrides: OK (5/5)` and exit code 0.

- [ ] **Step 6: Rebuild the libretro dylib**

```
cmake --build /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64 --target pcsx2_libretro -j4 2>&1 | grep -E "error|Built target pcsx2_libretro$" | tail -5
```
Expected: `Built target pcsx2_libretro`.

- [ ] **Step 7: Install the fresh dylib over the user runtime copy**

```
cp /Users/mark/Documents/Projects/pcsx2-libretro/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
stat -f "%Sm %z bytes" ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```
Expected: mtime matches the just-built dylib.

- [ ] **Step 8: Commit (in pcsx2-libretro repo)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add pcsx2-libretro/Settings.cpp \
        pcsx2-libretro/tools/test_settings_overrides.cpp \
        pcsx2-libretro/tools/.gitignore
git commit -m "$(cat <<'EOF'
feat(libretro): honor host-provided memcards/textures path overrides

Settings::InitializeDefaults now queries two new private libretro env
enums after the save_dir-based default block:
  RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR = 0x20003
  RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR = 0x20004

Each returns the user's per-folder override from RetroNest's
PathOverridesStore. When set, the override replaces the
<save_dir>/memcards (or /textures) default. Hosts that don't
implement the env enums (RetroArch, etc.) fall through to the
existing default unchanged.

Standalone unit test pins the helper contract: null cb / false
result / empty path all collapse to "unset"; truthy result with a
non-empty path passes through.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

(No push — pcsx2-libretro is a local-only fork.)

---

## Task 8: Build + integration smoke

This task verifies the full chain end-to-end with both binaries fresh. No code changes.

- [ ] **Step 1: Confirm both binaries are fresh**

```
stat -f "%Sm  %N" \
  ~/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
  ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```
Both mtimes should be from this session (after Task 7 install).

- [ ] **Step 2: Run all RetroNest unit tests**

```
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64
ctest --output-on-failure 2>&1 | tail -30
```
Expected: 100% pass. Both new tests (`PathOverridesStore`, the 4 new env_callbacks cases) appear in the pass list.

- [ ] **Step 3: Verify the Paths UI exposes PCSX2 + mGBA correctly**

Launch RetroNest:
```
~/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest 2>&1 | tee /tmp/retronest_paths_smoke.log
```
- Open Settings → Paths.
- Confirm **PCSX2 tab** exists and shows 3 rows: Memory Cards, Save States, Textures (each with a default path under `~/Documents/RetroNest/emulators/pcsx2/ps2/...`).
- Confirm **mGBA tab** shows 2 rows: Saves, Save states (no Screenshots row).
- Browse one PCSX2 row to a test dir (e.g. `/tmp/path_override_smoke/memcards`). Click Save.
- Quit RetroNest.

- [ ] **Step 4: Verify the override persisted to disk**

```
cat ~/Library/Application\ Support/RetroNest/path_overrides.json
```
Expected: JSON with `{"pcsx2": {"MemoryCards": "/tmp/path_override_smoke/memcards"}}` (or similar shape).

- [ ] **Step 5: Verify the override propagates to PCSX2 at runtime**

Relaunch RetroNest. Launch a PS2 game so PCSX2 boots.
```
grep -E "GET_MEMCARDS_DIR|Folders/MemoryCards|memcards" /tmp/retronest_paths_smoke.log | head
```
Expected (after relaunch): you should see the override env query fire from RetroNest's side, and PCSX2's settings log should show the override path applied. Confirm a new memcard write goes to `/tmp/path_override_smoke/memcards` (not the old default location).

- [ ] **Step 6: Reset the override and re-verify default behavior**

In RetroNest Settings → Paths → PCSX2 → Memory Cards → click Reset (or manually edit `path_overrides.json` to remove the key). Relaunch the game; verify memcards go back to the default location.

- [ ] **Step 7: Smoke test mGBA Saves override**

Similar to step 5 but for mGBA: set Saves override to `/tmp/path_override_smoke/mgba_saves`. Load a GBA game with a battery save (e.g. Pokemon). Confirm the `.srm` file ends up in the override dir, not the default.

- [ ] **Step 8: Push the RetroNest commits**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git log --oneline @{u}..HEAD   # confirm what's about to ship
git push origin main
```

(pcsx2-libretro commits stay local — no remote to push to.)

---

## Self-review against the spec

After completing all tasks, do the inline self-review (writing-plans skill, "Self-Review" section). For this plan specifically:

1. **Spec coverage** — every spec section maps to a task:
   - PathOverridesStore + JSON schema → Task 1
   - PCSX2 + mGBA pathsDefs → Task 3
   - ConfigService routing → Task 2
   - RetroNest env enums + dispatch → Task 4
   - Save States runtime consumption → Task 5
   - mGBA save_dir runtime consumption → Task 6
   - PCSX2 core env queries + Settings.cpp → Task 7
   - Tests, build, smoke → distributed; consolidated in Task 8
2. **Placeholder scan** — no TBD/TODO. Task 7 Step 2's env_cb access (`g_frontend.environ_cb` from `LibretroFrontend.h`) is concrete and verified against the existing namespace structure.
3. **Type consistency** — `PathOverridesStore::{read,write,clear}` signatures stable across all tasks; `PathDef` schema reused unchanged; env enum constants `0x20003` / `0x20004` consistent between RetroNest header (Task 4) and pcsx2-libretro Settings.cpp (Task 7).
