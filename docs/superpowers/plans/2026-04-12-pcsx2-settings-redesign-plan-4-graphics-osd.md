# PCSX2 Settings Redesign — Plan 4: Graphics / OSD (with live OSD preview)

Spec: `docs/superpowers/specs/2026-04-11-pcsx2-settings-redesign-design.md` (see §Graphics/OSD and §Preview behavior → OSD preview)

Plan series: Plan 1 (foundations), Plan 2 (Rendering + Post-Processing), Plan 3 (Display + aspect-ratio preview) — all merged. Plan 4 is the final Graphics sub-page and a second live preview widget that reproduces PCSX2 upstream's `pcsx2/ImGui/ImGuiOverlays.cpp` layout math.

Branch: `feat/pcsx2-settings-plan-4-graphics-osd` (already checked out, built against `cpp/build`).

## For agentic workers

Each task ends in a concrete commit. All code in this plan is the _complete_ source to write — no placeholders, no ellipses. If reality on disk disagrees with what is shown here (e.g. the OSD schema has different keys than the mapping table in Task 1), **STOP and surface the discrepancy as a BLOCKED condition**. Do not invent entries or categories.

Read this plan top-to-bottom before starting. Tasks 2 and 3 are independent (widget + unit test); Tasks 4–7 form the sub-page build; Task 8 wires it in; Task 9 is the smoke test. You should build after each task that touches C++ (`cmake --build cpp/build -j` from the repo root).

## Goal

Plan 4 replaces the `Pcsx2GraphicsStubSubPage` sitting at index 3 of `Pcsx2GraphicsPage`'s `QStackedWidget` with a real, schema-driven **Graphics / OSD** sub-page, and adds a second live preview widget — `Pcsx2OsdPreview` — that paints a miniature PS2 frame with the perf column, top-right indicators, bottom-right settings line, and bottom-left inputs line at the positions a real PCSX2 overlay would draw them. Every toggle / combo / slider on the page writes through `AppController::saveSettings` AND feeds the preview so the user sees a live echo of what the real OSD will look like in-game.

On completion, all four Graphics sub-pages (Display, Rendering, Post-Processing, OSD) are real, schema-driven pages and the Plan 2 `Pcsx2GraphicsStubSubPage` is no longer referenced anywhere in production code (optionally kept in the tree for reuse in future stubbed pages).

## Architecture

```
Pcsx2GraphicsPage (root QStackedWidget)
  ├─ [0] Pcsx2GraphicsDisplayPage        (Plan 3)
  ├─ [1] Pcsx2GraphicsRenderingPage      (Plan 2)
  ├─ [2] Pcsx2GraphicsPostProcessingPage (Plan 2)
  └─ [3] Pcsx2GraphicsOsdPage            (NEW — Plan 4)
        ├─ Top row (QHBoxLayout)
        │     ├─ Left compound Pcsx2Card (Focus=NoFocus)
        │     │     ├─ "PERFORMANCE STATS" inline section header
        │     │     ├─ 6 × Pcsx2ToggleRow (FPS, Speed, GPU, CPU, Res, VPS)
        │     │     ├─ "SETTINGS & INPUTS" inline section header
        │     │     └─ 3 × Pcsx2ToggleRow (Patches, Settings, Inputs)
        │     └─ Right preview Pcsx2Card (setPreviewStyle(true), Focus=NoFocus)
        │           ├─ "OSD PREVIEW" label
        │           ├─ Pcsx2OsdPreview  (NEW widget, 16:9 heightForWidth)
        │           ├─ Pcsx2SliderRow   (OsdScale)
        │           └─ 2 × Pcsx2ComboRow (OsdMessagesPos, OsdPerformancePos)
        └─ Bottom row (QGridLayout 3×3)
              9 × Pcsx2Card wrapping a Pcsx2ToggleRow each:
                row 0: OsdShowFrameTimes, OsdShowIndicators, OsdShowGSStats
                row 1: OsdShowHardwareInfo, OsdShowVersion, OsdShowVideoCapture
                row 2: OsdShowInputRec, OsdShowTextureReplacements, WarnAboutUnsafeSettings

Pcsx2OsdPreview (new paint-only widget)
  ├─ State: perfPos + every show* bool + osdScale%
  ├─ paintEvent → paintGameScene (warm-cool gradient)
  │                 + drawPerfColumn           (perf-pos driven)
  │                 + drawTopRightIndicators   (hard top-right)
  │                 + drawBottomRightSettings  (hard bottom-right)
  │                 + drawBottomLeftInputs     (hard bottom-left)
```

All arrow-key navigation on the OSD page reuses the exact spatial-nav pattern copied from `Pcsx2GraphicsDisplayPage::eventFilter` + `collectFocusables` + `findNextFocusSpatial`.

## Schema mapping (verified against `cpp/src/adapters/pcsx2_adapter.cpp:440–475`)

The current schema contains **21** Graphics/OSD entries. Every entry needs both `recommendedValue` and a `tooltip`. Use the mapping below verbatim. Every entry here was verified against the current file before this plan was written.

| Key | Type | Rec | Tooltip |
|---|---|---|---|
| `OsdScale` | Int | `"100"` | `"Global multiplier applied to every OSD overlay. 100% matches PCSX2 upstream's default size."` |
| `OsdMessagesPos` | Combo | `"1"` | `"Corner where transient messages (save-state loaded, shader reload, etc.) are drawn."` |
| `OsdPerformancePos` | Combo | `"3"` | `"Corner where the performance stats column (FPS/Speed/CPU/GPU/etc.) is drawn."` |
| `OsdShowSpeed` | Bool | `"false"` | `"Displays the emulation speed as a percentage. Red below 95%, green above 105%."` |
| `OsdShowFPS` | Bool | `"false"` | `"Displays the current frame rate reported by the GS. Useful for spotting performance issues."` |
| `OsdShowVPS` | Bool | `"false"` | `"Displays vertical syncs per second — the PS2 display refresh reported by the GS."` |
| `OsdShowResolution` | Bool | `"false"` | `"Displays the PS2 internal render resolution and interlacing mode."` |
| `OsdShowGSStats` | Bool | `"false"` | `"Displays per-frame GS statistics: draw-call count, VRAM use, and a frame-time summary."` |
| `OsdShowCPU` | Bool | `"false"` | `"Displays per-component CPU usage (EE, GS, VU)."` |
| `OsdShowGPU` | Bool | `"false"` | `"Displays GPU usage percentage and frame time in milliseconds."` |
| `OsdShowIndicators` | Bool | `"true"` | `"Displays icons for pause, fast-forward, slow-motion, and turbo modes in the top-right corner."` |
| `OsdShowFrameTimes` | Bool | `"false"` | `"Displays a rolling graph of recent frame times to visualise stutter."` |
| `OsdShowHardwareInfo` | Bool | `"false"` | `"Displays the CPU and GPU model names as two lines in the performance column."` |
| `OsdShowVersion` | Bool | `"false"` | `"Displays the PCSX2 version string in the performance column."` |
| `OsdShowSettings` | Bool | `"false"` | `"Displays a compact summary of active emulation settings in the bottom-right corner."` |
| `OsdshowPatches` | Bool | `"false"` | `"Appends active patches (widescreen, no-interlacing, etc.) to the settings line."` |
| `OsdShowInputs` | Bool | `"false"` | `"Displays the current controller input state at the bottom-left corner."` |
| `OsdShowVideoCapture` | Bool | `"true"` | `"Displays a recording indicator while video capture is active."` |
| `OsdShowInputRec` | Bool | `"true"` | `"Displays an indicator while input recording is active."` |
| `OsdShowTextureReplacements` | Bool | `"false"` | `"Displays an indicator when replacement textures are loaded for the current game."` |
| `WarnAboutUnsafeSettings` | Bool | `"true"` | `"Shows a startup warning if any unsafe settings are enabled. Currently mis-categorized under OSD; do not move in this plan."` |

Total = 21, so the test assertion `count >= 15` is satisfied.

**BLOCKED check for the engineer:** if the OSD block in `pcsx2_adapter.cpp` no longer matches this list (keys added, removed, renamed, or re-categorized), STOP and surface the diff. Do not silently update the mapping.

---

## Task 1 — Populate `recommendedValue` + tooltips for Graphics/OSD (TDD)

**Goal:** every Graphics/OSD entry in `pcsx2_adapter.cpp` has a non-empty `recommendedValue` and `tooltip`. A new test slot guards the invariant.

### Step 1.1 — Add the failing test slot

Open `cpp/tests/test_pcsx2_recommended_values.cpp` and add this slot immediately after `testGraphicsDisplaySettingsAllHaveRecommended`:

```cpp
    void testGraphicsOsdSettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Graphics") continue;
            if (d.subcategory != "OSD") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Graphics/OSD/%1").arg(d.key)));
            QVERIFY2(!d.tooltip.isEmpty(),
                     qPrintable(QString("missing tooltip for Graphics/OSD/%1").arg(d.key)));
        }
        QVERIFY2(count >= 15,
                 qPrintable(QString("expected >= 15 Graphics/OSD settings, got %1").arg(count)));
    }
```

Build and run: `cmake --build cpp/build -j --target test_pcsx2_recommended_values && cpp/build/test_pcsx2_recommended_values`. It must FAIL on the first entry (`OsdScale` has empty `recommendedValue` and tooltip).

### Step 1.2 — Rewrite the Graphics/OSD block in `pcsx2_adapter.cpp`

Replace lines 439–475 (the entire `// Graphics > OSD` block, from the comment banner down to and including the `WarnAboutUnsafeSettings` line) with the following. Each entry is converted to the scoped-block form introduced in Plans 1–3.

```cpp
    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > OSD
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "OSD", "On-Screen Display", "EmuCore/GS",
                     "OsdScale", "OSD Scale", "",
                     SettingDef::Int, "100", {}, 25, 500, 25, "", "%"};
        d.recommendedValue = "100";
        d.tooltip = "Global multiplier applied to every OSD overlay. 100% matches "
                    "PCSX2 upstream's default size.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "On-Screen Display", "EmuCore/GS",
                     "OsdMessagesPos", "OSD Messages Position", "",
                     SettingDef::Combo, "1",
                     {{"None", "0"}, {"Top Left (Default)", "1"}, {"Top Center", "2"},
                      {"Top Right", "3"}, {"Center Left", "4"}, {"Center", "5"},
                      {"Center Right", "6"}, {"Bottom Left", "7"},
                      {"Bottom Center", "8"}, {"Bottom Right", "9"}}, 0, 0, 0};
        d.recommendedValue = "1";
        d.tooltip = "Corner where transient messages (save-state loaded, shader "
                    "reload, etc.) are drawn.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "On-Screen Display", "EmuCore/GS",
                     "OsdPerformancePos", "OSD Performance Position", "",
                     SettingDef::Combo, "3",
                     {{"None", "0"}, {"Top Left", "1"}, {"Top Center", "2"},
                      {"Top Right (Default)", "3"}, {"Center Left", "4"},
                      {"Center", "5"}, {"Center Right", "6"}, {"Bottom Left", "7"},
                      {"Bottom Center", "8"}, {"Bottom Right", "9"}}, 0, 0, 0};
        d.recommendedValue = "3";
        d.tooltip = "Corner where the performance stats column "
                    "(FPS/Speed/CPU/GPU/etc.) is drawn.";
        s.append(d);
    }
    // ── Performance Stats ─────────────────────────────────────────────
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowSpeed", "Show Speed Percentages", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays the emulation speed as a percentage. "
                    "Red below 95%, green above 105%.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowFPS", "Show FPS", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays the current frame rate reported by the GS. "
                    "Useful for spotting performance issues.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowVPS", "Show VPS", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays vertical syncs per second — the PS2 display "
                    "refresh reported by the GS.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowResolution", "Show Resolution", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays the PS2 internal render resolution and "
                    "interlacing mode.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowGSStats", "Show GS Statistics", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays per-frame GS statistics: draw-call count, "
                    "VRAM use, and a frame-time summary.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowCPU", "Show CPU Usage", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays per-component CPU usage (EE, GS, VU).";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowGPU", "Show GPU Usage", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays GPU usage percentage and frame time in "
                    "milliseconds.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowIndicators", "Show Status Indicators", "",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        d.tooltip = "Displays icons for pause, fast-forward, slow-motion, "
                    "and turbo modes in the top-right corner.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Performance Stats", "EmuCore/GS",
                     "OsdShowFrameTimes", "Show Frame Times", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays a rolling graph of recent frame times to "
                    "visualise stutter.";
        s.append(d);
    }
    // ── System Information ───────────────────────────────────────────
    {
        SettingDef d{"Graphics", "OSD", "System Information", "EmuCore/GS",
                     "OsdShowHardwareInfo", "Show Hardware Info", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays the CPU and GPU model names as two lines in "
                    "the performance column.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "System Information", "EmuCore/GS",
                     "OsdShowVersion", "Show PCSX2 Version", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays the PCSX2 version string in the performance column.";
        s.append(d);
    }
    // ── Settings & Inputs ────────────────────────────────────────────
    {
        SettingDef d{"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS",
                     "OsdShowSettings", "Show Settings", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays a compact summary of active emulation settings "
                    "in the bottom-right corner.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS",
                     "OsdshowPatches", "Show Patches", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Appends active patches (widescreen, no-interlacing, etc.) "
                    "to the settings line.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS",
                     "OsdShowInputs", "Show Inputs", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays the current controller input state at the "
                    "bottom-left corner.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS",
                     "OsdShowVideoCapture", "Show Video Capture Status", "",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        d.tooltip = "Displays a recording indicator while video capture is active.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS",
                     "OsdShowInputRec", "Show Input Recording Status", "",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        d.tooltip = "Displays an indicator while input recording is active.";
        s.append(d);
    }
    {
        SettingDef d{"Graphics", "OSD", "Settings & Inputs", "EmuCore/GS",
                     "OsdShowTextureReplacements", "Show Texture Replacement Status", "",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false";
        d.tooltip = "Displays an indicator when replacement textures are loaded "
                    "for the current game.";
        s.append(d);
    }
    // ── Messages ─────────────────────────────────────────────────────
    {
        SettingDef d{"Graphics", "OSD", "Messages", "EmuCore",
                     "WarnAboutUnsafeSettings", "Warn About Unsafe Settings", "",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true";
        d.tooltip = "Shows a startup warning if any unsafe settings are enabled. "
                    "Currently mis-categorized under OSD; do not move in this plan.";
        s.append(d);
    }
```

### Step 1.3 — Verify the test passes

```
cmake --build cpp/build -j --target test_pcsx2_recommended_values
cpp/build/test_pcsx2_recommended_values
```

Expected: `testGraphicsOsdSettingsAllHaveRecommended PASS`, all other slots still PASS.

### Step 1.4 — Commit

```
git add cpp/src/adapters/pcsx2_adapter.cpp cpp/tests/test_pcsx2_recommended_values.cpp
git commit -m "Plan 4 Task 1: recommendedValue + tooltips for Graphics/OSD schema"
```

---

## Task 2 — `Pcsx2OsdPreview` widget

**Goal:** a paint-only QWidget that reproduces the layout math of `pcsx2/ImGui/ImGuiOverlays.cpp` for the perf column + top-right indicators + bottom-right settings + bottom-left inputs, driven by setters.

### Step 2.1 — Create the header

Write `cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.h`:

```cpp
#pragma once
#include <QWidget>
#include <QRectF>
#include <QStringList>

// Live OSD preview used inside Pcsx2GraphicsOsdPage. Paints a dummy 16:9
// PS2 frame and overlays a miniature of PCSX2 upstream's ImGuiOverlays
// layout: perf column (pos-driven), top-right indicators (hard corner),
// bottom-right settings (hard corner), bottom-left inputs (hard corner).
//
// Reference upstream file: pcsx2/ImGui/ImGuiOverlays.cpp
//   DrawPerformanceOverlay   (lines 191-557)
//   DrawIndicatorsOverlay    (lines 1201-1253)
//   DrawSettingsOverlay      (lines 780-929)
//   DrawInputsOverlay        (lines 931-...)
//   RenderOverlays dispatch  (lines 1682-1699)
class Pcsx2OsdPreview : public QWidget {
    Q_OBJECT
public:
    enum class OverlayPos {
        None,
        TopLeft, TopCenter, TopRight,
        CenterLeft, Center, CenterRight,
        BottomLeft, BottomCenter, BottomRight
    };

    explicit Pcsx2OsdPreview(QWidget* parent = nullptr);

    // Performance column position (OsdPerformancePos).
    void setPerformancePos(OverlayPos pos);

    // Perf column content toggles.
    void setShowFps(bool on);
    void setShowVps(bool on);
    void setShowSpeed(bool on);
    void setShowVersion(bool on);
    void setShowResolution(bool on);
    void setShowHardwareInfo(bool on);
    void setShowCpu(bool on);
    void setShowGpu(bool on);
    void setShowFrameTimes(bool on);
    void setShowGsStats(bool on);

    // Corner-hardcoded overlays.
    void setShowIndicators(bool on);
    void setShowVideoCapture(bool on);
    void setShowInputRec(bool on);
    void setShowTextureReplacements(bool on);
    void setShowSettings(bool on);
    void setShowPatches(bool on);
    void setShowInputs(bool on);

    // OSD scale (10..300 percent, clamped); scales the font proportionally.
    void setOsdScale(int percent);

    // Map the OsdPerformancePos combo value string (label text from the
    // schema) to an OverlayPos. Unknown → TopLeft.
    static OverlayPos fromPosValue(const QString& v);

    QSize sizeHint() const override        { return QSize(320, 180); }
    QSize minimumSizeHint() const override { return QSize(240, 135); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    struct State {
        OverlayPos perfPos = OverlayPos::TopRight;
        bool fps = false, vps = false, speed = false, version = false;
        bool resolution = false, hardwareInfo = false;
        bool cpu = false, gpu = false, frameTimes = false, gsStats = false;
        bool indicators = true, videoCapture = false, inputRec = false,
             textureReplacements = false;
        bool settings = false, patches = false, inputs = false;
        int osdScale = 100;
    } m_s;

    int  scaledFontPx() const;
    void paintGameScene(QPainter& p, const QRectF& r) const;

    QStringList buildPerfColumnLines() const;
    void drawPerfColumn(QPainter& p, const QRectF& screen) const;
    void drawTopRightIndicators(QPainter& p, const QRectF& screen) const;
    void drawBottomRightSettings(QPainter& p, const QRectF& screen) const;
    void drawBottomLeftInputs(QPainter& p, const QRectF& screen) const;
};
```

### Step 2.2 — Create the implementation

Write `cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.cpp`:

```cpp
#include "pcsx2_osd_preview.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QFont>
#include <QFontMetricsF>
#include <QFontDatabase>
#include <QStringList>
#include <QtMath>
#include <algorithm>

namespace {
constexpr qreal kMargin = 8.0;

QColor kShadow(0, 0, 0, 217);           // rgba(0,0,0,0.85)
QColor kWhite(0xff, 0xff, 0xff);
QColor kSpeedGreen(0x60, 0xff, 0x60);
QColor kDimRed  (0xff, 0x60, 0x60);
QColor kMuted   (0xd9, 0xd4, 0xcc);

QFont monospaceFont(int px) {
    // Prefer a fixed-pitch face known to exist on macOS; fall back to the
    // system default if neither is available.
    for (const char* name : {"Menlo", "Monaco", "Courier New"}) {
        QFont f(QString::fromLatin1(name));
        if (QFontInfo(f).fixedPitch()) {
            f.setPixelSize(px);
            return f;
        }
    }
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPixelSize(px);
    return f;
}

// Draw a single text line with a 1px dark shadow underneath.
void drawShadowedText(QPainter& p, const QPointF& baseline,
                      const QString& text, const QColor& color) {
    p.setPen(kShadow);
    p.drawText(baseline + QPointF(1, 1), text);
    p.setPen(color);
    p.drawText(baseline, text);
}
} // namespace

Pcsx2OsdPreview::Pcsx2OsdPreview(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void Pcsx2OsdPreview::setPerformancePos(OverlayPos pos) { m_s.perfPos = pos; update(); }

void Pcsx2OsdPreview::setShowFps(bool on)             { m_s.fps = on;             update(); }
void Pcsx2OsdPreview::setShowVps(bool on)             { m_s.vps = on;             update(); }
void Pcsx2OsdPreview::setShowSpeed(bool on)           { m_s.speed = on;           update(); }
void Pcsx2OsdPreview::setShowVersion(bool on)         { m_s.version = on;         update(); }
void Pcsx2OsdPreview::setShowResolution(bool on)      { m_s.resolution = on;      update(); }
void Pcsx2OsdPreview::setShowHardwareInfo(bool on)    { m_s.hardwareInfo = on;    update(); }
void Pcsx2OsdPreview::setShowCpu(bool on)             { m_s.cpu = on;             update(); }
void Pcsx2OsdPreview::setShowGpu(bool on)             { m_s.gpu = on;             update(); }
void Pcsx2OsdPreview::setShowFrameTimes(bool on)      { m_s.frameTimes = on;      update(); }
void Pcsx2OsdPreview::setShowGsStats(bool on)         { m_s.gsStats = on;         update(); }

void Pcsx2OsdPreview::setShowIndicators(bool on)         { m_s.indicators = on;           update(); }
void Pcsx2OsdPreview::setShowVideoCapture(bool on)       { m_s.videoCapture = on;         update(); }
void Pcsx2OsdPreview::setShowInputRec(bool on)           { m_s.inputRec = on;             update(); }
void Pcsx2OsdPreview::setShowTextureReplacements(bool on){ m_s.textureReplacements = on;  update(); }
void Pcsx2OsdPreview::setShowSettings(bool on)           { m_s.settings = on;             update(); }
void Pcsx2OsdPreview::setShowPatches(bool on)            { m_s.patches = on;              update(); }
void Pcsx2OsdPreview::setShowInputs(bool on)             { m_s.inputs = on;               update(); }

void Pcsx2OsdPreview::setOsdScale(int percent) {
    m_s.osdScale = std::clamp(percent, 10, 300);
    update();
}

Pcsx2OsdPreview::OverlayPos Pcsx2OsdPreview::fromPosValue(const QString& v) {
    const QString s = v.trimmed();
    if (s.compare("None", Qt::CaseInsensitive) == 0) return OverlayPos::None;
    // The schema label strings include "(Default)" suffixes for some
    // entries (e.g. "Top Right (Default)"). Match against the stem before
    // that suffix.
    const QString stem = s.section('(', 0, 0).trimmed();
    if (stem.compare("Top Left",      Qt::CaseInsensitive) == 0) return OverlayPos::TopLeft;
    if (stem.compare("Top Center",    Qt::CaseInsensitive) == 0) return OverlayPos::TopCenter;
    if (stem.compare("Top Right",     Qt::CaseInsensitive) == 0) return OverlayPos::TopRight;
    if (stem.compare("Center Left",   Qt::CaseInsensitive) == 0) return OverlayPos::CenterLeft;
    if (stem.compare("Center",        Qt::CaseInsensitive) == 0) return OverlayPos::Center;
    if (stem.compare("Center Right",  Qt::CaseInsensitive) == 0) return OverlayPos::CenterRight;
    if (stem.compare("Bottom Left",   Qt::CaseInsensitive) == 0) return OverlayPos::BottomLeft;
    if (stem.compare("Bottom Center", Qt::CaseInsensitive) == 0) return OverlayPos::BottomCenter;
    if (stem.compare("Bottom Right",  Qt::CaseInsensitive) == 0) return OverlayPos::BottomRight;
    return OverlayPos::TopLeft;
}

int Pcsx2OsdPreview::scaledFontPx() const {
    const int px = int(qRound(10.0 * double(m_s.osdScale) / 100.0));
    return std::clamp(px, 6, 24);
}

void Pcsx2OsdPreview::paintGameScene(QPainter& p, const QRectF& r) const {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0,  QColor(0x5e, 0x7e, 0xa6));   // cool sky
    g.setColorAt(0.55, QColor(0x8a, 0x70, 0x58));   // warm horizon
    g.setColorAt(1.0,  QColor(0x3a, 0x2c, 0x22));   // dark ground
    p.fillRect(r, g);

    // A simple sun disc in the upper third for visual interest.
    const qreal sunR = qMin(r.width(), r.height()) * 0.08;
    QPointF sunC(r.left() + r.width() * 0.72, r.top() + r.height() * 0.32);
    QRadialGradient sg(sunC, sunR * 2.2);
    sg.setColorAt(0.0, QColor(255, 235, 180, 230));
    sg.setColorAt(1.0, QColor(255, 200, 120, 0));
    p.setBrush(sg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(sunC, sunR * 2.2, sunR * 2.2);
}

QStringList Pcsx2OsdPreview::buildPerfColumnLines() const {
    QStringList out;

    // Line 1: summary joined with " | " — only enabled fragments.
    QStringList line1;
    if (m_s.speed)   line1 << QStringLiteral("Speed: 100%");
    if (m_s.vps)     line1 << QStringLiteral("VPS: 59.94");
    if (m_s.fps)     line1 << QStringLiteral("FPS: 59.94");
    if (m_s.version) line1 << QStringLiteral("PCSX2 2.3.0");
    if (!line1.isEmpty())
        out << line1.join(QStringLiteral(" | "));

    if (m_s.gsStats) {
        out << QStringLiteral("GS: 4328 draws")
            << QStringLiteral("VRAM: 384 MB / 512 MB")
            << QStringLiteral("6 QF | Min 14.2ms | Avg 21.4ms | Max 32.8ms");
    }
    if (m_s.resolution)
        out << QStringLiteral("640x448 NTSC Interlaced");
    if (m_s.hardwareInfo) {
        out << QStringLiteral("CPU: Apple M1 Max (10C/10T)")
            << QStringLiteral("GPU: Apple M1 Max");
    }
    if (m_s.cpu)
        out << QStringLiteral("EE: 32.5% (5.42ms)  GS: 14.2% (2.36ms)");
    if (m_s.gpu)
        out << QStringLiteral("GPU: 42.3% (4.21ms)");
    if (m_s.frameTimes)
        out << QStringLiteral("[\u2581\u2582\u2583\u2584\u2585\u2586\u2587\u2588"
                              "\u2587\u2586\u2585\u2584\u2583\u2582\u2581]");
    return out;
}

void Pcsx2OsdPreview::drawPerfColumn(QPainter& p, const QRectF& screen) const {
    if (m_s.perfPos == OverlayPos::None) return;
    const QStringList lines = buildPerfColumnLines();
    if (lines.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);

    qreal maxW = 0.0;
    for (const QString& l : lines)
        maxW = std::max(maxW, fm.horizontalAdvance(l));
    const qreal lineH = fm.height();
    const qreal blockH = lineH * lines.size();
    const qreal blockW = maxW;

    qreal x = screen.left() + kMargin;
    qreal y = screen.top()  + kMargin + fm.ascent();

    switch (m_s.perfPos) {
        case OverlayPos::TopLeft:
        case OverlayPos::CenterLeft:
        case OverlayPos::BottomLeft:
            x = screen.left() + kMargin; break;
        case OverlayPos::TopCenter:
        case OverlayPos::Center:
        case OverlayPos::BottomCenter:
            x = screen.left() + (screen.width() - blockW) * 0.5; break;
        case OverlayPos::TopRight:
        case OverlayPos::CenterRight:
        case OverlayPos::BottomRight:
            x = screen.right() - kMargin - blockW; break;
        default: break;
    }
    switch (m_s.perfPos) {
        case OverlayPos::TopLeft:
        case OverlayPos::TopCenter:
        case OverlayPos::TopRight:
            y = screen.top() + kMargin + fm.ascent(); break;
        case OverlayPos::CenterLeft:
        case OverlayPos::Center:
        case OverlayPos::CenterRight:
            y = screen.top() + (screen.height() - blockH) * 0.5 + fm.ascent(); break;
        case OverlayPos::BottomLeft:
        case OverlayPos::BottomCenter:
        case OverlayPos::BottomRight:
            y = screen.bottom() - kMargin - blockH + fm.ascent(); break;
        default: break;
    }

    // The first perf line uses green for the Speed fragment (when speed
    // is enabled); everything else is white. Split on " | " and colour
    // each fragment individually so Speed can be green and the rest white.
    const QString& first = lines.first();
    qreal cursorX = x;

    if (m_s.speed && first.startsWith("Speed")) {
        // Draw Speed in green, then each subsequent fragment in white,
        // with white " | " separators.
        const QStringList fragments = first.split(QStringLiteral(" | "));
        for (int i = 0; i < fragments.size(); ++i) {
            const QString& frag = fragments[i];
            const QColor color = (i == 0) ? kSpeedGreen : kWhite;
            drawShadowedText(p, QPointF(cursorX, y), frag, color);
            cursorX += fm.horizontalAdvance(frag);
            if (i != fragments.size() - 1) {
                const QString sep = QStringLiteral(" | ");
                drawShadowedText(p, QPointF(cursorX, y), sep, kWhite);
                cursorX += fm.horizontalAdvance(sep);
            }
        }
    } else {
        drawShadowedText(p, QPointF(x, y), first, kWhite);
    }

    // Remaining lines in white.
    for (int i = 1; i < lines.size(); ++i) {
        const qreal yy = y + lineH * i;
        drawShadowedText(p, QPointF(x, yy), lines[i], kWhite);
    }
    Q_UNUSED(kDimRed); // retained for future real-time speed colouring
    Q_UNUSED(kMuted);
}

void Pcsx2OsdPreview::drawTopRightIndicators(QPainter& p, const QRectF& screen) const {
    QStringList items;
    if (m_s.indicators)          items << QStringLiteral("\u23E9 FF");      // fast-forward
    if (m_s.videoCapture)        items << QStringLiteral("\u23FA REC");     // record dot
    if (m_s.inputRec)            items << QStringLiteral("\u25CF INPUT");   // filled circle
    if (m_s.textureReplacements) items << QStringLiteral("\U0001F3A8 TEX"); // art palette
    if (items.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);

    const qreal lineH = fm.height();
    qreal y = screen.top() + kMargin + fm.ascent();
    for (const QString& t : items) {
        const qreal w = fm.horizontalAdvance(t);
        const qreal x = screen.right() - kMargin - w;
        drawShadowedText(p, QPointF(x, y), t, kWhite);
        y += lineH;
    }
}

void Pcsx2OsdPreview::drawBottomRightSettings(QPainter& p, const QRectF& screen) const {
    if (!m_s.settings && !m_s.patches) return;

    QStringList parts;
    if (m_s.settings)
        parts << QStringLiteral("DB=2 P=5 C=0 | CR=1 FCDVD VSYNC EER=0 EEC=1");
    if (m_s.patches)
        parts << QStringLiteral("Patches: Widescreen, NoInterlace");
    const QString text = parts.join(QStringLiteral("  "));

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(text);
    const qreal x = screen.right()  - kMargin - w;
    const qreal y = screen.bottom() - kMargin;  // baseline
    drawShadowedText(p, QPointF(x, y), text, kWhite);
}

void Pcsx2OsdPreview::drawBottomLeftInputs(QPainter& p, const QRectF& screen) const {
    if (!m_s.inputs) return;
    const QString text =
        QStringLiteral("\U0001F3AE 1 \u2022 DualShock | A X \u2191 LT:0.42");

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal x = screen.left()   + kMargin;
    const qreal y = screen.bottom() - kMargin;
    drawShadowedText(p, QPointF(x, y), text, kWhite);
}

void Pcsx2OsdPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Fixed 16:9 screen area, centred vertically in the widget. Matches
    // Pcsx2AspectRatioPreview's sizing approach.
    QRectF client = rect().adjusted(1, 1, -1, -1);
    const qreal targetH = client.width() * 9.0 / 16.0;
    QRectF screen = client;
    if (targetH <= client.height()) {
        screen.setTop(client.top() + (client.height() - targetH) * 0.5);
        screen.setHeight(targetH);
    } else {
        const qreal targetW = client.height() * 16.0 / 9.0;
        screen.setLeft(client.left() + (client.width() - targetW) * 0.5);
        screen.setWidth(targetW);
    }

    paintGameScene(p, screen);

    // Thin frame around the "screen".
    p.setPen(QPen(QColor(0, 0, 0, 160), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(screen);

    drawPerfColumn(p, screen);
    drawTopRightIndicators(p, screen);
    drawBottomRightSettings(p, screen);
    drawBottomLeftInputs(p, screen);
}
```

### Step 2.3 — Register in CMake

In `cpp/CMakeLists.txt`, add the widget source next to the aspect-ratio preview entry (around line 88):

```cmake
    src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp
    src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.cpp
```

And add the header next to the aspect-ratio preview header (around line 170):

```cmake
    src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h
    src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.h
```

### Step 2.4 — Build

```
cmake --build cpp/build -j
```

Expected: clean build. No usage yet — this is a standalone widget.

### Step 2.5 — Commit

```
git add cpp/src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.{h,cpp} cpp/CMakeLists.txt
git commit -m "Plan 4 Task 2: Pcsx2OsdPreview paint-only preview widget"
```

---

## Task 3 — Unit test `test_pcsx2_osd_preview.cpp`

**Goal:** 4 slots exercising sizeHint, every setter path, enum parsing, and an all-on stress repaint.

### Step 3.1 — Write the test

Write `cpp/tests/test_pcsx2_osd_preview.cpp`:

```cpp
#include <QtTest>
#include <QApplication>
#include "ui/settings/pcsx2/widgets/pcsx2_osd_preview.h"

class TestPcsx2OsdPreview : public QObject {
    Q_OBJECT
private slots:
    void testSizeHint() {
        Pcsx2OsdPreview w;
        QCOMPARE(w.sizeHint(),        QSize(320, 180));
        QCOMPARE(w.minimumSizeHint(), QSize(240, 135));
        QVERIFY(w.hasHeightForWidth());
        QCOMPARE(w.heightForWidth(320), 180);
    }

    void testSettersDoNotCrash() {
        Pcsx2OsdPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        using Pos = Pcsx2OsdPreview::OverlayPos;
        for (Pos p : {Pos::None, Pos::TopLeft, Pos::TopCenter, Pos::TopRight,
                      Pos::CenterLeft, Pos::Center, Pos::CenterRight,
                      Pos::BottomLeft, Pos::BottomCenter, Pos::BottomRight}) {
            w.setPerformancePos(p);
        }

        w.setOsdScale(-500);   // clamped to 10
        w.setOsdScale(0);      // clamped to 10
        w.setOsdScale(100);
        w.setOsdScale(300);
        w.setOsdScale(9999);   // clamped to 300

        w.setShowFps(true);        w.setShowFps(false);
        w.setShowVps(true);        w.setShowVps(false);
        w.setShowSpeed(true);      w.setShowSpeed(false);
        w.setShowVersion(true);    w.setShowVersion(false);
        w.setShowResolution(true); w.setShowResolution(false);
        w.setShowHardwareInfo(true);
        w.setShowCpu(true);        w.setShowGpu(true);
        w.setShowFrameTimes(true); w.setShowGsStats(true);

        w.setShowIndicators(true);
        w.setShowVideoCapture(true);
        w.setShowInputRec(true);
        w.setShowTextureReplacements(true);
        w.setShowSettings(true);
        w.setShowPatches(true);
        w.setShowInputs(true);

        w.repaint();
    }

    void testFromPosValue() {
        using Pos = Pcsx2OsdPreview::OverlayPos;
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("None"),                  Pos::None);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Left"),              Pos::TopLeft);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Left (Default)"),    Pos::TopLeft);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Center"),            Pos::TopCenter);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Right"),             Pos::TopRight);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Right (Default)"),   Pos::TopRight);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Center Left"),           Pos::CenterLeft);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Center"),                Pos::Center);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Center Right"),          Pos::CenterRight);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Bottom Left"),           Pos::BottomLeft);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Bottom Center"),         Pos::BottomCenter);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Bottom Right"),          Pos::BottomRight);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("garbage"),               Pos::TopLeft);
    }

    void testAllTogglesOnAtOnce() {
        Pcsx2OsdPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        w.setPerformancePos(Pcsx2OsdPreview::OverlayPos::Center);
        w.setOsdScale(150);
        w.setShowFps(true);
        w.setShowVps(true);
        w.setShowSpeed(true);
        w.setShowVersion(true);
        w.setShowResolution(true);
        w.setShowHardwareInfo(true);
        w.setShowCpu(true);
        w.setShowGpu(true);
        w.setShowFrameTimes(true);
        w.setShowGsStats(true);
        w.setShowIndicators(true);
        w.setShowVideoCapture(true);
        w.setShowInputRec(true);
        w.setShowTextureReplacements(true);
        w.setShowSettings(true);
        w.setShowPatches(true);
        w.setShowInputs(true);

        w.repaint();
    }
};

QTEST_MAIN(TestPcsx2OsdPreview)
#include "test_pcsx2_osd_preview.moc"
```

### Step 3.2 — Register in CMake

In `cpp/CMakeLists.txt`, immediately after the `test_pcsx2_aspect_ratio_preview` block (around line 482), add:

```cmake
add_executable(test_pcsx2_osd_preview
    tests/test_pcsx2_osd_preview.cpp
    src/ui/settings/pcsx2/widgets/pcsx2_osd_preview.cpp
)
set_target_properties(test_pcsx2_osd_preview PROPERTIES AUTOMOC ON)
target_include_directories(test_pcsx2_osd_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_pcsx2_osd_preview PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME Pcsx2OsdPreview COMMAND test_pcsx2_osd_preview)
```

### Step 3.3 — Build + run

```
cmake --build cpp/build -j --target test_pcsx2_osd_preview
cpp/build/test_pcsx2_osd_preview
```

Expected: 4/4 PASS.

### Step 3.4 — Commit

```
git add cpp/tests/test_pcsx2_osd_preview.cpp cpp/CMakeLists.txt
git commit -m "Plan 4 Task 3: unit test for Pcsx2OsdPreview (4 slots)"
```

---

## Task 4 — `Pcsx2GraphicsOsdPage` skeleton

**Goal:** create the page class with header + cpp stubs compiling against the Plan 1–3 widget primitives. No layout yet — buildUi only calls empty helpers, loadValues is a no-op, syncPreview is a no-op. This isolates compile issues from layout bugs.

### Step 4.1 — Write the header

Write `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include <QList>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class Pcsx2Card;
class Pcsx2OsdPreview;
class Pcsx2ComboRow;
class Pcsx2ToggleRow;
class Pcsx2SliderRow;
class QHBoxLayout;
class QVBoxLayout;

class Pcsx2GraphicsOsdPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsOsdPage(Pcsx2SettingsDialog* dialog);
    ~Pcsx2GraphicsOsdPage() override;

signals:
    void settingFocused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void buildLeftCompoundCard(QHBoxLayout* topRow);
    void buildRightPreviewCard(QHBoxLayout* topRow);
    void buildBottomToggleGrid(QVBoxLayout* root);

    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    void syncPreview();

    QList<QWidget*> collectFocusables() const;
    QWidget* findNextFocusSpatial(QWidget* current, int key) const;

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef>  m_schema;

    Pcsx2OsdPreview* m_preview = nullptr;

    // Right card widgets.
    Pcsx2SliderRow* m_scaleSlider = nullptr;
    Pcsx2ComboRow*  m_messagesPosCombo = nullptr;
    Pcsx2ComboRow*  m_perfPosCombo = nullptr;
};
```

### Step 4.2 — Write the skeleton cpp

Write `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.cpp`:

```cpp
#include "pcsx2_graphics_osd_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../widgets/pcsx2_slider_row.h"
#include "../widgets/pcsx2_osd_preview.h"
#include "../widgets/pcsx2_toggle.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QVariantMap>
#include <QScrollArea>
#include <QFrame>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QAbstractItemView>
#include <limits>

Pcsx2GraphicsOsdPage::Pcsx2GraphicsOsdPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "OSD")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    syncPreview();
    qApp->installEventFilter(this);
}

Pcsx2GraphicsOsdPage::~Pcsx2GraphicsOsdPage() {
    qApp->removeEventFilter(this);
}

const SettingDef* Pcsx2GraphicsOsdPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2GraphicsOsdPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void Pcsx2GraphicsOsdPage::buildUi() {
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
        "QScrollBar::sub-page:vertical, QScrollBar::add-page:vertical { background: transparent; }");
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);
    buildLeftCompoundCard(topRow);
    buildRightPreviewCard(topRow);
    root->addLayout(topRow);

    buildBottomToggleGrid(root);
    root->addStretch();
}

void Pcsx2GraphicsOsdPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    // Filled in Task 5.
    Q_UNUSED(topRow);
}

void Pcsx2GraphicsOsdPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    // Filled in Task 6.
    Q_UNUSED(topRow);
}

void Pcsx2GraphicsOsdPage::buildBottomToggleGrid(QVBoxLayout* root) {
    // Filled in Task 7.
    Q_UNUSED(root);
}

void Pcsx2GraphicsOsdPage::loadValues() {
    // Filled in Task 7.
}

void Pcsx2GraphicsOsdPage::syncPreview() {
    // Filled in Task 7.
}

bool Pcsx2GraphicsOsdPage::eventFilter(QObject* obj, QEvent* e) {
    return QWidget::eventFilter(obj, e);
}

QList<QWidget*> Pcsx2GraphicsOsdPage::collectFocusables() const {
    return {};
}

QWidget* Pcsx2GraphicsOsdPage::findNextFocusSpatial(QWidget* /*current*/, int /*key*/) const {
    return nullptr;
}
```

### Step 4.3 — Register in CMake

In `cpp/CMakeLists.txt`, add the source next to `pcsx2_graphics_display_page.cpp`:

```cmake
    src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp
    src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.cpp
```

And the header next to `pcsx2_graphics_display_page.h`:

```cmake
    src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.h
    src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.h
```

### Step 4.4 — Build

```
cmake --build cpp/build -j
```

Expected: clean build. The class is not yet referenced from `Pcsx2GraphicsPage`.

### Step 4.5 — Commit

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.{h,cpp} cpp/CMakeLists.txt
git commit -m "Plan 4 Task 4: Pcsx2GraphicsOsdPage skeleton"
```

---

## Task 5 — Left compound card (Performance Stats + Settings & Inputs)

**Goal:** fill `buildLeftCompoundCard` with two inline mini section headers and nine toggle rows.

### Step 5.1 — Replace `buildLeftCompoundCard` in `pcsx2_graphics_osd_page.cpp`

```cpp
void Pcsx2GraphicsOsdPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    auto* card = new Pcsx2Card(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setMinimumHeight(460);

    if (const SettingDef* perfDef = findDef("OsdPerformancePos"))
        card->setSettingDef(*perfDef);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);

    auto addMiniHeader = [&](const QString& text) {
        auto* hdr = new QLabel(text, card);
        hdr->setStyleSheet(
            "color:#f59e0b; font-size:11px; font-weight:700;"
            "letter-spacing:1.0px; padding:6px 0 2px 0;");
        v->addWidget(hdr);
    };

    // key → preview-setter dispatcher for rows in the left card.
    auto addToggle = [&](const QString& key) {
        const SettingDef* d = findDef(key);
        if (!d) return;
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            if (!m_preview) return;
            if      (key == "OsdShowFPS")        m_preview->setShowFps(on);
            else if (key == "OsdShowSpeed")      m_preview->setShowSpeed(on);
            else if (key == "OsdShowVPS")        m_preview->setShowVps(on);
            else if (key == "OsdShowResolution") m_preview->setShowResolution(on);
            else if (key == "OsdShowCPU")        m_preview->setShowCpu(on);
            else if (key == "OsdShowGPU")        m_preview->setShowGpu(on);
            else if (key == "OsdShowSettings")   m_preview->setShowSettings(on);
            else if (key == "OsdshowPatches")    m_preview->setShowPatches(on);
            else if (key == "OsdShowInputs")     m_preview->setShowInputs(on);
        });
        v->addWidget(row);
    };

    addMiniHeader(QStringLiteral("PERFORMANCE STATS"));
    addToggle("OsdShowFPS");
    addToggle("OsdShowSpeed");
    addToggle("OsdShowGPU");
    addToggle("OsdShowCPU");
    addToggle("OsdShowResolution");
    addToggle("OsdShowVPS");

    addMiniHeader(QStringLiteral("SETTINGS & INPUTS"));
    addToggle("OsdshowPatches");
    addToggle("OsdShowSettings");
    addToggle("OsdShowInputs");

    v->addStretch();
    topRow->addWidget(card, 1);
}
```

### Step 5.2 — Build

```
cmake --build cpp/build -j
```

Expected: clean build. The page is not yet wired into `Pcsx2GraphicsPage` so nothing to visually test.

### Step 5.3 — Commit

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.cpp
git commit -m "Plan 4 Task 5: OSD page left compound card (PERFORMANCE STATS + SETTINGS & INPUTS)"
```

---

## Task 6 — Right preview card (label, OSD preview, scale, position combos)

**Goal:** fill `buildRightPreviewCard` with the preview widget, OSD scale slider, and two horizontal combo rows for message/performance positions. Wire each row to the preview setters.

### Step 6.1 — Replace `buildRightPreviewCard` in `pcsx2_graphics_osd_page.cpp`

```cpp
void Pcsx2GraphicsOsdPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    auto* card = new Pcsx2Card(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setMinimumHeight(460);
    card->setPreviewStyle(true);
    if (const SettingDef* d = findDef("OsdScale"))
        card->setSettingDef(*d);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    auto* lbl = new QLabel(QStringLiteral("OSD PREVIEW"), card);
    lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;"
                       "letter-spacing:0.8px;");
    v->addWidget(lbl);

    m_preview = new Pcsx2OsdPreview(card);
    v->addWidget(m_preview);

    if (const SettingDef* d = findDef("OsdScale")) {
        m_scaleSlider = new Pcsx2SliderRow(card);
        m_scaleSlider->setLabel(d->label);
        m_scaleSlider->setRange(int(d->minVal), int(d->maxVal));
        m_scaleSlider->setSuffix(d->suffix);
        m_scaleSlider->setSettingDef(*d);
        connect(m_scaleSlider, &Pcsx2SliderRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(m_scaleSlider, &Pcsx2SliderRow::valueChanged, this, [this](int val) {
            const SettingDef* dd = findDef("OsdScale");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
            if (m_preview) m_preview->setOsdScale(val);
        });
        v->addWidget(m_scaleSlider);
    }

    // Two combos in a tight horizontal row (label-left).
    auto* comboRow = new QHBoxLayout();
    comboRow->setSpacing(8);

    auto addPosCombo = [&](const QString& key, bool drivePerfPreview) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ComboRow(card, /*stacked=*/false);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this,
                [this, key, drivePerfPreview](const QString& val) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
            if (drivePerfPreview && m_preview)
                m_preview->setPerformancePos(Pcsx2OsdPreview::fromPosValue(val));
        });
        comboRow->addWidget(row, 1);
        return row;
    };

    m_messagesPosCombo = addPosCombo("OsdMessagesPos",    /*drivePerfPreview=*/false);
    m_perfPosCombo     = addPosCombo("OsdPerformancePos", /*drivePerfPreview=*/true);

    v->addLayout(comboRow);
    v->addStretch();
    topRow->addWidget(card, 1);
}
```

### Step 6.2 — Build

```
cmake --build cpp/build -j
```

Expected: clean build. The `Pcsx2ComboRow(card, /*stacked=*/false)` overload was introduced in Plan 2; the `setPreviewStyle(true)` method on `Pcsx2Card` in Plan 3.

### Step 6.3 — Commit

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.cpp
git commit -m "Plan 4 Task 6: OSD page right preview card (preview + OsdScale + position combos)"
```

---

## Task 7 — Bottom grid, loadValues, syncPreview, event filter

**Goal:** the remaining wiring — the 3×3 bottom toggle grid, initial value load, preview sync, and the arrow-key spatial navigation filter copied from the Display page.

### Step 7.1 — Replace `buildBottomToggleGrid`

```cpp
void Pcsx2GraphicsOsdPage::buildBottomToggleGrid(QVBoxLayout* root) {
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        const SettingDef* d = findDef(key);
        if (!d) return card;
        card->setSettingDef(*d);

        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            if (!m_preview) return;
            if      (key == "OsdShowFrameTimes")          m_preview->setShowFrameTimes(on);
            else if (key == "OsdShowIndicators")          m_preview->setShowIndicators(on);
            else if (key == "OsdShowGSStats")             m_preview->setShowGsStats(on);
            else if (key == "OsdShowHardwareInfo")        m_preview->setShowHardwareInfo(on);
            else if (key == "OsdShowVersion")             m_preview->setShowVersion(on);
            else if (key == "OsdShowVideoCapture")        m_preview->setShowVideoCapture(on);
            else if (key == "OsdShowInputRec")            m_preview->setShowInputRec(on);
            else if (key == "OsdShowTextureReplacements") m_preview->setShowTextureReplacements(on);
        });
        v->addWidget(row);
        return card;
    };

    auto* grid = new QGridLayout();
    grid->setSpacing(12);

    grid->addWidget(makeToggleCard("OsdShowFrameTimes"),           0, 0);
    grid->addWidget(makeToggleCard("OsdShowIndicators"),           0, 1);
    grid->addWidget(makeToggleCard("OsdShowGSStats"),              0, 2);
    grid->addWidget(makeToggleCard("OsdShowHardwareInfo"),         1, 0);
    grid->addWidget(makeToggleCard("OsdShowVersion"),              1, 1);
    grid->addWidget(makeToggleCard("OsdShowVideoCapture"),         1, 2);
    grid->addWidget(makeToggleCard("OsdShowInputRec"),             2, 0);
    grid->addWidget(makeToggleCard("OsdShowTextureReplacements"),  2, 1);
    if (findDef("WarnAboutUnsafeSettings"))
        grid->addWidget(makeToggleCard("WarnAboutUnsafeSettings"), 2, 2);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    root->addLayout(grid);
}
```

### Step 7.2 — Replace `loadValues`

```cpp
void Pcsx2GraphicsOsdPage::loadValues() {
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
}
```

### Step 7.3 — Replace `syncPreview`

```cpp
void Pcsx2GraphicsOsdPage::syncPreview() {
    if (!m_preview) return;

    if (m_perfPosCombo) {
        m_preview->setPerformancePos(
            Pcsx2OsdPreview::fromPosValue(m_perfPosCombo->value()));
    }
    if (m_scaleSlider)
        m_preview->setOsdScale(m_scaleSlider->value());

    // Walk every toggle row on the page and feed the current state back
    // into the corresponding preview setter. Using findChildren avoids
    // duplicating the key list.
    for (auto* tog : findChildren<Pcsx2ToggleRow*>()) {
        const QString& key = tog->settingDef().key;
        const bool on = tog->isChecked();
        if      (key == "OsdShowFPS")                 m_preview->setShowFps(on);
        else if (key == "OsdShowSpeed")               m_preview->setShowSpeed(on);
        else if (key == "OsdShowVPS")                 m_preview->setShowVps(on);
        else if (key == "OsdShowResolution")          m_preview->setShowResolution(on);
        else if (key == "OsdShowCPU")                 m_preview->setShowCpu(on);
        else if (key == "OsdShowGPU")                 m_preview->setShowGpu(on);
        else if (key == "OsdShowGSStats")             m_preview->setShowGsStats(on);
        else if (key == "OsdShowFrameTimes")          m_preview->setShowFrameTimes(on);
        else if (key == "OsdShowHardwareInfo")        m_preview->setShowHardwareInfo(on);
        else if (key == "OsdShowVersion")             m_preview->setShowVersion(on);
        else if (key == "OsdShowIndicators")          m_preview->setShowIndicators(on);
        else if (key == "OsdShowVideoCapture")        m_preview->setShowVideoCapture(on);
        else if (key == "OsdShowInputRec")            m_preview->setShowInputRec(on);
        else if (key == "OsdShowTextureReplacements") m_preview->setShowTextureReplacements(on);
        else if (key == "OsdShowSettings")            m_preview->setShowSettings(on);
        else if (key == "OsdshowPatches")             m_preview->setShowPatches(on);
        else if (key == "OsdShowInputs")              m_preview->setShowInputs(on);
    }
}
```

### Step 7.4 — Replace `eventFilter` + `collectFocusables` + `findNextFocusSpatial`

These are verbatim copies of the Plan 3 Display-page spatial nav with the class name substituted.

```cpp
bool Pcsx2GraphicsOsdPage::eventFilter(QObject* obj, QEvent* e) {
    Q_UNUSED(obj);
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        const int k = ke->key();
        if (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down) {
            QWidget* current = QApplication::focusWidget();
            if (current && isAncestorOf(current)) {
                if (auto* combo = qobject_cast<QComboBox*>(current)) {
                    if (combo->view() && combo->view()->isVisible()) {
                        return QWidget::eventFilter(obj, e);
                    }
                }
                if (QWidget* next = findNextFocusSpatial(current, k)) {
                    next->setFocus(Qt::TabFocusReason);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, e);
}

QList<QWidget*> Pcsx2GraphicsOsdPage::collectFocusables() const {
    QList<QWidget*> result;
    const auto all = this->findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (!w->isVisible()) continue;
        if (w->focusPolicy() == Qt::NoFocus) continue;
        if (qobject_cast<QComboBox*>(w)   ||
            qobject_cast<QSlider*>(w)     ||
            qobject_cast<QSpinBox*>(w)    ||
            qobject_cast<Pcsx2Toggle*>(w) ||
            qobject_cast<Pcsx2Card*>(w)) {
            result.append(w);
        }
    }
    return result;
}

QWidget* Pcsx2GraphicsOsdPage::findNextFocusSpatial(QWidget* current, int key) const {
    const auto focusables = collectFocusables();
    if (focusables.size() < 2) return nullptr;

    auto pagePoint = [this](QWidget* w) -> QPoint {
        return w->mapTo(const_cast<Pcsx2GraphicsOsdPage*>(this), QPoint(0, 0));
    };
    const QRect myRect(pagePoint(current), current->size());
    const QPoint myCenter = myRect.center();

    QWidget* best = nullptr;
    long long bestScore = std::numeric_limits<long long>::max();

    for (QWidget* w : focusables) {
        if (w == current) continue;
        const QRect r(pagePoint(w), w->size());
        const QPoint c = r.center();
        const int dx = c.x() - myCenter.x();
        const int dy = c.y() - myCenter.y();

        bool inDir = false;
        switch (key) {
            case Qt::Key_Left:  inDir = dx < 0; break;
            case Qt::Key_Right: inDir = dx > 0; break;
            case Qt::Key_Up:    inDir = dy < 0; break;
            case Qt::Key_Down:  inDir = dy > 0; break;
        }
        if (!inDir) continue;

        const bool vertical = (key == Qt::Key_Up || key == Qt::Key_Down);
        const long long adx = qAbs(dx);
        const long long ady = qAbs(dy);
        const long long score = vertical
            ? (ady * 10000LL + adx)
            : (adx * 10000LL + ady);

        if (score < bestScore) {
            bestScore = score;
            best = w;
        }
    }
    return best;
}
```

### Step 7.5 — Build

```
cmake --build cpp/build -j
```

Expected: clean build. The page compiles but still isn't on screen.

### Step 7.6 — Commit

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_osd_page.cpp
git commit -m "Plan 4 Task 7: OSD page bottom grid, loadValues, syncPreview, spatial nav"
```

---

## Task 8 — Wire `Pcsx2GraphicsOsdPage` into `Pcsx2GraphicsPage`

**Goal:** replace the OSD stub at QStackedWidget index 3 with the real page.

### Step 8.1 — Edit `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp`

Replace the `#include "pcsx2_graphics_stub_sub_page.h"` line with the real-page include _and_ keep the stub include (to remain backward compatible until Plan 4 deletion pass — see Note below):

```cpp
#include "pcsx2_graphics_stub_sub_page.h"
#include "pcsx2_graphics_rendering_page.h"
#include "pcsx2_graphics_post_processing_page.h"
#include "pcsx2_graphics_display_page.h"
#include "pcsx2_graphics_osd_page.h"
```

Then replace the "3: OSD (stub — Plan 4 replaces)" block (lines 67–69 in the current file) with:

```cpp
    // 3: OSD (real — Plan 4)
    auto* osd = new Pcsx2GraphicsOsdPage(m_dialog);
    connect(osd, &Pcsx2GraphicsOsdPage::settingFocused,
            this, &Pcsx2GraphicsPage::settingFocused);
    m_stack->addWidget(osd);
```

Leave the unused `#include "pcsx2_graphics_stub_sub_page.h"` in place and leave `Pcsx2GraphicsStubSubPage` in the source tree; a later cleanup pass can delete them. The landing tab stays `m_tabBar->setCurrentIndex(0)` (Display), unchanged from Plan 3.

**Note for the engineer:** if the compiler warns about the unused include, silence it by dropping the include line; do not delete the stub class header/cpp files themselves in this plan.

### Step 8.2 — Build

```
cmake --build cpp/build -j
```

Expected: clean build. The OSD tab now mounts `Pcsx2GraphicsOsdPage`.

### Step 8.3 — Commit

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp
git commit -m "Plan 4 Task 8: mount Pcsx2GraphicsOsdPage at Graphics stack index 3"
```

---

## Task 9 — Full build, tests, smoke test

**Goal:** regression-proof the plan — run every test, then manually exercise the new sub-page end-to-end.

### Step 9.1 — Full build + full test suite

```
cmake --build cpp/build -j
ctest --test-dir cpp/build --output-on-failure
```

Expected test count: **13 passing** (12 going into Plan 4 + `Pcsx2OsdPreview` added in Task 3). If any pre-existing test fails, STOP and root-cause it; do not mask it by rebasing.

### Step 9.2 — Launch + smoke-test checklist

Launch the app:

```
cmake --build cpp/build -j --target retronest
cpp/build/retronest
```

Walk this checklist. Each item should pass before the plan is considered done.

- [ ] Open a PCSX2 settings dialog, drill into **Graphics**, click the **OSD** sub-tab. Page renders with two top cards (left compound, right preview) and a 3×3 bottom toggle grid.
- [ ] The preview widget paints a dummy PS2 scene (cool-warm gradient + sun disc) inside a dark-bordered 16:9 rectangle.
- [ ] Toggle **Show FPS** on → `FPS: 59.94` appears in the perf column.
- [ ] Toggle **Show Speed Percentages** on → the first perf line becomes `Speed: 100% | FPS: 59.94`, with the `Speed: 100%` fragment drawn in green (`#60ff60`) and the separator + remainder in white.
- [ ] Change **OSD Performance Position** from `Top Right (Default)` to `Top Left` / `Center` / `Bottom Right` → the perf column block anchors to the correct corner of the preview rectangle.
- [ ] Toggle **Show Status Indicators** on → `⏩ FF` appears at the top-right corner of the preview, independent of perf position.
- [ ] Toggle **Show Settings** on in the left compound card → a `DB=2 P=5 C=0 | CR=1 FCDVD VSYNC EER=0 EEC=1` line appears at the bottom-right corner.
- [ ] Toggle **Show Patches** on → the settings line gains a `Patches: Widescreen, NoInterlace` suffix.
- [ ] Toggle **Show Inputs** on → a `🎮 1 • DualShock | A X ↑ LT:0.42` line appears in the bottom-left corner.
- [ ] Drag the **OSD Scale** slider from 100 → 200 → the perf column text size visibly scales up; dragging back below 50 clamps the rendered font to the 6 px floor and still repaints without artefacts.
- [ ] Focus any card or inner row (Tab and arrow keys both) → the description bar updates with that setting's tooltip + recommended pill.
- [ ] Arrow-key spatial navigation: with focus on a toggle in the left card, Right moves into the right card; Down walks through the perf rows; Down past the bottom of a card enters the 3×3 grid. No visual jumps, no focus loss.
- [ ] Close the dialog, reopen → the same values are shown (persistence through `AppController::saveSettings`).
- [ ] Navigate back to **Display**, **Rendering**, **Post-Processing**, **Emulation**, **Audio**, **Memory Cards**. None of them regress in layout, focus, or description-bar behavior.
- [ ] No new warnings in the log when drilling into Graphics/OSD or interacting with any control.

### Step 9.3 — Commit (optional)

No code changes in this task. If the smoke test reveals issues, fix them in-place and commit each fix as `Plan 4 Task 9 fix: <summary>`.

---

## Plan 4 completion criteria

- [ ] All 21 Graphics/OSD schema entries have non-empty `recommendedValue` and `tooltip` (guarded by `testGraphicsOsdSettingsAllHaveRecommended`).
- [ ] `Pcsx2OsdPreview` exists under `cpp/src/ui/settings/pcsx2/widgets/` with the full paint math and is unit-tested (4/4 PASS).
- [ ] `Pcsx2GraphicsOsdPage` exists under `cpp/src/ui/settings/pcsx2/pages/` and replaces the OSD stub at index 3 of `Pcsx2GraphicsPage`.
- [ ] Every toggle, combo, and slider on the OSD page persists through `AppController::saveSettings` AND drives `m_preview` setters.
- [ ] Arrow-key spatial navigation works across the left card, right card, and 3×3 toggle grid.
- [ ] `ctest --test-dir cpp/build` reports 13/13 passing.
- [ ] Full smoke-test checklist in Task 9 passes end-to-end with no regressions on the other Plan 1–3 pages.
- [ ] `Pcsx2GraphicsStubSubPage` is no longer referenced from production code (its header/cpp may still exist in the tree for follow-up cleanup).

## Not in Plan 4

These follow-ups are deliberately deferred:

- **Tab → sub-tab bar navigation.** Pressing Tab inside a Graphics sub-page still does not wrap out to the `Pcsx2GraphicsSubTabBar` above it. Addressed in a dedicated navigation polish plan that spans all four Graphics sub-pages uniformly.
- **Combo popup Enter-to-close behavior.** Pressing Enter on a highlighted item in an open `Pcsx2ComboRow` popup does not always dismiss the popup cleanly on macOS; carried forward from Plan 2.
- **`WarnAboutUnsafeSettings` recategorization.** The spec says this is a startup modal preference, not an OSD overlay. Plan 4 keeps it where it currently lives (Graphics/OSD → Messages) and displays it in the bottom grid; moving it to its proper home (likely Emulation/System) is a follow-up pass.
- **Real speed-colouring logic.** The preview always draws the Speed fragment in green, simulating a healthy 100% emulation speed. Wiring it to an actual hypothetical speed input (for "see what red looks like") is not needed for the settings UI and would add state without user value.
- **Upstream GSStats parity.** The perf-column stats lines (`GS: 4328 draws`, etc.) are representative sample values, not live data. The real overlay renders them at runtime; the preview intentionally uses static strings so engineers and users can see the layout at a glance.
- **`Pcsx2GraphicsStubSubPage` deletion.** Kept around for a future cleanup pass so git history stays bisectable across Plans 1–4.
- **OSD Messages Position live echo.** `OsdMessagesPos` is persisted but does not drive any preview visuals, because transient messages are by definition absent in a static preview. A future "flash sample message" affordance would let us exercise it — out of scope here.