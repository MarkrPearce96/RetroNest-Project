# Hotkey Settings Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite the HotkeySettingsPage dialog with sidebar navigation and expand from 9 to 39 PCSX2 hotkeys across 4 categories.

**Architecture:** The existing `HotkeySettingsPage` is rewritten from a flat scrollable list to a sidebar+content layout matching the controller mapping dialog pattern. The PCSX2 adapter's `hotkeyBindingDefs()` is expanded to 39 entries. No new files needed — just modify the adapter and the dialog. Shared binding code (`binding_widget_common.h`, `binding_display.h`) is reused.

**Tech Stack:** C++17, Qt6 Widgets (QDialog, QListWidget, QScrollArea, QGridLayout), existing SdlInputManager capture, existing AppController hotkey APIs

**Spec:** `docs/superpowers/specs/2026-04-04-hotkey-settings-design.md`

**Build command:**
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```

---

## File Structure

### Modified files
| File | Changes |
|------|---------|
| `cpp/src/adapters/pcsx2_adapter.cpp` | Expand `hotkeyBindingDefs()` from 9 to 39 entries |
| `cpp/src/ui/settings/hotkey_settings_page.h` | Rewrite: add sidebar, category switching, loadBindings() |
| `cpp/src/ui/settings/hotkey_settings_page.cpp` | Rewrite: sidebar + content layout, reuse shared binding code |

---

### Task 1: Expand PCSX2 adapter hotkey definitions

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp`

- [ ] **Step 1: Replace hotkeyBindingDefs() body**

Find the existing `hotkeyBindingDefs()` method (currently returns 9 entries). Replace the return block with all 39 hotkeys across 4 categories:

```cpp
QVector<HotkeyDef> PCSX2Adapter::hotkeyBindingDefs() const {
    return {
        // ── Speed Control ──
        {"Toggle Pause",                "Speed Control", "Hotkeys", "TogglePause",        "SDL-0/Guide"},
        {"Frame Advance",               "Speed Control", "Hotkeys", "FrameAdvance",       ""},
        {"Toggle Frame Limit",          "Speed Control", "Hotkeys", "ToggleFrameLimit",   ""},
        {"Toggle Turbo / Fast Forward", "Speed Control", "Hotkeys", "ToggleTurbo",        "Keyboard/Period"},
        {"Turbo / Fast Forward (Hold)", "Speed Control", "Hotkeys", "HoldTurbo",          ""},
        {"Toggle Slow Motion",          "Speed Control", "Hotkeys", "ToggleSlowMotion",   "Keyboard/Shift & Keyboard/Backspace"},
        {"Increase Target Speed",       "Speed Control", "Hotkeys", "IncreaseSpeed",      ""},
        {"Decrease Target Speed",       "Speed Control", "Hotkeys", "DecreaseSpeed",      ""},

        // ── System ──
        {"Reset Virtual Machine",       "System",        "Hotkeys", "ResetVM",            ""},
        {"Reload Patches",              "System",        "Hotkeys", "ReloadPatches",      ""},
        {"Swap Memory Cards",           "System",        "Hotkeys", "SwapMemCards",       ""},

        // ── Save States ──
        {"Select Previous Save Slot",   "Save States",   "Hotkeys", "PreviousSaveStateSlot",    "Keyboard/Shift & Keyboard/F2"},
        {"Select Next Save Slot",       "Save States",   "Hotkeys", "NextSaveStateSlot",         "Keyboard/F2"},
        {"Save State To Selected Slot", "Save States",   "Hotkeys", "SaveStateToSlot",           "Keyboard/F1"},
        {"Load State From Selected Slot","Save States",  "Hotkeys", "LoadStateFromSlot",         "Keyboard/F3"},
        {"Load Backup State",           "Save States",   "Hotkeys", "LoadBackupStateFromSlot",   ""},
        {"Save State and Select Next Slot","Save States", "Hotkeys","SaveStateAndSelectNextSlot", ""},
        {"Select Next Slot and Save State","Save States", "Hotkeys","SelectNextSlotAndSaveState", ""},
        {"Save State To Slot 1",        "Save States",   "Hotkeys", "SaveStateToSlot1",          ""},
        {"Load State From Slot 1",      "Save States",   "Hotkeys", "LoadStateFromSlot1",        ""},
        {"Save State To Slot 2",        "Save States",   "Hotkeys", "SaveStateToSlot2",          ""},
        {"Load State From Slot 2",      "Save States",   "Hotkeys", "LoadStateFromSlot2",        ""},
        {"Save State To Slot 3",        "Save States",   "Hotkeys", "SaveStateToSlot3",          ""},
        {"Load State From Slot 3",      "Save States",   "Hotkeys", "LoadStateFromSlot3",        ""},
        {"Save State To Slot 4",        "Save States",   "Hotkeys", "SaveStateToSlot4",          ""},
        {"Load State From Slot 4",      "Save States",   "Hotkeys", "LoadStateFromSlot4",        ""},
        {"Save State To Slot 5",        "Save States",   "Hotkeys", "SaveStateToSlot5",          ""},
        {"Load State From Slot 5",      "Save States",   "Hotkeys", "LoadStateFromSlot5",        ""},
        {"Save State To Slot 6",        "Save States",   "Hotkeys", "SaveStateToSlot6",          ""},
        {"Load State From Slot 6",      "Save States",   "Hotkeys", "LoadStateFromSlot6",        ""},
        {"Save State To Slot 7",        "Save States",   "Hotkeys", "SaveStateToSlot7",          ""},
        {"Load State From Slot 7",      "Save States",   "Hotkeys", "LoadStateFromSlot7",        ""},
        {"Save State To Slot 8",        "Save States",   "Hotkeys", "SaveStateToSlot8",          ""},
        {"Load State From Slot 8",      "Save States",   "Hotkeys", "LoadStateFromSlot8",        ""},
        {"Save State To Slot 9",        "Save States",   "Hotkeys", "SaveStateToSlot9",          ""},
        {"Load State From Slot 9",      "Save States",   "Hotkeys", "LoadStateFromSlot9",        ""},
        {"Save State To Slot 10",       "Save States",   "Hotkeys", "SaveStateToSlot10",         ""},
        {"Load State From Slot 10",     "Save States",   "Hotkeys", "LoadStateFromSlot10",       ""},

        // ── Audio ──
        {"Toggle Mute",                 "Audio",         "Hotkeys", "Mute",              ""},
        {"Increase Volume",             "Audio",         "Hotkeys", "IncreaseVolume",    ""},
        {"Decrease Volume",             "Audio",         "Hotkeys", "DecreaseVolume",    ""},
    };
}
```

- [ ] **Step 2: Build to verify**

```bash
cd cpp && cmake --build build
```

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "feat: expand PCSX2 hotkey definitions from 9 to 39 across 4 categories"
```

---

### Task 2: Rewrite HotkeySettingsPage header

**Files:**
- Modify: `cpp/src/ui/settings/hotkey_settings_page.h`

- [ ] **Step 1: Replace the entire header**

Replace `cpp/src/ui/settings/hotkey_settings_page.h` with:

```cpp
#pragma once

#include <QDialog>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVector>
#include <QWidget>

class SdlInputManager;
class AppController;

/**
 * HotkeySettingsPage — Qt Widget dialog for hotkey binding configuration.
 *
 * Layout: left sidebar (4 categories) + right content (grid of label + binding button).
 * Matches the controller mapping dialog sidebar pattern.
 */
class HotkeySettingsPage : public QDialog {
    Q_OBJECT

public:
    HotkeySettingsPage(SdlInputManager* inputManager,
                       AppController* appController,
                       QWidget* parent = nullptr);

private:
    void buildUI();
    void loadBindings();
    void showCategory(int index);
    void startCapture(const QString& key);
    void onBindingCaptured(int deviceIndex, const QString& element, bool isAxis, bool positive);
    void onKeyboardCaptured(const QString& keyString);
    void finishCapture(const QString& formatted);
    void onResetDefaults();

    SdlInputManager* m_inputManager;
    AppController* m_appController;

    QListWidget* m_categoryList = nullptr;
    QVector<QWidget*> m_categoryPages;  // one content page per category
    QWidget* m_contentArea = nullptr;

    // iniKey -> button widget
    QMap<QString, QPushButton*> m_bindingButtons;
    QString m_capturingKey; // INI key of the hotkey being captured

    // Category names in display order
    QStringList m_categories;

    // Cached hotkey data: iniKey -> {label, group, section, key, currentValue}
    struct HotkeyEntry {
        QString label;
        QString group;
        QString section;
        QString key;
        QString currentValue;
    };
    QMap<QString, HotkeyEntry> m_entries; // keyed by INI key
};
```

- [ ] **Step 2: Build to verify**

```bash
cd cpp && cmake --build build
```

This will have link errors since the .cpp hasn't been rewritten yet — that's expected.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/ui/settings/hotkey_settings_page.h
git commit -m "feat: rewrite HotkeySettingsPage header with sidebar + category support"
```

---

### Task 3: Rewrite HotkeySettingsPage implementation

**Files:**
- Modify: `cpp/src/ui/settings/hotkey_settings_page.cpp`

- [ ] **Step 1: Replace the entire implementation**

Replace `cpp/src/ui/settings/hotkey_settings_page.cpp` with a new implementation. Key structure:

**Includes:**
```cpp
#include "hotkey_settings_page.h"
#include "binding_widget_common.h"
#include "binding_display.h"
#include "core/sdl_input_manager.h"
#include "ui/app_controller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
```

**Constructor:**
- Set window title "Hotkey Settings", min/default size 900x600
- Set dialog stylesheet using `kBg` from shared header
- Load hotkey bindings from `m_appController->hotkeyBindings()` into `m_entries` map (keyed by INI key)
- Extract category names preserving order into `m_categories`
- Connect SDL input signals
- Call `buildUI()`
- Select first category

**buildUI():**
Create the three-part layout:

1. **Outer layout** — `QHBoxLayout` containing sidebar + right panel
2. **Left sidebar** — `QListWidget`, 160px fixed width, styled like controller mapping:
   ```cpp
   m_categoryList = new QListWidget();
   m_categoryList->setFixedWidth(160);
   m_categoryList->setStyleSheet(QString(
       "QListWidget { background: #1e1e3a; border: none; border-right: 1px solid %1; }"
       "QListWidget::item { padding: 10px 14px; color: %2; font-size: 13px; }"
       "QListWidget::item:selected { background: %3; color: #ffffff; font-weight: bold; }"
   ).arg(kBoxBorder, kTextSecondary, kAccent));
   ```
   Add each category name as an item. Connect `currentRowChanged` to `showCategory()`.

3. **Right panel** — `QVBoxLayout` with:
   - **Content area** (`m_contentArea`) containing a `QStackedWidget`-like approach: create one QWidget per category, show/hide based on selection
   - Each category page: `QScrollArea` containing a `QWidget` with `QGridLayout`
     - Row 0: category title (bold, 15px, kTextPrimary)
     - Row 1: subtitle ("Click a binding to remap. Right-click to clear.", kTextSecondary, 12px)
     - Rows 2+: for each hotkey in this category, label (col 0, 220px) + BindBtn (col 1)
   - **Bottom bar**: `QHBoxLayout` with stretch + "Restore Defaults" button + "Close" button
     - Close: `connect(closeBtn, ..., this, &QDialog::accept)`
     - Restore: `connect(restoreBtn, ..., this, &HotkeySettingsPage::onResetDefaults)`

**BindBtn setup for each hotkey:**
```cpp
auto* btn = new BindBtn();
btn->setFixedHeight(kBtnH);
btn->setCursor(Qt::PointingHandCursor);
btn->setStyleSheet(kBtnStyle);
btn->setText("Not bound");
m_bindingButtons[entry.key] = btn;

connect(btn, &QPushButton::clicked, this, [this, key = entry.key]() {
    startCapture(key);
});

btn->onRightClick = [this, key = entry.key]() {
    m_appController->clearHotkey("Hotkeys", key);
    m_entries[key].currentValue = "";
    loadBindings();
};
```

**loadBindings():**
```cpp
void HotkeySettingsPage::loadBindings() {
    QVariantList bindings = m_appController->hotkeyBindings();
    for (const auto& b : bindings) {
        auto map = b.toMap();
        QString key = map["key"].toString();
        if (m_entries.contains(key))
            m_entries[key].currentValue = map["currentValue"].toString();
    }

    for (auto it = m_bindingButtons.constBegin(); it != m_bindingButtons.constEnd(); ++it) {
        QString key = it.key();
        QPushButton* btn = it.value();
        QString val = m_entries.value(key).currentValue;
        // Display with controller-type-aware formatting
        QString display = displayBinding(val);
        btn->setText(display.isEmpty() ? "Not bound" : display);
        btn->setStyleSheet(kBtnStyle);
    }
}
```

Note: For display name formatting, use `displayBinding(val)` without detailed controller type for now (hotkeys are typically keyboard bindings so the controller-type display names matter less). If a device index is present, extract it with `deviceIndexFromBinding()` and call `detailedControllerTypeForDevice()` like the controller binding widgets do.

**showCategory(int index):**
```cpp
void HotkeySettingsPage::showCategory(int index) {
    for (int i = 0; i < m_categoryPages.size(); i++)
        m_categoryPages[i]->setVisible(i == index);
}
```

**startCapture(key):**
- Cancel previous capture if any (restore button text + style)
- Set `m_capturingKey = key`
- Set button text to "Press a button..." with `kCapturingStyle`
- Call `m_inputManager->startCapture()`

**onBindingCaptured / onKeyboardCaptured / finishCapture:**
Same pattern as controller binding widgets:
- Format SDL binding via `m_appController->formatCapturedBinding()`
- Save via `m_appController->saveHotkey("Hotkeys", key, formatted)`
- Update entry and call `loadBindings()`

**onResetDefaults():**
```cpp
void HotkeySettingsPage::onResetDefaults() {
    m_appController->resetHotkeys();
    // Reload bindings from INI
    QVariantList bindings = m_appController->hotkeyBindings();
    for (const auto& b : bindings) {
        auto map = b.toMap();
        QString key = map["key"].toString();
        if (m_entries.contains(key))
            m_entries[key].currentValue = map["currentValue"].toString();
    }
    loadBindings();
}
```

- [ ] **Step 2: Build to verify**

```bash
cd cpp && cmake --build build
```

- [ ] **Step 3: Run the app and test**

```bash
./cpp/build/EmulatorFrontend
```

Open the hotkey settings dialog. Verify:
- Left sidebar shows 4 categories
- Clicking a category switches the content
- Speed Control shows 8 hotkeys
- System shows 3
- Save States shows 25 (scrollable)
- Audio shows 3
- Click-to-bind works (SDL and keyboard)
- Right-click-to-clear works
- Restore Defaults resets all bindings
- Close closes the dialog
- Display names use `displayBinding()` formatter

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/settings/hotkey_settings_page.cpp
git commit -m "feat: rewrite HotkeySettingsPage with sidebar categories and 39 hotkeys"
```
