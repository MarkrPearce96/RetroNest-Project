#include "libretro_video_item.h"
#include <QPainter>
#include <cmath>
#include <algorithm>

LibretroVideoItem::LibretroVideoItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setFlag(ItemHasContents);
}

void LibretroVideoItem::setFrame(const QImage& frame) {
    {
        QMutexLocker l(&m_mutex);
        m_currentFrame = frame;
    }
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void LibretroVideoItem::setAspectMode(const QString& mode) {
    if (m_aspectMode == mode) return;
    m_aspectMode = mode;
    emit aspectModeChanged();
    update();
}

void LibretroVideoItem::setIntegerScale(bool on) {
    if (m_integerScale == on) return;
    m_integerScale = on;
    emit integerScaleChanged();
    update();
}

void LibretroVideoItem::paint(QPainter* p) {
    QImage frame;
    {
        QMutexLocker l(&m_mutex);
        frame = m_currentFrame;
    }

    const QRectF bounds = boundingRect();
    p->fillRect(bounds, Qt::black);

    if (frame.isNull())
        return;

    const double bw = bounds.width();
    const double bh = bounds.height();
    const double fw = frame.width();
    const double fh = frame.height();

    if (fw < 1.0 || fh < 1.0)
        return;

    // ─── Compute target width / height ────────────────────────────────────
    // For "stretch", just fill the bounds.
    // For all other modes, compute a (tw, th) that preserves some aspect
    // relationship, then fit it inside bounds with KeepAspectRatio semantics.
    double tw, th;

    if (m_aspectMode == QStringLiteral("stretch")) {
        tw = bw;
        th = bh;
    } else {
        // Determine the target display aspect ratio.
        double targetAR;
        if (m_aspectMode == QStringLiteral("4_3")) {
            targetAR = 4.0 / 3.0;
        } else if (m_aspectMode == QStringLiteral("16_9")) {
            targetAR = 16.0 / 9.0;
        } else {
            // "native" and "square": use the frame's own pixel dimensions.
            // For libretro cores that report correct base_width/base_height
            // (mGBA does), this is a square-pixel source, making "square"
            // equivalent to "native".
            targetAR = fw / fh;
        }

        // Fit a rect with targetAR inside bounds (KeepAspectRatio logic).
        const double boundsAR = bw / bh;
        if (targetAR > boundsAR) {
            // Wider than bounds — limited by width (letterbox).
            tw = bw;
            th = bw / targetAR;
        } else {
            // Taller than bounds — limited by height (pillarbox).
            tw = bh * targetAR;
            th = bh;
        }
    }

    // ─── Integer scale snap ────────────────────────────────────────────────
    // When integerScale is true, snap tw/th down to the largest integer
    // multiple of the source frame size that still fits inside bounds.
    // Skipped for "stretch" (integer scaling on a stretched frame is
    // meaningless — you'd just get the bounds anyway).
    if (m_integerScale && m_aspectMode != QStringLiteral("stretch")) {
        const double multW = std::floor(tw / fw);
        const double multH = std::floor(th / fh);
        double mult = std::min(multW, multH);
        if (mult < 1.0) mult = 1.0;
        tw = fw * mult;
        th = fh * mult;
        // Safety clamp: at very small item sizes the integer multiple can
        // overshoot; clamp to bounds and fall back to the un-snapped size.
        if (tw > bw || th > bh) {
            tw = std::min(tw, bw);
            th = std::min(th, bh);
        }
    }

    // ─── Center and draw ───────────────────────────────────────────────────
    const QRectF target(
        bounds.left() + (bw - tw) * 0.5,
        bounds.top()  + (bh - th) * 0.5,
        tw, th);

    p->setRenderHint(QPainter::SmoothPixmapTransform, false);  // pixel-art
    p->drawImage(target, frame);
}
