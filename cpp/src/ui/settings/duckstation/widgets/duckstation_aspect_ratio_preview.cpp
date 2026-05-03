#include "duckstation_aspect_ratio_preview.h"
#include "ui/settings/settings_dialog_theme.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────
// Draw-rect math — simplified version of PCSX2's GSRenderer pipeline,
// adapted for DuckStation's aspect-ratio options.
//
// The pipeline is:
//   1. Start from the widget client rect.
//   2. Apply optional crop values against a fixed 640x448 source.
//   3. Compute target_ar from the AspectRatio enum.
//   4. Compare target_ar to client_ar to determine pillarbox / letterbox.
//   5. Center the resulting rect inside the client.
// ─────────────────────────────────────────────────────────────────────

namespace {
constexpr double kSrcW = 640.0;
constexpr double kSrcH = 448.0;
}

DuckStationAspectRatioPreview::DuckStationAspectRatioPreview(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    setMinimumSize(minimumSizeHint());
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void DuckStationAspectRatioPreview::setAspectRatio(AspectRatio ratio) {
    if (m_ratio == ratio) return;
    m_ratio = ratio;
    update();
}

void DuckStationAspectRatioPreview::setCrop(int left, int top, int right, int bottom) {
    m_cropL = qBound(0, left,   100);
    m_cropT = qBound(0, top,    100);
    m_cropR = qBound(0, right,  100);
    m_cropB = qBound(0, bottom, 100);
    update();
}

float DuckStationAspectRatioPreview::ratioFloat() const {
    switch (m_ratio) {
        case AspectRatio::R4x3:    return 4.0f / 3.0f;
        case AspectRatio::R16x9:   return 16.0f / 9.0f;
        case AspectRatio::R19x9:   return 19.0f / 9.0f;
        case AspectRatio::R20x9:   return 20.0f / 9.0f;
        case AspectRatio::R21x9:   return 21.0f / 9.0f;
        case AspectRatio::R16x10:  return 16.0f / 10.0f;
        case AspectRatio::Par1x1:  return 4.0f / 3.0f;
        case AspectRatio::Stretch: return 16.0f / 9.0f;
        case AspectRatio::Auto:    return 4.0f / 3.0f;
    }
    return 4.0f / 3.0f;
}

DuckStationAspectRatioPreview::AspectRatio
DuckStationAspectRatioPreview::fromSchemaValue(const QString& value) {
    if (value == "4:3")             return AspectRatio::R4x3;
    if (value == "16:9")            return AspectRatio::R16x9;
    if (value == "19:9")            return AspectRatio::R19x9;
    if (value == "20:9")            return AspectRatio::R20x9;
    if (value == "21:9")            return AspectRatio::R21x9;
    if (value == "16:10")           return AspectRatio::R16x10;
    if (value == "PAR 1:1")         return AspectRatio::Par1x1;
    if (value == "Stretch To Fill") return AspectRatio::Stretch;
    return AspectRatio::Auto;
}

QString DuckStationAspectRatioPreview::labelForCurrentRatio() const {
    switch (m_ratio) {
        case AspectRatio::Auto:    return QStringLiteral("Auto");
        case AspectRatio::Stretch: return QStringLiteral("Stretch");
        case AspectRatio::R4x3:    return QStringLiteral("4 : 3");
        case AspectRatio::R16x9:   return QStringLiteral("16 : 9");
        case AspectRatio::R19x9:   return QStringLiteral("19 : 9");
        case AspectRatio::R20x9:   return QStringLiteral("20 : 9");
        case AspectRatio::R21x9:   return QStringLiteral("21 : 9");
        case AspectRatio::R16x10:  return QStringLiteral("16 : 10");
        case AspectRatio::Par1x1:  return QStringLiteral("PAR 1 : 1");
    }
    return {};
}

QRectF DuckStationAspectRatioPreview::computeDrawRect(const QRectF& client) const {
    if (client.width() <= 1.0 || client.height() <= 1.0)
        return client;

    // Apply crop against the fixed 640x448 source.
    const double crLeft   = static_cast<double>(m_cropL);
    const double crTop    = static_cast<double>(m_cropT);
    const double crRight  = static_cast<double>(m_cropR);
    const double crBottom = static_cast<double>(m_cropB);

    double crop_w = kSrcW - crLeft - crRight;
    double crop_h = kSrcH - crTop  - crBottom;
    if (crop_w < 1.0) crop_w = 1.0;
    if (crop_h < 1.0) crop_h = 1.0;

    const double src_rect_ar = crop_w / crop_h;
    const double src_size_ar = kSrcW / kSrcH;
    const double crop_adjust = src_rect_ar / src_size_ar;

    const double client_ar = client.width() / client.height();
    double target_ar = 4.0 / 3.0;
    switch (m_ratio) {
        case AspectRatio::Auto:    target_ar = 4.0 / 3.0;    break;
        case AspectRatio::Stretch: target_ar = client_ar;    break;
        case AspectRatio::R4x3:    target_ar = 4.0 / 3.0;    break;
        case AspectRatio::R16x9:   target_ar = 16.0 / 9.0;   break;
        case AspectRatio::R19x9:   target_ar = 19.0 / 9.0;   break;
        case AspectRatio::R20x9:   target_ar = 20.0 / 9.0;   break;
        case AspectRatio::R21x9:   target_ar = 21.0 / 9.0;   break;
        case AspectRatio::R16x10:  target_ar = 16.0 / 10.0;  break;
        case AspectRatio::Par1x1:  target_ar = 4.0 / 3.0;    break;
    }

    const double arr = (target_ar * crop_adjust) / client_ar;
    double w, h;
    if (arr < 1.0) {
        w = client.width() * arr;
        h = client.height();
    } else {
        w = client.width();
        h = client.height() / arr;
    }

    // Clamp to client bounds.
    if (w > client.width())  w = client.width();
    if (h > client.height()) h = client.height();

    // Center.
    const double x = client.left() + (client.width()  - w) * 0.5;
    const double y = client.top()  + (client.height() - h) * 0.5;
    return QRectF(x, y, w, h);
}

void DuckStationAspectRatioPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF full = rect();
    p.fillRect(full, SettingsDialogTheme::letterbox());

    const QRectF client = full;
    const QRectF dst = computeDrawRect(client);

    // Warm-ground → cool-sky gradient as the stand-in "game scene".
    QLinearGradient grad(dst.topLeft(), dst.bottomLeft());
    grad.setColorAt(0.0, QColor("#6c8b9c"));   // sky
    grad.setColorAt(0.55, QColor("#b7a589"));  // horizon
    grad.setColorAt(1.0, QColor("#6b513a"));   // ground
    p.fillRect(dst, grad);

    // Show cropped regions as dark overlays inside the dst rect.
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
