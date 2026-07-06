# Packet 7 Stage 3 — Registry + De-branching Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** System facts (display name, ScreenScraper ID, RA console ID) move into a single `manifests/systems.json` registry; emulator manifests gain `logo` + `detail_page` capabilities consumed as a row model; every `emuId === "dolphin"` / `"pcsx2"` branch and focus-index ternary leaves `EmulatorDetailPage.qml`; `EmulatorLogos.js` dies.

**Architecture:** A `SystemRegistry` static (mirrors the `Paths` pattern) loads `manifests/systems.json` at startup beside `ManifestLoader::loadAll` and replaces four competing truth sources (ThemeContext names, Scraper SS-IDs, RAClient console map, five per-adapter `raConsoleId()` overrides). `EmulatorManifest` gains `manifest_version` / `logo` / `detail_page` (controller pages + has_patches); a pure `detailActionRows()` helper in `retronest_core` turns manifest + install state into the QML row model, so the detail page becomes a Repeater with `isFocused: focusIndex === actionOffset + index`.

**Tech Stack:** C++17/Qt6, QtTest, QML.

**Spec:** `docs/superpowers/specs/2026-07-04-packet7-shared-contract-design.md` §Stage 3.

## Global Constraints

- x86_64 daily driver never left broken; **no pushes** until the user confirms the stage in-app; USER SMOKE GATE is a hard stop with a **standalone result report**.
- Fields are added only where a consumer exists today; speculative registry fields (bios requirements, max players, …) wait for a consumer (spec line 91).
- Builds/tests: `arch -x86_64 /usr/local/bin/cmake --build /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64 -j 6` (absolute path — cwd drifts; never `cmd; echo $?`, use `&& echo OK || echo FAILED`), `arch -x86_64 /usr/local/bin/ctest --test-dir …/cpp/build-x86_64 --output-on-failure`. Full builds run in background (macdeployqt hook exceeds the 10-min foreground cap).
- User-visible copy is preserved verbatim ("Refresh PCSX2 Patches", "GameCube Controller", …) — this stage moves data, it does not reword UI.
- Behavior invariants: unknown system → display name falls back to the raw systemId, ScreenScraper ID → −1, RA console ID → 0 at the session layer / −1 at the RAClient layer (today's exact contracts).

---

### Task 1: systems.json + SystemRegistry

**Files:**
- Create: `manifests/systems.json`
- Create: `cpp/src/core/system_registry.h`, `cpp/src/core/system_registry.cpp`
- Modify: `cpp/src/core/manifest_loader.cpp` (skip `systems.json` in `loadAll` — it lives in the same dir and would be rejected noisily as a malformed emulator manifest)
- Modify: `cpp/src/main.cpp:79-85` (load registry beside manifests)
- Modify: `cpp/CMakeLists.txt` (add sources to the `retronest_core` list; new test target)
- Test: `cpp/tests/test_system_registry.cpp`

**Interfaces (produces):**

```cpp
// system_registry.h — static holder, Paths-style. Loaded once at startup;
// lookups lowercase the key (matches today's .toLower() callers).
class SystemRegistry {
public:
    static bool load(const QString& jsonPath);        // false + qWarning on parse/shape error
    static bool loadFromData(const QByteArray& json); // test seam (also used by load())
    static bool isLoaded();
    static QString displayName(const QString& systemId);   // unknown → systemId unchanged
    static int screenScraperId(const QString& systemId);   // unknown → -1
    static int raConsoleId(const QString& systemId);        // unknown/none → -1
    static QList<int> allRaConsoleIds();                    // distinct ids of systems that declare one
private:
    struct Entry { QString name; int ssId = -1; int raId = -1; };
    static QHash<QString, Entry> s_entries;
};
```

`manifests/systems.json` shape — all 27 systems from `theme_context.cpp:252-283` with their ScreenScraper IDs from `scraper.cpp:74-99`; `ra_console_id` present on exactly the 8 systems `ra_client.cpp:334-346` + the five adapter overrides agree on (values already consistent between the two sources — verified during planning):

```json
{
  "format": 1,
  "systems": {
    "psx":          { "name": "PlayStation",     "screenscraper_id": 57,  "ra_console_id": 12 },
    "ps2":          { "name": "PlayStation 2",   "screenscraper_id": 58,  "ra_console_id": 21 },
    "psp":          { "name": "PSP",             "screenscraper_id": 61,  "ra_console_id": 41 },
    "nes":          { "name": "Nintendo NES",    "screenscraper_id": 3 },
    "snes":         { "name": "Super Nintendo",  "screenscraper_id": 4 },
    "n64":          { "name": "Nintendo 64",     "screenscraper_id": 14 },
    "gb":           { "name": "Game Boy",        "screenscraper_id": 9,   "ra_console_id": 4 },
    "gbc":          { "name": "Game Boy Color",  "screenscraper_id": 10,  "ra_console_id": 6 },
    "gba":          { "name": "Game Boy Advance","screenscraper_id": 12,  "ra_console_id": 5 },
    "nds":          { "name": "Nintendo DS",     "screenscraper_id": 15 },
    "3ds":          { "name": "Nintendo 3DS",    "screenscraper_id": 17 },
    "gc":           { "name": "GameCube",        "screenscraper_id": 13,  "ra_console_id": 16 },
    "wii":          { "name": "Wii",             "screenscraper_id": 16,  "ra_console_id": 19 },
    "wiiu":         { "name": "Wii U",           "screenscraper_id": 18 },
    "switch":       { "name": "Nintendo Switch", "screenscraper_id": 225 },
    "genesis":      { "name": "Sega Genesis",    "screenscraper_id": 1 },
    "saturn":       { "name": "Sega Saturn",     "screenscraper_id": 22 },
    "dreamcast":    { "name": "Dreamcast",       "screenscraper_id": 23 },
    "gamegear":     { "name": "Game Gear",       "screenscraper_id": 21 },
    "mastersystem": { "name": "Master System",   "screenscraper_id": 2 },
    "atari2600":    { "name": "Atari 2600",      "screenscraper_id": 26 },
    "atari7800":    { "name": "Atari 7800",      "screenscraper_id": 41 },
    "lynx":         { "name": "Atari Lynx",      "screenscraper_id": 28 },
    "jaguar":       { "name": "Atari Jaguar",    "screenscraper_id": 27 },
    "pcengine":     { "name": "PC Engine",       "screenscraper_id": 31 },
    "neogeo":       { "name": "Neo Geo",         "screenscraper_id": 142 },
    "arcade":       { "name": "Arcade",          "screenscraper_id": 75 }
  }
}
```

- [ ] **Step 1: Write the failing test** — `cpp/tests/test_system_registry.cpp`. Loads the REAL committed registry (path derived from `__FILE__`, like the schema guards):

```cpp
#include <QtTest>
#include <QFileInfo>
#include "core/system_registry.h"

class TestSystemRegistry : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        const QString path = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/../../manifests/systems.json";
        QVERIFY2(SystemRegistry::load(path), "manifests/systems.json failed to load");
        QVERIFY(SystemRegistry::isLoaded());
    }
    void displayNames() {
        QCOMPARE(SystemRegistry::displayName("psx"), QString("PlayStation"));
        QCOMPARE(SystemRegistry::displayName("GC"), QString("GameCube"));   // case-insensitive
        QCOMPARE(SystemRegistry::displayName("nosuch"), QString("nosuch")); // fallback = raw id
    }
    void screenScraperIds() {
        QCOMPARE(SystemRegistry::screenScraperId("psx"), 57);
        QCOMPARE(SystemRegistry::screenScraperId("arcade"), 75);
        QCOMPARE(SystemRegistry::screenScraperId("nosuch"), -1);
    }
    void raConsoleIds() {
        QCOMPARE(SystemRegistry::raConsoleId("ps2"), 21);
        QCOMPARE(SystemRegistry::raConsoleId("wii"), 19);
        QCOMPARE(SystemRegistry::raConsoleId("nes"), -1);   // system known, no RA id
        QCOMPARE(SystemRegistry::raConsoleId("nosuch"), -1);
    }
    void allRaConsoleIds_areTheEightSupported() {
        const QList<int> ids = SystemRegistry::allRaConsoleIds();
        QCOMPARE(QSet<int>(ids.begin(), ids.end()),
                 QSet<int>({4, 5, 6, 12, 16, 19, 21, 41}));
        QCOMPARE(ids.size(), 8);   // distinct — no dupes
    }
    void loadFromData_rejectsGarbage() {
        QVERIFY(!SystemRegistry::loadFromData("not json"));
        QVERIFY(!SystemRegistry::loadFromData("[1,2,3]"));
    }
};
QTEST_APPLESS_MAIN(TestSystemRegistry)
#include "test_system_registry.moc"
```

CMake (next to the other core test targets):

```cmake
add_executable(test_system_registry tests/test_system_registry.cpp)
target_link_libraries(test_system_registry PRIVATE retronest_core Qt6::Test)
add_test(NAME SystemRegistry COMMAND test_system_registry)
```

- [ ] **Step 2: Build the target — expect FAIL** (`system_registry.h` not found). Note: after editing CMakeLists run a reconfigure first (`arch -x86_64 /usr/local/bin/cmake -S …/cpp -B …/cpp/build-x86_64`) or the target is unknown to make.
- [ ] **Step 3: Write `manifests/systems.json`** exactly as above, and implement `system_registry.{h,cpp}`:

```cpp
// system_registry.cpp (shape; header per the interface block)
#include "system_registry.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

QHash<QString, SystemRegistry::Entry> SystemRegistry::s_entries;

bool SystemRegistry::load(const QString& jsonPath) {
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[SystemRegistry] cannot open" << jsonPath;
        return false;
    }
    return loadFromData(f.readAll());
}

bool SystemRegistry::loadFromData(const QByteArray& json) {
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[SystemRegistry] parse error:" << err.errorString();
        return false;
    }
    const QJsonObject systems = doc.object().value("systems").toObject();
    if (systems.isEmpty()) {
        qWarning() << "[SystemRegistry] no systems object";
        return false;
    }
    QHash<QString, Entry> entries;
    for (auto it = systems.begin(); it != systems.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        Entry e;
        e.name = o.value("name").toString();
        e.ssId = o.value("screenscraper_id").toInt(-1);
        e.raId = o.value("ra_console_id").toInt(-1);
        entries.insert(it.key().toLower(), e);
    }
    s_entries = std::move(entries);
    return true;
}

bool SystemRegistry::isLoaded() { return !s_entries.isEmpty(); }

QString SystemRegistry::displayName(const QString& systemId) {
    const auto it = s_entries.constFind(systemId.toLower());
    return (it != s_entries.constEnd() && !it->name.isEmpty()) ? it->name : systemId;
}
int SystemRegistry::screenScraperId(const QString& systemId) {
    return s_entries.value(systemId.toLower()).ssId;
}
int SystemRegistry::raConsoleId(const QString& systemId) {
    return s_entries.value(systemId.toLower()).raId;
}
QList<int> SystemRegistry::allRaConsoleIds() {
    QSet<int> ids;
    for (const auto& e : s_entries)
        if (e.raId > 0) ids.insert(e.raId);
    return QList<int>(ids.begin(), ids.end());
}
```

Add both files to the `retronest_core` source list in `cpp/CMakeLists.txt`.

- [ ] **Step 4: Loader skip + startup wiring.** In `manifest_loader.cpp` `loadAll` loop, immediately after `for (const auto& filename : files)`:

```cpp
        if (filename == QLatin1String("systems.json"))
            continue;   // system registry, not an emulator manifest (SystemRegistry::load)
```

In `main.cpp` directly after the `loader.loadAll(manifestsDir)` block:

```cpp
    if (!SystemRegistry::load(manifestsDir + "/systems.json")) {
        qCritical() << "Failed to load system registry from" << manifestsDir;
        return 1;
    }
```

(+ `#include "core/system_registry.h"`.)

- [ ] **Step 5: Build + run** `SystemRegistry` test → PASS; full suite green (49 + 1 new = 50).
- [ ] **Step 6: Commit** `packet7-3: manifests/systems.json + SystemRegistry (single source for system facts)`.

### Task 2: Rewire the four consumers, delete the five raConsoleId overrides

**Files:**
- Modify: `cpp/src/ui/theme_context.cpp:252-283` (`systemDisplayName`)
- Modify: `cpp/src/core/scraper.cpp:71-104` (`systemToScreenScraperId`)
- Modify: `cpp/src/core/ra_client.cpp:332-355` (delete `consoleIdMapping()`; rewire `raConsoleId` + `allConsoleIds`)
- Modify: `cpp/src/core/game_session.cpp:251` (registry instead of adapter virtual)
- Modify: `cpp/src/adapters/libretro/libretro_adapter.h:137-142` (delete the `raConsoleId` virtual) and delete the overrides: `mgba_libretro_adapter.{h,cpp}` (decl + `.cpp:206-211`), `dolphin_libretro_adapter.{h,cpp}` (decl + `.cpp:11-17`), `duckstation_libretro_adapter.h:16-21`, `pcsx2_libretro_adapter.h` (the `raConsoleId` inline ~line 25), `ppsspp_libretro_adapter.h:17-21`

**Interfaces (consumes):** `SystemRegistry::displayName/screenScraperId/raConsoleId/allRaConsoleIds` from Task 1.

- [ ] **Step 1:** Rewire bodies (each becomes a one-liner; keep the public signatures — themes/services compile untouched):

```cpp
// theme_context.cpp — replace the whole static QHash body of systemDisplayName:
QString ThemeContext::systemDisplayName(const QString& systemId) {
    return SystemRegistry::displayName(systemId);
}

// scraper.cpp — replace the whole static QHash body:
int Scraper::systemToScreenScraperId(const QString& system) {
    return SystemRegistry::screenScraperId(system);
}

// ra_client.cpp — delete static consoleIdMapping() entirely, then:
int RAClient::raConsoleId(const QString& systemId) {
    return SystemRegistry::raConsoleId(systemId);   // -1 when unsupported (same as before)
}
QList<int> RAClient::allConsoleIds() {
    return SystemRegistry::allRaConsoleIds();
}
```

(+ `#include "core/system_registry.h"` in each; scraper/ra_client are in core/ so `#include "system_registry.h"`.)

- [ ] **Step 2:** `game_session.cpp:251` — replace `cfg.raConsoleId = lr->raConsoleId(systemId);` with:

```cpp
    // RA console id comes from the system registry now (adapter overrides
    // retired in packet 7 stage 3). rcheevos treats 0 as "unknown console".
    const int raId = SystemRegistry::raConsoleId(systemId);
    cfg.raConsoleId = raId > 0 ? raId : 0;
```

- [ ] **Step 3:** Delete the base virtual + all five adapter overrides (decls, bodies, and their comments). `grep -rn "raConsoleId" cpp/src/adapters/` must return nothing afterward.
- [ ] **Step 4:** Full x86_64 rebuild (background) + full ctest → 50/50. Behavior checks that need no new tests: `test_system_registry` already pins every mapping value the deleted code carried.
- [ ] **Step 5: Commit** `packet7-3: system facts flow from the registry — 4 duplicate maps + 5 adapter overrides deleted`.

### Task 3: Manifest fattening + loader hardening

**Files:**
- Modify: `cpp/src/core/manifest.h` (new fields), `cpp/src/core/manifest_loader.cpp` (parse + unknown-key warning + version check)
- Modify: `manifests/{mgba,duckstation,ppsspp,dolphin,pcsx2}.json`
- Modify: `cpp/CMakeLists.txt` (add `qml/AppUI/images/mgba_logo.png` to BOTH qrc resource lists — it exists on disk but was never compiled in; grep `dolphin_logo.png` to find the two lists, ~lines 415/489)
- Test: extend `cpp/tests/test_manifest_libretro_fields.cpp`

**Interfaces (produces):**

```cpp
// manifest.h additions
struct ManifestControllerPage {
    QString label;   // detail-page button label, e.g. "GameCube Controller"
    QString type;    // controllerTypes() id passed to showControllerMapping; "" = default
};
struct EmulatorManifest {
    // ... existing fields unchanged ...
    int manifest_version = 0;                        // 0 = pre-versioning file (warned)
    QString logo;                                    // qrc path for tiles/popups ("" = none)
    QVector<ManifestControllerPage> controller_pages; // empty → single default "Controller Mapping"
    bool has_patches = false;                        // detail-page "Refresh <Name> Patches" action
};
```

- [ ] **Step 1: Failing tests** — add slots to `test_manifest_libretro_fields.cpp` (same `writeManifest` helper):

```cpp
    void testDetailPageFieldsParse() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "d.json", R"({
            "manifest_version":1,"id":"d","name":"D","systems":["s"],"github_repo":"o/r",
            "executable":"D","rom_extensions":["bin"],"launch_args":[],
            "logo":"qrc:/AppUI/qml/AppUI/images/dolphin_logo.png",
            "detail_page":{
                "controller_pages":[{"label":"GameCube Controller","type":"GCPad1"},
                                     {"label":"Wii Classic Controller","type":"Wiimote1"}],
                "has_patches":true}
        })");
        ManifestLoader loader;
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("d");
        QVERIFY(m != nullptr);
        QCOMPARE(m->manifest_version, 1);
        QCOMPARE(m->logo, QString("qrc:/AppUI/qml/AppUI/images/dolphin_logo.png"));
        QCOMPARE(m->controller_pages.size(), 2);
        QCOMPARE(m->controller_pages[0].label, QString("GameCube Controller"));
        QCOMPARE(m->controller_pages[0].type,  QString("GCPad1"));
        QVERIFY(m->has_patches);
    }
    void testDetailPageFieldsDefault() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "x.json", R"({
            "manifest_version":1,"id":"x","name":"X","systems":["s"],"github_repo":"o/r",
            "executable":"X","rom_extensions":["bin"],"launch_args":[]
        })");
        ManifestLoader loader;
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("x");
        QVERIFY(m != nullptr);
        QVERIFY(m->logo.isEmpty());
        QVERIFY(m->controller_pages.isEmpty());
        QVERIFY(!m->has_patches);
    }
    void testUnknownKeyWarns() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "u.json", R"({
            "manifest_version":1,"id":"u","name":"U","systems":["s"],"github_repo":"o/r",
            "executable":"U","rom_extensions":["bin"],"launch_args":[],
            "tpyo_field":true
        })");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression("unknown key.*tpyo_field"));
        ManifestLoader loader;
        loader.loadAll(dir.path());
        QVERIFY(loader.emulatorById("u") != nullptr);   // warn, don't reject
    }
    void testMissingVersionWarns() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "v.json", R"({
            "id":"v","name":"V","systems":["s"],"github_repo":"o/r",
            "executable":"V","rom_extensions":["bin"],"launch_args":[]
        })");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression("missing manifest_version"));
        ManifestLoader loader;
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("v");
        QVERIFY(m != nullptr);                          // grandfathered, not rejected
        QCOMPARE(m->manifest_version, 0);
    }
```

- [ ] **Step 2: Run — expect FAIL** (fields don't exist / no warnings emitted).
- [ ] **Step 3: Implement loader.** In `loadAll` after `const auto obj = doc.object();`:

```cpp
        // Loader hardening (packet 7 stage 3): version stamp + typo net.
        static const QSet<QString> kKnownKeys = {
            "manifest_version", "id", "name", "description", "systems",
            "github_repo", "executable", "install_folder", "rom_extensions",
            "launch_args", "backend", "core_dylib", "core_buildbot_path",
            "core_arch", "logo", "detail_page",
        };
        for (const QString& key : obj.keys()) {
            if (!kKnownKeys.contains(key))
                qWarning() << "[Manifest]" << filePath << "unknown key" << key
                           << "— ignored (typo?)";
        }
        m.manifest_version = obj.value("manifest_version").toInt(0);
        if (m.manifest_version == 0)
            qWarning() << "[Manifest]" << filePath << "missing manifest_version — treating as v0";
        else if (m.manifest_version > 1)
            qWarning() << "[Manifest]" << filePath << "manifest_version" << m.manifest_version
                       << "is newer than this build understands (1)";

        m.logo = obj.value("logo").toString();
        const QJsonObject dp = obj.value("detail_page").toObject();
        m.has_patches = dp.value("has_patches").toBool(false);
        for (const auto& v : dp.value("controller_pages").toArray()) {
            const QJsonObject po = v.toObject();
            ManifestControllerPage page;
            page.label = po.value("label").toString();
            page.type  = po.value("type").toString();
            if (!page.label.isEmpty())
                m.controller_pages.append(page);
        }
```

(+ manifest.h fields per the interface block.)

- [ ] **Step 4: Fatten the five real manifests.** Each gains `"manifest_version": 1` (first key) and `"logo": "qrc:/AppUI/qml/AppUI/images/<id>_logo.png"`. Additionally — dolphin:

```json
  "detail_page": {
    "controller_pages": [
      { "label": "GameCube Controller",     "type": "GCPad1" },
      { "label": "Wii Classic Controller",  "type": "Wiimote1" }
    ]
  }
```

pcsx2: `"detail_page": { "has_patches": true }`. Add `qml/AppUI/images/mgba_logo.png` to both qrc lists in `cpp/CMakeLists.txt`.

- [ ] **Step 5:** Build + tests PASS; run the app binary long enough to see `Loaded 5 emulator manifest(s)` with **zero unknown-key warnings** (proves the known-key list is complete against the real manifests).
- [ ] **Step 6: Commit** `packet7-3: manifest_version + logo + detail_page capabilities; loader warns on unknown keys`.

### Task 4: detailActionRows + QML de-branch (kills EmulatorLogos.js)

**Files:**
- Create: `cpp/src/core/detail_actions.h`, `cpp/src/core/detail_actions.cpp` (in `retronest_core` — pure data transform, testable)
- Modify: `cpp/src/ui/app_controller.h/.cpp` (`allEmulatorStatus` gains `logo` + `detailActions`; new `Q_INVOKABLE QString emulatorLogo(const QString&)`)
- Modify: `cpp/qml/AppUI/EmulatorDetailPage.qml` (Repeater row model; delete lines 31-96 branch logic + 380-458 ternaries)
- Modify: `cpp/qml/AppUI/EmulatorManageGrid.qml:54-79`, `cpp/qml/AppUI/AppWindow.qml:605` (logo from status/invokable)
- Delete: `cpp/qml/AppUI/EmulatorLogos.js` (+ its qrc entries in `cpp/CMakeLists.txt` — grep `EmulatorLogos`)
- Test: `cpp/tests/test_detail_actions.cpp`

**Interfaces (produces):**

```cpp
// detail_actions.h
#include <QVariantList>
#include "manifest.h"
// Ordered row model for EmulatorDetailPage's ACTIONS column. Each row:
//   { "action": <settings|controller|hotkeys|patches|reinstall|reset|uninstall>,
//     "label":  <button text>,
//     "controllerType": <type id for action=="controller", may be ""> }
// Not-installed → empty list (page shows GET STARTED instead). The BIOS
// button is NOT part of this model — it renders in the BIOS section and
// keeps its historical focus slot 0 via actionOffset.
QVariantList detailActionRows(const EmulatorManifest& m, bool installed, bool hasHotkeys);
```

Row order (matches today's visual order exactly): settings → controller page(s) → hotkeys (if hasHotkeys) → patches (if m.has_patches, label `"Refresh " + m.name + " Patches"` — yields the verbatim "Refresh PCSX2 Patches") → reinstall ("Reinstall / Update") → reset ("Reset Configuration") → uninstall ("Uninstall"). Controller pages: `m.controller_pages` if non-empty else one row `{label:"Controller Mapping", type:""}`.

- [ ] **Step 1: Failing test** `cpp/tests/test_detail_actions.cpp`:

```cpp
#include <QtTest>
#include "core/detail_actions.h"

class TestDetailActions : public QObject {
    Q_OBJECT
    static EmulatorManifest base(const QString& id, const QString& name) {
        EmulatorManifest m; m.id = id; m.name = name; return m;
    }
    static QStringList actions(const QVariantList& rows) {
        QStringList out;
        for (const auto& r : rows) out << r.toMap().value("action").toString();
        return out;
    }
private slots:
    void notInstalled_isEmpty() {
        QVERIFY(detailActionRows(base("x", "X"), false, true).isEmpty());
    }
    void plainEmulator_defaultChain() {
        const auto rows = detailActionRows(base("mgba", "mGBA"), true, true);
        QCOMPARE(actions(rows), QStringList({"settings","controller","hotkeys",
                                             "reinstall","reset","uninstall"}));
        QCOMPARE(rows[1].toMap()["label"].toString(), QString("Controller Mapping"));
        QCOMPARE(rows[1].toMap()["controllerType"].toString(), QString(""));
    }
    void noHotkeys_dropsRow() {
        const auto rows = detailActionRows(base("x", "X"), true, false);
        QVERIFY(!actions(rows).contains("hotkeys"));
    }
    void dolphin_twoControllerPages() {
        auto m = base("dolphin", "Dolphin");
        m.controller_pages = { {"GameCube Controller", "GCPad1"},
                               {"Wii Classic Controller", "Wiimote1"} };
        const auto rows = detailActionRows(m, true, true);
        QCOMPARE(actions(rows), QStringList({"settings","controller","controller",
                                             "hotkeys","reinstall","reset","uninstall"}));
        QCOMPARE(rows[2].toMap()["label"].toString(), QString("Wii Classic Controller"));
        QCOMPARE(rows[2].toMap()["controllerType"].toString(), QString("Wiimote1"));
    }
    void pcsx2_patchesRowAfterHotkeys() {
        auto m = base("pcsx2", "PCSX2");
        m.has_patches = true;
        const auto rows = detailActionRows(m, true, true);
        QCOMPARE(actions(rows), QStringList({"settings","controller","hotkeys","patches",
                                             "reinstall","reset","uninstall"}));
        QCOMPARE(rows[3].toMap()["label"].toString(), QString("Refresh PCSX2 Patches"));
    }
};
QTEST_APPLESS_MAIN(TestDetailActions)
#include "test_detail_actions.moc"
```

CMake target `test_detail_actions` linking `retronest_core Qt6::Test` + `add_test(NAME DetailActions …)`.

- [ ] **Step 2: Run — expect FAIL** (header missing). Reconfigure cmake for the new target.
- [ ] **Step 3: Implement** `detail_actions.cpp`:

```cpp
#include "detail_actions.h"

static QVariantMap row(const char* action, const QString& label,
                       const QString& controllerType = QString()) {
    return { {"action", action}, {"label", label}, {"controllerType", controllerType} };
}

QVariantList detailActionRows(const EmulatorManifest& m, bool installed, bool hasHotkeys) {
    if (!installed)
        return {};
    QVariantList rows;
    rows << row("settings", "Emulator Settings");
    if (m.controller_pages.isEmpty()) {
        rows << row("controller", "Controller Mapping");
    } else {
        for (const auto& page : m.controller_pages)
            rows << row("controller", page.label, page.type);
    }
    if (hasHotkeys)
        rows << row("hotkeys", "Hotkeys");
    if (m.has_patches)
        rows << row("patches", "Refresh " + m.name + " Patches");
    rows << row("reinstall", "Reinstall / Update");
    rows << row("reset", "Reset Configuration");
    rows << row("uninstall", "Uninstall");
    return rows;
}
```

Build; test PASS.

- [ ] **Step 4: AppController.** In `allEmulatorStatus()` (app_controller.cpp:480-510) add before `list.append(item)`:

```cpp
        item["logo"] = emu.logo;
        item["detailActions"] = detailActionRows(emu, item["installed"].toBool(),
                                                 hasHotkeys(emu.id));
```

New invokable (used by AppWindow's update popup, which only has an emuId):

```cpp
// .h (near hasHotkeys):
    Q_INVOKABLE QString emulatorLogo(const QString& emuId) const;
// .cpp:
QString AppController::emulatorLogo(const QString& emuId) const {
    const EmulatorManifest* m = m_loader->emulatorById(emuId);
    return m ? m->logo : QString();
}
```

- [ ] **Step 5: QML.** `EmulatorDetailPage.qml`:
  - Delete the `import "EmulatorLogos.js"` line, `patchesRefreshVisible`, and ALL emuId ternaries.
  - Add `property var detailActions: root.emuInfo.detailActions !== undefined ? root.emuInfo.detailActions : []` (refreshes with emuList/emuInfo).
  - `Keys.onPressed` maxIndex becomes: `var maxIndex = root.emuInfo.installed ? (actionOffset + detailActions.length - 1) : 0` (delete baseRows/actionRows).
  - `activateFocused()` becomes:

```qml
    function activateFocused() {
        if (!root.emuInfo.installed) {
            if (root.focusIndex === 0) installBtn.clicked()
            return
        }
        if (biosButtonVisible && root.focusIndex === 0) {
            app.openBiosFolder()
            return
        }
        runAction(detailActions[root.focusIndex - actionOffset])
    }
    function runAction(a) {
        if (!a) return
        switch (a.action) {
        case "settings":   app.showEmulatorSettings(root.emuId); break
        case "controller": a.controllerType ? app.showControllerMapping(root.emuId, a.controllerType)
                                            : app.showControllerMapping(root.emuId); break
        case "hotkeys":    app.showHotkeySettings(root.emuId); break
        case "patches":    app.refreshPcsx2Patches(); break
        case "reinstall":  root.beginInstall(); break
        case "reset":      resetDialog.open(); break
        case "uninstall":  uninstallDialog.open(); break
        }
    }
```

  - The eight hand-written `DetailButton`s in the ACTIONS column (lines 372-458) become ONE Repeater (styling switches on the action string — presentation only):

```qml
                    Repeater {
                        model: root.detailActions
                        DetailButton {
                            label: modelData.label
                            bgColor: modelData.action === "settings"  ? SettingsTheme.accent
                                   : modelData.action === "reinstall" ? SettingsTheme.accentDim
                                   : modelData.action === "uninstall" ? SettingsTheme.errorDim
                                   : SettingsTheme.card
                            textColor: modelData.action === "settings"  ? SettingsTheme.background
                                     : modelData.action === "reinstall" ? SettingsTheme.accent
                                     : modelData.action === "uninstall" ? SettingsTheme.error
                                     : SettingsTheme.text
                            isFocused: root.focusIndex === root.actionOffset + index
                            onClicked: root.runAction(modelData)
                        }
                    }
```

  - `beginInstall()` line 106: `progressPopup.logoSource = root.emuInfo.logo !== undefined ? root.emuInfo.logo : ""`.
  - `EmulatorManageGrid.qml`: `EmulatorLogos.logoForEmu(modelData.id)` → `modelData.logo` (3 sites) + drop the js import.
  - `AppWindow.qml:605`: `EmulatorLogos.logoForEmu(emuId)` → `app.emulatorLogo(emuId)` + drop the js import.
  - Delete `cpp/qml/AppUI/EmulatorLogos.js` and its qrc entries (grep `EmulatorLogos` in cpp/CMakeLists.txt — remove from both lists).
- [ ] **Step 6:** Full x86_64 rebuild (background, includes qml qrc) + full ctest → all green (52 targets now). Grep gate: `grep -rn 'emuId ===' cpp/qml/AppUI/EmulatorDetailPage.qml` returns ONLY hits unrelated to dolphin/pcsx2 identity (expected: none).
- [ ] **Step 7: Commit** `packet7-3: EmulatorDetailPage rendered from manifest-driven row model — emuId branching + EmulatorLogos.js retired`.

### Task 5: USER SMOKE GATE 10 (standalone report first)

- [ ] **Step 1:** Redeploy is implicit (POST_BUILD macdeployqt). Post the gate checklist as a STANDALONE message and STOP:
  1. Emulator manage grid: all five tiles show logos — **including mGBA for the first time**.
  2. Detail page per emulator with a GAMEPAD (focus regressions are the spec's named risk):
     - dolphin: rows = Settings / GameCube Controller / Wii Classic Controller / Hotkeys / Reinstall / Reset / Uninstall — d-pad walks every row in order, no dead or skipped focus stops, Enter activates the focused row.
     - pcsx2: "Refresh PCSX2 Patches" present after Hotkeys and works.
     - mgba (or any core w/o hotkeys if applicable): no Hotkeys row, focus chain contiguous.
     - BIOS button (visible on an emulator with missing required BIOS, e.g. fresh pcsx2 without BIOS — or skip if none applicable): still focus slot 0.
  3. Reinstall/update popup shows the right logo (AppWindow path).
  4. Game list / theme pages still show proper system display names (registry-backed now); a scrape and an RA panel still resolve (SS ids + RA ids from registry).
- [ ] **Step 2:** After the user passes the gate: update memory, standalone report with push status (push only on explicit say-so).

## Risks

| Risk | Mitigation |
|---|---|
| Focus-index regressions on detail page | Model-driven indices + gamepad-in-hand gate item per emulator shape |
| systems.json picked up as emulator manifest | Explicit `systems.json` skip in loadAll + test suite runs loadAll against real dir via app startup |
| Copy drift ("Refresh PCSX2 Patches") | Label built from manifest name pinned by test_detail_actions |
| RA fetch set changes | allRaConsoleIds pinned to the exact 8 ids in test_system_registry |
| mGBA logo qrc omission repeats | Task 3 Step 5 runs the app and eyeballs the tile at the gate |

## Effort

Task 1-2 ≈ half a session · Task 3-4 ≈ half a session · Gate ≈ user time.
