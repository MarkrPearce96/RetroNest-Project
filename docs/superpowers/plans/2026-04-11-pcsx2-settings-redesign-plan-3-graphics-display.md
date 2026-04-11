# PCSX2 Settings Redesign — Plan 3: Graphics / Display

**Spec:** `docs/superpowers/specs/2026-04-11-pcsx2-settings-redesign-design.md`
**Plan series:** 3 of 4 (Foundations → Rendering + Post-Processing → **Graphics Display** → Graphics OSD & Polish)
**Date:** 2026-04-11
**Branch:** `feat/pcsx2-settings-plan-3-graphics-display` (already checked out)

## For agentic workers

Plans 1 and 2 are merged to `main`. Read the previous plan docs if you need background — the schema `recommendedValue` field, `Pcsx2Theme`, every widget primitive (`Pcsx2Card`, `Pcsx2Toggle`, `Pcsx2ToggleRow`, `Pcsx2ComboRow`, `Pcsx2SliderRow`, `Pcsx2SectionHeader`, `Pcsx2DescriptionBar`, `Pcsx2GraphicsSubTabBar`), the `Pcsx2SettingsDialog` shell, the `Pcsx2CategoryHub`, the `Pcsx2EmulationPage`/`Pcsx2AudioPage`/`Pcsx2MemoryCardsPage`, and the `Pcsx2GraphicsPage` umbrella with its eager `QStackedWidget` are all shipped. `Pcsx2GraphicsStubSubPage` currently occupies index 0 (Display) of that stack.

Execute tasks in order. Each task ends in a commit. Do not skip ahead. After each task that touches C++ run:

```
cd cpp && cmake --build build 2>&1 | tail -20
ctest --test-dir build --output-on-failure
```

Plan 3 adds **one new test binary** (`test_pcsx2_aspect_ratio_preview`), so the total test count moves from 11 at the end of Plan 2 to 12 at the end of Plan 3. The existing `test_pcsx2_recommended_values` executable gains one slot but no new binary.

**Do NOT implement in this plan:** OSD sub-page (Plan 4 replaces it), any DuckStation/PPSSPP changes, schema changes beyond `recommendedValue`/`tooltip` for the Graphics/Display entries, slider ticks / spinbox linking, or a real preview of FMV cut-ins. Keep the preview scope exactly as specified.

## Goal

Ship the Graphics / Display sub-page as a fully functional schema-driven screen that replaces `Pcsx2GraphicsStubSubPage` at index 0 of the Graphics page's `QStackedWidget`, AND deliver a live `Pcsx2AspectRatioPreview` widget that paints a miniature of the emulated display as the user edits Aspect Ratio, Vertical Stretch, Crop, and Integer Scaling.

After Plan 3, drilling into Graphics → Display inside the PCSX2 dialog opens a two-column top layout (compound combo/toggle card on the left, preview card on the right) plus a 3×2 toggle grid along the bottom. Every Display setting round-trips through `AppController::settingValue` / `saveSettings`, the description bar shows the new tooltips with an amber `Recommended: …` pill, and the default landing tab for Graphics moves from index 1 (Rendering) back to index 0 (Display) now that Display is real.

## Architecture

```
Pcsx2GraphicsPage (unchanged shell)
  └── QStackedWidget
        ├── index 0: Pcsx2GraphicsDisplayPage        (NEW — Plan 3, replaces stub)
        │     ├── Top row (QHBoxLayout, stretch 1:1)
        │     │     ├── Left: Pcsx2Card (compound)
        │     │     │     ├── Pcsx2ComboRow Renderer
        │     │     │     ├── Pcsx2ComboRow AspectRatio
        │     │     │     ├── Pcsx2ComboRow FMVAspectRatioSwitch
        │     │     │     ├── Pcsx2ComboRow deinterlace_mode
        │     │     │     ├── Pcsx2ComboRow linear_present_mode
        │     │     │     └── Pcsx2ToggleRow EnableWideScreenPatches
        │     │     └── Right: Pcsx2Card (preview variant, #504c48 bg)
        │     │           ├── QLabel "ASPECT RATIO PREVIEW"
        │     │           ├── Pcsx2AspectRatioPreview  (NEW — Plan 3)
        │     │           ├── Pcsx2SliderRow StretchY
        │     │           └── QHBoxLayout: L/T/R/B QSpinBoxes with "px" suffix
        │     └── Bottom row (QGridLayout 3×2) — 6 toggle cards
        │           ├── Anti-Blur                    (pcrtc_antiblur)
        │           ├── Integer Scaling              (IntegerScaling)
        │           ├── No-Interlacing Patches       (EnableNoInterlacingPatches)
        │           ├── Screen Offsets               (pcrtc_offsets)
        │           ├── Disable Interlace Offset     (disable_interlace_offset)
        │           └── Show Overscan                (pcrtc_overscan)
        ├── index 1: Pcsx2GraphicsRenderingPage      (unchanged — Plan 2)
        ├── index 2: Pcsx2GraphicsPostProcessingPage (unchanged — Plan 2)
        └── index 3: Pcsx2GraphicsStubSubPage (OSD)  (unchanged — Plan 4 replaces)
```

`Pcsx2GraphicsDisplayPage` re-emits `settingFocused(SettingDef)` from every row, and `Pcsx2GraphicsPage` already re-emits it up to the dialog. This matches the Plan 2 pattern — no changes to the umbrella page wiring beyond swapping the stub for the real page.

### Keyboard navigation contract (read carefully)

The left compound card is a single `Pcsx2Card`. The spatial-arrow algorithm in `Pcsx2Card::keyPressEvent` (see `pcsx2_card.cpp`) only fires when the Pcsx2Card itself has focus; inside the card, standard Qt Tab / Shift+Tab focus chain moves between the five `Pcsx2ComboRow`s and the `Pcsx2ToggleRow`. Once the user Tab's into an inner row, Left/Right/Up/Down are consumed by Qt's default combo/toggle handling, not by `Pcsx2Card`. This is the intended behaviour and is documented in the spec under §Compound-card navigation — **do not** hand-roll arrow handling inside the compound card.

Concretely:

- Arrow keys between *cards*: Pcsx2Card spatial nav (left compound card ↔ right preview card ↔ bottom toggle grid cards).
- Tab / Shift+Tab within the compound card: moves between its six inner rows using Qt's default focus chain.
- Focus of an inner row fires `focused(def)` → re-emitted by the page → description bar updates.
- When the compound card itself has focus (user Tab'd onto the frame before any inner control), the description bar shows the AR setting (since we set AR as the compound card's representative `settingDef`).

## Tech Stack

- **Language:** C++17
- **UI framework:** Qt6 Widgets, `AUTOMOC ON`
- **Styling:** stylesheet strings via `Pcsx2Theme`; `paintEvent` only where stylesheet can't express it (preview widget draw rect)
- **Build system:** CMake 3.16, explicit source lists in `cpp/CMakeLists.txt`
- **Tests:** QtTest — one new slot in `test_pcsx2_recommended_values.cpp` and one new executable `test_pcsx2_aspect_ratio_preview`
- **Palette:** inherited from `pcsx2_theme.h` (Plan 1) — Plan 3 adds a `previewCardBg` constant

---

## Task 1: Populate `recommendedValue` + tooltips for Graphics/Display (TDD)

**Files:**
- `cpp/tests/test_pcsx2_recommended_values.cpp` (modify)
- `cpp/src/adapters/pcsx2_adapter.cpp` (modify)

Plans 1 and 2 converted Emulation / Audio / Memory Cards / Graphics-Rendering / Graphics-Post-Processing to the scoped-block pattern. Plan 3 does the same for Graphics/Display. Every entry currently has an empty `""` tooltip slot (positional arg 7) AND an empty `recommendedValue`; both are filled in this task.

- [ ] Add a new test slot to `cpp/tests/test_pcsx2_recommended_values.cpp` after the Plan 2 `testGraphicsPostProcessingSettingsAllHaveRecommended` slot:

```cpp
    void testGraphicsDisplaySettingsAllHaveRecommended() {
        int count = 0;
        for (const auto& d : schema_) {
            if (d.category != "Graphics") continue;
            if (d.subcategory != "Display") continue;
            ++count;
            QVERIFY2(!d.recommendedValue.isEmpty(),
                     qPrintable(QString("missing recommendedValue for Graphics/Display/%1").arg(d.key)));
            QVERIFY2(!d.tooltip.isEmpty(),
                     qPrintable(QString("missing tooltip for Graphics/Display/%1").arg(d.key)));
        }
        QVERIFY2(count >= 15,
                 qPrintable(QString("expected >= 15 Graphics/Display settings, got %1").arg(count)));
    }
```

- [ ] Build + run the test — expect FAIL on the first Display entry (`Renderer`):

```
cd cpp && cmake --build build --target test_pcsx2_recommended_values \
  && ctest --test-dir build -R Pcsx2RecommendedValues --output-on-failure
```

- [ ] Edit `cpp/src/adapters/pcsx2_adapter.cpp`. Replace the entire `// Graphics > Display` block (currently spans roughly lines 165–212) with the scoped-block form below. All recommended values mirror PCSX2 upstream `Pcsx2Config::GSOptions` defaults from `pcsx2/Config.h` — which in every case equals our existing `defaultValue`. Tooltip copy is user-facing, concise, and avoids jargon.

**Mapping table (authoritative — match byte-for-byte):**

| key | recommendedValue | tooltip |
|---|---|---|
| `Renderer` | `-1` | `Selects which backend PCSX2 uses to render frames. Auto picks the best option for your GPU; Vulkan and Metal are fastest on modern hardware, OpenGL is the most compatible, Software emulates the GS on CPU for perfect accuracy.` |
| `AspectRatio` | `4:3` | `Controls the aspect ratio of the emulated display. Auto selects 4:3 for interlaced games and 3:2 for progressive games. 16:9 stretches the image for widescreen TVs; Stretch fills the whole window.` |
| `FMVAspectRatioSwitch` | `Off` | `Overrides the aspect ratio only while full-motion video (FMV) is playing. Useful for games with widescreen cutscenes inside a 4:3 main game.` |
| `deinterlace_mode` | `0` | `Selects how interlaced frames are combined for progressive display. Automatic picks the best option per game; Weave preserves detail at the cost of combing; Bob and Blend smooth motion at the cost of vertical resolution.` |
| `linear_present_mode` | `1` | `Applies a bilinear filter when scaling the final image to the window. Smooth is the standard option; Sharp uses a pixel-art-friendly variant that keeps edges crisp.` |
| `StretchY` | `100` | `Multiplies the display height after aspect-ratio fitting. Values above 100% make the image taller than its letterbox; values below leave extra vertical space. Default is 100%.` |
| `CropLeft` | `0` | `Trims pixels from the left edge of the source image before it's fit to the display window. Useful for games with garbage pixels at the border.` |
| `CropTop` | `0` | `Trims pixels from the top edge of the source image before it's fit to the display window. Useful for games with garbage pixels at the border.` |
| `CropRight` | `0` | `Trims pixels from the right edge of the source image before it's fit to the display window. Useful for games with garbage pixels at the border.` |
| `CropBottom` | `0` | `Trims pixels from the bottom edge of the source image before it's fit to the display window. Useful for games with garbage pixels at the border.` |
| `EnableWideScreenPatches` | `false` | `Automatically applies community widescreen patches to supported games. Reshapes the rendering to true 16:9 instead of stretching the 4:3 picture.` |
| `EnableNoInterlacingPatches` | `false` | `Automatically applies community no-interlacing patches to supported games. Removes flicker in games that render in interlaced mode.` |
| `pcrtc_antiblur` | `true` | `Enables internal anti-blur hacks that remove the PS2's GS smear on commonly-affected games. Safe to leave on.` |
| `IntegerScaling` | `false` | `Snaps the rendered image to an integer multiple of the source pixel size. Produces crisp pixel-art scaling at the cost of leaving letterbox bars.` |
| `pcrtc_offsets` | `false` | `Enables PCRTC offsets so the screen is positioned exactly where the game requests. Fixes games that deliberately offset the viewport.` |
| `disable_interlace_offset` | `false` | `Disables the half-pixel interlace offset which can reduce jitter on some games that render at half vertical resolution.` |
| `pcrtc_overscan` | `false` | `Shows the overscan area of the display that would normally be hidden by a CRT bezel. Exposes any garbage the game draws outside the safe area.` |

Now replace the block:

```cpp
    // ═══════════════════════════════════════════════════════════════════════
    // Graphics > Display
    // ═══════════════════════════════════════════════════════════════════════
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "Renderer", "Renderer",
                     "Selects which backend PCSX2 uses to render frames. Auto picks the best option for your GPU; "
                     "Vulkan and Metal are fastest on modern hardware, OpenGL is the most compatible, Software "
                     "emulates the GS on CPU for perfect accuracy.",
                     SettingDef::Combo, "-1",
                     {{"Auto", "-1"}, {"OpenGL", "12"}, {"Vulkan", "14"},
#if defined(Q_OS_MACOS)
                      {"Metal", "17"},
#endif
                      {"Software", "13"}}, 0, 0, 0};
        d.recommendedValue = "-1"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "AspectRatio", "Aspect Ratio",
                     "Controls the aspect ratio of the emulated display. Auto selects 4:3 for interlaced games and "
                     "3:2 for progressive games. 16:9 stretches the image for widescreen TVs; Stretch fills the "
                     "whole window.",
                     SettingDef::Combo, "4:3",
                     {{"Auto 4:3/3:2", "Auto 4:3/3:2"}, {"4:3", "4:3"}, {"16:9", "16:9"},
                      {"10:7", "10:7"}, {"Stretch", "Stretch"}}, 0, 0, 0};
        d.recommendedValue = "4:3"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "FMVAspectRatioSwitch", "FMV Aspect Ratio Override",
                     "Overrides the aspect ratio only while full-motion video (FMV) is playing. Useful for games "
                     "with widescreen cutscenes inside a 4:3 main game.",
                     SettingDef::Combo, "Off",
                     {{"Off (Default)", "Off"}, {"Auto Standard (4:3 Interlaced / 3:2 Progressive)", "Auto 4:3/3:2"},
                      {"Standard (4:3)", "4:3"}, {"Widescreen (16:9)", "16:9"}, {"Native/Full (10:7)", "10:7"}}, 0, 0, 0};
        d.recommendedValue = "Off"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "deinterlace_mode", "Deinterlacing",
                     "Selects how interlaced frames are combined for progressive display. Automatic picks the best "
                     "option per game; Weave preserves detail at the cost of combing; Bob and Blend smooth motion "
                     "at the cost of vertical resolution.",
                     SettingDef::Combo, "0",
                     {{"Automatic", "0"}, {"Off", "1"}, {"Weave (Top)", "2"}, {"Weave (Bottom)", "3"},
                      {"Bob (Top)", "4"}, {"Bob (Bottom)", "5"}, {"Blend (Top)", "6"}, {"Blend (Bottom)", "7"},
                      {"Adaptive (Top)", "8"}, {"Adaptive (Bottom)", "9"}}, 0, 0, 0};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "linear_present_mode", "Bilinear Filtering",
                     "Applies a bilinear filter when scaling the final image to the window. Smooth is the standard "
                     "option; Sharp uses a pixel-art-friendly variant that keeps edges crisp.",
                     SettingDef::Combo, "1",
                     {{"None", "0"}, {"Bilinear (Smooth)", "1"}, {"Bilinear (Sharp)", "2"}}, 0, 0, 0};
        d.recommendedValue = "1"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "StretchY", "Vertical Stretch",
                     "Multiplies the display height after aspect-ratio fitting. Values above 100% make the image "
                     "taller than its letterbox; values below leave extra vertical space. Default is 100%.",
                     SettingDef::Int, "100", {}, 10, 300, 1, "", "%"};
        d.recommendedValue = "100"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "CropLeft", "Left",
                     "Trims pixels from the left edge of the source image before it's fit to the display window. "
                     "Useful for games with garbage pixels at the border.",
                     SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "CropTop", "Top",
                     "Trims pixels from the top edge of the source image before it's fit to the display window. "
                     "Useful for games with garbage pixels at the border.",
                     SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "CropRight", "Right",
                     "Trims pixels from the right edge of the source image before it's fit to the display window. "
                     "Useful for games with garbage pixels at the border.",
                     SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "CropBottom", "Bottom",
                     "Trims pixels from the bottom edge of the source image before it's fit to the display window. "
                     "Useful for games with garbage pixels at the border.",
                     SettingDef::Int, "0", {}, 0, 100, 1, "paired", "px"};
        d.recommendedValue = "0"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore", "EnableWideScreenPatches", "Apply Widescreen Patches",
                     "Automatically applies community widescreen patches to supported games. Reshapes the "
                     "rendering to true 16:9 instead of stretching the 4:3 picture.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore", "EnableNoInterlacingPatches", "Apply No-Interlacing Patches",
                     "Automatically applies community no-interlacing patches to supported games. Removes flicker "
                     "in games that render in interlaced mode.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "pcrtc_antiblur", "Anti-Blur",
                     "Enables internal anti-blur hacks that remove the PS2's GS smear on commonly-affected games. "
                     "Safe to leave on.",
                     SettingDef::Bool, "true", {}, 0, 0, 0};
        d.recommendedValue = "true"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "IntegerScaling", "Integer Scaling",
                     "Snaps the rendered image to an integer multiple of the source pixel size. Produces crisp "
                     "pixel-art scaling at the cost of leaving letterbox bars.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "pcrtc_offsets", "Screen Offsets",
                     "Enables PCRTC offsets so the screen is positioned exactly where the game requests. Fixes "
                     "games that deliberately offset the viewport.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "disable_interlace_offset", "Disable Interlace Offset",
                     "Disables the half-pixel interlace offset which can reduce jitter on some games that render "
                     "at half vertical resolution.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
    {
        SettingDef d{"Graphics", "Display", "", "EmuCore/GS", "pcrtc_overscan", "Show Overscan",
                     "Shows the overscan area of the display that would normally be hidden by a CRT bezel. "
                     "Exposes any garbage the game draws outside the safe area.",
                     SettingDef::Bool, "false", {}, 0, 0, 0};
        d.recommendedValue = "false"; s.append(d);
    }
```

Note: every `recommendedValue` above equals `defaultValue`. Per the convention established in Plans 1 and 2, we copy `defaultValue` into `recommendedValue` verbatim because upstream PCSX2 has no separate "recommendation" metadata; defaults already represent the recommended baseline.

- [ ] Re-run the test — expect PASS:

```
cd cpp && cmake --build build --target test_pcsx2_recommended_values \
  && ctest --test-dir build -R Pcsx2RecommendedValues --output-on-failure
```

- [ ] Commit:

```
git add cpp/src/adapters/pcsx2_adapter.cpp cpp/tests/test_pcsx2_recommended_values.cpp
git commit -m "Populate PCSX2 Graphics/Display recommendedValue + tooltips

Seventeen Display settings (Renderer, AspectRatio, FMVAspectRatioSwitch,
deinterlace_mode, linear_present_mode, StretchY, four Crop axes,
EnableWideScreenPatches, EnableNoInterlacingPatches, pcrtc_antiblur,
IntegerScaling, pcrtc_offsets, disable_interlace_offset, pcrtc_overscan)
now carry both tooltip copy and recommendedValue mirroring upstream
Pcsx2Config::GSOptions defaults. Guarded by a new slot in
test_pcsx2_recommended_values asserting count >= 15 and that both
fields are non-empty for every Graphics/Display entry."
```

---

## Task 2: `Pcsx2AspectRatioPreview` widget skeleton + paint math

**Files:**
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h` (create)
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp` (create)
- `cpp/src/ui/settings/pcsx2/pcsx2_theme.h` (modify — add one constant)
- `cpp/CMakeLists.txt` (modify)

The preview draws a miniature of the emulated display computed from the same pipeline PCSX2 upstream uses in `GSRenderer::CalculateDrawDstRect` + `CalculateDrawSrcRect` (`pcsx2-master/pcsx2/GS/GSRenderer.cpp:314-443`), scaled down to a 16:9 client widget with a fixed 640×448 NTSC source. Math is pure CPU — no GPU, no textures.

- [ ] Extend `pcsx2_theme.h` by appending one new colour accessor (so the preview card can reuse the palette):

```cpp
inline QColor previewCardBg() { return QColor("#504c48"); }
```

Place it immediately after `letterbox()` in the namespace block. The constant matches the spec's `preview-card` swatch.

- [ ] Create `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h`:

```cpp
#pragma once
#include <QWidget>
#include <QRectF>

// Live aspect-ratio / crop / stretch / integer-scaling preview used
// inside Pcsx2GraphicsDisplayPage. Reproduces the draw-rect math from
// PCSX2 upstream GSRenderer::CalculateDrawDstRect +
// CalculateDrawSrcRect on a fixed 640x448 NTSC source, scaled down
// into a 16:9 widget. All state is held in this widget — callers use
// the setters below and the widget repaints itself.
class Pcsx2AspectRatioPreview : public QWidget {
    Q_OBJECT
public:
    // Matches the five options of the Display/AspectRatio combo.
    // Upstream string values: "Stretch", "Auto 4:3/3:2", "4:3", "16:9",
    // "10:7".
    enum class AspectRatio {
        Stretch,
        Auto4_3_3_2,
        R4_3,
        R16_9,
        R10_7
    };

    explicit Pcsx2AspectRatioPreview(QWidget* parent = nullptr);

    void setAspectRatio(AspectRatio ratio);
    void setStretchY(int percent);                 // 10..300, default 100
    void setCrop(int left, int top, int right, int bottom);
    void setIntegerScaling(bool on);

    // Convenience: map the schema's AspectRatio combo value string to
    // the enum. Unknown strings fall back to R4_3.
    static AspectRatio fromSchemaValue(const QString& v);

    QSize sizeHint() const override        { return QSize(320, 180); }
    QSize minimumSizeHint() const override { return QSize(240, 135); }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    // Pure-function draw-rect computation. Returns a QRectF in widget
    // coordinates representing where the game scene is painted. See the
    // comment block at the top of pcsx2_aspect_ratio_preview.cpp for
    // the step-by-step mapping to upstream GSRenderer.cpp.
    QRectF computeDrawRect(const QRectF& client) const;

    QString labelForCurrentRatio() const;

    AspectRatio m_ratio = AspectRatio::R4_3;
    int m_stretchY      = 100;
    int m_cropL = 0, m_cropT = 0, m_cropR = 0, m_cropB = 0;
    bool m_integerScaling = false;
};
```

- [ ] Create `cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp`:

```cpp
#include "pcsx2_aspect_ratio_preview.h"
#include "../pcsx2_theme.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────
// Draw-rect math — mirrors PCSX2 upstream GSRenderer.cpp:314-443.
//
// The upstream pipeline is:
//   1. Start from the emulator window client rect (here: our widget
//      content rect).                       — GSRenderer.cpp:316
//   2. Take the source PCRTC rect (here: fixed 640x448 NTSC) and apply
//      Crop{Left,Top,Right,Bottom} by subtracting from the four edges
//      to produce a cropped source rect.    — GSRenderer.cpp:328-341
//   3. Compute `src_rect_ar = cropped.w / cropped.h`
//      and `src_size_ar = 640 / 448`, then
//      `crop_adjust = src_rect_ar / src_size_ar`.
//                                            — GSRenderer.cpp:355-360
//   4. Pick target_ar from the AspectRatio enum:
//        Stretch     → client.w / client.h  (degenerates to client_ar)
//        Auto 4:3/3:2 → 4/3                 (assume interlaced here)
//        4:3          → 4/3
//        16:9         → 16/9
//        10:7         → 10/7                — GSRenderer.cpp:370-400
//   5. client_ar = client.w / client.h.
//   6. arr = (target_ar * crop_adjust) / client_ar.
//      If arr < 1: result is pillarboxed. width = client.w * arr,
//                  height = client.h.
//      Else:       result is letterboxed. width = client.w,
//                  height = client.h / arr.
//                                            — GSRenderer.cpp:402-420
//   7. Apply StretchY: result.h *= stretchY / 100.0.
//                                            — GSRenderer.cpp:422-428
//   8. If integerScaling: snap result.w, result.h to the nearest
//      whole-number multiple of (640, 448) that still fits inside
//      the client. Minimum multiple is 1.   — GSRenderer.cpp:430-440
//   9. Center the resulting rect inside the client.
//                                            — GSRenderer.cpp:442
//
// The widget paints:
//   - Background: Pcsx2Theme::letterbox() (#3a3632) over whole widget.
//   - Game scene: warm-ground → cool-sky vertical gradient clipped to
//     the computed draw rect. This is a visual stand-in for a PS2
//     source frame — we're showing the *geometry*, not an actual game.
//   - Draw-rect border: 1px amber stroke.
//   - Centered label: the effective ratio name ("4 : 3", "16 : 9",
//     "Stretch", etc.) drawn in the text primary colour.
//   - Cropped-out regions: drawn *inside* the game-scene rect as dark
//     overlays matching the letterbox colour, so the user can see
//     which slice of the source is being clipped.
// ─────────────────────────────────────────────────────────────────────

namespace {
constexpr double kSrcW = 640.0;
constexpr double kSrcH = 448.0;
}

Pcsx2AspectRatioPreview::Pcsx2AspectRatioPreview(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    setMinimumSize(minimumSizeHint());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void Pcsx2AspectRatioPreview::setAspectRatio(AspectRatio ratio) {
    if (m_ratio == ratio) return;
    m_ratio = ratio;
    update();
}

void Pcsx2AspectRatioPreview::setStretchY(int percent) {
    if (percent < 10)  percent = 10;
    if (percent > 300) percent = 300;
    if (m_stretchY == percent) return;
    m_stretchY = percent;
    update();
}

void Pcsx2AspectRatioPreview::setCrop(int left, int top, int right, int bottom) {
    m_cropL = qBound(0, left,   100);
    m_cropT = qBound(0, top,    100);
    m_cropR = qBound(0, right,  100);
    m_cropB = qBound(0, bottom, 100);
    update();
}

void Pcsx2AspectRatioPreview::setIntegerScaling(bool on) {
    if (m_integerScaling == on) return;
    m_integerScaling = on;
    update();
}

Pcsx2AspectRatioPreview::AspectRatio
Pcsx2AspectRatioPreview::fromSchemaValue(const QString& v) {
    if (v == "Stretch")        return AspectRatio::Stretch;
    if (v == "Auto 4:3/3:2")   return AspectRatio::Auto4_3_3_2;
    if (v == "4:3")            return AspectRatio::R4_3;
    if (v == "16:9")           return AspectRatio::R16_9;
    if (v == "10:7")           return AspectRatio::R10_7;
    return AspectRatio::R4_3;
}

QString Pcsx2AspectRatioPreview::labelForCurrentRatio() const {
    switch (m_ratio) {
        case AspectRatio::Stretch:     return QStringLiteral("Stretch");
        case AspectRatio::Auto4_3_3_2: return QStringLiteral("Auto 4 : 3");
        case AspectRatio::R4_3:        return QStringLiteral("4 : 3");
        case AspectRatio::R16_9:       return QStringLiteral("16 : 9");
        case AspectRatio::R10_7:       return QStringLiteral("10 : 7");
    }
    return {};
}

QRectF Pcsx2AspectRatioPreview::computeDrawRect(const QRectF& client) const {
    if (client.width() <= 1.0 || client.height() <= 1.0)
        return client;

    // Step 2: cropped source (pixels cut off the 640x448 frame).
    const double crLeft   = static_cast<double>(m_cropL);
    const double crTop    = static_cast<double>(m_cropT);
    const double crRight  = static_cast<double>(m_cropR);
    const double crBottom = static_cast<double>(m_cropB);

    double crop_w = kSrcW - crLeft - crRight;
    double crop_h = kSrcH - crTop  - crBottom;
    if (crop_w < 1.0) crop_w = 1.0;
    if (crop_h < 1.0) crop_h = 1.0;

    // Step 3.
    const double src_rect_ar = crop_w / crop_h;
    const double src_size_ar = kSrcW / kSrcH;
    const double crop_adjust = src_rect_ar / src_size_ar;

    // Step 4.
    const double client_ar = client.width() / client.height();
    double target_ar = 4.0 / 3.0;
    switch (m_ratio) {
        case AspectRatio::Stretch:     target_ar = client_ar;     break;
        case AspectRatio::Auto4_3_3_2: target_ar = 4.0 / 3.0;     break;
        case AspectRatio::R4_3:        target_ar = 4.0 / 3.0;     break;
        case AspectRatio::R16_9:       target_ar = 16.0 / 9.0;    break;
        case AspectRatio::R10_7:       target_ar = 10.0 / 7.0;    break;
    }

    // Step 6.
    const double arr = (target_ar * crop_adjust) / client_ar;
    double w, h;
    if (arr < 1.0) {
        w = client.width() * arr;
        h = client.height();
    } else {
        w = client.width();
        h = client.height() / arr;
    }

    // Step 7: Vertical Stretch.
    h *= static_cast<double>(m_stretchY) / 100.0;

    // Clamp to client bounds so a stretched value doesn't escape the widget.
    if (w > client.width())  w = client.width();
    if (h > client.height()) h = client.height();

    // Step 8: integer scaling (snap to the largest integer multiple of
    // the source that still fits inside both w and h).
    if (m_integerScaling) {
        const double mult_w = std::floor(w / kSrcW);
        const double mult_h = std::floor(h / kSrcH);
        double mult = std::min(mult_w, mult_h);
        if (mult < 1.0) mult = 1.0;
        w = kSrcW * mult;
        h = kSrcH * mult;
        // Final clamp — at small preview sizes the single-multiple
        // result may still overshoot; if so fall back to the unsnapped
        // size so the preview stays visible.
        if (w > client.width() || h > client.height()) {
            w = std::min(w, client.width());
            h = std::min(h, client.height());
        }
    }

    // Step 9: center.
    const double x = client.left() + (client.width()  - w) * 0.5;
    const double y = client.top()  + (client.height() - h) * 0.5;
    return QRectF(x, y, w, h);
}

void Pcsx2AspectRatioPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Background — letterbox/pillarbox colour everywhere. Any portion
    // not covered by the draw rect below stays this colour.
    const QRectF full = rect();
    p.fillRect(full, Pcsx2Theme::letterbox());

    // Inset so the preview "screen" has a little breathing room.
    const QRectF client = full.adjusted(6, 6, -6, -6);

    // Compute the draw rect from current state.
    const QRectF dst = computeDrawRect(client);

    // Paint a warm-ground → cool-sky gradient inside the draw rect as
    // the stand-in "game scene". This matches the HTML mockup swatch
    // — warm brown at the bottom, muted teal at the top.
    QLinearGradient grad(dst.topLeft(), dst.bottomLeft());
    grad.setColorAt(0.0, QColor("#6c8b9c"));   // sky
    grad.setColorAt(0.55, QColor("#b7a589"));  // horizon
    grad.setColorAt(1.0, QColor("#6b513a"));   // ground
    p.fillRect(dst, grad);

    // Show cropped regions as dark overlays *inside* the dst rect.
    // They're proportional to the crop values vs the full source.
    if (m_cropL || m_cropT || m_cropR || m_cropB) {
        const double scaleX = dst.width()  / kSrcW;
        const double scaleY = dst.height() / kSrcH;
        p.setBrush(Pcsx2Theme::letterbox());
        p.setPen(Qt::NoPen);
        if (m_cropL > 0)
            p.drawRect(QRectF(dst.left(), dst.top(), m_cropL * scaleX, dst.height()));
        if (m_cropR > 0)
            p.drawRect(QRectF(dst.right() - m_cropR * scaleX, dst.top(),
                              m_cropR * scaleX, dst.height()));
        if (m_cropT > 0)
            p.drawRect(QRectF(dst.left(), dst.top(), dst.width(), m_cropT * scaleY));
        if (m_cropB > 0)
            p.drawRect(QRectF(dst.left(), dst.bottom() - m_cropB * scaleY,
                              dst.width(), m_cropB * scaleY));
    }

    // 1px amber border around the draw rect.
    {
        QPen pen(Pcsx2Theme::accent(), 1.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(dst.adjusted(0.5, 0.5, -0.5, -0.5));
    }

    // Centered ratio label over the draw rect.
    {
        QFont f = p.font();
        f.setPointSize(11);
        f.setBold(true);
        p.setFont(f);
        p.setPen(Pcsx2Theme::textPrimary());
        p.drawText(dst, Qt::AlignCenter, labelForCurrentRatio());
    }
}
```

- [ ] Register in `cpp/CMakeLists.txt` — add the new `.cpp` to SOURCES and `.h` to HEADERS, alphabetically next to the other `widgets/pcsx2_*` entries.

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

Expected: compiles cleanly. No test yet — Task 3 adds one.

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h \
        cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp \
        cpp/src/ui/settings/pcsx2/pcsx2_theme.h \
        cpp/CMakeLists.txt
git commit -m "Add Pcsx2AspectRatioPreview widget

Live draw-rect preview that reproduces PCSX2 upstream's
GSRenderer::CalculateDrawDstRect + CalculateDrawSrcRect math on a
fixed 640x448 NTSC source, scaled into a 16:9 client widget. Paints a
letterbox backdrop, a warm-ground-cool-sky gradient inside the
computed draw rect, crop overlays for the cropped slices, an amber
1px border, and a centered ratio label. Callers drive it via
setAspectRatio / setStretchY / setCrop / setIntegerScaling setters
that each call update(). Adds previewCardBg() (#504c48) to
Pcsx2Theme for the preview card variant built in Task 6."
```

---

## Task 3: Unit test for `Pcsx2AspectRatioPreview`

**Files:**
- `cpp/tests/test_pcsx2_aspect_ratio_preview.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

A lightweight smoke test. We don't verify pixel output — that's brittle against anti-aliasing, font metrics, and platform painter differences. We only verify the widget constructs, setters don't crash, and `sizeHint()` / `minimumSizeHint()` return the values declared in the header. This mirrors the Plan 1 convention of keeping new test binaries focused.

- [ ] Create `cpp/tests/test_pcsx2_aspect_ratio_preview.cpp`:

```cpp
#include <QtTest>
#include <QApplication>
#include "ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h"

class TestPcsx2AspectRatioPreview : public QObject {
    Q_OBJECT
private slots:
    void testSizeHint() {
        Pcsx2AspectRatioPreview w;
        QCOMPARE(w.sizeHint(),        QSize(320, 180));
        QCOMPARE(w.minimumSizeHint(), QSize(240, 135));
    }

    void testSettersDoNotCrash() {
        Pcsx2AspectRatioPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::Stretch);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::Auto4_3_3_2);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::R4_3);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::R16_9);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::R10_7);

        w.setStretchY(10);
        w.setStretchY(130);
        w.setStretchY(300);
        w.setStretchY(-99);   // clamped to 10
        w.setStretchY(9999);  // clamped to 300

        w.setCrop(0, 0, 0, 0);
        w.setCrop(20, 30, 40, 50);
        w.setCrop(999, 999, 999, 999); // clamped to 100 per axis

        w.setIntegerScaling(true);
        w.setIntegerScaling(false);

        // Force a repaint to exercise computeDrawRect + paintEvent.
        w.repaint();
    }

    void testFromSchemaValue() {
        using AR = Pcsx2AspectRatioPreview::AspectRatio;
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("Stretch"),      AR::Stretch);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("Auto 4:3/3:2"), AR::Auto4_3_3_2);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("4:3"),          AR::R4_3);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("16:9"),         AR::R16_9);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("10:7"),         AR::R10_7);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("garbage"),      AR::R4_3);
    }

    void testTinyClientDoesNotDivideByZero() {
        Pcsx2AspectRatioPreview w;
        w.resize(1, 1);  // degenerate — computeDrawRect should short-circuit
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::R16_9);
        w.setStretchY(300);
        w.setCrop(50, 50, 50, 50);
        w.setIntegerScaling(true);
        w.repaint();   // must not crash
    }
};

QTEST_MAIN(TestPcsx2AspectRatioPreview)
#include "test_pcsx2_aspect_ratio_preview.moc"
```

- [ ] Register the new executable in `cpp/CMakeLists.txt`. Mirror the existing `add_executable(test_pcsx2_recommended_values …)` + `add_test(…)` block pattern. Add after the Plan 2 test block:

```cmake
add_executable(test_pcsx2_aspect_ratio_preview
    tests/test_pcsx2_aspect_ratio_preview.cpp
    src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp
    src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h)
target_link_libraries(test_pcsx2_aspect_ratio_preview PRIVATE
    Qt6::Core Qt6::Widgets Qt6::Test)
target_include_directories(test_pcsx2_aspect_ratio_preview PRIVATE src)
add_test(NAME Pcsx2AspectRatioPreview COMMAND test_pcsx2_aspect_ratio_preview)
```

(Match the exact variable names and link lists used by the other `test_pcsx2_*` executables in the file — do not invent new ones.)

- [ ] Build + run:

```
cd cpp && cmake --build build --target test_pcsx2_aspect_ratio_preview \
  && ctest --test-dir build -R Pcsx2AspectRatioPreview --output-on-failure
```

Expected: 4/4 test slots pass. Full `ctest` count moves from 11 → 12.

- [ ] Commit:

```
git add cpp/tests/test_pcsx2_aspect_ratio_preview.cpp cpp/CMakeLists.txt
git commit -m "Add unit test for Pcsx2AspectRatioPreview

Four slots cover sizeHint / minimumSizeHint, the full setter surface
including out-of-range clamping, fromSchemaValue mapping for all five
combo strings, and a 1x1 client rect degenerate case to prove
computeDrawRect short-circuits instead of dividing by zero. Does not
verify pixel output — painter determinism varies across platforms."
```

---

## Task 4: `Pcsx2GraphicsDisplayPage` header + constructor skeleton

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.h` (create)
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp` (create)
- `cpp/CMakeLists.txt` (modify)

Set up the empty page that we'll fill with UI in Tasks 5-7. This task gets the class compiling with an empty `buildUi()` and the schema filter so subsequent tasks can focus purely on layout code.

- [ ] `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.h`:

```cpp
#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class Pcsx2Card;
class Pcsx2AspectRatioPreview;
class Pcsx2ComboRow;
class Pcsx2ToggleRow;
class Pcsx2SliderRow;
class QSpinBox;

// Real Graphics/Display sub-page for the PCSX2 settings dialog. Plan 3
// replaces the Pcsx2GraphicsStubSubPage that currently occupies index 0
// of Pcsx2GraphicsPage's QStackedWidget.
//
// Layout (spec §Graphics/Display):
//   Top row (QHBoxLayout, stretch 1:1)
//     Left:  compound Pcsx2Card — 5 combos + 1 toggle (Widescreen Patches)
//     Right: preview Pcsx2Card  — preview widget + Vertical Stretch + Crop spinboxes
//   Bottom row (QGridLayout 3x2) — 6 toggle cards
class Pcsx2GraphicsDisplayPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsDisplayPage(Pcsx2SettingsDialog* dialog);

signals:
    void settingFocused(SettingDef def);

private:
    void buildUi();
    void buildLeftCompoundCard(QHBoxLayout* topRow);
    void buildRightPreviewCard(QHBoxLayout* topRow);
    void buildBottomToggleGrid(QVBoxLayout* root);

    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    // Pushes current Aspect Ratio / Stretch / Crop / IntegerScaling
    // state from the respective widgets into the preview widget.
    void syncPreview();

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef>  m_schema; // filtered category=="Graphics" && subcategory=="Display"

    // Widgets the page needs to reference across build steps.
    Pcsx2AspectRatioPreview* m_preview = nullptr;
    Pcsx2ComboRow*  m_aspectCombo = nullptr;
    Pcsx2SliderRow* m_stretchSlider = nullptr;
    QSpinBox* m_cropL = nullptr;
    QSpinBox* m_cropT = nullptr;
    QSpinBox* m_cropR = nullptr;
    QSpinBox* m_cropB = nullptr;
    Pcsx2ToggleRow* m_integerScalingToggle = nullptr;
};
```

- [ ] `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp` (skeleton only — Tasks 5-7 fill `buildLeftCompoundCard` / `buildRightPreviewCard` / `buildBottomToggleGrid`):

```cpp
#include "pcsx2_graphics_display_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../widgets/pcsx2_slider_row.h"
#include "../widgets/pcsx2_aspect_ratio_preview.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>

Pcsx2GraphicsDisplayPage::Pcsx2GraphicsDisplayPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "Display")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    syncPreview();
}

const SettingDef* Pcsx2GraphicsDisplayPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2GraphicsDisplayPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void Pcsx2GraphicsDisplayPage::buildUi() {
    auto* root = new QVBoxLayout(this);
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

// Filled in Task 5.
void Pcsx2GraphicsDisplayPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    Q_UNUSED(topRow);
}

// Filled in Task 6.
void Pcsx2GraphicsDisplayPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    Q_UNUSED(topRow);
}

// Filled in Task 7.
void Pcsx2GraphicsDisplayPage::buildBottomToggleGrid(QVBoxLayout* root) {
    Q_UNUSED(root);
}

// Filled in Task 7 (once all widgets exist).
void Pcsx2GraphicsDisplayPage::loadValues() {}
void Pcsx2GraphicsDisplayPage::syncPreview() {}
```

- [ ] Register both files in `cpp/CMakeLists.txt` SOURCES / HEADERS.

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

Expected: compiles cleanly. The page renders as an empty rectangle — Tasks 5-7 fill it.

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.h \
        cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp \
        cpp/CMakeLists.txt
git commit -m "Add Pcsx2GraphicsDisplayPage skeleton

Empty page class that filters the Graphics/Display schema slice and
exposes buildLeftCompoundCard / buildRightPreviewCard /
buildBottomToggleGrid / loadValues / syncPreview stubs for Tasks 5-7
to fill. Registered in CMake so the subsequent layout tasks stay
focused on UI code instead of build plumbing."
```

---

## Task 5: Build the left compound card (5 combos + Widescreen Patches toggle)

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp` (modify — fill `buildLeftCompoundCard`)

This card is a single `Pcsx2Card` whose internal `QVBoxLayout` holds five combo rows and one toggle row stacked vertically with 8 px spacing. The card's representative `SettingDef` is `AspectRatio` (so that Tab-ing onto the card frame itself shows the AR description by default). Each inner row also wires its own `focused(def)` up to the page's `settingFocused` signal so hovering or Tab-ing an individual row updates the description bar row-by-row.

**Important navigation note (repeated from the Architecture section):** inside this card, Qt's default focus chain handles Tab / Shift+Tab. The existing Pcsx2Card spatial-nav only runs when the Pcsx2Card itself has focus — once focus is inside a combo or toggle, arrow keys are consumed by Qt defaults (combo box arrow-change, toggle left/right). **Do not** try to hand-roll arrow navigation inside this compound card.

- [ ] Replace the `buildLeftCompoundCard` stub:

```cpp
void Pcsx2GraphicsDisplayPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    auto* card = new Pcsx2Card(this);

    // Use AspectRatio as the card's representative setting so focusing
    // the card frame itself shows the AR description in the dialog bar.
    if (const SettingDef* arDef = findDef("AspectRatio"))
        card->setSettingDef(*arDef);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);

    // Helper: build a combo row from a schema key, wire focus + value
    // signals, and return the row. Returns nullptr if the schema entry
    // is missing (graceful no-op).
    auto addCombo = [&](const QString& key) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& val) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return row;
    };

    // Five combo rows in spec order.
    addCombo("Renderer");
    m_aspectCombo = addCombo("AspectRatio");
    addCombo("FMVAspectRatioSwitch");
    addCombo("deinterlace_mode");
    addCombo("linear_present_mode");

    // Inline Widescreen Patches toggle — matches the Emulation page's
    // label-left / toggle-right row style.
    if (const SettingDef* d = findDef("EnableWideScreenPatches")) {
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this](bool on) {
            const SettingDef* dd = findDef("EnableWideScreenPatches");
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
    }

    // Live-update the preview when the Aspect Ratio combo changes.
    // Declared here so the signal is wired the moment the combo exists
    // — even though m_preview isn't constructed yet, the connection
    // fires on valueChanged which can't be emitted until loadValues()
    // completes in Task 7, and m_preview is built in Task 6 before
    // that.
    if (m_aspectCombo) {
        connect(m_aspectCombo, &Pcsx2ComboRow::valueChanged, this, [this](const QString& val) {
            if (m_preview) {
                m_preview->setAspectRatio(Pcsx2AspectRatioPreview::fromSchemaValue(val));
            }
        });
    }

    topRow->addWidget(card, 1);
}
```

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

Expected: compiles, the page shows a half-width compound card with 6 stacked rows on the left side of an otherwise-empty top row.

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp
git commit -m "Pcsx2GraphicsDisplayPage: build left compound card

Single Pcsx2Card containing five combos (Renderer, AspectRatio,
FMVAspectRatioSwitch, deinterlace_mode, linear_present_mode) and an
inline Widescreen Patches toggle. AspectRatio is registered as the
card's representative settingDef so focusing the frame shows its
description; every inner row also emits focused() up to the page so
Tab-ing through inner controls updates the description bar per row.
The AspectRatio combo's valueChanged is wired to the (yet-to-exist)
preview widget so Task 6's construction just needs to assign
m_preview."
```

---

## Task 6: Build the right preview card (preview widget + Vertical Stretch + Crop spinboxes)

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp` (modify — fill `buildRightPreviewCard`, extend `Pcsx2Card` with preview styling)
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.h` (modify — add `setPreviewStyle(bool)`)
- `cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.cpp` (modify)

The preview card needs a slightly darker background (`#504c48`) so it reads as a distinct "preview" panel next to the standard card colour (`#646058`). We add a `Pcsx2Card::setPreviewStyle(bool)` switch that swaps the card's stylesheet. This keeps focus handling (the 2 px amber halo in `paintEvent`) identical to the regular card, which is exactly what we want.

- [ ] Extend `pcsx2_card.h`:

```cpp
    void setPreviewStyle(bool preview);
```

Place the declaration directly after `setSettingDef(...)` in the public section.

- [ ] Extend `pcsx2_card.cpp` by appending the definition. Place it immediately after the constructor:

```cpp
void Pcsx2Card::setPreviewStyle(bool preview) {
    if (preview) {
        setStyleSheet(QStringLiteral(
            "QFrame#Pcsx2Card {"
            "  background-color: #504c48;"
            "  border: 1px solid #706c66;"
            "  border-radius: 8px;"
            "}"
            "QFrame#Pcsx2Card[focused=\"true\"] {"
            "  border: 1px solid #f59e0b;"
            "}"));
    } else {
        setStyleSheet(Pcsx2Theme::cardQss());
    }
    style()->unpolish(this);
    style()->polish(this);
    update();
}
```

The focus halo in `Pcsx2Card::paintEvent` already paints on top of the stylesheet background and continues to work unchanged — it reads `hasFocus()` and paints an amber 2 px stroke inset 1 px from the card's edge, regardless of background colour.

- [ ] Replace the `buildRightPreviewCard` stub:

```cpp
void Pcsx2GraphicsDisplayPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    auto* card = new Pcsx2Card(this);
    card->setPreviewStyle(true);
    // The preview card represents the StretchY setting when focused —
    // it's the most-edited preview-affecting value.
    if (const SettingDef* d = findDef("StretchY"))
        card->setSettingDef(*d);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    // Small uppercase section label ("ASPECT RATIO PREVIEW") in muted
    // colour — matches the spec mockup.
    auto* lbl = new QLabel(QStringLiteral("ASPECT RATIO PREVIEW"), card);
    lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;"
                       "letter-spacing:0.8px;");
    v->addWidget(lbl);

    // Preview widget.
    m_preview = new Pcsx2AspectRatioPreview(card);
    m_preview->setMinimumHeight(180);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    v->addWidget(m_preview, 1);

    // Vertical Stretch slider. Lives inline inside the preview card,
    // not its own sub-card, because it directly drives the preview.
    if (const SettingDef* d = findDef("StretchY")) {
        m_stretchSlider = new Pcsx2SliderRow(card);
        m_stretchSlider->setLabel(d->label);
        m_stretchSlider->setRange(int(d->minVal), int(d->maxVal));
        m_stretchSlider->setSuffix(d->suffix);
        m_stretchSlider->setSettingDef(*d);
        connect(m_stretchSlider, &Pcsx2SliderRow::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(m_stretchSlider, &Pcsx2SliderRow::valueChanged, this, [this](int val) {
            const SettingDef* dd = findDef("StretchY");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
            if (m_preview) m_preview->setStretchY(val);
        });
        v->addWidget(m_stretchSlider);
    }

    // Crop row: label + four compact spinboxes (L / T / R / B) with
    // "px" suffix. Each change pushes all four values into the preview
    // in one call so the preview sees a consistent state.
    auto* cropLabel = new QLabel(QStringLiteral("Crop"), card);
    cropLabel->setStyleSheet("color:#d0ccc4;font-size:12px;font-weight:500;");
    v->addWidget(cropLabel);

    auto* cropRow = new QHBoxLayout();
    cropRow->setSpacing(8);

    auto makeCropSpin = [&](const QString& key, const QString& axis) -> QSpinBox* {
        auto* w = new QWidget(card);
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);

        auto* axisLbl = new QLabel(axis, w);
        axisLbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;");
        h->addWidget(axisLbl);

        auto* spin = new QSpinBox(w);
        spin->setRange(0, 100);
        spin->setSuffix(QStringLiteral(" px"));
        spin->setStyleSheet(
            "QSpinBox {"
            "  background:#585450; color:#f2efe8;"
            "  border:1px solid #706c66; border-radius:4px;"
            "  padding:2px 4px; min-width:58px;"
            "}"
            "QSpinBox:focus { border-color:#f59e0b; }");
        h->addWidget(spin, 1);

        cropRow->addWidget(w, 1);

        // Focused behaviour — the spin box re-emits settingFocused
        // through the page so the description bar updates with the
        // right crop-axis tooltip.
        if (const SettingDef* d = findDef(key)) {
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [this, key](int val) {
                const SettingDef* dd = findDef(key);
                if (dd) saveValue(dd->section, dd->key, QString::number(val));
                if (m_preview && m_cropL && m_cropT && m_cropR && m_cropB) {
                    m_preview->setCrop(
                        m_cropL->value(), m_cropT->value(),
                        m_cropR->value(), m_cropB->value());
                }
            });
            // QSpinBox has no focused(SettingDef) signal — we install a
            // lightweight event filter on the page in a follow-up plan
            // if we need per-spin focus descriptions. For Plan 3 the
            // containing card's focused signal (set to StretchY) is
            // sufficient; the spec describes this as acceptable.
            Q_UNUSED(d);
        }
        return spin;
    };

    m_cropL = makeCropSpin("CropLeft",   QStringLiteral("L"));
    m_cropT = makeCropSpin("CropTop",    QStringLiteral("T"));
    m_cropR = makeCropSpin("CropRight",  QStringLiteral("R"));
    m_cropB = makeCropSpin("CropBottom", QStringLiteral("B"));

    v->addLayout(cropRow);

    topRow->addWidget(card, 1);
}
```

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

Expected: compiles, preview card fills the right half of the top row with a darker background and contains the live preview widget, the Vertical Stretch slider, and four L/T/R/B spinboxes. Values don't yet load (that's Task 7), but the page opens and paints correctly.

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp \
        cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.h \
        cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.cpp
git commit -m "Pcsx2GraphicsDisplayPage: build preview card + Pcsx2Card preview style

Right-hand Pcsx2Card using a new setPreviewStyle(true) variant with
background #504c48. Contains the uppercase 'ASPECT RATIO PREVIEW'
label, the live Pcsx2AspectRatioPreview widget (min height 180),
the Vertical Stretch slider, and a four-spinbox crop row with L / T /
R / B axis labels and 'px' suffix. Stretch and crop spinboxes push
their values into the preview widget on every change. The preview
style swap reuses the existing focus halo in Pcsx2Card::paintEvent
so keyboard affordances are identical between regular and preview
cards."
```

---

## Task 7: Build the bottom 3×2 toggle grid and wire load/save + initial preview sync

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp` (modify — fill `buildBottomToggleGrid`, `loadValues`, `syncPreview`)

Six toggle cards in a 3-col × 2-row grid under the top row. Each is a `Pcsx2Card` wrapping a `Pcsx2ToggleRow`, using the same inline `makeToggleCard` pattern from Plan 2's `Pcsx2GraphicsRenderingPage`. The `IntegerScaling` toggle additionally pushes its state into the preview widget on every change.

- [ ] Replace the `buildBottomToggleGrid`, `loadValues`, and `syncPreview` stubs:

```cpp
void Pcsx2GraphicsDisplayPage::buildBottomToggleGrid(QVBoxLayout* root) {
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        if (!d) return card;
        if (auto* def = findDef(key)) card->setSettingDef(*def);

        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsDisplayPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            // Preview-affecting toggle: push state into the preview.
            if (key == "IntegerScaling" && m_preview)
                m_preview->setIntegerScaling(on);
        });
        if (key == "IntegerScaling")
            m_integerScalingToggle = row;
        v->addWidget(row);
        return card;
    };

    auto* grid = new QGridLayout();
    grid->setSpacing(12);

    grid->addWidget(makeToggleCard("pcrtc_antiblur"),             0, 0);
    grid->addWidget(makeToggleCard("IntegerScaling"),             0, 1);
    grid->addWidget(makeToggleCard("EnableNoInterlacingPatches"), 0, 2);
    grid->addWidget(makeToggleCard("pcrtc_offsets"),              1, 0);
    grid->addWidget(makeToggleCard("disable_interlace_offset"),   1, 1);
    grid->addWidget(makeToggleCard("pcrtc_overscan"),             1, 2);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    root->addLayout(grid);
}

void Pcsx2GraphicsDisplayPage::loadValues() {
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

    // Four Crop spinboxes — loaded individually because they're raw QSpinBoxes
    // with no Pcsx2*Row wrapper.
    auto loadCrop = [&](QSpinBox* spin, const QString& key) {
        if (!spin) return;
        const SettingDef* d = findDef(key);
        if (!d) return;
        QString cur = app->settingValue(emuId, d->section, d->key);
        bool ok = false;
        int v = cur.toInt(&ok);
        if (!ok) v = d->defaultValue.toInt();
        spin->setValue(v);
    };
    loadCrop(m_cropL, "CropLeft");
    loadCrop(m_cropT, "CropTop");
    loadCrop(m_cropR, "CropRight");
    loadCrop(m_cropB, "CropBottom");
}

void Pcsx2GraphicsDisplayPage::syncPreview() {
    if (!m_preview) return;

    if (m_aspectCombo) {
        m_preview->setAspectRatio(
            Pcsx2AspectRatioPreview::fromSchemaValue(m_aspectCombo->value()));
    }
    if (m_stretchSlider) {
        m_preview->setStretchY(m_stretchSlider->value());
    }
    if (m_cropL && m_cropT && m_cropR && m_cropB) {
        m_preview->setCrop(m_cropL->value(), m_cropT->value(),
                           m_cropR->value(), m_cropB->value());
    }
    if (m_integerScalingToggle) {
        m_preview->setIntegerScaling(m_integerScalingToggle->isChecked());
    }
}
```

Note: `Pcsx2ComboRow` has both a `value()` accessor (returns the currently-selected option's value string) and a `setValue()` setter; Plans 1 and 2 both use this pattern. Similarly for `Pcsx2SliderRow::value()`/`setValue(int)` and `Pcsx2ToggleRow::isChecked()`/`setChecked(bool)`. If any signature differs, match the existing Plan 2 `Pcsx2GraphicsRenderingPage` helpers — that page compiles and ships, so its usage is canonical.

- [ ] Build:

```
cd cpp && cmake --build build 2>&1 | tail -10
```

Expected: compiles, the page now renders the full three-section layout (top row with compound + preview card, bottom 3×2 toggle grid), values load from `PCSX2.ini` on open, and the preview syncs on first paint.

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp
git commit -m "Pcsx2GraphicsDisplayPage: toggle grid + loadValues + preview sync

Six bottom toggle cards (Anti-Blur, Integer Scaling, No-Interlacing
Patches, Screen Offsets, Disable Interlace Offset, Show Overscan) in a
3x2 grid using the same inline makeToggleCard pattern as the Plan 2
Rendering page. IntegerScaling's toggled signal additionally pushes
state into the preview widget. loadValues() walks Pcsx2ComboRow /
ToggleRow / SliderRow children plus the four raw crop QSpinBoxes and
hydrates them from AppController::settingValue. syncPreview() is
called once after loadValues() so the preview reflects the persisted
state on first open."
```

---

## Task 8: Replace the stub and move default Graphics tab to Display (index 0)

**Files:**
- `cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp` (modify)

The Plan 2 `Pcsx2GraphicsPage` currently constructs a `Pcsx2GraphicsStubSubPage` at index 0 and deliberately lands on index 1 (Rendering) because Display was a stub. Plan 3 swaps the stub for `Pcsx2GraphicsDisplayPage` and changes the default landing index back to 0.

- [ ] Add the new include near the other page headers in `pcsx2_graphics_page.cpp`:

```cpp
#include "pcsx2_graphics_display_page.h"
```

- [ ] Replace the Display stub construction in `Pcsx2GraphicsPage::Pcsx2GraphicsPage`. Before (from Plan 2):

```cpp
    // 0: Display (stub — Plan 3 replaces)
    auto* displayStub = new Pcsx2GraphicsStubSubPage(app, emuId, "Display", this);
    m_stack->addWidget(displayStub);
```

After:

```cpp
    // 0: Display (real — Plan 3)
    auto* display = new Pcsx2GraphicsDisplayPage(m_dialog);
    connect(display, &Pcsx2GraphicsDisplayPage::settingFocused,
            this, &Pcsx2GraphicsPage::settingFocused);
    m_stack->addWidget(display);
```

The `AppController*` and `emuId` locals remain — the OSD stub at index 3 still uses them.

- [ ] Change the default landing index from 1 (Rendering) to 0 (Display). At the bottom of the constructor, replace:

```cpp
    // Start on Rendering so the user lands on the most commonly-edited
    // Graphics sub-page instead of the Display stub.
    m_tabBar->setCurrentIndex(1);
    m_stack->setCurrentIndex(1);
```

With:

```cpp
    // Plan 3: Display is now a real page, so land on it by default.
    m_tabBar->setCurrentIndex(0);
    m_stack->setCurrentIndex(0);
```

- [ ] Build + run the full test suite:

```
cd cpp && cmake --build build 2>&1 | tail -10
ctest --test-dir cpp/build --output-on-failure
```

Expected: 12/12 tests pass (11 from Plans 1-2 + the new `Pcsx2AspectRatioPreview` executable from Task 3).

- [ ] Commit:

```
git add cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp
git commit -m "Pcsx2GraphicsPage: route Display to Pcsx2GraphicsDisplayPage

Replaces the Plan 2 Pcsx2GraphicsStubSubPage at stack index 0 with the
real Pcsx2GraphicsDisplayPage and restores the default landing index
from 1 (Rendering) back to 0 (Display) now that Display is shipping.
OSD stays on the stub at index 3 until Plan 4."
```

---

## Task 9: Build + launch smoke test

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

Expected: 12/12 tests pass, including the new `Pcsx2AspectRatioPreview` executable and the extended `testGraphicsDisplaySettingsAllHaveRecommended` slot in `test_pcsx2_recommended_values`.

- [ ] Launch the app:

```
open ./build/RetroNest.app
```

- [ ] Manual verification checklist:

  - Open RetroNest → Emulators → PCSX2 → settings button. Category hub opens (Plan 1 layout unchanged).
  - Arrow-navigate to the Graphics card, press Enter. `Pcsx2GraphicsPage` opens. The sub-tab bar shows Display / Rendering / Post-Proc / OSD with the **Display** tab active (amber underline, lifted background). This is the Plan 3 default-tab change — if it still lands on Rendering, Task 8 wasn't fully applied.
  - The **Display** sub-page renders the two-column top row:
    - Left half: a single compound card with five combo rows (Renderer, Aspect Ratio, FMV Aspect Ratio Override, Deinterlacing, Bilinear Filtering) and an inline Widescreen Patches toggle at the bottom.
    - Right half: a darker preview card (`#504c48` background vs `#646058`) containing the uppercase "ASPECT RATIO PREVIEW" label, the live preview widget showing a 4 : 3 display rect inside pillarbox bars, a Vertical Stretch slider, and a row of four L/T/R/B crop spinboxes with "px" suffix.
  - Below the top row, a 3×2 grid of six toggle cards: Anti-Blur, Integer Scaling, No-Interlacing Patches, Screen Offsets, Disable Interlace Offset, Show Overscan.
  - Focus any card or inner row with Tab/arrow keys — the description bar at the bottom of the dialog updates with the setting's tooltip and an amber `Recommended: …` pill. Confirm the pill uses the new Task-1 recommended values (e.g. Aspect Ratio pill reads `Recommended: 4:3`, Vertical Stretch reads `Recommended: 100`).
  - **Preview — Aspect Ratio changes:** change the Aspect Ratio combo to 16:9 → the preview immediately fills edge-to-edge horizontally with thin top/bottom letterbox bars (or none at all depending on the exact window ratio). Change to 4:3 → visible left/right pillarbox bars reappear. Change to Stretch → the draw rect fills the entire widget. Change to 10:7 → the draw rect shows a slightly wider-than-4:3 box.
  - **Preview — Vertical Stretch:** drag the slider from 100 → 130. The preview's draw rect grows taller while keeping the same width; at 200 it reaches the top and bottom edges. Drag back to 100 — the draw rect returns to its letterboxed height.
  - **Preview — Crop:** change CropLeft from 0 → 40. The left edge of the preview draw rect shows a thin dark overlay where the cropped pixels used to be, and (after the draw-rect recomputation kicks in) the overall rect may shrink slightly as the cropped source aspect changes the ar fit. Do the same for Top / Right / Bottom.
  - **Preview — Integer Scaling:** toggle Integer Scaling on. The preview draw rect snaps to a smaller 1× box centred inside the widget (or stays at the same size if the client was already a whole multiple); toggling off restores the smooth fit.
  - **Persistence:** change Aspect Ratio to 16:9 and Vertical Stretch to 120, close the dialog, re-open it, drill back into Graphics → Display. Both values persist. Open `PCSX2.ini` on disk and confirm `[EmuCore/GS] AspectRatio = 16:9` and `StretchY = 120`.
  - **Description bar copy:** focus each of the six bottom-grid toggles in turn. Each one shows a distinct tooltip from Task 1 — Integer Scaling reads "Snaps the rendered image to an integer multiple…", Show Overscan reads "Shows the overscan area…", etc.
  - **Sub-tab navigation:** click Rendering → the Plan 2 Rendering page renders unchanged. Click Post-Proc → the Plan 2 Post-Processing page renders unchanged. Click OSD → the Plan 2 stub still shows ("OSD — Coming in a later update"). Back to Display → the Plan 3 page re-renders and preview state persists from before.
  - **Back button:** click Back on the Graphics page → returns to the category hub, description bar clears.
  - **No regressions:** drill into Emulation, Audio, Memory Cards one at a time — all three Plan 1 pages still work correctly (no regressions from Plan 3 `Pcsx2Card::setPreviewStyle` addition).
  - **Cross-emulator check:** close the dialog. Reopen DuckStation settings and PPSSPP settings — both still route through the legacy `EmulatorSettingsPage` (only `emuId == "pcsx2"` uses the new dialog).

- [ ] If everything above passes, there are no new code changes. If a bug surfaces, fix it in a new commit *before* moving on — do not defer to Plan 4.

- [ ] Final commit (only if verification-only fixes are needed; otherwise skip):

```
git commit -m "Plan 3 manual smoke test fixes"
```

---

## Plan 3 completion criteria

- [ ] Every `Graphics/Display` schema entry has a non-empty `recommendedValue` **and** a non-empty `tooltip`, verified by `testGraphicsDisplaySettingsAllHaveRecommended` asserting count ≥ 15 and both fields populated.
- [ ] `Pcsx2AspectRatioPreview` compiles, exposes the API described in the spec, and passes its unit test (4 slots). Draw math reproduces upstream `GSRenderer::CalculateDrawDstRect` + `CalculateDrawSrcRect` steps 1–9 exactly, with a commented cross-reference at the top of the `.cpp`.
- [ ] `Pcsx2GraphicsDisplayPage` renders the spec's three-section layout: top row with left compound card (5 combos + Widescreen Patches toggle) and right preview card (preview widget + Vertical Stretch slider + 4 crop spinboxes), bottom 3×2 toggle grid.
- [ ] The preview widget live-updates when the user changes Aspect Ratio, Vertical Stretch, any Crop axis, or Integer Scaling.
- [ ] `Pcsx2Card::setPreviewStyle(true)` switches to the darker `#504c48` background variant while keeping the amber focus halo intact.
- [ ] `Pcsx2GraphicsPage` opens with index 0 (Display) active, and drilling into Display shows the new real page instead of the Plan 2 stub.
- [ ] Description bar updates correctly on focus across every control in the Display page — combos, toggles, slider, compound-card frame. (Crop spinboxes fall back to the preview card's StretchY description; this is the documented Plan 3 limitation.)
- [ ] `ctest --test-dir cpp/build` passes 12/12 tests.
- [ ] No regressions to Emulation, Audio, Memory Cards (Plan 1), Rendering or Post-Processing (Plan 2), or to DuckStation/PPSSPP settings routing.

## Not in Plan 3 (handled in later plans)

- **Plan 4 — Graphics / OSD & Polish:** real OSD sub-page with all `OsdShow*` toggles and OSD scale/position controls, live OSD preview widget reproducing `ImGuiOverlays.cpp` layout, `recommendedValue` backfill for Graphics/OSD, final polish (controller input verification, description-bar copy audit, helper-lambda hoist cleanup).
- Per-crop-spinbox focused(SettingDef) wiring — Plan 3 deliberately falls back to the preview card's representative StretchY settingDef for crop spinboxes because QSpinBox has no native focused signal and installing an event filter is out of scope. Plan 4 can add this if desired.
- DuckStation and PPSSPP redesigns — separate spec / plan series.
- Any schema changes beyond `recommendedValue` + `tooltip` plumbing for Graphics/Display.
- Tick marks or snap labels on the Vertical Stretch slider — Plan 3 uses the stock `Pcsx2SliderRow` widget unchanged.
- A real FMV preview for the FMV Aspect Ratio Override combo — Plan 3's preview always shows the main AspectRatio result, not the FMV-override variant.

### Critical Files for Implementation

- /Users/mark/Documents/RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/src/ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_display_page.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/src/ui/settings/pcsx2/pages/pcsx2_graphics_page.cpp
- /Users/mark/Documents/RetroNest-Project/cpp/src/ui/settings/pcsx2/widgets/pcsx2_card.cpp