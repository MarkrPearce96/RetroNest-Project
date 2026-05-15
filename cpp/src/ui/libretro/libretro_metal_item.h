// SPDX-License-Identifier: GPL-3.0+
// pcsx2-libretro / RetroNest — Metal-backed video item.
//
// QQuickItem that exposes a layer-backed NSView for hardware-rendering
// libretro cores (PCSX2 on macOS uses Metal via GSDeviceMTL; the core
// installs its own CAMetalLayer on the NSView we provide).
//
// LibretroMetalItem is the hardware-render counterpart of
// LibretroVideoItem (software). EmulationView.qml hosts one or the
// other based on the active adapter's prefersHardwareRender().
//
// Implementation lives in libretro_metal_item.mm because it touches
// Objective-C++ (NSView, CAMetalLayer).
//
// Aspect handling: the child NSView is letterboxed inside the
// QQuickItem's bounding rect according to aspectMode, mirroring
// LibretroVideoItem's pattern so the Metal path doesn't stretch the
// renderer's output to fill the host window. PCSX2 reports
// geometry.aspect_ratio = 4/3 for PS2 content via libretro av_info;
// nativeAspect captures that hint (default 4/3).
//
// Retina handling: NSView's contentsScale is synced to the host
// window's devicePixelRatio so PCSX2's CAMetalLayer renders into a
// retina-resolution drawable instead of a logical-pixel one (which
// macOS would otherwise upscale via bilinear, blurring everything
// regardless of PCSX2's own filter settings).

#pragma once

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class QWindow;

class LibretroMetalItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString aspectMode READ aspectMode WRITE setAspectMode
                   NOTIFY aspectModeChanged)
    Q_PROPERTY(qreal nativeAspect READ nativeAspect WRITE setNativeAspect
                   NOTIFY nativeAspectChanged)
public:
    explicit LibretroMetalItem(QQuickItem* parent = nullptr);
    ~LibretroMetalItem() override;

    LibretroMetalItem(const LibretroMetalItem&) = delete;
    LibretroMetalItem& operator=(const LibretroMetalItem&) = delete;

    /**
     * Returns the underlying NSView pointer suitable for
     * RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW. Returned as qulonglong so
     * QML can read it without raw-pointer marshalling. Pass to
     * GameSession::registerHardwareView before retro_load_game.
     * Returns 0 if the underlying window hasn't been realized yet.
     */
    Q_INVOKABLE qulonglong nativeView() const;

    QString aspectMode() const { return m_aspectMode; }
    void setAspectMode(const QString& mode);

    qreal nativeAspect() const { return m_nativeAspect; }
    void setNativeAspect(qreal a);

signals:
    void aspectModeChanged();
    void nativeAspectChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void updateInnerGeometry();   // letterboxes m_window inside the item's rect
    void syncContentsScale();     // matches NSView layer.contentsScale to host DPR

    QWindow* m_window = nullptr;          // owns the underlying NSView via Qt's surface
    QString m_aspectMode = QStringLiteral("native");
    qreal m_nativeAspect = 4.0 / 3.0;     // PS2 default; PCSX2 reports this via av_info
};
