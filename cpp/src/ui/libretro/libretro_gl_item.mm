// SPDX-License-Identifier: GPL-3.0+
#include "libretro_gl_item.h"
#include "core/libretro/video_hardware_gl.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <cmath>

#import <Metal/Metal.h>
#import <IOSurface/IOSurfaceRef.h>

// Cached MTLTexture import of the IOSurface, plus dimensions used to
// detect resize. PImpl keeps Metal/ObjC types out of the header.
struct LibretroGLItem::Impl {
    id<MTLTexture> mtlTexture = nil;
    void*          cachedSurface = nullptr;
    int            cachedWidth = 0;
    int            cachedHeight = 0;
};

LibretroGLItem::LibretroGLItem(QQuickItem* parent)
    : QQuickItem(parent), m_impl(std::make_unique<Impl>()) {
    setFlag(ItemHasContents);
}

LibretroGLItem::~LibretroGLItem() = default;

void LibretroGLItem::setVideoHardware(QObject* hw) {
    if (m_hw) {
        disconnect(m_hw, nullptr, this, nullptr);
        m_hw = nullptr;
    }
    auto* video = qobject_cast<VideoHardwareGL*>(hw);
    if (!video) {
        // null or wrong type — reset and let the next setSource succeed.
        m_impl->mtlTexture = nil;
        m_impl->cachedSurface = nullptr;
        update();
        return;
    }
    m_hw = video;
    connect(m_hw, &VideoHardwareGL::frameReady,
            this, &LibretroGLItem::onFrameReady, Qt::QueuedConnection);
    qInfo("[LibretroGLItem] connected to VideoHardwareGL=%p", (void*)m_hw);
    update();
}

void LibretroGLItem::onFrameReady(int width, int height) {
    Q_UNUSED(width);
    Q_UNUSED(height);
    update();
}

void LibretroGLItem::fenceForShutdown() {
    // Render-seam shutdown fence (review P10 — moved here from
    // GameSession so core/ owns no Qt Quick). Only meaningful for the GL
    // backend's IOSurface→MTLTexture coupling; GameSession only calls it
    // on that path.
    QQuickWindow* w = window();
    if (!w) {
        qWarning() << "[LibretroGLItem] fenceForShutdown: no window, "
                      "skipping fence (degraded — same risk as before the fix)";
        return;
    }

    // Drop this item's strong ARC ref to the MTLTexture and disconnect
    // its VideoHardwareGL signals. After the next sync, updatePaintNode
    // sees m_hw == nullptr and returns nullptr, deleting the
    // QSGSimpleTextureNode and its owned QSGTexture — releasing the
    // QSGMetalTexture wrapper's last strong ARC ref to the MTLTexture.
    setVideoHardware(nullptr);
    update();

    // Wait two render passes. Frame 1 covers the sync that processes the
    // cleared updatePaintNode and deletes the node + texture. Frame 2
    // covers any GPU command buffer that captured the MTLTexture before
    // the clear and is still draining on the GPU.
    QEventLoop loop;
    int framesSeen = 0;
    auto conn = QObject::connect(
        w, &QQuickWindow::afterRendering, &loop,
        [&framesSeen, &loop]() {
            if (++framesSeen >= 2) loop.quit();
        },
        Qt::QueuedConnection);   // afterRendering fires on QSGRenderThread

    // Hard cap — covers degenerate cases (window hidden, rendering paused,
    // app already quitting). At worst we're at the same risk as before the
    // fix; the cap doesn't make anything worse.
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();
    // Explicit disconnect before scope exit is load-bearing: the queued
    // lambda captures &loop and &framesSeen by reference. Disconnecting
    // here ensures no late delivery arrives after the locals are invalid.
    QObject::disconnect(conn);

    qInfo() << "[LibretroGLItem] fenceForShutdown drained" << framesSeen
            << "frame(s) before stop";
}

void LibretroGLItem::setAspectMode(const QString& mode) {
    if (m_aspectMode == mode) return;
    m_aspectMode = mode;
    emit aspectModeChanged();
    update();
}

void LibretroGLItem::setNativeAspect(qreal a) {
    if (qFuzzyCompare(m_nativeAspect, a)) return;
    m_nativeAspect = a;
    emit nativeAspectChanged();
    update();
}

void LibretroGLItem::setIntegerScale(bool on) {
    if (m_integerScale == on) return;
    m_integerScale = on;
    emit integerScaleChanged();
    update();
}

// Same letterbox / pillarbox math as LibretroVideoItem.cpp:32 — kept as
// a free function so the QML item can re-use it without coupling to
// QImage. Returns the target rect within `bounds` for a source of
// `srcW × srcH`, respecting aspectMode / nativeAspect / integerScale.
static QRectF computeTargetRect(const QRectF& bounds,
                                 int srcW, int srcH,
                                 const QString& aspectMode,
                                 qreal nativeAspect,
                                 bool integerScale) {
    const double bw = bounds.width();
    const double bh = bounds.height();
    if (srcW < 1 || srcH < 1 || bw < 1.0 || bh < 1.0) return bounds;

    double tw, th;
    if (aspectMode == QStringLiteral("stretch")) {
        tw = bw;
        th = bh;
    } else {
        double targetAR;
        if (aspectMode == QStringLiteral("4_3"))      targetAR = 4.0 / 3.0;
        else if (aspectMode == QStringLiteral("16_9")) targetAR = 16.0 / 9.0;
        else if (aspectMode == QStringLiteral("square")) targetAR = double(srcW) / double(srcH);
        else /* native */                              targetAR = nativeAspect > 0 ? nativeAspect : double(srcW) / double(srcH);

        const double boundsAR = bw / bh;
        if (targetAR > boundsAR) { tw = bw;          th = bw / targetAR; }
        else                      { tw = bh * targetAR; th = bh; }
    }

    if (integerScale && aspectMode != QStringLiteral("stretch")) {
        double multW = std::floor(tw / double(srcW));
        double multH = std::floor(th / double(srcH));
        if (multW < 1.0) multW = 1.0;
        if (multH < 1.0) multH = 1.0;
        tw = double(srcW) * multW;
        th = double(srcH) * multH;
        if (tw > bw) tw = bw;
        if (th > bh) th = bh;
    }

    return QRectF(bounds.left() + (bw - tw) * 0.5,
                  bounds.top()  + (bh - th) * 0.5,
                  tw, th);
}

QSGNode* LibretroGLItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    // Bail until VideoHardwareGL is wired up + has an IOSurface.
    if (!m_hw || !window()) {
        delete oldNode;
        return nullptr;
    }
    void* surfacePtr = m_hw->currentIOSurface();
    if (!surfacePtr) {
        delete oldNode;
        return nullptr;
    }

    IOSurfaceRef ioSurface = reinterpret_cast<IOSurfaceRef>(surfacePtr);
    const int w = m_hw->fboWidth();
    const int h = m_hw->fboHeight();
    if (w < 1 || h < 1) { delete oldNode; return nullptr; }

    // (Re)import the IOSurface as a Metal texture if the underlying
    // surface changed (allocateFbo creates a fresh IOSurface on resize)
    // or dimensions changed. The MTLTexture stays valid as long as the
    // IOSurface does; ARC keeps our reference alive between paints.
    if (m_impl->mtlTexture == nil ||
        m_impl->cachedSurface != surfacePtr ||
        m_impl->cachedWidth   != w ||
        m_impl->cachedHeight  != h) {
        QSGRendererInterface* ri = window()->rendererInterface();
        if (!ri) { delete oldNode; return nullptr; }
        if (ri->graphicsApi() != QSGRendererInterface::MetalRhi) {
            qWarning("[LibretroGLItem] Qt scene graph RHI is not Metal "
                     "(graphicsApi=%d) — IOSurface→MTLTexture import only "
                     "works on the Metal backend",
                     static_cast<int>(ri->graphicsApi()));
            delete oldNode;
            return nullptr;
        }
        // QSGRendererInterface::DeviceResource returns the native device
        // handle directly as void* — for Metal that's an id<MTLDevice>
        // (an ObjC object). __bridge casts it without changing the retain
        // count; Qt's RHI owns the lifetime, we just observe.
        void* deviceRaw = ri->getResource(window(), QSGRendererInterface::DeviceResource);
        id<MTLDevice> device = (__bridge id<MTLDevice>)deviceRaw;
        if (!device) {
            qWarning("[LibretroGLItem] Qt scene graph DeviceResource was nil");
            delete oldNode;
            return nullptr;
        }

        MTLTextureDescriptor* desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                width:w
                                                               height:h
                                                            mipmapped:NO];
        desc.usage       = MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModeShared;

        m_impl->mtlTexture = [device newTextureWithDescriptor:desc
                                                     iosurface:ioSurface
                                                         plane:0];
        if (m_impl->mtlTexture == nil) {
            qWarning("[LibretroGLItem] newTextureWithDescriptor:iosurface: returned nil");
            delete oldNode;
            return nullptr;
        }
        m_impl->cachedSurface = surfacePtr;
        m_impl->cachedWidth   = w;
        m_impl->cachedHeight  = h;
        qInfo("[LibretroGLItem] imported IOSurface %p as MTLTexture (%dx%d)",
              surfacePtr, w, h);
    }

    // Wrap the MTLTexture as a QSGTexture so the scene graph can draw it.
    // Qt 6.7+ uses createTextureFromRhiTexture for advanced cases; we use
    // QQuickWindow::createTextureFromNativeObject which accepts an opaque
    // MTL texture handle. The returned QSGTexture is owned by us — we set
    // it on the simple-texture node and tell it to own/delete on next swap.
    QSGTexture* qsgTex = QNativeInterface::QSGMetalTexture::fromNative(
        m_impl->mtlTexture, window(), QSize(w, h));
    if (!qsgTex) {
        qWarning("[LibretroGLItem] QSGMetalTexture::fromNative returned null");
        delete oldNode;
        return nullptr;
    }

    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
    }
    node->setTexture(qsgTex);
    const QRectF target = computeTargetRect(boundingRect(), w, h,
                                             m_aspectMode, m_nativeAspect, m_integerScale);
    node->setRect(target);
    // PPSSPP renders bottom-left-origin (`bottom_left_origin = 1` in its
    // SET_HW_RENDER request); composite must flip vertically.
    node->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
    node->setFiltering(QSGTexture::Linear);

    // Diagnostic — log every N frames (not every frame; QtRenderThread is hot).
    static int s_logCounter = 0;
    if ((s_logCounter++ % 120) == 0) {
        const QRectF b = boundingRect();
        qInfo("[LibretroGLItem] paint #%d  bounds=%.0fx%.0f  target=%.0fx%.0f@(%.0f,%.0f)  "
              "src=%dx%d  aspectMode=%s  nativeAspect=%.3f  qsgTex=%p  mtl=%p",
              s_logCounter, b.width(), b.height(),
              target.width(), target.height(), target.x(), target.y(),
              w, h, qPrintable(m_aspectMode), m_nativeAspect,
              (void*)qsgTex, (void*)m_impl->mtlTexture);
    }
    return node;
}
