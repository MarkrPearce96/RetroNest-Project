# PCSX2 Settings — Button Hints & Key Behavior Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add input-aware button hint pills to the PCSX2 settings description bar and fix key behavior so Escape acts as hierarchical back and L1/R1 cycle sub-tabs.

**Architecture:** Extend the existing `Pcsx2DescriptionBar` widget with a custom-painted hints row that adapts its glyphs to keyboard/Xbox/PlayStation based on `SdlInputManager::controllerType()`. Change the dialog's key handler to use Escape/Key_Back for hierarchical back and suppress Tab on non-tabbed pages. Map L1/R1 to Tab/Backtab in the SDL input manager.

**Tech Stack:** C++17, Qt6 Widgets (QPainter for pill rendering), SDL2

---

## File Structure

| File | Responsibility |
|------|---------------|
| `src/ui/settings/pcsx2/widgets/pcsx2_description_bar.h` | Add `ButtonHint` struct, `setHints()`, hint painting |
| `src/ui/settings/pcsx2/widgets/pcsx2_description_bar.cpp` | Implement hints row rendering with QPainter, input-type reactivity |
| `src/ui/settings/pcsx2/pcsx2_settings_dialog.h` | Add `m_hasSubTabs` tracking, forward-declare `SdlInputManager` |
| `src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp` | Change key handler (Esc→back, remove Backspace, add Key_Back, suppress Tab), pass hints on page push/pop |
| `src/core/sdl_input_manager.cpp` | Map L1→Backtab, R1→Tab |
| `src/ui/app_controller.h` | Add `sdlInputManager()` getter |
| `tests/test_pcsx2_description_bar.cpp` | Add tests for hints API |
| `cpp/CMakeLists.txt` | No changes needed (description bar test already linked) |

---

### Task 1: Map L1/R1 to Tab/Backtab in SDL Input Manager

**Files:**
- Modify: `src/core/sdl_input_manager.cpp:54-67` (`mapButtonToKey` function)

- [ ] **Step 1: Add L1/R1 mappings**

In `src/core/sdl_input_manager.cpp`, add two cases to the `mapButtonToKey` switch, just before the `default` line:

```cpp
static int mapButtonToKey(SDL_GameControllerButton btn) {
    switch (btn) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    return Qt::Key_Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return Qt::Key_Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return Qt::Key_Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Qt::Key_Right;
    case SDL_CONTROLLER_BUTTON_A:          return Qt::Key_Return;
    case SDL_CONTROLLER_BUTTON_B:          return Qt::Key_Back;
    case SDL_CONTROLLER_BUTTON_X:          return Qt::Key_Backspace;
    case SDL_CONTROLLER_BUTTON_Y:          return Qt::Key_M;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return Qt::Key_Backtab;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Qt::Key_Tab;
    // Start handled as signal, not key injection (conflicts with Shortcuts)
    default: return 0;
    }
}
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds with no errors.

- [ ] **Step 3: Commit**

```bash
git add src/core/sdl_input_manager.cpp
git commit -m "feat(input): map L1/R1 shoulder buttons to Tab/Backtab"
```

---

### Task 2: Add SdlInputManager Getter to AppController

**Files:**
- Modify: `src/ui/app_controller.h:28` (near `setSdlInputManager`)

- [ ] **Step 1: Add getter**

In `src/ui/app_controller.h`, add a getter right after the existing `setSdlInputManager` line (line 28):

```cpp
void setSdlInputManager(SdlInputManager* mgr) { m_inputManager = mgr; }
SdlInputManager* sdlInputManager() const { return m_inputManager; }
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/ui/app_controller.h
git commit -m "feat(app): add sdlInputManager() getter to AppController"
```

---

### Task 3: Add ButtonHint Struct and setHints/clearHints API to Description Bar

**Files:**
- Modify: `src/ui/settings/pcsx2/widgets/pcsx2_description_bar.h`
- Modify: `src/ui/settings/pcsx2/widgets/pcsx2_description_bar.cpp`
- Modify: `tests/test_pcsx2_description_bar.cpp`

- [ ] **Step 1: Write failing tests for hints API**

Add the following tests to `tests/test_pcsx2_description_bar.cpp`, after the existing `comboRecommendedTranslatesToLabel` test and before the closing `};`:

```cpp
void setHintsStoresHints() {
    Pcsx2DescriptionBar bar;
    QVector<Pcsx2DescriptionBar::ButtonHint> hints = {
        {"navigate_ud", "Navigate"},
        {"confirm", "Select"},
        {"back", "Close"},
    };
    bar.setHints(hints);
    QCOMPARE(bar.hints().size(), 3);
    QCOMPARE(bar.hints()[0].action, QString("navigate_ud"));
    QCOMPARE(bar.hints()[0].label, QString("Navigate"));
    QCOMPARE(bar.hints()[2].action, QString("back"));
}

void clearHintsRemovesAll() {
    Pcsx2DescriptionBar bar;
    bar.setHints({{"confirm", "Select"}});
    QCOMPARE(bar.hints().size(), 1);
    bar.clearHints();
    QCOMPARE(bar.hints().size(), 0);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```bash
cd cpp && cmake --build build --target test_pcsx2_description_bar 2>&1 | tail -10
```
Expected: FAIL — `setHints`, `clearHints`, `hints`, and `ButtonHint` are not defined.

- [ ] **Step 3: Add ButtonHint struct and API to header**

Replace the entire contents of `src/ui/settings/pcsx2/widgets/pcsx2_description_bar.h` with:

```cpp
#pragma once
#include <QFrame>
#include <QVector>
#include "core/setting_def.h"

class QLabel;
class QHBoxLayout;
class SdlInputManager;

class Pcsx2DescriptionBar : public QFrame {
    Q_OBJECT
public:
    struct ButtonHint {
        QString action;  // "confirm", "back", "navigate_ud", "navigate", "switch_tab"
        QString label;   // "Select", "Back", "Navigate", "Switch Tab"
    };

    explicit Pcsx2DescriptionBar(QWidget* parent = nullptr);

    void setSetting(const SettingDef& def);
    void clear();

    void setHints(const QVector<ButtonHint>& hints);
    void clearHints();
    QVector<ButtonHint> hints() const;

    void setInputManager(SdlInputManager* mgr);

    // test hooks
    QString descText() const;
    QString recommendedText() const;

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    struct GlyphStyle {
        QString text;
        QColor bg;
        QColor fg;
        QColor border;
        int fontSize = 14;
    };
    GlyphStyle glyphFor(const QString& action, int inputType) const;

    QLabel* m_text = nullptr;
    QLabel* m_rec = nullptr;
    QVector<ButtonHint> m_hints;
    SdlInputManager* m_inputManager = nullptr;
};
```

- [ ] **Step 4: Update the implementation file**

Replace the entire contents of `src/ui/settings/pcsx2/widgets/pcsx2_description_bar.cpp` with:

```cpp
#include "pcsx2_description_bar.h"
#include "../pcsx2_theme.h"
#include "core/sdl_input_manager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QFontMetrics>

Pcsx2DescriptionBar::Pcsx2DescriptionBar(QWidget* parent) : QFrame(parent) {
    setObjectName("Pcsx2DescriptionBar");
    setStyleSheet(Pcsx2Theme::descriptionBarQss());
    setMinimumHeight(100);
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    m_text = new QLabel(this);
    m_text->setObjectName("Pcsx2DescText");
    m_text->setWordWrap(true);
    m_rec = new QLabel(this);
    m_rec->setObjectName("Pcsx2DescRecommended");
    m_rec->setAlignment(Qt::AlignTop | Qt::AlignRight);
    lay->addWidget(m_text, 1);
    lay->addWidget(m_rec, 0, Qt::AlignTop);
    clear();
}

void Pcsx2DescriptionBar::setSetting(const SettingDef& def) {
    m_text->setText(def.tooltip.isEmpty()
                    ? QStringLiteral("No description available.")
                    : def.tooltip);
    QString rec = def.recommendedValue.isEmpty() ? def.defaultValue : def.recommendedValue;
    if (def.type == SettingDef::Combo) {
        for (const auto& opt : def.options) {
            if (opt.second == rec) {
                rec = opt.first;
                break;
            }
        }
    }
    m_rec->setText(QStringLiteral("Recommended: %1").arg(rec));
    m_rec->setVisible(!rec.isEmpty());
}

void Pcsx2DescriptionBar::clear() {
    m_text->setText(QStringLiteral("Focus a setting to see its description."));
    m_rec->setVisible(false);
}

void Pcsx2DescriptionBar::setHints(const QVector<ButtonHint>& hints) {
    m_hints = hints;
    // Make room for the hints row
    setMinimumHeight(m_hints.isEmpty() ? 100 : 100);
    update();
}

void Pcsx2DescriptionBar::clearHints() {
    m_hints.clear();
    update();
}

QVector<Pcsx2DescriptionBar::ButtonHint> Pcsx2DescriptionBar::hints() const {
    return m_hints;
}

void Pcsx2DescriptionBar::setInputManager(SdlInputManager* mgr) {
    m_inputManager = mgr;
    if (mgr) {
        connect(mgr, &SdlInputManager::controllerTypeChanged, this,
                QOverload<>::of(&Pcsx2DescriptionBar::update));
    }
}

Pcsx2DescriptionBar::GlyphStyle Pcsx2DescriptionBar::glyphFor(const QString& action, int inputType) const {
    if (inputType == 2) {
        // PlayStation
        if (action == "confirm")     return { QStringLiteral("\u2715"), QColor("#2a3a6a"), QColor("#6d9ddc"), QColor("#3a5a8a"), 18 };
        if (action == "back")        return { QStringLiteral("\u25CB"), QColor("#5c2a3a"), QColor("#dc6d8d"), QColor("#7a3a5a"), 18 };
        if (action == "navigate_ud") return { QStringLiteral("D-Pad \u25B4\u25BE"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "navigate")    return { QStringLiteral("D-Pad \u25B4\u25BE\u25C2\u25B8"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "switch_tab")  return { QStringLiteral("L1 / R1"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    } else if (inputType == 1) {
        // Xbox
        if (action == "confirm")     return { QStringLiteral("A"), QColor("#2a5c2a"), QColor("#6ddc6d"), QColor("#3a7a3a") };
        if (action == "back")        return { QStringLiteral("B"), QColor("#5c2a2a"), QColor("#dc6d6d"), QColor("#7a3a3a") };
        if (action == "navigate_ud") return { QStringLiteral("D-Pad \u25B4\u25BE"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "navigate")    return { QStringLiteral("D-Pad \u25B4\u25BE\u25C2\u25B8"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "switch_tab")  return { QStringLiteral("LB / RB"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    } else {
        // Keyboard
        if (action == "confirm")     return { QStringLiteral("Enter"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
        if (action == "back")        return { QStringLiteral("Esc"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
        if (action == "navigate_ud") return { QStringLiteral("\u2191\u2193"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 18 };
        if (action == "navigate")    return { QStringLiteral("\u2191\u2193\u2190\u2192"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 18 };
        if (action == "switch_tab")  return { QStringLiteral("Tab"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    }
    return { QStringLiteral("?"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
}

void Pcsx2DescriptionBar::paintEvent(QPaintEvent* e) {
    QFrame::paintEvent(e);

    if (m_hints.isEmpty()) return;

    int inputType = m_inputManager ? m_inputManager->controllerType() : 0;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int pillHeight = 28;
    const int pillRadius = 5;
    const int pillPadX = 8;   // horizontal padding inside pill
    const int hintSpacing = 20;
    const int labelGap = 5;   // gap between pill and label

    QFont glyphFont = font();
    glyphFont.setBold(true);

    QFont labelFont = font();
    labelFont.setPixelSize(14);
    labelFont.setWeight(QFont::Medium);

    // Measure total width of all hints
    int totalWidth = 0;
    struct MeasuredHint {
        GlyphStyle glyph;
        QString label;
        int pillWidth;
        int labelWidth;
    };
    QVector<MeasuredHint> measured;
    measured.reserve(m_hints.size());

    for (const auto& h : m_hints) {
        GlyphStyle g = glyphFor(h.action, inputType);
        glyphFont.setPixelSize(g.fontSize);
        QFontMetrics gfm(glyphFont);
        int pillW = gfm.horizontalAdvance(g.text) + pillPadX * 2;

        labelFont.setPixelSize(14);
        QFontMetrics lfm(labelFont);
        int labelW = lfm.horizontalAdvance(h.label);

        measured.append({g, h.label, pillW, labelW});
        totalWidth += pillW + labelGap + labelW;
    }
    totalWidth += hintSpacing * (m_hints.size() - 1);

    // Draw hints row centered horizontally, at the bottom of the widget
    int y = height() - pillHeight - 10;
    int x = (width() - totalWidth) / 2;

    for (const auto& mh : measured) {
        // Draw pill
        QRectF pillRect(x, y, mh.pillWidth, pillHeight);
        p.setPen(QPen(mh.glyph.border, 1));
        p.setBrush(mh.glyph.bg);
        p.drawRoundedRect(pillRect, pillRadius, pillRadius);

        glyphFont.setPixelSize(mh.glyph.fontSize);
        p.setFont(glyphFont);
        p.setPen(mh.glyph.fg);
        p.drawText(pillRect, Qt::AlignCenter, mh.glyph.text);

        // Draw label
        x += mh.pillWidth + labelGap;
        labelFont.setPixelSize(14);
        p.setFont(labelFont);
        p.setPen(QColor("#dddddd"));
        QRectF labelRect(x, y, mh.labelWidth, pillHeight);
        p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, mh.label);

        x += mh.labelWidth + hintSpacing;
    }
}

QString Pcsx2DescriptionBar::descText() const { return m_text->text(); }
QString Pcsx2DescriptionBar::recommendedText() const { return m_rec->text(); }
```

- [ ] **Step 5: Run tests to verify they pass**

Run:
```bash
cd cpp && cmake --build build --target test_pcsx2_description_bar 2>&1 | tail -5
./build/test_pcsx2_description_bar 2>&1
```
Expected: All 6 tests pass (4 existing + 2 new).

- [ ] **Step 6: Commit**

```bash
git add src/ui/settings/pcsx2/widgets/pcsx2_description_bar.h \
        src/ui/settings/pcsx2/widgets/pcsx2_description_bar.cpp \
        tests/test_pcsx2_description_bar.cpp
git commit -m "feat(pcsx2-settings): add button hints rendering to description bar"
```

---

### Task 4: Change Key Behavior in Pcsx2SettingsDialog

**Files:**
- Modify: `src/ui/settings/pcsx2/pcsx2_settings_dialog.h`
- Modify: `src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp`

- [ ] **Step 1: Update the header**

Replace the entire contents of `src/ui/settings/pcsx2/pcsx2_settings_dialog.h` with:

```cpp
#pragma once
#include <QDialog>
#include <QStack>
#include "core/setting_def.h"

class AppController;
class QStackedWidget;
class Pcsx2DescriptionBar;
class Pcsx2CategoryHub;

class Pcsx2SettingsDialog : public QDialog {
    Q_OBJECT
public:
    Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

    // Navigation API used by child pages
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
    Pcsx2CategoryHub* m_hub = nullptr;
    QStack<int> m_history;
    bool m_currentPageHasSubTabs = false;
};
```

- [ ] **Step 2: Update the implementation**

Replace the entire contents of `src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp` with:

```cpp
#include "pcsx2_settings_dialog.h"
#include "pcsx2_category_hub.h"
#include "pages/pcsx2_emulation_page.h"
#include "pages/pcsx2_audio_page.h"
#include "pages/pcsx2_memory_cards_page.h"
#include "pages/pcsx2_graphics_page.h"
#include "widgets/pcsx2_card.h"
#include "widgets/pcsx2_description_bar.h"
#include "pcsx2_theme.h"
#include "ui/app_controller.h"
#include "core/sdl_input_manager.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>

Pcsx2SettingsDialog::Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent)
    : QDialog(parent), m_app(app), m_emuId(emuId) {
    setWindowTitle("PCSX2 Settings");
    setMinimumSize(950, 550);
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(Pcsx2Theme::windowBg().name()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_descBar = new Pcsx2DescriptionBar(this);
    m_descBar->setInputManager(app->sdlInputManager());

    m_hub = new Pcsx2CategoryHub(this);
    connect(m_hub, &Pcsx2CategoryHub::categoryActivated,
            this, &Pcsx2SettingsDialog::onCategoryActivated);
    connect(m_hub, &Pcsx2CategoryHub::openNativeRequested, this,
            [this]{ m_app->openNativeEmulatorSettings(m_emuId); });
    m_stack->addWidget(m_hub);

    // Description bar is only meaningful on settings pages — hide it on the hub.
    // But we still show hints on the hub via the always-visible hints row.
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int index) {
        bool onHub = (m_stack->widget(index) == m_hub);
        m_descBar->setVisible(true); // Always visible now — shows hints even on hub
        if (onHub) {
            m_descBar->clear();
        }
        applyHintsForCurrentPage();
    });

    root->addWidget(m_stack, 1);
    root->addWidget(m_descBar, 0);

    // Initial state: hub is active, show hub hints
    applyHintsForCurrentPage();
}

void Pcsx2SettingsDialog::pushPage(QWidget* page, bool hasSubTabs) {
    m_currentPageHasSubTabs = hasSubTabs;
    int idx = m_stack->addWidget(page);
    m_history.push(m_stack->currentIndex());
    m_stack->setCurrentIndex(idx);
    if (size().width() < 1000 || size().height() < 700) {
        resize(1000, 700);
    }
    clearFocusedSetting();

    // Auto-focus the first focusable Pcsx2Card so arrow keys work
    // immediately.  Skip NoFocus cards (compound containers).
    for (auto* card : page->findChildren<Pcsx2Card*>()) {
        if (card->focusPolicy() != Qt::NoFocus) {
            card->setFocus(Qt::OtherFocusReason);
            break;
        }
    }
}

void Pcsx2SettingsDialog::popPage() {
    if (m_history.isEmpty()) { accept(); return; }
    QWidget* current = m_stack->currentWidget();
    int prev = m_history.pop();
    m_stack->setCurrentIndex(prev);
    m_currentPageHasSubTabs = false;
    if (m_stack->currentWidget() == m_hub) {
        resize(950, 550);
    }
    if (current && current != m_hub) { m_stack->removeWidget(current); current->deleteLater(); }
    clearFocusedSetting();
}

void Pcsx2SettingsDialog::setFocusedSetting(const SettingDef& def) { m_descBar->setSetting(def); }
void Pcsx2SettingsDialog::clearFocusedSetting() { m_descBar->clear(); }

void Pcsx2SettingsDialog::keyPressEvent(QKeyEvent* e) {
    // Escape and B-button (Key_Back) both act as hierarchical back.
    // On the hub, popPage() calls accept() which closes the dialog.
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Back) {
        popPage();
        return;
    }
    // Suppress Tab/Backtab on pages without sub-tabs so L1/R1 don't
    // accidentally move widget focus.
    if ((e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) &&
        !m_currentPageHasSubTabs && m_stack->currentWidget() != m_hub) {
        e->accept();
        return;
    }
    QDialog::keyPressEvent(e);
}

void Pcsx2SettingsDialog::applyHintsForCurrentPage() {
    using BH = Pcsx2DescriptionBar::ButtonHint;

    if (m_stack->currentWidget() == m_hub) {
        m_descBar->setHints({
            BH{"navigate_ud", "Navigate"},
            BH{"confirm",     "Select"},
            BH{"back",        "Close"},
        });
    } else if (m_currentPageHasSubTabs) {
        m_descBar->setHints({
            BH{"navigate",    "Navigate"},
            BH{"confirm",     "Select"},
            BH{"switch_tab",  "Switch Tab"},
            BH{"back",        "Back"},
        });
    } else {
        m_descBar->setHints({
            BH{"navigate",    "Navigate"},
            BH{"confirm",     "Select"},
            BH{"back",        "Back"},
        });
    }
}

void Pcsx2SettingsDialog::onCategoryActivated(const QString& category) {
    if (category == "Emulation") {
        auto* page = new Pcsx2EmulationPage(this);
        connect(page, &Pcsx2EmulationPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Audio") {
        auto* page = new Pcsx2AudioPage(this);
        connect(page, &Pcsx2AudioPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Memory Cards") {
        auto* page = new Pcsx2MemoryCardsPage(this);
        connect(page, &Pcsx2MemoryCardsPage::settingFocused, this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
        return;
    }
    if (category == "Graphics") {
        auto* page = new Pcsx2GraphicsPage(this);
        connect(page, &Pcsx2GraphicsPage::settingFocused,
                this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page, true);  // hasSubTabs = true
        return;
    }
    // Emulation / Audio / Memory Cards branches wired in Tasks 14-16.
}
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/ui/settings/pcsx2/pcsx2_settings_dialog.h \
        src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
git commit -m "feat(pcsx2-settings): Escape as hierarchical back, per-page button hints"
```

---

### Task 5: Manual Verification

- [ ] **Step 1: Launch the app**

Run:
```bash
cd cpp && open ./build/RetroNest.app
```

- [ ] **Step 2: Verify hub page hints**

1. Navigate to PCSX2 settings
2. Verify the description bar at the bottom shows hint pills: `D-Pad ▴▾ Navigate` | `Enter Select` | `Esc Close` (keyboard mode)
3. If a controller is connected, verify it switches to Xbox/PlayStation glyphs

- [ ] **Step 3: Verify sub-page hints (no tabs)**

1. Select "Emulation" from the hub
2. Verify hints show: `▴▾◂▸ Navigate` | `Enter Select` | `Esc Back`
3. Press Escape — verify it goes back to hub (not close dialog)
4. Verify description text updates when focusing settings

- [ ] **Step 4: Verify sub-page hints (with tabs)**

1. Select "Graphics" from the hub
2. Verify hints show: `▴▾◂▸ Navigate` | `Enter Select` | `Tab Switch Tab` | `Esc Back`
3. Press Tab — verify it cycles through Display/Rendering/Post-Proc/OSD tabs
4. Press Escape — verify it goes back to hub

- [ ] **Step 5: Verify hub close**

1. From the hub, press Escape
2. Verify the dialog closes entirely

- [ ] **Step 6: Verify controller input (if controller available)**

1. Connect a controller
2. Verify hint pills update to show Xbox/PlayStation glyphs
3. Verify L1/R1 cycle tabs on Graphics page
4. Verify B button acts as back

- [ ] **Step 7: Commit verification note**

If any visual tweaks were needed during verification, commit them:
```bash
git add -A
git commit -m "fix(pcsx2-settings): button hints visual adjustments from manual testing"
```
