# Setup Wizard Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-skin the first-run setup wizard in the "Sunset premium" visual system and rework its steps (drop Resolution/Aspect, add RetroAchievements + ScreenScraper, make ROM/BIOS storage locations configurable with all-console scaffolding).

**Architecture:** Keep the existing `SetupWizard` QML module (separate first-run engine), `SwipeView` + `WizardTheme` singleton + per-page components + `NavBar`. Rework layers on top: extend `Paths` with independent ROM/BIOS roots (mirroring how `root`/`theme` already persist), add `SystemRegistry::allSystemIds()`, extend `WizardState`, re-skin `WizardTheme`, and rework pages.

**Tech Stack:** C++17, Qt6 (QML + Widgets), CMake, QtTest.

## Global Constraints

- Backend x86_64 build: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`. Test suite: `arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 --output-on-failure`. Never pipe build output (masks exit status).
- `Paths` is the single choke point for all managed dirs — do not derive ROM/BIOS paths anywhere else.
- ROM/BIOS roots: absent/empty config value ⇒ default to `{root}/roms` and `{root}/bios`. No migration for existing installs.
- Saves/memcards/etc. stay under `{root}/emulators/{emuId}/{systemId}/` — never moved by ROM/BIOS relocation.
- All ROM subfolders scaffolded = **every** `SystemRegistry` system id, not just selected emulators.
- Normalize every stored path with `QDir::cleanPath` (matches existing `setRoot`).
- Visual tokens: gradient `#ff5e8a → #7a2b6b → #241033`; accent gradient `#ffb057 → #ff5e8a`; amber step labels `#ffd0a6`; body `#f2c9d8`; glass surfaces `#ffffff` @ 8–14% + `#ffffff2b` borders; white pill CTA `#fff5f0`/`#3a1230`. Reference mockup: `.superpowers/brainstorm/*/content/b2-storage-page-v3.html`.
- Commit after every task with a `fix:`/`feat:`/`docs:` message + the standard Co-Authored-By / Claude-Session trailers.

---

## PHASE 1 — Backend architecture (TDD)

### Task 1: `SystemRegistry::allSystemIds()`

**Files:**
- Modify: `cpp/src/core/system_registry.h` (public interface, near `allRaConsoleIds()`)
- Modify: `cpp/src/core/system_registry.cpp`
- Test: `cpp/tests/test_system_registry.cpp`

**Interfaces:**
- Produces: `static QStringList SystemRegistry::allSystemIds();` — every registered system id.

- [ ] **Step 1: Write the failing test** — append to `test_system_registry.cpp` (inside the existing test class; follow the file's existing `QTest` style):

```cpp
void testAllSystemIdsReturnsEveryEntry() {
    QVERIFY(SystemRegistry::isLoaded());
    const QStringList ids = SystemRegistry::allSystemIds();
    // systems.json defines these among others; each must appear exactly once.
    QVERIFY(ids.contains("psx"));
    QVERIFY(ids.contains("ps2"));
    QVERIFY(ids.contains("gba"));
    QVERIFY(ids.contains("gbc"));
    QVERIFY(ids.contains("gb"));
    QCOMPARE(ids.count("psx"), 1);
    // Count matches the entry table size (no dupes, no drops).
    QVERIFY(ids.size() >= 5);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_system_registry -j 6 && arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 -R SystemRegistry --output-on-failure`
Expected: FAIL — `allSystemIds` not declared.

- [ ] **Step 3: Declare in header** — add to `system_registry.h` public section (after `allRaConsoleIds()`):

```cpp
    /** Every registered system id (keys of the entry table). */
    static QStringList allSystemIds();
```

Add `#include <QStringList>` if not already present.

- [ ] **Step 4: Implement in `.cpp`**:

```cpp
QStringList SystemRegistry::allSystemIds() {
    return s_entries.keys();
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 -R SystemRegistry --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/system_registry.h cpp/src/core/system_registry.cpp cpp/tests/test_system_registry.cpp
git commit -m "feat(system-registry): add allSystemIds() accessor"
```

---

### Task 2: `Paths` — independent ROM/BIOS roots

**Files:**
- Modify: `cpp/src/core/paths.h` (add statics + setters)
- Modify: `cpp/src/core/paths.cpp` (`romsDir`, `biosDir`, `ensureDirectories`, new setters)
- Test: `cpp/tests/test_paths_roots.cpp` (new; register in `cpp/CMakeLists.txt` test list)

**Interfaces:**
- Consumes: existing `Paths::setRoot`, `Paths::root`.
- Produces:
  - `static void Paths::setRomsRoot(const QString& path);`
  - `static void Paths::setBiosRoot(const QString& path);`
  - `static QString Paths::romsRoot();` / `static QString Paths::biosRoot();`
  - `romsDir(systemId)` now returns `romsRoot() + "/" + systemId`; `biosDir()` returns `biosRoot()`.
  - Default (unset) `romsRoot()` = `root()+"/roms"`, `biosRoot()` = `root()+"/bios"`.

- [ ] **Step 1: Write the failing test** — create `cpp/tests/test_paths_roots.cpp` (mirror an existing QtTest in `cpp/tests/` for the class boilerplate / `QTEST_APPLESS_MAIN`):

```cpp
#include <QtTest>
#include "core/paths.h"

class TestPathsRoots : public QObject {
    Q_OBJECT
private slots:
    void defaultsDeriveFromRoot() {
        Paths::setRoot("/tmp/rn-test");
        Paths::setRomsRoot("");   // unset ⇒ default
        Paths::setBiosRoot("");
        QCOMPARE(Paths::romsDir("gba"), QString("/tmp/rn-test/roms/gba"));
        QCOMPARE(Paths::biosDir(), QString("/tmp/rn-test/bios"));
    }
    void customRootsOverrideAndNormalize() {
        Paths::setRoot("/tmp/rn-test");
        Paths::setRomsRoot("/Volumes/USB//Games");   // doubled slash
        Paths::setBiosRoot("/Volumes/USB/bios/");     // trailing slash
        QCOMPARE(Paths::romsDir("psx"), QString("/Volumes/USB/Games/psx"));
        QCOMPARE(Paths::biosDir(), QString("/Volumes/USB/bios"));
    }
    void emptyResetsToDefault() {
        Paths::setRoot("/tmp/rn-test");
        Paths::setRomsRoot("/Volumes/USB/Games");
        Paths::setRomsRoot("");    // back to default
        QCOMPARE(Paths::romsDir(""), QString("/tmp/rn-test/roms"));
    }
};
QTEST_APPLESS_MAIN(TestPathsRoots)
#include "test_paths_roots.moc"
```

Register the test in `cpp/CMakeLists.txt` following the pattern used for `test_path_overrides_store` (add the executable + `add_test(NAME PathsRoots ...)`).

- [ ] **Step 2: Run to verify it fails**

Run: `arch -x86_64 /usr/local/bin/cmake -S cpp -B cpp/build-x86_64 -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2" && arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_paths_roots -j 6`
Expected: FAIL — `setRomsRoot`/`romsRoot` undefined.

- [ ] **Step 3: Add statics + declarations** to `paths.h` (in the class):

```cpp
    static void setRomsRoot(const QString& path);   // "" ⇒ default {root}/roms
    static void setBiosRoot(const QString& path);   // "" ⇒ default {root}/bios
    static QString romsRoot();
    static QString biosRoot();
```
and in the private section:
```cpp
    static QString s_romsRoot;   // empty ⇒ derive from s_root
    static QString s_biosRoot;
```

- [ ] **Step 4: Implement in `paths.cpp`** — add the statics near `QString Paths::s_root;`:

```cpp
QString Paths::s_romsRoot;
QString Paths::s_biosRoot;

void Paths::setRomsRoot(const QString& path) {
    s_romsRoot = path.isEmpty() ? QString() : QDir::cleanPath(path);
}
void Paths::setBiosRoot(const QString& path) {
    s_biosRoot = path.isEmpty() ? QString() : QDir::cleanPath(path);
}
QString Paths::romsRoot() {
    return s_romsRoot.isEmpty() ? (s_root + "/roms") : s_romsRoot;
}
QString Paths::biosRoot() {
    return s_biosRoot.isEmpty() ? (s_root + "/bios") : s_biosRoot;
}
```

Change `romsDir` and `biosDir`:
```cpp
QString Paths::biosDir() { return biosRoot(); }
QString Paths::romsDir(const QString& systemId) {
    if (systemId.isEmpty()) return romsRoot();
    return romsRoot() + "/" + systemId;
}
```

Update `ensureDirectories()` so the `dirs` list uses `romsRoot()`/`biosRoot()` (they already flow through `biosDir()`/`romsDir()`, so `biosDir()` and `romsDir()` in that list now point at the configured roots — verify the list includes both).

- [ ] **Step 5: Run to verify it passes**

Run: `arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 -R PathsRoots --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/paths.h cpp/src/core/paths.cpp cpp/tests/test_paths_roots.cpp cpp/CMakeLists.txt
git commit -m "feat(paths): independent configurable ROM/BIOS roots (default to {root}/roms|bios)"
```

---

### Task 3: `Paths` — persist ROM/BIOS roots in config.json

**Files:**
- Modify: `cpp/src/core/paths.h` / `paths.cpp` (load/save helpers)
- Test: extend `cpp/tests/test_paths_roots.cpp`

**Interfaces:**
- Produces: `loadSavedRomsRoot()`, `saveRomsRoot(const QString&)`, `loadSavedBiosRoot()`, `saveBiosRoot(const QString&)` — mirror the existing `loadSavedTheme`/`saveTheme` (read-modify-write on `config.json`, keys `romsRoot`/`biosRoot`). Empty return ⇒ caller uses default.

- [ ] **Step 1: Write the failing test** — add to `TestPathsRoots`:

```cpp
    void persistRoundTrips() {
        Paths::saveRomsRoot("/Volumes/USB/Games");
        Paths::saveBiosRoot("/Volumes/USB/bios");
        QCOMPARE(Paths::loadSavedRomsRoot(), QString("/Volumes/USB/Games"));
        QCOMPARE(Paths::loadSavedBiosRoot(), QString("/Volumes/USB/bios"));
        Paths::saveRomsRoot("");   // clearing persists empty ⇒ default on load
        QVERIFY(Paths::loadSavedRomsRoot().isEmpty());
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_paths_roots -j 6`
Expected: FAIL — `saveRomsRoot` undefined.

- [ ] **Step 3: Declare in `paths.h`**:

```cpp
    static QString loadSavedRomsRoot();
    static void saveRomsRoot(const QString& path);
    static QString loadSavedBiosRoot();
    static void saveBiosRoot(const QString& path);
```

- [ ] **Step 4: Implement in `paths.cpp`** (reuse the file's existing `readAppConfig`/`writeAppConfig` static helpers, exactly like `loadSavedTheme`/`saveTheme`):

```cpp
QString Paths::loadSavedRomsRoot() { return readAppConfig()["romsRoot"].toString(); }
void Paths::saveRomsRoot(const QString& path) {
    QJsonObject obj = readAppConfig();
    obj["romsRoot"] = path.isEmpty() ? QString() : QDir::cleanPath(path);
    writeAppConfig(obj);
}
QString Paths::loadSavedBiosRoot() { return readAppConfig()["biosRoot"].toString(); }
void Paths::saveBiosRoot(const QString& path) {
    QJsonObject obj = readAppConfig();
    obj["biosRoot"] = path.isEmpty() ? QString() : QDir::cleanPath(path);
    writeAppConfig(obj);
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 -R PathsRoots --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/paths.h cpp/src/core/paths.cpp cpp/tests/test_paths_roots.cpp
git commit -m "feat(paths): persist romsRoot/biosRoot in config.json"
```

---

### Task 4: Load saved ROM/BIOS roots at startup

**Files:**
- Modify: `cpp/src/main.cpp` (right after the existing `Paths::setRoot(rootPath)` at ~line 166, before `Paths::ensureDirectories()`)

**Interfaces:**
- Consumes: `Paths::loadSavedRomsRoot/BiosRoot`, `Paths::setRomsRoot/BiosRoot`.

- [ ] **Step 1: Add the load calls** — in `main.cpp`, immediately after `Paths::setRoot(rootPath)` succeeds and before `Paths::ensureDirectories();`:

```cpp
    // ROM/BIOS roots may live outside the data root (e.g. USB). Empty ⇒ default.
    Paths::setRomsRoot(Paths::loadSavedRomsRoot());
    Paths::setBiosRoot(Paths::loadSavedBiosRoot());
```

- [ ] **Step 2: Build**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Expected: `Built target RetroNest`.

- [ ] **Step 3: Verify behaviour** — with no `romsRoot` in config.json, launch and confirm `roms/` still resolves under the data root:

Run: `cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest > /tmp/rn.log 2>&1 &` then after ~8s `kill %1; grep -i "Root:" /tmp/rn.log`
Expected: root logged; no errors; app scans `{root}/roms` as before.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/main.cpp
git commit -m "feat(startup): load saved ROM/BIOS roots after root"
```

---

### Task 5: `WizardState` — ROM/BIOS location props + accept persistence + all-console scaffold

**Files:**
- Modify: `cpp/src/ui/wizard_state.h` (properties, methods, members)
- Modify: `cpp/src/ui/wizard_state.cpp` (`accept`, browse handlers, defaults)

**Interfaces:**
- Consumes: `Paths::setRoot/setRomsRoot/setBiosRoot/saveRoot/saveRomsRoot/saveBiosRoot/ensureDirectories/ensureRomDirectories`, `SystemRegistry::allSystemIds()`.
- Produces (QML-facing):
  - `Q_PROPERTY(QString romsRoot READ romsRoot WRITE setRomsRoot NOTIFY romsRootChanged)`
  - `Q_PROPERTY(QString biosRoot READ biosRoot WRITE setBiosRoot NOTIFY biosRootChanged)`
  - existing `rootPath`, `browseFolder`, `accept` unchanged in signature.

- [ ] **Step 1: Add properties/members** to `wizard_state.h`:

```cpp
    Q_PROPERTY(QString romsRoot READ romsRoot WRITE setRomsRoot NOTIFY romsRootChanged)
    Q_PROPERTY(QString biosRoot READ biosRoot WRITE setBiosRoot NOTIFY biosRootChanged)
    // ...
    QString romsRoot() const;   void setRomsRoot(const QString& p);
    QString biosRoot() const;   void setBiosRoot(const QString& p);
signals:
    void romsRootChanged();
    void biosRootChanged();
private:
    QString m_romsRoot;   // empty ⇒ {rootPath}/roms
    QString m_biosRoot;   // empty ⇒ {rootPath}/bios
```

- [ ] **Step 2: Implement getters/setters + default-follows-root** in `wizard_state.cpp`:

```cpp
QString WizardState::romsRoot() const {
    return m_romsRoot.isEmpty() ? (m_rootPath + "/roms") : m_romsRoot;
}
void WizardState::setRomsRoot(const QString& p) {
    const QString v = p.isEmpty() ? QString() : QDir::cleanPath(p);
    if (v == m_romsRoot) return;
    m_romsRoot = v; emit romsRootChanged();
}
QString WizardState::biosRoot() const {
    return m_biosRoot.isEmpty() ? (m_rootPath + "/bios") : m_biosRoot;
}
void WizardState::setBiosRoot(const QString& p) {
    const QString v = p.isEmpty() ? QString() : QDir::cleanPath(p);
    if (v == m_biosRoot) return;
    m_biosRoot = v; emit biosRootChanged();
}
```
In `setRootPath`, also `emit romsRootChanged(); emit biosRootChanged();` so the defaults track the data folder live (they derive from `m_rootPath`).

- [ ] **Step 3: Update `accept()`** to apply + persist the roots and scaffold every console. Replace the body so it does (in order): `Paths::setRoot(m_rootPath)`, `Paths::setRomsRoot(m_romsRoot)`, `Paths::setBiosRoot(m_biosRoot)`, `Paths::ensureDirectories()`, `Paths::ensureRomDirectories(SystemRegistry::allSystemIds())`, then persist `Paths::saveRoot(m_rootPath)`, `Paths::saveRomsRoot(m_romsRoot)`, `Paths::saveBiosRoot(m_biosRoot)`, then `emit wizardAccepted();`. Add `#include "core/system_registry.h"`.

- [ ] **Step 4: Build**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Expected: `Built target RetroNest`.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/wizard_state.h cpp/src/ui/wizard_state.cpp
git commit -m "feat(wizard): ROM/BIOS location state, accept persists roots + scaffolds all consoles"
```

---

## PHASE 2 — Visual system

### Task 6: `WizardTheme.qml` — B2 "Sunset premium" tokens

**Files:**
- Modify: `cpp/qml/SetupWizard/WizardTheme.qml`

- [ ] **Step 1: Replace the color block** (keep property names so existing pages inherit; add the new gradient/glass tokens):

```qml
    // Colors — Sunset premium (B2)
    readonly property color background:     "#241033"   // gradient tail / fallback
    readonly property color gradTop:        "#ff5e8a"
    readonly property color gradMid:        "#7a2b6b"
    readonly property color gradBottom:     "#241033"
    readonly property color surface:        "#ffffff14" // glass fill
    readonly property color surfaceHover:   "#ffffff22"
    readonly property color surfaceBorder:  "#ffffff2b"
    readonly property color accent:         "#ff5e8a"
    readonly property color accentLight:    "#ffb057"
    readonly property color navBackground:  "transparent"
    readonly property color cardSelected:   "#ffffff26"
    readonly property color textPrimary:    "#fff5f0"
    readonly property color textSecondary:  "#f2c9d8"
    readonly property color textMuted:      "#e7b7c7"
    readonly property color textDim:        "#ffd0a6"   // step labels
    readonly property color divider:        "#ffffff1f"
    readonly property color success:        "#3ec6a0"
    readonly property color error:          "#ff6b6b"
    readonly property color ctaBg:          "#fff5f0"
    readonly property color ctaText:        "#3a1230"
```
Bump sizes: `pageMargin: 72`, `pageTopMargin: 52`, `pillHeight: 56`, `pillRadius: 28`. Keep animation tokens.

- [ ] **Step 2: Build + eyeball**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Expected: `Built target RetroNest` (qmlcachegen recompiles WizardTheme with no errors).

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/SetupWizard/WizardTheme.qml
git commit -m "feat(wizard): B2 Sunset-premium theme tokens"
```

---

### Task 7: `Main.qml` — 6-page structure, gradient backdrop, window size, remove Resolution/Aspect

**Files:**
- Modify: `cpp/qml/SetupWizard/Main.qml`

- [ ] **Step 1: Window + backdrop** — set `width: 1180; height: 720; minimumWidth: 960; minimumHeight: 620;` and replace the flat/gradient background `Rectangle` with the B2 radial gradient (full-bleed, no inner bordered card):

```qml
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: WizardTheme.gradTop }
            GradientStop { position: 0.5; color: WizardTheme.gradMid }
            GradientStop { position: 1.0; color: WizardTheme.gradBottom }
        }
    }
```
(Use a radial look via a layered `RadialGradient` from Qt5Compat if a truer match is wanted; the vertical stops are an acceptable first pass — confirm against the mockup.)

- [ ] **Step 2: Content layout** — remove the centered `wizardCard` Rectangle; lay the header + progress + SwipeView + NavBar directly in a full-bleed `ColumnLayout` with `WizardTheme.pageMargin` insets. Keep the header (title + `N / pageCount`) and the accent progress bar.

- [ ] **Step 3: SwipeView pages** — set `property int pageCount: 6` and replace the page list:

```qml
                WelcomePage { isCurrentPage: SwipeView.isCurrentItem }
                StorageLocationsPage { id: storagePage }
                EmulatorsPage { id: emulatorsPage }
                RetroAchievementsPage { id: raPage }
                ScreenScraperPage { id: scraperPage }
                InstallPage { id: installPage; isCurrentPage: SwipeView.isCurrentItem }
```

- [ ] **Step 4: Titles + nav** — update `pageTitleForIndex`:

```qml
    function pageTitleForIndex(index) {
        var titles = ["Welcome", "Storage Locations", "Select Emulators",
                      "RetroAchievements", "ScreenScraper", "Install"]
        return titles[index] || ""
    }
```
In `NavBar.onContinueClicked`, delete the `resolutionPage.refresh()/aspectRatioPage.refresh()` block and the old `cur === 4` Files refresh. Add: on the emulators page index, no refresh needed. Keep `canContinue` gating the Storage page (index 1) on `wizard.rootPath !== ""`. Keep the `installPage.startInstall()` trigger on reaching the last index.

- [ ] **Step 5: Build**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Expected: `Built target RetroNest`. (Will fail to *load* at runtime until Tasks 8–10 add the three new pages — that's fine; qmlcachegen still compiles Main.qml. If qmlcachegen errors on the missing components, do Tasks 8–10 first then return to build.)

- [ ] **Step 6: Commit**

```bash
git add cpp/qml/SetupWizard/Main.qml
git commit -m "feat(wizard): 6-step flow, full-bleed B2 backdrop, larger window; drop Resolution/Aspect"
```

---

## PHASE 3 — Pages

### Task 8: `StorageLocationsPage.qml` (new)

**Files:**
- Create: `cpp/qml/SetupWizard/StorageLocationsPage.qml`
- Modify: `cpp/CMakeLists.txt` (add to the SetupWizard QML module file list)

**Interfaces:**
- Consumes: `wizard.rootPath` (RW), `wizard.romsRoot` (RW), `wizard.biosRoot` (RW), `wizard.browseFolder(title)`.

- [ ] **Step 1: Create the page** — a full-bleed page matching the approved mockup (`b2-storage-page-v3.html`): data-folder picker (path glass field + Browse), a collapsible "Customize storage locations (optional)" revealing ROMs + BIOS pickers with hint text, and the "we'll create a folder for every console" note. Use `WizardTheme` tokens for all colors/type. Browse buttons call e.g. `var p = wizard.browseFolder("Choose ROMs folder"); if (p) wizard.romsRoot = p;`. Path fields bind to `wizard.rootPath` / `wizard.romsRoot` / `wizard.biosRoot` (the latter two show the live default when uncustomized). Follow the existing page component structure (look at how `EmulatorsPage.qml` lays out and consumes `wizard`/`WizardTheme`).

- [ ] **Step 2: Register in CMake** — add `qml/SetupWizard/StorageLocationsPage.qml` to the `qt_add_qml_module(...)` `QML_FILES` list for the SetupWizard module (find the existing list that names `FolderPage.qml`, `EmulatorsPage.qml`, etc.).

- [ ] **Step 3: Build**

Run: `arch -x86_64 /usr/local/bin/cmake -S cpp -B cpp/build-x86_64 -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2" && arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Expected: `Built target RetroNest`.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/SetupWizard/StorageLocationsPage.qml cpp/CMakeLists.txt
git commit -m "feat(wizard): Storage Locations page (data root + optional ROM/BIOS locations)"
```

---

### Task 9: `RetroAchievementsPage.qml` (new) + expose RA login to the wizard engine

**Files:**
- Create: `cpp/qml/SetupWizard/RetroAchievementsPage.qml`
- Modify: `cpp/CMakeLists.txt` (QML module list)
- Modify: `cpp/src/main.cpp` (wizard engine `setContextProperty` for RA login)

**Interfaces:**
- Consumes: an RA login context object exposed to the wizard engine. Reuse the existing RA login mechanism (`RAClient` / the object Settings uses). Confirm the exact invokable during implementation by reading the Settings RA login call site.

- [ ] **Step 1: Expose the RA controller** — in `main.cpp`, in the first-run wizard engine block (where `WizardTheme`, `wizard`, `emulators`, `installer` are set as context properties), add the RA login object as a context property (reuse the same object the main app uses, or construct the minimal one the wizard needs). Name it e.g. `raLogin`.

- [ ] **Step 2: Create the page** — B2-styled: heading "Track your achievements", a short blurb, username/password glass fields, a "Log in" pill CTA, and a prominent **Skip** ghost action. On success show a confirmed state; on skip, do nothing (leave Settings path untouched). Use `WizardTheme` tokens.

- [ ] **Step 3: Register in CMake** — add `qml/SetupWizard/RetroAchievementsPage.qml` to the QML module list.

- [ ] **Step 4: Build**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Expected: `Built target RetroNest`.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/SetupWizard/RetroAchievementsPage.qml cpp/CMakeLists.txt cpp/src/main.cpp
git commit -m "feat(wizard): RetroAchievements step (log in or skip)"
```

---

### Task 10: `ScreenScraperPage.qml` (new) + expose scraper validation to the wizard engine

**Files:**
- Create: `cpp/qml/SetupWizard/ScreenScraperPage.qml`
- Modify: `cpp/CMakeLists.txt` (QML module list)
- Modify: `cpp/src/main.cpp` (wizard engine context property for scraper validation)

**Interfaces:**
- Consumes: the scraper credential-validation object (reuse `app.validateScraperCredentials` mechanism from `ScraperSettings.qml`). Confirm the exact object/invokable during implementation.

- [ ] **Step 1: Expose the scraper controller** — add the scraper validation object as a wizard-engine context property (e.g. `scraper`), reusing the existing validation code path.

- [ ] **Step 2: Create the page** — B2-styled, mirroring the RA page shape: heading "Box art & metadata", blurb about ScreenScraper, username/password glass fields, "Connect" pill CTA with a "Validating…" state, and a **Skip** ghost action.

- [ ] **Step 3: Register in CMake** — add `qml/SetupWizard/ScreenScraperPage.qml` to the QML module list.

- [ ] **Step 4: Build**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Expected: `Built target RetroNest`.

- [ ] **Step 5: Commit**

```bash
git add cpp/qml/SetupWizard/ScreenScraperPage.qml cpp/CMakeLists.txt cpp/src/main.cpp
git commit -m "feat(wizard): ScreenScraper step (set up or skip)"
```

---

### Task 11: Re-skin the reused pages + chrome to B2

**Files:**
- Modify: `cpp/qml/SetupWizard/WelcomePage.qml`, `EmulatorsPage.qml`, `EmulatorCard.qml`, `InstallPage.qml`, `NavBar.qml`, `PillButton.qml`, `StepIndicator.qml`

**Approach:** these consume `WizardTheme`, so Task 6 already carried the palette. This task fixes layout/type/structure to the B2 look, page by page. For each file: read it, then (a) ensure backgrounds are transparent (the gradient shows through — no opaque card fills), (b) bump titles to the large bold scale (~40–48px / 800, tracking −1px), (c) uppercase amber step/section labels, (d) glass surfaces for any tiles/fields (`WizardTheme.surface` + `WizardTheme.surfaceBorder`), (e) primary buttons use the white pill (`WizardTheme.ctaBg`/`ctaText`), Back uses ghost text.

- [ ] **Step 1: NavBar + PillButton** — restyle: Continue → white pill (`ctaBg`/`ctaText`, `pillRadius`), Back → ghost text (`textSecondary`). Remove any opaque `navBackground` fill (now transparent).
- [ ] **Step 2: WelcomePage** — big B2 hero title + subtitle + a "Get started" pill; transparent bg.
- [ ] **Step 3: EmulatorsPage + EmulatorCard** — cards become glass tiles; selected = `cardSelected` + accent border/check; big page title.
- [ ] **Step 4: InstallPage** — re-skin progress to the accent gradient; on completion show the ROM/BIOS folder locations (`wizard.romsRoot`, `wizard.biosRoot`) and a "drop your games in" line (fulfils the Finish requirement).
- [ ] **Step 5: StepIndicator** — if still used, restyle to the accent; otherwise the top progress bar in Main.qml suffices (delete StepIndicator usage if redundant).
- [ ] **Step 6: Build + full GUI review**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Then temporarily force first-run to preview the wizard (rename config.json aside): `mv "$HOME/Library/Application Support/RetroNest/config.json" /tmp/rn-config.bak; open cpp/build-x86_64/RetroNest.app` — click through all 6 steps, then restore: `mv /tmp/rn-config.bak "$HOME/Library/Application Support/RetroNest/config.json"`.
Expected: every step matches the B2 look; nav works; nothing opaque/clipped.

- [ ] **Step 7: Commit**

```bash
git add cpp/qml/SetupWizard/
git commit -m "feat(wizard): re-skin Welcome/Emulators/Install/nav to B2"
```

---

### Task 12: Remove retired pages + cleanup

**Files:**
- Delete: `cpp/qml/SetupWizard/ResolutionPage.qml`, `AspectRatioPage.qml`, `FolderPage.qml`, `FilesPage.qml`
- Modify: `cpp/CMakeLists.txt` (remove the deleted files from the QML module list)

- [ ] **Step 1: Confirm no references** — `grep -rn "ResolutionPage\|AspectRatioPage\|FolderPage\|FilesPage" cpp/qml cpp/src` returns nothing (Main.qml no longer instantiates them after Task 7).
- [ ] **Step 2: Delete the files + remove from CMake QML list.**
- [ ] **Step 3: Build**

Run: `arch -x86_64 /usr/local/bin/cmake -S cpp -B cpp/build-x86_64 -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2" && arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target RetroNest -j 6`
Expected: `Built target RetroNest`.

- [ ] **Step 4: Commit**

```bash
git add -A cpp/qml/SetupWizard/ cpp/CMakeLists.txt
git commit -m "chore(wizard): remove retired Resolution/Aspect/Folder/Files pages"
```

---

### Task 13: Full integration — tests + end-to-end GUI verification

- [ ] **Step 1: Full test suite**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 6 && arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 --output-on-failure`
Expected: 100% pass (incl. `SystemRegistry`, `PathsRoots`).

- [ ] **Step 2: End-to-end wizard run (default paths)** — force first-run (rename config.json aside), run the wizard accepting defaults, finish install. Verify: `ls "$HOME/Documents/RetroNest/roms"` contains a subfolder for **every** `SystemRegistry` system (not just selected). Confirm saves dir untouched.

- [ ] **Step 3: End-to-end wizard run (custom USB ROMs path)** — re-run first-run, in Storage → Customize point ROMs at a temp folder (`/tmp/rn-usb-roms`), finish. Verify all-console subfolders created there and `config.json` has `romsRoot`. Relaunch and confirm it scans the custom path.

- [ ] **Step 4: Existing-library-in-place** — pre-create `/tmp/rn-usb-roms/psx` with a dummy `.chd`; re-run pointing there; confirm the existing folder/file is untouched and used.

- [ ] **Step 5: RA + scraper skip and login paths** — verify Skip proceeds with no persisted creds; verify a login attempt hits the existing mechanism.

- [ ] **Step 6: Restore config** — `mv /tmp/rn-config.bak "$HOME/Library/Application Support/RetroNest/config.json"` (if aside).

- [ ] **Step 7: Final commit (if any touch-ups)**

```bash
git add -A && git commit -m "test(wizard): end-to-end verification touch-ups"
```

---

## Notes for the implementer
- The wizard runs in a **separate QML engine** on first run (`main.cpp`); new context objects (RA, scraper) must be registered on **that** engine, not the main AppUI engine.
- `Paths::romsDir`/`biosDir` are the only place ROM/BIOS paths are derived — never hard-code `{root}/roms` elsewhere.
- When in doubt on a page's structure, read a sibling page (`EmulatorsPage.qml`) for the established pattern before writing the new one.
