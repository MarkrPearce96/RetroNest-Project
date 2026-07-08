// SPDX-License-Identifier: GPL-3.0+
// ppsspp-libretro / RetroNest — OpenGL-backed video item.
//
// QQuickItem that imports the IOSurface owned by VideoHardwareGL as a
// Metal texture and composites it into Qt's scene graph. This is the
// HW-render counterpart for cores that request RETRO_HW_CONTEXT_OPENGL_CORE
// (PPSSPP, future Dolphin / Citra). EmulationView.qml hosts this when
// the adapter declares hardwareRenderBackend() == GL.
//
// Different from LibretroMetalItem (PCSX2's path):
//   - PCSX2: QQuickItem owns NSView, core draws Metal into it directly.
//     The Qt scene graph never sees the pixel data; the NSView sits on
//     top of the QML surface.
//   - PPSSPP/GL: VideoHardwareGL owns the IOSurface. We import it as a
//     Metal texture and draw it as a scene-graph texture node. Pixels
//     flow through Qt's renderer, composited like any other QML item.
//
// Implementation lives in libretro_gl_item.mm — Metal types + IOSurface
// import APIs are Objective-C++.

#pragma once

#include "core/libretro/libretro_render_surface.h"

#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>
#include <memory>

class VideoHardwareGL;

class LibretroGLItem : public QQuickItem, public LibretroRenderSurface {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString aspectMode READ aspectMode WRITE setAspectMode
                   NOTIFY aspectModeChanged)
    Q_PROPERTY(qreal nativeAspect READ nativeAspect WRITE setNativeAspect
                   NOTIFY nativeAspectChanged)
    Q_PROPERTY(bool integerScale READ integerScale WRITE setIntegerScale
                   NOTIFY integerScaleChanged)
public:
    explicit LibretroGLItem(QQuickItem* parent = nullptr);
    ~LibretroGLItem() override;

    LibretroGLItem(const LibretroGLItem&) = delete;
    LibretroGLItem& operator=(const LibretroGLItem&) = delete;

    /**
     * Wire this item to the live VideoHardwareGL. Called by GameSession
     * after CoreRuntime grants a HW context. Disconnects the previous
     * source (if any) and connects the frameReady signal to schedule
     * paint updates. Pass nullptr to detach on stop().
     */
    Q_INVOKABLE void setVideoHardware(QObject* hw);

    /**
     * LibretroRenderSurface — GameSession's pre-stop fence. Drops the
     * MTLTexture ref (setVideoHardware(nullptr)) and drains two render
     * passes so QSGRenderThread can't touch the IOSurface while
     * VideoHardwareGL tears down on the worker thread. Bounded by a 500ms
     * cap. (Moved here from GameSession so core/ owns no Qt Quick.)
     */
    void fenceForShutdown() override;

    QString aspectMode() const { return m_aspectMode; }
    void setAspectMode(const QString& mode);

    qreal nativeAspect() const { return m_nativeAspect; }
    void setNativeAspect(qreal a);

    bool integerScale() const { return m_integerScale; }
    void setIntegerScale(bool on);

signals:
    void aspectModeChanged();
    void nativeAspectChanged();
    void integerScaleChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private slots:
    void onFrameReady(int width, int height);

private:
    // QPointer so the field auto-clears when VideoHardwareGL is destroyed
    // by CoreRuntime's teardown before this QML item is destroyed. With a
    // raw pointer, the QML Component.onDestruction handler calls
    // setVideoHardware(null), which dereferenced the dangling m_hw during
    // disconnect() and crashed (EXC_BAD_ACCESS in setVideoHardware+51).
    QPointer<VideoHardwareGL> m_hw;
    QString  m_aspectMode   = QStringLiteral("native");
    qreal    m_nativeAspect = 16.0 / 9.0;   // PSP default; PPSSPP reports via av_info
    bool     m_integerScale = false;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
