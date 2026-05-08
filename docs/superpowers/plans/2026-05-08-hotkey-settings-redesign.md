# Hotkey Settings Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the hotkey settings dialog on `EmulatorSettingsDialogBase` + `SettingsDialogTheme` so it shares chrome and palette with the settings dialogs and the controller-mapping page, replacing the bespoke 534-line `HotkeySettingsPage`.

**Architecture:** Three new files (`hotkey_settings_dialog`, `generic_hotkey_page`, `hotkey_binding_row`) replace `hotkey_settings_page.{h,cpp}`. `SettingsDescriptionBar` gains a small `setHotkey()` method. The capture state machine, multi-binding format helpers, and `AppController` API are reused verbatim. No adapter or `HotkeyDef` changes.

**Tech Stack:** C++17, Qt6 Widgets, Qt::Test (per `cpp/tests/test_*.cpp` patterns), CMake.

**Spec:** `docs/superpowers/specs/2026-05-08-hotkey-settings-redesign-design.md`

---

## File Map

**New files (sources):**
- `cpp/src/ui/settings/widgets/hotkey_binding_row.cpp` — single binding row widget (label + button + signals).
- `cpp/src/ui/settings/widgets/hotkey_binding_row.h`
- `cpp/src/ui/settings/generic_hotkey_page.cpp` — schema-driven page that consumes `hotkeyBindingDefs()`, owns the capture state machine.
- `cpp/src/ui/settings/generic_hotkey_page.h`
- `cpp/src/ui/settings/hotkey_settings_dialog.cpp` — dialog extending `EmulatorSettingsDialogBase`, wires page signals to description bar.
- `cpp/src/ui/settings/hotkey_settings_dialog.h`
- `cpp/tests/test_hotkey_binding_row.cpp` — Qt::Test smoke test for the row widget.
- `cpp/tests/test_generic_hotkey_page.cpp` — Qt::Test smoke test for the page.

**Files modified:**
- `cpp/src/ui/settings/widgets/settings_description_bar.h` — add `setHotkey()` method declaration; forward-declare `HotkeyDef`.
- `cpp/src/ui/settings/widgets/settings_description_bar.cpp` — implement `setHotkey()`.
- `cpp/tests/test_settings_description_bar.cpp` — add a test for `setHotkey()`.
- `cpp/src/ui/app_controller.cpp:13,464` — swap `#include` and dialog instantiation.
- `cpp/CMakeLists.txt` — drop `hotkey_settings_page.{h,cpp}` from `SOURCES`/`HEADERS`, add the three new pairs; add the two new test executables.

**Files deleted:**
- `cpp/src/ui/settings/hotkey_settings_page.h`
- `cpp/src/ui/settings/hotkey_settings_page.cpp`

**Files unchanged (verify these are NOT touched):**
- `cpp/src/core/binding_def.h` (`HotkeyDef`).
- All adapter files (`pcsx2_adapter.cpp`, `duckstation_adapter.cpp`, `ppsspp_adapter.cpp`, `dolphin_adapter.cpp`).
- `cpp/src/core/sdl_input_manager.{h,cpp}` (capture flow).
- `cpp/src/ui/settings/binding_widget_common.h` (`BindBtn`).
- `cpp/src/ui/settings/binding_display.h` (`displayBinding`, `deviceIndexFromBinding`).
- `cpp/src/services/config_service.{h,cpp}` (hotkey persistence).

---

## Cross-Task Conventions

- **Build & run:** `cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build`. The build is incremental; only the first task should expect a configure step.
- **Run a single test:** `./cpp/build/test_hotkey_binding_row` (Qt::Test executables are at `cpp/build/test_<name>`). Or `cd cpp/build && ctest --output-on-failure -R HotkeyBindingRow`.
- **Manual smoke test of the dialog:** `open ./cpp/build/RetroNest.app`, navigate to a game, open the in-game menu, choose "Hotkey Settings."
- **Commit style** (matches `git log`): lowercase scope prefix, em-dash separator, brief imperative summary. Example: `hotkey: extract row widget into shared widgets/`.
- **Test placement:** all tests live in `cpp/tests/`. Each new test executable needs an `add_executable(...)` + `add_test(...)` block in `cpp/CMakeLists.txt` mirroring the `test_controller_bindings_view` block.

---

## Task 1: Add `setHotkey()` to `SettingsDescriptionBar`

**Why first:** the dialog needs this method to drive the description bar from the page's `bindingFocused` signal. Adding it first means the later dialog code compiles against a real API rather than a placeholder.

**Files:**
- Modify: `cpp/src/ui/settings/widgets/settings_description_bar.h`
- Modify: `cpp/src/ui/settings/widgets/settings_description_bar.cpp`
- Modify: `cpp/tests/test_settings_description_bar.cpp` (add one test slot).

- [ ] **Step 1.1: Read the existing description-bar test file to learn the test fixture style**

Run: `cat cpp/tests/test_settings_description_bar.cpp`
Note the `initTestCase` setup, the `QApplication` requirement, and how `descText()` / `recommendedText()` are used as observation hooks.

- [ ] **Step 1.2: Write the failing test for `setHotkey()`**

Append a new test slot to `cpp/tests/test_settings_description_bar.cpp` after the existing slots:

```cpp
void setHotkey_writesLabelAndCurrentBinding() {
    SettingsDescriptionBar bar;

    HotkeyDef def;
    def.label = "Toggle Turbo";
    def.group = "Speed Control";
    def.section = "Hotkeys";
    def.key = "ToggleTurbo";

    bar.setHotkey(def, "Period");

    QCOMPARE(bar.descText(),
             QStringLiteral("Toggle Turbo  —  Currently: Period"));
    // Recommended badge is hidden for hotkeys (no recommended value).
    QVERIFY(bar.recommendedText().isEmpty()
            || !bar.findChild<QLabel*>("SettingsDescRecommended")->isVisible());
}

void setHotkey_emptyValueShowsNotBound() {
    SettingsDescriptionBar bar;

    HotkeyDef def;
    def.label = "Frame Advance";

    bar.setHotkey(def, QString());

    QCOMPARE(bar.descText(),
             QStringLiteral("Frame Advance  —  Currently: Not bound"));
}
```

Add `#include "core/binding_def.h"` near the top of the test file if not already present.

- [ ] **Step 1.3: Run the test to verify it fails**

Run:
```bash
cd cpp && cmake --build build --target test_settings_description_bar 2>&1 | tail -30
```
Expected: compile error (`'setHotkey' is not a member of 'SettingsDescriptionBar'`). This is the failing-test signal.

- [ ] **Step 1.4: Add the declaration to the header**

Edit `cpp/src/ui/settings/widgets/settings_description_bar.h`:

After the existing `#include "core/setting_def.h"` line, add a forward declaration:

```cpp
struct HotkeyDef;
```

In the public section, after the `setSetting` / `clear` / `setDescriptionVisible` block, add:

```cpp
// Hotkey variant: writes "<Label> — Currently: <value or 'Not bound'>"
// to the primary text and hides the recommended badge.
void setHotkey(const HotkeyDef& def, const QString& currentDisplay);
```

- [ ] **Step 1.5: Implement the method**

Edit `cpp/src/ui/settings/widgets/settings_description_bar.cpp`:

Add to the includes (after the existing `#include "core/sdl_input_manager.h"`):

```cpp
#include "core/binding_def.h"
```

After the existing `setSetting()` implementation, add:

```cpp
void SettingsDescriptionBar::setHotkey(const HotkeyDef& def, const QString& currentDisplay) {
    const QString shown = currentDisplay.isEmpty()
                              ? QStringLiteral("Not bound")
                              : currentDisplay;
    m_text->setText(QStringLiteral("%1  —  Currently: %2").arg(def.label, shown));
    m_rec->setVisible(false);
}
```

- [ ] **Step 1.6: Run the test to verify it passes**

Run:
```bash
cd cpp && cmake --build build --target test_settings_description_bar && ./build/test_settings_description_bar
```
Expected: all tests pass, including the two new `setHotkey_*` slots.

- [ ] **Step 1.7: Commit**

```bash
git add cpp/src/ui/settings/widgets/settings_description_bar.h \
        cpp/src/ui/settings/widgets/settings_description_bar.cpp \
        cpp/tests/test_settings_description_bar.cpp && \
git commit -m "$(cat <<'EOF'
description-bar: add setHotkey() variant for hotkey footer

Footer shows "<Label>  —  Currently: <value or 'Not bound'>" and hides
the recommended badge. Used by the upcoming HotkeySettingsDialog to
drive the description bar from the focused-binding signal.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Create `HotkeyBindingRow` widget

**Why this task:** the row is the smallest reusable unit — label + binding button. Building it stand-alone (and TDDing its signals) means the page assembly task only has to glue rows together, not invent the row type at the same time.

**Files:**
- Create: `cpp/src/ui/settings/widgets/hotkey_binding_row.h`
- Create: `cpp/src/ui/settings/widgets/hotkey_binding_row.cpp`
- Create: `cpp/tests/test_hotkey_binding_row.cpp`

- [ ] **Step 2.1: Write the failing test**

Create `cpp/tests/test_hotkey_binding_row.cpp`:

```cpp
// cpp/tests/test_hotkey_binding_row.cpp
//
// Smoke + signal tests for HotkeyBindingRow. The row is a label + a
// BindBtn; the test verifies signal emission for left-click (rebind),
// shift+left-click (append rebind), and right-click (clear), and that
// focusing the row emits `focused`.

#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QPushButton>

#include "core/binding_def.h"
#include "ui/settings/widgets/hotkey_binding_row.h"

class TestHotkeyBindingRow : public QObject {
    Q_OBJECT

private:
    HotkeyDef makeDef() const {
        HotkeyDef d;
        d.label = "Toggle Turbo";
        d.group = "Speed Control";
        d.section = "Hotkeys";
        d.key = "ToggleTurbo";
        d.defaultValue = "Keyboard/Period";
        return d;
    }

private slots:
    void constructsWithLabelAndButton() {
        HotkeyBindingRow row(makeDef());
        // Both children should be present.
        QVERIFY(row.findChild<QLabel*>() != nullptr);
        QVERIFY(row.findChild<QPushButton*>() != nullptr);
    }

    void setBindingDisplay_updatesButtonText() {
        HotkeyBindingRow row(makeDef());
        row.setBindingDisplay("Period");
        QCOMPARE(row.findChild<QPushButton*>()->text(), QStringLiteral("Period"));

        row.setBindingDisplay(QString());
        QCOMPARE(row.findChild<QPushButton*>()->text(), QStringLiteral("Not bound"));
    }

    void leftClick_emitsRebindRequested() {
        HotkeyBindingRow row(makeDef());
        QSignalSpy spy(&row, &HotkeyBindingRow::rebindRequested);

        auto* btn = row.findChild<QPushButton*>();
        QTest::mouseClick(btn, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().first().value<HotkeyDef>().key,
                 QStringLiteral("ToggleTurbo"));
    }

    void shiftLeftClick_emitsAppendRebindRequested() {
        HotkeyBindingRow row(makeDef());
        QSignalSpy spy(&row, &HotkeyBindingRow::appendRebindRequested);

        auto* btn = row.findChild<QPushButton*>();
        QTest::mouseClick(btn, Qt::LeftButton, Qt::ShiftModifier);

        QCOMPARE(spy.count(), 1);
    }

    void rightClick_emitsClearRequested() {
        HotkeyBindingRow row(makeDef());
        QSignalSpy spy(&row, &HotkeyBindingRow::clearRequested);

        auto* btn = row.findChild<QPushButton*>();
        QTest::mouseClick(btn, Qt::RightButton);

        QCOMPARE(spy.count(), 1);
    }

    void focusIn_emitsFocused() {
        HotkeyBindingRow row(makeDef());
        QSignalSpy spy(&row, &HotkeyBindingRow::focused);

        row.setFocus(Qt::TabFocusReason);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().first().value<HotkeyDef>().key,
                 QStringLiteral("ToggleTurbo"));
    }
};

QTEST_MAIN(TestHotkeyBindingRow)
#include "test_hotkey_binding_row.moc"
```

`HotkeyDef` needs `Q_DECLARE_METATYPE` for `QSignalSpy` to carry it. Confirm whether it already exists; if not, the next task adds it.

- [ ] **Step 2.2: Add `Q_DECLARE_METATYPE` for `HotkeyDef`**

Edit `cpp/src/core/binding_def.h`. After the closing `};` of `struct HotkeyDef`, add:

```cpp
#include <QMetaType>
Q_DECLARE_METATYPE(HotkeyDef)
```

(Place the `#include <QMetaType>` next to the existing `#include <QString>` at the top of the file instead, then put just the `Q_DECLARE_METATYPE(HotkeyDef)` line at the bottom.)

- [ ] **Step 2.3: Write the row header**

Create `cpp/src/ui/settings/widgets/hotkey_binding_row.h`:

```cpp
#pragma once

#include <QWidget>
#include "core/binding_def.h"

class QLabel;
class BindBtn;

// Single hotkey binding row: fixed-width label on the left, BindBtn on
// the right. Emits `focused` on focus-in, plus three input signals that
// the parent page wires to the capture / clear flows.
class HotkeyBindingRow : public QWidget {
    Q_OBJECT
public:
    explicit HotkeyBindingRow(const HotkeyDef& def, QWidget* parent = nullptr);

    const HotkeyDef& def() const { return m_def; }

    // Update the button text. Empty value renders as "Not bound".
    void setBindingDisplay(const QString& displayText);

    // Toggle the "currently capturing" visual style.
    void setCapturing(bool capturing);

signals:
    void focused(HotkeyDef def);
    void rebindRequested(HotkeyDef def);
    void appendRebindRequested(HotkeyDef def);
    void clearRequested(HotkeyDef def);

protected:
    void focusInEvent(QFocusEvent* e) override;

private:
    HotkeyDef m_def;
    QLabel* m_label = nullptr;
    BindBtn* m_button = nullptr;
};
```

- [ ] **Step 2.4: Write the row implementation**

Create `cpp/src/ui/settings/widgets/hotkey_binding_row.cpp`:

```cpp
#include "hotkey_binding_row.h"
#include "ui/settings/binding_widget_common.h"
#include "ui/settings/settings_dialog_theme.h"

#include <QHBoxLayout>
#include <QLabel>

HotkeyBindingRow::HotkeyBindingRow(const HotkeyDef& def, QWidget* parent)
    : QWidget(parent), m_def(def) {
    setFocusPolicy(Qt::StrongFocus);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(12);

    m_label = new QLabel(def.label, this);
    m_label->setFixedWidth(220);
    m_label->setStyleSheet(QStringLiteral(
        "color:%1; font-size:13px; background:transparent;")
        .arg(SettingsDialogTheme::textPrimary().name()));
    layout->addWidget(m_label);

    m_button = new BindBtn(this);
    m_button->setFixedHeight(kBtnH);
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:6px; font-size:12px; padding:4px 8px; }"
        "QPushButton:hover { border-color:%4; }")
        .arg(SettingsDialogTheme::inputBg().name(),
             SettingsDialogTheme::textPrimary().name(),
             SettingsDialogTheme::cardBorder().name(),
             SettingsDialogTheme::accent().name()));
    m_button->setText(QStringLiteral("Not bound"));

    connect(m_button, &QPushButton::clicked, this,
            [this]{ emit rebindRequested(m_def); });
    m_button->onShiftClick = [this]{ emit appendRebindRequested(m_def); };
    m_button->onRightClick = [this]{ emit clearRequested(m_def); };

    layout->addWidget(m_button, 1);
}

void HotkeyBindingRow::setBindingDisplay(const QString& displayText) {
    m_button->setText(displayText.isEmpty() ? QStringLiteral("Not bound")
                                            : displayText);
    // Tooltip preserves the three-line input guide users were trained on by
    // the legacy hotkey page (spec §6).
    const QString tip = (displayText.isEmpty() ? QStringLiteral("Not bound")
                                                : displayText)
        + QStringLiteral("\n\nLeft click to assign a new button\n"
                          "Shift + left click for additional bindings\n"
                          "Right click to clear binding");
    m_button->setToolTip(tip);
}

void HotkeyBindingRow::setCapturing(bool capturing) {
    if (capturing) {
        m_button->setStyleSheet(kCapturingStyle);
        m_button->setText(QStringLiteral("Press a button or key…"));
    } else {
        m_button->setStyleSheet(QStringLiteral(
            "QPushButton { background:%1; color:%2; border:1px solid %3;"
            "  border-radius:6px; font-size:12px; padding:4px 8px; }"
            "QPushButton:hover { border-color:%4; }")
            .arg(SettingsDialogTheme::inputBg().name(),
                 SettingsDialogTheme::textPrimary().name(),
                 SettingsDialogTheme::cardBorder().name(),
                 SettingsDialogTheme::accent().name()));
    }
}

void HotkeyBindingRow::focusInEvent(QFocusEvent* e) {
    QWidget::focusInEvent(e);
    emit focused(m_def);
}
```

- [ ] **Step 2.5: Wire row + test into the build**

Edit `cpp/CMakeLists.txt`:

In the `set(SOURCES ...)` block, add (alphabetised within `widgets/`, near `settings_description_bar.cpp`):

```cmake
    src/ui/settings/widgets/hotkey_binding_row.cpp
```

In the `set(HEADERS ...)` block, add (near `settings_description_bar.h`):

```cmake
    src/ui/settings/widgets/hotkey_binding_row.h
```

Find the existing `add_executable(test_controller_bindings_view ...)` block. Below it, add:

```cmake
add_executable(test_hotkey_binding_row
    tests/test_hotkey_binding_row.cpp
    src/ui/settings/widgets/hotkey_binding_row.cpp
    # Q_DECLARE_METATYPE(HotkeyDef) lives in core/binding_def.h — no extra source.
)
set_target_properties(test_hotkey_binding_row PROPERTIES AUTOMOC ON)
target_include_directories(test_hotkey_binding_row PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_hotkey_binding_row PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
add_test(NAME HotkeyBindingRow COMMAND test_hotkey_binding_row)
```

- [ ] **Step 2.6: Build and run the new test**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" \
  && cmake --build build --target test_hotkey_binding_row \
  && ./build/test_hotkey_binding_row
```
Expected: all 6 test slots pass.

If `Q_DECLARE_METATYPE(HotkeyDef)` is missing, the test fails to compile with an error mentioning `qMetaTypeId`. Confirm Step 2.2 was applied.

- [ ] **Step 2.7: Commit**

```bash
git add cpp/src/ui/settings/widgets/hotkey_binding_row.h \
        cpp/src/ui/settings/widgets/hotkey_binding_row.cpp \
        cpp/src/core/binding_def.h \
        cpp/tests/test_hotkey_binding_row.cpp \
        cpp/CMakeLists.txt && \
git commit -m "$(cat <<'EOF'
hotkey: add HotkeyBindingRow widget

Single label + BindBtn row used by the upcoming GenericHotkeyPage.
Emits focused/rebindRequested/appendRebindRequested/clearRequested.
Styled with SettingsDialogTheme colours so it matches the rest of
the redesigned settings surfaces.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Create `GenericHotkeyPage`

**Why this task:** the page is the schema-driven container — it reads `hotkeyBindingDefs()`, lays out section-grouped rows, and owns the capture state machine. Once it exists, the dialog wrapper in Task 4 is mostly glue.

**Files:**
- Create: `cpp/src/ui/settings/generic_hotkey_page.h`
- Create: `cpp/src/ui/settings/generic_hotkey_page.cpp`
- Create: `cpp/tests/test_generic_hotkey_page.cpp`

- [ ] **Step 3.1: Read the existing capture-flow source**

Run: `cat cpp/src/ui/settings/hotkey_settings_page.cpp`

Note specifically the `startCapture` / `stopCapture` / `onBindingCaptured` / `onKeyboardCaptured` / `finishCapture` / `eventFilter` flow and the timer countdown (`m_captureTimer`, `m_captureCountdown`). Also note `displayMultiBinding` (file-local helper) and the `isModifierKey` predicate. These all move into the new page.

- [ ] **Step 3.2: Write the page header**

Create `cpp/src/ui/settings/generic_hotkey_page.h`:

```cpp
#pragma once

#include <QWidget>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "core/binding_def.h"

class SdlInputManager;
class AppController;
class HotkeyBindingRow;

// Schema-driven hotkey page. Reads `AppController::hotkeyBindings(emuId)`
// once at construction, groups entries by `HotkeyDef::group` (preserving
// adapter declaration order), and renders one section header + one
// HotkeyBindingRow per entry. Owns the capture state machine that was
// previously embedded in HotkeySettingsPage.
class GenericHotkeyPage : public QWidget {
    Q_OBJECT
public:
    GenericHotkeyPage(SdlInputManager* inputManager,
                      AppController* appController,
                      const QString& emuId,
                      QWidget* parent = nullptr);

    bool isEmpty() const { return m_entries.isEmpty(); }

    // Public action API — called from the hosting dialog's face-button
    // shortcuts. All operate on the currently focused row, no-op when
    // there is no focus.
    void rebindFocused();
    void appendRebindFocused();
    void clearFocused();
    void restoreDefaults();

signals:
    // Emitted when a row gains focus. `currentDisplay` is the formatted
    // display string ("Period", "SDL-0 R1 + SDL-0 Circle", or empty).
    void bindingFocused(HotkeyDef def, QString currentDisplay);
    void bindingFocusCleared();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void buildLayout();
    void loadBindings();

    void onRowFocused(const HotkeyDef& def);
    void startCapture(const HotkeyDef& def, bool append);
    void stopCapture(bool save);
    void onBindingCaptured(int deviceIndex, const QString& element,
                            bool isAxis, bool positive);
    void onKeyboardCaptured(const QString& keyString);
    void finishCapture(const QString& formatted);

    QString currentDisplayFor(const QString& iniKey) const;

    SdlInputManager* m_inputManager;
    AppController* m_appController;
    QString m_emuId;

    QVector<HotkeyDef> m_entries;             // adapter declaration order
    QHash<QString, QString> m_currentValues;  // INI key -> raw stored value
    QHash<QString, HotkeyBindingRow*> m_rowByKey;

    HotkeyBindingRow* m_focusedRow = nullptr;

    QString m_capturingKey;
    bool m_appendMode = false;
    QTimer* m_captureTimer = nullptr;
    int m_captureCountdown = 0;
    QStringList m_capturedBindings;
};
```

- [ ] **Step 3.3: Write the page implementation**

Create `cpp/src/ui/settings/generic_hotkey_page.cpp`:

```cpp
#include "generic_hotkey_page.h"

#include "core/sdl_input_manager.h"
#include "ui/app_controller.h"
#include "ui/settings/binding_display.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/hotkey_binding_row.h"
#include "ui/settings/widgets/settings_section_header.h"

#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

// Format multi-part binding for display.
// INI stores "SDL-0/Back & SDL-0/RightShoulder", display as "SDL-0 Create + SDL-0 R1".
QString displayMultiBinding(const QString& raw, SdlInputManager* inputMgr) {
    if (raw.isEmpty()) return {};
    const QStringList parts = raw.split(QStringLiteral(" & "), Qt::SkipEmptyParts);
    QStringList displayed;
    for (const auto& part : parts) {
        const QString trimmed = part.trimmed();
        auto detailedType = SdlInputManager::TypeStandard;
        const int devIdx = deviceIndexFromBinding(trimmed);
        if (devIdx >= 0 && inputMgr)
            detailedType = inputMgr->detailedControllerTypeForDevice(devIdx);
        const QString d = displayBinding(trimmed, detailedType);
        if (!d.isEmpty()) displayed.append(d);
    }
    return displayed.join(QStringLiteral(" + "));
}

bool isModifierKey(int key) {
    return key == Qt::Key_Shift || key == Qt::Key_Control
        || key == Qt::Key_Alt   || key == Qt::Key_Meta;
}

} // namespace

GenericHotkeyPage::GenericHotkeyPage(SdlInputManager* inputManager,
                                     AppController* appController,
                                     const QString& emuId,
                                     QWidget* parent)
    : QWidget(parent),
      m_inputManager(inputManager),
      m_appController(appController),
      m_emuId(emuId) {
    setStyleSheet(QStringLiteral("background:%1;")
                      .arg(SettingsDialogTheme::windowBg().name()));

    // Ingest hotkey data from the adapter (via AppController -> ConfigService).
    // Guarding nullptr keeps the test layer simple: tests can construct the
    // page with nullptr and assert the empty-state branch.
    const QVariantList bindings = m_appController
        ? m_appController->hotkeyBindings(m_emuId)
        : QVariantList{};
    for (const auto& v : bindings) {
        const auto map = v.toMap();
        HotkeyDef def;
        def.label = map.value(QStringLiteral("label")).toString();
        def.group = map.value(QStringLiteral("group")).toString();
        if (def.group.isEmpty()) def.group = QStringLiteral("General");
        def.section = map.value(QStringLiteral("section")).toString();
        def.key = map.value(QStringLiteral("key")).toString();
        def.defaultValue = map.value(QStringLiteral("defaultValue")).toString();
        m_entries.append(def);
        m_currentValues.insert(def.key,
                               map.value(QStringLiteral("currentValue")).toString());
    }

    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(1000);
    connect(m_captureTimer, &QTimer::timeout, this, [this]{
        if (--m_captureCountdown <= 0) stopCapture(true);
    });

    if (m_inputManager) {
        connect(m_inputManager, &SdlInputManager::bindingCaptured,
                this, &GenericHotkeyPage::onBindingCaptured);
        connect(m_inputManager, &SdlInputManager::keyboardCaptured,
                this, &GenericHotkeyPage::onKeyboardCaptured);
        connect(m_inputManager, &SdlInputManager::captureButtonReleased,
                this, [this]{
                    if (!m_capturingKey.isEmpty() && !m_capturedBindings.isEmpty())
                        stopCapture(true);
                });
    }

    buildLayout();
    loadBindings();
}

void GenericHotkeyPage::buildLayout() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    if (m_entries.isEmpty()) {
        auto* empty = new QLabel(
            QStringLiteral("This emulator does not expose hotkey bindings."), this);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QStringLiteral("color:%1; font-size:14px;")
            .arg(SettingsDialogTheme::textSecondary().name()));
        outer->addWidget(empty, 1);
        return;
    }

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background:transparent; border:0; }"
        "QScrollArea > QWidget > QWidget { background:transparent; }"));

    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 20, 24, 20);
    contentLayout->setSpacing(8);

    QString currentGroup;
    QFrame* currentCard = nullptr;
    QVBoxLayout* currentCardLayout = nullptr;

    for (const auto& def : m_entries) {
        if (def.group != currentGroup) {
            currentGroup = def.group;
            contentLayout->addSpacing(4);
            contentLayout->addWidget(new SettingsSectionHeader(currentGroup, content));
            currentCard = new QFrame(content);
            currentCard->setObjectName(QStringLiteral("SettingsCard"));
            currentCard->setStyleSheet(SettingsDialogTheme::cardQss());
            currentCardLayout = new QVBoxLayout(currentCard);
            currentCardLayout->setContentsMargins(8, 8, 8, 8);
            currentCardLayout->setSpacing(2);
            contentLayout->addWidget(currentCard);
        }

        auto* row = new HotkeyBindingRow(def, currentCard);
        connect(row, &HotkeyBindingRow::focused,
                this, &GenericHotkeyPage::onRowFocused);
        connect(row, &HotkeyBindingRow::rebindRequested,
                this, [this](const HotkeyDef& d){ startCapture(d, false); });
        connect(row, &HotkeyBindingRow::appendRebindRequested,
                this, [this](const HotkeyDef& d){ startCapture(d, true); });
        connect(row, &HotkeyBindingRow::clearRequested,
                this, [this](const HotkeyDef& d){
                    m_appController->clearHotkey(m_emuId, d.section, d.key);
                    m_currentValues[d.key].clear();
                    if (auto it = m_rowByKey.constFind(d.key); it != m_rowByKey.constEnd())
                        (*it)->setBindingDisplay(QString());
                });
        currentCardLayout->addWidget(row);
        m_rowByKey.insert(def.key, row);
    }

    contentLayout->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
}

void GenericHotkeyPage::loadBindings() {
    if (!m_appController) return;
    const QVariantList bindings = m_appController->hotkeyBindings(m_emuId);
    for (const auto& v : bindings) {
        const auto map = v.toMap();
        const QString key = map.value(QStringLiteral("key")).toString();
        m_currentValues[key] = map.value(QStringLiteral("currentValue")).toString();
        if (auto it = m_rowByKey.constFind(key); it != m_rowByKey.constEnd())
            (*it)->setBindingDisplay(currentDisplayFor(key));
    }
}

void GenericHotkeyPage::onRowFocused(const HotkeyDef& def) {
    m_focusedRow = m_rowByKey.value(def.key, nullptr);
    emit bindingFocused(def, currentDisplayFor(def.key));
}

void GenericHotkeyPage::rebindFocused() {
    if (m_focusedRow) startCapture(m_focusedRow->def(), false);
}

void GenericHotkeyPage::appendRebindFocused() {
    if (m_focusedRow) startCapture(m_focusedRow->def(), true);
}

void GenericHotkeyPage::clearFocused() {
    if (!m_focusedRow || !m_appController) return;
    const HotkeyDef d = m_focusedRow->def();
    m_appController->clearHotkey(m_emuId, d.section, d.key);
    m_currentValues[d.key].clear();
    m_focusedRow->setBindingDisplay(QString());
    emit bindingFocused(d, QString());
}

void GenericHotkeyPage::restoreDefaults() {
    if (!m_appController) return;
    m_appController->resetHotkeys(m_emuId);
    loadBindings();
    if (m_focusedRow) {
        const HotkeyDef d = m_focusedRow->def();
        emit bindingFocused(d, currentDisplayFor(d.key));
    }
}

QString GenericHotkeyPage::currentDisplayFor(const QString& iniKey) const {
    return displayMultiBinding(m_currentValues.value(iniKey), m_inputManager);
}

void GenericHotkeyPage::startCapture(const HotkeyDef& def, bool append) {
    if (!m_capturingKey.isEmpty()) stopCapture(false);

    m_capturingKey = def.key;
    m_appendMode = append;
    m_capturedBindings.clear();
    m_captureCountdown = 5;

    if (auto it = m_rowByKey.constFind(def.key); it != m_rowByKey.constEnd())
        (*it)->setCapturing(true);

    if (m_inputManager) m_inputManager->startCapture();
    if (auto* w = window()) w->installEventFilter(this);
    m_captureTimer->start();
}

void GenericHotkeyPage::stopCapture(bool save) {
    m_captureTimer->stop();
    if (m_inputManager) m_inputManager->stopCapture();
    if (auto* w = window()) w->removeEventFilter(this);

    const QString key = m_capturingKey;
    m_capturingKey.clear();

    if (auto it = m_rowByKey.constFind(key); it != m_rowByKey.constEnd())
        (*it)->setCapturing(false);

    if (save && !m_capturedBindings.isEmpty()) {
        finishCapture(m_capturedBindings.join(QStringLiteral(" & ")));
    } else {
        loadBindings();  // refresh display from stored value
    }

    m_capturedBindings.clear();
}

void GenericHotkeyPage::onBindingCaptured(int deviceIndex, const QString& element,
                                          bool isAxis, bool positive) {
    if (m_capturingKey.isEmpty()) return;

    QString formatted;
    if (isAxis) {
        formatted = QStringLiteral("SDL-%1/%2%3")
                        .arg(deviceIndex)
                        .arg(positive ? '+' : '-')
                        .arg(element);
    } else {
        formatted = QStringLiteral("SDL-%1/%2").arg(deviceIndex).arg(element);
    }

    if (m_appendMode) m_capturedBindings.append(formatted);
    else              m_capturedBindings = QStringList{formatted};

    if (!m_appendMode) stopCapture(true);
}

void GenericHotkeyPage::onKeyboardCaptured(const QString& keyString) {
    if (m_capturingKey.isEmpty()) return;

    if (m_appendMode) m_capturedBindings.append(keyString);
    else              m_capturedBindings = QStringList{keyString};

    if (!m_appendMode) stopCapture(true);
}

void GenericHotkeyPage::finishCapture(const QString& formatted) {
    if (!m_focusedRow || !m_appController) return;
    const HotkeyDef d = m_focusedRow->def();
    m_appController->saveHotkey(m_emuId, d.section, d.key, formatted);
    m_currentValues[d.key] = formatted;
    m_focusedRow->setBindingDisplay(currentDisplayFor(d.key));
    emit bindingFocused(d, currentDisplayFor(d.key));
}

bool GenericHotkeyPage::eventFilter(QObject* obj, QEvent* event) {
    if (m_capturingKey.isEmpty()) return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (isModifierKey(ke->key())) return false;  // wait for the real key
        if (ke->key() == Qt::Key_Escape) {
            stopCapture(false);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
```

- [ ] **Step 3.4: Verify the `AppController` API exists exactly as used**

Run:
```bash
grep -n "saveHotkey\|clearHotkey\|resetHotkeys\|hotkeyBindings" cpp/src/ui/app_controller.h
```

Expected output (matching the page's calls):
```
QVariantList hotkeyBindings(const QString& emuId) const;
void saveHotkey(const QString& emuId, const QString& section, const QString& key, const QString& value);
void clearHotkey(const QString& emuId, const QString& section, const QString& key);
void resetHotkeys(const QString& emuId);
```

If any signature differs, update the page's calls — do NOT add a new overload.

- [ ] **Step 3.5: Verify the `SdlInputManager` capture API**

Run:
```bash
grep -n "void startCapture\|void stopCapture\|bindingCaptured\|keyboardCaptured\|captureButtonReleased" cpp/src/core/sdl_input_manager.h
```

Confirm: `startCapture()`, `stopCapture()` are public methods; `bindingCaptured(int,QString,bool,bool)`, `keyboardCaptured(QString)`, `captureButtonReleased()` are signals.

If any signature differs, fix the connect / call site in the page.

- [ ] **Step 3.6: Write the page smoke test**

Create `cpp/tests/test_generic_hotkey_page.cpp`:

```cpp
// cpp/tests/test_generic_hotkey_page.cpp
//
// Smoke tests for GenericHotkeyPage. Uses nullptr for AppController —
// the page guards the nullptr and renders the empty-state branch, so
// these tests verify chrome/structure rather than data binding.
// (Data-driven verification lives in the manual smoke pass per the
// implementation plan.)

#include <QtTest>
#include <QApplication>
#include <QSignalSpy>

#include "ui/settings/generic_hotkey_page.h"
#include "ui/settings/widgets/hotkey_binding_row.h"

class TestGenericHotkeyPage : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        qputenv("XDG_CONFIG_HOME", QByteArrayLiteral("/tmp/retronest-test-config"));
    }

    // With nullptr AppController the page falls through to the empty branch
    // (see Step 3.3). No crash, no rows.
    void nullAppControllerProducesEmptyPage() {
        GenericHotkeyPage page(/*inputManager=*/nullptr,
                                /*appController=*/nullptr,
                                /*emuId=*/"pcsx2");
        page.resize(900, 720);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        QVERIFY(page.isEmpty());
        QCOMPARE(page.findChildren<HotkeyBindingRow*>().size(), 0);
    }

    // Public action API must be no-ops when there is no focus / no AppController.
    void publicActionsNoopWithoutFocus() {
        GenericHotkeyPage page(nullptr, nullptr, "pcsx2");
        page.rebindFocused();
        page.appendRebindFocused();
        page.clearFocused();
        page.restoreDefaults();
        QVERIFY(true);  // surviving means no crash
    }
};

QTEST_MAIN(TestGenericHotkeyPage)
#include "test_generic_hotkey_page.moc"
```

> **Why the test stays at smoke level:** `GenericHotkeyPage` reads its data through `AppController::hotkeyBindings()`, which goes through `ConfigService`, which reads the live INI files under the user's RetroNest root. Constructing a real `AppController` in a unit test would require a populated config tree — far more setup than the existing `test_controller_bindings_view` does. Following its pattern, the unit test asserts only the structural / no-crash properties; full data-driven verification is the manual pass in Task 7.

- [ ] **Step 3.7: Wire the page + test into the build**

Edit `cpp/CMakeLists.txt`:

In `set(SOURCES ...)`, add (alphabetised, near `generic_settings_page.cpp`):
```cmake
    src/ui/settings/generic_hotkey_page.cpp
```

In `set(HEADERS ...)`, add a matching line:
```cmake
    src/ui/settings/generic_hotkey_page.h
```

Below the `add_executable(test_hotkey_binding_row ...)` block, add:

```cmake
add_executable(test_generic_hotkey_page
    tests/test_generic_hotkey_page.cpp
    src/ui/settings/generic_hotkey_page.cpp
    src/ui/settings/widgets/hotkey_binding_row.cpp
    src/ui/settings/widgets/settings_section_header.cpp
)
set_target_properties(test_generic_hotkey_page PROPERTIES AUTOMOC ON)
target_include_directories(test_generic_hotkey_page PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_generic_hotkey_page PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
add_test(NAME GenericHotkeyPage COMMAND test_generic_hotkey_page)
```

> Because the test passes nullptr for `AppController` and `SdlInputManager`, no stub is needed. The page's nullptr guards (added in Step 3.3) take it down the empty-state branch.

- [ ] **Step 3.8: Build & run the page test**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" \
  && cmake --build build --target test_generic_hotkey_page \
  && ./build/test_generic_hotkey_page
```
Expected: 2 test slots pass (`nullAppControllerProducesEmptyPage`, `publicActionsNoopWithoutFocus`).

- [ ] **Step 3.9: Commit**

```bash
git add cpp/src/ui/settings/generic_hotkey_page.h \
        cpp/src/ui/settings/generic_hotkey_page.cpp \
        cpp/tests/test_generic_hotkey_page.cpp \
        cpp/CMakeLists.txt && \
git commit -m "$(cat <<'EOF'
hotkey: add GenericHotkeyPage — schema-driven content page

Reads hotkeyBindingDefs() via AppController, groups by HotkeyDef::group
in adapter declaration order, renders one SettingsSectionHeader + one
SettingsCard per group with HotkeyBindingRow children. Owns the capture
state machine moved verbatim from HotkeySettingsPage. Emits
bindingFocused so the dialog can drive the description bar.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Create `HotkeySettingsDialog`

**Why this task:** the dialog is the public face of the redesign — it contributes the chrome (description bar, palette, history stack from `EmulatorSettingsDialogBase`) and wires the page's signals to the description bar and face-button hints.

**Files:**
- Create: `cpp/src/ui/settings/hotkey_settings_dialog.h`
- Create: `cpp/src/ui/settings/hotkey_settings_dialog.cpp`

- [ ] **Step 4.1: Read `EmulatorSettingsDialogBase` to confirm the protected API**

Run: `cat cpp/src/ui/settings/emulator_settings_dialog_base.h`

Confirm: `setupChrome(title, minSize, windowBg)` is protected; `setHub(QWidget*)` installs a hub widget into the stack at index 0; `pushPage(QWidget*, hasSubTabs)` pushes onto the stack; `m_descBar` is a protected member (`SettingsDescriptionBar*`).

- [ ] **Step 4.2: Write the dialog header**

Create `cpp/src/ui/settings/hotkey_settings_dialog.h`:

```cpp
#pragma once

#include "ui/settings/emulator_settings_dialog_base.h"
#include "core/binding_def.h"

class GenericHotkeyPage;
class SdlInputManager;
class AppController;

// Hotkey settings dialog. Extends EmulatorSettingsDialogBase to inherit
// the warm-grey + amber palette, history-stack chrome, and bottom
// SettingsDescriptionBar. The body is a single GenericHotkeyPage; the
// stack is single-entry (no hub) — hotkey data fits on one scrollable
// page, like the controller-mapping view.
class HotkeySettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    HotkeySettingsDialog(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void onBindingFocused(HotkeyDef def, QString currentDisplay);
    void onBindingFocusCleared();
    void onRestoreDefaultsClicked();

private:
    GenericHotkeyPage* m_page = nullptr;
    SdlInputManager* m_inputManager;
};
```

- [ ] **Step 4.3: Write the dialog implementation**

Create `cpp/src/ui/settings/hotkey_settings_dialog.cpp`:

```cpp
#include "hotkey_settings_dialog.h"
#include "ui/settings/generic_hotkey_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/settings_description_bar.h"
#include "ui/app_controller.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QVBoxLayout>

HotkeySettingsDialog::HotkeySettingsDialog(SdlInputManager* inputManager,
                                            AppController* appController,
                                            const QString& emuId,
                                            QWidget* parent)
    : EmulatorSettingsDialogBase(appController, emuId, parent),
      m_inputManager(inputManager) {
    setupChrome(QStringLiteral("Hotkey Settings"),
                QSize(900, 720),
                SettingsDialogTheme::windowBg());

    m_page = new GenericHotkeyPage(inputManager, appController, emuId, this);
    connect(m_page, &GenericHotkeyPage::bindingFocused,
            this, &HotkeySettingsDialog::onBindingFocused);
    connect(m_page, &GenericHotkeyPage::bindingFocusCleared,
            this, &HotkeySettingsDialog::onBindingFocusCleared);
    setHub(m_page);  // single-page dialog — page IS the hub

    // Face-button hints in the description bar.
    if (m_descBar) {
        m_descBar->setInputManager(inputManager);
        m_descBar->setHints({
            { QStringLiteral("confirm"),     QStringLiteral("Rebind") },
            { QStringLiteral("back"),        QStringLiteral("Clear")  },
            { QStringLiteral("navigate_ud"), QStringLiteral("Navigate") },
            { QStringLiteral("switch_tab"),  QStringLiteral("Add") },
        });
    }

    // Restore Defaults button — left-aligned in the description bar
    // footer, opposite the painted face hints.
    auto* restore = new QPushButton(QStringLiteral("Restore Defaults"), this);
    restore->setCursor(Qt::PointingHandCursor);
    restore->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:4px; padding:6px 14px; }"
        "QPushButton:focus { border-color:%4; }")
        .arg(SettingsDialogTheme::titleBarBg().name(),
             SettingsDialogTheme::textPrimary().name(),
             SettingsDialogTheme::cardBorder().name(),
             SettingsDialogTheme::accent().name()));
    connect(restore, &QPushButton::clicked, this,
            &HotkeySettingsDialog::onRestoreDefaultsClicked);

    if (auto* descLayout = qobject_cast<QHBoxLayout*>(m_descBar->layout())) {
        descLayout->insertWidget(0, restore, 0, Qt::AlignBottom);
    }
}

void HotkeySettingsDialog::onBindingFocused(HotkeyDef def, QString currentDisplay) {
    if (m_descBar) m_descBar->setHotkey(def, currentDisplay);
}

void HotkeySettingsDialog::onBindingFocusCleared() {
    if (m_descBar) m_descBar->clear();
}

void HotkeySettingsDialog::onRestoreDefaultsClicked() {
    m_page->restoreDefaults();
}

void HotkeySettingsDialog::keyPressEvent(QKeyEvent* e) {
    // Face-button shortcuts. SdlInputManager translates A/B/X/Y to these
    // Qt keys via the unified-input pipeline (see CLAUDE.md "Input System").
    switch (e->key()) {
        case Qt::Key_Return:                            // A — Rebind
            m_page->rebindFocused();
            return;
        case Qt::Key_Back:                              // B — Clear
            m_page->clearFocused();
            return;
        case Qt::Key_M:                                 // Y — Add (alternate binding)
            m_page->appendRebindFocused();
            return;
        default:
            break;
    }
    EmulatorSettingsDialogBase::keyPressEvent(e);
}
```

- [ ] **Step 4.4: Wire dialog into the build**

Edit `cpp/CMakeLists.txt`:

In `set(SOURCES ...)`, add (near `generic_hotkey_page.cpp`):
```cmake
    src/ui/settings/hotkey_settings_dialog.cpp
```

In `set(HEADERS ...)`, add:
```cmake
    src/ui/settings/hotkey_settings_dialog.h
```

- [ ] **Step 4.5: Build the main app target**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -20
```
Expected: clean build. The new files compile; nothing in `app_controller.cpp` has been switched yet, so the old `HotkeySettingsPage` is still referenced — that's fine for this checkpoint.

- [ ] **Step 4.6: Commit**

```bash
git add cpp/src/ui/settings/hotkey_settings_dialog.h \
        cpp/src/ui/settings/hotkey_settings_dialog.cpp \
        cpp/CMakeLists.txt && \
git commit -m "$(cat <<'EOF'
hotkey: add HotkeySettingsDialog wrapping GenericHotkeyPage

Extends EmulatorSettingsDialogBase to inherit the warm-grey + amber
palette, history-stack chrome, and SettingsDescriptionBar footer.
Wires bindingFocused -> setHotkey() on the description bar and
A/B/Y face-button keys to rebind/clear/append flows on the page.
Restore Defaults moves into the description bar opposite the face hints.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Switch the entry point

**Why this task:** flips the live dialog. After this commit, opening hotkey settings from the in-game menu uses the new dialog; the old `HotkeySettingsPage` is still in the tree but unreferenced.

**Files:**
- Modify: `cpp/src/ui/app_controller.cpp` (line 13 include, line 464 instantiation).

- [ ] **Step 5.1: Swap the include**

Edit `cpp/src/ui/app_controller.cpp`:

Line 13:
```cpp
#include "settings/hotkey_settings_page.h"
```
becomes:
```cpp
#include "settings/hotkey_settings_dialog.h"
```

- [ ] **Step 5.2: Swap the instantiation**

In the same file, line 464 (inside `AppController::showHotkeySettings`):
```cpp
auto* dialog = new HotkeySettingsPage(m_inputManager, this, emuId);
```
becomes:
```cpp
auto* dialog = new HotkeySettingsDialog(m_inputManager, this, emuId);
```

- [ ] **Step 5.3: Build the app and the existing tests**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -30
```
Expected: clean build. `HotkeySettingsPage` is no longer referenced from `app_controller.cpp` but still compiled because its sources are still in `SOURCES`. That's intentional — Task 6 deletes them.

- [ ] **Step 5.4: Manual smoke — open the new dialog**

Run:
```bash
open ./cpp/build/RetroNest.app
```

Steps:
1. Pick PCSX2 from the emulator list.
2. Launch any PS2 game.
3. Press Cmd+Escape (or Select+Circle) to open the in-game menu.
4. Choose "Hotkey Settings."

Expected:
- Window has the warm-grey background (`#585450`), not the old dark navy.
- Title row reads "Hotkey Settings" in the same font/colour as PCSX2 Settings.
- Section headers (SPEED CONTROL / SYSTEM / SAVE STATES / AUDIO) are amber, uppercase.
- Bindings render in card-shaped rows.
- Bottom description bar shows "Focus a setting to see its description." until a row is focused, then "<Label>  —  Currently: <value>".
- Tab moves focus between rows; A on the controller (or Enter) starts a rebind.

If anything is off, do not commit yet — report the divergence.

- [ ] **Step 5.5: Commit**

```bash
git add cpp/src/ui/app_controller.cpp && \
git commit -m "$(cat <<'EOF'
app: swap HotkeySettingsPage for HotkeySettingsDialog

One-line cutover at the dialog instantiation site. Old page sources
are still compiled; the next commit deletes them.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Delete the old page

**Why this task:** removes 534 lines of replaced code and trims the build graph. Single deletion commit so the diff is reviewable.

**Files:**
- Delete: `cpp/src/ui/settings/hotkey_settings_page.h`
- Delete: `cpp/src/ui/settings/hotkey_settings_page.cpp`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 6.1: Confirm zero references remain**

Run:
```bash
grep -rn "HotkeySettingsPage\|hotkey_settings_page" cpp/ --include='*.cpp' --include='*.h' --include='*.qml' --include='*.txt' | grep -v build/
```
Expected: only the file itself and its CMakeLists entries appear. If any other source still references `HotkeySettingsPage`, fix it before continuing.

- [ ] **Step 6.2: Drop the build entries**

Edit `cpp/CMakeLists.txt`:

Remove the `src/ui/settings/hotkey_settings_page.cpp` line from `set(SOURCES ...)` (was line 99).
Remove the `src/ui/settings/hotkey_settings_page.h` line from `set(HEADERS ...)` (was line 174).

- [ ] **Step 6.3: Delete the files (and stage the deletion)**

Run:
```bash
git rm cpp/src/ui/settings/hotkey_settings_page.h \
       cpp/src/ui/settings/hotkey_settings_page.cpp
```

`git rm` removes the working-tree files AND stages the deletion in one step.

- [ ] **Step 6.4: Reconfigure and rebuild**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" \
  && cmake --build build 2>&1 | tail -20
```
Expected: clean build. No undefined references.

- [ ] **Step 6.5: Run the test suite**

Run:
```bash
cd cpp/build && ctest --output-on-failure 2>&1 | tail -40
```
Expected: every test passes, including the three new ones (`SettingsDescriptionBar`, `HotkeyBindingRow`, `GenericHotkeyPage`).

- [ ] **Step 6.6: Commit**

The deletions were already staged by `git rm` in Step 6.3. Just stage the CMake change and commit.

```bash
git add cpp/CMakeLists.txt && \
git commit -m "$(cat <<'EOF'
hotkey: drop legacy HotkeySettingsPage

Replaced by HotkeySettingsDialog + GenericHotkeyPage + HotkeyBindingRow.
534 lines deleted.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Manual verification — full behaviour preservation

**Why this task:** the unit tests cover construction, signal flow, and individual widget behaviour, but the dialog is ultimately a UX surface. This step walks through every entry on the spec's behaviour-preservation checklist (§6) against a live build.

No code changes here — pure manual verification. Open issues (if any) are fixed in follow-up commits.

**Setup:**

```bash
open ./cpp/build/RetroNest.app
```

- [ ] **Step 7.1: PCSX2 — full pass**

1. Launch a PS2 game → in-game menu → Hotkey Settings.
2. **Click a row's button.** Button styling switches to "Press a button or key..."; press F4 → button updates to `F4`; description bar updates to `<label>  —  Currently: F4`.
3. **Right-click a row's button.** Binding clears; button reads `Not bound`.
4. **Shift+click a row's button.** Capture starts in append mode; press a controller button → binding appends (does NOT replace). Release the button → capture commits.
5. **Restore Defaults.** All bindings revert to their `defaultValue` per `pcsx2_adapter.cpp:hotkeyBindingDefs()`.
6. **ESC during capture.** Capture cancels cleanly; the original binding text returns.
7. **Multi-binding display.** Set a binding to two parts (Shift+click, then capture two buttons). Verify the button reads e.g. `SDL-0 Cross + SDL-0 Circle` (formatted via `displayMultiBinding`).

- [ ] **Step 7.2: DuckStation — abbreviated pass**

1. Launch a PS1 game → in-game menu → Hotkey Settings.
2. Click a row, capture a key, verify save.
3. Verify section headers match `duckstation_adapter.cpp:hotkeyBindingDefs()` ordering.

- [ ] **Step 7.3: PPSSPP — abbreviated pass**

1. Launch a PSP game → in-game menu → Hotkey Settings.
2. Click a row, capture a controller button, verify the saved value uses PPSSPP's numeric format (e.g. `10-19`) — NOT the SDL string format.
3. Verify the binding writes to `controls.ini` `[ControlMapping]` (per the adapter's bindings-config-file override). Run `cat <root>/emulators/ppsspp/PSP/SYSTEM/controls.ini | grep -A2 '\[ControlMapping\]'` to confirm.

- [ ] **Step 7.4: Dolphin — empty-state pass**

1. Open the Dolphin emulator detail page.
2. Verify the "Hotkeys" button is HIDDEN (Dolphin returns `{}` from `hotkeyBindingDefs()`).
3. If the button is still shown, edit the entry point in `cpp/qml/AppUI/EmulatorDetailPage.qml` around line 390:

   ```qml
   DetailButton {
       label: "Hotkeys"
       bgColor: SettingsTheme.card
       textColor: SettingsTheme.text
       isFocused: root.focusIndex === root.actionOffset + (root.emuId === "dolphin" ? 3 : 2)
       onClicked: app.showHotkeySettings(root.emuId)
   }
   ```

   Wrap it in a `visible:` gate using a new `Q_INVOKABLE bool hasHotkeys(QString emuId)` on `AppController` (returns `!hotkeyBindings(emuId).isEmpty()`), and adjust the `focusIndex` math so the gap closes when the button is hidden. Note: there are also two `case 2/3: app.showHotkeySettings(root.emuId)` keypress handlers at `EmulatorDetailPage.qml:65,74` — those need to be guarded too, since they're how the keyboard / controller activates the same button.

- [ ] **Step 7.5: Commit any spot-fixes from Step 7.4 (only if needed)**

If the hide-when-empty gate needs to be added:

```bash
git add <whatever-qml-or-source-was-touched> && \
git commit -m "$(cat <<'EOF'
hotkey: hide entry point for emulators with no hotkey bindings

Dolphin returns {} from hotkeyBindingDefs() — gating the in-game menu
entry on a non-empty list keeps the dialog from being reachable in a
state that would only show the empty placeholder.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

If no fix was needed, skip this step.

---

## Task 8: Update memory and final cleanup

**Why this task:** project-memory hygiene. Mirrors the controller-mapping-redesign memory entry so future sessions can find this redesign.

**Files:**
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md`
- Create: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/hotkey-settings-redesign.md`

> **Note:** memory lives outside the repo, so it does not get a git commit.

- [ ] **Step 8.1: Write the memory entry**

Create `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/hotkey-settings-redesign.md`:

```markdown
---
name: Hotkey settings redesign — chrome and palette alignment
description: Hotkey dialog now extends EmulatorSettingsDialogBase + uses SettingsDialogTheme; replaces the bespoke 534-line HotkeySettingsPage. Schema and adapters unchanged.
type: project
---

The hotkey dialog (`HotkeySettingsDialog` + `GenericHotkeyPage` + `HotkeyBindingRow`) replaces the bespoke `HotkeySettingsPage` (deleted 2026-05-08). All three settings surfaces — emulator settings, controller mapping, hotkeys — now share `SettingsDialogTheme` (warm grey + amber) and `EmulatorSettingsDialogBase` chrome (palette, history stack, `SettingsDescriptionBar` footer).

Layout: single scrollable page with `SettingsSectionHeader` between groups (mirrors controller-mapping shape; no hub). Capture state machine moved verbatim. `SettingsDescriptionBar` gained `setHotkey(HotkeyDef, currentDisplay)`. `HotkeyDef` and every adapter's `hotkeyBindingDefs()` are unchanged.

**Why:** to apply (concretely): cosmetic mismatch — hotkeys was the only Qt Widgets dialog still on the dark-navy palette `#1e1e3a` and hand-rolling its own chrome — was the explicit motivation. The data layer was already schema-driven; this redesign was about chrome and visual alignment, not architecture.

**How to apply:** when adding a new emulator with hotkey support, the only adapter work is overriding `hotkeyBindingDefs()`. The dialog renders the data automatically — no per-emulator dialog code. If the emulator has a non-SDL binding format (like PPSSPP's numeric `10-19`), reuse the existing `formatBinding()` override that controller mappings use; the page passes through the captured raw element to `formatBinding()` via `SdlInputManager`.
```

- [ ] **Step 8.2: Add the index entry**

Edit `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-RetroNest-Project/memory/MEMORY.md`:

Append a new line after the PPSSPP schema-alignment entry:

```markdown
- [Hotkey settings redesign](hotkey-settings-redesign.md) — hotkey dialog rebuilt on EmulatorSettingsDialogBase + SettingsDialogTheme; HotkeySettingsDialog + GenericHotkeyPage + HotkeyBindingRow replace the legacy bespoke page (2026-05-08).
```

- [ ] **Step 8.3: Final test sweep**

Run:
```bash
cd cpp/build && ctest --output-on-failure
```
Expected: all tests pass, no regressions.

- [ ] **Step 8.4: Final git status**

Run:
```bash
git log --oneline main..HEAD
```
Expected output (in commit order):
```
<sha> hotkey: drop legacy HotkeySettingsPage
<sha> app: swap HotkeySettingsPage for HotkeySettingsDialog
<sha> hotkey: add HotkeySettingsDialog wrapping GenericHotkeyPage
<sha> hotkey: add GenericHotkeyPage — schema-driven content page
<sha> hotkey: add HotkeyBindingRow widget
<sha> description-bar: add setHotkey() variant for hotkey footer
```
(Plus an optional Task 7 spot-fix commit.) The spec commit `21c4898` is already on `main`.

---

## Behaviour Preservation Checklist (from spec §6)

This checklist is what Task 7 verifies against a live build.

- [ ] Click a binding row → start 5-second capture.
- [ ] Shift+click a binding row → start capture in append mode.
- [ ] Right-click a binding row → clear that binding.
- [ ] Capture accepts SDL controller events and keyboard events; modifier keys accumulate, do not commit.
- [ ] On capture timeout or commit, save via the existing `AppController` flow.
- [ ] Multi-binding display: `"SDL-0/Back & SDL-0/RightShoulder"` → `"SDL-0 Create + SDL-0 R1"` via `displayMultiBinding`.
- [ ] Tooltip on each binding button (left click / Shift / right click guide).
- [ ] Restore Defaults restores every binding to `HotkeyDef::defaultValue`.
- [ ] Closing the dialog while a capture is in progress cancels cleanly.
- [ ] Empty-state — Dolphin's hotkey entry point is hidden (or shows the placeholder if opened).
