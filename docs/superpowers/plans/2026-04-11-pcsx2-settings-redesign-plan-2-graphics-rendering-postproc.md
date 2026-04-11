# PCSX2 Settings Redesign — Plan 2: Graphics / Rendering & Post-Processing

**Spec:** `docs/superpowers/specs/2026-04-11-pcsx2-settings-redesign-design.md`
**Plan series:** 2 of 4 (Foundations → **Graphics Rendering + Post-Processing** → Graphics Display → Graphics OSD & Polish)
**Date:** 2026-04-11
**Branch:** `feat/pcsx2-settings-plan-2-graphics-rendering-postproc` (already checked out)

## For agentic workers

Plan 1 is already merged to `main`. Read `docs/superpowers/plans/2026-04-11-pcsx2-settings-redesign-plan-1-foundations.md` first if you need the foundations — the schema `recommendedValue` field, `Pcsx2Theme`, every widget primitive (`Pcsx2Card`, `Pcsx2Toggle`, `Pcsx2ToggleRow`, `Pcsx2ComboRow`, `Pcsx2SliderRow`, `Pcsx2SectionHeader`, `Pcsx2DescriptionBar`), the `Pcsx2SettingsDialog` shell with push/pop navigation, the `Pcsx2CategoryHub` landing page, and the `Pcsx2EmulationPage` / `Pcsx2AudioPage` / `Pcsx2MemoryCardsPage` are all in place and shipping.

Execute tasks in order. Each task ends in a commit. Do not skip ahead. After each task that touches C++ run:

```
cd cpp && cmake --build build 2>&1 | tail -20
ctest --test-dir build --output-on-failure
```

Plan 2 extends the existing `test_pcsx2_recommended_values` executable and adds no new test binaries, so the total test count stays at 11 (the same as at the end of Plan 1).

**Do NOT implement in this plan:** Display sub-page (real), OSD sub-page (real), preview widgets, DuckStation/PPSSPP changes, any schema changes beyond `recommendedValue`.

## Goal

Ship the Graphics category container page with stacked-icon sub-tabs, and deliver the first two of its four sub-pages (Rendering, Post-Processing) as fully functional schema-driven screens. Display and OSD sub-tabs show a placeholder that bounces the user to the legacy `EmulatorSettingsPage`; Plans 3 and 4 replace those stubs.

After Plan 2, drilling into the Graphics card in the PCSX2 dialog enters `Pcsx2GraphicsPage` (no longer the legacy page), and the user can:

- Change Internal Resolution, Texture/Trilinear/Anisotropic filtering, Dithering, Blending Accuracy, and Hardware Mipmapping on the Rendering sub-tab, with values round-tripping through `AppController`.
- Change CAS mode / sharpness, FXAA, TV Shader, Shade Boost, and Shade Boost Brightness/Contrast/Saturation sliders on the Post-Processing sub-tab.
- Watch the three Shade Boost sliders grey out when Shade Boost is off and re-enable when it's on.
- Hit Display or OSD tabs and see a "Coming in a later update" stub with a button that opens the legacy page.
- See the description bar update on focus with tooltip + amber recommended pill, exactly like the Plan 1 pages.

## Architecture

```
Pcsx2SettingsDialog
  └── QStackedWidget
        ├── Pcsx2CategoryHub              (unchanged from Plan 1)
        ├── Pcsx2EmulationPage            (unchanged)
        ├── Pcsx2AudioPage                (unchanged)
        ├── Pcsx2MemoryCardsPage          (unchanged)
        └── Pcsx2GraphicsPage             (NEW — Plan 2)
              ├── Pcsx2GraphicsSubTabBar  (NEW — Display / Rendering / Post-Proc / OSD)
              └── QStackedWidget (eager)
                    ├── Pcsx2GraphicsStubSubPage  (Display — Plan 3 replaces)
                    ├── Pcsx2GraphicsRenderingPage       (NEW — Plan 2)
                    ├── Pcsx2GraphicsPostProcessingPage  (NEW — Plan 2)
                    └── Pcsx2GraphicsStubSubPage  (OSD — Plan 4 replaces)
```

`Pcsx2GraphicsPage` re-emits `settingFocused(SettingDef)` from each active sub-page up to the dialog's `setFocusedSetting` slot, mirroring the Plan 1 pattern.

The sub-tab bar is keyboard-driven: Left/Right moves the selection, Enter/Return activates. It is `Qt::StrongFocus` and participates in the normal arrow-key focus chain (no unified-input hooks needed — Plan 1 proved the default Qt focus chain is sufficient for PCSX2 pages).

## Tech Stack

- **Language:** C++17
- **UI framework:** Qt6 Widgets (`QDialog` / `QWidget` / `QStackedWidget`), `AUTOMOC ON`
- **Styling:** stylesheet strings via `Pcsx2Theme`; `paintEvent` only where stylesheet can't express it (sub-tab bar underline)
- **Build system:** CMake 3.16, explicit source lists in `cpp/CMakeLists.txt`
- **Tests:** QtTest schema assertions in the existing `test_pcsx2_recommended_values` executable
- **Palette:** inherited from `cpp/src/ui/settings/pcsx2/pcsx2_theme.h` (Plan 1)

---

## Task 1: Populate `recommendedValue` for Graphics/Rendering settings (TDD)

**Files:**
- `cpp/tests/test_pcsx2_recommended_values.cpp` (modify)
- `cpp/src/adapters/pcsx2_adapter.cpp` (modify)

Plan 1 established the pattern: `{ SettingDef d{...}; d.recommendedValue = "X"; s.append(d); }`. Apply the same pattern to Graphics/Rendering.

- [ ] Add a new test slot to `cpp/tests/test_pcsx2_recommended_values.cpp` after `testMemoryCardsAllHaveRecommended`:

```cpp
    void testGraphicsRenderingSettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Graphics") continue;
            if (d.subcategory != "Rendering") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Graphics/Rendering/%1").arg(d.key)));
        }
        QVERIFY2(count >= 7,
                 qPrintable(QString("expected >= 7 Graphics/Rendering settings, got %1").arg(count)));
    }
```

- [ ] Build + run the test — expect FAIL because none of the Rendering entries have been converted yet:

```
cd cpp && cmake --build build --target test_pcsx2_recommended_values \
  && ctest --test-dir build -R Pcsx2RecommendedValues --output-on-failure
```

Expected failure: `missing recommendedValue for Graphics/Rendering/upscale_multiplier`.

- [ ] Edit `cpp/src/adapters/pcsx2_adapter.cpp`. Locate the `// Graphics > Rendering` section (starts around line 215). Replace the 7 positional `s.append({...})` calls with scoped blocks that set `recommendedValue` explicitly. Recommendations mirror PCSX2 upstream `Pcsx2Config::GSOptions` defaults (master branch `pcsx2/Config.h`), which in turn match our schema `defaultValue`:

```cpp
    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Rendering
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "upscale_multiplier", "Internal Resolution", "",
                     SettingDef::Combo, "1",
                     {{"Native (PS2) (Default)", "1"}, {"2x Native (~720px/HD)", "2"}, {"3x Native (~1080px/FHD)", "3"},
                      {"4x Native (~1440px/QHD)", "4"}, {"5x Native (~1800px/QHD+)", "5"}, {"6x Native (~2160px/4K UHD)", "6"},
                      {"7x Native (~2520px)", "7"}, {"8x Native (~2880px/5K UHD)", "8"}, {"9x Native (~3240px)", "9"},
                      {"10x Native (~3600px/6K UHD)", "10"}, {"11x Native (~3960px)", "11"}, {"12x Native (~4320px/8K UHD)", "12"}}, 0, 0, 0};
        d.recommendedValue = "1"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "filter", "Texture Filtering", "",
                     SettingDef::Combo, "2",
                     {{"Nearest", "0"}, {"Bilinear (Forced)", "1"}, {"Bilinear (PS2)", "2"}, {"Bilinear (Forced excluding sprite)", "3"}}, 0, 0, 0};
        d.recommendedValue = "2"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "TriFilter", "Trilinear Filtering", "",
                     SettingDef::Combo, "-1",
                     {{"Auto (Default)", "-1"}, {"Off", "0"}, {"Trilinear (PS2)", "1"}, {"Trilinear (Forced)", "2"}}, 0, 0, 0};
        d.recommendedValue = "-1"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "MaxAnisotropy", "Anisotropic Filtering", "",
                     SettingDef::Combo, "0",
                     {{"Off", "0"}, {"2x", "2"}, {"4x", "4"}, {"8x", "8"}, {"16x", "16"}}, 0, 0, 0};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "dithering_ps2", "Dithering", "",
                     SettingDef::Combo, "2",
                     {{"Off", "0"}, {"Scaled", "1"}, {"Unscaled (Default)", "2"}, {"Force 32bit", "3"}}, 0, 0, 0};
        d.recommendedValue = "2"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "accurate_blending_unit", "Blending Accuracy", "",
                     SettingDef::Combo, "1",
                     {{"Minimum", "0"}, {"Basic (Default)", "1"}, {"Medium", "2"}, {"High", "3"}, {"Full", "4"}, {"Maximum", "5"}}, 0, 0, 0};
        d.recommendedValue = "1"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Rendering", "", "EmuCore/GS", "hw_mipmap", "Mipmapping",
                     "Enables mipmapping which improves texture quality at the cost of performance.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true"; s.append(d);
    }
```

Note: every recommendedValue above equals defaultValue because upstream PCSX2 has no separate "recommendation" metadata — the defaults already represent the recommended baseline. Per the Plan 1 convention, we copy `defaultValue` into `recommendedValue` verbatim.

- [ ] Re-run the test — expect PASS:

```
cd cpp && cmake --build build --target test_pcsx2_recommended_values \
  && ctest --test-dir build -R Pcsx2RecommendedValues --output-on-failure
```

- [ ] Commit:

```
git add cpp/src/adapters/pcsx2_adapter.cpp cpp/tests/test_pcsx2_recommended_values.cpp
git commit -m "Populate PCSX2 Graphics/Rendering recommendedValue

Seven Rendering settings (Internal Resolution, Texture / Trilinear /
Anisotropic filters, Dithering, Blending Accuracy, Mipmapping) now
carry recommendedValue mirroring upstream Pcsx2Config::GSOptions
defaults. Guarded by a new test slot in test_pcsx2_recommended_values."
```

---

## Task 2: Populate `recommendedValue` for Graphics/Post-Processing settings (TDD)

**Files:**
- `cpp/tests/test_pcsx2_recommended_values.cpp` (modify)
- `cpp/src/adapters/pcsx2_adapter.cpp` (modify)

- [ ] Add the test slot after `testGraphicsRenderingSettingsAllHaveRecommended`:

```cpp
    void testGraphicsPostProcessingSettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Graphics") continue;
            if (d.subcategory != "Post-Processing") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Graphics/Post-Processing/%1").arg(d.key)));
        }
        QVERIFY2(count >= 7,
                 qPrintable(QString("expected >= 7 Graphics/Post-Processing settings, got %1").arg(count)));
    }
```

- [ ] Build + run — expect FAIL on `CASMode`.

- [ ] Replace the `// Graphics > Post-Processing` section's 8 `s.append({...})` calls with scoped blocks. Recommendations mirror upstream defaults (all match schema `defaultValue`):

```cpp
    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Post-Processing
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Post-Processing", "Sharpening/Anti-Aliasing", "EmuCore/GS", "CASMode", "Contrast Adaptive Sharpening", "",
                     SettingDef::Combo, "0",
                     {{"None (Default)", "0"}, {"Sharpen Only (Internal Resolution)", "1"},
                      {"Sharpen and Resize (Display Resolution)", "2"}}, 0, 0, 0};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Sharpening/Anti-Aliasing", "EmuCore/GS", "CASSharpness", "Sharpness", "",
                     SettingDef::Int, "50", {}, 0, 100, 1, "", "%"};
        d.recommendedValue = "50"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Sharpening/Anti-Aliasing", "EmuCore/GS", "fxaa", "FXAA",
                     "Enables Fast Approximate Anti-Aliasing.", SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "TVShader", "TV Shader", "",
                     SettingDef::Combo, "0",
                     {{"None (Default)", "0"}, {"Scanline Filter", "1"}, {"Diagonal Filter", "2"}, {"Triangular Filter", "3"},
                      {"Wave Filter", "4"}, {"Lottes CRT", "5"},
                      {"4xRGSS downsampling (4x Rotated Grid SuperSampling)", "6"},
                      {"NxAGSS downsampling (Nx Automatic Grid SuperSampling)", "7"}}, 0, 0, 0};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost", "Shade Boost",
                     "Enables manual adjustment of display brightness, contrast, and saturation.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Brightness", "Brightness", "",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        d.recommendedValue = "50"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Contrast", "Contrast", "",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        d.recommendedValue = "50"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Post-Processing", "Filters", "EmuCore/GS", "ShadeBoost_Saturation", "Saturation", "",
                     SettingDef::Int, "50", {}, 1, 100, 1, "paired", "", "ShadeBoost"};
        d.recommendedValue = "50"; s.append(d);
    }
```

Note: the schema intentionally has **no `ShadeBoost_Gamma` entry**. When we build the Post-Processing page in Task 7 we skip rendering a Gamma slider rather than inventing a setting. If a future plan adds Gamma to the schema, extend the test count and the page layout then — not now.

- [ ] Re-run the test — expect PASS.

- [ ] Commit:

```
git add cpp/src/adapters/pcsx2_adapter.cpp cpp/tests/test_pcsx2_recommended_values.cpp
git commit -m "Populate PCSX2 Graphics/Post-Processing recommendedValue

Eight Post-Processing settings (CAS mode/sharpness, FXAA, TV Shader,
Shade Boost + Brightness/Contrast/Saturation) now carry
recommendedValue. ShadeBoost_Gamma is intentionally absent from the
schema and not added in this plan."
```

---

## Task 3: Create `Pcsx2GraphicsSubTabBar` widget

**Files:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_graphics_sub_tab_bar.h` (create)
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_graphics_sub_tab_bar.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

Stacked-icon box tabs. Each tab is a vertical stack: a 24 px emoji icon on top, a 12 px label below. Active tab draws a 3 px amber underline and a slightly lighter background. Arrow Left/Right moves the selection; Enter activates (emits `tabActivated`). Focus moves to the bar as a whole — individual tabs are not separate focusable widgets.

- [ ] `pcsx2_graphics_sub_tab_bar.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>

// Four-box sub-tab bar used by Pcsx2GraphicsPage. Each tab is a
// vertical icon-over-label stack rendered inside a rounded box.
// Keyboard-focusable; Left/Right changes selection, Enter activates.
class Pcsx2GraphicsSubTabBar : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsSubTabBar(QWidget* parent = nullptr);

    // Each call adds a tab at the next index. Icon is a short emoji
    // string; label is the tab title shown beneath.
    void addTab(const QString& icon, const QString& label);

    int currentIndex() const { return m_current; }
    void setCurrentIndex(int idx);

    QSize sizeHint() const override;

signals:
    // Emitted whenever the selection changes — either via keyboard or
    // mouse click. Consumer swaps a QStackedWidget's current index.
    void tabActivated(int index);

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    struct Tab { QString icon; QString label; };
    QVector<Tab> m_tabs;
    int m_current = 0;

    QRect tabRectAt(int idx) const;
};
```

- [ ] `pcsx2_graphics_sub_tab_bar.cpp`:

```cpp
#include "pcsx2_graphics_sub_tab_bar.h"
#include "../pcsx2_theme.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFontMetrics>

namespace {
constexpr int kTabWidth  = 120;
constexpr int kTabHeight = 64;
constexpr int kGap       = 10;
constexpr int kIconSize  = 22;
}

Pcsx2GraphicsSubTabBar::Pcsx2GraphicsSubTabBar(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumHeight(kTabHeight + 6);
    setCursor(Qt::PointingHandCursor);
}

void Pcsx2GraphicsSubTabBar::addTab(const QString& icon, const QString& label) {
    m_tabs.append({icon, label});
    updateGeometry();
    update();
}

void Pcsx2GraphicsSubTabBar::setCurrentIndex(int idx) {
    if (idx < 0 || idx >= m_tabs.size()) return;
    if (idx == m_current) return;
    m_current = idx;
    update();
    emit tabActivated(m_current);
}

QSize Pcsx2GraphicsSubTabBar::sizeHint() const {
    const int count = m_tabs.size();
    if (count == 0) return QSize(kTabWidth, kTabHeight + 6);
    return QSize(count * kTabWidth + (count - 1) * kGap, kTabHeight + 6);
}

QRect Pcsx2GraphicsSubTabBar::tabRectAt(int idx) const {
    return QRect(idx * (kTabWidth + kGap), 0, kTabWidth, kTabHeight);
}

void Pcsx2GraphicsSubTabBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < m_tabs.size(); ++i) {
        const QRect r = tabRectAt(i);
        const bool active = (i == m_current);

        // Box background
        QColor bg = active ? Pcsx2Theme::cardBg().lighter(110) : Pcsx2Theme::cardBg();
        QColor border = active ? Pcsx2Theme::accent() : Pcsx2Theme::cardBorder();
        p.setPen(QPen(border, active ? 1.5 : 1.0));
        p.setBrush(bg);
        p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 8, 8);

        // Icon (centered, upper half)
        p.setPen(Pcsx2Theme::textPrimary());
        QFont iconFont = p.font();
        iconFont.setPointSize(kIconSize);
        p.setFont(iconFont);
        QRect iconRect(r.x(), r.y() + 8, r.width(), kIconSize + 6);
        p.drawText(iconRect, Qt::AlignHCenter | Qt::AlignVCenter, m_tabs[i].icon);

        // Label (lower half)
        QFont labelFont = p.font();
        labelFont.setPointSize(10);
        labelFont.setBold(active);
        p.setFont(labelFont);
        p.setPen(active ? Pcsx2Theme::textPrimary() : Pcsx2Theme::textSecondary());
        QRect labelRect(r.x(), r.y() + kTabHeight - 22, r.width(), 18);
        p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, m_tabs[i].label);

        // Amber underline for active tab
        if (active) {
            p.setPen(Qt::NoPen);
            p.setBrush(Pcsx2Theme::accent());
            QRect underline(r.x() + 12, r.bottom() + 2, r.width() - 24, 3);
            p.drawRoundedRect(underline, 1.5, 1.5);
        }
    }

    // Focus ring around the whole bar when the widget has focus
    if (hasFocus()) {
        QColor halo = Pcsx2Theme::accent();
        halo.setAlphaF(0.35);
        p.setPen(QPen(halo, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 10, 10);
    }
}

void Pcsx2GraphicsSubTabBar::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_Left:
            if (m_current > 0) setCurrentIndex(m_current - 1);
            return;
        case Qt::Key_Right:
            if (m_current < m_tabs.size() - 1) setCurrentIndex(m_current + 1);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            emit tabActivated(m_current);
            return;
        default:
            QWidget::keyPressEvent(e);
    }
}

void Pcsx2GraphicsSubTabBar::mousePressEvent(QMouseEvent* e) {
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (tabRectAt(i).contains(e->pos())) {
            setFocus();
            setCurrentIndex(i);
            return;
        }
    }
    QWidget::mousePressEvent(e);
}

void Pcsx2GraphicsSubTabBar::focusInEvent(QFocusEvent* e) { QWidget::focusInEvent(e); update(); }
void Pcsx2GraphicsSubTabBar::focusOutEvent(QFocusEvent* e) { QWidget::focusOutEvent(e); update(); }
```

- [ ] Register in `cpp/CMakeLists.txt`. Add `.cpp` to the main SOURCES list and `.h` to the HEADERS list (insert alphabetically near the other `widgets/pcsx2_*` entries).

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

Expected: builds cleanly. No test — pure presentation.

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/widgets/pcsx2_graphics_sub_tab_bar.h \
        cpp/src/ui/settings/pcsx2/widgets/pcsx2_graphics_sub_tab_bar.cpp \
        cpp/CMakeLists.txt
git commit -m "Add Pcsx2GraphicsSubTabBar widget

Four-box stacked-icon tab bar used by the new Graphics page. Left/Right
arrow keys change the selection, Enter activates. Active tab draws an
amber underline and a lifted background; the whole bar draws an amber
focus halo when it owns keyboard focus."
```

---

## Task 4: Create `Pcsx2GraphicsStubSubPage` placeholder

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_stub_sub_page.h` (create)
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_stub_sub_page.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

Used for Display (Plan 3 replaces) and OSD (Plan 4 replaces). Shows a centered "Coming in a later update" label and a single push button that opens a legacy `EmulatorSettingsPage` as a modal, exactly like the Plan 1 Graphics-card fallback.

- [ ] `pcsx2_graphics_stub_sub_page.h`:

```cpp
#pragma once
#include <QWidget>

class AppController;

// Placeholder sub-page used for Graphics/Display and Graphics/OSD in
// Plan 2. Plans 3 and 4 replace these with real pages. Shows a
// "Coming in a later update" label and a button that opens the legacy
// EmulatorSettingsPage as a modal escape hatch.
class Pcsx2GraphicsStubSubPage : public QWidget {
    Q_OBJECT
public:
    Pcsx2GraphicsStubSubPage(AppController* app,
                             const QString& emuId,
                             const QString& subTabName,
                             QWidget* parent = nullptr);

private slots:
    void openLegacyDialog();

private:
    AppController* m_app;
    QString m_emuId;
    QString m_subTabName;
};
```

- [ ] `pcsx2_graphics_stub_sub_page.cpp`:

```cpp
#include "pcsx2_graphics_stub_sub_page.h"
#include "../pcsx2_theme.h"
#include "ui/settings/emulator_settings_page.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

Pcsx2GraphicsStubSubPage::Pcsx2GraphicsStubSubPage(AppController* app,
                                                   const QString& emuId,
                                                   const QString& subTabName,
                                                   QWidget* parent)
    : QWidget(parent), m_app(app), m_emuId(emuId), m_subTabName(subTabName) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 40, 24, 40);
    root->setSpacing(14);
    root->addStretch();

    auto* title = new QLabel(QStringLiteral("%1 — Coming in a later update").arg(m_subTabName), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#f2efe8;font-size:18px;font-weight:600;");

    auto* sub = new QLabel(
        "This sub-page is still being redesigned. Use the legacy settings\n"
        "dialog in the meantime — all settings remain fully functional.",
        this);
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color:#d0ccc4;font-size:13px;");

    auto* btn = new QPushButton("Open in legacy settings", this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton {"
        "  background:#4a4642; color:#f2efe8;"
        "  border:1px solid #706c66; border-radius:4px;"
        "  padding:8px 18px; font-size:13px;"
        "}"
        "QPushButton:focus { border-color:#f59e0b; }"
        "QPushButton:hover { background:#585450; }");
    connect(btn, &QPushButton::clicked, this, &Pcsx2GraphicsStubSubPage::openLegacyDialog);

    root->addWidget(title);
    root->addWidget(sub);
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(btn);
    btnRow->addStretch();
    root->addLayout(btnRow);
    root->addStretch();
}

void Pcsx2GraphicsStubSubPage::openLegacyDialog() {
    auto* legacy = new EmulatorSettingsPage(m_app, m_emuId);
    legacy->setAttribute(Qt::WA_DeleteOnClose);
    legacy->setWindowModality(Qt::ApplicationModal);
    legacy->show();
}
```

Note: also add `#include <QHBoxLayout>` at the top of the `.cpp`.

- [ ] Register both files in `cpp/CMakeLists.txt` (SOURCES / HEADERS).

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_stub_sub_page.h \
        cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_stub_sub_page.cpp \
        cpp/CMakeLists.txt
git commit -m "Add Pcsx2GraphicsStubSubPage placeholder

Used by the Plan 2 Graphics page for the Display and OSD sub-tabs
until Plans 3 and 4 replace them with real layouts. Mirrors the Plan 1
Graphics-card legacy fallback — one button that opens
EmulatorSettingsPage as a modal."
```

---

## Task 5: Create `Pcsx2GraphicsRenderingPage`

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_rendering_page.h` (create)
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_rendering_page.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

Layout per spec §Graphics/Rendering:

1. Full-width focused card at the top: **Internal Resolution** combo row.
2. Below it, a 2-column `QGridLayout` of 6 cards:
   - Row 0: Texture Filtering | Trilinear Filtering
   - Row 1: Anisotropic Filtering | Dithering
   - Row 2: Blending Accuracy | Hardware Mipmapping (toggle)

Each card is a `Pcsx2Card` wrapping a `Pcsx2ComboRow` or `Pcsx2ToggleRow`. Every row calls `setSettingDef(*d)` and emits `focused()` → which the page re-emits as `settingFocused`. Full load/save round-trip via `AppController::settingValue` / `saveSettings`, same as Plan 1.

We copy the `makeComboCard` / `makeToggleCard` helper lambdas from Plan 1's `Pcsx2EmulationPage` — they are tiny and keep the page self-contained. (An optional future refactor could hoist them into a shared helper header; that's deliberately out of scope for Plan 2.)

- [ ] `pcsx2_graphics_rendering_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;

class Pcsx2GraphicsRenderingPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsRenderingPage(Pcsx2SettingsDialog* dialog);

signals:
    void settingFocused(SettingDef def);

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef> m_schema; // filtered to category=="Graphics" && subcategory=="Rendering"
};
```

- [ ] `pcsx2_graphics_rendering_page.cpp`:

```cpp
#include "pcsx2_graphics_rendering_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QGridLayout>

Pcsx2GraphicsRenderingPage::Pcsx2GraphicsRenderingPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "Rendering")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
}

const SettingDef* Pcsx2GraphicsRenderingPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2GraphicsRenderingPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    // --- helpers (copied from Plan 1 Pcsx2EmulationPage pattern) ---
    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        if (!d) { // graceful no-op if schema is missing an expected key
            return card;
        }
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };

    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        if (!d) return card;
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    // --- Internal Resolution (focused, full-width) ---
    root->addWidget(makeComboCard("upscale_multiplier"));

    // --- 2-column grid of six cards ---
    auto* grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeComboCard("filter"),                  0, 0);
    grid->addWidget(makeComboCard("TriFilter"),               0, 1);
    grid->addWidget(makeComboCard("MaxAnisotropy"),           1, 0);
    grid->addWidget(makeComboCard("dithering_ps2"),           1, 1);
    grid->addWidget(makeComboCard("accurate_blending_unit"),  2, 0);
    grid->addWidget(makeToggleCard("hw_mipmap"),              2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    root->addLayout(grid);
    root->addStretch();
}

void Pcsx2GraphicsRenderingPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* tog : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = tog->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        tog->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
}

void Pcsx2GraphicsRenderingPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

Note: verify the `saveSettings` payload shape against Plan 1's `Pcsx2EmulationPage::saveValue`. It uses the `"section/key"` flat-map form — match that exactly.

- [ ] Register both files in CMake SOURCES/HEADERS.

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

Expected: compiles cleanly but page is not yet reachable from the dialog (Task 8 wires it up).

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_rendering_page.h \
        cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_rendering_page.cpp \
        cpp/CMakeLists.txt
git commit -m "Add Pcsx2GraphicsRenderingPage

Full-width Internal Resolution card plus 2-column grid of six cards
(Texture / Trilinear / Anisotropic filters, Dithering, Blending
Accuracy, Mipmapping toggle). Loads and saves values through
AppController matching the Plan 1 Emulation page pattern."
```

---

## Task 6: Create `Pcsx2GraphicsPostProcessingPage` with `dependsOn` handling

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_post_processing_page.h` (create)
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_post_processing_page.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

Layout per spec §Graphics/Post-Processing:

1. Section header "Sharpening / Anti-Aliasing"
2. Full-width card containing:
   - Row 1: Contrast Adaptive Sharpening combo (alone, full row).
   - Row 2: a 2-column `QHBoxLayout` — Sharpness slider on the left (stretch 1), FXAA toggle on the right (stretch 0).
3. Section header "Filters"
4. Full-width card containing:
   - Row 1: a 2-column `QHBoxLayout` — TV Shader combo on the left (stretch 1), Shade Boost toggle on the right (stretch 0).
   - Row 2: a 2×2 `QGridLayout` of sliders — Brightness | Contrast on row 0, Saturation | Gamma on row 1. **Skip Gamma entirely if `findDef("ShadeBoost_Gamma")` returns null** — leave the slot empty, don't stub with a disabled placeholder. Current schema has no Gamma, so the grid will be Brightness | Contrast / Saturation | (empty).

The four sliders' `SettingDef` carries `dependsOn = "ShadeBoost"` in the schema (verified in `pcsx2_adapter.cpp` around line 265). `refreshDependencies()` walks all `Pcsx2SliderRow*` children, checks each one's `settingDef().dependsOn`, looks up the master toggle state (here: the Shade Boost toggle row), and calls `setEnabled(masterIsOn)` on the *containing card* so both label and slider grey out.

- [ ] `pcsx2_graphics_post_processing_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include <QHash>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class Pcsx2ToggleRow;

class Pcsx2GraphicsPostProcessingPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsPostProcessingPage(Pcsx2SettingsDialog* dialog);

signals:
    void settingFocused(SettingDef def);

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;
    bool masterToggleState(const QString& masterKey) const;
    void refreshDependencies();

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef> m_schema; // filtered to category=="Graphics" && subcategory=="Post-Processing"
    // Map dependsOn master key -> toggle row widget, populated in buildUi().
    QHash<QString, Pcsx2ToggleRow*> m_masterToggles;
};
```

- [ ] `pcsx2_graphics_post_processing_page.cpp`:

```cpp
#include "pcsx2_graphics_post_processing_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_section_header.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../widgets/pcsx2_slider_row.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

Pcsx2GraphicsPostProcessingPage::Pcsx2GraphicsPostProcessingPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "Post-Processing")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    refreshDependencies();
}

const SettingDef* Pcsx2GraphicsPostProcessingPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

bool Pcsx2GraphicsPostProcessingPage::masterToggleState(const QString& masterKey) const {
    auto it = m_masterToggles.find(masterKey);
    if (it == m_masterToggles.end()) return true; // no master found → leave enabled
    return it.value()->isChecked();
}

void Pcsx2GraphicsPostProcessingPage::refreshDependencies() {
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        if (d.dependsOn.isEmpty()) continue;
        const bool enabled = masterToggleState(d.dependsOn);
        // Walk up to the containing Pcsx2Card and disable it — grays label
        // and track via Qt's default disabled-state rendering.
        QWidget* card = slider->parentWidget();
        while (card && qobject_cast<Pcsx2Card*>(card) == nullptr) {
            card = card->parentWidget();
        }
        if (card) card->setEnabled(enabled);
    }
}

void Pcsx2GraphicsPostProcessingPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(10);

    // --- helpers: inline so this page stays self-contained ---
    auto makeComboRow = [this](const QString& key) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ComboRow(this);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2GraphicsPostProcessingPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        return row;
    };

    auto makeToggleRow = [this](const QString& key) -> Pcsx2ToggleRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ToggleRow(this);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsPostProcessingPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            // Any toggle change may alter dependency state.
            refreshDependencies();
        });
        // Register as potential master so dependent sliders can look it up.
        m_masterToggles.insert(key, row);
        return row;
    };

    auto makeSliderCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2SliderRow(card);
        row->setLabel(d->label);
        row->setRange(int(d->minVal), int(d->maxVal));
        row->setSuffix(d->suffix);
        row->setSettingDef(*d);
        connect(row, &Pcsx2SliderRow::focused, this, &Pcsx2GraphicsPostProcessingPage::settingFocused);
        connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
        });
        v->addWidget(row);
        return card;
    };

    // ── Section 1: Sharpening / Anti-Aliasing ─────────────────────────────
    root->addWidget(new Pcsx2SectionHeader("Sharpening / Anti-Aliasing", this));

    auto* sharpCard = new Pcsx2Card(this);
    auto* sharpV = new QVBoxLayout(sharpCard);
    sharpV->setContentsMargins(14, 12, 14, 12);
    sharpV->setSpacing(8);

    if (auto* casCombo = makeComboRow("CASMode"))
        sharpV->addWidget(casCombo);

    auto* sharpRow = new QHBoxLayout();
    sharpRow->setSpacing(16);
    // Sharpness slider (no outer card — it's already inside sharpCard)
    if (const SettingDef* ds = findDef("CASSharpness")) {
        auto* slider = new Pcsx2SliderRow(sharpCard);
        slider->setLabel(ds->label);
        slider->setRange(int(ds->minVal), int(ds->maxVal));
        slider->setSuffix(ds->suffix);
        slider->setSettingDef(*ds);
        connect(slider, &Pcsx2SliderRow::focused, this, &Pcsx2GraphicsPostProcessingPage::settingFocused);
        connect(slider, &Pcsx2SliderRow::valueChanged, this, [this](int val){
            const SettingDef* dd = findDef("CASSharpness");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
        });
        sharpRow->addWidget(slider, 1);
    }
    if (auto* fxaaToggle = makeToggleRow("fxaa"))
        sharpRow->addWidget(fxaaToggle, 0);

    sharpV->addLayout(sharpRow);
    root->addWidget(sharpCard);

    // ── Section 2: Filters ────────────────────────────────────────────────
    root->addWidget(new Pcsx2SectionHeader("Filters", this));

    auto* filterCard = new Pcsx2Card(this);
    auto* filterV = new QVBoxLayout(filterCard);
    filterV->setContentsMargins(14, 12, 14, 12);
    filterV->setSpacing(10);

    // Row 1: TV Shader + Shade Boost side-by-side.
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(16);
    if (auto* tv = makeComboRow("TVShader")) topRow->addWidget(tv, 1);
    if (auto* sb = makeToggleRow("ShadeBoost")) topRow->addWidget(sb, 0);
    filterV->addLayout(topRow);

    // Row 2: 2×2 slider grid. Gamma is skipped if absent from schema.
    auto* sliderGrid = new QGridLayout();
    sliderGrid->setSpacing(10);
    if (auto* c = makeSliderCard("ShadeBoost_Brightness")) sliderGrid->addWidget(c, 0, 0);
    if (auto* c = makeSliderCard("ShadeBoost_Contrast"))   sliderGrid->addWidget(c, 0, 1);
    if (auto* c = makeSliderCard("ShadeBoost_Saturation")) sliderGrid->addWidget(c, 1, 0);
    if (auto* c = makeSliderCard("ShadeBoost_Gamma"))      sliderGrid->addWidget(c, 1, 1);
    // If Gamma is absent (it is in the current schema), cell (1,1) is
    // left empty — QGridLayout handles missing cells gracefully.
    sliderGrid->setColumnStretch(0, 1);
    sliderGrid->setColumnStretch(1, 1);
    filterV->addLayout(sliderGrid);

    root->addWidget(filterCard);
    root->addStretch();
}

void Pcsx2GraphicsPostProcessingPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* tog : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = tog->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        tog->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        bool ok = false;
        int v = cur.toInt(&ok);
        if (!ok) v = d.defaultValue.toInt();
        slider->setValue(v);
    }

    // After values are in place, sync the enabled state of any dependent
    // slider cards against their master toggle.
    refreshDependencies();
}

void Pcsx2GraphicsPostProcessingPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}
```

Important notes for the engineer:

- The three sliders (`ShadeBoost_Brightness/Contrast/Saturation`) each sit inside their own `Pcsx2Card` because `makeSliderCard` wraps them. `refreshDependencies()` walks up from the slider to its containing `Pcsx2Card` and calls `setEnabled(false)` on the card — Qt's default grayed-state rendering handles the visual, so no stylesheet work is needed.
- The Sharpness slider is intentionally *not* inside its own card (it shares the Sharpening/Anti-Aliasing card with CAS combo and FXAA toggle). It has no `dependsOn` in the schema so `refreshDependencies()` skips it.
- Connect Shade Boost's `toggled` to `refreshDependencies()` is handled inside `makeToggleRow`: every toggle change calls `refreshDependencies()` unconditionally. Cheap and correct.

- [ ] Register both files in CMake SOURCES/HEADERS.

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_post_processing_page.h \
        cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_post_processing_page.cpp \
        cpp/CMakeLists.txt
git commit -m "Add Pcsx2GraphicsPostProcessingPage with Shade Boost dependsOn

Sharpening / Anti-Aliasing section with CAS combo row + Sharpness
slider paired with an FXAA toggle on the second row. Filters section
with a TV Shader combo + Shade Boost toggle on the top row, and a
2x2 slider grid for Brightness / Contrast / Saturation / (Gamma).
Gamma is skipped because the current schema has no ShadeBoost_Gamma
entry. The three slider cards disable themselves when Shade Boost is
off via refreshDependencies() walking SettingDef::dependsOn."
```

---

## Task 7: Create `Pcsx2GraphicsPage` container

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.h` (create)
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

This is the umbrella Graphics page. It owns a back button, a sub-tab bar, and a `QStackedWidget` holding the four sub-pages instantiated eagerly (Display stub, Rendering, Post-Processing, OSD stub). Each sub-page that emits `settingFocused` gets re-emitted by this page so the dialog's description bar works correctly.

- [ ] `pcsx2_graphics_page.h`:

```cpp
#pragma once
#include <QWidget>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class Pcsx2GraphicsSubTabBar;
class QStackedWidget;

class Pcsx2GraphicsPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsPage(Pcsx2SettingsDialog* dialog);

signals:
    void settingFocused(SettingDef def);

private slots:
    void onSubTabActivated(int index);

private:
    Pcsx2SettingsDialog* m_dialog;
    Pcsx2GraphicsSubTabBar* m_tabBar = nullptr;
    QStackedWidget* m_stack = nullptr;
};
```

- [ ] `pcsx2_graphics_page.cpp`:

```cpp
#include "pcsx2_graphics_page.h"
#include "pcsx2_graphics_stub_sub_page.h"
#include "pcsx2_graphics_rendering_page.h"
#include "pcsx2_graphics_post_processing_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_graphics_sub_tab_bar.h"
#include "ui/app_controller.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>

Pcsx2GraphicsPage::Pcsx2GraphicsPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 14, 24, 8);
    root->setSpacing(12);

    // --- Back button ---
    auto* back = new QPushButton("← Back", this);
    back->setCursor(Qt::PointingHandCursor);
    back->setStyleSheet(
        "QPushButton { background:transparent; color:#f2efe8; border:none;"
        " font-size:14px; padding:4px 0; }"
        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &Pcsx2SettingsDialog::popPage);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(back, 0, Qt::AlignLeft);
    topRow->addStretch();
    root->addLayout(topRow);

    // --- Sub-tab bar ---
    m_tabBar = new Pcsx2GraphicsSubTabBar(this);
    m_tabBar->addTab(QStringLiteral("🖥"), "Display");
    m_tabBar->addTab(QStringLiteral("🎨"), "Rendering");
    m_tabBar->addTab(QStringLiteral("✨"), "Post-Proc");
    m_tabBar->addTab(QStringLiteral("📊"), "OSD");
    connect(m_tabBar, &Pcsx2GraphicsSubTabBar::tabActivated,
            this, &Pcsx2GraphicsPage::onSubTabActivated);
    root->addWidget(m_tabBar, 0, Qt::AlignLeft);

    // --- Sub-page stack (all four instantiated eagerly) ---
    m_stack = new QStackedWidget(this);

    AppController* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    // 0: Display (stub — Plan 3 replaces)
    auto* displayStub = new Pcsx2GraphicsStubSubPage(app, emuId, "Display", this);
    m_stack->addWidget(displayStub);

    // 1: Rendering (real — Plan 2)
    auto* rendering = new Pcsx2GraphicsRenderingPage(m_dialog);
    connect(rendering, &Pcsx2GraphicsRenderingPage::settingFocused,
            this, &Pcsx2GraphicsPage::settingFocused);
    m_stack->addWidget(rendering);

    // 2: Post-Processing (real — Plan 2)
    auto* postProc = new Pcsx2GraphicsPostProcessingPage(m_dialog);
    connect(postProc, &Pcsx2GraphicsPostProcessingPage::settingFocused,
            this, &Pcsx2GraphicsPage::settingFocused);
    m_stack->addWidget(postProc);

    // 3: OSD (stub — Plan 4 replaces)
    auto* osdStub = new Pcsx2GraphicsStubSubPage(app, emuId, "OSD", this);
    m_stack->addWidget(osdStub);

    root->addWidget(m_stack, 1);

    // Start on Rendering so the user lands on the most commonly-edited
    // Graphics sub-page instead of the Display stub.
    m_tabBar->setCurrentIndex(1);
    m_stack->setCurrentIndex(1);
}

void Pcsx2GraphicsPage::onSubTabActivated(int index) {
    m_stack->setCurrentIndex(index);
    // Clear the description bar when switching sub-tabs so stale setting
    // help doesn't linger before the user focuses a new card.
    m_dialog->clearFocusedSetting();
}
```

Note: landing on Rendering (index 1) instead of Display (index 0) is deliberate — Display is a stub in Plan 2 and bouncing users straight into a "coming later" screen is a bad first impression. Plan 3 changes the default to 0 once Display is real.

- [ ] Register both files in CMake SOURCES/HEADERS.

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

Expected: compiles cleanly. Still not reachable from the dialog — Task 8 wires it in.

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.h \
        cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp \
        cpp/CMakeLists.txt
git commit -m "Add Pcsx2GraphicsPage container with sub-tab bar

Owns a back button, a four-box sub-tab bar, and an eager QStackedWidget
holding all four sub-pages (Display stub, Rendering, Post-Processing,
OSD stub). Re-emits settingFocused from the two real sub-pages so the
dialog's description bar still updates on focus. Starts on the
Rendering tab — Display and OSD are stubs in Plan 2."
```

---

## Task 8: Wire Graphics category to `Pcsx2GraphicsPage` in the dialog

**Files:**
- `cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp` (modify)

Replace the Plan 1 legacy-fallback branch for `"Graphics"` in `onCategoryActivated` with a `Pcsx2GraphicsPage` push. The `#include "ui/settings/emulator_settings_page.h"` at the top of the file must stay — `Pcsx2GraphicsStubSubPage` still needs it for the Display/OSD legacy fallback button.

- [ ] In `pcsx2_settings_dialog.cpp`, add the new include near the other page headers:

```cpp
#include "pages/pcsx2_graphics_page.h"
```

- [ ] Replace the existing Graphics branch of `onCategoryActivated`. Before:

```cpp
    } else if (category == "Graphics") {
        // Plan 1 fallback; replaced by Pcsx2GraphicsPage in Plan 2.
        auto* legacy = new EmulatorSettingsPage(m_app, m_emuId);
        legacy->setAttribute(Qt::WA_DeleteOnClose);
        legacy->setWindowModality(Qt::ApplicationModal);
        legacy->show();
    }
```

After:

```cpp
    } else if (category == "Graphics") {
        auto* page = new Pcsx2GraphicsPage(this);
        connect(page, &Pcsx2GraphicsPage::settingFocused,
                this, &Pcsx2SettingsDialog::setFocusedSetting);
        pushPage(page);
    }
```

Leave the `#include "ui/settings/emulator_settings_page.h"` at the top of the file alone — the stub sub-pages still depend on it.

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

- [ ] Run the test suite and confirm it stays at 11 passing:

```
ctest --test-dir cpp/build --output-on-failure
```

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
git commit -m "Route PCSX2 Graphics category to Pcsx2GraphicsPage

Replaces the Plan 1 legacy EmulatorSettingsPage fallback with a proper
push of Pcsx2GraphicsPage onto the dialog stack. Display and OSD
sub-tabs still reach the legacy page through Pcsx2GraphicsStubSubPage
until Plans 3 and 4 land."
```

---

## Task 9: Build + launch smoke test and final commit

**Files:** (verification only — no code changes unless a bug surfaces)

- [ ] Clean rebuild:

```
cd cpp && cmake --build build 2>&1 | tail -20
```

Expected: no warnings, no errors.

- [ ] Full test run:

```
ctest --test-dir cpp/build --output-on-failure
```

Expected: 11/11 tests pass, including the two new Graphics/Rendering and Graphics/Post-Processing slots added to `test_pcsx2_recommended_values` in Tasks 1-2.

- [ ] Launch the app:

```
open ./build/RetroNest.app
```

- [ ] Manual verification checklist:

  - Open RetroNest → Emulators → PCSX2 → settings button. Category hub opens (Plan 1 layout unchanged).
  - Arrow-navigate to the Graphics card, press Enter. The new `Pcsx2GraphicsPage` opens — back button top-left, four-box sub-tab bar below it showing Display / Rendering / Post-Proc / OSD with emoji icons and labels. The **Rendering** tab is active on entry (amber underline, lifted background).
  - The **Rendering** sub-page renders: full-width Internal Resolution combo card at the top, then a 2-column grid of 6 cards — Texture Filtering, Trilinear Filtering, Anisotropic Filtering, Dithering, Blending Accuracy, and a Hardware Mipmapping toggle.
  - Focus any card with Tab/arrow keys — the description bar at the bottom of the dialog updates with the setting's tooltip and an amber `Recommended: …` pill. Confirm the pill actually uses the `recommendedValue` you populated in Task 1 (e.g. Internal Resolution pill reads `Recommended: 1`).
  - Change a value (e.g. Internal Resolution → 3x Native), close the dialog, re-open it, drill back into Graphics → Rendering. The value persists. Open `PCSX2.ini` on disk and confirm `upscale_multiplier = 3` under `[EmuCore/GS]`.
  - Keyboard focus to the sub-tab bar: press Left/Right — selection moves between tabs, Enter activates, the stacked widget switches. Click the tabs with the mouse — same result.
  - Click **Post-Proc**. The Sharpening / Anti-Aliasing section header + card render: CAS combo on top, Sharpness slider + FXAA toggle side-by-side below. Then the Filters section header + card: TV Shader combo paired with Shade Boost toggle on top, a 2×2 slider grid below. Gamma slot (row 1 col 1) is empty — there is no slider there, just blank space. This is correct; the schema has no `ShadeBoost_Gamma`.
  - Toggle Shade Boost **off**. The three slider cards (Brightness, Contrast, Saturation) grey out — labels and tracks visibly dim. Attempting to drag them has no effect.
  - Toggle Shade Boost **on**. The three sliders become fully interactive again.
  - Drag Brightness to 70, switch to another tab and back — value persists. Confirm `PCSX2.ini` `[EmuCore/GS]` has `ShadeBoost_Brightness = 70`.
  - Click **Display** sub-tab. A centered "Display — Coming in a later update" stub appears with an "Open in legacy settings" button. Click it — the legacy `EmulatorSettingsPage` opens as a modal window.
  - Close the legacy modal, click **OSD** sub-tab. Same stub appears with "OSD — Coming in a later update".
  - Click the back button on the Graphics page. Returns to the category hub, description bar clears.
  - Drill into Emulation, Audio, Memory Cards one at a time — all three Plan 1 pages still work correctly (no regressions from Plan 2 wiring changes).
  - Close the dialog. Reopen DuckStation settings and PPSSPP settings — both still route through the legacy `EmulatorSettingsPage` (no regression — only `emuId == "pcsx2"` uses the new dialog).

- [ ] If everything above passes, there are no new code changes. If a bug surfaces, fix it in a new commit *before* moving on — do not defer to Plan 3.

- [ ] Final commit (only if verification-only fixes are needed; otherwise skip):

```
git commit -m "Plan 2 manual smoke test fixes"
```

---

## Plan 2 completion criteria

- [ ] Every `Graphics/Rendering` schema entry has a non-empty `recommendedValue`, verified by `testGraphicsRenderingSettingsAllHaveRecommended` (count ≥ 7).
- [ ] Every `Graphics/Post-Processing` schema entry has a non-empty `recommendedValue`, verified by `testGraphicsPostProcessingSettingsAllHaveRecommended` (count ≥ 7).
- [ ] `Pcsx2GraphicsSubTabBar` renders four box-style tabs with stacked emoji + label, handles Left/Right/Enter, and emits `tabActivated`.
- [ ] `Pcsx2GraphicsPage` is pushed onto the dialog stack when the Graphics category is activated, replacing the Plan 1 legacy fallback.
- [ ] `Pcsx2GraphicsRenderingPage` renders 7 schema-driven cards (Internal Resolution full-width, 6-card 2×3 grid) with load/save round-trip.
- [ ] `Pcsx2GraphicsPostProcessingPage` renders the spec's two-section layout with Shade Boost `dependsOn` correctly greying the 3 dependent slider cards.
- [ ] Display and OSD sub-tabs show `Pcsx2GraphicsStubSubPage` with a working "Open in legacy settings" button.
- [ ] Description bar updates correctly on focus across the Rendering and Post-Processing sub-pages.
- [ ] `ctest --test-dir cpp/build` stays at 11/11 passing.
- [ ] No regressions to Emulation, Audio, Memory Cards (Plan 1) or to DuckStation/PPSSPP settings routing.

## Not in Plan 2 (handled in later plans)

- **Plan 3 — Graphics / Display:** real Display sub-page (Renderer, Aspect Ratio, FMV override, Deinterlace, Bilinear, Vertical Stretch, Crop, Widescreen Patches, 3×2 toggle grid), live aspect-ratio preview widget, `recommendedValue` backfill for all Graphics/Display settings.
- **Plan 4 — Graphics / OSD & Polish:** real OSD sub-page with all `OsdShow*` toggles and OSD scale/position controls, live OSD preview widget reproducing `ImGuiOverlays.cpp` layout, `recommendedValue` backfill for Graphics/OSD, final polish (controller input verification, description-bar copy audit).
- DuckStation and PPSSPP redesigns — separate spec / plan series.
- Any schema changes beyond `recommendedValue` plumbing for Rendering + Post-Processing.
- Hoisting the `makeComboCard` / `makeToggleCard` / `makeSliderCard` helpers into a shared header — the Plan 2 pages deliberately inline them to stay self-contained. A cleanup pass can dedupe after Plan 4 if the pattern proves stable.

### Critical Files for Implementation

- /Users/mark/Documents/RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/tests/test_pcsx2_recommended_values.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/src/ui/settings/pcsx2/pages/pcsx2_emulation_page.cpp (reference pattern for pages)
- /Users/mark/Documents/RetroNest-Project/cpp/CMakeLists.txt