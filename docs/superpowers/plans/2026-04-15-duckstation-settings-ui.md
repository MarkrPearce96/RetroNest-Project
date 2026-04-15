# DuckStation Settings UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the generic `EmulatorSettingsPage` used for DuckStation with a bespoke Qt Widgets settings UI that mirrors the existing PCSX2 settings UI (`cpp/src/ui/settings/pcsx2/`) in layout, structure, and design, using DuckStation's schema as the content source.

**Architecture:** New parallel tree at `cpp/src/ui/settings/duckstation/` containing a dialog, a 5-card category hub (Console / Emulation / Graphics / Audio / Memory Cards stretched), category pages, a 3-tab Graphics container (Rendering / Advanced / OSD), and two DuckStation-specific preview widgets (aspect ratio, OSD). Generic PCSX2 widgets (`pcsx2_toggle`, `pcsx2_combo_row`, `pcsx2_slider_row`, `pcsx2_section_header`, `pcsx2_toggle_row`, `pcsx2_card`, `pcsx2_description_bar`, `pcsx2_graphics_sub_tab_bar`) are reused in place by including their headers directly. No refactor of PCSX2 code. `AppController::showEmulatorSettings` is extended to branch on `emuId == "duckstation"`.

**Tech Stack:** C++17, Qt6 Widgets, CMake 3.16+, Qt Test for unit tests.

**Design spec:** `docs/superpowers/specs/2026-04-15-duckstation-settings-ui-design.md`.

**Reference implementation to mirror:** `cpp/src/ui/settings/pcsx2/`. Every DuckStation page is a near-copy of the corresponding PCSX2 page with (a) the schema filter swapped to DuckStation's category/subcategory names and (b) the setting keys replaced. When in doubt, read the PCSX2 page and copy the structure verbatim.

---

## File layout

All new files live under `cpp/src/ui/settings/duckstation/`:

```
duckstation_theme.h                                 (Task 1)
duckstation_settings_dialog.{h,cpp}                 (Task 2)
duckstation_category_hub.{h,cpp}                    (Task 3)
pages/duckstation_console_page.{h,cpp}              (Task 6)
pages/duckstation_emulation_page.{h,cpp}            (Task 7)
pages/duckstation_graphics_page.{h,cpp}             (Task 8)
pages/duckstation_graphics_rendering_page.{h,cpp}   (Task 10)
pages/duckstation_graphics_advanced_page.{h,cpp}    (Task 11)
pages/duckstation_graphics_osd_page.{h,cpp}         (Task 13)
pages/duckstation_audio_page.{h,cpp}                (Task 14)
pages/duckstation_memory_cards_page.{h,cpp}         (Task 15)
widgets/duckstation_aspect_ratio_preview.{h,cpp}    (Task 9)
widgets/duckstation_osd_preview.{h,cpp}             (Task 12)
```

Modified files:
- `cpp/src/ui/app_controller.cpp:862` — add DuckStation branch to `showEmulatorSettings()` (Task 4)
- `cpp/CMakeLists.txt:80-188` — register every new `.cpp`/`.h` alongside the PCSX2 entries (each task adds its own)

---

## Task 1: DuckStation theme header

**Files:**
- Create: `cpp/src/ui/settings/duckstation/duckstation_theme.h`
- Modify: `cpp/CMakeLists.txt` (add header alongside the PCSX2 entries)

Start by mirroring the PCSX2 palette 1:1. Divergence happens later per-page if desired; for now identical values produce identical visuals.

- [ ] **Step 1: Create the theme header**

```cpp
#pragma once
#include <QColor>
#include <QString>

// DuckStation Settings dialog palette. Initially identical to Pcsx2Theme per
// spec 2026-04-15 — divergence is deferred until per-page visual tweaks.
namespace DuckStationTheme {

inline QColor windowBg()       { return QColor("#585450"); }
inline QColor titleBarBg()     { return QColor("#4a4642"); }
inline QColor cardBg()         { return QColor("#646058"); }
inline QColor cardBorder()     { return QColor("#706c66"); }
inline QColor cardBorderFocus(){ return QColor("#f59e0b"); }
inline QColor inputBg()        { return QColor("#585450"); }
inline QColor textPrimary()    { return QColor("#f2efe8"); }
inline QColor textSecondary()  { return QColor("#d0ccc4"); }
inline QColor textMuted()      { return QColor("#9a9690"); }
inline QColor accent()         { return QColor("#f59e0b"); }
inline QColor letterbox()      { return QColor("#3a3632"); }
inline QColor previewCardBg()  { return QColor("#504c48"); }

} // namespace DuckStationTheme
```

- [ ] **Step 2: Add header to CMakeLists.txt**

Open `cpp/CMakeLists.txt` and find the block listing `src/ui/settings/pcsx2/pcsx2_theme.h` (around line 163). Add a new block immediately after the PCSX2 settings headers block:

```cmake
    # DuckStation settings
    src/ui/settings/duckstation/duckstation_theme.h
```

- [ ] **Step 3: Verify build still succeeds**

Run:
```sh
cd cpp && cmake --build build
```
Expected: build succeeds (the header isn't included anywhere yet, but CMake registers it for AUTOMOC).

- [ ] **Step 4: Commit**

```sh
git add cpp/src/ui/settings/duckstation/duckstation_theme.h cpp/CMakeLists.txt
git commit -m "feat(duckstation-settings): add DuckStation theme header"
```

---

## Task 2: Settings dialog (scaffold with empty stub hub)

**Files:**
- Create: `cpp/src/ui/settings/duckstation/duckstation_settings_dialog.h`
- Create: `cpp/src/ui/settings/duckstation/duckstation_settings_dialog.cpp`
- Reference: `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.{h,cpp}` — copy structure verbatim, replace `Pcsx2*` → `DuckStation*` and `PCSX2 Settings` → `DuckStation Settings`.

The dialog owns a `QStackedWidget` history stack, a shared `Pcsx2DescriptionBar` (reused), and a category hub. This task creates it with the hub stubbed to an empty widget — the real hub lands in Task 3.

- [ ] **Step 1: Create the header**

```cpp
#pragma once
#include <QDialog>
#include <QStack>
#include "core/setting_def.h"

class QStackedWidget;
class AppController;
class DuckStationCategoryHub;
class Pcsx2DescriptionBar;

class DuckStationSettingsDialog : public QDialog {
    Q_OBJECT
public:
    DuckStationSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

    AppController* appController() const { return m_app; }
    const QString& emuId() const { return m_emuId; }

    void pushPage(QWidget* page, bool hasSubTabs = false);
    void popPage();
    void setFocusedSetting(const SettingDef& def);
    void clearFocusedSetting();

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    void onCategoryActivated(const QString& category);
    void applyHintsForCurrentPage();

    AppController* m_app;
    QString m_emuId;
    QStackedWidget* m_stack = nullptr;
    DuckStationCategoryHub* m_hub = nullptr;
    Pcsx2DescriptionBar* m_descBar = nullptr;
    QStack<int> m_history;
    bool m_currentPageHasSubTabs = false;
};
```

- [ ] **Step 2: Create the cpp file**

Copy `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp` as the starting point. Apply these exact substitutions:
- `Pcsx2SettingsDialog` → `DuckStationSettingsDialog`
- `Pcsx2CategoryHub` → `DuckStationCategoryHub`
- `"PCSX2 Settings"` → `"DuckStation Settings"`
- `Pcsx2Theme::windowBg()` → `DuckStationTheme::windowBg()`
- Include `"duckstation_theme.h"` instead of `"pcsx2_theme.h"`
- Keep `Pcsx2DescriptionBar` (shared widget — do NOT rename).
- In `onCategoryActivated()`, stub every branch to a placeholder that does nothing yet — we'll wire real pages in Tasks 6–15. Replace the whole body of `onCategoryActivated` with just `(void)category;` for now.

Include the header with `#include "widgets/pcsx2_description_bar.h"` (note the relative path — it lives in the pcsx2 widgets folder and is reused).

- [ ] **Step 3: Add to CMakeLists.txt**

Append to the DuckStation settings block in `cpp/CMakeLists.txt`:

```cmake
    src/ui/settings/duckstation/duckstation_settings_dialog.cpp
```

And in the headers list:
```cmake
    src/ui/settings/duckstation/duckstation_settings_dialog.h
```

- [ ] **Step 4: Verify build**

```sh
cd cpp && cmake --build build
```
Expected: fails because `DuckStationCategoryHub` is not defined. That's the next task.

- [ ] **Step 5: Commit (with build still broken is OK — Task 3 fixes it)**

Do NOT commit yet; pair with Task 3's commit once the scaffold builds.

---

## Task 3: Category hub (5 cards, stretched Memory Cards)

**Files:**
- Create: `cpp/src/ui/settings/duckstation/duckstation_category_hub.h`
- Create: `cpp/src/ui/settings/duckstation/duckstation_category_hub.cpp`
- Reference: `cpp/src/ui/settings/pcsx2/pcsx2_category_hub.{h,cpp}`

The hub lays out 5 cards in a 2×2 + 1-stretched-row grid. Counts are pulled from the adapter's schema at construction time (not hardcoded).

- [ ] **Step 1: Create the header**

Mirror `pcsx2_category_hub.h` with names substituted. Signals: `categoryActivated(const QString&)`, `openNativeRequested()`. Event filter handles Up/Down between the stretched Memory Cards card and the native-settings button.

```cpp
#pragma once
#include <QWidget>
class QPushButton;
class Pcsx2Card;

class DuckStationCategoryHub : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationCategoryHub(QWidget* parent = nullptr);

signals:
    void categoryActivated(const QString& category);
    void openNativeRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    Pcsx2Card* makeCard(const QString& icon, const QString& title,
                        const QString& descriptor, int settingCount,
                        const QString& categoryKey);
    int countSettings(const QString& category) const;

    QPushButton* m_nativeBtn = nullptr;
    Pcsx2Card* m_stretchCard = nullptr;
};
```

- [ ] **Step 2: Create the cpp — hub layout**

Copy `pcsx2_category_hub.cpp` structure. Key differences:

1. Include the DuckStation adapter and count settings per category:
```cpp
#include "adapters/duckstation_adapter.h"

int DuckStationCategoryHub::countSettings(const QString& category) const {
    DuckStationAdapter adapter;
    int n = 0;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == category) ++n;
    return n;
}
```

2. Hub title: `"DuckStation Settings"`.

3. Grid population — 2×2 + stretched bottom:
```cpp
grid->addWidget(makeCard(QStringLiteral("\U0001F39B"),  "Console",
                         "Region, BIOS, fast boot",
                         countSettings("Console"), "Console"),   0, 0);
grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                         "Speed control, CPU, timing",
                         countSettings("Emulation"), "Emulation"), 0, 1);
grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"),  "Graphics",
                         "Renderer, rendering, advanced, OSD",
                         countSettings("Graphics") + countSettings("On-Screen Display"),
                         "Graphics"), 1, 0);
grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                         "Backend, latency, volume",
                         countSettings("Audio"), "Audio"),        1, 1);

m_stretchCard = makeCard(QStringLiteral("\U0001F4BE"), "Memory Cards",
                         "Slots and card types",
                         countSettings("Memory Cards"), "Memory Cards");
grid->addWidget(m_stretchCard, 2, 0, 1, 2);  // row 2, span 2 columns
m_stretchCard->installEventFilter(this);
```

- [ ] **Step 3: Event filter for Up/Down between stretched card and native button**

Copy the `eventFilter()` body from `pcsx2_category_hub.cpp` but change the bottom-row detection: only `m_stretchCard` (not "last card") is the one that steps down to the button.

Replace the `if (auto* card = qobject_cast<Pcsx2Card*>(obj))` block with:
```cpp
if (obj == m_stretchCard) {
    if (ke->key() == Qt::Key_Down) {
        m_nativeBtn->setFocus(Qt::TabFocusReason);
        return true;
    }
}
```

And replace the "up from native button → last card" block with:
```cpp
if (obj == m_nativeBtn) {
    if (ke->key() == Qt::Key_Up) {
        if (m_stretchCard) { m_stretchCard->setFocus(Qt::TabFocusReason); return true; }
    }
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
        emit openNativeRequested();
        return true;
    }
}
```

- [ ] **Step 4: Add to CMakeLists.txt**

Append `src/ui/settings/duckstation/duckstation_category_hub.cpp` to sources and `.h` to headers.

- [ ] **Step 5: Wire the hub into the dialog (finish Task 2)**

Reopen `duckstation_settings_dialog.cpp`. Replace the stub hub with `m_hub = new DuckStationCategoryHub(this);` and `#include "duckstation_category_hub.h"` at the top.

- [ ] **Step 6: Build**

```sh
cd cpp && cmake --build build
```
Expected: builds cleanly.

- [ ] **Step 7: Commit (Tasks 2 + 3 together)**

```sh
git add cpp/src/ui/settings/duckstation cpp/CMakeLists.txt
git commit -m "feat(duckstation-settings): scaffold dialog + 5-card category hub"
```

---

## Task 4: Dispatcher wiring

**Files:**
- Modify: `cpp/src/ui/app_controller.cpp:861-871`

- [ ] **Step 1: Add DuckStation branch**

Replace the body of `AppController::showEmulatorSettings` with:

```cpp
void AppController::showEmulatorSettings(const QString& emuId) {
    if (emuId == QLatin1String("pcsx2")) {
        auto* dialog = new Pcsx2SettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
    if (emuId == QLatin1String("duckstation")) {
        auto* dialog = new DuckStationSettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
    auto* dialog = new EmulatorSettingsPage(this, emuId);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
```

- [ ] **Step 2: Add include**

Near the top of `app_controller.cpp`, next to `#include "ui/settings/pcsx2/pcsx2_settings_dialog.h"`, add:
```cpp
#include "ui/settings/duckstation/duckstation_settings_dialog.h"
```

- [ ] **Step 3: Build and launch**

```sh
cd cpp && cmake --build build
open ./build/RetroNest.app
```

Manual check: open a DuckStation game's settings. You should see the new DuckStation hub with 5 cards (Memory Cards stretched full-width) instead of the old generic sidebar dialog. Clicking a card currently does nothing (pages stubbed out).

- [ ] **Step 4: Commit**

```sh
git add cpp/src/ui/app_controller.cpp
git commit -m "feat(duckstation-settings): route emuId=duckstation to new dialog"
```

---

## Task 5: Page recipe (read-only — no code to write)

**Every page in Tasks 6–15 follows the same recipe.** Skim this once, then treat each page task as "apply the recipe, swap the schema filter and keys".

**Pattern (as used by `pcsx2_emulation_page.cpp`, `pcsx2_audio_page.cpp`, etc.):**

1. Constructor filters adapter schema into `m_schema` by `category == "X"` (and/or `subcategory == "Y"`).
2. `buildUi()` creates outer `QScrollArea` wrapping a content widget with a `QVBoxLayout`.
3. First child: a `← Back` `QPushButton` wired to `dialog->popPage()`. Style identical to PCSX2 pages:
   ```cpp
   back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                       " font-size:14px; padding:4px 0; text-align:left; }"
                       "QPushButton:focus { color:#f59e0b; }");
   ```
4. For each logical group in the page: a `Pcsx2SectionHeader` label, then one or more `Pcsx2Card`s. Each card wraps one `Pcsx2ComboRow` / `Pcsx2ToggleRow` / `Pcsx2SliderRow` (or a compound layout for the Graphics Rendering page).
5. Emit `settingFocused(const SettingDef&)` when a card or row gains focus; wire it to the dialog's `setFocusedSetting`.
6. `loadValues()` walks `findChildren<Pcsx2ComboRow*>()` / `Pcsx2ToggleRow*` / `Pcsx2SliderRow*` and pulls from `app->settingValue(emuId, section, key)`.
7. `saveValue(section, key, value)` posts to `m_dialog->appController()->saveSettings(emuId, {{section+"/"+key, value}})`.
8. Widget includes used in every page:
   ```cpp
   #include "ui/settings/pcsx2/widgets/pcsx2_card.h"
   #include "ui/settings/pcsx2/widgets/pcsx2_section_header.h"
   #include "ui/settings/pcsx2/widgets/pcsx2_combo_row.h"
   #include "ui/settings/pcsx2/widgets/pcsx2_toggle_row.h"
   #include "ui/settings/pcsx2/widgets/pcsx2_slider_row.h"
   ```

**Whenever a page task says "use the recipe", it means copy the relevant PCSX2 page file (listed in the task), apply these substitutions, and fill in the page-specific section layout shown in the task:**

| PCSX2 file | Purpose |
|---|---|
| `pcsx2_emulation_page.cpp` | Combo cards stacked + toggle-grid pattern |
| `pcsx2_audio_page.cpp` | Combo/slider cards + (slider + toggle) horizontal rows |
| `pcsx2_memory_cards_page.cpp` | Slot-card pattern (toggle + filename label inside one card) |

Also reusable from `pcsx2_emulation_page.cpp`: the local helpers `makeComboCard` and `makeToggleCard` lambdas — copy verbatim into every page that needs combos and toggles.

Each page task below lists the exact section headers and DuckStation schema keys. Group the schema keys in the order given; the grouping by `group` field in the schema is a hint, not a command — follow the task's layout.

**Float settings:** DuckStation has a few `SettingDef::Float` keys (e.g. `RewindFrequency`). Render these with `Pcsx2SliderRow` the same way `Int` is handled, with `setRange(int(minVal*10), int(maxVal*10))` and displayed text computed by the slider's suffix. If `Pcsx2SliderRow` doesn't already support floats, treat it as a write-once integer rounded to the step — the widget's existing behaviour. No new widget needed.

---

## Task 6: Console page

**Files:**
- Create: `cpp/src/ui/settings/duckstation/pages/duckstation_console_page.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/pages/pcsx2_emulation_page.cpp`

Schema: `category == "Console"` (16 keys total). Section headers + card layout:

- [ ] **Step 1: Header file**

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class DuckStationSettingsDialog;

class DuckStationConsolePage : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationConsolePage(DuckStationSettingsDialog* dialog);

signals:
    void settingFocused(const SettingDef& def);

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    DuckStationSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
```

- [ ] **Step 2: CPP — copy pcsx2_emulation_page.cpp structure, substitute schema and layout**

Filter: `if (d.category == "Console") m_schema.append(d);`

Layout, in order:

```
Section: "Region"           (combo cards, stacked full-width)
    Region
    ForceVideoTiming

Section: "BIOS"             (toggle grid 2-col)
    PatchFastBoot
    FastForwardBoot
    FastForwardAccess
    Enable8MBRAM

Section: "CPU Emulation"    (combos stacked, then toggles in 2-col grid)
    ExecutionMode              (combo)
    OverclockEnable            (toggle, full width)
    OverclockNumerator         (combo, stacked)
    RecompilerICache           (toggle, full width)

Section: "CD-ROM Emulation" (combos + 2-col toggle grid)
    ReadSpeedup                (combo)
    SeekSpeedup                (combo)
    (toggle grid)
    LoadImageToRAM
    AutoDiscChange
    LoadImagePatches
    IgnoreHostSubcode
```

- [ ] **Step 3: Wire page into dialog**

In `duckstation_settings_dialog.cpp::onCategoryActivated`, add:

```cpp
if (category == "Console") {
    auto* page = new DuckStationConsolePage(this);
    connect(page, &DuckStationConsolePage::settingFocused,
            this, &DuckStationSettingsDialog::setFocusedSetting);
    pushPage(page);
    return;
}
```

And at the top: `#include "pages/duckstation_console_page.h"`.

- [ ] **Step 4: Add to CMakeLists.txt** (both `.cpp` and `.h`).

- [ ] **Step 5: Build and smoke-test**

```sh
cd cpp && cmake --build build
open ./build/RetroNest.app
```

Open DuckStation settings → click Console → verify page renders, combos are populated, toggling a setting persists across dialog close+reopen.

- [ ] **Step 6: Commit**

```sh
git add cpp/src/ui/settings/duckstation/pages/duckstation_console_page.* cpp/CMakeLists.txt cpp/src/ui/settings/duckstation/duckstation_settings_dialog.cpp
git commit -m "feat(duckstation-settings): add Console page"
```

---

## Task 7: Emulation page

**Files:**
- Create: `cpp/src/ui/settings/duckstation/pages/duckstation_emulation_page.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/pages/pcsx2_emulation_page.cpp`

Schema: `category == "Emulation"` (13 keys).

- [ ] **Step 1: Create header + cpp following Task 6's pattern.**

Layout:

```
Section: "Speed Control"       (combo stack)
    EmulationSpeed
    FastForwardSpeed
    TurboSpeed

Section: "Latency Control"     (toggle grid 2-col)
    VSync
    SyncToHostRefreshRate
    OptimalFramePacing
    PreFrameSleep
    SkipPresentingDuplicateFrames

Section: "Rewind"              (toggle + dependent sliders)
    RewindEnable                (toggle, full width)
    UseSoftwareRendererForMemoryStates   (toggle; greyed when RewindEnable==false)
    RewindFrequency             (slider)
    RewindSaveSlots             (slider)

Section: "Runahead"
    RunaheadFrameCount          (combo)
    RunaheadForAnalogInput      (toggle; greyed when RunaheadFrameCount=="0")
```

For the `dependsOn` grey-out behaviour, copy the exact mechanism from `pcsx2_emulation_page.cpp` if it implements any; if not, wire it with a simple slot: when the parent toggle/combo changes, call `setEnabled()` on the child cards. The schema's `d.dependsOn` field tells you the parent key.

- [ ] **Step 2: Wire into dialog, CMake, build, smoke-test, commit** — same pattern as Task 6.

```sh
git commit -m "feat(duckstation-settings): add Emulation page"
```

---

## Task 8: Graphics page (sub-tab container)

**Files:**
- Create: `cpp/src/ui/settings/duckstation/pages/duckstation_graphics_page.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp`

The container owns the sub-tab bar and a `QStackedWidget` of three sub-pages (Rendering, Advanced, OSD). This task creates it with all three sub-pages stubbed as empty widgets — real sub-pages follow in Tasks 10/11/13.

- [ ] **Step 1: Copy `pcsx2_graphics_page.h` structure with renames.**

- [ ] **Step 2: Copy cpp structure, with these sub-tab entries:**

```cpp
m_tabBar->addTab(QStringLiteral("\U0001F3A8"), "Rendering");
m_tabBar->addTab(QStringLiteral("\u2699"),     "Advanced");
m_tabBar->addTab(QStringLiteral("\U0001F4CA"), "OSD");
```

For now, add three `new QWidget(this)` stubs to the stack. Replace them in Tasks 10/11/13.

Use these includes for reused widgets:
```cpp
#include "ui/settings/pcsx2/widgets/pcsx2_graphics_sub_tab_bar.h"
#include "ui/settings/pcsx2/widgets/pcsx2_card.h"
#include "ui/settings/pcsx2/widgets/pcsx2_toggle.h"
```

- [ ] **Step 3: Wire into dialog (`pushPage(page, /*hasSubTabs=*/true)`).**

- [ ] **Step 4: Add to CMake, build, smoke-test (clicking Graphics opens an empty 3-tab page), commit.**

```sh
git commit -m "feat(duckstation-settings): add Graphics sub-tab container"
```

---

## Task 9: DuckStation aspect-ratio preview widget

**Files:**
- Create: `cpp/src/ui/settings/duckstation/widgets/duckstation_aspect_ratio_preview.{h,cpp}`
- Create: `cpp/tests/test_duckstation_aspect_ratio_preview.cpp`
- Reference: `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.{h,cpp}` + its test file `cpp/tests/test_pcsx2_aspect_ratio_preview.cpp`.

DuckStation's AR values differ: `Auto (Game Native)`, `Stretch To Fill`, `4:3`, `16:9`, `19:9`, `20:9`, `21:9`, `16:10`, `PAR 1:1`. The widget reuses the same 16:9 canvas logic as PCSX2 but maps these strings to ratios.

- [ ] **Step 1: Write the failing test first**

```cpp
#include <QTest>
#include "ui/settings/duckstation/widgets/duckstation_aspect_ratio_preview.h"

class TestDuckStationAspectRatioPreview : public QObject {
    Q_OBJECT
private slots:
    void fromSchemaValue_mapsKnownStrings() {
        using AR = DuckStationAspectRatioPreview::AspectRatio;
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("4:3"),            AR::R4x3);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("16:9"),           AR::R16x9);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("21:9"),           AR::R21x9);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("Stretch To Fill"),AR::Stretch);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("Auto (Game Native)"),
                 AR::Auto);
    }
    void fromSchemaValue_unknownFallsBackToAuto() {
        using AR = DuckStationAspectRatioPreview::AspectRatio;
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("nonsense"), AR::Auto);
    }
    void aspectRatioValue_returnsNumericRatio() {
        DuckStationAspectRatioPreview w;
        w.setAspectRatio(DuckStationAspectRatioPreview::AspectRatio::R16x9);
        QCOMPARE(w.ratioFloat(), 16.0f / 9.0f);
    }
};
QTEST_MAIN(TestDuckStationAspectRatioPreview)
#include "test_duckstation_aspect_ratio_preview.moc"
```

- [ ] **Step 2: Register test executable in CMakeLists.txt**

Find the block that defines `test_pcsx2_aspect_ratio_preview` in `cpp/CMakeLists.txt` and clone it, substituting the DuckStation names.

- [ ] **Step 3: Run test — it fails to compile (class doesn't exist)**

```sh
cd cpp && cmake --build build --target test_duckstation_aspect_ratio_preview
```
Expected: compile error "DuckStationAspectRatioPreview not found".

- [ ] **Step 4: Implement the widget**

Copy `pcsx2_aspect_ratio_preview.h` and `.cpp`. Apply renames. Replace the AR enum + `fromSchemaValue` mapping with the DuckStation set:

```cpp
enum class AspectRatio {
    Auto,        // "Auto (Game Native)"
    Stretch,     // "Stretch To Fill"
    R4x3, R16x9, R19x9, R20x9, R21x9, R16x10,
    Par1x1,      // "PAR 1:1"
};

static AspectRatio fromSchemaValue(const QString& value) {
    if (value == "4:3")              return AspectRatio::R4x3;
    if (value == "16:9")             return AspectRatio::R16x9;
    if (value == "19:9")             return AspectRatio::R19x9;
    if (value == "20:9")             return AspectRatio::R20x9;
    if (value == "21:9")             return AspectRatio::R21x9;
    if (value == "16:10")            return AspectRatio::R16x10;
    if (value == "PAR 1:1")          return AspectRatio::Par1x1;
    if (value == "Stretch To Fill")  return AspectRatio::Stretch;
    return AspectRatio::Auto;
}

float ratioFloat() const {
    switch (m_ratio) {
        case AspectRatio::R4x3:    return 4.0f / 3.0f;
        case AspectRatio::R16x9:   return 16.0f / 9.0f;
        case AspectRatio::R19x9:   return 19.0f / 9.0f;
        case AspectRatio::R20x9:   return 20.0f / 9.0f;
        case AspectRatio::R21x9:   return 21.0f / 9.0f;
        case AspectRatio::R16x10:  return 16.0f / 10.0f;
        case AspectRatio::Par1x1:  return 4.0f / 3.0f;   // PS1 native PAR approximation
        case AspectRatio::Stretch: return 16.0f / 9.0f;
        case AspectRatio::Auto:    return 4.0f / 3.0f;
    }
    return 4.0f / 3.0f;
}
```

Image assets already exist: `cpp/qml/AppUI/images/ar/duckstation-4x3.webp` and `-16x9.webp`. Use them the same way `pcsx2_aspect_ratio_preview.cpp` loads its assets — just substitute `pcsx2-` for `duckstation-` in the asset paths.

- [ ] **Step 5: Run test — passes**

```sh
cd cpp && cmake --build build --target test_duckstation_aspect_ratio_preview && ./build/test_duckstation_aspect_ratio_preview
```
Expected: all tests pass.

- [ ] **Step 6: Add widget to RetroNest sources in CMakeLists.txt** (both `.cpp` and `.h`).

- [ ] **Step 7: Commit**

```sh
git commit -m "feat(duckstation-settings): add DuckStation aspect ratio preview widget"
```

---

## Task 10: Graphics → Rendering sub-page

**Files:**
- Create: `cpp/src/ui/settings/duckstation/pages/duckstation_graphics_rendering_page.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp` (the compound-layout page — copy this, NOT the PCSX2 Rendering page).

This is the page styled like PCSX2's Display tab: top row is a 2-column split (left compound card with stacked combos, right preview card with AR preview + crop spinboxes), then bottom toggle grid.

Schema: `category == "Graphics"` AND (`subcategory == ""` OR `subcategory == "Rendering"`). That's the top-level Renderer/Adapter PLUS the Rendering subcategory.

- [ ] **Step 1: Copy `pcsx2_graphics_display_page.cpp` and apply renames.**

Replace the `Pcsx2AspectRatioPreview` include/usage with `DuckStationAspectRatioPreview`:

```cpp
#include "ui/settings/duckstation/widgets/duckstation_aspect_ratio_preview.h"
```

- [ ] **Step 2: Replace schema filter**

```cpp
for (const auto& d : adapter.settingsSchema()) {
    if (d.category == "Graphics" &&
        (d.subcategory == "" || d.subcategory == "Rendering"))
        m_schema.append(d);
}
```

- [ ] **Step 3: Left compound card — stacked combos**

Inside `buildLeftCompoundCard()`, use these keys in order (each a `Pcsx2ComboRow` stacked):

```
Renderer              (from subcategory "")
Adapter               (from subcategory "")
ResolutionScale       (Rendering)
AspectRatio           (Rendering)
Scaling               (Rendering)
DeinterlacingMode     (Rendering)
```

Then one toggle row for `WidescreenHack`.

Save `m_aspectCombo` as the combo for `AspectRatio`, mirroring what `pcsx2_graphics_display_page.cpp` does with `m_aspectCombo` — its `valueChanged` signal updates the preview:

```cpp
connect(m_aspectCombo, &Pcsx2ComboRow::valueChanged, this, [this](const QString& val) {
    if (m_preview) m_preview->setAspectRatio(
        DuckStationAspectRatioPreview::fromSchemaValue(val));
});
```

- [ ] **Step 4: Right preview card**

Replace the preview widget instantiation with `new DuckStationAspectRatioPreview(card);`.

DuckStation doesn't have `StretchY`, so delete that slider. DuckStation's crop model is different — instead of `CropLeft/Top/Right/Bottom` sliders, drop the crop spinboxes entirely from the preview card for now. Replace them with a simple `Pcsx2ComboRow` for `CropMode` (from Rendering subcategory):

```cpp
// Right card contents (top to bottom):
//   "ASPECT RATIO PREVIEW" label
//   DuckStationAspectRatioPreview widget
//   CropMode combo
```

- [ ] **Step 5: Bottom toggle grid**

3 columns × 2 rows with these DuckStation Rendering bool keys:

```
PGXPEnable          PGXPDepthBuffer          Force4_3For24Bit
ChromaSmoothing24Bit  ForceRoundTextureCoordinates   (leave the third cell empty or drop the row to 2 cells)
```

- [ ] **Step 6: Add remaining Rendering keys below the grid**

Below the toggle grid, stack these as normal combo cards (full-width):
```
TextureFilter
SpriteTextureFilter
DitheringMode
DownsampleMode
Scaling24Bit    (labelled "FMV Scaling")
```

- [ ] **Step 7: Wire sub-page into `DuckStationGraphicsPage::m_stack` (replace the stub from Task 8).**

- [ ] **Step 8: Add to CMakeLists.txt, build, smoke-test, commit.**

```sh
git commit -m "feat(duckstation-settings): add Graphics Rendering sub-page"
```

---

## Task 11: Graphics → Advanced sub-page

**Files:**
- Create: `cpp/src/ui/settings/duckstation/pages/duckstation_graphics_advanced_page.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_rendering_page.cpp` (a simpler stacked-cards page).

Schema: `category == "Graphics" && subcategory == "Advanced"`.

Layout:

```
Section: "Display Options"
    Alignment                   (combo)
    Rotation                    (combo)
    FineCropMode                (combo)
    FineCropLeft/Top/Right/Bottom  (4 sliders — greyed when FineCropMode == "None")
    DisableMailboxPresentation  (toggle)

Section: "Rendering Options"
    Multisamples                (combo)
    LineDetectMode              (combo)
    UseThread                   (toggle, full-width)
    MaxQueuedFrames             (slider — greyed when UseThread == false)
    EnableModulationCrop        (toggle)
    ScaledInterlacing           (toggle)
    UseSoftwareRendererForReadbacks (toggle)
```

- [ ] **Steps 1–5: Apply the Task 5 recipe, wire into graphics sub-tab stack, CMake, build, smoke-test, commit.**

```sh
git commit -m "feat(duckstation-settings): add Graphics Advanced sub-page"
```

---

## Task 12: DuckStation OSD preview widget

**Files:**
- Create: `cpp/src/ui/settings/duckstation/widgets/duckstation_osd_preview.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.{h,cpp}`

The preview is a background rectangle with positioned "pill" labels whose visibility is driven by DuckStation's OSD flags: `ShowFPS`, `ShowSpeed`, `ShowResolution`, `ShowCPU`, `ShowGPU`, `ShowStatusIndicators`, `ShowInputs`, `ShowOSDMessages` (check the adapter for the exact keys — see `duckstation_adapter.cpp` lines 335+ for the OSD group).

- [ ] **Step 1: Read the adapter's OSD schema** (the "On-Screen Display" category in `duckstation_adapter.cpp`) and list every bool key it defines.

- [ ] **Step 2: Copy `pcsx2_osd_preview.{h,cpp}` and rename.**

- [ ] **Step 3: Replace PCSX2 keys with DuckStation keys**

Each "pill" that PCSX2 shows maps to a DuckStation equivalent:
- FPS pill → `ShowFPS`
- Speed pill → `ShowSpeed`
- CPU pill → `ShowCPU`
- GPU pill → `ShowGPU`
- Resolution pill → `ShowResolution`
- Status pill → `ShowStatusIndicators`
- Input pill → `ShowInputs`
- Message pill → `ShowOSDMessages`

If a DuckStation key has no PCSX2 equivalent pill, add a new pill in a free corner. If a PCSX2 pill has no DuckStation equivalent, remove it.

- [ ] **Step 4: Replace `OSDScale`/`OSDMargin` hookups** — DuckStation uses the same names (`OSDScale`, `OSDMargin`), so no change needed here; just verify.

- [ ] **Step 5: Add to CMakeLists.txt and commit.**

```sh
git commit -m "feat(duckstation-settings): add DuckStation OSD preview widget"
```

---

## Task 13: Graphics → OSD sub-page

**Files:**
- Create: `cpp/src/ui/settings/duckstation/pages/duckstation_graphics_osd_page.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.cpp`

Schema: `category == "On-Screen Display"` (note — this is the top-level OSD category, folded under Graphics per spec).

Layout:

```
Full-width: DuckStationOsdPreview card
Section: "Display"
    OSDScale   (slider)
    OSDMargin  (slider)
Section: "Visibility"   (toggle grid 2- or 3-col)
    <every bool key from the "On-Screen Display" category>
```

- [ ] **Steps 1–5: Recipe. Wire into graphics sub-tab stack, CMake, build, smoke-test, commit.**

```sh
git commit -m "feat(duckstation-settings): add Graphics OSD sub-page"
```

---

## Task 14: Audio page

**Files:**
- Create: `cpp/src/ui/settings/duckstation/pages/duckstation_audio_page.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/pages/pcsx2_audio_page.cpp`

Schema: `category == "Audio"`.

- [ ] **Steps 1–5: Apply the recipe.**

Read the Audio category keys from `duckstation_adapter.cpp` lines 376+. Group them using the schema's `group` field (probably: Configuration + Volume Controls, matching PCSX2's shape). Use `Pcsx2SliderRow` for any latency/volume/buffer ints, `Pcsx2ComboRow` for backend/driver/stretch mode, `Pcsx2ToggleRow` for mute. The horizontal-row pattern (e.g. `OutputLatency + Minimal` in PCSX2) — look for any DuckStation equivalent pairing; otherwise stack.

Wire into dialog, CMake, build, smoke-test, commit.

```sh
git commit -m "feat(duckstation-settings): add Audio page"
```

---

## Task 15: Memory Cards page

**Files:**
- Create: `cpp/src/ui/settings/duckstation/pages/duckstation_memory_cards_page.{h,cpp}`
- Reference: `cpp/src/ui/settings/pcsx2/pages/pcsx2_memory_cards_page.cpp`

Schema: `category == "Memory Cards"`.

DuckStation's memory card schema is different — each slot is a `Card1Type` / `Card1Path` combo, using the `memCardTypes` options list from the adapter. Slot card pattern:

```
Card:
    Pcsx2ComboRow    Card1Type
    QLabel           Card1Path (read-only filename display)
Card:
    Pcsx2ComboRow    Card2Type
    QLabel           Card2Path
```

- [ ] **Steps 1–5: Apply the recipe, adapting `pcsx2_memory_cards_page.cpp`'s `makeSlotCard` helper to use a combo (for Type) instead of a toggle (for Enable).**

Wire into dialog, CMake, build, smoke-test, commit.

```sh
git commit -m "feat(duckstation-settings): add Memory Cards page"
```

---

## Task 16: Manual verification pass

**Files:** none.

Final end-to-end verification. No code changes.

- [ ] **Step 1: Full build**

```sh
cd cpp && cmake --build build
```
Expected: clean build.

- [ ] **Step 2: Run unit tests**

```sh
cd cpp && ctest --test-dir build --output-on-failure -R 'duckstation'
```
Expected: `test_duckstation_aspect_ratio_preview` passes. Any OSD preview test if added.

- [ ] **Step 3: Launch the app and open DuckStation settings**

```sh
open ./build/RetroNest.app
```

Manually verify:
- Hub shows 5 cards with correct counts; Memory Cards stretched full-width on row 3.
- Keyboard navigation: arrow keys cycle cards; Down from Memory Cards focuses "Open Native Settings"; Up returns to Memory Cards.
- Each category card opens its page. Back button (and Escape, and gamepad B) returns to hub.
- Graphics page has 3 sub-tabs. L1/R1 (Tab/Shift+Tab) switches them.
- Every combo, toggle, slider round-trips through `settings.ini` on disk — change a value, close the dialog, reopen, value persists.
- Description bar at the bottom of every page populates when a setting is focused, shows the tooltip + "Recommended" chip.
- `dependsOn` chains (FineCrop* depends on FineCropMode; MaxQueuedFrames depends on UseThread; Runahead dependents depend on RunaheadFrameCount; Rewind dependents depend on RewindEnable) visually grey out/enable in real time.
- "Open Native Settings" button on the hub still launches the bundled DuckStation executable.

- [ ] **Step 4: Sanity-check PCSX2 didn't regress**

Open a PCSX2 game's settings. Hub and pages should be visually unchanged.

- [ ] **Step 5: Final commit (if any fix-up changes were needed)**

```sh
git commit -m "chore(duckstation-settings): verification pass fixes"   # only if anything changed
```

---

## Self-review notes (for future iteration)

- Widget naming: DuckStation pages `#include` files named `pcsx2_*.h`. This is intentional and documented in the spec; a future refactor may extract these to `ui/settings/common/`.
- Visual parity: the mockups rendered during brainstorming were acknowledged to be imperfect; tweaks are expected per-page as implementation proceeds. Priority during implementation is *structural* parity (cards, layout, navigation), not *pixel* parity.
- Post-Processing: DuckStation's post-processing pipeline is out of scope for this plan; no sub-tab.
- If `Pcsx2SliderRow` does not handle `SettingDef::Float` cleanly (Task 7 `RewindFrequency`), record a follow-up issue — do not expand the widget here.
