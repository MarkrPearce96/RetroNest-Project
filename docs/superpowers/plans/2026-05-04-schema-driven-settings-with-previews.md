# Schema-Driven Settings (with Previews) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a `GenericSettingsPage` that renders any emulator's settings from `adapter->settingsSchema()`, applied first to Dolphin (Interface + Audio + Core + Graphics with live aspect-ratio preview). PCSX2's preview widgets get promoted to shared `widgets/preview/` so any emulator can opt in.

**Architecture:** Single Qt page class consumes a category-filtered `QVector<SettingDef>` and an adapter `PreviewSpec`. Renders cards via the existing `SettingsPageBuilder` widget kit; loads/saves via `AppController`. Preview widgets exposed via `Q_PROPERTY` so the page can live-bind setting changes to preview properties without knowing concrete classes. Existing PCSX2/DuckStation/PPSSPP dialogs are behaviorally untouched in v1; only mechanical changes (include path updates, adapter `previewSpec()` overrides).

**Tech Stack:** C++17, Qt6 Widgets, CMake, QtTest. Reuses existing `SettingsCard`, `SettingsToggleRow`, `SettingsComboRow`, `SettingsSliderRow`, `SettingsSectionHeader`, `SettingsDescriptionBar`, `SettingsGraphicsSubTabBar`, `SettingsPageBuilder`, `SettingsDialogTheme`, `EmulatorSettingsDialogBase`, `EmulatorCategoryHubBase`.

**Reference spec:** `docs/superpowers/specs/2026-05-04-schema-driven-settings-with-previews-design.md`.

---

## Phase 1 — Foundation (data structures)

### Task 1: Add `PreviewSpec` struct + `previewSpec()` virtual to `EmulatorAdapter`

**Files:**
- Create: `cpp/src/core/preview_spec.h`
- Modify: `cpp/src/adapters/emulator_adapter.h`
- Test: (no new test — exercised in Task 6 + Task 9)

- [ ] **Step 1: Create `preview_spec.h`**

```cpp
// cpp/src/core/preview_spec.h
#pragma once

#include <QHash>
#include <QString>

/**
 * PreviewSpec — describes the live preview widget for a category/sub-tab.
 *
 * Returned by EmulatorAdapter::previewSpec(category, subcategory).
 * When previewType is empty, no preview is shown — GenericSettingsPage
 * renders the standard stacked card layout.
 *
 * keyToProperty maps a SettingDef::key (the bare INI key, e.g.
 * "AspectRatio") to a Q_PROPERTY name on the named preview widget
 * (e.g. "aspectMode"). When a setting widget for a mapped key changes,
 * GenericSettingsPage updates the preview via QObject::setProperty().
 */
struct PreviewSpec {
    QString previewType;                     // "aspect" | "osd" | "" (none)
    QHash<QString, QString> keyToProperty;   // SettingDef::key → Q_PROPERTY name
};
```

- [ ] **Step 2: Add the virtual to `EmulatorAdapter`**

In `cpp/src/adapters/emulator_adapter.h`, near the existing `settingsSchema()` virtual (around line 149), add:

```cpp
#include "core/preview_spec.h"

// ... inside class EmulatorAdapter ...

    /**
     * Return the preview-widget spec for one (category, subcategory) pair.
     * Default returns empty (no preview). Adapters that want an
     * AspectRatioPreview or OsdPreview shown alongside the settings cards
     * override this to return a non-empty PreviewSpec.
     */
    virtual PreviewSpec previewSpec(const QString& category,
                                    const QString& subcategory) const {
        Q_UNUSED(category);
        Q_UNUSED(subcategory);
        return {};
    }
```

- [ ] **Step 3: Build to verify nothing broke**

Run: `cd cpp && cmake --build build`
Expected: clean build, no errors.

- [ ] **Step 4: Run existing tests to confirm no regression**

Run: `cd cpp && ctest --test-dir build --output-on-failure`
Expected: all existing tests pass.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/core/preview_spec.h cpp/src/adapters/emulator_adapter.h
git commit -m "core: add PreviewSpec + EmulatorAdapter::previewSpec() virtual"
```

---

### Task 2: Add `saveTransform` field to `SettingDef`

**Files:**
- Modify: `cpp/src/core/setting_def.h`

- [ ] **Step 1: Add the field**

In `cpp/src/core/setting_def.h`, append to the `SettingDef` struct (before the closing brace, after `recommendedValue`):

```cpp
    // Optional. If set, GenericSettingsPage invokes this instead of the
    // default AppController::saveSettings() call when the widget value
    // changes. Used for settings whose stored format diverges from the
    // widget value (e.g. percent slider → numerator/denominator pair).
    // Default unset → standard save path.
    //
    // Signature avoids depending on AppController in this header by
    // taking a generic save callback the page passes through.
    using SaveCallback = std::function<void(const QString& section,
                                            const QString& key,
                                            const QString& value)>;
    std::function<void(const QString& widgetValue,
                       const SaveCallback& defaultSave)> saveTransform;
```

Add `#include <functional>` at the top of the file if not already present.

- [ ] **Step 2: Build**

Run: `cd cpp && cmake --build build`
Expected: clean build (no consumers yet).

- [ ] **Step 3: Run tests**

Run: `cd cpp && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/setting_def.h
git commit -m "core: add optional SettingDef::saveTransform escape hatch"
```

---

### Task 2b: Add per-key `iniFilePath` routing to `SettingDef` + `ConfigService`

**Files:**
- Modify: `cpp/src/core/setting_def.h`
- Modify: `cpp/src/services/config_service.cpp`
- Test: `cpp/tests/test_setting_def_ini_routing.cpp`

> Why: Dolphin's Graphics schema needs to read/write `GFX.ini` for most keys but `Dolphin.ini` for some (e.g. `Fullscreen` lives in `[Display]` section of `Dolphin.ini`). Today `ConfigService::settingValue()` and `saveSettings()` only know about `adapter->configFilePath()` — there's no per-key routing. Adding it once benefits every future emulator with multi-file config (Cemu, RPCS3, …).

- [ ] **Step 1: Write the failing test**

Create `cpp/tests/test_setting_def_ini_routing.cpp`:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/setting_def.h"
#include "core/ini_file.h"

// Smoke test of just the field — full ConfigService routing is exercised
// via DolphinSchema's Graphics keys writing to GFX.ini in the manual smoke.
class TestSettingDefIniRouting : public QObject {
    Q_OBJECT
private slots:
    void testIniFilePathFieldDefaultsEmpty() {
        SettingDef d;
        QVERIFY(d.iniFilePath.isEmpty());
    }

    void testIniFilePathRoundTrip() {
        SettingDef d;
        d.iniFilePath = "/tmp/example.ini";
        QCOMPARE(d.iniFilePath, QString("/tmp/example.ini"));
    }
};

QTEST_MAIN(TestSettingDefIniRouting)
#include "test_setting_def_ini_routing.moc"
```

Add the executable to `cpp/CMakeLists.txt`:

```cmake
add_executable(test_setting_def_ini_routing
    tests/test_setting_def_ini_routing.cpp
    src/core/ini_file.cpp
)
set_target_properties(test_setting_def_ini_routing PROPERTIES AUTOMOC ON)
target_include_directories(test_setting_def_ini_routing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_setting_def_ini_routing PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SettingDefIniRouting COMMAND test_setting_def_ini_routing)
```

- [ ] **Step 2: Add the field to `SettingDef`**

In `cpp/src/core/setting_def.h`, alongside the other optional-extension fields, add:

```cpp
    // Optional per-key INI file override. When non-empty, ConfigService
    // routes reads/writes for this setting to this absolute file path
    // instead of adapter->configFilePath(). Mirrors IniPatch::iniFilePath.
    // Used by emulators that span multiple config files (e.g. Dolphin's
    // GFX.ini for graphics keys vs Dolphin.ini for everything else).
    QString iniFilePath;
```

- [ ] **Step 3: Build + run unit test**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R SettingDefIniRouting --output-on-failure`
Expected: PASS.

- [ ] **Step 4: Update `ConfigService::settingValue()` to honor the field**

In `cpp/src/services/config_service.cpp`, replace the body of `settingValue()`:

```cpp
QString ConfigService::settingValue(const QString& emuId, const QString& section, const QString& key) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    // Resolve which file to read: per-SettingDef override if set, else
    // adapter->configFilePath().
    QString configPath;
    for (const auto& d : adapter->settingsSchema()) {
        if (d.section == section && d.key == key) {
            configPath = d.iniFilePath;
            break;
        }
    }
    if (configPath.isEmpty()) configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return {};

    if (m_settingsCache && m_settingsCachePath == configPath)
        return m_settingsCache->value(section, key);

    IniFile ini;
    ini.load(configPath);
    return ini.value(section, key);
}
```

- [ ] **Step 5: Update `ConfigService::saveSettings()` to group writes by file**

In `cpp/src/services/config_service.cpp`, replace the body of `saveSettings()`:

```cpp
void ConfigService::saveSettings(const QString& emuId, const QVariantMap& values) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    const QString defaultPath = adapter->configFilePath();
    if (defaultPath.isEmpty()) return;

    // Build a per-key file map from the schema — empty iniFilePath means
    // "use defaultPath".
    QHash<QPair<QString,QString>, QString> fileForKey;
    for (const auto& d : adapter->settingsSchema()) {
        if (!d.iniFilePath.isEmpty())
            fileForKey.insert({d.section, d.key}, d.iniFilePath);
    }

    // Group incoming values by destination file.
    QHash<QString, QMap<QString, QString>> writesByFile;  // file → "section/key" → value
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const int lastSlash = it.key().lastIndexOf('/');
        if (lastSlash <= 0) {
            qWarning() << "[Settings] Skipping malformed key:" << it.key();
            continue;
        }
        const QString section = it.key().left(lastSlash);
        const QString key     = it.key().mid(lastSlash + 1);
        const QString path    = fileForKey.value({section, key}, defaultPath);
        writesByFile[path].insert(section + "/" + key, it.value().toString());
    }

    // Apply each group to its file. Cache only the default file (the cache
    // existed for a single-file world; multi-file invalidates it for non-
    // default files — load fresh and save).
    bool ok = true;
    for (auto it = writesByFile.constBegin(); it != writesByFile.constEnd(); ++it) {
        const QString& path = it.key();
        const auto& entries = it.value();

        const bool useCache = (m_settingsCache && m_settingsCachePath == path);
        IniFile localIni;
        IniFile& ini = useCache ? *m_settingsCache : localIni;
        if (!useCache) ini.load(path);

        for (auto e = entries.constBegin(); e != entries.constEnd(); ++e) {
            const int lastSlash = e.key().lastIndexOf('/');
            const QString section = e.key().left(lastSlash);
            const QString key     = e.key().mid(lastSlash + 1);
            ini.setValue(section, key, e.value());
        }
        if (!ini.save(path)) ok = false;
    }

    emit statusMessage(ok ? "Settings saved." : "Failed to save settings.");
}
```

- [ ] **Step 6: Build + run all tests**

Run: `cd cpp && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all pass — single-file emulators (PCSX2/DuckStation/PPSSPP) unaffected because their schemas have empty `iniFilePath`.

- [ ] **Step 7: Manual smoke — existing emulators still save correctly**

Run: `open ./cpp/build/RetroNest.app`
Open PCSX2 Settings → tweak a setting → close → reopen → confirm persisted. Same for DuckStation and PPSSPP. (No `iniFilePath` set in their schemas → behavior identical to before.)

- [ ] **Step 8: Commit**

```bash
git add cpp/src/core/setting_def.h cpp/src/services/config_service.cpp \
        cpp/tests/test_setting_def_ini_routing.cpp cpp/CMakeLists.txt
git commit -m "settings: per-key iniFilePath routing in SettingDef + ConfigService

- Adds SettingDef::iniFilePath (mirrors IniPatch::iniFilePath).
- ConfigService::settingValue() looks up the matching SettingDef by
  section/key and uses its iniFilePath if non-empty; falls back to
  adapter->configFilePath().
- ConfigService::saveSettings() groups incoming writes by destination
  file so a single call can atomically update multiple INI files.
- Existing single-file emulators unaffected (empty iniFilePath = old
  behavior). Required for Dolphin Graphics schema to write to GFX.ini."
```

---

## Phase 2 — Promote PCSX2 preview widgets to shared (no behavior change)

### Task 3: Move + rename `Pcsx2AspectRatioPreview` to shared `AspectRatioPreview` with Q_PROPERTY

**Files:**
- Create: `cpp/src/ui/settings/widgets/preview/aspect_ratio_preview.h`
- Create: `cpp/src/ui/settings/widgets/preview/aspect_ratio_preview.cpp`
- Delete: `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h`
- Delete: `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp`
- Modify: `cpp/tests/test_pcsx2_aspect_ratio_preview.cpp` (rename + retarget)

- [ ] **Step 1: Create the new header at the shared path**

Copy `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h` to `cpp/src/ui/settings/widgets/preview/aspect_ratio_preview.h` with these changes:

- Class renamed: `Pcsx2AspectRatioPreview` → `AspectRatioPreview`
- Header guard / comment updated to drop the "pcsx2" prefix
- Add `Q_PROPERTY` declarations so `QObject::setProperty()` can drive the inputs:

```cpp
#pragma once
#include <QWidget>
#include <QRectF>

// Live aspect-ratio / crop / stretch / integer-scaling preview.
// Reproduces the draw-rect math from PCSX2 upstream GSRenderer on a fixed
// 640x448 NTSC source, scaled into a 16:9 widget. Every input is optional
// — unset values mean "feature absent" and the corresponding overlay is
// not drawn. PCSX2 sets every input; emulators with a narrower feature
// surface (e.g. Dolphin) set only what they expose.
class AspectRatioPreview : public QWidget {
    Q_OBJECT
    // Q_PROPERTYs let GenericSettingsPage live-bind via setProperty() without
    // depending on this concrete class. Names match PreviewSpec.keyToProperty
    // values declared by adapters.
    Q_PROPERTY(QString aspectMode READ aspectModeString WRITE setAspectModeString)
    Q_PROPERTY(int stretchY READ stretchY WRITE setStretchY)
    Q_PROPERTY(int cropL READ cropL WRITE setCropL)
    Q_PROPERTY(int cropT READ cropT WRITE setCropT)
    Q_PROPERTY(int cropR READ cropR WRITE setCropR)
    Q_PROPERTY(int cropB READ cropB WRITE setCropB)
    Q_PROPERTY(bool integerScaling READ integerScaling WRITE setIntegerScaling)
    Q_PROPERTY(QString fmvAspectMode READ fmvAspectModeString WRITE setFmvAspectModeString)
public:
    enum class AspectRatio {
        Stretch,
        Auto4_3_3_2,
        R4_3,
        R16_9,
        R10_7
    };

    explicit AspectRatioPreview(QWidget* parent = nullptr);

    // Direct typed setters — preferred when the caller already has the enum.
    void setAspectRatio(AspectRatio ratio);
    void setStretchY(int percent);                           // 10..300, default 100
    void setCrop(int left, int top, int right, int bottom);
    void setIntegerScaling(bool on);

    // Per-edge crop setters — used by Q_PROPERTY binding so each crop
    // edge can map to its own SettingDef key.
    void setCropL(int v) { setCrop(v, m_cropT, m_cropR, m_cropB); }
    void setCropT(int v) { setCrop(m_cropL, v, m_cropR, m_cropB); }
    void setCropR(int v) { setCrop(m_cropL, m_cropT, v, m_cropB); }
    void setCropB(int v) { setCrop(m_cropL, m_cropT, m_cropR, v); }

    // Q_PROPERTY string adapters — accept the schema's string value
    // (e.g. "16:9") and route to the typed setter.
    void setAspectModeString(const QString& v);
    QString aspectModeString() const;
    void setFmvAspectModeString(const QString& v);
    QString fmvAspectModeString() const;

    int stretchY() const         { return m_stretchY; }
    int cropL() const            { return m_cropL; }
    int cropT() const            { return m_cropT; }
    int cropR() const            { return m_cropR; }
    int cropB() const            { return m_cropB; }
    bool integerScaling() const  { return m_integerScaling; }

    static AspectRatio fromSchemaValue(const QString& v);

    QSize sizeHint() const override        { return QSize(320, 180); }
    QSize minimumSizeHint() const override { return QSize(240, 135); }

protected:
    void paintEvent(QPaintEvent* e) override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }

private:
    QRectF computeDrawRect(const QRectF& client) const;
    QString labelForCurrentRatio() const;

    AspectRatio m_ratio = AspectRatio::R4_3;
    AspectRatio m_fmvRatio = AspectRatio::R4_3;  // unused by paintEvent today; kept for API
    int m_stretchY      = 100;
    int m_cropL = 0, m_cropT = 0, m_cropR = 0, m_cropB = 0;
    bool m_integerScaling = false;
};
```

- [ ] **Step 2: Create the new .cpp at the shared path**

Copy `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp` to `cpp/src/ui/settings/widgets/preview/aspect_ratio_preview.cpp`. Make these changes:

- Update the `#include` to `"ui/settings/widgets/preview/aspect_ratio_preview.h"`.
- Rename every `Pcsx2AspectRatioPreview::` to `AspectRatioPreview::`.
- Add the new string-adapter implementations at the bottom of the file:

```cpp
void AspectRatioPreview::setAspectModeString(const QString& v) {
    setAspectRatio(fromSchemaValue(v));
}

QString AspectRatioPreview::aspectModeString() const {
    switch (m_ratio) {
        case AspectRatio::Stretch:     return "Stretch";
        case AspectRatio::Auto4_3_3_2: return "Auto 4:3/3:2";
        case AspectRatio::R4_3:        return "4:3";
        case AspectRatio::R16_9:       return "16:9";
        case AspectRatio::R10_7:       return "10:7";
    }
    return "4:3";
}

void AspectRatioPreview::setFmvAspectModeString(const QString& v) {
    m_fmvRatio = fromSchemaValue(v);
    update();
}

QString AspectRatioPreview::fmvAspectModeString() const {
    // Mirrors aspectModeString() shape — value held but not currently
    // affecting paint. Reserved for the same FMV-switch behavior PCSX2
    // exposed previously.
    switch (m_fmvRatio) {
        case AspectRatio::Stretch:     return "Stretch";
        case AspectRatio::Auto4_3_3_2: return "Auto 4:3/3:2";
        case AspectRatio::R4_3:        return "4:3";
        case AspectRatio::R16_9:       return "16:9";
        case AspectRatio::R10_7:       return "10:7";
    }
    return "4:3";
}
```

- [ ] **Step 3: Delete the old PCSX2 widget files**

```bash
git rm cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h \
       cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp
```

- [ ] **Step 4: Update the existing test to target the new class**

In `cpp/tests/test_pcsx2_aspect_ratio_preview.cpp`:

- Rename file: `git mv cpp/tests/test_pcsx2_aspect_ratio_preview.cpp cpp/tests/test_aspect_ratio_preview.cpp`
- Rename the class inside: `TestPcsx2AspectRatioPreview` → `TestAspectRatioPreview`
- Update `#include` to `"ui/settings/widgets/preview/aspect_ratio_preview.h"`
- Replace every `Pcsx2AspectRatioPreview` with `AspectRatioPreview` (preserve the `::AspectRatio::` enum references — those still work since the enum is now nested in `AspectRatioPreview`).

- [ ] **Step 5: Add a Q_PROPERTY round-trip test**

Append a new slot inside `TestAspectRatioPreview`:

```cpp
    void testQPropertyRoundTrip() {
        AspectRatioPreview w;
        // String aspect mode property
        QVERIFY(w.setProperty("aspectMode", "16:9"));
        QCOMPARE(w.property("aspectMode").toString(), QString("16:9"));
        // Int crop property — single edge
        QVERIFY(w.setProperty("cropL", 25));
        QCOMPARE(w.property("cropL").toInt(), 25);
        // Bool integer scaling
        QVERIFY(w.setProperty("integerScaling", true));
        QCOMPARE(w.property("integerScaling").toBool(), true);
        // stretchY clamp: 999 → 300
        QVERIFY(w.setProperty("stretchY", 999));
        QCOMPARE(w.property("stretchY").toInt(), 300);
    }
```

- [ ] **Step 6: Update CMakeLists.txt**

In `cpp/CMakeLists.txt`:

- Replace `src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp` (line ~91) with `src/ui/settings/widgets/preview/aspect_ratio_preview.cpp`.
- Replace `src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h` (line ~212) with `src/ui/settings/widgets/preview/aspect_ratio_preview.h`.
- In the `add_executable(test_pcsx2_aspect_ratio_preview ...)` block (line ~583), rename target to `test_aspect_ratio_preview`, update the source paths to the new locations, and rename the `add_test(NAME ...)` to `AspectRatioPreview`.

- [ ] **Step 7: Build**

Run: `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build`

Expected: clean build. The PCSX2 Display page will fail to build at this point because it still includes the old header path — that gets fixed in Task 5. To unblock building right now, leave Task 3 incomplete and proceed to Task 5 first if needed. Easier path: do the include update for the PCSX2 pages as part of Task 3 step 7 (see below).

- [ ] **Step 7b: Update PCSX2 Display page include + class refs (so build still passes)**

In `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.h` and `.cpp`:

- Replace `#include "../widgets/pcsx2_aspect_ratio_preview.h"` with `#include "ui/settings/widgets/preview/aspect_ratio_preview.h"`.
- Replace every `Pcsx2AspectRatioPreview` reference with `AspectRatioPreview`. The nested enum `::AspectRatio` reference (`Pcsx2AspectRatioPreview::fromSchemaValue(...)`) becomes `AspectRatioPreview::fromSchemaValue(...)` — same enum, new class name.

- [ ] **Step 8: Build again**

Run: `cd cpp && cmake --build build`
Expected: clean build.

- [ ] **Step 9: Run the renamed test**

Run: `cd cpp && ctest --test-dir build -R AspectRatioPreview --output-on-failure`
Expected: PASS (existing tests + new Q_PROPERTY round-trip test).

- [ ] **Step 10: Run all tests**

Run: `cd cpp && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 11: Manual smoke — PCSX2 Display page still works**

Run: `open ./cpp/build/RetroNest.app`
Open PCSX2 Settings → Graphics → Display sub-tab. Verify the aspect-ratio preview renders, that changing the AspectRatio combo updates the preview live, that crop sliders update the preview, and that integer scaling toggle updates the preview.

- [ ] **Step 12: Commit**

```bash
git add -A
git commit -m "settings: promote AspectRatioPreview to shared widgets/preview/

- Renames Pcsx2AspectRatioPreview to AspectRatioPreview.
- Moves out of pcsx2/widgets/ into shared widgets/preview/.
- Adds Q_PROPERTY exposure (aspectMode, stretchY, cropL/T/R/B,
  integerScaling, fmvAspectMode) so GenericSettingsPage can live-bind
  via setProperty() without depending on the concrete class.
- Adds string-adapter setters that accept schema values directly.
- Per-edge crop setters added so each crop key can map individually.
- PCSX2 Display page updated to include new path and class name.
- Existing test renamed/retargeted, plus new Q_PROPERTY round-trip test."
```

---

### Task 4: Move + rename `Pcsx2OsdPreview` to shared `OsdPreview` with Q_PROPERTY

**Files:**
- Create: `cpp/src/ui/settings/widgets/preview/osd_preview.h`
- Create: `cpp/src/ui/settings/widgets/preview/osd_preview.cpp`
- Delete: `cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.h`
- Delete: `cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.cpp`
- Modify: `cpp/tests/test_pcsx2_osd_preview.cpp` (rename + retarget)
- Modify: `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.{h,cpp}` (include + class refs)
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Read the existing PCSX2 OsdPreview header to discover its inputs**

Run: `cat cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.h`

Catalog every public setter — these become Q_PROPERTYs in the new class. From the page code (`pcsx2_graphics_osd_page.cpp:115-230`) the bindings include: `setShowFps`, `setShowSpeed`, `setShowVps`, `setShowResolution`, `setShowCpu`, `setShowGpu`, `setShowSettings`, `setShowPatches`, `setShowInputs`, `setShowFrameTimes`, `setShowIndicators`, `setShowGsStats`, `setShowHardwareInfo`, `setShowVersion`, `setPerformancePos(enum)`, `setMessagesPos(enum)` (TBD on bind site), `setOsdScale(int)`. Verify the actual full list by reading the existing header.

- [ ] **Step 2: Create the new header at shared path**

Copy `cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.h` to `cpp/src/ui/settings/widgets/preview/osd_preview.h` with:

- Class renamed: `Pcsx2OsdPreview` → `OsdPreview`
- Header guard / comment updated
- Add `Q_PROPERTY` declarations for every existing setter. For each existing `setShowXxx(bool)` add:
  ```cpp
  Q_PROPERTY(bool showXxx READ showXxx WRITE setShowXxx)
  ```
  with a corresponding `bool showXxx() const { return m_showXxx; }` accessor (add the member if it didn't exist as a getter).
- For `performancePos` / `messagesPos`: expose as string Q_PROPERTYs that route to the existing enum setters via a string→enum conversion (mirroring `AspectRatioPreview::setAspectModeString`).
- For `osdScale`: expose as `Q_PROPERTY(int osdScale READ osdScale WRITE setOsdScale)`.

The exact list comes from the existing header — copy it faithfully and add Q_PROPERTY for each.

- [ ] **Step 3: Create the new .cpp at shared path**

Copy `cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.cpp` to the new path. Update the `#include`, rename the class throughout (`Pcsx2OsdPreview::` → `OsdPreview::`), and add the new string-adapter implementations for `performancePos` / `messagesPos` (mirror the aspect-mode string adapter pattern from Task 3).

- [ ] **Step 4: Delete the old files**

```bash
git rm cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.h \
       cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.cpp
```

- [ ] **Step 5: Rename and retarget the test**

```bash
git mv cpp/tests/test_pcsx2_osd_preview.cpp cpp/tests/test_osd_preview.cpp
```

Update the file: rename the class (`TestPcsx2OsdPreview` → `TestOsdPreview`), update the `#include` to the new path, replace every `Pcsx2OsdPreview` with `OsdPreview`. Add a Q_PROPERTY round-trip test slot:

```cpp
    void testQPropertyRoundTrip() {
        OsdPreview w;
        QVERIFY(w.setProperty("showFps", true));
        QCOMPARE(w.property("showFps").toBool(), true);
        QVERIFY(w.setProperty("showSpeed", false));
        QCOMPARE(w.property("showSpeed").toBool(), false);
        QVERIFY(w.setProperty("osdScale", 150));
        QCOMPARE(w.property("osdScale").toInt(), 150);
        QVERIFY(w.setProperty("performancePos", "Top-Right"));
        QCOMPARE(w.property("performancePos").toString(), QString("Top-Right"));
    }
```

(Adjust the string values to match what the existing PCSX2 schema feeds today — the round-trip just needs to demonstrate setProperty works.)

- [ ] **Step 6: Update PCSX2 OSD page**

In `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.h` and `.cpp`:

- Replace `#include "../widgets/pcsx2_osd_preview.h"` with `#include "ui/settings/widgets/preview/osd_preview.h"`.
- Replace every `Pcsx2OsdPreview` with `OsdPreview` (including any nested type references like `Pcsx2OsdPreview::Pos`, which becomes `OsdPreview::Pos`).

- [ ] **Step 7: Update CMakeLists.txt**

Same shape as Task 3 step 6 — find the four PCSX2 osd_preview source/header lines and the `add_executable(test_pcsx2_osd_preview ...)` block; update paths and target/test names to drop the `pcsx2_` prefix.

- [ ] **Step 8: Build**

Run: `cd cpp && cmake --build build`
Expected: clean build.

- [ ] **Step 9: Run the renamed test**

Run: `cd cpp && ctest --test-dir build -R OsdPreview --output-on-failure`
Expected: PASS.

- [ ] **Step 10: Run all tests**

Run: `cd cpp && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 11: Manual smoke — PCSX2 OSD page still works**

Run: `open ./cpp/build/RetroNest.app`
Open PCSX2 Settings → Graphics → OSD sub-tab. Verify every toggle (FPS, Speed, VPS, Resolution, CPU, GPU, Settings, Patches, Inputs, Frame Times, Indicators, GS Stats, Hardware Info, Version) updates the preview. Verify performancePos and messagesPos combos move the correct labels in the preview.

- [ ] **Step 12: Commit**

```bash
git add -A
git commit -m "settings: promote OsdPreview to shared widgets/preview/

- Renames Pcsx2OsdPreview to OsdPreview.
- Moves out of pcsx2/widgets/ into shared widgets/preview/.
- Adds Q_PROPERTY exposure for every show* toggle, position combos,
  and osdScale so GenericSettingsPage can live-bind via setProperty().
- PCSX2 OSD page updated to include new path and class name.
- Existing test renamed/retargeted, plus new Q_PROPERTY round-trip test."
```

---

## Phase 3 — PCSX2 adapter `previewSpec()` (declarative, no behavior change)

### Task 5: Implement `Pcsx2Adapter::previewSpec()` for Display + OSD

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.h`
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp`
- Test: extend an existing PCSX2 schema test or create `cpp/tests/test_pcsx2_preview_spec.cpp`

> Why this task even though PCSX2 keeps its bespoke pages: declaring `previewSpec()` now means PCSX2 is ready for migration without code changes to its adapter. It also exercises the API end-to-end with real-world bindings, surfacing API gaps before Dolphin uses it.

- [ ] **Step 1: Write the failing test**

Create `cpp/tests/test_pcsx2_preview_spec.cpp`:

```cpp
#include <QtTest>
#include "adapters/pcsx2_adapter.h"

class TestPcsx2PreviewSpec : public QObject {
    Q_OBJECT
private slots:
    void testDisplayReturnsAspectPreview() {
        Pcsx2Adapter a;
        const auto spec = a.previewSpec("Graphics", "Display");
        QCOMPARE(spec.previewType, QString("aspect"));
        QVERIFY(spec.keyToProperty.contains("AspectRatio"));
        QCOMPARE(spec.keyToProperty.value("AspectRatio"), QString("aspectMode"));
        // Crop edges each map to their own preview property.
        QCOMPARE(spec.keyToProperty.value("CropLeft"),  QString("cropL"));
        QCOMPARE(spec.keyToProperty.value("CropRight"), QString("cropR"));
        QCOMPARE(spec.keyToProperty.value("IntegerScaling"), QString("integerScaling"));
    }

    void testOsdReturnsOsdPreview() {
        Pcsx2Adapter a;
        const auto spec = a.previewSpec("Graphics", "OSD");
        QCOMPARE(spec.previewType, QString("osd"));
        QCOMPARE(spec.keyToProperty.value("OsdShowFPS"),    QString("showFps"));
        QCOMPARE(spec.keyToProperty.value("OsdShowCPU"),    QString("showCpu"));
        QCOMPARE(spec.keyToProperty.value("OsdMessagesPos"),    QString("messagesPos"));
        QCOMPARE(spec.keyToProperty.value("OsdPerformancePos"), QString("performancePos"));
    }

    void testUnknownCategoryReturnsEmpty() {
        Pcsx2Adapter a;
        QVERIFY(a.previewSpec("Audio", "").previewType.isEmpty());
        QVERIFY(a.previewSpec("Graphics", "Rendering").previewType.isEmpty());
    }
};

QTEST_MAIN(TestPcsx2PreviewSpec)
#include "test_pcsx2_preview_spec.moc"
```

- [ ] **Step 2: Add the test to CMakeLists.txt**

In `cpp/CMakeLists.txt`, add after the `test_pcsx2_osd_preview` block:

```cmake
add_executable(test_pcsx2_preview_spec
    tests/test_pcsx2_preview_spec.cpp
    src/adapters/pcsx2_adapter.cpp
    src/adapters/emulator_adapter.cpp
    src/core/ini_file.cpp
    src/core/github_client.cpp
)
set_target_properties(test_pcsx2_preview_spec PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_preview_spec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_preview_spec PRIVATE Qt6::Core Qt6::Network Qt6::Test chdr-static)
add_test(NAME Pcsx2PreviewSpec COMMAND test_pcsx2_preview_spec)
```

(Mirror the source list of `test_dolphin_schema` from `cpp/CMakeLists.txt:512-524` — same dependency shape.)

- [ ] **Step 3: Run the test to confirm it fails**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R Pcsx2PreviewSpec --output-on-failure`
Expected: FAIL — `previewSpec()` not yet overridden, returns empty.

- [ ] **Step 4: Add the override declaration in `pcsx2_adapter.h`**

Near other virtual overrides, add:

```cpp
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
```

Add `#include "core/preview_spec.h"` if not already pulled in via `emulator_adapter.h`.

- [ ] **Step 5: Implement in `pcsx2_adapter.cpp`**

```cpp
PreviewSpec Pcsx2Adapter::previewSpec(const QString& category,
                                       const QString& subcategory) const {
    if (category == "Graphics" && subcategory == "Display") {
        return {"aspect", {
            {"AspectRatio",          "aspectMode"},
            {"FMVAspectRatioSwitch", "fmvAspectMode"},
            {"StretchY",             "stretchY"},
            {"CropLeft",             "cropL"},
            {"CropTop",              "cropT"},
            {"CropRight",            "cropR"},
            {"CropBottom",           "cropB"},
            {"IntegerScaling",       "integerScaling"},
        }};
    }
    if (category == "Graphics" && subcategory == "OSD") {
        return {"osd", {
            {"OsdShowFPS",                 "showFps"},
            {"OsdShowSpeed",               "showSpeed"},
            {"OsdShowVPS",                 "showVps"},
            {"OsdShowResolution",          "showResolution"},
            {"OsdShowCPU",                 "showCpu"},
            {"OsdShowGPU",                 "showGpu"},
            {"OsdShowSettings",            "showSettings"},
            {"OsdshowPatches",             "showPatches"},
            {"OsdShowInputs",              "showInputs"},
            {"OsdShowFrameTimes",          "showFrameTimes"},
            {"OsdShowIndicators",          "showIndicators"},
            {"OsdShowGSStats",             "showGsStats"},
            {"OsdShowHardwareInfo",        "showHardwareInfo"},
            {"OsdShowVersion",             "showVersion"},
            {"OsdShowVideoCapture",        "showVideoCapture"},
            {"OsdShowInputRec",            "showInputRec"},
            {"OsdShowTextureReplacements", "showTextureReplacements"},
            {"OsdMessagesPos",             "messagesPos"},
            {"OsdPerformancePos",          "performancePos"},
            {"OsdScale",                   "osdScale"},
        }};
    }
    return {};
}
```

(Reconcile the exact key spellings with what `Pcsx2Adapter::settingsSchema()` uses — those keys are the source of truth. If a key spelled here doesn't exist in the schema, the binding is harmless, but lining them up keeps grep findable.)

- [ ] **Step 6: Build + run the test**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R Pcsx2PreviewSpec --output-on-failure`
Expected: PASS.

- [ ] **Step 7: Run all tests**

Run: `cd cpp && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 8: Commit**

```bash
git add cpp/tests/test_pcsx2_preview_spec.cpp cpp/src/adapters/pcsx2_adapter.{h,cpp} cpp/CMakeLists.txt
git commit -m "pcsx2: declare PreviewSpec for Graphics/Display + Graphics/OSD

Adapter-level declaration only — PCSX2's bespoke pages still drive
the previews directly. The override exists so PCSX2 is migration-ready
and so the API gets exercised against real bindings before Dolphin uses it."
```

---

## Phase 4 — Dolphin schema extension

### Task 6: Add Dolphin Graphics Display sub-tab settings

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`
- Modify: `cpp/tests/test_dolphin_schema.cpp`

> Source of truth for these keys: `references/dolphin-master/Source/Core/DolphinQt/Settings/` and `references/dolphin-master/Source/Core/Core/Config/GraphicsSettings.cpp`. Use those panes as a catalog reference, NOT a code source.

- [ ] **Step 1: Catalog the Display keys**

Read `references/dolphin-master/Source/Core/DolphinQt/Settings/AdvancedPane.cpp` and `references/dolphin-master/Source/Core/Core/Config/GraphicsSettings.cpp`. The five Display sub-tab keys for v1, with their canonical `[Settings]` (or `[Hardware]`) section in `GFX.ini`:

| Schema key | INI section | Notes |
|---|---|---|
| `AspectRatio` | `Settings` | Already exposed today via `aspectRatioOptions()`; also lives in schema for the page. Values: `0`=Auto, `1`=16:9, `2`=4:3, `3`=Stretch. |
| `InternalResolution` | `Settings` | Already exposed today via `resolutionOptions()`; values 1..6. |
| `IntegerScaling` | `Settings` | Bool. Crisp pixel scaling. |
| `VSync` | `Hardware` | Bool. Already patched in `patchGfxIni()`. |
| `Fullscreen` | `Display` (lives in `Dolphin.ini`, not `GFX.ini`) | Bool. Already patched in `patchDolphinIni()`. **This key routes to a different file** — set `iniFilePath` per-SettingDef. |

Confirm exact key spellings against the upstream source before writing the schema entries.

- [ ] **Step 2: Write the failing test**

In `cpp/tests/test_dolphin_schema.cpp`, update `testTopLevelCategories` to expect the new category:

```cpp
    void testTopLevelCategories() {
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories, QSet<QString>({"Interface", "Audio", "Core", "Graphics"}));
    }
```

Add a new slot:

```cpp
    void testGraphicsDisplaySubTabExists() {
        // Note: Dolphin has no IntegerScaling GFX.ini key — InternalResolution
        // is already integer-by-definition (1..6 = 1x..6x). 4 keys, not 5.
        const QSet<QString> expectedKeys{
            "AspectRatio", "InternalResolution", "VSync", "Fullscreen"
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Display")
                got.insert(d.key);
        QCOMPARE(got, expectedKeys);
    }
```

- [ ] **Step 3: Run the test — confirm it fails**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R DolphinSchema --output-on-failure`
Expected: FAIL — Graphics category and Display sub-tab don't exist yet.

- [ ] **Step 4: Add the schema entries**

In `cpp/src/adapters/dolphin_adapter.cpp::settingsSchema()`, append (after the existing Core block). Each entry sets `iniFilePath` via the post-construction trick: build the entry, then assign the field. Cleaner alternative — define a small helper lambda `auto gfx = [&](SettingDef d){ d.iniFilePath = gfxIniPath(); return d; };` at the top of the function and wrap each Graphics/GFX entry:

```cpp
        // ─── Graphics / Display ──────────────────────────────
        // AspectRatio, InternalResolution, IntegerScaling, VSync live in
        // GFX.ini → wrap in gfx(). Fullscreen lives in Dolphin.ini →
        // omit gfx() so it inherits configFilePath() = Dolphin.ini.
        gfx({"Graphics", "Display", "", "Settings", "AspectRatio",
         "Aspect Ratio",
         "Display aspect ratio. Auto matches the game's native aspect; "
         "Stretch fills the screen.",
         SettingDef::Combo, "0",
         { {"Auto","0"}, {"Force 16:9","1"}, {"Force 4:3","2"}, {"Stretch","3"} }}),

        gfx({"Graphics", "Display", "", "Settings", "InternalResolution",
         "Internal Resolution",
         "Render scale relative to native (1x = original, 6x ≈ 4K).",
         SettingDef::Combo, "1",
         { {"Native (1x)","1"}, {"2x (~720p)","2"}, {"3x (~1080p)","3"},
           {"4x (~1440p)","4"}, {"5x (~1800p)","5"}, {"6x (~4K)","6"} }}),

        gfx({"Graphics", "Display", "", "Settings", "IntegerScaling",
         "Integer Scaling",
         "Scales the rendered image by integer multiples only — produces "
         "crisp pixels at the cost of unused screen area.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Display", "", "Hardware", "VSync",
         "VSync",
         "Synchronizes output to the display refresh rate. Reduces tearing.",
         SettingDef::Bool, "True"}),

        // No gfx() wrapper — Fullscreen is in Dolphin.ini's [Display] section.
        {"Graphics", "Display", "", "Display", "Fullscreen",
         "Fullscreen",
         "Render in fullscreen mode. RetroNest already runs Dolphin "
         "embedded in our window, so this is True by default.",
         SettingDef::Bool, "True"},
```

Add the helper at the top of `settingsSchema()`:

```cpp
QVector<SettingDef> DolphinAdapter::settingsSchema() const {
    auto gfx = [this](SettingDef d) { d.iniFilePath = gfxIniPath(); return d; };
    // ... existing options vectors ...
    return {
        // ... existing Interface, Audio, Core entries (unchanged) ...
        // ... new Graphics entries below ...
    };
}
```

This relies on Task 2b (`SettingDef::iniFilePath` field + ConfigService routing) being complete first.

- [ ] **Step 5: Build + run the test**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R DolphinSchema --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Run all tests**

Run: `cd cpp && ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp cpp/tests/test_dolphin_schema.cpp
git commit -m "dolphin: add Graphics/Display sub-tab to settings schema

Adds AspectRatio, InternalResolution, IntegerScaling, VSync (and
optionally Fullscreen — see plan task 6) under category=Graphics
subcategory=Display. Test extended to assert the new category/sub-tab."
```

---

### Task 7: Add Dolphin Graphics Rendering sub-tab settings

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`
- Modify: `cpp/tests/test_dolphin_schema.cpp`

- [ ] **Step 1: Catalog the Rendering keys**

Read `references/dolphin-master/Source/Core/DolphinQt/Config/Graphics/EnhancementsWidget.cpp` and `HacksWidget.cpp`. v1 list:

| Schema key | INI section | Type | Notes |
|---|---|---|---|
| `MSAA` | `Settings` | Combo | "1" (none) / "2" / "4" / "8" |
| `MaxAnisotropy` | `Enhancements` | Combo | "0" off / "1"=2x / "2"=4x / "3"=8x / "4"=16x |
| `ShaderCompilationMode` | `Settings` | Combo | "0" sync / "1" sync ubershaders / "2" async ubershaders / "3" skip drawing |
| `WaitForShadersBeforeStarting` | `Settings` | Bool | Pre-compile pipeline before launch |
| `EnablePixelLighting` | `Hacks` | Bool | Per-pixel lighting upgrade |

Verify spellings against upstream before writing.

- [ ] **Step 2: Write the failing test**

Append to `test_dolphin_schema.cpp`:

```cpp
    void testGraphicsRenderingSubTabExists() {
        const QSet<QString> expectedKeys{
            "MSAA", "MaxAnisotropy", "ShaderCompilationMode",
            "WaitForShadersBeforeStarting", "EnablePixelLighting"
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Rendering")
                got.insert(d.key);
        QCOMPARE(got, expectedKeys);
    }
```

- [ ] **Step 3: Run the test — fails**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R DolphinSchema`
Expected: FAIL.

- [ ] **Step 4: Append schema entries to `dolphin_adapter.cpp`**

All five Rendering keys live in GFX.ini → wrap each in the `gfx()` helper introduced in Task 6.

```cpp
        // ─── Graphics / Rendering ────────────────────────────
        gfx({"Graphics", "Rendering", "", "Settings", "MSAA",
         "Anti-Aliasing (MSAA)",
         "Multi-sample anti-aliasing. Higher = smoother edges, slower.",
         SettingDef::Combo, "1",
         { {"None","1"}, {"2x","2"}, {"4x","4"}, {"8x","8"} }}),

        gfx({"Graphics", "Rendering", "", "Enhancements", "MaxAnisotropy",
         "Anisotropic Filtering",
         "Sharpens textures viewed at oblique angles.",
         SettingDef::Combo, "0",
         { {"Off","0"}, {"2x","1"}, {"4x","2"}, {"8x","3"}, {"16x","4"} }}),

        gfx({"Graphics", "Rendering", "", "Settings", "ShaderCompilationMode",
         "Shader Compilation",
         "How shaders are compiled. Asynchronous reduces stutter at the "
         "cost of brief texture/lighting pop-in on first encounter.",
         SettingDef::Combo, "0",
         { {"Synchronous","0"}, {"Synchronous Ubershaders","1"},
           {"Asynchronous Ubershaders","2"}, {"Skip Drawing","3"} }}),

        gfx({"Graphics", "Rendering", "", "Settings", "WaitForShadersBeforeStarting",
         "Wait for Shaders Before Starting",
         "Pre-compiles the shader pipeline before launching a game. "
         "Slower start, smoother gameplay.",
         SettingDef::Bool, "False"}),

        gfx({"Graphics", "Rendering", "", "Hacks", "EnablePixelLighting",
         "Per-Pixel Lighting",
         "Higher-quality lighting. Slight performance cost; some games "
         "look noticeably better with it on.",
         SettingDef::Bool, "False"}),
```

- [ ] **Step 5: Build + test**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R DolphinSchema`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.cpp cpp/tests/test_dolphin_schema.cpp
git commit -m "dolphin: add Graphics/Rendering sub-tab to settings schema

MSAA, MaxAnisotropy, ShaderCompilationMode, WaitForShadersBeforeStarting,
EnablePixelLighting. Test asserts the new sub-tab key set."
```

---

### Task 8: Implement `DolphinAdapter::previewSpec()` for Graphics/Display

**Files:**
- Modify: `cpp/src/adapters/dolphin_adapter.h`
- Modify: `cpp/src/adapters/dolphin_adapter.cpp`
- Modify: `cpp/tests/test_dolphin_schema.cpp`

- [ ] **Step 1: Write failing test**

Append to `test_dolphin_schema.cpp`:

```cpp
    void testGraphicsDisplayHasAspectPreview() {
        DolphinAdapter a;
        const auto spec = a.previewSpec("Graphics", "Display");
        QCOMPARE(spec.previewType, QString("aspect"));
        QCOMPARE(spec.keyToProperty.value("AspectRatio"), QString("aspectMode"));
        // Dolphin doesn't expose stretch/crop/integer-scaling as separate
        // GFX.ini keys, so AspectRatio is the only binding for now. The
        // preview's other Q_PROPERTYs stay at their "feature absent" defaults.
    }

    void testGraphicsRenderingHasNoPreview() {
        DolphinAdapter a;
        QVERIFY(a.previewSpec("Graphics", "Rendering").previewType.isEmpty());
    }
```

- [ ] **Step 2: Confirm it fails**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R DolphinSchema`
Expected: FAIL — `previewSpec()` returns empty default.

- [ ] **Step 3: Add the override declaration**

In `cpp/src/adapters/dolphin_adapter.h`, near the other overrides:

```cpp
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
```

- [ ] **Step 4: Implement in `dolphin_adapter.cpp`**

```cpp
PreviewSpec DolphinAdapter::previewSpec(const QString& category,
                                         const QString& subcategory) const {
    if (category == "Graphics" && subcategory == "Display") {
        // Dolphin only exposes aspect ratio in a way that maps to the
        // shared AspectRatioPreview. Stretch / crop / integer-scaling are
        // not GFX.ini keys in Dolphin, so the preview shows just the
        // aspect rectangle and leaves the rest at "feature absent" defaults.
        return {"aspect", {
            {"AspectRatio", "aspectMode"},
        }};
    }
    return {};
}
```

- [ ] **Step 5: Build + test**

Run: `cd cpp && cmake --build build && ctest --test-dir build -R DolphinSchema`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/adapters/dolphin_adapter.{h,cpp} cpp/tests/test_dolphin_schema.cpp
git commit -m "dolphin: declare PreviewSpec for Graphics/Display (aspect preview)

Maps AspectRatio → aspectMode and IntegerScaling → integerScaling.
Stretch/crop properties on AspectRatioPreview stay at their 'feature
absent' defaults — Dolphin doesn't expose those keys."
```

---

## Phase 5 — `GenericSettingsPage`

### Task 9: Skeleton `GenericSettingsPage` — constructor, scroll area, back button, schema iteration

**Files:**
- Create: `cpp/src/ui/settings/generic_settings_page.h`
- Create: `cpp/src/ui/settings/generic_settings_page.cpp`

> No unit test for this skeleton — Qt UI assembly is validated end-to-end via Dolphin smoke in Task 17. Existing per-emulator pages are not unit-tested either; this matches house style.

- [ ] **Step 1: Create the header**

```cpp
// cpp/src/ui/settings/generic_settings_page.h
#pragma once
#include <QWidget>
#include "core/setting_def.h"
#include "core/preview_spec.h"

class EmulatorSettingsDialogBase;
class EmulatorAdapter;
class QStackedWidget;
class SettingsGraphicsSubTabBar;

/**
 * Schema-driven settings page used by every emulator's in-app dialog.
 *
 * Constructor inputs:
 *   - dlg: parent dialog (back-button target, save-callback owner via
 *     dlg->appController() and dlg->emuId()).
 *   - categorySchema: schema entries pre-filtered to one category. May
 *     contain multiple subcategories — each becomes a sub-tab.
 *   - adapter: queried for previewSpec(category, subcategory) per active
 *     sub-tab. Owned by caller; pointer must outlive this page.
 *
 * Behaviour: groups by subcategory then SettingDef::group, dispatches on
 * SettingDef::type to make Combo/Toggle/Slider cards (via SettingsPageBuilder),
 * loads values from AppController, saves on widget change (or via
 * SettingDef::saveTransform when set), live-binds preview properties,
 * handles dependsOn / bitmask / spatial nav.
 */
class GenericSettingsPage : public QWidget {
    Q_OBJECT
public:
    GenericSettingsPage(EmulatorSettingsDialogBase* dlg,
                        QVector<SettingDef> categorySchema,
                        EmulatorAdapter* adapter,
                        QWidget* parent = nullptr);
    ~GenericSettingsPage() override;

signals:
    void settingFocused(const SettingDef& def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void buildSubcategory(const QString& subcategory);
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    void refreshDependencies();

    EmulatorSettingsDialogBase* m_dlg = nullptr;
    EmulatorAdapter* m_adapter = nullptr;
    QString m_category;                       // common to every entry in m_schema
    QVector<SettingDef> m_schema;             // pre-filtered to one category
    QStringList m_subcategories;              // ordered, deduplicated
    SettingsGraphicsSubTabBar* m_subTabBar = nullptr;
    QStackedWidget* m_subStack = nullptr;     // one page per subcategory
    QWidget* m_currentPreview = nullptr;      // active preview widget, if any
};
```

- [ ] **Step 2: Create the .cpp skeleton (constructor + buildUi)**

```cpp
// cpp/src/ui/settings/generic_settings_page.cpp
#include "generic_settings_page.h"
#include "emulator_settings_dialog_base.h"
#include "settings_page_builder.h"
#include "widgets/settings_graphics_sub_tab_bar.h"
#include "widgets/settings_section_header.h"
#include "ui/app_controller.h"
#include "adapters/emulator_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>

GenericSettingsPage::GenericSettingsPage(EmulatorSettingsDialogBase* dlg,
                                         QVector<SettingDef> categorySchema,
                                         EmulatorAdapter* adapter,
                                         QWidget* parent)
    : QWidget(parent)
    , m_dlg(dlg)
    , m_adapter(adapter)
    , m_schema(std::move(categorySchema)) {
    if (!m_schema.isEmpty()) m_category = m_schema.front().category;

    // Discover ordered, unique subcategories (preserving first-seen order).
    QSet<QString> seen;
    for (const auto& d : m_schema) {
        if (!seen.contains(d.subcategory)) {
            seen.insert(d.subcategory);
            m_subcategories.append(d.subcategory);
        }
    }

    buildUi();
    loadValues();
    refreshDependencies();
}

GenericSettingsPage::~GenericSettingsPage() = default;

void GenericSettingsPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(SettingsPageBuilder::kScrollAreaQss);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    // Back button (matches existing pages — see duckstation_console_page.cpp:76-81)
    auto* back = new QPushButton("← Back", content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dlg, &EmulatorSettingsDialogBase::popPage);
    root->addWidget(back);

    // Sub-tab handling: if there's more than one subcategory, render a
    // SettingsGraphicsSubTabBar at the top + a QStackedWidget that swaps
    // on tab change.
    if (m_subcategories.size() > 1) {
        m_subTabBar = new SettingsGraphicsSubTabBar(content);
        for (const auto& s : m_subcategories) m_subTabBar->addTab("", s);
        root->addWidget(m_subTabBar);

        m_subStack = new QStackedWidget(content);
        for (const auto& s : m_subcategories) {
            auto* sub = new QWidget(m_subStack);
            new QVBoxLayout(sub);  // populated by buildSubcategory
            m_subStack->addWidget(sub);
        }
        root->addWidget(m_subStack);

        connect(m_subTabBar, &SettingsGraphicsSubTabBar::tabActivated,
                m_subStack, &QStackedWidget::setCurrentIndex);
    }

    for (int i = 0; i < m_subcategories.size(); ++i) {
        // Per-subcategory build happens here. Stub for now — populated in Task 10.
        Q_UNUSED(i);
    }

    root->addStretch();
}

void GenericSettingsPage::buildSubcategory(const QString& /*subcategory*/) {
    // Implemented in Task 10.
}

void GenericSettingsPage::loadValues() {
    // Implemented in Task 11.
}

void GenericSettingsPage::saveValue(const QString& /*section*/,
                                    const QString& /*key*/,
                                    const QString& /*value*/) {
    // Implemented in Task 11.
}

void GenericSettingsPage::refreshDependencies() {
    // Implemented in Task 13.
}

bool GenericSettingsPage::eventFilter(QObject* obj, QEvent* e) {
    return QWidget::eventFilter(obj, e);  // Spatial nav implemented in Task 14.
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

In `cpp/CMakeLists.txt`, find the source list (around line 91 with the other settings pages) and add:

```cmake
    src/ui/settings/generic_settings_page.cpp
```

And in the headers list (around line 212):

```cmake
    src/ui/settings/generic_settings_page.h
```

- [ ] **Step 4: Build**

Run: `cd cpp && cmake --build build`
Expected: clean build (no consumers yet — class compiles in isolation).

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/settings/generic_settings_page.{h,cpp} cpp/CMakeLists.txt
git commit -m "settings: add GenericSettingsPage skeleton

Constructor + chrome (back button, scroll area, sub-tab bar+stack when
multiple subcategories exist). Per-subcategory rendering, value
load/save, dependency resolution, spatial nav, and preview binding all
land in subsequent tasks."
```

---

### Task 10: Render section headers + dispatch on `SettingDef::type` to Combo/Toggle/Slider cards

**Files:**
- Modify: `cpp/src/ui/settings/generic_settings_page.cpp`

- [ ] **Step 1: Implement `buildSubcategory`**

Replace the stub in `cpp/src/ui/settings/generic_settings_page.cpp`:

```cpp
void GenericSettingsPage::buildSubcategory(const QString& subcategory) {
    // Identify the parent layout for this subcategory's content.
    QVBoxLayout* layout = nullptr;
    if (m_subStack) {
        const int idx = m_subcategories.indexOf(subcategory);
        layout = qobject_cast<QVBoxLayout*>(m_subStack->widget(idx)->layout());
    } else {
        // Single-subcategory case: append directly to the scroll content.
        // The root layout is the second-to-last item before stretch — find by walking.
        auto* scroll = findChild<QScrollArea*>();
        Q_ASSERT(scroll && scroll->widget());
        layout = qobject_cast<QVBoxLayout*>(scroll->widget()->layout());
    }
    Q_ASSERT(layout);

    SettingsPageBuilder builder(this, m_schema,
        [this](const QString& sec, const QString& k, const QString& v){ saveValue(sec, k, v); },
        [this](const SettingDef& d){ emit settingFocused(d); });

    // Group entries by SettingDef::group (preserving first-seen order).
    QStringList groupOrder;
    QSet<QString> seenGroups;
    for (const auto& d : m_schema) {
        if (d.subcategory != subcategory) continue;
        if (!seenGroups.contains(d.group)) {
            seenGroups.insert(d.group);
            groupOrder.append(d.group);
        }
    }

    for (const QString& group : groupOrder) {
        if (!group.isEmpty()) {
            layout->addWidget(new SettingsSectionHeader(group, this));
        }
        for (const auto& d : m_schema) {
            if (d.subcategory != subcategory || d.group != group) continue;
            QWidget* card = nullptr;
            switch (d.type) {
                case SettingDef::Combo:
                    card = builder.makeComboCard(d.key);
                    break;
                case SettingDef::Bool:
                    card = builder.makeToggleCard(d.key);
                    break;
                case SettingDef::Int:
                case SettingDef::Float:
                    if (d.layout == "slider") card = builder.makeSliderCard(d.key);
                    break;
                default:
                    break;
            }
            if (card) layout->addWidget(card);
        }
    }
    layout->addStretch();
}
```

- [ ] **Step 2: Wire `buildSubcategory()` into `buildUi()`**

In `buildUi()`, replace the empty for-loop with:

```cpp
    for (const QString& s : m_subcategories) buildSubcategory(s);
```

- [ ] **Step 3: Build**

Run: `cd cpp && cmake --build build`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/settings/generic_settings_page.cpp
git commit -m "settings: GenericSettingsPage builds cards from schema by type

Iterates the schema slice, groups by SettingDef::group, dispatches on
SettingDef::type to SettingsPageBuilder's makeComboCard/makeToggleCard/
makeSliderCard. Section headers rendered for non-empty groups."
```

---

### Task 11: Wire load + save through `AppController`

**Files:**
- Modify: `cpp/src/ui/settings/generic_settings_page.cpp`

- [ ] **Step 1: Implement `loadValues()`**

```cpp
void GenericSettingsPage::loadValues() {
    auto* app = m_dlg->appController();
    const QString emuId = m_dlg->emuId();

    // Combo, toggle, and slider rows are added by SettingsPageBuilder. Find
    // them by widget type — same approach used in duckstation_console_page.cpp:228-258.
    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        const QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* toggle : findChildren<SettingsToggleRow*>()) {
        const SettingDef& d = toggle->settingDef();
        const QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        toggle->setChecked(v.compare("true", Qt::CaseInsensitive) == 0
                       || v.compare("True", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<SettingsSliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        const QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        slider->setValue(v.toInt());
    }
}
```

Add the missing includes at the top of the file:

```cpp
#include "widgets/settings_combo_row.h"
#include "widgets/settings_toggle_row.h"
#include "widgets/settings_slider_row.h"
```

- [ ] **Step 2: Implement `saveValue()` with `saveTransform` escape hatch**

```cpp
void GenericSettingsPage::saveValue(const QString& section,
                                    const QString& key,
                                    const QString& value) {
    auto* app = m_dlg->appController();
    const QString emuId = m_dlg->emuId();

    // Default save path
    auto defaultSave = [app, emuId](const QString& sec, const QString& k,
                                    const QString& v) {
        QVariantMap m;
        m[sec + "/" + k] = v;
        app->saveSettings(emuId, m);
    };

    // saveTransform escape hatch: if a SettingDef for this key has one,
    // call it instead of the default save.
    for (const auto& d : m_schema) {
        if (d.section == section && d.key == key && d.saveTransform) {
            d.saveTransform(value, defaultSave);
            return;
        }
    }

    defaultSave(section, key, value);
}
```

- [ ] **Step 3: Build**

Run: `cd cpp && cmake --build build`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/settings/generic_settings_page.cpp
git commit -m "settings: GenericSettingsPage load+save through AppController

loadValues() walks combo/toggle/slider rows and sets initial state
from AppController::settingValue(). saveValue() goes through the
default save path unless a SettingDef::saveTransform is set."
```

---

### Task 12: Wire description bar focus signals

**Files:**
- Modify: `cpp/src/ui/settings/generic_settings_page.cpp`

The Combo/Toggle/Slider rows produced by `SettingsPageBuilder` already emit `focused(SettingDef)` signals — and `SettingsCard::focused` does too. The page just needs to forward them up to its own `settingFocused` signal so `EmulatorSettingsDialogBase` can pipe them into the description bar.

- [ ] **Step 1: Confirm builder already wires the focus connections**

Read `cpp/src/ui/settings/settings_page_builder.cpp` around its `makeComboCard`/`makeToggleCard`/`makeSliderCard` impls. The pattern (mirrored in `duckstation_console_page.cpp:100-101`) connects `card->focused` and the row's `focused` to the page-supplied `FocusFn` lambda, which in our case is `[this](const SettingDef& d){ emit settingFocused(d); }` (already passed in Task 10's `SettingsPageBuilder` ctor call).

If the builder already wires these — no work needed. Verify by inspection.

- [ ] **Step 2: Build + run a quick visual smoke (after Tasks 15+ are done)**

This task is essentially a verify-only — the wiring is implicit in how `SettingsPageBuilder` was invoked. Mark complete after Task 17's smoke confirms the description bar updates.

- [ ] **Step 3: (No commit)** — pure verification.

---

### Task 13: Implement dependency resolution (`dependsOn`)

**Files:**
- Modify: `cpp/src/ui/settings/generic_settings_page.cpp`

- [ ] **Step 1: Lift the existing logic from `duckstation_console_page.cpp:373-398`**

Replace the `refreshDependencies()` stub with:

```cpp
void GenericSettingsPage::refreshDependencies() {
    // Master state map: which dependsOn-target keys are currently 'on'.
    QHash<QString, bool> masterStates;
    for (auto* tog : findChildren<SettingsToggleRow*>())
        masterStates.insert(tog->settingDef().key, tog->isChecked());

    // Apply to dependent slider rows (same pattern as duckstation page).
    for (auto* slider : findChildren<SettingsSliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        if (d.dependsOn.isEmpty()) continue;
        const bool active = masterStates.value(d.dependsOn, true);
        slider->setProperty("dependencyActive", active);
        if (auto* inner = slider->findChild<QSlider*>()) inner->setEnabled(active);
        if (!active) {
            if (!slider->graphicsEffect()) {
                auto* eff = new QGraphicsOpacityEffect(slider);
                eff->setOpacity(0.4);
                slider->setGraphicsEffect(eff);
            }
        } else {
            slider->setGraphicsEffect(nullptr);
        }
    }
}
```

Add includes:

```cpp
#include <QGraphicsOpacityEffect>
#include <QSlider>
#include <QHash>
```

- [ ] **Step 2: Trigger refresh on toggle changes**

In Task 10's `buildSubcategory()`, every toggle card created via `SettingsPageBuilder::makeToggleCard()` already wires its `toggled` signal to `saveValue`. Add a side-call to `refreshDependencies()`. Easiest path: change `saveValue()` to call `refreshDependencies()` at the end. Less elegant but mirrors what existing pages do.

```cpp
void GenericSettingsPage::saveValue(const QString& section,
                                    const QString& key,
                                    const QString& value) {
    // ... existing impl ...

    refreshDependencies();
}
```

- [ ] **Step 3: Build + commit**

```bash
cd cpp && cmake --build build
git add cpp/src/ui/settings/generic_settings_page.cpp
git commit -m "settings: GenericSettingsPage dependency resolution (dependsOn)

Lifted from duckstation_console_page.cpp:373-398. Toggle changes
re-evaluate which dependent rows are active, dim+disable inactive ones."
```

---

### Task 14: Implement spatial arrow-key navigation

**Files:**
- Modify: `cpp/src/ui/settings/generic_settings_page.cpp`

- [ ] **Step 1: Lift the existing eventFilter from `duckstation_console_page.cpp:266-371`**

Replace the `eventFilter()` stub with the full spatial-nav implementation from `duckstation_console_page.cpp` (lines 266-371). Adapt these references:

- `DuckStationConsolePage` → `GenericSettingsPage`
- `findNextCardSpatial` becomes a private method on `GenericSettingsPage` — copy it verbatim (lines 305-371) but rename the cast in the lambda to `const_cast<GenericSettingsPage*>(this)`.

Add the `findNextCardSpatial` declaration to the header:

```cpp
// In generic_settings_page.h, near other private methods:
SettingsCard* findNextCardSpatial(SettingsCard* current, int key) const;
```

Add the missing include:

```cpp
#include <QApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include <limits>
```

- [ ] **Step 2: Install/uninstall the app-level filter in ctor/dtor**

In ctor (after `refreshDependencies()`):

```cpp
    qApp->installEventFilter(this);
```

In dtor:

```cpp
GenericSettingsPage::~GenericSettingsPage() {
    qApp->removeEventFilter(this);
}
```

- [ ] **Step 3: Build + commit**

```bash
cd cpp && cmake --build build
git add cpp/src/ui/settings/generic_settings_page.{h,cpp}
git commit -m "settings: GenericSettingsPage spatial arrow-key navigation

Lifted from duckstation_console_page.cpp:266-371. Same logic, single
shared implementation. Per-emulator pages can keep theirs for now or
delete in their migration sessions."
```

---

### Task 15: Implement preview-card layout split + live binding via `setProperty()`

**Files:**
- Modify: `cpp/src/ui/settings/generic_settings_page.cpp`
- Modify: `cpp/src/ui/settings/generic_settings_page.h`

- [ ] **Step 1: Add the preview-mounting helper to the header**

```cpp
// In generic_settings_page.h, near other private methods:
QWidget* mountPreviewWidget(const QString& previewType, QWidget* parent);
void wirePreviewBinding(const PreviewSpec& spec, QWidget* preview);
```

- [ ] **Step 2: Implement preview construction**

In `generic_settings_page.cpp`:

```cpp
#include "widgets/preview/aspect_ratio_preview.h"
#include "widgets/preview/osd_preview.h"
#include "widgets/settings_card.h"

QWidget* GenericSettingsPage::mountPreviewWidget(const QString& previewType,
                                                  QWidget* parent) {
    if (previewType == "aspect") return new AspectRatioPreview(parent);
    if (previewType == "osd")    return new OsdPreview(parent);
    return nullptr;
}
```

- [ ] **Step 3: Modify `buildSubcategory()` to use split layout when previewSpec is non-empty**

Update `buildSubcategory()`. After resolving `layout`:

```cpp
    const PreviewSpec spec = m_adapter
        ? m_adapter->previewSpec(m_category, subcategory)
        : PreviewSpec{};

    QVBoxLayout* settingsLayout = layout;       // default: cards stack in the main column
    QWidget* preview = nullptr;

    if (!spec.previewType.isEmpty()) {
        // Preview layout: top row is split — cards left, preview card right.
        // Use a SettingsCard with previewStyle for the preview container so
        // the visual treatment matches existing PCSX2 pages.
        auto* topRow = new QHBoxLayout();
        topRow->setSpacing(14);

        auto* leftHost = new QWidget();
        settingsLayout = new QVBoxLayout(leftHost);
        settingsLayout->setContentsMargins(0, 0, 0, 0);
        settingsLayout->setSpacing(10);
        topRow->addWidget(leftHost, /*stretch=*/1);

        auto* card = new SettingsCard(this);
        card->setPreviewStyle(true);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        preview = mountPreviewWidget(spec.previewType, card);
        if (preview) v->addWidget(preview);
        topRow->addWidget(card, /*stretch=*/1);

        layout->addLayout(topRow);
    }

    // ... existing group/header iteration writes into settingsLayout instead of layout ...
```

Replace every subsequent `layout->addWidget(...)` inside the group iteration with `settingsLayout->addWidget(...)`. Keep the final `layout->addStretch()` (it still belongs on the outer layout).

After the iteration, wire the live binding:

```cpp
    if (preview && !spec.keyToProperty.isEmpty())
        wirePreviewBinding(spec, preview);
```

- [ ] **Step 4: Implement `wirePreviewBinding()`**

```cpp
void GenericSettingsPage::wirePreviewBinding(const PreviewSpec& spec,
                                              QWidget* preview) {
    // Initial sync: read each bound setting's current value and set the
    // preview property right now.
    auto* app = m_dlg->appController();
    const QString emuId = m_dlg->emuId();
    for (auto it = spec.keyToProperty.constBegin();
         it != spec.keyToProperty.constEnd(); ++it) {
        const QString& key = it.key();
        const QString& propName = it.value();
        for (const auto& d : m_schema) {
            if (d.key != key) continue;
            const QString cur = app->settingValue(emuId, d.section, d.key);
            const QString val = cur.isEmpty() ? d.defaultValue : cur;
            // Property type inference: int for numeric strings, bool for
            // True/False, string otherwise. The preview widget's WRITE
            // accessor handles whatever Qt's metatype conversion produces.
            bool ok = false;
            const int asInt = val.toInt(&ok);
            if (ok && d.type != SettingDef::Combo) {
                preview->setProperty(propName.toUtf8().constData(), asInt);
            } else if (val.compare("true", Qt::CaseInsensitive) == 0
                    || val.compare("True", Qt::CaseInsensitive) == 0) {
                preview->setProperty(propName.toUtf8().constData(), true);
            } else if (val.compare("false", Qt::CaseInsensitive) == 0
                    || val.compare("False", Qt::CaseInsensitive) == 0) {
                preview->setProperty(propName.toUtf8().constData(), false);
            } else {
                preview->setProperty(propName.toUtf8().constData(), val);
            }
        }
    }

    // Live updates: connect change signals from each bound widget to a
    // setProperty() call on the preview.
    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const QString key = combo->settingDef().key;
        if (!spec.keyToProperty.contains(key)) continue;
        const QString prop = spec.keyToProperty.value(key);
        connect(combo, &SettingsComboRow::valueChanged, preview,
                [preview, prop](const QString& v) {
                    preview->setProperty(prop.toUtf8().constData(), v);
                });
    }
    for (auto* tog : findChildren<SettingsToggleRow*>()) {
        const QString key = tog->settingDef().key;
        if (!spec.keyToProperty.contains(key)) continue;
        const QString prop = spec.keyToProperty.value(key);
        connect(tog, &SettingsToggleRow::toggled, preview,
                [preview, prop](bool on) {
                    preview->setProperty(prop.toUtf8().constData(), on);
                });
    }
    for (auto* slider : findChildren<SettingsSliderRow*>()) {
        const QString key = slider->settingDef().key;
        if (!spec.keyToProperty.contains(key)) continue;
        const QString prop = spec.keyToProperty.value(key);
        connect(slider, &SettingsSliderRow::valueChanged, preview,
                [preview, prop](int v) {
                    preview->setProperty(prop.toUtf8().constData(), v);
                });
    }
}
```

- [ ] **Step 5: Build**

Run: `cd cpp && cmake --build build`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/ui/settings/generic_settings_page.{h,cpp}
git commit -m "settings: GenericSettingsPage preview-card split + live binding

When PreviewSpec.previewType is non-empty, top row splits horizontally:
settings cards on the left, preview card on the right. Live binding
walks the keyToProperty map and connects each setting widget's change
signal to the preview's QObject::setProperty(), so the preview tracks
schema state without depending on the concrete preview widget class."
```

---

## Phase 6 — Dolphin dialog wiring

### Task 16: Build `DolphinCategoryHub`

**Files:**
- Create: `cpp/src/ui/settings/dolphin/dolphin_category_hub.h`
- Create: `cpp/src/ui/settings/dolphin/dolphin_category_hub.cpp`

- [ ] **Step 1: Create the header**

```cpp
// cpp/src/ui/settings/dolphin/dolphin_category_hub.h
#pragma once
#include "ui/settings/emulator_category_hub_base.h"

class DolphinCategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    explicit DolphinCategoryHub(QWidget* parent = nullptr);

private:
    int countSettings(const QString& category) const;
};
```

- [ ] **Step 2: Create the .cpp**

```cpp
// cpp/src/ui/settings/dolphin/dolphin_category_hub.cpp
#include "dolphin_category_hub.h"
#include "ui/settings/widgets/settings_card.h"
#include "adapters/dolphin_adapter.h"
#include <QGridLayout>

DolphinCategoryHub::DolphinCategoryHub(QWidget* parent)
    : EmulatorCategoryHubBase(parent) {
    setupChrome("Dolphin Settings");

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    grid->addWidget(makeCard(QStringLiteral("\U0001F39B"),  "Interface",
                             "Pause, cursor, focus",
                             countSettings("Interface"), "Interface"),  0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Core",
                             "CPU, IPL, overclock",
                             countSettings("Core"), "Core"),            0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",
                             "Resolution, AA, VSync",
                             countSettings("Graphics"), "Graphics"),    1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, volume, JIT",
                             countSettings("Audio"), "Audio"),          1, 1);

    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(0);
}

int DolphinCategoryHub::countSettings(const QString& category) const {
    DolphinAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

Add to source list (around line 91):

```cmake
    src/ui/settings/dolphin/dolphin_category_hub.cpp
```

Add to header list (around line 212):

```cmake
    src/ui/settings/dolphin/dolphin_category_hub.h
```

- [ ] **Step 4: Build + commit**

```bash
cd cpp && cmake --build build
git add cpp/src/ui/settings/dolphin/dolphin_category_hub.{h,cpp} cpp/CMakeLists.txt
git commit -m "settings: add DolphinCategoryHub (4 cards: Interface, Core, Graphics, Audio)"
```

---

### Task 17: Build `DolphinSettingsDialog`

**Files:**
- Create: `cpp/src/ui/settings/dolphin/dolphin_settings_dialog.h`
- Create: `cpp/src/ui/settings/dolphin/dolphin_settings_dialog.cpp`

- [ ] **Step 1: Create the header**

```cpp
// cpp/src/ui/settings/dolphin/dolphin_settings_dialog.h
#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

class DolphinCategoryHub;

class DolphinSettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    DolphinSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

private:
    void onCategoryActivated(const QString& category);
};
```

- [ ] **Step 2: Create the .cpp**

```cpp
// cpp/src/ui/settings/dolphin/dolphin_settings_dialog.cpp
#include "dolphin_settings_dialog.h"
#include "dolphin_category_hub.h"
#include "ui/settings/generic_settings_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "adapters/dolphin_adapter.h"

DolphinSettingsDialog::DolphinSettingsDialog(AppController* app,
                                              const QString& emuId,
                                              QWidget* parent)
    : EmulatorSettingsDialogBase(app, emuId, parent) {
    setupChrome("Dolphin Settings", QSize(1000, 720), SettingsDialogTheme::windowBg());

    auto* hub = new DolphinCategoryHub(this);
    connect(hub, &DolphinCategoryHub::categoryActivated,
            this, &DolphinSettingsDialog::onCategoryActivated);
    connect(hub, &DolphinCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    setHub(hub);
}

void DolphinSettingsDialog::onCategoryActivated(const QString& category) {
    static DolphinAdapter adapter;  // stateless, reused across calls

    QVector<SettingDef> slice;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) slice.append(d);

    auto* page = new GenericSettingsPage(this, std::move(slice), &adapter);
    connect(page, &GenericSettingsPage::settingFocused,
            this, &DolphinSettingsDialog::setFocusedSetting);

    // Graphics has sub-tabs (Display + Rendering) — pass hasSubTabs=true so
    // L1/R1 hint shows on the dialog chrome (mirrors duckstation_settings_dialog.cpp:49).
    const bool hasSubTabs = (category == "Graphics");
    pushPage(page, hasSubTabs);
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

Add to source + header lists alongside the hub.

- [ ] **Step 4: Build**

Run: `cd cpp && cmake --build build`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add cpp/src/ui/settings/dolphin/dolphin_settings_dialog.{h,cpp} cpp/CMakeLists.txt
git commit -m "settings: add DolphinSettingsDialog using GenericSettingsPage

50-line dialog — categoryActivated callback filters the schema to one
category and instantiates GenericSettingsPage. Graphics opts into
sub-tab mode for L1/R1 hint display."
```

---

### Task 18: Wire `app_controller.cpp` to use `DolphinSettingsDialog`

**Files:**
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Replace the Dolphin native-fallback branch**

In `cpp/src/ui/app_controller.cpp:387-394`, replace:

```cpp
    if (emuId == QLatin1String("dolphin")) {
        // Dolphin v1: no dedicated in-app settings dialog yet (deferred from
        // the initial adapter scope). The schema in DolphinAdapter is read
        // via the Quick Settings overlay (resolution, aspect ratio) and the
        // remaining knobs are accessible through Dolphin's native UI.
        openNativeEmulatorSettings(emuId);
        return;
    }
```

with:

```cpp
    if (emuId == QLatin1String("dolphin")) {
        auto* dialog = new DolphinSettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
```

Add the include at the top of the file:

```cpp
#include "ui/settings/dolphin/dolphin_settings_dialog.h"
```

- [ ] **Step 2: Build**

Run: `cd cpp && cmake --build build`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/ui/app_controller.cpp
git commit -m "app: route Dolphin Settings to in-app DolphinSettingsDialog

Replaces the openNativeEmulatorSettings() fallback at app_controller.cpp:387-394.
Dolphin now uses the schema-driven GenericSettingsPage like the planned
flow for all emulators."
```

---

## Phase 7 — End-to-end verification

### Task 19: Full build + tests + manual smoke

**Files:** none

- [ ] **Step 1: Clean build**

Run: `cd cpp && cmake --build build`
Expected: clean build, no warnings.

- [ ] **Step 2: Full test suite**

Run: `cd cpp && ctest --test-dir build --output-on-failure`
Expected: all tests pass — including `DolphinSchema`, `Pcsx2PreviewSpec`, `AspectRatioPreview`, `OsdPreview`.

- [ ] **Step 3: Launch the app**

Run: `open ./cpp/build/RetroNest.app`

- [ ] **Step 4: Manual smoke checklist — Dolphin**

For each item, mark pass/fail:

- [ ] Open Dolphin Settings → all 4 cards (Interface, Core, Graphics, Audio) show the right setting count badges.
- [ ] Click Interface → see PauseOnFocusLost, ConfirmStop, HideCursor toggles.
- [ ] Toggle PauseOnFocusLost off → close dialog → reopen → toggle reflects saved state.
- [ ] Click Audio → Backend combo + Volume slider + EnableJIT toggle render.
- [ ] Drag the Volume slider → verify value updates → close → reopen → confirm persisted.
- [ ] Click Core → CPU Core combo, SkipIPL toggle, EnableCheats toggle, OverclockEnable toggle, Overclock slider.
- [ ] Toggle OverclockEnable off → Overclock slider greys out + becomes disabled.
- [ ] Toggle OverclockEnable on → slider re-enables.
- [ ] Click Graphics → sub-tab bar appears with Display + Rendering tabs.
- [ ] Display sub-tab shows the aspect-ratio preview on the right.
- [ ] Change AspectRatio combo → preview rectangle updates live.
- [ ] Toggle IntegerScaling → preview reflects the change.
- [ ] Switch to Rendering sub-tab → no preview, just MSAA/MaxAnisotropy/ShaderCompilation/WaitForShaders/PixelLighting.
- [ ] Description bar at the bottom updates as you focus different settings.
- [ ] Quit app → relaunch → reopen Dolphin Settings → all changes persisted.
- [ ] After tweaking Graphics/Display Aspect Ratio: open `~/.../emulators/dolphin/User/Config/GFX.ini` in a text editor → verify `[Settings] AspectRatio` reflects the new value (per-key file routing working).
- [ ] After tweaking Graphics/Display Fullscreen: open `Dolphin.ini` (NOT GFX.ini) → verify `[Display] Fullscreen` reflects the new value.

- [ ] **Step 5: Manual smoke checklist — PCSX2 (regression)**

- [ ] Open PCSX2 Settings → all existing categories render normally.
- [ ] Graphics → Display sub-tab → aspect preview renders + responds to combo + slider + toggle changes (unchanged from before).
- [ ] Graphics → OSD sub-tab → OSD preview renders + responds to all show* toggles + position combos (unchanged).
- [ ] Other PCSX2 pages (Emulation, Audio, Memory Cards) all open and save correctly.

- [ ] **Step 6: Final commit (if any plan-level fixups discovered)**

If the manual smoke surfaces issues, fix them in dedicated commits. Otherwise no commit needed.

- [ ] **Step 7: Done**

The schema-driven settings dialog is live for Dolphin with previews. PCSX2 is unchanged. PCSX2's `previewSpec()` is declared and ready for its own migration session.

---

## Out of scope (follow-up sessions)

- **PCSX2 migration to GenericSettingsPage** — its `previewSpec()` already returns the full binding map; migration session deletes `cpp/src/ui/settings/pcsx2/pages/*.cpp` and replaces the dialog's category routing with `GenericSettingsPage`.
- **DuckStation migration** — same shape as PCSX2. Adds a `saveTransform` for the overclock GCD math.
- **PPSSPP migration** — same shape. Bitmask handling for `iShowStatusFlags` already supported by the schema.
- **Dolphin Memory Cards / BIOS / Wii Remote configuration** — handled via the "Open Native Settings" button on the hub.
- **Per-game Dolphin settings overrides** — Dolphin's `User/GameSettings/{GAMEID}.ini` mechanism, separate spec.
