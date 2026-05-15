// SPDX-License-Identifier: GPL-3.0+

#include "libretro_metal_item.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>
#include <QDebug>
#include <cmath>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

LibretroMetalItem::LibretroMetalItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, false);

    // Create a child QWindow that will own a layer-backed NSView.
    // setSurfaceType(MetalSurface) makes Qt request a NSView suitable
    // for CAMetalLayer attachment.
    m_window = new QWindow();
    m_window->setSurfaceType(QSurface::MetalSurface);
    m_window->setFlags(Qt::Widget);
    m_window->create(); // realizes the NSView immediately

    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(m_window->winId());
    if (view) {
        [view setWantsLayer:YES];
        // We do NOT install a CAMetalLayer here. PCSX2's GSDeviceMTL
        // creates and installs its own CAMetalLayer on this NSView via
        // AttachSurfaceOnMainThread(). Leaving the layer empty lets that
        // installation succeed.
    } else {
        qWarning() << "[LibretroMetalItem] winId() returned null — NSView not realized";
    }
}

LibretroMetalItem::~LibretroMetalItem()
{
    // Defer the QWindow destruction. The EmulationView pops during
    // mainStack.pop()'s transition animation; the destructor can fire
    // while macOS is still compositing this NSView. A direct delete
    // crashes the entire app; deleteLater queues destruction to the
    // next event-loop iteration, by which point compositing is done
    // and PCSX2's CAMetalLayer has fully detached.
    // GameSession::registerHardwareView(0) is called from QML's
    // Component.onDestruction before this dtor runs.
    if (m_window) {
        m_window->deleteLater();
        m_window = nullptr;
    }
}

qulonglong LibretroMetalItem::nativeView() const
{
    if (!m_window) return 0;
    return static_cast<qulonglong>(m_window->winId());
}

void LibretroMetalItem::setAspectMode(const QString& mode)
{
    if (m_aspectMode == mode) return;
    m_aspectMode = mode;
    emit aspectModeChanged();
    updateInnerGeometry();
}

void LibretroMetalItem::setNativeAspect(qreal a)
{
    if (qFuzzyCompare(m_nativeAspect, a) || a <= 0.0) return;
    m_nativeAspect = a;
    emit nativeAspectChanged();
    updateInnerGeometry();
}

void LibretroMetalItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickItem::itemChange(change, value);

    // When the item is added to a window, parent our child QWindow to it
    // so geometry tracking works. Without this, the underlying NSView is
    // a free-floating window not visually inside RetroNest.
    if (change == QQuickItem::ItemSceneChange && value.window) {
        if (m_window) {
            m_window->setParent(value.window);
            updateInnerGeometry();
            syncContentsScale();
            m_window->show();
        }
        // Track DPR changes (screen change, dock/undock, etc.) so the
        // CAMetalLayer's contentsScale stays in sync with the host
        // window's backingScaleFactor. Without this, PCSX2 renders at
        // logical-pixel resolution and macOS upscales the result via
        // bilinear, visibly blurring every frame.
        if (value.window) {
            connect(value.window, &QWindow::screenChanged,
                    this, &LibretroMetalItem::syncContentsScale);
        }
    }
}

void LibretroMetalItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateInnerGeometry();
    syncContentsScale();
}

void LibretroMetalItem::updateInnerGeometry()
{
    if (!m_window || !window()) return;
    const QRectF bounds = mapRectToScene(boundingRect());

    const double bw = bounds.width();
    const double bh = bounds.height();
    if (bw < 1.0 || bh < 1.0) {
        m_window->setGeometry(bounds.toRect());
        return;
    }

    // Stretch mode = fill the whole item rect, no letterbox.
    if (m_aspectMode == QStringLiteral("stretch")) {
        m_window->setGeometry(bounds.toRect());
        return;
    }

    // Target aspect: explicit override modes win; "native" falls back to
    // m_nativeAspect (libretro av_info.geometry.aspect_ratio — 4/3 for PS2).
    double targetAR;
    if (m_aspectMode == QStringLiteral("4_3")) {
        targetAR = 4.0 / 3.0;
    } else if (m_aspectMode == QStringLiteral("16_9")) {
        targetAR = 16.0 / 9.0;
    } else {
        targetAR = (m_nativeAspect > 0.0) ? m_nativeAspect : (4.0 / 3.0);
    }

    // Fit targetAR inside bounds with KeepAspectRatio semantics.
    const double boundsAR = bw / bh;
    double tw, th;
    if (targetAR > boundsAR) {
        tw = bw;
        th = bw / targetAR;
    } else {
        tw = bh * targetAR;
        th = bh;
    }

    const int x = static_cast<int>(std::floor(bounds.x() + (bw - tw) / 2.0));
    const int y = static_cast<int>(std::floor(bounds.y() + (bh - th) / 2.0));
    const int w = static_cast<int>(std::floor(tw));
    const int h = static_cast<int>(std::floor(th));
    m_window->setGeometry(QRect(x, y, w, h));
}

void LibretroMetalItem::syncContentsScale()
{
    if (!m_window) return;
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(m_window->winId());
    if (!view) return;

    // Use the host QQuickWindow's DPR when present; fall back to the
    // primary screen when the item isn't parented to a window yet (the
    // child QWindow has its own screen but Qt's QWindow::devicePixelRatio
    // returns 1.0 until it's actually shown on that screen).
    qreal scale = 1.0;
    if (window()) {
        scale = window()->devicePixelRatio();
    } else if (auto* screen = QGuiApplication::primaryScreen()) {
        scale = screen->devicePixelRatio();
    }
    if (scale < 1.0) scale = 1.0;

    if (view.layer) {
        view.layer.contentsScale = scale;
        // PCSX2's GSDeviceMTL attaches its CAMetalLayer as a sublayer of
        // the NSView's backing layer (or replaces it). Propagate
        // contentsScale to every sublayer so any layer PCSX2 installs
        // also renders at backing-scale resolution.
        for (CALayer* sub in view.layer.sublayers) {
            sub.contentsScale = scale;
            // CAMetalLayer specifically needs drawableSize bumped too,
            // otherwise its texture stays logical-pixel sized. Update
            // it from the layer's bounds * contentsScale.
            if ([sub isKindOfClass:[CAMetalLayer class]]) {
                CAMetalLayer* metalLayer = (CAMetalLayer*)sub;
                const CGSize bounds = metalLayer.bounds.size;
                metalLayer.drawableSize = CGSizeMake(bounds.width * scale,
                                                     bounds.height * scale);
            }
        }
    }
}
