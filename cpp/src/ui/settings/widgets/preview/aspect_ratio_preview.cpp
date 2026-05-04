#include "ui/settings/widgets/preview/aspect_ratio_preview.h"
#include "ui/settings/settings_dialog_theme.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <cmath>
#include <algorithm>

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
// ─────────────────────────────────────────────────────────────────────

namespace {
constexpr double kSrcW = 640.0;
constexpr double kSrcH = 448.0;
}

AspectRatioPreview::AspectRatioPreview(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    setMinimumSize(minimumSizeHint());
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void AspectRatioPreview::setAspectRatio(AspectRatio ratio) {
    if (m_ratio == ratio) return;
    m_ratio = ratio;
    update();
}

void AspectRatioPreview::setStretchY(int percent) {
    if (percent < 10)  percent = 10;
    if (percent > 300) percent = 300;
    if (m_stretchY == percent) return;
    m_stretchY = percent;
    update();
}

void AspectRatioPreview::setCrop(int left, int top, int right, int bottom) {
    m_cropL = qBound(0, left,   100);
    m_cropT = qBound(0, top,    100);
    m_cropR = qBound(0, right,  100);
    m_cropB = qBound(0, bottom, 100);
    update();
}

void AspectRatioPreview::setIntegerScaling(bool on) {
    if (m_integerScaling == on) return;
    m_integerScaling = on;
    update();
}

AspectRatioPreview::AspectRatio
AspectRatioPreview::fromSchemaValue(const QString& v) {
    if (v == "Stretch")        return AspectRatio::Stretch;
    if (v == "Auto 4:3/3:2")   return AspectRatio::Auto4_3_3_2;
    if (v == "4:3")            return AspectRatio::R4_3;
    if (v == "16:9")           return AspectRatio::R16_9;
    if (v == "10:7")           return AspectRatio::R10_7;
    return AspectRatio::R4_3;
}

QString AspectRatioPreview::labelForCurrentRatio() const {
    switch (m_ratio) {
        case AspectRatio::Stretch:     return QStringLiteral("Stretch");
        case AspectRatio::Auto4_3_3_2: return QStringLiteral("Auto 4 : 3");
        case AspectRatio::R4_3:        return QStringLiteral("4 : 3");
        case AspectRatio::R16_9:       return QStringLiteral("16 : 9");
        case AspectRatio::R10_7:       return QStringLiteral("10 : 7");
    }
    return {};
}

QRectF AspectRatioPreview::computeDrawRect(const QRectF& client) const {
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

void AspectRatioPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Background — letterbox/pillarbox colour everywhere. Any portion
    // not covered by the draw rect below stays this colour.
    const QRectF full = rect();
    p.fillRect(full, SettingsDialogTheme::letterbox());

    // No inset — the widget IS the preview screen. When the user picks
    // Stretch, the draw rect fills the entire widget edge-to-edge with
    // no visible bezel.
    const QRectF client = full;

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
        p.setBrush(SettingsDialogTheme::letterbox());
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
        QPen pen(SettingsDialogTheme::accent(), 1.0);
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
        p.setPen(SettingsDialogTheme::textPrimary());
        p.drawText(dst, Qt::AlignCenter, labelForCurrentRatio());
    }
}

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
    // Intentionally no update() — m_fmvRatio is held for API/Q_PROPERTY
    // round-trip but is not yet consumed by paintEvent. Add update() here
    // when paintEvent starts using m_fmvRatio.
}

QString AspectRatioPreview::fmvAspectModeString() const {
    switch (m_fmvRatio) {
        case AspectRatio::Stretch:     return "Stretch";
        case AspectRatio::Auto4_3_3_2: return "Auto 4:3/3:2";
        case AspectRatio::R4_3:        return "4:3";
        case AspectRatio::R16_9:       return "16:9";
        case AspectRatio::R10_7:       return "10:7";
    }
    return "4:3";
}
