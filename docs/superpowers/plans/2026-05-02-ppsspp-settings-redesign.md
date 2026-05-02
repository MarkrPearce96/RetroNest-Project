# PPSSPP Settings Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the generic `EmulatorSettingsPage` used for PPSSPP with a bespoke Qt Widgets dialog that mirrors the PCSX2 / DuckStation shells (4-card hub → drill-down pages → Graphics sub-tab bar with 4 tabs). Includes a small schema reorganization to promote CPU/memory settings from `Graphics → Emulation` into a top-level `Emulation` category.

**Architecture:** New parallel tree at `cpp/src/ui/settings/ppsspp/` containing `ppsspp_settings_dialog`, `ppsspp_category_hub`, `ppsspp_theme.h` (alias over `Pcsx2Theme`), and 8 page files. All `pcsx2_*` widgets are reused in place by `#include`-ing the existing headers (same convention DuckStation already follows). Routing is added to `AppController::showEmulatorSettings()` at `cpp/src/ui/app_controller.cpp:862`.

**Tech Stack:** C++17, Qt6 (Widgets), CMake, QtTest. No new dependencies.

**Spec:** `docs/superpowers/specs/2026-05-02-ppsspp-settings-redesign-design.md`

---

## File Structure

**New files (created by this plan):**

```
cpp/src/ui/settings/ppsspp/
  ppsspp_settings_dialog.{h,cpp}
  ppsspp_category_hub.{h,cpp}
  ppsspp_theme.h
  pages/
    ppsspp_emulation_page.{h,cpp}
    ppsspp_audio_page.{h,cpp}
    ppsspp_overlay_page.{h,cpp}
    ppsspp_graphics_page.{h,cpp}
    ppsspp_graphics_rendering_page.{h,cpp}
    ppsspp_graphics_performance_page.{h,cpp}
    ppsspp_graphics_textures_page.{h,cpp}
    ppsspp_graphics_pacing_fx_page.{h,cpp}
```

**Modified files:**

- `cpp/src/adapters/ppsspp_adapter.cpp` — schema reorg (5 settings move category)
- `cpp/tests/test_ppsspp_schema.cpp` — assertions updated for new categories
- `cpp/src/ui/app_controller.cpp:862` — add `if (emuId == "ppsspp")` routing branch
- `cpp/src/ui/app_controller.cpp` (top) — add `#include` for the new dialog header
- `cpp/CMakeLists.txt` — register new source/header files

**Untouched files:**

- All existing `pcsx2_*` and `duckstation_*` files
- `EmulatorSettingsPage` (kept as fall-through for any future emulator)

---

## Phase 1 — Schema reorganization

This phase is TDD-friendly because `test_ppsspp_schema` already pins the schema's category strings. Update the test first, watch it fail, then make it pass with the schema change.

### Task 1: Update schema test expectations

**Files:**
- Modify: `cpp/tests/test_ppsspp_schema.cpp`

- [ ] **Step 1: Update three test methods**

Replace these methods in `cpp/tests/test_ppsspp_schema.cpp` with the versions below.

`testCategoriesAreGraphicsAudioOverlay` becomes `testTopLevelCategories`:

```cpp
    void testTopLevelCategories() {
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories, QSet<QString>({"Emulation", "Graphics", "Audio", "Overlay"}));
    }
```

`testGraphicsSubcategories` — drop `"Emulation"` from the expected set:

```cpp
    void testGraphicsSubcategories() {
        QSet<QString> subs;
        for (const auto& d : schema_)
            if (d.category == "Graphics") subs.insert(d.subcategory);
        QCOMPARE(subs, QSet<QString>({
            "Rendering", "Frame Pacing",
            "Performance", "Textures", "Post-Processing"
        }));
    }
```

`testEmulationSettingsLiveUnderGraphics` becomes `testEmulationSettingsAtTopLevel`:

```cpp
    void testEmulationSettingsAtTopLevel() {
        // FastMemoryAccess used to live under Graphics → Emulation.
        // It now lives under top-level Emulation category.
        bool found = false;
        for (const auto& d : schema_) {
            if (d.key == "FastMemoryAccess") {
                QCOMPARE(d.category, QString("Emulation"));
                QCOMPARE(d.subcategory, QString(""));
                found = true;
            }
        }
        QVERIFY(found);
    }
```

- [ ] **Step 2: Run test, verify it fails**

Run:
```sh
cmake --build cpp/build --target test_ppsspp_schema && cpp/build/test_ppsspp_schema
```
Expected: at least one of `testTopLevelCategories`, `testGraphicsSubcategories`, `testEmulationSettingsAtTopLevel` FAILs because the schema still has the old categorization.

- [ ] **Step 3: Commit**

```sh
git add cpp/tests/test_ppsspp_schema.cpp
git commit -m "PPSSPP schema test: assert top-level Emulation category"
```

---

### Task 2: Move Emulation settings out of Graphics in the schema

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp` (lines ~60–79)

- [ ] **Step 1: Update the 5 schema entries**

Open `cpp/src/adapters/ppsspp_adapter.cpp`. In `PPSSPPAdapter::settingsSchema()`, find the block currently labelled `// Graphics → Emulation  (moved from top-level Emulation category)` (around line 60). Change the **first two positional fields** of all 5 `s.append({...})` calls in that block from `{"Graphics", "Emulation", ...}` to `{"Emulation", "", ...}`. The rest of each line is unchanged.

The block becomes:

```cpp
    // ═══════════════════════════════════════════════════════════════════════
    // Emulation (top-level — moved out of Graphics for parity with PCSX2/DS)
    // ═══════════════════════════════════════════════════════════════════════
    s.append({"Emulation", "", "", "CPU", "FastMemoryAccess", "Fast Memory (Unstable)",
              "Uses faster but less accurate memory access. May cause crashes in some games.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Emulation", "", "", "General", "IgnoreBadMemAccess", "Ignore Bad Memory Accesses",
              "Silently ignores invalid memory reads/writes instead of crashing.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Emulation", "", "", "CPU", "IOTimingMethod", "I/O Timing Method",
              "Controls how UMD (disc) I/O timing is handled.",
              SettingDef::Combo, "0",
              {{"Fast (lag on slow storage)", "0"}, {"Host", "1"},
               {"Simulate UMD Delays", "2"}, {"Simulate UMD Slow", "3"}}, 0, 0, 0});
    s.append({"Emulation", "", "", "General", "ForceLagSync2", "Force Real Clock Sync",
              "Slower but less lag. Forces the emulator to run at real clock speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Emulation", "", "", "CPU", "CPUSpeed", "CPU Clock (MHz)",
              "Overclock the emulated PSP's CPU. 0 = default (222 MHz). Unstable on high values.",
              SettingDef::Int, "0", {}, 0, 1000, 1, "slider", "MHz"});
```

- [ ] **Step 2: Build and run schema test**

Run:
```sh
cmake --build cpp/build --target test_ppsspp_schema && cpp/build/test_ppsspp_schema
```
Expected: PASS — all assertions in `test_ppsspp_schema` succeed.

- [ ] **Step 3: Build full app to confirm nothing else broke**

Run:
```sh
cmake --build cpp/build --target RetroNest
```
Expected: build succeeds.

- [ ] **Step 4: Commit**

```sh
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "PPSSPP schema: promote Emulation settings to top-level category"
```

---

## Phase 2 — Theme + dialog shell

### Task 3: Create the PPSSPP theme alias header

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/ppsspp_theme.h`

- [ ] **Step 1: Create the header**

Create directory and write the file:

```sh
mkdir -p cpp/src/ui/settings/ppsspp/pages
```

Write `cpp/src/ui/settings/ppsspp/ppsspp_theme.h`:

```cpp
#pragma once
#include "../pcsx2/pcsx2_theme.h"

// PPSSPP settings dialog uses the same warm-grey + amber palette as
// PCSX2 and DuckStation. Spec 2026-05-02 explicitly defers any per-
// emulator visual divergence — this header is a thin alias so that
// PPSSPP code can reference PpssppTheme:: without coupling directly
// to Pcsx2Theme.
namespace PpssppTheme {
    using namespace Pcsx2Theme;
}
```

- [ ] **Step 2: Register header in CMakeLists.txt**

Open `cpp/CMakeLists.txt`. Find the `set(HEADERS ...)` block (line ~115). Inside the block, after the line for `src/ui/settings/pcsx2/pcsx2_category_hub.h`, add:

```
    src/ui/settings/ppsspp/ppsspp_theme.h
```

- [ ] **Step 3: Build to confirm header compiles when included**

The header is only included where used; this step is just a syntax check.

- [ ] **Step 4: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/ppsspp_theme.h cpp/CMakeLists.txt
git commit -m "PPSSPP settings: theme alias header over Pcsx2Theme"
```

---

### Task 4: Create the category hub

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/ppsspp_category_hub.{h,cpp}`

- [ ] **Step 1: Write the header**

Write `cpp/src/ui/settings/ppsspp/ppsspp_category_hub.h`:

```cpp
#pragma once
#include <QWidget>

class Pcsx2Card;
class QPushButton;

class PpssppCategoryHub : public QWidget {
    Q_OBJECT
public:
    explicit PpssppCategoryHub(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

signals:
    void categoryActivated(QString category);
    void openNativeRequested();

private:
    Pcsx2Card* makeCard(const QString& icon, const QString& title,
                        const QString& descriptor, int settingCount,
                        const QString& categoryKey);
    QPushButton* m_nativeBtn = nullptr;
};
```

- [ ] **Step 2: Write the .cpp**

Write `cpp/src/ui/settings/ppsspp/ppsspp_category_hub.cpp`:

```cpp
#include "ppsspp_category_hub.h"
#include "../pcsx2/widgets/pcsx2_card.h"
#include "ppsspp_theme.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>

PpssppCategoryHub::PpssppCategoryHub(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(14);

    auto* title = new QLabel("PPSSPP Settings", this);
    title->setStyleSheet("color:#f2efe8;font-size:20px;font-weight:600;");
    root->addWidget(title);

    auto* grid = new QGridLayout();
    grid->setSpacing(14);
    grid->addWidget(makeCard(QStringLiteral("\U0001F3AE"), "Emulation",
                             "CPU clock, fast memory, I/O timing, real clock sync", 5, "Emulation"),
                    0, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F5BC"), "Graphics",
                             "Backend, resolution, frame pacing, textures, post-FX", 30, "Graphics"),
                    0, 1);
    grid->addWidget(makeCard(QStringLiteral("\U0001F50A"), "Audio",
                             "Backend, latency, volume mix, UI sounds", 12, "Audio"),
                    1, 0);
    grid->addWidget(makeCard(QStringLiteral("\U0001F4CA"), "Overlay",
                             "FPS counter, speed, battery, debug overlay", 4, "Overlay"),
                    1, 1);
    root->addLayout(grid, 1);

    m_nativeBtn = new QPushButton("Open Native Settings", this);
    m_nativeBtn->setCursor(Qt::PointingHandCursor);
    m_nativeBtn->setStyleSheet(
        "QPushButton { background:#4a4642; color:#f2efe8; border:1px solid #706c66;"
        " border-radius:4px; padding:6px 14px; }"
        "QPushButton:focus { border-color:#f59e0b; }");
    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    bottom->addWidget(m_nativeBtn);
    root->addLayout(bottom);
    connect(m_nativeBtn, &QPushButton::clicked, this, &PpssppCategoryHub::openNativeRequested);
    m_nativeBtn->installEventFilter(this);

    // Install filter on bottom-row cards so Down → native button works.
    const auto cards = findChildren<Pcsx2Card*>(QString(), Qt::FindDirectChildrenOnly);
    if (cards.size() >= 2) {
        cards[cards.size() - 2]->installEventFilter(this);
        cards[cards.size() - 1]->installEventFilter(this);
    }
}

bool PpssppCategoryHub::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, e);

    auto* ke = static_cast<QKeyEvent*>(e);

    if (obj == m_nativeBtn) {
        if (ke->key() == Qt::Key_Up) {
            const auto cards = findChildren<Pcsx2Card*>(QString(), Qt::FindDirectChildrenOnly);
            if (!cards.isEmpty()) {
                cards.last()->setFocus(Qt::TabFocusReason);
                return true;
            }
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            emit openNativeRequested();
            return true;
        }
    }

    if (auto* card = qobject_cast<Pcsx2Card*>(obj)) {
        if (ke->key() == Qt::Key_Down) {
            m_nativeBtn->setFocus(Qt::TabFocusReason);
            return true;
        }
    }

    return QWidget::eventFilter(obj, e);
}

Pcsx2Card* PpssppCategoryHub::makeCard(const QString& icon, const QString& title,
                                       const QString& descriptor, int settingCount,
                                       const QString& categoryKey) {
    auto* card = new Pcsx2Card(this);
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(180);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(20, 22, 20, 18);
    v->setSpacing(8);
    v->setAlignment(Qt::AlignHCenter);

    auto* iconTile = new QLabel(icon, card);
    iconTile->setAlignment(Qt::AlignCenter);
    iconTile->setFixedSize(56, 56);
    iconTile->setStyleSheet(
        "QLabel {"
        "  background-color: #585450;"
        "  border-radius: 12px;"
        "  font-size: 28px;"
        "  color: #f2efe8;"
        "}");

    auto* t = new QLabel(title, card);
    t->setAlignment(Qt::AlignCenter);
    t->setStyleSheet("color:#f2efe8;font-size:18px;font-weight:600;");

    auto* d = new QLabel(descriptor, card);
    d->setAlignment(Qt::AlignCenter);
    d->setWordWrap(true);
    d->setStyleSheet("color:#b0aca4;font-size:13px;");

    auto* c = new QLabel(QString("%1 settings  →").arg(settingCount), card);
    c->setAlignment(Qt::AlignCenter);
    c->setStyleSheet("color:#f59e0b;font-size:12px;font-weight:500;");

    v->addWidget(iconTile, 0, Qt::AlignHCenter);
    v->addSpacing(4);
    v->addWidget(t);
    v->addWidget(d);
    v->addStretch();
    v->addWidget(c);

    QObject::connect(card, &Pcsx2Card::activated, this,
                     [this, categoryKey]{ emit categoryActivated(categoryKey); });
    return card;
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

Open `cpp/CMakeLists.txt`. After the `pcsx2_category_hub.cpp` line in `set(SOURCES ...)`, add:

```
    src/ui/settings/ppsspp/ppsspp_category_hub.cpp
```

After the `pcsx2_category_hub.h` line in `set(HEADERS ...)`, add:

```
    src/ui/settings/ppsspp/ppsspp_category_hub.h
```

- [ ] **Step 4: Build**

Run: `cmake --build cpp/build --target RetroNest`
Expected: builds successfully (the hub isn't reachable yet — that comes in Task 5).

- [ ] **Step 5: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/ppsspp_category_hub.h \
        cpp/src/ui/settings/ppsspp/ppsspp_category_hub.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: 4-card category hub"
```

---

### Task 5: Create the dialog shell

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.{h,cpp}`

- [ ] **Step 1: Write the header**

Write `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.h`:

```cpp
#pragma once
#include <QDialog>
#include <QStack>
#include "core/setting_def.h"

class AppController;
class QStackedWidget;
class Pcsx2DescriptionBar;
class PpssppCategoryHub;

class PpssppSettingsDialog : public QDialog {
    Q_OBJECT
public:
    PpssppSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

    void pushPage(QWidget* page, bool hasSubTabs = false);
    void popPage();
    AppController* appController() const { return m_app; }
    QString emuId() const { return m_emuId; }

public slots:
    void setFocusedSetting(const SettingDef& def);
    void clearFocusedSetting();

protected:
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void onCategoryActivated(const QString& category);

private:
    void applyHintsForCurrentPage();

    AppController* m_app;
    QString m_emuId;
    QStackedWidget* m_stack = nullptr;
    Pcsx2DescriptionBar* m_descBar = nullptr;
    PpssppCategoryHub* m_hub = nullptr;
    QStack<int> m_history;
    bool m_currentPageHasSubTabs = false;
};
```

- [ ] **Step 2: Write the .cpp**

Write `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp`. Page-routing branches are stubbed — they will be filled in by later tasks as each page is built.

```cpp
#include "ppsspp_settings_dialog.h"
#include "ppsspp_category_hub.h"
#include "../pcsx2/widgets/pcsx2_card.h"
#include "../pcsx2/widgets/pcsx2_description_bar.h"
#include "ppsspp_theme.h"
#include "ui/app_controller.h"
#include "core/sdl_input_manager.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QKeyEvent>

PpssppSettingsDialog::PpssppSettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : QDialog(parent), m_app(app), m_emuId(emuId) {
    setWindowTitle("PPSSPP Settings");
    setMinimumSize(950, 550);
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(PpssppTheme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_descBar = new Pcsx2DescriptionBar(this);
    m_descBar->setInputManager(app->sdlInputManager());

    m_hub = new PpssppCategoryHub(this);
    connect(m_hub, &PpssppCategoryHub::categoryActivated,
            this, &PpssppSettingsDialog::onCategoryActivated);
    connect(m_hub, &PpssppCategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    m_stack->addWidget(m_hub);

    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int index) {
        bool onHub = (m_stack->widget(index) == m_hub);
        m_descBar->setDescriptionVisible(!onHub);
        if (onHub) m_descBar->clear();
        applyHintsForCurrentPage();
    });

    root->addWidget(m_stack, 1);
    root->addWidget(m_descBar, 0);

    m_descBar->setDescriptionVisible(false);
    applyHintsForCurrentPage();
}

void PpssppSettingsDialog::pushPage(QWidget* page, bool hasSubTabs) {
    m_currentPageHasSubTabs = hasSubTabs;
    int idx = m_stack->addWidget(page);
    m_history.push(m_stack->currentIndex());
    m_stack->setCurrentIndex(idx);
    if (size().width() < 1000 || size().height() < 700) resize(1000, 700);
    clearFocusedSetting();

    for (auto* card : page->findChildren<Pcsx2Card*>()) {
        if (card->focusPolicy() != Qt::NoFocus) {
            card->setFocus(Qt::OtherFocusReason);
            break;
        }
    }
}

void PpssppSettingsDialog::popPage() {
    if (m_history.isEmpty()) { accept(); return; }
    QWidget* current = m_stack->currentWidget();
    int prev = m_history.pop();
    m_stack->setCurrentIndex(prev);
    m_currentPageHasSubTabs = false;
    if (m_stack->currentWidget() == m_hub) resize(950, 550);
    if (current && current != m_hub) { m_stack->removeWidget(current); current->deleteLater(); }
    clearFocusedSetting();
}

void PpssppSettingsDialog::setFocusedSetting(const SettingDef& def) { m_descBar->setSetting(def); }
void PpssppSettingsDialog::clearFocusedSetting() { m_descBar->clear(); }

void PpssppSettingsDialog::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Back) { popPage(); return; }
    if ((e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) &&
        !m_currentPageHasSubTabs && m_stack->currentWidget() != m_hub) {
        e->accept();
        return;
    }
    QDialog::keyPressEvent(e);
}

void PpssppSettingsDialog::applyHintsForCurrentPage() {
    using BH = Pcsx2DescriptionBar::ButtonHint;
    if (m_stack->currentWidget() == m_hub) {
        m_descBar->setHints({
            BH{"navigate_ud", "Navigate"}, BH{"confirm", "Select"}, BH{"back", "Close"},
        });
    } else if (m_currentPageHasSubTabs) {
        m_descBar->setHints({
            BH{"navigate", "Navigate"}, BH{"confirm", "Select"},
            BH{"switch_tab", "Switch Tab"}, BH{"back", "Back"},
        });
    } else {
        m_descBar->setHints({
            BH{"navigate", "Navigate"}, BH{"confirm", "Select"}, BH{"back", "Back"},
        });
    }
}

void PpssppSettingsDialog::onCategoryActivated(const QString& category) {
    // Page branches wired in Tasks 6, 7, 8, 13.
    Q_UNUSED(category);
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

In `cpp/CMakeLists.txt`, after the `ppsspp_category_hub.cpp` line, add:

```
    src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp
```

After the `ppsspp_category_hub.h` line, add:

```
    src/ui/settings/ppsspp/ppsspp_settings_dialog.h
```

- [ ] **Step 4: Wire routing in app_controller.cpp**

Open `cpp/src/ui/app_controller.cpp`. After line 13 (the duckstation include), add:

```cpp
#include "settings/ppsspp/ppsspp_settings_dialog.h"
```

In `AppController::showEmulatorSettings()` at line 862, add a third branch BEFORE the `EmulatorSettingsPage` fall-through:

```cpp
    if (emuId == QLatin1String("ppsspp")) {
        auto* dialog = new PpssppSettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
```

- [ ] **Step 5: Build**

Run: `cmake --build cpp/build --target RetroNest`
Expected: build succeeds.

- [ ] **Step 6: Manual smoke test**

Run: `open ./cpp/build/RetroNest.app`
- Open PPSSPP settings (via Settings → Emulators → PPSSPP, or however the app exposes it).
- Expected: the new 4-card hub appears with the warm-grey + amber palette.
- Clicking a category card does nothing yet (branches are stubbed) — that is correct for this task.
- Clicking "Open Native Settings" launches the real PPSSPP — expected.
- Pressing Escape / B closes the dialog.

- [ ] **Step 7: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.h \
        cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp \
        cpp/src/ui/app_controller.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: dialog shell + app_controller routing"
```

---

## Phase 3 — Flat pages

### Task 6: Emulation page

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/pages/ppsspp_emulation_page.{h,cpp}`
- Modify: `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp` (wire route)
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Write `cpp/src/ui/settings/ppsspp/pages/ppsspp_emulation_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppEmulationPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppEmulationPage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
```

- [ ] **Step 2: Write the .cpp**

Write `cpp/src/ui/settings/ppsspp/pages/ppsspp_emulation_page.cpp`. The 5 settings being rendered are the ones moved in Task 2: `FastMemoryAccess`, `IgnoreBadMemAccess`, `IOTimingMethod`, `ForceLagSync2`, `CPUSpeed`.

```cpp
#include "ppsspp_emulation_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "../../pcsx2/widgets/pcsx2_slider_row.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>

PpssppEmulationPage::PpssppEmulationPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Emulation") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppEmulationPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppEmulationPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }"
        "QScrollBar::handle:vertical { background: #706c66; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #7a7670; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    auto* back = new QPushButton(QString::fromUtf8("\xE2\x86\x90 Back"), content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &PpssppSettingsDialog::popPage);
    root->addWidget(back);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppEmulationPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::focused, this, &PpssppEmulationPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppEmulationPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppEmulationPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };
    auto makeSliderCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2SliderRow(card);
        row->setLabel(d->label);
        row->setRange(int(d->minVal), int(d->maxVal));
        row->setSuffix(d->suffix);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppEmulationPage::settingFocused);
        connect(row, &Pcsx2SliderRow::focused, this, &PpssppEmulationPage::settingFocused);
        connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, QString::number(val));
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new Pcsx2SectionHeader("CPU & Memory", this));
    if (auto* c = makeToggleCard("FastMemoryAccess"))   root->addWidget(c);
    if (auto* c = makeToggleCard("IgnoreBadMemAccess")) root->addWidget(c);
    if (auto* c = makeComboCard ("IOTimingMethod"))     root->addWidget(c);
    if (auto* c = makeToggleCard("ForceLagSync2"))      root->addWidget(c);
    if (auto* c = makeSliderCard("CPUSpeed"))           root->addWidget(c);

    root->addStretch();
}

void PpssppEmulationPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* row : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setValue(v.toInt());
    }
}

void PpssppEmulationPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

- [ ] **Step 3: Wire route in PpssppSettingsDialog**

In `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp`, add this include near the others:

```cpp
#include "pages/ppsspp_emulation_page.h"
```

Then in `onCategoryActivated()`, replace the body with:

```cpp
void PpssppSettingsDialog::onCategoryActivated(const QString& category) {
    if (category == "Emulation") {
        auto* page = new PpssppEmulationPage(this);
        connect(page, &PpssppEmulationPage::settingFocused, this, &PpssppSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    // Audio / Overlay / Graphics branches wired in later tasks.
}
```

- [ ] **Step 4: Register in CMakeLists.txt**

In `cpp/CMakeLists.txt` add the cpp under SOURCES (after `ppsspp_settings_dialog.cpp`):

```
    src/ui/settings/ppsspp/pages/ppsspp_emulation_page.cpp
```

And the .h under HEADERS:

```
    src/ui/settings/ppsspp/pages/ppsspp_emulation_page.h
```

- [ ] **Step 5: Build and manually verify**

Run: `cmake --build cpp/build --target RetroNest && open ./cpp/build/RetroNest.app`
Expected:
- Open PPSSPP settings → click "Emulation" card → drill-down page opens with 5 rows (Fast Memory, Ignore Bad Memory, I/O Timing, Force Real Clock Sync, CPU Clock).
- Toggling Fast Memory and pressing Back; reopening the page shows the new state. Confirm `cpp/build/RetroNest.app/Contents/Resources/...` (or the `{root}/emulators/ppsspp/PSP/SYSTEM/ppsspp.ini` file) was updated.

- [ ] **Step 6: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/pages/ppsspp_emulation_page.h \
        cpp/src/ui/settings/ppsspp/pages/ppsspp_emulation_page.cpp \
        cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: Emulation page"
```

---

### Task 7: Audio page

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/pages/ppsspp_audio_page.{h,cpp}`
- Modify: `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Write `cpp/src/ui/settings/ppsspp/pages/ppsspp_audio_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppAudioPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppAudioPage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
```

- [ ] **Step 2: Write the .cpp**

The Audio category has 12 settings across 4 subcategories (`Audio playback`, `Game volume`, `UI sound`, `Audio backend`). Render each subcategory as a `Pcsx2SectionHeader` followed by its rows.

Write `cpp/src/ui/settings/ppsspp/pages/ppsspp_audio_page.cpp`:

```cpp
#include "ppsspp_audio_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "../../pcsx2/widgets/pcsx2_slider_row.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>

PpssppAudioPage::PpssppAudioPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Audio") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppAudioPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppAudioPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }"
        "QScrollBar::handle:vertical { background: #706c66; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #7a7670; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    auto* back = new QPushButton(QString::fromUtf8("\xE2\x86\x90 Back"), content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &PpssppSettingsDialog::popPage);
    root->addWidget(back);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppAudioPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::focused, this, &PpssppAudioPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppAudioPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppAudioPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };
    auto makeSliderCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2SliderRow(card);
        row->setLabel(d->label);
        row->setRange(int(d->minVal), int(d->maxVal));
        row->setSuffix(d->suffix);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppAudioPage::settingFocused);
        connect(row, &Pcsx2SliderRow::focused, this, &PpssppAudioPage::settingFocused);
        connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, QString::number(val));
        });
        v->addWidget(row);
        return card;
    };

    // Audio playback
    root->addWidget(new Pcsx2SectionHeader("Audio Playback", this));
    if (auto* c = makeComboCard ("AudioSyncMode")) root->addWidget(c);
    if (auto* c = makeToggleCard("FillAudioGaps")) root->addWidget(c);

    // Game volume
    root->addWidget(new Pcsx2SectionHeader("Game Volume", this));
    if (auto* c = makeToggleCard("Enable"))                  root->addWidget(c);
    if (auto* c = makeSliderCard("GameVolume"))              root->addWidget(c);
    if (auto* c = makeSliderCard("ReverbRelativeVolume"))    root->addWidget(c);
    if (auto* c = makeSliderCard("AltSpeedRelativeVolume"))  root->addWidget(c);
    if (auto* c = makeSliderCard("AchievementVolume"))       root->addWidget(c);

    // UI sound
    root->addWidget(new Pcsx2SectionHeader("UI Sound", this));
    if (auto* c = makeToggleCard("UISound"))           root->addWidget(c);
    if (auto* c = makeSliderCard("UIVolume"))          root->addWidget(c);
    if (auto* c = makeSliderCard("GamePreviewVolume")) root->addWidget(c);

    // Audio backend
    root->addWidget(new Pcsx2SectionHeader("Audio Backend", this));
    if (auto* c = makeSliderCard("AudioBufferSize")) root->addWidget(c);
    if (auto* c = makeToggleCard("AutoAudioDevice")) root->addWidget(c);

    root->addStretch();
}

void PpssppAudioPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* row : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setValue(v.toInt());
    }
}

void PpssppAudioPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

- [ ] **Step 3: Wire route in PpssppSettingsDialog**

In `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp` add include:

```cpp
#include "pages/ppsspp_audio_page.h"
```

Add the Audio branch inside `onCategoryActivated()` BEFORE the trailing comment:

```cpp
    if (category == "Audio") {
        auto* page = new PpssppAudioPage(this);
        connect(page, &PpssppAudioPage::settingFocused, this, &PpssppSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/ppsspp/pages/ppsspp_audio_page.cpp
```
Add to HEADERS:
```
    src/ui/settings/ppsspp/pages/ppsspp_audio_page.h
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build cpp/build --target RetroNest && open ./cpp/build/RetroNest.app`
Expected: open PPSSPP settings → Audio card → drill-down with 4 section headers and 12 rows. Adjusting Game Volume slider updates ppsspp.ini.

- [ ] **Step 6: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/pages/ppsspp_audio_page.h \
        cpp/src/ui/settings/ppsspp/pages/ppsspp_audio_page.cpp \
        cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: Audio page"
```

---

### Task 8: Overlay page (with bitmask toggles)

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/pages/ppsspp_overlay_page.{h,cpp}`
- Modify: `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp`
- Modify: `cpp/CMakeLists.txt`

PPSSPP's "Show FPS / Speed / Battery %" toggles all share INI key `iShowStatusFlags` with different bitmasks (2/4/8). Reading and writing must use `BitmaskHelpers::getBit` / `setBit` and re-read the current int from disk on every save so multiple toggles don't clobber each other.

- [ ] **Step 1: Write the header**

Write `cpp/src/ui/settings/ppsspp/pages/ppsspp_overlay_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppOverlayPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppOverlayPage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveBitmaskBit(const SettingDef& def, bool checked);
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findByLabel(const QString& label) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
```

- [ ] **Step 2: Write the .cpp**

```cpp
#include "ppsspp_overlay_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "core/bitmask_helpers.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>

PpssppOverlayPage::PpssppOverlayPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Overlay") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppOverlayPage::findByLabel(const QString& label) const {
    for (const auto& d : m_schema) if (d.label == label) return &d;
    return nullptr;
}

void PpssppOverlayPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }"
        "QScrollBar::handle:vertical { background: #706c66; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #7a7670; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    auto* back = new QPushButton(QString::fromUtf8("\xE2\x86\x90 Back"), content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &PpssppSettingsDialog::popPage);
    root->addWidget(back);

    auto makeBitmaskCard = [this](const QString& label) -> Pcsx2Card* {
        const SettingDef* d = findByLabel(label);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppOverlayPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppOverlayPage::settingFocused);
        SettingDef defCopy = *d;
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, defCopy](bool on){
            saveBitmaskBit(defCopy, on);
        });
        v->addWidget(row);
        return card;
    };
    auto makeComboCard = [this](const QString& label) -> Pcsx2Card* {
        const SettingDef* d = findByLabel(label);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppOverlayPage::settingFocused);
        connect(row, &Pcsx2ComboRow::focused, this, &PpssppOverlayPage::settingFocused);
        SettingDef defCopy = *d;
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, defCopy](const QString& v){
            saveValue(defCopy.section, defCopy.key, v);
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new Pcsx2SectionHeader("Status Indicators", this));
    if (auto* c = makeBitmaskCard("Show FPS Counter")) root->addWidget(c);
    if (auto* c = makeBitmaskCard("Show Speed"))       root->addWidget(c);
    if (auto* c = makeBitmaskCard("Show Battery %"))   root->addWidget(c);

    root->addWidget(new Pcsx2SectionHeader("Debug", this));
    if (auto* c = makeComboCard("Debug Overlay")) root->addWidget(c);

    root->addStretch();
}

void PpssppOverlayPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const int curInt = cur.isEmpty() ? d.defaultValue.toInt() : cur.toInt();
        row->setChecked(BitmaskHelpers::getBit(curInt, d.bitmask));
    }
    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
}

void PpssppOverlayPage::saveBitmaskBit(const SettingDef& def, bool checked) {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();
    // Re-read current int from disk so other bits stay intact when multiple
    // bitmask toggles share the same key.
    QString cur = app->settingValue(emuId, def.section, def.key);
    const int curInt = cur.isEmpty() ? def.defaultValue.toInt() : cur.toInt();
    const int newInt = BitmaskHelpers::setBit(curInt, def.bitmask, checked);
    saveValue(def.section, def.key, QString::number(newInt));
}

void PpssppOverlayPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

- [ ] **Step 3: Wire route**

In `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp` add include:

```cpp
#include "pages/ppsspp_overlay_page.h"
```

Add the branch in `onCategoryActivated()`:

```cpp
    if (category == "Overlay") {
        auto* page = new PpssppOverlayPage(this);
        connect(page, &PpssppOverlayPage::settingFocused, this, &PpssppSettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/ppsspp/pages/ppsspp_overlay_page.cpp
```
Add to HEADERS:
```
    src/ui/settings/ppsspp/pages/ppsspp_overlay_page.h
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build cpp/build --target RetroNest && open ./cpp/build/RetroNest.app`
Expected:
- Overlay drill-down shows 3 bitmask toggles + Debug Overlay combo.
- Toggle "Show FPS Counter" → ppsspp.ini `[Graphics] iShowStatusFlags` adds bit 2.
- Toggle "Show Speed" without unchecking FPS → bits 2 + 4 both set (value = 6).

- [ ] **Step 6: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/pages/ppsspp_overlay_page.h \
        cpp/src/ui/settings/ppsspp/pages/ppsspp_overlay_page.cpp \
        cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: Overlay page (bitmask toggles + debug overlay)"
```

---

## Phase 4 — Graphics tree

The Graphics page has a sub-tab container plus 4 sub-pages. Each sub-page is a flat-list page like Emulation/Audio. Build the 4 sub-pages first, then wire them into the container.

### Task 9: Graphics → Rendering sub-page

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_rendering_page.{h,cpp}`
- Modify: `cpp/CMakeLists.txt`

Settings on this page (subcategory `"Rendering"`): `GraphicsBackend` (combo), `InternalResolution` (combo), `SoftwareRenderer` (toggle), `MultiSampleLevel` (combo), `ReplaceTextures` (toggle).

- [ ] **Step 1: Write the header**

`cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_rendering_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppGraphicsRenderingPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppGraphicsRenderingPage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
```

- [ ] **Step 2: Write the .cpp**

```cpp
#include "ppsspp_graphics_rendering_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QVariantMap>

PpssppGraphicsRenderingPage::PpssppGraphicsRenderingPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Graphics" && d.subcategory == "Rendering") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppGraphicsRenderingPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppGraphicsRenderingPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ComboRow::focused, this, &PpssppGraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppGraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new Pcsx2SectionHeader("Backend & Resolution", this));
    if (auto* c = makeComboCard ("GraphicsBackend"))    root->addWidget(c);
    if (auto* c = makeComboCard ("InternalResolution")) root->addWidget(c);
    if (auto* c = makeToggleCard("SoftwareRenderer"))   root->addWidget(c);
    if (auto* c = makeComboCard ("MultiSampleLevel"))   root->addWidget(c);

    root->addWidget(new Pcsx2SectionHeader("Textures", this));
    if (auto* c = makeToggleCard("ReplaceTextures")) root->addWidget(c);

    root->addStretch();
}

void PpssppGraphicsRenderingPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
}

void PpssppGraphicsRenderingPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_rendering_page.cpp
```
Add to HEADERS:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_rendering_page.h
```

- [ ] **Step 4: Build**

Run: `cmake --build cpp/build --target RetroNest`
Expected: builds. (Page isn't reachable yet — wired in Task 13.)

- [ ] **Step 5: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_rendering_page.h \
        cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_rendering_page.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: Graphics → Rendering sub-page"
```

---

### Task 10: Graphics → Performance sub-page

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_performance_page.{h,cpp}`
- Modify: `cpp/CMakeLists.txt`

The schema for this sub-tab uses the third positional field (`group`) to split rows into two visual sections: `"Performance"` (4 settings) and `"Speed Hacks"` (7 settings). Render them as two `Pcsx2SectionHeader`s.

- [ ] **Step 1: Write the header**

`cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_performance_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppGraphicsPerformancePage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppGraphicsPerformancePage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
```

- [ ] **Step 2: Write the .cpp**

```cpp
#include "ppsspp_graphics_performance_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QVariantMap>

PpssppGraphicsPerformancePage::PpssppGraphicsPerformancePage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Graphics" && d.subcategory == "Performance") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppGraphicsPerformancePage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppGraphicsPerformancePage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsPerformancePage::settingFocused);
        connect(row, &Pcsx2ComboRow::focused, this, &PpssppGraphicsPerformancePage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsPerformancePage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppGraphicsPerformancePage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    // Section 1: "Performance" group (4 settings)
    root->addWidget(new Pcsx2SectionHeader("Performance", this));
    if (auto* c = makeComboCard ("InflightFrames"))       root->addWidget(c);
    if (auto* c = makeToggleCard("HardwareTransform"))    root->addWidget(c);
    if (auto* c = makeToggleCard("SoftwareSkinning"))     root->addWidget(c);
    if (auto* c = makeToggleCard("HardwareTessellation")) root->addWidget(c);

    // Section 2: "Speed Hacks" group (7 settings)
    root->addWidget(new Pcsx2SectionHeader("Speed Hacks", this));
    if (auto* c = makeToggleCard("SkipBufferEffects"))    root->addWidget(c);
    if (auto* c = makeToggleCard("DisableRangeCulling"))  root->addWidget(c);
    if (auto* c = makeComboCard ("SkipGPUReadbackMode"))  root->addWidget(c);
    if (auto* c = makeToggleCard("TextureBackoffCache"))  root->addWidget(c);
    if (auto* c = makeComboCard ("SplineBezierQuality"))  root->addWidget(c);
    if (auto* c = makeComboCard ("BloomHack"))            root->addWidget(c);
    if (auto* c = makeComboCard ("DepthRasterMode"))      root->addWidget(c);

    root->addStretch();
}

void PpssppGraphicsPerformancePage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
}

void PpssppGraphicsPerformancePage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_performance_page.cpp
```
Add to HEADERS:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_performance_page.h
```

- [ ] **Step 4: Build**

Run: `cmake --build cpp/build --target RetroNest`
Expected: builds.

- [ ] **Step 5: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_performance_page.h \
        cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_performance_page.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: Graphics → Performance sub-page"
```

---

### Task 11: Graphics → Textures sub-page

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_textures_page.{h,cpp}`
- Modify: `cpp/CMakeLists.txt`

Settings (subcategory `"Textures"`, 7): `TexHardwareScaling` (toggle), `TexScalingType` (combo), `TexScalingLevel` (combo), `TexDeposterize` (toggle), `AnisotropyLevel` (combo), `TextureFiltering` (combo), `Smart2DTexFiltering` (toggle).

- [ ] **Step 1: Write the header**

`cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_textures_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppGraphicsTexturesPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppGraphicsTexturesPage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
```

- [ ] **Step 2: Write the .cpp**

```cpp
#include "ppsspp_graphics_textures_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QVariantMap>

PpssppGraphicsTexturesPage::PpssppGraphicsTexturesPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Graphics" && d.subcategory == "Textures") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppGraphicsTexturesPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppGraphicsTexturesPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsTexturesPage::settingFocused);
        connect(row, &Pcsx2ComboRow::focused, this, &PpssppGraphicsTexturesPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsTexturesPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppGraphicsTexturesPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new Pcsx2SectionHeader("Texture Upscaling", this));
    if (auto* c = makeToggleCard("TexHardwareScaling")) root->addWidget(c);
    if (auto* c = makeComboCard ("TexScalingType"))     root->addWidget(c);
    if (auto* c = makeComboCard ("TexScalingLevel"))    root->addWidget(c);
    if (auto* c = makeToggleCard("TexDeposterize"))     root->addWidget(c);

    root->addWidget(new Pcsx2SectionHeader("Filtering", this));
    if (auto* c = makeComboCard ("AnisotropyLevel"))     root->addWidget(c);
    if (auto* c = makeComboCard ("TextureFiltering"))    root->addWidget(c);
    if (auto* c = makeToggleCard("Smart2DTexFiltering")) root->addWidget(c);

    root->addStretch();
}

void PpssppGraphicsTexturesPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
}

void PpssppGraphicsTexturesPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_textures_page.cpp
```
Add to HEADERS:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_textures_page.h
```

- [ ] **Step 4: Build**

Run: `cmake --build cpp/build --target RetroNest`
Expected: builds.

- [ ] **Step 5: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_textures_page.h \
        cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_textures_page.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: Graphics → Textures sub-page"
```

---

### Task 12: Graphics → Pacing & FX sub-page

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_pacing_fx_page.{h,cpp}`
- Modify: `cpp/CMakeLists.txt`

This sub-page combines two source subcategories: `"Frame Pacing"` (6 settings) and `"Post-Processing"` (1 setting). Render two section headers.

- [ ] **Step 1: Write the header**

`cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_pacing_fx_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppGraphicsPacingFxPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppGraphicsPacingFxPage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
```

- [ ] **Step 2: Write the .cpp**

```cpp
#include "ppsspp_graphics_pacing_fx_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "ui/app_controller.h"
#include "adapters/ppsspp_adapter.h"
#include <QVBoxLayout>
#include <QVariantMap>

PpssppGraphicsPacingFxPage::PpssppGraphicsPacingFxPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PPSSPPAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Graphics" &&
            (d.subcategory == "Frame Pacing" || d.subcategory == "Post-Processing"))
            m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* PpssppGraphicsPacingFxPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void PpssppGraphicsPacingFxPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsPacingFxPage::settingFocused);
        connect(row, &Pcsx2ComboRow::focused, this, &PpssppGraphicsPacingFxPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &PpssppGraphicsPacingFxPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &PpssppGraphicsPacingFxPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* dd = findDef(key)) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    root->addWidget(new Pcsx2SectionHeader("Frame Pacing", this));
    if (auto* c = makeToggleCard("VerticalSync"))           root->addWidget(c);
    if (auto* c = makeComboCard ("FrameSkip"))              root->addWidget(c);
    if (auto* c = makeToggleCard("AutoFrameSkip"))          root->addWidget(c);
    if (auto* c = makeComboCard ("FrameRate"))              root->addWidget(c);
    if (auto* c = makeComboCard ("FrameRate2"))             root->addWidget(c);
    if (auto* c = makeToggleCard("RenderDuplicateFrames"))  root->addWidget(c);

    root->addWidget(new Pcsx2SectionHeader("Post-Processing", this));
    if (auto* c = makeComboCard("PostShader1")) root->addWidget(c);

    root->addStretch();
}

void PpssppGraphicsPacingFxPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
}

void PpssppGraphicsPacingFxPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

- [ ] **Step 3: Register in CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_pacing_fx_page.cpp
```
Add to HEADERS:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_pacing_fx_page.h
```

- [ ] **Step 4: Build**

Run: `cmake --build cpp/build --target RetroNest`
Expected: builds.

- [ ] **Step 5: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_pacing_fx_page.h \
        cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_pacing_fx_page.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: Graphics → Pacing & FX sub-page"
```

---

### Task 13: Graphics page (sub-tab container) and routing

**Files:**
- Create: `cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_page.{h,cpp}`
- Modify: `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp`
- Modify: `cpp/CMakeLists.txt`

This page wraps the four sub-pages in a `Pcsx2GraphicsSubTabBar` + `QStackedWidget` and is what the Graphics card on the hub opens.

- [ ] **Step 1: Write the header**

`cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_page.h`:

```cpp
#pragma once
#include <QWidget>
#include "core/setting_def.h"

class PpssppSettingsDialog;
class Pcsx2GraphicsSubTabBar;
class QStackedWidget;

class PpssppGraphicsPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppGraphicsPage(PpssppSettingsDialog* dialog);
    ~PpssppGraphicsPage() override;

signals:
    void settingFocused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private slots:
    void onSubTabActivated(int index);

private:
    void focusFirstSettingOnCurrentTab();

    PpssppSettingsDialog* m_dialog;
    Pcsx2GraphicsSubTabBar* m_tabBar = nullptr;
    QStackedWidget* m_stack = nullptr;
};
```

- [ ] **Step 2: Write the .cpp**

```cpp
#include "ppsspp_graphics_page.h"
#include "ppsspp_graphics_rendering_page.h"
#include "ppsspp_graphics_performance_page.h"
#include "ppsspp_graphics_textures_page.h"
#include "ppsspp_graphics_pacing_fx_page.h"
#include "../ppsspp_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_graphics_sub_tab_bar.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_toggle.h"
#include "ui/app_controller.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QApplication>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QTimer>

PpssppGraphicsPage::PpssppGraphicsPage(PpssppSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 14, 24, 8);
    root->setSpacing(12);

    auto* back = new QPushButton(QString::fromUtf8("\xE2\x86\x90 Back"), this);
    back->setCursor(Qt::PointingHandCursor);
    back->setFocusPolicy(Qt::NoFocus);
    back->setStyleSheet(
        "QPushButton { background:transparent; color:#f2efe8; border:none;"
        " font-size:14px; padding:4px 0; }"
        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &PpssppSettingsDialog::popPage);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(back, 0, Qt::AlignLeft);
    topRow->addStretch();
    root->addLayout(topRow);

    m_tabBar = new Pcsx2GraphicsSubTabBar(this);
    m_tabBar->setFocusPolicy(Qt::NoFocus);
    m_tabBar->addTab(QStringLiteral("\U0001F3A8"), "Rendering");
    m_tabBar->addTab(QStringLiteral("⚡"),     "Performance");
    m_tabBar->addTab(QStringLiteral("\U0001F9F1"), "Textures");
    m_tabBar->addTab(QStringLiteral("✨"),     "Pacing & FX");
    connect(m_tabBar, &Pcsx2GraphicsSubTabBar::tabActivated,
            this, &PpssppGraphicsPage::onSubTabActivated);
    root->addWidget(m_tabBar, 0, Qt::AlignLeft);

    m_stack = new QStackedWidget(this);

    auto* rendering = new PpssppGraphicsRenderingPage(m_dialog);
    connect(rendering, &PpssppGraphicsRenderingPage::settingFocused,
            this, &PpssppGraphicsPage::settingFocused);
    m_stack->addWidget(rendering);

    auto* performance = new PpssppGraphicsPerformancePage(m_dialog);
    connect(performance, &PpssppGraphicsPerformancePage::settingFocused,
            this, &PpssppGraphicsPage::settingFocused);
    m_stack->addWidget(performance);

    auto* textures = new PpssppGraphicsTexturesPage(m_dialog);
    connect(textures, &PpssppGraphicsTexturesPage::settingFocused,
            this, &PpssppGraphicsPage::settingFocused);
    m_stack->addWidget(textures);

    auto* pacing = new PpssppGraphicsPacingFxPage(m_dialog);
    connect(pacing, &PpssppGraphicsPacingFxPage::settingFocused,
            this, &PpssppGraphicsPage::settingFocused);
    m_stack->addWidget(pacing);

    root->addWidget(m_stack, 1);

    m_tabBar->setCurrentIndex(0);
    m_stack->setCurrentIndex(0);

    QTimer::singleShot(0, this, [this]{ focusFirstSettingOnCurrentTab(); });

    qApp->installEventFilter(this);
}

PpssppGraphicsPage::~PpssppGraphicsPage() {
    qApp->removeEventFilter(this);
}

void PpssppGraphicsPage::onSubTabActivated(int index) {
    m_stack->setCurrentIndex(index);
    m_dialog->clearFocusedSetting();
    focusFirstSettingOnCurrentTab();
}

bool PpssppGraphicsPage::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        QWidget* current = QApplication::focusWidget();
        if (!current || !isAncestorOf(current))
            return QWidget::eventFilter(obj, e);

        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            const int count = m_tabBar->tabCount();
            if (count < 2) return QWidget::eventFilter(obj, e);
            int next = m_tabBar->currentIndex();
            if (ke->key() == Qt::Key_Backtab || (ke->modifiers() & Qt::ShiftModifier))
                next = (next - 1 + count) % count;
            else
                next = (next + 1) % count;
            m_tabBar->setCurrentIndex(next);
            return true;
        }
    }
    return QWidget::eventFilter(obj, e);
}

void PpssppGraphicsPage::focusFirstSettingOnCurrentTab() {
    QWidget* page = m_stack->currentWidget();
    if (!page) return;
    for (QWidget* w : page->findChildren<QWidget*>()) {
        if (!w->isVisible()) continue;
        if (w->focusPolicy() == Qt::NoFocus) continue;
        if (qobject_cast<QComboBox*>(w)    ||
            qobject_cast<QSlider*>(w)      ||
            qobject_cast<Pcsx2Toggle*>(w)  ||
            (qobject_cast<Pcsx2Card*>(w) && w->focusPolicy() != Qt::NoFocus)) {
            w->setFocus(Qt::TabFocusReason);
            return;
        }
    }
}
```

- [ ] **Step 3: Wire route in PpssppSettingsDialog**

In `cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp` add include:

```cpp
#include "pages/ppsspp_graphics_page.h"
```

Add the Graphics branch in `onCategoryActivated()`:

```cpp
    if (category == "Graphics") {
        auto* page = new PpssppGraphicsPage(this);
        connect(page, &PpssppGraphicsPage::settingFocused, this, &PpssppSettingsDialog::setFocusedSetting);
        pushPage(page, /*hasSubTabs=*/true);
        return;
    }
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add to SOURCES:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_page.cpp
```
Add to HEADERS:
```
    src/ui/settings/ppsspp/pages/ppsspp_graphics_page.h
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build cpp/build --target RetroNest && open ./cpp/build/RetroNest.app`
Expected:
- Open PPSSPP settings → Graphics card.
- 4-tab sub-bar shows Rendering / Performance / Textures / Pacing & FX with the amber underline under Rendering.
- L1/R1 (mapped to Tab/Shift+Tab) cycles tabs.
- Each tab renders the right rows (Rendering=5, Performance=11, Textures=7, Pacing & FX=7).
- Description bar at the bottom updates as you focus rows.
- Toggling/changing a setting updates `ppsspp.ini`.

- [ ] **Step 6: Commit**

```sh
git add cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_page.h \
        cpp/src/ui/settings/ppsspp/pages/ppsspp_graphics_page.cpp \
        cpp/src/ui/settings/ppsspp/ppsspp_settings_dialog.cpp \
        cpp/CMakeLists.txt
git commit -m "PPSSPP settings: Graphics page with 4-tab sub-bar"
```

---

## Phase 5 — Final verification

### Task 14: End-to-end smoke test and test suite run

**Files:**
- None modified — verification only.

- [ ] **Step 1: Run all tests**

Run:
```sh
cmake --build cpp/build && ctest --test-dir cpp/build --output-on-failure
```
Expected: every test passes, including `PPSSPPSchema`.

- [ ] **Step 2: End-to-end smoke**

Run: `open ./cpp/build/RetroNest.app`

Walk through every category and confirm the page works:

1. Hub: 4 cards visible, "Open Native Settings" works.
2. Emulation card → 5 settings render, change one, back-out, reopen, value persisted.
3. Audio card → 4 section headers, 12 settings render, slider changes save to ppsspp.ini.
4. Overlay card → 3 bitmask toggles share `iShowStatusFlags`, plus Debug Overlay combo. Toggling FPS counter then Speed counter results in `iShowStatusFlags = 6` (bits 2 + 4) — open ppsspp.ini under `{root}/emulators/ppsspp/PSP/SYSTEM/` to verify.
5. Graphics card → 4 sub-tabs visible, L1/R1 cycles tabs, Backend dropdown shows "Vulkan" / "OpenGL" and saves as e.g. `3 (VULKAN)` per the schema's translated combo values.
6. Description bar updates per focused row (title + tooltip + recommended pill where applicable).
7. Hints at the bottom show: hub = "Navigate / Select / Close"; flat page = "Navigate / Select / Back"; Graphics page = "Navigate / Select / Switch Tab / Back".
8. Escape / B closes the dialog from the hub; from a sub-page, pops back one level.

- [ ] **Step 3: Confirm PCSX2 and DuckStation dialogs are unchanged**

Open Settings for PCSX2 and DuckStation; confirm their dialogs render identically to before this work (no regressions from any incidental edits).

- [ ] **Step 4: If any verification step failed, fix it before completing**

If a setting doesn't load/save, the most likely issue is a wrong `key` or `section` string in the page's `make*Card("…")` calls — cross-reference against `cpp/src/adapters/ppsspp_adapter.cpp`. If a sub-tab is empty, the `subcategory` filter in that page's constructor isn't matching the schema (capitalization, spacing).

- [ ] **Step 5: Final commit (only if you needed to fix anything in step 4)**

If no fixes were needed, no commit. If yes:

```sh
git add <files>
git commit -m "PPSSPP settings: smoke-test fixups"
```

---

## Out of scope (intentionally not in this plan)

- Refactoring `pcsx2_*` widgets into a shared/neutral tree (deferred per spec).
- Aspect-ratio / OSD preview widgets for PPSSPP (deferred per spec — PPSSPP's overlay is text-only and ratio is fixed).
- Per-game settings overrides.
- Any change to PCSX2 or DuckStation UIs.
- Removing the `EmulatorSettingsPage` fall-through path — kept for future emulators that haven't been redesigned yet.
